//
//  actions.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  The condition/action bytecode interpreter: running the status
//  table and the event table, and the implementations
//  of the individual action opcodes.
//

#include <stdio.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "animations.h"
#include "gamedefs.h"
#include "gameload.h"
#include "graphics.h"
#include "irmak.h"
#include "parseinput.h"
#include "player.h"
#include "randomness.h"
#include "restorestate.h"
#include "textoutput.h"
#include "ui.h"
#include "utility.h"

#include "actions.h"

int ActionsExecuted;      /* Whether any action matched this turn */
int RecursionGuard = 0;   /* Set while END is running, to avoid re-entry */
static int DeferredGoto = 0;

#ifdef DEBUG

/*
 *	Debugging
 */
static unsigned char WordMap[256][5];

static const char *Condition[] = {
    "<ERROR>",
    "AT",
    "NOTAT",
    "ATGT",
    "ATLT",
    "PRESENT",
    "HERE",
    "ABSENT",
    "NOTHERE",
    "CARRIED",
    "NOTCARR",
    "WORN",
    "NOTWORN",
    "CREATED",
    "NOTCREATED",
    "ZERO",
    "NOTZERO",
    "W3",
    "W4",
    "W5",
    "CHANCE",
    "LT",
    "GT",
    "EQ",
    "NE",
    "OBJECT",
    "COND26",
    "COND27",
    "COND28",
    "COND29",
    "COND30",
    "COND31",
};

static const char *Action[] = {
    "<ERROR>",
    "RESTORE",
    "END",
    "INVENT",
    "ANYKEY",
    "SAVE",
    "DROPALL",
    "LOOK",
    "OK",
    "GET",
    "DROP",
    "GOTO",
    "GOBY",
    "SET",
    "CLEAR",
    "MESSPRINT",
    "CREATE",
    "DESTROY",
    "PRINT",
    "DELAY",
    "WEAR",
    "REMOVE",
    "LET",
    "PLUS",
    "MINUS",
    "PLACE",
    "SWAP",
    "SWAPF",
    "MEANS",
    "PUTWITH",
    "BEEP", /* Rebel Planet at least */
    "REFRESH?",
    "RAMSAVE",
    "RAMLOAD",
    "CLSLOW?",
    "OOPS",
    "WATCH",
    "SWITCH",
    "GOTOH",
    "PICTURE",
    "ACT40",
    "ACT41",
    "ACT42",
    "ACT43",
    "ACT44",
    "ACT45",
    "ACT46",
    "ACT47",
    "ACT48",
    "ACT49",
    "ACT50",
};

static void LoadWordTable(void)
{
    unsigned char *p = FileImage + VerbBase;

    while (1) {
        if (p[4] == 255)
            break;
        if (WordMap[p[4]][0] == 0)
            memcpy(WordMap[p[4]], p, 4);
        p += 5;
    }
}

static void PrintWord(unsigned char word)
{
    if (word == WORD_WILDCARD)
        debug_print("*	  ");
    else if (word == 0 || WordMap[word][0] == 0)
        debug_print("%-4d ", word);
    else {
        debug_print("%c%c%c%c ",
            WordMap[word][0],
            WordMap[word][1],
            WordMap[word][2],
            WordMap[word][3]);
    }
}

/* The trace of ExecuteLineCode: each condition tested, each action done
   (with its number in the file, before Questprobe 3's is translated), and
   a newline at the end of the line. */
static void DebugCondition(int op, int params, int arg1, int arg2)
{
    debug_print("%s %d ", Condition[op], arg1);
    if (params == 2)
        debug_print("%d ", arg2);
}

static void DebugAction(int done_bit, int op, int file_op, int params, int arg1, int arg2)
{
    if (done_bit)
        debug_print("DONE:");
    debug_print("%s(%d) ", Action[op], file_op);
    if (params >= 1)
        debug_print("%d ", arg1);
    if (params == 2)
        debug_print("%d ", arg2);
}

/* Set up the debug-trace tables once the game has been identified. */
void InitActionDebugging(void)
{
    /* GOBY prints from the secondary message table except in Blizzard Pass
       and Questprobe 3. */
    if (Version != BLIZZARD_PASS_TYPE && Version != QUESTPROBE3_TYPE)
        Action[12] = "MESSAGE2";
    LoadWordTable();
}

#else

static inline void PrintWord(unsigned char word) { (void)word; }
static inline void DebugCondition(int op, int params, int arg1, int arg2)
{
    (void)op, (void)params, (void)arg1, (void)arg2;
}
static inline void DebugAction(int done_bit, int op, int file_op, int params, int arg1, int arg2)
{
    (void)done_bit, (void)op, (void)file_op, (void)params, (void)arg1, (void)arg2;
}

#endif

/* Questprobe 3 uses different opcode numbering for conditions and actions.
   These tables map QP3 opcodes to the standard enum values. */
static const unsigned char Q3Condition[128] = {
    CONDITIONERROR,
    AT,
    NOTAT,
    ATGT,
    ATLT,
    PRESENT,
    ABSENT,
    CARRIED,
    NOTCARR,
    CREATED,
    NOTCREATED,
    ZERO,
    NOTZERO,
    W3,
    W4,
    CHANCE,
    LT,
    GT,
    EQ,
    OBJECT,
    /* The rest are unused: CONDITIONERROR */
};

static const unsigned char Q3Action[ACTION_OP_MASK + 1] = {
    ACTIONERROR,
    SWITCH, /* Swap inventory and dark flag */
    WATCH, /* Print Reed Richards' watch status message */
    RESTORE,
    END,
    INVENT,
    ANYKEY,
    SAVE,
    OK, /* Set "condition failed" flag, Flag[118], to 1 */
    GET,
    DROP,
    GOTO,
    GOTOH, /* Go to the location of the other guy */
    SET,
    CLEAR,
    MESSPRINT,
    CREATE,
    DESTROY,
    LET,
    PLUS,
    MINUS,
    PLACE,
    SWAP,
    PICTURE, /* Draw image on top of room image */
    /* The rest are unused: ACTIONERROR */
};

/* The Spectrum games succeed when a value from 0 to 99 is below n
   (CHANCE 0 never succeeds, CHANCE 100 always does). */
static int Chance(int n)
{
    return (erkyrath_random() % 100 < n);
}

static LoadResult LoadPrompt(void)
{
    OutCaps();
    glk_window_clear(Bottom);
    SysMessage(RESUME_A_SAVED_GAME);
    OutFlush();

    if (!YesOrNo()) {
        glk_window_clear(Bottom);
        if (DeferredGoto == 1) {
            MyLoc = 1;
            DeferredGoto = 0;
        }
        return LOAD_NO_FILE;
    } else {
        return LoadGame();
    }
}

static void QuitGame(void)
{
    if (LastChar == '\n')
        OutReplace(' ');
    OutFlush();
    Look();
    OutCaps();
    SysMessage(PLAY_AGAIN);
    OutFlush();
    if (YesOrNo()) {
        ShouldRestart = 1;
        StopTime = 2;
        return;
    } else {
        glk_exit();
    }
}

static void AnyKey(void)
{
    SysMessage(HIT_ENTER);
    OutFlush();
    WaitCharacter();
}

void Okay(void)
{
    SysMessage(OKAY);
    OutChar(' ');
    OutCaps();
    PrintedOK = 1;
}

/* Start a line of a TAKE ALL / DROP ALL listing: the object's name
   followed by a row of dots. *found tells whether this is the first. */
static void ListObjectWithDots(int obj, int *found)
{
    if (*found)
        OutChar('\n');
    *found = 1;
    PrintObject(obj);
    OutReplace(0);
    OutString("......");
}

/* End a line of the listing with what happened to the object. */
static void ListOutcome(const char *outcome)
{
    OutKillSpace();
    OutString(outcome);
    OutFlush();
}

/* Pick up all objects in the current room starting from object index start. */
static void TakeAll(int start)
{
    if (Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        return;
    }
    int found = 0;
    for (int i = start; i < NumObjects(); i++) {
        if (ObjectLoc[i] == MyLoc) {
            ListObjectWithDots(i, &found);
            if (CarryItem() == 0) {
                SysMessage(YOURE_CARRYING_TOO_MUCH);
                return;
            }
            ListOutcome("Taken");
            Put(i, Carried());
        }
    }
    if (!found) {
        Message(MSG_NOTHING_TO_GET);
    }
}

static void DropAll(int loud)
{
    int found = 0;
    for (int i = 0; i < NumObjects(); i++) {
        if (ObjectLoc[i] == Carried()) {
            if (loud) {
                ListObjectWithDots(i, &found);
                ListOutcome("Dropped");
            }
            Put(i, MyLoc);
        }
    }
    if (loud && !found) {
        OutString("You have nothing to drop. ");
    }
    ItemsCarried = 0;
}

static int GetObject(unsigned char obj)
{
    if (ObjectLoc[obj] == Carried()) {
        SysMessage(YOU_HAVE_IT);
        return 0;
    }
    if (HeldByPlayer(obj)) {
        SysMessage(YOU_ARE_WEARING_IT);
        return 0;
    }
    if (!(Version == QUESTPROBE3_TYPE && OtherGuyLoc == MyLoc && ObjectLoc[obj] == OtherGuyInv)) {
        if (ObjectLoc[obj] != MyLoc) {
            SysMessage(YOU_DONT_SEE_IT);
            return 0;
        }
    }
    if (CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return 0;
    }

    Put(obj, Carried());
    return 1;
}

static int DropObject(unsigned char obj)
{
    /* As in the Spectrum games: a worn object is dropped too, and does
       not count against the items carried. Questprobe 3 has no worn
       objects. */
    if (Version != QUESTPROBE3_TYPE && ObjectLoc[obj] == Worn()) {
        Put(obj, MyLoc);
        return 1;
    }
    if (ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return 0;
    }

    DropItem();
    Put(obj, MyLoc);
    return 1;
}

void Goto(unsigned char destination)
{
    if (BaseGame == QUESTPROBE3 && !PrintedOK)
        Okay();
    if (BaseGame == HEMAN && MyLoc == 0 && destination == 1) {
        DeferredGoto = 1;
    } else {
        MyLoc = destination;
        Redraw = 1;
    }
}

static void Delay(unsigned char seconds)
{
    OutChar(' ');
    OutFlush();

    if (Options & NO_DELAYS)
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    glk_request_timer_events(1000 * seconds);

    event_t ev;

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(AnimationRunning);
}

static void Wear(unsigned char obj)
{
    if (ObjectLoc[obj] == Worn()) {
        SysMessage(YOU_ARE_WEARING_IT);
        return;
    }
    if (ObjectLoc[obj] != Carried()) {
        SysMessage(YOU_HAVENT_GOT_IT);
        return;
    }
    DropItem();
    Put(obj, Worn());
}

static void Remove(unsigned char obj)
{
    if (ObjectLoc[obj] != Worn()) {
        SysMessage(YOU_ARE_NOT_WEARING_IT);
        return;
    }
    if (CarryItem() == 0) {
        SysMessage(YOURE_CARRYING_TOO_MUCH);
        return;
    }
    Put(obj, Carried());
}

/* Replace the current input words — used by action scripts to redirect
   a command to different verb/noun handling. */
static void Means(unsigned char verb, unsigned char noun)
{
    Word[0] = verb;
    Word[1] = noun;
}

static void Q3SwitchInvFlags(unsigned char a, unsigned char b)
{
    if (CarriedLoc == a) {
        CarriedLoc = b;
        OtherGuyInv = a;
    }
}

/* Questprobe 3 per-turn bookkeeping: track the other character's location,
   handle Xandu's presence flag, manage Reed Richards' watch, and
   update turn/asphyxiation counters. */
static void Q3UpdateFlags(void)
{
    if (ObjectLoc[Q3_OBJ_COVERED_IN_TAR] == LOC_INV_TORCH)
        ObjectLoc[Q3_OBJ_COVERED_IN_TAR] = LOC_INV_THING;
    if (IsThing) {
        if (ObjectLoc[Q3_OBJ_HOLDING_TORCH] == LOC_DESTROYED) {
            /* If the "holding HUMAN TORCH by the hands" object is destroyed (i.e. not held) */
            /* the "location of the other guy" flag is set to the location of the Human Torch object */
            OtherGuyLoc = ObjectLoc[Q3_OBJ_TORCH];
        } else {
            OtherGuyLoc = MyLoc;
        }
        Q3SwitchInvFlags(LOC_INV_TORCH, LOC_INV_THING);
    } else { /* I'm the HUMAN TORCH */
        if (ObjectLoc[Q3_OBJ_HOLDING_THING] == LOC_DESTROYED) {
            /* If the "holding THING by the hands" object is destroyed (i.e. not held) */
            /* The "location of the other guy" flag is set to the location of the Thing object */
            OtherGuyLoc = ObjectLoc[Q3_OBJ_THING];
        } else {
            OtherGuyLoc = MyLoc;
        }
        Q3SwitchInvFlags(LOC_INV_THING, LOC_INV_TORCH);
    }

    /* Clear the flag when Xandu is knocked out */
    if (ObjectLoc[Q3_OBJ_XANDU] != Q3_ROOM_XANDU)
        Q3XanduPresent = 0;
    /* And set it when he is present */
    else if (Present(Q3_OBJ_XANDU))
        Q3XanduPresent = 1;

    /* Make sure that:
     - The watch isn't carried in the "intro"
     - That Thing has it when the game starts,
     no matter who we begin as
     */
    if (!Q3SwitchedWatch) {
        if (MyLoc == Q3_ROOM_OPENING || MyLoc == 0) {
            ObjectLoc[Q3_OBJ_WATCH] = LOC_DESTROYED;
        } else {
            ObjectLoc[Q3_OBJ_WATCH] = LOC_INV_THING;
            Q3SwitchedWatch = 1;
        }
    }

    if (DrawImages)
        return;

    TurnsLow++; /* Turns played % 100 */
    if (TurnsLow == 100) {
        TurnsHigh++; /* Turns divided by 100 */
        TurnsLow = 0;
    }

    ThingAsphyx++; // Turns since Thing started holding breath
    if (ThingAsphyx == 0)
        ThingAsphyx = 0xff;
    TorchAsphyx++; // Turns since Torch started holding breath
    if (TorchAsphyx == 0)
        TorchAsphyx = 0xff;
}

/* Questprobe 3 numbers its opcodes differently, and its flags 4 lower
   than the other games: translate an opcode and its flag arguments to the
   canonical numbering. */
static void Q3TranslateCondition(unsigned char *op, unsigned char *arg1)
{
    *op = Q3Condition[*op];
    switch (*op) {
    case ZERO:
    case NOTZERO:
    case LT:
    case GT:
    case EQ:
    case NE:
        *arg1 += 4;
        break;
    default:
        break;
    }
}

static void Q3TranslateAction(unsigned char *op, unsigned char *arg1, unsigned char *arg2)
{
    *op = Q3Action[*op];
    switch (*op) {
    case SET:
    case CLEAR:
    case LET:
    case PLUS:
    case MINUS:
        *arg1 += 4;
        break;
    case SWAPF:
        *arg1 += 4;
        *arg2 += 4;
        break;
    default:
        break;
    }
}

/* Number of argument bytes that follow a condition opcode, as numbered
   in the game file. Every condition takes one; from LT on, two. */
static int ConditionParamCount(unsigned char op)
{
    int first_with_two = Version == QUESTPROBE3_TYPE ? 16 : 21;
    return op >= first_with_two ? 2 : 1;
}

/* Number of argument bytes that follow an action opcode (without its
   flag bits), as numbered in the game file. Opcodes 0-8 take none, 9 and
   up take one, and from LET on, two. */
static int ActionParamCount(unsigned char op)
{
    int first_with_two = Version == QUESTPROBE3_TYPE ? 18 : 22;
    if (op >= first_with_two)
        return 2;
    return op > 8 ? 1 : 0;
}

/* Temple of Terror uses flag 12 for two things, which breaks the final
   puzzle. Move the puzzle's use of it (told apart by the value compared
   or added) to the otherwise unused flag 60. */
static unsigned char ToTPuzzleFlag(unsigned char flag, unsigned char value, unsigned char puzzle_value)
{
    if (BaseGame == TEMPLE_OF_TERROR && flag == TOT_SHARED_FLAG && value == puzzle_value)
        return TOT_PUZZLE_FLAG;
    return flag;
}

/* Evaluate a single condition opcode (already translated to the canonical
   numbering). Returns nonzero if the condition holds. */
static int ConditionHolds(unsigned char op, unsigned char arg1, unsigned char arg2)
{
    switch (op) {
    case AT:
        return MyLoc == arg1;
    case NOTAT:
        return MyLoc != arg1;
    case ATGT:
        return MyLoc > arg1;
    case ATLT:
        return MyLoc < arg1;
    case PRESENT:
        return Present(arg1);
    case HERE:
        return ObjectLoc[arg1] == MyLoc;
    case ABSENT:
        return !Present(arg1);
    case NOTHERE:
        return ObjectLoc[arg1] != MyLoc;
    case CARRIED:
        return HeldByPlayer(arg1);
    case NOTCARR:
        return !HeldByPlayer(arg1);
    case WORN:
        return ObjectLoc[arg1] == Worn();
    case NOTWORN:
        return ObjectLoc[arg1] != Worn();
    case CREATED:
        return ObjectLoc[arg1] != Destroyed();
    case NOTCREATED:
        return ObjectLoc[arg1] == Destroyed();
    case ZERO:
        if (BaseGame == TEMPLE_OF_TERROR) {
            /* Unless we have kicked sand in the eyes of the guard,
               make sure he kills us if we try to pass. This fixes a bug
               in the original game, which lets us just walk past him. */
            if (arg1 == TOT_GUARD_FLAG && !ToTGuardBlinded && Word[0] == TOT_WORD_GO && Word[1] == TOT_WORD_DOOR)
                Flag[TOT_GUARD_FLAG] = 0;
        }
        return Flag[arg1] == 0;
    case NOTZERO:
        return Flag[arg1] != 0;
    /* W3/4/5 match against the third/fourth/fifth parsed input
       words (Word[0] and Word[1] are the verb and primary noun). */
    case W3:
        return Word[2] == arg1;
    case W4:
        return Word[3] == arg1;
    case W5:
        return Word[4] == arg1;
    case CHANCE:
        return Chance(arg1);
    case LT:
        return Flag[arg1] < arg2;
    case GT:
        return Flag[arg1] > arg2;
    case EQ:
        arg1 = ToTPuzzleFlag(arg1, arg2, 4);
        return Flag[arg1] == arg2;
    case NE:
        return Flag[arg1] != arg2;
    case OBJECT:
        return ObjectLoc[arg1] == arg2;
    default:
        fprintf(stderr, "Unknown condition %d.\n",
            op);
        return 0;
    }
}

static void SwapBytes(uint8_t *a, uint8_t *b)
{
    uint8_t tmp = *a;
    *a = *b;
    *b = tmp;
}

/* The BEEP action: duration and pitch are its two arguments */
static void PlayBeep(unsigned char duration, unsigned char pitch)
{
    /* The interpreter's Beep routine (Rebel Planet @0x6AD7) builds the
     * ROM beeper (0x03B5) inputs from the two operands, then JP 0x03B5:
     *   DE (cycles)      = (duration * 4) & 0x1FF
     *   HL (half-period) = pitch * DE   (16x16 multiply at 0x30A9)
     * The & 0x1FF is not a tidy-up: the routine forms duration*4 with
     * two ADD A,A and a single RL D, so only bit 6 of the duration
     * carries into the high byte (it keeps just the low 9 bits). Without
     * it, durations >= 64 (e.g. 0xFF) overshoot badly. f = 437500 /
     * (HL+30.125) Hz for DE cycles. Reverse-engineered in MAME: PRESS IH
     * in Rebel Planet gives duration=0xFF, pitch=2 -> DE=508, HL=1016
     * (418 Hz) -> ~1.2 s, matching the original. (The old arg1-as-HL
     * code made a ~3 ms click; a naive duration*4 made a ~4.8 s drone.) */
#if defined(GLK_MODULE_GARGLKBLEEP)
    (void)pitch;
    garglk_zbleep(1 + (duration == 250));
#elif defined(SPATTERLIGHT)
    int de = (duration * 4) & 0x1FF;
    win_beep_zx((pitch * de) & 0xFFFF, de);
#else
    (void)duration;
    (void)pitch;
    putchar('\007');
    fflush(stdout);
#endif
}

/* Perform a single action opcode (already translated to the canonical
   numbering). May set *done to stop further table scanning. Returns
   nonzero if the rest of the line must be abandoned immediately. */
static int PerformAction(unsigned char op, unsigned char arg1, unsigned char arg2, int *done)
{
    switch (op) {
    case RESTORE:
        /* A file that was read ends the turn, good or bad: a bad one has
           put the state back and said so. Only "no", or no file, goes on
           with the line. */
        if (LoadPrompt() != LOAD_NO_FILE) {
            *done = 1;
            return 1;
        }
        break;
    case END:
        if (!RecursionGuard) {
            RecursionGuard = 1;
            QuitGame();
        }
        *done = 1;
        return 1;
    case INVENT:
        Inventory();
        break;
    case ANYKEY:
        AnyKey();
        break;
    case SAVE:
        StopTime = 1;
        SaveGame();
        break;
    case DROPALL:
        /* The opcode is also used to empty the inventory silently. List
           what is dropped only when the player typed DROP ALL: word 20 is
           DROP, and ALL is word 141 in Rebel Planet, 254 in Kayleth. */
        if ((BaseGame == REBEL_PLANET && (Word[0] != 20 || Word[1] != 141)) ||
            (BaseGame == KAYLETH && (Word[0] != 20 || Word[1] != 254)))
            DropAll(0);
        else
            DropAll(1);
        break;
    case LOOK:
        Look();
        break;
    case OK:
        /* Despite the name, OK prints nothing: in QP3 and in all five
           other Spectrum games it only sets the stop flag. */
        *done = 1;
        break;
    case GET:
    case DROP:
        /* A GET or DROP that fails prints why and, in every game, ends
           the table scan at once, leaving the rest of the line undone. */
        if ((op == GET ? GetObject(arg1) : DropObject(arg1)) == 0) {
            *done = 1;
            return 1;
        }
        break;
    case GOTO:
        /* He-Man's transformation moves the player to a room with no
           description and OOPSes back to the room left. */
        if (BaseGame == HEMAN && arg1 == HEMAN_ROOM_GRAYSKULL)
            SaveUndo();
        Goto(arg1);
        break;
    case GOBY:
        /* Blizzard pass era */
        if (Version == BLIZZARD_PASS_TYPE)
            Goto(ObjectLoc[arg1]);
        else
            Message2(arg1);
        break;
    case SET:
        Flag[arg1] = 255;
        break;
    case CLEAR:
        Flag[arg1] = 0;
        break;
    case MESSPRINT:
        /* Prevent repeated "Blob returns to his post" messages */
        if (CurrentGame == QUESTPROBE3_64 && arg1 == Q3_64_MSG_BLOB_RETURNS)
            Q3_64BlobFlag = 0;
        Message(arg1);
        if (CurrentGame == BLIZZARD_PASS && arg1 != 160)
            OutChar('\n');
        break;
    case CREATE:
        Put(arg1, MyLoc);
        break;
    case DESTROY:
        Put(arg1, Destroyed());
        break;
    case PRINT:
        PrintNumber(Flag[arg1]);
        break;
    case DELAY:
        Delay(arg1);
        break;
    case WEAR:
        Wear(arg1);
        break;
    case REMOVE:
        Remove(arg1);
        break;
    case LET:
        if (BaseGame == TEMPLE_OF_TERROR) {
            if (arg1 == TOT_GUARD_FLAG && arg2 == 2) {
                /* If the serpent guard is present, we have just kicked sand in his eyes */
                ToTGuardBlinded = (ObjectLoc[TOT_OBJ_SERPENT_GUARD] == MyLoc);
            }
        }
        Flag[arg1] = arg2;
        break;
    case PLUS:
        arg1 = ToTPuzzleFlag(arg1, arg2, 1);
        Flag[arg1] = Flag[arg1] + arg2 > 255 ? 255 : Flag[arg1] + arg2;
        break;
    case MINUS:
        Flag[arg1] = Flag[arg1] > arg2 ? Flag[arg1] - arg2 : 0;
        break;
    case PLACE:
        Put(arg1, arg2);
        break;
    case SWAP: {
        uint8_t loc = ObjectLoc[arg1];
        Put(arg1, ObjectLoc[arg2]);
        Put(arg2, loc);
        break;
    }
    case SWAPF:
        SwapBytes(&Flag[arg1], &Flag[arg2]);
        break;
    case MEANS:
        Means(arg1, arg2);
        break;
    case PUTWITH:
        Put(arg1, ObjectLoc[arg2]);
        break;
    case BEEP:
        PlayBeep(arg1, arg2);
        break;
    case REFRESH:
        if (BaseGame == KAYLETH)
            TakeAll(78);
        if (BaseGame == HEMAN)
            TakeAll(45);
        Redraw = 1;
        break;
    case RAMSAVE:
        RamSave();
        break;
    case RAMLOAD:
        RamLoad();
        break;
    case CLSLOW:
        OutFlush();
        glk_window_clear(Bottom);
        break;
    case OOPS:
        RestoreUndo(0);
        Redraw = 1;
        break;
    case WATCH: {
        char buf[5];
        Message(Q3_MSG_WATCH);
        /* TurnsLow = turns % 100, TurnsHigh == turns / 100 */
        snprintf(buf, sizeof buf, "%04d", TurnsLow + TurnsHigh * 100);
        OutString(buf);
        Message(Q3_MSG_WATCH + 1);
        if (IsThing)
            /* THING is always 100 percent rested */
            OutString("100");
        else {
            /* Calculate "restedness" percentage */
            snprintf(buf, sizeof buf, "%d", (Q3Restedness >> 2) + Q3Restedness);
            OutString(buf);
        }
        Message(Q3_MSG_WATCH + 2);
        break;
    }
    case SWITCH: {
        /* Swap "current" and "other character" state: inventory code
           (CarriedLoc) plus the two dark flags. */
        SwapBytes(&CarriedLoc, &OtherGuyInv);
        SwapBytes(&Q3OtherGuyDark, &Flag[Q3_DARK_FLAG]);
        Redraw = 1;
        break;
    }
    case GOTOH:
        /* Go to the location of the other guy */
        MyLoc = ObjectLoc[arg1];
        /* Pick him up, so that you don't see yourself */
        GetObject(arg1);
        Redraw = 1;
        break;
    case PICTURE:
        if (!TAYLOR_GRAPHICS_ENABLED)
            break;
        /* The original draws the tar pit as seen from above here */
        if (MyLoc == Q3_ROOM_SUBMERGED_IN_TAR || Flag[DarkFlag()]) {
            DrawBlack();
            break;
        }
        if (arg1 == 0) {
            ClearGraphMem();
            DrawPictureNumber(MyLoc - 1, 1);
        } else if (arg1 == Q3_PIC_FALLEN_STATUE && ObjectLoc[Q3_OBJ_FALLEN_STATUE] != MyLoc) {
            /* A bug in the original: it draws the toppled statue
               while Xandu still stands. */
            break;
        } else {
            DrawPictureNumber(arg1 - 1, 1);
        }
        DrawBufferedPicture();
        break;
    default:
        fprintf(stderr, "Unknown command %d.\n", op);
        break;
    }
    return 0;
}

/* Execute one action line: evaluate conditions (bytes with bit 7 clear),
   and if all pass, run the action commands (bytes with bit 7 set).
   Bit 6 on an action byte sets *done to stop further table scanning. */
static void ExecuteLineCode(unsigned char *code, int *done)
{
    unsigned char arg1 = 0, arg2 = 0;

    /* Phase 1: read condition bytes until we see one with the action bit
       set (which signals the start of the action stream for this line). */
    while (!(*code & ACTION_BIT)) {
        unsigned char op = *code++;
        arg1 = *code++;
        int params = ConditionParamCount(op);
        if (params == 2)
            arg2 = *code++;

        if (Version == QUESTPROBE3_TYPE)
            Q3TranslateCondition(&op, &arg1);

        DebugCondition(op, params, arg1, arg2);

        if (!ConditionHolds(op, arg1, arg2)) {
            debug_print("\n");
            /* Condition failed (or unknown): abandon this line and let the
               caller advance to the next one. */
            return;
        }
    }

    /* Phase 2: all conditions passed — record that we did something this
       turn and execute the action bytes until we see one without the
       action bit (i.e. the next line's conditions) or run off the end. */
    ActionsExecuted = 1;

    while (*code & ACTION_BIT) {
        unsigned char op = *code++;

        /* Bit 6 ("DONE") asks the table scanner to stop after this line. */
        int done_bit = op & ACTION_DONE_BIT;
        if (done_bit)
            *done = 1;
        op &= ACTION_OP_MASK;
        unsigned char file_op = op;

        int params = ActionParamCount(op);
        if (params >= 1)
            arg1 = *code++;
        if (params == 2)
            arg2 = *code++;

        if (Version == QUESTPROBE3_TYPE) {
            Q3TranslateAction(&op, &arg1, &arg2);
            if (!PrintedOK)
                Okay();
        }

        DebugAction(done_bit, op, file_op, params, arg1, arg2);

        /* Track lighting before the action so we can redraw if the action
           toggles the dark flag (e.g. lighting a lamp in a dark room). */
        int WasDark = Flag[DarkFlag()];

        if (PerformAction(op, arg1, arg2, done))
            return;

        if (WasDark != Flag[DarkFlag()])
            Redraw = 1;

        /* The original stops right after an action with the DONE bit. */
        if (done_bit)
            return;
    }
    debug_print("\n");
}

/* Advance past the current action line (conditions + actions) to the
   start of the next one. */
static unsigned char *NextLine(unsigned char *ptr)
{
    while (!(*ptr & ACTION_BIT))
        ptr += 1 + ConditionParamCount(*ptr);
    while (*ptr & ACTION_BIT)
        ptr += 1 + ActionParamCount(*ptr & ACTION_OP_MASK);
    return ptr;
}

/* Draw two images that are unused in the original game */
void QP3DrawExtraImages(void)
{
    if (!TAYLOR_GRAPHICS_ENABLED)
        return;
    /* There is an unused image of the cannon
     on the road but it has the wrong background
     colour.
     */
    if (MyLoc == Q3_ROOM_ROAD && ObjectLoc[Q3_OBJ_CANNON] == Q3_ROOM_ROAD) {
        PatchAndDrawQP3Cannon();
    } else if (MyLoc == Q3_ROOM_TAR_PIT && ObjectLoc[Q3_OBJ_THING] == Q3_ROOM_TAR_PIT && TurnsLow > 16 && TurnsLow < 20) {
        /* Draw close-up of Thing as he sinks */
        DrawPictureNumber(Q3_PIC_THING_SINKING, 1);
        DrawBufferedPicture();
    }
}

/* Run the status table (STATUS in the BBC source) — it fires every turn
   regardless of player input. Entries are executed until ACTION_TABLE_END
   or an action sets the done flag. */
void RunStatusTable(void)
{
    if (StopTime) {
        StopTime--;
        return;
    }
    unsigned char *ptr = FileImage + StatusBase;

    int done = 0;
    ActionsExecuted = 0;

    if (Version == QUESTPROBE3_TYPE) {
        Q3UpdateFlags();
    }

    while (*ptr != ACTION_TABLE_END) {
        while (Version == QUESTPROBE3_TYPE && *ptr == Q3_STATUS_TABLE_PADDING) {
            ptr++;
        }
        ExecuteLineCode(ptr, &done);
        if (done) {
            return;
        }
        ptr = NextLine(ptr);
    }
    if (Version == QUESTPROBE3_TYPE)
        DrawImages = 0;
}

/* Run the event table (EVENT in the BBC source), matching the parsed input words
   against verb/noun entries. WORD_WILDCARD is a wildcard. The parser has
   put the verb first, whatever order the words were typed in. */
void RunEventTable(void)
{
    unsigned char *ptr = FileImage + EventBase;

    int done = 0;
    ActionsExecuted = 0;

    while (*ptr != ACTION_TABLE_END) {

        if ((*ptr == WORD_WILDCARD || *ptr == Word[0]) && (ptr[1] == WORD_WILDCARD || ptr[1] == Word[1])) {
            PrintWord(ptr[0]);
            PrintWord(ptr[1]);
            /* Work around a Questprobe 3 bug – the original game
               can be broken by The Thing picking something up in
               the great room right after entering it. */
            /* We make sure he is ordered out instead. */
            if (Version == QUESTPROBE3_TYPE) {
                /* In great room, Xandu present */
                if (Present(Q3_OBJ_XANDU)) {
                    Q3XanduPresent = 1;
                    Message(Q3_MSG_ORDERED_OUT);
                    Goto(Q3_ROOM_ORDERED_OUT_TO);
                    ActionsExecuted = 1;
                    return;
                }
            }
            ExecuteLineCode(ptr + 2, &done);
            if (done)
                return;
        }
        ptr = NextLine(ptr + 2);
    }
}


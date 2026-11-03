/*
 *  scott_actions.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Action engine: condition evaluation, opcode dispatch, and
 *  main action table.
 */

#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "sagagraphics.h"
#include "vector_common.h"
#include "rtpi_graphics.h"

#include "game_specific.h"
#include "gremlins.h"
#include "hulk.h"
#include "robin_of_sherwood.h"
#include "seas_of_blood.h"

#include "parser.h"
#include "ti99_4a_terp.h"
#include "saga.h"

#include "randomness.h"

#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

/* Game state operations — basically helper functions
   for PerformLine() and PerformTI99Line().
   Some of them are general enough to be used by other
   parts of the code as well. */

int CountCarried(void)
{
    int ct = 0;
    int n = 0;
    while (ct <= GameHeader.NumItems) {
        if (Items[ct].Location == CARRIED)
            n++;
        ct++;
    }
    return (n);
}

void GameOver(void)
{
    if (split_screen && Top)
        Look();
    Output("\n\n");
    Output(sys[PLAY_AGAIN]);
    Output("\n");
    if (YesOrNo()) {
        should_restart = 1;
    } else {
        CleanupAndExit();
    }
}

/* Pause for a given duration using Glk timer events.
 If vector drawing is in progress, waits for it to finish first. */
void Delay(float seconds)
{
    if (Options & NO_DELAYS)
        return;

    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
        return;

    glk_request_char_event(Bottom);
    glk_cancel_char_event(Bottom);

    if (DrawingVector()) {
        do {
            glk_select(&ev);
            Updates(ev);
        } while (DrawingVector());
        if (gli_slowdraw)
            seconds = 0.5;
    }

    glk_request_timer_events(1000 * seconds);

    do {
        glk_select(&ev);
        Updates(ev);
    } while (ev.type != evtype_Timer);

    glk_request_timer_events(0);
}

/* An item that the game does not have (a damaged or misread action line)
   is treated as not in play and never moved, rather than read from or
   written beyond the end of Items[]. */
int ItemExists(int item)
{
    return item >= 0 && item <= GameHeader.NumItems;
}

void SetItemLocation(int item, int loc)
{
    if (ItemExists(item))
        Items[item].Location = loc;
}

/* Move an item without the carry-limit check, redrawing the room if the
   item was visible in it. */
static void MoveItem(int item, int loc)
{
    if (!ItemExists(item))
        return;
    if (Items[item].Location == MyLoc)
        RequestRedraw();
    Items[item].Location = loc;
}

void MoveItemAToLocOfItemB(int itemA, int itemB)
{
    if (!ItemExists(itemA) || !ItemExists(itemB))
        return;
    Items[itemA].Location = Items[itemB].Location;
    if (Items[itemB].Location == MyLoc) {
        should_look_in_transcript = 1;
        DrawUSRoomObject(itemB);
        DrawImageOrVector();
    }
}

void GoTo(int loc)
{
    ACT_DEBUG("player location is now room %d (%s).\n", loc,
                Rooms[loc].Text);
    int oldloc = MyLoc;
    MyLoc = loc;
    RequestRedraw();
    Look();
    if (oldloc != MyLoc && (Options & FLICKER_ON))
        Delay(0.2);
}

void GoToStoredLoc(void)
{
    ACT_DEBUG("switch location to stored location (%d) (%s).\n",
        SavedRoom, Rooms[SavedRoom].Text);
    int t = MyLoc;
    MyLoc = SavedRoom;
    SavedRoom = t;
    RequestRedraw();
}

void SetBitFlag(int bit) {
    ACT_DEBUG("Bitflag %d is set\n", bit);
    BitFlags |= (uint64_t)1 << bit;
}

void ClearBitFlag(int bit) {
    ACT_DEBUG("Bitflag %d is cleared\n", bit);
    BitFlags &= ~((uint64_t)1 << bit);
}

int TestBitFlag(int bit) {
    return (BitFlags & ((uint64_t)1 << bit)) != 0;
}

static void ChangeDarkness(int dark) {
    int was_dark = ItIsDark();
    if (dark) {
        SetBitFlag(DARKBIT);
        should_look_in_transcript = (should_look_in_transcript == 1 || was_dark == 0);
    } else {
        ClearBitFlag(DARKBIT);
        should_look_in_transcript = (should_look_in_transcript == 1 || was_dark == 1);
    }
    if (should_draw_image == 0) {
        should_draw_image = should_look_in_transcript;
    }
}

void SetDark(void) {
    ChangeDarkness(1);
}

void SetLight(void) {
    ChangeDarkness(0);
}

/* Refill the light source and put it in the player's hands. */
void RefillLight(void)
{
    GameHeader.LightTime = LightRefill;
    Items[LIGHT_SOURCE].Location = CARRIED;
    ClearBitFlag(LIGHTOUTBIT);
}

void SwapLocAndRoomflag(int index)
{
    ACT_DEBUG("swap location<->roomflag[%d]\n", index);
    int temp = MyLoc;
    MyLoc = RoomSaved[index];
    RoomSaved[index] = temp;
    RequestRedraw();
    Look();
}

void SwapItemLocations(int itemA, int itemB)
{
    if (!ItemExists(itemA) || !ItemExists(itemB))
        return;
    int temp = Items[itemA].Location;
    Items[itemA].Location = Items[itemB].Location;
    Items[itemB].Location = temp;
    if (Items[itemA].Location == MyLoc || Items[itemB].Location == MyLoc)
        RequestRedraw();
}

void PutItemAInRoomB(int itemA, int roomB)
{
    if (!ItemExists(itemA))
        return;
    ACT_DEBUG("Item %d (%s) is put in room %d (%s). MyLoc: %d (%s)\n",
        itemA, Items[itemA].Text, roomB, Rooms[roomB].Text, MyLoc,
        Rooms[MyLoc].Text);
    if (Items[itemA].Location == MyLoc)
        LookWithPause();
    Items[itemA].Location = roomB;
}

void SwapCounters(int index)
{
    ACT_DEBUG(
        "Select a counter. Current counter is swapped with backup "
        "counter %d\n",
        index);
    if (index > NUM_COUNTERS - 1) {
        debug_print("ERROR! parameter out of range. Max %d, got %d\n", NUM_COUNTERS - 1, index);
        index = NUM_COUNTERS - 1;
    }
    int temp = CurrentCounter;

    CurrentCounter = Counters[index];
    Counters[index] = temp;
    ACT_DEBUG("Value of new selected counter is %d\n",
        CurrentCounter);
}

/* Turn the light on and move the player to the last room, which the
   games use as the death room */
static void MoveToDeathRoom(void)
{
    SetLight();
    MyLoc = GameHeader.NumRooms; /* It seems to be what the code says! */
    RequestRedraw();
}

void PlayerIsDead(void)
{
    ACT_DEBUG("Player is dead\n");
    Output(sys[IM_DEAD]);
    MoveToDeathRoom();
}

/* Where TAKE (the current room) or DROP (the inventory) finds its items */
int TakeOrDropSource(int verb)
{
    return (verb == TAKE) ? MyLoc : CARRIED;
}

static void PrintTakenOrDropped(int index)
{
    Output(sys[index]);
    size_t length = strlen(sys[index]);
    if (length > 0) {
        char last = sys[index][length - 1];
        if (last == '\n' || last == '\r')
            return;
    }
    Output(" ");
    if ((!(CurrentCommand->allflag & LASTALL))
        || split_screen == 0) {
        Output("\n");
    }
}

static void ClearScreen(void)
{
    glk_window_clear(Bottom);
}

int RandomPercent(int n)
{
    return (int)(erkyrath_random() % 100) < n;
}

void OutputNumber(int a) { Display(Bottom, "%d", a); }

void PrintCounter(void)
{
    OutputNumber(CurrentCounter);
    Output(" ");
}

int PrintScore(void)
{
    int i = 0;
    int n = 0;
    while (i <= GameHeader.NumItems) {
        if (Items[i].Location == GameHeader.TreasureRoom && *Items[i].Text == '*')
            n++;
        i++;
    }
    /* Guard against division by zero in games that define no treasures */
    int percent = GameHeader.Treasures ? (n * 100) / GameHeader.Treasures : 0;
    Display(Bottom, "%s %d %s%s %d.\n", sys[IVE_STORED], n, sys[TREASURES],
        sys[ON_A_SCALE_THAT_RATES], percent);
    if (n == GameHeader.Treasures) {
        Output(sys[YOUVE_SOLVED_IT]);
        if (CurrentGame == RETURN_TO_PIRATES_ISLE) {
            ShowUSCloseup(RTPI_WIN_IMAGE, 0);
        }
        GameOver();
        return 1;
    }
    return 0;
}

void PrintNoun(void)
{
    if (CurrentCommand)
        glk_put_string_stream_uni(glk_window_get_stream(Bottom),
            UnicodeWords[CurrentCommand->nounwordindex]);
}

void PrintMessage(int index)
{
    ACT_DEBUG("Print message %d: \"%s\"\n", index,
        Messages[index]);
    const char *message = Messages[index];
    if (message != NULL && message[0] != 0) {
        Output(message);
        const char lastchar = message[strlen(message) - 1];
        if (lastchar != '\r' && lastchar != '\n')
            Output(sys[MESSAGE_DELIMITER]);
    }
}


/* Test a single action condition (one of the COND_XXX codes other
 than COND_PARAMETER) against the current game state. arg is the
 condition's operand: an item, room, flag or counter value. Shared by
 the standard action engine and the TI-99/4A bytecode interpreter.
 Returns nonzero if the condition holds. */
int ConditionHolds(int condition, int arg)
{
    if (!ItemExists(arg)) {
        switch (condition) {
            case COND_CARRIED:
            case COND_IN_ROOM:
            case COND_PRESENT:
            case COND_IN_PLAY:
            case COND_MOVED:
                return 0;
            case COND_NOT_IN_ROOM:
            case COND_NOT_CARRIED:
            case COND_NOT_PRESENT:
            case COND_NOT_IN_PLAY:
            case COND_NOT_MOVED:
                return 1;
        }
    }

    switch (condition) {
        case COND_CARRIED:
            ACT_DEBUG("Does the player carry %s?\n", Items[arg].Text);
            if (Items[arg].Location != CARRIED)
                return 0;
            break;
        case COND_IN_ROOM:
            ACT_DEBUG("Is %s in location?\n", Items[arg].Text);
            if (Items[arg].Location != MyLoc)
                return 0;
            break;
        case COND_PRESENT:
            ACT_DEBUG("Is %s held or in location?\n", Items[arg].Text);
            if (!IsPresent(arg))
                return 0;
            break;
        case COND_AT_LOC:
            ACT_DEBUG("Is location %s?\n", Rooms[arg].Text);
            if (MyLoc != arg)
                return 0;
            break;
        case COND_NOT_IN_ROOM:
            ACT_DEBUG("Is %s NOT in location?\n", Items[arg].Text);
            if (Items[arg].Location == MyLoc)
                return 0;
            break;
        case COND_NOT_CARRIED:
            ACT_DEBUG("Does the player NOT carry %s?\n", Items[arg].Text);
            if (Items[arg].Location == CARRIED)
                return 0;
            break;
        case COND_NOT_AT_LOC:
            ACT_DEBUG("Is location NOT %s?\n", Rooms[arg].Text);
            if (MyLoc == arg)
                return 0;
            break;
        case COND_FLAG_SET:
            ACT_DEBUG("Is bitflag %d set?\n", arg);
            if (!TestBitFlag(arg))
                return 0;
            break;
        case COND_FLAG_CLEAR:
            ACT_DEBUG("Is bitflag %d NOT set?\n", arg);
            if (TestBitFlag(arg))
                return 0;
            break;
        case COND_CARRYING_ANY:
            ACT_DEBUG("Does the player carry anything?\n");
            if (CountCarried() == 0)
                return 0;
            break;
        case COND_CARRYING_NONE:
            ACT_DEBUG("Does the player carry nothing?\n");
            if (CountCarried())
                return 0;
            break;
        case COND_NOT_PRESENT:
            ACT_DEBUG("Is %s neither carried nor in room?\n", Items[arg].Text);
            if (IsPresent(arg))
                return 0;
            break;
        case COND_IN_PLAY:
            ACT_DEBUG("Is %s (%d) in play?\n", Items[arg].Text, arg);
            if (Items[arg].Location == 0)
                return 0;
            break;
        case COND_NOT_IN_PLAY:
            ACT_DEBUG("Is %s NOT in play?\n", Items[arg].Text);
            if (Items[arg].Location)
                return 0;
            break;
        case COND_COUNTER_LE:
            ACT_DEBUG("Is CurrentCounter <= %d?\n", arg);
            if (CurrentCounter > arg)
                return 0;
            break;
        case COND_COUNTER_GT:
            ACT_DEBUG("Is CurrentCounter > %d?\n", arg);
            if (CurrentCounter <= arg)
                return 0;
            break;
        case COND_NOT_MOVED:
            ACT_DEBUG("Is %s still in initial room?\n", Items[arg].Text);
            if (Items[arg].Location != Items[arg].InitialLoc)
                return 0;
            break;
        case COND_MOVED:
            ACT_DEBUG("Has %s been moved?\n", Items[arg].Text);
            if (Items[arg].Location == Items[arg].InitialLoc)
                return 0;
            break;
        case COND_COUNTER_EQ:
#ifdef DEBUG_ACTIONS
            debug_print("Is current counter == %d?\n", arg);
            if (CurrentCounter != arg)
                debug_print("Nope, current counter is %d\n", CurrentCounter);
#endif
            if (CurrentCounter != arg)
                return 0;
            break;
    }
    return 1;
}

/* What one action line carries from its conditions to its opcodes, and
   what the opcodes report back. */
typedef struct {
    /* The opcode phase can pop up to two params per opcode (NUM_OPCODES * 2),
       which exceeds the NUM_CONDITIONS params the condition phase can push.
       Sized for the reader and zeroed, so an action whose opcodes consume
       more params than its conditions supplied reads a defined 0 (item/room 0)
       rather than running off the end of the array into the stack and using the
       garbage as an Items[] index. */
    int param[NUM_OPCODES * 2];
    int pptr;
    int continuation;
    int dead;
} LineState;

static int NextParam(LineState *line)
{
    return line->param[line->pptr++];
}

/* Test the conditions of line ct, collecting its parameters.
   Conditions are encoded as (value * 20 + condition_code). Condition
   code 0 stores a parameter for later use by action opcodes.
   Returns 0 as soon as a condition fails. */
static int CheckConditions(int ct, LineState *line)
{
    int nparams = 0;
    for (int cc = 0; cc < NUM_CONDITIONS; cc++) {
        int cv = Actions[ct].Condition[cc];
        int dv = cv / CONDITION_MULTIPLIER;
        cv %= CONDITION_MULTIPLIER;
        ACT_DEBUG("Testing condition %d: ", cv);
        if (cv == COND_PARAMETER) {
            if (nparams < (int)(sizeof(line->param) / sizeof(line->param[0])))
                line->param[nparams++] = dv;
        } else if (!ConditionHolds(cv, dv)) {
            return 0;
        }
        ACT_DEBUG("YES\n");
    }
    return 1;
}

/* Opcode 89. Some SAGA releases use it to show room image 80:
   - Close-up of the genie in Adventureland
   - Map sailing animation in Pirate Adventure
   - Bomb blast ending animation in Mission Impossible
   Unlike the Adventure International UK interpreters, in these games
   it does NOT take an argument, so we must watch out for this. */
static void GameSpecificOpcode(LineState *line)
{
    int p = 0;
    ACT_DEBUG("Action OP_GAME_SPECIFIC, parameter %d\n", line->param[line->pptr]);
    if (Game->type == US_VARIANT) {
        ShowUSCloseup(US_CLOSEUP_IMAGE, 0);
    } else {
        p = NextParam(line);
    }
    switch (CurrentGame) {
        case SPIDERMAN:
        case SPIDERMAN_C64:
        case SPIDERMAN_ATARI8:
            DrawBlack();
            break;
        case SECRET_MISSION:
        case SECRET_MISSION_C64:
            SecretAction(p);
            break;
        case ADVENTURELAND:
        case ADVENTURELAND_C64:
            AdventurelandAction(p);
            break;
        case SEAS_OF_BLOOD:
        case SEAS_OF_BLOOD_C64:
            BloodAction(p);
            break;
        case ROBIN_OF_SHERWOOD:
        case ROBIN_OF_SHERWOOD_C64:
            SherwoodAction(p);
            break;
        case GREMLINS:
        case GREMLINS_SPANISH:
        case GREMLINS_SPANISH_C64:
        case GREMLINS_GERMAN:
        case GREMLINS_GERMAN_C64:
            GremlinsAction();
            break;
        default:
            break;
    }
}

/* Run one opcode. Opcodes 1-51 and 102+ print messages; 52-90 are game
   state operations. Returns 0 if the rest of the line is to be skipped. */
static int ExecuteOpcode(int op, LineState *line)
{
    int p, q;
    ACT_DEBUG("Performing action %d: ", op);
    if (op >= FIRST_MESSAGE && op <= LAST_DIRECT_MESSAGE) {
        PrintMessage(op);
        return 1;
    }
    if (op >= EXTENDED_MSG_BASE) {
        PrintMessage(op - EXTENDED_MSG_OFFSET);
        return 1;
    }
    switch (op) {
        case 0: /* NOP */
        case OP_LOOK:
            break;
        case OP_GET_ITEM:
            if (CountCarried() >= GameHeader.MaxCarry) {
                Output(sys[YOURE_CARRYING_TOO_MUCH]);
                return 0;
            }
            p = NextParam(line);
            if (ItemExists(p) && Items[p].Location == MyLoc)
                should_draw_image = 1;
            SetItemLocation(p, CARRIED);
            break;
        case OP_DROP_ITEM:
            p = NextParam(line);
            if (!ItemExists(p))
                break;
            ACT_DEBUG("item %d (\"%s\") is now in location.\n", p, Items[p].Text);
            DrawUSRoomObject(p);
            DrawImageOrVector();
            Items[p].Location = MyLoc;
            should_look_in_transcript = 1;
            break;
        case OP_GOTO_ROOM:
            GoTo(NextParam(line));
            break;
        case OP_DESTROY_ITEM:
        case OP_DESTROY_ITEM_2:
            p = NextParam(line);
            ACT_DEBUG("Item %d (%s) is removed from the game (put in room 0).\n",
                p, Items[p].Text);
            MoveItem(p, 0);
            break;
        case OP_SET_DARK:
            SetDark();
            break;
        case OP_SET_LIGHT:
            SetLight();
            break;
        case OP_SET_FLAG:
            SetBitFlag(NextParam(line));
            break;
        case OP_CLEAR_FLAG:
            ClearBitFlag(NextParam(line));
            break;
        case OP_DIE:
            PlayerIsDead();
            break;
        case OP_MOVE_ITEM:
            p = NextParam(line);
            PutItemAInRoomB(p, NextParam(line));
            break;
        case OP_GAME_OVER:
            ACT_DEBUG("Game over.\n");
            GameOver();
            line->dead = 1;
            break;
        case OP_PRINT_SCORE:
            line->dead = PrintScore();
            StopTime = SKIP_IMPLICIT_TURNS;
            break;
        case OP_LIST_INVENTORY:
            if (Game->type == SEAS_OF_BLOOD_VARIANT)
                AdventureSheet();
            else
                ListInventory(0);
            if (Game->type == US_VARIANT && HasGraphics()) {
                UpdateUSInventory();
            }
            StopTime = SKIP_IMPLICIT_TURNS;
            break;
        case OP_SET_FLAG_0:
            ACT_DEBUG("Set flag 0\n");
            SetBitFlag(0);
            break;
        case OP_CLEAR_FLAG_0:
            ACT_DEBUG("Clear flag 0\n");
            ClearBitFlag(0);
            break;
        case OP_REFILL_LIGHT:
            ACT_DEBUG("Refill lamp\n");
            RefillLight();
            break;
        case OP_CLEAR_SCREEN:
            ACT_DEBUG("Clear screen\n");
            ClearScreen(); /* pdd. */
            break;
        case OP_SAVE:
            ACT_DEBUG("Save game\n");
            SaveGame();
            StopTime = SKIP_IMPLICIT_TURNS;
            break;
        case OP_SWAP_ITEMS:
            p = NextParam(line);
            q = NextParam(line);
            ACT_DEBUG("Swap locations of item %d (%s) (currently at %d) and "
                      "item %d (%s) (currently at %d).\n",
                p, Items[p].Text, Items[p].Location,
                q, Items[q].Text, Items[q].Location);
            SwapItemLocations(p, q);
            break;
        case OP_CONTINUE:
            ACT_DEBUG("Continue with next line\n");
            line->continuation = 1;
            break;
        case OP_FORCE_CARRY:
            MoveItem(NextParam(line), CARRIED);
            break;
        case OP_MOVE_TO_LOC_OF:
            p = NextParam(line);
            MoveItemAToLocOfItemB(p, NextParam(line));
            break;
        case OP_LOOK_2:
            ACT_DEBUG("LOOK\n");
            print_look_to_transcript = should_draw_image = 1;
            if (split_screen)
                Look();
            print_look_to_transcript =
            should_look_in_transcript = 0;
            break;
        case OP_DEC_COUNTER:
            if (CurrentCounter >= 1)
                CurrentCounter--;
            ACT_DEBUG(
                        "decrementing current counter. Current counter is now %d.\n",
                        CurrentCounter);
            break;
        case OP_PRINT_COUNTER:
            PrintCounter();
            break;
        case OP_SET_COUNTER:
            CurrentCounter = NextParam(line);
            ACT_DEBUG("CurrentCounter is set to %d.\n", CurrentCounter);
            break;
        case OP_GOTO_STORED:
            GoToStoredLoc();
            break;
        case OP_SWAP_COUNTER:
            SwapCounters(NextParam(line));
            break;
        case OP_ADD_COUNTER:
            CurrentCounter += NextParam(line);
            break;
        case OP_SUB_COUNTER:
            CurrentCounter -= NextParam(line);
            if (CurrentCounter < -1)
                CurrentCounter = -1;
            /* Note: This seems to be needed. I don't yet
             know if there is a maximum value to limit too */
            break;
        case OP_PRINT_NOUN:
            PrintNoun();
            break;
        case OP_PRINTLN_NOUN:
            PrintNoun();
            Output("\n");
            break;
        case OP_NEWLINE:
            if (!(Options & SPECTRUM_STYLE))
                Output("\n");
            break;
        case OP_SWAP_ROOM:
            SwapLocAndRoomflag(NextParam(line));
            break;
        case OP_DELAY:
            ACT_DEBUG("Delay\n");
            Delay(1);
            break;
        case OP_GAME_SPECIFIC:
            GameSpecificOpcode(line);
            break;
        case OP_DRAW_IMAGE:
            p = NextParam(line);
            ACT_DEBUG("Draw Hulk image, parameter %d\n", p);
            if (!ItIsDark() && (CurrentGame == HULK || CurrentGame == HULK_C64)) {
                DrawHulkImage(p);
            } else if (Game->type == US_VARIANT) {
                ShowUSCloseup(p, 0);
            }
            break;
        default:
            debug_print("Unknown action %d [Param begins %d %d]\n", op,
                        line->param[line->pptr], line->param[line->pptr + 1]);
            break;
    }
    return 1;
}

/* Evaluate one action line: check up to 5 conditions, and if all pass,
 execute the 4 opcodes encoded in the action's two Opcode words.

 Returns ACT_FAILURE if a condition fails, ACT_CONTINUE for action 73
 (continue to next line), ACT_GAMEOVER on game-ending actions, or
 ACT_SUCCESS if all opcodes completed normally. */
static ActionResultType PerformLine(int ct)
{
    ACT_DEBUG("Performing line %d: ", ct);
    LineState line = { { 0 }, 0, 0, 0 };
    if (!CheckConditions(ct, &line))
        return ACT_FAILURE;

    /* Decode the 4 opcodes from two 16-bit words.
     Each word encodes two actions: high = word / VOCAB_MULTIPLIER,
     low = word % VOCAB_MULTIPLIER. */
    int act[NUM_OPCODES];
    act[0] = Actions[ct].Opcode[0] / VOCAB_MULTIPLIER;
    act[1] = Actions[ct].Opcode[0] % VOCAB_MULTIPLIER;
    act[2] = Actions[ct].Opcode[1] / VOCAB_MULTIPLIER;
    act[3] = Actions[ct].Opcode[1] % VOCAB_MULTIPLIER;
    for (int cc = 0; cc < NUM_OPCODES; cc++) {
        if (!ExecuteOpcode(act[cc], &line))
            return ACT_SUCCESS;
    }

    if (line.dead) {
        return ACT_GAMEOVER;
    } else if (line.continuation) {
        return ACT_CONTINUE;
    } else {
        return ACT_SUCCESS;
    }
}


/* GO with a direction. In the dark a wall is fatal. */
static void HandleGo(int direction, int dark)
{
    if (dark)
        Output(sys[DANGEROUS_TO_MOVE_IN_DARK]);
    int nl = Rooms[MyLoc].Exits[direction - 1];
    if (nl != 0) {
        /* Seas of Blood needs this to be able to flee back to the last room */
        if (Game->type == SEAS_OF_BLOOD_VARIANT)
            SavedRoom = MyLoc;
        if (Options & (SPECTRUM_STYLE | TI994A_STYLE))
            Output(sys[OK]);
        MyLoc = nl;
        RequestRedraw();
        if (CurrentCommand && CurrentCommand->next) {
            LookWithPause();
        }
    } else if (dark) {
        MoveToDeathRoom();
        Output(sys[YOU_FELL_AND_BROKE_YOUR_NECK]);
    } else {
        Output(sys[YOU_CANT_GO_THAT_WAY]);
    }
}

/* Scan the action table for lines matching vb/no (or, with vb 0, the
   implicit lines, each with its percentage chance). Action 73 (continue)
   chains the following lines with vocab 0,0. Sets *found_match when a
   line naming both the verb and the noun was tried. */
static ExplicitResultType RunActionTable(int vb, int no, int *found_match)
{
    ExplicitResultType flag = ER_RAN_ALL_LINES_NO_MATCH;
    int doagain = 0;
    int ct = 0;
    while (ct <= GameHeader.NumActions) {
        int verbvalue, nounvalue;
        verbvalue = Actions[ct].Vocab;
        /* Think this is now right. If a line we run has an action 73
         run all following lines with vocab of 0,0 */
        if (vb != 0 && (doagain && verbvalue != 0))
            break;
        /* Oops.. added this minor cockup fix 1.11 */
        if (vb != 0 && !doagain && flag == ER_SUCCESS)
            break;
        nounvalue = verbvalue % VOCAB_MULTIPLIER;
        verbvalue /= VOCAB_MULTIPLIER;
        if ((verbvalue == vb) || (doagain && Actions[ct].Vocab == 0)) {
            if ((verbvalue == 0 && RandomPercent(nounvalue)) || doagain || (verbvalue != 0 && (nounvalue == no || nounvalue == 0))) {
                if (verbvalue == vb && vb != 0 && nounvalue == no)
                    *found_match = 1;
                ActionResultType flag2;
                if (flag == ER_RAN_ALL_LINES_NO_MATCH)
                    flag = ER_RAN_ALL_LINES;
                if ((flag2 = PerformLine(ct)) != ACT_FAILURE) {
                    /* ahah finally figured it out ! */
                    flag = ER_SUCCESS;
                    if (flag2 == ACT_CONTINUE)
                        doagain = 1;
                    else if (flag2 == ACT_GAMEOVER)
                        return ER_SUCCESS;
                    if (vb != 0 && doagain == 0)
                        return ER_SUCCESS;
                }
            }
        }

        ct++;

        // clang-format off
        /* Previously this did not check ct against
         * GameHeader.NumActions and would read past the end of
         * Actions.  I don't know what should happen on the last
         * action, but doing nothing is better than reading one
         * past the end.
         * --Chris
         */
        // clang-format on

        if (ct <= GameHeader.NumActions && Actions[ct].Vocab != 0)
            doagain = 0;
    }
    return flag;
}

/* The built-in TAKE: the noun is matched against the items in the room,
   unless a TAKE ALL chain has already picked one. */
static void TakeItem(int no, int item)
{
    if (no == -1) {
        Output(sys[WHAT]);
        return;
    }
    if (CountCarried() >= GameHeader.MaxCarry) {
        Output(sys[YOURE_CARRYING_TOO_MUCH]);
        return;
    }
    if (!item)
        item = MatchUpItem(no, MyLoc);
    if (item == -1) {
        if (MatchUpItem(no, CARRIED) != -1)
            Output(sys[YOU_HAVE_IT]);
        else if (MatchUpItem(no, 0) != -1)
            Output(sys[YOU_DONT_SEE_IT]);
        else
            Output(sys[THATS_BEYOND_MY_POWER]);
        return;
    }
    Items[item].Location = CARRIED;
    should_draw_image = 1;
    PrintTakenOrDropped(TAKEN);
}

/* The built-in DROP, likewise for the items carried. */
static void DropItem(int no, int item)
{
    if (no == -1) {
        Output(sys[WHAT]);
        return;
    }
    if (!item)
        item = MatchUpItem(no, CARRIED);
    if (item == -1) {
        if (MatchUpItem(no, 0) != -1)
            Output(sys[YOU_HAVENT_GOT_IT]);
        else
            Output(sys[THATS_BEYOND_MY_POWER]);
        return;
    }
    Items[item].Location = MyLoc;
    DrawUSRoomObject(item);
    DrawImageOrVector();
    PrintTakenOrDropped(DROPPED);
}

/* TAKE or DROP that no action line handled.
   Yes they really _are_ hardcoded values */
static void TakeOrDrop(int vb, int no, int dark)
{
    int item = 0;
    if (CurrentCommand && CurrentCommand->allflag) {
        if (vb == TAKE && dark) {
            Output(sys[TOO_DARK_TO_SEE]);
            while (!(CurrentCommand->allflag & LASTALL)) {
                CurrentCommand = CurrentCommand->next;
            }
            return;
        }
        item = CurrentCommand->item;
        /* The ALL chain is built when the line is parsed, so an action
           that ran earlier in the chain may have moved this item since.
           Skip just this node and let the chain go on to the next one.
           (The old loop advanced CurrentCommand without re-reading
           item, so its test could never change: a single moved item
           fast-forwarded to LASTALL and silently discarded the whole
           rest of the ALL chain.) */
        if (Items[item].Location != TakeOrDropSource(vb))
            return;
    }

    if (vb == TAKE)
        TakeItem(no, item);
    else
        DropItem(no, item);
}

/* Main action dispatch: process player input (verb/noun pair) or
 implicit actions (vb=0).

 Handles movement (GO + direction), game-specific examine actions,
 the action table scan, and built-in TAKE/DROP with auto-matching.

 For TI-99/4A games, delegates to the TI-specific action handler. */
ExplicitResultType PerformActions(int vb, int no)
{
    int dark = ItIsDark();
    ExplicitResultType flag;
    int found_match = 0;

    if (vb == GO && no == -1) {
        Output(sys[DIRECTION]);
        return ER_SUCCESS;
    }
    if (vb == GO && no >= 1 && no <= NUM_EXITS) {
        HandleGo(no, dark);
        return ER_SUCCESS;
    }

    if (!dark)
        ShowCloseupOnExamine(vb, no);

    if (CurrentCommand && CurrentCommand->allflag && vb == CurrentCommand->verb && !(dark && vb == TAKE)) {
        if (!lastwasnewline)
            Output("\n");
        Output(Items[CurrentCommand->item].Text);
        Output("....");
    }

    if (CurrentGame != TI994A) {
        flag = RunActionTable(vb, no, &found_match);
    } else if (vb == 0) {
        RunImplicitTI99Actions();
        return ER_NO_RESULT;
    } else {
        flag = RunExplicitTI99Actions(vb, no);
    }

    if (found_match)
        return flag;

    if (flag != ER_SUCCESS && (vb == TAKE || vb == DROP)) {
        TakeOrDrop(vb, no, dark);
        return ER_SUCCESS;
    }
    return flag;
}

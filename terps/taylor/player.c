//
//  player.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//
//  Game state, room display, the per-turn input loop and glk_main.
//  Text output lives in textoutput.c, file loading and table lookup in
//  gameload.c, and the condition/action interpreter in actions.c.
//

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
#include "actions.h"
#include "animations.h"
#include "c64decrunch.h"
#include "extracommands.h"
#include "gamedefs.h"
#include "gameload.h"
#include "graphics.h"
#include "irmak.h"
#include "loading_screen.h"
#include "parseinput.h"
#include "randomness.h"
#include "restorestate.h"
#include "textoutput.h"
#include "utility.h"

#include "taylor.h"

uint8_t Flag[128];      /* Game state flags (conditions, counters, etc.) */
uint8_t ObjectLoc[256]; /* Location of each object (room number or special values) */

/* Objects below this index are "low objects" — printed as part of the
   room description rather than after the "You can see:" prompt. */
static int NumLowObjects;

int PrintedOK;             /* Whether "OK" has been printed this turn */
int Redraw = 0;            /* Room description needs refreshing */

int StopTime = 0;          /* Suppress status table runs for this many turns */
int JustStarted = 1;       /* True until the first player command */
int ShouldRestart = 0;     /* Set to trigger a full game restart */

int NoGraphics = 0;        /* Disable graphics display */

char DelimiterChar = '_';  /* Word separator in dictionary entries */

static int LastNoun = 0; /* Last noun for "IT" pronoun substitution */
static int GoVerb = 0;   /* Dictionary code for "GO" */

GameInfo *Game = NULL;
int InKaylethPreview = 0;

unsigned char Destroyed(void)
{
    return LOC_DESTROYED;
}

unsigned char Carried(void)
{
    return CarriedLoc;
}

unsigned char Worn(void)
{
    return Version == QUESTPROBE3_TYPE ? 0 : Flag[3];
}

unsigned char NumObjects(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return Q3_NUM_OBJECTS;

    /* This removes one weird empty "You notice." in Kayleth */
    if (BaseGame == KAYLETH)
        return KAYLETH_NUM_OBJECTS;

    return Flag[6];
}

static int WaitFlag(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return Q3_WAIT_COUNTER_FLAG;
    else if (BaseGame != REBEL_PLANET && BaseGame != KAYLETH)
        return -1;
    else
        return WAIT_COUNTER_FLAG;
}

int CarryItem(void)
{
    if (Version == QUESTPROBE3_TYPE)
        return 1;
    /* Flag 5: Items carried. Flag 4: Max carried */
    if (ItemsCarried == MaxCarried && CurrentGame != BLIZZARD_PASS)
        return 0;
    if (ItemsCarried < 255)
        ItemsCarried++;
    return 1;
}

int DarkFlag(void)
{
    return Version == QUESTPROBE3_TYPE ? Q3_DARK_FLAG : 1;
}

void DropItem(void)
{
    if (Version != QUESTPROBE3_TYPE && ItemsCarried > 0)
        ItemsCarried--;
}

void Put(unsigned char obj, unsigned char loc)
{
    /* Putting stuff in a room might change the picture, so redraw */
    if (ObjectLoc[obj] == MyLoc || loc == MyLoc)
        Redraw = 1;
    ObjectLoc[obj] = loc;
}

/* Carried or worn. Questprobe 3 has nothing worn, and its Worn() of 0
   is an ordinary location. */
int HeldByPlayer(unsigned char obj)
{
    if (ObjectLoc[obj] == Carried())
        return 1;
    return Version != QUESTPROBE3_TYPE && ObjectLoc[obj] == Worn();
}

int Present(unsigned char obj)
{
    if (obj >= NumObjects())
        return 0;
    unsigned char v = ObjectLoc[obj];
    return v == MyLoc || HeldByPlayer(obj)
        || (Version == QUESTPROBE3_TYPE && v == OtherGuyInv && OtherGuyLoc == MyLoc);
}

/* Room 0 is the intro/title pseudo-room; Kayleth's room 91 is an
   analogous non-game screen. Neither has a picture. */
#define KAYLETH_TITLE_ROOM 91

int IsIntroRoom(void)
{
    return MyLoc == 0 || (BaseGame == KAYLETH && MyLoc == KAYLETH_TITLE_ROOM);
}

/* Reset all game state to initial values and display the starting room. */
static void NewGame(void)
{
    Redraw = 1;
    /* Zero every flag, then copy the initial flag values stored in the
       file header: seven of them, of which QP3 keeps only Flag[0..3]. */
    memset(Flag, 0, sizeof(Flag));
    memcpy(Flag, FileImage + FlagBase, Version == QUESTPROBE3_TYPE ? 4 : 7);
    if (Version == QUESTPROBE3_TYPE) {
        CarriedLoc = LOC_INV_THING;  /* Carried() — default is Thing, before randomization */
        OtherGuyInv = LOC_INV_TORCH; /* Worn() slot used for other character's inventory */
    }
    /* Start in room 0 (the intro/title pseudo-room). */
    MyLoc = 0;
    /* Restore each object's initial location from the file's location table. */
    memcpy(ObjectLoc, FileImage + ObjLocBase, NumObjects());
    /* Reset the per-game wait counter, if this game uses one. */
    if (WaitFlag() != -1)
        Flag[WaitFlag()] = 0;
    Look();
    PrintedOK = 1;
}

static int GetGlkFileLength(strid_t stream)
{
    glk_stream_set_position(stream, 0, seekmode_End);
    return glk_stream_get_position(stream);
}

int YesOrNo(void)
{
    while (1) {
        uint8_t c = WaitCharacter();
        /* Don't echo Return (keycode_Return, cut down to a byte). */
        if (c == (uint8_t)keycode_Return)
            c = 0;
        OutChar(c);
        OutChar('\n');
        OutFlush();
        if (c == 'n' || c == 'N')
            return 0;
        if (c == 'y' || c == 'Y')
            return 1;
        OutString("Please answer Y or N.\n");
        OutFlush();
    }
}

/* Print a one-line notice about a save or restore. */
static void FileNotice(const char *text)
{
    OutString(text);
    OutFlush();
}

LoadResult LoadGame(void)
{
    frefid_t fileref = glk_fileref_create_by_prompt(fileusage_SavedGame,
        filemode_Read, 0);
    if (!fileref) {
        OutFlush();
        return LOAD_NO_FILE;
    }

    /*
     * Reject the file reference if we're expecting to read from it, and the
     * referenced file doesn't exist.
     */
    strid_t stream = NULL;
    if (glk_fileref_does_file_exist(fileref))
        stream = glk_stream_open_file(fileref, filemode_Read, 0);
    if (!stream) {
        glk_fileref_destroy(fileref);
        FileNotice("Unable to open file.\n");
        return LOAD_NO_FILE;
    }

    SavedState *state = SaveCurrentState();
    LoadResult result;

    /* Restore saved game data. */

    if (glk_get_buffer_stream(stream, (char *)Flag, sizeof(Flag)) != sizeof(Flag)
        || glk_get_buffer_stream(stream, (char *)ObjectLoc, sizeof(ObjectLoc)) != sizeof(ObjectLoc)
        || (size_t)GetGlkFileLength(stream) != sizeof(Flag) + sizeof(ObjectLoc)) {
        RecoverFromBadRestore(state);
        result = LOAD_BAD_DATA;
    } else {
        glk_window_clear(Bottom);
        Look();
        free(state);
        InKaylethPreview = 0;
        result = LOAD_OK;
    }
    glk_stream_close(stream, NULL);
    glk_fileref_destroy(fileref);
    return result;
}

/* Print the player's inventory: every object whose location matches the
   "carried" or "worn" code. Each worn item is annotated with a "(worn)"
   suffix; if nothing is carried, the "you have nothing" message prints. */
void Inventory(void)
{
    /* Rebel Planet does its own capitalization via PrintTextRebelPlanet's
       InventoryLower flag, so we skip OutCaps() for it. */
    if (BaseGame != REBEL_PLANET)
        OutCaps();
    SysMessage(INVENTORY);

    int printed_any = 0;
    for (int i = 0; i < NumObjects(); i++) {
        if (HeldByPlayer(i)) {
            printed_any = 1;
            PrintObject(i);
            if (ObjectLoc[i] == Worn()) {
                /* Drop any pending separator, then append "(worn)". */
                OutReplace(0);
                SysMessage(NOWWORN);
                if (CurrentGame == REBEL_PLANET) {
                    /* Rebel Planet uses a comma between items instead of
                       the default trailing space. */
                    OutKillSpace();
                    OutFlush();
                    OutChar(',');
                }
            }
        }
    }
    if (!printed_any) {
        SysMessage(NOTHING);
    } else {
        /* End the list with ". " and arm capitalization for whatever
           prints next (typically the prompt or the next room header). */
        OutReplace('.');
        OutChar(' ');
        OutCaps();
    }
}

void SaveGame(void)
{
    strid_t file;
    frefid_t ref;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0);
    file = ref ? glk_stream_open_file(ref, filemode_Write, 0) : NULL;
    if (ref)
        glk_fileref_destroy(ref);
    if (file == NULL) {
        FileNotice("Save failed.\n");
        return;
    }

    /* Write game state. */
    glk_put_buffer_stream(file, (char *)Flag, sizeof(Flag));
    glk_put_buffer_stream(file, (char *)ObjectLoc, sizeof(ObjectLoc));
    glk_stream_close(file, NULL);
    FileNotice("Saved.\n");
}

/* Exit table framing: each room's block starts with a marker byte, the
   room number with the high bit set, and the table ends with EXITS_END. */
#define EXIT_ROOM_MARKER 0x80
#define EXITS_END 0xff

/* Find the current room's block in the exit table. Returns a pointer to
   its first (direction, destination) pair, or NULL if the room has no
   block. The block ends at the next byte with the high bit set. */
static unsigned char *FindExitBlock(void)
{
    unsigned char want = EXIT_ROOM_MARKER | MyLoc;
    unsigned char *ptr = FileImage + ExitBase;
    while (*ptr != want) {
        if (*ptr == EXITS_END || ptr >= EndOfData - 1)
            return NULL;
        ptr++;
    }
    return ptr + 1;
}

/* List available exits from the current room by scanning the exit table. */
static void ListExits(int caps)
{
    unsigned char *entry = FindExitBlock();
    int printed_any = 0;

    if (entry == NULL)
        return;

    while (*entry < EXIT_ROOM_MARKER) {
        if (!printed_any) {
            if (CurrentGame == BLIZZARD_PASS && LastChar == ',')
                LastChar = 0;
            OutCaps();
            SysMessage(EXITS);
            printed_any = 1;
        }
        if (caps)
            OutCaps();
        SysMessage(*entry);
        entry += 2;
    }
    if (printed_any) {
        OutReplace('.');
        OutChar('\n');
    }
}

/* QP3 draws room images via the status table (which contains IMAGE
   actions). Run it once with DrawImages=255 to draw without advancing the
   turn; stash StopTime so the bypass doesn't suppress next turn's real
   status pass. */
static void QP3RedrawImages(void)
{
    int tempstop = StopTime;
    StopTime = 0;
    DrawImages = 255;
    RunStatusTable();
    QP3DrawExtraImages();
    StopTime = tempstop;
}

/* "Low" objects are part of the room description (e.g. "...a sword
   lies here."). They're listed inline rather than after "You see:". */
static void ListLowObjects(void)
{
    int printed_any = 0;

    for (int i = 0; i < NumLowObjects; i++) {
        if (ObjectLoc[i] == MyLoc) {
            if (!printed_any) {
                if (Version == QUESTPROBE3_TYPE) {
                    OutReplace(0);
                    SysMessage(YOU_SEE);
                } else if (BaseGame == HEMAN || BaseGame == REBEL_PLANET) {
                    OutChar(' ');
                }
                printed_any = 1;
            }
            PendSpace = 1;
            PrintObject(i);
        }
    }
    if (printed_any && !isalpha((unsigned char)LastChar))
        OutReplace('.');
}

/* The objects after the low ones are listed under a "You see:" heading. */
static void ListHighObjects(void)
{
    int printed_any = 0;

    for (int i = NumLowObjects > 0 ? NumLowObjects : 0; i < NumObjects(); i++) {
        if (ObjectLoc[i] == MyLoc) {
            if (!printed_any) {
                /* Only the text-only and hybrid games */
                if (BaseGame == TEMPLE_OF_TERROR
                    && CurrentGame != TEMPLE_OF_TERROR
                    && CurrentGame != TEMPLE_OF_TERROR_64) {
                    OutChar(' ');
                }
                SysMessage(YOU_SEE);
                if (CurrentGame == BLIZZARD_PASS) {
                    PendSpace = 0;
                    OutString(":- ");
                }
                if (Version == REBEL_PLANET_TYPE)
                    OutReplace(0);
                printed_any = 1;
            }
            PrintObject(i);
        }
    }
    if (printed_any)
        OutReplace('.');
    else
        OutChar('.');
}

/* Draw the picture of the current room, if there is a graphics window. */
static void DrawRoomPicture(void)
{
    if (MyLoc == 0 || NoGraphics || !TAYLOR_GRAPHICS_ENABLED || !Graphics)
        return;

    /* A resize-in-progress: reuse the buffered image rather than
       re-running the room's drawing pipeline. */
    if (Resizing) {
        DrawBufferedPicture();
        return;
    }
    if (Version == QUESTPROBE3_TYPE) {
        QP3RedrawImages();
    } else {
        /* Painting over the picture of the turn before needs no
           clear, and only its changed cells are painted. */
        if (!BufferedPictureShown()) {
            glk_window_clear(Graphics);
            GraphicsWindowChanged();
        }
        DrawRoomImage();
    }
}

/* Display the current room: description, visible objects (low objects
   inline, high objects after "You see:"), exits, and optional inventory.
   Draws the room image if graphics are enabled. */
void Look(void)
{
    int is_intro_room = IsIntroRoom();

    if (is_intro_room || NoGraphics)
        CloseGraphicsWindow();
    else
        OpenGraphicsWindow();

    PendSpace = 0;
    /* Skip the output reset while a line-input event is pending in the
       bottom window — resetting would lose buffered input. */
    if (!(CurrentWindow == Bottom && LineEvent))
        OutFlush();
    TopWindow();

    Redraw = 0;
    if (Transcript)
        glk_put_char_stream(Transcript, '\n');

    OutCaps();

    /* Dark room: print the canned "too dark" message and bail out before
       drawing the room or listing its contents. */
    if (Flag[DarkFlag()]) {
        SysMessage(TOO_DARK_TO_SEE);
        OutString("\n\n");
        DrawBlack();
        BottomWindow();
        return;
    }
    if (BaseGame == REBEL_PLANET && MyLoc > 0)
        OutString("You are ");
    PrintRoom(MyLoc);

    ListLowObjects();

    /* QP3 has no "You see:" phase — every visible item is a low object. */
    if (Version == QUESTPROBE3_TYPE) {
        ListExits(1);
    } else {
        ListHighObjects();
        ListExits(BaseGame != TEMPLE_OF_TERROR && BaseGame != HEMAN);
    }

    if (LastChar != '\n')
        OutChar('\n');

    /* Don't show top window inventory in intro rooms */
    if ((Options & FORCE_INVENTORY) && !is_intro_room) {
        OutChar('\n');
        Inventory();
        OutChar('\n');
    }

    OutChar('\n');

    BottomWindow();

    DrawRoomPicture();
}

/* Check if direction word v has a matching exit from the current room.
   If so, move the player there and return 1. */
static int AutoExit(unsigned char v)
{
    unsigned char *ptr = FindExitBlock();
    if (ptr == NULL)
        return 0;
    while (*ptr < EXIT_ROOM_MARKER) {
        if (*ptr == v) {
            Goto(ptr[1]);
            return 1;
        }
        ptr += 2;
    }
    return 0;
}

static void LookIfRedrawn(void)
{
    if (Redraw)
        Look();
}

/* Check if a word code is a direction (N/S/E/W/U/D etc.). */
static int IsDir(unsigned char word)
{
    if (word == 0)
        return 0;
    /* Direction words share their codes with the direction system
       messages. QP3 has six in a row (5 and 6 are up and down, though
       only BELOW, BENEATH and UNDER have those codes) and the words U, UP
       and D further up; the others have all ten in a row. Below these
       the originals answer an unmatched command with "You can't go that
       way". */
    if (Version == QUESTPROBE3_TYPE)
        return (word <= Q3_LAST_DIRECTION || word == Q3_WORD_UP || word == Q3_WORD_DOWN);
    return (word <= SOUTHWEST);
}

/* Whether the command is a move: a direction, or GO and a direction */
static int IsMoveCommand(unsigned char verb)
{
    return IsDir(verb) || (Word[0] == GoVerb && IsDir(Word[1]));
}

/* Process one player command: try extra commands, direction auto-exits,
   the command action table, and implicit verb carry-forward. Then run
   the status table and handle any pending room redraw. */
static void RunOneInput(void)
{
    PrintedOK = 0;

    if (FoundExtraCommand) {
        if (TryExtraCommand()) {
            LookIfRedrawn();
            return;
        }
    }
    /* Questprobe 3 gives up when there is no verb, the later games only
       when there is no word at all: a noun alone goes through their
       tables. */
    if (KnownWords == 0 || (Version == QUESTPROBE3_TYPE && Word[0] == 0)) {
        if (TryExtraCommand() == 0) {
            OutCaps();
            SysMessage(I_DONT_UNDERSTAND);
            StopTime = 2;
        } else {
            LookIfRedrawn();
        }
        return;
    }
    if (IsMoveCommand(Word[0])) {
        if (AutoExit(Word[0]) || AutoExit(Word[1])) {
            StopTime = 0;
            RunStatusTable();
            LookIfRedrawn();
            return;
        }
    }

    /* Handle IT */
    if (Word[1] == 128)
        Word[1] = LastNoun;
    if (Word[1] != 0)
        LastNoun = Word[1];

    OutCaps();
    RunEventTable();

    if (ActionsExecuted == 0) {
        int OriginalVerb = Word[0];
        if (TryExtraCommand() == 0) {
            if (IsMoveCommand(OriginalVerb)) {
                SysMessage(YOU_CANT_GO_THAT_WAY);
            } else if (OriginalVerb) {
                SysMessage(THATS_BEYOND_MY_POWER);
            } else {
                SysMessage(I_DONT_UNDERSTAND_THAT_VERB);
            }
            OutFlush();
            StopTime = 1;
            return;
        } else {
            if (Redraw)
                Look();
            return;
        }
    }

    if (Redraw && !((BaseGame == REBEL_PLANET && MyLoc == REBEL_ROOM_INSIDE_KUBE) || (BaseGame == KAYLETH && MyLoc == KAYLETH_ROOM_AZAP_ELY))) {
        Look();
    }

    Redraw = 0;

    int waitflag = WaitFlag();

    if (waitflag != -1 && Flag[waitflag] > 1)
        Flag[waitflag]++;

    do {
        if (waitflag != -1 && Flag[waitflag]) {
            Flag[waitflag]--;
            if (LastChar != '\n')
                OutChar('\n');
        }

        if (Version == QUESTPROBE3_TYPE) {
            DrawImages = 0;
            RunStatusTable();
            QP3RedrawImages();
        } else {
            RunStatusTable();
        }

        if (Redraw) {
            Look();
        }
        Redraw = 0;

    } while (waitflag != -1 && Flag[waitflag] > 0);
    if (AnimationRunning)
        glk_request_timer_events(AnimationRunning);
    if (waitflag != -1)
        Flag[waitflag] = 0;
}

static void RestartGame(void)
{
    RecursionGuard = 0;
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    OutFlush();
    glk_window_clear(Bottom);
    Look();
    RunStatusTable();
    ShouldRestart = 0;
    Look();
}

/* Unpack the game file if it is a C64 container, and work out which game
   it is. Sets Game and FileBaselineOffset; does not return on failure. */
static void IdentifyGame(void)
{
    /* Step 1: load the game file. DetectC64 handles both raw image files
       and C64 disk-image containers, decrunching as needed; the file is
       installed into FileImage/FileImageLen on success. */
    if (DetectC64(&FileImage, &FileImageLen) != UNKNOWN_GAME) {
        EndOfData = FileImage + FileImageLen;
    } else {
        /* Not an error: every ZX Spectrum file ends up here */
        debug_print("Not a C64 image\n");
    }

    debug_print("Loaded %zu bytes.\n", FileImageLen);

    /* Step 2: locate the verb table by scanning for its signature */
    VerbBase = FindVerbTable();
    if (VerbBase == NOT_FOUND) {
        Fatal("This is not a TaylorMade game: no verb table found");
    }

    /* Step 3: identify which game we're playing. The verb table's offset
       relative to its expected (in-game) address tells us how far the
       loaded file's addressing differs from the original Z80 layout. */
    if (!Game)
        Game = DetectGame(VerbBase);
    if (Game == NULL) {
        Fatal("Did not recognize game");
    } else {
        FileBaselineOffset = VerbBase - Game->start_of_dictionary;
    }
    debug_print("FileBaselineOffset: %ld\n", FileBaselineOffset);
}

/* Step 5: initialize state and present the opening room. NewGame()
   resets flags and objects; GuessLowObjectEnd() determines which
   objects render inline vs. in the "You see:" list; the initial
   state is snapshotted for the RESTART command. */
static void StartGame(void)
{
    NewGame();
    NumLowObjects = GuessLowObjectEnd();
    InitialState = SaveCurrentState();

    /* Run the status table once on entry so any per-turn setup actions
       (e.g. drawing the first room image) execute before the prompt. */
    RunStatusTable();
    if (Redraw) {
        OutFlush();
        Look();
    }

    /* Kayleth begins with a non-interactive "preview" sequence before
       handing control to the player; load its animation data and arm
       the flag so that undo is suppressed until MyLoc reaches 1. */
    if (BaseGame == KAYLETH) {
        LoadKaylethAnimationData();
        InKaylethPreview = 1;
    }
}

/* Main entry point. Loads and identifies the game file, locates all data
   tables, initializes graphics, and enters the main input/action loop. */
void glk_main(void)
{
    IdentifyGame();

    /* Seed the RNG. In determinism mode (used by automated tests and
       transcript playback) we use a fixed seed; otherwise time-based. */
#ifdef SPATTERLIGHT
    if (gli_determinism) {
        set_erkyrath_random(1234);
    } else
#endif
    set_erkyrath_random(0);

    DisplayInit();

    /* If the loaded file carried a ZX Spectrum loading screen, show it as
       a title image before the game proper begins. */
    DrawZXTitleImage();

    /* Temple of Terror ships as a paired text+graphics duo; check whether
       a companion file is present alongside the loaded one. */
    if (CurrentGame == TEMPLE_OF_TERROR || CurrentGame == TOT_TEXT_ONLY) {
        LookForSecondTOTGame();
    }

    /* Cache the dictionary code for "GO" — used by the implicit-verb
       fallback when the player types just a direction. */
    GoVerb = ParseWord("GO");

    InitGraphics();

    /* QP3 and Rebel Planet separate top and lower text windows by
       a row of '=' rather than the default '_'. */
    if (CurrentGame == QUESTPROBE3 || CurrentGame == REBEL_PLANET)
        DelimiterChar = '=';

    /* Step 4: locate all the remaining tables (tokens, messages, rooms,
       actions, etc.) using game-specific offsets or signature scans. */
    FindTables();
    InitActionDebugging();

    StartGame();

    /* Main turn loop: snapshot undo state, read a command, execute it,
       and repeat forever. Exits are handled by glk_exit() from within
       QUIT/restart paths. */
    while (1) {
        if (ShouldRestart) {
            RestartGame();
            /* Kayleth's restart sequence is itself non-interactive, so
               don't capture undo until it's done. */
            if (BaseGame != KAYLETH) {
                SaveUndo();
            }
        } else if (!StopTime && !InKaylethPreview) {
            /* Take an undo snapshot before each real player turn, but
               skip while time is paused or the preview is still running. */
            SaveUndo();
        }
        Parser();
        FirstAfterInput = 1;
        RunOneInput();
        if (StopTime) {
            StopTime--;
        } else if (!InKaylethPreview) {
            JustStarted = 0;
        }
        /* Kayleth's preview ends when the player is put in room 1. */
        if (MyLoc == 1) {
            InKaylethPreview = 0;
        }
    }
}

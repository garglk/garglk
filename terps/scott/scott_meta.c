/*
 *  scott_meta.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Save/load, restart, transcript, undo, and meta-command handling.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

#include "restore_state.h"

#include "parser.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"

#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif

/* Prompt for a file and open a (Unicode, if unicode is set) stream on it.
   Returns NULL if the player cancels the prompt, or if the file can't be
   opened, after printing failure_message if it is non-NULL. */
static strid_t OpenPromptedStream(glui32 usage, glui32 mode, int unicode,
    const char *failure_message)
{
    frefid_t ref = glk_fileref_create_by_prompt(usage, mode, 0);
    if (ref == NULL)
        return NULL;

    strid_t file;
    if (unicode)
        file = glk_stream_open_file_uni(ref, mode, 0);
    else
        file = glk_stream_open_file(ref, mode, 0);
    glk_fileref_destroy(ref);

    if (file == NULL && failure_message != NULL)
        Output(failure_message);
    return file;
}

/* Save the current game state to a file via Glk file prompts.
   Writes counters, room flags, bit flags, location, and item positions. */
void SaveGame(void)
{
    int ct;
    char buf[128];

    strid_t file = OpenPromptedStream(fileusage_TextMode | fileusage_SavedGame,
        filemode_Write, 0, NULL);
    if (file == NULL)
        return;

    SavedState *s = SaveCurrentState();

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        snprintf(buf, sizeof buf, "%d %d\n", s->Counters[ct], s->RoomSaved[ct]);
        glk_put_string_stream(file, buf);
    }
    snprintf(buf, sizeof buf, "%lld %d %hd %d %d %hd %d\n",
        (long long)s->BitFlags, (int)((s->BitFlags >> DARKBIT) & 1),
        (short)s->CurrentLoc, s->CurrentCounter,
        s->SavedRoom, (short)s->LightTime, s->AutoInventory);
    glk_put_string_stream(file, buf);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%hd\n", (short)s->ItemLocations[ct]);
        glk_put_string_stream(file, buf);
    }

    FreeSavedState(s);
    glk_stream_close(file, NULL);
    Output(sys[SAVED]);
}

/* Read a saved game from an open stream into the snapshot s,
   validating every value against the current database limits.
   Returns 0 as soon as anything is missing or out of range. */
static int ReadSavedGame(strid_t file, SavedState *s)
{
    char buf[128];
    int ct;
    short lo;
    short DarkFlag, Loc, LightTime;

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        if (glk_get_line_stream(file, buf, sizeof buf) == 0
            || sscanf(buf, "%d %d", &s->Counters[ct], &s->RoomSaved[ct]) != 2
            || s->RoomSaved[ct] < 0 || s->RoomSaved[ct] > GameHeader.NumRooms)
            return 0;
    }

    if (glk_get_line_stream(file, buf, sizeof buf) == 0)
        return 0;
    long long SavedBitFlags = 0;
    /* Older saves have no AutoInventory field, which then keeps the
       current value that s was created with */
    int result = sscanf(buf, "%lld %hd %hd %d %d %hd %d\n", &SavedBitFlags,
        &DarkFlag, &Loc, &s->CurrentCounter, &s->SavedRoom, &LightTime,
        &s->AutoInventory);
    if ((result != 7 && result != 6) || Loc > GameHeader.NumRooms || Loc < 1
        || s->SavedRoom < 0 || s->SavedRoom > GameHeader.NumRooms)
        return 0;
    s->BitFlags = (uint64_t)SavedBitFlags;
    s->CurrentLoc = Loc;
    s->LightTime = LightTime;

    /* Backward compatibility */
    if (DarkFlag)
        s->BitFlags |= (uint64_t)1 << DARKBIT;

    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        if (glk_get_line_stream(file, buf, sizeof buf) == 0
            || sscanf(buf, "%hd\n", &lo) != 1
            || lo < 0 || (lo > GameHeader.NumRooms && lo != CARRIED))
            return 0;
        s->ItemLocations[ct] = (unsigned char)lo;
    }

    /* There should be nothing left in the file */
    glui32 position = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_End);
    return glk_stream_get_position(file) == position;
}

/* Load a saved game state from a file via Glk file prompts.
   The file is read into a snapshot, which only replaces the live
   state if every value in it is within the current database limits. */
void LoadGame(void)
{
    strid_t file = OpenPromptedStream(fileusage_TextMode | fileusage_SavedGame,
        filemode_Read, 0, NULL);
    if (file == NULL)
        return;

    SavedState *state = SaveCurrentState();

    int success = ReadSavedGame(file, state);
    glk_stream_close(file, NULL);

    if (!success)
        Output(sys[BAD_DATA]);
    else
        RestoreState(state);
    FreeSavedState(state);

    /* The turn is not over, whether the restore worked or not */
    if (StopTime == 0)
        StopTime = 1;
    should_draw_image = 1;
    if (!success)
        return;

    SaveUndo();
    JustStarted = 0;
    StopTime = 1;
    should_look_in_transcript = 1;
}

void RestartGame(void)
{
    if (CurrentCommand)
        FreeCommands();
    RestoreState(InitialState);
    JustStarted = 0;
    StopTime = 0;
    Display(Bottom, "\n\n");
    glk_window_clear(Bottom);
    OpenTopWindow();
    should_restart = 0;
}

static void TranscriptOn(void)
{
    if (Transcript) {
        Output(sys[TRANSCRIPT_ALREADY]);
        return;
    }

    Transcript = OpenPromptedStream(fileusage_TextMode | fileusage_Transcript,
        filemode_Write, 1, sys[FAILED_TRANSCRIPT]);
    if (Transcript == NULL)
        return;

    PutUnicodeString(Transcript, sys[TRANSCRIPT_START]);

    print_look_to_transcript = 1;
    Look();

    glk_put_string_stream(glk_window_get_stream(Bottom),
        (char *)sys[TRANSCRIPT_ON]);
    glk_window_set_echo_stream(Bottom, Transcript);
}

static void TranscriptOff(void)
{
    if (Transcript == NULL) {
        Output(sys[NO_TRANSCRIPT]);
        return;
    }

    glk_window_set_echo_stream(Bottom, NULL);

    PutUnicodeString(Transcript, sys[TRANSCRIPT_END]);

    glk_stream_close(Transcript, NULL);
    Transcript = NULL;
    Output(sys[TRANSCRIPT_OFF]);
}

static void FlickerOn(void)
{
    if (Options & FLICKER_ON) {
        Output("Flicker is already on");
    } else {
        Output("Flicker is now on");
    }
    if (Options & NO_DELAYS)
        Output(" (but delays are off, so it won't have any effect.)");
    else Output(".");
    Output("\n");
    Options |= FLICKER_ON;
}

static void FlickerOff(void)
{
    if (Options & FLICKER_ON) {
        Output("Flicker is now off.");
    } else {
        Output("Flicker is already off.");
    }
    Output("\n");
    Options &= ~FLICKER_ON;
}

/* Handle meta-commands that aren't part of the game's action table:
   save, restore, restart, undo, RAM save/load, transcript, and flicker.
   Returns 1 if the command was handled, 0 otherwise. */
int PerformExtraCommand(int extra_stop_time)
{
    Command command = *CurrentCommand;
    int verb = command.verb;
    if (verb > GameHeader.NumWords)
        verb -= GameHeader.NumWords;
    int noun = command.noun;
    if (noun > GameHeader.NumWords)
        noun -= GameHeader.NumWords;
    else if (noun) {
        int newnoun = FindExtraNoun(CurrentCommand->nounwordindex);
        if (newnoun)
            noun = newnoun;
    }

    StopTime = 1 + extra_stop_time;

    switch (verb) {
    case RESTORE:
        if (noun == 0 || noun == GAME) {
            LoadGame();
            return 1;
        }
        break;
    case RESTART:
        if (noun == 0 || noun == GAME) {
            Output(sys[ARE_YOU_SURE]);
            if (YesOrNo()) {
                should_restart = 1;
            }
            return 1;
        }
        break;
    case SAVE:
        if (noun == 0 || noun == GAME) {
            SaveGame();
            return 1;
        }
        break;
    case UNDO:
        if (noun == 0 || noun == COMMAND) {
            RestoreUndo();
            return 1;
        }
        break;
    case RAM:
        if (noun == RAMLOAD) {
            RamRestore();
            return 1;
        } else if (noun == RAMSAVE) {
            RamSave();
            return 1;
        }
        break;
    case RAMSAVE:
        if (noun == 0) {
            RamSave();
            return 1;
        }
        break;
    case RAMLOAD:
        if (noun == 0) {
            RamRestore();
            return 1;
        }
        break;
    case SCRIPT:
        if (noun == ON || noun == 0) {
            TranscriptOn();
            return 1;
        } else if (noun == OFF) {
            TranscriptOff();
            return 1;
        }
        break;
    case EXCEPT:
        /* A bare EXCEPT/BUT (not following ALL) is meaningless: drop the
           command chain and fall through to the "not handled" path so the
           caller prints "I don't understand". Without this break it fell into
           FLICKER and silently switched the flicker option on. */
        FreeCommands();
        break;
    case FLICKER:
        if (noun == ON || noun == 0) {
            FlickerOn();
            return 1;
        } else if (noun == OFF) {
            FlickerOff();
            return 1;
        }
        break;
    }

    StopTime = 0;
    return 0;
}

/* Request a keypress (a Unicode one if unicode is set) in the main window
   and wait for it, passing any other events to Updates(). */
static glui32 GetKeyPress(int unicode)
{
    if (unicode)
        glk_request_char_event_uni(Bottom);
    else
        glk_request_char_event(Bottom);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type != evtype_CharInput)
            Updates(ev);
    } while (ev.type != evtype_CharInput);

    return ev.val1;
}

/* Prompt for a yes/no response via single-character input.
   Uses the localized first characters of sys[YES] and sys[NO]. */
int YesOrNo(void)
{
    int result = 0;
    const char y = tolower((unsigned char)sys[YES][0]);
    const char n = tolower((unsigned char)sys[NO][0]);

    do {
        glui32 key = GetKeyPress(1);
        if ((glsi32)key > ' ') {
            glk_put_char_stream_uni(glk_window_get_stream(Bottom), key);
        }
        const char reply = key < 0x100 ? tolower((int)key) : 0;
        if (reply == y) {
            result = 1;
        } else if (reply == n) {
            result = 2;
        } else {
            Output("\n");
            Output(sys[ANSWER_YES_OR_NO]);
        }
    } while (result == 0);

    return (result == 1);
}

/* Present a numbered menu — an intro line followed by the given titles — for a
   compilation (disk or tape) that holds several games, and return the
   zero-based index the player chooses. Entries are keyed 1-9, then A.. for any
   beyond nine. Mirrors YesOrNo's single-character input handling, and clears
   the window afterwards so the game starts on a clean screen. */
int SelectGameFromMenu(const char *intro, const char **titles, int count)
{
    if (count <= 1)
        return 0;
    if (count > 35) /* 1-9 then A-Z */
        count = 35;

    if (intro != NULL) {
        Output(intro);
        Output("\n");
    }
    for (int i = 0; i < count; i++) {
        char label = (i < 9) ? (char)('1' + i) : (char)('A' + (i - 9));
        Display(Bottom, "\n%c. %s", label, titles[i]);
    }

    int choice = -1;
    do {
        glui32 c = GetKeyPress(1);
        int idx = -1;
        if (c >= '1' && c <= '9')
            idx = (int)(c - '1');
        else if (c >= 'A' && c <= 'Z')
            idx = (int)(c - 'A' + 9);
        else if (c >= 'a' && c <= 'z')
            idx = (int)(c - 'a' + 9);
        if (idx >= 0 && idx < count)
            choice = idx;
    } while (choice < 0);

    glk_window_clear(Bottom);
    return choice;
}

void HitEnter(void)
{
    while (GetKeyPress(0) != keycode_Return)
        ;
    showing_closeup = 0;
    should_draw_image = 1;
    return;
}

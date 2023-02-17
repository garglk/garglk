//
//  restorestate.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "common.h"
#include "animations.h"
#include "graphics.h"
#include "parseinput.h"
#include "restorestate.h"


#define MAX_UNDOS 100

/* JustStarted is only used for the error message "Can't undo on first move" */
extern int JustStarted;

extern uint8_t Flag[];
extern uint8_t ObjectLoc[];

extern winid_t Bottom;

int JustRestored = 0;
int JustRestarted = 0;
ImgType SavedImgType;
int SavedImgIndex;

struct SavedState *InitialState = NULL;
static struct SavedState *ramsave = NULL;
static struct SavedState *last_undo = NULL;
static struct SavedState *oldest_undo = NULL;

static int number_of_undos;

struct SavedState *SaveCurrentState(void)
{
    struct SavedState *s = (struct SavedState *)MemAlloc(sizeof(struct SavedState));

    memcpy(s->Counters, Counters, 64 * sizeof(uint16_t));
    for (int i = 0; i <= GameHeader.NumItems; i++)
        s->ObjectLocations[i] = Items[i].Location;

    s->BitFlags = BitFlags;
    s->ProtagonistString = ProtagonistString;
    s->LastImgType = LastImgType;
    s->LastImgIndex = LastImgIndex;

    s->previousState = NULL;
    s->nextState = NULL;

    return s;
}

void RecoverFromBadRestore(struct SavedState *state)
{
    SystemMessage(BAD_DATA);
    RestoreState(state);
    free(state);
}

void RestoreState(struct SavedState *state)
{
    StopAnimation();
    memcpy(Counters, state->Counters, 64 * sizeof(uint16_t));

    for (int i = 0; i <= GameHeader.NumItems; i++)
        Items[i].Location = state->ObjectLocations[i];

    BitFlags = state->BitFlags;
    ProtagonistString = state->ProtagonistString;

    SetBit(DRAWBIT);
    SetBit(STOPTIMEBIT);

    /*
     These bits say whether the web and Sandman animations are loaded,
     and we always want to reload them after restoring state.
     */
    if (CurrentGame == SPIDERMAN) {
        ResetBit(9);
        ResetBit(10);
    }
    if (JustRestarted && CurrentGame != BANZAI)
        Look(0);
    else
        Look(1);
    if (CurrentGame == FANTASTIC4) {
        LastImgType = state->LastImgType;
        LastImgIndex = state->LastImgIndex;
        if (LastImgType == IMG_SPECIAL) {
            DrawCloseup(LastImgIndex);
        } else if (LastImgType == IMG_OBJECT) {
            DrawItemImage(LastImgIndex);
        }
    }
}

void SaveUndo(void)
{
    if (last_undo == NULL) {
        last_undo = SaveCurrentState();
        oldest_undo = last_undo;
        number_of_undos = 1;
        return;
    }

    if (number_of_undos == 0)
        Fatal("Number of undos == 0 but last_undo != NULL!");

    last_undo->nextState = SaveCurrentState();
    struct SavedState *current = last_undo->nextState;
    current->previousState = last_undo;
    last_undo = current;
    if (number_of_undos == MAX_UNDOS) {
        struct SavedState *oldest = oldest_undo;
        oldest_undo = oldest_undo->nextState;
        oldest_undo->previousState = NULL;
        free(oldest);
    } else {
        number_of_undos++;
    }
}

void RestoreUndo(int game)
{
    if (JustStarted) {
        SystemMessage(CANT_UNDO_ON_FIRST_TURN);
        return;
    }
    if (last_undo == NULL || last_undo->previousState == NULL) {
        SystemMessage(NO_UNDO_STATES);
        return;
    }

    if (game)
        SystemMessage(MOVE_UNDONE);

    struct SavedState *current = last_undo;
    last_undo = current->previousState;
    if (last_undo->previousState == NULL)
        oldest_undo = last_undo;
    RestoreState(last_undo);
    free(current);
    number_of_undos--;
}

void RamSave(int game)
{
    if (ramsave != NULL) {
        Display(Bottom, "Previous state deleted. ");
        free(ramsave);
    }

    ramsave = SaveCurrentState();
    if (game)
        SystemMessage(STATE_SAVED);
}

void RamLoad(void)
{
    if (ramsave == NULL) {
        SystemMessage(NO_SAVED_STATE);
        return;
    }

    SystemMessage(STATE_RESTORED);
    RestoreState(ramsave);
    SaveUndo();
}

void SaveGame(void)
{
    strid_t file;
    frefid_t ref;
    int ct, dummy = 0;
    char buf[128];

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
                                       filemode_Write, 0);
    if (ref == NULL)
        return;

    file = glk_stream_open_file(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return;

    for (ct = 0; ct < 64; ct++) {
        snprintf(buf, sizeof buf, "%d\n", Counters[ct]);
        glk_put_string_stream(file, buf);
    }
    snprintf(buf, sizeof buf, "%llu %d %d %d %d\n", (unsigned long long)BitFlags, ProtagonistString, dummy, (int)LastImgType, LastImgIndex);
    glk_put_string_stream(file, buf);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%hd\n", (short)Items[ct].Location);
        glk_put_string_stream(file, buf);
    }

    glk_stream_close(file, NULL);
    SystemMessage(SAVED);
    SetBit(STOPTIMEBIT);
}

int LoadGame(void)
{
    strid_t file;
    frefid_t ref;
    char buf[128];
    int ct = 0, dummy;
    short lo;

    ref = glk_fileref_create_by_prompt(fileusage_TextMode | fileusage_SavedGame,
                                       filemode_Read, 0);
    if (ref == NULL)
        return 0;

    file = glk_stream_open_file(ref, filemode_Read, 0);
    glk_fileref_destroy(ref);
    if (file == NULL)
        return 0;

    struct SavedState *state = SaveCurrentState();

    int result;

    for (ct = 0; ct < 64; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%" SCNu16, &Counters[ct]);
        if (result != 1) {
            RecoverFromBadRestore(state);
            return 0;
        }
    }
    glk_get_line_stream(file, buf, sizeof buf);
    int SavedImgTypeInt;
    result = sscanf(buf, "%" SCNu64 " %d %d %d %d\n", &BitFlags,  &ProtagonistString,
                    &dummy, &SavedImgTypeInt, &SavedImgIndex);
    SavedImgType = SavedImgTypeInt;
    debug_print("LoadGame: Result of sscanf: %d\n", result);
    if ((result < 3) || MyLoc > GameHeader.NumRooms || MyLoc < 1) {
        RecoverFromBadRestore(state);
        return 0;
    }

    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        glk_get_line_stream(file, buf, sizeof buf);
        result = sscanf(buf, "%hd\n", &lo);
        Items[ct].Location = (unsigned char)lo;
        if (result != 1 || (Items[ct].Location > GameHeader.NumRooms &&
                            Items[ct].Location != CARRIED &&
                            Items[ct].Location != HIDDEN &&
                            Items[ct].Location != HELD_BY_OTHER_GUY)) {
            fprintf(stderr, "LoadGame: Unexpected item location in save game file (Item %d, %s, is in room %d)\n", ct, Items[ct].Text, Items[ct].Location);
            RecoverFromBadRestore(state);
            return 0;
        }
    }

    glui32 position = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_End);
    glui32 end = glk_stream_get_position(file);
    if (end != position) {
        fprintf(stderr, "LoadGame: Save game file has unexpected length (%d, expected %d)\n", end, position);
        RecoverFromBadRestore(state);
        return 0;
    }

    JustStarted = 0;
    ClearAnimationBuffer();
    LastImgType = SavedImgType;
    LastImgIndex = SavedImgIndex;

    SetBit(DRAWBIT);
    Look(0);

    if (LastImgType == IMG_SPECIAL) {
        DrawCloseup(LastImgIndex);
    } else if (LastImgType == IMG_OBJECT) {
        DrawItemImage(LastImgIndex);
    }

    SaveUndo();
    JustRestored = 1;

    return 1;
}

void RestartGame(void)
{
    FreeInputWords();
    JustRestarted = 1;
    RestoreState(InitialState);
    JustStarted = 0;
    lastwasnewline = 1;
    ResetBit(STOPTIMEBIT);
    ClearAnimationBuffer();
    glk_window_clear(Bottom);
    OpenTopWindow();
    should_restart = 0;
}

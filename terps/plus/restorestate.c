//
//  restorestate.c
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "animations.h"
#include "common.h"
#include "glk.h"
#include "graphics.h"
#include "parseinput.h"
#include "restorestate.h"

#define MAX_UNDOS 100

int JustRestored = 0;
int JustRestarted = 0;
ImgType RestoredImgType = NO_IMG;
int RestoredImgIndex;

SavedState *InitialState = NULL;
static SavedState *ramsave = NULL;
static SavedState *last_undo = NULL;
static SavedState *oldest_undo = NULL;

static int number_of_undos;

SavedState *SaveCurrentState(void)
{
    size_t items = GameHeader.NumItems + 1, rooms = GameHeader.NumRooms + 1;
    /* The exits are ints, so they go first after the struct for alignment */
    SavedState *s = (SavedState *)MemAlloc(sizeof(SavedState)
        + rooms * sizeof s->RoomExits[0] + 2 * items * sizeof(uint16_t));
    s->RoomExits = (int (*)[NUM_SAVED_EXIT_COLUMNS])(s + 1);
    s->ObjectLocations = (uint16_t *)(s->RoomExits + rooms);
    s->ItemFlags = s->ObjectLocations + items;

    memcpy(s->Counters, Counters, sizeof s->Counters);
    for (size_t i = 0; i < items; i++) {
        s->ObjectLocations[i] = Items[i].Location;
        s->ItemFlags[i] = Items[i].Flag;
    }
    for (size_t i = 0; i < rooms; i++)
        memcpy(s->RoomExits[i], Rooms[i].Exits, sizeof s->RoomExits[i]);

    s->BitFlags = BitFlags;
    s->ProtagonistString = ProtagonistString;
    s->LastImgType = LastImgType;
    s->LastImgIndex = LastImgIndex;

    s->previousState = NULL;
    s->nextState = NULL;

    return s;
}

/* Give up loading a saved game: close the file, tell the player, and go
   back to the state we were in before we began reading it. */
static void RecoverFromBadRestore(SavedState *state, strid_t file)
{
    glk_stream_close(file, NULL);
    SystemMessage(BAD_DATA);
    RestoreState(state);
    free(state);
}

void RestoreState(SavedState *state)
{
    StopAnimation();
    memcpy(Counters, state->Counters, sizeof state->Counters);

    for (int i = 0; i <= GameHeader.NumItems; i++) {
        Items[i].Location = state->ObjectLocations[i];
        Items[i].Flag = state->ItemFlags[i];
    }
    for (int i = 0; i <= GameHeader.NumRooms; i++)
        memcpy(Rooms[i].Exits, state->RoomExits[i], sizeof state->RoomExits[i]);

    BitFlags = state->BitFlags;
    ProtagonistString = state->ProtagonistString;

    SetBit(DRAWBIT);
    SetBit(STOPTIMEBIT);

    /*
     These bits say whether the web and Sandman animations are loaded,
     and we always want to reload them after restoring state.
     */
    if (CurrentGame == SPIDERMAN) {
        ResetBit(SPIDERMAN_WEB_LOADED_BIT);
        ResetBit(SPIDERMAN_SANDMAN_LOADED_BIT);
    }
    if (JustRestarted && CurrentGame != BANZAI)
        Look(0);
    else
        Look(1);
    if (CurrentGame == FANTASTIC4) {
        LastImgType = state->LastImgType;
        LastImgIndex = state->LastImgIndex;
        DrawOverlayImage(LastImgType, LastImgIndex);
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
    SavedState *current = last_undo->nextState;
    current->previousState = last_undo;
    last_undo = current;
    if (number_of_undos == MAX_UNDOS) {
        SavedState *oldest = oldest_undo;
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

    SavedState *current = last_undo;
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

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        snprintf(buf, sizeof buf, "%d\n", Counters[ct]);
        glk_put_string_stream(file, buf);
    }
    snprintf(buf, sizeof buf, "%llu %d %d %d %d\n", (unsigned long long)BitFlags, ProtagonistString, dummy, (int)LastImgType, LastImgIndex);
    glk_put_string_stream(file, buf);
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%hd\n", (short)Items[ct].Location);
        glk_put_string_stream(file, buf);
    }
    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        snprintf(buf, sizeof buf, "%u\n", (unsigned)Items[ct].Flag);
        glk_put_string_stream(file, buf);
    }
    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        int *e = Rooms[ct].Exits;
        snprintf(buf, sizeof buf, "%d %d %d %d %d %d %d\n", e[0], e[1], e[2], e[3], e[4], e[5], e[6]);
        glk_put_string_stream(file, buf);
    }

    /* Glk reports no write errors, so there is nothing to check here */
    glk_stream_close(file, NULL);
    SystemMessage(SAVED);
    SetBit(STOPTIMEBIT);
}

/* Read one line of a saved game into buf. Returns 0 at end of file, so a
   short file fails explicitly rather than through sscanf on an empty line. */
static int ReadSaveLine(strid_t file, char *buf, glui32 size)
{
    return glk_get_line_stream(file, buf, size) != 0;
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

    SavedState *state = SaveCurrentState();

    int result;

    for (ct = 0; ct < NUM_COUNTERS; ct++) {
        if (!ReadSaveLine(file, buf, sizeof buf) || sscanf(buf, "%" SCNd16, &Counters[ct]) != 1) {
            RecoverFromBadRestore(state, file);
            return 0;
        }
    }
    int imgtype = NO_IMG, imgindex = 0;
    result = 0;
    if (ReadSaveLine(file, buf, sizeof buf))
        result = sscanf(buf, "%" SCNu64 " %d %d %d %d\n", &BitFlags, &ProtagonistString,
            &dummy, &imgtype, &imgindex);
    debug_print("LoadGame: Result of sscanf: %d\n", result);
    if ((result < 3) || MyLoc > GameHeader.NumRooms || MyLoc < 1) {
        RecoverFromBadRestore(state, file);
        return 0;
    }
    /* The image fields are optional; ignore them if missing or garbled */
    if (result < 5 || imgtype < NO_IMG || imgtype > IMG_SPECIAL)
        imgtype = NO_IMG;

    for (ct = 0; ct <= GameHeader.NumItems; ct++) {
        result = 0;
        if (ReadSaveLine(file, buf, sizeof buf))
            result = sscanf(buf, "%hd\n", &lo);
        /* Any non-negative room is possible: an action may put an item in
           a room that does not exist (X-Men uses 500 for used-up items). */
        if (result != 1 || lo < 0) {
            debug_print("LoadGame: Unexpected item location in save game file (Item %d, %s)\n", ct, Items[ct].Text);
            RecoverFromBadRestore(state, file);
            return 0;
        }
        Items[ct].Location = (uint16_t)lo;
    }

    /* The item flags and the room exits were added later. A file that
       ends after the item locations leaves them as they are. */
    if (ReadSaveLine(file, buf, sizeof buf)) {
        for (ct = 0; ct <= GameHeader.NumItems; ct++) {
            unsigned flag;
            if ((ct > 0 && !ReadSaveLine(file, buf, sizeof buf)) || sscanf(buf, "%u", &flag) != 1 || flag > UINT16_MAX) {
                debug_print("LoadGame: Unexpected item flags in save game file (Item %d)\n", ct);
                RecoverFromBadRestore(state, file);
                return 0;
            }
            Items[ct].Flag = (uint16_t)flag;
        }
        for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
            int *e = Rooms[ct].Exits;
            if (!ReadSaveLine(file, buf, sizeof buf) || sscanf(buf, "%d %d %d %d %d %d %d", &e[0], &e[1], &e[2], &e[3], &e[4], &e[5], &e[6]) != NUM_SAVED_EXIT_COLUMNS) {
                debug_print("LoadGame: Unexpected room exits in save game file (Room %d)\n", ct);
                RecoverFromBadRestore(state, file);
                return 0;
            }
        }
    }

    glui32 position = glk_stream_get_position(file);
    glk_stream_set_position(file, 0, seekmode_End);
    glui32 end = glk_stream_get_position(file);
    if (end != position) {
        debug_print("LoadGame: Save game file has unexpected length (%u, expected %u)\n", end, position);
        RecoverFromBadRestore(state, file);
        return 0;
    }

    glk_stream_close(file, NULL);
    free(state);

    JustStarted = 0;
    ClearAnimationBuffer();
    LastImgType = imgtype;
    LastImgIndex = imgindex;
    RestoredImgType = LastImgType;
    RestoredImgIndex = LastImgIndex;

    SetBit(DRAWBIT);
    Look(0);

    DrawOverlayImage(LastImgType, LastImgIndex);

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

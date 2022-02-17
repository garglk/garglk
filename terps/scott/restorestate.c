//
//  restorestate.c
//  scott
//
//  Created by Administrator on 2022-01-10.
//
#include <stdlib.h>

#include "restorestate.h"

#include "scott.h"

#define MAX_UNDOS 100

extern int CurrentCounter;
extern int RoomSaved[]; /* Range unknown */

extern int stop_time;
extern int just_started;

struct SavedState *initial_state;
static struct SavedState *ramsave = NULL;
static struct SavedState *last_undo = NULL;
static struct SavedState *oldest_undo = NULL;

static int number_of_undos;

struct SavedState *SaveCurrentState(void)
{
    struct SavedState *s = (struct SavedState *)MemAlloc(sizeof(struct SavedState));
    for (int ct = 0; ct < 16; ct++) {
        s->Counters[ct] = Counters[ct];
        s->RoomSaved[ct] = RoomSaved[ct];
    }

    s->BitFlags = BitFlags;
    s->CurrentLoc = MyLoc;
    s->CurrentCounter = CurrentCounter;
    s->SavedRoom = SavedRoom;
    s->LightTime = GameHeader.LightTime;
    s->AutoInventory = AutoInventory;

    s->ItemLocations = MemAlloc(GameHeader.NumItems + 1);

    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        s->ItemLocations[ct] = Items[ct].Location;
    }

    s->previousState = NULL;
    s->nextState = NULL;

    return s;
}

void RecoverFromBadRestore(struct SavedState *state)
{
    Output("BAD DATA! Invalid save file.\n");
    RestoreState(state);
    free(state);
}

void RestoreState(struct SavedState *state)
{
    for (int ct = 0; ct < 16; ct++) {
        Counters[ct] = state->Counters[ct];
        RoomSaved[ct] = state->RoomSaved[ct];
    }

    BitFlags = state->BitFlags;

    MyLoc = state->CurrentLoc;
    CurrentCounter = state->CurrentCounter;
    SavedRoom = state->SavedRoom;
    GameHeader.LightTime = state->LightTime;
    AutoInventory = state->AutoInventory;

    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        Items[ct].Location = state->ItemLocations[ct];
    }

    stop_time = 1;
    dead = 0;
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
        free(oldest->ItemLocations);
        free(oldest);
    } else {
        number_of_undos++;
    }
}

void RestoreUndo(void)
{
    if (just_started) {
        Output("Can't undo on first turn.\n");
        return;
    }
    if (last_undo == NULL || last_undo->previousState == NULL) {
        Output("No more undo states stored.\n");
        return;
    }
    struct SavedState *current = last_undo;
    last_undo = current->previousState;
    if (last_undo->previousState == NULL)
        oldest_undo = last_undo;
    RestoreState(last_undo);
    Output(sys[MOVE_UNDONE]);
    free(current->ItemLocations);
    free(current);
    number_of_undos--;
}

void RamSave(void)
{
    if (ramsave != NULL) {
        free(ramsave->ItemLocations);
        free(ramsave);
    }

    ramsave = SaveCurrentState();
    Output("State saved.\n");
}

void RamRestore(void)
{
    if (ramsave == NULL) {
        Output("No saved state exists.\n");
        return;
    }

    RestoreState(ramsave);
    Output("State restored.\n");
    SaveUndo();
}

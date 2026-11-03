//
//  restore_state.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <stdlib.h>

#include "restore_state.h"

#include "scott.h"

#define MAX_UNDOS 100

SavedState *InitialState = NULL;
static SavedState *ramsave = NULL;
static SavedState *last_undo = NULL;
static SavedState *oldest_undo = NULL;

static int number_of_undos;

SavedState *SaveCurrentState(void)
{
    SavedState *s = (SavedState *)MemAlloc(sizeof(SavedState));
    for (int ct = 0; ct < NUM_COUNTERS; ct++) {
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

void FreeSavedState(SavedState *state)
{
    if (state == NULL)
        return;
    free(state->ItemLocations);
    free(state);
}

void RestoreState(SavedState *state)
{
    for (int ct = 0; ct < NUM_COUNTERS; ct++) {
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

    if (StopTime == 0)
        StopTime = 1;
    
    should_draw_image = 1;
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
        FreeSavedState(oldest);
    } else {
        number_of_undos++;
    }
}

void RestoreUndo(void)
{
    if (JustStarted) {
        Output(sys[CANT_UNDO_ON_FIRST_TURN]);
        return;
    }
    if (last_undo == NULL || last_undo->previousState == NULL) {
        Output(sys[NO_UNDO_STATES]);
        return;
    }
    SavedState *current = last_undo;
    last_undo = current->previousState;
    if (last_undo->previousState == NULL)
        oldest_undo = last_undo;
    RestoreState(last_undo);
    Output(sys[MOVE_UNDONE]);
    FreeSavedState(current);
    number_of_undos--;
}

void RamSave(void)
{
    FreeSavedState(ramsave);
    ramsave = SaveCurrentState();
    Output(sys[STATE_SAVED]);
}

void RamRestore(void)
{
    if (ramsave == NULL) {
        Output(sys[NO_SAVED_STATE]);
        return;
    }

    RestoreState(ramsave);
    Output(sys[STATE_RESTORED]);
    SaveUndo();
}

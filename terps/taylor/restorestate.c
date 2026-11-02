//
//  restorestate.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <string.h>

#include "glk.h"
#include "player.h"
#include "restorestate.h"
#include "utility.h"

#define MAX_UNDOS 100

int just_undid = 0;

SavedState *InitialState = NULL;
static SavedState *ramsave = NULL;
static SavedState *last_undo = NULL;
static SavedState *oldest_undo = NULL;

static int number_of_undos;

SavedState *SaveCurrentState(void)
{
    SavedState *s = (SavedState *)MemAlloc(sizeof(SavedState));

    memcpy(s->Flags, Flag, sizeof(s->Flags));
    memcpy(s->ObjectLocations, ObjectLoc, sizeof(s->ObjectLocations));

    s->previousState = NULL;
    s->nextState = NULL;

    return s;
}

void RecoverFromBadRestore(SavedState *state)
{
    Display(Bottom, "BAD DATA! Invalid save file.\n");
    RestoreState(state);
    free(state);
}

void RestoreState(SavedState *state)
{
    memcpy(Flag, state->Flags, sizeof(state->Flags));
    memcpy(ObjectLoc, state->ObjectLocations, sizeof(state->ObjectLocations));

    StopTime = 1;
}

void SaveUndo(void)
{
    if (just_undid) {
        just_undid = 0;
        return;
    }
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

void RestoreUndo(int from_player)
{
    just_undid = 1;

    if (JustStarted) {
        Display(Bottom, "You can't undo on first turn\n");
        return;
    }
    if (last_undo == NULL || last_undo->previousState == NULL) {
        Display(Bottom, "No undo states remaining\n");
        return;
    }
    SavedState *current = last_undo;
    last_undo = current->previousState;
    if (last_undo->previousState == NULL)
        oldest_undo = last_undo;
    RestoreState(last_undo);
    if (from_player) {
        Display(Bottom, "Move undone.\n");
    } else {
        just_undid = 0;
    }
    free(current);
    number_of_undos--;
}

void RamSave(void)
{
    if (ramsave != NULL) {
        Display(Bottom, "Previous state deleted. ");
        free(ramsave);
    }

    ramsave = SaveCurrentState();
    Display(Bottom, "State saved.\n");
}

void RamLoad(void)
{
    if (ramsave == NULL) {
        Display(Bottom, "No saved state exists.\n");
        return;
    }

    RestoreState(ramsave);
    Display(Bottom, "State restored.\n");
    SaveUndo();
}

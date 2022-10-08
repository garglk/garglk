//
//  restorestate.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <string.h>

#include "glk.h"
#include "restorestate.h"
#include "utility.h"

#define MAX_UNDOS 100

extern int StopTime;
extern int JustStarted;
int just_undid = 0;

extern uint8_t Flag[];
extern uint8_t ObjectLoc[];

extern winid_t Bottom;

struct SavedState *InitialState = NULL;
static struct SavedState *ramsave = NULL;
static struct SavedState *last_undo = NULL;
static struct SavedState *oldest_undo = NULL;

static int number_of_undos;

struct SavedState *SaveCurrentState(void)
{
    struct SavedState *s = (struct SavedState *)MemAlloc(sizeof(struct SavedState));

    memcpy(s->Flags, Flag, 128);
    memcpy(s->ObjectLocations, ObjectLoc, 256);

    s->previousState = NULL;
    s->nextState = NULL;

    return s;
}

void RecoverFromBadRestore(struct SavedState *state)
{
    Display(Bottom, "BAD DATA! Invalid save file.\n");
    RestoreState(state);
    free(state);
}

void RestoreState(struct SavedState *state)
{
    memcpy(Flag, state->Flags, 128);
    memcpy(ObjectLoc, state->ObjectLocations, 256);

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
        Display(Bottom, "You can't undo on first turn\n");
        return;
    }
    if (last_undo == NULL || last_undo->previousState == NULL) {
        Display(Bottom, "No undo states remaining\n");
        return;
    }
    struct SavedState *current = last_undo;
    last_undo = current->previousState;
    if (last_undo->previousState == NULL)
        oldest_undo = last_undo;
    RestoreState(last_undo);
    if (game)
        Display(Bottom, "Move undone.\n");
    free(current);
    number_of_undos--;
    just_undid = 1;
}

void RamSave(int game)
{
    if (ramsave != NULL) {
        Display(Bottom, "Previous state deleted. ");
        free(ramsave);
    }

    ramsave = SaveCurrentState();
    if (game)
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

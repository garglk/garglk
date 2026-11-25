/*
 *  restorestate.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-01-10.
 */

#ifndef restorestate_h
#define restorestate_h

#include <stdlib.h>

typedef struct SavedState {
    uint8_t Flags[128];
    uint8_t ObjectLocations[256];
    struct SavedState *previousState;
    struct SavedState *nextState;
} SavedState;

void SaveUndo(void);
void RestoreUndo(int game);
void RamSave(void);
void RamLoad(void);
SavedState *SaveCurrentState(void);
void RestoreState(SavedState *state);
void RecoverFromBadRestore(SavedState *state);

extern SavedState *InitialState;

#endif /* restorestate_h */

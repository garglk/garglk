//
//  restorestate.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restorestate_h
#define restorestate_h

#include <stdlib.h>

struct SavedState {
    uint8_t Flags[128];
    uint8_t ObjectLocations[256];
    struct SavedState *previousState;
    struct SavedState *nextState;
};

void SaveUndo(void);
void RestoreUndo(int game);
void RamSave(int game);
void RamLoad(void);
struct SavedState *SaveCurrentState(void);
void RestoreState(struct SavedState *state);
void RecoverFromBadRestore(struct SavedState *state);

extern struct SavedState *InitialState;

#endif /* restorestate_h */

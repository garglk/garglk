//
//  restore_state.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restore_state_h
#define restore_state_h

#include <stdint.h>

#include "scott.h"

typedef struct SavedState {
    int Counters[NUM_COUNTERS];
    int RoomSaved[NUM_COUNTERS];
    uint64_t BitFlags;
    int CurrentLoc;
    int CurrentCounter;
    int SavedRoom;
    int LightTime;
    int AutoInventory;
    uint8_t *ItemLocations;
    struct SavedState *previousState;
    struct SavedState *nextState;
} SavedState;

extern SavedState *InitialState;

void SaveUndo(void);
void RestoreUndo(void);
void RamSave(void);
void RamRestore(void);
SavedState *SaveCurrentState(void);
void RestoreState(SavedState *state);
void FreeSavedState(SavedState *state);

#endif /* restore_state_h */

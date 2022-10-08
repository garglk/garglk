//
//  restorestate.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restorestate_h
#define restorestate_h

#include <stdint.h>
#include <stdio.h>
#include <stdint.h>

struct SavedState {
    int Counters[16];
    int RoomSaved[16];
    long BitFlags;
    int CurrentLoc;
    int CurrentCounter;
    int SavedRoom;
    int LightTime;
    int AutoInventory;
    uint8_t *ItemLocations;
    struct SavedState *previousState;
    struct SavedState *nextState;
};

void SaveUndo(void);
void RestoreUndo(void);
void RamSave(void);
void RamRestore(void);
struct SavedState *SaveCurrentState(void);
void RestoreState(struct SavedState *state);
void RecoverFromBadRestore(struct SavedState *state);

#endif /* restorestate_h */

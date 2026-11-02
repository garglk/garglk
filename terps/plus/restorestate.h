//
//  restorestate.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restorestate_h
#define restorestate_h

#include <stdint.h>

#include "definitions.h"

/* The room columns a saved state keeps: the exits and the description, as
   the original engines save them. The picture column never changes. */
#define NUM_SAVED_EXIT_COLUMNS (EXIT_DESCRIPTION + 1)

/* One allocation, freed with free(): the per-item and per-room arrays sit
   after the struct, sized for the game in memory. */
typedef struct SavedState {
    int16_t Counters[NUM_COUNTERS];
    uint16_t *ObjectLocations;
    uint16_t *ItemFlags;
    int (*RoomExits)[NUM_SAVED_EXIT_COLUMNS];
    uint64_t BitFlags;
    int ProtagonistString;
    ImgType LastImgType;
    int LastImgIndex;
    struct SavedState *previousState;
    struct SavedState *nextState;
} SavedState;

extern SavedState *InitialState;

/* The overlay image a loaded saved game showed, redrawn by glk_main after
   the first Look() of the restored turn */
extern ImgType RestoredImgType;
extern int RestoredImgIndex;

/* Set after a saved game was loaded from file, until the next turn */
extern int JustRestored;
/* JustRestarted is only used to adjust newlines and room descriptions in the transcript */
extern int JustRestarted;
/* JustStarted is only used for the error message "Can't undo on first move" */
extern int JustStarted;

void SaveUndo(void);
void RestoreUndo(int game);
void RamSave(int game);
void RamLoad(void);
SavedState *SaveCurrentState(void);
void RestoreState(SavedState *state);
int LoadGame(void);
void SaveGame(void);
void RestartGame(void);

#endif /* restorestate_h */

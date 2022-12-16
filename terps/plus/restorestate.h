//
//  restorestate.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-10.
//

#ifndef restorestate_h
#define restorestate_h

struct SavedState {
    uint16_t Counters[64];
    uint8_t ObjectLocations[256];
    uint64_t BitFlags;
    int ProtagonistString;
    ImgType LastImgType;
    int LastImgIndex;
    struct SavedState *previousState;
    struct SavedState *nextState;
};

extern struct SavedState *InitialState;

extern ImgType SavedImgType;
extern int SavedImgIndex;
extern int JustRestored;

void SaveUndo(void);
void RestoreUndo(int game);
void RamSave(int game);
void RamLoad(void);
struct SavedState *SaveCurrentState(void);
void RestoreState(struct SavedState *state);
void RecoverFromBadRestore(struct SavedState *state);
int LoadGame(void);
void SaveGame(void);
void RestartGame(void);

#endif /* restorestate_h */

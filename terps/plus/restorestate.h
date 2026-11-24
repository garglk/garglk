/*
 *  restorestate.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-01-10.
 */

#ifndef restorestate_h
#define restorestate_h

#include "definitions.h"

typedef struct SavedState SavedState;

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

/* Add the current state to the undo list, dropping the oldest beyond the
   limit. The main loop calls this after each turn, so the newest entry is
   the state the player is in, and undo goes back to the one before it. */
void SaveUndo(void);
/* Go back one turn. If game is nonzero, say so with a message. */
void RestoreUndo(int game);
/* Keep one snapshot in memory, replacing any earlier one, apart from the
   undo list and not on disk. If game is nonzero, confirm with a message. */
void RamSave(int game);
/* Go back to the snapshot of RamSave(), which stays. This counts as a new
   undo state. */
void RamLoad(void);
/* A new snapshot of the state, for free() */
SavedState *SaveCurrentState(void);
/* Ask for a saved game and read it. On a bad file, puts the state back as it
   was. Returns 1 if the game was restored. */
int LoadGame(void);
void SaveGame(void);
/* Go back to InitialState and clear the text */
void RestartGame(void);

#endif /* restorestate_h */

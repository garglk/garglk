//
//  player.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2026-05-18.
//

#ifndef player_h
#define player_h

#include "taylor.h"

/* What a restore did. A file that was opened but rejected has had the
   state it found put back, and ends the turn like a good one. */
typedef enum {
    LOAD_NO_FILE = 0, /* The prompt was cancelled, or the file would not open */
    LOAD_BAD_DATA, /* Opened, but not a saved game of this interpreter */
    LOAD_OK
} LoadResult;

LoadResult LoadGame(void);
void SaveGame(void);
void Look(void);
void Inventory(void);
int YesOrNo(void);
int IsIntroRoom(void);

/* Queries and updates of the object/flag state. */
unsigned char Destroyed(void);
unsigned char Carried(void);
unsigned char Worn(void);
unsigned char NumObjects(void);
int DarkFlag(void);
int CarryItem(void);
void DropItem(void);
void Put(unsigned char obj, unsigned char loc);
int Present(unsigned char obj);
int HeldByPlayer(unsigned char obj);

extern uint8_t Flag[];
extern uint8_t ObjectLoc[];
extern int PrintedOK;
extern int Redraw;
extern int StopTime;
extern int JustStarted;
extern int ShouldRestart;
extern int NoGraphics;
extern char DelimiterChar;
extern GameInfo *Game;
extern int InKaylethPreview;

#endif /* player_h */

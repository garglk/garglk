//
//  gameload.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-05.
//

#ifndef gameload_h
#define gameload_h

#include "taylor.h"

/* What FindCode() and the table searches return for a missing pattern */
#define NOT_FOUND ((size_t)-1)

size_t FindCode(const char *code, size_t base, size_t len);
size_t FindVerbTable(void);
GameInfo *DetectGame(size_t LocalVerbBase);
GameInfo *FindGameByID(GameIDType id);
void FindTables(void);
int GuessLowObjectEnd(void);
void LookForSecondTOTGame(void);

extern uint8_t *FileImage;
extern uint8_t *EndOfData;
extern size_t FileImageLen;
extern long FileBaselineOffset;

/* Offsets of the game's data tables within FileImage. */
extern size_t VerbBase;
extern size_t TokenBase;
extern size_t MessageBase;
extern size_t Message2Base;
extern size_t RoomBase;
extern size_t ObjectBase;
extern size_t ExitBase;
extern size_t ObjLocBase;
extern size_t StatusBase;
extern size_t EventBase;
extern size_t FlagBase;
extern size_t AnimationData;

#endif /* gameload_h */

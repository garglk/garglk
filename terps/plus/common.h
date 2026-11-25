/*
 *  common.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-06-04.
 */

#ifndef common_h
#define common_h

#include "common_utils.h"
#include "common_file_utils.h"
#include "debugprint.h"
#include "definitions.h"
#include "memory_allocation.h"

#include "glk.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Lets the compiler check the arguments of a printf-style function: the
   format string is argument fmt, the variable arguments start at first. */
#ifdef __GNUC__
#define PLUS_PRINTF(fmt, first) __attribute__((__format__(__printf__, fmt, first)))
#else
#define PLUS_PRINTF(fmt, first)
#endif

void Output(const char *string);
void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);
void Updates(event_t ev);
void Display(winid_t w, const char *fmt, ...) PLUS_PRINTF(2, 3);

/* The 64 flags of BitFlags. An index out of range is reported and ignored. */
void SetBit(int bit);
void ResetBit(int bit);
int IsSet(int bit);
int IsDark(void);

int GetDictWord(int group);
void OpenTopWindow(void);
void Look(int transcript);
Room *PlayerRoom(void);
void SystemMessage(SysMessageType message);
void SetTimer(glui32 milliseconds);

extern glui32 TimerRate;

extern const char *sys[];

extern int16_t Counters[];

extern ObjectImage *ObjectImages;
extern char **Messages;
extern Action *Actions;

extern uint64_t BitFlags;

extern winid_t Bottom;

extern char *DirPath;
extern size_t DirPathLength;
extern const char *game_file;

extern Header GameHeader;

extern Item *Items;
extern Room *Rooms;

extern int lastwasnewline;
extern int should_restart;

extern SystemType CurrentSys;
extern const GameInfo *Game;

extern strid_t Transcript;

extern int showing_inventory;

extern uint8_t *mem;
extern size_t memlen;

#endif /* common_h */

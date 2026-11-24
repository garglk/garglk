/*
 *  textoutput.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-04-05.
 */

#ifndef textoutput_h
#define textoutput_h

#include "taylor.h"

/* Buffered character output with smart spacing and capitalization. */
void OutChar(char c);
void OutString(const char *p);
void OutFlush(void);
void OutCaps(void);
void OutReplace(char c);
void OutKillSpace(void);

/* Decoding and printing of the game's compressed text tables. */
unsigned char *TokenText(unsigned char n);
void Message(unsigned char message_index);
void Message2(unsigned int message_index);
void SysMessage(unsigned char message_index);
void PrintObject(unsigned char object_index);
void PrintRoom(unsigned char room_index);
void PrintNumber(unsigned char number);

void WriteToRoomDescriptionStream(const char *fmt, ...)
#ifdef __GNUC__
__attribute__((__format__(__printf__, 1, 2)))
#endif
;

extern char LastChar;
extern int PendSpace;
extern int FirstAfterInput;
extern int JustWrotePeriod;
extern strid_t room_description_stream;

#endif /* textoutput_h */

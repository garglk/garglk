/*
 *  parseinput.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-06-04.
 */

#ifndef parseinput_h
#define parseinput_h

/* The number of dictionary words kept from one command */
#define WORDS_PER_COMMAND 5

void FreeInputWords(void);
void Parser(void);
int ParseWord(const char *word);
int EqualsIgnoringCase(const char *a, const char *b);

extern char **InputWordStrings;
extern int FirstWordPosition;
extern int KnownWords;
extern int WordIndex;
extern int WordsInInput;
extern uint8_t Word[WORDS_PER_COMMAND];
extern int FoundExtraCommand;

#endif /* parseinput_h */

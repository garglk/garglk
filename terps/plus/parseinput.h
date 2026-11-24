/*
 *  parseinput.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-06-04.
 */

#ifndef parseinput_h
#define parseinput_h

#include "definitions.h"

extern DictWord *Verbs;
extern DictWord *Nouns;
extern DictWord *Adverbs;
extern DictWord *Prepositions;

extern int ProtagonistString;

extern int LastVerb, LastNoun, LastPrep, LastPartp, LastNoun2, LastAdverb;
extern int CurrentVerb, CurrentNoun, CurrentPrep, CurrentPartp, CurrentNoun2, CurrentAdverb;

int CompareUpToHashSign(const char *word1, const char *word2);
int YesOrNo(void);
void FreeInputWords(void);
int GetInput(void);
int IsNextParticiple(int partp, int noun2);
int WithListNoun(void);
void StopProcessingCommand(void);

#endif /* parseinput_h */

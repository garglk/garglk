//
//  parseinput.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef parseinput_h
#define parseinput_h

#include <stdio.h>

int GetInput(void);
void StopProcessingCommand(void);
void FreeInputWords(void);
void Parser(void);
int ParseWord(char *p);

extern char **InputWordStrings;
extern int WordPositions[5];
extern int WordIndex;

#endif /* parseinput_h */

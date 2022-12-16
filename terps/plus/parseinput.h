//
//  parseinput.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-06-04.
//

#ifndef parseinput_h
#define parseinput_h

#include <stdio.h>

void FreeInputWords(void);
int GetInput(void);
int IsNextParticiple(int partp, int noun2);
void StopProcessingCommand(void);

#endif /* parseinput_h */

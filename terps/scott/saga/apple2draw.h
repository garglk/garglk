//
//  apple2draw.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-17.
//

#ifndef apple2draw_h
#define apple2draw_h

#include "sagagraphics.h"

void ClearApple2ScreenMem(void);
void DrawApple2ImageFromVideoMem(void);
int DrawScrambledApple2Image(uint8_t *origptr, size_t datasize);
int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize);
int DrawApple2Image(USImage *image);

extern uint8_t *descrambletable;

#endif /* apple2draw_h */

//
//  stdraw.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//  Routines to draw Atari ST graphics
//
//  Created by Petter Sjölund on 2022-01-30.
//

#ifndef stdraw_h
#define stdraw_h

#include <stddef.h>
#include <stdint.h>

int DrawSTImageFromData(uint8_t *imgdata, size_t total_size);
/* Whether drawing this picture begins by clearing the graphics window */
int STImageClearsWindow(const uint8_t *imgdata, size_t total_size);
void UpdateColorCycling(void);

extern int ColorCyclingRunning;

#endif /* stdraw_h */

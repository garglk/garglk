/*
 *  stdraw.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  Routines to draw Atari ST graphics
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-01-30.
 */

#ifndef stdraw_h
#define stdraw_h

#include <stddef.h>
#include <stdint.h>

/* Returns 0 if the picture data is too short to draw */
int DrawSTImageFromData(uint8_t *imgdata, size_t total_size);
/* Whether drawing this picture begins by clearing the graphics window */
int STImageClearsWindow(const uint8_t *imgdata, size_t total_size);
/* Step the cycling colours one timer tick and repaint their pixels */
void UpdateColorCycling(void);

extern int ColorCyclingRunning;

#endif /* stdraw_h */

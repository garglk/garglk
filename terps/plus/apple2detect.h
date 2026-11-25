/*
 *  apple2detect.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-08-03.
 */

#ifndef apple2detect_h
#define apple2detect_h

#include <stddef.h>
#include <stdint.h>

/* Finds the database on an Apple II disk image or its companion, and makes it
   the buffer *sf. Returns 1 if found. The pictures are read by
   LookForApple2Images. */
int DetectApple2(uint8_t **sf, size_t *extent);
void LookForApple2Images(void);

void ReadApple2SpecialImages(const uint8_t *data, size_t offset, size_t size);

#endif /* apple2detect_h */

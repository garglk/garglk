/*
 *  c64detect.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-01-30.
 */

#ifndef c64detect_h
#define c64detect_h

#include <stddef.h>
#include <stdint.h>

/* Finds the database on a C64 disk image and makes it the buffer *sf. Returns
   1 if found. */
int DetectC64(uint8_t **sf, size_t *extent);
/* Mends the two object pictures of the C64 Spider-Man that draw in the wrong
   place */
void PatchSpidermanImages(void);

#endif /* c64detect_h */

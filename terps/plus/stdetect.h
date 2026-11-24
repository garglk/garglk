/*
 *  stdetect.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-08-02.
 */

#ifndef stdetect_h
#define stdetect_h

#include <stddef.h>
#include <stdint.h>

/* Finds the database and pictures on an Atari ST disk image, ST or MSA. Returns
   nonzero if found. */
int DetectST(uint8_t **sf, size_t *extent);

#endif /* stdetect_h */

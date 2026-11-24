/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  c64_small.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2026-01-23.
 */

#ifndef c64_small_h
#define c64_small_h

#include "c64decrunch.h"
#include "sagagraphics.h"

GameIDType handle_all_in_one(uint8_t **database, size_t *datasize, c64rec c64_registry);
int DrawMiniC64(USImage *img);

/* The C64 palette the pictures are drawn in */

#endif /* c64_small_graphics_h */

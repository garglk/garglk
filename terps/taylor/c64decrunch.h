/*
 *  c64decrunch.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-01-30.
 */

#ifndef c64decrunch_h
#define c64decrunch_h

#include "taylor.h"

GameIDType DetectC64(uint8_t **sf, size_t *extent);

#endif /* c64decrunch_h */

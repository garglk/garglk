/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  apple2detect.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-08-03.
 */

#ifndef apple2detect_h
#define apple2detect_h

#include "scott_defines.h"

GameIDType DetectApple2(uint8_t **sf, size_t *extent);
int LoadDrawingDataFromDisk(uint8_t *data, size_t datasize);

#endif /* apple2detect_h */

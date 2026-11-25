/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  atari8detect.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-01-30.
 */

#ifndef atari8detect_h
#define atari8detect_h

#include "scott_defines.h"

GameIDType DetectAtari8(uint8_t **sf, size_t *extent);

#endif /* atari8detect_h */

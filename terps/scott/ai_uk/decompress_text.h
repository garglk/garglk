/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  decompress_text.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Implements the text decompression scheme used by Robin of Sherwood
 *  and Seas of Blood.
 *
 *  Created by Petter Sjölund on 2022-01-10.
 */

#ifndef decompress_text_h
#define decompress_text_h

#include <stdint.h>
#include <stdlib.h>

char *DecompressText(uint8_t *source, int stringindex);

#endif /* decompress_text_h */

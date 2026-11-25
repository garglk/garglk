/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  load_ti99_4a.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-02-12.
 */

#ifndef load_ti99_4a_h
#define load_ti99_4a_h

#include "scott_defines.h"
#include <stdint.h>
#include <stdio.h>

GameIDType DetectTI994A(void);

extern uint8_t *ti99_implicit_actions;
extern uint8_t *ti99_explicit_actions;
extern size_t ti99_implicit_extent;
extern size_t ti99_explicit_extent;
extern uint8_t **VerbActionOffsets;

#endif /* load_ti99_4a_h */

/*
 *  companionfile.h
 *  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-10-10.
 */

#ifndef companionfile_h
#define companionfile_h

#include <stddef.h>
#include <stdint.h>

/* Reads the other disk side of gamefile, found by its name. Returns a new
   buffer and sets *size, or returns NULL if there is none. */
uint8_t *GetCompanionFile(const char *gamefile, size_t *size);

#endif /* companionfile_h */

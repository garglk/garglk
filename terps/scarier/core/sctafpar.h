/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Prototypes for sctafpar*.cpp: the TAF game parser.
 */

#ifndef SCARIER_TAFPAR_H
#define SCARIER_TAFPAR_H

#include "scprops.h"
#include "sctaffil.h"

/* Game parser enumeration and functions. */
enum
{ ROOMLIST_NO_ROOMS = 0,
  ROOMLIST_ONE_ROOM = 1,
  ROOMLIST_SOME_ROOMS = 2,
  ROOMLIST_ALL_ROOMS = 3,
  ROOMLIST_NPC_PART = 4
};

extern scr_bool parse_game (scr_tafref_t taf, scr_prop_setref_t bundle);
extern void parse_debug_trace (scr_bool flag);
extern void parse_set_game_patches (scr_bool flag);
extern scr_bool parse_get_game_patches (void);
extern const scr_char *parse_get_applied_patch (void);

#endif

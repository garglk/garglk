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
 * Prototypes for scrooms.cpp: room groups, room alternates and room names.
 */

#ifndef SCARIER_ROOMS_H
#define SCARIER_ROOMS_H

#include "scprops.h"

extern scr_int lib_random_roomgroup_member (scr_gameref_t game,
                                            scr_int roomgroup);
extern scr_bool lib_use_room_alt (scr_gameref_t game, scr_int room,
                                  scr_int alt);
extern scr_bool lib_room_alt_darkens (scr_gameref_t game, scr_int room);
extern scr_bool lib_room_is_dark (scr_gameref_t game, scr_int room);
extern scr_int lib_find_starting_alt (scr_gameref_t game, scr_int room);
extern const scr_char *lib_get_room_name (scr_gameref_t game, scr_int room);
extern scr_bool lib_can_go (scr_gameref_t game, scr_int room,
                            scr_int direction);
extern void room_debug_trace (scr_bool flag);

#endif

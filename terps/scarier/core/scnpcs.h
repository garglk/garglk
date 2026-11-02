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
 * Prototypes for scnpcs.cpp: NPCs and their walks.
 */

#ifndef SCARIER_NPCS_H
#define SCARIER_NPCS_H

#include "sctypes.h"

/* NPC gender enumeration and functions. */
enum
{ NPC_MALE = 0,
  NPC_FEMALE = 1,
  NPC_NEUTER = 2
};

extern scr_bool npc_in_room (scr_gameref_t game, scr_int npc, scr_int room);
extern scr_int npc_count_in_room (scr_gameref_t game, scr_int room);
extern scr_int npc_count_referenced_present (scr_gameref_t game, scr_int *npc);
extern void npc_setup_initial (scr_gameref_t game);
extern void npc_start_npc_walk (scr_gameref_t game, scr_int npc, scr_int walk);
extern scr_bool npc_walk_is_enabled (scr_gameref_t game, scr_int npc,
                                    scr_int walk);
extern void npc_tick_npcs (scr_gameref_t game);
extern void npc_turn_update (scr_gameref_t game);
extern void npc_debug_trace (scr_bool flag);

#endif

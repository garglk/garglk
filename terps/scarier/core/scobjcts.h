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
 * Prototypes for scobjcts.cpp: objects, containers and the weight model.
 */

#ifndef SCARIER_OBJCTS_H
#define SCARIER_OBJCTS_H

#include "scprops.h"

/* Object open/closed state enumeration and functions. */
enum
{ OBJ_WONTCLOSE = 0,
  OBJ_OPEN = 5,
  OBJ_CLOSED = 6,
  OBJ_LOCKED = 7
};

extern scr_int obj_initial_location_code (scr_gameref_t game,
                                          scr_int object);
extern void obj_forget_bundle (scr_prop_setref_t bundle);
extern scr_bool obj_is_static (scr_gameref_t game, scr_int object);
extern scr_bool obj_is_container (scr_gameref_t game, scr_int object);
extern scr_bool obj_is_surface (scr_gameref_t game, scr_int object);
extern scr_int obj_container_object (scr_gameref_t game, scr_int n);
extern scr_int obj_surface_object (scr_gameref_t game, scr_int n);
extern scr_bool obj_indirectly_in_room (scr_gameref_t game,
                                       scr_int object, scr_int room);
extern scr_bool obj_indirectly_held_by_player (scr_gameref_t game,
                                               scr_int object);
extern scr_bool obj_directly_in_room (scr_gameref_t game,
                                     scr_int object, scr_int room);
extern scr_int obj_stateful_object (scr_gameref_t game, scr_int n);
extern scr_int obj_dynamic_object (scr_gameref_t game, scr_int n);
extern scr_int obj_wearable_object (scr_gameref_t game, scr_int n);
extern scr_int obj_standable_object (scr_gameref_t game, scr_int n);
extern scr_int obj_get_size (scr_gameref_t game, scr_int object);
extern scr_int obj_get_weight (scr_gameref_t game, scr_int object);
extern scr_int obj_get_base_weight (scr_gameref_t game, scr_int object);
extern scr_int obj_get_player_size_limit (scr_gameref_t game);
extern scr_int obj_get_player_weight_limit (scr_gameref_t game);
extern scr_bool obj_uses_burden_model (scr_gameref_t game);
extern scr_bool obj_uses_running_load (scr_gameref_t game);
extern scr_bool obj_uses_running_weight (scr_gameref_t game);
extern scr_int obj_get_burden (scr_gameref_t game, scr_int object);
extern scr_int obj_get_player_burden_limit (scr_gameref_t game);
extern scr_int obj_get_container_capacity (scr_gameref_t game, scr_int object);
extern scr_int obj_get_container_free_space (scr_gameref_t game,
                                             scr_int object);
extern scr_int obj_lieable_object (scr_gameref_t game, scr_int n);
extern scr_bool obj_appears_plural (scr_gameref_t game, scr_int object);
extern scr_char *obj_state_name (scr_gameref_t game, scr_int object);
extern scr_bool obj_shows_initial_description (scr_gameref_t game,
                                               scr_int object, scr_int room,
                                               scr_bool inroomdesc_absent);
extern void obj_mark_room_objects_seen (scr_gameref_t game, scr_int room);
extern void obj_mark_room_statics_seen (scr_gameref_t game, scr_int room);
extern void obj_mark_npc_parts_seen (scr_gameref_t game);
extern void obj_debug_trace (scr_bool flag);

#endif

/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SCLIBRAR_TAKE_INTERNAL_H
#define SCLIBRAR_TAKE_INTERNAL_H

/*
 * Internal header of the take files: what sclibrar_take.cpp and the file
 * split out of it share with each other and with no other library file
 * (sclibrar_internal.h has what the core files share).  They were one
 * translation unit, so some of this is shared state.
 *
 *   sclibrar_take.cpp           carrying capacity, "count", taking from a
 *                               character, lib_take_backend_common()
 *   sclibrar_take_multiple.cpp  "take all", the absent noun, "take X and
 *                               Y", lib_take_multiple_common()
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/* sclibrar_take.cpp */
extern scr_bool lib_take_over_capacity (scr_gameref_t game, scr_int object,
                                        scr_bool *is_size);
extern void lib_print_take_npc_refusal (scr_gameref_t game, scr_int npc);
extern scr_int lib_take_npc_overwrite (scr_gameref_t game);
extern scr_bool lib_take_from_npc_overwrite_380 (scr_gameref_t game,
                                                 scr_int npc, size_t mark);
extern void lib_take_refusal_redispatch_400 (scr_gameref_t game);
extern void lib_take_backend (scr_gameref_t game);

/*
 * The flags the command forms and the backend pass each other; each is
 * described where sclibrar_take.cpp defines it.
 */
extern scr_bool lib_take_single_named;
extern scr_bool lib_take_refusal_claimed;
extern scr_bool lib_take_and_pieces_400;
extern scr_bool lib_take_refusal_redispatch;

#endif

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

#ifndef SCLIBRAR_DISAMBIG_INTERNAL_H
#define SCLIBRAR_DISAMBIG_INTERNAL_H

/*
 * Internal header of the disambiguation files: what sclibrar_disambig.cpp
 * and the two files split out of it share with each other and with no
 * other library file (sclibrar_internal.h has what the core files share).
 * They were one translation unit, so some of this is shared state.
 *
 *   sclibrar_disambig.cpp       the "which" wording, the NPC pick,
 *                               lib_disambiguate_object_common()
 *   sclibrar_disambig_scan.cpp  the pre-4.0 checkobject scan and prompt
 *   sclibrar_disambig_400.cpp   the 4.0 question and its answer slot
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/* sclibrar_disambig.cpp */
extern scr_bool lib_which_runner_form (scr_gameref_t game);
extern void lib_which_head (scr_gameref_t game, const scr_char *head,
                            const scr_char *tail);

/* sclibrar_disambig_scan.cpp */
extern scr_bool lib_references_are_namesakes (scr_gameref_t game);
extern scr_bool
lib_named_answers_to (scr_gameref_t game, const scr_char *class_,
                      scr_int index_, const scr_char *name_field,
                      const scr_char *term);
extern scr_bool lib_object_held_pre380 (scr_gameref_t game, scr_int object);
extern scr_bool
lib_namesake_crowded_pre380 (scr_gameref_t game, const scr_char *line,
                             scr_int object, scr_bool want_held);
extern scr_bool lib_names_object_370 (scr_gameref_t game, const scr_char *line,
                                      scr_int object);
extern scr_bool
lib_names_only_shadowed_370 (scr_gameref_t game, const scr_char *line,
                             scr_int object);
extern scr_int lib_co_verb_mode (const scr_char *line);
extern scr_bool
lib_runner_co_scan (scr_gameref_t game, const scr_char *command,
                    const scr_char **prompt_term, const scr_char **list_term,
                    scr_int *present_count);

/* sclibrar_disambig_400.cpp */
extern scr_bool lib_co_400_raise_for_references (scr_gameref_t game);
extern scr_bool lib_co_400_raise_for_contained_aliases (scr_gameref_t game);

#endif

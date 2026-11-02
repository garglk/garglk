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

#ifndef SCLIBRAR_PUT_INTERNAL_H
#define SCLIBRAR_PUT_INTERNAL_H

/*
 * Internal header of the put files: what sclibrar_put.cpp and the file
 * split out of it share with each other and with no other library file
 * (sclibrar_internal.h has what the core files share).  They were one
 * translation unit, so some of this is shared state.
 *
 *   sclibrar_put.cpp           the filters, the implicit take, the "in" and
 *                              "on" backends and their validity tests
 *   sclibrar_put_multiple.cpp  "put all", the named and multiple-object
 *                              handlers, the pre-4.0 and 3.9 questions
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * What a put backend has to tell its caller beyond the object moves; the
 * fields are described above lib_output_length() in sclibrar_put.cpp.
 */
typedef struct
{
  scr_bool is_refusal_only;
  scr_bool is_silent;
  scr_bool is_tasks_only;
  scr_bool is_announce_only;
} lib_put_outcome_t;

/*
 * The per-command admission and flags, each described where
 * sclibrar_put.cpp defines it; lib_put_reset() clears them all.
 */
extern scr_int lib_put_present_unseen;
extern scr_bool lib_put_mid_word_390;
extern scr_bool lib_put_refusal_swept;

/*
 * The admission lasts for one command only.  Each put backend's multiple
 * form holds one of these for its whole run, so the object is forgotten
 * however the command ends.
 */
struct lib_put_admission_guard_t
{
  ~lib_put_admission_guard_t ()
  {
    lib_put_present_unseen = -1;
  }
};

/* sclibrar_put.cpp */
extern scr_bool lib_put_named_filter (scr_gameref_t game, scr_int object);
extern lib_put_outcome_t lib_put_in_backend (scr_gameref_t game,
                                             scr_int container,
                                             scr_bool is_all_form);
extern scr_bool lib_put_finish (scr_gameref_t game,
                                const lib_put_outcome_t &outcome);
extern scr_bool lib_put_multiple_filter (scr_gameref_t game, scr_int object,
                                         scr_int unused);
extern scr_bool lib_put_multiple_resolve_filter (scr_gameref_t game,
                                                 scr_int object,
                                                 scr_int unused);
extern scr_bool lib_put_except_filter (scr_gameref_t game, scr_int object,
                                       scr_int target);
extern scr_int lib_put_target_pre390 (scr_gameref_t game,
                                      const scr_char *prompt,
                                      scr_bool *is_ambiguous);
extern scr_bool lib_put_target_takes_on (scr_gameref_t game, scr_int target,
                                         scr_bool typed_on);
extern scr_bool lib_put_invalid_result (void);
extern scr_bool lib_put_in_is_valid (scr_gameref_t game, scr_int container);
extern lib_put_outcome_t lib_put_on_backend (scr_gameref_t game,
                                             scr_int supporter);
extern scr_bool lib_put_on_is_valid (scr_gameref_t game, scr_int supporter);

/* sclibrar_put_multiple.cpp */
extern scr_bool lib_put_refusal_first_390 (scr_gameref_t game);
/* The refusal-first sweep itself, on text printed since from. */
extern void lib_put_sweep_390 (scr_gameref_t game, const scr_char *input,
                               size_t from);

#endif

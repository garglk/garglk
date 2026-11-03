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

#ifndef SCRUN_MATCH_INTERNAL_H
#define SCRUN_MATCH_INTERNAL_H

/*
 * Internal header of the match files: what scrun_match.cpp and the two
 * files split out of it share with each other and with no other runner
 * file (scrunner.h has what the runner files share).  They were one
 * translation unit, so some of this is shared state.
 *
 *   scrun_match.cpp          the pattern cache, reference substitution,
 *                            run_match_task_commands(), the take/put
 *                            class filter
 *   scrun_match_run.cpp      restriction gating, strict and lenient
 *                            matching, run_game_commands_common(),
 *                            run_does_command_match(), task dispatch
 *   scrun_match_refusal.cpp  the repeat assist, the 3.9 spent-task claim,
 *                            run_task_refusal(), the 4.0 put/take rerun,
 *                            the 3.7 second pass
 */

#include "scrunner.h"

/* What run_task_restriction() finds; see there. */
enum run_restriction_t
{
  RUN_UNRESTRICTED, RUN_RESTRICTED_LOUDLY, RUN_RESTRICTED_SILENTLY
};

/* scrun_match.cpp */
extern scr_bool run_is_task_function (const scr_char *pattern,
                                      scr_gameref_t game);
extern void run_note_task_ran (scr_gameref_t game, scr_int task);
extern scr_bool run_task_ran_this_command (scr_int task);
extern scr_bool run_match_task_commands (scr_gameref_t game,
                                         scr_int task, const scr_char *string,
                                         scr_bool forwards,
                                         scr_bool is_library);
extern scr_bool run_task_passes_class_filter (scr_gameref_t game,
                                              scr_int task);

/*
 * The flags and the counter the matcher and its callers pass each other;
 * each is described where scrun_match.cpp defines it.
 */
extern scr_bool run_named_verb_only;
extern scr_bool run_passing_only;
extern scr_int run_task_runs_noted;

/* scrun_match_run.cpp */
extern run_restriction_t run_task_restriction (scr_gameref_t game,
                                               scr_int task);

#endif

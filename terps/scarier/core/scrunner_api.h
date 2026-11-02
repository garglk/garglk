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
 * Prototypes for runner/: the game runner's entry points and the hooks
 * the library and the engine call.  The runner's own umbrella is
 * runner/scrunner.h.
 */

#ifndef SCARIER_RUNNER_API_H
#define SCARIER_RUNNER_API_H

#include <string>
#include "sctypes.h"

/* Game runner functions. */
extern scr_bool run_game_task_commands (scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_typed_line_task_commands (scr_gameref_t game,
                                              const scr_char *string,
                                              scr_bool loud);
extern scr_bool run_task_run_by_index (scr_gameref_t game, scr_int task);
extern void run_npc_walk_task (scr_gameref_t game, scr_int walktask);
extern void run_event_task (scr_gameref_t game, scr_int eventtask);
extern scr_bool run_does_command_match (scr_gameref_t game,
                                        const scr_char *string,
                                        scr_bool check_restrictions = FALSE,
                                        scr_int *match_kind = nullptr);
extern void run_set_task_class_filter (scr_int mode);
extern scr_bool run_lenient_task_matching (void);
extern scr_bool run_passing_task_commands (scr_gameref_t game,
                                           const scr_char *string);
extern scr_bool run_in_priority_pass (void);
extern scr_bool run_in_put_clause_loop (void);
extern const scr_char *run_get_dispatch_input (void);
extern const scr_char *run_get_line_input (void);
extern scr_bool run_any_task_ran_this_command (void);
extern void run_queue_goto_step (const scr_char *step);
extern void run_set_goto_arrival (const scr_char *text);
extern scr_int run_c_word_pre400 (scr_int version, const scr_char *line,
                                  const scr_char *word);
extern scr_bool run_c_word (scr_int version, const scr_char *line,
                            const scr_char *word);
extern scr_bool run_priority_defer_if_active (void);
extern void run_priority_refuse (void);
extern void run_priority_unnamed_put_object (void);
extern scr_bool run_priority_put_was_unnamed (void);
extern std::string run_normalise_put_line (const scr_char *string);
extern scr_bool run_unnamed_put_fragment (const scr_char *string,
                                          std::string &fragment);
extern scr_gameref_t run_create (scr_read_callbackref_t callback, void *opaque);
extern void run_interpret (scr_gameref_t game);
extern void run_destroy (scr_gameref_t game);
extern void run_restart (scr_gameref_t game);
extern void run_save (scr_gameref_t game,
                      scr_write_callbackref_t callback, void *opaque);
extern void run_save_to_file (scr_gameref_t game,
                              scr_write_callbackref_t callback, void *opaque);
extern scr_bool run_save_prompted (scr_gameref_t game);
extern scr_bool run_restore (scr_gameref_t game,
                            scr_read_callbackref_t callback, void *opaque);
extern scr_bool run_restore_prompted (scr_gameref_t game);
extern scr_bool run_undo (scr_gameref_t game);
extern const std::string &run_get_undo_text (void);
extern void run_quit (scr_gameref_t game);
extern scr_bool run_is_running (scr_gameref_t game);
extern scr_int run_get_restart_count (void);
extern scr_bool run_has_completed (scr_gameref_t game);
extern void run_set_end_keyprompt (scr_bool flag);
extern scr_bool run_get_end_keyprompt (void);
extern void run_set_repeat_assist (scr_bool flag);
extern scr_bool run_get_repeat_assist (void);
extern scr_bool run_is_undo_available (scr_gameref_t game);
extern void run_get_attributes (scr_gameref_t game,
                                const scr_char **game_name,
                                const scr_char **game_author,
                                const scr_char **game_compile_date,
                                scr_int *turns, scr_int *score,
                                scr_int *max_score,
                                const scr_char **current_room_name,
                                const scr_char **status_line,
                                const scr_char **preferred_font,
                                scr_bool *bold_room_names, scr_bool *verbose,
                                scr_bool *notify_score_change);
extern scr_int run_startup_prompt (void);
extern std::string run_session_state (scr_gameref_t game);
extern scr_bool run_restore_session_state (scr_gameref_t game,
                                           const std::string &state);
extern void run_set_attributes (scr_gameref_t game,
                                scr_bool bold_room_names, scr_bool verbose,
                                scr_bool notify_score_change);
extern scr_hintref_t run_hint_iterate (scr_gameref_t game, scr_hintref_t hint);
extern const scr_char *run_get_hint_question (scr_gameref_t game,
                                             scr_hintref_t hint);
extern const scr_char *run_get_subtle_hint (scr_gameref_t game,
                                           scr_hintref_t hint);
extern const scr_char *run_get_unsubtle_hint (scr_gameref_t game,
                                             scr_hintref_t hint);

/* Runner hooks the library calls. */
extern scr_bool
run_line_matches_task_strictly (scr_gameref_t game, const scr_char *string,
                                const scr_char *except = nullptr,
                                scr_bool anywhere = FALSE);
extern scr_bool run_line_yields_to_library (scr_gameref_t game,
                                            const scr_char *string);
extern void run_forget_game (const void *game);

#endif

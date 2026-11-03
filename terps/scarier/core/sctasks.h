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
 * Prototypes for sctasks*.cpp: tasks, their actions and the end game.
 */

#ifndef SCARIER_TASKS_H
#define SCARIER_TASKS_H

#include "scvars.h"

/* Task functions. */
extern scr_bool task_has_hints (scr_gameref_t game, scr_int task);
extern const scr_char *task_get_hint_question (scr_gameref_t game,
                                               scr_int task);
extern const scr_char *task_get_hint_subtle (scr_gameref_t game, scr_int task);
extern const scr_char *task_get_hint_unsubtle (scr_gameref_t game,
                                               scr_int task);
extern scr_bool task_can_run_task_directional (scr_gameref_t game,
                                              scr_int task, scr_bool forwards);
extern scr_bool task_can_run_task (scr_gameref_t game, scr_int task);
extern scr_bool task_where_allows_run (scr_gameref_t game, scr_int task);
extern scr_bool task_is_room_refused (scr_gameref_t game,
                                      scr_int task, scr_bool forwards);
extern scr_bool task_is_done_refused (scr_gameref_t game, scr_int task);
extern scr_bool task_is_reverse_refused_390 (scr_gameref_t game, scr_int task);
extern scr_bool task_run_task (scr_gameref_t game, scr_int task,
                               scr_bool forwards);
extern scr_int task_execution_count (void);
extern scr_int task_selector_npc (scr_var_setref_t vars, scr_int var);
extern void task_push_dispatched_run (void);
extern void task_pop_dispatched_run (void);
extern void task_print_end_game_message (scr_gameref_t game);
extern void task_print_end_game_summary (scr_gameref_t game, scr_bool is_win,
                                         scr_bool is_death);
extern void task_print_end_keyprompt (scr_gameref_t game);
extern void task_debug_trace (scr_bool flag);
extern void task_set_move_assist (scr_bool flag);
extern scr_bool task_get_move_assist (void);
extern void task_set_room_assist (scr_bool flag);
extern scr_bool task_get_room_assist (void);
extern void task_forget_game (const void *game);

#endif

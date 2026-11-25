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
 * The hooks: the only calls core/ makes upward, into library/ and runner/.
 *
 * The engine is the bottom layer and the command library and the runner sit
 * on it, but three things in it genuinely need the layers above: events,
 * battle and NPC walks run tasks; task actions and the end of the game
 * print rooms and stock messages; and the debugger and the game state tell
 * the runner to stop or to forget a game.  Those calls are declared here
 * and nowhere else in core/, and `make -f Makefile.headless check-layers`
 * fails on any other lib_ or run_ symbol a core/ object leaves undefined.
 * Think twice before adding one: a query that prints nothing belongs in
 * core/ itself (scrooms.cpp, scobjcts.cpp, scgamest.cpp).
 *
 * The rest of the two layers' prototypes are library/sclibrar_api.h and
 * runner/scrunner_api.h.
 */

#ifndef SCARIER_HOOKS_H
#define SCARIER_HOOKS_H

#include "sctypes.h"

/* Into library/: printing rooms, names and stock messages. */
extern void lib_print_room_name (scr_gameref_t game, scr_int room);
extern scr_bool lib_room_name_headings (scr_gameref_t game);
extern void lib_print_room_description (scr_gameref_t game, scr_int room);
extern void lib_print_room_exits (scr_gameref_t game, scr_int room);
extern void lib_print_object_np (scr_gameref_t game, scr_int object);
/* An ALR could be matching the Runner's literal " is " in a listing. */
extern scr_bool lib_alrs_see_list_verb (scr_gameref_t game,
                                        scr_int container);
/* "I'm afraid you are dead!", or its first-person form for a pre-4.0 game. */
extern const scr_char *lib_get_death_message (scr_gameref_t game);

/* Into runner/: running tasks, and the life of the game. */
extern scr_bool run_task_run_by_index (scr_gameref_t game, scr_int task);
extern void run_npc_walk_task (scr_gameref_t game, scr_int walktask);
extern void run_event_task (scr_gameref_t game, scr_int eventtask);
extern void run_quit (scr_gameref_t game);
extern scr_bool run_is_running (scr_gameref_t game);
extern scr_bool run_get_end_keyprompt (void);
extern void run_forget_game (const void *game);

#endif

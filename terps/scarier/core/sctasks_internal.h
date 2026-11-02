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

#ifndef SCTASKS_INTERNAL_H
#define SCTASKS_INTERNAL_H

/*
 * Internal header of the task module: what sctasks.cpp and the four files
 * split out of it share.
 *
 *   sctasks.cpp          the dispatched-run bracket, assists, hints, the
 *                        per-task property cache, task_can_run_task()
 *   sctasks_move.cpp     the move object and move character actions
 *   sctasks_action.cpp   the other actions and the action dispatch
 *   sctasks_endgame.cpp  the end game action and its score summary
 *   sctasks_run.cpp      task_run_task_unrestricted(), task_run_task()
 */

/*
 * Module notes:
 *
 * o Implements task return FALSE on no output, a slight extension of
 *   current jAsea behavior.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "sctafpar.h"
#include "scvars.h"
#include "scexpr.h"
#include "scprintf.h"
#include "scgamest_api.h"
#include "sclibrar_api.h"
#include "scresour.h"
#include "scrunner_api.h"
#include "scevents.h"
#include "sctasks.h"
#include "screstrs.h"
#include "scnpcs.h"
#include "scbattle.h"
#include "scobjcts.h"
#include "scdump.h"
#include "scgamest.h"

/* One task's cached runnability properties; see the cache note in
 * sctasks.cpp, which owns the TASK_CACHE_* values. */
struct scr_task_props_s
{
  scr_byte repeatable;           /* tri-state, TASK_CACHE_* */
  scr_byte repeattext_empty;     /* tri-state: is RepeatText empty? */
  scr_byte reversible;           /* tri-state */
  scr_int where_type;            /* Where.Type + 1, 0 = unknown */
  scr_int where_room;            /* Where.Room, TASK_CACHE_NO_ROOM = unknown */
  std::vector<scr_byte> where_rooms;  /* per-room tri-state, sized lazily */
};
typedef struct scr_task_props_s scr_task_props_t;

/*
 * The task module's state: every file-scope variable of the five files, in
 * one instance, `tasks`, defined in sctasks.cpp.  The members are grouped by
 * lifetime:
 *
 *   trace    the interface's task trace flag (task_debug_trace()), set
 *            before any game exists and kept across them.
 *   options  the move and room assists, set by the interface through
 *            task_set_move_assist() / task_set_room_assist() and read by
 *            the move actions and task_can_run_task_directional().  They
 *            belong to the process, not to a game: whoever loads the next
 *            game sets them again.
 *   run      the dispatched-run nesting count, which the
 *            task_push_dispatched_run() / task_pop_dispatched_run() bracket
 *            returns to zero, and the count of tasks that have run, which
 *            only ever grows -- task_execution_count()'s callers compare
 *            two readings, so it needs no reset.
 *   cache    the per-task runnability properties of one game, filled
 *            lazily and dropped by task_forget_game() when that game is
 *            destroyed.
 *
 * Nothing here is saved, and there is no reset function: the cache has its
 * drop hook and the rest is process state.
 */
struct scr_task_state_s
{
  scr_bool trace = FALSE;

  struct
  {
    scr_bool move_assist = FALSE;
    scr_bool room_assist = FALSE;
  } options;

  struct
  {
    scr_int dispatch_depth = 0;
    scr_int executions = 0;
  } run;

  struct
  {
    const void *game = nullptr;
    std::vector<scr_task_props_t> props;
  } cache;
};
typedef struct scr_task_state_s scr_task_state_t;

extern scr_task_state_t tasks;

/* sctasks.cpp */
extern scr_bool task_in_dispatched_run (void);
extern scr_bool task_is_repeatable (scr_gameref_t game, scr_int task);
extern scr_bool task_repeattext_is_empty (scr_gameref_t game, scr_int task);

/* sctasks_move.cpp */
extern void task_run_move_object_action (scr_gameref_t game, scr_int var1,
                                         scr_int var2, scr_int var3);
extern void task_run_move_npc_action (scr_gameref_t game, scr_int var1,
                                      scr_int var2, scr_int var3);

/* sctasks_endgame.cpp */
extern scr_bool task_run_end_game_action (scr_gameref_t game, scr_int var1);

/* sctasks_action.cpp */
extern scr_bool task_run_task_actions (scr_gameref_t game, scr_int task);

#endif

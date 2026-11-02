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
 * Prototypes for scgamest.cpp: the game state accessors and mutators,
 * and the GS_FOR_EACH_* index loops.  The state layout itself is
 * scgamest.h, for the modules that need it.
 */

#ifndef SCARIER_GAMESTATE_API_H
#define SCARIER_GAMESTATE_API_H

#include "scmemos.h"
#include "scprintf.h"
#include "scprops.h"
#include "scvars.h"

/* Game state functions. */
extern scr_gameref_t gs_create (scr_var_setref_t vars, scr_prop_setref_t bundle,
                               scr_filterref_t filter);
extern scr_bool gs_is_game_valid (scr_gameref_t game);
extern scr_bool gs_room_valid (scr_gameref_t gs, scr_int room);
extern scr_bool gs_task_valid (scr_gameref_t gs, scr_int task);
extern scr_bool gs_object_valid (scr_gameref_t gs, scr_int object);
extern scr_bool gs_npc_valid (scr_gameref_t gs, scr_int npc);
extern void gs_copy (scr_gameref_t to, scr_gameref_t from);
extern void gs_destroy (scr_gameref_t game);

/* Game state accessors and mutators. */
extern void gs_move_player_to_room (scr_gameref_t game, scr_int room);
extern scr_bool gs_player_in_room (scr_gameref_t game, scr_int room);
extern scr_var_setref_t gs_get_vars (scr_gameref_t gs);
extern scr_prop_setref_t gs_get_bundle (scr_gameref_t gs);
extern scr_filterref_t gs_get_filter (scr_gameref_t gs);
extern scr_memo_setref_t gs_get_memento (scr_gameref_t gs);
extern void gs_set_playerroom (scr_gameref_t gs, scr_int room);
extern void gs_set_playerposition (scr_gameref_t gs, scr_int position);
extern void gs_set_playerparent (scr_gameref_t gs, scr_int parent);
extern scr_int gs_playerroom (scr_gameref_t gs);
extern scr_int gs_playerposition (scr_gameref_t gs);
extern scr_int gs_playerparent (scr_gameref_t gs);
extern scr_int gs_carried_weight (scr_gameref_t gs);
extern scr_int gs_carried_size (scr_gameref_t gs);
extern void gs_set_carried_suspend (scr_gameref_t gs, scr_bool flag);

/*
 * Suspend the carried-load tracker for the enclosing scope, lifting the
 * suspension on every exit from it -- fall-through, early return, or a
 * scr_fatal throw; see task_move_object() and evt_move_object().
 */
struct gs_carried_suspend_guard
{
  explicit gs_carried_suspend_guard (scr_gameref_t gs) : gs_ (gs)
  { gs_set_carried_suspend (gs_, TRUE); }
  ~gs_carried_suspend_guard () { gs_set_carried_suspend (gs_, FALSE); }
private:
  scr_gameref_t gs_;
  gs_carried_suspend_guard (const gs_carried_suspend_guard &);
  gs_carried_suspend_guard &operator= (const gs_carried_suspend_guard &);
};
extern scr_bool gs_runner_phantom_held (scr_gameref_t gs);
extern void gs_set_runner_phantom_held (scr_gameref_t gs, scr_bool flag);
extern void gs_carried_adjust (scr_gameref_t gs, scr_int weight, scr_int size);
extern scr_bool gs_runner_possessed (scr_gameref_t gs, scr_int object);
extern void gs_carried_recompute (scr_gameref_t gs);
extern void gs_set_capacity_assist (scr_bool flag);
extern scr_bool gs_get_capacity_assist (void);
/*
 * Index loops over a game's entities, one per kind:
 *
 *   GS_FOR_EACH_OBJECT (game, object)
 *     ...
 *
 * is for (object = 0; object < gs_object_count (game); object++).  The index
 * is a variable the caller declares, as in the loops these replaced, so a
 * search loop can still break out and read it afterwards.
 */
#define GS_FOR_EACH_EVENT(game, event) \
  for ((event) = 0; (event) < gs_event_count (game); (event)++)
#define GS_FOR_EACH_ROOM(game, room) \
  for ((room) = 0; (room) < gs_room_count (game); (room)++)
#define GS_FOR_EACH_TASK(game, task) \
  for ((task) = 0; (task) < gs_task_count (game); (task)++)
#define GS_FOR_EACH_OBJECT(game, object) \
  for ((object) = 0; (object) < gs_object_count (game); (object)++)
#define GS_FOR_EACH_NPC(game, npc) \
  for ((npc) = 0; (npc) < gs_npc_count (game); (npc)++)

extern scr_int gs_event_count (scr_gameref_t gs);
extern void gs_set_event_state (scr_gameref_t gs, scr_int event, scr_int state);
extern void gs_set_event_time (scr_gameref_t gs, scr_int event, scr_int etime);
extern scr_int gs_event_state (scr_gameref_t gs, scr_int event);
extern scr_int gs_event_time (scr_gameref_t gs, scr_int event);
extern void gs_set_event_loadtime (scr_gameref_t gs, scr_int event,
                                   scr_int etime);
extern scr_int gs_event_loadtime (scr_gameref_t gs, scr_int event);
extern void gs_set_event_taskstate (scr_gameref_t gs, scr_int event,
                                    scr_bool done);
extern scr_bool gs_event_taskstate (scr_gameref_t gs, scr_int event);
extern void gs_set_event_ticked (scr_gameref_t gs, scr_int event,
                                 scr_bool ticked);
extern scr_bool gs_event_ticked (scr_gameref_t gs, scr_int event);
extern void gs_decrement_event_time (scr_gameref_t gs, scr_int event);
extern scr_int gs_room_count (scr_gameref_t gs);
extern void gs_set_room_seen (scr_gameref_t gs, scr_int room, scr_bool seen);
extern scr_bool gs_room_seen (scr_gameref_t gs, scr_int room);
extern scr_int gs_task_count (scr_gameref_t gs);
extern void gs_set_task_done (scr_gameref_t gs, scr_int task, scr_bool done);
extern void gs_set_task_scored (scr_gameref_t gs, scr_int task,
                                scr_bool scored);
extern scr_bool gs_task_done (scr_gameref_t gs, scr_int task);
extern scr_bool gs_task_scored (scr_gameref_t gs, scr_int task);
extern scr_int gs_object_count (scr_gameref_t gs);
extern void gs_set_object_openness (scr_gameref_t gs,
                                    scr_int object, scr_int openness);
extern void gs_set_object_state (scr_gameref_t gs, scr_int object,
                                 scr_int state);
extern void gs_set_object_seen (scr_gameref_t gs, scr_int object,
                                scr_bool seen);
extern void gs_set_object_unmoved (scr_gameref_t gs,
                                   scr_int object, scr_bool unmoved);
extern void gs_set_object_static_unmoved (scr_gameref_t gs,
                                          scr_int object, scr_bool unmoved);
extern scr_int gs_object_openness (scr_gameref_t gs, scr_int object);
extern scr_int gs_object_state (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_seen (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_unmoved (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_static_unmoved (scr_gameref_t gs, scr_int object);
extern void gs_object_static_rooms_clear (scr_gameref_t gs, scr_int object);
extern void gs_object_static_rooms_add (scr_gameref_t gs,
                                        scr_int object, scr_int room);
extern scr_bool gs_object_static_rooms_empty (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_in_static_rooms (scr_gameref_t gs,
                                           scr_int object, scr_int room);
extern scr_int gs_object_position (scr_gameref_t gs, scr_int object);
extern scr_int gs_object_parent (scr_gameref_t gs, scr_int object);
extern scr_int gs_object_runner_parent (scr_gameref_t gs, scr_int object);
extern void gs_set_object_runner_parent (scr_gameref_t gs, scr_int object,
                                         scr_int runner_parent);
extern void gs_object_move_onto (scr_gameref_t gs, scr_int object,
                                 scr_int onto);
extern void gs_object_move_into (scr_gameref_t gs, scr_int object,
                                 scr_int into);
extern void gs_object_make_hidden (scr_gameref_t gs, scr_int object);
extern void gs_object_player_get (scr_gameref_t gs, scr_int object);
extern void gs_object_npc_get (scr_gameref_t gs, scr_int object, scr_int npc);
extern void gs_object_player_wear (scr_gameref_t gs, scr_int object);
extern void gs_object_npc_wear (scr_gameref_t gs, scr_int object, scr_int npc);
extern void gs_object_to_room (scr_gameref_t gs, scr_int object, scr_int room);
extern scr_int gs_npc_count (scr_gameref_t gs);
extern void gs_set_npc_location (scr_gameref_t gs, scr_int npc,
                                 scr_int location);
extern scr_int gs_npc_location (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_walk_hidden (scr_gameref_t gs, scr_int npc,
                                    scr_bool hidden);
extern scr_bool gs_npc_walk_hidden (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_dead (scr_gameref_t gs, scr_int npc, scr_bool dead);
extern scr_bool gs_npc_dead (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_position (scr_gameref_t gs, scr_int npc,
                                 scr_int position);
extern scr_int gs_npc_position (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_parent (scr_gameref_t gs, scr_int npc, scr_int parent);
extern scr_int gs_npc_parent (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_seen (scr_gameref_t gs, scr_int npc, scr_bool seen);
extern scr_bool gs_npc_seen (scr_gameref_t gs, scr_int npc);
extern void gs_set_playerstamina (scr_gameref_t gs, scr_int stamina);
extern scr_int gs_playerstamina (scr_gameref_t gs);
extern void gs_set_playerstaminacounter (scr_gameref_t gs, scr_int counter);
extern scr_int gs_playerstaminacounter (scr_gameref_t gs);
extern void gs_set_playerwield (scr_gameref_t gs, scr_int object);
extern scr_int gs_playerwield (scr_gameref_t gs);
extern void gs_set_npc_stamina (scr_gameref_t gs, scr_int npc, scr_int stamina);
extern scr_int gs_npc_stamina (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_staminacounter (scr_gameref_t gs, scr_int npc,
                                       scr_int counter);
extern scr_int gs_npc_staminacounter (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_attackcounter (scr_gameref_t gs, scr_int npc,
                                      scr_int counter);
extern scr_int gs_npc_attackcounter (scr_gameref_t gs, scr_int npc);
extern struct scr_battle_s *gs_player_battle (scr_gameref_t gs);
extern struct scr_battle_s *gs_npc_battle (scr_gameref_t gs, scr_int npc);
extern scr_int gs_npc_walkstep_count (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_walkstep (scr_gameref_t gs, scr_int npc,
                                 scr_int walk, scr_int walkstep);
extern scr_int gs_npc_walkstep (scr_gameref_t gs, scr_int npc, scr_int walk);
extern void gs_decrement_npc_walkstep (scr_gameref_t gs,
                                       scr_int npc, scr_int walkstep);
extern void gs_clear_npc_references (scr_gameref_t gs);
extern void gs_clear_object_references (scr_gameref_t gs);
extern void gs_set_multiple_references (scr_gameref_t gs);
extern void gs_clear_multiple_references (scr_gameref_t gs);

#endif

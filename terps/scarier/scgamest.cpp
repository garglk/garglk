/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"

/*
 * gs_carried_recompute()
 *
 * Recompute the player's carried weight and size totals from the objects
 * currently held or worn.  This is the "healed" load -- it does not reproduce
 * the Runner's take/drop double-count -- and matches what the real Runner does
 * when it loads game state.  Used to seed the running totals at game create,
 * copy, and restore, after which gs_carried_track() maintains them.
 */
static void
gs_carried_recompute (scr_gameref_t gs)
{
  scr_int index_, weight = 0, size = 0;

  for (index_ = 0; index_ < gs->object_count; index_++)
    {
      if (gs->objects[index_].position == OBJ_HELD_PLAYER
          || gs->objects[index_].position == OBJ_WORN_PLAYER)
        {
          weight += obj_get_weight (gs, index_);
          size += obj_get_size (gs, index_);
        }
    }
  gs->carried_weight = weight;
  gs->carried_size = size;
  gs->carried_ready = TRUE;
}

/*
 * gs_carried_track()
 *
 * Maintain the carried weight/size running totals as objects move in and out
 * of the player's hands, mirroring the Runner's incremental accounting: an
 * object's weight (including its container contents) is added when it becomes
 * held and subtracted when it leaves.  Because a container's contents are
 * counted both inside the container and again when taken out individually, the
 * running total can exceed the true held weight -- exactly the Runner quirk
 * that blocks an over-encumbered take.  No-op until the totals are seeded
 * (carried_ready), so it ignores object placement during game setup.
 */
static void
gs_carried_track (scr_gameref_t gs, scr_int object, scr_int old_pos, scr_int new_pos)
{
  if (!gs->carried_ready || old_pos == new_pos)
    return;

  if (new_pos == OBJ_HELD_PLAYER)
    {
      gs->carried_weight += obj_get_weight (gs, object);
      gs->carried_size += obj_get_size (gs, object);
    }
  else if (old_pos == OBJ_HELD_PLAYER)
    {
      gs->carried_weight -= obj_get_weight (gs, object);
      gs->carried_size -= obj_get_size (gs, object);
    }
}


/* Assorted definitions and constants. */
static const scr_uint GAME_MAGIC = 0x35aed26e;


/*
 * gs_move_player_to_room()
 * gs_player_in_room()
 *
 * Move the player to a given room, and check presence in a given room.
 */
void
gs_move_player_to_room (scr_gameref_t game, scr_int room)
{
  assert (gs_is_game_valid (game));

  if (room < 0)
    {
      scr_fatal ("gs_move_player_to_room: invalid room, %ld\n", room);
      return;
    }
  else if (room < game->room_count)
    game->playerroom = room;
  else
    {
      scr_int dest = lib_random_roomgroup_member (game,
                                                 room - game->room_count);
      if (dest < 0)
        return;                  /* Empty group: leave the player in place. */
      game->playerroom = dest;
    }

  game->playerparent = -1;
  game->playerposition = 0;
}

scr_bool
gs_player_in_room (scr_gameref_t game, scr_int room)
{
  assert (gs_is_game_valid (game));
  return game->playerroom == room;
}


/*
 * gs_in_range()
 *
 * Helper for event, room, object, and npc range assertions.
 */
static scr_bool
gs_in_range (scr_int value, scr_int limit)
{
  return value >= 0 && value < limit;
}


/*
 * gs_*()
 *
 * Game accessors and mutators.
 */
scr_var_setref_t
gs_get_vars (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->vars;
}

scr_prop_setref_t
gs_get_bundle (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->bundle;
}

scr_filterref_t
gs_get_filter (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->filter;
}

scr_memo_setref_t
gs_get_memento (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->memento;
}


/*
 * Game accessors and mutators for the player.
 */
void
gs_set_playerroom (scr_gameref_t gs, scr_int room)
{
  assert (gs_is_game_valid (gs));
  gs->playerroom = room;
}

void
gs_set_playerposition (scr_gameref_t gs, scr_int position)
{
  assert (gs_is_game_valid (gs));
  gs->playerposition = position;
}

void
gs_set_playerparent (scr_gameref_t gs, scr_int parent)
{
  assert (gs_is_game_valid (gs));
  gs->playerparent = parent;
}

scr_int
gs_playerroom (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->playerroom;
}

scr_int
gs_playerposition (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->playerposition;
}

scr_int
gs_playerparent (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->playerparent;
}

scr_int
gs_carried_weight (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->carried_weight;
}

scr_int
gs_carried_size (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->carried_size;
}


/*
 * Game accessors and mutators for events.
 */
scr_int
gs_event_count (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->event_count;
}

void
gs_set_event_state (scr_gameref_t gs, scr_int event, scr_int state)
{
  assert (gs_is_game_valid (gs) && gs_in_range (event, gs->event_count));
  gs->events[event].state = state;
}

void
gs_set_event_time (scr_gameref_t gs, scr_int event, scr_int etime)
{
  assert (gs_is_game_valid (gs) && gs_in_range (event, gs->event_count));
  gs->events[event].time = etime;
}

scr_int
gs_event_state (scr_gameref_t gs, scr_int event)
{
  assert (gs_is_game_valid (gs) && gs_in_range (event, gs->event_count));
  return gs->events[event].state;
}

scr_int
gs_event_time (scr_gameref_t gs, scr_int event)
{
  assert (gs_is_game_valid (gs) && gs_in_range (event, gs->event_count));
  return gs->events[event].time;
}

void
gs_decrement_event_time (scr_gameref_t gs, scr_int event)
{
  assert (gs_is_game_valid (gs) && gs_in_range (event, gs->event_count));
  gs->events[event].time--;
}


/*
 * Game accessors and mutators for rooms.
 */
scr_int
gs_room_count (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->room_count;
}

void
gs_set_room_seen (scr_gameref_t gs, scr_int room, scr_bool seen)
{
  assert (gs_is_game_valid (gs) && gs_in_range (room, gs->room_count));
  gs->rooms[room].visited = seen;
}

scr_bool
gs_room_seen (scr_gameref_t gs, scr_int room)
{
  assert (gs_is_game_valid (gs) && gs_in_range (room, gs->room_count));
  return gs->rooms[room].visited;
}


/*
 * Game accessors and mutators for tasks.
 */
scr_int
gs_task_count (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->task_count;
}

void
gs_set_task_done (scr_gameref_t gs, scr_int task, scr_bool done)
{
  assert (gs_is_game_valid (gs) && gs_in_range (task, gs->task_count));
  gs->tasks[task].done = done;
}

void
gs_set_task_scored (scr_gameref_t gs, scr_int task, scr_bool scored)
{
  assert (gs_is_game_valid (gs) && gs_in_range (task, gs->task_count));
  gs->tasks[task].scored = scored;
}

scr_bool
gs_task_done (scr_gameref_t gs, scr_int task)
{
  assert (gs_is_game_valid (gs) && gs_in_range (task, gs->task_count));
  return gs->tasks[task].done;
}

scr_bool
gs_task_scored (scr_gameref_t gs, scr_int task)
{
  assert (gs_is_game_valid (gs) && gs_in_range (task, gs->task_count));
  return gs->tasks[task].scored;
}


/*
 * Game accessors and mutators for objects.
 */
scr_int
gs_object_count (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->object_count;
}

void
gs_set_object_openness (scr_gameref_t gs, scr_int object, scr_int openness)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].openness = openness;
}

void
gs_set_object_state (scr_gameref_t gs, scr_int object, scr_int state)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].state = state;
}

void
gs_set_object_seen (scr_gameref_t gs, scr_int object, scr_bool seen)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].seen = seen;
}

void
gs_set_object_unmoved (scr_gameref_t gs, scr_int object, scr_bool unmoved)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].unmoved = unmoved;
}

void
gs_set_object_static_unmoved (scr_gameref_t gs, scr_int object, scr_bool unmoved)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].static_unmoved = unmoved;
}

scr_int
gs_object_openness (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].openness;
}

scr_int
gs_object_state (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].state;
}

scr_bool
gs_object_seen (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].seen;
}

scr_bool
gs_object_unmoved (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].unmoved;
}

scr_bool
gs_object_static_unmoved (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].static_unmoved;
}

scr_int
gs_object_position (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].position;
}

scr_int
gs_object_parent (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  return gs->objects[object].parent;
}

static void
gs_object_move_onto_unchecked (scr_gameref_t gs, scr_int object, scr_int onto)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_ON_OBJECT;
  gs->objects[object].parent = onto;
  gs_carried_track (gs, object, old_pos, OBJ_ON_OBJECT);
}

static void
gs_object_move_into_unchecked (scr_gameref_t gs, scr_int object, scr_int into)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_IN_OBJECT;
  gs->objects[object].parent = into;
  gs_carried_track (gs, object, old_pos, OBJ_IN_OBJECT);
}

static void
gs_object_make_hidden_unchecked (scr_gameref_t gs, scr_int object)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_HIDDEN;
  gs->objects[object].parent = -1;
  gs_carried_track (gs, object, old_pos, OBJ_HIDDEN);
}

static void
gs_object_player_get_unchecked (scr_gameref_t gs, scr_int object)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_HELD_PLAYER;
  gs->objects[object].parent = -1;
  gs_carried_track (gs, object, old_pos, OBJ_HELD_PLAYER);
}

static void
gs_object_npc_get_unchecked (scr_gameref_t gs, scr_int object, scr_int npc)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_HELD_NPC;
  gs->objects[object].parent = npc;
  gs_carried_track (gs, object, old_pos, OBJ_HELD_NPC);
}

static void
gs_object_player_wear_unchecked (scr_gameref_t gs, scr_int object)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_WORN_PLAYER;
  gs->objects[object].parent = 0;
  gs_carried_track (gs, object, old_pos, OBJ_WORN_PLAYER);
}

static void
gs_object_npc_wear_unchecked (scr_gameref_t gs, scr_int object, scr_int npc)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = OBJ_WORN_NPC;
  gs->objects[object].parent = npc;
  gs_carried_track (gs, object, old_pos, OBJ_WORN_NPC);
}

static void
gs_object_to_room_unchecked (scr_gameref_t gs, scr_int object, scr_int room)
{
  scr_int old_pos = gs->objects[object].position;
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  gs->objects[object].position = room + 1;
  gs->objects[object].parent = -1;
  gs_carried_track (gs, object, old_pos, room + 1);
}

void
gs_object_move_onto (scr_gameref_t gs, scr_int object, scr_int onto)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_ON_OBJECT
      || gs->objects[object].parent != onto)
    {
      gs_object_move_onto_unchecked (gs, object, onto);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_move_into (scr_gameref_t gs, scr_int object, scr_int into)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_IN_OBJECT
      || gs->objects[object].parent != into)
    {
      gs_object_move_into_unchecked (gs, object, into);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_make_hidden (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_HIDDEN)
    {
      gs_object_make_hidden_unchecked (gs, object);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_player_get (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_HELD_PLAYER)
    {
      gs_object_player_get_unchecked (gs, object);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_npc_get (scr_gameref_t gs, scr_int object, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_HELD_NPC
      || gs->objects[object].parent != npc)
    {
      gs_object_npc_get_unchecked (gs, object, npc);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_player_wear (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_WORN_PLAYER)
    {
      gs_object_player_wear_unchecked (gs, object);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_npc_wear (scr_gameref_t gs, scr_int object, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != OBJ_WORN_NPC
      || gs->objects[object].parent != npc)
    {
      gs_object_npc_wear_unchecked (gs, object, npc);
      gs->objects[object].unmoved = FALSE;
    }
}

void
gs_object_to_room (scr_gameref_t gs, scr_int object, scr_int room)
{
  assert (gs_is_game_valid (gs) && gs_in_range (object, gs->object_count));
  if (gs->objects[object].position != room + 1)
    {
      gs_object_to_room_unchecked (gs, object, room);
      gs->objects[object].unmoved = FALSE;
    }
}


/*
 * Game accessors and mutators for NPCs.
 */
scr_int
gs_npc_count (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->npc_count;
}

void
gs_set_npc_location (scr_gameref_t gs, scr_int npc, scr_int location)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].location = location;
}

scr_int
gs_npc_location (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].location;
}

void
gs_set_npc_position (scr_gameref_t gs, scr_int npc, scr_int position)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].position = position;
}

scr_int
gs_npc_position (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].position;
}

void
gs_set_npc_parent (scr_gameref_t gs, scr_int npc, scr_int parent)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].parent = parent;
}

scr_int
gs_npc_parent (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].parent;
}

void
gs_set_npc_seen (scr_gameref_t gs, scr_int npc, scr_bool seen)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].seen = seen;
}

scr_bool
gs_npc_seen (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].seen;
}

/* Battle system accessors -- player and NPC current stamina and recovery. */
void
gs_set_playerstamina (scr_gameref_t gs, scr_int stamina)
{
  assert (gs_is_game_valid (gs));
  gs->playerstamina = stamina;
}

scr_int
gs_playerstamina (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->playerstamina;
}

void
gs_set_playerstaminacounter (scr_gameref_t gs, scr_int counter)
{
  assert (gs_is_game_valid (gs));
  gs->playerstaminacounter = counter;
}

scr_int
gs_playerstaminacounter (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->playerstaminacounter;
}

void
gs_set_playerwield (scr_gameref_t gs, scr_int object)
{
  assert (gs_is_game_valid (gs));
  gs->playerwield = object;
}

scr_int
gs_playerwield (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return gs->playerwield;
}

void
gs_set_npc_stamina (scr_gameref_t gs, scr_int npc, scr_int stamina)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].stamina = stamina;
}

scr_int
gs_npc_stamina (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].stamina;
}

void
gs_set_npc_staminacounter (scr_gameref_t gs, scr_int npc, scr_int counter)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].staminacounter = counter;
}

scr_int
gs_npc_staminacounter (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].staminacounter;
}

void
gs_set_npc_attackcounter (scr_gameref_t gs, scr_int npc, scr_int counter)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  gs->npcs[npc].attackcounter = counter;
}

scr_int
gs_npc_attackcounter (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].attackcounter;
}

/*
 * gs_player_battle()
 * gs_npc_battle()
 *
 * Return a pointer to the mutable Battle System attributes of the player or
 * of a given NPC, for the battle code to read and update in place.
 */
scr_battle_t *
gs_player_battle (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  return &gs->playerbattle;
}

scr_battle_t *
gs_npc_battle (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return &gs->npcs[npc].battle;
}

scr_int
gs_npc_walkstep_count (scr_gameref_t gs, scr_int npc)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count));
  return gs->npcs[npc].walkstep_count;
}

void
gs_set_npc_walkstep (scr_gameref_t gs,
                     scr_int npc, scr_int walk, scr_int walkstep)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count)
          && gs_in_range (walk, gs->npcs[npc].walkstep_count));
  gs->npcs[npc].walksteps[walk] = walkstep;
}

scr_int
gs_npc_walkstep (scr_gameref_t gs, scr_int npc, scr_int walk)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count)
          && gs_in_range (walk, gs->npcs[npc].walkstep_count));
  return gs->npcs[npc].walksteps[walk];
}

void
gs_decrement_npc_walkstep (scr_gameref_t gs, scr_int npc, scr_int walk)
{
  assert (gs_is_game_valid (gs) && gs_in_range (npc, gs->npc_count)
          && gs_in_range (walk, gs->npcs[npc].walkstep_count));
  gs->npcs[npc].walksteps[walk]--;
}


/*
 * Convenience functions for bulk clearance of references.
 */
void
gs_clear_npc_references (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  std::fill (gs->npc_references.begin (), gs->npc_references.end (), FALSE);
}

void
gs_clear_object_references (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  std::fill (gs->object_references.begin (), gs->object_references.end (),
             FALSE);
}

void
gs_set_multiple_references (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  std::fill (gs->multiple_references.begin (), gs->multiple_references.end (),
             TRUE);
}

void
gs_clear_multiple_references (scr_gameref_t gs)
{
  assert (gs_is_game_valid (gs));
  std::fill (gs->multiple_references.begin (), gs->multiple_references.end (),
             FALSE);
}


/*
 * gs_populate()
 * gs_create()
 *
 * Create and initialize a game state.
 */
static void
gs_populate (scr_gameref_t game, scr_var_setref_t vars,
             scr_prop_setref_t bundle, scr_filterref_t filter)
{
  scr_vartype_t vt_key[4];
  scr_int index_;

  game->magic = GAME_MAGIC;

  /* Store the variables, properties bundle, and filter references. */
  game->vars = vars;
  game->bundle = bundle;
  game->filter = filter;

  /* Set memento to NULL for now; it's added later. */
  game->memento = NULL;

  /* Initialize for no debugger. */
  game->debugger = NULL;

  /* Initialize the undo buffers to NULL for now. */
  game->temporary = NULL;
  game->undo = NULL;
  game->undo_available = FALSE;

  /* Carried-load tracking off until seeded below; default to Runner-faithful
   * running totals rather than the legacy per-check recompute. */
  game->carried_weight = 0;
  game->carried_size = 0;
  game->carried_ready = FALSE;
  game->capacity_recompute = FALSE;

  /* Create rooms state array. */
  vt_key[0].string = "Rooms";
  game->room_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->rooms.resize (game->room_count);

  /* Set up initial rooms states. */
  for (index_ = 0; index_ < game->room_count; index_++)
    gs_set_room_seen (game, index_, FALSE);

  /* Create objects state array. */
  vt_key[0].string = "Objects";
  game->object_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->objects.resize (game->object_count);

  /* The NPCs array is not built until further below, but the object setup
   * that follows needs the NPC count to validate objects whose initial state
   * is "held/worn by NPC" (some games name a nonexistent NPC); read it now. */
  vt_key[0].string = "NPCs";
  game->npc_count = prop_get_child_count (bundle, "I<-s", vt_key);
  vt_key[0].string = "Objects";

  /* Set up initial object states. */
  for (index_ = 0; index_ < game->object_count; index_++)
    {
      const scr_char *inroomdesc;
      scr_bool is_static, unmoved;

      vt_key[1].integer = index_;

      vt_key[2].string = "Static";
      is_static = prop_get_boolean (bundle, "B<-sis", vt_key);
      if (is_static)
        {
          scr_int type;

          vt_key[2].string = "Where";
          vt_key[3].string = "Type";
          type = prop_get_integer (bundle, "I<-siss", vt_key);
          if (type == ROOMLIST_NPC_PART)
            {
              scr_int parent;

              game->objects[index_].position = OBJ_PART_NPC;

              vt_key[2].string = "Parent";
              parent = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
              game->objects[index_].parent = parent;
            }
          else
            gs_object_make_hidden_unchecked (game, index_);
        }
      else
        {
          scr_int initialparent, initialposition;

          vt_key[2].string = "Parent";
          initialparent = prop_get_integer (bundle, "I<-sis", vt_key);
          vt_key[2].string = "InitialPosition";
          initialposition = prop_get_integer (bundle, "I<-sis", vt_key);
          switch (initialposition)
            {
            case 0:            /* Hidden. */
              gs_object_make_hidden_unchecked (game, index_);
              break;

            case 1:            /* Held. */
              if (initialparent == 0)   /* By player. */
                gs_object_player_get_unchecked (game, index_);
              else                      /* By NPC. */
                {
                  const scr_int npc = initialparent - 1;
                  if (npc >= 0 && npc < game->npc_count)
                    gs_object_npc_get_unchecked (game, index_, npc);
                  else
                    {
                      scr_error ("gs_create: object held by"
                                " nonexistent NPC, %ld\n", npc);
                      gs_object_make_hidden_unchecked (game, index_);
                    }
                }
              break;

            case 2:            /* In container. */
              {
                const scr_int container = obj_container_object (game,
                                                               initialparent);
                if (container >= 0 && container < game->object_count)
                  gs_object_move_into_unchecked (game, index_, container);
                else
                  {
                    scr_error ("gs_create: object in"
                              " nonexistent container, %ld\n", container);
                    gs_object_make_hidden_unchecked (game, index_);
                  }
              }
              break;

            case 3:            /* On surface. */
              {
                const scr_int surface = obj_surface_object (game,
                                                           initialparent);
                if (surface >= 0 && surface < game->object_count)
                  gs_object_move_onto_unchecked (game, index_, surface);
                else
                  {
                    scr_error ("gs_create: object on"
                              " nonexistent surface, %ld\n", surface);
                    gs_object_make_hidden_unchecked (game, index_);
                  }
              }
              break;

            default:           /* In room, or worn by player/NPC. */
              if (initialposition >= 4
                  && initialposition < 4 + game->room_count)
                {
                  gs_object_to_room_unchecked (game,
                                               index_, initialposition - 4);
                }
              else if (initialposition == 4 + game->room_count)
                {
                  if (initialparent == 0)
                    gs_object_player_wear_unchecked (game, index_);
                  else
                    {
                      const scr_int npc = initialparent - 1;
                      if (npc >= 0 && npc < game->npc_count)
                        gs_object_npc_wear_unchecked (game, index_, npc);
                      else
                        {
                          scr_error ("gs_create: object worn by"
                                    " nonexistent NPC, %ld\n", npc);
                          gs_object_make_hidden_unchecked (game, index_);
                        }
                    }
                }
              else
                {
                  scr_error ("gs_create: object in out of bounds room, %ld\n",
                            initialposition - 4 - game->room_count);
                  gs_object_to_room_unchecked (game, index_, -2);
                }
            }
        }

      vt_key[2].string = "CurrentState";
      gs_set_object_state (game, index_,
                           prop_get_integer (bundle, "I<-sis", vt_key));

      vt_key[2].string = "Openable";
      gs_set_object_openness (game, index_,
                              prop_get_integer (bundle, "I<-sis", vt_key));

      gs_set_object_seen (game, index_, FALSE);

      vt_key[2].string = "InRoomDesc";
      inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (inroomdesc))
        {
          vt_key[2].string = "OnlyWhenNotMoved";
          if (prop_get_integer (bundle, "I<-sis", vt_key) == 1)
            unmoved = TRUE;
          else
            unmoved = FALSE;
        }
      else
        unmoved = FALSE;
      gs_set_object_unmoved (game, index_, unmoved);
      gs_set_object_static_unmoved (game, index_, TRUE);
    }

  /* Create tasks state array. */
  vt_key[0].string = "Tasks";
  game->task_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->tasks.resize (game->task_count);

  /* Set up initial tasks states. */
  for (index_ = 0; index_ < game->task_count; index_++)
    {
      gs_set_task_done (game, index_, FALSE);
      gs_set_task_scored (game, index_, FALSE);
    }

  /* Create events state array. */
  vt_key[0].string = "Events";
  game->event_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->events.resize (game->event_count);

  /* Set up initial events states. */
  for (index_ = 0; index_ < game->event_count; index_++)
    {
      scr_int startertype;

      vt_key[1].integer = index_;
      vt_key[2].string = "StarterType";
      startertype = prop_get_integer (bundle, "I<-sis", vt_key);

      switch (startertype)
        {
        case 1:
          gs_set_event_state (game, index_, ES_WAITING);
          gs_set_event_time (game, index_, 0);
          break;

        case 2:
          {
            scr_int start, end;

            gs_set_event_state (game, index_, ES_WAITING);
            vt_key[2].string = "StartTime";
            start = prop_get_integer (bundle, "I<-sis", vt_key);
            vt_key[2].string = "EndTime";
            end = prop_get_integer (bundle, "I<-sis", vt_key);
            gs_set_event_time (game, index_, scr_randomint (start, end));
            break;
          }

        case 3:
          gs_set_event_state (game, index_, ES_AWAITING);
          gs_set_event_time (game, index_, 0);
          break;
        }
    }

  /* Create NPCs state array. */
  vt_key[0].string = "NPCs";
  game->npc_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->npcs.resize (game->npc_count);

  /* Set up initial NPCs states. */
  for (index_ = 0; index_ < game->npc_count; index_++)
    {
      scr_int walk, walkstep_count;

      gs_set_npc_position (game, index_, 0);
      gs_set_npc_parent (game, index_, -1);
      gs_set_npc_seen (game, index_, FALSE);
      game->npcs[index_].stamina = 0;
      game->npcs[index_].staminacounter = 0;
      game->npcs[index_].attackcounter = 0;
      memset (&game->npcs[index_].battle, 0, sizeof (game->npcs[index_].battle));

      vt_key[1].integer = index_;

      vt_key[2].string = "StartRoom";
      gs_set_npc_location (game, index_,
                           prop_get_integer (bundle, "I<-sis", vt_key));

      vt_key[2].string = "Walks";
      walkstep_count = prop_get_child_count (bundle, "I<-sis", vt_key);

      game->npcs[index_].walkstep_count = walkstep_count;
      game->npcs[index_].walksteps.resize (walkstep_count);

      for (walk = 0; walk < walkstep_count; walk++)
        gs_set_npc_walkstep (game, index_, walk, 0);
    }

  /* Set up the player portions of the game state. */
  vt_key[0].string = "Header";
  vt_key[1].string = "StartRoom";
  game->playerroom = prop_get_integer (bundle, "I<-ss", vt_key);
  vt_key[0].string = "Globals";
  vt_key[1].string = "ParentObject";
  game->playerparent = prop_get_integer (bundle, "I<-ss", vt_key) - 1;
  vt_key[1].string = "Position";
  game->playerposition = prop_get_integer (bundle, "I<-ss", vt_key);
  game->playerstamina = 0;
  game->playerstaminacounter = 0;
  game->playerwield = -1;
  memset (&game->playerbattle, 0, sizeof (game->playerbattle));

  /* Initialize score notifications from game properties. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "NoScoreNotify";
  game->notify_score_change = !prop_get_boolean (bundle, "B<-ss", vt_key);

  /* Miscellaneous state defaults. */
  game->turns = 0;
  game->score = 0;
  game->bold_room_names = TRUE;
  game->verbose = TRUE;
  /* The owning status strings default-construct to NULL via value-init above. */

  /* Resource controls. */
  res_clear_resource (&game->requested_sound);
  res_clear_resource (&game->requested_graphic);
  res_clear_resource (&game->playing_sound);
  res_clear_resource (&game->displayed_graphic);
  game->stop_sound = FALSE;
  game->sound_active = FALSE;

  /* Initialize wait turns from game properties. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "WaitTurns";
  game->waitturns = prop_get_integer (bundle, "I<-ss", vt_key);

  /* Non-game conveniences. */
  game->is_running = FALSE;
  game->has_notified = FALSE;
  game->is_admin = FALSE;
  game->has_completed = FALSE;
  game->waitcounter = 0;
  game->do_again = FALSE;
  game->redo_sequence = 0;
  game->do_restart = FALSE;
  game->do_restore = FALSE;

  game->object_references.assign (game->object_count, FALSE);
  game->multiple_references.assign (game->object_count, FALSE);
  game->npc_references.assign (game->npc_count, FALSE);

  game->it_object = -1;
  game->him_npc = -1;
  game->her_npc = -1;
  game->it_npc = -1;

  /* Seed the carried-load totals now that initial object placement is done;
   * this also arms gs_carried_track() for subsequent moves. */
  gs_carried_recompute (game);
}

scr_gameref_t
gs_create (scr_var_setref_t vars,
           scr_prop_setref_t bundle, scr_filterref_t filter)
{
  scr_gameref_t game;
  assert (vars && bundle && filter);

  /* Create the initial state structure.  sc_game_s owns std::unique_ptr
   * status strings (P3 RAII), so it is non-POD and must be new()'d — value-init
   * zeroes the POD fields (all of which gs_populate sets explicitly anyway)
   * and default-constructs the owning strings to NULL. */
  game = new scr_game_s ();

  /*
   * Populate the state from the bundle.  The property reads can throw
   * (scr_fatal on a corrupt bundle); reclaim the partially built game on that
   * path -- its vector and owned-string members free themselves on delete --
   * then let the throw carry on to the interface boundary.
   */
  try
    {
      gs_populate (game, vars, bundle, filter);
    }
  catch (...)
    {
      delete game;
      throw;
    }

  /* Return the constructed game state. */
  return game;
}


/*
 * gs_is_game_valid()
 *
 * Return TRUE if pointer is a valid game, FALSE otherwise.
 */
scr_bool
gs_is_game_valid (scr_gameref_t game)
{
  return game && game->magic == GAME_MAGIC;
}


/*
 * gs_string_copy()
 *
 * Helper for gs_copy(), copies one malloc'ed string to another, or NULL
 * if from is NULL, taking care not to leak memory.
 */
static void
gs_string_copy (scr_owned_string &to_string, const scr_char *from_string)
{
  /* Copy from_string if set, otherwise set to_string to NULL.  Assigning the
   * unique_ptr frees any current contents. */
  if (from_string)
    {
      scr_char *copy = (scr_char *) scr_malloc (strlen (from_string) + 1);
      memcpy (copy, from_string, strlen (from_string) + 1);
      to_string.reset (copy);
    }
  else
    to_string.reset ();
}


/*
 * gs_copy()
 *
 * Deep-copy the dynamic parts of a game onto another existing
 * game structure.
 */
void
gs_copy (scr_gameref_t to, scr_gameref_t from)
{
  const scr_prop_setref_t bundle = from->bundle;
  scr_vartype_t vt_key[3];
  scr_int var_count, var, npc;
  assert (gs_is_game_valid (to) && gs_is_game_valid (from));

  /*
   * Copy over references to the properties bundle and filter.  The debugger
   * is specifically excluded, as it's considered to be tied to the game.
   */
  to->bundle = from->bundle;
  to->filter = from->filter;

  /* Copy over references to the undo buffers. */
  to->temporary = from->temporary;
  to->undo = from->undo;
  to->undo_available = from->undo_available;

  /* Copy over all variables values. */
  vt_key[0].string = "Variables";
  var_count = prop_get_child_count (bundle, "I<-s", vt_key);

  for (var = 0; var < var_count; var++)
    {
      const scr_char *name;
      scr_int var_type;

      vt_key[1].integer = var;

      vt_key[2].string = "Name";
      name = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = "Type";
      var_type = prop_get_integer (bundle, "I<-sis", vt_key);

      switch (var_type)
        {
        case TAFVAR_NUMERIC:
          var_put_integer (to->vars, name, var_get_integer (from->vars, name));
          break;

        case TAFVAR_STRING:
          var_put_string (to->vars, name, var_get_string (from->vars, name));
          break;

        default:
          scr_fatal ("gs_copy: unknown variable type, %ld\n", var_type);
        }
    }

  /* Copy over the variable timestamp. */
  var_set_elapsed_seconds (to->vars, var_get_elapsed_seconds (from->vars));

  /* Copy over room states. */
  assert (to->room_count == from->room_count);
  to->rooms = from->rooms;

  /* Copy over object states. */
  assert (to->object_count == from->object_count);
  to->objects = from->objects;

  /* Copy over task states. */
  assert (to->task_count == from->task_count);
  to->tasks = from->tasks;

  /* Copy over event states. */
  assert (to->event_count == from->event_count);
  to->events = from->events;

  /* Copy over NPC states individually, to avoid walks problems. */
  for (npc = 0; npc < from->npc_count; npc++)
    {
      to->npcs[npc].location = from->npcs[npc].location;
      to->npcs[npc].position = from->npcs[npc].position;
      to->npcs[npc].parent = from->npcs[npc].parent;
      to->npcs[npc].seen = from->npcs[npc].seen;
      to->npcs[npc].stamina = from->npcs[npc].stamina;
      to->npcs[npc].staminacounter = from->npcs[npc].staminacounter;
      to->npcs[npc].attackcounter = from->npcs[npc].attackcounter;
      to->npcs[npc].battle = from->npcs[npc].battle;
      to->npcs[npc].walkstep_count = from->npcs[npc].walkstep_count;

      /* Copy over NPC walks information. */
      assert (to->npcs[npc].walkstep_count == from->npcs[npc].walkstep_count);
      to->npcs[npc].walksteps = from->npcs[npc].walksteps;
    }

  /* Copy over player information. */
  to->playerroom = from->playerroom;
  to->playerposition = from->playerposition;
  to->playerparent = from->playerparent;
  to->playerstamina = from->playerstamina;
  to->playerstaminacounter = from->playerstaminacounter;
  to->playerwield = from->playerwield;
  to->playerbattle = from->playerbattle;

  /*
   * Copy over miscellaneous other details.  Specifically exclude bold rooms,
   * verbose, and score notification, so that they are invariant across copies,
   * particularly undo/restore.
   */
  to->turns = from->turns;
  to->score = from->score;

  gs_string_copy (to->current_room_name, from->current_room_name.get ());
  gs_string_copy (to->status_line, from->status_line.get ());
  gs_string_copy (to->title, from->title.get ());
  gs_string_copy (to->author, from->author.get ());
  gs_string_copy (to->hint_text, from->hint_text.get ());

  /*
   * Specifically exclude playing sound and displayed graphic from the copy
   * so that they remain invariant across game copies.
   */
  to->requested_sound = from->requested_sound;
  to->requested_graphic = from->requested_graphic;
  to->stop_sound = from->stop_sound;

  to->is_running = from->is_running;
  to->has_notified = from->has_notified;
  to->is_admin = from->is_admin;
  to->has_completed = from->has_completed;

  to->waitturns = from->waitturns;

  to->waitcounter = from->waitcounter;
  to->do_again = from->do_again;
  to->redo_sequence = from->redo_sequence;
  to->do_restart = from->do_restart;
  to->do_restore = from->do_restore;

  to->object_references = from->object_references;
  to->multiple_references = from->multiple_references;
  to->npc_references = from->npc_references;

  to->it_object = from->it_object;
  to->him_npc = from->him_npc;
  to->her_npc = from->her_npc;
  to->it_npc = from->it_npc;

  /*
   * Recompute the carried-load totals from the copied object positions rather
   * than carrying over the source's running totals.  This matches the Runner,
   * which recomputes carried load when it loads state, so undo and restore both
   * "heal" any accumulated take/drop double-count.  The capacity_recompute mode
   * flag is a session preference, so (like verbose) it is left invariant across
   * copies and deliberately not propagated here.
   */
  gs_carried_recompute (to);
}


/*
 * gs_destroy()
 *
 * Free all the memory associated with a game state.
 */
void
gs_destroy (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Drop the parser's cached %object%/%character% candidates and the task,
   * serializer, event and command-matcher property caches for this game, so a
   * later game allocated at the same address can't inherit them. */
  uip_forget_game (game);
  task_forget_game (game);
  ser_forget_game (game);
  evt_forget_game (game);
  run_forget_game (game);

  /* The state arrays (rooms, objects, tasks, events, npcs and their walksteps,
   * the *_references) are std::vector and the owning game strings
   * (current_room_name, status_line, title, author, hint_text) are
   * scr_owned_string; all free themselves when the game is delete'd below
   * (P3 RAII), so there is nothing to free by hand here. */

  /* Free the game state itself; its destructor releases the owned strings.
   * (The old 0xaa poison is gone — it would corrupt the unique_ptr members the
   * destructor is about to free, the same reason the scr_filter_s struct moved
   * to new/delete in P3.) */
  delete game;
}

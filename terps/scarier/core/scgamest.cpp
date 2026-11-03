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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "sctafpar.h"
#include "scvars.h"
#include "scprintf.h"
#include "scmemos.h"
#include "scgamest_api.h"
#include "scparser.h"
#include "sclibrar_api.h"
#include "scresour.h"
#include "scrunner_api.h"
#include "scevents.h"
#include "sctasks.h"
#include "scobjcts.h"
#include "scserial.h"
#include "scdump.h"
#include "scgamest.h"
#include "scmap.h"


/* Assorted definitions and constants. */
static const scr_uint GAME_MAGIC = 0x35aed26e;


/*
 * gs_game_ok()
 * gs_in_range()
 * gs_index_fatal()
 * GS_CHECK()
 * GS_CHECK_INDEX()
 * GS_CHECK_EVENT(), GS_CHECK_ROOM(), GS_CHECK_TASK(), GS_CHECK_OBJECT(),
 * GS_CHECK_NPC()
 *
 * The accessor guards.  Every accessor in this file opens with one of them:
 * GS_CHECK (gs) for the game alone, GS_CHECK_<KIND> (gs, index) for an entity
 * index as well.  The two halves are different kinds of check.
 *
 * The game half is an invariant: no game file can make a caller pass a
 * pointer that is not a game, so it is an assert().  The normal builds keep
 * asserts, which is why the validity test here is the file-local inline
 * gs_game_ok() rather than a call to the exported gs_is_game_valid() (that
 * call was 5% of self time in the 2026-10-04 profile, see test/README.md).
 * gs_is_game_valid() is the same test for the other modules.
 *
 * The index half is not an invariant.  Entity indexes reach these accessors
 * from the game file and from saved games (an object's parent, a task named
 * by an alternate description, a walk's character), and published games have
 * out of range ones; gs_create() repairs the cases met so far, see
 * test/adrift4/harness/badparent_test.cpp.  One that gets this far is bad
 * game data, so it is an scr_fatal(), which the entry points in scinterf.cpp
 * catch and turn into a clean failure, and not an abort() of the host.
 */
static inline scr_bool
gs_game_ok (const struct scr_game_s *game)
{
  return game && game->magic == GAME_MAGIC;
}

static inline scr_bool
gs_in_range (scr_int value, scr_int limit)
{
  return value >= 0 && value < limit;
}

#if defined(__GNUC__)
__attribute__ ((__noinline__, __cold__))
#endif
static void
gs_index_fatal (const scr_char *function,
                const scr_char *kind, scr_int value, scr_int limit)
{
  scr_fatal ("%s: %s %ld out of range, game has %ld\n",
             function, kind, value, limit);
}

#define GS_CHECK(gs) assert (gs_game_ok (gs))
#define GS_CHECK_INDEX(gs, kind, value, limit) \
  do \
    { \
      GS_CHECK (gs); \
      if (!gs_in_range ((value), (limit))) \
        gs_index_fatal (__func__, (kind), (value), (limit)); \
    } \
  while (0)
#define GS_CHECK_EVENT(gs, event) \
  GS_CHECK_INDEX (gs, "event", event, (gs)->event_count)
#define GS_CHECK_ROOM(gs, room) \
  GS_CHECK_INDEX (gs, "room", room, (gs)->room_count)
#define GS_CHECK_TASK(gs, task) \
  GS_CHECK_INDEX (gs, "task", task, (gs)->task_count)
#define GS_CHECK_OBJECT(gs, object) \
  GS_CHECK_INDEX (gs, "object", object, (gs)->object_count)
#define GS_CHECK_NPC(gs, npc) \
  GS_CHECK_INDEX (gs, "npc", npc, (gs)->npc_count)
#define GS_CHECK_WALK(gs, npc, walk) \
  do \
    { \
      GS_CHECK_NPC (gs, npc); \
      GS_CHECK_INDEX (gs, "walk", walk, (gs)->npcs[npc].walkstep_count); \
    } \
  while (0)


/*
 * Optional "capacity assist" default (opt-in, off by default).  The per-game
 * flag gs->capacity_recompute selects the legacy recompute-from-held-objects
 * accounting instead of the Runner's running total, and is normally flipped
 * per session by `glk capacity on` or by the known-game assist table, both of
 * which act on a game that already exists.  Hosts with no game in hand when
 * they choose -- the headless harness, which reads SCR_ASSUME_CAPACITY in its
 * pre-main constructor -- set this default instead, and every game created
 * afterwards starts with the assist on.
 */
static scr_bool gs_capacity_assist = FALSE;


/*
 * gs_set_capacity_assist()
 * gs_get_capacity_assist()
 *
 * Set and read the capacity assist default for games created later.
 */
void
gs_set_capacity_assist (scr_bool flag)
{
  gs_capacity_assist = flag;
}

scr_bool
gs_get_capacity_assist (void)
{
  return gs_capacity_assist;
}


/*
 * gs_carried_recompute()
 *
 * Seed the player's carried weight and size totals from the objects
 * currently held or worn, the way the run400 object loader does (Proc_19_5):
 * a held object adds its own BASE weight and size (loc_490709-490731), a
 * worn object its own base weight only (loc_4907A3-4907BB) -- contained
 * objects' weights are NOT rolled in, so a restore "loses" the contents
 * weight of anything held.  Proc_19_5 reads object records from the
 * adventure file itself, so this same seeding runs for a NEW game too
 * (measured live in run400: goldilocks turn-zero `count` reports the worn
 * items' weight), after Proc_19_4's tail (loc_45AA71-45AA7E) zeroes the
 * totals.  Called at new game and after restoring a saved game; undo
 * snapshots carry the totals verbatim (Proc_19_62, loc_45AEA4-45AEB7).
 */
void
gs_carried_recompute (scr_gameref_t gs)
{
  scr_int index_, weight = 0, size = 0;

  for (index_ = 0; index_ < gs->object_count; index_++)
    {
      if (gs->objects[index_].position == OBJ_HELD_PLAYER)
        {
          weight += obj_get_base_weight (gs, index_);
          size += obj_get_size (gs, index_);
        }
      else if (gs->objects[index_].position == OBJ_WORN_PLAYER)
        weight += obj_get_base_weight (gs, index_);
    }
  gs->carried_weight = weight;
  gs->carried_size = size;
  gs->carried_ready = TRUE;
}


/*
 * gs_runner_reaches()
 * gs_runner_possessed()
 * gs_runner_worn()
 *
 * The Runner's two possession predicates, Proc_21_46 @44615C ("with the
 * player") and Proc_21_45 @444BD0 ("worn, or inside something worn").  Both
 * walk the bare [2E] container field -- our runner_parent shadow -- and
 * neither consults openness or container/surface flags, so an object sealed
 * inside a closed carried box still counts as carried.  32 bottoms out an
 * authored parent cycle that the Runner itself would hang on.
 */
enum { GS_RUNNER_WALK_LIMIT = 32 };

static scr_bool
gs_runner_reaches (scr_gameref_t gs, scr_int object, scr_bool worn_only)
{
  scr_int walk = object;
  int depth;

  for (depth = 0; depth < GS_RUNNER_WALK_LIMIT; depth++)
    {
      const scr_int position = gs->objects[walk].position;

      if (position == OBJ_WORN_PLAYER)
        return TRUE;
      if (position == OBJ_HELD_PLAYER)
        return !worn_only;
      if (position != OBJ_IN_OBJECT && position != OBJ_ON_OBJECT)
        return FALSE;
      walk = gs->objects[walk].runner_parent;
      if (walk < 0 || walk >= gs->object_count)
        return FALSE;
    }
  return FALSE;
}

scr_bool
gs_runner_possessed (scr_gameref_t gs, scr_int object)
{
  return gs_runner_reaches (gs, object, FALSE);
}

static scr_bool
gs_runner_worn (scr_gameref_t gs, scr_int object)
{
  return gs_runner_reaches (gs, object, TRUE);
}


/*
 * gs_carried_track()
 *
 * Maintain the carried weight/size running totals as objects move around.
 * 4.0 funnels every move through one generic setter, Proc_21_54 @4528D8, and
 * that setter is the only thing that touches the two totals the `count`
 * command prints; the direct writes elsewhere in Proc_19_6/Proc_19_7 sit on
 * branches no player-facing command reaches.  Its rule, transcribed from
 * loc_4527DD-4528D2 and confirmed command by command against a purpose-built
 * probe adventure in run400 (2026-08-23):
 *
 *   was possessed:
 *     no longer possessed          weight -= RECURSIVE weight (Proc_21_55)
 *     no longer possessed, or                                 @447680
 *       newly worn                 size   -= OWN size ([2C], not the
 *                                                      recursive Proc_21_56)
 *     still possessed, no longer   size   += OWN size
 *       worn
 *   was not possessed:
 *     possessed now                weight += RECURSIVE weight
 *     possessed now, not worn      size   += OWN size
 *
 * Neither total is ever recomputed, so the Runner's asymmetries are leaks
 * that persist for the rest of the game and we reproduce them:
 *
 *   Putting a held object into a carried bag keeps its size counted, but
 *   dropping that bag only refunds the BAG's own size -- the contents' size
 *   stays on the total forever (measured: bag 9 + rock 27 = 36, drop bag
 *   leaves 27).  Weight refunds correctly because it recurses.
 *
 *   Wearing debits the size twice if the worn object is then dropped or put
 *   away, because "newly worn" and "no longer possessed" are separate
 *   subtractions with no matching credit (measured: held pack 50, wear 47,
 *   drop 44).
 *
 * The move-object task action (Proc_19_10) does its own accounting in
 * task_move_object.  No-op until the totals are seeded (carried_ready), so
 * it ignores object placement during game setup.
 *
 * All of this is 4.0's alone.  run390 has no single generic setter -- it
 * adjusts the same two globals from each command handler in turn -- and its
 * arithmetic comes out exact, so a 3.9 game reads its load back from
 * lib_carried_size()/lib_carried_weight() instead of from here; see
 * obj_uses_running_load().  The totals are still tracked for every version,
 * because a .tas save carries them; below 4.0 only a 3.9 game's weight is
 * consulted (obj_uses_running_weight()), the rule above standing in for
 * run390's per-handler arithmetic, which agrees with it on weight.
 */
static void
gs_carried_track (scr_gameref_t gs, scr_int object, scr_int old_pos,
                  scr_int new_pos, scr_bool was_possessed,
                  scr_bool was_worn)
{
  /*
   * A wielded weapon that leaves the player's hands -- dropped, thrown, given
   * away, put down, worn -- is no longer wielded, and the wield never falls
   * back to another carried weapon (Runner, settled live 2026-08-01).  Restore
   * bypasses this by assigning positions directly, so a restored wield is
   * whatever the save says.
   */
  if (object == gs->playerwield
      && old_pos == OBJ_HELD_PLAYER && new_pos != OBJ_HELD_PLAYER)
    gs->playerwield = -1;

  if (!gs->carried_ready || gs->carried_suspend)
    return;

  {
    const scr_bool is_possessed = gs_runner_possessed (gs, object);
    const scr_bool is_worn = gs_runner_worn (gs, object);

    if (was_possessed)
      {
        if (!is_possessed)
          gs->carried_weight -= obj_get_weight (gs, object);
        if (!is_possessed || (!was_worn && is_worn))
          gs->carried_size -= obj_get_size (gs, object);
        if (is_possessed && was_worn && !is_worn)
          gs->carried_size += obj_get_size (gs, object);
      }
    else if (is_possessed)
      {
        gs->carried_weight += obj_get_weight (gs, object);
        if (!is_worn)
          gs->carried_size += obj_get_size (gs, object);
      }
  }
}


/*
 * gs_move_player_to_room()
 * gs_player_in_room()
 *
 * Move the player to a given room, and check presence in a given room.
 */
void
gs_move_player_to_room (scr_gameref_t game, scr_int room)
{
  GS_CHECK (game);

  if (room < 0)
    {
      scr_fatal ("gs_move_player_to_room: invalid room, %ld\n", room);
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
  GS_CHECK (game);
  return game->playerroom == room;
}


/*
 * gs_room_valid()
 * gs_task_valid()
 * gs_object_valid()
 * gs_npc_valid()
 *
 * Range checks for a 0-based index read from the game file.  A TAF can name
 * any number in a task, room, object or NPC reference, and the accessors
 * below only assert their indices, so callers holding a file-derived index
 * test it here first and treat an out-of-range one as no reference at all.
 */
scr_bool
gs_room_valid (scr_gameref_t gs, scr_int room)
{
  GS_CHECK (gs);
  return gs_in_range (room, gs->room_count);
}

scr_bool
gs_task_valid (scr_gameref_t gs, scr_int task)
{
  GS_CHECK (gs);
  return gs_in_range (task, gs->task_count);
}

scr_bool
gs_object_valid (scr_gameref_t gs, scr_int object)
{
  GS_CHECK (gs);
  return gs_in_range (object, gs->object_count);
}

scr_bool
gs_npc_valid (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK (gs);
  return gs_in_range (npc, gs->npc_count);
}


/*
 * gs_*()
 *
 * Game accessors and mutators.
 */
scr_var_setref_t
gs_get_vars (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->vars;
}

scr_prop_setref_t
gs_get_bundle (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->bundle;
}

scr_filterref_t
gs_get_filter (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->filter;
}

scr_memo_setref_t
gs_get_memento (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->memento;
}


/*
 * Game accessors and mutators for the player.
 */
void
gs_set_playerroom (scr_gameref_t gs, scr_int room)
{
  GS_CHECK (gs);
  gs->playerroom = room;
}

void
gs_set_playerposition (scr_gameref_t gs, scr_int position)
{
  GS_CHECK (gs);
  gs->playerposition = position;
}

void
gs_set_playerparent (scr_gameref_t gs, scr_int parent)
{
  GS_CHECK (gs);
  gs->playerparent = parent;
}

scr_int
gs_playerroom (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->playerroom;
}

scr_int
gs_playerposition (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->playerposition;
}

scr_int
gs_playerparent (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->playerparent;
}

scr_int
gs_carried_weight (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->carried_weight;
}

scr_int
gs_carried_size (scr_gameref_t gs)
{
  scr_int possessed = 0, object;

  GS_CHECK (gs);

  /*
   * Deliberate deviation (2026-09-30).  run400 takes only a container's OWN
   * size off the running total when it leaves the player, so whatever was
   * put inside it stays charged for good (e66709de2): `take bag`, `take
   * rock`, `put rock in bag`, `drop bag` leaves 3 on `count` with empty
   * hands, and every such drop brings "hands are full" nearer.  The total
   * is capped at what the player still possesses by the same rule, so the
   * Runner's own count stands everywhere it does not leak.
   */
  for (object = 0; object < gs->object_count; object++)
    {
      if (gs_runner_possessed (gs, object) && !gs_runner_worn (gs, object))
        possessed += obj_get_size (gs, object);
    }
  if (gs->carried_size > possessed)
    SCR_DEVIATION ("carried_cap", "runner=%ld ours=%ld", gs->carried_size,
                   possessed);
  return gs->carried_size < possessed ? gs->carried_size : possessed;
}

void
gs_set_carried_suspend (scr_gameref_t gs, scr_bool flag)
{
  GS_CHECK (gs);
  gs->carried_suspend = flag;
}

scr_bool
gs_runner_phantom_held (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->runner_phantom_held;
}

void
gs_set_runner_phantom_held (scr_gameref_t gs, scr_bool flag)
{
  GS_CHECK (gs);
  gs->runner_phantom_held = flag;
}

void
gs_carried_adjust (scr_gameref_t gs, scr_int weight, scr_int size)
{
  GS_CHECK (gs);
  if (!gs->carried_ready)
    return;
  gs->carried_weight += weight;
  gs->carried_size += size;
}


/*
 * Game accessors and mutators for events.
 */
scr_int
gs_event_count (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->event_count;
}

void
gs_set_event_state (scr_gameref_t gs, scr_int event, scr_int state)
{
  GS_CHECK_EVENT (gs, event);
  gs->events[event].state = state;
}

void
gs_set_event_time (scr_gameref_t gs, scr_int event, scr_int etime)
{
  GS_CHECK_EVENT (gs, event);
  gs->events[event].time = etime;
}

scr_int
gs_event_state (scr_gameref_t gs, scr_int event)
{
  GS_CHECK_EVENT (gs, event);
  return gs->events[event].state;
}

void
gs_set_event_loadtime (scr_gameref_t gs, scr_int event, scr_int etime)
{
  GS_CHECK_EVENT (gs, event);
  gs->events[event].loadtime = etime;
}

scr_int
gs_event_loadtime (scr_gameref_t gs, scr_int event)
{
  GS_CHECK_EVENT (gs, event);
  return gs->events[event].loadtime;
}

void
gs_set_event_taskstate (scr_gameref_t gs, scr_int event, scr_bool done)
{
  GS_CHECK_EVENT (gs, event);
  gs->events[event].taskstate = done ? 1 : 0;
}

scr_bool
gs_event_taskstate (scr_gameref_t gs, scr_int event)
{
  GS_CHECK_EVENT (gs, event);
  return gs->events[event].taskstate != 0;
}

void
gs_set_event_ticked (scr_gameref_t gs, scr_int event, scr_bool ticked)
{
  GS_CHECK_EVENT (gs, event);
  gs->events[event].ticked = ticked ? 1 : 0;
}

scr_bool
gs_event_ticked (scr_gameref_t gs, scr_int event)
{
  GS_CHECK_EVENT (gs, event);
  return gs->events[event].ticked != 0;
}

scr_int
gs_event_time (scr_gameref_t gs, scr_int event)
{
  GS_CHECK_EVENT (gs, event);
  return gs->events[event].time;
}

void
gs_decrement_event_time (scr_gameref_t gs, scr_int event)
{
  GS_CHECK_EVENT (gs, event);
  gs->events[event].time--;
}


/*
 * Game accessors and mutators for rooms.
 */
scr_int
gs_room_count (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->room_count;
}

void
gs_set_room_seen (scr_gameref_t gs, scr_int room, scr_bool seen)
{
  GS_CHECK_ROOM (gs, room);
  gs->rooms[room].visited = seen;
}

scr_bool
gs_room_seen (scr_gameref_t gs, scr_int room)
{
  GS_CHECK_ROOM (gs, room);
  return gs->rooms[room].visited;
}


/*
 * Game accessors and mutators for tasks.
 */
scr_int
gs_task_count (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->task_count;
}

void
gs_set_task_done (scr_gameref_t gs, scr_int task, scr_bool done)
{
  GS_CHECK_TASK (gs, task);
  gs->tasks[task].done = done;
}

void
gs_set_task_scored (scr_gameref_t gs, scr_int task, scr_bool scored)
{
  GS_CHECK_TASK (gs, task);
  gs->tasks[task].scored = scored;
}

scr_bool
gs_task_done (scr_gameref_t gs, scr_int task)
{
  GS_CHECK_TASK (gs, task);
  return gs->tasks[task].done;
}

scr_bool
gs_task_scored (scr_gameref_t gs, scr_int task)
{
  GS_CHECK_TASK (gs, task);
  return gs->tasks[task].scored;
}


/*
 * Game accessors and mutators for objects.
 */
scr_int
gs_object_count (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->object_count;
}

void
gs_set_object_openness (scr_gameref_t gs, scr_int object, scr_int openness)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].openness = openness;
}

void
gs_set_object_state (scr_gameref_t gs, scr_int object, scr_int state)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].state = state;
}

void
gs_set_object_seen (scr_gameref_t gs, scr_int object, scr_bool seen)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].seen = seen;
}


/*
 * "Unmoved" is narrower than it sounds, and is not a general move tracker.
 * It is the live half of the Runner's OnlyWhenNotMoved byte, o(132): the
 * loader stores the authored mode, and exactly one other place in the whole
 * of run400 ever writes it back -- `takes` @0047BF66, which turns a 1 into
 * -1 the moment the library take handler successfully picks the object up
 * (`If o(132) = 1 Then o(132) = &HFF`).  Nothing else spends it: not a drop,
 * not a put, not a task moving the object about, not an NPC taking it.  So
 * the movers below leave the flag alone and the library take paths in
 * sclibrar_take*.cpp, sclibrar_open.cpp and sclibrar_put.cpp clear it by
 * hand.  See obj_shows_initial_description().
 */
void
gs_set_object_unmoved (scr_gameref_t gs, scr_int object, scr_bool unmoved)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].unmoved = unmoved;
}

void
gs_set_object_static_unmoved (scr_gameref_t gs, scr_int object,
                              scr_bool unmoved)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].static_unmoved = unmoved;
}

scr_int
gs_object_openness (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].openness;
}

scr_int
gs_object_state (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].state;
}

scr_bool
gs_object_seen (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].seen;
}

scr_bool
gs_object_unmoved (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].unmoved;
}

scr_bool
gs_object_static_unmoved (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].static_unmoved;
}


/*
 * A 4.0 event-moved static's room-presence set; see scr_objectstate_t.
 */
void
gs_object_static_rooms_clear (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].static_rooms.clear ();
}

void
gs_object_static_rooms_add (scr_gameref_t gs, scr_int object, scr_int room)
{
  std::vector<scr_int> *rooms;
  std::vector<scr_int>::iterator at;

  GS_CHECK_OBJECT (gs, object);
  rooms = &gs->objects[object].static_rooms;
  at = std::lower_bound (rooms->begin (), rooms->end (), room);
  if (at == rooms->end () || *at != room)
    rooms->insert (at, room);
}

scr_bool
gs_object_static_rooms_empty (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].static_rooms.empty ();
}

scr_bool
gs_object_in_static_rooms (scr_gameref_t gs, scr_int object, scr_int room)
{
  const std::vector<scr_int> *rooms;

  GS_CHECK_OBJECT (gs, object);
  rooms = &gs->objects[object].static_rooms;
  return std::binary_search (rooms->begin (), rooms->end (), room);
}

scr_int
gs_object_position (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].position;
}

scr_int
gs_object_parent (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].parent;
}


/*
 * The Runner-shadow container field; see its note in scgamest.h.  The
 * movers below keep it in step the way run400's do:
 *
 *   - A put-in/put-on writes the container or surface object.
 *   - Moving an object *out of* an in/on position detaches it: the field
 *     is cleared, whatever the destination (take out of container, task
 *     move to a room, "plant bean" sending an in-packet object hidden).
 *   - Removing a worn object also clears it (measured: remove+drop of the
 *     load-worn watch took Goldilocks's package phantom from 19 to 10).
 *   - NPC possession always clears it, matching the loader's treatment of
 *     NPC-held/worn initial placements.
 *   - A TASK move (execute_action, Proc_19_10) writes the field in every
 *     arm: &HFF for a room, hidden, roomgroup, player-held, player-worn or
 *     same-room destination, the container or surface for into/onto, and
 *     the NPC index for NPC-held/worn.  Measured with p4WORNNPC probes
 *     (2026-09-25): a task sending a stale-linked cloak or feather to
 *     hidden or to a room takes it out of the coin's weight, and one
 *     handing the cloak to NPC 0 moves its phantom from object 1 to
 *     object 0.  task_move_object() applies this after each mover.
 *   - Every other move leaves it untouched.  Measured: the broken bottle
 *     (stale raw Parent 0) keeps phantom-weighing the package through a
 *     player "take" from a room and a "drop".  Note that `count` never
 *     re-weighs: the running load only changes by the moved object's own
 *     weigh at the time of its move, so a child dropping out of a held
 *     object's phantom set shows up on the next weigh of that object
 *     (a drop or a take), not in `count`.
 */


/*
 * gs_rp_detach()
 *
 * Clear runner_parent only when the move detaches the object from an
 * in/on placement (and, when clear_worn is set, from a worn one).  All
 * other sources leave any stale value in place, as the Runner does.
 * Must be called before the mover rewrites the position field.
 */
static void
gs_rp_detach (scr_gameref_t gs, scr_int object, scr_bool clear_worn)
{
  const scr_int old_pos = gs->objects[object].position;

  if (old_pos == OBJ_IN_OBJECT || old_pos == OBJ_ON_OBJECT
      || (clear_worn
          && (old_pos == OBJ_WORN_PLAYER || old_pos == OBJ_WORN_NPC)))
    gs->objects[object].runner_parent = -1;
}

scr_int
gs_object_runner_parent (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  return gs->objects[object].runner_parent;
}

void
gs_set_object_runner_parent (scr_gameref_t gs, scr_int object,
                             scr_int runner_parent)
{
  GS_CHECK_OBJECT (gs, object);
  gs->objects[object].runner_parent = runner_parent;
}


/*
 * How a mover treats runner_parent: set it outright, or leave any stale
 * value unless gs_rp_detach() clears it (with or without the worn case).
 */
enum gs_rp_mode_t { GS_RP_SET, GS_RP_DETACH, GS_RP_DETACH_WORN };


/*
 * gs_object_place_unchecked()
 *
 * The body every unchecked mover below shares.  Both Runner predicates walk
 * the parent chain and gs_rp_detach() reads the old position, so all three
 * run before the move rewrites position and parent.
 */
static void
gs_object_place_unchecked (scr_gameref_t gs, scr_int object,
                           scr_int position, scr_int parent,
                           gs_rp_mode_t rp_mode, scr_int runner_parent)
{
  scr_int old_pos = gs->objects[object].position;
  scr_bool was_possessed, was_worn;

  GS_CHECK_OBJECT (gs, object);
  was_possessed = gs_runner_possessed (gs, object);
  was_worn = gs_runner_worn (gs, object);
  if (rp_mode != GS_RP_SET)
    gs_rp_detach (gs, object, rp_mode == GS_RP_DETACH_WORN);
  gs->objects[object].position = position;
  gs->objects[object].parent = parent;
  if (rp_mode == GS_RP_SET)
    gs->objects[object].runner_parent = runner_parent;
  gs_carried_track (gs, object, old_pos, position, was_possessed, was_worn);
}

static void
gs_object_move_onto_unchecked (scr_gameref_t gs, scr_int object, scr_int onto)
{
  gs_object_place_unchecked (gs, object, OBJ_ON_OBJECT, onto,
                             GS_RP_SET, onto);
}

static void
gs_object_move_into_unchecked (scr_gameref_t gs, scr_int object, scr_int into)
{
  gs_object_place_unchecked (gs, object, OBJ_IN_OBJECT, into,
                             GS_RP_SET, into);
}

static void
gs_object_make_hidden_unchecked (scr_gameref_t gs, scr_int object)
{
  gs_object_place_unchecked (gs, object, OBJ_HIDDEN, -1, GS_RP_DETACH, -1);
}

static void
gs_object_player_get_unchecked (scr_gameref_t gs, scr_int object)
{
  gs_object_place_unchecked (gs, object, OBJ_HELD_PLAYER, -1,
                             GS_RP_DETACH_WORN, -1);
}

static void
gs_object_npc_get_unchecked (scr_gameref_t gs, scr_int object, scr_int npc)
{
  gs_object_place_unchecked (gs, object, OBJ_HELD_NPC, npc, GS_RP_SET, -1);
}

static void
gs_object_player_wear_unchecked (scr_gameref_t gs, scr_int object)
{
  gs_object_place_unchecked (gs, object, OBJ_WORN_PLAYER, 0,
                             GS_RP_DETACH, -1);
}

static void
gs_object_npc_wear_unchecked (scr_gameref_t gs, scr_int object, scr_int npc)
{
  gs_object_place_unchecked (gs, object, OBJ_WORN_NPC, npc, GS_RP_SET, -1);
}

static void
gs_object_to_room_unchecked (scr_gameref_t gs, scr_int object, scr_int room)
{
  gs_object_place_unchecked (gs, object, room + 1, -1, GS_RP_DETACH, -1);
}

void
gs_object_move_onto (scr_gameref_t gs, scr_int object, scr_int onto)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_ON_OBJECT
      || gs->objects[object].parent != onto)
    {
      gs_object_move_onto_unchecked (gs, object, onto);
    }
}

void
gs_object_move_into (scr_gameref_t gs, scr_int object, scr_int into)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_IN_OBJECT
      || gs->objects[object].parent != into)
    {
      gs_object_move_into_unchecked (gs, object, into);
    }
}

void
gs_object_make_hidden (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_HIDDEN)
    {
      gs_object_make_hidden_unchecked (gs, object);
    }
}

void
gs_object_player_get (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_HELD_PLAYER)
    {
      gs_object_player_get_unchecked (gs, object);
    }
}

void
gs_object_npc_get (scr_gameref_t gs, scr_int object, scr_int npc)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_HELD_NPC
      || gs->objects[object].parent != npc)
    {
      gs_object_npc_get_unchecked (gs, object, npc);
    }
}

void
gs_object_player_wear (scr_gameref_t gs, scr_int object)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_WORN_PLAYER)
    {
      gs_object_player_wear_unchecked (gs, object);
    }
}

void
gs_object_npc_wear (scr_gameref_t gs, scr_int object, scr_int npc)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != OBJ_WORN_NPC
      || gs->objects[object].parent != npc)
    {
      gs_object_npc_wear_unchecked (gs, object, npc);
    }
}

void
gs_object_to_room (scr_gameref_t gs, scr_int object, scr_int room)
{
  GS_CHECK_OBJECT (gs, object);
  if (gs->objects[object].position != room + 1)
    {
      gs_object_to_room_unchecked (gs, object, room);
    }
}


/*
 * Game accessors and mutators for NPCs.
 */
scr_int
gs_npc_count (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->npc_count;
}

void
gs_set_npc_location (scr_gameref_t gs, scr_int npc, scr_int location)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].location = location;

  /*
   * Any placement at all clears the walk-hidden marker; npc_tick_npc_walk()
   * sets it again immediately after stamping a Hidden stop.  It clears the
   * dead marker for the same reason: both stand in for a value the Runner
   * keeps in the room field this call overwrites.
   */
  gs->npcs[npc].walk_hidden = FALSE;
  gs->npcs[npc].dead = FALSE;
}

void
gs_set_npc_walk_hidden (scr_gameref_t gs, scr_int npc, scr_bool hidden)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].walk_hidden = hidden;
}

scr_bool
gs_npc_walk_hidden (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].walk_hidden;
}

void
gs_set_npc_dead (scr_gameref_t gs, scr_int npc, scr_bool dead)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].dead = dead;
}

scr_bool
gs_npc_dead (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].dead;
}

scr_int
gs_npc_location (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].location;
}

void
gs_set_npc_position (scr_gameref_t gs, scr_int npc, scr_int position)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].position = position;
}

scr_int
gs_npc_position (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].position;
}

void
gs_set_npc_parent (scr_gameref_t gs, scr_int npc, scr_int parent)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].parent = parent;
}

scr_int
gs_npc_parent (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].parent;
}

void
gs_set_npc_seen (scr_gameref_t gs, scr_int npc, scr_bool seen)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].seen = seen;
}

scr_bool
gs_npc_seen (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].seen;
}


/*
 * Battle system accessors -- player and NPC current stamina and recovery.
 */
void
gs_set_playerstamina (scr_gameref_t gs, scr_int stamina)
{
  GS_CHECK (gs);
  gs->playerstamina = stamina;
}

scr_int
gs_playerstamina (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->playerstamina;
}

void
gs_set_playerstaminacounter (scr_gameref_t gs, scr_int counter)
{
  GS_CHECK (gs);
  gs->playerstaminacounter = counter;
}

scr_int
gs_playerstaminacounter (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->playerstaminacounter;
}

void
gs_set_playerwield (scr_gameref_t gs, scr_int object)
{
  GS_CHECK (gs);
  gs->playerwield = object;
}

scr_int
gs_playerwield (scr_gameref_t gs)
{
  GS_CHECK (gs);
  return gs->playerwield;
}

void
gs_set_npc_stamina (scr_gameref_t gs, scr_int npc, scr_int stamina)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].stamina = stamina;
}

scr_int
gs_npc_stamina (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].stamina;
}

void
gs_set_npc_staminacounter (scr_gameref_t gs, scr_int npc, scr_int counter)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].staminacounter = counter;
}

scr_int
gs_npc_staminacounter (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].staminacounter;
}

void
gs_set_npc_attackcounter (scr_gameref_t gs, scr_int npc, scr_int counter)
{
  GS_CHECK_NPC (gs, npc);
  gs->npcs[npc].attackcounter = counter;
}

scr_int
gs_npc_attackcounter (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
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
  GS_CHECK (gs);
  return &gs->playerbattle;
}

scr_battle_t *
gs_npc_battle (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return &gs->npcs[npc].battle;
}

scr_int
gs_npc_walkstep_count (scr_gameref_t gs, scr_int npc)
{
  GS_CHECK_NPC (gs, npc);
  return gs->npcs[npc].walkstep_count;
}

void
gs_set_npc_walkstep (scr_gameref_t gs,
                     scr_int npc, scr_int walk, scr_int walkstep)
{
  GS_CHECK_WALK (gs, npc, walk);
  gs->npcs[npc].walksteps[walk] = walkstep;
}

scr_int
gs_npc_walkstep (scr_gameref_t gs, scr_int npc, scr_int walk)
{
  GS_CHECK_WALK (gs, npc, walk);
  return gs->npcs[npc].walksteps[walk];
}

void
gs_decrement_npc_walkstep (scr_gameref_t gs, scr_int npc, scr_int walk)
{
  GS_CHECK_WALK (gs, npc, walk);
  gs->npcs[npc].walksteps[walk]--;
}


/*
 * Convenience functions for bulk clearance of references.
 */
void
gs_clear_npc_references (scr_gameref_t gs)
{
  GS_CHECK (gs);
  std::fill (gs->npc_references.begin (), gs->npc_references.end (), FALSE);
}

void
gs_clear_object_references (scr_gameref_t gs)
{
  GS_CHECK (gs);
  std::fill (gs->object_references.begin (), gs->object_references.end (),
             FALSE);
}

void
gs_set_multiple_references (scr_gameref_t gs)
{
  GS_CHECK (gs);
  std::fill (gs->multiple_references.begin (), gs->multiple_references.end (),
             TRUE);
}

void
gs_clear_multiple_references (scr_gameref_t gs)
{
  GS_CHECK (gs);
  std::fill (gs->multiple_references.begin (), gs->multiple_references.end (),
             FALSE);
}


/*
 * gs_populate_dynamic_object()
 *
 * Place a non-static object where the TAF starts it, and seed its Runner
 * [2E] container field the way the Runner's loader does.
 */
static void
gs_populate_dynamic_object (scr_gameref_t game, scr_prop_setref_t bundle,
                            scr_int index_)
{
  scr_vartype_t vt_key[3];
  scr_int initialparent, initialposition;

  vt_key[0].string = "Objects";
  vt_key[1].integer = index_;

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
          /*
           * 0 is the player; an unset -1 is too.  run380 already
           * treats Parent -1 on a worn object as the player
           * (tra.taf), and 4.0 files use the same leftover -1
           * (nem.taf's starting shoes).  Parent 1+ is NPC n-1.
           */
          if (initialparent <= 0)
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

  /*
   * Mirror the Runner's loader: except for in/on placements (where
   * the movers above already pointed runner_parent at the translated
   * container object) and NPC-possessed objects (below), [2E] keeps
   * the raw .taf Parent value verbatim.  For in-room and
   * not-yet-anywhere objects that value is leftover authoring data,
   * and for player-held/worn ones it is the player selector, 0 --
   * either way the Runner's weigh routine happily matches it against
   * a container's object number, which is the phantom weight this
   * field exists to reproduce (goldilocks: the worn watch and dress
   * and the nowhere bottle, all Parent 0, phantom-weigh package
   * object 0, measured live in run400 2026-08-22).
   *
   * NPC-held/worn objects are the exception: their raw Parent is the
   * NPC selector (npc + 1), and the same live measurements show no
   * phantom from them -- goldilocks' suitcase (Parent 4) does not
   * weigh into the spoon (object 4), nor the crown (Parent 1) into
   * the toaster (object 1) -- so the Runner's loader evidently
   * clears [2E] on that path, as its give-to-NPC mover does.
   *
   * Player-HELD objects are cleared too: run400's object loader
   * (Proc_19_5, 4906A2-4906BA) writes &HFF over a held object's zero
   * Parent before 490709 adds it to the running totals; only the worn
   * path (4907A3) leaves the 0 standing.  riding_home shows it: the
   * held cane is object 0 and the held laptop case has Parent 0, so
   * with the raw seed the intro's cane-into-case move made each weigh
   * the other, and `take cane` out of the open case was refused as too
   * heavy where run400 takes it.
   */
  if (game->objects[index_].position == OBJ_HELD_PLAYER)
    gs_set_object_runner_parent (game, index_, -1);
  else if (game->objects[index_].position == OBJ_HELD_NPC
           || game->objects[index_].position == OBJ_WORN_NPC)
    {
      /*
       * An NPC-HELD object keeps the NPC index in [2E].  run400's
       * loader does not clear the field on this path: 490749 tests
       * position 0 with a raw Parent above 0, writes -200 and then
       * Parent - 1 (490760), and the weigh routine 447680 matches
       * that value against object numbers like any other.  Measured
       * on wonderland (2026-09-25): the Card Guard's rod, letter and
       * key (NPC 0) phantom-weigh the knife (object 0) and the Queen's
       * Staff of Hearts (81, NPC 3) phantom-weighs the rod (object 3),
       * so `get knife` in an empty-handed start is 94 > 90, "The
       * ethereal knife is too heavy for you to carry at the moment."
       * The goldilocks measurements above tested the wrong targets
       * (Parent 4 lands on object 3, not 4), so they never saw it.
       *
       * NPC-WORN objects get the same seed: the worn branch has the
       * same Parent - 1 rewrite (490780/490797), and probes against
       * run400 (p4WORNNPC*, 2026-09-25) refuse `take coin` whenever
       * an NPC wears an 81-weight cloak with Parent 1 -- whether or
       * not the NPC is present, the cloak is wearable, or a built-in
       * give/wear has run in between.  goldilocks' crown (worn by
       * NPC 0, weight 9) does phantom-weigh the package (object 0)
       * too: with the package placed in the start room, `take
       * package` costs 47 in run400 against the seeded-held-only 38.
       * The earlier "does not weigh" reading was two offsetting
       * errors: the crown was missing, and the bottle (hidden, raw
       * Parent 0, weight 9) was still counted after task 54 `water
       * bean` had moved it to the garden, which in run400 rewrites
       * its container field (see task_move_object).
       */
      gs_set_object_runner_parent (game, index_,
                                   initialparent > 0
                                     ? initialparent - 1 : -1);
    }
  else if (game->objects[index_].position != OBJ_IN_OBJECT
           && game->objects[index_].position != OBJ_ON_OBJECT)
    gs_set_object_runner_parent (game, index_, initialparent);
}


/*
 * gs_populate_objects()
 *
 * Create the objects state array and set each object's starting state.
 */
static void
gs_populate_objects (scr_gameref_t game, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[4];
  scr_int index_;

  /* Create objects state array. */
  vt_key[0].string = "Objects";
  game->object_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->objects.resize (game->object_count);

  /*
   * The NPCs array is not built until further below, but the object setup
   * that follows needs the NPC count to validate objects whose initial state
   * is "held/worn by NPC" (some games name a nonexistent NPC); read it now.
   */
  vt_key[0].string = "NPCs";
  game->npc_count = prop_get_child_count (bundle, "I<-s", vt_key);
  vt_key[0].string = "Objects";

  /* Set up initial object states. */
  for (index_ = 0; index_ < game->object_count; index_++)
    {
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

              /*
               * Parent 0 is the player; a nonexistent NPC reads as 0 too,
               * as obj_static_in_room() does.
               */
              vt_key[2].string = "Parent";
              parent = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
              if (parent < 0 || parent >= game->npc_count)
                parent = -1;
              game->objects[index_].parent = parent;
              game->objects[index_].runner_parent = -1;
            }
          else
            {
              gs_object_make_hidden_unchecked (game, index_);
              /*
               * Statics never carry Runner [2E] data; the mover above no
               * longer clears unconditionally, so do it here (the vector
               * resize zero-fill would falsely match object 0).
               */
              game->objects[index_].runner_parent = -1;
            }
        }
      else
        gs_populate_dynamic_object (game, bundle, index_);

      vt_key[2].string = "CurrentState";
      gs_set_object_state (game, index_,
                           prop_get_integer (bundle, "I<-sis", vt_key));

      vt_key[2].string = "Openable";
      gs_set_object_openness (game, index_,
                              prop_get_integer (bundle, "I<-sis", vt_key));

      /*
       * openadv seeds the seen byte straight from the location field [26] it
       * has just built (@004909B5): it clears the byte, then sets it when the
       * location is 0 (held by the player) or &H9C (worn by the player).
       *
       * That location field is built from #InitialPosition, and from nothing
       * else -- @00490255 reads it for EVERY object, static or not, right
       * after $Description, and @00490268 maps it: `location = InitialPosition
       * - 1`, then 1 -> &HF6 (in object), 2 -> &HEC (on object), and anything
       * above 2 loses a further 2 to land on `room + 1`.  So location 0 means
       * InitialPosition 1, "held by the player", and nothing else.  The worn
       * remap to &H9C is @004907A3, INSIDE the `If Static = 0` block that runs
       * from @0049059C to @004907C0, so a static can never carry it either.
       *
       * A static's Where/Type never reaches [26]: the static branch
       * (@0049031A) writes only the per-room presence array [28].  A static
       * therefore starts seen only if its #InitialPosition is 1 -- and no
       * object in the 350-game corpus has that, statics being written with
       * #InitialPosition 0 (or, in a few hundred cases, the room they sit in,
       * 4 + room), so in practice **no static ever starts seen**.
       *
       * This corrects the reading this port shipped with on 2026-08-24, which
       * had the static branch's Where/Type reaching the dynamic mapping and so
       * marked every ONE_ROOM static seen at load.  Measured live: `x cauldron`
       * on turn 2 of asdfa (runner_probes/asdfa.run400.txt), `x desk` in CBN
       * (runner_probes/cbn.run400.txt), `x dust` in The Cellar
       * (runner_probes/cellar.run400.txt) -- all three ONE_ROOM statics the
       * player has not been shown, all three answered "You see no such thing."
       * by run400 where Scarier reached its second, seen-object pass and said
       * "You can't see the <X> from here!".
       */
      if (is_static)
        {
          vt_key[2].string = "InitialPosition";
          gs_set_object_seen (game, index_,
                              prop_get_integer (bundle, "I<-sis", vt_key)
                                == 1);
        }
      else
        {
          const scr_int position = gs_object_position (game, index_);

          gs_set_object_seen (game, index_,
                              position == OBJ_HELD_PLAYER
                              || position == OBJ_WORN_PLAYER);
        }

      /*
       * The Runner keeps the authored OnlyWhenNotMoved byte whatever the
       * in-room description says -- mode 1 with an EMPTY description is a
       * real and used combination, and it silences the object entirely
       * (camelot15's four bottles).  Only mode 1 is ever spent, so that is
       * all this flag tracks.
       */
      vt_key[2].string = "OnlyWhenNotMoved";
      unmoved = prop_get_integer (bundle, "I<-sis", vt_key) == 1;
      gs_set_object_unmoved (game, index_, unmoved);
      gs_set_object_static_unmoved (game, index_, TRUE);
    }

  /*
   * An object whose #InitialPosition puts it inside or on itself, directly
   * or round a loop of containers, sends every containment walk into
   * unbounded recursion on the first turn.  The movers above only check the
   * container index, so look for the loop here: a chain longer than the
   * object count must revisit an object.
   */
  for (index_ = 0; index_ < game->object_count; index_++)
    {
      scr_int object = index_, steps = 0;

      while (game->objects[object].position == OBJ_IN_OBJECT
             || game->objects[object].position == OBJ_ON_OBJECT)
        {
          if (++steps > game->object_count)
            scr_fatal ("gs_create: object %ld is inside itself\n", index_);
          object = game->objects[object].parent;
        }
    }
}


/*
 * gs_populate_events()
 *
 * Create the events state array and set each event's starting state.
 */
static void
gs_populate_events (scr_gameref_t game, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  scr_int index_;

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
      game->events[index_].loadtime = -1;
      game->events[index_].taskstate = 0;
      game->events[index_].ticked = 0;

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

            /*
             * In Runner-compatible RNG mode the roll is made once, in the
             * Runner's load order, by run_runner_load_draws(); a state
             * created here is one of three (game, temporary, undo), and the
             * Runner rolls only for the one it plays.
             */
            if (scr_is_runner_random ())
              gs_set_event_time (game, index_, 0);
            else
              gs_set_event_time (game, index_,
                                 scr_randomint_exclusive (start, end));
            break;
          }

        case 3:
          gs_set_event_state (game, index_, ES_AWAITING);
          gs_set_event_time (game, index_, 0);
          break;
        }
    }
}


/*
 * gs_populate_npcs()
 *
 * Create the NPCs state array and set each NPC's starting state.
 */
static void
gs_populate_npcs (scr_gameref_t game, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  scr_int index_;

  /* Create NPCs state array (npc_count was read before the objects). */
  vt_key[0].string = "NPCs";
  game->npcs.resize (game->npc_count);

  /* Set up initial NPCs states. */
  for (index_ = 0; index_ < game->npc_count; index_++)
    {
      scr_int walk, walkstep_count, startroom;

      gs_set_npc_position (game, index_, 0);
      gs_set_npc_parent (game, index_, -1);
      gs_set_npc_seen (game, index_, FALSE);
      game->npcs[index_].stamina = 0;
      game->npcs[index_].staminacounter = 0;
      game->npcs[index_].attackcounter = 0;
      game->npcs[index_].battle = {};

      vt_key[1].integer = index_;

      /* StartRoom 0 is nowhere, n room n-1; a nonexistent room is nowhere. */
      vt_key[2].string = "StartRoom";
      startroom = prop_get_integer (bundle, "I<-sis", vt_key);
      if (startroom < 0 || startroom > game->room_count)
        startroom = 0;
      gs_set_npc_location (game, index_, startroom);

      vt_key[2].string = "Walks";
      walkstep_count = prop_get_child_count (bundle, "I<-sis", vt_key);

      game->npcs[index_].walkstep_count = walkstep_count;
      game->npcs[index_].walksteps.resize (walkstep_count);

      for (walk = 0; walk < walkstep_count; walk++)
        gs_set_npc_walkstep (game, index_, walk, 0);
    }
}


/*
 * gs_populate_player()
 *
 * Set the player's starting room, position, and battle state.
 */
static void
gs_populate_player (scr_gameref_t game, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[2];

  /* Set up the player portions of the game state. */
  vt_key[0].string = "Header";
  vt_key[1].string = "StartRoom";
  game->playerroom = prop_get_integer (bundle, "I<-ss", vt_key);

  /* The start room indexes the rooms array from the first turn on. */
  if (game->playerroom < 0 || game->playerroom >= game->room_count)
    scr_fatal ("gs_create: start room out of range, %ld\n",
               game->playerroom);
  vt_key[0].string = "Globals";
  vt_key[1].string = "Position";
  game->playerposition = prop_get_integer (bundle, "I<-ss", vt_key);
  vt_key[1].string = "ParentObject";
  {
    /*
     * ParentObject is not an object index.  It is the 1-based ordinal the
     * Generator's position combo holds -- the n-th standable object for a
     * standing or sitting start, the n-th lieable object for a lying one,
     * 0 for the floor -- exactly the encoding the "Move player to standing/
     * sitting/lying on" task action uses (sctasks_move.cpp
     * task_run_move_player_action), and the restriction "Player must be
     * sitting on" tests.  We
     * used to read it as a raw 1-based object index, so CIBASS.taf (lying,
     * ParentObject 1) started the player on object 0, the window, and
     * `stand` answered "I stand up from the window."  run400 answers "I
     * stand up from the bed." (measured 2026-08-29): the bed is the first
     * lieable object.  Fugitive.taf (lying, 1) is the same shape and its
     * golden used to read "You stand up from your watch."; 3monkeys.taf
     * (sitting, 4) has "sheet" as object 3 but "bed" as the 4th standable.
     */
    scr_int ordinal = prop_get_integer (bundle, "I<-ss", vt_key);

    if (ordinal <= 0)
      game->playerparent = -1;
    else if (game->playerposition == 2)
      game->playerparent = obj_lieable_object (game, ordinal - 1);
    else
      game->playerparent = obj_standable_object (game, ordinal - 1);
  }
  game->playerstamina = 0;
  game->playerstaminacounter = 0;
  game->playerwield = -1;
  game->playerbattle = {};
}


/*
 * gs_populate_seen_sweep()
 *
 * Stamp the objects the Runner's afteroa reveals in the start room.
 */
static void
gs_populate_seen_sweep (scr_gameref_t game, scr_prop_setref_t bundle)
{
  /*
   * afteroa's start-room seen sweep.
   *
   * gs_populate_objects() stamps the seen byte for held and worn objects only,
   * so on its own it leaves every static unseen -- and that cannot be the whole
   * rule, because TenebraeSemper.taf (4.00, DispFirstRoom off, so tstart
   * @0044D68F never calls viewroom and no lister has run) answers `open desk`
   * on turn ONE with "You open your desk.  The pens are inside your desk.",
   * although the desk is a Static in the start room with #InitialPosition 0.
   * The noun resolver co() (@0046486C) needs the byte -- @00464372 ANDs it
   * into the match -- so something must set it before the first prompt.
   *
   * That something is afteroa, the routine the Runner runs between openadv
   * and tstart.  run400's is at 0046F0B4, and its second loop (@0046EDA5 to
   * @0046EDE6) walks the whole object table and sets the seen byte
   * (@0046EDCE) on every object for which Proc_21_53_44B578 (@0044B578) is
   * true.  That predicate is exactly this port's obj_indirectly_in_room()
   * against the player's room: obhere() first, then the object is visible if
   * it is a static that is present (@0044B4CD), or a dynamic held or worn by
   * the player or an NPC or lying in the room (@0044B4D8-@0044B508), or on
   * another visible object (@0044B514), or inside one whose openness is below
   * 6 (@0044B534).  Nothing else in the Runner reveals an object at load.
   *
   * run390's afteroa (@00441A54) has the same sweep but a much narrower
   * predicate, inline at @004418FF-@0044193C: static flag set AND the
   * presence array covers the player room AND isdark(playerroom) = 0.
   * Dynamics are not touched there, and neither Runner sweeps before 3.90 --
   * co() does not read the byte at all in run370 (@004261B4) or run380
   * (@0042DE60) -- so the sweep is gated at 3.90 and narrowed to statics
   * below 4.00.  The isdark term is lib_room_alt_darkens(): afteroa calls
   * isdark() itself (@0044192D), the bare condition ladder at @00433920, so
   * unlike every other darkness site it carries no HideObjects term -- a room
   * whose object alt fires without ticking "Hide objects" still starts its
   * statics unstamped.  A dark start room therefore begins with nothing
   * referenceable, which is the same rule lib_print_room_description() now
   * applies on every later look.
   *
   * The reading this port shipped with on 2026-08-24 had a static whose
   * Where/Type was ONE_ROOM starting seen, which is the same answer as this
   * sweep for a static in the start room but a wrong one for a ONE_ROOM
   * static anywhere else.  That is the asdfa/CBN/Cellar divergence:
   * `x cauldron` (runner_probes/asdfa.run400.txt), `x desk`
   * (runner_probes/cbn.run400.txt) and `x dust`
   * (runner_probes/cellar.run400.txt) are all ONE_ROOM statics of some
   * *other* room, all answered "You see no such thing." by run400 where
   * Scarier had reached its second, seen-object pass and said "You can't see
   * the <X> from here!".
   */
  const scr_int taf_version = prop_get_taf_version (bundle);
  scr_int index_;

  if (taf_version >= TAF_VERSION_390)
    {
      for (index_ = 0; index_ < game->object_count; index_++)
        {
          if (gs_object_seen (game, index_))
            continue;

          if (taf_version < TAF_VERSION_400
              && (!obj_is_static (game, index_)
                  || lib_room_alt_darkens (game, game->playerroom)))
            continue;

          if (obj_indirectly_in_room (game, index_, game->playerroom))
            gs_set_object_seen (game, index_, TRUE);
        }
    }
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
  game->memento = nullptr;

  /* Initialize for no debugger. */
  game->debugger = nullptr;

  /* Initialize the undo buffers to NULL for now. */
  game->temporary = nullptr;
  game->undo = nullptr;
  game->undo_available = FALSE;

  /*
   * Carried-load tracking off until seeded below; default to Runner-faithful
   * running totals rather than the legacy per-check recompute, unless the
   * capacity assist default asks for the recompute (see gs_capacity_assist).
   */
  game->carried_weight = 0;
  game->carried_size = 0;
  game->carried_ready = FALSE;
  game->carried_suspend = FALSE;
  game->capacity_recompute = gs_capacity_assist;
  game->runner_phantom_held = TRUE;

  /* Create rooms state array. */
  vt_key[0].string = "Rooms";
  game->room_count = prop_get_child_count (bundle, "I<-s", vt_key);
  game->rooms.resize (game->room_count);

  /* Set up initial rooms states. */
  for (index_ = 0; index_ < game->room_count; index_++)
    gs_set_room_seen (game, index_, FALSE);

  gs_populate_objects (game, bundle);

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

  gs_populate_events (game, bundle);

  gs_populate_npcs (game, bundle);

  gs_populate_player (game, bundle);

  /*
   * Score-change notifications start off.  The TAF does carry a NoScoreNotify
   * global, but no Runner reads it: "(Your score has increased by N)" exists
   * only in run400.exe -- run370/380/390 have no such string at all -- and
   * run400 gates it on Options -> High Scores/Scoring -> "Notify when score
   * changes", a persisted *user* preference read at startup as
   * GetSetting("ADRIFT", "Runner", "NotifyScore", CStr(False)) and saved back
   * by notify_Click.  A fresh installation therefore never notifies, which is
   * the state to match here; the `notify` command stands in for the menu item.
   * Verified live in run400 and run390 (make_arena_probe.py config SC and
   * make_39_endprobe.py both score +3 on an ordinary turn, in silence).  See
   * test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.
   */
  game->notify_score_change = FALSE;

  /* Miscellaneous state defaults. */
  game->turns = 0;
  game->score = 0;
  game->bold_room_names = TRUE;
  game->verbose = TRUE;

  /*
   * The owning status strings default-construct to NULL via value-init
   * above.
   */

  /* Resource controls. */
  res_clear_resource (&game->requested_sound);
  res_clear_resource (&game->requested_graphic);
  res_clear_resource (&game->playing_sound);
  res_clear_resource (&game->displayed_graphic);
  game->stop_sound = FALSE;
  game->sound_active = FALSE;

  /*
   * Initialize wait turns from game properties.  A negative count would set
   * a wait counter the main loop never brings back to zero, so it would run
   * turns forever without reading input; treat it as no wait.
   */
  game->waitturns = prop_get_global_integer (bundle, "WaitTurns");
  if (game->waitturns < 0)
    game->waitturns = 0;

  /* Non-game conveniences. */
  game->is_running = FALSE;
  game->has_notified = FALSE;
  game->is_admin = FALSE;
  game->player_moved_by_command = FALSE;
  game->has_completed = FALSE;
  game->pending_endgame = 0;
  game->waitcounter = 0;
  game->do_again = FALSE;
  game->redo_sequence = 0;
  game->do_restart = FALSE;
  game->do_restore = FALSE;

  game->object_references.assign (game->object_count, FALSE);
  game->multiple_references.assign (game->object_count, FALSE);
  game->npc_references.assign (game->npc_count, FALSE);

  game->it_object = -1;
  game->it_form = UIP_IT_INDEFINITE;
  game->him_npc = -1;
  game->her_npc = -1;
  game->it_npc = -1;
  game->last_npc = -1;

  /*
   * Seed the carried-load totals from the starting inventory.  run400 zeroes
   * globals 36/38 at new-game start (Proc_19_4's tail, loc_45AA71-45AA7E),
   * but the adventure-file object loader (Proc_19_5, loc_490709-4907BB) then
   * adds base size and weight for each object the player starts out holding
   * and base weight only for each object worn, the same base-only seeding a
   * restore performs -- measured live in run400 (goldilocks, 2026-08-22):
   * `count` on turn zero reports the worn wristwatch+dress weight (10) with
   * size 0.  gs_carried_recompute() implements exactly that rule, and also
   * arms gs_carried_track() for subsequent moves.
   */
  gs_carried_recompute (game);

  gs_populate_seen_sweep (game, bundle);
}

scr_gameref_t
gs_create (scr_var_setref_t vars,
           scr_prop_setref_t bundle, scr_filterref_t filter)
{
  scr_gameref_t game;

  assert (vars && bundle && filter);

  /*
   * Create the initial state structure.  sc_game_s owns std::unique_ptr
   * status strings (P3 RAII), so it is non-POD and must be new()'d --
   * value-init zeroes the POD fields (all of which gs_populate sets
   * explicitly anyway) and default-constructs the owning strings to NULL.
   */
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
  return gs_game_ok (game);
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
  /*
   * Copy from_string if set, otherwise set to_string to NULL.  Assigning the
   * unique_ptr frees any current contents.
   */
  if (from_string)
    to_string.reset (scr_strdup (from_string));
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
  scr_int var_count, var;

  GS_CHECK (to);
  GS_CHECK (from);

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

      name = var_indexed_name (bundle, var);
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

  /* Copy over NPC states, walks included. */
  assert (to->npc_count == from->npc_count);
  to->npcs = from->npcs;

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
  to->player_moved_by_command = from->player_moved_by_command;
  to->has_completed = from->has_completed;
  to->pending_endgame = from->pending_endgame;

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
  to->it_form = from->it_form;
  to->him_npc = from->him_npc;
  to->her_npc = from->her_npc;
  to->it_npc = from->it_npc;
  to->last_npc = from->last_npc;

  /*
   * Carry the running totals over verbatim: run400's undo snapshot restores
   * its carried-load globals from the snapshot values (Proc_19_62,
   * loc_45AEA4-45AEB7) rather than reweighing, so accumulated accounting
   * quirks survive an undo.  A restored SAVE is different -- the .tas
   * loader reseeds base-only sums -- and the restore path handles that by
   * calling gs_carried_recompute() on the freshly loaded game before it is
   * copied here.  The capacity_recompute mode flag is a session preference,
   * so (like verbose) it is left invariant across copies and deliberately
   * not propagated here.
   */
  to->carried_weight = from->carried_weight;
  to->carried_size = from->carried_size;
  to->carried_ready = from->carried_ready;

  /*
   * Undo carries the phantom along.  A restore copies in the freshly loaded
   * game's, so a restored save brings back a still-held one -- unmeasured.
   */
  to->runner_phantom_held = from->runner_phantom_held;
}


/*
 * gs_destroy()
 *
 * Free all the memory associated with a game state.
 */
void
gs_destroy (scr_gameref_t game)
{
  GS_CHECK (game);

  /*
   * Drop the parser's cached %object%/%character% candidates and the task,
   * serializer, event, command-matcher and map property caches for this game,
   * so a later game allocated at the same address can't inherit them.
   */
  uip_forget_game (game);
  task_forget_game (game);
  ser_forget_game (game);
  evt_forget_game (game);
  run_forget_game (game);
  scmap_forget_game (game);

  /*
   * The state arrays (rooms, objects, tasks, events, npcs and their walksteps,
   * the *_references) are std::vector and the owning game strings
   * (current_room_name, status_line, title, author, hint_text) are
   * scr_owned_string; all free themselves when the game is delete'd below,
   * so there is nothing to free by hand here.
   */

  /*
   * Free the game state itself; its destructor releases the owned strings.
   */
  delete game;
}

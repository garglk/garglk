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

#include "scarier.h"
#include "scprotos.h"

#ifndef SCARIER_GAMESTATE_H
#define SCARIER_GAMESTATE_H

#include <vector>

/* Room state structure, tracks rooms visited by the player. */
typedef struct scr_roomstate_s
{
  scr_bool visited;
} scr_roomstate_t;

/*
 * Object state structure, tracks object movement, position, parent, openness
 * for openable objects, state for stateful objects, and whether seen or not
 * by the player.  The enumerations are values assigned to position when the
 * object is other than just "in a room"; otherwise position contains the
 * room number + 1.
 */
enum
{ OBJ_HIDDEN = -1,
  OBJ_HELD_PLAYER = 0, OBJ_HELD_NPC = -200,
  OBJ_WORN_PLAYER = -100, OBJ_WORN_NPC = -300,
  OBJ_PART_PLAYER = -30, OBJ_PART_NPC = -30,
  OBJ_ON_OBJECT = -20, OBJ_IN_OBJECT = -10
};
typedef struct scr_objectstate_s
{
  scr_int position;
  scr_int parent;
  scr_int openness;
  scr_int state;
  scr_bool seen;
  scr_bool unmoved;
  scr_bool static_unmoved;
} scr_objectstate_t;

/* Task state structure, tracks task done, and if task scored. */
typedef struct scr_taskstate_s
{
  scr_bool done;
  scr_bool scored;
} scr_taskstate_t;

/* Event state structure, holds event state, and timing information. */
enum
{ ES_WAITING = 1,
  ES_RUNNING = 2, ES_AWAITING = 3, ES_FINISHED = 4, ES_PAUSED = 5
};
typedef struct scr_eventstate_s
{
  scr_int state;
  scr_int time;
} scr_eventstate_t;

/*
 * Mutable battle attributes.  Slots for the four ranged combat attributes,
 * each carrying a current [lo,hi] roll range and a maximum cap.
 */
enum
{ BATTLE_STRENGTH = 0, BATTLE_ACCURACY = 1,
  BATTLE_DEFENSE = 2, BATTLE_AGILITY = 3, BATTLE_ATTR_COUNT = 4
};

/*
 * Per-character mutable Battle System attributes.  Seeded from the game bundle
 * by battle_start(), then alterable at runtime by "Change battle attribute"
 * task actions (type 7).  Attitude and speed apply to NPCs only.  Live current
 * stamina and the recovery/attack counters are held in the fields above and in
 * the player block, not here.
 */
typedef struct scr_battle_s
{
  scr_bool seeded;
  scr_int attitude;                     /* 0 = neutral, 1 = ally, 2 = enemy. */
  scr_int maxstamina;
  scr_int speed;                        /* 0..4 attack cadence. */
  scr_int lo[BATTLE_ATTR_COUNT];
  scr_int hi[BATTLE_ATTR_COUNT];
  scr_int max[BATTLE_ATTR_COUNT];
} scr_battle_t;

/*
 * NPC state structure, tracks the NPC location and position, any parent
 * object, whether the NPC seen, and if the NPC walks, the count of walk
 * steps and a steps array sized to this count.
 */
typedef struct scr_npcstate_s
{
  scr_int location;
  scr_int position;
  scr_int parent;
  scr_int walkstep_count;
  std::vector<scr_int> walksteps;
  scr_bool seen;

  /* Battle system state -- current stamina, recovery and attack counters. */
  scr_int stamina;
  scr_int staminacounter;
  scr_int attackcounter;

  /* Mutable battle attributes (attitude, ranges, max stamina, speed). */
  scr_battle_t battle;
} scr_npcstate_t;

/*
 * Resource tracking structure, holds the resource name, including any
 * trailing "##" for looping sounds, its offset into the game file, and its
 * length.  Two resources are held -- active, and requested.  The game main
 * loop compares the two, and notifies the interface on a change.
 */
typedef struct scr_resource_s
{
  const scr_char *name;
  scr_int offset;
  scr_int length;
} scr_resource_t;

/*
 * Overall game state structure.  Arrays are malloc'ed for the appropriate
 * number of each of the above state structures.
 */
typedef struct scr_game_s
{
  scr_uint magic;

  /* References to assorted helper subsystems. */
  scr_var_setref_t vars;
  scr_prop_setref_t bundle;
  scr_filterref_t filter;
  scr_memo_setref_t memento;
  scr_debuggerref_t debugger;

  /* Undo information, also used by the debugger. */
  struct scr_game_s *temporary;
  struct scr_game_s *undo;
  scr_bool undo_available;

  /* Basic game state -- rooms, objects, and so on.  The state arrays are
   * std::vector (P3 RAII): they free themselves when the game is delete'd, so a
   * throw partway through gs_create (a prop_* scr_fatal) no longer leaks the
   * already-allocated arrays.  The *_count fields remain the iteration source
   * of truth (matching every existing loop, the serializer, and the asserts);
   * the vectors are sized to match. */
  scr_int room_count;
  std::vector<scr_roomstate_t> rooms;
  scr_int object_count;
  std::vector<scr_objectstate_t> objects;
  scr_int task_count;
  std::vector<scr_taskstate_t> tasks;
  scr_int event_count;
  std::vector<scr_eventstate_t> events;
  scr_int npc_count;
  std::vector<scr_npcstate_t> npcs;
  scr_int playerroom;
  scr_int playerposition;
  scr_int playerparent;

  /* Battle system state for the player -- current stamina, recovery, and the
   * object index of the weapon the player has wielded (-1 for none). */
  scr_int playerstamina;
  scr_int playerstaminacounter;
  scr_int playerwield;

  /* Mutable battle attributes for the player (no attitude/speed). */
  scr_battle_t playerbattle;

  scr_int turns;
  scr_int score;
  scr_bool bold_room_names;
  scr_bool verbose;
  scr_bool notify_score_change;
  /*
   * Owning game-status strings, freed at scope exit (P3 RAII).  scr_owned_string
   * preserves the engine's char* contract exactly — .get() hands the raw pointer
   * to readers and NULL stays NULL (no empty-vs-unset confusion) — while a
   * scr_fatal_error / run_loop_halt thrown mid-turn (e.g. from a pf_filter ->
   * prop_* deep in run_update_status) no longer leaks the buffer being replaced.
   * Having non-trivial members makes scr_game_s non-POD, so gs_create new()s it
   * and gs_destroy delete()s it (the old scr_malloc + 0xaa poison is gone).
   */
  scr_owned_string current_room_name;
  scr_owned_string status_line;
  scr_owned_string title;
  scr_owned_string author;
  scr_owned_string hint_text;

  /* Resource management data. */
  scr_resource_t requested_sound;
  scr_resource_t requested_graphic;
  scr_bool stop_sound;
  scr_bool sound_active;

  scr_resource_t playing_sound;
  scr_resource_t displayed_graphic;

  /* Game running and game completed flags. */
  scr_bool is_running;
  scr_bool has_completed;

  /* Player's setting for waitturns; overrides the game's. */
  scr_int waitturns;

  /* ADRIFT-style carried-load running totals.  The real Runner keeps the
   * player's carried weight and size as running totals, updated incrementally
   * on each take/drop (so taking a container and then removing its contents
   * double-counts those contents); it recomputes them only when loading state.
   * These mirror that: maintained incrementally during play, recomputed at
   * game create/copy/restore.  Derived state -- not part of the saved stream
   * (the Runner stores live totals in the save but recomputes on load).
   * carried_ready gates the incremental tracker off during game setup. */
  scr_int carried_weight;
  scr_int carried_size;
  scr_bool carried_ready;

  /* When TRUE, the capacity checks recompute the carried load from currently
   * held objects each time (legacy SCARIER behaviour, which avoids the Runner's
   * double-count); when FALSE (default) they consult the running totals above,
   * matching the real Runner.  Toggled with the "capacity" metacommand. */
  scr_bool capacity_recompute;

  /* Miscellaneous library and main loop conveniences. */
  scr_int waitcounter;
  scr_bool has_notified;
  scr_bool is_admin;
  scr_bool do_again;
  scr_int redo_sequence;
  scr_bool do_restart;
  scr_bool do_restore;
  std::vector<scr_bool> object_references;
  std::vector<scr_bool> multiple_references;
  std::vector<scr_bool> npc_references;
  scr_int it_object;
  scr_int him_npc;
  scr_int her_npc;
  scr_int it_npc;
} scr_game_t;

#endif

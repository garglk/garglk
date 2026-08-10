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
 * o Implements task return FALSE on no output, a slight extension of
 *   current jAsea behavior.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/*
 * Tasks can run other tasks, leading to the possibility of an infinite loop
 * in the task calling sequence.  It's a game error, and we'll apply a limit
 * to the task recursion depth to try and catch it more controllably than
 * waiting for memory exhaustion.
 */
enum { TASK_MAXIMUM_RECURSION = 128 };

/* Trace flag, set before running. */
static scr_bool task_trace = FALSE;

/*
 * Optional "move assist" mode (opt-in, off by default; sibling of the Battle
 * System's combat assist).  An ADRIFT 4.0 move task action stores its
 * destination type as a combo-box ListIndex; VB leaves an untouched combo at
 * -1.  A handful of native-4.0 games (e.g. To Hell & Beyond) were authored --
 * most likely upgraded from 3.9 in ADRIFT's own Generator -- with that combo
 * left at -1 on "move to room" actions, the room sitting unused in Var3.  The
 * reference Runner (run400.exe) silently ignores such a move (its Select Case
 * has no matching/else branch), so the faithful default here is also a no-op
 * (verified against run400.exe).  In some games those unset moves are redundant
 * (X-Files, HYPER Battle System still win); in others they sit on the critical
 * path, leaving the game unwinnable (To Hell & Beyond traps the player in
 * the mansion).  With move assist on, an unset (-1) move whose Var3 names a real
 * room is honoured as "to room", letting such games be completed.  Strictly
 * opt-in, as it deliberately diverges from the reference Runner.
 */
static scr_bool task_move_assist = FALSE;

void
task_set_move_assist (scr_bool flag)
{
  task_move_assist = flag;
}

scr_bool
task_get_move_assist (void)
{
  return task_move_assist;
}


/*
 * task_get_hint_common()
 * task_get_hint_question()
 * task_get_hint_subtle()
 * task_get_hint_unsubtle()
 * task_has_hints()
 *
 * Return the assorted hint text strings, and TRUE if the given task offers
 * hints.
 */
static const scr_char *
task_get_hint_common (scr_gameref_t game, scr_int task, const scr_char *hint)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *retval;

  /* Look up and return the requested hint string. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = hint;
  retval = prop_get_string (bundle, "S<-sis", vt_key);
  return retval;
}

const scr_char *
task_get_hint_question (scr_gameref_t game, scr_int task)
{
  return task_get_hint_common (game, task, "Question");
}

const scr_char *
task_get_hint_subtle (scr_gameref_t game, scr_int task)
{
  return task_get_hint_common (game, task, "Hint1");
}

const scr_char *
task_get_hint_unsubtle (scr_gameref_t game, scr_int task)
{
  return task_get_hint_common (game, task, "Hint2");
}

scr_bool
task_has_hints (scr_gameref_t game, scr_int task)
{
  /* A non-empty question implies hints available. */
  return !scr_strempty (task_get_hint_question (game, task));
}


/*
 * Cached per-task runnability properties.
 *
 * task_can_run_task_directional() is called for every task on every command
 * match and every turn tick, and before caching it re-read the same fixed
 * task properties from the bundle each time, which profiled as the largest
 * remaining prop_get() consumer.  The properties are immutable once a game
 * is loaded, so remember each answer the first time it is read.  Caching is
 * per field and lazy -- the first touch of each field goes through the same
 * fatal-checking prop_get_* wrapper the uncached code used, so a malformed
 * game fails in exactly the same way, and a field the uncached code never
 * read is never read here either.
 *
 * The cache tracks a single game; gs_destroy() calls task_forget_game() so
 * a later game reusing the same allocation address starts clean.
 */
enum
{ TASK_CACHE_UNKNOWN = 0, TASK_CACHE_FALSE = 1, TASK_CACHE_TRUE = 2 };
enum { TASK_CACHE_NO_ROOM = -2 };

typedef struct
{
  scr_byte repeatable;           /* tri-state, TASK_CACHE_* */
  scr_byte repeattext_empty;     /* tri-state: is RepeatText empty? */
  scr_byte reversible;           /* tri-state */
  scr_int where_type;            /* Where.Type + 1, 0 = unknown */
  scr_int where_room;            /* Where.Room, TASK_CACHE_NO_ROOM = unknown */
  std::vector<scr_byte> where_rooms;  /* per-room tri-state, sized lazily */
} scr_task_props_t;

static const void *task_cache_game = NULL;
static std::vector<scr_task_props_t> task_cache;

/*
 * task_cache_entry()
 *
 * Return the cache entry for a task, resetting the cache if it was built
 * for a different game.
 */
static scr_task_props_t *
task_cache_entry (scr_gameref_t game, scr_int task)
{
  if (task_cache_game != game)
    {
      scr_task_props_t initial;

      initial.repeatable = TASK_CACHE_UNKNOWN;
      initial.repeattext_empty = TASK_CACHE_UNKNOWN;
      initial.reversible = TASK_CACHE_UNKNOWN;
      initial.where_type = 0;
      initial.where_room = TASK_CACHE_NO_ROOM;

      task_cache.assign (gs_task_count (game), initial);
      task_cache_game = game;
    }
  return &task_cache[task];
}

/*
 * task_forget_game()
 *
 * Drop any task property cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
task_forget_game (const void *game)
{
  if (task_cache_game == game)
    {
      task_cache_game = NULL;
      task_cache.clear ();
    }
}

/*
 * task_state_allows_run()
 *
 * The half of the runnability test that has nothing to do with where the
 * player is standing: a completed task is not re-runnable forwards unless it
 * is repeatable or has repeat text to print, and a task is only runnable in
 * reverse if it is reversible.
 *
 * Split out from task_can_run_task_directional() so that the two halves can be
 * asked separately -- see task_is_room_refused() below.
 */
static scr_bool
task_is_repeatable (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_task_props_t *cached;

  cached = task_cache_entry (game, task);
  if (cached->repeatable == TASK_CACHE_UNKNOWN)
    {
      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = "Repeatable";
      cached->repeatable = prop_get_boolean (bundle, "B<-sis", vt_key)
                           ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
    }
  return cached->repeatable == TASK_CACHE_TRUE;
}


static scr_bool
task_repeattext_is_empty (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_task_props_t *cached;

  cached = task_cache_entry (game, task);
  if (cached->repeattext_empty == TASK_CACHE_UNKNOWN)
    {
      const scr_char *repeattext;

      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = "RepeatText";
      repeattext = prop_get_string (bundle, "S<-sis", vt_key);
      cached->repeattext_empty = scr_strempty (repeattext)
                                 ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
    }
  return cached->repeattext_empty == TASK_CACHE_TRUE;
}


static scr_bool
task_state_allows_run (scr_gameref_t game, scr_int task, scr_bool forwards)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_task_props_t *cached;

  cached = task_cache_entry (game, task);

  /* If already run, non-repeatable tasks are not re-runnable forwards. */
  if (forwards && gs_task_done (game, task))
    {
      if (!task_is_repeatable (game, task))
        return FALSE;

      if (!task_repeattext_is_empty (game, task))
        return FALSE;
    }

  /* If checking for reverse, test the reversibility flag. */
  if (!forwards)
    {
      if (cached->reversible == TASK_CACHE_UNKNOWN)
        {
          vt_key[0].string = "Tasks";
          vt_key[1].integer = task;
          vt_key[2].string = "Reversible";
          cached->reversible = prop_get_boolean (bundle, "B<-sis", vt_key)
                               ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
        }
      if (cached->reversible == TASK_CACHE_FALSE)
        return FALSE;
    }

  return TRUE;
}


/*
 * task_where_allows_run()
 *
 * The other half: TRUE if the player is standing in a room the task's Where
 * room list covers.
 */
static scr_bool
task_where_allows_run (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int type;
  scr_task_props_t *cached;

  cached = task_cache_entry (game, task);

  /* Check room list for the task and return it. */
  if (cached->where_type == 0)
    {
      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = "Where";
      vt_key[3].string = "Type";
      cached->where_type = prop_get_integer (bundle, "I<-siss", vt_key) + 1;
    }
  type = cached->where_type - 1;
  switch (type)
    {
    case ROOMLIST_NO_ROOMS:
      return FALSE;
    case ROOMLIST_ALL_ROOMS:
      return TRUE;

    case ROOMLIST_ONE_ROOM:
      if (cached->where_room == TASK_CACHE_NO_ROOM)
        {
          vt_key[0].string = "Tasks";
          vt_key[1].integer = task;
          vt_key[2].string = "Where";
          vt_key[3].string = "Room";
          cached->where_room = prop_get_integer (bundle, "I<-siss", vt_key);
        }
      return cached->where_room == gs_playerroom (game);

    case ROOMLIST_SOME_ROOMS:
      {
        const scr_int room = gs_playerroom (game);

        if (cached->where_rooms.empty ())
          cached->where_rooms.assign (gs_room_count (game),
                                      TASK_CACHE_UNKNOWN);

        /* An out-of-range room can't be cached; take the uncached path so
         * it fails inside prop_get_boolean exactly as it always did. */
        if (room < 0 || room >= (scr_int) cached->where_rooms.size ())
          {
            vt_key[0].string = "Tasks";
            vt_key[1].integer = task;
            vt_key[2].string = "Where";
            vt_key[3].string = "Rooms";
            vt_key[4].integer = room;
            return prop_get_boolean (bundle, "B<-sissi", vt_key);
          }

        if (cached->where_rooms[room] == TASK_CACHE_UNKNOWN)
          {
            vt_key[0].string = "Tasks";
            vt_key[1].integer = task;
            vt_key[2].string = "Where";
            vt_key[3].string = "Rooms";
            vt_key[4].integer = room;
            cached->where_rooms[room] =
                prop_get_boolean (bundle, "B<-sissi", vt_key)
                ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
          }
        return cached->where_rooms[room] == TASK_CACHE_TRUE;
      }

    default:
      scr_fatal ("task_where_allows_run: invalid type, %ld\n", type);
      return FALSE;
    }
}


/*
 * task_can_run_task_directional()
 *
 * Return TRUE if player is in a room where the task can be run and the task
 * is runnable in the given direction.
 */
scr_bool
task_can_run_task_directional (scr_gameref_t game,
                               scr_int task, scr_bool forwards)
{
#ifdef SCARIER_DUMP_TOOLS
  /* Reusable structural-dump / NPC-trace instrumentation; see scdump.c.
   * Compiled only into the headless walkthrough harness (-DSCARIER_DUMP_TOOLS);
   * a normal Spatterlight build omits this entirely. */
  scr_dump_structure_once (game);
#endif

  return task_state_allows_run (game, task, forwards)
         && task_where_allows_run (game, task);
}


/*
 * task_is_room_refused()
 *
 * Return TRUE if the player's room is what stands between this task and a run
 * in the given direction: its Where room list does not cover where the player
 * is standing.
 *
 * Pre-4.0 Runners answer a command that matches such a task with a refusal of
 * their own rather than the game's DontUnderstand text; see run_task_refusal()
 * in scrunner.c.
 *
 * Whether the task has already been run is deliberately NOT part of the test.
 * The Runner checks the room ahead of the task's state, and answers a task that
 * is both done and out of its rooms with the room refusal rather than "You have
 * already done that." (measured live -- probe task "theta").  The reverse
 * direction still asks task_state_allows_run(), which for a backwards run tests
 * only the reversibility flag.
 */
scr_bool
task_is_room_refused (scr_gameref_t game, scr_int task, scr_bool forwards)
{
  return !task_where_allows_run (game, task)
         && (forwards || task_state_allows_run (game, task, FALSE));
}


/*
 * task_is_done_refused()
 *
 * Return TRUE if the only thing standing between this task and a forwards run
 * is that it has already been done and is not repeatable -- the player is in a
 * room the task's Where list covers, but the task is spent.
 *
 * The Runners answer such a command with the task's RepeatText, or with "You
 * have already done that." when there is none; see run_task_refusal() in
 * scrunner.c.  The room half is deliberately part of the test, because a task
 * that is both done and out of its rooms gets the room refusal instead
 * (measured live -- probe task "theta").
 */
scr_bool
task_is_done_refused (scr_gameref_t game, scr_int task)
{
  return gs_task_done (game, task)
         && !task_is_repeatable (game, task)
         && task_where_allows_run (game, task);
}


/*
 * task_can_run_task()
 *
 * Returns TRUE if the task can be run in either direction.
 */
scr_bool
task_can_run_task (scr_gameref_t game, scr_int task)
{
  /*
   * Testing reversible tasks first may be a little more efficient if they
   * aren't common in games.  There is, though, probably a little bit of
   * redundant work going on here.
   */
  return task_can_run_task_directional (game, task, FALSE)
         || task_can_run_task_directional (game, task, TRUE);
}


/*
 * task_move_object()
 *
 * Move an object to a place.
 */
static void
task_move_object (scr_gameref_t game, scr_int object, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  /*
   * Ignore negative object indexes, as evt_move_object() does.  Var1 = 2 is
   * "the referenced object", and a task reached by redirection (or matched by
   * a pattern with no %object%) has none, so var_get_ref_object() hands back
   * -1.  The Runner's Select Case simply moves nothing; SCARE used to walk
   * into gs_object_*() and abort on its range assertion.  Mortality's task
   * 314 [? the return] does exactly this.
   */
  if (object < 0)
    {
      if (task_trace)
        scr_trace ("Task: ignoring move of unset object %ld\n", object);
      return;
    }

  /* Select action depending on var2. */
  switch (var2)
    {
    case 0:                    /* To room */
      if (var3 == 0)
        {
          if (task_trace)
            scr_trace ("Task: moving object %ld to hidden\n", object);

          gs_object_make_hidden (game, object);
        }
      else
        {
          if (task_trace)
            {
              scr_trace ("Task: moving object %ld to room %ld\n",
                        object, var3 - 1);
            }

          /* var3 != 0 here (the var3 == 0 "hidden" case is handled above). */
          gs_object_to_room (game, object, var3 - 1);
        }
      break;

    case 1:                    /* To roomgroup part */
      if (task_trace)
        {
          scr_trace ("Task: moving object %ld to random room in group %ld\n",
                    object, var3);
        }

      {
        scr_int dest = lib_random_roomgroup_member (game, var3);
        if (dest >= 0)           /* Empty group: leave the object in place. */
          gs_object_to_room (game, object, dest);
      }
      break;

    case 2:                    /* Into object */
      if (task_trace)
        scr_trace ("Task: moving object %ld into %ld\n", object, var3);

      gs_object_move_into (game, object, obj_container_object (game, var3));
      break;

    case 3:                    /* Onto object */
      if (task_trace)
        scr_trace ("Task: moving object %ld onto %ld\n", object, var3);

      gs_object_move_onto (game, object, obj_surface_object (game, var3));
      break;

    case 4:                    /* Held by */
      if (task_trace)
        scr_trace ("Task: moving object %ld to held by %ld\n", object, var3);

      if (var3 == 0)            /* Player */
        gs_object_player_get (game, object);
      else if (var3 == 1)       /* Ref character */
        gs_object_npc_get (game, object, var_get_ref_character (vars));
      else                      /* NPC id */
        gs_object_npc_get (game, object, var3 - 2);
      break;

    case 5:                    /* Worn by */
      if (task_trace)
        scr_trace ("Task: moving object %ld to worn by %ld\n", object, var3);

      if (var3 == 0)            /* Player */
        gs_object_player_wear (game, object);
      else if (var3 == 1)       /* Ref character */
        gs_object_npc_wear (game, object, var_get_ref_character (vars));
      else                      /* NPC id */
        gs_object_npc_wear (game, object, var3 - 2);
      break;

    case 6:                    /* Same room as */
      {
        scr_int room, npc;

        if (task_trace)
          {
            scr_trace ("Task: moving object %ld to same room as %ld\n",
                      object, var3);
          }

        if (var3 == 0)          /* Player */
          room = gs_playerroom (game);
        else if (var3 == 1)     /* Ref character */
          {
            npc = var_get_ref_character (vars);
            room = gs_npc_location (game, npc) - 1;
          }
        else                    /* NPC id */
          {
            npc = var3 - 2;
            room = gs_npc_location (game, npc) - 1;
          }
        gs_object_to_room (game, object, room);
        break;
      }

    default:
      /*
       * Unset (combo index -1) or unknown object move destination; ignored, as
       * the Runner does, rather than treated as fatal.  See the matching note in
       * task_run_move_npc_action.
       */
      if (task_trace)
        scr_trace ("Task: ignoring move with unset/unknown"
                  " object move type %ld\n", var2);
      break;
    }
}


/*
 * task_run_move_object_action()
 *
 * Demultiplex an object move action and execute it.
 */
static void
task_run_move_object_action (scr_gameref_t game,
                             scr_int var1, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int object;

  /* Select depending on value in var1. */
  switch (var1)
    {
    case 0:                    /* All held */
      for (object = 0; object < gs_object_count (game); object++)
        {
          if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
            task_move_object (game, object, var2, var3);
        }
      break;

    case 1:                    /* All worn */
      for (object = 0; object < gs_object_count (game); object++)
        {
          if (gs_object_position (game, object) == OBJ_WORN_PLAYER)
            task_move_object (game, object, var2, var3);
        }
      break;

    case 2:                    /* Ref object */
      object = var_get_ref_object (vars);
      task_move_object (game, object, var2, var3);
      break;

    default:                   /* Dynamic object */
      object = obj_dynamic_object (game, var1 - 3);
      task_move_object (game, object, var2, var3);
      break;
    }
}


/*
 * task_move_npc_to_room()
 *
 * Move an NPC to a given room.
 */
static void
task_move_npc_to_room (scr_gameref_t game, scr_int npc, scr_int room)
{
  if (task_trace)
    scr_trace ("Task: moving NPC %ld to room %ld\n", npc, room);

  /* Update the NPC's state. */
  if (room < gs_room_count (game))
    gs_set_npc_location (game, npc, room + 1);
  else
    {
      scr_int dest = lib_random_roomgroup_member (game,
                                                 room - gs_room_count (game));
      if (dest < 0)
        return;                  /* Empty group: leave the NPC in place. */
      gs_set_npc_location (game, npc, dest + 1);
    }

  gs_set_npc_parent (game, npc, -1);
  gs_set_npc_position (game, npc, 0);
}


/*
 * task_run_move_npc_action()
 *
 * Move player or NPC.
 */
static void
task_run_move_npc_action (scr_gameref_t game,
                          scr_int var1, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int npc, room, ref_npc = -1;

  /* Player or NPC? */
  if (var1 == 0)
    {
      /* Player -- decide where to move player to. */
      switch (var2)
        {
        case 0:                /* To room */
          gs_move_player_to_room (game, var3);
          return;

        case 1:                /* To roomgroup part */
          if (task_trace)
            {
              scr_trace ("Task: moving player to random room in group %ld\n",
                        var3);
            }

          {
            scr_int dest = lib_random_roomgroup_member (game, var3);
            if (dest >= 0)       /* Empty group: leave the player in place. */
              gs_move_player_to_room (game, dest);
          }
          return;

        case 2:                /* To same room as... */
          switch (var3)
            {
            case 0:            /* ...player! */
              return;
            case 1:            /* ...referenced NPC */
              npc = var_get_ref_character (vars);
              break;
            default:           /* ...specified NPC */
              npc = var3 - 2;
              break;
            }

          if (task_trace)
            scr_trace ("Task: moving player to same room as NPC %ld\n", npc);

          room = gs_npc_location (game, npc) - 1;
          if (room < 0)
            {
              if (task_trace)
                scr_trace ("Task: silently suppressed player move to hidden\n");
            }
          else
            gs_move_player_to_room (game, room);
          return;

        case 3:                /* To standing on */
          gs_set_playerposition (game, 0);
          gs_set_playerparent (game, obj_standable_object (game, var3 - 1));
          return;

        case 4:                /* To sitting on */
          gs_set_playerposition (game, 1);
          gs_set_playerparent (game, obj_standable_object (game, var3 - 1));
          return;

        case 5:                /* To lying on */
          gs_set_playerposition (game, 2);
          gs_set_playerparent (game, obj_lieable_object (game, var3 - 1));
          return;

        default:
          /*
           * ADRIFT stores a move destination as a combo-box index, and a value
           * of -1 (and any other out-of-range value) means the game's author
           * left the destination unset.  The Runner's Select Case silently
           * ignores such an action, so we treat it as a no-op rather than fatal.
           * With move assist on, honour an unset (-1) move whose Var3 names a
           * real room as "to room" (see task_move_assist).
           */
          if (task_move_assist && var2 == -1
              && var3 >= 0 && var3 < gs_room_count (game))
            {
              gs_move_player_to_room (game, var3);
              return;
            }
          if (task_trace)
            scr_trace ("Task: ignoring move with unset/unknown"
                      " player move type %ld\n", var2);
          return;
        }
    }
  else
    {
      /* NPC -- first find which NPC to move about. */
      if (var1 == 1)
        npc = var_get_ref_character (vars);
      else
        npc = var1 - 2;

      /* Decide where to move the NPC to. */
      switch (var2)
        {
        case 0:                /* To room */
          task_move_npc_to_room (game, npc, var3 - 1);
          return;

        case 1:                /* To roomgroup part */
          if (task_trace)
            {
              scr_trace ("Task: moving NPC %ld to random room in group %ld\n",
                        npc, var3);
            }

          {
            scr_int dest = lib_random_roomgroup_member (game, var3);
            if (dest >= 0)       /* Empty group: leave the NPC in place. */
              task_move_npc_to_room (game, npc, dest);
          }
          return;

        case 2:                /* To same room as... */
          switch (var3)
            {
            case 0:            /* ...player */
              if (task_trace)
                {
                  scr_trace ("Task: moving NPC %ld to same room as player\n",
                            npc);
                }

              task_move_npc_to_room (game, npc, gs_playerroom (game));
              break;
            case 1:            /* ...referenced NPC */
              ref_npc = var_get_ref_character (vars);
              if (task_trace)
                {
                  scr_trace ("Task: moving NPC %ld to"
                            " same room as referenced NPC %ld\n", npc, ref_npc);
                }

              room = gs_npc_location (game, ref_npc) - 1;
              task_move_npc_to_room (game, npc, room);
              break;
            default:           /* ...specified NPC */
              ref_npc = var3 - 2;
              if (task_trace)
                {
                  scr_trace ("Task: moving NPC %ld to"
                            " same room as NPC %ld\n", npc, ref_npc);
                }

              room = gs_npc_location (game, ref_npc) - 1;
              task_move_npc_to_room (game, npc, room);
              break;
            }
          return;

        case 3:                /* To standing on */
          gs_set_npc_position (game, npc, 0);
          gs_set_npc_parent (game, npc, obj_standable_object (game, var3));
          return;

        case 4:                /* To sitting on */
          gs_set_npc_position (game, npc, 1);
          gs_set_npc_parent (game, npc, obj_standable_object (game, var3));
          return;

        case 5:                /* To lying on */
          gs_set_npc_position (game, npc, 2);
          gs_set_npc_parent (game, npc, obj_lieable_object (game, var3));
          return;

        default:
          /* Unset/unknown NPC move destination; ignored, as the Runner does.
           * With move assist on, honour an unset (-1) move whose Var3 names a
           * real room as "to room" (1-based, as for case 0).  A positive Var3
           * that names no room is a dangling index (the editor stored it
           * against a differently-based or since-edited list; the offset even
           * differs between corpus games, so it cannot be decoded reliably);
           * every corpus instance of one -- X-Files' diner buzzer, HYPER
           * Battle System's flare rat -- is a "bring this character on-stage
           * here" direction, so summon the NPC to the player's room instead.
           * A Var3 of 0 or less means nothing was ever selected; that stays
           * a no-op. */
          if (task_move_assist && var2 == -1)
            {
              if (var3 - 1 >= 0 && var3 - 1 < gs_room_count (game))
                {
                  task_move_npc_to_room (game, npc, var3 - 1);
                  return;
                }
              if (var3 > 0)
                {
                  task_move_npc_to_room (game, npc, gs_playerroom (game));
                  return;
                }
            }
          if (task_trace)
            scr_trace ("Task: ignoring move with unset/unknown"
                      " NPC move type %ld\n", var2);
          return;
        }
    }
}


/*
 * task_run_change_object_status()
 *
 * Change the status of an object.
 */
static void
task_run_change_object_status (scr_gameref_t game, scr_int var1, scr_int var2)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object, openable, lockable;

  if (task_trace)
    {
      scr_trace ("Task: setting status of stateful object %ld to %ld\n",
                var1, var2);
    }

  /* Identify the target object. */
  object = obj_stateful_object (game, var1);

  /* See if openable. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Openable";
  openable = prop_get_integer (bundle, "I<-sis", vt_key);
  if (openable > 0)
    {
      /* See if lockable. */
      vt_key[2].string = "Key";
      lockable = prop_get_integer (bundle, "I<-sis", vt_key);
      if (lockable >= 0)
        {
          /* Lockable. */
          if (var2 <= 2)
            gs_set_object_openness (game, object, var2 + 5);
          else
            gs_set_object_state (game, object, var2 - 2);
        }
      else
        {
          /* Not lockable, though openable. */
          if (var2 <= 1)
            gs_set_object_openness (game, object, var2 + 5);
          else
            gs_set_object_state (game, object, var2 - 1);
        }
    }
  else
    /* Not openable. */
    gs_set_object_state (game, object, var2 + 1);

  if (task_trace)
    {
      scr_trace ("Task: openness of object %ld is now %ld\n",
                object, gs_object_openness (game, object));
      scr_trace ("Task: state of object %ld is now %ld\n",
                object, gs_object_state (game, object));
    }
}


/*
 * task_run_change_variable_action()
 *
 * Change a variable's value in inscrutable ways.
 */
static void
task_run_change_variable_action (scr_gameref_t game,
                                 scr_int var1, scr_int var2, scr_int var3,
                                 const scr_char *expr, scr_int var5)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[3];
  const scr_char *name, *string;
  scr_int type, value;

  /*
   * At this point, we need to checkpoint the filter.  We're about to change
   * a variable value, so interpolating here before doing that ensures that
   * any currently buffered text gets the values that were set when the text
   * was buffered.
   */
  pf_checkpoint (filter, vars, bundle);

  /* Get the name and type of the variable being addressed. */
  vt_key[0].string = "Variables";
  vt_key[1].integer = var1;
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Type";
  type = prop_get_integer (bundle, "I<-sis", vt_key);

  /* Select first based on variable type. */
  switch (type)
    {
    case TAFVAR_NUMERIC:       /* Integer */

      /* Select again based on action type. */
      switch (var2)
        {
        case 0:                /* Var = */
          if (task_trace)
            scr_trace ("Task: variable %ld (%s) = %ld\n", var1, name, var3);

          var_put_integer (vars, name, var3);
          return;

        case 1:                /* Var += */
          if (task_trace)
            scr_trace ("Task: variable %ld (%s) += %ld\n", var1, name, var3);

          value = var_get_integer (vars, name) + var3;
          var_put_integer (vars, name, value);
          return;

        case 2:                /* Var = rnd(range) */
          if (task_trace)
            {
              scr_trace ("Task: variable %ld (%s) = random(%ld,%ld)\n",
                        var1, name, var3, var5);
            }

          value = scr_randomint (var3, var5);
          var_put_integer (vars, name, value);
          return;

        case 3:                /* Var += rnd(range) */
          if (task_trace)
            {
              scr_trace ("Task: variable %ld (%s) += random(%ld,%ld)\n",
                        var1, name, var3, var5);
            }

          value = var_get_integer (vars, name) + scr_randomint (var3, var5);
          var_put_integer (vars, name, value);
          return;

        case 4:                /* Var = ref */
          value = var_get_ref_number (vars);
          if (task_trace)
            {
              scr_trace ("Task: variable %ld (%s) = ref, %ld\n",
                        var1, name, value);
            }

          var_put_integer (vars, name, value);
          return;

        case 5:                /* Var = expr */
          if (!expr_eval_numeric_expression (expr, vars, &value))
            {
              scr_error ("task_run_change_variable_action:"
                        " invalid expression, %s\n", expr);
              value = 0;
            }
          if (task_trace)
            {
              scr_trace ("Task: variable %ld (%s) = %s, %ld\n",
                        var1, name, expr, value);
            }

          var_put_integer (vars, name, value);
          return;

        default:
          scr_fatal ("task_run_change_variable_action:"
                    " unknown integer change type, %ld\n", var2);
        }

    case TAFVAR_STRING:        /* String */

      /* Select again based on action type. */
      switch (var2)
        {
        case 0:                /* Var = text literal */
          if (task_trace)
            {
              scr_trace ("Task: variable %ld (%s) = \"%s\"\n",
                        var1, name, expr);
            }

          var_put_string (vars, name, expr);
          return;

        case 1:                /* Var = ref */
          string = var_get_ref_text (vars);
          if (task_trace)
            {
              scr_trace ("Task: variable %ld (%s) = ref, \"%s\"\n",
                        var1, name, string);
            }

          var_put_string (vars, name, string);
          return;

        case 2:                /* Var = expr */
          {
            /*
             * expr_eval_string_expression() hands back a scr_malloc'd string
             * (or we synthesise the error one); own it with RAII so the
             * var_put_string() below -- which can throw scr_fatal_error from a
             * prop_put -- no longer leaks it past the old trailing scr_free.
             */
            scr_char *evaluated;
            if (!expr_eval_string_expression (expr, vars, &evaluated))
              {
                scr_error ("task_run_change_variable_action:"
                          " invalid string expression, %s\n", expr);
                evaluated = (scr_char *) scr_malloc (strlen ("[expr error]") + 1);
                memcpy (evaluated, "[expr error]", strlen ("[expr error]") + 1);
              }
            scr_owned_string mutable_string (evaluated);
            if (task_trace)
              {
                scr_trace ("Task: variable %ld (%s) = %s, %s\n",
                          var1, name, expr, mutable_string.get ());
              }

            var_put_string (vars, name, mutable_string.get ());
            return;
          }

        default:
          scr_fatal ("task_run_change_variable_action:"
                    " unknown string change type, %ld\n", var2);
        }

    default:
      scr_fatal ("task_run_change_variable_action:"
                " invalid variable type, %ld\n", type);
    }
}


/*
 * task_run_change_score_action()
 *
 * Change game score.
 */
static void
task_run_change_score_action (scr_gameref_t game, scr_int task, scr_int var1)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* Increasing or decreasing the score? */
  if (var1 > 0)
    {
      scr_bool increase_score;

      /* See if this task is already scored. */
      increase_score = !gs_task_scored (game, task);
      if (!increase_score)
        {
          scr_vartype_t vt_key[3];
          scr_int version;

          if (task_trace)
            scr_trace ("Task: already scored task %ld\n", var1);

          /* Version 3.8 and 3.7 games permit tasks to rescore. */
          vt_key[0].string = "Version";
          version = prop_get_integer (bundle, "I<-s", vt_key);
          if (version <= TAF_VERSION_380)
            {
              vt_key[0].string = "Tasks";
              vt_key[1].integer = task;
              vt_key[2].string = "SingleScore";
              increase_score = !prop_get_boolean (bundle, "B<-sis", vt_key);

              if (increase_score)
                {
                  if (task_trace)
                    scr_trace ("Task: rescoring version 3.8 task anyway\n");
                }
            }
        }

      /*
       * Increase the score if not yet scored or a version 3.8 multiple
       * scoring task, and note as a scored task.
       */
      if (increase_score)
        {
          if (task_trace)
            scr_trace ("Task: increased score by %ld\n", var1);

          game->score += var1;
          gs_set_task_scored (game, task, TRUE);
        }
    }
  else if (var1 < 0)
    {
      /* Decrease the score. */
      if (task_trace)
        scr_trace ("Task: decreased score by %ld\n", -(var1));

      game->score += var1;
    }
}


/*
 * task_run_set_task_action()
 *
 * Redirect to another task.
 */
static scr_bool
task_run_set_task_action (scr_gameref_t game, scr_int var1, scr_int var2)
{
  scr_bool status = FALSE;

  /* Select based on var1. */
  if (var1 == 0)
    {
      /*
       * Redirect forwards.  Once the game has ended, this dispatch is a no-op;
       * the guard lives at the top of task_run_task(), exactly as the Runner
       * puts it at the top of its RunTask.  Plain in-line actions are *not*
       * dropped; see task_run_task_actions() below.
       */
      if (task_can_run_task_directional (game, var2, TRUE))
        {
          if (task_trace)
            scr_trace ("Task: redirecting to task %ld\n", var2);

          status = task_run_task (game, var2, TRUE);
        }
      else
        {
          if (task_trace)
            scr_trace ("Task: can't redirect to task %ld\n", var2);
        }
    }
  else
    {
      /* Undo task. */
      gs_set_task_done (game, var2, FALSE);
      if (task_trace)
        scr_trace ("Task: reversing task %ld\n", var2);
    }

  return status;
}


/*
 * task_run_end_game_action()
 *
 * End of game task action.
 */
static scr_bool
task_run_end_game_action (scr_gameref_t game, scr_int var1)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool status = FALSE;

  /* Print a message based on var1. */
  switch (var1)
    {
    case 0:
      {
        scr_vartype_t vt_key[2];
        const scr_char *wintext;

        /* Get game WinText. */
        vt_key[0].string = "Header";
        vt_key[1].string = "WinText";
        wintext = prop_get_string (bundle, "S<-ss", vt_key);

        /* Print WinText, if any defined, otherwise a default. */
        if (!scr_strempty (wintext))
          {
            pf_buffer_string (filter, wintext);
            pf_buffer_character (filter, '\n');
          }
        else
          pf_buffer_string (filter, "Congratulations!\n");

        /* Handle any associated WinRes resource. */
        vt_key[0].string = "Globals";
        vt_key[1].string = "WinRes";
        res_handle_resource (game, "ss", vt_key);

        status = TRUE;
        break;
      }

    case 1:
      pf_buffer_string (filter, "Better luck next time.\n");
      status = TRUE;
      break;

    case 2:
      pf_buffer_string (filter, "I'm afraid you are dead!\n");
      status = TRUE;
      break;

    case 3:
      break;

    default:
      scr_fatal ("task_run_end_game_action: invalid type, %ld\n", var1);
    }

  /* Stop the game, and note that it's not resumeable. */
  game->is_running = FALSE;
  game->has_completed = TRUE;

  return status;
}


/*
 * task_run_change_battle_action()
 *
 * Battle System "Change battle attribute" task action (type 7).  var1 is the
 * ADRIFT attribute index, var2 the target character, var3 the value (a signed
 * delta, or an enum for attitude/speed).  Attitude (0) and Speed (0xB) apply to
 * NPCs only, with target 0 = the referenced character and target N>=1 = NPC
 * N-1.  The remaining attributes may also target the player: target 0 = player,
 * 1 = referenced character, N>=2 = NPC N-2.  An unresolved or out-of-range
 * target is silently ignored, as the Runner does.
 */
static void
task_run_change_battle_action (scr_gameref_t game,
                               scr_int var1, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int npc;

  if (var1 == 0 || var1 == 0xB)
    {
      npc = (var2 == 0) ? var_get_ref_character (vars) : var2 - 1;
      if (npc < 0 || npc >= gs_npc_count (game))
        return;
    }
  else if (var2 == 0)
    {
      npc = -1;                        /* Player. */
    }
  else
    {
      npc = (var2 == 1) ? var_get_ref_character (vars) : var2 - 2;
      if (npc < 0 || npc >= gs_npc_count (game))
        return;
    }

  if (task_trace)
    {
      scr_trace ("Task: changing battle attribute %ld of %s by/to %ld\n",
                var1, (npc < 0) ? "player" : "NPC", var3);
    }

  battle_change_attribute (game, npc, var1, var3);
}


/*
 * task_run_task_action()
 *
 * Demultiplexer for task actions.
 */
static scr_bool
task_run_task_action (scr_gameref_t game, scr_int task, scr_int action)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int type, var1, var2, var3, var5;
  const scr_char *expr;
  scr_bool status = FALSE;

  /* Get the task action type. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Actions";
  vt_key[3].integer = action;
  vt_key[4].string = "Type";
  type = prop_get_integer (bundle, "I<-sisis", vt_key);

  /* Demultiplex depending on type. */
  switch (type)
    {
    case 0:                    /* Move object. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_move_object_action (game, var1, var2, var3);
      break;

    case 1:                    /* Move player/NPC. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_move_npc_action (game, var1, var2, var3);
      break;

    case 2:                    /* Change object status. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_change_object_status (game, var1, var2);
      break;

    case 3:                    /* Change variable. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Expr";
      expr = prop_get_string (bundle, "S<-sisis", vt_key);
      vt_key[4].string = "Var5";
      var5 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_change_variable_action (game, var1, var2, var3, expr, var5);
      break;

    case 4:                    /* Change score. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_change_score_action (game, task, var1);
      break;

    case 5:                    /* Execute/unset task. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      status = task_run_set_task_action (game, var1, var2);
      break;

    case 6:                    /* End game. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      status = task_run_end_game_action (game, var1);
      break;

    case 7:                    /* Change battle attribute. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_change_battle_action (game, var1, var2, var3);
      break;

    default:
      scr_fatal ("task_run_task_action: unknown action type %ld\n", type);
    }

  return status;
}


/*
 * task_run_task_actions()
 *
 * Run every task action associated with the task.  Actions after one that
 * ends the game still run, with output muted; see the comment on the loop.
 * Returns TRUE if any action ran and itself returned TRUE.
 */
static scr_bool
task_run_task_actions (scr_gameref_t game, scr_int task)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int action_count, action;
  scr_bool status, muted;

  /* Get the count of task actions. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Actions";
  action_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  if (action_count > 0)
    {
      if (task_trace)
        {
          scr_trace ("Task: task %ld running %ld action%s\n",
                    task, action_count, action_count == 1 ? "" : "s");
        }
    }

  /*
   * Run all task actions, capturing any TRUE status returned.  If an action
   * ends the game, run the remaining ones silently rather than exiting the
   * loop early.
   *
   * This seems counterintuitive, but it is what the Runner does, and it was
   * confirmed against run400 with the "EG" arena probe (see
   * test/adrift4/harness/make_arena_probe.py): a task whose actions are
   * "end game" then "+7 points" finishes on 7 out of 7, exactly as when the
   * two are the other way round.  Nothing the trailing actions print is
   * shown, hence the muting.  The Runner's own action executor
   * (mdlSpreadTheLoad.Sub_20_11) reads its gameover flag exactly once, inside
   * the EndGame handler at 0008D621, where it guards re-printing the ending
   * -- there is no test at the loop head.  The one kind of action it *does*
   * drop after the ending is Execute Task, because that dispatches through
   * RunTask, which does have such a test; see task_run_task().
   */
  status = FALSE;
  muted = FALSE;
  for (action = 0; action < action_count; action++)
    {
      scr_bool was_running;

      was_running = game->is_running;
      status |= task_run_task_action (game, task, action);

      /* Did this action end the game? */
      if (was_running && !game->is_running)
        {
          if (task_trace)
            {
              scr_trace ("Task: task %ld action %ld ended game\n",
                        task, action);
            }

          /* Mute the filter, and note that we did it, but continue. */
          pf_mute (filter);
          muted = TRUE;
        }
    }

  /* If this stack frame muted the filter, un-mute it now. */
  if (muted)
    pf_clear_mute (filter);

  /* Return TRUE if any task action returned TRUE. */
  return status;
}


/*
 * task_start_npc_walks()
 *
 * Start NPC walks based on alerts.
 */
static void
task_start_npc_walks (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int alert_count, alert;

  /* Get a count of NPC walk alerts. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "NPCWalkAlert";
  alert_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /* Check alerts, and start any walks that need starting. */
  for (alert = 0; alert < alert_count; alert += 2)
    {
      scr_int npc, walk;

      vt_key[3].integer = alert;
      npc = prop_get_integer (bundle, "I<-sisi", vt_key);
      vt_key[3].integer = alert + 1;
      walk = prop_get_integer (bundle, "I<-sisi", vt_key);
      npc_start_npc_walk (game, npc, walk);
    }
}


/*
 * task_run_task_unrestricted()
 *
 * Run a task, providing restrictions permit, in the given direction.  Return
 * TRUE if the task ran, or we handled it in some complete way, for example by
 * outputting a message describing what prevented it, or why it couldn't be
 * done.
 */
static scr_bool
task_run_task_unrestricted (scr_gameref_t game, scr_int task, scr_bool forwards)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *completetext, *additionalmessage;
  scr_int action_count, showroomdesc;
  scr_bool status;

  /* Start considering task output tracking. */
  status = FALSE;

  /*
   * If reversing, print any reverse message for the task, and undo the task,
   * then return.
   */
  if (!forwards)
    {
      const scr_char *reversemessage;

      /* If not yet done, we can hardly reverse it. */
      if (gs_task_done (game, task))
        {
          vt_key[0].string = "Tasks";
          vt_key[1].integer = task;
          vt_key[2].string = "ReverseMessage";
          reversemessage = prop_get_string (bundle, "S<-sis", vt_key);
          if (!scr_strempty (reversemessage))
            {
              pf_buffer_paragraph_line (filter, reversemessage);
              status |= TRUE;
            }

          /* Undo the task. */
          gs_set_task_done (game, task, FALSE);
        }

      /* Return status of undo. */
      return status;
    }

  /* See if we are trying to repeat a task that's not repeatable. */
  if (gs_task_done (game, task))
    {
      scr_bool repeatable;

      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = "Repeatable";
      repeatable = prop_get_boolean (bundle, "B<-sis", vt_key);
      if (!repeatable)
        {
          const scr_char *repeattext;

          vt_key[2].string = "RepeatText";
          repeattext = prop_get_string (bundle, "S<-sis", vt_key);
          if (!scr_strempty (repeattext))
            {
              if (task_trace)
                {
                  scr_trace ("Task:"
                            " trying to repeat completed action, aborting\n");
                }

              pf_buffer_paragraph_line (filter, repeattext);
              status |= TRUE;
              return status;
            }

          /*
           * Task done, yet not repeatable, so don't consider this case
           * handled.
           */
          return status;
        }
    }

  /* Mark the task as done. */
  gs_set_task_done (game, task, TRUE);

  /* Print any task completion text. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "CompleteText";
  completetext = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (completetext))
    {
      pf_buffer_paragraph_line (filter, completetext);
      status |= TRUE;
    }

  /* Handle any task completion resource. */
  vt_key[2].string = "Res";
  res_handle_resource (game, "sis", vt_key);

  /*
   * Things get slightly tricky here.  We need to filter the completion text
   * for the task using any final variable values generated or modified by
   * task actions, but other task text, run by actions, according to the
   * variable value in effect when it runs.
   *
   * To do this, we take a local copy of the filter's current buffer at this
   * point, remove it from the filter, run task actions with checkpointing,
   * then prepend it back into the filter after all the actions are done.
   *
   * As an optimization, we can avoid doing this if there are no task actions.
   */
  vt_key[2].string = "Actions";
  action_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  if (action_count > 0)
    {
      /*
       * Take ownership of the current filter buffer text, then start NPC
       * walks based on alerts, and run any and all task actions.  Note that
       * the buffer transferred out of the filter may be NULL if there is no
       * text currently in the filter.  RAII the transferred buffer: the
       * task_start_npc_walks() / task_run_task_actions() below can throw
       * (run_loop_halt / scr_fatal_error), which previously leaked it past the
       * prepend-and-free.
       */
      scr_owned_string buffer (pf_transfer_buffer (filter));
      task_start_npc_walks (game, task);
      status |= task_run_task_actions (game, task);

      /* Prepend the saved buffer data back onto the front of the filter. */
      if (buffer)
        pf_prepend_string (filter, buffer.get ());
    }
  else
    {
      /* Start NPC walks only; there are no task actions. */
      task_start_npc_walks (game, task);
    }

  /* Append any room description and additional message for the task. */
  vt_key[2].string = "ShowRoomDesc";
  showroomdesc = prop_get_integer (bundle, "I<-sis", vt_key);
  if (showroomdesc != 0)
    {
      lib_print_room_name (game, showroomdesc - 1);
      lib_print_room_description (game, showroomdesc - 1);
      status |= TRUE;
    }

  vt_key[2].string = "AdditionalMessage";
  additionalmessage = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (additionalmessage))
    {
      pf_buffer_paragraph_line (filter, additionalmessage);
      status |= TRUE;
    }

  /* Return status -- TRUE if matched and we output something. */
  return status;
}


/*
 * task_run_task()
 *
 * Run a task, providing restrictions permit, in the given direction.  At the
 * same time, check for signs of an infinite loop in game tasks, and fail the
 * task with an error message if we seem to be in one.  Checked by counting
 * the call depth.
 */
scr_bool
task_run_task (scr_gameref_t game, scr_int task, scr_bool forwards)
{
  static scr_int recursion_depth = 0;

  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *fail_message;
  scr_bool restrictions_passed, status;

  if (task_trace)
    {
      scr_trace ("Task: running task %ld %s, depth %ld\n",
                task, forwards ? "forwards" : "backwards", recursion_depth);
    }

  /*
   * Refuse to run anything once the game has ended.  This is the Runner's own
   * first act in the equivalent routine: mdlSpreadTheLoad.Sub_20_22 opens at
   * file offset 0005F750 with
   *
   *     ImpAdLdUI1 <gameover> ; CI2UI1 ; LitI2_Byte 0 ; GtI2 ; BranchF ; ExitProc
   *
   * i.e. "If gameOver > 0 Then Exit Sub", ahead of restrictions, completion
   * text and actions alike.  The one caller that can reach here after an
   * ending is the type-5 Execute Task action, whose branch in the action
   * executor (Sub_20_11 @ 0008D5D1) calls RunTask unguarded -- so the Runner
   * refuses the *dispatch*, not the callee's completion.  Measured with the
   * "EG" arena probe: "printlast" (end game, then execute a task that scores)
   * finishes on 0 out of 7, while its "printfirst" control -- the same two
   * actions in the other order -- finishes on 7.  The action *loop* has no
   * such test, which is why in-line actions behind an ending still run; see
   * task_run_task_actions().
   */
  if (!game->is_running)
    {
      if (task_trace)
        scr_trace ("Task: game over, not running task %ld\n", task);

      return FALSE;
    }

  /* Check restrictions. */
  if (!restr_eval_task_restrictions (game, task,
                                     &restrictions_passed, &fail_message))
    {
      scr_error ("task_run_task: restrictions error, %ld\n", task);
      return FALSE;
    }
  if (!restrictions_passed)
    {
      if (task_trace)
        {
          scr_trace ("Task: restrictions failed, task %s\n",
                    fail_message ? "failed" : "aborted");
        }

      if (fail_message)
        {
          /*
           * Print a message, and return TRUE since we can consider this task
           * "done" (more accurately, we've output text, so the task command
           * searching in the main run loop can exit...).
           */
          pf_buffer_paragraph_line (filter, fail_message);
          return TRUE;
        }

      /* Task not done; look for more possibilities. */
      return FALSE;
    }

  /* Check for infinite recursion. */
  if (recursion_depth > TASK_MAXIMUM_RECURSION)
    {
      scr_error ("task_run_task: maximum recursion depth exceeded --"
                " game task loop?\n");
      return FALSE;
    }

  /* Increment depth, run the task, then decrement depth. */
  recursion_depth++;
  status = task_run_task_unrestricted (game, task, forwards);
  recursion_depth--;

  if (task_trace)
    {
      scr_trace ("Task: task %ld finished, return %s, depth %ld\n",
                task, status ? "true" : "false", recursion_depth);
    }

  /* Return the task's status. */
  return status;
}


/*
 * task_debug_trace()
 *
 * Set task tracing on/off.
 */
void
task_debug_trace (scr_bool flag)
{
  task_trace = flag;
}

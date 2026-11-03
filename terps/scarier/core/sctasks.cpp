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
 * Tasks, the core: the dispatched-run bracket, the move and room assists,
 * hints, the per-task property cache, and whether a task can run --
 * task_can_run_task() and its refusal predicates.
 */

#include "sctasks_internal.h"

/* The module's state; see sctasks_internal.h. */
scr_task_state_t tasks;

/*
 * How deep we are inside a task the engine dispatched rather than the player's
 * line -- an event's TaskAffected, a walk's CharTask or ObjectTask, an
 * "execute task" action, a battle task.
 *
 * The Runner's execute_task takes a mode argument, and it is the whole of the
 * difference between the two kinds of run.  With mode 1 the task's CompleteText
 * is joined onto the turn's one string after pspace() (run390 43F106-43F132,
 * run400 45A239-45A265); with any other mode the string is REPLACED by the
 * text (run390 43F15F, run400 45A27D-45A2AD, where the replacement reads
 * `msg = var_AC & CompleteText` with var_AC never assigned), and the caller
 * is the one that preserves whatever the turn had said -- run390's inventory
 * saves `out & "  "` before the call and puts it back afterwards (439B55).
 * Mode 1 is what Sub_20_22 passes (run400 45FA66, our run_task_run_by_index)
 * and what checkevent passes when it dispatches by command text (run390
 * 42D3F5, run380 43A762); the typed line's own matcher passes 0 and its
 * fallback pass 2 (run390 generaltasks 45F48B, 460584).
 *
 * The count is tasks.run.dispatch_depth.
 */

/*
 * task_push_dispatched_run()
 * task_pop_dispatched_run()
 * task_in_dispatched_run()
 *
 * Bracket a task run the engine asked for, so that task_run_task_unrestricted()
 * can tell it from the one the player's line matched; see the note above.
 * Nesting is why this is a count rather than a flag -- anything a dispatched
 * task runs was dispatched too.
 */
void
task_push_dispatched_run (void)
{
  tasks.run.dispatch_depth++;
}

void
task_pop_dispatched_run (void)
{
  assert (tasks.run.dispatch_depth > 0);
  tasks.run.dispatch_depth--;
}

scr_bool
task_in_dispatched_run (void)
{
  return tasks.run.dispatch_depth > 0;
}


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
 * the mansion).  With move assist on, an unset (-1) move whose Var3 names a
 * real room is honoured as "to room", letting such games be completed.
 * Strictly opt-in, as it deliberately diverges from the reference Runner.
 */
void
task_set_move_assist (scr_bool flag)
{
  tasks.options.move_assist = flag;
}

scr_bool
task_get_move_assist (void)
{
  return tasks.options.move_assist;
}


/*
 * Optional "room assist" mode (opt-in, off by default).  A task whose Where
 * room list is ROOMLIST_NO_ROOMS can never run: every Runner answers its
 * command with the wrong-room refusal, and Scarier does the same.  In a few
 * games the author left that setting on a critical-path task, most likely
 * meaning "anywhere" -- The Hangover's T10 (the doctor's fries) and T14 (the
 * winning `give approval notes to platypus`), so it walls at 5 of 7.  With
 * the assist on such a task is treated as runnable in every room.  Not safe
 * as a global default: other games (e.g. The Plague) park tasks there on
 * purpose.  Strictly opt-in, as it deliberately diverges from the Runner.
 */
void
task_set_room_assist (scr_bool flag)
{
  tasks.options.room_assist = flag;
}

scr_bool
task_get_room_assist (void)
{
  return tasks.options.room_assist;
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

  /* Look up and return the requested hint string. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = hint;
  return prop_get_string (bundle, "S<-sis", vt_key);
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

/*
 * task_cache_entry()
 *
 * Return the cache entry for a task, resetting the cache if it was built
 * for a different game.
 */
static scr_task_props_t *
task_cache_entry (scr_gameref_t game, scr_int task)
{
  if (tasks.cache.game != game)
    {
      scr_task_props_t initial;

      initial.repeatable = TASK_CACHE_UNKNOWN;
      initial.repeattext_empty = TASK_CACHE_UNKNOWN;
      initial.reversible = TASK_CACHE_UNKNOWN;
      initial.where_type = 0;
      initial.where_room = TASK_CACHE_NO_ROOM;

      tasks.cache.props.assign (gs_task_count (game), initial);
      tasks.cache.game = game;
    }
  if (!gs_task_valid (game, task))
    scr_fatal ("task_cache_entry: invalid task, %ld\n", task);
  return &tasks.cache.props[task];
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
  if (tasks.cache.game == game)
    {
      tasks.cache.game = nullptr;
      tasks.cache.props.clear ();
    }
}


/*
 * task_is_repeatable()
 * task_repeattext_is_empty()
 *
 * Cached readings of a task's Repeatable flag, and of whether its RepeatText
 * is empty.
 */
scr_bool
task_is_repeatable (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_task_props_t *cached;

  cached = task_cache_entry (game, task);
  if (cached->repeatable == TASK_CACHE_UNKNOWN)
    {
      cached->repeatable = prop_get_indexed_boolean (bundle, "Tasks", task,
                                                     "Repeatable")
                           ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
    }
  return cached->repeatable == TASK_CACHE_TRUE;
}

scr_bool
task_repeattext_is_empty (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_task_props_t *cached;

  cached = task_cache_entry (game, task);
  if (cached->repeattext_empty == TASK_CACHE_UNKNOWN)
    {
      const scr_char *repeattext;

      repeattext = prop_get_indexed_string (bundle, "Tasks", task,
                                            "RepeatText");
      cached->repeattext_empty = scr_strempty (repeattext)
                                 ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
    }
  return cached->repeattext_empty == TASK_CACHE_TRUE;
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
task_state_allows_run (scr_gameref_t game, scr_int task, scr_bool forwards)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
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
          cached->reversible = prop_get_indexed_boolean (bundle, "Tasks", task,
                                                         "Reversible")
                               ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
        }
      if (cached->reversible == TASK_CACHE_FALSE)
        return FALSE;

      /*
       * run390 reverses a task only when it is done or repeatable (checktask
       * 44B474); a reverse command of any other task claims the line with
       * its RepeatText instead, see run_spent_task_390().
       */
      if (prop_get_taf_version (bundle) == TAF_VERSION_390
          && !gs_task_done (game, task) && !task_is_repeatable (game, task))
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
scr_bool
task_where_allows_run (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int type;
  scr_task_props_t *cached;

  /* A nonexistent task is runnable nowhere. */
  if (!gs_task_valid (game, task))
    return FALSE;

  cached = task_cache_entry (game, task);
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Where";

  /* Check room list for the task and return it. */
  if (cached->where_type == 0)
    {
      vt_key[3].string = "Type";
      cached->where_type = prop_get_integer (bundle, "I<-siss", vt_key) + 1;
    }
  type = cached->where_type - 1;
  switch (type)
    {
    case ROOMLIST_NO_ROOMS:
      return tasks.options.room_assist;
    case ROOMLIST_ALL_ROOMS:
      return TRUE;

    case ROOMLIST_ONE_ROOM:
      if (cached->where_room == TASK_CACHE_NO_ROOM)
        {
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
        vt_key[3].string = "Rooms";
        vt_key[4].integer = room;

        /*
         * An out-of-range room can't be cached; take the uncached path so
         * it fails inside prop_get_boolean exactly as it always did.
         */
        if (room < 0 || room >= (scr_int) cached->where_rooms.size ())
          return prop_get_boolean (bundle, "B<-sissi", vt_key);

        if (cached->where_rooms[room] == TASK_CACHE_UNKNOWN)
          {
            cached->where_rooms[room] =
                prop_get_boolean (bundle, "B<-sissi", vt_key)
                ? TASK_CACHE_TRUE : TASK_CACHE_FALSE;
          }
        return cached->where_rooms[room] == TASK_CACHE_TRUE;
      }

    default:
      scr_fatal ("task_where_allows_run: invalid type, %ld\n", type);
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
  /*
   * Reusable structural-dump / NPC-trace instrumentation; see
   * tools/scdump.cpp.
   * Compiled only into the headless walkthrough harness (-DSCARIER_DUMP_TOOLS);
   * a normal Spatterlight build omits this entirely.
   */
  scr_dump_structure_once (game);
#endif

  if (!gs_task_valid (game, task))
    return FALSE;

  return task_state_allows_run (game, task, forwards)
         && task_where_allows_run (game, task);
}


/*
 * task_is_reverse_refused_390()
 *
 * TRUE for a 3.90 task whose reverse command run390 answers with the task's
 * RepeatText rather than a reversal: reversible, but neither done nor
 * repeatable (checktask 44B4B2).  See run_spent_task_390() in
 * runner/scrun_match_refusal.cpp.
 */
scr_bool
task_is_reverse_refused_390 (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return prop_get_taf_version (bundle) == TAF_VERSION_390
         && gs_task_valid (game, task)
         && prop_get_indexed_boolean (bundle, "Tasks", task, "Reversible")
         && !gs_task_done (game, task)
         && !task_is_repeatable (game, task);
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
 * in runner/scrun_match_refusal.cpp.
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
  return gs_task_valid (game, task)
         && !task_where_allows_run (game, task)
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
 * runner/scrun_match_refusal.cpp.  The room half is deliberately part of the
 * test, because a task that is both done and out of its rooms gets the room
 * refusal instead (measured live -- probe task "theta").
 */
scr_bool
task_is_done_refused (scr_gameref_t game, scr_int task)
{
  return gs_task_valid (game, task)
         && gs_task_done (game, task)
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
 * task_debug_trace()
 *
 * Set task tracing on/off.
 */
void
task_debug_trace (scr_bool flag)
{
  tasks.trace = flag;
}

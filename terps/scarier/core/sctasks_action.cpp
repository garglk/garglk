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
 * The remaining task actions -- object status, variable, score, set task,
 * battle attribute -- and the dispatch over a task's action list:
 * task_run_task_action(), task_run_task_actions().
 */

#include "sctasks_internal.h"

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

  if (tasks.trace)
    {
      scr_trace ("Task: setting status of stateful object %ld to %ld\n",
                var1, var2);
    }

  /* Identify the target object; none if there are no stateful objects. */
  object = obj_stateful_object (game, var1);
  if (object < 0)
    return;

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

  if (tasks.trace)
    {
      scr_trace ("Task: openness of object %ld is now %ld\n",
                object, gs_object_openness (game, object));
      scr_trace ("Task: state of object %ld is now %ld\n",
                object, gs_object_state (game, object));
    }
}


/*
 * task_change_integer_variable()
 *
 * The numeric variable arm of task_run_change_variable_action().
 */
static void
task_change_integer_variable (scr_gameref_t game, const scr_char *name,
                              scr_int var1, scr_int var2, scr_int var3,
                              const scr_char *expr, scr_int var5)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int value;

  switch (var2)
    {
    case 0:                /* Var = */
      if (tasks.trace)
        scr_trace ("Task: variable %ld (%s) = %ld\n", var1, name, var3);

      var_put_integer (vars, name, var3);
      return;

    case 1:                /* Var += */
      if (tasks.trace)
        scr_trace ("Task: variable %ld (%s) += %ld\n", var1, name, var3);

      value = var_get_integer (vars, name) + var3;
      var_put_integer (vars, name, value);
      return;

    case 2:                /* Var = rnd(range) */
      if (tasks.trace)
        {
          scr_trace ("Task: variable %ld (%s) = random(%ld,%ld)\n",
                    var1, name, var3, var5);
        }

      value = scr_randomint (var3, var5);
      var_put_integer (vars, name, value);
      return;

    case 3:                /* Var += rnd(range) */
      if (tasks.trace)
        {
          scr_trace ("Task: variable %ld (%s) += random(%ld,%ld)\n",
                    var1, name, var3, var5);
        }

      value = var_get_integer (vars, name) + scr_randomint (var3, var5);
      var_put_integer (vars, name, value);
      return;

    case 4:                /* Var = ref */
      value = var_get_ref_number (vars);
      if (tasks.trace)
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
      if (tasks.trace)
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
}


/*
 * task_change_string_variable()
 *
 * The string variable arm of task_run_change_variable_action().
 */
static void
task_change_string_variable (scr_gameref_t game, const scr_char *name,
                             scr_int var1, scr_int var2, const scr_char *expr)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *string;

  switch (var2)
    {
    case 0:                /* Var = text literal */
      if (tasks.trace)
        {
          scr_trace ("Task: variable %ld (%s) = \"%s\"\n",
                    var1, name, expr);
        }

      var_put_string (vars, name, expr);
      return;

    case 1:                /* Var = ref */
      string = var_get_ref_text (vars);
      if (tasks.trace)
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
         * (or we synthesise the error one); own it, since var_put_string()
         * below can throw scr_fatal_error from a prop_put.
         */
        scr_char *evaluated;
        if (!expr_eval_string_expression (expr, vars, &evaluated))
          {
            scr_error ("task_run_change_variable_action:"
                      " invalid string expression, %s\n", expr);
            evaluated = scr_strdup ("[expr error]");
          }
        scr_owned_string mutable_string (evaluated);
        if (tasks.trace)
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
  const scr_char *name;
  scr_int type;

  /*
   * At this point, we need to checkpoint the filter.  We're about to change
   * a variable value, so interpolating here before doing that ensures that
   * any currently buffered text gets the values that were set when the text
   * was buffered.
   *
   * Version 4.0 does no such thing.  Its filter passes happen at the end of a
   * completing task and at the flush -- both of them after this action has
   * run -- so text buffered before the change comes out holding the value the
   * change set, not the one it replaced.  Measured under run400.exe in Wine on
   * 2026-08-24 with harness/make_400_alrsrcprobe.py, whose "victor" task has
   * CompleteText "CT n=%n% TXT %w%." and one action setting n = 9 over an
   * initial 5: the Runner answers "CT n=9 TXT qqball."  Pre-4.0 keeps the
   * checkpoint, which is not measured either way.
   */
  if (prop_get_taf_version (bundle) < TAF_VERSION_400)
    pf_checkpoint (filter, vars, bundle);

  /* Get the name and type of the variable being addressed. */
  vt_key[0].string = "Variables";
  vt_key[1].integer = var1;
  name = var_indexed_name (bundle, var1);
  vt_key[2].string = "Type";
  type = prop_get_integer (bundle, "I<-sis", vt_key);

  /* Select first based on variable type. */
  switch (type)
    {
    case TAFVAR_NUMERIC:       /* Integer */
      task_change_integer_variable (game, name, var1, var2, var3, expr, var5);
      return;

    case TAFVAR_STRING:        /* String */
      task_change_string_variable (game, name, var1, var2, expr);
      return;

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
          if (tasks.trace)
            scr_trace ("Task: already scored task %ld\n", var1);

          /* Version 3.8 and 3.7 games permit tasks to rescore. */
          if (prop_get_taf_version (bundle) <= TAF_VERSION_380)
            {
              increase_score = !prop_get_indexed_boolean (bundle, "Tasks",
                                                          task, "SingleScore");
              if (increase_score && tasks.trace)
                scr_trace ("Task: rescoring version 3.8 task anyway\n");
            }
        }

      /*
       * Increase the score if not yet scored or a version 3.8 multiple
       * scoring task, and note as a scored task.
       */
      if (increase_score)
        {
          if (tasks.trace)
            scr_trace ("Task: increased score by %ld\n", var1);

          game->score += var1;
          gs_set_task_scored (game, task, TRUE);
        }
    }
  else if (var1 < 0)
    {
      /* Decrease the score. */
      if (tasks.trace)
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

  /*
   * Var2 is a task index straight from the TAF file; an out-of-range one
   * would otherwise reach the unchecked game state accessors below.
   */
  if (var2 < 0 || var2 >= gs_task_count (game))
    {
      if (tasks.trace)
        scr_trace ("Task: set-task action names invalid task %ld\n", var2);
      return FALSE;
    }

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
          if (tasks.trace)
            scr_trace ("Task: redirecting to task %ld\n", var2);

          status = run_task_run_by_index (game, var2);
        }
      else
        {
          if (tasks.trace)
            scr_trace ("Task: can't redirect to task %ld\n", var2);
        }

      /*
       * run400 48D5DE: after the execute-task action, events started by the
       * executed task are checked at once (4.0 only; see scevents.cpp).
       */
      evt_check_events_started_by_task (game, var2);
    }
  else
    {
      /* Undo task. */
      gs_set_task_done (game, var2, FALSE);
      if (tasks.trace)
        scr_trace ("Task: reversing task %ld\n", var2);
    }

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

  /*
   * The parser has already rewritten a 3.9 file's attribute index into the
   * 4.0 numbering (parse_fixup_v390_battle_attribute: 5->7 defence, 6->8
   * max defence, 7->0xB speed), so var1 is 4.0-numbered here for every
   * version.  run390 execute_action (4573DC) then agrees with run400 on the
   * targets of everything but Speed: its speed arm (45735E-45738D) writes
   * MemVar_468028(var2).global_122 = CByte(var3) -- the NPC array indexed by
   * var2 RAW, with no referenced-character case and no -1 -- where 4.0
   * (and 3.9 attitude, 456B91-456BF4) take 0 = referenced character and
   * N >= 1 = NPC N-1.  deaths task 15 ("kill jim": speed of target 1 = 3)
   * re-arms NPC 1 in run390, not the ally Joshua at NPC 0, which is why the
   * Runner never shows Joshua stabbing every third turn
   * (runner_transcripts/deaths T42, T48).
   */
  if (var1 == 0xB
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      if (var2 < 0 || var2 >= gs_npc_count (game))
        return;
      if (tasks.trace)
        scr_trace ("Task: setting 3.9 speed of NPC %ld to %ld\n", var2, var3);
      battle_change_attribute (game, var2, 0xB, var3);
      return;
    }

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
      /*
       * run390's max defence arm (45725D) indexes the NPC array with
       * Var2 - 1 (push 1 at 45730F/457345) where every other arm uses
       * Var2 - 2, so a 3.9 "max defence of NPC N" lands on NPC N+1.  Outside
       * task 11 raises Joe's max defence by 30 and the Runner shows Joe at
       * "Defense value: 80 (30)" afterwards -- the raise went to NPC 1, the
       * first guard -- so Joe dies on `escape`
       * (runner_probes/outside.run390.statx.txt).  A target past the last
       * NPC is skipped.
       */
      if (var2 != 1 && var1 == 8
          && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
        npc = var2 - 1;
      else
        npc = task_selector_npc (vars, var2);
      if (npc < 0 || npc >= gs_npc_count (game))
        return;
    }

  if (tasks.trace)
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
      prop_get_var_integers (bundle, vt_key, 3, &var1, &var2, &var3);
      task_run_move_object_action (game, var1, var2, var3);
      break;

    case 1:                    /* Move player/NPC. */
      prop_get_var_integers (bundle, vt_key, 3, &var1, &var2, &var3);
      task_run_move_npc_action (game, var1, var2, var3);
      break;

    case 2:                    /* Change object status. */
      prop_get_var_integers (bundle, vt_key, 2, &var1, &var2, nullptr);
      task_run_change_object_status (game, var1, var2);
      break;

    case 3:                    /* Change variable. */
      prop_get_var_integers (bundle, vt_key, 3, &var1, &var2, &var3);
      vt_key[4].string = "Expr";
      expr = prop_get_string (bundle, "S<-sisis", vt_key);
      vt_key[4].string = "Var5";
      var5 = prop_get_integer (bundle, "I<-sisis", vt_key);
      task_run_change_variable_action (game, var1, var2, var3, expr, var5);
      break;

    case 4:                    /* Change score. */
      prop_get_var_integers (bundle, vt_key, 1, &var1, nullptr, nullptr);
      task_run_change_score_action (game, task, var1);
      break;

    case 5:                    /* Execute/unset task. */
      prop_get_var_integers (bundle, vt_key, 2, &var1, &var2, nullptr);
      status = task_run_set_task_action (game, var1, var2);
      break;

    case 6:                    /* End game. */
      prop_get_var_integers (bundle, vt_key, 1, &var1, nullptr, nullptr);
      status = task_run_end_game_action (game, var1);
      break;

    case 7:                    /* Change battle attribute. */
      prop_get_var_integers (bundle, vt_key, 3, &var1, &var2, &var3);
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
scr_bool
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

  if (action_count > 0 && tasks.trace)
    {
      scr_trace ("Task: task %ld running %ld action%s\n",
                task, action_count, action_count == 1 ? "" : "s");
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
          if (tasks.trace)
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

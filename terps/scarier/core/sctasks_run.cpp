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
 * Running a task: NPC walk alerts, the room description rules,
 * task_run_task_unrestricted() and the recursion-checked task_run_task().
 */

#include "sctasks_internal.h"

/*
 * Tasks can run other tasks, leading to the possibility of an infinite loop
 * in the task calling sequence.  It's a game error, and we'll apply a limit
 * to the task recursion depth to try and catch it more controllably than
 * waiting for memory exhaustion.
 */
enum { TASK_MAXIMUM_RECURSION = 128 };


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
      if (!gs_npc_valid (game, npc)
          || walk < 0 || walk >= gs_npc_walkstep_count (game, npc))
        continue;
      npc_start_npc_walk (game, npc, walk);
    }
}


/*
 * task_suppresses_additional_message()
 *
 * TRUE if this Runner would drop the task's AdditionalMessage because the
 * turn's output already ends in two spaces.  3.80 games only.
 *
 * The Runners build a turn's text by concatenating onto one string, two
 * spaces between the pieces, and check first whether the string already ends
 * in a separator.  run380 bundled that check with the "is there a message at
 * all" one and put the append inside both, so an AdditionalMessage is lost
 * whenever what came before ended in two spaces:
 *
 *   0004D001  If AdditionalMessage <> "" And Right(out, 2) <> "  " Then
 *   0004D04F      out = out & "  " & AdditionalMessage
 *   0004D074  End If
 *
 * Its neighbours in the same sub are written correctly -- the CompleteText at
 * 0004CF26 and the room description at 0004CFB4 guard only the separator, and
 * append (or call viewroom) either way.  run370 at 00041C60 has no check at
 * all and always appends.  3.90 and 4.00 moved the test into a sub of its own
 * (Form1.pspace at 0002C880 in run390, General.Sub_22_58 at 0004A948 in
 * run400) that the callers invoke before appending, which fixes it.
 *
 * Measured live in run380 on jb2000.taf, whose task 14 ends its CompleteText
 * with 42 spaces and whose AdditionalMessage the Runner never shows.  Probes
 * hijacking its four unrestricted tasks pin the rule down: one trailing space
 * prints, two or more do not; a trailing tab prints; 42 trailing dots print,
 * so it is not length; spaces in the middle of the text print; leading spaces
 * on the AdditionalMessage itself change nothing; and turning on "show room
 * description" restores the message, because the room description lands in
 * between and the string no longer ends in spaces.  See
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.
 */
static scr_bool
task_suppresses_additional_message (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_taf_version (bundle) != TAF_VERSION_380)
    return FALSE;

  return pf_ends_with_double_space (gs_get_filter (game));
}


/*
 * task_show_room_desc()
 *
 * Append the task's ShowRoomDesc room name, description and exits, if it has
 * one.  Returns TRUE if anything was printed.
 *
 * The Runner emits this BEFORE it runs the task's actions, not after, so
 * the description shows the world as it stood when the task matched -- with
 * the one 4.0 exception in task_defers_room_desc().  Probe
 * SRD in test/adrift4/harness/make_arena_probe.py measured all three sides of
 * it live on run400:
 *
 *   alpha    move a room object into the player's hands, then show the room
 *            -> "Also here is a widget." is STILL listed
 *   beta     move a held object into the room, then show the room
 *            -> the gizmo is NOT listed
 *   gamma    redirect to a task whose CompleteText is "DELTA.", then show the
 *            room -> the room text comes first, "DELTA." after it
 *
 * epsilon adds an AdditionalMessage and pins the whole emission order down:
 * CompleteText, room description, action output, AdditionalMessage.  The
 * wild one in the corpus is Space Boy task 24 ("{take/get} {them/goggles}",
 * ShowRoomDesc = Treasure Island, action = move the goggles to held-by-
 * player), where run400 duly reprints "Resting on the platform is an odd
 * looking pair of goggles." for goggles the player is by then holding.
 *
 * Printing here also keeps the description out of the muting that
 * task_run_task_actions() applies once an action ends the game, which is why
 * topaz.taf task 22 shows its room ahead of the ending.
 *
 * The call sits ahead of the caller's buffer transfer, so the room text rides
 * along with the CompleteText: one paragraph break between the two, and both
 * interpolated at the final flush.  Emitting it after the transfer instead
 * would interpolate it with the values in effect pre-action, but it would also
 * land in an empty filter and so lose that paragraph break -- 120 corpus
 * walkthroughs' worth of blank line.  Nothing has measured which set of
 * variable values run400 uses here.
 */
static scr_bool
task_show_room_desc (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int showroomdesc;

  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "ShowRoomDesc";
  showroomdesc = prop_get_integer (bundle, "I<-sis", vt_key);
  if (!gs_room_valid (game, showroomdesc - 1))
    return FALSE;

  /*
   * Pre-3.9 Runners have no room-name heading anywhere -- see the
   * "showshortroom" note on lib_describe_player_room().  Measured on the same
   * run380 akron.taf replay, whose task 'west' into the factory shows the room
   * with no heading: "You step into the factory itself.  Machines clang all
   * around you. ..."
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    lib_print_room_name (game, showroomdesc - 1);
  lib_print_room_description (game, showroomdesc - 1);
  /*
   * The run400 room builder appends the exits list itself when the
   * ShowExits global is set (@00472BFF in Proc_19_63_472CA4), so
   * task-driven room displays include it too.
   */
  lib_print_room_exits (game, showroomdesc - 1);

  /*
   * The newline terminating the block above is ours, not the Runner's --
   * their flat output string simply stops where the description stopped, so
   * run380's AdditionalMessage double-space test (see
   * task_suppresses_additional_message()) reads the description's own tail.
   * Note the terminator so pf_ends_with_double_space() can look through it.
   * Measured live on superliam.taf (run380, 2026-08-31): tasks 23/24 show
   * rooms whose Long ends "  ", and the Runner drops both AdditionalMessages.
   */
  pf_note_trailing_auto_break (gs_get_filter (game));
  return TRUE;
}


/*
 * task_defers_room_desc()
 *
 * TRUE when the task's ShowRoomDesc block is built AFTER its actions have run
 * rather than before them.
 *
 * Probe SRD4 (make_arena_probe.py, transcript runner_probes/srd4.run400.txt,
 * run400 2026-09-07) walks the five field differences between probe SRD3 --
 * which showed that no SHAPE difference moves the block -- and lca.taf's
 * task 237.  Every cell has the same ShowRoomDesc = Back Room and the same
 * two actions, "Bob -> Store" then "player -> Back Room", so a cell that
 * omits "Bob is here, looking dangerous." is one whose block was built after
 * the moves:
 *
 *   b0  CompleteText, nothing else                Bob listed
 *   b1  empty CompleteText                        Bob listed
 *   b2  empty CompleteText + AdditionalMessage    Bob NOT listed
 *   b3  CompleteText + AdditionalMessage          Bob listed
 *   b4  Repeatable = 0                            Bob listed
 *   b5  Where = one room                          Bob listed
 *   ne  all five at once (task 237 to the letter) Bob NOT listed
 *
 * So it is neither field alone: the block moves behind the actions exactly
 * when the task has NO CompleteText and a NON-EMPTY AdditionalMessage.  Read
 * against the Runner's one-string room block (lib_print_room_description()),
 * the shape is that with no CompleteText to carry it out the description
 * rides along with the AdditionalMessage instead, which is emitted after the
 * actions -- and it still precedes that message, joined to it by the ordinary
 * "  " the way b3's does.
 *
 * The two corpus rows this closes pull in opposite directions, which is what
 * makes the pair the rule rather than a fit:
 *
 *   lca T252 (`ne`)         task 237, no CompleteText, an AdditionalMessage,
 *                           ShowRoomDesc = Haunted House, actions move the
 *                           player in and Daisy out -- run400 DROPS "The ever
 *                           alluring Daisy is here.", scarier kept it.
 *   ghosttown T19 (`u`)     task 129 `{go} [u/up]`, CompleteText "",
 *                           AdditionalMessage "   ", ShowRoomDesc = the
 *                           Kitchen, actions move Ninette in and then the
 *                           player in -- run400 LISTS "Ninette is here.",
 *                           scarier did not.
 *
 * ghosttown's AdditionalMessage is three spaces and prints nothing visible,
 * so the test is on the FIELD, not on whether the message shows -- and, since
 * scr_strempty() is whitespace-blind, not on scr_strempty() either.  Both
 * halves are the Runner's own `<> ""`: raw emptiness, VB's test on the string
 * it read out of the .taf.  (Only the AdditionalMessage half of that is
 * measured; nothing in the corpus has a whitespace-only CompleteText for the
 * other half to bite on, and reading them the same way is the assumption.)
 *
 * 4.0 only.  Nothing has measured the pre-4.0 Runners here, and their
 * AdditionalMessage handling is entangled with the room block in its own way
 * already (see task_suppresses_additional_message(), where the 3.8 double
 * space test reads the description's tail).
 */
static scr_bool
task_defers_room_desc (scr_gameref_t game,
                       const scr_char *completetext,
                       const scr_char *additionalmessage)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_taf_version (bundle) < TAF_VERSION_400)
    return FALSE;

  return completetext[0] == '\0' && additionalmessage[0] != '\0';
}


/*
 * task_run_task_reverse()
 *
 * Print any reverse message for the task, and undo the task.
 */
static scr_bool
task_run_task_reverse (scr_gameref_t game, scr_int task)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool status = FALSE;
  const scr_char *reversemessage;

  /*
   * If not yet done, we can hardly reverse it -- except that run390's
   * reverse_task (4283C8) has no such test, and its checktask hands it a
   * repeatable task whether done or not (probe p39REV `unpoke`).
   */
  if (gs_task_done (game, task)
      || (prop_get_taf_version (bundle) == TAF_VERSION_390
          && task_is_repeatable (game, task)))
    {
      reversemessage = prop_get_indexed_string (bundle, "Tasks", task,
                                                "ReverseMessage");
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


/*
 * task_run_task_repeat_refusal()
 *
 * Answer an attempt to repeat a task that is done and not repeatable.
 */
static scr_bool
task_run_task_repeat_refusal (scr_gameref_t game, scr_int task)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (!task_repeattext_is_empty (game, task))
    {
      if (tasks.trace)
        {
          scr_trace ("Task:"
                    " trying to repeat completed action, aborting\n");
        }

      pf_buffer_paragraph_line (filter,
                                prop_get_indexed_string (bundle,
                                                         "Tasks", task,
                                                         "RepeatText"));
      return TRUE;
    }

  /*
   * Task done, yet not repeatable, so don't consider this case
   * handled.
   */
  return FALSE;
}


/*
 * task_print_complete_text()
 *
 * Print any task completion text; TRUE if there was some.
 *
 * 4.0 tests the field raw, VB's `<> ""`, so a CompleteText of nothing but
 * spaces still counts as the task saying something: it lands in the
 * message buffer, the dispatcher at 48A481 reports the line handled, and
 * the tail's `48B573: If MemVar_4941B0 = "" ...` DontUnderstand never
 * fires.  wumpusrun task 167 (`climb ladder`, CompleteText " ", then End
 * Game) is the measured case: runner_transcripts/wumpusrun.txt shows the
 * lone space on its own line and then the WinText, where iachini task 30
 * (CompleteText "") gets "I don't understand what you mean!" first.
 * Probe .tafs without the task's variable, Execute Task and restriction
 * fail texts (runner_transcripts/wumpusrun.txt,
 * runner_probes/wumpusrun.run400.c.txt,
 * runner_probes/wumpusrun.run400.d.txt) all kept the silence.
 */
static scr_bool
task_print_complete_text (scr_gameref_t game, const scr_char *completetext)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_taf_version (bundle) >= TAF_VERSION_400
      ? completetext[0] != '\0' : !scr_strempty (completetext))
    {
      /*
       * A task the engine dispatched joins its text onto the turn's string
       * with pspace(), so an ALR can span it (see pf_buffer_join_line()).
       * That is the Runner's execute_task mode 1, and it covers an event's
       * task, a walk's, a battle's and the one an "execute task" action runs
       * -- see task_push_dispatched_run().  The hidden prefix says the same
       * thing for the action case at 4.0, where the turn's text stays in the
       * buffer
       * behind the barrier while the actions run; keep it, because the
       * refusal a 4.0 put leaves pending (pf_buffer_join_pending) reaches
       * here through the typed line's own matcher.
       */
      if (task_in_dispatched_run ()
          || (prop_get_taf_version (bundle) >= TAF_VERSION_400
              && pf_has_hidden_prefix (filter)))
        pf_buffer_join_line (filter, completetext);
      else
        pf_buffer_paragraph_line (filter, completetext);
      return TRUE;
    }
  return FALSE;
}


/*
 * task_run_task_actions_buffered()
 *
 * Start the NPC walks the task alerts and run its actions, if any, with
 * the turn's text so far kept out of their way.
 */
static scr_bool
task_run_task_actions_buffered (scr_gameref_t game, scr_int task)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int action_count;
  scr_bool status = FALSE;

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
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Actions";
  action_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  if (action_count > 0 && prop_get_taf_version (bundle) >= TAF_VERSION_400)
    {
      /*
       * Version 4.0 does none of that: the turn's text stays in the buffer
       * while the actions run, so a task an action runs filters it along with
       * its own (see pf_refilter()).  The probe's "uniform" task, whose
       * CompleteText is "CTU ball." and whose one action runs the "zulu" task,
       * answers "CTU qqqball." -- three walks, one of them zulu's.
       *
       * It is still hidden from the paragraph-spacing helpers, so that text an
       * action prints opens its paragraph exactly as it does pre-4.0; only the
       * filtering differs.  RAII, since the calls below can throw.
       */
      struct hide_guard
      {
        scr_filterref_t filter_;
        size_t previous_;
        explicit hide_guard (scr_filterref_t f)
          : filter_ (f), previous_ (pf_hide_prefix (f)) { }
        ~hide_guard () { pf_reveal_prefix (filter_, previous_); }
      } guard (filter);

      task_start_npc_walks (game, task);
      status |= task_run_task_actions (game, task);
    }
  else if (action_count > 0)
    {
      /*
       * Take ownership of the current filter buffer text, then start NPC
       * walks based on alerts, and run any and all task actions.  Note that
       * the buffer transferred out of the filter may be NULL if there is no
       * text currently in the filter.  RAII the transferred buffer: the
       * task_start_npc_walks() / task_run_task_actions() below can throw
       * (run_loop_halt / scr_fatal_error).
       */
      const scr_int printed_to = pf_printed_to (filter);
      scr_owned_string buffer (pf_transfer_buffer (filter));
      task_start_npc_walks (game, task);
      status |= task_run_task_actions (game, task);

      /*
       * Prepend the saved buffer data back onto the front of the filter,
       * and with it where a ShowRoomDesc's exits began (run370's second
       * matcher pass, run_takes_second_pass_370()).
       */
      if (buffer)
        {
          pf_prepend_string (filter, buffer.get ());
          pf_restore_printed_to (filter, printed_to);
        }
    }
  else
    {
      /* Start NPC walks only; there are no task actions. */
      task_start_npc_walks (game, task);
    }
  return status;
}


/*
 * task_print_additional_message()
 *
 * Print the task's AdditionalMessage, unless suppressed; TRUE if printed.
 */
static scr_bool
task_print_additional_message (scr_gameref_t game,
                               const scr_char *additionalmessage)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!scr_strempty (additionalmessage)
      && !task_suppresses_additional_message (game))
    {
      /*
       * It joins the turn's string with the two-space separator, at every
       * version: run370 441C78 appends "  " and the message outright, run380
       * 44D03F guards that with `Right(out, 2) <> "  "` (the superliam
       * suppression, see task_suppresses_additional_message), run390 43F1C5
       * calls pspace() at 43F1C8, and run400 does the same -- p4SRC.taf
       * prints "Y qqball.  ADD qqball." for a CompleteText and an
       * AdditionalMessage, on one line.  troll t17 (3.90) says it below 4.0:
       * `get in coach` ends with the coach room's block and then "The sun is
       * creeping up in the morning sky..." on the same line
       * (runner_transcripts/troll.txt).
       */
      pf_buffer_join_line (filter, additionalmessage);
      return TRUE;
    }
  return FALSE;
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
  scr_bool defer_room_desc, status;

  /* Start considering task output tracking. */
  status = FALSE;

  /* If reversing, print any reverse message, undo the task, and return. */
  if (!forwards)
    return task_run_task_reverse (game, task);

  /* See if we are trying to repeat a task that's not repeatable. */
  if (gs_task_done (game, task) && !task_is_repeatable (game, task))
    return task_run_task_repeat_refusal (game, task);

  /* Mark the task as done. */
  gs_set_task_done (game, task, TRUE);

  /* Print any task completion text. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "CompleteText";
  completetext = prop_get_string (bundle, "S<-sis", vt_key);
  status |= task_print_complete_text (game, completetext);

  /* Handle any task completion resource. */
  vt_key[2].string = "Res";
  res_handle_resource (game, "sis", vt_key);

  /*
   * Show the task's room, ahead of its actions -- unless this is one of the
   * 4.0 tasks that defers the block behind them; see
   * task_defers_room_desc().
   */
  vt_key[2].string = "AdditionalMessage";
  additionalmessage = prop_get_string (bundle, "S<-sis", vt_key);
  defer_room_desc = task_defers_room_desc (game, completetext,
                                           additionalmessage);
  if (!defer_room_desc)
    status |= task_show_room_desc (game, task);

  /* Run the task's actions, and start the NPC walks it alerts. */
  status |= task_run_task_actions_buffered (game, task);

  /*
   * The AdditionalMessage trails the actions, and is printed even when one of
   * them has just ended the game: measured live on topaz.taf (run400), whose
   * task 22 "wear ring" is a single "end game (win)" action with a room
   * description and "The two of you set out into the forest." as its
   * AdditionalMessage, and the Runner shows both, ahead of the ending.
   * marooned.taf task 29 says the same for run380.
   */
  if (defer_room_desc)
    status |= task_show_room_desc (game, task);

  status |= task_print_additional_message (game, additionalmessage);

  /*
   * A version 4.0 task that completes runs the output filter over the whole
   * of the turn's buffered text on its way out -- variables interpolated
   * where they stand and the ALR list walked -- and the turn's own flush then
   * filters what it leaves behind a second time.  That is the whole of the
   * "some text gets its ALRs applied twice" story; the measurements are in
   * pf_refilter().
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    pf_refilter (filter, gs_get_vars (game), bundle);

  /*
   * Pre-4.0 composes the ending as the task that armed it finishes, not at
   * the end of the turn: run380 tasks() appends WinText, the summary and
   * "[Press any key to end]" to the turn buffer right after the task's
   * action loop (44E2F9-44E4DE), and "[Press any key to end]" is no wait --
   * Text1_KeyDown only calls endgame() on the next key (43EAA5).  So a task
   * a walk runs from characters() ends the game mid-tick, and events() still
   * runs (the tail's ended test at 443176 was made before the tick) and
   * appends its text after the prompt.  haunt T84: Horace meets object 3,
   * task 23 wins, and the Clock event's chime follows "[Press any key to
   * end]" (runner_transcripts/haunt.rtf).  The end-of-turn call in the run
   * loop stays the 4.0 path.
   */
  if (prop_get_taf_version (bundle) < TAF_VERSION_400
      && game->pending_endgame != 0)
    task_print_end_game_message (game);

  /* Return status -- TRUE if matched and we output something. */
  return status;
}


/*
 * task_execution_count()
 *
 * How many tasks have passed their restrictions and gone on to run -- the
 * Runner's "a task ran for this line" flag (run390 MemVar_468198, run400
 * MemVar_4941F8, set in execute_task at 45A176) as a counter, so a caller
 * can compare two readings.  A task that only printed its FailMessage does
 * not count.  The counter is tasks.run.executions.
 */
scr_int
task_execution_count (void)
{
  return tasks.run.executions;
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

  if (tasks.trace)
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
      if (tasks.trace)
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
      if (tasks.trace)
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

  tasks.run.executions++;

  /* Check for infinite recursion. */
  if (recursion_depth > TASK_MAXIMUM_RECURSION)
    {
      scr_error ("task_run_task: maximum recursion depth exceeded --"
                " game task loop?\n");
      return FALSE;
    }

  /* Increment depth, run the task, then decrement depth -- also on a throw. */
  {
    struct depth_guard
    {
      scr_int &depth_;
      explicit depth_guard (scr_int &depth) : depth_ (depth) { depth_++; }
      ~depth_guard () { depth_--; }
    } guard (recursion_depth);

    status = task_run_task_unrestricted (game, task, forwards);
  }

  if (tasks.trace)
    {
      scr_trace ("Task: task %ld finished, return %s, depth %ld\n",
                task, status ? "true" : "false", recursion_depth);
    }

  /* Return the task's status. */
  return status;
}

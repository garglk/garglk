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
 * Restriction gating, strict and lenient matching, and running the tasks
 * a line matches: run_game_commands_common(), run_does_command_match(),
 * task dispatch for events and NPC walks.
 *
 * Split out of scrun_match.cpp; see scrun_match_internal.h.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "scrun_match_internal.h"


/*
 * run_task_restriction()
 *
 * Helper for run_game_commands_common().
 *
 * Adapter for uncovering task restriction state.  An unrestricted task can
 * run unimpeded; a loudly restricted one has a fail message that indicates
 * why it fails, and if run produces that message and changes no state; a
 * silently restricted one has no such message.  Restrictions that fail to
 * parse count as loudly restricted: the task must not run, and the loud
 * arm is the one that reports it.
 */
run_restriction_t
run_task_restriction (scr_gameref_t game, scr_int task)
{
  scr_bool restrictions_passed;
  const scr_char *fail_message;

  if (!restr_eval_task_restrictions (game, task,
                                     &restrictions_passed, &fail_message))
    {
      scr_error ("run_task_restriction: restrictions error, %ld\n", task);
      return RUN_RESTRICTED_LOUDLY;
    }

  if (restrictions_passed)
    return RUN_UNRESTRICTED;
  return fail_message ? RUN_RESTRICTED_LOUDLY : RUN_RESTRICTED_SILENTLY;
}


/*
 * run_task_is_silent_and_literal()
 *
 * First half of run_game_commands_common()'s exclude_silent_literal peek (see
 * run_all_commands(); the other half is run_task_reachable_by_library_
 * callback()).  Return TRUE for a task that (a) has no CompleteText of its
 * own to print and (b) has no wildcard-leading command pattern in either
 * direction -- i.e. a task that, if matched here, would run its actions with
 * no visible sign that it ran at all, and whose every pattern is specific
 * enough that the priority-command callback (run_game_task_commands(),
 * is_library=TRUE) could in principle get a look at it first.
 *
 * Silence alone is necessary but nowhere near sufficient, and must never be
 * used on its own: "Sommeril" task 35 ("take silver orb", +5, no message) is
 * silent and literal in exactly this sense, yet the Runner runs it and then
 * lets the library's own take answer "You are already carrying the SILVER
 * ORB." -- which is why the caller also requires that no library short form
 * can reach the pattern.  Both halves are load-bearing: dropping this one
 * costs 27 corpus walkthroughs, dropping the other costs 4.
 *
 * The case that needs excluding is "Space Boy's First Adventure" task 72,
 * "drop cape to the floor" (+250, no message) -- run400 lets the library's
 * ordinary drop win outright and the task never runs at all
 * (runner_probes/et2.run400.txt, runner_probes/et4.run400.txt, 2026-08-23).
 * The case that must NOT be excluded is that same game's task 27,
 * "{take/get}{them/boots}" (CompleteText "Taken."), which must still win over
 * the library's own take (run_v4_walkthroughs.sh space_boy golden, "Taken."
 * not "You take the pair of Flight Boots.").  Task 27 is kept safe here rather
 * than by reachability: the library only ever builds its callback string from
 * the object's display Short name ("get pair of Flight Boots"), never from an
 * alias like "boots", so 27 looks unreachable too -- but it has real
 * CompleteText and so is never silent, and priority's generic take never gets
 * to steal it.
 *
 * "No visible sign that it ran" has to cover every way task_run_task_
 * unrestricted() (sctasks_run.cpp) can print, not just CompleteText: a
 * ShowRoomDesc auto room-description or an AdditionalMessage are both
 * static per-task properties read the same cheap way, and both print
 * unconditionally when set (see the comment on that code, topaz.taf task 22
 * and marooned.taf task 29).  Found live on "Princess in the Tower"
 * (princess1.taf): task 15, the win task for walking into the tower, has
 * empty CompleteText but a non-zero ShowRoomDesc, so the original
 * CompleteText-only version of this check misclassified it as silent,
 * excluded it from the peek, and let task 18's refusal (identical literal
 * pattern "in") win the peek instead -- breaking the golden.  Also exclude
 * any task with an Execute-Task (action type 5) or End-Game (action type 6)
 * action: task_run_task_action() (sctasks_action.cpp) is the only place besides
 * CompleteText/ShowRoomDesc/AdditionalMessage that can make a task's status
 * come back TRUE, and only those two action types ever set it -- an
 * End-Game action prints a score summary/ending text of its own, and an
 * Execute-Task action cascades into another task whose own output can't be
 * predicted from here.  The other action types (move object, move NPC,
 * change object status, change variable, change score, change battle
 * attribute) never do.  A task tripping either check just falls through to
 * a full, unexcluded try in the next pass, same as any wildcard-leading
 * task -- this is a conservative "don't know, so don't peek" exclusion, not
 * a correctness requirement in itself.
 */
/*
 * run_task_reachable_by_library_callback()
 *
 * Second half of the exclude_silent_literal test (see run_all_commands()).
 * Return TRUE if the library's own priority-command callback could ever put
 * this task's pattern in front of run_game_task_commands() -- that is, if
 * some object's canonical "verb OBJECT" short form (the only shape
 * lib_try_game_command_common() ever constructs: "<verb> <Prefix> <Short>"
 * and, prefix dropped, "<verb> <Short>") matches one of the task's patterns.
 *
 * This is what actually separates the two run400-probed cases the peek has
 * to tell apart, and it is a property of the *pattern*, not of the task's
 * silence.  "Space Boy's First Adventure" task 72 is "drop cape to the
 * floor": the library's short forms are "drop the cape" and "drop cape", and
 * neither matches a pattern that insists on the trailing "to the floor", so
 * the Runner's library claims the command outright and the task never runs
 * (runner_probes/et2.run400.txt, runner_probes/et4.run400.txt, 2026-08-23).
 * "Sommeril" task 35 is the bare
 * "take silver orb", which IS the library's own short form for that object,
 * so the Runner runs it (silently, +5) and the library's take then answers
 * "You are already carrying the SILVER ORB." -- the shape the golden records.
 * Excluding the latter along with the former loses the task and five points;
 * see the corpus note in run_all_commands().
 *
 * uip_match() binds game object references as it goes, and this runs at the
 * moment a real match has already bound them for the task about to run, so
 * save and restore them around the probe -- same contract
 * lib_try_game_command_common() keeps for its own speculative matches.
 */
static scr_bool
run_task_reachable_by_library_callback (scr_gameref_t game, scr_int task,
                                        const scr_char *string,
                                        scr_bool is_forwards)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, is_forwards);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> references (game->object_references);
  const scr_char *verb;
  scr_int verb_length, object;
  scr_bool is_reachable = FALSE;

  /* Isolate the verb the player typed; no verb, nothing to construct. */
  verb = string + strspn (string, WHITESPACE);
  verb_length = strcspn (verb, WHITESPACE);
  if (verb_length == 0)
    return FALSE;

  for (object = 0; object < gs_object_count (game) && !is_reachable; object++)
    {
      scr_vartype_t vt_key[3];
      const scr_char *prefix, *name;
      std::string candidates[2];
      scr_int form;

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Prefix";
      prefix = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = "Short";
      name = prop_get_string (bundle, "S<-sis", vt_key);
      if (scr_strempty (name))
        continue;

      candidates[0] = std::string (verb, verb_length) + " " + name;
      candidates[1] = std::string (verb, verb_length) + " "
                      + (prefix ? prefix : "") + " " + name;

      for (form = 0; form < 2 && !is_reachable; form++)
        {
          scr_int command;

          for (command = 0; command < (scr_int) patterns.size (); command++)
            {
              if (uip_match (patterns[command], candidates[form].c_str (),
                             game))
                {
                  is_reachable = TRUE;
                  break;
                }
            }
        }
    }

  game->object_references = references;
  return is_reachable;
}


static scr_bool
run_task_is_silent_and_literal (scr_gameref_t game, scr_int task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  const scr_char *completetext, *additionalmessage;
  scr_int direction, showroomdesc, action_count, action;

  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "CompleteText";
  completetext = prop_get_string (bundle, "S<-sis", vt_key);
  /* 4.0: raw, as task_run_task_unrestricted() tests it. */
  if (run_get_version (bundle) == TAF_VERSION_400
      ? completetext[0] != '\0' : !scr_strempty (completetext))
    return FALSE;

  vt_key[2].string = "ShowRoomDesc";
  showroomdesc = prop_get_integer (bundle, "I<-sis", vt_key);
  if (showroomdesc != 0)
    return FALSE;

  vt_key[2].string = "AdditionalMessage";
  additionalmessage = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (additionalmessage))
    return FALSE;

  vt_key[2].string = "Actions";
  action_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (action = 0; action < action_count; action++)
    {
      scr_int type;

      vt_key[3].integer = action;
      vt_key[4].string = "Type";
      type = prop_get_integer (bundle, "I<-sisis", vt_key);
      if (type == 5 || type == 6)
        return FALSE;
    }

  for (direction = 0; direction < 2; direction++)
    {
      const scr_bool is_forwards = !direction;
      const std::vector<const scr_char *> &patterns =
          run_task_command_patterns (game, task, is_forwards);
      scr_int command;

      for (command = 0; command < (scr_int) patterns.size (); command++)
        {
          const scr_char *pattern = patterns[command];
          const scr_int first = strspn (pattern, WHITESPACE);

          if (pattern[first] == WILDCARD_PATTERN)
            return FALSE;
        }
    }
  return TRUE;
}


/*
 * run_game_commands_common()
 * run_game_commands_in_parser_context()
 *
 * The central handler for running, or at least trying to run, game-defined
 * tasks that have commands that match the input string.  Here's the algorithm
 * as currently understood (and it may not be right, so be warned):
 *
 *  for each task executable in the current room
 *    for direction in forwards, backwards
 *      for each command string defined by the task for this direction
 *        match against player input
 *      if any command string matched player input
 *        if task restrictions pass
 *          run the task actions in the current direction
 *          if the task actions produced output
 *            return
 *          is_matched := true
 *          break out of all loops
 *
 *  if not is_matched and we're allowing restrictions to fail tasks
 *    for each task executable in the current room
 *      for direction in forwards, backwards
 *        for each command string defined by the task for this direction
 *          match against player input
 *        if any command string matched player input
 *          if task restrictions fail with an error message
 *            run the task, to persuade it to print this error message
 *            return
 *
 * Part of the fun and games is that run_game_task_commands() is called by the
 * library to try to run "get " and "drop " game commands for standard get/drop
 * handlers and get_all/drop_all handlers.  No pressure, then.
 *
 * exclude_silent_literal makes the first (unrestricted) loop below a "peek":
 * a task that matches, would win, and is both silent (run_task_is_silent_and_
 * literal()) and unreachable from the library's own callback string
 * (run_task_reachable_by_library_callback()) does not win -- and, crucially,
 * neither does any task after it.  The whole pass is abandoned and returns
 * FALSE, because ADRIFT task precedence is "lowest matching index wins": a
 * task that matched cannot be stepped over in favour of a later one without
 * inventing a match the Runner never makes.  See run_all_commands().  This
 * has no effect on the second (loudly-restricted) loop, which is about
 * failing-restriction messages, not CompleteText.
 */
/*
 * run_task_run_speaks()
 *
 * Run a matched task and say whether the line counts as answered.  3.9 and
 * 4.0 test the turn's message buffer, not the task's own text: run390 tasks()
 * saves msg on entry and reports handled when it has changed (42BDAD), and
 * run400's dispatcher reports handled when the buffer is non-empty (44CCC0).
 * So text printed by anything the task's actions set off counts -- an event
 * an execute-task action starts prints its StartText into the same buffer
 * (evt_check_events_started_by_task()).  baroo (4.00, runner_transcripts/
 * baroo.txt T107): `close machine` runs task 113, which has no CompleteText;
 * its execute-task action starts the convertor event, and the Runner prints
 * that StartText alone, where the library close used to follow it with "The
 * machine is now closed.".
 */
static scr_bool
run_task_run_speaks (scr_gameref_t game, scr_int task, scr_bool is_forwards)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t length = pf_buffer_length (filter);

  if (task_run_task (game, task, is_forwards))
    return TRUE;

  return run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390
         && pf_buffer_length (filter) > length;
}

/*
 * run_restriction_cache_task_pick()
 *
 * The restriction-cache half of run400's task_pick Proc_19_66_454EF0, as the
 * dispatcher 44CCE0 calls it for the typed line (@44CBDB): tasks in index
 * order that pass the class filter, are in scope for the room and whose
 * state allows a run -- including a spent one with a RepeatText -- have
 * their restrictions walked with arg_10 = 1 (@454DDF) BEFORE the pattern
 * match, so a failing walk rewrites the task's cached results, and the
 * look-up stops at the first task that passes and matches.  Tasks after it
 * keep whatever they last cached.  See restr_cache_fallback().
 */
void
run_restriction_cache_task_pick (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int task_count = gs_task_count (game);
  scr_int task;

  for (task = 0; task < task_count; task++)
    {
      const scr_bool forwards = task_can_run_task_directional (game, task,
                                                               TRUE);
      const scr_bool reverse = task_can_run_task_directional (game, task,
                                                              FALSE);
      const scr_bool spent = task_is_done_refused (game, task)
                             && !scr_strempty (prop_get_indexed_string (
                                    bundle, "Tasks", task, "RepeatText"));
      const scr_char *fail_message;
      scr_bool pass;

      if (!run_task_passes_class_filter (game, task)
          || !task_where_allows_run (game, task))
        continue;
      if (!forwards && !reverse && !spent)
        continue;

      if (!restr_eval_task_restrictions_cached (game, task,
                                                &pass, &fail_message)
          || !pass)
        continue;

      if (((forwards || spent)
           && run_match_task_commands (game, task, string, TRUE, FALSE))
          || (reverse
              && run_match_task_commands (game, task, string, FALSE, FALSE)))
        break;
    }
}


/*
 * run_line_matches_task_strictly()
 *
 * Deliberate deviation, the gate for it (2026-09-27).  TRUE if any task the
 * player could run here has a command that matches the line the Runner's
 * way.  When none does, run_all_commands() gives the whole line lenient
 * task matching: the tolerant %object% matcher Scarier had before the
 * Runner ports -- articles, Prefixes, aliases and case forgiven, no seen
 * gate, no pre-4.0 substitute-then-compare -- a forgiven trailing space in a
 * command (uip_set_lenient_tasks()), and, in the library's retries, the
 * canonical verb and the pre-4.0 Prefix form (lib_try_game_command_common()).
 * Every line the Runner matches keeps the Runner's answer; only lines it
 * would turn away from every task are looked at again.
 *
 * The walk is a peek: the references it binds are put back, so the real
 * dispatch starts from the state it always did.
 *
 * With EXCEPT, a task counts only if no command of it matches EXCEPT too,
 * so a catch-all task that would take either line is passed over.  With
 * ANYWHERE, a task counts whatever room it is confined to: the question is
 * then whether the game has such a task at all, not whether it runs here.
 */
scr_bool
run_line_matches_task_strictly (scr_gameref_t game, const scr_char *string,
                                const scr_char *except, scr_bool anywhere)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const std::vector<scr_bool> object_references = game->object_references;
  const std::vector<scr_bool> npc_references = game->npc_references;
  const scr_int ref_object = var_get_ref_object (vars);
  const scr_int ref_character = var_get_ref_character (vars);
  const scr_int ref_number = var_get_ref_number (vars);
  const std::string ref_text (var_get_ref_text (vars));
  const scr_int task_count = gs_task_count (game);
  scr_bool matched = FALSE;
  scr_int task;

  for (task = 0; task < task_count && !matched; task++)
    {
      if (!anywhere && !task_where_allows_run (game, task))
        continue;
      matched = run_match_task_commands (game, task, string, TRUE, FALSE)
                || run_match_task_commands (game, task, string, FALSE, FALSE);
      if (matched && run_passing_only)
        {
          /* After the match, so a restriction on %object% sees the binding. */
          scr_bool pass;
          const scr_char *fail_message;
          matched = restr_eval_task_restrictions (game, task, &pass,
                                                  &fail_message)
                    && pass;
        }
      if (matched && except)
        matched = !run_match_task_commands (game, task, except, TRUE, FALSE)
                  && !run_match_task_commands (game, task, except,
                                               FALSE, FALSE);
    }

  game->object_references = object_references;
  game->npc_references = npc_references;
  var_set_ref_object (vars, ref_object);
  var_set_ref_character (vars, ref_character);
  var_set_ref_number (vars, ref_number);
  var_set_ref_text (vars, ref_text.c_str ());
  return matched;
}


#ifdef SCARIER_DUMP_TOOLS
/*
 * run_strict_tasks_requested()
 *
 * Dump tools: TRUE when STRING gets no leniency at all -- every line under
 * SCR_STRICT_TASKS or run_probe_strict (the os_ansi probe's strict child),
 * or the one line SCR_STRICT_LINE names.  To see what the Runner-way run
 * answers instead.
 */
scr_bool run_probe_strict = FALSE;

scr_bool
run_strict_tasks_requested (const scr_char *string)
{
  static const scr_bool strict_only = getenv ("SCR_STRICT_TASKS") != nullptr;
  static const scr_char *const strict_line = getenv ("SCR_STRICT_LINE");

  return strict_only || run_probe_strict
         || (strict_line && scr_strcasecmp (strict_line, string) == 0);
}
#endif


/*
 * run_line_yields_to_library()
 *
 * Deliberate deviation, narrowed (2026-09-30).  TRUE for a line no task
 * matches the Runner's way, that some task matches leniently, but only by a
 * command that starts with '*' -- one that does not name the verb.  Such a
 * command, read by the tolerant matcher, takes any line holding its words:
 * ShadricksUnderground task 16's ` * in * ` ran on `look in black jar` in
 * the cellar and walked the player into the tunnel, and The X-Files'
 * ` *Buzzer*` would ring the buzzer on `x buzzer`.  run_all_commands() gives
 * that line to the Runner's own dispatch first and keeps its answer unless
 * the library has nothing for it (lib_non_answer, or no claim at all), so
 * the lenient task still has `buzzer` -- "I don't understand what you want
 * me to do with the Buzzer." in every Runner -- but not `look in black jar`.
 * Found by run_lenient_census().
 *
 * A command that names the verb yields too when its task's restrictions
 * fail, since the task would only print its FailMessage: rocky's `get chain`
 * before the necklace is in reach answered "what gold necklace?" where the
 * Runner's way says "Take what?".  When the library has nothing either
 * (lib_non_answer), the author's refusal stands.  Found by probe_lenient.sh.
 */
scr_bool
run_line_yields_to_library (scr_gameref_t game, const scr_char *string)
{
  scr_bool yields = FALSE;

#ifdef SCARIER_DUMP_TOOLS
  if (run_strict_tasks_requested (string))
    return FALSE;
#endif

  const scr_lenient_tasks_guard strict_match (FALSE);
  if (!run_line_matches_task_strictly (game, string))
    {
      const scr_lenient_tasks_guard lenient_match (TRUE);

      run_named_verb_only = run_passing_only = TRUE;
      yields = !run_line_matches_task_strictly (game, string);
      run_named_verb_only = run_passing_only = FALSE;
      yields = yields && run_line_matches_task_strictly (game, string);
    }
  return yields;
}


scr_bool
run_lenient_task_matching (void)
{
  return uip_get_lenient_tasks ();
}


/*
 * Set while run_takes_second_pass_370() runs the matcher a second time for
 * the same typed line, which the one-task-per-line rule below would refuse.
 */
scr_bool run_matcher_second_pass = FALSE;

/*
 * One call of run_game_commands_common(): its arguments, what it has found
 * out about the tasks so far, and the two passes over them.
 *
 * Matching is expensive, so it helps to use a cache of results from the
 * first loop in the second.  If we're using the second, that is.  The cache
 * stays empty when restrictions are off (the second loop is then skipped).
 */
struct run_commands_t
{
  scr_gameref_t game;
  const scr_char *string;
  scr_bool include_restrictions, is_library, exclude_silent_literal;
  scr_bool is_restriction_first;
  scr_bool is_matched, is_handled, is_abandoned;
  std::vector<scr_bool> is_matching;

  void run_pass (void);
  void restriction_pass (void);
};

/*
 * run_commands_t::run_pass()
 *
 * Iterate over every task, ignoring those not runnable.  For each runnable
 * task, try matching task commands, and on matches, check restrictions and
 * if they pass, try running the task.
 *
 * Spent tasks (done, non-repeatable, but still in their rooms) also get
 * their forwards commands matched -- not to run them, but to seed the
 * cache for the loud restriction-failure pass below.  The Runner checks a
 * matched task's restrictions before its done state, so a spent task whose
 * restrictions fail with a message still prints that message (run400,
 * Provenance's squeeze-through-the-hole task: a second "s" at the hole
 * re-prints "The only way you are going to make it through that hole is if
 * you drop everything you are carrying.", 2026-08-22).  A spent task whose
 * restrictions PASS instead falls through to run_task_refusal(), which
 * answers with RepeatText or "You have already done that.".
 *
 * This holds on the library-callback path too, not just for plain game
 * commands: probe DONE, task `* get * gem *` with a "holding the stone"
 * restriction, run400 2026-08-23 -- once the task is spent, `get gem`
 * without the stone answers "BLOCK-GEM." instead of taking the gem, and
 * with the stone falls through to the library take ("Player take the
 * gem.").  So `is_library` does not gate the spent-task match here.
 *
 * All of that is 4.0 only.  The 3.9 twin of the probe (p39done.taf, built
 * by test/adrift4/harness/make_39_doneprobe.py) says run390 orders the two
 * tests the other way about: a spent task answers "You have already done
 * that." whether its restrictions pass or fail, and never prints a fail
 * message (`alpha` and `x book` after dropping the stone, 2026-08-23).
 * Pre-4.0 therefore leaves spent tasks out of the loud restriction pass
 * entirely, and run_task_refusal() answers them.
 */
void
run_commands_t::run_pass (void)
{
  const scr_int task_count = gs_task_count (game);
  scr_int task, direction;

  for (task = 0; task < task_count; task++)
    {
      const scr_bool is_refusing = include_restrictions
                                   && is_restriction_first
                                   && !run_task_ran_this_command (task)
                                   && task_is_done_refused (game, task);

      if (!is_refusing && !task_can_run_task (game, task))
        continue;
      if (!run_task_passes_class_filter (game, task))
        continue;

      /*
       * Try matching forwards and reverse commands.  If there's a match for
       * unrestricted tasks, run the task, and if it runs (defined as printing
       * some game output), we're done; otherwise, note the command match but
       * keep searching for other possible matches.
       */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;
          const scr_bool is_runnable_directional =
              task_can_run_task_directional (game, task, is_forwards);

          if (!is_runnable_directional && !(is_refusing && is_forwards))
            continue;

          if (run_match_task_commands (game, task, string,
                                       is_forwards, is_library))
            {
              if (is_runnable_directional
                  && run_task_restriction (game, task) == RUN_UNRESTRICTED)
                {
                  /*
                   * In the peek pass, a silent, literal task does not win --
                   * but neither may any task after it.  Abandon the whole
                   * pass so that task precedence (lowest matching index wins)
                   * is preserved: priority commands get their look next, and
                   * if they don't claim the command the immediately following
                   * unexcluded pass re-matches from task 0.  Skipping just
                   * this task and reading on would hand the command to a
                   * later, lower-precedence task the Runner never reaches --
                   * "The Forum" task 1 (literal "x ... hand", silent, falls
                   * through to the library's own examine) losing to task 2
                   * ("[examine/...]{at}[%object%]", which has CompleteText of
                   * its own), and seven more corpus walkthroughs like it.
                   */
                  if (exclude_silent_literal
                      && run_task_is_silent_and_literal (game, task)
                      && !run_task_reachable_by_library_callback (
                             game, task, string, is_forwards))
                    {
                      is_abandoned = TRUE;
                      break;
                    }

                  run_note_task_ran (game, task);
                  if (run_task_run_speaks (game, task, is_forwards))
                    is_handled = TRUE;
                  is_matched = TRUE;
                  break;
                }

              if (!is_matching.empty ())
                is_matching[task] = TRUE;
            }
        }
      if (is_matched || is_abandoned)
        break;
    }
}

/*
 * run_commands_t::restriction_pass()
 *
 * If no match, and we've been asked to consider failing restrictions, look
 * through all of the runnable tasks again, this time searching for
 * restricted ones with a fail message.  Use the cache built above to weed
 * out matches that are certain to fail.
 */
void
run_commands_t::restriction_pass (void)
{
  const scr_int task_count = gs_task_count (game);
  scr_int task, direction;

  for (task = 0; task < task_count; task++)
    {
      scr_bool is_refusing;

      if (!is_matching[task])
        continue;

      /* Spent tasks are eligible here too (4.0); see the first loop. */
      is_refusing = is_restriction_first
                    && !run_task_ran_this_command (task)
                    && task_is_done_refused (game, task);

      /*
       * Check matches of forwards and reverse commands.  If there's a
       * match for restricted tasks (ones that have and will print a fail
       * message if we try to run them), run the task to get the print of
       * the fail message, and we're done.
       */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;

          if ((task_can_run_task_directional (game, task, is_forwards)
               || (is_refusing && is_forwards))
              && run_match_task_commands (game, task, string,
                                          is_forwards, is_library))
            {
              /*
               * 4.0: the dispatcher's fallback (45404C @44CCA5) answers
               * from the cached restriction results, printing the
               * FailMessage of the first cached failure that still
               * fails; see restr_cache_fallback().  Either way the task
               * is done with once a cached failure still fails.
               */
              if (is_restriction_first)
                {
                  const scr_char *fail_message;

                  if (restr_cache_fallback (game, task,
                                            &fail_message) < 0)
                    {
                      if (fail_message)
                        {
                          run_note_task_ran (game, task);
                          pf_buffer_paragraph_line (gs_get_filter (game),
                                                    fail_message);
                          is_handled = TRUE;
                        }
                    }
                  break;
                }

              if (run_task_restriction (game, task)
                  == RUN_RESTRICTED_LOUDLY)
                {
                  run_note_task_ran (game, task);
                  if (task_run_task (game, task, is_forwards))
                    {
                      is_handled = TRUE;
                      break;
                    }
                }
            }
        }
      if (is_handled)
        break;
    }
}

scr_bool
run_game_commands_common (scr_gameref_t game, const scr_char *string,
                          scr_bool include_restrictions, scr_bool is_library,
                          scr_bool exclude_silent_literal)
{
  run_commands_t pass;

  /*
   * The Runner dispatches ONE task per typed line.  run400's dispatcher
   * Proc_19_24_44CCE0 asks the pre-matcher (Proc_19_66_454EF0) for a single
   * task, runs it, and returns "handled" only if the message buffer is
   * non-empty (44CCC0); the restriction-failure pass (Proc_19_68_45404C)
   * runs only when no task was found at all (44CCA5).  A silent task
   * therefore lets the LIBRARY run, but never a second task.  run390's
   * tasks() (42BDC4) is the same shape: checktask picks one task, execute_task
   * runs it.  Scarier reaches the same line in several passes (peek, no
   * restrictions, restrictions), and the later passes used to re-scan from
   * task 0 with the first task now spent: House (4.00, 2026-09-06,
   * runner_probes/house.run400.kiss.txt) `kiss cathy` as the first line naming
   * Cathy runs the silent once-only task 200 `*cathy*`, and run400 then prints
   * the library's "I'm not sure she would appreciate that!" -- the game's own
   * kiss task 882 `[hug/kiss/touch/shake] [her/cathy]` only runs on the SECOND
   * kiss.  Scarier ran 200 and then 882 on the first line.  Library callbacks
   * (is_library) are left alone: those model the Runner's own pre-matcher
   * look-ups from inside the library handlers, which happen after the
   * dispatcher regardless.
   */
  if (!is_library && !run_matcher_second_pass
      && run_any_task_ran_this_command ())
    return FALSE;

  pass.game = game;
  pass.string = string;
  pass.include_restrictions = include_restrictions;
  pass.is_library = is_library;
  pass.exclude_silent_literal = exclude_silent_literal;
  pass.is_restriction_first =
      run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400;
  pass.is_matched = pass.is_handled = pass.is_abandoned = FALSE;
  if (include_restrictions)
    pass.is_matching.assign (gs_task_count (game), FALSE);

  pass.run_pass ();
  if (pass.is_abandoned)
    return FALSE;

  /*
   * 4.0 enters that pass only when the picker found no task at all, and
   * a spent task with a RepeatText whose restrictions pass IS one it finds
   * (see run_task_refusal()), however far down the table: British Fox's
   * `attack guard` after the basement guard is down answers task 385's
   * "The guard is already unconscious.", never task 331's "The jailors are
   * not here" (runner_transcripts/britishfox.txt turns 215-256).
   */
  if (!pass.is_handled && !pass.is_matched && include_restrictions
      && !(pass.is_restriction_first && run_repeat_found_400))
    pass.restriction_pass ();

  /* Return TRUE if any game task handled the command in some way. */
  return pass.is_handled;
}

scr_bool
run_game_commands_in_parser_context (scr_gameref_t game, const scr_char *string,
                                     scr_bool include_restrictions,
                                     scr_bool exclude_silent_literal)
{
  /*
   * Try game commands, either with or without restrictions, and all full and
   * complete parse matching (no special case for game commands that begin
   * with a '*' wildcard).
   */
  return run_game_commands_common (game, string, include_restrictions, FALSE,
                                   exclude_silent_literal);
}


/*
 * run_does_command_match()
 *
 * Non-destructive probe: return TRUE if the input string matches a command of
 * any currently runnable game task (forwards or reverse), without running it.
 *
 * This lets the front end give author-defined commands precedence over its own
 * conveniences.  In particular, the Glk port expands single letters such as
 * "c", "k" and "p" into "close", "attack" and "open"; that silently corrupts
 * games which use single letters as menu choices (battle/conversation menus,
 * e.g. attack choices in hyper_b_s.taf, or "C" in The PK Girl).  The port asks
 * here first, and skips its expansion when the game already recognises the raw
 * input.
 *
 * The string is run through the game's input synonyms before matching, exactly
 * as run_game_task_commands() does with real input.  Without that, a game that
 * routes a letter to its tasks indirectly looks unclaimed: "The Warlord, The
 * Princess & The Bulldog" maps "i" to the synonym "iii" and keys its inventory
 * task on "iii", so probing the raw "i" found nothing and the port expanded it
 * to "inventory" -- announcing "[i -> inventory]" for what the author had
 * already handled.
 *
 * Matching has the same incidental side effects as ordinary command matching
 * (it may set referenced-object/NPC/variable state via uip_match()), but this
 * is harmless: the probe runs before input is submitted, and the real command
 * pass that follows re-matches and overwrites that state.
 *
 * With check_restrictions set, the probe is run400's task pre-matcher
 * Proc_19_35_453C50 (mdlSpreadTheLoad.bas:26153) called with its second
 * argument 1, as the put handler's implicit-take gate calls it @46E2C7.  That
 * routine makes two passes over the tasks, and neither ignores restrictions:
 *
 *   1. @453B00-453C0B: a task in scope for the room whose state allows a run
 *      ((not done Or repeatable) Or (done And reversible)) is a hit only if
 *      restriction_walk Proc_19_64_455C60 PASSES and a pattern matches.
 *   2. Proc_19_68_45404C(1, mode) @453C34: a pattern-matching task in scope
 *      whose lowest failing restriction has a NON-EMPTY FailMessage
 *      (Proc_19_2_481DA0(task, i, 1) records it @481D99; the hit is the
 *      message buffer having changed, @453FA1 and @45403A), or, with no
 *      failing restriction, whose RepeatText (text index 2) is non-empty
 *      @453FE2-454028.  A failing restriction with an empty message drops
 *      the task and the walk moves on to the next one @453FC6->454034.
 *
 * Measured live 2026-09-06 on House.taf (runner_probes/house.run400.wood.txt,
 * runner_probes/house.run400.t92.txt, runner_probes/house.run400.t93.txt): at
 * the fireplace, "put wood in fireplace" with the wood on the floor matches
 * task 60 "* %object%" (a take-flagged task restricted to the house
 * spinning, FailMessage empty), and run400 still prints "(Taking the wood
 * first)" -- the restriction-blind probe used to count that task as a hit
 * and skip the take.
 */
static scr_bool
run_task_match_has_text (scr_gameref_t game, scr_int task,
                         scr_bool matched_reverse)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (matched_reverse)
    return !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                   "ReverseMessage"));
  return !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                 "CompleteText"))
         || !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                    "AdditionalMessage"))
         || (gs_task_done (game, task)
             && !prop_get_indexed_boolean (bundle, "Tasks", task, "Repeatable")
             && !scr_strempty (prop_get_indexed_string (bundle, "Tasks", task,
                                                        "RepeatText")));
}

scr_bool
run_does_command_match (scr_gameref_t game, const scr_char *string,
                        scr_bool check_restrictions, scr_int *match_kind)
{
  scr_int task_count, task, direction;

  /* The pre-matcher takes its caller's mode alone: name_object's unfiltered
     look-up still sees a non-put task on a held-object put line. */
  struct put_class_suspend
  {
    const scr_bool saved;
    put_class_suspend () : saved (run_put_class_only)
    { run_put_class_only = FALSE; }
    ~put_class_suspend () { run_put_class_only = saved; }
  } const suspend;

  /*
   * match_kind, when asked for, is the pre-matcher's own return value:
   * 1 for a hit whose matched direction has text to print, or a fallback
   * hit (a failing restriction's message, a spent task's RepeatText --
   * 453C34 stores 1 for both), and 2 for a first-pass hit on a task that
   * would run silently (453BEC-453BFE).  Callers such as the take piece
   * 46302C exit only on a 1; a 2 dispatches and carries on.  A hit on a
   * failing restriction's message is reported as 3 (the Runner's 1 still):
   * 45404C is called with arg_C=1 there and restores the message buffer, so
   * nothing prints, which the per-piece take refusal needs to know.
   */
  if (match_kind)
    *match_kind = 0;

  /* Only meaningful while a game is actually running. */
  if (!run_is_running (game))
    return FALSE;

  /*
   * Apply input synonyms, so indirection through a synonym still counts --
   * for the interface's probe of a raw typed line only.  The Runner's
   * pre-matcher 453C50 applies none: its typed line has been through the
   * synonym table already (run_player_input()), and a line the library
   * rebuilds reaches it as built.  S.E.R.E. (4.00) maps get -> take and has
   * task 40 "take *  sniper * rifle" (two spaces, so the typed line misses)
   * with the alternative "get * rifle": run400 answers `take sniper rifle`
   * in the barn with the task, which the take piece's "get the sniper rifle"
   * pre-matches (runner_transcripts/sere.txt T22).  Filtered, that line read
   * "take the sniper rifle" and the library took the rifle.
   */
  scr_owned_string filtered (check_restrictions
                             ? nullptr
                             : pf_filter_input (string, gs_get_bundle (game)));
  if (filtered)
    string = scr_normalize_string (filtered.get ());

  /*
   * With restrictions checked, the Runner's two passes are separate loops:
   * 453C50 walks every task for a first-pass hit and only then calls the
   * fallback 45404C (@453C34).  A task whose failing restriction has a
   * message therefore loses to ANY later task that passes -- professor.taf's
   * `take mail` rewrites to "get mail from mailbox on-a rope", which fails
   * task 7 `get * rope` loudly but is a silent first-pass hit further down,
   * so run400 takes the mail (runner_probes/professor.run400.txt).
   */
  if (check_restrictions)
    {
      scr_int pass_number;

      task_count = gs_task_count (game);
      for (pass_number = 0; pass_number < 2; pass_number++)
        {
          for (task = 0; task < task_count; task++)
            {
              const scr_char *fail_message;
              scr_bool matched_forwards, matched_reverse, pass;

              if (!run_task_passes_class_filter (game, task)
                  || !task_where_allows_run (game, task))
                continue;

              matched_forwards = run_match_task_commands (game, task, string,
                                                          TRUE, FALSE);
              matched_reverse = !matched_forwards
                                && task_can_run_task_directional (game, task,
                                                                  FALSE)
                                && run_match_task_commands (game, task, string,
                                                            FALSE, FALSE);
              if (!matched_forwards && !matched_reverse)
                continue;

              /*
               * 4.0: the fallback 45404C reads the task's cached restriction
               * results instead of walking; see restr_cache_fallback().
               */
              if (pass_number == 1
                  && run_get_version (gs_get_bundle (game))
                     >= TAF_VERSION_400)
                {
                  if (restr_cache_fallback (game, task, &fail_message) < 0)
                    {
                      if (!fail_message)
                        continue;
                      if (match_kind)
                        *match_kind = 3;
                      return TRUE;
                    }
                  if (scr_strempty (prop_get_indexed_string (
                          gs_get_bundle (game), "Tasks", task, "RepeatText")))
                    continue;
                  if (match_kind)
                    *match_kind = 1;
                  return TRUE;
                }

              if (!restr_eval_task_restrictions (game, task,
                                                 &pass, &fail_message))
                pass = TRUE, fail_message = nullptr;

              if (pass_number == 1)
                {
                  /* Fallback pass: the failing restriction has a message. */
                  if (!pass && fail_message)
                    {
                      /* 453C34's 1, told apart for the take refusal. */
                      if (match_kind)
                        *match_kind = 3;
                      return TRUE;
                    }
                  continue;
                }

              /*
               * First pass: a runnable task whose restrictions pass.  Our
               * state test also admits a spent task with a RepeatText,
               * which is the fallback pass's other hit.
               */
              if (pass && (matched_reverse
                           || task_can_run_task_directional (game, task, TRUE)))
                {
                  if (match_kind)
                    *match_kind = run_task_match_has_text (game, task,
                                                           matched_reverse)
                                  ? 1 : 2;
                  return TRUE;
                }
            }
        }
      return FALSE;
    }

  /* Iterate over every task, ignoring those not runnable. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      if (!run_task_passes_class_filter (game, task))
        continue;

      if (!task_can_run_task (game, task))
        continue;

      /* A match in either direction means the game claims this command. */
      for (direction = 0; direction < 2; direction++)
        {
          const scr_bool is_forwards = !direction;

          if (task_can_run_task_directional (game, task, is_forwards)
              && run_match_task_commands (game, task, string,
                                          is_forwards, FALSE))
            return TRUE;
        }
    }

  return FALSE;
}


/*
 * run_task_run_by_index()
 *
 * Run a task selected by its index rather than by matching input -- the 4.0
 * Runner's Sub_20_22.  Every one of that routine's callers goes through here:
 * an "execute task" action, an event running its TaskAffected, a walk's
 * CharTask or ObjectTask, and the battle system.
 *
 * The reason it is not simply task_run_task() is the preamble: before running
 * the task, run400 walks the task's *alternate* commands looking for a task
 * command function, and a getdynfromroom() found there sets the Referenced
 * Object for the run.  The task's primary command is not in the array it
 * scans, so a task whose only command is the function never evaluates it.
 * Verified live in run400 (test/adrift4/notes/RUNNER_TESTS_TODO.md section 9):
 * wrapping each probe in an "execute task" action is the only way to make its
 * function fire at all, and a probe carrying the function as its sole command
 * stays silent.
 */
scr_bool
run_task_run_by_index (scr_gameref_t game, scr_int task)
{
  scr_int command;

  /* An "execute task" action or event naming no task runs nothing. */
  if (!gs_task_valid (game, task))
    return FALSE;

  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);

  /*
   * Sub_20_22 hands every one of its tasks to execute_task with mode 1
   * (run400 45FA66), so the task's CompleteText joins the turn's string
   * rather than replacing it.  RAII because task_run_task() can throw.
   */
  struct dispatch_guard
  {
    dispatch_guard () { task_push_dispatched_run (); }
    ~dispatch_guard () { task_pop_dispatched_run (); }
  } guard;

  for (command = 1; command < (scr_int) patterns.size (); command++)
    {
      if (run_is_task_function (patterns[command], game))
        break;
    }

  /* The dispatch filter's walk (45FA02) caches a failing result. */
  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && game->is_running)
    {
      const scr_char *fail_message;
      scr_bool pass;

      restr_eval_task_restrictions_cached (game, task, &pass, &fail_message);
    }

  return task_run_task (game, task, TRUE);
}


/*
 * run_npc_walk_task()
 *
 * Run the task triggered by an NPC walk meeting a character or an object (a
 * walk's CharTask or ObjectTask).  The 3.9 Runner does not run this one task
 * by its stored index: it copies the task's command text into the input
 * global and calls the task matcher, instruction-for-instruction the same
 * dispatch its events use (Form1.characters at 0005AAD5/0005AB88 vs
 * Form1.checkevent at 00048D83, all three "copy tasks[n-1].command[0], call
 * Form1.tasks(1)"), so everything verified for event dispatch holds here
 * too: the first task in list order that matches and passes where +
 * restrictions fires, an earlier runnable `*` wildcard steals the execution
 * outright, and a restricted match is skipped silently.  Verified live in
 * run390 (test/adrift4/harness/make_39_walkprobe.py variants E/F/G): with a wildcard first
 * the arrival turn prints the wildcard's text twice and the walk task's
 * never, with the walk task first it fires itself, and a restricted walk
 * task prints nothing.  The matcher dispatch is also what fans one walk
 * trigger out across same-command tasks -- e.g. "Lair of the CyberCow" has
 * two "#lured" tasks (steeple and chapel yard) and the fairy snatches the
 * milk bowl in whichever of those rooms the player is standing.
 *
 * The 4.0 Runner instead runs the task directly by index -- its walk handler
 * calls the same direct task runner (Sub_20_22) as its events -- so there is
 * no interception, and a failing restriction prints its FailMessage (which
 * task_run_task does).  Verified live in run400 (test/adrift4/harness/make_400_walkprobe.py
 * variants E/G): the walk task fires with a wildcard listed before it, and
 * a restricted walk task prints its FailMessage on every arrival turn.
 */
static void run_task_command_dispatch (scr_gameref_t game, scr_int eventtask);


void
run_npc_walk_task (scr_gameref_t game, scr_int walktask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (!gs_task_valid (game, walktask))
    return;

  if (run_get_version (bundle) < TAF_VERSION_400)
    run_task_command_dispatch (game, walktask);
  else if (task_can_run_task_directional (game, walktask, TRUE))
    run_task_run_by_index (game, walktask);
}


/*
 * run_event_task()
 *
 * Run the task executed by a finishing event (its TaskAffected, with
 * "task finished" unset) in a pre-4.0 game.  The reference Runner does not
 * run this task by its stored index: it submits the task's command
 * text through the task matcher, and the first task in list order that
 * matches the text -- a `*` wildcard matches it like any other input -- and
 * whose "where" and restrictions pass is the one that fires.  So a runnable
 * wildcard task earlier in the list steals the event's execution outright
 * (its text prints instead, the affected task does not run), and the theft
 * happens even when the affected task itself could not run where the player
 * is standing.  Restricted matches are passed over silently.
 *
 * All of this is verified against the live 3.9 Runner (see
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 2): "thetest" depends on the
 * stealing -- its "Nice try fish face!" `*` task fires on the same turn as the
 * library drop because an always-restarting one-turn event executes a task
 * every turn, and the author's ALRs splice the two messages -- while a probe
 * with the wildcard placed after the affected task shows list order deciding
 * the winner, and a probe without any event shows no same-turn firing at all.
 *
 * The literal-text comparison below backstops the pattern matcher for the
 * customary un-typeable "#name" commands, which never survive the player
 * input path; the matcher is still consulted so wildcard patterns match.
 *
 * run_task_command_dispatch() is the dispatch itself, shared with the 3.9
 * walk CharTask/ObjectTask path above (in the 3.9 Runner both are the same
 * P-code sequence); run_event_task() is the event-facing name.
 */
/*
 * run_note_dispatched_task_ran()
 *
 * An event's or a walk's task counts as "a task ran" for the pre-4.0
 * ambiguity prompt too.  run390's execute_task sets MemVar_468198 on entry
 * (43F032), whoever called it, and events() (46067A) and the walks in
 * characters() (460675) run before the prompt's guard `(468190 < 0) Or
 * (468198 = 1)` at 4606BD.  cybercow_win T118 `x berry`, with the held
 * berry and the static one in the hair both seen: the "#Rain" event runs
 * task 18 every turn, so run390 prints no "Which berry." and answers
 * examines()'s own "Nothing special." (referencedob -1, 44BF94; the game's
 * ALR makes it "I can tell you nothing about that").
 *
 * 3.80 has the same route and 3.70 has none (measured 2026-09-20).
 * p3xEVQ2 from make_3738_eventflagprobe.py is two hats both Short "hat"
 * with the adjective as the Prefix's last word, plus an immediate event
 * that restarts every turn and runs a `zzev` task; each version is built
 * twice, once with TaskAffected 0.  run380's control answers "Which hat.
 * The red hat or the blue hat?" to all of `poke hat`, `x hat`, `take hat`
 * and `put hat`, and with the event running the task `x hat` becomes
 * "Nothing special.  EVENT TASK RAN." and `take hat` "Take what?  EVENT
 * TASK RAN." -- generaltasks calls events() 44317E before the guard 4431B0
 * `(44F124 < 0) Or (44F12C = 1)`, and checkevent 43A762 dispatches
 * tasks(CByte(1)), which sets 44F12C at 44D0BA.  run370's guard 43C8D3 is
 * `(446140 < 0)` alone and its two files are byte-identical.
 *
 * The flag alone is not enough: it lifts the prompt off whatever the
 * handlers said beneath it.  lib_disambiguate_object_common()'s every-
 * candidate-refused path supplies takes()' "Take what?" and 3.8
 * examines()' "Nothing special.", and lib_verb_object_catch_all_pre390()
 * lets co()'s crowded arm keep run380's therest silent, so `poke hat` and
 * `put hat` leave the buffer empty, never tick, and keep the prompt.
 */
static void
run_note_dispatched_task_ran (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380)
    run_co_task_claimed = TRUE;
}

static void
run_task_command_dispatch (scr_gameref_t game, scr_int eventtask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_command;
  const scr_char *command;
  scr_int task_count, task;

  /*
   * checkevent dispatches by command text with mode 1 too (run390 42D3F5,
   * run380 43A762), so whichever task the text matches joins the turn's
   * string like Sub_20_22's.
   */
  struct dispatch_guard
  {
    dispatch_guard () { task_push_dispatched_run (); }
    ~dispatch_guard () { task_pop_dispatched_run (); }
  } guard;

  /*
   * Get the task's first command pattern; nothing to match if absent.  A
   * task can carry no command at all (Dolg's task 661 in the casino, whose
   * command list is empty), so read with prop_get() rather than the fatal
   * prop_get_string().
   */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = eventtask;
  vt_key[2].string = "Command";
  vt_key[3].integer = 0;
  command = prop_get (bundle, "S<-sisi", &vt_command, vt_key)
            ? vt_command.string : "";
  if (scr_strempty (command))
    {
      /* No command text to dispatch; run the task directly. */
      if (task_can_run_task_directional (game, eventtask, TRUE)
          && run_task_restriction (game, eventtask) == RUN_UNRESTRICTED)
        {
          run_note_dispatched_task_ran (game);
          task_run_task (game, eventtask, TRUE);
        }
      return;
    }

  /* Run the first task in list order the command text matches. */
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      scr_bool is_matched;

      if (!task_can_run_task (game, task)
          || !task_can_run_task_directional (game, task, TRUE))
        continue;

      if (task == eventtask)
        is_matched = TRUE;
      else
        {
          const scr_char *other;

          is_matched = run_match_task_commands (game, task, command,
                                                TRUE, FALSE);
          if (!is_matched)
            {
              vt_key[1].integer = task;
              other = prop_get (bundle, "S<-sisi", &vt_command, vt_key)
                      ? vt_command.string : "";
              vt_key[1].integer = eventtask;
              is_matched = !scr_strempty (other)
                           && scr_strcasecmp (command, other) == 0;
            }
        }
      if (!is_matched)
        continue;

      if (run_task_restriction (game, task) == RUN_UNRESTRICTED)
        {
          if (task != eventtask)
            SCR_TRACE_IF ("SCR_TRACE_EVENT_TASK", "EVTASK steal: task=%ld"
                          " stole [%s] from task=%ld\n", task, command,
                          eventtask);
          run_note_dispatched_task_ran (game);
          task_run_task (game, task, TRUE);
          return;
        }
    }

  SCR_TRACE_IF ("SCR_TRACE_EVENT_TASK",
                "EVTASK no-run: [%s] task=%ld candone=%d dir=%d\n",
                command, eventtask,
                (int) task_can_run_task (game, eventtask),
                (int) task_can_run_task_directional (game, eventtask, TRUE));
}

void
run_event_task (scr_gameref_t game, scr_int eventtask)
{
  if (gs_task_valid (game, eventtask))
    run_task_command_dispatch (game, eventtask);
}


/*
 * run_defer_loud_tasks_to_movement()
 *
 * In version 3.8 (and so also 3.7) a task whose command matches but whose
 * restrictions fail does NOT get to swallow a direction the player can
 * actually walk in: the movement happens, and the task's fail message is
 * never printed.  Verified live in run380 with "The Twilight" (2026-08-04):
 * its task 6 is "w" at the Cliff Top, restricted to Gale being present, and
 * before she joins you the Runner answers a bare "w" with "You move west."
 * and no message at all.  Under the version 4.0 ordering the message wins
 * instead, which strands the player on the very first move of that game --
 * and again later, since it also blocks the attic with "d"/"u" tasks
 * restricted to the Sentinel and the apparition being present.
 *
 * This is specific to movement.  Other standard commands still lose to the
 * message: in the same session "ask gale about mansion" printed task 4's
 * "You can't do that in your present company." even though the Runner has a
 * perfectly good answer of its own for asking an absent character (it says
 * "You can't talk to that." when no task matches at all).  So all this does
 * is let a *successful* move jump the queue.  A move that would be refused
 * changes nothing: the loud task still gets its say, and the refusal is
 * printed afterwards by run_standard_commands() if no task claims the input.
 */
scr_bool
run_defer_loud_tasks_to_movement (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (run_get_version (bundle) > TAF_VERSION_380)
    return FALSE;

  return run_movement_succeeds (game, string);
}

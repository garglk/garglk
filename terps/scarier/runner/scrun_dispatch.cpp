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
 * Command dispatch: run_line_t and its passes, run_all_commands() and the
 * task-command facets, and the `go <place>` walk state.
 *
 * Split out of scrunner.cpp; see scrunner.h for what the runner files
 * share.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"
#include "scrunner.h"


/*
 * The player's current command element (pronoun-substituted), stashed by
 * run_line_t::reset() so that library-initiated match attempts can consult
 * the verb the player actually typed alongside the library's canonical
 * constructed command ("get <object>", and so on).  A pass that runs a
 * line of its own making points it there for the while with a
 * run_dispatch_input_guard (scrunner.h).
 */
const scr_char *run_dispatch_input = nullptr;

/*
 * The steps of a `go <place>` walk still to be typed, and what to say on
 * arrival; see lib_cmd_go_place().  The Runner's route finder types each
 * direction into the input box and presses Return (SendKeys), so each step
 * is a line read at the prompt like any other: echoed, lower-cased, a turn
 * of its own.  run_goto_arrival_due marks the last step's turn, after which
 * run_main_loop() prints the arrival.
 *
 * run_goto_rest holds what was left of the typed line when the walk began
 * (`go to kitchen, look`).  Pre-4.0 generaltasks keeps its split queue in a
 * local (run390 var_E4, run380 var_E4), so each step, typed into the box
 * and run as a nested generaltasks, never sees it: the walk and its arrival
 * come first, then the rest of the line (run390x
 * runner_probes/goto.run390.gs.txt, run380x
 * runner_probes/goto.run380.gs.rtf).  run400 keeps the queue in the global
 * MemVar_4942E4 and empties it at the top of every generaltasks (48A01F),
 * so the first step throws the rest of the line away (run400x
 * runner_probes/goto.run400.gs.txt); see run_player_input().
 */
std::vector<std::string> run_goto_steps;

size_t run_goto_next = 0;

std::string run_goto_arrival;

scr_bool run_goto_arrival_due = FALSE;

std::string run_goto_rest;

void
run_queue_goto_step (const scr_char *step)
{
  run_goto_steps.push_back (step);
}

void
run_set_goto_arrival (const scr_char *text)
{
  run_goto_arrival = text;
}

void
run_finish_goto_walk (void)
{
  run_goto_steps.clear ();
  run_goto_next = 0;
  run_goto_arrival.clear ();
  run_goto_arrival_due = FALSE;
}

void
run_cancel_goto_walk (void)
{
  run_finish_goto_walk ();
  run_goto_rest.clear ();
}

/*
 * scr_take_scripted_line()
 *
 * For the ports' line readers: TRUE and the next walk step in buffer if a
 * walk is under way, in place of reading from the player.  The port prints
 * its prompt first and echoes the step as if typed.
 */
scr_bool
scr_take_scripted_line (scr_char *buffer, scr_int length)
{
  if (run_goto_next >= run_goto_steps.size ())
    return FALSE;

  strncpy (buffer, run_goto_steps[run_goto_next++].c_str (), length - 1);
  buffer[length - 1] = NUL;
  if (run_goto_next >= run_goto_steps.size ())
    {
      run_goto_steps.clear ();
      run_goto_next = 0;
      run_goto_arrival_due = TRUE;
    }
  return TRUE;
}

/*
 * run_get_dispatch_input()
 *
 * Expose the stashed command element to other modules.  sclibrar's
 * SCR_TRACE_CO diagnostic needs the player's own words to reproduce the
 * Runner's whole-command containment test.
 */
const scr_char *
run_get_dispatch_input (void)
{
  return run_dispatch_input;
}

/*
 * Tasks that have already been run by the current command element, reset by
 * run_line_t::reset() alongside run_dispatch_input.
 *
 * A command is dispatched to the tasks twice, once with restrictions ignored
 * and once with them honoured, and the spent-task refusal pass below has to
 * know the difference between a task that was already done when the player
 * typed, and one this very command has just completed.  Only the first is a
 * refusal.  "Shadow of the Past" is the case: `examine good book` runs a
 * silent task (it drops a key and scores, and has no completion text), which
 * turns its own "book not yet crumbled" restriction false; the library
 * examine then prints the book's -- now crumbled -- description.  run400
 * answers the first `examine good book` with that description and only a
 * second one with the restriction's "The book is nothing but dust now."
 * (measured live 2026-08-23).
 */
std::vector<scr_bool> run_tasks_ran_this_command;

/* TRUE while a 4.0 line has a spent task's RepeatText to answer with; set
   by run_line_t::put_prepass() from its REFUSAL_PASS_PROBE. */
scr_bool run_repeat_found_400 = FALSE;

/*
 * The last typed command as the dispatcher saw it, and whether a game task
 * ran for it: the pre-4.0 end-of-turn ambiguity prompt (see
 * lib_co_ambiguity_prompt()) fires only on a line no task claimed, and the
 * Runner's flag for that (MemVar_44F12C) is set at the same place any task
 * executes, library-callback tasks included.
 */
std::string run_co_pending_input;

scr_bool run_co_task_claimed = FALSE;

/*
 * The element as it reached run_line_t::reset(), before the give and
 * ask/talk reference rewrites: run390's checkverb compares the typed line
 * against its verb (42A4F4), so a bare `give` is still "give" there even
 * though the line it stores is the rewritten one.
 */
const scr_char *
run_get_line_input (void)
{
  return run_co_pending_input.empty ()
         ? nullptr : run_co_pending_input.c_str ();
}

/*
 * The line element `again` repeats.  run_player_input() owns it; it sits out
 * here so that run_session_state() can keep it across an autosave.
 */
scr_char run_prior_element[LINE_BUFFER_SIZE];

/*
 * The last two typed lines, lower-cased, as the Runner's command history
 * holds them: run390 shifts every new line into MemVar_468100 at 436268-
 * 43629F, run380 into MemVar_44F098, so (1) is the line being run and (2)
 * the one typed before it.  Blank lines go in too.  See run_element_command().
 */
std::string run_typed_line, run_previous_typed_line;

/*
 * The output that left the temporary game, and the undo game, in their
 * states; undo replays it (see lib_cmd_undo()).  Older undo states carry
 * theirs in the memo ring.
 */
std::string run_temporary_text, run_undo_text;

const std::string &
run_get_undo_text (void)
{
  return run_undo_text;
}


/*
 * run_put_class_guard
 *
 * Confines the put rows to their class test for one run_all_commands() line
 * (run_put_class_only), and lets them go again when the line is done.
 */
struct run_put_class_guard
{
  explicit run_put_class_guard (scr_bool on) { run_put_class_only = on; }
  ~run_put_class_guard () { run_put_class_only = FALSE; }
  run_put_class_guard (const run_put_class_guard &) = delete;
  run_put_class_guard &operator= (const run_put_class_guard &) = delete;
};

/*
 * run_line_t
 *
 * One typed line on its way through run_all_commands(): the passes the
 * Runner's generaltasks makes over it, in its order, and the flags those
 * passes hand each other.  run() is the sequence; every pass is a method of
 * its own, documented where it is defined below.
 */
class run_line_t
{
public:
  run_line_t (scr_gameref_t game_, const scr_char *string_)
    : game (game_), filter (gs_get_filter (game_)), string (string_) {}
  run_line_t (const run_line_t &) = delete;
  run_line_t &operator= (const run_line_t &) = delete;

  /* FORCE: 0 decides leniency from the line, >0 forces it on, <0 off. */
  scr_bool run (int force = 0);

private:
  void reset ();
  scr_bool lenient ();
  scr_bool spent_claim_390 ();
  void put_prepass ();
  void put_pass ();
  void get_outer_400 ();
  void task_peek ();
  void priority_pass ();
  void task_passes ();
  void battle_pass ();
  void library ();
  void settle ();
  scr_bool finish (scr_bool result);

  const scr_gameref_t game;
  const scr_filterref_t filter;
  const scr_char *const string;

  /* status is TRUE once a handler has answered the line; refused, once a
     put has printed a refusal and left the line to the task passes. */
  scr_bool status = FALSE, refused = FALSE;
  /* The register uip_rewrite_references() reads, as the line found it, and
     whether the "(<npc>)" echo came out; see spent_claim_390(). */
  scr_int prior_npc = -1;
  scr_bool ask_echo = FALSE;
  /* A spent 4.0 task's RepeatText answers the line (repeat_found) and, with
     no survivor handler, keeps the priority rows off it (repeat_pending). */
  scr_bool repeat_found = FALSE, repeat_pending = FALSE;
  /* put_drop_list's view of the line: the hoisted line it reads, its
     clauses, whether a named put row has it first, and the battle verbs
     beside it; see put_prepass(). */
  std::string put_hoisted;
  const scr_char *put_line = string;
  std::vector<std::string> put_clauses;
  scr_bool put_first = FALSE, put_contained = FALSE;
  scr_int battle_kinds = 0;
  /* The inventory listing printed ahead of the tasks; see put_pass(). */
  scr_bool inv_listed = FALSE;
  /* The line the task passes dispatch against. */
  const scr_char *task_string = string;
  /* get_outer's view of a 4.0 take line, and what its get_piece did; see
     get_outer_400(). */
  std::string outer_battle;
  const scr_char *outer_line = string;
  size_t outer_mark = 0;
  /* Where the buffer stood when the task dispatcher got the line, and what
     the passes before and after it settled; see task_peek() and
     task_passes(). */
  size_t task_mark = 0;
  scr_int task_mark_executions = 0;
  scr_bool claimed_before_tasks = FALSE, silent_before_priority = FALSE;
  scr_bool task_claimed = FALSE, silent_task_390 = FALSE;
  scr_bool silent_names_npc_390 = FALSE;
  /* The line the take and drop rows read, and its goto class; see
     priority_pass(). */
  std::string priority_hoisted, priority_goto_rest;
  const scr_char *priority_line = string;
  scr_int priority_goto = RUN_GOTO_NONE;
  /* Whether the library cascade got the line at all; see battle_pass(). */
  scr_bool reached_library = FALSE;
};

/*
 * silent_read_line()
 *
 * Whether LINE is a `read` line.  Below 4.0 a task that ran and printed
 * nothing claims its line, but not a read line: the Runner's "I don't
 * understand." there would swallow the object's own read text, and
 * Everything Emanuelle's task 14 (`read * diary *`, silent, sets the
 * variable for the best ending) would hide the diary, the game's one
 * clue.  The task has run; the library's read shows the text.  Deliberate
 * deviation from the Runner, a convenience.
 */
static scr_bool
silent_read_line (scr_gameref_t game, const scr_char *line)
{
  return line
      && run_c_word_pre400 (run_get_version (gs_get_bundle (game)), line,
                            "read") == 0;
}

/*
 * run_line_t::reset()
 *
 * The per-line stores generaltasks makes before it looks at the line.
 */
void
run_line_t::reset ()
{
  run_dispatch_input = string;
#ifdef SCARIER_DUMP_TOOLS
  run_trace_last_input = string;
  scr_trace_deviation_line ();
#endif
  run_co_pending_input = string;
  run_co_task_claimed = FALSE;
  run_tasks_ran_this_command.assign (gs_task_count (game), FALSE);
  lib_verb_object_note_line_top (game);
  lib_co_note_line_top (game);
  obj_mark_npc_parts_seen (game);
  lib_prepass_seen_3738 (game, string);
}

/*
 * run_line_t::lenient()
 *
 * Whether the line matches a task only leniently, for the lenient-task
 * guard run() holds over the whole line.
 */
scr_bool
run_line_t::lenient ()
{
  /* Deliberate deviation: see run_line_matches_task_strictly().  The peek
     runs strictly, so the flag is cleared for it first. */
#ifdef SCARIER_DUMP_TOOLS
  if (run_strict_tasks_requested (string))
    return FALSE;
#endif
  const scr_lenient_tasks_guard strict_match (FALSE);
  return !run_line_matches_task_strictly (game, string);
}

/*
 * run_line_t::spent_claim_390()
 *
 * The ask/talk echo and the character noting that come out ahead of
 * everything, then the pre-4.0 spent task's claim on the line.  Returns
 * TRUE when that claim ends the line.
 */
scr_bool
run_line_t::spent_claim_390 ()
{
  /*
   * The "(<npc>)" an "ask about" / "talk about" echoes comes out ahead of
   * everything, task matching included; see uip_print_ask_echo().  The
   * characters this line names are noted here too: run400 notes them inside
   * characters(), which every line reaches (48B56E is below the task
   * dispatch's exits), so a task-answered line names its characters just
   * the same.  Both read the register before the noting, so remember what
   * it held for the rewrite in library().
   */
  prior_npc = game->last_npc;
  ask_echo = uip_print_ask_echo (game, string);
  uip_note_named_npcs (game, string);

  /*
   * Pre-4.0 the task dispatcher's claim comes first: a spent task whose
   * command matches the line prints its RepeatText slot and nothing below
   * tasks(0) runs -- see run_spent_task_390().  The handlers run390 runs
   * above it, and the character pass that overwrites its message, still
   * answer; they are a turn either way.  Matched on the line as typed: the
   * give and ask/talk rewrites are further down generaltasks.
   */
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      && !run_repeat_assist)
    {
      const scr_char *message;
      const scr_int spent = run_spent_task_390 (game, string, &message);

      if (spent >= 0)
        {
          if (!run_spent_survivor_390 (game, string))
            run_spent_claim_390 (game, message);
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * run_line_t::put_prepass()
 *
 * What the passes below need to know before any of them speaks: whether
 * a spent task answers the line, and put_drop_list's view of it.
 */
void
run_line_t::put_prepass ()
{
  /*
   * 4.0 puts are the exception to the peek: the library's put-in / put-on
   * gets the line BEFORE any task does, and a task only ever answers a put
   * the library could not complete.  Measured with the PUT4-PUT7 arena probes
   * (runner_probes/put[4-7].run400*.txt, 2026-09-05), each pairing a passing
   * put task with reporter tasks restricted on "object is inside container":
   * run400 puts the pill in the cup and the task never runs, whatever its
   * spelling -- wildcard, prefixed or literal -- while run390 on the 3.9 twin
   * (put39.taf, runner_probes/put.run390.b.txt) hands the line to the task
   * and moves nothing.  Read off run400's insides handler
   * (Proc_19_43_46639C): a completed move returns 1 and the dispatcher stops
   * there.  The peek pass would have handed the line to a non-silent matching
   * task first, which is what cost "Sommeril" its `put fish in fountain`
   * (task 18's text where the Runner puts the fish,
   * runner_probes/sommeril.run400.probe.txt) and left `take wet page`
   * answered on the next turn where run400 says "Take what?".  Pre-4.0 keeps
   * the peek order: Scarier matches run390 on the put39 probe on all twelve
   * lines (the last of them, the prefixed `put a bean in a jar` row, closed
   * 2026-09-20 with the pre-4.0 canonical-retry suppression in
   * sclibrar_dispatch.cpp).
   *
   * A put the 4.0 library REFUSES on size or capacity is different again:
   * every message path of that handler exits without setting its return
   * byte, so the refusal is printed but the line is not claimed, and the
   * task passes then answer it as well, run straight on from the refusal
   * with the two-space separator -- "The rock is too big to fit inside the
   * slot.  PUTBIG." (PUT7, runner_probes/put7.run400.b.txt; Zack
   * Smackfoot's `put knife in slot`,
   * runner_transcripts/zacksmackfoot.txt).  The handler signals that with
   * run_priority_refuse(); the join is left pending so that a refusal no
   * task follows stands alone.  Either way the line is handled once the
   * refusal is printed, so the standard table's duplicate put rows never
   * print it a second time.
   */
  /*
   * A 4.0 line that a spent task answers with its RepeatText is not the
   * priority commands' either -- run400 dispatches tasks at 48A481, above
   * everything but inventory (48A457) and the put/drop list (48A462), so
   * `take coin` on a done `take coin` task prints the RepeatText and takes
   * nothing, and `put all on desk` on a done one prints it rather than the
   * put's own empty-handed answer.  The message itself waits until the task
   * passes below have all declined, because a task that can still RUN
   * outranks one that is merely spent; all that is needed here is whether
   * there IS one, so that the priority commands stand aside.
   * run_repeat_survivor_400() holds the handlers that keep the line anyway;
   * they are still a turn, though, because the dispatcher sets its handled
   * byte before the character pass overwrites the message (`x bob` on a
   * spent task ticks the probe's event, where a plain NPC examine does not
   * -- runner_probes/repeat2.run400.txt, and see the survivor's turn in
   * settle()).
   */
  repeat_found = run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
                 && run_task_refusal (game, string, REFUSAL_PASS_PROBE);
  repeat_pending = repeat_found && !run_repeat_survivor_400 (game, string);
  run_repeat_found_400 = repeat_found;

  /*
   * put_drop_list is one of the handlers 4.0 enters on its verb ANYWHERE in
   * the line, so this pre-pass reads the same hoisted line the library does
   * further down -- `blorp drop coin` is "You drop the coin.", not the
   * leftover-word answer the line as typed scores.  See run_hoist_verb_line().
   */
  /* A battle verb beside another handler's word; see run_battle_line().
     4.0's put_drop_list, like get_outer_400(), takes its word anywhere on
     such a line: `hit bob drop coin` with the coin loose is "You are not
     holding the coin.", no blow. */
  battle_kinds = repeat_pending ? 0 : run_battle_line_class (game, string);
  if ((battle_kinds & RUN_BATTLE_DROP)
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    {
      /* The words after the drop word: "drop bob coin" finds no coin. */
      const scr_char *const word = run_battle_kind_word (TAF_VERSION_400,
                                       string, RUN_BATTLE_DROP);
      const scr_int at = run_c_word_pre400 (TAF_VERSION_400, string, word);

      put_hoisted = at == 0
          ? run_battle_respell (game, string, RUN_BATTLE_DROP, word)
          : std::string (string + at);
      put_line = put_hoisted.c_str ();
    }
  else if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
           && run_hoist_verb_line (game, string, put_hoisted))
    put_line = put_hoisted.c_str ();
  /*
   * put_drop_list's own clause loop, carved before anything else looks at
   * the line: run400 enters the routine on the whole-word "put"/"drop"
   * alone, so whether the line has a NAMED put row is asked of the first
   * CLAUSE, which is what the Runner actually resolves.  See
   * lib_put_clauses_400() and put_pass().
   */
  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && !repeat_pending)
    lib_put_clauses_400 (game, put_line, put_clauses);
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      || repeat_pending)
    put_first = FALSE;
  else if (put_clauses.empty ())
    put_first = run_is_put_command_400 (game, put_line, &put_contained);
  else
    put_first = run_is_put_command (game, put_clauses[0].c_str ());
}

/*
 * run_line_t::put_pass()
 *
 * The handlers run400 runs above the task dispatcher: put_drop_list's
 * take-from, the inventory listing and the named put rows, clause by
 * clause.
 */
void
run_line_t::put_pass ()
{
  if (!repeat_pending && run_put_take_400 (game, string))
    {
      status = TRUE;
      put_first = FALSE;
    }
  /*
   * The inventory listing at 48A457 comes out ahead of the dispatcher too,
   * but unlike the put/drop rows it does not take the line away from the
   * task: 48A481 runs immediately afterwards and appends the CompleteText to
   * the listing.  So print it here and leave `status` alone, letting the
   * task passes below run on the same line; the listing claims whatever they
   * decline.  See run_is_inventory_command().
   *
   * run380 and run370 do the same: generaltasks stores inventory()'s result
   * in a scratch variable (run380 4421C2, run370 43B961) without the GoTo
   * that takes() and drops() get, and tasks(0) follows (4421F6 / 43B97F).
   * inventory() prints its listing directly (run380 42E218).  Measured on
   * wrecked T24 (run380x): "I am wearing my clothes, ..." and then task 43's
   * "Boff says ...".  Only run390 lets the listing claim the line (45F45B).
   */
  inv_listed = run_get_version (gs_get_bundle (game)) != TAF_VERSION_390
               && run_is_inventory_command (game, string)
               && run_priority_commands (game, string);
  if (put_first)
    {
      /*
       * `put coin in box and hat in desk` is put_drop_list's own multi-clause
       * loop (459C75), not the top-level splitter's: the " and " sits at or
       * beyond the preposition split, so the line arrived here whole, and the
       * Runner runs the put rows once per clause inside the ONE turn.  Their
       * answers come out back to back with no separator of any kind, which is
       * exactly what falls out of running the priority pass again on the same
       * line.  lib_put_clauses_400() carves the clauses; a trailing clause
       * with no preposition of its own is dropped there, unrun.
       */
      if (!put_clauses.empty ())
        {
          std::vector<std::string>::const_iterator clause;

          run_put_clause_loop_active = TRUE;
          for (clause = put_clauses.begin ();
               clause != put_clauses.end (); ++clause)
            {
              const scr_bool is_last = (clause + 1 == put_clauses.end ());
              const run_dispatch_input_guard input (clause->c_str ());

              if (run_priority_commands (game, clause->c_str ()))
                status = TRUE;
              /*
               * A refusing clause has left its message pending, and the
               * next clause's pass would reset the flag out from under it,
               * so settle it here rather than once after the loop.  A
               * refusal-only put ends without a terminator of its own (see
               * lib_put_in_refused), and between clauses run400 shows that
               * plainly: "The coin is too big to fit inside the box.You
               * can't put anything inside the desk!" is what the Runner's
               * own scrollback holds, with no separator at all where the
               * next clause begins (p4AND, runner_probes/and.run400.c.txt
               * + DUMP_SCROLLBACK, 2026-09-08).  Only the last clause
               * keeps the pending join, which is what a task answering the
               * same line wants.
               */
              refused = run_priority_refused;
              if (refused)
                {
                  pf_note_trailing_auto_break (filter);
                  if (is_last)
                    pf_buffer_join_pending (filter);
                  else
                    pf_undo_auto_break (filter);
                }
              /*
               * name_object answers every clause where it stands, so a
               * clause the tentative priority pass deferred (a put whose
               * target is not a container, which run400 lets a loud task
               * outrank -- see run_priority_commands) has to be finished
               * here, out of the duplicate STANDARD_COMMANDS rows.  Only
               * the LAST clause is left to the whole-line passes below,
               * which parse the line's final preposition and so answer for
               * that clause anyway.
               */
              if (!is_last && !status && !refused && run_priority_deferred
                  && run_standard_commands (game, clause->c_str ()))
                status = TRUE;
            }
          run_put_clause_loop_active = FALSE;
          refused = refused && !status;
        }
      else
        {
          {
            const run_dispatch_input_guard input (put_line);

            const scr_containment_guard containment (put_contained);

            status = run_priority_commands (game, put_line);
          }
          refused = !status && run_priority_refused;
          if (refused)
            {
              pf_note_trailing_auto_break (filter);
              pf_buffer_join_pending (filter);
            }
        }
    }
}

/*
 * run_get_motion_task_line()
 *
 * Deliberate deviation (2026-09-30).  run400's get_outer takes the object a
 * get line names before the task dispatcher runs, whatever words stand
 * between the verb and the name, so The X-Files' `get in the van` answers
 * "You take VW Van." and its task 26 `*Van*`, which gets the player in,
 * never runs (runner_probes/xfiles.run400.t76.txt).  TRUE when LINE is
 * "get" followed by a word that makes it a movement -- get in, into, on,
 * out, under, behind, through, up, down, ... -- and a task matches the typed
 * line.  get_outer then leaves the line to the tasks.  A plain `get van`,
 * and a movement line no task takes, keep the Runner's take.
 */
static scr_bool
run_get_motion_task_line (scr_gameref_t game, const scr_char *line,
                          const std::string &task_line)
{
  static const scr_char *const MOTION[] = {
    "in", "into", "inside", "on", "onto", "out", "under", "underneath",
    "beneath", "behind", "through", "over", "across", "aboard", "up", "down",
    "back", "away", "near", "beside", "onboard", nullptr
  };
  const scr_char *next;
  size_t length;
  scr_int index_;

  if (strncmp (line, "get ", 4) != 0)
    return FALSE;
  next = line + 4;
  while (*next == ' ')
    next++;
  length = strcspn (next, " ");
  for (index_ = 0; MOTION[index_]; index_++)
    {
      if (strlen (MOTION[index_]) == length
          && strncmp (next, MOTION[index_], length) == 0)
        {
          const scr_bool motion
              = run_line_matches_task_strictly (game, task_line.c_str ());

          if (motion)
            SCR_DEVIATION ("get_motion", nullptr);
          return motion;
        }
    }
  return FALSE;
}

/*
 * run_line_t::get_outer_400()
 *
 * run400's get_outer, between put_drop_list and the task dispatcher: a
 * take line's own pre-match against the take-family tasks, and the take
 * it makes when they decline.
 */
void
run_line_t::get_outer_400 ()
{
  /*
   * A 4.0 put-in whose direct object named nothing has already rewritten the
   * command line the tasks are dispatched against, and the Runner never puts
   * it back -- run_priority_unnamed_put_object() has the whole of it -- so
   * IceCream's own `put ice cream in cone` and advent350b's `drop bear` never
   * reach the tasks written for them.  Deliberate deviation (2026-09-27): the
   * task passes get the line as typed.
   */
  task_string = string;

  /*
   * run400's get_outer (4582D8) sits between put_drop_list and the
   * dispatcher, and its first act is `If c("empty") Then line = Replace(line,
   * "empty ", "get all from ")`: a whole-word "empty" ANYWHERE turns the line
   * into a take-from, so onnafa's `give empty beer mug to perry` first says
   * "You can't take anything from the beer mug." and `x empty beer mug`
   * with the mug gone is "I don't understand where you want to get things
   * from." (6095ff3b0).  Deliberate deviation: Scarier leaves a line that
   * only names an "empty ..." object alone; a line starting with "empty" is
   * the take-from rows' either way.
   */

  /*
   * get_outer (4582D8) runs at 48A46D, BEFORE the task dispatcher at 48A481.
   * On a get/take line with no whole-word "all" or "and", its get_piece
   * (473A34) first pre-matches the line against take-family tasks (453C50
   * class 1, 472D9C); a hit dispatches the task.  A miss goes on to the
   * library take, so a line whose task only matches in the dispatcher proper
   * is answered as a take.  Measured on The X-Files T76: "get in the van"
   * gives "You take VW Van." (runner_probes/xfiles.run400.t76.txt), where
   * T55's same line, with task 13 pre-matching, gets in.
   */
  /*
   * On a battle line get_outer is entered on its word anywhere, and claims
   * the way it always does, refusal included: `hit bob take coin` takes the
   * coin (or refuses) and strikes nothing, `take hit bob` names nothing to
   * take and is a blow.  See run_battle_line().
   */
  if ((battle_kinds & RUN_BATTLE_TAKE)
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    {
      outer_battle = run_battle_respell (game, string, RUN_BATTLE_TAKE,
          run_battle_kind_word (TAF_VERSION_400, string, RUN_BATTLE_TAKE));
      outer_line = outer_battle.c_str ();
    }
  if (!status && !refused && !put_first && !repeat_pending && !inv_listed
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && (strncmp (outer_line, "get ", 4) == 0
          || strncmp (outer_line, "take ", 5) == 0
          || strncmp (outer_line, "pick ", 5) == 0)
      && !lib_input_contains_word (outer_line, "all")
      && !lib_input_contains_word (outer_line, "and"))
    {
      /* The piece names its object by whole-word score, so "get in the
         van" is a take of the van; see lib_take_scored_400(). */
      auto outer_take = [&] () -> scr_bool
        {
          const run_dispatch_input_guard input (outer_line);
          scr_bool taken;

          taken = run_priority_commands (game, outer_line);
          if (!taken)
            {
              const scr_ref_number_guard ref_number (game);
              taken = lib_take_scored_400 (game);
            }
          return taken;
        };
      const scr_int kind = lib_task_prematch_kind_input (game, 1);

      if (kind == 2)
        {
          /*
           * get_piece dispatches a pre-matched task at 472DE7 but claims the
           * line only when the pre-matcher answered 1, a task with text of
           * its own.  A textless task (2) runs there and get_piece carries
           * on, resolving its noun only now.  make_400_takedoubleprobe.py
           * measures both halves (run400x runner_probes/tdbl.run400.txt,
           * runner_probes/tdbo.run400.txt).
           */
          outer_mark = pf_buffer_length (filter);
          run_game_commands_in_parser_context (game, task_string, TRUE, FALSE);
          if (lib_take_names_dynamic_400 (game, outer_line))
            {
              /*
               * An object in reach is taken, or refused, as usual, and the
               * task ran once only.  The take line comes first and the
               * task's text follows it (47359A, lib_take_backend_common());
               * a refusal is appended.  p4TDBO `get ball` "Player take the
               * ball.  BOBTEXT.", `get gem` held "BOBTEXT.Player is already
               * carrying the gem."; an object the task brought within reach
               * too, sommeril T49 `take silver key` "You are already
               * carrying the SILVER KEY." (runner_transcripts/sommeril.txt).
               */
              std::string task_text = pf_buffer_tail (filter, outer_mark);
              scr_int held_before = 0, held_after = 0, object;

              pf_truncate (filter, outer_mark);
              GS_FOR_EACH_OBJECT (game, object)
                held_before += gs_object_position (game, object)
                               == OBJ_HELD_PLAYER;
              status = outer_take ();
              GS_FOR_EACH_OBJECT (game, object)
                held_after += gs_object_position (game, object)
                              == OBJ_HELD_PLAYER;
              if (!task_text.empty ())
                {
                  if (held_after > held_before)
                    pf_buffer_join (filter, task_text.c_str ());
                  else
                    {
                      std::string answer = pf_buffer_tail (filter, outer_mark);

                      pf_truncate (filter, outer_mark);
                      while (!task_text.empty () && task_text.back () == '\n')
                        task_text.pop_back ();
                      pf_buffer_string (filter, task_text.c_str ());
                      pf_buffer_string (filter, answer.c_str ());
                    }
                  status = TRUE;
                }
            }
          else if (lib_task_prematch_kind_input (game, 1) != 0)
            {
              /*
               * With no object its 473241 pre-match hits again and exits
               * silently, and the dispatcher at 48A481 runs the typed line
               * AGAIN.  British Fox T318 `get grace` prints task 123's text
               * twice (runner_transcripts/britishfox.txt); p4TDBL `get bob`
               * "BOBTEXT.  BOBTEXT." with its counter up by two, `get eel`,
               * nothing printed by either run, "I don't understand.".
               *
               * Deliberate deviation (2026-10-17): the task runs ONCE.  The
               * second run is a Runner accident that doubles the task's
               * text and its actions (a counter it bumps goes up by two),
               * which no game was written for.  The line is claimed as
               * get_piece's; whatever the one run printed answers it.
               * Logged under "Deliberate deviations" in
               * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
               */
              status = TRUE;
            }
          else
            {
              /*
               * A task the run spent misses 473241, and get_piece goes on to
               * 47332B: "Take what?" into an empty buffer, and the line is
               * get_outer's.  FunHouse T2 `pick up money`, task 11 once-only
               * and silent (score only): "Take what?" and the turn ticks
               * (runner_transcripts/funhouse.txt); p4TDBL's first `get dog`
               * prints the text its execute action set off, and nothing more.
               */
              if (pf_buffer_length (filter) == outer_mark)
                pf_buffer_string (filter, "Take what?\n");
              status = TRUE;
            }
        }
      else if (run_get_motion_task_line (game, outer_line, task_string))
        {
          /* Not a take: the dispatcher has the line.  See
             run_get_motion_task_line(). */
        }
      else if (lib_take_names_dynamic_400 (game, outer_line))
        {
          if (kind != 0)
            {
              /*
               * The pre-match hit on a task with text, or on a failing
               * restriction's message, is get_piece's claim, loud: the
               * FailMessage prints through 45404C (44CCA5) before any
               * library take could.  bigspy2 T2 `get puzzle`, the boy in the
               * room: "Hey, leave my sliding puzzle alone..." (runner_
               * transcripts/bigspy2.txt); advent350b T386 `get chain`,
               * "It's locked to the friendly bear." (runner_transcripts/
               * advent350b.txt:1753).
               */
              status = run_game_commands_in_parser_context (game, task_string,
                                                            TRUE, FALSE);
            }
          else
            status = outer_take ();
        }
    }
}

/*
 * run_line_t::task_peek()
 *
 * The task dispatcher's first look at the line, ahead of the take and
 * drop rows: the all-or-nothing peek of the design note in
 * run_all_commands().
 */
void
run_line_t::task_peek ()
{
  if (!status && !refused
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    run_restriction_cache_task_pick (game, task_string);

  /*
   * Below 3.90 a drop line's one and only look at the task matcher is the
   * one inside drops(), which silences a matched task when nothing named is
   * in hand -- deliberately not ported; see sclibrar_drop.cpp.
   */

  /*
   * A put line with no in/on split that a task pre-matches reaches
   * name_object's mode-2 scorer, whose flat tie answer prints ahead of the
   * task and does not claim the line; see lib_put_task_tie_400().
   */
  if (!status && !refused && !put_first && !repeat_pending && !inv_listed
      && lib_put_task_tie_400 (game, put_line))
    {
      pf_note_trailing_auto_break (filter);
      pf_buffer_join_pending (filter);
    }

  task_mark = pf_buffer_length (filter);
  task_mark_executions = task_execution_count ();
  claimed_before_tasks = status;
  if (!status && !refused)
    {
      status = run_game_commands_in_parser_context (
                   game, task_string, FALSE,
                   run_get_version (gs_get_bundle (game))
                   >= TAF_VERSION_400);
      if (run_any_task_ran_this_command ())
        run_takes_second_pass_370 (game, string, task_string, task_mark);
      /* A silent task on a take or drop line: the handler's own "Take
         what?" / "Drop what?" claims it (see the helper). */
      if (!status && run_any_task_ran_this_command ()
          && pf_buffer_length (filter) == task_mark
          && lib_move_what_after_silent_task_pre400 (game))
        status = TRUE;
    }
  /*
   * Below 4.0 a task the dispatcher ran that printed nothing, and that no
   * "Take what?" / "Drop what?" claimed, keeps the take and drop handlers
   * out as well: the line is the silent claim's below.  p3xBEYOND
   * (make_beyondprobe.py, 2026-09-26): run370 `take orb` with the orb on the
   * floor runs silent task "take orb" twice and answers "I don't
   * understand.", the orb left there (runner_probes/beyond.run370.c.rtf);
   * run390 `drop cape to the floor` with the cape on the floor scores the
   * silent task and says "I don't understand." rather than "You don't have
   * the red cape!"  (runner_probes/beyond.run390.c.txt).
   */
  silent_before_priority = !claimed_before_tasks && !status
      && run_any_task_ran_this_command ()
      && pf_buffer_length (filter) == task_mark
      && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      && !silent_read_line (game, task_string);
}

/*
 * run_line_t::priority_pass()
 *
 * The priority rows -- the takes and drops that move objects to and from
 * inventory -- on the line they read, with gotoplace's turn after them.
 */
void
run_line_t::priority_pass ()
{
  /*
   * The take and drop rows live in the priority table, not in the library
   * cascade below, and pre-4.0 takes() and drops() are entered on their verb
   * ANYWHERE in the line exactly as the cascade's handlers are -- run380's
   * takes 43D788 / drops 438659, run370's 435E28 / 430475, run390's 454428 /
   * 445500, every one of them a disjunction of c() tests.  So this pass reads
   * the same hoisted line the cascade does; `blorp take coin` is a take of
   * the coin.  See run_hoist_verb_line(), and
   * lib_move_named_whole_line_pre400() for the noun half that then has to
   * find "coin" past the nonsense word.
   */
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      /* A leading goto word is no verb to takes() and drops(); see
         run_goto_line_class(). */
      priority_goto = run_goto_line_class (game, string);
      if (priority_goto != RUN_GOTO_NONE
          && run_goto_strip_head (game, string, priority_goto_rest))
        priority_line = priority_goto_rest.c_str ();
      if (run_hoist_verb_line (game, priority_line, priority_hoisted))
        priority_line = priority_hoisted.c_str ();
    }
  else if (battle_kinds & RUN_BATTLE_DROP)
    priority_line = put_line;
  /* A 4.0 list line with a second verb is put_drop_list's or get_outer's
     own list; see run_two_verb_line_400(). */
  else if ((run_c_word_pre400 (TAF_VERSION_400, string, "all") >= 0
            || run_c_word_pre400 (TAF_VERSION_400, string, "and") >= 0)
           && run_two_verb_line_400 (game, string, priority_hoisted))
    priority_line = priority_hoisted.c_str ();

  if (!status && !put_first && !inv_listed && !repeat_pending
      && !silent_before_priority)
    {
      const size_t goto_mark = pf_buffer_length (filter);
      std::vector<scr_int> goto_places;
      scr_int object;

      if (priority_goto == RUN_GOTO_TAKE || priority_goto == RUN_GOTO_DROP)
        GS_FOR_EACH_OBJECT (game, object)
          {
            goto_places.push_back (gs_object_position (game, object));
            goto_places.push_back (gs_object_parent (game, object));
          }
      const std::vector<scr_int> battle_places
          = battle_kinds ? run_battle_places (game) : std::vector<scr_int> ();
      {
        const run_dispatch_input_guard input (priority_line);

        status = run_priority_commands (game, priority_line);
      }
      /* Below 4.0 a take or drop that only refused is wiped by dobattle,
         which then has the line; 4.0's put_drop_list and get_outer claim
         on a refusal.  See run_battle_line(). */
      if (status && battle_kinds
          && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
          && run_battle_places (game) == battle_places)
        {
          pf_truncate (filter, goto_mark);
          status = FALSE;
        }
      /* A take that refused leaves gotoplace its turn: `take box goto
         cave` with the box in hand is "You've already got a box!Unknown
         place." below 4.0.  See run_goto_after(). */
      if (status && !goto_places.empty ())
        status = run_goto_after (game, string, priority_goto, goto_mark,
                                 goto_places, status);
      /*
       * The all/everything put rows are not put_first (see
       * run_is_put_command), so their tentative pass runs here, and a
       * refusal-only put among them -- the 4.0 closed-container refusal
       * from lib_put_in_closed_400(), a size or capacity refusal -- has
       * printed its refusal and left the line for the task passes exactly
       * as a named row's does in put_pass().  Settle it the same way, or the
       * STANDARD_COMMANDS twin below prints the refusal a second time:
       * probe PCLOSED `put all in chest` with the chest shut and a stone in
       * hand is one "The chest is closed!"
       * (runner_probes/closed.run400.feed2.txt:7).
       */
      if (!status && run_priority_refused)
        {
          refused = TRUE;
          pf_note_trailing_auto_break (filter);
          pf_buffer_join_pending (filter);
        }
    }
}

/*
 * run_line_t::task_passes()
 *
 * The task dispatcher proper: once ignoring failed restrictions, once
 * with them, and what a task that ran and printed nothing means below
 * 4.0.
 */
void
run_line_t::task_passes ()
{
  if (!status)
    status = run_game_commands_in_parser_context (game, task_string,
                                                  FALSE, FALSE);
  if (!status && !inv_listed
      && !run_defer_loud_tasks_to_movement (game, task_string))
    status = run_game_commands_in_parser_context (game, task_string,
                                                  TRUE, FALSE);
  task_claimed = !claimed_before_tasks && status
                 && run_any_task_ran_this_command ();
  /*
   * 3.9: a task that ran and printed nothing claims the line in run390 --
   * tasks() returns it, generaltasks skips everything below the dispatcher,
   * and an empty buffer is printed as the game's DontUnderstand text -- and
   * the line is not a turn.  Both halves are ours now: the clock first
   * (2026-09-19; ALEXIS.TAF T99 `open chest`, task 14 `open * chest` with no
   * CompleteText, draws nothing in run390x per alexis_tr_trace.txt, and
   * ticking there put every later battle roll a turn out of phase), then the
   * claim (2026-09-20), which is the `!silent_task_390` guard on library():
   * the Runner then prints DontUnderstand over the empty buffer, and
   * Scarier prints "Done." instead (deliberate deviation, see settle()).
   *
   * It is what run390 answers `read diary` in everything, `piss` in life,
   * `turn off tv` in lifesimulation and `open chest` in alexis with, and in
   * the_hangover it also keeps the filing cabinet shut: the task matched and
   * did nothing, so the library never opens it and the approval form stays
   * inside.  See the note "Measured 2026-08-23 (make_39_doneprobe.py" in
   * scrun_match.cpp.
   *
   * run370 and run380 claim the same way (p3xBEYOND, 2026-09-26: silent task
   * "wave cape slowly" +1000 is "I don't understand." where the object
   * catch-all would say "... with the red cape.",
   * runner_probes/beyond.run370.c.rtf, runner_probes/beyond.run380.c.rtf),
   * but there the line stays a turn: neither has the not-a-turn byte below
   * 3.90.
   */
  silent_task_390 = !claimed_before_tasks && !status
      && run_any_task_ran_this_command ()
      && pf_buffer_length (filter) == task_mark
      && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      && !silent_read_line (game, task_string);
}

/*
 * run_line_t::battle_pass()
 *
 * dobattle's stamina recovery and the battle line itself, and the claims
 * a refusal, an inventory listing or get_piece's double run make on a
 * line no task answered.
 */
void
run_line_t::battle_pass ()
{
  /*
   * dobattle (run400 Proc_11_4_47F084), called from generaltasks at 48A4A2
   * when the Battle System is on, opens with the stamina recovery loop for
   * every character, 47E682-47E764 -- see battle_recover_line().  It runs
   * here, once per line element, ahead of every library verb, and it is
   * gated the way the Runner's control flow gates it:
   *
   *  - Four handlers above it end the line by GoTo loc_48B4E3 when they
   *    claim it: the inventory listing 48A457, put_drop_list 48A462
   *    (result set only when name_object 46E5D8 answered, 459CB0/459DAD),
   *    get_outer 48A46D (result set only when the per-piece get 473A34
   *    answered, 458200/4582B9), and the task dispatcher 48A481, whose tail
   *    at 44CCC0 forces its result to 0 when the message buffer is empty.
   *    That is `status` here: a silent task leaves it FALSE and recovers,
   *    a put refusal leaves it FALSE too (`refused` is turned into a claim
   *    only below), and the inventory listing by itself does not claim.
   *  - A spent task's RepeatText is printed by the dispatcher and claims
   *    the line (`repeat_found`; its survivors run from 48B4E3, still past
   *    the call).
   *  - The not-a-turn byte MemVar_494281: reset at 48A010 for each element,
   *    and set inside dobattle itself at 47DCB9 by the status path, `If
   *    c("status") And MemVar_4941B0 = ""` -- the whole word anywhere in the
   *    line, with nothing yet in the message buffer -- before the loop at
   *    47E682 tests it.  No other store reaches the test on this path.
   *
   * So a line that is not a turn (`score`, a line nobody understands) still
   * recovers, `wait` recovers once per typed line and not per waited turn,
   * a Who-continuation recovers twice, and a claimed line not at all.  The
   * counters start at Recovery (battle_start()), so the first point lands
   * Recovery lines in.  Recovery is a 4.0 property; 3.9 has none of this.
   */
  if (!status && !repeat_found
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && battle_is_enabled (game)
      && !(pf_buffer_length (filter) == 0
           && lib_input_contains_word (string, "status")))
    battle_recover_line (game);

  if (refused)
    {
      pf_clear_join_pending (filter);
      status = TRUE;
    }
  if (inv_listed)
    status = TRUE;
  if (!status && !silent_task_390 && battle_kinds)
    status = run_battle_line (game, string, battle_kinds);
  reached_library = !status && !silent_task_390;
}

/*
 * run_line_t::library()
 *
 * The standard library cascade, on the line rewritten around the last
 * character named and hoisted the way 4.0's handlers read it, with the
 * already-done refusals slotted where the Runner has them.
 */
void
run_line_t::library ()
{
  if (status || silent_task_390)
    return;

  /*
   * Only now, with every task pass declined, does the Runner rewrite a
   * "give X" with no "to", or an "ask about" / "talk about", around the
   * last character a library command named (uip_rewrite_references()).
   * The order matters: run400's input routine dispatches typed-command
   * tasks at 48A481 (Proc_19_24_44CCE0), well before the give rewrite at
   * loc_48A98A and the Proc_19_0_480674 call at 48B56E that holds the
   * ask/talk rewrite, and a matched task jumps past both (GoTo 48B4E3).
   * So "Sommeril"'s literal task "ask about glass framed page" keeps
   * answering even with the Gargoyle as the last-named character;
   * rewriting first turned it into "ask gargoyle about ..." and lost the
   * task to the library's generic reply.  Its "(GARGOYLE)" is printed
   * all the same, up at the top of this routine -- only the rewritten
   * STRING is the library's; see uip_print_ask_echo().
   *
   * The Runner records the characters a line names inside Proc_19_0
   * (loc_47F3A2) as well, after both rewrites -- so a rewrite always
   * sees the register as the previous command left it.  That noting is
   * done up at the top of this routine now, because characters() runs
   * on every line; see uip_note_named_npcs().
   */
  scr_owned_string rewritten (uip_rewrite_references (game, string,
                                                     prior_npc, ask_echo));
  const scr_char *library_string =
      rewritten ? rewritten.get () : string;
  std::string sitstand_rest, openclose_rest;
  run_dispatch_input = library_string;
  /*
   * 4.0 enters its library handlers on the whole verb ANYWHERE in the
   * line, so a line whose verb is not at the head is answered with the
   * verb hoisted to the front; see run_hoist_verb_line().
   */
  /*
   * A goto line with one other verb: the verb's handler answers with
   * gotoplace switched off, and gotoplace has its turn after the block;
   * see run_goto_line_class().
   */
  const scr_int goto_class = run_goto_line_class (game, string);
  const size_t goto_mark = pf_buffer_length (gs_get_filter (game));
  std::vector<scr_int> goto_places;
  std::string goto_rest;
  if (goto_class != RUN_GOTO_NONE)
    {
      scr_int object;

      lib_go_place_off = TRUE;
      GS_FOR_EACH_OBJECT (game, object)
        {
          goto_places.push_back (gs_object_position (game, object));
          goto_places.push_back (gs_object_parent (game, object));
        }
      /* A leading goto word is no verb to the handlers above
         gotoplace: `goto cave take box` is the take's. */
      if (run_goto_strip_head (game, library_string, goto_rest))
        {
          library_string = goto_rest.c_str ();
          run_dispatch_input = library_string;
        }
    }
  std::string hoisted;
  if (run_hoist_verb_line (game, library_string, hoisted))
    {
      library_string = hoisted.c_str ();
      run_dispatch_input = library_string;
    }
  /*
   * Pre-4.0 the already-done refusal outranks the standard library; see
   * the note on run_task_refusal().  The room half still runs after it.
   */
  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      /* With the repeat assist on, the already-done answer waits for
         the post-library pass, after movement and the library. */
      if (!run_repeat_assist)
        status = run_task_refusal (game, library_string,
                                   REFUSAL_PASS_PRE);
    }
  else if (repeat_pending)
    {
      /*
       * Matched on the line as typed, the way put_prepass()'s probe did and
       * the way run400's dispatcher does: the give and ask/talk rewrites
       * below loc_48A98A are further down the routine than 48A481.
       */
      status = run_task_refusal (game, string, REFUSAL_PASS_PRE);
    }
  /* sitstand enters on its words anywhere; see lib_sitstand_anywhere(). */
  if (!status)
    {
      status = lib_sitstand_anywhere (game, run_line_for_anywhere,
                                      &sitstand_rest);
      if (!status && !sitstand_rest.empty ())
        {
          /* The move is made; the rest of the line goes on without
             the sit words, the shape the ask/talk rows know. */
          library_string = sitstand_rest.c_str ();
          run_dispatch_input = library_string;
        }
    }
  /*
   * openclose is the next Call generaltasks makes, and it makes it on
   * every line; see lib_openclose_anywhere().  A handler below it that
   * will speak for the line gets it with the open/close word cut out.
   */
  if (!status)
    {
      status = lib_openclose_anywhere (game, string, run_line_for_anywhere,
                                       &openclose_rest);
      if (!status && !openclose_rest.empty ())
        {
          library_string = openclose_rest.c_str ();
          run_dispatch_input = library_string;
        }
    }
  /* whereis is below examines and above therest; see
     lib_whereis_anywhere(). */
  if (!status)
    status = lib_whereis_anywhere (game, run_line_for_anywhere);
  if (!status)
    status = run_score_anywhere (game, library_string);
  if (!status)
    status = run_therest_absent_370 (game, library_string);
  if (!status)
    status = run_wait_anywhere (game, library_string);
  if (!status)
    status = run_standard_verb_commands (game, library_string);
  /*
   * The out-of-room refusal sits INSIDE the library, one line above
   * run390's Call therest() -- so it outranks the generic catch-alls and
   * loses to every dedicated handler above it.
   */
  if (!status
      && run_get_version (gs_get_bundle (game)) == TAF_VERSION_390
      && game->pending_endgame == 0)
    status = run_try_command_table (EXAMINE_ABOVE_REFUSAL_390_COMMANDS,
                                    game, library_string);
  if (!status)
    status = run_task_refusal (game, library_string, REFUSAL_PASS_MID);
  if (!status)
    status = run_standard_give_npc_commands (game, library_string);
  /* run390's therest opens with its "with" arm. */
  if (!status)
    status = lib_with_arm_390 (game);
  if (!status)
    status = run_therest_pre400 (game, library_string);
  if (!status)
    status = run_standard_fallback_commands (game, library_string);
  if (!status)
    status = run_task_refusal (game, library_string, REFUSAL_PASS_POST);
  if (goto_class != RUN_GOTO_NONE)
    status = run_goto_after (game, string, goto_class, goto_mark,
                             goto_places, status);
}

/*
 * run_line_t::settle()
 *
 * The stores generaltasks makes after the library: the survivor's turn,
 * the 3.9 silent task's lost turn and topic reply, and the 4.0 namesake
 * and with-half questions.
 */
void
run_line_t::settle ()
{
  /*
   * A survivor answered a line the dispatcher had already claimed: the
   * message is the library's but the turn is the refusal's, so an answer
   * that is normally administrative counts here.  run400 prints the
   * RepeatText at 48A481 and jumps to loc_48B4E3, which is below the stores
   * that would mark the line administrative and above the character pass at
   * 48B56E -- so the NPC examine's own text comes out and the walk and event
   * tick at 48B599 still runs.
   */
  if (status && repeat_found && !repeat_pending)
    game->is_admin = FALSE;
  /*
   * The silent task's DontUnderstand and its lost turn are one store in
   * run390: generaltasks 46063E-46065A, `If msg = "" And var_350 = 0 Then
   * msg = DontUnderstand: GoTo 46067F`, and the jump lands past the calls to
   * characters() and events().  var_350 is the scan of every character's
   * Name and first Alias over the line (4605EB-460638,
   * lib_line_names_npc_390()), present or not.  So a silent task's line that
   * names a character keeps its empty buffer AND its turn, and characters()
   * runs over it -- whose ask block (4597FE) writes the topic reply, or the
   * no-response answer over the empty buffer (459B46).  Measured on A Day In
   * Toronto (toronto.taf, 3.90; run390x runner_transcripts/toronto.txt T9):
   * task 3 `ask waiter about burger` has no CompleteText and only moves the
   * burger into the room, and the Runner answers the line with the Waiter's
   * topic "ok", where the substitution alone gives the game's "huh?".
   *
   * What the Runner shows when the buffer is STILL empty after characters()
   * -- a silent task on a non-ask line naming a character, or the character
   * out of the room (459941) -- is not measured: it prints that empty buffer
   * (4606D0), and Scarier keeps the DontUnderstand text there, the turn
   * being ticked all the same.
   */
  silent_names_npc_390 = silent_task_390
                         && lib_line_names_npc_390 (game, string);
  if (silent_task_390 && !silent_names_npc_390
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    game->is_admin = TRUE;

  /*
   * 3.9: the talk-to hint replaces the fail message of a task that did not
   * run; see lib_talk_to_hint_after_failed_task_390().
   */
  if (status && !claimed_before_tasks
      && task_execution_count () == task_mark_executions
      && pf_buffer_length (filter) > task_mark)
    lib_talk_to_hint_after_failed_task_390 (game, string, task_mark);

  /*
   * 3.9: a character's topic reply replaces what a task printed for an ask or
   * talk-to-about line; see lib_ask_npc_topic_after_task_390().  4.0 leaves
   * the task's text alone.
   */
  if ((task_claimed || silent_names_npc_390)
      && run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390
      && run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      && (uip_match ("ask %character% about %text%", string, game)
          || uip_match ("talk to %character% about %text%", string, game)))
    {
      if (lib_ask_npc_topic_after_task_390 (game, task_mark,
                                            silent_names_npc_390)
          && silent_names_npc_390)
        status = TRUE;
    }

  /*
   * Deliberate deviation (2026-10-15): a silent task's line that nothing
   * else answered says "Done." where every Runner before 4.0 prints the
   * game's DontUnderstand text over the empty buffer.  The task ran -- it
   * opened the chest (alexis), the tatami mat (namiki), scored the filing
   * cabinet (the_hangover) -- so "What you typed doesn't work." told the
   * player the opposite of what happened.  The claim itself stays: letting
   * the library answer the line instead made it refuse what the task had
   * just done ("The loose tatami mat is already open!", "You can't open
   * that.") in six of the eight goldens it reached.  The turn is the
   * Runner's still: is_admin above decides it, so the line is not a turn at
   * 3.9 and is one at 3.7/3.8 (see task_passes()) -- which the FALSE status
   * the line returned before this never ticked.
   */
  if (silent_task_390 && !status && pf_buffer_length (filter) == task_mark)
    {
      pf_buffer_string (filter, "Done.\n");
      status = TRUE;
    }

  /*
   * 4.0: a line a task answered that also names a term two present
   * characters share is not a turn.  run400 generaltasks ticks only when
   * `MemVar_4941EC = &HFF` (48B5B5: turns, walks and events), and the
   * namesake scan sets that index before the tick; the block at 48B60C then
   * sees "a task ran for this line" (MemVar_4941F8 = 1), prints the task's
   * text instead of asking "Which ...", and resets the index at 48BB92 -- so
   * no question and no tick.  Measured on Sun Empire (run400x
   * sun_empire_site.txt, VBRNG_SEED=10, commands 58 and 63, site-tagged
   * draws): `get sample from orgaan soldier` with the two soldiers Skyrv and
   * Skynd present runs task 63/64, and neither the Code Red siren nor the
   * battle draws a thing that turn.  characters()' give-branch co() scan
   * over the objects does the same for an object name that two present
   * objects share (lib_co_400_line_leaves_which_pending(); Cyberclones II
   * `give electric uniform to lightning`).
   */
  if (status && !game->is_admin && run_any_task_ran_this_command ()
      && (lib_npc_400_line_names_namesakes (game, string)
          || lib_co_400_line_leaves_which_pending (game, string)))
    game->is_admin = TRUE;

  /*
   * 4.0: a handler's own "Which" prompt on a line holding "with" is undone
   * by openclose, which runs below the handlers and rewrites the pending
   * index from the text after "with" (475C63-475D6C).  Where it leaves the
   * index at -1 the prompt stays printed but the question is never
   * registered and the line is a turn: `take stone with knife` ticks,
   * `take stone` does not.  See lib_openclose_with_half_400().
   */
  if (status && lib_co_400_named_question_raised ()
      && !run_any_task_ran_this_command ()
      && lib_openclose_with_half_400 (game, string))
    {
      lib_co_400_drop_question ();
      game->is_admin = FALSE;
    }

  /*
   * 4.0: and where openclose's loop leaves the index at an object, with no
   * handler question and no task, generaltasks asks it instead of whatever
   * the line printed: `x rope with stone`, `wear flint with stone`.  A line
   * claimed above openclose never gets there.  See
   * lib_openclose_with_half_raise_400().
   */
  /*
   * openclose's loop runs on a line nobody answers, too: therest's
   * DontUnderstand comes after it (`cut rope with gems` leaves "emerald").
   */
  if (reached_library && game->is_running
      && !run_any_task_ran_this_command ())
    lib_openclose_with_antecedent_400 (game, string);
  if (status && reached_library && game->is_running
      && !run_any_task_ran_this_command ()
      && lib_openclose_with_half_raise_400 (game, string))
    game->is_admin = TRUE;

  /*
   * 4.0: and when no task ran, the same scan ASKS.  The question belongs to
   * generaltasks (48B6AE-48BB92), not to any verb: it comes after everything
   * the line printed and replaces it, whatever answered.  Measured
   * 2026-09-20 on p4BATT (Ann and Bob both "a guard", Dave and a third guard
   * next door; run400x runner_probes/batt.run400.txt and
   * runner_probes/batt.run400.b.txt) -- `x guard`, `attack guard`, `attack
   * guard dave`, `status guard`, `talk to guard`, `where is guard`, `give
   * stone to guard`, `give club to guard` and the bare noun `guard` all
   * answer "Which guard.  A guard or a guard?", and the last two lose a real
   * answer ("You don't have the stone!") to it.  `probe`, a task, keeps its
   * own text (the 48B60C gate above).
   *
   * Not over a battle line that settled on one target or asked which was
   * meant (lib_battle_400_line_claimed()), nor over that question's answer
   * (lib_battle_which_pending()): the deviation logged in
   * lib_battle_attack_many() keeps the blow, or the question, in view and
   * lets `guard` answer "Guard or Droid?".
   *
   * Both halves of the question read ONE untyped index, MemVar_4941EC, and
   * the object half (48B6B1) gets first refusal: it runs when that index is
   * also a valid object index AND the line names that object, printing the
   * OBJECT's name over the character list.  In p4BATT the flagged character
   * is Bob, index 2, and object 2 is the stone, so `attack guard with stone`
   * and `give stone to guard` are "Which stone.  A guard or a guard?" while
   * `x guard sword` (object 0) and `attack guard with club` (object 1) are
   * "Which guard." -- a plain index collision, not a term choice.  Left
   * unmodelled; see the lead in test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
   */
  if (run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400
      && game->is_running
      && !run_any_task_ran_this_command ()
      && !lib_battle_400_line_claimed ()
      && !lib_battle_which_pending ()
      && lib_npc_400_line_names_namesakes (game, string))
    {
      pf_empty (filter);
      lib_npc_400_raise_for_line_string (game, string);
      status = TRUE;
    }
}

/*
 * run_line_t::finish()
 *
 * Clears the per-line stores and returns RESULT.
 */
scr_bool
run_line_t::finish (scr_bool result)
{
  if (SCR_TRACING_DEVIATIONS () && uip_get_lenient_tasks ())
    {
      std::string tasks;
      size_t task;

      for (task = 0; task < run_tasks_ran_this_command.size (); task++)
        if (run_tasks_ran_this_command[task])
          {
            const scr_task_commands_guard task_commands;
            std::string commands;

            for (const scr_char *pattern :
                 run_task_command_patterns (game, (scr_int) task, TRUE))
              commands += (commands.empty () ? "" : " | ")
                          + std::string (pattern);
            tasks += (tasks.empty () ? "" : " ; ") + std::to_string (task)
                     + " {" + commands.substr (0, 300) + "}";
          }
      if (!tasks.empty ())
        SCR_DEVIATION ("lenient_task", "tasks=%s", tasks.c_str ());
    }
  run_dispatch_input = nullptr;
  run_tasks_ran_this_command.clear ();
  return result;
}

/*
 * run_line_t::run()
 *
 * The passes in generaltasks' order.  The two guards live here so that
 * they hold for the whole line, and the design note in
 * run_all_commands() says why the passes stand where they do.
 */
scr_bool
run_line_t::run (int force)
{
  reset ();
  const scr_lenient_tasks_guard lenient_tasks (force == 0 ? lenient ()
                                               : force > 0);
  if (spent_claim_390 ())
    return finish (TRUE);

  put_prepass ();
  const run_put_class_guard put_class
      (!put_first && !repeat_pending
       && lib_put_held_unsplit_400 (game, put_line));
  put_pass ();
  get_outer_400 ();
  task_peek ();
  priority_pass ();
  task_passes ();
  battle_pass ();
  library ();
  settle ();
  return finish (status);
}

/*
 * run_all_commands()
 * run_game_task_commands()
 *
 * Alternative facets of run_game_commands_common().  The first is used by the
 * main user input handling loop; the latter by the library when looking for
 * game commands that override standard actions.
 */
scr_bool
run_all_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Adrift command matching is just weird, perhaps broken.  In theory, a
   * game can override system commands with a properly constructed task and
   * set of command matchers.  However, the Runner isn't terribly consistent
   * in when this will work and when not, and some games rely on that in-
   * consistency.  In particular, a game with a "* object" task that has
   * failing restrictions will not be able to override the system's "take
   * object", whereas a game's "take object", under the same circumstances,
   * will.  Yet if the restrictions pass, a game's "* object" overrides the
   * system's "take object" with no apparent difficulty.
   *
   * For example, "The Woods Are Dark" has a "* ball *" task with the
   * restriction "must be holding ball".  Without special casing it, there's
   * no way to get the ball in the first place.
   *
   * Trying to find the right way to do things here, then, has been tricky.
   * Here's the current process:  First, try "priority" system commands; ones
   * that move objects to inventory.  These system commands will call back
   * into trying game commands for objects taken or dropped, and in those
   * tries, allow overrides only if the game task is explicit about what it's
   * doing -- it doesn't start with "*", or it does but explicitly names a
   * verb (see run_match_task_commands() for the run400 probe results behind
   * that rule) -- and handle restrictions in those tries.  Next, run game
   * commands directly, ignoring any cases where restrictions fail to let the
   * task run.  After that, retry all game commands again with restrictions
   * enabled.  And finally, try all other standard library commands.
   *
   * Priority commands go BEFORE the direct game-command pass (not after, as
   * an earlier version of this had it): probe DONE, task 72 in "Space Boy's
   * First Adventure" is the literal, unrestricted, textless command "drop
   * cape to the floor" (+250 score, no message).  run400 answers the typed
   * command with the library's ordinary "Player drop the cape." and the
   * score UNCHANGED (runner_probes/et2.run400.txt,
   * runner_probes/et4.run400.txt, 2026-08-23) -- the task never runs at all,
   * even though its own literal pattern matches the raw input exactly.  A
   * same-shaped task using a bare object with no trailing words ("* drop *
   * rock *", i.e. equivalent to the library's own canonical "drop <object>"
   * callback string) DOES run silently alongside the library's message
   * (runner_probes/et.run400.txt).  So it is specifically the priority
   * command's own callback -- matching only its constructed "verb OBJECT"
   * short form, not the raw typed line -- that gets first refusal on a
   * recognised system verb; a task whose pattern extends past that short form
   * (trailing words the library only swallows as free %text%) is never reached
   * once the library has already claimed and finished the command.  Swapping
   * the two passes reproduces that: on a recognised system verb,
   * run_priority_commands() (and its short-form task callback) gets the first
   * and often the only look, and the direct, ignore-restrictions task pass
   * only sees what the priority commands didn't recognise as a system verb at
   * all (so "etcontrol"-style non-system task commands are unaffected).
   *
   * But that swap alone regresses the same game's task 27, "{take/get}
   * {them/boots}" (CompleteText "Taken."): the callback's constructed short
   * form is built from the object's display Short name ("get pair of Flight
   * Boots"), never from an alias like "boots", so the callback attempt fails
   * for a reason unrelated to task 72's "trailing words" problem, and
   * priority's own generic take now wins a task the golden walkthrough (and,
   * definitionally, the pre-swap Runner-matching behaviour this port has
   * always had) says must win instead.  (The bare swap is not a near miss:
   * it fails 75 of the 262 corpus walkthroughs, because scarier's callback
   * string is weaker than the Runner's -- Short name only, no aliases -- so
   * far more tasks are unreachable here than there.)  So a peek at the direct
   * task pass runs BEFORE priority after all, and task 72's shape is excluded
   * from it: silent (no CompleteText, ShowRoomDesc, AdditionalMessage or
   * status-setting action) AND unreachable from any library short form for
   * any object ("drop cape" / "drop the cape" can never satisfy a pattern
   * that insists on "to the floor").  Both halves are needed -- silence alone
   * loses "Sommeril" task 35 and 3 more walkthroughs, unreachability alone
   * loses 27 of them.
   *
   * Excluding a task means abandoning the ENTIRE peek, not reading past it to
   * the next task.  ADRIFT resolves a command to the lowest-indexed matching
   * task, so stepping over a match to let a later task win invents a Runner
   * behaviour that does not exist: "The Forum" task 1 (silent literal "x ...
   * hand", which falls through to the library's own examine) was being
   * skipped in favour of task 2, "[examine/read/x/l/look]{at}[%object%]",
   * whose CompleteText then answered every examine in the game.  That cost
   * 8 corpus walkthroughs (forum, forum2, cursed, iqsfot, sommeril, funhouse
   * and both to_hell_and_beyond replays) until the peek was made all-or-
   * nothing.  On abandonment priority gets its look next, and if it does not
   * claim the command the immediately following unexcluded pass re-matches
   * from task 0, unchanged from the original order.
   */
  /*
   * The carrying-capacity accounting toggle is exposed as a Glk port command
   * ("glk capacity"), handled in the front end before input ever reaches the
   * interpreter, so there is no administrative meta-command to match here.
   */
  /*
   * Deliberate deviation: a line only a verb-less task command takes
   * leniently is the Runner's first, and the task's only if the library
   * has nothing for it; see run_line_yields_to_library().  Nothing the
   * library says to such a line changes the game, so its words are simply
   * taken back.
   */
  if (run_line_yields_to_library (game, string))
    {
      const size_t mark = pf_buffer_length (gs_get_filter (game));
      scr_bool answered;

      lib_non_answer = FALSE;
      {
        run_line_t runner_line (game, string);

        answered = runner_line.run (-1) && !lib_non_answer;
      }
      lib_non_answer = FALSE;
      if (answered)
        {
          SCR_DEVIATION ("library_first", "kept the library's answer");
          return TRUE;
        }
      pf_truncate (gs_get_filter (game), mark);
      run_line_t lenient_line (game, string);

      return lenient_line.run (1);
    }

  run_line_t line (game, string);

  return line.run ();
}

scr_bool
run_game_task_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Try game commands, and note that this is a library call so that the parse
   * matcher can exclude game commands that begin with a '*' wildcard.
   *
   * Restrictions are honoured -- meaning a task whose restrictions fail with a
   * message gets run for the message, beating the library action -- from 4.0
   * on only.  Probe DONE, task `* get * gem *` restricted to "holding the
   * stone", measured 2026-08-23: run400 answers `get gem` without the stone
   * with "BLOCK-GEM.", while run390 on the 3.9 twin probe quietly takes the
   * gem instead.  Pre-4.0 the library wins, so the loud pass is switched off
   * here rather than being allowed to claim the command.  Tasks whose
   * restrictions PASS still run in either version -- the first loop below does
   * not consult this flag.
   */
  const scr_bool include_restrictions =
      run_get_version (gs_get_bundle (game)) >= TAF_VERSION_400;

  SCR_TRACE_IF ("SCR_TRACE_MATCH", "DISPATCH input=[%s]\n", string);

  /*
   * This is the dispatcher 44CCE0 as the library handlers call it, and its
   * task_pick refreshes the restriction cache first: 3monkeys' take piece
   * dispatches "get the coconut husk" after its pre-match hit on task 616's
   * stale results, and that dispatch's own fallback, reading the fresh
   * ones, finds nothing to say (run400x site trace, T41).
   */
  if (include_restrictions)
    run_restriction_cache_task_pick (game, string);

  return run_game_commands_common (game, string, include_restrictions, TRUE,
                                   FALSE);
}

/*
 * run_passing_task_commands()
 *
 * run_game_task_commands() without the restriction-failure pass: only a task
 * whose restrictions pass can claim the line, and a FailMessage never does.
 * For lib_try_typed_put_line_400(), whose look-up the Runner never makes --
 * there a failing task must not take the line away from the library put the
 * game may be waiting on (deadman's `put hand on plate`, "You'll need an
 * additional thumbprint.").
 */
scr_bool
run_passing_task_commands (scr_gameref_t game, const scr_char *string)
{
  return run_game_commands_common (game, string, FALSE, TRUE, FALSE);
}

/*
 * run_typed_line_task_commands()
 *
 * Offer a line to the tasks the way run390 does from inside a library
 * handler on the player's own words: a bare `*` matches, as it would from
 * the dispatcher.  Quiet (`loud` FALSE) is the handler's own tasks(1), which
 * is checktask(line, 0) (42BD5E): passrest() puts the entry buffer back when
 * running is 0 (452BE6), so a matched task whose restrictions fail says
 * nothing.  Loud (`loud` TRUE) is generaltasks' tasks(0) (45F48B), reached
 * when the handler's result stayed 0: checktask(line, 1) lets the failing
 * restriction's FailMessage overwrite the buffer (452BBD) and the put text
 * with it.  Still one task per line.  See lib_put_task_sweep_390().
 */
scr_bool
run_typed_line_task_commands (scr_gameref_t game, const scr_char *string,
                              scr_bool loud)
{
  return run_game_commands_common (game, string, loud, FALSE, FALSE);
}



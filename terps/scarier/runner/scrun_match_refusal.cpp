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
 * Answers for a line whose task cannot run: the repeat assist, the 3.9
 * spent-task claim, run_task_refusal(), the 4.0 put/take rerun and the
 * 3.7 second pass.
 *
 * Split out of scrun_match.cpp; see scrun_match_internal.h.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "scrun_match_internal.h"


/*
 * run_task_has_catchall_command()
 *
 * TRUE if any of the task's forward command patterns is a bare "*".
 *
 * Such a task matches every line the player types, and the Runner treats it as
 * a deliberate catch-all rather than as something the player got into the wrong
 * room for: run390's checktask sets the room flag for an out-of-room match and
 * then walks the task's own 25 command slots, clearing it again the moment one
 * of them is exactly "*" (loc_44B684 sets it, loc_44B6BC clears it).  Only the
 * forward Command list is walked, not ReverseCommand.
 */
static scr_bool
run_task_has_catchall_command (scr_gameref_t game, scr_int task)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);

  for (const scr_char *pattern : patterns)
    {
      if (strcmp (pattern, "*") == 0)
        return TRUE;
    }
  return FALSE;
}

/*
 * Optional "repeat assist" mode (opt-in, off by default; sibling of the
 * combat and move assists).  The pre-4.0 spent-task claim below is the
 * Runner's, measured, but in a few 3.90 games it sits on the critical path:
 * a finished, non-repeatable task whose command is an exit (Vampire's T61
 * `e` out of the Bozo backyard, Merry Murders' T46 `n` into the archives)
 * answers every later use of that exit with its RepeatText, so the player
 * can never leave and the game walls (70/100, 120/135).  With the assist on
 * the claim is skipped, and the line goes on to the handlers below tasks(0)
 * -- movement, look, examine -- as it did before the claim was ported.
 * Strictly opt-in, as it deliberately diverges from run390.
 */
scr_bool run_repeat_assist = FALSE;

void
run_set_repeat_assist (scr_bool flag)
{
  run_repeat_assist = flag;
}

scr_bool
run_get_repeat_assist (void)
{
  return run_repeat_assist;
}

/*
 * run_spent_task_390()
 *
 * The pre-4.0 task dispatcher's claim, read off run390's checktask
 * (Proc_19_?_44A9EE; generaltasks calls tasks(0) at 45F48B, which calls
 * checktask(text, 1)).  The scan runs over the WHOLE task table IN INDEX
 * ORDER, and for each task walks its command slots.  A task whose command
 * matches -- a bare "*" included -- and that is done and not repeatable
 * (44B4F4, 44B4FE) writes its RepeatText slot straight into the message
 * buffer (44B537: MemVar_468154 = record(200).global_0, no test of what the
 * buffer already held) and then CONTINUES with the next task (GoTo 44B66C ->
 * 44B6CC, which sits just above the loop's Next at 44B6DA).  A live match
 * goes to the restriction loop at 44B5F4 instead: all pass and the task is
 * the scan's result (44B663), one fails and passrest has written its fail
 * message, if it has one, over whatever the buffer held (452BB8), and the
 * scan moves on (44B636).  So the buffer ends up holding the LAST message
 * written in table order, RepeatText or fail message alike, and tasks()
 * (42BDC4) either executes the passing task -- whose text replaces the
 * buffer, which is why inverness's third `knock` prints task 23's text
 * although the spent task 22 stands above it
 * (runner_probes/inverness.run390.txt) -- or, when the buffer changed and
 * nothing passed, returns -1 and generaltasks prints the buffer and skips
 * everything below tasks(0): movement, look, examine, score, the room
 * refusal and therest().
 *
 * At load, openadv substitutes person(0) & " have already done that." into
 * an empty RepeatText slot (465A8B-465AB9), so the default message and an
 * authored RepeatText are one field; an authored " " (Vampire.taf) is not
 * empty and prints as itself.  The slot is therefore never empty at 44B537,
 * and a spent match always changes the buffer.
 *
 * Measured on the Runner transcripts of The Long Journey Home (run390
 * runner_probes/journ2.run390.t5.txt: `fly`, `north`, `w`, `x card`, `x
 * king`, `e` all "You have already done that." after task 5's `*` is spent;
 * `i` still lists the inventory and `x creature` still describes the
 * creature), Lair of the CyberCow (runner_transcripts/cybercow_win.txt: the
 * second `fix robot` prints task 80's RepeatText "The invincible robot is
 * structurally complete...", not the library's "I don't think you can fix
 * the robot."), inverness (runner_probes/inverness.run390.txt: `z`, `look`,
 * `score` all claimed; the five `knock`s run tasks 20, 22, 23, 24, 25 in
 * turn), circus (runner_transcripts/circus_sold_points.txt: `ask barb about
 * tape` claimed by task 77, and the NPC walk ticks, so the claim is a turn)
 * and chicago's `listen` (task 18).  4.0 moved the claim
 * to 48A481 and asks the restrictions there; see run_task_refusal() and
 * run_repeat_survivor_400().
 *
 * Returns the claiming spent task, or -1 when no spent task matches or a
 * live matching task passes its restrictions (that task runs instead).
 * When a task is returned, *message is what the buffer holds at the end of
 * the scan: the last RepeatText or restriction fail message written, never
 * NULL.  Prints nothing.
 */
scr_int
run_spent_task_390 (scr_gameref_t game, const scr_char *string,
                    const scr_char **message)
{
  scr_int task_count, task, spent;
  const scr_char *buffer;

  *message = nullptr;
  if (scr_strempty (string))
    return -1;

  spent = -1;
  buffer = nullptr;
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      scr_bool pass;
      const scr_char *fail_message;

      /*
       * run390's reverse pass (44B1B9-44B4D7), skipped when a forward
       * command matched a task that is not done (44B18F).  A reverse command
       * of a done or repeatable task is checktask's -2 in the task's rooms
       * (44B4A4 / 44B64D) -- the reversal runs, and the task passes below
       * handle it -- and nothing elsewhere.  A reverse command of any other
       * task writes its RepeatText into an EMPTY buffer (44B4B2), with no
       * room test: matt's `out` is "You have already done that." everywhere
       * until the boss room is entered (probe p39REV, run390x
       * runner_probes/rev.run390.txt, 2026-09-24).
       *
       * Deliberate deviation (2026-09-30): not ported.  The claim names a
       * task that was never done, in every room, so a room's own `out`
       * exit could not be taken and `leave box` was refused too
       * (32a3f3423).  The line goes on to the library.
       */
      if (SCR_TRACING_DEVIATIONS () && !buffer
          && task_is_reverse_refused_390 (game, task)
          && !run_match_task_commands (game, task, string, TRUE, FALSE)
          && run_match_task_commands (game, task, string, FALSE, FALSE))
        SCR_DEVIATION ("spent_reverse_390", "task=%ld", task);
      if (FALSE && !buffer
          && task_is_reverse_refused_390 (game, task)
          && !run_match_task_commands (game, task, string, TRUE, FALSE)
          && run_match_task_commands (game, task, string, FALSE, FALSE))
        {
          buffer = prop_get_indexed_string (gs_get_bundle (game), "Tasks",
                                            task, "RepeatText");
          spent = task;
          continue;
        }

      if (!task_where_allows_run (game, task))
        continue;

      if (task_can_run_task_directional (game, task, FALSE)
          && run_match_task_commands (game, task, string, FALSE, FALSE))
        return -1;

      if (!run_match_task_commands (game, task, string, TRUE, FALSE))
        continue;

      if (task_is_done_refused (game, task))
        {
          const scr_prop_setref_t bundle = gs_get_bundle (game);

          buffer = prop_get_indexed_string (bundle, "Tasks", task,
                                            "RepeatText");
          spent = task;
          continue;
        }

      if (!task_can_run_task_directional (game, task, TRUE))
        continue;

      /* A live task with passing restrictions executes instead. */
      if (!restr_eval_task_restrictions (game, task, &pass, &fail_message))
        pass = TRUE, fail_message = nullptr;
      if (pass)
        return -1;
      if (fail_message && !scr_strempty (fail_message))
        buffer = fail_message;
    }

  if (spent >= 0)
    *message = buffer;
  return spent;
}

/*
 * run_spent_survivor_390()
 *
 * The handlers run390's generaltasks runs ABOVE tasks(0), each of which ends
 * the line when it printed anything (GoTo loc_460589 on a nonzero result):
 * takes() 45F439, drops() 45F44A, inventory() 45F45B and insides() 45F471.
 * takes() answers only for an object co() resolved -- "Take what?" is a
 * later fallback -- so `take shovel` with no shovel about falls to the
 * spent task (journ2).  A put that the target turns away is still
 * insides()'s answer, so the tentative priority pass's deferral is finished
 * here out of the standard rows.
 *
 * Below tasks(0) only the character pass survives, because characters()
 * (45ACD8, called at 460675 on every line) OVERWRITES the message buffer:
 * `x creature` after the journ2 claim prints the creature's description.
 * The other character answers are not measured to survive -- circus's `ask
 * barb about tape` does not -- so only the NPC examine is taken here.
 *
 * TRUE if a survivor answered the line.
 */
scr_bool
run_spent_survivor_390 (scr_gameref_t game, const scr_char *string)
{
  static scr_commands_t NPC_EXAMINE_COMMANDS[] = {
#ifdef SCARIER_NO_ABBREVIATIONS
    {"[ex/exam/examine/look {at}] %character%", lib_cmd_examine_npc},
#else
    {"[x/ex/exam/examine/look {at}] %character%", lib_cmd_examine_npc},
#endif
    {nullptr, nullptr}
  };

  if (run_priority_commands (game, string))
    return TRUE;
  if (run_priority_deferred && run_standard_commands (game, string))
    return TRUE;

  return run_try_command_table (NPC_EXAMINE_COMMANDS, game, string);
}

/*
 * run_spent_claim_390()
 *
 * Print the spent claim: the buffer run_spent_task_390() ended with, which
 * for a RepeatText the author left empty is the already-done default that
 * openadv installs at load (see run_spent_task_390()).
 */
void
run_spent_claim_390 (scr_gameref_t game, const scr_char *message)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);

  if (message[0] != NUL)
    {
      pf_buffer_paragraph_line (filter, message);
      return;
    }

  pf_buffer_string (filter,
                    prop_get_global_integer (bundle, "Perspective")
                    == LIB_FIRST_PERSON ? "I" : "You");
  pf_buffer_paragraph_line (filter, " have already done that.");
}

/*
 * run_task_refusal()
 *
 * The Runners have two answers of their own for a command that matches a task
 * the main dispatcher would not run, both of which Scarier used to leave to the
 * standard library or to the game's DontUnderstand text:
 *
 *   - the player is in the wrong room for the task.  Pre-4.0 only: 3.7 and 3.8
 *     answer "You can't do that here.", 3.9 "You can't do that here!", and 4.0
 *     dropped the message (the string is still in run400.exe, unused).
 *   - the task has already been done and is not repeatable.  The Runner prints
 *     the task's RepeatText if it has one -- in EVERY version, 4.0 included --
 *     and otherwise "You have already done that.", which like the room refusal
 *     is a pre-4.0 message only (` have already done that.` is a UTF-16 literal
 *     in run370/run380/run390 and absent from run400).
 *
 * Measured live against run370 (Castle Quest), run380 (Marooned), run390 (The
 * Hangover plus the p39where probe built by test/adrift4/harness/
 * make_39_whereprobe.py) and run400 (make_400_whereprobe.py), 2026-08-09/10.
 *
 * The conditions are narrow, and the probes say how narrow.  A task refused by
 * a *restriction* that fails silently gets "I don't understand.", as does a
 * command that matches no task pattern at all.  Runner P-code guards the room
 * message with `OUT = "" And FLAG = 1`, so any output at all -- including a
 * standard library answer -- suppresses it, which is why this runs last, only
 * when nothing else claimed the input.
 *
 * The two refusals are ordered, and the order is measured rather than assumed:
 * probe task "theta" is done AND out of its room, and run390 answers "You can't
 * do that here!", so the room half is tested first.  That is why the
 * already-done test carries the room condition with it (task_is_done_refused()).
 *
 * Within one command the Runner scans every task and does not stop at the first
 * refusable one.  The already-done half writes its message as it goes, so the
 * first such task wins and ends the scan; the room half only raises a flag, and
 * each later out-of-room match overwrites it -- so it is the LAST matching
 * out-of-room task that decides, and a catch-all "*" task clears the flag for
 * good (run_task_has_catchall_command()).  Melbourne Beach (3.90) is the
 * measured case: its task 94 is `*` confined to room 0, so run390 answers "I
 * don't understand what you mean!" and not "You can't do that here!" for `play
 * volleyball` and `use shower` typed outside their rooms
 * (runner_probes/melbourne_beach.run390.txt).
 *
 * Both count as a turn, unlike DontUnderstand: in the probe, an event ticking
 * once a turn fires on either refusal and not on the parser complaint, matching
 * the `handled = 1` the Runner sets alongside the message.  Hence the TRUE
 * return, which lets the caller run the turn.  (Measured for all three pre-4.0
 * answers, and for 4.0's RepeatText on 2026-09-08: p4REPEAT2/p4REPEAT3 carry a
 * once-a-turn event and every RepeatText line in
 * runner_probes/repeat2.run400.txt and runner_probes/repeat3.run400.txt is
 * followed by its "TICK.".)
 *
 * The leading word follows Perspective, which pre-4.0 has only two of: run390
 * answers "I can't do that here!" / "I have already done that." for Perspective
 * 0 and the "You" forms for every other value, third person being a 4.0
 * addition (its inventory says "You are carrying nothing." for Perspective 2 as
 * well).
 */
enum { REFUSAL_NONE = 0, REFUSAL_ROOM, REFUSAL_DONE };

/*
 * Which of the four calls a scan is: the pass ahead of the standard library
 * (which answers the already-done half), the pass INSIDE it that answers the
 * room half, the pass after all of it, and a silent look-ahead that only
 * reports whether the pre-library pass has something to say.
 * run_all_commands() needs that last answer before it runs the priority
 * commands, because 4.0's RepeatText outranks those as well.
 */

/*
 * run_refusal_scan()
 *
 * Walk every task, looking for ones whose command matches the input and that
 * the dispatcher passed over for one of the two refusable reasons.  A task
 * blocked by anything else can never raise a refusal.  The already-done half
 * stops the scan where it fires; the room half keeps going, last one wins.
 *
 * Returns the refusal found, and for REFUSAL_DONE the task that raised it.
 * The done half is left out of the scan if room_only.
 */
static scr_int
run_refusal_scan (scr_gameref_t game, const scr_char *string,
                  scr_int version, scr_bool room_only, scr_int *refused_task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int task_count, task, direction;
  scr_int refusal;
  scr_bool is_room_refused;

  refusal = REFUSAL_NONE;
  *refused_task = -1;
  is_room_refused = FALSE;
  task_count = gs_task_count (game);
  for (task = 0; task < task_count; task++)
    {
      /* The room half first -- see the note on probe task "theta" at
         run_task_refusal().  */
      if (version < TAF_VERSION_400)
        {
          for (direction = 0; direction < 2; direction++)
            {
              const scr_bool is_forwards = !direction;

              /*
               * run390 raises the flag for a forward match only (44B66F);
               * an out-of-room reverse command is not understood (probe
               * p39REV `unpoke`).
               */
              if (!is_forwards && version == TAF_VERSION_390)
                continue;

              if (task_is_room_refused (game, task, is_forwards)
                  && run_match_task_commands (game, task, string,
                                              is_forwards, FALSE))
                {
                  is_room_refused =
                      !run_task_has_catchall_command (game, task);
                  break;
                }
            }
        }

      /*
       * A task the current command has just completed is not "already done"
       * for that command -- p39done.taf's silent `* x * scroll *` task runs,
       * prints nothing, and run390 then answers "I don't understand." rather
       * than "You have already done that." (2026-08-23).
       */
      /*
       * 4.0 also asks the restrictions.  A spent task whose restrictions now
       * fail is not the one the picker hands the dispatcher, and the library
       * gets the line as if the task were not there at all: `A Witch Tale`
       * task 2 is the literal `* north` at the bridge, spent from the first
       * crossing attempt and carrying a RepeatText, but restricted on `say
       * grue` being undone -- once the riddle is answered run400 walks the
       * player north (runner_transcripts/witchtale.txt) instead of printing "I
       * try to cross the bridge...".  Only silent failures are covered here; a
       * fail message of its own is the restriction pass's business, not this
       * one's.  Pre-4.0 keeps the shape chicago.taf measured.  At 4.0 the
       * post-library pass asks the restrictions too: it once let a spent,
       * restriction-failing task through as a fallback, for `The Magic Show`'s
       * "One rabbit trick is enough for any given act" on `show rabbit to
       * audience`, but run400 answers that line with the object catch-all "I
       * don't understand what you want to do with the audience."
       * (runner_probes/magicshow.run400.a.txt:47,
       * runner_probes/magicshow.run400.b.txt:40), and hcw's literal `2` (task
       * 240, spent, both restrictions failing) with the DontUnderstand text
       * (runner_probes/hcw.run400.txt, turn 227), not either RepeatText.
       */
      /*
       * The room-half pass does not look at the done half at all: the done
       * half breaks out of the scan where it fires, and a task refused as
       * done further up the table would then hide a later out-of-room match
       * from the "last one wins" rule.
       */
      /*
       * 4.0 also passes over a spent task with no RepeatText of its own: every
       * 4.0 answer below needs one, and the picker offers the dispatcher the
       * spent task that has it.  The Crooked Estate's wallpaper is tasks 47
       * (spent, silent) and 48 (spent, RepeatText) on the same commands, and
       * the third `peel wallpaper` gets 48's "I rip another, but another layer
       * hides behind that." (runner_transcripts/crookedestate.txt), not the
       * object catch-all.
       */
      if (!room_only
          && !run_task_ran_this_command (task)
          && task_is_done_refused (game, task)
          && (version < TAF_VERSION_400
              || (run_task_restriction (game, task) == RUN_UNRESTRICTED
                  && !scr_strempty (prop_get_indexed_string
                                      (bundle, "Tasks", task, "RepeatText"))))
          && run_match_task_commands (game, task, string, TRUE, FALSE))
        {
          refusal = REFUSAL_DONE;
          *refused_task = task;
          break;
        }
    }
  if (refusal == REFUSAL_NONE && is_room_refused)
    refusal = REFUSAL_ROOM;
  return refusal;
}


/*
 * run_refusal_print()
 *
 * Print the answer to a refusal that run_refusal_scan() found.  An authored
 * RepeatText replaces the already-done message, and is the one part of all
 * this that 4.0 kept.  Returns FALSE if there is nothing to say: a 4.0 task
 * with no RepeatText.
 */
static scr_bool
run_refusal_print (scr_gameref_t game, scr_int version,
                   scr_int refusal, scr_int refused_task)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int perspective;
  const scr_char *repeattext;

  repeattext = nullptr;
  if (refusal == REFUSAL_DONE)
    {
      repeattext = prop_get_indexed_string (bundle, "Tasks", refused_task,
                                            "RepeatText");
      if (scr_strempty (repeattext))
        {
          repeattext = nullptr;
          if (version >= TAF_VERSION_400)
            return FALSE;
        }
    }

  if (repeattext)
    {
      pf_buffer_paragraph_line (filter, repeattext);
      return TRUE;
    }

  perspective = prop_get_global_integer (bundle, "Perspective");

  pf_buffer_string (filter,
                    perspective == LIB_FIRST_PERSON ? "I" : "You");
  if (refusal == REFUSAL_ROOM)
    {
      pf_buffer_paragraph_line (filter,
                                version < TAF_VERSION_390
                                ? " can't do that here."
                                : " can't do that here!");
    }
  else
    pf_buffer_paragraph_line (filter, " have already done that.");
  return TRUE;
}


/*
 * The two halves do NOT sit at the same point in the dispatch order, and
 * chicago.taf (3.90) is the measurement that separates them.  Its task 18 is
 * `listen`, confined to rooms 2 and 8 and not repeatable, and the walkthrough
 * types `listen` twice inside those rooms.  The second time run390 answers
 * "You have already done that." -- not the library's "You hear nothing out of
 * the ordinary.", which run390 certainly has (` hear nothing out of the
 * ordinary.` is a UTF-16 literal in all four Runners, so this is not a
 * vocabulary difference).  The already-done message therefore beats the
 * standard library, exactly as the P-code shape says: the done half writes its
 * message during the task scan, while the room half only raises a flag that
 * `OUT = "" And FLAG = 1` later discards if anything else printed.
 *
 * So the done half runs BEFORE run_standard_commands() and the room half
 * later, and `done_only` / `room_only` select which.  The scan itself is
 * identical in every pass, because the room half still has to be tested first
 * WITHIN a pass -- probe task "theta" is done AND out of its room and run390
 * answers the room refusal.  The pre-library pass simply declines to emit a
 * room refusal and leaves it to the ones below.
 *
 * How much later the room half runs is measured, and it is NOT after the whole
 * library.  run390's generaltasks() (Public Sub generaltasks '460D6C, body from
 * loc_45EC34) clears the flag at 45EC7C, runs its() then tasks() (which is what
 * sets the flag, in checktask at loc_44B681), then its named per-verb handlers
 * -- wears() removes() dobattle() dohints() sitstand() openclose() viewroom()
 * the take code, adventure_Click(), whereis(), fonts(), gotoplace() -- and only
 * THEN reaches
 *
 *     loc_45FFE8:  If msg = "" And MemVar_468228 = 1 Then
 *                      msg = person(0) & " can't do that here!"
 *     loc_460004:  If msg = "" Then Call therest()
 *
 * therest() is the generic catch-all bucket: "You can't <verb> that.", "Give
 * what?", " is for sale.", the "Uh huh, yes, very interesting." of `say`.  All
 * of those lose to the room refusal, because the flag is tested one line above
 * the call.  Scarier's analogue of therest() is STANDARD_FALLBACK_COMMANDS, so
 * the room pass sits between run_standard_verb_commands() and
 * run_standard_fallback_commands(), not after both.
 *
 * Measured on ALEXIS.TAF under run390
 * (runner_probes/alexis_worn_cube.run390.txt): `turn
 * ring`, `buy metal helmet`, `open cupboard`, `open door`, `unlock door`, `give
 * stones to larnt` and `say the password` are all answered "You can't do that
 * here!" where Scarier reached "You can't turn that.", "I don't think that is
 * for sale.", "You can't open that.", "You can't unlock that." and "Uh huh,
 * yes, very interesting." -- eight turns on that row alone, every one of them a
 * STANDARD_FALLBACK_COMMANDS row.  The handlers that run ABOVE loc_45FFE8 keep
 * their answers, which is why the post-library pass has to stay: `put water in
 * pan` on the same row is run390's "You can't do that!" from a handler, not the
 * room refusal.
 *
 * What is NOT above the flag test, contrary to how generaltasks reads at a
 * glance, is the character catch-all.  Both `Call characters()` sites in
 * generaltasks (45FD08 and 460675) are the turn-advance pair `characters() :
 * events()` -- the NPC walk and the event tick -- and the second of them is
 * BELOW `Call therest()` at 460004; the "I don't understand what you want to
 * do with <Name>." / "<Name> is not here!" / "Who?" tail rides along there.
 * The object catch-all is below it too (45D35C inside therest(), 46024A in
 * generaltasks' own tail).  So both stay at the foot of the fallback table,
 * and the golden that pins it is the_hangover (3.90): `give approval notes to
 * platypus` with the platypus elsewhere answers "You can't do that here!" and
 * not "Platypus is not here!" (runner_transcripts/the_hangover.txt line
 * 215).  Moving the character row above the refusal also cost goldilocks
 * and yak_shaving (both 4.00) their `give X to Y` -> "Give what?", which is
 * the same ordering seen from 4.0's side.
 *
 * 4.0 has no already-done message at all, only RepeatText, and it sits ahead
 * of the library too -- ahead of MORE of it, in fact.  Measured 2026-09-08
 * with the p4REPEAT probes: the dispatcher prints the RepeatText and jumps
 * past every general verb, movement and take included, so the 4.0 pre-library
 * pass carries none of the three pre-4.0 conditions.  Only the handlers that
 * run before the dispatcher, or below the jump, keep their answer; they are
 * listed in run_repeat_survivor_400(), and the caller checks them.
 */
scr_bool
run_task_refusal (scr_gameref_t game, const scr_char *string,
                  run_refusal_pass_t pass)
{
  const scr_bool done_only =
      pass == REFUSAL_PASS_PRE || pass == REFUSAL_PASS_PROBE;
  const scr_bool room_only = pass == REFUSAL_PASS_MID;

  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int version, refusal, refused_task;

  /*
   * An empty input line element is not a command and gets no complaint of any
   * kind -- the same guard the DontUnderstand fallback uses.  Without it a
   * game with a bare "*" task command outside the player's room turns every
   * press-a-key blank line into a refusal.
   */
  if (scr_strempty (string))
    return FALSE;

  /*
   * A line that has already run a task gets no refusal.  In the Runner the
   * RepeatText is not a late pass at all: it is printed inside the task
   * dispatcher (Proc_19_24_44CCE0, called once at 48A481), which asks the
   * pre-matcher Proc_19_66_454EF0 for exactly ONE task and then either runs
   * it, reverses it, or prints its RepeatText.  So the task that refuses and
   * the task that runs are always the same one, and a line that ran a task
   * can never also be refused -- the restriction-failure pass at 44CCA5 is
   * likewise entered only when no task was found.
   *
   * Measured on easter.taf (run400 runner_probes/easter.run400.txt:304-308):
   * the winning `show basket to shopkeeper` runs task 63, which is silent
   * and only ends the game, and run400 prints nothing before the WinText --
   * not the "Since you already have Max's list..." RepeatText that a scan
   * over the other matching tasks turns up here.
   */
  if (run_any_task_ran_this_command ())
    return FALSE;

  version = run_get_version (bundle);

  /*
   * Both refusals are the Runner's answers to a line it matched, so the scan
   * matches the Runner's way even on a line the deviation gave lenient task
   * matching (run_line_matches_task_strictly()).  Leniently, Vampire's `look
   * at stick` matched an out-of-room stick task and answered "You can't do
   * that here!" over the library's "Nothing special.", and Grand Journey's
   * `climb bed` and `sit bench` the same (probe_lenient.sh, 2026-09-30).
   */
  const scr_lenient_tasks_guard strict_match (FALSE);

  refusal = run_refusal_scan (game, string, version, room_only,
                              &refused_task);
  if (refusal == REFUSAL_NONE)
    return FALSE;

  /*
   * The mid-library pass answers the room half and nothing else -- it stands
   * where run390 tests the flag, one line above Call therest(), so it outranks
   * the fallback verbs and nothing more.  The already-done half is not its
   * business: the pre-library pass has already had its go at that, and the
   * post-library pass keeps the wider fallback shape chicago.taf measured.
   */
  if (room_only && refusal != REFUSAL_ROOM)
    return FALSE;

  /*
   * The pre-library pass answers the already-done half and nothing else; a
   * room refusal found here waits for the mid-library pass, where the
   * Runner's own "did anything print?" guard applies to it.
   */
  if (done_only)
    {
      const scr_char *repeat;

      /*
       * Only the DONE half is answered early: the room half is guarded by
       * the Runner's own "did anything print?" test and stays late.
       *
       * Pre-4.0 this pass is a fallback now.  The claim itself is made in
       * run_all_commands() before any handler runs, by run_spent_task_390(),
       * which is where run390 makes it; what reaches here is a line that
       * probe declined because a live task earlier in the table outranked
       * the spent one, and that live task then did not run after all.
       */
      if (refusal != REFUSAL_DONE)
        return FALSE;

      repeat = prop_get_indexed_string (bundle, "Tasks", refused_task,
                                        "RepeatText");

      /*
       * 4.0 has no default already-done message and none of those three
       * conditions: measured 2026-09-08 (p4REPEAT/p4REPEAT2/p4REPEAT3 under
       * run400, runner_probes/repeat.run400.txt, repeat2.run400.txt and
       * repeat3.run400.txt), an authored RepeatText is printed by the task
       * dispatcher itself, ahead of nearly the whole library, and takes the
       * line away from a wildcard command and from movement just as readily
       * as from a literal one.  What it does NOT take
       * is listed in run_repeat_survivor_400(), which the caller tests -- the
       * answer is needed before the priority commands run, so a probe pass
       * reports it without printing anything.
       */
      if (version >= TAF_VERSION_400)
        {
          if (scr_strempty (repeat))
            return FALSE;
          if (pass == REFUSAL_PASS_PROBE)
            return TRUE;
        }
    }

  return run_refusal_print (game, version, refusal, refused_task);
}


/*
 * run_put_take_400()
 *
 * A 4.0 line holding a clauseless put and a take.  put_drop_list is the
 * first handler generaltasks calls, and its clauseless branch (46DC34)
 * writes "Where do you want to put <X>?" into the message buffer and falls
 * out without claiming, so get_outer (4582D8) has the line next.  A take
 * that succeeds saves the buffer (var_B0, 473597), composes its own line
 * and appends the saved text after the 44A9F4 separator (4736CD): `take put
 * coin` is "You take the coin. Where do you want to put the coin?".  The
 * already-carrying refusal appends to the buffer as it stands (462D4E):
 * `put take coin` with the coin held is "Where do you want to put the
 * coin?You are already carrying the coin.".  p4ORD (make_orderprobe.py)
 * cells 62 and 69 (run400x runner_probes/ord.run400.rest.txt, 2026-09-21).  A
 * put beside examine or drop already answers as the Runner does; any other
 * take outcome is not measured, and the line goes on as it did.
 */
scr_bool
run_put_take_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t mark = pf_buffer_length (filter);
  std::vector<scr_int> places;
  std::string question, take_line;
  scr_bool moved = FALSE;
  scr_int object;

  if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
      || !lib_input_contains_word (string, "put")
      || !(lib_input_contains_word (string, "take")
           || lib_input_contains_word (string, "get")
           || lib_input_contains_word (string, "pick"))
      || lib_input_contains_word (string, "all")
      || lib_input_contains_word (string, "and")
      || strstr (string, "take off"))
    return FALSE;

  run_dispatch_input = string;
  if (!lib_put_where_question_400 (game, &question))
    return FALSE;

  /* get_outer's own line: the put word plays no part in its object walk. */
  for (const scr_char *scan = string; *scan != NUL; )
    {
      if ((scan == string || scan[-1] == ' ')
          && scr_strncasecmp (scan, "put", 3) == 0
          && (scan[3] == NUL || scan[3] == ' '))
        {
          scan += 3;
          scan += strspn (scan, " ");
          continue;
        }
      take_line.push_back (*scan++);
    }
  while (!take_line.empty () && take_line[take_line.size () - 1] == ' ')
    take_line.erase (take_line.size () - 1);
  std::string hoisted;
  if (run_hoist_verb_line (game, take_line.c_str (), hoisted))
    take_line = hoisted;

  GS_FOR_EACH_OBJECT (game, object)
    {
      places.push_back (gs_object_position (game, object));
      places.push_back (gs_object_parent (game, object));
    }
  scr_bool status;
  {
    const run_dispatch_input_guard input (take_line.c_str ());

    status = run_priority_commands (game, take_line.c_str ());
  }
  if (!status)
    {
      pf_truncate (filter, mark);
      return FALSE;
    }
  GS_FOR_EACH_OBJECT (game, object)
    if (gs_object_position (game, object) != places[2 * object]
        || gs_object_parent (game, object) != places[2 * object + 1])
      moved = TRUE;

  std::string said = pf_cut_tail (filter, mark);
  if (moved)
    {
      while (!said.empty () && said[said.size () - 1] == '\n')
        said.erase (said.size () - 1);
      pf_buffer_string (filter, said.c_str ());
      pf_buffer_string (filter, " ");
      pf_buffer_string (filter, question.c_str ());
      pf_buffer_string (filter, "\n");
    }
  else if (said.find ("already carrying") != std::string::npos)
    {
      pf_buffer_string (filter, question.c_str ());
      pf_buffer_string (filter, said.c_str ());
    }
  else
    pf_buffer_string (filter, said.c_str ());
  return TRUE;
}


/*
 * run_takes_second_pass_370()
 *
 * run370 runs the task matcher TWICE on a take line that names an object.
 * takes() hands the line to tasks(1) itself (see lib_takes_offers_tasks_370())
 * and returns Empty, so generaltasks falls through to tasks(0) (43B972) and
 * matches the same line again against the world the first task left behind.
 * The two modes differ in what they do to the turn's string (00041B90): mode
 * 1 appends the CompleteText, mode 0 REPLACES the string with it.  What the
 * string holds by then is whatever was not printed yet -- a ShowRoomDesc's
 * viewroom prints everything before its exits sentence there and then
 * (@0003315C, pf_print_so_far()), which leaves only the exits in it; the
 * filter's pf_printed_to() note marks where they start.
 *
 * arlo (alices_restaurant) `get out of bus` at the church: the first pass
 * runs task 72 (`get out of *bus*`, Where the bus, Repeatable), which says
 * "You're on foot.", shows room 0 and moves the player there; the second
 * finds task 72's Where failing and task 107 -- the same five patterns,
 * Where room 0 -- matching instead, and its "You are no longer in the bus."
 * overwrites the exits.  So the Runner prints "... There is a mailbox here.
 * You are no longer in the bus." with no exits sentence, on both of the
 * walkthrough's visits (runner_probes/alices_restaurant.run370.rtf,
 * runner_transcripts/alices_restaurant).
 * Where nothing matches the second time -- `get out of bus` at the Dump,
 * `take garbage out of bus` -- the first task's text stands.
 *
 * Only the pass after a task ran is modelled.  A take line the library
 * answers also reaches tasks(1) and tasks(0) after the take, and the all and
 * and arms offer "get <Short>" per object; nothing measured tells those
 * apart from one pass yet.
 */
void
run_takes_second_pass_370 (scr_gameref_t game, const scr_char *string,
                           const scr_char *task_string, size_t task_mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  size_t clobber, mark;
  scr_int noted, printed;

  if (!lib_takes_offers_tasks_370 (game, string))
    return;

  printed = pf_printed_to (filter);
  clobber = std::max (task_mark, printed < 0 ? (size_t) 0 : (size_t) printed);
  mark = pf_buffer_length (filter);
  noted = run_task_runs_noted;

  run_matcher_second_pass = TRUE;
  run_game_commands_in_parser_context (game, task_string, FALSE, TRUE);
  run_matcher_second_pass = FALSE;

  if (run_task_runs_noted != noted)
    pf_erase (filter, clobber, mark);
}

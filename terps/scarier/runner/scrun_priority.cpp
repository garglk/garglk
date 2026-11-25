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
 * The priority pass over the library tables, and the line tests that go with
 * it: put, inventory and repeat-survivor lines, and the unnamed-put rewrite.
 *
 * See scrunner.h for what the `run_` files share.
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
 * run_priority_commands()
 * run_standard_commands()
 *
 * Compare a user input string against commands recognized by the library,
 * and action any command.  Returns TRUE if the string matched a command
 * that then ran successfully, FALSE otherwise.
 *
 * "Priority" commands are the ones the Runner actions ahead of game commands
 * that do not name their verb.  A game with one "ball" object and a task "*
 * ball *" with failing restrictions does not stop "take ball": the take
 * succeeds (probes pPREC and pPREC2; see run_command_match_t::tree_match()).
 * SCARE also noted "I don't understand..." to "drop ball" in that game, a
 * report no probe here has reproduced or ruled out.
 *
 * Scarier models that by having object acquisition take precedence over game
 * commands.
 *
 * The put-in/put-on family runs here TENTATIVELY: probed live against run400
 * (probe FM7 + a TheADRIFTProject .tas transplant), the real Runner lets a put
 * that can actually complete beat a matched task whose restrictions fail ("put
 * pill in cup" answers the library's "You put the small pill inside the coffee
 * cup." while the task's fail message is suppressed), but when the put would
 * be REFUSED (target not a container/surface) the failing task's message wins
 * ("drop pill in slime" prints the fail message, not the refusal).  So in this
 * pass the validity check defers instead of refusing -- run_priority_defer()
 * -- and the scan stops so the plain-drop %text% rows cannot swallow the
 * input; the loud-fail task pass then gets its chance, and the duplicate rows
 * in STANDARD_COMMANDS print the refusal when no task claims it.
 */
#ifdef SCARIER_DUMP_TOOLS
/* SCR_TRACE_ADMIN: the last command dispatched, for the ADMIN trace line. */
std::string run_trace_last_input;
#endif

static scr_bool run_priority_pass_active = FALSE;

scr_bool run_priority_deferred = FALSE;

scr_bool run_priority_refused = FALSE;

static scr_bool run_priority_unnamed_put = FALSE;

scr_bool
run_in_priority_pass (void)
{
  return run_priority_pass_active;
}

/*
 * run_in_put_clause_loop()
 *
 * TRUE while run_game_commands_common() is running the clauses of a 4.0 put
 * list one at a time; see lib_put_clauses_400() and lib_put_implicit_take().
 */
scr_bool run_put_clause_loop_active = FALSE;

scr_bool
run_in_put_clause_loop (void)
{
  return run_put_clause_loop_active;
}

static void
run_priority_defer (void)
{
  assert (run_priority_pass_active);
  run_priority_deferred = TRUE;
}

/*
 * run_priority_defer_if_active()
 *
 * run_priority_defer(), but only inside the tentative priority pass: a
 * handler about to print a refusal leaves the line for a matched task's fail
 * message to claim first (see lib_put_in_is_valid()).  Returns TRUE if it
 * deferred, in which case the caller returns FALSE without printing.
 */
scr_bool
run_priority_defer_if_active (void)
{
  if (!run_priority_pass_active)
    return FALSE;

  run_priority_defer ();
  return TRUE;
}

/*
 * run_priority_refuse()
 *
 * A priority command has printed a refusal that does NOT claim the command:
 * the 4.0 put whose every object was turned away on size or capacity.  The
 * handler returns FALSE after calling this, the table walk stops, and
 * run_line_t::put_pass() lets the task passes answer the same line, joined
 * onto the refusal.  See the 4.0 put notes on run_line_t::put_prepass().
 */
void
run_priority_refuse (void)
{
  assert (run_priority_pass_active);
  run_priority_refused = TRUE;
}

/*
 * run_priority_unnamed_put_object()
 *
 * 4.0's "put X in Y" where X names nothing leaves the command line
 * CLOBBERED, and the task passes never see what was typed.  run400 hands
 * every line holding the whole word "put" or "drop" to put_drop_list
 * (Proc_19_40_459DB4) ahead of the task dispatch; that normalises the line
 * ("drop " -> "put ", "inside"/"into" -> "in", "onto" -> "on"), splits it
 * at " in ", and calls name_object (Proc_19_41_46E5D8) to name the direct
 * object.  name_object sets the global command line to the fragment
 * Left(line, split) -- "put ice cream " -- and resolves that
 * (Proc_21_58_463640 mode 2).  Every other outcome puts the line back; the
 * one that does not is 46E142, reached when the fragment names nothing at
 * all.  There the Runner prints "It is not clear which object you are
 * referring to." ("Drop what?" for a "drop" line), but only when no
 * put/drop-class task pre-matches the typed line (Proc_19_35_453C50 mode 2),
 * and then returns with the fragment still installed.  generaltasks
 * dispatches the tasks against THAT, so a task that would have claimed the
 * typed line never matches, and the catch-all -- whose noun was resolved up
 * front from the original line -- answers instead: "I don't understand what
 * you want to do with the cone."
 *
 * Measured on IceCream.taf in run400 (runner_probes/icecream.run400.put.txt),
 * whose three `put/place/set [the] ice cream in/on [the] cone` tasks share one
 * pattern: `put ice cream in cone` and `put the ice cream in the cone` both
 * come out as the catch-all, while `place ice cream in cone` runs the task
 * (place never enters put_drop_list) and `put ice cream on cone` runs it too
 * -- the "on" branch has an escape of its own at 459C39 that zeroes the split
 * when the fragment resolves to nothing, so the line reaches the tasks intact.
 * The clobber is the "in" form only.
 *
 * The put-in handler signals the fragment with this, and the put refusals
 * read it back through run_priority_put_was_unnamed().  The task passes
 * get the line as typed instead; see run_line_t::get_outer_400().
 */
void
run_priority_unnamed_put_object (void)
{
  assert (run_priority_pass_active);
  run_priority_unnamed_put = TRUE;
}

/* TRUE when this line's put-in went silent at 46E15A; see
 * lib_cmd_put_unclear(). */
scr_bool
run_priority_put_was_unnamed (void)
{
  return run_priority_unnamed_put;
}

scr_bool
run_priority_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  scr_commandsref_t command;

  run_priority_pass_active = TRUE;
  run_priority_deferred = FALSE;
  run_priority_refused = FALSE;
  run_priority_unnamed_put = FALSE;
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (uip_match (command->command, string, game))
        {
          if (run_command_handle (command, game))
            {
              run_priority_pass_active = FALSE;
              return TRUE;
            }
          if (run_priority_deferred || run_priority_refused)
            break;
        }
    }
  run_priority_pass_active = FALSE;

  /* Nothing matched the string.  Or if it did, its handler failed. */
  return FALSE;
}

/*
 * run_is_put_command()
 *
 * TRUE if the string is one of the priority table's NAMED put/drop rows --
 * "put X in Y", "drop X on Y", "drop X", "put X down".  These are the rows
 * run400's put_drop_list answers itself, above the task dispatcher; the
 * all/everything rows are deliberately NOT among them.  The probe binds
 * object references as a side effect, so they are saved and put back, as
 * run_task_reachable_by_library_callback() does for its own speculative
 * matches.
 *
 * Which side of the line a row falls on was measured on p4REPEAT2.taf and
 * p4REPEAT3.taf (runner_probes/repeat[23].run400.txt), where every one of
 * the probe's cells is a task whose command is the typed line, none of them
 * done, all with a RepeatText:
 *
 *   `drop coin`, `drop hat`   library, and the task NEVER ran (no "C2"/"C3")
 *   `put coin on desk`, `put hat on desk`               likewise (no C3/C4)
 *   `drop all`, `put all on desk`     the TASK ran -- "C14"/"C15", and the
 *                                     second time round its RepeatText
 *
 * The named rows are the whole of run400's drop routine Proc_19_7_46FB8C:
 * its named-object arm resolves the noun itself and composes "<player> drop
 * <object>." without ever asking the dispatcher.  The `all` arm does the
 * opposite -- it tests the word "all" and, still before anything is
 * printed, hands the line to a task (`Proc_19_35_453C50("drop all")`) and
 * returns from the routine outright when one claims it (`Result: End Sub`).
 * So an "all" line reaches the tasks first and a named one does not, and the
 * empty-hands "You're not carrying anything." (for the drop half and the put
 * half alike) is only ever reached once the tasks have declined.
 */
scr_bool
run_is_put_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> references (game->refs.objects);
  scr_commandsref_t command;
  scr_bool is_put = FALSE;

  /*
   * Take the table in order and stop at the first row that matches, exactly
   * as run_priority_commands() does: "drop all" matches the named row
   * "[drop/put down] %text%" too, with %text% = "all", and it is only
   * because the all/everything row sits above it that the Runner's `all`
   * arm is the one that runs.  Scanning for the named handlers alone made
   * every "all" line look named.
   */
  for (command = PRIORITY_COMMANDS; command->command; command++)
    {
      if (!uip_match (command->command, string, game))
        continue;
      /* The container-first rows answer only their three exits and pass
       * everything else down to these; look through them. */
      if (command->handler == lib_cmd_put_container_400)
        continue;

      is_put = (command->flags & RUN_ROW_PUT) != 0;
      break;
    }

  game->refs.objects = references;
  return is_put;
}

/*
 * run_is_put_command_400()
 *
 * run_is_put_command(), and at 4.0 a second look by whole-word containment
 * when the container phrase names a fitting container or surface to run400's
 * scorer but the put rows' positional %object% does not -- `put my homework
 * on my desk`, the desk's Prefix being "your school"; see
 * lib_put_container_fits_400().  *CONTAINED says the put rows need that
 * containment again when they run.
 */
scr_bool
run_is_put_command_400 (scr_gameref_t game, const scr_char *string,
                        scr_bool *contained)
{
  scr_bool is_put;

  *contained = FALSE;
  if (run_is_put_command (game, string))
    return TRUE;
  if (!lib_put_container_fits_400 (game, string))
    return FALSE;

  {
    const scr_containment_guard containment (TRUE);

    is_put = run_is_put_command (game, string);
  }
  *contained = is_put;
  return is_put;
}

/*
 * run_is_inventory_command()
 *
 * TRUE for the lines run400's inventory handler answers.  That handler,
 * Proc_19_70_45C304, is called at loc_48A457 -- above the task dispatcher at
 * 48A481 -- so its listing is in the message buffer before any task runs, and
 * a task that then matches the same line has its CompleteText APPENDED to the
 * listing rather than replacing it.  Measured on p4REPEAT2/3
 * (runner_probes/repeat[23].run400.txt): the first `i` answers "You are
 * carrying a coin and a hat.  C1 i.", and `inv` / `inventory` the same way.
 * (The listing is also why a spent task's RepeatText never survives here --
 * the 44CC7D store is gated on the buffer still being empty; that is the
 * survivor table's first row.)
 */
scr_bool
run_is_inventory_command (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  scr_commandsref_t command;
  scr_bool is_inventory = FALSE;

  for (command = PRIORITY_COMMANDS; command->command && !is_inventory;
       command++)
    {
      if (command->handler != lib_cmd_inventory)
        continue;

      is_inventory = uip_match (command->command, string, game);
    }

  return is_inventory;
}

/*
 * run_repeat_survivor_400()
 *
 * TRUE if a 4.0 line is one that a spent task's RepeatText does NOT take
 * away from the standard library.  Measured with p4REPEAT.taf,
 * p4REPEAT2.taf and p4REPEAT3.taf (make_400_repeatprobe.py and
 * make_400_repeatprobe2.py, 30-odd one-cell tasks all done, non-repeatable
 * and carrying a RepeatText, each typed twice) under run400, transcripts
 * runner_probes/repeat*.run400.txt.
 *
 * run400 prints a done non-repeatable task's RepeatText inside the task
 * dispatcher at 48A481 (Proc_19_24_44CCE0) and then jumps to loc_48B4E3,
 * past every general library verb.  Only two groups of handlers escape it:
 *
 *   - the ones the input routine runs BEFORE the dispatcher: inventory
 *     (48A457) and the put/drop list (48A462).  They print first, and the
 *     RepeatText is emitted only while the output is still empty, so
 *     whatever they say silences it -- `i` lists the inventory, `drop coin`
 *     and `put coin on desk` keep "You are not holding the coin.".  The
 *     all/everything forms are in that same list but say nothing with empty
 *     hands, and `drop all` / `put all on desk` DO draw the RepeatText, so
 *     they are matched here first and excluded.
 *   - the per-character pass at 48B56E (Proc_19_0_480674), which sits BELOW
 *     48B4E3 and so still runs, its answer replacing the message: NPC
 *     examine and the ask-format hint that `talk to X`, `speak to X` and
 *     `ask X <anything but "about">` land on.
 *
 * Everything else loses the line to the RepeatText, movement and take
 * included: `north` and `east` print it and the player does not move,
 * `take coin` never takes the coin, `look`, `wait`, `score`, `turns`,
 * `open`, `read`, `wear`, `x <object>`, `x me`, `take all` and `drop all`
 * all print it, and a wildcard task's RepeatText wins as readily as a
 * literal one's (`* dance *`) -- so neither of the pre-4.0 pass's literal
 * and movement exemptions carries over.  `kiss X`, `give X to Y` and `ask X
 * about Y` are general verbs rather than parts of the character pass, and
 * lose the line too; `talk X` without the "to" is the 3.7/3.8 spelling that
 * 4.0's hint does not answer, so it loses it as well.
 */
typedef struct
{
  const scr_char *command;
  scr_bool survives;
} scr_repeat_survivor_t;

static const scr_repeat_survivor_t REPEAT_SURVIVOR_COMMANDS[] = {
  /* Pre-dispatcher, 48A457. */
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[inventory/inv]", TRUE},
#else
  {"[inventory/inv/i]", TRUE},
#endif

  /* Pre-dispatcher, 48A462 -- but only where it has something to say. */
  {"put [all/everything] *", FALSE},
  {"[drop/put down] [all/everything] *", FALSE},
  {"put %text% [in/into/inside {of}] %object%", TRUE},
  {"put %text% [on/onto/on top of] %object%", TRUE},
  {"[drop/put down] %text% [in/into/inside {of}] %object%", TRUE},
  {"[drop/put down] %text% [on/onto/on top of] %object%", TRUE},
  {"[drop/put down] %text%", TRUE},
  {"put %text% down", TRUE},

  /* The character pass at 48B56E, and the two branches that are not it. */
  {"ask %character% about %text%", FALSE},
  {"[talk/speak] to %character% about %text%", FALSE},
#ifdef SCARIER_NO_ABBREVIATIONS
  {"[ex/exam/examine/look {at}] %character%", TRUE},
#else
  {"[x/ex/exam/examine/look {at}] %character%", TRUE},
#endif
  {"[talk/speak] to %character% *", TRUE},
  {"ask %character% *", TRUE},

  {nullptr, FALSE}
};

scr_bool
run_repeat_survivor_400 (scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_ref_entity_guard ref_entity (game);
  std::vector<scr_bool> objects (game->refs.objects);
  std::vector<scr_bool> npcs (game->refs.npcs);
  const scr_repeat_survivor_t *row;
  scr_bool survives = FALSE;

  for (row = REPEAT_SURVIVOR_COMMANDS; row->command; row++)
    {
      if (uip_match (row->command, string, game))
        {
          survives = row->survives;
          break;
        }
    }

  game->refs.objects = objects;
  game->refs.npcs = npcs;
  return survives;
}

/*
 * run_normalise_put_line()
 * run_unnamed_put_fragment()
 *
 * Rebuild the command line run400's put_drop_list leaves behind when the
 * direct object of a "put X in Y" names nothing -- see the 46E142 note on
 * run_priority_unnamed_put_object() above.  put_drop_list normalises the
 * line with four plain VB Replace() calls (substring not word, and "inside"
 * before "into" so that "inside" cannot be reached by the shorter pattern),
 * splits it at the first " in ", and name_object installs Left(line, split)
 * -- everything up to and including the space before "in" -- as the command
 * line.
 *
 * The list branches leave before the clobber, so none of them is rebuilt
 * here: a whole-word "all" or "and" in the fragment is answered by the
 * loops at 46E04E and 46E0B2 (unported), and an " and " at or beyond the
 * split sends put_drop_list round its own loop, whose clauses are carved by
 * lib_put_clauses_400().  "put a in b and put c in d" is not an
 * example of either: the word after its " and " is "put", not an object, so
 * the top-level splitter cuts it into two commands (run_find_split_400()).
 */
std::string
run_normalise_put_line (const scr_char *string)
{
  /*
   * Deliberate deviation: the four rewrites are whole-word, and
   * "drop" only as the line's verb.  run400's substring Replace()s reach
   * into names, so `put coin in drop box` became "put coin in put box" and
   * was "I don't understand what you want to put things inside.", and a
   * "pointo" or "gloveinside" would lose letters the same way.
   */
  static const scr_char *const WORDS[][2] = {
    {"inside", "in"}, {"into", "in"}, {"onto", "on"}
  };
  std::string line (string);
  size_t index_;

  if (line.compare (0, 5, "drop ") == 0)
    line.replace (0, 4, "put");
  for (index_ = 0; index_ < sizeof WORDS / sizeof WORDS[0]; index_++)
    {
      const std::string from (WORDS[index_][0]);
      size_t at = 0;

      while ((at = line.find (from, at)) != std::string::npos)
        {
          const size_t end = at + from.size ();

          if ((at == 0 || line[at - 1] == ' ')
              && (end == line.size () || line[end] == ' '))
            {
              line.replace (at, from.size (), WORDS[index_][1]);
              at += strlen (WORDS[index_][1]);
            }
          else
            at = end;
        }
    }
  if (SCR_TRACING_DEVIATIONS ())
    {
      static const scr_char *const RUNNER[][2] = {
        {"drop", "put"}, {"inside", "in"}, {"into", "in"}, {"onto", "on"}
      };
      std::string runner (string);

      for (index_ = 0; index_ < sizeof RUNNER / sizeof RUNNER[0]; index_++)
        for (size_t at = 0;
             (at = runner.find (RUNNER[index_][0], at)) != std::string::npos;
             at += strlen (RUNNER[index_][1]))
          runner.replace (at, strlen (RUNNER[index_][0]), RUNNER[index_][1]);
      if (runner != line)
        SCR_DEVIATION ("put_whole_word", "runner=\"%s\" ours=\"%s\"",
                       runner.c_str (), line.c_str ());
    }
  return line;
}

scr_bool
run_unnamed_put_fragment (const scr_char *string, std::string &fragment)
{
  std::string line = run_normalise_put_line (string);
  std::string::size_type split;

  split = line.find (" in ");
  if (split == std::string::npos)
    return FALSE;

  fragment = line.substr (0, split + 1);
  if ((" " + fragment).find (" all ") != std::string::npos
      || (" " + fragment).find (" and ") != std::string::npos
      || line.find (" and ", split) != std::string::npos)
    return FALSE;

  return TRUE;
}

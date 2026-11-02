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
 * Player input: run_player_input() and the line's road from the prompt to
 * run_all_commands() -- reading, separator cuts, repeat, the spelling
 * filter, questions, the 4.0 co answer, not-understood and the undo note.
 *
 * Split out of scrun_dispatch.cpp; see scrunner.h for what the runner files
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
 * run_separator_task_line()
 *
 * Deliberate deviation (2026-09-30).  TRUE when a task command that itself
 * holds one of the line separators the line holds (" and ", " then ", ",",
 * ". ") matches the whole line; the line is then not cut.  run400 cuts at
 * those words first (7d38fbf08), and from 3.9 the Runners cut at " then "
 * and ". ", so an author's `jump up and down`, `rock and roll` or `search
 * then pray` never reached its task: 4.0 answered `jump up and down` with
 * the jump and then a movement refusal.  A command without the separator
 * (a catch-all `*`) does not count, so `get coin then n` still splits.
 */
static scr_bool
run_ci_contains (const scr_char *haystack, const scr_char *needle)
{
  const size_t length = strlen (needle);

  for (; *haystack != NUL; haystack++)
    if (scr_strncasecmp (haystack, needle, length) == 0)
      return TRUE;
  return FALSE;
}

static std::string run_separator_trace;

scr_bool
run_separator_task_line (scr_gameref_t game, const scr_char *line)
{
  static const scr_char *const SEPARATORS[] = {
    " and ", " then ", ",", ". ", nullptr
  };
  const scr_task_commands_guard task_commands;
  const scr_int task_count = gs_task_count (game);
  scr_int task;

  if (!run_ci_contains (line, " and ") && !run_ci_contains (line, " then ")
      && !strchr (line, ',') && !strstr (line, ". "))
    return FALSE;

  for (task = 0; task < task_count; task++)
    {
      for (const scr_bool forwards : {scr_bool (TRUE), scr_bool (FALSE)})
        {
          for (const scr_char *pattern :
               run_task_command_patterns (game, task, forwards))
            {
              const scr_char *const *separator;
              scr_bool shared = FALSE;

              if (pattern[strspn (pattern, WHITESPACE)] == SPECIAL_PATTERN)
                continue;
              for (separator = SEPARATORS; *separator && !shared; separator++)
                shared = run_ci_contains (pattern, *separator)
                         && run_ci_contains (line, *separator);
              if (!shared)
                continue;
              if (scr_strcasecmp (pattern, line) == 0
                  || uip_match (pattern, line, game))
                {
                  if (SCR_TRACING_DEVIATIONS ())
                    run_separator_trace = "line=\"" + std::string (line)
                                          + "\" task=" + std::to_string (task)
                                          + " cmd=\"" + pattern + "\"";
                  return TRUE;
                }
            }
        }
    }
  return FALSE;
}

/*
 * run_comma_splits_pre390()
 *
 * Deliberate deviation: 3.7/3.8 split a line at a comma, as SCARE did and
 * 3.9 does, where run370/run380 never do (run_find_split_pre400()).  The
 * Runner rule stands wherever a task command matches the whole line, so a
 * game that wrote a comma into a command keeps it: arlo's `kill, kill, kill`
 * and `hello, customer`, tra's `mirror mirror on the wall, who's the fairest
 * of them all`, wrecked's `out (Redstown, no ticket)`, and any `say *` that
 * takes a comma in what is said.  The test is the plain matcher on the
 * unfiltered line, ahead of the synonym and pronoun rewrites.
 */
scr_bool
run_comma_splits_pre390 (scr_gameref_t game, const scr_char *line)
{
  const scr_task_commands_guard task_commands;
  const scr_int task_count = gs_task_count (game);
  scr_int task;

  if (!strchr (line, ','))
    return FALSE;

  for (task = 0; task < task_count; task++)
    {
      for (const scr_bool forwards : {scr_bool (TRUE), scr_bool (FALSE)})
        {
          for (const scr_char *pattern :
               run_task_command_patterns (game, task, forwards))
            {
              if (pattern[strspn (pattern, WHITESPACE)] == SPECIAL_PATTERN)
                continue;
              if (scr_strcasecmp (pattern, line) == 0
                  || uip_match (pattern, line, game))
                return FALSE;
            }
        }
    }
  return TRUE;
}


/*
 * The typed line still to run, and the element of it being run now.
 * run_player_input() owns both; run_input_reset() clears them.
 */
static scr_char run_line_buffer[LINE_BUFFER_SIZE];
scr_char run_line_element[LINE_BUFFER_SIZE];

/*
 * run_is_repeat_word()
 *
 * The words that repeat the last element; see run_player_input().
 */
static scr_bool
run_is_repeat_word (scr_gameref_t game, const scr_char *element)
{
  std::string word (element);
  const size_t first = word.find_first_not_of (' ');

  if (first == std::string::npos)
    return FALSE;
  word = word.substr (first, word.find_last_not_of (' ') - first + 1);

  if (word == "again" || word == "last" || word == "previous")
    return TRUE;
#ifndef SCARIER_NO_ABBREVIATIONS
  if (word == "g")
    return run_get_version (gs_get_bundle (game)) >= TAF_VERSION_390;
#endif
  return FALSE;
}

/*
 * run_input_reset()
 *
 * Resets all noted line input to initial conditions, for a game that is
 * not running.
 */
static void
run_input_reset (void)
{
  memset (run_line_buffer, NUL, sizeof (run_line_buffer));
  memset (run_prior_element, NUL, sizeof (run_prior_element));
  memset (run_line_element, NUL, sizeof (run_line_element));
  run_typed_line.clear ();
  run_previous_typed_line.clear ();
  lib_co_400_reset ();
  lib_battle_who_reset ();
  lib_with_prefix_390_reset ();
  lib_put_reset ();
  run_cancel_goto_walk ();
}

/*
 * run_repeat_element()
 *
 * `again`: makes the last element the current one.  Returns FALSE, with
 * the complaint printed, when there is nothing to repeat.
 */
static scr_bool
run_repeat_element (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  game->do_again = FALSE;

  /* Check there is a last element to repeat. */
  if (run_prior_element[0] == NUL)
    {
      pf_buffer_string (gs_get_filter (game),
                        "You can hardly repeat that.\n");
      return FALSE;
    }

  /*
   * 4.0 with "References in brackets" ticked (the setting Scarier
   * models) echoes the command it is about to repeat, in round brackets
   * on its own line: run400 generaltasks loc_48A058 walks the history
   * past the "again"s, then at loc_48A095 tests MemVar_4942BA and prints
   * "(" & command & ")" & vbCrLf through Proc_21_19_47B568.  run390
   * echoes it the same way: p39WITH `probe`, `again` -> "(probe)" then
   * "PROBE OK." (runner_probes/with.run390.txt, 2026-09-14).  3.8 and
   * earlier are not measured.
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    pf_buffer_reference (gs_get_filter (game), run_prior_element,
                         gs_get_vars (game), bundle);

  /* Make the last element the current input element. */
  strncpy (run_line_element, run_prior_element, LINE_BUFFER_SIZE);
  return TRUE;
}

/*
 * run_read_line()
 *
 * Reads a new line of player input if none is buffered -- or the rest of
 * the line a pre-4.0 walk held back -- and otherwise separates output so
 * far with a newline.  Returns TRUE when a line was read.
 */
static scr_bool
run_read_line (scr_gameref_t game)
{
  scr_bool is_new_line = FALSE;

  /*
   * A pre-4.0 walk is over: the rest of the line it held back runs now,
   * in the same turn as the arrival.  See run_goto_rest.
   */
  if (run_line_buffer[0] == NUL && !run_goto_rest.empty ()
      && run_goto_steps.empty () && !run_goto_arrival_due)
    {
      strncpy (run_line_buffer, run_goto_rest.c_str (), LINE_BUFFER_SIZE - 1);
      run_line_buffer[LINE_BUFFER_SIZE - 1] = NUL;
      run_goto_rest.clear ();
    }

  if (run_line_buffer[0] == NUL)
    {
      if_read_line (run_line_buffer, sizeof (run_line_buffer));
      is_new_line = TRUE;

      /* run400 48A2EA: a new typed line, no event ticked yet. */
      evt_clear_ticked_events (game);

      /*
       * Every Runner lower-cases the whole typed line before it parses
       * anything: run400 Form1 loc_45C5D1..45C5E5 echoes `"> " & cmd`
       * first and only then assigns `cmd = LCase(cmd)`, pushing the
       * lower-cased copy into the command history array as well
       * (run390 loc_436235..436249, run380 loc_426FA9, run370
       * loc_422091 -- unconditional in all four, no version gate).
       * The echo is unaffected because it happens above the LCase, and
       * Glk echoes the input line for us here.
       *
       * This matters because the game's own SYNONYM rewrites run AFTER
       * it, so an author's replacement text is the only thing that can
       * put an upper-case letter back into a command -- which is what
       * makes a character unreferenceable in uip_case_folds_name().
       */
      for (scr_char *cursor = run_line_buffer; *cursor != NUL; cursor++)
        *cursor = scr_tolower (*cursor);
      run_previous_typed_line = run_typed_line;
      run_typed_line = run_line_buffer;
    }
  else
    if_print_character ('\n');
  return is_new_line;
}

/*
 * run_cut_element()
 *
 * Cuts the next element off the front of the line buffer, and makes it
 * the current input element.
 */
static void
run_cut_element (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int length, extent;

  /*
   * Find the length of the next input line element.  At 4.0, unless the
   * line buffer is empty, we always take the first character, even if
   * it's a separator.  This catches odd input like "." and turns it into
   * a parser complaint, rather than treating it as two empty commands
   * with a separator between them.
   */
  scr_int sep_length = 1;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    {
      /*
       * 4.0 cuts the line at the first separator, in pass order, whose
       * tail does not begin with an object name; see
       * run_find_split_400().  The separator goes, and any whitespace
       * after it: that prevents "i. ." looking like "i" and ""; it
       * instead looks like "i" and ".", and results in a parser
       * complaint.
       */
      const scr_int split = (run_line_buffer[0] == NUL
                             || run_separator_task_line (game,
                                                         run_line_buffer))
                            ? -1
                            : run_find_split_400 (game, run_line_buffer,
                                                  &sep_length);

      if (SCR_TRACING_DEVIATIONS () && split < 0 && run_line_buffer[0] != NUL
          && run_separator_task_line (game, run_line_buffer))
        {
          scr_int runner_length = 1;

          if (run_find_split_400 (game, run_line_buffer, &runner_length) >= 0)
            SCR_DEVIATION ("separator_unsplit", "%s",
                           run_separator_trace.c_str ());
        }
      length = (split < 0) ? (scr_int) strlen (run_line_buffer) : split;
      extent = length;
      extent += (run_line_buffer[length] == NUL) ? 0 : sep_length;
      extent += strspn (run_line_buffer + extent, WHITESPACE);
    }
  else
    {
      /*
       * Pre-4.0 keeps its tail as the Runner does, less one leading
       * space, and the head may be empty (`then look`); see
       * run_find_split_pre400().
       */
      const scr_int version = prop_get_taf_version (bundle);

      length = run_separator_task_line (game, run_line_buffer)
               ? -1
               : run_find_split_pre400 (version, run_line_buffer, &extent,
                                        version < TAF_VERSION_390
                                        && run_comma_splits_pre390
                                             (game, run_line_buffer));
      if (SCR_TRACING_DEVIATIONS () && length < 0
          && run_separator_task_line (game, run_line_buffer))
        {
          scr_int runner_extent = 0;

          if (run_find_split_pre400 (version, run_line_buffer, &runner_extent,
                                     version < TAF_VERSION_390
                                     && run_comma_splits_pre390
                                          (game, run_line_buffer)) >= 0)
            SCR_DEVIATION ("separator_unsplit", "%s",
                           run_separator_trace.c_str ());
        }
      if (length < 0)
        length = extent = (scr_int) strlen (run_line_buffer);
      else if (length == 0 && version == TAF_VERSION_390
               && strncmp (run_line_buffer, "then", 4) == 0)
        {
          const std::string queue = run_empty_then_head_390 (run_line_buffer);

          strncpy (run_line_buffer, queue.c_str (), LINE_BUFFER_SIZE - 1);
          run_line_buffer[LINE_BUFFER_SIZE - 1] = NUL;
          length = extent = (scr_int) strlen (run_line_buffer);
        }
    }

  /*
   * Make this the current input element, and remove it and the
   * separator from the front of the line buffer.
   */
  memcpy (run_line_element, run_line_buffer, length);
  run_line_element[length] = NUL;
  memmove (run_line_buffer,
           run_line_buffer + extent, strlen (run_line_buffer) - extent + 1);

  /*
   * The Runner strips one trailing space from the head it keeps (the
   * splitter's loop at 4596E1), so `x coin , x hat` leaves "x coin",
   * not "x coin ".
   */
  if (length > 0 && run_line_element[length - 1] == ' ')
    run_line_element[length - 1] = NUL;
}

/*
 * run_count_element()
 *
 * 3.9 counts line elements, not turns: generaltasks adds one to its turn
 * counter MemVar_4681A4 at its very top (45EC5B), as run380 does to
 * MemVar_44F138 (441A21), before the not-a-turn flag is even cleared, so
 * `turns`, a DontUnderstand line and a blank line all count.  Every jump
 * back for the next queued element (4609F9) lands above the increment
 * too.  `again` does not: run390 answers the `turns` that follows 18
 * elements with 19 twice over (runner_probes/admin.run390.txt).  The
 * end-of-turn tail in run_main_loop() leaves the counter alone at 3.9.
 * The increment sits after the line is read, so an autosave taken at the
 * prompt and restored there never counts a line twice.
 */
static void
run_count_element (scr_gameref_t game)
{
  if (run_counts_line_elements (game))
    {
      game->turns++;

      /*
       * `both` is the one in-line re-run that really counts twice: 45FEB6
       * swaps the line for the menu stash MemVar_46813C.global_0 and jumps
       * back to 45EC4B, above the increment.  With no menu pending the stash
       * is empty and the re-run is DontUnderstand, which Scarier already
       * prints for the word; p39WITH `turns`, `both`, `turns` answers 24
       * then 27 (runner_probes/with.run390.txt).  The question prefix does
       * not: the jump at 4601A5 never fires for a "With what?" or "Wear
       * what?" answer (`knife`, `coin` each count once in the same drive).
       */
      if (scr_strcasecmp (run_line_element, "both") == 0
          && prop_get_taf_version (gs_get_bundle (game)) == TAF_VERSION_390)
        game->turns++;
    }
}

/*
 * run_element_filtered()
 *
 * The element with the game's synonyms and the built-in rewrites applied,
 * or NULL if nothing changed.  Deliberate deviations (2026-09-30), each for
 * the lines where the Runner's own rewrite leaves the author's task out of
 * reach:
 *
 *  - The game's synonyms are applied the Runner's way (pf_apply_synonym())
 *    and Scarier's older way (pf_apply_synonyms_by_word()).  The Runner's
 *    loops replace substrings inside words, skip a synonym whose first hit
 *    sits inside another word or whose Original has a capital, and let a
 *    later synonym rewrite an earlier one's text, so Vardock Bates' `hablar
 *    con jason` became "talk con jason jason dhirco" and no task took it.
 *  - run380 rewrites a typed "take" to "get" before any task sees the line,
 *    so a 3.80 task whose commands say only `take X` can never fire:
 *    great.taf's `take picasso` just picks the painting up.
 *  - Every Runner rewrites everything->all and slap->hit, and 3.8+ the
 *    except/but/apart from words, before the tasks (see pf_filter_input()),
 *    so a task written `slap *` or `confess everything to priest` never
 *    fires.  The line without those rewrites is tried last.
 *
 * The Runner's spelling runs whenever it reaches a task, so Dolg's `войти в
 * дом` and a `take X` that reaches a `get X` task are unchanged.  Otherwise
 * the first other spelling that reaches a task runs, and a line no spelling
 * takes gets the whole-word synonyms and the Runner's take->get.
 */
scr_char *
run_element_filtered (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  scr_owned_string runner (pf_filter_input (run_line_element, bundle));
  std::vector<scr_owned_string> others;
  size_t index_;

  /*
   * Deliberate deviation (2026-09-30): a typed `l` or `look` that the
   * game's synonyms turn into a bare `x`, `ex` or `examine` stays a look.
   * Lair of the Vampire rewrites `look` to `x`, and run400 answers the bare
   * examine word with "Try something different.", so the room was never
   * listed and the objects in it stayed unseen
   * (runner_probes/lair.run400.[abc].txt).
   */
  if (runner)
    {
      std::vector<scr_char> typed (run_line_element,
                                   run_line_element
                                   + strlen (run_line_element) + 1);
      std::vector<scr_char> rewritten (runner.get (),
                                       runner.get ()
                                       + strlen (runner.get ()) + 1);
      const scr_char *const typed_line = scr_normalize_string (typed.data ());
      const scr_char *const new_line = scr_normalize_string (rewritten.data ());

      if ((scr_strcasecmp (typed_line, "l") == 0
           || scr_strcasecmp (typed_line, "look") == 0)
          && (scr_strcasecmp (new_line, "x") == 0
              || scr_strcasecmp (new_line, "ex") == 0
              || scr_strcasecmp (new_line, "examine") == 0))
        {
          SCR_DEVIATION ("look_kept", "runner=%s", new_line);
          return nullptr;
        }
    }

  others.emplace_back (pf_filter_input (run_line_element, bundle,
                                        TRUE, FALSE));
  if (version == TAF_VERSION_380)
    {
      others.emplace_back (pf_filter_input (run_line_element, bundle,
                                            FALSE, TRUE));
      others.emplace_back (pf_filter_input (run_line_element, bundle,
                                            FALSE, FALSE));
    }
  others.emplace_back (pf_filter_input (run_line_element, bundle,
                                        TRUE, TRUE, FALSE));
  others.emplace_back (pf_filter_input (run_line_element, bundle,
                                        TRUE, FALSE, FALSE));

  /* The spelling the matcher sees: trimmed, whitespace runs collapsed. */
  auto spelling = [] (const scr_owned_string &text) -> std::string
    {
      std::vector<scr_char> copy;
      const scr_char *source = text ? text.get () : run_line_element;

      copy.assign (source, source + strlen (source) + 1);
      return scr_normalize_string (copy.data ());
    };
  const std::string runner_spelling = spelling (runner);

  for (index_ = 0; index_ < others.size (); index_++)
    {
      if (spelling (others[index_]) != runner_spelling)
        break;
    }
  if (index_ == others.size ())
    return runner.release ();

  if (run_line_matches_task_strictly (game, runner_spelling.c_str ()))
    return runner.release ();
  for (index_ = 0; index_ < others.size (); index_++)
    {
      const std::string other = spelling (others[index_]);

      if (other != runner_spelling
          && run_line_matches_task_strictly (game, other.c_str ()))
        {
          SCR_DEVIATION ("spelling_task",
                         "variant=%zu runner=\"%s\" ran=\"%s\"", index_,
                         runner_spelling.c_str (), other.c_str ());
          return others[index_].release ();
        }
    }

  /* No spelling reaches a task: the whole-word synonyms, with take->get. */
  if (scr_strcasecmp (spelling (others[0]).c_str (),
                      runner_spelling.c_str ()) != 0)
    SCR_DEVIATION ("spelling_library", "runner=\"%s\" ran=\"%s\"",
                   runner_spelling.c_str (), spelling (others[0]).c_str ());
  return others[0].release ();
}

/*
 * run_element_command()
 *
 * The command the element runs as: the element filtered for synonyms and
 * pronouns, and glued onto the previous line when it begins "with ".
 */
static void
run_element_command (scr_gameref_t game, std::string &command)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Filter the input element for synonyms, then for pronouns.  Both are
   * scr_malloc'd, so own them with RAII; .get() feeds the raw char* to the
   * pointer-aliasing logic that decides which buffer "wins", and COMMAND
   * takes a copy of the winner.
   */
  scr_owned_string filtered (run_element_filtered (game));
  scr_owned_string replaced (uip_replace_pronouns (game,
      filtered ? filtered.get () : run_line_element));

  /*
   * If filtering didn't replace synonyms, and no pronouns were replaced, use
   * the original line element.  The "(to Nobody)" / "(GARGOYLE)" reference
   * rewrites are NOT applied here: the Runner applies them only once its
   * typed-command task dispatcher has declined the line -- see
   * run_line_t::library().
   */
  command = replaced ? scr_normalize_string (replaced.get ())
            : (filtered ? scr_normalize_string (filtered.get ())
                        : run_line_element);

  /*
   * 3.8 on, an element beginning "with " is glued onto the line typed before
   * it: `If Left(line, 5) = "with " Then line = history(2) & " " & line`,
   * after the synonyms, pronouns and the everything/slap/take/except
   * rewrites (run380 441C9D, run390 45F2AF, run400 48A399; run370 has none).
   * So after `look`, `with stone` runs as "look with stone" -- the stone's
   * description at 3.9 (run390x runner_probes/npcamb.run390.with.txt) and
   * "Nothing special." at 3.8 (run380x
   * runner_probes/npcamb.run380.with.rtf).
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_380
      && strncmp (command.c_str (), "with ", 5) == 0)
    command = run_previous_typed_line + " " + command;
}

/*
 * run_element_begin()
 *
 * The per-element stores made before the command is run: the open
 * questions are noted, and the referenced object and character forgotten.
 */
static void
run_element_begin (scr_gameref_t game, const scr_char *command,
                   scr_bool is_new_line)
{
  /*
   * Upstream SCARE echoed the rewritten command in italic square brackets,
   * for synonyms and for pronouns alike.  No Runner does that: run370 and
   * run380 have no "[" string literal at all, run390's only one is the
   * "[More]" pager, and 4.0's synonym substitution is silent too.  The echo
   * also exposed authoring the player is not meant to see -- "The Warlord,
   * The Princess & The Bulldog" routes "i"/"inv"/"inventory" through a
   * synonym to the keyword its inventory task listens for, so every "i"
   * answered with a bare "[iii]".
   *
   * What 4.0 does print, with "References in brackets" ticked, is the
   * pronoun's antecedent in round brackets on its own line -- "(a trophy)";
   * uip_replace_pronouns() buffers that as it substitutes.
   */

  /*
   * Note whether a 4.0 ambiguity prompt left a question open.  The question
   * a typed LINE began with is spent by its first element; one raised by an
   * earlier element of the same line is not, and the element that meets it
   * answers "That wasn't one of the options!".  See lib_co_400_raise() in
   * sclibrar_disambig_400.cpp.
   */
  lib_co_400_begin_line (is_new_line);
  lib_antecedent_begin_line_400 (game, command);
  lib_battle_who_begin_element (is_new_line);
  lib_with_prefix_390_begin_element ();

  /*
   * Every Runner forgets the referenced object and character at the top of
   * every command: run400 generaltasks stores &HFF in MemVar_494208 and
   * MemVar_49420A at 48A004/48A009, run390 in MemVar_4681A8/4681AA at
   * 45EC66/45EC6B.  Only an %object% / %character% bind or a library handler
   * sets them again, so a state restriction on "the referenced object" in a
   * task whose command names no %object% fails outright (run400 480F9E,
   * run390 44ABF0) -- it never sees whatever the previous command referenced.
   * Professor's task 7 (`take mailbox`, square) is the case: run400 never
   * runs it (runner_probes/professor.run400.profmail.txt,
   * runner_probes/professor.run400.mail2.txt,
   * runner_probes/professor.run400.profmail3.txt), Scarier used to pass it on
   * the mailbox left over from `examine mailbox`.
   *
   * 3.7 and 3.8 forget too, which p*OBJREF's task 2 shows from the printing
   * side: `zork`, whose command binds nothing, prints its text's %object%
   * and %character% raw on the turn right after `nurb rock` bound the red
   * rock, in all four Runners (runner_probes/objref.run370.rtf,
   * runner_probes/objref.run380.rtf, runner_probes/objref.run390.txt,
   * runner_probes/objref.run400.txt, 2026-09-20).  Below 3.90 the reference
   * looks to be local to checktask, which is the same thing seen from the
   * other end.  We kept the older Runners' references across the line and
   * printed "ZORKED a red rock."
   */
  var_set_ref_object (gs_get_vars (game), -1);
  var_set_ref_character (gs_get_vars (game), -1);
}

/*
 * run_element_questions()
 *
 * An open "Who do you want to attack?" continues a line nothing answered:
 * the battle prefix goes in front and the line runs again, its own output
 * discarded.  See lib_battle_who_continuation() in sclibrar_battle.cpp.
 * The "With what?" question rule and 3.9's own with-prefix continuation
 * follow.  Returns the element's final status.
 */
static scr_bool
run_element_questions (scr_gameref_t game, const scr_char *command,
                       scr_bool status)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::string rerun (lib_battle_who_continuation (command, status));

  if (!rerun.empty ())
    {
      const scr_bool is_400 = prop_get_taf_version (bundle)
                              >= TAF_VERSION_400;
      std::string collapsed (rerun);
      size_t pair;

      /*
       * 4.0 runs the joined line with its spaces collapsed and past the
       * task matcher; 3.9 runs it as joined, and its task matcher sees
       * every space: p39WITHQ's `saw rope` / `knife` fires the task wired
       * `saw rope with  knife` and not its one-space twin (run390x
       * runner_probes/withq.run390.txt, 2026-09-25).
       */
      if (is_400)
        while ((pair = collapsed.find ("  ")) != std::string::npos)
          collapsed.erase (pair, 1);

      pf_empty (gs_get_filter (game));
      game->is_admin = FALSE;
      run_rerun_skips_tasks = is_400 && collapsed != rerun;
      run_rerun_exact_spaces = !is_400
          && rerun.find ("  ") != std::string::npos;
      status = run_all_commands (game, collapsed.c_str ());
      run_rerun_skips_tasks = FALSE;
      run_rerun_exact_spaces = FALSE;

      /*
       * 3.9's prefix rerun (4601A5-4601C4) is a GoTo 45EC4B, above the
       * element counter at 45EC5B, so the joined line counts as one more
       * element: on p39WITHQ every continued `knife` and `sword` moves
       * `turns` by two, the `knife` after "Whittle it with what?" -- no
       * prefix, the object catch-all -- by one.  See run_count_element().
       */
      if (prop_get_taf_version (bundle) == TAF_VERSION_390)
        game->turns++;
    }

  /*
   * A turn that ends asking "With what?" or "...with?" leaves the line
   * plus " with " as the question prefix (3.9 and 4.0), and at 4.0 is not
   * a turn -- task text included.  See lib_question_with_rule() in
   * sclibrar_battle.cpp.
   */
  if (status
      && lib_question_with_rule (game, rerun.empty () ? command
                                                      : rerun.c_str ()))
    game->is_admin = TRUE;

  /*
   * 3.9: an open "With what?" continues a line nothing understood, as
   * `<prefix><line>` and through therest only -- the task matcher never
   * sees the joined line.  See lib_with_prefix_390_continuation() in
   * sclibrar_battle.cpp.
   */
  if (rerun.empty ())
    {
      const std::string joined (lib_with_prefix_390_continuation (command,
                                                                  status));

      if (!joined.empty ())
        {
          pf_empty (gs_get_filter (game));
          run_rerun_skips_tasks = TRUE;
          status = run_all_commands (game, joined.c_str ());
          run_rerun_skips_tasks = FALSE;
        }
    }
  lib_with_prefix_390_end_element ();
  return status;
}

/*
 * run_element_co_answer_400()
 *
 * 4.0: with an ambiguity question open, a line that did nothing is an
 * answer to it rather than a line the game misunderstood.  "Did nothing"
 * is either of the two refusals that end a turn empty-handed -- the
 * DontUnderstand path below, and the unhandled-verb catch-all in
 * lib_cmd_verb_object() -- and the turn's own output goes with it, the way
 * the 3.8 prompt replaces a turn wholesale (pf_empty() in
 * lib_co_ambiguity_prompt()).  A line that DID something runs normally and
 * simply spends the question: run400 answers `x tree rock` / `x rock` with
 * "A plain thing." and not with the refusal
 * (runner_probes/co.run400.t930.txt).
 *
 * Answering does not score the answer against the prompt's candidates at
 * all: generaltasks splices the typed words into the stored command in
 * front of its term and re-runs the whole line (48B097-48B15B for an
 * object, 48B15E-48B197 when the command does not hold the term), which
 * is how `chop tree` / `chop keys` ends in a SECOND full prompt naming
 * the keys as well as the trees -- the rebuilt `chop chop keys tree`
 * names them.  "That is still ambiguous!" is then the re-run's own raise
 * meeting the list the last prompt left behind, not an answer this slot
 * gives.  Measured on p4CO.taf, runner_probes/co.run400.t927.txt,
 * runner_probes/co.run400.t929.txt and runner_probes/co.run400.co11.txt
 * -- see lib_co_400_object_answer_line().
 *
 * 48B15B jumps to 489FEB, the top of the ELEMENT loop, which clears the
 * element's reply and the question but NOT what the last prompt offered
 * (48BB53's 4941F4, cleared only at the end of an element that flagged
 * nothing).  lib_co_400_take_question() is that top: it drops the
 * question the re-run must not answer a second time, and leaves that
 * list standing so that the re-run's own raise can meet it.
 *
 * Returns TRUE when the line was the answer, with STATUS the re-run's.
 */
static scr_bool
run_element_co_answer_400 (scr_gameref_t game, const scr_char *command,
                           scr_bool &status)
{
  if (!lib_co_400_question_pending () || scr_strempty (command)
      || (status && !lib_co_400_line_refused ()))
    return FALSE;

  const std::string rerun (lib_co_400_pending_is_npc ()
                           ? lib_co_400_npc_answer_line (command)
                           : lib_co_400_object_answer_line (command));

  lib_co_400_take_question ();
  pf_empty (gs_get_filter (game));

  /*
   * 489FEB is above the stores that mark a line administrative, so the
   * re-run starts a turn of its own and the answer is counted by what
   * the rebuilt line does -- not by the prompt that asked for it.
   * p4WTIE, run400 runner_probes/wtie.run400.w8.txt turn 10: `cut rope
   * with stone` is the question and no turn, and `red stone` runs `cut
   * rope with red stone`, answers "You don't have the red stone." and
   * ticks.  A re-run that asks again marks itself
   * (lib_co_400_raise_common()), and so does the still-ambiguous arm
   * below, so only a line that did something reaches the clock.
   */
  game->is_admin = FALSE;

  status = run_all_commands (game, rerun.c_str ());
  if (!status)
    {
      pf_empty (gs_get_filter (game));
      lib_co_400_print_still_ambiguous (game);
      status = TRUE;
    }
  return TRUE;
}

/*
 * run_element_not_understood()
 *
 * Nothing answered the element: the game's DontUnderstand text, or
 * nothing at all after a 4.0 ending.
 */
static void
run_element_not_understood (scr_gameref_t game, const scr_char *command)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *message;

  /*
   * An EMPTY line element complains too.  Upstream SCARE guarded this
   * whole block with `if (!scr_strempty (command))`, so a bare Return
   * printed nothing.  Both Runners answer one with DontUnderstand:
   * the stardust and xfiles feeds were the only CRLF feeds in the Wine
   * Runner harness, so every command in those two runs went in followed
   * by an extra empty Return, and run390 answered all 115 of them with
   * S_Tar_Dus's ALR for the message ("I are confused.  DURHH!",
   * runner_probes/stardust.run390.txt) and run400 all 22 of xfiles'
   * ("Nope!", runner_probes/xfiles.run400.b.txt).  No walk or event
   * line follows one, so the turn does not tick either -- which the
   * FALSE return below already gives us, run_main_loop() ticking only
   * on TRUE.  Only a genuinely empty input line gets here: the splitter
   * above takes the first character even when it is a separator, so "."
   * and "i. ." were complaints before this and still are.
   */

  /*
   * 4.0: a line that names a character and was ended by a task says
   * nothing at all.  run400's tail tests two conditions together --
   * `48B573: If MemVar_4941B0 = "" And var_29C = 0 Then
   * MemVar_4941B0 = MemVar_4941A8` -- where var_29C is set by the walk
   * over the characters at 48B53C-48B569 (uip_line_names_npc() here) and
   * MemVar_4941A8 is the game's DontUnderstand text.  Outside an ending
   * the second condition never shows: a line naming a present character
   * that nothing else answered gets the catch-all from characters()
   * instead, so the buffer is not empty.  Once a task has ended the game
   * that catch-all is suppressed (4805CD, see lib_cmd_verb_npc()), and
   * the empty buffer meets var_29C = 1 and prints nothing.
   *
   * easter.taf's winning `show basket to shopkeeper` is the measured
   * case: run400 runner_probes/easter.run400.txt:304-308 goes straight
   * from the task's text to the WinText.  Gated on the ending so the
   * general shape of the test cannot disturb an ordinary line.
   */
  if (game->pending_endgame != 0
      && run_get_version (bundle) == TAF_VERSION_400
      && uip_line_names_npc (game, run_line_element))
    {
      run_line_buffer[0] = NUL;
      return;
    }

  /*
   * Command line element not understood.  Own the escaped copy:
   * var_set_ref_text() can throw (scr_fatal_error).
   */
  scr_owned_string escaped
      (pf_escape (scr_normalize_string (run_line_element)));
  var_set_ref_text (gs_get_vars (game), escaped.get ());
  message = prop_get_global_string (bundle, "DontUnderstand");
  pf_buffer_string (filter, message);
  pf_buffer_character (filter, '\n');

  /*
   * A line element that's not understood leaves the rest of the line
   * alone.  Upstream SCARE threw the remaining elements out here; no
   * Runner does.  run400 re-reads its queue at the very END of
   * generaltasks (48BCF2, `If MemVar_4942E4 <> "" Then MemVar_494174 =
   * MemVar_4942E4 : GoTo 489FEB`), below every exit the DontUnderstand
   * text can take: `wave zzz and yyy` on the p4AND probe answers NO IDEA
   * twice (runner_probes/and.run400.txt).  run390 answers `zzz, look`
   * with NO IDEA and the room (runner_probes/ask.run390.split2.txt),
   * run380 `zzz then look` with "I don't understand." and the room
   * (runner_probes/ask.run380.split2.rtf).
   */

  /*
   * 4.0: and what the setter was handed on the way down still stands --
   * openclose's loop runs above therest's DontUnderstand, so `cut rope
   * with gems` answers NO IDEA and leaves "it" at the emerald (p4WTIE,
   * runner_probes/wtie3.run400.it2.txt).  See uip_note_antecedent_400()
   * in scparser.cpp.
   */
  if (run_get_version (bundle) >= TAF_VERSION_400)
    {
      uip_commit_antecedent_400 (game);
      uip_set_pronoun_flags (FALSE, FALSE);
    }
  /* 3.9: generaltasks' co() pre-pass ran before anything answered. */
  else if (run_get_version (bundle) == TAF_VERSION_390)
    uip_assign_pronouns (game, command);
}

/*
 * run_element_note_undo()
 *
 * The element was answered: unless administrative, back up any valid
 * undo, copy the temporary game into the undo buffer, and assign the
 * pronouns the command used ready for the next element.
 */
static void
run_element_note_undo (scr_gameref_t game, const scr_char *command,
                       scr_bool was_undo_available)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Unless administrative, back up any valid undo, copy the temporary
   * game into the undo buffer, flag the undo buffer as available, and
   * assign any pronouns used in the command ready for the next iteration.
   */
  /*
   * An undo, restore or restart is never itself backed up: at 3.9 undo
   * and restore are real turns (see lib_is_version_390() in
   * sclibrar_print.cpp), and backing up an undo would re-arm the buffer it
   * just spent.
   */
  if (!game->is_admin && !game->do_restart && !game->do_restore
      && !(was_undo_available && !game->undo_available))
    {
      if (game->undo_available)
        memo_save_game (gs_get_memento (game), game->undo,
                        run_undo_text.c_str ());

      gs_copy (game->undo, game->temporary);
      run_undo_text = run_temporary_text;
      game->undo_available = TRUE;

      uip_assign_pronouns (game, command);
    }
  else if (run_get_version (bundle) >= TAF_VERSION_400
           && !game->do_restart && !game->do_restore)
    {
      /*
       * A 4.0 line that was not a turn -- a question, "see no such
       * thing" -- still leaves what the setter was handed; see
       * uip_note_antecedent_400() in scparser.cpp.
       */
      uip_commit_antecedent_400 (game);
      uip_set_pronoun_flags (FALSE, FALSE);
    }
  else if (run_get_version (bundle) == TAF_VERSION_390
           && !game->do_restart && !game->do_restore)
    uip_assign_pronouns (game, command);
}

/*
 * run_element_finish()
 *
 * After an answered element: the history, the rest of the line a walk
 * holds back, the restart/restore/undo special case, and the element
 * `again` will repeat.
 */
static void
run_element_finish (scr_gameref_t game, scr_bool is_rerunning,
                    scr_bool was_undo_available)
{
  const scr_memo_setref_t memento = gs_get_memento (game);

  /*
   * If do_again is set, we'll come round with the prior command in line
   * element in a moment, so save nothing for that case.  Otherwise save the
   * command in the history.
   */
  if (!scr_strempty (run_line_element) && !game->do_again)
    {
      /*
       * If this is a failed redo, redo_sequence will be set but do_again will
       * be clear.  Suppress the save for this special case; otherwise, failed
       * redo commands get into the history, where they can cause problems
       * later on.
       */
      if (game->redo_sequence == 0)
        {
          scr_int timestamp;

          timestamp = var_get_elapsed_seconds (gs_get_vars (game));
          memo_save_command (memento, run_line_element, timestamp,
                             game->turns);
        }
      else
        game->redo_sequence = 0;
    }

  /*
   * This element set a `go <place>` walk going: its steps are read at the
   * prompt, so the rest of the line must not run first.  Pre-4.0 holds it
   * until the arrival; run400's first step empties the queue.  See
   * run_goto_rest.
   */
  if (run_goto_next < run_goto_steps.size () && run_line_buffer[0] != NUL)
    {
      if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
        run_goto_rest = run_line_buffer;
      run_line_buffer[0] = NUL;
    }

  /*
   * Special case restart and restore commands; throw out any remaining input
   * and return straight away.  Do the same if this was an undo, detected by
   * noting that undo is no longer available, where it was on entry.
   */
  if (game->do_restart || game->do_restore
      || (was_undo_available && !game->undo_available))
    {
      run_line_buffer[0] = NUL;
      run_cancel_goto_walk ();
      return;
    }

  /* If not empty, consider as saving for "again" calls and in the history. */
  if (!scr_strempty (run_line_element))
    {
      /*
       * Unless "again", note this line element as prior input.  "Again" shows
       * up as do_again set in the game, where it wasn't when we entered here.
       */
      if (!game->do_again && !is_rerunning)
        strncpy (run_prior_element, run_line_element, LINE_BUFFER_SIZE);

      /*
       * If this was a request to run a command from the history, copy that
       * command into run_prior_element for the next iteration.  The library
       * should have verified the value in redo_sequence, so fetching the
       * command string should not fail.
       */
      if (game->do_again && game->redo_sequence != 0)
        {
          const scr_char *redo_command;

          redo_command = memo_find_command (memento, game->redo_sequence);
          if (redo_command)
            strncpy (run_prior_element, redo_command, LINE_BUFFER_SIZE);
          else
            {
              scr_error ("run_player_input: invalid redo sequence request\n");
              game->do_again = FALSE;
            }
          game->redo_sequence = 0;
        }
    }
}

/*
 * run_player_input()
 *
 * Take a line of player input and buffer it.  Split the line into elements
 * separated by periods.  For the first element, try to match it to either a
 * task or a standard command, and return TRUE if it matched, FALSE otherwise.
 *
 * On subsequent calls, successively work with the next line element until
 * none remain.  In this case, prompt for more player input and continue as
 * above.
 *
 * For the case of "again" or "g", rerun the last successful command element.
 *
 * One extra special special case; if called with a game that is not running,
 * this is a signal to reset all noted line input to initial conditions, and
 * just return.  Sorry about the ugliness.
 */
scr_bool
run_player_input (scr_gameref_t game)
{
  scr_bool is_rerunning, was_undo_available, status;
  scr_bool is_new_line = FALSE;
  std::string command;

  /* Special case; reset statics if the game isn't running. */
  if (!game->is_running)
    {
      run_input_reset ();
      return TRUE;
    }

  /*
   * Save the settings of the game's do_again and undo_available flags for
   * later checks.
   */
  is_rerunning = game->do_again;
  was_undo_available = game->undo_available;

  /* See if the player asked to rerun a command element. */
  if (game->do_again)
    {
      if (!run_repeat_element (game))
        return FALSE;
    }
  else
    {
      is_new_line = run_read_line (game);
      run_cut_element (game);
    }

  /* `again` is no element of its own; see run_count_element(). */
  if (!is_rerunning)
    run_count_element (game);

  /* Copy the current game to the temporary undo buffer, along with the
     output that the turn just finished left it with. */
  gs_copy (game->temporary, game);
  run_temporary_text = pf_take_printed (gs_get_filter (game));
  game->player_moved_by_command = FALSE;

  run_element_command (game, command);
  run_element_begin (game, command.c_str (), is_new_line);

  /*
   * The repeat words are tested on the whole line before any task gets it:
   * run400 89FE2 and run390 45F094 sit above tasks(0) (45F48B), run380 441B79
   * and run370 43B3C9 likewise, but without `g`.  So a task that listens for
   * `g` can never see it typed from 3.90 on; shadowpeak's riddle (TASK 404,
   * answer `g`) repeats the previous command in run400 (2026-09-14).  Below
   * 3.90 the `g` row in the standard table stays a Scarier abbreviation and
   * keeps its place after the tasks.
   */
  if (!is_rerunning && run_is_repeat_word (game, run_line_element))
    status = lib_cmd_again (game);
  else
    /* Try the command line element against command matchers. */
    status = run_all_commands (game, command.c_str ());

  status = run_element_questions (game, command.c_str (), status);

  if (run_element_co_answer_400 (game, command.c_str (), status))
    {
      run_line_buffer[0] = NUL;
      return status;
    }
  if (!status)
    {
      run_element_not_understood (game, command.c_str ());
      return status;
    }
  run_element_note_undo (game, command.c_str (), was_undo_available);
  run_element_finish (game, is_rerunning, was_undo_available);
  return status;
}


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
 * Look, quit, restart, undo, history/redo, hints, help and the other
 * out-of-game commands, plus wait, verbose/brief, notify and time.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * lib_cmd_look()
 *
 * Command handler for "look" command.
 */
scr_bool
lib_cmd_look (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_answer_break (filter);
  lib_describe_player_room (game, TRUE);
  return TRUE;
}


/*
 * lib_cmd_look_typed()
 *
 * The typed room look.  The Runner does not parse it: generaltasks compares
 * the whole line, after the game's SYNONYM rewrites, against a short list --
 *
 *     run400 48A5E3 / run390 45F5F2:  l, look, x room, x location,
 *         examine room, look room, examine location, l room
 *     run380 442377 / run370 43BB23:  l, look, x room, x location
 *
 * -- so bare `x`, `ex`, `examine`, `look at` and `the room` forms are not a
 * look.  Bare `x`, `ex` and `examine` are not an examine either: examines
 * opens with `If input = "examine" Or "ex" Or "x" Then Exit` (run400 471340,
 * run390 44B758), and the line ends in the game's DontUnderstand.  Measured
 * on Lair of the Vampire (4.00), whose synonym 2 rewrites `look` to `x`:
 * run400 answers `look` with "Try something different." in three separate
 * captures (runner_probes/lair.run400.[abc].txt), and because the room is
 * never listed, the cobalt key an earlier task dropped there stays unseen and
 * the next `get all` leaves it behind.
 *
 * 3.7/3.8 examines has no such exit, but the list is just as exact, so a
 * bare `x`, `ex`, `examine`, `exam` or `look at` enters examines and
 * answers "Nothing special." there, as do `examine room`, `look room`, `x
 * the room`, `look at room` and `look around`; `l room` is DontUnderstand.
 * Only the four listed lines look, in any case.  (`x,` is "Nothing
 * special." there too: the comma ends the word -- see
 * uip_match_whitespace().)  p37EXAM/p38EXAM
 * (harness/make_3738_examprobe.py), run370x
 * runner_probes/exam.run370.bare.rtf / runner_probes/exam.run370.bare_b.rtf,
 * run380x runner_probes/exam.run380.bare.rtf /
 * runner_probes/exam.run380.bare_b.rtf.
 *
 * Deliberate deviation: any line this row matches that names the room or
 * location looks, so `examine room`, `look at room` and `x the room` show
 * the room instead of "Nothing special." (3.7-3.9) or "You see no such
 * thing." (4.0).  Below 3.9 since 2026-09-27, at every version since
 * 2026-09-30 (bec2def78).  `look around` and `l around` look as well, at
 * every version since 2026-10-15; the Runners answer them as above.  The
 * bare forms keep the Runner's answer.  A
 * typed `l` or `look` that a game synonym would turn into a bare `x` stays
 * a look; see run_element_filtered().
 */
scr_bool
lib_cmd_look_typed (scr_gameref_t game)
{
  static const scr_char *const LOOK_LINES[] = {
    "l", "look", "x room", "x location",
    "examine room", "look room", "examine location", "l room", nullptr
  };
  const scr_char *input = run_get_dispatch_input ();

  if (input)
    {
      scr_owned_string owned_line (scr_strdup (input));
      scr_char *const line = owned_line.get ();
      const scr_char *const *entry;
      scr_bool is_look = FALSE;

      scr_normalize_string (line);
      /* The pre-3.9 list stops after "x location". */
      for (entry = LOOK_LINES;
           *entry && !is_look
           && (entry - LOOK_LINES < 4
               || lib_is_version_390 (game) || lib_is_version_400 (game));
           entry++)
        is_look = scr_strcasecmp (line, *entry) == 0;
      if (!is_look)
        {
          const scr_char *last = strrchr (line, ' ');
          last = last ? last + 1 : line;
          is_look = scr_strcasecmp (last, "room") == 0
                    || scr_strcasecmp (last, "location") == 0
                    || scr_strcasecmp (line, "look around") == 0
                    || scr_strcasecmp (line, "l around") == 0;
          if (is_look)
            SCR_DEVIATION ("look_at_room", nullptr);
        }
      if (!is_look)
        return FALSE;
    }

  return lib_cmd_look (game);
}


/*
 * lib_cmd_quit()
 *
 * Called on "quit", "bye" and "end".  Exits from the game main loop.
 *
 * Declining the confirmation is not silent: every Runner answers "I'm so glad
 * you said no...".  The branch is two statements, and the second runs whatever
 * the first did --
 *
 *     run370 loc_43C07E:  Me.Global.Unload MemVar_4461A8
 *     run370 loc_43C089:  MemVar_4460E4 = "I'm so glad you said no..."
 *
 * (run380 loc_44A6xx, run390 loc_45FA5D, run400 loc_48AABA/48AAC2 are the same
 * pair) -- because VB's Unload raises Form_QueryUnload, which is where the
 * Runner puts the "Are you sure?" box.  Confirm and the process is gone before
 * the assignment can matter; decline and Unload simply returns, leaving the
 * line as the turn's whole output.  Note it is an assignment, not an append,
 * so it *replaces* anything the turn had buffered; here the buffer is
 * necessarily empty, since a matched task would have taken the line before
 * run_standard_commands() ever ran.
 *
 * The literal is in all four constant pools, so this is not version-gated.
 */
scr_bool
lib_cmd_quit (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_QUIT))
    game->is_running = FALSE;
  else
    pf_buffer_string (gs_get_filter (game), "I'm so glad you said no...\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_endgame()
 *
 * Called on "endgame".  Ends the game there and then, printing nothing but
 * the score summary.
 *
 * This is *not* a synonym of `quit`, and it does not go through the ending
 * machinery either: the Runner writes the two lines inline in generaltasks and
 * then sets the gameover byte, so there is no WinText, no "Better luck next
 * time.", and no "Well done - you scored maximum points!" / "You finished N
 * points short." line -- those live in Form1.endmessage, which this path never
 * reaches.  run400 loc_48AAC9-48ABB8:
 *
 *     If cmd = "endgame" Then
 *       out &= "You scored" & Str(score) & " out of the maximum" & Str(MaxScore) & "!" & CRLF
 *       If MaxScore = 0 Then MaxScore = 1
 *       out &= "That is" & Str(Int(score * (100 / MaxScore))) & "% of the game!" & CRLF & CRLF
 *       out &= "[Press any key to end]"
 *       gameover = 3
 *
 * Note the order: the "out of the maximum" figure is the game's real MaxScore,
 * and the 0 -> 1 fix-up applies only to the divisor, so a scoreless game gets
 * "out of the maximum 0!" and "That is 0% of the game!".  That differs from
 * task_print_end_game_summary(), which follows Form1.endmessage in skipping
 * the whole summary for a scoreless 4.0 game and reporting a scoreless pre-4.0
 * one as 100%, so the two printers are deliberately kept apart.
 *
 * All four Runners carry the branch, identically worded (run370 loc_43C095,
 * run380 loc_442xxx, run390 loc_45FB94, run400 loc_48AAC9), and every literal
 * is in all four constant pools, so it is not gated on a version.
 *
 * "[Press any key to end]" is the Runner's own end-of-session prompt, printed
 * here for a host that ends the session on a keypress the way the Runner does;
 * see task_print_end_keyprompt().
 */
scr_bool
lib_cmd_endgame (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_score, divisor, percent;
  scr_char buffer[32];

  max_score = prop_get_global_integer (bundle, "MaxScore");

  pf_buffer_string (filter, "You scored ");
  snprintf (buffer, sizeof (buffer), "%ld", game->score);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, " out of the maximum ");
  snprintf (buffer, sizeof (buffer), "%ld", max_score);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, "!\n");

  divisor = max_score == 0 ? 1 : max_score;
  percent = (scr_int) floor (game->score * (100.0 / divisor));
  pf_buffer_string (filter, "That is ");
  snprintf (buffer, sizeof (buffer), "%ld", percent);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, "% of the game!\n");

  /* The second of the block's two CRLFs, and then the Runner's own
     end-of-session prompt on the far side of the blank line it leaves
     ("...% of the game!" & CRLF & CRLF & the prompt), exactly as
     Form1.endmessage closes an ending; see task_print_end_keyprompt(). */
  pf_buffer_answer_break (filter);
  task_print_end_keyprompt (game);

  /* Stop the game, and note that it's not resumeable -- the gameover byte the
     Runner sets here is the same one an EndGame task action writes. */
  game->is_running = FALSE;
  game->has_completed = TRUE;
  return TRUE;
}


/*
 * lib_cmd_control_panel()
 *
 * Called on "control panel", "control-panel", "control" and "panel".
 *
 * All four Runners take these as one whole-line test and open (or focus) the
 * Runner's Control Panel window, answering "Control Panel on" the first time
 * and "Control Panel already on." after that (run370 loc_43C34E, run400
 * loc_48AEAE).  That window is a Windows Runner feature with no counterpart
 * here, so rather than leave the words to fall through to the game -- which
 * the Runner never does -- say plainly that there is none.
 */
scr_bool
lib_cmd_control_panel (scr_gameref_t game)
{
  return lib_print_message (game,
                            "Scarier does not implement a Control Panel."
                            "  Try typing in commands instead.\n");
}


/*
 * lib_cmd_restart()
 *
 * Called on "restart".  Exits from the game main loop with restart
 * request set.
 */
scr_bool
lib_cmd_restart (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_RESTART))
    {
      game->is_running = FALSE;
      game->do_restart = TRUE;
    }

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_undo()
 *
 * Called on "undo".  Restores any undo game or memo to the main game.
 *
 * The wording is a three-way version split, and none of the three is what
 * SCARE used to say.  Measured 2026-09-07 from the four Runners' constant
 * pools and the handlers that print them:
 *
 *   3.70  has no `undo` at all -- the word does not occur in run370's pool,
 *         so the line falls through to the game's DontUnderstand.
 *   3.80  knows the word and has nothing behind it: run380 generaltasks
 *         @442EE9 tests `c("undo")` and answers "I can't undo your
 *         blundering." every time, with no state to restore.
 *   3.90+ answers "Undone." on success (run390 do_undo @436AB5, run400
 *         Proc_19_62 @45B0FF) and "I can't undo any more of your
 *         blunderings!" when the history is empty (@45B158).
 *
 * "[The previous turn has been undone.]", "Sorry, no more undo is
 * available." and "You can't undo what hasn't been done." are in no Runner's
 * pool at all, and neither Runner prints a room name with the answer:
 * runner_transcripts/cellar.txt (4.00) reads plain "Undone." three times
 * running.
 *
 * 3.9 and 4.0 also REPLAY the restored turn's output.  Each Runner keeps a
 * 10-deep record array (run400 MemVar_494124) whose field 0 is the turn's
 * whole output buffer, stamped from MemVar_4941B0 -- the output of the turn
 * before -- when the record is written at the start of each line (@48BD6E);
 * `undo` reads slot *1* and prints "Undone." & vbCrLf & the text that slot
 * holds, so undoing `e` re-prints what `take satchel` said, the next undo
 * what `x chair` said (runner_probes/cellar.run400.b.txt), and
 * runner_probes/hero.run400.txt's wait/wait/undo re-prints the first "Time
 * passes...".  An emptied slot is stamped "!!" (@45B146) and that sentinel is
 * what the availability test reads (@45AE5C); run390 do_undo has the same
 * shape.  Scarier keeps the text beside each undo state: the flushed output
 * taken as the temporary game is copied (run_get_undo_text() for the undo
 * game, the memo for older ones).
 */
scr_bool
lib_cmd_undo (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_memo_setref_t memento = gs_get_memento (game);

  /*
   * Deliberate deviation: 3.70 and 3.80 undo as 3.9/4.0 do.  The 3.70
   * Runner does not know the word and 3.80 refuses it every time, but an
   * interpreter that can undo should; a game's own `undo` task still wins,
   * because tasks are matched before the library (Cut_the_Red_Wire).  No
   * 3.7/3.8 corpus game names undo in a task.
   */

  /*
   * 3.9's undo leaves the turn counter where it is:
   * runner_probes/admin.run390.txt's `turns` after
   * undo/redo/x me/i/z/wait/exits/yes reads 37, one per element typed.
   */
  const scr_int turns = game->turns;
  std::string replay;

  /* If an undo buffer is available, restore it. */
  if (game->undo_available)
    {
      gs_copy (game, game->undo);
      if (lib_is_version_390 (game))
        game->turns = turns;
      game->undo_available = FALSE;

      pf_buffer_string (filter, "Undone.\n");
      pf_buffer_printed (filter, gs_get_vars (game), gs_get_bundle (game),
                         run_get_undo_text ());

      /* Undo can't properly unravel layered sounds... */
      game->stop_sound = TRUE;
    }

  /*
   * If there is no undo buffer, try to restore one saved previously in a
   * memo.  If that works, treat as for restore from file, since that's
   * effectively what it is.  The Runner's own history is ten records deep and
   * needs no such split; this is the port's second tier of the same thing, so
   * it answers with the same word.
   */
  else if (memo_load_game (memento, game, &replay))
    {
      pf_buffer_string (filter, "Undone.\n");
      pf_buffer_printed (filter, gs_get_vars (game), gs_get_bundle (game),
                         replay);
      if (lib_is_version_390 (game))
        game->turns = turns;

      game->is_running = FALSE;
      game->do_restore = TRUE;
    }

  /* If no undo buffer and memo restore failed, there's no undo available. */
  else
    pf_buffer_string (filter,
                      "I can't undo any more of your blunderings!\n");

  /* A turn in 3.9; see lib_is_version_390(). */
  game->is_admin = !lib_is_version_390 (game);
  return TRUE;
}


/*
 * lib_format_elapsed_time()
 *
 * Format a count of elapsed game seconds as "[Hh ][M]Mm SSs".
 */
static void
lib_format_elapsed_time (scr_int timestamp, scr_char *buffer, size_t length)
{
  scr_int hr, min, sec;

  /* Separate the timestamp out into components. */
  hr = timestamp / SECS_PER_HOUR;
  min = (timestamp % SECS_PER_HOUR) / SECS_PER_MINUTE;
  sec = timestamp % SECS_PER_MINUTE;

  if (hr > 0)
    snprintf (buffer, length, "%ldh %02ldm %02lds", hr, min, sec);
  else
    snprintf (buffer, length, "%ldm %02lds", min, sec);
}


/*
 * lib_cmd_history_common()
 * lib_cmd_history_number()
 * lib_cmd_history()
 *
 * Prints a history of saved commands for the game.  Print directly rather
 * than using the printfilter to avoid possible clashes with ALRs.
 */
static scr_bool
lib_cmd_history_common (scr_gameref_t game, scr_int limit)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_int first, count, timestamp;

  /*
   * The runner main loop will add an entry for the "history" command that
   * got us here, but it hasn't done so yet.  To keep the history list
   * accurate for recalling commands, we add a surrogate "history" command
   * to the history here, and remove it when we've done listing.  This matches
   * the c-shell, which always shows 'history' listed last.
   */
  timestamp = var_get_elapsed_seconds (vars);
  memo_save_command (memento, "[history]", timestamp, game->turns);

  /* Decide on the first history to display; all if limit is 0 or less. */
  if (limit > 0)
    {
      /*
       * Get a count of the history length recorded.  Because of the surrogate
       * "history" above, this is always at least one.  From this, choose a
       * start point for the display; all if not enough history.
       */
      count = memo_get_command_count (memento);
      first = (count > limit) ? count - limit : 0;
    }
  else
    first = 0;

  if_print_string ("These are your most recent game commands:\n\n");

  /* Display history starting at the first entry determined above. */
  memo_first_command (memento);
  for (count = 0; memo_more_commands (memento); count++)
    {
      const scr_char *command;
      scr_int sequence, turns;

      /* Obtain the history entry, and write if included. */
      memo_next_command (memento, &command, &sequence, &timestamp, &turns);
      if (count >= first)
        {
          scr_char buffer[64];

          /* Write the history entry sequence. */
          snprintf (buffer, sizeof(buffer), "%4ld -- Time ", sequence);
          if_print_string (buffer);

          /* Print playing time as "[HHh ][M]Mm SSs". */
          lib_format_elapsed_time (timestamp, buffer, sizeof(buffer));
          if_print_string (buffer);

          /* Follow up with the turns count, and the command string itself. */
          snprintf (buffer, sizeof(buffer), ", turn %ld : ", turns);
          if_print_string (buffer);
          if_print_string (command);
          if_print_character ('\n');
        }
    }

  /* Remove the surrogate "history"; the main loop will add the real one. */
  memo_unsave_command (memento);

  lib_set_admin (game);
  return TRUE;
}

scr_bool
lib_cmd_history_number (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int limit;

  /* Get requested length of history list, and complain if not valid. */
  limit = var_get_ref_number (vars);
  if (limit < 1)
    {
      if_print_string ("That's not a valid history length.\n");

      game->is_admin = TRUE;
      return TRUE;
    }

  return lib_cmd_history_common (game, limit);
}

scr_bool
lib_cmd_history (scr_gameref_t game)
{
  return lib_cmd_history_common (game, 0);
}


/*
 * lib_cmd_again()
 * lib_cmd_redo_number()
 * lib_cmd_redo_text_last_common()
 * lib_cmd_redo_text()
 * lib_cmd_redo_last()
 *
 * The first function is called on "again", and simply sets the game do_again
 * flag.  The others allow the user to select a command from the history list
 * to re-run.
 */
scr_bool
lib_cmd_again (scr_gameref_t game)
{
  game->do_again = TRUE;
  game->redo_sequence = 0;

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_redo_number (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_int sequence;

  /*
   * Get the history sequence entry requested and validate it.  The sequence
   * may be positive (absolute) or negative (relative to history end), but
   * not zero.
   */
  sequence = var_get_ref_number (vars);
  if (sequence != 0 && memo_find_command (memento, sequence))
    {
      game->do_again = TRUE;
      game->redo_sequence = sequence;
    }
  else
    {
      if_print_string ("No matching entry found in the command history.\n");

      /*
       * This is a failed redo, but returning FALSE will cause the game's
       * unknown command message to come up.  However, returning TRUE will
       * cause the runner main loop to add this to its history, and at some
       * point a "redo 7" could cause problems (say, when it's at sequence 7,
       * where it'll cause an infinite loop).  To work round this, here we'll
       * return a redo_sequence _without_ do_again, and have the runner catch
       * that as an indication not to save the command in its history.  Sorry
       * for the ugliness.
       */
      game->do_again = FALSE;
      game->redo_sequence = INT_MAX;
    }

  game->is_admin = TRUE;
  return TRUE;
}

static scr_bool
lib_cmd_redo_text_last_common (scr_gameref_t game, const scr_char *target)
{
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_bool is_do_last, is_contains;
  scr_int length, matched_sequence;

  /* Make a special case of "!!", rerun the final command in the history. */
  is_do_last = (strcmp (target, "!") == 0);

  /*
   * Differentiate starts-with and contains searches, setting is_contains and
   * advancing by one if the target begins '?' (word search).  Note target
   * string length.
   */
  is_contains = (target[0] == '?');
  target += is_contains ? 1 : 0;
  length = strlen (target);

  /* If there's no text left to search for, reject this call now. */
  if (length == 0)
    {
      if_print_string ("No matching entry found in the command history.\n");

      /* As with failed numeric redo above, special-case this return. */
      game->do_again = FALSE;
      game->redo_sequence = INT_MAX;

      game->is_admin = TRUE;
      return TRUE;
    }

  /*
   * Search saved commands for one that matches the target string in the
   * required way.  We want to return the most recently saved match, so ideally
   * we'd search backwards, but the iterator is only forwards, so we do it the
   * hard way.
   */
  matched_sequence = 0;
  memo_first_command (memento);
  while (memo_more_commands (memento))
    {
      const scr_char *command;
      scr_int sequence, timestamp, turns;
      scr_bool is_matched;

      /* Get the command; only command and sequence are relevant. */
      memo_next_command (memento, &command, &sequence, &timestamp, &turns);

      /*
       * If this is the "!!" special case, match everything.  Otherwise,
       * either search the command for the target, or match if the command
       * begins with the target.
       */
      if (is_do_last)
        is_matched = TRUE;
      else if (is_contains)
        {
          scr_int index_;

          /* Search this command for an occurrence of target anywhere. */
          is_matched = FALSE;
          for (index_ = strlen (command) - length; index_ >= 0; index_--)
            {
              if (scr_strncasecmp (command + index_, target, length) == 0)
                {
                  is_matched = TRUE;
                  break;
                }
            }
        }
      else
        is_matched = (scr_strncasecmp (command, target, length) == 0);

      /* If the command matched the target criteria, note it and continue. */
      if (is_matched)
        matched_sequence = sequence;
    }

  /* If we found a match, set the redo values accordingly. */
  if (matched_sequence > 0)
    {
      game->do_again = TRUE;
      game->redo_sequence = matched_sequence;
    }
  else
    {
      if_print_string ("No matching entry found in the command history.\n");

      /* As with failed numeric redo above, special-case this return. */
      game->do_again = FALSE;
      game->redo_sequence = INT_MAX;
    }

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_redo_text (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  /* Call the common redo with the referenced text from %text%. */
  return lib_cmd_redo_text_last_common (game, var_get_ref_text (vars));
}

scr_bool
lib_cmd_redo_last (scr_gameref_t game)
{
  /* Call the common redo with, literally, "!", forming "!!" . */
  return lib_cmd_redo_text_last_common (game, "!");
}


/*
 * lib_cmd_hints()
 *
 * Called on "hints".  Requests the interface to display any available hints.
 */
scr_bool
lib_cmd_hints (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int task;
  scr_bool game_has_hints;

  /*
   * Check for the presence of any game hints at all, no matter whether the
   * task is runnable or not.
   */
  game_has_hints = FALSE;
  GS_FOR_EACH_TASK (game, task)
    {
      if (task_has_hints (game, task))
        {
          game_has_hints = TRUE;
          break;
        }
    }

  /* If the game has hints, display any relevant ones. */
  if (game_has_hints)
    {
      if (run_hint_iterate (game, nullptr))
        {
          if (if_confirm (SCR_CONF_VIEW_HINTS))
            if_display_hints (game);
        }
      else
        {
          /* The Runners' own line: run380 42D2D4, run390 437A24 and run400
             45A0CC all say "No hints currently available."; run370 426D72
             "No hints available.".  Dolg (3.90) ALRs the 3.8+ wording into
             Russian, so the exact text matters (runner_transcripts/dolg.txt
             T4). */
          const scr_prop_setref_t bundle = gs_get_bundle (game);

          pf_buffer_string (filter,
                            prop_get_taf_version (bundle) < TAF_VERSION_380
                            ? "No hints available.\n"
                            : "No hints currently available.\n");
        }
    }
  else
    {
      pf_buffer_string (filter,
                        "There are no hints available for this adventure.\n");
      pf_buffer_string (filter,
                        "You're just going to have to work it out for"
                        " yourself...\n");
    }

  /* A turn in 3.7-3.9; see lib_is_version_390(). */
  game->is_admin = lib_is_version_400 (game);
  return TRUE;
}


/*
 * lib_print_string_bold()
 * lib_print_string_italics()
 *
 * Convenience helpers for printing licensing and game information.
 */
static void
lib_print_string_bold (const scr_char *string)
{
  if_print_tag (SCR_TAG_BOLD, "");
  if_print_string (string);
  if_print_tag (SCR_TAG_ENDBOLD, "");
}

static void
lib_print_string_italics (const scr_char *string)
{
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string (string);
  if_print_tag (SCR_TAG_ENDITALICS, "");
}


/*
 * lib_cmd_help()
 * lib_cmd_license()
 *
 * A form of standard help output for games that don't define it themselves,
 * and the GPL licensing.  Print directly rather than using the printfilter
 * to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_help (scr_gameref_t game)
{
  if_print_string (
    "These are some of the typical commands used in this adventure:\n\n");

  if_print_string (
    "  [N]orth, [E]ast, [S]outh, [W]est, [U]p, [D]own, [In], [O]ut,"
    " [L]ook, [Exits]\n  E[x]amine <object>, [Get <object>],"
    " [Drop <object>], [...it], [...all]\n  [Where is <object>]\n"
    "  [Give <object> to  <character>], [Open...], [Close...],"
    " [Ask <character> about <subject>]\n"
    "  [Wear <object>], [Remove <object>], [I]nventory\n"
    "  [Put <object> into <object>], [Put <object> onto <object>]\n");

  if_print_string ("\nUse the ");
  lib_print_string_italics ("Save");
  if_print_string (", ");
  lib_print_string_italics ("Restore");
  if_print_string (", ");
  lib_print_string_italics ("Undo");
  if_print_string (", and ");
  lib_print_string_italics ("Quit");
  if_print_string (
    " commands to save and restore games, undo a move, and leave the"
    " game.  Use ");
  lib_print_string_italics ("History");
  if_print_string (" and ");
  lib_print_string_italics ("Redo");
  if_print_string (
    " to view and repeat recent game commands.\n");

  if_print_string ("\nThe ");
  lib_print_string_italics ("Hint");
  if_print_string (" command displays any game hints, ");
  lib_print_string_italics ("Notify");
  if_print_string (" provides score change notification, and ");
  lib_print_string_italics ("Verbose");
  if_print_string (" and ");
  lib_print_string_italics ("Brief");
  if_print_string (" control room descriptions.\n");

  if_print_string ("\nUse ");
  lib_print_string_italics ("License");
  if_print_string (
    " to view Scarier's licensing terms and conditions, and ");
  lib_print_string_italics ("Version");
  if_print_string (
    " to print both Scarier's and the game's version number.\n");

  /* A turn in 3.7-3.9; see lib_is_version_390(). */
  game->is_admin = lib_is_version_400 (game);
  return TRUE;
}

scr_bool
lib_cmd_license (scr_gameref_t game)
{
  lib_print_string_bold ("Scarier");
  if_print_string (" is ");
  lib_print_string_italics (
    "Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford,"
    " Copyright (C) 2026  Petter Sj\xf6lund");
  if_print_string (".\n\n");

  if_print_string (
    "This program is free software; you can redistribute it and/or modify"
    " it under the terms of version 2 of the GNU General Public License"
    " as published by the Free Software Foundation.\n\n");

  if_print_string (
    "This program is distributed in the hope that it will be useful, but ");
  lib_print_string_bold ("WITHOUT ANY WARRANTY");
  if_print_string ("; without even the implied warranty of ");
  lib_print_string_bold ("MERCHANTABILITY");
  if_print_string (" or ");
  lib_print_string_bold ("FITNESS FOR A PARTICULAR PURPOSE");
  if_print_string (
    ".  See the GNU General Public License for more details.\n\n");

  if_print_string (
    "You should have received a copy of the GNU General Public License"
    " along with this program; if not, write to the Free Software"
    " Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301"
    " USA\n\n");

  if_print_string ("Please report any bugs, omissions, or misfeatures at ");
  lib_print_string_italics ("https://github.com/angstsmurf/spatterlight/issues");
  if_print_string (".\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_information()
 *
 * Display a few small pieces of game information, done by a dialog GUI
 * in real Adrift.  Prints directly rather than using the printfilter to
 * avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_information (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[1];
  const scr_char *gamename, *compile_date, *gameauthor;
  scr_char *filtered;

  gamename = prop_get_global_string (bundle, "GameName");
  filtered = pf_filter_for_info (gamename, vars);
  pf_strip_tags (filtered);

  if_print_string ("\"");
  if_print_string (!scr_strempty (filtered) ? filtered : "Untitled");
  if_print_string ("\"");
  scr_free (filtered);

  vt_key[0].string = "CompileDate";
  compile_date = prop_get_string (bundle, "S<-s", vt_key);
  if (!scr_strempty (compile_date))
    {
      if_print_string (", ");
      if_print_string (compile_date);
    }

  gameauthor = prop_get_global_string (bundle, "GameAuthor");
  filtered = pf_filter_for_info (gameauthor, vars);
  pf_strip_tags (filtered);

  if_print_string (", ");
  if_print_string (!scr_strempty (filtered) ? filtered : "Anonymous");
  if_print_string (".\n");
  scr_free (filtered);

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_cmd_clear()
 *
 * Clear the main game window (almost).
 */
scr_bool
lib_cmd_clear (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_tag (filter, SCR_TAG_CLS);
  pf_buffer_string (filter, "Screen cleared.\n");

  /* A turn in 3.7-3.9; see lib_is_version_390(). */
  game->is_admin = lib_is_version_400 (game);
  return TRUE;
}


/*
 * lib_cmd_statusline()
 *
 * Display the status line as would be shown by the Runner.  Useful for
 * interpreter builds that can't offer a true status line.  Prints directly
 * rather than using the printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_statusline (scr_gameref_t game)
{
  const scr_char *name, *author, *room, *status;
  scr_int score;

  /*
   * Retrieve the game's name and author, the description of the current
   * game room, and any formatted game status line.
   */
  run_get_attributes (game, &name, &author, nullptr, nullptr,
                      &score, nullptr, &room, &status,
                      nullptr, nullptr, nullptr, nullptr);

  /* If nothing is yet determined, print the game name and author. */
  if (!room || scr_strempty (room))
    {
      if_print_string (name);
      if_print_string (" | ");
      if_print_string (author);
    }
  else
    {
      /* Print the player location, and a separator. */
      if_print_string (room);
      if_print_string (" | ");

      /* If the game offers a status line, print it, otherwise the score. */
      if (status && !scr_strempty (status))
        if_print_string (status);
      else
        {
          scr_char buffer[32];

          if_print_string ("Score: ");
          snprintf (buffer, sizeof(buffer), "%ld", score);
          if_print_string (buffer);
        }
    }
  if_print_character ('\n');

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_version()
 *
 * Display the "Runner version".  Prints directly rather than using the
 * printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_version (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key;
  scr_char buffer[64];
  scr_int major, minor, point;
  const scr_char *version;

  if_print_string ("Scarier version ");
  if_print_string (SCARIER_VERSION SCARIER_PATCH_LEVEL);
  if_print_string (" [Adrift ");
  major = SCARIER_EMULATION / 1000;
  minor = (SCARIER_EMULATION % 1000) / 100;
  point = SCARIER_EMULATION % 100;
  snprintf (buffer, sizeof(buffer), "%ld.%02ld.%02ld", major, minor, point);
  if_print_string (buffer);
  if_print_string (" compatible], ");

  vt_key.string = "VersionString";
  version = prop_get_string (bundle, "S<-s", &vt_key);
  if_print_string ("Generator version ");
  if_print_string (version);
  if_print_string (".\n");

  /* A turn in 3.9; see lib_is_version_390(). */
  game->is_admin = !lib_is_version_390 (game);
  return TRUE;
}


/*
 * lib_cmd_wait()
 * lib_cmd_wait_number()
 *
 * Set game waitcounter to a count of turns for which the main loop will run
 * without taking input.  Many Adrift Runners ignore any WaitTurns setting in
 * the game, and use always use one; this might make a game misbehave, so to
 * try to cover this case we supply 'wait N' as a player control to override
 * the game's setting.  The latter prints directly rather than using the
 * printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_wait (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int waitturns;

  /* Note if wait turns is different from the game's setting. */
  waitturns = prop_get_global_integer (bundle, "WaitTurns");
  if (waitturns != game->waitturns)
    {

      pf_buffer_string (filter, "(");
      pf_buffer_integer (filter, game->waitturns);
      pf_buffer_string (filter,
                        game->waitturns == 1 ? " turn)\n" : " turns)\n");
    }

  /* Reset the wait counter to the current waitturns setting. */
  game->waitcounter = game->waitturns;

  /*
   * 4.0 stores "Time passes..." with a vbCrLf of its own (run400 48ABDA:
   * the literal, then Proc_21_4_442418 which returns vbCrLf), so a walk
   * announcement in the same turn starts a new line instead of joining
   * with two spaces; 3.9 stores the bare literal (run390 45E636) and joins.
   * Measured run400, EV15, runner_probes/ev15.run400.txt: "Time passes..." /
   * "Walker arrives from the east." on separate lines.
   */
  pf_buffer_string (filter, "Time passes...\n");
  if (lib_is_version_400 (game))
    pf_buffer_hard_break (filter);
  return TRUE;
}


scr_bool
lib_cmd_wait_number (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int waitturns;
  scr_char buffer[32];

  /* Get and validate the waitturns setting. */
  waitturns = var_get_ref_number (vars);
  if (waitturns < 1 || waitturns > 20)
    {
      if_print_string ("You can only wait between 1 and 20 turns.\n");
      game->is_admin = TRUE;
      return TRUE;
    }

  /* Update the game setting, and confirm for the player. */
  game->waitturns = waitturns;

  if_print_string ("The game will now wait ");
  snprintf (buffer, sizeof(buffer), "%ld", waitturns);
  if_print_string (buffer);
  if_print_string (waitturns == 1 ? " turn" : " turns");
  if_print_string (" for each 'wait' command you enter.\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_verbose()
 * lib_cmd_brief()
 *
 * Set/clear game verbose flag.  Print directly rather than using the
 * printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_verbose (scr_gameref_t game)
{
  /* Set game verbose flag and return. */
  game->verbose = TRUE;
  if_print_string ("The game is now in its ");
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string ("verbose");
  if_print_tag (SCR_TAG_ENDITALICS, "");
  if_print_string (" mode, which always gives long descriptions of locations"
                   " (even if you've been there before).\n");

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_brief (scr_gameref_t game)
{
  /* Clear game verbose flag and return. */
  game->verbose = FALSE;
  if_print_string ("The game is now in its ");
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string ("brief");
  if_print_tag (SCR_TAG_ENDITALICS, "");
  if_print_string (" mode, which gives long descriptions of places never"
                   " before visited and short descriptions otherwise.\n");

  game->is_admin = TRUE;
  return TRUE;
}

/*
 * lib_cmd_notify_on_off()
 * lib_cmd_notify()
 *
 * Set/clear/query game score change notification flag.  Print directly
 * rather than using the printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_notify_on_off (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *control;

  /* Get the text following the notify command, and check for "on"/"off". */
  control = var_get_ref_text (vars);
  if (scr_strcasecmp (control, "on") == 0)
    {
      /* Set score change notification. */
      game->notify_score_change = TRUE;
      if_print_string ("Game score change notification is now ");
      if_print_tag (SCR_TAG_ITALICS, "");
      if_print_string ("on");
      if_print_tag (SCR_TAG_ENDITALICS, "");
      if_print_string (", and the game will tell you of any changes in the"
                       " score.\n");
    }
  else if (scr_strcasecmp (control, "off") == 0)
    {
      /* Clear score change notification. */
      game->notify_score_change = FALSE;
      if_print_string ("Game score change notification is now ");
      if_print_tag (SCR_TAG_ITALICS, "");
      if_print_string ("off");
      if_print_tag (SCR_TAG_ENDITALICS, "");
      if_print_string (", and the game will be silent on changes in the"
                       " score.\n");
    }
  else
    {
      if_print_string ("Use 'notify on' or 'notify off' to control game"
                       " score notification.\n");
    }

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_notify (scr_gameref_t game)
{
  /* Report the current state of notification. */
  if_print_string ("Game score change notification is ");
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string (game->notify_score_change ? "on" : "off");
  if_print_tag (SCR_TAG_ENDITALICS, "");

  if (game->notify_score_change)
    {
      if_print_string (", and the game will tell you of any changes in the"
                       " score.\n");
    }
  else
    {
      if_print_string (", and the game will be silent on changes in the"
                       " score.\n");
    }

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_time()
 * lib_cmd_date()
 *
 * Print elapsed game time, and smart-alec "date" response.  The Adrift
 * Runner responds here with the system time and date, but we'll do something
 * different.
 */
scr_bool
lib_cmd_time (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_char buffer[64];

  /* Get elapsed game time and convert to hour, minutes, and seconds. */
  lib_format_elapsed_time (var_get_elapsed_seconds (vars), buffer,
                           sizeof(buffer));

  /* Print the game's elapsed time. */
  if_print_string ("You have been running the game for ");
  if_print_string (buffer);
  if_print_string (".\n");

  /* A turn in 3.9; see lib_is_version_390(). */
  game->is_admin = !lib_is_version_390 (game);
  return TRUE;
}

scr_bool
lib_cmd_date (scr_gameref_t game)
{
  return lib_print_message (game, "Maybe we should just be good friends.\n");
}

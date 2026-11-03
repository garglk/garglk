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
 * Game lifecycle, session state, the main loop and Runner load-draw replay.
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
#include "sessrec.h"



/*
 * run_loop_halt
 *
 * Control-flow exception used to unwind out of run_main_loop() back to
 * run_interpret() when a *running* game is quit / restarted / restored / has a
 * turn undone.  This replaces a longjmp(game->quitter) that skipped the
 * destructors of any non-trivial C++ local live in the command/task/print/expr
 * call tree -- undefined behaviour once that tree holds std::string/std::vector,
 * which is what blocked RAII across the runner.  Throwing unwinds the same
 * frames but runs their destructors.  All throw sites and the sole catch are in
 * this file; the meaning (quit vs restart vs restore) is still carried by the
 * game's do_restart/do_restore flags exactly as before, so the type is empty.
 *
 * It is caught specifically (never `catch (...)`) so that a P2 scr_fatal_error
 * -- or any other genuine exception -- still propagates to the scinterf boundary
 * instead of being mistaken for a normal halt.
 */
namespace { struct run_loop_halt {}; }


/*
 * run_update_status()
 *
 * Update the game's current room and status line strings.
 */
static void
run_update_status (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *name, *status;
  scr_char *filtered;
  scr_bool statusbox;

  /* Get the current room name, and filter and untag it. */
  name = lib_get_room_name (game, gs_playerroom (game));
  filtered = pf_filter (name, vars, bundle);
  pf_strip_tags (filtered);

  /* Save this room name; the owning pointer frees any existing name. */
  game->current_room_name.reset (filtered);

  /* See if the game does a status box. */
  statusbox = prop_get_global_boolean (bundle, "StatusBox");
  if (statusbox)
    {
      /* Get the status line, and filter and untag it. */
      status = prop_get_global_string (bundle, "StatusBoxText");
      filtered = pf_filter (status, vars, bundle);
      pf_strip_tags (filtered);
    }
  else
    /* No status line, so use NULL. */
    filtered = nullptr;

  /* Save this status text; the owning pointer frees any existing line. */
  game->status_line.reset (filtered);
}


/*
 * run_notify_score_change()
 *
 * Print an indication of any score change, if appropriate.  The change is
 * detected by comparing against the undo game.  Uses if_print_string()
 * directly for printing, rather than the filter, so that it can place its
 * output ahead of buffered printfilter text.
 */
static void
run_notify_score_change (scr_gameref_t game)
{
  const scr_gameref_t undo = game->undo;
  scr_char buffer[32];
  assert (gs_is_game_valid (undo));

  /*
   * Do nothing if no undo available, or if notification is off, or if we've
   * already done this once this turn.
   */
  if (!game->undo_available
      || !game->notify_score_change || game->has_notified)
    return;

  /* Note any change in the score. */
  if (game->score > undo->score)
    {
      if_print_string ("(Your score has increased by ");
      snprintf (buffer, sizeof(buffer), "%ld", game->score - undo->score);
      if_print_string (buffer);
      if_print_string (")\n");
    }
  else if (game->score < undo->score)
    {
      if_print_string ("(Your score has decreased by ");
      snprintf (buffer, sizeof(buffer), "%ld", undo->score - game->score);
      if_print_string (buffer);
      if_print_string (")\n");
    }
  game->has_notified = TRUE;
}


/*
 * run_session_state()
 * run_restore_session_state()
 *
 * What a Spatterlight autosave needs beyond the saved game.  The save stream
 * carries what an ADRIFT save file does, and an in-game restore deliberately
 * leaves the rest alone; an autorestore resumes the same session, so it puts
 * that back as well:
 *
 *   - the line element `again` repeats, and the command history;
 *   - the pronouns, in the game and in the one-turn undo buffer, and the
 *     parser's pronoun echo flags;
 *   - a question the next line answers: the 4.0 ambiguity prompt (see
 *     lib_co_400_raise()) together with the list it offered, which decides
 *     "That is still ambiguous!" against the prompt, and the question
 *     prefix ("Who do you want to attack?", "Wear what?", "...with?");
 *   - the output each undo state replays after "Undone." (see
 *     lib_cmd_undo()): the finished turn's, still to be taken by the next
 *     line; the undo game's; and one per memo ring entry, oldest first, set
 *     after the container has rebuilt the ring;
 *   - the player's settings: verbose, score notification and wait turns;
 *   - the name and gender typed at the startup prompts, which live in the
 *     property bundle;
 *   - which startup prompt the save was taken at, if either: see
 *     run_startup_prompt().
 *
 * The encoding is a run of session records (sessrec.h), the framing the
 * ADRIFT 5 engine's parser continuation shares.  The reader skips keys it
 * does not know and keeps the current value of any it does not find, so
 * records can be added without breaking an older autosave; only broken
 * framing fails the restore.
 */
/*
 * The startup prompt a line is being read for, and the one an autorestore
 * resumes at: 0 for neither, else RUN_STARTUP_NAME or RUN_STARTUP_GENDER.
 * A game saved at one of them has not shown its first room, so the main loop
 * runs its startup block again on the relaunch; run_startup_resume has it
 * leave out what the restored transcript already shows -- the title, the
 * startup text, any earlier prompt, and the question itself.
 */
enum { RUN_STARTUP_NAME = 1, RUN_STARTUP_GENDER = 2 };
static scr_int run_startup_stage = 0;
static scr_int run_startup_resume = 0;

scr_int
run_startup_prompt (void)
{
  return run_startup_stage;
}

static std::string
run_session_join (const std::vector<scr_int> &values)
{
  std::string text;
  size_t index_;

  for (index_ = 0; index_ < values.size (); index_++)
    {
      if (index_ > 0)
        text += ' ';
      text += std::to_string ((long) values[index_]);
    }
  return text;
}

/* Up to count leading integers; *rest, if given, is what follows them. */
static std::vector<scr_int>
run_session_split (const std::string &text, size_t count,
                   const scr_char **rest)
{
  std::vector<scr_int> values;
  const scr_char *cursor = text.c_str ();

  while (values.size () < count)
    {
      scr_char *end;
      const long value = strtol (cursor, &end, 10);

      if (end == cursor)
        break;
      values.push_back (value);
      cursor = end;
    }
  if (rest)
    *rest = (*cursor == ' ') ? cursor + 1 : cursor;
  return values;
}

static std::string
run_session_pronouns (scr_gameref_t game)
{
  return run_session_join ({game->it_object, game->it_form,
                            game->him_npc, game->her_npc, game->it_npc,
                            game->last_npc});
}

static void
run_session_set_pronouns (scr_gameref_t game, scr_gameref_t target,
                          const std::string &text)
{
  const std::vector<scr_int> values (run_session_split (text, 6, nullptr));
  const scr_int npcs = gs_npc_count (game);
  scr_int index_;

  if (values.size () < 6
      || values[0] < -1 || values[0] >= gs_object_count (game))
    return;
  for (index_ = 2; index_ < 6; index_++)
    {
      if (values[index_] < -1 || values[index_] >= npcs)
        return;
    }

  target->it_object = values[0];
  target->it_form = values[1] >= UIP_IT_INDEFINITE && values[1] <= UIP_IT_BARE
                    ? values[1] : UIP_IT_INDEFINITE;
  target->him_npc = values[2];
  target->her_npc = values[3];
  target->it_npc = values[4];
  target->last_npc = values[5];
}

std::string
run_session_state (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  std::string out, term, command, prefix, prefix_at_line, with_prefix;
  std::vector<scr_int> candidates, offered_list;
  scr_bool is_pending, offered, used, definite;

  sessrec_put (out, "name", prop_get_global_string (bundle, "PlayerName"));
  sessrec_put (out, "gender",
               std::to_string ((long) prop_get_global_integer
                               (bundle, "PlayerGender")));
  if (run_startup_stage != 0)
    sessrec_put (out, "startup_prompt",
                 std::to_string ((long) run_startup_stage));
  sessrec_put (out, "settings",
               run_session_join ({game->verbose,
                                  game->notify_score_change,
                                  game->waitturns}));

  sessrec_put (out, "pronouns", run_session_pronouns (game));
  if (game->undo_available)
    sessrec_put (out, "undo_pronouns", run_session_pronouns (game->undo));
  uip_get_pronoun_flags (&used, &definite);
  sessrec_put (out, "pronoun_flags", run_session_join ({used, definite}));

  sessrec_put (out, "printed", pf_get_printed (gs_get_filter (game)));
  if (game->undo_available)
    sessrec_put (out, "undo_text", run_undo_text);
  for (scr_int index_ = 0; index_ < memo_get_undo_count (memento); index_++)
    sessrec_put (out, "ring_text", memo_get_undo_text (memento, index_));

  sessrec_put (out, "again", run_prior_element);
  sessrec_put (out, "typed_line", run_typed_line);
  sessrec_put (out, "previous_typed_line", run_previous_typed_line);
  memo_first_command (memento);
  while (memo_more_commands (memento))
    {
      const scr_char *entry;
      scr_int sequence, timestamp, turns;

      memo_next_command (memento, &entry, &sequence, &timestamp, &turns);
      sessrec_put (out, "history",
                   run_session_join ({sequence, timestamp, turns})
                   + ' ' + entry);
    }

  lib_co_400_get_question (&is_pending, &term, &command, &candidates,
                           &offered, &offered_list);
  if (is_pending)
    {
      sessrec_put (out, "which_term", term);
      sessrec_put (out, "which_command", command);
      sessrec_put (out, "which_candidates", run_session_join (candidates));
    }
  /* The list the last prompt offered outlives the question: a line that
     ties on it again gets "That is still ambiguous!", not the prompt. */
  if (offered)
    sessrec_put (out, "which_offered", run_session_join (offered_list));
  lib_battle_who_get_prefix (&prefix, &prefix_at_line);
  sessrec_put (out, "prefix", prefix);
  sessrec_put (out, "prefix_at_line", prefix_at_line);
  lib_with_prefix_390_get (&with_prefix);
  sessrec_put (out, "with_prefix", with_prefix);
  return out;
}

scr_bool
run_restore_session_state (scr_gameref_t game, const std::string &state)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  std::string which_term, which_command, which_candidates, which_offered;
  std::string prefix, prefix_at_line, with_prefix;
  scr_bool has_which = FALSE, has_offered = FALSE, has_prefix = FALSE;
  scr_bool has_history = FALSE, has_with_prefix = FALSE;
  scr_int ring_text = 0;
  scr_vartype_t vt_key[2];
  size_t pos = 0;

  vt_key[0].string = "Globals";
  while (pos < state.size ())
    {
      std::string key, value;

      if (!sessrec_next (state, &pos, &key, &value))
        return FALSE;

      const std::vector<scr_int> numbers
          (run_session_split (value, (size_t) -1, nullptr));

      if (key == "name")
        {
          vt_key[1].string = "PlayerName";
          prop_put_string (bundle, "S<-ss", value.c_str (), vt_key);
        }
      else if (key == "gender" && numbers.size () == 1
               && (numbers[0] == NPC_MALE || numbers[0] == NPC_FEMALE
                   || numbers[0] == NPC_NEUTER))
        {
          vt_key[1].string = "PlayerGender";
          prop_put_integer (bundle, "I<-ss", numbers[0], vt_key);
        }
      else if (key == "startup_prompt" && numbers.size () == 1
               && (numbers[0] == RUN_STARTUP_NAME
                   || numbers[0] == RUN_STARTUP_GENDER))
        run_startup_resume = numbers[0];
      else if (key == "settings" && numbers.size () >= 3)
        {
          game->verbose = numbers[0] != 0;
          game->notify_score_change = numbers[1] != 0;
          if (numbers[2] >= 0)
            game->waitturns = numbers[2];
        }
      else if (key == "pronouns")
        run_session_set_pronouns (game, game, value);
      else if (key == "undo_pronouns" && game->undo_available)
        run_session_set_pronouns (game, game->undo, value);
      else if (key == "pronoun_flags" && numbers.size () >= 2)
        uip_set_pronoun_flags (numbers[0] != 0, numbers[1] != 0);
      else if (key == "printed")
        pf_set_printed (gs_get_filter (game), value);
      else if (key == "undo_text" && game->undo_available)
        run_undo_text = value;
      else if (key == "ring_text")
        memo_set_undo_text (memento, ring_text++, value.c_str ());
      else if (key == "again" && value.size () < LINE_BUFFER_SIZE)
        memcpy (run_prior_element, value.c_str (), value.size () + 1);
      else if (key == "typed_line")
        run_typed_line = value;
      else if (key == "previous_typed_line")
        run_previous_typed_line = value;
      else if (key == "history")
        {
          const scr_char *entry;
          const std::vector<scr_int> fields
              (run_session_split (value, 3, &entry));

          if (!has_history)
            memo_clear_commands (memento);
          has_history = TRUE;
          if (fields.size () == 3)
            memo_restore_command (memento, entry,
                                  fields[0], fields[1], fields[2]);
        }
      else if (key == "which_term")
        which_term = value, has_which = TRUE;
      else if (key == "which_command")
        which_command = value;
      else if (key == "which_candidates")
        which_candidates = value;
      else if (key == "which_offered")
        which_offered = value, has_offered = TRUE;
      else if (key == "prefix")
        prefix = value, has_prefix = TRUE;
      else if (key == "prefix_at_line")
        prefix_at_line = value;
      else if (key == "with_prefix")
        with_prefix = value, has_with_prefix = TRUE;
    }

  if (has_which || has_offered)
    {
      std::vector<scr_int> candidates
          (run_session_split (which_candidates, (size_t) -1, nullptr));
      std::vector<scr_int> offered
          (run_session_split (which_offered, (size_t) -1, nullptr));
      scr_bool valid = TRUE;

      for (scr_int candidate : candidates)
        valid = valid && candidate >= 0 && candidate < gs_object_count (game);
      for (scr_int candidate : offered)
        valid = valid && candidate >= 0 && candidate < gs_object_count (game);
      if (valid)
        lib_co_400_set_question (has_which, which_term, which_command,
                                 candidates, has_offered, offered);
    }
  if (has_prefix)
    lib_battle_who_set_prefix (prefix, prefix_at_line);
  if (has_with_prefix)
    lib_with_prefix_390_set (with_prefix);
  return TRUE;
}


/*
 * run_text_ends_in_newline()
 *
 * Return TRUE if the displayable form of text -- tags stripped and any <br>
 * mapped to a newline -- ends in a newline, ignoring trailing horizontal
 * whitespace.  Used so the startup intro's own trailing line break is not
 * doubled up by Scarier's paragraph break before the first room.
 */
static scr_bool
run_text_ends_in_newline (const scr_char *text)
{
  scr_int length;

  std::vector<scr_char> stripped (text, text + strlen (text) + 1);
  pf_strip_tags_for_hints (stripped.data ());

  length = strlen (stripped.data ());
  while (length > 0
         && (stripped[length - 1] == ' ' || stripped[length - 1] == '\t'))
    length--;
  return (length > 0 && stripped[length - 1] == '\n');
}


/*
 * run_prompt_restore()
 *
 * Helper for the game-start name and gender prompts.  If the player types
 * "restore" (or "load") at one of these prompts, initiate a restore, exactly
 * as the equivalent game command would.  On a successful restore this unwinds
 * back into the interpreter loop (via run_loop_halt) and never returns; on a
 * failed or cancelled
 * restore it returns TRUE so the caller re-prompts (rather than treating the
 * typed word as an answer).  Returns FALSE when the reply is not a restore.
 */
static scr_bool
run_prompt_restore (scr_gameref_t game, const scr_char *reply)
{
  const scr_char *string;

  /* Skip leading whitespace. */
  for (string = reply; *string == ' ' || *string == '\t'; string++)
    ;

  if (scr_strcasecmp (string, "restore") == 0
      || scr_strcasecmp (string, "load") == 0)
    {
      run_restore_prompted (game);
      return TRUE;
    }

  return FALSE;
}


/*
 * run_prompt_player_name()
 *
 * When a game's "prompt for player name" option is set, the Runner asks the
 * player to type a name at game start (InputBox "Please enter your name:") and
 * uses it for the player throughout (%player% substitutions); an empty answer
 * becomes "Anonymous".  Scarier parsed but never honoured the option, so the
 * name stayed at its authored default (often blank -> "Player").  Ask for it
 * here, mirroring the Runner.  Like the gender choice, the answer is stored in
 * the session-persistent property bundle.
 *
 * The two Runners that have the prompt disagree (read 2026-09-13).  run400
 * (Form1 46EA89) asks whenever the option is set, seeds the box with the
 * current name, and turns an empty answer into "Anonymous" (46EAEF).  run390
 * (4416B8) asks only while the name is still EMPTY -- an authored name
 * suppresses the question -- and loops back to the InputBox (44170F) until
 * the answer is not empty; it has no "Anonymous" fallback here.  run380 and
 * run370 have no such prompt, and their TAF schema defaults PromptName off.
 */
static void
run_prompt_player_name (scr_gameref_t game, scr_int resume)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_bool is_400 = run_get_version (bundle) >= TAF_VERSION_400;
  scr_vartype_t vt_key[2];
  scr_char buffer[LINE_BUFFER_SIZE];
  const scr_char *name;

  /* Answered before the autosave being resumed was taken; the name came
     back with the session state. */
  if (resume > RUN_STARTUP_NAME)
    return;
  if (!prop_get_global_boolean (bundle, "PromptName"))
    return;
  if (!is_400 && !scr_strempty (prop_get_global_string (bundle, "PlayerName")))
    return;

  for (;;)
    {
      /* Resuming here, the question is already on screen. */
      if (resume == RUN_STARTUP_NAME)
        resume = 0;
      else
        {
          pf_buffer_string (filter, "Please enter your name: ");
          pf_flush (filter, vars, bundle);
        }

      run_startup_stage = RUN_STARTUP_NAME;
      if_read_line (buffer, sizeof (buffer));      /* Trailing newline stripped. */
      run_startup_stage = 0;

      /* "restore"/"load" initiates a restore instead of naming the player. */
      if (run_prompt_restore (game, buffer))
        continue;

      /* Skip leading whitespace; a blank answer becomes "Anonymous" at 4.0
       * and is asked again below it. */
      for (name = buffer; *name == ' ' || *name == '\t'; name++)
        ;
      if (*name == NUL)
        {
          if (!is_400)
            continue;
          name = "Anonymous";
        }
      break;
    }

  vt_key[0].string = "Globals";
  vt_key[1].string = "PlayerName";
  prop_put_string (bundle, "S<-ss", name, vt_key);
}


/*
 * run_prompt_player_gender()
 *
 * Adrift stores the player's gender as Male, Female, or Unknown.  When it is
 * Unknown, the Runner shows a "Please choose player gender" dialog at game
 * start and stores the answer; tasks and restrictions then test it (for
 * example, "the Player is Male").  Without this choice such a restriction can
 * never pass, which can render a game unwinnable (e.g. The Secret of the Lost
 * World gates the castle on it).  Prompt for the choice and record it in the
 * globals, mirroring the Runner.  The value lives in the (session-persistent)
 * property bundle, so it survives save/restore/undo within a session, and a
 * fresh load re-asks -- exactly as the Runner behaves.
 *
 * The stored value is ADRIFT's own gender enumeration -- Male 0, Female 1,
 * Unknown/Neuter 2, the NPC_MALE/NPC_FEMALE/NPC_NEUTER of scprotos.h -- because
 * that is what a type-3 var2=7 restriction compares against (screstrs.cpp case
 * 7 does a bare `gender == var3`).  Recording male as 1 and female as 0, as
 * this used to, silently ran every gender-gated task on the opposite branch:
 * "Provenance" dressed a male player in a house dress, and "The Secret of the
 * Lost World" answered "female" but took the male path (ring to the princess).
 */
static void
run_prompt_player_gender (scr_gameref_t game, scr_int resume)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[2];
  scr_int gender;

  gender = prop_get_global_integer (bundle, "PlayerGender");

  /* Only an Unknown (2) gender needs a choice; Male (0)/Female (1) are set. */
  if (gender != NPC_NEUTER)
    return;

  for (;;)
    {
      scr_char buffer[LINE_BUFFER_SIZE];
      const scr_char *reply;

      /* Resuming here, the question is already on screen. */
      if (resume == RUN_STARTUP_GENDER)
        resume = 0;
      else
        {
          pf_buffer_string (filter, "Please choose the player's gender"
                                    " (male or female): ");
          pf_flush (filter, vars, bundle);
        }

      run_startup_stage = RUN_STARTUP_GENDER;
      if_read_line (buffer, sizeof (buffer));
      run_startup_stage = 0;

      /* "restore"/"load" initiates a restore instead of choosing a gender. */
      if (run_prompt_restore (game, buffer))
        continue;

      for (reply = buffer; *reply == ' ' || *reply == '\t'; reply++)
        ;
      if (*reply == 'm' || *reply == 'M')
        {
          gender = NPC_MALE;
          break;
        }
      if (*reply == 'f' || *reply == 'F')
        {
          gender = NPC_FEMALE;
          break;
        }
      pf_buffer_string (filter, "Please answer \"male\" or \"female\".\n");
    }

  vt_key[0].string = "Globals";
  vt_key[1].string = "PlayerGender";
  prop_put_integer (bundle, "I<-ss", gender, vt_key);
}


/*
 * run_set_end_keyprompt()
 * run_get_end_keyprompt()
 *
 * Host control of the Runner's end-of-session prompt, "[Press any key to end]"
 * (off by default; see task_print_end_keyprompt()).  A host that ends a
 * completed game by blocking on a keypress, as the Windows Runner does, turns
 * it on; one that offers its own RESTART/UNDO/QUIT choices instead leaves it
 * off rather than print a prompt no key can answer.
 */
static scr_bool run_end_keyprompt = FALSE;

void
run_set_end_keyprompt (scr_bool flag)
{
  run_end_keyprompt = flag;
}

scr_bool
run_get_end_keyprompt (void)
{
  return run_end_keyprompt;
}


/*
 * run_main_intro()
 * run_main_startup()
 * run_main_turn_input()
 * run_main_turn_tick()
 * run_main_turn_end()
 * run_main_shutdown()
 *
 * The stages of run_main_loop(), in the order it runs them.  Startup is
 * everything a fresh game does before its first prompt, the intro being the
 * part of it an autorestore to a startup prompt skips; a turn is input, the
 * tick the input earned, and the end-of-turn output; shutdown is what follows
 * the loop however it was left.
 */
static void
run_main_intro (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];
  const scr_char *gamename, *startuptext;

  /* Initial clear screen. */
  pf_buffer_tag (filter, SCR_TAG_CLS);

  /*
   * Print the game name.  The Runner gives this line a look of its own,
   * not the plain body style: one step larger than normal text, in the
   * secondary ("command") colour -- the same red that <c> spans and the
   * player's own typing come out in.  Measured off a 3.90 Runner shot of
   * rich_text_390.taf: the title's ascenders run 13px against normal
   * text's 11 (12pt -> 14pt), with the stroke weight of normal text, not
   * of bold.
   *
   * Emit it as markup rather than as a port-side special case, so it
   * costs the ports nothing: the Glk port already maps a 14pt font to
   * style_Subheader and <c> to the input colour, and the ANSI port
   * discards both tags, leaving headless output unchanged.
   */
  gamename = prop_get_global_string (bundle, "GameName");
  pf_buffer_string (filter, "<font size=14><c>");
  pf_buffer_string (filter, gamename);
  pf_buffer_string (filter, "</c></font>");
  pf_buffer_character (filter, '\n');

  /*
   * Print the game header.  Adrift StartupText conventionally ends with a
   * <br> tag to set off the intro from the first room.  Scarier supplies its
   * own paragraph break below (the forced newline here plus the leading
   * newline from lib_cmd_look()), so adding a terminator when the text
   * already ends in a line break leaves the first room preceded by two
   * blank lines.  The Adrift Runner shows just one; only add the
   * terminator when the displayed text doesn't already end in a newline.
   */
  vt_key[0].string = "Header";
  vt_key[1].string = "StartupText";
  startuptext = prop_get_string (bundle, "S<-ss", vt_key);
  pf_buffer_string (filter, startuptext);
  if (!run_text_ends_in_newline (startuptext))
    pf_buffer_character (filter, '\n');

  /*
   * Alignment is a local of the Runner's display routine, so it starts out
   * left on every call and no <center> outlives the one string it was
   * opened in.  Scarier instead buffers a whole turn's worth of strings and
   * hands the lot to the port as one stream, so a title page that opens
   * <center> and never closes it -- "Cut the Red Wire! No, the Blue Wire!"
   * for one -- would carry on centering the first room description, which
   * the Runner displays in a separate call.  Close the intro's alignment
   * here, at that call boundary.  Games with balanced tags see nothing:
   * the tag lands at the start of a line, where the Glk port breaks no
   * paragraph because the alignment doesn't change and the ANSI port
   * breaks none because there is nothing buffered on the line.
   */
  pf_buffer_tag (filter, SCR_TAG_ENDCENTER);

  /*
   * Bold, italics and underline are locals of that routine as well
   * (run390 45BE84-45BF04), so an intro that leaves one open -- Plains
   * of Fantasy ends on a title in italics closed with a second <i> --
   * does not set the first room description in it either.
   */
  pf_buffer_tag (filter, SCR_TAG_ENDBOLD);
  pf_buffer_tag (filter, SCR_TAG_ENDITALICS);
  pf_buffer_tag (filter, SCR_TAG_ENDUNDERLINE);
}

static void
run_main_startup (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];
  scr_bool disp_first_room;
  const scr_int resume = run_startup_resume;

  run_startup_resume = 0;

  /* An autorestore to one of the startup prompts below picks up at that
     prompt: everything before it is in the restored transcript. */
  if (resume == 0)
    run_main_intro (game);

  /* If the game asks, prompt for the player's name, then (if Unknown) the
   * player's gender -- both at game start, like the Runner. */
  run_prompt_player_name (game, resume);
  run_prompt_player_gender (game, resume);

  /*
   * Start the events that start immediately, before anything is
   * described: both Runners do this during load, so an immediate event's
   * LookText belongs to the OPENING room description and its StartText is
   * never seen (probes EV6 / make_39_fwprobe variant "e", live
   * 2026-08-02 -- see evt_start_load_events()).
   */
  evt_start_load_events (game);

  /* If flagged, describe the initial room. */
  disp_first_room = prop_get_global_boolean (bundle, "DispFirstRoom");
  if (disp_first_room)
    lib_cmd_look (game);

  /* Handle any introductory resources. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "IntroRes";
  res_handle_resource (game, "ss", vt_key);

  /* Set initial values for NPC states. */
  npc_setup_initial (game);

  /* Roll initial battle stamina if the Battle System is enabled. */
  battle_start (game);

  /*
   * Nudge events, but NOT NPC walks: the Runner's walk handler (Sub_20_2)
   * is reachable only from the typed-command evaluator (Sub_20_62, called
   * solely by Form1.evaluate), so walks never tick before the first
   * command -- settled live 2026-08-01 in BOTH Runners (walk probe C: no
   * CharTask fires before the first prompt; Scarier used to move walking
   * NPCs, and fire their CharTask, during startup).
   *
   * The zero-length half of the load start finishes here rather than
   * above: its FinishText, TaskAffected and any restart land BELOW the
   * opening description in the real Runner.
   *
   * Both halves are 3.90+ only.  The startup tick is run390's `tstart'
   * (42E940) calling events() at 42E90B straight after viewroom, and
   * run400's tstart calling 449310 the same way; run380's events()
   * (425094) and run370's (432538) have exactly ONE caller each,
   * generaltasks, so nothing ticks before the first command in those
   * two.  Their load code (run380 448DC9, run370 440083) puts a
   * StarterType=1 event straight into RUNNING with its rolled length --
   * no StartText, the same silent start as above -- and a StarterType=2
   * event into WAITING with its rolled delay, which the first command
   * then decrements: a delay of 1 starts the event on turn 1, not at
   * load.  Measured 2026-09-04 in run380 with haunt.taf (transcript
   * runner_probes/haunt.run380.rtf): its Weather event (delay 1..1, length
   * 4, restart after delay) prints "Thunder rumbles ominously." on the
   * first command turn and cycles from there, where the startup tick had
   * put that line under the intro and every later weather line one turn
   * early.  A zero-length immediate event parks in 3.8 (its clock goes -1,
   * -2, ... past the `= 0' finish test at run380 43A474), so it is not
   * finished at load either; that half is read from the decompile, not
   * measured -- the corpus has one such event, wrecked.taf's EVENT 35,
   * whose only effect is an un-complete of a task nothing has completed
   * yet.
   */
  if (run_get_version (bundle) >= TAF_VERSION_390)
    {
      evt_finish_load_events (game);
      evt_tick_events (game);
    }

  /*
   * Notify the debugger that the game has started.  This is a chance to
   * set watchpoints to catch game startup actions.  Done before setting
   * the initial room visited as this is how the debugger differentiates
   * restarts from restore or undo back to game start.
   */
  debug_game_started (game);

  /* Note the initial room as visited. */
  gs_set_room_seen (game, gs_playerroom (game), TRUE);
}

static scr_bool
run_main_turn_input (scr_gameref_t game)
{
  scr_bool status;

  /* If waitcounter is zero, accept and try a command. */
  if (game->waitcounter == 0)
    {
      /* Not waiting, so handle a player input line. */
      run_update_status (game);
      status = run_player_input (game);

      /*
       * If waitcounter is now set, decrement it, as this turn counts as
       * one of them.
       */
      if (game->waitcounter > 0)
        game->waitcounter--;
    }
  else
    {
      /*
       * Currently "waiting"; decrement wait turns, then run a turn having
       * taken no input.
       */
      game->waitcounter--;
      status = TRUE;
    }

  return status;
}

static void
run_main_turn_tick (scr_gameref_t game, scr_bool status)
{
  /*
   * Do usual turn stuff unless either something stopped the game, or the
   * last command didn't match, or the last command did match but was
   * administrative.
   */
  if (status && game->is_admin)
    SCR_TRACE_IF ("SCR_TRACE_ADMIN", "ADMIN turn=%ld after [%s]\n",
                  game->turns, run_trace_last_input.c_str ());
  if (status && !game->is_admin)
    {
      /*
       * Increment turn counter, and clear notifications done flag.  3.8
       * and 3.9 counted this line's elements as they read them; see
       * run_player_input().
       */
      /*
       * run400 counts the turn only in the end-of-turn tick
       * (48B5B8-48B5C1), which the EndGame action's game-over byte
       * (4941AD) skips: goldbe's winning `climb down rope` leaves
       * %turns% at 31 (runner_transcripts/goldbe.txt).
       */
      if (!run_counts_line_elements (game)
          && !(prop_get_taf_version (gs_get_bundle (game))
               >= TAF_VERSION_400 && !game->is_running))
        game->turns++;
      game->has_notified = FALSE;

      if (game->is_running)
        {
          /*
           * Nudge NPCs (each walk tick followed by that NPC's battle
           * turn) then events (Runner: Sub_20_2 before Sub_20_32).
           */
          npc_tick_npcs (game);
          evt_tick_events (game);

          /*
           * Stamina recovery is not here: it is dobattle's, run per
           * line element from run_all_commands() before the library
           * verbs.  See battle_recover_line().
           */

          /* Update NPC states. */
          npc_turn_update (game);

          /* Note the current room as visited. */
          gs_set_room_seen (game, gs_playerroom (game), TRUE);

          /* Give the debugger a chance to catch watchpoints. */
          debug_turn_update (game);
        }
    }
}

static void
run_main_turn_end (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /*
   * Pre-4.0: the Runner's object-ambiguity flag, raised by the scan at
   * the top of generaltasks() and read only now, after the events have
   * ticked (run380 @4431B0), replaces the turn's whole output with its
   * "Which <term>.  <list>?" -- unless a game task claimed the line.
   */
  if (!run_co_pending_input.empty ())
    {
      if (!run_co_task_claimed)
        lib_co_ambiguity_prompt (game, run_co_pending_input.c_str ());
      run_co_pending_input.clear ();
    }

  /*
   * The last step of a `go <place>` walk has had its turn: the Runner's
   * route finder gets control back from SendKeys and prints the arrival
   * (run390 43CBB9, run380 431FF2).  A walk the game ended stops there.
   */
  if (run_goto_arrival_due && game->is_running
      && game->pending_endgame == 0)
    {
      pf_buffer_string (filter, run_goto_arrival.c_str ());
      run_finish_goto_walk ();
    }
  else if (!game->is_running || game->pending_endgame != 0)
    run_cancel_goto_walk ();

  /*
   * End of turn: if an EndGame task action armed an ending, print it now.
   * The Runner's turn driver does exactly this, testing its gameover byte
   * only after Form1.evaluate has returned (0005C681), so anything the
   * rest of the turn did to the score is already in the summary.
   */
  task_print_end_game_message (game);
}

static void
run_main_shutdown (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Catch an ending armed before the loop ever ran -- evt_start_load_events()
   * and the startup evt_tick_events() in run_main_startup() can both fire a
   * task.  Harmless when the loop already printed it; the pending ending is
   * cleared as it goes.
   */
  task_print_end_game_message (game);

  /*
   * Final status update, for games that vary it on completion, then notify
   * the debugger that the game has ended, to let it make a last watchpoint
   * scan and offer the dialog if appropriate.
   */
  run_update_status (game);
  debug_game_ended (game);

  /*
   * Final resource sync, score change notification and printfilter flush
   * on game-instigated loop exit.
   */
  res_sync_resources (game);
  run_notify_score_change (game);
  pf_flush (filter, vars, bundle);

  /*
   * Reset static variables inside run_player_input() with a call to it with
   * is_running false; this is a special case.
   */
  assert (!game->is_running);
  run_player_input (game);
}


/*
 * run_main_loop()
 *
 * Main interpreter loop.
 */
static void
run_main_loop (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

#ifdef SCARIER_DUMP_TOOLS
  if (run_census_requested (game))
    exit (EXIT_SUCCESS);
#endif

  /*
   * This may not be the very first time this game has been used, for example
   * saving a game right at the start, or undo-ing back to the start through
   * memos.  Caught by looking to see if the player room is marked as seen.
   */
  if (!gs_room_seen (game, gs_playerroom (game)))
    run_main_startup (game);
  else
    {
      /* Notify the debugger that the game has restarted. */
      debug_game_started (game);
    }

  /*
   * Game loop, exits either when a command parser handler sets the game
   * running flag to FALSE, or by call to run_quit().
   */
  while (game->is_running)
    {
      scr_bool status;

      /*
       * Synchronize any resources in use; do this before flushing so that any
       * appropriate graphics/sound appear before waits or waitkey tag delays
       * invoked by flushing the printfilter.  Also, print any score change
       * notifications.
       */
      res_sync_resources (game);
      run_notify_score_change (game);

      /*
       * Flush printfilter of any accumulated output, and clear any prior
       * notion of administrative commands from input.
       */
      pf_flush (filter, vars, bundle);
      game->is_admin = FALSE;

      status = run_main_turn_input (game);
      run_main_turn_tick (game, status);
      run_main_turn_end (game);
    }

  run_main_shutdown (game);
}


/*
 * run_runner_resource_draws()
 * run_runner_load_draws()
 *
 * Runner-compatible RNG mode (SCR_RNG=xoshiro) only: consume the words run400
 * draws while it loads a game, in its order, so that a Scarier stream seeded
 * like the Runner's (vbrng.dll, VBRNG=xoshiro) stays aligned draw for draw
 * from the first turn.  Every site is in openadv (mdlSpreadTheLoad 49347C):
 *
 *   48ED52  the decoded .taf's temp file, `\_<Int(Rnd * 10000)>.tmp`;
 *   48F48E  the player's starting stamina, Battle System games only,
 *           read with the header: `Int(Rnd * (hi - lo)) + lo`;
 *   491628  each StarterType 1 event's length, `Int(Rnd*(T2-T1))+T1+1`,
 *   491678  each StarterType 2 event's delay, `Int(Rnd*(End-Start))+Start`,
 *           in event order -- StarterType 3 rolls nothing;
 *   4920B1  each NPC's starting stamina, Battle System games only;
 *   454874  one `Int(Rnd * 100000)` per resource the .taf carries data for
 *           (name set, length > 0 -- a back-reference or an external file
 *           rolls nothing), naming its temp file.  These are throwaways, so
 *           only their count matters, but the Runner's order is kept:
 *           IntroRes, WinRes, then rooms (Res, then per alternate Res1 and
 *           Res2), objects (Res1, Res2), tasks (Res), events (Res 0-4),
 *           NPCs (Res 0-3), sound before graphic for each.
 *
 * Scarier makes the same rolls in its own places -- gs_create(),
 * evt_start_load_events(), battle_start() -- and in its own order; in this
 * mode those consume what is rolled here instead (see gs_event_loadtime(),
 * battle_preroll_*()).  Called at the end of run_create() and again on
 * restart, which in the Runner is a fresh load.
 */
static void
run_runner_resource_draw (scr_prop_setref_t bundle,
                          const scr_char *partial_format,
                          const scr_vartype_t vt_partial[],
                          const scr_char *embedded_key)
{
  scr_vartype_t vt_key[8], vt_rvalue;
  scr_char format[16];
  size_t length;

  length = strlen (partial_format);
  assert (length + 1 < sizeof (vt_key) / sizeof (vt_key[0]));
  memcpy (vt_key, vt_partial, length * sizeof (vt_key[0]));
  vt_key[length].string = embedded_key;
  snprintf (format, sizeof (format), "I<-%ss", partial_format);

  /* The key is absent when the game has no sound or no graphics at all. */
  if (prop_get (bundle, format, &vt_rvalue, vt_key) && vt_rvalue.integer)
    scr_randomint (0, 99999);
}

static void
run_runner_resource_draws (scr_prop_setref_t bundle,
                           const scr_char *partial_format,
                           const scr_vartype_t vt_partial[])
{
  run_runner_resource_draw (bundle, partial_format, vt_partial,
                            "SoundEmbedded");
  run_runner_resource_draw (bundle, partial_format, vt_partial,
                            "GraphicEmbedded");
}

/*
 * run_runner_event_draws()
 *
 * The event starts every Runner draws while loading (run390 46616F/4661BF,
 * run400 491628/491678): for each event with a random starter, one DRAW
 * between Time1 and Time2 (StarterType 1, the load time) or between
 * StartTime and EndTime (StarterType 2, the start time).
 */
static void
run_runner_event_draws (scr_gameref_t game,
                        scr_int (*draw) (scr_int lo, scr_int hi))
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int index_;

  GS_FOR_EACH_EVENT (game, index_)
    {
      scr_int startertype, lo, hi;

      vt_key[0].string = "Events";
      vt_key[1].integer = index_;
      vt_key[2].string = "StarterType";
      startertype = prop_get_integer (bundle, "I<-sis", vt_key);

      switch (startertype)
        {
        case 1:
          vt_key[2].string = "Time1";
          lo = prop_get_integer (bundle, "I<-sis", vt_key);
          vt_key[2].string = "Time2";
          hi = prop_get_integer (bundle, "I<-sis", vt_key);
          gs_set_event_loadtime (game, index_, draw (lo, hi));
          break;

        case 2:
          vt_key[2].string = "StartTime";
          lo = prop_get_integer (bundle, "I<-sis", vt_key);
          vt_key[2].string = "EndTime";
          hi = prop_get_integer (bundle, "I<-sis", vt_key);
          gs_set_event_time (game, index_, draw (lo, hi));
          break;

        default:
          break;
        }
    }
}

/* Int(Rnd * (hi - lo)) + lo, from the codec's LCG; see below. */
static scr_int
run_legacy_event_draw (scr_int lo, scr_int hi)
{
  return lo + (scr_int) floor (scr_vb_rnd () * (hi - lo));
}

static void
run_runner_legacy_load_draws (scr_gameref_t game)
{
  /*
   * A 3.9 game draws NOTHING from the game stream while loading: run390
   * seeds with Timer only at the end of openadv (467013), after the codec's
   * `Randomize 1976`, so its event starts (46616F/4661BF, the same formulas
   * as run400's) and Speed 1 attack counters (466A43) come from the codec's
   * own LCG, continued from the last file byte -- deterministic, and replayed
   * here from the decoder's state (taf_runtime_rnd).  Events are read before
   * NPCs.  No temp-file draw, no stamina draws, no embedded resources.
   */
  taf_runtime_rnd_reset ();
  run_runner_event_draws (game, run_legacy_event_draw);
  battle_preroll_legacy (game);
}

static void
run_runner_load_draws (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[6], vt_rvalue;
  scr_int index_, count, sub;

  if (!scr_is_runner_random ())
    return;

  vt_key[0].string = "Version";
  if (prop_get (bundle, "I<-s", &vt_rvalue, vt_key)
      && vt_rvalue.integer < TAF_VERSION_400)
    {
      run_runner_legacy_load_draws (game);
      return;
    }

  /* 48ED52: the temp file name. */
  scr_randomint (0, 9999);

  /* 48F48E: the player's stamina. */
  battle_preroll_player_stamina (game);

  /* 491628 / 491678: event starts. */
  run_runner_event_draws (game, scr_randomint_exclusive);

  /* 4920B1: NPC stamina. */
  battle_preroll_npc_stamina (game);

  /* 454874: temp files for the embedded resources, 4.0 games only. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "Embedded";
  if (!prop_get_boolean (bundle, "B<-ss", vt_key))
    return;

  vt_key[1].string = "IntroRes";
  run_runner_resource_draws (bundle, "ss", vt_key);
  vt_key[1].string = "WinRes";
  run_runner_resource_draws (bundle, "ss", vt_key);

  vt_key[0].string = "Rooms";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      scr_int alts;

      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      run_runner_resource_draws (bundle, "sis", vt_key);

      vt_key[2].string = "Alts";
      alts = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (sub = 0; sub < alts; sub++)
        {
          /* Both sounds, then both graphics (492B4B..492CA0). */
          vt_key[3].integer = sub;
          vt_key[4].string = "Res1";
          run_runner_resource_draw (bundle, "sisis", vt_key, "SoundEmbedded");
          vt_key[4].string = "Res2";
          run_runner_resource_draw (bundle, "sisis", vt_key, "SoundEmbedded");
          vt_key[4].string = "Res1";
          run_runner_resource_draw (bundle, "sisis", vt_key,
                                    "GraphicEmbedded");
          vt_key[4].string = "Res2";
          run_runner_resource_draw (bundle, "sisis", vt_key,
                                    "GraphicEmbedded");
        }
    }

  vt_key[0].string = "Objects";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res1";
      run_runner_resource_draws (bundle, "sis", vt_key);
      vt_key[2].string = "Res2";
      run_runner_resource_draws (bundle, "sis", vt_key);
    }

  vt_key[0].string = "Tasks";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      run_runner_resource_draws (bundle, "sis", vt_key);
    }

  vt_key[0].string = "Events";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      for (sub = 0; sub < 5; sub++)
        {
          vt_key[3].integer = sub;
          run_runner_resource_draws (bundle, "sisi", vt_key);
        }
    }

  vt_key[0].string = "NPCs";
  count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < count; index_++)
    {
      vt_key[1].integer = index_;
      vt_key[2].string = "Res";
      for (sub = 0; sub < 4; sub++)
        {
          vt_key[3].integer = sub;
          run_runner_resource_draws (bundle, "sisi", vt_key);
        }
    }
}

/*
 * run_destroy_bundle()
 *
 * Destroy a game's properties bundle, first dropping the object, variable,
 * library and restriction caches built from it, so a later bundle allocated
 * at the same address can't inherit them.  (Those caches are keyed by bundle
 * rather than by game because the undo and temporary game copies share their
 * game's bundle.)
 */
static void
run_destroy_bundle (scr_prop_setref_t bundle)
{
  obj_forget_bundle (bundle);
  var_forget_bundle (bundle);
  lib_forget_bundle (bundle);
  restr_forget_bundle (bundle);
  prop_destroy (bundle);
}

/*
 * run_create()
 *
 * Create a game context from a callback.
 */
scr_gameref_t
run_create (scr_read_callbackref_t callback, void *opaque)
{
  scr_tafref_t taf;
  scr_prop_setref_t bundle = nullptr;
  scr_var_setref_t vars = nullptr, temporary_vars = nullptr,
                   undo_vars = nullptr;
  scr_filterref_t filter = nullptr;
  scr_gameref_t game = nullptr, temporary_game = nullptr, undo_game = nullptr;
  assert (callback);

  /* Create a new TAF using the callback; return NULL if this fails. */
  taf = taf_create (callback, opaque);
  if (!taf)
    return nullptr;
  else if (if_get_trace_flag (SCR_DUMP_TAF))
    taf_debug_dump (taf);
  restr_cache_reset ();
  uip_state_reset ();

  /*
   * Any construction step below can throw (scr_fatal on a corrupt game);
   * reclaim whatever has been built so far on that path -- mirroring
   * run_destroy()'s teardown -- then let the throw carry on to the interface
   * boundary, which reports the game as unusable.
   */
  try
    {
      /* Create a properties bundle, and parse the TAF data into it. */
      bundle = prop_create (taf);
      if (!bundle)
        {
          scr_error ("run_create: error parsing game data\n");
          taf_destroy (taf);
          return nullptr;
        }
      else if (if_get_trace_flag (SCR_DUMP_PROPERTIES))
        prop_debug_dump (bundle);

      /* Try to set an interpreter locale from the properties bundle. */
      loc_detect_game_locale (bundle);
      if (if_get_trace_flag (SCR_DUMP_LOCALE_TABLES))
        loc_debug_dump ();

      /* Create a set of variables from the bundle. */
      vars = var_create (bundle);
      if (if_get_trace_flag (SCR_DUMP_VARIABLES))
        var_debug_dump (vars);

      /* Create a printfilter for the game. */
      filter = pf_create ();

      /*
       * Create an initial game state, and register it with variables.  Also,
       * create undo buffers, and initialize them in the same way.
       */
      game = gs_create (vars, bundle, filter);
      var_register_game (vars, game);

      temporary_vars = var_create (bundle);
      temporary_game = gs_create (temporary_vars, bundle, filter);
      var_register_game (temporary_vars, temporary_game);

      undo_vars = var_create (bundle);
      undo_game = gs_create (undo_vars, bundle, filter);
      var_register_game (undo_vars, undo_game);

      /* Add the undo buffers and memos to the game, and return it. */
      game->temporary = temporary_game;
      game->undo = undo_game;
      game->memento = memo_create ();

      /* Replay the Runner's load-time draws (Runner-compatible RNG only). */
      run_runner_load_draws (game);
      return game;
    }
  catch (...)
    {
      if (undo_game)
        gs_destroy (undo_game);
      if (undo_vars)
        var_destroy (undo_vars);
      if (temporary_game)
        gs_destroy (temporary_game);
      if (temporary_vars)
        var_destroy (temporary_vars);
      if (game)
        gs_destroy (game);
      if (filter)
        pf_destroy (filter);
      if (vars)
        var_destroy (vars);
      if (bundle)
        run_destroy_bundle (bundle);  /* also destroys the taf it adopted */
      else
        taf_destroy (taf);
      throw;
    }
}


/*
 * run_get_restart_count()
 *
 * How many times a game has been restarted in this process.
 *
 * A restart never comes back through run_interpret's caller: RESTART typed at
 * the prompt unwinds into the loop below, which replays the opening without
 * returning, and even the front end's own scr_restart_game() leaves nothing
 * behind to see.  So a front end with screen furniture to reconsider when the
 * game goes back to the beginning -- the Glk port's map pane -- watches this
 * for a change instead.
 *
 * Deliberately not part of the game state: a restart replaces that wholesale,
 * and a restore or undo would carry an older count back.
 */
static scr_int run_restart_count = 0;

scr_int
run_get_restart_count (void)
{
  return run_restart_count;
}


/*
 * run_restart_handler()
 *
 * Return a game context to initial states to restart a game.
 */
static void
run_restart_handler (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_gameref_t new_game;
  scr_var_setref_t new_vars;

  /*
   * Create a fresh set of variables from the current game properties,
   * then a new game using these variables and existing properties and
   * printfilter.
   */
  new_vars = var_create (bundle);
  new_game = gs_create (new_vars, bundle, filter);
  var_register_game (new_vars, new_game);

  /*
   * Overwrite the dynamic parts of the current game with the new one.
   */
  new_game->temporary = game->temporary;
  new_game->undo = game->undo;
  gs_copy (game, new_game);

  /* A restart is a fresh load in the Runner: replay its load-time draws. */
  run_runner_load_draws (game);
  restr_cache_reset ();

  /* Destroy invalid game status strings. */
  game->current_room_name.reset ();
  game->status_line.reset ();

  /*
   * Now it's safely copied, destroy the temporary new game, and its
   * associated variable set.
   */
  gs_destroy (new_game);
  var_destroy (new_vars);

  /* Reset resources handling. */
  res_cancel_resources (game);

  /* The one place a restart can be counted; see run_get_restart_count(). */
  run_restart_count++;
}


/*
 * run_restore_handler()
 *
 * Adjust a game context for continuation after restoring a game.
 */
static void
run_restore_handler (scr_gameref_t game)
{
  /* Invalidate the undo buffer. */
  game->undo_available = FALSE;

  /*
   * Resources: a saved game does not record which resources were active,
   * so they cannot be re-offered on restore.  Cancelling everything would
   * be too broad; stopping sounds (in case one is looping) is the one thing
   * done.
   */
  game->stop_sound = TRUE;
}


/*
 * run_quit_handler()
 *
 * Tidy up printfilter and input statics on game quit.
 */
static void
run_quit_handler (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* Flush printfilter and notifications of any dangling output. */
  run_notify_score_change (game);
  pf_flush (filter, vars, bundle);

  /* Cancel any active resources. */
  res_cancel_resources (game);

  /*
   * Make the special call to reset all of the static variables inside
   * run_player_input().
   */
  assert (!game->is_running);
  run_player_input (game);
}


/*
 * run_interpret()
 *
 * Intepret the game in a game context.
 */
void
run_interpret (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Verify the game is not already running, and is runnable. */
  if (game->is_running)
    {
      scr_error ("run_interpret: game is already running\n");
      return;
    }
  if (game->has_completed)
    {
      scr_error ("run_interpret: game has already completed\n");
      return;
    }

  /* Refuse to run a game with no rooms. */
  if (gs_room_count (game) == 0)
    {
      scr_error ("run_interpret: game contains no rooms\n");
      return;
    }

  /* Run the main interpreter loop until no more restarts. */
  game->is_running = TRUE;
  do
    {
      /* Run the game until some form of halt is requested. */
      try
        {
          run_main_loop (game);
        }
      catch (const run_loop_halt &)
        {
          /*
           * run_quit / run_restart / run_restore / run_undo unwound a running
           * game out of the main loop; the do_restart/do_restore flags below
           * decide whether we loop again or stop (matching the old longjmp).
           */
        }

      /*
       * If the halt was a restart or restore, cancel the request, handle
       * restart or restore game adjustments, and set the game running
       * again.
       */
      if (game->do_restart)
        {
          game->do_restart = FALSE;
          run_restart_handler (game);
          game->is_running = TRUE;
        }

      if (game->do_restore)
        {
          game->do_restore = FALSE;
          run_restore_handler (game);
          game->is_running = TRUE;
        }
    }
  while (game->is_running);

  /* Tidy up the printfilter and input statics. */
  run_quit_handler (game);
}


/*
 * run_destroy()
 *
 * Destroy a game context, and free all resources.
 */
void
run_destroy (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Can't destroy the context of a running game. */
  if (game->is_running)
    {
      scr_error ("run_destroy: game is running, stop it first\n");
      return;
    }

  /*
   * Cancel any game state debugger -- this frees its resources.  Only the
   * primary game may have acquired a debugger.
   */
  debug_set_enabled (game, FALSE);
  assert (!debug_get_enabled (game->temporary));
  assert (!debug_get_enabled (game->undo));

  /*
   * Destroy the game state, variables, properties bundle, memos, undo
   * buffers and their variables, and filter.  The bundle and printfilter
   * are shared by the main game, the undo game, and the temporary game, so
   * destroy these only once!  The main game has a memento, but it is not
   * visible to these other two games, neither of which have one.
   */
  assert (gs_get_bundle (game->temporary) == gs_get_bundle (game));
  assert (gs_get_filter (game->temporary) == gs_get_filter (game));
  assert (gs_get_vars (game->temporary) != gs_get_vars (game));
  assert (!gs_get_memento (game->temporary));
  var_destroy (gs_get_vars (game->temporary));
  gs_destroy (game->temporary);

  assert (gs_get_bundle (game->undo) == gs_get_bundle (game));
  assert (gs_get_filter (game->undo) == gs_get_filter (game));
  assert (gs_get_vars (game->undo) != gs_get_vars (game));
  assert (!gs_get_memento (game->undo));
  var_destroy (gs_get_vars (game->undo));
  gs_destroy (game->undo);

  run_destroy_bundle (gs_get_bundle (game));
  pf_destroy (gs_get_filter (game));
  var_destroy (gs_get_vars (game));
  memo_destroy (gs_get_memento (game));

  gs_destroy (game);
}


/*
 * run_quit()
 *
 * Quits a running game.  This function throws run_loop_halt to unwind back to
 * run_interpret as if run_main_loop() returned, and so never returns to its
 * caller.
 */
void
run_quit (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Disallow quitting a non-running game. */
  if (!game->is_running)
    {
      scr_error ("run_quit: game is not running\n");
      return;
    }

  /* Exit the main loop by unwinding back to run_interpret. */
  game->is_running = FALSE;
  throw run_loop_halt ();
}


/*
 * run_restart()
 *
 * Restarts either a running or a stopped game.  For running games, this
 * function throws run_loop_halt to unwind back to run_interpret as if
 * run_main_loop() returned, and so never returns to its caller.  For stopped
 * games, it returns.
 */
void
run_restart (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /*
   * If the game is running, stop it, request a restart, and exit the main
   * loop by throwing run_loop_halt.
   */
  if (game->is_running)
    {
      game->is_running = FALSE;
      game->do_restart = TRUE;
      throw run_loop_halt ();
    }

  /* Restart locally, and ensure that the game remains stopped. */
  run_restart_handler (game);
  game->is_running = FALSE;
}


/*
 * run_save()
 * run_save_to_file()
 * run_save_prompted()
 *
 * Saves either a running or a stopped game.
 *
 * run_save_to_file() is for a save the player keeps, and writes a pre-4.0
 * game's own save format so the original Runner can read it back; run_save()
 * always writes the full 4.0 layout, for snapshots that have to survive a
 * round trip losslessly (the Spatterlight autosave).  See ser_save_game().
 */
void
run_save (scr_gameref_t game, scr_write_callbackref_t callback, void *opaque)
{
  assert (gs_is_game_valid (game));
  assert (callback);

  ser_save_game (game, callback, opaque);
}

void
run_save_to_file (scr_gameref_t game,
                  scr_write_callbackref_t callback, void *opaque)
{
  assert (gs_is_game_valid (game));
  assert (callback);

  ser_save_game_to_file (game, callback, opaque);
}

scr_bool
run_save_prompted (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return ser_save_game_prompted (game);
}


/*
 * run_restore_common()
 * run_restore()
 * run_restore_prompted()
 *
 * Restores either a running or a stopped game.  For running games, on
 * successful restore, these functions throw run_loop_halt to unwind back to
 * run_interpret as if run_main_loop() returned, and so never return to their
 * caller.  On failed
 * restore, and for stopped games, they will return, with TRUE if successful,
 * FALSE if restore failed.
 */
static scr_bool
run_restore_common (scr_gameref_t game,
                    scr_read_callbackref_t callback, void *opaque)
{
  scr_bool is_running, status;

  /*
   * Save the game running flag, and call the restore appropriate for the
   * caller.  The indication of a call from run_restore_prompted() is a
   * callback of NULL; callback cannot be NULL for run_restore() calls.
   */
  is_running = game->is_running;
  status = callback ? ser_load_game (game, callback, opaque)
                    : ser_load_game_prompted (game);
  if (status)
    {
      /* Loading a game clears is_running -- restore it here. */
      game->is_running = is_running;

      /*
       * If the game is (was) running, set flags so that the interpreter
       * loop cycles, and exit the main loop by throwing run_loop_halt.
       */
      if (game->is_running)
        {
          game->is_running = FALSE;
          game->do_restore = TRUE;
          throw run_loop_halt ();
        }
    }

  /* Return TRUE on successful restore of a stopped game, FALSE on error. */
  return status;
}

scr_bool
run_restore (scr_gameref_t game, scr_read_callbackref_t callback, void *opaque)
{
  assert (gs_is_game_valid (game));
  assert (callback);

  return run_restore_common (game, callback, opaque);
}

scr_bool
run_restore_prompted (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return run_restore_common (game, nullptr, nullptr);
}


/*
 * run_undo()
 *
 * Undo a turn in either a running or a stopped game.  Returns TRUE on
 * successful undo, FALSE if no undo buffer is available.
 */
scr_bool
run_undo (scr_gameref_t game)
{
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_bool is_running;
  assert (gs_is_game_valid (game));

  /* Save the game's running state, so we can restore it later. */
  is_running = game->is_running;

  /* If there's an undo buffer available, restore it. */
  if (game->undo_available)
    {
      /* Restore the undo buffer, and then restore running flag. */
      gs_copy (game, game->undo);
      game->undo_available = FALSE;
      game->is_running = is_running;

      /* Location may have changed; update status. */
      run_update_status (game);

      /* Bring resources into line with the revised game. */
      res_sync_resources (game);
      return TRUE;
    }

  /*
   * If there is no undo buffer, try to restore one saved previously in a
   * memo.  Handle as if restoring from a file.
   */
  if (memo_load_game (memento, game, nullptr))
    {
      /* Loading a game clears is_running -- restore it here. */
      game->is_running = is_running;

      /*
       * If the game is (was) running, set flags so that the interpreter
       * loop cycles, and exit the main loop by throwing run_loop_halt.
       */
      if (game->is_running)
        {
          game->is_running = FALSE;
          game->do_restore = TRUE;
          throw run_loop_halt ();
        }

      /* Game undo on non-running game accomplished with memos. */
      return TRUE;
    }

  /* No undo buffer and no memos available. */
  return FALSE;
}


/*
 * run_is_running()
 *
 * Query the game running state.
 */
scr_bool
run_is_running (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return game->is_running;
}


/*
 * run_has_completed()
 *
 * Query the game completion state.  Completed games cannot be resumed,
 * since they've run the exit task and thus have nowhere to go.
 */
scr_bool
run_has_completed (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  return game->has_completed;
}


/*
 * run_is_undo_available()
 *
 * Query the game turn undo buffer and memo availability.
 */
scr_bool
run_is_undo_available (scr_gameref_t game)
{
  const scr_memo_setref_t memento = gs_get_memento (game);
  assert (gs_is_game_valid (game));

  return game->undo_available || memo_is_load_available (memento);
}


/*
 * run_get_attributes()
 * run_set_attributes()
 *
 * Get and set selected game attributes.
 */
void
run_get_attributes (scr_gameref_t game,
                    const scr_char **game_name, const scr_char **game_author,
                    const scr_char **game_compile_date,
                    scr_int *turns, scr_int *score, scr_int *max_score,
                    const scr_char **current_room_name,
                    const scr_char **status_line, const scr_char **preferred_font,
                    scr_bool *bold_room_names, scr_bool *verbose,
                    scr_bool *notify_score_change)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[2];
  assert (gs_is_game_valid (game));

  /* Return the game name, author, and compile date if requested. */
  if (game_name)
    {
      if (!game->title)
        {
          const scr_char *gamename;
          scr_char *filtered;

          gamename = prop_get_global_string (bundle, "GameName");

          filtered = pf_filter_for_info (gamename, vars);
          pf_strip_tags (filtered);
          game->title.reset (filtered);
        }
      *game_name = game->title.get ();
    }
  if (game_author)
    {
      if (!game->author)
        {
          const scr_char *gameauthor;
          scr_char *filtered;

          gameauthor = prop_get_global_string (bundle, "GameAuthor");

          filtered = pf_filter_for_info (gameauthor, vars);
          pf_strip_tags (filtered);
          game->author.reset (filtered);
        }
      *game_author = game->author.get ();
    }
  if (game_compile_date)
    {
      vt_key[0].string = "CompileDate";
      *game_compile_date = prop_get_string (bundle, "S<-s", vt_key);
    }

  /* Return the current room name and status line if requested. */
  if (current_room_name)
    *current_room_name = game->current_room_name.get ();
  if (status_line)
    *status_line = game->status_line.get ();

  /* Return any game preferred font, or NULL if none. */
  if (preferred_font)
    {
      vt_key[0].string = "CustomFont";
      if (prop_get_boolean (bundle, "B<-s", vt_key))
        {
          vt_key[0].string = "FontNameSize";
          *preferred_font = prop_get_string (bundle, "S<-s", vt_key);
        }
      else
        *preferred_font = nullptr;
    }

  /* Return any other selected game attributes. */
  if (turns)
    *turns = game->turns;
  if (score)
    *score = game->score;
  if (max_score)
    {
      *max_score = prop_get_global_integer (bundle, "MaxScore");
    }
  if (bold_room_names)
    *bold_room_names = game->bold_room_names;
  if (verbose)
    *verbose = game->verbose;
  if (notify_score_change)
    *notify_score_change = game->notify_score_change;
}

void
run_set_attributes (scr_gameref_t game,
                    scr_bool bold_room_names, scr_bool verbose,
                    scr_bool notify_score_change)
{
  assert (gs_is_game_valid (game));

  /* Set game options. */
  game->bold_room_names = bold_room_names;
  game->verbose = verbose;
  game->notify_score_change = notify_score_change;
}


/*
 * run_hint_iterate()
 *
 * Return the next hint appropriate to the game state, or the first if
 * hint is NULL.  Returns NULL if none, or no more hints.  This function
 * works with pointers to a task state rather than task indexes so that
 * the token passed in and out is a pointer, and readily made opaque to
 * the client as a void*.
 */
scr_hintref_t
run_hint_iterate (scr_gameref_t game, scr_hintref_t hint)
{
  scr_int task;
  assert (gs_is_game_valid (game));

  /*
   * Hint is a pointer to a task state; convert to a task index, adding one
   * to move on to the next task, or start at the first task if null.
   */
  if (!hint)
    task = 0;
  else
    {
      /* Convert into pointer, and range check. */
      task = hint - game->tasks.data ();
      if (task < 0 || task >= gs_task_count (game))
        {
          scr_error ("run_hint_iterate: invalid iteration hint\n");
          return nullptr;
        }

      /* Advance beyond current task. */
      task++;
    }

  /* Scan for the next runnable task that offers a hint. */
  for (; task < gs_task_count (game); task++)
    {
      if (task_can_run_task (game, task) && task_has_hints (game, task))
        break;
    }

  /* Return a pointer to the state of the task identified, or NULL. */
  return task < gs_task_count (game) ? &game->tasks[task] : nullptr;
}


/*
 * run_get_hint_common()
 * run_get_hint_question()
 * run_get_subtle_hint()
 * run_get_unsubtle_hint()
 *
 * Return the strings for a hint.  Front-ends to task functions.  Each
 * converts the hint "address" to a task index through pointer arithmetic,
 * then filters it and returns a temporary, valid only until the next hint
 * call.
 *
 * Hint strings are NULL if empty (not defined by the game).
 */
static const scr_char *
run_get_hint_common (scr_gameref_t game, scr_hintref_t hint,
                     const scr_char *(*handler) (scr_gameref_t, scr_int))
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int task;
  const scr_char *string;
  assert (gs_is_game_valid (game));

  /* Verify the caller passed in a valid hint. */
  task = hint - game->tasks.data ();
  if (task < 0 || task >= gs_task_count (game))
    {
      scr_error ("run_get_hint_common: invalid iteration hint\n");
      return nullptr;
    }
  else if (!task_has_hints (game, task))
    {
      scr_error ("run_get_hint_common: task has no hint\n");
      return nullptr;
    }

  /* Get the required game text by calling the given handler function. */
  string = handler (game, task);
  if (!scr_strempty (string))
    {
      scr_char *filtered;

      /* Filter and strip tags, note in game. */
      filtered = pf_filter (string, vars, bundle);
      pf_strip_tags_for_hints (filtered);
      game->hint_text.reset (filtered);
    }
  else
    {
      /* Hint text is empty; drop any text noted in game. */
      game->hint_text.reset ();
    }

  return game->hint_text.get ();
}

const scr_char *
run_get_hint_question (scr_gameref_t game, scr_hintref_t hint)
{
  return run_get_hint_common (game, hint, task_get_hint_question);
}

const scr_char *
run_get_subtle_hint (scr_gameref_t game, scr_hintref_t hint)
{
  return run_get_hint_common (game, hint, task_get_hint_subtle);
}

const scr_char *
run_get_unsubtle_hint (scr_gameref_t game, scr_hintref_t hint)
{
  return run_get_hint_common (game, hint, task_get_hint_unsubtle);
}

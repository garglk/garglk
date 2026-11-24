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
 * The session record: what a Spatterlight autosave carries beyond the saved
 * game, written at the prompt and read back on an autorestore.
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
#include "sessrec.h"


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
 *   - whether this turn's score change has been announced: the save is
 *     taken at the prompt, after run_notify_score_change(), and a fresh
 *     process would otherwise announce it again before that prompt;
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
  const scr_pronouns_t &pronouns = game->pronouns;

  return run_session_join ({pronouns.it_object, pronouns.it_form,
                            pronouns.him_npc, pronouns.her_npc,
                            pronouns.it_npc, pronouns.last_npc});
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

  target->pronouns.it_object = values[0];
  target->pronouns.it_form
      = values[1] >= UIP_IT_INDEFINITE && values[1] <= UIP_IT_BARE
        ? values[1] : UIP_IT_INDEFINITE;
  target->pronouns.him_npc = values[2];
  target->pronouns.her_npc = values[3];
  target->pronouns.it_npc = values[4];
  target->pronouns.last_npc = values[5];
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
               run_session_join ({game->settings.verbose,
                                  game->settings.notify_score_change,
                                  game->settings.waitturns}));
  sessrec_put (out, "notified", run_session_join ({game->turn.has_notified}));

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
  /*
   * The list the last prompt offered outlives the question: a line that
   * ties on it again gets "That is still ambiguous!", not the prompt.
   */
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
          game->settings.verbose = numbers[0] != 0;
          game->settings.notify_score_change = numbers[1] != 0;
          if (numbers[2] >= 0)
            game->settings.waitturns = numbers[2];
        }
      else if (key == "notified" && numbers.size () == 1)
        game->turn.has_notified = numbers[0] != 0;
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

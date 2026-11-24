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
 * Walking the library command tables: which rows a line may reach (the NPC,
 * with-deferred and SCARE-only row classes), the table walk itself, and the
 * standard verb, give-NPC and fallback buckets.
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
 * run_move_commands()
 *
 * Return the movement command table matching the game's compass setting.
 */
static scr_commandsref_t
run_move_commands (const scr_prop_setref_t bundle)
{
  return prop_get_global_boolean (bundle, "EightPointCompass")
         ? MOVE_COMMANDS_8 : MOVE_COMMANDS_4;
}

/*
 * run_npc_library_blocked()
 *
 * Whether the character-handler rows are skipped for this line: from 3.9
 * on, once a task has run for it.
 */
static scr_bool
run_npc_library_blocked (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return run_any_task_ran_this_command ();
}

/*
 * run_npc_row_blocked()
 *
 * Whether a library row is one of the character-handler branches that the
 * Runner skips once a task has run for this line.  See the comment at the
 * call site in run_try_command_table() for the run400 addresses.
 */
static scr_bool
run_npc_row_blocked (const scr_commands_t *command)
{
  if (command->handler == lib_cmd_attack_npcs
      || command->handler == lib_cmd_attack_npcs_with)
    return TRUE;
  if (strstr (command->command, "%character%") == nullptr)
    return FALSE;
  return command->handler != lib_cmd_give_object_npc
         && command->handler != lib_cmd_ask_npc_about
         && command->handler != lib_cmd_talk_to_npc_about
         && command->handler != lib_cmd_kiss_npc
         && command->handler != lib_cmd_status_npc
         && command->handler != lib_cmd_verb_npc;
}

/*
 * Set while the pre-4.0 verb pass runs, so that the give-to-character rows
 * wait for run_standard_give_npc_commands() below the room refusal.
 */
static scr_bool run_defer_give_npc = FALSE;

/*
 * Set while the 3.9 verb pass runs on a line run390's therest with-arm will
 * answer (lib_with_arm_390()): the characters() attack rows, which run390
 * calls below therest, wait for it -- and lose to it.  With the Battle
 * System on, those rows are dobattle's, which run390 calls above therest
 * (45F4AF): secret_of_lost_world's `hit ghost with excalibur` is a blow.
 * The rows that wait carry RUN_ROW_WITH_DEFERRED.
 */
static scr_bool run_defer_with_390 = FALSE;

/*
 * RUN_ROW_SCARE_ONLY
 * run_line_opens_with_scare_word()
 * run_game_owns_scare_word()
 *
 * Deliberate deviation's limit.  `verbose`, `brief`, `notify`, `notification`,
 * `hints`, `hist`, `redo`, `gpl`, `license`, `statusline` and `stats` are
 * SCARE's words; no Runner answers them (see the audit note in
 * STANDARD_COMMANDS).  A task is matched before the library, so one that
 * speaks keeps its line -- but one that does not claim it (restrictions that
 * fail without a message, nothing to say, already done, another room) would
 * otherwise lose it to our command, where the Runner says it does not
 * understand: `stats` in suburbanprodigy3 is that, and the census
 * (SCR_CENSUS_META, tools/scrun_census.cpp) finds 275 tasks in 28 games that
 * take `hints`, 12 games whose `statusline` we would answer at the first
 * prompt.
 *
 * So a word a game's tasks name is the game's: when any task has a command
 * that matches the line the Runner's way, in whatever room and state, the
 * SCARE rows stand aside and the line goes on as it would in the Runner.  A
 * task that would take the line with any other word in the place of ours
 * (`* on`, `*`) does not name it and does not count.  Nothing is put out of
 * reach: `glk verbose`, `glk notify`, `glk hints`, `glk license` and `glk
 * statusline` cannot be shadowed, and `history` and `!` are the Runner's
 * own.  The one-letter `g`, `i`, `q` and `z` are left alone -- conveniences
 * kept on purpose.  The rows that stand aside carry RUN_ROW_SCARE_ONLY.
 */
/* The length of the SCARE word the line opens with, or 0. */
static size_t
run_line_opens_with_scare_word (const scr_char *string)
{
  static const scr_char *const WORDS[] = {
    "verbose", "brief", "notify", "notification", "hints", "hist", "redo",
    "gpl", "license", "statusline", "stats", nullptr
  };
  const size_t length = strcspn (string, WHITESPACE);

  for (scr_int index_ = 0; WORDS[index_]; index_++)
    {
      if (strlen (WORDS[index_]) == length
          && scr_strncasecmp (string, WORDS[index_], length) == 0)
        return length;
    }
  return 0;
}

static scr_bool
run_game_owns_scare_word (scr_gameref_t game, const scr_char *string)
{
  string += strspn (string, WHITESPACE);
  const size_t length = run_line_opens_with_scare_word (string);

  if (length == 0)
    return FALSE;

  /* The same line with a word no task can have been written for. */
  const std::string other = std::string ("qzxqzx") + (string + length);
  const scr_lenient_tasks_guard strict_match (FALSE);

  return run_line_matches_task_strictly (game, string, other.c_str (), TRUE);
}

/*
 * run_try_command_table()
 *
 * Search a command table for a match to the string, returning TRUE on the
 * first matching command whose handler succeeds.
 */
scr_bool
run_try_command_table (scr_commandsref_t command,
                       scr_gameref_t game, const scr_char *string)
{
  const scr_ref_number_guard ref_number (game);
  const scr_bool npc_blocked = run_npc_library_blocked (game);
  scr_int scare_word_owned = -1;

  for (; command->command; command++)
    {
      if (run_defer_give_npc && command->handler == lib_cmd_give_object_npc)
        continue;
      if (run_defer_with_390 && (command->flags & RUN_ROW_WITH_DEFERRED))
        continue;

      /*
       * Once a game task has run for this line, most of the Runner's
       * character handler answers nothing: the who, hit/kill/kick/punch/
       * attack, get/take/pick up, talk to/speak to, ask-without-about hint,
       * where/find/locate, examine/look and take-from branches of run400's
       * Proc_19_0_480674 all test a flag that execute_task sets and the
       * input routine clears.  run390 guards the same branches with a flag
       * of its own; run370 has no such flag, and run380's rendering of the
       * test (44054B) is too ambiguous to lean on, so pre-3.9 keeps every
       * row.  Measured live on House (4.0): the silent
       * task "# attention on cathy grave vision" (`*cathy*`, once only) runs
       * on the first `get cathy` and the library then says "Take what?"
       * (runner_probes/house.run400.t93.txt,
       * runner_probes/house.run400.girl.txt); `x cathy` on that first
       * mention gets "You see no such thing."
       * (runner_probes/house.run400.t98.txt); with the task spent, both fall
       * to the NPC handlers -- "I don't think girl would appreciate being
       * handled." and her description.
       *
       * The rows that survive a task are the ones the Runner reaches by
       * another route: give is handled in the input routine at 48A98A with
       * no flag test; `ask X about Y` is re-opened by the "<player> can't
       * talk to that." buffer that generaltasks_verbs seeds whenever no
       * object took the ask (run390 tests no flag at all) -- Humbug's
       * silent scoring task `ask * hacker about * humbug` still gets the
       * hacker's reply (runner_probes/humbug.run400.b.txt); kiss is gated on
       * the buffer, not the flag; and the end-of-handler "I don't understand
       * what you want to do with" fallback asks for an empty buffer rather
       * than the flag.  (It does have one other guard, added
       * here 2026-09-07: `loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub`
       * above it takes it off a line that a task has just ended the game on
       * -- see lib_cmd_verb_npc().)
       */
      if (npc_blocked && run_npc_row_blocked (command))
        continue;

      if (uip_match (command->command, string, game))
        {
          /*
           * A SCARE word the game has a task for is the game's; see
           * run_game_owns_scare_word().  Asked once, and only of a line
           * that reaches one of those rows.
           */
          if (command->flags & RUN_ROW_SCARE_ONLY)
            {
              if (scare_word_owned < 0)
                scare_word_owned = run_game_owns_scare_word (game, string);
              if (scare_word_owned)
                continue;
            }
          if (run_command_handle (command, game))
            return TRUE;
        }
    }
  return FALSE;
}

/*
 * run_movement_succeeds()
 *
 * Return TRUE if the input is a movement command that would really move the
 * player out of the room.  Prints nothing and changes nothing -- the movement
 * handlers run under lib_set_movement_probe().  Used only by the version 3.8
 * ordering in run_all_commands().
 */
scr_bool
run_movement_succeeds (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_movement;

  lib_set_movement_probe (TRUE);
  is_movement = run_try_command_table (run_move_commands (bundle),
                                       game, string);
  lib_set_movement_probe (FALSE);

  return is_movement;
}


/*
 * run_standard_verb_commands_inner()
 *
 * The dedicated verb handlers: everything run390's generaltasks() calls by
 * name, above the out-of-room task refusal at loc_45FFE8.  Split from the
 * fallback bucket below because the refusal goes between the two; see the
 * ordering note on run_task_refusal().
 */
static scr_bool
run_standard_verb_commands_inner (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /*
   * Search movement commands first, returning TRUE if any matching command
   * handler succeeded.  Then repeat for standard library commands.
   */
  if (run_try_command_table (run_move_commands (bundle), game, string))
    return TRUE;

  /* The take-from catch-alls; see STANDARD_TAKE_FROM_COMMANDS. */
  if (run_try_command_table (STANDARD_TAKE_FROM_COMMANDS, game, string))
    return TRUE;

  if (run_try_command_table (STANDARD_COMMANDS, game, string))
    return TRUE;

  /*
   * The same commands again with whole-word containment for a trailing
   * %object% (uip_match_entity()): the Runner's co() finds an object named
   * anywhere in the line, so `x silver key` examines "a key" (man_overboard,
   * 4.0).  A separate pass, so that every pattern's positional match has
   * had its turn first, and before the catch-alls below so that they never
   * pre-empt an object the line does name.
   */
  {
    const scr_containment_guard containment (TRUE);

    if (run_try_command_table (STANDARD_COMMANDS, game, string))
      return TRUE;
  }

  /* The pre-4.0 put catch-alls; see STANDARD_PUT_COMMANDS. */
  if (run_try_command_table (STANDARD_PUT_COMMANDS, game, string))
    return TRUE;

  /*
   * run390's take handler, still above the refusal; see
   * STANDARD_ABOVE_REFUSAL_COMMANDS.
   */
  const scr_containment_guard containment (TRUE);

  return run_try_command_table (STANDARD_ABOVE_REFUSAL_COMMANDS, game, string);
}

/*
 * run_standard_verb_commands()
 *
 * The verb pass with its pre-4.0 deferrals set for the while.
 */
scr_bool
run_standard_verb_commands (scr_gameref_t game, const scr_char *string)
{
  /*
   * Pre-4.0 the give to a present character is characters()'s (run390) or
   * therest()'s (run380), both below the room refusal, and run390's give
   * writes only into an empty message (or one holding " might need " / "I
   * don't understand").  the_hangover
   * (3.9) T42 `give the doctor some french fries`, with Where=0 task 10
   * matching it: "You can't do that here!" (runner_transcripts/
   * the_hangover.txt), not "Doctor doesn't seem interested in the french
   * fries.".  4.0 gives in the input routine (48A98A), above the refusal.
   */
  run_defer_give_npc =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400;
  run_defer_with_390 = !battle_is_enabled (game)
                       && lib_with_arm_390_applies (game);
  const scr_bool status = run_standard_verb_commands_inner (game, string);
  run_defer_give_npc = FALSE;
  run_defer_with_390 = FALSE;
  return status;
}

/*
 * The give-to-character rows of STANDARD_COMMANDS, run pre-4.0 between the
 * room refusal and the fallback bucket; see run_standard_verb_commands().
 */
scr_bool
run_standard_give_npc_commands (scr_gameref_t game, const scr_char *string)
{
  static scr_commands_t GIVE_NPC_COMMANDS[] = {
    {"give %object% to %character%", lib_cmd_give_object_npc},
    {"give %character% %object%", lib_cmd_give_object_npc},
    {"give %object% %character%", lib_cmd_give_object_npc},
    {nullptr, nullptr}
  };

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;
  if (run_try_command_table (GIVE_NPC_COMMANDS, game, string))
    return TRUE;
  const scr_containment_guard containment (TRUE);

  return run_try_command_table (GIVE_NPC_COMMANDS, game, string);
}


/* run390's therest(): the generic catch-alls, below the refusal. */
scr_bool
run_standard_fallback_commands (scr_gameref_t game, const scr_char *string)
{
  const scr_bool is_ended = game->turn.pending_endgame != 0
      && prop_get_taf_version (gs_get_bundle (game)) == TAF_VERSION_400;

  /*
   * The fallback verbs resolve their noun the Runner's way too: generaltasks
   * (Proc_19_85_489F4C) runs co() once, up front, and every generic verb after
   * it sees the object it found -- `pull de la palanca` (Vardock Bates, 4.0,
   * a `tirar`->`pull` synonym leaving the "de" in place) is "You pull la
   * palanca, but nothing happens." in run400 (transcript), not the object-less
   * "You pull, but nothing happens.".  Containment stays a per-row fallback
   * (positional first), so a row's "%object% *" form still wins over the
   * "%text%" catch-all beneath it.
   */
  const scr_containment_guard containment (TRUE);

  return run_try_command_table (is_ended ? STANDARD_ENDED_FALLBACK_COMMANDS
                                         : STANDARD_FALLBACK_COMMANDS,
                                game, string);
}

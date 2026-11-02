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
 * Save/restore, locate, turns, score, battle status, examining
 * other things, and the one-line stock responses.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/*
 * lib_cmd_save()
 * lib_cmd_restore()
 *
 * Save/restore a game.
 */
scr_bool
lib_cmd_save (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_SAVE))
    {
      if (ser_save_game_prompted (game))
        if_print_string ("Ok.\n");
      else
        if_print_string ("Save failed.\n");
    }

  /*
   * A turn in 3.9 (see lib_is_version_390()): run390 answers `save` with
   * "Game saved." and then the event tick, which is where FarFromHome's
   * event clock gained a tick per save.
   */
  game->is_admin = !lib_is_version_390 (game);
  return TRUE;
}

scr_bool
lib_cmd_restore (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_RESTORE))
    {
      if (ser_load_game_prompted (game))
        {
          if_print_string ("Ok.\n");
          game->is_running = FALSE;
          game->do_restore = TRUE;
        }
      else
        if_print_string ("Restore failed.\n");
    }

  /* A turn in 3.9; see lib_is_version_390(). */
  game->is_admin = !lib_is_version_390 (game);
  return TRUE;
}


/*
 * lib_locate_referenced_object()
 *
 * The one seen object the last command referenced, or -1 once the "don't
 * know" or "be more clear" answer has been printed for none or several.
 */
static scr_int
lib_locate_referenced_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int index_, count, object;

  /*
   * Filter to remove unseen object references.  Note that this is different
   * from NPCs, who we acknowledge even when unseen.
   */
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (!gs_object_seen (game, index_))
        game->object_references[index_] = FALSE;
    }

  /* Count the number of objects referenced by the last command. */
  count = 0;
  object = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (game->object_references[index_])
        {
          count++;
          object = index_;
        }
    }

  /*
   * If no objects identified, be coy about revealing anything; if more than
   * one, be vague.
   */
  if (count == 0)
    {
      pf_buffer_string (filter, "I don't know where that is.\n");
      return -1;
    }
  else if (count > 1)
    {
      pf_buffer_string (filter,
                        "Please be more clear about what you want to"
                        " locate.\n");
      return -1;
    }

  return object;
}

/* "<Npc> is carrying / is wearing <object>.", for a character once seen. */
static void
lib_locate_object_on_npc (scr_gameref_t game,
                          scr_int object, scr_int position, scr_int parent)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (gs_npc_seen (game, parent))
    {
      pf_new_sentence (filter);
      lib_print_npc_np (game, parent);
      /*
       * "is carrying", not "is holding" -- the two literals sit side by
       * side in the Runner, run400 @467FC1 and @46802E, run380 @4372E6
       * and @43736B.  Upstream SCARE invented "holding".
       */
      pf_buffer_string (filter,
                        (position == OBJ_HELD_NPC)
                          ? " is carrying " : " is wearing ");
      lib_print_object_np (game, object);
      pf_buffer_string (filter, ".\n");
    }
  else
    pf_buffer_string (filter, "I don't know where that is.\n");
}

/* "<Object> is a part of you / of <npc>." */
static void
lib_locate_object_part (scr_gameref_t game, scr_int object, scr_int parent)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (parent == -1)
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             " a part of you!\n",
                                             " a part of me!\n",
                                             " a part of %player%!\n"));
    }
  else
    {
      if (gs_npc_seen (game, parent))
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object,
                                                  " is", " are"));
          lib_print_wrapped_npc (game, " a part of ", parent, ".\n");
        }
      else
        pf_buffer_string (filter, "I don't know where that is.\n");
    }
}

/* "<Object> is on / inside <parent>.", for a parent once seen. */
static void
lib_locate_object_nested (scr_gameref_t game,
                          scr_int object, scr_int position, scr_int parent)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (gs_object_seen (game, parent))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      pf_buffer_string (filter,
                        (position == OBJ_ON_OBJECT) ? " on " : " inside ");
      lib_print_object_np (game, parent);
      pf_buffer_string (filter, ".\n");
    }
  else
    pf_buffer_string (filter, "I don't know where that is.\n");
}

/*
 * lib_locate_object_by_position()
 *
 * Answer from the object's position and parent where those say it all;
 * FALSE leaves a static, or a dynamic object on a floor, to the room search.
 */
static scr_bool
lib_locate_object_by_position (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int position, parent;

  /* See if we can print a message based on position and parent. */
  position = gs_object_position (game, object);
  parent = gs_object_parent (game, object);
  switch (position)
    {
    case OBJ_HIDDEN:
      if (!obj_is_static (game, object))
        {
          pf_buffer_string (filter, "I don't know where that is.\n");
          return TRUE;
        }
      break;

    case OBJ_HELD_PLAYER:
      pf_new_sentence (filter);
      lib_print_response_object (game,
                                 "You are carrying ",
                                 "I am carrying ",
                                 "%player% is carrying ",
                                 object, "!\n");
      return TRUE;

    case OBJ_WORN_PLAYER:
      pf_new_sentence (filter);
      lib_print_response_object (game,
                                 "You are wearing ",
                                 "I am wearing ",
                                 "%player% is wearing ",
                                 object, "!\n");
      return TRUE;

    case OBJ_HELD_NPC:
    case OBJ_WORN_NPC:
      lib_locate_object_on_npc (game, object, position, parent);
      return TRUE;

    case OBJ_PART_NPC:
      lib_locate_object_part (game, object, parent);
      return TRUE;

    case OBJ_ON_OBJECT:
    case OBJ_IN_OBJECT:
      lib_locate_object_nested (game, object, position, parent);
      return TRUE;
    }

  return FALSE;
}

/*
 * lib_locate_object_in_rooms()
 *
 * Name the first room holding the object, if the player has been there.
 */
static void
lib_locate_object_in_rooms (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int room;

  /*
   * Object is either static unmoved, or dynamic and on the floor of a room.
   * Check each room for the object, stopping on first found.
   */
  GS_FOR_EACH_ROOM (game, room)
    {
      if (obj_indirectly_in_room (game, object, room))
        break;
    }
  if (room == gs_room_count (game))
    {
      pf_buffer_string (filter, "I don't know where that is.\n");
      return;
    }

  /* Check that this room's been visited by the player. */
  if (!gs_room_seen (game, room))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      /*
       * No "that" here.  The object branch concatenates a bare "somewhere "
       * (run400 @4681B0, run380 @4375D3) where the character branch of the
       * same command uses " is somewhere that " (run400 @47FD89,
       * run380 @440C11) -- an inconsistency of ADRIFT's own that all four
       * Runners carry.
       */
      pf_buffer_string (filter,
                        lib_select_response (game,
                             " somewhere you haven't been yet.\n",
                             " somewhere I haven't been yet.\n",
                             " somewhere %player_pronoun% hasn't been yet.\n"));
      return;
    }

  /*
   * "<Object> is <lowercased room name>."  The Runner builds this as
   * name & isare(prefix, short) & LCase(room name) & "." -- run400 @468115
   * through @46814E, run380 @4374E1 -- and the " -- " upstream SCARE printed
   * here appears in none of the four binaries.
   */
  pf_new_sentence (filter);
  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_plurality (game, object, " is ", " are "));
  lib_print_room_name_lower (game, room);
  pf_buffer_string (filter, ".\n");
}

/*
 * lib_cmd_locate_object()
 * lib_cmd_locate_npc()
 *
 * Display the location of a selected object, and selected NPC.
 */
scr_bool
lib_cmd_locate_object (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int object;

  /*
   * "where is X" is a real turn in every Runner: run400's whereis
   * (Proc_19_33_4684E4, body 467CE0-4684E1, which also prints "I don't
   * know where that is!" at 4684DA) and the characters() where/find/locate
   * block 47FC8D-47FE19 never write the not-a-turn flag MemVar_494281, and
   * run390's whereis 43FF98 / characters() 45ACD8 never write 468219.
   * Ticket (runner_probes/ticket.run400.txt, 2026-09-12): "where is young
   * girl" ticks 59 draws.
   */

  object = lib_locate_referenced_object (game);
  if (object == -1)
    return TRUE;

  /*
   * The reference is unambiguous, so we're responsible for noting it in
   * variables.  Disambiguation would normally do this for us, but we just
   * bypassed it.
   */
  var_set_ref_object (vars, object);

  if (!lib_locate_object_by_position (game, object))
    lib_locate_object_in_rooms (game, object);
  return TRUE;
}

/* Answer for a character the player has yet to meet; TRUE if that is so. */
static scr_bool
lib_npc_not_seen_yet (scr_gameref_t game, scr_int npc)
{
  if (gs_npc_seen (game, npc))
    return FALSE;

  lib_print_response_npc (game,
                          "You haven't seen ",
                          "I haven't seen ",
                          "%player% haven't seen ",
                          npc, " yet!\n");
  return TRUE;
}

scr_bool
lib_cmd_locate_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, npc, room;

  /* A real turn in every Runner; see lib_cmd_locate_object(). */

  /* Count the number of NPCs referenced by the last command. */
  count = lib_count_npc_references (game, &npc);

  /*
   * If no NPCs identified, be coy about revealing anything; if more than one,
   * be vague.  The "... where that is..." is the correct message even for
   * NPCs -- it's the same response as for lib_cmd_locate_other().
   */
  if (count == 0)
    {
      pf_buffer_string (filter, "I don't know where that is.\n");
      return TRUE;
    }
  else if (count > 1 && lib_is_version_400 (game))
    {
      pf_buffer_string (filter,
                        "Please be more clear about who you want to locate.\n");
      return TRUE;
    }

  /*
   * Pre-4.0 assigns the answer outright for every character named, so the
   * LAST one named answers, here or not: `where is guard` with Ann and Bob
   * here and Cora next door is "You haven't seen Cora yet!", and once she
   * is met "Cora is cave." (run370x/run380x/run390x,
   * runner_probes/npcamb.run370.b.rtf, runner_probes/npcamb.run380.rtf,
   * runner_probes/npcamb.run390.txt; run380 440B3E).  npc is already the
   * last named.
   */

  /*
   * The reference is unambiguous, so we're responsible for noting it in
   * variables.  Disambiguation would normally do this for us, but we just
   * bypassed it.
   */
  var_set_ref_character (vars, npc);

  /* See if this NPC has been seen yet. */
  if (lib_npc_not_seen_yet (game, npc))
    return TRUE;

  /*
   * A corpse gets its own answer, ahead of the room search.  The Runner
   * reaches it only after the seen test and only when the room field is not a
   * real room -- the -5 it stamps there fails the `> 0` gate -- at run400
   * @47FDB9 and run390 @459D68.  3.7 and 3.8 have no battle system and no such
   * branch or string at all, so there is nothing to gate on version.
   */
  if (gs_npc_dead (game, npc))
    {
      lib_print_wrapped_npc (game, "", npc, " is dead!\n");
      return TRUE;
    }

  /* Check each room for the NPC, stopping on first found. */
  GS_FOR_EACH_ROOM (game, room)
    {
      if (npc_in_room (game, npc, room))
        break;
    }
  if (room == gs_room_count (game))
    {
      lib_print_wrapped_npc (game, "I don't know where ", npc, " is.\n");
      return TRUE;
    }

  /* Check that this room's been visited by the player. */
  if (!gs_room_seen (game, room))
    {
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter,
                        lib_select_response (game,
                             " is somewhere that you haven't been yet.\n",
                             " is somewhere that I haven't been yet.\n",
                             " is somewhere that %player_pronoun% hasn't been yet.\n"));
      return TRUE;
    }

  /*
   * "<Name> is <lowercased room name>.", then the smart-alec clause when the
   * NPC is standing next to the player.  The character branch uses a literal
   * " is " rather than isare(), and the room name is lowercased exactly as in
   * the object branch:
   *      run370 @438D0A   run380 @440BDF   run390 @459D27   run400 @47FD19
   *
   * The clause itself was disabled upstream; all four Runners print it, and
   * there is no comma before "silly" -- the literals are "  (Right next to "
   * and " silly!)" with the perspective pronoun spliced between them
   * (run380 @00040BCE/@00040BE2, run400 @47FD56/@47FD6A).  Scarier deliberately
   * keeps the vocative comma (deviation policy), except in a game with an ALR
   * written against the Runner's "silly!)" -- Fugitive, IceCream, panic, The
   * Dead Man and eight more rewrite or blank the clause -- where the Runner's
   * form is kept so that the author's replacement still fires.
   */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, " is ");
  lib_print_room_name_lower (game, room);
  pf_buffer_string (filter, ".");
  if (room == gs_playerroom (game))
    {
      if (pf_alr_mentions (gs_get_bundle (game), "silly!)"))
        pf_buffer_string (filter,
                          lib_select_response (game,
                                            "  (Right next to you silly!)",
                                            "  (Right next to me silly!)",
                                            "  (Right next to %player% silly!)"));
      else
        pf_buffer_string (filter,
                          lib_select_response (game,
                                            "  (Right next to you, silly!)",
                                            "  (Right next to me, silly!)",
                                            "  (Right next to %player%, silly!)"));
    }
  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_cmd_turns()
 * lib_cmd_score()
 *
 * Display turns taken and score so far.
 */
scr_bool
lib_cmd_turns (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* "1 turns" -- no Runner singularises it (run400 48ACA1 concatenates the
     one literal; measured run400 EV16 runner_probes/ev16.run400.txt and
     run390 BobBobsly.taf runner_probes/bob_bobsly.run390.txt, both "You
     have taken 1 turns so far."). */
  pf_buffer_string (filter, "You have taken ");
  pf_buffer_integer (filter, game->turns);
  pf_buffer_string (filter, " turns so far.\n");

  lib_set_admin (game);
  return TRUE;
}

scr_bool
lib_cmd_score (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_score, percent;

  /* Get max score, and calculate score as a percentage. */
  max_score = prop_get_global_integer (bundle, "MaxScore");
  if (game->score > 0 && max_score > 0)
    percent = (game->score * 100) / max_score;
  else
    percent = 0;

  /* Output carefully formatted response. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "Your score is ",
                                         "My score is ",
                                         "%player%'s score is "));
  pf_buffer_integer (filter, game->score);
  pf_buffer_string (filter, " out of a maximum of ");
  pf_buffer_integer (filter, max_score);
  pf_buffer_string (filter, ".  (");
  pf_buffer_integer (filter, percent);
  pf_buffer_string (filter, "%)\n");

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_print_battle_attribute()
 * lib_print_battle_status()
 *
 * Helpers for the Battle System "status" command, matching the Runner's
 * status table (settled live 2026-08-01, probes pWS/pWS2): a header row
 * "Range / Max / Current value (inc weapons/armour)", then one row per
 * attribute.  The stamina row is live / max / live (no lo-hi range); the
 * other rows are "lo-hi", the configured max, a fresh effective roll that
 * folds in the wielded weapon and worn armour, and -- for the three rows
 * equipment can affect -- the equipment share in parentheses (agility takes
 * none, so its row has no parenthesis).  The table has no leading "You
 * have:" line, and the trailing wielding line is indented to the first
 * column and names the weapon with its article prefix ("a sword"), or
 * "nothing".  Until a wield is set the player's values are bare -- no
 * would-be weapon is folded in.
 */
enum
{ STATUS_COL_LABEL = 16,
  STATUS_COL_RANGE = 14,
  STATUS_COL_MAX = 12,
  STATUS_COL_CURRENT = 12
};

static void
lib_print_battle_attribute (scr_gameref_t game, scr_int npc,
                            const scr_char *label, const scr_char *base,
                            scr_bool has_bonus)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_char buffer[96], range[32];
  scr_int lo, hi, current;

  battle_attribute_report (game, npc, base, &lo, &hi, &current);
  snprintf (range, sizeof (range), "%ld-%ld", lo, hi);
  if (has_bonus)
    snprintf (buffer, sizeof (buffer), "%-*s%-*s%-*ld%-*ld(%ld)",
              STATUS_COL_LABEL, label, STATUS_COL_RANGE, range,
              STATUS_COL_MAX, battle_attribute_max (game, npc, base),
              STATUS_COL_CURRENT, current,
              battle_attribute_bonus (game, npc, base));
  else
    snprintf (buffer, sizeof (buffer), "%-*s%-*s%-*ld%ld",
              STATUS_COL_LABEL, label, STATUS_COL_RANGE, range,
              STATUS_COL_MAX, battle_attribute_max (game, npc, base),
              current);
  pf_buffer_string (filter, buffer);
  pf_buffer_answer_break (filter);
}

static void
lib_print_battle_status (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_char buffer[96];
  scr_int stamina, maxstamina, weapon;

  maxstamina = battle_attribute_max (game, npc, "Stamina");

  /* A character with no stamina configured is not a combatant. */
  if (npc >= 0 && maxstamina <= 0)
    {
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter, " has:\n   Nothing worth mentioning.\n");
      return;
    }

  /* The header row, indented past the label column as the Runner's is. */
  snprintf (buffer, sizeof (buffer), "%-*s%-*s%-*s%s",
            STATUS_COL_LABEL, "", STATUS_COL_RANGE, "Range",
            STATUS_COL_MAX, "Max", "Current value (inc weapons/armour)");
  pf_buffer_string (filter, buffer);
  pf_buffer_answer_break (filter);

  /* The stamina row is live / max / live -- no lo-hi range, no bonus. */
  stamina = (npc < 0)
            ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
  snprintf (buffer, sizeof (buffer), "%-*s%-*ld%-*ld%ld",
            STATUS_COL_LABEL, "Stamina:", STATUS_COL_RANGE, stamina,
            STATUS_COL_MAX, maxstamina, stamina);
  pf_buffer_string (filter, buffer);
  pf_buffer_answer_break (filter);

  lib_print_battle_attribute (game, npc, "Hit strength:", "Strength", TRUE);
  lib_print_battle_attribute (game, npc, "Accuracy:", "Accuracy", TRUE);
  lib_print_battle_attribute (game, npc, "Defense value:", "Defense", TRUE);
  lib_print_battle_attribute (game, npc, "Agility:", "Agility", FALSE);

  /* Name the weapon the combatant is wielding -- "nothing" when unarmed --
   * with the article prefix the Runner uses ("... is wielding a sword."). */
  weapon = battle_combatant_weapon (game, npc);
  snprintf (buffer, sizeof (buffer), "%-*s", STATUS_COL_LABEL, "");
  pf_buffer_string (filter, buffer);
  if (npc < 0)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are wielding ",
                                           "I am wielding ",
                                           "%player% is wielding "));
  else
    {
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter, " is wielding ");
    }
  if (weapon >= 0)
    lib_print_object (game, weapon);
  else
    pf_buffer_string (filter, "nothing");
  pf_buffer_string (filter, ".\n");
}


/*
 * lib_print_battle_status_390()
 *
 * run390's status is not the 4.0 table but three tab-joined rows (dobattle
 * 44C595..44C80F, player arm from 44C6D2):
 *
 *   Stamina:<tab><tab>80 (102)
 *   Hit strength:<tab><tab>6 (1)
 *   Defense value:<tab>3 (0)
 *
 * The value is the live stamina, hitstrength() (the record's strength plus
 * the wielded or best weapon's HitValue) or armourstrength() (defence plus
 * worn protection), and the bracket is the attribute's maximum field.  No
 * header, no Accuracy or Agility row and no wielding line.  Measured on the
 * town of azra (3.90, run390x runner_transcripts/the_town_of_azra_v390.txt
 * T60, 2026-09-14).
 */
static void
lib_print_battle_status_390 (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_char buffer[96];
  scr_int lo, hi, current;

  snprintf (buffer, sizeof (buffer), "Stamina:\t\t%ld (%ld)\n",
            (npc < 0) ? gs_playerstamina (game) : gs_npc_stamina (game, npc),
            battle_attribute_max (game, npc, "Stamina"));
  pf_buffer_string (filter, buffer);

  battle_attribute_report (game, npc, "Strength", &lo, &hi, &current);
  snprintf (buffer, sizeof (buffer), "Hit strength:\t\t%ld (%ld)\n",
            current, battle_attribute_max (game, npc, "Strength"));
  pf_buffer_string (filter, buffer);

  battle_attribute_report (game, npc, "Defense", &lo, &hi, &current);
  snprintf (buffer, sizeof (buffer), "Defense value:\t%ld (%ld)\n",
            current, battle_attribute_max (game, npc, "Defense"));
  pf_buffer_string (filter, buffer);
}


/*
 * lib_cmd_status_player()
 * lib_cmd_status_npc()
 *
 * The Battle System "status" and "status <character>" commands.  When the
 * Battle System is disabled these fall back to the traditional behaviour of
 * "status", which prints the game's status line.
 */
scr_bool
lib_cmd_status_player (scr_gameref_t game)
{
  if (!battle_is_enabled (game))
    return lib_cmd_statusline (game);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    lib_print_battle_status_390 (game, -1);
  else
    lib_print_battle_status (game, -1);
  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_status_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int index_, count, npc;

  if (!battle_is_enabled (game))
    return lib_cmd_statusline (game);

  game->is_admin = TRUE;

  /*
   * run390's NPC loop (44C53A-44C6CD) takes the lowest-indexed character the
   * line names by Name or first Alias, and exits on it: its table if seen,
   * otherwise the refusal below.  A line naming nobody drops out of the loop
   * into the player's table.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      GS_FOR_EACH_NPC (game, index_)
        {
          if (!game->npc_references[index_])
            continue;
          var_set_ref_character (vars, index_);
          if (gs_npc_seen (game, index_))
            lib_print_battle_status_390 (game, index_);
          else
            pf_buffer_string (filter,
                              lib_select_response (game,
                                  "You can't get the status of a character"
                                  " you've not seen yet!\n",
                                  "I can't get the status of a character"
                                  " I've not seen yet!\n",
                                  "%player% can't get the status of a"
                                  " character you've not seen yet!\n"));
          return TRUE;
        }
      lib_print_battle_status_390 (game, -1);
      return TRUE;
    }

  /* Count and identify the NPCs referenced by the command. */
  count = lib_count_npc_references (game, &npc);

  if (count == 0)
    {
      pf_buffer_string (filter, "I don't know who you mean.\n");
      return TRUE;
    }
  else if (count > 1)
    {
      pf_buffer_string (filter,
                        "Please be more clear about whose status you want.\n");
      return TRUE;
    }

  /* Unambiguous reference; note it, as we bypassed disambiguation. */
  var_set_ref_character (vars, npc);

  /* Refuse to report on a character the player has not encountered. */
  if (lib_npc_not_seen_yet (game, npc))
    return TRUE;

  lib_print_battle_status (game, npc);
  return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Standard response commands.  These are uninteresting catch-all cases,
 * but it's good to make then right as game ALRs may look for them.
 */
scr_bool
lib_cmd_profanity (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I really don't think there's any need for language like"
                            " that!\n");
}

/*
 * lib_cmd_profanity_390()
 * lib_cmd_profanity_pre_400()
 *
 * Two words entered and left the Runner's swearing list.  `bugger` is in the
 * 3.9 and 4.0 lists but in neither 3.7's nor 3.8's, and `bloody` is in
 * 3.7/3.8/3.9 and gone from 4.0.  Where the word is not in that Runner's list
 * these decline, so the input falls through to the rest of the grammar exactly
 * as any other unrecognised word does.
 */
scr_bool
lib_cmd_profanity_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return lib_cmd_profanity (game);
}

scr_bool
lib_cmd_profanity_pre_400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_cmd_profanity (game);
}

scr_bool
lib_cmd_examine_all (scr_gameref_t game)
{
  return lib_print_message (game, "Please examine one object at a time.\n");
}

/*
 * lib_npc_examine_absent()
 *
 * `x <character who is somewhere else>`.  Every Runner rewrites the examine
 * tail below when the line names a character who is not in the room:
 *
 *     run370 438F2F-438F4F   run380 440E42-440E62
 *     run390 45A07C-45A09C   run400 4801AD-48021F
 *
 * All four are the same clause, inside characters()' examine branch, and all
 * four compose the same sentence: person word, " cannot see ", the record's
 * Name verbatim, " from here.".  It is a REWRITE, not a handler -- it fires
 * only when the message the turn has produced so far is the examine tail
 * itself, which is what makes this the right place for it:
 *
 *     pre-4.0   msg contains "<player> can't see that", or msg is exactly
 *               "Nothing special."
 *     4.0       msg contains "<player> can't see that", or msg is exactly
 *               "<player> see no such thing."  AND the character's seen
 *               byte (var_140(26)) is 1.
 *
 * That seen byte is the whole 3.9-vs-4.0 difference, and it is already
 * measured from the other side: run400 on EV16 answers `x dave`, with Dave
 * alive in the next room and never yet met, "You see no such thing." rather
 * than naming him (runner_probes/ev16.run400.txt; see lib_cmd_examine_other
 * below).  Pre-4.0 has no such test -- probe 1 on ALEXIS.TAF under run390
 * named an unseen character back at the player
 * (runner_probes/alexis.run390.absent_npc.txt).  The 4.0 half is measured too:
 * cobl (4.00), `x cat` for the ginger cat seen in an earlier room, "You cannot
 * see the ginger cat from here."
 *
 * The reference test is the shared Name-or-first-Alias one, run390 4592B8
 * (`c(LCase(Name)) Or c(LCase(Alias))`); see lib_npc_named_in_line().
 *
 * The one suppression: if the line names the character by its ALIAS and also
 * names any object, the rewrite is skipped (run390 459FFE-45A041 sets
 * var_252 from co(); run400 480172-4801A7 does the same through
 * Proc_21_39_46486C).  A character named only by its Name never runs that
 * scan, so `x goblin` against an object called "goblin" still speaks.  The
 * first named absent character wins: the rewrite destroys the message the
 * guard tests, so no later character in the loop can pass it.
 *
 * The guard is an EQUALITY against the engine's own default, so where the
 * game ALRs that default the answer depends on WHEN the Runner applies its
 * ALRs -- before characters() the equality would miss and the ALR'd tail
 * would stand.  Scarier applies them in the output filter, after this hook,
 * and that is now measured rather than assumed:
 *
 *   run400, p4ALRNPC.taf (runner_probes/alrnpc.run400.txt), ALRs "You see no
 *     such thing." -> "... , or else it is unimportant." and "cannot see" ->
 *     "cannot spot".  `x dave` from the next room, Dave seen: "You cannot spot
 *     Dave from here."  The rewrite fired against the UNALR'd default, and its
 *     own output was then ALR'd -- so the ALR pass runs strictly after this
 *     one.  `x erin` (alive, never seen) gives the ALR'd tail on the same
 *     path, which is the seen gate and the ALR wiring in one control.
 *   run390, p39ALRNPC.taf (runner_probes/alrnpc.run390.txt), the same world
 *     with "Nothing special." ALR'd instead: `x dave` and `x erin` both
 *     answer "You cannot spot <Name> from here." -- the pre-4.0 arm, with
 *     no seen gate.
 *
 * Both transcripts are identical to scarier's on every turn.  That settles
 * the_pk_girl t~3067, whose golden line this rule moved.
 */
static scr_bool
lib_npc_examine_absent (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int npc;

  if (!input)
    return FALSE;

  GS_FOR_EACH_NPC (game, npc)
    {
      const scr_char *name, *alias;
      scr_vartype_t vt_key[4];

      if (!lib_npc_named_in_line (game, npc, input))
        continue;

      /* Present: the examine branch above prints the description instead. */
      if (npc_in_room (game, npc, gs_playerroom (game)))
        return FALSE;

      if (lib_is_version_400 (game) && !gs_npc_seen (game, npc))
        continue;

      alias = lib_first_alias (bundle, vt_key, "NPCs", npc);
      if (alias && alias[0] != NUL && lib_input_contains_word (input, alias))
        {
          scr_int object;
          scr_bool clash = FALSE;

          GS_FOR_EACH_OBJECT (game, object)
            {
              if (lib_verb_object_name_score (game, object, input) > 0)
                {
                  clash = TRUE;
                  break;
                }
            }
          if (clash)
            continue;
        }

      name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
      if (!name || name[0] == NUL)
        continue;

      pf_buffer_string (filter,
                        lib_select_response (game, "You cannot see ",
                                             "I cannot see ",
                                             "%player% cannot see "));
      pf_buffer_string (filter, name);
      pf_buffer_string (filter, " from here.\n");
      return TRUE;
    }

  return FALSE;
}


/*
 * lib_cmd_examine_other()
 *
 * `x <noun>` where the noun names nothing at all, in a lit room.  This is the
 * last line of the Runner's examines(), and 4.0 rewrote it.  Pre-4.0 answers
 * the flat, person-free "Nothing special." (run370 435BF4, readable verbatim
 * in run370's Form1.frm; the string is in all four exes).  4.0 answers
 * "<player> see no such thing." (run400 471EF6) -- and that string is in
 * run400.exe alone, which is what dates the change.
 *
 * Both halves are measured, not argued:
 *   run390, Merry_Murders.taf (3.90),
 *     runner_probes/merry_murders.run390.pocket.txt line 38 --
 *     `x pocket` in the lit Plaza, no `pocket` object in the game:
 *     "Nothing special."
 *   run400, The_X-Files_A_New_Beginning.taf (4.00),
 *     runner_probes/xfiles.run400.txt lines 187 and 233 -- `look at camera`
 *     and `look up byers`, neither noun an object: "You see no such
 *     thing."
 *
 * 4.0 also sets a flag beside this message (MemVar_494281 at 471F02), and
 * that is the whole of 4.0's tail.  Pre-4.0 puts a darkness fork in front of
 * it instead: run390 44BFDE tests the message it has built so far against ""
 * and "Nothing special.", and when the room is dark (lib_room_is_dark())
 * answers "<player> can't see that very clearly." (44C477), or, for `x me`,
 * "<player> can just make out that <you> <are> okay." (44C430).  Note that
 * lib_npc_examine_absent()'s guard already anticipates this: it fires on a
 * message that CONTAINS "<player> can't see that", which is the darkness
 * tail as well as the lit one, so the absent-character rewrite still wins in
 * a dark room -- and that is why it is tested first here.
 *
 * The object-found-but-silent default one branch up is the SAME 4.0 rewrite
 * and splits the same way, but it is decompile-only so far -- see
 * lib_cmd_examine_object() and harness/make_39_examprobe.py.
 */
scr_bool
lib_cmd_examine_other (scr_gameref_t game)
{
  /*
   * A bare `x`, `ex` or `examine` never reaches this tail from 3.9 on:
   * examines exits on it at once (run400 471340, run390 44B758).  run400's
   * line falls to the game's DontUnderstand -- see lib_cmd_look_typed() --
   * but run390's therest() still has checkverb arms for all three (45E64E-
   * 45E67C), so 3.9 answers "Examine what?".  `exam` is not in that test and
   * still comes here: "Nothing special." at 3.9, "You see no such thing." at
   * 4.0.  3.7/3.8 have no exit, and every bare form is "Nothing special.".
   * p39EXAM run390x runner_probes/exam.run390.abbr.txt, p4EXAM run400x
   * runner_probes/exam.run400.abbr.txt, p38EXAM run380x
   * runner_probes/exam.run380.abbr.rtf.
   */
  if (lib_is_version_390 (game) || lib_is_version_400 (game))
    {
      const scr_char *input = run_get_dispatch_input ();

      if (input)
        {
          scr_owned_string owned_line (scr_strdup (input));
          scr_char *const line = owned_line.get ();
          scr_bool is_bare;

          scr_normalize_string (line);
          is_bare = scr_strcasecmp (line, "x") == 0
                    || scr_strcasecmp (line, "ex") == 0
                    || scr_strcasecmp (line, "examine") == 0;
          if (is_bare)
            {
              if (!lib_is_version_390 (game))
                return FALSE;
              lib_what (game, "Examine");
              /* `x` / `stone` examines the stone
               * (runner_probes/task.run390.pfx.txt); lib_what() stores only
               * a line equal to its verb. */
              lib_battle_who_pending = input;
              return TRUE;
            }
        }
    }

  /*
   * Pre-4.0 characters() names a character by c(Name) Or c(Alias) anywhere
   * in the line, and its examine arm assigns the description of every one
   * that is here -- so `x big dave` and `x tall guard`, which SCARE's
   * %character% pattern does not bind, describe Dave and (of Ann and Bob,
   * both guards) Bob, the LAST present (run370x/run380x/run390x,
   * runner_probes/npcamb.run370.b.rtf, runner_probes/npcamb.run380.rtf,
   * runner_probes/npcamb.run390.txt; run380 440D0B).
   */
  if (!lib_is_version_400 (game))
    {
      const scr_char *input = run_get_dispatch_input ();
      scr_int npc, found = -1;

      for (npc = 0; input && npc < gs_npc_count (game); npc++)
        {
          if (npc_in_room (game, npc, gs_playerroom (game))
              && lib_npc_named_in_line (game, npc, input))
            found = npc;
        }
      if (found != -1)
        {
          GS_FOR_EACH_NPC (game, npc)
            game->npc_references[npc] = (npc == found);
          if (lib_cmd_examine_npc (game))
            return TRUE;
        }
    }

  /*
   * characters() rewrites this tail when the noun names an absent character;
   * see lib_npc_examine_absent().  4.0 has already set its not-a-turn flag
   * by then (471F02, before characters() runs), so the named answer is an
   * admin turn there and an ordinary one pre-4.0, exactly like the tail it
   * replaces.
   */
  if (lib_npc_examine_absent (game))
    {
      if (lib_is_version_400 (game))
        game->is_admin = TRUE;
      return TRUE;
    }

  if (!lib_is_version_400 (game))
    {
      if (lib_room_is_dark (game, gs_playerroom (game)))
        return lib_print_response_message (game,
                                  "You can't see that very clearly.\n",
                                  "I can't see that very clearly.\n",
                                  "%player% can't see that very clearly.\n");
      return lib_print_message (game, "Nothing special.\n");
    }

  /*
   * The 4.0 text is the player's name and the literal " see no such thing."
   * (471EF6), so a named third-person player gets "Player see no such
   * thing." -- measured run400, EV16, runner_probes/ev16.run400.txt (`x dave`,
   * Dave being in the next room).  The same probe reads `turns` unchanged
   * across it: the flag 4.0 sets beside the message (MemVar_494281 at 471F02,
   * and at 4801E1 for the character handler's copy) is the Runner's "not a
   * turn" flag, the one `turns` and `score` set.
   */
  lib_print_response_message (game,
                              "You see no such thing.\n",
                              "I see no such thing.\n",
                              "%player% see no such thing.\n");
  game->is_admin = TRUE;
  return TRUE;
}

/*
 * lib_cmd_look_anywhere_pre_400()
 *
 * Pre-4.0 therest() tests c("look") over the WHOLE line, not just its first
 * word, and answers the flat "Nothing special." (run370 43D906, run380
 * 44439D, run390 45DB9F):
 *
 *     If c("look") Or (input = "l" And msg = "") Then msg = "Nothing special."
 *
 * The typed-look list (lib_cmd_look_typed()) has already taken an exact
 * `look`, so what reaches here is a line with "look" somewhere else in it:
 * `look,`, `zzz, look` and `, look` (run380x
 * runner_probes/ask.run380.split2.rtf) and, at 3.9 only, `look.` (run390x
 * runner_probes/ask.run390.split.txt) -- 3.9's c() also ends a word at a
 * period, 3.8 answers `look.` with the game's DontUnderstand.
 *
 * c() is the Runner's (run_c_word_pre400()), not uip_contains_words().
 * The row sits above the object and character catch-alls, which the Runner
 * reaches only on an empty message.  A line whose look is overwritten by a
 * later arm (`look, push lamp`) never gets here: run_therest_pre400() has
 * answered it with that arm.
 */
scr_bool
lib_cmd_look_anywhere_pre_400 (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *const input = run_get_dispatch_input ();

  if (version >= TAF_VERSION_400 || !input
      || run_c_word_pre400 (version, input, "look") < 0)
    return FALSE;

  return lib_print_message (game, "Nothing special.\n");
}

/*
 * lib_cmd_examine_absent()
 *
 * `x <object seen elsewhere>`, once every other examine row has declined it.
 * 4.0 only; see lib_absent_seen_object() for the measurement.
 */
scr_bool
lib_cmd_examine_absent (scr_gameref_t game)
{
  /* Pre-3.9; see lib_absent_named_object_pre_390(). */
  const scr_int object = lib_absent_named_object_pre_390 (game);

  if (object != -1)
    {
      if (gs_object_seen (game, object))
        return lib_cant_see_named_pre_390 (game, object, FALSE,
                                           " from here!\n");
      return lib_print_response_message (game,
                                         "You can't see that.\n",
                                         "I can't see that.\n",
                                         "%player% can't see that.\n");
    }

  return lib_cant_see_absent_object (game, " from here!\n", TRUE);
}

scr_bool
lib_cmd_locate_other (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* A real turn in every Runner (run400 4684DA); see lib_cmd_locate_object(). */
  pf_buffer_string (filter, "I don't know where that is!\n");
  return TRUE;
}

scr_bool
lib_cmd_unix_like (scr_gameref_t game)
{
  return lib_print_message (game, "This isn't Unix you know!\n");
}

scr_bool
lib_cmd_dos_like (scr_gameref_t game)
{
  return lib_print_message (game, "This isn't Dos you know!\n");
}

scr_bool
lib_cmd_cry (scr_gameref_t game)
{
  return lib_print_message (game, "There's no need for that!\n");
}

scr_bool
lib_cmd_dance (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You do a little dance.\n",
                                     "I do a little dance.\n",
                                     "%player% do a little dance.\n");
}

scr_bool
lib_cmd_eat_other (scr_gameref_t game)
{
  /*
   * A 4.0 string (run400 4889C7).  run380's eat arm (443D6E) and run390's
   * (45D4EF) speak only for a present object, so with none the line goes
   * on to the catch-all: `eat statue` from the wrong room is the same-room
   * answer in 3.8 (runner_probes/exam.run380.verb.rtf) and "I don't
   * understand." in 3.9 (runner_probes/exam.run390.verb.txt).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
      && !lib_is_version_400 (game))
    return FALSE;

  return lib_print_message (game,
                            "I don't understand what you are trying to eat.\n");
}

scr_bool
lib_cmd_fight (scr_gameref_t game)
{
  return lib_print_message (game, "There is nothing worth fighting here.\n");
}

scr_bool
lib_cmd_feed (scr_gameref_t game)
{
  return lib_print_message (game, "There is nothing worth feeding here.\n");
}

scr_bool
lib_cmd_feel (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You feel nothing out of the ordinary.\n",
      "I feel nothing out of the ordinary.\n",
      "%player% feel nothing out of the ordinary.\n");
}

scr_bool
lib_cmd_fly (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You can't fly.\n",
                                     "I can't fly.\n",
                                     "%player% can't fly.\n");
}

scr_bool
lib_cmd_hint (scr_gameref_t game)
{
  return lib_print_message (game,
                            "You're just going to have to work it out for"
                            " yourself...\n");
}

scr_bool
lib_cmd_hum (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You hum a little tune.\n",
                                     "I hum a little tune.\n",
                                     "%player% hum a little tune.\n");
}

scr_bool
lib_cmd_jump (scr_gameref_t game)
{
  return lib_print_message (game, "Wheee-boinng.\n");
}

scr_bool
lib_cmd_listen (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You hear nothing out of the ordinary.\n",
      "I hear nothing out of the ordinary.\n",
      "%player% hear nothing out of the ordinary.\n");
}

scr_bool
lib_cmd_please (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "Your kindness gets you nowhere.\n",
                                     "My kindness gets me nowhere.\n",
                                     "%player%'s kindness gets nowhere.\n");
}

scr_bool
lib_cmd_punch (scr_gameref_t game)
{
  return lib_print_message (game, "Who do you think you are, Mike Tyson?\n");
}

scr_bool
lib_cmd_run (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "Why would you want to run?\n",
                                     "Why would I want to run?\n",
                                     "Why would %player_pronoun% want to run?\n");
}

scr_bool
lib_cmd_shout (scr_gameref_t game)
{
  return lib_print_message (game, "Aaarrrrgggghhhhhh!\n");
}

scr_bool
lib_cmd_say (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *string = nullptr;

  /*
   * run390 (therest 45DAD2) and run400 (488DE4) both draw Int(Rnd*6) over
   * six responses; run380/run370 (4442AA/43D813) draw Int(Rnd*5) over the
   * same table, so their sixth response is unreachable.  Keeping the span
   * per version keeps the runner-mode stream in step with the Wine
   * Runners (inverness, measured 2026-09-12).
   */
  switch (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
          ? scr_randomint (1, 6) : scr_randomint (1, 5))
    {
    case 1:
      string = "Gosh, that was very impressive.\n";
      break;
    case 2:
      string = lib_select_response (game,
                                    "Not surprisingly, no-one takes any notice"
                                    " of you.\n",
                                    "Not surprisingly, no-one takes any notice"
                                    " of me.\n",
                                    "Not surprisingly, no-one takes any notice"
                                    " of %player%.\n");
      break;
    case 3:
      string = "Wow!  That achieved a lot.\n";
      break;
    case 4:
      string = "Uh huh, yes, very interesting.\n";
      break;
    case 5:
      string = "That's the most interesting thing I've ever heard!\n";
      break;
    default:
      string = lib_select_response (game,
                                    "No-one listens to your rabblings.\n",
                                    "No-one listens to my rabblings.\n",
                                    "No-one listens to %player%'s"
                                    " rabblings.\n");
      break;
    }

  pf_buffer_string (filter, string);
  return TRUE;
}

scr_bool
lib_cmd_sing (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You sing a little song.\n",
                                     "I sing a little song.\n",
                                     "%player% sing a little song.\n");
}

scr_bool
lib_cmd_sleep (scr_gameref_t game)
{
  return lib_print_message (game, "Zzzzz.  Bored are you?\n");
}

/*
 * 3.7 and 3.8's talk hint (run370 loc_438748, run380 loc_4405D7) is guarded
 * by c("talk") Or c("speak") inside the named-character block -- the words
 * need not be next to each other, and there is no room test.  So `talk with
 * dave` is the hint too, where the `[talk/speak] %character%` rows miss it:
 * 'Use the format "ask Dave about <subject>".' (run370x
 * runner_probes/npcamb.run370.with.rtf) and '... [subject]".' (run380x
 * runner_probes/npcamb.run380.with.rtf).  The last character named wins, as
 * for lib_cmd_talk_to_npc().
 */

static scr_bool
lib_talk_hint_anywhere_pre390 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_, npc = -1;

  if (!input
      || prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;

  GS_FOR_EACH_NPC (game, index_)
    {
      if (lib_npc_referenced (game, index_, input))
        npc = index_;
    }
  if (npc == -1)
    return FALSE;

  var_set_ref_character (gs_get_vars (game), npc);
  lib_print_wrapped_npc (game, "Use the format \"ask ",
                         npc, lib_ask_format_subject (game));
  return TRUE;
}

scr_bool
lib_cmd_speak_pre390 (scr_gameref_t game)
{
  return lib_talk_hint_anywhere_pre390 (game);
}

/*
 * A `talk` line no character answered.  therest's `c("ask") Or c("talk to")`
 * block (run370 43E9B7, run380 444039, run390 45D831, run400 488B5C) takes
 * `talk to` as it takes `ask`: without "about", an empty buffer and no task
 * run for the line, it prints the generic 'Use the format "ask [character]
 * about [subject]"' (run400 488D40-488D8C).  The rabblings line just below
 * it (488DA2-488DC6) only fills a buffer still empty, so it is what a bare
 * `talk dirty` gets, and a `talk to` line whose task ran silently (sophie_comp
 * `talk to devil`).
 *
 * Measured on run400x: Aegis `talk to Elder`, Whatever Happened to Uncle
 * Grumble `talk to mayor`, Shards of Memory `talk to reaper` and YADFA `talk
 * to elves` -- none of them a character's Name or Alias -- all answer the
 * generic hint.
 *
 * Both lines want an empty buffer, and 4.0's up-front resolver (see
 * lib_absent_seen_object()) has already filled it for a seen-but-absent
 * object the line names: The Plague - Redux `talk to girl`, with the girls
 * left behind, is "I couldn't see the girls." (run400x).
 */
scr_bool
lib_cmd_talk (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (lib_talk_hint_anywhere_pre390 (game))
    return TRUE;

  if (lib_cmd_verb_absent_400 (game))
    return TRUE;

  if (input && lib_input_contains_word (input, "talk to")
      && !run_any_task_ran_this_command ())
    return lib_cmd_ask_other (game);

  return lib_print_response_message (game,
      "No-one listens to your rabblings.\n",
      "No-one listens to my rabblings.\n",
      "No-one listens to %player%'s rabblings.\n");
}

scr_bool
lib_cmd_thank (scr_gameref_t game)
{
  return lib_print_message (game, "You're welcome.\n");
}

scr_bool
lib_cmd_whistle (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You whistle a little tune.\n",
                                     "I whistle a little tune.\n",
                                     "%player% whistle a little tune.\n");
}

scr_bool
lib_cmd_interrogation (scr_gameref_t game)
{
  static const scr_char *const RESPONSES[] = {
    "Why do you want to know?\n",
    "Interesting question.\n",
    "Let me think about that one...\n",
    "I haven't a clue!\n",
    "All these questions are hurting my head.\n",
    "I'm not going to tell you.\n",
    "Someday I'll know the answer to that one.\n",
    "I could tell you, but then I'd have to kill you.\n",
    "Ha, as if I'd tell you!\n",
    "Ask me again later.\n",
    "I don't know - could you ask anyone else?\n",
    "Err, yes?!?\n",
    "Let me just check my memory banks...\n",
    "Because that's just the way it is.\n",
    "Do I ask you all sorts of awkward questions?\n",
    "Questions, questions...\n",
    "Who cares.\n"
  };

  return lib_print_message (game,
                            RESPONSES[scr_randomint (1, 17) - 1]);
}

scr_bool
lib_cmd_xyzzy (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I'm sorry, but XYZZY doesn't do anything special in"
                            " this game!\n");
}

scr_bool
lib_cmd_egotistic (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, "No comment.\n");
  return TRUE;
}

scr_bool
lib_cmd_yes_or_no (scr_gameref_t game)
{
  return lib_print_message (game,
                            "That's interesting, but it doesn't mean much.\n");
}

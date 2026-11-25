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
 * The where/find/locate answers for objects and NPCs.
 */

#include "sclibrar.h"


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
        game->refs.objects[index_] = FALSE;
    }

  /* Count the number of objects referenced by the last command. */
  count = 0;
  object = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (game->refs.objects[index_])
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
       * side in the Runner, run400 @467FC1 and @46802E.  Upstream SCARE
       * invented "holding".
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
 * lib_locate_runner_shape()
 *
 * TRUE if the where/find answer for 'name' in 'room' must keep the Runner's
 * shape, "<name> is <lowercased room name>.", rather than Scarier's "is in".
 *
 * Every Runner builds the answer as name & is/are & LCase(room) & "." with
 * no preposition -- "The fire is house.", run400 @468115 for objects and
 * @47FD19 for characters.  Scarier says "is in" (deviation policy: grammar
 * over parity), except in a game whose ALR table was written across that
 * join: The Dead Man rewrites "is control room" to "is on the floor of the
 * control room." and chains two more ALRs off the result, and I am the Law
 * rewrites whole character answers, "Tim is corridor." among them.  There
 * the Runner's exact shape is kept so that the author's replacement still
 * fires.  The test is the one the ALR filter will make: does some ALR
 * original match the Runner's sentence at a position before the room name
 * and run into it?  An ALR that stops before the room ("The fire is ") or
 * starts at it ("house.") matches either form and needs no exception.
 * Only those two games in the corpus have such an ALR (2026-11-07 scan of
 * every table against every object x room and character x room sentence).
 */
static scr_bool
lib_locate_runner_shape (scr_gameref_t game, const scr_char *name,
                         const scr_char *verb, scr_int room)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::string runner (name), lower (lib_get_room_name (game, room));
  size_t join;

  if (!runner.empty ())
    runner[0] = scr_toupper (runner[0]);
  runner += verb;
  join = runner.size ();
  scr_lowercase (lower);
  runner += lower;
  runner += ".";
  return pf_alr_rewrites_span (bundle, runner.c_str (), join, join + 1);
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
  const scr_char *verb;
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
       * (run400 @4681B0) where the character branch of the same command
       * uses " is somewhere that " (run400 @47FD89) -- an inconsistency of
       * ADRIFT's own that all four Runners carry.
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
   * -- and the " -- " upstream SCARE printed here appears in none of the
   * four binaries.  Scarier keeps a capitalised name's case; see
   * lib_print_room_name_lower().  "is in" unless an ALR was written
   * against the Runner's shape; see lib_locate_runner_shape().
   */
  verb = lib_select_plurality (game, object, " is ", " are ");
  pf_new_sentence (filter);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, verb);
  if (!lib_locate_runner_shape (game,
                                lib_definite_object_name (gs_get_bundle (game),
                                                          object).c_str (),
                                verb, room))
    pf_buffer_string (filter, "in ");
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
   * (Proc_19_33_4684E4, which also prints "I don't know where that is!")
   * and the characters() where/find/locate block never write the not-a-turn
   * flag MemVar_494281, and run390's whereis 43FF98 / characters() 45ACD8
   * never write 468219.  Ticket (runner_probes/ticket.run400.txt): "where
   * is young girl" ticks 59 draws.
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
scr_bool
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
   * runner_probes/npcamb.{run370.b.rtf,run380.rtf,run390.txt}; run380
   * 440B3E).  npc is already the last named.
   */

  /* Note the reference, as lib_cmd_locate_object() does. */
  var_set_ref_character (vars, npc);

  /* See if this NPC has been seen yet. */
  if (lib_npc_not_seen_yet (game, npc))
    return TRUE;

  /*
   * A corpse gets its own answer, ahead of the room search.  The Runner
   * reaches it only after the seen test and only when the room field is not a
   * real room -- the -5 it stamps there fails the `> 0` gate -- at run400
   * @47FDB9.  3.7 and 3.8 have no battle system and no such branch or
   * string at all, so there is nothing to gate on version.
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
   * the object branch (Scarier keeps a capitalised name's case; see
   * lib_print_room_name_lower()):
   *      run370 @438D0A ... run400 @47FD19
   *
   * The clause itself was disabled upstream; all four Runners print it, and
   * there is no comma before "silly" -- the literals are "  (Right next to "
   * and " silly!)" with the perspective pronoun spliced between them
   * (run400 @47FD56/@47FD6A).  Scarier deliberately keeps the vocative
   * comma (deviation policy), except in a game with an ALR written against
   * the Runner's "silly!)" -- Fugitive, IceCream, panic, The Dead Man and
   * eight more rewrite or blank the clause -- where the Runner's form is
   * kept so that the author's replacement still fires.  The "is in" of the
   * sentence itself is gated the same way; see lib_locate_runner_shape().
   */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, " is ");
  if (!lib_locate_runner_shape (game,
                                prop_get_indexed_string (gs_get_bundle (game),
                                                         "NPCs", npc, "Name"),
                                " is ", room))
    pf_buffer_string (filter, "in ");
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


scr_bool
lib_cmd_locate_other (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* A real turn in every Runner (run400 4684DA); see lib_cmd_locate_object(). */
  pf_buffer_string (filter, "I don't know where that is!\n");
  return TRUE;
}

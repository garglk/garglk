/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Taking objects from containers, supporters and NPCs.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_take_from_filter()
 *
 * Helper function for deciding if an object may be acquired in this context.
 * Returns TRUE if an object may be acquired, FALSE otherwise.
 */
scr_bool
lib_take_from_filter (scr_gameref_t game, scr_int object, scr_int associate)
{
  /*
   * To be take-able, an object must be either inside or on the specified
   * object, and -- like every other object match in 3.9/4.0 games -- the
   * player must have seen it.  "get all from" a container whose contents
   * are still unrevealed takes nothing (run400, measured live in iachini
   * 2026-08-22: the dining room table's card and towel need "x table"
   * first); lib_take_from_unseen() supplies the refusal for that case.
   */
  return (gs_object_position (game, object) == OBJ_IN_OBJECT
          || gs_object_position (game, object) == OBJ_ON_OBJECT)
         && !obj_is_static (game, object)
         && gs_object_parent (game, object) == associate
         && (!lib_matcher_requires_seen (game)
             || gs_object_seen (game, object));
}


/*
 * lib_take_from_unseen()
 *
 * TRUE if the associate physically holds takeable objects that the filter
 * rejected only because the player hasn't seen them yet.  The Runner
 * refuses "get all from" outright in that case rather than calling the
 * container empty.
 */
static scr_bool
lib_take_from_unseen (scr_gameref_t game, scr_int associate)
{
  scr_int object;

  if (!lib_matcher_requires_seen (game))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && !obj_is_static (game, object)
          && gs_object_parent (game, object) == associate
          && !gs_object_seen (game, object))
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_take_from_unseen_refusal()
 *
 * Response for "get all from" a container or supporter whose contents the
 * player hasn't seen yet.  4.0 refuses outright: run400 answers "You can't
 * take anything from the round glass table." (iachini, measured live
 * 2026-08-22).  Pre-4.0 runs its normal take loop over an empty match and
 * prints just the bare verb phrase: run390 on ALEXIS answers `get all from
 * table` with "You take " -- trailing space, no object list, no period
 * (measured live 2026-08-22).
 */
static void
lib_take_from_unseen_refusal (scr_gameref_t game, scr_int associate)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (lib_is_version_400 (game))
    lib_print_response_object (game,
                               "You can't take anything from ",
                               "I can't take anything from ",
                               "%player% can't take anything from ",
                               associate, ".");
  else
    pf_buffer_string (filter, lib_take_from_verb (game));
}


/*
 * lib_take_from_nothing_inside_pre390()
 *
 * 3.7/3.8's own "There is nothing inside a bag.": the object's raw Prefix
 * and Short (run380 446F60 and 44750A, run370 43AD4D and 43B109), where 3.9
 * names it definitely.  The all/and arm (446F60) says "inside" whatever the
 * object is, and run370 has no other literal, so a surface gets it too:
 * `get all from tray`, the tray an empty supporter, is "There is nothing
 * inside a tray." in run370x and run380x (p37TFSW/p38TFSW,
 * runner_probes/tfsw.run370.feed2.rtf, runner_probes/tfsw.run380.feed2.rtf,
 * 2026-09-19).  Scarier deliberately keeps
 * grammatical text instead (deviation policy): the object is named
 * definitely, and a plain surface has nothing "on" it, not "inside" it.
 */
static void
lib_take_from_nothing_inside_pre390 (scr_gameref_t game, scr_int associate)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (obj_is_surface (game, associate) && !obj_is_container (game, associate))
    pf_buffer_string (filter, "There is nothing on ");
  else
    pf_buffer_string (filter, "There is nothing inside ");
  lib_print_object_np (game, associate);
  pf_buffer_character (filter, '.');
}


/*
 * lib_take_from_nothing_taken_pre390()
 *
 * A pre-3.9 take-from that named objects, none of them in the container.
 * insides() sets its message to "<You> take " before the take loop and,
 * when nothing was added, compares it with "<You> get " (run380 4474C7).
 * run370 spells the take "You get ", so the test holds and the answer is
 * "There is nothing inside a bag."; run380 reworded the take and not the
 * test, so the bare "You take " goes out, trailing space and all.  `get
 * coin from bag` with the coin in hand: run370x "There is nothing inside a
 * bag.", run380x "You take " (p37TFSW/p38TFSW,
 * runner_probes/tfsw.run370.feed2.rtf, runner_probes/tfsw.run380.feed2.rtf,
 * 2026-09-19).  Scarier deliberately gives 3.8 the 3.7 sentence rather
 * than the half-sentence (deviation policy).
 */
static void
lib_take_from_nothing_taken_pre390 (scr_gameref_t game, scr_int associate)
{
  lib_take_from_nothing_inside_pre390 (game, associate);
}


/*
 * lib_take_from_empty()
 *
 * Common error handling for when nothing is taken from a container or
 * supporter object.
 */
void
lib_take_from_empty (scr_gameref_t game, scr_int associate, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!is_except
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    {
      lib_take_from_nothing_inside_pre390 (game, associate);
      return;
    }

  if (obj_is_container (game, associate) && obj_is_surface (game, associate))
    {
      if (gs_object_openness (game, associate) <= OBJ_OPEN)
        {
          if (is_except)
            pf_buffer_string (filter, "There is nothing else in or on ");
          else
            pf_buffer_string (filter, "There is nothing in or on ");
          lib_print_object_np (game, associate);
          pf_buffer_character (filter, '.');
        }
      else
        {
          if (is_except)
            pf_buffer_string (filter, "There is nothing else on ");
          else
            pf_buffer_string (filter, "There is nothing on ");
          lib_print_object_np (game, associate);
          if (gs_object_openness (game, associate) == OBJ_LOCKED)
            pf_buffer_string (filter, " and it is locked.");
          else
            pf_buffer_string (filter, " and it is closed.");
        }
    }
  else
    {
      if (obj_is_container (game, associate))
        {
          if (gs_object_openness (game, associate) <= OBJ_OPEN)
            {
              if (is_except)
                pf_buffer_string (filter, "There is nothing else inside ");
              else
                pf_buffer_string (filter, "There is nothing inside ");
              lib_print_object_np (game, associate);
              pf_buffer_character (filter, '.');
            }
          else
            {
              pf_new_sentence (filter);
              lib_print_object_np (game, associate);
              /* Always " is ": see lib_cmd_examine_object(). */
              pf_buffer_string (filter, " is ");
              if (gs_object_openness (game, associate) == OBJ_LOCKED)
                pf_buffer_string (filter, "locked.");
              else
                pf_buffer_string (filter, "closed.");
            }
        }
      else
        {
          if (is_except)
            pf_buffer_string (filter, "There is nothing else on ");
          else
            pf_buffer_string (filter, "There is nothing on ");
          lib_print_object_np (game, associate);
          pf_buffer_character (filter, '.');
        }
    }
}


/*
 * lib_take_from_has_contents()
 *
 * TRUE if the associate holds anything the take-from filter would accept.
 * 4.0 asks this before it looks at the names the line gave; see
 * lib_take_from_multiple_common().
 */
static scr_bool
lib_take_from_has_contents (scr_gameref_t game, scr_int associate)
{
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_take_from_filter (game, object, associate))
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_take_from_empty_verb()
 *
 * TRUE where the line's verb is "empty" and the game is pre-4.0, in which
 * case the take-from family must decline the line altogether.
 *
 * "empty" is a 4.0 addition.  run390's insides() gate at loc_4627E2 is
 * `( c("from") Or (c("all") And container>0) ) And ( c("get") Or c("remove")
 * Or c("take") Or c("pick") )` -- no "empty" anywhere in it -- and measured
 * live that is exactly what run390 does: `empty box`, `empty torch` and
 * `empty stone` all answer the DontUnderstand catch-all, "I don't understand
 * what you want me to do with the box.", while `empty zzzz` is the bare "I
 * don't understand." (p39DARK, runner_probes/dark.run390.feed4.txt,
 * runner_probes/tfrom.run400.feed2.txt, runner_probes/dark.run390.feed5.txt,
 * 2026-09-10).  run400 on
 * the same feed takes the coin out of the box for `empty box`.
 */
static scr_bool
lib_take_from_empty_verb (scr_gameref_t game)
{
  const scr_char *input;
  scr_int index_;

  if (lib_is_version_400 (game))
    return FALSE;

  input = run_get_dispatch_input ();
  if (!input)
    return FALSE;

  while (*input == ' ')
    input++;
  for (index_ = 0; index_ < 5; index_++)
    {
      if (scr_tolower (input[index_]) != "empty"[index_])
        return FALSE;
    }
  return input[5] == '\0' || input[5] == ' ';
}


/*
 * lib_take_from_answer_370()
 *
 * run370's insides() has no answer for taking from something that is
 * neither a container nor a surface: its tail (43AF92) tests only c("in")
 * and c("on"), so a take-from line leaves the message empty and generaltasks
 * goes on to therest and its catch-all, which names the first object on the
 * line, in index order, that is here.  `get coin from stone` is "I don't
 * understand what you want me to do with the coin." and `get all from
 * stone` "... with the stone." (p37TFSW, run370x
 * runner_probes/tfsw.run370.rtf, 2026-09-19), where run380 says "You can't
 * take anything from the stone!".  A line without get or remove never reaches
 * insides() at all, so it gets the same catch-all whatever the source is -- a
 * gate Scarier deliberately does not apply (see the run370 insides() note in
 * sclibrar_take.cpp).  An object the player has not been shown is "What
 * <Short>?", as in lib_cmd_verb_object().  TRUE once this has answered; FALSE
 * for any other version or object.
 */
static scr_bool
lib_take_from_answer_370 (scr_gameref_t game, scr_int associate)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_380
      || (associate >= 0
          && (obj_is_container (game, associate)
              || obj_is_surface (game, associate))))
    return FALSE;

  /*
   * Two passes, as the catch-all's own: first the object the line names
   * that is present (lib_present_370) and seen, in index order; failing
   * that, "What <Short>?" for a named object the player has not been shown,
   * present or not -- `get nut from gem` with the gem in another room and
   * the nut in a box on the floor is "What gem?" (p37TKA cell 125), where
   * `get nut from statue` names the statue (cell 122) and, with the nut in
   * a held box, the nut (p37TKB cells 165, 167; run370x
   * runner_probes/tka.run370.rtf / runner_probes/tkb.run370.rtf, 2026-09-20).
   */
  gs_clear_multiple_references (game);
  for (object = 0; input && object < gs_object_count (game); object++)
    {
      if (!lib_present_370 (game, object)
          || !gs_object_seen (game, object)
          || !lib_co_pre400 (game, input, object, 0))
        continue;

      var_set_ref_object (gs_get_vars (game), object);
      uip_note_definite_reference ();
      lib_print_wrapped_object (game,
                                "I don't understand what you want me to do with ",
                                object, ".\n");
      lib_non_answer = TRUE;
      return TRUE;
    }
  for (object = 0; input && object < gs_object_count (game); object++)
    {
      if (gs_object_seen (game, object)
          || !lib_co_pre400 (game, input, object, 0))
        continue;

      var_set_ref_object (gs_get_vars (game), object);
      pf_buffer_string (filter, "What ");
      pf_buffer_string (filter, prop_get_indexed_string (bundle, "Objects",
                                                         object, "Short"));
      pf_buffer_string (filter, "?\n");
      return TRUE;
    }
  return FALSE;
}


/*
 * lib_take_from_is_valid()
 *
 * Validate the supporter requested in "take from" commands.
 */
static scr_bool
lib_take_from_is_valid (scr_gameref_t game, scr_int associate)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /*
   * Disallow emptying non-container/non-surface objects.  4.0 ends the
   * sentence with a full stop (run400 mdlSpreadTheLoad loc_4739CA, "."
   * appended at loc_4739E9); 3.8 and 3.9 end it with an exclamation mark
   * (run390 loc_463D6E, "!" at loc_463D9E).  Measured live on the same feed:
   * `get all from torch` is "You can't take anything from the torch!" in
   * run390 and "You can't take anything from the torch." in run400
   * (runner_probes/dark.run390.feed3.txt,
   * runner_probes/tfrom.run400.feed2.txt, 2026-09-10).  run370 has no such
   * literal at all -- its tail at loc_43AF92 tests only c("in") and c("on") --
   * and 3.70 never gets here: its callers answer with the catch-all first
   * (lib_take_from_answer_370).
   */
  if (!(obj_is_container (game, associate)
        || obj_is_surface (game, associate)))
    {
      lib_print_response_object (game,
                                 "You can't take anything from ",
                                 "I can't take anything from ",
                                 "%player% can't take anything from ",
                                 associate,
                                 lib_is_version_400 (game) ? "." : "!");
      return FALSE;
    }

  /* If object is a container, and is closed, reject now. */
  if (obj_is_container (game, associate)
      && gs_object_openness (game, associate) > OBJ_OPEN)
    {
      /*
       * Pre-4.0 names the container and the closure in one sentence of its
       * own (run390 loc_4632D7, literals " can't get anything from " at
       * loc_4632E3 and " as it is closed!" at loc_463302): `get all from box`
       * with the box closed is "You can't get anything from the box as it is
       * closed!" (p39DARK, runner_probes/dark.run390.feed4.txt,
       * runner_probes/dark.run390.feed5.txt, 2026-09-10).  4.0 dropped that
       * literal -- it is in run370, run380 and run390 and in no run400 -- and
       * answers "The box is closed." for every form of the command.
       */
      if (lib_is_version_400 (game))
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, associate);
          /* Always " is ": see openness in lib_cmd_examine_object(). */
          pf_buffer_string (filter, " is closed.");
        }
      else
        lib_print_response_object (game,
                                   "You can't get anything from ",
                                   "I can't get anything from ",
                                   "%player% can't get anything from ",
                                   associate, " as it is closed!");
      return FALSE;
    }

  /*
   * Pre-3.9: a dynamic container or supporter must be held or worn (run380
   * 446CFB).  The closed test at 446D19 runs after it and overwrites its
   * message, so a closed chest on the floor is "as it is closed!", not "not
   * holding" -- p37TFSW/p38TFSW `get stone from chest`, run370x/run380x
   * runner_probes/tfsw.run370.rtf, runner_probes/tfsw.run380.rtf, 2026-09-19.
   */
  if (lib_take_container_unheld (game, associate))
    {
      lib_print_not_holding (game, associate, ".");
      return FALSE;
    }

  /* Associate is a valid target for "take from". */
  return TRUE;
}


/*
 * lib_cmd_take_all_from()
 *
 * Attempt to take all objects contained in or supported by a given object.
 */
scr_bool
lib_cmd_take_all_from (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int associate, objects;
  scr_bool is_ambiguous;

  /* A line with "and" has its own rules in every Runner. */
  if (lib_take_from_and_line (game))
    return lib_take_from_and (game);

  /* Get the referenced object, and if none, consider complete. */
  associate = lib_disambiguate_object (game, "take from", &is_ambiguous);
  if (associate == -1)
    return is_ambiguous;

  /* Validate the associate object to take from; 3.7 has its own answer.
     run380's task sweep follows a refusal too. */
  if (lib_take_from_answer_370 (game, associate))
    return TRUE;
  if (!lib_take_from_is_valid (game, associate))
    {
      lib_take_from_task_sweep_380 (game);
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_take_from_filter, associate, FALSE, nullptr);
  gs_clear_multiple_references (game);
  lib_take_from_single_named = FALSE;
  if (objects > 0)
    lib_take_from_object_backend (game, associate);
  else if (lib_take_from_unseen (game, associate))
    lib_take_from_unseen_refusal (game, associate);
  else
    lib_take_from_empty (game, associate, FALSE);

  lib_take_from_task_sweep_380 (game);
  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_from_no_name()
 *
 * Answer a "take from" whose own noun named nothing the player can reach.
 *
 * run390 decides this before it looks at the container at all: the arm at
 * loc_462FD2 is entered when the line matched fewer than two objects or the
 * named one is not here, and with the name unresolved it prints "You can't
 * do that!" (loc_463140/463165; run380 loc_446B38 and run370 loc_439C3A hold
 * the same literal, and pre-3.9 that arm is the whole of the branch).  The
 * measured turn is `get coin from box` with the coin inside a CLOSED box, so
 * the coin is not reachable: run390 answers "You can't do that!" while `get
 * stone from box` on the very same turn gets the closed-container refusal
 * instead (p39DARK, runner_probes/dark.run390.feed5.txt, 2026-09-10).
 *
 * 4.0 dropped that arm along with the literal, which is in run370, run380 and
 * run390 and in no run400.  There the container has already answered by the
 * time the names are read, so the only thing left is the take handler's own
 * "Take what?".
 */
static scr_bool
lib_take_from_no_name (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return lib_print_message (game, "Take what?\n");

  return lib_print_response_message (game,
                                     "You can't do that!\n",
                                     "I can't do that!\n",
                                     "%player% can't do that!\n");
}


static const scr_char *const LIB_TAKE_FROM_NOWHERE_400 =
    "I don't understand where you want to get things from.\n";

/*
 * lib_take_from_nowhere_named_390()
 *
 * run390's named take-from with no container found (the arm at 462FD2; see
 * lib_cmd_take_from_nowhere()).  Where the named object is in or on
 * something, "Get <the X> from what?" also arms a pending slot, and a bare
 * line typed next reruns as "get <X> from <line>": `get nut from zzz`, then
 * `box`, is "You take a nut from the box." (p39TKB, run390x
 * runner_probes/tkb.run390.txt cells 139/140, 147/148, 158/159, 2026-09-20).
 */
static scr_bool
lib_take_from_nowhere_named_390 (scr_gameref_t game, const scr_char *named)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::string copy;
  scr_int object;
  scr_bool is_ambiguous;

  /* Take a copy; the match below rewrites the referenced text. */
  copy = named ? named : "";
  if (copy.empty () || !uip_match ("%object%", copy.c_str (), game))
    return lib_take_from_no_name (game);

  object = lib_disambiguate_object (game, "take", &is_ambiguous);
  if (object == -1)
    return is_ambiguous ? TRUE : lib_take_from_no_name (game);

  if (gs_object_position (game, object) == OBJ_IN_OBJECT
      || gs_object_position (game, object) == OBJ_ON_OBJECT)
    {
      pf_buffer_string (filter, "Get ");
      lib_print_object_np (game, object);
      pf_buffer_string (filter, " from what?\n");
      lib_battle_who_store (std::string ("get ") + copy + " from");
      return TRUE;
    }

  pf_new_sentence (filter);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, " isn't in or on anything!\n");
  return TRUE;
}


/*
 * lib_take_from_trailing()
 *
 * A take-from whose "from" ends the line.  4.0 answers nothing at all --
 * run400 prints an empty turn for `get nut from` (p4TKB cell 158) -- and
 * 3.9 treats it as a container it could not find: "Get the nut from what?"
 * with the slot armed (p39TKB cell 158).  3.7/3.8 have their one "You can't
 * do that!" (p37TKB/p38TKB cell 158).  runner_probes/tkb.run370.rtf,
 * runner_probes/tkb.run380.rtf, runner_probes/tkb.run390.txt,
 * runner_probes/tkb.run400.txt, 2026-09-20.
 */
scr_bool
lib_take_from_trailing (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  scr_int at;

  if (version >= TAF_VERSION_400)
    return TRUE;
  if (version < TAF_VERSION_390)
    return lib_take_from_no_name (game);

  at = run_c_word_pre400 (version, line, "from");
  if (at < 0)
    return lib_take_from_no_name (game);
  return lib_take_from_nowhere_named_390 (game,
                                          lib_take_from_head (line, at).c_str ());
}


/*
 * lib_take_from_and_line()
 *
 * TRUE for a take-from line the Runners read through their own "and"
 * rules: 4.0 when "and" follows "from" (Proc_19_23_473A34 splits the
 * container clause); pre-4.0 when "and" is anywhere on the line, since
 * insides() tests c("and") over the whole line (run390 462FD2, run380
 * 4468B3).
 */
scr_bool
lib_take_from_and_line (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  scr_int at;

  if (!line)
    return FALSE;
  at = run_c_word_pre400 (version, line, "from");
  if (at < 0)
    return FALSE;
  if (version >= TAF_VERSION_400)
    return run_c_word_pre400 (version, line + at + 4, "and") >= 0;
  return lib_input_contains_word (line, "and");
}


/*
 * lib_take_from_slot_pre390()
 *
 * run380's insides() from-branch (4468B3-44750A; run370 43A745 is the same
 * with Short names only).  It has no parse: every object the line names is
 * walked in index order and the container "slot" is the last one that beat
 * the current holder -- a holder that is not obhere() is replaced outright,
 * one that is only by an object whose highest name position on the line is
 * past the holder's lowest (446A2F-446AE4).  So `get nut from box and bag`
 * reaches the bag and `get nut from bag and box` the box, and `get gem
 * from box` with the gem elsewhere the box: the object named is as good a
 * slot as the container.  Fewer than two names without "all" is "You can't
 * do that!" (446B1D); the slot is then tested as a container (field 29,
 * static in the room, dynamic held, not closed: lib_take_from_is_valid)
 * and its contents collected -- with "and" on the line only the named ones,
 * with "all" every one, else the named ones -- and an empty collection is
 * "There is nothing inside <a slot>." on an and/all line, else 3.7's
 * nothing-inside / 3.8's bare "You take ".
 *
 * Measured on p37TKA/p38TKA (runner_probes/tka.run370.rtf,
 * runner_probes/tka.run380.rtf, cells 47-134, 122-130) and p37TKB/p38TKB
 * (runner_probes/tkb.run370.rtf, runner_probes/tkb.run380.rtf, cells 67-135,
 * 165-181), 2026-09-20.  3.7's catch-all answers first for a slot that is no
 * container (lib_take_from_answer_370).
 */
static scr_bool
lib_take_from_slot_pre390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, count, slot, slot_lowest, any;
  scr_bool is_and, is_all;

  if (!line)
    return FALSE;

  count = 0;
  slot = -1;
  slot_lowest = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_int lowest, highest;

      if (!lib_co_pre400 (game, line, object, 0))
        continue;
      count++;
      if (!lib_name_instr_range (game, line, object, &lowest, &highest))
        continue;
      if (slot == -1 || !lib_obhere_380 (game, slot) || highest > slot_lowest)
        {
          slot = object;
          slot_lowest = lowest;
        }
    }

  is_and = lib_input_contains_word (line, "and");
  is_all = lib_input_contains_word (line, "all");
  if (count < 2 && !is_all)
    return lib_take_from_no_name (game);
  if (slot == -1)
    return lib_print_response_message (game,
                                       "You can't get anything from that.\n",
                                       "I can't get anything from that.\n",
                                       "%player% can't get anything from that.\n");

  if (lib_take_from_answer_370 (game, slot))
    return TRUE;
  if (obj_is_static (game, slot)
      && !obj_indirectly_in_room (game, slot, gs_playerroom (game)))
    {
      pf_buffer_string (filter,
                        lib_select_response (game, "You can't see ",
                                             "I can't see ",
                                             "%player% can't see "));
      lib_print_object (game, slot);
      pf_buffer_character (filter, '.');
      lib_take_from_task_sweep_380 (game);
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  if (!lib_take_from_is_valid (game, slot))
    {
      lib_take_from_task_sweep_380 (game);
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  gs_clear_object_references (game);
  gs_clear_multiple_references (game);
  any = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if ((gs_object_position (game, object) != OBJ_IN_OBJECT
           && gs_object_position (game, object) != OBJ_ON_OBJECT)
          || obj_is_static (game, object)
          || gs_object_parent (game, object) != slot)
        continue;
      if ((is_and || !is_all) && !lib_co_pre400 (game, line, object, 0))
        continue;
      game->object_references[object] = TRUE;
      any++;
    }

  if (any == 0)
    {
      if (is_and || is_all)
        lib_take_from_nothing_inside_pre390 (game, slot);
      else
        lib_take_from_nothing_taken_pre390 (game, slot);
    }
  else
    {
      lib_take_from_single_named = FALSE;
      lib_take_from_object_backend (game, slot);
    }
  lib_take_from_task_sweep_380 (game);
  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_from_and_390()
 *
 * run390's insides() on a line with "and" (462FD2-463E77).  The object
 * named is the first the text before "from" names that is present and
 * seen; the container is the LAST match in the text after it (`get nut from
 * box and bag` reaches the bag); with fewer than two names on the line, or
 * none before "from", and no "all", the answer is the named object's own
 * ("Get <the X> from what?" in or on something, "<The X> isn't in or on
 * anything!" loose or held, "You can't do that!" unnamed: 46309E, 46311A,
 * 463140).  Any "and" sets the collection to the named form (var_CC = 2),
 * which takes only the named contents of the container and complains of
 * nothing: `get nut and bolt from box` is "You take the nut from the box."
 * alone, `get all from box and bag` "There is nothing inside the bag.".
 * Measured on p39TKA (runner_probes/tka.run390.txt, cells 47-134) and p39TKB
 * (runner_probes/tkb.run390.txt, cells 67-181), 2026-09-20.
 */
static scr_bool
lib_take_from_and_390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *line = run_get_dispatch_input ();
  std::string before;
  const scr_char *after;
  scr_int at, object, count, named, container, container_at, any;
  scr_bool is_all;

  at = line ? run_c_word_pre400 (TAF_VERSION_390, line, "from") : -1;
  if (at < 0)
    return FALSE;
  before.assign (line, (size_t) at);
  after = line + at + 4;

  count = 0;
  named = -1;
  container = -1;
  container_at = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_int lowest, highest;

      if (!lib_co_pre400 (game, line, object, 0))
        continue;
      count++;
      if (named == -1 && lib_co_pre400 (game, before.c_str (), object, 0))
        named = object;
      if (lib_co_pre400 (game, after, object, 0)
          && lib_name_instr_range (game, after, object, &lowest, &highest)
          && highest > container_at)
        {
          container = object;
          container_at = highest;
        }
    }

  is_all = lib_input_contains_word (line, "all");
  if ((count < 2 || named == -1) && !is_all)
    {
      if (named == -1)
        return lib_take_from_no_name (game);
      if (gs_object_position (game, named) == OBJ_IN_OBJECT
          || gs_object_position (game, named) == OBJ_ON_OBJECT)
        {
          pf_buffer_string (filter, "Get ");
          lib_print_object_np (game, named);
          pf_buffer_string (filter, " from what?\n");
          lib_battle_who_store (std::string ("get ")
                                + lib_take_from_head (line, at) + " from");
          return TRUE;
        }
      pf_new_sentence (filter);
      lib_print_object_np (game, named);
      pf_buffer_string (filter, " isn't in or on anything!\n");
      return TRUE;
    }

  if (container == -1)
    return lib_print_response_message (game,
                                       "You can't get anything from that.\n",
                                       "I can't get anything from that.\n",
                                       "%player% can't get anything from that.\n");
  if (!lib_take_from_is_valid (game, container))
    {
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  gs_clear_object_references (game);
  gs_clear_multiple_references (game);
  any = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_take_from_filter (game, object, container)
          && lib_co_pre400 (game, line, object, 0))
        {
          game->object_references[object] = TRUE;
          any++;
        }
    }
  if (any == 0)
    lib_take_from_empty (game, container, FALSE);
  else
    {
      lib_take_from_single_named = FALSE;
      lib_take_from_object_backend (game, container);
    }
  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_from_and_400()
 *
 * run400's take-from with "and" after "from": the names before "from" are
 * resolved first, each on its own (a name that resolves to nothing, such
 * as the ring in a closed chest never seen, is "Take what?" before the
 * container is looked at: p4TKB cells 119/121 against 127), and the
 * container is the FIRST clause after "from" (`get nut from box and bag`
 * takes the nut from the box, `get nut from bag and box` is "Take what?"),
 * an unresolvable one being the nowhere refusal.  Then the container's own
 * tests, and only the names it holds move.  p4TKA runner_probes/tka.run400.txt
 * cells 47-134, p4TKB runner_probes/tkb.run400.txt cells 67-181, 2026-09-20.
 */
static scr_bool
lib_take_from_and_400 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *line = run_get_dispatch_input ();
  std::string names, clause;
  std::vector<scr_int> resolved;
  scr_int at, container, objects, references, object;
  scr_bool is_all, tied_any = FALSE;
  size_t start;

  at = line ? run_c_word_pre400 (TAF_VERSION_400, line, "from") : -1;
  if (at < 0)
    return FALSE;

  names = lib_take_from_head (line, at);
  if (names.empty ())
    return lib_print_message (game, "Take what?\n");
  is_all = scr_strcasecmp (names.c_str (), "all") == 0
           || scr_strcasecmp (names.c_str (), "everything") == 0;
  if (!is_all)
    {
      start = 0;
      while (start <= names.size ())
        {
          size_t end = names.find (" and ", start);
          std::string piece;
          std::vector<scr_int> tied;
          size_t from, to;

          piece = names.substr (start, end == std::string::npos
                                       ? std::string::npos : end - start);
          start = end == std::string::npos ? names.size () + 1 : end + 5;
          from = piece.find_first_not_of (' ');
          to = piece.find_last_not_of (' ');
          if (from == std::string::npos)
            continue;
          piece = piece.substr (from, to - from + 1);

          object = lib_take_resolve_400_string (game, piece.c_str (), &tied);
          if (object >= 0)
            resolved.push_back (object);
          else if (object == -1)
            tied_any = TRUE;
        }
      if (resolved.empty () && !tied_any)
        return lib_print_message (game, "Take what?\n");
    }

  /* The container: the first clause after "from". */
  {
    const scr_char *rest = line + at + 4;
    scr_int and_at;

    while (*rest == ' ')
      rest++;
    and_at = run_c_word_pre400 (TAF_VERSION_400, rest, "and");
    clause = and_at >= 0 ? std::string (rest, (size_t) and_at)
                         : std::string (rest);
    while (!clause.empty () && clause[clause.size () - 1] == ' ')
      clause.erase (clause.size () - 1);
  }
  container = lib_verb_object_resolve_400_string (game, clause.c_str (),
                                                  nullptr, TRUE);
  if (container < 0)
    {
      game->is_admin = TRUE;
      return lib_print_message (game, LIB_TAKE_FROM_NOWHERE_400);
    }

  if (!lib_take_from_is_valid (game, container))
    {
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  if (!lib_take_from_has_contents (game, container))
    {
      if (lib_take_from_unseen (game, container))
        lib_take_from_unseen_refusal (game, container);
      else
        lib_take_from_empty (game, container, FALSE);
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  gs_clear_multiple_references (game);
  references = 0;
  if (is_all)
    gs_set_multiple_references (game);
  else
    {
      for (size_t index_ = 0; index_ < resolved.size (); index_++)
        {
          if (!game->multiple_references[resolved[index_]])
            {
              game->multiple_references[resolved[index_]] = TRUE;
              references++;
            }
        }
    }
  objects = lib_apply_filter (game, lib_take_from_filter, container, FALSE,
                              is_all ? nullptr : &references);
  gs_clear_multiple_references (game);
  if (objects == 0)
    {
      if (obj_is_surface (game, container) && !obj_is_container (game, container))
        lib_print_response_object (game,
                                   "You can't take anything from ",
                                   "I can't take anything from ",
                                   "%player% can't take anything from ",
                                   container, ".");
      else
        pf_buffer_string (filter, "Take what?");
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  lib_take_from_single_named = !is_all && resolved.size () == 1;
  lib_take_from_object_backend (game, container);
  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_from_and()
 *
 * The take-from "and" line, by version; see the three above.
 */
scr_bool
lib_take_from_and (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));

  if (version >= TAF_VERSION_400)
    return lib_take_from_and_400 (game);
  if (version >= TAF_VERSION_390)
    return lib_take_from_and_390 (game);
  return lib_take_from_slot_pre390 (game);
}


/*
 * lib_take_from_piece_400()
 *
 * 4.0's get_piece names the piece of a take-from before it looks at the
 * container at all.  The container is resolved first (472E03-472F19; nothing
 * -> "I don't understand where you want to get things from."), then the
 * text before "from" goes to the noun scorer 463640 in mode 1 with the
 * container as its argument (473011): what is visibly in or on the
 * container, then -- nothing scoring -- mode 0's present and seen objects,
 * then every seen one (the 46361D restart).  Only a unique winner reaches the
 * container tests of the tail (473795); a tie answers at 473336/4733BD and a
 * piece naming nothing at 47332B, both before them.  p4WTIE (run400,
 * runner_probes/wtie.run400.from.txt, 2026-09-21), two loose stones with the
 * Short "stone", a ruby and an emerald aliased "gems", a static rope, a held
 * knife and a locked box:
 *
 *   take stone from box     Which stone.  The red stone or the blue stone?
 *   take gems from box      It is not clear which gems you are referring to.
 *   take zzz from box       Take what?
 *   take ruby from box      The box is closed.
 *   take rope from box      The box is closed.
 *   take stone from knife   Which stone.  The red stone or the blue stone?
 *   take zzz from knife     Take what?
 *   take stone from zzz     I don't understand where you want to get things from.
 *
 * and with the box open and empty the three named cells are the same while
 * `take ruby from box` is "There is nothing inside the box.".  The question is
 * administrative and the two flat answers are turns, as for a plain take; the
 * answer `red stone` rebuilds the line into `take red stone from box`.
 * runner_probes/wtie.run400.11.txt turn 16 is the held case: with both stones
 * in hand `take stone from knife` still asks.  The -1 arm's task pre-match and
 * its static refusal loop (473241-47330A) are not modelled; a static the line
 * names is scored like any other object, so it reaches the tail.
 *
 * Returns TRUE when it has answered the line.
 */
static scr_bool
lib_take_from_piece_400 (scr_gameref_t game, scr_int associate)
{
  const scr_char *piece = var_get_ref_text (gs_get_vars (game));
  std::vector<scr_int> marked, tied;
  scr_int object, pending, last_tied, mark_count;

  if (!piece || lib_input_contains_word_400 (piece, "all"))
    return FALSE;

  disambig.resolve_parent_400 = associate;
  object = lib_name_object_resolve_400 (game, piece, 4, &pending, &last_tied,
                                        &marked, &mark_count);
  disambig.resolve_parent_400 = -1;
  if (object == -2)
    object = lib_name_object_resolve_400 (game, piece, 0, &pending,
                                          &last_tied, &marked, &mark_count);
  if (object == -2)
    object = lib_verb_object_resolve_400_string (game, piece, &tied, FALSE);

  if (object >= 0)
    return FALSE;
  if (object == -1)
    return !tied.empty ()
           ? FALSE
           : lib_take_tie_400 (game, piece, pending, last_tied, marked,
                               mark_count);

  gs_clear_multiple_references (game);
  pf_buffer_string (gs_get_filter (game), "Take what?");
  pf_buffer_answer_break (gs_get_filter (game));
  return TRUE;
}

/*
 * lib_take_from_resolved()
 *
 * The take-from handler once the container (associate) is fixed and the
 * names are parsed into multiple_references: validate the container, filter
 * the names by it and answer.
 */
static scr_bool
lib_take_from_resolved (scr_gameref_t game, scr_int associate,
                        scr_bool is_except, scr_int references)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_bool is_400 = lib_is_version_400 (game);
  scr_int objects;

  /* Note single-object takes; the backend prints their prefix raw pre-4.0. */
  lib_take_from_single_named = !is_except && references == 1;

  /* Validate the associate object to take from; 4.0 did it above, and 3.7
     may decline.  run380's task sweep follows a refusal too. */
  if (!is_400 && lib_take_from_answer_370 (game, associate))
    return TRUE;
  if (!is_400 && !lib_take_from_is_valid (game, associate))
    {
      lib_take_from_task_sweep_380 (game);
      pf_buffer_answer_break (filter);
      return TRUE;
    }

  /* As a special case, complain about requests to retain the associate. */
  if (is_except
      && lib_multiple_retains_associate (game, associate, "leave"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_take_from_filter, associate, is_except,
                              &references);
  /*
   * A 4.0 line that named only things the container does not hold has taken
   * nothing and has nothing left to report -- the container already passed
   * every test above -- so the take handler falls to its own "Take what?".
   */
  if (is_400 && !is_except && objects == 0)
    {
      gs_clear_multiple_references (game);
      /*
       * A plain surface has its own line for it: `get nut from table` with the
       * key on the table is "You can't take anything from the table." (p4TKA
       * cell 119), `get nut from tray` the same (p4TKB cell 163), where a
       * container is "Take what?" (run400x runner_probes/tka.run400.txt /
       * runner_probes/tkb.run400.txt, 2026-09-20).
       */
      if (obj_is_surface (game, associate) && !obj_is_container (game, associate))
        {
          lib_print_response_object (game,
                                     "You can't take anything from ",
                                     "I can't take anything from ",
                                     "%player% can't take anything from ",
                                     associate, ".");
          pf_buffer_answer_break (filter);
        }
      else
        pf_buffer_string (filter, "Take what?\n");
      return TRUE;
    }

  if (!is_except && objects == 0 && references > 0
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    {
      gs_clear_multiple_references (game);
      lib_take_from_nothing_taken_pre390 (game, associate);
    }
  else if (objects > 0 || references > 0)
    lib_take_from_object_backend (game, associate);
  else if (lib_take_from_unseen (game, associate))
    lib_take_from_unseen_refusal (game, associate);
  else
    lib_take_from_empty (game, associate, is_except);

  lib_take_from_task_sweep_380 (game);
  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_from_multiple_common()
 *
 * Take the objects inside or on an object and listed in %text%, or -- for
 * is_except -- every one of them but those listed.  Neither is mandatory:
 * plain "take <object>" works fine with containers and surfaces, but they
 * are a standard in Adrift so here they are.
 */
static scr_bool
lib_take_from_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_bool is_400 = lib_is_version_400 (game);
  scr_int associate, references;
  scr_bool is_ambiguous;

  /* A line with "and" has its own rules in every Runner. */
  if (!is_except && lib_take_from_and_line (game))
    return lib_take_from_and (game);

  /* Get the referenced object, and if none, consider complete. */
  associate = lib_disambiguate_object (game, "take from", &is_ambiguous);
  if (associate == -1)
    return is_ambiguous;

  /*
   * run390's container is insides()' slot, not the parser's best match:
   * every object co() names is walked in index order, the first reachable
   * one whose Short or first Alias falls after "from" takes the slot, and a
   * later one replaces it only when its name lies further along the line
   * (462811-462E75, measured on lockedout T22, 8811c2b76).  Deliberate
   * deviation: Scarier keeps the parser's best match at 3.9 too.
   */

  /*
   * 4.0 inspects the container before it ever looks at the names the line
   * gave, and the three answers it can give there outrank anything the names
   * could say.  Measured live on p4TFROM against run400
   * (runner_probes/tfrom.run400.txt, runner_probes/tfrom.run400.feed2.txt,
   * 2026-09-10), with the box the only container in the room:
   *
   *   box closed, coin inside   `get coin from box`   The box is closed.
   *   box closed, stone outside `get stone from box`  The box is closed.
   *   box open and empty        `get stone from box`  There is nothing inside the box.
   *   box open and empty        `get coin from box`   There is nothing inside the box.
   *                             (with the coin in hand -- still the container's answer)
   *   box open, coin inside     `get stone from box`  Take what?
   *
   * so the empty-container line precedes the membership test, and a name the
   * container does not hold is simply dropped.  Pre-4.0 has the opposite order
   * -- run390 answers the same five turns "You can't do that!", "You can't get
   * anything from the box as it is closed!", "The stone is not inside the
   * box!", "The coin is not inside the box!" and "The stone is not inside the
   * box!" (runner_probes/dark.run390.feed4.txt,
   * runner_probes/dark.run390.feed5.txt) -- so the names are parsed first
   * there, exactly as before, and lib_take_from_no_name() carries the first of
   * them.
   */
  if (is_400)
    {
      /* The piece is named first; see lib_take_from_piece_400(). */
      if (!is_except && lib_take_from_piece_400 (game, associate))
        return TRUE;

      if (!lib_take_from_is_valid (game, associate))
        {
          pf_buffer_answer_break (filter);
          return TRUE;
        }

      if (!lib_take_from_has_contents (game, associate))
        {
          if (lib_take_from_unseen (game, associate))
            lib_take_from_unseen_refusal (game, associate);
          else
            lib_take_from_empty (game, associate, is_except);

          pf_buffer_answer_break (filter);
          return TRUE;
        }
    }

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "leave" : "take",
                                   lib_take_from_filter, associate,
                                   &references))
    {
      /* Pre-3.9 has no parse; the names pick a slot of their own, and the
         object named is as good a slot as the container (`get gem from box`
         with the gem elsewhere is "You are not holding a box." with the box
         on the floor, p37TKA/p38TKA cell 126; see lib_take_from_slot_pre390). */
      if (!is_except
          && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
        return lib_take_from_slot_pre390 (game);
      return lib_take_from_no_name (game);
    }
  else if (references == 0)
    return TRUE;

  return lib_take_from_resolved (game, associate, is_except, references);
}


/*
 * lib_cmd_take_from_except_multiple()
 * lib_cmd_take_from_multiple()
 *
 * Facets of lib_take_from_multiple_common().
 */
scr_bool
lib_cmd_take_from_except_multiple (scr_gameref_t game)
{
  return lib_take_from_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_take_from_multiple (scr_gameref_t game)
{
  return lib_take_from_multiple_common (game, FALSE);
}


/*
 * lib_cmd_take_from_nowhere_all()
 * lib_cmd_take_from_nowhere()
 *
 * "take from" where nothing the player can reach answers to the noun after
 * "from".  Both Runners have a dedicated answer for it, and neither is the
 * take handler's "Take what?" that scarier reached instead.
 *
 * 4.0 has one answer for every form of the command, all-form and named-form
 * alike: "I don't understand where you want to get things from." (run400
 * mdlSpreadTheLoad loc_472F1F-472F35, printed once both resolver passes have
 * failed to find a container).  Measured on p4TFROM against run400,
 * `get all from zzzz`, `take all from zzzz`, `pick all from zzzz`, `get stone
 * from zzzz`, `get coin from zzzz`, `empty zzzz`, `remove coin from zzzz`,
 * `get all from me`, `get lamp from me` and `empty me` all answer it
 * (runner_probes/tfrom.run400.txt, runner_probes/tfrom.run400.feed2.txt,
 * 2026-09-10).  "me" is not a container to 4.0, so it lands here too.
 *
 * Pre-4.0 splits the two forms.  The all-form is the fall-through of run390's
 * `If var_8C >= 0` at loc_463176, "You can't get anything from that." at
 * loc_463E77 (run380 loc_4474AB and run370 loc_43B0BE hold the same literal;
 * no run400 does).  run390 answers `get all from zzzz`, `take all from zzzz`,
 * `pick all from zzzz`, `get all from me` and -- the reason a resolvable
 * object is not enough -- `get all from box` typed in the room the box is not
 * in, all with that one line (p39DARK/p39DARK lit-room control,
 * runner_probes/dark.run390.feed3.txt, runner_probes/dark.run390.feed4.txt).
 *
 * The named form is the arm at loc_462FD2, and 3.9 splits it three ways on
 * where the named object is:
 *
 *   named nothing reachable  `get stone from zzzz`   You can't do that!
 *   in or on something       `remove coin from zzzz` Get the coin from what?
 *   held, or loose in a room `get coin from zzzz`    The coin isn't in or on anything!
 *
 * (runner_probes/dark.run390.feed3.txt, runner_probes/dark.run390.feed4.txt,
 * runner_probes/dark.run390.feed5.txt, the second and third turns run with the
 * coin first inside the box and then in hand.)  Both of the last two literals
 * are 3.9 only -- " from what?" and "isn't in or on anything" are in no other
 * Runner -- and run380 loc_446B1D/run370 loc_439C1F show the whole arm
 * collapsed to "You can't do that!" there, so that is what 3.7 and 3.8 print
 * for all three.
 *
 * The "Get X from what?" turn also arms a pending slot in run390 that a later
 * bare line re-prompts from (`empty me`, two turns on, answers "Get the coin
 * from what?" again); that slot is not ported.
 */
scr_bool
lib_cmd_take_from_nowhere_all (scr_gameref_t game)
{
  /* Pre-4.0 has no "empty" verb; see lib_take_from_empty_verb(). */
  if (lib_take_from_empty_verb (game))
    return FALSE;
  if (lib_take_from_and_line (game))
    return lib_take_from_and (game);

  /*
   * A bare `empty` is no take-from: get_outer's Replace wants "empty " with
   * its space, so the line reaches the library untouched and the game's
   * DontUnderstand answers it (onnafa "Nope. Didn't mean a thing to me that
   * didn't.", trickortreat "Sorry. That's not a command used in this
   * adventure." -- p_onnafa_empty, p_tot_empty, run400x 2026-09-15).
   */
  if (lib_is_version_400 (game))
    {
      const scr_char *input = run_get_dispatch_input ();

      while (input && *input == ' ')
        input++;
      if (input && strncmp (input, "empty", 5) == 0
          && input[5 + strspn (input + 5, " ")] == NUL)
        return FALSE;
    }

  /*
   * Not a turn in 4.0: run400 472F31 sets the not-a-turn flag (MemVar_494281)
   * beside this refusal, so no event, walk or counter ticks -- measured on
   * escape_to_new_york turn 187 `get all from gladstone bag` (xoshiro trace
   * 2026-09-12: the Runner drew nothing, Scarier ticked 34 draws).
   */
  if (lib_is_version_400 (game))
    {
      game->is_admin = TRUE;
      return lib_print_message (game, LIB_TAKE_FROM_NOWHERE_400);
    }

  /* Pre-3.9 walks the names for a slot of its own first. */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return lib_take_from_slot_pre390 (game);

  return lib_print_response_message (game,
                                     "You can't get anything from that.\n",
                                     "I can't get anything from that.\n",
                                     "%player% can't get anything from that.\n");
}

scr_bool
lib_cmd_take_from_nowhere (scr_gameref_t game)
{
  /* Pre-4.0 has no "empty" verb; see lib_take_from_empty_verb(). */
  if (lib_take_from_empty_verb (game))
    return FALSE;
  if (lib_take_from_and_line (game))
    return lib_take_from_and (game);

  /*
   * Not a turn in 4.0: run400 472F31 sets the not-a-turn flag (MemVar_494281)
   * beside this refusal, so no event, walk or counter ticks -- measured on
   * escape_to_new_york turn 187 `get all from gladstone bag` (xoshiro trace
   * 2026-09-12: the Runner drew nothing, Scarier ticked 34 draws).
   */
  if (lib_is_version_400 (game))
    {
      game->is_admin = TRUE;
      return lib_print_message (game, LIB_TAKE_FROM_NOWHERE_400);
    }

  /* 3.7 and 3.8 walk the names for a slot of their own (the one "You can't
     do that!" when that finds fewer than two); 3.9 answers by the named
     object, and arms the pending slot. */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return lib_take_from_slot_pre390 (game);

  return lib_take_from_nowhere_named_390 (game,
                                          var_get_ref_text (gs_get_vars (game)));
}


/*
 * lib_take_from_npc_filter()
 *
 * Helper function for deciding if an object may be acquired in this context.
 * Returns TRUE if an object may be acquired, FALSE otherwise.
 */
static scr_bool
lib_take_from_npc_filter (scr_gameref_t game, scr_int object, scr_int associate)
{
  /*
   * To be take-able, an object must be either held or worn by the specified
   * NPC.
   */
  return (gs_object_position (game, object) == OBJ_HELD_NPC
          || gs_object_position (game, object) == OBJ_WORN_NPC)
         && !obj_is_static (game, object)
         && gs_object_parent (game, object) == associate;
}


/*
 * lib_take_from_npc_pre390()
 *
 * Which character a 3.7/3.8 take-from-character line is about.  run380
 * characters()' take arm (44054B) fires for any line holding take, get or
 * pick up, so it speaks first for the LAST character named, here or not;
 * insides() then overwrites it only for a character who is here.  So `take
 * stone from cora`, Cora next door, is "Cora is not here!" (run370x
 * runner_probes/npcamb.run370.one.rtf, run380x
 * runner_probes/npcamb.run380.one.rtf), and with Ann and Bob both guards
 * here and Cora a third, `take stone from guard` is Cora's too
 * (runner_probes/npcamb.run380.rtf, runner_probes/npcamb.run370.b.rtf).  At
 * 3.7 a line without get or remove never reaches insides(), so the take
 * arm's answer stands: `take stone from dave` is "I don't think   would
 * appreciate being handled." (runner_probes/npcamb.run370.one.rtf); Scarier
 * deliberately gives 3.7 run380's gate there (see the run370 insides() note
 * in sclibrar_take.cpp).  3.9 picks the last PRESENT character instead ("Bob
 * is not carrying the stone!", runner_probes/npcamb.run390.txt).
 *
 * TRUE once this has answered.  Otherwise *npc is the character to take
 * from, or -1 to leave it to lib_disambiguate_npc_pick().
 */
static scr_bool
lib_take_from_npc_pre390 (scr_gameref_t game, scr_int *npc)
{
  *npc = -1;
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;

  *npc = lib_last_named_npc (game);
  if (*npc == -1)
    return FALSE;
  if (lib_print_npc_not_here_pre390 (game, *npc))
    return TRUE;

  var_set_ref_character (gs_get_vars (game), *npc);
  return FALSE;
}


/*
 * lib_cmd_take_all_from_npc()
 *
 * Attempt to take all objects held or worn by a given NPC.
 */
scr_bool
lib_cmd_take_all_from_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int associate, objects;
  scr_bool is_ambiguous;

  /* Get the referenced NPC, and if none, consider complete. */
  if (lib_take_from_npc_pre390 (game, &associate))
    return TRUE;
  if (associate == -1)
    associate = lib_disambiguate_npc_pick (game, "take from", &is_ambiguous,
                                           NPC_PICK_LAST);
  if (associate == -1)
    return is_ambiguous;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game, lib_take_from_npc_filter,
                              associate, FALSE, nullptr);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_take_from_npc_backend (game, associate);
  else
    {
      pf_new_sentence (filter);
      lib_print_npc_np (game, associate);
      pf_buffer_string (filter, " is not carrying anything!");
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_take_from_npc_multiple_common()
 *
 * Attempt to take the objects held or worn by an NPC and listed in %text%,
 * or -- for is_except -- every one of them but those listed.
 */
static scr_bool
lib_take_from_npc_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int associate, objects, references;
  scr_bool is_ambiguous;

  /* Get the referenced NPC, and if none, consider complete. */
  if (lib_take_from_npc_pre390 (game, &associate))
    return TRUE;
  if (associate == -1)
    {
      scr_int named = lib_last_named_npc (game);

      associate = lib_disambiguate_npc_pick (game, "take from", &is_ambiguous,
                                             NPC_PICK_LAST);

      /*
       * 3.9, a source naming only an absent character: insides() finds no
       * source here and answers from its no-source arm, as for `get coin
       * from zzzz` -- `take stone from cora`, the stone held and Cora next
       * door, is "The stone isn't in or on anything!" (run390x
       * runner_probes/npcamb.run390.one.txt).
       */
      if (associate == -1 && !is_ambiguous && named != -1 && !is_except
          && lib_is_version_390 (game))
        return lib_cmd_take_from_nowhere (game);
    }
  if (associate == -1)
    return is_ambiguous;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "leave" : "take",
                                   lib_take_from_npc_filter, associate,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_take_from_npc_filter, associate, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_take_from_npc_backend (game, associate);
  else
    {
      pf_new_sentence (filter);
      lib_print_npc_np (game, associate);
      pf_buffer_string (filter, " is not carrying anything");
      if (is_except)
        pf_buffer_string (filter, " else");
      pf_buffer_character (filter, '!');
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * lib_cmd_take_from_npc_except_multiple()
 * lib_cmd_take_from_npc_multiple()
 *
 * Facets of lib_take_from_npc_multiple_common().
 */
scr_bool
lib_cmd_take_from_npc_except_multiple (scr_gameref_t game)
{
  return lib_take_from_npc_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_take_from_npc_multiple (scr_gameref_t game)
{
  return lib_take_from_npc_multiple_common (game, FALSE);
}

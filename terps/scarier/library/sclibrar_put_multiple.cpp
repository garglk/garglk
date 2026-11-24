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
 * Putting objects in and on other objects, the command forms: "put all",
 * the named and multiple-object handlers, and "put X in Y" with no Y here.
 * The backends they run are in sclibrar_put.cpp; what a line does below
 * 4.0 (the "put X" and 3.9 "put X in that" questions, the named handler,
 * the refusal-first sweep) is in sclibrar_put_pre400.cpp.
 */

#include "sclibrar_put_internal.h"


/*
 * lib_put_all_common()
 *
 * Put all objects currently held by the player into a container, or onto a
 * supporter.  One body, because below 3.9 it is one handler; which way a
 * given line runs is lib_put_target_takes_on()'s to say, and the all form
 * is routed by the target's kind exactly as the named one is -- `put all on
 * box` with the box in hand is "You put an torch, a lamp and a table inside
 * the box." on run370 and run380 (runner_probes/surf5.run3[78]0.txt:12).
 */
static scr_bool
lib_put_all_common (scr_gameref_t game, scr_int target, scr_bool typed_on)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_bool is_on = lib_put_target_takes_on (game, target, typed_on);
  const scr_int container = target;
  scr_int objects;
  lib_put_outcome_t outcome;

  /* Validate the target object to put onto or into (deferred -> unhandled). */
  if (is_on ? !lib_put_on_is_valid (game, container)
            : !lib_put_in_is_valid (game, container))
    return lib_put_invalid_result ();

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game, lib_put_except_filter, container, FALSE,
                              nullptr);
  gs_clear_multiple_references (game);

  /*
   * 3.9's insides() tests c("and") AFTER c("all"), so a line holding both
   * is the and-arm, whose count pass keeps only the objects co(obj, 0)
   * names.  p39PUT T29 `put all except coin and stone in cupboard`, the
   * coin and stone lying on the held table, is "Nothing will fit inside
   * the cupboard." (run390x runner_probes/put.run390.txt): neither named
   * object is held or loose in the room, and the table, unnamed, is not a
   * candidate.  (The and-arm also admits worn objects; not modelled.)
   */
  if (lib_is_version_390 (game) && run_get_dispatch_input ()
      && lib_co_contains (run_get_dispatch_input (), "and"))
    {
      scr_int object;

      GS_FOR_EACH_OBJECT (game, object)
        {
          if (game->refs.objects[object]
              && !lib_co_pre400 (game, run_get_dispatch_input (), object, 0))
            {
              game->refs.objects[object] = FALSE;
              objects--;
            }
        }
    }

  outcome = {};
  if (objects > 0)
    outcome = is_on ? lib_put_on_backend (game, container)
                    : lib_put_in_backend (game, container, TRUE);
  else
    {
      /*
       * Nothing to put, and all FOUR Runner generations word it differently
       * -- this is the widest generation split on the put row.  Every arm is
       * now measured on the same probe turn, `put all in box` repeated once
       * the first has emptied the player's hands:
       *
       *   run400  You are carrying nothing!
       *           p4TFROM, runner_probes/tfrom.run400.putin3.txt:20
       *   run390  Nothing will fit inside the box.
       *           p39DARK, runner_probes/dark.run390.putin3.txt:59
       *   run380  You are not carrying anything.
       *           p38DARK, runner_probes/dark.run380.putin4.txt:25
       *   run370  You have nothing to put inside the box.
       *           p37DARK, runner_probes/dark.run370.putin4.txt:25
       *
       * (the 3.7/3.8 halves were driven with a feed authored for this row
       * because none of the three original put feeds ever runs `put all in`
       * with the container in the player's hands.)  The census had guessed the
       * 3.7 wording for both pre-3.9 Runners, on the strength of " have
       * nothing to put inside " sitting in run370 and run380 alike; the
       * measurement refutes that.  run380 does hold the string -- it is just
       * not on this path -- and answers with the flat drop-all wording
       * instead, the same literal lib_print_nothing_held() prints.
       *
       * The surface row was driven on the same day, `put all on table` with
       * the table in hand and the first call having emptied the player:
       *
       *   run400  (nothing; see below)
       *           p4SURF,  runner_probes/surf4.run400.txt:6
       *   run390  You have nothing to put onto the table.
       *           p39SURF, runner_probes/surf4.run390.txt:6
       *   run380  You are not carrying anything.
       *           p38SURF, runner_probes/surf4.run380.txt:6
       *   run370  You have nothing to put inside the table.
       *           p37SURF, runner_probes/surf4.run370.txt:6
       *
       * -- so 3.7's wording really is " have nothing to put inside " whatever
       * the target is, 3.8 shares its flat line with the container row, and
       * only 3.9 tells the two apart.  Note that 3.9 does NOT reach for
       * "Nothing will fit" on a surface, even one already over its capacity:
       * the table held six objects on runner_probes/surf6.run390.txt:6 and the
       * answer was still "You have nothing to put onto the table."
       */
      if (lib_is_version_400 (game))
        {
          /*
           * 4.0 speaks only for hands that are genuinely empty.  With the
           * table itself the only thing carried, `put all on table` says
           * nothing of its own and falls through to the generic catch-all,
           * "I don't understand what you want me to do with the table."
           * (p4SURF, runner_probes/surf4.run400.txt:6), where the same command
           * with nothing at all in hand is "You are carrying nothing!"
           * (p4SURF, runner_probes/surf3.run400.txt:16).  The container row is
           * the same shape, and the shut state of the container changes
           * nothing on either arm: probe PCLOSED, `put all in box` with the
           * box the only thing carried, answers "I don't understand what you
           * want me to do with the box." both shut and just opened
           * (runner_probes/closed.run400.txt:11/13), and with nothing at all
           * in hand `put all in chest` / `put all in box` against SHUT
           * containers on the floor answer "You are carrying nothing!"
           * (runner_probes/closed.run400.feed2.txt:4/5) -- name_object counts
           * the held objects and speaks at 46E5BC only when the count is nil,
           * ahead of any insides call, and so ahead of the closed test.
           * Something else in hand reaches insides and its closed refusal,
           * "The chest is closed!" (runner_probes/closed.run400.feed2.txt:7).
           */
          {
            scr_int index_;

            GS_FOR_EACH_OBJECT (game, index_)
              {
                if (gs_object_position (game, index_) == OBJ_HELD_PLAYER)
                  return FALSE;
              }
          }
          pf_buffer_string (filter,
                            lib_select_response (game,
                                               "You are carrying nothing!",
                                               "I am carrying nothing!",
                                               "%player% is carrying nothing!"));
        }
      else if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
        {
          if (is_on)
            lib_print_response_object (game,
                                       "You have nothing to put onto ",
                                       "I have nothing to put onto ",
                                       "%player% has nothing to put onto ",
                                       container, ".");
          else
            lib_print_wrapped_object (game, "Nothing will fit inside ",
                                      container, ".");
        }
      else if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380)
        pf_buffer_string (filter,
                          lib_select_response (game,
                                               "You are not carrying anything.",
                                               "I am not carrying anything.",
                                               "%player% is not carrying anything."));
      else
        lib_print_response_object (game,
                                   "You have nothing to put inside ",
                                   "I have nothing to put inside ",
                                   "%player% has nothing to put inside ",
                                   container, ".");
    }

  return lib_put_finish (game, outcome);
}


/*
 * lib_cmd_put_all_in()
 * lib_cmd_put_all_on()
 *
 * Facets of lib_put_all_common().
 */
scr_bool
lib_cmd_put_all_in (scr_gameref_t game)
{
  scr_int container;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  container = lib_put_target_pre390 (game, "put that into", &is_ambiguous);
  if (container == -1)
    return is_ambiguous;

  return lib_put_all_common (game, container, FALSE);
}

scr_bool
lib_cmd_put_all_on (scr_gameref_t game)
{
  scr_int supporter;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  supporter = lib_put_target_pre390 (game, "put that onto", &is_ambiguous);
  if (supporter == -1)
    return is_ambiguous;

  return lib_put_all_common (game, supporter, TRUE);
}


/*
 * lib_put_fragment_names_nothing()
 *
 * run400's name_object resolves the fragment with the noun scorer
 * Proc_21_58_463640 in mode 2 -- every object PRESENT, seen or not -- and
 * only the empty answer (&HFF) reaches the clobbering exit at 46E23B.
 * That is a narrower test than the library's own %text% parse above: the
 * parse also applies the put filter, so an object whose name the fragment
 * plainly holds can still fail it.  hub's `put soup in pan` is the case --
 * object "minestrone soup" carries the alias "soup" and scores 1 in the
 * Runner, so run400 names it and never rewrites the line, while Scarier's
 * parse rejects it for sitting inside the can.  Ask the scorer directly.
 */

static scr_bool
lib_put_fragment_names_nothing (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  std::string fragment;
  scr_int index_;

  if (!input || !run_unnamed_put_fragment (input, fragment))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (!obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        continue;
      if (lib_verb_object_name_score (game, index_, fragment.c_str ()) > 0)
        return FALSE;
    }

  return TRUE;
}

/*
 * lib_put_fragment_present_object()
 *
 * The unique highest-scoring object among those merely PRESENT (seen or
 * not) that the unnamed put fragment names -- run400's mode 2 scorer,
 * asked directly rather than through the seen-gated %text% parse above.
 * -1 for none or a tie, exactly like lib_verb_object_resolve_400_string();
 * a tie is left to the ordinary flow (which will itself fail and fall to
 * the unclear-object/rewrite handling below in the caller).
 */
static scr_int
lib_put_fragment_present_object (scr_gameref_t game, const scr_char *fragment)
{
  scr_int index_, object, best, best_count;

  object = -1;
  best = 0;
  best_count = 0;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      scr_int score;

      if (!obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        continue;

      score = lib_verb_object_name_score (game, index_, fragment);
      if (score == 0)
        continue;
      if (score > best)
        {
          object = index_;
          best = score;
          best_count = 1;
        }
      else if (score == best)
        best_count++;
    }

  return best_count == 1 ? object : -1;
}


/*
 * lib_put_already_400()
 *
 * Version 4.0 only.  An object already sitting in the container the player
 * has just named is refused before the container is looked at at all, and
 * the wording turns on whether the CONTAINER is in the player's hands:
 *
 *   box held, coin inside it    The coin is already inside the box!
 *   box on the floor, ditto     You are not holding the coin.
 *
 * -- p4TFROM under run400, `put coin in box` at
 * runner_probes/tfrom.run400.putin.txt:12 and again at :34 with the box
 * shut (still "already inside", so this outranks the closed-container
 * refusal), against runner_probes/tfrom.run400.putin2.txt:30 with the box
 * dropped.  " is already inside " is in run400 and in no earlier Runner;
 * the floor case is the take phase's own skip, which
 * lib_put_implicit_take() already carries into the "You are not holding
 * ..." report.
 *
 * The surface half (is_on) splits the same way on whether the SUPPORTER is
 * in the player's hands.  Measured on p4SURF under run400, 2026-09-12, all
 * with the coin sitting on the table:
 *
 *   table held      put coin on table   The coin is already on the table!
 *                                       (runner_probes/surf1.run400.txt:7,
 *                                       and again at :10 once the coin has
 *                                       been taken and put back)
 *   table on floor  put coin on table   You are not holding the coin.
 *                                       (runner_probes/surf1.run400.txt:18,
 *                                       after `drop table`)
 *
 * and the same pair on the nut and the table,
 * runner_probes/surf2.run400.txt:7/9.  " is already on " is in run400 and in
 * no earlier Runner too.
 *
 * Returns TRUE if it consumed every named object, so the caller is done.
 */
static scr_bool
lib_put_already_400 (scr_gameref_t game, scr_int target, scr_bool is_on)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int position = is_on ? OBJ_ON_OBJECT : OBJ_IN_OBJECT;
  scr_int object, object_count, left;
  scr_bool has_printed;

  if (!lib_is_version_400 (game)
      || !obj_indirectly_held_by_player (game, target))
    return FALSE;

  object_count = gs_object_count (game);
  has_printed = FALSE;
  left = 0;
  for (object = 0; object < object_count; object++)
    {
      if (!game->refs.multiple[object])
        continue;

      if (gs_object_position (game, object) == position
          && gs_object_parent (game, object) == target)
        {
          lib_new_clause (game, has_printed);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object,
                                                  " is", " are"));
          pf_buffer_string (filter, is_on ? " already on " : " already inside ");
          lib_print_object_np (game, target);
          pf_buffer_character (filter, '!');
          game->refs.multiple[object] = FALSE;
          has_printed = TRUE;
          continue;
        }
      left++;
    }

  if (has_printed && left == 0)
    {
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  return FALSE;
}


/*
 * lib_put_shut_in_container_400()
 *
 * The already-inside refusal has to reach an object the ordinary matcher
 * cannot: with the box SHUT and the coin inside it, `put coin in box` is
 * still "The coin is already inside the box!" on run400 (p4TFROM,
 * runner_probes/tfrom.run400.putin.txt:34), where
 * obj_indirectly_in_room() -- and so lib_disambiguate_object_common()'s own
 * gate -- has already ruled the coin out of the room.  Score the %text%
 * fragment against the container's contents directly, and return the single
 * object it names, or -1.
 */
static scr_int
lib_put_shut_in_container_400 (scr_gameref_t game, scr_int container)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *named = var_get_ref_text (vars);
  scr_int object, found;

  if (!named || !obj_indirectly_held_by_player (game, container))
    return -1;

  found = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_position (game, object) != OBJ_IN_OBJECT
          || gs_object_parent (game, object) != container)
        continue;
      if (lib_verb_object_name_score (game, object, named) <= 0)
        continue;
      if (found >= 0)
        return -1;
      found = object;
    }
  return found;
}

/*
 * lib_put_named_400()
 *
 * The 4.0 put noun goes through name_object's own resolver, not the
 * seen-gated %text% matcher: 46E5D8 hands the object fragment (the line
 * up to " in "/" on ") to Proc_21_58_463640 in mode 2, the same two-pass
 * scorer a plain `drop X` uses -- pass 0 over
 * what the player holds, directly or inside something held, pass 1 over
 * everything present, no seen gate in either; see
 * lib_name_object_resolve_400().
 * Probe PPUTTIE (runner_probes/puttie.run400.txt), a box in hand:
 *
 *   put key in box   brass key held, iron key on the floor
 *                    You put the brass key inside the box.   (no prompt)
 *   put key in box   the brass key now inside the held box
 *                    The brass key is already inside the box!
 *   put gem in box   both gems on the floor: a pass-1 tie, the pending
 *                    object cleared by the restore at 46355E
 *                    It is not clear which gem you are referring to.
 *   put coin in box  both coins held: a pass-0 tie, pending object kept
 *                    Which coin.  The gold coin or the silver coin?
 *
 * The all/and/except forms take name_object's own branches and are left to
 * the ordinary parse.  Returns TRUE when the line is answered
 * here (a prompt or the not-clear refusal); otherwise a unique winner is left
 * alone in the multiple references with *REFERENCES 1, or *REFERENCES stays -1
 * for the ordinary parse.
 */
static scr_bool
lib_put_named_400 (scr_gameref_t game, scr_int *references)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *fragment = var_get_ref_text (gs_get_vars (game));
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  *references = -1;
  if (!lib_is_version_400 (game) || !fragment || !run_get_dispatch_input ()
      || lib_input_contains_word_400 (fragment, "all")
      || lib_input_contains_word_400 (fragment, "everything")
      || lib_input_contains_word_400 (fragment, "and")
      || lib_input_contains_word_400 (fragment, "except"))
    return FALSE;

  object = lib_name_object_resolve_400 (game, fragment, 2, &pending,
                                        &last_tied, &marked, &mark_count);
  if (object == -2)
    return FALSE;

  if (object == -1)
    {
      if (pending < 0)
        {
          lib_print_not_clear_which_400
            (game, lib_drop_named_term_400 (game, last_tied, fragment, FALSE));
          pf_buffer_character (filter, '\n');
          return TRUE;
        }

      /* As lib_drop_named_400(): the run-together list is unmeasured. */
      if ((scr_int) marked.size () == mark_count)
        {
          lib_co_400_raise_named (game,
                                  lib_drop_named_term_400 (game, pending,
                                                           fragment, TRUE),
                                  marked);
          return TRUE;
        }
      return FALSE;
    }

  /*
   * Mode 2 has no seen gate, and the Runner's name composer answers "that"
   * for an object still unseen; see lib_put_print_object_or_that().
   */
  if (!gs_object_seen (game, object))
    lib_put_present_unseen = object;
  gs_clear_multiple_references (game);
  game->refs.multiple[object] = TRUE;
  *references = 1;
  return FALSE;
}


/*
 * lib_put_multiple_close()
 *
 * The close the "in" and "on" forms of a multiple put share once the list
 * is parsed: 4.0's take phase, the target's validation, the retained
 * target of an except line, then the filter and the backend.
 */
static scr_bool
lib_put_multiple_close (scr_gameref_t game, scr_int target, scr_bool is_on,
                        scr_bool is_except)
{
  scr_int objects, references;
  lib_put_outcome_t outcome;

  /* 4.0's take phase speaks before the target is examined at all. */
  if (!is_except && lib_put_already_400 (game, target, is_on))
    return TRUE;

  /* Validate the object to put into or onto (deferred -> unhandled). */
  if (!(is_on ? lib_put_on_is_valid (game, target)
              : lib_put_in_is_valid (game, target)))
    return lib_put_invalid_result ();

  /* As a special case, complain about requests to retain the target. */
  if (is_except
      && lib_multiple_retains_associate (game, target, "retain"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              is_except ? lib_put_except_filter
                                        : lib_put_multiple_filter,
                              is_except ? target : -1, is_except,
                              &references);
  outcome = {};
  if (objects > 0 || references > 0)
    outcome = is_on ? lib_put_on_backend (game, target)
                    : lib_put_in_backend (game, target, is_except);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");

  return lib_put_finish (game, outcome);
}


/*
 * lib_put_in_multiple_common()
 *
 * Put the objects held by the player and listed in %text% into an object,
 * or -- for is_except -- every one of them but those listed.  The except
 * form has to keep the container itself out of the list it builds, so it
 * filters on a different predicate.
 */
static scr_bool
lib_put_in_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  scr_int container, references;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  container = is_except
              ? lib_disambiguate_object (game, "put that into", &is_ambiguous)
              : lib_put_target_pre390 (game, "put that into",
                                       &is_ambiguous);
  if (container == -1)
    return is_ambiguous;

  /* Pre-4.0 names the object first, and answers for it first. */
  if (!is_except && !lib_is_version_400 (game))
    return lib_put_named_pre400 (game, container, FALSE);

  /* Parse the multiple objects list to find the target objects. */
  lib_put_admission_guard_t admission_guard;
  references = -1;
  if (!is_except && lib_put_named_400 (game, &references))
    return TRUE;
  scr_bool parsed = references == 1
                    || lib_parse_multiple_objects (game,
                                   is_except ? "retain" : "move",
                                   is_except ? lib_put_except_filter
                                             : lib_put_multiple_resolve_filter,
                                   is_except ? container : -1, &references);

  /*
   * name_object's scorer (463640) counts every object the player has seen,
   * wherever it now lies, and nothing between it and the take asks where the
   * object is.  So run400 fetches a noun naming nothing present but one seen
   * object from that other room: p4LOCK `put gem in jar`, the gem left in
   * Beta, answers "(Taking the gem first)" / "You put the gem inside the jar."
   * (runner_probes/lock.run400.txt), and the archived britishfox, 3monkeys and
   * house transcripts (runner_transcripts/) each lean on it once.  Scarier
   * deliberately does NOT follow (deviation): putting an object that is not in
   * the room is a Runner bug, no game needs it -- every affected route can
   * walk to the object and carry it instead -- and the noun falls to the
   * clobbering exit below like an unseen one.  The same goes for "put X on Y",
   * see lib_put_on_multiple_common().  What IS still recovered here is a
   * fragment naming something present that the seen-gated %text% parse above
   * could not see.
   */
  if (!parsed && !is_except && lib_is_version_400 (game))
    {
      const scr_char *input = run_get_dispatch_input ();
      std::string fragment;

      if (input && run_unnamed_put_fragment (input, fragment)
          && !lib_put_fragment_names_nothing (game))
        {
          /*
           * The fragment names something present after all -- just not
           * anything the seen-gated %text% parse above could see.  Ask
           * the scorer directly, the same way run400's own mode 2 does;
           * see lib_put_fragment_present_object().
           */
          const scr_int present =
            lib_put_fragment_present_object (game, fragment.c_str ());

          if (present >= 0 && !obj_is_static (game, present))
            {
              /*
               * Only the genuinely unseen case prints as "that" --
               * lib_put_fragment_present_object() ignores seen state
               * when it searches, so it can also recover an object the
               * top parse missed for some other reason while it was
               * already seen (measured on TheADRIFTProject:
               * `put battery in remote` finds an already-seen battery
               * this way and must still name it, not say "that").
               */
              if (!gs_object_seen (game, present))
                lib_put_present_unseen = present;
              gs_clear_multiple_references (game);
              game->refs.multiple[present] = TRUE;
              references = 1;
              parsed = TRUE;
            }
        }
    }

  if (!parsed)
    {
      /*
       * ... unless the container is holding it.  A shut container's contents
       * are out of the matcher's reach but not out of the take phase's; see
       * lib_put_shut_in_container_400().
       */
      if (!is_except && lib_is_version_400 (game))
        {
          const scr_int shut = lib_put_shut_in_container_400 (game, container);

          if (shut >= 0)
            {
              gs_clear_multiple_references (game);
              game->refs.multiple[shut] = TRUE;
              if (lib_put_already_400 (game, container, FALSE))
                return TRUE;
              gs_clear_multiple_references (game);
            }
        }

      /*
       * A 4.0 "put X in Y" whose X names nothing is run400's 46E142, the one
       * exit of name_object that leaves the command line rewritten to the
       * fragment "put X " -- see run_priority_unnamed_put_object().  The
       * container has to be a real container for the Runner to have got that
       * far: name_object turns anything else away with "can't put anything
       * inside X!", along the exit that puts the line back.  The except form
       * takes the "all" branch and never reaches the clobber.
       */
      if (!is_except
          && lib_is_version_400 (game)
          && run_in_priority_pass ()
          && obj_is_container (game, container)
          && lib_put_fragment_names_nothing (game))
        {
          /*
           * With no put/drop-class task pre-matching the typed line the Runner
           * speaks here and the line is done, a turn: p4PUT `put zzz in box`
           * -> "It is not clear which object you are referring to." and the
           * probe's ticker fires (runner_probes/put.run400.txt).  With one, it
           * stays silent and the tasks get the fragment.
           */
          if (!lib_task_prematches_input (game, 2))
            {
              /* The whole word "drop" picks "Drop what?" (p4PUT
               * `drop zzz in box`, also a turn). */
              if (lib_input_contains_word (run_get_dispatch_input (), "drop"))
                return lib_what (game, "Drop");
              return lib_cmd_unclear_object (game);
            }
          run_priority_unnamed_put_object ();
        }
      return FALSE;
    }
  else if (references == 0)
    return TRUE;

  return lib_put_multiple_close (game, container, FALSE, is_except);
}


/*
 * lib_cmd_put_in_nowhere()
 * lib_cmd_put_on_nowhere()
 *
 * "put X in Y" -- or "put X on Y" -- where nothing present answers to Y.  The
 * %object% rows above cannot match such a line at all, so without this it
 * falls all the way through to the game's DontUnderstand; run390 answers it,
 * and the answer depends on whether X named anything:
 *
 *   `put lamp in box`  (the box a room away)   Put the lamp inside what?
 *   `put coin in me`   ("me" is not an object) Put the coin inside what?
 *   `put stone in box` (both a room away)      You can't do that!
 *   `put coin in zzzz`                         Put the coin inside what?
 *   `put zzzz in box`  (via the %object% row)  You can't do that!
 *
 * (p39DARK, runner_probes/dark.run390.putin.txt:64,
 * runner_probes/dark.run390.putin2.txt:67/70,
 * runner_probes/dark.run390.putin3.txt:24/27/30.)  The surface row
 * is the same in its own preposition -- `put lamp on table` with the table a
 * room away is "Put the lamp onto what?", `put stone on table` with both a
 * room away is "You can't do that!", and so is `put coin on zzzz` with the
 * coin unseen on the table (p39SURF, runner_probes/surf3.run390.txt:5/7 and
 * runner_probes/surf2.run390.txt:15).  The object's failure
 * outranks the container's, which is why the noun is resolved here before the
 * prompt is printed.  4.0 has its own answers for the same shapes, up in
 * lib_cmd_put_container_400(); the prompt is composed from pieces every Runner
 * holds, so the census cannot date it and it is gated at 3.9 -- see
 * lib_put_what_pre400().
 *
 * A table of its own, run after STANDARD_COMMANDS has had its go so that a
 * line naming a real container never reaches it; see STANDARD_PUT_COMMANDS.
 */
static scr_bool
lib_put_nowhere_common (scr_gameref_t game, scr_bool typed_on)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  std::string named;
  scr_int object;
  scr_bool is_ambiguous;

  if (lib_is_version_400 (game))
    return FALSE;

  /* Take a copy; the match below rewrites the referenced text. */
  named = var_get_ref_text (vars);

  /* What "put all in/on <nothing>" answers before 4.0 is unmeasured. */
  if (uip_match ("[all/everything]", named.c_str (), game))
    return FALSE;

  /*
   * 3.7 and 3.8 have no composed prompt to reach for, and no interest in what
   * the object fragment named: a container fragment that names nothing
   * anywhere in the game ends the line flat.  `put coin in zzzz` with the coin
   * lying in the box (runner_probes/dark.run3[78]0.putin2.txt:72) and `put
   * coin in me` with the coin held (runner_probes/dark.run3[78]0.putin.txt:66)
   * both answer "You can't do that!".
   * Note that a fragment naming a container the player merely cannot see no
   * longer arrives here at all -- those are resolved over the whole game now;
   * see lib_put_target_pre390().
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return lib_put_no_object_pre400 (game);

  if (!uip_match ("%object%", named.c_str (), game))
    return lib_put_no_object_pre400 (game);

  object = lib_disambiguate_object (game, "put", &is_ambiguous);
  if (object == -1)
    return is_ambiguous ? TRUE : lib_put_no_object_pre400 (game);

  return lib_put_what_pre400 (game, object, typed_on);
}

scr_bool
lib_cmd_put_in_nowhere (scr_gameref_t game)
{
  return lib_put_nowhere_common (game, FALSE);
}

scr_bool
lib_cmd_put_on_nowhere (scr_gameref_t game)
{
  return lib_put_nowhere_common (game, TRUE);
}


/*
 * lib_cmd_put_in_except_multiple()
 * lib_cmd_put_in_multiple()
 *
 * Facets of lib_put_in_multiple_common().
 *
 * Before 4.0 insides() has no exception list at all: "except" and "but"
 * appear only in the take and drop handlers (run380 43D8EE / 438793, run390
 * drops 4456AB; run370 has neither word), so `put all except X in Y` is
 * c("all")'s arm and puts everything.  p37PUT / p38PUT / p39PUT `put all
 * except stone in cupboard` put the stone too (run370x / run380x / run390x
 * runner_probes/put.run3[78]0.feed2.rtf, runner_probes/put.run390.txt).
 * Deliberate deviation: Scarier keeps the exception at every version, in
 * and on alike.
 */
scr_bool
lib_cmd_put_in_except_multiple (scr_gameref_t game)
{
  return lib_put_in_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_put_in_multiple (scr_gameref_t game)
{
  return lib_put_in_multiple_common (game, FALSE);
}


/*
 * lib_put_on_multiple_common()
 *
 * Put the objects held by the player and listed in %text% onto an object,
 * or -- for is_except -- every one of them but those listed.  As with
 * putting in, the except form filters the supporter itself out of the list.
 */
static scr_bool
lib_put_on_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  scr_int supporter, references;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  supporter = is_except
              ? lib_disambiguate_object (game, "put that onto", &is_ambiguous)
              : lib_put_target_pre390 (game, "put that onto", &is_ambiguous);
  if (supporter == -1)
    return is_ambiguous;

  /* Pre-4.0 names the object first, and answers for it first. */
  if (!is_except && !lib_is_version_400 (game))
    return lib_put_named_pre400 (game, supporter, TRUE);

  /* Parse the multiple objects list to find the target objects; the 4.0
   * noun goes through name_object's resolver first (lib_put_named_400). */
  lib_put_admission_guard_t admission_guard;
  references = -1;
  if (!is_except && lib_put_named_400 (game, &references))
    return TRUE;
  if (references == 1)
    ;
  else if (!lib_parse_multiple_objects (game, is_except ? "retain" : "move",
                                        is_except
                                        ? lib_put_except_filter
                                        : lib_put_multiple_resolve_filter,
                                        is_except ? supporter : -1,
                                        &references))
    {
      /*
       * name_object is one routine for "in" and "on", so run400 fetches a
       * seen object lying elsewhere here too (warlord `put globe on stove`,
       * the globe hidden by the wrap-in-hankie task, "(Taking the
       * foul-smelling globe first)").  Scarier deliberately does not; see
       * lib_put_in_multiple_common().  In Scarier the globe is held after
       * `unwrap globe`, so warlord's golden never needed it.
       */
      return FALSE;
    }
  else if (references == 0)
    return TRUE;

  return lib_put_multiple_close (game, supporter, TRUE, is_except);
}


/*
 * lib_cmd_put_on_except_multiple()
 * lib_cmd_put_on_multiple()
 *
 * Facets of lib_put_on_multiple_common().
 */
scr_bool
lib_cmd_put_on_except_multiple (scr_gameref_t game)
{
  return lib_put_on_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_put_on_multiple (scr_gameref_t game)
{
  return lib_put_on_multiple_common (game, FALSE);
}

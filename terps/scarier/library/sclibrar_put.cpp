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
 * Putting objects in and on other objects: the filters, the implicit take,
 * the "in" and "on" backends and their validity tests.  The command forms
 * that run them ("put all", the named and multiple-object handlers) are in
 * sclibrar_put_multiple.cpp.
 *
 * See sclibrar.h for what the library files share, sclibrar_internal.h for
 * what the core files share and sclibrar_put_internal.h for what the two
 * put files share.
 */

#include "sclibrar_put_internal.h"

/*
 * lib_check_put_recursion()
 *
 * Checks for infinite recursion when placing an object in, or for is_on
 * onto, an object.  Returns TRUE if no recursion detected.
 */
static scr_bool
lib_check_put_recursion (scr_gameref_t game, scr_int object, scr_int target,
                         scr_bool report, scr_bool is_on)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *const preposition = is_on ? "onto" : "inside";
  scr_int check;

  /* Avoid the obvious possibility of infinite recursion. */
  if (target == object)
    {
      if (report)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can't put an object ",
                                                 "I can't put an object ",
                                                 "%player% can't put an object "));
          pf_buffer_string (filter, preposition);
          pf_buffer_string (filter, " itself!");
        }
      return FALSE;
    }

  /* Avoid the subtle possibility of infinite recursion. */
  check = target;
  while (gs_object_position (game, check) == OBJ_ON_OBJECT
         || gs_object_position (game, check) == OBJ_IN_OBJECT)
    {
      check = gs_object_parent (game, check);
      if (check == object)
        {
          if (report)
            {
              pf_buffer_string (filter,
                                lib_select_response (game,
                                                     "You can't put an object ",
                                                     "I can't put an object ",
                                                     "%player% can't put an object "));
              pf_buffer_string (filter, preposition);
              pf_buffer_string (filter, " one it's on or in!");
            }
          return FALSE;
        }
    }

  /* No infinite recursion detected. */
  return TRUE;
}


/*
 * An object present but still unseen, resolved for the rest
 * of that one command by asking the scorer directly instead of the seen-gated
 * %text% parse; see lib_put_fragment_present_object() and
 * lib_put_in_multiple_common().  Named so lib_put_print_object_or_that() can
 * tell this one object to print as "that" rather than by name -- the object
 * genuinely remains unseen, and the Runner's name composer answers "that"
 * for any unseen object it is asked to print (see the "Getting off" comment
 * above lib_cmd_go_*()).
 */
scr_int lib_put_present_unseen = -1;

/*
 * lib_put_print_object_or_that()
 *
 * lib_print_object_np(), except for the one object this command resolved
 * despite it being unseen: that one prints as "that", matching the Runner's
 * name composer.  Measured on hub (run400, 2026-09-17): the minestrone soup,
 * revealed only by task 11 opening the tin can with no listing of its own,
 * answers `put soup in pan` with "I put that inside the saucepan." rather
 * than naming the soup.
 */
static void
lib_put_print_object_or_that (scr_gameref_t game, scr_int object)
{
  if (object == lib_put_present_unseen)
    pf_buffer_string (gs_get_filter (game), "that");
  else
    lib_print_object_np (game, object);
}

/*
 * lib_put_named_filter()
 * lib_put_all_filter()
 *
 * The two universes a "put" command draws its objects from.  They are not
 * the same set, and the Runners' put handlers make the split explicitly.
 *
 * Naming an object (run400's Proc_19_41, selection code at loc_46E01F on)
 * resolves it with the general object matcher and then applies *no* position
 * test whatsoever before running it -- the execute loop's guard reads
 * "var_AC(obj) = 1 Or (<all-matched> And ...)", so a named object arrives
 * pre-approved.  Anything the player can see in the room is therefore fair
 * game, and 4.0 quietly picks it up first (see lib_put_implicit_take).  The
 * pre-4.0 handlers are stricter but still not held-only: run390's
 * loc_461C66 accepts field22 = 0 (held) or field22 = the worn marker, so a
 * worn object may be put down without removing it first.
 *
 * "all" is narrower, and narrower again at 4.0 than before it.  run400's
 * candidate loop takes objects with field26 = 0 And field24 = 0, i.e. lying
 * in the player's hands, never worn and never inside anything; that is the
 * set "put all in X" and "put all except Y on Z" range over there, and it is
 * what the "You are not holding ..." leftover report is phrased for.  run390
 * also takes what is lying loose in the room: on p39DARK
 * (runner_probes/dark.run390.putin3.txt:50, 2026-09-12), with the torch, lamp
 * and coin in hand and the stone and the pebble on the cave floor, `put all in
 * box` answers "You put the torch, the lamp, the stone, the pebble and the
 * coin inside the box."  It stops there -- once every object is inside the
 * box, the same command answers "Nothing will fit inside the box."
 * (runner_probes/dark.run390.putin3.txt:59), the empty-list message, so the
 * contents of a container standing in the room are NOT candidates.  The 4.0
 * control is runner_probes/tfrom.run400.putin3.txt:11: `put all in box` with
 * the stone, pebble and coin loose in the room moves only the two held
 * objects.
 */
scr_bool
lib_put_named_filter (scr_gameref_t game, scr_int object)
{
  /*
   * A static is named too, at 4.0.  There is no static test at name time --
   * the one that turns the piece away lives in `insides` (@465ED7) and is
   * silent -- so the line is claimed and the take piece runs on it first.
   * Probe PSTAT (runner_probes/pstat.run400.txt,
   * runner_probes/pstat.run400.b.txt): `put anvil in box`, the anvil a static
   * lying in the room, answers "(Taking the anvil first)" / "You can't take
   * the anvil!" where scarier used to fall through to the game's
   * DontUnderstand.  Pre-4.0 handlers never see a static.
   */
  if (obj_is_static (game, object))
    return lib_is_version_400 (game)
           && obj_indirectly_in_room (game, object, gs_playerroom (game));

  if (lib_is_version_400 (game))
    return obj_indirectly_in_room (game, object, gs_playerroom (game));

  /*
   * Pre-4.0 is wider than "held or worn" as well.  run390 takes an object
   * lying loose in the room -- there is no implicit take and no announcement,
   * the put simply runs -- and one sitting inside a container the player is
   * carrying.  Measured on p39DARK against run390 (2026-09-12):
   *
   *   runner_probes/dark.run390.putin2.txt:55
   *                   pebble loose on the cave floor, box in hand, `put
   *                   pebble in box` -> "You put the pebble inside the box."
   *   runner_probes/dark.run390.putin.txt:31
   *                   box in hand with the coin inside it, `put coin in box`
   *                   -> "You put the coin inside the box." (a no-op move,
   *                   but the Runner says it all the same)
   *
   * and it is only this wide: the coin inside the box once the box is on the
   * FLOOR is refused, "You can't see that."
   * (runner_probes/dark.run390.putin2.txt:40), so the reach is one step from
   * the player's hands, not the whole room's contents.  See
   * lib_put_not_reachable_pre400() for that refusal.
   *
   * 3.7 and 3.8 are narrower again, and the step into the player's own
   * container is exactly what they lack: a container's contents are out of
   * the put universe no matter whose hands the container is in.  Measured on
   * p38DARK under run380 and p37DARK under run370, 2026-09-12, the box held
   * and open with the coin inside it -- `put coin in box` answers "You can't
   * see that." (runner_probes/dark.run380.putin.txt:18/24/33 and
   * runner_probes/dark.run370.putin.txt:18/24/33, and again with the box
   * brought into the lit room, runner_probes/dark.run380.putin4.txt:28 /
   * runner_probes/dark.run370.putin4.txt:28), where run390 moves the coin and
   * says so.  Loose room objects stay reachable on both: `put pebble in box`
   * -> "You put the pebble inside the box."
   * (runner_probes/dark.run380.putin2.txt:57 /
   * runner_probes/dark.run370.putin2.txt:57).  So run390's one step of reach
   * into a carried container is a 3.90 addition.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return gs_object_position (game, object) == OBJ_HELD_PLAYER
           || gs_object_position (game, object) == OBJ_WORN_PLAYER
           || obj_directly_in_room (game, object, gs_playerroom (game));

  return gs_object_position (game, object) == OBJ_HELD_PLAYER
         || gs_object_position (game, object) == OBJ_WORN_PLAYER
         || obj_indirectly_held_by_player (game, object)
         || obj_directly_in_room (game, object, gs_playerroom (game));
}


/*
 * lib_put_resolve_filter()
 *
 * The tie-break a named put uses when more than one object present answers
 * to the word the player typed.  It is lib_put_named_filter() with the
 * statics taken back out again: a static reaches the handler, and is turned
 * away there (see lib_put_implicit_take), but it never wins a name it shares
 * with something the player can actually move.
 *
 * Measured in three run400 replays, none of which raises the 4.0 "Which X."
 * prompt on the line:
 *
 *   easter      runner_probes/easter.run400.txt:135
 *               `put egg in basket` -> "You put the creme egg inside the
 *               Easter basket.", with the game's other eggs standing in the
 *               room
 *   helsing     runner_transcripts/helsing.txt:50
 *               `put beads on dance floor` runs the game's own task past a
 *               namesake "beads"
 *   provenance  runner_probes/provenance.run400.txt:1781
 *               `put wood on stump` -> "You place the piece of wood on the
 *               stump.", picking it over the cord of wood
 *
 * Only the resolver narrows this way.  The filter that selects the objects
 * the handler then works on stays wide, so `put anvil in box` -- one static,
 * no rival -- still reaches the take piece (probe PSTAT).
 */
static scr_bool
lib_put_resolve_filter (scr_gameref_t game, scr_int object)
{
  return !obj_is_static (game, object) && lib_put_named_filter (game, object);
}

static scr_bool
lib_put_all_filter (scr_gameref_t game, scr_int object, scr_int associate)
{
  if (obj_is_static (game, object) || object == associate)
    return FALSE;

  if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
    return TRUE;

  return !lib_is_version_400 (game)
         && obj_directly_in_room (game, object, gs_playerroom (game));
}


/*
 * The bytes of "(Taking ... first)" announcements printed by the current put
 * backend; see lib_put_outcome_t's is_announce_only.
 */
static size_t lib_put_announce_bytes = 0;

/*
 * lib_put_reject_reference()
 *
 * Move 'object' out of the object references and into the multiple
 * references, for the "You are not holding ..." report; see
 * lib_put_implicit_take, whose several exits all do this.  Always FALSE, for
 * a direct "return lib_put_reject_reference (...)".
 */
static scr_bool
lib_put_reject_reference (scr_gameref_t game, scr_int object)
{
  game->object_references[object] = FALSE;
  game->multiple_references[object] = TRUE;
  return FALSE;
}


/*
 * lib_put_implicit_take()
 *
 * Version 4.0 only.  Having accepted a named object that the player is not
 * carrying, run400 acquires it before the put proper, printing "(Taking the
 * lamp first)" and then continuing as if it had always been held (loc_46E295
 * on).  The announcement uses Proc_21_31_448710 with mode 0, the definite
 * "the lamp" form, and unlike the key-acquisition wording it carries no
 * "from the chest" clause.  The string appears in run400 and in no earlier
 * Runner, so this is squarely a 4.0 addition.  run400 guards the take with
 * a task match on the unmodified player line (Proc_19_35_453C50 on
 * MemVar_49428C); the caller has just run the equivalent test, so there is
 * nothing left to check here.
 *
 * The announcement is unconditional -- it is printed before the acquisition
 * is attempted, not after it succeeds -- and the acquisition itself is the
 * ordinary take, capacity limits and all.  Measured in run400 with a full
 * rucksack (Provenance, 2026-08-23):
 *
 *   > put raincoat in rucksack
 *   (Taking the raincoat first)
 *   The raincoat is too heavy for you to carry at the moment.  You are not
 *   holding the raincoat.
 *
 * Returns TRUE if the put may go ahead, FALSE if the object has been moved
 * to the multiple references for that "You are not holding ..." report.
 * *printed says whether anything was written, so the caller can join the
 * report to it with the usual two spaces.
 */
static scr_bool
lib_put_implicit_take (scr_gameref_t game, scr_int object, scr_int target,
                       scr_bool *printed)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!lib_is_version_400 (game))
    return TRUE;

  if (obj_indirectly_held_by_player (game, object))
    return TRUE;

  /*
   * The acquisition is skipped when the object already sits at the
   * destination -- run400 tests obj.global_46 <> var_92, the container or
   * surface it is about to be moved to -- and the put executor's own
   * possession test then refuses it (Proc_21_46_44615C at loc_465EED).  So
   * "put ball on marker" with the ball already on the marker answers "You
   * are not holding the golf ball.", which is what the multiple-reference
   * report is phrased for.
   */
  if (gs_object_parent (game, object) == target
      && (gs_object_position (game, object) == OBJ_IN_OBJECT
          || gs_object_position (game, object) == OBJ_ON_OBJECT))
    return lib_put_reject_reference (game, object);

  /*
   * The take is also skipped, silently, when the TYPED line pre-matches a
   * task (run400 name_object Proc_19_41_46E5D8 @46E2C7: Proc_19_35_453C50
   * on MemVar_49428C, the line as the synonym table left it, with the
   * task's restrictions passing or failing on a restriction that has a
   * message -- House's task 60 "* %object%", restricted to the spinning
   * house with no message, is NOT a hit, and run400 goes on to "(Taking the
   * wood first)"; runner_probes/house.run400.wood.txt); the object then
   * reaches the put handler unheld and draws its "not holding" refusal, or --
   * if the handler's own canonical look-up claims -- the task.  The task the
   * player spelled out is what the refusal hands the line to afterwards (see
   * run_all_commands()).
   */
  if (lib_task_prematches_input (game, 1))
    return lib_put_reject_reference (game, object);

  {
    /*
     * Inside a put list's clause loop the announcement goes to the front of
     * the turn: the Runner prints it straight to its textbox while the
     * clause answers accumulate in a string shown at the end of the turn.
     * See pf_hoist_tail() and run_in_put_clause_loop().
     */
    const size_t hoist_from = pf_buffer_length (filter);

    /*
     * The name is Proc_21_31_448710 mode 0, which answers "that" for an
     * object the player has never seen: fairscare T1 `put doll in fire`,
     * the voodoo doll present but unseen, is "(Taking that first)"
     * (run400x seed 1234, runner_transcripts/fairscare.txt).
     */
    pf_buffer_string (filter, "(Taking ");
    if (gs_object_seen (game, object))
      lib_put_print_object_or_that (game, object);
    else
      pf_buffer_string (filter, "that");
    pf_buffer_string (filter, " first)\n");
    lib_put_announce_bytes += pf_buffer_length (filter) - hoist_from;
    if (run_in_put_clause_loop ())
      pf_hoist_tail (filter, hoist_from);
  }

  /*
   * The announcement is printed by name_object before the take piece runs
   * (@46E2EA-46E30C, vbCrLf included), and the piece then gives the tasks
   * "get the X" first (Proc_19_39_46302C @462AED); a claim ends the take
   * there, and the put handler goes on to test possession as the task
   * left it.
   */
  if (lib_try_game_command_take_definite (game, object))
    {
      if (obj_indirectly_held_by_player (game, object))
        return TRUE;
      *printed = TRUE;
      return lib_put_reject_reference (game, object);
    }

  /*
   * A static piece cannot be taken at all, and the take piece says so in its
   * own words: run400 @47329D builds " can't take " + name + "!", the same
   * exclamation the 4.0 single-take handler ends on.  It does NOT go on to
   * become a leftover -- probe PSTAT command 4 (`put anvil in box` with the
   * coin in hand) prints "You can't take the anvil!" and then the clause
   * separator and nothing at all, where a leftover would have been named --
   * so clear the reference and leave the multiple references alone.
   */
  if (obj_is_static (game, object))
    {
      lib_new_clause (game, FALSE);
      lib_print_response_object (game,
                                 "You can't take ",
                                 "I can't take ",
                                 "%player% can't take ", object, "!");
      game->object_references[object] = FALSE;
      *printed = TRUE;
      return FALSE;
    }

  if (lib_object_too_heavy (game, object))
    {
      lib_new_clause (game, FALSE);
      lib_print_too_heavy (game, object);
    }
  else if (lib_object_too_large (game, object))
    {
      lib_new_clause (game, FALSE);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "Your hands are full.",
                                             "My hands are full.",
                                             "%player%'s hands are full."));
    }
  else
    {
      /*
       * The announcement ends its own line (run400 appends vbCrLf), so the
       * put report that follows starts fresh rather than being joined on.
       */
      gs_object_player_get (game, object);
      gs_set_object_unmoved (game, object, FALSE);
      return TRUE;
    }

  *printed = TRUE;
  return lib_put_reject_reference (game, object);
}


/*
 * lib_put_outcome_t
 * lib_output_length()
 *
 * What a put backend has to tell its caller beyond the object moves.
 * is_refusal_only is the 4.0 size/capacity refusal that leaves the line
 * unclaimed (lib_put_in_refused()); is_silent says the backend printed
 * nothing at all, which at 4.0 also leaves the line unclaimed, with no line
 * ending of its own; is_tasks_only says everything it printed came from
 * the task look-ups, whose output has already ended its line, so the
 * caller's own newline would only add a blank one (shadowpeak `put bottle
 * on altar`, pestilence `put brick on desk`, hub `put saucepan on hob`).
 * Output is measured on the filter buffer, so a task's text counts however
 * it was produced.
 */
static scr_int
lib_output_length (scr_gameref_t game)
{
  const scr_char *buffer = pf_get_buffer (gs_get_filter (game));

  return buffer ? (scr_int) strlen (buffer) : 0;
}

/*
 * lib_put_nothing_carried_400()
 *
 * The report name_object makes when its own loop has left it with nothing to
 * put down: run400 46E5A0, the literal an empty-handed `drop all` prints too
 * (see lib_cmd_drop_all).  It closes the take phase, ahead of the task
 * look-ups `insides` makes, and it is printed only when all three of these
 * hold -- measured on probe PSTAT in run400, runner_probes/pstat.run400.txt,
 * runner_probes/pstat.run400.b.txt:
 *
 *   nothing left to act on    `put coin in box` with the coin in hand moves
 *                             it and says nothing (cmd 5), and `put box in
 *                             box` takes the box first, so its action list
 *                             is not empty either, and again nothing at all
 *                             follows the announcement (cmd 13/18)
 *   nothing left to name      `put coin in box` with the coin already inside
 *                             the box is "You are not holding the coin." and
 *                             no more, empty-handed (cmd 6) or not (cmd 8);
 *                             a refused static is never named at all
 *   the player holds nothing  `put anvil in box` is "You can't take the
 *                             anvil!  You are carrying nothing!" empty-handed
 *                             (cmd 2/11/17) and just "You can't take the
 *                             anvil!" with the coin in hand (cmd 4)
 *
 * Worn does not count as carried: cmd 15 wears the cap, `i` answers "You are
 * wearing a cap, and you are carrying nothing." and the put that follows
 * still reports "You are carrying nothing!".  That is name_object's own
 * universe for "all", field26 = 0 And field24 = 0 -- held, never worn and
 * never inside anything (see lib_put_all_filter).
 *
 * Returns TRUE if it printed.
 */
static scr_bool
lib_put_nothing_carried_400 (scr_gameref_t game, scr_bool has_printed)
{
  scr_int object;

  if (!lib_is_version_400 (game))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (game->object_references[object] || game->multiple_references[object])
        return FALSE;

      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        return FALSE;
    }

  lib_print_clause (game, has_printed,
                    "You are carrying nothing!",
                    "I am carrying nothing!",
                    "%player% is carrying nothing!");
  return TRUE;
}


/*
 * lib_put_in_closed_400()
 *
 * 4.0's closed-container refusal, printed from insides' own place in the
 * put: after name_object's take piece, the tasks() call, the possession and
 * itself tests, and before the size and capacity tests (run400 46639C, the
 * state test at 4661BE = 7 " is locked!" / 4661C9 = 6 " is closed!", ahead
 * of the size test at 466219).  Because the take piece has already run, a
 * put into a shut container still ACQUIRES the object: probe PCLOSED
 * (runner_probes/closed.run400.txt) `put ring in box` with the box held and
 * shut prints "(Taking the ring first)" / "The box is closed!" and `i` then
 * lists the ring; `put coin in chest` with the chest shut on the floor is the
 * same shape, "(Taking the coin first)" / "The chest is closed!", and the coin
 * is held afterwards.  An object already in hand draws the refusal alone (`put
 * stone in box`, "The box is closed!").  Returns TRUE and drops every
 * remaining reference when it refused, so the size and capacity tests see
 * nothing; like those, the refusal leaves the line for the task pass, since
 * insides exits without setting its result byte.
 */
static scr_bool
lib_put_in_closed_400 (scr_gameref_t game, scr_int container,
                       scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, object_count;
  scr_bool any_referenced;

  if (!lib_is_version_400 (game)
      || gs_object_openness (game, container) <= OBJ_OPEN)
    return FALSE;

  any_referenced = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (game->object_references[object])
        {
          game->object_references[object] = FALSE;
          any_referenced = TRUE;
        }
    }
  if (!any_referenced)
    return FALSE;

  lib_new_clause (game, has_printed);
  lib_print_object_np (game, container);
  pf_buffer_string (filter,
                    lib_select_plurality (game, container, " is", " are"));
  if (gs_object_openness (game, container) == OBJ_LOCKED)
    pf_buffer_string (filter, " locked!");
  else
    pf_buffer_string (filter, " closed!");
  return TRUE;
}


/*
 * Set while lib_put_that_390() hands a mid-word target to the named-put
 * pipeline: the objects come from co() alone, and the put is 3.9's and-arm
 * however many there are.
 */
scr_bool lib_put_mid_word_390 = FALSE;

/*
 * lib_put_deferred_stage_400()
 *
 * The part of a put that version 4.0 runs after the take phase, shared by
 * the container and the supporter backends: the "carrying nothing" close to
 * the takes, the task look-ups the take loop deferred into `pending`, and
 * the put-into-itself test.  `is_on` selects the supporter wording and the
 * "on" preposition.  Before 4.0 `pending` is empty and only the first step
 * can act.
 */
static void
lib_put_deferred_stage_400 (scr_gameref_t game, const lib_list_t &pending,
                            scr_int target, scr_bool is_on,
                            scr_bool *has_printed, scr_bool *task_claimed,
                            scr_bool *recursion_rejected)
{
  const scr_char *const preposition = is_on ? "on" : "in";

  /*
   * name_object's own close to the take phase, and then the look-ups it
   * deferred.  run400 takes every named piece first and hands the pair to
   * `insides` only afterwards (@46E34F), so the report above comes out
   * ahead of the first task: probe PSTAT command 12, `put slab in box`
   * with the slab a static and the inventory empty, reads "(Taking the
   * slab first)" / "You can't take the slab!  You are carrying nothing!
   * SLABTASK."
   */
  if (!*recursion_rejected
      && lib_put_nothing_carried_400 (game, *has_printed))
    *has_printed = TRUE;

  for (const scr_int pending_object : pending)
    {
      if (lib_try_game_command_with_object_400 (game, "put", pending_object,
                                                preposition, target))
        {
          game->object_references[pending_object] = FALSE;
          game->multiple_references[pending_object] = FALSE;
          *has_printed = TRUE;
          *task_claimed = TRUE;
        }
    }

  /* The typed line, when the rebuild missed; see
     lib_try_typed_put_line_400(). */
  if (!*task_claimed && pending.size () == 1
      && lib_try_typed_put_line_400 (game))
    {
      game->object_references[pending.front ()] = FALSE;
      game->multiple_references[pending.front ()] = FALSE;
      *has_printed = TRUE;
      *task_claimed = TRUE;
    }

  /*
   * 4.0's self-container test, in insides' own place: after the take phase
   * and the tasks() call at 465EB5, after the possession test at 465EED --
   * so only an object the take left in hand reaches it, a failed take
   * having already moved it to the "You are not holding ..." report -- and
   * before the "already inside", size and capacity tests (arg_10 = arg_C
   * @465FA0, the wording chosen by the target's flags at 465FDA/46600C/
   * 46602A).  Probe PBOXBOX (runner_probes/boxbox.run400.txt): `put box in
   * box` with the box on the floor prints "(Taking the box first)" / "You
   * can't put an object inside itself!" and the box IS taken -- `i` answers
   * "You are carrying a box." -- where the same line with the box held prints
   * only the itself line, and a ring inside the box changes nothing.  Probe
   * PSTAT's silent commands 13 and 18 are not a counter-measurement: there the
   * coin, object #1, sits inside the box, and run400's carried-weight cycle
   * (447680) eats the report; see the deliberate deviation in
   * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
   */
  for (const scr_int pending_object : pending)
    {
      if (!game->object_references[pending_object])
        continue;

      if (!lib_check_put_recursion (game, pending_object, target,
                                    !*has_printed, is_on))
        {
          game->object_references[pending_object] = FALSE;
          *has_printed = TRUE;
          *recursion_rejected = TRUE;
        }
    }
}


/*
 * What a put backend's output amounted to, measured against the buffer
 * length before it began and after its task pass.
 */
static void
lib_put_note_output (scr_gameref_t game, lib_put_outcome_t *outcome,
                     scr_int length_before, scr_int length_after_tasks,
                     scr_bool task_claimed)
{
  outcome->is_silent = lib_output_length (game) == length_before;
  outcome->is_tasks_only = task_claimed
                           && lib_output_length (game) == length_after_tasks;
  outcome->is_announce_only = task_claimed && lib_put_announce_bytes > 0
                              && (size_t) (lib_output_length (game)
                                           - length_before)
                                 == lib_put_announce_bytes;
}


/*
 * lib_put_in_backend()
 *
 * Common backend handler for placing objects in containers.  Places all
 * objects currently referenced in the game into a container, trying game
 * commands first, and then moving other unhandled objects into the container.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
struct lib_put_in_t
{
  const scr_gameref_t game;
  const scr_int container;
  const scr_bool is_all_form;
  const scr_filterref_t filter;
  scr_int object_count, entry_count, capacity, free_space;
  scr_bool has_printed, task_claimed;
  scr_bool static_refused, recursion_rejected, has_moved, nothing_fits_390;
  lib_list_t list, pending;

  lib_put_in_t (scr_gameref_t game_, scr_int container_,
                scr_bool is_all_form_)
    : game (game_), container (container_), is_all_form (is_all_form_),
      filter (gs_get_filter (game_)), object_count (gs_object_count (game_)),
      entry_count (0), capacity (0), free_space (0), has_printed (FALSE),
      task_claimed (FALSE), static_refused (FALSE),
      recursion_rejected (FALSE), has_moved (FALSE),
      nothing_fits_390 (FALSE) {}

  void try_game_commands (void);
  void report_full_390 (void);
  void put_counted_390 (void);
  void put_remaining (void);
  void report_full_pre390 (void);
  void report_nothing_fits_390 (void);
  void report_too_big (void);
  void report_no_room (void);
};

/*
 * Try game commands for all referenced objects first.  If any succeed,
 * remove that reference from the list.  At the same time, check for and
 * weed out any moves that result in infinite recursion.
 */
void
lib_put_in_t::try_game_commands (void)
{
  scr_int object;

  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /*
       * Version 4.0 picks up an object it has been asked to put down, and it
       * does so BEFORE the handler's task look-up.  run400's name_object
       * loop runs the take piece at loc_46E2B5 and only then hands the pair
       * to insides (Proc_19_43_46639C @loc_46E34F), which is where the
       * canonical line reaches the tasks -- so a task that goes on to claim
       * the put still carries the announcement ahead of its own text.
       * Measured on frustrated turns 53-55
       * (runner_transcripts/frustrated.txt): `put small rock on left pan`
       * matches task 511 `put*small*left*` and still opens "(Taking the small
       * rock first)".
       *
       * It runs ahead of the self-container test as well: insides tests
       * possession (Proc_21_46_44615C @465EED) before it tests arg_10 =
       * arg_C (@465FA0), and name_object's take piece has already run by
       * then.  Probe PBOXBOX (runner_probes/boxbox.run400.txt): `put box in
       * box` with the box on the floor prints "(Taking the box first)" / "You
       * can't put an object inside itself!" and the box IS taken -- `i`
       * answers "You are carrying a box." -- where the same line with the box
       * held prints only the itself line, and a ring inside the box changes
       * nothing.  Probe PSTAT's silent commands 13 and 18 are not a
       * counter-measurement: there the coin, object #1, sits inside the box,
       * and run400's carried-weight cycle (447680) eats the report; see the
       * deliberate deviation in test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
       */
      {
        scr_bool take_printed = FALSE;
        const scr_bool is_static = obj_is_static (game, object);

        lib_put_implicit_take (game, object, container, &take_printed);
        has_printed |= take_printed;
        static_refused |= is_static;
      }

      /*
       * The tasks' turn.  At 4.0 this is the handler's own look-up, on the
       * definite canonical line, ahead of the size and capacity tests; see
       * lib_try_game_command_with_object_400().  A matching task it does
       * not reach -- the typed spelling with no canonical twin -- gets the
       * line from run_all_commands() only if the library then refuses the
       * put, joined after the refusal (PUT4, runner_probes/put4.run400.txt).
       * It runs even for an object the take above could not acquire: run400
       * reaches insides' tasks() call at loc_465EB5 before the possession test
       * at loc_465EED, so a claim takes the object back out of the "You are
       * not holding ..." report.  The look-up itself is deferred to the second
       * pass below, so that the whole take phase precedes it.
       */
      if (lib_is_version_400 (game))
        {
          pending.push_back (object);
          continue;
        }

      /*
       * Reject and remove attempts to place objects in themselves.  Pre-4.0
       * never gets this far with a direct self-put -- the callers answer
       * "Put the box inside what?" / "You can't do that!" first (see
       * lib_put_what_pre400) -- so this is the subtle case, the container
       * already inside the object.  The 4.0 test sits after the deferred
       * task pass below.
       */
      if (!lib_check_put_recursion (game, object, container, !has_printed,
                                    FALSE))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
          continue;
        }

      if (lib_try_game_command_with_object (game,
                                            "put", object, "in", container))
        {
          game->object_references[object] = FALSE;
          game->multiple_references[object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
          continue;
        }
    }
}

/*
 * run390's " is full." arm (461E59): after the count and its refusals,
 * before the move, an `inside` put whose container is already holding
 * exactly its capacity answers "<The X> is full." and moves nothing --
 * but only while something on the line still fits, which for a full
 * container means a size of nil.  Size is SizeMultiple raised to the
 * SizeWeight's tens digit, so a size of nil needs a SizeMultiple of nil,
 * which the ADRIFT editor never writes; pPUTFULL39 could not raise the
 * arm and it was read as dead.  It is not: pPUTZERO39 (SizeMultiple 0,
 * a bag of capacity 2 holding a coin and a stone, `put feather in bag`
 * with the feather at SizeWeight 10) is "The bag is full." in run390 and
 * the feather stays in hand (runner_probes/putzero.run390.txt).  The same
 * line with a size-1 pebble is the ordinary "The pebble can't fit inside the
 * bag at the moment.", so the per-object refusal above still comes first,
 * and `put all in bag` with two size-0 objects held is "The bag is full."
 * rather than "Nothing will fit inside the bag." -- the count sees them fit,
 * so the nil-count refusal never speaks.  A surface is barred by the arm's
 * third test (var_E0 = "inside"): the same probe puts three objects onto a
 * plate of capacity 2 without a word.
 *
 * That measurement also settles obj_scale()'s long-standing claim that a
 * multiple of nil gives a size of nil, which nothing had tested.
 */
void
lib_put_in_t::report_full_390 (void)
{
  scr_int object;

  if (lib_is_version_390 (game) && free_space == 0
      && obj_is_container (game, container))
    {
      scr_bool any_fits = FALSE;

      for (object = 0; object < object_count; object++)
        {
          if (game->object_references[object]
              && obj_get_size (game, object) <= free_space)
            any_fits = TRUE;
        }

      if (any_fits)
        {
          for (object = 0; object < object_count; object++)
            game->object_references[object] = FALSE;

          lib_new_clause (game, has_printed);
          lib_print_object_np (game, container);
          pf_buffer_string (filter, " is full.");
          has_printed = TRUE;
        }
    }
}

/*
 * Version 3.9's all/and arm counts before it moves anything: insides()
 * (461AF8-461BDB) walks the objects in index order, counting each whose
 * Size fits in what is left once the ones counted before it are charged,
 * and then (461EE8-46239B) moves the FIRST that many of them, whatever
 * their sizes, capping the list with "  You can't put any more inside
 * the bag as it is full." at the first one left over and saying nothing
 * of the rest.  A count of nil is "Nothing will fit inside the bag."
 * (461E0B), with no object named.  pPUTREF39.taf / pPUTREF39E.taf
 * (--emptybag), run390x, 2026-09-19: `put stone and pebble in bag` with
 * the bag full is "Nothing will fit inside the bag.", as is `put all in
 * bag` (runner_probes/putref.run390.feed4.txt:5/11); with room for two, `put
 * stone and pebble and lamp in bag` is "You put the stone and the lamp
 * inside the bag.  You can't put any more inside the bag as it is full."
 * (runner_probes/putref.run390.feed6.txt:5).  A single object keeps the
 * size/capacity pair below.  The " is full." arm at 461E9D never spoke on
 * these probes, the bag exactly full included, because it wants an object
 * that fits into a container with nothing left; it is modelled just above.
 */
void
lib_put_in_t::put_counted_390 (void)
{
  scr_int object;

  nothing_fits_390 = FALSE;
  if (lib_is_version_390 (game)
      && (is_all_form || entry_count > 1 || lib_put_mid_word_390))
    {
      scr_int fits, spent, left_over, remaining;

      fits = spent = remaining = 0;
      free_space = obj_get_container_free_space (game, container);
      for (object = 0; object < object_count; object++)
        {
          remaining += game->object_references[object] ? 1 : 0;
          if (game->object_references[object]
              && obj_get_size (game, object) <= free_space - spent)
            {
              spent += obj_get_size (game, object);
              fits++;
            }
        }

      /* Nothing fits: leave the references for the refusal below. */
      nothing_fits_390 = fits == 0 && remaining > 0;

      list.clear ();
      left_over = 0;
      for (object = 0; fits > 0 && object < object_count; object++)
        {
          if (!game->object_references[object])
            continue;

          game->object_references[object] = FALSE;
          if (static_cast<scr_int> (list.size ()) < fits)
            {
              list.push_back (object);
              gs_object_move_into (game, object, container);
            }
          else
            left_over++;
        }

      if (!list.empty ())
        {
          lib_print_clause (game, has_printed,
                            "You put ", "I put ", "%player% put ");
          lib_print_list (game, list, lib_put_print_object_or_that, " and ");
          lib_print_wrapped_object (game, " inside ", container, ".");
          if (left_over > 0)
            {
              pf_buffer_string (filter, "  ");
              lib_print_response_object (game,
                                         "You can't put any more inside ",
                                         "I can't put any more inside ",
                                         "%player% can't put any more inside ",
                                         container, " as it is full.");
            }
          has_printed = TRUE;
          has_moved = TRUE;
        }
    }
}

void
lib_put_in_t::put_remaining (void)
{
  scr_int object;

  /* Put in every object that remains referenced. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      scr_int size;

      if (!game->object_references[object])
        continue;

      /* If too big, or no longer room for it, ignore for now. */
      size = obj_get_size (game, object);
      if (size > capacity || size > free_space)
        continue;
      free_space -= size;

      list.push_back (object);
      gs_object_move_into (game, object, container);
      game->object_references[object] = FALSE;
    }

  if (!list.empty ())
    {
      lib_print_clause (game, has_printed,
                        "You put ",
                        "I put ",
                        "%player% put ");
      /*
       * Pre-3.9's "put all in" names what it moved with the raw Prefix &
       * " " & Short, where its named twin one row up runs the same objects
       * through tense() and gets the definite form.  p38DARK/p37DARK, box
       * held, torch (Prefix "an") and lamp (Prefix "a") in hand: `put all
       * in box` -> "You put an torch and a lamp inside the box."
       * (runner_probes/dark.run380.putin4.txt:22,
       * runner_probes/dark.run370.putin4.txt:22, 2026-09-12), against `put
       * torch in box` -> "You put an torch inside the box." and `put pebble in
       * box` -> "You put the pebble inside the box." on the same probes
       * (runner_probes/dark.run380.putin2.txt:60/57).  Only the list is raw --
       * the container that follows it is definite in both.  3.9 and 4.0
       * normalise throughout ("the torch, the lamp, ..."
       * runner_probes/dark.run390.putin3.txt:50, "the torch and the lamp"
       * runner_probes/tfrom.run400.putin3.txt:11), so this is the pre-3.9
       * all-form alone.  Scarier deliberately names them definitely there too
       * (deviation policy): "You put an torch and a lamp inside the box." is
       * the raw prefix leaking into a success report.
       */
      lib_print_list (game, list, lib_put_print_object_or_that, " and ");
      pf_buffer_string (filter, " inside ");
      lib_print_object_np (game, container);
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();
  has_moved = !list.empty ();
}

/*
 * Version 3.8 has one container refusal and one only.  run380 answers "The
 * box is full." to everything it will not take in -- whether the container
 * has no room left, or (with a Capacity of 0) never had any, and whatever
 * the object's Size/weight class, which it does not charge against the
 * container at all (measured 2026-08-03; see obj_get_container_capacity).
 * Consume the leftovers here, so that neither 4.0 report below finds
 * anything to say about them.
 */
void
lib_put_in_t::report_full_pre390 (void)
{
  scr_int object, count;

  if (obj_uses_burden_model (game))
    {
      count = 0;
      for (object = 0; object < object_count; object++)
        {
          if (!game->object_references[object])
            continue;

          game->object_references[object] = FALSE;
          count++;
        }

      if (count > 0)
        {
          lib_new_clause (game, has_printed);
          lib_print_object_np (game, container);
          pf_buffer_string (filter,
                            lib_select_plurality (game, container,
                                                  " is", " are"));
          pf_buffer_string (filter, " full.");
          has_printed = TRUE;
        }
    }
}

void
lib_put_in_t::report_nothing_fits_390 (void)
{
  scr_int object;

  /* 3.9's all/and arm with nothing fitting names no object; see above. */
  if (nothing_fits_390)
    {
      for (object = 0; object < object_count; object++)
        game->object_references[object] = FALSE;
      lib_new_clause (game, has_printed);
      lib_print_wrapped_object (game, "Nothing will fit inside ",
                                container, ".");
      has_printed = TRUE;
    }
}

/*
 * Report objects not put in because of their size.  These objects remain in
 * standard references, as do objects rejected because of capacity limits.
 * By removing too large objects in this loop, we're left later on with just
 * the objects rejected by capacity limits.
 */
void
lib_put_in_t::report_too_big (void)
{
  scr_int object;

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      if (!(obj_get_size (game, object) > capacity))
        continue;

      list.push_back (object);
      game->object_references[object] = FALSE;
    }

  if (!list.empty ())
    {
      lib_new_clause (game, has_printed);
      lib_print_list (game, list, lib_print_object_np, " and ");
      pf_buffer_string (filter,
                        list.size () == 1
                        ? lib_select_plurality (game, list[0],
                                                " is too big", " are too big")
                        : " are too big");
      pf_buffer_string (filter, " to fit inside ");
      lib_print_object_np (game, container);
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();
}

/*
 * Report objects not put in because the container is too full.  This should
 * be all remaining objects in standard references.
 */
void
lib_put_in_t::report_no_room (void)
{
  scr_int object;

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      list.push_back (object);
      game->object_references[object] = FALSE;
    }

  if (!list.empty ())
    {
      lib_new_clause (game, has_printed);
      lib_print_list (game, list, lib_print_object_np, " and ");
      lib_print_wrapped_object (game, " can't fit inside ",
                                container, " at the moment.");
    }
  has_printed |= !list.empty ();
}

lib_put_outcome_t
lib_put_in_backend (scr_gameref_t game, scr_int container,
                    scr_bool is_all_form)
{
  scr_int object, length_before, length_after_tasks;
  scr_bool closed_refusal_only, is_refusal_only;
  lib_put_outcome_t outcome;
  lib_put_in_t put (game, container, is_all_form);

  /* How many objects the line named, for 3.9's all/and arm below. */
  put.entry_count = 0;
  GS_FOR_EACH_OBJECT (game, object)
    put.entry_count += game->object_references[object] ? 1 : 0;

  length_before = lib_output_length (game);
  lib_put_announce_bytes = 0;
  put.try_game_commands ();

  /* 4.0's take-phase close, deferred tasks and itself test. */
  lib_put_deferred_stage_400 (game, put.pending, container, FALSE,
                              &put.has_printed, &put.task_claimed,
                              &put.recursion_rejected);
  length_after_tasks = lib_output_length (game);

  /*
   * 4.0's closed-container test, in insides' own place: after the take
   * phase, the tasks and the itself test, before size and capacity.  It is
   * a refusal in the size refusal's sense -- printed, and the line left
   * for the task pass -- unless something already spoke ahead of it.
   */
  closed_refusal_only = FALSE;
  if (lib_put_in_closed_400 (game, container, put.has_printed))
    {
      closed_refusal_only = !put.has_printed;
      put.has_printed = TRUE;
    }

  /*
   * Retrieve the container's total volume, and the volume it has left.  The
   * free space is tracked across the loop below rather than recomputed, since
   * each object put in spends some of it.
   */
  put.capacity = obj_get_container_capacity (game, container);
  put.free_space = obj_get_container_free_space (game, container);

  put.report_full_390 ();
  put.put_counted_390 ();
  put.put_remaining ();
  put.report_full_pre390 ();

  /*
   * A put that moved nothing and said nothing so far is about to say only
   * the size and capacity refusals below; see the return value.  A closed
   * refusal that spoke first counts the same way.
   */
  is_refusal_only = !put.has_printed || closed_refusal_only;

  put.report_nothing_fits_390 ();
  put.report_too_big ();
  put.report_no_room ();

  /* Refusals only if they were printed, and nothing before them. */
  is_refusal_only = is_refusal_only && put.has_printed;

  /* Note any remaining multiple references left out of the operation. */
  lib_drain_multiple_references (game, put.object_count, put.list);

  lib_print_object_list (game, put.has_printed, put.list, " or ", '.',
                         "You are not holding ",
                         "I am not holding ",
                         "%player% is not holding ");

  /*
   * Refusal-only when the whole put came to nothing but size or capacity
   * refusals: nothing moved, no task or implicit take spoke, nobody was
   * "not holding" anything.  4.0 does not count that as handling the
   * command; see lib_put_in_refused().
   */
  /*
   * A static named in a 4.0 put leaves the line for the task pass as surely
   * as a size refusal does: run400's `insides` exits at loc_465ED7 on
   * obj.global_24 = 1, ahead of the result byte var_86 that a completed move
   * sets, so nothing claims.  thelasthour turn 26 is the live case --
   * `put hands into hole`, the hand a static, answers "I can't take the
   * hand!  I am carrying nothing!" and then task 15's own "Can't take the
   * mouse. Too far." -- and probe PSTAT command 12 is the same shape built
   * from nothing (`put slab in box` -> "... You are carrying nothing!
   * SLABTASK.").
   */
  outcome.is_refusal_only = (is_refusal_only && put.list.empty ())
                            || (put.static_refused && !put.has_moved);
  lib_put_note_output (game, &outcome, length_before, length_after_tasks,
                       put.task_claimed);
  return outcome;
}


/*
 * lib_put_in_refused()
 *
 * The put's line has been printed and terminated.  At 4.0, in the priority
 * pass, a refusal-only put leaves the command unclaimed: run400's insides
 * handler (Proc_19_43_46639C) exits every message path without setting its
 * return byte, so "The rock is too big to fit inside the slot." is printed
 * and a matching task then answers the same line, joined on with two spaces
 * (arena probe PUT7, runner_probes/put7.run400.b.txt, 2026-09-05; Zack
 * Smackfoot `put knife in slot`, runner_transcripts/zacksmackfoot.txt).
 * Signal that to run_priority_commands() and report the handler's return.
 * Pre-4.0 the refusal claims the line, as it always has.
 */
static scr_bool
lib_put_in_refused (scr_gameref_t game, scr_bool is_refusal_only)
{
  if (is_refusal_only && lib_is_version_400 (game) && run_in_priority_pass ())
    {
      run_priority_refuse ();
      return FALSE;
    }

  return TRUE;
}


/*
 * lib_put_finish()
 *
 * End the put's line and settle its claim from the backend's outcome; see
 * lib_put_outcome_t.  Only the container backend ever reports a
 * refusal-only outcome; see lib_put_in_refused().  A 4.0 put that printed
 * nothing is left unclaimed with no line ending, for the task pass to
 * answer.
 *
 * A 4.0 put whose only words are the implicit take's announcement, with a
 * task claiming it silently, ends on the game's DontUnderstand text.  run400
 * writes "(Taking X first)" straight to the textbox (@46E2EA-46E30C) while
 * the turn's answer collects in MemVar_4941B0, and a claim that printed
 * nothing leaves that empty for generaltasks' tail (48B573) to fill.  hcw
 * turn 162 (runner_probes/hcw.run400.txt:1087): `put susan in trunk`
 * pre-matches the lower-cased rebuilds, dispatches the raw "get sleeping
 * Susan" / "put sleeping Susan in the car trunk", runs nothing (see
 * lib_run_rebuilt_line_400()) and reads "(Taking sleeping Susan first)" / "I
 * don't understand what you mean.".
 */
scr_bool
lib_put_finish (scr_gameref_t game, const lib_put_outcome_t &outcome)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (outcome.is_silent && lib_is_version_400 (game))
    return FALSE;
  if (outcome.is_announce_only && lib_is_version_400 (game))
    {
      pf_buffer_string (filter, prop_get_global_string (gs_get_bundle (game),
                                                        "DontUnderstand"));
      pf_buffer_answer_break (filter);
      return TRUE;
    }
  if (!outcome.is_tasks_only)
    pf_buffer_answer_break (filter);
  return lib_put_in_refused (game, outcome.is_refusal_only);
}


/*
 * lib_put_multiple_filter()
 * lib_put_multiple_resolve_filter()
 * lib_put_except_filter()
 *
 * Helper functions for deciding if an object may be put in or on another in
 * this context, in the shape the multiple-object parser and lib_apply_filter()
 * take.  Returns TRUE if an object may be manipulated, FALSE otherwise.  The
 * named form is much the wider; see lib_put_named_filter().
 */
scr_bool
lib_put_multiple_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_named_filter (game, object);
}

/* The resolver twin of the above; see lib_put_resolve_filter(). */
scr_bool
lib_put_multiple_resolve_filter (scr_gameref_t game, scr_int object,
                                 scr_int unused)
{
  assert (unused == -1);

  return lib_put_resolve_filter (game, object);
}

/* The except form: everything held but the target itself. */
scr_bool
lib_put_except_filter (scr_gameref_t game, scr_int object, scr_int target)
{
  return lib_put_all_filter (game, object, target);
}


/*
 * lib_put_target_pre390()
 *
 * Name the container of a "put X in Y", or the supporter of a "put X on Y",
 * the way run370 and run380 do, which is not the way anything else in the
 * library names an object: the room is no part of the test.  On
 * p38DARK/p37DARK with the player standing in the lit room and the box left
 * behind on the cave floor, `put lamp in box` and `put stone in box` both
 * answer "You are not holding a box."
 * (runner_probes/dark.run380.putin3.txt:25/31,
 * runner_probes/dark.run370.putin3.txt:25/31, 2026-09-12), and so does `put
 * all in box` (runner_probes/dark.run380.putin3.txt:50,
 * runner_probes/dark.run370.putin3.txt:50) -- the box is found, and then
 * refused for not being in the player's hands.  run390 never finds it and asks
 * "Put the lamp inside what?" instead
 * (runner_probes/dark.run390.putin3.txt:24).
 *
 * A noun that names nothing anywhere is still nothing: `put coin in zzzz`
 * and `put coin in me` fall through to lib_cmd_put_in_nowhere()
 * (runner_probes/dark.run380.putin2.txt:72,
 * runner_probes/dark.run380.putin.txt:66).
 *
 * The room-filtered resolver runs first and keeps the ambiguity prompt for
 * the cases it already handled; only when it comes back empty-handed, and
 * unambiguously so, is the whole game searched, and then only a single
 * candidate is accepted.  Two namesakes in two rooms are left to the caller
 * as no container at all, which is where they were before this.
 *
 * The surface half of the row is the same code: with the table left in the
 * cave and the player standing in the lit room, `put lamp on table` and `put
 * stone on table` are both "You are not holding a table." on run370 and
 * run380 (p37SURF/p38SURF, runner_probes/surf3.run370.txt:5/7 and
 * runner_probes/surf3.run380.txt:5/7, 2026-09-12), where run390 never finds
 * the table and asks "Put the lamp onto what?" instead.  That is only to be
 * expected once the two handlers turn out to be one; see
 * lib_put_target_takes_on().
 */
scr_int
lib_put_target_pre390 (scr_gameref_t game, const scr_char *prompt,
                       scr_bool *is_ambiguous)
{
  const scr_bool requires_seen = lib_matcher_requires_seen (game);
  std::vector<scr_bool> named;
  scr_int index_, object, count, container;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return lib_disambiguate_object (game, prompt, is_ambiguous);

  /* lib_disambiguate_object_common() clears what it rejects; keep a copy. */
  named.assign (gs_object_count (game), FALSE);
  GS_FOR_EACH_OBJECT (game, index_)
    named[index_] = game->object_references[index_];

  container = lib_disambiguate_object (game, prompt, is_ambiguous);
  if (container != -1 || *is_ambiguous)
    return container;

  count = 0;
  object = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (!named[index_] || (requires_seen && !gs_object_seen (game, index_)))
        continue;
      count++;
      object = index_;
    }

  if (count != 1)
    return -1;

  GS_FOR_EACH_OBJECT (game, index_)
    game->object_references[index_] = (index_ == object);
  return object;
}


/*
 * lib_put_target_takes_on()
 *
 * Which of the two put handlers a line actually runs.  From 3.9 on the two
 * are separate and the answer is simply the preposition the player typed.
 *
 * Below 3.9 they are ONE handler, and the preposition in the answer comes
 * from the TARGET's kind rather than from anything the player wrote.
 * Measured on p37SURF under run370 and p38SURF under run380, 2026-09-12,
 * with the box (a container) and the table (a surface) both in hand:
 *
 *   put pebble on box   You put the pebble inside the box.
 *                       (runner_probes/surf5.run370.txt:4,
 *                       runner_probes/surf5.run380.txt:4)
 *   put stone in table  You put the stone on the table.
 *                       (runner_probes/surf5.run370.txt:7,
 *                       runner_probes/surf5.run380.txt:7)
 *   put lamp on box     with the box shut
 *                       You can't put anything inside the box as it is
 *                       closed!
 *                       (runner_probes/surf5.run370.txt:10,
 *                       runner_probes/surf5.run380.txt:10)
 *   put all on box      You put an torch, a lamp and a table inside the box.
 *                       (runner_probes/surf5.run370.txt:12,
 *                       runner_probes/surf5.run380.txt:12)
 *
 * -- where run390 and run400 keep the two apart and refuse every one of
 * them, "You can't put anything onto the box." / "...inside the table."
 * (runner_probes/surf5.run390.txt:4/7, runner_probes/surf5.run400.txt:4/7).
 * It also explains the third of the pre-3.9 answers that used to look like a
 * surface test and is not: `put coin on box` with the box standing on the cave
 * floor is "You are not holding a box." (runner_probes/surf1.run370.txt:16),
 * the container handler's own held refusal, reached because the line was
 * routed by the box's kind.
 *
 * A target that is both, or neither, has no kind to be routed by, so it
 * keeps the preposition the player typed -- which is what the neither case
 * is measured to do: `put coin on stone` is "You can't put anything on the
 * stone." (runner_probes/surf1.run370.txt:14) against `put coin in stone`'s
 * "...inside the stone." on the same Runner.
 */
scr_bool
lib_put_target_takes_on (scr_gameref_t game, scr_int target, scr_bool typed_on)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return typed_on;

  if (obj_is_container (game, target) && !obj_is_surface (game, target))
    return FALSE;
  if (obj_is_surface (game, target) && !obj_is_container (game, target))
    return TRUE;
  return typed_on;
}


/*
 * lib_put_refusal_swept
 * lib_put_invalid_result()
 *
 * run390 insides() settles the target before its task look-up at 461A6C,
 * exactly as it does a line naming fewer than two objects (see
 * lib_put_refusal_first_390()): the wrong-kind refusals "You can't put
 * anything inside/onto <the X>." (4623F8) and the shut container's "... as it
 * is closed!" are written with no checktask of their own, and the post-put
 * sweep at 462550 then decides.  A claimant named in the line runs the tasks
 * QUIET, so a failing task leaves the refusal standing; with none, the LOUD
 * pass lets its FailMessage replace it; a passing task replaces it either
 * way.  Measured on pPUTREF39.taf (make_39_putrefprobe.py), run390x,
 * 2026-09-19 (runner_probes/putref.run390.txt /
 * runner_probes/putref.run390.feed2.txt), each line with a matching task whose
 * restriction fails:
 *
 *   put coin in lamp    You can't put anything inside the lamp.
 *   put coin in box     You can't put anything inside the box as it is closed!
 *   put coin on box     You can't put anything onto the box.
 *   put gem in ring     T7 FAIL.   (both in the bag: no claimant, LOUD)
 *   put stone in box    T10 PASS.  (a passing task)
 *
 * where the deferred refusals had let every one of them print its task's
 * FailMessage.  The validators run the sweep themselves and raise this flag,
 * so that their callers claim the line rather than hand it on.
 */
scr_bool lib_put_refusal_swept = FALSE;

/*
 * lib_put_reset()
 *
 * Forget every per-command put admission and flag, for a game that is
 * about to (re)start; run_player_input() calls this beside the other
 * library resets.  A sweep whose put never reached its verdict, or a
 * pre-4.0 noun still mid-word when the game ended, would otherwise leak
 * into the next game's first put.
 */
void
lib_put_reset (void)
{
  lib_put_present_unseen = -1;
  lib_put_announce_bytes = 0;
  lib_put_mid_word_390 = FALSE;
  lib_put_refusal_swept = FALSE;
}

scr_bool
lib_put_invalid_result (void)
{
  if (lib_put_refusal_swept)
    {
      lib_put_refusal_swept = FALSE;
      return TRUE;
    }
  return !run_in_priority_pass ();
}

static void
lib_put_refusal_sweep_390 (scr_gameref_t game, size_t from)
{
  lib_put_sweep_390 (game, run_get_dispatch_input (), from);
  lib_put_refusal_swept = TRUE;
}


/*
 * lib_put_static_absent_pre390()
 *
 * lib_put_target_pre390() finds a target anywhere in the game, and 3.7/3.8
 * insides() then refuses a static one that is not in the player's room with
 * "You can't see " & Prefix & " " & Short & "." -- the arm
 * lib_put_co_refusal_pre390() already gives for a line it could not parse.
 * p37PUT/p38PUT, standing in the cave with the open static cupboard in the
 * lit room: `put coin in cupboard` is "You can't see a cupboard." (run370x
 * runner_probes/put.run370.drop.rtf, run380x
 * runner_probes/put.run380.drop.rtf, 2026-09-19); Scarier had put the coin
 * into it.  The not-a-container refusal comes first: `put coin in statue`, the
 * plain static statue also left behind, is "You can't put anything inside the
 * statue." on the same turns.  The static surface is the same handler, and
 * unmeasured.
 */
static scr_bool
lib_put_static_absent_pre390 (scr_gameref_t game, scr_int target)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390
      || !obj_is_static (game, target)
      || obj_directly_in_room (game, target, gs_playerroom (game)))
    return FALSE;
  if (run_priority_defer_if_active ())
    return TRUE;
  lib_cant_see_named_pre_390 (game, target, FALSE, ".\n");
  return TRUE;
}


/*
 * lib_put_in_is_valid()
 *
 * Validate the container requested in "put in" commands.
 */
scr_bool
lib_put_in_is_valid (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* Verify that the container object is a container. */
  if (!obj_is_container (game, container))
    {
      /*
       * In the tentative priority pass the refusal is deferred, so a matched
       * task's fail message can claim the input first; the STANDARD_COMMANDS
       * duplicate prints it when no task does (run400-verified, 2026-08-02).
       */
      const scr_bool is_first = lib_put_refusal_first_390 (game);
      const size_t from = pf_buffer_length (filter);

      if (!is_first && run_priority_defer_if_active ())
        return FALSE;
      /*
       * Only 4.0 shouts.  The pre-4.0 Runners all carry the literal
       * " can't put anything inside " -- run400 carries none of it, it
       * composes its own from "You can't put anything " & preposition --
       * and they end the line with a full stop: p39DARK under run390,
       * `put coin in stone` -> "You can't put anything inside the stone."
       * (runner_probes/dark.run390.putin.txt:43), against run400's "You can't
       * put anything inside the stone!" on the same turn of p4TFROM
       * (runner_probes/tfrom.run400.putin.txt:24).  Measured 2026-09-12.
       */
      lib_print_response_object (game,
                                 "You can't put anything inside ",
                                 "I can't put anything inside ",
                                 "%player% can't put anything inside ",
                                 container,
                                 lib_is_version_400 (game) ? "!\n" : ".\n");
      if (is_first)
        lib_put_refusal_sweep_390 (game, from);
      return FALSE;
    }

  /*
   * Version 3.8 will only fill a dynamic container the player is holding: an
   * open box sitting on the floor answers "You are not holding a box." (note
   * the object's own prefix, not the "the" of most refusals).  Static
   * containers are exempt -- run380 puts a coin into Wrecked's static red
   * locker where it stands, which is just as well, since nothing could ever
   * pick one up.  Both measured 2026-08-03.
   */
  if (lib_put_static_absent_pre390 (game, container))
    return FALSE;
  if (obj_uses_burden_model (game)
      && !obj_is_static (game, container)
      && gs_object_position (game, container) != OBJ_HELD_PLAYER)
    {
      if (run_priority_defer_if_active ())
        return FALSE;
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object (game, container);
      pf_buffer_string (filter, ".\n");
      return FALSE;
    }

  /*
   * If the container is closed, reject now -- before 4.0 only.  4.0's closed
   * test is insides' own, and it sits late: run400 name_object has already
   * run its take piece (@46E2B5) before insides (46639C) tests the target's
   * state at 4661BE/4661C9, after the tasks() call, the possession test and
   * the itself test.  So at 4.0 it belongs to the backend, after the take
   * phase; see lib_put_in_closed_400().
   */
  if (gs_object_openness (game, container) > OBJ_OPEN
      && !lib_is_version_400 (game))
    {
      const scr_bool is_first = lib_put_refusal_first_390 (game);
      const size_t from = pf_buffer_length (filter);

      if (!is_first && run_priority_defer_if_active ())
        return FALSE;

      /*
       * 4.0 reworded this one too, and pre-4.0 has no lock to report: the
       * literal " as it is closed!" is in run370, run380 and run390 and in
       * no run400, while " as it is locked!", " is locked!" and every other
       * spelling of "locked" are in run400 alone.  So a container the game
       * calls locked draws the closed wording before 4.0.  The closed half
       * is measured on both arms with the same probe turn, `put lamp in
       * box` with the box shut: run390 "You can't put anything inside the
       * box as it is closed!" (p39DARK,
       * runner_probes/dark.run390.putin2.txt:28) against run400 "The box is
       * closed!" (p4TFROM, runner_probes/tfrom.run400.putin2.txt:18),
       * 2026-09-12.
       */
      lib_print_response_object (game,
                                 "You can't put anything inside ",
                                 "I can't put anything inside ",
                                 "%player% can't put anything inside ",
                                 container, " as it is closed!\n");
      if (is_first)
        lib_put_refusal_sweep_390 (game, from);
      return FALSE;
    }

  /* Container is a valid target for "put in". */
  return TRUE;
}


/*
 * lib_put_on_backend()
 *
 * Common backend handler for placing objects on supporters.  Places all
 * objects currently referenced in the game onto a supporter, trying game
 * commands first, and then moving other unhandled objects onto the supporter.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
lib_put_outcome_t
lib_put_on_backend (scr_gameref_t game, scr_int supporter)
{
  const scr_bool is_pre_390 =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;
  lib_move_verb_t verb;
  scr_int object_count, object, length_before, length_after_tasks;
  scr_bool has_printed, task_claimed, recursion_rejected;
  lib_put_outcome_t outcome;
  lib_list_t pending;

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.  At the same time, check for and
   * weed out any moves that result in infinite recursion.
   */
  length_before = lib_output_length (game);
  lib_put_announce_bytes = 0;
  has_printed = FALSE;
  task_claimed = FALSE;
  recursion_rejected = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /*
       * Version 4.0 picks up an object it has been asked to put down, and it
       * does so BEFORE the handler's task look-up.  run400's name_object
       * loop runs the take piece at loc_46E2B5 and only then hands the pair
       * to insides (Proc_19_43_46639C @loc_46E34F), which is where the
       * canonical line reaches the tasks -- so a task that goes on to claim
       * the put still carries the announcement ahead of its own text.
       * Measured on frustrated turns 53-55
       * (runner_transcripts/frustrated.txt): `put small rock on left pan`
       * matches task 511 `put*small*left*` and still opens "(Taking the small
       * rock first)".
       */
      {
        scr_bool take_printed = FALSE;

        lib_put_implicit_take (game, object, supporter, &take_printed);
        has_printed |= take_printed;
      }

      /* The tasks' turn; see lib_put_in_backend(). */
      if (lib_is_version_400 (game))
        {
          pending.push_back (object);
          continue;
        }

      /* Reject and remove attempts to place objects on themselves; the
       * 4.0 test sits after the deferred task pass (see
       * lib_put_deferred_stage_400). */
      if (!lib_check_put_recursion (game, object, supporter, !has_printed,
                                    TRUE))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
          continue;
        }

      if (lib_try_game_command_with_object (game,
                                            "put", object, "on", supporter))
        {
          game->object_references[object] = FALSE;
          game->multiple_references[object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
          continue;
        }
    }

  /*
   * 4.0's take-phase close, deferred tasks and itself test, as insides
   * orders them.  The itself wording at 46600C is the surface one whenever
   * the target is a surface and not also a container.
   */
  lib_put_deferred_stage_400 (game, pending, supporter, TRUE,
                              &has_printed, &task_claimed,
                              &recursion_rejected);
  length_after_tasks = lib_output_length (game);

  /*
   * "onto" is 3.9's word.  run370 and run380 write " on ": `put stone on
   * table` with the table in hand is "You put the stone on the table."
   * (runner_probes/surf1.run370.txt:15, runner_probes/surf1.run380.txt:15,
   * 2026-09-12), against run390's "You put the stone onto the table."
   * (runner_probes/surf1.run390.txt:15).  The all form also prints its list
   * with the objects' own prefixes there, the same way the pre-3.9 "put all
   * in" does: `put all on table` is "You put an torch and a lamp on the
   * table." (runner_probes/surf4.run370.txt:5,
   * runner_probes/surf4.run380.txt:5) where the named form says "the".  See
   * lib_put_in_backend() for the container twin.
   */
  verb = LIB_PUT_ON_VERB;
  if (is_pre_390)
    verb.onto = " on ";

  lib_move_backend (game, &verb, supporter, has_printed);

  outcome.is_refusal_only = FALSE;
  lib_put_note_output (game, &outcome, length_before, length_after_tasks,
                       task_claimed);
  return outcome;
}


/*
 * lib_put_on_is_valid()
 *
 * Validate the supporter requested in "put on" commands.  The twin of
 * lib_put_in_is_valid(), and split by version in the same two places.
 */
scr_bool
lib_put_on_is_valid (scr_gameref_t game, scr_int supporter)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_bool is_pre_390 =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;

  /* Verify that the supporter object is a supporter. */
  if (!obj_is_surface (game, supporter))
    {
      const scr_bool is_first = lib_put_refusal_first_390 (game);
      const size_t from = pf_buffer_length (filter);

      /* Deferred in the tentative priority pass; see lib_put_in_is_valid. */
      if (!is_first && run_priority_defer_if_active ())
        return FALSE;
      /*
       * Three generations, three spellings, and Scarier had none of them: it
       * said "on" with 4.0's exclamation mark.  3.7 and 3.8 carry the whole
       * literal " can't put anything on "; 3.9 composes " can't put anything "
       * with "onto" (the census finds bare "onto" in run390 and run400 only,
       * and the run370/run380 pools hold nothing but "Mapontop"); and only
       * 4.0 shouts.  `put coin on stone` on the surface probe, 2026-09-12:
       *
       *   run370  You can't put anything on the stone.
       *           runner_probes/surf1.run370.txt:14
       *   run380  You can't put anything on the stone.
       *           runner_probes/surf1.run380.txt:14
       *   run390  You can't put anything onto the stone.
       *           runner_probes/surf1.run390.txt:14
       *   run400  You can't put anything onto the stone!
       *           runner_probes/surf1.run400.txt:14
       */
      lib_print_response_object (game,
                                 is_pre_390 ? "You can't put anything on "
                                            : "You can't put anything onto ",
                                 is_pre_390 ? "I can't put anything on "
                                            : "I can't put anything onto ",
                                 is_pre_390
                                   ? "%player% can't put anything on "
                                   : "%player% can't put anything onto ",
                                 supporter,
                                 lib_is_version_400 (game) ? "!\n" : ".\n");
      if (is_first)
        lib_put_refusal_sweep_390 (game, from);
      return FALSE;
    }

  /*
   * And below 3.9 a surface has to be in the player's hands, exactly as a
   * container does -- one handler, one test.  With the table left standing
   * in the cave, `put lamp on table`, `put stone on table` and `put all on
   * table` all answer "You are not holding a table." on run370 and run380
   * (runner_probes/surf3.run370.txt:5/7, runner_probes/surf3.run380.txt:5/7,
   * runner_probes/surf6.run370.txt:3, runner_probes/surf6.run380.txt:3,
   * 2026-09-12) -- the object's own prefix, not the "the" of most refusals.
   * Statics are exempt, and have to be: `put stone on bench`, the bench a
   * static surface on the cave floor, is "You put the stone on the bench." on
   * both (runner_probes/surf2.run370.txt:12,
   * runner_probes/surf2.run380.txt:12).
   *
   * 3.9 and 4.0 have no such test.  The same `put all on table` with the
   * table on the floor is "You put the torch, the lamp, the stone, the
   * pebble and the box onto the table." on run390
   * (runner_probes/surf6.run390.txt:3), and run400 moves onto it too
   * (runner_probes/surf6.run400.txt:6).
   */
  if (lib_put_static_absent_pre390 (game, supporter))
    return FALSE;
  if (is_pre_390
      && obj_uses_burden_model (game)
      && !obj_is_static (game, supporter)
      && gs_object_position (game, supporter) != OBJ_HELD_PLAYER)
    {
      if (run_priority_defer_if_active ())
        return FALSE;
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object (game, supporter);
      pf_buffer_string (filter, ".\n");
      return FALSE;
    }

  /* Surface is a valid target for "put on". */
  return TRUE;
}

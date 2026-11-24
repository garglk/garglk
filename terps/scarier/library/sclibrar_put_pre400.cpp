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
 * Putting objects below 4.0: the answers a pre-4.0 "put X in Y" gives
 * before it moves anything, the 3.9 refusal-first task sweep, the named
 * handler (lib_put_named_pre400), the co() test the put handlers make, and
 * the 3.9 "put X in that" question.  The command forms that call them, and
 * the 4.0 counterparts, are in sclibrar_put_multiple.cpp.
 */

#include "sclibrar_put_internal.h"


/*
 * lib_put_in_present_filter()
 *
 * The universe run390's put names its object from, before any test of where
 * that object is: everything present in the room, the contents of open
 * containers included.  It is deliberately wider than lib_put_named_filter()
 * -- the pre-4.0 handler answers differently for a noun that names nothing
 * at all and one that names something it cannot reach, so the two have to be
 * told apart; see lib_put_named_pre400().
 */
static scr_bool
lib_put_in_present_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return obj_indirectly_in_room (game, object, gs_playerroom (game));
}


/*
 * lib_put_no_object_pre400()
 * lib_put_not_reachable_pre400()
 * lib_put_what_pre400()
 *
 * The three answers a pre-4.0 "put X in Y" gives before it ever moves
 * anything, and they are decided in this order -- the object's own failure
 * outranks the container's.  Measured on p39DARK under run390:
 *
 *   runner_probes/dark.run390.putin2.txt:67
 *                  `put zzzz in box`   You can't do that!
 *   runner_probes/dark.run390.putin3.txt:27
 *                  `put stone in lamp` You can't do that!    (the stone is a
 *                  room away; the lamp is held and is not a container, and
 *                  that refusal never gets a word in)
 *   runner_probes/dark.run390.putin2.txt:40
 *                  `put coin in box`   You can't see that.   (the coin is
 *                  inside the box, the box on the floor -- present, named,
 *                  and out of reach; see lib_put_named_filter)
 *   runner_probes/dark.run390.putin3.txt:24
 *                  `put lamp in box`   Put the lamp inside what?  (the lamp
 *                  is held, the box a room away)
 *   runner_probes/dark.run390.putin.txt:34
 *                  `put coin in coin`  Put the coin inside what?
 *   runner_probes/dark.run390.putin.txt:37
 *                  `put box in box`    Put the box inside what?
 *   runner_probes/dark.run390.putin.txt:64
 *                  `put coin in me`    Put the coin inside what?
 *
 * The surface handler answers the same shapes the same way, in its own
 * preposition (p39SURF under run390): `put stone on lamp` with
 * the stone a room away is "You can't do that!"
 * (runner_probes/surf1.run390.txt:6), `put coin on table` with the coin on the
 * table and the table on the floor is "You can't see that."
 * (runner_probes/surf1.run390.txt:5), and `put coin on coin`, `put table on
 * table` and `put coin on me` are "Put the coin onto what?", "Put the table
 * onto what?" and "Put the coin onto what?"
 * (runner_probes/surf1.run390.txt:11/12/19).
 *
 * -- so the container fragment is resolved with the object already spoken
 * for, and a fragment that names only the object, or names nothing at all,
 * leaves the Runner with no container and it asks.  (4.0 answers the last
 * three "You can't put anything inside the coin!", "You can't put an object
 * inside itself!" and "I don't understand what you want to put things
 * inside." on the same turns of p4TFROM,
 * runner_probes/tfrom.run400.putin.txt:15/18/46.)
 *
 * " can't do that!" is in run370, run380 and run390 and in no run400, and
 * " can't see that." is in all four; the prompt is composed, so the census
 * cannot pin it and it is gated at 3.9, the version it was measured on.
 * 3.7 and 3.8 keep what they had.
 */
static scr_bool lib_put_co_refusal_pre390 (scr_gameref_t game);

/*
 * lib_put_refusal_first_390()
 *
 * TRUE if this refusal is run390's to give before any task sees the line.
 * insides() runs above tasks(0) in generaltasks, and its first question,
 * before the task look-up, is how many objects co() names in the line.
 * co() answers only for a present, seen object, so a put whose object is
 * absent or never seen counts one name, the container's, and fewer than
 * two is answered on the spot (461646): "Put <the X> inside what?" when the
 * object was named and the container was not, "You can't do that!"
 * otherwise.  Both go to the post-put sweep, whose claimant runs the tasks
 * QUIET and whose absence leaves them LOUD, as after a put that moved
 * something; see lib_put_task_sweep_390().
 *
 * Measured on Lair of the CyberCow (runner_transcripts/cybercow.txt T62,
 * run390x): `put bones in robot` with the bones never made --
 * the fairy got away -- is "You can't do that!".  The robot, lying in the
 * cellar, claims the sweep; tasks 81-85 all match and all fail QUIET, so
 * task 85's FailMessage "You're not holding the little bones to install
 * them in the Invincible Robot." never replaces the refusal, where the
 * deferred refusal had let the loud task pass print it.
 *
 * Only the refusals the priority pass reaches are moved; the put-nowhere
 * catch-alls below STANDARD_COMMANDS still follow the task passes.
 */
scr_bool
lib_put_refusal_first_390 (scr_gameref_t game)
{
  return lib_is_version_390 (game) && run_in_priority_pass ()
         && run_get_dispatch_input ();
}

scr_bool
lib_put_no_object_pre400 (scr_gameref_t game)
{
  if (lib_put_refusal_first_390 (game))
    {
      const size_t from = pf_buffer_length (gs_get_filter (game));

      lib_print_response_message (game,
                                  "You can't do that!\n",
                                  "I can't do that!\n",
                                  "%player% can't do that!\n");
      lib_put_sweep_390 (game, run_get_dispatch_input (), from);
      return TRUE;
    }

  if (run_priority_defer_if_active ())
    return FALSE;

  /* 3.7/3.8 insides() names what co() chose; see lib_put_co_refusal_pre390(). */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      && lib_put_co_refusal_pre390 (game))
    return TRUE;

  return lib_print_response_message (game,
                                     "You can't do that!\n",
                                     "I can't do that!\n",
                                     "%player% can't do that!\n");
}

/*
 * lib_cmd_put_no_clause_pre400()
 *
 * `put on X` and `put in X`, the spellings with nothing between the verb and
 * the preposition.  From 3.8 insides() takes them like any other put (its
 * entry is c("put") with c("inside"/"into"/"in"/"on"), run380 445746), and
 * the line co() names at most once, so it is answered "You can't do that!"
 * before any target test -- 3.8 at 445A2A, 3.9 at 461646, where the target
 * pass has already taken the one name for the container because it stands
 * after InStr(line, "on").  3.7 never gets here: the wear rows above claim
 * the line, insides() being a plain bottom-of-generaltasks Call there
 * (43BA0A); see lib_wear_is_put_line_380().
 */
scr_bool
lib_cmd_put_no_clause_pre400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_put_no_object_pre400 (game);
}

static scr_bool
lib_put_not_reachable_pre400 (scr_gameref_t game)
{
  if (lib_put_refusal_first_390 (game))
    {
      const size_t from = pf_buffer_length (gs_get_filter (game));

      lib_print_response_message (game,
                                  "You can't see that.\n",
                                  "I can't see that.\n",
                                  "%player% can't see that.\n");
      lib_put_sweep_390 (game, run_get_dispatch_input (), from);
      return TRUE;
    }

  if (run_priority_defer_if_active ())
    return FALSE;

  return lib_print_response_message (game,
                                     "You can't see that.\n",
                                     "I can't see that.\n",
                                     "%player% can't see that.\n");
}

scr_bool
lib_put_what_pre400 (scr_gameref_t game, scr_int object, scr_bool typed_on)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;

  if (lib_put_refusal_first_390 (game))
    {
      const size_t from = pf_buffer_length (filter);

      pf_buffer_string (filter, "Put ");
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        typed_on ? " onto what?\n" : " inside what?\n");
      lib_put_sweep_390 (game, run_get_dispatch_input (), from);
      return TRUE;
    }

  if (run_priority_defer_if_active ())
    return FALSE;

  pf_buffer_string (filter, "Put ");
  lib_print_object_np (game, object);
  pf_buffer_string (filter, typed_on ? " onto what?\n" : " inside what?\n");
  return TRUE;
}


/*
 * lib_put_task_sweep_390()
 *
 * run390's insides() moves a single named object and writes "You put X onto
 * Y." without ever setting its own result; that is left to the sweep that
 * follows, and it decides which task pass the line gets afterwards.  The
 * sweep walks every object whose o(22) is 0 (held), the player's room (a
 * dynamic lying there, or a static authored with InitialPosition 4 + room,
 * which is the only o(22) a static ever has) or &H9C (worn), and the first
 * one co() finds named in the line claims the put: the buffer is emptied,
 * tasks(1) -- checktask(line, 0), QUIET -- runs on the typed line, the
 * result becomes 1, and the put text comes back only if the buffer is
 * still empty.  A task the move enabled then speaks instead of the put
 * text; one whose restrictions fail says nothing.  Nothing named and
 * eligible means the result stays 0, and generaltasks goes on to tasks(0)
 * -- checktask(line, 1), LOUD -- with the put text still in the buffer: a
 * matched task's failing restriction overwrites it with the FailMessage
 * (452BBD), a passing one runs and replaces it.  The moved object itself
 * never qualifies (it is now &HF6/&HEC), so the usual claimant is the
 * container, and only when it is held or lying in the room.
 *
 * Measured, run390:
 *   - Lost Tomb (runner_transcripts/losttomb.txt T85): `put dung beetle on
 *     green pillar` -- pillar static, nothing else eligible named -- moves the
 *     beetle and the LOUD pass runs task 30 (bare `*`, all four animals on
 *     their pillars), whose "The four pillars slowly sink into the ground."
 *     replaces the put message.
 *   - Secret of the Lost World (runner_transcripts/secret_of_lost_world.txt
 *     T118-T119, and the probe
 *     runner_probes/secret_of_lost_world.run390.red.txt): `put red gem on
 *     statue` names the held green gem (co() matches Short "gem"; the prefix
 *     is not consulted), moves it onto the static statue, and the LOUD pass
 *     matches task 5 `* red * statue`, whose first restriction wants the red
 *     gem held: "You don't have a red gem." replaces the put text, and `x
 *     statue` then shows "A green gem is on ancient statue.".  T119 `put green
 *     gem on statue` finds the gem already there and task 6's "You don't have
 *     a green gem." is all that prints.
 *   - Troll (runner_transcripts/troll.txt T116, run400x and the drop/firewater
 *     probes runner_probes/troll.run390.{drop,fire}.txt): `put breadcrumbs in
 *     basin` with the basin HELD -- the basin claims the sweep, the QUIET
 *     pass matches task 51 `* breadcrumbs * basin *` whose second
 *     restriction fails, and the put text "You put the lot of breadcrumbs
 *     inside the wash basin." stands.
 * The "all" and "and" forms rebuild a line per object instead, and are not
 * ported.  3.9 only; run380 sweeps take-from lines instead
 * (lib_take_from_task_sweep_380()), and run400 dispatches its insides
 * look-ups before the move.
 */
static scr_bool lib_put_co_named_term (scr_gameref_t game,
                                       const scr_char *line, scr_int object);

/* insides()' co() name count over the whole game, as run380 keeps it. */
static scr_int
lib_put_co_count_pre390 (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, matches = 0;

  if (!line)
    return 2;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_put_co_named_term (game, line, object))
        matches++;
    }
  return matches;
}

static scr_bool
lib_put_sweep_claims_390 (scr_gameref_t game, const scr_char *line)
{
  const scr_int playerroom = gs_playerroom (game);
  const scr_int object_count = gs_object_count (game);
  scr_int object;

  for (object = 0; object < object_count; object++)
    {
      scr_bool is_eligible;

      if (obj_is_static (game, object))
        {
          const scr_int code = obj_initial_location_code (game, object);

          is_eligible = code == 0 || code == playerroom + 1;
        }
      else
        {
          const scr_int position = gs_object_position (game, object);

          is_eligible = position == OBJ_HELD_PLAYER
                        || position == OBJ_WORN_PLAYER
                        || position == playerroom + 1;
        }
      if (is_eligible && lib_put_co_named_term (game, line, object))
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_put_sweep_390()
 *
 * The sweep itself, on whatever insides() left in the buffer from `from`:
 * the task pass it picks, and the put text put back only if that pass said
 * nothing.
 */
static void
lib_put_task_pass_390 (scr_gameref_t game, const scr_char *input,
                       size_t from, scr_bool is_loud)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *buffer;
  std::string text;

  buffer = pf_get_buffer (filter);
  if (buffer && strlen (buffer) > from)
    text = buffer + from;
  pf_truncate (filter, from);
  run_typed_line_task_commands (game, input, is_loud);
  if (pf_buffer_length (filter) == from && !text.empty ())
    pf_buffer_string (filter, text.c_str ());
}

void
lib_put_sweep_390 (scr_gameref_t game, const scr_char *input, size_t from)
{
  lib_put_task_pass_390 (game, input, from,
                         !lib_put_sweep_claims_390 (game, input));
}

static void
lib_put_task_sweep_390 (scr_gameref_t game, scr_int container,
                        const lib_list_t &moving, size_t from)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *input = run_get_dispatch_input ();
  scr_bool has_moved = FALSE;

  if (version < TAF_VERSION_390 || version >= TAF_VERSION_400 || !input)
    return;

  for (const scr_int object : moving)
    {
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && gs_object_parent (game, object) == container)
        has_moved = TRUE;
    }
  if (!has_moved)
    return;

  lib_put_sweep_390 (game, input, from);
}


/*
 * lib_put_co_resolve_390()
 *
 * run390's insides() does not parse the object fragment at all.  Its
 * single-object loop offers every object to co(obj, 0) --
 * the Short, or failing that an Alias, typed as a whole word, the prefix
 * consulted only when present, seen namesakes make the term ambiguous -- and
 * moves each one that is also seen (o(44)), present (obhere), not the
 * container, and either held or lying loose in the player's room (o(22) =
 * room).  So a prefix word that names an ABSENT namesake is simply not
 * heard: secret_of_lost_world T118 (run390, runner_transcripts/
 * secret_of_lost_world.txt and the probe
 * runner_probes/secret_of_lost_world.run390.red.txt) `put red gem
 * on statue` with the green gem the only gem in play moves the green gem onto
 * the statue -- `x statue` afterwards: "A green gem is on ancient statue.",
 * and `i` no longer lists it.  (What prints is task 5's "You don't have a red
 * gem."; see lib_put_task_sweep_390().)
 *
 * Only reached when the parser found nothing under the typed words; a
 * fragment the parser does resolve is left to it.  3.9 only -- run380's
 * insides is unmeasured here.
 */
static scr_bool
lib_put_co_resolve_390 (scr_gameref_t game, scr_int container,
                        scr_int *references)
{
  const scr_char *line = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  const scr_int object_count = gs_object_count (game);
  scr_int object, count = 0;

  if (!line)
    return FALSE;

  for (object = 0; object < object_count; object++)
    {
      const scr_int position = gs_object_position (game, object);

      if (object == container || obj_is_static (game, object))
        continue;
      if (position != OBJ_HELD_PLAYER && position != room + 1)
        continue;
      if (!gs_object_seen (game, object)
          || !obj_indirectly_in_room (game, object, room))
        continue;
      if (!lib_co_pre400 (game, line, object, 0))
        continue;
      game->refs.multiple[object] = TRUE;
      count++;
    }
  if (count == 0)
    return FALSE;
  *references = count;
  return TRUE;
}


/*
 * lib_put_named_pre400()
 *
 * The pre-4.0 "put <named object> in <container>" pipeline, and -- the same
 * pipeline, because below 3.9 it is literally the same handler -- the "put
 * <named object> on <supporter>" one.  It differs from 4.0's in the order it
 * asks its questions as much as in the words it uses: the object is named
 * first and its failures are answered first, the target's own refusals come
 * next, and only then is the object tested for being somewhere the player
 * can put it from.  See lib_put_no_object_pre400() for the evidence.
 *
 * typed_on is the preposition the player wrote; which handler actually runs
 * is lib_put_target_takes_on()'s to say.
 */
scr_bool
lib_put_named_pre400 (scr_gameref_t game, scr_int target, scr_bool typed_on)
{
  const scr_bool is_pre_390 =
      prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390;
  const scr_bool is_on = lib_put_target_takes_on (game, target, typed_on);
  const scr_int container = target;
  scr_int object, object_count, objects, references;
  scr_bool has_object, status;
  lib_put_outcome_t outcome;
  lib_list_t moving;
  size_t sweep_from;

  object_count = gs_object_count (game);
  references = 0;

  /* Name the object over everything present, out of reach or not. */
  has_object = !lib_put_mid_word_390
               && lib_parse_multiple_objects (game, "move",
                                              lib_put_in_present_filter,
                                              -1, &references);
  if (!has_object && !is_pre_390
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400
      && lib_put_co_resolve_390 (game, container, &references))
    has_object = TRUE;
  if (!has_object)
    {
      /*
       * 3.7 and 3.8 invert 3.9's precedence and answer for the CONTAINER
       * first -- but only once insides() has counted two co() names on the
       * line (run380 4457A1; "You can't do that!" below that).  A fragment
       * that names nothing ANYWHERE leaves the container alone on the line,
       * and the flat refusal outranks its tests: p37PUT/p38PUT `put zzz in
       * statue` (static, no container), `put zzz in coin` (held, no
       * container) and `put zzz in chest` (static container, open and shut)
       * are all "You can't do that!" (run370x / run380x
       * runner_probes/put.run3[78]0.feed2.rtf).  An object named but out of
       * reach still counts, and goes on to the container tests below.
       */
      if (!is_pre_390 || lib_put_co_count_pre390 (game) < 2)
        return lib_put_no_object_pre400 (game);
    }
  else if (references == 0)
    return TRUE;

  /*
   * What a 3.7/3.8 put does with a static is unmeasured, so a line that
   * names one there is left exactly where it was, for the task passes and
   * the generic tail below them.  3.9 goes on: insides() counts only the
   * dynamic objects it could move, so a static that is present leaves the
   * message at "You put " and the target's own refusals first, then " can't
   * see that." and the sweep, as for any object it could not reach.
   * pPUTFULL39 `put statue in cupboard`, both statics in the room and the
   * cupboard open, is "You can't see that." (run390x
   * runner_probes/putfull.run390.txt).
   */
  for (object = 0; has_object && is_pre_390 && object < object_count;
       object++)
    {
      if (game->refs.multiple[object] && obj_is_static (game, object))
        {
          gs_clear_multiple_references (game);
          return FALSE;
        }
    }

  /*
   * The container fragment named only the object itself, so it named none.
   *
   * This one outranks the container's own refusals on 3.7 and 3.8 as well
   * as on 3.9 -- it is the one thing that does -- but the two generations
   * say different things about it.  3.9 asks; 3.7 and 3.8 answer flatly,
   * and they do so even where the named object is not a container at all
   * and the container tests below would have had something to say.
   * Measured on p38DARK/p37DARK: `put coin in coin` with the
   * coin inside the box, `put box in box` with the box held and open, and
   * `put stone in stone` with the stone loose on the floor all answer "You
   * can't do that!" (runner_probes/dark.run3[78]0.putin.txt:36/39/42),
   * against the "You can't put anything inside the coin." that the very
   * same coin draws as somebody else's container two feeds later
   * (runner_probes/dark.run380.putin3.txt:47).
   */
  if (has_object && references == 1 && game->refs.multiple[container])
    {
      gs_clear_multiple_references (game);
      if (is_pre_390)
        return lib_put_no_object_pre400 (game);
      return lib_put_what_pre400 (game, container, typed_on);
    }

  /*
   * Validate the container object to put into (deferred -> unhandled).
   *
   * On 3.7 and 3.8 this is where the turn is usually decided: the
   * container's not-holding, not-a-container and closed refusals all
   * outrank whatever the object fragment did or did not name.  Measured on
   * p38DARK under run380 and p37DARK under run370, 2026-09-12, all with the
   * object a room away or sealed out of reach --
   *
   *   put lamp in box   box on the floor a room away
   *                     You are not holding a box.
   *                     (runner_probes/dark.run3[78]0.putin3.txt:25)
   *   put stone in lamp lamp held, not a container
   *                     You can't put anything inside the lamp.
   *                     (runner_probes/dark.run3[78]0.putin3.txt:28)
   *   put lamp in box   box held and shut
   *                     You can't put anything inside the box as it is
   *                     closed!
   *                     (runner_probes/dark.run3[78]0.putin2.txt:30)
   *
   * -- where run390 answers for the object on every one of those turns
   * (runner_probes/dark.run390.putin3.txt:24/27,
   * runner_probes/dark.run390.putin2.txt:28).  Note the first: run370 and
   * run380 resolve the container over the whole game, not just the room, which
   * is why it reaches this test at all; see lib_put_target_pre390().
   */
  if (is_on ? !lib_put_on_is_valid (game, container)
            : !lib_put_in_is_valid (game, container))
    {
      gs_clear_multiple_references (game);
      return lib_put_invalid_result ();
    }

  /* The container is fine, so now the object fragment gets its answer. */
  if (!has_object)
    return lib_put_no_object_pre400 (game);

  /* Now, and only now, the object has to be somewhere the player can reach. */
  for (object = 0; object < object_count; object++)
    {
      if (game->refs.multiple[object]
          && !lib_put_named_filter (game, object))
        {
          gs_clear_multiple_references (game);
          return lib_put_not_reachable_pre400 (game);
        }
    }

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game, lib_put_multiple_filter, -1, FALSE,
                              &references);
  for (object = 0; object < object_count; object++)
    {
      if (game->refs.objects[object])
        moving.push_back (object);
    }
  sweep_from = pf_buffer_length (gs_get_filter (game));

  outcome = {};
  if (objects > 0 || references > 0)
    outcome = is_on ? lib_put_on_backend (game, container)
                    : lib_put_in_backend (game, container, FALSE);
  else
    lib_print_nothing_held (game, FALSE, FALSE, ".");

  status = lib_put_finish (game, outcome);
  lib_put_task_sweep_390 (game, container, moving, sweep_from);

  /*
   * A 3.9 put that came to nothing but a size or capacity refusal moved
   * nothing, so no sweep runs; insides() returns unclaimed and generaltasks'
   * LOUD tasks(0) gets the line with the refusal still in the buffer, where a
   * matching task's FailMessage overwrites it.  pPUTREF39.taf, run390x: `put
   * coin in bag` with the bag full is "T6 FAIL."
   * (runner_probes/putref.run390.feed2.txt:5) and the coin stays in hand,
   * where with no task the same line is "The coin can't fit inside the bag at
   * the moment."; a passing task likewise speaks alone (make_39_putprobe.py
   * PUTBIG).
   */
  if (outcome.is_refusal_only && lib_is_version_390 (game)
      && run_in_priority_pass () && run_get_dispatch_input ())
    lib_put_task_pass_390 (game, run_get_dispatch_input (), sweep_from, TRUE);
  return status;
}


/*
 * lib_instr()
 * lib_put_co_named_term()
 * lib_put_co_reachable()
 * lib_put_co_refusal_pre390()
 *
 * What run380's insides() (entry 44755C) answers for a put line its
 * container fragment could not resolve.  It does not parse fragments at
 * all.  It walks every object with co() -- Short or alias anywhere in the
 * line, wherever the object is -- counting the matches and choosing one
 * (from 4457A1):
 *
 *   - while nothing chosen so far is REACHABLE (a static in the room, or a
 *     dynamic object in the room, held, or worn), each match simply
 *     replaces the choice, so the last match in index order wins;
 *   - once one is, a later match replaces it only when its Short or alias
 *     starts further right in the line than the choice's.
 *
 * Fewer than two matches is the flat "You can't do that!" (445A2A).  Two or
 * more, and the choice is not a container or surface, is "You can't put
 * anything on|inside " & tense(Prefix) & " " & Short & "." with `on` chosen by
 * c("on") (446428); a dynamic container not held is "You are not holding
 * <Prefix> <Short>." and a static one not here "You can't see <Prefix>
 * <Short>.".
 *
 * Measured on cave.taf (3.8) up the tree, nothing referenced present
 * (runner_probes/cave.run380.rtf): `put raft in water` (river water 18, raft
 * 42, pool water 70) is "You can't put anything inside the pool water." (turn
 * 52), and `put amulet on table` (table 48, amulet 64) is "You can't put
 * anything on the star shaped amulet." (turn 212) -- the object, not the
 * table, because the unreachable table never pinned the choice.
 *
 * Returns TRUE when it printed; FALSE leaves the line to the flat refusal,
 * which is also what it answers for the closed and the reachable-container
 * arms, both unmeasured from here.
 */
scr_int
lib_instr (const scr_char *line, const scr_char *term)
{
  scr_int length, index_;

  if (!line || !term || term[0] == NUL)
    return 0;
  length = strlen (term);
  for (index_ = 0; line[index_] != NUL; index_++)
    {
      if (scr_strncasecmp (line + index_, term, length) == 0)
        return index_ + 1;
    }
  return 0;
}

static const scr_char *
lib_put_co_alias (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];

  return lib_first_alias (bundle, vt_key, "Objects", object);
}

static scr_bool
lib_put_co_named_term (scr_gameref_t game, const scr_char *line,
                       scr_int object)
{
  const scr_char *shortname, *alias;

  shortname = prop_get_indexed_string (gs_get_bundle (game),
                                       "Objects", object, "Short");
  if (lib_co_contains (line, shortname))
    return TRUE;
  alias = lib_put_co_alias (game, object);
  return alias && alias[0] != NUL && lib_co_contains (line, alias);
}

static scr_bool
lib_put_co_reachable (scr_gameref_t game, scr_int object)
{
  const scr_int position = gs_object_position (game, object);

  if (obj_directly_in_room (game, object, gs_playerroom (game)))
    return TRUE;
  return !obj_is_static (game, object)
         && (position == OBJ_HELD_PLAYER || position == OBJ_WORN_PLAYER);
}

/* insides()' "further right" test: either name of I past either name of
 * the choice, the choice's name found.  3.9 also wants I's name past PAST,
 * the preposition's position; 3.7/3.8 pass 0. */
static scr_bool
lib_put_co_further_right (scr_gameref_t game, const scr_char *line,
                          scr_int object, scr_int chosen, scr_int past)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *names[2], *chosen_names[2];
  scr_int name, chosen_name;

  names[0] = prop_get_indexed_string (bundle, "Objects", object, "Short");
  names[1] = lib_put_co_alias (game, object);
  chosen_names[0] = prop_get_indexed_string (bundle, "Objects", chosen,
                                             "Short");
  chosen_names[1] = lib_put_co_alias (game, chosen);

  for (chosen_name = 0; chosen_name < 2; chosen_name++)
    {
      const scr_int chosen_at =
          lib_instr (line, chosen_names[chosen_name]);

      if (chosen_at == 0)
        continue;
      for (name = 0; name < 2; name++)
        {
          const scr_int at = lib_instr (line, names[name]);

          if (at > chosen_at && at > past)
            return TRUE;
        }
    }
  return FALSE;
}

static scr_bool
lib_put_co_refusal_pre390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *line = run_get_dispatch_input ();
  scr_int object, matches, chosen;
  scr_bool pinned;

  if (!line || !lib_co_contains (line, "put"))
    return FALSE;

  matches = 0;
  chosen = -1;
  pinned = FALSE;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_put_co_named_term (game, line, object))
        continue;

      matches++;
      if (chosen == -1 || !pinned)
        {
          chosen = object;
          pinned = lib_put_co_reachable (game, object);
        }
      else if (lib_put_co_further_right (game, line, object, chosen, 0))
        chosen = object;
    }
  if (matches < 2)
    return FALSE;

  if (!obj_is_container (game, chosen) && !obj_is_surface (game, chosen))
    {
      const scr_bool on = lib_co_contains (line, "on");

      lib_print_response_object (game,
                                 on ? "You can't put anything on "
                                    : "You can't put anything inside ",
                                 on ? "I can't put anything on "
                                    : "I can't put anything inside ",
                                 on ? "%player% can't put anything on "
                                    : "%player% can't put anything inside ",
                                 chosen, ".\n");
      return TRUE;
    }

  if (!obj_is_static (game, chosen)
      && gs_object_position (game, chosen) != OBJ_HELD_PLAYER)
    {
      pf_buffer_string (filter,
                        lib_select_response (game, "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object_raw (game, chosen);
      pf_buffer_string (filter, ".\n");
      return TRUE;
    }
  if (obj_is_static (game, chosen)
      && !obj_directly_in_room (game, chosen, gs_playerroom (game)))
    return lib_cant_see_named_pre_390 (game, chosen, FALSE, ".\n");

  return FALSE;
}


/*
 * lib_cmd_put_in_that_390()
 * lib_cmd_put_on_that_390()
 *
 * run390 insides() counts, over the whole object table, every object co()
 * finds named in the line, and separately notes the one named after the
 * preposition.  With two or more named, no "all", and none of them after
 * the preposition, it writes "You can't put anything inside that!" / "onto
 * that!" (461769) before any task look-up, and the post-put sweep follows:
 * a claimant named in the line runs the tasks QUIET, so a failing task
 * leaves the refusal standing.
 * pPUTREF39.taf (make_39_putrefprobe.py), run390x: `put pebble and stone in
 * junk` / `on junk`, each with a matching task whose restriction fails, answer
 * "You can't put anything inside that!" / "onto that!"
 * (runner_probes/putref.run390.feed2.txt:8/12), not the FailMessage.
 *
 * The target is insides()' own choice, 3.8's loop (see
 * lib_put_co_refusal_pre390()) with one more test: an object's Short or
 * alias must first appear in the line after InStr(line, Left(prep, 2)),
 * where prep is "onto" when the line holds the whole word on or onto and
 * "inside" otherwise.  That InStr is a raw substring search, so the "in"
 * inside "coin" or the "on" inside "stone" counts as the preposition:
 * `put coin and stone in junk` chooses the stone and answers "You can't put
 * anything inside the stone." (pPUTREF39 T4,
 * runner_probes/putref.run390.txt:8), which is the wrong-kind refusal of
 * lib_put_in_is_valid().  A choice that is a valid open container or surface
 * would put the other object into it; that arm is unmeasured and declines.
 * (The Runner tests the chooser's own name against the preposition with an
 * un-LCased Short, so a capitalised Short may never displace a pinned choice;
 * also unmeasured, not modelled.)
 *
 * These rows sit in the priority table so as to answer ahead of the tasks;
 * anything that is not this case declines to the rows below and to the
 * put-nowhere catch-alls, and 3.7, 3.8 and 4.0 never get here at all.  A
 * line whose whole-word preposition is followed by an object name is left to
 * the ordinary rows.
 */

static scr_int
lib_put_target_390 (scr_gameref_t game, const scr_char *line)
{
  const scr_bool on = lib_co_contains (line, "on")
                      || lib_co_contains (line, "onto");
  const scr_int past = lib_instr (line, on ? "on" : "in");
  scr_int object, chosen;
  scr_bool pinned;

  chosen = -1;
  pinned = FALSE;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_put_co_named_term (game, line, object))
        continue;

      if (chosen == -1 || !pinned)
        {
          const scr_char *const shortname =
              prop_get_indexed_string (gs_get_bundle (game),
                                       "Objects", object, "Short");

          if (lib_instr (line, shortname) > past
              || lib_instr (line, lib_put_co_alias (game, object))
                 > past)
            {
              chosen = object;
              pinned = lib_put_co_reachable (game, object);
            }
        }
      else if (lib_put_co_further_right (game, line, object, chosen, past))
        chosen = object;
    }
  return chosen;
}

/*
 * lib_put_that_refuse_390()
 *
 * lib_put_that_390()'s answer when insides() is left without a target:
 * "You can't put anything inside that!" (or "onto"), then the task sweep.
 */
static scr_bool
lib_put_that_refuse_390 (scr_gameref_t game, const scr_char *line,
                         scr_bool typed_on)
{
  const size_t from = pf_buffer_length (gs_get_filter (game));

  lib_print_response_message (game,
                              typed_on
                              ? "You can't put anything onto that!\n"
                              : "You can't put anything inside that!\n",
                              typed_on
                              ? "I can't put anything onto that!\n"
                              : "I can't put anything inside that!\n",
                              typed_on
                              ? "%player% can't put anything onto that!\n"
                              : "%player% can't put anything inside that!\n");
  lib_put_sweep_390 (game, line, from);
  return TRUE;
}

static scr_bool
lib_put_that_390 (scr_gameref_t game, scr_bool typed_on)
{
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *const in_words[] = {"into", "inside", "in", nullptr};
  const scr_char *const on_words[] = {"onto", "on", nullptr};
  const scr_char *const *words = typed_on ? on_words : in_words;
  std::string lower, tail;
  size_t cut;
  scr_int object, named, chosen;

  if (!lib_put_refusal_first_390 (game) || !line)
    return FALSE;

  /*
   * c("all") skips the two-names test (461646) but not the target choice:
   * with no object named past the preposition there is no target, and
   * insides() refuses at 461769 like any other line.  pPUTFULL39 `put all
   * in junk` / `put all on junk` are "You can't put anything inside
   * that!" / "... onto that!" (run390x runner_probes/putfull.run390.txt).
   * A named target is left to the "put all" rows.  generaltasks has
   * already made "everything" "all" (Replace at 45F225), so `put everything
   * in zzz` / `on zzz` refuse the same way (p39PUT, run390x
   * runner_probes/put.run390.drop.txt).
   */
  if (lib_co_contains (line, "all") || lib_co_contains (line, "everything"))
    {
      if (lib_put_target_390 (game, line) != -1)
        return FALSE;
      return lib_put_that_refuse_390 (game, line, typed_on);
    }

  named = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_put_co_named_term (game, line, object))
        named++;
    }
  if (named < 2)
    return FALSE;

  /* The text after the first whole-word preposition must name nothing. */
  lower = line;
  for (char &c : lower)
    c = scr_tolower (c);
  cut = std::string::npos;
  for (const scr_char *const *word = words; *word; word++)
    {
      const std::string needle = std::string (" ") + *word + " ";
      const size_t at = (" " + lower + " ").find (needle);

      if (at != std::string::npos && (cut == std::string::npos || at < cut))
        cut = at + needle.length () - 1;
    }
  if (cut == std::string::npos || cut > lower.length ())
    return FALSE;
  tail = lower.substr (cut);
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_put_co_named_term (game, tail.c_str (), object))
        return FALSE;
    }

  /* A mid-word preposition can still leave the Runner a target. */
  chosen = lib_put_target_390 (game, lower.c_str ());
  if (chosen != -1)
    {
      const scr_bool on = lib_co_contains (line, "on")
                          || lib_co_contains (line, "onto");

      scr_bool status;

      if (!(on ? lib_put_on_is_valid (game, chosen)
               : lib_put_in_is_valid (game, chosen)))
        {
          lib_put_refusal_swept = FALSE;
          return TRUE;
        }

      gs_clear_multiple_references (game);
      lib_put_mid_word_390 = TRUE;
      status = lib_put_named_pre400 (game, chosen, on);
      lib_put_mid_word_390 = FALSE;
      return status;
    }

  return lib_put_that_refuse_390 (game, line, typed_on);
}

scr_bool
lib_cmd_put_in_that_390 (scr_gameref_t game)
{
  return lib_put_that_390 (game, FALSE);
}

scr_bool
lib_cmd_put_on_that_390 (scr_gameref_t game)
{
  return lib_put_that_390 (game, TRUE);
}

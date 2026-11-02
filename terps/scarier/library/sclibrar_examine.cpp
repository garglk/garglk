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
 * Examining the player, NPCs and objects, absent-object refusals, and
 * the listing of container/supporter contents.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_examine_self_pre390()
 *
 * The 3.7/3.8 self description fallback, closed with its answer break.
 * 3.7/3.8 build the whole reply in one piece (run370 435AED-435BEA,
 * run380 43D43E): the fallback's own full stop comes only with the
 * sitting/lying clause, the standing-on clause follows with two spaces
 * and no full stop before it, the object is named by its raw Prefix,
 * and one "." closes the lot.  run370x p3738sit3 (2026-09-19): "...the
 * circumstances.  You are sitting down on a stool." and "...the
 * circumstances  You are standing on a stool.".
 */
static void
lib_examine_self_pre390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int parent = gs_playerparent (game);

  pf_buffer_string (filter,
                    lib_select_response (game,
                                   "You are as well as can be expected,"
                                   " considering the circumstances",
                                   "I am as well as can be expected,"
                                   " considering the circumstances",
                                   "%player% is as well as can be"
                                   " expected, considering the"
                                   " circumstances"));
  if (gs_playerposition (game) == 1 || gs_playerposition (game) == 2)
    {
      pf_buffer_string (filter, ".  ");
      pf_buffer_string (filter,
                        gs_playerposition (game) == 1
                        ? lib_select_response (game,
                                               "You are sitting down",
                                               "I am sitting down",
                                               "%player% is sitting down")
                        : lib_select_response (game,
                                               "You are lying down",
                                               "I am lying down",
                                               "%player% is lying down"));
    }
  if (parent != -1)
    {
      if (gs_playerposition (game) == 0)
        {
          pf_buffer_string (filter, "  ");
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You are standing",
                                                 "I am standing",
                                                 "%player% is standing"));
        }
      pf_buffer_string (filter, " on ");
      lib_print_object_raw (game, parent);
    }
  pf_buffer_character (filter, '.');
  /*
   * And that is the whole reply: neither run370's block (435A9B-435BEA)
   * nor run380's (43D3EC-43D53B) walks the worn objects, so a 3.7/3.8
   * `x me` never says "You are wearing ..." -- only `i` does.  run370x/
   * run380x p37SITN/p38SITN `wear hat sit` then `x me`
   * (runner_probes/sitn.run370.rtf, runner_probes/sitn.run380.rtf,
   * 2026-09-20): "...the circumstances.  You are sitting down." while `i`
   * lists the hat worn.
   */
  pf_buffer_answer_break (filter);
}


/*
 * lib_examine_self_close()
 *
 * After the description: if not just standing on the floor, say more;
 * otherwise close an authored description with its full stop.
 */
static void
lib_examine_self_close (scr_gameref_t game, const scr_char *description)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *position = nullptr;

  switch (gs_playerposition (game))
    {
    case 0:
      position = lib_select_response (game,
                                      "You are standing",
                                      "I am standing",
                                      "%player% is standing");
      break;
    case 1:
      position = lib_select_response (game,
                                      "You are sitting down",
                                      "I am sitting down",
                                      "%player% is sitting down");
      break;
    case 2:
      position = lib_select_response (game,
                                      "You are lying down",
                                      "I am lying down",
                                      "%player% is lying down");
      break;
    }

  if (position
      && !(gs_playerposition (game) == 0 && gs_playerparent (game) == -1))
    {
      pf_buffer_string (filter, "  ");
      pf_buffer_string (filter, position);
      if (gs_playerparent (game) != -1)
        {
          pf_buffer_string (filter, " on ");
          lib_print_object_np (game, gs_playerparent (game));
        }
      pf_buffer_character (filter, '.');
    }
  else if (!scr_strempty (description)
           && prop_get_taf_version (bundle) >= TAF_VERSION_390)
    {
      /*
       * The Runner closes the description with a full stop if the author
       * did not.  It looks at the raw text's last character BEFORE any ALR
       * replacement: run390 examines() @44C488, loc_44C1F1-44C21E, appends
       * "." when Right$(text, 1) <> "."; run400 Proc_19_87_471F94,
       * loc_471C6D-471D40, also lets "!", ")", "%" and "?" stand.  (When
       * the position clause above is printed its own "." satisfies the same
       * test, so only the bare description needs it here.)  Measured on
       * Archie's Birthday (3.90, run390, 2026-09-05): its PlayerDesc is the
       * ALR key "[player=%player val%]", so the Runner appends "." to the
       * key and the substituted paragraph ends "...self-delusions."; and on
       * yak_shaving (4.00, run400, same day): `x me` answers "You are
       * somewhat raggedy looking after your journey." for a PlayerDesc with
       * no full stop.  3.7/3.8 (run370 examines @435C9C, run380 @43D5EC)
       * build the reply differently and are not modelled.
       */
      const scr_char last = description[strlen (description) - 1];
      const scr_bool closed = prop_get_taf_version (bundle) >= TAF_VERSION_400
                              ? strchr (".!)%?", last) != nullptr
                              : last == '.';
      if (!closed)
        pf_buffer_character (filter, '.');
    }
}


/*
 * lib_examine_self_worn()
 *
 * List each object worn by the player (3.9+; 3.7/3.8 never do, see
 * lib_examine_self_pre390()).
 */
static void
lib_examine_self_worn (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  lib_list_t list;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_position (game, object) == OBJ_WORN_PLAYER)
        list.push_back (object);
    }
  if (!list.empty ())
    {
      lib_print_clause (game, TRUE,
                        "You are wearing ",
                        "I am wearing ",
                        "%player% is wearing ");
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_character (filter, '.');
    }
}


/*
 * lib_cmd_examine_self()
 *
 * Show the long description of a player.
 */
scr_bool
lib_cmd_examine_self (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2];
  scr_int task;
  const scr_char *description;

  /*
   * The darkness fork again, this time on its `x me` arm: run390 44C424
   * answers "<player> can just make out that <you> <are> okay." (44C430) in
   * place of the whole self description.  The two slots are the person
   * array's 5 and 4, filled at 464800-4648A8 -- "I"/"am" in the first
   * person, "you"/"are" in the second; a third-person player cannot reach
   * this at all, lib_get_perspective() clamping pre-4.0 to those two.  See
   * lib_room_is_dark() and lib_cmd_examine_other().
   */
  if (lib_room_is_dark (game, gs_playerroom (game)))
    return lib_print_response_message (game,
                    "You can just make out that you are okay.\n",
                    "I can just make out that I am okay.\n",
                    "%player% can just make out that"
                    " %player_pronoun% is okay.\n");

  /* Get selection task. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "Task";
  task = prop_get_integer (bundle, "I<-ss", vt_key) - 1;

  /* Select either the main or the alternate description. */
  if (gs_task_valid (game, task) && gs_task_done (game, task))
    vt_key[1].string = "AltDesc";
  else
    vt_key[1].string = "PlayerDesc";

  /* Print the description, or default response. */
  description = prop_get_string (bundle, "S<-ss", vt_key);
  if (!scr_strempty (description))
    pf_buffer_string (filter, description);
  else if (prop_get_taf_version (bundle) < TAF_VERSION_390)
    {
      lib_examine_self_pre390 (game);
      return TRUE;
    }
  else
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                       "You are as well as can be expected,"
                                       " considering the circumstances.",
                                       "I am as well as can be expected,"
                                       " considering the circumstances.",
                                       "%player% is as well as can be expected,"
                                       " considering the circumstances."));
    }

  lib_examine_self_close (game, description);
  lib_examine_self_worn (game);

  pf_buffer_answer_break (filter);
  return TRUE;
}


/*
 * The score is run400's noun score, Proc_21_58_463640 4632AC-463387,
 * shared with the unhandled-verb resolver below: 1 if the object's Short
 * name is whole-phrase in the typed line, +1 if ANY alias is (the alias
 * loop leaves at its first hit, 463304), then +1 for every Prefix word
 * present.  A multi-word Short scores only as the whole phrase -- "east
 * wall" earns nothing from `x wall`; it is the alias "wall" that scores.
 * 0 means the object was never a candidate at all (var_9E).
 */

static scr_int
lib_absent_seen_object (scr_gameref_t game)
{
  const scr_char *input;
  scr_int index_, object;

  if (!lib_is_version_400 (game))
    return -1;

  /*
   * This is run400's up-front resolver Proc_21_58_463640, called once per
   * line from generaltasks (48A3F5, mode 0) and read back by the examine
   * resolver Proc_19_88_457034 before anything else (456DFC).  Pass 1
   * scores objects that are present AND seen (463119-463137); only when
   * that leaves no unique winner does the re-entry at 46360D-46363B run
   * pass 2 over every SEEN object (463143-463156), present or not.  The
   * winner is the unique maximum score; equal scores tie (4633C3-46341F
   * encodes the tie as a negative result), and a tie or no candidate falls
   * back to 457034's own pass A, co(i, 3), which needs a same-named object
   * PRESENT and so answers &HFF, "<player> see no such thing."  (457034
   * does have passes after A -- see lib_examine_referencedob_400() -- but
   * with no present namesake pass A marks nothing and they never run; the
   * "co(i, 4)" at 456E6A is a single vestigial call, not a loop.)
   *
   * Measured on cowboyblues (4.00, runner_probes/cowboyblues.run400.txt line
   * 1070): `x wall` in the Sheriff's Office after visiting the Back Room
   * ("east wall" 91, alias "wall") and Blood Alley ("walls" 96, alias "wall")
   * answers "You see no such thing." -- both score 1 on the alias and tie.
   * The old Short-word count here gave "east wall" 1 and "walls" 0 and wrongly
   * picked 91.  Measured on humbug (runner_probes/humbug.run400.b.txt lines
   * 1602-1604): `X machine` with only the washing machine (81, Short
   * "machine") seen answers "I can't see the washing machine from here!",
   * and `X chute` against several seen "chute"s ties and prints the ALR'd
   * "Nothing Special.".
   *
   * The candidates are every seen object whose name words the line holds,
   * not only what %object% bound: House's `open bathroom door` from the
   * Hallway, the door seen (examined, even) on the Landing, answers "You
   * can't open that." (runner_probes/house_sober.run400.doorprobe.txt,
   * runner_probes/house_sober.run400.txt T78-83), because the street's sign
   * (alias "door") and front door (Short "door") score 1 alongside it, the tie
   * is negative, and openclose leaves at 4756BC for therest's flat refusal.
   */
  input = run_get_dispatch_input ();
  if (!input)
    return -1;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      /* Something the noun names is here; the ordinary path handles it. */
      if (game->object_references[index_]
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        return -1;
    }

  /* Pass 1, present and seen: a unique winner is present, not absent. */
  if (lib_verb_object_resolve_400_string (game, input, nullptr, TRUE) >= 0)
    return -1;

  /* Pass 2, every seen object; a tie or no score is -1 as well. */
  object = lib_verb_object_resolve_400_string (game, input, nullptr, FALSE);
  if (object < 0
      || !game->object_references[object]
      || obj_indirectly_in_room (game, object, gs_playerroom (game)))
    return -1;

  return object;
}

scr_bool
lib_cant_see_absent_object (scr_gameref_t game,
                            const scr_char *suffix, scr_bool is_definite)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_int object = lib_absent_seen_object (game);

  if (object == -1)
    return FALSE;

  var_set_ref_object (vars, object);
  pf_buffer_string (filter,
                    lib_select_response (game, "You can't see ",
                                         "I can't see ",
                                         "%player% can't see "));
  if (is_definite)
    lib_print_object_np (game, object);
  else
    lib_print_object (game, object);
  pf_buffer_string (filter, suffix);
  return TRUE;
}


/*
 * lib_absent_named_object_pre_390()
 * lib_cant_see_named_pre_390()
 *
 * The 3.7 and 3.8 Runners' co() (run380 42DE60) matches an object's Short or
 * alias wherever the object is, seen or not, and each verb handler then
 * answers for the first match it cannot reach.  Measured on p38EXAM and
 * p37EXAM (make_3738_examprobe.py; run380 runner_probes/exam.run380.txt and
 * runner_probes/exam.run380.2.txt, run370 runner_probes/exam.run370.txt and
 * runner_probes/exam.run370.2.txt, 2026-09-14), with the statue seen in the
 * North Room and a gem in a room the player never enters, both named from
 * elsewhere:
 *
 *     command       3.80                                3.70
 *     x statue      seen:   You can't see a statue      the same
 *                           from here!
 *                   unseen: You can't see that.
 *     take statue   You can't see a statue from here!   the same
 *     wear statue   You are not holding a statue.       the same
 *     open statue   seen:   You can't see a statue.     You can't see the
 *                   unseen: Open what?                    statue.
 *     close statue  as open, "Close what?"              as open
 *     buy statue    I don't think that is for sale.     as open
 *
 * The indefinite "a statue" is each handler's own Prefix & " " & Short:
 * examines 43D258, takes 43E4CA, wears 433218, openclose 42F1B1/42F36B.
 * Only examines and openclose read the seen byte (record field 40).  3.7's
 * open, close and buy never get as far as a verb branch: its openclose
 * (426770) has no refusals at all, and therest's up-front test
 * (43D169-43D187) names the first matched object that is not here through
 * tense(), the definite form.  run390's co() wants the object present, so
 * none of this survives into 3.9.
 *
 * takes() walks every object and overwrites a message that still ends in
 * " from here!" (43E3F6), so the LAST match speaks: cave.taf `take parchment`
 * is "You can't see half of a parchment from here!", not the old parchment
 * before it (runner_probes/cave.run380.rtf turns 115/145).  The other handlers
 * are measured on single matches only and keep the first.
 *
 * Returns the first (or, with last, the last) object the line names, in
 * index order, or -1 when the
 * game is 3.9 or later, the line names nothing, or something it names is
 * within reach -- that one belongs to the ordinary handlers above these rows.
 */
scr_int
lib_absent_named_object_pre_390 (scr_gameref_t game, scr_bool last)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object, first;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return -1;
  if (!input)
    return -1;
  if (!uip_match ("* %object%", input, game)
      && !uip_match ("* %object% *", input, game))
    return -1;

  first = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!game->object_references[object])
        continue;

      if (obj_indirectly_in_room (game, object, gs_playerroom (game))
          || obj_indirectly_held_by_player (game, object))
        return -1;

      if (first == -1 || last)
        first = object;
    }

  return first;
}

scr_bool
lib_cant_see_named_pre_390 (scr_gameref_t game, scr_int object,
                            scr_bool is_definite, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    lib_select_response (game, "You can't see ",
                                         "I can't see ",
                                         "%player% can't see "));
  if (is_definite)
    lib_print_object_np (game, object);
  else
    lib_print_object_raw (game, object);
  pf_buffer_string (filter, suffix);
  return TRUE;
}


/*
 * lib_therest_absent_370()
 *
 * run370's therest() opens with a test the later Runners dropped
 * (43D169-43D187): when the line names an object that is not here, it
 * answers "You can't see " & tense(Prefix) & " " & Short & "." and leaves
 * before any verb arm.  The caller only asks for lines that get this far in
 * the Runner -- see run_therest_absent_370().  Measured on p37GOTO
 * (harness/make_37_gotoprobe.py), run370x
 * runner_probes/goto.run370.cantsee.rtf: with the stone in another room, go,
 * enter, push, smell, kiss, turn, jump, buy, open, sing, look, climb, sit on
 * and fly the stone are all "You can't see the stone.", and with it present
 * go, push and kiss answer as ever.
 *
 * Returns TRUE if the line was answered.
 */
scr_bool
lib_therest_absent_370 (scr_gameref_t game)
{
  scr_int object;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380)
    return FALSE;

  object = lib_absent_named_object_pre_390 (game);
  if (object == -1)
    return FALSE;

  return lib_cant_see_named_pre_390 (game, object, TRUE, ".\n");
}


/*
 * lib_list_npc_inventory()
 *
 * List objects carried and worn by an NPC.
 */
static scr_bool
lib_list_npc_inventory (scr_gameref_t game, scr_int npc, scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool wearing;
  lib_list_t list;

  /*
   * Find and list each object worn by the NPC.  Like container listings,
   * this reveals the objects -- the Runner marks NPC possessions seen
   * only when they're listed to the player.
   */
  wearing = lib_reveal_children (game, npc, OBJ_WORN_NPC, list) > 0;
  if (wearing)
    {
      lib_new_clause (game, is_described);
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter, " is wearing ");
      lib_print_list (game, list, lib_print_object, " and ");
    }

  /* Find and list each object owned by the NPC. */
  lib_reveal_children (game, npc, OBJ_HELD_NPC, list);
  if (!list.empty ())
    {
      if (!wearing)
        {
          lib_new_clause (game, is_described);
          lib_print_npc_np (game, npc);
          pf_buffer_string (filter, " is");
        }
      else
        {
          /*
           * "... is wearing a hat, and carrying a document." -- the second
           * clause has no subject and no second "is".  run390 and run400
           * build it the same way: the worn-items branch sets a flag (var_AA),
           * and the carried-items branch tests it, appending ", and" and
           * jumping straight past the name and the " is" to " carrying "
           * (run400 @45BA6B/45BA91/45BAA5, run390 @382E1/382FF/38311).
           *
           * 3.8 does repeat the subject: run380 @2CA67 appends ", and " and
           * then falls through to its own name & " is", giving "... is
           * wearing a hat, and Grandad is carrying a document."  (run370 has
           * no NPC worn/carried listing at all.)  Measured on humbug.taf,
           * 2026-08-24: run400 prints "Grandad is wearing a hat, and
           * carrying a document."
           */
          if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
            {
              pf_buffer_string (filter, ", and ");
              lib_print_npc_np (game, npc);
              pf_buffer_string (filter, " is");
            }
          else
            pf_buffer_string (filter, ", and");
        }
      pf_buffer_string (filter, " carrying ");
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_character (filter, '.');
    }
  else
    {
      if (wearing)
        pf_buffer_character (filter, '.');
    }

  /* Return TRUE if anything worn or carried. */
  return wearing || !list.empty ();
}


/*
 * lib_cmd_examine_npc()
 *
 * Show the long description of the most recently referenced NPC, and a
 * list of what they're wearing and carrying.
 */
static void
lib_describe_npc (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int task, resource;
  const scr_char *description;

  /* Get selection task. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Task";
  task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;

  /* Select either the main or the alternate description. */
  if (gs_task_valid (game, task) && gs_task_done (game, task))
    {
      vt_key[2].string = "AltText";
      resource = 1;
    }
  else
    {
      vt_key[2].string = "Descr";
      resource = 0;
    }

  /* Print the description, or a default message if none. */
  description = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (description))
    pf_buffer_string (filter, description);
  else
    {
      pf_buffer_string (filter, "There's nothing special about ");
      lib_print_npc_np (game, npc);
      pf_buffer_character (filter, '.');
    }

  /* Handle any associated resource. */
  vt_key[2].string = "Res";
  vt_key[3].integer = resource;
  res_handle_resource (game, "sisi", vt_key);

  /* Print what the NPC is wearing and carrying. */
  lib_list_npc_inventory (game, npc, TRUE);

  pf_buffer_answer_break (filter);

  /*
   * In 4.0 examining a character is not a turn: run400 counts no turn,
   * moves no walker and ticks no event after it, exactly as it treats its
   * own `turns` and `score`.  3.9 counts it like any other command.
   *
   * Measured 2026-08-29 under Wine.  run400, arena probes EV14/EV15/EV16
   * (harness/make_arena_probe.py; runner_probes/ev14.run400.txt,
   * runner_probes/ev15.run400.txt, runner_probes/ev16.run400.txt): a one-turn
   * event started by the previous command finishes on the `z` AFTER `x bob`,
   * not on `x bob` itself; a looping walker with enter/exit lines prints
   * nothing on that turn; and `turns` reads 0 after `x bob`, `x carl` (empty
   * description), `look at bob` and `examine bob`, while `x me` and `x pebble`
   * count.  run390, BobBobsly.taf (3.90), runner_probes/bob_bobsly.run390.txt:
   * `turns` climbs by one across `x bouncer`.
   *
   * This is Beanstalk turn 45: the stranger's greeting is a one-turn event
   * started by `sell cow`, the player examines him and walks east, and by
   * the time the Runner ticks the event again the player is no longer on
   * the road it is restricted to.
   */
  if (lib_is_version_400 (game))
    game->is_admin = TRUE;
}

scr_bool
lib_cmd_examine_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous, was_admin;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc_pick (game, "examine", &is_ambiguous,
                                   NPC_PICK_LAST);
  if (npc == -1)
    return is_ambiguous;

  was_admin = game->is_admin;
  lib_describe_npc (game, npc);

  /*
   * 4.0: the NPC arm of characters() (47FE19-480157) stores no not-a-turn
   * flag; a plain `x <npc>` is administrative only because examines() ran
   * first and found no object, whose "see no such thing" arm stores
   * MemVar_494281 at 471F02.  When the 463640 scorer's seen pass instead
   * settles on a unique object that is not here, examines answers "can't
   * see <it> from here!" (471958) with no flag, characters() overwrites it
   * with the description, and the line is a turn.  Humbug T634 `X robot` at
   * the Bus Stop, with the static robot (213) seen in the tunnel: run400
   * draws for the tick and Grandad arrives on that line
   * (runner_transcripts/humbug.txt).
   */
  if (lib_is_version_400 (game))
    {
      const scr_char *input = run_get_dispatch_input ();
      scr_int object;

      object = lib_verb_object_resolve_400_string (game, input, nullptr, FALSE);
      if (object >= 0
          && !obj_indirectly_in_room (game, object, gs_playerroom (game)))
        game->is_admin = was_admin;
    }
  return TRUE;
}


/*
 * lib_list_in_object_normal()
 *
 * List the objects in a given container object, normal format listing.
 */
static scr_bool
lib_list_in_object_normal (scr_gameref_t game,
                           scr_int container, scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  lib_list_t list;

  /* List out the containers contained in this container. */
  lib_collect_children (game, container, OBJ_IN_OBJECT, list);
  if (!list.empty ())
    {
      if (is_described)
        pf_buffer_string (filter, "  ");
      pf_buffer_string (filter, "Inside ");
      lib_print_object_np (game, container);
      /*
       * " is ", never " are ", however plural the contents.  The Runner has
       * an is/are helper (run400 isare(), Proc_19_69_4507BC @4507BC) and uses
       * it for "Also here is/are", but this listing does not call it: the
       * string is a literal, at run400 46A7C7 and run390 43E245's sibling.
       * Measured on humbug.taf, 2026-08-24: run400 prints "On the triangular
       * table is some swimming goggles, a watch, a musket and a china doll."
       * Scarier deliberately keeps number agreement instead ("... table are
       * some swimming goggles, ..."): the literal is a display accident, not
       * behaviour (deviation policy).
       */
      pf_buffer_string (filter,
                        lib_select_list_plurality (game, container, list,
                                                   " is ", " are "));
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_character (filter, '.');
    }

  /* Return TRUE if anything listed. */
  return !list.empty ();
}


/*
 * lib_list_in_object_alternate()
 *
 * List the objects in a given container object, alternate format listing.
 */
static scr_bool
lib_list_in_object_alternate (scr_gameref_t game,
                              scr_int container, scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  lib_list_t list;

  /* List out the objects contained in this object. */
  lib_collect_children (game, container, OBJ_IN_OBJECT, list);
  if (!list.empty ())
    {
      lib_new_clause (game, is_described);
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_string (filter,
                        list.size () == 1
                        ? lib_select_plurality (game, list[0],
                                                " is inside ", " are inside ")
                        : " are inside ");

      /* Print out the container. */
      lib_print_object_np (game, container);
      pf_buffer_character (filter, '.');
    }

  /* Return TRUE if anything listed. */
  return !list.empty ();
}


/*
 * lib_list_in_object_joined()
 *
 * List the objects in a given container object, run onto the end of a
 * surface listing already printed for the same object.
 *
 * The Runner's combined lister keeps a flag, var_9E, that it sets when it
 * has printed an "on" listing, and the container half then tests it before
 * anything else: at run400 loc_46A786 var_9E == 1 appends the literal
 * ", and inside is " and jumps straight to loc_46A7E0, the list loop the
 * "Inside <cont> is " branch also falls into.  So the joined form takes no
 * format choice, names no container, opens no new sentence, and gets the
 * turn's single closing '.' (appended once at loc_46A8C6) -- see
 * lib_list_in_on_object() for where the two halves meet.
 */
static scr_bool
lib_list_in_object_joined (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);
  lib_list_t list;

  /* List out the objects contained in this container. */
  lib_collect_children (game, container, OBJ_IN_OBJECT, list);
  if (!list.empty ())
    {
      /* The Runner's literal is ", and inside is "; Scarier agrees. */
      pf_buffer_string (filter,
                        lib_select_list_plurality (game, container, list,
                                                   ", and inside is ",
                                                   ", and inside are "));
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_character (filter, '.');
    }

  /* Return TRUE if anything listed. */
  return !list.empty ();
}


/*
 * lib_list_in_object()
 *
 * List the objects in a given container object.
 *
 * The Runner has two distinct styles for listing a container's contents, and
 * run400.exe selects purely on the *number* of contained objects.  The
 * listing helper at 0006A418 in run400.txt counts the objects whose position
 * is 246 (in object) and whose parent is this container into var_98, then
 *
 *   0006A49E   if (var_98 == 1 && var_9E == 0)  ->  "<obj> is inside <cont>."
 *   0006A607   if (var_98 == 2 && var_9E == 0)  ->  "<a> and <b> are inside <cont>."
 *   0006A786   otherwise                        ->  "Inside <cont> is <list>."
 *
 * -- i.e. one or two objects get the alternate (postfixed) format and three or
 * more get the normal (prefixed) one, with no test on the container being
 * static or dynamic anywhere in the chain.  var_9E == 1 is the case where
 * an "on" listing for this same object has already gone out; that one is
 * lib_list_in_object_joined(), and note that its guard comes FIRST, so a
 * surface listing forces the joined wording whatever the count -- the
 * count-1 and count-2 branches above are both guarded on var_9E == 0.
 *
 * Confirmed against the real Runner in the "It's Easter, Peeps!" walkthrough
 * transcript, which exercises all three: "An umbrella is inside the umbrella
 * stand." (static, 1), "A crumpled note and a candy coin are inside the pay
 * phone." (static, 2), "A few bills and a couple of photographs are inside
 * your wallet." (dynamic, 2) and "Inside the Easter basket is a strip of
 * candy dots, ... and a lollipop." (dynamic, 6).
 *
 * The part-of-NPC test below is not in run400's chain, but it is kept as an
 * extra alternative so that containers worn by or attached to an NPC keep the
 * format they had before this rule was derived; it can only matter for three
 * or more contained objects.
 *
 * Before 3.9 there is no choice to make: the alternate format does not exist.
 * run370 and run380 carry only the "  Inside " literal -- run380 @43D0B1 is
 * the whole of it, `"  Inside " & tense(prefix) & " " & short & " is " &
 * list` -- and neither binary contains " is inside " or " are inside "
 * anywhere; both strings first appear in run390.  Measured live under Wine
 * 2026-08-24 on mikes.taf (3.80) in run380, whose toilet holds exactly one
 * object: `open toilet` answers "You open the toilet.  Inside the toilet is
 * a poop." where 3.9 and 4.0 would say "A poop is inside the toilet."  The
 * same replay shows it again on `look in mailbox`.
 */
scr_bool
lib_list_in_object (scr_gameref_t game, scr_int container,
                    scr_bool is_described, scr_bool joined)
{
  scr_bool use_alternate_format = FALSE;
  scr_int count;
  lib_list_t list;

  /*
   * Count the objects this container holds.  Listing a container's
   * contents is also what reveals them: the Runner sets each listed
   * object's seen flag here, and until then the object can't be referred
   * to at all (in version 3.9 and later games).
   */
  count = lib_reveal_children (game, container, OBJ_IN_OBJECT, list);

  /*
   * A surface listing for this same object has just been printed, so the
   * two clauses are run together and there is no format to choose.
   */
  if (joined)
    return lib_list_in_object_joined (game, container);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    use_alternate_format = FALSE;
  else if (count == 1 || count == 2)
    use_alternate_format = TRUE;
  else if (obj_is_static (game, container)
           && gs_object_position (game, container) == OBJ_PART_NPC)
    use_alternate_format = TRUE;

  /* List contained objects using the selected handler. */
  return use_alternate_format
         ? lib_list_in_object_alternate (game, container, is_described)
         : lib_list_in_object_normal (game, container, is_described);
}


/*
 * lib_list_on_object_normal()
 *
 * List the objects on a given surface object, normal format listing.
 */
static scr_bool
lib_list_on_object_normal (scr_gameref_t game, scr_int supporter,
                           scr_bool is_described, scr_bool omit_period)
{
  const scr_filterref_t filter = gs_get_filter (game);
  lib_list_t list;

  /* List out the objects standing on this surface. */
  lib_collect_children (game, supporter, OBJ_ON_OBJECT, list);
  if (!list.empty ())
    {
      if (is_described)
        pf_buffer_string (filter, "  ");
      pf_buffer_string (filter, "On ");
      lib_print_object_np (game, supporter);
      /*
       * The Runner's " is " is unconditional; Scarier agrees with the list
       * -- see lib_list_in_object_normal().
       */
      pf_buffer_string (filter,
                        lib_select_list_plurality (game, supporter, list,
                                                   " is ", " are "));
      lib_print_list (game, list, lib_print_object, " and ");
      if (!omit_period)
        pf_buffer_character (filter, '.');
    }

  /* Return TRUE if anything listed. */
  return !list.empty ();
}


/*
 * lib_list_on_object_alternate()
 *
 * List the objects on a given surface object, alternate format listing.
 */
static scr_bool
lib_list_on_object_alternate (scr_gameref_t game, scr_int supporter,
                              scr_bool is_described, scr_bool omit_period)
{
  const scr_filterref_t filter = gs_get_filter (game);
  lib_list_t list;

  /* List out the objects standing on this object. */
  lib_collect_children (game, supporter, OBJ_ON_OBJECT, list);
  if (!list.empty ())
    {
      lib_new_clause (game, is_described);
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_string (filter,
                        list.size () == 1
                        ? lib_select_plurality (game, list[0],
                                                " is on ", " are on ")
                        : " are on ");

      /* Print out the surface. */
      lib_print_object_np (game, supporter);
      if (!omit_period)
        pf_buffer_character (filter, '.');
    }

  /* Return TRUE if anything listed. */
  return !list.empty ();
}


/*
 * lib_list_on_object()
 *
 * List the objects on a given surface object.
 *
 * The Runner picks between the same two styles it uses for containers, on
 * the same rule: one or two objects get the alternate (postfixed) format,
 * three or more the normal (prefixed) one -- see lib_list_in_object() for
 * the run400 derivation.  SCARE used the postfixed format unconditionally
 * for surfaces.  Both halves are visible in the Professor Von Witt
 * walkthrough transcript (Runner 4.00, 2026-08-18): "On the shelves is a
 * leg to stand on, a handy dandy extra hand, the sloppy jalopy, a portable
 * doorknob and a pollen popper upper." (5 objects) against "A container of
 * Reggie's Remedy Rust Resolvent is on the shelves." (1 object).
 *
 * And it drops the alternate format before 3.9 for the same reason
 * containers do: " is on " and " are on " are absent from run370 and run380
 * and first appear in run390, which leaves "On <surface> is <list>." as the
 * only wording those two can produce.
 */
static scr_bool
lib_list_on_object (scr_gameref_t game, scr_int supporter,
                    scr_bool is_described, scr_bool omit_period)
{
  scr_bool use_alternate_format = FALSE;
  scr_int count;
  lib_list_t list;

  /*
   * Count the objects this surface holds, marking each one seen as we
   * go -- as with containers, the listing is what reveals the contents
   * to the parser.
   */
  count = lib_reveal_children (game, supporter, OBJ_ON_OBJECT, list);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    use_alternate_format = FALSE;
  else if (count == 1 || count == 2)
    use_alternate_format = TRUE;
  else if (obj_is_static (game, supporter)
           && gs_object_position (game, supporter) == OBJ_PART_NPC)
    use_alternate_format = TRUE;

  /* List objects on the surface using the selected handler. */
  return use_alternate_format
         ? lib_list_on_object_alternate (game, supporter, is_described,
                                         omit_period)
         : lib_list_on_object_normal (game, supporter, is_described,
                                      omit_period);
}


/*
 * lib_list_in_on_object()
 *
 * List both what is on an object and what is inside it.
 *
 * The Runner does the two together, in one helper -- whatisinon(), run400
 * Proc_19_26_46A950 @46A950 (mdlSpreadTheLoad.bas:21880), body 46A058-46A94A.
 * Its second argument is a mode: the "on" half is guarded on mode <> 0
 * (loc_46A083) and the "in" half on mode <> 1 (loc_46A41E), so mode 0 is
 * containers only, mode 1 surfaces only, and mode 2 both.  Of its four
 * callers, openclose() (@475852) and the room-description lister in
 * General.bas (@479919) pass 0, while inventory() (@45C2C8) and examines()
 * (@471928) pass 2 -- which is why `open desk` and `x desk` word the same
 * desk differently.
 *
 * When both halves have something to say, the surface goes FIRST and the
 * container is run onto the end of the same sentence:
 *
 *   > x desk
 *   Your Desk is open.  Your Coffee Mug and The Memo are on Your Desk, and
 *   inside is Gun Holster, Your Cell Phone, Neatly Wrapped Gift and Your
 *   Badge.
 *
 * measured live in run400 on The_X-Files_A_New_Beginning.taf (4.00),
 * 2026-08-25, runner_probes/xfiles.run400.txt line 9.  SCARE had it the other
 * way and as two sentences, "Inside Your Desk is ...  Your Coffee Mug and The
 * Memo are on Your Desk."  The single closing '.' is appended once at the end
 * of whatisinon (loc_46A8C6, and only if anything was added at all), so the
 * "on" clause does not carry one when an "in" clause follows it.
 *
 * All of that is 3.9-and-later.  Before 3.9 there is no combined lister at
 * all: run380 has whatisin1() @4297AC and whatisin2() @42998C as separate
 * subs, and its examines() (@43D5EC) carries its own listing inline, an
 * either/or on one field -- run380 loc_43D07A prints "  Inside <obj>" when
 * that field is 1 and loc_43D0D0 prints "  On <obj>" when it is 2, never
 * both.  ", and inside is " is absent from run370.exe and run380.exe and
 * first appears in run390.exe (same boundary as " is inside " and " is on ",
 * counted in the binaries 2026-08-25), so pre-3.9 games keep the older
 * container-then-surface pair of sentences here rather than take a wording
 * their Runner cannot produce.
 */
scr_bool
lib_list_in_on_object (scr_gameref_t game, scr_int object,
                       scr_bool is_described)
{
  scr_bool is_open_container, on_listed = FALSE, in_listed = FALSE;
  scr_bool has_contents = FALSE;
  scr_int contained;

  is_open_container = obj_is_container (game, object)
                      && gs_object_openness (game, object) <= OBJ_OPEN;

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    {
      if (is_open_container)
        in_listed = lib_list_in_object (game, object, is_described, FALSE);
      /*
       * A closed object lists nothing, surface or not: run380's examine
       * gates its whole in/on listing on field 44 <> 5 (@43CFF3), as do
       * whatisin1 and whatisin2 -- Crime_Adventure.taf's closed dresser,
       * a surface with Openable set (decompile-read, not measured live).
       */
      if (obj_is_surface (game, object)
          && gs_object_openness (game, object) != OBJ_CLOSED)
        on_listed = lib_list_on_object (game, object,
                                        is_described || in_listed, FALSE);
      return in_listed || on_listed;
    }

  /*
   * Look ahead for contents, so that the surface listing knows to leave
   * its sentence open for them.
   */
  if (is_open_container)
    {
      GS_FOR_EACH_OBJECT (game, contained)
        {
          if (gs_object_position (game, contained) == OBJ_IN_OBJECT
              && gs_object_parent (game, contained) == object)
            {
              has_contents = TRUE;
              break;
            }
        }
    }

  /* For surface objects, list out what's on them -- first. */
  if (obj_is_surface (game, object))
    on_listed = lib_list_on_object (game, object, is_described, has_contents);

  /* For open container objects, list out what's in them. */
  if (is_open_container)
    in_listed = lib_list_in_object (game, object,
                                    is_described || on_listed, on_listed);

  return on_listed || in_listed;
}


/*
 * lib_list_object_state()
 *
 * Describe the state of a stateful object.
 */
static scr_bool
lib_list_object_state (scr_gameref_t game, scr_int object, scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_bool is_statussed;
  scr_char *state;

  /* Get object statefulness. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "CurrentState";
  is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;

  /* Ensure this is a stateful object. */
  if (is_statussed)
    {
      lib_new_clause (game, is_described);
      lib_print_object_np (game, object);

      /*
       * run400 examines() builds this line at 4718E1 from a literal " is "
       * and the raw state name, the same as the open/closed suffix above
       * it; isare() is never consulted.  magicshow (4.00, Prefix "the",
       * Short "gates", states "Up|Down") measures `examine gates` as "The
       * gates are down." (runner_probes/magicshow.run400.b.txt T80) only
       * because the game's own ALRs rewrite "The gates is Down." -- an "are"
       * never matches them.  Only 4.0 gets here: the 3.7-3.9 schemas have no
       * states (ZCurrentState), and run390's examine tail (44BE60-44BEEE) is
       * just the open/closed line and whatisinon().
       */
      pf_buffer_string (filter, " is ");

      /* Add object state string. */
      state = obj_state_name (game, object);
      if (state)
        {
          pf_buffer_string (filter, state);
          scr_free (state);
          pf_buffer_string (filter, ".");
        }
      else
        {
          scr_error ("lib_list_object_state: invalid object state\n");
          pf_buffer_string (filter, "[invalid state].");
        }
    }

  /* Return TRUE if a state was printed. */
  return is_statussed;
}


/*
 * lib_examine_referencedob_400()
 *
 * run400's examine resolver referencedob, Proc_19_88_457034, for a line
 * whose up-front noun score (Proc_21_58_463640, MemVar_4942F8) tied.  It
 * works from co() (Proc_21_39_46486C), whose first step picks ONE name word
 * per object: the Short if it is a whole word of the line (454CB0), else
 * the LAST alias that is (the alias loop 4642DE never breaks).  co() then
 * counts the objects that answer to exactly that word (4465C8) and are
 * present and seen (46435A-46437E).
 *
 *   pass A  456E2D  co(i, 3): marks every object whose word has ANY such
 *                   namesake -- the object itself need not be present.
 *                   None marked -> &HFF; one -> that object.
 *   pass B  456ED3  co(i, 0) over the marked: true when the word has
 *                   exactly one namesake (464853).  One true -> that
 *                   object.  More than one namesake takes the 454454/
 *                   "Which" arm at 464560, modelled for the question it
 *                   leaves by lib_co_400_raise_for_references(), not here.
 *   pass C  456F5D  over the marked, count the words of the Prefix found in
 *                   the line; a new best takes the object, an equal count
 *                   gives &HFE.  With no Prefix word anywhere the answer is
 *                   whatever pass B (or A) touched last.
 *
 * Returns the object, -1 for &HFF, -2 for &HFE, and -3 when pass B met a
 * crowded name and the answer is the unmodelled arm's.
 */
const scr_char *
lib_co_400_name_word (scr_gameref_t game, scr_int object, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name, *word;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && lib_input_contains_word_400 (input, name))
    return name;

  word = nullptr;
  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && lib_input_contains_word_400 (input, name))
        word = name;
    }
  return word;
}

scr_int
lib_co_400_present_namesakes (scr_gameref_t game, const scr_char *word)
{
  scr_int object, count;

  count = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_seen (game, object)
          && obj_indirectly_in_room (game, object, gs_playerroom (game))
          && lib_co_object_answers_to (game, object, word))
        count++;
    }
  return count;
}

scr_int
lib_examine_referencedob_ex_400 (scr_gameref_t game, const scr_char *input,
                                 scr_bool crowd_contest)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::vector<scr_int> marked;
  scr_int object, index_, result, hits, best;

  result = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *word = lib_co_400_name_word (game, object, input);

      if (word && lib_co_400_present_namesakes (game, word) > 0)
        {
          marked.push_back (object);
          result = object;
        }
    }
  if (marked.size () < 2)
    return result;

  hits = 0;
  for (index_ = 0; index_ < (scr_int) marked.size (); index_++)
    {
      const scr_char *word = lib_co_400_name_word (game, marked[index_], input);
      const scr_int count = lib_co_400_present_namesakes (game, word);

      if (count > 1 && !crowd_contest)
        return -3;
      if (count == 1
          || (count > 1
              && lib_co_400_prefix_contest (game, word, input,
                                            gs_playerroom (game))
                 == marked[index_]))
        {
          hits++;
          result = marked[index_];
        }
    }
  if (hits == 1)
    return result;

  best = 0;
  for (index_ = 0; index_ < (scr_int) marked.size (); index_++)
    {
      const scr_char *prefix;
      scr_char *word, *next;
      scr_int found;

      prefix = prop_get_indexed_string (bundle, "Objects", marked[index_],
                                        "Prefix");
      if (scr_strempty (prefix))
        prefix = "a";

      found = 0;
      scr_owned_string copy (scr_strdup (prefix));
      for (word = copy.get (); word; word = next)
        {
          next = strchr (word, ' ');
          if (next)
            *next++ = NUL;
          if (word[0] == NUL || !lib_input_contains_word_400 (input, word))
            continue;
          found++;
          if (found == best)
            result = -2;
          else if (found > best)
            {
              result = marked[index_];
              best = found;
            }
        }
    }
  return result;
}

scr_int
lib_examine_referencedob_400 (scr_gameref_t game, const scr_char *input)
{
  return lib_examine_referencedob_ex_400 (game, input, FALSE);
}

/*
 * lib_examine_tied_absent_400()
 *
 * Where referencedob's answer to a tied line is an object that is not
 * here, examines speaks for THAT object (471933): "<player> can't see
 * <the X> from here!" when it has been seen (471958), else "<player> can't
 * see that." (471995).  Neither sets the not-a-turn flag.  Measured on
 * warlord (runner_probes/warlord.run400.txt): in the Great Hall "tapestries"
 * (90, alias "tapestry") and "third tapestry" (91, alias "tapestry three")
 * both score 1 on `x tapestry three`; pass A marks 90, 91 and the unseen
 * tapestries of room 35 (286), whose word "tapestry" has the present 90 as
 * its namesake; pass B is true for all three; no "the" is typed, so pass C
 * leaves 286 and the Runner answers "You can't see that." (and the same for
 * `x tapestry six`).  Only the absent answer is taken over; the others
 * stay with the ordinary path.
 */
static scr_bool
lib_examine_tied_absent_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (!lib_is_version_400 (game) || !input
      || lib_verb_object_resolve_400_string (game, input, nullptr, TRUE) != -1)
    return FALSE;

  object = lib_examine_referencedob_400 (game, input);
  if (object < 0
      || obj_indirectly_in_room (game, object, gs_playerroom (game)))
    return FALSE;

  if (gs_object_seen (game, object))
    {
      var_set_ref_object (gs_get_vars (game), object);
      lib_print_response_object (game,
                                 "You can't see ",
                                 "I can't see ",
                                 "%player% can't see ",
                                 object, " from here!\n");
    }
  else
    lib_print_response_message (game,
                                "You can't see that.\n",
                                "I can't see that.\n",
                                "%player% can't see that.\n");
  return TRUE;
}


/*
 * lib_examine_npc_overwrite_400()
 *
 * The present character an object examine line also names, or -1.  run400
 * calls characters() from the generaltasks tail (48B56E), after examines()
 * has already described the object, and its per-NPC examine arm (47FE19-
 * 480157: x/ex/examine/look/exam, no task ran, NPC in the room) assigns the
 * description to the message buffer without testing it -- so the object's
 * answer is thrown away.  The NPC gate is 45E99C mode 1, Name or any alias as
 * a word and no present namesake character; each passing NPC overwrites the
 * last, so the highest index wins.  Examining the object still marks it seen.
 * Unlike a plain character examine the line stays a turn: the arm writes no
 * MemVar_494281, and marking it administrative moves Lair's ambient room text
 * from T180 on out of step with runner_probes/lair.run400.a.txt.
 *
 * Measured on Lair of the Vampire (4.00): in the Ancient Feasthall, static
 * object "skeleton" and Havelock's skeleton (alias skeleton) share the room,
 * and `x skeleton` answers only "Havelock's skeletal remains sit on the
 * throne..." (runner_probes/lair.run400.b.txt:1573,
 * runner_probes/lair.run400.c.txt:1574).
 *
 * run390 has the same arm (characters() 459E2A-459FD9, called at 460675
 * below therest), gated only on c(Name) or c(Alias) at 4592B8, no task ran
 * (468198) and the NPC in the player's room, and it too assigns the
 * description untested.  Measured on Lair of the CyberCow (3.90,
 * runner_transcripts/cybercow_win.txt T72): object 4 "the fairy" and
 * Vluurinik (alias fairy) share the meadow, and `x fairy` answers only "The
 * fairy darts in circles around the warm bowl of CyberCow milk.", the
 * character's AltText.
 *
 * run380's arm (440D0B-440D88) and run370's (438E23) are the same again:
 * c("x") Or c("ex") Or c("examine") Or c("look at") Or c("exam") [And no
 * task ran, 3.8 only, by VB precedence], the NPC named by Name or first
 * Alias (lib_npc_named_in_line()) and in the room, the description or
 * "There's nothing special about <Name>." assigned outright, then charinv.
 * Measured on p37/p38NPCAMB (make_3738_npcambprobe.py): `x dave with stone`
 * answers "A quiet man." (run370x runner_probes/npcamb.run370.with.rtf,
 * run380x runner_probes/npcamb.run380.with.rtf), not the stone's description.
 */
scr_int
lib_examine_npc_overwrite_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int npc, found = -1;

  if (!input
      || (lib_is_version_400 (game)
          && lib_npc_400_find_namesakes (game, nullptr, nullptr)))
    return -1;

  /*
   * Deliberate deviation (2026-09-30): a character's name inside a longer
   * object name on the line does not count (lib_mask_long_names()).  The
   * Runners' c() finds "dog" in `x dog collar`, so the collar's examine
   * printed the dog, and `take dog collar` took the collar while saying "I
   * don't think dog would appreciate being handled." (bec2def78, 0b6154aa4,
   * 9b5cdae73).  A character whose name IS an object's (Lair's skeleton)
   * still overwrites.
   */
  const std::string masked = lib_mask_long_names (game, input, TRUE);

  GS_FOR_EACH_NPC (game, npc)
    {
      if (npc_in_room (game, npc, gs_playerroom (game))
          && lib_npc_referenced (game, npc, masked.c_str ()))
        found = npc;
    }
  if (SCR_TRACING_DEVIATIONS ())
    {
      scr_int runner = -1;

      GS_FOR_EACH_NPC (game, npc)
        if (npc_in_room (game, npc, gs_playerroom (game))
            && lib_npc_referenced (game, npc, input))
          runner = npc;
      if (runner != found)
        SCR_DEVIATION ("mask_npc_overwrite", "runner=%ld ours=%ld", runner,
                       found);
    }
  return found;
}

/*
 * lib_examine_tail()
 *
 * What follows an object's description in an examine: its openness state,
 * any listed state, and what is on and in it.  Returns the updated
 * is_described.  Pre-4.0 `read` is answered inside examines() and falls
 * into this same tail; see lib_read_object().
 */
scr_bool
lib_examine_tail (scr_gameref_t game, scr_int object, scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int openness;
  scr_bool is_statussed, is_mentioned;

  vt_key[0].string = "Objects";

  /* If the object is openable, print its openness state. */
  openness = gs_object_openness (game, object);
  switch (openness)
    {
    case OBJ_OPEN:
    case OBJ_CLOSED:
    case OBJ_LOCKED:
      {
        /*
         * Openness state, indexed by openness from OBJ_OPEN.  Always " is ":
         * no Runner inflects this one.  run370, run380, run390 and run400
         * each carry only " is open." / " is closed." / " is locked."
         * (run400 examine at 47395C and 4717F5/47183E/471887), with no
         * " are " variant anywhere in their string pools, and man_overboard
         * (4.00) measures `x drawers` as "The set of drawers is closed."
         * where we used to select "are" from the "some"-ish prefix.
         */
        static const scr_char *const states[] = {
          " is open.", " is closed.", " is locked."
        };

        /*
         * How the object is named here splits at 4.0.  run370, run380 and
         * run390 all build the line as `"  The " & Name & " is open."` from
         * the bare Short name -- run370 loc_435629/loc_435659, run380
         * loc_43CF4A/loc_43CF7A, run390 loc_44BE84/loc_44BEB4 -- so a
         * multi-word prefix vanishes: gamma.taf (3.90, Prefix "a mini",
         * Short "fridge") measures `x mini fridge` as "The fridge is
         * open." (runner_transcripts/gamma.txt).  run400 instead composes the
         * name with the tensed prefix (Proc_21_31_448710 at 4717D1): man
         * overboard.taf (4.00, Prefix "the set of", Short "drawers")
         * measures `x drawers` as "The set of drawers is closed."
         */
        lib_new_clause (game, is_described);
        if (prop_get_taf_version (bundle) < TAF_VERSION_400)
          {
            pf_buffer_string (filter, "the ");
            pf_buffer_string (filter,
                              prop_get_indexed_string (bundle, "Objects",
                                                       object, "Short"));
          }
        else
          lib_print_object_np (game, object);
        pf_buffer_string (filter, states[openness - OBJ_OPEN]);
        is_described = TRUE;
      }
      break;

    default:
      break;
    }

  /* Add any extra details for stateful objects. */
  vt_key[1].integer = object;
  vt_key[2].string = "CurrentState";
  is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
  if (is_statussed)
    {
      vt_key[2].string = "StateListed";
      is_mentioned = prop_get_boolean (bundle, "B<-sis", vt_key);
      if (is_mentioned)
        is_described |= lib_list_object_state (game, object, is_described);
    }

  /* List out what's on and what's inside the object. */
  is_described |= lib_list_in_on_object (game, object, is_described);

  return is_described;
}

/*
 * lib_examine_crowded_390()
 *
 * 3.9's examines() never asks the parser which object the line meant: it
 * calls referencedob() (42DEF8) and takes what that returns.  Pass one
 * counts the objects co(obj, 0) accepts -- Short or Alias in the line,
 * then the present-and-seen namesake count, then the last word of the
 * object's own Prefix (lib_co_pre400).  None is -1 ("Nothing special.");
 * one is the answer.  MORE than one runs a second pass (42DF60-42DFD4)
 * that is nothing like the first: it walks EVERY object in the game,
 * present or not, seen or not, and keeps the ones whose Short or FIRST
 * Alias is exactly lastword(line) -- the last space-delimited word typed.
 * Exactly one survivor is the answer; anything else returns &HFE = -2 and
 * examines() prints "Please examine one object at a time." (44BFA9), the
 * line a bare `x all` gets.
 *
 * MEASURED p39PFX2 (run390x, runner_probes/pfx2.run390.txt, 2026-09-20): three
 * trees, all Short "tree", Prefixed "a big red", "a red" and "the red".  `x
 * red tree`, `x a red tree`, `x the red tree` and `x big red tree` all type
 * the last Prefix word "red" that all three share, so pass one keeps
 * three and the second finds three Shorts "tree" -- "Please examine one
 * object at a time." every time, the multi-word "big red tree" no help at
 * all (the Runner has no such form; only 4.0 scores whole Prefixes, and
 * p4PFX2 answers "A big red tree." there).  `x tree`, `x big tree` and `x
 * a tree` type no last-Prefix word, so pass one keeps none and the turn is
 * the ordinary "Which tree.  The big red tree, the red tree or the red
 * tree?"  p39PFX turn 17 `x tree red` is the one-survivor case: "A red
 * tree.".
 *
 * Returns the object, -2 for "one object at a time", or -3 when pass one
 * kept fewer than two and the ordinary path should run.
 */

scr_int
lib_examine_crowded_390 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *tail;
  scr_int index_, kept, matches, match;

  if (!lib_is_version_390 (game) || !line)
    return -3;

  kept = 0;
  GS_FOR_EACH_OBJECT (game, index_)
    if (lib_co_pre400 (game, line, index_, 0))
      kept++;
  if (kept < 2)
    return -3;

  tail = lib_co_lastword (line);
  matches = 0;
  match = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      scr_vartype_t vt_key[4];
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "Objects", index_, "Short");
      if (!(name && scr_strcasecmp (name, tail) == 0))
        {
          name = lib_first_alias (bundle, vt_key, "Objects", index_);
          if (!(name && name[0] != NUL && scr_strcasecmp (name, tail) == 0))
            continue;
        }
      matches++;
      match = index_;
    }

  return matches == 1 ? match : -2;
}

/*
 * lib_cmd_examine_object()
 *
 * Show the long description of the most recently referenced object.
 */
scr_bool
lib_cmd_examine_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object, task, npc;
  scr_bool is_described, is_ambiguous, should_be;
  const scr_char *description, *resource;

  /* A 4.0 tie that referencedob settles on an absent object. */
  if (lib_examine_tied_absent_400 (game))
    return TRUE;

  /* 3.9's referencedob() settles a crowd of co()-true objects itself. */
  object = lib_examine_crowded_390 (game);
  if (object == -2)
    return lib_print_message (game,
                              "Please examine one object at a time.\n");
  if (object >= 0)
    var_set_ref_object (gs_get_vars (game), object);
  else
    {
      /* Get the referenced object, and if none, consider complete. */
      object = lib_disambiguate_object (game, "examine", &is_ambiguous);
      if (object == -1)
        return is_ambiguous;
    }

  /*
   * Examining an object marks it seen.  This can matter in version 3.8
   * games, where the matcher doesn't require objects to have been seen.
   */
  gs_set_object_seen (game, object, TRUE);

  /* 4.0 names a present object to the antecedent setter (471749-471789). */
  if (lib_is_version_400 (game)
      && (obj_indirectly_in_room (game, object, gs_playerroom (game))
          || obj_indirectly_held_by_player (game, object)))
    uip_note_antecedent_400 (object, UIP_IT_INDEFINITE, UIP_STAGE_HANDLER);

  /* A present character the line names overwrites the answer. */
  npc = lib_examine_npc_overwrite_400 (game);
  if (npc != -1)
    {
      scr_bool was_admin = game->is_admin;

      gs_set_npc_seen (game, npc, TRUE);
      lib_describe_npc (game, npc);
      game->is_admin = was_admin;
      return TRUE;
    }

  /* Begin assuming no description printed. */
  is_described = FALSE;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;

  /*
   * The darkness fork's other half.  Pre-4.0 consults its darkness byte the
   * moment the object has been matched and before a single word of
   * description is composed: run390 44BC37 substitutes "<player> can't see "
   * & <the object> & " very clearly." for the message and jumps (44BC7E) to
   * 44BE60 -- which is *not* the end of the answer but the openness state
   * lines, so what the jump skips is only the description, its alternate and
   * the associated resource.  run380 43CD95 and run370 435481 do the same at
   * the same point, and the same byte is read before examines() has even
   * looked at whether the verb was `read` (44BC81), so a readable object in
   * the dark answers this too; see lib_cmd_read_object().
   *
   * It applies to a carried object as much as a scenery one -- the byte is
   * computed from the room alone, and the Runner's object loop never
   * separates the two.
   *
   * Measured on p39DARK.taf (3.90), runner_probes/dark.run390.seen.txt: with
   * the box already seen, `x box` in the dark answers "You can't see the box
   * very clearly.  The box is open.  A coin is inside the box.", and `x lamp`
   * -- held -- answers "You can't see the lamp very clearly."
   * runner_probes/dark.run390.unseen.txt is the companion run where the same
   * objects have never been seen, and there the noun does not resolve at all;
   * see lib_print_room_description().
   */
  if (lib_room_is_dark (game, gs_playerroom (game)))
    {
      lib_print_response_object (game,
                                 "You can't see ",
                                 "I can't see ",
                                 "%player% can't see ",
                                 object, " very clearly.");
      is_described = TRUE;
    }
  else
    {
      /*
       * Get selection task and expected state; for the expected task state,
       * FALSE indicates task completed, TRUE not completed.
       */
      vt_key[2].string = "Task";
      task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
      vt_key[2].string = "TaskNotDone";
      should_be = !prop_get_boolean (bundle, "B<-sis", vt_key);

      /* Select either the main or the alternate description. */
      if (gs_task_valid (game, task) && gs_task_done (game, task) == should_be)
        {
          vt_key[2].string = "AltDesc";
          resource = "Res2";
        }
      else
        {
          vt_key[2].string = "Description";
          resource = "Res1";
        }

      /* Print the description, or a default response. */
      description = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (description))
        {
          pf_buffer_string (filter, description);
          is_described = TRUE;
        }

      /* Handle any associated resource. */
      vt_key[2].string = resource;
      res_handle_resource (game, "sis", vt_key);
    }

  is_described = lib_examine_tail (game, object, is_described);

  /*
   * If nothing yet said, print a default response.
   *
   * This is the other half of the 4.0 rewrite of the Runner's examines().  In
   * 3.7, 3.8 and 3.9 an object whose Description is empty leaves the Runner's
   * message string empty, and the empty string falls all the way through to
   * the same tail that answers an unmatched noun: a flat "Nothing special."
   * (run370 435BF4, run380 43D545, run390 44C3DC, all reached from `If msg =
   * vbNullString`).  4.0 fills the message in here instead, one branch before
   * that tail can see it, with "<player> see nothing special about <obj>."
   * (run400 471A08/471A1C).  Beware the near-miss: "There's nothing special
   * about <obj>." (run380 440D4C, run390 459EC1, run400 480041) is the
   * CHARACTER default, not this one; Scarier already prints it, at
   * lib_cmd_examine_npc.
   *
   * Measured on p39EXAM.taf (3.90), runner_probes/exam.run390.txt: `x stone`
   * -- the stone is in the room, has an empty Description, and answers
   * "Nothing special."  runner_probes/exam.run390.held.txt repeats it with the
   * stone held, and answers the same, so being carried makes no difference.
   * The 4.0 twin p4EXAM.taf, runner_probes/exam.run400.txt, answers "You see
   * nothing special about the stone."  ms_mobius_solution is the corpus's only
   * pre-4.0 exposure.
   */
  if (!is_described)
    {
      if (!lib_is_version_400 (game))
        pf_buffer_string (filter, "Nothing special.");
      else
        {
          pf_buffer_string (filter,
                        lib_select_response (game,
                                       "You see nothing special about ",
                                       "I see nothing special about ",
                                       "%player% see nothing special about "));
          lib_print_object_np (game, object);
          pf_buffer_character (filter, '.');
        }
    }

  pf_buffer_answer_break (filter);
  return TRUE;
}

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
 * Standing, sitting and lying, getting off, and the pre-4.0
 * anywhere-in-the-line open/close and where-is scans.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/* Enumerated sit/stand/lie types. */
enum
{ OBJ_STANDABLE_MASK = 1 << 0,
  OBJ_LIEABLE_MASK = 1 << 1
};
enum
{ MOVE_SIT, MOVE_SIT_FLOOR,
  MOVE_STAND, MOVE_STAND_FLOOR, MOVE_LIE, MOVE_LIE_FLOOR,
  MOVE_GET_ON
};

/*
 * lib_stand_sit_lie_floor_pre390()
 *
 * The bare sit/stand/lie arms of the pre-3.9 sitstand (run380 4340C5,
 * 4342C7, 434509; run370 42AF3C, 42B0E4, 42B2C9).  Sitting or lying down
 * on the floor keeps the parent object: only standing up clears it, so
 * `stand on stool`, `lie`, `sit`, `stand` is "You lie down on the ground.",
 * "You sit up.", "You stand up from the stool.".  Standing on an object is
 * position 0, so a bare `stand` there is "You are already standing!".
 * p37SIT/p38SIT (make_3738_sitprobe.py), run370x
 * runner_probes/sit.run370.sit2.rtf, run380x
 * runner_probes/sit.run380.sit2.rtf, 2026-09-19.
 */
static scr_bool
lib_stand_sit_lie_floor_pre390 (scr_gameref_t game, scr_int movement)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int position = gs_playerposition (game);

  switch (movement)
    {
    case MOVE_STAND_FLOOR:
      if (position == 0)
        return lib_print_response_message (game,
                                          "You are already standing!\n",
                                          "I am already standing!\n",
                                          "%player% is already standing!\n");
      pf_buffer_string (filter,
                        lib_select_response (game, "You stand up",
                                             "I stand up",
                                             "%player% stand up"));
      if (gs_playerparent (game) != -1)
        {
          pf_buffer_string (filter, " from ");
          lib_print_object_np (game, gs_playerparent (game));
        }
      pf_buffer_string (filter, ".\n");
      gs_set_playerposition (game, 0);
      gs_set_playerparent (game, -1);
      return TRUE;

    case MOVE_SIT_FLOOR:
      if (position == 1)
        return lib_print_response_message (game,
                                     "You are already sitting down.\n",
                                     "I am already sitting down.\n",
                                     "%player% is already sitting down.\n");
      if (position == 2)
        lib_print_response_message (game, "You sit up.\n", "I sit up.\n",
                                    "%player% sit up.\n");
      else
        lib_print_response_message (game,
                                    "You sit down on the ground.\n",
                                    "I sit down on the ground.\n",
                                    "%player% sit down on the ground.\n");
      gs_set_playerposition (game, 1);
      return TRUE;

    case MOVE_LIE_FLOOR:
      if (position == 2)
        return lib_print_response_message (game,
                                       "You are already lying down.\n",
                                       "I am already lying down.\n",
                                       "%player% is already lying down.\n");
      lib_print_response_message (game, "You lie down on the ground.\n",
                                  "I lie down on the ground.\n",
                                  "%player% lie down on the ground.\n");
      gs_set_playerposition (game, 2);
      return TRUE;

    default:
      return FALSE;
    }
}


/*
 * lib_stand_sit_lie_floor_390()
 *
 * The bare sit/stand/lie arms of the 3.9/4.0 sitstand, which is one proc in
 * both (run390 444010-444A04, run400 46B370-46BCF8).  Unlike pre-3.9 these
 * name the parent object:
 *   - `sit` standing on O is "sit down on the O" and keeps O, whatever its
 *     SitLie; lying on O is "sit up on the O" when O is sittable, else "sit
 *     up on the ground." -- still keeping O.
 *   - `lie` standing or sitting on O is "lie down on the O" when O is
 *     lieable (SitLie > 1), else "lie down on the ground." and O is dropped.
 *   - `stand` at position 0 is "already standing!" even on an object.
 * `sit on the ground/floor` is its own arm: "sit down on the ground." from
 * any place but the floor, where it is "are already sitting on the floor!"
 * (or "ground!") with a literal "are".  p39SIT/p4SIT, run390x
 * runner_probes/sit.run390.sit2.txt, run400x
 * runner_probes/sit.run400.sit2.txt, 2026-09-19.
 */
static scr_bool
lib_stand_sit_lie_floor_390 (scr_gameref_t game, scr_int movement)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *line = run_get_dispatch_input ();
  const scr_int position = gs_playerposition (game);
  const scr_int parent = gs_playerparent (game);
  scr_int sit_lie;
  scr_vartype_t vt_key[3];

  sit_lie = 0;
  if (parent != -1)
    {
      vt_key[0].string = "Objects";
      vt_key[1].integer = parent;
      vt_key[2].string = "SitLie";
      sit_lie = prop_get_integer (bundle, "I<-sis", vt_key);
    }

  switch (movement)
    {
    case MOVE_STAND_FLOOR:
      /* Same as pre-3.9 (run400 46B9D7-46BAB1, run390 4446E3-4447B9). */
      return lib_stand_sit_lie_floor_pre390 (game, movement);

    case MOVE_SIT_FLOOR:
      if (lib_co_contains (line, "on") || lib_co_contains (line, "in"))
        {
          /* run400 46B3BC-46B43E, run390 444086-44410F.  The Runners put
             the pronoun before a fixed " are", giving "I are" and
             "%player% are"; Scarier deliberately agrees the copula
             (deviation policy). */
          if (position == 1 && parent == -1)
            {
              pf_buffer_string (filter,
                                lib_select_response (game, "You are", "I am",
                                                     "%player% is"));
              pf_buffer_string (filter,
                                lib_co_contains (line, "floor")
                                ? " already sitting on the floor!\n"
                                : " already sitting on the ground!\n");
              return TRUE;
            }
          lib_print_response_message (game,
                                      "You sit down on the ground.\n",
                                      "I sit down on the ground.\n",
                                      "%player% sit down on the ground.\n");
          gs_set_playerposition (game, 1);
          gs_set_playerparent (game, -1);
          return TRUE;
        }
      if (position == 1)
        return lib_print_response_message (game,
                                     "You are already sitting down.\n",
                                     "I am already sitting down.\n",
                                     "%player% is already sitting down.\n");
      if (position == 2)
        {
          if (parent != -1 && (sit_lie == 1 || sit_lie == 3))
            lib_print_response_object (game, "You sit up on ", "I sit up on ",
                                       "%player% sit up on ", parent, ".\n");
          else
            lib_print_response_message (game, "You sit up on the ground.\n",
                                        "I sit up on the ground.\n",
                                        "%player% sit up on the ground.\n");
        }
      else if (parent != -1)
        lib_print_response_object (game, "You sit down on ", "I sit down on ",
                                   "%player% sit down on ", parent, ".\n");
      else
        lib_print_response_message (game, "You sit down on the ground.\n",
                                    "I sit down on the ground.\n",
                                    "%player% sit down on the ground.\n");
      gs_set_playerposition (game, 1);
      return TRUE;

    case MOVE_LIE_FLOOR:
      /* run400 46BBFC-46BCCC, run390 44491F-4449F7. */
      if (position == 2)
        return lib_print_response_message (game,
                                       "You are already lying down.\n",
                                       "I am already lying down.\n",
                                       "%player% is already lying down.\n");
      if (parent != -1 && sit_lie > 1)
        lib_print_response_object (game, "You lie down on ", "I lie down on ",
                                   "%player% lie down on ", parent, ".\n");
      else
        {
          lib_print_response_message (game, "You lie down on the ground.\n",
                                      "I lie down on the ground.\n",
                                      "%player% lie down on the ground.\n");
          gs_set_playerparent (game, -1);
        }
      gs_set_playerposition (game, 2);
      return TRUE;

    default:
      return FALSE;
    }
}


/*
 * lib_sit_lie_scan_370()
 *
 * run370's sitstand object loop (42AE7D, stand 42B025, lie 42B20A) has no
 * co() and no location test: it takes every object whose Short or Alias is
 * a whole word of the line and whose SitLie fits, held, contained or in
 * another room alike, and the last match wins.  p37SIT `sit on bed` from the
 * other room is "You sit down on a bed.", and a held stool is sat on (run370x
 * runner_probes/sit.run370.rtf, 2026-09-19).  Returns -1 for no match; the
 * line then falls to therest, whose answers Scarier already gives
 * ("You can't see the crate." for `lie on crate`, sit-only, elsewhere).
 */
static scr_int
lib_sit_lie_scan_370 (scr_gameref_t game, scr_int movement_mask)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *line = run_get_dispatch_input ();
  scr_vartype_t vt_key[4];
  scr_int object, match;

  match = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *alias;
      scr_bool named;

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Short";
      named = lib_co_contains (line, prop_get_string (bundle, "S<-sis",
                                                      vt_key));
      alias = lib_first_alias (bundle, vt_key, "Objects", object);
      if (!named && !(alias && lib_co_contains (line, alias)))
        continue;

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "SitLie";
      if (prop_get_integer (bundle, "I<-sis", vt_key) & movement_mask)
        match = object;
    }
  return match;
}


/*
 * lib_stand_sit_lie()
 *
 * Central handler for stand, sit, and lie commands.
 */
static scr_bool
lib_stand_sit_lie (scr_gameref_t game, scr_int movement)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, position;
  const scr_char *success_message;
  scr_bool is_pre_390;

  is_pre_390 = prop_get_taf_version (bundle) < TAF_VERSION_390;
  if (is_pre_390 ? lib_stand_sit_lie_floor_pre390 (game, movement)
                 : lib_stand_sit_lie_floor_390 (game, movement))
    return TRUE;

  /* Get a target object for movement, -1 if floor. */
  switch (movement)
    {
    case MOVE_STAND:
    case MOVE_GET_ON:
    case MOVE_SIT:
    case MOVE_LIE:
      {
        const scr_char *disambiguate, *cant_do_that;
        scr_int sit_lie_flags, movement_mask;
        scr_vartype_t vt_key[3];
        scr_bool is_ambiguous;

        /* Initialize variables to avoid gcc warnings. */
        disambiguate = nullptr;
        cant_do_that = nullptr;

        /* Set disambiguation and not amenable messages. */
        switch (movement)
          {
          case MOVE_STAND:
            disambiguate = "stand on";
            cant_do_that = lib_select_response (game,
                                                "You can't stand on ",
                                                "I can't stand on ",
                                                "%player% can't stand on ");
            movement_mask = OBJ_STANDABLE_MASK;
            break;
          case MOVE_GET_ON:
            /*
             * Same branch, no refusal of its own -- see lib_cmd_get_on_object.
             */
            disambiguate = "stand on";
            cant_do_that = nullptr;
            movement_mask = OBJ_STANDABLE_MASK;
            break;
          case MOVE_SIT:
            disambiguate = "sit on";
            cant_do_that = lib_select_response (game,
                                                "You can't sit on ",
                                                "I can't sit on ",
                                                "%player% can't sit on ");
            movement_mask = OBJ_STANDABLE_MASK;
            break;
          case MOVE_LIE:
            disambiguate = "lie on";
            cant_do_that = lib_select_response (game,
                                                "You can't lie on ",
                                                "I can't lie on ",
                                                "%player% can't lie on ");
            movement_mask = OBJ_LIEABLE_MASK;
            break;
          default:
            scr_fatal ("lib_sit_stand_lie: movement error, %ld\n", movement);
          }

        if (prop_get_taf_version (bundle) < TAF_VERSION_380)
          {
            object = lib_sit_lie_scan_370 (game, movement_mask);
            if (object != -1)
              break;
          }

        /* Get the referenced object; if none, consider complete. */
        object = lib_disambiguate_object (game, disambiguate, &is_ambiguous);
        if (object == -1)
          return is_ambiguous;

        /*
         * Verify the referenced object is amenable, and on the floor of the
         * player's room.  The 3.8+ sitstand loops take an object only
         * when it is a dynamic object whose room is the player's, or a static
         * one listed in that room, and only then read SitLie (run400
         * 46B8F2-46B93A, run390 4445F8-44463A, run380 434042); run370's loop
         * has no location test at all (lib_sit_lie_scan_370).  A held stool is never
         * stood on, and the line falls to the "can't stand on" refusal.
         * House.taf's `stand on stool` with the stool in hand, whose ALR
         * turns that refusal into "While you're still holding it?"
         * (runner_probes/house.run400.turnbisect.txt).
         */
        vt_key[0].string = "Objects";
        vt_key[1].integer = object;
        vt_key[2].string = "SitLie";
        sit_lie_flags = prop_get_integer (bundle, "I<-sis", vt_key);
        if (!(sit_lie_flags & movement_mask)
            || !obj_directly_in_room (game, object, gs_playerroom (game)))
          {
            if (!cant_do_that)
              return FALSE;
            pf_buffer_string (filter, cant_do_that);
            lib_print_object_np (game, object);
            pf_buffer_string (filter, ".\n");
            return TRUE;
          }
        break;
      }

    default:
      scr_fatal ("lib_sit_stand_lie: movement error, %ld\n", movement);
    }

  /*
   * No sitstand object arm asks whether the player is already there: `sit on
   * stool` twice is "You sit down on the stool." twice, and sitting on an
   * object is "sit down on" even from lying (run400 46B4D4/46B958/46BB7D,
   * run390 4441A7/444661/444890, run380 43408C/43428E/4344D0, run370
   * 42AF05/42B0AD/42B292).  Before 3.9 the object is named by its authored
   * Prefix, not "the": "You sit down on a stool.".  p37SIT..p4SIT
   * (make_3738_sitprobe.py), run370x runner_probes/sit.run370.sit2.rtf,
   * run380x runner_probes/sit.run380.sit2.rtf, run390x
   * runner_probes/sit.run390.sit2.txt, run400x
   * runner_probes/sit.run400.sit2.txt, 2026-09-19.
   */
  switch (movement)
    {
    case MOVE_STAND:
    case MOVE_GET_ON:
      success_message = lib_select_response (game, "You stand on ",
                                             "I stand on ",
                                             "%player% stand on ");
      position = 0;
      break;
    case MOVE_SIT:
      success_message = lib_select_response (game, "You sit down on ",
                                             "I sit down on ",
                                             "%player% sit down on ");
      position = 1;
      break;
    case MOVE_LIE:
      success_message = lib_select_response (game, "You lie down on ",
                                             "I lie down on ",
                                             "%player% lie down on ");
      position = 2;
      break;
    default:
      scr_fatal ("lib_sit_stand_lie: movement error, %ld\n", movement);
    }

  pf_buffer_string (filter, success_message);
  if (is_pre_390)
    lib_print_object_raw (game, object);
  else
    lib_print_object_np (game, object);
  pf_buffer_string (filter, ".\n");
  gs_set_playerposition (game, position);
  gs_set_playerparent (game, object);
  return TRUE;
}


/*
 * lib_cmd_stand_*
 * lib_cmd_sit_*
 * lib_cmd_lie_*
 *
 * Stand, sit, or lie on an object, or on the floor.
 */
scr_bool
lib_cmd_stand_on_object (scr_gameref_t game)
{
  return lib_stand_sit_lie (game, MOVE_STAND);
}


/*
 * lib_cmd_get_on_object()
 *
 * `get on X` is the stand-on branch of the 3.9/4.0 sitstand proc, entered by
 * `c("stand") Or c("get up") Or c("get on")` and then `c("on") Or c("in")`
 * (run400 loc_46B889, run390 loc_444565).  3.9 is also where takes() gained
 * its matching `Not c("get on")` exclusion (run390 loc_4544C6, run400
 * loc_47B68A), without which the take handler would eat the command first;
 * neither literal exists anywhere in run370/run380, so this is 3.9+ only.
 *
 * It differs from `stand on X` in what happens when the object is not a
 * standable one.  The refusal "You can't stand on X." is not produced by
 * the sitstand proc at all -- it comes from a later generaltasks fallback
 * keyed on the literal phrase "stand on" (run400 loc_489DDF), which `get on
 * X` does not contain.  So a non-standable object leaves the whole turn
 * unanswered and drops through to the generic unknown-verb reply: run390 on
 * Microwave Man answers `get on glass` with "I don't understand what you
 * want me to do with the shard of glass.", where `stand on glass` refuses.
 * Declining here reproduces that fall-through.
 *
 * Deliberate deviation: 3.7/3.8 take it too; see lib_cmd_get_off().
 */
scr_bool
lib_cmd_get_on_object (scr_gameref_t game)
{
  return lib_stand_sit_lie (game, MOVE_GET_ON);
}

/*
 * lib_floor_named()
 *
 * TRUE when a floor row's line says `on`/`in` the ground or floor.  Only the
 * 3.9+ sit block has an arm for that (run400 46B39E, run390 444077); lie
 * and stand, and every block before 3.9 (whose c("on") takes the object
 * loop), find no object and write nothing, so the line falls to therest's
 * "You can't sit on that." (p37SIT..p4SIT: run370x
 * runner_probes/sit.run370.sit3.rtf, run380x
 * runner_probes/sit.run380.sit3.rtf, run390x
 * runner_probes/sit.run390.sit3.txt, run400x
 * runner_probes/sit.run400.sit3.txt, 2026-09-19).
 */
/*
 * Deliberate deviation (2026-09-30): the floor rows answer every line they
 * match, so `lie on floor`, `stand on the ground` and a pre-3.9 `sit on the
 * floor` lie, stand or sit as they did before a1183ae58, instead of "You
 * can't lie on that.".  The Runner rule is kept here for reference.
 */
static scr_bool
lib_floor_named (void)
{
  const scr_char *line = run_get_dispatch_input ();

  if (line && (lib_co_contains (line, "on") || lib_co_contains (line, "in")))
    SCR_DEVIATION ("floor_named", nullptr);
  return FALSE
         && line && (lib_co_contains (line, "on")
                     || lib_co_contains (line, "in"));
}

scr_bool
lib_cmd_stand_on_floor (scr_gameref_t game)
{
  if (lib_floor_named ())
    return FALSE;
  return lib_stand_sit_lie (game, MOVE_STAND_FLOOR);
}

scr_bool
lib_cmd_sit_on_object (scr_gameref_t game)
{
  return lib_stand_sit_lie (game, MOVE_SIT);
}

scr_bool
lib_cmd_sit_on_floor (scr_gameref_t game)
{
  if (lib_floor_named ()
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return lib_stand_sit_lie (game, MOVE_SIT_FLOOR);
}

scr_bool
lib_cmd_lie_on_object (scr_gameref_t game)
{
  return lib_stand_sit_lie (game, MOVE_LIE);
}

scr_bool
lib_cmd_lie_on_floor (scr_gameref_t game)
{
  if (lib_floor_named ())
    return FALSE;
  return lib_stand_sit_lie (game, MOVE_LIE_FLOOR);
}


/*
 * lib_cmd_sit_scan_370()
 * lib_cmd_stand_scan_370()
 * lib_cmd_lie_scan_370()
 *
 * run370's object loops (lib_sit_lie_scan_370) look at no scope, so an
 * object in another room is sat on even when no %object% row binds it:
 * p37SIT `sit on bed` from the Lit Room is "You sit down on a bed." (run370x
 * runner_probes/sit.run370.rtf, 2026-09-19).  3.7 only; declines when nothing
 * matches, leaving the line to the therest refusals below.
 */
static scr_bool
lib_sit_stand_lie_scan_370 (scr_gameref_t game, scr_int movement)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
      || lib_sit_lie_scan_370 (game, movement == MOVE_LIE
                                     ? OBJ_LIEABLE_MASK
                                     : OBJ_STANDABLE_MASK) == -1)
    return FALSE;
  return lib_stand_sit_lie (game, movement);
}

/*
 * lib_sitstand_claims_370()
 *
 * TRUE when run370's sitstand, which runs before therest, would take the
 * line: its therest "can't see" test (lib_therest_absent_370) must then
 * stand aside for the rows above.
 */
scr_bool
lib_sitstand_claims_370 (scr_gameref_t game)
{
  const scr_char *line = run_get_dispatch_input ();

  if (!line || !(lib_co_contains (line, "on") || lib_co_contains (line, "in")))
    return FALSE;
  if ((lib_co_contains (line, "sit") || lib_co_contains (line, "stand"))
      && lib_sit_lie_scan_370 (game, OBJ_STANDABLE_MASK) != -1)
    return TRUE;
  return lib_co_contains (line, "lie")
         && lib_sit_lie_scan_370 (game, OBJ_LIEABLE_MASK) != -1;
}

scr_bool
lib_cmd_sit_scan_370 (scr_gameref_t game)
{
  return lib_sit_stand_lie_scan_370 (game, MOVE_SIT);
}

scr_bool
lib_cmd_stand_scan_370 (scr_gameref_t game)
{
  return lib_sit_stand_lie_scan_370 (game, MOVE_STAND);
}

scr_bool
lib_cmd_lie_scan_370 (scr_gameref_t game)
{
  return lib_sit_stand_lie_scan_370 (game, MOVE_LIE);
}


/*
 * `lay` is a lie at every version.  Only run400's lie block tests c("lay")
 * (46BACE, beside c("lie") at 46BAC1); run370, run380 and run390 have no
 * such word, so `lay down` is "I don't understand." and `lay on stool` "I
 * don't understand what you want me to do with the stool." (p37SIT..p4SIT,
 * run370x runner_probes/sit.run370.sit4.rtf, run380x
 * runner_probes/sit.run380.sit4.rtf, run390x
 * runner_probes/sit.run390.sit4.txt, run400x
 * runner_probes/sit.run400.sit4.txt, 2026-09-19).  Deliberate deviation:
 * Scarier's [lie/lay] rows take it below 4.0 too.
 */


/*
 * lib_sitstand_block()
 *
 * One block of the Runner's sitstand, entered on its word anywhere in the
 * line: with c("on") Or c("in") the object loop, where every object that
 * passes writes and the last in index order wins, else the bare arm.  The
 * 3.8+ loop takes an object when co(obj) passes, it lies on the floor of
 * the player's room and its SitLie fits (run400 46B4D4/46B8F2/46BB1A, run390
 * 4441A7/4445F8/44481C, run380 434042); run370's has no scope at all
 * (lib_sit_lie_scan_370).  From 3.9 the sit block first takes its
 * ground/floor arm when the line names one, and the loop still runs after
 * it (run400 46B39E, run390 444077).  Each write replaces what the line has
 * said so far from mark: the Runner keeps one message and overwrites it.
 */
static void
lib_sitstand_block (scr_gameref_t game, scr_int movement, size_t mark)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int taf_version = prop_get_taf_version (bundle);
  const scr_char *line = run_get_dispatch_input ();
  const auto replace_from = [&] (size_t before)
    {
      if (pf_buffer_length (filter) > before && before > mark)
        {
          const std::string text = pf_cut_tail (filter, before);

          pf_truncate (filter, mark);
          pf_buffer_string (filter, text.c_str ());
        }
    };
  const size_t before = pf_buffer_length (filter);
  scr_int mask, object, match;

  if (!(lib_co_contains (line, "on") || lib_co_contains (line, "in")))
    {
      const scr_int floor_movement = movement == MOVE_SIT ? MOVE_SIT_FLOOR
                                     : movement == MOVE_LIE ? MOVE_LIE_FLOOR
                                     : MOVE_STAND_FLOOR;

      if (taf_version < TAF_VERSION_390)
        lib_stand_sit_lie_floor_pre390 (game, floor_movement);
      else
        lib_stand_sit_lie_floor_390 (game, floor_movement);
      replace_from (before);
      return;
    }

  if (movement == MOVE_SIT && taf_version >= TAF_VERSION_390
      && (lib_co_contains (line, "ground") || lib_co_contains (line, "floor")))
    {
      lib_stand_sit_lie_floor_390 (game, MOVE_SIT_FLOOR);
      replace_from (before);
    }

  mask = movement == MOVE_LIE ? OBJ_LIEABLE_MASK : OBJ_STANDABLE_MASK;
  match = -1;
  if (taf_version < TAF_VERSION_380)
    match = lib_sit_lie_scan_370 (game, mask);
  else
    GS_FOR_EACH_OBJECT (game, object)
      {
        if (lib_co_pre400 (game, line, object, 0)
            && obj_directly_in_room (game, object, gs_playerroom (game))
            && (prop_get_indexed_integer (bundle, "Objects", object, "SitLie")
                & mask))
          match = object;
      }
  if (match == -1)
    return;

  const size_t loop_before = pf_buffer_length (filter);
  pf_buffer_string (filter,
                    movement == MOVE_SIT
                    ? lib_select_response (game, "You sit down on ",
                                           "I sit down on ",
                                           "%player% sit down on ")
                    : movement == MOVE_LIE
                    ? lib_select_response (game, "You lie down on ",
                                           "I lie down on ",
                                           "%player% lie down on ")
                    : lib_select_response (game, "You stand on ",
                                           "I stand on ",
                                           "%player% stand on "));
  if (taf_version < TAF_VERSION_390)
    lib_print_object_raw (game, match);
  else
    lib_print_object_np (game, match);
  pf_buffer_string (filter, ".\n");
  gs_set_playerposition (game, movement == MOVE_SIT ? 1
                               : movement == MOVE_LIE ? 2 : 0);
  gs_set_playerparent (game, match);
  replace_from (loop_before);
}


/*
 * lib_sitstand_strip()
 *
 * The line with its sit/stand/lie words (4.0 also lay) and a "down"/"up"
 * right after each cut out, for the rows that must see the rest of it.
 */
static std::string
lib_sitstand_strip (const scr_char *line, scr_int taf_version)
{
  std::string out;
  const scr_char *p = line;

  while (*p)
    {
      const scr_char *end = p;
      size_t length;

      while (*end && *end != ' ')
        end++;
      length = end - p;
      const scr_bool is_move =
          (length == 3 && (scr_strncasecmp (p, "sit", 3) == 0
                           || scr_strncasecmp (p, "lie", 3) == 0
                           || (taf_version >= TAF_VERSION_400
                               && scr_strncasecmp (p, "lay", 3) == 0)))
          || (length == 5 && scr_strncasecmp (p, "stand", 5) == 0);
      if (is_move)
        {
          p = end + strspn (end, " ");
          if ((scr_strncasecmp (p, "down", 4) == 0
               && (p[4] == NUL || p[4] == ' '))
              || (scr_strncasecmp (p, "up", 2) == 0
                  && (p[2] == NUL || p[2] == ' ')))
            {
              p += p[0] == 'd' || p[0] == 'D' ? 4 : 2;
              p += strspn (p, " ");
            }
          continue;
        }
      if (!out.empty ())
        out += ' ';
      out.append (p, length);
      p = end + strspn (end, " ");
    }
  return out;
}


/*
 * lib_sitstand_speech_line()
 *
 * TRUE for a line led by a speech verb (ask, say, tell, talk, shout, yell),
 * which sitstand leaves alone as free text; see lib_sitstand_anywhere().
 */
static scr_bool
lib_sitstand_speech_line (const scr_char *line, scr_int taf_version)
{
  static const scr_char *const SPEECH[] = {
    "ask", "say", "tell", "talk", "shout", "yell", nullptr
  };
  const scr_char *const *word;

  for (word = SPEECH; *word; word++)
    {
      const size_t length = strlen (*word);

      if (scr_strncasecmp (line, *word, length) == 0
          && (line[length] == NUL || line[length] == ' '))
        {
          if (lib_co_contains (line, "sit") || lib_co_contains (line, "stand")
              || lib_co_contains (line, "lie")
              || (taf_version >= TAF_VERSION_400
                  && lib_co_contains (line, "lay")))
            SCR_DEVIATION ("sitstand_speech", nullptr);
          return TRUE;
        }
    }
  return FALSE;
}


/*
 * lib_sitstand_trace_mask()
 *
 * Deviation trace for a sit/stand/lie word that lib_mask_long_names() hid
 * inside an object or character name.
 */
static void
lib_sitstand_trace_mask (const scr_char *line, const std::string &masked,
                         scr_int taf_version)
{
  if (SCR_TRACING_DEVIATIONS () && masked != line
      && ((lib_co_contains (line, "sit") != lib_co_contains (masked.c_str (), "sit"))
          || (lib_co_contains (line, "stand")
              != lib_co_contains (masked.c_str (), "stand"))
          || (lib_co_contains (line, "lie")
              != lib_co_contains (masked.c_str (), "lie"))
          || (taf_version >= TAF_VERSION_400
              && lib_co_contains (line, "lay")
                 != lib_co_contains (masked.c_str (), "lay"))))
    SCR_DEVIATION ("mask_sitstand", "masked=\"%s\"", masked.c_str ());
}


/*
 * lib_sitstand_plain_move()
 *
 * TRUE for a line that is only a move -- sit, stand, lie or lay leading,
 * an optional down/up, then nothing or an on/in phrase -- which the
 * ordinary sit/stand/lie rows answer instead.
 */
static scr_bool
lib_sitstand_plain_move (const scr_char *line)
{
  static const scr_char *const VERBS[] = {
    "sit", "stand", "lie", "lay", nullptr
  };
  const scr_char *const *word;
  const scr_char *rest_ = nullptr;

  for (word = VERBS; *word && !rest_; word++)
    {
      const size_t length = strlen (*word);

      if (scr_strncasecmp (line, *word, length) == 0
          && (line[length] == NUL || line[length] == ' '))
        rest_ = line + length;
    }
  if (!rest_)
    return FALSE;

  rest_ += strspn (rest_, " ");
  if (scr_strncasecmp (rest_, "down", 4) == 0
      && (rest_[4] == NUL || rest_[4] == ' '))
    rest_ += 4;
  else if (scr_strncasecmp (rest_, "up", 2) == 0
           && (rest_[2] == NUL || rest_[2] == ' '))
    rest_ += 2;
  rest_ += strspn (rest_, " ");
  return rest_[0] == NUL
         || ((scr_strncasecmp (rest_, "on", 2) == 0
              || scr_strncasecmp (rest_, "in", 2) == 0)
             && (rest_[2] == NUL || rest_[2] == ' '));
}


/*
 * lib_sitstand_examines()
 * lib_sitstand_swears()
 *
 * The examines and swearing words, anywhere in the line, by Runner version.
 */
static scr_bool
lib_sitstand_examines (const scr_char *line, scr_int taf_version)
{
  static const scr_char *const EXAMINES[] = {
    "x", "examine", "look at", "ex", "exam", "read", nullptr
  };
  const scr_char *const *word;

  for (word = EXAMINES; *word; word++)
    if (lib_co_contains (line, *word))
      return TRUE;
  return (taf_version >= TAF_VERSION_380 && lib_co_contains (line, "look in"))
         || (taf_version >= TAF_VERSION_390
             && (lib_co_contains (line, "look") || lib_co_contains (line, "l")));
}

static scr_bool
lib_sitstand_swears (const scr_char *line, scr_int taf_version)
{
  static const scr_char *const PROFANITY[] = {
    "shit", "fuck", "bastard", "cunt", "crap", "hell", "shag", "bollocks",
    "bollox", "piss", nullptr
  };
  const scr_char *const *word;

  for (word = PROFANITY; *word; word++)
    if (lib_co_contains (line, *word))
      return TRUE;
  return (taf_version >= TAF_VERSION_390 && lib_co_contains (line, "bugger"))
         || (taf_version < TAF_VERSION_400 && lib_co_contains (line, "bloody"));
}


/*
 * lib_sitstand_prerun()
 *
 * wears() and removes() run before sitstand and keep their move; a
 * 3.7/4.0 put keeps the line only when it moved something (a refusal
 * does not claim there, see lib_sitstand_anywhere()).  The text goes either
 * way: sitstand writes over it.  Returns TRUE when a put moved something.
 */
static scr_bool
lib_sitstand_prerun (scr_gameref_t game, lib_line_runner_t run_line,
                     const std::string &stripped, scr_bool putish)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t before = pf_buffer_length (filter);
  std::vector<scr_int> positions;
  scr_int object;

  if (putish)
    GS_FOR_EACH_OBJECT (game, object)
      positions.push_back (gs_object_position (game, object)
                           * (gs_object_count (game) + 2)
                           + gs_object_parent (game, object) + 1);
  run_line (game, stripped.c_str ());
  if (putish)
    GS_FOR_EACH_OBJECT (game, object)
      if (gs_object_position (game, object)
          * (gs_object_count (game) + 2)
          + gs_object_parent (game, object) + 1 != positions[object])
        return TRUE;
  pf_truncate (filter, before);
  return FALSE;
}


/*
 * lib_sitstand_anywhere()
 *
 * The Runner's sitstand is one proc of blocks, each entered on its word
 * ANYWHERE in the line -- c("sit"), c("stand"), c("lie") (4.0 also
 * c("lay")) -- run in that code order whatever the word order, all sharing
 * the player's position and parent, each overwriting the one message
 * (run400 46B370-46BCF8, run390 444010-444A04, run380 433F8x-4345xx,
 * run370 42AE7D-42B3xx).  generaltasks calls it unconditionally after the
 * takes, drops, inventory and task handlers have left on a line they
 * claimed, and after wears, removes, battle and hints have written only a
 * message it may overwrite; openclose after it writes nothing once it has
 * (run390 Call sitstand() 45F50D, openclose 45F512).  examines comes later
 * still and replaces the text but not the move.  Measured on p37SIT..p4SIT
 * (run370x runner_probes/sit.run370.sit4.rtf, run380x
 * runner_probes/sit.run380.sit4.rtf, run390x
 * runner_probes/sit.run390.sit4.txt, run400x
 * runner_probes/sit.run400.sit4.txt, 2026-09-19), every Runner alike:
 *   sit lie, lie stand                  "You lie down on the ground."
 *   stand sit, sit stand                "You stand up."
 *   please sit, sit quietly, push stone sit, open stool sit, wear coin sit
 *                                       "You sit down on the ground."
 *   sit on stool lie, lie on stool sit  "You lie down on the stool."
 *   sit on chair stand on stool         "You stand on the chair." (index)
 *   stand up sit down lie down          "You lie down on the ground."
 *   x stool sit                         the stool's description; sitting
 *   sit and wait (3.7-3.9)              "You sit down on the ground."
 * A line whose only such word leads it, followed by nothing, down/up, or
 * on/in, is left to the rows that already answer it with their own
 * refusals.
 *
 * The rest of generaltasks around it, measured on p37SITN..p4SITN (the SIT
 * world plus a worn hat and Bob with a "hat" topic) with
 * make_3738_sitnpcprobe.py's feed (run370x runner_probes/sitn.run370.rtf,
 * run380x runner_probes/sitn.run380.rtf, run390x
 * runner_probes/sitn.run390.txt, run400x runner_probes/sitn.run400.txt,
 * 2026-09-20):
 *   - takes and drops claim the line (GoTo past sitstand): `take stool
 *     sit` takes, `drop stool sit` drops, no move.  A refused put claims
 *     at 3.8 and 3.9 ("You can't put anything on the stool.") but not at
 *     3.7 or 4.0, where `put coin on stool sit` sits ON THE STOOL -- the
 *     put's refusal is overwritten and the object loop takes the stool.
 *   - inventory, the help hint, give, say, a direction and `hint` (an exact
 *     line) all lose to the sit: `i sit`, `sit i`, `inventory sit`, `n sit`,
 *     `sit n`, `north sit`, `help sit`, `hint sit`, `say hello sit`, `give
 *     coin to bob sit` are "You sit down on the ground.", nothing listed,
 *     nobody moved, the coin kept.  Everything below the wait gate is
 *     `If msg = "" Then` (run390 45FFE8, 460004), so therest never speaks.
 *   - wears and removes DO their move before sitstand overwrites them:
 *     `wear hat sit` wears the hat and says "You sit down on the ground."
 *     (x me: "...sitting down. You are wearing a hat."); `remove hat sit`
 *     removes it the same way, 4.0 included.
 *   - `score` anywhere (run390 45F6B5, run400 48A6AE) and the swearing arm
 *     (run390 45F8E4-45F9CE, run400 48A976) come AFTER sitstand and
 *     overwrite it: `score sit` sits and prints the score, `shit sit` sits
 *     and prints the language line.  `bugger`/`bloody` only where that
 *     Runner's list has them.
 *   - characters() runs last and overwrites too.  `talk to bob sit` sits and
 *     answers 'Use the format "ask Bob about [subject]".' at every version
 *     (c("talk to"), run390 45973D).  `ask bob about hat sit` sits and
 *     answers "BOB HAT."; `sit ask bob about hat` does so only in 4.0 --
 *     the pre-4.0 ask arm wants the character's name at column 5, i.e.
 *     "ask <name> ..." leading the line (InStr(...) <> 5 skips it, run390
 *     459818-459895), so 3.7/3.8/3.9 answer "You sit down on the ground."
 * run_line runs the standard rows on a stripped line for the wear/remove
 * and 3.7/4.0 put pre-runs.  On FALSE with *rest set, the caller should
 * dispatch the rest of the line from *rest (the sit words cut), so the ask
 * and talk rows see the shape they know.  Returns TRUE when it answered the
 * line; an examine line gets the move and FALSE, so the examine row speaks.
 */
scr_bool
lib_sitstand_anywhere (scr_gameref_t game, lib_line_runner_t run_line,
                       std::string *rest)
{
  static const scr_char *const LEFT_ALONE[] = {
    "get", "take", "pick", "drop", "leave", nullptr
  };
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int taf_version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *const *word;
  scr_bool sit, stand, lie, examine, profanity, score, speaks;
  scr_int blocks;

  if (!line || game->pending_endgame != 0)
    return FALSE;

  /*
   * Deliberate deviation (2026-09-30).  The Runners enter sitstand on the
   * word anywhere, so `open hat stand` was "You are already standing!",
   * `push lie detector` "You are already lying down.", and `ask bob about
   * stand` or `tell bob a lie` lost their topic.  A sit/stand/lie inside
   * a longer object or character name is no verb (lib_mask_long_names()),
   * and a line that says, asks, tells or talks is free text.
   */
  if (lib_sitstand_speech_line (line, taf_version))
    return FALSE;
  const std::string masked = lib_mask_long_names (game, line);
  lib_sitstand_trace_mask (line, masked, taf_version);

  sit = lib_co_contains (masked.c_str (), "sit");
  stand = lib_co_contains (masked.c_str (), "stand");
  lie = lib_co_contains (masked.c_str (), "lie")
        || (taf_version >= TAF_VERSION_400
            && lib_co_contains (masked.c_str (), "lay"));
  blocks = sit + stand + lie;
  if (blocks == 0)
    return FALSE;

  for (word = LEFT_ALONE; *word; word++)
    if (lib_co_contains (line, *word))
      return FALSE;

  const scr_bool wearish = lib_co_contains (line, "wear")
                           || lib_co_contains (line, "put on")
                           || lib_co_contains (line, "remove");
  const scr_bool putish = !wearish && lib_co_contains (line, "put");
  if (putish
      && (taf_version == TAF_VERSION_380 || taf_version == TAF_VERSION_390))
    return FALSE;

  if (blocks == 1 && lib_sitstand_plain_move (line))
    return FALSE;

  examine = lib_sitstand_examines (line, taf_version);
  profanity = lib_sitstand_swears (line, taf_version);
  score = lib_co_contains (line, "score");

  /* characters() overwrites last: talk-to anywhere, ask leading pre-4.0. */
  speaks = lib_co_contains (line, "talk to");
  if (lib_co_contains (line, "ask"))
    speaks = speaks || taf_version >= TAF_VERSION_400
             || scr_strncasecmp (line, "ask ", 4) == 0;

  const std::string stripped = lib_sitstand_strip (line, taf_version);

  if ((wearish || putish) && run_line && !stripped.empty ()
      && lib_sitstand_prerun (game, run_line, stripped, putish))
    return TRUE;

  const size_t mark = pf_buffer_length (filter);
  if (sit)
    lib_sitstand_block (game, MOVE_SIT, mark);
  if (stand)
    lib_sitstand_block (game, MOVE_STAND, mark);
  if (lie)
    lib_sitstand_block (game, MOVE_LIE, mark);

  if (speaks)
    {
      pf_truncate (filter, mark);
      if (rest)
        *rest = stripped;
      return FALSE;
    }
  if (profanity)
    {
      pf_truncate (filter, mark);
      return lib_cmd_profanity (game);
    }
  if (score)
    {
      pf_truncate (filter, mark);
      return lib_cmd_score (game);
    }
  if (examine)
    {
      pf_truncate (filter, mark);
      return FALSE;
    }
  return pf_buffer_length (filter) > mark;
}


/*
 * lib_openclose_word()
 *
 * Where the line's first open/close word stands, and how long it is.
 */
static const scr_char *
lib_openclose_word (const scr_char *line, const scr_char **word_out)
{
  static const scr_char *const WORDS[] = { "open", "close", nullptr };
  const scr_char *scan;

  for (scan = line; *scan != NUL; scan++)
    {
      const scr_char *const *word;

      if (scan != line && scan[-1] != ' ')
        continue;
      for (word = WORDS; *word; word++)
        {
          const size_t size = strlen (*word);

          if (scr_strncasecmp (scan, *word, size) == 0
              && (scan[size] == NUL || scan[size] == ' '))
            {
              *word_out = *word;
              return scan;
            }
        }
    }
  return nullptr;
}


/*
 * lib_line_cut_word()
 *
 * The line with the span at AT, LENGTH long, taken out of it.
 */
static std::string
lib_line_cut_word (const scr_char *line, const scr_char *at, size_t length)
{
  std::string out (line, at - line);
  const scr_char *tail = at + length;

  tail += strspn (tail, " ");
  out += tail;
  while (!out.empty () && out[out.size () - 1] == ' ')
    out.erase (out.size () - 1);
  return out;
}


/*
 * lib_openclose_anywhere()
 *
 * openclose is a plain Call, entered on c("open") / c("close") -- the whole
 * word ANYWHERE in the line -- and generaltasks makes it on EVERY line, one
 * line below sitstand (run400 48A515, run390 45F512, run380 4422xx, run370
 * 43B9xx).  It cannot claim, so the handlers below it overwrite its message
 * while keeping its act, and the ones above it claim the line before it is
 * reached.  Measured on p37ORD..p4ORD (make_orderprobe.py; run370x
 * runner_probes/ord.run370.ord2.rtf, run380x
 * runner_probes/ord.run380.ord2.rtf, run390x
 * runner_probes/ord.run390.ord2.txt, run400x
 * runner_probes/ord.run400.ord2.txt, 2026-09-21), the box shut before each
 * cell:
 *   x open box, open examine box, open look at box   the description of an
 *   open read box, open x box (3.7-3.9)              OPEN box; it opened
 *   open where is box                 "You are carrying the box!", and open
 *   push open box, open x box (4.0)   "You open the box.", therest silent
 *   open take box                     "You take the box.", and still shut
 *   wear open hat                     "You put on the hat." (hat unopenable)
 * The Runner's own gate is the message buffer: its two refusals, " can't
 * open " (4756EA) and " can't see " (475952), are written only when nothing
 * has spoken yet, while the success (475822), " is already open!" (47592D),
 * " as it is locked!" (4757A5) and " not carrying " (4758D3) are plain
 * assignments that overwrite.  So the act, not the refusal, is what carries
 * past a handler above it, and that is the test here: the pass runs the
 * open/close row for its ACT on a line re-spelled with the word at the head,
 * takes the message back when a handler below openclose will speak for the
 * line, and hands that handler the line with the open/close word cut out.
 *
 * Only open and close are read.  The lock and unlock arms of the same proc
 * (475D71, 47612F) are called just as unconditionally, but no probe row has
 * stood them beside a second verb, and "pick lock" is a task command in four
 * of the walkthroughs.
 */
scr_bool
lib_openclose_anywhere (scr_gameref_t game, const scr_char *typed,
                        lib_line_runner_t run_line, std::string *rest)
{
  /*
   * takes, drops, put_drop_list and the inventory listing claim the line
   * above openclose.  gotoplace runs below it and adds to what it says (or
   * walks over it); run_goto_line_class() sees to that half, so a goto line
   * gets openclose's own answer here.
   */
  static const scr_char *const LEFT_ALONE[] = {
    "get", "take", "pick", "drop", "put", "leave", "i", "inv", "inventory",
    nullptr
  };
  static const scr_char *const EXAMINES[] = {
    "examine", "look at", "read", nullptr
  };
  static const scr_char *const EXAMINE_HEADS_400[] = {
    "x", "ex", "exam", nullptr
  };
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int taf_version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *const *word;
  const scr_char *at, *hit = nullptr, *cut_at, *cut_hit = nullptr;
  scr_bool speaks;
  scr_int object;

  if (!typed || !line || !run_line || game->pending_endgame != 0)
    return FALSE;

  /*
   * The tests are the typed line's, the way generaltasks hands every
   * handler the line as it stands.  What the pass hands BACK is the line
   * the anchored rows are already working from, since the two-verb rules
   * above may have re-spelled it -- and below 4.0 they drop the open/close
   * word themselves when the other verb is one of the five.
   */
  at = lib_openclose_word (typed, &hit);
  if (!at)
    return FALSE;

  /*
   * Deliberate deviation (2026-09-30): the words below are tested on the
   * line with every longer object and character name masked
   * (lib_mask_long_names()), so `open drop box` is an open and not a drop,
   * and an "open" inside a name opens nothing.
   */
  const std::string masked = lib_mask_long_names (game, typed);
  const scr_char *const words = masked.c_str ();
  {
    const scr_char *masked_hit = nullptr;

    if (!lib_openclose_word (words, &masked_hit))
      {
        const scr_char *typed_hit = nullptr;

        if (SCR_TRACING_DEVIATIONS () && lib_openclose_word (typed, &typed_hit))
          SCR_DEVIATION ("mask_openclose", "masked=\"%s\"", words);
        return FALSE;
      }
  }
  cut_at = lib_openclose_word (line, &cut_hit);

  const std::string cut =
      cut_at ? lib_line_cut_word (line, cut_at, strlen (cut_hit))
             : std::string (line);

  for (word = LEFT_ALONE; *word; word++)
    if (lib_co_contains (words, *word))
      {
        /* The claim is theirs, and only the leading open/close word stands
           between them and the anchored row that carries it. */
        if (at == typed && rest)
          *rest = cut;
        return FALSE;
      }

  /* Everything below openclose that writes over it: the typed look and
     examines (x, ex and exam at the head only from 4.0), score, whereis,
     and characters, whose ask arm wants the name at column 5 below 4.0. */
  speaks = FALSE;
  for (word = EXAMINES; *word && !speaks; word++)
    speaks = lib_co_contains (words, *word);
  if ((taf_version >= TAF_VERSION_380 && lib_co_contains (words, "look in"))
      || (taf_version >= TAF_VERSION_390
          && (lib_co_contains (words, "look") || lib_co_contains (words, "l"))))
    speaks = TRUE;
  for (word = EXAMINE_HEADS_400; *word && !speaks; word++)
    {
      const size_t size = strlen (*word);

      speaks = taf_version >= TAF_VERSION_400
               ? scr_strncasecmp (words, *word, size) == 0
                 && (words[size] == NUL || words[size] == ' ')
               : lib_co_contains (words, *word);
    }
  if (lib_co_contains (words, "where") || lib_co_contains (words, "find")
      || lib_co_contains (words, "locate") || lib_co_contains (words, "score")
      || lib_co_contains (words, "talk to"))
    speaks = TRUE;
  if (lib_co_contains (words, "ask"))
    speaks = speaks || taf_version >= TAF_VERSION_400
             || scr_strncasecmp (words, "ask ", 4) == 0;
  /*
   * And the `go` nudge at the end of the verb sweep, which overwrites
   * whatever was said (lib_cmd_just_a_direction()): `open go to cave` is
   * "Just a direction will do." at every version, since a mid-line "go to" is
   * no gotoplace line (make_orderprobe.py, run370x
   * runner_probes/ord.run370.ord.rtf, run380x
   * runner_probes/ord.run380.ord.rtf, run390x
   * runner_probes/ord.run390.ord.txt, run400x
   * runner_probes/ord.run400.ord.txt).  The nudge row answers a bare `go`.
   */
  const scr_bool nudged = lib_co_contains (words, "go")
                          && !lib_goto_line_enters (game, typed);
  speaks = speaks || nudged;

  /* A line the open/close word leads, with nobody below to speak for it, is
     the anchored row's own and is answered where it always was. */
  if (at == typed && !speaks)
    return FALSE;

  {
    const std::string body = lib_line_cut_word (typed, at, strlen (hit));
    const std::string acting = std::string (hit)
                               + (body.empty () ? "" : " ") + body;
    const size_t mark = pf_buffer_length (filter);
    std::vector<scr_int> openness;
    scr_bool acted = FALSE;

    GS_FOR_EACH_OBJECT (game, object)
      openness.push_back (gs_object_openness (game, object));
    run_line (game, acting.c_str ());
    GS_FOR_EACH_OBJECT (game, object)
      if (gs_object_openness (game, object) != openness[object])
        acted = TRUE;

    /*
     * give fills only an EMPTY buffer, and " not carrying " (4758D3) is an
     * outright write, so at 4.0 -- where the box must be held to open --
     * `give open box bob` is openclose's "You are not carrying the box!"
     * though nothing opened (p4ORD, run400x
     * runner_probes/ord.run400.give.txt).  Below 4.0 the box simply opens.
     */
    if (!acted && !speaks && taf_version >= TAF_VERSION_400
        && lib_co_contains (words, "give") && pf_buffer_length (filter) > mark)
      return TRUE;
    if (!acted || speaks)
      {
        pf_truncate (filter, mark);
        if (speaks && rest)
          *rest = nudged ? std::string ("go") : cut;
        return FALSE;
      }
    return pf_buffer_length (filter) > mark;
  }
}


/*
 * lib_whereis_anywhere()
 *
 * whereis (run400 4684E4, entered on c("where"), c("find") or c("locate") at
 * 467CE5-467D03) is another plain Call generaltasks makes on every line, and
 * it sits below examines -- which claims the line, so `x where is coin` is
 * the coin's description and no where-is -- and above therest, whose arms are
 * all `If msg = "" Then`: `push where is coin` is "The coin is lit room." at
 * every version (cells 52 and 33; the same four transcripts as
 * lib_openclose_anywhere()).  A line naming a handler that claims or acts
 * above it is left alone; nothing has measured those.
 */
scr_bool
lib_whereis_anywhere (scr_gameref_t game, lib_line_runner_t run_line)
{
  static const scr_char *const WORDS[] = {
    "where", "find", "locate", nullptr
  };
  static const scr_char *const LEFT_ALONE[] = {
    "get", "take", "pick", "drop", "put", "leave", "i", "inv", "inventory",
    "wear", "remove", "sit", "stand", "lie", "wait", "examine", "look at",
    "look in", "look", "l", "x", "ex", "exam", "read", "ask", "talk to",
    "goto", "go to", "go", "open", "close", "score", nullptr
  };
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *line = run_get_dispatch_input ();
  const scr_char *const *word;
  const scr_char *scan, *at = nullptr;

  if (!line || !run_line || game->pending_endgame != 0)
    return FALSE;

  for (scan = line; *scan != NUL && !at; scan++)
    {
      if (scan != line && scan[-1] != ' ')
        continue;
      for (word = WORDS; *word && !at; word++)
        {
          const size_t size = strlen (*word);

          if (scr_strncasecmp (scan, *word, size) == 0
              && (scan[size] == NUL || scan[size] == ' '))
            at = scan;
        }
    }
  /* At the head it is the anchored row's line already. */
  if (!at || at == line)
    return FALSE;

  for (word = LEFT_ALONE; *word; word++)
    if (lib_co_contains (line, *word))
      return FALSE;

  {
    const size_t mark = pf_buffer_length (filter);

    run_line (game, at);
    if (pf_buffer_length (filter) > mark)
      return TRUE;
    pf_truncate (filter, mark);
    return FALSE;
  }
}

/*
 * lib_cmd_get_off_object()
 * lib_cmd_get_off()
 *
 * Get off whatever supporter the player rests on.
 */
scr_bool
lib_cmd_get_off_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /*
   * Before 4.0 the line reaches takes() first: its entry test excludes only
   * c("get on") and c("get down") (run390 4544C6), and generaltasks leaves on
   * a take that happened (45F439) before sitstand (45F50D) runs.  3.7/3.8
   * have no get-off at all, so there `get off stool` IS a take, even when
   * the player is on the stool: "You pick up the stool.", "You can't take a
   * chair.", "You've already got a stool!" (run370x/run380x p3738sit3,
   * 2026-09-19).  Deliberate deviation: they get the 3.9 behaviour, where a
   * take that happened stands and any other take answer is overwritten by
   * sitstand's below (see lib_cmd_get_off()).  And only where the line
   * might name an "off ..." object; see lib_get_off_may_take().
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400
      && lib_player_parent_here (game) == -1
      && lib_get_off_may_take (game))
    {
      const scr_filterref_t filter = gs_get_filter (game);
      const scr_char *line = run_get_dispatch_input ();
      const scr_char *rest = line ? strstr (line, " off ") : nullptr;
      const size_t mark = pf_buffer_length (filter);
      scr_int held_before, held_after;
      scr_bool taken;

      held_before = 0;
      GS_FOR_EACH_OBJECT (game, object)
        held_before += gs_object_position (game, object) == OBJ_HELD_PLAYER;
      taken = FALSE;
      if (rest)
        {
          var_set_ref_text (gs_get_vars (game), rest + 5);
          taken = lib_cmd_take_multiple (game);
        }
      held_after = 0;
      GS_FOR_EACH_OBJECT (game, object)
        held_after += gs_object_position (game, object) == OBJ_HELD_PLAYER;
      if (taken && held_after > held_before)
        return TRUE;
      pf_truncate (filter, mark);
    }

  /*
   * 3.9+ get-off asks about the parent before it looks at the name: `get off
   * chair` standing on nothing is "You are not standing on anything!"
   * (run400 46B702, run390 4443E1; run390x runner_probes/sit.run390.sit3.txt,
   * run400x runner_probes/sit.run400.sit3.txt, 2026-09-19).
   */
  if (lib_player_parent_here (game) == -1)
    return lib_print_response_message (game,
                                "You are not standing on anything!\n",
                                "I am not standing on anything!\n",
                                "%player% is not standing on anything!\n");

  /* Get the referenced object; if none, consider complete. */
  object = lib_disambiguate_object (game, "get off", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject the attempt if the player is not on the given object. */
  if (lib_player_parent_here (game) != object)
    {
      lib_print_response_object (game,
                                 "You are not standing on ",
                                 "I am not standing on ",
                                 "%player% is not standing on ",
                                 object, "!\n");
      return TRUE;
    }

  /* Confirm movement. */
  lib_print_response_object (game,
                             "You get off ",
                             "I get off ",
                             "%player% get off ",
                             object, ".\n");

  /* Adjust player position and parent. */
  gs_set_playerposition (game, 0);
  gs_set_playerparent (game, -1);
  return TRUE;
}

/*
 * lib_player_parent_here()
 *
 * The object the player stands, sits or lies on, for get-off.  Before 3.9
 * walking off leaves the parent in place (see lib_go()), so there a parent
 * that is not in the player's room is a stale one, and the player is on
 * nothing.
 */
scr_int
lib_player_parent_here (scr_gameref_t game)
{
  const scr_int parent = gs_playerparent (game);

  if (parent != -1
      && prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
      && !obj_indirectly_in_room (game, parent, gs_playerroom (game)))
    return -1;
  return parent;
}

/*
 * lib_name_starts_off()
 * lib_get_off_may_take()
 *
 * Whether `get off X` may still be read as a take of X while the player is
 * on nothing.  Every Runner reads it so, 3.7 to 4.0: the take rows see the
 * line before any get-off (see lib_cmd_get_off_object()), so `get off
 * stool` beside the stool picked it up.  Deliberate deviation since
 * 2026-10-15: it is "You are not standing on anything!" instead, unless the
 * game has a takeable object a Short or Alias of which starts with the word
 * "off" (an "off switch"), which such a line might really name.  No game in
 * the corpus has one: the only whole-word "off" names are insidejob's static
 * "Log Off" and SBFT's "cut-off jeans".
 */
static scr_bool
lib_name_starts_off (const scr_char *name)
{
  if (!name)
    return FALSE;
  name += strspn (name, " ");
  return scr_strncasecmp (name, "off", 3) == 0
         && (name[3] == ' ' || name[3] == NUL);
}

scr_bool
lib_get_off_may_take (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      if (obj_is_static (game, object))
        continue;
      if (lib_name_starts_off (prop_get_indexed_string (bundle, "Objects",
                                                        object, "Short")))
        return TRUE;
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          if (lib_name_starts_off (prop_get_string (bundle, "S<-sisi",
                                                    vt_key)))
            return TRUE;
        }
    }
  return FALSE;
}

scr_bool
lib_cmd_get_off (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /*
   * `get off`, `get down` and `get on X` arrived in the Runner at 3.9;
   * run370 and run380 have neither literal, so `get off` there is the take
   * handler's "Take what?" (run380 on Wrecked, microwaveman).  Deliberate
   * deviation: Scarier understands them at every version, with the 3.9
   * behaviour.  A game's own task still wins, tasks being matched first.
   */
  /* Reject the attempt if the player is not on anything. */
  if (lib_player_parent_here (game) == -1)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not standing on anything!\n",
                                             "I am not standing on anything!\n",
                                             "%player% is not standing on anything!\n"));
      return TRUE;
    }

  /* Confirm movement. */
  lib_print_response_object (game,
                             "You get off ",
                             "I get off ",
                             "%player% get off ",
                             gs_playerparent (game), ".\n");

  /* Adjust player position and parent. */
  gs_set_playerposition (game, 0);
  gs_set_playerparent (game, -1);
  return TRUE;
}

/*
 * lib_cmd_get_down()
 *
 * `get down` shares the Runner's dismount branch with `get off`: both
 * arrived in 3.9, and Scarier takes both at every version (see
 * lib_cmd_get_off()).
 */
scr_bool
lib_cmd_get_down (scr_gameref_t game)
{
  return lib_cmd_get_off (game);
}

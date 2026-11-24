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
 * Room groups, room alternates and room names: the world-model queries about
 * rooms that read game state and print nothing.  They keep the lib_ prefix
 * they had in library/sclibrar_print.cpp.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "scgamest_api.h"
#include "screstrs.h"
#include "scobjcts.h"
#include "scrooms.h"
#include "scgamest.h"


/* Trace flag, set before running; it follows the library trace flag. */
static scr_bool room_trace = FALSE;

/*
 * lib_random_roomgroup_member()
 *
 * Return a random member of a roomgroup.
 */
scr_int
lib_random_roomgroup_member (scr_gameref_t game, scr_int roomgroup)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int count, room;

  /* Get the count of rooms in the group. */
  vt_key[0].string = "RoomGroups";
  vt_key[1].integer = roomgroup;
  vt_key[2].string = "List2";
  count = prop_get_child_count (bundle, "I<-sis", vt_key);
  if (count == 0)
    {
      /*
       * The group contains no rooms -- some games define a room group but
       * never assign any rooms to it, then point an NPC walk (or other move)
       * at it.  There is no valid destination, so return -1 ("no room"); each
       * caller leaves the mover where it is.  The Runner tolerates this rather
       * than aborting.
       */
      if (room_trace)
        scr_trace ("Library: room group %ld is empty, no destination\n",
                  roomgroup);
      return -1;
    }

  /* Pick a room at random and return it. */
  vt_key[3].integer = scr_randomint (0, count - 1);
  room = prop_get_integer (bundle, "I<-sisi", vt_key);

  if (room_trace)
    {
      scr_trace ("Library: random room for group %ld is %ld\n",
                roomgroup, room);
    }

  return room;
}


/*
 * lib_room_alt_task()
 * lib_room_alt_object_state()
 *
 * The task and stateful-object alternate types.  VT_KEY is the caller's
 * Rooms/room/Alts/alt key, with the last element free for the field name.
 */
static scr_bool
lib_room_alt_task (scr_gameref_t game, scr_vartype_t *vt_key)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool retval;
  scr_int var2, var3;

  vt_key[4].string = "Var2";
  var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
  if (!gs_task_valid (game, var2 - 1))     /* No task. */
    retval = TRUE;
  else
    {
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

      retval = gs_task_done (game, var2 - 1) == !(var3 != 0);
    }
  return retval;
}

static scr_bool
lib_room_alt_object_state (scr_gameref_t game, scr_vartype_t *vt_key)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool retval;
  scr_int var2, var3;

  vt_key[4].string = "Var2";
  var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
  if (var2 == 0)          /* No object. */
    retval = TRUE;
  else
    {
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

      /*
       * Var2 here is a 1-based GLOBAL object number, NOT an index
       * into the stateful-object list like a task object-state
       * restriction's Var1.  Proof is corpus-wide: Professor Von
       * Witt's Laboratory alts carry Var2 = 5 for the mailbox, which
       * is stateful object #1 but global object #5 (run400 displays
       * the alt keyed on the mailbox's state); and many games author
       * Var2 far beyond their stateful-object count, which no
       * stateful reading could address at all -- Beanstalk 8 of 3,
       * Terrified 30 of 3, Showtime at the Gallows 59 of 1,
       * goldilocks 90 of 22, cursed 683.  (Task restrictions and
       * change-status actions really are stateful-indexed: across
       * the 107 corpus games using them, no index exceeds the
       * stateful count.)  SCARE always mapped Var2 through the
       * stateful list here, so every one of these alts tested the
       * wrong object's state.
       */
      retval = restr_object_in_state (game, var2 - 1, var3 - 1);
    }
  return retval;
}

/*
 * lib_room_alt_player_object()
 *
 * The object a player-condition alternate tests, from its Var2 (the
 * condition) and Var3 (the object number); -1 if there is none.
 */
static scr_int
lib_room_alt_player_object (scr_gameref_t game, scr_int var2, scr_int var3)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object;

  if (var3 == 0)
    {
      /*
       * No object selected.  The Runner still runs the same test, on
       * an object it cannot find, so every "isn't ..." condition
       * holds and every "is ..." condition fails.  SCARE's table had
       * that right for holding (0/1) and wearing (2/3) but inverted
       * for the same-room pair, answering FALSE to "isn't in the same
       * room as <nothing>".  Measured in run400 on The X-Files: A New
       * Beginning, whose Lobby (room 1) and Davis Storage Warehouse
       * (room 8) both carry a Var2 = 4, Var3 = 0 alt -- the Runner
       * prints "Do you have your badge?" and "You left the key back
       * in D.C., didn't you?" on every visit, unconditionally, where
       * SCARE printed neither ever.  Only three alts corpus-wide use
       * Var3 = 0: those two, and one in House.taf with empty text.
       *
       * That "cannot find" holds only while object 0 belongs to the
       * list Var3 indexes.  Otherwise, run400's Var3 - 1 = -1 lookup
       * (Proc_19_12 @453249) falls through to object 0 itself, and
       * the test runs on that object.  The list is the Wearable flag
       * alone for the wearing pair and non-static for the rest.
       * Measured on Dragonsphere, where object 0 is the backpack:
       * it is worn but not flagged Wearable, and the Runner never
       * prints the "isn't wearing" alt "It is very cold out here."
       */
      if (prop_get_taf_version (bundle) >= TAF_VERSION_400
          && gs_object_count (game) > 0
          && ((var2 == 2 || var2 == 3)
              ? !prop_get_indexed_boolean (bundle, "Objects", 0,
                                           "Wearable")
              : obj_is_static (game, 0)))
        object = 0;
      else
        object = -1;
    }
  else if (var2 == 2 || var2 == 3)
    object = obj_wearable_object (game, var3 - 1);
  else
    object = obj_dynamic_object (game, var3 - 1);

  return object;
}

/* The answer of player condition VAR2 when it has no object to test. */
static scr_bool
lib_room_alt_player_no_object (scr_int var2)
{
  scr_bool retval;

  switch (var2)
    {
    case 0: case 2: case 4:
      retval = TRUE;
      break;
    case 1: case 3: case 5:
      retval = FALSE;
      break;
    default:
      scr_fatal ("lib_use_room_alt:"
                " invalid player condition, %ld\n", var2);
    }
  return retval;
}

/* The answer of player condition VAR2 for OBJECT. */
static scr_bool
lib_room_alt_player_test (scr_gameref_t game, scr_int var2, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool retval;

  switch (var2)
    {
    case 0:              /* Isn't holding (or wearing). */
      /*
       * The Runner's holding test is its recursive possession
       * predicate (run400's isdark calls Proc_21_46 @44615C), so an
       * object inside or on something the player carries or wears
       * counts as held.  SCARE tested the object's own position only.
       * Measured in run400 on The X-Files: A New Beginning, whose
       * Parking Garage B (room 3) carries a Var2 = 1, Var3 = 2 alt on
       * the gun, which lives inside the worn holster: the Runner
       * prints "It may be unwise to pull a gun on this guy." on every
       * visit, where SCARE printed the unconditioned alt instead.
       *
       * That is 4.0 only.  The pre-4.0 Runners test the object's own
       * position field, exactly as SCARE did: run390 isdark (433920,
       * the ladder transcribed below) reads objects(ns(Obj)).global_22
       * raw -- `<> 0` for "isn't holding", `= 0` for "is holding" --
       * and never walks a container, and run380 43C708 / run370 434E95
       * are the same code.  Measured in run390 on Govard
       * (runner_transcripts/govard.txt): the Ruins' alt
       * "...на юго-востоке - Хибара." (room 11, Obj 15 the knife,
       * TypeHideObjects 10) fires while the knife is carried directly
       * and stops the moment it is put in the worn belt, and the
       * Road's alt (room 23, Obj 105 the licence) never fires while
       * the licence sits inside the held hunter's bag.  So a held
       * container's contents are NOT held for a pre-4.0 room alt.
       */
      if (prop_get_taf_version (bundle) < TAF_VERSION_400)
        retval = gs_object_position (game, object) != OBJ_HELD_PLAYER;
      else
        retval = !gs_runner_possessed (game, object);
      break;
    case 1:              /* Is holding (or wearing). */
      if (prop_get_taf_version (bundle) < TAF_VERSION_400)
        retval = gs_object_position (game, object) == OBJ_HELD_PLAYER;
      else
        retval = gs_runner_possessed (game, object);
      break;
    case 2:              /* Isn't wearing. */
      retval = gs_object_position (game, object) != OBJ_WORN_PLAYER;
      break;
    case 3:              /* Is wearing. */
      retval = gs_object_position (game, object) == OBJ_WORN_PLAYER;
      break;
    case 4:              /* Isn't in the same room as. */
      retval = !obj_indirectly_in_room (game,
                                        object, gs_playerroom (game));
      break;
    case 5:              /* Is in the same room as. */
      retval = obj_indirectly_in_room (game,
                                       object, gs_playerroom (game));
      break;
    default:
      scr_fatal ("lib_use_room_alt:"
                " invalid player condition, %ld\n", var2);
    }
  return retval;
}

/* The player-condition alternate type; VT_KEY as lib_room_alt_task(). */
static scr_bool
lib_room_alt_player (scr_gameref_t game, scr_vartype_t *vt_key)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int var2, var3, object;

  vt_key[4].string = "Var2";
  var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
  vt_key[4].string = "Var3";
  var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
  object = lib_room_alt_player_object (game, var2, var3);

  /*
   * No object to test -- Var3 = 0 as above, or a Var3 naming no
   * object at all (negative, or an empty list), read the same way.
   */
  if (object < 0)
    return lib_room_alt_player_no_object (var2);

  return lib_room_alt_player_test (game, var2, object);
}


/*
 * lib_use_room_alt()
 *
 * Return TRUE if a particular alternate room description should be used.
 */
scr_bool
lib_use_room_alt (scr_gameref_t game, scr_int room, scr_int alt)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int type;
  scr_bool retval;

  /* Get alternate type. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";
  vt_key[3].integer = alt;
  vt_key[4].string = "Type";
  type = prop_get_integer (bundle, "I<-sisis", vt_key);

  /* Select based on type. */
  switch (type)
    {
    case 0:                    /* Task. */
      retval = lib_room_alt_task (game, vt_key);
      break;

    case 1:                    /* Stateful object. */
      retval = lib_room_alt_object_state (game, vt_key);
      break;

    case 2:                    /* Player condition. */
      retval = lib_room_alt_player (game, vt_key);
      break;

    default:
      scr_fatal ("lib_use_room_alt: invalid type, %ld\n", type);
    }

  return retval;
}


/*
 * lib_room_object_alt_fires()
 * lib_room_alt_darkens()
 * lib_room_is_dark()
 *
 * The pre-4.0 Runners' one and only darkness test, in its two forms.
 *
 * There is no lamp model in ADRIFT 4, and no "It is pitch dark" message.  A
 * dark room is authored as a room whose object condition holds and whose
 * "Hide objects" box is ticked, and the Runner recomputes that one predicate
 * at the top of examines() into a byte it then consults twice.  run390's copy
 * is the ladder at 44B888, over the room record's own fields -- 102 is
 * HideObjects, 100 the object, 104 the condition type -- with the same six
 * cases lib_use_room_alt() already implements for a type-2 alt:
 *
 *     If room(102) = 1 Then
 *       dark = 0
 *       If room(100) <> 0 Then
 *         Select Case room(104)
 *           Case 0: If obj.parent <> 0     Then dark = 1   ' isn't holding
 *           Case 1: If obj.parent  = 0     Then dark = 1   ' is holding
 *           Case 2: If obj.parent <> &H9C  Then dark = 1   ' isn't wearing
 *           Case 3: If obj.parent  = &H9C  Then dark = 1   ' is wearing
 *           Case 4: If obj.parent > 0 And obj.parent <> playerroom Then ...
 *           Case 5: If obj.parent = 0 Or &H9C Or playerroom      Then ...
 *
 * run380 (43C708, room fields 78/76/80) and run370 (434E95, the same three
 * fields, the same six arms) are character-for-character the same
 * computation, so this is not a 3.9 invention.  run400 kept the shape but
 * never assigns its flag: Proc_19_87_471F94's dark byte is dead, which is
 * why 4.0 answers a dark room's `x` with its ordinary tail.  Hence the gate
 * here is < 4.0, not >= 3.90.  Measured on p4DARK.taf, the same cave in the
 * 4.0 layout (runner_probes/dark.run400.{unseen,seen}.txt): no "can't see"
 * answer anywhere, an unseen object "You see no such thing." and a seen one
 * its plain description.  The HideObjects alt still keeps the hidden
 * objects unseen; see lib_print_room_description().
 *
 * The conversion in parse_fixup_v390_v380_room_alts() writes that room-level
 * object condition out as the LAST alt, type 2, with Var2 the condition type,
 * Var3 the object and HideObjects the ticked box -- and it is the only type-2
 * alt a converted game has, the task alts all being type 0 with HideObjects
 * 0.  So scanning the alts for a matching type-2 alt is exactly the Runner's
 * ladder, and the HideObjects term is the room field 102 the ladder is
 * wrapped in.
 *
 * The Runner keeps the two halves apart, and so does this pair:
 *
 *   lib_room_alt_darkens()  is isdark() itself (run390 433920), the condition
 *     alone, with no HideObjects term.  Only afteroa's start-room seen sweep
 *     reads it that way (run390 44192D).
 *   lib_room_is_dark()      is what examines() computes into that byte and
 *     what viewroom acts on -- the condition AND HideObjects.  Every message
 *     site below wants this one.
 *
 * Measured: p39DARK.taf (3.9) under run390,
 * runner_probes/dark.run390.{unseen,seen}.txt -- a Dark Cave whose alt fires
 * while the torch is not held, "Hide objects" ticked.  Also ALEXIS.TAF
 * (3.9), runner_probes/alexis_worn_cube.run390.txt, the Passage in the Caves
 * of eternal night: "It is too dark to see." and no object list, with `x
 * large stone table` and `x holes in the wall` both "You can't see that very
 * clearly." where a lit room would have said "Nothing special."
 */
static scr_bool
lib_room_object_alt_fires (scr_gameref_t game, scr_int room,
                           scr_bool require_hide)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int alt, alt_count;

  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    return FALSE;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";
  alt_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  for (alt = 0; alt < alt_count; alt++)
    {
      scr_int type, hideobjects;

      vt_key[3].integer = alt;
      vt_key[4].string = "Type";
      type = prop_get_integer (bundle, "I<-sisis", vt_key);
      if (type != 2)
        continue;

      if (require_hide)
        {
          vt_key[4].string = "HideObjects";
          hideobjects = prop_get_integer (bundle, "I<-sisis", vt_key);
          if (hideobjects != 1)
            continue;
        }

      if (lib_use_room_alt (game, room, alt))
        return TRUE;
    }

  return FALSE;
}

scr_bool
lib_room_alt_darkens (scr_gameref_t game, scr_int room)
{
  return lib_room_object_alt_fires (game, room, FALSE);
}

scr_bool
lib_room_is_dark (scr_gameref_t game, scr_int room)
{
  return lib_room_object_alt_fires (game, room, TRUE);
}


/*
 * lib_find_starting_alt()
 *
 * Return the alt index for the alt at which we need to start running down
 * the alts list when generating room names or descriptions.  Returns -1 if
 * no alt overrides the default room long description.
 */
scr_int
lib_find_starting_alt (scr_gameref_t game, scr_int room)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int alt_count, alt, retval;

  /* Get count of room alternates. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";
  alt_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /* Search backwards for a method-0 or method-1 overriding description. */
  retval = -1;
  for (alt = alt_count - 1; alt >= 0; alt--)
    {
      scr_int method;

      vt_key[3].integer = alt;
      vt_key[4].string = "DisplayRoom";
      method = prop_get_integer (bundle, "I<-sisis", vt_key);

      if (!(method == 0 || method == 1))
        continue;

      if (lib_use_room_alt (game, room, alt))
        {
          /*
           * A matching alt is the starting point whether or not its M1 is
           * empty.  run400's room lister (Proc_19_63 @472CA4) accumulates
           * forwards and applies the display method on a match with no
           * emptiness test at all (method 0 assigns M1 over the buffer;
           * method 1 resets it to the room's Long and then appends M1 only
           * if both are non-empty), so a later matching method-0/1 alt
           * discards everything appended before it even when its own text
           * is blank.  The pre-4.0 Runners reach the same place by a
           * different route: their viewroom picks the task alt on
           * task-doneness alone (run370 @43318C, run390 @447648) and then
           * skips the base/LastDesc branch entirely.  Only the non-matching
           * branch is guarded, on M2, below.
           */
          retval = alt;
          break;
        }
      else
        {
          const scr_char *m2;

          vt_key[3].integer = alt;
          vt_key[4].string = "M2";
          m2 = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (m2))
            {
              retval = alt;
              break;
            }
        }
    }

  /* Return the index of the base alt, or -1 if none found. */
  return retval;
}


/*
 * lib_get_room_name()
 *
 * Get the name for a given room.  lib_print_room_name() in
 * library/sclibrar_room.cpp prints it.
 */
const scr_char *
lib_get_room_name (scr_gameref_t game, scr_int room)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int alt_count, alt, start;
  const scr_char *name;

  /* Get the basic room name, and the count of room alternates. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  vt_key[2].string = "Alts";
  alt_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /* Get our starting point in the alts list. */
  start = lib_find_starting_alt (game, room);

  /*
   * run400's room lister resets the name to the room's Short and then,
   * inside its one forwards loop over EVERY alt, lets each matching alt with
   * a non-empty Changed overwrite it -- whatever that alt's display method,
   * and whether or not a later method-0/1 alt restarts the description.
   * togetyou (4.0) `infect cut`: The Ear's alt 0 (task 22, method 2,
   * Changed "The Infected Ear") sits below alt 1 (method 1, M2 shown), so
   * the Runner heads the room "The Infected Ear"
   * (runner_transcripts/togetyou.txt T16) where the start-point walk below
   * never reached alt 0 and kept "The Ear".
   *
   * Pre-4.0 keeps the start-point walk, and it can never be seen to differ:
   * no Runner below 4.0 can change a room's name at all.  "Changed" is a
   * field of the 4.0 ROOM_ALT record only (sctafpar.cpp's grammar), so the
   * alts this loader synthesises for a 3.7/3.8/3.9 game are all written with
   * Changed "" (parse_fixup_v390_v380_room_alt), and the Runners agree:
   * run390's viewroom heads the room with `"<b>" & rooms(broom).Short &
   * "</b>"` and consults no alt, while run380's viewroom (439C08) reaches
   * the alt array only for description text.  So the walk below decides the
   * name only for a 4.0 game, where it is bypassed.
   */
  if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
    start = -1;

  /*
   * Run forwards through all alts lower than our starting point, or all alts
   * if no starting point found.
   */
  for (alt = (start != -1) ? start : 0; alt < alt_count; alt++)
    {
      /* Ignore all non-method-2 alts except for the starter (not 4.0). */
      if (alt != start && prop_get_taf_version (bundle) < TAF_VERSION_400)
        {
          scr_int method;

          vt_key[3].integer = alt;
          vt_key[4].string = "DisplayRoom";
          method = prop_get_integer (bundle, "I<-sisis", vt_key);

          if (method != 2)
            continue;
        }

      /* If this alt offers a name change, note it and continue. */
      if (lib_use_room_alt (game, room, alt))
        {
          const scr_char *changed;

          vt_key[3].integer = alt;
          vt_key[4].string = "Changed";
          changed = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (changed))
            name = changed;
        }
    }

  /* Return the final selected name. */
  return name;
}


/*
 * lib_can_go()
 *
 * Return TRUE if the player can move in the given direction.  Also the map's
 * test for which connectors are currently usable (scmap.cpp).
 */
scr_bool
lib_can_go (scr_gameref_t game, scr_int room, scr_int direction)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int restriction;
  scr_bool is_restricted = FALSE;

  /* Set up invariant parts of key. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  vt_key[3].integer = direction;

  /* Check for any movement restrictions. */
  vt_key[4].string = "Var1";
  restriction = prop_get_integer (bundle, "I<-sisis", vt_key) - 1;
  if (restriction >= 0)
    {
      scr_int type;

      if (room_trace)
        scr_trace ("Library: hit move restriction\n");

      /* Get restriction type. */
      vt_key[4].string = "Var3";
      type = prop_get_integer (bundle, "I<-sisis", vt_key);
      switch (type)
        {
        case 0:                /* Task type restriction */
          {
            scr_int check;

            /* Get the expected completion state. */
            vt_key[4].string = "Var2";
            check = prop_get_integer (bundle, "I<-sisis", vt_key);

            if (room_trace)
              {
                scr_trace ("Library: task %ld, check %ld\n",
                          restriction, check);
              }

            /* Restrict if task isn't done/not done as expected. */
            if (gs_task_valid (game, restriction)
                && (check != 0) == gs_task_done (game, restriction))
              is_restricted = TRUE;
            break;
          }

        case 1:                /* Object state restriction */
          {
            scr_int object, check, openable;

            /* Get the target object; with none, nothing restricts. */
            object = obj_stateful_object (game, restriction);
            if (object < 0)
              break;

            /* Get the expected object state. */
            vt_key[4].string = "Var2";
            check = prop_get_integer (bundle, "I<-sisis", vt_key);

            if (room_trace)
              scr_trace ("Library: object %ld, check %ld\n", object, check);

            /* Check openable and lockable objects. */
            vt_key[0].string = "Objects";
            vt_key[1].integer = object;
            vt_key[2].string = "Openable";
            openable = prop_get_integer (bundle, "I<-sis", vt_key);
            if (openable > 0)
              {
                scr_int lockable;

                /* See if lockable. */
                vt_key[2].string = "Key";
                lockable = prop_get_integer (bundle, "I<-sis", vt_key);
                if (lockable >= 0)
                  {
                    /* Lockable. */
                    if (check <= 2)
                      {
                        if (gs_object_openness (game, object) != check + 5)
                          is_restricted = TRUE;
                      }
                    else
                      {
                        if (gs_object_state (game, object) != check - 2)
                          is_restricted = TRUE;
                      }
                  }
                else
                  {
                    /* Not lockable, though openable. */
                    if (check <= 1)
                      {
                        if (gs_object_openness (game, object) != check + 5)
                          is_restricted = TRUE;
                      }
                    else
                      {
                        if (gs_object_state (game, object) != check - 1)
                          is_restricted = TRUE;
                      }
                  }
              }
            else
              {
                /* Not openable. */
                if (gs_object_state (game, object) != check + 1)
                  is_restricted = TRUE;
              }
            break;
          }
        }
    }

  /* Return TRUE if not restricted. */
  return !is_restricted;
}


/*
 * room_debug_trace()
 *
 * Set room tracing on/off.  Called from lib_debug_trace().
 */
void
room_debug_trace (scr_bool flag)
{
  room_trace = flag;
}

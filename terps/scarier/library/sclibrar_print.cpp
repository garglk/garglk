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
 * Room alternates and room names, object and NPC name printing,
 * version/perspective helpers, response selection and list printing.
 *
 * See sclibrar.h for what the library files share.
 */

#include "sclibrar.h"


/* Trace flag, set before running. */
scr_bool lib_trace = FALSE;


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
      if (lib_trace)
        scr_trace ("Library: room group %ld is empty, no destination\n",
                  roomgroup);
      return -1;
    }

  /* Pick a room at random and return it. */
  vt_key[3].integer = scr_randomint (0, count - 1);
  room = prop_get_integer (bundle, "I<-sisi", vt_key);

  if (lib_trace)
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
       * predicate (run400 4579C1/4579EB call Proc_21_46 @44615C), so
       * an object inside or on something the player carries or wears
       * counts as held.  SCARE tested the object's own position only.
       * Measured in run400 on The X-Files: A New Beginning, whose
       * Parking Garage B (room 3) carries a Var2 = 1, Var3 = 2 alt on
       * the gun, which lives inside the worn holster: the Runner
       * prints "It may be unwise to pull a gun on this guy." on every
       * visit, where SCARE printed the unconditioned alt instead.
       *
       * That is 4.0 only.  The pre-4.0 Runners test the object's own
       * position field, exactly as SCARE did: run390 isdark (433920,
       * the ladder at 433734-4337C6 transcribed below) reads
       * objects(ns(Obj)).global_22 raw -- `<> 0` for "isn't holding",
       * `= 0` for "is holding" -- and never walks a container, and
       * run380 43C708 / run370 434E95 are the same code.  Measured in
       * run390 on Govard (runner_transcripts/govard.txt): the Ruins' alt
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
 * is the ladder at 44B888-44BA5A, over the room record's own fields -- 102 is
 * HideObjects, 100 the object, 104 the condition type -- with the same six
 * cases lib_use_room_alt() already implements for a type-2 alt:
 *
 *     If room(102) = 1 Then
 *       var_BC = 0
 *       If room(100) <> 0 Then
 *         Select Case room(104)
 *           Case 0: If obj.parent <> 0     Then var_BC = 1   ' isn't holding
 *           Case 1: If obj.parent  = 0     Then var_BC = 1   ' is holding
 *           Case 2: If obj.parent <> &H9C  Then var_BC = 1   ' isn't wearing
 *           Case 3: If obj.parent  = &H9C  Then var_BC = 1   ' is wearing
 *           Case 4: If obj.parent > 0 And obj.parent <> playerroom Then ...
 *           Case 5: If obj.parent = 0 Or &H9C Or playerroom      Then ...
 *
 * run380 (43C708, room fields 78/76/80, flag var_F0) and run370 (434E95, the
 * same three fields, the same six arms) are character-for-character the same
 * computation, so this is not a 3.9 invention.  run400 kept the shape but
 * never assigns its flag: Proc_19_87_471F94's var_AC is dead, which is why
 * 4.0 answers a dark room's `x` with its ordinary tail.  Hence the gate here
 * is < 4.0, not >= 3.90.
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
 *   lib_room_is_dark()      is what examines() computes into var_BC and what
 *     viewroom acts on -- the condition AND HideObjects.  Every message site
 *     below wants this one.
 *
 * Measured: p39DARK.taf (3.90) under run390,
 * runner_probes/dark.run390.unseen.txt and runner_probes/dark.run390.seen.txt
 * -- a Dark Cave whose alt fires while the torch is not held, "Hide objects"
 * ticked.  Also ALEXIS.TAF (3.90), runner_probes/alexis_worn_cube.run390.txt,
 * the Passage in the Caves of eternal night: "It is too dark to see."
 * and no object list, with `x large stone table` and `x holes in the wall`
 * both "You can't see that very clearly." where a lit room would have said
 * "Nothing special."
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
           * emptiness test at all (472102 method 0 assigns M1 over the
           * buffer; 472127 method 1 resets it to the room's Long and then
           * appends M1 only if both are non-empty), so a later matching
           * method-0/1 alt discards everything appended before it even when
           * its own text is blank.  The pre-4.0 Runners reach the same place
           * by a different route: their viewroom picks the task alt on
           * task-doneness alone (run370 @43318C, run390 @447648 loc_447670)
           * and then skips the base/LastDesc branch entirely.  Only the
           * non-matching branch is guarded, on M2, below.
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
 * lib_print_room_name()
 *
 * Get/print out the name for a given room.
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
   * run400's room lister resets the name to the room's Short (472058) and
   * then, inside its one forwards loop over EVERY alt, lets each matching
   * alt with a non-empty Changed overwrite it (472244-472254) -- whatever
   * that alt's display method, and whether or not a later method-0/1 alt
   * restarts the description.  togetyou (4.00) `infect cut`: The Ear's alt 0
   * (task 22, method 2, Changed "The Infected Ear") sits below alt 1
   * (method 1, M2 shown), so the Runner heads the room "The Infected Ear"
   * (runner_transcripts/togetyou.txt T16) where the start-point walk below
   * never reached alt 0 and kept "The Ear".
   *
   * Pre-4.0 keeps the start-point walk, and it can never be seen to differ:
   * no Runner below 4.0 can change a room's name at all.  "Changed" is a
   * field of the 4.0 ROOM_ALT record only (sctafpar.cpp's grammar), so the
   * alts this loader synthesises for a 3.7/3.8/3.9 game are all written with
   * Changed "" (parse_fixup_v390_v380_room_alt), and the Runners agree:
   * run390's viewroom heads the room with `"<b>" & rooms(broom).Short &
   * "</b>"` and consults no alt (447741-447754), while run380's viewroom
   * (439C08) reaches the alt array only for description text
   * (var_A4(60).n(n), 43952F/4395EC/4396A9).  So the walk below decides the
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
 * lib_print_room_name_lower()
 *
 * Print a room's name folded to lower case.  The Runner's where/find/locate
 * handlers are the only place that does this -- they build the answer as
 * `... & LCase(room.name) & "."` -- so it gets its own small helper rather
 * than a flag on lib_print_room_name().
 *      run380 @4374E1 (objects) and @440BDF (characters)
 *      run400 @468143 (objects) and @47FD19 (characters)
 */
void
lib_print_room_name_lower (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::string name (lib_get_room_name (game, room));

  pf_buffer_string (filter, scr_lowercase (name).c_str ());
}


void
lib_print_room_name (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *name;

  /*
   * Open a paragraph for the name.  The runner has no inline room name, so
   * anything already buffered ran straight into the room description there;
   * here it would run straight into a heading instead.  See
   * pf_buffer_paragraph_break().
   */
  pf_buffer_paragraph_break (filter);

  /* Print the room name, possibly in bold. */
  name = lib_get_room_name (game, room);
  if (game->bold_room_names)
    {
      pf_buffer_tag (filter, SCR_TAG_BOLD);
      pf_buffer_string (filter, name);
      pf_buffer_tag (filter, SCR_TAG_ENDBOLD);
    }
  else
    pf_buffer_string (filter, name);

  /*
   * The heading's newline is one the Runner has too, so it is not a section
   * terminator of ours for pf_buffer_join() to pop.  Left unmarked, a room
   * whose description is empty ran its contents straight on after the name:
   *
   *     Inside the Top Hat  A bunny twitches its whiskers at me  <- Scarier
   *     Inside the Top Hat                                       <- run400
   *     A bunny twitches its whiskers at me
   *
   * tophat.taf (4.00), whose one room has an empty Long, in both `up` turns
   * of its transcript; professor t11/t18/t38/t54, viewtohome t20/t56 and
   * woof t8 say the same.  Read with the leading break that
   * pf_buffer_paragraph_break() supplies just above, the measurements fit a
   * Runner that appends a break, the name and a break onto the one output
   * string the turn is building -- pspace() only ever appends a separator,
   * so it leaves that trailing Chr(10) standing and never takes it back.
   * The P-code has not been read; the transcripts are the evidence.
   */
  pf_buffer_character (filter, '\n');
  pf_buffer_hard_break (filter);
}


/*
 * lib_definite_object_name()
 * lib_print_object_np
 * lib_print_object
 *
 * Convenience functions to print out an object's name, with a "normalized"
 * prefix -- any "a"/"an"/"some" is replaced by "the" -- and with the full
 * prefix.  lib_definite_object_name builds the normalized form as a string,
 * for %theobject% too (scvars.cpp): run400 expands that marker through the
 * same name builder in the same normalizing mode, Proc_21_31_448710 mode 0
 * called from Proc_21_15_4619CC at 461540.
 *
 * Pre-3.9 normalizes far less, and the branch below spells out exactly what
 * Form1.tense() does instead.  "some" is the case that was measured first:
 * it joined the normalized list in 3.9, and 3.7 and 3.8 leave a "some" prefix
 * exactly as the author wrote it.  Measured live on one game carried across
 * three .taf versions by the ADRIFT generators, so the data is provably
 * identical in all three: microwaveman.taf (3.80) upconverted with gen390 and
 * gen400, `take clothes` on an object whose Prefix is "some" -- run380 "You
 * pick up some aluminum clothes.", run390 "You pick up the aluminum clothes.",
 * run400 "You take the aluminum clothes."  3.70 answers like 3.80: a copy of
 * arlo.taf with the bone's "a" prefix rewritten to "some" gives "You pick up
 * some bone." in run370.  See test/adrift4/notes/RUNNER_TESTS_TODO.md section
 * 4.
 */
/*
 * lib_compare_article()
 *
 * scr_compare_word() for the article normalizer, but case-SENSITIVE.
 *
 * Form1.tense() is VB6 string equality and Left$() comparison under the
 * default Option Compare Binary, so an author who capitalised the article
 * gets it back untouched -- exactly as an authored "The" already does, there
 * being no "the" test at all.  Measured 2026-09-07 on probe PFX (run400,
 * runner_probes/pfx.run400.tense.txt), eleven objects one per spelling:
 *
 *     take alpha    (Prefix "a")      Player take the alpha.
 *     take bravo    (Prefix "A")      Player take A bravo.
 *     take charlie  (Prefix "an")     Player take the charlie.
 *     take delta    (Prefix "An")     Player take An delta.
 *     take echo     (Prefix "some")   Player take the echo.
 *     take foxtrot  (Prefix "Some")   Player take Some foxtrot.
 *     take golf     (Prefix "a big")  Player take the big golf.
 *     take hotel    (Prefix "A big")  Player take A big hotel.
 *     take india    (Prefix "SOME")   Player take SOME india.
 *     take juliet   (Prefix "the")    Player take the juliet.
 *
 * The same run's `i` and room listing print every prefix verbatim, so only
 * the definite form was ever folding case.  The live game that turned this
 * up is The X-Files: A New Beginning, whose Stool carries the Prefix "A":
 * run400 answers `u` from it with "(Getting off A Stool first)".
 *
 * The pre-3.9 branch below already compares with strcmp/strncmp, and run370
 * and run380 are the same VB, so this makes all four Runners agree.
 *
 * Scarier deliberately does not follow the Runner here (deviation policy):
 * an author's capital article mid-sentence ("You drop A Bow of Icy Arrows.",
 * "(Getting off A Stool first)") is a display accident, so the comparison
 * folds case and every spelling becomes "the".
 */
static scr_bool
lib_compare_article (const scr_char *string, const scr_char *word,
                     scr_int length)
{
  assert (string && word);

  return scr_compare_word (string, word, length);
}

/*
 * The Runner's own case-sensitive test, for the one caller that builds a
 * command line rather than text: the battle "with" prefix re-parsed on the
 * next turn (lib_battle_definite_name), which keeps the measured spelling.
 */
scr_bool
lib_compare_article_binary (const scr_char *string, const scr_char *word,
                            scr_int length)
{
  assert (string && word);

  return strncmp (string, word, length) == 0
         && (string[length] == NUL || scr_isspace (string[length]));
}


/*
 * lib_definite_prefix()
 *
 * Write the article lib_definite_object_name() opens with to RESULT, and
 * return what is left of PREFIX to follow it.  The 4.0 name builder's own
 * rule, for command lines rather than printed names, is
 * sclibrar_dispatch.cpp's lib_definite_prefix_400().
 */
static const scr_char *
lib_definite_prefix (const scr_char *prefix, scr_bool is_pre_390,
                     std::string &result)
{
  const scr_char *normalized;

  /*
   * Normalize by skipping any leading "a"/"an"/"some", replacing it instead
   * with "the", and skipping any odd "the" already present.  If no prefix at
   * all, add a "the " anyway.
   *
   * This is empirical, based on observed Adrift Runner behavior.  The empty
   * prefix case was the last guess left in it, and it is measured now:
   * run400.exe playing La hija del relojero (whose Phoenix has no prefix)
   * answers "coger fenix" with "You take the Fenix de laton de el cajon.",
   * i.e. "the " is right.  lib_print_object below defaults the same empty
   * prefix to "a ", and that is right too -- the same run prints "Encontre un
   * Fenix de laton dentro del cajon." on the container-reveal path.  See the
   * divergence table in test/adrift4/notes/RUNNER_TESTS_TODO.md, settled
   * 2026-08-14.
   */
  normalized = prefix;

  /*
   * Pre-3.9 is Form1.tense(), and it is a much smaller thing: three
   * literal tests and no default (run370 @420F28, run380 @425FA8, the two
   * byte-identical).  An exact "a" becomes "the"; a prefix opening "a " or
   * "an " has that much replaced by "the "; anything else -- a bare "an",
   * a "some" -- comes back untouched.  So run370 answers `take implement
   * of destruction` with "You pick up an implement of destruction." where
   * 3.9 and 4.0 say "the".  Measured live under Wine 2026-08-24 on
   * arlo.taf.
   *
   * tense() has no empty-prefix branch either, but pre-3.9 still comes
   * out with "the", because the empty prefix never reaches it: the .taf
   * loader rewrites it on the way in.  run380 @4481B2 reads the Prefix
   * line and, `If (var_3C8(0) = vbNullString) Then var_3C8(0) = "a"`,
   * substitutes a literal "a" (run370 @43F5DA does the same).  So pre-3.9
   * an empty prefix simply *is* an "a" prefix, and both of scarier's
   * existing defaults -- "the " here, "a " in lib_print_object below --
   * already fall out of it correctly.  4.0 turns out to do it too, at
   * loc_4900EC in its own object loader (mdlSpreadTheLoad.bas:7599), so
   * the substitution is not a pre-3.9 quirk at all; it is only that the
   * defaults here make it invisible.
   *
   * Measured live under Wine 2026-08-24 on mikes.taf (3.80) in run380,
   * whose dresser, toilet, poop and vans all carry an empty Prefix
   * (deobfuscated straight out of the .taf, so this is not inference):
   * `take vans` answers "You pick up the pair of vans.", `take poop`
   * "You take a poop from the toilet.", `take all from dresser` "You
   * take a socks, a shirt, a underwear and a pair of pants from the
   * dresser."  Note "a underwear" -- the substituted article is never
   * inflected to "an", exactly as lib_print_object's own default isn't.
   *
   * Scarier deliberately normalizes a bare "an" (and the "the"/capital
   * spellings below) in every version all the same (deviation policy):
   * "You pick up an implement of destruction." names an object already
   * in hand as if it were a new one.  Only the grammatical "some" keeps
   * its pre-3.9 spelling.
   */
  if (lib_compare_article (prefix, "a", 1))
    {
      normalized = prefix + 1;
      result += "the";
    }
  else if (scr_compare_word (prefix, "an", 2))
    {
      normalized = prefix + 2;
      result += "the";
    }
  /*
   * No "the" branch.  The Runner's normalizer only ever rewrites the three
   * indefinite articles, so a prefix the author wrote as "The" comes back
   * "The", capital and all.  run400's tense (Proc_21_13_44F474 @44F474, called
   * from the name builder Proc_21_31_448710 in its normalizing mode 0) tests
   * exactly six things -- the whole string against "a", "an" and "some", and
   * its head against "a ", "an " and "some " -- and returns its argument
   * untouched otherwise; pre-3.9's tense is the same shape with the two "some"
   * tests missing (see the branch above).  Measured live: run400 playing The
   * X-Files: A New Beginning, whose Memo carries the Prefix "The", answers
   * `take all from desk` with "... Your Badge and The Memo from Your Desk.",
   * `x desk` with "Your Coffee Mug and The Memo are on Your Desk" and `burn
   * memo` with "I don't understand what you want me to do with The Memo."
   * (runner_probes/xfiles.run400.txt, 2026-08-25).  Falling through leaves the
   * prefix in "normalized", which the tail below prints verbatim, so all this
   * branch ever did was lower-case the author's capital.
   *
   * 3.9 is bracketed rather than read: the run390 decompilation does not
   * reach its normalizer.  Both neighbours leave "the" alone, and the only
   * thing 3.9 is known to have added to tense is the "some" pair, so the
   * 3.90 games in the corpus (The Spirit's Flight, whose Spirit Dagger and
   * Orb of Storms both carry a "The" prefix) follow 4.0 here.
   *
   * Scarier deliberately keeps a "the" branch anyway (deviation policy): "You
   * take The Grim Reaper's Scythe." is a capital mid-sentence and nothing
   * more, so an authored "The" is folded like the pre-b607dd8 code did.
   */
  else if (lib_compare_article (prefix, "the", 3))
    {
      normalized = prefix + 3;
      result += "the";
    }
  else if (!is_pre_390 && lib_compare_article (prefix, "some", 4))
    {
      normalized = prefix + 4;
      result += "the";
    }
  else if (scr_strempty (prefix))
    {
      normalized = "";
      result += "the";
    }

  return normalized;
}

/*
 * lib_short_without_article()
 *
 * An object's Short name, past any leading article that has more of the
 * name after it.
 */
static const scr_char *
lib_short_without_article (const scr_char *name)
{
  /*
   * Print the object's name, verbatim.  Inherited SCARE looked for a leading
   * article here and stripped it, on the grounds that "some games may avoid
   * prefix and do this instead".  No Runner can do that, and the whole path
   * is readable end to end:
   *
   * - The object loader normalizes the two fields as it reads them.  run400
   *   at loc_4900E3 (mdlSpreadTheLoad.bas:7594) LineInputs the Prefix,
   *   substitutes a literal "a" for an empty one (loc_4900EC), then loops
   *   stripping trailing spaces (loc_490100..loc_49015C); it then LineInputs
   *   the Short at loc_49016C and loops stripping *leading* spaces
   *   (loc_490170..loc_4901CC), and moves straight on to the alias count.
   *   Nothing looks at an article.  run370 @43F5DA and run380 @4481B2 make
   *   the same "a" substitution.
   * - run400 builds every object name in one place, Proc_21_31_448710
   *   (General.bas:7041, 232 call sites), as the single string
   *   `Prefix & " " & Short` (loc_4486B7..loc_4486CF).
   * - It hands that to tense, Proc_21_13_44F474 (General.bas:1728), which
   *   tests exactly six things and nothing else: the whole string against
   *   "a", "an" and "some", and its first 2/3/5 characters against "a ",
   *   "an " and "some ".  Callers tense the result a second time (e.g.
   *   Battles.bas:261 then :265), which changes nothing.
   *
   * So the only characters ever inspected are at the head of the
   * concatenation, and after the loader's substitution the head is always the
   * prefix.  An object with Prefix "The" and Short "the Memo" comes out of
   * run400 as "The the Memo"; one with no prefix at all is "a Fenix de
   * laton" going in and "the Fenix de laton" coming out, which is what run400
   * playing La hija del relojero answers to "coger fenix" and what the two
   * defaults in this file and in lib_print_object below already reproduce.
   * Pre-3.9's tense is the same shape with the two "some" tests missing (see
   * the branch above), so this holds in all four Runners.
   *
   * The loader's whitespace trims -- trailing spaces off Prefix, leading ones
   * off Short -- are modelled too, in parse_trim_object_names().
   *
   * Scarier deliberately strips a leading article off the Short all the same
   * (deviation policy), as SCARE did: "the The horn of the angels" and "The
   * the Memo" are doubled articles, nothing more.  Only an article followed
   * by more of the name goes, with the spaces after it, so the old "the  horn"
   * double space does not come back either.
   */
  static const struct { const scr_char *word; scr_int length; } articles[] = {
    {"a", 1}, {"an", 2}, {"the", 3}, {"some", 4}
  };
  size_t index_;

  for (index_ = 0; index_ < sizeof (articles) / sizeof (articles[0]);
       index_++)
    {
      if (scr_compare_word (name, articles[index_].word,
                            articles[index_].length)
          && !scr_strempty (name + articles[index_].length))
        {
          name += articles[index_].length;
          while (scr_isspace (*name))
            name++;
          break;
        }
    }

  return name;
}

std::string
lib_definite_object_name (scr_prop_setref_t bundle, scr_int object)
{
  std::string result;
  scr_vartype_t vt_key[3], vt_version[1];
  const scr_char *prefix, *normalized, *name;
  scr_bool is_pre_390;

  vt_version[0].string = "Version";

  /* Get the object's prefix. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

  is_pre_390 = prop_get_integer (bundle, "I<-s", vt_version) < TAF_VERSION_390;
  normalized = lib_definite_prefix (prefix, is_pre_390, result);

  /*
   * Print what the normalizer left of the prefix, and a separating space.
   * Both are unconditional: the Runner's name builder is a plain
   * `tense(Prefix) & " " & Short` concatenation, so a prefix it hands back
   * empty still costs a space.  That case is only reachable at all for an
   * authored whitespace-only prefix, which the loader trims to nothing after
   * declining to substitute an "a" into it -- probe ISARE's single-space
   * rings answers `where rings` with " rings are test arena.", leading space
   * and no article (run400, runner_probes/isare.run400.txt, 2026-09-07).
   * Scarier deliberately gives that empty prefix a "the" instead (deviation
   * policy).
   */
  result += normalized;
  result += ' ';

  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  name = lib_short_without_article (name);
  result += name;
  return result;
}

void
lib_print_object_np (scr_gameref_t game, scr_int object)
{
  pf_buffer_string (gs_get_filter (game),
                    lib_definite_object_name (gs_get_bundle (game),
                                              object).c_str ());
}

void
lib_print_object (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix, *name;

  /*
   * Get the object's prefix and print it, then a space; both unconditionally,
   * the Runner's builder being a plain concatenation.  An object authored
   * with no prefix at all arrives here carrying the literal "a" the loader
   * substituted into it (see parse_trim_object_names in sctafpar.cpp), never
   * inflected to "an" before a vowel: run380 on the pwearv probe, four
   * wearables with empty prefixes and vowel-initial names, answers "You put
   * on a apple." and lists them back as "You are wearing a apple, a orange, a
   * egg and a umbrella" (2026-08-23).
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, prefix);
  pf_buffer_character (filter, ' ');

  /* Print object name. */
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, name);
}


/*
 * lib_print_object_raw()
 *
 * Print an object as the pre-3.9 "remove" handler does: the authored prefix,
 * a space, and the name, with no normalizing and no default for an empty
 * prefix.  run370 @42980D and run380 @42FF38 both build the message by plain
 * concatenation, `... & " remove " & ob(0) & " " & ob(4) & "."`, where every
 * other lister in those Runners passes the prefix through tense() first
 * (run370 drop @430A50, run380 drop @438C3B).  3.9 unified the two: run390
 * builds both its drop @00045B93 and its remove @00039E15 out of the same
 * General.Sub_3_45 helper, so from 3.9 on "remove" normalizes like the rest.
 * Measured live under Wine 2026-08-24, akron.taf (3.80) in run380: "You remove
 * a grubby sweatshirt." and "You remove a pair of ragged jeans." where the
 * same game in Scarier said "the".
 *
 * Scarier deliberately keeps "the" in the success reports all the same
 * (deviation policy) -- an indefinite article on the object just removed,
 * worn or put away reads as if it were some other one -- so this printer is
 * left to the refusals ("You are not wearing a hat!"), where it reads fine.
 */
void
lib_print_object_raw (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;

  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

  /*
   * "No default for an empty prefix" is right about the message builder and
   * wrong about what reaches it: the pre-3.9 loaders rewrite an empty Prefix
   * to a literal "a" before any of this (run370 @43F5DA, run380 @4481B2, and
   * run400 at loc_4900EC too), so the concatenation never sees one.  That
   * rewrite is modelled in parse_trim_object_names now, so nothing is needed
   * here.
   */
  pf_buffer_string (filter, prefix);
  pf_buffer_character (filter, ' ');

  vt_key[2].string = "Short";
  pf_buffer_string (filter, prop_get_string (bundle, "S<-sis", vt_key));
}


/*
 * lib_print_npc_np
 *
 * Convenience function to print out an NPC's name, without any prefix.
 */
void
lib_print_npc_np (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* Get the NPC's short description, and print it. */
  pf_buffer_string (filter,
                    prop_get_indexed_string (bundle, "NPCs", npc, "Name"));
}


/*
 * lib_get_perspective()
 *
 * Return Globals/Perspective as the Runner of this game's version reads it.
 *
 * ADRIFT gained the third person in 4.0.  The pre-4.0 Runners know two
 * perspectives only, and treat every value that is not LIB_FIRST_PERSON as the
 * second person -- measured live in run390 on the 3.9 Where probe with
 * Perspective set to 1, 2 and 3 in turn: all three answer "You can't do that
 * here!" and "You are carrying nothing.", and only 0 answers "I".  The same
 * split showed up on the 3.9 capacity probe, which run390 narrates "You pick up
 * the a1." / "You put the b1 inside the c52t." where run400 renders the very
 * same Perspective 2 in the third person ("Player put the d2 inside the
 * c52t.").  SCARE read the field the 4.0 way in every version, so a pre-4.0
 * game authored with Perspective 2 came out in the third person here and in the
 * second person in its own Runner.
 *
 * Clamping here rather than at parse time keeps the authored value intact for
 * the dumps (scdump.cpp's GAME line prints it raw) and confines the rule to the
 * two places that render a person.  Out-of-range values fall through unclamped
 * for 4.0, where lib_select_response() still reports them as an error.
 *
 * Exported because the battle narration renders a person too, and reads the
 * same global through the same clamp; see battle_print_combatant().
 */
scr_int
lib_get_perspective (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int perspective;

  perspective = prop_get_global_integer (bundle, "Perspective");

  if (prop_get_taf_version (bundle) < TAF_VERSION_400
      && perspective != LIB_FIRST_PERSON)
    return LIB_SECOND_PERSON;

  return perspective;
}


/*
 * lib_is_version_400()
 *
 * TRUE if this game came from a 4.0 .taf, FALSE for 3.7/3.8/3.9.  Several
 * library messages were reworded in 4.0 and the Runners never share them
 * across versions.
 */
scr_bool
lib_is_version_400 (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400;
}


/*
 * lib_is_version_390()
 *
 * TRUE for a 3.9 .taf only.  run390's not-a-turn flag MemVar_468219 is
 * cleared at the top of generaltasks (45EC74) and set again only by status
 * (44C52B), history/past (45F52F), score (45F6C7), count/num (45F7D6),
 * about/info/author/information (45FB3D), quit/bye/end (45FB6E) and turns
 * (45FD4D).  Everything else it answers is an ordinary turn that ticks
 * characters() and events() -- hint, help, clear/cls, time, version, save,
 * restore and undo among them.  Measured on p39ADMIN.taf, whose length-1
 * event prints TICK. after each of those (runner_probes/admin.run390.txt).
 * 4.0 has its own, longer, list.
 */
scr_bool
lib_is_version_390 (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));

  return version >= TAF_VERSION_390 && version < TAF_VERSION_400;
}


/*
 * lib_set_admin()
 *
 * Mark a built-in meta-command as an administrative turn -- but only for
 * 3.90/4.00 games.  Administrative turns exist only from 3.90: run390
 * generaltasks (460D6C) sets its flag 468219 for history/score/count/
 * information/end/turns, and its end-of-turn characters()+events() at
 * 460675/46067A is guarded by that flag.  run380 generaltasks (44349C) and
 * run370 (43B5xx) have no such flag: their tail (run380 443160-44317E,
 * run370 43C88D-43C8AB) ticks characters()+events() after EVERY command
 * that left output, unless the game has ended; only opensave() (save/
 * restore/restart, GoTo 443326) and quit (unloads the form) bypass it.
 * Measured 2026-09-04, run380 haunt.taf: the `score` at turn 83 is followed
 * by the Weather event's finish text and the grandfather-clock line, which
 * Scarier had suppressed as an administrative turn.  The 3.8 turn counter
 * (44F138, 441A21) likewise increments on every command, so the pre-3.90
 * non-admin path is right on both counts.  Callers are the meta-commands
 * 3.8 recognises and answers (score, turns, count, hint, help, about,
 * clear, history, where); verbs 3.8 does not know at all keep their 4.0
 * behaviour.
 */
void
lib_set_admin (scr_gameref_t game)
{
  game->is_admin = prop_get_taf_version (gs_get_bundle (game))
                   >= TAF_VERSION_390;
}


/*
 * lib_matcher_requires_seen()
 *
 * TRUE if the parser's object matcher should reject objects the player
 * hasn't seen yet.  The 3.9 and 4.0 Runners both gate every name match on
 * the object's seen flag (run390 co() tests the flag alongside obhere();
 * run400's matcher requires it in every match mode), so an object inside
 * an unlisted container "doesn't exist" until a room description, contents
 * listing, or examination reveals it.  The 3.8 Runner's co()/obhere() have
 * no such test.
 *
 * Pinned in the decompiles 2026-08-24.  The flag is at a different struct
 * offset in each Runner -- 40 in run370 and run380, 44 in run390, 48 in
 * run400 -- and co() grew from 50 lines (run370 4261B4) and 75 (run380
 * 42DE60) to 408 (run390 43B6BC) and 530 (run400 46486C).  Only the last
 * two read it: run390 at 43B2FB/43B4AE/43B677, run400 at
 * 464372/464693/4647DD/46480F, in each case after obhere() has failed.
 * Before 3.9 exactly two routines consult the flag, and neither is the
 * matcher: whereis() (run370 430320 @42FC06, run380 437954 @43712E), and
 * therest()'s "With what?" check on the second noun of <verb> X with Y
 * (run370 43EAA8 @43CFBE, run380 4455B4 @443AB6).
 */
scr_bool
lib_matcher_requires_seen (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390;
}


/*
 * lib_get_death_message()
 *
 * The sentence a Runner prints when the player dies, either from an EndGame
 * task action or from losing a Battle System fight.
 *
 * The pre-4.0 Runners build it around the perspective, "I'm afraid " & Ary(5)
 * & " " & Ary(4) & " dead!"; 4.0 replaced the whole thing with one fixed
 * literal and no longer varies it.  In run390 that assembly appears twice, in
 * the EndGame printer at 0003F5C8 and again in Form1.chardohit at 00042B14, so
 * the battle death is worded the same way as the task death.  run400 holds
 * "I'm afraid you are dead!" as a single literal in General.Sub_22_70
 * (000523DC), and Battles.Sub_12_1 calls that same sub (0004AE95), so 4.0 says
 * "you are" even in the first person.  A UTF-16LE census agrees: the assembled
 * pair is in all four Runners, the finished sentence only in run400.
 *
 * Measured live on the same EndGame with Perspective flipped: run370 on
 * castle.taf (its "south" death task rewired to a trivially runnable "zdie")
 * and run380 on wrecked.taf (task 42 rewired the same way) and run390 on the
 * synthetic p39end probe all answer "I'm afraid I am dead!" at Perspective 0
 * and "I'm afraid you are dead!" at Perspective 1.  Perspective 2 answers like
 * 1 pre-4.0, which lib_get_perspective() already folds in.
 *
 * The status line stays "You are dead!" throughout -- that is a separate fixed
 * literal, present unchanged in all four Runners.  See
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 4.
 */
const scr_char *
lib_get_death_message (scr_gameref_t game)
{
  if (!lib_is_version_400 (game)
      && lib_get_perspective (game) == LIB_FIRST_PERSON)
    return "I'm afraid I am dead!";

  return "I'm afraid you are dead!";
}


/*
 * lib_select_response()
 * lib_select_plurality()
 *
 * Convenience functions for multiple handlers.  Returns the appropriate
 * response string for a game, based on perspective or object plurality.
 *
 * The third-person strings are NOT the second-person ones conjugated.  Every
 * Runner keeps one literal per message and prefixes it with a slot from a
 * seven-element pronoun array, filled at run400 48F60C-48F798: for the third
 * person Ary(0) and Ary(2) are the player's name, Ary(1) and Ary(3) the name
 * plus "'s", Ary(4) "is", Ary(5) "he" or "she" by Globals/PlayerGender, and
 * Ary(6) "s".  The verb lives in the literal, in its second-person form, and
 * only ONE message in the whole library appends Ary(6): the player's own
 * movement, " move" & Ary(6) & " north." at 474C3C and its eleven siblings.
 * So 4.0 really does print "%player% take the acorn." and "%player% sing a
 * little song.", and games work around it -- herrdoktor.taf ships the ALRs
 * "[The good doctor take] -> [The good doctor takes]" and "[doctor eat] ->
 * [doctor eats]", which turned SCARE's already-conjugated "takes" into
 * "takess" (2026-09-05).  Measured live against run400 on Main Course.taf
 * (Perspective 2, no ALRs to confound it): listen, sing, dance, sit down,
 * stand up and lie down all come back bare.
 *
 * Where a message reads Ary(5) rather than Ary(0) the third-person string
 * carries the internal %player_pronoun% token instead (see scvars.cpp);
 * pre-4.0 never reaches either, because lib_get_perspective() clamps.
 *
 * Scarier deliberately does not print the Runner's "%player% take the acorn."
 * (deviation policy: a subject-verb disagreement is a display accident).  The
 * source keeps the Runner's bare literals, and lib_select_response() hands
 * them to lib_conjugate_third_person(), which conjugates the verb -- except
 * where the game ships an ALR that already rewrites the bare verb, as
 * herrdoktor, iqsfot and Albert is Lost do ("Irvine open" -> "Irvine
 * uncloses"): there the bare text is what the author's ALR expects, and
 * conjugating it first gave SCARE's old "takess" and "unclosess".
 */

/*
 * lib_conjugate_verb()
 *
 * The third person singular of a bare second-person verb word, or an empty
 * string when the word is not one to conjugate (a modal, or already third
 * person).
 */
static std::string
lib_conjugate_verb (const std::string &word)
{
  static const scr_char *const KEEP[] = {
    "is", "was", "has", "does", "can", "can't", "cannot", "could", "couldn't",
    "must", "might", "may", "will", "won't", "would", "wouldn't", "should",
    "shouldn't", "isn't", "wasn't", "hasn't", "doesn't", "already", "not",
    nullptr
  };
  static const scr_char *const IRREGULAR[][2] = {
    {"don't", "doesn't"}, {"haven't", "hasn't"}, {"aren't", "isn't"},
    {"are", "is"}, {"have", "has"}, {"do", "does"}, {"go", "goes"},
    {nullptr, nullptr}
  };
  scr_int index_;
  const size_t length = word.length ();

  if (length == 0)
    return std::string ();
  for (index_ = 0; KEEP[index_]; index_++)
    {
      if (word == KEEP[index_])
        return std::string ();
    }
  for (index_ = 0; IRREGULAR[index_][0]; index_++)
    {
      if (word == IRREGULAR[index_][0])
        return IRREGULAR[index_][1];
    }
  if (word[length - 1] == 's' || word.find ('\'') != std::string::npos)
    return std::string ();

  if (word[length - 1] == 'x' || word[length - 1] == 'z'
      || word[length - 1] == 'o'
      || (length > 1 && (word.compare (length - 2, 2, "sh") == 0
                         || word.compare (length - 2, 2, "ch") == 0)))
    return word + "es";
  if (length > 1 && word[length - 1] == 'y'
      && !strchr ("aeiou", word[length - 2]))
    return word.substr (0, length - 1) + "ies";
  return word + "s";
}

/*
 * lib_conjugate_third_person()
 *
 * Conjugate the verb after a leading "%player% " (or the "'ve" of a leading
 * "%player%'ve") in a third-person library literal; see the comment above.
 * Returns the literal itself when there is nothing to change.
 */
static const scr_char *
lib_conjugate_third_person (scr_gameref_t game, const scr_char *literal)
{
  static const scr_char PLAYER[] = "%player%";
  enum { RING_SIZE = 16 };
  static std::string ring[RING_SIZE];
  static scr_int ring_next = 0;
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const size_t prefix = sizeof (PLAYER) - 1;
  const scr_char *rest, *playername;
  std::string word, conjugated, bare;
  size_t word_length, name_length;

  if (!literal || strncmp (literal, PLAYER, prefix) != 0)
    return literal;
  rest = literal + prefix;

  if (strncmp (rest, "'ve ", 4) == 0)
    {
      /* "%player%'ve already got " -> "%player% has already got ". */
      word_length = 3;
      conjugated = " has";
    }
  else
    {
      if (rest[0] != ' ')
        return literal;
      /* The verb is one of the library's own English words: ASCII, and
         not the locale's alphabet, is the right test here. */
      for (word_length = 1;
           rest[word_length] && ((rest[word_length] >= 'a'
                                  && rest[word_length] <= 'z')
                                 || rest[word_length] == '\'');
           word_length++)
        ;
      word.assign (rest + 1, word_length - 1);
      conjugated = lib_conjugate_verb (word);
      if (conjugated.empty ())
        return literal;
      conjugated = " " + conjugated;
    }

  /* Leave the bare verb to a game ALR written to rewrite it. */
  playername = prop_get_global_string (bundle, "PlayerName");
  if (scr_strempty (playername))
    playername = "Anonymous";
  name_length = strlen (playername);
  bare = std::string (playername) + rest;
  if (pf_alr_rewrites_span (bundle, bare.c_str (),
                            rest[0] == ' ' ? name_length + 1 : name_length,
                            name_length + word_length))
    return literal;

  ring_next = (ring_next + 1) % RING_SIZE;
  ring[ring_next] = std::string (PLAYER) + conjugated + (rest + word_length);
  return ring[ring_next].c_str ();
}

const scr_char *
lib_select_response (scr_gameref_t game,
                     const scr_char *second_person,
                     const scr_char *first_person,
                     const scr_char *third_person)
{
  scr_int perspective;
  const scr_char *response;

  /* Return the response appropriate for Perspective. */
  perspective = lib_get_perspective (game);
  switch (perspective)
    {
    case LIB_FIRST_PERSON:
      response = first_person;
      break;
    case LIB_SECOND_PERSON:
      response = second_person;
      break;
    case LIB_THIRD_PERSON:
      response = lib_conjugate_third_person (game, third_person);
      break;
    default:
      scr_error ("lib_select_response:"
                " unknown perspective, %ld\n", perspective);
      response = second_person;
      break;
    }

  return response;
}

const scr_char *
lib_select_plurality (scr_gameref_t game, scr_int object,
                      const scr_char *singular, const scr_char *plural)
{
  return obj_appears_plural (game, object) ? plural : singular;
}

/*
 * lib_select_list_plurality()
 *
 * The verb that agrees with a gathered list: plural for two or more
 * objects, else by the one object's own plurality.  The contents listings
 * ("Inside the desk are ...", "On the table are ...") use this where the
 * Runner prints a literal " is " whatever the count; Scarier deliberately
 * keeps the agreement (see test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md,
 * deliberate deviations).
 */
/*
 * lib_alrs_see_list_verb()
 *
 * TRUE if one of the game's ALRs could be matching the Runner's literal
 * " is " in the contents listing of this container: an Original holding
 * "Inside/On [the] <its short name> is" ("Inside the first aid kit is a
 * large bandage roll, ...", deadman; "On <X> is ", dolg), the joined
 * ", and inside is",
 * or a bare " is " (les_feux translates it).  Such a listing keeps the
 * Runner's " is " so the same ALRs fire -- which ALRs fire is game
 * behaviour, not text shape.
 */
static scr_bool
lib_alrs_word_before (const scr_char *text, const scr_char *end,
                      const scr_char *word)
{
  const size_t length = strlen (word);

  return (size_t) (end - text) >= length
         && scr_strncasecmp (end - length, word, length) == 0
         && (end - length == text || !scr_isalpha (end[-(scr_int) length - 1]));
}

static scr_bool
lib_alrs_prefixed (const scr_char *text, const scr_char *end)
{
  const scr_char *word_end;
  scr_int pass;

  /*
   * "Inside/On <name>", allowing up to four words between: an article and
   * the rest of the object's prefix ("Inside the first aid kit is").
   */
  for (pass = 0; pass < 5; pass++)
    {
      while (end > text && end[-1] == ' ')
        end--;
      if (lib_alrs_word_before (text, end, "inside")
          || lib_alrs_word_before (text, end, "on"))
        return TRUE;
      word_end = end;
      while (end > text && end[-1] != ' ')
        end--;
      if (end == word_end)
        break;
    }
  return FALSE;
}

static scr_bool
lib_alrs_text_has (const scr_char *text, const scr_char *first,
                   const scr_char *second, scr_bool prefixed)
{
  const size_t first_length = strlen (first);
  const size_t second_length = strlen (second);
  const scr_char *cursor;

  for (cursor = text; *cursor; cursor++)
    {
      if (scr_strncasecmp (cursor, first, first_length) == 0
          && scr_strncasecmp (cursor + first_length,
                              second, second_length) == 0
          && (!prefixed || lib_alrs_prefixed (text, cursor)))
        return TRUE;
    }
  return FALSE;
}

scr_bool
lib_alrs_see_list_verb (scr_gameref_t game, scr_int container)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int alr_count, alr;
  const scr_char *name;

  vt_key[0].string = "Objects";
  vt_key[1].integer = container;
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  vt_key[0].string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (alr = 0; alr < alr_count; alr++)
    {
      const scr_char *original;

      vt_key[1].integer = alr;
      vt_key[2].string = "Original";
      original = prop_get_string (bundle, "S<-sis", vt_key);
      if ((!scr_strempty (name)
           && lib_alrs_text_has (original, name, " is", TRUE))
          || lib_alrs_text_has (original, "inside", " is", FALSE))
        return TRUE;
      original += strspn (original, " ");
      if (scr_strncasecmp (original, "is", 2) == 0
          && strspn (original + 2, " ") == strlen (original + 2))
        return TRUE;
    }
  return FALSE;
}

const scr_char *
lib_select_list_plurality (scr_gameref_t game, scr_int container,
                           const lib_list_t &list,
                           const scr_char *singular, const scr_char *plural)
{
  if (lib_alrs_see_list_verb (game, container))
    return singular;
  if (list.size () > 1)
    return plural;
  return list.empty () ? singular
                       : lib_select_plurality (game, list[0], singular, plural);
}


/*
 * lib_print_wrapped_object()
 * lib_print_wrapped_npc()
 * lib_print_response_object()
 * lib_print_response_npc()
 *
 * Print an object or NPC name framed by a prefix and a suffix, the response
 * variants selecting the prefix by Perspective.  These fold a three-call
 * sequence repeated throughout the module.
 */
void
lib_print_wrapped_object (scr_gameref_t game, const scr_char *prefix,
                          scr_int object, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, prefix);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, suffix);
}

void
lib_print_wrapped_npc (scr_gameref_t game, const scr_char *prefix,
                       scr_int npc, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, prefix);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, suffix);
}

void
lib_print_response_object (scr_gameref_t game,
                           const scr_char *second_person,
                           const scr_char *first_person,
                           const scr_char *third_person,
                           scr_int object, const scr_char *suffix)
{
  lib_print_wrapped_object (game,
                            lib_select_response (game, second_person,
                                                 first_person, third_person),
                            object, suffix);
}

void
lib_print_response_npc (scr_gameref_t game,
                        const scr_char *second_person,
                        const scr_char *first_person,
                        const scr_char *third_person,
                        scr_int npc, const scr_char *suffix)
{
  lib_print_wrapped_npc (game,
                         lib_select_response (game, second_person,
                                              first_person, third_person),
                         npc, suffix);
}

/*
 * lib_print_message()
 *
 * Buffer a fixed response message and indicate a handled command; the whole
 * action of the many chit-chat handlers near the end of the module.
 */
scr_bool
lib_print_message (scr_gameref_t game, const scr_char *message)
{
  pf_buffer_string (gs_get_filter (game), message);
  return TRUE;
}

scr_bool
lib_print_response_message (scr_gameref_t game,
                            const scr_char *second_person,
                            const scr_char *first_person,
                            const scr_char *third_person)
{
  return lib_print_message (game,
                            lib_select_response (game, second_person,
                                                 first_person, third_person));
}


/*
 * lib_new_clause()
 * lib_print_clause()
 *
 * Start a new clause in the turn's output.  Where something has already been
 * printed this turn, the clause is set off from it by two spaces; the rest is
 * either a fresh sentence, or the phrase appropriate to the perspective.
 */
void
lib_new_clause (scr_gameref_t game, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (has_printed)
    pf_buffer_string (filter, "  ");
  pf_new_sentence (filter);
}

void
lib_print_clause (scr_gameref_t game, scr_bool has_printed,
                  const scr_char *second_person,
                  const scr_char *first_person,
                  const scr_char *third_person)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (has_printed)
    pf_buffer_string (filter, "  ");
  pf_buffer_string (filter,
                    lib_select_response (game, second_person,
                                         first_person, third_person));
}


/*
 * lib_collect_children()
 * lib_reveal_children()
 *
 * Gather, in index order, the objects at the given position whose parent is
 * the given object or NPC: the contents of a container, the load on a
 * surface, or a character's held or worn possessions.  The reveal form also
 * marks each one seen, which is what listing them does in the Runner, and
 * returns how many there were.
 */
void
lib_collect_children (scr_gameref_t game, scr_int parent,
                      scr_int position, lib_list_t &list)
{
  scr_int object;

  list.clear ();
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_position (game, object) == position
          && gs_object_parent (game, object) == parent)
        list.push_back (object);
    }
}

scr_int
lib_reveal_children (scr_gameref_t game, scr_int parent,
                     scr_int position, lib_list_t &list)
{
  lib_collect_children (game, parent, position, list);
  for (const scr_int object : list)
    gs_set_object_seen (game, object, TRUE);
  return (scr_int) list.size ();
}


/*
 * lib_count_npc_references()
 *
 * Count the NPCs the command referenced, leaving the last of them in *npc,
 * or -1 if there was none.
 */
scr_int
lib_count_npc_references (scr_gameref_t game, scr_int *npc)
{
  scr_int index_, count;

  count = 0;
  *npc = -1;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (game->npc_references[index_])
        {
          count++;
          *npc = index_;
        }
    }
  return count;
}


/*
 * lib_print_list()
 * lib_print_name_list()
 *
 * Print a gathered list as "a, b, c and d", the last separator being the
 * given conjunction -- " and " for most lists, " or " where the game is
 * offering a choice.  Callers collect into a lib_list_t first rather than
 * printing as they iterate, so that the clause introducing the list can be
 * chosen from the finished list rather than reconstructed on the fly.
 */
void
lib_print_list (scr_gameref_t game, const lib_list_t &list,
                lib_print_item_t print_item, const scr_char *conjunction)
{
  const scr_filterref_t filter = gs_get_filter (game);
  size_t index_;

  for (index_ = 0; index_ < list.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == list.size () - 1 ? conjunction : ", ");
      print_item (game, list[index_]);
    }
}

void
lib_print_name_list (scr_gameref_t game, const lib_list_t &list,
                     const scr_char *const *names,
                     const scr_char *conjunction)
{
  const scr_filterref_t filter = gs_get_filter (game);
  size_t index_;

  for (index_ = 0; index_ < list.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == list.size () - 1 ? conjunction : ", ");
      pf_buffer_string (filter, names[list[index_]]);
    }
}


/*
 * lib_print_object_list()
 *
 * Print "<clause>a, b and c." for a gathered list of objects -- the shape
 * every "You can't wear ..."-style report about a batch of objects takes.
 * Prints nothing for an empty list.  Returns TRUE if it printed anything,
 * so that callers can fold the result into their has_printed.
 *
 * Names are normalized ("the ...") unless the caller asks for lib_print_object
 * instead; the pre-3.9 wear report is the one place that does.
 */
scr_bool
lib_print_object_list (scr_gameref_t game, scr_bool has_printed,
                       const lib_list_t &list,
                       const scr_char *conjunction, scr_char terminator,
                       const scr_char *second_person,
                       const scr_char *first_person,
                       const scr_char *third_person,
                       lib_print_item_t print_item)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (list.empty ())
    return FALSE;

  lib_print_clause (game, has_printed,
                    second_person, first_person, third_person);
  lib_print_list (game, list, print_item, conjunction);
  /* '\0' leaves the sentence open (no caller does so at present). */
  if (terminator != '\0')
    pf_buffer_character (filter, terminator);
  return TRUE;
}


/*
 * lib_get_npc_inroom_text()
 *
 * Returns the inroom description to be use for an NPC; if the NPC has
 * gone walkabout and offers a changed description, return that; otherwise
 * return the standard inroom text.
 */
const scr_char *
lib_get_npc_inroom_text (scr_gameref_t game, scr_int npc)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int walk_count, walk;
  const scr_char *inroomtext;

  /* Get the count of NPC walks. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Walks";
  walk_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /*
   * Check for any active walk with a description, return if found.
   *
   * "Active" here is the walk's task state -- StartTask complete, StoppingTask
   * not -- and nothing to do with how much counter the walk has left; the
   * Runner's room lister looks at those two fields and the ChangedDesc, and at
   * no counter (run400 viewroom Proc_19_63_472CA4, the NPC loop at
   * 4727E2..472931, re-read from the P-code 2026-08-24; run390 28995; 3.7
   * and 3.8 have no ChangedDesc at all, and their walk schema leaves it
   * empty).  So a walk that has run its course goes on describing the NPC
   * for the rest of the game.  In "The
   * Fun House" (4.00) that is the difference between "The tall man is
   * pleasant." and dropping him into the room's "... are here." sentence from
   * turn 16 on.
   *
   * The Runner scans upwards and keeps overwriting, so the highest-numbered
   * walk with a description wins; scanning down and returning is the same
   * pick.
   */
  for (walk = walk_count - 1; walk >= 0; walk--)
    {
      if (npc_walk_is_enabled (game, npc, walk))
        {
          const scr_char *changeddesc;

          /* Get and check any walk active description. */
          vt_key[3].integer = walk;
          vt_key[4].string = "ChangedDesc";
          changeddesc = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (changeddesc))
            return changeddesc;
        }
    }

  /* Return the standard inroom text. */
  vt_key[2].string = "InRoomText";
  inroomtext = prop_get_string (bundle, "S<-sis", vt_key);
  return inroomtext;
}


/*
 * lib_npc_text_is_default()
 *
 * TRUE if an NPC's in-room text is one the room lister folds into its joined
 * "X, Y and Z are here." sentence -- that is, one ending in " is here.".  The
 * Runner's "#" has already become such a text by the time its lister runs;
 * see the note in lib_print_room_contents().  Exact and case-sensitive, as
 * measured: "Golf is here!" and "Hotel IS HERE." are not folded.
 */

scr_bool
lib_npc_text_is_default (const scr_char *description)
{
  static const scr_char *const SUFFIX = " is here.";

  scr_int length = strlen (description);

  return length > LIB_NPC_HERE_LENGTH
         && strcmp (description + length - LIB_NPC_HERE_LENGTH, SUFFIX) == 0;
}


/*
 * lib_inroomdesc_is_absent()
 *
 * TRUE when an object has no in-room description at all.  The Runner's test
 * is an exact `InRoomDesc = ""` (run400 @00472589 for the print, @00449B0C
 * inside the helper that decides the "Also here is" list), NOT a
 * whitespace-trimmed one, so an in-room description of a single SPACE counts
 * as present: the Runner prints it -- invisibly -- and, because it printed
 * it, leaves the object out of the fallback list.  Authors use exactly that
 * to suppress an object the room's own long text already mentions.
 *
 * topaz.taf is the corpus case.  Its sword, object 5, sits in the Darkness
 * where the room text already says "The sword lies on the floor, a soft amber
 * glow shining from the jewel in its hilt", and its InRoomDesc is " ".
 * scr_strempty() trims, so Scarier read that as no description and added
 * "Also here is a Topaz." to a room that had just described the sword
 * (measured in run400 under Wine 2026-09-05, runner_transcripts/topaz.txt
 * turn 11).
 */
scr_bool
lib_inroomdesc_is_absent (const scr_char *inroomdesc)
{
  return inroomdesc == nullptr || inroomdesc[0] == NUL;
}

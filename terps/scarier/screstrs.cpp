/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
enum { MAX_NESTING_DEPTH = 32 };
static const scr_char NUL = '\0';

/* Trace flag, set before running. */
static scr_bool restr_trace = FALSE;


/*
 * restr_integer_variable()
 *
 * Return the index of the n'th integer found.
 */
static scr_int
restr_integer_variable (scr_gameref_t game, scr_int n)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int var_count, var, count;

  /* Get the count of variables. */
  vt_key[0].string = "Variables";
  var_count = prop_get_child_count (bundle, "I<-s", vt_key);

  /* Progress through variables until n integers found. */
  count = n;
  for (var = 0; var < var_count && count >= 0; var++)
    {
      scr_int type;

      vt_key[1].integer = var;
      vt_key[2].string = "Type";
      type = prop_get_integer (bundle, "I<-sis", vt_key);
      if (type == TAFVAR_NUMERIC)
        count--;
    }
  return var - 1;
}


/*
 * restr_object_in_place()
 *
 * Is object in a certain place, state, or condition.
 */
static scr_bool
restr_object_in_place (scr_gameref_t game,
                       scr_int object, scr_int var2, scr_int var3,
                       scr_bool quantified)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int npc, holder;

  if (restr_trace)
    {
      scr_trace ("Restr: checking"
                " object in place, %ld, %ld, %ld\n", object, var2, var3);
    }

  /* Var2 controls what we do. */
  switch (var2)
    {
    case 0:
    case 6:                    /* In room */
      /*
       * `position` is meaningful for a dynamic object: -1 is genuinely
       * "hidden", and >= 1 is the room, 1-based.  A static can still reach
       * here through the "referenced object" form, and then reads as hidden
       * until something moves it -- which is what the Adrift 4 runner does
       * too.  See restr_pass_task_object_location below.
       */
      if (var3 == 0)
        return gs_object_position (game, object) == OBJ_HIDDEN;
      else
        return gs_object_position (game, object) == var3;

    case 1:
    case 7:                    /* Held by */
      /*
       * "Held by the player" is broader than it sounds.  The Adrift 4 runner
       * answers TRUE for an object the player is *wearing* as well as one
       * being carried, and also for an object sitting inside a container that
       * the player carries or wears -- one level of nesting, not a recursive
       * search.  See the notes above restr_pass_task_object_location below.
       *
       * The container's openness is NOT consulted, and that half was checked
       * separately: probe `p39held` (test/adrift4/harness/make_39_heldprobe.py) run in the
       * real run390.exe answers KEY IS HELD for a key inside a *closed* box
       * the player carries, and only turns to NOT HELD when the box is
       * dropped -- picking the closed box back up restores it.  All five
       * states (loose, carried, open-carried-container, closed-carried-
       * container, closed-container-on-floor) match Scarier exactly.
       * Verified 2026-08-02; this is what lets inverness's desk be unlocked
       * with the old key still sealed inside its riddle box.
       *
       * Note that none of this applies to the NPC forms, nor to "worn by":
       * those really are the single exact position test they look like.
       *
       * ...and it does not apply to the "any object" / "no object" quantified
       * form either, which the Runner evaluates in a SEPARATE, hand-duplicated
       * per-object switch (`quantified` here).  That copy dropped the worn
       * case: at 00080871 in mdlSpreadTheLoad.Sub_20_3 the Var3 = 0 arm tests
       * only `location == 0` (held) and the container arm `location == 246`
       * with the parent held (0) or worn (156) -- there is no `location == 156`
       * test on the object itself, where the single-object path at 00080C9B
       * plainly has one.  So a worn object counts as held when a restriction
       * names it, but NOT when the restriction quantifies over all objects.
       *
       * Almost certainly a Runner slip rather than a design, but it is what
       * shipped, and games depend on it: Cursed's second interlude gates the
       * magical entrance on "no object is held by the player" while the player
       * wears street clothes that the game refuses to let you remove.  Count
       * the clothes and the veil can never be entered and the game is
       * unwinnable from that point on.
       */
      if (var3 == 0)            /* Player */
        {
          scr_int position, parent, parent_position;

          position = gs_object_position (game, object);
          if (position == OBJ_HELD_PLAYER)
            return TRUE;
          if (position == OBJ_WORN_PLAYER)
            return !quantified;

          if (position != OBJ_IN_OBJECT)
            return FALSE;

          parent = gs_object_parent (game, object);
          if (parent < 0 || parent >= gs_object_count (game))
            return FALSE;

          parent_position = gs_object_position (game, parent);
          return parent_position == OBJ_HELD_PLAYER
                 || parent_position == OBJ_WORN_PLAYER;
        }
      else if (var3 == 1)       /* Ref character */
        npc = var_get_ref_character (vars);
      else
        npc = var3 - 2;

      return gs_object_position (game, object) == OBJ_HELD_NPC
             && gs_object_parent (game, object) == npc;

    case 2:
    case 8:                    /* Worn by */
      if (var3 == 0)            /* Player */
        return gs_object_position (game, object) == OBJ_WORN_PLAYER;
      else if (var3 == 1)       /* Ref character */
        npc = var_get_ref_character (vars);
      else
        npc = var3 - 2;

      return gs_object_position (game, object) == OBJ_WORN_NPC
             && gs_object_parent (game, object) == npc;

    case 3:
    case 9:                    /* Visible to */
      if (var3 == 0)            /* Player */
        return obj_indirectly_in_room (game,
                                       object, gs_playerroom (game));
      else if (var3 == 1)       /* Ref character */
        npc = var_get_ref_character (vars);
      else
        npc = var3 - 2;

      return obj_indirectly_in_room (game, object,
                                     gs_npc_location (game, npc) - 1);

    /*
     * "Inside" and "on top of" both index a sublist -- containers for one,
     * surfaces for the other -- with Var3 - 1, and there is NO "nothing"
     * option.  SCARE used to read Var3 = 0 as "is not inside anything" and
     * return the negation of the position test; the guess was flagged with a
     * `/ * Nothing? * /` comment, and it was wrong.
     *
     * Probed 2026-08-01 against run400.exe, using a Topaz variant whose
     * `hedges` (an all-rooms static, so it is in the start room) was flipped to
     * a container with the dynamic `ring` starting inside it -- the runner
     * loads it and agrees, answering `look in bushes` with "The silver ring is
     * inside the hedges":
     *
     *   Var1  Var2  Var3  meaning                            runner  old SCARE
     *   4     4     1     ring is inside container 1         PASS    pass
     *   4     4     0     ring is inside nothing             fail    fail
     *   3     4     0     Topaz-object is inside nothing     fail    PASS  <--
     *   3     10    0     Topaz-object is NOT inside nothing PASS    fail  <--
     *   4     5     0     ring is on top of nothing          fail    PASS  <--
     *   3     11    0     ...is NOT on top of nothing        PASS    fail  <--
     *
     * Row 1 confirms the 1-based container-sublist index.  The rest are all
     * explained by Var3 = 0 producing index -1, which no object can match: the
     * plain form is then always false and the negated form always true.  So
     * return FALSE here and let the caller's negation do the rest.
     *
     * The Generator agrees -- its restriction dropdown strings run
     * "in room / held by / worn by / visible to / inside object / on object"
     * with a "- No room -" sentinel for the in-room case and no counterpart for
     * the other two, so Var3 = 0 is not authorable at all.  Nothing in the v4
     * corpus has it either: all 111 inside/on-top-of restrictions use Var3 >= 1.
     *
     * An index past the end of the sublist answers false in the runner without
     * complaint (unlike Var1, which raises "Subscript out of range").
     * obj_nth_object() overruns to the last object instead of failing, so test
     * what came back rather than trusting it.  That can only reject an index
     * that was already out of range -- a valid one always yields a real
     * container/surface.
     */
    case 4:
    case 10:                   /* Inside */
      if (var3 == 0)
        return FALSE;

      holder = obj_container_object (game, var3 - 1);
      if (holder < 0 || !obj_is_container (game, holder))
        return FALSE;

      return gs_object_position (game, object) == OBJ_IN_OBJECT
             && gs_object_parent (game, object) == holder;

    case 5:
    case 11:                   /* On top of */
      if (var3 == 0)
        return FALSE;

      holder = obj_surface_object (game, var3 - 1);
      if (holder < 0 || !obj_is_surface (game, holder))
        return FALSE;

      return gs_object_position (game, object) == OBJ_ON_OBJECT
             && gs_object_parent (game, object) == holder;

    default:
      scr_fatal ("restr_object_in_place: bad var2, %ld\n", var2);
      return FALSE;
    }
}


/*
 * restr_pass_task_object_location()
 *
 * Evaluate restrictions relating to object location.
 */
static scr_bool
restr_pass_task_object_location (scr_gameref_t game,
                                 scr_int var1, scr_int var2, scr_int var3)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_bool should_be;
  scr_int object;

  if (restr_trace)
    {
      scr_trace ("Restr: running object"
                " location restriction, %ld, %ld, %ld\n", var1, var2, var3);
    }

  /* See how things should look. */
  if (var2 >= 0 && var2 < 6)
    should_be = TRUE;
  else if (var2 >= 6 && var2 < 12)
    should_be = FALSE;
  else
    scr_fatal ("restr_pass_task_object_location: bad var2, %ld\n", var2);

  /*
   * Now find the addressed object.
   *
   * DELIBERATE DIVERGENCE for the "any object" / "no object" forms combined
   * with a negated Var2 (6..11).  We negate once, at the end of the loop
   * below, so "ANY object is NOT in room R" means "no dynamic object is in
   * room R" and "NO object is NOT in room R" means "some dynamic object is in
   * room R".  The real Runner cannot express either: the per-object condition
   * switch inside its any/no loop (mdlSpreadTheLoad.Sub_20_3, dispatch chain
   * at 000807EE..00080AF1) has cases for Var2 0..5 only, so a negated Var2
   * matches nothing, and the loop result collapses to a constant -- "any
   * object" always FAILS and "no object" always PASSES, whatever Var3 and the
   * world state are.
   *
   * Verified 2026-08-01 against run400.exe itself (Wine/Rosetta), with twelve
   * hand-built single-restriction Topaz variants: the six Var2 < 6 probes all
   * agree, and the six negated ones show the Runner returning that constant.
   * We keep the meaningful reading; nothing in the v4 corpus authors a negated
   * any/no object-location restriction, so no game can tell the difference.
   */
  if (var1 == 0)
    {
      object = -1;              /* No object */
      should_be = !should_be;
    }
  else if (var1 == 1)
    object = -1;                /* Any object */
  else if (var1 == 2)
    object = var_get_ref_object (vars);
  else if (var1 >= 3)
    /*
     * Confirmed against run400.exe 2026-08-01: Var1 - 3 indexes the DYNAMIC
     * objects only, 0-based, not the full object list.  Four probes on Topaz
     * (dynamics are object 5 `Topaz`, in room 4, and object 8 `ring`, hidden)
     * pin both halves down -- addressing Var1 = 3 as "in room 4" passes, which
     * a full-list reading could not do (object 3 is the static `sky`), and
     * Var1 = 4 as "in room 4" fails, which an off-by-one base could not do.
     * Var1 = 5 and 6 make the runner raise "evaluate error - Subscript out of
     * range", so its array really is the two-element dynamic one.  SCARE is
     * softer there: obj_nth_object() runs off the end and hands back the last
     * object.  Silent and wrong, but harmless -- no shipped game can contain
     * such an index, because loading it would crash the runner.
     */
    object = obj_dynamic_object (game, var1 - 3);
  else
    scr_fatal ("restr_pass_task_object_location: bad var1, %ld\n", var1);

  /*
   * A static object can arrive here through the "referenced object" form, and
   * unlike the any/no quantifier above the runner does NOT filter it out: it
   * evaluates the condition on whatever %object% matched.  SCARE used to
   * reject statics unconditionally at this point, which made "the referenced
   * object is visible to the player" -- by far the most common way this form
   * is authored, 25 of the 30 occurrences in the v4 corpus -- impossible to
   * satisfy by naming any piece of scenery.
   *
   * Verified 2026-08-01 against run400.exe (Wine/Rosetta), with six
   * single-restriction Topaz variants whose task command is `probe %object%`
   * and whose referenced object is the static `sky`:
   *
   *   Var2  Var3  meaning                     runner
   *   0     0     is hidden                   PASS
   *   0     1     is in room 1 (player's)     fail
   *   0     4     is in room 4                fail
   *   1     0     is held by the player       fail
   *   3     0     is visible to the player    PASS
   *   6     0     is NOT hidden               fail
   *
   * Falling through to restr_object_in_place() reproduces all six.  The two
   * halves come out right for different reasons, and both are worth spelling
   * out:
   *
   *  - "Visible to" is genuinely computed.  The runner splits static from
   *    dynamic there (mdlSpreadTheLoad.Sub_20_7) and consults the authored
   *    per-room list; so does obj_indirectly_in_room() via obj_static_in_room.
   *    Note `sky` is a Where-type-3 (all rooms) static, hence PASS -- and note
   *    that Var3 = 1 above is FALSE even though `sky` is in every room, which
   *    is what proves the "in room" case is NOT consulting that same list.
   *
   *  - The other cases match only because both engines end up reading a
   *    location field that statics never maintain.  The runner keeps a static's
   *    whereabouts in the per-room array at [1C] but still reads the dynamic
   *    location int at [1A] here, and that stays at its initial -1; SCARE
   *    likewise leaves an unmoved static's `position` at OBJ_HIDDEN.  So both
   *    answer "hidden", accidentally in agreement.  Body-part statics are the
   *    one place the accident breaks down: SCARE gives them OBJ_PART_NPC, so
   *    "is hidden" is FALSE where the runner would say TRUE.  Left alone --
   *    it is the saner answer, and no corpus game asks.
   */

  /* Try to put it all together. */
  if (object == -1)
    {
      scr_int target;

      for (target = 0; target < gs_object_count (game); target++)
        {
          /*
           * "Any object" / "No object" ranges over DYNAMIC objects only.  The
           * Adrift 4 runner opens this loop with an explicit `if not
           * Objects(i).Static then ... next i` filter, and it keeps no location
           * field for statics at all -- their whereabouts live in an authored
           * per-room list, while only dynamic objects get the location value
           * whose -1 means "hidden".
           *
           * Without this filter SCARE reads a static's unused position field,
           * which sits at -1 (== OBJ_HIDDEN) until an event moves the object.
           * Every piece of scenery therefore looked HIDDEN, so a "no object is
           * hidden" restriction could never pass in any game that has scenery,
           * and an "any object is hidden" one always did.  That is what made
           * Topaz's win task (wear the silver ring) unreachable.
           */
          if (obj_is_static (game, target))
            continue;

          if (restr_object_in_place (game, target, var2, var3, TRUE))
            return should_be;
        }
      return !should_be;
    }
  return should_be == restr_object_in_place (game, object, var2, var3, FALSE);
}


/*
 * restr_pass_task_object_state()
 *
 * Evaluate restrictions relating to object states.  This function is called
 * from the library by lib_pass_alt_room(), so cannot be static.
 */
scr_bool
restr_pass_task_object_state (scr_gameref_t game, scr_int var1, scr_int var2)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[3];
  scr_int object, openable, key;

  if (restr_trace)
    {
      scr_trace ("Restr:"
                " running object state restriction, %ld, %ld\n", var1, var2);
    }

  /* Find the object being addressed. */
  if (var1 == 0)
    object = var_get_ref_object (vars);
  else
    object = obj_stateful_object (game, var1 - 1);

  /*
   * If the restriction refers to "the referenced object" but the player's
   * command bound no object (var_get_ref_object returns -1, e.g. a wildcard
   * task command with no %object%), there is no object whose state can match,
   * so the restriction simply fails.  Guarding here avoids passing a negative
   * key down to prop_get_integer (which aborts); the Runner does not crash.
   */
  if (object < 0)
    return FALSE;

  /* We're interested only in openable objects. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Openable";
  openable = prop_get_integer (bundle, "I<-sis", vt_key);
  if (openable > 0)
    {
      /* Is this object lockable? */
      vt_key[2].string = "Key";
      key = prop_get_integer (bundle, "I<-sis", vt_key);
      if (key >= 0)
        {
          if (var2 <= 2)
            return gs_object_openness (game, object) == var2 + 5;
          else
            return gs_object_state (game, object) == var2 - 2;
        }
      else
        {
          if (var2 <= 1)
            return gs_object_openness (game, object) == var2 + 5;
          else
            return gs_object_state (game, object) == var2 - 1;
        }
    }
  else
    return gs_object_state (game, object) == var2 + 1;
}


/*
 * restr_pass_task_task_state()
 *
 * Evaluate restrictions relating to task states.
 */
static scr_bool
restr_pass_task_task_state (scr_gameref_t game, scr_int var1, scr_int var2)
{
  scr_bool should_be;

  if (restr_trace)
    scr_trace ("Restr: running task restriction, %ld, %ld\n", var1, var2);

  /* See if the task should be done or not done. */
  if (var2 == 0)
    should_be = TRUE;
  else if (var2 == 1)
    should_be = FALSE;
  else
    scr_fatal ("restr_pass_task_task_state: bad var2, %ld\n", var2);

  /* Check all tasks? */
  if (var1 == 0)
    {
      scr_int task;

      for (task = 0; task < gs_task_count (game); task++)
        {
          if (gs_task_done (game, task) == should_be)
            return FALSE;
        }
      return TRUE;
    }

  /* Check just the given task. */
  return gs_task_done (game, var1 - 1) == should_be;
}


/*
 * restr_pass_task_char()
 *
 * Evaluate restrictions relating to player and NPCs.
 */
static scr_bool
restr_pass_task_char (scr_gameref_t game, scr_int var1, scr_int var2, scr_int var3)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int npc1, npc2;

  if (restr_trace)
    {
      scr_trace ("Restr:"
                " running char restriction, %ld, %ld, %ld\n", var1, var2, var3);
    }

  /* Handle var2 types 1 and 2. */
  if (var2 == 1)                /* Not in same room as */
    return !restr_pass_task_char (game, var1, 0, var3);
  else if (var2 == 2)           /* Alone */
    return !restr_pass_task_char (game, var1, 3, var3);

  /* Decode NPC number, -1 if none. */
  npc1 = npc2 = -1;
  if (var1 == 1)
    npc1 = var_get_ref_character (vars);
  else if (var1 > 1)
    npc1 = var1 - 2;

  /* Player or NPC? */
  if (var1 == 0)
    {
      scr_vartype_t vt_key[2];
      scr_int gender;

      /* Player -- decode based on var2. */
      switch (var2)
        {
        case 0:                /* In same room as */
          if (var3 == 1)
            npc2 = var_get_ref_character (vars);
          else if (var3 > 1)
            npc2 = var3 - 2;
          if (var3 == 0)       /* Player */
            return TRUE;
          else
            return npc_in_room (game, npc2, gs_playerroom (game));

        case 3:                /* Not alone */
          return npc_count_in_room (game, gs_playerroom (game)) > 1;

        case 4:                /* Standing on */
          return gs_playerposition (game) == 0
                 && gs_playerparent (game) == obj_standable_object (game,
                                                                    var3 - 1);

        case 5:                /* Sitting on */
          return gs_playerposition (game) == 1
                 && gs_playerparent (game) == obj_standable_object (game,
                                                                    var3 - 1);

        case 6:                /* Lying on */
          return gs_playerposition (game) == 2
                 && gs_playerparent (game) == obj_lieable_object (game,
                                                                  var3 - 1);

        case 7:                /* Player gender */
          vt_key[0].string = "Globals";
          vt_key[1].string = "PlayerGender";
          gender = prop_get_integer (bundle, "I<-ss", vt_key);
          return gender == var3;

        default:
          scr_fatal ("restr_pass_task_char: invalid type, %ld\n", var2);
          return FALSE;
        }
    }
  else
    {
      scr_vartype_t vt_key[3];
      scr_int gender;

      /* NPC -- decode based on var2. */
      switch (var2)
        {
        case 0:                /* In same room as */
          if (var3 == 0)
            return npc_in_room (game, npc1, gs_playerroom (game));
          if (var3 == 1)
            npc2 = var_get_ref_character (vars);
          else if (var3 > 1)
            npc2 = var3 - 2;
          return npc_in_room (game, npc1, gs_npc_location (game, npc2) - 1);

        case 3:                /* Not alone */
          return npc_count_in_room (game, gs_npc_location (game, npc1) - 1) > 1;

        /* Cases 4-6 test the NPC's position but the *player's* parent object.
           This reproduces the original SCARE behavior; don't "correct" it into
           a divergence from the Adrift runner without evidence.  */
        case 4:                /* Standing on */
          return gs_npc_position (game, npc1) == 0
                 && gs_playerparent (game) == obj_standable_object (game, var3);

        case 5:                /* Sitting on */
          return gs_npc_position (game, npc1) == 1
                 && gs_playerparent (game) == obj_standable_object (game, var3);

        case 6:                /* Lying on */
          return gs_npc_position (game, npc1) == 2
                 && gs_playerparent (game) == obj_lieable_object (game, var3);

        case 7:                /* NPC gender */
          vt_key[0].string = "NPCs";
          vt_key[1].integer = npc1;
          vt_key[2].string = "Gender";
          gender = prop_get_integer (bundle, "I<-sis", vt_key);
          return gender == var3;

        default:
          scr_fatal ("restr_pass_task_char: invalid type, %ld\n", var2);
          return FALSE;
        }
    }
}


/*
 * restr_pass_task_int_var()
 *
 * Helper for restr_pass_task_var(), handles integer variable restrictions.
 */
static scr_bool
restr_pass_task_int_var (scr_gameref_t game,
                         scr_int var2, scr_int var3, scr_int value)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int value2;

  if (restr_trace)
    {
      scr_trace ("Restr: running"
                " integer var restriction, %ld, %ld, %ld\n", var2, var3, value);
    }

  /* Compare against var3 if that's what var2 says. */
  switch (var2)
    {
    case 0:
      return value < var3;
    case 1:
      return value <= var3;
    case 2:
      return value == var3;
    case 3:
      return value >= var3;
    case 4:
      return value > var3;
    case 5:
      return value != var3;

    default:
      /*
       * Compare against the integer var numbered in var3 - 1, or the
       * referenced number if var3 is zero.  Make sure that we're comparing
       * integer variables.
       */
      if (var3 == 0)
        value2 = var_get_ref_number (vars);
      else
        {
          const scr_char *name;
          scr_int ivar, type;

          ivar = restr_integer_variable (game, var3 - 1);
          name = prop_get_indexed_string (bundle, "Variables", ivar, "Name");
          type = prop_get_indexed_integer (bundle, "Variables", ivar, "Type");

          if (type != TAFVAR_NUMERIC)
            {
              scr_fatal ("restr_pass_task_int_var:"
                        " non-integer in comparison, %s\n", name);
            }

          /* Get the value in variable numbered in var3 - 1. */
          value2 = var_get_integer (vars, name);
        }

      switch (var2)
        {
        case 10:
          return value < value2;
        case 11:
          return value <= value2;
        case 12:
          return value == value2;
        case 13:
          return value >= value2;
        case 14:
          return value > value2;
        case 15:
          return value != value2;

        default:
          scr_fatal ("restr_pass_task_int_var:"
                    " unknown int comparison, %ld\n", var2);
          return FALSE;
        }
    }
}


/*
 * restr_pass_task_string_var()
 *
 * Helper for restr_pass_task_var(), handles string variable restrictions.
 */
static scr_bool
restr_pass_task_string_var (scr_int var2,
                            const scr_char *var4, const scr_char *value)
{
  if (restr_trace)
    {
      scr_trace ("Restr: running string"
                " var restriction, %ld, \"%s\", \"%s\"\n", var2, var4, value);
    }

  /* Make comparison against var4 based on var2 value. */
  switch (var2)
    {
    case 0:
      return strcmp (value, var4) == 0;  /* == */
    case 1:
      return strcmp (value, var4) != 0;  /* != */

    default:
      scr_fatal ("restr_pass_task_string_var:"
                " unknown string comparison, %ld\n", var2);
      return FALSE;
    }
}


/*
 * restr_pass_task_var()
 *
 * Evaluate restrictions relating to variables.
 */
static scr_bool
restr_pass_task_var (scr_gameref_t game,
                     scr_int var1, scr_int var2, scr_int var3,
                     const scr_char *var4)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[3];
  scr_int type, value;
  const scr_char *name, *string;

  if (restr_trace)
    {
      scr_trace ("Restr: running var restriction,"
                " %ld, %ld, %ld, \"%s\"\n", var1, var2, var3, var4);
    }

  /*
   * For var1=0, compare against referenced number.  For var1=1, compare
   * against referenced text.
   */
  if (var1 == 0)
    {
      value = var_get_ref_number (vars);
      return restr_pass_task_int_var (game, var2, var3, value);
    }
  else if (var1 == 1)
    {
      string = var_get_ref_text (vars);
      return restr_pass_task_string_var (var2, var4, string);
    }

  /* Get the name and type of the variable being addressed. */
  vt_key[0].string = "Variables";
  vt_key[1].integer = var1 - 2;
  vt_key[2].string = "Name";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Type";
  type = prop_get_integer (bundle, "I<-sis", vt_key);

  /* Select first based on variable type. */
  switch (type)
    {
    case TAFVAR_NUMERIC:
      value = var_get_integer (vars, name);
      return restr_pass_task_int_var (game, var2, var3, value);

    case TAFVAR_STRING:
      string = var_get_string (vars, name);
      return restr_pass_task_string_var (var2, var4, string);

    default:
      scr_fatal ("restr_pass_task_var: invalid variable type, %ld\n", type);
      return FALSE;
    }
}


/*
 * restr_pass_task_restriction()
 *
 * Demultiplexer for task restrictions.
 */
static scr_bool
restr_pass_task_restriction (scr_gameref_t game, scr_int task, scr_int restriction)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int type, var1, var2, var3;
  const scr_char *var4;
  scr_bool result = FALSE;

  if (restr_trace)
    {
      scr_trace ("Restr:"
                " evaluating task %ld restriction %ld\n", task, restriction);
    }

  /* Get the task restriction type. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Restrictions";
  vt_key[3].integer = restriction;
  vt_key[4].string = "Type";
  type = prop_get_integer (bundle, "I<-sisis", vt_key);

  /* Demultiplex depending on type. */
  switch (type)
    {
    case 0:                    /* Object location. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      result = restr_pass_task_object_location (game, var1, var2, var3);
      break;

    case 1:                    /* Object state. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      result = restr_pass_task_object_state (game, var1, var2);
      break;

    case 2:                    /* Task state. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      result = restr_pass_task_task_state (game, var1, var2);
      break;

    case 3:                    /* Player and NPCs. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      result = restr_pass_task_char (game, var1, var2, var3);
      break;

    case 4:                    /* Variable. */
      vt_key[4].string = "Var1";
      var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var2";
      var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var3";
      var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
      vt_key[4].string = "Var4";
      var4 = prop_get_string (bundle, "S<-sisis", vt_key);
      result = restr_pass_task_var (game, var1, var2, var3, var4);
      break;

    case 5:                    /* Action type (Runner: Sub_20_3 type 5 / sentinel 0xEC). */
      /* No known TAF file emits type 5; the TAF parser does not define it.
       * Don't fatal — return FALSE so the restriction fails silently. */
      scr_trace ("Restr: task %ld restriction %ld type 5"
                 " (action-type) not implemented; returning FALSE\n",
                 task, restriction);
      result = FALSE;
      break;

    default:
      scr_fatal ("restr_pass_task_restriction:"
                " unknown restriction type %ld\n", type);
    }

  if (restr_trace)
    {
      scr_trace ("Restr: task %ld restriction"
                " %ld is %s\n", task, restriction, result ? "PASS" : "FAIL");
    }

  return result;
}


/* Enumeration of restrictions combination string tokens. */
enum
{ TOK_RESTRICTION = '#',
  TOK_AND = 'A',
  TOK_OR = 'O',
  TOK_LPAREN = '(',
  TOK_RPAREN = ')',
  TOK_EOS = '\0'
};

/* #O#A(#O#)-style expression, for tokenizing. */
static const scr_char *restr_expression = NULL;
static scr_int restr_index = 0;

/*
 * restr_tokenize_start()
 * restr_tokenize_end()
 *
 * Start and wrap up restrictions combinations string tokenization.
 */
static void
restr_tokenize_start (const scr_char *expression)
{
  /* Save expression, and restart index. */
  restr_expression = expression;
  restr_index = 0;
}

static void
restr_tokenize_end (void)
{
  restr_expression = NULL;
  restr_index = 0;
}


/*
 * restr_next_token()
 *
 * Simple tokenizer for restrictions combination expressions.
 */
static scr_char
restr_next_token (void)
{
  assert (restr_expression);

  /* Find the next non-space, and return it. */
  while (TRUE)
    {
      /* Return NUL if at string end. */
      if (restr_expression[restr_index] == NUL)
        return restr_expression[restr_index];

      /* Spin on whitespace. */
      restr_index++;
      if (scr_isspace (restr_expression[restr_index - 1]))
        continue;

      /* Return the character just passed. */
      return restr_expression[restr_index - 1];
    }
}


/* Evaluation values stack. */
static scr_bool restr_eval_values[MAX_NESTING_DEPTH];
static scr_int restr_eval_stack = 0;

/*
 * The restriction number to evaluate.  This advances with each call to
 * evaluate and stack a restriction result.
 */
static scr_int restr_eval_restriction = 0;

/* The current game used to evaluate restrictions, and the task in question. */
static scr_gameref_t restr_eval_game = (scr_gameref_t) NULL;
static scr_int restr_eval_task = 0;

/* The id of the lowest-indexed failing restriction. */
static scr_int restr_lowest_fail = -1;

/*
 * restr_eval_start()
 *
 * Reset the evaluation stack to an empty state, and note the things we have
 * to note for when we need to evaluate a restriction.
 */
static void
restr_eval_start (scr_gameref_t game, scr_int task)
{
  /* Clear stack. */
  restr_eval_stack = 0;
  restr_eval_restriction = 0;

  /* Note evaluation details. */
  restr_eval_game = game;
  restr_eval_task = task;

  /* Clear lowest indexed failing restriction. */
  restr_lowest_fail = -1;
}


/*
 * restr_eval_push()
 *
 * Push a value onto the values stack.
 */
static void
restr_eval_push (scr_bool value)
{
  if (restr_eval_stack >= MAX_NESTING_DEPTH)
    scr_fatal ("restr_eval_push: stack overflow\n");

  restr_eval_values[restr_eval_stack++] = value;
}


/*
 * expr_restr_action()
 *
 * Evaluate the effect of an and/or into the values stack.
 */
static void
restr_eval_action (scr_char token)
{
  /* Select action based on parsed token. */
  switch (token)
    {
      /* Handle evaluating and pushing a restriction result. */
    case TOK_RESTRICTION:
      {
        scr_bool result;

        /* Evaluate and push the next restriction. */
        result = restr_pass_task_restriction (restr_eval_game,
                                              restr_eval_task,
                                              restr_eval_restriction);
        restr_eval_push (result);

        /*
         * If the restriction failed, and there isn't yet a first failing one
         * set, note this one as the first to fail.
         */
        if (restr_lowest_fail == -1 && !result)
          restr_lowest_fail = restr_eval_restriction;

        /* Increment restriction sequence identifier. */
        restr_eval_restriction++;
        break;
      }

      /* Handle cases of or-ing/and-ing restrictions. */
    case TOK_OR:
    case TOK_AND:
      {
        scr_bool val1, val2, result = FALSE;

        /*
         * Guard against stack underflow with a real fatal (as scexpr.cpp does
         * for the analogous expression case) so it survives NDEBUG, where a
         * bare assert is a no-op and a malformed restriction stream could read
         * before the values stack.
         */
        if (restr_eval_stack < 2)
          scr_fatal ("restr_eval_action: stack underflow\n");

        /* Get the top two stack values. */
        val1 = restr_eval_values[restr_eval_stack - 2];
        val2 = restr_eval_values[restr_eval_stack - 1];

        /* Or, or and, into result. */
        switch (token)
          {
          case TOK_OR:
            result = val1 || val2;
            break;
          case TOK_AND:
            result = val1 && val2;
            break;

          default:
            scr_fatal ("restr_eval_action: bad token, '%c'\n", token);
          }

        /* Put result back at top of stack. */
        restr_eval_stack--;
        restr_eval_values[restr_eval_stack - 1] = result;
        break;
      }

    default:
      scr_fatal ("restr_eval_action: bad token, '%c'\n", token);
    }
}


/*
 * restr_eval_result()
 *
 * Return the top of the values stack as the evaluation result.
 */
static scr_int
restr_eval_result (scr_int *lowest_fail)
{
  if (restr_eval_stack != 1)
    scr_fatal ("restr_eval_result: values stack not completed\n");

  *lowest_fail = restr_lowest_fail;
  return restr_eval_values[0];
}


/* Parse error jump buffer. */
static jmp_buf restr_parse_error;

/* Single lookahead token for parser. */
static scr_char restr_lookahead = '\0';

/*
 * restr_match()
 *
 * Match a token with an expectation.
 */
static void
restr_match (scr_char c)
{
  if (restr_lookahead == c)
    restr_lookahead = restr_next_token ();
  else
    {
      scr_error ("restr_match:"
                " syntax error, expected %d, got %d\n", c, restr_lookahead);
      scr_longjmp (restr_parse_error, 1);
    }
}


/* Forward declaration for recursion. */
static void restr_bexpr (void);

/*
 * restr_expr()
 * restr_bexpr()
 *
 * Expression parsers.  Here we go again...
 *
 * "A" and "O" have EQUAL precedence and associate to the LEFT, so "#O#A#"
 * is "(1 OR 2) AND 3" and never "1 OR (2 AND 3)".  SCARE used to parse the
 * mask with C precedence -- an or-expression over and-expressions -- which
 * agrees whenever every A precedes every O, and differs the moment an O
 * comes before an A at the same bracket level.
 *
 * Ground truth is run400.exe's own P-code, mdlSpreadTheLoad.Sub_20_57
 * ("evaluaterestrictions", 00055CAC..00055EB9), which recurses from the RIGHT:
 *
 *   If s = "T" Then True : If s = "F" Then False
 *   If Right(s, 1) = ")" Then          ' Sub_20_56 finds the matching "("
 *     grp = trailing bracket group : tail = evaluaterestrictions(inside grp)
 *     s = Left(s, Len(s) - Len(grp))
 *   Else
 *     tail = evaluaterestrictions(Right(s, 1)) : s = Left(s, Len(s) - 1)
 *   If s = "" Then tail
 *   ElseIf Right(s, 1) = "A" Then tail And evaluaterestrictions(Left(s, -1))
 *   ElseIf Right(s, 1) = "O" Then tail Or  evaluaterestrictions(Left(s, -1))
 *   Else MsgBox "Oops - bad bracket string (evaluaterestrictions): "
 *
 * -- one operator per level, the whole head re-parsed underneath it.  Peeling
 * the LAST operand off and recursing on the head is left association: for
 * "a A b O c" the outermost call folds `c Or evaluaterestrictions("aAb")`,
 * i.e. "(a And b) Or c".  There is no second precedence level anywhere in the
 * routine -- "A" and "O" are two arms of the same If.  Its
 * caller Sub_20_65 first walks the restrictions in index order substituting
 * "T"/"F" for each "#" (so every restriction is evaluated, no short circuit,
 * as SCARE also does), then hands the resulting string to Sub_20_57.
 *
 * 20 of the v4 corpus games author a mask that mixes A and O at one bracket
 * level; the ones this changes are the ones where an O comes first.  The
 * case that found it is 3monkeys T21, the author's own `winnable` self-check,
 * whose group "#O(#A#)A#" is "(bucket on the hook OR the coconut is set up)
 * AND the gate is still shut".  Under C precedence the trailing AND binds
 * only to the second disjunct, the group is true from turn 1, and the game
 * declares itself unwinnable before the player has moved.
 *
 * The parse walks the mask left to right, so restrictions are evaluated in
 * index order -- matching Sub_20_65's loop, and keeping restr_lowest_fail
 * (the FailMessage pick) on the lowest-indexed failure.
 */
static void
restr_expr (void)
{
  restr_bexpr ();

  while (restr_lookahead == TOK_AND || restr_lookahead == TOK_OR)
    {
      scr_char operator_ = restr_lookahead;

      restr_match (operator_);
      restr_bexpr ();
      restr_eval_action (operator_);
    }
}

static void
restr_bexpr (void)
{
  switch (restr_lookahead)
    {
    case TOK_RESTRICTION:
      restr_match (TOK_RESTRICTION);
      restr_eval_action (TOK_RESTRICTION);
      break;

    case TOK_LPAREN:
      restr_match (TOK_LPAREN);
      restr_expr ();
      restr_match (TOK_RPAREN);
      break;

    default:
      scr_error ("restr_bexpr: syntax error, unexpected %d\n", restr_lookahead);
      scr_longjmp (restr_parse_error, 1);
    }
}


/*
 * restr_get_fail_message()
 *
 * Get the FailMessage for the given task restriction; NULL if none.
 */
static const scr_char *
restr_get_fail_message (scr_gameref_t game, scr_int task, scr_int restriction)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  const scr_char *message;

  /* Get the restriction message. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Restrictions";
  vt_key[3].integer = restriction;
  vt_key[4].string = "FailMessage";
  message = prop_get_string (bundle, "S<-sisis", vt_key);

  /* Return it, or NULL if empty. */
  return !scr_strempty (message) ? message : NULL;
}


/*
 * restr_debug_trace()
 *
 * Set restrictions tracing on/off.
 */
void
restr_debug_trace (scr_bool flag)
{
  restr_trace = flag;
}


/*
 * restr_eval_task_restrictions()
 *
 * Main handler for a given set of task restrictions.  Returns TRUE in pass
 * if the restrictions pass, FALSE if not.  On FALSE pass returns, it also
 * returns a fail message string from the restriction deemed to have caused
 * the failure (that is, the first one with a FailMessage property), or NULL
 * if no failing restriction has a FailMessage.  The function's main return
 * value is TRUE if restrictions parsed successfully, FALSE otherwise.
 */
scr_bool
restr_eval_task_restrictions (scr_gameref_t game,
                              scr_int task, scr_bool *pass,
                              const scr_char **fail_message)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int restr_count, lowest_fail;
  const scr_char *pattern;
  scr_bool result;
  assert (pass && fail_message);

  /* Get the count of restrictions on the task. */
  vt_key[0].string = "Tasks";
  vt_key[1].integer = task;
  vt_key[2].string = "Restrictions";
  restr_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /* If none, stop now, acting as if all passed. */
  if (restr_count == 0)
    {
      if (restr_trace)
        scr_trace ("Restr: task %ld has no restrictions\n", task);

      *pass = TRUE;
      *fail_message = NULL;
      return TRUE;
    }

  /* Get the task's restriction combination pattern. */
  vt_key[2].string = "RestrMask";
  pattern = prop_get_string (bundle, "S<-sis", vt_key);

  if (restr_trace)
    {
      scr_trace ("Restr: task %ld"
                " has %ld restrictions, %s\n", task, restr_count, pattern);
    }

  /* Set up the evaluation stack and tokenizer. */
  restr_eval_start (game, task);
  restr_tokenize_start (pattern);

  /* Try parsing the pattern, and catch errors. */
  if (scr_setjmp (restr_parse_error) == 0)
    {
      /* Parse the pattern, and ensure it ends at string end. */
      restr_lookahead = restr_next_token ();
      restr_expr ();
      restr_match (TOK_EOS);
    }
  else
    {
      /* Parse error -- clean up tokenizer and return fail. */
      restr_tokenize_end ();
      return FALSE;
    }

  /* Clean up tokenizer and get the evaluation result. */
  restr_tokenize_end ();
  result = restr_eval_result (&lowest_fail);

  if (restr_trace)
    {
      scr_trace ("Restr: task %ld"
                " restrictions %s\n", task, result ? "PASS" : "FAIL");
    }

  /*
   * Return the result, and if a restriction fails, then return the
   * FailMessage of the lowest indexed failing restriction (or NULL if this
   * restriction has no FailMessage).
   *
   * Then return TRUE since parsing and running the restrictions succeeded
   * (even if the restrictions themselves didn't).
   */
  *pass = result;
  if (result)
    *fail_message = NULL;
  else
    *fail_message = restr_get_fail_message (game, task, lowest_fail);
  return TRUE;
}

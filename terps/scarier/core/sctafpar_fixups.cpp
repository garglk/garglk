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
 * Version fixups for the TAF parser.  The '|...|' specials in the version
 * 3.9, 3.8 and 3.7 parse schemas land here, and each handler rewrites the
 * properties just parsed into their version 4.0 form, so that the rest of
 * the interpreter sees one layout whatever the TAF version.  The handlers
 * reach the parse state through sctafpar_internal.h.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "scarier.h"
#include "sctafpar_internal.h"


/*
 * parse_fixup_room_alt_put_res()
 *
 * Helper for parse_fixup_v390_v380_room_alt(); write one Res1/Res2 resource
 * node of a room alt.  Because this is not version 4.0, we can ignore
 * lengths, and set them to zero.
 */
static void
parse_fixup_room_alt_put_res (const scr_char *res_key,
                              scr_bool has_sound, scr_bool has_graphics,
                              const scr_char *soundfile,
                              const scr_char *graphicfile)
{
  scr_vartype_t vt_key;

  if (!has_sound && !has_graphics)
    return;

  vt_key.string = res_key;
  parse_push_key (vt_key, PROP_KEY_STRING);
  if (has_sound)
    {
      parse_put_keyed_string ("SoundFile", soundfile);
      parse_put_keyed_integer ("SoundLen", 0);
    }
  if (has_graphics)
    {
      parse_put_keyed_string ("GraphicFile", graphicfile);
      parse_put_keyed_integer ("GraphicLen", 0);
    }
  parse_pop_key ();
}


/*
 * parse_fixup_v390_v380_room_alt()
 *
 * Helper for parse_fixup_v390_v380_room_alts().  Handles creation of
 * version 4.0 room alts for version 3.9 and version 3.8 games.  The M2
 * "else" text and the Changed room name are always empty in a converted
 * alt, and only the Res1 resource is ever copied over from the main room.
 */
static void
parse_fixup_v390_v380_room_alt (const scr_char *m1, scr_int type,
                                const scr_char *resource1,
                                scr_int var2, scr_int hide_objects,
                                scr_int var3, scr_int display_room)
{
  scr_vartype_t vt_key;
  scr_bool has_sound, has_graphics;
  scr_int alt_count;
  const scr_char *soundfile1, *graphicfile1;

  /*
   * Initialize resource files to empty, for cases where no resource is copied
   * over from the main room (NULL resource1).
   */
  soundfile1 = "";
  graphicfile1 = "";

  /* Get sound and graphics flags, always FALSE before version 3.9. */
  has_sound = parse_get_global_boolean ("Sound");
  has_graphics = parse_get_global_boolean ("Graphics");

  /* Get a count of alts so far defined for the room. */
  alt_count = parse_get_keyed_child_count ("Alts");

  /* Lookup any resource details now, and save them. */
  if ((has_sound || has_graphics) && resource1)
    {
      vt_key.string = resource1;
      parse_push_key (vt_key, PROP_KEY_STRING);
      if (has_sound)
        soundfile1 = parse_get_keyed_string ("SoundFile");
      if (has_graphics)
        graphicfile1 = parse_get_keyed_string ("GraphicFile");
      parse_pop_key ();
    }

  /*
   * Create a room alt to match data passed in.  Start with the Alts string
   * and the index to the alt being written.  To correctly emulate the parse,
   * we also have to reverse the "Alts" and the index, as parse_put_property()
   * will swap them.  Madness.
   */
  vt_key.integer = alt_count;
  parse_push_key (vt_key, PROP_KEY_INTEGER);
  vt_key.string = "Alts";
  parse_push_key (vt_key, PROP_KEY_STRING);

  /* Write M1 and Type, with any resources retrieved above. */
  parse_put_keyed_string ("M1", m1);
  parse_put_keyed_integer ("Type", type);
  parse_fixup_room_alt_put_res ("Res1", has_sound, has_graphics,
                                soundfile1, graphicfile1);

  /* Write the empty M2, Var2, and empty Res2 resources. */
  parse_put_keyed_string ("M2", "");
  parse_put_keyed_integer ("Var2", var2);
  parse_fixup_room_alt_put_res ("Res2", has_sound, has_graphics, "", "");

  /* Finish off with the last four alt properties. */
  parse_put_keyed_integer ("HideObjects", hide_objects);
  parse_put_keyed_string ("Changed", "");
  parse_put_keyed_integer ("Var3", var3);
  parse_put_keyed_integer ("DisplayRoom", display_room);

  parse_pop_key ();
  parse_pop_key ();
}


/* Multiplier for combination AltDesc Type and HideObject values. */
enum { V390_V380_ALT_TYPEHIDE_MULT = 10 };

/*
 * parse_fixup_v390_v380_task_alt()
 *
 * Helper for parse_fixup_v390_v380_room_alts().  If the given task
 * alternate description is defined, create a room alt to add after the
 * main description, one that stops printing once done.
 */
static void
parse_fixup_v390_v380_task_alt (const scr_char *task_key,
                                const scr_char *desc_key,
                                const scr_char *res_key)
{
  scr_int var2;

  var2 = parse_get_keyed_integer (task_key);
  if (var2 > 0)
    {
      parse_fixup_v390_v380_room_alt (parse_get_keyed_string (desc_key),
                                      0 /* Task condition */,
                                      res_key, var2, 0, 0,
                                      1 /* Print after main and stop */);
    }
}


/*
 * parse_fixup_v390_v380_room_alts()
 *
 * Common helper function for parse_fixup_v390() and parse_fixup_v380(),
 * converts version 3.9 and version 3.8 fixed room description alts into
 * an equivalent array of version 4.0 style room alts.
 *
 * The order matters, and is the opposite of what reads naturally.  Alts are
 * selected by lib_find_starting_alt(), which scans the array *backwards* for
 * the first matching method-0/1 alt, then prints forwards from there, adding
 * only later method-2 alts.  So the least specific alt has to come first and
 * the most specific last: LastDesc catch-all, Task1, Task2, then the object
 * alt that overrides everything.  This matches the alt array that the ADRIFT
 * Generator itself writes when it converts a 3.9 game to 4.0, and matches the
 * 3.9 Runner, which prints the AddDesc *instead of* the LastDesc rather than
 * as well as it (verified live -- see test/adrift4/notes/RUNNER_TESTS_TODO.md
 * section 3a).
 */
static void
parse_fixup_v390_v380_room_alts (void)
{
  const scr_char *m1;
  scr_int var3;

  /*
   * The catch-all room alt, printed only when no other alt applies.  Its
   * condition is an always-true task condition.
   */
  m1 = parse_get_keyed_string ("LastDesc");
  if (!scr_strempty (m1))
    {
      parse_fixup_v390_v380_room_alt (m1, 0 /* Task condition, always TRUE */,
                                      "LastRes", 0, 0, 0,
                                      2 /* Lowest priority output */);
    }

  /* Create alts for the first and second task additional descriptions. */
  parse_fixup_v390_v380_task_alt ("Task1", "AddDesc1", "Task1Res");
  parse_fixup_v390_v380_task_alt ("Task2", "AddDesc2", "Task2Res");

  /*
   * Last, and so overriding all the others, a room alt controlled by an
   * object condition and with optional object hiding.
   */
  var3 = parse_get_keyed_integer ("Obj");
  if (var3 > 0)
    {
      scr_int typehideobjects, var2, hide_objects;

      m1 = parse_get_keyed_string ("AltDesc");
      typehideobjects = parse_get_keyed_integer ("TypeHideObjects");

      var2 = typehideobjects / V390_V380_ALT_TYPEHIDE_MULT;
      hide_objects = typehideobjects % V390_V380_ALT_TYPEHIDE_MULT;

      parse_fixup_v390_v380_room_alt (m1, 2 /* Object condition */,
                                      "AltRes", var2, hide_objects, var3,
                                      0 /* Override all others */);
    }
}


/*
 * parse_write_restrmask()
 *
 * Count the restrictions written so far under the current key, and store a
 * RestrMask that 'and's them all together ("#", "#A#", "#A#A#", ...).  Both
 * the 3.9 and the 3.8 task conversions end by doing exactly this.  A task
 * with no restrictions gets no mask at all, as before.
 */
static void
parse_write_restrmask (void)
{
  scr_int restriction_count;

  /* Get a count of restrictions. */
  restriction_count = parse_get_keyed_child_count ("Restrictions");

  /* Allocate and fill a new mask for these restrictions. */
  if (restriction_count > 0)
    {
      scr_char *restrmask;
      scr_int index_;
      size_t restrmask_size = parse_checked_multiply (restriction_count, 2);

      /* "#" then "A#" per additional restriction: 2 * count bytes with the
         terminator, which is exactly restrmask_size. */
      size_t used = 1;

      restrmask = (decltype(restrmask)) scr_malloc (restrmask_size);
      restrmask[0] = '#';
      restrmask[1] = '\0';
      for (index_ = 1; index_ < restriction_count; index_++)
        {
          snprintf (restrmask + used, restrmask_size - used, "A#");
          used += 2;
        }

      parse_put_keyed_string ("RestrMask", restrmask);
      prop_adopt (parse_bundle, restrmask);
    }
}


/*
 * parse_fixup_v390_battle_attribute()
 *
 * Rewrite the attribute index of a version 3.9 "Change battle attribute" task
 * action into its version 4.0 equivalent.
 *
 * A 3.9 character has only Stamina, Strength, Defence and (for NPCs) Attitude
 * and Speed -- see the 3.9 BATTLE and NPC_BATTLE schemas above, which have no
 * Accuracy or Agility fields at all.  Its Generator dropdown is therefore the
 * eight-entry list
 *
 *     0 Attitude  1 Stamina  2 Max Stamina  3 Strength  4 Max Strength
 *     5 Defence   6 Max Defence  7 Speed
 *
 * and version 4.0 is that same list with the two new Accuracy and Agility
 * pairs spliced in, so the mapping is 0-4 unchanged, 5->7, 6->8, 7->11.
 * Without this the three moved indices land on Accuracy, Max Accuracy and
 * Defence, and a 3.9 game that hands the player armour instead makes it a
 * better shot.
 *
 * Note that gen400's own 3.9 -> 4.0 conversion agrees on 6->8 and 7->11 but
 * turns 5 into 11 as well -- a cascade of un-chained `If`s that rewrites 5 to
 * 7 and then that 7 to 11.  It is the Generator that is wrong; see
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 3(a).
 */
static void
parse_fixup_v390_battle_attribute (void)
{
  static const scr_int MAPPING[] = {0, 1, 2, 3, 4, 7, 8, 0xB};
  enum { MAPPING_SIZE = sizeof (MAPPING) / sizeof (MAPPING[0]) };

  const scr_int attribute = parse_get_keyed_integer ("Var1");

  if (attribute >= 0 && attribute < MAPPING_SIZE)
    parse_put_keyed_integer ("Var1", MAPPING[attribute]);
}


/*
 * parse_fixup_conditional_increment()
 *
 * Rewrite an integer property as one greater if its current value exceeds
 * the given threshold.  Two version 3.9 fixups are exactly this.
 */
static void
parse_fixup_conditional_increment (const scr_char *key, scr_int threshold)
{
  scr_int value;

  value = parse_get_keyed_integer (key);
  if (value > threshold)
    parse_put_keyed_integer (key, value + 1);
}


/*
 * parse_fixup_openable_key()
 *
 * Common helper for parse_fixup_v390() and parse_fixup_v380(); exchange
 * openable values 5 and 6, and write a -1 Key for openable objects.  A
 * version 3.8 game may also hold the odd Openable value of 1, which is
 * rewritten as zero.
 */
static void
parse_fixup_openable_key (scr_bool rewrite_one_as_zero)
{
  scr_int openable;

  /* Retrieve Openable, and if 5 or 6, exchange. */
  openable = parse_get_keyed_integer ("Openable");
  if (openable == 5 || openable == 6)
    parse_put_keyed_integer ("Openable", (openable == 5) ? 6 : 5);

  /* If the odd value of 1, rewrite as zero. */
  else if (rewrite_one_as_zero && openable == 1)
    parse_put_keyed_integer ("Openable", 0);

  /* For openable objects, store a Key of -1. */
  if (openable == 5 || openable == 6)
    parse_put_keyed_integer ("Key", -1);
}


/*
 * parse_fixup_v390()
 *
 * Handler for fixup special items to help with conversions from TAF version
 * 3.9 format into version 4.0.
 */
void
parse_fixup_v390 (const scr_char *fixup)
{
  if (parse_trace)
    scr_trace ("Parse: entering version 3.9 fixup %s\n", fixup);

  /* Fixup a version 3.9 task action by incrementing Type > 4. */
  if (strcmp (fixup, "|V390_TASK_ACTION:Type>4?#Type++|") == 0)
    parse_fixup_conditional_increment ("Type", 4);

  /* Remap the attribute index of a 3.9 "Change battle attribute" action. */
  else if (strcmp (fixup, "|V390_TASK_ACTION:_BattleAttr_|") == 0)
    parse_fixup_v390_battle_attribute ();

  /* Handle either Expr or Var5 for version 3.9 task actions. */
  else if (strcmp (fixup, "|V390_TASK_ACTION:$Expr_#Var5|") == 0)
    {
      /* Either Expr or Var5, depending on Var2. */
      if (parse_get_keyed_integer ("Var2") == 5)
        parse_descriptor ("$Expr ZVar5");
      else
        parse_descriptor ("EExpr #Var5");
    }

  /*
   * Exchange openable values 5 and 6, and write -1 key for openable objects.
   */
  else if (strcmp (fixup, "|V390_OBJECT:_Openable_,Key|") == 0)
    parse_fixup_openable_key (FALSE);

  /* Create a RestrMask that 'and's all the restrictions together. */
  else if (strcmp (fixup, "|V390_TASK:$RestrMask|") == 0)
    parse_write_restrmask ();

  /*
   * Increment var1 for variable restrictions to compensate for there being no
   * referenced text comparison (no string variables).
   */
  else if (strcmp (fixup, "|V390_TASK_RESTR:Var1>0?#Var1++|") == 0)
    parse_fixup_conditional_increment ("Var1", 0);

  /* Convert version 3.9 fixed alts into a version 4.0 array. */
  else if (strcmp (fixup, "|V390_ROOM:_Alts_|") == 0)
    parse_fixup_v390_v380_room_alts ();

  /* Error if no fixup special handler available. */
  else
    scr_fatal ("parse_fixup_v390: no handler for \"%s\"\n", fixup);

  if (parse_trace)
    scr_trace ("Parse: leaving version 3.9 fixup %s\n", fixup);
}


/*
 * Object surface and container masks for version 3.8 object fixup, container
 * capacity conversion factor and default object sizing, and the count of
 * task movements in a version 3.8 task.
 */
enum { V380_OBJ_IS_SURFACE = 2, V380_OBJ_IS_CONTAINER = 1 };
enum { V380_OBJ_CAPACITY_MULT = 10, V380_OBJ_DEFAULT_SIZE = 2 };
enum { V380_TASK_MOVEMENTS = 6 };

/*
 * Version 3.8 stores a single "Size/weight" class index, 0..4, per object;
 * it is not the version 4.0 packing of size in the tens digit and weight in
 * the units.  Read raw, class 2 becomes weight 3^2*... and class 4 weight
 * 3^4 = 81, which silently exceeds most games' carrying limits, so it cannot
 * be handed to the version 4.0 model as it stands.
 *
 * ADRIFT Generator 3.90 converts the class -- diffing our parse of
 * marooned.taf against a gen390 conversion of the same file gives its exact
 * table:
 *
 *   0 normal     -> 22    3 large       -> 32
 *   1 heavy      -> 23    4 very large  -> 42
 *   2 very heavy -> 24
 *
 * That table is directionally right and wrong by one step, and the genuine
 * run380.exe says why (measured 2026-08-03 with patched probe files, in the
 * Wine Runner harness).  Version 3.8 has no size axis and no weight axis:
 * it has ONE pooled burden, whose per-class costs are 1/3/7/3/7, and the
 * player's capacity is exactly #MaxCarried.  A 4.0 packed base^digit value
 * cannot express a cost of 7, so gen390 rounds the top class up to 3^2 = 9,
 * which is what makes converted 3.8 games stop being finishable.
 *
 * So the class is kept, verbatim, in its own SizeWeightClass key, and the
 * burden model that reads it lives in scobjcts.cpp with the rest of the
 * carrying arithmetic -- see obj_get_burden().  What goes into SizeWeight
 * itself is the 4.0 "normal" 22, which is not a claim about the object: it
 * keeps a 3.8 game's *container* volumes the plain object counts the
 * neighbouring Capacity*10+2 fixup makes them (the pooled burden is a strictly
 * tighter limit on the player, so it subsumes the size axis it leaves behind).
 * How 3.8 containers really charge for their contents has not been measured.
 */
enum { V380_OBJ_NORMAL_SIZEWEIGHT = 22 };

/*
 * parse_fixup_v380_entry()
 * parse_fixup_v380_action()
 * parse_fixup_v380_restr()
 *
 * Helpers for the version 3.8 task fixups, adding a task action or
 * restriction.  Both are written the same way, into the list named by
 * `collection`; the only difference is that a restriction carries a failure
 * message, and every restriction has one.
 */
static void
parse_fixup_v380_entry (const scr_char *collection,
                        scr_int type, scr_int var_count,
                        scr_int var1, scr_int var2, scr_int var3,
                        const scr_char *failmessage)
{
  scr_vartype_t vt_key;
  scr_int entry_count;

  /* Get a count of entries so far defined for the task. */
  entry_count = parse_get_keyed_child_count (collection);

  /* Write the collection key, reversed to emulate parse actions. */
  vt_key.integer = entry_count;
  parse_push_key (vt_key, PROP_KEY_INTEGER);
  vt_key.string = collection;
  parse_push_key (vt_key, PROP_KEY_STRING);

  /* Write the new entry according to the given arguments. */
  parse_put_keyed_integer ("Type", type);
  parse_put_keyed_integer ("Var1", var1);
  if (var_count > 1)
    parse_put_keyed_integer ("Var2", var2);
  if (var_count > 2)
    parse_put_keyed_integer ("Var3", var3);
  if (failmessage)
    parse_put_keyed_string ("FailMessage", failmessage);

  parse_pop_key ();
  parse_pop_key ();
}

static void
parse_fixup_v380_action (scr_int type, scr_int var_count,
                         scr_int var1, scr_int var2, scr_int var3)
{
  parse_fixup_v380_entry ("Actions", type, var_count,
                          var1, var2, var3, nullptr);
}

static void
parse_fixup_v380_restr (scr_int type, scr_int var_count,
                        scr_int var1, scr_int var2, scr_int var3,
                        const scr_char *failmessage)
{
  parse_fixup_v380_entry ("Restrictions", type, var_count,
                          var1, var2, var3, failmessage);
}


/*
 * parse_fixup_v380_movement()
 *
 * Helper for parse_fixup_task_actions(), converts a version 3.8 task movement
 * into an action.
 */
static void
parse_fixup_v380_movement (scr_int mvar1, scr_int mvar2, scr_int mvar3,
                           scr_bool is_v370)
{
  scr_int var1;

  /* If nothing was selected to move, ignore the call. */
  if (mvar1 == 0)
    return;

  /*
   * Accept only player moves into rooms.  Other combinations, such as move
   * player to worn by player, are unlikely.  And move player to same room as
   * player isn't useful.
   */
  if (mvar1 == 1)
    {
      /*
       * The 3.8 Runner's executor (run380 tasks() 44D1D4) loads movement
       * Var2 minus one and moves the player only behind "If Var2 > 1" --
       * meant to exclude the none/hidden entries, but it also excludes
       * room 1, so a 3.8 task can never move the PLAYER to the game's
       * first room; the object branch has no such guard.  Measured live
       * 2026-08-31 on cave.taf run380: "climb down" at the halfway room
       * (raw Var2 = 2) prints its CompleteText and leaves the player in
       * place.  So raw 2 converts to no action at all.
       *
       * Version 3.7 is NOT affected: its player-move encoding sits one
       * higher (the first room is raw Var2 = 3, which its identical
       * "If Var2 > 1" post-decrement guard admits -- run370 tasks()
       * 441E55 = 3.8's guard over 3.7's encoding).  3.8 shifted the
       * encoding down by one and forgot the guard, 3.9 rewrote the
       * executor around typed actions and the bug went away again.  By
       * the time the 3.7 conversion reaches this function its values are
       * already in 3.8 form, so the exclusion must not apply to it --
       * alices_restaurant (arlo.taf, 3.70) moves the player to its first
       * room, In Front of the Church, by task.
       */
      if (mvar3 == 0 && mvar2 >= (is_v370 ? 2 : 3))
        parse_fixup_v380_action (1, 3, 0, 0, mvar2 - 2);
      return;
    }

  /*
   * Convert movement var1 into action var1.  Var1 is the dynamic object + 3,
   * or 2 for referenced object, or 0 for all held.
   */
  switch (mvar1)
    {
    case 2:
      var1 = 2;
      break;                    /* Referenced obj */
    case 3:
      var1 = 0;
      break;                    /* All held */
    default:
      var1 = mvar1 - 1;
      break;                    /* Dynamic obj */
    }

  /* Dissect the rest of the movement. */
  switch (mvar3)
    {
    case 0:                    /* To room */
      /*
       * Convert movement var2 into action var2 and var3.  Var2 is 0 for move
       * to room, 6 for move to player room.  Var3 is 0 for hidden, otherwise
       * the room number plus one.
       */
      if (mvar2 == 0)           /* Hidden */
        parse_fixup_v380_action (0, 3, var1, 0, 0);
      else if (mvar2 == 1)      /* Player room */
        parse_fixup_v380_action (0, 3, var1, 6, 0);
      else                      /* Specified room */
        parse_fixup_v380_action (0, 3, var1, 0, mvar2 - 1);
      break;

    case 1:                    /* To inside */
    case 2:                    /* To onto */
      /*
       * Convert movement var2 and var3 into action var3 and var2, a simple
       * conversion, but check that var2 is not 'not selected' first.
       */
      if (mvar2 > 0)
        parse_fixup_v380_action (0, 3, var1, mvar3 + 1, mvar2 - 1);
      break;

    case 3:                    /* To held by */
    case 4:                    /* To worn by */
      /*
       * Convert movement var2 and var3 into action var3 and var2, in this
       * case a simple conversion, since version 4.0 task actions are close
       * here.
       */
      parse_fixup_v380_action (0, 3, var1, mvar3 + 1, mvar2);
      break;

    default:
      scr_fatal ("parse_fixup_v380_movement: invalid mvar3, %ld\n", mvar3);
    }
}


/*
 * parse_v380_object_to_dynamic()
 *
 * Helper for the version 3.8 restriction fixups; count the dynamic
 * (non-static) objects among objects 0 to last_object inclusive, and
 * return that count less one, converting an object index into an index
 * that considers only dynamic objects.
 */
static scr_int
parse_v380_object_to_dynamic (scr_int last_object)
{
  scr_int object, dynamic;

  dynamic = 0;
  for (object = 0; object <= last_object; object++)
    {
      if (!prop_get_indexed_boolean (parse_bundle, "Objects",
                                     object, "Static"))
        dynamic++;
    }

  return dynamic - 1;
}


/*
 * parse_fixup_v380_obj_restr()
 * parse_fixup_v380_task_restr()
 * parse_fixup_v380_wear_restr()
 * parse_fixup_v380_npc_restr()
 * parse_fixup_v380_objroom_restr()
 * parse_fixup_v380_objstate_restr()
 *
 * Helper handlers for parse_fixup_v380(); create task restrictions.
 */
static void
parse_fixup_v380_obj_restr (scr_bool holding,
                            scr_int holdobj, const scr_char *failmessage)
{
  /* Ignore if no object selected. */
  if (holdobj > 0)
    {
      scr_int var1, var2;

      /*
       * Create version 4.0 task restriction to check for either the
       * referenced object or a dynamic object being either held or in the
       * same room (visible to player).
       */
      var1 = (holdobj == 1) ? 2 : holdobj + 1;
      var2 = holding ? 1 : 3;
      parse_fixup_v380_restr (0, 3, var1, var2, 0, failmessage);
    }
}

static void
parse_fixup_v380_task_restr (scr_bool tasknotdone, scr_int task,
                             const scr_char *failmessage)
{
  /* Ignore if no task selected. */
  if (task > 0)
    {
      scr_int var2;

      /* Create version 4.0 restriction to check task state. */
      var2 = tasknotdone ? 1 : 0;
      parse_fixup_v380_restr (2, 2, task, var2, 0, failmessage);
    }
}

static void
parse_fixup_v380_wear_restr (scr_int wearobj, const scr_char *failmessage)
{
  scr_int object_count, object, dynamic, obj_index;

  /* Ignore if no object selected. */
  if (wearobj <= 0)
    return;

  /*
   * Create version 4.0 restrictions for something or nothing worn by
   * player.
   */
  if (wearobj == 1)
    {
      parse_fixup_v380_restr (0, 3, 1, 2, 0, failmessage);
      return;
    }
  if (wearobj == 2)
    {
      parse_fixup_v380_restr (0, 3, 0, 2, 0, failmessage);
      return;
    }

  /* Get the count of objects defined. */
  object_count = parse_get_top_child_count (parse_bundle, "Objects");

  /* Convert wearobj from worn index to object index. */
  wearobj -= 2;
  for (object = 0; object < object_count && wearobj > 0; object++)
    {
      scr_bool bstatic, wearable;

      bstatic = prop_get_indexed_boolean (parse_bundle, "Objects",
                                          object, "Static");
      if (!bstatic)
        {
          wearable = prop_get_indexed_boolean (parse_bundle, "Objects",
                                               object, "Wearable");
          if (wearable)
            wearobj--;
        }
    }

  /* No such wearable object: no restriction, as for none selected. */
  if (wearobj > 0)
    return;
  obj_index = object - 1;

  /* Now convert wearobj from object index to dynamic index. */
  dynamic = parse_v380_object_to_dynamic (obj_index);

  /* Create version 4.0 restriction for object worn by player. */
  parse_fixup_v380_restr (0, 3, dynamic + 3, 2, 0, failmessage);
}

static void
parse_fixup_v380_npc_restr (scr_bool notinsameroom, scr_int npc,
                            const scr_char *failmessage)
{
  /* Ignore if no NPC selected. */
  if (npc > 0)
    {
      scr_int var2;

      if (npc == 1)
        {
          /* Create restriction to look for alone, or not. */
          var2 = notinsameroom ? 3 : 2;
          parse_fixup_v380_restr (3, 3, 0, var2, 0, failmessage);
          return;
        }

      /* Create restriction to look for company. */
      var2 = notinsameroom ? 1 : 0;
      parse_fixup_v380_restr (3, 3, 0, var2, npc, failmessage);
    }
}

static void
parse_fixup_v380_objroom_restr (scr_int obj, scr_int objroom,
                                const scr_char *failmessage)
{
  /* Ignore if no object selected. */
  if (obj > 0)
    {
      /* Create version 4.0 restriction to check object in room. */
      parse_fixup_v380_restr (0, 3, obj + 1, 0, objroom, failmessage);
    }
}

static void
parse_fixup_v380_objstate_restr (scr_int obj, scr_int ivar1, scr_int ivar2,
                                 const scr_char *failmessage)
{
  scr_int object, dynamic, var2, var3;

  /* Ignore restrictions with no "type". */
  if (ivar1 == 0)
    return;

  /* Look for opened/closed restrictions, convert and return. */
  if (ivar1 == 3 || ivar1 == 4)
    {
      scr_int stateful;

      /* Convert obj from object to openable (stateful) index. */
      stateful = 0;
      for (object = 0; object <= obj - 1; object++)
        {
          scr_int openable;

          openable = prop_get_indexed_integer (parse_bundle, "Objects",
                                               object, "Openable");
          if (openable > 0)
            stateful++;
        }
      stateful--;

      /*
       * Create a version 4.0 restriction that checks that an object's state
       * is open (var2 = 0) or closed (var2 = 1).
       */
      var2 = (ivar1 == 3) ? 0 : 1;
      parse_fixup_v380_restr (1, 2, stateful + 1, var2, 0, failmessage);
      return;
    }

  /*
   * The Runner reads Obj2 - 1 as a raw index into the whole object table
   * (run380 tasks() 44CA87-44CC93, run370 441867), so Obj2 may name a static
   * object.  A static's parent is -1 (run380 openadv 4484F6) and it is never
   * in or held by anything, so every one of these tests fails.  The dynamic
   * conversion below would instead move the test onto the preceding dynamic
   * object.  twilight.taf task 59 `cook *cheese*` names the static stove
   * (Obj2 = 57) "inside container 7" where the cheese was meant; run380
   * answers "You can't do that yet." after `put cheese in stove`
   * (runner_transcripts/twilight.rtf).  Var3 = 0 makes the restriction
   * always false.
   */
  if (obj > 0
      && prop_get_indexed_boolean (parse_bundle, "Objects", obj - 1, "Static"))
    {
      dynamic = parse_v380_object_to_dynamic (obj - 1);
      parse_fixup_v380_restr (0, 3, dynamic >= 0 ? dynamic + 3 : 3, 4, 0,
                              failmessage);
      return;
    }

  /* Convert obj from object to dynamic index. */
  dynamic = parse_v380_object_to_dynamic (obj - 1);

  /* Create version 4.0 object location restrictions for the rest. */
  switch (ivar1)
    {
    case 1:
      var2 = 4;
      var3 = ivar2;
      break;                    /* Inside */
    case 2:
      var2 = 5;
      var3 = ivar2;
      break;                    /* On */
    case 5:
      var2 = 1;
      var3 = ivar2 + 1;
      break;                    /* Held by */
    case 6:
      var2 = 2;
      var3 = ivar2 + 1;
      break;                    /* Worn by */
    default:
      scr_fatal ("parse_fixup_v380_objstate_restr: invalid ivar1, %ld\n",
                 ivar1);
    }
  parse_fixup_v380_restr (0, 3, dynamic + 3, var2, var3, failmessage);
}


/*
 * parse_fixup_v380_task_restrictions()
 *
 * Create version 4.0 task restrictions from a version 3.8 task.
 */
static void
parse_fixup_v380_task_restrictions (void)
{
  scr_bool holding;
  scr_int obj2;
  const scr_char *holdmsg;

  /* Create restrictions for objects not held or absent. */
  holding = parse_get_keyed_boolean ("HoldingSameRoom");
  holdmsg = parse_get_keyed_string ("HoldMsg");
  parse_fixup_v380_obj_restr (holding,
                              parse_get_keyed_integer ("HoldObj1"),
                              holdmsg);
  parse_fixup_v380_obj_restr (holding,
                              parse_get_keyed_integer ("HoldObj2"),
                              holdmsg);
  parse_fixup_v380_obj_restr (holding,
                              parse_get_keyed_integer ("HoldObj3"),
                              holdmsg);

  /* Create any task state restriction. */
  parse_fixup_v380_task_restr (parse_get_keyed_boolean ("TaskNotDone"),
                               parse_get_keyed_integer ("Task"),
                               parse_get_keyed_string ("TaskMsg"));

  /* Create any object not worn restrictions. */
  parse_fixup_v380_wear_restr (parse_get_keyed_integer ("WearObj1"),
                               parse_get_keyed_string ("WearMsg"));
  parse_fixup_v380_wear_restr (parse_get_keyed_integer ("WearObj2"),
                               parse_get_keyed_string ("WearMsg"));

  /* Check for presence/absence of NPCs restriction. */
  parse_fixup_v380_npc_restr (parse_get_keyed_boolean ("NotInSameRoom"),
                              parse_get_keyed_integer ("NPC"),
                              parse_get_keyed_string ("CompanyMsg"));

  /* Create any object location restriction. */
  parse_fixup_v380_objroom_restr (parse_get_keyed_integer ("Obj1"),
                                  parse_get_keyed_integer ("Obj1Room"),
                                  parse_get_keyed_string ("Obj1Msg"));

  /* And finally, any object state restriction. */
  obj2 = parse_get_keyed_integer ("Obj2");
  if (obj2 > 0)
    {
      parse_fixup_v380_objstate_restr (obj2,
                                    parse_get_keyed_integer ("Obj2Var1"),
                                    parse_get_keyed_integer ("Obj2Var2"),
                                    parse_get_keyed_string ("Obj2Msg"));
    }

  /* Mask off the restrictions just created. */
  parse_write_restrmask ();
}


/*
 * parse_fixup_v380_initial_positions()
 *
 * Adjust dynamic object initial positions and parents (where contained
 * or on surfaces) into version 4.0 range.
 *
 * Version 3.7 shares this code.  Its position list is the same one --
 * hidden, held by the player, inside or on the parent object, the rooms,
 * then worn -- but it stops there, with no way to start an object on an
 * NPC, so its Parent is meaningless for the held and worn entries and is
 * forced to the player below.  Measured in the real Runners: run370 gives
 * castle.taf's sweatshirt to the player whatever Parent says, and run380
 * gives tra.taf's red sox hat (Parent 0) and loose change (Parent -1) both
 * to the player, so version 3.8's holder is one-based with zero -- or the
 * unset -1 that fills most files -- meaning the player.
 */
static void
parse_fixup_v380_initial_positions (scr_bool is_v370)
{
  scr_vartype_t vt_key[3];
  scr_int object_count, object, room_count;

  /* Get a count of objects, and of rooms for the "worn" entry. */
  object_count = parse_get_top_child_count (parse_bundle, "Objects");
  room_count = parse_get_top_child_count (parse_bundle, "Rooms");
  vt_key[0].string = "Objects";

  /*
   * Build an array of object container/surface types.  A std::vector
   * frees itself if a prop_*() call below throws (scr_fatal) mid-parse.
   */
  std::vector<scr_int> object_type (parse_checked_count (object_count));
  for (object = 0; object < object_count; object++)
    {
      object_type[object] = prop_get_indexed_integer (parse_bundle,
                                                      "Objects", object,
                                                      "SurfaceContainer");
    }

  /* Adjust each object's initial position if necessary. */
  for (object = 0; object < object_count; object++)
    {
      scr_vartype_t vt_value;
      scr_bool is_static;
      scr_int initialposition;

      /* Ignore static objects; we only want dynamic ones. */
      is_static = prop_get_indexed_boolean (parse_bundle, "Objects",
                                            object, "Static");
      if (is_static)
        continue;

      /* If initial position is above on/in, increment. */
      vt_key[1].integer = object;
      vt_key[2].string = "InitialPosition";
      initialposition = prop_get_integer (parse_bundle, "I<-sis", vt_key);
      if (initialposition > 2)
        {
          vt_value.integer = initialposition + 1;
          prop_put (parse_bundle, "I->sis", vt_value, vt_key);
        }

      /*
       * If held or worn, put the holder into version 4.0 range: zero is
       * the player there too, but an unset -1 is not, and version 3.7
       * has no holder at all.
       */
      if (initialposition == 1 || initialposition == 3 + room_count)
        {
          vt_key[2].string = "Parent";
          if (is_v370 || prop_get_integer (parse_bundle,
                                           "I<-sis", vt_key) < 0)
            {
              vt_value.integer = 0;
              prop_put (parse_bundle, "I->sis", vt_value, vt_key);
            }
          vt_key[2].string = "InitialPosition";
        }

      /*
       * If initial position is on or in, decide which, depending on the
       * type of the parent.  From this, expand initial position into a
       * version 4.0 value.
       */
      if (initialposition == 2)
        {
          scr_int count, parent, index_;

          /* Get parent container/surface index. */
          vt_key[1].integer = object;
          vt_key[2].string = "Parent";
          count = prop_get_integer (parse_bundle, "I<-sis", vt_key);

          /*
           * An unset -1 parent means the first container or surface.
           * Measured in run380: microwaveman.taf's pistol (obj 2, Parent
           * -1) is taken "from some aluminum clothes", obj 0, the first
           * container (runner_probes/microwave_man.run380.rtf).
           * marooned.taf's pill (obj 33) is the other corpus case.  Left
           * at -1, the loop below never runs and parent indexes
           * object_type[-1].
           */
          if (count < 0)
            count = 0;

          /* Convert container/surface index. */
          for (parent = 0; parent < object_count && count >= 0; parent++)
            {
              if (object_type[parent] == V380_OBJ_IS_CONTAINER
                  || object_type[parent] == V380_OBJ_IS_SURFACE)
                count--;
            }
          parent--;

          /*
           * Past the last container or surface: leave Parent as it is, out
           * of range for version 4.0 too, so it names no parent.
           */
          if (count >= 0)
            continue;

          /* If parent is a surface, adjust position. */
          if (object_type[parent] == V380_OBJ_IS_SURFACE)
            {
              vt_key[2].string = "InitialPosition";
              vt_value.integer = initialposition + 1;
              prop_put (parse_bundle, "I->sis", vt_value, vt_key);
            }

          /*
           * For both, adjust parent to be an object index for that type
           * of object only.
           */
          count = 0;
          for (index_ = 0; index_ < parent; index_++)
            {
              if (object_type[index_] == object_type[parent])
                count++;
            }
          vt_key[2].string = "Parent";
          vt_value.integer = count;
          prop_put (parse_bundle, "I->sis", vt_value, vt_key);
        }
    }
}


/*
 * parse_fixup_v380_max_score()
 *
 * Add up positive scoring tasks to arrive at max score.
 */
static void
parse_fixup_v380_max_score (void)
{
  scr_vartype_t vt_key[2], vt_value;
  scr_int task_count, maxscore, task;

  /* Get a count of tasks. */
  task_count = parse_get_top_child_count (parse_bundle, "Tasks");

  /* Sum positive scoring tasks. */
  maxscore = 0;
  for (task = 0; task < task_count; task++)
    {
      scr_int score;

      score = prop_get_indexed_integer (parse_bundle, "Tasks",
                                        task, "Score");
      if (score > 0)
        maxscore += score;
    }

  /* Write MaxScore global property. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "MaxScore";
  vt_value.integer = maxscore;
  prop_put (parse_bundle, "I->ss", vt_value, vt_key);
}


/*
 * parse_fixup_v380_meet_object()
 *
 * Convert walk meetobject from dynamic index to object.
 */
static void
parse_fixup_v380_meet_object (void)
{
  scr_int count, object_count, object;

  /* Get a count of objects. */
  object_count = parse_get_top_child_count (parse_bundle, "Objects");

  /* Convert dynamic index to object, and rewrite. */
  count = parse_get_keyed_integer ("MeetObject") - 1;
  for (object = 0; object < object_count && count >= 0; object++)
    {
      scr_bool bstatic;

      bstatic = prop_get_indexed_boolean (parse_bundle, "Objects",
                                          object, "Static");
      if (!bstatic)
        count--;
    }
  object--;

  /*
   * Store the global index one-based: the runtime reads MeetObject and
   * subtracts one from it (the version 3.9 and 4.0 schemas hold a
   * one-based dynamic index), so writing a zero-based value here would
   * leave the walk's ObjectTask watching the preceding object.  In
   * "House of the Damned" that made the game unwinnable -- the zombie
   * meeting the whisky bottle in the laboratory is what wins it.
   */
  parse_put_keyed_integer ("MeetObject", object + 1);
}


/*
 * parse_fixup_v370_movement()
 *
 * Helper for parse_fixup_task_actions(), converts a version 3.7 task movement
 * into a version 3.8 one, then hands it to the version 3.8 conversion.
 *
 * Version 3.8 splits a movement destination into "where" (Var2) and "how"
 * (Var3, one of to room / to inside / to onto / to held by / to worn by).
 * Version 3.7 has neither: it has one flat destination list per movement,
 * whose first three entries are hidden, held by the player, and the player's
 * room, and whose remaining entries are the rooms.  That is exactly the
 * version 3.8 list with "held by the player" spliced in at index 1, so
 * everything from index 2 up converts by subtracting one, and the two games
 * agree -- castle.taf teleports the player to Var2 22 with twenty rooms
 * (22 - 3 = 19, its treasure room), and arlo.taf tips its garbage into Var2 13
 * with thirty-seven (13 - 3 = 10, its fifteen foot cliff).
 *
 * The list was then measured directly, by handing the genuine 3.70 Runner a
 * castle.taf whose task moves two objects from a distant room to each Var2 in
 * turn: 0 removed them from play, 1 put them into the player's inventory, 2
 * dropped them where the player stood, and 3+n placed them in room n.  Nothing
 * in the list moves an object onto an NPC or into a container, which is why
 * only version 3.8's "to room" and "held by" conversions are needed here.
 */
static void
parse_fixup_v370_movement (scr_int mvar1, scr_int mvar2)
{
  /* If nothing was selected to move, ignore the call. */
  if (mvar1 == 0)
    return;

  /*
   * Moving the player is the one case where the low destinations mean nothing
   * (the player cannot be held or hidden); the version 3.8 conversion ignores
   * them for us, since they all convert to below its own first room.
   */
  if (mvar1 == 1)
    {
      parse_fixup_v380_movement (mvar1, mvar2 - 1, 0, TRUE);
      return;
    }

  switch (mvar2)
    {
    case 0:                    /* Hidden */
      parse_fixup_v380_movement (mvar1, 0, 0, TRUE);
      break;

    case 1:                    /* Held by the player */
      /* Version 3.8's "held by" passes Var2 straight through as the holder,
         and there zero is the player (one is the referenced character).  */
      parse_fixup_v380_movement (mvar1, 0, 3, TRUE);
      break;

    default:                   /* Player's room, or a room */
      parse_fixup_v380_movement (mvar1, mvar2 - 1, 0, TRUE);
      break;
    }
}


/*
 * parse_fixup_task_actions()
 *
 * Shared handler for the version 3.8 and version 3.7 "_Actions_" task
 * fixups: create version 4.0 task actions for the task's score, its game
 * ending flags, and each of its movements.  The two versions differ only in
 * that a version 3.7 task has no WinGame flag (its winning task is named
 * once, in the header) and its movements are pairs rather than triples.
 */
static void
parse_fixup_task_actions (scr_bool is_v370)
{
  scr_vartype_t vt_key;
  scr_int score, movement;

  /* Create any appropriate score change action. */
  score = parse_get_keyed_integer ("Score");
  if (score != 0)
    parse_fixup_v380_action (4, 1, score, 0, 0);

  /* Create any appropriate game ending actions. */
  if (parse_get_keyed_boolean ("KillsPlayer"))
    parse_fixup_v380_action (6, 1, 2, 0, 0);
  if (!is_v370 && parse_get_keyed_boolean ("WinGame"))
    parse_fixup_v380_action (6, 1, 0, 0, 0);

  /* Handle each defined movement for the task. */
  for (movement = 0; movement < V380_TASK_MOVEMENTS; movement++)
    {
      scr_int mvar1, mvar2, mvar3;

      vt_key.integer = movement;
      parse_push_key (vt_key, PROP_KEY_INTEGER);
      vt_key.string = "Movements";
      parse_push_key (vt_key, PROP_KEY_STRING);

      /* Retrieve the movement parameters. */
      mvar1 = parse_get_keyed_integer ("Var1");
      mvar2 = parse_get_keyed_integer ("Var2");
      mvar3 = is_v370 ? 0 : parse_get_keyed_integer ("Var3");

      parse_pop_key ();
      parse_pop_key ();

      /* Create the corresponding task action. */
      if (is_v370)
        parse_fixup_v370_movement (mvar1, mvar2);
      else
        parse_fixup_v380_movement (mvar1, mvar2, mvar3, FALSE);
    }
}


/*
 * parse_fixup_v380()
 *
 * Handler for fixup special items to help with conversions from TAF version
 * 3.8 format into version 4.0.
 */
void
parse_fixup_v380 (const scr_char *fixup)
{
  if (parse_trace)
    scr_trace ("Parse: entering version 3.8 fixup %s\n", fixup);

  /* Convert container capacity attributes to version 4.0 values. */
  if (strcmp (fixup, "|V380_OBJECT:#Capacity*10+2|") == 0)
    {
      /* Convert capacity from version 3.8 format to version 4.0. */
      if (parse_get_keyed_integer ("SurfaceContainer") == V380_OBJ_IS_CONTAINER)
        {
          parse_put_keyed_integer ("Capacity",
                                   parse_get_keyed_integer ("Capacity")
                                   * V380_OBJ_CAPACITY_MULT
                                   + V380_OBJ_DEFAULT_SIZE);
        }
    }

  /*
   * Keep the 3.8 size/weight class in its own key for the burden model, and
   * hand the version 4.0 fields a "normal" object (see the note on
   * V380_OBJ_NORMAL_SIZEWEIGHT above).
   */
  else if (strcmp (fixup, "|V380_OBJECT:_SizeWeight_|") == 0)
    {
      parse_put_keyed_integer ("SizeWeightClass",
                               parse_get_keyed_integer ("SizeWeight"));
      parse_put_keyed_integer ("SizeWeight", V380_OBJ_NORMAL_SIZEWEIGHT);
    }

  /*
   * Exchange openable values 5 and 6, watch for a possible 1 from a 3.8 game
   * (interpret as 0), and write -1 key for openable objects.
   */
  else if (strcmp (fixup, "|V380_OBJECT:_Openable_,Key|") == 0)
    parse_fixup_openable_key (TRUE);

  /* Create version 4.0 task actions from a version 3.8 task. */
  else if (strcmp (fixup, "|V380_TASK:_Actions_|") == 0)
    parse_fixup_task_actions (FALSE);

  /* Create version 4.0 task restrictions from a version 3.8 task. */
  else if (strcmp (fixup, "|V380_TASK:_Restrictions_|") == 0)
    parse_fixup_v380_task_restrictions ();

  /* Adjust dynamic object initial positions and parents into 4.0 range. */
  else if (strcmp (fixup, "|V380_OBJECT:_InitialPositions_|") == 0)
    parse_fixup_v380_initial_positions (FALSE);
  else if (strcmp (fixup, "|V370_OBJECT:_InitialPositions_|") == 0)
    parse_fixup_v380_initial_positions (TRUE);

  /*
   * Turn on the 3.8 pooled burden model, and convert the carry limit into
   * version 4.0-like size and weight limits as well.  The burden limit is
   * #MaxCarried itself; the 4.0-shaped pair is what the save serialiser and
   * the container arithmetic still read (and, being an object count against
   * normalised objects, it can never bind before the burden does).
   */
  else if (strcmp (fixup, "|V380_MaxSize_MaxWt_|") == 0)
    {
      scr_int limit;

      limit = parse_get_keyed_integer ("MaxCarried") * V380_OBJ_CAPACITY_MULT
              + V380_OBJ_DEFAULT_SIZE;

      parse_put_keyed_integer ("MaxSize", limit);
      parse_put_keyed_integer ("MaxWt", limit);
      parse_put_keyed_integer ("BurdenModel", TRUE);
    }

  /* Add up positive scoring tasks to arrive at max score. */
  else if (strcmp (fixup, "|V380_GLOBAL:_MaxScore_|") == 0)
    parse_fixup_v380_max_score ();

  /* Convert walk meetobject from dynamic index to object. */
  else if (strcmp (fixup, "|V380_WALK:_MeetObject_|") == 0)
    parse_fixup_v380_meet_object ();

  /* Convert version 3.8 room data into a version 4.0 alts array. */
  else if (strcmp (fixup, "|V380_ROOM:_Alts_|") == 0)
    parse_fixup_v390_v380_room_alts ();

  /* Error if no fixup special handler available. */
  else
    scr_fatal ("parse_fixup_v380: no handler for \"%s\"\n", fixup);

  if (parse_trace)
    scr_trace ("Parse: leaving version 3.8 fixup %s\n", fixup);
}


/*
 * The seventeen built-in commands that a version 3.7 game stores, in file
 * order, with the words ADRIFT 3.7 itself uses for them.  The block is the
 * whole of version 3.7's synonym support -- "basic synonyms ... for common
 * commands", added in Adventure Generator 3.31 -- and an author may rewrite
 * any entry: arlo.taf has "look at" where castle.taf has the standard
 * "examine".  A rewritten direction, look, inventory or help entry becomes
 * a version 4.0 synonym that maps the author's word back onto the standard
 * one, which is what the library commands answer to; goto and the five
 * object handlers read their slots themselves.
 */
static const scr_char *const V370_COMMANDS[] = {
  "north", "east", "south", "west", "up", "down", "in", "out",
  "look", "inventory", "examine", "pick up", "put down", "wear",
  "remove", "goto", "help"
};
enum
{ V370_COMMANDS_SIZE = sizeof (V370_COMMANDS) / sizeof (V370_COMMANDS[0])
};


/*
 * parse_fixup_v370()
 *
 * Handler for fixup special items in version 3.7 files.  Version 3.7 is
 * version 3.8 in all but the few places listed at V370_PARSE_SCHEMA, so
 * anything not handled here is a version 3.8 fixup.
 */
void
parse_fixup_v370 (const scr_char *fixup)
{
  if (parse_trace)
    scr_trace ("Parse: entering version 3.7 fixup %s\n", fixup);

  /*
   * Create version 4.0 task actions from a version 3.7 task.  As version 3.8,
   * except that movements are pairs, and that a task cannot win the game on
   * its own account -- the winning task is named once, in the header.
   */
  if (strcmp (fixup, "|V370_TASK:_Actions_|") == 0)
    parse_fixup_task_actions (TRUE);

  /*
   * Turn any renamed built-in command into a version 4.0 synonym.  Commands
   * left at their standard word need none.
   */
  else if (strcmp (fixup, "|V370_GLOBAL:_Synonyms_|") == 0)
    {
      scr_vartype_t vt_key[3], vt_value;
      scr_int command_count, command, synonyms;

      /* Get a count of stored commands. */
      command_count = parse_get_top_child_count (parse_bundle, "Commands");

      synonyms = 0;
      for (command = 0;
           command < command_count && command < V370_COMMANDS_SIZE; command++)
        {
          const scr_char *word;

          word = prop_get_indexed_string (parse_bundle, "Commands",
                                          command, "Word");

          /*
           * Ignore an unset or unchanged command word.  A renamed "goto" is
           * no synonym: run370's gotoplace tests the author's word itself
           * alongside "goto" and cuts its own length -- see
           * lib_cmd_go_place().  Nor are examine, take, drop, wear and
           * remove (slots 10-14): each handler tests the author's word
           * beside its own, and the line keeps it for the tasks and for
           * object names -- see lib_pre400_handler_words().
           */
          if (!word || word[0] == NUL
              || strcmp (word, V370_COMMANDS[command]) == 0
              || strcmp (V370_COMMANDS[command], "goto") == 0
              || (command >= 10 && command <= 14))
            continue;

          /* Rewrite the author's word back into the standard one. */
          vt_key[0].string = "Synonyms";
          vt_key[1].integer = synonyms;
          vt_key[2].string = "Original";
          vt_value.string = word;
          prop_put (parse_bundle, "S->sis", vt_value, vt_key);
          vt_key[2].string = "Replacement";
          vt_value.string = V370_COMMANDS[command];
          prop_put (parse_bundle, "S->sis", vt_value, vt_key);

          synonyms++;
        }

      if (parse_trace)
        scr_trace ("Parse: 3.7 renamed commands, %ld\n", synonyms);
    }

  /*
   * Give the game's winning task the "end game, win" action that version 3.8
   * would have carried in the task itself.  The header holds a plain 0-based
   * task index; anything outside the tasks is taken as "no winning task".
   */
  else if (strcmp (fixup, "|V370_TASK:_WinTask_|") == 0)
    {
      scr_vartype_t vt_key[2], vt_push;
      scr_int wintask, task_count;

      vt_key[0].string = "Header";
      vt_key[1].string = "WinTask";
      wintask = prop_get_integer (parse_bundle, "I<-ss", vt_key);

      task_count = parse_get_top_child_count (parse_bundle, "Tasks");

      if (wintask >= 0 && wintask < task_count)
        {
          /* Key the task, reversed as parse actions leave the stack. */
          vt_push.integer = wintask;
          parse_push_key (vt_push, PROP_KEY_INTEGER);
          vt_push.string = "Tasks";
          parse_push_key (vt_push, PROP_KEY_STRING);

          parse_fixup_v380_action (6, 1, 0, 0, 0);

          parse_pop_key ();
          parse_pop_key ();

          if (parse_trace)
            scr_trace ("Parse: 3.7 winning task is %ld\n", wintask);
        }
      else if (parse_trace)
        scr_trace ("Parse: 3.7 game has no winning task\n");
    }

  /* Anything else is a version 3.8 fixup. */
  else
    parse_fixup_v380 (fixup);

  if (parse_trace)
    scr_trace ("Parse: leaving version 3.7 fixup %s\n", fixup);
}


/*
 * parse_fixup_v400()
 *
 * Handler for fixup special items in native version 4.0 files.
 */
void
parse_fixup_v400 (const scr_char *fixup)
{
  if (parse_trace)
    scr_trace ("Parse: entering version 4.0 fixup %s\n", fixup);

  /*
   * Read three Var fields for a task restriction whose Type is outside the
   * documented 0-4 range.
   *
   * Sophie's Adventure (IFComp 2003 release, sophie.taf) contains one such
   * record: a restriction with Type 12 and Vars 7667826/7209070/7471205.  Those
   * three integers are the four-byte little-endian halves of the UTF-16 string
   * "runner", and 12 is its byte length -- a stray string blob that some tool
   * wrote over the restriction.  The real run400.exe loads and plays the game
   * regardless, so it consumes three Vars here and ignores the unknown type;
   * without this, the parse slides three lines and dies shortly after on
   * Tasks/4489/Actions/1/Type.  Field count inferred from that one sample,
   * which is the only occurrence anywhere in the walkthrough corpus.
   */
  if (strcmp (fixup, "|V400_TASK_RESTR:Type>4?#Var1,#Var2,#Var3|") == 0)
    {
      if (parse_get_keyed_integer ("Type") > 4)
        parse_descriptor ("#Var1 #Var2 #Var3");
    }

  /* Error if no fixup special handler available. */
  else
    scr_fatal ("parse_fixup_v400: no handler for \"%s\"\n", fixup);

  if (parse_trace)
    scr_trace ("Parse: leaving version 4.0 fixup %s\n", fixup);
}

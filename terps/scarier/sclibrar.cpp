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
 * o Ensure module messages precisely match the real Runner ones.  This
 *   matters for ALRs.
 *
 * o Capacity checks on the player and on containers are implemented, but
 *   may not be right.
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NUL = '\0';
static const scr_char COMMA = ',';
enum
{ SECS_PER_MINUTE = 60,
  MINS_PER_HOUR = 60,
  SECS_PER_HOUR = 3600
};
enum { LIB_ALLOCATION_AVOIDANCE_SIZE = 128 };

/*
 * A gathered list of objects, NPCs, or directions, and the printer used for
 * one of its elements.  See lib_print_list() below.
 */
typedef std::vector<scr_int> lib_list_t;
typedef void (*lib_print_item_t) (scr_gameref_t game, scr_int item);

/* Trace flag, set before running. */
static scr_bool lib_trace = FALSE;


/*
 * lib_warn_battle_system()
 *
 * Display a warning when the battle system is detected in a game.  Print
 * directly rather than using the printfilter to avoid possible clashes
 * with ALRs.
 */
void
lib_warn_battle_system (void)
{
  if_print_tag (SCR_TAG_FONT, "size=16");
  if_print_string ("SCARIER WARNING");
  if_print_tag (SCR_TAG_ENDFONT, "");

  if_print_string (
    "\n\nThe game uses Adrift's Battle System, something not fully supported"
    " by this release of SCARIER.\n\n");

  if_print_string (
    "SCARIER will still run the game, but it will not create character"
    " battles where they would normally occur.  For some games, this may"
    " be perfectly okay, as the Battle System is sometimes turned on"
    " by accident in a game, but never actually used.  For others, though,"
    " the omission of this feature may be more serious.\n\n");

  if_print_string ("Please press a key to continue...\n\n");
  if_print_tag (SCR_TAG_WAITKEY, "");
}


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
 * lib_use_room_alt()
 *
 * Return TRUE if a particular alternate room description should be used.
 */
static scr_bool
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
      {
        scr_int var2, var3;

        vt_key[4].string = "Var2";
        var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
        if (var2 == 0)          /* No task. */
          retval = TRUE;
        else
          {
            vt_key[4].string = "Var3";
            var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

            retval = gs_task_done (game, var2 - 1) == !(var3 != 0);
          }
        break;
      }

    case 1:                    /* Stateful object. */
      {
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
        break;
      }

    case 2:                    /* Player condition. */
      {
        scr_int var2, var3, object;

        vt_key[4].string = "Var2";
        var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
        vt_key[4].string = "Var3";
        var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

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
             */
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
            break;
          }

        if (var2 == 2 || var2 == 3)
          object = obj_wearable_object (game, var3 - 1);
        else
          object = obj_dynamic_object (game, var3 - 1);

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
             */
            retval = !gs_runner_possessed (game, object);
            break;
          case 1:              /* Is holding (or wearing). */
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
        break;
      }

    default:
      scr_fatal ("lib_use_room_alt: invalid type, %ld\n", type);
    }

  return retval;
}


/*
 * lib_find_starting_alt()
 *
 * Return the alt index for the alt at which we need to start running down
 * the alts list when generating room names or descriptions.  Returns -1 if
 * no alt overrides the default room long description.
 */
static scr_int
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
   * Run forwards through all alts lower than our starting point, or all alts
   * if no starting point found.
   */
  for (alt = (start != -1) ? start : 0; alt < alt_count; alt++)
    {
      /* Ignore all non-method-2 alts except for the starter. */
      if (alt != start)
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
static void
lib_print_room_name_lower (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::string name (lib_get_room_name (game, room));

  for (std::string::size_type index_ = 0; index_ < name.size (); index_++)
    name[index_] = scr_tolower (name[index_]);
  pf_buffer_string (filter, name.c_str ());
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
   *     Inside the Top Hat  A bunny twitches its whiskers at me  <- SCARIER
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
 * lib_print_object_np
 * lib_print_object
 *
 * Convenience functions to print out an object's name, with a "normalized"
 * prefix -- any "a"/"an"/"some" is replaced by "the" -- and with the full
 * prefix.
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
 * some bone." in run370.  See RUNNER_TESTS_TODO.md section 4.
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
 * Adrift_940_pfx.txt), eleven objects one per spelling:
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
 */
static scr_bool
lib_compare_article (const scr_char *string, const scr_char *word,
                     scr_int length)
{
  assert (string && word);

  return strncmp (string, word, length) == 0
         && (string[length] == NUL || scr_isspace (string[length]));
}


void
lib_print_object_np (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3], vt_version[1];
  const scr_char *prefix, *normalized, *name;

  vt_version[0].string = "Version";

  /* Get the object's prefix. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

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
   * divergence table in RUNNER_TESTS_TODO.md, settled 2026-08-14.
   */
  normalized = prefix;
  if (prop_get_integer (bundle, "I<-s", vt_version) < TAF_VERSION_390)
    {
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
       */
      if (strcmp (prefix, "a") == 0)
        pf_buffer_string (filter, "the ");
      else if (strncmp (prefix, "a ", 2) == 0)
        {
          pf_buffer_string (filter, "the ");
          pf_buffer_string (filter, prefix + 2);
          pf_buffer_character (filter, ' ');
        }
      else if (strncmp (prefix, "an ", 3) == 0)
        {
          pf_buffer_string (filter, "the ");
          pf_buffer_string (filter, prefix + 3);
          pf_buffer_character (filter, ' ');
        }
      else
        {
          pf_buffer_string (filter, prefix);
          pf_buffer_character (filter, ' ');
        }
    }
  else if (lib_compare_article (prefix, "a", 1))
    {
      normalized = prefix + 1;
      pf_buffer_string (filter, "the");
    }
  else if (lib_compare_article (prefix, "an", 2))
    {
      normalized = prefix + 2;
      pf_buffer_string (filter, "the");
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
   * (Adrift_22_xfiles.txt, 2026-08-25).  Falling through leaves the prefix in
   * "normalized", which the tail below prints verbatim, so all this branch
   * ever did was lower-case the author's capital.
   *
   * 3.9 is bracketed rather than read: the run390 decompilation does not
   * reach its normalizer.  Both neighbours leave "the" alone, and the only
   * thing 3.9 is known to have added to tense is the "some" pair, so the
   * 3.90 games in the corpus (The Spirit's Flight, whose Spirit Dagger and
   * Orb of Storms both carry a "The" prefix) follow 4.0 here.
   */
  else if (lib_compare_article (prefix, "some", 4))
    {
      normalized = prefix + 4;
      pf_buffer_string (filter, "the");
    }

  /*
   * Print what the normalizer left of the prefix, and a separating space.
   * Both are unconditional: the Runner's name builder is a plain
   * `tense(Prefix) & " " & Short` concatenation, so a prefix it hands back
   * empty still costs a space.  That case is only reachable at all for an
   * authored whitespace-only prefix, which the loader trims to nothing after
   * declining to substitute an "a" into it -- probe ISARE's single-space
   * rings answers `where rings` with " rings are test arena.", leading space
   * and no article (run400, Adrift_isare.txt, 2026-09-07).  The pre-3.9
   * branch above has already emitted its own prefix and separator.
   */
  if (prop_get_integer (bundle, "I<-s", vt_version) >= TAF_VERSION_390)
    {
      pf_buffer_string (filter, normalized);
      pf_buffer_character (filter, ' ');
    }

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
   */
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, name);
}

static void
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
 */
static void
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
 */
static scr_int
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
static scr_bool
lib_is_version_400 (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400;
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
static void
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
static scr_bool
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
 * literal, present unchanged in all four Runners.  See RUNNER_TESTS_TODO.md
 * section 4.
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
 */
static const scr_char *
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
      response = third_person;
      break;
    default:
      scr_error ("lib_select_response:"
                " unknown perspective, %ld\n", perspective);
      response = second_person;
      break;
    }

  return response;
}

static const scr_char *
lib_select_plurality (scr_gameref_t game, scr_int object,
                      const scr_char *singular, const scr_char *plural)
{
  return obj_appears_plural (game, object) ? plural : singular;
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
static void
lib_print_wrapped_object (scr_gameref_t game, const scr_char *prefix,
                          scr_int object, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, prefix);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, suffix);
}

static void
lib_print_wrapped_npc (scr_gameref_t game, const scr_char *prefix,
                       scr_int npc, const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, prefix);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, suffix);
}

static void
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

static void
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
static scr_bool
lib_print_message (scr_gameref_t game, const scr_char *message)
{
  pf_buffer_string (gs_get_filter (game), message);
  return TRUE;
}

static scr_bool
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
static void
lib_new_clause (scr_gameref_t game, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (has_printed)
    pf_buffer_string (filter, "  ");
  pf_new_sentence (filter);
}

static void
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
 * lib_print_list()
 * lib_print_name_list()
 *
 * Print a gathered list as "a, b, c and d", the last separator being the
 * given conjunction -- " and " for most lists, " or " where the game is
 * offering a choice.  Callers collect into a lib_list_t first rather than
 * printing as they iterate, so that the clause introducing the list can be
 * chosen from the finished list rather than reconstructed on the fly.
 */
static void
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

static void
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
static scr_bool
lib_print_object_list (scr_gameref_t game, scr_bool has_printed,
                       const lib_list_t &list,
                       const scr_char *conjunction, scr_char terminator,
                       const scr_char *second_person,
                       const scr_char *first_person,
                       const scr_char *third_person,
                       lib_print_item_t print_item = lib_print_object_np)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (list.empty ())
    return FALSE;

  lib_print_clause (game, has_printed,
                    second_person, first_person, third_person);
  lib_print_list (game, list, print_item, conjunction);
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
static const scr_char *
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
enum { LIB_NPC_HERE_LENGTH = 9 };       /* strlen (" is here.") */

static scr_bool
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
 * (measured in run400 under Wine 2026-09-05, Adrift_46.txt turn 11).
 */
static scr_bool
lib_inroomdesc_is_absent (const scr_char *inroomdesc)
{
  return inroomdesc == NULL || inroomdesc[0] == NUL;
}

/*
 * lib_print_room_contents()
 *
 * Print a list of the contents of a room.
 */
static void
lib_print_room_contents (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *entry_buffer = pf_get_buffer (filter);
  const size_t entry_length = entry_buffer ? strlen (entry_buffer) : 0;
  scr_vartype_t vt_key[4];
  scr_int object, npc;
  lib_list_t list;

  /*
   * List all objects that show their initial description.
   *
   * The Runner's room lister (run400 @00472515, re-checked by its helper
   * @00449B6C) prints an object's InRoomDesc only when ListFlag matches the
   * object's kind: a dynamic object needs ListFlag clear, while a STATIC
   * object needs ListFlag ("specifically list") SET.  A static object with
   * an InRoomDesc but no ListFlag prints nothing -- authors use that for
   * text mirrored in the room description itself (Goldilocks' hall trapdoor,
   * whose "[TRAP=...]" sentence is already part of the room's long text).
   * Pre-4.0 games have no InRoomDesc property at all, so the gate only ever
   * bites on version 4.0 games.
   */
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (obj_directly_in_room (game, object, room))
        {
          const scr_char *inroomdesc;
          scr_bool listflag;

          vt_key[0].string = "Objects";
          vt_key[1].integer = object;
          vt_key[2].string = "ListFlag";
          listflag = prop_get_boolean (bundle, "B<-sis", vt_key);
          if (listflag != obj_is_static (game, object))
            continue;

          /* Find and print in room description. */
          vt_key[2].string = "InRoomDesc";
          inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);
          if (!obj_shows_initial_description (game, object, room,
                                              lib_inroomdesc_is_absent
                                                (inroomdesc)))
            continue;
          if (!lib_inroomdesc_is_absent (inroomdesc))
            {
              pf_buffer_join (filter, inroomdesc);
            }
        }
    }

  /*
   * List dynamic objects directly located in the room, and not already listed
   * above since they lack, or suppress, an in room description.
   *
   * If an object sets ListFlag, then if dynamic it's suppressed from the list
   * where it would normally be included, but if static it's included where it
   * would normally be excluded.
   */
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (obj_directly_in_room (game, object, room))
        {
          const scr_char *inroomdesc;

          vt_key[0].string = "Objects";
          vt_key[1].integer = object;
          vt_key[2].string = "InRoomDesc";
          inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);

          if (!obj_shows_initial_description (game, object, room,
                                              lib_inroomdesc_is_absent
                                                (inroomdesc)))
            {
              scr_bool listflag;

              vt_key[2].string = "ListFlag";
              listflag = prop_get_boolean (bundle, "B<-sis", vt_key);

              if (listflag == obj_is_static (game, object))
                list.push_back (object);
            }
        }
    }
  if (!list.empty ())
    {
      /*
       * The two spaces are the Runner's own, hard-coded into the literal
       * "  Also here" it appends (run400 @00472696) -- not a pspace() call,
       * so they go in even after a description that already ended in a
       * break.  Take back our section terminator first; it stands where the
       * Runner's string simply carried on.
       */
      pf_undo_auto_break (filter);
      pf_buffer_string (filter,
                        lib_select_plurality (game, list[0],
                                              "  Also here is ",
                                              "  Also here are "));
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_string (filter, ".");
    }

  /*
   * List the NPCs in the room.  A "#" in-room text asks for the default
   * "<name> is here.", and the Runner splits the room's characters into the
   * ones saying exactly that -- joined into one sentence -- and the ones with
   * something of their own to say.
   *
   * Two things about that split are not what they look like, both measured
   * live in run400 (RUNNER_TESTS_TODO.md section 9).  The "#" substitution
   * happens in the *loader* (@00091EDF): the text simply becomes "<name> is
   * here." before the game starts, and by the time the room lister
   * (@00072944) runs there is no "#" left to test for.  What it tests instead
   * is the tail -- Right(text, 9) = " is here." -- so a character whose text
   * the author wrote out in full joins the sentence too, contributing the text
   * with those nine characters trimmed off rather than its own name.  Probe
   * NPCs "Delta" ("Delta is here.") and "Foxtrot" ("The stranger is here.")
   * come out as "Alpha, Charlie, Delta and The stranger are here.", so it is
   * the text that is trimmed and not the name that is looked up.  The test is
   * exact and case-sensitive: "Golf is here!" and "Hotel IS HERE." both stay
   * in the second group.
   *
   * And the joined sentence comes *first*, ahead of the characters with their
   * own text, which is the other half of what Scarier had backwards.
   */
  {
    std::vector<std::string> joined;

    for (npc = 0; npc < gs_npc_count (game); npc++)
      {
        const scr_char *description;

        if (!npc_in_room (game, npc, room))
          continue;

        description = lib_get_npc_inroom_text (game, npc);
        if (!scr_strcasecmp (description, "#"))
          {
            joined.push_back (prop_get_indexed_string (bundle, "NPCs",
                                                       npc, "Name"));
          }
        else
          {
            /*
             * Trim the suffix to get the name the Runner joins in.  Both the
             * test and the trim run on the text exactly as the author wrote
             * it: run400 @004729A7 asks Right(text, 9) = " is here." and
             * @004729FE takes Left(text, Len(text) - 9), and neither looks
             * past a leading break.  So a character whose in-room text opens
             * with "<br>" keeps that break, and it lands *after* the two
             * separator spaces rather than instead of them --
             * Adrift_226_spooked.txt lines 120-122 show the room
             * description's trailing "  ", then the author's blank line, then
             * "Samuel, your scientist pal, is here."
             */
            if (lib_npc_text_is_default (description))
              {
                joined.push_back (std::string (description,
                                               strlen (description)
                                               - LIB_NPC_HERE_LENGTH));
              }
          }
      }

    if (!joined.empty ())
      {
        size_t index_;

        /*
         * Two spaces, hard-coded like the object list's (run400 @0047295B),
         * and again not a pspace() call -- the Runner puts them in whatever
         * the string already ends with.  Take back our own terminator first.
         */
        pf_undo_auto_break (filter);
        pf_buffer_string (filter, "  ");
        pf_new_sentence (filter);
        for (index_ = 0; index_ < joined.size (); index_++)
          {
            if (index_ > 0)
              {
                pf_buffer_string (filter,
                                  index_ == joined.size () - 1
                                  ? " and " : ", ");
              }
            pf_buffer_string (filter, joined[index_].c_str ());
          }
        pf_buffer_string (filter,
                          joined.size () == 1 ? " is here" : " are here");
        pf_buffer_string (filter, ".");
      }
  }

  /* List NPCs directly in the room that have an in room description. */
  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      if (npc_in_room (game, npc, room))
        {
          const scr_char *description;

          /*
           * Print any text not already folded into the sentence above.  The
           * test has to be the same one the collection loop made, on the same
           * raw text, or a character would either be listed twice or vanish.
           */
          description = lib_get_npc_inroom_text (game, npc);
          if (!scr_strempty (description)
              && scr_strcasecmp (description, "#")
              && !lib_npc_text_is_default (description))
            {
              /*
               * Authors typically begin a character's InRoomText with a line
               * break -- a literal newline or a "<br>" tag -- so that the
               * character appears on a line of its own.  The Runner keeps
               * that break and adds nothing of its own beyond pspace()'s
               * clause gap (run400 @00472B01 calls it, @00472B06 appends the
               * text verbatim), so the break the author wrote is the one that
               * does the spacing.  pf_buffer_join() is pspace(), plus taking
               * back our own section terminator, which the Runner's string
               * never had.
               */
              pf_buffer_join (filter, description);
            }
        }
    }

  /*
   * Terminate the room block, and record the newline as ours: everything
   * appended after it -- an event's LookText, a task's message, the next
   * turn's text -- joins onto the Runner's single room string, so whoever
   * comes next may take this break back again.
   */
  {
    const scr_char *buffered = pf_get_buffer (filter);

    if (buffered && strlen (buffered) > entry_length
        && !pf_text_ends_with_break (buffered))
      {
        pf_buffer_character (filter, '\n');
        pf_note_trailing_auto_break (filter);
      }
  }
}


/*
 * lib_print_room_description()
 *
 * Print out the long description for a given room.
 */
void
lib_print_room_description (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_bool showobjects, is_described, is_suppressed, looked;
  scr_int alt_count, alt, start, event;

  /* Get count of room alternates. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";
  alt_count = prop_get_child_count (bundle, "I<-sis", vt_key);

  /* Start with no description, and get our starting point in the alts list. */
  is_described = FALSE;
  start = lib_find_starting_alt (game, room);

  /* Print the standard description unless a start alt indicates not. */
  if (start == -1)
    is_suppressed = FALSE;
  else
    {
      scr_int method;

      vt_key[3].integer = start;
      vt_key[4].string = "DisplayRoom";
      method = prop_get_integer (bundle, "I<-sisis", vt_key);

      is_suppressed = (method == 0);
    }
  if (!is_suppressed)
    {
      const scr_char *description;

      vt_key[0].string = "Rooms";
      vt_key[1].integer = room;
      vt_key[2].string = "Long";
      description = prop_get_string (bundle, "S<-sis", vt_key);
      if (!scr_strempty (description))
        {
          pf_buffer_string (filter, description);
          is_described = TRUE;
        }
      else if (prop_get_taf_version (bundle) == TAF_VERSION_380)
        {
          /*
           * 3.8 substitutes "There is nothing of interest here." into an
           * empty Long at LOAD time (run380 447FEE), so the sentence prints
           * wherever the Long would -- even when an alt goes on to describe
           * the room.  Measured live 2026-08-31 on cave.taf (3.80): "By the
           * old shack" and "By the large tree" have empty Longs and a
           * LastDesc alt, and run380 prints "There is nothing of interest
           * here.  You are standing..." on every show.  3.9's render-time
           * guard below fires only when nothing else described the room.
           */
          pf_buffer_string (filter, "There is nothing of interest here.");
          is_described = TRUE;
        }

      vt_key[2].string = "Res";
      res_handle_resource (game, "sis", vt_key);
    }

  /* Ensure that we're back to handling room alts. */
  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Alts";

  /*
   * Run forwards through all alts lower than our starting point, or all alts
   * if no starting point overrider found.
   */
  showobjects = TRUE;
  for (alt = (start != -1) ? start : 0; alt < alt_count; alt++)
    {
      /* Ignore all non-method-2 alts except for the starter. */
      if (alt != start)
        {
          scr_int method;

          vt_key[3].integer = alt;
          vt_key[4].string = "DisplayRoom";
          method = prop_get_integer (bundle, "I<-sisis", vt_key);

          if (method != 2)
            continue;
        }

      if (lib_use_room_alt (game, room, alt))
        {
          const scr_char *m1;
          scr_int hideobjects;

          vt_key[3].integer = alt;
          vt_key[4].string = "M1";
          m1 = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (m1))
            {
              if (is_described)
                pf_buffer_string (filter, "  ");
              pf_buffer_string (filter, m1);
              is_described = TRUE;
            }

          vt_key[4].string = "Res1";
          res_handle_resource (game, "sisis", vt_key);

          vt_key[4].string = "HideObjects";
          hideobjects = prop_get_integer (bundle, "I<-sisis", vt_key);
          if (hideobjects == 1)
            showobjects = FALSE;
        }
      else
        {
          const scr_char *m2;

          vt_key[3].integer = alt;
          vt_key[4].string = "M2";
          m2 = prop_get_string (bundle, "S<-sisis", vt_key);
          if (!scr_strempty (m2))
            {
              if (is_described)
                pf_buffer_string (filter, "  ");
              pf_buffer_string (filter, m2);
              is_described = TRUE;
            }

          vt_key[4].string = "Res2";
          res_handle_resource (game, "sisis", vt_key);
        }
    }

  /*
   * A room that ends up with no description at all.
   *
   * 3.8 and 3.9 both supply one.  3.9 does it while rendering: viewroom()
   * appends "There is nothing of interest here." when the branch's own
   * alternative text is empty AND the room's Long is empty -- the same
   * guarded shape at all four of its exits (run390 4478CA/4479A5/447A3A/
   * 447ACF, each preceded by a `<alt string> <> vbNullString` branch that
   * jumps clean past it).  3.8 does it at LOAD time instead, substituting the
   * sentence into the empty Long itself (run380 447FEE) -- and cave.taf
   * (measured live 2026-08-31) proves the routes DIVERGE when an alt
   * describes the room: 3.8 prints the substitute in the Long's place AND
   * the alt after it, where this render-time guard stayed silent.  The 3.80
   * case therefore now lives up at the Long print; only 3.90 keeps this
   * guarded tail.  3.7 has no such string and leaves the
   * room blank; 4.0 dropped it again.
   *
   * Read the guard, not the literal: hanging the sentence off an empty Long
   * alone moves 16 of the 303 corpus goldens.  Gating it on "nothing has
   * described this room yet" moves exactly two, yeh and richard, both 3.90 --
   * which is the shape a correct fix should have.
   *
   * Measured on p39EXAM.taf (3.90), Adrift_41_p39exam.txt and
   * Adrift_43_p39exam.txt: the Void Room has an empty Long, no alts and no
   * objects, and both `e` and `look` answer "There is nothing of interest
   * here.  You can only move west." -- the sentence joined to the exits line
   * with the ordinary two-space clause gap.  The 4.0 twin p4EXAM.taf,
   * Adrift_1_p4exam.txt, prints the exits alone.
   */
  if (!is_described)
    {
      const scr_int version = prop_get_taf_version (bundle);

      if (version == TAF_VERSION_390)
        {
          pf_buffer_string (filter, "There is nothing of interest here.");
          is_described = TRUE;
        }
    }

  /*
   * Terminate the description block with a single line break.  Many ADRIFT
   * room descriptions already end with a trailing "<br>" of their own; if we
   * unconditionally added a newline here it would double up with that break,
   * leaving a stray blank line before the contents.  Add the break only when
   * the buffer does not already end with one.
   *
   * Record it as ours.  The Runner has no terminator here at all -- viewroom
   * keeps concatenating onto the one room string, and the contents list joins
   * straight on with the two spaces of its own literal "  Also here"
   * (@00472696) -- so lib_print_room_contents() takes this newline back
   * again the moment it has anything to say.  What is left standing is the
   * room block's terminator for the case where it has nothing.
   */
  if (is_described)
    {
      const scr_char *buffered = pf_get_buffer (filter);

      if (!(buffered && pf_text_ends_with_break (buffered)))
        pf_buffer_character (filter, '\n');
      pf_note_trailing_auto_break (filter);
    }

  /*
   * Reveal what a full room description reveals.  Not gated on showobjects:
   * the Runner's marking loops sit above its "Also here" list and below its
   * brief-mode exit, so a HideObjects alt suppresses the sentence and not
   * the knowledge.  See obj_mark_room_objects_seen().
   */
  obj_mark_room_objects_seen (game, room);

  /* Print room contents. */
  if (showobjects)
    {
      const scr_char *buffered = pf_get_buffer (filter);
      size_t noted = buffered ? strlen (buffered) : 0;

      lib_print_room_contents (game, room);

      buffered = pf_get_buffer (filter);
      if (buffered && strlen (buffered) > noted)
        is_described = TRUE;
    }

  /*
   * Finally, print any relevant event look text.  The runner appends it dead
   * last in the room block, after the object list and the character lines,
   * run on with its two-space separator (probed live in both runners,
   * 2026-08-02).  Join onto the description block only if this call printed
   * one, so an event's text can't migrate up onto the room name line.
   *
   * The visibility test asks about `room`, the room being described, not
   * about the player -- run400's viewroom indexes the event's room list with
   * its own room argument (loc_472B53) and the two differ whenever a task
   * with ShowRoomDesc displays a room before its actions have moved anyone.
   * See evt_can_see_event_in_room().
   */
  looked = FALSE;
  for (event = 0; event < gs_event_count (game); event++)
    {
      if (gs_event_state (game, event) == ES_RUNNING
          && evt_can_see_event_in_room (game, event, room))
        {
          const scr_char *looktext;

          vt_key[0].string = "Events";
          vt_key[1].integer = event;
          vt_key[2].string = "LookText";
          looktext = prop_get_string (bundle, "S<-sis", vt_key);
          if (!scr_strempty (looktext))
            {
              if (is_described || looked)
                pf_buffer_join (filter, looktext);
              else
                pf_buffer_string (filter, looktext);
              looked = TRUE;
            }

          vt_key[2].string = "Res";
          vt_key[3].integer = 1;
          res_handle_resource (game, "sisi", vt_key);
        }
    }
  if (looked)
    {
      const scr_char *buffered = pf_get_buffer (filter);

      if (!(buffered && pf_text_ends_with_break (buffered)))
        pf_buffer_character (filter, '\n');
    }
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

      if (lib_trace)
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

            if (lib_trace)
              {
                scr_trace ("Library: task %ld, check %ld\n",
                          restriction, check);
              }

            /* Restrict if task isn't done/not done as expected. */
            if ((check != 0) == gs_task_done (game, restriction))
              is_restricted = TRUE;
            break;
          }

        case 1:                /* Object state restriction */
          {
            scr_int object, check, openable;

            /* Get the target object. */
            object = obj_stateful_object (game, restriction);

            /* Get the expected object state. */
            vt_key[4].string = "Var2";
            check = prop_get_integer (bundle, "I<-sisis", vt_key);

            if (lib_trace)
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


/* List of direction names, for printing and counting exits. */
static const scr_char *const DIRNAMES_4[] = {
  "north", "east", "south", "west", "up", "down", "in", "out",
  NULL
};
static const scr_char *const DIRNAMES_8[] = {
  "north", "east", "south", "west", "up", "down", "in", "out",
  "northeast", "southeast", "southwest", "northwest",
  NULL
};


/*
 * lib_room_has_exits()
 *
 * Return TRUE if the player room has at least one usable exit.  Used to
 * suppress the automatic exit listing appended to room descriptions when a
 * room has no exits at all.  The original ADRIFT runner builds the exit
 * string and, for the room description (as opposed to an explicit "exits"
 * command), discards it if it ends in "any direction!"; testing for usable
 * exits up front is the equivalent.
 */
const scr_char *
lib_direction_name (scr_int direction)
{
  const scr_int count = sizeof DIRNAMES_8 / sizeof DIRNAMES_8[0] - 1;

  if (direction < 0 || direction >= count)
    return NULL;
  return DIRNAMES_8[direction];
}


/*
 * lib_compass_names()
 *
 * Return the direction names list for the game's compass, eight point or
 * four.
 */
static const scr_char *const *
lib_compass_names (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  return prop_get_global_boolean (bundle, "EightPointCompass")
         ? DIRNAMES_8 : DIRNAMES_4;
}


/*
 * lib_room_exit_available()
 *
 * Return TRUE if the given room defines an exit in the given direction and
 * nothing currently blocks its use.
 */
static scr_bool
lib_room_exit_available (scr_gameref_t game, scr_int room, scr_int direction)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4], vt_rvalue;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = room;
  vt_key[2].string = "Exits";
  vt_key[3].integer = direction;
  return prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key)
         && lib_can_go (game, room, direction);
}


/*
 * lib_room_exit_destination()
 *
 * Return TRUE and write the destination of the player room's exit in the
 * given direction, FALSE if the room defines no such exit.
 */
static scr_bool
lib_room_exit_destination (scr_gameref_t game,
                           scr_int direction, scr_int *destination)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5], vt_rvalue;

  vt_key[0].string = "Rooms";
  vt_key[1].integer = gs_playerroom (game);
  vt_key[2].string = "Exits";
  vt_key[3].integer = direction;
  vt_key[4].string = "Dest";
  if (!prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key))
    return FALSE;

  *destination = vt_rvalue.integer - 1;
  return TRUE;
}


static scr_bool
lib_room_has_exits (scr_gameref_t game, scr_int room)
{
  const scr_char *const *dirnames;
  scr_int index_;

  /* Return on the first valid, usable exit found. */
  dirnames = lib_compass_names (game);
  for (index_ = 0; dirnames[index_]; index_++)
    {
      if (lib_room_exit_available (game, room, index_))
        return TRUE;
    }
  return FALSE;
}


/*
 * lib_print_exits_list()
 *
 * Print a list of exits from the given room.
 */
static void
lib_print_exits_list (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *const *dirnames;
  scr_int index_;
  lib_list_t list;

  /* Poll for an exit for each valid direction name. */
  dirnames = lib_compass_names (game);
  for (index_ = 0; dirnames[index_]; index_++)
    {
      if (lib_room_exit_available (game, room, index_))
        list.push_back (index_);
    }
  if (!list.empty ())
    {
      /*
       * Vary text for a lone exit.  SCARE used to print "There is an
       * exit "/"There are exits " on turn 0, but no Runner version has
       * that wording at all (run370-run400 binaries carry only " can
       * only move " and " can move "), and the run400 Goldilocks
       * transcript says "I can move ..." even in the game-start room
       * display.
       */
      if (list.size () == 1)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can only move ",
                                                 "I can only move ",
                                                 "%player% can only move "));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can move ",
                                                 "I can move ",
                                                 "%player% can move "));
        }
      lib_print_name_list (game, list, dirnames, " and ");
      pf_buffer_string (filter, ".\n");
    }
  else
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                      "You can't go in any direction!\n",
                                      "I can't go in any direction!\n",
                                      "%player% can't go in any direction!\n"));
    }
}


/*
 * lib_cmd_print_room_exits()
 *
 * Command handler for "exits"; lists exits from the player room.
 */
scr_bool
lib_cmd_print_room_exits (scr_gameref_t game)
{
  lib_print_exits_list (game, gs_playerroom (game));
  return TRUE;
}


/*
 * lib_print_room_exits()
 *
 * Append the exits list to a room description if the ShowExits global
 * requests it.  The run400 room builder itself runs the "exits" command at
 * the end of every room display when MemVar_4941E9 (ShowExits) is set, and
 * strips the result again if it ends "any direction!" (@00472BFF-00472C64
 * in Proc_19_63_472CA4) -- so this applies to task ShowRoomDesc displays
 * just as much as to player-room ones.
 */
void
lib_print_room_exits (scr_gameref_t game, scr_int room)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_global_boolean (bundle, "ShowExits")
      && lib_room_has_exits (game, room))
    {
      pf_buffer_character (filter, '\n');
      lib_print_exits_list (game, room);
    }
}


/*
 * lib_describe_player_room()
 *
 * Print out details of the player room, in brief if verbose not set and the
 * room has already been visited.
 *
 * The room-name heading above the description is a 3.9 feature.  It is the
 * "showshortroom" Appearance option, and the string occurs eight times in the
 * run390 P-code and twice in run400's, but not once in run370's or run380's --
 * their Options -> Appearance submenu offers colours and a font size only.  So
 * the pre-3.9 Runners print no heading at all in verbose mode, and in brief
 * mode print the room name *with a trailing period* as the whole of the
 * description.  Measured live under Wine 2026-08-24: arlo.taf (3.70) in
 * run370, twenty-odd room entries with Verbose on and not one heading, then
 * the same replay with Verbose off giving "Alice's Garden.  Horse walks
 * towards you from the north." for a revisit; and akron.taf (3.80) in run380,
 * 25 room entries, again no heading anywhere.
 */
static void
lib_describe_player_room (scr_gameref_t game, scr_bool force_verbose)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_verbose;

  is_verbose = force_verbose
               || game->verbose || !gs_room_seen (game, gs_playerroom (game));

  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    lib_print_room_name (game, gs_playerroom (game));
  else if (!is_verbose)
    {
      /*
       * Pre-3.9 brief: the name, with a period, is the whole description.  No
       * paragraph break -- the pre-3.9 room text runs on the line below the
       * move message rather than after a blank line, "You move south." then
       * "Alice's Garden.  Horse walks towards you from the north."
       */
      pf_buffer_string (filter, lib_get_room_name (game, gs_playerroom (game)));
      pf_buffer_string (filter, ".\n");
    }

  /* Print other room details if applicable. */
  if (is_verbose)
    {
      /* Print room description, and objects and NPCs. */
      lib_print_room_description (game, gs_playerroom (game));

      /* Print exits if the ShowExits global requests it. */
      lib_print_room_exits (game, gs_playerroom (game));
    }
}


/*
 * lib_cmd_look()
 *
 * Command handler for "look" command.
 */
scr_bool
lib_cmd_look (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_character (filter, '\n');
  lib_describe_player_room (game, TRUE);
  return TRUE;
}


/*
 * lib_cmd_quit()
 *
 * Called on "quit", "bye" and "end".  Exits from the game main loop.
 *
 * Declining the confirmation is not silent: every Runner answers "I'm so glad
 * you said no...".  The branch is two statements, and the second runs whatever
 * the first did --
 *
 *     run370 loc_43C07E:  Me.Global.Unload MemVar_4461A8
 *     run370 loc_43C089:  MemVar_4460E4 = "I'm so glad you said no..."
 *
 * (run380 loc_44A6xx, run390 loc_45FA5D, run400 loc_48AABA/48AAC2 are the same
 * pair) -- because VB's Unload raises Form_QueryUnload, which is where the
 * Runner puts the "Are you sure?" box.  Confirm and the process is gone before
 * the assignment can matter; decline and Unload simply returns, leaving the
 * line as the turn's whole output.  Note it is an assignment, not an append,
 * so it *replaces* anything the turn had buffered; here the buffer is
 * necessarily empty, since a matched task would have taken the line before
 * run_standard_commands() ever ran.
 *
 * The literal is in all four constant pools, so this is not version-gated.
 */
scr_bool
lib_cmd_quit (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_QUIT))
    game->is_running = FALSE;
  else
    pf_buffer_string (gs_get_filter (game), "I'm so glad you said no...\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_endgame()
 *
 * Called on "endgame".  Ends the game there and then, printing nothing but
 * the score summary.
 *
 * This is *not* a synonym of `quit`, and it does not go through the ending
 * machinery either: the Runner writes the two lines inline in generaltasks and
 * then sets the gameover byte, so there is no WinText, no "Better luck next
 * time.", and no "Well done - you scored maximum points!" / "You finished N
 * points short." line -- those live in Form1.endmessage, which this path never
 * reaches.  run400 loc_48AAC9-48ABB8:
 *
 *     If cmd = "endgame" Then
 *       out &= "You scored" & Str(score) & " out of the maximum" & Str(MaxScore) & "!" & CRLF
 *       If MaxScore = 0 Then MaxScore = 1
 *       out &= "That is" & Str(Int(score * (100 / MaxScore))) & "% of the game!" & CRLF & CRLF
 *       out &= "[Press any key to end]"
 *       gameover = 3
 *
 * Note the order: the "out of the maximum" figure is the game's real MaxScore,
 * and the 0 -> 1 fix-up applies only to the divisor, so a scoreless game gets
 * "out of the maximum 0!" and "That is 0% of the game!".  That differs from
 * task_print_end_game_summary(), which follows Form1.endmessage in skipping
 * the whole summary for a scoreless 4.0 game and reporting a scoreless pre-4.0
 * one as 100%, so the two printers are deliberately kept apart.
 *
 * All four Runners carry the branch, identically worded (run370 loc_43C095,
 * run380 loc_442xxx, run390 loc_45FB94, run400 loc_48AAC9), and every literal
 * is in all four constant pools, so it is not gated on a version.
 *
 * "[Press any key to end]" is the Runner's own end-of-session prompt and is
 * supplied by the host here, not by the library; see the transcript note in
 * test/adrift4/notes/WINE-TRANSCRIPTS-TODO.md.
 */
scr_bool
lib_cmd_endgame (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_score, divisor, percent;
  scr_char buffer[32];

  max_score = prop_get_global_integer (bundle, "MaxScore");

  pf_buffer_string (filter, "You scored ");
  snprintf (buffer, sizeof (buffer), "%ld", game->score);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, " out of the maximum ");
  snprintf (buffer, sizeof (buffer), "%ld", max_score);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, "!\n");

  divisor = max_score == 0 ? 1 : max_score;
  percent = (scr_int) floor (game->score * (100.0 / divisor));
  pf_buffer_string (filter, "That is ");
  snprintf (buffer, sizeof (buffer), "%ld", percent);
  pf_buffer_string (filter, buffer);
  pf_buffer_string (filter, "% of the game!\n");

  /* Stop the game, and note that it's not resumeable -- the gameover byte the
     Runner sets here is the same one an EndGame task action writes. */
  game->is_running = FALSE;
  game->has_completed = TRUE;
  return TRUE;
}


/*
 * lib_cmd_control_panel()
 *
 * Called on "control panel", "control-panel", "control" and "panel".
 *
 * All four Runners take these as one whole-line test and open (or focus) the
 * Runner's Control Panel window, answering "Control Panel on" the first time
 * and "Control Panel already on." after that (run370 loc_43C34E, run400
 * loc_48AEAE).  That window is a Windows Runner feature with no counterpart
 * here, so rather than leave the words to fall through to the game -- which
 * the Runner never does -- say plainly that there is none.
 */
scr_bool
lib_cmd_control_panel (scr_gameref_t game)
{
  return lib_print_message (game,
                            "Scarier does not implement a Control Panel."
                            "  Try typing in commands instead.\n");
}


/*
 * lib_cmd_restart()
 *
 * Called on "restart".  Exits from the game main loop with restart
 * request set.
 */
scr_bool
lib_cmd_restart (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_RESTART))
    {
      game->is_running = FALSE;
      game->do_restart = TRUE;
    }

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_undo()
 *
 * Called on "undo".  Restores any undo game or memo to the main game.
 *
 * The wording is a three-way version split, and none of the three is what
 * SCARE used to say.  Measured 2026-09-07 from the four Runners' constant
 * pools and the handlers that print them:
 *
 *   3.70  has no `undo` at all -- the word does not occur in run370's pool,
 *         so the line falls through to the game's DontUnderstand.
 *   3.80  knows the word and has nothing behind it: run380 generaltasks
 *         @442EE9 tests `c("undo")` and answers "I can't undo your
 *         blundering." every time, with no state to restore.
 *   3.90+ answers "Undone." on success (run390 do_undo @436AB5, run400
 *         Proc_19_62 @45B0FF) and "I can't undo any more of your
 *         blunderings!" when the history is empty (@45B158).
 *
 * "[The previous turn has been undone.]", "Sorry, no more undo is
 * available." and "You can't undo what hasn't been done." are in no Runner's
 * pool at all, and neither Runner prints a room name with the answer:
 * Adrift_361_cellar.txt (4.00) reads plain "Undone." three times running.
 *
 * NOT ported, and visible in that same transcript: 3.9 and 4.0 also REPLAY
 * the restored turn's output.  Each Runner keeps a 10-deep record array
 * (run400 MemVar_494124) whose field 0 is the turn's whole output buffer,
 * stamped from MemVar_4941B0 when the record is pushed (@48BD6E); `undo`
 * reads slot *1* -- the previous turn -- and prints "Undone." & vbCrLf & the
 * text that slot holds, so undoing `e` re-prints what `take satchel` said.
 * An emptied slot is stamped "!!" (@45B146) and that sentinel is what the
 * availability test reads (@45AE5C).  Scarier's filter keeps no per-turn text
 * to replay; recording one is the open half of this lead.
 */
scr_bool
lib_cmd_undo (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));

  /* 3.70 does not know the word; let the game answer instead. */
  if (version < TAF_VERSION_380)
    return FALSE;

  /* 3.80 knows it and refuses, always. */
  if (version < TAF_VERSION_390)
    {
      pf_buffer_string (filter, "I can't undo your blundering.\n");
      game->is_admin = TRUE;
      return TRUE;
    }

  /* If an undo buffer is available, restore it. */
  if (game->undo_available)
    {
      gs_copy (game, game->undo);
      game->undo_available = FALSE;

      pf_buffer_string (filter, "Undone.\n");

      /* Undo can't properly unravel layered sounds... */
      game->stop_sound = TRUE;
    }

  /*
   * If there is no undo buffer, try to restore one saved previously in a
   * memo.  If that works, treat as for restore from file, since that's
   * effectively what it is.  The Runner's own history is ten records deep and
   * needs no such split; this is the port's second tier of the same thing, so
   * it answers with the same word.
   */
  else if (memo_load_game (memento, game))
    {
      pf_buffer_string (filter, "Undone.\n");

      game->is_running = FALSE;
      game->do_restore = TRUE;
    }

  /* If no undo buffer and memo restore failed, there's no undo available. */
  else
    pf_buffer_string (filter,
                      "I can't undo any more of your blunderings!\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_format_elapsed_time()
 *
 * Format a count of elapsed game seconds as "[Hh ][M]Mm SSs".
 */
static void
lib_format_elapsed_time (scr_int timestamp, scr_char *buffer, size_t length)
{
  scr_int hr, min, sec;

  /* Separate the timestamp out into components. */
  hr = timestamp / SECS_PER_HOUR;
  min = (timestamp % SECS_PER_HOUR) / MINS_PER_HOUR;
  sec = timestamp % SECS_PER_MINUTE;

  if (hr > 0)
    snprintf (buffer, length, "%ldh %02ldm %02lds", hr, min, sec);
  else
    snprintf (buffer, length, "%ldm %02lds", min, sec);
}


/*
 * lib_cmd_history_common()
 * lib_cmd_history_number()
 * lib_cmd_history()
 *
 * Prints a history of saved commands for the game.  Print directly rather
 * than using the printfilter to avoid possible clashes with ALRs.
 */
static scr_bool
lib_cmd_history_common (scr_gameref_t game, scr_int limit)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_int first, count, timestamp;

  /*
   * The runner main loop will add an entry for the "history" command that
   * got us here, but it hasn't done so yet.  To keep the history list
   * accurate for recalling commands, we add a surrogate "history" command
   * to the history here, and remove it when we've done listing.  This matches
   * the c-shell, which always shows 'history' listed last.
   */
  timestamp = var_get_elapsed_seconds (vars);
  memo_save_command (memento, "[history]", timestamp, game->turns);

  /* Decide on the first history to display; all if limit is 0 or less. */
  if (limit > 0)
    {
      /*
       * Get a count of the history length recorded.  Because of the surrogate
       * "history" above, this is always at least one.  From this, choose a
       * start point for the display; all if not enough history.
       */
      count = memo_get_command_count (memento);
      first = (count > limit) ? count - limit : 0;
    }
  else
    first = 0;

  if_print_string ("These are your most recent game commands:\n\n");

  /* Display history starting at the first entry determined above. */
  memo_first_command (memento);
  for (count = 0; memo_more_commands (memento); count++)
    {
      const scr_char *command;
      scr_int sequence, turns;

      /* Obtain the history entry, and write if included. */
      memo_next_command (memento, &command, &sequence, &timestamp, &turns);
      if (count >= first)
        {
          scr_char buffer[64];

          /* Write the history entry sequence. */
          snprintf (buffer, sizeof(buffer), "%4ld -- Time ", sequence);
          if_print_string (buffer);

          /* Print playing time as "[HHh ][M]Mm SSs". */
          lib_format_elapsed_time (timestamp, buffer, sizeof(buffer));
          if_print_string (buffer);

          /* Follow up with the turns count, and the command string itself. */
          snprintf (buffer, sizeof(buffer), ", turn %ld : ", turns);
          if_print_string (buffer);
          if_print_string (command);
          if_print_character ('\n');
        }
    }

  /* Remove the surrogate "history"; the main loop will add the real one. */
  memo_unsave_command (memento);

  lib_set_admin (game);
  return TRUE;
}

scr_bool
lib_cmd_history_number (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int limit;

  /* Get requested length of history list, and complain if not valid. */
  limit = var_get_ref_number (vars);
  if (limit < 1)
    {
      if_print_string ("That's not a valid history length.\n");

      game->is_admin = TRUE;
      return TRUE;
    }

  return lib_cmd_history_common (game, limit);
}

scr_bool
lib_cmd_history (scr_gameref_t game)
{
  return lib_cmd_history_common (game, 0);
}


/*
 * lib_cmd_again()
 * lib_cmd_redo_number()
 * lib_cmd_redo_text_last_common()
 * lib_cmd_redo_text()
 * lib_cmd_redo_last()
 *
 * The first function is called on "again", and simply sets the game do_again
 * flag.  The others allow the user to select a command from the history list
 * to re-run.
 */
scr_bool
lib_cmd_again (scr_gameref_t game)
{
  game->do_again = TRUE;
  game->redo_sequence = 0;

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_redo_number (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_int sequence;

  /*
   * Get the history sequence entry requested and validate it.  The sequence
   * may be positive (absolute) or negative (relative to history end), but
   * not zero.
   */
  sequence = var_get_ref_number (vars);
  if (sequence != 0 && memo_find_command (memento, sequence))
    {
      game->do_again = TRUE;
      game->redo_sequence = sequence;
    }
  else
    {
      if_print_string ("No matching entry found in the command history.\n");

      /*
       * This is a failed redo, but returning FALSE will cause the game's
       * unknown command message to come up.  However, returning TRUE will
       * cause the runner main loop to add this to its history, and at some
       * point a "redo 7" could cause problems (say, when it's at sequence 7,
       * where it'll cause an infinite loop).  To work round this, here we'll
       * return a redo_sequence _without_ do_again, and have the runner catch
       * that as an indication not to save the command in its history.  Sorry
       * for the ugliness.
       */
      game->do_again = FALSE;
      game->redo_sequence = INT_MAX;
    }

  game->is_admin = TRUE;
  return TRUE;
}

static scr_bool
lib_cmd_redo_text_last_common (scr_gameref_t game, const scr_char *target)
{
  const scr_memo_setref_t memento = gs_get_memento (game);
  scr_bool is_do_last, is_contains;
  scr_int length, matched_sequence;

  /* Make a special case of "!!", rerun the final command in the history. */
  is_do_last = (strcmp (target, "!") == 0);

  /*
   * Differentiate starts-with and contains searches, setting is_contains and
   * advancing by one if the target begins '?' (word search).  Note target
   * string length.
   */
  is_contains = (target[0] == '?');
  target += is_contains ? 1 : 0;
  length = strlen (target);

  /* If there's no text left to search for, reject this call now. */
  if (length == 0)
    {
      if_print_string ("No matching entry found in the command history.\n");

      /* As with failed numeric redo above, special-case this return. */
      game->do_again = FALSE;
      game->redo_sequence = INT_MAX;

      game->is_admin = TRUE;
      return TRUE;
    }

  /*
   * Search saved commands for one that matches the target string in the
   * required way.  We want to return the most recently saved match, so ideally
   * we'd search backwards, but the iterator is only forwards, so we do it the
   * hard way.
   */
  matched_sequence = 0;
  memo_first_command (memento);
  while (memo_more_commands (memento))
    {
      const scr_char *command;
      scr_int sequence, timestamp, turns;
      scr_bool is_matched;

      /* Get the command; only command and sequence are relevant. */
      memo_next_command (memento, &command, &sequence, &timestamp, &turns);

      /*
       * If this is the "!!" special case, match everything.  Otherwise,
       * either search the command for the target, or match if the command
       * begins with the target.
       */
      if (is_do_last)
        is_matched = TRUE;
      else if (is_contains)
        {
          scr_int index_;

          /* Search this command for an occurrence of target anywhere. */
          is_matched = FALSE;
          for (index_ = strlen (command) - length; index_ >= 0; index_--)
            {
              if (scr_strncasecmp (command + index_, target, length) == 0)
                {
                  is_matched = TRUE;
                  break;
                }
            }
        }
      else
        is_matched = (scr_strncasecmp (command, target, length) == 0);

      /* If the command matched the target criteria, note it and continue. */
      if (is_matched)
        matched_sequence = sequence;
    }

  /* If we found a match, set the redo values accordingly. */
  if (matched_sequence > 0)
    {
      game->do_again = TRUE;
      game->redo_sequence = matched_sequence;
    }
  else
    {
      if_print_string ("No matching entry found in the command history.\n");

      /* As with failed numeric redo above, special-case this return. */
      game->do_again = FALSE;
      game->redo_sequence = INT_MAX;
    }

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_redo_text (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  /* Call the common redo with the referenced text from %text%. */
  return lib_cmd_redo_text_last_common (game, var_get_ref_text (vars));
}

scr_bool
lib_cmd_redo_last (scr_gameref_t game)
{
  /* Call the common redo with, literally, "!", forming "!!" . */
  return lib_cmd_redo_text_last_common (game, "!");
}


/*
 * lib_cmd_hints()
 *
 * Called on "hints".  Requests the interface to display any available hints.
 */
scr_bool
lib_cmd_hints (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int task;
  scr_bool game_has_hints;

  /*
   * Check for the presence of any game hints at all, no matter whether the
   * task is runnable or not.
   */
  game_has_hints = FALSE;
  for (task = 0; task < gs_task_count (game); task++)
    {
      if (task_has_hints (game, task))
        {
          game_has_hints = TRUE;
          break;
        }
    }

  /* If the game has hints, display any relevant ones. */
  if (game_has_hints)
    {
      if (run_hint_iterate (game, NULL))
        {
          if (if_confirm (SCR_CONF_VIEW_HINTS))
            if_display_hints (game);
        }
      else
        pf_buffer_string (filter, "There are currently no hints available.\n");
    }
  else
    {
      pf_buffer_string (filter,
                        "There are no hints available for this adventure.\n");
      pf_buffer_string (filter,
                        "You're just going to have to work it out for"
                        " yourself...\n");
    }

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_print_string_bold()
 * lib_print_string_italics()
 *
 * Convenience helpers for printing licensing and game information.
 */
static void
lib_print_string_bold (const scr_char *string)
{
  if_print_tag (SCR_TAG_BOLD, "");
  if_print_string (string);
  if_print_tag (SCR_TAG_ENDBOLD, "");
}

static void
lib_print_string_italics (const scr_char *string)
{
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string (string);
  if_print_tag (SCR_TAG_ENDITALICS, "");
}


/*
 * lib_cmd_help()
 * lib_cmd_license()
 *
 * A form of standard help output for games that don't define it themselves,
 * and the GPL licensing.  Print directly rather than using the printfilter
 * to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_help (scr_gameref_t game)
{
  if_print_string (
    "These are some of the typical commands used in this adventure:\n\n");

  if_print_string (
    "  [N]orth, [E]ast, [S]outh, [W]est, [U]p, [D]own, [In], [O]ut,"
    " [L]ook, [Exits]\n  E[x]amine <object>, [Get <object>],"
    " [Drop <object>], [...it], [...all]\n  [Where is <object>]\n"
    "  [Give <object> to  <character>], [Open...], [Close...],"
    " [Ask <character> about <subject>]\n"
    "  [Wear <object>], [Remove <object>], [I]nventory\n"
    "  [Put <object> into <object>], [Put <object> onto <object>]\n");

  if_print_string ("\nUse the ");
  lib_print_string_italics ("Save");
  if_print_string (", ");
  lib_print_string_italics ("Restore");
  if_print_string (", ");
  lib_print_string_italics ("Undo");
  if_print_string (", and ");
  lib_print_string_italics ("Quit");
  if_print_string (
    " commands to save and restore games, undo a move, and leave the "
    " game.  Use ");
  lib_print_string_italics ("History");
  if_print_string (" and ");
  lib_print_string_italics ("Redo");
  if_print_string (
    " to view and repeat recent game commands.\n");

  if_print_string ("\nThe ");
  lib_print_string_italics ("Hint");
  if_print_string (" command displays any game hints, ");
  lib_print_string_italics ("Notify");
  if_print_string (" provides score change notification, and ");
  lib_print_string_italics ("Verbose");
  if_print_string (" and ");
  lib_print_string_italics ("Brief");
  if_print_string (" control room descriptions.\n");

  if_print_string ("\nUse ");
  lib_print_string_italics ("License");
  if_print_string (
    " to view SCARIER's licensing terms and conditions, and ");
  lib_print_string_italics ("Version");
  if_print_string (
    " to print both SCARIER's and the game's version number.\n");

  lib_set_admin (game);
  return TRUE;
}

scr_bool
lib_cmd_license (scr_gameref_t game)
{
  lib_print_string_bold ("SCARIER");
  if_print_string (" is ");
  lib_print_string_italics (
    "Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford");
  if_print_string (".\n\n");

  if_print_string (
    "This program is free software; you can redistribute it and/or modify"
    " it under the terms of version 2 of the GNU General Public License"
    " as published by the Free Software Foundation.\n\n");

  if_print_string (
    "This program is distributed in the hope that it will be useful, but ");
  lib_print_string_bold ("WITHOUT ANY WARRANTY");
  if_print_string ("; without even the implied warranty of ");
  lib_print_string_bold ("MERCHANTABILITY");
  if_print_string (" or ");
  lib_print_string_bold ("FITNESS FOR A PARTICULAR PURPOSE");
  if_print_string (
    ".  See the GNU General Public License for more details.\n\n");

  if_print_string (
    "You should have received a copy of the GNU General Public License"
    " along with this program; if not, write to the Free Software"
    " Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301"
    " USA\n\n");

  if_print_string ("Please report any bugs, omissions, or misfeatures to ");
  lib_print_string_italics ("simon_baldwin@yahoo.com");
  if_print_string (".\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_information()
 *
 * Display a few small pieces of game information, done by a dialog GUI
 * in real Adrift.  Prints directly rather than using the printfilter to
 * avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_information (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_vartype_t vt_key[1];
  const scr_char *gamename, *compile_date, *gameauthor;
  scr_char *filtered;

  gamename = prop_get_global_string (bundle, "GameName");
  filtered = pf_filter_for_info (gamename, vars);
  pf_strip_tags (filtered);

  if_print_string ("\"");
  if_print_string (!scr_strempty (filtered) ? filtered : "Untitled");
  if_print_string ("\"");
  scr_free (filtered);

  vt_key[0].string = "CompileDate";
  compile_date = prop_get_string (bundle, "S<-s", vt_key);
  if (!scr_strempty (compile_date))
    {
      if_print_string (", ");
      if_print_string (compile_date);
    }

  gameauthor = prop_get_global_string (bundle, "GameAuthor");
  filtered = pf_filter_for_info (gameauthor, vars);
  pf_strip_tags (filtered);

  if_print_string (", ");
  if_print_string (!scr_strempty (filtered) ? filtered : "Anonymous");
  if_print_string (".\n");
  scr_free (filtered);

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_cmd_clear()
 *
 * Clear the main game window (almost).
 */
scr_bool
lib_cmd_clear (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_tag (filter, SCR_TAG_CLS);
  pf_buffer_string (filter, "Screen cleared.\n");
  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_cmd_statusline()
 *
 * Display the status line as would be shown by the Runner.  Useful for
 * interpreter builds that can't offer a true status line.  Prints directly
 * rather than using the printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_statusline (scr_gameref_t game)
{
  const scr_char *name, *author, *room, *status;
  scr_int score;

  /*
   * Retrieve the game's name and author, the description of the current
   * game room, and any formatted game status line.
   */
  run_get_attributes (game, &name, &author, NULL, NULL,
                      &score, NULL, &room, &status, NULL, NULL, NULL, NULL);

  /* If nothing is yet determined, print the game name and author. */
  if (!room || scr_strempty (room))
    {
      if_print_string (name);
      if_print_string (" | ");
      if_print_string (author);
    }
  else
    {
      /* Print the player location, and a separator. */
      if_print_string (room);
      if_print_string (" | ");

      /* If the game offers a status line, print it, otherwise the score. */
      if (status && !scr_strempty (status))
        if_print_string (status);
      else
        {
          scr_char buffer[32];

          if_print_string ("Score: ");
          snprintf (buffer, sizeof(buffer), "%ld", score);
          if_print_string (buffer);
        }
    }
  if_print_character ('\n');

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_version()
 *
 * Display the "Runner version".  Prints directly rather than using the
 * printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_version (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key;
  scr_char buffer[64];
  scr_int major, minor, point;
  const scr_char *version;

  if_print_string ("SCARIER version ");
  if_print_string (SCARIER_VERSION SCARIER_PATCH_LEVEL);
  if_print_string (" [Adrift ");
  major = SCARIER_EMULATION / 1000;
  minor = (SCARIER_EMULATION % 1000) / 100;
  point = SCARIER_EMULATION % 100;
  snprintf (buffer, sizeof(buffer), "%ld.%02ld.%02ld", major, minor, point);
  if_print_string (buffer);
  if_print_string (" compatible], ");

  vt_key.string = "VersionString";
  version = prop_get_string (bundle, "S<-s", &vt_key);
  if_print_string ("Generator version ");
  if_print_string (version);
  if_print_string (".\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_wait()
 * lib_cmd_wait_number()
 *
 * Set game waitcounter to a count of turns for which the main loop will run
 * without taking input.  Many Adrift Runners ignore any WaitTurns setting in
 * the game, and use always use one; this might make a game misbehave, so to
 * try to cover this case we supply 'wait N' as a player control to override
 * the game's setting.  The latter prints directly rather than using the
 * printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_wait (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int waitturns;

  /* Note if wait turns is different from the game's setting. */
  waitturns = prop_get_global_integer (bundle, "WaitTurns");
  if (waitturns != game->waitturns)
    {

      pf_buffer_string (filter, "(");
      pf_buffer_integer (filter, game->waitturns);
      pf_buffer_string (filter,
                        game->waitturns == 1 ? " turn)\n" : " turns)\n");
    }

  /* Reset the wait counter to the current waitturns setting. */
  game->waitcounter = game->waitturns;

  /*
   * 4.0 stores "Time passes..." with a vbCrLf of its own (run400 48ABDA:
   * the literal, then Proc_21_4_442418 which returns vbCrLf), so a walk
   * announcement in the same turn starts a new line instead of joining
   * with two spaces; 3.9 stores the bare literal (run390 45E636) and joins.
   * Measured run400, EV15, Adrift_1_ev15.txt: "Time passes..." / "Walker
   * arrives from the east." on separate lines.
   */
  pf_buffer_string (filter, "Time passes...\n");
  if (lib_is_version_400 (game))
    pf_buffer_hard_break (filter);
  return TRUE;
}


/*
 * lib_cmd_wait_390()
 * lib_cmd_wait_number_390()
 *
 * `z` is not in the pre-3.9 Runner vocabulary: the verb harvest finds the
 * "z" literal only from run390 on (index/verbs.py; run390_3.bas 45FCB0),
 * and run380 live answers `z` with "Say again?" -- measured 2026-08-31 on
 * cave.taf, where every `z` in the solution fell dead and the egg-hatch
 * timeline drifted.  These wrap the wait handlers for the `z` rows and
 * decline below 3.90, so `z` falls through to the ordinary unknown-command
 * reply -- which is already "Say again?" here.
 */
scr_bool
lib_cmd_wait_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return lib_cmd_wait (game);
}

scr_bool
lib_cmd_wait_number_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return lib_cmd_wait_number (game);
}

scr_bool
lib_cmd_wait_number (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int waitturns;
  scr_char buffer[32];

  /* Get and validate the waitturns setting. */
  waitturns = var_get_ref_number (vars);
  if (waitturns < 1 || waitturns > 20)
    {
      if_print_string ("You can only wait between 1 and 20 turns.\n");
      game->is_admin = TRUE;
      return TRUE;
    }

  /* Update the game setting, and confirm for the player. */
  game->waitturns = waitturns;

  if_print_string ("The game will now wait ");
  snprintf (buffer, sizeof(buffer), "%ld", waitturns);
  if_print_string (buffer);
  if_print_string (waitturns == 1 ? " turn" : " turns");
  if_print_string (" for each 'wait' command you enter.\n");

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_verbose()
 * lib_cmd_brief()
 *
 * Set/clear game verbose flag.  Print directly rather than using the
 * printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_verbose (scr_gameref_t game)
{
  /* Set game verbose flag and return. */
  game->verbose = TRUE;
  if_print_string ("The game is now in its ");
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string ("verbose");
  if_print_tag (SCR_TAG_ENDITALICS, "");
  if_print_string (" mode, which always gives long descriptions of locations"
                   " (even if you've been there before).\n");

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_brief (scr_gameref_t game)
{
  /* Clear game verbose flag and return. */
  game->verbose = FALSE;
  if_print_string ("The game is now in its ");
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string ("brief");
  if_print_tag (SCR_TAG_ENDITALICS, "");
  if_print_string (" mode, which gives long descriptions of places never"
                   " before visited and short descriptions otherwise.\n");

  game->is_admin = TRUE;
  return TRUE;
}

/*
 * lib_cmd_notify_on_off()
 * lib_cmd_notify()
 *
 * Set/clear/query game score change notification flag.  Print directly
 * rather than using the printfilter to avoid possible clashes with ALRs.
 */
scr_bool
lib_cmd_notify_on_off (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *control;

  /* Get the text following the notify command, and check for "on"/"off". */
  control = var_get_ref_text (vars);
  if (scr_strcasecmp (control, "on") == 0)
    {
      /* Set score change notification. */
      game->notify_score_change = TRUE;
      if_print_string ("Game score change notification is now ");
      if_print_tag (SCR_TAG_ITALICS, "");
      if_print_string ("on");
      if_print_tag (SCR_TAG_ENDITALICS, "");
      if_print_string (", and the game will tell you of any changes in the"
                       " score.\n");
    }
  else if (scr_strcasecmp (control, "off") == 0)
    {
      /* Clear score change notification. */
      game->notify_score_change = FALSE;
      if_print_string ("Game score change notification is now ");
      if_print_tag (SCR_TAG_ITALICS, "");
      if_print_string ("off");
      if_print_tag (SCR_TAG_ENDITALICS, "");
      if_print_string (", and the game will be silent on changes in the"
                       " score.\n");
    }
  else
    {
      if_print_string ("Use 'notify on' or 'notify off' to control game"
                       " score notification.\n");
    }

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_notify (scr_gameref_t game)
{
  /* Report the current state of notification. */
  if_print_string ("Game score change notification is ");
  if_print_tag (SCR_TAG_ITALICS, "");
  if_print_string (game->notify_score_change ? "on" : "off");
  if_print_tag (SCR_TAG_ENDITALICS, "");

  if (game->notify_score_change)
    {
      if_print_string (", and the game will tell you of any changes in the"
                       " score.\n");
    }
  else
    {
      if_print_string (", and the game will be silent on changes in the"
                       " score.\n");
    }

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_time()
 * lib_cmd_date()
 *
 * Print elapsed game time, and smart-alec "date" response.  The Adrift
 * Runner responds here with the system time and date, but we'll do something
 * different.
 */
scr_bool
lib_cmd_time (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_char buffer[64];

  /* Get elapsed game time and convert to hour, minutes, and seconds. */
  lib_format_elapsed_time (var_get_elapsed_seconds (vars), buffer,
                           sizeof(buffer));

  /* Print the game's elapsed time. */
  if_print_string ("You have been running the game for ");
  if_print_string (buffer);
  if_print_string (".\n");

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_date (scr_gameref_t game)
{
  return lib_print_message (game, "Maybe we should just be good friends.\n");
}


/*
 * Direction enumeration.  Used by movement commands, to multiplex them all
 * into a single function.  The values are explicit to ensure they match
 * enumerations in the game data.
 */
enum
{ DIR_NORTH = 0, DIR_EAST = 1, DIR_SOUTH = 2, DIR_WEST = 3,
  DIR_UP = 4, DIR_DOWN = 5, DIR_IN = 6, DIR_OUT = 7,
  DIR_NORTHEAST = 8, DIR_SOUTHEAST = 9, DIR_SOUTHWEST = 10, DIR_NORTHWEST = 11
};


/*
 * lib_set_movement_probe()
 *
 * Put lib_go() into probe mode, in which it prints nothing, moves nobody,
 * and returns TRUE only if the movement it was handed would really have
 * taken the player out of the room.  Used by the version 3.8 movement
 * pre-pass in run_all_commands(); see the commentary there.
 */
static scr_bool lib_movement_probe = FALSE;

void
lib_set_movement_probe (scr_bool probe)
{
  lib_movement_probe = probe;
}


/*
 * lib_go()
 *
 * Central movement command, called by all movement handlers.
 */
static scr_bool
lib_go (scr_gameref_t game, scr_int direction)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool is_trapped, is_exitable[12];
  scr_int destination, index_;
  const scr_char *const *dirnames;

  /* Decide on four or eight point compass names list. */
  dirnames = lib_compass_names (game);

  /* Start by seeing if there are any exits at all available. */
  is_trapped = TRUE;
  for (index_ = 0; dirnames[index_]; index_++)
    {
      is_exitable[index_] = lib_room_exit_available (game, gs_playerroom (game),
                                                     index_);
      if (is_exitable[index_])
        is_trapped = FALSE;
    }
  if (is_trapped)
    {
      if (lib_movement_probe)
        return FALSE;

      pf_buffer_string (filter,
                        lib_select_response (game,
                                      "You can't go in any direction!\n",
                                      "I can't go in any direction!\n",
                                      "%player% can't go in any direction!\n"));
      return TRUE;
    }

  /*
   * Check for the exit, and if it doesn't exist, refuse, and list the possible
   * options.
   */
  /*
   * A blocked exit is refused exactly like a missing one.  The Runner's
   * movement refusal (run400 Proc_19_29_475638) knows nothing about why a
   * direction failed: it recounts the exits with Proc_19_28_454684 -- the
   * same restriction-aware test as is_exitable[] above, reading the exit's
   * door state and task gate at 45459A-45463C -- and prints " can only
   * move X." or " can't go in that direction, but ... can move ..." from
   * that count.  No Runner from 3.7 to 4.0 carries an "(at present)"
   * string at all; Scarier's old "can't go in that direction (at present)"
   * for an exit that exists but is currently shut was an invention.
   * Measured on humbug (4.00, Adrift_4_humbug.txt): `W` into the keypad
   * door, an exit gated on a task, answers "I can't go in that direction,
   * but I can move north, east and south."
   */
  if (!lib_room_exit_destination (game, direction, &destination)
      || !lib_can_go (game, gs_playerroom (game), direction))
    {
      lib_list_t list;

      if (lib_movement_probe)
        return FALSE;

      /* List available exits, found in exit test loop earlier. */
      for (index_ = 0; dirnames[index_]; index_++)
        {
          if (is_exitable[index_])
            list.push_back (index_);
        }

      /*
       * With exactly one usable exit the Runner prints just " can only
       * move X.", with no "can't go in that direction" prefix; the prefix
       * exists only in the several-exits branch (run400 @00474A75 vs
       * @00474AFB in Proc_19_29_475638).
       */
      if (list.size () == 1)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You can only move ",
                                                 "I can only move ",
                                                 "%player% can only move "));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                             "You can't go in that direction, but you can move ",
                             "I can't go in that direction, but I can move ",
                             "%player% can't go in that direction, but %player_pronoun% can move "));
        }
      lib_print_name_list (game, list, dirnames, " and ");
      pf_buffer_string (filter, ".\n");
      return TRUE;
    }

  /* The move would go through; that is all a probe wants to know. */
  if (lib_movement_probe)
    return TRUE;

  if (lib_trace)
    {
      scr_trace ("Library: moving player from %ld to %ld\n",
                gs_playerroom (game), destination);
    }

  /*
   * Indicate if getting off something or standing up first.  Both lines are
   * bracketed *references*: 3.7 and 3.8 print them unconditionally (run370
   * loc_42303C / loc_423078, run380 loc_428244 / loc_428280), and 3.9 AND
   * 4.0 print them behind Options -> Display & Media... -> Appearance ->
   * "References in brackets" (run400 loc_450339 / loc_4503BF test
   * MemVar_4942BA, saved as "showbrackets" @4679A1; run390 moveroom
   * 431A4C tests m_showbrackets.Checked at loc_431911 for "(Getting off "
   * and loc_4319A9 for "(Standing up first)").  An earlier census read
   * run390 as having no "Getting off" literal at all and gated this
   * `< 3.90 || >= 4.00`; the literal lives in run390_3.bas:9909, and the
   * wingman1.taf (3.90) replay of 2026-08-30 (Adrift_3_wingman1.txt,
   * brackets ON) prints "(Getting off the Barstool first)" before "You
   * move in."
   *
   * Scarier models the Runner with that box ticked -- the reference
   * setting the transcripts are measured under -- so every version prints
   * the lines.  Measured on monsters (4.00) commands 5 and 23, where
   * run400 with brackets on answers "in" from the bed with "(Getting off
   * Sissy's four poster bed first)" on its own line before "I move in."
   * (and later "(Getting off the pink plastic chair first)"); with the box
   * unticked (humbug command 254, 2026-08-24) it prints nothing.
   *
   * The parent-less half -- sitting or lying on the FLOOR, so "(Standing up
   * first)" rather than "(Getting off X first)" -- was measured 2026-09-07 on
   * Main Course.taf (4.00, Adrift_931.txt), whose player starts sitting with
   * ParentObject 0, and on goldilocks (Adrift_932.txt) turn 94, where a task
   * action seats the player on an unset object.  Both print the line with the
   * box ticked and nothing without it, and both had earlier brackets-OFF
   * transcripts that read as an engine bug until they were re-driven.
   */
  if (gs_playerparent (game) != -1)
    {
      pf_buffer_string (filter, "(Getting off ");
      lib_print_object_np (game, gs_playerparent (game));
      pf_buffer_string (filter, " first)\n");
    }
  else if (gs_playerposition (game) != 0)
    pf_buffer_string (filter, "(Standing up first)\n");

  /* Confirm and then make move. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You move ",
                                         "I move ",
                                         "%player% moves "));
  pf_buffer_string (filter, dirnames[direction]);
  pf_buffer_string (filter, ".\n");

  gs_move_player_to_room (game, destination);

  /* Describe the new room and return. */
  lib_describe_player_room (game, FALSE);
  return TRUE;
}


/*
 * lib_cmd_go_*()
 *
 * Direction-specific movement commands.
 *
 * `in` also answers to `inside` and `enter`, and `out` to `outside` and
 * `exit`; the other ten directions have no such alternates.  Every Runner
 * tests the three- and four-way alternations right where it tests `in` and
 * `out` themselves, and by equality against the whole command rather than
 * with c(): run370 loc_434AEA / loc_434C02, run380 loc_43B44D / loc_43B556,
 * run390 loc_44FDF6 / loc_44FF01, run400 loc_474FEF / loc_4750A4.
 *
 * `exit` used to sit in the `exits`/`where`/`directions` row below, which is
 * only right for a room with no out exit.  A room that has one is left by
 * `exit`, and where it has none the wording still differs from a real exits
 * request: the Runner seeds the response of all twenty movement words with
 * the exits summary before the direction blocks get their chance to overwrite
 * it (run380 loc_43AA58), and the several-exits form of that seed is prefixed
 * " can't go in that direction, but" for every word except `exits`, `where`
 * and `directions` (loc_43AC7E).  That is exactly lib_go()'s own refusal, so
 * routing `exit` to lib_cmd_go_out() gets both cases right at once.
 *
 * `inside` and `outside` are absent from that twenty-word seed list, so in a
 * room without the matching exit the Runner has nothing to say and drops
 * through to the catch-all.  We print the exits summary there instead, which
 * is what lib_go() does for every other direction word; the deviation is
 * confined to the case where the movement fails.
 */
scr_bool
lib_cmd_go_north (scr_gameref_t game)
{
  return lib_go (game, DIR_NORTH);
}

scr_bool
lib_cmd_go_east (scr_gameref_t game)
{
  return lib_go (game, DIR_EAST);
}

scr_bool
lib_cmd_go_south (scr_gameref_t game)
{
  return lib_go (game, DIR_SOUTH);
}

scr_bool
lib_cmd_go_west (scr_gameref_t game)
{
  return lib_go (game, DIR_WEST);
}

scr_bool
lib_cmd_go_up (scr_gameref_t game)
{
  return lib_go (game, DIR_UP);
}

scr_bool
lib_cmd_go_down (scr_gameref_t game)
{
  return lib_go (game, DIR_DOWN);
}

scr_bool
lib_cmd_go_in (scr_gameref_t game)
{
  return lib_go (game, DIR_IN);
}

scr_bool
lib_cmd_go_out (scr_gameref_t game)
{
  return lib_go (game, DIR_OUT);
}

/*
 * lib_cmd_just_a_direction()
 *
 * Every Runner ends generaltasks' verb sweep with a pair of branches that
 * answer anything still containing the whole word `go` or `enter` -- run370
 * loc_43DD8B / loc_43DDB4, run380 loc_44481C / loc_444845, run390 loc_45DF66
 * / loc_45DF83, run400 loc_489377 / loc_48938E.  Neither is guarded on the
 * response line being empty, so they overwrite whatever an earlier branch
 * had to say; `enter mansion` gets this and not the generic
 * unknown-verb-with-object reply.  A bare `enter`, `in`, `out` and the rest
 * never reach it -- those are movement words, handled above.
 */
scr_bool
lib_cmd_just_a_direction (scr_gameref_t game)
{
  return lib_print_message (game, "Just a direction will do.\n");
}


/*
 * lib_cmd_just_a_direction_pre_390()
 *
 * `go <somewhere>` only reaches the Runner's gotoplace() from 3.9 on.  In 3.7
 * and 3.8 the sub is guarded on the whole word `goto`, or on a `go to ` with
 * an argument, and nothing else (run370 loc_42B994, run380 loc_431B8D); 3.9
 * and 4.0 relaxed the second half of that test to a bare `go ` prefix (run390
 * loc_43C764, run400 loc_46494C).  So under the older Runners a `go bedroom`
 * is not a room request at all -- it reaches no direction and no place, and
 * generaltasks answers it with the nudge above.
 */
scr_bool
lib_cmd_just_a_direction_pre_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;

  return lib_cmd_just_a_direction (game);
}

scr_bool
lib_cmd_go_northeast (scr_gameref_t game)
{
  return lib_go (game, DIR_NORTHEAST);
}

scr_bool
lib_cmd_go_southeast (scr_gameref_t game)
{
  return lib_go (game, DIR_SOUTHEAST);
}

scr_bool
lib_cmd_go_northwest (scr_gameref_t game)
{
  return lib_go (game, DIR_NORTHWEST);
}

scr_bool
lib_cmd_go_southwest (scr_gameref_t game)
{
  return lib_go (game, DIR_SOUTHWEST);
}


/*
 * lib_skip_article()
 *
 * Bypass any "a"/"an"/"the" prefix on a filtered, normalized room name,
 * returning the name trimmed of it.
 */
static scr_char *
lib_skip_article (scr_char *name)
{
  scr_char *skipped;

  if (scr_compare_word (name, "a", 1))
    skipped = name + 1;
  else if (scr_compare_word (name, "an", 2))
    skipped = name + 2;
  else if (scr_compare_word (name, "the", 3))
    skipped = name + 3;
  else
    skipped = name;

  return scr_trim_string (skipped);
}


/*
 * lib_compare_rooms()
 *
 * Helper for lib_cmd_go_room().  Compare the name of the passed in room
 * with the string passed in, and return TRUE if they match.  The routine
 * requires that string is filtered, stripped, trimmed and normalized.
 */
static scr_bool
lib_compare_rooms (scr_gameref_t game, scr_int room, const scr_char *string)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_char *name, *compare_name;
  scr_bool status;

  /* Get the name of the room, and filter it down to a plain string. */
  name = pf_filter (lib_get_room_name (game, room), vars, bundle);
  pf_strip_tags (name);
  scr_normalize_string (scr_trim_string (name));

  /* Bypass any prefix on the room name. */
  compare_name = lib_skip_article (name);

  /* Compare strings, then free the allocated name. */
  status = scr_strcasecmp (compare_name, string) == 0;
  scr_free (name);

  return status;
}


/*
 * lib_cmd_go_room()
 *
 * A weak replica of the Runner's claimed ability to go to a named room via
 * rooms that have already been visited using a shortest-path search.  This
 * version scans adjacent rooms for accessibility, and then generates the
 * required directional move for any unique match.
 *
 * Note that rooms can have the same name after they've been cleaned up for
 * text comparisons, for example, two "Manor Grounds" at the start of Humbug,
 * differentiated within the game with trailing "<some_tag>" components.
 */
scr_bool
lib_cmd_go_room (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool is_trapped, is_ambiguous;
  scr_int direction, destination, index_;
  const scr_char *const *dirnames;
  scr_char *name, *compare_name;

  /* Determine the requested room, and filter it down to a plain string. */
  name = pf_filter (var_get_ref_text (vars), vars, bundle);
  pf_strip_tags (name);
  scr_normalize_string (scr_trim_string (name));

  /* Bypass any prefix on the request room name. */
  compare_name = lib_skip_article (name);

  /* See if the named room is the current player room. */
  if (lib_compare_rooms (game, gs_playerroom (game), compare_name))
    {
      pf_buffer_string (filter, "You are already there!\n");
      scr_free (name);
      return TRUE;
    }

  /* Decide on four or eight point compass names list. */
  dirnames = lib_compass_names (game);

  /* Search adjacent and available rooms for a name match. */
  is_trapped = TRUE;
  is_ambiguous = FALSE;
  direction = -1;
  destination = -1;
  for (index_ = 0; dirnames[index_]; index_++)
    {
      scr_int location;

      if (lib_room_exit_available (game, gs_playerroom (game), index_))
        {
          is_trapped = FALSE;

          /*
           * Room is available.  Compare its name with that requested provided
           * that it's a location we've not already accepted (that is, some
           * rooms are reachable by multiple directions, such as both "south"
           * and "out").
           */
          if (lib_room_exit_destination (game, index_, &location)
              && location != destination
              && lib_compare_rooms (game, location, compare_name))
            {
              if (direction != -1)
                is_ambiguous = TRUE;
              direction = index_;
              destination = location;
            }
        }
    }
  scr_free (name);

  /* If trapped or it's unclear where to go, handle these cases. */
  if (is_trapped)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                      "You can't go in any direction!\n",
                                      "I can't go in any direction!\n",
                                      "%player% can't go in any direction!\n"));
      return TRUE;
    }
  else if (is_ambiguous)
    {
      pf_buffer_string (filter,
                        "I'm not clear about where you want to go."
                        "  Please try using just a direction.\n");
      pf_buffer_character (filter, '\n');
      lib_cmd_print_room_exits (game);
      return TRUE;
     }

  /* If no match, note it, otherwise handle as standard directional move. */
  if (direction == -1)
    {
      pf_buffer_string (filter, "I don't know how to get there from here.\n");
      pf_buffer_character (filter, '\n');
      lib_cmd_print_room_exits (game);
      return TRUE;
    }

  return lib_go (game, direction);
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
  scr_int task, object;
  lib_list_t list;
  const scr_char *description, *position = NULL;

  /* Get selection task. */
  vt_key[0].string = "Globals";
  vt_key[1].string = "Task";
  task = prop_get_integer (bundle, "I<-ss", vt_key) - 1;

  /* Select either the main or the alternate description. */
  if (task >= 0 && gs_task_done (game, task))
    vt_key[1].string = "AltDesc";
  else
    vt_key[1].string = "PlayerDesc";

  /* Print the description, or default response. */
  description = prop_get_string (bundle, "S<-ss", vt_key);
  if (!scr_strempty (description))
    pf_buffer_string (filter, description);
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

  /* If not just standing on the floor, say more. */
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
                              ? strchr (".!)%?", last) != NULL
                              : last == '.';
      if (!closed)
        pf_buffer_character (filter, '.');
    }

  /* Find and list each object worn by the player. */
  for (object = 0; object < gs_object_count (game); object++)
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

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_disambiguate_npc()
 *
 * Filter, then search the set of NPC matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 */
static scr_int
lib_disambiguate_npc (scr_gameref_t game,
                      const scr_char *verb, scr_bool *is_ambiguous)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, index_, npc, listed;

  /*
   * Filter out all referenced NPCs not actually visible or seen.  Count the
   * number of NPCs remaining as referenced by the last command, and note the
   * last referenced NPC, for where count is 1.
   */
  count = 0;
  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_]
          && gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          npc = index_;
        }
      else
        game->npc_references[index_] = FALSE;
    }

  /* If the reference is unambiguous, set in variables and return it. */
  if (count == 1)
    {
      /* Set this NPC as the referenced character. */
      var_set_ref_character (vars, npc);

      /* Return, setting no ambiguity. */
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      return npc;
    }

  /* If nothing referenced, return no NPC. */
  if (count == 0)
    {
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      else
        {
          pf_buffer_string (filter,
                            "Please be more clear, who do you want to ");
          pf_buffer_string (filter, verb);
          pf_buffer_string (filter, "?\n");
        }
      return -1;
    }

  /* The NPC reference is ambiguous, so list the choices. */
  pf_buffer_string (filter, "Please be more clear, who do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");

  pf_new_sentence (filter);
  listed = 0;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_])
        {
          lib_print_npc_np (game, index_);
          listed++;
          if (listed < count)
            pf_buffer_string (filter, (listed < count - 1) ? ", " : " or ");
        }
    }
  pf_buffer_string (filter, "?\n");

  /* Return no NPC for an ambiguous reference. */
  if (is_ambiguous)
    *is_ambiguous = TRUE;
  return -1;
}


/*
 * lib_co_contains()
 * lib_co_lastword()
 * lib_runner_co_scan()
 * lib_co_ambiguity_prompt()
 * lib_trace_runner_co()
 *
 * The pre-4.0 Runners' object-ambiguity test, and the end-of-turn prompt it
 * raises.  Scarier's own `%object%` matcher is positional, so `take truck
 * keys` binds only the truck keys; the Runner has no positional matcher and
 * asks instead.
 *
 * The Runner matches an object by scanning the *whole* typed command for its
 * Short name or, failing that, its Alias (run380 `c()` @429048: a
 * case-insensitive InStr whose hit must start at the string start or after a
 * space, and end at the string end, a space or a comma).  Its `co()`
 * @42DE60 (run370 @4261B4, run390 @43B6BC) then takes the term that
 * matched, counts every object *present* (obhere: in the room, carried,
 * worn, or in/on something here) whose Short or Alias is exactly that term,
 * and if more than one is present flags the command ambiguous by stamping
 * the object's number into MemVar_44F124 (@42DDC7).  One escape hatch: if
 * the player also typed the last word of the object's own Prefix ("take
 * *silver* key"), @42DD4C stamps the resolved marker &HFE instead, and that
 * marker outranks any ambiguity flagged by any other object in the same
 * scan (@42DDC1 only writes an object number when the marker is not already
 * set).  The list is built once, by the first ambiguous object (@42DC1E:
 * every present object answering to that term, tense(Prefix) & " " & Short,
 * joined with ", " and " or "); the prompt's term comes from the LAST
 * flagged object.
 *
 * generaltasks() runs that scan over every object at the start of EVERY
 * command (run380 loc_441D5D, straight after the built-in input rewrites),
 * so the flag is raised whatever handler goes on to answer the line.  It is
 * read at the very end of the turn, after events have ticked (@4431B0,
 * `If (MemVar_44F124 < 0) Or (MemVar_44F12C = 1)`): unless a game task ran
 * (MemVar_44F12C, set at 44D0BA inside the task executor) the turn's whole
 * output is thrown away and replaced by
 *
 *     Which <term>.  <The X, the Y or the Z>?
 *
 * (@4432AA with the Short when the player typed it, @443303 with the Alias
 * otherwise).  Everything the turn DID still stands: mikes.taf cmd 27 `take
 * truck keys`, with the carried mustang keys and the truck keys both
 * aliased "keys", answers "Which keys.  The mustang keys or the truck
 * keys?" -- and the truck keys are taken, because `drive truck bob` works
 * 30 commands later (run380 under Wine, Adven_8_mikes.rtf and the
 * 2026-09-04 re-drive; an earlier note that the keys were NOT taken was
 * wrong).  The next command is not eaten as an answer -- `east` after the
 * prompt simply moves east.  4.0 narrows differently (the up-front word
 * score of Proc_21_58_463640, see lib_absent_seen_object()) and never raises this
 * prompt from the dispatcher, so the port stops at 3.9.
 */
static scr_bool
lib_co_contains (const scr_char *command, const scr_char *term)
{
  scr_int term_length, index_;

  if (!command || !term || term[0] == NUL)
    return FALSE;

  term_length = strlen (term);
  for (index_ = 0; command[index_] != NUL; index_++)
    {
      scr_char after;

      if (scr_strncasecmp (command + index_, term, term_length) != 0)
        continue;
      if (index_ > 0 && command[index_ - 1] != ' ')
        continue;

      after = command[index_ + term_length];
      return after == NUL || after == ' ' || after == ',';
    }
  return FALSE;
}

static const scr_char *
lib_co_lastword (const scr_char *string)
{
  const scr_char *space;

  if (!string)
    return NULL;
  space = strrchr (string, ' ');
  return space ? space + 1 : string;
}

/* TRUE if the object's Short or Alias is exactly the term. */
static scr_bool
lib_co_object_answers_to (scr_gameref_t game, scr_int object,
                          const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (shortname && scr_strcasecmp (shortname, term) == 0)
    return TRUE;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Alias";
  alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *alias_name;

      vt_key[3].integer = alias;
      alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (alias_name && alias_name[0] != NUL
          && scr_strcasecmp (alias_name, term) == 0)
        return TRUE;
    }
  return FALSE;
}

/*
 * Reproduce the scan.  Returns TRUE when the Runner would prompt, with
 * *prompt_term the last flagged object's term, *list_term the term the list
 * was built from (the first ambiguous object's), and *present that first
 * object's count of present namesakes.
 */
static scr_bool
lib_runner_co_scan (scr_gameref_t game, const scr_char *command,
                    const scr_char **prompt_term, const scr_char **list_term,
                    scr_int *present_count)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, room;
  const scr_char *flagged_term = NULL, *first_term = NULL;
  scr_int first_present = 0;
  scr_bool resolved = FALSE;

  if (!command)
    return FALSE;
  room = gs_playerroom (game);

  for (object = 0; object < gs_object_count (game); object++)
    {
      const scr_char *shortname, *prefix, *term;
      scr_int alias_count, alias, other, present;

      shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
      prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");

      /* Pick the term the Runner would have matched on: Short, then Alias. */
      term = NULL;
      if (lib_co_contains (command, shortname))
        term = shortname;
      else
        {
          scr_vartype_t vt_key[4];

          vt_key[0].string = "Objects";
          vt_key[1].integer = object;
          vt_key[2].string = "Alias";
          alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
          for (alias = 0; alias < alias_count; alias++)
            {
              const scr_char *alias_name;

              vt_key[3].integer = alias;
              alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
              if (lib_co_contains (command, alias_name))
                {
                  term = alias_name;
                  break;
                }
            }
        }
      if (!term)
        continue;

      /* Count present objects whose Short or Alias is exactly that term. */
      present = 0;
      for (other = 0; other < gs_object_count (game); other++)
        {
          if (obj_indirectly_in_room (game, other, room)
              && lib_co_object_answers_to (game, other, term))
            present++;
        }

      if (present > 1)
        {
          if (!first_term)
            {
              first_term = term;
              first_present = present;
            }
          if (lib_co_contains (command, lib_co_lastword (prefix)))
            resolved = TRUE;
          else if (!resolved)
            flagged_term = term;
        }
    }

  if (!flagged_term || resolved)
    return FALSE;
  if (prompt_term)
    *prompt_term = flagged_term;
  if (list_term)
    *list_term = first_term;
  if (present_count)
    *present_count = first_present;
  return TRUE;
}

scr_bool
lib_co_ambiguity_prompt (scr_gameref_t game, const scr_char *command)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *prompt_term, *list_term;
  scr_int present, room, object, listed;

  /*
   * 3.7 and 3.8 only.  run390 still has co() (@43B6BC) and the same
   * end-of-turn replacement (@4607BC/460832, gated on its own task-ran
   * flag MemVar_468198), but no longer runs the scan from generaltasks():
   * its co() is called only from characters() (@459436/45A025/45A0F0:
   * "give"/"attack ... with" style character commands) and sitstand()
   * (@44413E-444827).  hangover.taf `ask doctor about approval form` with
   * two approval forms present is answered by the doctor in run390
   * (Adrift_1_hangover_run390.txt), where the 3.8 scan would have prompted.
   * Those two 3.9 handlers are not ported; see WINE-TRANSCRIPTS-TODO.md.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;
  if (!lib_runner_co_scan (game, command, &prompt_term, &list_term, &present))
    return FALSE;

  /* The whole turn's output goes; only the prompt is shown. */
  pf_empty (filter);
  pf_buffer_string (filter, "Which ");
  pf_buffer_string (filter, prompt_term);
  pf_buffer_string (filter, ".  ");

  room = gs_playerroom (game);
  listed = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (!obj_indirectly_in_room (game, object, room)
          || !lib_co_object_answers_to (game, object, list_term))
        continue;

      if (listed > 0)
        pf_buffer_string (filter, listed == present - 1 ? " or " : ", ");
      else
        pf_new_sentence (filter);
      lib_print_object_np (game, object);
      listed++;
    }
  pf_buffer_string (filter, "?");
  pf_buffer_character (filter, '\n');
  return TRUE;
}

/*
 * lib_co_400_*()
 *
 * The 4.0 object-ambiguity prompt, its pending question, and the answer
 * slot the question opens.  Measured 2026-09-07 with harness/make_400_coprobe.py
 * -> p4CO.taf, two rooms and eight static objects sharing one description so
 * that only the CHOICE shows: two trees whose Short is "tree", a rock (the
 * unique control), a mustang key and a truck key both aliased "keys", a hut
 * aliased "shed" beside an object whose Short IS "shed", and a third "tree"
 * in the far room as the presence control.  One task `poke %object%`, the
 * game's DontUnderstand "NO IDEA.", and a one-shot event printing "TICK." so
 * that a swallowed turn shows.  Transcripts Adrift_924-930, run400 under
 * Wine (feeds cmdfile_co.txt .. cmdfile_co6.txt in the harness prefix).
 *
 * The gate comment on lib_co_ambiguity_prompt() above used to say 4.0 "never
 * raises this prompt from the dispatcher, so the port stops at 3.9".  It does
 * raise it -- from the two handlers rather than from the turn driver, under
 * two different tests (Adrift_926, every cell isolated by a neutral `look`):
 *
 *     chop tree   ->  Which tree.  The red tree or the blue tree?
 *     x    tree   ->  Which tree.  The red tree or the blue tree?
 *     chop shed   ->  NO IDEA.
 *     x    shed   ->  Which shed.  The hut or the shed?
 *     chop keys   ->  NO IDEA.
 *     x    keys   ->  Which keys.  The mustang key or the truck key?
 *     chop rock   ->  I don't understand what you want me to do with the rock.
 *     x    rock   ->  A plain thing.
 *
 * So the library EXAMINE path prompts whenever two or more PRESENT objects
 * answer to the typed term by Short or by Alias ("shed" is one Short plus one
 * alias, "keys" two aliases; both prompt), while the unhandled-verb path
 * prompts only where the term is the SHORT of every tied candidate -- a
 * Short+alias or alias+alias tie is not ambiguous enough for it and the
 * game's DontUnderstand comes out instead.  Presence really is filtered: with
 * only the far room's tree present, `chop tree` gives the plain "I don't
 * understand what you want me to do with the tree." (Adrift_924).
 *
 * The wording is the 3.7/3.8 one already ported above -- `Which <term>.
 * <NP> or <NP>?`, a full stop, two spaces, the noun phrases in index order
 * joined ", " / " or " -- which is why lca T91's two identically named trees
 * read "Which tree.  The tree or the tree?".
 *
 * What counts as a name, what gets listed, and where the term comes from
 * (Adrift_928-930):
 *
 *     chop key        ->  NO IDEA.
 *     x    key        ->  You see no such thing.
 *     chop mustang    ->  NO IDEA.
 *     tree            ->  Which tree.  The red tree or the blue tree?
 *     rock            ->  I don't understand what you want me to do with the
 *                         rock.
 *     x    tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
 *     x    rock tree  ->  Which tree.  The red tree, the blue tree or the rock?
 *     chop tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
 *
 * A name matches whole or not at all -- "key" is a word inside two Shorts and
 * names nothing -- so an ambiguity 4.0 prompts about is always a shared whole
 * name.  The list is every object the LINE referenced, in index order, and
 * not just the term's namesakes (the rock is listed under "Which tree"), and
 * the term comes from the lowest-indexed ambiguous object rather than from
 * the order the nouns were typed (`x rock tree` still says "Which tree").  No
 * verb is needed for either path: a bare `tree` prompts, a bare `rock` gets
 * the unhandled-verb catch-all naming it.
 *
 * Neither the prompt nor any of its answers is a turn.  The probe's ticker
 * has StarterType 1 and Time1 = Time2 = 1, so its "TICK." lands on the first
 * real turn of the session: Adrift_925 prints it after the opening `look`,
 * Adrift_926 after the `look` that FOLLOWS `chop tree`, and Adrift_927 after
 * the `look` that follows both `x keys` and its answer `mustang`.  Every
 * prompt and every answer is therefore administrative.
 *
 * The prompt leaves a question pending and the NEXT line is read against it
 * (Adrift_925/927, and lca):
 *
 *     x keys / mustang     ->  That is still ambiguous!
 *     chop tree / red      ->  I don't understand what you want me to do with
 *                              the red tree.
 *     x tree / zzz         ->  That is still ambiguous!
 *     chop tree / x tree   ->  That is still ambiguous!
 *     x shed / chop keys   ->  That is still ambiguous!
 *     x tree rock / rock   ->  That is still ambiguous!
 *     x tree rock / blue   ->  A plain thing.
 *     x tree rock / x rock ->  A plain thing.
 *     x keys / x rock      ->  A plain thing.
 *     chop tree / look     ->  (the room description)
 *     chop tree / n        ->  (lca Adrift_328_lca.txt:738 -- the player moves)
 *
 * The rule that fits all of them is not "the next line is an answer": it is
 * that the pending question changes only the places a line can end up with
 * nothing to say.
 *
 *   - A line that DID something runs normally and drops the question
 *     (`look`, `n`, `x rock`).
 *   - A line that raises a NEW ambiguity prints "That is still ambiguous!"
 *     instead of a second full prompt (`x tree`, and `chop keys`, whose
 *     alias tie the unhandled-verb path would otherwise pass over to
 *     DontUnderstand).
 *   - A line that did nothing goes to the answer slot, and its own output
 *     goes with it: that is either the game's DontUnderstand text or the
 *     unhandled-verb catch-all.  `rock` / `x rock` is the pair that settles
 *     the second half -- bare `rock` gets the catch-all in isolation
 *     (Adrift_930), so with a question open it is claimed and answers "That
 *     is still ambiguous!", while `x rock` examines the rock.
 *
 * In the slot the typed words are taken as extra adjectives in front of the
 * pending noun and the prompt's own candidates re-scored with the ordinary
 * 4.0 noun score.  Exactly one winner re-runs the ORIGINAL command on that
 * object -- `red` scores the red tree 2 (Short plus the Prefix word) against
 * the blue tree's 1, so `chop tree` is re-run and its catch-all names the red
 * tree.  Anything else prints "That is still ambiguous!": `mustang keys` ties
 * 1-1 on the two aliases, `zzz tree` ties on the two Shorts, and `rock tree`
 * ties three ways -- the answer is scored against the pending term, so
 * naming a listed object outright does NOT pick it.
 *
 * Either answer clears the question: Adrift_925's `x keys` gets the full
 * prompt again immediately after `chop keys` had answered "That is still
 * ambiguous!".  The sibling string "That wasn't one of the options!" was
 * never triggered by any cell and is still unexplained.
 */
static scr_bool lib_input_contains_word (const scr_char *input,
                                         const scr_char *word);
static scr_int lib_verb_object_name_score (scr_gameref_t game, scr_int object,
                                           const scr_char *input);

/* The open question, and the object an answer resolved it to. */
static scr_bool lib_co_400_pending = FALSE;
static scr_bool lib_co_400_refused = FALSE;
static scr_bool lib_co_400_was_pending = FALSE;
static std::string lib_co_400_term;
static std::string lib_co_400_command;
static std::vector<scr_int> lib_co_400_candidates;
static scr_int lib_co_400_forced_object = -1;

void
lib_co_400_reset (void)
{
  lib_co_400_pending = FALSE;
  lib_co_400_was_pending = FALSE;
  lib_co_400_term.clear ();
  lib_co_400_command.clear ();
  lib_co_400_candidates.clear ();
  lib_co_400_forced_object = -1;
  lib_co_400_refused = FALSE;
}

/* Called once per typed line element, before it is dispatched. */
void
lib_co_400_begin_line (void)
{
  lib_co_400_was_pending = lib_co_400_pending;
  lib_co_400_pending = FALSE;
  lib_co_400_refused = FALSE;
}

scr_bool
lib_co_400_question_pending (void)
{
  return lib_co_400_was_pending;
}

const scr_char *
lib_co_400_pending_command (void)
{
  return lib_co_400_command.c_str ();
}

/*
 * The object an answer picked, honoured by both resolvers while the original
 * command is re-run so that the re-run cannot raise the same question again.
 */
scr_int
lib_co_400_forced (void)
{
  return lib_co_400_forced_object;
}

void
lib_co_400_set_forced (scr_int object)
{
  lib_co_400_forced_object = object;
}

/*
 * The unhandled-verb catch-all leaves the turn as empty-handed as the
 * DontUnderstand path does, so a pending question claims that line too; see
 * the answer slot in run_process_input_line().
 */
void
lib_co_400_note_refusal (void)
{
  lib_co_400_refused = TRUE;
}

scr_bool
lib_co_400_line_refused (void)
{
  return lib_co_400_refused;
}

void
lib_co_400_print_still_ambiguous (scr_gameref_t game)
{
  pf_buffer_string (gs_get_filter (game), "That is still ambiguous!\n");
  game->is_admin = TRUE;
}

/* How many of the candidates answer to exactly this name. */
static scr_int
lib_co_400_namesake_count (scr_gameref_t game,
                           const std::vector<scr_int> &objects,
                           const scr_char *term)
{
  scr_int index_, count;

  count = 0;
  for (index_ = 0; index_ < (scr_int) objects.size (); index_++)
    {
      if (lib_co_object_answers_to (game, objects[index_], term))
        count++;
    }
  return count;
}

/*
 * Raise the question.  With one already open the Runner does not print a
 * second prompt, only the short refusal; either way the line is
 * administrative and the question that was open is now spent.
 */
static void
lib_co_400_raise (scr_gameref_t game, const scr_char *term,
                  const std::vector<scr_int> &objects)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *command;
  scr_int index_;

  game->is_admin = TRUE;

  if (lib_co_400_was_pending)
    {
      pf_buffer_string (filter, "That is still ambiguous!\n");
      return;
    }

  pf_buffer_string (filter, "Which ");
  pf_buffer_string (filter, term);
  pf_buffer_string (filter, ".  ");

  pf_new_sentence (filter);
  for (index_ = 0; index_ < (scr_int) objects.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == (scr_int) objects.size () - 1
                          ? " or " : ", ");
      lib_print_object_np (game, objects[index_]);
    }
  pf_buffer_string (filter, "?");
  pf_buffer_character (filter, '\n');

  command = run_get_dispatch_input ();
  lib_co_400_pending = TRUE;
  lib_co_400_term = term;
  lib_co_400_command = command ? command : "";
  lib_co_400_candidates = objects;
}

/*
 * The examine path's test.  The candidates are every object the line
 * referenced, in index order, and the question's term is the first name --
 * Short first, then Alias -- that the line contains and that two or more of
 * those candidates answer to.  Measured on p4CO with run400
 * (Adrift_928/929): `x tree rock` and `x rock tree` both answer
 * "Which tree.  The red tree, the blue tree or the rock?", so the list is
 * the whole reference set and not just the term's namesakes, and the term
 * comes from the lowest-indexed ambiguous object rather than from the order
 * the nouns were typed in.
 */
static scr_bool
lib_co_400_raise_for_references (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  std::vector<scr_int> referenced;
  scr_int object, index_;

  if (!input)
    return FALSE;

  for (object = 0; object < gs_object_count (game); object++)
    {
      if (game->object_references[object])
        referenced.push_back (object);
    }
  if (referenced.size () < 2)
    return FALSE;

  for (index_ = 0; index_ < (scr_int) referenced.size (); index_++)
    {
      const scr_char *names[1 + 8];
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias, count, name;

      object = referenced[index_];

      count = 0;
      names[count++] = prop_get_indexed_string (bundle, "Objects",
                                                object, "Short");
      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (alias = 0; alias < alias_count && count < 1 + 8; alias++)
        {
          vt_key[3].integer = alias;
          names[count++] = prop_get_string (bundle, "S<-sisi", vt_key);
        }

      for (name = 0; name < count; name++)
        {
          if (scr_strempty (names[name])
              || !lib_co_contains (input, names[name]))
            continue;
          if (lib_co_400_namesake_count (game, referenced, names[name]) < 2)
            continue;

          lib_co_400_raise (game, names[name], referenced);
          return TRUE;
        }
    }

  return FALSE;
}

/*
 * The unhandled-verb path's test.  The candidates are the objects the 4.0
 * noun score tied on, and the term is the Short of the lowest-indexed
 * candidate that the line contains and that two or more of them share --
 * aliases do not count here, which is why run400 answers `chop shed` (the
 * hut answers to "shed" only by alias) with the game's DontUnderstand while
 * `x shed` raises the question.  A candidate that shares no name still gets
 * listed: `chop tree rock` prompts with all three (Adrift_929).
 */
static scr_bool
lib_co_400_raise_for_short_tie (scr_gameref_t game,
                                const std::vector<scr_int> &tied)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;

  if (!input || tied.size () < 2)
    return FALSE;

  for (index_ = 0; index_ < (scr_int) tied.size (); index_++)
    {
      const scr_char *term;
      scr_int other, count;

      term = prop_get_indexed_string (bundle, "Objects", tied[index_],
                                      "Short");
      if (scr_strempty (term) || !lib_input_contains_word (input, term))
        continue;

      count = 0;
      for (other = 0; other < (scr_int) tied.size (); other++)
        {
          const scr_char *name;

          name = prop_get_indexed_string (bundle, "Objects", tied[other],
                                          "Short");
          if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
            count++;
        }
      if (count < 2)
        continue;

      lib_co_400_raise (game, term, tied);
      return TRUE;
    }

  return FALSE;
}

/*
 * The answer slot.  Returns the object the answer picked, or -1 for "That is
 * still ambiguous!", which it prints itself.
 */
scr_int
lib_co_400_answer_object (scr_gameref_t game, const scr_char *line)
{
  std::string phrase;
  scr_int index_, best, best_count, object;

  phrase = line ? line : "";
  if (!phrase.empty ())
    phrase += ' ';
  phrase += lib_co_400_term;

  object = -1;
  best = 0;
  best_count = 0;
  for (index_ = 0; index_ < (scr_int) lib_co_400_candidates.size (); index_++)
    {
      const scr_int candidate = lib_co_400_candidates[index_];
      const scr_int score = lib_verb_object_name_score (game, candidate,
                                                        phrase.c_str ());

      if (score > best)
        {
          object = candidate;
          best = score;
          best_count = 1;
        }
      else if (score == best)
        best_count++;
    }

  if (best == 0 || best_count > 1)
    return -1;
  return object;
}


#ifdef SCARIER_DUMP_TOOLS
/*
 * SCR_TRACE_CO: report where the Runner's test disagrees with ours at each
 * lib_disambiguate_object_common() call.  `ours=` is our own post-filter
 * reference count, so ours=1 is a real divergence and ours>1 means only the
 * prompt's wording differed before lib_co_ambiguity_prompt() existed.  A
 * measurement harness only: it changes nothing.
 */
static void
lib_trace_runner_co (scr_gameref_t game, const scr_char *verb, scr_int count)
{
  static const scr_bool trace_co = getenv ("SCR_TRACE_CO") != NULL;
  const scr_char *command, *ambig_term;
  scr_int ambig_present;

  if (!trace_co)
    return;
  command = run_get_dispatch_input ();
  if (lib_runner_co_scan (game, command, &ambig_term, NULL, &ambig_present))
    fprintf (stderr, "CO-AMBIG verb=[%s] input=[%s] term=[%s]"
             " present=%ld ours=%ld\n",
             verb ? verb : "", command, ambig_term, ambig_present, count);
}
#endif

/*
 * lib_disambiguate_object_common()
 * lib_disambiguate_object()
 *
 * Filter, then search the set of object matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 *
 * If normal disambiguation returns more than one object, the resolver
 * function, if supplied, is used to see if the multiple objects can be
 * resolved into just one object.  The resolver function can normally be the
 * same as the function used to filter objects for multiple references.
 */
static scr_int
lib_disambiguate_object_common (scr_gameref_t game, const scr_char *verb,
                               scr_bool (*resolver)
                                   (scr_gameref_t, scr_int, scr_int),
                               scr_int resolver_arg,
                               scr_bool *is_ambiguous)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_bool requires_seen = lib_matcher_requires_seen (game);
  scr_int count, index_, object, listed;

  /*
   * Filter out all referenced objects not actually visible or seen.  Count
   * the number of objects remaining as referenced by the last command, and
   * note the last referenced object, for where count is 1.  Version 3.8
   * games skip the seen test -- see lib_matcher_requires_seen().
   */
  count = 0;
  object = -1;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (game->object_references[index_]
          && (!requires_seen || gs_object_seen (game, index_))
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          object = index_;
        }
      else
        game->object_references[index_] = FALSE;
    }

#ifdef SCARIER_DUMP_TOOLS
  lib_trace_runner_co (game, verb, count);
#endif

  /*
   * An answer to a 4.0 ambiguity prompt re-runs the original command with
   * its object already picked, so the question cannot be raised twice; see
   * lib_co_400_answer_object().
   */
  if (count > 1 && lib_co_400_forced () >= 0
      && game->object_references[lib_co_400_forced ()])
    {
      object = lib_co_400_forced ();
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        game->object_references[index_] = (index_ == object);
      count = 1;
    }

  /*
   * If this reference is ambiguous and a resolver was supplied, try to
   * resolve it unambiguously by calling the resolver filter on the remaining
   * set references.
   */
  if (resolver && count > 1)
    {
      scr_int retry_count;

      /*
       * Search for objects accepted by the resolver filter, but don't filter
       * references just yet.  Again, note the last referenced.
       */
      retry_count = 0;
      object = -1;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (game->object_references[index_]
              && resolver (game, index_, resolver_arg))
            {
              retry_count++;
              object = index_;
            }
        }

      /* See if we narrowed the field without eliminating every object. */
      if (retry_count > 0 && retry_count < count)
        {
          /*
           * If we got down to a single object, the ambiguity is resolved.
           * In this case, set count to 1 so that 'object' is returned.
           */
          if (retry_count == 1)
            count = retry_count;
          else
            {
              /*
               * We got down to fewer objects; reduce references so that the
               * disambiguation message is clearer.  Note that here we still
               * leave with count greater than 1.
               */
              count = 0;
              for (index_ = 0; index_ < gs_object_count (game); index_++)
                {
                  if (game->object_references[index_]
                      && resolver (game, index_, resolver_arg))
                    count++;
                  else
                    game->object_references[index_] = FALSE;
                }
            }
        }
    }

  /* If the reference is unambiguous, set in variables and return it. */
  if (count == 1)
    {
      /* Set this object as referenced. */
      var_set_ref_object (vars, object);

      /* Return, setting no ambiguity. */
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      return object;
    }

  /* If nothing referenced, return no object. */
  if (count == 0)
    {
      if (is_ambiguous)
        *is_ambiguous = FALSE;
      else
        {
          pf_buffer_string (filter,
                            "Please be more clear, what do you want to ");
          pf_buffer_string (filter, verb);
          pf_buffer_string (filter, "?\n");
        }
      return -1;
    }

  /*
   * 4.0 asks the Runner's own question instead.  "Please be more clear, what
   * do you want to <verb>?" is a SCARE invention -- the string is in none of
   * the four Runner binaries -- and what run400 really prints where two
   * present objects answer to the typed noun is the same "Which <term>.
   * <list>?" the 3.7/3.8 scan above raises.  Measured on the examine path
   * (see lib_co_400_raise()); the other library commands that disambiguate
   * an object were not measured, but they cannot be printing an invented
   * string either, so they share the wording here.
   */
  if (lib_is_version_400 (game) && lib_co_400_raise_for_references (game))
    {
      if (is_ambiguous)
        *is_ambiguous = TRUE;
      return -1;
    }

  /* The object reference is ambiguous, so list the choices. */
  pf_buffer_string (filter, "Please be more clear, what do you want to ");
  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, "?  ");

  pf_new_sentence (filter);
  listed = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (game->object_references[index_])
        {
          lib_print_object_np (game, index_);
          listed++;
          if (listed < count)
            pf_buffer_string (filter, (listed < count - 1) ? ", " : " or ");
        }
    }
  pf_buffer_string (filter, "?\n");

  /* Return no object for an ambiguous reference. */
  if (is_ambiguous)
    *is_ambiguous = TRUE;
  return -1;
}

static scr_int
lib_disambiguate_object (scr_gameref_t game,
                         const scr_char *verb, scr_bool *is_ambiguous)
{
  return lib_disambiguate_object_common (game, verb, NULL, -1, is_ambiguous);
}


/*
 * lib_absent_seen_object()
 * lib_cant_see_absent_object()
 *
 * 4.0's matcher runs a second pass.  When nothing the noun names is in the
 * room, it looks again over everything the player has *seen*, and the
 * handlers above it then answer "<player> can't see <it>" instead of their
 * own generic refusal.  Pre-4.0 has no such pass: run390's co() simply fails
 * to match an object that is elsewhere, and the command falls through to the
 * flat can't-do tail.
 *
 * Measured on p4EXAM.taf, one statue seen in the North Room and examined
 * from the Test Room (Adrift_1_p4exam.txt, all 32 commands echoed) against
 * p39EXAM.taf under run390 (Adrift_41/43_p39exam.txt):
 *
 *     command        run400                              run390
 *     x statue       You can't see the statue from here! Nothing special.
 *     open statue    You can't see the statue.           You can't open that.
 *     close statue   You can't see a statue.             You can't close that.
 *     buy statue     You can't see the statue.           I don't think that
 *                                                          is for sale.
 *
 * Note the article: `close` alone is indefinite.  That is not a stylistic
 * choice, it is a different piece of code -- run400's openclose() composes
 * the open half from the definite-name helper Proc_21_31_448710 (475966) and
 * the close half by hand, Prefix & " " & Short (475C10-475C2D).  `examine`
 * uses the definite helper and appends " from here!" (471958-471975), and
 * `buy` never reaches its own branch at all: therest()'s very first clause
 * (4887A0-4887F5) prints the definite form and exits before the whole verb
 * chain below it.
 *
 * Only those four verbs are measured, so only those four call this.  The
 * therest() clause is verb-wide in the Runner, and the tail of
 * lib_cmd_verb_object() already models the same sentence for anything that
 * reaches it; what is unmeasured is which of the other therest() verbs 4.0
 * intercepts on the way in.
 *
 * WHERE THE CALLS GO MATTERS.  Every one of the Runner's four sites is
 * guarded by "the output buffer is still empty" (471933, 475952, 475BFC,
 * 4887A0 all test MemVar_4941B0 = vbNullString), i.e. the clause speaks only
 * when nothing else in the turn has.  So the callers are four thin handlers
 * sitting immediately above the catch-all `*` rows in scrunner.cpp, not the
 * `%object%` handlers at the top of the table: humbug names an NPC and an
 * absent object both "robot", and unraveling_god an NPC and an absent object
 * both "people", and in each case the Runner and the golden print the NPC's
 * description.  Checking inside lib_cmd_examine_object() would have stolen
 * the turn from lib_cmd_examine_npc() one row below it.  Each `_absent` row
 * re-matches %object%, which repopulates the references that
 * lib_disambiguate_object_common() cleared on the way past.
 *
 * One deliberate deviation: the close half is spelled Prefix & " " & Short
 * with no fallback, so an object with an empty Prefix would give the Runner
 * "You can't see  statue."  lib_print_object() substitutes the usual "a "
 * there instead.  Nothing has measured an empty-Prefix object in this
 * position, and the double space is almost certainly not what 4.0 meant.
 *
 * The gate is the object's seen byte, +48 in the Runner's object record,
 * written the moment an object is listed or described (run400 471749,
 * 46A142) and read by every one of the four sites above.  It is Scarier's
 * gs_object_seen(), so this needs no new state.
 */
/*
 * Whole-word containment of a single name word in the typed line, run400
 * Proc_21_38_454CB0: case-insensitive, and a hit only where the word is
 * bounded by the line's ends or spaces.
 */
static scr_bool
lib_input_contains_word (const scr_char *input, const scr_char *word)
{
  const scr_int length = strlen (word);
  const scr_char *scan;

  if (length == 0)
    return FALSE;

  for (scan = input; *scan != NUL; scan++)
    {
      if ((scan == input || scan[-1] == ' ')
          && scr_strncasecmp (scan, word, length) == 0
          && (scan[length] == NUL || scan[length] == ' '))
        return TRUE;
    }

  return FALSE;
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
static scr_int lib_verb_object_name_score (scr_gameref_t game,
                                           scr_int object,
                                           const scr_char *input);

static scr_int
lib_absent_seen_object (scr_gameref_t game)
{
  const scr_char *input;
  scr_int index_, object, best, best_count;

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
   * PRESENT and so answers &HFF, "<player> see no such thing."  There is no
   * further seen-object pass in 457034: its var_90 set is the present
   * matches only, and the "co(i, 4)" at 456E6A is a single vestigial call,
   * not a loop (P-code checked 2026-09-06).
   *
   * Measured on cowboyblues (4.00, Adrift_330_cowboyblues.txt line 1070):
   * `x wall` in the Sheriff's Office after visiting the Back Room ("east
   * wall" 91, alias "wall") and Blood Alley ("walls" 96, alias "wall")
   * answers "You see no such thing." -- both score 1 on the alias and tie.
   * The old Short-word count here gave "east wall" 1 and "walls" 0 and
   * wrongly picked 91.  Measured on humbug (Adrift_4_humbug.txt lines
   * 1602-1604): `X machine` with only the washing machine (81, Short
   * "machine") seen answers "I can't see the washing machine from here!",
   * and `X chute` against several seen "chute"s ties and prints the ALR'd
   * "Nothing Special.".
   */
  input = run_get_dispatch_input ();
  object = -1;
  best = -1;
  best_count = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      scr_int score;

      if (!game->object_references[index_])
        continue;

      /* Something the noun names is here; the ordinary path handles it. */
      if (obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        return -1;

      if (!gs_object_seen (game, index_))
        continue;

      score = input ? lib_verb_object_name_score (game, index_, input) : 0;
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

  return best_count > 1 ? -1 : object;
}

static scr_bool
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
 * lib_list_npc_inventory()
 *
 * List objects carried and worn by an NPC.
 */
static scr_bool
lib_list_npc_inventory (scr_gameref_t game, scr_int npc, scr_bool is_described)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool wearing;
  lib_list_t list;

  /*
   * Find and list each object worn by the NPC.  Like container listings,
   * this reveals the objects -- the Runner marks NPC possessions seen
   * only when they're listed to the player.
   */
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_WORN_NPC
          && gs_object_parent (game, object) == npc)
        {
          list.push_back (object);
          gs_set_object_seen (game, object, TRUE);
        }
    }
  wearing = !list.empty ();
  if (wearing)
    {
      lib_new_clause (game, is_described);
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter, " is wearing ");
      lib_print_list (game, list, lib_print_object, " and ");
    }

  /* Find and list each object owned by the NPC. */
  list.clear ();
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_HELD_NPC
          && gs_object_parent (game, object) == npc)
        {
          list.push_back (object);
          gs_set_object_seen (game, object, TRUE);
        }
    }
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
scr_bool
lib_cmd_examine_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  scr_int npc, task, resource;
  scr_bool is_ambiguous;
  const scr_char *description;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "examine", &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  /* Get selection task. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Task";
  task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;

  /* Select either the main or the alternate description. */
  if (task >= 0 && gs_task_done (game, task))
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

  pf_buffer_character (filter, '\n');

  /*
   * In 4.0 examining a character is not a turn: run400 counts no turn,
   * moves no walker and ticks no event after it, exactly as it treats its
   * own `turns` and `score`.  3.9 counts it like any other command.
   *
   * Measured 2026-08-29 under Wine.  run400, arena probes EV14/EV15/EV16
   * (harness/make_arena_probe.py; Adrift_1_ev14.txt, _ev15.txt, _ev16.txt):
   * a one-turn event started by the previous command finishes on the `z`
   * AFTER `x bob`, not on `x bob` itself; a looping walker with enter/exit
   * lines prints nothing on that turn; and `turns` reads 0 after `x bob`,
   * `x carl` (empty description), `look at bob` and `examine bob`, while
   * `x me` and `x pebble` count.  run390, BobBobsly.taf (3.90),
   * Adrift_1_bob390.txt: `turns` climbs by one across `x bouncer`.
   *
   * This is Beanstalk turn 45: the stranger's greeting is a one-turn event
   * started by `sell cow`, the player examines him and walks east, and by
   * the time the Runner ticks the event again the player is no longer on
   * the road it is restricted to.
   */
  if (lib_is_version_400 (game))
    game->is_admin = TRUE;
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
  scr_int object;
  lib_list_t list;

  /* List out the containers contained in this container. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Contained? */
      if (gs_object_position (game, object) == OBJ_IN_OBJECT
          && gs_object_parent (game, object) == container)
        list.push_back (object);
    }
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
       */
      pf_buffer_string (filter, " is ");
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
  scr_int object;
  lib_list_t list;

  /* List out the objects contained in this object. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Contained? */
      if (gs_object_position (game, object) == OBJ_IN_OBJECT
          && gs_object_parent (game, object) == container)
        list.push_back (object);
    }
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
  scr_int object;
  lib_list_t list;

  /* List out the objects contained in this container. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_IN_OBJECT
          && gs_object_parent (game, object) == container)
        list.push_back (object);
    }
  if (!list.empty ())
    {
      pf_buffer_string (filter, ", and inside is ");
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
 * which one it picks used to be recorded here as "frankly, a mystery".  It
 * isn't: run400.exe selects purely on the *number* of contained objects.  The
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
static scr_bool
lib_list_in_object (scr_gameref_t game, scr_int container,
                    scr_bool is_described, scr_bool joined)
{
  scr_bool use_alternate_format = FALSE;
  scr_int object, count;

  /*
   * Count the objects this container holds.  Listing a container's
   * contents is also what reveals them: the Runner sets each listed
   * object's seen flag here, and until then the object can't be referred
   * to at all (in version 3.9 and later games).
   */
  count = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_IN_OBJECT
          && gs_object_parent (game, object) == container)
        {
          count++;
          gs_set_object_seen (game, object, TRUE);
        }
    }

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
  scr_int object;
  lib_list_t list;

  /* List out the objects standing on this surface. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Standing on? */
      if (gs_object_position (game, object) == OBJ_ON_OBJECT
          && gs_object_parent (game, object) == supporter)
        list.push_back (object);
    }
  if (!list.empty ())
    {
      if (is_described)
        pf_buffer_string (filter, "  ");
      pf_buffer_string (filter, "On ");
      lib_print_object_np (game, supporter);
      /* " is " unconditionally -- see lib_list_in_object_normal(). */
      pf_buffer_string (filter, " is ");
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
  scr_int object;
  lib_list_t list;

  /* List out the objects standing on this object. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Standing on? */
      if (gs_object_position (game, object) == OBJ_ON_OBJECT
          && gs_object_parent (game, object) == supporter)
        list.push_back (object);
    }
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
  scr_int object, count;

  /*
   * Count the objects this surface holds, marking each one seen as we
   * go -- as with containers, the listing is what reveals the contents
   * to the parser.
   */
  count = 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_ON_OBJECT
          && gs_object_parent (game, object) == supporter)
        {
          count++;
          gs_set_object_seen (game, object, TRUE);
        }
    }

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
 * 2026-08-25, Adrift_22_xfiles.txt line 9.  SCARE had it the other way and
 * as two sentences, "Inside Your Desk is ...  Your Coffee Mug and The Memo
 * are on Your Desk."  The single closing '.' is appended once at the end of
 * whatisinon (loc_46A8C6, and only if anything was added at all), so the
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
static scr_bool
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
      for (contained = 0; contained < gs_object_count (game); contained++)
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
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is ", " are "));

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
  scr_int object, task, openness;
  scr_bool is_described, is_statussed, is_mentioned, is_ambiguous, should_be;
  const scr_char *description, *resource;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "examine", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /*
   * Examining an object marks it seen.  This can matter in version 3.8
   * games, where the matcher doesn't require objects to have been seen.
   */
  gs_set_object_seen (game, object, TRUE);

  /* Begin assuming no description printed. */
  is_described = FALSE;

  /*
   * Get selection task and expected state; for the expected task state, FALSE
   * indicates task completed, TRUE not completed.
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Task";
  task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
  vt_key[2].string = "TaskNotDone";
  should_be = !prop_get_boolean (bundle, "B<-sis", vt_key);

  /* Select either the main or the alternate description. */
  if (task >= 0 && gs_task_done (game, task) == should_be)
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
      is_described |= TRUE;
    }

  /* Handle any associated resource. */
  vt_key[2].string = resource;
  res_handle_resource (game, "sis", vt_key);

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
         * open." (Adrift_3_gamma.txt).  run400 instead composes the name
         * with the tensed prefix (Proc_21_31_448710 at 4717D1): man
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
        is_described |= TRUE;
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
   * Measured on p39EXAM.taf (3.90), Adrift_41_p39exam.txt: `x stone` -- the
   * stone is in the room, has an empty Description, and answers "Nothing
   * special."  Adrift_43_p39exam.txt repeats it with the stone held, and
   * answers the same, so being carried makes no difference.  The 4.0 twin
   * p4EXAM.taf, Adrift_1_p4exam.txt, answers "You see nothing special about
   * the stone."  ms_mobius_solution is the corpus's only pre-4.0 exposure.
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

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_save_game_references()
 * lib_restore_game_references()
 *
 * Helpers for trying game commands.  Save and restore game references
 * so that parsing game commands doesn't interfere with backend loops that
 * are working through game references set by prior commands.  Saving
 * references uses the buffer passed in if possible, otherwise allocates
 * its own buffer; testing the return value shows which happened.
 */
static scr_bool *
lib_save_object_references (scr_gameref_t game, scr_bool buffer[], scr_int length)
{
  scr_int required, available;
  scr_bool *references;

  /*
   * Calculate the required bytes for references, and then either allocate or
   * use the buffer supplied.
   */
  required = gs_object_count (game) * sizeof (*references);
  available = length * sizeof (buffer[0]);
  references = required > available ? (decltype(+buffer)) scr_malloc (required) : buffer;

  /* Copy over references from the game, and return the saved copy. */
  memcpy (references, game->object_references.data (), required);
  return references;
}

static void
lib_restore_object_references (scr_gameref_t game, const scr_bool references[])
{
  scr_int bytes;

  /* Calculate the bytes in the references array, and copy back to the game. */
  bytes = gs_object_count (game) * sizeof (references[0]);
  memcpy (game->object_references.data (), references, bytes);
}


/*
 * lib_try_game_command_common()
 * lib_try_game_command_short()
 * lib_try_game_command_with_object()
 * lib_try_game_command_with_npc()
 *
 * Try a game command with a standard verb.  Used by get and drop handlers
 * to retry game commands using standard "get " and "drop " commands.  This
 * makes "take/pick up/put down" work with a game's overridden get/drop.
 */
/*
 * lib_object_short_name_is_ambiguous()
 *
 * Return TRUE if any object other than the one passed shares its Short name
 * (case-insensitively).  Used to suppress the bare-name game-command retry
 * below: if "key" names several objects, a generic game task matched by the
 * bare noun ("get key") is a disambiguation/catch-all handler, not a specific
 * override for the addressed object, and must not block the standard take of a
 * fully-qualified reference ("get brass key").  The Runner never reconstructs a
 * bare noun like this, so it never lets such a task hijack the take; mirror that
 * by only attempting the bare-name retry when the name is unambiguous.
 */
static scr_bool
lib_object_short_name_is_ambiguous (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *name;
  scr_int other, object_count;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  if (!name || name[0] == NUL)
    return FALSE;

  object_count = gs_object_count (game);
  for (other = 0; other < object_count; other++)
    {
      const scr_char *other_name;

      if (other == object)
        continue;
      vt_key[1].integer = other;
      other_name = prop_get_string (bundle, "S<-sis", vt_key);
      if (other_name && scr_strcasecmp (name, other_name) == 0)
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_typed_verb()
 *
 * The retry below is built from a canonical library verb ("get", "drop"),
 * but the Runner matches tasks against the words the player actually typed.
 * "TenebraeSemper.taf" task 0 is "get * pen(s)": run400 runs it for
 * `get pens` ("You take a pen from the drawer.") and NOT for `take pens`,
 * which completes the library take of the pens object untouched (probes
 * Adrift_1_tenebrae_probe{,3}.txt, 2026-08-30) -- so the game's "have a pen"
 * gate on leaving the dorm really does require typing `get`.  Retrying with
 * a canonical "get" let Scarier's `take pens` fire that task and walk past
 * the gate.  The precedents the retry was tuned on all keep the typed verb
 * (Wax Worx `get marie` -> "get * head", Sommeril `take silver orb` ->
 * "take silver orb"), so hand the retry the verb the player used: the
 * library's own synonym of the canonical verb that opens the dispatched
 * command element, or the canonical verb when none does.
 */
static const scr_char *
lib_typed_verb (const scr_char *verb)
{
  static const scr_char *const GET_FORMS[] = {"get", "take", "pick up", "pick"};
  static const scr_char *const DROP_FORMS[] = {"drop", "put down"};
  const scr_char *const *forms;
  scr_int count, index_;
  const scr_char *input;

  if (strcmp (verb, "get") == 0)
    forms = GET_FORMS, count = 4;
  else if (strcmp (verb, "drop") == 0)
    forms = DROP_FORMS, count = 2;
  else
    return verb;

  input = run_get_dispatch_input ();
  if (!input)
    return verb;
  while (scr_isspace (*input))
    input++;

  for (index_ = 0; index_ < count; index_++)
    {
      const scr_int length = strlen (forms[index_]);
      if (scr_strncasecmp (input, forms[index_], length) == 0
          && (input[length] == NUL || scr_isspace (input[length])))
        return forms[index_];
    }
  return verb;
}

/*
 * lib_definite_prefix()
 *
 * The prefix as run400's name builder Proc_21_31_448710 composes it in its
 * normalizing mode 0: through tense (Proc_21_13_44F474), which rewrites a
 * whole "a", "an" or "some" to "the" and a leading "a ", "an " or "some " to
 * "the ", and leaves everything else alone.  An empty Prefix is already "a"
 * by the time 4.0 has loaded the game (loader @4900EC), so it composes as
 * "the" too.
 */
static const scr_char *
lib_definite_prefix (const scr_char *prefix, scr_char *buffer, size_t size)
{
  static const scr_char *const ARTICLES[] = { "a", "an", "some" };
  size_t index_;

  if (scr_strempty (prefix))
    return "the";
  for (index_ = 0; index_ < sizeof (ARTICLES) / sizeof (ARTICLES[0]); index_++)
    {
      const scr_char *const article = ARTICLES[index_];
      const size_t length = strlen (article);

      if (scr_strcasecmp (prefix, article) == 0)
        return "the";
      if (scr_strncasecmp (prefix, article, length) == 0
          && prefix[length] == ' ')
        {
          if (strlen (prefix) - length + 4 > size)
            return prefix;
          sprintf (buffer, "the%s", prefix + length);
          return buffer;
        }
    }
  return prefix;
}

/*
 * lib_task_prematches_input()
 *
 * run400's task pre-matcher Proc_19_35_453C50 on the typed line: does any
 * task in scope pattern-match it with its restrictions passing, or fail a
 * restriction that has a message to print?  (Not "restrictions ignored", as
 * this used to say; see run_does_command_match() for the two passes and the
 * House measurement.)  Object references are left exactly as they were.
 * class_filter is the pre-matcher's mode byte -- 1 for the take-family
 * look-ups, 2 for the put/drop family, 0 for none; see
 * run_set_task_class_filter().
 */
static scr_bool
lib_task_prematches_input (scr_gameref_t game, scr_int class_filter)
{
  scr_bool references_buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  scr_bool *references, status;
  const scr_char *input;

  input = run_get_dispatch_input ();
  if (!input)
    return FALSE;

  references = lib_save_object_references (game, references_buffer,
                                           LIB_ALLOCATION_AVOIDANCE_SIZE);
  run_set_task_class_filter (class_filter);
  status = run_does_command_match (game, input, TRUE);
  run_set_task_class_filter (0);
#ifdef SCARIER_DUMP_TOOLS
  if (getenv ("SCR_TRACE_MATCH"))
    fprintf (stderr, "PREMATCH mode=%ld input=[%s] %s\n", (long) class_filter,
             input, status ? "HIT" : "miss");
#endif
  lib_restore_object_references (game, references);
  if (references != references_buffer)
    scr_free (references);
  return status;
}

static scr_bool
lib_try_game_command_common (scr_gameref_t game,
                             const scr_char *verb, scr_int object,
                             const scr_char *preposition,
                             scr_int associate,
                             scr_bool is_associate_object,
                             scr_bool is_associate_npc,
                             scr_bool use_typed_verb,
                             scr_bool use_definite = FALSE)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (use_typed_verb)
    verb = lib_typed_verb (verb);
  scr_vartype_t vt_key[3];
  scr_char buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  scr_bool references_buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  const scr_char *prefix, *name;
  scr_char *command;
  scr_bool *references, status;
  assert (!is_associate_object || !is_associate_npc);

  /* Save the game's references, for restore later on. */
  references = lib_save_object_references (game, references_buffer,
                                           LIB_ALLOCATION_AVOIDANCE_SIZE);

  /* Get the addressed object's prefix and main name. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  /* Construct and try for game commands with a standard verb. */
  if (is_associate_object || is_associate_npc)
    {
      const scr_char *associate_prefix, *associate_name;
      scr_char definite_prefix[64], definite_associate_prefix[64];
      scr_int required;

      /* Get the associate's prefix and main name. */
      if (is_associate_object)
        {
          associate_prefix = prop_get_indexed_string (bundle, "Objects",
                                                      associate, "Prefix");
          associate_name = prop_get_indexed_string (bundle, "Objects",
                                                    associate, "Short");
        }
      else
        {
          assert (is_associate_npc);
          associate_prefix = prop_get_indexed_string (bundle, "NPCs",
                                                      associate, "Prefix");
          associate_name = prop_get_indexed_string (bundle, "NPCs", associate,
                                                    "Name");
        }
      /*
       * 4.0's put handler composes both names in the name builder's
       * normalizing mode 0 (run400 insides @465DED-465E51, two calls to
       * Proc_21_31_448710 with mode 0), so its canonical line reads "put the
       * bean in the jar", never "put a bean in a jar"; see
       * lib_definite_prefix().
       */
      if (use_definite && is_associate_object)
        {
          prefix = lib_definite_prefix (prefix, definite_prefix,
                                        sizeof (definite_prefix));
          associate_prefix = lib_definite_prefix (associate_prefix,
                                                  definite_associate_prefix,
                                                  sizeof (definite_associate_prefix));
        }

      assert (preposition);
      required = strlen (verb) + strlen (prefix) + strlen (name)
                 + strlen (preposition) + strlen (associate_prefix)
                 + strlen (associate_name) + 6;
      command = required > (scr_int) sizeof (buffer)
                ? (decltype(+buffer)) scr_malloc (required) : buffer;

      /*
       * Try the command with prefixes on both the target object and the
       * associate.  This used to also try the prefix-dropped combinations,
       * but the real Runner does not: probed live 2026-08-02 (FM7 + a
       * TheADRIFTProject .tas transplant in run400), "put pill in cup"
       * completes the library put untouched by a matched-but-failing
       * "put * pill in cup" task -- the prefix-less retry is exactly the
       * form that task would steal.  (The single-object retry below keeps
       * its prefixed form too, which is what lets Wax Worx's "get * head"
       * claim "get marie" via "get Marie Antoinette's head" -- the wildcard
       * absorbs the prefix there, matching the Runner.)
       */
      sprintf (command, "%s %s %s %s %s %s", verb,
               prefix, name, preposition, associate_prefix, associate_name);
      status = run_game_task_commands (game, command);
    }
  else
    {
      scr_char definite_prefix[64];
      scr_int required;

      /*
       * The implicit take of 4.0's put (run400 Proc_19_39_46302C @462AED)
       * pre-matches "get " & name(obj, mode 0) -- the definite form, one
       * spelling, no prefix-less retry.
       */
      if (use_definite)
        prefix = lib_definite_prefix (prefix, definite_prefix,
                                      sizeof (definite_prefix));
      required = strlen (verb) + strlen (prefix) + strlen (name) + 3;
      command = required > (scr_int) sizeof (buffer)
                ? (decltype(+buffer)) scr_malloc (required) : buffer;

      /* Try the command with and without prefixes on the addressed object.
       * The prefix-less retry can re-hit a task that already matched and
       * failed its restrictions this turn; that is what the Runner shows
       * too (cobl: "take medicine" after "look in rubbish" prints the
       * task's fail text, not the library take -- run400 probe
       * 2026-08-30).
       */
      sprintf (command, "%s %s %s", verb, prefix, name);
      status = run_game_task_commands (game, command);
      if (!status && !use_definite
          && !lib_object_short_name_is_ambiguous (game, object))
        {
          sprintf (command, "%s %s", verb, name);
          status = run_game_task_commands (game, command);
        }
    }

  /* Restore the game object references back to their state on entry. */
  lib_restore_object_references (game, references);

  /* Free any allocations, and return the game command status. */
  if (command != buffer)
    scr_free (command);
  if (references != references_buffer)
    scr_free (references);
  return status;
}

static scr_bool
lib_try_game_command_short (scr_gameref_t game,
                            const scr_char *verb, scr_int object)
{
  return lib_try_game_command_common (game, verb, object,
                                      NULL, -1, FALSE, FALSE, TRUE);
}

/*
 * The refusal-exit pre-match of run400's per-piece take (Proc_19_23_473A34
 * @473241) is built from the RESOLVED object and the canonical "get", not
 * from the typed words: `take poster` on man overboard.taf runs "Get *
 * poster", a task with no take form at all (Adrift_1_man_overboard.txt:39,
 * Adrift_1_moprobe.txt, 2026-08-29/30).  Only the pre-action retry above is
 * verb-literal.
 */
static scr_bool
lib_try_game_command_short_canonical (scr_gameref_t game,
                                      const scr_char *verb, scr_int object)
{
  return lib_try_game_command_common (game, verb, object,
                                      NULL, -1, FALSE, FALSE, FALSE);
}

/*
 * lib_try_game_command_short_definite()
 *
 * The 4.0 drop handler's per-object task look-up, and the drop half of the
 * same rule lib_try_game_command_with_object_400() documents for put: the
 * line offered to the tasks is rebuilt from the RESOLVED object in the
 * normalizing mode 0 -- "drop " & name(obj, 0), so "drop the board" -- and
 * that one spelling is all the tasks ever see.  No authored-prefix form, no
 * prefix-less retry (run400 @46F33B-46F358, class-filter mode 2).
 *
 * The two measurements it reconciles are the same shape as put's.  dusk.taf
 * task 48 `drop * board`, the board in hand, claims `drop board` in run400
 * (Adrift_221_dusk.txt:80) -- the wildcard absorbs the article the rebuild
 * puts in.  p4REPEAT3.taf task 3, whose command is the literal `drop hat`,
 * does not: run400 answers both `drop hat` turns out of the library, "You
 * drop the hat." and then "You are not holding the hat.", and neither the
 * CompleteText nor the RepeatText is ever printed (Adrift_952.txt,
 * 2026-09-08).  "drop the hat" simply is not `drop hat`.
 *
 * Pre-4.0 keeps the authored-prefix form and its bare-name retry, where the
 * typed line has already been past the tasks before the library sees it.
 */
static scr_bool
lib_try_game_command_short_definite (scr_gameref_t game,
                                     const scr_char *verb, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_bool references_buffer[LIB_ALLOCATION_AVOIDANCE_SIZE];
  scr_vartype_t vt_key[4];
  scr_bool *references, status;
  scr_int alias_count, alias;

  assert (lib_is_version_400 (game));

  run_set_task_class_filter (2);
  status = lib_try_game_command_common (game, verb, object,
                                        NULL, -1, FALSE, FALSE, FALSE, TRUE);

  /*
   * Any of the object's names can fill the noun slot, not just its Short.
   * frustrated.taf's `drop tree` names the upper half of the trunk by an
   * alias, and run400 gives the line to `*drop*tree*`
   * (Adrift_274_frustrated.txt) -- "drop the upper half of the trunk" is not
   * what that pattern matches, "drop the tree" is.
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Alias";
  alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (alias = 0; alias < alias_count && !status; alias++)
    {
      scr_char buffer[LIB_ALLOCATION_AVOIDANCE_SIZE], definite_prefix[64];
      const scr_char *name, *prefix;

      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name))
        continue;

      prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
      prefix = lib_definite_prefix (prefix, definite_prefix,
                                    sizeof (definite_prefix));
      if (strlen (verb) + strlen (prefix) + strlen (name) + 3
          > sizeof (buffer))
        continue;

      sprintf (buffer, "%s %s %s", verb, prefix, name);
      references = lib_save_object_references (game, references_buffer,
                                               LIB_ALLOCATION_AVOIDANCE_SIZE);
      status = run_game_task_commands (game, buffer);
      lib_restore_object_references (game, references);
      if (references != references_buffer)
        scr_free (references);
    }
  run_set_task_class_filter (0);
  return status;
}

/*
 * lib_try_game_command_take_definite()
 *
 * The task look-up inside 4.0's implicit take (run400 Proc_19_39_46302C,
 * @462AED-462C85): "get " & name(obj, 0), or "get " & name(obj, 0) & " from "
 * & name(holder, 0) when the object sits inside a container, both names in
 * the normalizing mode 0 ("get the wood", "get the coin from the box").  A
 * hit runs that line through the task dispatcher and claims; the library
 * take only follows a miss.
 */
static scr_bool
lib_try_game_command_take_definite (scr_gameref_t game, scr_int object)
{
  scr_bool status;

  /* The take piece's look-ups run in the pre-matcher's mode 1 (@462B12,
   * @462B84): only tasks carrying the take flag can answer. */
  run_set_task_class_filter (1);
  if (gs_object_position (game, object) == OBJ_IN_OBJECT)
    status = lib_try_game_command_common (game, "get", object,
                                          "from",
                                          gs_object_parent (game, object),
                                          TRUE, FALSE, FALSE, TRUE);
  else
    status = lib_try_game_command_common (game, "get", object,
                                          NULL, -1, FALSE, FALSE, FALSE, TRUE);
  run_set_task_class_filter (0);
  return status;
}

static scr_bool
lib_try_game_command_with_object (scr_gameref_t game,
                                  const scr_char *verb, scr_int object,
                                  const scr_char *preposition,
                                  scr_int other_object)
{
  return lib_try_game_command_common (game, verb, object,
                                      preposition, other_object, TRUE, FALSE,
                                      TRUE);
}

/*
 * lib_try_game_command_with_object_400()
 *
 * The task look-up of 4.0's put-in / put-on handler (run400 insides
 * Proc_19_43_46639C, body 465CA4-46639A).  With a target in hand it rebuilds
 * the line in canonical form -- "put " & name(obj) & " " & Left(prep, 2) &
 * " " & name(target), both names in the normalizing mode 0, so "put the bean
 * in the jar" -- and pre-matches THAT with Proc_19_35_453C50 (@465CBB-465D39,
 * restrictions ignored); the typed line is only consulted when there is no
 * target (@465DC8, the drop and put-down forms).  On a hit the same rebuild
 * goes to the task dispatcher Proc_19_24_44CCE0 at @465EB5, restriction-
 * failure pass included, and a claim exits the handler with 2 before the
 * possession, size and capacity tests.  No claim, and the handler carries
 * on to its own refusal or move.  Here the pre-match and the dispatch are the
 * one run_game_task_commands() call on the definite line.
 *
 * The definite form is what reconciles the measurements: `put * firewood in
 * * fireplace` wins `put firewood in fireplace` in run400
 * (Adrift_1_goldilocks.txt 383, prefixes "a pile of" / "the"), as does
 * `put * battery * flashlight` (Adrift_1_Tear.txt), while the PUT5 arena
 * probe's `put a bean in a jar`, `put * pill in cup` and `put coin in box`
 * tasks (Adrift_82.txt, 2026-09-05) and sommeril's `put fish in fountain`
 * (Adrift_78.txt, both prefixes empty) all lose to the library: none of
 * those patterns matches "put the bean in the jar" / "put the FISH in the
 * FOUNTAIN".  The typed spelling never reaches the tasks through this
 * handler at all -- The ADRIFT Project's `put battery in charger` comes out
 * of the synonym table as "put nickel-cadmium nickel-cadmium accumulator in
 * charger", which no pattern of its `#Charge battery` task matches, and the
 * author's own run400 transcript still shows the task firing: it is the
 * rebuild "put the small battery in the battery charger" that `put * battery
 * * charger *` claims.  Pre-4.0 keeps the authored-prefix retry of
 * lib_try_game_command_with_object(), and there the typed line has already
 * been through the tasks before the library sees it.
 */
static scr_bool
lib_try_game_command_with_object_400 (scr_gameref_t game,
                                      const scr_char *verb, scr_int object,
                                      const scr_char *preposition,
                                      scr_int other_object)
{
  scr_bool status;

  assert (lib_is_version_400 (game));

  /* The insides handler pre-matches its rebuilt line in mode 2 (@465D39):
   * only tasks carrying the put/drop flag are consulted. */
  run_set_task_class_filter (2);
  status = lib_try_game_command_common (game, verb, object,
                                        preposition, other_object, TRUE, FALSE,
                                        FALSE, TRUE);
  run_set_task_class_filter (0);
  return status;
}

static scr_bool
lib_try_game_command_with_npc (scr_gameref_t game,
                               const scr_char *verb, scr_int object,
                               const scr_char *preposition, scr_int npc)
{
  return lib_try_game_command_common (game, verb, object,
                                      preposition, npc, FALSE, TRUE, TRUE);
}


/*
 * lib_parse_next_object()
 *
 * Helper for lib_parse_multiple_objects().  Extracts the next object, if any,
 * from referenced text, and returns it.  Disambiguates any ambiguous objects
 * using the verb supplied, and sets are_more_objects if we found an object
 * but there appear to be more following it.
 */
static scr_bool
lib_parse_next_object (scr_gameref_t game, const scr_char *verb,
                       scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int),
                       scr_int resolver_arg,
                       scr_int *object,
                       scr_bool *are_more_objects, scr_bool *is_ambiguous)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *list;
  scr_bool is_matched;

  /*
   * Look for "object" or "object and ...", and set match and more flags.
   *
   * Fall back to "object <trailing text>" -- a single object followed by
   * anything that isn't "and" -- if neither matches.  run400 tolerates
   * filler after a single object reference (probe DONE 2026-08-23: Space
   * Boy's First Adventure Task 72's own command is the literal, unrestricted,
   * textless "drop cape to the floor"; typing it live gets the library's
   * ordinary "Player drop the cape." with the score unchanged, Adrift_8_pET2.txt/
   * Adrift_10_pET4.txt), where scarier's exact-match-only %object% previously
   * failed to parse "cape to the floor" at all, so the library's drop never
   * ran and the command fell through to "Drop what?" instead.  This fallback
   * is tried last so it never preempts a real "X and Y" list.
   */
  list = var_get_ref_text (vars);
  if (uip_match ("%object%", list, game))
    {
      *are_more_objects = FALSE;
      is_matched = TRUE;
    }
  else if (uip_match ("%object% and %text%", list, game))
    {
      *are_more_objects = TRUE;
      is_matched = TRUE;
    }
  else if (uip_match ("%object% %text%", list, game))
    {
      *are_more_objects = FALSE;
      is_matched = TRUE;
    }
  else
    is_matched = FALSE;

  /* If we extracted an object from referenced text, disambiguate. */
  if (is_matched)
    *object = lib_disambiguate_object_common (game, verb,
                                              resolver, resolver_arg,
                                              is_ambiguous);
  else
    *is_ambiguous = FALSE;

  /* Return TRUE if we matched anything. */
  return is_matched;
}


/*
 * lib_parse_multiple_objects()
 *
 * Parser for commands that take multiple object targets from a %text% match.
 * Parses object lists such as "object" and "object and object" and returns
 * the multiple objects in the game's multiple_references.
 */
static scr_bool
lib_parse_multiple_objects (scr_gameref_t game, const scr_char *verb,
                            scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int),
                            scr_int resolver_arg,
                            scr_int *count)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int count_, object;
  scr_bool are_more_objects, is_ambiguous;

  /* Initialize variables to avoid gcc warnings. */
  object = -1;
  are_more_objects = FALSE;

  /* Clear all current multiple object references, and the count. */
  gs_clear_multiple_references (game);
  count_ = 0;

  /*
   * Parse the first object from the list.  If we get nothing here, return
   * FALSE if it didn't look like a multiple object list, TRUE if ambiguous.
   * Beyond here, we always return TRUE, since after this point _something_
   * looked believable...
   */
  if (!lib_parse_next_object (game, verb,
                              resolver, resolver_arg,
                              &object, &are_more_objects, &is_ambiguous))
    return FALSE;
  else if (object == -1)
    {
      if (is_ambiguous)
        {
          /*
           * Return TRUE, with zero count, to cause caller to return.  We get
           * here if the first parsed object was ambiguous.  In this case,
           * the disambiguation has printed a message, so we want our caller
           * to simply return TRUE to indicate that the command was handled,
           * albeit not fully successfully.
           */
          *count = count_;
          return TRUE;
        }
      else
        {
          /*
           * No object matched after disambiguation, so return FALSE to have
           * our caller ignore the command.
           */
          return FALSE;
        }
    }

  /* Mark this first object as referenced in the return array. */
  game->multiple_references[object] = TRUE;
  count_++;

  /* Now parse each additional object from the list. */
  while (are_more_objects)
    {
      scr_int last_object;

      /*
       * If no next object, leave the loop.  If no disambiguation message
       * then it was probably garble, so print a message for that case.  We
       * also catch repeated objects here.
       */
      last_object = object;
      if (!lib_parse_next_object (game, verb,
                                  resolver, resolver_arg,
                                  &object, &are_more_objects, &is_ambiguous)
          || object == -1
          || game->multiple_references[object])
        {
          if (!is_ambiguous)
            {
              pf_buffer_string (filter,
                                "I only understood you as far as wanting to ");
              pf_buffer_string (filter, verb);
              pf_buffer_character (filter, ' ');
              lib_print_object_np (game, last_object);
              pf_buffer_string (filter, ".\n");
            }

          /* Zero count to indicate an error somewhere in the list. */
          count_ = 0;
          break;
        }

      /* Mark the object as referenced in the return array. */
      game->multiple_references[object] = TRUE;
      count_++;
    }

  /* We found at least enough of an object list to say we matched. */
  *count = count_;
  return TRUE;
}


/*
 * lib_print_nothing_held()
 *
 * The complaint the "<verb> all [except ...]" commands make when the filter
 * left them with nothing to work on: "You are not holding anything[ else]",
 * or the wearing form, rounded off by `tail`.
 */
static void
lib_print_nothing_held (scr_gameref_t game, scr_bool worn,
                        scr_bool add_else, const scr_char *tail)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (worn)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are not wearing anything",
                                           "I am not wearing anything",
                                           "%player% is not wearing anything"));
  else
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are not holding anything",
                                           "I am not holding anything",
                                           "%player% is not holding anything"));
  if (add_else)
    pf_buffer_string (filter, " else");
  pf_buffer_string (filter, tail);
}


/*
 * lib_multiple_retains_associate()
 *
 * The "put the box in the box" case: the object list named the very object
 * the command acts through.  Complains and returns TRUE where it did.
 */
static scr_bool
lib_multiple_retains_associate (scr_gameref_t game, scr_int associate,
                                const scr_char *verb)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!game->multiple_references[associate])
    return FALSE;

  pf_buffer_string (filter, "I only understood you as far as wanting to ");
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, associate);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}


/*
 * lib_apply_filter()
 *
 * Apply filters for multiple object frontends.  Transfer multiple object
 * references into standard object references, using the supplied filter.
 * `is_except` inverts the sense of the parsed list: its objects are the ones
 * to leave behind, and everything else the filter admits is referenced.
 */
static scr_int
lib_apply_filter (scr_gameref_t game,
                  scr_bool (*filter) (scr_gameref_t, scr_int, scr_int),
                  scr_int filter_arg, scr_bool is_except, scr_int *references)
{
  scr_int count, object, references_;

  /* Clear all object references initially. */
  gs_clear_object_references (game);

  /*
   * Find objects included by the filter, and transfer the reference of each
   * from the multiple references into standard references.
   */
  count = 0;
  references_ = references ? *references : 0;
  for (object = 0; object < gs_object_count (game); object++)
    {
      scr_bool is_listed;

      if (!filter (game, object, filter_arg))
        continue;

      /* Consume the list entry, if this object was one. */
      is_listed = game->multiple_references[object];
      if (is_listed)
        {
          game->multiple_references[object] = FALSE;
          references_--;
        }

      /* Reference the listed objects, or -- for "all except" -- the rest. */
      if (is_except ? !is_listed : is_listed)
        {
          game->object_references[object] = TRUE;
          count++;
        }
    }

  /* Copy back the updated reference count, return count. */
  if (references)
    *references = references_;
  return count;
}


/*
 * lib_carried_burden()
 *
 * Return the total burden of everything the player is currently holding, under
 * the version 3.8 pooled model (see obj_get_burden()).
 *
 * Worn objects are free.  Wearing something does not spend any of MaxCarried,
 * and neither does keeping it on -- measured with a MaxCarried 2 probe holding
 * a wearable and two others: run370 and run380 both accept all three, and
 * "count" reports the two held (pworn/qworn, 2026-08-23).  Taking anything off
 * therefore costs a slot again.
 *
 * Always computed afresh, and that is faithful: unlike 4.0's running size and
 * weight totals, the pooled burden does not drift.  A container is not charged
 * for its contents and wearing is not a charge at all, so neither of the two
 * shapes of 4.0 leak has anything to leak here -- confirmed against both
 * Runners with the drop-a-loaded-container and wear-then-drop sequences
 * (pleakc/pleakw and their 3.70 twins).  This is why "glk capacity" is a no-op
 * for a 3.7/3.8 game: there is no Runner arithmetic to escape.  It is a no-op
 * for a 3.9 game too, for the same reason by a different route -- run390 keeps
 * a running total but keeps it exactly; see obj_uses_running_load().
 */
static scr_int
lib_carried_burden (scr_gameref_t game)
{
  scr_int index_, burden = 0;

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER)
        burden += obj_get_burden (game, index_);
    }

  return burden;
}


/*
 * lib_carried_size()
 * lib_carried_weight()
 *
 * The player's current carried size and weight as the capacity checks see
 * them.  For a 4.0 game this is by default the Runner's running total
 * (gs_carried_*, with its take/drop double-count); otherwise, and in legacy
 * mode, it is recomputed afresh from what is currently held or worn.
 *
 * Only 4.0 keeps a running total worth reproducing: run390 adjusts the same
 * two globals but does so exactly, so recomputing is what a 3.9 game looks
 * like from the outside -- see obj_uses_running_load() for the probe that
 * separates the two.  Legacy mode also drops the Runner's stale parent rule
 * from obj_weigh(), so it is free of the phantom weight too; a 3.9 game gets
 * that for the same reason, having no running total to poison.
 */
static scr_int
lib_carried_size (scr_gameref_t game)
{
  scr_int index_, size;

  if (obj_uses_running_load (game) && !game->capacity_recompute)
    return gs_carried_size (game);

  size = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER
          || gs_object_position (game, index_) == OBJ_WORN_PLAYER)
        size += obj_get_size (game, index_);
    }

  return size;
}

static scr_int
lib_carried_weight (scr_gameref_t game)
{
  scr_int index_, weight;

  if (obj_uses_running_load (game) && !game->capacity_recompute)
    return gs_carried_weight (game);

  weight = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (gs_object_position (game, index_) == OBJ_HELD_PLAYER
          || gs_object_position (game, index_) == OBJ_WORN_PLAYER)
        weight += obj_get_weight (game, index_);
    }

  return weight;
}


/*
 * lib_cmd_count()
 *
 * Display player weight and size limits and amounts currently carried.
 */
scr_bool
lib_cmd_count (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int size, weight;

  /*
   * A version 3.8 game has neither of these axes, and its "count" is a single
   * unlabelled line counting objects rather than the two tab-stopped Size and
   * Weight lines below: "You have 0 objects.  The most you can hold is 2."
   * There is no singular -- one object still reads "1 objects" -- and the
   * first person is "I have ... The most I can hold is ...".  Measured on
   * run380 with the pcount/pcount1 probes, 2026-08-23.
   */
  if (obj_uses_burden_model (game))
    {
      pf_buffer_string (filter,
                        lib_select_response (game, "You have ", "I have ",
                                             "%player% have "));
      pf_buffer_integer (filter, lib_carried_burden (game));
      pf_buffer_string (filter, " objects");
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             ".  The most you can hold is ",
                                             ".  The most I can hold is ",
                                             ".  The most %player_pronoun% can hold is "));
      pf_buffer_integer (filter, obj_get_player_burden_limit (game));
      pf_buffer_string (filter, ".\n");

      lib_set_admin (game);
      return TRUE;
    }

  /* Report the same carried totals the capacity checks use. */
  size = lib_carried_size (game);
  weight = lib_carried_weight (game);

  /*
   * Print the player limits and amounts used, in the Runner's own layout --
   * a tab stop after each label (two tabs for the shorter "Size:") and the
   * game's perspective for the pronouns: "Size:\t\tI have 0.  The most I
   * can hold is 108." (run400, goldilocks/iachini transcripts 2026-08-22).
   */
  pf_buffer_string (filter, "Size:\t\t");
  pf_buffer_string (filter,
                    lib_select_response (game, "You have ", "I have ",
                                         "%player% have "));
  pf_buffer_integer (filter, size);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         ".  The most you can hold is ",
                                         ".  The most I can hold is ",
                                         ".  The most %player_pronoun% can hold is "));
  pf_buffer_integer (filter, obj_get_player_size_limit (game));
  pf_buffer_string (filter, ".\n");

  pf_buffer_string (filter, "Weight:\t");
  pf_buffer_string (filter,
                    lib_select_response (game, "You have ", "I have ",
                                         "%player% have "));
  pf_buffer_integer (filter, weight);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         ".  The most you can hold is ",
                                         ".  The most I can hold is ",
                                         ".  The most %player_pronoun% can hold is "));
  pf_buffer_integer (filter, obj_get_player_weight_limit (game));
  pf_buffer_string (filter, ".\n");

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_object_too_heavy()
 *
 * Return TRUE if the given object is too heavy for the player to carry.
 */
static scr_bool
lib_object_too_heavy (scr_gameref_t game, scr_int object)
{
  scr_int player_limit, weight, object_weight;

  /*
   * Version 3.8 has no weight axis, and no "too heavy" refusal to go with one
   * -- its single pooled burden is checked in lib_object_too_large() below,
   * and answers "Your hands are full." to everything it refuses.
   */
  if (obj_uses_burden_model (game))
    return FALSE;

  /* Get the player limit and the given object weight. */
  player_limit = obj_get_player_weight_limit (game);
  object_weight = obj_get_weight (game, object);

  /* Establish the player's current carried weight. */
  weight = lib_carried_weight (game);

  /* Return TRUE if the new object exceeds limit. */
  return weight + object_weight > player_limit;
}


/*
 * lib_print_too_heavy()
 *
 * Print the weight refusal for one object, into a clause the caller has
 * already opened.
 *
 * The whole sentence is a 4.0 rewording.  run390 answers a fixed "That is too
 * heavy for you to carry." -- it names no object, and it never qualifies the
 * refusal.  run400 names the object and always qualifies it: "The lump is too
 * heavy for Player to carry at the moment."  Both were measured on the p39wt
 * probe (2026-08-23), one Runner each, against a brick that fits in empty
 * hands and a lump that does not: each Runner gave its single shape for both,
 * so the "at the moment" is not the portability hedge SCARE took it for.
 * Neither Runner has a plural form -- there is no "are too heavy" string in
 * either binary.
 *
 * A 3.7 or 3.8 game never reaches here: the pooled burden has no weight axis
 * and lib_object_too_heavy() stands down for it.
 */
static void
lib_print_too_heavy (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_400)
    {
      pf_buffer_string (filter,
                        lib_select_response
                          (game,
                           "That is too heavy for you to carry.",
                           "That is too heavy for me to carry.",
                           "That is too heavy for %player% to carry."));
      return;
    }

  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_response
                      (game,
                       " is too heavy for you to carry at the moment.",
                       " is too heavy for me to carry at the moment.",
                       " is too heavy for %player% to carry at the moment."));
}


/*
 * lib_object_too_large()
 *
 * Return TRUE if the given object is too large for the player to carry.
 */
static scr_bool
lib_object_too_large (scr_gameref_t game, scr_int object)
{
  scr_int player_limit, size, object_size;

  /*
   * Version 3.8's pooled burden against its plain MaxCarried limit, which
   * replaces both 4.0 axes outright: every class costs at least 1, so the
   * burden is never looser than the object count the size axis would have
   * enforced against normalised 3.8 objects.
   */
  if (obj_uses_burden_model (game))
    {
      scr_int object_burden = obj_get_burden (game, object);

      player_limit = obj_get_player_burden_limit (game);

      return lib_carried_burden (game) + object_burden > player_limit;
    }

  /* Get the player limit and the given object size. */
  player_limit = obj_get_player_size_limit (game);
  object_size = obj_get_size (game, object);

  /* Establish the player's current carried size. */
  size = lib_carried_size (game);

  /* Return TRUE if the new object exceeds limit. */
  return size + object_size > player_limit;
}


/*
 * lib_cmd_take_npc()
 *
 * Reject attempts to take an npc.
 */
scr_bool
lib_cmd_take_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "take", &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  /*
   * Reject this attempt.  The Runner names the NPC by its Prefix and first
   * Alias here, not its Name: run400 47F750-47F7BC and run390 45969B-4596C6
   * print "I don't think " & [Prefix & " "] & Alias(0) when the alias is
   * set (the prefix only when it is too), and fall back to the Name when it
   * is empty; run380 44057D and run370 4386EE always print Prefix & " " &
   * Alias(0), empty or not.  House (4.00), Cathy alias "girl", no prefix:
   * "I don't think girl would appreciate being handled." (Adrift_95,
   * 2026-09-06).
   */
  {
    const scr_filterref_t filter = gs_get_filter (game);
    const scr_prop_setref_t bundle = gs_get_bundle (game);
    const scr_bool is_390_plus =
        prop_get_taf_version (bundle) >= TAF_VERSION_390;
    const scr_char *prefix, *alias = NULL;
    scr_vartype_t vt_key[4];

    prefix = prop_get_indexed_string (bundle, "NPCs", npc, "Prefix");
    vt_key[0].string = "NPCs";
    vt_key[1].integer = npc;
    vt_key[2].string = "Alias";
    if (prop_get_child_count (bundle, "I<-sis", vt_key) > 0)
      {
        vt_key[3].integer = 0;
        alias = prop_get_string (bundle, "S<-sisi", vt_key);
      }
    if (!prefix)
      prefix = "";
    if (!alias)
      alias = "";

    pf_buffer_string (filter, "I don't think ");
    if (!is_390_plus)
      {
        pf_buffer_string (filter, prefix);
        pf_buffer_string (filter, " ");
        pf_buffer_string (filter, alias);
      }
    else if (alias[0] != NUL)
      {
        if (prefix[0] != NUL)
          {
            pf_buffer_string (filter, prefix);
            pf_buffer_string (filter, " ");
          }
        pf_buffer_string (filter, alias);
      }
    else
      lib_print_npc_np (game, npc);
    pf_buffer_string (filter, " would appreciate being handled.\n");
  }
  return TRUE;
}


/*
 * lib_take_backend_common()
 *
 * Common backend handler for taking objects.  Takes all objects currently
 * referenced in the game, trying game commands first, and then moving other
 * unhandled objects to the player inventory.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */

/* Set when the take command named exactly one object; cleared by the
   backend.  Selects the single-take "already carrying" refusal wording. */
static scr_bool lib_take_single_named = FALSE;
static scr_bool lib_take_refusal_claimed = FALSE;

/* Set when a "take from <object>" command named exactly one object; cleared
   by the backend.  Only that single-named form echoes the taken object's
   raw prefix in pre-4.0 games (see the wording comment in the backend). */
static scr_bool lib_take_from_single_named = FALSE;

/*
 * lib_take_container_unheld()
 * lib_print_not_holding()
 *
 * The pre-3.9 hold gate on the container or surface an object is taken
 * from.  run380's takes() (loc_43E47B) rewrites a plain "take X" whose X sits
 * in or on a present object into "take X from <parent>", and its insides()
 * from-branch (loc_446CBB..446CFB) then insists that a DYNAMIC parent is held
 * (position 0) or worn (&H9C) by the player, else "<You> <are> not holding
 * <raw prefix> <short>." -- before the closed test, before any game task is
 * tried for "get X from Y".  A static parent only has to be in the room.
 * run370's insides() (43B118, "not holding" at 439D33/43AB5E) is the same;
 * run390 has no "not holding" on any take path and run400 none at all, so
 * from 3.90 on a contained object is taken wherever its parent is.
 *
 * Measured on jb2000.taf (James Bond - Happy Landings, 3.80) in run380,
 * 2026-09-04: `take key card` with the card inside a jacket lying in the
 * room answers "You are not holding a brown jacket.", and `take learner`
 * from a suitcase on the floor "You are not holding a brown suitcase.";
 * `take jacket` first, and the card comes out with "You take the cockpit
 * key card from the brown jacket."  The prefix is the author's, untensed
 * (var_41C(0) & " " & var_41C(4)), hence lib_print_object.  Held means
 * held directly: a container nested inside a carried one has position
 * &HF6, not 0, and is refused like any other.
 */
static scr_bool
lib_take_container_unheld (scr_gameref_t game, scr_int container)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;
  if (obj_is_static (game, container))
    return FALSE;

  return !(gs_object_position (game, container) == OBJ_HELD_PLAYER
           || gs_object_position (game, container) == OBJ_WORN_PLAYER);
}

static void
lib_print_not_holding (scr_gameref_t game, scr_int container,
                       const scr_char *suffix)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You are not holding ",
                                         "I am not holding ",
                                         "%player% is not holding "));
  lib_print_object (game, container);
  pf_buffer_string (filter, suffix);
}

/*
 * lib_cannot_reach_container()
 *
 * Pre-4.0 refuses to reach into a container or onto a surface while the
 * player is sitting, standing or lying on some OTHER object.
 *
 * run390's insides() (463EEC) tests the player record's +16 byte -- the
 * object the player is on or in, &HFF when free -- against the container
 * being reached into, and prints the refusal unless it is one or the other
 * (loc_4631D0..4631E1):
 *
 *     If player.on = &HFF Or player.on = container Then ok
 *     Else <You> & " can't reach " & the(container) & " from " &
 *          the(player.on) & "!"
 *
 * Both names come out of the definite-name helper, so it is "the " & Short
 * with the author's capitalisation kept: "You can't reach the dresser from
 * the Bed!"
 *
 * Measured 2026-09-05 on A_Morning_with_a_Headache.taf (3.90) in run390: the
 * game starts the player in bed, and `take alarm clock` -- the clock sits on
 * the dresser -- is refused exactly that way, as is `take suit` off the
 * chair.  The literal " can't reach " is in run370, run380 and run390's
 * constant pools and is ABSENT from run400's, so 4.0 dropped the rule
 * outright; the gate is < 4.00 rather than the usual 3.9 split.
 *
 * Two things here are inferred rather than measured, and are noted in
 * notes/WINE-TRANSCRIPTS-TODO.md: whether a game task for the same command
 * would run first (nothing in that game claims `take alarm clock`), and how
 * this orders against 3.7/3.8's own "not holding" gate, which is why it sits
 * after lib_take_container_unheld() rather than before it.
 */
static scr_bool
lib_cannot_reach_container (scr_gameref_t game, scr_int container)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;

  return gs_playerparent (game) != -1 && gs_playerparent (game) != container;
}

static void
lib_print_cannot_reach (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You can't reach ",
                                         "I can't reach ",
                                         "%player% can't reach "));
  lib_print_object_np (game, container);
  pf_buffer_string (filter, " from ");
  lib_print_object_np (game, gs_playerparent (game));
  pf_buffer_string (filter, "!");
}

static void
lib_take_backend_common (scr_gameref_t game, scr_int associate,
                         scr_bool is_associate_object, scr_bool is_associate_npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object_count, object, total, npc;
  scr_bool has_printed;
  lib_list_t list;
  /*
   * Objects that would take the player over capacity, in the order they
   * were encountered.  The Runner's multi-take loop appends a separate
   * "<Name> is too heavy for me to carry at the moment."/" hands are full."
   * sentence for every such object (run400 @0047C127/@0047C0F0), not just
   * the first.
   */
  lib_list_t over_capacity, over_is_size;
  assert (!is_associate_object || !is_associate_npc);

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.  At the same time, filter out and
   * flag any object that takes us over the player's capacity.
   */
  has_printed = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      scr_bool status;

      if (!game->object_references[object])
        continue;

      /*
       * Pre-3.9 refuses the whole take when the object's parent is a
       * dynamic container or surface the player isn't holding; see
       * lib_take_container_unheld().  With an associate the parent was
       * already vetted by lib_take_from_is_valid().
       */
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && lib_take_container_unheld (game,
                                        gs_object_parent (game, object)))
        {
          scr_int index_;

          lib_print_not_holding (game, gs_object_parent (game, object), ".");
          for (index_ = 0; index_ < object_count; index_++)
            game->object_references[index_] = FALSE;
          return;
        }

      /*
       * And pre-4.0 refuses it outright while the player is on or in some
       * other object; see lib_cannot_reach_container().
       */
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && lib_cannot_reach_container (game,
                                         gs_object_parent (game, object)))
        {
          scr_int index_;

          lib_print_cannot_reach (game, gs_object_parent (game, object));
          for (index_ = 0; index_ < object_count; index_++)
            game->object_references[index_] = FALSE;
          return;
        }

      /*
       * If the object is inside or on something already held by the player,
       * capacity checks are meaningless.
       */
      if (!((gs_object_position (game, object) == OBJ_IN_OBJECT
            || gs_object_position (game, object) == OBJ_ON_OBJECT)
            && obj_indirectly_held_by_player (game,
                                              gs_object_parent (game, object))))
        {
          /*
           * See if the object takes us beyond capacity.  If it does and it's
           * the first of its kind, note it and continue.
           */
          if (lib_object_too_heavy (game, object))
            {
              over_capacity.push_back (object);
              over_is_size.push_back (FALSE);
              game->object_references[object] = FALSE;
              continue;
            }
          if (lib_object_too_large (game, object))
            {
              over_capacity.push_back (object);
              over_is_size.push_back (TRUE);
              game->object_references[object] = FALSE;
              continue;
            }
        }

      /* Now try for a game command, using the associate if supplied. */
      if (is_associate_object)
        status = lib_try_game_command_with_object (game, "get",
                                                   object, "from", associate);
      else if (is_associate_npc)
        status = lib_try_game_command_with_npc (game, "get",
                                                object, "from", associate);
      else
        status = lib_try_game_command_short (game, "get", object);
      if (status)
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }

  /*
   * We attempt acquisition of get-able objects here only for cases where
   * there is either no associate, or where the associate is an object.  If
   * the associate is an NPC, we're going to refuse all acquisitions later
   * on, by forcing object references.
   */
  total = 0;
  if (!is_associate_npc)
    {
      scr_int parent, start, limit;

      /*
       * Attempt to acquire each remaining get-able object in turn, looping
       * on each possible parent object in turn, with an initial parent of
       * -1 for objects not contained or supported.
       *
       * If we're dealing with only objects from a known container or
       * supporter, eliminate all but one iteration of the parent search.
       */
      start = is_associate_object ? associate : -1;
      limit = is_associate_object ? associate : object_count - 1;

      for (parent = start; parent <= limit; parent++)
        {
          list.clear ();
          for (object = 0; object < object_count; object++)
            {
              if (!game->object_references[object])
                continue;

              /*
               * If parent is -1, ignore contained objects, otherwise ignore
               * objects not contained, or if contained, not contained by the
               * current parent.
               */
              if (parent == -1)
                {
                  if (gs_object_position (game, object) == OBJ_IN_OBJECT
                      || gs_object_position (game, object) == OBJ_ON_OBJECT)
                    continue;
                }
              else
                {
                  if (!(gs_object_position (game, object) == OBJ_IN_OBJECT
                        || gs_object_position (game, object) == OBJ_ON_OBJECT))
                    continue;
                  if (gs_object_parent (game, object) != parent)
                    continue;
                }

              /*
               * Here we have to repeat capacity checks.  As objects are
               * acquired more and more of the player's capacity gets used up.
               * This means a check directly before each acquisition.
               */
               if (parent == -1
                   || !obj_indirectly_held_by_player (game, parent))
                {
                  if (lib_object_too_heavy (game, object))
                    {
                      over_capacity.push_back (object);
                      over_is_size.push_back (FALSE);
                      continue;
                    }
                  if (lib_object_too_large (game, object))
                    {
                      over_capacity.push_back (object);
                      over_is_size.push_back (TRUE);
                      continue;
                    }
                }

              list.push_back (object);
              gs_object_player_get (game, object);
              /* A successful library take spends OnlyWhenNotMoved mode 1
               * (run400 `takes` @0047BF66).  Nothing else does. */
              gs_set_object_unmoved (game, object, FALSE);
            }

          if (!list.empty ())
            {
              if (has_printed)
                pf_buffer_string (filter, total == 0 ? "\n" : "  ");
              /*
               * 4.0 reworded the loose-in-the-room take; the from-container
               * branch keeps the same verb in every version.  Measured live on
               * a bare `take rock` / `get rock` / `take all`: run370 and
               * run390 answer "You pick up the rock.", run400 "You take the
               * rock."  (run380 shows the pre-4.0 half of the same handler
               * through its "nothing to pick up here" refusal.)
               *
               * What 4.0 did change in the from-container branch is the
               * *taken* object's prefix: pre-4.0 prints it raw, 4.0 normalizes
               * it like everything else.  The container is normalized in both.
               * Measured on the one game carried across three .taf versions by
               * the generators (see lib_print_object_np above): `take gun` out
               * of a "some"-prefixed container holding an "a"-prefixed pistol
               * gives run380 "You take a small pistol from some aluminum
               * clothes.", run390 "You take a small pistol from the aluminum
               * clothes.", run400 "You take the small pistol from the aluminum
               * clothes."  Only the pistol's own prefix moves at 4.0; the
               * clothes move at 3.9, which is the "some" rule, not this one.
               *
               * The raw pre-4.0 prefix belongs to the Runner's SINGLE-take
               * handler only -- any take that named exactly one object,
               * with or without an explicit "from" clause (the pistol probe
               * above was a bare `take gun`).  The multi-take loop
               * normalizes in every version: run390 on ALEXIS ("a"-prefixed
               * diary) answers `take diary from table` with "You take a
               * diary from the old oak table." but `take diary and cloak
               * from table` with "You take the diary and the woven cloak
               * from the old oak table." and `get all from table` with "You
               * take the diary, the brass lantern, the woven cloak and the
               * nice food from the old oak table."  (Adrift_8.txt, measured
               * live 2026-08-22.)
               *
               * That multi-take exception is a 3.9 change of its own: pre-3.9
               * the multi-take loop prints raw too, so from 3.7 to 3.8 every
               * from-container take is raw whatever it named.  Measured live
               * under Wine 2026-08-24 on mikes.taf (3.80) in run380, whose
               * dresser holds four objects with EMPTY prefixes (deobfuscated
               * from the .taf, so this is not a guess): `take all from
               * dresser` answers "You take a socks, a shirt, a underwear and
               * a pair of pants from the dresser."  Empty is the case that
               * separates the two printers -- raw defaults it to "a ", the
               * normalizing one to "the " -- and the same replay pins both
               * halves at once, since the container itself comes out "the
               * dresser".  3.9 normalizes the same shape: the ALEXIS `get all
               * from table` above.
               */
              if (parent == -1)
                pf_buffer_string (filter,
                                  lib_is_version_400 (game)
                                  ? lib_select_response (game,
                                                         "You take ",
                                                         "I take ",
                                                         "%player% take ")
                                  : lib_select_response (game,
                                                         "You pick up ",
                                                         "I pick up ",
                                                         "%player% pick up "));
              else
                pf_buffer_string (filter,
                                  lib_select_response (game,
                                                       "You take ",
                                                       "I take ",
                                                       "%player% take "));
              lib_print_list (game, list,
                              parent == -1 || lib_is_version_400 (game)
                              || (prop_get_taf_version (gs_get_bundle (game))
                                  >= TAF_VERSION_390
                                  && !(lib_take_single_named
                                       || lib_take_from_single_named))
                              ? lib_print_object_np : lib_print_object,
                              " and ");
              if (parent != -1)
                {
                  pf_buffer_string (filter, " from ");
                  lib_print_object_np (game, parent);
                }
              pf_buffer_character (filter, '.');
            }
          total += (scr_int) list.size ();
          has_printed |= !list.empty ();
        }
    }

  /*
   * If we ran out of capacity, either in weight or in size, print the
   * details.  Each over-weight object gets its own sentence, in the order
   * encountered; a "hands are full" size refusal is printed at most once.
   */
  {
    scr_bool size_reported;
    size_t over;

    size_reported = FALSE;
    for (over = 0; over < over_capacity.size (); over++)
      {
        if (over_is_size[over])
          {
            if (size_reported)
              continue;
            size_reported = TRUE;

            lib_print_clause (game, has_printed,
                              "Your hands are full.",
                              "My hands are full.",
                              "%player%'s hands are full.");
          }
        else
          {
            lib_new_clause (game, has_printed);
            lib_print_too_heavy (game, over_capacity[over]);
          }
        has_printed |= TRUE;
      }
  }

  /*
   * Note any remaining multiple references left out of the take operation.
   * This is some workload...
   *
   * First, deal with the case where we have an associated object.
   */
  if (is_associate_object)
    {
      list.clear ();
      for (object = 0; object < object_count; object++)
        {
          if (!game->multiple_references[object])
            continue;

          if (gs_object_position (game, object) == OBJ_HELD_PLAYER
              || gs_object_position (game, object) == OBJ_WORN_PLAYER)
            continue;

          list.push_back (object);
          game->multiple_references[object] = FALSE;
        }

      if (!list.empty ())
        {
          lib_new_clause (game, has_printed);
          lib_print_list (game, list, lib_print_object_np, " and ");
          pf_buffer_string (filter,
                            list.size () == 1
                            ? lib_select_plurality (game, list[0],
                                                    " is not ", " are not ")
                            : " are not ");
          if (obj_is_container (game, associate))
            {
              pf_buffer_string (filter, "in ");
              if (obj_is_surface (game, associate))
                pf_buffer_string (filter, "or on ");
            }
          else
            pf_buffer_string (filter, "on ");
          lib_print_object_np (game, associate);
          pf_buffer_character (filter, '.');
        }
      has_printed |= !list.empty ();
    }

  /*
   * Now, deal with the case where we have an associated NPC.  Once this
   * case is handled, we can force the object references so that the code
   * that follows on from here will report errors taking all objects.
   *
   * Note that this means that we can never successfully take an object
   * from an NPC; that'll have to happen via a game's own commands.
   */
  if (is_associate_npc)
    {
      list.clear ();
      for (object = 0; object < object_count; object++)
        {
          if (!game->multiple_references[object])
            continue;

          if (gs_object_position (game, object) == OBJ_PART_NPC)
            continue;

          list.push_back (object);
          game->multiple_references[object] = FALSE;
        }

      if (!list.empty ())
        {
          lib_new_clause (game, has_printed);
          lib_print_npc_np (game, associate);
          pf_buffer_string (filter, " is not carrying ");
          lib_print_list (game, list, lib_print_object_np, " or ");
          pf_buffer_character (filter, '!');
        }
      has_printed |= !list.empty ();

      /*
       * Merge any remaining object references into multiple references,
       * so that succeeding code complains about the inability to acquire
       * these objects.
       */
      for (object = 0; object < object_count; object++)
        {
          game->multiple_references[object] |= game->object_references[object];
          game->object_references[object] = FALSE;
        }
    }

  /*
   * The remainder of this routine is common error reporting for both object
   * and NPC associates (and also for no associates).
   */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  /*
   * 4.0 gives the tasks their look at a held object too, and earlier than
   * the one ahead of " can't take " below: run400's take piece
   * Proc_19_39_46302C pre-matches "get <the object>" in the take-family
   * mode (@462AED / @462B84) and dispatches it with the restriction-failure
   * pass on (@462C5C, Proc_19_24_44CCE0(1, 1)) BEFORE either the
   * " can't take " test at 462CA0 or the " already carrying " one at
   * 462D01 -- and a claim exits the piece outright.  So a task restricted on
   * already holding the object answers with its own fail message where the
   * library would have said "You are already carrying the cone.".
   *
   * Measured on IceCream.taf in run400 (Adrift_900_icecream2.txt,
   * 2026-09-07): `take cone`, with the cone already in hand at the start of
   * the game, is task 14's "  You already have an empty cone." -- the
   * FailMessage of its one restriction -- and not the library refusal.
   *
   * The look-up is the one 4.0's implicit take uses, definite name and
   * " from <holder>" clause included; see
   * lib_try_game_command_take_definite().
   */
  if (lib_is_version_400 (game) && !list.empty ())
    {
      lib_list_t held;
      scr_bool is_claimed = FALSE;

      for (const scr_int object : list)
        {
          if (lib_try_game_command_take_definite (game, object))
            is_claimed = TRUE;
          else
            held.push_back (object);
        }
      list.swap (held);
      if (is_claimed)
        {
          /* The task's text is complete in itself, terminator and all. */
          lib_take_refusal_claimed = TRUE;
          has_printed = TRUE;
        }
    }

  /*
   * A take that named a single object uses the Runner's single-take
   * handler, which words the held-object refusal "I am already carrying
   * <object>." (run400 @00462D25); "'ve already got <object>!" is the
   * multi-take processor's wording (run400 @0047BE1B).
   *
   * 4.0 only: the pre-4.0 Runners have no "already carrying" wording at
   * all (no such literal in run370/run380/run390.bas; the only held-take
   * refusal is "'ve already got <object>!" -- run370 436561, run380
   * 43E03E, run390_3 454EC5).  Measured live 2026-08-31 on cave.taf
   * run380: single-named `take parchment` while holding it answers
   * "You've already got half of a parchment!".
   */
  if (lib_is_version_400 (game) && lib_take_single_named && list.size () == 1)
    {
      lib_new_clause (game, has_printed);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are already carrying ",
                                             "I am already carrying ",
                                             "%player% is already carrying "));
      lib_print_object_np (game, list[0]);
      pf_buffer_character (filter, '.');
      has_printed |= TRUE;
    }
  else
    has_printed |= lib_print_object_list (game, has_printed, list, " and ", '!',
                                          "You've already got ",
                                          "I've already got ",
                                          "%player%'ve already got ");

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      if (gs_object_position (game, object) != OBJ_WORN_PLAYER)
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  has_printed |= lib_print_object_list (game, has_printed, list, " and ", '!',
                                        "You're already wearing ",
                                        "I'm already wearing ",
                                        "%player% is already wearing ");

  for (npc = 0; npc < gs_npc_count (game); npc++)
    {
      list.clear ();
      for (object = 0; object < object_count; object++)
        {
          if (!game->multiple_references[object])
            continue;

          if (gs_object_position (game, object) != OBJ_HELD_NPC
              && gs_object_position (game, object) != OBJ_WORN_NPC)
            continue;
          if (gs_object_parent (game, object) != npc)
            continue;

          list.push_back (object);
          game->multiple_references[object] = FALSE;
        }

      if (!list.empty ())
        {
          lib_new_clause (game, has_printed);
          lib_print_npc_np (game, npc);
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 " refuses to give you ",
                                                 " refuses to give me ",
                                                 " refuses to give %player% "));
          lib_print_list (game, list, lib_print_object_np, " and ");
          pf_buffer_character (filter, '!');
        }
      has_printed |= !list.empty ();
    }

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  /*
   * Only the 4.0 single-take handler ends this with "!" (run400 @47329D
   * builds " can't take " + name + "!"); its multi-take processor
   * (run400 @47C172) and both of 3.9's (run390 @4551A6, @455533) end it
   * with ".".  Measured 2026-08-29 in run390 on CAH.taf (cruel), `take it`
   * after `x jacket` -> "(a jacket)" / "You can't take the jacket.".
   */
  /*
   * 4.0 gives the tasks one more look at each object it is about to refuse:
   * run400's per-piece take handler (Proc_19_23_473A34) calls the task
   * pre-matcher Proc_19_35_453C50 at @473241, directly ahead of the loop
   * that builds " can't take " (@47329D), and a hit re-dispatches to the
   * task dispatcher Proc_19_24_44CCE0, restriction-failure pass included.
   * The match is by the resolved object's name, not by the typed words:
   * `take kelly` -- an alias, with no "poster" anywhere in the line -- still
   * runs "Get * poster", as do `take poster`, `pick up poster`, `take the
   * poster` and `take bed` ("Get * bed"); `grab poster` is no take verb at
   * all and gets "I don't understand what you want me to do with the kelly
   * brook poster."  (man overboard.taf, run400, Adrift_1_moprobe.txt and
   * Adrift_1_man_overboard.txt line 39-40, measured 2026-08-29.)  The loop
   * at the top of this function does the same for take-able objects; the
   * static ones never reach it, so they get their turn here instead.
   */
  if (!has_printed && !list.empty () && lib_is_version_400 (game))
    {
      lib_list_t refused;
      for (const scr_int object : list)
        {
          if (!lib_try_game_command_short_canonical (game, "get", object))
            refused.push_back (object);
        }
      list.swap (refused);
      /* The tasks' text is complete in itself; no refusal line follows. */
      lib_take_refusal_claimed = list.empty ();
    }
  lib_print_object_list (game, has_printed, list, " and ",
                         lib_is_version_400 (game)
                         && lib_take_single_named && list.size () == 1
                         ? '!' : '.',
                         "You can't take ",
                         "I can't take ",
                         "%player% can't take ");

  lib_take_single_named = FALSE;
  lib_take_from_single_named = FALSE;
}


/*
 * lib_take_backend()
 * lib_take_from_object_backend()
 * lib_take_from_npc_backend()
 *
 * Facets of lib_take_backend_common().  Provide backend handling for either
 * the plain "take" handlers, or the "take from <something>" handlers.
 */
static void
lib_take_backend (scr_gameref_t game)
{
  lib_take_backend_common (game, -1, FALSE, FALSE);
}

static void
lib_take_from_object_backend (scr_gameref_t game, scr_int associate)
{
  lib_take_backend_common (game, associate, TRUE, FALSE);
}

static void
lib_take_from_npc_backend (scr_gameref_t game, scr_int associate)
{
  lib_take_backend_common (game, associate, FALSE, TRUE);
}


/*
 * lib_take_filter()
 *
 * Helper function for deciding if an object may be acquired in this context.
 * Returns TRUE if an object may be acquired, FALSE otherwise.
 */
static scr_bool
lib_take_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  /*
   * To be take-able by name, an object must be visible in the room, not
   * static, and not already held or worn by the player or an NPC.  Note
   * that obj_indirectly_in_room() recurses only through open containers
   * and surfaces, so a named take (or "take X and Y") reaches objects
   * inside an open container or on a surface present in the room -- the
   * Runners' "and"-list take mode does the same -- while excluding the
   * contents of closed containers.
   */
  return obj_indirectly_in_room (game, object, gs_playerroom (game))
         && !obj_is_static (game, object)
         && !(gs_object_position (game, object) == OBJ_HELD_PLAYER
              || gs_object_position (game, object) == OBJ_WORN_PLAYER)
         && !(gs_object_position (game, object) == OBJ_HELD_NPC
              || gs_object_position (game, object) == OBJ_WORN_NPC);
}


/*
 * lib_take_all_filter()
 *
 * The universe that "all" ranges over, which is much narrower than the set
 * a named take can reach: every Runner builds the "all" candidate list from
 * dynamic objects lying directly on the floor of the player's room (run380
 * takes() requires location == current room and static == 0; run400's take
 * handler, Proc_19_6, does the same plus a seen test).  Objects inside or
 * on top of other objects are never included, even when the container is
 * open and its contents revealed -- only naming them, or "take all from X",
 * reaches inside.  A direct consequence is that "all" also leaves alone
 * anything already in the player's possession, including the contents of a
 * carried open container: in Ticket to No Where, holding the open bag of
 * shopping and typing "get all" answers "You take the pamphlet." and leaves
 * the tights, pet food, deodorant and gloves in the bag, while "get paper"
 * still lifts the scrap of paper out of the carried wallet (verified live
 * against run400.exe, 2026-08-02).
 *
 * The seen test does need porting: since obj_mark_room_objects_seen() moved
 * the room's marking into the room lister, a loose object in a room whose
 * description never printed is unseen, and 4.0 leaves it where it lies.
 */
static scr_bool
lib_take_all_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == gs_playerroom (game) + 1
         && (!lib_matcher_requires_seen (game)
             || gs_object_seen (game, object));
}


/*
 * lib_cmd_take_all()
 *
 * Attempt to take all objects currently visible to the player.
 */
scr_bool
lib_cmd_take_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_take_all_filter, -1, FALSE, NULL);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_take_backend (game);
  else
    pf_buffer_string (filter,
                      lib_is_version_400 (game)
                      ? "There is nothing worth taking here."
                      : "There is nothing to pick up here.");

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_take_absent_score()
 *
 * The Runner's co() score for one object against the typed line: 1 if the
 * line contains the whole Short, 1 more for the first Alias it contains,
 * and 1 more for each word of the Prefix that appears in the line (run400
 * General.Sub_22_66 at 000632BE .. 00063387).  Zero means the line names
 * nothing of this object -- a Prefix word alone never does, since the
 * prefix loop is only reached once a name has matched.  *term is left at
 * the name the object answered to, Short before Alias, which is what
 * mdlSpreadTheLoad.Sub_20_43 at 00046BFC hands the ambiguity message.
 */
static scr_int
lib_take_absent_score (scr_gameref_t game, scr_int object,
                       const scr_char *input, const scr_char **term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname, *prefix;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias, score;

  score = 0;
  *term = NULL;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (shortname) && lib_co_contains (input, shortname))
    {
      score++;
      *term = shortname;
    }

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Alias";
  alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *name;

      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (scr_strempty (name) || !lib_co_contains (input, name))
        continue;

      score++;
      if (!*term)
        *term = name;
      break;
    }

  if (score == 0)
    return 0;

  /* Each Prefix word the line also carries is worth another point. */
  prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
  if (!scr_strempty (prefix))
    {
      std::string word;
      const scr_char *cursor;

      for (cursor = prefix; ; cursor++)
        {
          if (*cursor != NUL && !scr_isspace (*cursor))
            {
              word.append (1, *cursor);
              continue;
            }

          if (!word.empty () && lib_co_contains (input, word.c_str ()))
            score++;
          word.clear ();

          if (*cursor == NUL)
            break;
        }
    }

  return score;
}


/*
 * lib_cmd_take_absent()
 *
 * 4.0's named take once nothing here can answer to the noun.  The Runner's
 * take handler (run400 Proc_19_6) resolves the direct object with the co()
 * style whole-name resolver General.Sub_22_66 at 00073011, whose result is
 * 255 for "the line named nothing", a positive index+256 for one object, and
 * -(index + 2) for an ambiguity; the three answers are printed at the tail
 * of the same procedure -- "Take what?" at 0007332B and again as the very
 * last default at 00073A25, "It is not clear which " & <term> & " you are
 * referring to." at 0007335C, and "There is nothing worth taking here." at
 * 00073A13, which is where a line that resolved an object but took nothing
 * lands (the `If var_92 > 255` at 00073798 is the "take X from Y" test, so a
 * plain named take falls straight through to it).
 *
 * The resolver runs its scan TWICE.  Its first pass (mode 1, at 00063161)
 * only considers what Sub_22_61 calls present -- held, worn, in the room, or
 * inside an open container there -- and if that pass ends with no candidate
 * at all it resets the mode to 0 and jumps back to the top of the procedure
 * (00063465: `If var_9C = 1 And var_A0 = 0 And param_10 > 0`, then Branch
 * 000630BC), where the test at 0006310D is the object's seen byte alone.  So
 * the noun that names nothing here is offered every object the player has
 * ever SEEN, and that second pass is what this function reproduces.
 *
 * What it ranges over is measured rather than argued:
 * harness/make_400_takeprobe.py builds p4TAKE (a coin loose in Alpha, a
 * statue static in Alpha, a widget and a gizmo loose in Bravo), with --tie
 * p4TAKE2 (a red widget and a blue widget, both Short "widget" with Prefix
 * "a red"/"a blue", plus a lamp aliased "light", all in Bravo), and with
 * --hidden p4TAKE3 (the same pair, with tasks that move either or both of
 * them to hidden).  Six feeds under Wine, run400, transcripts
 * Adrift_p4take1 .. Adrift_p4take6 in the harness prefix:
 *
 *     take widget   (Bravo never entered)        ->  Take what?
 *     take widget   (from Alpha, Bravo seen)     ->  There is nothing worth
 *                                                    taking here.
 *     take statue   (from Bravo, Alpha seen)     ->  There is nothing worth
 *                                                    taking here.
 *     take light    (from Alpha, an ALIAS)       ->  There is nothing worth
 *                                                    taking here.
 *     take widget   (two seen absent namesakes)  ->  It is not clear which
 *                                                    widget you are referring
 *                                                    to.
 *     take widget   (one, then both, hidden)     ->  It is not clear which
 *                                                    widget you are referring
 *                                                    to.
 *     take red      (a Prefix word only)         ->  Take what?
 *     take zzz                                   ->  Take what?
 *     take widget   (both present, in Bravo)     ->  Which widget.  The red
 *                                                    widget or the blue
 *                                                    widget?
 *
 * So the candidate set is the objects the player has SEEN -- the same seen
 * byte the examine path reads, and the same one that makes `x widget` answer
 * "You can't see the widget from here!" in the very next command of
 * Adrift_p4take2 -- matched by WHOLE name, Short or Alias, with a Prefix word
 * naming nothing on its own.  Statics count (the statue) and so do hidden
 * objects (p4TAKE3's pair still ties with both of them nowhere).  A candidate
 * that is present hands the line back to the ordinary path, which is where
 * the present tie's co() prompt is raised.
 *
 * The scores then decide between the two refusals, and only a genuine tie
 * for the best score is ambiguous.  That is what zelda's `get key` turns on:
 * its super-hot key is Short "key" with an Alias "key" as well, so it scores
 * 2 against the iron key's 1 and wins outright -- run400 answers with the
 * flat refusal (Adrift_319_zelda.txt:573) even though both keys are seen and
 * both are called "key".
 */
scr_bool
lib_cmd_take_absent (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *best_term = NULL;
  scr_int object, best_score, best_count;

  if (!lib_is_version_400 (game) || !input)
    return FALSE;

  best_score = 0;
  best_count = 0;

  for (object = 0; object < gs_object_count (game); object++)
    {
      const scr_char *term;
      scr_int score;

      score = lib_take_absent_score (game, object, input, &term);
      if (score == 0)
        continue;

      /* Something the noun names is here; the ordinary path handles it. */
      if (obj_indirectly_in_room (game, object, gs_playerroom (game)))
        return FALSE;

      if (!gs_object_seen (game, object))
        continue;

      if (score > best_score)
        {
          best_score = score;
          best_count = 1;
          best_term = term;
        }
      else if (score == best_score)
        {
          best_count++;
          best_term = term;
        }
    }

  /* Nothing the player has seen answers to it; "Take what?" says so. */
  if (best_count == 0)
    return FALSE;

  if (best_count > 1)
    {
      pf_buffer_string (filter, "It is not clear which ");
      pf_buffer_string (filter, best_term);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             " you are referring to.\n",
                                             " I am referring to.\n",
                                             " %player% is referring"
                                             " to.\n"));
      return TRUE;
    }

  pf_buffer_string (filter, "There is nothing worth taking here.\n");
  return TRUE;
}


/*
 * lib_take_multiple_common()
 *
 * Take the objects available to the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 */
static scr_bool
lib_take_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int);
  scr_int objects, references;
  scr_bool library_printed;

  /*
   * "take all except ..." works over the "all" universe, which excludes
   * whatever the player already carries; a named take reaches further, into
   * a carried open container (see lib_take_all_filter).
   */
  resolver = is_except ? lib_take_all_filter : lib_take_filter;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "leave" : "take",
                                   resolver, -1,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Note single-object takes; the backend words some refusals differently. */
  lib_take_single_named = !is_except && references == 1;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              resolver, -1, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_take_backend (game);
  else if (lib_is_version_400 (game))
    {
      /*
       * 4.0 has one flat refusal and no "else" form: run400 answers
       * "There is nothing worth taking here." to `take all except rock`
       * whether or not the rock is the only thing left in the room.
       */
      pf_buffer_string (filter, "There is nothing worth taking here.");
    }
  else
    {
      pf_buffer_string (filter, "There is nothing");
      if (is_except && objects == 0)
        pf_buffer_string (filter, " else");
      pf_buffer_string (filter, " to pick up here.");
    }
  lib_take_single_named = FALSE;

  if (!lib_take_refusal_claimed)
    pf_buffer_character (filter, '\n');
  lib_take_refusal_claimed = FALSE;
  return TRUE;
}


/*
 * lib_cmd_take_except_multiple()
 * lib_cmd_take_multiple()
 *
 * Facets of lib_take_multiple_common().
 */
scr_bool
lib_cmd_take_except_multiple (scr_gameref_t game)
{
  return lib_take_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_take_multiple (scr_gameref_t game)
{
  return lib_take_multiple_common (game, FALSE);
}


/*
 * lib_take_from_filter()
 *
 * Helper function for deciding if an object may be acquired in this context.
 * Returns TRUE if an object may be acquired, FALSE otherwise.
 */
static scr_bool
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

  for (object = 0; object < gs_object_count (game); object++)
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
 * (Adrift_8.txt, measured live 2026-08-22).
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
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You take ",
                                           "I take ",
                                           "%player% take "));
}


/*
 * lib_take_from_empty()
 *
 * Common error handling for when nothing is taken from a container or
 * supporter object.
 */
static void
lib_take_from_empty (scr_gameref_t game, scr_int associate, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);

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
 * lib_take_from_is_valid()
 *
 * Validate the supporter requested in "take from" commands.
 */
static scr_bool
lib_take_from_is_valid (scr_gameref_t game, scr_int associate)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* Disallow emptying non-container/non-surface objects. */
  if (!(obj_is_container (game, associate)
        || obj_is_surface (game, associate)))
    {
      lib_print_response_object (game,
                                 "You can't take anything from ",
                                 "I can't take anything from ",
                                 "%player% can't take anything from ",
                                 associate, ".\n");
      return FALSE;
    }

  /* Pre-3.9: a dynamic supporter must be held or worn (run380 446CFB). */
  if (lib_take_container_unheld (game, associate))
    {
      lib_print_not_holding (game, associate, ".\n");
      return FALSE;
    }

  /* If object is a container, and is closed, reject now. */
  if (obj_is_container (game, associate)
      && gs_object_openness (game, associate) > OBJ_OPEN)
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, associate);
      /* Always " is ": see the openness table in lib_cmd_examine_object(). */
      pf_buffer_string (filter, " is closed.\n");
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

  /* Get the referenced object, and if none, consider complete. */
  associate = lib_disambiguate_object (game, "take from", &is_ambiguous);
  if (associate == -1)
    return is_ambiguous;

  /* Validate the associate object to take from. */
  if (!lib_take_from_is_valid (game, associate))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_take_from_filter, associate, FALSE, NULL);
  gs_clear_multiple_references (game);
  lib_take_from_single_named = FALSE;
  if (objects > 0)
    lib_take_from_object_backend (game, associate);
  else if (lib_take_from_unseen (game, associate))
    lib_take_from_unseen_refusal (game, associate);
  else
    lib_take_from_empty (game, associate, FALSE);

  pf_buffer_character (filter, '\n');
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
  scr_int associate, objects, references;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  associate = lib_disambiguate_object (game, "take from", &is_ambiguous);
  if (associate == -1)
    return is_ambiguous;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "leave" : "take",
                                   lib_take_from_filter, associate,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Note single-object takes; the backend prints their prefix raw pre-4.0. */
  lib_take_from_single_named = !is_except && references == 1;

  /* Validate the associate object to take from. */
  if (!lib_take_from_is_valid (game, associate))
    return TRUE;

  /* As a special case, complain about requests to retain the associate. */
  if (is_except
      && lib_multiple_retains_associate (game, associate, "leave"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_take_from_filter, associate, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_take_from_object_backend (game, associate);
  else if (lib_take_from_unseen (game, associate))
    lib_take_from_unseen_refusal (game, associate);
  else
    lib_take_from_empty (game, associate, is_except);

  pf_buffer_character (filter, '\n');
  return TRUE;
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
  associate = lib_disambiguate_npc (game, "take from", &is_ambiguous);
  if (associate == -1)
    return is_ambiguous;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_take_from_npc_filter, associate, FALSE, NULL);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_take_from_npc_backend (game, associate);
  else
    {
      pf_new_sentence (filter);
      lib_print_npc_np (game, associate);
      pf_buffer_string (filter, " is not carrying anything!");
    }

  pf_buffer_character (filter, '\n');
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
  associate = lib_disambiguate_npc (game, "take from", &is_ambiguous);
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

  pf_buffer_character (filter, '\n');
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


/*
 * The verb-specific half of drop, remove, and put-on.  All three list the
 * objects they acted on, then list the ones they had to leave alone, and
 * differ only in how an object moves and in the words around the lists.
 */
typedef struct
{
  void (*move) (scr_gameref_t game, scr_int object, scr_int target);
  const scr_char *onto;       /* " onto ", or NULL where there is no target */
  const scr_char *does[3];    /* "You drop ", and so on */
  const scr_char *lacks[3];   /* "You are not holding ", and so on */
  scr_char lacks_end;         /* Terminator for the "not holding" list */
  scr_bool raw_prefix_pre_390;/* Print the authored prefix below 3.9 */
  /*
   * Below 3.9 the leftovers are not a list at all.  run380's drops()
   * (@438DD5-438E13) and its remove handler (@430076-4300C8) each walk the
   * objects in index order and, for the first name-match they cannot act
   * on, set the turn's message ONLY IF IT IS STILL EMPTY: "<You> don't have
   * <raw prefix> <short>!" and "<You> <are> not wearing <raw prefix>
   * <short>!" -- so a second unactionable object is never named, and none
   * is once something was dropped or removed on the same line.  run370 is
   * the same code (drop @430BD4, remove @429954).  NULL keeps the list.
   */
  const scr_char *lacks_pre_390[3];
  scr_char lacks_end_pre_390;
} lib_move_verb_t;

static void
lib_move_to_room (scr_gameref_t game, scr_int object, scr_int target)
{
  (void) target;
  gs_object_to_room (game, object, gs_playerroom (game));
}

static void
lib_move_to_player (scr_gameref_t game, scr_int object, scr_int target)
{
  (void) target;
  gs_object_player_get (game, object);
}

static void
lib_move_onto (scr_gameref_t game, scr_int object, scr_int target)
{
  gs_object_move_onto (game, object, target);
}

static const lib_move_verb_t LIB_DROP_VERB = {
  lib_move_to_room, NULL,
  {"You drop ", "I drop ", "%player% drop "},
  {"You are not holding ", "I am not holding ", "%player% is not holding "},
  '.', FALSE,
  /* run380 @438E13: `MemVar_44F108(0) & " don't have " & ...` -- the
   * third-person form really is "<name> don't have". */
  {"You don't have ", "I don't have ", "%player% don't have "}, '!'
};

static const lib_move_verb_t LIB_REMOVE_VERB = {
  lib_move_to_player, NULL,
  {"You remove ", "I remove ", "%player% remove "},
  {"You are not wearing ", "I am not wearing ", "%player% is not wearing "},
  '!', TRUE,
  {"You are not wearing ", "I am not wearing ", "%player% is not wearing "}, '!'
};

static const lib_move_verb_t LIB_PUT_ON_VERB = {
  lib_move_onto, " onto ",
  {"You put ", "I put ", "%player% put "},
  {"You are not holding ", "I am not holding ", "%player% is not holding "},
  '.', FALSE,
  {NULL, NULL, NULL}, '.'
};


/*
 * lib_move_try_commands()
 *
 * Try game commands for all referenced objects.  If any succeed, remove that
 * reference from the list.  Returns TRUE if any game command printed, which
 * is what tells the caller's lists whether they have to indent past it.
 */
static scr_bool
lib_move_try_commands (scr_gameref_t game, const scr_char *command,
                       scr_bool use_definite)
{
  scr_int object_count, object;
  scr_bool has_printed;

  has_printed = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      if (use_definite
          ? lib_try_game_command_short_definite (game, command, object)
          : lib_try_game_command_short (game, command, object))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }

  return has_printed;
}


/*
 * lib_move_backend()
 *
 * Shared tail of the drop, remove, and put-on handlers.  Moves and lists
 * every object still flagged in object_references, then lists the ones left
 * in multiple_references as objects the player hasn't got.  The caller has
 * already offered the objects to the game's own handlers, and says with
 * has_printed whether any of those printed anything.
 *
 * Returns TRUE if the backend printed anything of its OWN -- a move clause or
 * a leftover list.  A caller that ends the line with a newline needs to know:
 * when every referenced object went to a game command there is nothing of the
 * library's on the line, and run400's drop routine leaves without adding one.
 */
static scr_bool
lib_move_backend (scr_gameref_t game, const lib_move_verb_t *verb,
                  scr_int target, scr_bool has_printed)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object_count, object;
  scr_bool library_printed;
  lib_list_t list;

  object_count = gs_object_count (game);

  /* Move every object that remains referenced. */
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      list.push_back (object);
      verb->move (game, object, target);
    }

  if (!list.empty ())
    {
      lib_print_clause (game, has_printed,
                        verb->does[0],
                        verb->does[1],
                        verb->does[2]);
      lib_print_list (game, list,
                      verb->raw_prefix_pre_390
                      && prop_get_taf_version (bundle) < TAF_VERSION_390
                      ? lib_print_object_raw : lib_print_object_np, " and ");
      if (verb->onto)
        {
          pf_buffer_string (filter, verb->onto);
          lib_print_object_np (game, target);
        }
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();
  library_printed = !list.empty ();

  /* Note any remaining multiple references left out of the operation. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  /*
   * Pre-3.9: first leftover only, raw prefix, and only while nothing has
   * been said (see lacks_pre_390 above).  Measured on Crime_Adventure.taf
   * (3.80) in run380, 2026-09-04: `remove shoes` with the golf shoes lying
   * unworn in the shop is "You are not wearing a pair of golf shoes!", where
   * Scarier said "the pair of golf shoes"; `drop cash` naming a penny it
   * never held is "You don't have a penny!".
   */
  if (verb->lacks_pre_390[0]
      && prop_get_taf_version (bundle) < TAF_VERSION_390)
    {
      if (!has_printed && !list.empty ())
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 verb->lacks_pre_390[0],
                                                 verb->lacks_pre_390[1],
                                                 verb->lacks_pre_390[2]));
          lib_print_object_raw (game, list[0]);
          pf_buffer_character (filter, verb->lacks_end_pre_390);
          library_printed = TRUE;
        }
      return library_printed;
    }

  lib_print_object_list (game, has_printed, list, " or ", verb->lacks_end,
                         verb->lacks[0], verb->lacks[1], verb->lacks[2]);
  return library_printed || !list.empty ();
}


/*
 * lib_drop_backend()
 *
 * Common backend handler for dropping objects.  Drops all objects currently
 * referenced in the game, trying game commands first, and then moving other
 * unhandled objects to the player room floor.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static scr_bool
lib_drop_backend (scr_gameref_t game)
{
  scr_bool has_printed;

  has_printed = lib_move_try_commands (game, "drop",
                                       lib_is_version_400 (game));

  /*
   * A named object the player is not holding still gets its line offered to
   * the game's tasks before the library refuses it.  Oh, Human (Oh_Human.taf,
   * 4.00): the electrical device sits on the floor with the light circling
   * it, and `drop device` runs task 6 "[drop/get rid of/lose/put down/set
   * down/remove/take off] {the/a} [device] {...}" -- the game's whole
   * free-the-light puzzle -- where the library alone would answer "You are
   * not holding the device." and the game would be unwinnable.  The rebuilt
   * spelling is the same definite form the held objects get, so a task whose
   * command is the bare typed line (p4REPEAT3 task 2 `drop hat`, run400
   * Adrift_952.txt) still loses to the library.
   */
  if (lib_is_version_400 (game))
    {
      const scr_int object_count = gs_object_count (game);
      scr_int object;

      for (object = 0; object < object_count; object++)
        {
          if (!game->multiple_references[object])
            continue;

          if (lib_try_game_command_short_definite (game, "drop", object))
            {
              game->multiple_references[object] = FALSE;
              has_printed = TRUE;
            }
        }
    }

  return lib_move_backend (game, &LIB_DROP_VERB, -1, has_printed);
}


/*
 * lib_drop_filter()
 *
 * Helper function for deciding if an object may be dropped in this context.
 * Returns TRUE if an object may be dropped, FALSE otherwise.
 */
static scr_bool
lib_drop_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == OBJ_HELD_PLAYER;
}

/*
 * lib_drop_named_filter()
 *
 * Variant of the above for explicitly named objects.  The Runner also drops
 * a *worn* object named in a drop command, implicitly removing it first --
 * "drop cloak" while wearing it answers "You drop the cloak." -- while a bare
 * "drop all" leaves worn items alone.  Both verified live against run390;
 * see RUNNER_TESTS_TODO.md section 2.
 */
static scr_bool
lib_drop_named_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && (gs_object_position (game, object) == OBJ_HELD_PLAYER
             || gs_object_position (game, object) == OBJ_WORN_PLAYER);
}


/*
 * lib_cmd_drop_all()
 *
 * Drop all objects currently held by the player.
 */
scr_bool
lib_cmd_drop_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_drop_filter, -1, FALSE, NULL);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_drop_backend (game);
  else
    {
      /*
       * Not a contraction, and 4.0 reworded it.  Every Runner assembles this
       * from the pronoun array as Ary(0) & " " & Ary(4) & <literal>, so the
       * copula is spelled out: run400 name_object 46E5A0 appends
       * " carrying nothing!", run390 445867 (and its 3.7/3.8 twins)
       * " not carrying anything.".  "carrying nothing!" is absent from all
       * three pre-4.0 binaries and the contraction from all four.  Measured
       * live 2026-09-05, run400 on ptbad.taf: a second `drop all` answers
       * "You are carrying nothing!".
       */
      pf_buffer_string (filter,
                        lib_is_version_400 (game)
                        ? lib_select_response (game,
                                          "You are carrying nothing!",
                                          "I am carrying nothing!",
                                          "%player% is carrying nothing!")
                        : lib_select_response (game,
                                          "You are not carrying anything.",
                                          "I am not carrying anything.",
                                          "%player% is not carrying anything."));
    }

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_drop_multiple_common()
 *
 * Drop the objects held by the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 */
static scr_bool
lib_drop_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_bool (*resolver) (scr_gameref_t, scr_int, scr_int);
  scr_int objects, references;
  scr_bool library_printed;

  /*
   * Named objects may also be dropped from worn; the "all" universe that
   * "drop all except ..." works over is held objects only.
   */
  resolver = is_except ? lib_drop_filter : lib_drop_named_filter;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "drop",
                                   resolver, -1,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              resolver, -1, is_except,
                              &references);
  if (objects > 0 || references > 0)
    library_printed = lib_drop_backend (game);
  else
    {
      lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");
      library_printed = TRUE;
    }

  /*
   * 4.0 runs this handler above the task dispatcher, so a drop every one of
   * whose objects went to a game command leaves the line entirely to that
   * task -- and run400's drop routine returns from its per-object arm without
   * closing a line it never opened (@46F358).  Adding one here put a blank
   * line after Glum Fiddle's `drop tray` and JGrim's `drop mud`.
   */
  if (library_printed || !lib_is_version_400 (game))
    pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_cmd_drop_except_multiple()
 * lib_cmd_drop_multiple()
 *
 * Facets of lib_drop_multiple_common().
 */
scr_bool
lib_cmd_drop_except_multiple (scr_gameref_t game)
{
  return lib_drop_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_drop_multiple (scr_gameref_t game)
{
  return lib_drop_multiple_common (game, FALSE);
}


/*
 * lib_cmd_give_object_npc()
 * lib_cmd_give_object()
 *
 * Attempt to give an object to an NPC.
 */
scr_bool
lib_cmd_give_object_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, npc;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "give", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "give to", NULL);
  if (npc == -1)
    return TRUE;

  /* Reject if not holding the object offered. */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You don't have ",
                                 "I don't have ",
                                 "%player% don't have ",
                                 object, "!\n");
      return TRUE;
    }

  /* After all that, the npc is disinterested. */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  lib_print_wrapped_object (game, " doesn't seem interested in ",
                            object, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_give_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "give", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject if not holding the object offered. */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You don't have ",
                                 "I don't have ",
                                 "%player% don't have ",
                                 object, "!\n");
      return TRUE;
    }

  /* After all that, we have to ask (and shouldn't this be "to whom?"). */
  lib_print_wrapped_object (game, "Give ", object, " to who?\n");
  return TRUE;
}


/*
 * lib_wear_backend()
 *
 * Common backend handler for wearing objects.  Puts on all objects currently
 * referenced in the game, moving objects to worn by player.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static void
lib_wear_backend (scr_gameref_t game)
{
  scr_int object_count, object;
  scr_bool has_printed;
  lib_list_t list;

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.
   */
  has_printed = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      if (lib_try_game_command_short (game, "wear", object))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
        }
    }

  /* Wear every object referenced. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      list.push_back (object);
      gs_object_player_wear (game, object);
    }

  /*
   * Pre-3.9 announces the wear with the object's own prefix where every other
   * report normalizes it: run370 and run380 answer "You put on a rusty w3."
   * and "You put on a w1." for an empty prefix, against "You pick up the rusty
   * w3." on the take path in the same session.  A "some" or "the ..." prefix
   * comes out verbatim either way, so the "a ..." and empty cases are what
   * separate the two printers.  3.9 normalized this along with the rest:
   * run390 and run400 both say "You put on the w0, ..." for the very same
   * objects.  Measured on the pwear probe carried across all four Runners
   * (2026-08-23); the empty-prefix default is a bare "a" even before a vowel,
   * measured separately on pwearv.  See RUNNER_TESTS_TODO.md section 4.
   */
  has_printed |= lib_print_object_list (game, has_printed, list, " and ", '.',
                                        "You put on ",
                                        "I put on ",
                                        "%player% put on ",
                                        prop_get_taf_version
                                        (gs_get_bundle (game))
                                        >= TAF_VERSION_390
                                        ? lib_print_object_np
                                        : lib_print_object);

  /* Note any remaining multiple references left out of the wear operation. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      if (gs_object_position (game, object) != OBJ_WORN_PLAYER)
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  /* The 4.0 Runner ends this with "!"; pre-4.0 Runners build the wear-path
     variant of this message without the "!" (run400 47BE3C/4638FE vs
     run380 432FCB). */
  has_printed |= lib_print_object_list (game, has_printed, list, " and ",
                                        lib_is_version_400 (game) ? '!' : '.',
                                        "You are already wearing ",
                                        "I am already wearing ",
                                        "%player% is already wearing ");

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  has_printed |= lib_print_object_list (game, has_printed, list, " or ", '.',
                                        "You are not holding ",
                                        "I am not holding ",
                                        "%player% is not holding ");

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  lib_print_object_list (game, has_printed, list, " or ", '.',
                         "You can't wear ",
                         "I can't wear ",
                         "%player% can't wear ");
}


/*
 * lib_wear_filter()
 *
 * Helper function for deciding if an object may be worn in this context.
 * Returns TRUE if an object may be worn, FALSE otherwise.
 */
static scr_bool
lib_wear_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  assert (unused == -1);

  /*
   * The object is wearable if the player is holding it, and it's not static
   * (static moved to player inventory by event), and if it's marked wearable
   * in properties.
   */
  if (gs_object_position (game, object) == OBJ_HELD_PLAYER
      && !obj_is_static (game, object))
    {
      scr_vartype_t vt_key[3];

      /* Return wearability from the object properties. */
      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Wearable";
      return prop_get_boolean (bundle, "B<-sis", vt_key);
    }

  return FALSE;
}


/*
 * lib_cmd_wear_all()
 *
 * Wear all wearable objects currently held by the player.
 */
scr_bool
lib_cmd_wear_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_wear_filter, -1, FALSE, NULL);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_wear_backend (game);
  else
    {
      /*
       * " that can be worn." is SCARE's own: no Runner holds it, or any
       * fragment of it.  All four print the one literal
       * " don't have anything to wear." after Ary(0) -- un-conjugated, so
       * the third person reads "%player% don't have ...".  Measured live
       * 2026-09-05, run400 on ptbad.taf with an empty inventory.
       */
      pf_buffer_string (filter,
                        lib_select_response (game,
                                           "You don't have anything to wear.",
                                           "I don't have anything to wear.",
                                           "%player% don't have anything to wear."));
    }

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_wear_multiple_common()
 *
 * Wear the wearable objects held by the player and listed in %text%, or --
 * for is_except -- every one of them but those listed.
 */
static scr_bool
lib_wear_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects, references;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "wear",
                                   lib_wear_filter, -1,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_wear_filter, -1, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_wear_backend (game);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0,
                            " that can be worn.");

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_cmd_wear_except_multiple()
 * lib_cmd_wear_multiple()
 *
 * Facets of lib_wear_multiple_common().
 */
scr_bool
lib_cmd_wear_except_multiple (scr_gameref_t game)
{
  return lib_wear_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_wear_multiple (scr_gameref_t game)
{
  return lib_wear_multiple_common (game, FALSE);
}


/*
 * lib_remove_backend()
 *
 * Common backend handler for removing objects.  Takes off on all objects
 * currently referenced in the game, moving objects to held by player.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static void
lib_remove_backend (scr_gameref_t game)
{
  scr_bool has_printed;

  has_printed = lib_move_try_commands (game, "remove", FALSE);
  lib_move_backend (game, &LIB_REMOVE_VERB, -1, has_printed);
}


/*
 * lib_remove_filter()
 *
 * Helper function for deciding if an object may be removed in this context.
 * Returns TRUE if an object is currently being worn, FALSE otherwise.
 */
static scr_bool
lib_remove_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return !obj_is_static (game, object)
         && gs_object_position (game, object) == OBJ_WORN_PLAYER;
}


/*
 * lib_cmd_remove_all()
 *
 * Remove all objects currently held by the player.
 */
scr_bool
lib_cmd_remove_all (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects;

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_remove_filter, -1, FALSE, NULL);
  gs_clear_multiple_references (game);
  if (objects > 0)
    lib_remove_backend (game);
  else
    {
      /*
       * As above: " that can be removed." is SCARE's, and the Runners all
       * assemble Ary(0) & " " & Ary(4) & " not wearing anything.".
       * Measured live 2026-09-05, run400 on ptbad.taf.
       */
      pf_buffer_string (filter,
                        lib_select_response (game,
                                           "You are not wearing anything.",
                                           "I am not wearing anything.",
                                           "%player% is not wearing anything."));
    }

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_remove_multiple_common()
 *
 * Remove the objects worn by the player and listed in %text%, or -- for
 * is_except -- every one of them but those listed.
 *
 * The two forms disagree over the empty complaint: the except form says
 * "not wearing", the plain one "not holding".  Kept as it stands upstream.
 */
static scr_bool
lib_remove_multiple_common (scr_gameref_t game, scr_bool is_except)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int objects, references;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "remove",
                                   lib_remove_filter, -1,
                                   &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              lib_remove_filter, -1, is_except,
                              &references);
  if (objects > 0 || references > 0)
    lib_remove_backend (game);
  else
    lib_print_nothing_held (game, is_except, is_except && objects == 0,
                            " that can be removed.");

  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_cmd_remove_except_multiple()
 * lib_cmd_remove_multiple()
 *
 * Facets of lib_remove_multiple_common().
 */
scr_bool
lib_cmd_remove_except_multiple (scr_gameref_t game)
{
  return lib_remove_multiple_common (game, TRUE);
}

scr_bool
lib_cmd_remove_multiple (scr_gameref_t game)
{
  return lib_remove_multiple_common (game, FALSE);
}


/*
 * lib_cmd_inventory()
 *
 * List objects carried and worn by the player.
 *
 * Listing reveals: every object this prints is marked seen, exactly as the
 * NPC lister marks an NPC's possessions (lib_list_npc_inventory).  That is
 * the ONLY thing that reveals a possession the player was never shown --
 * there is no standing "anything you hold is seen" rule, which is what this
 * port used to have.
 *
 * Probe SEEN, driven in run400 2026-09-07 (Adrift_p4seen.txt), walks the
 * four ways into the player's possession with four hidden objects and asks
 * `x <name>` after each:
 *
 *   zza  task action, move object -> held by player   x alpha  "A probe object."
 *   zzb  task action, move object -> worn by player   x bravo  "A probe object."
 *   zzc  task action, move object -> the player's
 *        room, no room description printed            x gamma  "A probe object."
 *   zzd  a task that starts an event whose Obj1
 *        goes -> held by player                       x delta  "You see no such
 *                                                               thing."
 *
 * The first three are the task mover's own post-move seen stamp (see
 * task_move_object); the event mover has no such stamp off the player-room
 * branch (evt_move_object), so an event can put something in the player's
 * hands and leave it unreferenceable.  `i` on the next line lists all four,
 * and `x delta` immediately after it answers "A probe object." -- the
 * listing, and nothing else, is what let go of it.
 *
 * yak_shaving is the row that asked the question: its jar of pickled eggs is
 * object 0, InitialPosition hidden, and the Dada Lama's event hands it to the
 * player.  run400 answers `x eggs`, `open eggs` and `give eggs to acolyte`
 * with the not-here refusals for the whole game -- the player never once
 * refers to the jar by noun -- while `give*eggs*lama` and `give*eggs*yeti`,
 * task patterns that resolve no noun, fire normally.
 */
scr_bool
lib_cmd_inventory (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool wearing;
  lib_list_t list;

  /* Find and list each object worn by the player. */
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_WORN_PLAYER)
        {
          list.push_back (object);
          gs_set_object_seen (game, object, TRUE);
        }
    }
  wearing = !list.empty ();
  if (wearing)
    {
      lib_print_clause (game, FALSE,
                        "You are wearing ",
                        "I am wearing ",
                        "%player% is wearing ");
      lib_print_list (game, list, lib_print_object, " and ");
    }

  /* Find and list each object owned by the player. */
  list.clear ();
  for (object = 0; object < gs_object_count (game); object++)
    {
      if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
        {
          list.push_back (object);
          gs_set_object_seen (game, object, TRUE);
        }
    }
  if (!list.empty ())
    {
      if (wearing)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                            ", and you are carrying ",
                                            ", and I am carrying ",
                                            ", and %player_pronoun% is carrying "));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 "You are carrying ",
                                                 "I am carrying ",
                                                 "%player% is carrying "));
        }
      lib_print_list (game, list, lib_print_object, " and ");
      pf_buffer_character (filter, '.');

      /* Print contents of every container and surface carried. */
      for (object = 0; object < gs_object_count (game); object++)
        {
          if (gs_object_position (game, object) == OBJ_HELD_PLAYER)
            lib_list_in_on_object (game, object, TRUE);
        }
      pf_buffer_character (filter, '\n');
    }
  else
    {
      if (wearing)
        {
          pf_buffer_string (filter, ", and ");
          pf_buffer_string (filter,
                            lib_select_response (game,
                                            "you are carrying nothing.\n",
                                            "I am carrying nothing.\n",
                                            "%player% is carrying nothing.\n"));
        }
      else
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                            "You are carrying nothing.\n",
                                            "I am carrying nothing.\n",
                                            "%player% is carrying nothing.\n"));
        }
    }

  /* Successful command. */
  return TRUE;
}


/*
 * lib_list_in_object_pre_390()
 *
 * The listing the 3.8 open handler appends: whatisin1 (run380 @42998C) and
 * whatisin2 (@4297AC) each gather every object whose parent field names the
 * opened object -- in or on, they never look at which -- and print them as
 * "  Inside <the object> is a, b and c." from the one "  Inside " literal
 * the Runner has (see lib_list_in_object).  The object only has to be a
 * surface or container of some kind (field 29 nonzero) and not closed, so
 * an openable SURFACE lists this way too: Crime_Adventure.taf (3.80) keeps
 * a penny and a golf ball on a closed dresser (SurfaceContainer 2), and
 * run380 answers `open dresser` with "You open the dresser.  Inside the
 * dresser is a penny and a golf ball." (measured 2026-09-04) where the
 * container-only lister said nothing.  whatisin1, the held case, marks each
 * listed object seen; whatisin2, the static case, does not.  run370 is the
 * same pair (@42B78E).
 */
static scr_bool
lib_list_in_object_pre_390 (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  lib_list_t list;

  for (object = 0; object < gs_object_count (game); object++)
    {
      if ((gs_object_position (game, object) == OBJ_IN_OBJECT
           || gs_object_position (game, object) == OBJ_ON_OBJECT)
          && gs_object_parent (game, object) == container)
        {
          list.push_back (object);
          if (!obj_is_static (game, container))
            gs_set_object_seen (game, object, TRUE);
        }
    }
  if (list.empty ())
    return FALSE;

  pf_buffer_string (filter, "  Inside ");
  lib_print_object_np (game, container);
  pf_buffer_string (filter, " is ");
  lib_print_list (game, list, lib_print_object, " and ");
  pf_buffer_character (filter, '.');
  return TRUE;
}


/*
 * lib_cmd_open_object()
 *
 * Attempt to open the referenced object.
 */
scr_bool
lib_cmd_open_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, openness;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "open", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Get the current object openness. */
  openness = gs_object_openness (game, object);

  /* React to the request based on openness state. */
  switch (openness)
    {
    case OBJ_OPEN:
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      /* run400 has only " is already open!" (no " are " form). */
      pf_buffer_string (filter, " is already open!\n");
      return TRUE;

    case OBJ_CLOSED:
      /*
       * The 4.0 Runner only opens a dynamic object the player is holding
       * (or wearing, possibly nested in a carried container): its open
       * handler (Proc_19_3, loc_4757CA) allows the open when the object
       * is static Or Proc_21_46 (held-or-worn, recursive) passes, and
       * otherwise answers "<I am> not carrying <the object>!".  The 3.8
       * and 3.9 handlers have no such test.
       */
      if (lib_is_version_400 (game)
          && !obj_is_static (game, object)
          && !obj_indirectly_held_by_player (game, object))
        {
          lib_print_response_object (game,
                                     "You are not carrying ",
                                     "I am not carrying ",
                                     "%player% is not carrying ",
                                     object, "!\n");
          return TRUE;
        }

      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You open ",
                                             "I open ",
                                             "%player% open "));
      lib_print_object_np (game, object);
      pf_buffer_character (filter, '.');

      /*
       * Set open state, and list contents.  The 3.8 open handler
       * (openclose, run380 42F3A4) lists through whatisin1 (42998C: only a
       * dynamic container held directly by the player, position 0 -- not
       * worn, not lying in the room, not nested) and whatisin2 (4297AC:
       * only a static one present in the room); anything else gets the
       * bare "You open X."  Measured on jb2000.taf in run380, 2026-09-04:
       * `open bag` on a suitcase lying in the room prints just "You open
       * the brown suitcase.", while the same command after `take bag` adds
       * "  Inside the brown suitcase is a 9mm hand gun, a lazer watch and
       * a mind learner."  3.9 and 4.0 list regardless.
       */
      gs_set_object_openness (game, object, OBJ_OPEN);
      if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
        {
          if (obj_is_static (game, object)
              || gs_object_position (game, object) == OBJ_HELD_PLAYER)
            lib_list_in_object_pre_390 (game, object);
        }
      else
        lib_list_in_object (game, object, TRUE, FALSE);
      pf_buffer_character (filter, '\n');
      return TRUE;

    case OBJ_LOCKED:
      lib_print_response_object (game,
                                 "You can't open ",
                                 "I can't open ",
                                 "%player% can't open ",
                                 object, " as it is locked!\n");
      return TRUE;

    default:
      break;
    }

  /* The object isn't openable. */
  lib_print_response_object (game,
                             "You can't open ",
                             "I can't open ",
                             "%player% can't open ",
                             object, "!\n");
  return TRUE;
}


/*
 * lib_cmd_close_object()
 *
 * Attempt to close the referenced object.
 */
scr_bool
lib_cmd_close_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, openness;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "close", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Get the current object openness. */
  openness = gs_object_openness (game, object);

  /* React to the request based on openness state. */
  switch (openness)
    {
    case OBJ_OPEN:
      /* Same 4.0-only carrying gate as in lib_cmd_open_object above. */
      if (lib_is_version_400 (game)
          && !obj_is_static (game, object)
          && !obj_indirectly_held_by_player (game, object))
        {
          lib_print_response_object (game,
                                     "You are not carrying ",
                                     "I am not carrying ",
                                     "%player% is not carrying ",
                                     object, "!\n");
          return TRUE;
        }

      lib_print_response_object (game,
                                 "You close ",
                                 "I close ",
                                 "%player% close ",
                                 object, ".\n");

      /* Set closed state. */
      gs_set_object_openness (game, object, OBJ_CLOSED);
      return TRUE;

    case OBJ_CLOSED:
    case OBJ_LOCKED:
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      /* run400 has only " is already closed!" (no " are " form). */
      pf_buffer_string (filter, " is already closed!\n");
      return TRUE;

    default:
      break;
    }

  /*
   * The object isn't closeable.
   *
   * 4.0 has a dedicated message for this, ending in a bang (run400 475A31,
   * with "!" appended at 475A5F).  No earlier Runner does: openclose() gives
   * `open` a not-openable branch but gives `close` none at all (run380
   * 42F25C..42F322 tests only openness 6 and 5, run390 43A2xx the same), so a
   * present-but-not-closeable object falls out of openclose() with the
   * message still empty and is answered by the generic can't-do tail further
   * down -- which ends in a period (run370 43D231, run380 443D31, run390
   * 45D4BE/45D4CF).  Same sentence, different punctuation.
   *
   * Measured on p39EXAM.taf (3.90), Adrift_43_p39exam.txt:
   *   `open stone` -> "You can't open the stone!"
   *   `close stone` -> "You can't close the stone."
   * and on p4EXAM.taf (4.00), Adrift_1_p4exam.txt, where both end in "!".
   */
  lib_print_response_object (game,
                             "You can't close ",
                             "I can't close ",
                             "%player% can't close ",
                             object,
                             lib_is_version_400 (game) ? "!\n" : ".\n");
  return TRUE;
}


/*
 * lib_attempt_key_acquisition()
 *
 * Automatically get an object being used as a key, if possible.
 */
static void
lib_attempt_key_acquisition (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* Disallow getting static objects. */
  if (obj_is_static (game, object))
    return;

  /* If the object is not seen or available, reject the attempt. */
  if (!((gs_object_seen (game, object)
         || !lib_matcher_requires_seen (game))
        && obj_indirectly_in_room (game, object, gs_playerroom (game))))
    return;

  /*
   * Check if we already have it, or are wearing it, or if a NPC has or is
   * wearing it.
   */
  if (gs_object_position (game, object) == OBJ_HELD_PLAYER
      || gs_object_position (game, object) == OBJ_WORN_PLAYER
      || gs_object_position (game, object) == OBJ_HELD_NPC
      || gs_object_position (game, object) == OBJ_WORN_NPC)
    return;

  /*
   * If the object is contained in or on something we're already holding,
   * capacity checks are meaningless.
   */
  if (!obj_indirectly_held_by_player (game, object))
    {
      if (lib_object_too_heavy (game, object)
          || lib_object_too_large (game, object))
        return;
    }

  /* Retry game commands for the object with a standard "get". */
  if (lib_try_game_command_short (game, "get", object))
    return;

  /* Note what we're doing. */
  if (gs_object_position (game, object) == OBJ_IN_OBJECT
      || gs_object_position (game, object) == OBJ_ON_OBJECT)
    {
      pf_buffer_string (filter, "(Taking ");
      lib_print_object_np (game, object);

      pf_buffer_string (filter, " from ");
      lib_print_object_np (game, gs_object_parent (game, object));
      pf_buffer_string (filter, " first)\n");
    }
  else
    {
      lib_print_wrapped_object (game, "(Picking up ", object, " first)\n");
    }

  /* Take possession of the object.  The implicit take runs the Runner's
   * own `takes`, so it spends OnlyWhenNotMoved mode 1 too. */
  gs_object_player_get (game, object);
  gs_set_object_unmoved (game, object, FALSE);
}


/*
 * The verb-specific half of lock and unlock.  The two commands run the same
 * steps -- disambiguate the object, check its openness, look up the key it
 * takes, check the player holds that key, then flip the state -- and differ
 * only in the openness they act on, the openness they leave behind, and the
 * words they print.
 */
typedef struct
{
  scr_int required_openness;      /* Openness the verb acts on */
  scr_int new_openness;           /* Openness it leaves behind */
  const scr_char *verb;           /* "lock", for disambiguation */
  const scr_char *verb_with;      /* "lock that with", ditto */
  const scr_char *prompt;         /* Asked when no key was referenced */
  const scr_char *nothing_to;     /* " anything to lock " */
  const scr_char *wrong_state[2]; /* Singular and plural state refusal */
  const scr_char *cant[3];        /* "You can't lock ", and so on */
  const scr_char *does[3];        /* "You lock ", and so on */
} lib_lock_verb_t;

static const lib_lock_verb_t LIB_UNLOCK_VERB = {
  OBJ_LOCKED, OBJ_CLOSED,
  "unlock", "unlock that with",
  "What do you want to unlock that with?\n",
  " anything to unlock ",
  {" is not locked!\n", " are not locked!\n"},
  {"You can't unlock ", "I can't unlock ", "%player% can't unlock "},
  {"You unlock ", "I unlock ", "%player% unlock "}
};

static const lib_lock_verb_t LIB_LOCK_VERB = {
  OBJ_CLOSED, OBJ_LOCKED,
  "lock", "lock that with",
  "What do you want to lock that with?\n",
  " anything to lock ",
  {" is already locked!\n", " are already locked!\n"},
  {"You can't lock ", "I can't lock ", "%player% can't lock "},
  {"You lock ", "I lock ", "%player% lock "}
};

/* What lib_lock_check_openness() made of the object's current state. */
enum {
  LIB_LOCK_PROCEED, LIB_LOCK_REFUSED, LIB_LOCK_NOT_LOCKABLE
};


/*
 * lib_lock_check_openness()
 *
 * Decide whether the object is in a state this verb can work on, printing
 * the refusal itself if it is not.  Locking something that stands open is
 * refused in its own terms; every other openness the verb doesn't act on
 * gets the "is not locked"/"is already locked" complaint.  Anything with no
 * openness at all isn't lockable, and the caller says so.
 */
static scr_int
lib_lock_check_openness (scr_gameref_t game, scr_int object,
                         const lib_lock_verb_t *verb)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int openness;

  openness = gs_object_openness (game, object);
  if (openness == verb->required_openness)
    return LIB_LOCK_PROCEED;

  if (verb->new_openness == OBJ_LOCKED && openness == OBJ_OPEN)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             verb->cant[0],
                                             verb->cant[1],
                                             verb->cant[2]));
      lib_print_object_np (game, object);
      pf_buffer_string (filter, " as it is open.\n");
      return LIB_LOCK_REFUSED;
    }

  if (openness == OBJ_OPEN || openness == OBJ_CLOSED || openness == OBJ_LOCKED)
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object,
                                              verb->wrong_state[0],
                                              verb->wrong_state[1]));
      return LIB_LOCK_REFUSED;
    }

  return LIB_LOCK_NOT_LOCKABLE;
}


/*
 * lib_lock_backend()
 *
 * Attempt to lock or unlock the referenced object.  With with_key set the
 * key comes from the player's own referenced text and has to be the right
 * one; otherwise the object's key is looked up and the player tries to lay
 * hands on it first.
 */
static scr_bool
lib_lock_backend (scr_gameref_t game, const lib_lock_verb_t *verb,
                  scr_bool with_key)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, key = -1;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, verb->verb, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /*
   * Now try to get the key from referenced text, and disambiguate as usual.
   */
  if (with_key)
    {
      const scr_var_setref_t vars = gs_get_vars (game);

      if (!uip_match ("%object%", var_get_ref_text (vars), game))
        {
          pf_buffer_string (filter, verb->prompt);
          return TRUE;
        }
      key = lib_disambiguate_object (game, verb->verb_with, NULL);
      if (key == -1)
        return TRUE;
    }

  /* React to the request based on openness state. */
  switch (lib_lock_check_openness (game, object, verb))
    {
    case LIB_LOCK_REFUSED:
      return TRUE;

    case LIB_LOCK_PROCEED:
      {
        scr_int key_index, the_key;

        key_index = prop_get_indexed_integer (bundle, "Objects", object,
                                              "Key");
        if (key_index == -1)
          break;

        the_key = obj_dynamic_object (game, key_index);
        if (with_key)
          {
            if (the_key != key)
              {
                pf_buffer_string (filter,
                                  lib_select_response (game,
                                                       verb->cant[0],
                                                       verb->cant[1],
                                                       verb->cant[2]));
                lib_print_object_np (game, object);
                lib_print_wrapped_object (game, " with ", key, ".\n");
                return TRUE;
              }
          }
        else
          {
            key = the_key;
            lib_attempt_key_acquisition (game, key);
          }

        /*
         * The runner asks whether the key is indirectly held by the player,
         * not whether it sits in the hands: a key that is worn, or stowed in
         * an open bag being carried, unlocks perfectly well.  Provenance
         * relies on this -- its walkthrough wears the brass key so that the
         * cave's forced "drop all" can't take it away, then unlocks the
         * wooden chest while still only wearing it.
         */
        if (!obj_indirectly_held_by_player (game, key))
          {
            if (with_key)
              {
                lib_print_response_object (game,
                                           "You are not holding ",
                                           "I am not holding ",
                                           "%player% is not holding ",
                                           key, ".\n");
              }
            else
              {
                pf_buffer_string (filter,
                                  lib_select_response (game,
                                                       "You don't have",
                                                       "I don't have",
                                                       "%player% don't have"));
                pf_buffer_string (filter, verb->nothing_to);
                lib_print_object_np (game, object);
                pf_buffer_string (filter, " with!\n");
              }
            return TRUE;
          }

        gs_set_object_openness (game, object, verb->new_openness);
        pf_buffer_string (filter,
                          lib_select_response (game,
                                               verb->does[0],
                                               verb->does[1],
                                               verb->does[2]));
        lib_print_object_np (game, object);
        lib_print_wrapped_object (game, " with ", key, ".\n");
        return TRUE;
      }

    default:
      break;
    }

  /* The object isn't lockable. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         verb->cant[0],
                                         verb->cant[1],
                                         verb->cant[2]));
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}


/*
 * lib_cmd_unlock_object_with()
 * lib_cmd_unlock_object()
 * lib_cmd_lock_object_with()
 * lib_cmd_lock_object()
 *
 * Attempt to lock or unlock the referenced object, either with the key the
 * player named or with one selected automatically.
 */
scr_bool
lib_cmd_unlock_object_with (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_UNLOCK_VERB, TRUE);
}

scr_bool
lib_cmd_unlock_object (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_UNLOCK_VERB, FALSE);
}

scr_bool
lib_cmd_lock_object_with (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_LOCK_VERB, TRUE);
}

scr_bool
lib_cmd_lock_object (scr_gameref_t game)
{
  return lib_lock_backend (game, &LIB_LOCK_VERB, FALSE);
}


/*
 * lib_compare_subject()
 *
 * Compare a subject, comma or NUL terminated.  Helper for ask.
 */
static scr_bool
lib_compare_subject (const scr_char *subject, scr_int posn,
                     const scr_char *string)
{
  scr_int word_posn, string_posn;

  /* Skip any leading subject spaces. */
  for (word_posn = posn;
       subject[word_posn] != NUL && scr_isspace (subject[word_posn]);)
    word_posn++;
  for (string_posn = 0;
       string[string_posn] != NUL && scr_isspace (string[string_posn]);)
    string_posn++;

  /* Match characters from words with the string at position. */
  while (TRUE)
    {
      /* Any character mismatch means no match. */
      if (scr_tolower (subject[word_posn]) != scr_tolower (string[string_posn]))
        return FALSE;

      /* Move to next character in each. */
      word_posn++;
      string_posn++;

      /*
       * If at space, advance over whitespace in subjects list.  Stop when we
       * hit the end of the element or list.
       */
      while (scr_isspace (subject[word_posn])
             && subject[word_posn] != COMMA && subject[word_posn] != NUL)
        word_posn++;

      /* Advance over whitespace in the current string too. */
      while (scr_isspace (string[string_posn]) && string[string_posn] != NUL)
        string_posn++;

      /*
       * If we found the end of the subject, and the end of the current string,
       * we've matched.  If not at the end of the current string, though, only
       * a partial match.
       */
      if (subject[word_posn] == NUL || subject[word_posn] == COMMA)
        {
          if (string[string_posn] == NUL)
            break;
          else
            return FALSE;
        }
    }

  /* Matched in the loop; return TRUE. */
  return TRUE;
}


/*
 * lib_npc_reply_to()
 *
 * Reply for an NPC on a given topic.  Helper for ask.
 */
static scr_bool
lib_npc_reply_to (scr_gameref_t game, scr_int npc, scr_int topic)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int task;
  const scr_char *response;

  /* Find any associated task to control response. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Topics";
  vt_key[3].integer = topic;
  vt_key[4].string = "Task";
  task = prop_get_integer (bundle, "I<-sisis", vt_key);

  /* Get the response, and print if anything there. */
  if (task > 0 && gs_task_done (game, task - 1))
    vt_key[4].string = "AltReply";
  else
    vt_key[4].string = "Reply";
  response = prop_get_string (bundle, "S<-sisis", vt_key);
  if (!scr_strempty (response))
    {
      pf_buffer_string (filter, response);
      pf_buffer_character (filter, '\n');
      return TRUE;
    }

  /* No response to this combination. */
  return FALSE;
}


static const scr_char *lib_ask_format_subject (scr_gameref_t game);

/*
 * lib_ask_npc_about()
 * lib_cmd_ask_npc_about()
 * lib_cmd_talk_to_npc_about()
 *
 * Converse with NPC.
 *
 * `talk to X about Y` enters the same branch as `ask X about Y` in every
 * Runner -- its guard is `c("ask") Or c("talk to")` (run370 loc_4387F4,
 * run380 loc_440683, run390 loc_4597F2, run400 loc_47F8F7) -- but it differs
 * in what happens when no topic matches.  The ask-format hint branch runs
 * just before it and has already claimed the response line for anything
 * containing `talk to` (see lib_cmd_talk_to_npc), and the no-topic reply is
 * written only over an empty one (run380 loc_4409E8).  A matching topic does
 * overwrite it (loc_440918), so the topic still wins where there is one.
 */
static scr_bool
lib_ask_npc_about (scr_gameref_t game, const scr_char *verb,
                   scr_bool hint_when_silent)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[5];
  scr_int npc, topic_count, topic, topic_match, default_topic;
  scr_bool found, default_found, is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, verb, &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  if (lib_trace)
    scr_trace ("Library: asking NPC %ld\n", npc);

  /* Get the topics the NPC converses about. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Topics";
  topic_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  topic_match = default_topic = -1;
  found = default_found = FALSE;
  for (topic = 0; topic < topic_count; topic++)
    {
      const scr_char *subjects;
      scr_int posn;

      /* Get subject list for this topic. */
      vt_key[3].integer = topic;
      vt_key[4].string = "Subject";
      subjects = prop_get_string (bundle, "S<-sisis", vt_key);

      /* If this is the special "*" topic, note and continue. */
      if (!scr_strcasecmp (subjects, "*"))
        {
          if (lib_trace)
            scr_trace ("Library: \"*\" is %ld\n", topic);

          default_topic = topic;
          default_found = TRUE;
          continue;
        }

      /* Split into subjects by comma delimiter. */
      for (posn = 0; subjects[posn] != NUL;)
        {
          if (lib_trace)
            scr_trace ("Library: subject %s[%ld]\n", subjects, posn);

          /* See if this subject matches. */
          if (lib_compare_subject (subjects, posn, var_get_ref_text (vars)))
            {
              if (lib_trace)
                scr_trace ("Library: matched\n");

              topic_match = topic;
              found = TRUE;
              break;
            }

          /* Move to next subject, or end of list. */
          while (subjects[posn] != COMMA && subjects[posn] != NUL)
            posn++;
          if (subjects[posn] == COMMA)
            posn++;
        }
    }

  /* Handle any matched subject first, and "*" second. */
  if (found && lib_npc_reply_to (game, npc, topic_match))
    return TRUE;
  else if (default_found && lib_npc_reply_to (game, npc, default_topic))
    return TRUE;

  /* No topic matched, so `talk to` falls back on the hint it displaced. */
  if (hint_when_silent)
    {
      lib_print_wrapped_npc (game, "Use the format \"ask ",
                             npc, lib_ask_format_subject (game));
      return TRUE;
    }

  /* NPC has no response. */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                " does not respond to your question.\n",
                                " does not respond to my question.\n",
                                " does not respond to %player%'s question.\n"));
  return TRUE;
}

scr_bool
lib_cmd_ask_npc_about (scr_gameref_t game)
{
  return lib_ask_npc_about (game, "ask", FALSE);
}

scr_bool
lib_cmd_talk_to_npc_about (scr_gameref_t game)
{
  return lib_ask_npc_about (game, "talk to", TRUE);
}


/*
 * lib_check_put_in_recursion()
 *
 * Checks for infinite recursion when placing an object in an object.  Returns
 * TRUE if no recursion detected.
 */
static scr_bool
lib_check_put_in_recursion (scr_gameref_t game,
                            scr_int object, scr_int container, scr_bool report)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int check;

  /* Avoid the obvious possibility of infinite recursion. */
  if (container == object)
    {
      if (report)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                "You can't put an object inside itself!",
                                "I can't put an object inside itself!",
                                "%player% can't put an object inside itself!"));
        }
      return FALSE;
    }

  /* Avoid the subtle possibility of infinite recursion. */
  check = container;
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
                                    "You can't put an object inside one",
                                    "I can't put an object inside one",
                                    "%player% can't put an object inside one"));
              pf_buffer_string (filter, " it's on or in!");
            }
          return FALSE;
        }
    }

  /* No infinite recursion detected. */
  return TRUE;
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
 * "all", by contrast, is held-only in every version: the candidate loop
 * takes objects with field26 = 0 And field24 = 0, i.e. lying in the player's
 * hands, never worn and never inside anything.  That narrower set is what
 * "put all in X" and "put all except Y on Z" range over, and it is also what
 * the "You are not holding ..." leftover report is phrased for.
 */
static scr_bool
lib_put_named_filter (scr_gameref_t game, scr_int object)
{
  /*
   * A static is named too, at 4.0.  There is no static test at name time --
   * the one that turns the piece away lives in `insides` (@465ED7) and is
   * silent -- so the line is claimed and the take piece runs on it first.
   * Probe PSTAT (Adrift_941/942_pstat.txt): `put anvil in box`, the anvil a
   * static lying in the room, answers "(Taking the anvil first)" / "You
   * can't take the anvil!" where scarier used to fall through to the game's
   * DontUnderstand.  Pre-4.0 handlers never see a static.
   */
  if (obj_is_static (game, object))
    return lib_is_version_400 (game)
           && obj_indirectly_in_room (game, object, gs_playerroom (game));

  if (lib_is_version_400 (game))
    return obj_indirectly_in_room (game, object, gs_playerroom (game));

  return gs_object_position (game, object) == OBJ_HELD_PLAYER
         || gs_object_position (game, object) == OBJ_WORN_PLAYER;
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
 *   easter      Adrift_273:135  `put egg in basket` -> "You put the creme
 *               egg inside the Easter basket.", with the game's other eggs
 *               standing in the room
 *   helsing     Adrift_181:50   `put beads on dance floor` runs the game's
 *               own task past a namesake "beads"
 *   provenance  Adrift_342:1781 `put wood on stump` -> "You place the piece
 *               of wood on the stump.", picking it over the cord of wood
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
  return !obj_is_static (game, object)
         && gs_object_position (game, object) == OBJ_HELD_PLAYER
         && object != associate;
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
    {
      game->object_references[object] = FALSE;
      game->multiple_references[object] = TRUE;
      return FALSE;
    }

  /*
   * The take is also skipped, silently, when the TYPED line pre-matches a
   * task (run400 name_object Proc_19_41_46E5D8 @46E2C7: Proc_19_35_453C50
   * on MemVar_49428C, the line as the synonym table left it, with the
   * task's restrictions passing or failing on a restriction that has a
   * message -- House's task 60 "* %object%", restricted to the spinning
   * house with no message, is NOT a hit, and run400 goes on to "(Taking the
   * wood first)"; Adrift_91.txt); the object then reaches the put handler
   * unheld and draws its "not holding" refusal, or -- if the handler's own
   * canonical look-up claims -- the task.  The task the player spelled out
   * is what the refusal hands the line to afterwards (see
   * run_all_commands()).
   */
  if (lib_task_prematches_input (game, 1))
    {
      game->object_references[object] = FALSE;
      game->multiple_references[object] = TRUE;
      return FALSE;
    }

  pf_buffer_string (filter, "(Taking ");
  lib_print_object_np (game, object);
  pf_buffer_string (filter, " first)\n");

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
      game->object_references[object] = FALSE;
      game->multiple_references[object] = TRUE;
      *printed = TRUE;
      return FALSE;
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

  game->object_references[object] = FALSE;
  game->multiple_references[object] = TRUE;
  *printed = TRUE;
  return FALSE;
}


/*
 * lib_put_outcome_t
 * lib_output_length()
 * lib_put_drop_statics_400()
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
typedef struct
{
  scr_bool is_refusal_only;
  scr_bool is_silent;
  scr_bool is_tasks_only;
} lib_put_outcome_t;

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
 * hold -- measured on probe PSTAT in run400, Adrift_941/942_pstat.txt:
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

  for (object = 0; object < gs_object_count (game); object++)
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
 * lib_put_in_backend()
 *
 * Common backend handler for placing objects in containers.  Places all
 * objects currently referenced in the game into a container, trying game
 * commands first, and then moving other unhandled objects into the container.
 *
 * Objects to action are flagged in object_references; objects requested but
 * deemed not actionable are flagged in multiple_references.
 */
static lib_put_outcome_t
lib_put_in_backend (scr_gameref_t game, scr_int container)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object_count, object, count, capacity, free_space;
  scr_int length_before, length_after_tasks;
  scr_bool has_printed, is_refusal_only, task_claimed;
  scr_bool static_refused, recursion_rejected, has_moved;
  lib_put_outcome_t outcome;
  lib_list_t list, pending;

  /*
   * Try game commands for all referenced objects first.  If any succeed,
   * remove that reference from the list.  At the same time, check for and
   * weed out any moves that result in infinite recursion.
   */
  length_before = lib_output_length (game);
  has_printed = FALSE;
  task_claimed = FALSE;
  static_refused = recursion_rejected = has_moved = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /*
       * Reject and remove attempts to place objects in themselves.  This
       * guard is ours, not name_object's -- run400 announces "(Taking the
       * box first)" for `put box in box` and then says nothing whatever
       * (probe PSTAT commands 13 and 18) -- so a line it rejects never
       * reaches the report below either.
       */
      if (!lib_check_put_in_recursion (game, object, container, !has_printed))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
          continue;
        }

      /*
       * Version 4.0 picks up an object it has been asked to put down, and it
       * does so BEFORE the handler's task look-up.  run400's name_object
       * loop runs the take piece at loc_46E2B5 and only then hands the pair
       * to insides (Proc_19_43_46639C @loc_46E34F), which is where the
       * canonical line reaches the tasks -- so a task that goes on to claim
       * the put still carries the announcement ahead of its own text.
       * Measured on frustrated turns 53-55 (Adrift_274_frustrated.txt):
       * `put small rock on left pan` matches task 511 `put*small*left*` and
       * still opens "(Taking the small rock first)".
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
       * put, joined after the refusal (PUT4, Adrift_81.txt).  It runs even
       * for an object the take above could not acquire: run400 reaches
       * insides' tasks() call at loc_465EB5 before the possession test at
       * loc_465EED, so a claim takes the object back out of the "You are
       * not holding ..." report.  The look-up itself is deferred to the
       * second pass below, so that the whole take phase precedes it.
       */
      if (lib_is_version_400 (game))
        {
          pending.push_back (object);
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

  /*
   * name_object's own close to the take phase, and then the look-ups it
   * deferred.  run400 takes every named piece first and hands the pair to
   * `insides` only afterwards (@46E34F), so the report above comes out
   * ahead of the first task: probe PSTAT command 12, `put slab in box`
   * with the slab a static and the inventory empty, reads "(Taking the
   * slab first)" / "You can't take the slab!  You are carrying nothing!
   * SLABTASK."
   */
  if (!recursion_rejected && lib_put_nothing_carried_400 (game, has_printed))
    has_printed = TRUE;

  for (const scr_int pending_object : pending)
    {
      if (lib_try_game_command_with_object_400 (game, "put", pending_object,
                                                "in", container))
        {
          game->object_references[pending_object] = FALSE;
          game->multiple_references[pending_object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
        }
    }
  length_after_tasks = lib_output_length (game);

  /*
   * Retrieve the container's total volume, and the volume it has left.  The
   * free space is tracked across the loop below rather than recomputed, since
   * each object put in spends some of it.
   */
  capacity = obj_get_container_capacity (game, container);
  free_space = obj_get_container_free_space (game, container);

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
      lib_print_list (game, list, lib_print_object_np, " and ");
      pf_buffer_string (filter, " inside ");
      lib_print_object_np (game, container);
      pf_buffer_character (filter, '.');
    }
  has_printed |= !list.empty ();
  has_moved = !list.empty ();

  /*
   * Version 3.8 has one container refusal and one only.  run380 answers "The
   * box is full." to everything it will not take in -- whether the container
   * has no room left, or (with a Capacity of 0) never had any, and whatever
   * the object's Size/weight class, which it does not charge against the
   * container at all (measured 2026-08-03; see obj_get_container_capacity).
   * Consume the leftovers here, so that neither 4.0 report below finds
   * anything to say about them.
   */
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

  /*
   * A put that moved nothing and said nothing so far is about to say only
   * the size and capacity refusals below; see the return value.
   */
  is_refusal_only = !has_printed;

  /*
   * Report objects not put in because of their size.  These objects remain in
   * standard references, as do objects rejected because of capacity limits.
   * By removing too large objects in this loop, we're left later on with just
   * the objects rejected by capacity limits.
   */
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

  /*
   * Report objects not put in because the container is too full.  This should
   * be all remaining objects in standard references.
   */
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

  /* Refusals only if they were printed, and nothing before them. */
  is_refusal_only = is_refusal_only && has_printed;

  /* Note any remaining multiple references left out of the operation. */
  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object])
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }

  lib_print_object_list (game, has_printed, list, " or ", '.',
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
  outcome.is_refusal_only = (is_refusal_only && list.empty ())
                            || (static_refused && !has_moved);
  outcome.is_silent = lib_output_length (game) == length_before;
  outcome.is_tasks_only = task_claimed
                          && lib_output_length (game) == length_after_tasks;
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
 * (arena probe PUT7, Adrift_87, 2026-09-05; Zack Smackfoot `put knife in
 * slot`, Adrift_57).  Signal that to run_priority_commands() and report the
 * handler's return.  Pre-4.0 the refusal claims the line, as it always has.
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
 * lib_put_in_finish()
 * lib_put_on_finish()
 *
 * End the put's line and settle its claim from the backend's outcome; see
 * lib_put_outcome_t.  A 4.0 put that printed nothing (statics only, see
 * lib_put_drop_statics_400()) is left unclaimed with no line ending, for
 * the task pass to answer.
 */
static scr_bool
lib_put_in_finish (scr_gameref_t game, const lib_put_outcome_t &outcome)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (outcome.is_silent && lib_is_version_400 (game))
    return FALSE;
  if (!outcome.is_tasks_only)
    pf_buffer_character (filter, '\n');
  return lib_put_in_refused (game, outcome.is_refusal_only);
}

static scr_bool
lib_put_on_finish (scr_gameref_t game, const lib_put_outcome_t &outcome)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (outcome.is_silent && lib_is_version_400 (game))
    return FALSE;
  if (!outcome.is_tasks_only)
    pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_put_in_filter()
 * lib_put_in_not_container_filter()
 *
 * Helper functions for deciding if an object may be put in another this
 * context.  Returns TRUE if an object may be manipulated, FALSE otherwise.
 * The named form is much the wider of the two; see lib_put_named_filter().
 */
static scr_bool
lib_put_in_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_named_filter (game, object);
}

/* The resolver twin of the above; see lib_put_resolve_filter(). */
static scr_bool
lib_put_in_resolve_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_resolve_filter (game, object);
}

static scr_bool
lib_put_in_not_container_filter (scr_gameref_t game,
                                 scr_int object, scr_int container)
{
  return lib_put_all_filter (game, object, container);
}


/*
 * lib_put_in_is_valid()
 *
 * Validate the container requested in "put in" commands.
 */
static scr_bool
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
      if (run_in_priority_pass ())
        {
          run_priority_defer ();
          return FALSE;
        }
      lib_print_response_object (game,
                                 "You can't put anything inside ",
                                 "I can't put anything inside ",
                                 "%player% can't put anything inside ",
                                 container, "!\n");
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
  if (obj_uses_burden_model (game)
      && !obj_is_static (game, container)
      && gs_object_position (game, container) != OBJ_HELD_PLAYER)
    {
      if (run_in_priority_pass ())
        {
          run_priority_defer ();
          return FALSE;
        }
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not holding ",
                                             "I am not holding ",
                                             "%player% is not holding "));
      lib_print_object (game, container);
      pf_buffer_string (filter, ".\n");
      return FALSE;
    }

  /* If the container is closed, reject now. */
  if (gs_object_openness (game, container) > OBJ_OPEN)
    {
      if (run_in_priority_pass ())
        {
          run_priority_defer ();
          return FALSE;
        }
      pf_new_sentence (filter);
      lib_print_object_np (game, container);
      pf_buffer_string (filter,
                        lib_select_plurality (game, container, " is", " are"));
      if (gs_object_openness (game, container) == OBJ_LOCKED)
        pf_buffer_string (filter, " locked!\n");
      else
        pf_buffer_string (filter, " closed!\n");
      return FALSE;
    }

  /* Container is a valid target for "put in". */
  return TRUE;
}


/*
 * lib_cmd_put_all_in()
 *
 * Put all objects currently held by the player into a container.
 */
scr_bool
lib_cmd_put_all_in (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int container, objects;
  scr_bool is_ambiguous;
  lib_put_outcome_t outcome;

  /* Get the referenced object, and if none, consider complete. */
  container = lib_disambiguate_object (game, "put that into", &is_ambiguous);
  if (container == -1)
    return is_ambiguous;

  /* Validate the container object to take from (deferred -> unhandled). */
  if (!lib_put_in_is_valid (game, container))
    return !run_in_priority_pass ();

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_put_in_not_container_filter,
                              container, FALSE, NULL);
  gs_clear_multiple_references (game);
  outcome = {};
  if (objects > 0)
    outcome = lib_put_in_backend (game, container);
  else
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                           "You're not carrying anything",
                                           "I'm not carrying anything",
                                           "%player%'s not carrying anything"));
      if (obj_indirectly_held_by_player (game, container))
        pf_buffer_string (filter, " else");
      pf_buffer_character (filter, '.');
    }

  return lib_put_in_finish (game, outcome);
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

  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (!obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        continue;
      if (lib_verb_object_name_score (game, index_, fragment.c_str ()) > 0)
        return FALSE;
    }

  return TRUE;
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
  scr_int container, objects, references;
  scr_bool is_ambiguous;
  lib_put_outcome_t outcome;

  /* Get the referenced object, and if none, consider complete. */
  container = lib_disambiguate_object (game, "put that into", &is_ambiguous);
  if (container == -1)
    return is_ambiguous;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "move",
                                   is_except ? lib_put_in_not_container_filter
                                             : lib_put_in_resolve_filter,
                                   is_except ? container : -1, &references))
    {
      /*
       * A 4.0 "put X in Y" whose X names nothing is run400's 46E142, the one
       * exit of name_object that leaves the command line rewritten to the
       * fragment "put X " -- see run_priority_unnamed_put_object().  The
       * container has to be a real container for the Runner to have got that
       * far: 46DE19 turns anything else away with "can't put anything inside
       * X!", along the exit that puts the line back.  The except form takes
       * the "all" branch at 46E04E and never reaches the clobber.
       */
      if (!is_except
          && lib_is_version_400 (game)
          && run_in_priority_pass ()
          && obj_is_container (game, container)
          && lib_put_fragment_names_nothing (game))
        run_priority_unnamed_put_object ();
      return FALSE;
    }
  else if (references == 0)
    return TRUE;

  /* Validate the container object to put into (deferred -> unhandled). */
  if (!lib_put_in_is_valid (game, container))
    return !run_in_priority_pass ();

  /* As a special case, complain about requests to retain the container. */
  if (is_except
      && lib_multiple_retains_associate (game, container, "retain"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              is_except ? lib_put_in_not_container_filter
                                        : lib_put_in_filter,
                              is_except ? container : -1, is_except,
                              &references);
  outcome = {};
  if (objects > 0 || references > 0)
    outcome = lib_put_in_backend (game, container);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");

  return lib_put_in_finish (game, outcome);
}


/*
 * lib_cmd_put_in_except_multiple()
 * lib_cmd_put_in_multiple()
 *
 * Facets of lib_put_in_multiple_common().
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
 * lib_check_put_on_recursion()
 *
 * Checks for infinite recursion when placing an object on an object.  Returns
 * TRUE if no recursion detected.
 */
static scr_bool
lib_check_put_on_recursion (scr_gameref_t game,
                            scr_int object, scr_int supporter, scr_bool report)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int check;

  /* Avoid the obvious possibility of infinite recursion. */
  if (supporter == object)
    {
      if (report)
        {
          pf_buffer_string (filter,
                            lib_select_response (game,
                                  "You can't put an object onto itself!",
                                  "I can't put an object onto itself!",
                                  "%player% can't put an object onto itself!"));
        }
      return FALSE;
    }

  /* Avoid the subtle possibility of infinite recursion. */
  check = supporter;
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
                                      "You can't put an object onto one",
                                      "I can't put an object onto one",
                                      "%player% can't put an object onto one"));
              pf_buffer_string (filter, " it's on or in!");
            }
          return FALSE;
        }
    }

  /* No infinite recursion detected. */
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
static lib_put_outcome_t
lib_put_on_backend (scr_gameref_t game, scr_int supporter)
{
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
  has_printed = FALSE;
  task_claimed = FALSE;
  recursion_rejected = FALSE;
  object_count = gs_object_count (game);
  for (object = 0; object < object_count; object++)
    {
      if (!game->object_references[object])
        continue;

      /* Reject and remove attempts to place objects on themselves; the
       * guard is ours, not name_object's (see lib_put_in_backend). */
      if (!lib_check_put_on_recursion (game, object, supporter, !has_printed))
        {
          game->object_references[object] = FALSE;
          has_printed = TRUE;
          recursion_rejected = TRUE;
          continue;
        }

      /*
       * Version 4.0 picks up an object it has been asked to put down, and it
       * does so BEFORE the handler's task look-up.  run400's name_object
       * loop runs the take piece at loc_46E2B5 and only then hands the pair
       * to insides (Proc_19_43_46639C @loc_46E34F), which is where the
       * canonical line reaches the tasks -- so a task that goes on to claim
       * the put still carries the announcement ahead of its own text.
       * Measured on frustrated turns 53-55 (Adrift_274_frustrated.txt):
       * `put small rock on left pan` matches task 511 `put*small*left*` and
       * still opens "(Taking the small rock first)".
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
   * name_object's own close to the take phase, and then the look-ups it
   * deferred.  run400 takes every named piece first and hands the pair to
   * `insides` only afterwards (@46E34F), so the report above comes out
   * ahead of the first task: probe PSTAT command 12, `put slab in box`
   * with the slab a static and the inventory empty, reads "(Taking the
   * slab first)" / "You can't take the slab!  You are carrying nothing!
   * SLABTASK."
   */
  if (!recursion_rejected && lib_put_nothing_carried_400 (game, has_printed))
    has_printed = TRUE;

  for (const scr_int pending_object : pending)
    {
      if (lib_try_game_command_with_object_400 (game, "put", pending_object,
                                                "on", supporter))
        {
          game->object_references[pending_object] = FALSE;
          game->multiple_references[pending_object] = FALSE;
          has_printed = TRUE;
          task_claimed = TRUE;
        }
    }
  length_after_tasks = lib_output_length (game);

  lib_move_backend (game, &LIB_PUT_ON_VERB, supporter, has_printed);

  outcome.is_refusal_only = FALSE;
  outcome.is_silent = lib_output_length (game) == length_before;
  outcome.is_tasks_only = task_claimed
                          && lib_output_length (game) == length_after_tasks;
  return outcome;
}


/*
 * lib_put_on_filter()
 * lib_put_on_not_supporter_filter()
 *
 * Helper functions for deciding if an object may be put on another this
 * context.  Returns TRUE if an object may be manipulated, FALSE otherwise.
 * The named form is much the wider of the two; see lib_put_named_filter().
 */
static scr_bool
lib_put_on_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_named_filter (game, object);
}

/* The resolver twin of the above; see lib_put_resolve_filter(). */
static scr_bool
lib_put_on_resolve_filter (scr_gameref_t game, scr_int object, scr_int unused)
{
  assert (unused == -1);

  return lib_put_resolve_filter (game, object);
}

static scr_bool
lib_put_on_not_supporter_filter (scr_gameref_t game,
                                 scr_int object, scr_int supporter)
{
  return lib_put_all_filter (game, object, supporter);
}


/*
 * lib_put_on_is_valid()
 *
 * Validate the supporter requested in "put on" commands.
 */
static scr_bool
lib_put_on_is_valid (scr_gameref_t game, scr_int supporter)
{
  /* Verify that the supporter object is a supporter. */
  if (!obj_is_surface (game, supporter))
    {
      /* Deferred in the tentative priority pass; see lib_put_in_is_valid. */
      if (run_in_priority_pass ())
        {
          run_priority_defer ();
          return FALSE;
        }
      lib_print_response_object (game,
                                 "You can't put anything on ",
                                 "I can't put anything on ",
                                 "%player% can't put anything on ",
                                 supporter, "!\n");
      return FALSE;
    }

  /* Surface is a valid target for "put on". */
  return TRUE;
}


/*
 * lib_cmd_put_all_on()
 *
 * Put all objects currently held by the player onto a supporter.
 */
scr_bool
lib_cmd_put_all_on (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int supporter, objects;
  scr_bool is_ambiguous;
  lib_put_outcome_t outcome;

  /* Get the referenced object, and if none, consider complete. */
  supporter = lib_disambiguate_object (game, "put that onto", &is_ambiguous);
  if (supporter == -1)
    return is_ambiguous;

  /* Validate the supporter object to take from. */
  if (!lib_put_on_is_valid (game, supporter))
    return !run_in_priority_pass ();

  /* Filter objects into references, then handle with the backend. */
  gs_set_multiple_references (game);
  objects = lib_apply_filter (game,
                              lib_put_on_not_supporter_filter,
                              supporter, FALSE, NULL);
  gs_clear_multiple_references (game);
  outcome = {};
  if (objects > 0)
    outcome = lib_put_on_backend (game, supporter);
  else
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                           "You're not carrying anything",
                                           "I'm not carrying anything",
                                           "%player%'s not carrying anything"));
      if (obj_indirectly_held_by_player (game, supporter))
        pf_buffer_string (filter, " else");
      pf_buffer_character (filter, '.');
    }

  return lib_put_on_finish (game, outcome);
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
  scr_int supporter, objects, references;
  scr_bool is_ambiguous;
  lib_put_outcome_t outcome;

  /* Get the referenced object, and if none, consider complete. */
  supporter = lib_disambiguate_object (game, "put that onto", &is_ambiguous);
  if (supporter == -1)
    return is_ambiguous;

  /* Parse the multiple objects list to find the target objects. */
  if (!lib_parse_multiple_objects (game, is_except ? "retain" : "move",
                                   is_except ? lib_put_on_not_supporter_filter
                                             : lib_put_on_resolve_filter,
                                   is_except ? supporter : -1, &references))
    return FALSE;
  else if (references == 0)
    return TRUE;

  /* Validate the supporter object to put into. */
  if (!lib_put_on_is_valid (game, supporter))
    return !run_in_priority_pass ();

  /* As a special case, complain about requests to retain the supporter. */
  if (is_except
      && lib_multiple_retains_associate (game, supporter, "retain"))
    return TRUE;

  /* Filter objects into references, then handle with the backend. */
  objects = lib_apply_filter (game,
                              is_except ? lib_put_on_not_supporter_filter
                                        : lib_put_on_filter,
                              is_except ? supporter : -1, is_except,
                              &references);
  outcome = {};
  if (objects > 0 || references > 0)
    outcome = lib_put_on_backend (game, supporter);
  else
    lib_print_nothing_held (game, FALSE, is_except && objects == 0, ".");

  return lib_put_on_finish (game, outcome);
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


/*
 * lib_cmd_read_object()
 * lib_cmd_read_other()
 *
 * Attempt to read the referenced object, or something else.
 */
scr_bool
lib_cmd_read_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object, task;
  scr_bool is_readable, is_ambiguous;
  const scr_char *readtext, *description;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "read", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Verify that the object is readable. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Readable";
  is_readable = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (!is_readable)
    {
      lib_print_response_object (game,
                                 "You can't read ",
                                 "I can't read ",
                                 "%player% can't read ",
                                 object, "!\n");
      return TRUE;
    }

  /* Get and print the object's read text, if any. */
  vt_key[2].string = "ReadText";
  readtext = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (readtext))
    {
      pf_buffer_string (filter, readtext);
      pf_buffer_character (filter, '\n');
      return TRUE;
    }

  /* Degrade to a shortened object examine. */
  vt_key[2].string = "Task";
  task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;

  /* Select either the main or the alternate description. */
  if (task >= 0 && gs_task_done (game, task))
    vt_key[2].string = "AltDesc";
  else
    vt_key[2].string = "Description";

  /* Print the description, or a "nothing special" default. */
  description = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (description))
    pf_buffer_string (filter, description);
  else
    {
      pf_buffer_string (filter, "There is nothing special about ");
      lib_print_object_np (game, object);
      pf_buffer_character (filter, '.');
    }

  pf_buffer_character (filter, '\n');
  return TRUE;
}

scr_bool
lib_cmd_read_other (scr_gameref_t game)
{
  /*
   * Pre-4.0 `read` is not a verb of its own: it is ORed into the words that
   * ENTER examines() (run370 434E2A, run380 43C69D, run390 44B7FF), so a noun
   * that names nothing answers exactly as `x` does -- the flat, person-free
   * "Nothing special." tail, the same one lib_cmd_examine_other prints.  4.0
   * gave read its own handler and its own "<player> see no such thing."
   *
   * Measured: p39EXAM.taf (3.90), Adrift_41_p39exam.txt, `read zzzz` ->
   * "Nothing special."; p4EXAM.taf (4.00), Adrift_1_p4exam.txt, the same
   * command -> "You see no such thing."
   */
  if (!lib_is_version_400 (game))
    return lib_print_message (game, "Nothing special.\n");

  /* Reject the attempt -- the same "<name> see no such thing." literal as
     lib_cmd_examine_other(), unconjugated in the third person. */
  return lib_print_response_message (game,
                                     "You see no such thing.\n",
                                     "I see no such thing.\n",
                                     "%player% see no such thing.\n");
}


/*
 * lib_battle_player_strike()
 *
 * Shared Battle System helper: enforce a method verb's weapon-method
 * requirement, then deliver the blow with the given weapon (-1 for bare
 * hands).  method is -1 for a generic attack, or a method code 0..5
 * (chop/cut/hit/shoot/stab/throw) that the weapon must match; verb names the
 * action for the mismatch message.  No requirement is imposed when striking
 * bare-handed, so an unarmed method verb lands a plain blow (Runner-verified).
 */
static void
lib_battle_player_strike (scr_gameref_t game, scr_int npc,
                          const scr_char *verb, scr_int method, scr_int weapon)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (method >= 0 && weapon >= 0
      && battle_weapon_method (game, weapon) != method)
    {
      pf_buffer_string (filter, "You can't ");
      pf_buffer_string (filter, verb);
      lib_print_wrapped_object (game, " with ", weapon, "!\n");
      return;
    }

  battle_player_attack (game, npc, weapon);
}

/*
 * lib_battle_attack_bare()
 * lib_battle_attack_with()
 *
 * Cores for the player's attack commands, respectively without and with an
 * explicit weapon.  verb names the action and method is its required weapon
 * method (-1 for a generic attack).  With the Battle System enabled a real
 * blow is resolved.  Otherwise: a "legacy" verb (one with a traditional combat
 * grammar) prints the usual flavour response, while a verb introduced solely
 * for the Battle System falls through, leaving non-battle games' behaviour for
 * these words exactly as it was.
 */
/*
 * lib_battle_absent_npc_400()
 *
 * The 4.0 battle parser dobattle (Proc_11_4_47F084, entered from
 * generaltasks 48A4A2 whenever the Battle System is on) walks the NPCs in
 * index order and, for each whose Name is a whole word of the line
 * (47EB46), attacks it if it is in the player's room.  A named NPC who is
 * NOT here (47EF5E-47EFF4) prints "<Name> isn't here!" (47EFE5) when the
 * NPC has been seen (var_194(26) = 1), no earlier-named NPC was present
 * (var_92 = 0) and no NPC the line refers to is present (45E99C loop,
 * var_8A = 0).  Unlike the catch-all this is an ordinary turn: 494281 is
 * left alone, and the walk+event tick runs.
 *
 * Measured 2026-09-06 on Shadowpeak (Adrift_110): `attack margo with
 * sword` from the Torture Chamber, Margo seen and elsewhere, answers
 * "Margo isn't here!  Seeker hums!" -- the walker's line proves the tick.
 * The corpus's six `attack holga` lines are the same case.  Scarier used
 * to fall through to the catch-all ("I don't understand what you want me
 * to do with the body.") or the game's DontUnderstand text.
 *
 * Returns TRUE having printed for every such NPC; 3.9's battle parser is
 * unmeasured, so the clause is 4.0-only.
 */
static scr_bool
lib_battle_absent_npc_400 (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;
  scr_bool printed;

  if (!lib_is_version_400 (game) || !battle_is_enabled (game) || !input)
    return FALSE;

  printed = FALSE;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "NPCs", index_, "Name");
      if (!name || name[0] == NUL
          || !lib_input_contains_word (input, name)
          || !gs_npc_seen (game, index_)
          || npc_in_room (game, index_, gs_playerroom (game)))
        continue;

      pf_buffer_string (filter, name);
      pf_buffer_string (filter, " isn't here!\n");
      printed = TRUE;
    }

  return printed;
}

scr_bool
lib_battle_attack_bare (scr_gameref_t game, const scr_char *verb,
                        scr_int method, scr_bool legacy)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int npc;
  scr_bool is_ambiguous;

  /* A Battle-System-only verb defers to other grammar when battle is off. */
  if (!battle_is_enabled (game) && !legacy)
    return FALSE;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, verb, &is_ambiguous);
  if (npc == -1)
    {
      /* 4.0: a seen NPC named in the line but elsewhere "isn't here!" */
      if (!is_ambiguous && lib_battle_absent_npc_400 (game))
        return TRUE;
      return is_ambiguous;
    }

  /* With the Battle System enabled, resolve a real attack. */
  if (battle_is_enabled (game))
    {
      scr_int weapon = battle_player_wielded_weapon (game);

      /*
       * With no wield set the Runner auto-selects a solitary carried weapon
       * (the blow then persists it as the wield), fights bare-handed when
       * carrying none, and with two or more carried weapons asks -- a
       * rhetorical question, always worded with "attack" whatever the verb,
       * that costs no combat turn and whose reply is not read as an answer
       * (settled live 2026-08-01).
       */
      if (weapon < 0)
        {
          const scr_int count = battle_player_weapon_count (game);

          if (count > 1)
            {
              pf_buffer_string (filter, "What do you want to attack ");
              lib_print_npc_np (game, npc);
              pf_buffer_string (filter, " with?\n");
              game->is_admin = TRUE;
              return TRUE;
            }
          if (count == 1)
            weapon = battle_player_best_weapon (game);
        }
      lib_battle_player_strike (game, npc, verb, method, weapon);
      return TRUE;
    }

  /* Print a standard response. */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter,
                    lib_select_response (game,
                                      " avoids your feeble attempts.\n",
                                      " avoids my feeble attempts.\n",
                                      " avoids %player%'s feeble attempts.\n"));
  return TRUE;
}

static scr_bool
lib_battle_attack_with (scr_gameref_t game, const scr_char *verb,
                        scr_int method, scr_bool legacy)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, npc;
  scr_vartype_t vt_key[3];
  scr_bool weapon, is_ambiguous;

  /* A Battle-System-only verb defers to other grammar when battle is off. */
  if (!battle_is_enabled (game) && !legacy)
    return FALSE;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, verb, &is_ambiguous);
  if (npc == -1)
    {
      /* 4.0: a seen NPC named in the line but elsewhere "isn't here!" */
      if (!is_ambiguous && lib_battle_absent_npc_400 (game))
        return TRUE;
      return is_ambiguous;
    }

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, verb, NULL);
  if (object == -1)
    return TRUE;

  /* Ensure the referenced object is held.  The Runner: "Player is not
   * carrying the rock!" (probe pWS2 -- unlike wield's "aren't carrying"). */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You are not carrying ",
                                 "I am not carrying ",
                                 "%player% is not carrying ",
                                 object, "!\n");
      return TRUE;
    }

  /* With the Battle System enabled, resolve a real attack with the weapon. */
  if (battle_is_enabled (game))
    {
      /* The Runner rejects attacking with a non-weapon outright. */
      if (!battle_is_weapon (game, object))
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object, " is", " are"));
          pf_buffer_string (filter, " not a weapon!\n");
          return TRUE;
        }
      lib_battle_player_strike (game, npc, verb, method, object);
      return TRUE;
    }

  /* Check for static object moved to player by event. */
  if (obj_is_static (game, object))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      pf_buffer_string (filter, " not a weapon.\n");
      return TRUE;
    }

  /* Print standard response depending on if the object is a weapon. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Weapon";
  weapon = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (weapon)
    {
      lib_print_response_npc (game,
                              "You swing at ",
                              "I swing at ",
                              "%player% swing at ",
                              npc, " with ");
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             " but you miss.\n",
                                             " but I miss.\n",
                                             " but misses.\n"));
    }
  else
    {
      /*
       * "affective" [sic] and the "!" are the Runner's: every Runner string
       * table, run370 through run400, carries "I don't think X would be a
       * very affective weapon!" verbatim (measured 2026-08-17).
       */
      lib_print_wrapped_object (game, "I don't think ",
                                object, " would be a very affective weapon!\n");
    }
  return TRUE;
}


/*
 * lib_cmd_attack_npc()
 * lib_cmd_attack_npc_with()
 * lib_cmd_*_npc(), lib_cmd_*_npc_with()
 *
 * Attack an NPC, with and without weaponry.  The generic verbs (attack, fight,
 * kill, kick, slap) impose no weapon-method requirement; the remaining verbs
 * require a wielded weapon whose method matches (chop 0, cut 1, hit 2,
 * shoot 3, stab 4, throw 5) when the Battle System is enabled.
 */
scr_bool
lib_cmd_attack_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "attack", -1, TRUE);
}

scr_bool
lib_cmd_attack_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "attack", -1, TRUE);
}

/*
 * lib_cmd_slap_*()
 *
 * `slap` is not a verb in any Runner's grammar.  It is a pre-parse text
 * rewrite to `hit`, applied to the command line just after the game's own
 * synonyms and alongside "everything" -> "all" and "except" -> "but":
 *
 *   run370 loc_43B430, run380 loc_441C50   change("slap", "hit")
 *   run390 loc_45F246                      Replace("slap", "hit")
 *   run400 loc_48A330                      Replace(" slap ", " hit ")
 *
 * 4.0's form is space-bounded on both sides, so a command that *begins*
 * with `slap` is never rewritten.  That is why run400 answers `slap gizmo`
 * with the "I'm afraid that I wasn't anticipating that particular input."
 * a nonsense verb gets, while run390 gives it the combat dispatch (both
 * measured live 2026-08-18, on easter.taf and The Town of Azra).  So these
 * decline on 4.0 and behave as `hit` -- not `attack` -- everywhere else:
 * `hit` carries battle attack index 2, `attack`/`kick` carry -1.
 *
 * `smack` and `strike` are synonyms at no version: neither literal occurs
 * anywhere in run370.bas, run380.bas, run390 Form1.frm or run400.bas.
 */
static scr_bool
lib_slap_declines (scr_gameref_t game)
{
  return lib_is_version_400 (game);
}

scr_bool
lib_cmd_slap_npc (scr_gameref_t game)
{
  if (lib_slap_declines (game))
    return FALSE;
  return lib_cmd_hit_npc (game);
}

scr_bool
lib_cmd_slap_npc_with (scr_gameref_t game)
{
  if (lib_slap_declines (game))
    return FALSE;
  return lib_cmd_hit_npc_with (game);
}

scr_bool
lib_cmd_slap_object (scr_gameref_t game)
{
  if (lib_slap_declines (game))
    return FALSE;
  return lib_cmd_hit_object (game);
}

scr_bool
lib_cmd_slap_other (scr_gameref_t game)
{
  if (lib_slap_declines (game))
    return FALSE;
  return lib_cmd_hit_other (game);
}

scr_bool
lib_cmd_slap_what (scr_gameref_t game)
{
  if (lib_slap_declines (game))
    return FALSE;
  return lib_cmd_hit_what (game);
}

scr_bool
lib_cmd_chop_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "chop", 0, FALSE);
}

scr_bool
lib_cmd_chop_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "chop", 0, FALSE);
}

scr_bool
lib_cmd_cut_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "cut", 1, FALSE);
}

scr_bool
lib_cmd_cut_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "cut", 1, FALSE);
}

scr_bool
lib_cmd_hit_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "hit", 2, TRUE);
}

scr_bool
lib_cmd_hit_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "hit", 2, TRUE);
}

scr_bool
lib_cmd_shoot_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "shoot", 3, TRUE);
}

scr_bool
lib_cmd_shoot_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "shoot", 3, TRUE);
}

scr_bool
lib_cmd_stab_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "stab", 4, FALSE);
}

scr_bool
lib_cmd_stab_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "stab", 4, TRUE);
}

scr_bool
lib_cmd_throw_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "throw", 5, FALSE);
}

/*
 * lib_cmd_kill_npc(), lib_cmd_kill_npc_with()
 * lib_cmd_fight_npc(), lib_cmd_fight_npc_with()
 *
 * The Adrift Runner lists "kill" and "fight" among the Battle System's generic
 * attack verbs, so with the Battle System enabled they resolve a real blow like
 * "attack".  They are not "legacy" verbs, though: with the Battle System off
 * these fall through (return FALSE) to the later "kill *"/"fight *" grammar,
 * preserving the traditional flavour responses lib_cmd_kill_other() and
 * lib_cmd_fight() provide.
 */
scr_bool
lib_cmd_kill_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "kill", -1, FALSE);
}

scr_bool
lib_cmd_kill_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "kill", -1, FALSE);
}

scr_bool
lib_cmd_fight_npc (scr_gameref_t game)
{
  return lib_battle_attack_bare (game, "fight", -1, FALSE);
}

scr_bool
lib_cmd_fight_npc_with (scr_gameref_t game)
{
  return lib_battle_attack_with (game, "fight", -1, FALSE);
}


/*
 * lib_cmd_wield()
 *
 * Battle System weapon selection.  "wield <weapon>" sets a held weapon as the
 * player's persistent wield; the wield is cleared -- not swapped for another
 * carried weapon -- when the weapon leaves the player's hands.  There is no
 * "unwield" verb (the Runner answers "I don't understand.").  Falls through to
 * other grammar when the Battle System is disabled, as plain wielding is not
 * otherwise modelled.
 */
scr_bool
lib_cmd_wield (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;

  if (!battle_is_enabled (game))
    return FALSE;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "wield", NULL);
  if (object == -1)
    return TRUE;

  /* The weapon must be held, and must actually be a weapon.  The Runner's
   * refusal is "Player aren't carrying the rock!" [sic] (probe pWS2). */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You aren't carrying ",
                                 "I am not carrying ",
                                 "%player% aren't carrying ",
                                 object, "!\n");
      return TRUE;
    }
  if (!battle_is_weapon (game, object))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      pf_buffer_string (filter, " not a weapon!\n");
      return TRUE;
    }

  /* Wielding the already-wielded weapon is acknowledged, not repeated. */
  if (gs_playerwield (game) == object)
    {
      lib_print_response_object (game,
                                 "You are already wielding ",
                                 "I am already wielding ",
                                 "%player% is already wielding ",
                                 object, ".\n");
      return TRUE;
    }

  gs_set_playerwield (game, object);
  lib_print_response_object (game,
                             "You wield ",
                             "I wield ",
                             "%player% wield ",
                             object, ".\n");
  return TRUE;
}


/*
 * lib_cmd_kiss_npc()
 * lib_cmd_kiss_object()
 * lib_cmd_kiss_other()
 *
 * Reject romantic advances in all cases.
 */
scr_bool
lib_cmd_kiss_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int npc, gender;
  scr_bool is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "kiss", &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Gender";
  gender = prop_get_integer (bundle, "I<-sis", vt_key);

  switch (gender)
    {
    case NPC_MALE:
      pf_buffer_string (filter, "I'm not sure he would appreciate that!\n");
      break;

    case NPC_FEMALE:
      pf_buffer_string (filter, "I'm not sure she would appreciate that!\n");
      break;

    case NPC_NEUTER:
      pf_buffer_string (filter, "I'm not sure it would appreciate that!\n");
      break;

    default:
      scr_error ("lib_cmd_kiss_npc: unknown gender, %ld\n", gender);
    }
  return TRUE;
}

scr_bool
lib_cmd_kiss_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "kiss", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  lib_print_wrapped_object (game, "I'm not sure ",
                            object, " would appreciate that.\n");
  return TRUE;
}

scr_bool
lib_cmd_kiss_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  return lib_print_message (game, "I'm not sure it would appreciate that.\n");
}


/*
 * lib_cmd_buy_object()
 * lib_cmd_buy_other()
 *
 * Standard responses to attempts to buy something.
 */
scr_bool
lib_cmd_buy_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "buy", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  pf_buffer_string (filter, "I don't think ");
  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_plurality (game, object, " is", " are"));
  pf_buffer_string (filter, " for sale.\n");
  return TRUE;
}

scr_bool
lib_cmd_buy_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  return lib_print_message (game, "I don't think that is for sale.\n");
}

/*
 * lib_cmd_buy_absent()
 *
 * 4.0's therest() opens with the same clause, before any of its verb
 * branches; `buy` is the one that has been measured.  See
 * lib_absent_seen_object().
 */
scr_bool
lib_cmd_buy_absent (scr_gameref_t game)
{
  return lib_cant_see_absent_object (game, ".\n", TRUE);
}


/*
 * lib_cmd_break_object()
 * lib_cmd_break_other()
 *
 * Standard responses to attempts to break something.
 */
scr_bool
lib_cmd_break_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "break", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  lib_print_response_object (game,
                             "You might need ",
                             "I might need ",
                             "%player% might need ",
                             object, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_break_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  return lib_print_response_message (game,
                                     "You might need that.\n",
                                     "I might need that.\n",
                                     "%player% might need that.\n");
}


/*
 * lib_cmd_smell_object()
 * lib_cmd_smell_other()
 *
 * Standard responses to attempts to smell something.
 */
scr_bool
lib_cmd_smell_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "smell", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  pf_new_sentence (filter);
  lib_print_object_np (game, object);
  pf_buffer_string (filter, " smells normal.\n");
  return TRUE;
}

scr_bool
lib_cmd_smell_other (scr_gameref_t game)
{
  /* Reject this attempt. */
  return lib_print_message (game, "That smells normal.\n");
}


/*
 * lib_cmd_sell_object()
 * lib_cmd_sell_other()
 *
 * Standard responses to attempts to sell something.
 */
scr_bool
lib_cmd_sell_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "sell", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject this attempt. */
  lib_print_wrapped_object (game, "No-one is interested in buying ",
                            object, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_sell_other (scr_gameref_t game)
{
  return lib_print_message (game, "No-one is interested in buying that.\n");
}


/*
 * lib_cmd_eat_object()
 *
 * Consume edible objects.
 */
scr_bool
lib_cmd_eat_object (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int object;
  scr_bool edible, is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "eat", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Check that we have the object to eat. */
  if (gs_object_position (game, object) != OBJ_HELD_PLAYER)
    {
      lib_print_response_object (game,
                                 "You are not holding ",
                                 "I am not holding ",
                                 "%player% is not holding ",
                                 object, ".\n");
      return TRUE;
    }

  /* Check for static object moved to player by event. */
  if (obj_is_static (game, object))
    {
      lib_print_response_object (game,
                                 "You can't eat ",
                                 "I can't eat ",
                                 "%player% can't eat ",
                                 object, ".\n");
      return TRUE;
    }

  /* Is this object inedible? */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Edible";
  edible = prop_get_boolean (bundle, "B<-sis", vt_key);
  if (!edible)
    {
      lib_print_response_object (game,
                                 "You can't eat ",
                                 "I can't eat ",
                                 "%player% can't eat ",
                                 object, ".\n");
      return TRUE;
    }

  /* Confirm, and hide the object. */
  lib_print_response_object (game,
                             "You eat ",
                             "I eat ",
                             "%player% eat ",
                             object,
                             ".  Not bad, but it could do with a"
                             " pinch of salt!\n");
  gs_object_make_hidden (game, object);
  return TRUE;
}


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
  const scr_char *already_doing_that, *success_message;

  /* Initialize variables to avoid gcc warnings. */
  already_doing_that = FALSE;
  success_message = FALSE;

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
        disambiguate = NULL;
        cant_do_that = NULL;

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
            cant_do_that = NULL;
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

        /* Get the referenced object; if none, consider complete. */
        object = lib_disambiguate_object (game, disambiguate, &is_ambiguous);
        if (object == -1)
          return is_ambiguous;

        /* Verify the referenced object is amenable. */
        vt_key[0].string = "Objects";
        vt_key[1].integer = object;
        vt_key[2].string = "SitLie";
        sit_lie_flags = prop_get_integer (bundle, "I<-sis", vt_key);
        if (!(sit_lie_flags & movement_mask))
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

    case MOVE_STAND_FLOOR:
    case MOVE_SIT_FLOOR:
    case MOVE_LIE_FLOOR:
      object = -1;
      break;

    default:
      scr_fatal ("lib_sit_stand_lie: movement error, %ld\n", movement);
    }

  /* Set up confirmation messages and position. */
  switch (movement)
    {
    case MOVE_STAND:
    case MOVE_GET_ON:
      already_doing_that = lib_select_response (game,
                                            "You are already standing on ",
                                            "I am already standing on ",
                                            "%player% is already standing on ");
      success_message = lib_select_response (game,
                                             "You stand on ",
                                             "I stand on ",
                                             "%player% stand on ");
      position = 0;
      break;

    case MOVE_STAND_FLOOR:
      already_doing_that = lib_select_response (game,
                                             "You are already standing!\n",
                                             "I am already standing!\n",
                                             "%player% is already standing!\n");
      success_message = lib_select_response (game,
                                             "You stand up",
                                             "I stand up",
                                             "%player% stand up");
      position = 0;
      break;

    case MOVE_SIT:
      already_doing_that = lib_select_response (game,
                                             "You are already sitting on ",
                                             "I am already sitting on ",
                                             "%player% is already sitting on ");
      if (gs_playerposition (game) == 2)
        success_message = lib_select_response (game,
                                               "You sit up on ",
                                               "I sit up on ",
                                               "%player% sit up on ");
      else
        success_message = lib_select_response (game,
                                               "You sit down on ",
                                               "I sit down on ",
                                               "%player% sit down on ");
      position = 1;
      break;

    case MOVE_SIT_FLOOR:
      already_doing_that = lib_select_response (game,
                                         "You are already sitting down.\n",
                                         "I am already sitting down.\n",
                                         "%player% is already sitting down.\n");
      if (gs_playerposition (game) == 2)
        success_message = lib_select_response (game,
                                           "You sit up on the ground.\n",
                                           "I sit up on the ground.\n",
                                           "%player% sit up on the ground.\n");
      else
        success_message = lib_select_response (game,
                                         "You sit down on the ground.\n",
                                         "I sit down on the ground.\n",
                                         "%player% sit down on the ground.\n");
      position = 1;
      break;

    case MOVE_LIE:
      already_doing_that = lib_select_response (game,
                                               "You are already lying on ",
                                               "I am already lying on ",
                                               "%player% is already lying on ");
      success_message = lib_select_response (game,
                                             "You lie down on ",
                                             "I lie down on ",
                                             "%player% lie down on ");
      position = 2;
      break;

    case MOVE_LIE_FLOOR:
      already_doing_that = lib_select_response (game,
                                           "You are already lying down.\n",
                                           "I am already lying down.\n",
                                           "%player% is already lying down.\n");
      success_message = lib_select_response (game,
                                         "You lie down on the ground.\n",
                                         "I lie down on the ground.\n",
                                         "%player% lie down on the ground.\n");
      position = 2;
      break;

    default:
      scr_fatal ("lib_sit_stand_lie: movement error, %ld\n", movement);
    }

  /* See if already doing this. */
  if (gs_playerposition (game) == position && gs_playerparent (game) == object)
    {
      pf_buffer_string (filter, already_doing_that);
      if (object != -1)
        {
          lib_print_object_np (game, object);
          pf_buffer_string (filter, ".\n");
        }
      return TRUE;
    }

  /* Confirm movement, with special case for getting off an object. */
  pf_buffer_string (filter, success_message);
  if (movement == MOVE_STAND_FLOOR)
    {
      if (gs_playerparent (game) != -1)
        {
          pf_buffer_string (filter, " from ");
          lib_print_object_np (game, gs_playerparent (game));
        }
      pf_buffer_string (filter, ".\n");
    }
  else if (object != -1)
    {
      lib_print_object_np (game, object);
      pf_buffer_string (filter, ".\n");
    }

  /* Adjust player position and parent. */
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

static scr_bool lib_has_get_off (scr_gameref_t game);

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
 */
scr_bool
lib_cmd_get_on_object (scr_gameref_t game)
{
  if (!lib_has_get_off (game))
    return FALSE;
  return lib_stand_sit_lie (game, MOVE_GET_ON);
}

scr_bool
lib_cmd_stand_on_floor (scr_gameref_t game)
{
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
  return lib_stand_sit_lie (game, MOVE_LIE_FLOOR);
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

  /* Get the referenced object; if none, consider complete. */
  object = lib_disambiguate_object (game, "get off", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Reject the attempt if the player is not on the given object. */
  if (gs_playerparent (game) != object)
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

static scr_bool
lib_has_get_off (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390;
}

scr_bool
lib_cmd_get_off (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /*
   * Not a verb before 3.9.  run380 on Wrecked answers a bare `get off` with
   * "Take what?", i.e. the take handler, so decline and let the take family
   * keep it.
   */
  if (!lib_has_get_off (game))
    return FALSE;

  /* Reject the attempt if the player is not on anything. */
  if (gs_playerparent (game) == -1)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not standing on anything!\n",
                                             "I am not standing on anything!\n",
                                             "%player% is not standing on anything!\n"));
      return TRUE;
    }

  /* Confirm movement. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You get off ", "I get off ",
                                         "%player% get off "));
  lib_print_object_np (game, gs_playerparent (game));
  pf_buffer_string (filter, ".\n");

  /* Adjust player position and parent. */
  gs_set_playerposition (game, 0);
  gs_set_playerparent (game, -1);
  return TRUE;
}

/*
 * lib_cmd_get_down()
 *
 * `get down` shares the Runner's dismount branch with `get off`, so it is
 * gated the same way: both arrived in 3.9.  Under run380, `get down` and
 * `get off` are answered "Take what?" -- the take handler, not sitstand --
 * and declining here reproduces that, because the take family is matched
 * first and only falls through once it has failed to resolve an object.
 */
scr_bool
lib_cmd_get_down (scr_gameref_t game)
{
  if (!lib_has_get_off (game))
    return FALSE;
  return lib_cmd_get_off (game);
}


/*
 * lib_cmd_save()
 * lib_cmd_restore()
 *
 * Save/restore a game.
 */
scr_bool
lib_cmd_save (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_SAVE))
    {
      if (ser_save_game_prompted (game))
        if_print_string ("Ok.\n");
      else
        if_print_string ("Save failed.\n");
    }

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_restore (scr_gameref_t game)
{
  if (if_confirm (SCR_CONF_RESTORE))
    {
      if (ser_load_game_prompted (game))
        {
          if_print_string ("Ok.\n");
          game->is_running = FALSE;
          game->do_restore = TRUE;
        }
      else
        if_print_string ("Restore failed.\n");
    }

  game->is_admin = TRUE;
  return TRUE;
}


/*
 * lib_cmd_locate_object()
 * lib_cmd_locate_npc()
 *
 * Display the location of a selected object, and selected NPC.
 */
scr_bool
lib_cmd_locate_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int index_, count, object, room, position, parent;

  lib_set_admin (game);

  /*
   * Filter to remove unseen object references.  Note that this is different
   * from NPCs, who we acknowledge even when unseen.
   */
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (!gs_object_seen (game, index_))
        game->object_references[index_] = FALSE;
    }

  /* Count the number of objects referenced by the last command. */
  count = 0;
  object = -1;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (game->object_references[index_])
        {
          count++;
          object = index_;
        }
    }

  /*
   * If no objects identified, be coy about revealing anything; if more than
   * one, be vague.
   */
  if (count == 0)
    {
      pf_buffer_string (filter, "I don't know where that is.\n");
      return TRUE;
    }
  else if (count > 1)
    {
      pf_buffer_string (filter,
                        "Please be more clear about what you want to"
                        " locate.\n");
      return TRUE;
    }

  /*
   * The reference is unambiguous, so we're responsible for noting it in
   * variables.  Disambiguation would normally do this for us, but we just
   * bypassed it.
   */
  var_set_ref_object (vars, object);

  /* See if we can print a message based on position and parent. */
  position = gs_object_position (game, object);
  parent = gs_object_parent (game, object);
  switch (position)
    {
    case OBJ_HIDDEN:
      if (!obj_is_static (game, object))
        {
          pf_buffer_string (filter, "I don't know where that is.\n");
          return TRUE;
        }
      break;

    case OBJ_HELD_PLAYER:
      pf_new_sentence (filter);
      lib_print_response_object (game,
                                 "You are carrying ",
                                 "I am carrying ",
                                 "%player% is carrying ",
                                 object, "!\n");
      return TRUE;

    case OBJ_WORN_PLAYER:
      pf_new_sentence (filter);
      lib_print_response_object (game,
                                 "You are wearing ",
                                 "I am wearing ",
                                 "%player% is wearing ",
                                 object, "!\n");
      return TRUE;

    case OBJ_HELD_NPC:
    case OBJ_WORN_NPC:
      if (gs_npc_seen (game, parent))
        {
          pf_new_sentence (filter);
          lib_print_npc_np (game, parent);
          /*
           * "is carrying", not "is holding" -- the two literals sit side by
           * side in the Runner, run400 @467FC1 and @46802E, run380 @4372E6
           * and @43736B.  Upstream SCARE invented "holding".
           */
          pf_buffer_string (filter,
                            (position == OBJ_HELD_NPC)
                              ? " is carrying " : " is wearing ");
          lib_print_object_np (game, object);
          pf_buffer_string (filter, ".\n");
        }
      else
        pf_buffer_string (filter, "I don't know where that is.\n");
      return TRUE;

    case OBJ_PART_NPC:
      if (parent == -1)
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object, " is", " are"));
          pf_buffer_string (filter,
                            lib_select_response (game,
                                                 " a part of you!\n",
                                                 " a part of me!\n",
                                                 " a part of %player%!\n"));
        }
      else
        {
          if (gs_npc_seen (game, parent))
            {
              pf_new_sentence (filter);
              lib_print_object_np (game, object);
              pf_buffer_string (filter,
                                lib_select_plurality (game, object,
                                                      " is", " are"));
              lib_print_wrapped_npc (game, " a part of ", parent, ".\n");
            }
          else
            pf_buffer_string (filter, "I don't know where that is.\n");
        }
      return TRUE;

    case OBJ_ON_OBJECT:
    case OBJ_IN_OBJECT:
      if (gs_object_seen (game, parent))
        {
          pf_new_sentence (filter);
          lib_print_object_np (game, object);
          pf_buffer_string (filter,
                            lib_select_plurality (game, object, " is", " are"));
          pf_buffer_string (filter,
                            (position == OBJ_ON_OBJECT) ? " on " : " inside ");
          lib_print_object_np (game, parent);
          pf_buffer_string (filter, ".\n");
        }
      else
        pf_buffer_string (filter, "I don't know where that is.\n");
      return TRUE;
    }

  /*
   * Object is either static unmoved, or dynamic and on the floor of a room.
   * Check each room for the object, stopping on first found.
   */
  for (room = 0; room < gs_room_count (game); room++)
    {
      if (obj_indirectly_in_room (game, object, room))
        break;
    }
  if (room == gs_room_count (game))
    {
      pf_buffer_string (filter, "I don't know where that is.\n");
      return TRUE;
    }

  /* Check that this room's been visited by the player. */
  if (!gs_room_seen (game, room))
    {
      pf_new_sentence (filter);
      lib_print_object_np (game, object);
      pf_buffer_string (filter,
                        lib_select_plurality (game, object, " is", " are"));
      /*
       * No "that" here.  The object branch concatenates a bare "somewhere "
       * (run400 @4681B0, run380 @4375D3) where the character branch of the
       * same command uses " is somewhere that " (run400 @47FD89,
       * run380 @440C11) -- an inconsistency of ADRIFT's own that all four
       * Runners carry.
       */
      pf_buffer_string (filter,
                        lib_select_response (game,
                             " somewhere you haven't been yet.\n",
                             " somewhere I haven't been yet.\n",
                             " somewhere %player_pronoun% haven't been yet.\n"));
      return TRUE;
    }

  /*
   * "<Object> is <lowercased room name>."  The Runner builds this as
   * name & isare(prefix, short) & LCase(room name) & "." -- run400 @468115
   * through @46814E, run380 @4374E1 -- and the " -- " upstream SCARE printed
   * here appears in none of the four binaries.
   */
  pf_new_sentence (filter);
  lib_print_object_np (game, object);
  pf_buffer_string (filter,
                    lib_select_plurality (game, object, " is ", " are "));
  lib_print_room_name_lower (game, room);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_locate_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int index_, count, npc, room;

  lib_set_admin (game);

  /* Count the number of NPCs referenced by the last command. */
  count = 0;
  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_])
        {
          count++;
          npc = index_;
        }
    }

  /*
   * If no NPCs identified, be coy about revealing anything; if more than one,
   * be vague.  The "... where that is..." is the correct message even for
   * NPCs -- it's the same response as for lib_locate_other().
   */
  if (count == 0)
    {
      pf_buffer_string (filter, "I don't know where that is.\n");
      return TRUE;
    }
  else if (count > 1)
    {
      pf_buffer_string (filter,
                        "Please be more clear about who you want to locate.\n");
      return TRUE;
    }

  /*
   * The reference is unambiguous, so we're responsible for noting it in
   * variables.  Disambiguation would normally do this for us, but we just
   * bypassed it.
   */
  var_set_ref_character (vars, npc);

  /* See if this NPC has been seen yet. */
  if (!gs_npc_seen (game, npc))
    {
      lib_print_response_npc (game,
                              "You haven't seen ",
                              "I haven't seen ",
                              "%player% haven't seen ",
                              npc, " yet!\n");
      return TRUE;
    }

  /*
   * A corpse gets its own answer, ahead of the room search.  The Runner
   * reaches it only after the seen test and only when the room field is not a
   * real room -- the -5 it stamps there fails the `> 0` gate -- at run400
   * @47FDB9 and run390 @459D68.  3.7 and 3.8 have no battle system and no such
   * branch or string at all, so there is nothing to gate on version.
   */
  if (gs_npc_dead (game, npc))
    {
      lib_print_wrapped_npc (game, "", npc, " is dead!\n");
      return TRUE;
    }

  /* Check each room for the NPC, stopping on first found. */
  for (room = 0; room < gs_room_count (game); room++)
    {
      if (npc_in_room (game, npc, room))
        break;
    }
  if (room == gs_room_count (game))
    {
      lib_print_wrapped_npc (game, "I don't know where ", npc, " is.\n");
      return TRUE;
    }

  /* Check that this room's been visited by the player. */
  if (!gs_room_seen (game, room))
    {
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter,
                        lib_select_response (game,
                             " is somewhere that you haven't been yet.\n",
                             " is somewhere that I haven't been yet.\n",
                             " is somewhere that %player_pronoun% haven't been yet.\n"));
      return TRUE;
    }

  /*
   * "<Name> is <lowercased room name>.", then the smart-alec clause when the
   * NPC is standing next to the player.  The character branch uses a literal
   * " is " rather than isare(), and the room name is lowercased exactly as in
   * the object branch:
   *      run370 @438D0A   run380 @440BDF   run390 @459D27   run400 @47FD19
   *
   * The clause itself was disabled upstream; all four Runners print it, and
   * there is no comma before "silly" -- the literals are "  (Right next to "
   * and " silly!)" with the perspective pronoun spliced between them
   * (run380 @00040BCE/@00040BE2, run400 @47FD56/@47FD6A).
   */
  pf_new_sentence (filter);
  lib_print_npc_np (game, npc);
  pf_buffer_string (filter, " is ");
  lib_print_room_name_lower (game, room);
  pf_buffer_string (filter, ".");
  if (room == gs_playerroom (game))
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                          "  (Right next to you silly!)",
                                          "  (Right next to me silly!)",
                                          "  (Right next to %player% silly!)"));
    }
  pf_buffer_character (filter, '\n');
  return TRUE;
}


/*
 * lib_cmd_turns()
 * lib_cmd_score()
 *
 * Display turns taken and score so far.
 */
scr_bool
lib_cmd_turns (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  /* "1 turns" -- no Runner singularises it (run400 48ACA1 concatenates the
     one literal; measured run400 EV16 Adrift_1_ev16.txt and run390
     BobBobsly.taf Adrift_1_bob390.txt, both "You have taken 1 turns so
     far."). */
  pf_buffer_string (filter, "You have taken ");
  pf_buffer_integer (filter, game->turns);
  pf_buffer_string (filter, " turns so far.\n");

  lib_set_admin (game);
  return TRUE;
}

scr_bool
lib_cmd_score (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int max_score, percent;

  /* Get max score, and calculate score as a percentage. */
  max_score = prop_get_global_integer (bundle, "MaxScore");
  if (game->score > 0 && max_score > 0)
    percent = (game->score * 100) / max_score;
  else
    percent = 0;

  /* Output carefully formatted response. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "Your score is ",
                                         "My score is ",
                                         "%player%'s score is "));
  pf_buffer_integer (filter, game->score);
  pf_buffer_string (filter, " out of a maximum of ");
  pf_buffer_integer (filter, max_score);
  pf_buffer_string (filter, ".  (");
  pf_buffer_integer (filter, percent);
  pf_buffer_string (filter, "%)\n");

  lib_set_admin (game);
  return TRUE;
}


/*
 * lib_print_battle_attribute()
 * lib_print_battle_status()
 *
 * Helpers for the Battle System "status" command, matching the Runner's
 * status table (settled live 2026-08-01, probes pWS/pWS2): a header row
 * "Range / Max / Current value (inc weapons/armour)", then one row per
 * attribute.  The stamina row is live / max / live (no lo-hi range); the
 * other rows are "lo-hi", the configured max, a fresh effective roll that
 * folds in the wielded weapon and worn armour, and -- for the three rows
 * equipment can affect -- the equipment share in parentheses (agility takes
 * none, so its row has no parenthesis).  The table has no leading "You
 * have:" line, and the trailing wielding line is indented to the first
 * column and names the weapon with its article prefix ("a sword"), or
 * "nothing".  Until a wield is set the player's values are bare -- no
 * would-be weapon is folded in.
 */
enum
{ STATUS_COL_LABEL = 16,
  STATUS_COL_RANGE = 14,
  STATUS_COL_MAX = 12,
  STATUS_COL_CURRENT = 12
};

static void
lib_print_battle_attribute (scr_gameref_t game, scr_int npc,
                            const scr_char *label, const scr_char *base,
                            scr_bool has_bonus)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_char buffer[96], range[32];
  scr_int lo, hi, current;

  battle_attribute_report (game, npc, base, &lo, &hi, &current);
  snprintf (range, sizeof (range), "%ld-%ld", lo, hi);
  if (has_bonus)
    snprintf (buffer, sizeof (buffer), "%-*s%-*s%-*ld%-*ld(%ld)",
              STATUS_COL_LABEL, label, STATUS_COL_RANGE, range,
              STATUS_COL_MAX, battle_attribute_max (game, npc, base),
              STATUS_COL_CURRENT, current,
              battle_attribute_bonus (game, npc, base));
  else
    snprintf (buffer, sizeof (buffer), "%-*s%-*s%-*ld%ld",
              STATUS_COL_LABEL, label, STATUS_COL_RANGE, range,
              STATUS_COL_MAX, battle_attribute_max (game, npc, base),
              current);
  pf_buffer_string (filter, buffer);
  pf_buffer_character (filter, '\n');
}

static void
lib_print_battle_status (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_char buffer[96];
  scr_int stamina, maxstamina, weapon;

  maxstamina = battle_attribute_max (game, npc, "Stamina");

  /* A character with no stamina configured is not a combatant. */
  if (npc >= 0 && maxstamina <= 0)
    {
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter, " has:\n   Nothing worth mentioning.\n");
      return;
    }

  /* The header row, indented past the label column as the Runner's is. */
  snprintf (buffer, sizeof (buffer), "%-*s%-*s%-*s%s",
            STATUS_COL_LABEL, "", STATUS_COL_RANGE, "Range",
            STATUS_COL_MAX, "Max", "Current value (inc weapons/armour)");
  pf_buffer_string (filter, buffer);
  pf_buffer_character (filter, '\n');

  /* The stamina row is live / max / live -- no lo-hi range, no bonus. */
  stamina = (npc < 0)
            ? gs_playerstamina (game) : gs_npc_stamina (game, npc);
  snprintf (buffer, sizeof (buffer), "%-*s%-*ld%-*ld%ld",
            STATUS_COL_LABEL, "Stamina:", STATUS_COL_RANGE, stamina,
            STATUS_COL_MAX, maxstamina, stamina);
  pf_buffer_string (filter, buffer);
  pf_buffer_character (filter, '\n');

  lib_print_battle_attribute (game, npc, "Hit strength:", "Strength", TRUE);
  lib_print_battle_attribute (game, npc, "Accuracy:", "Accuracy", TRUE);
  lib_print_battle_attribute (game, npc, "Defense value:", "Defense", TRUE);
  lib_print_battle_attribute (game, npc, "Agility:", "Agility", FALSE);

  /* Name the weapon the combatant is wielding -- "nothing" when unarmed --
   * with the article prefix the Runner uses ("... is wielding a sword."). */
  weapon = battle_combatant_weapon (game, npc);
  snprintf (buffer, sizeof (buffer), "%-*s", STATUS_COL_LABEL, "");
  pf_buffer_string (filter, buffer);
  if (npc < 0)
    pf_buffer_string (filter,
                      lib_select_response (game,
                                           "You are wielding ",
                                           "I am wielding ",
                                           "%player% is wielding "));
  else
    {
      lib_print_npc_np (game, npc);
      pf_buffer_string (filter, " is wielding ");
    }
  if (weapon >= 0)
    lib_print_object (game, weapon);
  else
    pf_buffer_string (filter, "nothing");
  pf_buffer_string (filter, ".\n");
}


/*
 * lib_cmd_status_player()
 * lib_cmd_status_npc()
 *
 * The Battle System "status" and "status <character>" commands.  When the
 * Battle System is disabled these fall back to the traditional behaviour of
 * "status", which prints the game's status line.
 */
scr_bool
lib_cmd_status_player (scr_gameref_t game)
{
  if (!battle_is_enabled (game))
    return lib_cmd_statusline (game);

  lib_print_battle_status (game, -1);
  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_status_npc (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int index_, count, npc;

  if (!battle_is_enabled (game))
    return lib_cmd_statusline (game);

  game->is_admin = TRUE;

  /* Count and identify the NPCs referenced by the command. */
  count = 0;
  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_])
        {
          count++;
          npc = index_;
        }
    }

  if (count == 0)
    {
      pf_buffer_string (filter, "I don't know who you mean.\n");
      return TRUE;
    }
  else if (count > 1)
    {
      pf_buffer_string (filter,
                        "Please be more clear about whose status you want.\n");
      return TRUE;
    }

  /* Unambiguous reference; note it, as we bypassed disambiguation. */
  var_set_ref_character (vars, npc);

  /* Refuse to report on a character the player has not encountered. */
  if (!gs_npc_seen (game, npc))
    {
      lib_print_response_npc (game,
                              "You haven't seen ",
                              "I haven't seen ",
                              "%player% haven't seen ",
                              npc, " yet!\n");
      return TRUE;
    }

  lib_print_battle_status (game, npc);
  return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Standard response commands.  These are uninteresting catch-all cases,
 * but it's good to make then right as game ALRs may look for them.
 */
scr_bool
lib_cmd_profanity (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I really don't think there's any need for language like"
                            " that!\n");
}

/*
 * lib_cmd_profanity_390()
 * lib_cmd_profanity_pre_400()
 *
 * Two words entered and left the Runner's swearing list.  `bugger` is in the
 * 3.9 and 4.0 lists but in neither 3.7's nor 3.8's, and `bloody` is in
 * 3.7/3.8/3.9 and gone from 4.0.  Where the word is not in that Runner's list
 * these decline, so the input falls through to the rest of the grammar exactly
 * as any other unrecognised word does.
 */
scr_bool
lib_cmd_profanity_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;
  return lib_cmd_profanity (game);
}

scr_bool
lib_cmd_profanity_pre_400 (scr_gameref_t game)
{
  if (lib_is_version_400 (game))
    return FALSE;
  return lib_cmd_profanity (game);
}

scr_bool
lib_cmd_examine_all (scr_gameref_t game)
{
  return lib_print_message (game, "Please examine one object at a time.\n");
}

/*
 * lib_cmd_examine_other()
 *
 * `x <noun>` where the noun names nothing at all, in a lit room.  This is the
 * last line of the Runner's examines(), and 4.0 rewrote it.  Pre-4.0 answers
 * the flat, person-free "Nothing special." (run370 435BF4, readable verbatim
 * in run370's Form1.frm; the string is in all four exes).  4.0 answers
 * "<player> see no such thing." (run400 471EF6) -- and that string is in
 * run400.exe alone, which is what dates the change.
 *
 * Both halves are measured, not argued:
 *   run390, Merry_Murders.taf (3.90), Adrift_39_merry_murders.txt line 38 --
 *     `x pocket` in the lit Plaza, no `pocket` object in the game:
 *     "Nothing special."
 *   run400, The_X-Files_A_New_Beginning.taf (4.00), Adrift_22_xfiles.txt
 *     lines 187 and 233 -- `look at camera` and `look up byers`, neither noun
 *     an object: "You see no such thing."
 *
 * Not ported, because nothing has measured them: 4.0 also sets a flag beside
 * this message (MemVar_494281 at 471F02), and no Runner's darkness answers
 * ("<player> can't see that very clearly.", "<player> can just make out that
 * ...") exist here at all.
 *
 * The object-found-but-silent default one branch up is the SAME 4.0 rewrite
 * and splits the same way, but it is decompile-only so far -- see
 * lib_cmd_examine_object() and harness/make_39_examprobe.py.
 */
scr_bool
lib_cmd_examine_other (scr_gameref_t game)
{
  if (!lib_is_version_400 (game))
    return lib_print_message (game, "Nothing special.\n");

  /*
   * The 4.0 text is the player's name and the literal " see no such thing."
   * (471EF6), so a named third-person player gets "Player see no such
   * thing." -- measured run400, EV16, Adrift_1_ev16.txt (`x dave`, Dave
   * being in the next room).  The same probe reads `turns` unchanged across
   * it: the flag 4.0 sets beside the message (MemVar_494281 at 471F02, and
   * at 4801E1 for the character handler's copy) is the Runner's "not a
   * turn" flag, the one `turns` and `score` set.
   */
  lib_print_response_message (game,
                              "You see no such thing.\n",
                              "I see no such thing.\n",
                              "%player% see no such thing.\n");
  game->is_admin = TRUE;
  return TRUE;
}

/*
 * lib_cmd_examine_absent()
 *
 * `x <object seen elsewhere>`, once every other examine row has declined it.
 * 4.0 only; see lib_absent_seen_object() for the measurement.
 */
scr_bool
lib_cmd_examine_absent (scr_gameref_t game)
{
  return lib_cant_see_absent_object (game, " from here!\n", TRUE);
}

scr_bool
lib_cmd_locate_other (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, "I don't know where that is!\n");
  lib_set_admin (game);
  return TRUE;
}

scr_bool
lib_cmd_unix_like (scr_gameref_t game)
{
  return lib_print_message (game, "This isn't Unix you know!\n");
}

scr_bool
lib_cmd_dos_like (scr_gameref_t game)
{
  return lib_print_message (game, "This isn't Dos you know!\n");
}

scr_bool
lib_cmd_cry (scr_gameref_t game)
{
  return lib_print_message (game, "There's no need for that!\n");
}

scr_bool
lib_cmd_dance (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You do a little dance.\n",
                                     "I do a little dance.\n",
                                     "%player% do a little dance.\n");
}

scr_bool
lib_cmd_eat_other (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I don't understand what you are trying to eat.\n");
}

scr_bool
lib_cmd_fight (scr_gameref_t game)
{
  return lib_print_message (game, "There is nothing worth fighting here.\n");
}

scr_bool
lib_cmd_feed (scr_gameref_t game)
{
  return lib_print_message (game, "There is nothing worth feeding here.\n");
}

scr_bool
lib_cmd_feel (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You feel nothing out of the ordinary.\n",
      "I feel nothing out of the ordinary.\n",
      "%player% feel nothing out of the ordinary.\n");
}

scr_bool
lib_cmd_fly (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You can't fly.\n",
                                     "I can't fly.\n",
                                     "%player% can't fly.\n");
}

scr_bool
lib_cmd_hint (scr_gameref_t game)
{
  return lib_print_message (game,
                            "You're just going to have to work it out for"
                            " yourself...\n");
}

scr_bool
lib_cmd_hum (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You hum a little tune.\n",
                                     "I hum a little tune.\n",
                                     "%player% hum a little tune.\n");
}

scr_bool
lib_cmd_jump (scr_gameref_t game)
{
  return lib_print_message (game, "Wheee-boinng.\n");
}

scr_bool
lib_cmd_listen (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You hear nothing out of the ordinary.\n",
      "I hear nothing out of the ordinary.\n",
      "%player% hear nothing out of the ordinary.\n");
}

scr_bool
lib_cmd_please (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "Your kindness gets you nowhere.\n",
                                     "My kindness gets me nowhere.\n",
                                     "%player%'s kindness gets nowhere.\n");
}

scr_bool
lib_cmd_punch (scr_gameref_t game)
{
  return lib_print_message (game, "Who do you think you are, Mike Tyson?\n");
}

scr_bool
lib_cmd_run (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "Why would you want to run?\n",
                                     "Why would I want to run?\n",
                                     "Why would %player_pronoun% want to run?\n");
}

scr_bool
lib_cmd_shout (scr_gameref_t game)
{
  return lib_print_message (game, "Aaarrrrgggghhhhhh!\n");
}

scr_bool
lib_cmd_say (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *string = NULL;

  switch (scr_randomint (1, 5))
    {
    case 1:
      string = "Gosh, that was very impressive.\n";
      break;
    case 2:
      string = lib_select_response (game,
                                    "Not surprisingly, no-one takes any notice"
                                    " of you.\n",
                                    "Not surprisingly, no-one takes any notice"
                                    " of me.\n",
                                    "Not surprisingly, no-one takes any notice"
                                    " of %player%.\n");
      break;
    case 3:
      string = "Wow!  That achieved a lot.\n";
      break;
    case 4:
      string = "Uh huh, yes, very interesting.\n";
      break;
    default:
      string = "That's the most interesting thing I've ever heard!\n";
      break;
    }

  pf_buffer_string (filter, string);
  return TRUE;
}

scr_bool
lib_cmd_sing (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You sing a little song.\n",
                                     "I sing a little song.\n",
                                     "%player% sing a little song.\n");
}

scr_bool
lib_cmd_sleep (scr_gameref_t game)
{
  return lib_print_message (game, "Zzzzz.  Bored are you?\n");
}

scr_bool
lib_cmd_talk (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "No-one listens to your rabblings.\n",
      "No-one listens to my rabblings.\n",
      "No-one listens to %player%'s rabblings.\n");
}

scr_bool
lib_cmd_thank (scr_gameref_t game)
{
  return lib_print_message (game, "You're welcome.\n");
}

scr_bool
lib_cmd_whistle (scr_gameref_t game)
{
  return lib_print_response_message (game,
                                     "You whistle a little tune.\n",
                                     "I whistle a little tune.\n",
                                     "%player% whistle a little tune.\n");
}

scr_bool
lib_cmd_interrogation (scr_gameref_t game)
{
  static const scr_char *const RESPONSES[] = {
    "Why do you want to know?\n",
    "Interesting question.\n",
    "Let me think about that one...\n",
    "I haven't a clue!\n",
    "All these questions are hurting my head.\n",
    "I'm not going to tell you.\n",
    "Someday I'll know the answer to that one.\n",
    "I could tell you, but then I'd have to kill you.\n",
    "Ha, as if I'd tell you!\n",
    "Ask me again later.\n",
    "I don't know - could you ask anyone else?\n",
    "Err, yes?!?\n",
    "Let me just check my memory banks...\n",
    "Because that's just the way it is.\n",
    "Do I ask you all sorts of awkward questions?\n",
    "Questions, questions...\n",
    "Who cares.\n"
  };

  return lib_print_message (game,
                            RESPONSES[scr_randomint (1, 17) - 1]);
}

scr_bool
lib_cmd_xyzzy (scr_gameref_t game)
{
  return lib_print_message (game,
                            "I'm sorry, but XYZZY doesn't do anything special in"
                            " this game!\n");
}

scr_bool
lib_cmd_egotistic (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

#if 0
  pf_buffer_string (filter,
                    "Campbell wrote this Adrift Runner.  It's pretty"
                    " good huh!\n");
#else
  pf_buffer_string (filter, "No comment.\n");
#endif

  return TRUE;
}

scr_bool
lib_cmd_yes_or_no (scr_gameref_t game)
{
  return lib_print_message (game,
                            "That's interesting, but it doesn't mean much.\n");
}


/*
 * lib_ask_format_subject()
 * lib_ask_format_character()
 *
 * The ask-format hint spells its placeholders with angle brackets in 3.7 and
 * with square ones from 3.8 on -- the only thing that changed about it in
 * four Runner releases:
 *
 *   run370 loc_4387A0, loc_438BDC   "ask " & name & " about <subject>" & "."
 *   run370 loc_43D782               "ask <character> about <subject>"
 *   run380 loc_44062F, loc_440A6B   "ask " & name & " about [subject]" & "."
 *   run380 loc_444219               "ask [character] about [subject]"
 *   run390 loc_45976B / loc_45DA37, run400 loc_47F879 / loc_488D55: as 3.8.
 */
static const scr_char *
lib_ask_format_subject (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
         ? " about [subject]\".\n" : " about <subject>\".\n";
}

static const scr_char *
lib_ask_format_character (scr_gameref_t game)
{
  return prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_380
         ? "Use the format \"ask [character] about [subject]\".\n"
         : "Use the format \"ask <character> about <subject>\".\n";
}


/*
 * lib_cmd_ask_npc()
 * lib_cmd_ask_object()
 * lib_cmd_ask_other()
 * lib_cmd_talk_to_npc()
 * lib_cmd_talk_to_npc_pre_390()
 *
 * Malformed and rhetorical question responses.
 *
 * `talk to X` and `speak to X` reach the same hint.  It is produced by the
 * per-character pass, inside the block that a command only enters when it
 * names the character (run380 loc_4401AA, `c(name) Or c(descriptor)`), by a
 * branch whose guard is the one thing here that moved between releases:
 *
 *   run370 loc_438748, run380 loc_4405D7   c("talk") Or c("speak")
 *   run390 loc_45973D, run400 loc_47F84A   c("talk to") Or c("speak to")
 *
 * So 3.7 and 3.8 answer a bare `talk bob` or `speak bob` with the hint and
 * 3.9 and 4.0 do not -- there the bare word falls through to the
 * generaltasks `c("talk")` rabblings line, which every Runner has
 * (run400 loc_488DA2), and `speak` alone reaches nothing at all.
 *
 * `talk to X about Y` is different again: `c("ask") Or c("talk to")` guards
 * the real conversation branch in all four (run370 loc_4387F4, run380
 * loc_440683, run390 loc_4597F2, run400 loc_47F8F7), and it runs after the
 * hint branch and overwrites it.  `speak to` is not in that list, so it only
 * ever gets the hint.  See the grammar rows in scrunner.cpp.
 *
 * Not ported: the pre-parse rewrites of a command *beginning* `ask about `
 * or `talk about ` into `ask <last named character> about ...` (run370
 * loc_4380CF/loc_438185, run380 loc_43FF4F/loc_440005, run390 loc_459010/
 * loc_4590E1, run400 loc_47F14C/loc_47F20F).  They need the Runner's
 * "character most recently named by a command" register, which SCARE has no
 * equivalent of.
 */
scr_bool
lib_cmd_ask_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "ask", &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  /* Incomplete ask command, so offer help and return. */
  lib_print_wrapped_npc (game, "Use the format \"ask ",
                         npc, lib_ask_format_subject (game));
  return TRUE;
}

scr_bool
lib_cmd_talk_to_npc (scr_gameref_t game)
{
  scr_int npc;
  scr_bool is_ambiguous;

  /* Get the referenced npc, and if none, consider complete. */
  npc = lib_disambiguate_npc (game, "talk to", &is_ambiguous);
  if (npc == -1)
    return is_ambiguous;

  lib_print_wrapped_npc (game, "Use the format \"ask ",
                         npc, lib_ask_format_subject (game));
  return TRUE;
}

scr_bool
lib_cmd_talk_to_npc_pre_390 (scr_gameref_t game)
{
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return FALSE;

  return lib_cmd_talk_to_npc (game);
}

scr_bool
lib_cmd_ask_object (scr_gameref_t game)
{
  scr_int object;
  scr_bool is_ambiguous;

  /* Get the referenced object, and if none, consider complete. */
  object = lib_disambiguate_object (game, "ask", &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* No reply. */
  lib_print_response_object (game,
                             "You get no reply from ",
                             "I get no reply from ",
                             "%player% get no reply from ",
                             object, ".\n");
  return TRUE;
}

scr_bool
lib_cmd_ask_other (scr_gameref_t game)
{
  /* Incomplete ask command, so offer help and return. */
  return lib_print_message (game, lib_ask_format_character (game));
}


/*
 * lib_cmd_ask_about_nothing()
 *
 * `ask ... about ...` that named neither a character nor an object.
 *
 * Every Runner splits its `c("ask") Or c("talk to")` block on `c("about")`
 * (run370 loc_43E9B7, run380 loc_444039, run390 loc_45D8E5, run400
 * loc_488B87).  The branch WITHOUT "about" is the per-character format hint
 * -- what lib_cmd_ask_other() prints -- and the branch WITH it looks for a
 * named, present object ("<You> get no reply from <it>.", lib_cmd_ask_object)
 * and, failing that, seeds the response buffer with
 *
 *   MemVar_4941D0(0) & " can't talk to that."      run400 loc_488C65
 *   MemVar_44F108(0) & " can't talk to that."      run380 loc_44410C
 *
 * so a line the character handler then answers -- a topic, "<npc> does not
 * respond to your question.", "<npc> isn't here!" -- overwrites it, and a
 * line nothing answers keeps it.  Scarier's grammar puts the character and
 * object rows ahead of this one, which is the same order.
 *
 * Measured on thelasthour (4.00, Adrift_297_thelasthour.txt turn 80):
 * `ask sly about him` -- "sly" is a task word, not a character, and the
 * game has no male to fill "him" in -- answers "(No male)" and then "I
 * can't talk to that." where Scarier used to print the format hint.
 */
scr_bool
lib_cmd_ask_about_nothing (scr_gameref_t game)
{
  return lib_print_response_message (game,
      "You can't talk to that.\n",
      "I can't talk to that.\n",
      "%player% can't talk to that.\n");
}


/*
 * lib_cmd_kill_other()
 *
 * Uninteresting kill message when no weaponry is involved.
 */
scr_bool
lib_cmd_kill_other (scr_gameref_t game)
{
  return lib_print_message (game, "Now that isn't very nice.\n");
}


/*
 * lib_nothing_happens_common()
 * lib_nothing_happens_object()
 * lib_nothing_happens_other()
 *
 * Central handler for a range of nothing-happens messages.  More
 * uninteresting responses.
 */
static scr_bool
lib_nothing_happens_common (scr_gameref_t game,
                            const scr_char *verb_general,
                            const scr_char *verb_third_person,
                            scr_bool is_object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int perspective, object;
  const scr_char *person, *verb;
  scr_bool is_ambiguous;

  /* Use person and verb tense according to perspective. */
  perspective = lib_get_perspective (game);
  switch (perspective)
    {
    case LIB_FIRST_PERSON:
      person = "I ";
      verb = verb_general;
      break;
    case LIB_SECOND_PERSON:
      person = "You ";
      verb = verb_general;
      break;
    case LIB_THIRD_PERSON:
      person = "%player% ";
      verb = verb_third_person;
      break;
    default:
      scr_error ("lib_nothing_happens: unknown perspective, %ld\n", perspective);
      person = "You ";
      verb = verb_general;
      break;
    }

  /* If the command target was not an object, end it here. */
  if (!is_object)
    {
      pf_buffer_string (filter, person);
      pf_buffer_string (filter, verb);
      pf_buffer_string (filter, ", but nothing happens.\n");
      return TRUE;
    }

  /* Get the referenced object.  If none, return immediately. */
  object = lib_disambiguate_object (game, verb_general, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Nothing happens. */
  pf_buffer_string (filter, person);
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ", but nothing happens.\n");
  return TRUE;
}

static scr_bool
lib_nothing_happens_object (scr_gameref_t game,
                            const scr_char *verb_general,
                            const scr_char *verb_third_person)
{
  return lib_nothing_happens_common (game,
                                     verb_general, verb_third_person, TRUE);
}

static scr_bool
lib_nothing_happens_other (scr_gameref_t game,
                           const scr_char *verb_general,
                           const scr_char *verb_third_person)
{
  return lib_nothing_happens_common (game,
                                     verb_general, verb_third_person, FALSE);
}


/*
 * lib_cmd_*()
 *
 * Shake, rattle and roll, and assorted nothing-happens handlers.
 */
scr_bool
lib_cmd_hit_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "hit", "hits");
}

scr_bool
lib_cmd_kick_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "kick", "kicks");
}

scr_bool
lib_cmd_press_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "press", "presses");
}

scr_bool
lib_cmd_push_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "push", "pushes");
}

scr_bool
lib_cmd_pull_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "pull", "pulls");
}

scr_bool
lib_cmd_shake_object (scr_gameref_t game)
{
  return lib_nothing_happens_object (game, "shake", "shakes");
}

scr_bool
lib_cmd_hit_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "hit", "hits");
}

scr_bool
lib_cmd_kick_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "kick", "kicks");
}

scr_bool
lib_cmd_press_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "press", "presses");
}

scr_bool
lib_cmd_push_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "push", "pushes");
}

scr_bool
lib_cmd_pull_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "pull", "pulls");
}

scr_bool
lib_cmd_shake_other (scr_gameref_t game)
{
  return lib_nothing_happens_other (game, "shake", "shakes");
}


/*
 * lib_cant_do_common()
 * lib_cant_do_object()
 * lib_cant_do_other()
 *
 * Central handler for a range of can't-do messages.  Yet more uninterest-
 * ing responses.
 */
static scr_bool
lib_cant_do_common (scr_gameref_t game,
                    const scr_char *verb, scr_bool is_object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous;

  /* If the target is not an object, end it here. */
  if (!is_object)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You can't ",
                                             "I can't ", "%player% can't "));
      pf_buffer_string (filter, verb);
      pf_buffer_string (filter, " that.\n");
      return TRUE;
    }

  /* Get the referenced object.  If none, return immediately. */
  object = lib_disambiguate_object (game, verb, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Whatever it is, don't do it. */
  pf_buffer_string (filter,
                    lib_select_response (game,
                                         "You can't ",
                                         "I can't ", "%player% can't "));
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}

static scr_bool
lib_cant_do_object (scr_gameref_t game, const scr_char *verb)
{
  return lib_cant_do_common (game, verb, TRUE);
}

static scr_bool
lib_cant_do_other (scr_gameref_t game, const scr_char *verb)
{
  return lib_cant_do_common (game, verb, FALSE);
}


/*
 * lib_cmd_*()
 *
 * Assorted can't-do messages.
 */
scr_bool
lib_cmd_block_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "block");
}

scr_bool
lib_cmd_climb_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "climb");
}

scr_bool
lib_cmd_clean_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "clean");
}

scr_bool
lib_cmd_cut_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "cut");
}

scr_bool
lib_cmd_drink_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "drink");
}

scr_bool
lib_cmd_light_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "light");
}

scr_bool
lib_cmd_lift_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "lift");
}

scr_bool
lib_cmd_move_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "move");
}

scr_bool
lib_cmd_rub_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "rub");
}

scr_bool
lib_cmd_stop_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "stop");
}

scr_bool
lib_cmd_suck_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "suck");
}

scr_bool
lib_cmd_touch_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "touch");
}

scr_bool
lib_cmd_turn_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "turn");
}

scr_bool
lib_cmd_unblock_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "unblock");
}

scr_bool
lib_cmd_wash_object (scr_gameref_t game)
{
  return lib_cant_do_object (game, "wash");
}

scr_bool
lib_cmd_block_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "block");
}

scr_bool
lib_cmd_climb_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "climb");
}

scr_bool
lib_cmd_clean_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "clean");
}

scr_bool
lib_cmd_close_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "close");
}

/*
 * lib_cmd_open_absent()
 * lib_cmd_close_absent()
 *
 * 4.0's openclose() answers for an object it has seen but cannot see now --
 * the open half with the definite name, the close half with the object's own
 * Prefix.  See lib_absent_seen_object().
 */
scr_bool
lib_cmd_open_absent (scr_gameref_t game)
{
  return lib_cant_see_absent_object (game, ".\n", TRUE);
}

scr_bool
lib_cmd_close_absent (scr_gameref_t game)
{
  return lib_cant_see_absent_object (game, ".\n", FALSE);
}

/*
 * lib_cmd_open_other()
 *
 * `open <anything the parser could not resolve to an object>`.  SCARE routed
 * this to lib_what(), for "Open what?", but no Runner has ever printed that
 * for an open: every version composes the same flat refusal its `close` twin
 * does.  Measured, not argued, on the examine-refusal probes:
 *
 *   run390, p39EXAM.taf (3.90), Adrift_41_p39exam.txt / Adrift_43_p39exam.txt --
 *     bare `open`, `open door` (a noun no object bears) and `open statue`
 *     (an object seen in another room, absent from this one) all answer
 *     "You can't open that."
 *   run400, p4EXAM.taf (4.00), Adrift_1_p4exam.txt -- bare `open` and
 *     `open door` answer "You can't open that." too.  (4.0's `open statue`
 *     answers "You can't see the statue." instead, but that is the 4.0
 *     absent-object resolver speaking one layer up, not this handler.)
 *
 * So this is not a version split: it is scrunner.cpp's `open *` row having
 * been asymmetric with the `close *` row sitting directly beneath it.
 * "Open what?" is in run380/390/400's constant pools, but no probe row has
 * ever reached it.
 */
scr_bool
lib_cmd_open_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "open");
}

scr_bool
lib_cmd_lock_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "lock");
}

scr_bool
lib_cmd_unlock_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "unlock");
}

scr_bool
lib_cmd_stand_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "stand on");
}

scr_bool
lib_cmd_sit_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "sit on");
}

scr_bool
lib_cmd_lie_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "lie on");
}

scr_bool
lib_cmd_cut_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "cut");
}

scr_bool
lib_cmd_drink_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "drink");
}

scr_bool
lib_cmd_lift_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "lift");
}

scr_bool
lib_cmd_light_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "light");
}

scr_bool
lib_cmd_move_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "move");
}

scr_bool
lib_cmd_stop_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "stop");
}

scr_bool
lib_cmd_rub_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "rub");
}

scr_bool
lib_cmd_suck_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "suck");
}

scr_bool
lib_cmd_turn_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "turn");
}

scr_bool
lib_cmd_touch_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "touch");
}

scr_bool
lib_cmd_unblock_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "unblock");
}

scr_bool
lib_cmd_wash_other (scr_gameref_t game)
{
  return lib_cant_do_other (game, "wash");
}


/*
 * lib_dont_think_common()
 * lib_dont_think_object()
 * lib_dont_think_other()
 *
 * Central handler for a range of don't_think messages.  Still more
 * uninteresting responses.
 */
static scr_bool
lib_dont_think_common (scr_gameref_t game,
                       const scr_char *verb, scr_bool is_object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;
  scr_bool is_ambiguous;

  /* If the target is not an object, end it here. */
  if (!is_object)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "I don't think you can ",
                                             "I don't think I can ",
                                             "I don't think %player% can "));
      pf_buffer_string (filter, verb);
      pf_buffer_string (filter, " that.\n");
      return TRUE;
    }

  /* Get the referenced object.  If none, return immediately. */
  object = lib_disambiguate_object (game, verb, &is_ambiguous);
  if (object == -1)
    return is_ambiguous;

  /* Whatever it is, don't do it. */
  pf_buffer_string (filter, "I don't think you can ");
  pf_buffer_string (filter, verb);
  pf_buffer_character (filter, ' ');
  lib_print_object_np (game, object);
  pf_buffer_string (filter, ".\n");
  return TRUE;
}

static scr_bool
lib_dont_think_object (scr_gameref_t game, const scr_char *verb)
{
  return lib_dont_think_common (game, verb, TRUE);
}

static scr_bool
lib_dont_think_other (scr_gameref_t game, const scr_char *verb)
{
  return lib_dont_think_common (game, verb, FALSE);
}


/*
 * lib_cmd_*()
 *
 * Assorted don't-think messages.
 */
scr_bool
lib_cmd_fix_object (scr_gameref_t game)
{
  return lib_dont_think_object (game, "fix");
}

scr_bool
lib_cmd_mend_object (scr_gameref_t game)
{
  return lib_dont_think_object (game, "mend");
}

scr_bool
lib_cmd_repair_object (scr_gameref_t game)
{
  return lib_dont_think_object (game, "repair");
}

scr_bool
lib_cmd_fix_other (scr_gameref_t game)
{
  return lib_dont_think_other (game, "fix");
}

scr_bool
lib_cmd_mend_other (scr_gameref_t game)
{
  return lib_dont_think_other (game, "mend");
}

scr_bool
lib_cmd_repair_other (scr_gameref_t game)
{
  return lib_dont_think_other (game, "repair");
}


/*
 * lib_what()
 *
 * Central handler for doing something, but unsure to what.
 */
static scr_bool
lib_what (scr_gameref_t game, const scr_char *verb)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter, verb);
  pf_buffer_string (filter, " what?\n");
  return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Assorted "what?" messages.
 */
scr_bool
lib_cmd_block_what (scr_gameref_t game)
{
  return lib_what (game, "Block");
}

scr_bool
lib_cmd_break_what (scr_gameref_t game)
{
  return lib_what (game, "Break");
}

scr_bool
lib_cmd_destroy_what (scr_gameref_t game)
{
  return lib_what (game, "Destroy");
}

scr_bool
lib_cmd_smash_what (scr_gameref_t game)
{
  return lib_what (game, "Smash");
}

scr_bool
lib_cmd_buy_what (scr_gameref_t game)
{
  return lib_what (game, "Buy");
}

scr_bool
lib_cmd_clean_what (scr_gameref_t game)
{
  return lib_what (game, "Clean");
}

scr_bool
lib_cmd_climb_what (scr_gameref_t game)
{
  return lib_what (game, "Climb");
}

scr_bool
lib_cmd_cut_what (scr_gameref_t game)
{
  return lib_what (game, "Cut");
}

scr_bool
lib_cmd_drink_what (scr_gameref_t game)
{
  return lib_what (game, "Drink");
}

scr_bool
lib_cmd_fix_what (scr_gameref_t game)
{
  return lib_what (game, "Fix");
}

scr_bool
lib_cmd_hit_what (scr_gameref_t game)
{
  return lib_what (game, "Hit");
}

scr_bool
lib_cmd_kick_what (scr_gameref_t game)
{
  return lib_what (game, "Kick");
}

scr_bool
lib_cmd_light_what (scr_gameref_t game)
{
  return lib_what (game, "Light");
}

scr_bool
lib_cmd_lift_what (scr_gameref_t game)
{
  return lib_what (game, "Lift");
}

scr_bool
lib_cmd_mend_what (scr_gameref_t game)
{
  return lib_what (game, "Mend");
}

scr_bool
lib_cmd_move_what (scr_gameref_t game)
{
  return lib_what (game, "Move");
}

scr_bool
lib_cmd_press_what (scr_gameref_t game)
{
  return lib_what (game, "Press");
}

scr_bool
lib_cmd_pull_what (scr_gameref_t game)
{
  return lib_what (game, "Pull");
}

scr_bool
lib_cmd_push_what (scr_gameref_t game)
{
  return lib_what (game, "Push");
}

scr_bool
lib_cmd_repair_what (scr_gameref_t game)
{
  return lib_what (game, "Repair");
}

scr_bool
lib_cmd_sell_what (scr_gameref_t game)
{
  return lib_what (game, "Sell");
}

scr_bool
lib_cmd_shake_what (scr_gameref_t game)
{
  return lib_what (game, "Shake");
}

scr_bool
lib_cmd_rub_what (scr_gameref_t game)
{
  return lib_what (game, "Rub");
}

scr_bool
lib_cmd_stop_what (scr_gameref_t game)
{
  return lib_what (game, "Stop");
}

scr_bool
lib_cmd_suck_what (scr_gameref_t game)
{
  return lib_what (game, "Suck");
}

scr_bool
lib_cmd_touch_what (scr_gameref_t game)
{
  return lib_what (game, "Touch");
}

scr_bool
lib_cmd_turn_what (scr_gameref_t game)
{
  return lib_what (game, "Turn");
}

scr_bool
lib_cmd_unblock_what (scr_gameref_t game)
{
  return lib_what (game, "Unblock");
}

scr_bool
lib_cmd_wash_what (scr_gameref_t game)
{
  return lib_what (game, "Wash");
}

/*
 * 4.0's put/drop list parser (run400 Proc_19_40_459DB4, entered when the
 * line holds the whole word "put" or "drop") names each piece with
 * name_object (Proc_19_41_46E5D8), which resolves it against the objects
 * PRESENT (463640 mode 2) and, when that finds nothing and no task pattern
 * pre-matches the line (453C50), speaks at 46E165-46E18B: "Drop what?" if
 * the line holds the whole word "drop", else "It is not clear which object
 * you are referring to."  Pre-4.0 Runners have no such literal; their
 * `put <absent> in X` stays the flat can't-do tail.  Measured on humbug
 * (Adrift_4_humbug.txt 3407 `Put powder in chute`, powder never taken,
 * the D chute present; 3538 `Put powder in machine`; 3903 `Put sapphire in
 * chute`).  The seen-but-absent clause of lib_cant_see_absent_object()
 * runs first in the Runner too, which is why p4EXAM's `put xyzzy in statue`
 * still answers "You can't see the statue.".
 */
static scr_bool
lib_cmd_unclear_object (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);

  pf_buffer_string (filter,
                    "It is not clear which object you are referring to.\n");
  return TRUE;
}

/*
 * lib_first_named_object_pre_390()
 *
 * The pre-3.9 drop and remove handlers match their noun with co() (run380
 * @42DE60), which tests every object's Short and alias against the line
 * with no regard to where the object is, and then name the first match in
 * index order that they cannot act on (see lacks_pre_390 in the move verb
 * table).  So `drop cash` with the penny (alias "cash") shut in a dresser
 * two rooms away answers "You don't have a penny!" rather than "Drop what?",
 * which run380 keeps (@438FE6) for a line naming nothing at all.  Measured
 * on Crime_Adventure.taf (3.80) in run380, 2026-09-04.  Returns the first
 * object the line names below 3.9, else -1.
 */
static scr_int
lib_first_named_object_pre_390 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    return -1;
  if (!input)
    return -1;
  if (!uip_match ("* %object%", input, game)
      && !uip_match ("* %object% *", input, game))
    return -1;

  for (object = 0; object < gs_object_count (game); object++)
    {
      if (game->object_references[object])
        return object;
    }
  return -1;
}

scr_bool
lib_cmd_drop_what (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int object;

  if (lib_is_version_400 (game)
      && input && !lib_input_contains_word (input, "drop"))
    return lib_cmd_unclear_object (game);

  object = lib_first_named_object_pre_390 (game);
  if (object != -1)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You don't have ",
                                             "I don't have ",
                                             "%player% don't have "));
      lib_print_object_raw (game, object);
      pf_buffer_string (filter, "!\n");
      return TRUE;
    }

  return lib_what (game, "Drop");
}

scr_bool
lib_cmd_put_unclear (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;

  if (!lib_is_version_400 (game) || !input)
    return FALSE;

  /*
   * A first noun that names something present is name_object's success
   * path, and the command belongs to the put handlers and the "I don't
   * understand what you want me to do with" catch-all below (measured:
   * TheADRIFTProject `put batter in remote`, thelasthour `put bowl near
   * spyhole`, both with the first object held).
   */
  if (uip_match ("put %object% *", input, game)
      || uip_match ("put %object%", input, game))
    {
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (game->object_references[index_]
              && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
            return FALSE;
        }
    }

  /*
   * The seen-but-absent clause speaks first: p4EXAM `put xyzzy in statue`
   * from the room next door is "You can't see the statue.", not this.
   * Whether a seen-but-absent FIRST noun also gets that clause is
   * unmeasured (humbug's powder had never been seen); the same rule is
   * applied to both.
   */
  if (uip_match ("* %object% *", input, game)
      || uip_match ("* %object%", input, game))
    {
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (game->object_references[index_]
              && gs_object_seen (game, index_))
            return FALSE;
        }
    }

  return lib_cmd_unclear_object (game);
}

scr_bool
lib_cmd_get_what (scr_gameref_t game)
{
  return lib_what (game, "Take");
}

scr_bool
lib_cmd_give_what (scr_gameref_t game)
{
  return lib_what (game, "Give");
}

scr_bool
lib_cmd_remove_what (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object;

  /* run380 @4300C8, the same first-named-object rule as drop above. */
  object = lib_first_named_object_pre_390 (game);
  if (object != -1)
    {
      pf_buffer_string (filter,
                        lib_select_response (game,
                                             "You are not wearing ",
                                             "I am not wearing ",
                                             "%player% is not wearing "));
      lib_print_object_raw (game, object);
      pf_buffer_string (filter, "!\n");
      return TRUE;
    }

  return lib_what (game, "Remove");
}

scr_bool
lib_cmd_wear_what (scr_gameref_t game)
{
  return lib_what (game, "Wear");
}

scr_bool
lib_cmd_lock_what (scr_gameref_t game)
{
  return lib_what (game, "Lock");
}

scr_bool
lib_cmd_unlock_what (scr_gameref_t game)
{
  return lib_what (game, "Unlock");
}



/*
 * lib_verb_object_resolve_400()
 *
 * The 4.0 Runner decides which object an otherwise unhandled line is
 * "about" before any verb runs: generaltasks calls the noun resolver
 * Proc_21_58_463640 at 48A3F5 (mode 0: candidates are the objects both
 * PRESENT and seen) and parks the answer in MemVar_4942F8.  The resolver
 * scores every candidate by the words of its name the line contains --
 * one for the Short as a whole word, one more for any alias (4632D3: the
 * alias loop runs whether or not the Short hit), and one more for each
 * word of the Prefix found (4632A9-463387, whole-word test
 * Proc_21_38_454CB0) -- and keeps the unique maximum.  thelasthour's
 * spyhole (Short "spyhole", alias "spyhole") outscores its bowl 2 to 1 on
 * `put bowl near spyhole` (Adrift_111); Shadowpeak's "cell door" scores 0
 * on `put sword near cell door`, "door" being no alias of it
 * (Adrift_112).  Two candidates with different names and
 * the same score leave it with a negative index (4633C3-463405), and the
 * "I don't understand what you want me to do with" catch-all at
 * 48B1B0-48B236 needs MemVar_4942F8 > -1, so a tie prints NOTHING and the
 * line falls to the game's DontUnderstand text.
 *
 * Measured on House (4.00) from the fireplace, run400 Adrift_105/106
 * 2026-09-06: `throw diary at cathy`, `throw diary at zzz`, `wibble diary`
 * and `wibble diary cathy` all answer "I don't understand what you want
 * me to do with the diary." (an NPC is not a candidate), while `throw
 * diary at fireplace`, `throw fireplace at diary`, `throw diary at hook`,
 * `throw fireplace diary` and `wibble diary fireplace` all print "What?",
 * House's [error=6] DontUnderstand -- with the fireplace freshly examined
 * or not.  Scarier used to speak for the first object its `* %object% *`
 * row bound.
 *
 * Returns the resolver's object, -1 for a tie, -2 when nothing scored.
 * 3.8 (run380 442F5D) instead walks the objects in index order and speaks
 * for the first present, seen one -- unmeasured, and left to the caller's
 * own choice.
 */
static scr_int
lib_verb_object_name_score (scr_gameref_t game,
                            scr_int object, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname, *prefix;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias, score;
  scr_char *copy, *word, *next;

  score = 0;
  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (shortname && shortname[0] != NUL
      && lib_input_contains_word (input, shortname))
    score = 1;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Alias";
  alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *alias_name;

      vt_key[3].integer = alias;
      alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (alias_name && alias_name[0] != NUL
          && lib_input_contains_word (input, alias_name))
        {
          score++;
          break;
        }
    }
  if (score == 0)
    return 0;

  /*
   * The 4.0 loader stores "a" for an empty Prefix (run400 4900EC), so an
   * object authored without one scores the word "a" like any "a X" object:
   * House's fireplace ties the diary on `throw a diary at fireplace` and
   * `throw diary at a fireplace` (Adrift_107/108), and matches the hook's
   * "the" on `throw a fireplace at the hook` (Adrift_109).
   */
  prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
  if (!prefix || prefix[0] == NUL)
    prefix = "a";

  copy = (scr_char *) scr_malloc (strlen (prefix) + 1);
  strcpy (copy, prefix);
  for (word = copy; word; word = next)
    {
      next = strchr (word, ' ');
      if (next)
        *next++ = NUL;
      if (word[0] != NUL && lib_input_contains_word (input, word))
        score++;
    }
  scr_free (copy);
  return score;
}

static scr_int
lib_verb_object_resolve_400_common (scr_gameref_t game,
                                    std::vector<scr_int> *tied)
{
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_, object, best, best_count;

  if (tied)
    tied->clear ();
  if (!input)
    return -2;

  /*
   * An answer to an ambiguity prompt names the object outright, so the
   * re-run of the original command cannot tie again; see
   * lib_co_400_answer_object().
   */
  if (lib_co_400_forced () >= 0)
    return lib_co_400_forced ();

  object = -2;
  best = 0;
  best_count = 0;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      scr_int score;

      if (!gs_object_seen (game, index_)
          || !obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        continue;

      score = lib_verb_object_name_score (game, index_, input);
      if (score == 0)
        continue;
      if (score > best)
        {
          object = index_;
          best = score;
          best_count = 1;
          if (tied)
            {
              tied->clear ();
              tied->push_back (index_);
            }
        }
      else if (score == best)
        {
          best_count++;
          if (tied)
            tied->push_back (index_);
        }
    }

  return best_count > 1 ? -1 : object;
}

static scr_int
lib_verb_object_resolve_400 (scr_gameref_t game)
{
  return lib_verb_object_resolve_400_common (game, NULL);
}

/*
 * lib_put_where_400()
 * lib_cmd_put_where_400()
 *
 * The 4.0 put parser (Proc 459DB4, branch 46DC34-46DD2C) takes a line
 * that starts "put", has neither " in " nor " on " in it and no word
 * "down", and that no task pre-matched (453C50 = 0): it asks the same noun
 * resolver as the catch-all (46DCC7, mode 0) and answers "Where do you
 * want to put <the object>?" for a unique winner (46DCDB) or "Where do
 * you want to put that?" (46DD19) for a tie or nothing scoring.  Both set
 * MemVar_494281 (46DD25), so like the catch-all the answer is not a turn.
 *
 * Measured 2026-09-06: thelasthour `put bowl near spyhole` answers "Where
 * do you want to put the spyhole?" and no event fires (Adrift_111);
 * Shadowpeak `put sword near cell door` answers "Where do you want to put
 * the sword?" and Seeker stays quiet (Adrift_112).  Scarier's catch-all
 * used to answer "I don't understand what you want me to do with the
 * bowl." and tick.
 *
 * Only lines the can't-see clause above lets through get here, which is
 * the Runner's order too (therest 4887C1 runs before the put parser: see
 * lib_cmd_unclear_object).  A line with "into"/"onto" is left to the put
 * handlers, as the Runner's " in "/" on " tests may be preceded by a
 * rewrite we have not read.
 */
static scr_bool
lib_is_put_where_line_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!lib_is_version_400 (game) || !input)
    return FALSE;
  return scr_strncasecmp (input, "put ", 4) == 0
         && !strstr (input, " in ") && !strstr (input, " on ")
         && !strstr (input, " into ") && !strstr (input, " onto ")
         && !lib_input_contains_word (input, "down");
}

static scr_bool
lib_put_where_400 (scr_gameref_t game, scr_int resolved)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (!lib_is_put_where_line_400 (game))
    return FALSE;

  if (resolved >= 0)
    {
      var_set_ref_object (gs_get_vars (game), resolved);
      lib_print_wrapped_object (game, "Where do you want to put ",
                                resolved, "?\n");
    }
  else
    pf_buffer_string (filter, "Where do you want to put that?\n");

  game->is_admin = TRUE;
  return TRUE;
}

scr_bool
lib_cmd_put_where_400 (scr_gameref_t game)
{
  return lib_put_where_400 (game, lib_verb_object_resolve_400 (game));
}

/*
 * lib_cmd_verb_object()
 * lib_cmd_verb_character()
 *
 * Handlers for unrecognized verbs with known object/NPC.
 */
scr_bool
lib_cmd_verb_object (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, object, index_;

  /* Ensure the reference is unambiguous. */
  count = 0;
  object = -1;
  for (index_ = 0; index_ < gs_object_count (game); index_++)
    {
      if (game->object_references[index_]
          && (gs_object_seen (game, index_)
              || !lib_matcher_requires_seen (game))
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          object = index_;
        }
    }

  /*
   * An answer to a 4.0 ambiguity prompt re-runs this line with its object
   * already picked; see lib_co_400_answer_object().
   */
  if (lib_co_400_forced () >= 0)
    {
      count = 1;
      object = lib_co_400_forced ();
    }

  if (count != 1)
    {
      /*
       * 4.0: two present objects the player named by their shared Short is
       * the Runner's ambiguity prompt, and it comes out here -- our
       * positional matcher binds both trees for `chop tree`, so the count is
       * 2 and the 4.0 resolver below is never reached.  A bare `put` is left
       * alone: lib_cmd_put_where_400() answers it with "Where do you want to
       * put that?", which is what the tie was measured to give there.
       */
      if (lib_is_version_400 (game) && !lib_is_put_where_line_400 (game))
        {
          std::vector<scr_int> tied;

          if (lib_verb_object_resolve_400_common (game, &tied) == -1
              && lib_co_400_raise_for_short_tie (game, tied))
            return TRUE;
        }

      /*
       * No object of that name is here.  Before giving up on the command --
       * which hands it to the game's DontUnderstand text -- see whether the
       * name refers unambiguously to an object that is simply elsewhere, and
       * if it does, say so.  Every Runner from 3.7 to 4.0 answers an
       * otherwise unhandled command this way: therest() resolves a noun,
       * and when obhere() says the object is not present it prints
       * "<player> can't see <the object>." and returns without running any
       * of the verb branches below it (run370 43D169 @Form1.frm:3336,
       * run380 443C6A, run400 4887C1 @mdlSpreadTheLoad.bas:41196).  The
       * clause is reached only when nothing else produced output, which is
       * exactly where we are.
       *
       * Measured on hauntedhouse.taf (Adrift_16_hauntedhouse.txt, turn 34):
       * "melt statue" from the Front porch, with the statue in the Entrance,
       * answers "You can't see the statue." and not the game's own
       * DontUnderstand text.
       */
      count = 0;
      object = -1;
      for (index_ = 0; index_ < gs_object_count (game); index_++)
        {
          if (game->object_references[index_]
              && (gs_object_seen (game, index_)
                  || !lib_matcher_requires_seen (game)))
            {
              count++;
              object = index_;
            }
        }
      if (count != 1)
        return FALSE;

      var_set_ref_object (vars, object);
      lib_print_response_object (game,
                                 "You can't see ",
                                 "I can't see ",
                                 "%player% can't see ",
                                 object, ".\n");
      return TRUE;
    }

  /*
   * 4.0: the object is the resolver's, not the first one our `* %object% *`
   * row bound, and a tie between two present objects' names means the
   * Runner says nothing here -- see lib_verb_object_resolve_400().
   */
  if (lib_is_version_400 (game))
    {
      std::vector<scr_int> tied;
      const scr_int resolved =
          lib_verb_object_resolve_400_common (game, &tied);

      if (lib_put_where_400 (game, resolved))
        return TRUE;
      if (resolved == -1)
        {
          /*
           * A tie the player named by Short is the Runner's own ambiguity
           * prompt -- `chop tree` with two trees called "tree" asks "Which
           * tree.  The red tree or the blue tree?", while the alias ties
           * `chop shed` and `chop keys` fall through to DontUnderstand as
           * before.  See lib_co_400_raise().
           */
          if (lib_co_400_raise_for_short_tie (game, tied))
            return TRUE;
          return FALSE;
        }
      if (resolved >= 0)
        object = resolved;
    }

  /*
   * 4.0: a task that has just ended the game takes this answer off the line.
   * run400's generaltasks tests the gameover byte right after the turn
   * counter -- `If MemVar_4941AD <> 0 Then GoTo loc_48B4E3` at loc_48AC62 --
   * and 48B4E3 is the tail of the routine, past the object-counting loop at
   * 48AFF0 and past this catch-all at 48B19A.  The verb branches ahead of
   * 48AC62 (open/close 48A515, movement 48A5D8, wear, remove, look, the wait
   * loop) all still run, so an ending does not silence the library as such;
   * only the unhandled-verb tail is lost, and what the tail does print is
   * characters() (48B56E) and then, with the message buffer still empty and
   * no character named, the game's DontUnderstand text (48B58F).
   *
   * Measured live on relojero.taf (4.00, run400 Adrift_909.txt, 2026-09-07):
   * from the state the walkthrough reaches at `x trozo`, holding both the
   * Phoenix and the broken cord, `pulir fenix`, bare `fenix`, `pulir trozo`,
   * `tirar fenix`, `dar fenix` and `poner fenix` all get this catch-all
   * (which the game's ALR rewrites to "Extranos pensamientos afloran en mi
   * mente a proposito de <object>."), while `arreglar fenix` -- the one line
   * of the seven that matches a task, T5, which is silent and whose only
   * action is End Game (win) -- answers "Disculpa pero no te entiendo.", the
   * game's DontUnderstand from plain line 25, and only then prints the
   * WinText.
   *
   * It is the ending and not the task that does this.  A silent task on its
   * own leaves the catch-all family alone: seaside's T25 `do form` runs task
   * 3 (hides two objects, moves the form to the player, scores) and run400
   * still answers with an object message, "You must be in the same room as
   * the leisure access card form to be able to do anything with it."
   * (Adrift_236_seaside.txt:123).
   *
   * Returning FALSE hands the line to `put *` and `* %character% *` and then
   * to the DontUnderstand text in run_process_input_line(), which is the
   * tail's order too.
   */
  if (lib_is_version_400 (game) && game->pending_endgame != 0)
    return FALSE;

  /* Save in variables. */
  var_set_ref_object (vars, object);

  /*
   * 4.0: this answer is not a turn.  The catch-all at run400 48B19A-48B30A
   * stores 1 in MemVar_494281 (48B232), and the end-of-turn walk+event tick
   * at 48B599 runs only while that flag is 0; the DontUnderstand path a tie
   * takes (48B573) jumps past the tick too.  So neither answer moves an NPC
   * or advances an event.  Shadowpeak's second `attack margo with sword`
   * used to be followed by "Seeker hums!" here; run400 is silent.
   */
  if (lib_is_version_400 (game))
    game->is_admin = TRUE;

  /*
   * Print don't understand message.  run400's generaltasks composes this
   * object as the antecedent in the definite form (loc_48A409-48A42E):
   * `throw shovel` then `x it` echoes "(the shovel)" -- see
   * uip_definite_form() in scparser.cpp.
   */
  uip_note_definite_reference ();
  lib_print_wrapped_object (game, "I don't understand what you want me to do with ",
                            object, ".\n");
  lib_co_400_note_refusal ();
  return TRUE;
}

scr_bool
lib_cmd_verb_npc (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, npc, index_;

  /*
   * 4.0: the ending takes the character catch-all off the line as well.
   * This message is the tail of run400's characters() (Proc_19_0_480674,
   * printed at 480603), and the instruction before it is
   * `loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub` -- the same gameover
   * byte the object catch-all above is gated on, tested a second time
   * because characters() is called from the routine's tail (48B56E), below
   * the jump at 48AC62.
   *
   * Measured live on easter.taf (run400 Adrift_273_easter.txt:304-308,
   * 2026-09-07): `show basket to shopkeeper`, the winning move, prints the
   * task's text and the WinText and nothing in between -- no "I don't
   * understand what you want to do with shopkeeper.", and no DontUnderstand
   * either, because the line names a character (48B573's var_29C).
   */
  if (lib_is_version_400 (game) && game->pending_endgame != 0)
    return FALSE;

  /* Ensure the reference is unambiguous. */
  count = 0;
  npc = -1;
  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (game->npc_references[index_]
          && gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          npc = index_;
        }
    }
  if (count != 1)
    return FALSE;

  /* Save in variables. */
  var_set_ref_character (vars, npc);

  /*
   * 4.0: like the object catch-all above, this answer is not a turn.  The
   * character pass stores 1 in MemVar_494281 -- the not-a-turn flag the
   * walk+event tick at 48B599 is gated on -- as the last thing it does with
   * this message (run400 loc_48061A-48061E), and neither of the two
   * neighbouring branches does: `" is not here!"` at 480640 and `"Who?"` at
   * 480659 both fall straight through to 480660.
   *
   * Measured on p4REPEAT3.taf (run400 Adrift_952.txt, 2026-09-08): every
   * line of that probe is followed by the every-turn event's "TICK." except
   * the two `bob, hello` turns, which print "I don't understand what you
   * want to do with Bob." and the game's DontUnderstand text and stop.
   */
  if (lib_is_version_400 (game))
    game->is_admin = TRUE;

  /* Print don't understand message; unlike objects, there's no "me" here. */
  lib_print_wrapped_npc (game, "I don't understand what you want to do with ",
                         npc, ".\n");
  return TRUE;
}


/*
 * lib_debug_trace()
 *
 * Set library tracing on/off.
 */
void
lib_debug_trace (scr_bool flag)
{
  lib_trace = flag;
}

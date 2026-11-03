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
 * Module notes:
 *
 * o Gender enumerations are 0/1/2, but 1/2/3 in jAsea.  The 0/1/2 values
 *   seem to be right.  Is jAsea off by one?
 *
 * o jAsea tries to read Globals.CompileDate.  It's just CompileDate.
 *
 * o State_ and obstate are implemented, but not fully tested due to a lack
 *   of games that use them.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <set>
#include <string>
#include <vector>
#include <vector>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "scvars.h"
#include "scgamest_api.h"
#include "scparser.h"
#include "sclibrar_api.h"
#include "scnpcs.h"
#include "scobjcts.h"
#include "sclocale.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_uint VARS_MAGIC = 0xabcc7a71;

/* Variables trace flag. */
static scr_bool var_trace = FALSE;

/*
 * var_is_clock_frozen()
 *
 * Determinism/testing mode -- the same switch that selects the portable,
 * predictable congruential RNG via scr_set_portable_random() (the os layers
 * enable both together: os_ansi via SCR_STABLE_RANDOM_ENABLED, os_glk via
 * Spatterlight's determinism mode) -- also freezes the real-time clock.
 *
 * ADRIFT's %time% / elapsed-seconds system variable is
 * difftime(time(NULL), game_start), genuinely non-reproducible: it makes
 * day/night-style displays flip across runs depending on how a replay straddles
 * a wall-clock second boundary.  Freezing it (delta == 0, so only time_offset
 * remains) makes scripted regression replays byte-stable, exactly as seeding
 * the RNG does.  When determinism is off, faithful real-time behaviour (as in
 * the reference Runner) is retained.
 */
static scr_bool
var_is_clock_frozen (void)
{
  return scr_is_congruential_random ();
}

/* Table of numbers zero to twenty spelled out. */
enum { VAR_NUMBERS_SIZE = 21 };
static const scr_char *const VAR_NUMBERS[VAR_NUMBERS_SIZE] = {
  "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
  "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
  "sixteen", "seventeen", "eighteen", "nineteen", "twenty"
};

/* Variable entry, held on a list hashed by variable name. */
typedef struct scr_var_s
{
  struct scr_var_s *next;

  const scr_char *name;
  scr_int type;
  scr_vartype_t value;
} scr_var_t;
typedef scr_var_t *scr_varref_t;

/*
 * Variables set structure.  A self-contained set of variables on which
 * variables functions operate.  211 is prime, making it a reasonable hash
 * divisor.  There's no rehashing here; few games, if any, are likely to
 * exceed a fill factor of two (~422 variables).
 */
enum { VAR_HASH_TABLE_SIZE = 211 };
typedef struct scr_var_set_s
{
  scr_uint magic;
  scr_prop_setref_t bundle;
  scr_int referenced_character;
  scr_int referenced_object;
  scr_int referenced_number;
  scr_bool is_number_referenced;
  scr_char *referenced_text;
  scr_char *temporary;
  time_t timestamp;
  scr_uint time_offset;
  scr_gameref_t game;
  scr_varref_t variable[VAR_HASH_TABLE_SIZE];
} scr_var_set_t;


/*
 * var_is_valid()
 *
 * Return TRUE if pointer is a valid variables set, FALSE otherwise.
 */
static scr_bool
var_is_valid (scr_var_setref_t vars)
{
  return vars && vars->magic == VARS_MAGIC;
}


/*
 * var_hash_name()
 *
 * Hash a variable name, modulo'ed to the number of buckets.
 */
static scr_uint
var_hash_name (const scr_char *name)
{
  return scr_hash (name) % VAR_HASH_TABLE_SIZE;
}


/*
 * var_create_empty()
 *
 * Create and return a new empty set of variables.
 */
static scr_var_setref_t
var_create_empty (void)
{
  scr_var_setref_t vars;
  scr_int index_;

  /* Create a clean set of variables. */
  vars = (decltype(vars)) scr_malloc (sizeof (*vars));
  vars->magic = VARS_MAGIC;
  vars->bundle = nullptr;
  vars->referenced_character = -1;
  vars->referenced_object = -1;
  vars->referenced_number = 0;
  vars->is_number_referenced = FALSE;
  vars->referenced_text = nullptr;
  vars->temporary = nullptr;
  vars->timestamp = time (nullptr);
  vars->time_offset = 0;
  vars->game = nullptr;

  /* Clear all variable hash lists. */
  for (index_ = 0; index_ < VAR_HASH_TABLE_SIZE; index_++)
    vars->variable[index_] = nullptr;

  return vars;
}


/*
 * var_destroy()
 *
 * Destroy a variable set, and free its heap memory.
 */
void
var_destroy (scr_var_setref_t vars)
{
  scr_int index_;
  assert (var_is_valid (vars));

  /*
   * Free the content of each string variable, and variable entry.  String
   * variable content needs to use mutable string instead of const string.
   */
  for (index_ = 0; index_ < VAR_HASH_TABLE_SIZE; index_++)
    {
      scr_varref_t var, next;

      for (var = vars->variable[index_]; var; var = next)
        {
          next = var->next;
          if (var->type == VAR_STRING)
            scr_free (var->value.mutable_string);
          scr_free (var);
        }
    }

  /* Free any temporary and reference text storage area. */
  scr_free (vars->temporary);
  scr_free (vars->referenced_text);

  /* Poison and free the variable set itself. */
  memset (vars, 0xaa, sizeof (*vars));
  scr_free (vars);
}


/*
 * var_find()
 * var_add()
 *
 * Find and return a pointer to a named variable structure, or NULL if no such
 * variable exists, and add a new variable structure to the lists.
 */
static scr_varref_t
var_find (scr_var_setref_t vars, const scr_char *name)
{
  scr_uint hash;
  scr_varref_t var;

  /* Hash name, search list and return if name match found. */
  hash = var_hash_name (name);
  for (var = vars->variable[hash]; var; var = var->next)
    {
      if (strcmp (name, var->name) == 0)
        break;
    }

  /* Return variable, or NULL if no such variable. */
  return var;
}

static scr_varref_t
var_add (scr_var_setref_t vars, const scr_char *name, scr_int type)
{
  scr_varref_t var;
  scr_uint hash;

  /* Create a new variable entry. */
  var = (decltype(var)) scr_malloc (sizeof (*var));
  var->name = name;
  var->type = type;
  var->value.voidp = nullptr;

  /* Hash its name, and insert it at start of the relevant list. */
  hash = var_hash_name (name);
  var->next = vars->variable[hash];
  vars->variable[hash] = var;

  return var;
}


/*
 * var_get_scarier_version()
 *
 * Return the value of %scarier_version%.  Used to generate the system version
 * of this variable, and to re-initialize user versions initialized to zero.
 */
static scr_int
var_get_scarier_version (void)
{
  scr_int major, minor, point, version;

  if (sscanf (SCARIER_VERSION, "%ld.%ld.%ld", &major, &minor, &point) != 3)
    {
      scr_error ("var_get_scarier_version: unable to generate scarier_version\n");
      return 0;
    }

  version = major * 10000 + minor * 100 + point;
  return version;
}


/*
 * var_put()
 *
 * Store a variable type in a named variable.  If not present, the variable
 * is created.  Type is one of 'I' or 'S' for integer or string.
 */
static void
var_put (scr_var_setref_t vars,
         const scr_char *name, scr_int type, scr_vartype_t vt_value)
{
  scr_varref_t var;
  scr_bool is_modification;
  assert (var_is_valid (vars));
  assert (name);

  /* Check type is either integer or string. */
  switch (type)
    {
    case VAR_INTEGER:
    case VAR_STRING:
      break;

    default:
      scr_fatal ("var_put: invalid variable type, %ld\n", type);
    }

  /* See if the user variable already exists. */
  var = var_find (vars, name);
  if (var)
    {
      /* Verify that nothing is trying to change the variable's type. */
      if (var->type != type)
        scr_fatal ("var_put: variable type changed, %s\n", name);

      /*
       * Special case %scarier_version%.  If a game changes its value, it may
       * compromise version checking, so warn here, but continue.
       */
      if (strcmp (name, "scarier_version") == 0)
        {
          if (var->value.integer != vt_value.integer)
            scr_error ("var_put: warning: %%%s%% value changed\n", name);
        }

      is_modification = TRUE;
    }
  else
    {
      /*
       * Special case %scarier_version%.  If a game defines this and initializes
       * it to zero, re-initialize it to Scarier's version number.  Games that
       * define %scarier_version%, initially zero, can use this to test if
       * running under Scarier or Runner.
       */
      if (strcmp (name, "scarier_version") == 0 && vt_value.integer == 0)
        {
          vt_value.integer = var_get_scarier_version ();

          if (var_trace)
            scr_trace ("Variable: %%%s%% [new] caught and mapped\n", name);
        }

      /*
       * Create a new and empty variable entry.  The mutable string needs to
       * be set to NULL here so that realloc works correctly on assigning
       * the value below.
       */
      var = var_add (vars, name, type);
      var->value.mutable_string = nullptr;

      is_modification = FALSE;
    }

  /* Update the existing variable, or populate the new one fully. */
  switch (var->type)
    {
    case VAR_INTEGER:
      var->value.integer = vt_value.integer;
      break;

    case VAR_STRING:
      /* Use mutable string instead of const string. */
      var->value.mutable_string
        = (scr_char *) scr_realloc (var->value.mutable_string,
                                    strlen (vt_value.string) + 1);
      memcpy (var->value.mutable_string, vt_value.string,
              strlen (vt_value.string) + 1);
      break;

    default:
      scr_fatal ("var_put: invalid variable type, %ld\n", var->type);
    }

  if (var_trace)
    {
      scr_trace ("Variable: %%%s%%%s = ",
                name, is_modification ? "" : " [new]");
      switch (var->type)
        {
        case VAR_INTEGER:
          scr_trace ("%ld", var->value.integer);
          break;
        case VAR_STRING:
          scr_trace ("\"%s\"", var->value.string);
          break;

        default:
          scr_trace ("[invalid variable type, %ld]", var->type);
          break;
        }
      scr_trace ("\n");
    }
}


/*
 * var_append_temp()
 *
 * Helper for object listers.  Extends temporary, and appends the given text
 * to the string.
 */
static void
var_append_temp (scr_var_setref_t vars, const scr_char *string)
{
  scr_bool new_sentence;
  scr_int noted;

  if (!vars->temporary)
    {
      /* Create a new temporary area and copy string. */
      new_sentence = TRUE;
      noted = 0;
      vars->temporary = scr_strdup (string);
    }
  else
    {
      /* Append string to existing temporary; `noted` is already the length to
         append at, and is reused below to case the first new character. */
      new_sentence = (vars->temporary[0] == NUL);
      noted = strlen (vars->temporary);
      {
        size_t size = (size_t) noted + strlen (string) + 1;

        vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, size);
        snprintf (vars->temporary + noted, size - (size_t) noted, "%s", string);
      }
    }

  if (new_sentence)
    vars->temporary[noted] = scr_toupper (vars->temporary[noted]);
}


/*
 * var_clear_temp()
 * var_set_temp()
 *
 * Empty temporary ahead of var_append_temp() calls, or replace it with a
 * copy of `string`.
 */
static void
var_clear_temp (scr_var_setref_t vars)
{
  vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 1);
  vars->temporary[0] = NUL;
}

static void
var_set_temp (scr_var_setref_t vars, const scr_char *string)
{
  vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, strlen (string) + 1);
  memcpy (vars->temporary, string, strlen (string) + 1);
}


/*
 * var_number_text()
 *
 * The %t_...% spelling of a number: its word from zero to twenty, otherwise
 * its digits, formatted into temporary.
 */
static const scr_char *
var_number_text (scr_var_setref_t vars, scr_int number)
{
  const scr_char *word = var_number_word (number);

  if (word)
    return word;

  vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 32);
  snprintf (vars->temporary, 32, "%ld", number);
  return vars->temporary;
}


/*
 * var_openness_word()
 *
 * "open", "closed" or "locked" for an openable object's openness, `unknown`
 * for anything else.
 */
static const scr_char *
var_openness_word (scr_gameref_t game, scr_int object, const scr_char *unknown)
{
  switch (gs_object_openness (game, object))
    {
    case OBJ_OPEN:
      return "open";
    case OBJ_CLOSED:
      return "closed";
    case OBJ_LOCKED:
      return "locked";
    default:
      return unknown;
    }
}


/*
 * var_object_is_stateful()
 * var_set_temp_state()
 *
 * Whether an object has states at all, and copy its current state name into
 * temporary (FALSE if it has none to give).
 */
static scr_bool
var_object_is_stateful (scr_prop_setref_t bundle, scr_int object)
{
  return prop_get_indexed_integer (bundle, "Objects", object,
                                   "CurrentState") != 0;
}

static scr_bool
var_set_temp_state (scr_gameref_t game, scr_var_setref_t vars, scr_int object)
{
  scr_char *state = obj_state_name (game, object);

  if (!state)
    return FALSE;
  var_set_temp (vars, state);
  scr_free (state);
  return TRUE;
}


/*
 * var_print_object_np
 * var_print_object
 *
 * Convenience functions to append an object's name, with "the" and with its
 * own prefix, to variables temporary.  The definite form is the library's
 * (lib_definite_object_name), which is also what %theobject% expands to.
 */
static void
var_print_object_np (scr_gameref_t game, scr_int object)
{
  var_append_temp (gs_get_vars (game),
                   lib_definite_object_name (gs_get_bundle (game),
                                             object).c_str ());
}

static void
var_print_object (scr_gameref_t game, scr_int object)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix, *name;

  /*
   * Get the object's prefix.  As with the library, if the prefix is empty,
   * put in an "a ".
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  if (!scr_strempty (prefix))
    {
      var_append_temp (vars, prefix);
      var_append_temp (vars, " ");
    }
  else
    var_append_temp (vars, "a ");

  /* Print the object's name. */
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  var_append_temp (vars, name);
}


/*
 * var_select_plurality()
 *
 * Convenience function for listers.  Selects one of two responses depending
 * on whether an object appears singular or plural.
 */
static const scr_char *
var_select_plurality (scr_gameref_t game, scr_int object,
                      const scr_char *singular, const scr_char *plural)
{
  return obj_appears_plural (game, object) ? plural : singular;
}


/*
 * var_print_list()
 *
 * Print a gathered list of objects as "a, b and c".  The listers below
 * collect first and print afterwards, so that the phrase introducing the
 * list can be picked from the finished list.
 */
typedef std::vector<scr_int> var_list_t;

static void
var_print_list (scr_gameref_t game, const var_list_t &list)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  size_t index_;

  for (index_ = 0; index_ < list.size (); index_++)
    {
      if (index_ > 0)
        var_append_temp (vars, index_ == list.size () - 1 ? " and " : ", ");
      var_print_object (game, list[index_]);
    }
}


/*
 * var_select_list_plurality()
 *
 * " is " or " are " for the prefixed "Inside/On <object> is <list>." form:
 * plural for two or more objects, else by the one object's plurality.
 */
static const scr_char *
var_select_list_plurality (scr_gameref_t game, scr_int associate,
                           const var_list_t &list)
{
  if (lib_alrs_see_list_verb (game, associate))
    return " is ";
  if (list.size () > 1)
    return " are ";
  return var_select_plurality (game, list[0], " is ", " are ");
}


/*
 * var_use_alternate_format()
 *
 * Pick between the Runner's two listing styles for the contents of a
 * container or a surface.  This is the same routine the room and examine
 * listers go through -- run400 lists a container's or a surface's contents
 * from one place, 0006A418, and %in_<object>%, %on_<object>% and the library
 * listers all end up there -- so the choice is made on the same rule:
 * one or two objects get the alternate (postfixed) "<list> is/are inside
 * <cont>." format, three or more the normal (prefixed) "Inside <cont> is
 * <list>." one, and before 3.9 the alternate format does not exist at all.
 * See lib_list_in_object() in sclibrar_examine.cpp for the derivation and
 * for the live measurements behind it.
 *
 * The variables used to take the alternate format unconditionally.  Measured
 * live in run400 under Wine 2026-08-24 on WhereAreMyKeys.taf, whose fridge is
 * opened by a task whose CompleteText ends "%in_fridge%": with three objects
 * in it the Runner answers "You open the fridge and the light comes on.  Well
 * that's something. Inside the fridge is a tub of butter, a butter knife and
 * a bottle of milk." (runner_probes/where_are_my_keys.run400.txt), where we
 * printed "A tub of butter, a butter knife and a bottle of milk are inside the
 * fridge."  The same replay shows the two-object case keeping the alternate
 * format, from the library lister: `open unit` -> "A large knife and a jar of
 * coffee are inside the kitchen unit."
 */
static scr_bool
var_use_alternate_format (scr_gameref_t game, scr_int associate, size_t count)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_taf_version (bundle) < TAF_VERSION_390)
    return FALSE;
  if (count == 1 || count == 2)
    return TRUE;

  return obj_is_static (game, associate)
         && gs_object_position (game, associate) == OBJ_PART_NPC;
}


/*
 * var_list_at_object()
 * var_list_in_object()
 * var_list_on_object()
 *
 * List the objects held in a given container object, or standing on a given
 * surface object.  `position` picks which, `prefix` introduces the normal
 * format, and `singular` and `plural` are the phrase joining the list to the
 * associate in the alternate one.
 *
 * Whatever is listed is seen from then on, as with the library's own
 * listers (lib_reveal_children()).  run400 fills %in_%, %on_% and %onin_%
 * through whatisinon (46A950), which writes the seen byte for each object it
 * names, in the on-half at 46A376 and the in-half at 46A81E.  Darkness and
 * Fire in the Blood describe a table with "%on_round table%" and then expect
 * a plain `get plate` to work (runner_transcripts darkness, 2026-10-09).
 */
static var_list_t
var_collect_at_object (scr_gameref_t game, scr_int associate, scr_int position)
{
  var_list_t list;
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      /* Contained, or standing on? */
      if (gs_object_position (game, object) == position
          && gs_object_parent (game, object) == associate)
        {
          gs_set_object_seen (game, object, TRUE);
          list.push_back (object);
        }
    }
  return list;
}

static void
var_list_at_clause (scr_gameref_t game, scr_int associate,
                    const var_list_t &list, const scr_char *prefix,
                    const scr_char *singular, const scr_char *plural)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  if (var_use_alternate_format (game, associate, list.size ()))
    {
      var_print_list (game, list);
      var_append_temp (vars,
                       list.size () == 1
                       ? var_select_plurality (game, list[0], singular, plural)
                       : plural);

      /* Print out the container or surface. */
      var_print_object_np (game, associate);
    }
  else
    {
      /*
       * The Runner's " is " is a literal whatever the count; Scarier
       * keeps agreement -- see lib_list_in_object_normal().
       */
      var_append_temp (vars, prefix);
      var_print_object_np (game, associate);
      var_append_temp (vars, var_select_list_plurality (game, associate, list));
      var_print_list (game, list);
    }
}

static void
var_list_at_object (scr_gameref_t game, scr_int associate, scr_int position,
                    const scr_char *prefix,
                    const scr_char *singular, const scr_char *plural)
{
  const var_list_t list = var_collect_at_object (game, associate, position);

  /* List out the objects held by this object. */
  if (!list.empty ())
    {
      var_list_at_clause (game, associate, list, prefix, singular, plural);
      var_append_temp (gs_get_vars (game), ".");
    }
}

/*
 * var_shows_contents()
 *
 * Only an open container lists its contents.  run390 fills %in_<object>%
 * through whatisinon() (loop at 0045B3CC), whose in-branch (00443A46)
 * requires the container flag and an openness other than closed, the same
 * gate as the room lister in lib_list_in_on_object().  thewill
 * (runner_transcripts/thewill.txt): the Hallway's "%in_clock%" prints
 * nothing while the clock is shut, where we listed the pocket watch
 * "inside the open grandfather clock".
 *
 * %onin_<object>% goes through the very same branch.  run400's whatisinon
 * (46A950) enters its in-half at 46A421 for any mode but 1 (on), so for
 * mode 0 (in) and mode 2 (onin) alike, and tests global_33 (container),
 * global_52 < 6 (openness: not closed, not locked) and obhere at 46A424-
 * 46A44A.  A closed container's %onin_% therefore names only what is on it.
 */
static scr_bool
var_shows_contents (scr_gameref_t game, scr_int container)
{
  return obj_is_container (game, container)
         && gs_object_openness (game, container) <= OBJ_OPEN;
}

static void
var_list_in_object (scr_gameref_t game, scr_int container)
{
  if (var_shows_contents (game, container))
    var_list_at_object (game, container, OBJ_IN_OBJECT, "Inside ",
                        " is inside ", " are inside ");
}

static void
var_list_on_object (scr_gameref_t game, scr_int supporter)
{
  var_list_at_object (game, supporter, OBJ_ON_OBJECT, "On ",
                      " is on ", " are on ");
}


/*
 * var_list_onin_object()
 *
 * List the objects on and in a given associate object.
 */
static void
var_list_onin_object (scr_gameref_t game, scr_int associate)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_bool supporting;
  var_list_t list;

  /* List out the objects standing on this object. */
  list = var_collect_at_object (game, associate, OBJ_ON_OBJECT);
  supporting = !list.empty ();
  if (supporting)
    var_list_at_clause (game, associate, list, "On ", " is on ", " are on ");

  /* List out the objects contained in this object, if it is open. */
  if (var_shows_contents (game, associate))
    list = var_collect_at_object (game, associate, OBJ_IN_OBJECT);
  else
    list.clear ();
  if (!list.empty ())
    {
      /*
       * With something on the surface as well, the contents run onto the
       * same sentence.  MEASURED 2026-09-28, run400 on p4ONIN.taf
       * (runner_probes/onin.run400.txt; make_400_oninprobe.py): "A plate and a
       * bowl are on the crate, and inside is a key and a ring." and "... on
       * the rack, and inside is a nail." -- whatisinon's literal ", and inside
       * is " whatever the count, as in lib_list_in_object_joined().  Scarier
       * keeps agreement there, and so here.  The unnested clause is the one
       * %in_<object>% would have produced, so it goes through the same
       * selector.
       */
      if (supporting)
        {
          var_append_temp (vars, ", and inside");
          var_append_temp (vars,
                           var_select_list_plurality (game, associate, list));
          var_print_list (game, list);
        }
      else
        var_list_at_clause (game, associate, list, "Inside ",
                            " is inside ", " are inside ");
      var_append_temp (vars, ".");
    }
  else
    {
      if (supporting)
        var_append_temp (vars, ".");
    }
}


/*
 * var_return_integer()
 * var_return_string()
 *
 * Convenience helpers for var_get_system().  Provide convenience and some
 * mild syntactic sugar for making returning a value as a system variable
 * a bit easier.  Set appropriate values for return type and the relevant
 * return value field, and always return TRUE.  A macro was tempting here...
 */
static scr_bool
var_return_integer (scr_int value, scr_int *type, scr_vartype_t *vt_rvalue)
{
  *type = VAR_INTEGER;
  vt_rvalue->integer = value;
  return TRUE;
}

static scr_bool
var_return_string (const scr_char *value, scr_int *type, scr_vartype_t *vt_rvalue)
{
  *type = VAR_STRING;
  vt_rvalue->string = value;
  return TRUE;
}


/*
 * var_return_global_string()
 * var_return_game_count()
 *
 * The two shapes several system variables share: a global game string with
 * a placeholder for when the author left it empty, and a counter held in the
 * game, zero (with an error naming WHAT) when there is no game yet.
 */
static scr_bool
var_return_global_string (scr_var_setref_t vars, const scr_char *key,
                          const scr_char *unknown,
                          scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_char *value;

  value = prop_get_global_string (vars->bundle, key);
  if (scr_strempty (value))
    value = unknown;

  return var_return_string (value, type, vt_rvalue);
}

static scr_bool
var_return_game_count (scr_var_setref_t vars, scr_int scr_game_s::*count,
                       const scr_char *what,
                       scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_gameref_t game = vars->game;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for %s\n", what);
      return var_return_integer (0, type, vt_rvalue);
    }

  return var_return_integer (game->*count, type, vt_rvalue);
}


/*
 * var_find_object_by_short()
 *
 * Find the lowest-indexed object whose Short, alone or after its Prefix and
 * a space, equals `name` ignoring case, skipping objects the filter rules
 * out.  -1 if none.  The shared scan behind var_status_object() and
 * var_resolve_marker_object() below.
 */
typedef enum
{
  VAR_ANY_OBJECT,
  VAR_OPENABLE_OBJECT,
  VAR_STATEFUL_OBJECT
} var_object_filter_t;

static scr_int
var_find_object_by_short (scr_gameref_t game, const scr_char *name,
                          var_object_filter_t filter)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *prefix, *shortname;
      std::string prefixed;

      if (filter == VAR_OPENABLE_OBJECT
          && prop_get_indexed_integer (bundle, "Objects", object,
                                       "Openable") == 0)
        continue;
      if (filter == VAR_STATEFUL_OBJECT
          && !var_object_is_stateful (bundle, object))
        continue;

      shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
      if (scr_strcasecmp (name, shortname) == 0)
        return object;

      prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
      if (scr_strempty (prefix))
        continue;

      prefixed.assign (prefix);
      prefixed.append (1, ' ');
      prefixed.append (shortname);
      if (scr_strcasecmp (name, prefixed.c_str ()) == 0)
        return object;
    }

  return -1;
}


/*
 * var_status_object()
 *
 * Find the object that a %status_<name>% marker names, and return its index,
 * or -1 for no match.
 *
 * MEASURED 2026-09-07, run400 on p4STATUS.taf
 * (runner_probes/status.run400.txt, .b.txt and .c.txt, all eleven commands
 * echoed each time; make_400_statusprobe.py).  The probe holds two closed
 * doors, object 0 in Alpha and object 1 in Bravo, and opens and closes them
 * one at a time while reading `ST=[%status_door%]` out of each room's Long:
 *
 *   where    door 0   door 1   run400   scarier was
 *   Bravo    closed   OPEN     closed   open
 *   Alpha    closed   open     closed   open
 *   Bravo    open     CLOSED   open     closed
 *   Charlie  open     closed   open     closed
 *
 * run400 answers for object 0 in every cell and scarier answered for object 1
 * in every cell: it is the LOWEST-indexed match, not the highest, and neither
 * engine cares in the least where the player is or which door was last
 * referred to (Charlie holds no door at all and still gets an answer).  The
 * old code got the highest index because it asked the parser -- uip_match()
 * over "%object%" walks every object and keeps the LAST that matched -- which
 * also meant %status_% quietly rewrote the game's object references in the
 * middle of rendering a room description.
 *
 * The same transcript pins what a name may be.  Object 2 is Prefix "a", Short
 * "portal", Alias "gate", and object 5 is Prefix "the", Short "grate":
 *
 *   AL=[%status_gate%]        left verbatim   an Alias, bare
 *   AP=[%status_a gate%]      left verbatim   an Alias, prefixed
 *   PF=[%status_the grate%]   open            the Short, prefixed
 *   PB=[%status_grate%]       open            the Short, bare
 *
 * so aliases are not searched at all, and the Short answers both with and
 * without its Prefix.  A name that matches nothing is left in the text as it
 * stands, percent signs and all -- which is why the branch below returns
 * FALSE rather than a placeholder, pf_interpolate_vars() copying the marker
 * through unchanged.  (aparty's `%status_the china cabinet%` is exactly that
 * case: "china cabinet" is object 10's Alias, its Short being "china
 * cupboard", so the Runner prints the marker.)
 *
 * Objects that are not openable are skipped rather than matched and then
 * rejected: HA=[%status_hatch%] answers "open" for object 4 with object 3, an
 * unopenable "hatch", sitting in front of it.
 */
static scr_int
var_status_object (scr_gameref_t game, const scr_char *name)
{
  return var_find_object_by_short (game, name, VAR_OPENABLE_OBJECT);
}


/*
 * var_resolve_marker_object()
 *
 * Point the referenced object at the object an %in_<name>%, %on_<name>%,
 * %onin_<name>% or %state_<name>% marker names, and return TRUE, or FALSE
 * if nothing answers to <name>.  The LOWEST-indexed object whose Short (with
 * or without its Prefix) equals `name` exactly wins, the same binary
 * Replace-on-Short scan as var_status_object() above: General.bas
 * 4798A7-479A31 runs it for %in_ and %on_ regardless of what kind of object
 * <name> names, 479BD5 the same for %onin_, and 47A0B2 for %state_ over
 * the objects that have a current state.  With no exact-Short match the
 * caller's name goes to uip_match() instead (aliases, pronouns, and
 * anything else the parser alone can resolve).
 *
 * MEASURED escape_to_new_york turn 72 `open desk` (Ticket run400 xoshiro
 * trace 2026-09-12): the room's Long reads %in_desk% and two objects carry
 * a desk-shaped name, a low-indexed plain "desk" and a higher-indexed
 * "roll-top desk"; the Runner always lists the LOW one's contents, while
 * uip_match()'s last-match-wins walk had been picking the high one whenever
 * both were in scope.
 */
static scr_bool
var_resolve_marker_object (scr_gameref_t game, const scr_char *name,
                           var_object_filter_t filter)
{
  const scr_int matched = var_find_object_by_short (game, name, filter);

  if (matched == -1)
    return uip_match ("%object%", name, game);

  gs_get_vars (game)->referenced_object = matched;
  return TRUE;
}


/*
 * var_get_listing()
 *
 * The %in_<name>%, %on_<name>% and %onin_<name>% markers: resolve <name>,
 * list what is in or on it with `lister`, and hand the listing back, leaving
 * the referenced object as it was.  `marker` and `unavailable` are for the
 * error paths.
 *
 * MEASURED 2026-09-28, run400 on p4ONIN.taf (runner_probes/onin.run400.txt;
 * make_400_oninprobe.py): two boxes, the low-indexed one in Alpha, and
 * from Bravo all three markers come back empty.  The low box wins, as the
 * name scan says, and whatisinon() (46A950) lists nothing for it because
 * it tests obhere (452E9C) first: an object away from the player's room
 * has no contents to report.  whatisinon is a 3.9 routine, so the test
 * stops there.
 */
static scr_bool
var_get_listing (scr_var_setref_t vars, const scr_char *name,
                 const scr_char *marker, const scr_char *unavailable,
                 void (*lister) (scr_gameref_t, scr_int),
                 scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_gameref_t game = vars->game;
  const scr_int saved_ref_object = vars->referenced_object;
  scr_int object;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for %s\n", marker);
      return var_return_string (unavailable, type, vt_rvalue);
    }
  if (!var_resolve_marker_object (game, name + strlen (marker),
                                  VAR_ANY_OBJECT))
    {
      scr_error ("var_get_system: invalid object for %s\n", marker);
      return var_return_string (unavailable, type, vt_rvalue);
    }
  object = vars->referenced_object;

  /* Clear any current temporary for appends, and list into it. */
  var_clear_temp (vars);
  if (prop_get_taf_version (vars->bundle) < TAF_VERSION_390
      || obj_indirectly_in_room (game, object, gs_playerroom (game)))
    lister (game, object);

  /* Restore saved referenced object and return. */
  vars->referenced_object = saved_ref_object;
  return var_return_string (vars->temporary, type, vt_rvalue);
}


/*
 * var_system_*()
 *
 * One function per system variable: construct it, and return its type and
 * value through var_return_*().  A FALSE return means the name is not a
 * variable after all.  NAME is the whole name as written, for the variables
 * matched by prefix, which read an object's name after it.
 */
static scr_bool
var_system_author (scr_var_setref_t vars, const scr_char * /* name */,
                   scr_int *type, scr_vartype_t *vt_rvalue)
{
  /* Get and return the global gameauthor string. */
  return var_return_global_string (vars, "GameAuthor", "[Author unknown]",
                                   type, vt_rvalue);
}

static scr_bool
var_system_character (scr_var_setref_t vars, const scr_char * /* name */,
                      scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;

  /* See if there is a referenced character. */
  if (vars->referenced_character != -1)
    {
      const scr_char *npc_name;

      /* Return the character name string. */
      npc_name = prop_get_indexed_string (bundle, "NPCs",
                                          vars->referenced_character,
                                          "Name");
      if (scr_strempty (npc_name))
        npc_name = "[Character unknown]";

      return var_return_string (npc_name, type, vt_rvalue);
    }
  else
    {
      /*
       * An unbound reference is printed RAW, in every Runner: the
       * substitution simply does not happen, so the pattern itself
       * reaches the player.  p*OBJREF's task 2 `zork` prints "ZORKED
       * %object% and %character%." -- its command binds neither -- and
       * all four Runners answer it literally, including the turn right
       * after `nurb rock` has bound the red rock
       * (runner_probes/objref.run370.rtf,
       * runner_probes/objref.run380.rtf,
       * runner_probes/objref.run390.txt,
       * runner_probes/objref.run400.txt, 2026-09-20).  We used to
       * print "[Character unknown]".
       */
      return var_return_string ("%character%", type, vt_rvalue);
    }
}

static scr_bool
var_system_pronoun (scr_var_setref_t vars, const scr_char *name, scr_int *type,
                    scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;

  /* See if there is a referenced character. */
  if (vars->referenced_character != -1)
    {
      scr_vartype_t vt_key[3];
      scr_int gender;
      const scr_char *retval;

      /* Return the appropriate character gender string. */
      vt_key[0].string = "NPCs";
      vt_key[1].integer = vars->referenced_character;
      vt_key[2].string = "Gender";
      gender = prop_get_integer (bundle, "I<-sis", vt_key);
      switch (gender)
        {
        case NPC_MALE:
          retval = (strcmp (name, "heshe") == 0) ? "he" : "him";
          break;
        case NPC_FEMALE:
          retval = (strcmp (name, "heshe") == 0) ? "she" : "her";
          break;
        case NPC_NEUTER:
          retval = "it";
          break;

        default:
          scr_error ("var_get_system: unknown gender, %ld\n", gender);
          retval = "[Gender unknown]";
          break;
        }
      return var_return_string (retval, type, vt_rvalue);
    }
  else
    {
      scr_error ("var_get_system: no referenced character yet\n");
      return var_return_string ("[Gender unknown]", type, vt_rvalue);
    }
}

static scr_bool
var_system_in (scr_var_setref_t vars, const scr_char *name, scr_int *type,
               scr_vartype_t *vt_rvalue)
{
  return var_get_listing (vars, name, "in_", "[In_ unavailable]",
                          var_list_in_object, type, vt_rvalue);
}

static scr_bool
var_system_maxscore (scr_var_setref_t vars, const scr_char * /* name */,
                     scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  scr_int maxscore;

  /* Return the maximum score. */
  maxscore = prop_get_global_integer (bundle, "MaxScore");

  return var_return_integer (maxscore, type, vt_rvalue);
}

static scr_bool
var_system_modified (scr_var_setref_t vars, const scr_char * /* name */,
                     scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  scr_vartype_t vt_key;
  const scr_char *compiledate;

  /* Return the game compilation date. */
  vt_key.string = "CompileDate";
  compiledate = prop_get_string (bundle, "S<-s", &vt_key);
  if (scr_strempty (compiledate))
    compiledate = "[Modified unknown]";

  return var_return_string (compiledate, type, vt_rvalue);
}

static scr_bool
var_system_number (scr_var_setref_t vars, const scr_char * /* name */,
                   scr_int *type, scr_vartype_t *vt_rvalue)
{
  /* Return the referenced number, or 0 if none yet. */
  return var_return_integer (vars->referenced_number, type, vt_rvalue);
}

static scr_bool
var_system_object (scr_var_setref_t vars, const scr_char * /* name */,
                   scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;

  /* See if we have a referenced object yet. */
  if (vars->referenced_object != -1)
    {
      /* Return object name with its prefix. */
      const scr_char *prefix, *objname;
      size_t size;

      prefix = prop_get_indexed_string (bundle, "Objects",
                                        vars->referenced_object, "Prefix");
      objname = prop_get_indexed_string (bundle, "Objects",
                                         vars->referenced_object, "Short");

      size = strlen (prefix) + strlen (objname) + 2;
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, size);
      snprintf (vars->temporary, size, "%s %s", prefix, objname);

      return var_return_string (vars->temporary, type, vt_rvalue);
    }
  else
    {
      /* Raw, like the unbound %character% above: p*OBJREF `zork`. */
      return var_return_string ("%object%", type, vt_rvalue);
    }
}

static scr_bool
var_system_obstate (scr_var_setref_t vars, const scr_char * /* name */,
                    scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  const scr_gameref_t game = vars->game;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for obstate\n");
      return var_return_string ("[Obstate unavailable]", type, vt_rvalue);
    }
  if (vars->referenced_object == -1)
    {
      scr_error ("var_get_system: no object for obstate\n");
      return var_return_string ("[Obstate unavailable]", type, vt_rvalue);
    }

  /*
   * If not a stateful object, Runner 4.0.45 crashes; we'll do something
   * different here.
   */
  if (!var_object_is_stateful (bundle, vars->referenced_object))
    return var_return_string ("stateless", type, vt_rvalue);

  /* Get state, and copy to temporary. */
  if (!var_set_temp_state (game, vars, vars->referenced_object))
    {
      scr_error ("var_get_system: invalid state for obstate\n");
      return var_return_string ("[Obstate unknown]", type, vt_rvalue);
    }

  /* Return temporary. */
  return var_return_string (vars->temporary, type, vt_rvalue);
}

static scr_bool
var_system_obstatus (scr_var_setref_t vars, const scr_char * /* name */,
                     scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  const scr_gameref_t game = vars->game;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for obstatus\n");
      return var_return_string ("[Obstatus unavailable]", type, vt_rvalue);
    }
  if (vars->referenced_object == -1)
    {
      scr_error ("var_get_system: no object for obstatus\n");
      return var_return_string ("[Obstatus unavailable]", type, vt_rvalue);
    }

  /* If not an openable object, return unopenable to match Adrift. */
  if (prop_get_indexed_integer (bundle, "Objects", vars->referenced_object,
                                "Openable") == 0)
    return var_return_string ("unopenable", type, vt_rvalue);

  /* Return one of open, closed, or locked. */
  return var_return_string (var_openness_word (game,
                                               vars->referenced_object,
                                               "[Obstatus unknown]"),
                            type, vt_rvalue);
}

static scr_bool
var_system_on (scr_var_setref_t vars, const scr_char *name, scr_int *type,
               scr_vartype_t *vt_rvalue)
{
  return var_get_listing (vars, name, "on_", "[On_ unavailable]",
                          var_list_on_object, type, vt_rvalue);
}

static scr_bool
var_system_onin (scr_var_setref_t vars, const scr_char *name, scr_int *type,
                 scr_vartype_t *vt_rvalue)
{
  return var_get_listing (vars, name, "onin_", "[Onin_ unavailable]",
                          var_list_onin_object, type, vt_rvalue);
}

static scr_bool
var_system_player (scr_var_setref_t vars, const scr_char * /* name */,
                   scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  const scr_char *playername;

  /*
   * Return player's name from properties.  An empty authored name is
   * "Anonymous" at 4.0: run400's openadv fills it at load (48F39F,
   * `If field(4) = "" Then field(4) = "Anonymous"`), and that field is
   * what %player% and the third-person pronoun array (48F6F2) read.
   * Measured live 2026-09-19, probe ANON (make_arena_probe.py,
   * runner_probes/anon.run400.txt, PromptName off, Perspective third):
   * `i` answers "Anonymous is carrying nothing." and a task's "Name is
   * [%player%]." prints "Name is [Anonymous]."  Scarier said "Player"
   * for both.
   *
   * run390 has no load-time default (its name prompt is the only
   * writer, 4416C8), so before 4.0 the empty name would stay empty;
   * that has not been measured, and SCARE's "Player" stays there.
   */
  playername = prop_get_global_string (bundle, "PlayerName");
  if (scr_strempty (playername))
    playername = prop_get_taf_version (bundle) >= TAF_VERSION_400
                 ? "Anonymous" : "Player";

  return var_return_string (playername, type, vt_rvalue);
}

static scr_bool
var_system_player_pronoun (scr_var_setref_t vars, const scr_char * /* name */,
                           scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  scr_int gender;

  /*
   * Not an ADRIFT variable: an internal token the 4.0 third-person
   * library messages carry where the Runner reads Ary(5) rather than
   * Ary(0).  run400 fills that slot at 48F76E/48F77F with "he" when
   * Globals/PlayerGender is 0 and "she" otherwise -- there is no neuter
   * form -- and uses it in ", and he is carrying ", ".  The most he can
   * hold is ", " ... but he can move ", " somewhere he haven't been
   * yet." and "Why would he want to run?".  The pre-4.0 Runners have no
   * third person at all (lib_get_perspective() clamps it), so nothing
   * outside those messages can reach this.
   */
  gender = prop_get_global_integer (bundle, "PlayerGender");
  return var_return_string (gender == 0 ? "he" : "she", type, vt_rvalue);
}

static scr_bool
var_system_room (scr_var_setref_t vars, const scr_char * /* name */,
                 scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_gameref_t game = vars->game;
  const scr_char *roomname;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for room\n");
      return var_return_string ("[Room unavailable]", type, vt_rvalue);
    }

  /* Return the current player room. */
  roomname = lib_get_room_name (game, gs_playerroom (game));
  return var_return_string (roomname, type, vt_rvalue);
}

static scr_bool
var_system_score (scr_var_setref_t vars, const scr_char * /* name */,
                  scr_int *type, scr_vartype_t *vt_rvalue)
{
  /* Return the current game score. */
  return var_return_game_count (vars, &scr_game_s::score, "score",
                                type, vt_rvalue);
}

static scr_bool
var_system_state (scr_var_setref_t vars, const scr_char *name, scr_int *type,
                  scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  const scr_gameref_t game = vars->game;
  scr_int saved_ref_object = vars->referenced_object;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for state_\n");
      return var_return_string ("[State_ unavailable]", type, vt_rvalue);
    }
  /*
   * The lowest-indexed stateful namesake, anywhere: from Bravo, p4ONIN's
   * %state_lever% reads the Alpha lever, and %state_knob% the Alpha knob
   * past a stateless one in view (runner_probes/onin.run400.txt).
   */
  if (!var_resolve_marker_object (game, name + 6, VAR_STATEFUL_OBJECT))
    {
      scr_error ("var_get_system: invalid object for state_\n");
      return var_return_string ("[State_ unavailable]", type, vt_rvalue);
    }

  /* Verify this is a stateful object. */
  if (!var_object_is_stateful (bundle, vars->referenced_object))
    {
      vars->referenced_object = saved_ref_object;
      scr_error ("var_get_system: stateless object for state_\n");
      return var_return_string ("[State_ unavailable]", type, vt_rvalue);
    }

  /* Get state, and copy to temporary. */
  if (!var_set_temp_state (game, vars, vars->referenced_object))
    {
      vars->referenced_object = saved_ref_object;
      scr_error ("var_get_system: invalid state for state_\n");
      return var_return_string ("[State_ unknown]", type, vt_rvalue);
    }

  /*
   * MEASURED 2026-08-25, run400 on p4STATE.taf
   * (runner_probes/state.run400.txt, all 29 commands echoed):
   * %state_<obj>% comes back LOWER-CASED, over the whole string, wherever
   * it sits in the sentence --
   *
   *   st panel   ST=[r1]                       (States "R1")
   *   st sign    ST=[sur la gauche]            ("Sur la gauche")
   *   st lever   ST=[in the up position]       ("In the UP position")
   *   mid lever  MID: the lever reads in the up position today.
   *
   * and it is not a first-letter rule: "UP" and "R1" both lose their
   * capitals.  The other two readers of the same States list do NOT --
   * the examine lister prints "The lever is In the UP position." and
   * %obstate% answers "OB=[In the UP position]", both verbatim, in the
   * same transcript.  So the fold belongs here and nowhere else.
   */
  for (scr_char *cursor = vars->temporary; *cursor != NUL; cursor++)
    *cursor = scr_tolower (*cursor);

  /* Restore saved referenced object and return. */
  vars->referenced_object = saved_ref_object;
  return var_return_string (vars->temporary, type, vt_rvalue);
}

static scr_bool
var_system_status (scr_var_setref_t vars, const scr_char *name, scr_int *type,
                   scr_vartype_t *vt_rvalue)
{
  const scr_gameref_t game = vars->game;
  scr_int object;

  /* Check there's enough information to return a value. */
  if (!game)
    {
      scr_error ("var_get_system: no game for status_\n");
      return FALSE;
    }

  object = var_status_object (game, name + 7);
  if (object == -1)
    {
      scr_error ("var_get_system: invalid object for status_\n");
      return FALSE;
    }

  /* Return one of open, closed, or locked. */
  return var_return_string (var_openness_word (game, object,
                                               "[Status_ unknown]"),
                            type, vt_rvalue);
}

static scr_bool
var_system_t_number (scr_var_setref_t vars, const scr_char * /* name */,
                     scr_int *type, scr_vartype_t *vt_rvalue)
{
  /*
   * There is no such thing as "no referenced number yet": the Runner
   * keeps one Long (run390 MemVar_4681AC, run400 MemVar_49420C) and it
   * starts at 0, so the very first turn of p39NUMREF and p4NUMREF
   * answers task 5's `zap` with "ZAP [0] [zero]." before any line has
   * named a number (runner_probes/numref.run390.txt,
   * runner_probes/numref.run400.txt, 2026-09-20).  We used to print
   * "[Number unknown]".
   */
  return var_return_string (var_number_text (vars,
                                             vars->referenced_number),
                            type, vt_rvalue);
}

static scr_bool
var_system_t_variable (scr_var_setref_t vars, const scr_char *name,
                       scr_int *type, scr_vartype_t *vt_rvalue)
{
  scr_varref_t var;

  /* Find the variable; must be a user, not a system, one. */
  var = var_find (vars, name + 2);
  if (!var)
    {
      scr_error ("var_get_system:"
                " no such variable, %s\n", name + 2);
      return var_return_string ("[Unknown variable]", type, vt_rvalue);
    }
  else if (var->type != VAR_INTEGER)
    {
      scr_error ("var_get_system:"
                " not an integer variable, %s\n", name + 2);
      return var_return_string (var->value.string, type, vt_rvalue);
    }
  else
    {
      /* Return the variable value as a string. */
      return var_return_string (var_number_text (vars,
                                                 var->value.integer),
                                type, vt_rvalue);
    }
}

static scr_bool
var_system_text (scr_var_setref_t vars, const scr_char * /* name */,
                 scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_char *retval;

  /* Return any referenced text, otherwise a neutral string. */
  if (vars->referenced_text)
    retval = vars->referenced_text;
  else
    {
      scr_error ("var_get_system: no text yet to reference\n");
      retval = "[Text unknown]";
    }

  return var_return_string (retval, type, vt_rvalue);
}

static scr_bool
var_system_theobject (scr_var_setref_t vars, const scr_char * /* name */,
                      scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;

  /* See if we have a referenced object yet. */
  if (vars->referenced_object != -1)
    {
      /* Return object name prefixed with "the"... */
      var_set_temp (vars, lib_definite_object_name (bundle,
                                    vars->referenced_object).c_str ());

      return var_return_string (vars->temporary, type, vt_rvalue);
    }
  else
    {
      /* Raw by the same mechanism, the substitution never happening. */
      return var_return_string ("%theobject%", type, vt_rvalue);
    }
}

static scr_bool
var_system_time (scr_var_setref_t vars, const scr_char * /* name */,
                 scr_int *type, scr_vartype_t *vt_rvalue)
{
  double delta;
  scr_int retval;

  /* Return the elapsed game time in seconds. */
  delta = var_is_clock_frozen ()
          ? 0.0 : difftime (time (nullptr), vars->timestamp);
  retval = (scr_int) delta + vars->time_offset;

  return var_return_integer (retval, type, vt_rvalue);
}

static scr_bool
var_system_title (scr_var_setref_t vars, const scr_char * /* name */,
                  scr_int *type, scr_vartype_t *vt_rvalue)
{
  /* Return the game's title. */
  return var_return_global_string (vars, "GameName", "[Title unknown]",
                                   type, vt_rvalue);
}

static scr_bool
var_system_turns (scr_var_setref_t vars, const scr_char * /* name */,
                  scr_int *type, scr_vartype_t *vt_rvalue)
{
  /* Return the count of game turns. */
  return var_return_game_count (vars, &scr_game_s::turns, "turns",
                                type, vt_rvalue);
}

static scr_bool
var_system_version (scr_var_setref_t /* vars */, const scr_char * /* name */,
                    scr_int *type, scr_vartype_t *vt_rvalue)
{
  /* Return the Adrift emulation level of Scarier. */
  return var_return_integer (SCARIER_EMULATION, type, vt_rvalue);
}

static scr_bool
var_system_scarier_version (scr_var_setref_t /* vars */,
                            const scr_char * /* name */, scr_int *type,
                            scr_vartype_t *vt_rvalue)
{
  /* Private system variable, return Scarier's version number. */
  return var_return_integer (var_get_scarier_version (), type, vt_rvalue);
}


/*
 * The system variables, in the order their names are tested.  A prefix row
 * matches any name that starts with it, so "t_number" has to come before
 * "t_".
 */
struct var_system_entry_t
{
  const scr_char *name;
  scr_bool is_prefix;
  scr_bool (*get) (scr_var_setref_t vars, const scr_char *name,
                   scr_int *type, scr_vartype_t *vt_rvalue);
};

static const var_system_entry_t VAR_SYSTEM[] = {
  { "author", FALSE, var_system_author },
  { "character", FALSE, var_system_character },
  { "heshe", FALSE, var_system_pronoun },
  { "himher", FALSE, var_system_pronoun },
  { "in_", TRUE, var_system_in },
  { "maxscore", FALSE, var_system_maxscore },
  { "modified", FALSE, var_system_modified },
  { "number", FALSE, var_system_number },
  { "object", FALSE, var_system_object },
  { "obstate", FALSE, var_system_obstate },
  { "obstatus", FALSE, var_system_obstatus },
  { "on_", TRUE, var_system_on },
  { "onin_", TRUE, var_system_onin },
  { "player", FALSE, var_system_player },
  { "player_pronoun", FALSE, var_system_player_pronoun },
  { "room", FALSE, var_system_room },
  { "score", FALSE, var_system_score },
  { "state_", TRUE, var_system_state },
  { "status_", TRUE, var_system_status },
  { "t_number", FALSE, var_system_t_number },
  { "t_", TRUE, var_system_t_variable },
  { "text", FALSE, var_system_text },
  { "theobject", FALSE, var_system_theobject },
  { "time", FALSE, var_system_time },
  { "title", FALSE, var_system_title },
  { "turns", FALSE, var_system_turns },
  { "version", FALSE, var_system_version },
  { "scarier_version", FALSE, var_system_scarier_version }
};


/*
 * var_get_system()
 *
 * Construct a system variable, and return its type and value, or FALSE
 * if invalid name passed in.
 */
static scr_bool
var_get_system (scr_var_setref_t vars,
                const scr_char *name, scr_int *type, scr_vartype_t *vt_rvalue)
{
  for (const var_system_entry_t &entry : VAR_SYSTEM)
    {
      const scr_bool is_match
        = entry.is_prefix ? strncmp (name, entry.name, strlen (entry.name)) == 0
                          : strcmp (name, entry.name) == 0;

      if (is_match)
        return entry.get (vars, name, type, vt_rvalue);
    }

  return FALSE;
}


/*
 * var_get_user()
 *
 * Retrieve a user variable, and return its type and value, or FALSE if the
 * name passed in is not a defined user variable.
 */
static scr_bool
var_get_user (scr_var_setref_t vars,
              const scr_char *name, scr_int *type, scr_vartype_t *vt_rvalue)
{
  scr_varref_t var;

  /* Check user variables for a reference to the named variable. */
  var = var_find (vars, name);
  if (var)
    {
      /* Copy out variable details. */
      *type = var->type;
      switch (var->type)
        {
        case VAR_INTEGER:
          vt_rvalue->integer = var->value.integer;
          break;
        case VAR_STRING:
          vt_rvalue->string = var->value.string;
          break;

        default:
          scr_fatal ("var_get_user: invalid variable type, %ld\n", var->type);
        }

      /* Return success. */
      return TRUE;
    }

  return FALSE;
}


/*
 * var_get()
 *
 * Retrieve a variable, and return its value and type.  Returns FALSE if the
 * named variable does not exist.
 */
scr_bool
var_get (scr_var_setref_t vars,
         const scr_char *name, scr_int *type, scr_vartype_t *vt_rvalue)
{
  scr_bool status;
  assert (var_is_valid (vars));
  assert (name && type && vt_rvalue);

  /*
   * Check user and system variables for a reference to the name.  User
   * variables take precedence over system ones; that is, they may override
   * them in a game.
   */
  status = var_get_user (vars, name, type, vt_rvalue);
  if (!status)
    status = var_get_system (vars, name, type, vt_rvalue);

  if (var_trace)
    {
      if (status)
        {
          scr_trace ("Variable: %%%s%% retrieved, ", name);
          switch (*type)
            {
            case VAR_INTEGER:
              scr_trace ("%ld", vt_rvalue->integer);
              break;
            case VAR_STRING:
              scr_trace ("\"%s\"", vt_rvalue->string);
              break;

            default:
              scr_trace ("Variable: invalid variable type, %ld\n", *type);
              break;
            }
          scr_trace ("\n");
        }
      else
        scr_trace ("Variable: \"%s\", no such variable\n", name);
    }

  return status;
}


/*
 * var_is_user_ordered()
 * var_interpolate_user_ordered()
 *
 * 3.9+ substitutes user variables one at a time, in variable-index order,
 * each by Replace(text, "%" & Name & "%", value) over the whole string --
 * after the system tags (run400 47A23F, run390 45BBCD).  So two
 * markers that share a '%' resolve by index, not by position: Date With
 * Death's "b_notice%b_notice%b_purified%b_purified%" with b_purified (170)
 * ahead of b_notice (189) becomes "b_notice%b_notice1b_purified%", and the
 * ALRs then print "b_notice%[Noticeboard]b_purified%"
 * (runner_transcripts/datewithdeath.txt).  var_is_user_ordered() says whether
 * a name is left to that pass; var_interpolate_user_ordered() runs it,
 * returning TRUE if it changed text.
 */
scr_bool
var_is_user_ordered (scr_var_setref_t vars, const scr_char *name)
{
  assert (var_is_valid (vars));
  return prop_get_taf_version (vars->bundle) >= TAF_VERSION_390
         && var_find (vars, name) != nullptr;
}


/*
 * var_is_unknown_reference()
 *
 * TRUE when NAME is a reference this file's Runner has never heard of, so
 * the marker reaches the player raw -- the same thing an unbound %character%
 * does, and for the same reason: the substitution simply does not happen.
 *
 * Which references a Runner knows is a plain string census of the exe, and
 * it splits cleanly by version: "%object%" is in all four, "%character%"
 * from 3.80, "%number%" and "%t_number%" from 3.90, "%text%" at 4.00 only
 * (run370 and run380 hold no "%number%", "%t_number%" or "%text%" literal
 * anywhere at all).  p37NUMREF and p38NUMREF confirm the print side:
 * task 5's `zap` answers "ZAP [%number%] [%t_number%]." at every turn of
 * the feed, including the turns right after a line naming a number, while
 * p39NUMREF and p4NUMREF answer "ZAP [5] [five]."
 * (runner_probes/numref.run*.*, 2026-09-20).  The matcher side is
 * run_match_task_commands().
 */
scr_bool
var_is_unknown_reference (scr_var_setref_t vars, const scr_char *name)
{
  const scr_int version = prop_get_taf_version (vars->bundle);

  assert (var_is_valid (vars));
  if (strcmp (name, "number") == 0 || strcmp (name, "t_number") == 0)
    return version < TAF_VERSION_390;
  if (strcmp (name, "text") == 0)
    return version < TAF_VERSION_400;
  return FALSE;
}


/*
 * var_get_command_number()
 *
 * The value a task command's %<name>% substitutes, or FALSE when NAME
 * reaches no variable of this game's own.
 *
 * checktask walks the variable array and, for each, InStr's `"%" & Name &
 * "%"` in the command and Replaces it with Format(Value) (run390
 * 44AF07-44AFDA; run400 45F105-45F1B3 inside the shared substituter
 * Proc_19_36_45F268).  Three things follow, and p39VARREF/p4VARREF measure
 * all three (runner_probes/varref.run390.txt,
 * runner_probes/varref.run400.txt, 2026-09-20):
 *
 *  - The value is the NUMERIC one, always.  `word` is the string "quux" at
 *    4.00, and run400 refuses `nurb quux` against task 4's "nurb %word%"
 *    while running it on `nurb 0`.  (3.90 cannot even ask: its VARIABLE
 *    record has no Type field -- sctafpar.cpp spells it `ZType`, a
 *    defaulted zero read from nothing -- so every 3.90 variable is a
 *    number.)
 *  - The command is LOWER-CASED before the walk and the Name is not, so a
 *    marker reaches a variable only when the stored Name is itself all
 *    lower case.  Task 7 is "wibb %NUM%" against a variable `num` and
 *    `wibb 7` runs it, so the marker's case does not matter; tasks 8 and 9
 *    are "bork %Big%" and "snib %big%" against a variable `Big`, and `bork
 *    5` and `snib 5` are BOTH refused, so the Name's case does.  A
 *    capitalised variable is unreachable from any task command at all --
 *    Riding_Home's "knock {on} {your/%NewPlayer%'s} {door}" is dead.
 *  - "%t_<name>%" never substitutes.  Its arm is there (run390 44AFF8,
 *    run400 45F1C6) and it spells the number out with int2text, but the
 *    InStr that guards it searches the wrong string -- var_8C, the typed
 *    line, where the %<name>% arm just above searched the command itself
 *    -- so it can only fire for a line the player cannot type.  `frob
 *    seven` and `frob 7` are both refused against task 3's "frob %t_num%".
 *    Nor does a marker naming no variable survive as something typeable:
 *    `blip %nosuch%` against "blip %nosuch%" is refused as well.
 */
scr_bool
var_get_command_number (scr_var_setref_t vars, const scr_char *name,
                        scr_int *number)
{
  std::string lowered (name);
  scr_varref_t var;

  assert (var_is_valid (vars));
  for (char &c : lowered)
    c = scr_tolower (c);

  var = var_find (vars, lowered.c_str ());
  if (!var)
    return FALSE;

  *number = var->type == VAR_INTEGER ? var->value.integer : 0;
  return TRUE;
}


/*
 * var_get_command_text()
 *
 * Deliberate deviation (2026-09-30), for lenient task matching only: a
 * task command's %name% as Scarier read it before the Runner port
 * (72ae49a1e).  The name is found in any case, and a string variable
 * spells its text, so Lair of the Vampire's `drop %item%` and a `call
 * %who%` over who="bob" answer the words the variable holds.  The Runner
 * lower-cases the marker, finds no capitalised Name and spells a string
 * variable as 0 (var_get_command_number()).
 */
scr_bool
var_get_command_text (scr_var_setref_t vars, const scr_char *name,
                      std::string &text)
{
  scr_varref_t var = nullptr;
  scr_int index_;

  assert (var_is_valid (vars));
  for (index_ = 0; index_ < VAR_HASH_TABLE_SIZE && !var; index_++)
    {
      scr_varref_t candidate;

      for (candidate = vars->variable[index_]; candidate;
           candidate = candidate->next)
        {
          if (scr_strcasecmp (candidate->name, name) == 0)
            {
              var = candidate;
              break;
            }
        }
    }
  if (!var)
    return FALSE;

  if (var->type == VAR_STRING)
    text = var->value.string ? var->value.string : "";
  else
    text = std::to_string ((long) var->value.integer);
  return TRUE;
}


/*
 * var_number_word()
 *
 * int2text() (run390 429048's caller, numintext2 42946C), the Runner's
 * spelling of the numbers zero to twenty; NULL outside that range.
 */
const scr_char *
var_number_word (scr_int number)
{
  return number >= 0 && number < VAR_NUMBERS_SIZE
         ? VAR_NUMBERS[number] : nullptr;
}

scr_bool
var_interpolate_user_ordered (scr_var_setref_t vars, std::string &text)
{
  scr_int var_count, index_;
  scr_vartype_t vt_key[3];
  scr_bool changed;
  assert (var_is_valid (vars));

  if (prop_get_taf_version (vars->bundle) < TAF_VERSION_390
      || text.find ('%') == std::string::npos)
    return FALSE;

  changed = FALSE;
  vt_key[0].string = "Variables";
  var_count = prop_get_child_count (vars->bundle, "I<-s", vt_key);
  for (index_ = 0; index_ < var_count; index_++)
    {
      std::string marker, value;
      scr_varref_t var;
      size_t at;

      vt_key[1].integer = index_;
      vt_key[2].string = "Name";
      marker = std::string ("%")
               + prop_get_string (vars->bundle, "S<-sis", vt_key) + "%";
      at = text.find (marker);
      if (at == std::string::npos)
        continue;

      var = var_find (vars, marker.substr (1, marker.length () - 2).c_str ());
      if (!var)
        continue;
      if (var->type == VAR_INTEGER)
        value = std::to_string (var->value.integer);
      else
        value = var->value.string;

      for (; at != std::string::npos; at = text.find (marker, at))
        {
          text.replace (at, marker.length (), value);
          at += value.length ();
        }
      changed = TRUE;
    }

  return changed;
}


/*
 * var_put_integer()
 * var_get_integer()
 *
 * Convenience functions to store and retrieve an integer variable.  It is
 * an error for the variable not to exist or to have the wrong type.
 */
void
var_put_integer (scr_var_setref_t vars, const scr_char *name, scr_int value)
{
  scr_vartype_t vt_value;
  assert (var_is_valid (vars));

  vt_value.integer = value;
  var_put (vars, name, VAR_INTEGER, vt_value);
}

/*
 * var_get_typed()
 *
 * The lookup under var_get_integer() and var_get_string(): fatal, naming
 * CALLER, if NAME is not a variable or is not of type WANT.
 */
static scr_vartype_t
var_get_typed (scr_var_setref_t vars, const scr_char *name, scr_int want,
               const scr_char *caller, const scr_char *kind)
{
  scr_vartype_t vt_rvalue;
  scr_int type;
  assert (var_is_valid (vars));

  if (!var_get (vars, name, &type, &vt_rvalue))
    scr_fatal ("%s: no such variable, %s\n", caller, name);
  else if (type != want)
    scr_fatal ("%s: not %s, %s\n", caller, kind, name);

  return vt_rvalue;
}

scr_int
var_get_integer (scr_var_setref_t vars, const scr_char *name)
{
  return var_get_typed (vars, name, VAR_INTEGER,
                        "var_get_integer", "an integer").integer;
}


/*
 * var_put_string()
 * var_get_string()
 *
 * Convenience functions to store and retrieve a string variable.  It is
 * an error for the variable not to exist or to have the wrong type.
 */
void
var_put_string (scr_var_setref_t vars,
                const scr_char *name, const scr_char *string)
{
  scr_vartype_t vt_value;
  assert (var_is_valid (vars));

  vt_value.string = string;
  var_put (vars, name, VAR_STRING, vt_value);
}

const scr_char *
var_get_string (scr_var_setref_t vars, const scr_char *name)
{
  return var_get_typed (vars, name, VAR_STRING,
                        "var_get_string", "a string").string;
}


/*
 * var_indexed_name()
 *
 * The storage key of the variable at INDEX_: its Name, unless an earlier
 * variable has the same Name, in which case a key no %marker% can spell.
 * The Runner keeps its variables in an array, so restrictions, actions and
 * saves address a duplicate by index, while a %name% marker resolves to the
 * first by the index-order Replace (var_interpolate_user_ordered()).  mages
 * declares "sleep" twice, as 13 (210) and 15 (0): keyed by name the second
 * overwrote the first, so the `#pass out` event's sleep <= 10 test passed
 * on turn 0, where run390 never passes out (runner_transcripts/mages.txt).
 */
static scr_prop_setref_t var_names_bundle = nullptr;
static std::vector<const scr_char *> var_names;

const scr_char *
var_indexed_name (scr_prop_setref_t bundle, scr_int index_)
{
  static std::set<std::string> duplicate_keys;
  scr_vartype_t vt_key;
  const scr_char *name;
  scr_int earlier;

  /*
   * The keys never change once a game is loaded, but finding one means
   * comparing against every earlier variable's Name, and gs_copy() asks for
   * every variable on every turn -- quadratic in the variable count, and a
   * fifth of some walkthroughs' run time.  So remember each key; the runner
   * calls var_forget_bundle() before destroying a bundle.
   */
  if (bundle != var_names_bundle)
    {
      var_names.clear ();
      var_names_bundle = bundle;
    }
  if (index_ >= 0 && index_ < (scr_int) var_names.size () && var_names[index_])
    return var_names[index_];

  /*
   * The index is the game's: a restriction, action or save names its
   * variable by number, and a damaged file can name one that is not there.
   */
  vt_key.string = "Variables";
  if (index_ < 0 || index_ >= prop_get_child_count (bundle, "I<-s", &vt_key))
    scr_fatal ("var_indexed_name: no variable %ld\n", index_);
  if (index_ >= (scr_int) var_names.size ())
    var_names.resize (index_ + 1, nullptr);

  name = prop_get_indexed_string (bundle, "Variables", index_, "Name");
  for (earlier = 0; earlier < index_; earlier++)
    {
      if (strcmp (name, prop_get_indexed_string (bundle, "Variables",
                                                 earlier, "Name")) == 0)
        {
          std::string key = std::string (name) + "\x01" + std::to_string (index_);
          name = duplicate_keys.insert (key).first->c_str ();
          break;
        }
    }
  var_names[index_] = name;
  return name;
}

void
var_forget_bundle (scr_prop_setref_t bundle)
{
  if (var_names_bundle == bundle)
    {
      var_names_bundle = nullptr;
      var_names.clear ();
    }
}


/*
 * var_create()
 *
 * Create and return a new set of variables.  Variables are created from the
 * properties bundle passed in.
 */
scr_var_setref_t
var_create (scr_prop_setref_t bundle)
{
  scr_var_setref_t vars;
  scr_int var_count, index_;
  scr_vartype_t vt_key[3];
  assert (bundle);

  /* Create a clean set of variables to fill from the bundle. */
  vars = var_create_empty ();
  vars->bundle = bundle;

  /*
   * The property reads below can throw (scr_fatal on a corrupt bundle);
   * reclaim the partially filled variable set on that path, then let the
   * throw carry on to the interface boundary.
   */
  try
    {
      /* Retrieve the count of variables. */
      vt_key[0].string = "Variables";
      var_count = prop_get_child_count (bundle, "I<-s", vt_key);

      /* Create a variable for each variable property held. */
      for (index_ = 0; index_ < var_count; index_++)
        {
          const scr_char *name;
          scr_int var_type;
          const scr_char *value;

          /* Retrieve variable name, type, and string initial value. */
          vt_key[1].integer = index_;
          name = var_indexed_name (bundle, index_);

          vt_key[2].string = "Type";
          var_type = prop_get_integer (bundle, "I<-sis", vt_key);

          vt_key[2].string = "Value";
          value = prop_get_string (bundle, "S<-sis", vt_key);

          /* Handle numerics and strings differently. */
          switch (var_type)
            {
            case TAFVAR_NUMERIC:
              {
                scr_int integer_value;
                if (sscanf (value, "%ld", &integer_value) != 1)
                  {
                    scr_error ("var_create:"
                              " invalid numeric variable %s, %s\n", name, value);
                    integer_value = 0;
                  }
                var_put_integer (vars, name, integer_value);
                break;
              }

            case TAFVAR_STRING:
              var_put_string (vars, name, value);
              break;

            default:
              scr_fatal ("var_create: invalid variable type, %ld\n", var_type);
            }
        }
    }
  catch (...)
    {
      var_destroy (vars);
      throw;
    }

  return vars;
}


/*
 * var_register_game()
 *
 * Register the game, used by variables to satisfy requests for selected
 * system variables.  To ensure integrity, the game being registered must
 * reference this variable set.
 */
void
var_register_game (scr_var_setref_t vars, scr_gameref_t game)
{
  assert (var_is_valid (vars));
  assert (gs_is_game_valid (game));

  if (vars != gs_get_vars (game))
    scr_fatal ("var_register_game: game binding error\n");

  vars->game = game;
}


/*
 * var_set_ref_character()
 * var_set_ref_object()
 * var_set_ref_number()
 * var_set_ref_text()
 *
 * Set the "referenced" character, object, number, and text.
 */
void
var_set_ref_character (scr_var_setref_t vars, scr_int character)
{
  assert (var_is_valid (vars));
  vars->referenced_character = character;
}

void
var_set_ref_object (scr_var_setref_t vars, scr_int object)
{
  assert (var_is_valid (vars));
  vars->referenced_object = object;
}

void
var_set_ref_number (scr_var_setref_t vars, scr_int number)
{
  assert (var_is_valid (vars));
  vars->referenced_number = number;
  vars->is_number_referenced = TRUE;
}

void
var_set_ref_text (scr_var_setref_t vars, const scr_char *text)
{
  assert (var_is_valid (vars));

  /* Take a copy of the string, and retain it. */
  vars->referenced_text
    = (scr_char *) scr_realloc (vars->referenced_text, strlen (text) + 1);
  memcpy (vars->referenced_text, text, strlen (text) + 1);
}


/*
 * var_get_ref_character()
 * var_get_ref_object()
 * var_get_ref_number()
 * var_get_ref_text()
 *
 * Get the "referenced" character, object, number, and text.
 */
scr_int
var_get_ref_character (scr_var_setref_t vars)
{
  assert (var_is_valid (vars));
  return vars->referenced_character;
}

scr_int
var_get_ref_object (scr_var_setref_t vars)
{
  assert (var_is_valid (vars));
  return vars->referenced_object;
}

scr_int
var_get_ref_number (scr_var_setref_t vars)
{
  assert (var_is_valid (vars));
  return vars->referenced_number;
}

/*
 * var_is_number_referenced()
 * var_restore_ref_number()
 *
 * Peek at, and put back, the whole referenced-number state -- the value and
 * the "has one ever been set" flag that %number% substitution tests.  Used by
 * the runner's scr_ref_number_guard (runner/scrunner.h) to keep Scarier's own
 * meta commands ("wait 5", "hist 3") from writing the game's referenced
 * number: they match a %number% pattern, but the real Runner has no such
 * commands and only ever sets its referenced number (run400 MemVar_49420C,
 * written solely by numintext/numintext2 off the wildcard expansion in
 * mdlSpreadTheLoad.Proc_19_36_45F268) while expanding a pattern that really
 * contains %number%.
 */
scr_bool
var_is_number_referenced (scr_var_setref_t vars)
{
  assert (var_is_valid (vars));
  return vars->is_number_referenced;
}

void
var_restore_ref_number (scr_var_setref_t vars,
                        scr_int number, scr_bool is_referenced)
{
  assert (var_is_valid (vars));
  vars->referenced_number = number;
  vars->is_number_referenced = is_referenced;
}

const scr_char *
var_get_ref_text (scr_var_setref_t vars)
{
  assert (var_is_valid (vars));

  /*
   * If currently NULL, return "".  A game may check restrictions involving
   * referenced text before any value has been set; returning "" here for
   * this case prevents problems later (strcmp (NULL, ...), for example).
   */
  return vars->referenced_text ? vars->referenced_text : "";
}


/*
 * var_get_elapsed_seconds()
 * var_set_elapsed_seconds()
 *
 * Get a count of seconds elapsed since the variables were created (start
 * of game), and set the count to a given value (game restore).
 */
scr_uint
var_get_elapsed_seconds (scr_var_setref_t vars)
{
  double delta;
  assert (var_is_valid (vars));

  delta = var_is_clock_frozen ()
          ? 0.0 : difftime (time (nullptr), vars->timestamp);
  return (scr_uint) delta + vars->time_offset;
}

void
var_set_elapsed_seconds (scr_var_setref_t vars, scr_uint seconds)
{
  assert (var_is_valid (vars));

  /*
   * Reset the timestamp to now, and store seconds in offset.  This is sort-of
   * forced by the fact that ANSI offers difftime but no 'settime' -- here,
   * we'd really want to set the timestamp to now less seconds.
   */
  vars->timestamp = time (nullptr);
  vars->time_offset = seconds;
}


/*
 * var_debug_trace()
 *
 * Set variable tracing on/off.
 */
void
var_debug_trace (scr_bool flag)
{
  var_trace = flag;
}


/*
 * var_debug_dump()
 *
 * Print out a complete variables set.
 */
void
var_debug_dump (scr_var_setref_t vars)
{
  scr_int index_;
  scr_varref_t var;
  assert (var_is_valid (vars));

  /* Dump complete structure. */
  scr_trace ("Variable: debug dump follows...\n");
  scr_trace ("vars->bundle = %p\n", (void *) vars->bundle);
  scr_trace ("vars->referenced_character = %ld\n", vars->referenced_character);
  scr_trace ("vars->referenced_object = %ld\n", vars->referenced_object);
  scr_trace ("vars->referenced_number = %ld\n", vars->referenced_number);
  scr_trace ("vars->is_number_referenced = %s\n",
            vars->is_number_referenced ? "true" : "false");

  scr_trace ("vars->referenced_text = ");
  if (vars->referenced_text)
    scr_trace ("\"%s\"\n", vars->referenced_text);
  else
    scr_trace ("(nil)\n");

  scr_trace ("vars->temporary = %p\n", (void *) vars->temporary);
  scr_trace ("vars->timestamp = %lu\n", (scr_uint) vars->timestamp);
  scr_trace ("vars->game = %p\n", (void *) vars->game);

  scr_trace ("vars->variables =\n");
  for (index_ = 0; index_ < VAR_HASH_TABLE_SIZE; index_++)
    {
      for (var = vars->variable[index_]; var; var = var->next)
        {
          if (var == vars->variable[index_])
            scr_trace ("%3ld : ", index_);
          else
            scr_trace ("    : ");
          switch (var->type)
            {
            case VAR_STRING:
              scr_trace ("[String ] %s = \"%s\"", var->name, var->value.string);
              break;
            case VAR_INTEGER:
              scr_trace ("[Integer] %s = %ld", var->name, var->value.integer);
              break;

            default:
              scr_trace ("[Invalid] %s = %p", var->name, var->value.voidp);
              break;
            }
          scr_trace ("\n");
        }
    }
}

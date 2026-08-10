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

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_uint VARS_MAGIC = 0xabcc7a71;
static const scr_char NUL = '\0';

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
  vars->bundle = NULL;
  vars->referenced_character = -1;
  vars->referenced_object = -1;
  vars->referenced_number = 0;
  vars->is_number_referenced = FALSE;
  vars->referenced_text = NULL;
  vars->temporary = NULL;
  vars->timestamp = time (NULL);
  vars->time_offset = 0;
  vars->game = NULL;

  /* Clear all variable hash lists. */
  for (index_ = 0; index_ < VAR_HASH_TABLE_SIZE; index_++)
    vars->variable[index_] = NULL;

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
  var->value.voidp = NULL;

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
void
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
       * it to zero, re-initialize it to SCARIER's version number.  Games that
       * define %scarier_version%, initially zero, can use this to test if
       * running under SCARIER or Runner.
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
      var->value.mutable_string = NULL;

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
      var->value.mutable_string = (decltype(var->value.mutable_string)) scr_realloc (var->value.mutable_string,
                                              strlen (vt_value.string) + 1);
      memcpy (var->value.mutable_string, vt_value.string, strlen (vt_value.string) + 1);
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
      vars->temporary = (decltype(vars->temporary)) scr_malloc (strlen (string) + 1);
      memcpy (vars->temporary, string, strlen (string) + 1);
    }
  else
    {
      /* Append string to existing temporary. */
      new_sentence = (vars->temporary[0] == NUL);
      noted = strlen (vars->temporary);
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary,
                                    strlen (vars->temporary) +
                                    strlen (string) + 1);
      strncat (vars->temporary, string, strlen (string));
    }

  if (new_sentence)
    vars->temporary[noted] = scr_toupper (vars->temporary[noted]);
}


/*
 * var_print_object_np
 * var_print_object
 *
 * Convenience functions to append an object's name, with and without any
 * prefix, to variables temporary.
 */
static void
var_print_object_np (scr_gameref_t game, scr_int object)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix, *normalized, *name;

  /* Get the object's prefix. */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

  /*
   * Try the same shenanigans as done by the equivalent function in the
   * library.
   */
  normalized = prefix;
  if (scr_compare_word (prefix, "a", 1))
    {
      normalized = prefix + 1;
      var_append_temp (vars, "the");
    }
  else if (scr_compare_word (prefix, "an", 2))
    {
      normalized = prefix + 2;
      var_append_temp (vars, "the");
    }
  else if (scr_compare_word (prefix, "the", 3))
    {
      normalized = prefix + 3;
      var_append_temp (vars, "the");
    }
  else if (scr_compare_word (prefix, "some", 4))
    {
      normalized = prefix + 4;
      var_append_temp (vars, "the");
    }
  else if (scr_strempty (prefix))
    var_append_temp (vars, "the ");

  /* As with the library, handle the remaining prefix. */
  if (!scr_strempty (normalized))
    {
      var_append_temp (vars, normalized);
      var_append_temp (vars, " ");
    }
  else if (normalized > prefix)
    var_append_temp (vars, " ");

  /*
   * Print the object's name, again, as with the library, stripping any
   * leading article
   */
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  if (scr_compare_word (name, "a", 1))
    name += 1;
  else if (scr_compare_word (name, "an", 2))
    name += 2;
  else if (scr_compare_word (name, "the", 3))
    name += 3;
  else if (scr_compare_word (name, "some", 4))
    name += 4;
  var_append_temp (vars, name);
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
 * var_list_at_object()
 * var_list_in_object()
 * var_list_on_object()
 *
 * List the objects held in a given container object, or standing on a given
 * surface object.  `position` picks which, and `singular` and `plural` are
 * the phrase joining the list to the associate.
 */
static void
var_list_at_object (scr_gameref_t game, scr_int associate, scr_int position,
                    const scr_char *singular, const scr_char *plural)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int object, count, trail;

  /* List out the objects held by this object. */
  count = 0;
  trail = -1;
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Contained, or standing on? */
      if (gs_object_position (game, object) == position
          && gs_object_parent (game, object) == associate)
        {
          if (count > 0)
            {
              if (count > 1)
                var_append_temp (vars, ", ");

              /* Print out the current list object. */
              var_print_object (game, trail);
            }
          trail = object;
          count++;
        }
    }
  if (count >= 1)
    {
      /* Print out final listed object. */
      if (count == 1)
        {
          var_print_object (game, trail);
          var_append_temp (vars,
                           var_select_plurality (game, trail,
                                                 singular, plural));
        }
      else
        {
          var_append_temp (vars, " and ");
          var_print_object (game, trail);
          var_append_temp (vars, plural);
        }

      /* Print out the container or surface. */
      var_print_object_np (game, associate);
      var_append_temp (vars, ".");
    }
}

static void
var_list_in_object (scr_gameref_t game, scr_int container)
{
  var_list_at_object (game, container, OBJ_IN_OBJECT,
                      " is inside ", " are inside ");
}

static void
var_list_on_object (scr_gameref_t game, scr_int supporter)
{
  var_list_at_object (game, supporter, OBJ_ON_OBJECT,
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
  scr_int object, count, trail;
  scr_bool supporting;

  /* List out the objects standing on this object. */
  count = 0;
  trail = -1;
  supporting = FALSE;
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Standing on? */
      if (gs_object_position (game, object) == OBJ_ON_OBJECT
          && gs_object_parent (game, object) == associate)
        {
          if (count > 0)
            {
              if (count > 1)
                var_append_temp (vars, ", ");

              /* Print out the current list object. */
              var_print_object (game, trail);
            }
          trail = object;
          count++;
        }
    }
  if (count >= 1)
    {
      /* Print out final listed object. */
      if (count == 1)
        {
          var_print_object (game, trail);
          var_append_temp (vars,
                           var_select_plurality (game, trail,
                                                 " is on ", " are on "));
        }
      else
        {
          var_append_temp (vars, " and ");
          var_print_object (game, trail);
          var_append_temp (vars, " are on ");
        }

      /* Print out the surface. */
      var_print_object_np (game, associate);
      supporting = TRUE;
    }

  /* List out the objects contained in this object. */
  count = 0;
  trail = -1;
  for (object = 0; object < gs_object_count (game); object++)
    {
      /* Contained? */
      if (gs_object_position (game, object) == OBJ_IN_OBJECT
          && gs_object_parent (game, object) == associate)
        {
          if (count > 0)
            {
              if (count == 1)
                {
                  if (supporting)
                    var_append_temp (vars, ", and ");
                }
              else
                var_append_temp (vars, ", ");

              /* Print out the current list object. */
              var_print_object (game, trail);
            }
          trail = object;
          count++;
        }
    }
  if (count >= 1)
    {
      /* Print out final listed object. */
      if (count == 1)
        {
          if (supporting)
            var_append_temp (vars, ", and ");
          var_print_object (game, trail);
          var_append_temp (vars,
                           var_select_plurality (game, trail,
                                                 " is inside ",
                                                 " are inside "));
        }
      else
        {
          var_append_temp (vars, " and ");
          var_print_object (game, trail);
          var_append_temp (vars, " are inside");
        }

      /* Print out the container. */
      if (!supporting)
        {
          var_append_temp (vars, " ");
          var_print_object_np (game, associate);
        }
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
 * var_get_system()
 *
 * Construct a system variable, and return its type and value, or FALSE
 * if invalid name passed in.  Uses var_return_*() to reduce code untidiness.
 */
static scr_bool
var_get_system (scr_var_setref_t vars,
                const scr_char *name, scr_int *type, scr_vartype_t *vt_rvalue)
{
  const scr_prop_setref_t bundle = vars->bundle;
  const scr_gameref_t game = vars->game;

  /* Check name for known system variables. */
  if (strcmp (name, "author") == 0)
    {
      scr_vartype_t vt_key[2];
      const scr_char *author;

      /* Get and return the global gameauthor string. */
      vt_key[0].string = "Globals";
      vt_key[1].string = "GameAuthor";
      author = prop_get_string (bundle, "S<-ss", vt_key);
      if (scr_strempty (author))
        author = "[Author unknown]";

      return var_return_string (author, type, vt_rvalue);
    }

  else if (strcmp (name, "character") == 0)
    {
      /* See if there is a referenced character. */
      if (vars->referenced_character != -1)
        {
          scr_vartype_t vt_key[3];
          const scr_char *npc_name;

          /* Return the character name string. */
          vt_key[0].string = "NPCs";
          vt_key[1].integer = vars->referenced_character;
          vt_key[2].string = "Name";
          npc_name = prop_get_string (bundle, "S<-sis", vt_key);
          if (scr_strempty (npc_name))
            npc_name = "[Character unknown]";

          return var_return_string (npc_name, type, vt_rvalue);
        }
      else
        {
          scr_error ("var_get_system: no referenced character yet\n");
          return var_return_string ("[Character unknown]", type, vt_rvalue);
        }
    }

  else if (strcmp (name, "heshe") == 0 || strcmp (name, "himher") == 0)
    {
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

  else if (strncmp (name, "in_", 3) == 0)
    {
      scr_int saved_ref_object = vars->referenced_object;

      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for in_\n");
          return var_return_string ("[In_ unavailable]", type, vt_rvalue);
        }
      if (!uip_match ("%object%", name + 3, game))
        {
          scr_error ("var_get_system: invalid object for in_\n");
          return var_return_string ("[In_ unavailable]", type, vt_rvalue);
        }

      /* Clear any current temporary for appends. */
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 1);
      vars->temporary[0] = '\0';

      /* Write what's in the object into temporary. */
      var_list_in_object (game, vars->referenced_object);

      /* Restore saved referenced object and return. */
      vars->referenced_object = saved_ref_object;
      return var_return_string (vars->temporary, type, vt_rvalue);
    }

  else if (strcmp (name, "maxscore") == 0)
    {
      scr_vartype_t vt_key[2];
      scr_int maxscore;

      /* Return the maximum score. */
      vt_key[0].string = "Globals";
      vt_key[1].string = "MaxScore";
      maxscore = prop_get_integer (bundle, "I<-ss", vt_key);

      return var_return_integer (maxscore, type, vt_rvalue);
    }

  else if (strcmp (name, "modified") == 0)
    {
      scr_vartype_t vt_key;
      const scr_char *compiledate;

      /* Return the game compilation date. */
      vt_key.string = "CompileDate";
      compiledate = prop_get_string (bundle, "S<-s", &vt_key);
      if (scr_strempty (compiledate))
        compiledate = "[Modified unknown]";

      return var_return_string (compiledate, type, vt_rvalue);
    }

  else if (strcmp (name, "number") == 0)
    {
      /* Return the referenced number, or 0 if none yet. */
      if (!vars->is_number_referenced)
        scr_error ("var_get_system: no referenced number yet\n");

      return var_return_integer (vars->referenced_number, type, vt_rvalue);
    }

  else if (strcmp (name, "object") == 0)
    {
      /* See if we have a referenced object yet. */
      if (vars->referenced_object != -1)
        {
          /* Return object name with its prefix. */
          scr_vartype_t vt_key[3];
          const scr_char *prefix, *objname;

          vt_key[0].string = "Objects";
          vt_key[1].integer = vars->referenced_object;
          vt_key[2].string = "Prefix";
          prefix = prop_get_string (bundle, "S<-sis", vt_key);

          vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, strlen (prefix) + 1);
          memcpy (vars->temporary, prefix, strlen (prefix) + 1);

          vt_key[2].string = "Short";
          objname = prop_get_string (bundle, "S<-sis", vt_key);

          vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary,
                                        strlen (vars->temporary)
                                        + strlen (objname) + 2);
          strncat (vars->temporary, " ", 1);
          strncat (vars->temporary, objname, strlen (objname));

          return var_return_string (vars->temporary, type, vt_rvalue);
        }
      else
        {
          scr_error ("var_get_system: no referenced object yet\n");
          return var_return_string ("[Object unknown]", type, vt_rvalue);
        }
    }

  else if (strcmp (name, "obstate") == 0)
    {
      scr_vartype_t vt_key[3];
      scr_bool is_statussed;
      scr_char *state;

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
      vt_key[0].string = "Objects";
      vt_key[1].integer = vars->referenced_object;
      vt_key[2].string = "CurrentState";
      is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
      if (!is_statussed)
        return var_return_string ("stateless", type, vt_rvalue);

      /* Get state, and copy to temporary. */
      state = obj_state_name (game, vars->referenced_object);
      if (!state)
        {
          scr_error ("var_get_system: invalid state for obstate\n");
          return var_return_string ("[Obstate unknown]", type, vt_rvalue);
        }
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, strlen (state) + 1);
      memcpy (vars->temporary, state, strlen (state) + 1);
      scr_free (state);

      /* Return temporary. */
      return var_return_string (vars->temporary, type, vt_rvalue);
    }

  else if (strcmp (name, "obstatus") == 0)
    {
      scr_vartype_t vt_key[3];
      scr_bool is_openable;
      scr_int openness;
      const scr_char *retval;

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
      vt_key[0].string = "Objects";
      vt_key[1].integer = vars->referenced_object;
      vt_key[2].string = "Openable";
      is_openable = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
      if (!is_openable)
        return var_return_string ("unopenable", type, vt_rvalue);

      /* Return one of open, closed, or locked. */
      openness = gs_object_openness (game, vars->referenced_object);
      switch (openness)
        {
        case OBJ_OPEN:
          retval = "open";
          break;
        case OBJ_CLOSED:
          retval = "closed";
          break;
        case OBJ_LOCKED:
          retval = "locked";
          break;
        default:
          retval = "[Obstatus unknown]";
          break;
        }
      return var_return_string (retval, type, vt_rvalue);
    }

  else if (strncmp (name, "on_", 3) == 0)
    {
      scr_int saved_ref_object = vars->referenced_object;

      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for on_\n");
          return var_return_string ("[On_ unavailable]", type, vt_rvalue);
        }
      if (!uip_match ("%object%", name + 3, game))
        {
          scr_error ("var_get_system: invalid object for on_\n");
          return var_return_string ("[On_ unavailable]", type, vt_rvalue);
        }

      /* Clear any current temporary for appends. */
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 1);
      vars->temporary[0] = '\0';

      /* Write what's on the object into temporary. */
      var_list_on_object (game, vars->referenced_object);

      /* Restore saved referenced object and return. */
      vars->referenced_object = saved_ref_object;
      return var_return_string (vars->temporary, type, vt_rvalue);
    }

  else if (strncmp (name, "onin_", 5) == 0)
    {
      scr_int saved_ref_object = vars->referenced_object;

      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for onin_\n");
          return var_return_string ("[Onin_ unavailable]", type, vt_rvalue);
        }
      if (!uip_match ("%object%", name + 5, game))
        {
          scr_error ("var_get_system: invalid object for onin_\n");
          return var_return_string ("[Onin_ unavailable]", type, vt_rvalue);
        }

      /* Clear any current temporary for appends. */
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 1);
      vars->temporary[0] = '\0';

      /* Write what's on/in the object into temporary. */
      var_list_onin_object (game, vars->referenced_object);

      /* Restore saved referenced object and return. */
      vars->referenced_object = saved_ref_object;
      return var_return_string (vars->temporary, type, vt_rvalue);
    }

  else if (strcmp (name, "player") == 0)
    {
      scr_vartype_t vt_key[2];
      const scr_char *playername;

      /*
       * Return player's name from properties, or just "Player" if not set
       * in the properties.
       */
      vt_key[0].string = "Globals";
      vt_key[1].string = "PlayerName";
      playername = prop_get_string (bundle, "S<-ss", vt_key);
      if (scr_strempty (playername))
        playername = "Player";

      return var_return_string (playername, type, vt_rvalue);
    }

  else if (strcmp (name, "room") == 0)
    {
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

  else if (strcmp (name, "score") == 0)
    {
      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for score\n");
          return var_return_integer (0, type, vt_rvalue);
        }

      /* Return the current game score. */
      return var_return_integer (game->score, type, vt_rvalue);
    }

  else if (strncmp (name, "state_", 6) == 0)
    {
      scr_int saved_ref_object = vars->referenced_object;
      scr_vartype_t vt_key[3];
      scr_bool is_statussed;
      scr_char *state;

      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for state_\n");
          return var_return_string ("[State_ unavailable]", type, vt_rvalue);
        }
      if (!uip_match ("%object%", name + 6, game))
        {
          scr_error ("var_get_system: invalid object for state_\n");
          return var_return_string ("[State_ unavailable]", type, vt_rvalue);
        }

      /* Verify this is a stateful object. */
      vt_key[0].string = "Objects";
      vt_key[1].integer = vars->referenced_object;
      vt_key[2].string = "CurrentState";
      is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
      if (!is_statussed)
        {
          vars->referenced_object = saved_ref_object;
          scr_error ("var_get_system: stateless object for state_\n");
          return var_return_string ("[State_ unavailable]", type, vt_rvalue);
        }

      /* Get state, and copy to temporary. */
      state = obj_state_name (game, vars->referenced_object);
      if (!state)
        {
          vars->referenced_object = saved_ref_object;
          scr_error ("var_get_system: invalid state for state_\n");
          return var_return_string ("[State_ unknown]", type, vt_rvalue);
        }
      vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, strlen (state) + 1);
      memcpy (vars->temporary, state, strlen (state) + 1);
      scr_free (state);

      /* Restore saved referenced object and return. */
      vars->referenced_object = saved_ref_object;
      return var_return_string (vars->temporary, type, vt_rvalue);
    }

  else if (strncmp (name, "status_", 7) == 0)
    {
      scr_int saved_ref_object = vars->referenced_object;
      scr_vartype_t vt_key[3];
      scr_bool is_openable;
      scr_int openness;
      const scr_char *retval;

      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for status_\n");
          return var_return_string ("[Status_ unavailable]", type, vt_rvalue);
        }
      if (!uip_match ("%object%", name + 7, game))
        {
          scr_error ("var_get_system: invalid object for status_\n");
          return var_return_string ("[Status_ unavailable]", type, vt_rvalue);
        }

      /* Verify this is an openable object. */
      vt_key[0].string = "Objects";
      vt_key[1].integer = vars->referenced_object;
      vt_key[2].string = "Openable";
      is_openable = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
      if (!is_openable)
        {
          vars->referenced_object = saved_ref_object;
          scr_error ("var_get_system: stateless object for status_\n");
          return var_return_string ("[Status_ unavailable]", type, vt_rvalue);
        }

      /* Return one of open, closed, or locked. */
      openness = gs_object_openness (game, vars->referenced_object);
      switch (openness)
        {
        case OBJ_OPEN:
          retval = "open";
          break;
        case OBJ_CLOSED:
          retval = "closed";
          break;
        case OBJ_LOCKED:
          retval = "locked";
          break;
        default:
          retval = "[Status_ unknown]";
          break;
        }

      /* Restore saved referenced object and return. */
      vars->referenced_object = saved_ref_object;
      return var_return_string (retval, type, vt_rvalue);
    }

  else if (strcmp (name, "t_number") == 0)
    {
      /* See if we have a referenced number yet. */
      if (vars->is_number_referenced)
        {
          scr_int number;
          const scr_char *retval;

          /* Return the referenced number as a string. */
          number = vars->referenced_number;
          if (number >= 0 && number < VAR_NUMBERS_SIZE)
            retval = VAR_NUMBERS[number];
          else
            {
              vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 32);
              snprintf (vars->temporary, 32, "%ld", number);
              retval = vars->temporary;
            }

          return var_return_string (retval, type, vt_rvalue);
        }
      else
        {
          scr_error ("var_get_system: no referenced number yet\n");
          return var_return_string ("[Number unknown]", type, vt_rvalue);
        }
    }

  else if (strncmp (name, "t_", 2) == 0)
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
          scr_int number;
          const scr_char *retval;

          /* Return the variable value as a string. */
          number = var->value.integer;
          if (number >= 0 && number < VAR_NUMBERS_SIZE)
            retval = VAR_NUMBERS[number];
          else
            {
              vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, 32);
              snprintf (vars->temporary, 32, "%ld", number);
              retval = vars->temporary;
            }

          return var_return_string (retval, type, vt_rvalue);
        }
    }

  else if (strcmp (name, "text") == 0)
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

  else if (strcmp (name, "theobject") == 0)
    {
      /* See if we have a referenced object yet. */
      if (vars->referenced_object != -1)
        {
          /* Return object name prefixed with "the"... */
          scr_vartype_t vt_key[3];
          const scr_char *prefix, *normalized, *objname;

          vt_key[0].string = "Objects";
          vt_key[1].integer = vars->referenced_object;
          vt_key[2].string = "Prefix";
          prefix = prop_get_string (bundle, "S<-sis", vt_key);

          vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary, strlen (prefix) + 5);
          vars->temporary[0] = '\0';

          normalized = prefix;
          if (scr_compare_word (prefix, "a", 1))
            {
              strncat (vars->temporary, "the", 3);
              normalized = prefix + 1;
            }
          else if (scr_compare_word (prefix, "an", 2))
            {
              strncat (vars->temporary, "the", 3);
              normalized = prefix + 2;
            }
          else if (scr_compare_word (prefix, "the", 3))
            {
              strncat (vars->temporary, "the", 3);
              normalized = prefix + 3;
            }
          else if (scr_compare_word (prefix, "some", 4))
            {
              strncat (vars->temporary, "the", 3);
              normalized = prefix + 4;
            }
          else if (scr_strempty (prefix))
            strncat (vars->temporary, "the ", 4);

          if (!scr_strempty (normalized))
            {
              strncat (vars->temporary, normalized, strlen (normalized));
              strncat (vars->temporary, " ", 1);
            }
          else if (normalized > prefix)
            strncat (vars->temporary, " ", 1);

          vt_key[2].string = "Short";
          objname = prop_get_string (bundle, "S<-sis", vt_key);
          if (scr_compare_word (objname, "a", 1))
            objname += 1;
          else if (scr_compare_word (objname, "an", 2))
            objname += 2;
          else if (scr_compare_word (objname, "the", 3))
            objname += 3;
          else if (scr_compare_word (objname, "some", 4))
            objname += 4;

          vars->temporary = (decltype(vars->temporary)) scr_realloc (vars->temporary,
                                        strlen (vars->temporary)
                                        + strlen (objname) + 1);
          strncat (vars->temporary, objname, strlen (objname));

          return var_return_string (vars->temporary, type, vt_rvalue);
        }
      else
        {
          scr_error ("var_get_system: no referenced object yet\n");
          return var_return_string ("[Object unknown]", type, vt_rvalue);
        }
    }

  else if (strcmp (name, "time") == 0)
    {
      double delta;
      scr_int retval;

      /* Return the elapsed game time in seconds. */
      delta = var_is_clock_frozen ()
              ? 0.0 : difftime (time (NULL), vars->timestamp);
      retval = (scr_int) delta + vars->time_offset;

      return var_return_integer (retval, type, vt_rvalue);
    }

  else if (strcmp (name, "title") == 0)
    {
      scr_vartype_t vt_key[2];
      const scr_char *gamename;

      /* Return the game's title. */
      vt_key[0].string = "Globals";
      vt_key[1].string = "GameName";
      gamename = prop_get_string (bundle, "S<-ss", vt_key);
      if (scr_strempty (gamename))
        gamename = "[Title unknown]";

      return var_return_string (gamename, type, vt_rvalue);
    }

  else if (strcmp (name, "turns") == 0)
    {
      /* Check there's enough information to return a value. */
      if (!game)
        {
          scr_error ("var_get_system: no game for turns\n");
          return var_return_integer (0, type, vt_rvalue);
        }

      /* Return the count of game turns. */
      return var_return_integer (game->turns, type, vt_rvalue);
    }

  else if (strcmp (name, "version") == 0)
    {
      /* Return the Adrift emulation level of SCARIER. */
      return var_return_integer (SCARIER_EMULATION, type, vt_rvalue);
    }

  else if (strcmp (name, "scarier_version") == 0)
    {
      /* Private system variable, return SCARIER's version number. */
      return var_return_integer (var_get_scarier_version (), type, vt_rvalue);
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

scr_int
var_get_integer (scr_var_setref_t vars, const scr_char *name)
{
  scr_vartype_t vt_rvalue;
  scr_int type;
  assert (var_is_valid (vars));

  if (!var_get (vars, name, &type, &vt_rvalue))
    scr_fatal ("var_get_integer: no such variable, %s\n", name);
  else if (type != VAR_INTEGER)
    scr_fatal ("var_get_integer: not an integer, %s\n", name);

  return vt_rvalue.integer;
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
  scr_vartype_t vt_rvalue;
  scr_int type;
  assert (var_is_valid (vars));

  if (!var_get (vars, name, &type, &vt_rvalue))
    scr_fatal ("var_get_string: no such variable, %s\n", name);
  else if (type != VAR_STRING)
    scr_fatal ("var_get_string: not a string, %s\n", name);

  return vt_rvalue.string;
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
          vt_key[2].string = "Name";
          name = prop_get_string (bundle, "S<-sis", vt_key);

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
  vars->referenced_text = (decltype(vars->referenced_text)) scr_realloc (vars->referenced_text, strlen (text) + 1);
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
          ? 0.0 : difftime (time (NULL), vars->timestamp);
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
  vars->timestamp = time (NULL);
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

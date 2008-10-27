/* vi: set ts=8:
 *
 * Copyright (C) 2003-2005  Simon Baldwin and Mark J. Tilford
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
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
 * o State_ and obstate are implemented, but not fully tested due lack
 *   of games that use them.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_uint32	VARS_MAGIC		= 0xABCC7A71;
enum {			NBUCKET_PRIME		= 211 };
static const sc_char	NUL			= '\0';

/* Variables trace flag. */
static sc_bool		var_trace		= FALSE;

/* Enumerated variable types. */
enum {			VAR_INTEGER		= 'I',
			VAR_STRING		= 'S' };

/* TAF file enumerated values. */
enum {			TAFVAR_NUMERIC		= 0,
			TAFVAR_STRING		= 1 };
enum {			NPC_MALE		= 0,
			NPC_FEMALE		= 1,
			NPC_NEUTER		= 2 };
enum {			OBJ_OPEN		= 5,
			OBJ_CLOSED		= 6,
			OBJ_LOCKED		= 7 };

/* Table of numbers zero to twenty spelled out. */
static sc_char		*NUMBER_NAMES[] = {
	"zero",     "one",      "two",      "three",    "four",     "five",
	"six",      "seven",    "eight",    "nine",     "ten",      "eleven",
	"twelve",   "thirteen", "fourteen", "fifteen",  "sixteen",  "seventeen",
       	"eighteen", "nineteen", "twenty" };
static const sc_int32	NUMBER_NAMES_LENGTH	=
			sizeof (NUMBER_NAMES) / sizeof (NUMBER_NAMES[0]);

/* Variable entry, held on a list hashed by variable name. */
typedef struct sc_var_s {
	sc_char				*name;
	sc_char				type;
	sc_vartype_t			value;
	struct sc_var_s			*next;
} sc_var_t;
typedef sc_var_t			*sc_varref_t;

/*
 * Variables set structure.  A self-contained set of variables on which
 * variables functions operate.
 */
typedef struct sc_var_set_s {
	sc_uint32			magic;
	sc_prop_setref_t		bundle;
	sc_int32			ref_character;
	sc_int32			ref_object;
	sc_int32			ref_number;
	sc_bool				number_refd;
	sc_char				*ref_text;
	sc_char				*temporary;
	time_t				timestamp;
	sc_uint32			time_offset;
	sc_gamestateref_t		gamestate;
	sc_varref_t			variable[NBUCKET_PRIME];
} sc_var_set_t;


/*
 * var_hash_name()
 *
 * Hash a variable name, modulo'ed to the number of buckets.
 */
static sc_uint32
var_hash_name (const sc_char *name)
{
	return sc_hash (name) % NBUCKET_PRIME;
}


/*
 * var_create_empty()
 *
 * Create and return a new set of variables.  Variables are created from
 * the properties bundle passed in.
 */
static sc_var_setref_t
var_create_empty (void)
{
	sc_var_setref_t		vars;
	sc_uint32		index;

	/* Create a clean set of variables. */
	vars = sc_malloc (sizeof (*vars));
	vars->magic		= VARS_MAGIC;
	vars->bundle		= NULL;
	vars->ref_character	= -1;
	vars->ref_object	= -1;
	vars->ref_number	= 0;
	vars->number_refd	= FALSE;
	vars->ref_text		= NULL;
	vars->temporary		= NULL;
	vars->timestamp		= time (NULL);
	vars->time_offset	= 0;
	vars->gamestate		= NULL;

	/* Clear all variable hash lists. */
	for (index = 0; index < NBUCKET_PRIME; index++)
		vars->variable[index] = NULL;

	/* Return new variables. */
	return vars;
}


/*
 * var_destroy()
 *
 * Destroy a variable set, and free its heap memory.
 */
void
var_destroy (sc_var_setref_t vars)
{
	sc_uint32	index;
	sc_varref_t	var, next;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/* Free the content of each string variable, and variable entry. */
	for (index = 0; index < NBUCKET_PRIME; index++)
	    {
		for (var = vars->variable[index]; var != NULL; var = next)
		    {
			next = var->next;
			if (var->type == VAR_STRING)
				sc_free (var->value.string);
			sc_free (var);
		    }
	    }

	/* Free any temporary and reference text storage area. */
	if (vars->temporary != NULL)
		sc_free (vars->temporary);
	if (vars->ref_text != NULL)
		sc_free (vars->ref_text);

	/* Shred and free the variable set itself. */
	memset (vars, 0, sizeof (*vars));
	sc_free (vars);
}


/*
 * var_find()
 * var_add()
 *
 * Find and return a pointer to a named variable structure, or NULL if
 * no such variable exists, and add a new variable structure to the
 * lists.
 */
static sc_varref_t
var_find (sc_var_setref_t vars, const sc_char *name)
{
	sc_uint32	hash;
	sc_varref_t	var;

	/* Hash name, search list and return if name match found. */
	hash = var_hash_name (name);
	for (var = vars->variable[hash]; var != NULL; var = var->next)
	    {
		if (!strcmp (name, var->name))
			return var;
	    }

	/* No such variable. */
	return NULL;
}

static void
var_add (sc_var_setref_t vars, sc_varref_t var)
{
	sc_uint32	hash;

	/* Hash name, and insert at start of the relevant list. */
	hash = var_hash_name (var->name);
	var->next		= vars->variable[hash];
	vars->variable[hash]	= var;
}


/*
 * var_put()
 *
 * Store a variable type in a named variable.  If not present, the
 * variable is created.  Type is one of 'I' or 'S' for integer or string.
 */
void
var_put (sc_var_setref_t vars,
		sc_char *name, sc_char type, sc_vartype_t vt_value)
{
	sc_varref_t	var;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/* Check type is either integer or string. */
	switch (type)
	    {
	    case VAR_INTEGER: case VAR_STRING:
		break;

	    default:
		sc_fatal ("var_put: invalid type, %c\n", type);
	    }

	/* See if the user variable already exists. */
	var = var_find (vars, name);
	if (var != NULL)
	    {
		/* It exists, so modify its value to that passed in. */
		if (var->type != type)
		    {
			sc_fatal ("var_put: variable type"
					" changed, %s\n", name);
		    }
		switch (var->type)
		    {
		    case VAR_INTEGER:
			var->value.integer = vt_value.integer;
			break;

		    case VAR_STRING:
			var->value.string = sc_realloc (var->value.string,
					   strlen (vt_value.string) + 1);
			strcpy (var->value.string, vt_value.string);
			break;

		    default:
			sc_fatal ("var_put: invalid type, %c\n", var->type);
		    }

		/* Trace variable modification. */
		if (var_trace)
		    {
			sc_trace ("Variable: %%%s%% = ", name);
			switch (var->type)
			    {
			    case VAR_INTEGER:
				sc_trace ("%ld", var->value.integer);
				break;
			    case VAR_STRING:
				sc_trace ("\"%s\"", var->value.string);
				break;
			    }
			sc_trace ("\n");
		    }

		/* All done. */
		return;
	    }

	/* Variable not found, so create a new variable and populate it. */
	var = sc_malloc (sizeof (*var));
	var->name	= name;
	var->type	= type;
	switch (var->type)
	    {
	    case VAR_INTEGER:
		var->value.integer = vt_value.integer;
		break;

	    case VAR_STRING:
		var->value.string = sc_malloc (strlen (vt_value.string) + 1);
		strcpy (var->value.string, vt_value.string);
		break;

	    default:
		sc_fatal ("var_put: invalid type, %c\n", var->type);
	    }
	var->next	= NULL;

	/* Add the new variable to the set. */
	var_add (vars, var);

	/* Trace variable creation. */
	if (var_trace)
	    {
		sc_trace ("Variable: %%%s%% [new] = ", name);
		switch (var->type)
		    {
		    case VAR_INTEGER:
			sc_trace ("%ld", var->value.integer);
			break;
		    case VAR_STRING:
			sc_trace ("\"%s\"", var->value.string);
			break;
		    }
		sc_trace ("\n");
	    }
}


/*
 * var_append_temp()
 *
 * Helper for object listers.  Extends temporary, and appends the given
 * text to the string.
 */
static void
var_append_temp (sc_var_setref_t vars, const sc_char *string)
{
	/* See if any temporary string already exists. */
	if (vars->temporary == NULL)
	    {
		/* Create a new temporary area and copy string. */
		vars->temporary = sc_malloc (strlen (string) + 1);
		strcpy (vars->temporary, string);
	    }
	else
	    {
		/* Append string to existing temporary. */
		vars->temporary = sc_realloc (vars->temporary,
				strlen (vars->temporary) + strlen (string) + 1);
		strcat (vars->temporary, string);
	    }
}


/*
 * var_print_object_np
 * var_print_object
 *
 * Convenience functions to append an object's name, with and without
 * any prefix, to variables temporary.
 */
static void
var_print_object_np (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*prefix, *normalized, *name;

	/* Get the object's prefix. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);

	/*
	 * Try the same shenanigans as done by the equivalent function
	 * in the library.
	 */
	normalized = prefix;
	if (!sc_strncasecmp (prefix, "a", 1)
			&& (prefix[1] == NUL || isspace (prefix[1])))
	    {
		normalized = prefix + strlen ("a");
		var_append_temp (vars, "the");
	    }
	else if (!sc_strncasecmp (prefix, "an", 2)
			&& (prefix[2] == NUL || isspace (prefix[2])))
	    {
		normalized = prefix + strlen ("an");
		var_append_temp (vars, "the");
	    }
	else if (!sc_strncasecmp (prefix, "the", 3)
			&& (prefix[3] == NUL || isspace (prefix[3])))
	    {
		normalized = prefix + strlen ("the");
		var_append_temp (vars, "the");
	    }
	else if (!sc_strncasecmp (prefix, "some", 4)
			&& (prefix[4] == NUL || isspace (prefix[4])))
	    {
		normalized = prefix + strlen ("some");
		var_append_temp (vars, "the");
	    }
	else if (sc_strempty (prefix))
		var_append_temp (vars, "the ");

	/* As with the library, handle the remaining prefix. */
	if (!sc_strempty (normalized))
	    {
		var_append_temp (vars, normalized);
		var_append_temp (vars, " ");
	    }
	else if (normalized > prefix)
		var_append_temp (vars, " ");

	/*
	 * Print the object's name, again, as with the library, stripping
	 * any leading article
	 */
	vt_key[2].string = "Short";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strncasecmp (name, "a", 1)
			&& (name[1] == NUL || isspace (name[1])))
		name += strlen ("a");
	else if (!sc_strncasecmp (name, "an", 2)
			&& (name[2] == NUL || isspace (name[2])))
		name += strlen ("an");
	else if (!sc_strncasecmp (name, "the", 3)
			&& (name[3] == NUL || isspace (name[3])))
		name += strlen ("the");
	else if (!sc_strncasecmp (name, "some", 4)
			&& (name[4] == NUL || isspace (name[4])))
		name += strlen ("some");
	var_append_temp (vars, name);
}

static void
var_print_object (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*prefix, *name;

	/*
	 * Get the object's prefix.  As with the library, if the prefix
	 * is empty, put in an "a ".
	 */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strempty (prefix))
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
 * var_list_in_object()
 *
 * List the objects in a given container object.
 */
static void
var_list_in_object (sc_var_setref_t vars,
		sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_int32		cobject, count, trail;

	/* List out the objects contained in this object. */
	count = 0; trail = 0;
	for (cobject = 0; cobject < gamestate->object_count; cobject++)
	    {
		/* Contained? */
		if (gs_get_object_position (gamestate, cobject)
								== OBJ_IN_OBJECT
			&& gs_get_object_parent (gamestate, cobject)
								== object)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					if (strlen (vars->temporary) > 0)
						var_append_temp (vars, "  ");
					var_append_temp (vars, "Inside ");
					var_print_object_np (gamestate, object);
					var_append_temp (vars, " is ");
				    }
				else
					var_append_temp (vars, ", ");

				/* Print out the current list object. */
				var_print_object (gamestate, trail);
			    }
			trail = cobject;
			count++;
		    }
	    }
	if (count >= 1)
	    {
		/* Print out final listed object. */
		if (count == 1)
		    {
			if (strlen (vars->temporary) > 0)
				var_append_temp (vars, "  ");
			var_append_temp (vars, "Inside ");
			var_print_object_np (gamestate, object);
			var_append_temp (vars, " is ");
		    }
		else
			var_append_temp (vars, " and ");
		var_print_object (gamestate, trail);
		var_append_temp (vars, ".");
	    }
}


/*
 * var_list_on_object()
 *
 * List the objects on a given surface object.
 */
static void
var_list_on_object (sc_var_setref_t vars,
		sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_int32		sobject, count, trail;

	/* List out the objects standing on this object. */
	count = 0; trail = 0;
	for (sobject = 0; sobject < gamestate->object_count; sobject++)
	    {
		/* Standing on? */
		if (gs_get_object_position (gamestate, sobject)
								== OBJ_ON_OBJECT
			&& gs_get_object_parent (gamestate, sobject)
								== object)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					if (strlen (vars->temporary) > 0)
						var_append_temp (vars, "  ");
					var_append_temp (vars, "On ");
					var_print_object_np (gamestate, object);
					var_append_temp (vars, " is ");
				    }
				else
					var_append_temp (vars, ", ");

				/* Print out the current list object. */
				var_print_object (gamestate, trail);
			    }
			trail = sobject;
			count++;
		    }
	    }
	if (count >= 1)
	    {
		/* Print out final listed object. */
		if (count == 1)
		    {
			if (strlen (vars->temporary) > 0)
				var_append_temp (vars, "  ");
			var_append_temp (vars, "On ");
			var_print_object_np (gamestate, object);
			var_append_temp (vars, " is ");
		    }
		else
			var_append_temp (vars, " and ");
		var_print_object (gamestate, trail);
		var_append_temp (vars, ".");
	    }
}


/*
 * var_get_system()
 *
 * Construct a system variable, and return its type and value, or FALSE
 * if invalid name passed in.
 */
static sc_bool
var_get_system (sc_var_setref_t vars,
		sc_char *name, sc_char *type, sc_vartype_t *vt_rvalue)
{
	sc_prop_setref_t	bundle		= vars->bundle;
	sc_gamestateref_t	gamestate	= vars->gamestate;

	/* Check name for known system variables. */
	if (!strcmp (name, "author"))
	    {
		sc_vartype_t	vt_key[2];

		/* Get and return the global gameauthor string. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "GameAuthor";
		vt_rvalue->string = prop_get_string (bundle, "S<-ss", vt_key);
		if (sc_strempty (vt_rvalue->string))
			vt_rvalue->string = "[Author unknown]";
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "character"))
	    {
		/* See if there is a referenced character. */
		if (vars->ref_character != -1)
		    {
			sc_vartype_t	vt_key[3];

			/* Return the character name string. */
			vt_key[0].string  = "NPCs";
			vt_key[1].integer = vars->ref_character;
			vt_key[2].string  = "Name";
			vt_rvalue->string = prop_get_string
						(bundle, "S<-sis", vt_key);
			if (sc_strempty (vt_rvalue->string))
				vt_rvalue->string = "[Character unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
		else
		    {
			sc_error ("var_get: no referenced character yet\n");
			vt_rvalue->string = "[Character unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
	    }

	else if (!strcmp (name, "heshe")
		|| !strcmp (name, "himher"))
	    {
		/* See if there is a referenced character. */
		if (vars->ref_character != -1)
		    {
			sc_vartype_t	vt_key[3];
			sc_int32	gender;

			/* Return the appropriate character gender string. */
			vt_key[0].string  = "NPCs";
			vt_key[1].integer = vars->ref_character;
			vt_key[2].string  = "Gender";
			gender = prop_get_integer (bundle, "I<-sis", vt_key);
			switch (gender)
			    {
			    case NPC_MALE:
				vt_rvalue->string =
					!strcmp (name, "heshe") ? "he" : "him";
				break;

			    case NPC_FEMALE:
				vt_rvalue->string =
					!strcmp (name, "heshe") ? "she" : "her";
				break;

			    case NPC_NEUTER:
				vt_rvalue->string = "it";
				break;

			    default:
				sc_error ("var_get: unknown"
						" gender, %ld\n", gender);
				vt_rvalue->string = "[Gender unknown]";
				break;
			    }
			*type = VAR_STRING;
			return TRUE;
		    }
		else
		    {

			sc_error ("var_get: no referenced character yet\n");
			vt_rvalue->string = "[Gender unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
	    }

	else if (!strncmp (name, "in_", strlen ("in_")))
	    {
		sc_int32	saved_ref_object = vars->ref_object;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for in_\n");
			vt_rvalue->string = "[In_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (!uip_match ("%object%",
				name + strlen ("in_"), gamestate))
		    {
			sc_error ("var_get: invalid object for in_\n");
			vt_rvalue->string = "[In_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Clear any current temporary for appends. */
		vars->temporary = sc_realloc (vars->temporary, 1);
		strcpy (vars->temporary, "");

		/* Write what's in the object into temporary. */
		var_list_in_object (vars, gamestate, vars->ref_object);
		vt_rvalue->string = vars->temporary;
		*type = VAR_STRING;

		/* Restore saved referenced object and return. */
		vars->ref_object = saved_ref_object;
		return TRUE;
	    }

	else if (!strcmp (name, "maxscore"))
	    {
		sc_vartype_t	vt_key[2];

		/* Return the maximum score. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "MaxScore";
		vt_rvalue->integer = prop_get_integer (bundle, "I<-ss", vt_key);
		*type = VAR_INTEGER;
		return TRUE;
	    }

	else if (!strcmp (name, "modified"))
	    {
		sc_vartype_t	vt_key;

		/* Return the game compilation date. */
		vt_key.string = "CompileDate";
		vt_rvalue->string = prop_get_string (bundle, "S<-s", &vt_key);
		if (sc_strempty (vt_rvalue->string))
			vt_rvalue->string = "[Modified unknown]";
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "number"))
	    {
		/* Return the referenced number, or 0 if non yet. */
		if (!vars->number_refd)
			sc_error ("var_get: no referenced number yet\n");

		vt_rvalue->integer = vars->ref_number;
		*type = VAR_INTEGER;
		return TRUE;
	    }

	else if (!strcmp (name, "object"))
	    {
		/* See if we have a referenced object yet. */
		if (vars->ref_object != -1)
		    {
			/* Return object name with its prefix. */
			sc_vartype_t	vt_key[3];
			sc_char		*prefix, *objname;

			vt_key[0].string  = "Objects";
			vt_key[1].integer = vars->ref_object;
			vt_key[2].string  = "Prefix";
			prefix = prop_get_string (bundle, "S<-sis", vt_key);

			vars->temporary = sc_realloc (vars->temporary,
							strlen (prefix) + 1);
			strcpy (vars->temporary, prefix);

			vt_key[2].string  = "Short";
			objname = prop_get_string (bundle, "S<-sis", vt_key);

			vars->temporary = sc_realloc (vars->temporary,
							strlen (vars->temporary)
							+ strlen (objname) + 2);
			strcat (vars->temporary, " ");
			strcat (vars->temporary, objname);

			vt_rvalue->string = vars->temporary;
			*type = VAR_STRING;
			return TRUE;
		    }
		else
		    {
			sc_error ("var_get: no referenced object yet\n");
			vt_rvalue->string = "[Object unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
	    }

	else if (!strcmp (name, "obstate"))
	    {
		sc_vartype_t	vt_key[3];
		sc_bool		is_statussed;
		sc_char		*state;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for obstate\n");
			vt_rvalue->string = "[Obstate unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (vars->ref_object == -1)
		    {
			sc_error ("var_get: no object for obstate\n");
			vt_rvalue->string = "[Obstate unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Verify this is a stateful object. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = vars->ref_object;
		vt_key[2].string = "CurrentState";
		is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		if (!is_statussed)
		    {
			sc_error ("var_get: stateless object for obstate\n");
			vt_rvalue->string = "[Obstate unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Get state, and copy to temporary. */
		state = obj_state_name (gamestate, vars->ref_object);
		if (state == NULL)
		    {
			sc_error ("var_get: invalid state for obstate\n");
			vt_rvalue->string = "[Obstate unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
		vars->temporary = sc_realloc (vars->temporary,
							strlen (state) + 1);
		strcpy (vars->temporary, state);
		sc_free (state);

		/* Return temporary. */
		vt_rvalue->string = vars->temporary;
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "obstatus"))
	    {
		sc_vartype_t	vt_key[3];
		sc_bool		is_openable;
		sc_int32	openness;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for obstatus\n");
			vt_rvalue->string = "[Obstatus unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (vars->ref_object == -1)
		    {
			sc_error ("var_get: no object for obstatus\n");
			vt_rvalue->string = "[Obstatus unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Verify this is an openable object. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = vars->ref_object;
		vt_key[2].string = "Openable";
		is_openable = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		if (!is_openable)
		    {
			sc_error ("var_get: stateless object for status_\n");
			vt_rvalue->string = "[Obstatus unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Return one of open, closed, or locked. */
		openness = gs_get_object_openness (gamestate, vars->ref_object);
		switch (openness)
		    {
		    case OBJ_OPEN:	vt_rvalue->string = "open";	break;
		    case OBJ_CLOSED:	vt_rvalue->string = "closed";	break;
		    case OBJ_LOCKED:	vt_rvalue->string = "locked";	break;
		    default:
			vt_rvalue->string = "[Obstatus unknown]";
			break;
		    }
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strncmp (name, "on_", strlen ("on_")))
	    {
		sc_int32	saved_ref_object = vars->ref_object;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for on_\n");
			vt_rvalue->string = "[On_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (!uip_match ("%object%",
				name + strlen ("on_"), gamestate))
		    {
			sc_error ("var_get: invalid object for on_\n");
			vt_rvalue->string = "[On_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Clear any current temporary for appends. */
		vars->temporary = sc_realloc (vars->temporary, 1);
		strcpy (vars->temporary, "");

		/* Write what's on the object into temporary. */
		var_list_on_object (vars, gamestate, vars->ref_object);
		vt_rvalue->string = vars->temporary;
		*type = VAR_STRING;

		/* Restore saved referenced object and return. */
		vars->ref_object = saved_ref_object;
		return TRUE;
	    }

	else if (!strncmp (name, "onin_", strlen ("onin_")))
	    {
		sc_int32	saved_ref_object = vars->ref_object;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for onin_\n");
			vt_rvalue->string = "[Onin_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (!uip_match ("%object%",
				name + strlen ("onin_"), gamestate))
		    {
			sc_error ("var_get: invalid object for onin_\n");
			vt_rvalue->string = "[Onin_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Clear any current temporary for appends. */
		vars->temporary = sc_realloc (vars->temporary, 1);
		strcpy (vars->temporary, "");

		/* Write what's on/in the object into temporary. */
		var_list_on_object (vars, gamestate, vars->ref_object);
		var_list_in_object (vars, gamestate, vars->ref_object);
		vt_rvalue->string = vars->temporary;
		*type = VAR_STRING;

		/* Restore saved referenced object and return. */
		vars->ref_object = saved_ref_object;
		return TRUE;
	    }

	else if (!strcmp (name, "player"))
	    {
		sc_vartype_t	vt_key[2];

		/*
		 * Return player's name from properties, or just "Player" if
		 * not set in the properties.
		 */
		vt_key[0].string = "Globals";
		vt_key[1].string = "PlayerName";
		vt_rvalue->string = prop_get_string (bundle, "S<-ss", vt_key);
		if (sc_strempty (vt_rvalue->string))
			vt_rvalue->string = "Player";
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "room"))
	    {
		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for room\n");
			vt_rvalue->string = "[Room unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Return the current player room. */
		vt_rvalue->string = lib_get_room_name (gamestate,
						gamestate->playerroom);
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "score"))
	    {
		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for score\n");
			vt_rvalue->string = "[Score unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Return the current game score. */
		vt_rvalue->integer = gamestate->score;
		*type = VAR_INTEGER;
		return TRUE;
	    }

	else if (!strncmp (name, "state_", strlen ("state_")))
	    {
		sc_int32	saved_ref_object = vars->ref_object;
		sc_vartype_t	vt_key[3];
		sc_bool		is_statussed;
		sc_char		*state;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for state_\n");
			vt_rvalue->string = "[State_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (!uip_match ("%object%",
				name + strlen ("state_"), gamestate))
		    {
			sc_error ("var_get: invalid object for state_\n");
			vt_rvalue->string = "[State_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Verify this is a stateful object. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = vars->ref_object;
		vt_key[2].string = "CurrentState";
		is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		if (!is_statussed)
		    {
			sc_error ("var_get: stateless object for state_\n");
			vt_rvalue->string = "[State_ unavailable]";
			*type = VAR_STRING;
			vars->ref_object = saved_ref_object;
			return TRUE;
		    }

		/* Get state, and copy to temporary. */
		state = obj_state_name (gamestate, vars->ref_object);
		if (state == NULL)
		    {
			sc_error ("var_get: invalid state for state_\n");
			vt_rvalue->string = "[State_ unknown]";
			*type = VAR_STRING;
			vars->ref_object = saved_ref_object;
			return TRUE;
		    }
		vars->temporary = sc_realloc (vars->temporary,
							strlen (state) + 1);
		strcpy (vars->temporary, state);
		sc_free (state);

		/* Return temporary. */
		vt_rvalue->string = vars->temporary;
		*type = VAR_STRING;

		/* Restore saved referenced object and return. */
		vars->ref_object = saved_ref_object;
		return TRUE;
	    }

	else if (!strncmp (name, "status_", strlen ("status_")))
	    {
		sc_int32	saved_ref_object = vars->ref_object;
		sc_vartype_t	vt_key[3];
		sc_bool		is_openable;
		sc_int32	openness;

		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for status_\n");
			vt_rvalue->string = "[Status_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		if (!uip_match ("%object%",
				name + strlen ("status_"), gamestate))
		    {
			sc_error ("var_get: invalid object for status_\n");
			vt_rvalue->string = "[Status_ unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Verify this is an openable object. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = vars->ref_object;
		vt_key[2].string = "Openable";
		is_openable = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		if (!is_openable)
		    {
			sc_error ("var_get: stateless object for status_\n");
			vt_rvalue->string = "[Status_ unavailable]";
			*type = VAR_STRING;
			vars->ref_object = saved_ref_object;
			return TRUE;
		    }

		/* Return one of open, closed, or locked. */
		openness = gs_get_object_openness (gamestate, vars->ref_object);
		switch (openness)
		    {
		    case OBJ_OPEN:	vt_rvalue->string = "open";	break;
		    case OBJ_CLOSED:	vt_rvalue->string = "closed";	break;
		    case OBJ_LOCKED:	vt_rvalue->string = "locked";	break;
		    default:
			vt_rvalue->string = "[Status_ unknown]";
			break;
		    }
		*type = VAR_STRING;

		/* Restore saved referenced object and return. */
		vars->ref_object = saved_ref_object;
		return TRUE;
	    }

	else if (!strcmp (name, "t_number"))
	    {
		/* See if we have a referenced number yet. */
		if (vars->number_refd)
		    {
			sc_int32	number;

			/* Return the referenced number as a string. */
			number = vars->ref_number;
			if (number >= 0 && number < NUMBER_NAMES_LENGTH)
				vt_rvalue->string = NUMBER_NAMES[number];
			else
			    {
				vars->temporary = sc_realloc
							(vars->temporary, 16);
				sprintf (vars->temporary, "%ld", number);
				vt_rvalue->string = vars->temporary;
			    }
			*type = VAR_STRING;
			return TRUE;
		    }
		else
		    {
			sc_error ("var_get: no referenced number yet\n");
			vt_rvalue->string = "[Number unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
	    }

	else if (!strncmp (name, "t_", strlen ("t_")))
	    {
		sc_varref_t		var;

		/* Find the variable; must be a user, not a system, one. */
		var = var_find (vars, name + strlen ("t_"));
		if (var == NULL)
		    {
			sc_error ("var_get: no such variable, %s\n",
							name + strlen ("t_"));
			vt_rvalue->string = "[Unknown variable]";
			*type = VAR_STRING;
			return TRUE;
		    }
		else if (var->type != VAR_INTEGER)
		    {
			sc_error ("var_get: not an integer variable, %s\n",
							name + strlen ("t_"));
			vt_rvalue->string = var->value.string;
			*type = VAR_STRING;
			return TRUE;
		    }
		else
		    {
			sc_int32	number;

			/* Return the variable value as a string. */
			number = var->value.integer;
			if (number >= 0 && number < NUMBER_NAMES_LENGTH)
				vt_rvalue->string = NUMBER_NAMES[number];
			else
			    {
				vars->temporary = sc_realloc
							(vars->temporary, 16);
				sprintf (vars->temporary, "%ld", number);
				vt_rvalue->string = vars->temporary;
			    }
			*type = VAR_STRING;
			return TRUE;
		    }
	    }

	else if (!strcmp (name, "text"))
	    {
		/* Return any referenced text, otherwise a neutral string. */
		if (vars->ref_text != NULL)
			vt_rvalue->string = vars->ref_text;
		else
		    {
			sc_error ("var_get: no text yet to reference\n");
			vt_rvalue->string = "[Text unknown]";
		    }
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "theobject"))
	    {
		/* See if we have a referenced object yet. */
		if (vars->ref_object != -1)
		    {
			/* Return object name prefixed with "the"... */
			sc_vartype_t	vt_key[3];
			sc_char		*prefix, *normalized, *objname;

			vt_key[0].string  = "Objects";
			vt_key[1].integer = vars->ref_object;
			vt_key[2].string  = "Prefix";
			prefix = prop_get_string (bundle, "S<-sis", vt_key);

			vars->temporary = sc_realloc (vars->temporary,
							strlen (prefix) + 5);
			strcpy (vars->temporary, "");

			normalized = prefix;
			if (!sc_strncasecmp (prefix, "a", 1)
				&& (prefix[1] == NUL || isspace (prefix[1])))
			    {
				strcat (vars->temporary, "the");
				normalized = prefix + strlen ("a");
			    }
			else if (!sc_strncasecmp (prefix, "an", 2)
				&& (prefix[2] == NUL || isspace (prefix[2])))
			    {
				strcat (vars->temporary, "the");
				normalized = prefix + strlen ("an");
			    }
			else if (!sc_strncasecmp (prefix, "the", 3)
				&& (prefix[3] == NUL || isspace (prefix[3])))
			    {
				strcat (vars->temporary, "the");
				normalized = prefix + strlen ("the");
			    }
			else if (!sc_strncasecmp (prefix, "some", 4)
				&& (prefix[4] == NUL || isspace (prefix[4])))
			    {
				strcat (vars->temporary, "the");
				normalized = prefix + strlen ("some");
			    }
			else if (sc_strempty (prefix))
				strcat (vars->temporary, "the ");

			if (!sc_strempty (normalized))
			    {
				strcat (vars->temporary, normalized);
				strcat (vars->temporary, " ");
			    }
			else if (normalized > prefix)
				strcat (vars->temporary, " ");

			vt_key[2].string  = "Short";
			objname = prop_get_string (bundle, "S<-sis", vt_key);
			if (!sc_strncasecmp (objname, "a", 1)
				 && (objname[1] == NUL || isspace (objname[1])))
				objname += strlen ("a");
			else if (!sc_strncasecmp (objname, "an", 2)
				 && (objname[2] == NUL || isspace (objname[2])))
				objname += strlen ("an");
			else if (!sc_strncasecmp (objname, "the", 3)
				 && (objname[3] == NUL || isspace (objname[3])))
				objname += strlen ("the");
			else if (!sc_strncasecmp (objname, "some", 4)
				 && (objname[4] == NUL || isspace (objname[4])))
				objname += strlen ("some");

			vars->temporary = sc_realloc (vars->temporary,
							strlen (vars->temporary)
							+ strlen (objname) + 1);
			strcat (vars->temporary, objname);

			vt_rvalue->string = vars->temporary;
			*type = VAR_STRING;
			return TRUE;
		    }
		else
		    {
			sc_error ("var_get: no referenced object yet\n");
			vt_rvalue->string = "[Object unknown]";
			*type = VAR_STRING;
			return TRUE;
		    }
	    }

	else if (!strcmp (name, "time"))
	    {
		/* Return the elapsed game time in seconds. */
		vt_rvalue->integer = (sc_int32)
				difftime (time (NULL), vars->timestamp)
				+ vars->time_offset;
		*type = VAR_INTEGER;
		return TRUE;
	    }

	else if (!strcmp (name, "title"))
	    {
		sc_vartype_t	vt_key[2];

		/* Return the game's title. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "GameName";
		vt_rvalue->string = prop_get_string (bundle, "S<-ss", vt_key);
		if (sc_strempty (vt_rvalue->string))
			vt_rvalue->string = "[Title unknown]";
		*type = VAR_STRING;
		return TRUE;
	    }

	else if (!strcmp (name, "turns"))
	    {
		/* Check there's enough information to return a value. */
		if (gamestate == NULL)
		    {
			sc_error ("var_get: no gamestate for turns\n");
			vt_rvalue->string = "[Turns unavailable]";
			*type = VAR_STRING;
			return TRUE;
		    }

		/* Return the count of game turns. */
		vt_rvalue->integer = gamestate->turns;
		*type = VAR_INTEGER;
		return TRUE;
	    }

	else if (!strcmp (name, "version"))
	    {
		/* Return the Adrift emulation level of SCARE. */
		vt_rvalue->integer = SCARE_EMULATION;
		*type = VAR_INTEGER;
		return TRUE;
	    }

	return FALSE;
}


/*
 * var_get()
 *
 * Retrieve a variable, and return its value and type.  Returns FALSE if
 * the named variable does not exist.
 */
sc_bool
var_get (sc_var_setref_t vars,
		sc_char *name, sc_char *type, sc_vartype_t *vt_rvalue)
{
	sc_varref_t		var;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/* Check user variables for a reference to the named variable. */
	var = var_find (vars, name);
	if (var != NULL)
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
			sc_fatal ("var_get: invalid type, %c\n", var->type);
		    }

		/* Trace user variable reads. */
		if (var_trace)
		    {
			sc_trace ("Variable: %%%s%% [user] retrieved, ", name);
			switch (var->type)
			    {
			    case VAR_INTEGER:
				sc_trace ("%ld", var->value.integer);
				break;
			    case VAR_STRING:
				sc_trace ("\"%s\"", var->value.string);
				break;
			    }
			sc_trace ("\n");
		    }

		/* Return success. */
		return TRUE;
	    }

	/* Try for a system variable matching this name. */
	if (var_get_system (vars, name, type, vt_rvalue))
	    {
		if (var_trace)
		    {
			sc_trace ("Variable: %%%s%% [system]"
						" retrieved, ", name);
			switch (*type)
			    {
			    case VAR_INTEGER:
				sc_trace ("%ld", vt_rvalue->integer);
				break;
			    case VAR_STRING:
				sc_trace ("\"%s\"", vt_rvalue->string);
				break;
			    }
			sc_trace ("\n");
		    }

		/* Return success. */
		return TRUE;
	    }

	if (var_trace)
		sc_trace ("Variable: \"%s\", no such variable\n", name);

	/* No such variable. */
	return FALSE;
}


/*
 * var_put_integer()
 * var_get_integer()
 *
 * Convenience functions to store and retrieve an integer variable.  It is
 * an error for the variable not to exist on retrieval, or to be of the
 * wrong type.
 */
void
var_put_integer (sc_var_setref_t vars, sc_char *name, sc_int32 value)
{
	sc_vartype_t		vt_value;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	vt_value.integer = value;
	var_put (vars, name, VAR_INTEGER, vt_value);
}
sc_int32
var_get_integer (sc_var_setref_t vars, sc_char *name)
{
	sc_vartype_t		vt_rvalue;
	sc_char			type;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	if (!var_get (vars, name, &type, &vt_rvalue))
		sc_fatal ("var_get_integer: no such variable, %s\n", name);
	if (type != VAR_INTEGER)
		sc_fatal ("var_get_integer: not an integer, %s\n", name);
	return vt_rvalue.integer;
}


/*
 * var_put_string()
 * var_get_string()
 *
 * Convenience functions to store and retrieve a string variable.  It is
 * an error for the variable not to exist on retrieval, or to be of the
 * wrong type.
 */
void
var_put_string (sc_var_setref_t vars, sc_char *name, sc_char *string)
{
	sc_vartype_t		vt_value;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	vt_value.string = string;
	var_put (vars, name, VAR_STRING, vt_value);
}
sc_char *
var_get_string (sc_var_setref_t vars, sc_char *name)
{
	sc_vartype_t		vt_rvalue;
	sc_char			type;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	if (!var_get (vars, name, &type, &vt_rvalue))
		sc_fatal ("var_get_string: no such variable, %s\n", name);
	if (type != VAR_STRING)
		sc_fatal ("var_get_string: not a string, %s\n", name);
	return vt_rvalue.string;
}


/*
 * var_create()
 *
 * Create and return a new set of variables.  Variables are created from
 * the properties bundle passed in.
 */
sc_var_setref_t
var_create (sc_prop_setref_t bundle)
{
	sc_var_setref_t		vars;
	sc_int32		var_count, index;
	sc_vartype_t		vt_key[3], vt_rvalue;
	assert (bundle != NULL);

	/* Create a clean set of variables to fill from the bundle. */
	vars = var_create_empty ();
	vars->bundle = bundle;

	/* Retrieve the count of variables. */
	vt_key[0].string = "Variables";
	var_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;

	/* Create a variable for each variable property held. */
	for (index = 0; index < var_count; index++)
	    {
		sc_char		*name;
		sc_int32	var_type;
		sc_char		*initial_string;

		/* Retrieve variable name, type, and string initial value. */
		vt_key[1].integer = index;
		vt_key[2].string  = "Name";
		name = prop_get_string (bundle, "S<-sis", vt_key);

		vt_key[2].string = "Type";
		var_type = prop_get_integer (bundle, "I<-sis", vt_key);

		vt_key[2].string = "Value";
		initial_string = prop_get_string (bundle, "S<-sis", vt_key);

		/* Handle numerics and strings differently. */
		switch (var_type)
		    {
		    case TAFVAR_NUMERIC:
			{
			sc_int32	initial_integer;
			if (sscanf (initial_string,
						"%ld", &initial_integer) != 1)
			    {
				sc_error ("var_load:"
					" invalid variable %ld numeric, %s\n",
					index, initial_string);
				initial_integer = 0;
			    }
			var_put_integer (vars, name, initial_integer);
			break;
			}

		    case TAFVAR_STRING:
			var_put_string (vars, name, initial_string);
			break;

		    default:
			sc_fatal ("var_load:"
				" unknown variable type, %ld\n", var_type);
		    }
	    }

	/* Return the newly loaded variable set. */
	return vars;
}


/*
 * var_register_gamestate()
 *
 * Register the gamestate, used by variables to satisfy requests for
 * selected system variables.
 */
void
var_register_gamestate (sc_var_setref_t vars, sc_gamestateref_t gamestate)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/* Save gamestate in variables. */
	vars->gamestate = gamestate;
}


/*
 * var_set_ref_character()
 * var_set_ref_object()
 * var_set_ref_number()
 * var_set_ref_text()
 *
 * Set the "referenced" character, object, number, and text for the variable
 * set.
 */
void
var_set_ref_character (sc_var_setref_t vars, sc_int32 character)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);
	vars->ref_character = character;
}

void
var_set_ref_object (sc_var_setref_t vars, sc_int32 object)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);
	vars->ref_object = object;
}

void
var_set_ref_number (sc_var_setref_t vars, sc_int32 number)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);
	vars->ref_number = number;
	vars->number_refd = TRUE;
}

void
var_set_ref_text (sc_var_setref_t vars, sc_char *text)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/* Take a copy of the string, and retain it. */
	vars->ref_text = sc_realloc (vars->ref_text, strlen (text) + 1);
	strcpy (vars->ref_text, text);
}


/*
 * var_get_ref_character()
 * var_get_ref_object()
 * var_get_ref_number()
 * var_get_ref_text()
 *
 * Get the "referenced" character, object, number, and text for the variable
 * set.
 */
sc_int32
var_get_ref_character (sc_var_setref_t vars)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);
	return vars->ref_character;
}

sc_int32
var_get_ref_object (sc_var_setref_t vars)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);
	return vars->ref_object;
}

sc_int32
var_get_ref_number (sc_var_setref_t vars)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);
	return vars->ref_number;
}

sc_char *
var_get_ref_text (sc_var_setref_t vars)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	return vars->ref_text;
}


/*
 * var_get_elapsed_seconds()
 * var_set_elapsed_seconds()
 *
 * Get a count of seconds elapsed since the variables were created (start
 * of game), and set the count to a given value (game restore).
 */
sc_uint32
var_get_elapsed_seconds (sc_var_setref_t vars)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	return (sc_uint32) difftime (time (NULL), vars->timestamp)
			+ vars->time_offset;
}

void
var_set_elapsed_seconds (sc_var_setref_t vars, sc_uint32 seconds)
{
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/*
	 * Reset the timestamp to now, and store seconds in offset.
	 * This is sort-of forced by the fact that ANSI offers difftime
	 * but no other time manipulations -- here, we'd really want
	 * to set the timestamp to now less seconds.
	 */
	vars->timestamp		= time (NULL);
	vars->time_offset	= seconds;
}


/*
 * var_debug_trace()
 *
 * Set variable tracing on/off.
 */
void
var_debug_trace (sc_bool flag)
{
	var_trace = flag;
}


/*
 * var_debug_dump()
 *
 * Print out a complete variables set.
 */
void
var_debug_dump (sc_var_setref_t vars)
{
	sc_uint32		index;
	sc_varref_t		var;
	assert (vars != NULL && vars->magic == VARS_MAGIC);

	/* Dump complete structure. */
	sc_trace ("vars->bundle = %p\n", (void *) vars->bundle);
	sc_trace ("vars->ref_character = %ld\n", vars->ref_character);
	sc_trace ("vars->ref_object = %ld\n", vars->ref_object);
	sc_trace ("vars->ref_number = %ld\n", vars->ref_number);
	sc_trace ("vars->number_refd = %s\n",
				vars->number_refd ? "TRUE" : "FALSE");
	sc_trace ("vars->ref_text = ");
	if (vars->ref_text != NULL)
		sc_trace ("\"%s\"\n", vars->ref_text);
	else
		sc_trace ("(null)\n");
	sc_trace ("vars->temporary = %p\n", (void *) vars->temporary);
	sc_trace ("vars->timestamp = %lu\n", (sc_uint32)vars->timestamp);
	sc_trace ("vars->gamestate = %p\n", (void *) vars->gamestate);
	sc_trace ("vars->variables =\n");
	for (index = 0; index < NBUCKET_PRIME; index++)
	    {
		for (var = vars->variable[index]; var != NULL; var = var->next)
		    {
			if (var == vars->variable[index])
				sc_trace ("%3ld : ", index);
			else
				sc_trace ("    : ");
			sc_trace ("[%c] %s = ", var->type, var->name);
			switch (var->type)
			    {
			    case VAR_STRING:
				sc_trace ("\"%s\"", var->value.string);
				break;
			    case VAR_INTEGER:
				sc_trace ("%ld", var->value.integer);
				break;
			    default:
				sc_fatal ("var_debug_dump: invalid type\n");
			    }
			sc_trace ("\n");
		    }
	    }
}

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
 * o ...
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <setjmp.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
enum {			MAX_NESTING_DEPTH	= 32 };
static const sc_char	NUL			= '\0';
enum {			TAFVAR_NUMERIC		= 0,
			TAFVAR_STRING		= 1 };

/* Trace flag, set before running. */
static sc_bool		restr_trace		= FALSE;


/*
 * restr_integer_variable()
 *
 * Return the index of the n'th integer found.
 */
static sc_int32
restr_integer_variable (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_int32		var_count, var, count;

	/* Get the count of variables. */
	vt_key[0].string = "Variables";
	var_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;

	/* Progress through variables until n integers found. */
	count = n;
	for (var = 0; var < var_count && count >= 0; var++)
	    {
		sc_int32	type;

		vt_key[1].integer = var;
		vt_key[2].string = "Type";
		type = prop_get_integer (bundle, "I<-sis", vt_key);
		if (type == 0)
			count--;
	    }
	return var - 1;
}


/*
 * restr_object_in_place()
 *
 * Is object in a certain place, state, or condition.
 */
static sc_bool
restr_object_in_place (sc_gamestateref_t gamestate,
		sc_int32 object, sc_int32 var2, sc_int32 var3)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		npc;

	if (restr_trace)
	    {
		sc_trace ("Restr: checking object in place,"
				" %ld, %ld, %ld\n", object, var2, var3);
	    }

	/* Var2 controls what we do. */
	switch (var2)
	    {
	    case 0: case 6:				/* In room */
		if (var3 == 0)
			return gs_get_object_position (gamestate, object)
							== OBJ_HIDDEN;
		else
			return gs_get_object_position (gamestate, object)
							== var3;

	    case 1: case 7:				/* Held by */
		if (var3 == 0)				/* Player */
			return gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER;
		else if (var3 == 1)			/* Ref character */
			npc = var_get_ref_character (vars);
		else
			npc = var3 - 2;
		return gs_get_object_position (gamestate, object)
							== OBJ_HELD_NPC
			&& gs_get_object_parent (gamestate, object) == npc;

	    case 2: case 8:				/* Worn by */
		if (var3 == 0)				/* Player */
			return gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER;
		else if (var3 == 1)			/* Ref character */
			npc = var_get_ref_character (vars);
		else
			npc = var3 - 2;
		return gs_get_object_position (gamestate, object)
							== OBJ_WORN_NPC
			&& gs_get_object_parent (gamestate, object) == npc;

	    case 3: case 9:				/* Visible to */
		if (var3 == 0)				/* Player */
			return obj_indirectly_in_room (gamestate,
						object, gamestate->playerroom);
		else if (var3 == 1)			/* Ref character */
			npc = var_get_ref_character (vars);
		else
			npc = var3 - 2;
		return obj_indirectly_in_room (gamestate,
						object,
					gamestate->npcs[npc].location - 1);

	    case 4: case 10:				/* Inside */
		if (var3 == 0)				/* Nothing? */
			return gs_get_object_position (gamestate, object)
							!= OBJ_IN_OBJECT;
		return gs_get_object_position (gamestate, object)
							== OBJ_IN_OBJECT
			&& gs_get_object_parent (gamestate, object)
				== obj_container_object (gamestate, var3 - 1);

	    case 5: case 11:				/* On top of */
		if (var3 == 0)				/* Nothing? */
			return gs_get_object_position (gamestate, object)
							!= OBJ_ON_OBJECT;
		return gs_get_object_position (gamestate, object)
							== OBJ_ON_OBJECT
			&& gs_get_object_parent (gamestate, object)
				== obj_surface_object (gamestate, var3 - 1);

	    default:
		sc_fatal ("restr_object_in_place: bad var2, %ld\n", var2);
	    }

	sc_fatal ("restr_object_in_place: invalid fall-through\n");
	return FALSE;
}


/*
 * restr_pass_task_object_location()
 *
 * Evaluate restrictions relating to object location.
 */
static sc_bool
restr_pass_task_object_location (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2, sc_int32 var3)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_bool			should_be;
	sc_int32		object;

	if (restr_trace)
	    {
		sc_trace ("Restr: running object location restriction,"
				" %ld, %ld, %ld\n", var1, var2, var3);
	    }

	/* Unnecessary initializations to keep GCC happy. */
	should_be = FALSE;	object = -1;

	/* See how things should look. */
	if (var2 >= 0 && var2 < 6)
		should_be = TRUE;
	else if (var2 >= 6 && var2 < 12)
		should_be = FALSE;
	else
	    {
		sc_fatal ("restr_pass_task_object_location:"
					" bad var2, %ld\n", var2);
	    }

	/* Now find the addressed object. */
	if (var1 == 0)
	    {
		object = -1;				/* No object */
		should_be = !should_be;
	    }
	else if (var1 == 1)
		object = -1;				/* Any object */
	else if (var1 == 2)
		object = var_get_ref_object (vars);
	else if (var1 >= 3)
		object = obj_dynamic_object (gamestate, var1 - 3);
	else
	    {
		sc_fatal ("restr_pass_task_object_location:"
					" bad var1, %ld\n", var1);
	    }

	/*
	 * Here it seems that we have to special case static objects that
	 * may have crept in through the referenced object.  The object in
	 * place function isn't built to handle these.  TODO What is the
	 * meaning of applying object restrictions to static objects?
	 */
	if (var1 == 2 && object != -1
			&& obj_is_static (gamestate, object))
	    {
		if (restr_trace)
			sc_trace ("Restr: restriction object %ld"
					" is static, rejecting\n", object);
		return FALSE;
	    }

	/* Try to put it all together. */
	if (object == -1)
	    {
		sc_int32	target;

		for (target = 0; target < gamestate->object_count; target++)
		    {
			if (restr_object_in_place
						(gamestate, target, var2, var3))
				return should_be;
		    }
		return !should_be;
	    }
	return should_be == restr_object_in_place
						(gamestate, object, var2, var3);
}


/*
 * restr_pass_task_object_state()
 *
 * Evaluate restrictions relating to object states.  This function is called
 * from the library by lib_pass_alt_room(), so cannot be static.
 */
sc_bool
restr_pass_task_object_state (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3];
	sc_int32		object, openable, key;

	if (restr_trace)
	    {
		sc_trace ("Restr: running object state"
				" restriction, %ld, %ld\n", var1, var2);
	    }

	/* Find the object being addressed. */
	if (var1 == 0)
		object = var_get_ref_object (vars);
	else
		object = obj_stateful_object (gamestate, var1 - 1);

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
				return gs_get_object_openness
							(gamestate, object)
								== var2 + 5;
			else
				return gs_get_object_state (gamestate, object)
								== var2 - 2;
		    }
		else
		    {
			if (var2 <= 1)
				return gs_get_object_openness
							(gamestate, object)
								== var2 + 5;
			else
				return gs_get_object_state (gamestate, object)
								== var2 - 1;
		    }
	    }
	else
		return gs_get_object_state (gamestate, object) == var2 + 1;
}


/*
 * restr_pass_task_task_state()
 *
 * Evaluate restrictions relating to task states.
 */
static sc_bool
restr_pass_task_task_state (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2)
{
	sc_bool		should_be;

	if (restr_trace)
	    {
		sc_trace ("Restr: running task"
				" restriction, %ld, %ld\n", var1, var2);
	    }

	/* Unnecessary initialization to keep GCC happy. */
	should_be = FALSE;

	/* See if the task should be done or not done. */
	if (var2 == 0)
		should_be = TRUE;
	else if (var2 == 1)
		should_be = FALSE;
	else
		sc_fatal ("restr_pass_task_task_state: bad var2, %ld\n", var2);

	/* Check all tasks? */
	if (var1 == 0)
	    {
		sc_int32	task;

		for (task = 0; task < gamestate->task_count; task++)
		    {
			if (gs_get_task_done (gamestate, task) == should_be)
				return FALSE;
		    }
		return TRUE;
	    }

	/* Check just the given task. */
	return gs_get_task_done (gamestate, var1 - 1) == should_be;
}


/*
 * restr_pass_task_char()
 *
 * Evaluate restrictions relating to player and NPCs.
 */
static sc_bool
restr_pass_task_char (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2, sc_int32 var3)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		npc1, npc2;

	if (restr_trace)
	    {
		sc_trace ("Restr: running char restriction,"
				" %ld, %ld, %ld\n", var1, var2, var3);
	    }

	/* Handle var2 types 1 and 2. */
	if (var2 == 1)				/* Not in same room as */
		return !restr_pass_task_char (gamestate, var1, 0, var3);
	else if (var2 == 2)			/* Alone */
		return !restr_pass_task_char (gamestate, var1, 3, var3);

	/* Decode NPC number, -1 if none. */
	npc1 = npc2 = -1;
	if (var1 == 1)
		npc1 = var_get_ref_character (vars);
	else if (var1 > 1)
		npc1 = var1 - 2;

	/* Player or NPC? */
	if (var1 == 0)
	    {
		sc_vartype_t	vt_key[2];
		sc_int32	gender;

		/* Player -- decode based on var2. */
		switch (var2)
		    {
		    case 0:			/* In same room as */
			if (var3 == 1)
				npc2 = var_get_ref_character (vars);
			else if (var3 > 1)
				npc2 = var3 - 2;
			if (var3 == 0)		/* Player */
				return TRUE;
			else
				return npc_in_room (gamestate, npc2,
							gamestate->playerroom);

		    case 3:			/* Not alone */
			return npc_count_in_room (gamestate,
						gamestate->playerroom) > 1;

		    case 4:			/* Standing on */
			return gamestate->playerposition == 0
				&& gamestate->playerparent
					== obj_standable_object
							(gamestate, var3 - 1);

		    case 5:			/* Sitting on */
			return gamestate->playerposition == 1
				&& gamestate->playerparent
					== obj_standable_object
							(gamestate, var3 - 1);

		    case 6:			/* Lying on */
			return gamestate->playerposition == 2
				&& gamestate->playerparent
					== obj_lieable_object
							(gamestate, var3 - 1);

		    case 7:			/* Player gender */
			vt_key[0].string = "Globals";
			vt_key[1].string = "PlayerGender";
			gender = prop_get_integer (bundle, "I<-ss", vt_key);
			return gender == var3;

		    default:
			sc_fatal ("restr_pass_task_char:"
						" invalid type, %ld\n", var2);
		    }
	    }
	else
	    {
		sc_vartype_t	vt_key[3];
		sc_int32	gender;

		/* NPC -- decode based on var2. */
		switch (var2)
		    {
		    case 0:			/* In same room as */
			if (var3 == 0)
				return npc_in_room (gamestate, npc1,
							gamestate->playerroom);
			if (var3 == 1)
				npc2 = var_get_ref_character (vars);
			else if (var3 > 1)
				npc2 = var3 - 2;
			return npc_in_room (gamestate, npc1,
					gamestate->npcs[npc2].location - 1);

		    case 3:			/* Not alone */
			return npc_count_in_room (gamestate,
					gamestate->npcs[npc1].location - 1) > 1;

		    case 4:			/* Standing on */
			return gamestate->npcs[npc1].position == 0
				&& gamestate->playerparent
					== obj_standable_object
							(gamestate, var3);

		    case 5:			/* Sitting on */
			return gamestate->npcs[npc1].position == 1
				&& gamestate->playerparent
					== obj_standable_object
							(gamestate, var3);

		    case 6:			/* Lying on */
			return gamestate->npcs[npc1].position == 2
				&& gamestate->playerparent
					== obj_lieable_object
							(gamestate, var3);

		    case 7:			/* NPC gender */
			vt_key[0].string = "NPCs";
			vt_key[1].integer = npc1;
			vt_key[2].string = "Gender";
			gender = prop_get_integer (bundle, "I<-sis", vt_key);
			return gender == var3;

		    default:
			sc_fatal ("restr_pass_task_char:"
						" invalid type, %ld\n", var2);
		    }
	    }

	sc_fatal ("restr_pass_task_char: invalid fall-through\n");
	return FALSE;
}


/*
 * restr_pass_task_int_var()
 *
 * Helper for restr_pass_task_var(), handles integer variable restrictions.
 */
static sc_bool
restr_pass_task_int_var (sc_gamestateref_t gamestate,
		sc_int32 var2, sc_int32 var3, sc_char *var4, sc_int32 value)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3];
	sc_int32		value2;

	if (restr_trace)
	    {
		sc_trace ("Restr: running integer var restriction,"
			" %ld, %ld, \"%s\", %ld\n", var2, var3, var4, value);
	    }

	/* Insist that var4 is empty for this restriction type. */
	if (strlen (var4) != 0)
	    {
		sc_fatal ("restr_pass_task_int_var:"
				" non-empty var4 for integer type\n");
	    }

	/* Compare against var3 if that's what var2 says. */
	switch (var2)
	    {
	    case 0:	return value <  var3;
	    case 1:	return value <= var3;
	    case 2:	return value == var3;
	    case 3:	return value >= var3;
	    case 4:	return value >  var3;
	    case 5:	return value != var3;

	    default:
		/*
		 * Compare against the integer var numbered in var3 - 1,
		 * or the referenced number if var3 is zero.  Make sure
		 * that we're comparing integer variables.
		 */
		if (var3 == 0)
			value2 = var_get_ref_number (vars);
		else
		    {
			sc_char		*name;
			sc_int32	ivar, type;

			ivar = restr_integer_variable (gamestate, var3 - 1);
			vt_key[0].string = "Variables";
			vt_key[1].integer = ivar;
			vt_key[2].string = "Name";
			name = prop_get_string (bundle, "S<-sis", vt_key);
			vt_key[2].string = "Type";
			type = prop_get_integer (bundle, "I<-sis", vt_key);

			if (type != 0)
			    {
				sc_fatal ("restr_pass_task_int_var:"
				" non-integer in comparison, %s\n", name);
			    }

			/* Get the value in variable numbered in var3 - 1. */
			value2 = var_get_integer (vars, name);
		    }

		switch (var2)
		    {
		    case 10:	return value <  value2;
		    case 11:	return value <= value2;
		    case 12:	return value == value2;
		    case 13:	return value >= value2;
		    case 14:	return value >  value2;
		    case 15:	return value != value2;

		    default:
			sc_fatal ("restr_pass_task_int_var:"
					" unknown int comparison, %ld\n", var2);
		    }
	    }

	sc_fatal ("restr_pass_task_int_var: invalid fall-through\n");
	return FALSE;
}


/*
 * restr_pass_task_string_var()
 *
 * Helper for restr_pass_task_var(), handles string variable restrictions.
 */
static sc_bool
restr_pass_task_string_var (sc_int32 var2, sc_int32 var3,
		sc_char *var4, sc_char *value)
{
	/* Insist that var3 is zero for this restriction type. */
	if (var3 != 0)
	    {
		sc_fatal ("restr_pass_task_string_var:"
				" non-zero var3 for string type\n");
	    }

	/* Make comparison against var4 based on var2 value. */
	switch (var2)
	    {
	    case 0:	return strcmp (value, var4) == 0;	/* == */
	    case 1:	return strcmp (value, var4) != 0;	/* != */

	    default:
		sc_fatal ("restr_pass_task_string_var:"
				" unknown string comparison, %ld\n", var2);
	    }

	sc_fatal ("restr_pass_task_string_var: invalid fall-through\n");
	return FALSE;
}


/*
 * restr_pass_task_var()
 *
 * Evaluate restrictions relating to variables.
 */
static sc_bool
restr_pass_task_var (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2, sc_int32 var3, sc_char *var4)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3];
	sc_int32		type, ivalue;
	sc_char			*name, *svalue;

	if (restr_trace)
	    {
		sc_trace ("Restr: running var restriction,"
			" %ld, %ld, %ld, \"%s\"\n", var1, var2, var3, var4);
	    }

	/*
	 * For var1=0, compare against referenced number.  For var1=1,
	 * compare against referenced text.
	 */
	if (var1 == 0)
	    {
		ivalue = var_get_ref_number (vars);
		return restr_pass_task_int_var
				(gamestate, var2, var3, var4, ivalue);
	    }
	else if (var1 == 1)
	    {
		svalue = var_get_ref_text (vars);
		return restr_pass_task_string_var (var2, var3, var4, svalue);
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
		ivalue = var_get_integer (vars, name);
		return restr_pass_task_int_var (gamestate,
						var2, var3, var4, ivalue);

	    case TAFVAR_STRING:
		svalue = var_get_string (vars, name);
		return restr_pass_task_string_var (var2, var3, var4, svalue);

	    default:
		sc_fatal ("restr_pass_task_var:"
				" invalid variable type, %ld\n", type);
	    }

	sc_fatal ("restr_pass_task_var: invalid fall-through\n");
	return FALSE;
}


/*
 * restr_pass_task_restriction()
 *
 * Demultiplexer for task restrictions.
 */
static sc_bool
restr_pass_task_restriction (sc_gamestateref_t gamestate,
			sc_int32 task, sc_int32 restriction)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		type, var1, var2, var3;
	sc_char			*var4;
	sc_bool			result = FALSE;

	if (restr_trace)
	    {
		sc_trace ("Restr: evaluating task %ld"
				" restriction %ld\n", task, restriction);
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
	    case 0:	/* Object location. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		result = restr_pass_task_object_location
						(gamestate, var1, var2, var3);
		break;

	    case 1:	/* Object state. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		result = restr_pass_task_object_state
						(gamestate, var1, var2);
		break;

	    case 2:	/* Task state. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		result = restr_pass_task_task_state (gamestate, var1, var2);
		break;

	    case 3:	/* Player and NPCs. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		result = restr_pass_task_char (gamestate, var1, var2, var3);
		break;

	    case 4:	/* Variable. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var4";
		var4 = prop_get_string (bundle, "S<-sisis", vt_key);
		result = restr_pass_task_var
				(gamestate, var1, var2, var3, var4);
		break;

	    default:
		sc_fatal ("restr_pass_task_restriction: unknown"
					" restriction type %ld\n", type);
	    }

	if (restr_trace)
	    {
		sc_trace ("Restr: task %ld restriction %ld is %s\n",
				task, restriction, result ? "PASS" : "FAIL");
	    }
	return result;
}


/* Enumeration of restrictions combination string tokens. */
enum {			TOK_RESTRICTION		= '#',
			TOK_AND			= 'A',
			TOK_OR			= 'O',
			TOK_LPAREN		= '(',
			TOK_RPAREN		= ')',
			TOK_EOS			= '\0' };

/* #O#A(#O#)-style expression, for tokenizing. */
static sc_char		*restr_expression	= NULL;
static sc_uint16	restr_index		= 0;

/*
 * restr_tokenize_start()
 * restr_tokenize_end()
 *
 * Start and wrap up restrictions combinations string tokenization.
 */
static void
restr_tokenize_start (sc_char *expression)
{
	/* Save expression, and restart index. */
	restr_expression	= expression;
	restr_index		= 0;
}

static void
restr_tokenize_end (void)
{
	restr_expression	= NULL;
	restr_index		= 0;
}


/*
 * restr_next_token()
 *
 * Simple tokenizer for restrictions combination expressions.
 */
static sc_char
restr_next_token (void)
{
	assert (restr_expression != NULL);

	/* Find the next non-space, and return it. */
	while (TRUE)
	    {
		/* Return NUL if at string end. */
		if (restr_expression[restr_index] == NUL)
			return restr_expression[restr_index];

		/* Spin on whitespace. */
		restr_index++;
		if (isspace (restr_expression[restr_index - 1]))
			continue;

		/* Return the character just passed. */
		return restr_expression[restr_index - 1];
	    }
}


/* Evaluation values stack. */
static sc_bool		restr_eval_values[MAX_NESTING_DEPTH];
static sc_uint16	restr_eval_stack		= 0;

/*
 * The restriction number to evaluate.  This advances with each call
 * to evaluate and stack a restriction result.
 */
static sc_int32		restr_eval_restriction	= 0;

/*
 * The current gamestate used to evaluate restrictions, and the task
 * in question.
 */
static sc_gamestateref_t restr_eval_gamestate	= NULL;
static sc_int32		restr_eval_task		= 0;

/* The id of the lowest-indexed failing restriction. */
static sc_int32		restr_lowest_fail	= -1;

/*
 * restr_eval_start()
 *
 * Reset the evaluation stack to an empty state, and note the things we
 * have to note for when we need to evaluate a restriction.
 */
static void
restr_eval_start (sc_gamestateref_t gamestate, sc_int32 task)
{
	/* Clear stack. */
	restr_eval_stack	= 0;
	restr_eval_restriction	= 0;

	/* Note evaluation details. */
	restr_eval_gamestate	= gamestate;
	restr_eval_task		= task;

	/* Clear lowest indexed failing restriction. */
	restr_lowest_fail	= -1;
}


/*
 * restr_eval_push()
 *
 * Push a value onto the values stack.
 */
static void
restr_eval_push (sc_bool value)
{
	/* Check for stack overflow. */
	if (restr_eval_stack >= MAX_NESTING_DEPTH)
		sc_fatal ("restr_eval_push: stack overflow\n");

	/* Push value. */
	restr_eval_values[restr_eval_stack++] = value;
}


/*
 * expr_restr_action()
 *
 * Evaluate the effect of an and/or into the values stack.
 */
static void
restr_eval_action (sc_char token)
{
	/* Select action based on parsed token. */
	switch (token)
	    {
	    /* Handle evaluating and pushing a restriction result. */
	    case TOK_RESTRICTION:
		{
		sc_bool		result;

		/* Evaluate and push the next restriction. */
		result = restr_pass_task_restriction
				(restr_eval_gamestate,
				restr_eval_task, restr_eval_restriction);
		restr_eval_push (result);

		/*
		 * If the restriction failed, and there isn't yet a first
		 * failing one set, note this one as the first to fail.
		 */
		if (restr_lowest_fail == -1
				&& !result)
			restr_lowest_fail = restr_eval_restriction;

		/* Increment restriction sequence identifier. */
		restr_eval_restriction++;
		break;
		}

	    /* Handle cases of or-ing/and-ing restrictions. */
	    case TOK_OR: case TOK_AND:
		{
		sc_bool		val1, val2, result = FALSE;
		assert (restr_eval_stack >= 2);

		/* Get the top two stack values. */
		val1 = restr_eval_values[restr_eval_stack - 2];
		val2 = restr_eval_values[restr_eval_stack - 1];

		/* Or, or and, into result. */
		switch (token)
		    {
		    case TOK_OR:	result = val1 || val2;	break;
		    case TOK_AND:	result = val1 && val2;	break;

		    default:
			sc_fatal ("restr_eval_action:"
						" bad token, '%c'\n", token);
		    }

		/* Put result back at top of stack. */
		restr_eval_stack--;
		restr_eval_values[restr_eval_stack - 1] = result;
		break;
		}

	    default:
		sc_fatal ("restr_eval_action: bad token, '%c'\n", token);
	    }
}


/*
 * restr_eval_result()
 *
 * Return the top of the values stack as the evaluation result.
 */
static sc_int32
restr_eval_result (sc_int32 *lowest_fail)
{
	if (restr_eval_stack != 1)
		sc_fatal ("restr_eval_result: values stack not completed\n");
	*lowest_fail = restr_lowest_fail;
	return restr_eval_values[0];
}


/* Parse error jump buffer. */
static jmp_buf		restr_parse_error;

/* Single lookahead token for parser. */
static sc_char		restr_lookahead	= '\0';

/*
 * restr_match()
 *
 * Match a token with an expectation.
 */
static void
restr_match (sc_char c)
{
	if (restr_lookahead == c)
		restr_lookahead = restr_next_token ();
	else
	    {
		sc_error ("restr_match: syntax error, expected %d, got %d\n",
						c, restr_lookahead);
		longjmp (restr_parse_error, 1);
	    }
}


/* Forward declaration for recursion. */
static void	restr_bexpr (void);

/*
 * restr_andexpr()
 * restr_orexpr()
 * restr_bexpr()
 *
 * Expression parsers.  Here we go again...
 */
static void
restr_andexpr (void)
{
	restr_bexpr ();
	while (TRUE)
	    {
		switch (restr_lookahead)
		    {
		    case TOK_AND:
			restr_match (TOK_AND);
			restr_bexpr ();
			restr_eval_action (TOK_AND);
			continue;

		    default:
			return;
		    }
	    }
}

static void
restr_orexpr (void)
{
	restr_andexpr ();
	while (TRUE)
	    {
		switch (restr_lookahead)
		    {
		    case TOK_OR:
			restr_match (TOK_OR);
			restr_andexpr ();
			restr_eval_action (TOK_OR);
			continue;

		    default:
			return;
		    }
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
		restr_orexpr ();
		restr_match (TOK_RPAREN);
		break;

	    default:
		sc_error ("restr_bexpr: syntax error, unexpected %d\n",
							restr_lookahead);
		longjmp (restr_parse_error, 1);
	    }
}


/*
 * restr_get_fail_message()
 *
 * Get the FailMessage for the given task restriction; NULL if none.
 */
static sc_char *
restr_get_fail_message (sc_gamestateref_t gamestate,
		sc_int32 task, sc_int32 restriction)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_char			*message;

	/* Get the restriction message. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Restrictions";
	vt_key[3].integer = restriction;
	vt_key[4].string = "FailMessage";
	message = prop_get_string (bundle, "S<-sisis", vt_key);

	/* Return it, or NULL if empty. */
	return !sc_strempty (message) ? message : NULL;
}


/*
 * restr_debug_trace()
 *
 * Set restrictions tracing on/off.
 */
void
restr_debug_trace (sc_bool flag)
{
	restr_trace = flag;
}


/*
 * restr_evaluate_task_restrictions()
 *
 * Main handler for a given set of task restrictions.  Returns TRUE if
 * the restrictions pass, FALSE if not.  On FALSE, it also returns a
 * fail message string from the restriction deemed to have caused the
 * failure (that is, the first one with a FailMessage property), or
 * NULL if no failing restriction has a FailMessage.
 */
sc_bool
restr_evaluate_task_restrictions (sc_gamestateref_t gamestate,
		sc_int32 task, sc_bool *pass, sc_char **fail_message)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_int32		restr_count;
	sc_char			*pattern;
	sc_bool			result;
	sc_int32		lowest_fail;

	/* Get the count of restrictions on the task. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Restrictions";
	restr_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;

	/* If none, stop now, acting as if all passed. */
	if (restr_count == 0)
	    {
		if (restr_trace)
		    {
			sc_trace ("Restr: task %ld has no"
					" restrictions\n", task);
		    }

		*pass = TRUE;
		*fail_message = NULL;
		return TRUE;
	    }

	/* Get the task's restriction combination pattern. */
	vt_key[2].string = "RestrMask";
	pattern = prop_get_string (bundle, "S<-sis", vt_key);

	if (restr_trace)
	    {
		sc_trace ("Restr: task %ld has %ld restrictions, %s\n",
					task, restr_count, pattern);
	    }

	/* Set up the evaluation stack and tokenizer. */
	restr_eval_start (gamestate, task);
	restr_tokenize_start (pattern);

	/* Set up error handling jump buffer. */
	if (setjmp (restr_parse_error) == 0)
	    {
		/* Parse the pattern, and ensure it ends at string end. */
		restr_lookahead = restr_next_token ();
		restr_orexpr ();
		restr_match (TOK_EOS);

		/* Clean up tokenizer and get the evaluation result. */
		restr_tokenize_end ();
		result = restr_eval_result (&lowest_fail);

		if (restr_trace)
		    {
			sc_trace ("Restr: task %ld restrictions %s\n",
					task, result ? "PASS" : "FAIL");
		    }

		/*
		 * Return the result, and if a restriction fail, then return
		 * the FailMessage of the lowest indexed failing restriction
		 * (or NULL if this restricion has no FailMessage).
		 *
		 * Then return TRUE since parsing and running the restrictions
		 * succeeded (even if the restrictions themselves didn't).
		 */
		*pass = result;
		if (result)
			*fail_message = NULL;
		else
			*fail_message = restr_get_fail_message
						(gamestate, task, lowest_fail);
		return TRUE;
	    }

	/* Parse error -- clean up tokenizer and return fail. */
	restr_tokenize_end ();
	return FALSE;
}

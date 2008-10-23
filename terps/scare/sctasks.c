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
 * o Implements task return FALSE on no output, a slight extension of
 *   current jAsea behavior.
 */

#include <assert.h>
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
enum {			ROOMLIST_NO_ROOMS	= 0,
			ROOMLIST_ONE_ROOM	= 1,
			ROOMLIST_SOME_ROOMS	= 2,
			ROOMLIST_ALL_ROOMS	= 3 };

/* Enumeration of TAF file versions. */
enum {			TAF_VERSION_400		= 400,
			TAF_VERSION_390		= 390,
			TAF_VERSION_380		= 380 };

/* Trace flag, set before running. */
static sc_bool		task_trace		= FALSE;


/*
 * task_has_hints()
 *
 * Return TRUE if the given task offers hints.
 */
sc_bool
task_has_hints (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*question;

	/* A non-empty question implies hints available. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Question";
	question = prop_get_string (bundle, "S<-sis", vt_key);
	return !sc_strempty (question);
}


/*
 * task_get_hint_question()
 * task_get_hint_subtle()
 * task_get_hint_unsubtle()
 *
 * Return the assorted hint text strings.
 */
sc_char *
task_get_hint_question (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*question;

	/* Look up and return the relevant string.  */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Question";
	question = prop_get_string (bundle, "S<-sis", vt_key);
	return question;
}

sc_char *
task_get_hint_subtle (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*hint1;

	/* Look up and return the relevant string.  */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Hint1";
	hint1 = prop_get_string (bundle, "S<-sis", vt_key);
	return hint1;
}

sc_char *
task_get_hint_unsubtle (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*hint2;

	/* Look up and return the relevant string.  */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Hint2";
	hint2 = prop_get_string (bundle, "S<-sis", vt_key);
	return hint2;
}


/*
 * task_can_run_task()
 *
 * Return TRUE if player is in a room where the task can be run.
 */
sc_bool
task_can_run_task (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		type;

	/* Check room list for the task and return it. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Where";
	vt_key[3].string = "Type";
	type = prop_get_integer (bundle, "I<-siss", vt_key);
	switch (type)
	    {
	    case ROOMLIST_NO_ROOMS:			return FALSE;
	    case ROOMLIST_ALL_ROOMS:			return TRUE;

	    case ROOMLIST_ONE_ROOM:
		vt_key[3].string = "Room";
		return prop_get_integer (bundle, "I<-siss", vt_key)
						== gamestate->playerroom;

	    case ROOMLIST_SOME_ROOMS:
		vt_key[3].string = "Rooms";
		vt_key[4].integer = gamestate->playerroom;
		return prop_get_boolean (bundle, "B<-sissi", vt_key);

	    default:
		sc_fatal ("evt_can_see_event: invalid type, %ld\n", type);
		return FALSE;
	    }
}


/*
 * task_move_object()
 *
 * Move an object to a place.
 */
static void
task_move_object (sc_gamestateref_t gamestate,
		sc_int32 object, sc_int32 var2, sc_int32 var3)
{
	sc_var_setref_t		vars	= gamestate->vars;

	/* Select action depending on var2. */
	switch (var2)
	    {
	    case 0:					/* To room */
		if (var3 == 0)
		    {
			if (task_trace)
			    {
				sc_trace ("Task: moving object %ld"
						" to hidden\n", object);
			    }
			gs_object_make_hidden (gamestate, object);
		    }
		else
		    {
			if (task_trace)
			    {
				sc_trace ("Task: moving object %ld to room"
						" %ld\n", object, var3 - 1);
			    }
			if (var3 == 0)
				gs_object_player_get (gamestate, object);
			else
				gs_object_to_room (gamestate, object, var3 - 1);
		    }
		break;

	    case 1:					/* To roomgroup part */
		if (task_trace)
		    {
			sc_trace ("Task: moving object %ld to random"
					" room in group %ld\n", object, var3);
		    }
		gs_object_to_room (gamestate, object,
			lib_random_roomgroup_member (gamestate, var3));
		break;

	    case 2:					/* Into object */
		if (task_trace)
		    {
			sc_trace ("Task: moving object %ld"
					" into %ld\n", object, var3);
		    }
		gs_object_move_into (gamestate, object,
				obj_container_object (gamestate, var3));
		break;

	    case 3:					/* Onto object */
		if (task_trace)
		    {
			sc_trace ("Task: moving object %ld"
					" onto %ld\n", object, var3);
		    }
		gs_object_move_onto (gamestate, object,
				obj_surface_object (gamestate, var3));
		break;

	    case 4:					/* Held by */
		if (task_trace)
		    {
			sc_trace ("Task: moving object %ld"
					" to held by %ld\n", object, var3);
		    }
		if (var3 == 0)				/* Player */
			gs_object_player_get (gamestate, object);
		else if (var3 == 1)			/* Ref character */
			gs_object_npc_get (gamestate, object,
					var_get_ref_character (vars));
		else					/* NPC id */
			gs_object_npc_get (gamestate, object, var3 - 2);
		break;

	    case 5:					/* Worn by */
		if (task_trace)
		    {
			sc_trace ("Task: moving object %ld"
					" to worn by %ld\n", object, var3);
		    }
		if (var3 == 0)				/* Player */
			gs_object_player_wear (gamestate, object);
		else if (var3 == 1)			/* Ref character */
			gs_object_npc_wear (gamestate, object,
					var_get_ref_character (vars));
		else					/* NPC id */
			gs_object_npc_wear (gamestate, object, var3 - 2);
		break;

	    case 6:					/* Same room as */
		{
		sc_int32	room, npc;

		if (task_trace)
		    {
			sc_trace ("Task: moving object %ld"
					" to same room as %ld\n", object, var3);
		    }
		if (var3 == 0)				/* Player */
			room = gamestate->playerroom;
		else if (var3 == 1)			/* Ref character */
		    {
			npc = var_get_ref_character (vars);
			room = gamestate->npcs[npc].location - 1;
		    }
		else					/* NPC id */
		    {
			npc = var3 - 2;
			room = gamestate->npcs[npc].location - 1;
		    }
		gs_object_to_room (gamestate, object, room);
		break;
		}

	    default:
		sc_fatal ("task_move_object:"
					" unknown move type, %ld\n", var2);
	    }
}


/*
 * task_run_move_object_action()
 *
 * Demultiplex an object move action and execute it.
 */
static void
task_run_move_object_action (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2, sc_int32 var3)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		object;

	/* Select depending on value in var1. */
	switch (var1)
	    {
	    case 0:					/* All held */
		for (object = 0; object < gamestate->object_count; object++)
		    {
			if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER)
				task_move_object (gamestate,
							object, var2, var3);
		    }
		break;

	    case 1:					/* All worn */
		for (object = 0; object < gamestate->object_count; object++)
		    {
			if (gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER)
				task_move_object (gamestate,
							object, var2, var3);
		    }
		break;

	    case 2:					/* Ref object */
		object = var_get_ref_object (vars);
		task_move_object (gamestate, object, var2, var3);
		break;

	    default:					/* Dynamic object */
		object = obj_dynamic_object (gamestate, var1 - 3);
		task_move_object (gamestate, object, var2, var3);
		break;
	    }
}


/*
 * task_move_npc_to_room()
 *
 * Move an NPC to a given room.
 */
static void
task_move_npc_to_room (sc_gamestateref_t gamestate, sc_int32 npc, sc_int32 room)
{
	if (task_trace)
		sc_trace ("Task: moving NPC %ld to room %ld\n", npc, room);

	/* Update the NPC's state. */
	if (room < gamestate->room_count)
		gamestate->npcs[npc].location = room + 1;
	else
		gamestate->npcs[npc].location =
			lib_random_roomgroup_member
				(gamestate, room - gamestate->room_count) + 1;

	gamestate->npcs[npc].parent = -1;
	gamestate->npcs[npc].position = 0;
}


/*
 * task_run_move_npc_action()
 *
 * Move player or NPC.
 */
static void
task_run_move_npc_action (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2, sc_int32 var3)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		npc, room, ref_npc = -1;

	/* Player or NPC? */
	if (var1 == 0)
	    {
		/* Player -- decide where to move player to. */
		switch (var2)
		    {
		    case 0:				/* To room */
			gs_move_player_to_room (gamestate, var3);
			return;

		    case 1:				/* To roomgroup part */
			if (task_trace)
			    {
				sc_trace ("Task: moving player to random"
						" room in group %ld\n", var3);
			    }
			gs_move_player_to_room (gamestate,
					lib_random_roomgroup_member
							(gamestate, var3));
			return;

		    case 2:				/* To same room as... */
			switch (var3)
			    {
			    case 0:			/* ...player! */
				return;
			    case 1:			/* ...referenced NPC */
				npc = var_get_ref_character (vars);
				break;
			    default:			/* ...specified NPC */
				npc = var3 - 2;
				break;
			    }

			if (task_trace)
			    {
				sc_trace ("Task: moving player to same"
						" room as NPC %ld\n", npc);
			    }
			room = gamestate->npcs[npc].location - 1;
			gs_move_player_to_room (gamestate, room);
			return;

		    case 3:				/* To standing on */
			gamestate->playerposition = 0;
			gamestate->playerparent =
				obj_standable_object (gamestate, var3 - 1);
			return;

		    case 4:				/* To sitting on */
			gamestate->playerposition = 1;
			gamestate->playerparent =
				obj_standable_object (gamestate, var3 - 1);
			return;

		    case 5:				/* To lying on */
			gamestate->playerposition = 2;
			gamestate->playerparent =
				obj_lieable_object (gamestate, var3 - 1);
			return;

		    default:
			break;
		    }
		sc_fatal ("task_run_move_npc_action:"
				" unknown player move type, %ld\n", var2);
	    }
	else
	    {
		/* NPC -- first find which NPC to move about. */
		if (var1 == 1)
			npc = var_get_ref_character (vars);
		else
			npc = var1 - 2;

		/* Decide where to move the NPC to. */
		switch (var2)
		    {
		    case 0:				/* To room */
			task_move_npc_to_room (gamestate, npc, var3 - 1);
			return;

		    case 1:				/* To roomgroup part */
			if (task_trace)
			    {
				sc_trace ("Task: moving npc %ld to random"
					" room in group %ld\n", npc, var3);
			    }
			task_move_npc_to_room (gamestate, npc,
					lib_random_roomgroup_member
							(gamestate, var3));
			return;

		    case 2:				/* To same room as... */
			switch (var3)
			    {
			    case 0:			/* ...player */
				if (task_trace)
				    {
					sc_trace ("Task: moving NPC %ld to same"
						" room as player\n", npc);
				    }
				task_move_npc_to_room (gamestate, npc,
						gamestate->playerroom);
				break;
			    case 1:			/* ...referenced NPC */
				ref_npc = var_get_ref_character (vars);
				if (task_trace)
				    {
					sc_trace ("Task: moving NPC %ld to same"
						" room as referenced NPC %ld\n",
						npc, ref_npc);
				    }
				room = gamestate->npcs[ref_npc].location - 1;
				task_move_npc_to_room (gamestate, npc, room);
				break;
			    default:			/* ...specified NPC */
				ref_npc = var3 - 2;
				if (task_trace)
				    {
					sc_trace ("Task: moving NPC %ld to same"
					" room as NPC %ld\n", npc, ref_npc);
				    }
				room = gamestate->npcs[ref_npc].location - 1;
				task_move_npc_to_room (gamestate, npc, room);
				break;
			    }
			return;

		    case 3:				/* To standing on */
			gamestate->npcs[npc].position = 0;
			gamestate->npcs[npc].parent =
				obj_standable_object (gamestate, var3);
			return;

		    case 4:				/* To sitting on */
			gamestate->npcs[npc].position = 1;
			gamestate->npcs[npc].parent =
				obj_standable_object (gamestate, var3);
			return;

		    case 5:				/* To lying on */
			gamestate->npcs[npc].position = 2;
			gamestate->npcs[npc].parent =
				obj_lieable_object (gamestate, var3);
			return;

		    default:
			break;
		    }
		sc_fatal ("task_run_move_npc_action:"
				" unknown NPC move type, %ld\n", var2);
	    }
	sc_fatal ("task_run_move_npc_action: invalid fall-through\n");
}


/*
 * task_run_change_object_status()
 *
 * Change the status of an object.
 */
static void
task_run_change_object_status (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		object, openable, lockable;

	if (task_trace)
	    {
		sc_trace ("Task: setting status of stateful"
				" object %ld to %ld\n", var1, var2);
	    }

	/* Identify the target object. */
	object = obj_stateful_object (gamestate, var1);

	/* See if openable. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Openable";
	openable = prop_get_integer (bundle, "I<-sis", vt_key);
	if (openable > 0)
	    {
		/* See if lockable. */
		vt_key[2].string = "Key";
		lockable = prop_get_integer (bundle, "I<-sis", vt_key);
		if (lockable >= 0)
		    {
			/* Lockable. */
			if (var2 <= 2)
				gs_set_object_openness
						(gamestate, object, var2 + 5);
			else
				gs_set_object_state
						(gamestate, object, var2 - 2);
		    }
		else
		    {
			/* Not lockable, though openable. */
			if (var2 <= 1)
				gs_set_object_openness
						(gamestate, object, var2 + 5);
			else
				gs_set_object_state
						(gamestate, object, var2 - 1);
		    }
	    }
	else
		/* Not openable. */
		gs_set_object_state (gamestate, object, var2 + 1);

	if (task_trace)
	    {
		sc_trace ("Task:     openness of object %ld is now %ld\n",
			object, gs_get_object_openness (gamestate, object));
		sc_trace ("Task:     state of object %ld is now %ld\n",
			object, gs_get_object_state (gamestate, object));
	    }
}


/*
 * task_run_change_variable_action()
 *
 * Change a variable's value in inscrutable ways.
 */
static void
task_run_change_variable_action (sc_gamestateref_t gamestate,
		sc_int32 var1, sc_int32 var2, sc_int32 var3,
		sc_char *expr, sc_int32 var5)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3];
	sc_char			*name;
	sc_int32		type;
	sc_int32		final;
	sc_char			*sfinal;

	/* Get the name and type of the variable being addressed. */
	vt_key[0].string = "Variables";
	vt_key[1].integer = var1;
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	vt_key[2].string = "Type";
	type = prop_get_integer (bundle, "I<-sis", vt_key);

	/* Select first based on variable type. */
	switch (type)
	    {
	    case 0:					/* Integer */

		/* Select again based on action type. */
		switch (var2)
		    {
		    case 0:				/* Var = */
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) = %ld\n",
							var1, name, var3);
			    }
			var_put_integer (vars, name, var3);
			return;

		    case 1:				/* Var += */
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) += %ld\n",
							var1, name, var3);
			    }
			final = var_get_integer (vars, name) + var3;
			var_put_integer (vars, name, final);
			return;

		    case 2:				/* Var = rnd(range) */
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) ="
				" random(%ld,%ld)\n", var1, name, var3, var5);
			    }
			final = sc_randomint (var3, var5);
			var_put_integer (vars, name, final);
			return;

		    case 3:				/* Var += rnd(range) */
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) +="
				" random(%ld,%ld)\n", var1, name, var3, var5);
			    }
			final = var_get_integer (vars, name)
						+ sc_randomint (var3, var5);
			var_put_integer (vars, name, final);
			return;

		    case 4:				/* Var = ref */
			final = var_get_ref_number (vars);
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) = ref,"
						" %ld\n", var1, name, final);
			    }
			var_put_integer (vars, name, final);
			return;

		    case 5:				/* Var = expr */
			if (!expr_eval_numeric_expression (expr, vars, &final))
			    {
				sc_error ("task_run_change_variable_action:"
					" invalid expression, %s\n", expr);
				final = 0;
			    }
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) = %s, %ld\n",
						var1, name, expr, final);
			    }
			var_put_integer (vars, name, final);
			return;

		    default:
			sc_fatal ("task_run_change_variable_action:"
				" unknown integer change type, %ld\n", var2);
		    }

	    case 1:					/* String */

		/* Select again based on action type. */
		switch (var2)
		    {
		    case 0:				/* Var = text literal */
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) = \"%s\"\n",
							var1, name, expr);
			    }
			var_put_string (vars, name, expr);
			return;

		    case 1:				/* Var = ref */
			sfinal = var_get_ref_text (vars);
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s)"
					" = ref, \"%s\"\n", var1, name, sfinal);
			    }
			var_put_string (vars, name, sfinal);
			return;

		    case 2:				/* Var = expr */
			if (!expr_eval_string_expression (expr, vars, &sfinal))
			    {
				sc_error ("task_run_change_variable_action:"
					" invalid string expression, %s\n",
									expr);
				sfinal = sc_malloc
						(strlen ("[expr error]") + 1);
				strcpy (sfinal, "[expr error]");
			    }
			if (task_trace)
			    {
				sc_trace ("Task: variable %ld (%s) = %s, %s\n",
						var1, name, expr, sfinal);
			    }
			var_put_string (vars, name, sfinal);
			sc_free (sfinal);
			return;

		    default:
			sc_fatal ("task_run_change_variable_action:"
				" unknown string change type, %ld\n", var2);
		    }

	    default:
		sc_fatal ("task_run_change_variable_action:"
				" invalid variable type, %ld\n", type);
	    }
}


/*
 * task_run_change_score_action()
 *
 * Change game score.
 */
static void
task_run_change_score_action (sc_gamestateref_t gamestate,
	       sc_int32 task, sc_int32 var1)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;

	/* Increasing or decreasing the score? */
	if (var1 > 0)
	    {
		sc_bool		increase_score;

		/* See if this task is already scored. */
		increase_score = !gs_get_task_scored (gamestate, task);
		if (!increase_score)
		    {
			sc_vartype_t	vt_key[3];
			sc_int32	version;

			if (task_trace)
				sc_trace ("Task: already scored"
							" task %ld\n", var1);

			/* Version 3.8 games permit tasks to rescore. */
			vt_key[0].string = "Version";
			version = prop_get_integer (bundle, "I<-s", vt_key);
			if (version == TAF_VERSION_380)
			    {
				vt_key[0].string = "Tasks";
				vt_key[1].integer = task;
				vt_key[2].string = "SingleScore";
				increase_score = !prop_get_boolean
						(bundle, "B<-sis", vt_key);

				if (increase_score)
				    {
					if (task_trace)
						sc_trace ("Task: rescoring"
						" version 3.8 task anyway\n");
				    }
			    }
		    }

		/*
		 * Increase the score if not yet scored or a version 3.8
		 * multiple scoring task, and note as a scored task.
		 */
		if (increase_score)
		    {
			if (task_trace)
				sc_trace ("Task: increased score by"
							" %ld\n", var1);

			gamestate->score += var1;
			gs_set_task_scored (gamestate, task, TRUE);
		    }
	    }
	else if (var1 < 0)
	    {
		/* Decrease the score. */
		if (task_trace)
			sc_trace ("Task: decreased score by %ld\n", -(var1));

		gamestate->score += var1;
	    }
}


/*
 * task_run_set_task_action()
 *
 * Redirect to another task.
 */
static sc_bool
task_run_set_task_action (sc_gamestateref_t gamestate,
	       sc_int32 var1, sc_int32 var2)
{
	sc_bool		status = FALSE;

	/* Select based on var1. */
	if (var1 == 0)
	    {
		/* Redirect forwards. */
		if (task_can_run_task (gamestate, var2))
		    {
			if (task_trace)
			    {
				sc_trace ("Task: redirecting to"
						" task %ld\n", var2);
			    }
			status = task_run_task (gamestate, var2, TRUE);
		    }
		else
		    {
			if (task_trace)
			    {
				sc_trace ("Task: can't redirect to"
						" task %ld\n", var2);
			    }
		    }
	    }
	else
	    {
		/* Undo task. */
		gs_set_task_done (gamestate, var2, FALSE);
		if (task_trace)
			sc_trace ("Task: reversing task %ld\n", var2);
	    }

	return status;
}


/*
 * task_run_end_game_action()
 *
 * End of game task action.
 */
static sc_bool
task_run_end_game_action (sc_gamestateref_t gamestate, sc_int32 var1)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_bool			status = FALSE;

	/* Print a message based on var1. */
	switch (var1)
	    {
	    case 0:
		{
		sc_vartype_t	vt_key[2];
		sc_char		*wintext;

		/* Get game WinText. */
		vt_key[0].string = "Header";
		vt_key[1].string = "WinText";
		wintext = prop_get_string (bundle, "S<-ss", vt_key);

		/* Print WinText, if any defined, otherwise a default. */
		if (!sc_strempty (wintext))
		    {
			pf_buffer_string (filter, wintext);
			pf_buffer_character (filter, '\n');
		    }
		else
			pf_buffer_string (filter, "Congratulations!\n");

		/* Handle any associated WinRes resource. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "WinRes";
		res_handle_resource (gamestate, "ss", vt_key);

		status = TRUE;
		break;
		}

	    case 1:
		pf_buffer_string (filter, "Better luck next time.\n");
		status = TRUE;
		break;

	    case 2:
		pf_buffer_string (filter, "I'm afraid you are dead!\n");
		status = TRUE;
		break;

	    case 3:
		break;

	    default:
		sc_fatal ("task_run_end_game_action:"
					" invalid type, %ld\n", var1);
	    }

	/* Stop the game, and note that it's not resumeable. */
	gamestate->is_running = FALSE;
	gamestate->has_completed = TRUE;

	return status;
}


/*
 * task_run_task_action()
 *
 * Demultiplexer for task actions.
 */
static sc_bool
task_run_task_action (sc_gamestateref_t gamestate,
			sc_int32 task, sc_int32 action)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		type, var1, var2, var3, var5;
	sc_char			*expr;
	sc_bool			status = FALSE;

	/* Get the task action type. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Actions";
	vt_key[3].integer = action;
	vt_key[4].string = "Type";
	type = prop_get_integer (bundle, "I<-sisis", vt_key);

	/* Demultiplex depending on type. */
	switch (type)
	    {
	    case 0:	/* Move object. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		task_run_move_object_action (gamestate, var1, var2, var3);
		break;

	    case 1:	/* Move player/NPC. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		task_run_move_npc_action (gamestate, var1, var2, var3);
		break;

	    case 2:	/* Change object status. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		task_run_change_object_status (gamestate, var1, var2);
		break;

	    case 3:	/* Change variable. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Expr";
		expr = prop_get_string (bundle, "S<-sisis", vt_key);
		vt_key[4].string = "Var5";
		var5 = prop_get_integer (bundle, "I<-sisis", vt_key);
		task_run_change_variable_action
				(gamestate, var1, var2, var3, expr, var5);
		break;

	    case 4:	/* Change score. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		task_run_change_score_action (gamestate, task, var1);
		break;

	    case 5:	/* Execute/unset task. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		status = task_run_set_task_action (gamestate, var1, var2);
		break;

	    case 6:	/* End game. */
		vt_key[4].string = "Var1";
		var1 = prop_get_integer (bundle, "I<-sisis", vt_key);
		status = task_run_end_game_action (gamestate, var1);
		break;

	    case 7:	/* Battle options, ignored for now... */
		break;

	    default:
		sc_fatal ("task_run_task_action: unknown"
						" action type %ld\n", type);
	    }

	return status;
}


/*
 * task_run_task()
 *
 * Run a task, providing restrictions permit, in the given direction.
 * Return true if the task ran, or we handled it in some complete way,
 * for example by outputting a message describing what prevented it, or
 * why it couldn't be done.
 */
sc_bool
task_run_task (sc_gamestateref_t gamestate, sc_int32 task, sc_bool forwards)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4], vt_rvalue;
	sc_char			*completetext;
	sc_int32		action, action_count;
	sc_int32		showroomdesc;
	sc_char			*additionalmessage;
	sc_bool			restrictions_passed;
	sc_char			*fail_message;
	sc_int32		alert_count, alert;
	sc_bool			status;

	/* See if the task can be run at all. */
	if (!task_can_run_task (gamestate, task))
		return FALSE;

	if (task_trace)
	    {
		sc_trace ("Task: running task %ld %s\n", task,
				forwards ? "forwards" : "backwards");
	    }

	/* Set up some invariant key parts, used throughout. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;

	/* Handle restrictions. */
	if (!restr_evaluate_task_restrictions (gamestate, task,
					&restrictions_passed, &fail_message))
	    {
		sc_error ("task_run_task: restrictions error, %ld\n", task);
		return FALSE;
	    }
	if (!restrictions_passed)
	    {
		if (task_trace)
			sc_trace ("Task: restrictions failed\n");

		if (fail_message != NULL)
		    {
			if (task_trace)
				sc_trace ("Task: aborted\n");

			/*
			 * Print a message, and return TRUE since we can
			 * consider this task "done".
			 */
			pf_buffer_string (filter, fail_message);
			pf_buffer_character (filter, '\n');
			return TRUE;
		    }

		/* Task not done; look for more possibilities. */
		return FALSE;
	    }

	/* Start considering task output tracking. */
	status = FALSE;

	/*
	 * If reversing, print any reverse message for the task, and undo
	 * the task, then return.
	 */
	if (!forwards)
	    {
		sc_char		*reversemessage;

		/* If not yet done, we can hardly reverse it. */
		if (!gs_get_task_done (gamestate, task))
			return status;

		vt_key[2].string = "ReverseMessage";
		reversemessage = prop_get_string (bundle, "S<-sis", vt_key);
		if (!sc_strempty (reversemessage))
		    {
			pf_buffer_string (filter, reversemessage);
			pf_buffer_character (filter, '\n');
			status |= TRUE;
		    }

		/* Undo the task. */
		gs_set_task_done (gamestate, task, FALSE);
		return status;
	    }

	/* See if we are trying to repeat a task that's not repeatable. */
	if (gs_get_task_done (gamestate, task))
	    {
		sc_bool		repeatable;

		vt_key[2].string = "Repeatable";
		repeatable = prop_get_boolean (bundle, "B<-sis", vt_key);
		if (!repeatable)
		    {
			sc_char		*repeattext;

			vt_key[2].string = "RepeatText";
			repeattext = prop_get_string (bundle, "S<-sis", vt_key);
			if (!sc_strempty (repeattext))
			    {
				if (task_trace)
				    {
					sc_trace ("Task: trying to repeat"
					" completed action, aborting\n");
				    }

				pf_buffer_string (filter, repeattext);
				pf_buffer_character (filter, '\n');
				status |= TRUE;
				return status;
			    }

			/*
			 * Task done, yet not repeatable, so don't consider
			 * this case handled.
			 */
			return status;
		    }
	    }

	/* Mark the task as done. */
	gs_set_task_done (gamestate, task, TRUE);

	/* Start NPC walks based on alerts. */
	vt_key[2].string = "NPCWalkAlert";
	alert_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;
	for (alert = 0; alert < alert_count; alert += 2)
	    {
		sc_int32	npc, walk;

		vt_key[3].integer = alert;
		npc = prop_get_integer (bundle, "I<-sisi", vt_key);
		vt_key[3].integer = alert + 1;
		walk = prop_get_integer (bundle, "I<-sisi", vt_key);
		npc_start_npc_walk (gamestate, npc, walk);
	    }

	/* Print task completed text. */
	vt_key[2].string = "CompleteText";
	completetext = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strempty (completetext))
	    {
		pf_buffer_string (filter, completetext);
		pf_buffer_character (filter, '\n');
		status |= TRUE;
	    }

	/* Handle any task completion resource. */
	vt_key[2].string = "Res";
	res_handle_resource (gamestate, "sis", vt_key);

	/*
	 * Run every task action associated with the task.  If any action
	 * ends the game, return immediately.
	 */
	vt_key[2].string = "Actions";
	action_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
				? vt_rvalue.integer : 0;
	for (action = 0; action < action_count; action++)
	   {
		status |= task_run_task_action (gamestate, task, action);

		/* Did this action end the game? */
		if (!gamestate->is_running)
		    {
			if (task_trace)
			    {
				sc_trace ("Task: task %ld action %ld, game"
						" over, return %s\n",
						task, action,
						status ? "TRUE" : "FALSE");
			    }
			return status;
		    }
	   }

	/* Print out any additional messages about the task. */
	vt_key[2].string = "ShowRoomDesc";
	showroomdesc = prop_get_integer (bundle, "I<-sis", vt_key);
	if (showroomdesc != 0)
	    {
		lib_print_room_name (gamestate, showroomdesc - 1);
		lib_print_room_description (gamestate, showroomdesc - 1);
		status |= TRUE;
	    }

	vt_key[2].string = "AdditionalMessage";
	additionalmessage = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strempty (additionalmessage))
	    {
		pf_buffer_string (filter, additionalmessage);
		pf_buffer_character (filter, '\n');
		status |= TRUE;
	    }

	/* Return status -- TRUE if matched and we output something. */
	if (task_trace)
	    {
		sc_trace ("Task: task %ld finished, return %s\n",
				task, status ? "TRUE" : "FALSE");
	    }
	return status;
}


/*
 * task_debug_trace()
 *
 * Set task tracing on/off.
 */
void
task_debug_trace (sc_bool flag)
{
	task_trace = flag;
}

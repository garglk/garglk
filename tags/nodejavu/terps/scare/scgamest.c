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
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_uint32	GAMESTATE_MAGIC		= 0x35AED26E;
enum {			TAFVAR_NUMERIC		= 0,
			TAFVAR_STRING		= 1 };
enum {			ROOMLIST_NO_ROOMS	= 0,
			ROOMLIST_ONE_ROOM	= 1,
			ROOMLIST_SOME_ROOMS	= 2,
			ROOMLIST_ALL_ROOMS	= 3,
			ROOMLIST_NPC_PART	= 4 };


/*
 * gs_create()
 *
 * Create and initialize a game state.
 */
sc_gamestateref_t
gs_create (sc_var_setref_t vars, sc_prop_setref_t bundle,
		sc_printfilterref_t filter)
{
	sc_gamestateref_t	gamestate;
	sc_vartype_t		vt_key[4], vt_rvalue;
	sc_int32		index;

	/* Create the initial state structure. */
	gamestate = sc_malloc (sizeof (*gamestate));
	gamestate->magic		= GAMESTATE_MAGIC;

	/* Store the variables, properties bundle, and filter references. */
	gamestate->vars			= vars;
	gamestate->bundle		= bundle;
	gamestate->filter		= filter;

	/* Initialize for no debugger. */
	gamestate->debugger		= NULL;

	/* Initialize the undo buffers to NULL for now. */
	gamestate->temporary		= NULL;
	gamestate->undo			= NULL;
	gamestate->undo_available	= FALSE;

	/* Create rooms state array. */
	vt_key[0].string = "Rooms";
	gamestate->room_count =
		(prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	gamestate->rooms = sc_malloc
		(gamestate->room_count * sizeof (*gamestate->rooms));

	/* Set up initial rooms states. */
	for (index = 0; index < gamestate->room_count; index++)
		gs_set_room_seen (gamestate, index, FALSE);

	/* Create objects state array. */
	vt_key[0].string = "Objects";
	gamestate->object_count =
		(prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	gamestate->objects = sc_malloc
		(gamestate->object_count * sizeof (*gamestate->objects));

	/* Set up initial object states. */
	for (index = 0; index < gamestate->object_count; index++)
	    {
		sc_char		*inroomdesc;
		sc_bool		is_static, unmoved;

		vt_key[1].integer = index;

		vt_key[2].string = "Static";
		is_static = prop_get_boolean (bundle, "B<-sis", vt_key);
		if (is_static)
		    {
			sc_int32	type;

			vt_key[2].string = "Where";
			vt_key[3].string = "Type";
			type = prop_get_integer (bundle, "I<-siss", vt_key);
			if (type == ROOMLIST_NPC_PART)
			    {
				sc_int32	parent;

				gamestate->objects[index].position
								= OBJ_PART_NPC;

				vt_key[2].string = "Parent";
				parent = prop_get_integer
						(bundle, "I<-sis", vt_key) - 1;
				gamestate->objects[index].parent = parent;
			    }
			else
				gs_object_make_hidden (gamestate, index);
		    }
		else
		    {
			sc_int32	initialparent, initialposition;

			vt_key[2].string = "Parent";
			initialparent = prop_get_integer
						(bundle, "I<-sis", vt_key);
			vt_key[2].string = "InitialPosition";
			initialposition = prop_get_integer
						(bundle, "I<-sis", vt_key);
			switch (initialposition)
			    {
			    case 0:	/* Hidden. */
				gs_object_make_hidden (gamestate, index);
				break;

			    case 1:	/* Held. */
				if (initialparent == 0)		/* By player. */
					gs_object_player_get
							(gamestate, index);
				else				/* By NPC. */
					gs_object_npc_get (gamestate, index,
							initialparent - 1);
				break;

			    case 2:	/* In container. */
				gs_object_move_into (gamestate, index,
					obj_container_object (gamestate,
								initialparent));
				break;

			    case 3:	/* On surface. */
				gs_object_move_onto (gamestate, index,
					obj_surface_object (gamestate,
								initialparent));
				break;

			    default:	/* In room, or worn by player/NPC. */
				if (initialposition >= 4
						&& initialposition <
						4 + gamestate->room_count)
					gs_object_to_room (gamestate, index,
							initialposition - 4);
				else if (initialposition ==
						4 + gamestate->room_count)
				    {
					if (initialparent == 0)
						gs_object_player_wear
							(gamestate, index);
					else
						gs_object_npc_wear
							(gamestate, index,
							initialparent - 1);
				    }
				else
				    {
					sc_error ("gs_create_gamestate: "
						" object in out of bounds"
						" room, %ld\n",
						initialposition - 4
						- gamestate->room_count);
					gs_object_to_room (gamestate,
								index, -2);
				    }
			    }
		    }

		vt_key[2].string = "CurrentState";
		gs_set_object_state (gamestate, index,
				prop_get_integer (bundle, "I<-sis", vt_key));

		vt_key[2].string = "Openable";
		gs_set_object_openness (gamestate, index,
				prop_get_integer (bundle, "I<-sis", vt_key));

		gs_set_object_seen (gamestate, index, FALSE);

		vt_key[2].string = "InRoomDesc";
		inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);
		if (!sc_strempty (inroomdesc))
		    {
			vt_key[2].string = "OnlyWhenNotMoved";
			if (prop_get_integer (bundle, "I<-sis", vt_key) == 1)
				unmoved = TRUE;
			else
				unmoved = FALSE;
		    }
		else
			unmoved = FALSE;
		gs_set_object_unmoved (gamestate, index, unmoved);
	    }

	/* Create tasks state array. */
	vt_key[0].string = "Tasks";
	gamestate->task_count =
		(prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	gamestate->tasks = sc_malloc
		(gamestate->task_count * sizeof (*gamestate->tasks));

	/* Set up initial tasks states. */
	for (index = 0; index < gamestate->task_count; index++)
	    {
		gs_set_task_done (gamestate, index, FALSE);
		gs_set_task_scored (gamestate, index, FALSE);
	    }

	/* Create events state array. */
	vt_key[0].string = "Events";
	gamestate->event_count =
		(prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	gamestate->events = sc_malloc
		(gamestate->event_count * sizeof (*gamestate->events));

	/* Set up initial events states. */
	for (index = 0; index < gamestate->event_count; index++)
	    {
		sc_int32	startertype;

		vt_key[1].integer = index;

		vt_key[2].string = "StarterType";
		startertype = prop_get_integer (bundle, "I<-sis", vt_key);
		switch (startertype)
		    {
		    case 1:
			gs_set_event_state (gamestate, index, ES_WAITING);
			gs_set_event_time (gamestate, index, 0);
			break;

		    case 2:
			{
			sc_int32	start, end;

			gs_set_event_state (gamestate, index, ES_WAITING);
			vt_key[2].string = "StartTime";
			start = prop_get_integer (bundle, "I<-sis", vt_key);
			vt_key[2].string = "EndTime";
			end = prop_get_integer (bundle, "I<-sis", vt_key);
			gs_set_event_time (gamestate, index,
						sc_randomint (start, end));
			break;
			}

		    case 3:
			gs_set_event_state (gamestate, index, ES_AWAITING);
			gs_set_event_time (gamestate, index, 0);
			break;
		    }
	    }

	/* Create NPCs state array. */
	vt_key[0].string = "NPCs";
	gamestate->npc_count =
		(prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	gamestate->npcs = sc_malloc
		(gamestate->npc_count * sizeof (*gamestate->npcs));

	/* Set up initial NPCs states. */
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		sc_int32	walk;

		gamestate->npcs[index].position = 0;
		gamestate->npcs[index].parent = -1;
		gamestate->npcs[index].seen = FALSE;

		vt_key[1].integer = index;

		vt_key[2].string = "StartRoom";
		gamestate->npcs[index].location =
				prop_get_integer (bundle, "I<-sis", vt_key);

		vt_key[2].string = "Walks";
		gamestate->npcs[index].walkstep_count =
			(prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
		gamestate->npcs[index].walksteps = sc_malloc
			(gamestate->npcs[index].walkstep_count
				* sizeof (*gamestate->npcs[index].walksteps));

		for (walk = 0; walk
				< gamestate->npcs[index].walkstep_count; walk++)
			gamestate->npcs[index].walksteps[walk] = 0;
	    }

	/* Set up the player portions of the game state. */
	vt_key[0].string = "Header";
	vt_key[1].string = "StartRoom";
	gamestate->playerroom	  = prop_get_integer (bundle, "I<-ss", vt_key);
	vt_key[0].string = "Globals";
	vt_key[1].string = "ParentObject";
	gamestate->playerparent	  = prop_get_integer
						(bundle, "I<-ss", vt_key) - 1;
	vt_key[1].string = "Position";
	gamestate->playerposition = prop_get_integer (bundle, "I<-ss", vt_key);

	/* Initialize score notifications from game properties. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "NoScoreNotify";
	gamestate->notify_score_change	= !prop_get_boolean
						(bundle, "B<-ss", vt_key);

	/* Miscellaneous state defaults. */
	gamestate->turns		= 0;
	gamestate->score		= 0;
	gamestate->bold_room_names	= TRUE;
	gamestate->verbose		= FALSE;
	gamestate->current_room_name	= NULL;
	gamestate->status_line		= NULL;
	gamestate->title		= NULL;
	gamestate->author		= NULL;
	gamestate->hint_text		= NULL;

	/* Resource controls. */
	res_clear_resource (&gamestate->requested_sound);
	res_clear_resource (&gamestate->requested_graphic);
	res_clear_resource (&gamestate->playing_sound);
	res_clear_resource (&gamestate->displayed_graphic);
	gamestate->stop_sound		= FALSE;
	gamestate->sound_active		= FALSE;

	/* Non-gamestate conveniences. */
	gamestate->is_running		= FALSE;
	gamestate->is_admin		= FALSE;
	gamestate->has_completed	= FALSE;
	gamestate->waitturns		= 0;
	gamestate->do_again		= FALSE;
	gamestate->do_restart		= FALSE;
	gamestate->do_restore		= FALSE;

	gamestate->object_references = sc_malloc
		(gamestate->object_count * sizeof
					(*gamestate->object_references));
	for (index = 0; index < gamestate->object_count; index++)
		gamestate->object_references[index] = FALSE;

	gamestate->npc_references = sc_malloc
		(gamestate->npc_count * sizeof (*gamestate->npc_references));
	for (index = 0; index < gamestate->npc_count; index++)
		gamestate->npc_references[index] = FALSE;

	gamestate->is_object_pronoun	= FALSE;
	gamestate->is_npc_pronoun	= FALSE;

	gamestate->it_object		= -1;
	gamestate->him_npc		= -1;
	gamestate->her_npc		= -1;
	gamestate->it_npc		= -1;

	/* Clear the quit jump buffer for tidiness. */
	memset (gamestate->quitter, 0, sizeof (*gamestate->quitter));

	/* Return the constructed game state. */
	return gamestate;
}


/*
 * gs_string_copy()
 *
 * Helper for gs_copy(), copies one malloc'ed string to another, or NULL
 * if from is NULL, taking care not to leak memory.
 */
static void
gs_string_copy (sc_char **to_string, sc_char *from_string)
{
	/* Free any current contents of to_string. */
	if (*to_string != NULL)
		sc_free (*to_string);

	/* Copy from_string if set, otherwise set to_string to NULL. */
	if (from_string != NULL)
	    {
		*to_string = sc_malloc (strlen (from_string) + 1);
		strcpy (*to_string, from_string);
	    }
	else
		*to_string = NULL;
}


/*
 * gs_copy()
 *
 * Deep-copy the dynamic parts of a gamestate onto another existing
 * gamestate structure.
 */
void
gs_copy (sc_gamestateref_t to, sc_gamestateref_t from)
{
	sc_prop_setref_t	bundle	= from->bundle;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_int32		var_count, var, npc;

	/*
	 * Copy over references to the properties bundle and filter.  The
	 * debugger is specifically excluded, as it's considered to be
	 * tied to the gamestate.
	 */
	to->bundle	= from->bundle;
	to->filter	= from->filter;

	/* Copy over references to the undo buffers. */
	to->temporary		= from->temporary;
	to->undo		= from->undo;
	to->undo_available	= from->undo_available;

	/* Copy over all variables values. */
	vt_key[0].string = "Variables";
	var_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	for (var = 0; var < var_count; var++)
	    {
		sc_char		*name;
		sc_int32	var_type;

		vt_key[1].integer = var;

		vt_key[2].string = "Name";
		name = prop_get_string (bundle, "S<-sis", vt_key);
		vt_key[2].string = "Type";
		var_type = prop_get_integer (bundle, "I<-sis", vt_key);

		switch (var_type)
		    {
		    case TAFVAR_NUMERIC:
			var_put_integer (to->vars, name,
					var_get_integer (from->vars, name));
			break;

		    case TAFVAR_STRING:
			var_put_string (to->vars, name,
					var_get_string (from->vars, name));
			break;

		    default:
			sc_fatal ("gs_copy:"
				" unknown variable type, %ld\n", var_type);
		    }
	    }

	/* Copy over the variable timestamp. */
	var_set_elapsed_seconds (to->vars,
				var_get_elapsed_seconds (from->vars));

	/* Copy over room states. */
	assert (to->room_count == from->room_count);
	memcpy (to->rooms, from->rooms,
				from->room_count * sizeof (*from->rooms));

	/* Copy over object states. */
	assert (to->object_count == from->object_count);
	memcpy (to->objects, from->objects,
				from->object_count * sizeof (*from->objects));

	/* Copy over task states. */
	assert (to->task_count == from->task_count);
	memcpy (to->tasks, from->tasks,
				from->task_count * sizeof (*from->tasks));

	/* Copy over event states. */
	assert (to->event_count == from->event_count);
	memcpy (to->events, from->events,
				from->event_count * sizeof (*from->events));

	/* Copy over NPC states individually, to avoid walks problems. */
	for (npc = 0; npc < from->npc_count; npc++)
	    {
		to->npcs[npc].location	     = from->npcs[npc].location;
		to->npcs[npc].position	     = from->npcs[npc].position;
		to->npcs[npc].parent	     = from->npcs[npc].parent;
		to->npcs[npc].seen	     = from->npcs[npc].seen;
		to->npcs[npc].walkstep_count = from->npcs[npc].walkstep_count;

		/* Copy over NPC walks information. */
		assert (to->npcs[npc].walkstep_count
					== from->npcs[npc].walkstep_count);
		memcpy (to->npcs[npc].walksteps, from->npcs[npc].walksteps,
				from->npcs[npc].walkstep_count
					* sizeof (*from->npcs[npc].walksteps));
	    }

	/* Copy over player information. */
	to->playerroom		= from->playerroom;
	to->playerposition	= from->playerposition;
	to->playerparent	= from->playerparent;

	/*
	 * Copy over miscellaneous other details.  Specifically exclude
	 * bold rooms, verbose, and score notification, so that they are
	 * invariant across copies, particularly undo/restore.
	 */
	to->turns		= from->turns;
	to->score		= from->score;

	gs_string_copy (&to->current_room_name, from->current_room_name);
	gs_string_copy (&to->status_line, from->status_line);
	gs_string_copy (&to->title, from->title);
	gs_string_copy (&to->author, from->author);
	gs_string_copy (&to->hint_text, from->hint_text);

	/*
	 * Specifically exclude playing sound and displayed graphic from the
	 * copy so that they remain invariant across gamestate copies.
	 */
	to->requested_sound	= from->requested_sound;
	to->requested_graphic	= from->requested_graphic;
	to->stop_sound		= from->stop_sound;

	to->is_running		= from->is_running;
	to->is_admin		= from->is_admin;
	to->has_completed	= from->has_completed;

	to->waitturns		= from->waitturns;
	to->do_again		= from->do_again;
	to->do_restart		= from->do_restart;
	to->do_restore		= from->do_restore;

	memcpy (to->object_references, from->object_references,
			from->object_count * sizeof (*from->object_references));
	memcpy (to->npc_references, from->npc_references,
			from->npc_count * sizeof (*from->npc_references));

	to->is_object_pronoun	= from->is_object_pronoun;
	to->is_npc_pronoun	= from->is_npc_pronoun;

	to->it_object		= from->it_object;
	to->him_npc		= from->him_npc;
	to->her_npc		= from->her_npc;
	to->it_npc		= from->it_npc;

	/* Copy over the quit jump buffer. */
	memcpy (to->quitter, from->quitter, sizeof (*from->quitter));
}


/*
 * gs_destroy()
 *
 * Free all the memory associated with a game state.
 */
void
gs_destroy (sc_gamestateref_t gamestate)
{
	sc_int32	npc;

	/* Free the malloc'ed state arrays. */
	sc_free (gamestate->rooms);
	sc_free (gamestate->objects);
	sc_free (gamestate->tasks);
	sc_free (gamestate->events);
	for (npc = 0; npc < gamestate->npc_count; npc++)
		sc_free (gamestate->npcs[npc].walksteps);
	sc_free (gamestate->npcs);

	/* Free the malloc'ed object and NPC references. */
	sc_free (gamestate->object_references);
	sc_free (gamestate->npc_references);

	/* Free malloc'ed gamestate strings. */
	if (gamestate->current_room_name != NULL)
		sc_free (gamestate->current_room_name);
	if (gamestate->status_line != NULL)
		sc_free (gamestate->status_line);
	if (gamestate->title != NULL)
		sc_free (gamestate->title);
	if (gamestate->author != NULL)
		sc_free (gamestate->author);
	if (gamestate->hint_text != NULL)
		sc_free (gamestate->hint_text);

	/* Shred and free the game state itself. */
	memset (gamestate, 0, sizeof (*gamestate));
	sc_free (gamestate);
}


/*
 * gs_is_gamestate_valid()
 *
 * Return TRUE if pointer is a valid gamestate, false otherwise.
 */
sc_bool
gs_is_gamestate_valid (sc_gamestateref_t gamestate)
{
	return gamestate != NULL && gamestate->magic == GAMESTATE_MAGIC;
}


/*
 * gs_move_player_to_room()
 * gs_player_in_room()
 *
 * Move the player to a given room, and check presence in a given room.
 */
void
gs_move_player_to_room (sc_gamestateref_t gamestate, sc_int32 room)
{
	if (room < gamestate->room_count)
		gamestate->playerroom = room;
	else
		gamestate->playerroom = lib_random_roomgroup_member (gamestate,
						room - gamestate->room_count);

	gamestate->playerparent = -1;
	gamestate->playerposition = 0;
}

sc_bool
gs_player_in_room (sc_gamestateref_t gamestate, sc_int32 room)
{
	return gamestate->playerroom == room;
}


/*
 * sc_gs_*()
 *
 * Gamestate accessors and mutators for events.
 */
void
gs_set_event_state (sc_gamestateref_t gs, sc_int32 event, sc_int32 state)
{
	gs->events[event].state = state;
}

void
gs_set_event_time (sc_gamestateref_t gs, sc_int32 event, sc_int32 etime)
{
	gs->events[event].time = etime;
}

sc_int32
gs_get_event_state (sc_gamestateref_t gs, sc_int32 event)
{
	return gs->events[event].state;
}

sc_int32
gs_get_event_time (sc_gamestateref_t gs, sc_int32 event)
{
	return gs->events[event].time;
}

void
gs_dec_event_time (sc_gamestateref_t gs, sc_int32 event)
{
	gs->events[event].time--;
}


/*
 * Gamestate accessors and mutators for rooms.
 */
void
gs_set_room_seen (sc_gamestateref_t gs, sc_int32 room, sc_bool seen)
{
	gs->rooms[room].visited = seen;
}

sc_bool
gs_get_room_seen (sc_gamestateref_t gs, sc_int32 room)
{
	return gs->rooms[room].visited;
}


/*
 * Gamestate accessors and mutators for tasks.
 */
void
gs_set_task_done (sc_gamestateref_t gs, sc_int32 task, sc_bool done)
{
	gs->tasks[task].done = done;
}

void
gs_set_task_scored (sc_gamestateref_t gs, sc_int32 task, sc_bool scored)
{
	gs->tasks[task].scored = scored;
}

sc_bool
gs_get_task_done (sc_gamestateref_t gs, sc_int32 task)
{
	return gs->tasks[task].done;
}

sc_bool
gs_get_task_scored (sc_gamestateref_t gs, sc_int32 task)
{
	return gs->tasks[task].scored;
}


/*
 * Gamestate accessors and mutators for objects.
 */
void
gs_set_object_openness (sc_gamestateref_t gs,
			sc_int32 object, sc_int32 openness)
{
	gs->objects[object].openness = openness;
}

void
gs_set_object_state (sc_gamestateref_t gs, sc_int32 object, sc_int32 state)
{
	gs->objects[object].state = state;
}

void
gs_set_object_seen (sc_gamestateref_t gs, sc_int32 object, sc_bool seen)
{
	gs->objects[object].seen = seen;
}

void
gs_set_object_unmoved (sc_gamestateref_t gs,
			sc_int32 object, sc_bool unmoved)
{
	gs->objects[object].unmoved = unmoved;
}

sc_int32
gs_get_object_openness (sc_gamestateref_t gs, sc_int32 object)
{
	return gs->objects[object].openness;
}

sc_int32
gs_get_object_state (sc_gamestateref_t gs, sc_int32 object)
{
	return gs->objects[object].state;
}

sc_bool
gs_get_object_seen (sc_gamestateref_t gs, sc_int32 object)
{
	return gs->objects[object].seen;
}

sc_bool
gs_get_object_unmoved (sc_gamestateref_t gs, sc_int32 object)
{
	return gs->objects[object].unmoved;
}

sc_int32
gs_get_object_position (sc_gamestateref_t gs, sc_int32 object)
{
	return gs->objects[object].position;
}

sc_int32
gs_get_object_parent (sc_gamestateref_t gs, sc_int32 object)
{
	return gs->objects[object].parent;
}

void
gs_object_move_onto (sc_gamestateref_t gs, sc_int32 object, sc_int32 onto)
{
	gs->objects[object].position = OBJ_ON_OBJECT;
	gs->objects[object].parent = onto;
}

void
gs_object_move_into (sc_gamestateref_t gs, sc_int32 object, sc_int32 into)
{
	gs->objects[object].position = OBJ_IN_OBJECT;
	gs->objects[object].parent = into;
}

void
gs_object_make_hidden (sc_gamestateref_t gs, sc_int32 object)
{
	gs->objects[object].position = OBJ_HIDDEN;
	gs->objects[object].parent = -1;
}

void
gs_object_player_get (sc_gamestateref_t gs, sc_int32 object)
{
	gs->objects[object].position = OBJ_HELD_PLAYER;
	gs->objects[object].parent = -1;
}

void
gs_object_npc_get (sc_gamestateref_t gs, sc_int32 object, sc_int32 npc)
{
	gs->objects[object].position = OBJ_HELD_NPC;
	gs->objects[object].parent = npc;
}

void
gs_object_player_wear (sc_gamestateref_t gs, sc_int32 object)
{
	gs->objects[object].position = OBJ_WORN_PLAYER;
	gs->objects[object].parent = 0;
}

void
gs_object_npc_wear (sc_gamestateref_t gs, sc_int32 object, sc_int32 npc)
{
	gs->objects[object].position = OBJ_WORN_NPC;
	gs->objects[object].parent = npc;
}

void
gs_object_to_room (sc_gamestateref_t gs, sc_int32 object, sc_int32 room)
{
	gs->objects[object].position = room + 1;
	gs->objects[object].parent = -1;
}

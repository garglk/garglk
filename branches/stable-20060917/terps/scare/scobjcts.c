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
#include <ctype.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_char	NUL			= '\0';
enum {			OBJ_WONTCLOSE		= 0,
			OBJ_OPEN		= 5,
			OBJ_CLOSED		= 6,
			OBJ_LOCKED		= 7 };
enum {			ROOMLIST_NO_ROOMS	= 0,
			ROOMLIST_ONE_ROOM	= 1,
			ROOMLIST_SOME_ROOMS	= 2,
			ROOMLIST_ALL_ROOMS	= 3,
			ROOMLIST_NPC_PART	= 4 };

/* Trace flag, set before running. */
static sc_bool		obj_trace		= FALSE;


/*
 * obj_is_static()
 * obj_is_surface()
 * obj_is_container()
 *
 * Convenience functions to return TRUE for given object attributes.
 */
sc_bool
obj_is_static (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_bool			bstatic;

	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Static";
	bstatic = prop_get_boolean (bundle, "B<-sis", vt_key);
	return bstatic;
}

sc_bool
obj_is_container (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_bool			is_container;

	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Container";
	is_container = prop_get_boolean (bundle, "B<-sis", vt_key);
	return is_container;
}

sc_bool
obj_is_surface (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_bool			is_surface;

	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Surface";
	is_surface = prop_get_boolean (bundle, "B<-sis", vt_key);
	return is_surface;
}


/*
 * obj_container_object()
 *
 * Return the index of the n'th container object found.
 */
sc_int32
obj_container_object (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_int32		object, count;

	/* Progress through objects until n containers found. */
	count = n;
	for (object = 0;
		object < gamestate->object_count && count >= 0; object++)
	    {
		if (obj_is_container (gamestate, object))
			count--;
	    }
	return object - 1;
}


/*
 * obj_container_index()
 *
 * Return index such that obj_container_object(index) == objnum.
 */
sc_int32
obj_container_index (sc_gamestateref_t gamestate, sc_int32 objnum)
{
	sc_int32		object, count;

	/* Progress through objects up to objnum. */
	count = 0;
	for (object = 0; object < objnum; object++)
	    {
		if (obj_is_container (gamestate, object))
			count++;
	    }
	return count;
}


/*
 * obj_surface_object()
 *
 * Return the index of the n'th surface object found.
 */
sc_int32
obj_surface_object (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_int32		object, count;

	/* Progress through objects until n surfaces found. */
	count = n;
	for (object = 0;
		object < gamestate->object_count && count >= 0; object++)
	    {
		if (obj_is_surface (gamestate, object))
			count--;
	    }
	return object - 1;
}


/*
 * obj_surface_index()
 *
 * Return index such that obj_surface_object(index) == objnum.
 */
sc_int32
obj_surface_index (sc_gamestateref_t gamestate, sc_int32 objnum)
{
	sc_int32		object, count;

	/* Progress through objects up to objnum. */
	count = 0;
	for (object = 0; object < objnum; object++)
	    {
		if (obj_is_surface (gamestate, object))
			count++;
	    }
	return count;
}


/*
 * obj_stateful_object()
 *
 * Return the index of the n'th openable or statussed object found.
 */
sc_int32
obj_stateful_object (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, count;

	/* Progress through objects until n matches found. */
	count = n;
	for (object = 0;
		object < gamestate->object_count && count >= 0; object++)
	    {
		sc_vartype_t	vt_key[3];
		sc_bool		is_openable, is_statussed;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Openable";
		is_openable = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		vt_key[2].string = "CurrentState";
		is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		if (is_openable || is_statussed)
			count--;
	    }
	return object - 1;
}


/*
 * obj_stateful_index()
 *
 * Return index such that obj_stateful_object(index) == objnum.
 */
sc_int32
obj_stateful_index (sc_gamestateref_t gamestate, sc_int32 objnum)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, count;

	/* Progress through objects up to objnum. */
	count = 0;
	for (object = 0; object < objnum; object++)
	    {
		sc_vartype_t	vt_key[3];
		sc_bool		is_openable, is_statussed;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Openable";
		is_openable = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		vt_key[2].string = "CurrentState";
		is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
		if (is_openable || is_statussed)
			count++;
	    }
	return count;
}


/*
 * obj_state_name()
 *
 * Return the string name of the state of a given stateful object.  The
 * string is malloc'ed, and needs to be freed by the caller.  Returns NULL
 * if no valid state string found.
 */
sc_char *
obj_state_name (sc_gamestateref_t gamestate, sc_int32 objnum)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*states;
	sc_int32		state, count;
	sc_uint16		first, last;
	sc_char			*string;

	/* Get the list of state strings for the object. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = objnum;
	vt_key[2].string = "States";
	states = prop_get_string (bundle, "S<-sis", vt_key);

	/* Find the start of the element for the current state. */
	state = gamestate->objects[objnum].state;
	for (first = 0, count = state;
		first < strlen (states) && count > 1; first++)
	    {
		if (states[first] == '|')
			count--;
	    }
	if (count != 1)
		return NULL;

	/* Find the end of the state string. */
	for (last = first; last < strlen (states); last++)
	    {
		if (states[last] == '|')
			break;
	    }

	/* Allocate and take a copy of the state string. */
	string = sc_malloc (last - first + 1);
	memcpy (string, states + first, last - first);
	string[last - first] = NUL;

	return string;
}


/*
 * obj_dynamic_object()
 *
 * Return the index of the n'th non-static object found.
 */
sc_int32
obj_dynamic_object (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_int32		object, count;

	/* Progress through objects until n matches found. */
	count = n;
	for (object = 0;
		object < gamestate->object_count && count >= 0; object++)
	    {
		if (!obj_is_static (gamestate, object))
			count--;
	    }
	return object - 1;
}


/*
 * Size is held in the ten's digit of SizeWeight, and weight in the
 * units.  Size and weight are multipliers -- the actual relative size
 * and weight rise rise by a factor of three for each incremental
 * multiplier.  These factors are also used for the maximum size of
 * object that can fit in a container, and the number of these that fit.
 */
static const sc_int32		OBJ_SIZE_WEIGHT_DIVISOR	= 10;
static const sc_int32		OBJ_DIMENSION_MULTIPLE	= 3;

/*
 * obj_get_size()
 * obj_get_weight()
 *
 * Return the relative size and weight of an object.  For containers, the
 * weight includes the weight of each contained object.
 *
 * TODO It's possible to have static objects in the player inventory, moved
 * by events -- how should these be handled, as they have no SizeWeight?
 */
sc_int32
obj_get_size (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		size, count;

	/* TODO For now, give static objects no size. */
	if (obj_is_static (gamestate, object))
		return 0;

	/* Size is the 'tens' component of SizeWeight. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "SizeWeight";
	count = prop_get_integer (bundle, "I<-sis", vt_key)
					/ OBJ_SIZE_WEIGHT_DIVISOR;

	/* Calculate and return size. */
	size = 1;
	for ( ; count > 0; count--)
		size *= OBJ_DIMENSION_MULTIPLE;

	if (obj_trace)
		sc_trace ("Object: object %ld is size %ld\n", object, size);
	return size;
}

sc_int32
obj_get_weight (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		weight, count;

	/* TODO For now, give static objects no weight. */
	if (obj_is_static (gamestate, object))
		return 0;

	/* Weight is the 'units' component of SizeWeight. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "SizeWeight";
	count = prop_get_integer (bundle, "I<-sis", vt_key)
					% OBJ_SIZE_WEIGHT_DIVISOR;

	/* Calculate base object weight. */
	weight = 1;
	for ( ; count > 0; count--)
		weight *= OBJ_DIMENSION_MULTIPLE;

	/* If a container or a surface, add weights of parented objects. */
	if (obj_is_container (gamestate, object)
			|| obj_is_surface (gamestate, object))
	    {
		sc_int32	cobject;

		/* Find and add contained or surface objects. */
		for (cobject = 0; cobject < gamestate->object_count; cobject++)
		    {
			if ((gs_get_object_position (gamestate, cobject)
							== OBJ_IN_OBJECT
				|| gs_get_object_position (gamestate, cobject)
							== OBJ_ON_OBJECT)
				&& gs_get_object_parent (gamestate, cobject)
							== object)
			    {
				weight += obj_get_weight (gamestate, cobject);
			    }
		    }
	    }

	if (obj_trace)
		sc_trace ("Object: object %ld is weight %ld\n", object, weight);

	/* Return total weight. */
	return weight;
}


/*
 * obj_get_container_maxsize()
 * obj_get_container_capacity()
 *
 * Return the maximum size of an object that can be placed in a container,
 * and the number that will fit.
 */
sc_int32
obj_get_container_maxsize (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		maxsize, count;

	/* Maxsize is found from the 'units' component of Capacity. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Capacity";
	count = prop_get_integer (bundle, "I<-sis", vt_key)
					% OBJ_SIZE_WEIGHT_DIVISOR;

	/* Calculate and return maximum size. */
	maxsize = 1;
	for ( ; count > 0; count--)
		maxsize *= OBJ_DIMENSION_MULTIPLE;

	if (obj_trace)
	    {
		sc_trace ("Object: object %ld has max"
					" size %ld\n", object, maxsize);
	    }

	return maxsize;
}

sc_int32
obj_get_container_capacity (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		capacity;

	/* The count of objects is in the 'tens' component of Capacity. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Capacity";
	capacity = prop_get_integer (bundle, "I<-sis", vt_key);

	if (obj_trace)
	    {
		sc_trace ("Object: object %ld has capacity %ld\n",
				object, capacity / OBJ_SIZE_WEIGHT_DIVISOR);
	    }

	return capacity / OBJ_SIZE_WEIGHT_DIVISOR;
}


/* Sit/lie bit mask enumerations. */
enum {			OBJ_STANDABLE_MASK	= 0x01,
			OBJ_LIEABLE_MASK	= 0x02 };

/*
 * obj_standable_object()
 *
 * Return the index of the n'th standable object found.
 */
sc_int32
obj_standable_object (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, count;

	/* Progress through objects until n standable found. */
	count = n;
	for (object = 0;
		object < gamestate->object_count && count >= 0; object++)
	    {
		sc_vartype_t	vt_key[3];
		sc_int32	sitlie;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "SitLie";
		sitlie = prop_get_integer (bundle, "I<-sis", vt_key);
		if (sitlie & OBJ_STANDABLE_MASK)
			count--;
	    }
	return object - 1;
}


/*
 * obj_lieable_object()
 *
 * Return the index of the n'th lieable object found.
 */
sc_int32
obj_lieable_object (sc_gamestateref_t gamestate, sc_int32 n)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, count;

	/* Progress through objects until n lieable found. */
	count = n;
	for (object = 0;
		object < gamestate->object_count && count >= 0; object++)
	    {
		sc_vartype_t	vt_key[3];
		sc_int32	sitlie;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "SitLie";
		sitlie = prop_get_integer (bundle, "I<-sis", vt_key);
		if (sitlie & OBJ_LIEABLE_MASK)
			count--;
	    }
	return object - 1;
}


/*
 * obj_directly_in_room_internal()
 * obj_directly_in_room()
 *
 * Return TRUE if a given object is currently on the floor of a given room.
 */
static sc_bool
obj_directly_in_room_internal (sc_gamestateref_t gamestate,
		sc_int32 object, sc_int32 room)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;

	/* See if the object is static or dynamic. */
	if (obj_is_static (gamestate, object))
	    {
		sc_vartype_t	vt_key[5];
		sc_int32	type;

		/* Static object moved to player or room by event? */
		if (gs_get_object_position (gamestate, object) > -1)
		    {
			if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER)
				return FALSE;
			else
				return gs_get_object_position
						(gamestate, object) - 1 == room;
		    }

		/* Check and return the room list for the object. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Where";
		vt_key[3].string = "Type";
		type = prop_get_integer (bundle, "I<-siss", vt_key);
		switch (type)
		    {
		    case ROOMLIST_ALL_ROOMS:			return TRUE;
		    case ROOMLIST_NO_ROOMS:
		    case ROOMLIST_NPC_PART:			return FALSE;

		    case ROOMLIST_ONE_ROOM:
			vt_key[3].string = "Room";
			return prop_get_integer (bundle, "I<-siss", vt_key)
								== room + 1;

		    case ROOMLIST_SOME_ROOMS:
			vt_key[3].string = "Rooms";
			vt_key[4].integer = room + 1;
			return prop_get_boolean (bundle, "B<-sissi", vt_key);

		    default:
			sc_fatal ("obj_directly_in_room_internal:"
						" invalid type, %ld\n", type);
			return FALSE;
		    }
	    }
	else
		return gs_get_object_position (gamestate, object) == room + 1;
}

sc_bool
obj_directly_in_room (sc_gamestateref_t gamestate,
		sc_int32 object, sc_int32 room)
{
	sc_bool		result;

	/* Check, trace result, and return. */
	result = obj_directly_in_room_internal (gamestate, object, room);
	if (obj_trace)
	    {
		sc_trace ("Object: checking for object %ld directly"
					" in room %ld, %s\n", object, room,
					result ? "TRUE" : "FALSE");
	    }
	return result;
}


/*
 * obj_indirectly_in_room_internal()
 * obj_indirectly_in_room()
 *
 * Return TRUE if a given object is currently in a given room, either
 * directly, on an object indirectly, in an open object indirectly, or
 * carried by an NPC in the room.
 */
static sc_bool
obj_indirectly_in_room_internal (sc_gamestateref_t gamestate,
		sc_int32 object, sc_int32 room)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;

	/* See if the object is static or dynamic. */
	if (obj_is_static (gamestate, object))
	    {
		sc_vartype_t	vt_key[5];
		sc_int32	type;

		/* Static object moved to player or room by event? */
		if (gs_get_object_position (gamestate, object) > -1)
		    {
			if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER)
				return gs_player_in_room (gamestate, room);
			else
				return gs_get_object_position
						(gamestate, object) - 1 == room;
		    }

		/* Check and return the room list for the object. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Where";
		vt_key[3].string = "Type";
		type = prop_get_integer (bundle, "I<-siss", vt_key);
		switch (type)
		    {
		    case ROOMLIST_ALL_ROOMS:			return TRUE;
		    case ROOMLIST_NO_ROOMS:			return FALSE;

		    case ROOMLIST_ONE_ROOM:
			vt_key[3].string = "Room";
			return prop_get_integer (bundle, "I<-siss", vt_key)
								== room + 1;

		    case ROOMLIST_SOME_ROOMS:
			vt_key[3].string = "Rooms";
			vt_key[4].integer = room + 1;
			return prop_get_boolean (bundle, "B<-sissi", vt_key);

		    case ROOMLIST_NPC_PART:
		        {
			sc_int32	npc;

			vt_key[2].string = "Parent";
			npc = prop_get_integer (bundle, "I<-sis", vt_key);
			if (npc == 0)
				return gs_player_in_room (gamestate, room);
			else
				return npc_in_room (gamestate, npc - 1, room);
		        }

		    default:
			sc_fatal ("obj_indirectly_in_room_internal:"
						" invalid type, %ld\n", type);
			return FALSE;
		    }
	    }
	else
	    {
		sc_int32	parent, position;

		/* Get dynamic object's parent and position. */
		parent = gs_get_object_parent (gamestate, object);
		position = gs_get_object_position (gamestate, object);

		/* Decide depending on positioning. */
		switch (position)
		    {
		    case OBJ_HIDDEN:		/* Hidden. */
			return FALSE;

		    case OBJ_HELD_PLAYER:	/* Held by player. */
		    case OBJ_WORN_PLAYER:	/* Worn by player. */
			return gs_player_in_room (gamestate, room);

		    case OBJ_HELD_NPC:		/* Held by NPC. */
		    case OBJ_WORN_NPC:		/* Worn by NPC. */
			return npc_in_room (gamestate, parent, room);

		    case OBJ_IN_OBJECT:		/* In another object. */
			{
			sc_int32	openness;

			openness = gs_get_object_openness (gamestate, parent);
			switch (openness)
			    {
			    case OBJ_WONTCLOSE: case OBJ_OPEN:
				return obj_indirectly_in_room
						(gamestate, parent, room);
			    default:
				return FALSE;
			    }
			}

		    case OBJ_ON_OBJECT:		/* On another object. */
			return obj_indirectly_in_room (gamestate, parent, room);

		    default:			/* Within a room. */
			if (position > gamestate->room_count + 1)
			    {
				sc_error ("sc_object_indirectly_in_room:"
				" position out of bounds, %ld\n", position);
			    }
			return position - 1 == room;
		    }
	    }

	sc_error ("obj_indirectly_in_room: invalid drop through\n");
	return FALSE;
}

sc_bool
obj_indirectly_in_room (sc_gamestateref_t gamestate,
		sc_int32 object, sc_int32 room)
{
	sc_bool		result;

	/* Check, trace result, and return. */
	result = obj_indirectly_in_room_internal (gamestate, object, room);
	if (obj_trace)
	    {
		sc_trace ("Object: checking for object %ld indirectly"
					" in room %ld, %s\n", object, room,
					result ? "TRUE" : "FALSE");
	    }
	return result;
}


/*
 * obj_indirectly_held_by_player_internal()
 * obj_indirectly_held_by_player()
 *
 * Return TRUE if a given object is currently held by the player, either
 * directly, on an object indirectly, or in an open object indirectly.
 */
static sc_bool
obj_indirectly_held_by_player_internal (sc_gamestateref_t gamestate,
		sc_int32 object)
{
	/* See if the object is static or dynamic. */
	if (obj_is_static (gamestate, object))
	    {
		/* Static objects are by definition not held by the player. */
		return FALSE;
	    }
	else
	    {
		sc_int32	parent, position;

		/* Get dynamic object's parent and position. */
		parent = gs_get_object_parent (gamestate, object);
		position = gs_get_object_position (gamestate, object);

		/* Decide depending on positioning. */
		switch (position)
		    {
		    case OBJ_HIDDEN:		/* Hidden. */
			return FALSE;

		    case OBJ_HELD_PLAYER:	/* Held by player. */
		    case OBJ_WORN_PLAYER:	/* Worn by player. */
			return TRUE;

		    case OBJ_HELD_NPC:		/* Held by NPC. */
		    case OBJ_WORN_NPC:		/* Worn by NPC. */
			return FALSE;

		    case OBJ_IN_OBJECT:		/* In another object. */
			{
			sc_int32	openness;

			openness = gs_get_object_openness (gamestate, parent);
			switch (openness)
			    {
			    case OBJ_WONTCLOSE: case OBJ_OPEN:
				return obj_indirectly_held_by_player
							(gamestate, parent);
			    default:
				return FALSE;
			    }
			}

		    case OBJ_ON_OBJECT:		/* On another object. */
			return obj_indirectly_held_by_player
							(gamestate, parent);

		    default:			/* Within a room. */
			return FALSE;
		    }
	    }

	sc_error ("obj_indirectly_held_by_player: invalid drop through\n");
	return FALSE;
}

sc_bool
obj_indirectly_held_by_player (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_bool		result;

	/* Check, trace result, and return. */
	result = obj_indirectly_held_by_player_internal (gamestate, object);
	if (obj_trace)
	    {
		sc_trace ("Object: checking for object %ld indirectly"
					" held by player, %s\n", object,
					result ? "TRUE" : "FALSE");
	    }
	return result;
}


/*
 * sc_obj_shows_initial_description()
 *
 * Return TRUE if this object should be listed as room content.
 */
sc_bool
obj_shows_initial_description (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		onlywhennotmoved;

	/* Get only when moved property. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "OnlyWhenNotMoved";
	onlywhennotmoved = prop_get_integer (bundle, "I<-sis", vt_key);

	/* Combine this with gamestate in mysterious ways. */
	switch (onlywhennotmoved)
	    {
	    case 0:
		return TRUE;

	    case 1:
		return gs_get_object_unmoved (gamestate, object);

	    case 2:
		{
		sc_int32	initialposition;

		if (gs_get_object_unmoved (gamestate, object))
			return TRUE;

		vt_key[2].string = "InitialPosition";
		initialposition = prop_get_integer
					(bundle, "I<-sis", vt_key) - 3;
		return gs_get_object_position (gamestate, object)
							== initialposition;
		}
	    }

	/* What you talkin' 'bout, Willis? */
	return FALSE;
}


/*
 * obj_turn_update()
 * obj_setup_initial()
 *
 * Set initial values for object states, and update after a turn.
 */
void
obj_turn_update (sc_gamestateref_t gamestate)
{
	sc_int32	index;

	/* Update object seen and unmoved flags to current state. */
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (obj_indirectly_in_room (gamestate, index,
						gamestate->playerroom))
			gs_set_object_seen (gamestate, index, TRUE);
		if (gs_get_object_position (gamestate, index)
							== OBJ_HELD_PLAYER)
			gs_set_object_unmoved (gamestate, index, FALSE);
	    }
}

void
obj_setup_initial (sc_gamestateref_t gamestate)
{
	/* Set initial seen states for objects. */
	obj_turn_update (gamestate);
}


/*
 * obj_debug_trace()
 *
 * Set object tracing on/off.
 */
void
obj_debug_trace (sc_bool flag)
{
	obj_trace = flag;
}

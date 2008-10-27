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
 * o Ensure module messages precisely match the real Runner ones.  This
 *   matters for ALRs.
 *
 * o Capacity checks on the player and on containers are implemented, but
 *   may not be right.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_char	NUL			= '\0',
			COMMA			= ',';

/* NPC gender enumerations. */
enum {			NPC_MALE		= 0,
			NPC_FEMALE		= 1,
			NPC_NEUTER		= 2 };

/* Openable object openness enumerations. */
enum {			OBJ_OPEN		= 5,
			OBJ_CLOSED		= 6,
			OBJ_LOCKED		= 7 };

/* Perspective enumerations. */
enum {			FIRST_PERSON		= 0,
			SECOND_PERSON		= 1,
			THIRD_PERSON		= 2 };

/* Enumeration of TAF file versions. */
enum {			TAF_VERSION_400		= 400,
			TAF_VERSION_390		= 390,
			TAF_VERSION_380		= 380 };

/* Trace flag, set before running. */
static sc_bool		lib_trace		= FALSE;


/*
 * lib_warn_battle_system()
 *
 * Display a warning when the battle system is detected in a game.
 */
void
lib_warn_battle_system (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter,
	"\n<FONT SIZE=16>SCARE WARNING</FONT>\n\n"
	"The game uses Adrift's Battle System, something not fully supported"
	" by this release of SCARE.\n\n");

	pf_buffer_string (filter,
	"SCARE will still run the game, but it will not create character"
	" battles where they would normally occur.  For some games, this may"
	" be perfectly okay, as the Battle System is sometimes turned on"
	" by accident in a game, but never actually used.  For others, though,"
	" the omission of this feature may be more serious.\n\n");

	pf_buffer_string (filter,
	"Please press a key to continue...\n\n<WAITKEY>");
}


/*
 * lib_random_roomgroup_member()
 *
 * Return a random member of a roomgroup.
 */
sc_int32
lib_random_roomgroup_member (sc_gamestateref_t gamestate,
		sc_int32 roomgroup)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4], vt_rvalue;
	sc_int32		count, room;

	/* Get the count of rooms in the group. */
	vt_key[0].string = "RoomGroups";
	vt_key[1].integer = roomgroup;
	vt_key[2].string = "List2";
	count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;
	if (count == 0)
	    {
		sc_fatal ("lib_random_roomgroup_member:"
				" no rooms in group %ld\n", roomgroup);
	    }

	/* Pick a room at random and return it. */
	vt_key[3].integer = sc_randomint (0, count - 1);
	room = prop_get_integer (bundle, "I<-sisi", vt_key);

	if (lib_trace)
	    {
		sc_trace ("Library: random room for"
				" group %ld is %ld\n", roomgroup, room);
	    }
	return room;
}


/*
 * lib_pass_alt_room()
 *
 * Return TRUE if a particular alternate room description should be used.
 */
static sc_bool
lib_pass_alt_room (sc_gamestateref_t gamestate, sc_int32 room, sc_int32 alt)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		type;
	sc_bool			pass;

	/* Get alternate type. */
	vt_key[0].string = "Rooms";
	vt_key[1].integer = room;
	vt_key[2].string = "Alts";
	vt_key[3].integer = alt;
	vt_key[4].string = "Type";
	type = prop_get_integer (bundle, "I<-sisis", vt_key);

	/* Select based on type. */
	pass = FALSE;
	switch (type)
	    {
	    case 0:	/* Task. */
		{
		sc_int32	var2, var3;

		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		if (var2 == 0)		/* No task. */
			pass = TRUE;
		else
		    {
			vt_key[4].string = "Var3";
			var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

			pass = gs_get_task_done
					(gamestate, var2 - 1) == !(var3 != 0);
		    }
		break;
		}

	    case 1:	/* Stateful object. */
		{
		sc_int32	var2, var3;

		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		if (var2 == 0)		/* No object. */
			pass = TRUE;
		else
		    {
			vt_key[4].string = "Var3";
			var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

			pass = restr_pass_task_object_state
					(gamestate,
					obj_stateful_index (gamestate, var2),
					var3 - 1);
		    }
		break;
		}

	    case 2:	/* Player condition. */
		{
		sc_int32	var2, var3, object;

		vt_key[4].string = "Var2";
		var2 = prop_get_integer (bundle, "I<-sisis", vt_key);
		vt_key[4].string = "Var3";
		var3 = prop_get_integer (bundle, "I<-sisis", vt_key);

		if (var3 == 0)
		    {
			switch (var2)
			    {
			    case 0: case 2: case 5: pass = TRUE;	break;
			    case 1: case 3: case 4: pass = FALSE;	break;
			    default:
				sc_fatal ("lib_pass_alt_room: invalid "
					" player condition, %ld\n", var2);
			    }
			break;
		    }

		object = obj_dynamic_object (gamestate, var3 - 1);
		switch (var2)
		    {
		    case 0:	/* Isn't holding. */
			pass = gs_get_object_position (gamestate, object)
							!= OBJ_HELD_PLAYER;
			break;
		    case 1:	/* Is holding. */
			pass = gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER;
			break;
		    case 2:	/* Isn't wearing. */
			pass = gs_get_object_position (gamestate, object)
							!= OBJ_WORN_PLAYER;
			break;
		    case 3:	/* Is wearing. */
			pass = gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER;
			break;
		    case 4:	/* Isn't in the same room as. */
			pass = !obj_indirectly_in_room (gamestate, object,
						gamestate->playerroom);
			break;
		    case 5:	/* Is in the same room as. */
			pass = obj_indirectly_in_room (gamestate, object,
						gamestate->playerroom);
			break;
		    default:
			sc_fatal ("lib_pass_alt_room: invalid cond alt"
						" type, %ld\n", var2);
		    }
		break;
		}

	    default:
		sc_fatal ("lib_pass_alt_room: invalid type, %ld\n", type);
	    }

	return pass;
}


/*
 * lib_get_room_name()
 * lib_print_room_name()
 *
 * Get/print out the name for a given room.
 */
sc_char *
lib_get_room_name (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_int32		alt_count, alt, c_type;
	sc_char			*name;

	/* Start with a null name. */
	name = NULL;

	/* Get count of room alternates. */
	vt_key[0].string = "Rooms";
	vt_key[1].integer = room;
	vt_key[2].string = "Alts";
	alt_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;

	/* Try each type-0, then type-1, alternate. */
	for (c_type = 0; c_type <= 1; c_type++)
	    {
		for (alt = 0; alt < alt_count; alt++)
		    {
			sc_int32	type;

			vt_key[3].integer = alt;
			vt_key[4].string = "Type";
			type = prop_get_integer (bundle, "I<-sisis", vt_key);
			if (type == c_type
				&& lib_pass_alt_room (gamestate, room, alt))
			    {
				sc_char		*changed;

				vt_key[4].string = "Changed";
				changed = prop_get_string (bundle,
							"S<-sisis", vt_key);
				if (!sc_strempty (changed))
				    {
					name = changed;
					break;
				    }
			    }
			else
			    {
				vt_key[2].string = "Short";
				name = prop_get_string (bundle,
							"S<-sis", vt_key);
				break;
			    }
		    }
		if (name != NULL)
			break;
	    }

	/* If no name yet, try type-2 alternates. */
	if (name == NULL)
	    {
		for (alt = 0; alt < alt_count; alt++)
		    {
			sc_int32	type;

			vt_key[3].integer = alt;
			vt_key[4].string = "Type";
			type = prop_get_integer (bundle, "I<-sisis", vt_key);
			if (type == 2
				&& lib_pass_alt_room (gamestate, room, alt))
			    {
				sc_char		*changed;

				vt_key[4].string = "Changed";
				changed = prop_get_string (bundle,
							"S<-sisis", vt_key);
				if (!sc_strempty (changed))
				    {
					name = changed;
					break;
				    }
			    }
		    }
	    }

	/* If still no name, fall through to default short. */
	if (name == NULL)
	    {
		vt_key[2].string = "Short";
		name = prop_get_string (bundle, "S<-sis", vt_key);
	    }

	/* Return the name. */
	return name;
}

void
lib_print_room_name (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_char		*name;

	/* Get the room name. */
	name = lib_get_room_name (gamestate, room);

	/* Print the room name, possibly in bold). */
	if (gamestate->bold_room_names)
	    {
		pf_buffer_tag (filter, SC_TAG_BOLD);
		pf_buffer_string (filter, name);
		pf_buffer_tag (filter, SC_TAG_ENDBOLD);
	    }
	else
		pf_buffer_string (filter, name);
	pf_buffer_character (filter, '\n');
}


/*
 * lib_print_object_np
 * lib_print_object
 *
 * Convenience functions to print out an object's name, with a "normalized"
 * prefix -- any "a"/"an"/"some" is replaced by "the" -- and with the full
 * prefix.
 */
static void
lib_print_object_np (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*prefix, *normalized, *name;

	/* Get the object's prefix. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);

	/*
	 * Normalize by skipping any leading "a"/"an"/"some", replacing it
	 * instead with "the", and skipping any odd "the" already present.
	 * If no prefix at all, add a "the " anyway.
	 *
	 * TODO This is empirical, based on observed Adrift Runner behavior,
	 * and what it's _really_ supposed to do is a mystery.  This routine
	 * has been a real PITA.
	 */
	normalized = prefix;
	if (!sc_strncasecmp (prefix, "a", 1)
			&& (prefix[1] == NUL || isspace (prefix[1])))
	    {
		normalized = prefix + strlen ("a");
		pf_buffer_string (filter, "the");
	    }
	else if (!sc_strncasecmp (prefix, "an", 2)
			&& (prefix[2] == NUL || isspace (prefix[2])))
	    {
		normalized = prefix + strlen ("an");
		pf_buffer_string (filter, "the");
	    }
	else if (!sc_strncasecmp (prefix, "the", 3)
			&& (prefix[3] == NUL || isspace (prefix[3])))
	    {
		normalized = prefix + strlen ("the");
		pf_buffer_string (filter, "the");
	    }
	else if (!sc_strncasecmp (prefix, "some", 4)
			&& (prefix[4] == NUL || isspace (prefix[4])))
	    {
		normalized = prefix + strlen ("some");
		pf_buffer_string (filter, "the");
	    }
	else if (sc_strempty (prefix))
		pf_buffer_string (filter, "the ");

	/*
	 * If the remaining normalized prefix isn't empty, print it, and
	 * a space.  If it is, then consider adding a space to any "the"
	 * printed above, except for the one done for empty prefixes,
	 * that is.
	 */
	if (!sc_strempty (normalized))
	    {
		pf_buffer_string (filter, normalized);
		pf_buffer_character (filter, ' ');
	    }
	else if (normalized > prefix)
		pf_buffer_character (filter, ' ');

	/*
	 * Print the object's name; here we also look for a leading article
	 * and strip if found -- some games may avoid prefix and do this
	 * instead.
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
	pf_buffer_string (filter, name);
}

static void
lib_print_object (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*prefix, *name;

	/*
	 * Get the object's prefix, and print if not empty, otherwise
	 * default to an "a " prefix, as that's what Adrift seems to do.
	 */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strempty (prefix))
	    {
		pf_buffer_string (filter, prefix);
		pf_buffer_character (filter, ' ');
	    }
	else
		pf_buffer_string (filter, "a ");

	/* Print object name. */
	vt_key[2].string = "Short";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	pf_buffer_string (filter, name);
}


/*
 * lib_print_npc_np
 * lib_print_npc
 *
 * Convenience functions to print out an NPC's name, with and without
 * any prefix.
 */
static void
lib_print_npc_np (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*name;

	/* Get the NPC's short description, and print it. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);

	pf_buffer_string (filter, name);
}

#if 0
static void
lib_print_npc (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*prefix;

	/* Get the NPC's prefix. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);

	/* If the prefix isn't empty, print it, then print NPC name. */
	if (!sc_strempty (prefix))
	    {
		pf_buffer_string (filter, prefix);
		pf_buffer_character (filter, ' ');
	    }
	lib_print_npc_np (gamestate, npc);
}
#endif


/*
 * lib_get_npc_inroom_text()
 *
 * Returns the inroom description to be use for an NPC; if the NPC has
 * gone walkabout and offers a changed description, return that; otherwise
 * return the standard inroom text.
 */
static sc_char *
lib_get_npc_inroom_text (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_int32		walk_count, walk;
	sc_char			*inroomtext;

	/* Get the count of NPC walks. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Walks";
	walk_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;

	/* Check for any active walk with a description, return if found. */
	for (walk = walk_count - 1; walk >= 0; walk--)
	    {
		if (gamestate->npcs[npc].walksteps[walk] > 0)
		    {
			sc_char		*changeddesc;

			/* Get and check any walk active description. */
			vt_key[3].integer = walk;
			vt_key[4].string = "ChangedDesc";
			changeddesc = prop_get_string
						(bundle, "S<-sisis", vt_key);
			if (!sc_strempty (changeddesc))
				return changeddesc;
		    }
	    }

	/* Return the standard inroom text. */
	vt_key[2].string = "InRoomText";
	inroomtext = prop_get_string (bundle, "S<-sis", vt_key);
	return inroomtext;
}


/*
 * lib_print_room_contents()
 *
 * Print a list of the contents of a room.
 */
static void
lib_print_room_contents (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_int32		object, npc;
	sc_int32		count, trail;

	/* List all objects that show their initial description. */
	count = 0;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (obj_directly_in_room (gamestate, object, room)
				&& obj_shows_initial_description
							(gamestate, object))
		    {
			sc_char		*inroomdesc;

			/* Find and print in room description. */
			vt_key[0].string = "Objects";
			vt_key[1].integer = object;
			vt_key[2].string = "InRoomDesc";
			inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);
			if (!sc_strempty (inroomdesc))
			    {
				if (count == 0)
					pf_buffer_character (filter, '\n');
				else
					pf_buffer_string (filter, "  ");
				pf_buffer_string (filter, inroomdesc);
				count++;
			    }
		    }
	    }
	if (count > 0)
		pf_buffer_character (filter, '\n');

	/*
	 * List dynamic objects directly located in the room, and not already
	 * listed above since they lack, or suppress, an in room description.
	 *
	 * If an object sets ListFlag, then if dynamic it's suppressed from
	 * the list where it would normally be included, but if static it's
	 * included where it would normally be excluded.
	 */
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (obj_directly_in_room (gamestate, object, room))
		    {
			sc_char		*inroomdesc;

			vt_key[0].string = "Objects";
			vt_key[1].integer = object;
			vt_key[2].string = "InRoomDesc";
			inroomdesc = prop_get_string (bundle, "S<-sis", vt_key);

			if (!obj_shows_initial_description (gamestate, object)
				|| sc_strempty (inroomdesc))
			    {
				sc_bool		listflag;

				vt_key[2].string = "ListFlag";
				listflag = prop_get_boolean
						(bundle, "B<-sis", vt_key);

				if (listflag
					== obj_is_static (gamestate, object))
				    {
					if (count > 0)
					    {
						if (count == 1)
							pf_buffer_string
								(filter,
							"\nAlso here is ");
						else
							pf_buffer_string
								(filter, ", ");
						lib_print_object
							(gamestate, trail);
					    }
					trail = object;
					count++;
				    }
			    }
		    }
	    }
	if (count >= 1)
	    {
		if (count == 1)
			pf_buffer_string (filter, "\nAlso here is ");
		else
			pf_buffer_string (filter, " and ");
		lib_print_object (gamestate, trail);
		pf_buffer_string (filter, ".\n");
	    }

	/* List NPCs directly in the room that have an in room description. */
	count = 0;
	for (npc = 0; npc < gamestate->npc_count; npc++)
	    {
		if (npc_in_room (gamestate, npc, room))
		    {
			sc_char		*descr;

			vt_key[0].string = "NPCs";
			vt_key[1].integer = npc;

			/* Print any non='#' in-room description. */
			descr = lib_get_npc_inroom_text (gamestate, npc);
			if (!sc_strempty (descr)
					&& sc_strcasecmp (descr, "#"))
			    {
				if (count == 0)
					pf_buffer_character (filter, '\n');
				else
					pf_buffer_string (filter, "  ");
				pf_buffer_string (filter, descr);
				count++;
			    }
		    }
	    }
	if (count > 0)
		pf_buffer_character (filter, '\n');

	/*
	 * List NPCs in the room that don't have an in room description and
	 * that request a default "...is here" with "#".  TODO Is this right?
	 */
	count = 0; trail = -1;
	for (npc = 0; npc < gamestate->npc_count; npc++)
	    {
		if (npc_in_room (gamestate, npc, room))
		    {
			sc_char		*descr;

			vt_key[0].string = "NPCs";
			vt_key[1].integer = npc;

			/* Print name for descriptions marked '#'. */
			descr = lib_get_npc_inroom_text (gamestate, npc);
			if (!sc_strempty (descr)
					&& !sc_strcasecmp (descr, "#"))
			    {
				if (count > 0)
				    {
					if (count > 1)
						pf_buffer_string (filter, ", ");
					else
					    {
						pf_buffer_character
								(filter, '\n');
						pf_new_sentence (filter);
					    }
					lib_print_npc_np (gamestate, trail);
				    }
				trail = npc;
				count++;
			    }
		    }
	    }
	if (count >= 1)
	    {
		if (count == 1)
		    {
			pf_buffer_character (filter, '\n');
			pf_new_sentence (filter);
			lib_print_npc_np (gamestate, trail);
			pf_buffer_string (filter, " is here");
		    }
		else
		    {
			pf_buffer_string (filter, " and ");
			lib_print_npc_np (gamestate, trail);
			pf_buffer_string (filter, " are here");
		    }
		pf_buffer_string (filter, ".\n");
	    }
}


/*
 * lib_print_room_description()
 *
 * Print out the long description for a given room.
 */
void
lib_print_room_description (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_bool			showobjects, keepprinting;
	sc_int32		alt_count, alt, c_type;
	sc_bool			described;
	sc_int32		event;
	sc_char			*descr;

	/* Begin by defaulting to printing everything. */
	showobjects = TRUE;
	keepprinting = TRUE;
	described = FALSE;

	/* Get count of room alternates. */
	vt_key[0].string = "Rooms";
	vt_key[1].integer = room;
	vt_key[2].string = "Alts";
	alt_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;

	/* Check alts with DisplayRoom type-0. */
	for (alt = 0; alt < alt_count && keepprinting; alt++)
	    {
		sc_int32	displayroom;

		vt_key[3].integer = alt;
		vt_key[4].string = "DisplayRoom";
		displayroom = prop_get_integer (bundle, "I<-sisis", vt_key);
		if (displayroom == 0)
		    {
			if (lib_pass_alt_room (gamestate, room, alt))
			    {
				sc_char		*m1;
				sc_int32	hideobjects;

				vt_key[4].string = "M1";
				m1 = prop_get_string (bundle,
							"S<-sisis", vt_key);
				if (!sc_strempty (m1))
				    {
					if (described)
						pf_buffer_string (filter, "  ");
					pf_buffer_string (filter, m1);
					described = TRUE;
				    }
				keepprinting = FALSE;

				vt_key[4].string = "Res1";
				res_handle_resource (gamestate,
							"sisis", vt_key);

				vt_key[4].string = "HideObjects";
				hideobjects = prop_get_integer (bundle,
							"I<-sisis", vt_key);
				if (hideobjects == 1)
					showobjects = FALSE;
			    }
			else
			    {
				sc_char		*m2;

				vt_key[4].string = "M2";
				m2 = prop_get_string (bundle,
							"S<-sisis", vt_key);
				if (!sc_strempty (m2))
				    {
					if (described)
						pf_buffer_string (filter, "  ");
					pf_buffer_string (filter, m2);
					described = TRUE;
				    }

				vt_key[4].string = "Res2";
				res_handle_resource (gamestate,
							"sisis", vt_key);

			    }
		    }
	    }

	/* Print the standard long room description. */
	if (keepprinting)
	    {
		vt_key[2].string = "Long";
		descr = prop_get_string (bundle, "S<-sis", vt_key);
		if (!sc_strempty (descr))
		    {
			if (described)
				pf_buffer_string (filter, "  ");
			pf_buffer_string (filter, descr);
			described = TRUE;
		    }

		vt_key[2].string = "Res";
		res_handle_resource (gamestate, "sis", vt_key);
	    }

	/* Back to handling alts. */
	vt_key[2].string = "Alts";

	/* Check alts with DisplayRoom type-1 and type 2. */
	for (c_type = 1; c_type <= 2; c_type++)
	    {
		for (alt = 0; alt < alt_count && keepprinting; alt++)
		    {
			sc_int32	displayroom;

			vt_key[3].integer = alt;
			vt_key[4].string = "DisplayRoom";
			displayroom = prop_get_integer
						(bundle, "I<-sisis", vt_key);
			if (displayroom == c_type)
			    {
				if (lib_pass_alt_room
						(gamestate, room, alt))
				    {
					sc_char		*m1;
					sc_int32	hideobjects;

					vt_key[4].string = "M1";
					m1 = prop_get_string (bundle,
							"S<-sisis", vt_key);
					if (!sc_strempty (m1))
					    {
						if (described)
							pf_buffer_string
								(filter, "  ");
						pf_buffer_string (filter, m1);
						described = TRUE;
					    }
					if (displayroom == 1)
						keepprinting = FALSE;

					vt_key[4].string = "Res1";
					res_handle_resource (gamestate,
							"sisis", vt_key);

					vt_key[4].string = "HideObjects";
					hideobjects = prop_get_integer
							(bundle,
							"I<-sisis", vt_key);
					if (hideobjects == 1)
						showobjects = FALSE;
				    }
				else
				    {
					sc_char		*m2;

					vt_key[4].string = "M2";
					m2 = prop_get_string (bundle,
							"S<-sisis", vt_key);
					if (!sc_strempty (m2))
					    {
						if (described)
							pf_buffer_string
								(filter, "  ");
						pf_buffer_string (filter, m2);
						described = TRUE;
					    }

					vt_key[4].string = "Res2";
					res_handle_resource (gamestate,
							"sisis", vt_key);

				    }
			    }
		    }
		if (!keepprinting)
			break;
	    }

	/* Print out any relevant event look text. */
	for (event = 0; event < gamestate->event_count; event++)
	    {
		if (gs_get_event_state (gamestate, event) == ES_RUNNING
				&& evt_can_see_event (gamestate, event))
		    {
			sc_char		*looktext;

			vt_key[0].string = "Events";
			vt_key[1].integer = event;
			vt_key[2].string = "LookText";
			looktext = prop_get_string (bundle, "S<-sis", vt_key);
			if (described)
				pf_buffer_string (filter, "  ");
			pf_buffer_string (filter, looktext);
			described = TRUE;

			vt_key[2].string = "Res";
			vt_key[3].integer = 1;
			res_handle_resource (gamestate, "sisi", vt_key);

		    }
	    }
	if (described)
		pf_buffer_character (filter, '\n');

	/* Finally, print room contents. */
	if (showobjects)
		lib_print_room_contents (gamestate, room);
}


/*
 * lib_can_go()
 *
 * Return TRUE if the player can move in the given direction.
 */
static sc_bool
lib_can_go (sc_gamestateref_t gamestate, sc_int32 room, sc_int32 direction)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		restriction;
	sc_bool			is_restricted = FALSE;

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
		sc_int32	type;

		if (lib_trace)
			sc_trace ("Library: hit move restriction\n");

		/* Get restriction type. */
		vt_key[4].string = "Var3";
		type = prop_get_integer (bundle, "I<-sisis", vt_key);
		switch (type)
		    {
		    case 0:	/* Task type restriction */
			{
			sc_int32	check;

			/* Get the expected completion state. */
			vt_key[4].string = "Var2";
			check = prop_get_integer (bundle, "I<-sisis", vt_key);

			if (lib_trace)
			    {
				sc_trace ("Library:   task %ld, check %ld\n",
							restriction, check);
			    }

			/* Restrict if task isn't done/not done as expected. */
			if ((check != 0) == gs_get_task_done
						(gamestate, restriction))
				is_restricted = TRUE;
			break;
			}

		    case 1:	/* Object state restriction */
			{
			sc_int32	object, check;
			sc_int32	openable, lockable;

			/* Get the target object. */
			object = obj_stateful_object (gamestate, restriction);

			/* Get the expected object state. */
			vt_key[4].string = "Var2";
			check = prop_get_integer (bundle, "I<-sisis", vt_key);

			if (lib_trace)
			    {
				sc_trace ("Library:   object %ld, check %ld\n",
							object, check);
			    }

			/* Check openable and lockable objects. */
			vt_key[0].string = "Objects";
			vt_key[1].integer = object;
			vt_key[2].string = "Openable";
			openable = prop_get_integer (bundle, "I<-sis", vt_key);
			if (openable > 0)
			    {
				/* See if lockable. */
				vt_key[2].string = "Key";
				lockable = prop_get_integer (bundle,
							"I<-sis", vt_key);
				if (lockable >= 0)
				    {
					/* Lockable. */
					if (check <= 2)
					    {
						if (gs_get_object_openness
							(gamestate, object)
								!= check + 5)
							is_restricted = TRUE;
					    }
					else
					    {
						if (gs_get_object_state
							(gamestate, object)
								!= check - 2)
							is_restricted = TRUE;
					    }
				    }
				else
				    {
					/* Not lockable, though openable. */
					if (check <= 1)
					    {
						if (gs_get_object_openness
							(gamestate, object)
								!= check + 5)
							is_restricted = TRUE;
					    }
					else
					    {
						if (gs_get_object_state
							(gamestate, object)
								!= check - 1)
							is_restricted = TRUE;
					    }
				    }
			   }
			else
			   {
				/* Not openable. */
				if (gs_get_object_state (gamestate, object)
								!= check + 1)
					is_restricted = TRUE;
			   }
			break;
			}
		    }
	    }

	/* Return TRUE if not restricted. */
	return !is_restricted;
}


/*
 * lib_select_response()
 *
 * Convenience function for multiple handlers.  Returns the appropriate
 * response string for a game, based on perspective.
 */
static sc_char *
lib_select_response (sc_gamestateref_t gamestate, sc_char *second_person,
		sc_char *first_person, sc_char *third_person)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_int32		perspective;
	sc_char			*response;

	/* Return the response appropriate for Perspective. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "Perspective";
	perspective = prop_get_integer (bundle, "I<-ss", vt_key);
	switch (perspective)
	    {
	    case FIRST_PERSON:	response = first_person;	break;
	    case SECOND_PERSON:	response = second_person;	break;
	    case THIRD_PERSON:	response = third_person;	break;
	    default:
		sc_error ("lib_select_response:"
				" unknown perspective, %ld\n", perspective);
		response = second_person;			break;
	    }

	return response;
}


/* List of direction names, for printing and counting exits. */
static sc_char	*DIRNAMES_4[] = {
	"north", "east", "south", "west", "up", "down", "in", "out",
	NULL
};
static sc_char	*DIRNAMES_8[] = {
	"north", "east", "south", "west", "up", "down", "in", "out",
	"northeast", "southeast", "southwest", "northwest",
	NULL
};


/*
 * lib_cmd_print_room_exits()
 *
 * Print a list of exits from the player room.
 */
sc_bool
lib_cmd_print_room_exits (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_char			**dirnames;
	sc_int32		count, index, trail;

	/* Decide on four or eight point compass names list. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "EightPointCompass";
	if (prop_get_boolean (bundle, "B<-ss", vt_key))
		dirnames = DIRNAMES_8;
	else
		dirnames = DIRNAMES_4;

	/* Poll for an exit for each valid direction name. */
	count = 0; trail = -1;
	for (index = 0; dirnames[index] != NULL; index++)
	    {
		sc_vartype_t	vt_rvalue;

		vt_key[0].string = "Rooms";
		vt_key[1].integer = gamestate->playerroom;
		vt_key[2].string = "Exits";
		vt_key[3].integer = index;
		if (prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key)
			&& lib_can_go (gamestate, gamestate->playerroom, index))
		    {
			if (count > 0)
			    {
				if (count == 1)
					pf_buffer_string (filter,
							"There are exits ");
				else
					pf_buffer_string (filter, ", ");
				pf_buffer_string (filter, dirnames[trail]);
			    }
			trail = index;
			count++;
		    }
	    }
	if (count >= 1)
	    {
		if (count == 1)
			pf_buffer_string (filter, "There is an exit ");
		else
			pf_buffer_string (filter, " and ");
		pf_buffer_string (filter, dirnames[trail]);
		pf_buffer_string (filter, ".\n");
	    }
	else
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't go in any direction!\n",
				"I can't go in any direction!\n",
				"%player% can't go in any direction!\n"));
	    }

	return TRUE;
}


/*
 * lib_describe_player_room()
 *
 * Print out details of the player room, in brief if verbose not set
 * and the room has already been visited.
 */
static void
lib_describe_player_room (sc_gamestateref_t gamestate,
		sc_bool force_verbose)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];

	/* Print the room name. */
	lib_print_room_name (gamestate, gamestate->playerroom);

	/* Print other room details if applicable. */
	if (force_verbose
			|| gamestate->verbose
			|| !gs_get_room_seen (gamestate, gamestate->playerroom))
	    {
		sc_bool		showexits;

		/* Print room description, and objects and NPCs. */
		lib_print_room_description (gamestate, gamestate->playerroom);

		/* Print exits if the ShowExits global requests it. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "ShowExits";
		showexits = prop_get_boolean (bundle, "B<-ss", vt_key);
		if (showexits)
		    {
			pf_buffer_character (filter, '\n');
			lib_cmd_print_room_exits (gamestate);
		    }
	    }
}


/*
 * lib_cmd_look()
 *
 * Command handler for "look" command.
 */
sc_bool
lib_cmd_look (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_character (filter, '\n');
	lib_describe_player_room (gamestate, TRUE);
	return TRUE;
}


/*
 * lib_cmd_quit()
 *
 * Called on "quit".  Exits from the game main loop.
 */
sc_bool
lib_cmd_quit (sc_gamestateref_t gamestate)
{
	/* If confirmed, set game running flag to false and return. */
	if (if_confirm (SC_CONF_QUIT))
		gamestate->is_running = FALSE;

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_restart()
 *
 * Called on "restart".  Exits from the game main loop with restart
 * request set.
 */
sc_bool
lib_cmd_restart (sc_gamestateref_t gamestate)
{
	/* If confirmed, set game running flag to false, and restart true. */
	if (if_confirm (SC_CONF_RESTART))
	    {
		gamestate->is_running = FALSE;
		gamestate->do_restart = TRUE;
	    }

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_undo()
 *
 * Called on "undo".  Restores any undo gamestate to the main gamestate.
 */
sc_bool
lib_cmd_undo (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* If an undo buffer is available, restore it. */
	if (gamestate->undo_available)
	    {
		gs_copy (gamestate, gamestate->undo);
		gamestate->undo_available = FALSE;

		lib_print_room_name (gamestate, gamestate->playerroom);
		pf_buffer_string (filter,
				"The previous turn has been undone.\n");

		/* Undo can't properly unravel layered sounds... */
		gamestate->stop_sound = TRUE;
	    }
	else
	    {
		if (gamestate->turns == 0)
			pf_buffer_string (filter,
				"You can't undo what hasn't been done.\n");
		else
			pf_buffer_string (filter,
				"Sorry, no more undo is available.\n");
	    }

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_hints()
 *
 * Called on "hints".  Requests the interface to display any available hints.
 */
sc_bool
lib_cmd_hints (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		task;
	sc_bool			game_has_hints;

	/*
	 * Check for the presence of any game hints at all, no matter
	 * whether the task is runnable or not.
	 */
	game_has_hints = FALSE;
	for (task = 0; task < gamestate->task_count; task++)
	    {
		if (task_has_hints (gamestate, task))
		    {
			game_has_hints = TRUE;
			break;
		    }
	    }

	/* If the game has hints, display any relevant ones. */
	if (game_has_hints)
	    {
		if (run_hint_iterate (gamestate, NULL) != NULL)
		    {
			if (if_confirm (SC_CONF_VIEW_HINTS))
				if_display_hints (gamestate);
		    }
		else
			pf_buffer_string (filter,
					"No hints currently available.\n");
	    }
	else
	    {
		pf_buffer_string (filter, "There are no hints available"
						" for this adventure.\n");
		pf_buffer_string (filter, "You're just going to have to work"
						" it out for yourself...\n");
	    }

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_print_string_bold()
 * lib_print_string_italics()
 *
 * Convenience helpers for printing licensing and game information.
 */
static void
lib_print_string_bold (const sc_char *string)
{
	if_print_tag (SC_TAG_BOLD, "");
	if_print_string (string);
	if_print_tag (SC_TAG_ENDBOLD, "");
}

static void
lib_print_string_italics (const sc_char *string)
{
	if_print_tag (SC_TAG_ITALICS, "");
	if_print_string (string);
	if_print_tag (SC_TAG_ENDITALICS, "");
}


/*
 * lib_cmd_help()
 * lib_cmd_license()
 *
 * A form of standard help output for games that don't define it themselves,
 * and the GPL licensing.  Print directly rather that using the printfilter
 * to avoid possible clashes with ALRs.
 */
sc_bool
lib_cmd_help (sc_gamestateref_t gamestate)
{
	if_print_string (
	"These are some of the typical commands used in this adventure:\n\n");

	if_print_string (
	"  [N]orth, [E]ast, [S]outh, [W]est, [U]p, [D]own, [In], [O]ut,"
	" [L]ook, [Exits]\n"
	"  E[x]amine <object>, [Get <object>],"
	" [Drop <object>], [...it], [...all]\n"
	"  [Where is <object>]\n"
	"  [Give <object> to  <character>], [Open...], [Close...],"
	" [Ask <character> about <subject>]\n"
	"  [Wear <object>], [Remove <object>], [I]nventory\n"
	"  [Put <object> into <object>],"
	" [Put <object> onto <object>]\n");

	if_print_string ("\nThe ");
	lib_print_string_italics ("License");
	if_print_string (
	" command displays SCARE's licensing terms and conditions.  The ");
	lib_print_string_italics ("Version");
	if_print_string (
	" command displays SCARE's version number, and that of the game.\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}
sc_bool
lib_cmd_license (sc_gamestateref_t gamestate)
{
	if_print_character ('\n');
	lib_print_string_bold ("SCARE");
	if_print_string (" is ");
	lib_print_string_italics (
	"Copyright (C) 2003-2005  Simon Baldwin and Mark J. Tilford");
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
	" Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307"
	" USA\n\n");

	if_print_string (
	"Please report any bugs, omissions, or misfeatures to ");
	lib_print_string_italics ("simon_baldwin@yahoo.com");
	if_print_string (".\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_information()
 *
 * Display a few small pieces of game information, done by a dialog GUI
 * in real Adrift.  Prints directly rather that using the printfilter to
 * avoid possible clashes with ALRs.
 */
sc_bool
lib_cmd_information (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[2];
	sc_char			*gamename, *gameauthor, *modified;
	sc_char			*filtered;

	vt_key[0].string = "Globals";
	vt_key[1].string = "GameName";
	gamename = prop_get_string (bundle, "S<-ss", vt_key);
	filtered = pf_filter_for_info (gamename, vars);
	pf_strip_tags (filtered);

	if_print_string ("\nThis is ");
	lib_print_string_bold (filtered);
	sc_free (filtered);

	vt_key[0].string = "Globals";
	vt_key[1].string = "GameAuthor";
	gameauthor = prop_get_string (bundle, "S<-ss", vt_key);
	filtered = pf_filter_for_info (gameauthor, vars);
	pf_strip_tags (filtered);

	if_print_string (", by ");
	lib_print_string_bold (filtered);
	sc_free (filtered);
	if_print_string (".\n");

	vt_key[0].string = "CompileDate";
	modified = prop_get_string (bundle, "S<-s", vt_key);
	if (!sc_strempty (modified))
	    {
		if_print_string ("The game was last modified on ");
		if_print_string (modified);
		if_print_string (".\n");
	    }

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_clear()
 *
 * Clear the main game window (almost).
 */
sc_bool
lib_cmd_clear (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_tag (filter, SC_TAG_CLS);
	pf_buffer_string (filter, "Screen cleared.\n");
	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_version()
 *
 * Display the "Runner version".  Prints directly rather that using the
 * printfilter to avoid possible clashes with ALRs.
 */
sc_bool
lib_cmd_version (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[1];
	sc_char			buffer[48], *version;

	if_print_string ("You are running the game with SCARE ");
	if_print_string (SCARE_VERSION SCARE_PATCH_LEVEL);
	if_print_string (" [Adrift ");
	sprintf (buffer, "%d.%02d.%02d",
				 SCARE_EMULATION / 1000,
				(SCARE_EMULATION % 1000) / 100,
				 SCARE_EMULATION % 100);
	if_print_string (buffer);
	if_print_string (" compatible].\n");

	vt_key[0].string = "VersionString";
	version = prop_get_string (bundle, "S<-s", vt_key);
	if_print_string ("The game was created with Adrift version ");
	if_print_string (version);
	if_print_string (".\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_wait()
 *
 * Set gamestate waitturns to a count of turns for which the main loop
 * will run without taking input.
 */
sc_bool
lib_cmd_wait (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];

	/* Get WaitTurns global, and set in gamestate. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "WaitTurns";
	gamestate->waitturns = prop_get_integer (bundle, "I<-ss", vt_key);

	pf_buffer_string (filter, "Time passes...\n");
	return TRUE;
}


/*
 * lib_cmd_verbose()
 * lib_cmd_brief()
 *
 * Set/clear gamestate verbose flag.
 */
sc_bool
lib_cmd_verbose (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Set game verbose flag and return. */
	gamestate->verbose = TRUE;
	pf_buffer_string (filter, "The game is now in its ");
	pf_buffer_tag (filter, SC_TAG_ITALICS);
	pf_buffer_string (filter, "verbose");
	pf_buffer_tag (filter, SC_TAG_ENDITALICS);
	pf_buffer_string (filter, " mode, which always gives long"
			" descriptions of locations (even if you've been"
			" there before).\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}

sc_bool
lib_cmd_brief (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Clear game verbose flag and return. */
	gamestate->verbose = FALSE;
	pf_buffer_string (filter, "The game is now in its ");
	pf_buffer_tag (filter, SC_TAG_ITALICS);
	pf_buffer_string (filter, "brief");
	pf_buffer_tag (filter, SC_TAG_ENDITALICS);
	pf_buffer_string (filter, " printing mode, which gives long"
			" descriptions of places never before visited"
			" and short descriptions otherwise.\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_notify_on()
 * lib_cmd_notify_off()
 * lib_cmd_notify()
 *
 * Set/clear/query gamestate score change notification flag.
 */
sc_bool
lib_cmd_notify_on (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Set score change notification and return. */
	gamestate->notify_score_change = TRUE;
	pf_buffer_string (filter, "Game score change notification is now ");
	pf_buffer_tag (filter, SC_TAG_ITALICS);
	pf_buffer_string (filter, "on");
	pf_buffer_tag (filter, SC_TAG_ENDITALICS);
	pf_buffer_string (filter, ", and the game will tell you of any"
			" changes in the score.\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}

sc_bool
lib_cmd_notify_off (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Clear score change notification and return. */
	gamestate->notify_score_change = FALSE;
	pf_buffer_string (filter, "Game score change notification is now ");
	pf_buffer_tag (filter, SC_TAG_ITALICS);
	pf_buffer_string (filter, "off");
	pf_buffer_tag (filter, SC_TAG_ENDITALICS);
	pf_buffer_string (filter, ", and the game will be silent on"
			" changes in the score.\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}

sc_bool
lib_cmd_notify (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Report the current state of notification. */
	pf_buffer_string (filter, "Game score change notification is ");
	pf_buffer_tag (filter, SC_TAG_ITALICS);
	pf_buffer_string (filter,
			gamestate->notify_score_change ? "on" : "off");
	pf_buffer_tag (filter, SC_TAG_ENDITALICS);
	pf_buffer_string (filter, ".\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_again()
 *
 * Called on "again".  Sets gamestate do_again flag.
 */
sc_bool
lib_cmd_again (sc_gamestateref_t gamestate)
{
	/* Set game do_again flag to true and return. */
	gamestate->do_again = TRUE;

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * Direction enumeration.  Used by movement commands, to multiplex them
 * all into a single function.  The values are explicit to ensure they
 * match enumerations in the game data.
 */
typedef enum {
	DIR_NORTH	= 0,		DIR_EAST	= 1,
	DIR_SOUTH	= 2,		DIR_WEST	= 3,
	DIR_UP		= 4,		DIR_DOWN	= 5,
	DIR_IN		= 6,		DIR_OUT		= 7,
	DIR_NORTHEAST	= 8,		DIR_SOUTHEAST	= 9,
	DIR_SOUTHWEST	= 10,		DIR_NORTHWEST	= 11
} sc_direction_t;


/*
 * lib_go()
 *
 * Central movement command, called by all movement handlers.
 */
static sc_bool
lib_go (sc_gamestateref_t gamestate, sc_direction_t direction)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_int32		destination;
	sc_char			**dirnames;
	sc_bool			is_trapped;
	sc_uint16		index;
	sc_bool			is_exitable[12];

	/* Decide on four or eight point compass names list. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "EightPointCompass";
	if (prop_get_boolean (bundle, "B<-ss", vt_key))
		dirnames = DIRNAMES_8;
	else
		dirnames = DIRNAMES_4;

	/* Start by seeing if there are any exits at all available. */
	is_trapped = TRUE;
	for (index = 0; dirnames[index] != NULL; index++)
	    {
		vt_key[0].string = "Rooms";
		vt_key[1].integer = gamestate->playerroom;
		vt_key[2].string = "Exits";
		vt_key[3].integer = index;
		if (prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key)
			&& lib_can_go (gamestate, gamestate->playerroom, index))
		    {
			is_exitable[index] = TRUE;
			is_trapped	   = FALSE;
		    }
		else
			is_exitable[index] = FALSE;
	    }
	if (is_trapped)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't go in any direction!\n",
				"I can't go in any direction!\n",
				"%player% can't go in any direction!\n"));
		return TRUE;
	    }

	/*
	 * Check for the exit, and if it doesn't exist, refuse, and list
	 * the possible options.
	 */
	vt_key[0].string = "Rooms";
	vt_key[1].integer = gamestate->playerroom;
	vt_key[2].string = "Exits";
	vt_key[3].integer = direction;
	vt_key[4].string = "Dest";
	if (!prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key))
	    {
		sc_int32	count, trail;

		pf_buffer_string (filter, lib_select_response (gamestate,
			"You can't go in that direction, but you can go ",
			"I can't go in that direction, but I can go ",
			"%player% can't go in that direction, but can go "));

		/* List available exits, found in exit test loop earlier. */
		count = 0; trail = -1;
		for (index = 0; dirnames[index] != NULL; index++)
		    {
			if (is_exitable[index])
			    {
				if (count > 0)
				    {
					if (count > 1)
						pf_buffer_string (filter, ", ");
					pf_buffer_string (filter,
							dirnames[trail]);
				    }
				trail = index;
				count++;
			    }
		    }
		if (count >= 1)
		    {
			if (count > 1)
				pf_buffer_string (filter, " and ");
			pf_buffer_string (filter, dirnames[trail]);
		    }
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Extract destination from the Dest property read above. */
	destination = vt_rvalue.integer - 1;

	/* Check for any movement restrictions. */
	if (!lib_can_go (gamestate, gamestate->playerroom, direction))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't go that way (at present).\n",
				"I can't go that way (at present).\n",
				"%player% can't go that way (at present).\n"));
		return TRUE;
	    }

	/* Move the player. */
	if (lib_trace)
	    {
		sc_trace ("Library: moving player from %ld to %ld\n",
					gamestate->playerroom, destination);
	    }

	/* Indicate if getting off something or standing up first. */
	if (gamestate->playerparent != -1)
	    {
		pf_buffer_string (filter, "(Getting off ");
		lib_print_object_np (gamestate, gamestate->playerparent);
		pf_buffer_string (filter, " first)\n");
	    }
	else if (gamestate->playerposition != 0)
		pf_buffer_string (filter, "(Standing up first)\n");

	/* Confirm and then make move. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You move ", "I move ", "%player% moves "));
	pf_buffer_string (filter, dirnames[direction]);
	pf_buffer_string (filter, ".\n");

	gs_move_player_to_room (gamestate, destination);

	/* Describe the new room and return. */
	lib_describe_player_room (gamestate, FALSE);
	return TRUE;
}


/*
 * lib_cmd_go_*()
 *
 * Direction-specific movement commands.
 */
sc_bool lib_cmd_go_north (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_NORTH); }
sc_bool lib_cmd_go_east (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_EAST); }
sc_bool lib_cmd_go_south (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_SOUTH); }
sc_bool lib_cmd_go_west (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_WEST); }
sc_bool lib_cmd_go_up (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_UP); }
sc_bool lib_cmd_go_down (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_DOWN); }
sc_bool lib_cmd_go_in (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_IN); }
sc_bool lib_cmd_go_out (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_OUT); }
sc_bool lib_cmd_go_northeast (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_NORTHEAST); }
sc_bool lib_cmd_go_southeast (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_SOUTHEAST); }
sc_bool lib_cmd_go_northwest (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_NORTHWEST); }
sc_bool lib_cmd_go_southwest (sc_gamestateref_t gamestate)
		{ return lib_go (gamestate, DIR_SOUTHWEST); }


/*
 * lib_cmd_examine_self()
 *
 * Show the long description of a player.
 */
sc_bool
lib_cmd_examine_self (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_int32		task, object, count, trail;
	sc_char			*description;
	sc_char			*position = NULL;

	/* Get selection task. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "Task";
	task = prop_get_integer (bundle, "I<-ss", vt_key) - 1;

	/* Select either the main or the alternate description. */
	if (task >= 0 && gs_get_task_done (gamestate, task))
		vt_key[1].string = "AltDesc";
	else
		vt_key[1].string = "PlayerDesc";

	/* Print the description, or default response. */
	description = prop_get_string (bundle, "S<-ss", vt_key);
	if (!sc_strempty (description))
		pf_buffer_string (filter, description);
	else
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You are as well as can be expected,"
					" considering the circumstances.",
				"I am as well as can be expected,"
					" considering the circumstances.",
				"%player% is as well as can be expected,"
					" considering the circumstances."));
	    }

	/* If not just standing on the floor, say more. */
	switch (gamestate->playerposition)
	    {
	    case 0:	position = lib_select_response (gamestate,
					"You are standing on ",
					"I am standing on ",
					"%player% is standing on ");	break;
	    case 1:	position = lib_select_response (gamestate,
					"You are sitting down on ",
					"I am sitting down on ",
					"%player% is sitting down on ");break;
	    case 2:	position = lib_select_response (gamestate,
					"You are lying down on ",
					"I am lying down on ",
					"%player% is lying down on ");	break;
	    }

	if (position != NULL
			&& !(gamestate->playerposition == 0
				&& gamestate->playerparent == -1))
	    {
		pf_buffer_string (filter, "  ");
		pf_buffer_string (filter, position);
		if (gamestate->playerparent != -1)
			lib_print_object_np
					(gamestate, gamestate->playerparent);
		else
			pf_buffer_string (filter, "the floor");
		pf_buffer_character (filter, '.');
	    }

	/* Find and list each object worn by the player. */
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					pf_buffer_string (filter,
						lib_select_response (gamestate,
						"  You are wearing ",
						"  I am wearing ",
						"  %player% is wearing "));
				    }
				else
					pf_buffer_string (filter, ", ");
				lib_print_object (gamestate, trail);
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
			pf_buffer_string (filter,
				lib_select_response (gamestate,
						"  You are wearing ",
						"  I am wearing ",
						"  %player% is wearing "));
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object (gamestate, trail);
		pf_buffer_character (filter, '.');
	    }

	pf_buffer_character (filter, '\n');
	return TRUE;
}


/*
 * lib_set_pronoun_npc()
 * lib_set_pronoun_object()
 *
 * Set the pronouns in the gamestate to the given NPC or object.
 */
static void
lib_set_pronoun_npc (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		version, gender;

	/*
	 * Version 3.80 games lack NPC gender information, so for this
	 * case set "him"/"her" each time called, and never set "it";
	 * this matches the version 3.80 runner.
	 */
	vt_key[0].string  = "Version";
	version = prop_get_integer (bundle, "I<-s", vt_key);
	if (version == TAF_VERSION_380)
	    {
		gamestate->him_npc = npc;
		gamestate->her_npc = npc;
		gamestate->it_npc  = -1;
		return;
	    }

	/* Set the pronoun appropriate to the NPC gender. */
	vt_key[0].string  = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string  = "Gender";
	gender = prop_get_integer (bundle, "I<-sis", vt_key);

	switch (gender)
	    {
	    case NPC_MALE:	gamestate->him_npc = npc;	break;
	    case NPC_FEMALE:	gamestate->her_npc = npc;	break;
	    case NPC_NEUTER:	gamestate->it_npc  = npc;	break;
	    default:
		sc_error ("lib_set_pronoun_npc:"
					"unknown gender, %ld\n", gender);
	    }
}

static void
lib_set_pronoun_object (sc_gamestateref_t gamestate, sc_int32 object)
{
	/* Nothing special to do here. */
	gamestate->it_object = object;
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
static sc_int32
lib_disambiguate_npc (sc_gamestateref_t gamestate, const sc_char *verb,
		sc_bool *is_ambiguous)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		count, index, npc;
	sc_int32		listed;

	/* Filter out all referenced NPCs not actually visible or seen. */
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		if (!(gamestate->npcs[index].seen
				&& npc_in_room (gamestate, index,
						gamestate->playerroom)))
			gamestate->npc_references[index] = FALSE;
	    }

	/*
	 * Count number of NPCs remaining as referenced by the last command,
	 * and note the last referenced NPC, for where count is 1.
	 */
	count = 0; npc = -1;
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		if (gamestate->npc_references[index])
		    {
			count++;
			npc = index;
		    }
	    }

	/*
	 * If the reference is unambiguous, note it for future use as "him"
	 * or "her", and return the referenced NPC.
	 */
	if (count == 1)
	    {
		/*
		 * Set this NPC as the referenced character, and also set
		 * the appropriate "him"/"her"/"it" reference.
		 */
		var_set_ref_character (vars, npc);
		lib_set_pronoun_npc (gamestate, npc);

		/* If this was a pronoun match, indicate it. */
		if (gamestate->is_npc_pronoun)
		    {
			pf_buffer_character (filter, '(');
			lib_print_npc_np (gamestate, npc);
			pf_buffer_string (filter, ")\n");
		    }

		/* Return, setting no ambiguity. */
		if (is_ambiguous != NULL)
			*is_ambiguous = FALSE;
		return npc;
	    }

	/* If nothing referenced, return no NPC. */
	if (count == 0)
	    {
		if (is_ambiguous != NULL)
			*is_ambiguous = FALSE;
		else
		    {
			pf_buffer_string (filter, "Please be more clear,"
						" who do you want to ");
			pf_buffer_string (filter, verb);
			pf_buffer_string (filter, "?\n");
		    }
		return -1;
	    }

	/* The NPC reference is ambiguous, so list the choices. */
	pf_buffer_string (filter, "Please be more clear,"
					" who do you want to ");
	pf_buffer_string (filter, verb);
	pf_buffer_string (filter, "?  ");

	listed = 0;
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		if (gamestate->npc_references[index])
		    {
			if (listed == 0)
				pf_new_sentence (filter);
			lib_print_npc_np (gamestate, index);
			listed++;
			if (listed < count - 1)
				pf_buffer_string (filter, ", ");
			else if (listed < count)
				pf_buffer_string (filter, ", or ");
		    }
	    }
	pf_buffer_string (filter, "?\n");

	/* Return no NPC for an ambiguous reference. */
	if (is_ambiguous != NULL)
		*is_ambiguous = TRUE;
	return -1;
}


/*
 * lib_disambiguate_object()
 *
 * Filter, then search the set of object matches.  If only one matched, note
 * and return it.  If multiple matched, print a disambiguation message and
 * the list, and return -1 with *is_ambiguous TRUE.  If none matched, return
 * -1 with *is_ambiguous FALSE if requested, otherwise print a message then
 * return -1.
 */
static sc_int32
lib_disambiguate_object (sc_gamestateref_t gamestate, const sc_char *verb,
		sc_bool *is_ambiguous)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		count, index, object;
	sc_int32		listed;

	/* Filter out all referenced objects not actually visible or seen. */
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (!(gs_get_object_seen (gamestate, index)
				&& obj_indirectly_in_room (gamestate,
						index, gamestate->playerroom)))
			gamestate->object_references[index] = FALSE;
	    }

	/*
	 * Count number of objects remaining as referenced by the last command,
	 * and note the last referenced object, for where count is 1.
	 */
	count = 0; object = -1;
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (gamestate->object_references[index])
		    {
			count++;
			object = index;
		    }
	    }

	/*
	 * If the reference is unambiguous, note it for future use as "it",
	 * set in variables, and return the referenced object.
	 */
	if (count == 1)
	    {
		/* Set this object as referenced and note as "it". */
		var_set_ref_object (vars, object);
		lib_set_pronoun_object (gamestate, object);

		/* If this was a pronoun match, indicate it. */
		if (gamestate->is_object_pronoun)
		    {
			pf_buffer_character (filter, '(');
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, ")\n");
		    }

		/* Return, setting no ambiguity. */
		if (is_ambiguous != NULL)
			*is_ambiguous = FALSE;
		return object;
	    }

	/* If nothing referenced, return no object. */
	if (count == 0)
	    {
		if (is_ambiguous != NULL)
			*is_ambiguous = FALSE;
		else
		    {
			pf_buffer_string (filter, "Please be more clear,"
						" what do you want to ");
			pf_buffer_string (filter, verb);
			pf_buffer_string (filter, "?\n");
		    }
		return -1;
	    }

	/* The object reference is ambiguous, so list the choices. */
	pf_buffer_string (filter, "Please be more clear,"
					" what do you want to ");
	pf_buffer_string (filter, verb);
	pf_buffer_string (filter, "?  ");

	listed = 0;
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (gamestate->object_references[index])
		    {
			if (listed == 0)
				pf_new_sentence (filter);
			lib_print_object_np (gamestate, index);
			listed++;
			if (listed < count - 1)
				pf_buffer_string (filter, ", ");
			else if (listed < count)
				pf_buffer_string (filter, " or ");
		    }
	    }
	pf_buffer_string (filter, "?\n");

	/* Return no object for an ambiguous reference. */
	if (is_ambiguous != NULL)
		*is_ambiguous = TRUE;
	return -1;
}


/*
 * lib_npc_inventory()
 *
 * List objects carried by an NPC.
 */
static sc_bool
lib_npc_inventory (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, count, trail, wear_count;

	/* Find and list each object worn by the NPC. */
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (gs_get_object_position (gamestate, object) == OBJ_WORN_NPC
			&& gs_get_object_parent (gamestate, object) == npc)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					pf_buffer_character (filter, '\n');
					pf_new_sentence (filter);
					lib_print_npc_np (gamestate, npc);
					pf_buffer_string (filter,
							" is wearing ");
				    }
				else
					pf_buffer_string (filter, ", ");
				lib_print_object (gamestate, trail);
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
			pf_buffer_character (filter, '\n');
			pf_new_sentence (filter);
			lib_print_npc_np (gamestate, npc);
			pf_buffer_string (filter, " is wearing ");
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object (gamestate, trail);
		pf_buffer_string (filter, ".\n");
	    }

	/* Find and list each object owned by the NPC. */
	wear_count = count;
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (gs_get_object_position (gamestate, object) == OBJ_HELD_NPC
			&& gs_get_object_parent (gamestate, object) == npc)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					if (wear_count == 0)
					    {
						pf_buffer_character (filter,
									'\n');
						pf_new_sentence (filter);
						lib_print_npc_np (gamestate,
									npc);
					    }
					else
						pf_buffer_string (filter,
								", and");
					pf_buffer_string (filter,
							" is carrying ");
				    }
				else
					pf_buffer_string (filter, ", ");
				lib_print_object (gamestate, trail);
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
			if (wear_count == 0)
			    {
				pf_buffer_character (filter, '\n');
				pf_new_sentence (filter);
				lib_print_npc_np (gamestate, npc);
			    }
			else
				pf_buffer_string (filter, ", and");
			pf_buffer_string (filter, " is carrying ");
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object (gamestate, trail);
		pf_buffer_string (filter, ".\n");
	    }

	/* Return TRUE if anything worn or carried. */
	return wear_count > 0 || count > 0;
}


/*
 * lib_cmd_examine_npc()
 *
 * Show the long description of the most recently referenced NPC, and a
 * list of what they're wearing and carrying.
 */
sc_bool
lib_cmd_examine_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_int32		npc, task, resource;
	sc_bool			described;
	sc_char			*description;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "examine", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Begin assuming no description printed. */
	described = FALSE;

	/* Get selection task. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Task";
	task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;

	/* Select either the main or the alternate description. */
	if (task >= 0 && gs_get_task_done (gamestate, task))
	    {
		vt_key[2].string = "AltText";
		resource = 1;
	    }
	else
	    {
		vt_key[2].string = "Descr";
		resource = 0;
	    }

	/* Print the description, if available. */
	description = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strempty (description))
	    {
		pf_buffer_string (filter, description);
		pf_buffer_character (filter, '\n');
		described |= TRUE;
	    }

	/* Handle any associated resource. */
	vt_key[2].string = "Res";
	vt_key[3].integer = resource;
	res_handle_resource (gamestate, "sisi", vt_key);

	/* Print what the NPC is wearing and carrying. */
	described |= lib_npc_inventory (gamestate, npc);

	/* If nothing yet said, print a default response. */
	if (!described)
	    {
		pf_buffer_string (filter, "There's nothing special about ");
		lib_print_npc_np (gamestate, npc);
		pf_buffer_string (filter, ".\n");
	    }

	return TRUE;
}


/*
 * lib_list_in_object_normal()
 *
 * List the objects in a given container object, normal format listing.
 */
static sc_bool
lib_list_in_object_normal (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool described)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		cobject, count, trail;

	/* List out the objects contained in this object. */
	count = 0; trail = -1;
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
					if (described)
						pf_buffer_string (filter, "  ");
					pf_buffer_string (filter, "Inside ");
					lib_print_object_np (gamestate, object);
					pf_buffer_string (filter, " is ");
				    }
				else
					pf_buffer_string (filter, ", ");

				/* Print out the current list object. */
				lib_print_object (gamestate, trail);
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
			if (described)
				pf_buffer_string (filter, "  ");
			pf_buffer_string (filter, "Inside ");
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, " is ");
		    }
		else
			pf_buffer_string (filter, " and ");

		/* Print out the final object. */
		lib_print_object (gamestate, trail);
		pf_buffer_character (filter, '.');
	    }

	/* Return TRUE if anything listed. */
	return count > 0;
}


/*
 * lib_list_in_object_alternate()
 *
 * List the objects in a given container object, alternate format listing.
 */
static sc_bool
lib_list_in_object_alternate (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool described)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		cobject, count, trail;

	/* List out the objects contained in this object. */
	count = 0; trail = -1;
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
					if (described)
						pf_buffer_string (filter, "  ");
					pf_new_sentence (filter);
				    }
				else
					pf_buffer_string (filter, ", ");

				/* Print out the current list object. */
				lib_print_object (gamestate, trail);
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
			if (described)
				pf_buffer_string (filter, "  ");
			pf_new_sentence (filter);
			lib_print_object (gamestate, trail);
			pf_buffer_string (filter, " is inside ");
		    }
		else
		    {
			pf_buffer_string (filter, " and ");
			lib_print_object (gamestate, trail);
			pf_buffer_string (filter, " are inside ");
		    }

		/* Print out the container. */
		lib_print_object_np (gamestate, object);
		pf_buffer_character (filter, '.');
	    }

	/* Return TRUE if anything listed. */
	return count > 0;
}


/*
 * lib_list_in_object()
 *
 * List the objects in a given container object.
 */
static sc_bool
lib_list_in_object (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool described)
{
	if (obj_is_static (gamestate, object))
		return lib_list_in_object_alternate
					(gamestate, object, described);
	else
		return lib_list_in_object_normal
					(gamestate, object, described);
}


/*
 * lib_list_on_object()
 *
 * List the objects on a given surface object.
 */
static sc_bool
lib_list_on_object (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool described)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		sobject, count, trail;

	/* List out the objects standing on this object. */
	count = 0; trail = -1;
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
					if (described)
						pf_buffer_string (filter, "  ");
					pf_new_sentence (filter);
				    }
				else
					pf_buffer_string (filter, ", ");

				/* Print out the current list object. */
				lib_print_object (gamestate, trail);
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
			if (described)
				pf_buffer_string (filter, "  ");
			pf_new_sentence (filter);
			lib_print_object (gamestate, trail);
			pf_buffer_string (filter, " is on ");
		    }
		else
		    {
			pf_buffer_string (filter, " and ");
			lib_print_object (gamestate, trail);
			pf_buffer_string (filter, " are on ");
		    }

		/* Print out the surface. */
		lib_print_object_np (gamestate, object);
		pf_buffer_character (filter, '.');
	    }

	/* Return TRUE if anything listed. */
	return count > 0;
}


/*
 * lib_list_object_state()
 *
 * Describe the state of a stateful object.
 */
static sc_bool
lib_list_object_state (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool described)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_bool			is_statussed;
	sc_char			*state;

	/* Get object statefulness. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "CurrentState";
	is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;

	/* Ensure this is a stateful object. */
	if (is_statussed)
	    {
		if (described)
			pf_buffer_string (filter, "  ");
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is ");

		/* Add object state string. */
		state = obj_state_name (gamestate, object);
		if (state != NULL)
		    {
			pf_buffer_string (filter, state);
			sc_free (state);
			pf_buffer_string (filter, ".");
		    }
		else
		    {
			sc_error ("lib_list_object_state: invalid"
							" object state\n");
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
sc_bool
lib_cmd_examine_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		object, task, openness;
	sc_bool			described;
	sc_bool			is_statussed, is_mentioned;
	sc_char			*description, *resource;
	sc_bool			is_ambiguous, should_be;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "examine", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Begin assuming no description printed. */
	described = FALSE;

	/*
	 * Get selection task and expected state; for the expected task
	 * state, FALSE indicates task completed, TRUE not completed.
	 */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Task";
	task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;
	vt_key[2].string = "TaskNotDone";
	should_be = !prop_get_boolean (bundle, "B<-sis", vt_key);

	/* Select either the main or the alternate description. */
	if (task >= 0
		&& gs_get_task_done (gamestate, task) == should_be)
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
	if (!sc_strempty (description))
	    {
		pf_buffer_string (filter, description);
		described |= TRUE;
	    }

	/* Handle any associated resource. */
	vt_key[2].string = resource;
	res_handle_resource (gamestate, "sis", vt_key);

	/* If the object is openable, print its openness state. */
	openness = gs_get_object_openness (gamestate, object);
	switch (openness)
	    {
	    case OBJ_OPEN:
		if (described)
			pf_buffer_string (filter, "  ");
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is open.");
		described |= TRUE;
		break;

	    case OBJ_CLOSED:
		if (described)
			pf_buffer_string (filter, "  ");
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is closed.");
		described |= TRUE;
		break;

	    case OBJ_LOCKED:
		if (described)
			pf_buffer_string (filter, "  ");
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is locked.");
		described |= TRUE;
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
		    {
			described |= lib_list_object_state (gamestate,
							object, described);
		    }
	    }

	/* For open container objects, list out what's in them. */
	if (obj_is_container (gamestate, object)
			&& openness <= OBJ_OPEN)
	    {
		described |= lib_list_in_object (gamestate, object, described);
	    }

	/* For surface objects, list out what's on them. */
	if (obj_is_surface (gamestate, object))
	    {
		described |= lib_list_on_object (gamestate, object, described);
	    }

	/* If nothing yet said, print a default response. */
	if (!described)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You see nothing special about ",
				"I see nothing special about ",
				"%player% sees nothing special about "));
		lib_print_object_np (gamestate, object);
		pf_buffer_character (filter, '.');
	    }

	pf_buffer_character (filter, '\n');
	return TRUE;
}


/*
 * lib_try_game_command()
 *
 * Try a game command with a standard verb.  Used by get and drop handlers
 * to retry game commands using standard "get " and "drop " commands.  This
 * makes "take/pick up/put down" work with a game's overridden get/drop.
 */
static sc_bool
lib_try_game_command (sc_gamestateref_t gamestate, sc_int32 object,
		const sc_char *verb)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			*prefix, *name, *command;
	sc_bool			status;

	/* Construct and try for game commands with a standard verb. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);
	vt_key[2].string = "Short";
	name = prop_get_string (bundle, "S<-sis", vt_key);

	command = sc_malloc (strlen (verb)
				+ strlen (name) + strlen (prefix) + 3);
	strcpy (command, verb);
	strcat (command, " ");
	strcat (command, prefix);
	strcat (command, " ");
	strcat (command, name);

	status = run_task_commands (gamestate, command, TRUE);
	sc_free (command);
	return status;
}


/* Object sizes and weight calculation magic numbers. */
static const sc_int32		OBJ_SIZE_WEIGHT_DIVISOR	= 10;
static const sc_int32		OBJ_DIMENSION_MULTIPLE	= 3;

/*
 * lib_object_too_heavy()
 *
 * Return TRUE if the given object is too heavy for the player to carry.
 */
static sc_bool
lib_object_too_heavy (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool *is_portable)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_int32		index, max_weight, count;
	sc_int32		player_max, weight, object_weight;

	/* Get the player's stored weight limit. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "MaxWt";
	max_weight = prop_get_integer (bundle, "I<-ss", vt_key);

	/* Turn into a maximum carriable weight. */
	player_max = max_weight / OBJ_SIZE_WEIGHT_DIVISOR;
	for (count = max_weight % OBJ_SIZE_WEIGHT_DIVISOR; count > 0; count--)
		player_max *= OBJ_DIMENSION_MULTIPLE;

	/* Sum weights for objects currently held or worn by player. */
	weight = 0;
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (gs_get_object_position (gamestate, index)
							== OBJ_HELD_PLAYER
				|| gs_get_object_position (gamestate, index)
							== OBJ_WORN_PLAYER)
			weight += obj_get_weight (gamestate, index);
	    }

	object_weight = obj_get_weight (gamestate, object);

	/* If requested, return object portability. */
	if (is_portable != NULL)
		*is_portable = !(object_weight > player_max);

	/* Return TRUE if the new object exceeds limit. */
	return (weight + object_weight > player_max);
}


/*
 * lib_object_too_large()
 *
 * Return TRUE if the given object is too large for the player to carry.
 */
static sc_bool
lib_object_too_large (sc_gamestateref_t gamestate, sc_int32 object,
		sc_bool *is_portable)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_int32		index, max_size, count;
	sc_int32		player_max, size, object_size;

	/* Get the player's stored size limit. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "MaxSize";
	max_size = prop_get_integer (bundle, "I<-ss", vt_key);

	/* Turn into a maximum carriable size. */
	player_max = max_size / OBJ_SIZE_WEIGHT_DIVISOR;
	for (count = max_size % OBJ_SIZE_WEIGHT_DIVISOR; count > 0; count--)
		player_max *= OBJ_DIMENSION_MULTIPLE;

	/* Sum sizes for objects currently held or worn by player. */
	size = 0;
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (gs_get_object_position (gamestate, index)
							== OBJ_HELD_PLAYER
				|| gs_get_object_position (gamestate, index)
							== OBJ_WORN_PLAYER)
			size += obj_get_size (gamestate, index);
	    }

	object_size = obj_get_size (gamestate, object);

	/* If requested, return object portability. */
	if (is_portable != NULL)
		*is_portable = !(object_size > player_max);

	/* Return TRUE if the new object exceeds limit. */
	return (size + object_size > player_max);
}


/*
 * lib_cmd_get_object()
 *
 * Attempt to take the referenced object.
 */
sc_bool
lib_cmd_get_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "get", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Check if we already have it, or are wearing it. */
	if (gs_get_object_position (gamestate, object) == OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You've already got ",
					"I've already got ",
					"%player% already has "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }
	if (gs_get_object_position (gamestate, object) == OBJ_WORN_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You're already wearing ",
					"I'm already wearing ",
					"%player% is already wearing "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Check if an NPC already has it. */
	if (gs_get_object_position (gamestate, object) == OBJ_HELD_NPC
		|| gs_get_object_position (gamestate, object) == OBJ_WORN_NPC)
	    {
		lib_print_npc_np (gamestate,
				gs_get_object_parent (gamestate, object));
		pf_buffer_string (filter, lib_select_response (gamestate,
					" refuses to give you ",
					" refuses to give me ",
					" refuses to give %player% "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Disallow getting static objects. */
	if (obj_is_static (gamestate, object))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't get that!\n",
					"I can't get that!\n",
					"%player% can't get that!\n"));
		return TRUE;
	    }

	/*
	 * If the object is contained in or on something we're already
	 * holding, capacity checks are meaningless.
	 */
	if (!obj_indirectly_held_by_player (gamestate, object))
	    {
		sc_bool		is_portable;

		if (lib_object_too_heavy (gamestate, object, &is_portable))
		    {
			pf_new_sentence (filter);
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter,
				lib_select_response (gamestate,
				" is too heavy for you to carry",
				" is too heavy for me to carry",
				" is too heavy for %player% to carry"));
			if (is_portable)
				pf_buffer_string (filter, " at the moment");
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }
		if (lib_object_too_large (gamestate, object, &is_portable))
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
					"Your hands are full",
					"My hands are full",
					"%player%'s hands are full"));
			if (is_portable)
				pf_buffer_string (filter, " at the moment");
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }
	    }

	/* Retry game commands for the object with a standard "get". */
	if (lib_try_game_command (gamestate, object, "get"))
		return TRUE;

	/*
	 * Note what we're doing, taking care to try to match the real Adrift
	 * responses here.
	 */
	if (gs_get_object_position (gamestate, object) == OBJ_IN_OBJECT
		|| gs_get_object_position (gamestate, object) == OBJ_ON_OBJECT)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You take ",
					"I take ",
					"%player% takes "));
		lib_print_object_np (gamestate, object);

		pf_buffer_string (filter, " from ");
		lib_print_object_np (gamestate,
				gs_get_object_parent (gamestate, object));
	    }
	else
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You pick up ",
					"I pick up ",
					"%player% picks up "));
		lib_print_object_np (gamestate, object);
	    }
	pf_buffer_string (filter, ".\n");

	/* Take possession of the object. */
	gs_object_player_get (gamestate, object);
	return TRUE;
}


/*
 * lib_cmd_get_object_from()
 *
 * Attempt to take one get-able object from a container/surface.  This
 * function isn't mandatory -- plain GET <object> works fine with contain-
 * ers and surfaces, but it's a standard in Adrift so here it is.
 */
sc_bool
lib_cmd_get_object_from (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		object, supporter;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "get", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/*
	 * Now try to get the container or surface from referenced text,
	 * and disambiguate as usual.
	 */
	if (!uip_match ("%object%", var_get_ref_text (vars), gamestate))
	    {
		pf_buffer_string (filter,
				"What do you want to get that from?\n");
		return TRUE;
	    }
	supporter = lib_disambiguate_object (gamestate, "get from", NULL);
	if (supporter == -1)
		return TRUE;

	/* Check if we already have it, or are wearing it. */
	if (gs_get_object_position (gamestate, object) == OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You've already got ",
					"I've already got ",
					"%player% already has "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }
	if (gs_get_object_position (gamestate, object) == OBJ_WORN_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You're already wearing ",
					"I'm already wearing ",
					"%player% is already wearing "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Disallow removal from non-container/non-surface objects. */
	if (!(obj_is_container (gamestate, supporter)
			|| obj_is_surface (gamestate, supporter)))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't take anything from ",
				"I can't take anything from ",
				"%player% can't take anything from "));
		lib_print_object_np (gamestate, supporter);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Check the object is in or on the container or surface. */
	if (!((gs_get_object_position (gamestate, object) == OBJ_IN_OBJECT
			|| gs_get_object_position (gamestate, object)
							== OBJ_ON_OBJECT)
			&& gs_get_object_parent (gamestate, object)
							== supporter))
	    {
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		if (obj_is_container (gamestate, supporter))
		    {
			pf_buffer_string (filter, " is not in ");
			if (obj_is_surface (gamestate, supporter))
				pf_buffer_string (filter, " or on ");
		    }
		else
			pf_buffer_string (filter, " is not on ");
		lib_print_object_np (gamestate, supporter);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Disallow getting static objects. */
	if (obj_is_static (gamestate, object))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't get that!\n",
					"I can't get that!\n",
					"%player% can't get that!\n"));
		return TRUE;
	    }

	/*
	 * If the object is contained in or on something we're already
	 * holding, capacity checks are meaningless.
	 */
	if (!obj_indirectly_held_by_player (gamestate, object))
	    {
		sc_bool		is_portable;

		if (lib_object_too_heavy (gamestate, object, &is_portable))
		    {
			pf_new_sentence (filter);
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter,
				lib_select_response (gamestate,
				" is too heavy for you to carry",
				" is too heavy for me to carry",
				" is too heavy for %player% to carry"));
			if (is_portable)
				pf_buffer_string (filter, " at the moment");
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }
		if (lib_object_too_large (gamestate, object, &is_portable))
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
					"Your hands are full",
					"My hands are full",
					"%player%'s hands are full"));
			if (is_portable)
				pf_buffer_string (filter, " at the moment");
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }
	    }

	/* Retry game commands for the object with a standard "get". */
	if (lib_try_game_command (gamestate, object, "get"))
		return TRUE;

	/* Note what we're doing. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You take ",
				"I take ",
				"%player% takes "));
	lib_print_object_np (gamestate, object);

	pf_buffer_string (filter, " from ");
	lib_print_object_np (gamestate, supporter);
	pf_buffer_string (filter, ".\n");

	/* Take possession of the object. */
	gs_object_player_get (gamestate, object);
	return TRUE;
}


/*
 * lib_cmd_get_npc()
 *
 * Reject attempts to take an npc.
 */
sc_bool
lib_cmd_get_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		npc;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "get", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_buffer_string (filter, "I don't think ");
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, " would appreciate being handled.\n");
	return TRUE;
}


/*
 * lib_cmd_drop_object()
 *
 * Attempt to drop the referenced object.
 */
sc_bool
lib_cmd_drop_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "drop", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Check that we have the object to drop. */
	if (gs_get_object_position (gamestate, object) != OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not holding ",
					"I am not holding ",
					"%player% is not holding "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Retry game commands for the object with a standard "drop". */
	if (lib_try_game_command (gamestate, object, "drop"))
		return TRUE;

	/* Note what we're doing. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You drop ",
				"I drop ",
				"%player% drops "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");

	/* Drop the object. */
	gs_object_to_room (gamestate, object, gamestate->playerroom);
	return TRUE;
}


/*
 * lib_cmd_get_all()
 *
 * Attempt to take all get-able objects.
 */
sc_bool
lib_cmd_get_all (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, count, trail, too_large, too_heavy;
	sc_bool			too_large_portable, too_heavy_portable;
	sc_int32		bytes;
	sc_bool			*is_getable, triggered;

	/* Unnecessary initializations to keep GCC happy. */
	too_large_portable = FALSE;	too_heavy_portable = FALSE;

	/* Create an array of flags to indicate get-able objects. */
	bytes = gamestate->object_count * sizeof (*is_getable);
	is_getable = sc_malloc (bytes);
	memset (is_getable, FALSE, bytes);

	/* Filter in only objects that are get-able within this context. */
	for (object = 0; object < gamestate->object_count; object++)
	    {
		/* Ignore any object not visible in the room. */
		if (!obj_indirectly_in_room (gamestate, object,
						gamestate->playerroom))
			continue;

		/* Disallow getting static objects. */
		if (obj_is_static (gamestate, object))
			continue;

		/* Check if we already have or are wearing it. */
		if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER
				|| gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER)
			continue;

		/* Check if an NPC already has it. */
		if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_NPC
				|| gs_get_object_position (gamestate, object)
							== OBJ_WORN_NPC)
			continue;

		/*
		 * Check if it's contained or on a surface (for this, it's
		 * necessary to use "get all from ...").
		 */
		if (gs_get_object_position (gamestate, object)
							== OBJ_ON_OBJECT
				|| gs_get_object_position (gamestate, object)
							== OBJ_IN_OBJECT)
			continue;

		/* This object is get-able. */
		is_getable[object] = TRUE;
	    }

	/* Try game commands for each get-able object with a standard "get". */
	too_large = too_heavy = -1;
	triggered = FALSE;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		sc_bool		is_portable;

		/* Ignore objects that are not get-able. */
		if (!is_getable[object])
			continue;

		/*
		 * See if the object takes us beyond capacity.  If it does
		 * and it's the first of its kind, note it, then see if we
		 * are able to pick up other objects, not yet tried.
		 */
		if (lib_object_too_heavy (gamestate, object, &is_portable))
		    {
			if (too_heavy == -1)
			    {
				too_heavy = object;
				too_heavy_portable = is_portable;
			    }
			is_getable[object] = FALSE;
			continue;
		    }
		if (lib_object_too_large (gamestate, object, &is_portable))
		    {
			if (too_large == -1)
			    {
				too_large = object;
				too_large_portable = is_portable;
			    }
			is_getable[object] = FALSE;
			continue;
		    }

		/* Try a standard game "get" with this object. */
		if (lib_try_game_command (gamestate, object, "get"))
		    {
			is_getable[object] = FALSE;
			triggered = TRUE;
		    }
	    }

	/* Check each remaining get-able object in turn. */
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		sc_bool		is_portable;

		/* Ignore objects not get-able or handled by the game. */
		if (!is_getable[object])
			continue;

		/* Same capacity checks as above. */
		if (lib_object_too_heavy (gamestate, object, &is_portable))
		    {
			if (too_heavy == -1)
			    {
				too_heavy = object;
				too_heavy_portable = is_portable;
			    }
			continue;
		    }
		if (lib_object_too_large (gamestate, object, &is_portable))
		    {
			if (too_large == -1)
			    {
				too_large = object;
				too_large_portable = is_portable;
			    }
			continue;
		    }

		/* Note what we're doing. */
		if (count > 0)
		    {
			if (count == 1)
			    {
				if (triggered)
					pf_buffer_character (filter, '\n');
				pf_buffer_string (filter,
					lib_select_response (gamestate,
							"You take ",
							"I take ",
							"%player% takes "));
			    }
			else
				pf_buffer_string (filter, ", ");
			lib_print_object_np (gamestate, trail);
		    }
		trail = object;

		/* Take possession of the object. */
		gs_object_player_get (gamestate, object);
		count++;
	    }
	sc_free (is_getable);

	if (count >= 1)
	    {
		/* Print out final listed object. */
		if (count == 1)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
						"You take ",
						"I take ",
						"%player% takes "));
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object_np (gamestate, trail);
		pf_buffer_character (filter, '.');
	    }

	/* If we ran out of capacity, print details of why. */
	if (too_heavy != -1)
	    {
		if (count > 0)
			pf_buffer_string (filter, "  ");
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, too_heavy);
		pf_buffer_string (filter, lib_select_response (gamestate,
					" is too heavy for you to carry",
					" is too heavy for me to carry",
					" is too heavy for %player% to carry"));
		if (too_heavy_portable)
			pf_buffer_string (filter, " at the moment");
		pf_buffer_string (filter, ".\n");
	    }
	else if (too_large != -1)
	    {
		if (count > 0)
			pf_buffer_string (filter, "  ");
		pf_buffer_string (filter,
				lib_select_response (gamestate,
				"Your hands are full",
				"My hands are full",
				"%player%'s hands are full"));
		if (too_large_portable)
			pf_buffer_string (filter, " at the moment");
		pf_buffer_string (filter, ".\n");
	    }
	else if (count == 0)
	    {
		if (!triggered)
			pf_buffer_string (filter,
				"There is nothing to pick up here.\n");
	    }
	else
		pf_buffer_character (filter, '\n');

	return TRUE;
}


/*
 * lib_cmd_get_all_from()
 *
 * Attempt to take all get-able objects from a container/surface.
 */
sc_bool
lib_cmd_get_all_from (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, cobject, count, trail;
	sc_int32		too_large, too_heavy;
	sc_bool			too_large_portable, too_heavy_portable;
	sc_bool			is_ambiguous;
	sc_int32		bytes;
	sc_bool			*is_getable, triggered;

	/* Unnecessary initializations to keep GCC happy. */
	too_large_portable = FALSE;	too_heavy_portable = FALSE;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "get from", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Disallow emptying non-container/non-surface objects. */
	if (!(obj_is_container (gamestate, object)
			|| obj_is_surface (gamestate, object)))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't take anything from ",
				"I can't take anything from ",
				"%player% can't take anything from "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* If object is a container, and is closed, reject now. */
	if (obj_is_container (gamestate, object)
		&& gs_get_object_openness (gamestate, object) > OBJ_OPEN)
	    {
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is closed.\n");
		return TRUE;
	    }

	/* Create an array of flags to indicate a game handling an object. */
	bytes = gamestate->object_count * sizeof (*is_getable);
	is_getable = sc_malloc (bytes);
	memset (is_getable, FALSE, bytes);

	/* Filter in only objects that are get-able within this context. */
	for (cobject = 0; cobject < gamestate->object_count; cobject++)
	    {
		/* Ignore any object not contained or on a surface. */
		if (gs_get_object_position (gamestate, cobject)
							!= OBJ_ON_OBJECT
				&& gs_get_object_position (gamestate, cobject)
							!= OBJ_IN_OBJECT)
			continue;

		/* Ignore objects not belonging to our object. */
		if (gs_get_object_parent (gamestate, cobject) != object)
			continue;

		/* Reject removing objects from closed containers. */
		if (gs_get_object_position (gamestate, cobject)
							== OBJ_IN_OBJECT
				&& gs_get_object_openness (gamestate, object)
							> OBJ_OPEN)
			continue;

		/* Disallow getting static objects. */
		if (obj_is_static (gamestate, cobject))
			continue;

		/* This object is get-able. */
		is_getable[cobject] = TRUE;
	    }

	/* Try game commands for each get-able object with a standard "get". */
	too_large = too_heavy = -1;
	triggered = FALSE;
	for (cobject = 0; cobject < gamestate->object_count; cobject++)
	    {
		sc_bool		is_portable;

		/* Ignore objects that are not get-able. */
		if (!is_getable[cobject])
			continue;

		/*
		 * If we're holding the container or surface, checking
		 * the player's capacity is meaningless, otherwise, check.
		 */
		if (!obj_indirectly_held_by_player (gamestate, cobject))
		    {
			if (lib_object_too_heavy
					(gamestate, cobject, &is_portable))
			    {
				if (too_heavy == -1)
				    {
					too_heavy = cobject;
					too_heavy_portable = is_portable;
				    }
				is_getable[cobject] = FALSE;
				continue;
			    }
			if (lib_object_too_large
					(gamestate, cobject, &is_portable))
			    {
				if (too_large == -1)
				    {
					too_large = cobject;
					too_large_portable = is_portable;
				    }
				is_getable[cobject] = FALSE;
				continue;
			    }
		    }

		/* Try a standard game "get" with this object. */
		if (lib_try_game_command (gamestate, cobject, "get"))
		    {
			is_getable[cobject] = FALSE;
			triggered = TRUE;
		    }
	    }

	/* Check each remaining get-able object in turn. */
	count = 0; trail = -1;
	for (cobject = 0; cobject < gamestate->object_count; cobject++)
	    {
		sc_bool		is_portable;

		/* Ignore objects not get-able or handled by the game. */
		if (!is_getable[cobject])
			continue;

		/* Same capacity checks as above. */
		if (!obj_indirectly_held_by_player (gamestate, cobject))
		    {
			if (lib_object_too_heavy
					(gamestate, cobject, &is_portable))
			    {
				if (too_heavy == -1)
				    {
					too_heavy = cobject;
					too_heavy_portable = is_portable;
				    }
				continue;
			    }
			if (lib_object_too_large
					(gamestate, cobject, &is_portable))
			    {
				if (too_large == -1)
				    {
					too_large = cobject;
					too_large_portable = is_portable;
				    }
				continue;
			    }
		    }

		/* Note what we're doing. */
		if (count > 0)
		    {
			if (count == 1)
			    {
				if (triggered)
					pf_buffer_character (filter, '\n');
				pf_buffer_string (filter,
					lib_select_response (gamestate,
							"You take ",
							"I take ",
							"%player% takes "));
			    }
			else
				pf_buffer_string (filter, ", ");
			lib_print_object_np (gamestate, trail);
		    }
		trail = cobject;

		/* Take possession of the object. */
		gs_object_player_get (gamestate, cobject);
		count++;
	    }
	sc_free (is_getable);

	if (count >= 1)
	    {
		/* Print out final listed object. */
		if (count == 1)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
						"You take ",
						"I take ",
						"%player% takes "));
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object_np (gamestate, trail);
		pf_buffer_string (filter, " from ");
		lib_print_object_np (gamestate, object);
		pf_buffer_character (filter, '.');
	    }

	/* If we ran out of capacity, print details of why. */
	if (too_heavy != -1)
	    {
		if (count > 0)
			pf_buffer_string (filter, "  ");
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, too_heavy);
		pf_buffer_string (filter, lib_select_response (gamestate,
					" is too heavy for you to carry",
					" is too heavy for me to carry",
					" is too heavy for %player% to carry"));
		if (too_heavy_portable)
			pf_buffer_string (filter, " at the moment");
		pf_buffer_string (filter, ".\n");
	    }
	else if (too_large != -1)
	    {
		if (count > 0)
			pf_buffer_string (filter, "  ");
		pf_buffer_string (filter,
				lib_select_response (gamestate,
				"Your hands are full",
				"My hands are full",
				"%player%'s hands are full"));
		if (too_large_portable)
			pf_buffer_string (filter, " at the moment");
		pf_buffer_string (filter, ".\n");
	    }
	else if (count == 0)
	    {
		if (!triggered)
		    {
			if (obj_is_container (gamestate, object)
				&& obj_is_surface (gamestate, object))
			    {
				if (gs_get_object_openness (gamestate, object)
								<= OBJ_OPEN)
				    {
					pf_buffer_string (filter,
						"There is nothing in or on ");
					lib_print_object_np (gamestate, object);
				    }
				else
				    {
					pf_buffer_string (filter,
							"There is nothing on ");
					lib_print_object_np (gamestate, object);
					pf_buffer_string (filter,
							" and it is closed");
				    }
			    }
			else
			    {
				if (obj_is_container (gamestate, object))
					pf_buffer_string (filter,
						"There is nothing inside ");
				else
					pf_buffer_string (filter,
						"There is nothing on ");
				lib_print_object_np (gamestate, object);
			    }
			pf_buffer_string (filter, ".\n");
		    }
	    }
	else
		pf_buffer_character (filter, '\n');

	return TRUE;
}


/*
 * lib_cmd_drop_all()
 *
 * Attempt to drop all held objects.
 */
sc_bool
lib_cmd_drop_all (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, count, trail;
	sc_int32		bytes;
	sc_bool			*is_dropable, triggered;

	/* Create an array of flags to indicate a game handling an object. */
	bytes = gamestate->object_count * sizeof (*is_dropable);
	is_dropable = sc_malloc (bytes);
	memset (is_dropable, FALSE, bytes);

	/* Filter in only objects that are drop-able within this context. */
	for (object = 0; object < gamestate->object_count; object++)
	    {
		/* Check that we have the object to drop. */
		if (gs_get_object_position (gamestate, object)
							!= OBJ_HELD_PLAYER)
			continue;

		/* This object is drop-able. */
		is_dropable[object] = TRUE;
	    }

	/* Try game commands for each drop-able object with standard "drop". */
	triggered = FALSE;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		/* Ignore objects that are not get-able. */
		if (!is_dropable[object])
			continue;

		/* Try a standard "drop" with this object. */
		if (lib_try_game_command (gamestate, object, "drop"))
		    {
			is_dropable[object] = FALSE;
			triggered = TRUE;
		    }
	    }

	/* Check each remaining drop-able object in turn. */
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		/* Ignore objects not drop-able or handled by the game. */
		if (!is_dropable[object])
			continue;

		/* Note what we're doing. */
		if (count > 0)
		    {
			if (count == 1)
			    {
				if (triggered)
					pf_buffer_character (filter, '\n');
				pf_buffer_string (filter,
					lib_select_response (gamestate,
							"You drop ",
							"I drop ",
							"%player% drops "));
			    }
			else
				pf_buffer_string (filter, ", ");
			lib_print_object_np (gamestate, trail);
		    }
		trail = object;

		/* Drop the object. */
		gs_object_to_room (gamestate, object, gamestate->playerroom);
		count++;
	    }
	sc_free (is_dropable);

	if (count >= 1)
	    {
		/* Print out final listed object. */
		if (count == 1)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
						"You drop ",
						"I drop ",
						"%player% drops "));
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object_np (gamestate, trail);
		pf_buffer_string (filter, ".\n");
	    }
	else
	    {
		if (!triggered)
			pf_buffer_string (filter,
					lib_select_response (gamestate,
					"You're not carrying anything.\n",
					"I'm not carrying anything.\n",
					"%player%'s not carrying anything.\n"));
	    }

	return TRUE;
}


/*
 * lib_cmd_give_object_npc()
 *
 * Attempt to give an object to an NPC.
 */
sc_bool
lib_cmd_give_object_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, npc;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "give", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "give to", NULL);
	if (npc == -1)
		return TRUE;

	/* After all that, the npc is disinterested. */
	pf_new_sentence (filter);
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, " doesn't seem interested in ");
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_wear_object()
 *
 * Attempt to wear the referenced object.
 */
sc_bool
lib_cmd_wear_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		object;
	sc_bool			wearable;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "wear", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Check that we're carrying it. */
	if (gs_get_object_position (gamestate, object) != OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not holding ",
					"I am not holding ",
					"%player% is not holding "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Check for static object moved to player by event. */
	if (obj_is_static (gamestate, object))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't wear ",
					"I can't wear ",
					"%player% can't wear "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Disallow wearing non-wearable objects. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Wearable";
	wearable = prop_get_boolean (bundle, "B<-sis", vt_key);
	if (!wearable)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't wear ",
					"I can't wear ",
					"%player% can't wear "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Note what we're doing. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You put on ",
				"I put on ",
				"%player% puts on "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");

	/* Put on the object. */
	gs_object_player_wear (gamestate, object);
	return TRUE;
}


/*
 * lib_cmd_remove_object()
 *
 * Attempt to remove the referenced object.
 */
sc_bool
lib_cmd_remove_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "remove", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Check that we are wearing the object. */
	if (gs_get_object_position (gamestate, object) != OBJ_WORN_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not wearing ",
					"I am not wearing ",
					"%player% is not wearing "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Note what we're doing. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You remove ",
				"I remove ",
				"%player% removes "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");

	/* Take off the object, return to held state. */
	gs_object_player_get (gamestate, object);
	return TRUE;
}


/*
 * lib_cmd_remove_all()
 *
 * Attempt to remove all worn objects.
 */
sc_bool
lib_cmd_remove_all (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, count, trail;

	/* Find and remove each object worn by the player. */
	count = 0; trail = -1;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (gs_get_object_position (gamestate, object)
							!= OBJ_WORN_PLAYER)
			continue;

		/* Note what we're doing. */
		if (count > 0)
		    {
			if (count == 1)
			    {
				pf_buffer_string (filter,
					lib_select_response (gamestate,
							"You remove ",
							"I remove ",
							"%player% removes "));
			    }
			else
				pf_buffer_string (filter, ", ");
			lib_print_object_np (gamestate, trail);
		    }
		trail = object;

		/* Take off the object, return to held state. */
		gs_object_player_get (gamestate, object);
		count++;
	    }
	if (count >= 1)
	    {
		/* Print out final listed object. */
		if (count == 1)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
						"You remove ",
						"I remove ",
						"%player% removes "));
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object_np (gamestate, trail);
		pf_buffer_string (filter, ".\n");
	    }
	else
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You're not wearing anything.\n",
					"I'm not wearing anything.\n",
					"%player%'s not wearing anything.\n"));
	    }

	return TRUE;
}


/*
 * lib_cmd_inventory()
 *
 * List objects carried and worn by the player.
 */
sc_bool
lib_cmd_inventory (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object, count, trail;
	sc_bool			wearing;

	/* Find and list each object worn by the player. */
	count = 0; trail = -1; wearing = FALSE;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					pf_buffer_string (filter,
						lib_select_response (gamestate,
						"You are wearing ",
						"I am wearing ",
						"%player% is wearing "));
				    }
				else
					pf_buffer_string (filter, ", ");
				lib_print_object (gamestate, trail);
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
			pf_buffer_string (filter,
				lib_select_response (gamestate,
						"You are wearing ",
						"I am wearing ",
						"%player% is wearing "));
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object (gamestate, trail);
		wearing = TRUE;
	    }

	/* Find and list each object owned by the player. */
	count = 0;
	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER)
		    {
			if (count > 0)
			    {
				if (count == 1)
				    {
					if (wearing)
					    {
						pf_buffer_string (filter,
						lib_select_response (gamestate,
						", and you are carrying ",
						", and I am carrying ",
						", and %player% is carrying "));
					    }
					else
					    {
					    pf_buffer_string (filter,
						lib_select_response (gamestate,
						"You are carrying ",
						"I am carrying ",
						"%player% is carrying "));
					    }
				    }
				else
					pf_buffer_string (filter, ", ");
				lib_print_object (gamestate, trail);
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
			if (wearing)
			    {
				pf_buffer_string (filter,
					lib_select_response (gamestate,
						", and you are carrying ",
						", and I am carrying ",
						", and %player% is carrying "));
			    }
			else
			    {
				pf_buffer_string (filter,
					lib_select_response (gamestate,
						"You are carrying ",
						"I am carrying ",
						"%player% is carrying "));
			    }
		    }
		else
			pf_buffer_string (filter, " and ");
		lib_print_object (gamestate, trail);
		pf_buffer_character (filter, '.');

		/* Print contents of every container carried. */
		for (object = 0; object < gamestate->object_count; object++)
		    {
			if (gs_get_object_position (gamestate, object)
							== OBJ_HELD_PLAYER)
			    {
				if (obj_is_container (gamestate, object)
					&& gs_get_object_openness (gamestate,
							object) <= OBJ_OPEN)
				    {
					lib_list_in_object (gamestate,
								object, TRUE);
				    }
			    }
		    }
		pf_buffer_character (filter, '\n');
	    }
	else
	    {
		if (wearing)
			pf_buffer_string (filter, ".\n");
		else
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
				"You are carrying nothing.\n",
				"I am carrying nothing.\n",
				"%player% is carrying nothing.\n"));
		    }
	    }

	/* Successful command. */
	return TRUE;
}


/*
 * lib_cmd_open_object()
 *
 * Attempt to open the referenced object.
 */
sc_bool
lib_cmd_open_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "open", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Get the current object openness. */
	openness = gs_get_object_openness (gamestate, object);

	/* React to the request based on openness state. */
	switch (openness)
	    {
	    case OBJ_OPEN:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is already open!\n");
		return TRUE;

	    case OBJ_CLOSED:
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You open ",
					"I open ",
					"%player% opens "));
		lib_print_object_np (gamestate, object);
		pf_buffer_character (filter, '.');

		/* Set open state, and list contents. */
		gs_set_object_openness (gamestate, object, OBJ_OPEN);
		lib_list_in_object (gamestate, object, TRUE);
		pf_buffer_character (filter, '\n');
		return TRUE;

	    case OBJ_LOCKED:
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't open ",
				"I can't open ",
				"%player% can't open "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " as it is locked!\n");
		return TRUE;

	    default:
		break;
	    }

	/* The object isn't openable. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't open ",
				"I can't open ",
				"%player% can't open "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, "!\n");
	return TRUE;
}


/*
 * lib_cmd_close_object()
 *
 * Attempt to close the referenced object.
 */
sc_bool
lib_cmd_close_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "close", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Get the current object openness. */
	openness = gs_get_object_openness (gamestate, object);

	/* React to the request based on openness state. */
	switch (openness)
	    {
	    case OBJ_OPEN:
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You close ",
					"I close ",
					"%player% closes "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");

		/* Set closed state. */
		gs_set_object_openness (gamestate, object, OBJ_CLOSED);
		return TRUE;

	    case OBJ_CLOSED:
	    case OBJ_LOCKED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is already closed!\n");
		return TRUE;

	    default:
		break;
	    }

	/* The object isn't closeable. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't close ",
				"I can't close ",
				"%player% can't close "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_attempt_key_acquisition()
 *
 * Automatically get an object being used as a key, if possible.
 */
static void
lib_attempt_key_acquisition (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Disallow getting static objects. */
	if (obj_is_static (gamestate, object))
		return;

	/* If the object is not seen or available, reject the attempt. */
	if (!(gs_get_object_seen (gamestate, object)
			&& obj_indirectly_in_room (gamestate,
						object, gamestate->playerroom)))
		return;

	/*
	 * Check if we already have it, or are wearing it, or if a NPC
	 * has or is wearing it.
	 */
	if (gs_get_object_position (gamestate, object) == OBJ_HELD_PLAYER
			|| gs_get_object_position (gamestate, object)
							== OBJ_WORN_PLAYER
			|| gs_get_object_position (gamestate, object)
							== OBJ_HELD_NPC
			|| gs_get_object_position (gamestate, object)
							== OBJ_WORN_NPC)
		return;

	/*
	 * If the object is contained in or on something we're already
	 * holding, capacity checks are meaningless.
	 */
	if (!obj_indirectly_held_by_player (gamestate, object))
	    {
		if (lib_object_too_heavy (gamestate, object, NULL)
			    || lib_object_too_large (gamestate, object, NULL))
			return;
	    }

	/* Retry game commands for the object with a standard "get". */
	if (lib_try_game_command (gamestate, object, "get"))
		return;

	/* Note what we're doing. */
	if (gs_get_object_position (gamestate, object) == OBJ_IN_OBJECT
		|| gs_get_object_position (gamestate, object) == OBJ_ON_OBJECT)
	    {
		pf_buffer_string (filter, "(Taking ");
		lib_print_object_np (gamestate, object);

		pf_buffer_string (filter, " from ");
		lib_print_object_np (gamestate,
				gs_get_object_parent (gamestate, object));
		pf_buffer_string (filter, " first)\n");
	    }
	else
	    {
		pf_buffer_string (filter, "(Picking up ");
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " first)\n");
	    }

	/* Take possession of the object. */
	gs_object_player_get (gamestate, object);
}


/*
 * lib_cmd_unlock_object_with()
 *
 * Attempt to unlock the referenced object.
 */
sc_bool
lib_cmd_unlock_object_with (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, key;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "unlock", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/*
	 * Now try to get the key from referenced text, and disambiguate
	 * as usual.
	 */
	if (!uip_match ("%object%", var_get_ref_text (vars), gamestate))
	    {
		pf_buffer_string (filter,
				"What do you want to unlock that with?\n");
		return TRUE;
	    }
	key = lib_disambiguate_object (gamestate, "unlock that with", NULL);
	if (key == -1)
		return TRUE;

	/* React to the request based on openness state. */
	openness = gs_get_object_openness (gamestate, object);
	switch (openness)
	    {
	    case OBJ_OPEN:
	    case OBJ_CLOSED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is not locked!\n");
		return TRUE;

	    case OBJ_LOCKED:
		{
		sc_vartype_t	vt_key[3];
		sc_int32	key_index, the_key;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Key";
		key_index = prop_get_integer (bundle, "I<-sis", vt_key);
		if (key_index == -1)
			break;

		the_key = obj_dynamic_object (gamestate, key_index);
		if (the_key != key)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
					"You can't unlock ",
					"I can't unlock ",
					"%player% can't unlock "));
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, " with ");
			lib_print_object_np (gamestate, key);
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }

		if (gs_get_object_position (gamestate, key) != OBJ_HELD_PLAYER)
		    {
			pf_buffer_string (filter,
					lib_select_response (gamestate,
						"You are not holding ",
						"I am not holding ",
						"%player% is not holding "));
			lib_print_object_np (gamestate, key);
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }

		gs_set_object_openness (gamestate, object, OBJ_CLOSED);
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You unlock ",
				"I unlock ",
				"%player% unlocks "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " with ");
		lib_print_object_np (gamestate, key);
		pf_buffer_string (filter, ".\n");
		return TRUE;
		}

	    default:
		break;
	    }

	/* The object isn't lockable. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't unlock ",
				"I can't unlock ",
				"%player% can't unlock "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_unlock_object()
 *
 * Attempt to unlock the referenced object, automatically selecting key.
 */
sc_bool
lib_cmd_unlock_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "unlock", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* React to the request based on openness state. */
	openness = gs_get_object_openness (gamestate, object);
	switch (openness)
	    {
	    case OBJ_OPEN:
	    case OBJ_CLOSED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is not locked!\n");
		return TRUE;

	    case OBJ_LOCKED:
		{
		sc_vartype_t	vt_key[3];
		sc_int32	key_index, key;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Key";
		key_index = prop_get_integer (bundle, "I<-sis", vt_key);
		if (key_index == -1)
			break;

		key = obj_dynamic_object (gamestate, key_index);
		lib_attempt_key_acquisition (gamestate, key);
		if (gs_get_object_position (gamestate, key) != OBJ_HELD_PLAYER)
		    {
			pf_buffer_string (filter,
					lib_select_response (gamestate,
						"You don't have",
						"I don't have",
						"%player% doesn't have"));
			pf_buffer_string (filter, " anything to unlock ");
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, " with!\n");
			return TRUE;
		    }

		gs_set_object_openness (gamestate, object, OBJ_CLOSED);
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You unlock ",
					"I unlock ",
					"%player% unlocks "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " with ");
		lib_print_object_np (gamestate, key);
		pf_buffer_string (filter, ".\n");
		return TRUE;
		}

	    default:
		break;
	    }

	/* The object isn't lockable. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't unlock ",
				"I can't unlock ",
				"%player% can't unlock "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_lock_object_with()
 *
 * Attempt to lock the referenced object.
 */
sc_bool
lib_cmd_lock_object_with (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, key;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "lock", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/*
	 * Now try to get the key from referenced text, and disambiguate
	 * as usual.
	 */
	if (!uip_match ("%object%", var_get_ref_text (vars), gamestate))
	    {
		pf_buffer_string (filter,
				"What do you want to lock that with?\n");
		return TRUE;
	    }
	key = lib_disambiguate_object (gamestate, "lock that with", NULL);
	if (key == -1)
		return TRUE;

	/* React to the request based on openness state. */
	openness = gs_get_object_openness (gamestate, object);
	switch (openness)
	    {
	    case OBJ_OPEN:
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't lock ",
					"I can't lock ",
					"%player% can't lock "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " as it is open.\n");
		return TRUE;

	    case OBJ_CLOSED:
		{
		sc_vartype_t	vt_key[3];
		sc_int32	key_index, the_key;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Key";
		key_index = prop_get_integer (bundle, "I<-sis", vt_key);
		if (key_index == -1)
			break;

		the_key = obj_dynamic_object (gamestate, key_index);
		if (the_key != key)
		    {
			pf_buffer_string (filter,
					lib_select_response (gamestate,
						"You can't lock ",
						"I can't lock ",
						"%player% can't lock "));
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, " with ");
			lib_print_object_np (gamestate, key);
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }

		if (gs_get_object_position (gamestate, key) != OBJ_HELD_PLAYER)
		    {
			pf_buffer_string (filter,
					lib_select_response (gamestate,
						"You are not holding ",
						"I am not holding ",
						"%player% is not holding "));
			lib_print_object_np (gamestate, key);
			pf_buffer_string (filter, ".\n");
			return TRUE;
		    }

		gs_set_object_openness (gamestate, object, OBJ_LOCKED);
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You lock ",
					"I lock ",
					"%player% locks "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " with ");
		lib_print_object_np (gamestate, key);
		pf_buffer_string (filter, ".\n");
		return TRUE;
		}

	    case OBJ_LOCKED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is already locked!\n");
		return TRUE;

	    default:
		break;
	    }

	/* The object isn't lockable. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't lock ",
				"I can't lock ",
				"%player% can't lock "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_lock_object()
 *
 * Attempt to lock the referenced object, automatically selecting key.
 */
sc_bool
lib_cmd_lock_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "lock", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* React to the request based on openness state. */
	openness = gs_get_object_openness (gamestate, object);
	switch (openness)
	    {
	    case OBJ_OPEN:
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't lock ",
					"I can't lock ",
					"%player% can't lock "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " as it is open.\n");
		return TRUE;

	    case OBJ_CLOSED:
		{
		sc_vartype_t	vt_key[3];
		sc_int32	key_index, key;

		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Key";
		key_index = prop_get_integer (bundle, "I<-sis", vt_key);
		if (key_index == -1)
			break;

		key = obj_dynamic_object (gamestate, key_index);
		lib_attempt_key_acquisition (gamestate, key);
		if (gs_get_object_position (gamestate, key) != OBJ_HELD_PLAYER)
		    {
			pf_buffer_string (filter,
					lib_select_response (gamestate,
						"You don't have",
						"I don't have",
						"%player% doesn't have"));
			pf_buffer_string (filter, " anything to lock ");
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, " with!\n");
			return TRUE;
		    }

		gs_set_object_openness (gamestate, object, OBJ_LOCKED);
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You lock ",
					"I lock ",
					"%player% locks "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " with ");
		lib_print_object_np (gamestate, key);
		pf_buffer_string (filter, ".\n");
		return TRUE;
		}

	    case OBJ_LOCKED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is already locked!\n");
		return TRUE;

	    default:
		break;
	    }

	/* The object isn't lockable. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't lock ",
				"I can't lock ",
				"%player% can't lock "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_compare_subject()
 *
 * Compare a subject, comma or NUL terminated.  Helper for ask.
 */
static sc_bool
lib_compare_subject (const sc_char *subject, sc_uint16 posn,
		const sc_char *string)
{
	sc_uint16	wpos, spos;

	/* Skip any leading subject spaces. */
	for (wpos = posn; subject[wpos] != NUL
				&& isspace (subject[wpos]); )
		wpos++;
	for (spos = 0; string[spos] != NUL
				&& isspace (string[spos]); )
		spos++;

	/* Match characters from words with the string at position. */
	while (TRUE)
	    {
		/* Any character mismatch means no match. */
		if (tolower (subject[wpos]) != tolower (string[spos]))
			return FALSE;

		/* Move to next character in each. */
		wpos++; spos++;

		/*
		 * If at space, advance over whitespace in subjects list.
		 * Stop when we hit the end of the element or list.
		 */
		while (isspace (subject[wpos])
				&& subject[wpos] != COMMA
				&& subject[wpos] != NUL)
			subject++;

		/* Advance over whitespace in the current string too. */
		while (isspace (string[spos])
					&& string[spos] != NUL)
			spos++;

		/*
		 * If we found the end of the subject, and the end of
		 * the current string, we've matched.  If not at the end
		 * of the current string, though, only a partial match.
		 */
		if (subject[wpos] == NUL || subject[wpos] == COMMA)
		    {
			if (string[spos] == NUL)
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
static sc_bool
lib_npc_reply_to (sc_gamestateref_t gamestate, sc_int32 npc, sc_int32 topic)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		task;
	sc_char			*response;

	/* Find any associated task to control response. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Topics";
	vt_key[3].integer = topic;
	vt_key[4].string = "Task";
	task = prop_get_integer (bundle, "I<-sisis", vt_key);

	/* Get the response, and print if anything there. */
	if (task > 0 && gs_get_task_done (gamestate, task - 1))
		vt_key[4].string = "AltReply";
	else
		vt_key[4].string = "Reply";
	response = prop_get_string (bundle, "S<-sisis", vt_key);
	if (!sc_strempty (response))
	    {
		pf_buffer_string (filter, response);
		pf_buffer_character (filter, '\n');
		return TRUE;
	    }

	/* No response to this combination. */
	return FALSE;
}


/*
 * lib_cmd_ask_npc_about()
 *
 * Converse with NPC.
 */
sc_bool
lib_cmd_ask_npc_about (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_int32		npc, topic_count, topic;
	sc_int32		topic_match, default_topic;
	sc_bool			found, default_found;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "ask", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	if (lib_trace)
		sc_trace ("Library: asking NPC %ld\n", npc);

	/* Get the topics the NPC converses about. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Topics";
	topic_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;
	topic_match = default_topic = -1;
	found = default_found = FALSE;
	for (topic = 0; topic < topic_count; topic++)
	    {
		sc_char		*subjects;
		sc_uint16	posn;

		/* Get subject list for this topic. */
		vt_key[3].integer = topic;
		vt_key[4].string = "Subject";
		subjects = prop_get_string (bundle, "S<-sisis", vt_key);

		/* If this is the special "*" topic, note and continue. */
		if (!sc_strcasecmp (subjects, "*"))
		    {
			if (lib_trace)
				sc_trace ("Library:   \"*\" is %ld\n", topic);

			default_topic = topic;
			default_found = TRUE;
			continue;
		    }

		/* Split into subjects by comma delimiter. */
		for (posn = 0; subjects[posn] != NUL; )
		    {
			if (lib_trace)
			    {
				sc_trace ("Library:   subject %s[%hd]\n",
							subjects, posn);
			    }

			/* See if this subject matches. */
			if (lib_compare_subject (subjects, posn,
						var_get_ref_text (vars)))
			    {
				if (lib_trace)
					sc_trace ("Library:   matched\n");

				topic_match = topic;
				found = TRUE;
				break;
			    }

			/* Move to next subject, or end of list. */
			while (subjects[posn] != COMMA
						&& subjects[posn] != NUL)
				posn++;
			if (subjects[posn] == COMMA)
				posn++;
		    }
	    }

	/* Handle any matched subject first, and "*" second. */
	if (found
		&& lib_npc_reply_to (gamestate, npc, topic_match))
			return TRUE;
	else if (default_found
		&& lib_npc_reply_to (gamestate, npc, default_topic))
			return TRUE;

	/* NPC has no response. */
	pf_new_sentence (filter);
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, lib_select_response (gamestate,
			" does not respond to your question.\n",
			" does not respond to my question.\n",
			" does not respond to %player%'s question.\n"));
	return TRUE;
}


/*
 * lib_cmd_put_object_on()
 *
 * Attempt to put the referenced object onto a surface.
 */
sc_bool
lib_cmd_put_object_on (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		object, surface, check;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "move", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Ensure the referenced object is held. */
	if (gs_get_object_position (gamestate, object) != OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not holding ",
					"I am not holding ",
					"%player% is not holding "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/*
	 * Now try to get the surface from referenced text, and disambiguate
	 * as usual.
	 */
	if (!uip_match ("%object%", var_get_ref_text (vars), gamestate))
	    {
		pf_buffer_string (filter,
				"What do you want to put that on?\n");
		return TRUE;
	    }
	surface = lib_disambiguate_object (gamestate, "put that on", NULL);
	if (surface == -1)
		return TRUE;

	/* Verify that the surface object is a surface. */
	if (!obj_is_surface (gamestate, surface))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't put anything on ",
					"I can't put anything on ",
					"%player% can't put anything on "));
		lib_print_object_np (gamestate, surface);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Avoid the obvious possibility of infinite recursion. */
	if (surface == object)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
			"You can't put an object onto itself!\n",
			"I can't put an object onto itself!\n",
			"%player% can't put an object onto itself!\n"));
		return TRUE;
	    }

	/* Avoid the subtle possibility of infinite recursion. */
	check = surface;
	while (gs_get_object_position (gamestate, check) == OBJ_ON_OBJECT
		|| gs_get_object_position (gamestate, check) == OBJ_IN_OBJECT)
	    {
		check = gs_get_object_parent (gamestate, check);
		if (check == object)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
				"You can't put an object onto one",
				"I can't put an object onto one",
				"%player% can't put an object onto one"));
			pf_buffer_string (filter, " it's already on or in!\n");
			return TRUE;
		    }
	    }

	/* Update object state, print response, and return. */
	gs_object_move_onto (gamestate, object, surface);
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You put ",
				"I put ",
				"%player% puts "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, " on ");
	lib_print_object_np (gamestate, surface);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_put_object_in()
 *
 * Attempt to put the referenced object into a container.
 */
sc_bool
lib_cmd_put_object_in (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		object, cobject, container, check, count;
	sc_int32		openness;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "move", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Ensure the referenced object is held. */
	if (gs_get_object_position (gamestate, object) != OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not holding ",
					"I am not holding ",
					"%player% is not holding "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/*
	 * Now try to get the container from referenced text, and disambiguate
	 * as usual.
	 */
	if (!uip_match ("%object%", var_get_ref_text (vars), gamestate))
	    {
		pf_buffer_string (filter,
				"What do you want to put that in?\n");
		return TRUE;
	    }
	container = lib_disambiguate_object (gamestate, "put that in", NULL);
	if (container == -1)
		return TRUE;

	/* Verify that the container object is a container. */
	if (!obj_is_container (gamestate, container))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't put anything inside ",
				"I can't put anything inside ",
				"%player% can't put anything inside "));
		lib_print_object_np (gamestate, container);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Check for locked or closed containers. */
	openness = gs_get_object_openness (gamestate, container);
	switch (openness)
	    {
	    case OBJ_LOCKED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, container);
		pf_buffer_string (filter, " is locked!\n");
		return TRUE;
	    case OBJ_CLOSED:
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, container);
		pf_buffer_string (filter, " is closed!\n");
		return TRUE;
	    }

	/* Is the object too big for the container? */
	if (obj_get_size (gamestate, object)
			> obj_get_container_maxsize (gamestate, container))
	    {
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is too big to fit inside ");
		lib_print_object_np (gamestate, container);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Count container contents, and check capacity limits. */
	count = 0;
	for (cobject = 0; cobject < gamestate->object_count; cobject++)
	    {
		if (gs_get_object_position (gamestate, cobject)
								== OBJ_IN_OBJECT
				&& gs_get_object_parent (gamestate, cobject)
								== container)
			count++;
	    }
	if (count >= obj_get_container_capacity (gamestate, container))
	    {
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " can't fit inside ");
		lib_print_object_np (gamestate, container);
		pf_buffer_string (filter, " at the moment.\n");
		return TRUE;
	    }

	/* Avoid the obvious possibility of infinite recursion. */
	if (container == object)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
			"You can't put an object inside itself!\n",
			"I can't put an object inside itself!\n",
			"%player% can't put an object inside itself!\n"));
		return TRUE;
	    }

	/* Avoid the subtle possibility of infinite recursion. */
	check = container;
	while (gs_get_object_position (gamestate, check) == OBJ_ON_OBJECT
		|| gs_get_object_position (gamestate, check) == OBJ_IN_OBJECT)
	    {
		check = gs_get_object_parent (gamestate, check);
		if (check == object)
		    {
			pf_buffer_string (filter,
				lib_select_response (gamestate,
				"You can't put an object inside one",
				"I can't put an object inside one",
				"%player% can't put an object inside one"));
			pf_buffer_string (filter, " it's already on or in!\n");
			return TRUE;
		    }
	    }

	/* Update object state, print response and return. */
	gs_object_move_into (gamestate, object, container);
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You put ",
				"I put ",
				"%player% puts "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, " in ");
	lib_print_object_np (gamestate, container);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_read_object()
 *
 * Attempt to read the referenced object.
 */
sc_bool
lib_cmd_read_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		object;
	sc_bool			is_readable;
	sc_char			*readtext, *description;
	sc_int32		task;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "read", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Verify that the object is readable. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Readable";
	is_readable = prop_get_boolean (bundle, "B<-sis", vt_key);
	if (!is_readable)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't read ",
					"I can't read ",
					"%player% can't read "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;
	    }

	/* Get and print the object's read text, if any. */
	vt_key[2].string = "ReadText";
	readtext = prop_get_string (bundle, "S<-sis", vt_key);
	if (!sc_strempty (readtext))
	    {
		pf_buffer_string (filter, readtext);
		pf_buffer_character (filter, '\n');
		return TRUE;
	    }

	/* Degrade to a shortened object examine. */
	vt_key[2].string = "Task";
	task = prop_get_integer (bundle, "I<-sis", vt_key) - 1;

	/* Select either the main or the alternate description. */
	if (task >= 0 && gs_get_task_done (gamestate, task))
		vt_key[2].string = "AltDesc";
	else
		vt_key[2].string = "Description";

	/* Print the description. */
	description = prop_get_string (bundle, "S<-sis", vt_key);
	pf_buffer_string (filter, description);
	pf_buffer_character (filter, '\n');
	return TRUE;
}


/*
 * lib_cmd_attack_npc()
 * lib_cmd_attack_npc_with()
 *
 * Attempt to attack an NPC, with and without weaponry.
 */
sc_bool
lib_cmd_attack_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		npc;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "attack", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Print a standard response. */
	pf_new_sentence (filter);
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, lib_select_response (gamestate,
				" avoids your feeble attempts.\n",
				" avoids my feeble attempts.\n",
				" avoids %player%'s feeble attempts.\n"));
	return TRUE;
}

sc_bool
lib_cmd_attack_npc_with (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object, npc;
	sc_vartype_t		vt_key[3];
	sc_bool			weapon;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "attack", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "attack with", NULL);
	if (object == -1)
		return TRUE;

	/* Ensure the referenced object is held. */
	if (gs_get_object_position (gamestate, object) != OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not holding ",
					"I am not holding ",
					"%player% is not holding "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Check for static object moved to player by event. */
	if (obj_is_static (gamestate, object))
	    {
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, " is not a weapon.\n");
		return TRUE;
	    }

	/* Print standard response depending on if the object is a weapon. */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Weapon";
	weapon = prop_get_boolean (bundle, "B<-sis", vt_key);
	if (weapon)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You swing at ",
					"I swing at ",
					"%player% swings at "));
		lib_print_npc_np (gamestate, npc);
		pf_buffer_string (filter, " with ");
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, lib_select_response (gamestate,
					" but you miss.\n",
					" but I miss.\n",
					" but misses.\n"));
	    }
	else
	    {
		pf_buffer_string (filter, "I don't think ");
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter,
				" would be a very effective weapon.\n");
	    }
	return TRUE;
}


/*
 * lib_cmd_kiss_npc()
 * lib_cmd_kiss_object()
 * lib_cmd_kiss_other()
 *
 * Reject romantic advances in all cases.
 */
sc_bool
lib_cmd_kiss_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		npc, gender;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "kiss", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	vt_key[0].string  = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string  = "Gender";
	gender = prop_get_integer (bundle, "I<-sis", vt_key);

	switch (gender)
	    {
	    case NPC_MALE:
		pf_buffer_string (filter,
				"I'm not sure he would appreciate that!\n");
		break;

	    case NPC_FEMALE:
		pf_buffer_string (filter,
				"I'm not sure she would appreciate that!\n");
		break;

	    case NPC_NEUTER:
		pf_buffer_string (filter,
				"I'm not sure it would appreciate that!\n");
		break;

	    default:
		sc_error ("lib_cmd_kiss_npc: unknown gender, %ld\n", gender);
	    }
	return TRUE;
}

sc_bool
lib_cmd_kiss_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "kiss", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_buffer_string (filter, "I'm not sure ");
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, " would appreciate that.\n");
	return TRUE;
}

sc_bool
lib_cmd_kiss_other (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	/* Reject this attempt. */
	pf_buffer_string (filter, "I'm not sure it would appreciate that.\n");
	return TRUE;
}


/*
 * lib_cmd_feed_npc()
 * lib_cmd_feed_object()
 * lib_cmd_feed_other()
 *
 * Standard responses to attempts to feed something.
 */
sc_bool
lib_cmd_feed_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		npc;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "feed", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, " is not hungry.\n");
	return TRUE;
}

sc_bool
lib_cmd_feed_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "feed", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_buffer_string (filter, "It's not hungry.\n");
	return TRUE;
}

sc_bool
lib_cmd_feed_other (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "There is nothing worth feeding here.\n");
	return TRUE;
}


/*
 * lib_cmd_break_npc()
 * lib_cmd_break_object()
 *
 * Standard responses to attempts to break something.
 */
sc_bool
lib_cmd_break_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		npc;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "break", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_buffer_string (filter, "I don't think ");
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, " would appreciate that.\n");
	return TRUE;
}

sc_bool
lib_cmd_break_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "break", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You might need ",
				"I might need ",
				"%player% might need "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_smell_npc()
 * lib_cmd_smell_object()
 *
 * Standard responses to attempts to smell something.
 */
sc_bool
lib_cmd_smell_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		npc;
	sc_bool			is_ambiguous;

	/* Get the referenced npc, and if none, consider complete. */
	npc = lib_disambiguate_npc (gamestate, "smell", &is_ambiguous);
	if (npc == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_new_sentence (filter);
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, " smells normal.\n");
	return TRUE;
}

sc_bool
lib_cmd_smell_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "smell", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_new_sentence (filter);
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, " smells normal.\n");
	return TRUE;
}


/*
 * lib_cmd_sell_object()
 * lib_cmd_sell_other()
 *
 * Standard responses to attempts to sell something.
 */
sc_bool
lib_cmd_sell_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "sell", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Reject this attempt. */
	pf_buffer_string (filter, "No-one is interested in buying ");
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}

sc_bool
lib_cmd_sell_other (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "No-one is interested in buying.\n");
	return TRUE;
}


/*
 * lib_cmd_eat_object()
 *
 * Consume edible objects.
 */
sc_bool
lib_cmd_eat_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_int32		object;
	sc_bool			edible;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "eat", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Check that we have the object to eat. */
	if (gs_get_object_position (gamestate, object) != OBJ_HELD_PLAYER)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are not holding ",
					"I am not holding ",
					"%player% is not holding "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Check for static object moved to player by event. */
	if (obj_is_static (gamestate, object))
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't eat ",
					"I can't eat ",
					"%player% can't eat "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Is this object inedible? */
	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Edible";
	edible = prop_get_boolean (bundle, "B<-sis", vt_key);
	if (!edible)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You can't eat ",
					"I can't eat ",
					"%player% can't eat "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
		return TRUE;
	    }

	/* Confirm, and hide the object. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You eat ",
				"I eat ",
				"%player% eats "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter,
			".  Not bad, but it could do with a pinch of salt!\n");
	gs_object_make_hidden (gamestate, object);
	return TRUE;
}


/* Enumerated sit/stand/lie types. */
enum {			OBJ_STANDABLE_MASK	= 0x01,
			OBJ_LIEABLE_MASK	= 0x02 };
typedef enum {		MOVE_SIT,	MOVE_SIT_FLOOR,
			MOVE_STAND,	MOVE_STAND_FLOOR,
			MOVE_LIE,	MOVE_LIE_FLOOR
} sc_movement_t;

/*
 * lib_stand_sit_lie()
 *
 * Central handler for stand, sit, and lie commands.
 */
static sc_bool
lib_stand_sit_lie (sc_gamestateref_t gamestate, sc_movement_t movement)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		object;
	sc_char			*already_doing, *okay;
	sc_int32		position;

	/* Unnecessary initializations to keep GCC happy. */
	object = -1;	already_doing = FALSE;
	okay = FALSE;	position = 0;

	/* Get a target object for movement, -1 if floor. */
	switch (movement)
	    {
	    case MOVE_STAND:
	    case MOVE_SIT:
	    case MOVE_LIE:
		{
		sc_char		*disambiguate, *cant_do;
		sc_int32	sitlie, mask;
		sc_vartype_t	vt_key[3];
		sc_bool		is_ambiguous;

		/* Unnecessary initializations to keep GCC happy. */
		disambiguate = NULL;	cant_do = NULL;
		mask = 0;

		/* Set disambiguation and not amenable messages. */
		switch (movement)
		    {
		    case MOVE_STAND:
			disambiguate = "stand on";
			cant_do = lib_select_response (gamestate,
				"That's not something you can stand on.\n",
				"That's not something I can stand on.\n",
			       "That's not something %player% can stand on.\n");
			mask = OBJ_STANDABLE_MASK;
			break;
		    case MOVE_SIT:
			disambiguate = "sit on";
			cant_do = lib_select_response (gamestate,
				"That's not something you can sit on.\n",
				"That's not something I can sit on.\n",
				"That's not something %player% can sit on.\n");
			mask = OBJ_STANDABLE_MASK;
			break;
		    case MOVE_LIE:
			disambiguate = "lie on";
			cant_do = lib_select_response (gamestate,
				"That's not something you can lie on.\n",
				"That's not something I can lie on.\n",
				"That's not something %player% can lie on.\n");
			mask = OBJ_LIEABLE_MASK;
			break;
		    default:
			sc_fatal ("lib_sit_stand_lie: movement error, %d\n",
								movement);
		    }

		/* Get the referenced object; if none, consider complete. */
		object = lib_disambiguate_object (gamestate,
						disambiguate, &is_ambiguous);
		if (object == -1)
			return is_ambiguous;

		/* Verify the referenced object is amenable. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "SitLie";
		sitlie = prop_get_integer (bundle, "I<-sis", vt_key);
		if (!(sitlie & mask))
		    {
			pf_buffer_string (filter, cant_do);
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
		sc_fatal ("lib_sit_stand_lie: movement error, %d\n", movement);
	    }

	/* Set up confirmation messages and position. */
	switch (movement)
	    {
	    case MOVE_STAND:
		already_doing = lib_select_response (gamestate,
				"You are already standing on ",
				"I am already standing on ",
				"%player% is already standing on ");
		okay = lib_select_response (gamestate,
				"You stand on ",
				"I stand on ",
				"%player% stands on ");
		position = 0;
		break;

	    case MOVE_STAND_FLOOR:
		already_doing = lib_select_response (gamestate,
				"You are already standing!\n",
				"I am already standing!\n",
				"%player% is already standing!\n");
		okay = lib_select_response (gamestate,
				"You stand up.\n",
				"I stand up.\n",
				"%player% stands up.\n");
		position = 0;
		break;

	    case MOVE_SIT:
		already_doing = lib_select_response (gamestate,
				"You are already sitting on ",
				"I am already sitting on ",
				"%player% is already sitting on ");
		okay = lib_select_response (gamestate,
				"You sit on ",
				"I sit on ",
				"%player% sits on ");
		position = 1;
		break;

	    case MOVE_SIT_FLOOR:
		already_doing = lib_select_response (gamestate,
				"You are already sitting down.\n",
				"I am already sitting down.\n",
				"%player% is already sitting down.\n");
		okay = lib_select_response (gamestate,
				"You sit down on the ground.\n",
				"I sit down on the ground.\n",
				"%player% sits down on the ground.\n");
		position = 1;
		break;

	    case MOVE_LIE:
		already_doing = lib_select_response (gamestate,
				"You are already lying on ",
				"I am already lying on ",
				"%player% is already lying on ");
		okay = lib_select_response (gamestate,
				"You lie down on ",
				"I lie down on ",
				"%player% lies down on ");
		position = 2;
		break;

	    case MOVE_LIE_FLOOR:
		already_doing = lib_select_response (gamestate,
				"You are already lying on the floor.\n",
				"I am already lying on the floor.\n",
				"%player% is already lying on the floor.\n");
		okay = lib_select_response (gamestate,
				"You lie down on the floor.\n",
				"I lie down on the floor.\n",
				"%player% lies down on the floor.\n");
		position = 2;
		break;

	    default:
		sc_fatal ("lib_sit_stand_lie: movement error, %d\n", movement);
	    }

	/* See if already doing this. */
	if (gamestate->playerposition == position
			&& gamestate->playerparent == object)
	    {
		pf_buffer_string (filter, already_doing);
		if (object != -1)
		    {
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, ".\n");
		    }
		return TRUE;
	    }

	/* Adjust player position and parent. */
	gamestate->playerposition = position;
	gamestate->playerparent = object;
	pf_buffer_string (filter, okay);
	if (object != -1)
	    {
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, ".\n");
	    }
	return TRUE;
}


/*
 * lib_cmd_stand_*
 * lib_cmd_sit_*
 * lib_cmd_lie_*
 *
 * Stand, sit, or lie on an object, or on the floor.
 */
sc_bool lib_cmd_stand_on_object (sc_gamestateref_t gamestate)
		{ return lib_stand_sit_lie (gamestate, MOVE_STAND); }
sc_bool lib_cmd_stand_on_floor (sc_gamestateref_t gamestate)
		{ return lib_stand_sit_lie (gamestate, MOVE_STAND_FLOOR); }
sc_bool lib_cmd_sit_on_object (sc_gamestateref_t gamestate)
		{ return lib_stand_sit_lie (gamestate, MOVE_SIT); }
sc_bool lib_cmd_sit_on_floor (sc_gamestateref_t gamestate)
		{ return lib_stand_sit_lie (gamestate, MOVE_SIT_FLOOR); }
sc_bool lib_cmd_lie_on_object (sc_gamestateref_t gamestate)
		{ return lib_stand_sit_lie (gamestate, MOVE_LIE); }
sc_bool lib_cmd_lie_on_floor (sc_gamestateref_t gamestate)
		{ return lib_stand_sit_lie (gamestate, MOVE_LIE_FLOOR); }


/*
 * lib_cmd_save()
 * lib_cmd_restore()
 *
 * Save/restore a gamestate.
 */
sc_bool
lib_cmd_save (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	if (if_confirm (SC_CONF_SAVE))
	    {
		if (ser_save_game (gamestate))
			pf_buffer_string (filter, "Ok.\n");
		else
			pf_buffer_string (filter, "Save failed.\n");
	    }

	gamestate->is_admin = TRUE;
	return TRUE;
}

sc_bool
lib_cmd_restore (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	if (if_confirm (SC_CONF_RESTORE))
	    {
		if (ser_load_game (gamestate))
		    {
			pf_buffer_string (filter, "Ok.\n");
			gamestate->is_running = FALSE;
			gamestate->do_restore = TRUE;
		    }
		else
			pf_buffer_string (filter, "Restore failed.\n");
	    }

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_locate_object()
 * lib_cmd_locate_npc()
 *
 * Display the location of a selected object, and selected NPC.
 */
sc_bool
lib_cmd_locate_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		index, count, object, room;
	sc_int32		position, parent;

	gamestate->is_admin = TRUE;

	/*
	 * Filter to remove unseen object references.  Note that this is
	 * different from NPCs, who we acknowledge even when unseen.
	 */
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (!gs_get_object_seen (gamestate, index))
			gamestate->object_references[index] = FALSE;
	    }

	/* Count the number of objects referenced by the last command. */
	count = 0; object = -1;
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (gamestate->object_references[index])
		    {
			count++;
			object = index;
		    }
	    }

	/*
	 * If no objects identified, be coy about revealing anything; if
	 * more than one, be vague.
	 */
	if (count == 0)
	    {
		pf_buffer_string (filter, "I don't know where that is.\n");
		return TRUE;
	    }
	else if (count > 1)
	    {
		pf_buffer_string (filter, "Please be more clear about"
					" what you want to locate.\n");
		return TRUE;
	    }

	/*
	 * The reference is unambiguous, so we're responsible for noting
	 * it for future use as "it", and setting it in variables.  Disam-
	 * biguation would normally do this for us, but we just bypassed it.
	 */
	var_set_ref_object (vars, object);
	lib_set_pronoun_object (gamestate, object);

	/* See if we can print a message based on position and parent. */
	position = gs_get_object_position (gamestate, object);
	parent = gs_get_object_parent (gamestate, object);
	switch (position)
	    {
	    case OBJ_HIDDEN:
		if (!obj_is_static (gamestate, object))
		    {
			pf_buffer_string (filter,
					"I don't know where that is.\n");
			return TRUE;
		    }
		break;

	    case OBJ_HELD_PLAYER:
		pf_new_sentence (filter);
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are carrying ",
					"I am carrying ",
					"%player% is carrying "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;

	    case OBJ_WORN_PLAYER:
		pf_new_sentence (filter);
		pf_buffer_string (filter, lib_select_response (gamestate,
					"You are wearing ",
					"I am wearing ",
					"%player% is wearing "));
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, "!\n");
		return TRUE;

	    case OBJ_HELD_NPC:
	    case OBJ_WORN_NPC:
		if (gamestate->npcs[parent].seen)
		    {
			pf_new_sentence (filter);
			lib_print_npc_np (gamestate, parent);
			pf_buffer_string (filter, (position == OBJ_HELD_NPC)
					? " is holding " : " is wearing ");
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, ".\n");
		    }
		else
		    {
			pf_buffer_string (filter,
					"I don't know where that is.\n");
		    }
		return TRUE;

	    case OBJ_PART_NPC:
		if (parent == -1)
		    {
			pf_new_sentence (filter);
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter,
					lib_select_response (gamestate,
						" is a part of you!\n",
						" is a part of me!\n",
						" is a part of %player%!\n"));
		    }
		else
		    {
			if (gamestate->npcs[parent].seen)
			    {
				pf_new_sentence (filter);
				lib_print_object_np (gamestate, object);
				pf_buffer_string (filter, " is a part of ");
				lib_print_npc_np (gamestate, parent);
				pf_buffer_string (filter, ".\n");
			    }
			else
			    {
				pf_buffer_string (filter,
					"I don't know where that is.\n");
			    }
		    }
		return TRUE;

	    case OBJ_ON_OBJECT:
	    case OBJ_IN_OBJECT:
		if (gs_get_object_seen (gamestate, parent))
		    {
			pf_new_sentence (filter);
			lib_print_object_np (gamestate, object);
			pf_buffer_string (filter, (position == OBJ_ON_OBJECT)
					? " is on " : " is inside ");
			lib_print_object_np (gamestate, parent);
			pf_buffer_string (filter, ".\n");
		    }
		else
		    {
			pf_buffer_string (filter,
					"I don't know where that is.\n");
		    }
		return TRUE;
	    }

	/*
	 * Object is either static unmoved, or dynamic and on the floor of a
	 * room.  Check each room for the object, stopping on first found.
	 */
	for (room = 0; room < gamestate->room_count; room++)
	    {
		if (obj_indirectly_in_room (gamestate, object, room))
			break;
	    }
	if (room == gamestate->room_count)
	    {
		pf_buffer_string (filter, "I don't know where that is.\n");
		return TRUE;
	    }

	/* Check that this room's been visited by the player. */
	if (!gs_get_room_seen (gamestate, room))
	    {
		pf_new_sentence (filter);
		lib_print_object_np (gamestate, object);
		pf_buffer_string (filter, lib_select_response (gamestate,
			" is somewhere that you haven't been yet.\n",
			" is somewhere that I haven't been yet.\n",
			" is somewhere that %player% hasn't been yet.\n"));
		return TRUE;
	    }

	/* Print the details of the object's room. */
	pf_new_sentence (filter);
	pf_buffer_string (filter, lib_get_room_name (gamestate, room));
	pf_buffer_string (filter, ".\n");
	return TRUE;
}

sc_bool
lib_cmd_locate_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		index, count, npc, room;

	gamestate->is_admin = TRUE;

	/* Count the number of NPCs referenced by the last command. */
	count = 0; npc = -1;
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		if (gamestate->npc_references[index])
		    {
			count++;
			npc = index;
		    }
	    }

	/*
	 * If no NPCs identified, be coy about revealing anything; if
	 * more than one, be vague.  The "... where that is..." is the
	 * correct message even for NPCs -- it's the same response as
	 * for lib_locate_other().
	 */
	if (count == 0)
	    {
		pf_buffer_string (filter, "I don't know where that is.\n");
		return TRUE;
	    }
	else if (count > 1)
	    {
		pf_buffer_string (filter, "Please be more clear about"
					" who you want to locate.\n");
		return TRUE;
	    }

	/*
	 * The reference is unambiguous, so we're responsible for noting
	 * it for future use as "him", "her", or "it", and setting it in
	 * variables.  Disambiguation would normally do this for us, but
	 * we just bypassed it.
	 */
	var_set_ref_character (vars, npc);
	lib_set_pronoun_npc (gamestate, npc);

	/* See if this NPC has been seen yet. */
	if (!gamestate->npcs[npc].seen)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You haven't seen ",
				"I haven't seen ",
				"%player% hasn't seen "));
		lib_print_npc_np (gamestate, npc);
		pf_buffer_string (filter, " yet!\n");
		return TRUE;
	    }

	/* Check each room for the NPC, stopping on first found. */
	for (room = 0; room < gamestate->room_count; room++)
	    {
		if (npc_in_room (gamestate, npc, room))
			break;
	    }
	if (room == gamestate->room_count)
	    {
		pf_buffer_string (filter, "I don't know where ");
		lib_print_npc_np (gamestate, npc);
		pf_buffer_string (filter, " is.\n");
		return TRUE;
	    }

	/* Check that this room's been visited by the player. */
	if (!gs_get_room_seen (gamestate, room))
	    {
		lib_print_npc_np (gamestate, npc);
		pf_buffer_string (filter, lib_select_response (gamestate,
			" is somewhere that you haven't been yet.\n",
			" is somewhere that I haven't been yet.\n",
			" is somewhere that %player% hasn't been yet.\n"));
		return TRUE;
	    }

	/* Print the location, and smart-alec response. */
	pf_new_sentence (filter);
	pf_buffer_string (filter, lib_get_room_name (gamestate, room));
#if 0
	if (room == gamestate->playerroom)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
			"  (Right next to you, silly!)",
			"  (Right next to me, silly!)",
			"  (Right next to %player%, silly!)"));
	    }
#endif
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_turns()
 * lib_cmd_score()
 *
 * Display turns taken and score so far.
 */
sc_bool
lib_cmd_turns (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_char			buffer[12];

	pf_buffer_string (filter, "You have taken ");
	sprintf (buffer, "%ld", gamestate->turns);
	pf_buffer_string (filter, buffer);
	if (gamestate->turns == 1)
		pf_buffer_string (filter, " turn so far.\n");
	else
		pf_buffer_string (filter, " turns so far.\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}
sc_bool
lib_cmd_score (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_int32		max_score, percent;
	sc_char			buffer[12];

	/* Get max score, and calculate score as a percentage. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "MaxScore";
	max_score = prop_get_integer (bundle, "I<-ss", vt_key);
	if (gamestate->score > 0
			&& max_score > 0)
		percent = (gamestate->score * 100) / max_score;
	else
		percent = 0;

	/* Output carefully formatted response. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"Your score is ",
				"My score is ",
				"%player%'s score is "));
	sprintf (buffer, "%ld", gamestate->score);
	pf_buffer_string (filter, buffer);
	pf_buffer_string (filter, " out of a maximum of ");
	sprintf (buffer, "%ld", max_score);
	pf_buffer_string (filter, buffer);
	pf_buffer_string (filter, ".  (");
	sprintf (buffer, "%ld", percent);
	pf_buffer_string (filter, buffer);
	pf_buffer_string (filter, "%)\n");

	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Standard response commands.  These are uninteresting catch-all cases,
 * but it's good to make then right as game ALRs may look for them.
 */
sc_bool
lib_cmd_profanity (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "I really don't think there's any need"
					" for language like that!\n");
	return TRUE;
}
sc_bool
lib_cmd_examine_other (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"You see no such thing.\n",
				"I see no such thing.\n",
				"%player% sees no such thing.\n"));
	return TRUE;
}
sc_bool
lib_cmd_locate_other (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "I don't know where that is!\n");
	gamestate->is_admin = TRUE;
	return TRUE;
}
sc_bool
lib_cmd_unix_like (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "This isn't Unix you know!\n");
	return TRUE;
}
sc_bool
lib_cmd_dos_like (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "This isn't Dos you know!\n");
	return TRUE;
}
sc_bool
lib_cmd_buy (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "Nothing is for sale.\n");
	return TRUE;
}
sc_bool
lib_cmd_cry (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "There's no need for that!\n");
	return TRUE;
}
sc_bool
lib_cmd_dance (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"You do a little dance.\n",
				"I do a little dance.\n",
				"%player% does a little dance.\n"));
	return TRUE;
}
sc_bool
lib_cmd_feel (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
			"You feel nothing out of the ordinary.\n",
			"I feel nothing out of the ordinary.\n",
			"%player% feels nothing out of the ordinary.\n"));
	return TRUE;
}
sc_bool
lib_cmd_fly (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't fly.\n",
				"I can't fly.\n",
				"%player% can't fly.\n"));
	return TRUE;
}
sc_bool
lib_cmd_hint (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "You're just going to have to work it"
					" out for yourself...\n");
	return TRUE;
}
sc_bool
lib_cmd_hum (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"You hum a little tune.\n",
				"I hum a little tune.\n",
				"%player% hums a little tune.\n"));
	return TRUE;
}
sc_bool
lib_cmd_jump (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "Wheee-boinng.\n");
	return TRUE;
}
sc_bool
lib_cmd_listen (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
			"You hear nothing out of the ordinary.\n",
			"I hear nothing out of the ordinary.\n",
			"%player% hears nothing out of the ordinary.\n"));
	return TRUE;
}
sc_bool
lib_cmd_please (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"Your kindness gets you nowhere.\n",
				"My kindness gets me nowhere.\n",
				"%player%'s kindness gets nowhere.\n"));
	return TRUE;
}
sc_bool
lib_cmd_punch (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "Who do you think you are, Mike Tyson?\n");
	return TRUE;
}
sc_bool
lib_cmd_run (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"Why would you want to run?\n",
				"Why would I want to run?\n",
				"Why would %player% want to run?\n"));
	return TRUE;
}
sc_bool
lib_cmd_shout (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "Aaarrrrgggghhhhhh!\n");
	return TRUE;
}
sc_bool
lib_cmd_say (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_char			*string = NULL;

	switch (sc_randomint (1, 5))
	    {
	    case 1:	string = "Gosh, that was very impressive.\n";	break;
	    case 2:	string = lib_select_response (gamestate,
				 "Not surprisingly, no-one takes any notice"
						" of you.\n",
				 "Not surprisingly, no-one takes any notice"
						" of me.\n",
				 "Not surprisingly, no-one takes any notice"
						" of %player%.\n");	break;
	    case 3:	string = "Wow!  That achieved a lot.\n";	break;
	    case 4:	string = "Uh huh, yes, very interesting.\n";	break;
	    default:	string = "That's the most interesting thing I've ever"
						" heard!\n";		break;
	    }

	pf_buffer_string (filter, string);
	return TRUE;
}
sc_bool
lib_cmd_sing (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"You sing a little song.\n",
				"I sing a little song.\n",
				"%player% sings a little song.\n"));
	return TRUE;
}
sc_bool
lib_cmd_sleep (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "Zzzzz.  Bored are you?\n");
	return TRUE;
}
sc_bool
lib_cmd_talk (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"No-one listens to your rabblings.\n",
				"No-one listens to my rabblings.\n",
				"No-one listens to %player%'s rabblings.\n"));
	return TRUE;
}
sc_bool
lib_cmd_thank (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "You're welcome.\n");
	return TRUE;
}
sc_bool
lib_cmd_whistle (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, lib_select_response (gamestate,
				"You whistle a little tune.\n",
				"I whistle a little tune.\n",
				"%player% whistles a little tune.\n"));
	return TRUE;
}
sc_bool
lib_cmd_interrogation (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_char			*string = NULL;

	switch (sc_randomint (1, 17))
	    {
	    case 1:	string = "Why do you want to know?\n";		break;
	    case 2:	string = "Interesting question.\n";		break;
	    case 3:	string = "Let me think about that one...\n";	break;
	    case 4:	string = "I haven't a clue!\n";			break;
	    case 5:	string = "All these questions are hurting my head.\n";
									break;
	    case 6:	string = "I'm not going to tell you.\n";	break;
	    case 7:	string = "Someday I'll know the answer to that one.\n";
									break;
	    case 8:	string = "I could tell you, but then I'd have to"
							" kill you.\n";	break;
	    case 9:	string = "Ha, as if I'd tell you!\n";		break;
	    case 10:	string = "Ask me again later.\n";		break;
	    case 11:	string = "I don't know - could you ask anyone else?\n";
									break;
	    case 12:	string = "Err, yes?!?\n";			break;
	    case 13:	string = "Let me just check my memory banks...\n";
									break;
	    case 14:	string = "Because that's just the way it is.\n";break;
	    case 15:	string = "Do I ask you all sorts of awkward"
							" questions?\n";break;
	    case 16:	string = "Questions, questions...\n";		break;
	    default:	string = "Who cares.\n";			break;
	    }

	pf_buffer_string (filter, string);
	return TRUE;
}
sc_bool
lib_cmd_xyzzy (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "I'm sorry, but XYZZY doesn't do anything"
					" special in this game!\n");
	return TRUE;
}
sc_bool
lib_cmd_yes (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, "That's interesting, but it doesn't"
					" mean much.\n");
	return TRUE;
}


/*
 * lib_cmd_ask_object()
 *
 * Rhetorical question response.
 */
sc_bool
lib_cmd_ask_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Get the referenced object, and if none, consider complete. */
	object = lib_disambiguate_object (gamestate, "ask", &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* No reply. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You get no reply from ",
				"I get no reply from ",
				"%player% gets no reply from "));
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_nothing_happens()
 *
 * Central handler for a range of nothing-happens messages.  More
 * uninteresting responses.
 */
static sc_bool
lib_nothing_happens (sc_gamestateref_t gamestate, const sc_char *verb_general,
		const sc_char *verb_third_person, sc_bool just_wont)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_int32		perspective;
	const sc_char		*person, *verb;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Use person and verb tense according to perspective. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "Perspective";
	perspective = prop_get_integer (bundle, "I<-ss", vt_key);
	switch (perspective)
	    {
	    case FIRST_PERSON:
		person = "I ";
		verb = verb_general;
		break;
	    case SECOND_PERSON:
		person = "You ";
		verb = verb_general;
		break;
	    case THIRD_PERSON:
		person = "%player% ";
		verb = verb_third_person;
		break;
	    default:
		sc_error ("lib_nothing_happens:"
			" unknown perspective, %ld\n", perspective);
		person = "You ";
		verb = verb_general;
		break;
	    }

	/* Don't bother with disambiguation if asked not to. */
	if (just_wont)
	    {
		pf_buffer_string (filter, person);
		pf_buffer_string (filter, verb);
		pf_buffer_string (filter, ", but nothing happens.\n");
		return TRUE;
	    }

	/* Get the referenced object.  If none, return immediately. */
	object = lib_disambiguate_object (gamestate,
						verb_general, &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Nothing happens. */
	pf_buffer_string (filter, person);
	pf_buffer_string (filter, verb);
	pf_buffer_character (filter, ' ');
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ", but nothing happens.\n");
	return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Shake, rattle and roll, and assorted nothing-happens handlers.
 */
sc_bool lib_cmd_hit_object (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "hit", "hits", FALSE); }
sc_bool lib_cmd_kick_object (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "kick", "kicks", FALSE); }
sc_bool lib_cmd_press_object (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "press", "presses", FALSE); }
sc_bool lib_cmd_push_object (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "push", "pushes", FALSE); }
sc_bool lib_cmd_pull_object (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "pull", "pulls", FALSE); }
sc_bool lib_cmd_shake_object (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "shake", "shakes", FALSE); }
sc_bool lib_cmd_hit_other (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "hit", "hits", TRUE); }
sc_bool lib_cmd_kick_other (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "kick", "kicks", TRUE); }
sc_bool lib_cmd_press_other (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "press", "presses", TRUE); }
sc_bool lib_cmd_push_other (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "push", "pushes", TRUE); }
sc_bool lib_cmd_pull_other (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "pull", "pulls", TRUE); }
sc_bool lib_cmd_shake_other (sc_gamestateref_t gamestate)
	{ return lib_nothing_happens (gamestate, "shake", "shakes", TRUE); }


/*
 * lib_cant_do()
 *
 * Central handler for a range of can't-do messages.  Yet more uninterest-
 * ing responses.
 */
static sc_bool
lib_cant_do (sc_gamestateref_t gamestate, const sc_char *verb,
		sc_bool just_cant)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Don't bother with disambiguation if asked not to. */
	if (just_cant)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't ", "I can't ", "%player% can't "));
		pf_buffer_string (filter, verb);
		pf_buffer_string (filter, " that.\n");
		return TRUE;
	    }

	/* Get the referenced object.  If none, return immediately. */
	object = lib_disambiguate_object (gamestate, verb, &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Whatever it is, don't do it. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You can't ", "I can't ", "%player% can't "));
	pf_buffer_string (filter, verb);
	pf_buffer_character (filter, ' ');
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Assorted can't-do messages.
 */
sc_bool lib_cmd_block_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "block", FALSE); }
sc_bool lib_cmd_clean_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "clean", FALSE); }
sc_bool lib_cmd_climb_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "climb", FALSE); }
sc_bool lib_cmd_cut_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "cut", FALSE); }
sc_bool lib_cmd_light_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "light", FALSE); }
sc_bool lib_cmd_move_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "move", FALSE); }
sc_bool lib_cmd_suck_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "suck", FALSE); }
sc_bool lib_cmd_turn_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "turn", FALSE); }
sc_bool lib_cmd_unblock_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "unblock", FALSE); }
sc_bool lib_cmd_wash_object (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "wash", FALSE); }
sc_bool lib_cmd_block_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "block", TRUE); }
sc_bool lib_cmd_clean_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "clean", TRUE); }
sc_bool lib_cmd_climb_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "climb", TRUE); }
sc_bool lib_cmd_cut_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "cut", TRUE); }
sc_bool lib_cmd_light_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "light", TRUE); }
sc_bool lib_cmd_move_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "move", TRUE); }
sc_bool lib_cmd_suck_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "suck", TRUE); }
sc_bool lib_cmd_turn_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "turn", TRUE); }
sc_bool lib_cmd_unblock_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "unblock", TRUE); }
sc_bool lib_cmd_wash_other (sc_gamestateref_t gamestate)
		{ return lib_cant_do (gamestate, "wash", TRUE); }


/*
 * lib_dont_think()
 *
 * Central handler for a range of don't_think messages.  Still more
 * uninteresting responses.
 */
static sc_bool
lib_dont_think (sc_gamestateref_t gamestate, const sc_char *verb,
		sc_bool just_dont)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_int32		object;
	sc_bool			is_ambiguous;

	/* Don't bother with disambiguation if asked not to. */
	if (just_dont)
	    {
		pf_buffer_string (filter, lib_select_response (gamestate,
				"You don't think you can ",
				"I don't think I can ",
				"%player% doesn't they can "));
		pf_buffer_string (filter, verb);
		pf_buffer_string (filter, " that.\n");
		return TRUE;
	    }

	/* Get the referenced object.  If none, return immediately. */
	object = lib_disambiguate_object (gamestate, verb, &is_ambiguous);
	if (object == -1)
		return is_ambiguous;

	/* Whatever it is, don't do it. */
	pf_buffer_string (filter, lib_select_response (gamestate,
				"You don't think you can ",
				"I don't think I can ",
				"%player% doesn't they can "));
	pf_buffer_string (filter, verb);
	pf_buffer_character (filter, ' ');
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Assorted don't-think messages.
 */
sc_bool
lib_cmd_fix_object (sc_gamestateref_t gamestate)
		{ return lib_dont_think (gamestate, "fix", FALSE); }
sc_bool
lib_cmd_mend_object (sc_gamestateref_t gamestate)
		{ return lib_dont_think (gamestate, "mend", FALSE); }
sc_bool
lib_cmd_repair_object (sc_gamestateref_t gamestate)
		{ return lib_dont_think (gamestate, "repair", FALSE); }
sc_bool
lib_cmd_fix_other (sc_gamestateref_t gamestate)
		{ return lib_dont_think (gamestate, "fix", TRUE); }
sc_bool
lib_cmd_mend_other (sc_gamestateref_t gamestate)
		{ return lib_dont_think (gamestate, "mend", TRUE); }
sc_bool
lib_cmd_repair_other (sc_gamestateref_t gamestate)
		{ return lib_dont_think (gamestate, "repair", TRUE); }


/*
 * lib_what()
 *
 * Central handler for doing something, but unsure to what.
 */
static sc_bool
lib_what (sc_gamestateref_t gamestate, const sc_char *verb)
{
	sc_printfilterref_t	filter	= gamestate->filter;

	pf_buffer_string (filter, verb);
	pf_buffer_string (filter, " what?\n");
	return TRUE;
}


/*
 * lib_cmd_*()
 *
 * Assorted "what?" messages.
 */
sc_bool
lib_cmd_get_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Take"); }
sc_bool
lib_cmd_open_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Open"); }
sc_bool
lib_cmd_close_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Close"); }
sc_bool
lib_cmd_give_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Give"); }
sc_bool
lib_cmd_remove_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Remove"); }
sc_bool
lib_cmd_drop_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Drop"); }
sc_bool
lib_cmd_wear_other (sc_gamestateref_t gamestate)
		{ return lib_what (gamestate, "Wear"); }


/*
 * lib_cmd_verb_object()
 * lib_cmd_verb_character()
 *
 * Handlers for unrecognized verbs with known object/NPC.
 */
sc_bool
lib_cmd_verb_object (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		count, object, index;

	/* Ensure the reference is unambiguous. */
	count = 0; object = -1;
	for (index = 0; index < gamestate->object_count; index++)
	    {
		if (gamestate->object_references[index]
				&& gs_get_object_seen (gamestate, index)
				&& obj_indirectly_in_room (gamestate, index,
						gamestate->playerroom))
		    {
			count++;
			object = index;
		    }
	    }
	if (count != 1)
		return FALSE;

	/* Note as "it", and save in variables. */
	var_set_ref_object (vars, object);
	lib_set_pronoun_object (gamestate, object);

	/* Print don't understand message. */
	pf_buffer_string (filter,
			"I don't understand what you want me to do with ");
	lib_print_object_np (gamestate, object);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}

sc_bool
lib_cmd_verb_npc (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		count, npc, index;

	/* Ensure the reference is unambiguous. */
	count = 0; npc = -1;
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		if (gamestate->npc_references[index]
				&& gamestate->npcs[index].seen
				&& npc_in_room (gamestate, index,
						gamestate->playerroom))
		    {
			count++;
			npc = index;
		    }
	    }
	if (count != 1)
		return FALSE;

	/* Note as "him"/"her"/"it", and save in variables. */
	var_set_ref_character (vars, npc);
	lib_set_pronoun_npc (gamestate, npc);

	/* Print don't understand message. */
	pf_buffer_string (filter,
			"I don't understand what you want me to do with ");
	lib_print_npc_np (gamestate, npc);
	pf_buffer_string (filter, ".\n");
	return TRUE;
}


/*
 * lib_debug_trace()
 *
 * Set library tracing on/off.
 */
void
lib_debug_trace (sc_bool flag)
{
	lib_trace = flag;
}

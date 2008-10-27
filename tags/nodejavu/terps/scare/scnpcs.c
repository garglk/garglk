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

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */

/* Trace flag, set before running. */
static sc_bool		npc_trace		= FALSE;


/*
 * npc_in_room()
 *
 * Return TRUE if a given NPC is currently in a given room.
 */
sc_bool
npc_in_room (sc_gamestateref_t gamestate, sc_int32 npc, sc_int32 room)
{
	if (npc_trace)
	    {
		sc_trace ("NPC: checking NPC %ld in room %ld (NPC is in %ld)\n",
				npc, room, gamestate->npcs[npc].location);
	    }
	return gamestate->npcs[npc].location - 1 == room;
}


/*
 * npc_count_in_room()
 *
 * Return the count of characters in the room, including the player.
 */
sc_int32
npc_count_in_room (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_int32	count, npc;

	/* Start with the player. */
	count = (gamestate->playerroom == room) ? 1 : 0;

	/* Total up other NPCs inhabiting the room. */
	for (npc = 0; npc < gamestate->npc_count; npc++)
	    {
		if (gamestate->npcs[npc].location - 1 == room)
			count++;
	    }
	return count;
}


/*
 * npc_start_npc_walk()
 *
 * Start the given walk for the given NPC.
 */
void
npc_start_npc_walk (sc_gamestateref_t gamestate, sc_int32 npc, sc_int32 walk)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[6];
	sc_int32		movetime;

	/* Retrieve movetime 0 for the NPC walk. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Walks";
	vt_key[3].integer = walk;
	vt_key[4].string = "MoveTimes";
	vt_key[5].integer = 0;
	movetime = prop_get_integer (bundle, "I<-sisisi", vt_key) + 1;

	/* Set up walkstep. */
	gamestate->npcs[npc].walksteps[walk] = movetime;
}


/*
 * npc_turn_update()
 * npc_setup_initial()
 *
 * Set initial values for NPC states, and update on turns.
 */
void
npc_turn_update (sc_gamestateref_t gamestate)
{
	sc_int32	index;

	/* Set current values for NPC seen states. */
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		if (npc_in_room (gamestate, index,
						gamestate->playerroom))
			gamestate->npcs[index].seen = TRUE;
	    }
}

void
npc_setup_initial (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_int32		index;

	/* Start any walks that do not depend on a StartTask */
	vt_key[0].string = "NPCs";
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		sc_int32	walk_count, walk;

		/* Get NPC walk details. */
		vt_key[1].integer = index;
		vt_key[2].string = "Walks";
		walk_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
		for (walk = 0; walk < walk_count; walk++)
		    {
			sc_int32        starttask;

			/* If StartTask is zero, start walk at game start. */
			vt_key[3].integer = walk;
			vt_key[4].string = "StartTask";
			starttask = prop_get_integer
						(bundle, "I<-sisis", vt_key);
			if (starttask == 0)
				npc_start_npc_walk (gamestate, index, walk);
		    }
	    }

	/* Update seen flags for initial states. */
	npc_turn_update (gamestate);
}


/*
 * npc_room_in_roomgroup()
 *
 * Return TRUE if a given room is in a given group.
 */
static sc_bool
npc_room_in_roomgroup (sc_gamestateref_t gamestate,
		sc_int32 room, sc_int32 group)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_int32		member;

	/* Check roomgroup membership. */
	vt_key[0].string = "RoomGroups";
	vt_key[1].integer = group;
	vt_key[2].string = "List";
	vt_key[3].integer = room;
	member = prop_get_integer (bundle, "I<-sisi", vt_key);
	return member != 0;
}


/* List of direction names, for printing entry/exit messages. */
static sc_char  *DIRNAMES_4[] = {
	"the north", "the east", "the south", "the west", "above", "below",
	"inside", "outside",
	NULL
};
static sc_char  *DIRNAMES_8[] = {
	"the north", "the east", "the south", "the west", "above", "below",
	"inside", "outside",
	"the northeast", "the southeast", "the southwest", "the northwest",
	NULL
};

/*
 * npc_random_adjacent_roomgroup_member()
 *
 * Return a random member of group adjacent to given room.
 */
static sc_int32
npc_random_adjacent_roomgroup_member (sc_gamestateref_t gamestate,
		sc_int32 room, sc_int32 group)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5];
	sc_int32		roomlist[12], count;
	sc_uint16		length, index;

	/* If given room is "hidden", return nothing. */
	if (room == -1)
		return -1;

	/* How many exits to consider? */
	vt_key[0].string = "Globals";
	vt_key[1].string = "EightPointCompass";
	if (prop_get_boolean (bundle, "B<-ss", vt_key))
		length = sizeof (DIRNAMES_8) / sizeof (DIRNAMES_8[0]) - 1;
	else
		length = sizeof (DIRNAMES_4) / sizeof (DIRNAMES_4[0]) - 1;

	/* Poll adjacent rooms. */
	vt_key[0].string = "Rooms";
	vt_key[1].integer = room;
	vt_key[2].string = "Exits";
	count = 0;
	for (index = 0; index < length; index++)
	    {
		sc_vartype_t	vt_rvalue;
		sc_int32	aroom;

		vt_key[3].integer = index;
		vt_key[4].string = "Dest";
		aroom = (prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
		if (aroom > 0
			&& npc_room_in_roomgroup (gamestate, aroom - 1, group))
		    {
			roomlist[count] = aroom - 1;
			count++;
		    }
	    }

	if (count > 0)
		return roomlist[sc_randomint (0, count - 1)];
	return -1;
}


/*
 * npc_announce()
 *
 * Helper for npc_tick_npc().
 */
static void
npc_announce (sc_gamestateref_t gamestate, sc_int32 npc,
		sc_int32 room, sc_bool is_exit, sc_int32 npc_room)

{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[5], vt_rvalue;
	sc_char			*text, *name;
	sc_char			**dirnames;
	sc_uint16		dir, dir_match;
	sc_bool			showenterexit, found;

	/* If no announcement required, return immediately. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "ShowEnterExit";
	showenterexit = prop_get_boolean (bundle, "B<-sis", vt_key);
	if (!showenterexit)
		return;

	/* Get exit or entry text, and NPC name. */
	if (is_exit)
		vt_key[2].string = "ExitText";
	else
		vt_key[2].string = "EnterText";
	text = prop_get_string (bundle, "S<-sis", vt_key);
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);

	/* Decide on four or eight point compass names list. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "EightPointCompass";
	if (prop_get_boolean (bundle, "B<-ss", vt_key))
		dirnames = DIRNAMES_8;
	else
		dirnames = DIRNAMES_4;

	/* Set invariant key for room exit search. */
	vt_key[0].string = "Rooms";
	vt_key[1].integer = room;
	vt_key[2].string = "Exits";

	/* Find the room exit that matches the NPC room. */
	found = FALSE; dir_match = 0;
	for (dir = 0; dirnames[dir] != NULL; dir++)
	    {
		vt_key[3].integer = dir;
		if (prop_get (bundle, "I<-sisi", &vt_rvalue, vt_key))
		    {
			sc_int32	dest;

			/* Get room's direction destination, and compare. */
			vt_key[4].string = "Dest";
			dest = prop_get_integer
					(bundle, "I<-sisis", vt_key) - 1;
			if (dest == npc_room)
			    {
				dir_match = dir;
				found = TRUE;
				break;
			    }
		    }
	    }

	/* Print NPC exit/entry details. */
	pf_buffer_character (filter, '\n');
	pf_new_sentence (filter);
	pf_buffer_string (filter, name);
	pf_buffer_character (filter, ' ');
	pf_buffer_string (filter, text);
	if (found)
	    {
		pf_buffer_string (filter, is_exit ? " to " : " from ");
		pf_buffer_string (filter, dirnames[dir_match]);
	    }
	pf_buffer_string (filter, ".\n");

	/* Handle any associated resource. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Res";
	vt_key[3].integer = is_exit ? 3 : 2;
	res_handle_resource (gamestate, "sisi", vt_key);
}


/*
 * npc_tick_npc_walk()
 *
 * Helper for npc_tick_npc().
 */
static void
npc_tick_npc_walk (sc_gamestateref_t gamestate, sc_int32 npc, sc_int32 walk)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[6], vt_rvalue;
	sc_int32		roomgroups;
	sc_int32		movetimes, walkstep;
	sc_int32		start, dest, destnum;
	sc_int32		chartask, objecttask;

	/* Count roomgroups for later use. */
	vt_key[0].string = "RoomGroups";
	roomgroups = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;

	/* Get move times array length. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Walks";
	vt_key[3].integer = walk;
	vt_key[4].string = "MoveTimes";
	movetimes = (prop_get (bundle, "I<-sisis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;

	/* Find a step to match the movetime. */
	for (walkstep = 0; walkstep < movetimes - 1; walkstep++)
	    {
		sc_int32	movetime;

		vt_key[5].integer = walkstep + 1;
		movetime = prop_get_integer (bundle, "I<-sisisi", vt_key);
		if (gamestate->npcs[npc].walksteps[walk] > movetime)
			break;
	    }

	/* Sort out a destination. */
	start = gamestate->npcs[npc].location - 1;
	dest = gamestate->npcs[npc].location - 1;

	vt_key[4].string = "Rooms";
	vt_key[5].integer = walkstep;
	destnum = prop_get_integer (bundle, "I<-sisisi", vt_key);

	if (destnum == 0)	/* Hidden. */
		dest = -1;
	else if (destnum == 1)	/* Follow player. */
		dest = gamestate->playerroom;
	else if (destnum < gamestate->room_count + 2)
		dest = destnum - 2;
	else if (destnum < gamestate->room_count + 2 + roomgroups)
	    {
		sc_int32	group;

		group = destnum - 2 - gamestate->room_count;
		dest = npc_random_adjacent_roomgroup_member
						(gamestate, start, group);
		if (dest == -1)
			dest = lib_random_roomgroup_member (gamestate, group);
	    }

	/* Move NPC to destination. */
	gamestate->npcs[npc].location = dest + 1;

	if (npc_trace)
		sc_trace ("NPC: walking NPC %ld moved to %ld\n", npc, dest);

	/* If the NPC actually moved, handle some post-movement stuff. */
	if (start != dest)
	    {
		/* Handle announcing NPC movements. */
		if (start == gamestate->playerroom)
			npc_announce (gamestate, npc, start, TRUE, dest);
		else if (dest == gamestate->playerroom)
			npc_announce (gamestate, npc, dest, FALSE, start);
	    }

	/* Handle meeting characters and objects. */
	vt_key[4].string = "CharTask";
	chartask = prop_get_integer (bundle, "I<-sisis", vt_key);
	if (chartask != 0)
	    {
		sc_int32	meetchar;

		/* Run meetchar task if appropriate. */
		vt_key[4].string = "MeetChar";
		meetchar = prop_get_integer (bundle, "I<-sisis", vt_key);
		if ((meetchar == 0
				&& dest == gamestate->playerroom)
			|| (meetchar > 0
				&& dest == gamestate->npcs
					[meetchar - 1].location - 1))
		    {
			task_run_task (gamestate, chartask - 1, TRUE);
		    }
	    }

	vt_key[4].string = "ObjectTask";
	objecttask = prop_get_integer (bundle, "I<-sisis", vt_key);
	if (objecttask != 0)
	    {
		sc_int32	meetobject;

		/* Run meetobject task if appropriate. */
		vt_key[4].string = "MeetObject";
		meetobject = prop_get_integer (bundle, "I<-sisis", vt_key);
		if (obj_directly_in_room (gamestate, meetobject, dest))
		    {
			task_run_task (gamestate, objecttask - 1, TRUE);
		    }
	    }
}


/*
 * npc_tick_npc()
 *
 * Move an NPC one step along current walk.
 */
static void
npc_tick_npc (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[6], vt_rvalue;
	sc_int32		walk_count, walk;
	sc_bool			has_moved = FALSE;

	/* Get steps in the NPC walk. */
	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Walks";
	walk_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;
	for (walk = walk_count - 1; walk >= 0; walk--)
	    {
		sc_int32	starttask, stoppingtask;

		/* Ignore finished walks. */
		if (gamestate->npcs[npc].walksteps[walk] <= 0)
			continue;

		/* Get start and stopping tasks. */
		vt_key[3].integer = walk;
		vt_key[4].string = "StartTask";
		starttask = prop_get_integer
					(bundle, "I<-sisis", vt_key) - 1;
		vt_key[4].string = "StoppingTask";
		stoppingtask = prop_get_integer
					(bundle, "I<-sisis", vt_key) - 1;

		/* Check tasks. */
		if ((starttask >= 0 &&
				!gs_get_task_done (gamestate, starttask))
			|| (stoppingtask >= 0 &&
				 gs_get_task_done (gamestate, stoppingtask)))
		    {
			gamestate->npcs[npc].walksteps[walk] = -1;
			if (npc_trace)
			    {
				sc_trace ("NPC: stopped NPC %ld walk,"
							" tasks wrong\n", npc);
			    }
			continue;
		    }

		/* Decrement steps. */
		gamestate->npcs[npc].walksteps[walk]--;

		/* If we just hit a walk end, loop if called for. */
		if (gamestate->npcs[npc].walksteps[walk] == 0)
		    {
			sc_bool		is_loop;

			/* If walk is a loop, restart it. */
			vt_key[4].string = "Loop";
			is_loop = prop_get_boolean (bundle, "B<-sisis", vt_key);
			if (is_loop)
			    {
				vt_key[4].string = "MoveTimes";
				vt_key[5].integer = 0;
				gamestate->npcs[npc].walksteps[walk] =
						prop_get_integer (bundle,
							"I<-sisisi", vt_key);
			    }
			else
				/* Not a loop, complete it. */
				gamestate->npcs[npc].walksteps[walk] = -1;
		    }

		/*
		 * If not yet made a move on this walk, make one, and once
		 * made, make no other
		 */
		if (!has_moved)
		    {
			npc_tick_npc_walk (gamestate, npc, walk);
			has_moved = TRUE;
		    }
	    }
}


/*
 * npc_tick_npcs()
 *
 * Move each NPC one step along current walk.
 */
void
npc_tick_npcs (sc_gamestateref_t gamestate)
{
	sc_int32	npc;

	/* Iterate over NPCs. */
	for (npc = 0; npc < gamestate->npc_count; npc++)
		npc_tick_npc (gamestate, npc);
}


/*
 * npc_debug_trace()
 *
 * Set NPC tracing on/off.
 */
void
npc_debug_trace (sc_bool flag)
{
	npc_trace = flag;
}

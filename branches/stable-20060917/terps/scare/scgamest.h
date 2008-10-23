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

#include <setjmp.h>

#include "scare.h"

#ifndef GAMESTATE_H
#define GAMESTATE_H

/* Room state structure, tracks rooms visited by the player. */
typedef struct sc_roomstate_s {
	sc_bool		visited;
} sc_roomstate_t;

/*
 * Object state structure, tracks object movement, position, parent,
 * openness for openable objects, state for stateful objects, and whether
 * seen or not by the player.  The enumerations are values assigned to
 * position when the object is other than just "in a room"; otherwise
 * position contains the room number + 1.
 */
enum {	OBJ_HIDDEN	= -1,
	OBJ_HELD_PLAYER	=  0,	OBJ_HELD_NPC	= -200,
	OBJ_WORN_PLAYER	= -100, OBJ_WORN_NPC	= -300,
	OBJ_PART_PLAYER	= -30,	OBJ_PART_NPC	= -30,
	OBJ_ON_OBJECT	= -20,	OBJ_IN_OBJECT	= -10 };
typedef struct sc_objectstate_s {
	sc_int32	position;
	sc_int32	parent;
	sc_int32	openness;
	sc_int32	state;
	sc_bool		seen;
	sc_bool		unmoved;
} sc_objectstate_t;

/* Task state structure, tracks task done, and if task scored. */
typedef struct sc_taskstate_s {
	sc_bool		done;
	sc_bool		scored;
} sc_taskstate_t;

/* Event state structure, holds event state, and timing information. */
typedef struct sc_eventstate_s {
	enum		{ ES_WAITING  = 1, ES_RUNNING = 2, ES_AWAITING = 3,
			  ES_FINISHED = 4, ES_PAUSED  = 5 }
			state;
	sc_int32	time;
} sc_eventstate_t;

/*
 * NPC state structure, tracks the NPC location and position, any parent
 * object, whether the NPC seen, and if the NPC walks, the count of walk
 * steps and a steps array sized to this count.
 */
typedef struct sc_npcstate_s {
	sc_int32	location;
	sc_int32	position;
	sc_int32	parent;
	sc_int32	walkstep_count;
	sc_int32	*walksteps;
	sc_bool		seen;
} sc_npcstate_t;

/*
 * Resource tracking structure, holds the resource name, including any
 * trailing "##" for looping sounds, its offset into the game file, and its
 * length.  Two resources are held -- active, and requested.  The game main
 * loop compares the two, and notifies the interface on a change.
 */
typedef struct sc_resource_s {
	sc_char		*name;
	sc_int32	offset;
	sc_int32	length;
} sc_resource_t;

/*
 * Overall game state structure.  Arrays are malloc'ed for the appropriate
 * number of each of the above state structures.
 */
typedef struct sc_gamestate_s {
	sc_uint32		magic;

	/* References to assorted helper subsystems. */
	sc_var_setref_t		vars;
	sc_prop_setref_t	bundle;
	sc_printfilterref_t	filter;
	sc_debuggerref_t	debugger;

	/* Undo information, also used by the debugger. */
	struct sc_gamestate_s	*temporary;
	struct sc_gamestate_s	*undo;
	sc_bool			undo_available;

	/* Basic game state -- rooms, objects, and so on. */
	sc_int32		room_count;
	sc_roomstate_t		*rooms;
	sc_int32		object_count;
	sc_objectstate_t	*objects;
	sc_int32		task_count;
	sc_taskstate_t		*tasks;
	sc_int32		event_count;
	sc_eventstate_t		*events;
	sc_int32		npc_count;
	sc_npcstate_t		*npcs;
	sc_int32		playerroom;
	sc_int32		playerposition;
	sc_int32		playerparent;
	sc_uint32		turns;
	sc_int32		score;
	sc_bool			bold_room_names;
	sc_bool			verbose;
	sc_bool			notify_score_change;
	sc_char			*current_room_name;
	sc_char			*status_line;
	sc_char			*title;
	sc_char			*author;
	sc_char			*hint_text;

	/* Resource management data. */
	sc_resource_t		requested_sound;
	sc_resource_t		requested_graphic;
	sc_bool			stop_sound;
	sc_bool			sound_active;

	sc_resource_t		playing_sound;
	sc_resource_t		displayed_graphic;

	/* Game running and game completed flags. */
	sc_bool			is_running;
	sc_bool			has_completed;

	/* Miscellaneous library and main loop conveniences. */
	sc_int32		waitturns;
	sc_bool			is_admin;
	sc_bool			do_again;
	sc_bool			do_restart;
	sc_bool			do_restore;
	sc_bool			*object_references;
	sc_bool			*npc_references;
	sc_bool			is_object_pronoun;
	sc_bool			is_npc_pronoun;
	sc_int32		it_object;
	sc_int32		him_npc;
	sc_int32		her_npc;
	sc_int32		it_npc;

	/* Longjump buffer for external requests to quit. */
	jmp_buf			quitter;
} sc_gamestate_t;

#endif

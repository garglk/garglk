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
#include <stdio.h>
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_uint32	DEBUG_MAGIC		= 0xC4584D2E;
enum {			DEBUG_BUFFER_SIZE	= 256 };
static const sc_char	WILDCARD		= '*';
enum {			VAR_INTEGER		= 'I',
			VAR_STRING		= 'S' };
enum {			OBJ_OPEN		= 5,
			OBJ_CLOSED		= 6,
			OBJ_LOCKED		= 7 };

/* Debugging command and command argument type. */
typedef enum {	DEBUG_NONE, DEBUG_CONTINUE, DEBUG_STEP, DEBUG_BUFFER,
		DEBUG_RESOURCES, DEBUG_HELP, DEBUG_GAME,
		DEBUG_PLAYER, DEBUG_ROOMS, DEBUG_OBJECTS, DEBUG_NPCS,
		DEBUG_EVENTS, DEBUG_TASKS, DEBUG_VARIABLES,
		DEBUG_OLDPLAYER, DEBUG_OLDROOMS, DEBUG_OLDOBJECTS,
		DEBUG_OLDNPCS, DEBUG_OLDEVENTS, DEBUG_OLDTASKS,
		DEBUG_OLDVARIABLES,
		DEBUG_WATCHPLAYER, DEBUG_WATCHOBJECTS, DEBUG_WATCHNPCS,
		DEBUG_WATCHEVENTS, DEBUG_WATCHTASKS, DEBUG_WATCHVARIABLES,
		DEBUG_CLEARPLAYER, DEBUG_CLEAROBJECTS, DEBUG_CLEARNPCS,
		DEBUG_CLEAREVENTS, DEBUG_CLEARTASKS, DEBUG_CLEARVARIABLES,
		DEBUG_WATCHALL, DEBUG_CLEARALL }
sc_debug_command_t;
typedef enum {	COMMAND_QUERY, COMMAND_RANGE, COMMAND_ONE, COMMAND_ALL }
sc_debug_cmdtype_t;

/* Table connecting debugging command strings to commands. */
typedef struct {
	const sc_char			*command_string;
	const sc_debug_command_t	command;
} sc_strings_t;
static const sc_strings_t	DEBUG_COMMANDS[] = {
{ "continue",	   DEBUG_CONTINUE      },{ "step",	   DEBUG_STEP         },
{ "buffer",	   DEBUG_BUFFER        },{ "resources",	   DEBUG_RESOURCES    },
{ "help",	   DEBUG_HELP          },
{ "game",	   DEBUG_GAME          },{ "player",	   DEBUG_PLAYER       },
{ "rooms",	   DEBUG_ROOMS         },{ "objects",	   DEBUG_OBJECTS      },
{ "npcs",	   DEBUG_NPCS          },{ "events",	   DEBUG_EVENTS       },
{ "tasks",	   DEBUG_TASKS         },{ "variables",	   DEBUG_VARIABLES    },
{ "oldplayer",	   DEBUG_OLDPLAYER     },
{ "oldrooms",	   DEBUG_OLDROOMS      },{ "oldobjects",   DEBUG_OLDOBJECTS   },
{ "oldnpcs",	   DEBUG_OLDNPCS       },{ "oldevents",	   DEBUG_OLDEVENTS    },
{ "oldtasks",	   DEBUG_OLDTASKS      },{ "oldvariables", DEBUG_OLDVARIABLES },
{ "watchplayer",   DEBUG_WATCHPLAYER   },{ "clearplayer",  DEBUG_CLEARPLAYER  },
{ "watchobjects",  DEBUG_WATCHOBJECTS  },{ "watchnpcs",	   DEBUG_WATCHNPCS    },
{ "watchevents",   DEBUG_WATCHEVENTS   },{ "watchtasks",   DEBUG_WATCHTASKS   },
{ "watchvariables",DEBUG_WATCHVARIABLES},
{ "clearobjects",  DEBUG_CLEAROBJECTS  },{ "clearnpcs",	   DEBUG_CLEARNPCS    },
{ "clearevents",   DEBUG_CLEAREVENTS   },{ "cleartasks",   DEBUG_CLEARTASKS   },
{ "clearvariables",DEBUG_CLEARVARIABLES},
{ "watchall",	   DEBUG_WATCHALL      },{ "clearall",	   DEBUG_CLEARALL     },
{ NULL,		   DEBUG_NONE          }
};

/*
 * Debugging control information structure.  The structure is created and
 * added to the gamestate on enabling debug, and removed and destroyed
 * on disabling debugging.
 */
typedef struct sc_debugger_s {
	sc_uint32	magic;
	sc_bool		watch_player;
	sc_bool		*watch_objects;
	sc_bool		*watch_npcs;
	sc_bool		*watch_events;
	sc_bool		*watch_tasks;
	sc_bool		*watch_variables;
	sc_bool		single_step;
	sc_uint32	elapsed_seconds;
} sc_debugger_t;


/*
 * debug_variable_count()
 *
 * Common helper to return the count of variables defined in a gamestate.
 */
static sc_int32
debug_variable_count (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[1], vt_rvalue;
	sc_int32		variable_count;

	/* Find and return the variables count. */
	vt_key[0].string = "Variables";
	variable_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	return variable_count;
}


/*
 * debug_initialize()
 *
 * Create a new set of debug control information, and append it to the
 * gamestate passed in.
 */
static void
debug_initialize (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug;

	/* Create the easy bits of the new debugging set. */
	debug = sc_malloc (sizeof (*debug));
	debug->magic		= DEBUG_MAGIC;
	debug->watch_player	= FALSE;
	debug->single_step	= FALSE;
	debug->elapsed_seconds	= 0;

	/* Allocate watchpoints for everything we can watch. */
	debug->watch_objects	= sc_malloc (gamestate->object_count
					* sizeof (*debug->watch_objects));
	debug->watch_npcs	= sc_malloc (gamestate->npc_count
					* sizeof (*debug->watch_npcs));
	debug->watch_events	= sc_malloc (gamestate->event_count
					* sizeof (*debug->watch_events));
	debug->watch_tasks	= sc_malloc (gamestate->task_count
					* sizeof (*debug->watch_tasks));
	debug->watch_variables	= sc_malloc (debug_variable_count (gamestate)
					* sizeof (*debug->watch_variables));

	/* Clear all watchpoint arrays. */
	memset (debug->watch_objects, FALSE, gamestate->object_count
					* sizeof (*debug->watch_objects));
	memset (debug->watch_npcs, FALSE, gamestate->npc_count
					* sizeof (*debug->watch_npcs));
	memset (debug->watch_events, FALSE, gamestate->event_count
					* sizeof (*debug->watch_events));
	memset (debug->watch_tasks, FALSE, gamestate->task_count
					* sizeof (*debug->watch_tasks));
	memset (debug->watch_variables, FALSE, debug_variable_count (gamestate)
					* sizeof (*debug->watch_variables));

	/* Append the new debugger set to the gamestate. */
	assert (gamestate->debugger == NULL);
	gamestate->debugger = debug;
}


/*
 * debug_finalize()
 *
 * Destroy a debug data set, free its heap memory, and remove its reference
 * from the gamestate.
 */
static void
debug_finalize (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug	= gamestate->debugger;
	assert (debug != NULL && debug->magic == DEBUG_MAGIC);

	/* Free all allocated watchpoint arrays. */
	if (debug->watch_objects != NULL)
		sc_free (debug->watch_objects);
	if (debug->watch_npcs != NULL)
		sc_free (debug->watch_npcs);
	if (debug->watch_events != NULL)
		sc_free (debug->watch_events);
	if (debug->watch_tasks != NULL)
		sc_free (debug->watch_tasks);
	if (debug->watch_variables != NULL)
		sc_free (debug->watch_variables);

	/* Shred and free the debugger itself. */
	memset (debug, 0, sizeof (*debug));
	sc_free (debug);

	/* Remove the debug reference from the gamestate. */
	gamestate->debugger = NULL;
}


/*
 * debug_help()
 *
 * Print debugging help.
 */
static void
debug_help (sc_debug_command_t topic)
{
	/* Is help general, or specific? */
	if (topic == DEBUG_NONE)
	    {
		if_print_debug (
		"The following debugging commands examine game state:\n\n");
		if_print_debug (
		" game -- Print general game information, and class counts\n"
		" player -- Show the player location and position\n"
		" rooms [RANGE] -- Print information on game rooms\n"
		" objects [RANGE] -- Print information on objects in the game\n"
		" npcs [RANGE] -- Print information on game NPCs\n"
		" events [RANGE] -- Print information on the game's events\n"
		" tasks [RANGE] -- Print information on the game's tasks\n"
		" variables [RANGE] -- Show variables defined by the game\n\n");
		if_print_debug (
		"Most commands take range inputs.  This can be a single"
		" number, to apply the command to just that item, a range such"
		" as '0 to 10' (or '0 - 10') to apply to that range of items,"
		" or '*' to apply the command to all items of the class.  If"
		" omitted, the command is applied only to the items of the"
		" class 'relevant' to the current game state; see the help for"
		" specific commands for more on what is 'relevant'.\n\n");
		if_print_debug (
		"The 'player', 'objects', 'npcs', 'events', 'tasks', and"
		" 'variables' commands may be prefixed with 'old', in which"
		" case the values printed will be those for the previous game"
		" turn, rather than the current values.\n\n");
		if_print_debug (
		"These debugging commands manage watchpoints:\n\n");
		if_print_debug (
		"The 'player', 'objects', 'npcs', 'events', 'tasks', and"
		" 'variables' commands may be prefixed with 'watch', to set"
		" watchpoints.  Watchpoints automatically enter the debugger"
		" when the item changes state during a game turn.  For example"
		" 'watchobject 10' monitors object 10 for changes, and"
		" 'watchnpc *' monitors all NPCs.  A 'watch' command with no"
		" range prints out all watchpoints set for that class.\n\n");
		if_print_debug (
		"Prefix commands with 'clear' to clear watchpoints, for"
		" example 'clearnpcs *'.  Use 'watchall' to obtain a complete"
		" list of every watchpoint set, and 'clearall' to clear all"
		" watchpoints in one go.  A 'clear' command with no range"
		" behaves the same as a 'watch' command with no range.\n\n");
		if_print_debug (
		"These debugging commands print details of game output and"
		" control the debugger:\n\n");
		if_print_debug (
		" buffer -- Show the current buffered game text\n"
		" resources -- Show current and requested game resources\n"
		" step -- Run one game turn, then re-enter the debugger\n"
		" continue -- Leave the debugger and resume the game\n"
		" help [COMMAND] -- Print help specific to COMMAND\n\n"
		"An empty input line is treated as either 'step' or"
		" 'continue', depending on how the debugger was last exited."
		"  Debugging commands may be abbreviated to their shortest"
		" unambiguous form.\n\n");
		if_print_debug (
		"Use the 'debug' or '#debug' command in a game, typed at the"
		" usual game prompt, to return to the debugger.\n");
		return;
	    }

	/* Command-specific help. */
	switch (topic)
	    {
	    case DEBUG_HELP:
		if_print_debug (
		"Give the name of the command you want help on, for example"
		" 'help continue'.\n");
		break;

	    case DEBUG_CONTINUE:
		if_print_debug (
		"Leave the debugger and resume the game.  Use the 'debug' or"
		" '#debug' command in a game, typed at the usual game prompt,"
		" to return to the debugger.\n");
		break;

	    case DEBUG_STEP:
		if_print_debug (
		"Run one game turn, then re-enter the debugger.  Useful for"
		" games that intercept empty input lines, which otherwise"
		" catch the 'debug' command before SCARE can get to it.\n");
		break;

	    case DEBUG_BUFFER:
		if_print_debug (
		"Print the current text that the game has buffered for"
		" output.  The debugger catches games before they have printed"
		" their turn output -- this is the text that will be filtered"
		" and printed on exiting the debugger.\n");
		break;

	    case DEBUG_RESOURCES:
		if_print_debug (
		"Print any resources currently active, and any requested by"
		" the game on the current turn.  The requested resources will"
		" become the active ones on exiting the debugger.\n");
		break;

	    case DEBUG_GAME:
		if_print_debug (
		"Print general game information, including the number of"
		" rooms, objects, events, tasks, and variables that the game"
		" defines\n");
		break;

	    case DEBUG_PLAYER:
		if_print_debug (
		"Print out the current player room and position, and any"
		" parent object of the player character.\n");
		break;

	    case DEBUG_OLDPLAYER:
		if_print_debug (
		"Print out the player room and position from the previous"
		" turn, and any parent object of the player character.\n");
		break;

	    case DEBUG_ROOMS:
		if_print_debug (
		"Print out the name and contents of rooms in the range.  If"
		" no range, print details of the room containing the"
		" player.\n");
		break;

	    case DEBUG_OLDROOMS:
		if_print_debug (
		"Print out the name and contents of rooms in the range for"
		" the previous turn.  If no range, print details of the room"
		" that contained the player on the previous turn.\n");
		break;

	    case DEBUG_OBJECTS:
		if_print_debug (
		"Print out details of all objects in the range.  If"
		" no range, print details of objects in the room containing"
		" the player, and visible to the player.\n");
		break;

	    case DEBUG_OLDOBJECTS:
		if_print_debug (
		"Print out details of all objects in the range for the"
		" previous turn.  If no range, print details of objects in"
		" the room that contained the player, and were visible to the"
		" player.\n");
		break;

	    case DEBUG_NPCS:
		if_print_debug (
		"Print out details of all NPCs in the range.  If no range,"
		" print details of only NPCs in the room containing the"
		" the player.\n");
		break;

	    case DEBUG_OLDNPCS:
		if_print_debug (
		"Print out details of all NPCs in the range for the previous"
		" turn.  If no range, print details of only NPCs in the room"
		" that contained the the player.\n");
		break;

	    case DEBUG_EVENTS:
		if_print_debug (
		"Print out details of all events in the range.  If no range,"
		" print details of only events currently running.\n");
		break;

	    case DEBUG_OLDEVENTS:
		if_print_debug (
		"Print out details of all events in the range for the previous"
		" turn.  If no range, print details of only events running on"
		" the previous turn.\n");
		break;

	    case DEBUG_TASKS:
		if_print_debug (
		"Print out details of all tasks in the range.  If no range,"
		" print details of only tasks that are runnable, for the"
		" current state of the game.\n");
		break;

	    case DEBUG_OLDTASKS:
		if_print_debug (
		"Print out details of all tasks in the range for the previous"
		" turn.  If no range, print details of only tasks that were"
		" runnable, for the previous state of the game.\n");
		break;

	    case DEBUG_VARIABLES:
		if_print_debug (
		"Print out the names, types, and values of all game variables"
		" in the range.  If no range, print details of all variables"
		" (equivalent to 'variables *').\n");
		break;

	    case DEBUG_OLDVARIABLES:
		if_print_debug (
		"Print out the names, types, and values at the previous turn"
		" of all game variables in the range.  If no range, print"
		" details of all variables (equivalent to 'variables *').\n");
		break;

	    case DEBUG_WATCHPLAYER:
		if_print_debug (
		"If no range is given, list any watchpoint on player movement."
		"  If range '0' is given, set a watchpoint on player movement."
		"  Other usages of 'watchplayer' behave as if no range is"
		" given.\n");
		break;

	    case DEBUG_WATCHOBJECTS:
		if_print_debug (
		"Set watchpoints on all objects in the range.  If"
		" no range, list out object watchpoints currently set.\n");
		break;

	    case DEBUG_WATCHNPCS:
		if_print_debug (
		"Set watchpoints on all NPCs in the range.  If no range,"
		" list out NPC watchpoints currently set.\n");
		break;

	    case DEBUG_WATCHEVENTS:
		if_print_debug (
		"Set watchpoints on all events in the range.  If no range,"
		" list out event watchpoints currently set.\n");
		break;

	    case DEBUG_WATCHTASKS:
		if_print_debug (
		"Set watchpoints on all tasks in the range.  If no range,"
		" list out task watchpoints currently set.\n");
		break;

	    case DEBUG_WATCHVARIABLES:
		if_print_debug (
		"Set watchpoints on all game variables in the range.  If no"
		" range, list variable watchpoints currently set.\n");
		break;

	    case DEBUG_CLEARPLAYER:
		if_print_debug (
		"Clear any watchpoint set on player movements.\n");
		break;

	    case DEBUG_CLEAROBJECTS:
		if_print_debug (
		"Clear watchpoints on all objects in the range.  If"
		" no range, list out object watchpoints currently set.\n");
		break;

	    case DEBUG_CLEARNPCS:
		if_print_debug (
		"Clear watchpoints on all NPCs in the range.  If no range,"
		" list out NPC watchpoints currently set.\n");
		break;

	    case DEBUG_CLEAREVENTS:
		if_print_debug (
		"Clear watchpoints on all events in the range.  If no range,"
		" list out event watchpoints currently set.\n");
		break;

	    case DEBUG_CLEARTASKS:
		if_print_debug (
		"Clear watchpoints on all tasks in the range.  If no range,"
		" list out task watchpoints currently set.\n");
		break;

	    case DEBUG_CLEARVARIABLES:
		if_print_debug (
		"Clear watchpoints on all game variables in the range.  If no"
		" range, list variable watchpoints currently set.\n");
		break;

	    case DEBUG_WATCHALL:
		if_print_debug (
		"Print out a list of all all watchpoints set for all the"
		" classes of item on which watchpoints can be used.\n");
		break;

	    case DEBUG_CLEARALL:
		if_print_debug (
		"Clear all watchpoints set, on all classes of item on which"
		" watchpoints can be used.\n");
		break;

	    default:
		if_print_debug (
		"Sorry, there is no help available on that at the moment.\n");
		break;
	}
}


/*
 * debug_print_quoted()
 * debug_print_player()
 * debug_print_room()
 * debug_print_object()
 * debug_print_npc()
 * debug_print_event()
 * debug_print_task()
 * debug_print_variable()
 *
 * Low level output helpers.
 */
static void
debug_print_quoted (const sc_char *string)
{
	if_print_debug_character ('"');
	if_print_debug (string);
	if_print_debug_character ('"');
}

static void
debug_print_player (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_char			*playername;

	vt_key[0].string = "Globals";
	vt_key[1].string = "PlayerName";
	playername = prop_get_string (bundle, "S<-ss", vt_key);
	if_print_debug ("Player ");
	debug_print_quoted (playername);
}

static void
debug_print_room (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			buffer[16], *name;

	if_print_debug ("Room ");
	if (room < 0 || room >= gamestate->room_count)
	    {
		sprintf (buffer, "%ld ", room);
		if_print_debug (buffer);
		if_print_debug ("[Out of range]");
		return;
	    }

	vt_key[0].string = "Rooms";
	vt_key[1].integer = room;
	vt_key[2].string = "Short";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	sprintf (buffer, "%ld ", room);
	if_print_debug (buffer);
	debug_print_quoted (name);
}

static void
debug_print_object (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_bool			bstatic;
	sc_char			buffer[16], *prefix, *name;

	if (object < 0 || object >= gamestate->object_count)
	    {
		if_print_debug ("Object ");
		sprintf (buffer, "%ld ", object);
		if_print_debug (buffer);
		if_print_debug ("[Out of range]");
		return;
	    }

	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Static";
	bstatic = prop_get_boolean (bundle, "B<-sis", vt_key);
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);
	vt_key[2].string = "Short";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	if (bstatic)
		if_print_debug ("Static ");
	else
		if_print_debug ("Dynamic ");
	sprintf (buffer, "%ld ", object);
	if_print_debug (buffer);
	debug_print_quoted (prefix);
	if_print_debug_character (' ');
	debug_print_quoted (name);
}

static void
debug_print_npc (sc_gamestateref_t gamestate, sc_int32 npc)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			buffer[16], *prefix, *name;

	if_print_debug ("NPC ");
	if (npc < 0 || npc >= gamestate->npc_count)
	    {
		sprintf (buffer, "%ld ", npc);
		if_print_debug (buffer);
		if_print_debug ("[Out of range]");
		return;
	    }

	vt_key[0].string = "NPCs";
	vt_key[1].integer = npc;
	vt_key[2].string = "Prefix";
	prefix = prop_get_string (bundle, "S<-sis", vt_key);
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	sprintf (buffer, "%ld ", npc);
	if_print_debug (buffer);
	debug_print_quoted (prefix);
	if_print_debug_character (' ');
	debug_print_quoted (name);
}

static void
debug_print_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3];
	sc_char			buffer[16], *name;

	if_print_debug ("Event ");
	if (event < 0 || event >= gamestate->event_count)
	    {
		sprintf (buffer, "%ld ", event);
		if_print_debug (buffer);
		if_print_debug ("[Out of range]");
		return;
	    }

	vt_key[0].string = "Events";
	vt_key[1].integer = event;
	vt_key[2].string = "Short";
	name = prop_get_string (bundle, "S<-sis", vt_key);
	sprintf (buffer, "%ld ", event);
	if_print_debug (buffer);
	debug_print_quoted (name);
}

static void
debug_print_task (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[4];
	sc_char			buffer[16], *command;

	if_print_debug ("Task ");
	if (task < 0 || task >= gamestate->task_count)
	    {
		sprintf (buffer, "%ld ", task);
		if_print_debug (buffer);
		if_print_debug ("[Out of range]");
		return;
	    }

	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	vt_key[2].string = "Command";
	vt_key[3].integer = 0;
	command = prop_get_string (bundle, "S<-sisi", vt_key);
	sprintf (buffer, "%ld ", task);
	if_print_debug (buffer);
	debug_print_quoted (command);
}

static void
debug_print_variable (sc_gamestateref_t gamestate, sc_int32 variable)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_char			buffer[16], *name, var_type;

	if (variable < 0 || variable >= debug_variable_count (gamestate))
	    {
		if_print_debug ("Variable ");
		sprintf (buffer, "%ld ", variable);
		if_print_debug (buffer);
		if_print_debug ("[Out of range]");
		return;
	    }

	vt_key[0].string = "Variables";
	vt_key[1].integer = variable;
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);

	if (var_get (vars, name, &var_type, &vt_rvalue))
	    {
		switch (var_type)
		    {
		    case VAR_INTEGER:	if_print_debug ("Integer ");	break;
		    case VAR_STRING:	if_print_debug ("String ");	break;
		    default:		if_print_debug ("[Invalid type] ");
									break;
		    }
	    }
	else
		if_print_debug ("[Invalid variable] ");
	sprintf (buffer, "%ld ", variable);
	if_print_debug (buffer);
	debug_print_quoted (name);
}


/*
 * debug_game()
 *
 * Display overall game details.
 */
static void
debug_game (sc_gamestateref_t gamestate,
		sc_debug_cmdtype_t command_type)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_debuggerref_t	debug	= gamestate->debugger;
	sc_vartype_t		vt_key[2];
	sc_char			*version, *gamename, *gameauthor;
	sc_int32		perspective, waitturns;
	sc_bool			has_sound, has_graphics, has_battle;
	sc_char			buffer[16];

	if (command_type != COMMAND_QUERY)
	    {
		if_print_debug ("The Game command takes no arguments.\n");
		return;
	    }

	if_print_debug ("Game ");
	vt_key[0].string = "Globals";
	vt_key[1].string = "GameName";
	gamename = prop_get_string (bundle, "S<-ss", vt_key);
	debug_print_quoted (gamename);

	if_print_debug (", Author ");
	vt_key[0].string = "Globals";
	vt_key[1].string = "GameAuthor";
	gameauthor = prop_get_string (bundle, "S<-ss", vt_key);
	debug_print_quoted (gameauthor);
	if_print_debug_character ('\n');

	vt_key[0].string = "VersionString";
	version = prop_get_string (bundle, "S<-s", vt_key);
	if_print_debug ("    Version ");
	if_print_debug (version);

	vt_key[0].string = "Globals";
	vt_key[1].string = "Perspective";
	perspective = prop_get_integer (bundle, "I<-ss", vt_key);
	switch (perspective)
	    {
	    case 0:	if_print_debug (", First person");		break;
	    case 1:	if_print_debug (", Second person");		break;
	    case 2:	if_print_debug (", Third person");		break;
	    default:	if_print_debug (", [Unknown perspective]");	break;
	    }

	vt_key[0].string = "Globals";
	vt_key[1].string = "WaitTurns";
	waitturns = prop_get_integer (bundle, "I<-ss", vt_key);
	if_print_debug (", Waitturns ");
	sprintf (buffer, "%ld", waitturns);
	if_print_debug (buffer);

	vt_key[0].string = "Globals";
	vt_key[1].string = "Sound";
	has_sound = prop_get_boolean (bundle, "B<-ss", vt_key);
	vt_key[1].string = "Graphics";
	has_graphics = prop_get_boolean (bundle, "B<-ss", vt_key);
	if (has_sound)
		if_print_debug (", Sound");
	if (has_graphics)
		if_print_debug (", Graphics");
	if_print_debug_character ('\n');

	vt_key[0].string = "Globals";
	vt_key[1].string = "BattleSystem";
	has_battle = prop_get_boolean (bundle, "B<-ss", vt_key);
	if (has_battle)
		if_print_debug ("    Battle system\n");

	if_print_debug ("    Room count ");
	sprintf (buffer, "%ld", gamestate->room_count);
	if_print_debug (buffer);

	if_print_debug (", Object count ");
	sprintf (buffer, "%ld", gamestate->object_count);
	if_print_debug (buffer);

	if_print_debug (", NPC count ");
	sprintf (buffer, "%ld", gamestate->npc_count);
	if_print_debug (buffer);
	if_print_debug_character ('\n');

	if_print_debug ("    Event count ");
	sprintf (buffer, "%ld", gamestate->event_count);
	if_print_debug (buffer);

	if_print_debug (", Task count ");
	sprintf (buffer, "%ld", gamestate->task_count);
	if_print_debug (buffer);

	if_print_debug (", Variable count ");
	sprintf (buffer, "%ld", debug_variable_count (gamestate));
	if_print_debug (buffer);
	if_print_debug_character ('\n');

	if (gamestate->is_running)
		if_print_debug ("    Running");
	else
		if_print_debug ("    Not running");
	if (gamestate->has_completed)
		if_print_debug (", Completed");
	else
		if_print_debug (", Not completed");
	if (gamestate->verbose)
		if_print_debug (", Verbose");
	else
		if_print_debug (", Not verbose");
	if (gamestate->bold_room_names)
		if_print_debug (", Bold");
	else
		if_print_debug (", Not bold");
	if (gamestate->undo_available)
		if_print_debug (", Undo");
	else
		if_print_debug (", No undo");
	if_print_debug_character ('\n');

	if_print_debug ("    Score ");
	sprintf (buffer, "%ld", gamestate->score);
	if_print_debug (buffer);
	if_print_debug (", Turns ");
	sprintf (buffer, "%ld", gamestate->turns);
	if_print_debug (buffer);
	if_print_debug (", Seconds ");
	sprintf (buffer, "%ld", debug->elapsed_seconds);
	if_print_debug (buffer);
	if_print_debug_character ('\n');
}


/*
 * debug_player()
 *
 * Print a few brief details about the player status.
 */
static void
debug_player (sc_gamestateref_t gamestate,
		sc_debug_command_t command,
		sc_debug_cmdtype_t command_type)
{
	if (command_type != COMMAND_QUERY)
	    {
		if_print_debug ("The Player command takes no arguments.\n");
		return;
	    }

	if (command == DEBUG_OLDPLAYER)
	    {
		if (!gamestate->undo_available)
		    {
			if_print_debug ("There is no previous game state"
					" to examine.\n");
			return;
		    }

		gamestate = gamestate->undo;
		assert (gs_is_gamestate_valid (gamestate));
	    }

	debug_print_player (gamestate);
	if_print_debug_character ('\n');

	if (gamestate->playerroom == -1)
		if_print_debug ("    Hidden!\n");
	else
	    {
		if_print_debug ("    In ");
		debug_print_room (gamestate, gamestate->playerroom);
		if_print_debug_character ('\n');
	    }

	switch (gamestate->playerposition)
	    {
	    case 0:	if_print_debug ("    Standing\n");		break;
	    case 1:	if_print_debug ("    Sitting\n");		break;
	    case 2:	if_print_debug ("    Lying\n");			break;
	    default:	if_print_debug ("    [Invalid position]\n");	break;
	    }

	if (gamestate->playerparent != -1)
	    {
		if_print_debug ("    Parent is ");
		debug_print_object (gamestate, gamestate->playerparent);
		if_print_debug_character ('\n');
	    }
}


/*
 * debug_normalize_arguments()
 *
 * Normalize a set of arguments parsed from a debugger command line, for
 * debug commands that take ranges.
 */
static sc_bool
debug_normalize_arguments (sc_debug_cmdtype_t command_type,
		sc_int32 *arg1, sc_int32 *arg2, sc_int32 limit)
{
	sc_int32	low = 0, high = 0;

	/* Set range low and high depending on the command type. */
	switch (command_type)
	    {
	    case COMMAND_QUERY:
	    case COMMAND_ALL:
		low = 0;	high = limit - 1;			break;
	    case COMMAND_ONE:
		low = *arg1;	high = *arg1;				break;
	    case COMMAND_RANGE:
		low = *arg1;	high = *arg2;				break;
	    default:
		sc_fatal ("debug_normalize_arguments: bad command type\n");
	    }

	/* If range is valid, copy out and return TRUE. */
	if (low >= 0 && low < limit
			&& high >= 0 && high < limit
			&& high >= low)
	    {
		*arg1 = low;	*arg2 = high;
		return TRUE;
	    }

	/* Input range is invalid. */
	return FALSE;
}


/*
 * debug_filter_room()
 * debug_dump_room()
 *
 * Print details of rooms and their direct contents.
 */
static sc_bool
debug_filter_room (sc_gamestateref_t gamestate, sc_int32 room)
{
	return room == gamestate->playerroom;
}

static void
debug_dump_room (sc_gamestateref_t gamestate, sc_int32 room)
{
	sc_int32	object, npc;

	debug_print_room (gamestate, room);
	if_print_debug_character ('\n');

	if (gs_get_room_seen (gamestate, room))
		if_print_debug ("    Visited\n");
	else
		if_print_debug ("    Not visited\n");

	if (gamestate->playerroom == room)
	    {
		if_print_debug ("    ");
		debug_print_player (gamestate);
		if_print_debug_character ('\n');
	    }

	for (object = 0; object < gamestate->object_count; object++)
	    {
		if (obj_indirectly_in_room (gamestate, object, room))
		    {
			if_print_debug ("    ");
			debug_print_object (gamestate, object);
			if_print_debug_character ('\n');
		    }
	    }

	for (npc = 0; npc < gamestate->npc_count; npc++)
	    {
		if (npc_in_room (gamestate, npc, room))
		    {
			if_print_debug ("    ");
			debug_print_npc (gamestate, npc);
			if_print_debug_character ('\n');
		    }
	    }
}


/*
 * debug_filter_object()
 * debug_dump_object()
 *
 * Print the changeable details of game objects.
 */
static sc_bool
debug_filter_object (sc_gamestateref_t gamestate, sc_int32 object)
{
	return obj_indirectly_in_room
				(gamestate, object, gamestate->playerroom);
}

static void
debug_dump_object (sc_gamestateref_t gamestate, sc_int32 object)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_int32		openness;
	sc_vartype_t		vt_key[3];
	sc_bool			bstatic, is_statussed;
	sc_int32		position, parent;

	debug_print_object (gamestate, object);
	if_print_debug_character ('\n');

	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "Static";
	bstatic = prop_get_boolean (bundle, "B<-sis", vt_key);

	if (gs_get_object_seen (gamestate, object))
		if_print_debug ("    Seen");
	else
		if_print_debug ("    Not seen");
	if (!bstatic)
	    {
		vt_key[2].string = "OnlyWhenNotMoved";
		if (prop_get_boolean (bundle, "B<-sis", vt_key) == 1)
		    {
			if (gs_get_object_unmoved (gamestate, object))
				if_print_debug (", Not moved");
			else
				if_print_debug (", Moved");
		    }
	    }
	openness = gs_get_object_openness (gamestate, object);
	switch (openness)
	    {
	    case OBJ_OPEN:	if_print_debug (", Open");		break;
	    case OBJ_CLOSED:	if_print_debug (", Closed");		break;
	    case OBJ_LOCKED:	if_print_debug (", Locked");		break;
	    }
	if_print_debug_character ('\n');

	position = gs_get_object_position (gamestate, object);
	parent = gs_get_object_parent (gamestate, object);
	switch (position)
	    {
	    case OBJ_HIDDEN:
		if (bstatic)
			if_print_debug ("    Static unmoved\n");
		else
			if_print_debug ("    Hidden\n");
		break;
	    case OBJ_HELD_PLAYER:
		if_print_debug ("    Held by ");
		debug_print_player (gamestate);
		if_print_debug_character ('\n');			break;
	    case OBJ_HELD_NPC:
		if_print_debug ("    Held by ");
		debug_print_npc (gamestate, parent);
		if_print_debug_character ('\n');			break;
	    case OBJ_WORN_PLAYER:
		if_print_debug ("    Worn by ");
		debug_print_player (gamestate);
		if_print_debug_character ('\n');			break;
	    case OBJ_WORN_NPC:
		if_print_debug ("    Worn by ");
		debug_print_npc (gamestate, parent);
		if_print_debug_character ('\n');			break;
	    case OBJ_PART_NPC:
		if_print_debug ("    Part of ");
		if (parent == -1)
			debug_print_player (gamestate);
		else
			debug_print_npc (gamestate, parent);
		if_print_debug_character ('\n');			break;
	    case OBJ_ON_OBJECT:
		if_print_debug ("    On ");
		debug_print_object (gamestate, parent);
		if_print_debug_character ('\n');			break;
	    case OBJ_IN_OBJECT:
		if_print_debug ("    Inside ");
		debug_print_object (gamestate, parent);
		if_print_debug_character ('\n');			break;
	    default:
		if_print_debug ("    In ");
		debug_print_room (gamestate, position - 1);
		if_print_debug_character ('\n');			break;
	    }

	vt_key[0].string = "Objects";
	vt_key[1].integer = object;
	vt_key[2].string = "CurrentState";
	is_statussed = prop_get_integer (bundle, "I<-sis", vt_key) != 0;
	if (is_statussed)
	    {
		sc_char		buffer[16], *states;

		if_print_debug ("    State ");
		sprintf (buffer, "%ld",
				gs_get_object_state (gamestate, object));
		if_print_debug (buffer);

		vt_key[2].string = "States";
		states = prop_get_string (bundle, "S<-sis", vt_key);
		if_print_debug (" of ");
		debug_print_quoted (states);
		if_print_debug_character ('\n');
	    }
}


/*
 * debug_filter_npc()
 * debug_dump_npc()
 *
 * Print stuff about NPCs.
 */
static sc_bool
debug_filter_npc (sc_gamestateref_t gamestate, sc_int32 npc)
{
	return npc_in_room (gamestate, npc, gamestate->playerroom);
}

static void
debug_dump_npc (sc_gamestateref_t gamestate, sc_int32 npc)
{
	debug_print_npc (gamestate, npc);
	if_print_debug_character ('\n');

	if (gamestate->npcs[npc].seen)
		if_print_debug ("    Seen\n");
	else
		if_print_debug ("    Not seen\n");

	if (gamestate->npcs[npc].location - 1 == -1)
		if_print_debug ("    Hidden\n");
	else
	    {
		if_print_debug ("    In ");
		debug_print_room (gamestate, gamestate->npcs[npc].location - 1);
		if_print_debug_character ('\n');
	    }

	switch (gamestate->npcs[npc].position)
	    {
	    case 0:	if_print_debug ("    Standing\n");		break;
	    case 1:	if_print_debug ("    Sitting\n");		break;
	    case 2:	if_print_debug ("    Lying\n");			break;
	    default:	if_print_debug ("    [Invalid position]\n");	break;
	    }

	if (gamestate->npcs[npc].parent != -1)
	    {
		if_print_debug ("    Parent is ");
		debug_print_object (gamestate, gamestate->npcs[npc].parent);
		if_print_debug_character ('\n');
	    }

	if (gamestate->npcs[npc].walkstep_count > 0)
	    {
		sc_char		buffer[16];
		sc_int32	walk;

		if_print_debug ("    Walkstep count ");
		sprintf (buffer, "%ld", gamestate->npcs[npc].walkstep_count);
		if_print_debug (buffer);
		if_print_debug (", Walks { ");
		for (walk = 0;
			walk < gamestate->npcs[npc].walkstep_count; walk++)
		    {
			sprintf (buffer, "%ld",
					gamestate->npcs[npc].walksteps[walk]);
			if_print_debug (buffer);
			if_print_debug_character (' ');
		    }
		if_print_debug ("}.\n");
	    }
}


/*
 * debug_filter_event()
 * debug_dump_event()
 *
 * Print stuff about events.
 */
static sc_bool
debug_filter_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	return gs_get_event_state (gamestate, event) == ES_RUNNING;
}

static void
debug_dump_event (sc_gamestateref_t gamestate, sc_int32 event)
{
	sc_char		buffer[16];

	debug_print_event (gamestate, event);
	if_print_debug_character ('\n');

	switch (gs_get_event_state (gamestate, event))
	    {
	    case ES_WAITING:	if_print_debug ("    Waiting\n");	break;
	    case ES_RUNNING:	if_print_debug ("    Running\n");	break;
	    case ES_AWAITING:	if_print_debug ("    Awaiting\n");	break;
	    case ES_FINISHED:	if_print_debug ("    Finished\n");	break;
	    case ES_PAUSED:	if_print_debug ("    Paused\n");	break;
	    default:		if_print_debug ("    [Invalid state]\n");
									break;
	    }

	if_print_debug ("    Time ");
	sprintf (buffer, "%ld\n", gs_get_event_time (gamestate, event));
	if_print_debug (buffer);
}


/*
 * debug_filter_task()
 * debug_dump_task()
 *
 * Print stuff about tasks.
 */
static sc_bool
debug_filter_task (sc_gamestateref_t gamestate, sc_int32 task)
{
	return task_can_run_task (gamestate, task);
}

static void
debug_dump_task (sc_gamestateref_t gamestate, sc_int32 task)
{
	debug_print_task (gamestate, task);
	if_print_debug_character ('\n');

	if (task_can_run_task (gamestate, task))
		if_print_debug ("    Runnable");
	else
		if_print_debug ("    Not runnable");
	if (gs_get_task_done (gamestate, task))
		if_print_debug (", Done");
	else
		if_print_debug (", Not done");
	if (gs_get_task_scored (gamestate, task))
		if_print_debug (", Scored\n");
	else
		if_print_debug (", Not scored\n");
}


/*
 * debug_dump_variable()
 *
 * Print stuff about variables.
 */
static void
debug_dump_variable (sc_gamestateref_t gamestate, sc_int32 variable)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_char			*name, var_type;

	debug_print_variable (gamestate, variable);
	if_print_debug_character ('\n');

	vt_key[0].string = "Variables";
	vt_key[1].integer = variable;
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);

	if_print_debug ("    Value = ");
	if (var_get (vars, name, &var_type, &vt_rvalue))
	    {
		switch (var_type)
		    {
		    case VAR_INTEGER:
			{
			sc_char		buffer[16];

			sprintf (buffer, "%ld", vt_rvalue.integer);
			if_print_debug (buffer);
			break;
			}
		    case VAR_STRING:
			debug_print_quoted (vt_rvalue.string);
			break;
		    default:
			if_print_debug ("[Unknown]");
			break;
		    }
	    }
	else
		if_print_debug ("[Unknown]");
	if_print_debug_character ('\n');
}


/*
 * debug_dump_common()
 *
 * Common handler for iterating dumps of classes.
 */
static void
debug_dump_common (sc_gamestateref_t gamestate,
		sc_debug_command_t command,
		sc_debug_cmdtype_t command_type,
		sc_int32 arg1, sc_int32 arg2)
{
	sc_int32		low = arg1, high = arg2;
	sc_int32		limit, index;
	sc_char			*class;
	sc_bool			(*query_filter) (sc_gamestateref_t, sc_int32);
	void			(*print_function) (sc_gamestateref_t, sc_int32);
	sc_bool			printed = FALSE;

	/* Unnecessary initializations to keep GCC happy. */
	limit = 0;		class = NULL;
	query_filter = NULL;	print_function = NULL;

	/* Switch to undo gamestate on relevant commands. */
	switch (command)
	    {
	    case DEBUG_OLDROOMS:	case DEBUG_OLDOBJECTS:
	    case DEBUG_OLDNPCS:		case DEBUG_OLDEVENTS:
	    case DEBUG_OLDTASKS:	case DEBUG_OLDVARIABLES:
		if (!gamestate->undo_available)
		    {
			if_print_debug ("There is no previous game state"
					" to examine.\n");
			return;
		    }

		gamestate = gamestate->undo;
		assert (gs_is_gamestate_valid (gamestate));

	    default:
		break;
	    }

	/* Demultiplex dump command. */
	switch (command)
	    {
	    case DEBUG_ROOMS:		case DEBUG_OLDROOMS:
		class		= "Room";
		query_filter	= debug_filter_room;
		print_function	= debug_dump_room;
		limit		= gamestate->room_count;
		break;
	    case DEBUG_OBJECTS:		case DEBUG_OLDOBJECTS:
		class		= "Object";
		query_filter	= debug_filter_object;
		print_function	= debug_dump_object;
		limit		= gamestate->object_count;
		break;
	    case DEBUG_NPCS:		case DEBUG_OLDNPCS:
		class		= "NPC";
		query_filter	= debug_filter_npc;
		print_function	= debug_dump_npc;
		limit		= gamestate->npc_count;
		break;
	    case DEBUG_EVENTS:		case DEBUG_OLDEVENTS:
		class		= "Event";
		query_filter	= debug_filter_event;
		print_function	= debug_dump_event;
		limit		= gamestate->event_count;
		break;
	    case DEBUG_TASKS:		case DEBUG_OLDTASKS:
		class		= "Task";
		query_filter	= debug_filter_task;
		print_function	= debug_dump_task;
		limit		= gamestate->task_count;
		break;
	    case DEBUG_VARIABLES:	case DEBUG_OLDVARIABLES:
		class		= "Variable";
		query_filter	= NULL;
		print_function	= debug_dump_variable;
		limit		= debug_variable_count (gamestate);
		break;
	    default:
		sc_fatal ("debug_dump_common: invalid command\n");
	    }

	/* Normalize to this limit. */
	if (!debug_normalize_arguments (command_type, &low, &high, limit))
	    {
		if (limit == 0)
		    {
			if_print_debug ("There is nothing of type ");
			debug_print_quoted (class);
			if_print_debug (" to print.\n");
		    }
		else
		    {
			sc_char		buffer[16];

			if_print_debug ("Invalid item or range for ");
			debug_print_quoted (class);
			if_print_debug ("; valid values are 0 to ");
			sprintf (buffer, "%ld", limit - 1);
			if_print_debug (buffer);
			if_print_debug (".\n");
		    }
		return;
	    }

	/* Print each item of the class, filtering on query commands. */
	for (index = low; index <= high; index++)
	    {
		if (command_type == COMMAND_QUERY
				&& query_filter != NULL
				&& !(*query_filter) (gamestate, index))
			continue;

		if (printed)
			if_print_debug_character ('\n');
		(*print_function) (gamestate, index);
		printed = TRUE;
	    }
	if (!printed)
	    {
		if_print_debug ("Nothing of type ");
		debug_print_quoted (class);
		if_print_debug (" is relevant.\nTry \"");
		if_print_debug (class);
		if_print_debug (" *\" to show all items of this type.\n");
	    }
}


/*
 * debug_buffer()
 *
 * Print the current raw printfilter contents.
 */
static void
debug_buffer (sc_gamestateref_t gamestate,
		sc_debug_cmdtype_t command_type)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_char			*buffer;

	if (command_type != COMMAND_QUERY)
	    {
		if_print_debug ("The Buffer command takes no arguments.\n");
		return;
	    }

	buffer = pf_get_buffer (filter);
	if (buffer != NULL)
		if_print_debug (buffer);
	else
		if_print_debug ("There is no game text buffered.\n");
}


/*
 * debug_print_resource()
 *
 * Helper for debug_resources().
 */
static void
debug_print_resource (sc_resource_t *resource)
{
	sc_char		buffer[16];

	debug_print_quoted (resource->name);
	if_print_debug (", offset ");
	sprintf (buffer, "%ld", resource->offset);
	if_print_debug (buffer);
	if_print_debug (", length ");
	sprintf (buffer, "%ld", resource->length);
	if_print_debug (buffer);
}


/*
 * debug_resources()
 *
 * Print any active and requested resources.
 */
static void
debug_resources (sc_gamestateref_t gamestate,
		sc_debug_cmdtype_t command_type)
{
	sc_bool		printed = FALSE;

	if (command_type != COMMAND_QUERY)
	    {
		if_print_debug ("The Resources command takes no arguments.\n");
		return;
	    }

	if (gamestate->stop_sound)
	    {
		if_print_debug ("Sound stop");
		if (strlen (gamestate->requested_sound.name) > 0)
			if_print_debug (" before new sound");
		if_print_debug (" requested");
		if (gamestate->sound_active)
			if_print_debug (", sound active");
		if_print_debug (".\n");
		printed = TRUE;
	    }
	if (!res_compare_resource (&gamestate->requested_sound,
					&gamestate->playing_sound))
	    {
		if_print_debug ("Requested Sound ");
		debug_print_resource (&gamestate->requested_sound);
		if_print_debug (".\n");
		printed = TRUE;
	    }
	if (!res_compare_resource (&gamestate->requested_graphic,
					&gamestate->displayed_graphic))
	    {
		if_print_debug ("Requested Graphic ");
		debug_print_resource (&gamestate->requested_graphic);
		if_print_debug (".\n");
		printed = TRUE;
	    }

	if (strlen (gamestate->playing_sound.name) > 0)
	    {
		if_print_debug ("Playing Sound ");
		debug_print_resource (&gamestate->playing_sound);
		if_print_debug (".\n");
		printed = TRUE;
	    }
	if (strlen (gamestate->displayed_graphic.name) > 0)
	    {
		if_print_debug ("Displaying Graphic ");
		debug_print_resource (&gamestate->displayed_graphic);
		if_print_debug (".\n");
		printed = TRUE;
	    }

	if (!printed)
		if_print_debug ("There is no game resource activity.\n");
}


/*
 * debug_watchpoint_common()
 *
 * Common handler for setting and clearing watchpoints.
 */
static void
debug_watchpoint_common (sc_gamestateref_t gamestate,
		sc_debug_command_t command,
		sc_debug_cmdtype_t command_type,
		sc_int32 arg1, sc_int32 arg2)
{
	sc_debuggerref_t	debug	= gamestate->debugger;
	sc_int32		low = arg1, high = arg2;
	sc_int32		limit, index;
	sc_char			*class;
	sc_bool			*watchpoints, action;
	sc_char			buffer[16];

	/* Unnecessary initializations to keep GCC happy. */
	limit = 0;		class = NULL;
	watchpoints = NULL;	action = FALSE;

	/* Set action to TRUE or FALSE, for setting/clearing watchpoints. */
	switch (command)
	    {
	    case DEBUG_WATCHPLAYER:	case DEBUG_WATCHOBJECTS:
	    case DEBUG_WATCHNPCS:	case DEBUG_WATCHEVENTS:
	    case DEBUG_WATCHTASKS:	case DEBUG_WATCHVARIABLES:
		action = TRUE;
		break;
	    case DEBUG_CLEARPLAYER:	case DEBUG_CLEAROBJECTS:
	    case DEBUG_CLEARNPCS:	case DEBUG_CLEAREVENTS:
	    case DEBUG_CLEARTASKS:	case DEBUG_CLEARVARIABLES:
		action = FALSE;
		break;
	    default:
		sc_fatal ("debug_watch_common: invalid command\n");
	    }

	/* Handle player watchpoint setting. */
	if (command == DEBUG_WATCHPLAYER
			|| command == DEBUG_CLEARPLAYER)
	    {
		if (command == DEBUG_CLEARPLAYER)
		    {
			debug->watch_player = action;
			if_print_debug ("Cleared Player watchpoint.\n");
		    }
		else if (command_type == COMMAND_ONE && arg1 == 0)
		    {
			debug->watch_player = action;
			if_print_debug ("Set Player watchpoint.\n");
		    }
		else
		    {
			if (debug->watch_player)
				if_print_debug ("Player watchpoint is set.\n");
			else
				if_print_debug
					("No Player watchpoint is set; to"
					" set one, use \"Watchplayer 0\".\n");
		    }
		return;
	    }

	/* Demultiplex watchpoint command. */
	switch (command)
	    {
	    case DEBUG_WATCHOBJECTS:	case DEBUG_CLEAROBJECTS:
		class		= "Object";
		watchpoints	= debug->watch_objects;
		limit		= gamestate->object_count;
		break;
	    case DEBUG_WATCHNPCS:	case DEBUG_CLEARNPCS:
		class		= "NPC";
		watchpoints	= debug->watch_npcs;
		limit		= gamestate->npc_count;
		break;
	    case DEBUG_WATCHEVENTS:	case DEBUG_CLEAREVENTS:
		class		= "Event";
		watchpoints	= debug->watch_events;
		limit		= gamestate->event_count;
		break;
	    case DEBUG_WATCHTASKS:	case DEBUG_CLEARTASKS:
		class		= "Task";
		watchpoints	= debug->watch_tasks;
		limit		= gamestate->task_count;
		break;
	    case DEBUG_WATCHVARIABLES:	case DEBUG_CLEARVARIABLES:
		class		= "Variable";
		watchpoints	= debug->watch_variables;
		limit		= debug_variable_count (gamestate);
		break;
	    default:
		sc_fatal ("debug_watch_common: invalid command\n");
	    }

	/* Normalize to this limit. */
	if (!debug_normalize_arguments (command_type, &low, &high, limit))
	    {
		if (limit == 0)
		    {
			if_print_debug ("There is nothing of type ");
			debug_print_quoted (class);
			if_print_debug (" to watch.\n");
		    }
		else
		    {
			if_print_debug ("Invalid item or range for ");
			debug_print_quoted (class);
			if_print_debug ("; valid values are 0 to ");
			sprintf (buffer, "%ld", limit - 1);
			if_print_debug (buffer);
			if_print_debug (".\n");
		    }
		return;
	    }

	/* On query, search the array for set flags, and print out. */
	if (command_type == COMMAND_QUERY)
	    {
		sc_bool		printed = FALSE;

		/* Scan for set watchpoints, and list each found. */
		for (index = low; index <= high; index++)
		    {
			if (watchpoints[index])
			    {
				if (!printed)
				    {
					if_print_debug
						("Watchpoints are set for ");
					if_print_debug (class);
					if_print_debug (" { ");
				    }
				sprintf (buffer, "%ld", index);
				if_print_debug (buffer);
				if_print_debug_character (' ');
				printed = TRUE;
			    }
		    }
		if (printed)
			if_print_debug ("}.\n");
		else
		    {
			if_print_debug ("No ");
			if_print_debug (class);
			if_print_debug (" watchpoints are set.\n");
		    }
		return;
	    }

	/*
	 * For non-queries, set watchpoint flags as defined in action for
	 * the range determined, and print confirmation.
	 */
	for (index = low; index <= high; index++)
		watchpoints[index] = action;

	if (action)
		if_print_debug ("Set ");
	else
		if_print_debug ("Cleared ");
	sprintf (buffer, "%ld ", high - low + 1);
	if_print_debug (buffer);
	if_print_debug (class);
	if (high == low)
		if_print_debug (" watchpoint.\n");
	else
		if_print_debug (" watchpoints.\n");
}


/*
 * debug_watchall_common()
 *
 * Common handler to list out and clear all set watchpoints at a stroke.
 */
static void
debug_watchall_common (sc_gamestateref_t gamestate,
		sc_debug_command_t command,
		sc_debug_cmdtype_t command_type)
{
	sc_debuggerref_t	debug	= gamestate->debugger;

	if (command_type != COMMAND_QUERY)
	    {
		if (command == DEBUG_WATCHALL)
			if_print_debug
				("The Watchall command takes no arguments.\n");
		else
			if_print_debug
				("The Clearall command takes no arguments.\n");
		return;
	    }

	/* Query all set watchpoints using common watchpoint handler... */
	if (command == DEBUG_WATCHALL)
	    {
		debug_watchpoint_common (gamestate, DEBUG_WATCHPLAYER,
							COMMAND_QUERY, 0, 0);
		debug_watchpoint_common (gamestate, DEBUG_WATCHOBJECTS,
							COMMAND_QUERY, 0, 0);
		debug_watchpoint_common (gamestate, DEBUG_WATCHNPCS,
							COMMAND_QUERY, 0, 0);
		debug_watchpoint_common (gamestate, DEBUG_WATCHEVENTS,
							COMMAND_QUERY, 0, 0);
		debug_watchpoint_common (gamestate, DEBUG_WATCHTASKS,
							COMMAND_QUERY, 0, 0);
		debug_watchpoint_common (gamestate, DEBUG_WATCHVARIABLES,
							COMMAND_QUERY, 0, 0);
		return;
	    }

	/*  ...but reset all the fast way, with memset(). */
	assert (command == DEBUG_CLEARALL);
	debug->watch_player = FALSE;
	memset (debug->watch_objects, FALSE, gamestate->object_count
					* sizeof (*debug->watch_objects));
	memset (debug->watch_npcs, FALSE, gamestate->npc_count
					* sizeof (*debug->watch_npcs));
	memset (debug->watch_events, FALSE, gamestate->event_count
					* sizeof (*debug->watch_events));
	memset (debug->watch_tasks, FALSE, gamestate->task_count
					* sizeof (*debug->watch_tasks));
	memset (debug->watch_variables, FALSE, debug_variable_count (gamestate)
					* sizeof (*debug->watch_variables));
	if_print_debug ("Cleared all watchpoints.\n");
}


/*
 * debug_compare_object()
 *
 * Compare two objects, and return TRUE if the same.
 */
static sc_bool
debug_compare_object (sc_gamestateref_t from, sc_gamestateref_t with,
		sc_int32 object)
{
	sc_objectstate_t	*from_object = from->objects + object;
	sc_objectstate_t	*with_object = with->objects + object;

	return (from_object->unmoved == with_object->unmoved
			&& from_object->position == with_object->position
			&& from_object->parent == with_object->parent
			&& from_object->openness == with_object->openness
			&& from_object->state == with_object->state
			&& from_object->seen == with_object->seen);
}


/*
 * debug_compare_npc()
 *
 * Compare two NPCs, and return TRUE if the same.
 */
static sc_bool
debug_compare_npc (sc_gamestateref_t from, sc_gamestateref_t with,
		sc_int32 npc)
{
	sc_npcstate_t		*from_npc = from->npcs + npc;
	sc_npcstate_t		*with_npc = with->npcs + npc;

	if (from_npc->walkstep_count != with_npc->walkstep_count)
		sc_fatal ("debug_compare_npc: walkstep count error\n");

	return (from_npc->location == with_npc->location
			&& from_npc->position == with_npc->position
			&& from_npc->parent == with_npc->parent
			&& from_npc->seen == with_npc->seen
			&& !memcmp (from_npc->walksteps, with_npc->walksteps,
					from_npc->walkstep_count
					* sizeof (*from_npc->walksteps)));
}


/*
 * debug_compare_event()
 *
 * Compare two events, and return TRUE if the same.
 */
static sc_bool
debug_compare_event (sc_gamestateref_t from, sc_gamestateref_t with,
		sc_int32 event)
{
	sc_eventstate_t		*from_event = from->events + event;
	sc_eventstate_t		*with_event = with->events + event;

	return (from_event->state == with_event->state
			&& from_event->time == with_event->time);
}


/*
 * debug_compare_task()
 *
 * Compare two tasks, and return TRUE if the same.
 */
static sc_bool
debug_compare_task (sc_gamestateref_t from, sc_gamestateref_t with,
		sc_int32 task)
{
	sc_taskstate_t		*from_task = from->tasks + task;
	sc_taskstate_t		*with_task = with->tasks + task;

	return (from_task->done == with_task->done
			&& from_task->scored == with_task->scored);
}


/*
 * debug_compare_variable()
 *
 * Compare two variables, and return TRUE if the same.
 */
static sc_bool
debug_compare_variable (sc_gamestateref_t from, sc_gamestateref_t with,
		sc_int32 variable)
{
	sc_prop_setref_t	bundle	 = from->bundle;
	sc_var_setref_t		from_var = from->vars;
	sc_var_setref_t		with_var = with->vars;
	sc_vartype_t		vt_key[3], vt_rvalue, vt_rvalue2;
	sc_char			*name, var_type, var_type2;
	sc_bool			equal = FALSE;

	if (from->bundle != with->bundle)
	    {
		sc_fatal ("debug_compare_variable:"
					" property sharing malfunction\n");
	    }

	vt_key[0].string = "Variables";
	vt_key[1].integer = variable;
	vt_key[2].string = "Name";
	name = prop_get_string (bundle, "S<-sis", vt_key);

	if (!var_get (from_var, name, &var_type, &vt_rvalue)
			|| !var_get (with_var, name, &var_type2, &vt_rvalue2))
	    {
		sc_fatal ("debug_compare_variable:"
					" can't find variable %s\n", name);
	    }
	if (var_type != var_type2)
	    {
		sc_fatal ("debug_compare_variable:"
					" variable type mismatch %s\n", name);
	    }

	switch (var_type)
	    {
	    case VAR_INTEGER:
		equal = (vt_rvalue.integer == vt_rvalue2.integer);
		break;
	    case VAR_STRING:
		equal = !strcmp (vt_rvalue.string, vt_rvalue2.string);
		break;
	    default:
		sc_fatal ("debug_compare_variable:"
				" invalid variable type, %c\n", var_type);
	    }

	return equal;
}


/*
 * debug_check_class()
 *
 * Central handler for checking watchpoints.  Compares a number of items
 * of a class using the comparison function given, where indicated by a
 * watchpoints flags array.  Prints entries that differ, and returns TRUE
 * if any differed.
 */
static sc_bool
debug_check_class (sc_gamestateref_t from, sc_gamestateref_t with,
		const sc_char *class, sc_int32 class_count,
		sc_bool *watchpoints,
		sc_bool (*compare_function)
			(sc_gamestateref_t, sc_gamestateref_t, sc_int32))
{
	sc_int32	index;
	sc_bool		triggered = FALSE;

	/*
	 * Scan the watchpoints array for set watchpoints, comparing classes
	 * where the watchpoint flag is set.
	 */
	for (index = 0; index < class_count; index++)
	    {
		if (!watchpoints[index])
			continue;

		if (!(*compare_function) (from, with, index))
		    {
			sc_char		buffer[16];

			if (!triggered)
			    {
				if_print_debug ("--- ");
				if_print_debug (class);
				if_print_debug (" watchpoint triggered { ");
			    }
			sprintf (buffer, "%ld ", index);
			if_print_debug (buffer);
			triggered = TRUE;
		    }
	    }
	if (triggered)
		if_print_debug ("}.\n");

	/* Return TRUE if anything differed. */
	return triggered;
}


/*
 * debug_check_watchpoints()
 *
 * Checks the gamestate against the undo gamestate for all set watchpoints.
 * Returns TRUE if any triggered, FALSE if none (or if the undo gamestate
 * isn't available, in which case no check is possible).
 */
static sc_bool
debug_check_watchpoints (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug	= gamestate->debugger;
	sc_gamestateref_t	undo	= gamestate->undo;
	sc_bool			triggered;
	assert (gs_is_gamestate_valid (undo));

	/* If no undo is present, no check is possible. */
	if (!gamestate->undo_available)
		return FALSE;

	/* Check first for player watchpoint. */
	triggered = FALSE;
	if (debug->watch_player)
	    {
		if (gamestate->playerroom != undo->playerroom
			|| gamestate->playerposition != undo->playerposition
			|| gamestate->playerparent != undo->playerparent)
		    {
			if_print_debug ("--- Player watchpoint triggered.\n");
			triggered |= TRUE;
		    }
	    }

	/* Now check other classes of watchpoint. */
	triggered |= debug_check_class (gamestate, undo,
				"Object", gamestate->object_count,
				debug->watch_objects, debug_compare_object);
	triggered |= debug_check_class (gamestate, undo,
				"NPC", gamestate->npc_count,
				debug->watch_npcs, debug_compare_npc);
	triggered |= debug_check_class (gamestate, undo,
				"Event", gamestate->event_count,
				debug->watch_events, debug_compare_event);
	triggered |= debug_check_class (gamestate, undo,
				"Task", gamestate->task_count,
				debug->watch_tasks, debug_compare_task);
	triggered |= debug_check_class (gamestate, undo,
				"Variable", debug_variable_count (gamestate),
				debug->watch_variables, debug_compare_variable);

	return triggered;
}


/*
 * debug_parse_command()
 *
 * Given a debugging command string, try to parse it and return the
 * appropriate command and its arguments.  Returns DEBUG_NONE if the parse
 * fails.
 */
static sc_debug_command_t
debug_parse_command (const sc_char *command_string,
			sc_debug_cmdtype_t *command_type,
			sc_int32 *arg1, sc_int32 *arg2,
			sc_debug_command_t *help_topic)
{
	sc_debug_command_t	return_command;
	sc_debug_cmdtype_t	return_command_type;
	sc_int32		val1, val2, converted, matches;
	sc_char			*string, junk, wildcard;
	sc_bool			help_special = FALSE;
	sc_bool			line_ok, wildcard_ok;
	const sc_strings_t	*entry;

	/* Allocate a string long enough to take a copy of the input. */
	string = sc_malloc (strlen (command_string) + 1);

	/*
	 * Parse the input line, in a very simplistic fashion.  The
	 * argument count is one less than sscanf converts.
	 */
	line_ok = FALSE; wildcard_ok = FALSE; help_special = FALSE;
	val1 = val2 = 0;
	converted = sscanf (command_string, " help %s %c", string, &junk);
	if (converted == 1)
	    {
		help_special = TRUE;
		line_ok = TRUE;
	    }
	if (!line_ok)
	    {
		converted = sscanf (command_string, " %s %ld to %ld %c",
					string, &val1, &val2, &junk);
		if (converted != 3)
			converted = sscanf (command_string, " %s %ld - %ld %c",
					string, &val1, &val2, &junk);
		line_ok |= converted == 3;
	    }
	if (!line_ok)
	    {
		converted = sscanf (command_string, " %s %ld %c",
					string, &val1, &junk);
		line_ok |= converted == 2;
	    }
	if (!line_ok)
	    {
		converted = sscanf (command_string, " %s %c%c",
					string, &wildcard, &junk);
		if (converted == 2 && wildcard == WILDCARD)
		    {
			wildcard_ok = TRUE;
			line_ok = TRUE;
		    }
		else
			line_ok |= converted == 1;
	    }
	if (!line_ok)
	    {
		if_print_debug ("Invalid debug command.");
		if_print_debug ("  Type 'help' for a list"
						" of valid commands.\n");
		sc_free (string);
		return DEBUG_NONE;
	    }

	/* Decide on a command type based on the parse. */
	if (wildcard_ok)
		return_command_type = COMMAND_ALL;
	else if (converted == 3)
		return_command_type = COMMAND_RANGE;
	else if (converted == 2)
		return_command_type = COMMAND_ONE;
	else
		return_command_type = COMMAND_QUERY;

	/*
	 * Find the first unambiguous command matching the string.  If none,
	 * return DEBUG_NONE.
	 */
	matches = 0; return_command = DEBUG_NONE;
	for (entry = DEBUG_COMMANDS;
			entry->command_string != NULL; entry++)
	    {
		if (!sc_strncasecmp (string,
				entry->command_string, strlen (string)))
		    {
			matches++;
			return_command = entry->command;
		    }
	    }
	if (matches != 1)
	    {
		if (matches > 1)
			if_print_debug ("Ambiguous debug command.");
		else
			if_print_debug ("Unrecognized debug command.");
		if_print_debug ("  Type 'help' for a list"
						" of valid commands.\n");
		sc_free (string);
		return DEBUG_NONE;
	    }

	/* Done with temporary command parse area. */
	sc_free (string);

	/*
	 * Return the command type, arguments, and the debugging command.
	 * For help <topic>, the command is help, with the command on
	 * which help requested in *help_topic.  All clear, then?
	 */
	*command_type	= return_command_type;
	*arg1		= val1;
	*arg2		= val2;
	*help_topic	= !help_special ? DEBUG_NONE : return_command;
	return !help_special ? return_command : DEBUG_HELP;
}


/*
 * debug_dispatch()
 *
 * Dispatch a debugging command to the appropriate handler.
 */
static void
debug_dispatch (sc_gamestateref_t gamestate,
			sc_debug_command_t command,
			sc_debug_cmdtype_t command_type,
			sc_int32 arg1, sc_int32 arg2,
			sc_debug_command_t help_topic)
{
	/* Demultiplex debugging command, and call handlers. */
	switch (command)
	    {
	    case DEBUG_HELP:
		debug_help (help_topic);
		break;
	    case DEBUG_BUFFER:
		debug_buffer (gamestate, command_type);
		break;
	    case DEBUG_RESOURCES:
		debug_resources (gamestate, command_type);
		break;
	    case DEBUG_GAME:
		debug_game (gamestate, command_type);
		break;
	    case DEBUG_PLAYER:
	    case DEBUG_OLDPLAYER:
		debug_player (gamestate, command, command_type);
		break;
	    case DEBUG_ROOMS:		case DEBUG_OBJECTS:
	    case DEBUG_NPCS:		case DEBUG_EVENTS:
	    case DEBUG_TASKS:		case DEBUG_VARIABLES:
	    case DEBUG_OLDROOMS:	case DEBUG_OLDOBJECTS:
	    case DEBUG_OLDNPCS:		case DEBUG_OLDEVENTS:
	    case DEBUG_OLDTASKS:	case DEBUG_OLDVARIABLES:
		debug_dump_common (gamestate, command,
						command_type, arg1, arg2);
		break;
	    case DEBUG_WATCHPLAYER:	case DEBUG_WATCHOBJECTS:
	    case DEBUG_WATCHNPCS:	case DEBUG_WATCHEVENTS:
	    case DEBUG_WATCHTASKS:	case DEBUG_WATCHVARIABLES:
	    case DEBUG_CLEARPLAYER:	case DEBUG_CLEAROBJECTS:
	    case DEBUG_CLEARNPCS:	case DEBUG_CLEAREVENTS:
	    case DEBUG_CLEARTASKS:	case DEBUG_CLEARVARIABLES:
		debug_watchpoint_common (gamestate, command,
						command_type, arg1, arg2);
		break;
	    case DEBUG_WATCHALL:	case DEBUG_CLEARALL:
		debug_watchall_common (gamestate, command, command_type);
		break;
	    case DEBUG_NONE:
		break;
	    default:
		sc_fatal ("debug_dispatch: invalid debug command\n");
	    }
}


/*
 * debug_dialog()
 *
 * Create a small debugging dialog with the user.
 */
static void
debug_dialog (sc_gamestateref_t gamestate)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_debuggerref_t	debug	= gamestate->debugger;

	/* Note elapsed seconds, so time stands still while debugging. */
	debug->elapsed_seconds = var_get_elapsed_seconds (vars);

	/* Handle debug commands until debugger quit. */
	while (TRUE)
	    {
		sc_char			buffer[DEBUG_BUFFER_SIZE];
		sc_debug_command_t	command, help_topic;
		sc_debug_cmdtype_t	command_type;
		sc_int32		arg1, arg2;

		/*
		 * Get a debugging command string from the user.  Exit the
		 * dialog on an empty line, interpreted as either continue
		 * or single-step, depending on last dialog exit type.
		 */
		if_read_debug (buffer, sizeof (buffer));
		if (sc_strempty (buffer))
			break;

		/* Parse the command read, and handle dialog exit commands. */
		command = debug_parse_command (buffer, &command_type,
						&arg1, &arg2, &help_topic);
		if (command == DEBUG_CONTINUE
				|| command == DEBUG_STEP)
		    {
			debug->single_step = (command == DEBUG_STEP);
			break;
		    }

		/* Dispatch the remaining debugging commands. */
		debug_dispatch (gamestate, command, command_type,
						arg1, arg2, help_topic);
	    }

	/* Restart time. */
	var_set_elapsed_seconds (vars, debug->elapsed_seconds);
}


/*
 * debug_run_command()
 *
 * Handle a single debugging command line from the outside world.  Returns
 * TRUE if valid, FALSE if invalid (parse failed, not understood).
 */
sc_bool
debug_run_command (sc_gamestateref_t gamestate,
		const sc_char *debug_command)
{
	sc_debuggerref_t	debug;
	sc_debug_command_t	command, help_topic;
	sc_debug_cmdtype_t	command_type;
	sc_int32		arg1, arg2;
	assert (gs_is_gamestate_valid (gamestate));

	/* If debugging disallowed (not initialized), refuse the call. */
	if (gamestate->debugger == NULL)
		return FALSE;

	/* Retrieve and check the debugger structure in the game state. */
	debug = gamestate->debugger;
	assert (debug->magic == DEBUG_MAGIC);

	/*
	 * Parse the command string passed in, and return FALSE if the
	 * parse fails, or if it returns DEBUG_CONTINUE or DEBUG_STEP,
	 * neither of which make any sense in this context.
	 */
	command = debug_parse_command (debug_command, &command_type,
						&arg1, &arg2, &help_topic);
	if (command == DEBUG_NONE
			|| command == DEBUG_CONTINUE
			|| command == DEBUG_STEP)
		return FALSE;

	/* Dispatch the remaining debugging commands, return successfully. */
	debug_dispatch (gamestate, command, command_type,
						arg1, arg2, help_topic);
	return TRUE;
}


/*
 * debug_cmd_debugger()
 *
 * Called by the run main loop on user "debug" command request.  Prints
 * a polite refusal if debugging is not enabled, otherwise runs a debugging
 * dialog.  Uses if_print_string() as this isn't debug output.
 */
sc_bool
debug_cmd_debugger (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug;
	assert (gs_is_gamestate_valid (gamestate));

	/* If debugging disallowed (not initialized), ignore the call. */
	if (gamestate->debugger == NULL)
	   {
		if_print_string ("SCARE's game debugger is not"
						" enabled.  Sorry.\n");
		gamestate->is_admin = TRUE;
		return TRUE;
	   }

	/* Retrieve and check the debugger structure in the game state. */
	debug = gamestate->debugger;
	assert (debug->magic == DEBUG_MAGIC);

	/*
	 * Run the debugger dialog, and set as administrative command, so
	 * as not to consume a game turn, and return successfully.
	 */
	debug_dialog (gamestate);
	gamestate->is_admin = TRUE;
	return TRUE;
}


/*
 * debug_game_started()
 * debug_game_ended()
 *
 * The first is called on entry to the game main loop, and gives us a chance
 * to look at things before any turns are run, and to set watchpoints to
 * catch things in games that use catch-all command tasks on startup (The PK
 * Girl, for example).
 *
 * The second is called on exit from the game, and may make a final sweep for
 * watchpoints and offer the debug dialog one last time.
 */
void
debug_game_started (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug;
	assert (gs_is_gamestate_valid (gamestate));

	/* If debugging disallowed (not initialized), ignore the call. */
	if (gamestate->debugger == NULL)
		return;

	/* Retrieve and check the debugger structure in the game state. */
	debug = gamestate->debugger;
	assert (debug->magic == DEBUG_MAGIC);

	/*
	 * If this is a new game starting, print a banner, and run the
	 * debugger dialog.  If it's a restart, run the dialog only if
	 * single-stepping -- no need to check watchpoints on a restart,
	 * as none can be set; no undo.
	 */
	if (gamestate->turns == 0)
	    {
		if_print_debug ("\n--- SCARE " SCARE_VERSION " Game Debugger\n"
				"--- Type 'help' for a list of commands.\n");
		debug_dialog (gamestate);
	    }
	else
	    {
		if (debug->single_step)
			debug_dialog (gamestate);
	    }
}

void
debug_game_ended (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug;
	assert (gs_is_gamestate_valid (gamestate));

	/* If debugging disallowed (not initialized), ignore the call. */
	if (gamestate->debugger == NULL)
		return;

	/* Retrieve and check the debugger structure in the game state. */
	debug = gamestate->debugger;
	assert (debug->magic == DEBUG_MAGIC);

	/*
	 * Using our carnal knowledge of the run main loop, we know here
	 * that if the loop exited with do_restart or do_restore, we'll
	 * get a call to debug_game_start() when the loop restarts.  So
	 * in this case, ignore the call (even if single stepping).
	 */
	if (gamestate->do_restart
			|| gamestate->do_restore)
		return;

	/*
	 * Check for any final watchpoints, and print a message describing
	 * why we're here.  Suppress the check for watchpoints if the user
	 * exited the game, as it'll only be a repeat of any found last
	 * turn update.
	 */
	if (!gamestate->is_running)
	    {
		if (gamestate->has_completed)
		    {
			debug_check_watchpoints (gamestate);
			if_print_debug ("\n--- The game has completed.\n");
		    }
		else
			if_print_debug ("\n--- The game has exited.\n");
	    }
	else
	    {
		debug_check_watchpoints (gamestate);
		if_print_debug ("\n--- The game is still running!\n");
	    }

	/* Run a final dialog. */
	debug_dialog (gamestate);
}


/*
 * debug_turn_update()
 *
 * Called after each turn by the main game loop.  Checks for any set
 * watchpoints, and triggers a debug dialog when any fire.
 */
void
debug_turn_update (sc_gamestateref_t gamestate)
{
	sc_debuggerref_t	debug;
	sc_bool			triggered;
	assert (gs_is_gamestate_valid (gamestate));

	/* If debugging disallowed (not initialized), ignore the call. */
	if (gamestate->debugger == NULL)
		return;

	/* Retrieve and check the debugger structure in the game state. */
	debug = gamestate->debugger;
	assert (debug->magic == DEBUG_MAGIC);

	/*
	 * Again using carnal knowledge of the run main loop, if we're
	 * in mid-wait, ignore the call.  Also, ignore the call if the
	 * game is no longer running, as we'll see a debug_game_ended()
	 * call come along to handle that.
	 */
	if (gamestate->waitturns > 0
			|| !gamestate->is_running)
		return;

	/*
	 * Run debugger dialog if any watchpoints triggered, or if single
	 * stepping (even if none triggered).
	 */
	triggered = debug_check_watchpoints (gamestate);
	if (triggered
		|| debug->single_step)
	    {
		debug_dialog (gamestate);
	    }
}


/*
 * debug_set_enabled()
 * debug_get_enabled()
 *
 * Enable/disable debugging, and return debugging status.  Debugging is
 * enabled when there is a debugger reference in the gamestate, and disabled
 * when it's NULL -- that's the flag.  To avoid lugging about all the
 * watchpoint memory with a gamestate, debugger data is allocated on
 * enabling, and free'd on disabling; as a result, any set watchpoints are
 * lost on disabling.
 */
void
debug_set_enabled (sc_gamestateref_t gamestate, sc_bool enable)
{
	assert (gs_is_gamestate_valid (gamestate));

	/*
	 * If enabling and already enabled, or disabling and already
	 * disabled, just return.
	 */
	if ((enable && gamestate->debugger != NULL)
			|| (!enable && gamestate->debugger == NULL))
		return;

	/* Initialize or finalize debugging, as appropriate. */
	if (enable)
		debug_initialize (gamestate);
	else
		debug_finalize (gamestate);
}

sc_bool
debug_get_enabled (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	return gamestate->debugger != NULL;
}

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
#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
enum {			LINE_BUFFER_SIZE	= 256 };
static const sc_char	NUL			= '\0',
			COMMA			= ',',
			SPECIAL_PATTERN		= '#';


/*
 * run_special_task_function()
 *
 * Check for the presence of a command function in the first task command,
 * and action it if found.  This is a 4.00.42 compatibility hack -- at
 * present, only getdynfromroom() exists.  Returns TRUE if function found
 * and handled.
 */
static sc_bool
run_special_task_function (const sc_char *pattern, sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[3];
	sc_int32		room, object, index;
	sc_char			*arg, *name;

	/* Simple comparison against the one known task expression. */
	arg = sc_malloc (strlen (pattern) + 1);
	if (sscanf (pattern,
			" # %%object%% = getdynfromroom (%[^)])", arg) == 0)
	    {
		sc_free (arg);
		return FALSE;
	    }

	/*
	 * Compare the argument read in against known room names.  TODO Is
	 * this simple room name comparison good enough?
	 */
	vt_key[0].string = "Rooms";
	for (room = 0; room < gamestate->room_count; room++)
	    {
		vt_key[1].integer = room;
		vt_key[2].string = "Short";
		name = prop_get_string (bundle, "S<-sis", vt_key);
		if (!sc_strcasecmp (name, arg))
			break;
	    }
	sc_free (arg);
	if (room == gamestate->room_count)
		return FALSE;

	/*
	 * Select a dynamic object from the room.  TODO What are the
	 * selection criteria supposed to be?  Here we use "on the floor".
	 */
	vt_key[0].string = "Objects";
	for (object = 0; object < gamestate->object_count; object++)
	    {
		sc_bool		bstatic;

		vt_key[1].integer = object;
		vt_key[2].string = "Static";
		bstatic = prop_get_boolean (bundle, "B<-sis", vt_key);
		if (!bstatic
			    && obj_directly_in_room (gamestate, object, room))
			break;
	    }
	if (object == gamestate->object_count)
		return FALSE;

	/* Set this object reference, unambiguously, as if %object% match. */
	for (index = 0; index < gamestate->object_count; index++)
		gamestate->object_references[index] = FALSE;

	gamestate->object_references[object] = TRUE;
	var_set_ref_object (vars, object);

	return TRUE;
}


/*
 * run_task_command()
 *
 * Compare a user input string against a game command.  On match, action
 * the command and return TRUE.
 */
static sc_bool
run_task_command (sc_gamestateref_t gamestate, const sc_char *string,
		sc_int32 task, sc_bool forwards)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_rvalue, vt_key[4];
	sc_int32		command_count, command;

	/* Get the count of task commands, zero if none. */
	vt_key[0].string = "Tasks";
	vt_key[1].integer = task;
	if (forwards)
		vt_key[2].string = "Command";
	else
		vt_key[2].string = "ReverseCommand";
	command_count = (prop_get (bundle, "I<-sis", &vt_rvalue, vt_key))
					? vt_rvalue.integer : 0;

	/* Iterate over commands. */
	for (command = 0; command < command_count; command++)
	    {
		sc_char		*pattern;
		sc_uint16	index;
		sc_bool		status;

		/* Get the command pattern. */
		vt_key[3].integer = command;
		pattern = prop_get_string (bundle, "S<-sisi", vt_key);

		/*
		 * Look for patterns starting with a '#'.  If one found,
		 * check against "task command functions", otherwise do the
		 * normal parser match.
		 */
		for (index = 0; isspace (pattern[index])
						&& pattern[index] != NUL; )
			index++;
		if (pattern[index] == SPECIAL_PATTERN)
			status = run_special_task_function (pattern, gamestate);
		else
			status = uip_match (pattern, string, gamestate);

		/* If command matched, run the task, and return if it runs.*/
		if (status)
		    {
			if (task_run_task (gamestate, task, forwards))
				return TRUE;
		    }
	    }

	/* No matching task ran. */
	return FALSE;
}


/*
 * run_task_is_unrestricted()
 * run_task_commands()
 *
 * Compare a user input string against commands defined by the game tasks,
 * and action any command.  Return TRUE on a successful match.  Of the
 * runnable tasks, selects either only unrestricted or restricted ones
 * (in which case restriction failures count as success).
 *
 * run_task_commands() is called from the library to try to run "get " and
 * "drop " game commands for standard get/drop handlers and get_all/drop_all
 * handlers.
 */
static sc_bool
run_task_is_unrestricted (sc_gamestateref_t gamestate, sc_int32 task)
{
	sc_bool		restrictions_passed;
	sc_char		*fail_message;

	/* Check restrictions, ignoring fail message. */
	if (!restr_evaluate_task_restrictions (gamestate, task,
				&restrictions_passed, &fail_message))
	    {
		sc_error ("run_task_is_restricted:"
				" restrictions error, %ld\n", task);
		return FALSE;
	    }

	/* Return TRUE if task is not restricted, FALSE if it is. */
	return restrictions_passed;
}

sc_bool
run_task_commands (sc_gamestateref_t gamestate, const sc_char *string,
		sc_bool unrestricted)
{
	sc_int32	task;

	/*
	 * Iterate over every task, ignoring those not runnable, and those
	 * runnable but not matching the requested restriction state.
	 */
	for (task = 0; task < gamestate->task_count; task++)
	    {
		if (task_can_run_task (gamestate, task)
			&& unrestricted
				== run_task_is_unrestricted (gamestate, task))
		    {
			if (run_task_command (gamestate, string, task, TRUE))
				return TRUE;
			if (run_task_command (gamestate, string, task, FALSE))
				return TRUE;
		    }
	    }

	/* No runnable and matching task ran. */
	return FALSE;
}


/* Structure used to associate a pattern with a handler function. */
typedef struct sc_commands_s {
	const sc_char		*command;
	sc_bool			(*handler) (sc_gamestateref_t gamestate);
} sc_commands_t;
typedef sc_commands_t	*sc_commandsref_t;

/* Movement commands for the four point compass. */
static sc_commands_t	MOVE_COMMANDS_4[] = {
	{ "{go} {to} {the} [north/n]",		lib_cmd_go_north },
	{ "{go} {to} {the} [east/e]",		lib_cmd_go_east },
	{ "{go} {to} {the} [south/s]",		lib_cmd_go_south },
	{ "{go} {to} {the} [west/w]",		lib_cmd_go_west },
	{ "{go} {to} {the} [up/u]",		lib_cmd_go_up },
	{ "{go} {to} {the} [down/d]",		lib_cmd_go_down },
	{ "{go} {to} {the} [in]",		lib_cmd_go_in },
	{ "{go} {to} {the} [out/o]",		lib_cmd_go_out },
	{ NULL,					NULL }
};

/* Movement commands for the eight point compass. */
static sc_commands_t	MOVE_COMMANDS_8[] = {
	{ "{go} {to} {the} [north/n]",		lib_cmd_go_north },
	{ "{go} {to} {the} [east/e]",		lib_cmd_go_east },
	{ "{go} {to} {the} [south/s]",		lib_cmd_go_south },
	{ "{go} {to} {the} [west/w]",		lib_cmd_go_west },
	{ "{go} {to} {the} [up/u]",		lib_cmd_go_up },
	{ "{go} {to} {the} [down/d]",		lib_cmd_go_down },
	{ "{go} {to} {the} [in]",		lib_cmd_go_in },
	{ "{go} {to} {the} [out/o]",		lib_cmd_go_out },
	{ "{go} {to} {the} [northeast/north-east/ne]",
						lib_cmd_go_northeast },
	{ "{go} {to} {the} [southeast/south-east/se]",
						lib_cmd_go_southeast },
	{ "{go} {to} {the} [northwest/north-west/nw]",
						lib_cmd_go_northwest },
	{ "{go} {to} {the} [southwest/south-west/sw]",
						lib_cmd_go_southwest },
	{ NULL,					NULL }
};

/* General library commands that have an effect on the gamestate. */
static sc_commands_t	GENERAL_COMMANDS[] = {

	/* Inventory, and general investigation of surroundings. */
	{ "[inventory/inv/i]",			lib_cmd_inventory },
	{ "[x/ex/exam/examine/l/look] {{at} {the} [room/location]}",
						lib_cmd_look },
	{ "[x/ex/exam/examine/look at/look] %object%",
						lib_cmd_examine_object },
	{ "[x/ex/exam/examine/look at/look] %character%",
						lib_cmd_examine_npc },
	{ "[x/ex/exam/examine/look at/look] [me/self/myself]",
						lib_cmd_examine_self },

	/* Acquisition of and disposal of inventory. */
	{ "[get/take/pick up] %object%",	lib_cmd_get_object },
	{ "pick %object% up",			lib_cmd_get_object },
	{ "[get/take/pick up] %character%",	lib_cmd_get_npc },
	{ "[get/take] all",			lib_cmd_get_all },
	{ "pick [up all/all up]",		lib_cmd_get_all },
	{ "[get/take/remove] %object% from %text%",
						lib_cmd_get_object_from },
	{ "[[get/take/remove] all from/empty] %object%",
						lib_cmd_get_all_from },
	{ "[drop/put down] %object%",		lib_cmd_drop_object },
	{ "put %object% down",			lib_cmd_drop_object },
	{ "drop all",				lib_cmd_drop_all },
	{ "put [down all/all down]",		lib_cmd_drop_all },

	/* Manipulating selected objects. */
	{ "put %object% [on/onto/on top of] %text%",
						lib_cmd_put_object_on },
	{ "put %object% [in/into/inside] %text%",
						lib_cmd_put_object_in },
	{ "open %object%",			lib_cmd_open_object },
	{ "close %object%",			lib_cmd_close_object },
	{ "unlock %object% with %text%",	lib_cmd_unlock_object_with },
	{ "lock %object% with %text%",		lib_cmd_lock_object_with },
	{ "unlock %object%",			lib_cmd_unlock_object },
	{ "lock %object%",			lib_cmd_lock_object },
	{ "read %object%",			lib_cmd_read_object },
	{ "give %object% to %character%",	lib_cmd_give_object_npc },
	{ "sit [on/in] %object%",		lib_cmd_sit_on_object },
	{ "stand on %object%",			lib_cmd_stand_on_object },
	{ "[lie/lay] on %object%",		lib_cmd_lie_on_object },
	{ "sit {down/on {the} [ground/floor]}",	lib_cmd_sit_on_floor },
	{ "stand {up/on {the} [ground/floor]}",	lib_cmd_stand_on_floor },
	{ "[lie/lay] {down/on {the} [ground/floor]}",
						lib_cmd_lie_on_floor },
	{ "eat %object%",			lib_cmd_eat_object },

	/* Dressing up, and dressing down. */
	{ "[wear/put on/don] %object%",		lib_cmd_wear_object },
	{ "put %object% on",			lib_cmd_wear_object },
	{ "[remove/take off/doff] %object%",	lib_cmd_remove_object },
	{ "take %object% off",			lib_cmd_remove_object },
	{ "[remove/take off/doff] all",		lib_cmd_remove_all },
	{ "take all off",			lib_cmd_remove_all },

	/* Selected NPC interactions and conversation. */
	{ "ask %character% about %text%",	lib_cmd_ask_npc_about },
	{ "kiss %character%",			lib_cmd_kiss_npc },
	{ "[attack/hit/kill/slap/shoot/stab] %character% with %object%",
						lib_cmd_attack_npc_with },
	{ "[attack/hit/kill/slap/shoot/stab/punch/kick] %character%",
						lib_cmd_attack_npc },

	/* Waiting, and miscellaneous administrative commands. */
	{ "[exit/exits]",			lib_cmd_print_room_exits },
	{ "[goto/go {to}] *",			lib_cmd_print_room_exits },
	{ "[wait/z]",				lib_cmd_wait },
	{ "save",				lib_cmd_save },
	{ "[restore/load]",			lib_cmd_restore },
	{ "restart",				lib_cmd_restart },
	{ "[again/g]",				lib_cmd_again },
	{ "[quit/q]",				lib_cmd_quit },
	{ "turns",				lib_cmd_turns },
	{ "score",				lib_cmd_score },
	{ "undo",				lib_cmd_undo },
	{ "[hint/hints]",			lib_cmd_hints },
	{ "verbose",				lib_cmd_verbose },
	{ "brief",				lib_cmd_brief },
	{ "[notify/notification] on",		lib_cmd_notify_on },
	{ "[notify/notification] off",		lib_cmd_notify_off },
	{ "[notify/notification]",		lib_cmd_notify },
	{ "help",				lib_cmd_help },
	{ "[gpl/license]",			lib_cmd_license },
	{ "[about/info/information/author]",	lib_cmd_information },
	{ "[clear/cls/clr]",			lib_cmd_clear },
	{ "version",				lib_cmd_version },

	{ "[locate/where [is/are]/where/find] %object%",
						lib_cmd_locate_object },
	{ "[locate/where [is/are]/where/find] %character%",
						lib_cmd_locate_npc },

	{ "{#}debug{ger}",			debug_cmd_debugger },
	{ NULL,					NULL }
};

/* General library commands that have no effect on the gamestate. */
static sc_commands_t	STANDARD_COMMANDS[] = {

	/* Standard response commands; no real action, just output. */
	{ "[get/take/pick up] *",		lib_cmd_get_other },
	{ "open *",				lib_cmd_open_other },
	{ "close *",				lib_cmd_close_other },
	{ "give *",				lib_cmd_give_other },
	{ "[remove/take off/doff] *",		lib_cmd_remove_other },
	{ "[drop/put down] *",			lib_cmd_drop_other },
	{ "[wear/put on/don] *",		lib_cmd_wear_other },
	{ "[shit/fuck/bastard/cunt/crap/hell/shag/bollocks/bugger] *",
						lib_cmd_profanity },
	{ "[x/examine/look at/look] *",		lib_cmd_examine_other },
	{ "[locate/where [is/are]/where/find] *",
						lib_cmd_locate_other },
	{ "[cp/mv/ln/ls] *",			lib_cmd_unix_like },
	{ "dir *",				lib_cmd_dos_like },
	{ "ask %object% *",			lib_cmd_ask_object },
	{ "block %object%",			lib_cmd_block_object },
	{ "block *",				lib_cmd_block_other },
	{ "[break/destroy/smash] %character%",	lib_cmd_break_npc },
	{ "[break/destroy/smash] %object%",	lib_cmd_break_object },
	{ "buy *",				lib_cmd_buy },
	{ "feed %object%",			lib_cmd_feed_object },
	{ "feed *",				lib_cmd_feed_other },
	{ "clean %object%",			lib_cmd_clean_object },
	{ "clean *",				lib_cmd_clean_other },
	{ "climb %object%",			lib_cmd_climb_object },
	{ "climb *",				lib_cmd_climb_other },
	{ "cry *",				lib_cmd_cry },
	{ "cut %object%",			lib_cmd_cut_object },
	{ "cut *",				lib_cmd_cut_other },
	{ "dance *",				lib_cmd_dance },
	{ "feed %character%",			lib_cmd_feed_npc },
	{ "feed %object%",			lib_cmd_feed_object },
	{ "feed *",				lib_cmd_feed_other },
	{ "feel *",				lib_cmd_feel },
	{ "fix %object%",			lib_cmd_fix_object },
	{ "fix *",				lib_cmd_fix_other },
	{ "fly *",				lib_cmd_fly },
	{ "hint *",				lib_cmd_hint },
	{ "hit %object%",			lib_cmd_hit_object },
	{ "hit *",				lib_cmd_hit_other },
	{ "hum *",				lib_cmd_hum },
	{ "jump *",				lib_cmd_jump },
	{ "kick %object%",			lib_cmd_kick_object },
	{ "kick *",				lib_cmd_kick_other },
	{ "kiss %object%",			lib_cmd_kiss_object },
	{ "kiss *",				lib_cmd_kiss_other },
	{ "light %object%",			lib_cmd_light_object },
	{ "light *",				lib_cmd_light_other },
	{ "listen *",				lib_cmd_listen },
	{ "mend %object%",			lib_cmd_mend_object },
	{ "mend *",				lib_cmd_mend_other },
	{ "move %object%",			lib_cmd_move_object },
	{ "move *",				lib_cmd_move_other },
	{ "please *",				lib_cmd_please },
	{ "press %object%",			lib_cmd_press_object },
	{ "press *",				lib_cmd_press_other },
	{ "pull %object%",			lib_cmd_pull_object },
	{ "pull *",				lib_cmd_pull_other },
	{ "punch *",				lib_cmd_punch },
	{ "push %object%",			lib_cmd_push_object },
	{ "push *",				lib_cmd_push_other },
	{ "repair %object%",			lib_cmd_repair_object },
	{ "repair *",				lib_cmd_repair_other },
	{ "run *",				lib_cmd_run },
	{ "say *",				lib_cmd_say },
	{ "sell %object%",			lib_cmd_sell_object },
	{ "sell *",				lib_cmd_sell_other },
	{ "shake %object%",			lib_cmd_shake_object },
	{ "shake *",				lib_cmd_shake_other },
	{ "shout *",				lib_cmd_shout },
	{ "sing *",				lib_cmd_sing },
	{ "sleep *",				lib_cmd_sleep },
	{ "smell %character%",			lib_cmd_smell_npc },
	{ "smell %object%",			lib_cmd_smell_object },
	{ "suck %object%",			lib_cmd_suck_object },
	{ "suck *",				lib_cmd_suck_other },
	{ "talk *",				lib_cmd_talk },
	{ "thank *",				lib_cmd_thank },
	{ "turn %object%",			lib_cmd_turn_object },
	{ "turn *",				lib_cmd_turn_other },
	{ "unblock %object%",			lib_cmd_unblock_object },
	{ "unblock *",				lib_cmd_unblock_other },
	{ "wash %object%",			lib_cmd_wash_object },
	{ "wash *",				lib_cmd_wash_other },
	{ "whistle *",				lib_cmd_whistle },
	{ "[why/when/what/can/how] *",		lib_cmd_interrogation },
	{ "xyzzy *",				lib_cmd_xyzzy },
	{ "yes *",				lib_cmd_yes },
	{ "* %object%",				lib_cmd_verb_object },
	{ "* %character%",			lib_cmd_verb_npc },

	{ NULL,					NULL }
};


/*
 * run_general_commands()
 * run_standard_commands()
 *
 * Compare a user input string against the standard commands recognized
 * by the library, and action any command.  Returns TRUE if the command
 * was a request by the user to quit the game.
 */
static sc_bool
run_general_commands (sc_gamestateref_t gamestate, const sc_char *string)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_bool			eightpointcompass;
	sc_commandsref_t	command = NULL;

	/* Select the appropriate movement commands. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "EightPointCompass";
	eightpointcompass = prop_get_boolean (bundle, "B<-ss", vt_key);
	if (eightpointcompass)
		command = MOVE_COMMANDS_8;
	else
		command = MOVE_COMMANDS_4;

	/* Search movement commands first. */
	for ( ; command->command != NULL; command++)
	    {
		if (uip_match (command->command, string, gamestate))
		    {
			/* Run the handler.  If it succeeds, return. */
			if (command->handler (gamestate))
				return TRUE;
		    }
	    }

	/* Now do the same for the general library commands. */
	for (command = GENERAL_COMMANDS; command->command != NULL; command++)
	    {
		if (uip_match (command->command, string, gamestate))
		    {
			/* Run the handler.  If it succeeds, return. */
			if (command->handler (gamestate))
				return TRUE;
		    }
	    }

	/* Nothing appeared to match the command. */
	return FALSE;
}

static sc_bool
run_standard_commands (sc_gamestateref_t gamestate, const sc_char *string)
{
	sc_commandsref_t	command = NULL;

	/* Try to run all standard library commands. */
	for (command = STANDARD_COMMANDS; command->command != NULL; command++)
	    {
		if (uip_match (command->command, string, gamestate))
		    {
			/* Run the handler.  If it succeeds, return. */
			if (command->handler (gamestate))
				return TRUE;
		    }
	    }

	/* Nothing appeared to match the command. */
	return FALSE;
}


/*
 * run_update_status()
 *
 * Update the gamestate's current room and status line strings.
 */
static void
run_update_status (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[2];
	sc_char			*name, *status, *filtered;
	sc_bool			statusbox;

	/* Get the current room name, and filter and untag it. */
	name = lib_get_room_name (gamestate, gamestate->playerroom);
	filtered = pf_filter (name, vars, bundle);
	pf_strip_tags (filtered);

	/* Free any existing room name. */
	if (gamestate->current_room_name != NULL)
		sc_free (gamestate->current_room_name);

	/* Save this room name. */
	gamestate->current_room_name = filtered;

	/* See if the game does a status box. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "StatusBox";
	statusbox = prop_get_boolean (bundle, "B<-ss", vt_key);
	if (statusbox)
	    {
		/* Get the status line, and filter and untag it. */
		vt_key[1].string = "StatusBoxText";
		status = prop_get_string (bundle, "S<-ss", vt_key);
		filtered = pf_filter (status, vars, bundle);
		pf_strip_tags (filtered);
	    }
	else
		/* No status line, so return NULL. */
		filtered = NULL;

	/* Free any existing status line. */
	if (gamestate->status_line != NULL)
		sc_free (gamestate->status_line);

	/* Save this status text. */
	gamestate->status_line = filtered;
}


/*
 * run_notify_score_change()
 *
 * Print an indication of any score change, if appropriate.  The change is
 * detected by comparing against the undo gamestate.
 */
static void
run_notify_score_change (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_gamestateref_t	undo	= gamestate->undo;
	sc_char			buffer[16];
	assert (gs_is_gamestate_valid (undo));

	/* Do nothing if no undo available, or if notification is off. */
	if (!gamestate->undo_available
			|| !gamestate->notify_score_change)
		return;

	/* Note any change in the score. */
	if (gamestate->score > undo->score)
	    {
		pf_buffer_string (filter, "\n(Your score has increased by ");
		sprintf (buffer, "%ld", gamestate->score - undo->score);
		pf_buffer_string (filter, buffer);
		pf_buffer_string (filter, ")\n");
	    }
	else if (gamestate->score < undo->score)
	    {
		pf_buffer_string (filter, "\n(Your score has decreased by ");
		sprintf (buffer, "%ld", undo->score - gamestate->score);
		pf_buffer_string (filter, buffer);
		pf_buffer_string (filter, ")\n");
	    }
}


/*
 * run_player_input()
 *
 * Take a line of player input and buffer it.  Split the line into
 * elements separated by commas.  For the first element, try to match
 * it to either a task or a standard command, and return TRUE if it
 * matched, FALSE otherwise.
 *
 * On subsequent calls, successively work with the next line element
 * until none remain.  In this case, prompt for more player input and
 * continue as above.
 *
 * For the case of "again" or "g", rerun the last successful command
 * element.
 *
 * One extra special special case; if called with a game that is not
 * running, this is a signal to reset all noted line input to initial
 * conditions, and just return.  Sorry about the ugliness.
 */
static sc_bool
run_player_input (sc_gamestateref_t gamestate)
{
	static sc_char		line_buffer[LINE_BUFFER_SIZE];
	static sc_bool		line_avail = FALSE;
	static sc_uint16	line_end   = 0;
	static sc_uint16	start	   = 0;
	static sc_uint16	end	   = 0;
	static sc_char		prior_element[LINE_BUFFER_SIZE];
	static sc_bool		prior_avail = FALSE;

	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_char			line_element[LINE_BUFFER_SIZE];
	sc_bool			rerunning;
	sc_char			*filtered;
	sc_bool			status;

	/* Special case; reset statics if the game isn't running. */
	if (!gamestate->is_running)
	    {
		line_avail	= FALSE;
		prior_avail	= FALSE;
		return TRUE;
	    }

	/* See if the player asked to rerun the last element. */
	if (gamestate->do_again)
	    {
		gamestate->do_again = FALSE;

		/* Check there is a last element to repeat. */
		if (!prior_avail)
		    {
			pf_buffer_string (filter,
					"You can hardly repeat that.\n");
			return FALSE;
		    }

		/* Make the last element the current input element. */
		strcpy (line_element, prior_element);
		rerunning = TRUE;
	    }
	else
	    {
		/* If there's none buffered, read a new line of player input. */
		if (!line_avail)
		    {
			if_read_line (line_buffer, sizeof (line_buffer));

			/* Find line end and set up for interpreting. */
			line_end = strlen (line_buffer);
			assert (line_buffer[line_end] == NUL);

			line_avail = TRUE;
			start = end = 0;
		    }
		else
			pf_buffer_character (filter, '\n');

		/*
		 * Find the end of the next input line element.  If the
		 * input line is empty, we're already set, with start and
		 * end correct for the degenerate case.
		 */
		if (!sc_strempty (line_buffer))
		    {
			for (end = start + 1;
					line_buffer[end] != COMMA
					&& end < line_end; )
				end++;
		    }

		/* Make this the current input element. */
		strncpy (line_element, line_buffer + start, end - start);
		line_element[end - start] = NUL;
		rerunning = FALSE;
	    }

	/* Copy the current gamestate to the temporary undo buffer. */
	gs_copy (gamestate->temporary, gamestate);

	/* Filter the input element for synonyms. */
	filtered = pf_filter_input (line_element, bundle);
	if (sc_strcasecmp (filtered, line_element) != 0)
	    {
		if_print_tag (SC_TAG_ITALICS, "");
		if_print_character ('[');
		if_print_string (filtered);
		if_print_character (']');
		if_print_tag (SC_TAG_ENDITALICS, "");
		if_print_character ('\n');
	    }

	/*
	 * Try the line element against command matchers; run unrestricted
	 * game task commands first, then general commands (ones that affect
	 * the gamestate), then look for restricted (and failing) game tasks,
	 * and finally the catch-all standard library commands.
	 */
	status = run_task_commands (gamestate, filtered, TRUE);
	if (!status)
		status = run_general_commands (gamestate, filtered);
	if (!status)
		status = run_task_commands (gamestate, filtered, FALSE);
	if (!status)
		status = run_standard_commands (gamestate, filtered);
	if (!status)
	    {
		/* Only complain on non-empty input line elements. */
		if (!sc_strempty (filtered))
		    {
			sc_vartype_t	vt_key[2];
			sc_char		*message;

			/* Command line not understood. */
			var_set_ref_text (vars, line_element);
			vt_key[0].string = "Globals";
			vt_key[1].string = "DontUnderstand";
			message = prop_get_string (bundle, "S<-ss", vt_key);
			pf_buffer_string (filter, message);
			pf_buffer_character (filter, '\n');

			/*
			 * On a line element that's not understood, throw
			 * out any remaining input line elements.
			 */
			line_avail = FALSE;
			sc_free (filtered);
			return status;
		    }
	    }
	else
	    {
		/*
		 * Unless administrative, copy the temporary gamestate into
		 * the undo buffer, and flag the undo buffer as available.
		 */
		if (!gamestate->is_admin)
		    {
			gs_copy (gamestate->undo, gamestate->temporary);
			gamestate->undo_available = TRUE;
		    }
	    }
	sc_free (filtered);

	/*
	 * Special case restart and restore commands; throw out any
	 * remaining input.
	 */
	if (gamestate->do_restart
			|| gamestate->do_restore)
	    {
		line_avail = FALSE;
		return status;
	    }

	/* If this was not a repeated command, advance input element. */
	if (!rerunning)
	    {
		/*
		 * If at line end, mark for a read on next call.  Otherwise,
		 * move the element start over the element just handled.
		 */
		if (end == line_end)
			line_avail = FALSE;
		else
		    {
			/* Advance element start. */
			start = end + 1;

			/*
			 * If start is now at or beyond the line end, no
			 * more buffered input is available.  This catches
			 * lines that end with a comma.
			 */
			if (start >= line_end)
				line_avail = FALSE;
		    }

		/*
		 * Unless an "again" command or empty, note this line
		 * element as prior input.  "Again" commands show up as
		 * do_again set in the gamestate, where it wasn't when
		 * we entered this routine.
		 */
		if (!gamestate->do_again
				&& !sc_strempty (line_element))
		    {
			strcpy (prior_element, line_element);
			prior_avail = TRUE;
		    }
	    }

	/* Return the match status. */
	return status;
}


/*
 * run_main_loop()
 *
 * Main interpreter loop.
 */
static void
run_main_loop (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;

	/*
	 * See if this is the very start of a new game.  If the turns
	 * count is not zero, then we're resuming a game that's already
	 * been partially run.
	 */
	if (gamestate->turns == 0)
	    {
		sc_vartype_t		vt_key[2];
		sc_char			*gamename, *startuptext;
		sc_bool			disp_first_room, battle_system;

		/* Initial clear screen. */
		pf_buffer_tag (filter, SC_TAG_CLS);

		/* If battle system and no debugger display a warning. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "BattleSystem";
		battle_system = prop_get_boolean (bundle, "B<-ss", vt_key);
		if (battle_system
				&& !debug_get_enabled (gamestate))
		    {
			lib_warn_battle_system (gamestate);
			pf_buffer_tag (filter, SC_TAG_CLS);
		    }

		/* Print the game name. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "GameName";
		gamename = prop_get_string (bundle, "S<-ss", vt_key);
		pf_buffer_string (filter, gamename);
		pf_buffer_character (filter, '\n');

		/* Print the game header. */
		vt_key[0].string = "Header";
		vt_key[1].string = "StartupText";
		startuptext = prop_get_string (bundle, "S<-ss", vt_key);
		pf_buffer_string (filter, startuptext);
		pf_buffer_character (filter, '\n');

		/* If flagged, describe the initial room. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "DispFirstRoom";
		disp_first_room = prop_get_boolean (bundle, "B<-ss", vt_key);
		if (disp_first_room)
			lib_cmd_look (gamestate);

		/* Handle any introductory resources. */
		vt_key[0].string = "Globals";
		vt_key[1].string = "IntroRes";
		res_handle_resource (gamestate, "ss", vt_key);

		/*
		 * This may not be the very first time this gamestate has
		 * been used, for example saving a game right at the start.
		 * Caught by looking to see if the player room is already
		 * marked as seen.
		 */
		if (!gs_get_room_seen (gamestate, gamestate->playerroom))
		    {
			/* Set initial values for NPC and object states. */
			npc_setup_initial (gamestate);
			obj_setup_initial (gamestate);

			/* Nudge events and NPCs. */
			evt_tick_events (gamestate);
			npc_tick_npcs (gamestate);

			/* Note the initial room as visited. */
			gs_set_room_seen (gamestate,
						gamestate->playerroom, TRUE);
		    }
	    }

	/*
	 * Notify the debugger that the game has started or restarted.
	 * This is a chance to set watchpoints to catch game startup actions.
	 */
	debug_game_started (gamestate);

	/*
	 * Game loop, exits either when a command parser handler sets the
	 * gamestate running flag to FALSE, or by call to run_quit().
	 */
	while (gamestate->is_running)
	    {
		sc_bool		status;

		/*
		 * Synchronize any resources in use; do this before flushing
		 * so that any appropriate graphics/sound appear before waits
		 * or waitkey tag delays invoked by flushing the printfilter.
		 */
		res_sync_resources (gamestate);

		/*
		 * Flush printfilter of any accumulated output, and clear
		 * any prior notion of administrative commands from input.
		 */
		pf_flush (filter, vars, bundle);
		gamestate->is_admin = FALSE;

		/* If waitturns is zero, accept and try a command. */
		if (gamestate->waitturns == 0)
		    {
			/* Not waiting, so handle a player input line. */
			run_update_status (gamestate);
			status = run_player_input (gamestate);

			/*
			 * If waitturns is now set, decrement it, as this
			 * turn counts as one of them.
			 */
			if (gamestate->waitturns > 0)
				gamestate->waitturns--;
		    }
		else
		    {
			/*
			 * Currently "waiting"; decrement wait turns, then
			 * run a turn having taken no input.
			 */
			gamestate->waitturns--;
			status = TRUE;
		    }

		/*
		 * Do usual turn stuff unless either something stopped the
		 * game, or the last command didn't match, or the last command
		 * did match but was administrative.
		 */
		if (gamestate->is_running
				&& status && !gamestate->is_admin)
		    {
			/* Increment turn counter. */
			gamestate->turns++;

			/* Nudge events and NPCs. */
			evt_tick_events (gamestate);
			npc_tick_npcs (gamestate);

			/* Update NPC and object states. */
			npc_turn_update (gamestate);
			obj_turn_update (gamestate);

			/* Note the current room as visited. */
			gs_set_room_seen (gamestate,
						gamestate->playerroom, TRUE);

			/* Indicate any score change, if applicable. */
			run_notify_score_change (gamestate);

			/* Give the debugger a chance to catch watchpoints. */
			debug_turn_update (gamestate);
		    }
	    }


	/*
	 * Final status update, for games that vary it on completion, then
	 * notify the debugger that the game has ended, to let it make a
	 * last watchpoint scan and offer the dialog if appropriate.
	 */
	run_update_status (gamestate);
	debug_game_ended (gamestate);

	/* Flush printfilter on game-instigated loop exit. */
	pf_flush (filter, vars, bundle);

	/*
	 * Reset static variables inside run_player_input() with a call
	 * to it with is_running false; this is a special case.
	 */
	assert (!gamestate->is_running);
	run_player_input (gamestate);
}


/*
 * run_create_commmon()
 * run_create()
 * run_create_file()
 * run_create_filename()
 *
 * Create a game context from a filename, stream, or callback.  Adapters
 * to create a TAF and properties bundle, and encapsulate in a context.
 */
static sc_gamestateref_t
run_create_common (sc_tafref_t taf, sc_uint32 trace_flags)
{
	sc_prop_setref_t	bundle;
	sc_var_setref_t		vars;
	sc_var_setref_t		temporary_vars, undo_vars;
	sc_printfilterref_t	filter;
	sc_gamestateref_t	gamestate;
	sc_gamestateref_t	temporary_gamestate, undo_gamestate;

	/* Set up tracing in modules that support it. */
	if (trace_flags & SC_TR_TRACE_PARSE)
		parse_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_PROPERTIES)
		prop_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_VARIABLES)
		var_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_PARSER)
		uip_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_LIBRARY)
		lib_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_EVENTS)
		evt_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_NPCS)
		npc_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_OBJECTS)
		obj_debug_trace (TRUE);
	if (trace_flags & SC_TR_TRACE_TASKS)
	    {
		task_debug_trace (TRUE);
		restr_debug_trace (TRUE);
	    }
	if (trace_flags & SC_TR_TRACE_PRINTFILTER)
		pf_debug_trace (TRUE);

	/* Dump out the TAF passed in if requested. */
	if (trace_flags & SC_TR_DUMP_TAF)
		taf_debug_dump (taf);

	/*
	 * Create a properties bundle, and parse the TAF data into it.
	 * If requested, dump out the bundle.
	 */
	bundle = prop_create (taf);
	if (bundle == NULL)
	    {
		sc_error ("run_create_common: error parsing game data\n");
		taf_destroy (taf);
		return NULL;
	    }
	if (trace_flags & SC_TR_DUMP_PROPERTIES)
		prop_debug_dump (bundle);

	/*
	 * Create a set of variables from the bundle, and dump them too
	 * if requested.
	 */
	vars = var_create (bundle);
	if (trace_flags & SC_TR_DUMP_VARIABLES)
		var_debug_dump (vars);

	/* Create a printfilter for the game. */
	filter = pf_create ();

	/*
	 * Create an initial game state, and register it with variables.
	 * Also, create undo buffers, and initialize them in the same way.
	 */
	gamestate = gs_create (vars, bundle, filter);
	var_register_gamestate (vars, gamestate);

	temporary_vars = var_create (bundle);
	temporary_gamestate = gs_create (temporary_vars, bundle, filter);
	var_register_gamestate (temporary_vars, temporary_gamestate);

	undo_vars = var_create (bundle);
	undo_gamestate = gs_create (undo_vars, bundle, filter);
	var_register_gamestate (undo_vars, undo_gamestate);

	/* Add the undo buffers to the gamestate, and return it. */
	gamestate->temporary = temporary_gamestate;
	gamestate->undo = undo_gamestate;
	return gamestate;
}

sc_gamestateref_t
run_create (sc_uint16 (*callback) (void *, sc_byte *, sc_uint16),
		void *opaque, sc_uint32 trace_flags)
{
	sc_tafref_t		taf;
	assert (callback != NULL);

	taf = taf_create (callback, opaque, TRUE);
	if (taf != NULL)
		return run_create_common (taf, trace_flags);
	return NULL;
}

sc_gamestateref_t
run_create_file (FILE *stream, sc_uint32 trace_flags)
{
	sc_tafref_t		taf;
	assert (stream != NULL);

	taf = taf_create_file (stream, TRUE);
	if (taf != NULL)
		return run_create_common (taf, trace_flags);
	return NULL;
}

sc_gamestateref_t
run_create_filename (const sc_char *filename, sc_uint32 trace_flags)
{
	sc_tafref_t		taf;
	assert (filename != NULL);

	taf = taf_create_filename (filename, TRUE);
	if (taf != NULL)
		return run_create_common (taf, trace_flags);
	return NULL;
}


/*
 * run_restart_handler()
 *
 * Return a game context to initial states to restart a game.
 */
static void
run_restart_handler (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_gamestateref_t	new_gamestate;
	sc_var_setref_t		new_vars;

	/*
	 * Create a fresh set of variables from the current gamestate
	 * properties, then a new gamestate using these variables and
	 * existing properties and printfilter.
	 */
	new_vars = var_create (bundle);
	new_gamestate = gs_create (new_vars, bundle, filter);
	var_register_gamestate (new_vars, new_gamestate);

	/*
	 * Overwrite the dynamic parts of the current gamestate with the
	 * new one.
	 */
	new_gamestate->temporary = gamestate->temporary;
	new_gamestate->undo = gamestate->undo;
	gs_copy (gamestate, new_gamestate);

	/* Destroy invalid game status strings. */
	if (gamestate->current_room_name != NULL)
	    {
		sc_free (gamestate->current_room_name);
		gamestate->current_room_name = NULL;
	    }
	if (gamestate->status_line != NULL)
	    {
		sc_free (gamestate->status_line);
		gamestate->status_line = NULL;
	    }

	/*
	 * Now it's safely copied, destroy the temporary new gamestate,
	 * and its associated variable set.
	 */
	gs_destroy (new_gamestate);
	var_destroy (new_vars);

	/* Reset resources handling. */
	res_cancel_resources (gamestate);
}


/*
 * run_quit_handler()
 *
 * Tidy up printfilter and input statics on game quit.
 */
static void
run_quit_handler (sc_gamestateref_t gamestate)
{
	sc_printfilterref_t	filter	= gamestate->filter;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;

	/* Flush printfilter of any dangling output. */
	pf_flush (filter, vars, bundle);

	/* Cancel any active resources. */
	res_cancel_resources (gamestate);

	/*
	 * Make the special call to reset all of the static variables
	 * inside run_player_input().
	 */
	assert (!gamestate->is_running);
	run_player_input (gamestate);
}


/*
 * run_interpret()
 *
 * Intepret the game in a game context.
 */
void
run_interpret (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Verify the game is not already running, and is runnable. */
	if (gamestate->is_running)
	    {
		sc_error ("run_interpret: game is already running\n");
		return;
	    }
	if (gamestate->has_completed)
	    {
		sc_error ("run_interpret: game has already completed\n");
		return;
	    }

	/* Refuse to run a game with no rooms. */
	if (gamestate->room_count == 0)
	    {
		sc_error ("run_interpret: game contains no rooms\n");
		return;
	    }

	/* Run the main interpreter loop until no more restarts. */
	gamestate->is_running = TRUE;
	do
	    {
		/* Run the game until some form of halt is requested. */
		if (setjmp (gamestate->quitter) == 0)
		    {
			run_main_loop (gamestate);
		    }

		/* See if the halt included a restart request. */
		if (gamestate->do_restart)
		    {
			/* Cancel, then handle the restart request. */
			gamestate->do_restart = FALSE;
			run_restart_handler (gamestate);

			/* Set running, and continue as if nothing happened. */
			gamestate->is_running = TRUE;
		    }

		/* See if we just executed a restore. */
		if (gamestate->do_restore)
		    {
			/* Invalidate the undo buffer. */
			gamestate->undo_available = FALSE;

			/* Cancel the restore note, and set running again. */
			gamestate->do_restore = FALSE;
			gamestate->is_running = TRUE;

			/*
			 * Resources handling?  Arguably we should re-offer
			 * resources active when the game was saved, but I
			 * can't see how this can be achieved with Adrift the
			 * way it is.  Canceling is too broad, so I'll go
			 * here with just stopping sounds (in case looping).
			 * TODO Rationalize what happens here.
			 */
			gamestate->stop_sound = TRUE;
		    }
	    }
	while (gamestate->is_running);

	/* Tidy up the printfilter and input statics. */
	run_quit_handler (gamestate);
}


/*
 * run_destroy()
 *
 * Destroy a game context, and free all resources.
 */
void
run_destroy (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Can't destroy the context of a running game. */
	if (gamestate->is_running)
	    {
		sc_error ("run_destroy: game is running, stop it first\n");
		return;
	    }

	/*
	 * Cancel any game state debugger -- this frees its resources.
	 * Only the primary gamestate may have acquired a debugger.
	 */
	debug_set_enabled (gamestate, FALSE);
	assert (!debug_get_enabled (gamestate->temporary));
	assert (!debug_get_enabled (gamestate->undo));

	/*
	 * Destroy the game state, variables, properties bundle, undo
	 * buffers and their variables, and filter.
	 */
	var_destroy (gamestate->vars);
	prop_destroy (gamestate->bundle);
	pf_destroy (gamestate->filter);
	var_destroy (gamestate->temporary->vars);
	gs_destroy (gamestate->temporary);
	var_destroy (gamestate->undo->vars);
	gs_destroy (gamestate->undo);
	gs_destroy (gamestate);
}


/*
 * run_quit()
 *
 * Quits a running game.  This function calls a longjump to act as if
 * run_main_loop() returned, and so never returns to its caller.
 */
void
run_quit (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Can't quit a non-running game. */
	if (!gamestate->is_running)
	    {
		sc_error ("run_quit: game is not running\n");
		return;
	    }

	gamestate->is_running = FALSE;
	longjmp (gamestate->quitter, 1);
	sc_fatal ("run_quit: unable to quit cleanly\n");
}


/*
 * run_restart()
 *
 * Restarts either a running or a stopped game.  For running games, this
 * function calls a longjump to act as if run_main_loop() returned, and so
 * never returns to its caller.  For stopped games, it returns.
 */
void
run_restart (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/*
	 * If the game is not running, restart locally, ensure stopped,
	 * and return.
	 */
	if (!gamestate->is_running)
	    {
		run_restart_handler (gamestate);
		gamestate->is_running = FALSE;
		return;
	    }

	/* Stop the game and jump. */
	gamestate->is_running = FALSE;
	gamestate->do_restart = TRUE;
	longjmp (gamestate->quitter, 1);
	sc_fatal ("run_restart: unable to restart cleanly\n");
}


/*
 * run_save()
 *
 * Saves either a running or a stopped game.
 */
sc_bool
run_save (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Try a save, return result. */
	return ser_save_game (gamestate);
}


/*
 * run_restore()
 *
 * Restores either a running or a stopped game.  For running games, on
 * successful restore, this function calls a longjump to act as if
 * run_main_loop() returned, and so never returns to its caller.  On failed
 * restore, and for stopped games, it will return, with TRUE if successful,
 * FALSE if restore failed.
 */
sc_bool
run_restore (sc_gamestateref_t gamestate)
{
	sc_bool		is_running;
	assert (gs_is_gamestate_valid (gamestate));

	/*
	 * Note if the game is currently running.  Loading a saved game
	 * always clears the gamestate's is_running flag, and we need to
	 * know how the flag was set here on entry.
	 */
	is_running = gamestate->is_running;

	/* Try to restore the gamestate from a saved file. */
	if (ser_load_game (gamestate))
	    {
		/* If the game was running, ensure stopped and jump. */
		if (is_running)
		    {
			gamestate->is_running = FALSE;
			gamestate->do_restore = TRUE;
			longjmp (gamestate->quitter, 1);
			sc_fatal ("run_restore: unable to restart cleanly\n");
		    }

		/*
		 * Game was not running on entry; ensure stopped anyway,
		 * and return successfully.
		 */
		gamestate->is_running = FALSE;
		return TRUE;
	    }

	/* If we return at all, return a failure. */
	return FALSE;
}


/*
 * run_undo()
 *
 * Undo a turn in either a running or a stopped game.  Returns TRUE on
 * successful undo, FALSE if no undo buffer is available.
 */
sc_bool
run_undo (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* If there's an undo buffer available, restore it. */
	if (gamestate->undo_available)
	    {
		sc_bool		is_running;

		/* Restore the undo buffer, retaining running flag. */
		is_running = gamestate->is_running;
		gs_copy (gamestate, gamestate->undo);
		gamestate->undo_available = FALSE;
		gamestate->is_running = is_running;

		/* Location may have changed; update status. */
		run_update_status (gamestate);

		/* Bring resources into line with the revised gamestate. */
		res_sync_resources (gamestate);
		return TRUE;
	    }

	/* No undo buffer available. */
	return FALSE;
}


/*
 * run_is_running()
 *
 * Query the game running state.
 */
sc_bool
run_is_running (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	return gamestate->is_running;
}


/*
 * run_has_completed()
 *
 * Query the game completion state.  Completed games cannot be resumed,
 * since they've run the exit task and thus have nowhere to go.
 */
sc_bool
run_has_completed (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	return gamestate->has_completed;
}


/*
 * run_is_undo_available()
 *
 * Query the game turn undo buffer availability.
 */
sc_bool
run_is_undo_available (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	return gamestate->undo_available;
}


/*
 * run_get_attributes()
 * run_set_attributes()
 *
 * Get and set selected game attributes.
 */
void
run_get_attributes (sc_gamestateref_t gamestate,
		sc_char **game_name, sc_char **game_author,
		sc_uint32 *turns, sc_int32 *score, sc_int32 *max_score,
		sc_char **current_room_name, sc_char **status_line,
		sc_char **preferred_font,
		sc_bool *bold_room_names, sc_bool *verbose,
		sc_bool *notify_score_change)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_vartype_t		vt_key[2];
	assert (gs_is_gamestate_valid (gamestate));

	/* Return the game name and author if requested. */
	if (game_name != NULL)
	    {
		if (gamestate->title == NULL)
		    {
			sc_char		*gamename, *filtered;

			vt_key[0].string = "Globals";
			vt_key[1].string = "GameName";
			gamename = prop_get_string (bundle, "S<-ss", vt_key);

			filtered = pf_filter_for_info (gamename, vars);
			pf_strip_tags (filtered);
			gamestate->title = filtered;
		    }
		*game_name = gamestate->title;
	    }
	if (game_author != NULL)
	    {
		if (gamestate->author == NULL)
		    {
			sc_char		*gameauthor, *filtered;

			vt_key[0].string = "Globals";
			vt_key[1].string = "GameAuthor";
			gameauthor = prop_get_string (bundle, "S<-ss", vt_key);

			filtered = pf_filter_for_info (gameauthor, vars);
			pf_strip_tags (filtered);
			gamestate->author = filtered;
		    }
		*game_author = gamestate->author;
	    }

	/* Return the current room name and status line if requested. */
	if (current_room_name != NULL)
		*current_room_name = gamestate->current_room_name;
	if (status_line != NULL)
		*status_line = gamestate->status_line;

	/* Return any game preferred font, or NULL if none. */
	if (preferred_font != NULL)
	    {
		vt_key[0].string = "CustomFont";
		if (prop_get_boolean (bundle, "B<-s", vt_key))
		    {
			vt_key[0].string = "FontNameSize";
			*preferred_font = prop_get_string
						(bundle, "S<-s", vt_key);
		    }
		else
			*preferred_font = NULL;
	    }

	/* Return any other selected gamestate attributes. */
	if (turns != NULL)
		*turns = gamestate->turns;
	if (score != NULL)
		*score = gamestate->score;
	if (max_score != NULL)
	    {
		vt_key[0].string = "Globals";
		vt_key[1].string = "MaxScore";
		*max_score = prop_get_integer (bundle, "I<-ss", vt_key);
	    }
	if (bold_room_names != NULL)
		*bold_room_names = gamestate->bold_room_names;
	if (verbose != NULL)
		*verbose = gamestate->verbose;
	if (notify_score_change != NULL)
		*notify_score_change = gamestate->notify_score_change;
}

void
run_set_attributes (sc_gamestateref_t gamestate,
		sc_bool bold_room_names, sc_bool verbose,
		sc_bool notify_score_change)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Set game options. */
	gamestate->bold_room_names	= bold_room_names;
	gamestate->verbose		= verbose;
	gamestate->notify_score_change	= notify_score_change;
}


/*
 * run_hint_iterate()
 *
 * Return the next hint appropriate to the game state, or the first if
 * hint is NULL.  Returns NULL if none, or no more hints.  This function
 * works with pointers to a task state rather than task indexes so that
 * the token passed in and out is a pointer, and readily made opaque to
 * the client as a void*.
 */
sc_hintref_t
run_hint_iterate (sc_gamestateref_t gamestate, sc_hintref_t hint)
{
	sc_int32		task;
	assert (gs_is_gamestate_valid (gamestate));

	/*
	 * Hint is a pointer to a task state; convert to a task index,
	 * adding one to move on to the next task, or start at the first
	 * task if null.
	 */
	if (hint == NULL)
		task = 0;
	else
	    {
		/* Convert into pointer, and range check. */
		task = hint - gamestate->tasks;
		if (task < 0 || task >= gamestate->task_count)
		     {
			sc_error ("run_hint_iterate:"
					" invalid iteration hint\n");
			return NULL;
		     }

		/* Advance beyond current task. */
		task++;
	    }

	/* Scan for the next runnable task that offers a hint. */
	for ( ; task < gamestate->task_count; task++)
	    {
		if (task_can_run_task (gamestate, task)
				&& task_has_hints (gamestate, task))
			break;
	    }

	/* Return a pointer to the state of the task identified, or NULL. */
	if (task < gamestate->task_count)
		return gamestate->tasks + task;
	else
		return NULL;
}


/*
 * run_get_hint_question()
 * run_get_subtle_hint()
 * run_get_unsubtle_hint()
 *
 * Return the strings for a hint.  Front-ends to task functions.  Each
 * converts the hint "address" to a task index through pointer arithmetic,
 * then filters it and returns a temporary, valid only until the next hint
 * call.
 *
 * Hint strings are NULL if empty (not defined by the game).
 */
sc_char *
run_get_hint_question (sc_gamestateref_t gamestate, sc_hintref_t hint)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		task;
	sc_char			*question, *filtered;
	assert (gs_is_gamestate_valid (gamestate));

	/* Verify the caller passed in a valid hint. */
	task = hint - gamestate->tasks;
	if (task < 0 || task >= gamestate->task_count)
	    {
		sc_error ("run_get_hint_question: invalid iteration hint\n");
		return NULL;
	    }
	if (!task_has_hints (gamestate, task))
	    {
		sc_error ("run_get_hint_question: task has no hint\n");
		return NULL;
	    }

	/* Get game question from the task. */
	question = task_get_hint_question (gamestate, task);
	assert (question != NULL);

	/* Filter and strip tags, note in gamestate. */
	filtered = pf_filter (question, vars, bundle);
	pf_strip_tags_for_hints (filtered);
	if (gamestate->hint_text != NULL)
		sc_free (gamestate->hint_text);
	gamestate->hint_text = filtered;

	return filtered;
}

sc_char *
run_get_subtle_hint (sc_gamestateref_t gamestate, sc_hintref_t hint)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		task;
	sc_char			*string, *filtered;
	assert (gs_is_gamestate_valid (gamestate));

	/* Verify the caller passed in a valid hint. */
	task = hint - gamestate->tasks;
	if (task < 0 || task >= gamestate->task_count)
	    {
		sc_error ("run_get_subtle_hint: invalid iteration hint\n");
		return NULL;
	    }
	if (!task_has_hints (gamestate, task))
	    {
		sc_error ("run_get_subtle_hint: task has no hint\n");
		return NULL;
	    }

	/* Get the hint string, and return NULL if empty. */
	string = task_get_hint_subtle (gamestate, task);
	if (sc_strempty (string))
		return NULL;

	/* Filter and strip tags, note in gamestate. */
	filtered = pf_filter (string, vars, bundle);
	pf_strip_tags_for_hints (filtered);
	if (gamestate->hint_text != NULL)
		sc_free (gamestate->hint_text);
	gamestate->hint_text = filtered;

	return filtered;
}

sc_char *
run_get_unsubtle_hint (sc_gamestateref_t gamestate, sc_hintref_t hint)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_var_setref_t		vars	= gamestate->vars;
	sc_int32		task;
	sc_char			*string, *filtered;
	assert (gs_is_gamestate_valid (gamestate));

	/* Verify the caller passed in a valid hint. */
	task = hint - gamestate->tasks;
	if (task < 0 || task >= gamestate->task_count)
	    {
		sc_error ("run_get_unsubtle_hint: invalid iteration hint\n");
		return NULL;
	    }
	if (!task_has_hints (gamestate, task))
	    {
		sc_error ("run_get_unsubtle_hint: task has no hint\n");
		return NULL;
	    }

	/* Get the hint string, and return NULL if empty. */
	string = task_get_hint_unsubtle (gamestate, task);
	if (sc_strempty (string))
		return NULL;

	/* Filter and strip tags, note in gamestate. */
	filtered = pf_filter (string, vars, bundle);
	pf_strip_tags_for_hints (filtered);
	if (gamestate->hint_text != NULL)
		sc_free (gamestate->hint_text);
	gamestate->hint_text = filtered;

	return filtered;
}

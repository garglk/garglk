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
static const sc_char	NEWLINE			= '\n',
			CARRIAGE_RETURN		= '\r',
			NUL			= '\0';


/*
 * if_print_string()
 * if_print_debug()
 * if_print_character()
 * if_print_debug_character()
 * if_print_tag()
 *
 * Call OS-specific print function for the given arguments.
 */
void
if_print_string (const sc_char *string)
{
	assert (string != NULL);

	os_print_string (string);
}

void
if_print_debug (const sc_char *string)
{
	assert (string != NULL);

	os_print_string_debug (string);
}

void
if_print_character (sc_char character)
{
	sc_char		buffer[2];

	buffer[0] = character;
	buffer[1] = NUL;
	os_print_string (buffer);
}

void
if_print_debug_character (sc_char character)
{
	sc_char		buffer[2];

	buffer[0] = character;
	buffer[1] = NUL;
	os_print_string_debug (buffer);
}

void
if_print_tag (sc_uint32 tag, const sc_char *arg)
{
	os_print_tag (tag, arg);
}


/*
 * if_read_line()
 * if_read_debug()
 *
 * Call OS-specific line read function, after flushing OS output.  Called
 * for both game and debugging input -- for game input, add a newline;
 * for debugging input, suppress it to give a prompt effect.
 */
static void
if_read_line_common (sc_char *buffer, sc_uint16 length, sc_bool is_debug_input)
{
	sc_bool		line_available;
	assert (buffer != NULL && length > 0);

	/* Loop until valid player input is available. */
	do
	    {
		/* Space with a blank line, and clear buffer. */
		if_print_character ('\n');
		memset (buffer, 0, length);

		/* Flush pending output, then read in a line of text. */
		os_flush ();
		line_available = is_debug_input
				? os_read_line_debug (buffer, length)
				: os_read_line (buffer, length);
	    }
	while (!line_available);

	/* Drop any trailing newline/return. */
	if (strlen (buffer) > 0)
	    {
		sc_int32	last;

		last = strlen(buffer) - 1;
		while (last > 0
				&& (buffer[last] == CARRIAGE_RETURN
					|| buffer[last] == NEWLINE))
			buffer[last--] = NUL;
	    }
}

void
if_read_line (sc_char *buffer, sc_uint16 length)
{
	if_read_line_common (buffer, length, FALSE);
}

void
if_read_debug (sc_char *buffer, sc_uint16 length)
{
	if_read_line_common (buffer, length, TRUE);
}


/*
 * if_confirm()
 *
 * Call OS-specific confirm function, after flushing OS output.
 */
sc_bool
if_confirm (sc_uint32 type)
{
	os_flush ();
	return os_confirm (type);
}


/*
 * if_open_saved_game()
 * if_write_saved_game()
 * if_read_saved_game()
 * if_close_saved_game()
 *
 * Call OS-specific functions for saving and restoring games.
 */
void *
if_open_saved_game (sc_bool is_save)
{
	os_flush ();
	return os_open_file (is_save);
}

void
if_write_saved_game (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	assert (buffer != NULL);

	os_write_file (opaque, buffer, length);
}

sc_uint16
if_read_saved_game (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	assert (buffer != NULL);

	return os_read_file (opaque, buffer, length);
}

void
if_close_saved_game (void *opaque)
{
	os_close_file (opaque);
}


/*
 * if_display_hints()
 *
 * Call OS-specific hint display function, after flushing OS output.
 */
void
if_display_hints (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	os_flush ();
	os_display_hints ((sc_game) gamestate);
}


/*
 * if_update_sound()
 * if_update_graphic()
 *
 * Call OS-specific sound and graphic handler functions.
 */
void
if_update_sound (sc_char *filename,
		sc_int32 sound_offset, sc_int32 sound_length,
		sc_bool is_looping)
{
	if (strlen (filename) > 0)
		os_play_sound (filename,
				sound_offset, sound_length, is_looping);
	else
		os_stop_sound ();
}

void
if_update_graphic (sc_char *filename,
		sc_int32 graphic_offset, sc_int32 graphic_length)
{
	os_show_graphic (filename, graphic_offset, graphic_length);
}


/*
 * if_initialize()
 *
 * First-time runtime checks for the overall interpreter.  This function
 * tries to ensure correct compile options.
 */
static void
if_initialize (void)
{
	static	sc_bool		initialized = FALSE;

	/* Only do checks on the first call. */
	if (!initialized)
	    {
		/* Make a few quick checks on type sizes. */
		if (sizeof (sc_byte) != 1
				|| sizeof (sc_char) != 1)
		    {
			sc_error ("if_initialize:"
				" sizeof sc_byte/sc_char is not 1,"
				" check compile options\n");
		    }
		if (sizeof (sc_uint16) != 2
				|| sizeof (sc_int16) != 2)
		    {
			sc_error ("if_initialize:"
				" sizeof sc_uint16/sc_int16 is not 2,"
				" check compile options\n");
		    }
		if (sizeof (sc_uint32) != 4
				|| sizeof (sc_int32) != 4)
		    {
			sc_error ("if_initialize:"
				" sizeof sc_uint32/sc_int32 is not 4,"
				" check compile options\n");
		    }

		/* Set checked flag. */
		initialized = TRUE;
	    }
}


/*
 * sc_scare_version()
 * sc_scare_emulation()
 *
 * Return a version string and Adrift emulation level.
 */
const sc_char *
sc_scare_version (void)
{
	return "SCARE " SCARE_VERSION;
}

sc_uint32
sc_scare_emulation (void)
{
	return SCARE_EMULATION;
}


/*
 * sc_game_from_filename()
 * sc_game_from_stream()
 * sc_game_from_callback()
 *
 * Called by the OS-specific layer to create a run context.
 */
sc_game
sc_game_from_filename (const sc_char *filename, sc_uint32 trace_flags)
{
	if (filename == NULL)
	    {
		sc_error ("sc_game_from_filename: NULL filename\n");
		return NULL;
	    }

	if_initialize ();
	return run_create_filename (filename, trace_flags);
}

sc_game
sc_game_from_stream (FILE *stream, sc_uint32 trace_flags)
{
	if (stream == NULL)
	    {
		sc_error ("sc_game_from_stream: NULL stream\n");
		return NULL;
	    }

	if_initialize ();
	return run_create_file (stream, trace_flags);
}

sc_game
sc_game_from_callback (sc_uint16 (*callback) (void *, sc_byte *, sc_uint16),
				void *opaque, sc_uint32 trace_flags)
{
	if (callback == NULL)
	    {
		sc_error ("sc_game_from_callback: NULL callback\n");
		return NULL;
	    }

	if_initialize ();
	return run_create (callback, opaque, trace_flags);
}


/*
 * if_gamestate_error()
 *
 * Common function to verify that the game passed in to functions below
 * is a valid gamestate.  Returns TRUE on gamestate error, FALSE if okay.
 */
static sc_bool
if_gamestate_error (sc_gamestateref_t gamestate, const char *function_name)
{
	/* Check for invalid gamestate -- null pointer or bad magic. */
	if (!gs_is_gamestate_valid (gamestate))
	    {
		if (gamestate == NULL)
			sc_error ("%s: NULL game\n", function_name);
		else
			sc_error ("%s: invalid game\n", function_name);
		return TRUE;
	    }

	/* No gamestate error. */
	return FALSE;
}


/*
 * sc_interpret_game()
 * sc_restart_game()
 * sc_save_game()
 * sc_load_game()
 * sc_undo_game_turn()
 * sc_quit_game()
 *
 * Called by the OS-specific layer to run a game loaded into a run context,
 * and to quit the interpreter on demand, if required.  sc_quit_game()
 * is implemented as a longjmp(), so never returns to the caller --
 * instead, the program behaves as if sc_interpret_game() had returned.
 * sc_load_game() will longjmp() if the restore is successful (thus
 * behaving like sc_restart_game()), but will return if the game could not
 * be restored.  sc_undo_game_turn() behaves like sc_load_game().
 */
void
sc_interpret_game (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_interpret_game"))
		return;

	run_interpret (gamestate);
}

void
sc_restart_game (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_restart_game"))
		return;

	run_restart (gamestate);
}

sc_bool
sc_save_game (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_save_game"))
		return FALSE;

	return run_save (gamestate);
}

sc_bool
sc_load_game (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_restore_game"))
		return FALSE;

	return run_restore (gamestate);
}

sc_bool
sc_undo_game_turn (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_undo_game_turn"))
		return FALSE;

	return run_undo (gamestate);
}

void
sc_quit_game (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_quit_game"))
		return;

	run_quit (gamestate);
}


/*
 * sc_free_game()
 *
 * Called by the OS-specific layer to free run context memory.
 */
void
sc_free_game (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_free_game"))
		return;

	run_destroy (gamestate);
}


/*
 * sc_is_game_running()
 * sc_get_game_name()
 * sc_get_game_author()
 * sc_get_game_turns()
 * sc_get_game_score()
 * sc_get_game_max_score()
 * sc_get_game_room ()
 * sc_get_game_status_line ()
 * sc_get_game_preferred_font ()
 * sc_get_game_bold_room_names()
 * sc_get_game_verbose()
 * sc_get_game_notify_score_change()
 * sc_has_game_completed()
 * sc_is_game_undo_available()
 *
 * Return a few attributes of a game.
 */
sc_bool
sc_is_game_running (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_is_game_running"))
		return FALSE;

	return run_is_running (gamestate);
}

sc_char *
sc_get_game_name (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_char			*retval;

	if (if_gamestate_error (gamestate, "sc_get_game_name"))
		return "[invalid game]";

	run_get_attributes (gamestate, &retval,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	return retval;
}

sc_char *
sc_get_game_author (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_char			*retval;

	if (if_gamestate_error (gamestate, "sc_get_game_author"))
		return "[invalid game]";

	run_get_attributes (gamestate, NULL, &retval,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	return retval;
}

sc_uint32
sc_get_game_turns (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_uint32		retval;

	if (if_gamestate_error (gamestate, "sc_get_game_turns"))
		return 0;

	run_get_attributes (gamestate, NULL, NULL, &retval,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	return retval;
}

sc_int32
sc_get_game_score (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_int32		retval;

	if (if_gamestate_error (gamestate, "sc_get_game_score"))
		return 0;

	run_get_attributes (gamestate, NULL, NULL, NULL, &retval,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	return retval;
}

sc_int32
sc_get_game_max_score (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_int32		retval;

	if (if_gamestate_error (gamestate, "sc_get_game_max_score"))
		return 0;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, &retval,
		NULL, NULL, NULL, NULL, NULL, NULL);
	return retval;
}

sc_char *
sc_get_game_room (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_char			*retval;

	if (if_gamestate_error (gamestate, "sc_get_game_room"))
		return "[invalid game]";

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, &retval,
		NULL, NULL, NULL, NULL, NULL);
	return retval;
}

sc_char *
sc_get_game_status_line (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_char			*retval;

	if (if_gamestate_error (gamestate, "sc_get_game_status_line"))
		return "[invalid game]";

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
		&retval, NULL, NULL, NULL, NULL);
	return retval;
}

sc_char *
sc_get_game_preferred_font (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_char			*retval;

	if (if_gamestate_error (gamestate, "sc_get_game_preferred_font"))
		return "[invalid game]";

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, &retval, NULL, NULL, NULL);
	return retval;
}

sc_bool
sc_get_game_bold_room_names (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_bool			retval;

	if (if_gamestate_error (gamestate, "sc_get_game_bold_room_names"))
		return FALSE;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, &retval, NULL, NULL);
	return retval;
}

sc_bool
sc_get_game_verbose (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_bool			retval;

	if (if_gamestate_error (gamestate, "sc_get_game_verbose"))
		return FALSE;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, &retval, NULL);
	return retval;
}

sc_bool
sc_get_game_notify_score_change (sc_game game)
{
	sc_gamestateref_t	gamestate = game;
	sc_bool			retval;

	if (if_gamestate_error (gamestate, "sc_get_game_notify_score_change"))
		return FALSE;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, &retval);
	return retval;
}

sc_bool
sc_has_game_completed (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_has_game_completed"))
		return FALSE;

	return run_has_completed (gamestate);
}

sc_bool
sc_is_game_undo_available (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_is_game_undo_available"))
		return FALSE;

	return run_is_undo_available (gamestate);
}


/*
 * sc_set_game_bold_room_names()
 * sc_set_game_verbose()
 * sc_set_game_notify_score_change()
 *
 * Set a few attributes of a game.
 */
void
sc_set_game_bold_room_names (sc_game game, sc_bool flag)
{
	sc_gamestateref_t	gamestate = game;
	sc_bool			bold, verbose, notify;

	if (if_gamestate_error (gamestate, "sc_set_game_bold_room_names"))
		return;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, &bold, &verbose, &notify);
	run_set_attributes (gamestate, flag, verbose, notify);
}

void
sc_set_game_verbose (sc_game game, sc_bool flag)
{
	sc_gamestateref_t	gamestate = game;
	sc_bool			bold, verbose, notify;

	if (if_gamestate_error (gamestate, "sc_set_game_verbose"))
		return;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, &bold, &verbose, &notify);
	run_set_attributes (gamestate, bold, flag, notify);
}

void
sc_set_game_notify_score_change (sc_game game, sc_bool flag)
{
	sc_gamestateref_t	gamestate = game;
	sc_bool			bold, verbose, notify;

	if (if_gamestate_error (gamestate, "sc_set_game_notify_score_change"))
		return;

	run_get_attributes (gamestate, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, &bold, &verbose, &notify);
	run_set_attributes (gamestate, bold, verbose, flag);
}


/*
 * sc_does_game_use_sounds()
 * sc_does_game_use_graphics()
 *
 * Indicate the game's use of resources.
 */
sc_bool
sc_does_game_use_sounds (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_does_game_use_sounds"))
		return FALSE;

	return res_has_sound (gamestate);
}

sc_bool
sc_does_game_use_graphics (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_does_game_use_graphics"))
		return FALSE;

	return res_has_graphics (gamestate);
}


/*
 * sc_iterate_game_hints()
 * sc_get_game_hint_question()
 * sc_get_game_subtle_hint()
 * sc_get_game_sledgehammer_hint()
 *
 * Iterate currently available hints, and return strings for a hint.
 */
sc_game_hint
sc_iterate_game_hints (sc_game game, sc_game_hint hint)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_iterate_game_hints"))
		return NULL;

	return run_hint_iterate (gamestate, hint);
}

sc_char *
sc_get_game_hint_question (sc_game game, sc_game_hint hint)
{
	sc_gamestateref_t	gamestate = game;
	sc_hintref_t		hintref = hint;

	if (if_gamestate_error (gamestate, "sc_get_game_hint_question"))
		return NULL;
	if (hintref == NULL)
	    {
		sc_error ("sc_get_game_hint_question: NULL hint\n");
		return NULL;
	    }

	return run_get_hint_question (gamestate, hintref);
}

sc_char *
sc_get_game_subtle_hint (sc_game game, sc_game_hint hint)
{
	sc_gamestateref_t	gamestate = game;
	sc_hintref_t		hintref = hint;

	if (if_gamestate_error (gamestate, "sc_get_game_subtle_hint"))
		return NULL;
	if (hintref == NULL)
	    {
		sc_error ("sc_get_game_subtle_hint: NULL hint\n");
		return NULL;
	    }

	return run_get_subtle_hint (gamestate, hintref);
}

sc_char *
sc_get_game_unsubtle_hint (sc_game game, sc_game_hint hint)
{
	sc_gamestateref_t	gamestate = game;
	sc_hintref_t		hintref = hint;

	if (if_gamestate_error (gamestate, "sc_get_game_unsubtle_hint"))
		return NULL;
	if (hintref == NULL)
	    {
		sc_error ("sc_get_game_unsubtle_hint: NULL hint\n");
		return NULL;
	    }

	return run_get_unsubtle_hint (gamestate, hintref);
}


/*
 * sc_set_game_debugger_enabled()
 * sc_is_game_debugger_enabled()
 * sc_run_game_debugger_command()
 *
 * Enable, disable, and query game debugging, and run a single debug command.
 */
void
sc_set_game_debugger_enabled (sc_game game, sc_bool flag)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_set_game_debugger_enabled"))
		return;

	debug_set_enabled (gamestate, flag);
}

sc_bool
sc_get_game_debugger_enabled (sc_game game)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_get_game_debugger_enabled"))
		return FALSE;

	return debug_get_enabled (gamestate);
}

sc_bool
sc_run_game_debugger_command (sc_game game, const sc_char *debug_command)
{
	sc_gamestateref_t	gamestate = game;

	if (if_gamestate_error (gamestate, "sc_run_game_debugger_command"))
		return FALSE;

	return debug_run_command (gamestate, debug_command);
}

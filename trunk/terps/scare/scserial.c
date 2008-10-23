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
#include <stdio.h>

#include "zlib.h"
#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_uint16	BUFFER_SIZE		= 4096;
enum {			TAFVAR_NUMERIC		= 0,
			TAFVAR_STRING		= 1 };
static const sc_char	NEWLINE			= '\n',
			CARRIAGE_RETURN		= '\r';

/* Output buffer. */
static sc_byte		*ser_buffer		= NULL;
static sc_uint16	ser_buffer_length	= 0;

/* Opaque pointer to pass to output functions. */
static void		*ser_opaque		= NULL;


/*
 * ser_flush()
 * ser_buffer_character()
 *
 * Flush pending buffer contents; add a character to the buffer.
 */
static void
ser_flush (sc_bool final)
{
	static sc_bool		initialized = FALSE;
	static sc_byte		*out_buffer = NULL;
	static sc_uint16	out_buffer_size = 0;
	static z_stream		stream;

	sc_int16		status;

	/* If this is an initial call, initialize deflation. */
	if (!initialized)
	    {
		/* Allocate an initial output buffer. */
		out_buffer_size = BUFFER_SIZE;
		out_buffer = sc_malloc (out_buffer_size);

		/* Initialize Zlib deflation functions. */
		stream.next_out	= out_buffer;
		stream.avail_out= out_buffer_size;
		stream.next_in	= ser_buffer;
		stream.avail_in	= 0;

		stream.zalloc	= Z_NULL;
		stream.zfree	= Z_NULL;
		stream.opaque	= Z_NULL;

		status = deflateInit (&stream, Z_DEFAULT_COMPRESSION);
		if (status != Z_OK)
		    {
			sc_error ("ser_flush:"
					" deflateInit() error %d\n", status);
			ser_buffer_length = 0;

			sc_free (out_buffer);
			out_buffer	= NULL;
			out_buffer_size	= 0;
			return;
		    }

		initialized = TRUE;
	    }

	/* Deflate data from the current output buffer. */
	stream.next_in = ser_buffer;
	stream.avail_in= ser_buffer_length;

	/* Loop while deflate output is pending and buffer not emptied. */
	for (;;)
	    {
		sc_int16	in_bytes, out_bytes;

		/*
		 * Compress stream data, requesting finish if this is
		 * the final flush.
		 */
		if (final)
			status = deflate (&stream, Z_FINISH);
		else
			status = deflate (&stream, Z_NO_FLUSH);
		if (status != Z_STREAM_END && status != Z_OK)
		    {
			sc_error ("ser_flush: deflate() error %d\n", status);
			ser_buffer_length = 0;

			sc_free (out_buffer);
			out_buffer	= NULL;
			out_buffer_size	= 0;
			initialized	= FALSE;
			return;
		    }

		/* Calculate bytes used, and output. */
		in_bytes  = ser_buffer_length - stream.avail_in;
		out_bytes = out_buffer_size - stream.avail_out;

		/* See if compressed data is available. */
		if (out_bytes > 0)
		    {
			/* Write it to save file output. */
			if_write_saved_game (ser_opaque, out_buffer, out_bytes);

			/* Reset deflation stream for available space. */
			stream.next_out	= out_buffer;
			stream.avail_out= out_buffer_size;
		    }

		/* Remove consumed data from the input buffer. */
		if (in_bytes > 0)
		    {
			/* Move any unused data, and reduce length. */
			memmove (ser_buffer, ser_buffer + in_bytes,
						ser_buffer_length - in_bytes);
			ser_buffer_length -= in_bytes;

			/* Reset deflation stream for consumed data. */
			stream.next_in = ser_buffer;
			stream.avail_in= ser_buffer_length;
		    }

		/* If final flush, wait until deflate indicates finished. */
		if (final && status == Z_OK)
			continue;

		/* If data was consumed or produced, break. */
		if (out_bytes > 0
				|| in_bytes > 0)
			break;
	    }

	/* If this was a final call, clean up. */
	if (final)
	    {
		/* Compression completed. */
		status = deflateEnd (&stream);
		if (status != Z_OK)
		    {
			sc_error ("ser_flush: warning:"
					" deflateEnd() error %d\n", status);
		    }
		if (ser_buffer_length != 0)
		    {
			sc_error ("ser_flush: warning:"
					" deflate missed data\n");
			ser_buffer_length = 0;
		    }

		/* Free the allocated compression buffer. */
		sc_free (ser_buffer);
		ser_buffer = NULL;

		/*
		 * Free output buffer, and reset flag for reinitialization
		 * on the next call.
		 */
		sc_free (out_buffer);
		out_buffer	= NULL;
		out_buffer_size	= 0;
		initialized	= FALSE;
	    }
}

static void
ser_buffer_character (sc_char character)
{
	/* Allocate the buffer if not yet done. */
	if (ser_buffer == NULL)
	    {
		assert (ser_buffer_length == 0);
		ser_buffer = sc_malloc (BUFFER_SIZE);
	    }

	/* Add to the buffer, with intermediate flush if filled. */
	ser_buffer[ser_buffer_length++] = character;
	if (ser_buffer_length == BUFFER_SIZE)
		ser_flush (FALSE);
}


/*
 * ser_buffer_string()
 * ser_buffer_uint32()
 * ser_buffer_int32()
 * ser_buffer_int32_special()
 * ser_buffer_boolean()
 *
 * Buffer a string, a 32 bit [un]signed integer, and a boolean.
 */
static void
ser_buffer_string (const sc_char *string)
{
	sc_uint16	index;

	/* Add each character to the buffer, flush if filled. */
	for (index = 0; string[index] != '\0'; index++)
		ser_buffer_character (string[index]);

	/* Add CR/LF, DOS-style, to output. */
	ser_buffer_character (CARRIAGE_RETURN);
	ser_buffer_character (NEWLINE);
}

static void
ser_buffer_int32 (sc_int32 int32)
{
	sc_char		buffer[16];

	/* Convert to a string and buffer that. */
	sprintf (buffer, "%ld", int32);
	ser_buffer_string (buffer);
}

static void
ser_buffer_int32_special (sc_int32 int32)
{
	sc_char		buffer[16];

	/* Weirdo formatting for compatibility. */
	sprintf (buffer, "% ld ", int32);
	ser_buffer_string (buffer);
}

static void
ser_buffer_uint32 (sc_uint32 uint32)
{
	sc_char		buffer[16];

	/* Convert to a string and buffer that. */
	sprintf (buffer, "%lu", uint32);
	ser_buffer_string (buffer);
}

static void
ser_buffer_boolean (sc_bool boolean)
{
	/* Write a 1 for TRUE, 0 for FALSE. */
	if (boolean)
		ser_buffer_string ("1");
	else
		ser_buffer_string ("0");
}


/*
 * ser_save_game()
 *
 * Serialize a game and save its state.
 */
sc_bool
ser_save_game (sc_gamestateref_t gamestate)
{
	sc_var_setref_t		vars	= gamestate->vars;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_int32		index, var_count;

	/* Open an output stream, save its opaque identifier. */
	ser_opaque = if_open_saved_game (TRUE);
	if (ser_opaque == NULL)
		return FALSE;

	/* Write the game name. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "GameName";
	ser_buffer_string (prop_get_string (bundle, "S<-ss", vt_key));

	/* Write the counts of rooms, objects, etc. */
	ser_buffer_int32 (gamestate->room_count);
	ser_buffer_int32 (gamestate->object_count);
	ser_buffer_int32 (gamestate->task_count);
	ser_buffer_int32 (gamestate->event_count);
	ser_buffer_int32 (gamestate->npc_count);

	/* Write the score and player information. */
	ser_buffer_int32 (gamestate->score);
	ser_buffer_int32 (gamestate->playerroom + 1);
	ser_buffer_int32 (gamestate->playerparent);
	ser_buffer_int32 (gamestate->playerposition);

	/* Write player gender. */
	vt_key[0].string = "Globals";
	vt_key[1].string = "PlayerGender";
	ser_buffer_int32 (prop_get_integer (bundle, "I<-ss", vt_key));

	/*
	 * Write encumbrance details, not currently maintained by
	 * the gamestate.
	 */
	ser_buffer_int32 (90); ser_buffer_int32 (0);
	ser_buffer_int32 (90); ser_buffer_int32 (0);

	/* Save rooms information. */
	for (index = 0; index < gamestate->room_count; index++)
		ser_buffer_boolean (gs_get_room_seen (gamestate, index));

	/* Save objects information. */
	for (index = 0; index < gamestate->object_count; index++)
	    {
		ser_buffer_int32 (gs_get_object_position (gamestate, index));
		ser_buffer_boolean (gs_get_object_seen (gamestate, index));
		ser_buffer_int32 (gs_get_object_parent (gamestate, index));
		if (gs_get_object_openness (gamestate, index) != 0)
		    {
			ser_buffer_int32 (gs_get_object_openness
							(gamestate, index));
		    }
		if (gs_get_object_state (gamestate, index) != 0)
		    {
			ser_buffer_int32 (gs_get_object_state
							(gamestate, index));
		    }
		ser_buffer_boolean (gs_get_object_unmoved (gamestate, index));
	    }

	/* Save tasks information. */
	for (index = 0; index < gamestate->task_count; index++)
	    {
		ser_buffer_boolean (gs_get_task_done (gamestate, index));
		ser_buffer_boolean (gs_get_task_scored (gamestate, index));
	    }

	/* Save events information. */
	for (index = 0; index < gamestate->event_count; index++)
	    {
		sc_int32	startertype, task;

		/* Get starter task, if any. */
		vt_key[0].string = "Events";
		vt_key[1].integer = index;
		vt_key[2].string = "StarterType";
		startertype = prop_get_integer (bundle, "I<-sis", vt_key);
		if (startertype == 3)
		    {
			vt_key[2].string = "TaskNum";
			task = prop_get_integer (bundle, "I<-sis", vt_key);
		    }
		else
			task = 0;

		/* Save event details. */
		ser_buffer_int32 (gs_get_event_time (gamestate, index));
		ser_buffer_int32 (task);
		ser_buffer_int32 (gs_get_event_state (gamestate, index) - 1);
		if (task > 0)
			ser_buffer_boolean (gs_get_task_done
							(gamestate, task - 1));
		else
			ser_buffer_boolean (FALSE);
	    }

	/* Save NPCs information. */
	for (index = 0; index < gamestate->npc_count; index++)
	    {
		sc_int32	step;

		ser_buffer_int32 (gamestate->npcs[index].location);
		ser_buffer_boolean (gamestate->npcs[index].seen);
		for (step = 0;
			step < gamestate->npcs[index].walkstep_count; step++)
		    {
			ser_buffer_int32_special
				(gamestate->npcs[index].walksteps[step]);
		    }
	    }

	/* Save each variable. */
	vt_key[0].string = "Variables";
	var_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	for (index = 0; index < var_count; index++)
	    {
		sc_char		*name;
		sc_int32	var_type;

		vt_key[1].integer = index;

		vt_key[2].string = "Name";
		name = prop_get_string (bundle, "S<-sis", vt_key);
		vt_key[2].string = "Type";
		var_type = prop_get_integer (bundle, "I<-sis", vt_key);

		switch (var_type)
		    {
		    case TAFVAR_NUMERIC:
			ser_buffer_int32 (var_get_integer (vars, name));
			break;

		    case TAFVAR_STRING:
			ser_buffer_string (var_get_string (vars, name));
			break;

		    default:
			sc_fatal ("ser_save_game:"
				" unknown variable type, %ld\n", var_type);
		    }
	    }

	/* Save timing information. */
	ser_buffer_uint32 (var_get_elapsed_seconds (vars));

	/* Save turns count. */
	ser_buffer_uint32 (gamestate->turns);

	/* Flush the last buffer contents, and close the saved game. */
	ser_flush (TRUE);
	if_close_saved_game (ser_opaque);

	/* Completed successfully. */
	ser_opaque = NULL;
	return TRUE;
}


/* TAS input file line counter. */
static sc_tafref_t		ser_tas			= NULL;
static sc_uint32		ser_tasline		= 0;

/* Restore error jump buffer. */
static jmp_buf			ser_tas_error;

/*
 * ser_get_string()
 * ser_get_int32()
 * ser_get_uint32()
 * ser_get_boolean()
 *
 * Wrapper round obtaining the next TAS file line, with variants to convert
 * the line content into an appropriate type.
 */
static sc_char *
ser_get_string (void)
{
	sc_char		*line;

	/* Get the next line, and complain if absent. */
	line = taf_next_line (ser_tas);
	if (line == NULL)
	    {
		sc_error ("ser_get_string: out of TAS data"
					" at line %ld\n", ser_tasline);
		longjmp (ser_tas_error, 1);
	    }

	ser_tasline++;
	return line;
}

static sc_int32
ser_get_int32 (void)
{
	sc_char		*line;
	sc_int32	integer;

	/* Get line, and scan for a single integer; return it. */
	line = ser_get_string ();
	if (sscanf (line, "%ld", &integer) != 1)
	    {
		sc_error ("ser_get_int32: invalid integer"
					" at line %ld\n", ser_tasline - 1);
		longjmp (ser_tas_error, 1);
	    }

	return integer;
}

static sc_uint32
ser_get_uint32 (void)
{
	sc_char		*line;
	sc_uint32	integer;

	/* Get line, and scan for a single integer; return it. */
	line = ser_get_string ();
	if (sscanf (line, "%lu", &integer) != 1)
	    {
		sc_error ("ser_get_uint32: invalid integer"
					" at line %ld\n", ser_tasline - 1);
		longjmp (ser_tas_error, 1);
	    }

	return integer;
}

static sc_bool
ser_get_boolean (void)
{
	sc_char		*line;
	sc_bool		boolean;

	/*
	 * Get line, and scan for a single integer; check it's a valid-
	 * looking flag, and return it.
	 */
	line = ser_get_string ();
	if (sscanf (line, "%hu", &boolean) != 1)
	    {
		sc_error ("ser_get_boolean: invalid boolean"
					" at line %ld\n", ser_tasline - 1);
		longjmp (ser_tas_error, 1);
	    }
	if (boolean != 0 && boolean != 1)
	    {
		sc_error ("ser_get_boolean: warning: suspect boolean"
					" at line %ld\n", ser_tasline);
	    }

	return (boolean != 0);
}


/*
 * ser_load_game()
 *
 * Load a serialized game into the given gamestate.
 */
sc_bool
ser_load_game (sc_gamestateref_t gamestate)
{
	static sc_var_setref_t		new_vars;	/* For setjmp safety */
	static sc_gamestateref_t	new_gamestate;	/* For setjmp safety */

	sc_printfilterref_t	filter	= gamestate->filter;
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[3], vt_rvalue;
	sc_int32		index, var_count;
	void			*opaque;
	sc_char			*gamename;

	/* Open an input stream, save its opaque identifier. */
	opaque = if_open_saved_game (FALSE);
	if (opaque == NULL)
		return FALSE;

	/*
	 * Create a TAF (TAS) file from the input stream, and close
	 * the stream immediately (all the data's now in the TAF (TAS)).
	 */
	ser_tas = taf_create (if_read_saved_game, opaque, FALSE);
	if_close_saved_game (opaque);
	if (ser_tas == NULL)
		return FALSE;

	/* Reset line counter for error messages. */
	ser_tasline = 1;

	new_gamestate	= NULL;
	new_vars	= NULL;

	/* Set up error handling jump buffer, and handle errors. */
	if (setjmp (ser_tas_error) != 0)
	    {
		/* Destroy any temporary gamestate and variables. */
		if (new_gamestate != NULL)
			gs_destroy (new_gamestate);
		if (new_vars != NULL)
			var_destroy (new_vars);

		/* Destroy the TAF (TAS) file and return fail status. */
		taf_destroy (ser_tas);
		ser_tas = NULL;
		return FALSE;
	    }

	/*
	 * Read the game name, and compare with the one in the gamestate.
	 * Fail if they don't match exactly.  A tighter check than this
	 * would perhaps be preferable, say, something based on the TAF
	 * file header, but this isn't in the save file format.
	 */
	vt_key[0].string = "Globals";
	vt_key[1].string = "GameName";
	gamename = prop_get_string (bundle, "S<-ss", vt_key);
	if (strcmp (ser_get_string (), gamename) != 0)
	    {
		longjmp (ser_tas_error, 1);
	    }

	/* Read and verify the counts in the saved game. */
	if (ser_get_int32 () != gamestate->room_count
			|| ser_get_int32 () != gamestate->object_count
			|| ser_get_int32 () != gamestate->task_count
			|| ser_get_int32 () != gamestate->event_count
			|| ser_get_int32 () != gamestate->npc_count)
	    {
		longjmp (ser_tas_error, 1);
	    }

	/* Create a variables set and gamestate to restore into. */
	new_vars = var_create (bundle);
	new_gamestate = gs_create (new_vars, bundle, filter);
	var_register_gamestate (new_vars, new_gamestate);

	/* All set to load TAF (TAS) data into the new gamestate. */

	/* Restore the score and player information. */
	new_gamestate->score = ser_get_int32 ();
	new_gamestate->playerroom = ser_get_int32 () - 1;
	new_gamestate->playerparent = ser_get_int32 ();
	new_gamestate->playerposition = ser_get_int32 ();

	/* Skip player gender. */
	(void) ser_get_int32 ();

	/*
	 * Skip encumbrance details, not currently maintained by the
	 * gamestate.
	 */
	(void) ser_get_int32 (); (void) ser_get_int32 ();
	(void) ser_get_int32 (); (void) ser_get_int32 ();

	/* Restore rooms information. */
	for (index = 0; index < new_gamestate->room_count; index++)
		gs_set_room_seen (new_gamestate, index, ser_get_boolean ());

	/* Restore objects information. */
	for (index = 0; index < new_gamestate->object_count; index++)
	    {
		sc_int32	openable, currentstate;

		/* Bypass mutators for position and parent.  Fix later? */
		new_gamestate->objects[index].position = ser_get_int32 ();
		gs_set_object_seen (new_gamestate, index, ser_get_boolean ());
		new_gamestate->objects[index].parent = ser_get_int32 ();

		vt_key[0].string = "Objects";
		vt_key[1].integer = index;
		vt_key[2].string = "Openable";
		openable = prop_get_integer (bundle, "I<-sis", vt_key);
		gs_set_object_openness (new_gamestate, index,
				openable != 0 ? ser_get_int32 () : 0);

		vt_key[2].string = "CurrentState";
		currentstate = prop_get_integer (bundle, "I<-sis", vt_key);
		gs_set_object_state (new_gamestate, index,
				currentstate != 0 ? ser_get_int32 () : 0);

		gs_set_object_unmoved (new_gamestate, index,
							ser_get_boolean ());
	    }

	/* Restore tasks information. */
	for (index = 0; index < new_gamestate->task_count; index++)
	    {
		gs_set_task_done (new_gamestate, index, ser_get_boolean ());
		gs_set_task_scored (new_gamestate, index, ser_get_boolean ());
	    }

	/* Restore events information. */
	for (index = 0; index < new_gamestate->event_count; index++)
	    {
		sc_int32	startertype, task;

		/* Restore first event details. */
		gs_set_event_time (new_gamestate, index, ser_get_int32 ());
		task = ser_get_int32 ();
		gs_set_event_state (new_gamestate, index, ser_get_int32 () + 1);

		/* Verify and restore the starter task, if any. */
		if (task > 0)
		    {
			vt_key[0].string = "Events";
			vt_key[1].integer = index;
			vt_key[2].string = "StarterType";
			startertype = prop_get_integer (bundle,
							"I<-sis", vt_key);
			if (startertype != 3)
				longjmp (ser_tas_error, 1);

			/* Restore task state. */
			gs_set_task_done (new_gamestate, task - 1,
							ser_get_boolean ());

		    }
		else
			(void) ser_get_boolean ();
	    }

	/* Restore NPCs information. */
	for (index = 0; index < new_gamestate->npc_count; index++)
	    {
		sc_int32	step;

		new_gamestate->npcs[index].location = ser_get_int32 ();
		new_gamestate->npcs[index].seen = ser_get_boolean ();
		for (step = 0;
			step < new_gamestate->npcs[index].walkstep_count;
			step++)
		    {
			new_gamestate->npcs[index].walksteps[step] =
							ser_get_int32 ();
		    }
	    }

	/* Restore each variable. */
	vt_key[0].string = "Variables";
	var_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	for (index = 0; index < var_count; index++)
	    {
		sc_char		*name;
		sc_int32	var_type;

		vt_key[1].integer = index;

		vt_key[2].string = "Name";
		name = prop_get_string (bundle, "S<-sis", vt_key);
		vt_key[2].string = "Type";
		var_type = prop_get_integer (bundle, "I<-sis", vt_key);

		switch (var_type)
		    {
		    case TAFVAR_NUMERIC:
			var_put_integer (new_vars, name, ser_get_int32 ());
			break;

		    case TAFVAR_STRING:
			var_put_string (new_vars, name, ser_get_string ());
			break;

		    default:
			sc_fatal ("ser_load_game:"
				" unknown variable type, %ld\n", var_type);
		    }
	    }

	/* Restore timing information. */
	var_set_elapsed_seconds (new_vars, ser_get_uint32 ());

	/* Restore turns count. */
	new_gamestate->turns = ser_get_uint32 ();

	/*
	 * Resources tweak -- set requested to match those in the current
	 * gamestate so that they remain unchanged by the gs_copy() of
	 * new_gamestate onto gamestate.  This way, both the requested and
	 * the active resources in the gamestate are unchanged by restore.
	 */
	new_gamestate->requested_sound = gamestate->requested_sound;
	new_gamestate->requested_graphic = gamestate->requested_graphic;

	/*
	 * If we got this far, we successfully restored the gamestate
	 * from the file.  As our final act, copy the new gamestate onto
	 * the old one.
	 */
	new_gamestate->temporary = gamestate->temporary;
	new_gamestate->undo = gamestate->undo;
	gs_copy (gamestate, new_gamestate);

	/* Done with the temporary gamestate and variables. */
	gs_destroy (new_gamestate);
	var_destroy (new_vars);

	/* Done with TAF (TAS) file. */
	taf_destroy (ser_tas);
	ser_tas = NULL;
	return TRUE;
}

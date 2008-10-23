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
 * o This module represents just about the simplest platform input/output
 *   code possible for SCARE.  Actually, it could be simplified still
 *   further by abandoning attempts to word wrap at 78 columns of text,
 *   and by ignoring all tags altogether, though this may stop some
 *   games playing quite like they should.
 *
 * o Feel free to use this code as a starting point for a platform port.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "scare.h"

enum {			FALSE = 0,
			TRUE  = !FALSE };

static sc_char		lbuffer[79];
static sc_uint16	llength = 0;

static sc_char		*game_file;

/*
 * os_flush()
 * os_print_tag()
 * os_print_string()
 * os_print_string_debug()
 */
void
os_flush (void)
{
	if (llength > 0)
	    {
		fwrite (lbuffer, 1, llength, stdout);
		llength = 0;
	    }
	fflush (stdout);
}

void
os_print_tag (sc_uint32 tag, const sc_char *arg)
{
	sc_uint16	i;
	const sc_char	*unused; unused = arg;

	switch (tag)
	    {
	    case SC_TAG_CLS:
		os_flush ();
		for (i = 0; i < 25; i++)
			printf ("\n");
		break;

	    case SC_TAG_CENTER:
	    case SC_TAG_RIGHT:
	    case SC_TAG_ENDCENTER:
    	    case SC_TAG_ENDRIGHT:
		if (llength > 0)
		    {
			os_flush ();
			printf ("\n");
		    }
		break;

	    case SC_TAG_WAITKEY:
		{
		sc_char	dummy[256];
		os_flush ();
		fgets (dummy, sizeof (dummy), stdin);
		break;
		}
	    }
}

void
os_print_string (const sc_char *string)
{
	sc_uint16	index;

	for (index = 0; string[index] != '\0'; index++)
	    {
		if (string[index] == '\n')
		    {
			os_flush ();
			printf ("\n");
			continue;
		    }

		lbuffer[llength++] = string[index];
		if (llength >= sizeof (lbuffer) - 1)
		    {
			sc_char		*lbreak;

			lbuffer[llength] = '\0';
			lbreak = strrchr (lbuffer, ' ');
			if (lbreak != NULL)
			    {
				fwrite (lbuffer, 1, lbreak - lbuffer, stdout);
				memmove (lbuffer, lbreak + 1,
							strlen (lbreak) + 1);
				llength = strlen (lbuffer);
			    }
			else
				os_flush ();

			printf ("\n");
		    }
	    }
}

void
os_print_string_debug (const sc_char *string)
{
	os_print_string (string);
}


/*
 * os_play_sound()
 * os_stop_sound()
 * os_show_graphic()
 */
void
os_play_sound (sc_char *filepath,
		sc_int32 offset, sc_int32 length, sc_bool is_looping)
{
	sc_char		*unused1;
	sc_int32	unused2, unused3;
	sc_bool		unused4;
	unused1 = filepath;
	unused2 = offset; unused3 = length;
	unused4 = is_looping;
}

void
os_stop_sound (void)
{
}

void
os_show_graphic (sc_char *filepath, sc_int32 offset, sc_int32 length)
{
#ifdef LINUX_GRAPHICS
	sc_char		*unused1;
	unused1 = filepath;

	if (length > 0
		&& strlen (game_file) < 768)
	    {
		sc_char		buffer[1024];

		sprintf (buffer, "dd if=%s ibs=1c skip=%ld count=%ld obs=100k"
				 " of=/tmp/scare.jpg 2>/dev/null",
						game_file, offset, length);
		system (buffer);
		system ("xv /tmp/scare.jpg >/dev/null 2>&1 &");
		system ("( sleep 10; rm /tmp/scare.jpg ) >/dev/null 2>&1 &");
	    }
#else
	sc_char		*unused1;
	sc_int32	unused2, unused3;
	unused1 = filepath;
	unused2 = offset; unused3 = length;
#endif
}


/*
 * os_read_line()
 * os_read_line_debug()
 */
sc_bool
os_read_line (sc_char *buffer, sc_uint16 length)
{
	printf (">");
	fflush (stdout);
	fgets (buffer, length, stdin);
	return TRUE;
}

sc_bool
os_read_line_debug (sc_char *buffer, sc_uint16 length)
{
	printf ("[SCARE debug]");
	return os_read_line (buffer, length);
}


/*
 * os_confirm()
 */
sc_bool
os_confirm (sc_uint32 type)
{
	sc_char		buffer[256];

	if (type == SC_CONF_SAVE)
		return TRUE;

	do
	    {
		printf ("Do you really want to ");
		switch (type)
		    {
		    case SC_CONF_QUIT:		printf ("quit");	break;
		    case SC_CONF_RESTART:	printf ("restart");	break;
		    case SC_CONF_SAVE:		printf ("save");	break;
		    case SC_CONF_RESTORE:	printf ("restore");	break;
		    case SC_CONF_VIEW_HINTS:	printf ("view hints");	break;
		    default:			printf ("do that");	break;
		    }
		printf ("? [Y/N] ");
		fflush (stdout);
		fgets (buffer, sizeof (buffer), stdin);
	    }
	while (toupper (buffer[0]) != 'Y' && toupper (buffer[0]) != 'N');
	return (toupper (buffer[0]) == 'Y');
}


/*
 * os_open_file()
 * os_write_file()
 * os_read_file()
 * os_close_file()
 */
void *
os_open_file (sc_bool is_save)
{
	sc_char		path[256];
	FILE		*stream;

	printf ("Enter saved game to %s: ", is_save ? "save" : "load");
	fflush (stdout);
	fgets (path, sizeof (path), stdin);
	if (path[strlen (path) - 1] == '\n')
		path[strlen (path) - 1] = '\0';

	if (is_save)
	    {
		stream = fopen (path, "rb");
		if (stream != NULL)
		    {
			fclose (stream);
			printf ("File already exists.\n");
			return NULL;
		    }
		stream = fopen (path, "wb");
	    }
	else
		stream = fopen (path, "rb");

	if (stream == NULL)
	    {
		printf ("Error opening file.\n");
		return NULL;
	    }
	return stream;
}

void
os_write_file (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	FILE		*stream = (FILE *) opaque;

	fwrite (buffer, 1, length, stream);
	if (ferror (stream))
		fprintf (stderr, "Write error: %s\n", strerror (errno));
}

sc_uint16
os_read_file (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	FILE		*stream = (FILE *) opaque;
	sc_uint16	bytes;

	bytes = fread (buffer, 1, length, stream);
	if (ferror (stream))
		fprintf (stderr, "Read error: %s\n", strerror (errno));

	return bytes;
}

void
os_close_file (void *opaque)
{
	FILE		*stream = (FILE *) opaque;

	fclose (stream);
}


/*
 * os_display_hints()
 */
void
os_display_hints (sc_game game)
{
	sc_game_hint	hint;

	for (hint = sc_iterate_game_hints (game, NULL);
			hint != NULL; hint = sc_iterate_game_hints (game, hint))
	    {
		sc_char		*hint_text;

		printf ("%s\n", sc_get_game_hint_question (game, hint));
		hint_text = sc_get_game_subtle_hint (game, hint);
		if (hint_text != NULL)
			printf ("- %s\n", hint_text);
		hint_text = sc_get_game_unsubtle_hint (game, hint);
		if (hint_text != NULL)
			printf ("- %s\n", hint_text);
	    }
}


/*
 * main()
 */
int
main (int argc, char *argv[])
{
	FILE		*stream;
	sc_game		game;
	sc_uint32	trace_flags = 0;

	if (argc != 2)
	    {
		fprintf (stderr, "Usage: %s taf_file\n", argv[0]);
		return 1;
	    }

	stream = fopen (argv[1], "rb");
	if (stream == NULL)
	    {
		fprintf (stderr, "%s: %s: %s\n", argv[0],
					argv[1], strerror (errno));
		return 1;
	    }

	if (getenv ("SC_TRACE_FLAGS"))
		trace_flags = strtoul (getenv ("SC_TRACE_FLAGS"), NULL, 0);

	printf ("Loading game...\n");
	game = sc_game_from_stream (stream, trace_flags);
	if (game == NULL)
	    {
		fprintf (stderr, "%s: game read error\n", argv[0]);
		return 1;
	    }
	fclose (stream);

	if (getenv ("SC_DEBUGGER_ENABLED"))
		sc_set_game_debugger_enabled (game, 1);

	game_file = argv[1];

	sc_interpret_game (game);
	sc_free_game (game);
	return 0;
}

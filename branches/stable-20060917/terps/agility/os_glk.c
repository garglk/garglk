/*
 * Copyright (C) 2002-2003  Simon Baldwin, simon_baldwin@yahoo.com
 * Mac portions Copyright (C) 2002  Ben Hines
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
 * Glk interface for AGiliTy 1.1.1
 * -------------------------------
 *
 * This module contains the the Glk porting layer for AGiliTy.  It
 * defines the Glk arguments list structure, the entry points for the
 * Glk library framework to use, and all platform-abstracted I/O to
 * link to Glk's I/O.
 *
 * The following items are omitted from this Glk port:
 *
 *  o Calls to glk_tick().  The Glk documentation states that the
 *    interpreter should call glk_tick() every opcode or so.  This is
 *    intrusive to code (it goes outside of this module), and since
 *    most Glk libraries do precisely nothing in glk_tick(), there is
 *    little motivation to add it.
 *
 *  o Glk tries to assert control over _all_ file I/O.  It's just too
 *    disruptive to add it to existing code, so for now, the AGiliTy
 *    interpreter is still dependent on stdio and the like.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>

#ifndef GLK_ANSI_ONLY
#if !defined (__USE_POSIX)
#	define __USE_POSIX			/* Gets fdopen() definition. */
#	include <stdio.h>
#	undef __USE_POSIX
#else
#	include <stdio.h>
#endif

#if TARGET_RT_MAC_CFM
#	include <time.h>
	/* Sys/types.h absent on CW MSL (aka CFM), replace with fcntls. */
#	include <fcntl.h>
#	define	FD_SETSIZE	1024
	/*
	 * CW's MSL defines exec() in unistd.h which conflicts with exec in
	 * interp.  To work around this I'll put prototypes of the stuff we
	 * use from it here.
	 */
	_MSL_IMP_EXP_C int		close(int);
#else
#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/time.h>
#	include <sys/types.h>
#endif
#else
#	include <stdio.h>
#endif

#include "agility.h"
#include "interp.h"

#include "glk.h"

#if TARGET_OS_MAC
#	include "macglk_startup.h"
#else
#	include "glkstart.h"
#endif


/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous externals not in header files      */
/*---------------------------------------------------------------------*/

/* Glk AGiliTy port version number. */
static const glui32	GAGT_PORT_VERSION		= 0x00010609;

/*
 * We use two Glk windows; one is two lines at the top of the display area
 * for the status line, and the other is the remainder of the display area,
 * used for, well, everything else.  Where a particular Glk implementation
 * won't do more than one window, the status window remains NULL.
 */
static winid_t		gagt_main_window		= NULL;
static winid_t		gagt_status_window		= NULL;

/*
 * Transcript stream and input log.  These are NULL if there is no current
 * collection of these strings.
 */
static strid_t		gagt_transcript_stream		= NULL;
static strid_t		gagt_inputlog_stream		= NULL;

/* Input read log stream, for reading back an input log. */
static strid_t		gagt_readlog_stream		= NULL;

/* Options that may be turned off or set by command line flags. */
static enum		{ FONT_AUTOMATIC, FONT_FIXED_WIDTH,
			  FONT_PROPORTIONAL, FONT_DEBUG }
			gagt_font_mode			= FONT_AUTOMATIC;
static enum		{ DELAY_FULL, DELAY_SHORT, DELAY_OFF }
			gagt_delay_mode			= DELAY_SHORT;
static int		gagt_replacement_enabled	= TRUE;
static int		gagt_extended_status_enabled	= TRUE;
static int		gagt_abbreviations_enabled	= TRUE;
static int		gagt_commands_enabled		= TRUE;

/* Delays are not possible if Glk doesn't implement timeouts.  */
static int		gagt_delays_possible		= TRUE;

/* Forward declaration of event wait functions. */
static void		gagt_event_wait (glui32 wait_type, event_t *event);
static void		gagt_event_wait_2 (glui32 wait_type_1,
	       				glui32 wait_type_2, event_t *event);

/*
 * Forward declaration of the glk_exit() wrapper.  Normal functions in this
 * module should not to call glk_exit() directly; they should always call it
 * through the wrapper instead.
 */
static void		gagt_exit (void);


/*---------------------------------------------------------------------*/
/*  Glk arguments list                                                 */
/*---------------------------------------------------------------------*/

#if !TARGET_OS_MAC
glkunix_argumentlist_t glkunix_arguments[] = {
		{ (char *) "-gf", glkunix_arg_NoValue,
	(char *) "-gf        Force Glk to use only a fixed width font" },
		{ (char *) "-gp", glkunix_arg_NoValue,
	(char *) "-gp        Allow Glk to use only a proportional font" },
		{ (char *) "-ga", glkunix_arg_NoValue,
	(char *) "-ga        Try to use a suitable Glk font automatically" },
		{ (char *) "-gd", glkunix_arg_NoValue,
	(char *) "-gd        Delay for the full period in Glk" },
		{ (char *) "-gh", glkunix_arg_NoValue,
	(char *) "-gh        Delay for approximately half the period in Glk" },
		{ (char *) "-gn", glkunix_arg_NoValue,
	(char *) "-gn        Turn off all game delays in Glk" },
		{ (char *) "-gr", glkunix_arg_NoValue,
	(char *) "-gr        Turn off Glk text replacement" },
		{ (char *) "-gx", glkunix_arg_NoValue,
	(char *) "-gx        Turn off Glk abbreviation expansions" },
		{ (char *) "-gs", glkunix_arg_NoValue,
	(char *) "-gs        Display a short status window in Glk" },
		{ (char *) "-gl", glkunix_arg_NoValue,
	(char *) "-gl        Display an extended status window in Glk" },
		{ (char *) "-gc", glkunix_arg_NoValue,
	(char *) "-gc        Turn off Glk command escapes in games" },
		{ (char *) "-gD", glkunix_arg_NoValue,
	(char *) "-gD        Turn on Glk port module debug tracing" },
		{ (char *) "-g#", glkunix_arg_NoValue,
	(char *) "-g#        Test for clean exit (Glk module debugging only)" },
		{ (char *) "-1", glkunix_arg_NoValue,
	(char *) "-1         IRUN Mode: Print messages in first person" },
		{ (char *) "-d", glkunix_arg_NoValue,
	(char *) "-d         Debug metacommand execution" },
		{ (char *) "-t", glkunix_arg_NoValue,
	(char *) "-t         Test mode" },
		{ (char *) "-c", glkunix_arg_NoValue,
	(char *) "-c         Create test file" },
		{ (char *) "-m", glkunix_arg_NoValue,
	(char *) "-m         Force descriptions to be loaded from disk" },
#ifdef OPEN_AS_TEXT
		{ (char *) "-b", glkunix_arg_NoValue,
	(char *) "-b         Open data files as binary files" },
#endif
		{ (char *) "-p", glkunix_arg_NoValue,
	(char *) "-p         Debug parser" },
		{ (char *) "-x", glkunix_arg_NoValue,
	(char *) "-x         Debug verb execution loop" },
		{ (char *) "-a", glkunix_arg_NoValue,
	(char *) "-a         Debug disambiguation system" },
		{ (char *) "-s", glkunix_arg_NoValue,
	(char *) "-s         Debug STANDARD message handler" },
#ifdef MEM_INFO
		{ (char *) "-M", glkunix_arg_NoValue,
	(char *) "-M         Debug memory allocation" },
#endif
		{ (char *) "", glkunix_arg_ValueCanFollow,
	(char *) "filename   game to run" },
{ NULL, glkunix_arg_End, NULL }
};
#endif


/*---------------------------------------------------------------------*/
/*  Glk port utility functions                                         */
/*---------------------------------------------------------------------*/

/*
 * gagt_fatal()
 *
 * Fatal error handler.  The function returns, expecting the caller to
 * abort() or otherwise handle the error.
 */
void
gagt_fatal (const char *string)
{
	/*
	 * If the failure happens too early for us to have a window, print
	 * the message to stderr.
	 */
	if (gagt_main_window == NULL)
	    {
		fprintf (stderr, "\n\nINTERNAL ERROR: ");
		fprintf (stderr, "%s", string);
		fprintf (stderr, "\n");

		fprintf (stderr, "\nPlease record the details of this error,");
		fprintf (stderr, " try to note down everything you did to");
		fprintf (stderr, " cause it, and email this information to");
		fprintf (stderr, " simon_baldwin@yahoo.com.\n\n");
		return;
	    }

	/* Cancel all possible pending window input events. */
	glk_cancel_line_event (gagt_main_window, NULL);
	glk_cancel_char_event (gagt_main_window);

	/* Print a message indicating the error, and exit. */
	glk_set_window (gagt_main_window);
	glk_set_style (style_Normal);
	glk_put_string ("\n\nINTERNAL ERROR: ");
	glk_put_string ((char *) string);
	glk_put_string ("\n");

	glk_put_string ("\nPlease record the details of this error,");
	glk_put_string (" try to note down everything you did to");
	glk_put_string (" cause it, and email this information to");
	glk_put_string (" simon_baldwin@yahoo.com.\n\n");
}


/*
 * gagt_malloc()
 * gagt_realloc()
 *
 * Non-failing malloc and realloc; call gagt_fatal() and exit if memory
 * allocation fails.
 */
static void *
gagt_malloc (size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Malloc, and call gagt_fatal() if the malloc fails. */
	pointer = malloc (size);
	if (pointer == NULL)
	    {
		gagt_fatal ("GLK: Out of system memory");
		gagt_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}

static void *
gagt_realloc (void *ptr, size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Realloc, and call gagt_fatal() if the realloc fails. */
	pointer = realloc (ptr, size);
	if (pointer == NULL)
	    {
		gagt_fatal ("GLK: Out of system memory");
		gagt_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}


/*
 * gagt_strncasecmp()
 * gagt_strcasecmp()
 *
 * Strncasecmp and strcasecmp are not ANSI functions, so here are local
 * definitions to do the same jobs.
 */
static int
gagt_strncasecmp (const char *s1, const char *s2, size_t n)
{
	size_t		index;			/* Strings iterator. */

	/* Compare each character, return +/- 1 if not equal. */
	for (index = 0; index < n; index++)
	    {
		if (glk_char_to_lower (s1[ index ])
				< glk_char_to_lower (s2[ index ]))
			return -1;
		else if (glk_char_to_lower (s1[ index ])
				> glk_char_to_lower (s2[ index ]))
			return  1;
	    }

	/* Strings are identical to n characters. */
	return 0;
}
static int
gagt_strcasecmp (const char *s1, const char *s2)
{
	size_t		s1len, s2len;		/* String lengths. */
	int		result;			/* Result of strncasecmp. */

	/* Note the string lengths. */
	s1len = strlen (s1);
	s2len = strlen (s2);

	/* Compare first to shortest length, return any definitive result. */
	result = gagt_strncasecmp (s1, s2, (s1len < s2len) ? s1len : s2len);
	if (result != 0)
		return result;

	/* Return the longer string as being the greater. */
	if (s1len < s2len)
		return -1;
	else if (s1len > s2len)
		return  1;

	/* Same length, and same characters -- identical strings. */
	return 0;
}


/*
 * gagt_debug()
 *
 * Handler for module debug output.
 */
static void
gagt_debug (const char *function, const char *format, ...)
{
	/* If debug is not enabled, ignore the call. */
	if (!DEBUG_OUT)
		return;

	/* Print the function name as the message introducer. */
	fprintf (debugfile, "%.16s", function);

	/* If there's more, follow up with details. */
	if (format != NULL
			&& strlen (format) > 0)
	    {
		int		width;		/* Dotted padding width. */
		va_list		ap;		/* Varargs. */

		/* Pad out to 16 columns. */
		for (width = strlen (function); width < 16; width++)
			fprintf (debugfile, ".");
		fprintf (debugfile, ": ");

		/* Print the debugging information. */
		va_start (ap, format);
		vfprintf (debugfile, format, ap);
		va_end (ap);
	    }

	/* End with a newline and flush. */
	fprintf (debugfile, "\n");
	fflush (debugfile);
}


/*---------------------------------------------------------------------*/
/*  Functions not ported - functionally unchanged from os_none.c       */
/*---------------------------------------------------------------------*/

/*
 * agt_tone()
 *
 * Produce a hz-Hertz sound for ms milliseconds.
 */
void
agt_tone (int hz, int ms)
{
	gagt_debug ("agt_tone", "hz=%d, ms=%d", hz, ms);
}


/*
 * agt_rand()
 *
 * Return random number from a to b inclusive.
 */
int
agt_rand (int a, int b)
{
	static int	initialized	= FALSE;	/* First call flag. */

	int		result;				/* Return value. */

	/* If this is the first call, seed the random number routines. */
	if (!initialized)
	    {
		/* Set either really random, or predictable, numbers. */
		if (stable_random)
			srand (6);
		else
			srand (time (0));

		/* Note that random numbers are now seeded. */
		initialized = TRUE;
		gagt_debug ("agt_rand", "[initialized]");
	    }

	/* Return the random number in the range requested. */
	result =  a + (rand () >> 2) % (b - a + 1);
	gagt_debug ("agt_rand", "a=%d, b=%d -> %d", a, b, result);
	return result;
}


/*---------------------------------------------------------------------*/
/*  Workrounds for bugs in core AGiliTy.                               */
/*---------------------------------------------------------------------*/

/*
 * gagt_workround_menus()
 *
 * Somewhere in * AGiliTy's menu handling stuff is a condition that sets up
 * an eventual NULL dereference in rstrncpy(), called from num_name_func().
 * For some reason, perhaps memory overruns, perhaps something else, it
 * happens after a few turns have been made through agt_menu().  Replacing
 * agt_menu() won't avoid it.
 *
 * However, the menu stuff isn't too useful, or attractive, in a game, so one
 * solution is to simply disable it.  While not possible to do this directly,
 * there is a sneaky way, using our carnal knowledge of core AGiliTy.  In
 * runverb.c, there is code to prevent menu mode from being turned on where
 * verbmenu is NULL.  Verbmenu is set up in agil.c on loading the game, but,
 * crucially, is set up before agil.c calls start_interface().  So... here
 * we can free it, set it to NULL, set menu_mode to 0 (it probably is already)
 * and AGiliTy behaves as if the game prevents menu mode.
 */
static void
gagt_workround_menus (void)
{
	/* Free and dispose of any verbmenu built on loading the game. */
	if (verbmenu != NULL)
	    {
		free (verbmenu);
		verbmenu = NULL;
	    }

	/* Set menu mode to off (for good, hopefully). */
	menu_mode = 0;
}


/*
 * gagt_workround_fileexist()
 *
 * This function verifies that the game file can be opened, in effect second-
 * guessing run_game().
 *
 * AGiliTy's fileexist() has in it either a bug, or a misfeature.  It always
 * passes a nofix value of 1 into try_open_file(), which defeats the code to
 * retry with both upper and lower cased filenames.  So here we have to go
 * round the houses, with readopen()/readclose().
 */
static int
gagt_workround_fileexist (fc_type fc, filetype ft)
{
	genfile		file;			/* Test file. */
	char		*errstr;		/* Readopen error. */
	int		result;			/* Returned result. */

	/* Initialize result to false, and error string to NULL. */
	result = FALSE;
	errstr = NULL;

	/*
	 * Try opening the file for read.  If it works, close it, and set
	 * the return result to TRUE.
	 */
	file = readopen (fc, ft, &errstr);
	if (file != NULL)
	    {
		result = TRUE;
		readclose (file);
	    }

	/*
	 * If AGiliTy allocated and returned an error string, free it.
	 * We're not interested in the contents.
	 */
	if (errstr != NULL)
		free (errstr);

	/* Return TRUE if we opened the file, FALSE otherwise. */
	return result;
}


/*---------------------------------------------------------------------*/
/*  I/O interface start and stop functions.                            */
/*---------------------------------------------------------------------*/

/* AGiliTy font_status values that indicate what font may be used. */
enum {	GAGT_FIXED_REQUIRED			= 1,
	GAGT_PROPORTIONAL_OKAY			= 2 };


/*
 * start_interface()
 * close_interface()
 *
 * Startup and shutdown callout points.  The start function for Glk looks
 * at the value of font_status that the game sets, to see if it has a strong
 * view of the font to use.  If it does, then we'll reflect that in the
 * module's font contol, perhaps overriding any command line options that the
 * user has passed in.
 */
void
start_interface (fc_type fc)
{
	/* See if the loaded game has indicated any form of font preference. */
	switch (font_status)
	    {
	    case GAGT_FIXED_REQUIRED:
		/* If the game forces fixed fonts, honor it. */
		gagt_font_mode	= FONT_FIXED_WIDTH;
		break;

	    case GAGT_PROPORTIONAL_OKAY:
		/* If the game permits proportional fonts, use them. */
		gagt_font_mode	= FONT_PROPORTIONAL;
		break;

	    default:
		break;
	    }

	/* Activate the workround for the menus bug in core AGiliTy. */
	gagt_workround_menus ();

	gagt_debug ("start_interface", "fc=%p", fc);
}

void
close_interface (void)
{
	/* Close script output file, if any. */
	if (filevalid (scriptfile, fSCR))
		close_pfile (scriptfile, 0);

	gagt_debug ("close_interface", "");
}


/*---------------------------------------------------------------------*/
/*  Code page 437 to ISO 8859 Latin-1 translations                     */
/*---------------------------------------------------------------------*/

/*
 * AGiliTy uses IBM code page 437 characters, and Glk works in ISO 8859
 * Latin-1.  There's some good news, in that a number of the characters,
 * especially international ones, in these two sets are the same.  The bad
 * news is that, for codes above 127 (that is, beyond 7-bit ASCII), or for
 * codes below 32, they live in different places.  So, here is a table of
 * conversions for codes not equivalent to 7-bit ASCII, and a pair of
 * conversion routines.
 *
 * Note that some code page 437 characters don't have ISO 8859 Latin-1
 * equivalents.  Predominantly, these are the box-drawing characters, which
 * is a pity, because these are the ones that are used the most.  Anyway,
 * in these cases, the table substitutes an approximated base ASCII char-
 * acter in its place.
 *
 * The first entry of table comments below is the character's UNICODE value,
 * just in case it's useful at some future date.
 */
struct gagt_char {
	const unsigned char	cp437;		/* Code page 437 code. */
	const unsigned char	iso8859_1;	/* ISO 8859 Latin-1 code. */
};
typedef const struct gagt_char*		gagt_charref_t;
static const struct gagt_char		GAGT_CHAR_TABLE[] = {
	/*
	 * Low characters -- those below 0x20.   These are the really odd code
	 * page 437 characters, rarely used by AGT games.  Low characters are
	 * omitted from the reverse lookup, and participate only in the
	 * forwards lookup from code page 437 to ISO 8859 Latin-1.
	 */
	{ 0x01, '@'  }, /* 263a White smiling face */
	{ 0x02, '@'  }, /* 263b Black smiling face */
	{ 0x03, '?'  }, /* 2665 Black heart suit */
	{ 0x04, '?'  }, /* 2666 Black diamond suit */
	{ 0x05, '?'  }, /* 2663 Black club suit */
	{ 0x06, '?'  }, /* 2660 Black spade suit */
	{ 0x07, 0xb7 }, /* 2022 Bullet */
	{ 0x08, 0xb7 }, /* 25d8 Inverse bullet */
	{ 0x09, 0xb7 }, /* 25e6 White bullet */
	{ 0x0A, 0xb7 }, /* 25d9 Inverse white circle */
	{ 0x0B, '?'  }, /* 2642 Male sign */
	{ 0x0C, '?'  }, /* 2640 Female sign */
	{ 0x0D, '?'  }, /* 266a Eighth note */
	{ 0x0E, '?'  }, /* 266b Beamed eighth notes */
	{ 0x0F, 0xa4 }, /* 263c White sun with rays */
	{ 0x10, '>'  }, /* 25b6 Black right-pointing triangle */
	{ 0x11, '<'  }, /* 25c0 Black left-pointing triangle */
	{ 0x12, 0xa6 }, /* 2195 Up down arrow */
	{ 0x13, '!'  }, /* 203c Double exclamation mark */
	{ 0x14, 0xb6 }, /* 00b6 Pilcrow sign */
	{ 0x15, 0xa7 }, /* 00a7 Section sign */
	{ 0x16, '#'  }, /* 25ac Black rectangle */
	{ 0x17, 0xa6 }, /* 21a8 Up down arrow with base */
	{ 0x18, '^'  }, /* 2191 Upwards arrow */
	{ 0x19, 'v'  }, /* 2193 Downwards arrow */
	{ 0x1A, '>'  }, /* 2192 Rightwards arrow */
	{ 0x1B, '<'  }, /* 2190 Leftwards arrow */
	{ 0x1C, '?'  }, /* 2310 Reversed not sign */
	{ 0x1D, '-'  }, /* 2194 Left right arrow */
	{ 0x1E, '^'  }, /* 25b2 Black up-pointing triangle */
	{ 0x1F, 'v'  }, /* 25bc Black down-pointing triangle */

	/*
	 * High characters -- those above 0x7F.  These are more often used
	 * by AGT games, particularly for box drawing.
	 */
	{ 0x80, 0xc7 }, /* 00c7 Latin capital letter c with cedilla */
	{ 0x81, 0xfc }, /* 00fc Latin small letter u with diaeresis */
	{ 0x82, 0xe9 }, /* 00e9 Latin small letter e with acute */
	{ 0x83, 0xe2 }, /* 00e2 Latin small letter a with circumflex */
	{ 0x84, 0xe4 }, /* 00e4 Latin small letter a with diaeresis */
	{ 0x85, 0xe0 }, /* 00e0 Latin small letter a with grave */
	{ 0x86, 0xe5 }, /* 00e5 Latin small letter a with ring above */
	{ 0x87, 0xe7 }, /* 00e7 Latin small letter c with cedilla */
	{ 0x88, 0xea }, /* 00ea Latin small letter e with circumflex */
	{ 0x89, 0xeb }, /* 00eb Latin small letter e with diaeresis */
	{ 0x8a, 0xe8 }, /* 00e8 Latin small letter e with grave */
	{ 0x8b, 0xef }, /* 00ef Latin small letter i with diaeresis */
	{ 0x8c, 0xee }, /* 00ee Latin small letter i with circumflex */
	{ 0x8d, 0xec }, /* 00ec Latin small letter i with grave */
	{ 0x8e, 0xc4 }, /* 00c4 Latin capital letter a with diaeresis */
	{ 0x8f, 0xc5 }, /* 00c5 Latin capital letter a with ring above */
	{ 0x90, 0xc9 }, /* 00c9 Latin capital letter e with acute */
	{ 0x91, 0xe6 }, /* 00e6 Latin small ligature ae */
	{ 0x92, 0xc6 }, /* 00c6 Latin capital ligature ae */
	{ 0x93, 0xf4 }, /* 00f4 Latin small letter o with circumflex */
	{ 0x94, 0xf6 }, /* 00f6 Latin small letter o with diaeresis */
	{ 0x95, 0xf2 }, /* 00f2 Latin small letter o with grave */
	{ 0x96, 0xfb }, /* 00fb Latin small letter u with circumflex */
	{ 0x97, 0xf9 }, /* 00f9 Latin small letter u with grave */
	{ 0x98, 0xff }, /* 00ff Latin small letter y with diaeresis */
	{ 0x99, 0xd6 }, /* 00d6 Latin capital letter o with diaeresis */
	{ 0x9a, 0xdc }, /* 00dc Latin capital letter u with diaeresis */
	{ 0x9b, 0xa2 }, /* 00a2 Cent sign */
	{ 0x9c, 0xa3 }, /* 00a3 Pound sign */
	{ 0x9d, 0xa5 }, /* 00a5 Yen sign */
	{ 0x9e, 'p'  }, /* 20a7 Peseta sign */
	{ 0x9f, 'f'  }, /* 0192 Latin small letter f with hook */
	{ 0xa0, 0xe1 }, /* 00e1 Latin small letter a with acute */
	{ 0xa1, 0xed }, /* 00ed Latin small letter i with acute */
	{ 0xa2, 0xf3 }, /* 00f3 Latin small letter o with acute */
	{ 0xa3, 0xfa }, /* 00fa Latin small letter u with acute */
	{ 0xa4, 0xf1 }, /* 00f1 Latin small letter n with tilde */
	{ 0xa5, 0xd1 }, /* 00d1 Latin capital letter n with tilde */
	{ 0xa6, 0xaa }, /* 00aa Feminine ordinal indicator */
	{ 0xa7, 0xba }, /* 00ba Masculine ordinal indicator */
	{ 0xa8, 0xbf }, /* 00bf Inverted question mark */
	{ 0xa9, '.'  }, /* 2310 Reversed not sign */
	{ 0xaa, 0xac }, /* 00ac Not sign */
	{ 0xab, 0xbd }, /* 00bd Vulgar fraction one half */
	{ 0xac, 0xbc }, /* 00bc Vulgar fraction one quarter */
	{ 0xad, 0xa1 }, /* 00a1 Inverted exclamation mark */
	{ 0xae, 0xab }, /* 00ab Left-pointing double angle quotation mark */
	{ 0xaf, 0xbb }, /* 00bb Right-pointing double angle quotation mark */
	{ 0xb0, '#'  }, /* 2591 Light shade */
	{ 0xb1, '#'  }, /* 2592 Medium shade */
	{ 0xb2, '#'  }, /* 2593 Dark shade */
	{ 0xb3, '|'  }, /* 2502 Box light vertical */
	{ 0xb4, '+'  }, /* 2524 Box light vertical and left */
	{ 0xb5, '+'  }, /* 2561 Box vertical single and left double */
	{ 0xb6, '|'  }, /* 2562 Box vertical double and left single */
	{ 0xb7, '+'  }, /* 2556 Box down double and left single */
	{ 0xb8, '+'  }, /* 2555 Box down single and left double */
	{ 0xb9, '+'  }, /* 2563 Box double vertical and left */
	{ 0xba, '|'  }, /* 2551 Box double vertical */
	{ 0xbb, '\\' }, /* 2557 Box double down and left */
	{ 0xbc, '/'  }, /* 255d Box double up and left */
	{ 0xbd, '+'  }, /* 255c Box up double and left single */
	{ 0xbe, '+'  }, /* 255b Box up single and left double */
	{ 0xbf, '\\' }, /* 2510 Box light down and left */
	{ 0xc0, '\\' }, /* 2514 Box light up and right */
	{ 0xc1, '+'  }, /* 2534 Box light up and horizontal */
	{ 0xc2, '+'  }, /* 252c Box light down and horizontal */
	{ 0xc3, '+'  }, /* 251c Box light vertical and right */
	{ 0xc4, '-'  }, /* 2500 Box light horizontal */
	{ 0xc5, '+'  }, /* 253c Box light vertical and horizontal */
	{ 0xc6, '|'  }, /* 255e Box vertical single and right double */
	{ 0xc7, '|'  }, /* 255f Box vertical double and right single */
	{ 0xc8, '\\' }, /* 255a Box double up and right */
	{ 0xc9, '/'  }, /* 2554 Box double down and right */
	{ 0xca, '+'  }, /* 2569 Box double up and horizontal */
	{ 0xcb, '+'  }, /* 2566 Box double down and horizontal */
	{ 0xcc, '+'  }, /* 2560 Box double vertical and right */
	{ 0xcd, '='  }, /* 2550 Box double horizontal */
	{ 0xce, '+'  }, /* 256c Box double vertical and horizontal */
	{ 0xcf, '='  }, /* 2567 Box up single and horizontal double */
	{ 0xd0, '+'  }, /* 2568 Box up double and horizontal single */
	{ 0xd1, '='  }, /* 2564 Box down single and horizontal double */
	{ 0xd2, '+'  }, /* 2565 Box down double and horizontal single */
	{ 0xd3, '+'  }, /* 2559 Box up double and right single */
	{ 0xd4, '+'  }, /* 2558 Box up single and right double */
	{ 0xd5, '+'  }, /* 2552 Box down single and right double */
	{ 0xd6, '+'  }, /* 2553 Box down double and right single */
	{ 0xd7, '+'  }, /* 256b Box vertical double and horizontal single */
	{ 0xd8, '+'  }, /* 256a Box vertical single and horizontal double */
	{ 0xd9, '/'  }, /* 2518 Box light up and left */
	{ 0xda, '/'  }, /* 250c Box light down and right */
	{ 0xdb, '@'  }, /* 2588 Full block */
	{ 0xdc, '@'  }, /* 2584 Lower half block */
	{ 0xdd, '@'  }, /* 258c Left half block */
	{ 0xde, '@'  }, /* 2590 Right half block */
	{ 0xdf, '@'  }, /* 2580 Upper half block */
	{ 0xe0, 'a'  }, /* 03b1 Greek small letter alpha */
	{ 0xe1, 0xdf }, /* 00df Latin small letter sharp s */
	{ 0xe2, 'G'  }, /* 0393 Greek capital letter gamma */
	{ 0xe3, 'p'  }, /* 03c0 Greek small letter pi */
	{ 0xe4, 'S'  }, /* 03a3 Greek capital letter sigma */
	{ 0xe5, 's'  }, /* 03c3 Greek small letter sigma */
	{ 0xe6, 0xb5 }, /* 00b5 Micro sign */
	{ 0xe7, 't'  }, /* 03c4 Greek small letter tau */
	{ 0xe8, 'F'  }, /* 03a6 Greek capital letter phi */
	{ 0xe9, 'T'  }, /* 0398 Greek capital letter theta */
	{ 0xea, 'O'  }, /* 03a9 Greek capital letter omega */
	{ 0xeb, 'd'  }, /* 03b4 Greek small letter delta */
	{ 0xec, '.'  }, /* 221e Infinity */
	{ 0xed, 'f'  }, /* 03c6 Greek small letter phi */
	{ 0xee, 'e'  }, /* 03b5 Greek small letter epsilon */
	{ 0xef, '^'  }, /* 2229 Intersection */
	{ 0xf0, '='  }, /* 2261 Identical to */
	{ 0xf1, 0xb1 }, /* 00b1 Plus-minus sign */
	{ 0xf2, '>'  }, /* 2265 Greater-than or equal to */
	{ 0xf3, '<'  }, /* 2264 Less-than or equal to */
	{ 0xf4, 'f'  }, /* 2320 Top half integral */
	{ 0xf5, 'j'  }, /* 2321 Bottom half integral */
	{ 0xf6, 0xf7 }, /* 00f7 Division sign */
	{ 0xf7, '='  }, /* 2248 Almost equal to */
	{ 0xf8, 0xb0 }, /* 00b0 Degree sign */
	{ 0xf9, 0xb7 }, /* 2219 Bullet operator */
	{ 0xfa, 0xb7 }, /* 00b7 Middle dot */
	{ 0xfb, '/'  }, /* 221a Square root */
	{ 0xfc, 'n'  }, /* 207f Superscript latin small letter n */
	{ 0xfd, 0xb2 }, /* 00b2 Superscript two */
	{ 0xfe, '#'  }, /* 25a0 Black square */
	{ 0xff, 0xa0 }, /* 00a0 No-break space */
	{    0,    0 }  /* 0000 [END OF TABLE] */
};


/*
 * gagt_cp_to_iso()
 *
 * Convert a string from code page 437 into ISO 8859 Latin-1.  The input and
 * output buffers may be one and the same.
 */
static void
gagt_cp_to_iso (const unsigned char *from_string, unsigned char *to_string)
{
	static int	initialized	= FALSE;/* First call flag. */
	static unsigned char
			table[UCHAR_MAX + 1];	/* Conversion lookup table. */

	int		index;			/* Loop index. */
	unsigned char	cp437;			/* Unconverted input. */
	unsigned char	iso8859_1;		/* Converted output. */
	assert (from_string != NULL && to_string != NULL);

	/* If this is the first call, build the lookup table. */
	if (!initialized)
	    {
		gagt_charref_t	entry;		/* Table iterator. */

		/*
		 * Create a lookup entry for each code in the main table.
		 * Fill in gaps for 7-bit characters with their ASCII equi-
		 * valent values.  Any remaining codes not represented in
		 * the main table will map to zeroes in the lookup table,
		 * as static variables are initialized to zero.
		 */
		for (entry = GAGT_CHAR_TABLE; entry->cp437 != 0; entry++)
		    {
			assert (entry->cp437 < 0x20
				|| (entry->cp437 > SCHAR_MAX
						&& entry->cp437 <= UCHAR_MAX));
			table[ entry->cp437 ] = entry->iso8859_1;
		    }
		for (index = 0; index <= SCHAR_MAX; index++)
		    {
			if (table[ index ] == 0)
				table[ index ] = index;
		    }

		/* Set the flag to indicate that the table is built. */
		initialized = TRUE;
	    }

	/* Convert each character in the string through the table. */
	for (index = 0; index < strlen (from_string); index++)
	    {
		cp437     = from_string[ index ];
		iso8859_1 = table[ cp437 ];

		to_string[ index ] = (iso8859_1 != 0) ? iso8859_1 : cp437;
	    }

	/* Terminate the return string. */
	to_string[ strlen (from_string) ] = '\0';
}


/*
 * gagt_iso_to_cp()
 *
 * Convert a string from ISO 8859 Latin-1 to code page 437.  The input and
 * output buffers may be one and the same.
 */
static void
gagt_iso_to_cp (const unsigned char *from_string, unsigned char *to_string)
{
	static int	initialized	= FALSE;/* First call flag. */
	static unsigned char
			table[UCHAR_MAX + 1];	/* Conversion lookup table. */

	int		index;			/* Loop index. */
	unsigned char	iso8859_1;		/* Unconverted input. */
	unsigned char	cp437;			/* Converted output. */
	assert (from_string != NULL && to_string != NULL);

	/* If this is the first call, build the lookup table. */
	if (!initialized)
	    {
		gagt_charref_t	entry;		/* Table iterator. */

		/*
		 * Create a reverse lookup entry for each code in the main
		 * table, overriding all of the low table entries (that is,
		 * anything under 128) with their ASCII no matter what the
		 * table contained.
		 *
		 * Any codes not represented in the main table will map to
		 * zeroes in the reverse lookup table, since static variables
		 * are initialized to zero.  The first 128 characters are
		 * equivalent to ASCII.  Moreover, some ISO 8859 Latin-1
		 * entries are faked as base ASCII; where an entry is already
		 * occupied, the main table entry is skipped, so the match,
		 * which is n:1 in the reverse direction, works in first-found
		 * mode.
		 */
		for (entry = GAGT_CHAR_TABLE; entry->iso8859_1 != 0; entry++)
		    {
			assert (entry->iso8859_1 <= UCHAR_MAX);
			if (table[ entry->iso8859_1 ] == 0)
				table[ entry->iso8859_1 ] = entry->cp437;
		    }
		for (index = 0; index <= SCHAR_MAX; index++)
			table[ index ] = index;

		/* Set the flag to indicate that the table is built. */
		initialized = TRUE;
	    }

	/* Convert each character in the string through the table. */
	for (index = 0; index < strlen (from_string); index++)
	    {
		iso8859_1 = from_string[ index ];
		cp437     = table[ iso8859_1 ];

		to_string[ index ] = (cp437 != 0) ? cp437 : iso8859_1;
	    }

	/* Terminate the return string. */
	to_string[ strlen (from_string) ] = '\0';
}


/*---------------------------------------------------------------------*/
/*  Glk port status line functions                                     */
/*---------------------------------------------------------------------*/

/*
 * Waiting indicator for the right hand side of the second status line,
 * and exits list introducer.
 */
static const char	*GAGT_STATUS_WAITING		= "Waiting... ";
static const char	*GAGT_STATUS_EXITS		= "  Exits: ";

/*
 * Buffered copy of the latest status line passed in by the interpreter.
 * Buffering it means it's readily available to print for Glk libraries
 * that don't support separate windows.  We also need a copy of the last
 * status buffer printed for non-windowing Glk libraries, for comparison.
 */
static char		*gagt_status_buffer		= NULL;
static char		*gagt_status_buffer_printed	= NULL;

/*
 * Indication that we are in mid-delay.  The delay is silent, and can look
 * kind of confusing, so to try to make it less so, we'll have the status
 * window show something about it.
 */
static int		gagt_inside_delay		= FALSE;


/*
 * agt_statline()
 *
 * This function is called from our call to print_statline().  Here we'll
 * convert and buffer the string for later use.
 */
void
agt_statline (const char *string)
{
	assert (string != NULL);

	/*
	 * Free any prior buffered status string, and allocate space for
	 * a copy of this one.
	 */
	if (gagt_status_buffer != NULL)
		free (gagt_status_buffer);
	gagt_status_buffer = gagt_malloc (strlen (string) + 1);

	/*
	 * Convert the status string from IBM cp 437 to Glk's ISO 8859
	 * Latin-1, and store in the status string buffer.
	 */
	gagt_cp_to_iso (string, gagt_status_buffer);

	gagt_debug ("agt_statline", "string='%s'", string);
}


/*
 * gagt_status_update_extended()
 *
 * Helper for gagt_status_update() and gagt_status_in_delay().  This function
 * displays the second line of any extended status display, giving a list of
 * exits from the compass rose, and if in an AGT delay, a waiting indicator.
 */
static void
gagt_status_update_extended (void)
{
	glui32		width, height;		/* Status window dimensions */
	strid_t		status_stream;		/* Status window stream */
	int		index;			/* Compass/string index. */
	assert (gagt_status_window != NULL);

	/* Measure the status window, and do nothing if no second line. */
	glk_window_get_size (gagt_status_window, &width, &height);
	if (height < 2)
		return;

	/* Clear the second status line only. */
	glk_window_move_cursor (gagt_status_window, 0, 1);
	status_stream = glk_window_get_stream (gagt_status_window);
	for (index = 0; index < width; index++)
		glk_put_char_stream (status_stream, ' ');

	/* Print the exits introducer at the left of the second line. */
	glk_window_move_cursor (gagt_status_window, 0, 1);
	glk_put_string_stream (status_stream, (char *) GAGT_STATUS_EXITS);

	/*
	 * Check bits in the compass rose, and print out exit names from
	 * the exitname[] array.
	 */
	for (index = 0;
			index < sizeof (exitname) / sizeof (exitname[0]);
			index++)
	    {
		/* Print the exit name if the compass bit is set. */
		if (compass_rose & (1 << index))
		    {
			glk_put_string_stream (status_stream,
						(char *) exitname[ index ]);
			glk_put_char_stream (status_stream, ' ');
		    }
	    }

	/* If the delay flag is set, print a waiting indicator. */
	if (gagt_inside_delay)
	    {
		/* Print the waiting indicator at line right edge. */
		glk_window_move_cursor (gagt_status_window,
				width - strlen (GAGT_STATUS_WAITING), 1);
		glk_put_string_stream (status_stream,
						(char *) GAGT_STATUS_WAITING);
	    }
}


/*
 * gagt_status_update()
 *
 *
 * This function calls print_statline() to prompt the interpreter into calling
 * our agt_statline(), then if we have a status window, displays the status
 * string, and calls gagt_status_update_extended() if necessary to handle the
 * second status line.  If we don't see a call to our agt_statline, we output
 * a default status string.
 */
static void
gagt_status_update (void)
{
	glui32		width, height;		/* Status window dimensions */
	strid_t		status_stream;		/* Status window stream */
	assert (gagt_status_window != NULL);

	/* Measure the status window, and do nothing if height is zero. */
	glk_window_get_size (gagt_status_window, &width, &height);
	if (height == 0)
		return;

	/* Clear the current status window contents, and position cursor. */
	glk_window_clear (gagt_status_window);
	glk_window_move_cursor (gagt_status_window, 0, 0);
	status_stream = glk_window_get_stream (gagt_status_window);

	/* Call print_statline() to refresh status line buffer contents. */
	print_statline ();

	/* See if we have a buffered status line available. */
	if (gagt_status_buffer != NULL)
	    {
		/*
		 * Print the basic buffered status string, truncating to the
		 * current status window width if necessary.
		 */
		glk_put_buffer_stream (status_stream, gagt_status_buffer,
				width < strlen (gagt_status_buffer)
					? width : strlen (gagt_status_buffer));

		/* If extended status enabled, try adding the second line. */
		if (gagt_extended_status_enabled)
			gagt_status_update_extended ();
	    }
	else
	    {
		/*
		 * We don't (yet) have a status line.  Perhaps we're at the
		 * very start of a game.  Print a standard message.
		 */
		glk_put_string_stream (status_stream,
						" Glk AGiliTy version 1.1.1");
	    }
}


/*
 * gagt_status_print()
 *
 * Print the current contents of the completed status line buffer out in the
 * main window, if it has changed since the last call.  This is for non-
 * windowing Glk libraries.
 *
 * Like gagt_status_update(), this function calls print_statline() to prompt
 * the interpreter into calling our agt_statline(), then if we have a new
 * status line, it prints it.
 */
static void
gagt_status_print (void)
{
	/* Call print_statline() to refresh status line buffer contents. */
	print_statline ();

	/*
	 * Do no more if there is no status line to print, or if the status
	 * line hasn't changed since last printed.
	 */
	if (gagt_status_buffer == NULL
			|| (gagt_status_buffer_printed != NULL
				&& !strcmp (gagt_status_buffer,
				    		gagt_status_buffer_printed)))
		return;

	/* Set fixed width font to try to preserve status line formatting. */
	glk_set_style (style_Preformatted);

	/*
	 * Bracket, and output the status line buffer.  We don't need to put
	 * any spacing after the opening bracket or before the closing one,
	 * because AGiliTy put leading/trailing spaces on its status lines.
	 */
	glk_put_string ("[");
	glk_put_string (gagt_status_buffer);
	glk_put_string ("]\n");

	/* Save the details of the printed status buffer. */
	if (gagt_status_buffer_printed != NULL)
		free (gagt_status_buffer_printed);
	gagt_status_buffer_printed = gagt_malloc
					(strlen (gagt_status_buffer) + 1);
	strcpy (gagt_status_buffer_printed, gagt_status_buffer);
}


/*
 * gagt_status_notify()
 *
 * Front end function for updating status.  Either updates the status window
 * or prints the status line to the main window.
 *
 * Functions interested in updating the status line should call either this
 * function, or gagt_status_redraw(), and not print_statline().
 */
static void
gagt_status_notify (void)
{
	/* Do nothing if in batch mode. */
	if (BATCH_MODE)
		return;

	/*
	 * If there is a status window, update it; if not, print the status
	 * line to the main window.
	 */
	if (gagt_status_window != NULL)
		gagt_status_update ();
	else
		gagt_status_print ();
}


/*
 * gagt_status_redraw()
 *
 * Redraw the contents of any status window with the buffered status string.
 * This function handles window sizing, and updates the interpreter with
 * status_width, so may, and should, be called on resize and arrange events.
 *
 * Functions interested in updating the status line should call either this
 * function, or gagt_status_notify(), and not print_statline().
 */
static void
gagt_status_redraw (void)
{
	/* Do nothing if in batch mode. */
	if (BATCH_MODE)
		return;

	/* If there is no status window, ignore the call. */
	if (gagt_status_window != NULL)
	    {
		glui32		width;		/* Status window width. */
		glui32		height;		/* Status window height. */
		winid_t		parent;		/* Status window parent. */

		/*
		 * Measure the status window, and update the interpreter's
		 * status_width variable.
		 */
		glk_window_get_size (gagt_status_window, &width, &height);
		status_width = width;

		/*
		 * Rearrange the status window, without changing its actual
		 * arrangement in any way.  This is a hack to work round
		 * incorrect window repainting in Xglk; it forces a complete
		 * repaint of affected windows on Glk window resize and
		 * arrange events, and works in part because Xglk doesn't
		 * check for actual arrangement changes in any way before
		 * invalidating its windows.  The hack should be harmless to
		 * Glk libraries other than Xglk, moreover, we're careful to
		 * activate it only on resize and arrange events.
		 */
		parent = glk_window_get_parent (gagt_status_window);
		glk_window_set_arrangement (parent,
					winmethod_Above|winmethod_Fixed,
		 			height, NULL);

		/* Update the status window display. */
		gagt_status_update ();
	    }
}


/*
 * gagt_status_in_delay()
 *
 * Tells status line functions whether the game is delaying, or not.  This
 * function updates the extended status line, if present, automatically.
 */
static void
gagt_status_in_delay (int inside_delay)
{
	/* Do nothing if in batch mode. */
	if (BATCH_MODE)
		return;

	/* Save the new delay status flag. */
	gagt_inside_delay = inside_delay;

	/*
	 * Update just the second line of the status window display, if
	 * extended status is being displayed.
	 */
	if (gagt_status_window != NULL
			&& gagt_extended_status_enabled)
		gagt_status_update_extended ();
}


/*
 * gagt_status_cleanup()
 *
 * Free memory resources allocated by status line functions.  Called on game
 * end.
 */
static void
gagt_status_cleanup (void)
{
	if (gagt_status_buffer != NULL)
	    {
		free (gagt_status_buffer);
		gagt_status_buffer = NULL;
	    }
	if (gagt_status_buffer_printed != NULL)
	    {
		free (gagt_status_buffer_printed);
		gagt_status_buffer_printed = NULL;
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk port color and text attribute handling                         */
/*---------------------------------------------------------------------*/

/*
 * AGT color and character attribute definitions.  This is the range of
 * values passed in to agt_textcolor().
 */
enum {			AGT_BLACK			=  0,
			AGT_BLUE			=  1,
			AGT_GREEN			=  2,
			AGT_CYAN			=  3,
			AGT_RED				=  4,
			AGT_MAGENTA			=  5,
			AGT_BROWN			=  6,
			AGT_NORMAL			=  7,
			AGT_BLINKING			=  8,
			AGT_WHITE			=  9,
			AGT_FIXED_FONT			= 10,
			AGT_VARIABLE_FONT		= 11,
			AGT_EMPHASIS			= -1,
			AGT_DE_EMPHASIS			= -2 };

/*
 * AGiliTy colors and text attributes seem a bit confused.  Let's see if we
 * can sort them out.  Sadly, once we have, it's often not possible to
 * render the full range in all Glk's anyway.  Nevertheless...
 */
typedef struct {
	int	color;			/* Text color. */
	int	blink;			/* Text blinking flag. */
	int	fixed;			/* Text fixed font flag. */
	int	emphasis;		/* Text emphasized flag. */
} gagt_attrset_t;

/*
 * Attributes as currently set by AGiliTy.  The default values set up here
 * correspond to AGT_NORMAL.
 */
gagt_attrset_t		gagt_current_attribute_set	= { AGT_WHITE,
							  FALSE, FALSE, FALSE };

/*
 * An extra flag to indicate if we have coerced fixed font override.  On
 * some occasions, we need to ensure that we get fixed font no matter what
 * the game says.
 */
static int		gagt_coerced_fixed		= FALSE;

/*
 * Bit masks for packing colors and attributes.  Normally, I don't like
 * bit-twiddling all that much, but for packing all of the above into a
 * single byte, that's what we need.  Stuff color into the low four bits,
 * convenient since color is from 0 to 9, then use three bits for the other
 * attributes.
 */
typedef unsigned char		gagt_attr_t;
static const gagt_attr_t	GAGT_COLOR_MASK		= 0x0F;
static const gagt_attr_t	GAGT_BLINK_MASK		= (1 << 4);
static const gagt_attr_t	GAGT_FIXED_MASK		= (1 << 5);
static const gagt_attr_t	GAGT_EMPHASIS_MASK	= (1 << 6);

/* Forward declaration of message function. */
static void		gagt_standout_string (const char *message);


/*
 * agt_textcolor()
 *
 * The AGiliTy porting guide defines the use of this function as:
 *
 *   Set text color to color #c, where the colors are as follows:
 *    0=Black, 1=Blue,    2=Green, 3=Cyan,
 *    4=Red,   5=Magenta, 6=Brown,
 *    7=Normal("White"-- which may actually be some other color)
 *       This should turn off blinking, bold, color, etc. and restore
 *       the text mode to its default appearance.
 *    8=Turn on blinking.
 *    9= *Just* White (not neccessarily "normal" and no need to turn off
 *        blinking)
 *   10=Turn on fixed pitch font.
 *   11=Turn off fixed pitch font
 *   Also used to set other text attributes:
 *     -1=emphasized text, used (e.g.) for room titles
 *     -2=end emphasized text
 *
 * Here we try to make sense of all this.  Given an argument, we'll try to
 * update our separated color and text attributes flags to reflect the
 * expected text rendering.
 */
void
agt_textcolor (int color)
{
	/* Try to sort out "color" into real color and text attributes. */
	switch (color)
	    {
	    case AGT_BLACK:
	    case AGT_BLUE:
	    case AGT_GREEN:
	    case AGT_CYAN:
	    case AGT_RED:
	    case AGT_MAGENTA:
	    case AGT_BROWN:
	    case AGT_WHITE:
		gagt_current_attribute_set.color	= color;
		break;

	    case AGT_NORMAL:
		gagt_current_attribute_set.color	= AGT_WHITE;
		gagt_current_attribute_set.blink	= FALSE;
		gagt_current_attribute_set.fixed	= FALSE;
		gagt_current_attribute_set.emphasis	= FALSE;
		break;

	    case AGT_BLINKING:
		gagt_current_attribute_set.blink	= TRUE;
		break;

	    case AGT_FIXED_FONT:
		gagt_current_attribute_set.fixed	= TRUE;
		break;

	    case AGT_VARIABLE_FONT:
		gagt_current_attribute_set.fixed	= FALSE;
		break;

	    case AGT_EMPHASIS:
		gagt_current_attribute_set.emphasis	= TRUE;
		break;

	    case AGT_DE_EMPHASIS:
		gagt_current_attribute_set.emphasis	= FALSE;
		break;

	    default:
		gagt_fatal ("GLK: Unknown color encountered");
		return;
	    }

	gagt_debug ("agt_textcolor", "color=% d -> %d%s%s%s", color,
			gagt_current_attribute_set.color,
			gagt_current_attribute_set.blink    ? " blink" : "",
			gagt_current_attribute_set.fixed    ? " fixed" : "",
			gagt_current_attribute_set.emphasis ? " bold"  : "");
}


/*
 * gagt_coerce_fixed_font()
 *
 * This coerces, or relaxes, a fixed font setting.  Used by box drawing, to
 * ensure that we get a temporary fixed font setting for known differenti-
 * ated parts of game output text.  Pass in TRUE to coerce fixed font, and
 * FALSE to relax it.
 */
static void
gagt_coerce_fixed_font (int coerce)
{
	/* Save the given fixed font coercion. */
	gagt_coerced_fixed = coerce;
}


/*
 * gagt_pack_attributes()
 *
 * Pack a set of color and text rendering attributes into a single byte,
 * and return it.  This function is used so that a set of text attributes
 * can be encoded into a byte array that parallels the output strings that
 * we buffer from the interpreter.
 */
static gagt_attr_t
gagt_pack_attributes (gagt_attrset_t *attribute_set, int coerced)
{
	gagt_attr_t	packed;		/* Return packed attributes. */
	assert (attribute_set != NULL);

	/* Set the initial result to be color; these are the low bits. */
	assert ((attribute_set->color & ~GAGT_COLOR_MASK) == 0); 
	packed = attribute_set->color;

	/*
	 * Now OR in the text attributes settings, taking either the value
	 * for fixed, or the coerced fixed font.
	 */
	if (attribute_set->blink)
		packed |= GAGT_BLINK_MASK;
	if (attribute_set->fixed
			|| coerced)
		packed |= GAGT_FIXED_MASK;
	if (attribute_set->emphasis)
		packed |= GAGT_EMPHASIS_MASK;

	/* Return the result. */
	return packed;
}


/*
 * gagt_unpack_attributes()
 *
 * Unpack a set of packed current color and text rendering attributes from a
 * single byte, and return the result of unpacking.  This reconstitutes the
 * text attributes that were current at the time of packing.
 */
static void
gagt_unpack_attributes (gagt_attr_t packed,
				gagt_attrset_t *attribute_set)
{
	assert (attribute_set != NULL);

	/* Extract the color attribute. */
	attribute_set->color	= (packed & GAGT_COLOR_MASK);
	attribute_set->blink	= (packed & GAGT_BLINK_MASK) != 0;
	attribute_set->fixed	= (packed & GAGT_FIXED_MASK) != 0;
	attribute_set->emphasis	= (packed & GAGT_EMPHASIS_MASK) != 0;
}


/*
 * gagt_pack_current_attributes()
 *
 * Pack the current color and text rendering attributes into a single byte,
 * and return it.
 */
static gagt_attr_t
gagt_pack_current_attributes (void)
{
	gagt_attr_t	packed;		/* Return packed attributes. */

	/*
	 * Pack the current text attributes, including any fixed font
	 * coercion flag.
	 */
	packed = gagt_pack_attributes (&gagt_current_attribute_set,
						gagt_coerced_fixed);

	/* Return the result. */
	return packed;
}


/*
 * gagt_init_user_styles()
 *
 * Attempt to set up two defined styles, User1 and User2, to represent
 * fixed font with AGT emphasis (rendered as Glk subheader), and fixed font
 * with AGT blink (rendered as Glk emphasis), respectively.
 *
 * The Glk stylehints here may not actually be honored by the Glk library.
 * We'll try to detect this later on.
 */
static void
gagt_init_user_styles (void)
{
	/*
	 * Set User1 to be fixed width, bold, and not italic.  Here we're
	 * sort of assuming that the style starts life equal to Normal.
	 */
	glk_stylehint_set (wintype_TextBuffer,
				style_User1, stylehint_Proportional, 0);
	glk_stylehint_set (wintype_TextBuffer,
				style_User1, stylehint_Weight, 1);
	glk_stylehint_set (wintype_TextBuffer,
				style_User1, stylehint_Oblique, 0);

	/*
	 * Set User2 to be fixed width, normal, and italic, with the same
	 * assumptions.
	  */
	glk_stylehint_set (wintype_TextBuffer,
				style_User2, stylehint_Proportional, 0);
	glk_stylehint_set (wintype_TextBuffer,
				style_User2, stylehint_Weight, 0);
	glk_stylehint_set (wintype_TextBuffer,
				style_User2, stylehint_Oblique, 1);
}


/*
 * gagt_confirm_appearance()
 *
 * Attempt to find out if a Glk style's on screen appearance matches a given
 * expectation.  There's a chance (often 100% with current Xglk) that we
 * can't tell, in which case we'll play safe, and say that it doesn't (our
 * caller is hoping does).
 *
 * That is, when we return FALSE, we mean either it's not as expected, or we
 * don't know.
 */
static int
gagt_confirm_appearance (glui32 style, glui32 stylehint, glui32 expected)
{
	glui32		result;		/* Stylehint measure result. */

	/* Query the style for this stylehint. */
	if (glk_style_measure (gagt_main_window, style, stylehint, &result))
	    {
		/*
		 * Measurement succeeded, so return TRUE if the result matches
		 * the caller's expectation.
		 */
		if (result == expected)
			return TRUE;
	    }

	/* No straight answer, or the style's stylehint failed to match. */
	return FALSE;
}


/*
 * gagt_is_style_fixed()
 * gagt_is_style_bold()
 * gagt_is_style_oblique()
 *
 * Convenience functions for gagt_select_style().  A return of TRUE
 * indicates that the style has this attribute; FALSE indicates either that
 * it hasn't, or that it's not determinable.
 */
static int
gagt_is_style_fixed (glui32 style)
{
	return gagt_confirm_appearance (style, stylehint_Proportional, 0);
}

static int
gagt_is_style_bold (glui32 style)
{
	return gagt_confirm_appearance (style, stylehint_Weight, 1);
}

static int
gagt_is_style_oblique (glui32 style)
{
	return gagt_confirm_appearance (style, stylehint_Oblique, 1);
}


/*
 * gagt_select_style()
 *
 * Given a set of AGT text attributes, this function returns a Glk style that
 * is suitable (or more accurately, the best we can come up with) for render-
 * ing this set of attributes.
 *
 * For now, we ignore color totally, and just concentrate on the other attr-
 * ibutes.  This is because few, if any, games use color (no Photopia here),
 * few Glk libraries, at least on Linux, allow fine grained control over text
 * color, and even if you can get it, the scarcity of user-defined styles in
 * Glk makes it too painful to contemplate.
 */
static glui32
gagt_select_style (gagt_attrset_t *attribute_set)
{
	glui32		style;			/* Return Glk style. */
	assert (attribute_set != NULL);

	/*
	 * Glk styles are mutually exclusive, so here we'll work here by
	 * making a precedence selection: AGT emphasis take precedence over
	 * AGT blinking, which itself takes precedence over normal text.
	 * Fortunately, few, if any, AGT games set both emphasis and blinking
	 * (not likely to be a pleasant combination).
	 *
	 * We'll try to map AGT emphasis to Glk Subheader, AGT blink to Glk
	 * Emphasized, and normal text to Glk Normal, with modifications to
	 * this for fixed width requests.
	 *
	 * First, then, see if emphasized text is requested in the attributes.
	 */
	if (attribute_set->emphasis)
	    {
		/*
		 * Consider whether something requested a fixed width font or
		 * disallowed a proportional one.
		 *
		 * Glk Preformatted is boring, flat, and lifeless.  It often
		 * offers no fine grained control over emphasis, and so on.
		 * So here we try to find something better.  However, not all
		 * Glk libraries implement stylehints, so we need to try to
		 * be careful to ensure that we get a fixed width font, no
		 * matter what else we may miss out on.
		 */
		if (attribute_set->fixed)
		    {
			/*
			 * To start off, we'll see if User1, the font we set
			 * up for fixed width bold, really is fixed width and
			 * bold.  If it is, we'll use it.
			 *
			 * If it isn't, we'll check Subheader.  Our Glk library
			 * probably isn't implementing stylehints, but if
			 * Subheader is fixed width, it may provide a better
			 * look than Preformatted -- certainly it's worth a go.
			 *
			 * If Subheader isn't fixed width, we'll take another
			 * look at User1.  It could be that the check for bold
			 * wasn't definitive, but it is nevertheless bold.  So
			 * check for fixed width -- if set, it's probably good
			 * enough to use this font, certainly no worse than
			 * Preformatted.
			 *
			 * If Subheader isn't guaranteed fixed width, nor is
			 * User1, we're cornered into Preformatted.
			 */
			if (gagt_is_style_fixed (style_User1)
					&& gagt_is_style_bold (style_User1))
				style	= style_User1;

			else if (gagt_is_style_fixed (style_Subheader))
				style	= style_Subheader;

			else if (gagt_is_style_fixed (style_User1))
				style	= style_User1;

			else
				style	= style_Preformatted;
		    }
		else
			/* This is the easy case, use Subheader. */
			style	= style_Subheader;
	    }
	else if (attribute_set->blink)
	    {
		/*
		 * Again, consider whether something requested a fixed width
		 * font or disallowed a proportional one.
		 */
		if (attribute_set->fixed)
		    {
			/*
			 * As above, try to find something better than Pre-
			 * formatted, first trying User2, then Emphasized,
			 * then User2 again, and finally settling for Prefor-
			 * matted if neither of these two looks any better.
			 */
			if (gagt_is_style_fixed (style_User2)
					&& gagt_is_style_oblique
								(style_User2))
				style	= style_User2;

			else if (gagt_is_style_fixed (style_Emphasized))
				style	= style_Emphasized;

			else if (gagt_is_style_fixed (style_User2))
				style	= style_User2;

			else
				style	= style_Preformatted;
		    }
		else
			/* This is the easy case, use Emphasized. */
			style	= style_Emphasized;
	    }
	else
	    {
		/*
		 * There's no emphasis or blinking in the attributes.  In this
		 * case, use Preformatted for fixed width, and Normal for text
		 * that can be rendered proportionally.
		 */
		if (attribute_set->fixed)
			style	= style_Preformatted;
		else
			style	= style_Normal;
	    }

	/* Return the style we settled on. */
	return style;
}


/*---------------------------------------------------------------------*/
/*  Glk port output buffering functions                                */
/*---------------------------------------------------------------------*/

/*
 * Buffering game output happens at two levels.  The first level is a single
 * line buffer, used to catch text sent to us with agt_puts().  In parallel
 * with the text strings, we keep and buffer the game text attributes, as
 * handed to agt_textcolor(), that are in effect at the time the string is
 * handed to us, packed for brevity.
 *
 * As each line is completed, by a call to agt_newline(), this single line
 * buffer is transferred to a main text page buffer.  The main page buffer
 * has places in it where we can assign paragraph numbers, font hints, and
 * perhaps other marker information to a line.  Initially unset, they're
 * filled in at the point where we need to display the buffer.
 */

/*
 * Increment used to grow the line buffer.  This is approximately 1/5 of the
 * longest line we'd expect to see from the interpreter (around 80 charac-
 * ters), so we anticipate needing to grow a line no more than five times.
 * No null terminator is required.
 */
static const int	GAGT_BUFFER_INCREMENT		= 16;

/*
 * Definition of the single (current) line buffer.  This is a growable string
 * and a parallel growable attributes array, where size defines the allocated
 * space, and length the used space.  The string is buffered without any null
 * terminator -- not needed since we retain length.
 */
static unsigned char	*gagt_line_buffer		= NULL;
static gagt_attr_t	*gagt_line_attributes		= NULL;
static int		gagt_line_size			= 0;
static int		gagt_line_length		= 0;

/*
 * Definition of font hints values.  Font hints may be:
 *   o none, for lines not in a definite paragraph;
 *   o proportional, for lines that can probably be safely rendered in a
 *     proportional font (if the AGT game text attributes allow it) and
 *     where the newline may be replaced by a space;
 *   o proportional_newline, for lines that may be rendered using a
 *     proportional font, but where the newline looks like it matters;
 *   o proportional_newline_standout, for proportional_newline lines that
 *     are also standout (for spacing in display functions);
 *   o fixed_width, for tables and other text that looks like it is a
 *     candidate for fixed font output.
 */
typedef enum {		HINT_NONE,
			HINT_PROPORTIONAL,
			HINT_PROPORTIONAL_NEWLINE,
			HINT_PROPORTIONAL_NEWLINE_STANDOUT,
			HINT_FIXED_WIDTH }
gagt_font_hint_t;

/* Magic number used to ensure a pointer points to a page buffer line. */
static const unsigned int
			GAGT_LINE_MAGIC			= 0x5BC14482;

/* Paragraph number for lines not in any paragraph. */
static const int	GAGT_NO_PARAGRAPH		= -1;

/*
 * Definition of a page buffer entry.  This is a structure that holds the
 * the result of a single line buffer above, plus additional areas that
 * describe line text positioning, a blank line flag, a paragraph number
 * (-1 if not in a paragraph), a special paragraph table entry pointer
 * (NULL if none), and a font hint.
 */
typedef struct gagt_line*		gagt_lineref_t;
typedef const struct gagt_special*	gagt_specialref_t;
struct gagt_line {
	unsigned int		magic;		/* Assertion check dog-tag. */
	unsigned char		*string;	/* Buffer string data. */
	gagt_attr_t		*attributes;	/* Parallel attributes. */
	int			size;		/* String/attributes alloc'n. */
	int			length;		/* String/attributes space. */

	int			indent;		/* Line indentation. */
	int			outdent;	/* Trailing line whitespace. */
	int			real_length;	/* Real line length. */
	int			is_blank_line;	/* Line blank flag. */
	int			is_hyphenated;	/* Line hyphenated flag. */

	int			paragraph;	/* Paragraph sequence number. */
	gagt_specialref_t	special;	/* Special paragraph entry. */
	gagt_font_hint_t	font_hint;	/* Line's font hint. */

	gagt_lineref_t		next;		/* List next element. */
	gagt_lineref_t		prior;		/* List prior element. */
};

/*
 * Definition of the actual page buffer.  This is a doubly-linked list of
 * lines, with a tail pointer to facilitate adding entries at the end.
 */
static gagt_lineref_t	gagt_page_head		= NULL;
static gagt_lineref_t	gagt_page_tail		= NULL;


/*
 * gagt_output_delete()
 *
 * Delete all buffered page and line text.  Free all malloc'ed buffer memory,
 * and return the buffer variables to their initial values.
 */
static void
gagt_output_delete (void)
{
	gagt_lineref_t	line, next_line;	/* Page buffer iterators. */

	/* Destroy each line held in the page buffer. */
	for (line = gagt_page_head; line != NULL; line = next_line)
	    {
		assert (line->magic == GAGT_LINE_MAGIC);
		next_line = line->next;

		/* Free the allocated fields of the line. */
		if (line->string != NULL)
			free (line->string);
		if (line->attributes != NULL)
			free (line->attributes);

		/* Shred, then destroy the line entry itself. */
		memset (line, 0, sizeof (*line));
		free (line);
	    }

	/* Return the page buffer to an empty state. */
	gagt_page_head	= NULL;
	gagt_page_tail	= NULL;

	/* Free the line buffer. */
	if (gagt_line_buffer != NULL)
	    {
		free (gagt_line_buffer);
		gagt_line_buffer	= NULL;
	    }
	if (gagt_line_attributes != NULL)
	    {
		free (gagt_line_attributes);
		gagt_line_attributes	= NULL;
	    }
	gagt_line_size   = 0;
	gagt_line_length = 0;
}


/*
 * agt_puts()
 *
 * Buffer the string passed in into our current single line buffer.  The
 * function converts to ISO 8859 Latin-1 encoding before buffering.
 */
void
agt_puts (const char *string)
{
	char		*cvt_string;		/* ISO 8859 conversion. */
	int		required_size;		/* Size needed to buffer. */
	gagt_attr_t	packed;			/* Packed char attributes. */
	assert (string != NULL);

	/* Do nothing if in batch mode. */
	if (BATCH_MODE)
		return;

	/* Update the apparent (virtual) window x position. */
	curr_x += strlen (string);

	/* Convert the buffer from IBM cp 437 to Glk's ISO 8859 Latin-1. */
	cvt_string = gagt_malloc (strlen (string) + 1);
	gagt_cp_to_iso (string, cvt_string);

	/*
	 * Find the size we'll need from the line buffer to add this string,
	 * rounded up to an integral number of increments.
	 */
	required_size = gagt_line_length + strlen (cvt_string);
	required_size = ((required_size + GAGT_BUFFER_INCREMENT - 1)
					/ GAGT_BUFFER_INCREMENT)
						* GAGT_BUFFER_INCREMENT;

	/*
	 * If it's necessary, grow the text line and attribute buffers to
	 * the required size.
	 */
	if (required_size > gagt_line_size)
	    {
		int	bytes;			/* Size of realloc in bytes. */

		/* Reallocate the two buffered arrays. */
		bytes = required_size * sizeof (*gagt_line_buffer);
		gagt_line_buffer = gagt_realloc
					    (gagt_line_buffer, bytes);

		bytes = required_size * sizeof (*gagt_line_attributes);
		gagt_line_attributes = gagt_realloc
					    (gagt_line_attributes, bytes);

		/* Update the buffer size. */
		gagt_line_size = required_size;
	    }

	/* Add the string to the line buffer. */
	memcpy (gagt_line_buffer + gagt_line_length,
					cvt_string, strlen (cvt_string));

	/* Get the current packed text attributes, and store these too. */
	packed = gagt_pack_current_attributes ();
	memset (gagt_line_attributes + gagt_line_length,
					packed, strlen (cvt_string));

	/* Update the buffer length to reflect the new data. */
	gagt_line_length += strlen (cvt_string);

	/* Add the string to any script file. */
	if (script_on)
		textputs (scriptfile, cvt_string);

	/* Finished with malloc'ed conversion string. */
	free (cvt_string);

	gagt_debug ("agt_puts", "string='%s'", string);
}


/*
 * agt_newline()
 *
 * Accept a newline to the main window.  Our job here is to append the
 * current line buffer to the page buffer, and clear the line buffer to
 * begin accepting new text.
 */
void
agt_newline (void)
{
	int			index;		/* Character iterator. */
	gagt_lineref_t		line;		/* New page buffer line. */

	/* Do nothing if in batch mode. */
	if (BATCH_MODE)
		return;

	/* Update the apparent (virtual) window x position. */
	curr_x = 0;

	/* Create a new line entry for the page buffer. */
	line			= gagt_malloc (sizeof (*line));
	line->magic		= GAGT_LINE_MAGIC;

	/* Move the line from the line buffer into the page buffer. */
	line->string		= gagt_line_buffer;
	line->attributes	= gagt_line_attributes;
	line->size		= gagt_line_size;
	line->length		= gagt_line_length;

	gagt_line_buffer	= NULL;
	gagt_line_attributes	= NULL;
	gagt_line_size		= 0;
	gagt_line_length	= 0;

	/* Fill in the line indent field. */
	line->indent		= 0;
	for (index = 0; index < line->length
			&& isspace (line->string[ index ]); index++)
		line->indent++;

	/* Fill in the line outdent. */
	line->outdent		= 0;
	for (index = line->length - 1; index >= 0
			&& isspace (line->string[ index ]); index--)
		line->outdent++;

	/* Fill in the effective line length and blank line indicator. */
	if (line->indent == line->length)
	    {
		assert (line->outdent == line->length);
		line->real_length = 0;
		line->is_blank_line = TRUE;
	    }
	else
	    {
		line->real_length = line->length
					- line->indent - line->outdent;
		line->is_blank_line = FALSE;
	    }

	/* See if the line ends in a hyphen that breaks a word. */
	line->is_hyphenated	= FALSE;
	if (!line->is_blank_line
			&& line->real_length > 1)
	    {
		index = line->length - line->outdent - 1;

		/* Look for a hyphen preceded by an alphabetic. */
		if (line->string[ index ] == '-')
		    {
			if (isalpha (line->string[ index - 1 ]))
				line->is_hyphenated = TRUE;
		    }
	    }

	/* For now, default the remaining page buffer fields for the line. */
	line->paragraph		= GAGT_NO_PARAGRAPH;
	line->special		= NULL;
	line->font_hint		= HINT_NONE;

	/* Set line's next and prior list links. */
	line->next		= NULL;
	line->prior		= gagt_page_tail;

	/*
	 * Either start a new page buffer list with this line, or add it to
	 * the end of the existing page buffer list.
	 */
	if (gagt_page_head == NULL)
	    {
		assert (gagt_page_tail == NULL);
		gagt_page_head		= line;
	    }
	else
	    {
		assert (gagt_page_tail != NULL);
		gagt_page_tail->next	= line;
	    }
	gagt_page_tail	= line;

	/* Add the string to any script file. */
	if (script_on)
		textputs (scriptfile, "\n");

	gagt_debug ("agt_newline", "");
}


/*
 * gagt_get_first_page_line()
 * gagt_get_next_page_line()
 * gagt_get_prior_page_line()
 *
 * Iterator functions for the page buffer.  These functions return the first
 * line from the page buffer, the next line, or the previous line, given a
 * line, respectively.  They return NULL if no lines, or no more lines, are
 * available.
 */
static gagt_lineref_t
gagt_get_first_page_line (void)
{
	gagt_lineref_t		line;		/* First line to return. */

	/* Access the first line, check it, and return */
	line = gagt_page_head;
	assert (line == NULL || line->magic == GAGT_LINE_MAGIC);
	return line;
}

static gagt_lineref_t
gagt_get_next_page_line (gagt_lineref_t line)
{
	gagt_lineref_t		next_line;	/* Next line to return. */
	assert (line != NULL && line->magic == GAGT_LINE_MAGIC);

	/* Access the next line, check it, and return */
	next_line = line->next;
	assert (next_line == NULL || next_line->magic == GAGT_LINE_MAGIC);
	return next_line;
}

static gagt_lineref_t
gagt_get_prior_page_line (gagt_lineref_t line)
{
	gagt_lineref_t		prior_line;	/* Previous line to return. */
	assert (line != NULL && line->magic == GAGT_LINE_MAGIC);

	/* Access the previous line, check it, and return */
	prior_line = line->prior;
	assert (prior_line == NULL || prior_line->magic == GAGT_LINE_MAGIC);
	return prior_line;
}


/*
 * gagt_get_first_paragraph_line()
 * gagt_get_next_paragraph_line()
 * gagt_get_prior_paragraph_line()
 *
 * Iterator functions for the page buffer.  These functions implement a
 * paragraph-based view of the page buffer.
 *
 * The functions find the first line of a given paragraph; given a line,
 * the next line in the same paragraph, or NULL if line is the last para-
 * graph line (or the last line in the page buffer); and given a line,
 * the previous line in the same paragraph, or NULL if line is the first
 * paragraph line (or the first line in the page buffer).
 */
static gagt_lineref_t
gagt_get_first_paragraph_line (int paragraph)
{
	gagt_lineref_t		line;		/* Page buffer line iterator. */
	gagt_lineref_t		result;		/* Return paragraph line. */
	assert (paragraph >= 0);

	/* Search for and return the first paragraph line. */
	result = NULL;
	for (line = gagt_get_first_page_line ();
			line != NULL;
			line = gagt_get_next_page_line (line))
	    {
		if (line->paragraph == paragraph)
		    {
			result = line;
			break;
		    }
	    }

	/* Return the line found; NULL if no lines for this paragraph. */
	return result;
}

static gagt_lineref_t
gagt_get_next_paragraph_line (gagt_lineref_t line)
{
	gagt_lineref_t		next_line;	/* Next line to return. */

	/* Get the next line, and return it if the paragraph matches. */
	next_line = gagt_get_next_page_line (line);
	if (next_line != NULL
			&& next_line->paragraph == line->paragraph)
		return next_line;

	/* No next line, or no paragraph match, so return NULL. */
	return NULL;
}

static gagt_lineref_t
gagt_get_prior_paragraph_line (gagt_lineref_t line)
{
	gagt_lineref_t		prior_line;	/* Previous line to return. */

	/* Get the previous line, and return it if the paragraph matches. */
	prior_line = gagt_get_prior_page_line (line);
	if (prior_line != NULL
			&& prior_line->paragraph == line->paragraph)
		return prior_line;

	/* No prior line, or no paragraph match, so return NULL. */
	return NULL;
}


/*---------------------------------------------------------------------*/
/*  Glk port special paragraph functions and data                      */
/*---------------------------------------------------------------------*/

/*
 * It's helpful to handle some AGiliTy interpreter output specially, to im-
 * prove the look of the text where Glk fonts and styles are available.  We
 * build a table of paragraphs the interpreter can come out with, and the
 * replacement text we'll use when we see this paragraph.  Note that matches
 * are made after factoring out indentation, replacement lines do not auto-
 * matically print with a newline, and there is no connection between the
 * maximum number of match lines and the maximum number of replacements.
 * All clear, then?  Here's the table entry definition.
 */
enum {			GAGT_SPECIAL_MATCH_MAX		= 8,
			GAGT_SPECIAL_REPLACE_MAX	= 8 };
struct gagt_replace {
	const char	*text;		/* A line of replacement text. */
	const glui32	style;		/* Glk style to use for this line. */
};
struct gagt_special {
	const char			*match[GAGT_SPECIAL_MATCH_MAX];
	const struct gagt_replace	replace[GAGT_SPECIAL_REPLACE_MAX];
};
typedef const struct gagt_replace*	gagt_replaceref_t;

/*
 * Table of special AGiliTy interpreter strings and paragraphs -- where one
 * appears in game output, we'll print out its replacement instead.  Be
 * warned; these strings are VERY specific to AGiliTy 1.1.1, and are extre-
 * mely likely to change with any future interpreter releases.  They also
 * omit initializers with abandon, expecting the compiler to default these
 * to NULL/zero.
 */
static const struct gagt_special	GAGT_SPECIALS[] = {

/* Initial screen AGT game type line. */
{ { "[Created with Malmberg and Welch's Adventure Game Toolkit]" },
 {{ "Created with Malmberg and Welch's Adventure Game Toolkit\n",
							style_Emphasized }} },

/* Normal version of initial interpreter information block. */
{ { "This game is being executed by",
    "AGiliTy: The (Mostly) Universal AGT Interpreter  version 1.1.1",
    "Copyright (C) 1996-99,2001 by Robert Masenten",
    "Glk version" },
 {{ "This game is being executed by:\n\n    ",		style_Normal },
  { "AGiliTy, The (Mostly) Universal AGT Interpreter, Version 1.1.1",
							style_Subheader },
  { "\n    ",						style_Normal },
  { "Copyright (C) 1996-1999,2001 by Robert Masenten",	style_Emphasized },
  { "\n    ",						style_Normal },
  { "Glk version",					style_Emphasized },
  { "\n",						style_Normal }} },

/* AGiliTy "information" screen header block. */
{ { "AGiliTy",
    "The (Mostly) Universal AGT Interpreter, version 1.1.1",
    "Copyright (C) 1996-1999,2001 by Robert Masenten",
    "[Glk version]",
    "-----------------------------------------------------------" },
 {{ "AGiliTy, The (Mostly) Universal AGT Interpreter, Version 1.1.1\n",
							style_Subheader },
  { "Copyright (C) 1996-1999,2001 by Robert Masenten\n",
							style_Emphasized },
  { "Glk version\n",					style_Emphasized }} },

/* "HIT ANY KEY" message, usually displayed after a game's introduction. */
{ { "--- HIT ANY KEY ---" },
 {{ "[Press any key...]",				style_Emphasized }} },

/* Alternative, shrunken version of initial interpreter information block. */
{ { "Being run by AGiliTy  version 1.1.1, Copyright (C) 1996-99,2001"
	" Robert Masenten",
    "Glk version" },
 {{ "This game is being executed by:\n\n    ",		style_Normal },
  { "AGiliTy, The (Mostly) Universal AGT Interpreter, Version 1.1.1",
							style_Subheader },
  { "\n    ",						style_Normal },
  { "Copyright (C) 1996-1999,2001 by Robert Masenten",	style_Emphasized },
  { "\n    ",						style_Normal },
  { "Glk version",					style_Emphasized },
  { "\n",						style_Normal }} },

/* Alternative, minimal version of initial interpreter information block. */
{ { "Being run by AGiliTy  version 1.1.1, Copyright (C) 1996-99,2001"
	" Robert Masenten" },
 {{ "This game is being executed by:\n\n    ",		style_Normal },
  { "AGiliTy, The (Mostly) Universal AGT Interpreter, Version 1.1.1",
							style_Subheader },
  { "\n    ",						style_Normal },
  { "Copyright (C) 1996-1999,2001 by Robert Masenten",	style_Emphasized },
  { "\n    ",						style_Normal },
  { "Glk version",					style_Emphasized },
  { "\n",						style_Normal }} },

/* Lengthy version of the "Created with..." message. */
{ { "This game was created with Malmberg and Welch's Adventure Game Toolkit;"
	" it is",
    "being executed by" },
 {{ "Created with Malmberg and Welch's Adventure Game Toolkit\n",
							style_Emphasized }} },

/* Three-line version of initial interpreter information block. */
{ { "AGiliTy: The (Mostly) Universal AGT Interpreter  version 1.1.1",
    "Copyright (C) 1996-99,2001 by Robert Masenten",
    "Glk version" },
 {{ "This game is being executed by:\n\n    ",		style_Normal },
  { "AGiliTy, The (Mostly) Universal AGT Interpreter, Version 1.1.1",
							style_Subheader },
  { "\n    ",						style_Normal },
  { "Copyright (C) 1996-1999,2001 by Robert Masenten",	style_Emphasized },
  { "\n    ",						style_Normal },
  { "Glk version",					style_Emphasized },
  { "\n",						style_Normal }} },

/*
 * Assorted special verb output messages, with the extra icky quality that
 * we have to spot messages that wrap because we forced screen_width to 80.
 */
{ { "[Now in BRIEF mode (room descriptions will only be printed"
	" when they are entered",
    "the first time)]" },
 {{ "[Now in BRIEF mode: Room descriptions will only be printed"
	" when they are entered for the first time.]\n",style_Emphasized }} },

{ { "[Now in VERBOSE mode (room descriptions will be"
	" printed every time you enter a",
    "room)]" },
 {{ "[Now in VERBOSE mode: Room descriptions will be"
	" printed every time you enter a room.]\n",	style_Emphasized }} },

{ { "[LISTEXIT mode on: room exits will be listed.]" },
 {{ "[LISTEXIT mode on: Room exits will be listed.]\n",	style_Emphasized }} },

{ { "[LISTEXIT mode off: room exits will not be listed.]" },
 {{ "[LISTEXIT mode off: Room exits will not be listed.]\n",
							style_Emphasized }} },

/* End of table sentinel entry.  Do not delete. */
{ { NULL }, {{ NULL, 0 }}}
};


/*
 * gagt_mark_special()
 *
 * Given a paragraph, see if it matches any of the special ones set up in
 * our array.  If it does, set the special marker field for each line in the
 * paragraph to the relevant special table entry pointer, and return TRUE.
 * Return FALSE if the paragraph matches no known special one.
 */
static void
gagt_mark_special (int paragraph)
{
	int			paragraph_lines;/* Lines in the paragraph */
	gagt_specialref_t	special;	/* Special table iterator */
	gagt_lineref_t		line;		/* Line iterator */
	gagt_specialref_t	match;		/* Table match special */
	assert (paragraph >= 0);

	/* Count the number of lines in the paragraph passed in. */
	paragraph_lines = 0;
	for (line = gagt_get_first_paragraph_line (paragraph);
			line != NULL;
			line = gagt_get_next_paragraph_line (line))
		paragraph_lines++;

	/*
	 * Iterate over each special paragraph entry, stopping at the end
	 * sentinel, defined as an entry with match[0] set to NULL.
	 */
	match = NULL;
	for (special = GAGT_SPECIALS;
			special->match[0] != NULL && match == NULL;
			special++)
	    {
		int		index;			/* Special line */
		int		special_lines;		/* Special line count */

		/*
		 * Count the lines for this special entry; this is either
		 * the maximum allowed, or the non-NULL match strings found.
		 */
		special_lines = 0;
		for (index = 0;
				index < GAGT_SPECIAL_MATCH_MAX
					&& special->match[ index ] != NULL;
				index++)
			special_lines++;

		/*
		 * If the count of lines doesn't match the length of the
		 * paragraph we have, reject this one and continue.
		 */
		if (special_lines != paragraph_lines)
			continue;

		/* Assume for the moment that this special entry matches. */
		match = special;

		/* Compare the special array line by line with the buffer. */
		line = gagt_get_first_paragraph_line (paragraph);
		for (index = 0; index < special_lines; index++)
		    {
			/*
			 * Compare text lines.  We're just a little flexible
			 * about case.  But we do match length and string,
			 * and do it separately because the entry string
			 * isn't NULL-terminated.
			 */
			assert (line != NULL);
			if (strlen (special->match[ index ])
						!= line->real_length
				|| gagt_strncasecmp (special->match[ index ],
						line->string + line->indent,
						line->real_length))
			    {
				/*
				 * No match for this line -- fail the complete
				 * paragraph.
				 */
				match = NULL;
				break;
			    }

			/* Advance to the next paragraph line. */
			line = gagt_get_next_paragraph_line (line);
		    }
	    }


	/*
	 * If we found a match, mark each paragraph line with a pointer to
	 * the matching entry.
	 */
	if (match != NULL)
	    {
		for (line = gagt_get_first_paragraph_line (paragraph);
				line != NULL;
				line = gagt_get_next_paragraph_line (line))
			line->special = match;
	    }
}


/*
 * gagt_display_special()
 *
 * Display the replacement text for the specified special table entry.  The
 * current Glk style in force is passed in; we return the Glk style in force
 * after we've done.
 */
static glui32
gagt_display_special (gagt_specialref_t special, glui32 current_style)
{
	glui32			set_style;	/* Set output style */
	int			index;		/* Line iterator */
	gagt_replaceref_t	replace;	/* Special replacement */

	/* Begin with set style initialized to the current style. */
	set_style = current_style;

	/* Iterate over each replacement line for this special entry. */
	replace = special->replace;
	for (index = 0;
			index < GAGT_SPECIAL_REPLACE_MAX
					&& replace[ index ].text != NULL;
			index++ )
	    {
		/* See if the replacement line needs a style change. */
		if (replace[ index ].style != set_style)
		    {
			glk_set_style (replace[ index ].style);
			set_style = replace[ index ].style;
		    }

		/* Output the replacement line. */
		glk_put_string ((char *) replace[ index ].text);
	    }

	/* Return the Glk style that is now set. */
	return set_style;
}


/*---------------------------------------------------------------------*/
/*  Glk port page buffer analysis functions                            */
/*---------------------------------------------------------------------*/

/*
 * Threshold for consecutive punctuation/spaces before we decide that a line
 * is in fact part of a table, and a small selection of characters to apply
 * a somewhat larger threshold to when looking for punctuation (typically,
 * characters that appear together multiple times in non-table text).
 */
static const int	GAGT_THRESHOLD			= 4;
static const char	*GAGT_COMMON_PUNCTUATION	= ".!?";
static const int	GAGT_COMMON_THRESHOLD		= 8;


/*
 * gagt_line_is_standout()
 *
 * Return TRUE if a page buffer line appears to contain "standout" text.
 * This is one of:
 *    - a line where all characters have some form of AGT text attribute
 *      set (blinking, fixed width font, or emphasis),
 *    - a line where each alphabetical character is uppercase.
 * Typically, this describes room and other miscellaneous header lines.
 */
static int
gagt_line_is_standout (gagt_lineref_t line)
{
	int		index;			/* Character iterator */
	int		all_formatted;		/* All chars formatted flag */
	int		upper_count;		/* Count uppercase chars */
	int		lower_count;		/* Count lowercase chars */

	/*
	 * Look first for a line where all characters in it have AGT font
	 * attributes.  Iterate over only the significant characters in the
	 * string.
	 */
	all_formatted = TRUE;
	for (index = line->indent;
			index < line->length - line->outdent; index++)
	    {
		gagt_attrset_t	attribute_set;	/* Unpacked font attributes. */

		/* Unpack the AGT font attributes for this character. */
		gagt_unpack_attributes (line->attributes[ index ],
							&attribute_set);

		/*
		 * If no AGT attribute is set for this character, then not
		 * all of the line is standout text.  In this case, reset
		 * the all_formatted flag, and break from the loop.
		 */
		if (!attribute_set.blink
				&& !attribute_set.fixed
				&& !attribute_set.emphasis)
		    {
			all_formatted = FALSE;
			break;
		    }
	    }

	/*
	 * If every character on the line showed some form of standout,
	 * return TRUE now.
	 */
	if (all_formatted)
		return TRUE;

	/*
	 * Repeat the loop, this time counting the upper and lowercase
	 * characters found.
	 */
	upper_count = lower_count = 0;
	for (index = line->indent;
			index < line->length - line->outdent; index++)
	    {
		/* Increment the applicable character counter. */
		if (islower (line->string[ index ]))
			lower_count++;
		else
			if (isupper (line->string[ index ]))
				upper_count++;
	    }

	/*
	 * Return true if the line contained alphabetical characters, and
	 * all were uppercase.
	 */
	if (upper_count > 0
			&& lower_count == 0)
		return TRUE;

	/* No line standout detected. */
	return FALSE;
}


/*
 * gagt_set_font_hint_proportional()
 * gagt_set_font_hint_proportional_newline()
 * gagt_set_font_hint_fixed_width()
 *
 * Helpers for assigning font hints.  Font hints have strengths, and these
 * functions ensure that gagt_assign_font_hints() only increases strengths,
 * and doesn't need to worry about checking before setting.  In the case of
 * newline, the function also adds standout to the font hint if appropriate.
 */
static void
gagt_set_font_hint_proportional (gagt_lineref_t line)
{
	/* The only weaker hint than proportional is none. */
	if (line->font_hint == HINT_NONE)
		line->font_hint = HINT_PROPORTIONAL;
}

static void
gagt_set_font_hint_proportional_newline (gagt_lineref_t line)
{
	/*
	 * Proportional and none are weaker than newline.  Because of the
	 * way we set font hints, this function can't be called with a
	 * current line hint of proportional newline.
	 */
	if (line->font_hint == HINT_NONE
			|| line->font_hint == HINT_PROPORTIONAL)
	    {
		if (gagt_line_is_standout (line))
			line->font_hint = HINT_PROPORTIONAL_NEWLINE_STANDOUT;
		else
			line->font_hint = HINT_PROPORTIONAL_NEWLINE;
	    }
}

static void
gagt_set_font_hint_fixed_width (gagt_lineref_t line)
{
	/* Fixed width font is the strongest hint. */
	if (line->font_hint == HINT_NONE
		    || line->font_hint == HINT_PROPORTIONAL
		    || line->font_hint == HINT_PROPORTIONAL_NEWLINE
		    || line->font_hint == HINT_PROPORTIONAL_NEWLINE_STANDOUT)
		line->font_hint = HINT_FIXED_WIDTH;
}


/*
 * gagt_assign_font_hints()
 *
 * For a given paragraph in the page buffer, and knowing how many paragraphs
 * are in the page buffer, this function looks at the text style used, and
 * assigns a font hint value to each entry.  Font hints indicate whether the
 * line probably requires fixed width font, or may be okay in variable width,
 * and for lines that look like they might be okay in variable width, whether
 * the newline should probably be rendered at the end of the line, or if it
 * might be omitted.
 */
static void
gagt_assign_font_hints (int paragraph, int paragraph_count)
{
	static int	initialized	= FALSE;/* First call flag. */
	static int	threshold[UCHAR_MAX + 1];
						/* Punctuation run thresholds */

	gagt_lineref_t	first_line;		/* First paragraph line */
	gagt_lineref_t	line;			/* Page buffer iterator */
	int		is_table;		/* Table indicator */
	int		in_list;		/* Inside list flag */
	assert (paragraph >= 0);

	/* On first call, set up the table on punctuation run thresholds. */
	if (!initialized)
	    {
		int	index;			/* Character code iterator */

		/* Set up a threshold value for each possible character. */
		for (index = 0; index <= UCHAR_MAX; index++)
		    {
			/*
			 * Set the threshold, either a normal value, or
			 * a larger one for characters that tend to have
			 * consecutive runs in non-table text.
			 */
			if (strchr (GAGT_COMMON_PUNCTUATION, index) == NULL)
				threshold[ index ] = GAGT_THRESHOLD;
			else
				threshold[ index ] = GAGT_COMMON_THRESHOLD;
		    }

		/* Set initialized flag. */
		initialized = TRUE;
	    }

	/*
	 * Note the first paragraph line.  This value is commonly used, and
	 * under certain circumstances, it's also modified later on.
	 */
	first_line = gagt_get_first_paragraph_line (paragraph);
	assert (first_line != NULL);

	/*
	 * Phase 1 -- look for pages that consist of just one paragraph,
	 *            itself consisting of only one line.
	 *
	 * There is no point in attempting alignment of text in a one
	 * paragraph, one line page.  This would be, for example, an error
	 * message from the interpreter parser.  In this case, set the line
	 * for proportional with newline, and return immediately.
	 */
	if (paragraph_count == 1
		&& gagt_get_next_paragraph_line (first_line) == NULL)
	    {
		/*
		 * Set the first paragraph line for proportional with a
		 * newline, and return.
		 */
		gagt_set_font_hint_proportional_newline (first_line);
		return;
	    }

	/*
	 * Phase 2 -- try to identify paragraphs that are tables, based on
	 *            looking for runs of punctuation.
	 *
	 * Search for any string that has a run of apparent line drawing or
	 * other formatting characters in it.  If we find one, we'll consider
	 * the paragraph to be a "table", that is, it has some quality that
	 * we might destroy if we used a proportional font.
	 */
	is_table = FALSE;
	for (line = first_line;
			line != NULL && !is_table;
			line = gagt_get_next_paragraph_line (line))
	    {
		int	index;			/* Line iterator */
		int	counts[UCHAR_MAX + 1];	/* Consecutive punctuation */
		int	counts_empty;		/* Optimization flag */

		/*
		 * Clear the initial counts.  Using memset() here is an
		 * order of magnitude or two faster than a for-loop.  Also
		 * there's a flag to note when counts needs to be recleared,
		 * or is already clear.
		 */
		memset (counts, 0, sizeof (counts));
		counts_empty = TRUE;

		/*
		 * Count consecutive punctuation in the line, excluding the
		 * indentation and outdent.
		 */
		for (index = line->indent;
			index < line->length - line->outdent && !is_table;
			index++)
		    {
			int	character;	/* Character at index. */

			/* Get line character of interest. */
			character = line->string[ index ];

			/* Test for punctuation. */
			if (ispunct (character))
			    {
				/*
				 * Increment the count for this character, and
				 * note that counts are no longer empty.
				 */
				counts[ character ]++;
				counts_empty = FALSE;

				/* Check the count against the threshold. */
				if (counts[ character ]
						>= threshold[ character ])
					is_table = TRUE;
			    }
			else
			    {
				/*
				 * Re-clear all counts, again with memset()
				 * for speed, but only if they need clearing.
				 * As they often won't, this optimization
				 * saves quite a bit of work.
				 */
				if (!counts_empty)
				    {
					memset (counts, 0, sizeof (counts));
					counts_empty = TRUE;
				    }
			    }
		    }
	    }

	/*
	 * Phase 3 -- try again to identify paragraphs that are tables, based
	 *            this time on looking for runs of whitespace.
	 *
	 * If no evidence found so far, look again, this time searching for
	 * any run of four or more spaces on the line (excluding any lead-in
	 * or trailing spaces).
	 */
	if (!is_table)
	    {
		for (line = first_line;
				line != NULL && !is_table;
				line = gagt_get_next_paragraph_line (line))
		    {
			int	index;		/* Line iterator */
			int	count;		/* Consecutive figures */

			/*
			 * Count consecutive spaces in the line, excluding
			 * the indentation and outdent.
			 */
			count = 0;
			for (index = line->indent;
				index < line->length - line->outdent
								&& !is_table;
				index++)
			    {
				if (isspace (line->string[ index ]))
				    {
					count++;
					if (count >= GAGT_THRESHOLD)
						is_table = TRUE;
				    }
				else
					count = 0;
			    }
		    }
	    }

	/*
	 * If the paragraph appears to be a table, and if it consists of
	 * more than just a single line, mark all lines as fixed font output
	 * and return.
	 */
	if (is_table
		&& gagt_get_next_paragraph_line (first_line) != NULL)
	    {
		for (line = first_line;
				line != NULL;
				line = gagt_get_next_paragraph_line (line))
			gagt_set_font_hint_fixed_width (line);

		/* Nothing more to do. */
		return;
	    }

	/*
	 * Phase 4 -- consider separating the first line from the rest of
	 *            the paragraph.
	 *
	 * Not a table, so the choice is between proportional rendering with
	 * a newline, and proportional rendering without...
	 *
	 * If the first paragraph line is standout or short, render it pro-
	 * portionally with a newline, and don't consider it as a further
	 * part of the paragraph.
	 */
	if (gagt_line_is_standout (first_line)
			|| first_line->real_length < screen_width / 2)
	    {
		/* Set the first paragraph line for a newline. */
		gagt_set_font_hint_proportional_newline (first_line);

		/*
		 * Disassociate this line from the rest of the paragraph by
		 * moving on the value of the first_line variable.  If it
		 * turns out that there is no next paragraph line, then we
		 * have a one-line paragraph, and there's no more to do.
		 */
		first_line = gagt_get_next_paragraph_line (first_line);
		if (first_line == NULL)
			return;
	    }

	/*
	 * Phase 5 -- try to identify lists by a simple initial look at line
	 *            indentations.
	 *
	 * Look through the paragraph for apparent lists, and decide for each
	 * line whether it's appropriate to output a newline, and render
	 * proportionally, or just render proportionally.
	 *
	 * After this loop, each line will have some form of font hint assigned
	 * to it.
	 */
	in_list = FALSE;
	for (line = first_line;
			line != NULL;
			line = gagt_get_next_paragraph_line (line))
	    {
		gagt_lineref_t		next_line;	/* Next line pointer */

		/* Access the next line, or NULL if none. */
		next_line = gagt_get_next_paragraph_line (line);

		/*
		 * Special last-iteration processing.  The newline is always
		 * output at the end of a paragraph, so if there isn't a next
		 * line, then this line is the last paragraph line.  Set its
		 * font hint appropriately, and do no more for the line.
		 */
		if (next_line == NULL)
		    {
			gagt_set_font_hint_proportional_newline (line);
			continue;
		    }

		/*
		 * If the next line's indentation is deeper that that of the
		 * first line, this paragraph looks like it is trying to be
		 * some form of a list.  In this case, make newline signifi-
		 * cant for the current line, and set the in_list flag so we
		 * can delay the return to proportional by one line.  On
		 * return to first line indentation, make newline significant
		 * for the return line.
		 */
		if (next_line->indent > first_line->indent)
		    {
			gagt_set_font_hint_proportional_newline (line);
			in_list = TRUE;
		    }
		else
		    {
			if (in_list)
				gagt_set_font_hint_proportional_newline (line);
			else
				gagt_set_font_hint_proportional (line);
			in_list = FALSE;
		    }
	    }

	/*
	 * Phase 6 -- look again for lines that look like they are supposed
	 *            to stand out from their neighbors.
	 *
	 * Now rescan the paragraph, looking this time for lines that stand
	 * out from their neighbours.  Make newline significant for each such
	 * line, and the line above, if there is one.
	 *
	 * Here we split the loop on lines so that we avoid looking at the
	 * prior line of the current first line -- because of "adjustments",
	 * it may not be the real paragraph first line.
	 *
	 * So, deal with the current first line...
	 */
	if (gagt_line_is_standout (first_line))
	    {
		/* Make newline significant for this line. */
		gagt_set_font_hint_proportional_newline (first_line);
	    }

	/* ... then deal with the rest of the lines. */
	for (line = gagt_get_next_paragraph_line (first_line);
			line != NULL;
			line = gagt_get_next_paragraph_line (line))
	    {
		/* Check for a standout line. */
		if (gagt_line_is_standout (line))
		    {
			gagt_lineref_t	prior_line;	/* Previous line */

			/* Make newline significant for this line. */
			gagt_set_font_hint_proportional_newline (line);

			/*
			 * Make newline significant for the line above.  There
			 * will always be one because we start the loop past
			 * the first line.
			 */
			prior_line = gagt_get_prior_paragraph_line (line);
			gagt_set_font_hint_proportional_newline (prior_line);
		    }
	    }

	/*
	 * Phase 7 -- special case short lines at the paragraph start.
	 *
	 * Make a special case of lines that begin a paragraph, and are short
	 * and followed by a much longer line.  This should catch games which
	 * output room titles above descriptions without using AGT fonts/bold
	 * /whatever.  Without this trap, room titles and their descriptions
	 * are run together.  This is more programmatic guesswork than heur-
	 * istics.
	 */
	if (gagt_get_next_paragraph_line (first_line) != NULL)
	    {
		gagt_lineref_t		next_line;	/* Next line pointer */

		/* Access the second paragraph line. */
		next_line = gagt_get_next_paragraph_line (first_line);

		/*
		 * See if the first line is less than half width, and the
		 * second line is more than three quarters width.  If it is,
		 * set newline as significant for the first paragraph line.
		 */
		if (first_line->real_length < screen_width / 2
				&& next_line->real_length
						> screen_width * 3 / 4)
			gagt_set_font_hint_proportional_newline (first_line);
	    }

	/*
	 * Phase 8 -- special case paragraphs of only short lines.
	 *
	 * Make a special case out of paragraphs where all lines are short.
	 * This catches elements like indented addresses.
	 */
	if (gagt_get_next_paragraph_line (first_line) != NULL)
	    {
		int	all_short;		/* All lines short flag */

		/* Iterate each line in the paragraph. */
		all_short = TRUE;
		for (line = first_line;
				line != NULL;
				line = gagt_get_next_paragraph_line (line))
		    {
			/* Clear flag if this line isn't 'short'. */
			if (line->real_length >= screen_width / 2)
			    {
				all_short = FALSE;
				break;
			    }
		    }

		/*
		 * If all lines were short, mark the complete paragraph as
		 * having significant newlines.
		 */
		if (all_short)
		    { 
			for (line = first_line;
				line != NULL;
				line = gagt_get_next_paragraph_line (line))
			    {
				gagt_set_font_hint_proportional_newline (line);
			    }
		    }
	    }
}


/*
 * gagt_find_paragraph_start()
 *
 * Find and return the next non-blank line in the page buffer, given a start
 * point.  Returns NULL if there are no more blank lines.
 */
static gagt_lineref_t
gagt_find_paragraph_start (gagt_lineref_t begin)
{
	gagt_lineref_t		line, match;	/* Page buffer iterators */

	/*
	 * Advance line to the beginning of the next paragraph, ending the
	 * loop if we reach the end of the page buffer.
	 */
	match = NULL;
	for (line = begin;
		line != NULL; line = gagt_get_next_page_line (line))
	    {
		/*
		 * We've found the start of paragraph when we encounter a
		 * non-blank line.
		 */
		if (!line->is_blank_line)
		    {
			match = line;
			break;
		    }
	    }

	/* Return the page start line, or NULL if none found. */
	return match;
}


/*
 * gagt_find_block_end()
 *
 * Find and return the apparent end of a paragraph from the page buffer,
 * given a start point, and an indentation reference.  The end is either
 * the point where indentation returns to the reference indentation, or
 * the next blank line.
 *
 * Indentation reference can be -1, indicating that only the next blank
 * line will end the paragraph.
 */
static gagt_lineref_t
gagt_find_block_end (gagt_lineref_t begin, int indent)
{
	gagt_lineref_t		line, match;	/* Page buffer iterators */

	/*
	 * Initialize the match to be the start of the block, then advance
	 * line until we hit a blank line or the end of the page buffer.
	 * At this point, match contains the last line checked.
	 */
	match = begin;
	for (line = begin;
		line != NULL; line = gagt_get_next_page_line (line))
	    {
		/*
		 * Found if we reach a blank line, or when given an inden-
		 * tation to check for, we find it.
		 */
		if (line->is_blank_line
				|| (indent > 0 && line->indent == indent))
			break;

		/* Set match to be this line. */
		match = line;
	    }

	/* Return the line considered to the the last one in this block. */
	return match;
}


/*
 * gagt_find_paragraph_end()
 *
 * Find and return the apparent end of a paragraph from the page buffer,
 * given a start point.  The function attempts to recognize paragraphs by
 * the "shape" of indentation.
 */
static gagt_lineref_t
gagt_find_paragraph_end (gagt_lineref_t first_line)
{
	gagt_lineref_t		second_line;	/* Second line pointer */

	/*
	 * If the start line is the last line in the buffer, or if the
	 * next line is a blank line, return the start line as also being
	 * the end of the paragraph.
	 */
	second_line = gagt_get_next_page_line (first_line);
	if (second_line == NULL
			|| second_line->is_blank_line)
		return first_line;

	/*
	 * Time to look at line indentations...
	 *
	 * If either line is grossly indented, forget about trying to
	 * infer anything from this, and just break the paragraph on the
	 * next blank line.
	 */
	if (first_line->indent > screen_width / 4
				|| second_line->indent > screen_width / 4)
		return gagt_find_block_end (second_line, -1);

	/*
	 * If the first line is indented more than the second, end the
	 * paragraph on a blank line, or on a return in indentation to the
	 * level of the first line.  Here we're looking for paragraphs
	 * with the shape
	 *
	 *     aksjdj jfkasjd fjkasjd ajksdj fkaj djf akjsd fkjas dfs
	 * kasjdlkfjkj fj aksd jfjkasj dlkfja skjdk flaks dlf jalksdf
	 * ksjdf kjs kdf lasjd fkjalks jdfkjasjd flkjasl djfkasjfdkl
	 */
	else if (first_line->indent > second_line->indent)
		return gagt_find_block_end (second_line, first_line->indent);

	/*
	 * If the second line is more indented than the first, this may
	 * indicate a title line, followed by normal indented paragraphing.
	 * In this case, use the second line indentation as the reference,
	 * and begin searching at the next line.  This finds
	 *
	 * ksjdkfjask ksadf
	 *     kajskd fksjkfj jfkj jfkslaj fksjlfj jkjskjlfa j fjksal
	 * sjkkdjf sj fkjkajkdlfj lsjak dfjk djkfjskl dklf alks dfll
	 * fjksja jkj dksja kjdk kaj dskfj aksjdf aksjd kfjaks fjks
	 *
	 * and
	 *
	 * asdfj kjsdf kjs
	 *     akjsdkj fkjs kdjfa lskjdl fjalsj dlfjksj kdj fjkd jlsjd
	 *     jalksj jfk slj lkfjsa lkjd lfjlaks dlfkjals djkj alsjd
	 *     kj jfksj fjksjl alkjs dlkjf lakjsd fkjas ldkj flkja fsd
	 */
	else if (second_line->indent > first_line->indent)
	    {
		gagt_lineref_t	third_line;	/* Third line pointer */

		/*
		 * See if we have a third buffer line to look at.  If we
		 * don't, or if we do but it's blank, the paragraph ends
		 * here.
		 */
		third_line = gagt_get_next_page_line (second_line);
		if (third_line == NULL
				|| third_line->is_blank_line)
			return second_line;

		/* As above, give up on gross indentation. */
		if (second_line->indent > screen_width / 4
				|| third_line->indent > screen_width / 4)
			return gagt_find_block_end (third_line, -1);

		/*
		 * If the second line indentation exceeds the third, this
		 * is probably a paragraph with a title line.  In this
		 * case, end the paragraph on a return to the indentation
		 * of the second line.  If not, just find the next blank
		 * line.
		 */
		else if (second_line->indent > third_line->indent)
			return gagt_find_block_end (third_line,
							second_line->indent);
		else
			return gagt_find_block_end (third_line, -1);
	    }

	/*
	 * Otherwise, the first and second line indentations are the same,
	 * so break only on the next empty line.  This finds the simple
	 *
	 * ksd kjal jdljf lakjsd lkj lakjsdl jfla jsldj lfaksdj fksj
	 * lskjd fja kjsdlk fjlakjs ldkjfksj lkjdf kjalskjd fkjklal
	 * skjd fkaj djfkjs dkfjal sjdlkfj alksjdf lkajs ldkjf alljjf
	 */
	else
	    {
		assert (second_line->indent == first_line->indent);
		return gagt_find_block_end (second_line, -1);
	    }
}


/*
 * gagt_annotate_page()
 *
 * This function breaks the page buffer into what appear to be paragraphs,
 * based on observations of indentation and blank separator lines.
 *
 * Paragraphs are numbered sequentially starting at zero, and checked to
 * see if they match a known special.  Each line in a paragraph is marked
 * with its paragraph number, and assigned a font hint (unless special).
 */
static void
gagt_annotate_page (void)
{
	gagt_lineref_t	start;			/* Paragraph start */
	int		sequence;		/* Paragraph sequence */
	int		paragraph;		/* Paragraph index */

	/* Start numbering paragraphs at zero. */
	sequence = 0;

	/* First split the page up into paragraphs. */
	start = gagt_find_paragraph_start (gagt_get_first_page_line ());
	while (start != NULL)
	    {
		gagt_lineref_t	end;		/* Paragraph end line */
		gagt_lineref_t	line;		/* Line iterator */
		gagt_lineref_t	next_line;	/* Start line search */

		/* From the start, identify the paragraph end. */
		end = gagt_find_paragraph_end (start);

		/*
		 * Set the paragraph number to the sequence for each line
		 * identified as part of this paragraph.
		 */
		for (line = start;
				line != end;
				line = gagt_get_next_page_line (line))
			line->paragraph = sequence;
		end->paragraph = sequence;

		/*
		 * If there's another line, look for the next paragraph
		 * there, otherwise we're done.
		 */
		next_line = gagt_get_next_page_line (end);
		if (next_line != NULL)
			start = gagt_find_paragraph_start (next_line);
		else
			start = NULL;

		/* Increment the paragraph sequence number. */
		sequence++;
	    }

	/*
	 * Now set replacement specials and font hints for each paragraph
	 * found in the page.
	 */
	for (paragraph = 0; paragraph < sequence; paragraph++)
	    {
		/*
		 * See if this paragraph is one of the known special ones.
		 * If it is, this will mark the special field for each line
		 * in the paragraph.
		 */
		if (gagt_replacement_enabled)
			gagt_mark_special (paragraph);

		/*
		 * Assign font hints to each line in the paragraph.  This
		 * function gets to know how many paragraphs there are in
		 * the page, as this may help it to find suitable hints.
		 */
		gagt_assign_font_hints (paragraph, sequence);
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk port output functions                                          */
/*---------------------------------------------------------------------*/

/*
 * gagt_display_text_element()
 *
 * Display an element of a buffer string using matching packed attributes.
 * The currently set Glk style is supplied, and the function returns the
 * new currently set Glk style.
 *
 * The function handles a flag to coerce fixed width font.
 */
static glui32
gagt_display_text_element (char *string, gagt_attr_t *attributes, int length,
			glui32 current_style, int fixed_width)
{
	int		marker;			/* Delayed output marker. */
	int		index;			/* Character iterator. */
	glui32		set_style;		/* Set output style. */

	/* Begin with set style initialized to the current style. */
	set_style = current_style;

	/*
	 * Iterate each character in the line range.  We actually delay output
	 * until we see a change in style; that way, we can send a buffer
	 * of characters to Glk, rather than sending them just one at a time.
	 */
	marker = 0;
	for (index = 0; index < length; index++)
	    {
		gagt_attrset_t	attribute_set;	/* Unpacked font attributes. */
		glui32		style;		/* Output style. */
		assert (attributes != NULL && string != NULL);

		/*
		 * Unpack the AGT font attributes for this character, and
		 * add fixed width font coercion.
		 */
		gagt_unpack_attributes (attributes[ index ], &attribute_set);
		attribute_set.fixed |= fixed_width;

		/*
		 * Decide on any applicable new Glk text styling.  If it's
		 * different to the current style, output the delayed char-
		 * acters, and update Glk's style setting.
		 */
		style = gagt_select_style (&attribute_set);
		if (style != set_style)
		    {
			glk_put_buffer (string + marker, index - marker);
			marker = index;

			glk_set_style (style);
			set_style = style;
		    }
	    }

	/* Output any remaining delayed characters. */
	if (marker < length)
		glk_put_buffer (string + marker, length - marker);

	/* Return the Glk style now set. */
	return set_style;
}


/*
 * gagt_display_line()
 *
 * Display a page buffer line, starting in the current Glk style, and
 * returning the new current Glk style.
 *
 * The function takes additional flags to force fixed width font, skip over
 * indentation and trailing line whitespace, and trim hyphens (if skipping
 * trailing whitespace).
 */
static glui32
gagt_display_line (gagt_lineref_t line, glui32 current_style,
			int fixed_width,
			int skip_indent, int skip_outdent, int trim_hyphen)
{
	int		start;			/* First display character. */
	int		length;			/* Length of data to display. */
	glui32		set_style;		/* Set output style. */

	/*
	 * Check the skip indent flag to find the first character to display,
	 * and the count of characters to display.
	 */
	start  = 0;
	length = line->length;
	if (skip_indent)
	    {
		start  += line->indent;
		length -= line->indent;
	    }

	/* Adjust length for skipping outdent and trimming hyphens. */
	if (skip_outdent)
	    {
		length -= line->outdent;
		if (trim_hyphen
				&& line->is_hyphenated)
			length--;
	    }

	/* Display this line segment. */
	set_style = gagt_display_text_element (line->string + start,
					line->attributes + start, length,
					current_style, fixed_width);

	/* Return the new Glk style. */
	return set_style;
}


/*
 * gagt_display_hinted_line()
 *
 * Display a page buffer line, starting in the current Glk style, and
 * returning the new current Glk style.  The function uses the font hints
 * from the line, and receives the font hint of the prior line.
 */
static glui32
gagt_display_hinted_line (gagt_lineref_t line, glui32 current_style,
				gagt_font_hint_t prior_hint)
{
	glui32		style;			/* Output style. */

	/* Begin with style initialized to the current style. */
	style = current_style;

	/* Decide on display format using the line's font hint. */
	switch (line->font_hint)
	    {
	    case HINT_FIXED_WIDTH:
		/* Force fixed width font on the line. */
		style = gagt_display_line (line, style,
						TRUE,
						FALSE, FALSE, FALSE);

		/* Always output a newline. */
		glk_put_char ('\n');
		break;

	    case HINT_PROPORTIONAL:
		/*
		 * Permit proportional font, and suppress outdent.  Suppress
		 * indent too if this line follows a line that suppressed
		 * newline, or is the first line in the paragraph.  For all
		 * cases, trim the hyphen from hyphenated lines.
		 */
		if (prior_hint == HINT_PROPORTIONAL
					|| prior_hint == HINT_NONE)
			style = gagt_display_line (line, style,
							FALSE,
							TRUE, TRUE, TRUE);
		else
			style = gagt_display_line (line, style,
							FALSE,
							FALSE, TRUE, TRUE);

		/*
		 * Where the line is not hyphenated, output a space in place
		 * of newline.  This lets paragraph text to flow to the full
		 * display width.
		 */
		if (!line->is_hyphenated)
			glk_put_char (' ');
		break;

	    case HINT_PROPORTIONAL_NEWLINE:
	    case HINT_PROPORTIONAL_NEWLINE_STANDOUT:
		/*
		 * As above, permit proportional font, suppress outdent, and
		 * suppress indent too under certain conditions; in this case,
		 * only when the prior line suppressed newline.
		 */
		if (prior_hint == HINT_PROPORTIONAL)
			style = gagt_display_line (line, style,
							FALSE,
							TRUE, TRUE, FALSE);
		else
			style = gagt_display_line (line, style,
							FALSE,
							FALSE, TRUE, FALSE);

		/* Always output a newline. */
		glk_put_char ('\n');
		break;

	    case HINT_NONE:
		/*
		 * This should never happen.  All paragraph lines should be
		 * assigned font hints.
		 */
		gagt_fatal ("GLK: Page buffer line with no font hint");
		gagt_exit ();

	    default:
		gagt_fatal ("GLK: Invalid font hint encountered");
		gagt_exit ();
	    }

	/* Return the Glk style now set. */
	return style;
}


/*
 * gagt_display_auto()
 *
 * Display buffered output text to the Glk main window using a bunch of
 * occasionally rather dodgy heuristics to try to automatically set a
 * suitable font details for the way the text is structured, and replacing
 * special paragraphs with altered text.
 */
static void
gagt_display_auto (void)
{
	gagt_lineref_t		line;		/* Page buffer iterator. */
	int			paragraph;	/* Current paragraph number. */
	glui32			style;		/* Output style. */
	gagt_font_hint_t	prior_hint;	/* Notes prior font hint. */
	assert (glk_stream_get_current () != NULL);

	/* Set the initial main window display style. */
	style = style_Normal;
	glk_set_style (style);

	/*
	 * Initialize prior hint to be none; this value indicates that we're
	 * at the first line of a paragraph, since it's set at the loop end
	 * to be the font hints for the line just handled.  And initialize
	 * the paragraph number to none.
	 */
	prior_hint = HINT_NONE;
	paragraph  = GAGT_NO_PARAGRAPH;

	/* Handle each line in the page buffer. */
	for (line = gagt_get_first_page_line ();
			line != NULL;
			line = gagt_get_next_page_line (line))
	    {
		/*
		 * Ignore all blank lines seen in the buffer.  We do our own
		 * inter-paragraph blank line control.
		 */
		if (line->is_blank_line)
		    {
			assert (line->paragraph == GAGT_NO_PARAGRAPH);
			continue;
		    }

		/* Look for a change in the current paragraph. */
		if (line->paragraph != paragraph)
		    {
			/*
			 * If this is the first line in the paragraph, output
			 * a blank line only where it's standout -- this sets
			 * it apart from the prompt; if it's not the first
			 * line, output a blank line to separate paragraphs.
			 */
			if (paragraph == GAGT_NO_PARAGRAPH)
			    {
				if (line->font_hint
					  == HINT_PROPORTIONAL_NEWLINE_STANDOUT)
					glk_put_char ('\n');
			    }
			else
				glk_put_char ('\n');

			/*
			 * Reset prior hint on change of paragraph, and note
			 * the new paragraph.
			 */
			prior_hint = HINT_NONE;
			paragraph  = line->paragraph;

			/*
			 * If this is a special paragraph, output replacement
			 * text instead.
			 */
			if (line->special != NULL)
			    {
				style = gagt_display_special
							(line->special, style);
				continue;
			    }
		    }

		/*
		 * Ignore special paragraphs.  We've done all we need to for
		 * these on paragraph change.
		 */
		if (line->special == NULL)
		    {
			/*
			 * Put it all together by printing this page buffer
			 * line according to its font hint.  This function
			 * also handles newline or space terminators.
			 */
			style = gagt_display_hinted_line (line,
							style, prior_hint);

			/* Note font hint for the next loop iteration. */
			prior_hint = line->font_hint;
		    }
	    }

	/*
	 * Output the newline for the end of the last paragraph.  If the page
	 * buffer was empty, however, do nothing.
	 */
	if (gagt_get_first_page_line () != NULL)
		glk_put_char ('\n');

	/* Output any unterminated line from the line buffer. */
	style = gagt_display_text_element (gagt_line_buffer,
						gagt_line_attributes,
						gagt_line_length, style,
						FALSE);
}


/*
 * gagt_display_manual()
 *
 * Display buffered output text in the Glk main window, with either a fixed
 * width or a proportional font.
 */
static void
gagt_display_manual (int fixed_width)
{
	gagt_lineref_t		line;		/* Page buffer iterator. */
	int			paragraph;	/* Current paragraph number. */
	glui32			style;		/* Output style. */
	assert (glk_stream_get_current () != NULL);

	/* Set the initial main window display style. */
	style = style_Normal;
	glk_set_style (style);

	/* Initialize the paragraph number to none. */
	paragraph = GAGT_NO_PARAGRAPH;

	/* Handle each line in the page buffer. */
	for (line = gagt_get_first_page_line ();
			line != NULL;
			line = gagt_get_next_page_line (line))
	    {
		/* Look for a change in the current paragraph. */
		if (line->paragraph != paragraph)
		    {
			/* Note the new current paragraph. */
			paragraph = line->paragraph;

			/*
			 * If this is a special paragraph, output replacement
			 * text instead.
			 */
			if (line->special != NULL)
			    {
				style = gagt_display_special
							(line->special, style);
				continue;
			    }
		    }

		/*
		 * Ignore special paragraphs.  We've done all we need to for
		 * these on paragraph change.
		 */
		if (line->special == NULL)
		    {
			/* Display the page buffer line, note new style. */
			style = gagt_display_line (line, style,
							fixed_width,
							FALSE, FALSE, FALSE);

			/* Terminate with a new line. */
			glk_put_char ('\n');
		    }
	    }

	/* Output any unterminated line from the line buffer. */
	style = gagt_display_text_element (gagt_line_buffer,
						gagt_line_attributes,
						gagt_line_length, style,
						fixed_width);
}


/*
 * gagt_display_debug()
 *
 * Display the analyzed page buffer in a form that shows all of its gory
 * detail.
 */
static void
gagt_display_debug (void)
{
	gagt_lineref_t		line;		/* Page buffer iterator. */
	char			buffer[256];	/* String format buffer. */
	assert (glk_stream_get_current () != NULL);

	/* Print out each page buffer entry in debug format. */
	glk_set_style (style_Preformatted);
	for (line = gagt_get_first_page_line ();
			line != NULL;
			line = gagt_get_next_page_line (line))
	    {
		sprintf (buffer,
			"%02d,%02d S=%02d L=%02d I=%02d O=%02d [%02d] %c%c| ",
			line->paragraph,
			line->special != NULL
				? line->special - GAGT_SPECIALS : 0,
			line->size, line->length,
			line->indent, line->outdent,
			line->real_length,
			line->is_hyphenated ? 'h' : '_',
			line->is_blank_line ? 'b' :
			line->font_hint == HINT_PROPORTIONAL		? 'P' :
			line->font_hint == HINT_PROPORTIONAL_NEWLINE	? 'N' :
			line->font_hint == HINT_PROPORTIONAL_NEWLINE_STANDOUT
									? 'S' :
			line->font_hint == HINT_FIXED_WIDTH		? 'F' :
			'_');
		glk_put_string (buffer);

		glk_put_buffer (line->string, line->length);
		glk_put_char ('\n');
	    }

	/* Finish up with any unterminated line from the line buffer. */
	if (gagt_line_length > 0)
	    {
		sprintf (buffer, "__,__ S=%02d L=%02d I=__ O=__ [__] __| ",
				gagt_line_size, gagt_line_length);
		glk_put_string (buffer);

		glk_put_buffer (gagt_line_buffer, gagt_line_length);
	    }
}


/*
 * gagt_output_flush()
 *
 * Flush any buffered output text to the Glk main window, and clear the
 * buffer ready for new output text.  The function concerns itself with
 * both the page buffer, and any unterminated line in the line buffer.
 */
static void
gagt_output_flush (void)
{
	assert (glk_stream_get_current () != NULL);

	/*
	 * Run the analysis of page buffer contents.  This will fill in the
	 * paragraph and font hints fields, any any applicable special
	 * pointer, for every line held in the buffer.
	 */
	gagt_annotate_page ();

	/*
	 * Select the appropriate display routine to use, and call it.  The
	 * display routines present somewhat different output, and are resp-
	 * onsible for displaying both the page buffer _and_ any buffered
	 * current line text.
	 */
	switch (gagt_font_mode)
	    {
	    case FONT_AUTOMATIC:	gagt_display_auto ();		break;
	    case FONT_FIXED_WIDTH:	gagt_display_manual (TRUE);	break;
	    case FONT_PROPORTIONAL:	gagt_display_manual (FALSE);	break;
	    case FONT_DEBUG:		gagt_display_debug ();		break;
	    default:
		gagt_fatal ("GLK: Invalid font mode encountered");
		gagt_exit ();
	    }

	/* Empty the buffer, ready for new game strings. */
	gagt_output_delete ();
}


/*
 * agt_clrscr()
 *
 * Clear the main playing area window.  Although there may be little point
 * in flushing (rather than emptying) the buffers, nevertheless that is
 * what we do.
 */
void
agt_clrscr (void)
{
	/* Do nothing if in batch mode. */
	if (BATCH_MODE)
		return;

	/* Update the apparent (virtual) window x position. */
	curr_x = 0;

	/* Flush any pending buffered output. */
	gagt_output_flush ();

	/* Clear the main Glk window. */
	glk_window_clear (gagt_main_window);

	/* Add the string to any script file. */
	if (script_on)
		textputs (scriptfile, "\n\n\n\n");

	gagt_debug ("agt_clrscr", "");
}


/*
 * gagt_standout_string()
 * gagt_standout_char()
 *
 * Print a string in a style that stands out from game text.
 */
static void
gagt_standout_string (const char *message)
{
	assert (message != NULL);

	/*
	 * Print the message, in a style that hints that it's from the
	 * interpreter, not the game.
	 */
	glk_set_style (style_Emphasized);
	glk_put_string ((char *) message);
	glk_set_style (style_Normal);
}

static void
gagt_standout_char (char c)
{
	/* Print the character, in a message style. */
	glk_set_style (style_Emphasized);
	glk_put_char (c);
	glk_set_style (style_Normal);
}


/*
 * gagt_normal_string()
 * gagt_normal_char()
 *
 * Print a string in normal text style.
 */
static void
gagt_normal_string (const char *message)
{
	assert (message != NULL);

	/* Print the message, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_string ((char *) message);
}

static void
gagt_normal_char (char c)
{
	/* Print the character, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_char (c);
}


/*
 * gagt_header_string()
 *
 * Print a string as a header.
 */
static void
gagt_header_string (const char *banner)
{
        assert (banner != NULL);

        /* Print the string in a header style. */
        glk_set_style (style_Header);
        glk_put_string ((char *) banner);
        glk_set_style (style_Normal);
}


/*---------------------------------------------------------------------*/
/*  Glk port delay functions                                           */
/*---------------------------------------------------------------------*/

/* Number of milliseconds in a second (traditionally, 1000). */
static const int	GAGT_MS_PER_SEC			= 1000;

/*
 * Number of milliseconds to timeout.  Because of jitter in the way Glk
 * generates timeouts, it's worthwhile implementing a delay using a number
 * of shorter timeouts.  This minimizes inaccuracies in the actual delay.
 */
static const glui32	GAGT_DELAY_TIMEOUT		= 50;

/* The character key that can be pressed to cancel, and suspend, delays. */
static const char	GAGT_DELAY_SUSPEND		= ' ';

/*
 * Flag to temporarily turn off all delays.  This is set when the user
 * cancels a delay with a keypress, and remains set until the next time
 * that AGiliTy requests user input.  This way, games that call agt_delay()
 * sequentially don't require multiple keypresses to jump out of delay
 * sections.
 */
static int		gagt_delays_suspended		= FALSE;


/*
 * agt_delay()
 *
 * Delay for the specified number of seconds.  The delay can be canceled
 * by a user keypress.
 */
void
agt_delay (int seconds)
{
	glui32		milliseconds;		/* Milliseconds to delay. */
	glui32		delayed;		/* Incremental delay amount. */
	int		delay_completed;	/* Delay loop expiry flag. */

	/* Suppress delay if in fast replay or batch mode. */
	if (fast_replay
			|| BATCH_MODE)
		return;

	/*
	 * Do nothing if Glk doesn't have timers, if the delay state is set
	 * to ignore delays, if a zero or negative delay was specified, or
	 * if delays are currently temporarily suspended.
	 */
	if (!gagt_delays_possible
			|| gagt_delay_mode == DELAY_OFF
			|| seconds <= 0
			|| gagt_delays_suspended)
		return;

	/* Flush any pending buffered output. */
	gagt_output_flush ();

	/* Refresh status, showing the waiting indicator. */
	gagt_status_in_delay (TRUE);

	/* Calculate the number of milliseconds to delay. */
	if (gagt_delay_mode == DELAY_SHORT)
		milliseconds = (seconds * GAGT_MS_PER_SEC) / 2;
	else
		milliseconds = (seconds * GAGT_MS_PER_SEC);

	/* Request timer events, and let a keypress cancel the delay. */
	glk_request_char_event (gagt_main_window);
	glk_request_timer_events (GAGT_DELAY_TIMEOUT);

	/*
	 * Implement the delay using a sequence of shorter Glk timeouts,
	 * with an option to cancel the delay with a keypress.
	 */
	delay_completed = TRUE;
	for (delayed = 0;
			delayed < milliseconds;
			delayed += GAGT_DELAY_TIMEOUT)

	    {
		event_t		event;		/* Glk event buffer. */

		/* Wait for the next timeout, or a character. */
		gagt_event_wait_2 (evtype_CharInput,
						evtype_Timer, &event);

		/* See if event was a character. */
		if (event.type == evtype_CharInput)
		    {
			/*
			 * If suspend requested, stop the delay, and set the
			 * delay suspension flag, and a note that the delay
			 * loop didn't complete.  Otherwise, reissue the
			 * character input request.
			 */
			if (event.val1 == GAGT_DELAY_SUSPEND)
			    {
				gagt_delays_suspended = TRUE;
				delay_completed = FALSE;
				break;
			    }
			else
				glk_request_char_event (gagt_main_window);
		    }
	    }

	/* Cancel any pending character input, and timer events. */
	if (delay_completed)
		glk_cancel_char_event (gagt_main_window);
	glk_request_timer_events (0);

	/* Clear the waiting indicator. */
	gagt_status_in_delay (FALSE);

	gagt_debug ("agt_delay", "seconds=%d [%lu mS] -> %s",
				seconds, milliseconds,
				delay_completed ? "completed" : "canceled");
}


/*
 * gagt_delay_resume()
 *
 * Unsuspend delays.  This function should be called by agt_input() and
 * agt_getkey(), to re-enable delays when the interpreter next requests
 * user input.
 */
static void
gagt_delay_resume (void)
{
	gagt_delays_suspended = FALSE;
}


/*---------------------------------------------------------------------*/
/*  Glk port box drawing functions                                     */
/*---------------------------------------------------------------------*/

/* Saved details of any current box dimensions and flags. */
static int		gagt_box_busy	= FALSE;
static unsigned long	gagt_box_flags	= 0;
static int		gagt_box_width	= 0;
static int		gagt_box_height	= 0;
static int		gagt_box_startx	= 0;


/*
 * gagt_box_rule()
 * gagt_box_position()
 *
 * Draw a line at the top or bottom of a box, and position the cursor
 * with a box indent.
 */
static void
gagt_box_rule (int width)
{
	char	*ruler;

	/* Write a +--...--+ ruler to delimit a box. */
	ruler = gagt_malloc (width + 2 + 1);
	memset (ruler + 1, '-', width);
	ruler[0] = ruler[ width + 1 ] = '+';
	ruler[ width + 2 ] = '\0';
	agt_puts (ruler);
	free (ruler);
}

static void
gagt_box_position (int indent)
{
	char	*spaces;

	/* Write a newline before the indent. */
	agt_newline ();

	/* Write the indent to the start of box text. */
	spaces = gagt_malloc (indent + 1);
	memset (spaces, ' ', indent);
	spaces[ indent ] = '\0';
	agt_puts (spaces);
	free (spaces);
}


/*
 * agt_makebox()
 * agt_qnewline()
 * agt_endbox()
 *
 * Start a box of given width, height, and with given flags.  Write a new
 * line in the box.  And end the box.
 */
void
agt_makebox (int width, int height, unsigned long flags)
{
	int	centering_width;
	assert (!gagt_box_busy);

	/* Note the box flags and dimensions for later on. */
	gagt_box_busy   = TRUE;
	gagt_box_flags  = flags;
	gagt_box_width  = width;
	gagt_box_height = height;

	/* If no centering requested, set the indent to zero. */
	if (gagt_box_flags & TB_NOCENT)
		gagt_box_startx = 0;
	else
	    {
		/*
		 * Calculate the indent for centering, adding 4 characters
		 * for borders.  Here, since screen_width is artificial,
		 * we'll center off status_width if it is less than screen
		 * width, otherwise we'll center by using screen_width.  The
		 * reason for shrinking to screen_width is that if we don't,
		 * we could drive curr_x to beyond screen_width with our box
		 * indentations, and that confuses AGiliTy.
		 */
		if (status_width < screen_width)
			centering_width = status_width;
		else
			centering_width = screen_width;
		if (gagt_box_flags & TB_BORDER)
			gagt_box_startx = (centering_width
						- gagt_box_width - 4) / 2;
		else
			gagt_box_startx = (centering_width
						- gagt_box_width    ) / 2;

		/*
		 * If the box turns out to be wider than the window,
		 * abandon centering.
		 */
		if (gagt_box_startx < 0)
			gagt_box_startx = 0;
	    }

	/*
	 * When in a box, we'll coerce fixed width font by setting it in
	 * the AGT font attributes.  This ensures that the box displays as
	 * accurately as we're able to achieve.
	 */
	gagt_coerce_fixed_font (TRUE);

	/* Position the cursor for the box, and if bordered, write the rule. */
	gagt_box_position (gagt_box_startx);
	if (gagt_box_flags & TB_BORDER)
	    {
		gagt_box_rule (gagt_box_width + 2);
		gagt_box_position (gagt_box_startx);
		agt_puts ("| ");
	    }
}

void
agt_qnewline (void)
{
	assert (gagt_box_busy);

	/* Write box characters for the current and next line. */
	if (gagt_box_flags & TB_BORDER)
	    {
		agt_puts (" |");
		gagt_box_position (gagt_box_startx);
		agt_puts ("| ");
	    }
	else
		gagt_box_position (gagt_box_startx);
}

void
agt_endbox (void)
{
	assert (gagt_box_busy);

	/* Finish off the current box. */
	if (gagt_box_flags & TB_BORDER)
	    {
		agt_puts (" |");
		gagt_box_position (gagt_box_startx);
		gagt_box_rule (gagt_box_width + 2);
	    }
	agt_newline ();

	/* An extra newline here improves the appearance. */
	agt_newline ();

	/* Back to allowing proportional font output again. */
	gagt_coerce_fixed_font (FALSE);

	/* Forget the details of the completed box. */
	gagt_box_busy   = FALSE;
	gagt_box_flags  = 0;
	gagt_box_width  = 0;
	gagt_box_startx = 0;
}


/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/* Valid command control values. */
static const char	*GAGT_COMMAND_ON		= "on";
static const char	*GAGT_COMMAND_OFF		= "off";


/*
 * gagt_command_script()
 *
 * Turn game output scripting (logging) on and off.
 */
static void
gagt_command_script (const char *argument)
{
	assert (argument != NULL);

	/* Set up a transcript according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a transcript is already active. */
		if (gagt_transcript_stream != NULL)
		    {
			gagt_normal_string ("Glk transcript is already ");
			gagt_normal_string (GAGT_COMMAND_ON);
			gagt_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a transcript. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_Transcript | fileusage_TextMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gagt_standout_string ("Glk transcript failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gagt_transcript_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gagt_transcript_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gagt_standout_string ("Glk transcript failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Set the new transcript stream as the main echo stream. */
		glk_window_set_echo_stream (gagt_main_window,
						gagt_transcript_stream);

		/* Confirm action. */
		gagt_normal_string ("Glk transcript is now ");
		gagt_normal_string (GAGT_COMMAND_ON);
		gagt_normal_string (".\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* If a transcript is active, close it. */
		if (gagt_transcript_stream != NULL)
		    {
			glk_stream_close (gagt_transcript_stream, NULL);
			gagt_transcript_stream = NULL;

			/* Clear the main echo stream. */
			glk_window_set_echo_stream (gagt_main_window, NULL);

			/* Confirm action. */
			gagt_normal_string ("Glk transcript is now ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
		    }
		else
		    {
			/* Note that scripts are already disabled. */
			gagt_normal_string ("Glk transcript is already ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * transcript mode.
		 */
		gagt_normal_string ("Glk transcript is ");
		if (gagt_transcript_stream != NULL)
			gagt_normal_string (GAGT_COMMAND_ON);
		else
			gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gagt_normal_string ("Glk transcript can be ");
		gagt_standout_string (GAGT_COMMAND_ON);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
}


/*
 * gagt_command_inputlog()
 *
 * Turn game input logging on and off.
 */
static void
gagt_command_inputlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up an input log according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if an input log is already active. */
		if (gagt_inputlog_stream != NULL)
		    {
			gagt_normal_string ("Glk input logging is already ");
			gagt_normal_string (GAGT_COMMAND_ON);
			gagt_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for an input log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gagt_standout_string ("Glk input logging failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gagt_inputlog_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gagt_inputlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gagt_standout_string ("Glk input logging failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gagt_normal_string ("Glk input logging is now ");
		gagt_normal_string (GAGT_COMMAND_ON);
		gagt_normal_string (".\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* If an input log is active, close it. */
		if (gagt_inputlog_stream != NULL)
		    {
			glk_stream_close (gagt_inputlog_stream, NULL);
			gagt_inputlog_stream = NULL;

			/* Confirm action. */
			gagt_normal_string ("Glk input log is now ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current input log. */
			gagt_normal_string ("Glk input logging is already ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * input logging mode.
		 */
		gagt_normal_string ("Glk input logging is ");
		if (gagt_inputlog_stream != NULL)
			gagt_normal_string (GAGT_COMMAND_ON);
		else
			gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gagt_normal_string ("Glk input logging can be ");
		gagt_standout_string (GAGT_COMMAND_ON);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
}


/*
 * gagt_command_readlog()
 *
 * Set the game input log, to read input from a file.
 */
static void
gagt_command_readlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up a read log according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a read log is already active. */
		if (gagt_readlog_stream != NULL)
		    {
			gagt_normal_string ("Glk read log is already ");
			gagt_normal_string (GAGT_COMMAND_ON);
			gagt_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a read log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_Read, 0);
		if (fileref == NULL)
		    {
			gagt_standout_string ("Glk read log failed.\n");
			return;
		    }

		/*
		 * Reject the file reference if we're expecting to read
		 * from it, and the referenced file doesn't exist.
		 */
		if (!glk_fileref_does_file_exist (fileref))
		    {
			glk_fileref_destroy (fileref);
			gagt_standout_string ("Glk read log failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gagt_readlog_stream = glk_stream_open_file
					(fileref, filemode_Read, 0);
		if (gagt_readlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gagt_standout_string ("Glk read log failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gagt_normal_string ("Glk read log is now ");
		gagt_normal_string (GAGT_COMMAND_ON);
		gagt_normal_string (".\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* If a read log is active, close it. */
		if (gagt_readlog_stream != NULL)
		    {
			glk_stream_close (gagt_readlog_stream, NULL);
			gagt_readlog_stream = NULL;

			/* Confirm action. */
			gagt_normal_string ("Glk read log is now ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current read log. */
			gagt_normal_string ("Glk read log is already ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * read logging mode.
		 */
		gagt_normal_string ("Glk read log is ");
		if (gagt_readlog_stream != NULL)
			gagt_normal_string (GAGT_COMMAND_ON);
		else
			gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gagt_normal_string ("Glk read log can be ");
		gagt_standout_string (GAGT_COMMAND_ON);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
}


/*
 * gagt_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gagt_command_abbreviations (const char *argument)
{
	assert (argument != NULL);

	/* Set up abbreviation expansions according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		/* See if expansions are already on. */
		if (gagt_abbreviations_enabled)
		    {
			gagt_normal_string ("Glk abbreviation expansions"
					" are already ");
			gagt_normal_string (GAGT_COMMAND_ON);
			gagt_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions on. */
		gagt_abbreviations_enabled = TRUE;
		gagt_normal_string ("Glk abbreviation expansions are now ");
		gagt_normal_string (GAGT_COMMAND_ON);
		gagt_normal_string (".\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* See if expansions are already off. */
		if (!gagt_abbreviations_enabled)
		    {
			gagt_normal_string ("Glk abbreviation expansions"
					" are already ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions off. */
		gagt_abbreviations_enabled = FALSE;
		gagt_normal_string ("Glk abbreviation expansions are now ");
		gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * expansion mode.
		 */
		gagt_normal_string ("Glk abbreviation expansions are ");
		if (gagt_abbreviations_enabled)
			gagt_normal_string (GAGT_COMMAND_ON);
		else
			gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gagt_normal_string ("Glk abbreviation expansions can be ");
		gagt_standout_string (GAGT_COMMAND_ON);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
}


/*
 * gagt_command_print_version_number()
 * gagt_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gagt_command_print_version_number (glui32 version)
{
	char		buffer[64];		/* Output buffer string. */

	/* Print out the three version number component parts. */
	sprintf (buffer, "%lu.%lu.%lu",
			(version & 0xFFFF0000) >> 16,
			(version & 0x0000FF00) >>  8,
			(version & 0x000000FF)      );
	gagt_normal_string (buffer);
}
static void
gagt_command_version (const char *argument)
{
	glui32	version;		/* Glk library version number. */
	assert (argument != NULL);

	/* Get the Glk library version number. */
	version = glk_gestalt (gestalt_Version, 0);

	/* Print the Glk library and port version numbers. */
	gagt_normal_string ("The Glk library version is ");
	gagt_command_print_version_number (version);
	gagt_normal_string (".\n");
	gagt_normal_string ("This is version ");
	gagt_command_print_version_number (GAGT_PORT_VERSION);
	gagt_normal_string (" of the Glk AGiliTy port.\n");
}


/* List of valid font control values. */
static const char	*GAGT_COMMAND_FIXED		= "fixed";
static const char	*GAGT_COMMAND_VARIABLE		= "variable";
static const char	*GAGT_COMMAND_PROPORTIONAL	= "proportional";
static const char	*GAGT_COMMAND_AUTO		= "auto";
static const char	*GAGT_COMMAND_AUTOMATIC		= "automatic";
static const char	*GAGT_COMMAND_DEBUG		= "debug";

/*
 * gagt_command_fonts()
 *
 * Set the value for gagt_font_mode depending on the argument from the
 * user's command escape.
 *
 * Despite our best efforts, font control may still be wrong in some games.
 * This command gives us a chance to correct that.
 */
static void
gagt_command_fonts (const char *argument)
{
	assert (argument != NULL);

	/* Set up font control according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_FIXED))
	    {
		/* See if font control is already fixed. */
		if (gagt_font_mode == FONT_FIXED_WIDTH)
		    {
			gagt_normal_string ("Glk font control is already '");
			gagt_normal_string (GAGT_COMMAND_FIXED);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set fixed font. */
		gagt_font_mode = FONT_FIXED_WIDTH;

		/* Confirm actions. */
		gagt_normal_string ("Glk font control is now '");
		gagt_normal_string (GAGT_COMMAND_FIXED);
		gagt_normal_string ("'.\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_VARIABLE)
		|| !gagt_strcasecmp (argument, GAGT_COMMAND_PROPORTIONAL))
	    {
		/* See if font control is already variable. */
		if (gagt_font_mode == FONT_PROPORTIONAL)
		    {
			gagt_normal_string ("Glk font control is already '");
			gagt_normal_string (GAGT_COMMAND_PROPORTIONAL);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set proportional font. */
		gagt_font_mode = FONT_PROPORTIONAL;

		/* Confirm actions. */
		gagt_normal_string ("Glk font control is now '");
		gagt_normal_string (GAGT_COMMAND_PROPORTIONAL);
		gagt_normal_string ("'.\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_AUTO)
		|| !gagt_strcasecmp (argument, GAGT_COMMAND_AUTOMATIC))
	    {
		/* See if font control is already automatic. */
		if (gagt_font_mode == FONT_AUTOMATIC)
		    {
			gagt_normal_string ("Glk font control is already '");
			gagt_normal_string (GAGT_COMMAND_AUTOMATIC);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set automatic font control. */
		gagt_font_mode	= FONT_AUTOMATIC;

		/* Confirm actions. */
		gagt_normal_string ("Glk font control is now '");
		gagt_normal_string (GAGT_COMMAND_AUTOMATIC);
		gagt_normal_string ("'.\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_DEBUG))
	    {
		/* See if font control is already debug. */
		if (gagt_font_mode == FONT_DEBUG)
		    {
			gagt_normal_string ("Glk font control is already '");
			gagt_normal_string (GAGT_COMMAND_DEBUG);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set automatic font control. */
		gagt_font_mode	= FONT_DEBUG;

		/* Confirm actions. */
		gagt_normal_string ("Glk font control is now '");
		gagt_normal_string (GAGT_COMMAND_DEBUG);
		gagt_normal_string ("'.\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * font state.
		 */
		gagt_normal_string ("Glk font control is set to '");
		switch (gagt_font_mode)
		    {
		    case FONT_AUTOMATIC:
			gagt_normal_string (GAGT_COMMAND_AUTOMATIC);
			break;
		    case FONT_FIXED_WIDTH:
			gagt_normal_string (GAGT_COMMAND_FIXED);
			break;
		    case FONT_PROPORTIONAL:
			gagt_normal_string (GAGT_COMMAND_PROPORTIONAL);
			break;
		    case FONT_DEBUG:
			gagt_normal_string (GAGT_COMMAND_DEBUG);
			break;
		    default:
			gagt_fatal ("GLK: Invalid font mode encountered");
			gagt_exit ();
		    }
		gagt_normal_string ("'.\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print out a
		 * list of valid font arguments.  Don't mention the debug
		 * setting, since it's not useful unless debugging this
		 * module.
		 */
		gagt_normal_string ("Glk font control can be ");
		gagt_standout_string (GAGT_COMMAND_FIXED);
		gagt_normal_string (", ");
		gagt_standout_string (GAGT_COMMAND_PROPORTIONAL);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_AUTOMATIC);
		gagt_normal_string (".\n");
	    }
}


/* List of valid additional delay control values. */
static const char	*GAGT_COMMAND_FULL		= "full";
static const char	*GAGT_COMMAND_SHORT		= "short";
static const char	*GAGT_COMMAND_HALF		= "half";
static const char	*GAGT_COMMAND_NONE		= "none";

/*
 * gagt_command_delays()
 *
 * Set a value for gagt_delay_mode depending on the argument from
 * the user's command escape.
 */
static void
gagt_command_delays (const char *argument)
{
	assert (argument != NULL);

	/* If delays are not possible, simply say so and return. */
	if (!gagt_delays_possible)
	    {
		gagt_normal_string ("Glk delays are not available.\n");
		return;
	    }

	/* Set up delay control according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_FULL)
			 || !gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		/* See if delay mode is already full. */
		if (gagt_delay_mode == DELAY_FULL)
		    {
			gagt_normal_string ("Glk delay mode is already '");
			gagt_normal_string (GAGT_COMMAND_FULL);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set full delays. */
		gagt_delay_mode	= DELAY_FULL;

		/* Confirm actions. */
		gagt_normal_string ("Glk delay mode is now '");
		gagt_normal_string (GAGT_COMMAND_FULL);
		gagt_normal_string ("'.\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_SHORT)
			|| !gagt_strcasecmp (argument, GAGT_COMMAND_HALF))
	    {
		/* See if delay mode is already short. */
		if (gagt_delay_mode == DELAY_SHORT)
		    {
			gagt_normal_string ("Glk delay mode is already '");
			gagt_normal_string (GAGT_COMMAND_SHORT);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set short delays. */
		gagt_delay_mode	= DELAY_SHORT;

		/* Confirm actions. */
		gagt_normal_string ("Glk delay mode is now '");
		gagt_normal_string (GAGT_COMMAND_SHORT);
		gagt_normal_string ("'.\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_NONE)
			|| !gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* See if delay mode is already none. */
		if (gagt_delay_mode == DELAY_OFF)
		    {
			gagt_normal_string ("Glk delay mode is already '");
			gagt_normal_string (GAGT_COMMAND_NONE);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Set no delays at all. */
		gagt_delay_mode	= DELAY_OFF;

		/* Confirm actions. */
		gagt_normal_string ("Glk delay mode is now '");
		gagt_normal_string (GAGT_COMMAND_NONE);
		gagt_normal_string ("'.\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * delay mode.
		 */
		gagt_normal_string ("Glk delay mode is set to '");
		switch (gagt_delay_mode)
		    {
		    case DELAY_FULL:
			gagt_normal_string (GAGT_COMMAND_FULL);
			break;
		    case DELAY_SHORT:
			gagt_normal_string (GAGT_COMMAND_SHORT);
			break;
		    case DELAY_OFF:
			gagt_normal_string (GAGT_COMMAND_NONE);
			break;
		    default:
			gagt_fatal ("GLK: Invalid delay mode encountered");
			gagt_exit ();
		    }
		gagt_normal_string ("'.\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print out a
		 * list of valid delay arguments.
		 */
		gagt_normal_string ("Glk delay mode can be ");
		gagt_standout_string (GAGT_COMMAND_FULL);
		gagt_normal_string (", ");
		gagt_standout_string (GAGT_COMMAND_SHORT);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_NONE);
		gagt_normal_string (".\n");
	    }
}


/*
 * gagt_command_replacements()
 *
 * Turn Glk special paragraph replacement on and off.
 */
static void
gagt_command_replacements (const char *argument)
{
	assert (argument != NULL);

	/* Set up replacement control according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		/* See if replacement mode is already on. */
		if (gagt_replacement_enabled)
		    {
			gagt_normal_string ("Glk replacements are already ");
			gagt_normal_string (GAGT_COMMAND_ON);
			gagt_normal_string (".\n");
			return;
		    }

		/* Enable replacements. */
		gagt_replacement_enabled	= TRUE;

		/* Confirm actions. */
		gagt_normal_string ("Glk replacements are now ");
		gagt_normal_string (GAGT_COMMAND_ON);
		gagt_normal_string (".\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* See if replacement is already off. */
		if (!gagt_replacement_enabled)
		    {
			gagt_normal_string ("Glk replacements are already ");
			gagt_normal_string (GAGT_COMMAND_OFF);
			gagt_normal_string (".\n");
			return;
		    }

		/* Disable replacements. */
		gagt_replacement_enabled	= FALSE;

		/* Confirm actions. */
		gagt_normal_string ("Glk replacements are now ");
		gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * replacement mode.
		 */
		gagt_normal_string ("Glk replacements are ");
		if (gagt_replacement_enabled)
			gagt_normal_string (GAGT_COMMAND_ON);
		else
			gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print out a
		 * list of valid replacement arguments.
		 */
		gagt_normal_string ("Glk replacements can be ");
		gagt_standout_string (GAGT_COMMAND_ON);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
}


/* List of valid additional statusline control values. */
static const char	*GAGT_COMMAND_EXTENDED		= "extended";
static const char	*GAGT_COMMAND_NORMAL		= "normal";

/*
 * gagt_command_statusline()
 *
 * Turn the extended status line on and off.
 */
static void
gagt_command_statusline (const char *argument)
{
	assert (argument != NULL);

	/* If there is no status window, simply say so and return. */
	if (gagt_status_window == NULL)
	    {
		gagt_normal_string ("Glk status window is not available.\n");
		return;
	    }

	/* Set up status line control according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_EXTENDED)
			 || !gagt_strcasecmp (argument, GAGT_COMMAND_FULL))
	    {
		/* See if extended status is already set. */
		if (gagt_extended_status_enabled)
		    {
			gagt_normal_string ("Glk status line mode"
							" is already '");
			gagt_normal_string (GAGT_COMMAND_EXTENDED);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Expand the status window down to a second line. */
		glk_window_set_arrangement
				(glk_window_get_parent (gagt_status_window),
				 winmethod_Above|winmethod_Fixed, 2, NULL);
		gagt_extended_status_enabled	= TRUE;

		/* Confirm actions. */
		gagt_normal_string ("Glk status line mode is now '");
		gagt_normal_string (GAGT_COMMAND_EXTENDED);
		gagt_normal_string ("'.\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_SHORT)
			 || !gagt_strcasecmp (argument, GAGT_COMMAND_NORMAL))
	    {
		/* See if short status is already set. */
		if (!gagt_extended_status_enabled)
		    {
			gagt_normal_string ("Glk status line mode"
							" is already '");
			gagt_normal_string (GAGT_COMMAND_SHORT);
			gagt_normal_string ("'.\n");
			return;
		    }

		/* Shrink the status window down to one line. */
		glk_window_set_arrangement
				(glk_window_get_parent (gagt_status_window),
				 winmethod_Above|winmethod_Fixed, 1, NULL);
		gagt_extended_status_enabled	= FALSE;

		/* Confirm actions. */
		gagt_normal_string ("Glk status line mode is now '");
		gagt_normal_string (GAGT_COMMAND_SHORT);
		gagt_normal_string ("'.\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * status window mode.
		 */
		gagt_normal_string ("Glk status line mode is set to '");
		if (gagt_extended_status_enabled)
			gagt_normal_string (GAGT_COMMAND_EXTENDED);
		else
			gagt_normal_string (GAGT_COMMAND_SHORT);
		gagt_normal_string ("'.\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print out a
		 * list of valid status line arguments.
		 */
		gagt_normal_string ("Glk status line can be ");
		gagt_standout_string (GAGT_COMMAND_EXTENDED);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_SHORT);
		gagt_normal_string (".\n");
	    }
}


/*
 * gagt_command_width()
 *
 * Print out the (approximate) display width, from status_width.  It's
 * approximate because the main window might include a scrollbar that
 * the status window doesn't have, may use a different size font, and so
 * on.  But the main window won't tell us a width at all - it always
 * returns zero.  If we don't happen to have a status window available
 * to us, there's not much we can say.
 *
 * Note that this function uses the interpreter variable status_width,
 * so it's important to keep this updated with the current window size at
 * all times.
 */
static void
gagt_command_width (const char *argument)
{
	assert (argument != NULL);

	/* See if we're currently managing any status window at all. */
	if (gagt_status_window != NULL)
	    {
		char	buffer[16];		/* Output buffer string. */

		/* Print the current approximate width. */
		gagt_normal_string ("Glk's current display width"
							" is approximately ");
		sprintf (buffer, "%d", status_width);
		gagt_normal_string (buffer);
		if (status_width == 1)
			gagt_normal_string (" character.\n");
		else
			gagt_normal_string (" characters.\n");
	    }
	else
	    {
		/* Print a message indicating we know nothing about width. */
		gagt_normal_string ("Glk's current display width"
							" is unknown.\n");
	    }
}


/*
 * gagt_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gagt_command_commands (const char *argument)
{
	assert (argument != NULL);

	/* Set up command handling according to the argument given. */
	if (!gagt_strcasecmp (argument, GAGT_COMMAND_ON))
	    {
		/* Commands must already be on. */
		gagt_normal_string ("Glk commands are already ");
		gagt_normal_string (GAGT_COMMAND_ON);
		gagt_normal_string (".\n");
	    }
	else if (!gagt_strcasecmp (argument, GAGT_COMMAND_OFF))
	    {
		/* The user has turned commands off. */
		gagt_commands_enabled = FALSE;
		gagt_normal_string ("Glk commands are now ");
		gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * command mode.
		 */
		gagt_normal_string ("Glk commands are ");
		if (gagt_commands_enabled)
			gagt_normal_string (GAGT_COMMAND_ON);
		else
			gagt_normal_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gagt_normal_string ("Glk commands can be ");
		gagt_standout_string (GAGT_COMMAND_ON);
		gagt_normal_string (", or ");
		gagt_standout_string (GAGT_COMMAND_OFF);
		gagt_normal_string (".\n");
	    }
}


/* Escape introducer string. */
static const char	*GAGT_COMMAND_ESCAPE		= "glk";

/* Small table of Glk subcommands and handler functions. */
struct gagt_command {
	const char	*command;		/* Glk subcommand. */
	void		(*handler) (const char *argument);
						/* Subcommand handler. */
	int		takes_argument;		/* Argument flag. */
};
typedef const struct gagt_command*	gagt_commandref_t;
static const struct gagt_command	GAGT_COMMAND_TABLE[] = {
	{ "script",		gagt_command_script,		TRUE  },
	{ "inputlog",		gagt_command_inputlog,		TRUE  },
	{ "readlog",		gagt_command_readlog,		TRUE  },
	{ "abbreviations",	gagt_command_abbreviations,	TRUE  },
	{ "fonts",		gagt_command_fonts,		TRUE  },
	{ "delays",		gagt_command_delays,		TRUE  },
	{ "width",		gagt_command_width,		FALSE },
	{ "replacements",	gagt_command_replacements,	TRUE  },
	{ "statusline",		gagt_command_statusline,	TRUE  },
	{ "version",		gagt_command_version,		FALSE },
	{ "commands",		gagt_command_commands,		TRUE  },
	{ NULL,			NULL,				FALSE }
};

/* List of whitespace command-argument separator characters. */
static const char	*GAGT_COMMAND_WHITESPACE	= "\t ";


/*
 * gagt_command_dispatch()
 *
 * Given a command string and an argument, this function finds and runs any
 * handler for it.
 *
 * It searches for the first unambiguous command to match the string passed
 * in.  The return is the count of command matches found; zero represents no
 * match (fail), one represents an unambiguous match (success, and handler
 * run), and more than one represents an ambiguous match (fail).
 */
static int
gagt_command_dispatch (const char *command, const char *argument)
{
	gagt_commandref_t	entry;		/* Table search entry. */
	gagt_commandref_t	matched;	/* Matched table entry. */
	int			matches;	/* Count of command matches. */
	assert (command != NULL && argument != NULL);

	/*
	 * Search for the first unambiguous table command string matching
	 * the command passed in.
	 */
	matches = 0;
	matched = NULL;
	for (entry = GAGT_COMMAND_TABLE;
				entry->command != NULL; entry++)
	    {
		if (!gagt_strncasecmp (command,
					entry->command, strlen (command)))
		    {
			matches++;
			matched = entry;
		    }
	    }

	/* If the match was unambiguous, call the command handler. */
	if (matches == 1)
	    {
		gagt_normal_char ('\n');
		(matched->handler) (argument);

		/* Issue a brief warning if an argument was ignored. */
		if (!matched->takes_argument && strlen (argument) > 0)
		    {
			gagt_normal_string ("[The ");
			gagt_standout_string (matched->command);
			gagt_normal_string
					(" command ignores arguments.]\n");
		    }
	    }

	/* Return the count of matching table entries. */
	return matches;
}


/*
 * gagt_command_usage()
 *
 * On an empty, invalid, or ambiguous command, print out a list of valid
 * commands and perhaps some Glk status information.
 */
static void
gagt_command_usage (const char *command, int is_ambiguous)
{
	gagt_commandref_t	entry;	/* Command table iteration entry. */
	assert (command != NULL);

	/* Print a blank line separator. */
	gagt_normal_char ('\n');

	/* If the command isn't empty, indicate ambiguous or invalid. */
	if (strlen (command) > 0)
	    {
		gagt_normal_string ("The Glk command ");
		gagt_standout_string (command);
		if (is_ambiguous)
			gagt_normal_string (" is ambiguous.\n");
		else
			gagt_normal_string (" is not valid.\n");
	    }

	/* Print out a list of valid commands. */
	gagt_normal_string ("Glk commands are");
	for (entry = GAGT_COMMAND_TABLE; entry->command != NULL; entry++)
	    {
		gagt_commandref_t	next;	/* Next command table entry. */

		next = entry + 1;
		gagt_normal_string (next->command != NULL ? " " : " and ");
		gagt_standout_string (entry->command);
		gagt_normal_string (next->command != NULL ? "," : ".\n");
	    }

	/* Write a note about abbreviating commands. */
	gagt_normal_string ("Glk commands may be abbreviated, as long as");
	gagt_normal_string (" the abbreviations are unambiguous.\n");

	/*
	 * If no command was given, call each command handler function with
	 * an empty argument to prompt each to report the current setting.
	 */
	if (strlen (command) == 0)
	    {
		gagt_normal_char ('\n');
		for (entry = GAGT_COMMAND_TABLE;
					entry->command != NULL; entry++)
			(entry->handler) ("");
	    }
}


/*
 * gagt_skip_characters()
 *
 * Helper function for command escapes.  Skips over either whitespace or
 * non-whitespace in string, and returns the revised string pointer.
 */
static char *
gagt_skip_characters (char *string, int skip_whitespace)
{
	char		*result;		/* Return string pointer. */
	assert (string != NULL);

	/* Skip over leading characters of the specified type. */
	for (result = string; *result != '\0'; result++)
	    {
		int	is_whitespace;		/* Whitespace flag. */

		/* Break if encountering a character not the required type. */
		is_whitespace =
			(strchr (GAGT_COMMAND_WHITESPACE, *result) != NULL);
		if ((skip_whitespace && !is_whitespace)
				|| (!skip_whitespace && is_whitespace))
			break;
	    }

	/* Return the revised pointer. */
	return result;
}


/*
 * gagt_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 */
static int
gagt_command_escape (char *string)
{
	char		*temporary;		/* Temporary string pointer */
	char		*string_copy;		/* Destroyable string copy. */
	char		*command;		/* Glk subcommand. */
	char		*argument;		/* Glk subcommand argument. */
	int		matches;		/* Dispatcher matches. */
	assert (string != NULL);

	/*
	 * Return FALSE if the string doesn't begin with the Glk command
	 * escape introducer.
	 */
	temporary = gagt_skip_characters (string, TRUE);
	if (gagt_strncasecmp (temporary, GAGT_COMMAND_ESCAPE,
					strlen (GAGT_COMMAND_ESCAPE)))
		return FALSE;

	/* Take a copy of the string, without any leading space. */
	string_copy = gagt_malloc (strlen (temporary) + 1);
	strcpy (string_copy, temporary);

	/* Find the subcommand; the word after the introducer. */
	command = gagt_skip_characters (string_copy
				+ strlen (GAGT_COMMAND_ESCAPE), TRUE);

	/* Skip over command word, be sure it terminates with NUL. */
	temporary = gagt_skip_characters (command, FALSE);
	if (*temporary != '\0')
	    {
		*temporary = '\0';
		temporary++;
	    }

	/* Now find any argument data for the command. */
	argument = gagt_skip_characters (temporary, TRUE);

	/* Ensure that argument data also terminates with a NUL. */
	temporary = gagt_skip_characters (argument, FALSE);
	*temporary = '\0';

	/*
	 * Try to handle the command and argument as a Glk subcommand.  If
	 * it doesn't run unambiguously, print command usage.
	 */
	matches = gagt_command_dispatch (command, argument);
	if (matches != 1)
	    {
		if (matches == 0)
			gagt_command_usage (command, FALSE);
		else
			gagt_command_usage (command, TRUE);
	    }

	/* Done with the copy of the string. */
	free (string_copy);

	/* Return TRUE to indicate string contained a Glk command. */
	return TRUE;
}


/*---------------------------------------------------------------------*/
/*  Glk port input functions                                           */
/*---------------------------------------------------------------------*/

/* Longest line we're going to buffer for input. */
enum {			GAGT_INPUTBUFFER_LENGTH		= 256 };

/* Quote used to suppress abbreviation expansion and local commands. */
static const char	GAGT_QUOTED_INPUT		= '\'';

/* Definition of whitespace characters to skip over. */
static const char	*GAGT_WHITESPACE		= "\t ";


/*
 * gagt_char_is_whitespace()
 *
 * Check for ASCII whitespace characters.  Returns TRUE if the character
 * qualifies as whitespace (NUL is not whitespace).
 */
static int
gagt_char_is_whitespace (char c)
{
	return (c != '\0' && strchr (GAGT_WHITESPACE, c) != NULL);
}


/*
 * gagt_skip_leading_whitespace()
 *
 * Skip over leading whitespace, returning the address of the first non-
 * whitespace character.
 */
static char *
gagt_skip_leading_whitespace (char *string)
{
	char	*result;			/* Return string pointer. */
	assert (string != NULL);

	/* Move result over leading whitespace. */
	for (result = string; gagt_char_is_whitespace (*result); )
		result++;
	return result;
}


/* Table of single-character command abbreviations. */
struct gagt_abbreviation {
	const char	abbreviation;		/* Abbreviation character. */
	const char	*expansion;		/* Expansion string. */
};
typedef const struct gagt_abbreviation*	gagt_abbreviationref_t;
static const struct gagt_abbreviation	GAGT_ABBREVIATIONS[] = {
	{ 'c',	"close"   },	{ 'g',	"again" },	{ 'i',	"inventory" },
	{ 'k',	"attack"  },	{ 'l',	"look"  },	{ 'p',	"open"      },
	{ 'q',	"quit"    },	{ 'r',	"drop"  },	{ 't',	"take"      },
	{ 'x',	"examine" },	{ 'y',	"yes"   },	{ 'z',	"wait"      },
	{ '\0',	NULL }
};

/*
 * gagt_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems, but not always normal in AGT games.
 */
static void
gagt_expand_abbreviations (char *buffer, int size)
{
	char		*command;	/* Single-character command. */
	gagt_abbreviationref_t
			entry, match;	/* Table search entry, and match. */
	assert (buffer != NULL);

	/* Skip leading spaces to find command, and return if nothing. */
	command = gagt_skip_leading_whitespace (buffer);
	if (strlen (command) == 0)
		return;

	/* If the command is not a single letter one, do nothing. */
	if (strlen (command) > 1
			&& !gagt_char_is_whitespace (command[1]))
		return;

	/* Scan the abbreviations table for a match. */
	match = NULL;
	for (entry = GAGT_ABBREVIATIONS; entry->expansion != NULL; entry++)
	    {
		if (entry->abbreviation == glk_char_to_lower
					((unsigned char) command[0]))
		    {
			match = entry;
			break;
		    }
	    }
	if (match == NULL)
		return;

	/* Match found, check for a fit. */
	if (strlen (buffer) + strlen (match->expansion) - 1 >= size)
		return;

	/* Replace the character with the full string. */
	memmove (command + strlen (match->expansion) - 1,
					command, strlen (command) + 1);
	memcpy (command, match->expansion, strlen (match->expansion));

	/* Provide feedback on the expansion. */
	gagt_standout_string ("[");
	gagt_standout_char   (match->abbreviation);
	gagt_standout_string (" -> ");
	gagt_standout_string (match->expansion);
	gagt_standout_string ("]\n");
}


/*
 * agt_input()
 *
 * Read a line from the keyboard, allocating space for it using malloc.
 * AGiliTy defines the following for the in_type argument:
 *
 *   in_type: 0=command, 1=number, 2=question, 3=userstr, 4=filename,
 *               5=RESTART,RESTORE,UNDO,QUIT
 *   Negative values are for internal use by the interface (i.e. this module)
 *   and so are free to be defined by the porter.
 *
 * Since it's unclear what use we can make of this information in Glk,
 * for the moment the argument is ignored.  It seems that no-one else
 * uses it, either.
 */
char *
agt_input (int in_type)
{
	event_t		event;			/* Glk event buffer. */
	char		*buffer;		/* Line input buffer. */

	/*
	 * Update the current status line display, and flush any pending
	 * buffered output.  Release any suspension of delays.
	 */
	gagt_status_notify ();
	gagt_output_flush ();
	gagt_delay_resume ();

	/* Reset current x, as line input implies a newline. */
	curr_x = 0;

	/* Allocate a line input buffer, allowing 256 characters and a NUL. */
	buffer = gagt_malloc (GAGT_INPUTBUFFER_LENGTH + 1);

	/*
	 * If we have an input log to read from, use that until it is
	 * exhausted.  On end of file, close the stream and resume input
	 * from line requests.
	 */
	if (gagt_readlog_stream != NULL)
	    {
		glui32		chars;		/* Characters read. */

		/* Get the next line from the log stream. */
		chars = glk_get_line_stream
				(gagt_readlog_stream, buffer,
			 			GAGT_INPUTBUFFER_LENGTH + 1);
		if (chars > 0)
		    {
			/* Echo the line just read in input style. */
			glk_set_style (style_Input);
			glk_put_buffer (buffer, chars);
			glk_set_style (style_Normal);

			/*
			 * Convert the string from Glk's ISO 8859 Latin-1
			 * to IBM cp 437, add to any script, and return it.
			 */
			gagt_iso_to_cp (buffer, buffer);
			if (script_on)
				textputs (scriptfile, buffer);
			return buffer;
		    }

		/*
		 * We're at the end of the log stream.  Close it, and then
		 * continue on to request a line from Glk.
		 */
		glk_stream_close (gagt_readlog_stream, NULL);
		gagt_readlog_stream = NULL;
	    }

	/* Set this up as a read buffer for the main window, and wait. */
	glk_request_line_event (gagt_main_window, buffer,
						GAGT_INPUTBUFFER_LENGTH, 0);
	gagt_event_wait (evtype_LineInput, &event);

	/* Terminate the input line with a NUL. */
	assert (event.val1 <= GAGT_INPUTBUFFER_LENGTH);
	buffer[ event.val1 ] = '\0';

	/*
	 * If neither abbreviations nor local commands are enabled, use the
	 * data read above without further massaging.
	 */
	if (gagt_abbreviations_enabled
				|| gagt_commands_enabled)
	    {
		char		*command;	/* Command part of buffer. */

		/* Find the first non-space character in the input buffer. */
		command = gagt_skip_leading_whitespace (buffer);

		/*
		 * If the first non-space input character is a quote, bypass
		 * all abbreviation expansion and local command recognition,
		 * and use the unadulterated input, less introductory quote.
		 */
		if (command[0] == GAGT_QUOTED_INPUT)
		    {
			/* Delete the quote with memmove(). */
			memmove (command, command + 1, strlen (command));
		    }
		else
		    {
			/* Check for, and expand, any abbreviated commands. */
			if (gagt_abbreviations_enabled)
				gagt_expand_abbreviations (buffer,
						GAGT_INPUTBUFFER_LENGTH + 1);

			/*
			 * Check for Glk port special commands, and if found
			 * then suppress the interpreter's use of this input.
			 */
			if (gagt_commands_enabled
					&& gagt_command_escape (buffer))
			    {
				/*
				 * Overwrite the buffer with an empty line if
				 * we saw a command.
				 */
				buffer[0] = '\0';

				/* Return the overwritten input line. */
				return buffer;
			    }
		    }
	    }

	/*
	 * If there is an input log active, log this input string to it.
	 * Note that by logging here we get any abbreviation expansions but
	 * we won't log glk special commands, nor any input read from a
	 * current open input log.
	 */
	if (gagt_inputlog_stream != NULL)
	    {
		glk_put_string_stream (gagt_inputlog_stream, buffer);
		glk_put_char_stream (gagt_inputlog_stream, '\n');
	    }

	/*
	 * Convert from Glk's ISO 8859 Latin-1 to IBM cp 437, and add to any
	 * script.
	 */
	gagt_iso_to_cp (buffer, buffer);
	if (script_on)
		textputs (scriptfile, buffer);

	/* Return the buffer. */
	gagt_debug ("agt_input", "in_type=%d -> '%s'", in_type, buffer);
	return buffer;
}


/*
 * agt_getkey()
 *
 * Read a single character and return it.  AGiliTy defines the echo_char
 * argument as:
 *
 *   If echo_char=1, echo character. If 0, then the character is not
 *   required to be echoed (and ideally shouldn't be).
 *
 * However, I've found that not all other ports really do this, and in
 * practice it doesn't always look right.  So for Glk, the character is
 * always echoed to the main window.
 */
char
agt_getkey (rbool echo_char)
{
	event_t		event;			/* Glk event buffer. */
	char		buffer[3];		/* Char, and textputs buffer. */
	assert (glk_stream_get_current () != NULL);

	/*
	 * Update the current status line display, and flush any pending
	 * buffered output.  Release any suspension of delays.
	 */
	gagt_status_notify ();
	gagt_output_flush ();
	gagt_delay_resume ();

	/* Reset current x, as echoed character input implies a newline. */
	curr_x = 0;

	/*
	 * If we have an input log to read from, use that as above until it
	 * is exhausted.  We take just the first character of a given line.
	 */
	if (gagt_readlog_stream != NULL)
	    {
		glui32		chars;		/* Characters read. */
		char		logbuffer[GAGT_INPUTBUFFER_LENGTH];
						/* Input log buffer. */

		/* Get the next line from the log stream. */
		chars = glk_get_line_stream (gagt_readlog_stream,
						logbuffer, sizeof (logbuffer));
		if (chars > 0)
		    {
			/*
			 * Take just the first character, adding a newline
			 * if necessary.
			 */
			buffer[0] = logbuffer[0];
			if (buffer[0] == '\n')
				buffer[1] = '\0';
			else
			    {
				buffer[1] = '\n';
				buffer[2] = '\0';
			    }

			/* Echo the character just read in input style. */
			glk_set_style (style_Input);
			glk_put_buffer (buffer, chars);
			glk_set_style (style_Normal);

			/*
			 * Convert from Glk's ISO 8859 Latin-1 to IBM cp 437,
			 * add to any script, and return the character.
			 */
			gagt_iso_to_cp (buffer, buffer);
			if (script_on)
				textputs (scriptfile, buffer);
			return buffer[0];
		    }

		/*
		 * We're at the end of the log stream.  Close it, and then
		 * continue on to request a character from Glk.
		 */
		glk_stream_close (gagt_readlog_stream, NULL);
		gagt_readlog_stream = NULL;
	    }

	/*
	 * Request a single character from main window, and wait.  Ignore non-
	 * ASCII codes that Glk returns for special function keys; we just want
	 * one ASCII return value.  (Glk does treat Return as a special key,
	 * though, and we want to pass that back as ASCII return.)
	 */
	do
	    {
		glk_request_char_event (gagt_main_window);
		gagt_event_wait (evtype_CharInput, &event);
	    }
	while (event.val1 > UCHAR_MAX && event.val1 != keycode_Return);

	/*
	 * Save the character into a short string buffer, converting Return
	 * to newline, and adding a newline if not Return.
	 */
	buffer[0] = (event.val1 == keycode_Return) ? '\n' : event.val1;
	if (buffer[0] == '\n')
		buffer[1] = '\0';
	else
	    {
		buffer[1] = '\n';
		buffer[2] = '\0';
	    }

	/* If there is an input log active, log this input string to it. */
	if (gagt_inputlog_stream != NULL)
		glk_put_string_stream (gagt_inputlog_stream, buffer);

	/*
	 * No matter what echo_char says, as it happens, the output doesn't
	 * doesn't look great if we don't write out the character, and also
	 * a newline (c.f. the "Yes/No" confirmation of the QUIT command)...
	 */
	glk_set_style (style_Input);
	glk_put_string (buffer);
	glk_set_style (style_Normal);

	/*
	 * Convert from Glk's ISO 8859 Latin-1 to IBM cp 437, and add to any
	 * script.
	 */
	gagt_iso_to_cp (buffer, buffer);
	if (script_on)
		textputs (scriptfile, buffer);

	/* Finally, return the character typed. */
	gagt_debug ("agt_getkey", "echo_char=%d -> '%c'",
				echo_char, buffer[0] == '\n' ? '$' : buffer[0]);
	return buffer[0];
}


/*---------------------------------------------------------------------*/
/*  Glk port event functions                                           */
/*---------------------------------------------------------------------*/

/*
 * gagt_event_wait_2()
 * gagt_event_wait()
 *
 * Process Glk events until one of the expected type, or types, arrives.
 * Return the event of that type.
 */
static void
gagt_event_wait_2 (glui32 wait_type_1, glui32 wait_type_2, event_t *event)
{
	assert (event != NULL);

	/* Get events, until one matches one of the requested types. */
	do
	    {
		/* Get next event. */
		glk_select (event);

		/* Handle events of interest locally. */
		switch (event->type)
		    {
		    case evtype_Arrange:
		    case evtype_Redraw:
			/* Refresh any sensitive windows on size events. */
			gagt_status_redraw ();
			break;
		    }
	    }
	while (event->type != wait_type_1
				&& event->type != wait_type_2);
}

static void
gagt_event_wait (glui32 wait_type, event_t *event)
{
	assert (event != NULL);
	gagt_event_wait_2 (wait_type, evtype_None, event);
}


/*---------------------------------------------------------------------*/
/*  Miscellaneous Glk port startup and options functions               */
/*---------------------------------------------------------------------*/

/*
 * Default screen height and width, and also a default status width for
 * use with Glk libraries that don't support separate windows.
 */
static const int	GAGT_DEFAULT_SCREEN_WIDTH	= 80;
static const int	GAGT_DEFAULT_SCREEN_HEIGHT	= 25;
static const int	GAGT_DEFAULT_STATUS_WIDTH	= 76;


/*
 * agt_option()
 *
 * Platform-specific setup and options handling.  AGiliTy defines the
 * arguments and options as:
 *
 *   If setflag is 0, then the option was prefixed with NO_. Return 1 if
 *   the option is recognized.
 *
 * The Glk port has no options file handling, so none of this is
 * implemented here.
 */
rbool
agt_option (int optnum, char *optstr[], rbool setflag)
{
	gagt_debug ("agt_option", "optnum=%d, optstr=%s, setflag=%d",
						optnum, optstr[0], setflag);
	return 0;
}


/*
 * agt_globalfile()
 *
 * Global options file handle handling.  For now, this is a stub, since
 * there is no .agilrc for this port.
 */
genfile
agt_globalfile (int fid)
{
	gagt_debug ("agt_globalfile", "fid=%d", fid);
	return badfile (fCFG);
}


/*
 * init_interface()
 *
 * General initialization for the module; sets some variables, and creates
 * the Glk windows to work in.  Called from the AGiliTy main().
 */
void
init_interface (int argc, char *argv[])
{
	glui32		status_height;		/* Status window height. */

	/*
	 * Begin with some default values for global variables that this
	 * module is somehow responsible for.
	 */
	script_on	= FALSE;
	scriptfile	= badfile (fSCR);
	center_on	= FALSE;
	par_fill_on	= FALSE;
	debugfile	= stderr;

	/*
	 * Set up AGT-specific Glk styles.  This needs to be done before any
	 * Glk window is opened.
	 */
	gagt_init_user_styles ();

	/*
	 * Create the main game window.  The main game window creation must
	 * succeed.  If it fails, we'll return, and the caller can detect
	 * this by looking for NULL main window.
	 */
	gagt_main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
	if (gagt_main_window == NULL)
		return;

	/*
	 * Set the main window to be the default window, for convenience.
	 * We do this again in glk_main() -- this call is here just in case
	 * this version of init_interface() is ever called by AGiliTy's main.
	 */
	glk_set_window (gagt_main_window);

	/*
	 * Screen height is something we don't use.  Linux Xglk returns
	 * dimensions of 0x0 for text buffer windows, so we can't measure
	 * the main window height anyway.  But... the height does come into
	 * play in AGiliTy's agil.c, when the interpreter is deciding how
	 * to output game titles, and how much of its own subsequent verb-
	 * iage to output.  This gives us a problem, since this "verbiage"
	 * is stuff we look for and replace with our own special text.  So...
	 * sigh, set 25, and try to cope in the special text we've set up
	 * with all the variations that ensue.
	 *
	 * Screen width does get used, but so, so many games, and for that
	 * matter the interpreter itself, assume 80 chars, so it's simplest
	 * just to set, and keep, this, and put up with the minor odd effects
	 * (making it match status_width, or making it something like MAX_INT
	 * to defeat the game's own wrapping, gives a lot of odder effects,
	 * trust me on this one...).
	 */
	screen_width	= GAGT_DEFAULT_SCREEN_WIDTH;
	screen_height	= GAGT_DEFAULT_SCREEN_HEIGHT;

	/*
	 * Create a status window, with one or two lines as selected by
	 * user options or flags.  We can live without a status window if
	 * we have to.
	 */
	status_height = gagt_extended_status_enabled ? 2 : 1;
	gagt_status_window = glk_window_open (gagt_main_window,
			 		winmethod_Above|winmethod_Fixed,
					status_height, wintype_TextGrid, 0);
	if (gagt_status_window != NULL)
	    {
		/*
		 * Call gagt_status_redraw() to set the interpreter's
		 * status_width variable initial value.
		 */
		gagt_status_redraw ();
	    }
	else
	    {
		/*
		 * No status window, so set a suitable default status width.
		 * In this case, we're using a value four characters less
		 * than the set screen width.  AGiliTy's status line code
		 * will fill to this width with justified text, and we add
		 * two characters of bracketing when displaying status lines
		 * for Glks that don't support separate windows, making a
		 * total of 78 characters, which should be fairly standard.
		 */
		status_width	= GAGT_DEFAULT_STATUS_WIDTH;
	    }

	/* Initial clear screen. */
	agt_clrscr ();

	gagt_debug ("init_interface", "argc=%d, argv=%p", argc, argv);
}


/*---------------------------------------------------------------------*/
/*  Replacement interface.c functions                                  */
/*---------------------------------------------------------------------*/

/* Get_user_file() type codes. */
enum {			AGT_SCRIPT			= 0,
			AGT_SAVE			= 1,
			AGT_RESTORE			= 2,
			AGT_LOG_READ			= 3,
			AGT_LOG_WRITE			= 4 };

/* Longest acceptable filename. */
enum {			GAGT_MAX_PATH			= 1024 };


#ifdef GLK_ANSI_ONLY
/*
 * gagt_confirm()
 *
 * Print a confirmation prompt, and read a single input character, taking
 * only [YyNn] input.  If the character is 'Y' or 'y', return TRUE.
 *
 * This function is only required for the ANSI version of get_user_file().
 */
static int
gagt_confirm (const char *prompt)
{
	event_t		event;			/* Glk event buffer. */
	unsigned char	response;		/* Response character. */
	assert (prompt != NULL);

	/*
	 * Print the confirmation prompt, in a style that hints that it's
	 * from the interpreter, not the game.
	 */
	gagt_standout_string (prompt);

	/* Wait for a single 'Y' or 'N' character response. */
	do
	    {
		/* Wait for a standard key, ignoring Glk special keys. */
		do
		    {
			glk_request_char_event (gagt_main_window);
			gagt_event_wait (evtype_CharInput, &event);
		    }
		while (event.val1 > UCHAR_MAX);
		response = glk_char_to_upper (event.val1);
	    }
	while (response != 'Y' && response != 'N');

	/* Echo the confirmation response, and a blank line. */
	glk_set_style (style_Input);
	glk_put_string (response == 'Y' ? "Yes" : "No" );
	glk_set_style (style_Normal);
	glk_put_string ("\n");

	/* Return TRUE if 'Y' was entered. */
	return (response == 'Y');
}
#endif


/*
 * gagt_get_user_file_ansi()
 * gagt_get_user_file_glk()
 *
 * Alternative versions of functions to get a file name from the user, and
 * return a file stream structure.  These functions are front-ended by the
 * main get_user_file() function, which first converts the AGT file type
 * into Glk usage and filemode, and also a mode for fopen()/fdopen().
 *
 * The ANSI version of the function prompts for the file using the simple
 * method of querying the user through input in the main window.  It then
 * constructs a file stream around the path entered, and returns it.
 *
 * The non-ANSI, Glk version is more sneaky.  It prompts for a file using
 * Glk's functions to get filenames by prompt, file selection dialog, or
 * whatever.  Then it attempts to uncover which file descriptor Glk opened
 * its file on, dup's it, closes the Glk stream, and returns a file stream
 * built on this file descriptor.  This is all highly non-ANSI, requiring
 * dup() and fdopen(), and making some assumptions about the way that dup,
 * open, and friends work.  It works on Linux, and on Mac (CodeWarrior).
 * It may also work for you, but if it doesn't, or if your system lacks
 * things like dup or fdopen, define GLK_ANSI_ONLY and use the safe version.
 *
 * If GARGLK is used, non-ansi version calls garglk_fileref_get_name()
 * instead, and opens a file the highly portable way, but still with a
 * Glkily nice prompt dialog.
 */
#ifdef GLK_ANSI_ONLY
static genfile
gagt_get_user_file_ansi (glui32 usage, glui32 fmode, char *fdtype)
{
	char		filepath[GAGT_MAX_PATH];/* Path to file to open. */
	event_t		event;			/* Glk event. */
	int		index;			/* String iterator. */
	int		all_spaces;		/* String all space flag. */
	genfile		retfile;		/* Return value. */
	assert (fdtype != NULL);

	/* Prompt in a similar way to Glk. */
	switch (usage)
	    {
	    case fileusage_SavedGame:
		gagt_normal_string ("Enter saved game");
		break;

	    case fileusage_Transcript:
		gagt_normal_string ("Enter transcript file");
		break;

	    case fileusage_InputRecord:
		gagt_normal_string ("Enter command record file");
		break;
	    }
	switch (fmode)
	    {
	    case filemode_Read:
		gagt_normal_string (" to load: ");
		break;

	    case filemode_Write:
		gagt_normal_string (" to store: ");
		break;
	    }

	/* Get the path to the file from the user. */
	glk_request_line_event (gagt_main_window, filepath,
						sizeof (filepath) - 1, 0);
	gagt_event_wait (evtype_LineInput, &event);

	/* Terminate the file path with a NUL. */
	assert (event.val1 < sizeof (filepath));
	filepath[ event.val1 ] = '\0';

	/* Reject file paths that only contain any whitespace characters. */
	all_spaces = TRUE;
	for (index = 0; index < strlen (filepath); index++)
	    {
		if (!isspace (filepath[ index ]))
		    {
			all_spaces = FALSE;
			break;
		    }
	    }
	if (all_spaces)
		return badfile (fSAV);

	/* Confirm overwrite of any existing file. */
	if (fmode == filemode_Write)
	    {
		genfile		file;			/* Test file */

		/* If openable for read, assume it exists. */
		file = fopen (filepath, "r");
		if (file != NULL)
		    {
			fclose (file);

			/* Confirm overwrite. */
			if (!gagt_confirm ("Overwrite existing file? [y/n] "))
				return badfile (fSAV);
		    }
	    }

	/* Open a FILE* stream onto the return descriptor. */
	retfile = fopen (filepath, fdtype);
	if (retfile == NULL)
		return badfile (fSAV);

	/* Return the resulting file stream. */
	return retfile;
}

#else
static genfile
gagt_get_user_file_glk (glui32 usage, glui32 fmode, char *fdtype)
{
	frefid_t	fileref;		/* Glk file reference. */
	strid_t		stream;			/* Glk stream reference. */
	int		tryfd, glkfd, dupfd, retfd;
						/* File descriptors. */
	genfile		retfile;		/* Return value. */
	assert (fdtype != NULL);

	/* Try to get a Glk file reference with these attributes. */
	fileref = glk_fileref_create_by_prompt (usage, fmode, 0);
	if (fileref == NULL)
		return badfile (fSAV);

	/*
	 * Reject the file reference if we're expecting to read from it,
	 * and the referenced file doesn't exist.
	 */
	if (fmode == filemode_Read
			&& !glk_fileref_does_file_exist (fileref))
	    {
		glk_fileref_destroy (fileref);
		return badfile (fSAV);
	    }

	/*
	 * Now, it gets ugly.  Glk assumes that the interpreter will do all of
	 * its reading and writing using the Glk streams read/write functions.
	 * It won't; at least, not without major surgery.  So here we're going
	 * to do some dangerous stuff...
	 *
	 * Since a Glk stream is opaque, it's hard to tell what the underlying
	 * file descriptor is for it.  We can get it if we want to play around
	 * in the internals of the strid_t structure, but it's unpleasant.
	 * The alternative is, arguably, no more pleasant, but it makes for
	 * (perhaps) more portable code.  What we'll do is to dup a file, then
	 * immediately close it, and call glk_stream_open_file().  The open()
	 * in glk_stream_open_file() will return the same file descriptor number
	 * that we just close()d (in theory...).  This makes the following two
	 * major assumptions:
	 *
	 *  1) glk_stream_open_file() opens precisely one file with open()
	 *  2) open() always uses the lowest available file descriptor number,
	 *     like dup()
	 *
	 * Believe it or not, this is better than the alternatives.  There is
	 * no Glk function to return the filename from a frefid_t, and it
	 * moves about in different Glk libraries so we can't just take it
	 * from a given offset.  And there is no Glk function to return the
	 * underlying file descriptor or FILE* from a Glk stream either. :-(
	 */

#ifdef GARGLK
	retfile = fopen(garglk_fileref_get_name(fileref), fdtype);
#else

	/* So, start by dup()'ing the first file descriptor we can, ... */
	glkfd = -1;
	for (tryfd = 0; tryfd < FD_SETSIZE; tryfd++)
	    {
		glkfd = fcntl (tryfd, F_DUPFD, 0);
		if (glkfd != -1)
			break;
	    }
	if (tryfd >= FD_SETSIZE)
	    {
		glk_fileref_destroy (fileref);
		return badfile (fSAV);
	    }

	/* ...then closing it, ... */
	close (glkfd);

	/* ...now open the Glk stream, assuming it opens on file 'glkfd', ... */
	stream = glk_stream_open_file (fileref, fmode, 0);
	if (stream == NULL)
	    {
		glk_fileref_destroy (fileref);
		return badfile (fSAV);
	    }

	/* ...dup() the Glk file onto another file descriptor, ... */
	dupfd = fcntl (glkfd, F_DUPFD, 0);
	assert (dupfd != -1);

	/* ...close and destroy the Glk edifice for this file, ... */
	glk_stream_close (stream, NULL);
	glk_fileref_destroy (fileref);

	/* ...for neatness, dup() back to the old Glk file descriptor, ... */
	retfd = fcntl (dupfd, F_DUPFD, 0);
	assert (retfd != -1 && retfd == glkfd);
	close (dupfd);

	/* ...and finally, open a FILE* stream onto the return descriptor. */
	retfile = fdopen (retfd, fdtype);
	if (retfile == NULL)
		return badfile (fSAV);
#endif

	/*
	 * The result of all of this should now be that retfile is a FILE*
	 * wrapper round a file descriptor open on a file indicated by the
	 * user through Glk.  Return it.
	 */
	return retfile;
}
#endif


/*
 * get_user_file()
 *
 * Get a file name from the user, and return the file stream structure.
 * This is a front-end to ANSI and non-ANSI variants of the function.
 */
genfile
get_user_file (int type)
{
	glui32		usage, fmode;		/* File attributes. */
	char		*fdtype;		/* Type for fopen/fdopen. */
	genfile		retfile;		/* Return value. */

	/* Flush any pending buffered output. */
	gagt_output_flush ();

	/* Map AGiliTy type to Glk usage and filemode. */
	switch (type)
	    {
	    case AGT_SCRIPT:
		usage = fileusage_Transcript;
		fmode = filemode_Write;
		break;

	    case AGT_SAVE:
		usage = fileusage_SavedGame;
		fmode = filemode_Write;
		break;

	    case AGT_RESTORE:
		usage = fileusage_SavedGame;
		fmode = filemode_Read;
		break;

	    case AGT_LOG_READ:
		usage = fileusage_InputRecord;
		fmode = filemode_Read;
		break;

	    case AGT_LOG_WRITE:
		usage = fileusage_InputRecord;
		fmode = filemode_Write;
		break;

	    default:
		gagt_fatal ("GLK: Unknown file type encountered");
		return badfile (fSAV);
	    }

	/* From these, determine a mode type for the f[d]open() call. */
	if (fmode == filemode_Write)
		fdtype = (usage == fileusage_SavedGame) ? "wb" : "w";
	else
		fdtype = (usage == fileusage_SavedGame) ? "rb" : "r";

	/* Get a file stream from these using the appropriate function. */
#ifdef GLK_ANSI_ONLY
	retfile = gagt_get_user_file_ansi (usage, fmode, fdtype);
#else
	retfile = gagt_get_user_file_glk (usage, fmode, fdtype);
#endif

	/* Return the resulting file stream. */
	gagt_debug ("get_user_file", "type=%d -> %p", type, retfile);
	return retfile;
}


/*
 * set_default_filenames()
 *
 * Set defaults for last save, log, and script filenames.
 */
void
set_default_filenames (fc_type fc)
{
	/*
	 * There is nothing to do in this function, since Glk has its own
	 * ideas on default names for files obtained with a prompt.
	 */
	gagt_debug ("set_default_filenames", "fc=%p", fc);
}


/*---------------------------------------------------------------------*/
/*  Functions intercepted by link-time wrappers                        */
/*---------------------------------------------------------------------*/

/*
 * __wrap_toupper()
 * __wrap_tolower()
 *
 * Wrapper functions around toupper(), tolower(), and fatal().  The Linux
 * linker's --wrap option will convert calls to mumble() to __wrap_mumble()
 * if we give it the right options.  We'll use this feature to translate
 * all toupper() and tolower() calls in the interpreter code into calls to
 * Glk's versions of these functions.
 *
 * It's not critical that we do this.  If a linker, say a non-Linux one,
 * won't do --wrap, then just do without it.  It's unlikely that there
 * will be much noticeable difference.
 */
int
__wrap_toupper (int ch)
{
	unsigned char		uch;

	uch = glk_char_to_upper ((unsigned char) ch);
	return (int) uch;
}
int
__wrap_tolower (int ch)
{
	unsigned char		lch;

	lch = glk_char_to_lower ((unsigned char) ch);
	return (int) lch;
}


/*---------------------------------------------------------------------*/
/*  Replacements for AGiliTy main() and options parsing                */
/*---------------------------------------------------------------------*/

/* External declaration of interface.c's set default options function. */
extern void		set_default_options (void);

/*
 * The following values need to be passed between the startup_code and main
 * functions.
 */
static int	gagt_saved_argc		= 0;		/* Recorded argc. */
static char	**gagt_saved_argv	= NULL;		/* Recorded argv. */
static char	*gagt_gamefile		= NULL;		/* Name of game file. */
static char	*gagt_game_message	= NULL;		/* Error message. */

/*
 * Flag to set if we want to test for a clean exit.  Without this it's a
 * touch tricky sometimes to corner AGiliTy into calling exit() for us; it
 * tends to require a broken game file.
 */
static int	gagt_clean_exit_test	= FALSE;


/*
 * gagt_parse_option()
 *
 * Glk-ified version of AGiliTy's parse_options() function.  In practice,
 * because Glk has got to them first, most options that come in here are
 * probably going to be single-character ones, since this is what we told
 * Glk in the arguments structure above.  The Glk font control and other
 * special tweaky flags will probably be the only multiple-character ones.
 */
static int
gagt_parse_option (const char *option)
{
	unsigned int	index;			/* Option character index. */
	assert (option != NULL);

	/* Handle each option character in the string passed in. */
	assert (option[0] == '-');
	for (index = 1; option[ index ] != '\0'; index++)
	    {
		switch (option[ index ])
		    {
		    case 'g':
			/*
			 * For -g, read the next character, to get Glk special
			 * controls.
			 */
			switch (option[ ++index ])
			    {
			    case 'f':
				gagt_font_mode		    = FONT_FIXED_WIDTH;
				break;
			    case 'p':
				gagt_font_mode		    = FONT_PROPORTIONAL;
				break;
			    case 'a':
				gagt_font_mode		    = FONT_AUTOMATIC;
				break;
			    case 'd':
				gagt_delay_mode		    = DELAY_FULL;
				break;
			    case 'h':
				gagt_delay_mode		    = DELAY_SHORT;
				break;
			    case 'n':
				gagt_delay_mode		    = DELAY_OFF;
				break;
			    case 'r':
				gagt_replacement_enabled    = FALSE;
				break;
			    case 'x':
				gagt_abbreviations_enabled  = FALSE;
				break;
			    case 's':
				gagt_extended_status_enabled= TRUE;
				break;
			    case 'l':
				gagt_extended_status_enabled= FALSE;
				break;
			    case 'c':
				gagt_commands_enabled	    = FALSE;
				break;
			    case 'D':
				DEBUG_OUT		    = TRUE;
				break;
			    case '#':
				gagt_clean_exit_test	    = TRUE;
				break;
			    default:
				return FALSE;
			    }
			break;

		    /* Normal AGiliTy options follow. */
		    case 'p':	debug_parse		= TRUE;	break;
		    case 'a':	DEBUG_DISAMBIG		= TRUE;	break;
		    case 'd':	DEBUG_AGT_CMD		= TRUE;	break;
		    case 'x':	DEBUG_EXEC_VERB		= TRUE;	break;
		    case 's':	DEBUG_SMSG		= TRUE;	break;
#ifdef MEM_INFO
		    case 'M':	DEBUG_MEM		= TRUE;	break;
#endif
		    case 'm':	descr_maxmem		= 0;	break;
		    case 't':	BATCH_MODE		= TRUE;	break;
		    case 'c':	make_test		= TRUE;	break;
		    case '1':	irun_mode		= TRUE;	break;
#ifdef OPEN_FILE_AS_TEXT
		    case 'b':	open_as_binary		= TRUE;	break;
#endif

		    case '?':
		    default:
			/* Return FALSE for unknown options arguments. */
			return FALSE;
		    }
	    }

	/* The options found permit us to continue. */
	return TRUE;
}


/*
 * gagt_startup_code()
 * gagt_main()
 *
 * Together, these functions take the place of the original AGiliTy main().
 * The first one is called from glkunix_startup_code(), to parse and
 * generally handle options.  The second is called from glk_main(), and
 * does the real work of running the game.
 */
static int
gagt_startup_code (int argc, char *argv[])
{
	int		argv_index;		/* Argument string index. */

	/*
	 * Before doing anything else, stash argc and argv away for use by
	 * gagt_main() below.
	 */
	gagt_saved_argc = argc;
	gagt_saved_argv = argv;

	/* Make the mandatory call for initialization. */
	set_default_options ();

	/* Handle command line arguments. */
	for (argv_index = 1;
			argv_index < argc && argv[ argv_index ][0] == '-';
			argv_index++)
	    {
		/*
		 * Handle an option string coming after "-".  If the options
		 * parse fails, return FALSE.
		 */
		if (!gagt_parse_option (argv[ argv_index ]))
			return FALSE;
	    }

	/*
	 * Get the name of the game file.  Since we need this in our call
	 * from glk_main, we need to keep it in a module static variable.
	 * If the game file name is omitted, then here we'll set the pointer
	 * to NULL, and complain about it later in main.  Passing the
	 * message string around like this is a nuisance...
	 */
	if (argv_index == argc - 1)
	    {
		gagt_gamefile = argv[ argv_index ];
		gagt_game_message = NULL;
#ifdef GARGLK
		char *s;
		s = strrchr(gagt_gamefile, '\\');
		if (s) garglk_set_story_name(s+1);
		s = strrchr(gagt_gamefile, '/');
		if (s) garglk_set_story_name(s+1);
#endif
	    }
	else
	    {
		gagt_gamefile = NULL;
		if (argv_index < argc - 1)
			gagt_game_message =
				"More than one game file was given"
							" on the command line.";
		else
			gagt_game_message =
				"No game file was given on the command line.";
	    }

	/* All startup options were handled successfully. */
	return TRUE;
}

static void
gagt_main (void)
{
	fc_type		fc;			/* Game file context. */
	assert (gagt_saved_argc != 0 && gagt_saved_argv != NULL);

	/* Ensure AGiliTy internal types have the right sizes. */
	if (sizeof (integer) < 2
			|| sizeof (int32) < 4 || sizeof (uint32) < 4)
	    {
		gagt_fatal ("GLK: Types sized incorrectly,"
					" recompilation is needed");
		gagt_exit ();
	    }

	/*
	 * Initialize the interface.  As it happens, init_interface() is in
	 * our module here (above), and ignores argc and argv, but since the
	 * main() in AGiliTy passes them, we'll do so here, just in case we
	 * ever want to go back to using AGiliTy's main() function.
	 *
	 * init_interface() can fail if there is a problem creating the main
	 * window.  As it doesn't return status, we have to detect this by
	 * checking that gagt_main_window is not NULL.
	 */
	init_interface (gagt_saved_argc, gagt_saved_argv);
	if (gagt_main_window == NULL)
	    {
		gagt_fatal ("GLK: Can't open main window");
		gagt_exit ();
	    }
	glk_window_clear (gagt_main_window);
	glk_set_window (gagt_main_window);
	glk_set_style (style_Normal);

	/* If there's a problem with the game file, complain now. */
	if (gagt_gamefile == NULL)
	    {
		assert (gagt_game_message != NULL);
		if (gagt_status_window != NULL)
			glk_window_close (gagt_status_window, NULL);
		gagt_header_string ("Glk AGiliTy Error\n\n");
		gagt_normal_string (gagt_game_message);
		gagt_normal_char ('\n');
		gagt_exit ();
	    }

	/* If Glk can't do timers, note that delays are not possible. */
	if (glk_gestalt (gestalt_Timer, 0))
		gagt_delays_possible = TRUE;
	else
		gagt_delays_possible = FALSE;

	/*
	 * Create a game file context, and try to ensure it will open
	 * successfully in run_game().
	 */
	fc = init_file_context (gagt_gamefile, fDA1);
	errno = 0;
	if (!gagt_workround_fileexist (fc, fAGX)
			&& !gagt_workround_fileexist (fc, fDA1))
	    {
		/* Report the error, including any details in errno. */
		if (gagt_status_window != NULL)
			glk_window_close (gagt_status_window, NULL);
		gagt_header_string ("Glk AGiliTy Error\n\n");
		gagt_normal_string ("Can't find or open game '");
		gagt_normal_string (gagt_gamefile);
		gagt_normal_char ('\'');
		if (errno != 0)
		    {
			gagt_normal_string (": ");
			gagt_normal_string (strerror (errno));
		    }
		gagt_normal_char ('\n');

		/* Nothing more to be done. */
		gagt_exit ();
	    }

	/*
	 * Run the game interpreter in AGiliTy.  run_game() releases the
	 * file context, so we don't have to, don't want to, and shouldn't.
	 */
	run_game (fc);

	/*
	 * Handle any updated status, and flush all remaining buffered
	 * output; this also frees all malloc'ed memory in the buffers.
	 */
	gagt_status_notify ();
	gagt_output_flush ();

	/*
	 * Free any temporary memory that may have been used by status
	 * line functions.
	 */
	gagt_status_cleanup ();

	/* Close any open transcript, input log, and/or read log. */
	if (gagt_transcript_stream != NULL)
		glk_stream_close (gagt_transcript_stream, NULL);
	if (gagt_inputlog_stream != NULL)
		glk_stream_close (gagt_inputlog_stream, NULL);
	if (gagt_readlog_stream != NULL)
		glk_stream_close (gagt_readlog_stream, NULL);
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the AGiliTy interpreter   */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int		gagt_startup_called	= FALSE;
static int		gagt_main_called	= FALSE;

/*
 * We try to catch calls to exit() from the interpreter, and redirect them
 * to glk_exit().  To help tell these calls from a call to exit() from
 * glk_exit() itself, we need to monitor when interpreter code is running,
 * and when not.
 */
static int		gagt_agility_running	= FALSE;


/*
 * gagt_finalizer()
 *
 * ANSI atexit() handler.  This is the first part of trying to catch and re-
 * direct the calls the core AGiliTy interpreter makes to exit() -- we really
 * want it to call glk_exit(), but it's hard to achieve.  There are three
 * basic approaches possible, and all have drawbacks:
 *
 *   o #define exit to gagt_something, and provide the gagt_something()
 *     function.  This type of macro definition is portable for the most
 *     part, but tramples the code badly, and messes up the build of the
 *     non-interpreter "support" binaries.
 *   o Use ld's --wrap to wrapper exit.  This only works with Linux's linker
 *     and so isn't at all portable.
 *   o Register an exit handler with atexit(), and try to cope in it after
 *     exit() has been called.
 *
 * Here we try the last of these.  The one sticky part of it is that in our
 * exit handler we'll want to call glk_exit(), which will in all likelihood
 * call exit().  And multiple calls to exit() from a program are "undefined".
 *
 * In practice, C runtimes tend to do one of three things: they treat the
 * exit() call from the exit handler as if it was a return; they recurse
 * indefinitely through the hander; or they do something ugly (abort, for
 * example).  The first of these is fine, ideal in fact, and seems to be the
 * Linux and SVR4 behavior.  The second we can avoid with a flag.  The last
 * is the problem case, seen only with SVR3 (and even then, it occurs only
 * on program exit, after everything's cleaned up, and for that matter only
 * on abnormal exit).
 *
 * Note that here we're not expecting to get a call to this routine, and if
 * we do, and interpreter code is still running, it's a sign that we need
 * to take actions we'd hoped not to have to take.
 */
static void
gagt_finalizer (void)
{
	/*
	 * If interpreter code is still active, the core interpreter code
	 * called exit().  Handle cleanup.
	 */
	if (gagt_agility_running)
	    {
		event_t		event;		/* Glk event buffer. */

	 	/*
		 * If we have a main window, try to update status (which
		 * may go to the status window, or to the main window) and
		 * flush any pending buffered output.
		 */
		if (gagt_main_window != NULL)
		    {
			gagt_status_notify ();
			gagt_output_flush ();
		    }

		/*
		 * Clear the flag to avoid recursion, and call glk_exit() to
		 * clean up Glk and terminate.  This is the call that probably
		 * re-calls exit(), and thus prods "undefined" bits of the C
		 * runtime, so we'll make it configurable and overrideable
		 * for problem cases.
		 */
		gagt_agility_running = FALSE;
#ifndef GLK_CLEAN_EXIT
		if (getenv ("GLKAGIL_CLEAN_EXIT") == NULL)
		    {
			glk_exit ();
			return;
		    }
#endif

		/*
		 * We've decided not to take the dangerous route.
		 *
		 * In that case, providing we have a main window, fake a
		 * Glk-like-ish hit-any-key-and-wait message using a simple
		 * string in the main window.  Not great, but usable where
		 * we're forced into bypassing glk_exit().  If we have no
		 * main window, there's no point in doing anything more.
		 */
		if (gagt_main_window != NULL)
		    {
			glk_set_style (style_Alert);
			glk_put_string ("\n\nHit any key to exit.\n");
			glk_request_char_event (gagt_main_window);
			gagt_event_wait (evtype_CharInput, &event);
		    }
	    }
}


/*
 * gagt_exit()
 *
 * Glk_exit() local wrapper.  This is the second part of trying to catch
 * and redirect calls to exit().  Glk_finalizer() above needs to know that
 * we called glk_exit() already from here, so it doesn't try to do it again.
 */
static void
gagt_exit (void)
{
	assert (gagt_agility_running);

	/*
	 * Clear the running flag to neutralize gagt_finalizer(), throw
	 * out any buffered output data, and then call the real glk_exit().
	 */
	gagt_agility_running = FALSE;
	gagt_output_delete ();
	glk_exit ();
}


/*
 * __wrap_exit()
 *
 * Exit() wrapper where a linker does --wrap.  This is the third part of
 * trying to catch and redirect calls to exit().
 *
 * This function is for use only with IFP, and avoids a nasty attempt at
 * reusing a longjmp buffer.   IFP will redirect calls to exit() into
 * glk_exit() as a matter of course.  It also handles atexit(), and we've
 * registered a function with atexit() that calls glk_exit(), and
 * IFP redirects glk_exit() to be an effective return from glk_main().  At
 * that point it calls finalizers.  So without doing something special for
 * IFP, we'll find ourselves calling glk_exit() twice -- once as the IFP
 * redirected exit(), and once from our finalizer.  Two returns from the
 * function glk_main() is a recipe for unpleasantness.
 *
 * As IFP is Linux-only, at present, --wrap will always be available to IFP
 * plugin builds.  So here, we'll wrap exit() before IFP can get to it, and
 * handle it safely.  For non-IFP/non-wrap links, this is just an unused
 * function definition, and can be safely ignored...
 */
void
__wrap_exit (int status)
{
	assert (gagt_agility_running);

	/*
	 * In an IFP plugin, only the core interpreter code could have called
	 * exit() here -- we don't, and IFP redirects glk_exit(), the only
	 * other potential caller of exit().  (It also redirects exit() if
	 * we don't get to it here first.)
	 *
	 * So, if we have a main window, flush it.  This is the same cleanup
	 * as done by the finalizer.
	 */
	if (gagt_main_window != NULL)
	    {
		gagt_status_notify ();
		gagt_output_flush ();
	    }

	/* Clear the running flag, and transform exit() into a glk_exit(). */
	gagt_agility_running = FALSE;
	glk_exit ();
}


#if TARGET_OS_MAC
/* Additional Mac variables. */
static strid_t		gagt_mac_gamefile	= NULL;
static short		gagt_savedVRefNum	= 0;
static long		gagt_savedDirID		= 0;


/*
 * gagt_mac_whenselected()
 * gagt_mac_whenbuiltin()
 * macglk_startup_code()
 *
 * Startup entry points for Mac versions of Glk AGiliTy.  Glk will call
 * macglk_startup_code() for details on what to do when the application
 * is selected.  On selection, an argv[] vector is built, and passed
 * to the normal AGiliTy startup code, after which, Glk will call
 * glk_main().
 */
static Boolean
gagt_mac_whenselected (FSSpec *file, OSType filetype)
{
	static char*		argv[2];
	assert (!gagt_startup_called);
	gagt_startup_called = TRUE;

	/* Set the WD to where the file is, so later fopens work. */
	if (HGetVol (0, &gagt_savedVRefNum, &gagt_savedDirID) != 0)
	    {
		gagt_fatal ("GLK: HGetVol failed");
		return FALSE;
	    }
	if (HSetVol (0, file->vRefNum, file->parID) != 0)
	    {
		gagt_fatal ("GLK: HSetVol failed");
		return FALSE;
	    }

	/* Put a CString version of the PString name into argv[1]. */
	argv[1] = gagt_malloc (file->name[0] + 1);
	BlockMoveData (file->name + 1, argv[1], file->name[0]);
	argv[1][file->name[0]] = '\0';
	argv[2] = NULL;

	return gagt_startup_code (2, argv);
}

static Boolean
gagt_mac_whenbuiltin (void)
{
	/* Not implemented yet. */
	return TRUE;
}

Boolean
macglk_startup_code (macglk_startup_t *data)
{
	static OSType		gagt_mac_gamefile_types[] = { 'AGTS' };

	data->startup_model	 = macglk_model_ChooseOrBuiltIn;
	data->app_creator	 = 'cAGL';
	data->gamefile_types	 = gagt_mac_gamefile_types;
	data->num_gamefile_types = sizeof (gagt_mac_gamefile_types)
					/ sizeof (*gagt_mac_gamefile_types);
	data->savefile_type	 = 'BINA';
	data->datafile_type	 = 0x3F3F3F3F;
	data->gamefile		 = &gagt_mac_gamefile;
	data->when_selected	 = gagt_mac_whenselected;
	data->when_builtin	 = gagt_mac_whenbuiltin;
	/* macglk_setprefs(); */
	return TRUE;
}


#else /* not TARGET_OS_MAC */
/*
 * glkunix_startup_code()
 *
 * Startup entry point for UNIX versions of Glk AGiliTy.  Glk will call
 * glkunix_startup_code() to pass in arguments.  On startup, we call
 * our function to parse arguments and generally set stuff up.
 */
int
glkunix_startup_code (glkunix_startup_t *data)
{
	assert (!gagt_startup_called);
	gagt_startup_called = TRUE;

#ifdef GARGLK
	garglk_set_program_name("Agility 1.1.1");
	garglk_set_program_info(
			"AGiliTy 1.1.1 by Robert Masenten\n"
			"Glk port by Simon Baldwin\n"
	);
#endif

	return gagt_startup_code (data->argc, data->argv);
}
#endif /* TARGET_OS_MAC */


/*
 * glk_main()
 *
 * Main entry point for Glk.  Here, all startup is done, and we call our
 * function to run the game.
 */
void
glk_main (void)
{
	assert (gagt_startup_called && !gagt_main_called);
	gagt_main_called = TRUE;

	/*
	 * Register gagt_finalizer() with atexit() to cleanup on exit.  Note
	 * that this module doesn't expect the atexit() handler to be called
	 * on all forms of exit -- see comments in gagt_finalizer() for more.
	 */
	if (atexit (gagt_finalizer) != 0)
	    {
		gagt_fatal ("GLK: Failed to register finalizer");
		gagt_exit ();
	    }

	/*
	 * If we're testing for a clean exit, deliberately call exit() to see
	 * what happens.  We're hoping for a clean process termination, but
	 * our exit code explores "undefined" ANSI.  If we get something ugly,
	 * like a core dump, we'll want to set GLK[AGIL]_CLEAN_EXIT.
	 */
	if (gagt_clean_exit_test)
	    {
		gagt_agility_running = TRUE;
		exit (0);
	    }

	/*
	 * The final part of trapping exit().  Set the running flag, and call
	 * call the interpreter main function.  Clear the flag when the main
	 * function returns.
	 */
	gagt_agility_running = TRUE;
	gagt_main ();
	gagt_agility_running = FALSE;
}

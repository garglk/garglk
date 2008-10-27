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
 * Glk interface for Level9
 * ------------------------
 *
 * This module contains the the Glk porting layer for the Level9
 * interpreter.  It defines the Glk arguments list structure, the
 * entry points for the Glk library framework to use, and all
 * platform-abstracted I/O to link to Glk's I/O.
 *
 * The following items are omitted from this Glk port:
 *
 *  o Glk tries to assert control over _all_ file I/O.  It's just too
 *    disruptive to add it to existing code, so for now, the Level9
 *    interpreter is still dependent on stdio and the like.
 *
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>

#include "level9.h"

#include "glk.h"

#if TARGET_OS_MAC
#	include "macglk_startup.h"
#else
#	include "glkstart.h"
#endif


/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous other stuff                        */
/*---------------------------------------------------------------------*/

/* Glk Level9 port version number. */
static const glui32	GLN_PORT_VERSION		= 0x00010306;

/*
 * We use the main Glk window for pretty much everything.  Level9 doesn't
 * generate a status line, but we have a "status" window to use as a
 * banner at the top of the window, and as a handy place to hang an Xglk
 * workround.
 */
static winid_t		gln_main_window			= NULL;
static winid_t		gln_status_window		= NULL;

/*
 * Transcript stream and input log.  These are NULL if there is no
 * current collection of these strings.
 */
static strid_t		gln_transcript_stream		= NULL;
static strid_t		gln_inputlog_stream		= NULL;

/* Input read log stream, for reading back an input log. */
static strid_t		gln_readlog_stream		= NULL;

/* Options that may be turned off by command line flags. */
static int		gln_intercept_enabled		= TRUE;
static int		gln_prompt_enabled		= TRUE;
static int		gln_loopcheck_enabled		= TRUE;
static int		gln_abbreviations_enabled	= TRUE;
static int		gln_commands_enabled		= TRUE;

/* Note indicating whether the Glk library has timers, or not. */
static int		gln_timeouts_possible		= TRUE;

/* Reason for stopping the game, used to detect restarts and ^C exits. */
static enum		{ STOP_NONE, STOP_FORCE, STOP_RESTART, STOP_EXIT }
			gln_stop_reason			= STOP_NONE;

/* Level9 standard input prompt string. */
static const char	*GLN_INPUT_PROMPT		= "> ";

/* Internal interpreter symbols used for our own deviant purposes. */
extern void		save (void);
extern void		restore (void);
extern L9BOOL		Cheating;

/* Forward declaration of event wait functions. */
static void		gln_event_wait (glui32 wait_type, event_t *event);
static void		gln_event_wait_2 (glui32 wait_type_1,
					glui32 wait_type_2, event_t *event);

/* Forward declaration of the confirmation function. */
static int		gln_confirm (const char *prompt);

/* so we defeat the CR after os_input, because Glk already adds it */
static int		gln_kill_newline = 0;

/*---------------------------------------------------------------------*/
/*  Glk arguments list                                                 */
/*---------------------------------------------------------------------*/

#if !TARGET_OS_MAC
glkunix_argumentlist_t glkunix_arguments[] = {
    { (char *) "-nc", glkunix_arg_NoValue,
	(char *) "-nc        No local handling for Glk special commands" },
    { (char *) "-na", glkunix_arg_NoValue,
	(char *) "-na        Turn off abbreviation expansions" },
    { (char *) "-ni", glkunix_arg_NoValue,
	(char *) "-ni        No local handling for 'quit', 'restart'"
						" 'save', and 'restore'" },
    { (char *) "-np", glkunix_arg_NoValue,
	(char *) "-np        Turn off additional interpreter prompt" },
    { (char *) "-nl", glkunix_arg_NoValue,
	(char *) "-nl        Turn off infinite loop detection" },
    { (char *) "", glkunix_arg_ValueCanFollow,
	(char *) "filename   game to run" },
{ NULL, glkunix_arg_End, NULL }
};
#endif


/*---------------------------------------------------------------------*/
/*  Glk port utility functions                                         */
/*---------------------------------------------------------------------*/

/*
 * gln_fatal()
 *
 * Fatal error handler.  The function returns, expecting the caller to
 * abort() or otherwise handle the error.
 */
void
gln_fatal (const char *string)
{
	/*
	 * If the failure happens too early for us to have a window, print
	 * the message to stderr.
	 */
	if (gln_main_window == NULL)
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
	glk_cancel_line_event (gln_main_window, NULL);
	glk_cancel_char_event (gln_main_window);

	/* Print a message indicating the error, and exit. */
	glk_set_window (gln_main_window);
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
 * gln_malloc()
 *
 * Non-failing malloc and realloc; call gln_fatal and exit if memory
 * allocation fails.
 */
static void *
gln_malloc (size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Malloc, and call gln_fatal if the malloc fails. */
	pointer = malloc (size);
	if (pointer == NULL)
	    {
		gln_fatal ("GLK: Out of system memory");
		glk_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}

static void *
gln_realloc (void *ptr, size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Realloc, and call gln_fatal() if the realloc fails. */
	pointer = realloc (ptr, size);
	if (pointer == NULL)
	    {
		gln_fatal ("GLK: Out of system memory");
		glk_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}


/*
 * gln_strncasecmp()
 * gln_strcasecmp()
 *
 * Strncasecmp and strcasecmp are not ANSI functions, so here are local
 * definitions to do the same jobs.
 *
 * They're global here so that the core interpreter can use them; otherwise
 * it tries to use the non-ANSI str[n]icmp() functions.
 */
int
gln_strncasecmp (const char *s1, const char *s2, size_t n)
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
int
gln_strcasecmp (const char *s1, const char *s2)
{
	size_t		s1len, s2len;		/* String lengths. */
	int		result;			/* Result of strncasecmp. */

	/* Note the string lengths. */
	s1len = strlen (s1);
	s2len = strlen (s2);

	/* Compare first to shortest length, return any definitive result. */
	result = gln_strncasecmp (s1, s2, (s1len < s2len) ? s1len : s2len);
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


/*---------------------------------------------------------------------*/
/*  Glk port stub graphics functions                                   */
/*---------------------------------------------------------------------*/

#ifdef GARGLK
#include "glkgfx.c"
#else

void gln_update_graphics() {}

/*
 * Glk libraries aren't really geared up for the type of line drawing and
 * point-plotting graphics that the Level9 games contain, so for now,
 * these are stub functions.
 */
void	os_graphics (int mode)					{}
void	os_cleargraphics (void)					{}
void	os_setcolour (int colour, int index)			{}
void	os_drawline (int x1, int y1, int x2, int y2,
				int colour1, int colour2)	{}
void	os_fill (int x, int y, int colour1, int colour2)	{}
void	os_show_bitmap(int pic, int x, int y)			{}

#endif

/*---------------------------------------------------------------------*/
/*  Glk port infinite loop detection functions                         */
/*---------------------------------------------------------------------*/

/* Short timeout to wait purely in order to get the display updated. */
static const glui32	GLN_WATCHDOG_FIXUP		= 50;

/*
 * Timestamp of the last watchdog tick call, and timeout.  This is used to
 * monitor the elapsed time since the interpreter made an I/O call.  If it
 * remains silent for long enough, set by the timeout, we'll offer the
 * option to end the game.  A timeout of zero disables the watchdog.
 */
static time_t		gln_watchdog_monitor		= 0;
static int		gln_watchdog_timeout_secs	= 0;


/*
 * gln_watchdog_start()
 * gln_watchdog_stop()
 *
 * Start and stop watchdog monitoring.
 */
static void
gln_watchdog_start (int timeout)
{
	assert (timeout >= 0);

	gln_watchdog_timeout_secs = timeout;
	gln_watchdog_monitor = time (NULL);
}

static void
gln_watchdog_stop (void)
{
	gln_watchdog_timeout_secs = 0;
}


/*
 * gln_watchdog_tick()
 *
 * Set the watchdog timestamp to the current system time.
 *
 * This function should be called just before almost every os_* function
 * returns to the interpreter, as a means of timing how long the interp-
 * reter dwells in running game code.
 */
static void
gln_watchdog_tick (void)
{
	gln_watchdog_monitor = time (NULL);
}


/*
 * gln_watchdog_timeout()
 *
 * Check to see if too much time has elapsed since the last tick.  If it
 * has, offer the option to stop the game, and if accepted, return TRUE.
 * Otherwise, if no timeout, or if the watchdog is disabled, return FALSE.
 */
static int
gln_watchdog_timeout (void)
{
	time_t		current_time;		/* Current system time. */
	double		delta_time;		/* Time difference. */

	/*
	 * If loop detection is off, or if the timeout is set to zero,
	 * do nothing.
	 */
	if (!gln_loopcheck_enabled
			|| gln_watchdog_timeout_secs == 0)
		return FALSE;

	/* Find elapsed time in seconds since the last watchdog. */
	current_time = time (NULL);
	delta_time   = difftime (current_time, gln_watchdog_monitor);

	/* If too much time has passed, offer to end the game. */
	if (delta_time >= gln_watchdog_timeout_secs)
	    {
		/* It looks like the game is in an endless loop. */
		if (gln_confirm ("\nThe game may be in an infinite loop."
				"  Do you want to stop it? [Y or N] "))
		    {
			/* Return TRUE -- timed out, and stop requested. */
			gln_watchdog_monitor = time (NULL);
			return TRUE;
		    }

		/*
		 * If we have timers, set a really short timeout and let it
		 * expire.  This is to force a display update with the
		 * response of the confirm -- without this, we may not get
		 * a screen update for a while since at this point the
		 * game isn't, by definition, doing any input or output.
		 * If we don't have timers, no biggie.
		 */
		if (gln_timeouts_possible)
		    {
			event_t		event;	/* Glk event buffer. */

			glk_request_timer_events (GLN_WATCHDOG_FIXUP);
			gln_event_wait (evtype_Timer, &event);
			glk_request_timer_events (0);
		    }

		/* Reset the monitor and return FALSE -- stop rejected. */
		gln_watchdog_monitor = time (NULL);
		return FALSE;
	    }

	/* No timeout yet. */
	return FALSE;
}


/*---------------------------------------------------------------------*/
/*  Glk port status line functions                                     */
/*---------------------------------------------------------------------*/

/*
 * gln_status_redraw()
 *
 * Level9 doesn't produce a status line, so we've nothing to do here except
 * print a default piece of text describing the interpreter.  However, we
 * need this spot to hang the Xglk workround on.  This function should be
 * called on the appropriate Glk window resize and arrange events.
 */
static void
gln_status_redraw (void)
{
	/* If there is a status window, update it. */
	if (gln_status_window != NULL)
	    {
		winid_t		parent;		/* Status window parent. */
		strid_t		status_stream;	/* Status window stream. */

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
		parent = glk_window_get_parent (gln_status_window);
		glk_window_set_arrangement (parent,
					winmethod_Above|winmethod_Fixed,
		 			1, NULL);

		/* ...now update the status window. */
		glk_window_clear (gln_status_window);
		status_stream = glk_window_get_stream (gln_status_window);
		glk_put_string_stream (status_stream, "Glk Level9 version 4.0");
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk port output functions                                          */
/*---------------------------------------------------------------------*/

/*
 * Output buffer.  We receive characters one at a time, and it's a bit
 * more efficient for everyone if we buffer them, and output a complete
 * string on a flush call.
 */
static const int	GLN_BUFFER_INCREMENT		= 1024;
static char		*gln_output_buffer		= NULL;
static int		gln_output_size			= 0;
static int		gln_output_length		= 0;

/*
 * Output activity flag.  Set when os_printchar() is called, and queried
 * periodically by os_readchar().  Helps os_readchar() judge whether it must
 * request input, or when it's being used as a crude scroll control.
 */
static int		gln_output_activity		= FALSE;

/*
 * Flag to indicate if the last buffer flushed looked like it ended in a
 * "> " prompt.  Some later games switch to this mode after a while, and
 * it's nice not to duplicate this prompt with our own.
 */
static int		gln_output_prompt		= FALSE;


/*
 * gln_output_notify()
 *
 * Register recent text output from the interpreter.  This function is
 * called by os_printchar().
 */
static void
gln_output_notify (void)
{
	/* Set the output activity flag. */
	gln_output_activity = TRUE;
}


/*
 * gln_recent_output()
 *
 * Return TRUE if the interpreter has recently output text, FALSE otherwise.
 * Clears the flag, so that more output text is required before the next
 * call returns TRUE.
 */
static int
gln_recent_output (void)
{
	int		result;				/* Return value. */

	/* Save the current flag value, and reset the main flag. */
	result = gln_output_activity;
	gln_output_activity = FALSE;

	/* Return the old value. */
	return result;
}


/*
 * gln_game_prompted()
 *
 * Return TRUE if the last game output appears to have been a "> " prompt.
 * Once called, the flag is reset to FALSE, and requires more game output
 * to set it again.
 */
static int
gln_game_prompted (void)
{
	int		result;				/* Return value. */

	/* Save the current flag value, and reset the main flag. */
	result = gln_output_prompt;
	gln_output_prompt = FALSE;

	/* Return the old value. */
	return result;
}


/*
 * gln_detect_game_prompt()
 *
 * See if the last non-newline-terminated line in the output buffer seems
 * to be a prompt, and set the game prompted flag if it does, otherwise
 * clear it.
 */
static void
gln_detect_game_prompt (void)
{
	int		index;			/* Output buffer iterator. */

	/* Begin with a clear prompt flag. */
	gln_output_prompt = FALSE;

	/* Search across any last unterminated buffered line. */
	for (index = gln_output_length - 1;
		index >= 0 && gln_output_buffer[ index ] != '\n';
		index--)
	    {
		/* Looks like a prompt if a non-space character found. */
		if (gln_output_buffer[ index ] != ' ')
		    {
			gln_output_prompt = TRUE;
			break;
		    }
	    }
}


/*
 * gln_output_delete()
 *
 * Delete all buffered output text.  Free all malloc'ed buffer memory, and
 * return the buffer variables to their initial values.
 */
static void
gln_output_delete (void)
{
	/* Clear and free the buffer of current contents. */
	if (gln_output_buffer != NULL)
		free (gln_output_buffer);
	gln_output_buffer = NULL;
	gln_output_size   = 0;
	gln_output_length = 0;
}


/*
 * gln_output_flush()
 *
 * Flush any buffered output text to the Glk main window, and clear the
 * buffer.  Check in passing for game prompts that duplicate our's.
 */
static void
gln_output_flush (void)
{
	assert (glk_stream_get_current () != NULL);

	/* Do nothing if the buffer is currently empty. */
	if (gln_output_length > 0)
	    {
		/* See if the game issued a standard prompt. */
		gln_detect_game_prompt ();

		/*
		 * Print the buffer to the stream for the main window, in
		 * game output style.
		 */
		glk_set_style (style_Normal);
		glk_put_buffer (gln_output_buffer, gln_output_length);

		/* Clear and free the buffer of current contents. */
		gln_output_delete ();
	    }
}


/*
 * os_printchar()
 *
 * Buffer a character for eventual printing to the main window.
 */
void
os_printchar (char c)
{
	assert (gln_output_length <= gln_output_size);

	/* Note that the game created some output. */
	gln_output_notify ();

	/* Grow the output buffer if necessary. */
	if (gln_output_length == gln_output_size)
	    {
		gln_output_size += GLN_BUFFER_INCREMENT;
		gln_output_buffer = gln_realloc (gln_output_buffer,
							gln_output_size);
	    }

	/* Handle return as a newline. */
	if (c == '\r')
	    {
		if (!gln_kill_newline)
		gln_output_buffer[ gln_output_length++ ] = '\n';
		return;
	    }

	/* Add the character to the buffer. */
	gln_output_buffer[ gln_output_length++ ] = c;

	gln_kill_newline = 0;
}


/*
 * gln_standout_string()
 * gln_standout_char()
 *
 * Print a string in a style that stands out from game text.
 */
static void
gln_standout_string (const char *message)
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
gln_standout_char (char c)
{
	/* Print the character, in a message style. */
	glk_set_style (style_Emphasized);
	glk_put_char (c);
	glk_set_style (style_Normal);
}


/*
 * gln_normal_string()
 * gln_normal_char()
 *
 * Print a string in normal text style.
 */
static void
gln_normal_string (const char *message)
{
	assert (message != NULL);

	/* Print the message, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_string ((char *) message);
}

static void
gln_normal_char (char c)
{
	/* Print the character, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_char (c);
}


/*
 * gln_header_string()
 * gln_banner_string()
 *
 * Print text messages for the banner at the start of a game run.
 */
static void
gln_header_string (const char *banner)
{
	assert (banner != NULL);

	/* Print the string in a header style. */
	glk_set_style (style_Header);
	glk_put_string ((char *) banner);
	glk_set_style (style_Normal);
}

static void
gln_banner_string (const char *banner)
{
	assert (banner != NULL);

	/* Print the banner in a subheader style. */
	glk_set_style (style_Subheader);
	glk_put_string ((char *) banner);
	glk_set_style (style_Normal);
}


/*
 * ms_flush()
 *
 * Handle a core interpreter call to flush the output buffer.  Because
 * Glk only flushes its buffers and displays text on glk_select(), we
 * can ignore these calls as long as we call glk_output_flush() when
 * reading line or character input.
 */
void os_flush (void)
{
}


/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/* Valid command control values. */
static const char	*GLN_COMMAND_ON			= "on";
static const char	*GLN_COMMAND_OFF		= "off";


/*
 * gln_command_script()
 *
 * Turn game output scripting (logging) on and off.
 */
static void
gln_command_script (const char *argument)
{
	assert (argument != NULL);

	/* Set up a transcript according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a transcript is already active. */
		if (gln_transcript_stream != NULL)
		    {
			gln_normal_string ("Glk transcript is already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a transcript. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_Transcript | fileusage_TextMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gln_standout_string ("Glk transcript failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gln_transcript_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gln_transcript_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gln_standout_string ("Glk transcript failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Set the new transcript stream as the main echo stream. */
		glk_window_set_echo_stream (gln_main_window,
						gln_transcript_stream);

		/* Confirm action. */
		gln_normal_string ("Glk transcript is now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* If a transcript is active, close it. */
		if (gln_transcript_stream != NULL)
		    {
			glk_stream_close (gln_transcript_stream, NULL);
			gln_transcript_stream = NULL;

			/* Clear the main echo stream. */
			glk_window_set_echo_stream (gln_main_window, NULL);

			/* Confirm action. */
			gln_normal_string ("Glk transcript is now ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
		    }
		else
		    {
			/* Note that scripts are already disabled. */
			gln_normal_string ("Glk transcript is already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * transcript mode.
		 */
		gln_normal_string ("Glk transcript is ");
		if (gln_transcript_stream != NULL)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk transcript can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}



/*
 * gln_command_inputlog()
 *
 * Turn game input logging on and off.
 */
static void
gln_command_inputlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up an input log according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if an input log is already active. */
		if (gln_inputlog_stream != NULL)
		    {
			gln_normal_string ("Glk input logging is already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for an input log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gln_standout_string ("Glk input logging failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gln_inputlog_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gln_inputlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gln_standout_string ("Glk input logging failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gln_normal_string ("Glk input logging is now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* If an input log is active, close it. */
		if (gln_inputlog_stream != NULL)
		    {
			glk_stream_close (gln_inputlog_stream, NULL);
			gln_inputlog_stream = NULL;

			/* Confirm action. */
			gln_normal_string ("Glk input log is now ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current input log. */
			gln_normal_string ("Glk input logging is already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * input logging mode.
		 */
		gln_normal_string ("Glk input logging is ");
		if (gln_inputlog_stream != NULL)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk input logging can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/*
 * gln_command_readlog()
 *
 * Set the game input log, to read input from a file.
 */
static void
gln_command_readlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up a read log according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a read log is already active. */
		if (gln_readlog_stream != NULL)
		    {
			gln_normal_string ("Glk read log is already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a read log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_Read, 0);
		if (fileref == NULL)
		    {
			gln_standout_string ("Glk read log failed.\n");
			return;
		    }

		/*
		 * Reject the file reference if we're expecting to read
		 * from it, and the referenced file doesn't exist.
		 */
		if (!glk_fileref_does_file_exist (fileref))
		    {
			glk_fileref_destroy (fileref);
			gln_standout_string ("Glk read log failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gln_readlog_stream = glk_stream_open_file
					(fileref, filemode_Read, 0);
		if (gln_readlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gln_standout_string ("Glk read log failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gln_normal_string ("Glk read log is now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* If a read log is active, close it. */
		if (gln_readlog_stream != NULL)
		    {
			glk_stream_close (gln_readlog_stream, NULL);
			gln_readlog_stream = NULL;

			/* Confirm action. */
			gln_normal_string ("Glk read log is now ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current read log. */
			gln_normal_string ("Glk read log is already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * read logging mode.
		 */
		gln_normal_string ("Glk read log is ");
		if (gln_readlog_stream != NULL)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk read log can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/*
 * gln_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gln_command_abbreviations (const char *argument)
{
	assert (argument != NULL);

	/* Set up abbreviation expansions according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		/* See if expansions are already on. */
		if (gln_abbreviations_enabled)
		    {
			gln_normal_string ("Glk abbreviation expansions"
					" are already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions on. */
		gln_abbreviations_enabled = TRUE;
		gln_normal_string ("Glk abbreviation expansions are now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* See if expansions are already off. */
		if (!gln_abbreviations_enabled)
		    {
			gln_normal_string ("Glk abbreviation expansions"
					" are already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions off. */
		gln_abbreviations_enabled = FALSE;
		gln_normal_string ("Glk abbreviation expansions are now ");
		gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * expansion mode.
		 */
		gln_normal_string ("Glk abbreviation expansions are ");
		if (gln_abbreviations_enabled)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk abbreviation expansions can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/*
 * gln_command_print_version_number()
 * gln_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gln_command_print_version_number (glui32 version)
{
	char		buffer[64];		/* Output buffer string. */

	/* Print out the three version number component parts. */
	sprintf (buffer, "%lu.%lu.%lu",
			(version & 0xFFFF0000) >> 16,
			(version & 0x0000FF00) >>  8,
			(version & 0x000000FF)      );
	gln_normal_string (buffer);
}
static void
gln_command_version (const char *argument)
{
	glui32		version;		/* Glk lib version number. */

	/* Get the Glk library version number. */
	version = glk_gestalt (gestalt_Version, 0);

	/* Print the Glk library and port version numbers. */
	gln_normal_string ("The Glk library version is ");
	gln_command_print_version_number (version);
	gln_normal_string (".\n");
	gln_normal_string ("This is version ");
	gln_command_print_version_number (GLN_PORT_VERSION);
	gln_normal_string (" of the Glk Level9 port.\n");
}


/*
 * gln_command_loopchecks()
 *
 * Turn loop checking (for game infinite loops) on and off.
 */
static void
gln_command_loopchecks (const char *argument)
{
	assert (argument != NULL);

	/* Set up loopcheck according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		/* See if loop checks are already on. */
		if (gln_loopcheck_enabled)
		    {
			gln_normal_string ("Glk loop detection"
					" is already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned loop checking on. */
		gln_loopcheck_enabled = TRUE;
		gln_normal_string ("Glk loop detection is now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* See if loop checks are already off. */
		if (!gln_loopcheck_enabled)
		    {
			gln_normal_string ("Glk loop detection"
					" is already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned loop checks off. */
		gln_loopcheck_enabled = FALSE;
		gln_normal_string ("Glk loop detection is now ");
		gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * loop check mode.
		 */
		gln_normal_string ("Glk loop detection is ");
		if (gln_loopcheck_enabled)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk loop detection can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/*
 * gln_command_locals()
 *
 * Turn local interpretation of "quit" etc. on and off.
 */
static void
gln_command_locals (const char *argument)
{
	assert (argument != NULL);

	/* Set up local commands according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		/* See if local commands are already on. */
		if (gln_intercept_enabled)
		    {
			gln_normal_string ("Glk local commands"
					" are already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned local commands on. */
		gln_intercept_enabled = TRUE;
		gln_normal_string ("Glk local commands are now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* See if local commands are already off. */
		if (!gln_intercept_enabled)
		    {
			gln_normal_string ("Glk local commands"
					" are already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned local commands off. */
		gln_intercept_enabled = FALSE;
		gln_normal_string ("Glk local commands are now ");
		gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * local command mode.
		 */
		gln_normal_string ("Glk local commands are ");
		if (gln_intercept_enabled)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk local commands can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/*
 * gln_command_prompts()
 *
 * Turn the extra "> " prompt output on and off.
 */
static void
gln_command_prompts (const char *argument)
{
	assert (argument != NULL);

	/* Set up prompt according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		/* See if prompt is already on. */
		if (gln_prompt_enabled)
		    {
			gln_normal_string ("Glk extra prompts are already ");
			gln_normal_string (GLN_COMMAND_ON);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned prompt on. */
		gln_prompt_enabled = TRUE;
		gln_normal_string ("Glk extra prompts are now ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");

		/* Check for a game prompt to clear the flag. */
		gln_game_prompted ();
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* See if prompt is already off. */
		if (!gln_prompt_enabled)
		    {
			gln_normal_string ("Glk extra prompts are already ");
			gln_normal_string (GLN_COMMAND_OFF);
			gln_normal_string (".\n");
			return;
		    }

		/* The user has turned prompt off. */
		gln_prompt_enabled = FALSE;
		gln_normal_string ("Glk extra prompts are now ");
		gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * extra prompt mode.
		 */
		gln_normal_string ("Glk extra prompts are ");
		if (gln_prompt_enabled)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk extra prompts can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/*
 * gln_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gln_command_commands (const char *argument)
{
	assert (argument != NULL);

	/* Set up command handling according to the argument given. */
	if (!gln_strcasecmp (argument, GLN_COMMAND_ON))
	    {
		/* Commands must already be on. */
		gln_normal_string ("Glk commands are already ");
		gln_normal_string (GLN_COMMAND_ON);
		gln_normal_string (".\n");
	    }
	else if (!gln_strcasecmp (argument, GLN_COMMAND_OFF))
	    {
		/* The user has turned commands off. */
		gln_commands_enabled = FALSE;
		gln_normal_string ("Glk commands are now ");
		gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * command mode.
		 */
		gln_normal_string ("Glk commands are ");
		if (gln_commands_enabled)
			gln_normal_string (GLN_COMMAND_ON);
		else
			gln_normal_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gln_normal_string ("Glk commands can be ");
		gln_standout_string (GLN_COMMAND_ON);
		gln_normal_string (", or ");
		gln_standout_string (GLN_COMMAND_OFF);
		gln_normal_string (".\n");
	    }
}


/* Escape introducer string, and special intercepted commands. */
static const char	*GLN_COMMAND_ESCAPE		= "glk";
static const char	*GLN_COMMAND_QUIT		= "quit";
static const char	*GLN_COMMAND_RESTART		= "restart";
static const char	*GLN_COMMAND_SAVE		= "save";
static const char	*GLN_COMMAND_RESTORE		= "restore";
static const char	*GLN_COMMAND_LOAD		= "load";

/* Small table of Glk subcommands and handler functions. */
struct gln_command {
	const char	*command;		/* Glk subcommand. */
	void		(*handler) (const char *argument);
						/* Subcommand handler. */
	const int	takes_argument;		/* Argument flag. */
};
typedef const struct gln_command*	gln_commandref_t;
static const struct gln_command		GLN_COMMAND_TABLE[] = {
	{ "script",		gln_command_script,		TRUE  },
	{ "inputlog",		gln_command_inputlog,		TRUE  },
	{ "readlog",		gln_command_readlog,		TRUE  },
	{ "abbreviations",	gln_command_abbreviations,	TRUE  },
	{ "loopchecks",		gln_command_loopchecks,		TRUE  },
	{ "locals",		gln_command_locals,		TRUE  },
	{ "prompts",		gln_command_prompts,		TRUE  },
	{ "version",		gln_command_version,		FALSE },
	{ "commands",		gln_command_commands,		TRUE  },
	{ NULL,			NULL,				FALSE }
};

/* List of whitespace command-argument separator characters. */
static const char	*GLN_COMMAND_WHITESPACE		= "\t ";


/*
 * gln_command_dispatch()
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
gln_command_dispatch (const char *command, const char *argument)
{
	gln_commandref_t	entry;		/* Table search entry. */
	gln_commandref_t	matched;	/* Matched table entry. */
	int			matches;	/* Count of command matches. */
	assert (command != NULL && argument != NULL);

	/*
	 * Search for the first unambiguous table command string matching
	 * the command passed in.
	 */
	matches = 0;
	matched = NULL;
	for (entry = GLN_COMMAND_TABLE;
				entry->command != NULL; entry++)
	    {
		if (!gln_strncasecmp (command,
					entry->command, strlen (command)))
		    {
			matches++;
			matched = entry;
		    }
	    }

	/* If the match was unambiguous, call the command handler. */
	if (matches == 1)
	    {
		gln_normal_char ('\n');
		(matched->handler) (argument);

		/* Issue a brief warning if an argument was ignored. */
		if (!matched->takes_argument && strlen (argument) > 0)
		    {
			gln_normal_string ("[The ");
			gln_standout_string (matched->command);
			gln_normal_string (" command ignores arguments.]\n");
		    }
	    }

	/* Return the count of matching table entries. */
	return matches;
}


/*
 * gln_command_usage()
 *
 * On an empty, invalid, or ambiguous command, print out a list of valid
 * commands and perhaps some Glk status information.
 */
static void
gln_command_usage (const char *command, int is_ambiguous)
{
	gln_commandref_t	entry;	/* Command table iteration entry. */
	assert (command != NULL);

	/* Print a blank line separator. */
	gln_normal_char ('\n');

	/* If the command isn't empty, indicate ambiguous or invalid. */
	if (strlen (command) > 0)
	    {
		gln_normal_string ("The Glk command ");
		gln_standout_string (command);
		if (is_ambiguous)
			gln_normal_string (" is ambiguous.\n");
		else
			gln_normal_string (" is not valid.\n");
	    }

	/* Print out a list of valid commands. */
	gln_normal_string ("Glk commands are");
	for (entry = GLN_COMMAND_TABLE; entry->command != NULL; entry++)
	    {
		gln_commandref_t	next;	/* Next command table entry. */

		next = entry + 1;
		gln_normal_string (next->command != NULL ? " " : " and ");
		gln_standout_string (entry->command);
		gln_normal_string (next->command != NULL ? "," : ".\n");
	    }

	/* Write a note about abbreviating commands. */
	gln_normal_string ("Glk commands may be abbreviated, as long as");
	gln_normal_string (" the abbreviations are unambiguous.\n");

	/*
	 * If no command was given, call each command handler function with
	 * an empty argument to prompt each to report the current setting.
	 */
	if (strlen (command) == 0)
	    {
		gln_normal_char ('\n');
		for (entry = GLN_COMMAND_TABLE;
					entry->command != NULL; entry++)
			(entry->handler) ("");
	    }
}


/*
 * gln_skip_characters()
 *
 * Helper function for command escapes.  Skips over either whitespace or
 * non-whitespace in string, and returns the revised string pointer.
 */
static char *
gln_skip_characters (char *string, int skip_whitespace)
{
	char	*result;			/* Return string pointer. */
	assert (string != NULL);

	/* Skip over leading characters of the specified type. */
	for (result = string; *result != '\0'; result++)
	    {
		int	is_whitespace;		/* Whitespace flag. */

		/* Break if encountering a character not the required type. */
		is_whitespace =
			(strchr (GLN_COMMAND_WHITESPACE, *result) != NULL);
		if ((skip_whitespace && !is_whitespace)
				|| (!skip_whitespace && is_whitespace))
			break;
	    }

	/* Return the revised pointer. */
	return result;
}


/*
 * gln_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 */
static int
gln_command_escape (char *string)
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
	temporary = gln_skip_characters (string, TRUE);
	if (gln_strncasecmp (temporary, GLN_COMMAND_ESCAPE,
					strlen (GLN_COMMAND_ESCAPE)))
		return FALSE;

	/* Take a copy of the string, without any leading space. */
	string_copy = gln_malloc (strlen (temporary) + 1);
	strcpy (string_copy, temporary);

	/* Find the subcommand; the word after the introducer. */
	command = gln_skip_characters (string_copy
					+ strlen (GLN_COMMAND_ESCAPE), TRUE);

	/* Skip over command word, be sure it terminates with NUL. */
	temporary = gln_skip_characters (command, FALSE);
	if (*temporary != '\0')
	    {
		*temporary = '\0';
		temporary++;
	    }

	/* Now find any argument data for the command. */
	argument = gln_skip_characters (temporary, TRUE);

	/* Ensure that argument data also terminates with a NUL. */
	temporary = gln_skip_characters (argument, FALSE);
	*temporary = '\0';

	/*
	 * Try to handle the command and argument as a Glk subcommand.  If
	 * it doesn't run unambiguously, print command usage.
	 */
	matches = gln_command_dispatch (command, argument);
	if (matches != 1)
	    {
		if (matches == 0)
			gln_command_usage (command, FALSE);
		else
			gln_command_usage (command, TRUE);
	    }

	/* Done with the copy of the string. */
	free (string_copy);

	/* Return TRUE to indicate string contained a Glk command. */
	return TRUE;
}


/*
 * gln_command_intercept()
 *
 * The Level 9 games handle the commands "quit" and "restart" oddly, and
 * somewhat similarly.  Both prompt "Press SPACE to play again", and
 * then ignore all characters except space.  This makes it especially
 * hard to exit from a game without killing the interpreter process.
 * It also handles "restore" via an odd security mechanism which has no
 * real place here (the base Level9 interpreter sidesteps this with its
 * "#restore" command), and has some bugs in "save".
 *
 * To try to improve these, here we'll catch and special case the input
 * lines "quit", "save", "restore", and "restart".  "Load" is a synonym
 * for "restore".
 *
 * On "quit" or "restart", the function sets the interpreter stop reason
 * code, stops the current game run.  On "save" or "restore" it calls the
 * appropriate internal interpreter function.
 *
 * The return value is TRUE if an intercepted command was found, otherwise
 * FALSE.
 */
static int
gln_command_intercept (char *string)
{
	char	*first, *trailing;	/* Start and end of the first word */
	char	*end;			/* Skipped spaces after first word */
	assert (string != NULL);

	/*
	 * Find the first significant character in string, and the space
	 * or NUL after the first word.
	 */
	first    = gln_skip_characters (string, TRUE);
	trailing = gln_skip_characters (first, FALSE);

	/*
	 * Find the first character, or NUL, following the whitespace
	 * after the first word.
	 */
	end = gln_skip_characters (trailing, TRUE);

	/* Forget it if string isn't a single word only. */
	if (strlen (end) != 0)
		return FALSE;

	/* If this command was "quit", confirm, then call StopGame(). */
	if (!gln_strncasecmp (first, GLN_COMMAND_QUIT,
						strlen (GLN_COMMAND_QUIT))
			&& first + strlen (GLN_COMMAND_QUIT) == trailing)
	    {
		/* Confirm quit just as a Level 9 game does. */
		if (gln_confirm ("\nDo you really want to stop? [Y or N] "))
		    {
			gln_stop_reason = STOP_EXIT;
			StopGame ();
		    }
		return TRUE;
	    }

	/* If this command was "restart", confirm, then call StopGame(). */
	if (!gln_strncasecmp (first, GLN_COMMAND_RESTART,
						strlen (GLN_COMMAND_RESTART))
			&& first + strlen (GLN_COMMAND_RESTART) == trailing)
	    {
		/* Confirm restart somewhat as a Level 9 game does. */
		if (gln_confirm ("\nDo you really want to restart? [Y or N] "))
		    {
			gln_stop_reason = STOP_RESTART;
			StopGame ();
		    }
		return TRUE;
	    }

	/* If this command was "save", simply call save(). */
	if (!gln_strncasecmp (first, GLN_COMMAND_SAVE,
						strlen (GLN_COMMAND_SAVE))
			&& first + strlen (GLN_COMMAND_SAVE) == trailing)
	    {
		/* Print a message and call the level9 internal save. */
		gln_standout_string ("\nSaving using interpreter\n\n");
		save ();
		return TRUE;
	    }

	/* If this command was "restore" or "load", call restore(). */
	if ((!gln_strncasecmp (first, GLN_COMMAND_RESTORE,
						strlen (GLN_COMMAND_RESTORE))
			&& first + strlen (GLN_COMMAND_RESTORE) == trailing)
		|| (!gln_strncasecmp (first, GLN_COMMAND_LOAD,
						strlen (GLN_COMMAND_LOAD))
			&& first + strlen (GLN_COMMAND_LOAD) == trailing))
	    {
		/*
		 * Print a message and call the level9 restore.  There is no
		 * need for confirmation since the file selection can be
		 * canceled.
		 */
		gln_standout_string ("\nRestoring using interpreter\n\n");
		restore ();
		return TRUE;
	    }

	/* No special buffer contents found. */
	return FALSE;
}


/*---------------------------------------------------------------------*/
/*  Glk port input functions                                           */
/*---------------------------------------------------------------------*/

/* Ctrl-C and Ctrl-U character constants. */
static const char	GLN_CONTROL_C			= '\003';
static const char	GLN_CONTROL_U			= '\025';

/*
 * os_readchar() call count limit, after which we really read a character.
 * Also, call count limit on os_stoplist calls, after which we poll for a
 * character press to stop the listing, and a stoplist poll timeout.
 */
static const int	GLN_READCHAR_LIMIT		= 1024;
static const int	GLN_STOPLIST_LIMIT		= 10;
static const glui32	GLN_STOPLIST_TIMEOUT		= 50;

/* Quote used to suppress abbreviation expansion and local commands. */
static const char	GLN_QUOTED_INPUT		= '\'';

/* Definition of whitespace characters to skip over. */
static const char	*GLN_WHITESPACE			= "\t ";

/*
 * Note of when the interpreter is in list output.  The last element of
 * any list generally lacks a terminating newline, and unless we do
 * something special with it, it'll look like a valid prompt to us.
 */
static int		gln_inside_list			= FALSE;


/*
 * gln_char_is_whitespace()
 *
 * Check for ASCII whitespace characters.  Returns TRUE if the character
 * qualifies as whitespace (NUL is not whitespace).
 */
static int
gln_char_is_whitespace (char c)
{
	return (c != '\0' && strchr (GLN_WHITESPACE, c) != NULL);
}


/*
 * gln_skip_leading_whitespace()
 *
 * Skip over leading whitespace, returning the address of the first non-
 * whitespace character.
 */
static char *
gln_skip_leading_whitespace (char *string)
{
	char	*result;			/* Return string pointer. */
	assert (string != NULL);

	/* Move result over leading whitespace. */
	for (result = string; gln_char_is_whitespace (*result); )
		result++;
	return result;
}


/* Table of single-character command abbreviations. */
struct gln_abbreviation {
	const char	abbreviation;		/* Abbreviation character. */
	const char	*expansion;		/* Expansion string. */
};
typedef const struct gln_abbreviation*		gln_abbreviationref_t;
static const struct gln_abbreviation		GLN_ABBREVIATIONS[] = {
	{ 'c',	"close"   },	{ 'g',	"again" },	{ 'i',	"inventory" },
	{ 'k',	"attack"  },	{ 'l',	"look"  },	{ 'p',	"open"      },
	{ 'q',	"quit"    },	{ 'r',	"drop"  },	{ 't',	"take"      },
	{ 'x',	"examine" },	{ 'y',	"yes"   },	{ 'z',	"wait"      },
	{ '\0',	NULL }
};

/*
 * gln_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems, but not always normal in Magnetic Scrolls games.
 */
static void
gln_expand_abbreviations (char *buffer, int size)
{
	char		*command;	/* Single-character command. */
	gln_abbreviationref_t
			entry, match;	/* Table search entry, and match. */
	assert (buffer != NULL);

	/* Skip leading spaces to find command, and return if nothing. */
	command = gln_skip_leading_whitespace (buffer);
	if (strlen (command) == 0)
		return;

	/* If the command is not a single letter one, do nothing. */
	if (strlen (command) > 1
			&& !gln_char_is_whitespace (command[1]))
		return;

	/* Scan the abbreviations table for a match. */
	match = NULL;
	for (entry = GLN_ABBREVIATIONS; entry->expansion != NULL; entry++)
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

#if 0
	/* Provide feedback on the expansion. */
	gln_standout_string ("[");
	gln_standout_char   (match->abbreviation);
	gln_standout_string (" -> ");
	gln_standout_string (match->expansion);
	gln_standout_string ("]\n");
#endif
}


/*
 * gln_output_endlist()
 *
 * The core interpreter doesn't terminate lists with a newline, so we take
 * care of that here; a fixup for input functions.
 */
static void
gln_output_endlist (void)
{
	/* Ignore the call if not inside a list. */
	if (gln_inside_list)
	    {
		/*
		 * Supply the missing newline, using os_printchar() so that
		 * list output doesn't look like a prompt when we come to
		 * flush it.
		 */
		os_printchar ('\n');

		/* Clear the list indicator. */
		gln_inside_list = FALSE;
	    }
}


/*
 * os_input()
 *
 * Read a line from the keyboard.  This function makes a special case of
 * some command strings, and will also perform abbreviation expansion.
 */
L9BOOL
os_input (char *buffer, int size)
{
	event_t		event;			/* Glk event buffer. */
	assert (buffer != NULL);

	/* Flush any pending buffered output, terminating any open list. */
	gln_output_endlist ();
	gln_output_flush ();

	/* Refresh graphics display. */
	gln_update_graphics (1);

	/*
	 * Level 9 games tend not to issue a prompt after reading an empty
	 * line of input, and the Adrian Mole games don't issue a prompt at
	 * all when outside the 1/2/3 menuing system.  This can make for a
	 * very blank looking screen.
	 *
	 * To slightly improve things, if it looks like we didn't get a
	 * prompt from the game, do our own.
	 */
	if (gln_prompt_enabled
			&& !gln_game_prompted ())
	    {
		gln_normal_char ('\n');
		gln_normal_string (GLN_INPUT_PROMPT);
	    }

	/*
	 * If we have an input log to read from, use that until it is
	 * exhausted.  On end of file, close the stream and resume input
	 * from line requests.
	 */
	if (gln_readlog_stream != NULL)
	    {
		glui32		chars;		/* Characters read. */

		/* Get the next line from the log stream. */
		chars = glk_get_line_stream
				(gln_readlog_stream, buffer, size);
		if (chars > 0)
		    {
			/* Echo the line just read in input style. */
			glk_set_style (style_Input);
			glk_put_buffer (buffer, chars);
			glk_set_style (style_Normal);

			/* Tick the watchdog, and return. */
			gln_watchdog_tick ();
			return TRUE;
		    }

		/*
		 * We're at the end of the log stream.  Close it, and then
		 * continue on to request a line from Glk.
		 */
		glk_stream_close (gln_readlog_stream, NULL);
		gln_readlog_stream = NULL;
	    }

	/* Set up the read buffer for the main window, and wait. */
	glk_request_line_event (gln_main_window, buffer, size - 1, 0);
	gln_event_wait (evtype_LineInput, &event);

	/* Terminate the input line with a NUL. */
	assert (event.val1 <= size - 1);
	buffer[ event.val1 ] = '\0';

	/*
	 * If neither abbreviations nor local commands are enabled, nor
	 * game command interceptions, then return the data read above
	 * without further massaging.
	 */
	if (gln_abbreviations_enabled
				|| gln_commands_enabled
				|| gln_intercept_enabled)
	    {
		char		*command;	/* Command part of buffer. */

		/* Find the first non-space character in the input buffer. */
		command = gln_skip_leading_whitespace (buffer);

		/*
		 * If the first non-space input character is a quote, bypass
		 * all abbreviation expansion and local command recognition,
		 * and use the unadulterated input, less introductory quote.
		 */
		if (command[0] == GLN_QUOTED_INPUT)
		    {
			/* Delete the quote with memmove(). */
			memmove (command, command + 1, strlen (command));
		    }
		else
		    {
			/* Check for, and expand, and abbreviated commands. */
			if (gln_abbreviations_enabled)
				gln_expand_abbreviations (buffer, size);

			/*
			 * Check for Glk port special commands, and if found
			 * then suppress the interpreter's use of this input.
			 */
			if (gln_commands_enabled
					&& gln_command_escape (buffer))
			    {
				gln_watchdog_tick ();
				return FALSE;
			    }

			/*
			 * Check for intercepted commands, and if found then
			 * suppress the interpreter's use of this input.
			 */
			if (gln_intercept_enabled
					&& gln_command_intercept (buffer))
			    {
				gln_watchdog_tick ();
				return FALSE;
			    }
		    }
	    }

	/*
	 * If there is an input log active, log this input string to it.
	 * Note that by logging here we get any abbreviation expansions but
	 * we won't log glk special commands, nor any input read from a
	 * current open input log.
	 */
	if (gln_inputlog_stream != NULL)
	    {
		glk_put_string_stream (gln_inputlog_stream, buffer);
		glk_put_char_stream (gln_inputlog_stream, '\n');
	    }

	/* Return TRUE since data buffered. */
	gln_watchdog_tick ();
	gln_kill_newline = 1;
	return TRUE;
}


/*
 * os_readchar()
 *
 * Poll the keyboard for characters, and return the character code of any key
 * pressed, or 0 if none pressed.
 *
 * Simple though this sounds, it's tough to do right in a timesharing OS, and
 * requires something close to an abuse of Glk.
 *
 * The initial, tempting, implementation is to wait inside this function for
 * a key press, then return the code.  Unfortunately, this causes problems in
 * the Level9 interpreter.  Here's why: the interpreter is a VM emulating a
 * single-user microprocessor system.  On such a system, it's quite okay for
 * code to spin in a loop waiting for a keypress; there's nothing else
 * happening on the system, so it can burn CPU.  To wait for a keypress, game
 * code might first wait for no-keypress (0 from this function), then a
 * keypress (non-0), then no-keypress again (and it does indeed seem to do
 * just this).  If, in os_readchar(), we simply wait for and return key codes,
 * we'll never return a 0, so the above wait for a keypress in the game will
 * hang forever.
 *
 * To make matters more complex, some Level 9 games poll for keypresses as a
 * way for a user to halt scrolling.  For these polls, we really want to
 * return 0, otherwise the output grinds to a halt.  Moreover, some games even
 * use key polling as a crude form of timeout - poll and increment a counter,
 * and exit when either os_readchar() returns non-0, or after some 300 or so
 * polls.
 *
 * So, this function MUST return 0 sometimes, and real key codes other times.
 * The solution adopted is best described as expedient.  Depending on what Glk
 * provides in the way of timers, we'll do one of two things:
 *
 *   o If we have timers, we'll set up a timeout, and poll for a key press
 *     within that timeout.  As a way to smooth output for games that use key
 *     press polling for scroll control, we'll ignore calls until we get two
 *     in a row without intervening character output.
 *
 *   o If we don't have timers, then we'll return 0 most of the time, and then
 *     really wait for a key one time out of some number.  A game polling for
 *     keypresses to halt scrolling will probably be to the point where it
 *     cannot continue without user input at this juncture, and once we've
 *     rejected a few hundred calls we can now really wait for Glk key press
 *     event, and avoid a spinning loop.  A game using key polling as crude
 *     timing may, or may not, time out in the calls for which we return 0.
 *
 * Empirically, this all seems to work.  The only odd behaviour is with the
 * DEMO mode of Adrian Mole where Glk has no timers, and this is primarily
 * because the DEMO mode relies on the delay of keyboard polling for part of
 * its effect; on a modern system, the time to call through here is nowhere
 * near the time consumed by the original platform.  The other point of note
 * is that this all means that we can't return characters from any readlog
 * with this function; it's timing stuff and it's general polling nature make
 * it impossible to connect to readlog, so it just won't work at all with the
 * Adrian Mole games, Glk timers or otherwise.
 */
char
os_readchar (int millis)
{
	static int	call_count = 0;		/* Calls count (no timers). */

	event_t		event;			/* Glk event buffer. */
	char		character;		/* Character read. */

	/*
	 * Here's the way we try to emulate keyboard polling for the case of
	 * no Glk timers.  We'll say nothing is pressed for some number of
	 * consecutive calls, then continue after that number of calls.
	 */
	if (!gln_timeouts_possible)
	    {
		call_count++;
		if (call_count < GLN_READCHAR_LIMIT)
		    {
			/* Call tick as we may be outside an opcode loop. */
			glk_tick ();
			gln_watchdog_tick ();
			return 0;
		    }
		else
			call_count = 0;
	    }

	/*
	 * If we have Glk timers, we can smooth game output with games that
	 * continuously use this input function by pretending that there is
	 * no keypress if the game printed output since the last call.  This
	 * helps with the Adrian Mole games, which check for a keypress at
	 * the end of a line as a way to temporarily halt scrolling.
	 */
	if (gln_timeouts_possible)
	    {
		if (gln_recent_output ())
		    {
			/* Call tick, and return no keypress. */
			glk_tick ();
			gln_watchdog_tick ();
			return 0;
		    }
	    }

	/*
	 * Now flush any pending buffered output.  We do it here rather
	 * than earlier as it only needs to be done when we're going to
	 * request Glk input, and we may have avoided this with the checks
	 * above.
	 */
	gln_output_endlist ();
	gln_output_flush ();

	/* Refresh graphics display. */
	gln_update_graphics (1);

	/*
	 * Set up a character event request, and a timeout if the Glk
	 * library can do them, and wait until one or the other occurs.
	 * Loop until we read an acceptable ASCII character (if we don't
	 * time out).
	 */
	do
	    {
		glk_request_char_event (gln_main_window);
		if (gln_timeouts_possible)
		    {
			/* Wait for a character or a timeout event. */
			glk_request_timer_events (millis);
			gln_event_wait_2 (evtype_CharInput,
							evtype_Timer, &event);
			glk_request_timer_events (0);

			/*
			 * If the event was a timeout, cancel the unfilled
			 * character request, and return no-keypress value.
			 */
			if (event.type == evtype_Timer)
			    {
				glk_cancel_char_event (gln_main_window);
				gln_watchdog_tick ();
				return 0;
			    }
		    }
		else
		    {
			/* Wait for only character events. */
			gln_event_wait (evtype_CharInput, &event);
		    }
	    }
	while (event.val1 > UCHAR_MAX && event.val1 != keycode_Return);

	/* Extract the character from the event, converting Return, no echo. */
	character = (event.val1 == keycode_Return) ? '\n' : event.val1;

	/*
	 * Special case ^U as a way to press a key on a wait, yet return a
	 * code to the interpreter as if no key was pressed.  Useful if
	 * scrolling stops where there are no Glk timers, to get scrolling
	 * started again.  ^U is always active.
	 */
	if (character == GLN_CONTROL_U)
	    {
		/* Pretend there was no key press after all. */
		gln_watchdog_tick ();
		return 0;
	    }

	/*
	 * Special case ^C to quit the program.  Without this, there's no
	 * easy way to exit from a game that never uses os_input(), but
	 * instead continually uses just os_readchar().  ^C handling can be
	 * disabled with command line options.
	 */
	if (gln_intercept_enabled
			&& character == GLN_CONTROL_C)
	    {
		/* Confirm the request to quit the game. */
		if (gln_confirm ("\n\nDo you really want to stop? [Y or N] "))
		    {
			gln_stop_reason = STOP_EXIT;
			StopGame ();

			/* Pretend there was no key press after all. */
			gln_watchdog_tick ();
			return 0;
		    }
	    }

	/*
	 * If there is a transcript stream, send the input to it as a one-
	 * line string, otherwise it won't be visible in the transcript.
	 */
	if (gln_transcript_stream != NULL)
	    {
		glk_put_char_stream (gln_transcript_stream, character);
		glk_put_char_stream (gln_transcript_stream, '\n');
	    }

	/* Return the single character read. */
	gln_watchdog_tick ();
	return character;
}


/*
 * os_stoplist()
 *
 * This is called from #dictionary listings to poll for a request to stop
 * the listing.  A check for keypress is usual at this point.  However, Glk
 * cannot check for keypresses without a delay, which slows listing consid-
 * erably, since it also adjusts and renders the display.  As a compromise,
 * then, we'll check for keypresses on a small percentage of calls, say one
 * in ten, which means that listings happen with only a short delay, but
 * there's still an opportunity to stop them.
 *
 * This applies only where the Glk library has timers.  Where it doesn't, we
 * can't check for keypresses without blocking, so we do no checks at all,
 * and let lists always run to completion.
 */
L9BOOL
os_stoplist (void)
{
	static int	call_count = 0;		/* Calls count (timers only). */

	event_t		event;			/* Glk event buffer. */
	int		status;			/* Confirm status. */

	/* Note that the interpreter is producing a list. */
	gln_inside_list = TRUE;

	/*
	 * If there are no Glk timers, then polling for a keypress but
	 * continuing on if there isn't one is not an option.  So flush
	 * output, return FALSE, and just keep listing on to the end.
	 */
	if (!gln_timeouts_possible)
	    {
		gln_output_flush ();
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Increment the call count, and return FALSE if under the limit. */
	call_count++;
	if (call_count < GLN_STOPLIST_LIMIT)
	    {
		/* Call tick as we may be outside an opcode loop. */
		glk_tick ();
		gln_watchdog_tick ();
		return FALSE;
	    }
	else
		call_count = 0;

	/*
	 * Flush any pending buffered output, delayed to here in case it's
	 * avoidable.
	 */
	gln_output_flush ();

	/*
	 * Look for a keypress, with a very short timeout in place, in a
	 * similar way as done for os_readchar() above.
	 */
	glk_request_char_event (gln_main_window);
	glk_request_timer_events (GLN_STOPLIST_TIMEOUT);
	gln_event_wait_2 (evtype_CharInput, evtype_Timer, &event);
	glk_request_timer_events (0);

	/*
	 * If the event was a timeout, cancel the unfilled character
	 * request, and return FALSE to continue listing.
	 */
	if (event.type == evtype_Timer)
	    {
		glk_cancel_char_event (gln_main_window);
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Keypress detected, so offer to stop listing. */
	assert (event.type == evtype_CharInput);
	status = gln_confirm ("\n\nStop listing? [Y or N] ");

	/*
	 * As we've output a newline, we no longer consider that we're
	 * inside a list.  Clear the flag, and also and prompt detection.
	 */
	gln_inside_list = FALSE;
	gln_game_prompted ();

	/* Return TRUE if stop was confirmed, FALSE to keep listing. */
	gln_watchdog_tick ();
	return status;
}


/*
 * gln_confirm()
 *
 * Print a confirmation prompt, and read a single input character, taking
 * only [YyNn] input.  If the character is 'Y' or 'y', return TRUE.
 */
static int
gln_confirm (const char *prompt)
{
	event_t		event;			/* Glk event buffer. */
	unsigned char	response;		/* Response character. */
	assert (prompt != NULL);

	/*
	 * Print the confirmation prompt, in a style that hints that it's
	 * from the interpreter, not the game.
	 */
	gln_standout_string (prompt);

	/* Wait for a single 'Y' or 'N' character response. */
	do
	    {
		/* Wait for a standard key, ignoring Glk special keys. */
		do
		    {
			glk_request_char_event (gln_main_window);
			gln_event_wait (evtype_CharInput, &event);
		    }
		while (event.val1 > UCHAR_MAX);
		response = glk_char_to_upper (event.val1);
	    }
	while (response != 'Y' && response != 'N');

	/* Echo the confirmation response, and a blank line. */
	glk_set_style (style_Input);
	glk_put_string (response == 'Y' ? "Yes" : "No");
	glk_set_style (style_Normal);
	glk_put_string ("\n\n");

	/* Return TRUE if 'Y' was entered. */
	return (response == 'Y');
}


/*---------------------------------------------------------------------*/
/*  Glk port event functions                                           */
/*---------------------------------------------------------------------*/

/*
 * gln_event_wait_2()
 * gln_event_wait()
 *
 * Process Glk events until one of the expected type, or types, arrives.
 * Return the event of that type.
 */
static void
gln_event_wait_2 (glui32 wait_type_1, glui32 wait_type_2, event_t *event)
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
			gln_update_graphics (0);
			gln_status_redraw ();
			break;
		    }
	    }
	while (event->type != wait_type_1
				&& event->type != wait_type_2);
}

static void
gln_event_wait (glui32 wait_type, event_t *event)
{
	assert (event != NULL);
	gln_event_wait_2 (wait_type, evtype_None, event);
}


/*---------------------------------------------------------------------*/
/*  Glk port file functions                                            */
/*---------------------------------------------------------------------*/

/*
 * os_save_file ()
 * os_load_file ()
 *
 * Save the current game state to a file, and load a game state.
 */
L9BOOL
os_save_file (L9BYTE *ptr, int bytes)
{
	frefid_t	fileref;		/* Glk file reference. */
	strid_t		stream;			/* Glk stream reference. */
	assert (ptr != NULL);

	/* Flush any pending buffered output. */
	gln_output_flush ();

	/* Get a Glk file reference for a game save file. */
	fileref = glk_fileref_create_by_prompt
				(fileusage_SavedGame, filemode_Write, 0);
	if (fileref == NULL)
	    {
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Open a Glk stream for the fileref. */
	stream = glk_stream_open_file (fileref, filemode_Write, 0);
	if (stream == NULL)
	    {
		glk_fileref_destroy (fileref);
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Write the game state data. */
	glk_put_buffer_stream (stream, ptr, bytes);

	/* Close and destroy the Glk stream and fileref. */
	glk_stream_close (stream, NULL);
	glk_fileref_destroy (fileref);

	/* All done. */
	gln_watchdog_tick ();
	return TRUE;
}

L9BOOL
os_load_file (L9BYTE *ptr, int *bytes, int max)
{
	frefid_t	fileref;		/* Glk file reference. */
	strid_t		stream;			/* Glk stream reference. */
	assert (ptr != NULL && bytes != NULL);

	/* Flush any pending buffered output. */
	gln_output_flush ();

	/* Get a Glk file reference for a game save file. */
	fileref = glk_fileref_create_by_prompt
				(fileusage_SavedGame, filemode_Read, 0);
	if (fileref == NULL)
	    {
		gln_watchdog_tick ();
		return FALSE;
	    }

	/*
	 * Reject the file reference if we're expecting to read from it,
	 * and the referenced file doesn't exist.
	 */
	if (!glk_fileref_does_file_exist (fileref))
	    {
		glk_fileref_destroy (fileref);
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Open a Glk stream for the fileref. */
	stream = glk_stream_open_file (fileref, filemode_Read, 0);
	if (stream == NULL)
	    {
		glk_fileref_destroy (fileref);
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Read back the game state data. */
	*bytes = glk_get_buffer_stream (stream, ptr, max);

	/* Close and destroy the Glk stream and fileref. */
	glk_stream_close (stream, NULL);
	glk_fileref_destroy (fileref);

	/* All done. */
	gln_watchdog_tick ();
	return TRUE;
}


/*---------------------------------------------------------------------*/
/*  Glk port multi-file game functions                                 */
/*---------------------------------------------------------------------*/

/* File path delimiter, used to be #defined in v2 interpreter. */
#if defined(_Windows) || defined(__MSDOS__) || defined (_WIN32) \
		|| defined (__WIN32__)
static const char	GLN_FILE_DELIM		= '\\';
#else
static const char	GLN_FILE_DELIM		= '/';
#endif

/*
 * os_get_game_file ()
 *
 * This function is a bit of a cheat.  It's called when the emulator has
 * detected a request from the game to restart the tape, on a tape-based
 * game.  Ordinarily, we should prompt the player for the name of the
 * system file containing the next game part.  Unfortunately, Glk doesn't
 * make this at all easy.  The requirement is to return a filename, but Glk
 * hides these inside fileref_t's, and won't let them out.
 *
 * Theoretically, according to the porting guide, this function should
 * prompt the user for a new game file name, that being the next part of the
 * game just (presumably) completed.
 *
 * However, the newname passed in is always the current game file name, as
 * level9.c ensures this for us.  If we search for, and find, and then inc-
 * rement, the last digit in the filename passed in, we wind up with, in
 * all likelihood, the right file path.  This is icky.
 *
 * This function is likely to be a source of portability problems on
 * platforms that don't implement a file path/name mechanism that matches
 * the expectations of the Level9 base interpreter fairly closely.
 */
L9BOOL
os_get_game_file (char *newname, int size)
{
	char		*basename;			/* Base of newname. */
	int		index;				/* Char iterator. */
	int		digit;				/* Digit index. */
	int		file_number;			/* Filename's number. */
	FILE		*stream;			/* Test open stream. */
	assert (newname != NULL);

	/* Find the last element of the filename passed in. */
	basename = strrchr (newname, GLN_FILE_DELIM);
	if (basename == NULL)
		basename = newname;
	else
		basename++;

	/* Search for the last numeric character in the basename. */
	digit = -1;
	//for (index = strlen (basename) - 1; index >= 0; index--)
	for (index = 0; index < strlen (basename); index++)
	    {
		if (isdigit (basename[ index ]))
		    {
			digit = index;
			break;
		    }
	    }
	if (digit == -1)
	    {
		/* No numeric character, but still note watchdog tick. */
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Convert the digit and increment it. */
	file_number = basename[ digit ] - '0' + 1;

	/* Refuse values outside of the range 1 to 9. */
	if (file_number < 1 || file_number > 9)
	    {
		gln_watchdog_tick ();
		return FALSE;
	    }

	/* Write the new number back into the file. */
	basename[ digit ] = file_number + '0';

	/* Flush pending output, then display the filename generated. */
	gln_output_flush ();
	gln_game_prompted ();
	gln_standout_string ("\nNext load file: ");
	gln_standout_string (basename);
	gln_standout_string ("\n\n");

	/*
	 * Try to confirm access to the file.  Otherwise, if we return TRUE
	 * but the interpreter can't open the file, it stops the game, and
	 * we then lose any chance to save it before quitting.
	 */
	stream = fopen (newname, "rb");
	if (stream == NULL)
	    {
		/* Restore newname to how it was, and return fail. */
		basename[ digit ] = file_number - 1 + '0';
		gln_watchdog_tick ();
		return FALSE;
	    }
	fclose (stream);

	/* Return success. */
	gln_watchdog_tick ();
	return TRUE;
}


/*
 * os_set_filenumber()
 *
 * This function returns the next file in a game series for a disk-based
 * game (typically, gamedat1.dat, gamedat2.dat...).  It finds a single digit
 * in a filename, and resets it to the new value passed in.  The implemen-
 * tation here is based on the generic interface version, and with the same
 * limitations, specifically being limited to file numbers in the range 0
 * to 9, since it works on only the last digit character in the filename
 * buffer passed in.
 *
 * This function may also be a source of portability problems on platforms
 * that don't use "traditional" file path schemes.
 */
void
os_set_filenumber (char *newname, int size, int file_number)
{
	char		*basename;			/* Base of newname. */
	int		index;				/* Char iterator. */
	int		digit;				/* Digit index. */
	assert (newname != NULL);

	/* Do nothing if the file number is beyond what we can handle. */
	if (file_number < 0 || file_number > 9)
	    {
		gln_watchdog_tick ();
		return;
	    }

	/* Find the last element of the new filename. */
	basename = strrchr (newname, GLN_FILE_DELIM);
	if (basename == NULL)
		basename = newname;
	else
		basename++;

	/* Search for the last numeric character in the basename. */
	digit = -1;
	//for (index = strlen (basename) - 1; index >= 0; index--)
	for (index = 0; index < strlen (basename); index++)
	    {
		if (isdigit (basename[ index ]))
		    {
			digit = index;
			break;
		    }
	    }
	if (digit == -1)
	    {
		/* No numeric character, but still note watchdog tick. */
		gln_watchdog_tick ();
		return;
	    }

	/* Reset the digit in the file name. */
	basename[ digit ] = file_number + '0';

	/* Flush pending output, then display the filename generated. */
	gln_output_flush ();
	gln_game_prompted ();
	gln_standout_string ("\nNext disk file: ");
	gln_standout_string (basename);
	gln_standout_string ("\n\n");

	/* Note watchdog tick and return. */
	gln_watchdog_tick ();
}


/*---------------------------------------------------------------------*/
/*  Functions intercepted by link-time wrappers                        */
/*---------------------------------------------------------------------*/

/*
 * __wrap_toupper()
 * __wrap_tolower()
 *
 * Wrapper functions around toupper() and tolower().  The Linux linker's
 * --wrap option will convert calls to mumble() to __wrap_mumble() if we
 * give it the right options.  We'll use this feature to translate all
 * toupper() and tolower() calls in the interpreter code into calls to
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
/*  main() and options parsing                                         */
/*---------------------------------------------------------------------*/

/*
 * Watchdog timeout -- we'll wait for five seconds of silence from the core
 * interpreter before offering to stop the game forcibly, and we'll check
 * it every 10,240 opcodes.
 */
static const int	GLN_WATCHDOG_TIMEOUT		= 5;
static const int	GLN_WATCHDOG_FREQUENCY		= 10240;

/*
 * The following values need to be passed between the startup_code and main
 * functions.
 */
       char	*gln_gamefile		= NULL;		/* Name of game file. */
static char	*gln_game_message	= NULL;		/* Error message. */


/*
 * gln_startup_code()
 * gln_main()
 *
 * Together, these functions take the place of the original main().
 * The first one is called from glkunix_startup_code(), to parse and
 * generally handle options.  The second is called from glk_main(), and
 * does the real work of running the game.
 */
static int
gln_startup_code (int argc, char *argv[])
{
	int		argv_index;			/* Argument iterator. */

	/* Handle command line arguments. */
	for (argv_index = 1;
		argv_index < argc && argv[ argv_index ][0] == '-'; argv_index++)
	    {
		if (strcmp (argv[ argv_index ], "-ni") == 0)
		    {
			gln_intercept_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-nc") == 0)
		    {
			gln_commands_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-na") == 0)
		    {
			gln_abbreviations_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-np") == 0)
		    {
			gln_prompt_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-nl") == 0)
		    {
			gln_loopcheck_enabled = FALSE;
			continue;
		    }
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
		gln_gamefile = argv[ argv_index ];
		gln_game_message = NULL;
#ifdef GARGLK
		{
			char *s;
			s = strrchr(gln_gamefile, '\\');
			if (s) garglk_set_story_name(s+1);
			s = strrchr(gln_gamefile, '/');
			if (s) garglk_set_story_name(s+1);
		}
#endif
	    }
	else
	    {
		gln_gamefile = NULL;
		if (argv_index < argc - 1)
			gln_game_message = "More than one game file"
					" was given on the command line.";
		else
			gln_game_message = "No game file was given"
					" on the command line.";
	    }

	/* All startup options were handled successfully. */
	return TRUE;
}

static void
gln_main (void)
{
	/* Ensure Level9 types have the right sizes. */
	if (sizeof (L9BYTE) != 1
			|| sizeof (L9UINT16) != 2
			|| sizeof (L9UINT32) != 4)
	    {
		gln_fatal ( "GLK: Types sized incorrectly,"
					" recompilation is needed");
		glk_exit ();
	    }

	/* Create the Glk window, and set its stream as the current one. */
	gln_main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
	if (gln_main_window == NULL)
	    {
		gln_fatal ("GLK: Can't open main window");
		glk_exit ();
	    }
	glk_window_clear (gln_main_window);
	glk_set_window (gln_main_window);
	glk_set_style (style_Normal);

	/* If there's a problem with the game file, complain now. */
	if (gln_gamefile == NULL)
	    {
		assert (gln_game_message != NULL);
		gln_header_string ("Glk Level9 Error\n\n");
		gln_normal_string (gln_game_message);
		gln_normal_char ('\n');
		glk_exit ();
	    }

	/* If Glk can't do timers, note that timeouts are not possible. */
	if (glk_gestalt (gestalt_Timer, 0))
		gln_timeouts_possible = TRUE;
	else
		gln_timeouts_possible = FALSE;

	/* Try to create a one-line status window.  We can live without it. */
/*
	gln_status_window = glk_window_open (gln_main_window,
					winmethod_Above|winmethod_Fixed,
					1, wintype_TextGrid, 0);
	gln_status_redraw ();
*/

	/*
	 * The main interpreter uses rand(), but never seeds the random
	 * number generator.  This can lead to predictability in games
	 * that might be better with a little less, so here, we'll seed
	 * the random number generator ourselves.
	 */
	srand (time (NULL));

	/* Repeat this game until no more restarts requested. */
	do
	    {
		int	watchdog_counter;	/* Watchdog freq counter. */

		/* Clear the Glk screen. */
		glk_window_clear (gln_main_window);

		/*
		 * Load the game.  At present, the Glk interface does no
		 * pictures, so the picture file argument is NULL.
		 */
		errno = 0;
		if (!LoadGame (gln_gamefile, NULL))
		    {
			/* Report the error, including any details in errno. */
			if (gln_status_window != NULL)
				glk_window_close (gln_status_window, NULL);
			gln_header_string ("Glk Level9 Error\n\n");
			gln_normal_string ("Can't find, open,"
						" or load game file '");
			gln_normal_string (gln_gamefile);
			gln_normal_char ('\'');
			if (errno != 0)
			    {
				gln_normal_string (": ");
				gln_normal_string (strerror (errno));
			    }
			gln_normal_char ('\n');

			/* Free interpreter allocated memory. */
			FreeMemory ();

			/* Nothing more to be done. */
			glk_exit ();
		    }

		/* Print out a short banner. */
		gln_header_string ("\nLevel 9 Interpreter, version 4.0\n");
		gln_banner_string ("Written by Glen Summers and David Kinder\n"
					"Glk interface by Simon Baldwin\n\n");

		/*
		 * Set the stop reason indicator to none.  A game will then
		 * exit with a reason if we call StopGame(), or none if it
		 * exits of its own accord (or with the "#quit" command, say).
		 */
		gln_stop_reason = STOP_NONE;

		/* Start, or restart, watchdog checking. */
		gln_watchdog_start (GLN_WATCHDOG_TIMEOUT);
		watchdog_counter = 0;

		/* Run the game until StopGame called. */
		while (TRUE)
		    {
			/* Execute an opcode - returns FALSE on StopGame. */
			if (!RunGame ())
				break;
			glk_tick ();

			/* Check for a possible game infinite loop. */
			watchdog_counter++;
			if (watchdog_counter == GLN_WATCHDOG_FREQUENCY)
			    {
				if (gln_watchdog_timeout ())
				    {
					gln_stop_reason = STOP_FORCE;
					StopGame ();
					break;
				    }
				watchdog_counter = 0;
			    }
		    }

		/*
		 * Stop watchdog functions, and flush any pending buffered
		 * output.
		 */
		gln_watchdog_stop ();
		gln_output_flush ();

		/* Free interpreter allocated memory. */
		FreeMemory ();

		/*
		 * Unset any "stuck" game 'cheating' flag.  This can get
		 * stuck on if exit is forced from the #cheat mode in the
		 * Adrian Mole games, which otherwise loop infinitely.  Un-
		 * setting the flag here permits restarts; without this,
		 * the core interpreter remains permanently in silent
		 * #cheat mode.
		 */
		Cheating = FALSE;

		/*
		 * If the stop reason is none, something in the game stopped
		 * itself, or the user entered "#quit".  If the stop reason
		 * is force, the user terminated because of an apparent inf-
		 * inite loop.  For both of these, offer the choice to restart,
		 * or not (equivalent to exit).
		 */
		if (gln_stop_reason == STOP_NONE
				|| gln_stop_reason == STOP_FORCE)
		    {
			if (gln_stop_reason == STOP_NONE)
				gln_standout_string (
					"\nThe game has exited.\n");
			else
				gln_standout_string (
					"\nGame exit was forced.  The current"
					" game state is unrecoverable."
					"  Sorry.\n");
			if (gln_confirm
					("\nDo you want to restart? [Y or N] "))
				gln_stop_reason = STOP_RESTART;
			else
				gln_stop_reason = STOP_EXIT;
		    }
	    }
	while (gln_stop_reason == STOP_RESTART);

	/* Close any open transcript, input log, and/or read log. */
	if (gln_transcript_stream != NULL)
		glk_stream_close (gln_transcript_stream, NULL);
	if (gln_inputlog_stream != NULL)
		glk_stream_close (gln_inputlog_stream, NULL);
	if (gln_readlog_stream != NULL)
		glk_stream_close (gln_readlog_stream, NULL);
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the real interpreter      */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int		gln_startup_called	= FALSE;
static int		gln_main_called		= FALSE;

#if TARGET_OS_MAC
/* Additional Mac variables. */
static strid_t		gln_mac_gamefile	= NULL;
static short		gln_savedVRefNum	= 0;
static long		gln_savedDirID		= 0;


/*
 * gln_mac_whenselected()
 * gln_mac_whenbuiltin()
 * macglk_startup_code()
 *
 * Startup entry points for Mac versions of Glk interpreter.  Glk will
 * call macglk_startup_code() for details on what to do when the app-
 * lication is selected.  On selection, an argv[] vector is built, and
 * passed to the normal interpreter startup code, after which, Glk will
 * call glk_main().
 */
static Boolean
gln_mac_whenselected (FSSpec *file, OSType filetype)
{
	static char*		argv[2];
	assert (!gln_startup_called);
	gln_startup_called = TRUE;

	/* Set the WD to where the file is, so later fopens work. */
	if (HGetVol (0, &gln_savedVRefNum, &gln_savedDirID) != 0)
	    {
		gln_fatal ("GLK: HGetVol failed");
		return FALSE;
	    }
	if (HSetVol (0, file->vRefNum, file->parID) != 0)
	    {
		gln_fatal ("GLK: HSetVol failed");
		return FALSE;
	    }

	/* Put a CString version of the PString name into argv[1]. */
	argv[1] = gln_malloc (file->name[0] + 1);
	BlockMoveData (file->name + 1, argv[1], file->name[0]);
	argv[1][file->name[0]] = '\0';
	argv[2] = NULL;

	return gln_startup_code (2, argv);
}

static Boolean
gln_mac_whenbuiltin (void)
{
	/* Not implemented yet. */
	return TRUE;
}

Boolean
macglk_startup_code (macglk_startup_t *data)
{
	static OSType		gln_mac_gamefile_types[] = { 'LVL9' };

	data->startup_model	 = macglk_model_ChooseOrBuiltIn;
	data->app_creator	 = 'cAGL';
	data->gamefile_types	 = gln_mac_gamefile_types;
	data->num_gamefile_types = sizeof (gln_mac_gamefile_types)
					/ sizeof (*gln_mac_gamefile_types);
	data->savefile_type	 = 'BINA';
	data->datafile_type	 = 0x3F3F3F3F;
	data->gamefile		 = &gln_mac_gamefile;
	data->when_selected	 = gln_mac_whenselected;
	data->when_builtin	 = gln_mac_whenbuiltin;
	/* macglk_setprefs(); */
	return TRUE;
}


#else /* not TARGET_OS_MAC */
/*
 * glkunix_startup_code()
 *
 * Startup entry point for UNIX versions of Glk interpreter.  Glk will
 * call glkunix_startup_code() to pass in arguments.  On startup, we call
 * our function to parse arguments and generally set stuff up.
 */
int
glkunix_startup_code (glkunix_startup_t *data)
{
	assert (!gln_startup_called);
	gln_startup_called = TRUE;

#ifdef GARGLK
	garglk_set_program_name("Level 9 4.0");
	garglk_set_program_info(
		"Level 9 4.0 by Glen Summers, David Kinder\n"
		"Alan Staniforth, Simon Baldwin and Dieter Baron\n"
		"Glk Graphics support by Tor Andersson\n");
#endif

	return gln_startup_code (data->argc, data->argv);
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
	assert (gln_startup_called && !gln_main_called);
	gln_main_called = TRUE;

	/* Call the interpreter main function. */
	gln_main ();
}

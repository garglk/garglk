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

#undef _WIN32	/* Gargoyle */
#define __unix	/* Gargoyle */

/*
 * Module notes:
 *
 * o The Glk interface makes no effort to set text colors, background
 *   colors, and so forth, and minimal effort to set fonts and other
 *   style effects.  This is due in large measure to the fact that I have
 *   only Xglk to test with, and it doesn't implement style hints at all,
 *   making efforts to use them here irrelevant.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "scare.h"
#include "glk.h"

#include "scprotos.h" /* for SCARE_VERSION */

/*
 * True and false definitions -- usually defined in glkstart.h, but just
 * in case they're not, or we're not using glkstart.h, we'll define them
 * here too.  We also need NULL, but that's normally from stdio.h or
 * one of it's cousins.
 */
#ifndef FALSE
# define	FALSE	0
#endif
#ifndef TRUE
# define	TRUE	(!FALSE)
#endif


/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous other stuff                        */
/*---------------------------------------------------------------------*/

/* Glk SCARE interface version number. */
static const glui32	GSC_PORT_VERSION		= 0x00010303;

/* Two windows, one for the main text, and one for a status line. */
static winid_t		gsc_main_window			= NULL;
static winid_t		gsc_status_window		= NULL;

/*
 * Transcript stream and input log.  These are NULL if there is no
 * current collection of these strings.
 */
static strid_t		gsc_transcript_stream		= NULL;
static strid_t		gsc_inputlog_stream		= NULL;

/* Input read log stream, for reading back an input log. */
static strid_t		gsc_readlog_stream		= NULL;

/* Options that may be turned off by command line flags. */
static int		gsc_commands_enabled		= TRUE;
static int		gsc_abbreviations_enabled	= TRUE;

/* Adrift game to interpret. */
static sc_game		gsc_game			= NULL;

/* Special out-of-band os_confirm() options used locally with os_glk. */
static const sc_uint32	GSC_CONF_SUBTLE_HINT		= UINT_MAX,
			GSC_CONF_UNSUBTLE_HINT		= UINT_MAX - 1,
			GSC_CONF_CONTINUE_HINTS		= UINT_MAX - 2;

/* Forward declaration of event wait function, and a short delay. */
static void		gsc_event_wait (glui32 wait_type, event_t *event);
static void		gsc_short_delay (void);


/*---------------------------------------------------------------------*/
/*  Glk port utility functions                                         */
/*---------------------------------------------------------------------*/

/*
 * gsc_fatal()
 *
 * Fatal error handler.  The function returns, expecting the caller to
 * abort() or otherwise handle the error.
 */
static void
gsc_fatal (const char *string)
{
	/*
	 * If the failure happens too early for us to have a window, print
	 * the message to stderr.
	 */
	if (gsc_main_window == NULL)
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
	glk_cancel_line_event (gsc_main_window, NULL);
	glk_cancel_char_event (gsc_main_window);

	/* Print a message indicating the error, and exit. */
	glk_set_window (gsc_main_window);
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
 * gsc_malloc()
 *
 * Non-failing malloc; call gsc_fatal and exit if memory allocation fails.
 */
static void *
gsc_malloc (size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Malloc, and call gsc_fatal if the malloc fails. */
	pointer = malloc (size);
	if (pointer == NULL)
	    {
		gsc_fatal ("GLK: Out of system memory");
		glk_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}


/*---------------------------------------------------------------------*/
/*  Status line functions                                              */
/*---------------------------------------------------------------------*/

/* Default width used for non-windowing Glk status lines. */
enum {			GSC_DEFAULT_STATUS_WIDTH	= 74 };


/*
 * gsc_status_update()
 *
 * Update the status line from the current game state.  This is for windowing
 * Glk libraries.
 */
static void
gsc_status_update (void)
{
	strid_t		stream;			/* Status stream. */
	glui32		width, height;		/* Window dimensions. */
	int			i;
	assert (gsc_status_window != NULL);

	/* Get status window dimensions. */
	glk_window_get_size (gsc_status_window, &width, &height);
	if (height == 0)
		return;

	stream = glk_window_get_stream (gsc_status_window);

	/* Clear the status window, and move cursor to left edge. */
	glk_window_clear (gsc_status_window);

	glk_set_style_stream(stream, style_User1);
	for (i = 0; i < width; i++)
	    glk_put_char_stream(stream, ' ');

	width -= 2;

	glk_window_move_cursor (gsc_status_window, 1, 0);


	/* See if the game is indicating any current player room. */
	if (sc_get_game_room (gsc_game) == NULL)
	    {
		/*
		 * Player location is indeterminate, so print out a generic
		 * status, showing the game name and author.
		 */
		glk_put_string_stream (stream, sc_get_game_name (gsc_game));
		glk_put_string_stream (stream, " | ");
		glk_put_string_stream (stream, sc_get_game_author (gsc_game));
	    }
	else
	    {
		sc_char		*status;

		/* Print the player location. */
		glk_put_string_stream (stream, sc_get_game_room (gsc_game));

		/* See if the game offers a specialized status line. */
		status = sc_get_game_status_line (gsc_game);
		if (status != NULL)
		    {
			/* Print the game's status line at window right. */
			glk_window_move_cursor (gsc_status_window,
						width - strlen (status), 0);
			glk_put_string_stream (stream, status);
		    }
		else
		    {
			char	buffer[32];

			/* Print the score at window right. */
			sprintf (buffer, "Score: %ld",
						sc_get_game_score (gsc_game));
			glk_window_move_cursor (gsc_status_window,
						width - strlen (buffer), 0);
			glk_put_string_stream (stream, buffer);
		    }
	    }
}


/*
 * gsc_status_safe_strcat()
 *
 * Helper for gsc_status_print(), concatenates strings only up to the
 * available length.
 */
static void
gsc_status_safe_strcat (char *dest, size_t length, const char *src)
{
	size_t		avail, src_len;

	/* Append only as many characters as will fit. */
	src_len = strlen (src);
	avail = length - strlen (dest) - 1;
	if (avail > 0)
		strncat (dest, src, src_len < avail ? src_len : avail);
}


/*
 * gsc_status_print()
 *
 * Print the current contents of the completed status line buffer out in the
 * main window, if it has changed since the last call.  This is for non-
 * windowing Glk libraries.
 */
static void
gsc_status_print (void)
{
	static char	current_status[GSC_DEFAULT_STATUS_WIDTH + 1];
						/* Saved line buffer. */

	char		buffer[GSC_DEFAULT_STATUS_WIDTH + 1];
						/* Local line buffer. */
	sc_char		*status;		/* Game status string. */
	char		score[32];		/* Score conversion buffer. */
	unsigned int	index;			/* Padding loop index. */

	/* Do nothing if the game isn't indicating any current player room. */
	if (sc_get_game_room (gsc_game) == NULL)
		return;

	/* Format a reasonable attempt at a status line, room at left... */
	strcpy (buffer, "");
	gsc_status_safe_strcat (buffer,
				sizeof (buffer), sc_get_game_room (gsc_game));

	/* ... and either game status or score at right. */
	status = sc_get_game_status_line (gsc_game);
	if (status == NULL)
	    {
		sprintf (score, "Score: %ld", sc_get_game_score (gsc_game));
		status = score;
	    }

	/* Pad status line, then append status or score. */
	for (index = strlen (buffer);
			index < sizeof (buffer) - strlen (status) - 1;
			index++)
		gsc_status_safe_strcat (buffer, sizeof (buffer), " ");

	gsc_status_safe_strcat (buffer, sizeof (buffer), status);

	/* If this matches the current saved status line, do nothing more. */
	if (strcmp (buffer, current_status) == 0)
		return;

	/* Set fixed width font to try to preserve status line formatting. */
	glk_set_style (style_Preformatted);

	/* Bracket, and output the status line buffer. */
	glk_put_string ("[ ");
	glk_put_string (buffer);
	glk_put_string (" ]\n");

	/* Save the details of the printed status buffer. */
	strcpy (current_status, buffer);
}


/*
 * gsc_status_notify()  
 *                       
 * Front end function for updating status.  Either updates the status window
 * or prints the status line to the main window.
 */     
static void
gsc_status_notify (void)
{       
	/*
	 * If there is a status window, update it; if not, print the status
	 * line to the main window.
	 */ 
	if (gsc_status_window != NULL)
		gsc_status_update ();
	else
		gsc_status_print ();
}


/*
 * gsc_status_redraw()
 *
 * Redraw the contents of any status window with the constructed status string.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
static void
gsc_status_redraw (void)
{
	winid_t		parent;		/* Status window parent. */

	/* If there is no status window, ignore the call. */
	if (gsc_status_window == NULL)
		return;

	/*
	 * Rearrange the status window, without changing its actual
	 * arrangement in any way.  This is a hack to work round incorrect
	 * window repainting in Xglk; it forces a complete repaint of
	 * affected windows on Glk window resize and arrange events, and
	 * works in part because Xglk doesn't check for actual arrangement
	 * changes in any way before invalidating its windows.  The hack
	 * should be harmless to Glk libraries other than Xglk, moreover,
	 * we're careful to activate it only on resize and arrange events.
	 */
	parent = glk_window_get_parent (gsc_status_window);
	glk_window_set_arrangement (parent,
				winmethod_Above|winmethod_Fixed, 1, NULL);

	/* ...now update the status window. */
	gsc_status_update ();
}


/*---------------------------------------------------------------------*/
/*  Output functions                                                   */
/*---------------------------------------------------------------------*/

/* Bit mask for font monospacing. */
static const glui32	GSC_MONOSPACED_MASK		= 1 << 31;

/* Font stack and attributes for nesting tags. */
enum {			GSC_MAX_STYLE_NESTING		= 32 };
static glui32		gsc_font_stack[GSC_MAX_STYLE_NESTING];
static glui32		gsc_font_index			= 0;
static glui32		gsc_attribute_bold		= 0,
			gsc_attribute_italic		= 0,
			gsc_attribute_underline		= 0,
			gsc_attribute_secondary_color	= 0;

/* Notional default font size, and limit font sizes. */
static const sc_uint32	GSC_DEFAULT_FONT_SIZE		= 12,
			GSC_MEDIUM_FONT_SIZE		= 14,
			GSC_LARGE_FONT_SIZE		= 16;

/* Milliseconds per second and timeouts count for delay tags. */
static const glui32	GSC_MILLISECONDS_PER_SECOND	= 1000;
static const glui32	GSC_TIMEOUTS_COUNT		= 10;

/* Known valid character printing range. */
static const char	GSC_MIN_PRINTABLE		= ' ',
			GSC_MAX_PRINTABLE		= 126;

/* Number of hints to refuse before offering to end hint display. */
static const sc_uint16	GSC_HINT_REFUSAL_LIMIT		= 5;

/*
 * gsc_set_glk_style()
 *
 * Set a Glk style based on the top of the font stack and attributes.
 */
static void
gsc_set_glk_style (void)
{
	sc_bool		monospaced;		/* Monospaced flag. */
	sc_uint32	font_size;		/* Font point size. */

	/* Get the current font stack top, or default value. */
	if (gsc_font_index > 0)
	    {
		monospaced = (gsc_font_stack[gsc_font_index - 1]
						&  GSC_MONOSPACED_MASK) != 0;
		font_size  =  gsc_font_stack[gsc_font_index - 1]
						& ~GSC_MONOSPACED_MASK;
	    }
	else
	    {
		monospaced = FALSE;
		font_size  = GSC_DEFAULT_FONT_SIZE;
	    }

	/*
	 * Map the font and current attributes into a Glk style.  Because
	 * Glk styles aren't cumulative this has to be done by precedences.
	 */
	if (monospaced)
	    {
		/*
		 * No matter the size or attributes, if monospaced use
		 * Preformatted style, as it's all we have.
		 */
		glk_set_style (style_Preformatted);
	    }
	else
	    {
		/*
		 * For large and medium point sizes, use Header or Subheader
		 * styles respectively.
		 */
		if (font_size >= GSC_LARGE_FONT_SIZE)
			glk_set_style (style_Header);
		else if (font_size >= GSC_MEDIUM_FONT_SIZE)
			glk_set_style (style_Subheader);
		else
		    {
			/*
			 * For bold, use Subheader; for italics, underline,
			 * or secondary color, use Emphasized.
			 */
			if (gsc_attribute_bold > 0)
				glk_set_style (style_Subheader);
			else if ((gsc_attribute_italic > 0)
					|| (gsc_attribute_underline > 0)
					|| (gsc_attribute_secondary_color > 0))
				glk_set_style (style_Emphasized);
			else
			    {
				/*
				 * There's nothing special about this text,
				 * so drop down to Normal style.
				 */
				glk_set_style (style_Normal);
			    }
		    }
	    }
}


/*
 * gsc_handle_font_tag()
 * gsc_handle_endfont_tag()
 *
 * Push the settings of a font tag onto the font stack, and pop on end of
 * font tag.  Set the appropriate Glk style.
 */
static void
gsc_handle_font_tag (const sc_char *arg)
{
	sc_char		*lowercase_arg;		/* Lowercased arg string. */
	sc_char		*face_arg;		/* Face= font arg. */
	sc_char		*size_arg;		/* Size= font arg. */
	unsigned int	index;			/* String copy index. */
	sc_bool		monospaced;		/* Monospaced flag. */
	sc_uint32	font_size;		/* Font point size. */

	/* Ignore the call on stack overrun. */
	if (gsc_font_index >= GSC_MAX_STYLE_NESTING)
		return;

	/* Get the current top of stack, or default on empty stack. */
	if (gsc_font_index > 0)
	    {
		monospaced = (gsc_font_stack[gsc_font_index - 1]
						&  GSC_MONOSPACED_MASK) != 0;
		font_size  =  gsc_font_stack[gsc_font_index - 1]
						& ~GSC_MONOSPACED_MASK;
	    }
	else
	    {
		monospaced = FALSE;
		font_size  = GSC_DEFAULT_FONT_SIZE;
	    }

	/* Copy and convert arg to all lowercase. */
	lowercase_arg = gsc_malloc (strlen (arg) + 1);
	for (index = 0; arg[index] != '\0'; index++)
		lowercase_arg[index] = glk_char_to_lower (arg[index]);
	lowercase_arg[strlen (arg)] = '\0';

	/* Find any face= portion of the tag argument. */
	face_arg = strstr (lowercase_arg, "face=");
	if (face_arg != NULL)
	    {
		/*
		 * There may be plenty of monospaced fonts, but we do only
		 * courier and terminal.
		 */
		monospaced =!sc_strncasecmp (face_arg, "face=\"courier\"", 14)
			  | !sc_strncasecmp (face_arg, "face=\"terminal\"", 15);
	    }

	/* Find the size= portion of the tag argument. */
	size_arg = strstr (lowercase_arg, "size=");
	if (size_arg != NULL)
	    {
		sc_uint32	size;

		/* Deal with incremental and absolute sizes. */
		if (!sc_strncasecmp (size_arg, "size=+", 6)
				&& sscanf (size_arg, "size=+%lu", &size) == 1)
			font_size += size;
		else if (!sc_strncasecmp (size_arg, "size=-", 6)
				&& sscanf (size_arg, "size=-%lu", &size) == 1)
			font_size -= size;
		else if (sscanf (size_arg, "size=%lu", &size) == 1)
			font_size = size;
	    }

	/* Done with tag argument copy. */
	free (lowercase_arg);

	/*
	 * Push the new font setting onto the font stack, and set Glk
	 * style.
	 */
	gsc_font_stack[gsc_font_index++] =
			(monospaced ? GSC_MONOSPACED_MASK : 0) | font_size;
	gsc_set_glk_style ();
}

static void
gsc_handle_endfont_tag (void)
{
	/* Unless underrun, pop the font stack and set Glk style. */
	if (gsc_font_index > 0)
	    {
		gsc_font_index--;
		gsc_set_glk_style ();
	    }
}


/*
 * gsc_handle_attribute_tag()
 *
 * Increment the required attribute nesting counter, or decrement on end
 * tag.  Set the appropriate Glk style.
 */
static void
gsc_handle_attribute_tag (sc_uint32 tag)
{
	/*
	 * Increment the required attribute nesting counter, and set Glk
	 * style.
	 */
	switch (tag)
	    {
	    case SC_TAG_BOLD:		gsc_attribute_bold++;		break;
	    case SC_TAG_ITALICS:	gsc_attribute_italic++;		break;
	    case SC_TAG_UNDERLINE:	gsc_attribute_underline++;	break;
	    case SC_TAG_SCOLOR:		gsc_attribute_secondary_color++;break;
	    default:							break;
	    }
	gsc_set_glk_style ();
}

static void
gsc_handle_endattribute_tag (sc_uint32 tag)
{
	/*
	 * Decrement the required attribute nesting counter, unless underrun,
	 * and set Glk style.
	 */
	switch (tag)
	    {
	    case SC_TAG_ENDBOLD:
		if (gsc_attribute_bold > 0)
			gsc_attribute_bold--;
		break;
	    case SC_TAG_ENDITALICS:
		if (gsc_attribute_italic > 0)
			gsc_attribute_italic--;
		break;
	    case SC_TAG_ENDUNDERLINE:
		if (gsc_attribute_underline > 0)
			gsc_attribute_underline--;
		break;
	    case SC_TAG_ENDSCOLOR:
		if (gsc_attribute_secondary_color > 0)
			gsc_attribute_secondary_color--;
		break;
	    default:
		break;
	    }
	gsc_set_glk_style ();
}


/*
 * gsc_reset_glk_style()
 *
 * Drop all stacked fonts and nested attributes, and return to normal
 * Glk style.
 */
static void
gsc_reset_glk_style (void)
{
	/* Reset the font stack and attributes, and set a normal style. */
	gsc_font_index			= 0;
	gsc_attribute_bold		= 0;
	gsc_attribute_italic		= 0;
	gsc_attribute_underline		= 0;
	gsc_attribute_secondary_color	= 0;
	gsc_set_glk_style ();
}


/*
 * os_print_tag()
 *
 * Interpret selected Adrift output control tags.  Not all are implemented
 * here; several are ignored.
 */
void
os_print_tag (sc_uint32 tag, const sc_char *arg)
{
	event_t		event;			/* Event buffer. */
	assert (arg != NULL);

	/* Control based on the tag type. */
	switch (tag)
	    {
	    case SC_TAG_CLS:
		/* Clear the main text display window. */
		glk_window_clear (gsc_main_window);
		break;

	    case SC_TAG_FONT:
		/* Handle with specific tag handler function. */
		gsc_handle_font_tag (arg);
		break;

	    case SC_TAG_ENDFONT:
		/* Handle with specific endtag handler function. */
		gsc_handle_endfont_tag ();
		break;

	    case SC_TAG_BOLD:		case SC_TAG_ITALICS:
	    case SC_TAG_UNDERLINE:	case SC_TAG_SCOLOR:
		/* Handle with common attribute tag handler function. */
		gsc_handle_attribute_tag (tag);
		break;

	    case SC_TAG_ENDBOLD:	case SC_TAG_ENDITALICS:
	    case SC_TAG_ENDUNDERLINE:	case SC_TAG_ENDSCOLOR:
		/* Handle with common attribute endtag handler function. */
		gsc_handle_endattribute_tag (tag);
		break;

	    case SC_TAG_CENTER:		case SC_TAG_RIGHT:
	    case SC_TAG_ENDCENTER:	case SC_TAG_ENDRIGHT:
		/*
		 * We don't center or justify text, but so that things look
		 * right we do want a newline on starting or ending such a
		 * section.
		 */
		glk_put_char ('\n');
		break;

	    case SC_TAG_WAIT:
		{
		float		fwait = 0.0f;	/* Adrift delay in n.n form. */
		glui32		mswait, timeout;/* Glk delay, timeout count. */

		/* Ignore the wait tag if the Glk doesn't have timers. */
		if (!glk_gestalt (gestalt_Timer, 0))
			break;

		/* Determine the delay time, and convert to milliseconds. */
		sscanf (arg, "%f", &fwait);
		mswait = (glui32) (fwait * GSC_MILLISECONDS_PER_SECOND);
		if (fwait <= 0.0f || mswait == 0)
			break;

		/* Update status line. */
		gsc_status_notify ();

		/*
		 * Request timeouts at 1/10 of the wait period, to minimize
		 * Glk timer jitter, then wait for 10 timeouts.  Cancel Glk
		 * timeouts when done.
		 */
		glk_request_timer_events (mswait / GSC_TIMEOUTS_COUNT);
		for (timeout = 0; timeout < GSC_TIMEOUTS_COUNT; timeout++)
			gsc_event_wait (evtype_Timer, &event);
		glk_request_timer_events (0);
		break;
		}

	    case SC_TAG_WAITKEY:
		/* Update the status line. */
		gsc_status_notify ();

		/* Request a character event, and wait for it to be filled. */
		glk_request_char_event (gsc_main_window);
		gsc_event_wait (evtype_CharInput, &event);
		break;

	    default:
		/* Ignore unimplemented and unknown tags. */
		break;
	    }
}


/*
 * os_print_string()
 *
 * Print a text string to the main output window.  Adrift games tend to
 * use assorted Microsoft CP-1252 characters, so output conversion may
 * be needed.
 */
void
os_print_string (const sc_char *string)
{
	unsigned int	index;			/* String iterator. */
	unsigned int	marker;			/* Printout location. */
	assert (string != NULL);
	assert (glk_stream_get_current () != NULL);

	/*
	 * Iterate each character in the string, scanning for unprintable
	 * characters that need special handling.
	 */
	marker = 0;
	for (index = 0; string[index] != '\0'; index++)
	    {
		unsigned char	character;

		/*
		 * Extract and check for Glk printability, and loop if
		 * printable; special check for newline which Xglk denies
		 * will print, though we know it will.
		 */
		character = (unsigned char) string[index];
		if (character == '\n'
				|| (character >= GSC_MIN_PRINTABLE
					&& character <= GSC_MAX_PRINTABLE)
				|| glk_gestalt (gestalt_CharOutput, character))
			continue;

		/* Flush data up to this point. */
		if (index > marker)
			glk_put_buffer ((char *) string + marker,
							index - marker);

		/* Handle unprintable character, advance marker. */
		switch (character)
		    {
		    case '\t':	glk_put_string ("        ");	break;
		    case '\r':	glk_put_char ('\n');		break;

		    case 0x80:	glk_put_char ('E');  break; /* Euro */
		    case 0x83:	glk_put_char ('f');  break; /* Florin */
		    case 0x85:	glk_put_char ('.');
				glk_put_char ('.');
				glk_put_char ('.');  break; /* Ellipsis */
		    case 0x86:	glk_put_char ('+');  break; /* Dagger */
		    case 0x87:	glk_put_char ('#');  break; /* Double dagger */
		    case 0x88:	glk_put_char ('^');  break; /* Circumflex */
		    case 0x8B:	glk_put_char ('<');  break; /* L angle quote */
		    case 0x8C:	glk_put_char ('O');
				glk_put_char ('E');  break; /* OE */
		    case 0x91:
		    case 0x92:	glk_put_char ('\''); break; /* L/R single q */
		    case 0x93:
		    case 0x94:	glk_put_char ('"');  break; /* L/R double q */
		    case 0x95:	glk_put_char (0xb7); break; /* Bullet */
		    case 0x96:
		    case 0x97:	glk_put_char ('-');  break; /* En/Em dash */
		    case 0x98:	glk_put_char ('~');  break; /* Small tilde */
		    case 0x99:	glk_put_char ('[');
				glk_put_char ('T');
				glk_put_char ('M');
				glk_put_char (']');  break; /* Trade mark */
		    case 0x9B:	glk_put_char ('>');  break; /* R angle quote */
		    case 0x9C:	glk_put_char ('o');
				glk_put_char ('e');  break; /* oe */
		    default:	glk_put_char ('?');  break; /* Unsupported */
		    }
		marker = index + 1;
	    }

	/* Flush remaining unprinted data. */
	if (index > marker)
		glk_put_buffer ((char *) string + marker, index - marker);
}


/*
 * os_print_string_debug()
 *
 * Debugging output goes to the main Glk window -- no special effects or
 * dedicated debugging window attempted.
 */
void
os_print_string_debug (const sc_char *string)
{
	os_print_string (string);
}


/*
 * gsc_standout_string()
 * gsc_standout_char()
 *
 * Print a string in a style that stands out from game text.
 */
static void
gsc_standout_string (const char *message)
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
gsc_standout_char (char c)
{
	/* Print the character, in a message style. */
	glk_set_style (style_Emphasized);
	glk_put_char (c);
	glk_set_style (style_Normal);
}


/*
 * gsc_normal_string()
 * gsc_normal_char()
 *
 * Print a string in normal text style.
 */
static void
gsc_normal_string (const char *message)
{
	assert (message != NULL);

	/* Print the message, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_string ((char *) message);
}

static void
gsc_normal_char (char c)
{
	/* Print the character, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_char (c);
}


/*
 * gsc_header_string()
 *
 * Print text messages for the banner on game error.
 */
static void
gsc_header_string (const char *banner)
{
	assert (banner != NULL);

	/* Print the string in a header style. */
	glk_set_style (style_Header);
	glk_put_string ((char *) banner);
	glk_set_style (style_Normal);
}


/*
 * os_flush()
 *
 * This function is a stub; we don't buffer data, and Glk flushes only
 * when awaiting events.
 */
void
os_flush (void)
{
}


/*
 * os_display_hints()
 *
 * This is a very basic hints display.  In mitigation, very few games use
 * hints at all, and those that do are usually sparse in what they hint
 * at, so it's sort of good enough for the moment.
 */
void
os_display_hints (sc_game game)
{
	sc_game_hint	hint;
	sc_uint16	refused;

	/* For each hint, print the question, and confirm hint display. */
	refused = 0;
	for (hint = sc_iterate_game_hints (game, NULL);
			hint != NULL; hint = sc_iterate_game_hints (game, hint))
	    {
		sc_char		*hint_question, *hint_text;

		/* If enough refusals, offer a way out of the loop. */
		if (refused >= GSC_HINT_REFUSAL_LIMIT)
		    {
			if (!os_confirm (GSC_CONF_CONTINUE_HINTS))
				break;
			refused = 0;
		    }

		/* Pop the question. */
		hint_question = sc_get_game_hint_question (game, hint);
		gsc_normal_char ('\n');
		gsc_standout_string (hint_question);
		gsc_normal_char ('\n');

		/* Print the subtle hint, or on to the next hint. */
		hint_text = sc_get_game_subtle_hint (game, hint);
		if (hint_text != NULL)
		    {
			if (!os_confirm (GSC_CONF_SUBTLE_HINT))
			    {
				refused++;
				continue;
			    }
			gsc_normal_char ('\n');
			gsc_standout_string (hint_text);
			gsc_normal_string ("\n\n");
		    }

		/* Print the less than subtle hint, or on to the next hint. */
		hint_text = sc_get_game_unsubtle_hint (game, hint);
		if (hint_text != NULL)
		    {
			if (!os_confirm (GSC_CONF_UNSUBTLE_HINT))
			    {
				refused++;
				continue;
			    }
			gsc_normal_char ('\n');
			gsc_standout_string (hint_text);
			gsc_normal_string ("\n\n");
		    }
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk resource handling functions                                    */
/*---------------------------------------------------------------------*/

/*
 * os_play_sound()
 * os_stop_sound()
 *
 * Stub functions.  The unused variables defeat gcc warnings.
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


/*
 * os_show_graphic()
 *
 * For graphic-capable Glk libraries on Linux, attempt graphics using xv.
 * The graphic capability test isn't really required, it's just a way of
 * having graphics behave without surprises; someone using a non-graphical
 * Glk probably won't expect graphics to pop up.
 *
 * For other cases, this is a stub function, with unused variables to
 * defeat gcc warnings.
 */
#ifdef LINUX_GRAPHICS
static int		gsclinux_graphics_enabled	= TRUE;
static char		*gsclinux_game_file		= NULL;
void
os_show_graphic (sc_char *filepath, sc_int32 offset, sc_int32 length)
{
	sc_char		*unused1; unused1 = filepath;

	if (length > 0
		&& gsclinux_graphics_enabled
		&& glk_gestalt (gestalt_Graphics, 0))
	    {
		sc_char		*buffer;

		/*
		 * Try to extract data with dd.  Assuming that works, back-
		 * ground xv to display the image, then background a job to
		 * delay ten seconds and then delete the temporary file
		 * containing the image.  Not exactly finessed.
		 */
		assert (gsclinux_game_file != NULL);
		buffer = gsc_malloc (strlen (gsclinux_game_file) + 128);
		sprintf (buffer, "dd if=%s ibs=1c skip=%ld count=%ld obs=100k"
				 " of=/tmp/scare.jpg 2>/dev/null",
					gsclinux_game_file, offset, length);
		system (buffer);
		free (buffer);
		system ("xv /tmp/scare.jpg >/dev/null 2>&1 &");
		system ("( sleep 10; rm /tmp/scare.jpg ) >/dev/null 2>&1 &");
	    }
}
#else
void
os_show_graphic (sc_char *filepath, sc_int32 offset, sc_int32 length)
{
	sc_char		*unused1;
	sc_int32	unused2, unused3;
	unused1 = filepath;
	unused2 = offset; unused3 = length;
}
#endif


/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/* Valid command control values. */
static const char	*GSC_COMMAND_ON			= "on";
static const char	*GSC_COMMAND_OFF		= "off";


/*
 * gsc_command_script()
 *
 * Turn game output scripting (logging) on and off.
 */
static void
gsc_command_script (const char *argument)
{
	assert (argument != NULL);

	/* Set up a transcript according to the argument given. */
	if (!sc_strcasecmp (argument, GSC_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a transcript is already active. */
		if (gsc_transcript_stream != NULL)
		    {
			gsc_normal_string ("Glk transcript is already ");
			gsc_normal_string (GSC_COMMAND_ON);
			gsc_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a transcript. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_Transcript | fileusage_TextMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gsc_standout_string ("Glk transcript failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gsc_transcript_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gsc_transcript_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gsc_standout_string ("Glk transcript failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Set the new transcript stream as the main echo stream. */
		glk_window_set_echo_stream (gsc_main_window,
						gsc_transcript_stream);

		/* Confirm action. */
		gsc_normal_string ("Glk transcript is now ");
		gsc_normal_string (GSC_COMMAND_ON);
		gsc_normal_string (".\n");
	    }
	else if (!sc_strcasecmp (argument, GSC_COMMAND_OFF))
	    {
		/* If a transcript is active, close it. */
		if (gsc_transcript_stream != NULL)
		    {
			glk_stream_close (gsc_transcript_stream, NULL);
			gsc_transcript_stream = NULL;

			/* Clear the main echo stream. */
			glk_window_set_echo_stream (gsc_main_window, NULL);

			/* Confirm action. */
			gsc_normal_string ("Glk transcript is now ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
		    }
		else
		    {
			/* Note that scripts are already disabled. */
			gsc_normal_string ("Glk transcript is already ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * transcript mode.
		 */
		gsc_normal_string ("Glk transcript is ");
		if (gsc_transcript_stream != NULL)
			gsc_normal_string (GSC_COMMAND_ON);
		else
			gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gsc_normal_string ("Glk transcript can be ");
		gsc_standout_string (GSC_COMMAND_ON);
		gsc_normal_string (", or ");
		gsc_standout_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
}


/*
 * gsc_command_inputlog()
 *
 * Turn game input logging on and off.
 */
static void
gsc_command_inputlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up an input log according to the argument given. */
	if (!sc_strcasecmp (argument, GSC_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if an input log is already active. */
		if (gsc_inputlog_stream != NULL)
		    {
			gsc_normal_string ("Glk input logging is already ");
			gsc_normal_string (GSC_COMMAND_ON);
			gsc_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for an input log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gsc_standout_string ("Glk input logging failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gsc_inputlog_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gsc_inputlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gsc_standout_string ("Glk input logging failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gsc_normal_string ("Glk input logging is now ");
		gsc_normal_string (GSC_COMMAND_ON);
		gsc_normal_string (".\n");
	    }
	else if (!sc_strcasecmp (argument, GSC_COMMAND_OFF))
	    {
		/* If an input log is active, close it. */
		if (gsc_inputlog_stream != NULL)
		    {
			glk_stream_close (gsc_inputlog_stream, NULL);
			gsc_inputlog_stream = NULL;

			/* Confirm action. */
			gsc_normal_string ("Glk input log is now ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current input log. */
			gsc_normal_string ("Glk input logging is already ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * input logging mode.
		 */
		gsc_normal_string ("Glk input logging is ");
		if (gsc_inputlog_stream != NULL)
			gsc_normal_string (GSC_COMMAND_ON);
		else
			gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gsc_normal_string ("Glk input logging can be ");
		gsc_standout_string (GSC_COMMAND_ON);
		gsc_normal_string (", or ");
		gsc_standout_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
}


/*
 * gsc_command_readlog()
 *
 * Set the game input log, to read input from a file.
 */
static void
gsc_command_readlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up a read log according to the argument given. */
	if (!sc_strcasecmp (argument, GSC_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a read log is already active. */
		if (gsc_readlog_stream != NULL)
		    {
			gsc_normal_string ("Glk read log is already ");
			gsc_normal_string (GSC_COMMAND_ON);
			gsc_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a read log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_Read, 0);
		if (fileref == NULL)
		    {
			gsc_standout_string ("Glk read log failed.\n");
			return;
		    }

		/*
		 * Reject the file reference if we're expecting to read
		 * from it, and the referenced file doesn't exist.
		 */
		if (!glk_fileref_does_file_exist (fileref))
		    {
			glk_fileref_destroy (fileref);
			gsc_standout_string ("Glk read log failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gsc_readlog_stream = glk_stream_open_file
					(fileref, filemode_Read, 0);
		if (gsc_readlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gsc_standout_string ("Glk read log failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gsc_normal_string ("Glk read log is now ");
		gsc_normal_string (GSC_COMMAND_ON);
		gsc_normal_string (".\n");
	    }
	else if (!sc_strcasecmp (argument, GSC_COMMAND_OFF))
	    {
		/* If a read log is active, close it. */
		if (gsc_readlog_stream != NULL)
		    {
			glk_stream_close (gsc_readlog_stream, NULL);
			gsc_readlog_stream = NULL;

			/* Confirm action. */
			gsc_normal_string ("Glk read log is now ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current read log. */
			gsc_normal_string ("Glk read log is already ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * read logging mode.
		 */
		gsc_normal_string ("Glk read log is ");
		if (gsc_readlog_stream != NULL)
			gsc_normal_string (GSC_COMMAND_ON);
		else
			gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gsc_normal_string ("Glk read log can be ");
		gsc_standout_string (GSC_COMMAND_ON);
		gsc_normal_string (", or ");
		gsc_standout_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
}


/*
 * gsc_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gsc_command_abbreviations (const char *argument)
{
	assert (argument != NULL);

	/* Set up abbreviation expansions according to the argument given. */
	if (!sc_strcasecmp (argument, GSC_COMMAND_ON))
	    {
		/* See if expansions are already on. */
		if (gsc_abbreviations_enabled)
		    {
			gsc_normal_string ("Glk abbreviation expansions"
					" are already ");
			gsc_normal_string (GSC_COMMAND_ON);
			gsc_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions on. */
		gsc_abbreviations_enabled = TRUE;
		gsc_normal_string ("Glk abbreviation expansions are now ");
		gsc_normal_string (GSC_COMMAND_ON);
		gsc_normal_string (".\n");
	    }
	else if (!sc_strcasecmp (argument, GSC_COMMAND_OFF))
	    {
		/* See if expansions are already off. */
		if (!gsc_abbreviations_enabled)
		    {
			gsc_normal_string ("Glk abbreviation expansions"
					" are already ");
			gsc_normal_string (GSC_COMMAND_OFF);
			gsc_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions off. */
		gsc_abbreviations_enabled = FALSE;
		gsc_normal_string ("Glk abbreviation expansions are now ");
		gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * expansion mode.
		 */
		gsc_normal_string ("Glk abbreviation expansions are ");
		if (gsc_abbreviations_enabled)
			gsc_normal_string (GSC_COMMAND_ON);
		else
			gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gsc_normal_string ("Glk abbreviation expansions can be ");
		gsc_standout_string (GSC_COMMAND_ON);
		gsc_normal_string (", or ");
		gsc_standout_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
}


/*
 * gsc_command_print_version_number()
 * gsc_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gsc_command_print_version_number (glui32 version)
{
	char		buffer[64];		/* Output buffer string. */

	/* Print out the three version number component parts. */
	sprintf (buffer, "%lu.%lu.%lu",
			(version & 0xFFFF0000) >> 16,
			(version & 0x0000FF00) >>  8,
			(version & 0x000000FF)      );
	gsc_normal_string (buffer);
}
static void
gsc_command_version (const char *argument)
{
	glui32		version;		/* Glk lib version number. */
	const char	*dummy;			/* Warning suppression. */

	/* Get the Glk library version number. */
	version = glk_gestalt (gestalt_Version, 0);

	/* Print the Glk library and port version numbers. */
	gsc_normal_string ("The Glk library version is ");
	gsc_command_print_version_number (version);
	gsc_normal_string (".\n");
	gsc_normal_string ("This is version ");
	gsc_command_print_version_number (GSC_PORT_VERSION);
	gsc_normal_string (" of the Glk SCARE port.\n");

	/* Suppress gcc compiler warning about unused argument. */
	dummy = argument;
}


/*
 * gsc_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gsc_command_commands (const char *argument)
{
	assert (argument != NULL);

	/* Set up command handling according to the argument given. */
	if (!sc_strcasecmp (argument, GSC_COMMAND_ON))
	    {
		/* Commands must already be on. */
		gsc_normal_string ("Glk commands are already ");
		gsc_normal_string (GSC_COMMAND_ON);
		gsc_normal_string (".\n");
	    }
	else if (!sc_strcasecmp (argument, GSC_COMMAND_OFF))
	    {
		/* The user has turned commands off. */
		gsc_commands_enabled = FALSE;
		gsc_normal_string ("Glk commands are now ");
		gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * command mode.
		 */
		gsc_normal_string ("Glk commands are ");
		if (gsc_commands_enabled)
			gsc_normal_string (GSC_COMMAND_ON);
		else
			gsc_normal_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gsc_normal_string ("Glk commands can be ");
		gsc_standout_string (GSC_COMMAND_ON);
		gsc_normal_string (", or ");
		gsc_standout_string (GSC_COMMAND_OFF);
		gsc_normal_string (".\n");
	    }
}


/* Escape introducer string. */
static const char	*GSC_COMMAND_ESCAPE		= "glk";

/* Small table of Glk subcommands and handler functions. */
struct gsc_command {
	const char	*command;		/* Glk subcommand. */
	void		(*handler) (const char *argument);
						/* Subcommand handler. */
	const int	takes_argument;		/* Argument flag. */
};
typedef const struct gsc_command*	gsc_commandref_t;
static const struct gsc_command		GSC_COMMAND_TABLE[] = {
	{ "script",		gsc_command_script,		TRUE  },
	{ "inputlog",		gsc_command_inputlog,		TRUE  },
	{ "readlog",		gsc_command_readlog,		TRUE  },
	{ "abbreviations",	gsc_command_abbreviations,	TRUE  },
	{ "version",		gsc_command_version,		FALSE },
	{ "commands",		gsc_command_commands,		TRUE  },
	{ NULL,			NULL,				FALSE }
};

/* List of whitespace command-argument separator characters. */
static const char	*GSC_COMMAND_WHITESPACE		= "\t ";


/*
 * gsc_command_dispatch()
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
gsc_command_dispatch (const char *command, const char *argument)
{
	gsc_commandref_t	entry;		/* Table search entry. */
	gsc_commandref_t	matched;	/* Matched table entry. */
	int			matches;	/* Count of command matches. */
	assert (command != NULL && argument != NULL);

	/*
	 * Search for the first unambiguous table command string matching
	 * the command passed in.
	 */
	matches = 0;
	matched = NULL;
	for (entry = GSC_COMMAND_TABLE;
				entry->command != NULL; entry++)
	    {
		if (!sc_strncasecmp (command,
					entry->command, strlen (command)))
		    {
			matches++;
			matched = entry;
		    }
	    }

	/* If the match was unambiguous, call the command handler. */
	if (matches == 1)
	    {
		gsc_normal_char ('\n');
		(matched->handler) (argument);

		/* Issue a brief warning if an argument was ignored. */
		if (!matched->takes_argument && strlen (argument) > 0)
		    {
			gsc_normal_string ("[The ");
			gsc_standout_string (matched->command);
			gsc_normal_string (" command ignores arguments.]\n");
		    }
	    }

	/* Return the count of matching table entries. */
	return matches;
}


/*
 * gsc_command_usage()
 *
 * On an empty, invalid, or ambiguous command, print out a list of valid
 * commands and perhaps some Glk status information.
 */
static void
gsc_command_usage (const char *command, int is_ambiguous)
{
	gsc_commandref_t	entry;	/* Command table iteration entry. */
	assert (command != NULL);

	/* Print a blank line separator. */
	gsc_normal_char ('\n');

	/* If the command isn't empty, indicate ambiguous or invalid. */
	if (strlen (command) > 0)
	    {
		gsc_normal_string ("The Glk command ");
		gsc_standout_string (command);
		if (is_ambiguous)
			gsc_normal_string (" is ambiguous.\n");
		else
			gsc_normal_string (" is not valid.\n");
	    }

	/* Print out a list of valid commands. */
	gsc_normal_string ("Glk commands are");
	for (entry = GSC_COMMAND_TABLE; entry->command != NULL; entry++)
	    {
		gsc_commandref_t	next;	/* Next command table entry. */

		next = entry + 1;
		gsc_normal_string (next->command != NULL ? " " : " and ");
		gsc_standout_string (entry->command);
		gsc_normal_string (next->command != NULL ? "," : ".\n");
	    }

	/* Write a note about abbreviating commands. */
	gsc_normal_string ("Glk commands may be abbreviated, as long as");
	gsc_normal_string (" the abbreviations are unambiguous.\n");

	/*
	 * If no command was given, call each command handler function with
	 * an empty argument to prompt each to report the current setting.
	 */
	if (strlen (command) == 0)
	    {
		gsc_normal_char ('\n');
		for (entry = GSC_COMMAND_TABLE;
					entry->command != NULL; entry++)
			(entry->handler) ("");
	    }
}


/*
 * gsc_skip_characters()
 *
 * Helper function for command escapes.  Skips over either whitespace or
 * non-whitespace in string, and returns the revised string pointer.
 */
static char *
gsc_skip_characters (char *string, int skip_whitespace)
{
	char	*result;			/* Return string pointer. */
	assert (string != NULL);

	/* Skip over leading characters of the specified type. */
	for (result = string; *result != '\0'; result++)
	    {
		int	is_whitespace;		/* Whitespace flag. */

		/* Break if encountering a character not the required type. */
		is_whitespace =
			(strchr (GSC_COMMAND_WHITESPACE, *result) != NULL);
		if ((skip_whitespace && !is_whitespace)
				|| (!skip_whitespace && is_whitespace))
			break;
	    }

	/* Return the revised pointer. */
	return result;
}


/*
 * gsc_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 */
static int
gsc_command_escape (char *string)
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
	temporary = gsc_skip_characters (string, TRUE);
	if (sc_strncasecmp (temporary, GSC_COMMAND_ESCAPE,
					strlen (GSC_COMMAND_ESCAPE)))
		return FALSE;

	/* Take a copy of the string, without any leading space. */
	string_copy = gsc_malloc (strlen (temporary) + 1);
	strcpy (string_copy, temporary);

	/* Find the subcommand; the word after the introducer. */
	command = gsc_skip_characters (string_copy
					+ strlen (GSC_COMMAND_ESCAPE), TRUE);

	/* Skip over command word, be sure it terminates with NUL. */
	temporary = gsc_skip_characters (command, FALSE);
	if (*temporary != '\0')
	    {
		*temporary = '\0';
		temporary++;
	    }

	/* Now find any argument data for the command. */
	argument = gsc_skip_characters (temporary, TRUE);

	/* Ensure that argument data also terminates with a NUL. */
	temporary = gsc_skip_characters (argument, FALSE);
	*temporary = '\0';

	/*
	 * Try to handle the command and argument as a Glk subcommand.  If
	 * it doesn't run unambiguously, print command usage.
	 */
	matches = gsc_command_dispatch (command, argument);
	if (matches != 1)
	    {
		if (matches == 0)
			gsc_command_usage (command, FALSE);
		else
			gsc_command_usage (command, TRUE);
	    }

	/* Done with the copy of the string. */
	free (string_copy);

	/* Return TRUE to indicate string contained a Glk command. */
	return TRUE;
}


/*---------------------------------------------------------------------*/
/*  Input functions                                                    */
/*---------------------------------------------------------------------*/

/* Quote used to suppress abbreviation expansion and local commands. */
static const char	GSC_QUOTED_INPUT		= '\'';

/* Definition of whitespace characters to skip over. */
static const char	*GSC_WHITESPACE			= "\t ";

/*
 * gsc_char_is_whitespace()
 *
 * Check for ASCII whitespace characters.  Returns TRUE if the character
 * qualifies as whitespace (NUL is not whitespace).
 */
static int
gsc_char_is_whitespace (char c)
{
	return (c != '\0' && strchr (GSC_WHITESPACE, c) != NULL);
}


/*
 * gsc_skip_leading_whitespace()
 *
 * Skip over leading whitespace, returning the address of the first non-
 * whitespace character.
 */
static char *
gsc_skip_leading_whitespace (char *string)
{
	char	*result;			/* Return string pointer. */
	assert (string != NULL);

	/* Move result over leading whitespace. */
	for (result = string; gsc_char_is_whitespace (*result); )
		result++;
	return result;
}


/* Table of single-character command abbreviations. */
struct gsc_abbreviation {
	const char	abbreviation;		/* Abbreviation character. */
	const char	*expansion;		/* Expansion string. */
};
typedef const struct gsc_abbreviation*		gsc_abbreviationref_t;
static const struct gsc_abbreviation		GSC_ABBREVIATIONS[] = {
	{ 'c',	"close"   },	{ 'g',	"again" },	{ 'i',	"inventory" },
	{ 'k',	"attack"  },	{ 'l',	"look"  },	{ 'p',	"open"      },
	{ 'q',	"quit"    },	{ 'r',	"drop"  },	{ 't',	"take"      },
	{ 'x',	"examine" },	{ 'y',	"yes"   },	{ 'z',	"wait"      },
	{ '\0',	NULL }
};

/*
 * gsc_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems.
 */
static void
gsc_expand_abbreviations (char *buffer, int size)
{
	char		*command;	/* Single-character command. */
	gsc_abbreviationref_t
			entry, match;	/* Table search entry, and match. */
	assert (buffer != NULL);

	/* Skip leading spaces to find command, and return if nothing. */
	command = gsc_skip_leading_whitespace (buffer);
	if (strlen (command) == 0)
		return;

	/* If the command is not a single letter one, do nothing. */
	if (strlen (command) > 1
			&& !gsc_char_is_whitespace (command[1]))
		return;

	/* Scan the abbreviations table for a match. */
	match = NULL;
	for (entry = GSC_ABBREVIATIONS; entry->expansion != NULL; entry++)
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
	if ((int) (strlen (buffer) + strlen (match->expansion) - 1) >= size)
		return;

	/* Replace the character with the full string. */
	memmove (command + strlen (match->expansion) - 1,
					command, strlen (command) + 1);
	memcpy (command, match->expansion, strlen (match->expansion));

	/* Provide feedback on the expansion. */
	gsc_standout_string ("[");
	gsc_standout_char   (match->abbreviation);
	gsc_standout_string (" -> ");
	gsc_standout_string (match->expansion);
	gsc_standout_string ("]\n");
}


/*
 * os_read_line()
 *
 * Read and return a line of player input.
 */
sc_bool
os_read_line (sc_char *buffer, sc_uint16 length)
{
	event_t		event;			/* Event buffer. */
	assert (buffer != NULL && length > 0);

	/*
	 * Ensure normal style, update the status line, and issue an
	 * input prompt.
	 */
	gsc_reset_glk_style ();
	gsc_status_notify ();
	glk_put_string (">");

	/*
	 * If we have an input log to read from, use that until it is
	 * exhausted.  On end of file, close the stream and resume input
	 * from line requests.
	 */
	if (gsc_readlog_stream != NULL)
	    {
		glui32		chars;		/* Characters read. */

		/* Get the next line from the log stream. */
		chars = glk_get_line_stream
				(gsc_readlog_stream, buffer, length);
		if (chars > 0)
		    {
			/* Echo the line just read in input style. */
			glk_set_style (style_Input);
			glk_put_buffer (buffer, chars);
			glk_set_style (style_Normal);

			/* Return this line as player input. */
			return TRUE;
		    }

		/*
		 * We're at the end of the log stream.  Close it, and then
		 * continue on to request a line from Glk.
		 */
		glk_stream_close (gsc_readlog_stream, NULL);
		gsc_readlog_stream = NULL;
	    }

	/* Set up the read buffer for the main window, and wait. */
	glk_request_line_event (gsc_main_window, buffer, length - 1, 0);
	gsc_event_wait (evtype_LineInput, &event);

	/* Terminate the input line with a NUL. */
	assert (event.val1 <= (glui32) length - 1);
	buffer[event.val1] = '\0';

	/*
	 * If neither abbreviations nor local commands are enabled, then
	 * return the data read above without further massaging.
	 */
	if (gsc_abbreviations_enabled
				|| gsc_commands_enabled)
	    {
		char		*command;	/* Command part of buffer. */

		/* Find the first non-space character in the input buffer. */
		command = gsc_skip_leading_whitespace (buffer);

		/*
		 * If the first non-space input character is a quote, bypass
		 * all abbreviation expansion and local command recognition,
		 * and use the unadulterated input, less introductory quote.
		 */
		if (command[0] == GSC_QUOTED_INPUT)
		    {
			/* Delete the quote with memmove(). */
			memmove (command, command + 1, strlen (command));
		    }
		else
		    {
			/* Check for, and expand, and abbreviated commands. */
			if (gsc_abbreviations_enabled)
				gsc_expand_abbreviations (buffer, length);

			/*
			 * Check for Glk port special commands, and if found
			 * then suppress the interpreter's use of this input.
			 */
			if (gsc_commands_enabled
					&& gsc_command_escape (buffer))
				return FALSE;
		    }
	    }

	/*
	 * If there is an input log active, log this input string to it.
	 * Note that by logging here we get any abbreviation expansions but
	 * we won't log glk special commands, nor any input read from a
	 * current open input log.
	 */
	if (gsc_inputlog_stream != NULL)
	    {
		glk_put_string_stream (gsc_inputlog_stream, buffer);
		glk_put_char_stream (gsc_inputlog_stream, '\n');
	    }

	/* Return TRUE since data buffered. */
	return TRUE;
}


/*
 * os_read_line_debug()
 *
 * Read and return a debugger command line.  There's no dedicated debugging
 * window, so this is just a call to the normal readline, with an additional
 * prompt.
 */
sc_bool
os_read_line_debug (sc_char *buffer, sc_uint16 length)
{
	gsc_reset_glk_style ();
	glk_put_string ("[SCARE debug]");
	return os_read_line (buffer, length);
}


/*
 * os_confirm()
 *
 * Confirm a game action with a yes/no prompt.
 */
sc_bool
os_confirm (sc_uint32 type)
{
	sc_char		response;		/* Glk response buffer. */

	/* Always allow game saves and hint display. */
	if (type == SC_CONF_SAVE
			|| type == SC_CONF_VIEW_HINTS)
		return TRUE;

	/* Ensure back to normal style. */
	gsc_reset_glk_style ();

	/* Prompt for the confirmation, based on the type. */
	if (type == GSC_CONF_SUBTLE_HINT)
		glk_put_string ("View the subtle hint for this topic");
	else if (type == GSC_CONF_UNSUBTLE_HINT)
		glk_put_string ("View the unsubtle hint for this topic");
	else if (type == GSC_CONF_CONTINUE_HINTS)
		glk_put_string ("Continue with hints");
	else
	    {
		glk_put_string ("Do you really want to ");
		switch (type)
		    {
		    case SC_CONF_QUIT:	  glk_put_string ("quit");	break;
		    case SC_CONF_RESTART: glk_put_string ("restart");	break;
		    case SC_CONF_SAVE:	  glk_put_string ("save");	break;
		    case SC_CONF_RESTORE: glk_put_string ("restore");	break;
		    case SC_CONF_VIEW_HINTS:
					  glk_put_string ("view hints");break;
		    default:		  glk_put_string ("do that");	break;
		    }
	    }
	glk_put_string ("? ");

	/* Update status, then loop until 'yes' or 'no' returned. */
	gsc_status_notify ();
	do
	    {
		event_t		event;		/* Event buffer. */

		/* Wait for a standard key, ignoring Glk special keys. */
		do
		    {
			glk_request_char_event (gsc_main_window);
			gsc_event_wait (evtype_CharInput, &event);
		    }
		while (event.val1 > UCHAR_MAX);
		response = glk_char_to_upper (event.val1);
	    }
	while (response != 'Y' && response != 'N');

	/* Echo the confirmation response, and a new line. */
	glk_set_style (style_Input);
	glk_put_string (response == 'Y' ? "Yes" : "No");
	glk_set_style (style_Normal);
	glk_put_char ('\n');

	/* Use a short delay on restarts, if confirmed. */
	if (type == SC_CONF_RESTART
				&& response == 'Y')
		gsc_short_delay ();

	/* Return TRUE if 'Y' was entered. */
	return (response == 'Y');
}


/*---------------------------------------------------------------------*/
/*  Event functions                                                    */
/*---------------------------------------------------------------------*/

/* Short delay before restarts; 1s, in 100ms segments. */
static const glui32	GSC_DELAY_TIMEOUT		= 100;
static const glui32	GSC_DELAY_TIMEOUTS_COUNT	= 10;

/*
 * gsc_short_delay()
 *
 * Delay for a short period; used before restarting a completed game,
 * to improve the display where 'r', or confirming restart, triggers
 * an otherwise immediate, and abrupt, restart.
 */
static void
gsc_short_delay (void)
{
	glui32		timeout;		/* Timeout count. */

	/* Ignore the call if the Glk doesn't have timers. */
	if (!glk_gestalt (gestalt_Timer, 0))
		return;

	/* Timeout in small chunks to minimize Glk jitter. */
	glk_request_timer_events (GSC_DELAY_TIMEOUT);
	for (timeout = 0;
		timeout < GSC_DELAY_TIMEOUTS_COUNT; timeout++)
	    {
		event_t		event;		/* Event buffer. */

		/* Wait for a timer event. */
		gsc_event_wait (evtype_Timer, &event);
	    }
	glk_request_timer_events (0);
}


/*
 * gsc_event_wait()
 *
 * Process Glk events until one of the expected type arrives.  Return the
 * event of that type.
 */
static void
gsc_event_wait (glui32 wait_type, event_t *event)
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
			gsc_status_redraw ();
			break;
		    }
		}
	while (event->type != wait_type);
}


/*---------------------------------------------------------------------*/
/*  File functions                                                     */
/*---------------------------------------------------------------------*/

/*
 * os_open_file()
 *
 * Open a file for save or restore, and return a Glk stream for the
 * opened file.
 */
void *
os_open_file (sc_bool is_save)
{
	glui32		usage, fmode;		/* Glk usage and mode. */
	frefid_t	fileref;		/* Glk fileref. */
	strid_t		stream;			/* Glk stream. */

	/* Set Glk usage and mode. */
	usage = fileusage_SavedGame | fileusage_BinaryMode;
	fmode = is_save ? filemode_Write : filemode_Read;

	/* Prompt player for the file's name or location. */
	fileref = glk_fileref_create_by_prompt (usage, fmode, 0);
	if (fileref == NULL)
		return NULL;

	/* If restoring and the file does not exist, fail the call. */
	if (!is_save
		&& !glk_fileref_does_file_exist (fileref))
	    {
		glk_fileref_destroy (fileref);
		return NULL;
	    }

	/* Open a stream for the file. */
	stream = glk_stream_open_file (fileref, fmode, 0);
	if (stream == NULL)
	    {
		glk_fileref_destroy (fileref);
		return NULL;
	    }

	/* Done with fileref; return stream. */
	glk_fileref_destroy (fileref);
	return stream;
}


/*
 * os_write_file()
 * os_read_file()
 *
 * Write/read the given buffered data to/from the open Glk stream.
 */
void
os_write_file (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	strid_t		stream = (strid_t) opaque;
	assert (opaque != NULL && buffer != NULL);

	/* Write data using Glk. */
	glk_put_buffer_stream (stream, (char *) buffer, length);
}

sc_uint16
os_read_file (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	strid_t		stream = (strid_t) opaque;
	assert (opaque != NULL && buffer != NULL);

	/* Read data using Glk, return byte count. */
	return glk_get_buffer_stream (stream, (char *) buffer, length);
}


/*
 * os_close_file()
 *
 * Close the opened Glk stream.
 */
void
os_close_file (void *opaque)
{
	strid_t		stream = (strid_t) opaque;
	assert (opaque != NULL);

	/* Close Glk stream. */
	glk_stream_close (stream, NULL);
}


/*---------------------------------------------------------------------*/
/*  main() and game setup                                              */
/*---------------------------------------------------------------------*/

/* Loading message flush delay timeout. */
static const glui32	GSC_LOADING_TIMEOUT	= 100;

/* Error message for setup errors. */
static char		*gsc_game_message	= NULL;

/* Enumerated game end options. */
enum gsc_end_option	{ GAME_RESTART, GAME_UNDO, GAME_QUIT };

/*
 * gsc_callback()
 *
 * Callback function for reading in game data; fills a buffer with TAF
 * file data from a Glk stream, and returns the byte count.
 */
static sc_uint16
gsc_callback (void *opaque, sc_byte *buffer, sc_uint16 length)
{
	strid_t		stream = (strid_t) opaque;

	/* Read and return byte count. */
	return glk_get_buffer_stream (stream, (char *) buffer, length);
}


/*
 * gsc_get_ending_option()
 *
 * Offer the option to restart, undo, or quit.  Returns the selected game
 * end option.  Called on game completion.
 */
static enum gsc_end_option
gsc_get_ending_option (void)
{
	sc_char		response;		/* Glk response buffer. */

	/* Ensure back to normal style. */
	gsc_reset_glk_style ();

	/* Prompt for restart, undo, or quit. */
	glk_put_string ("\nWould you like to RESTART, UNDO a turn, or QUIT? ");

	/* Update status, then loop until 'restart', 'undo' or 'quit'. */
	gsc_status_notify ();
	do
	    {
		event_t		event;		/* Event buffer. */

		/* Wait for a standard key, ignoring Glk special keys. */
		do
		    {
			glk_request_char_event (gsc_main_window);
			gsc_event_wait (evtype_CharInput, &event);
		    }
		while (event.val1 > UCHAR_MAX);
		response = glk_char_to_upper (event.val1);
	    }
	while (response != 'R' && response != 'U' && response != 'Q');

	/* Echo the confirmation response, and a new line. */
	glk_set_style (style_Input);
	switch (response)
	    {
	    case 'R':	glk_put_string ("Restart");		break;
	    case 'U':	glk_put_string ("Undo");		break;
	    case 'Q':	glk_put_string ("Quit");		break;
	    default:	gsc_fatal ("GLK: Invalid response encountered");
			glk_exit ();
	    }
	glk_set_style (style_Normal);
	glk_put_char ('\n');

	/* Return the appropriate value for response. */
	switch (response)
	    {
	    case 'R':	return GAME_RESTART;
	    case 'U':	return GAME_UNDO;
	    case 'Q':	return GAME_QUIT;
	    default:	gsc_fatal ("GLK: Invalid response encountered");
			glk_exit ();
	    }

	/* Unreachable; supplied to suppress compiler warning. */
	return GAME_QUIT;
}


/*
 * gsc_startup_code()
 * gsc_main
 *
 * Together, these functions take the place of the original main().  The
 * first one is called from the platform-specific startup_code(), which
 * should have already opening a Glk stream to the game and discerned the
 * trace flags, if any; it parses the game from the Glk stream passed in,
 * and sets it in gsc_game, or sets an error message in gsc_game_message.
 * It always returns TRUE, no matter what the outcome of loading the game
 * is -- error handling is the province of gsc_main().
 *
 * The second one does the real work of running the game, or, if it is
 * NULL, opens a Glk window and reports the error in gsc_game_message.
 */
static int
gsc_startup_code (strid_t stream, sc_uint32 trace_flags,
			sc_bool enable_debugger)
{
	winid_t		window;			/* Startup temporary windowp. */
	assert (stream != NULL);

	/* Open a temporary Glk main window. */
	window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
	if (window != NULL)
	    {
		/* Clear and initialize the temporary window. */
		glk_window_clear (window);
		glk_set_window (window);
		glk_set_style (style_Normal);

		/*
		 * Display a brief loading game message; here we have to use
		 * a timeout to ensure that the text is flushed to Glk.
		 */
		glk_put_string ("Loading game...\n");
		if (glk_gestalt (gestalt_Timer, 0))
		    {
			event_t		event;		/* Event buffer. */

			glk_request_timer_events (GSC_LOADING_TIMEOUT);
			do
			    {
				glk_select (&event);
			    }
			while (event.type != evtype_Timer);
			glk_request_timer_events (0);
		    }
	    }

	/* Try to create a SCARE game reference from the TAF file. */
	gsc_game = sc_game_from_callback (gsc_callback, stream, trace_flags);
	if (gsc_game == NULL)
	    {
		gsc_game = NULL;
		gsc_game_message = "Unable to load an Adrift game from the"
				" requested file.";
	    }
	glk_stream_close (stream, NULL);

	/* Set up game debugging if the game was created successfully. */
	if (gsc_game != NULL)
		sc_set_game_debugger_enabled (gsc_game, enable_debugger);

	/* Close the temporary window. */
	if (window != NULL)
		glk_window_close (window, NULL);

	/* Set title of game */
#ifdef GARGLK
	garglk_set_story_name(sc_get_game_name(gsc_game));
#endif

	/* Game set up, perhaps successfully. */
	return TRUE;
}

static void
gsc_main (void)
{
	sc_bool		running;		/* Player actively playing. */

	/* Ensure SCARE types have the right sizes. */
	if (sizeof (sc_byte) != 1 || sizeof (sc_char) != 1
			|| sizeof (sc_uint16) != 2 || sizeof (sc_int16) != 2
			|| sizeof (sc_uint32) != 4 || sizeof (sc_int32) != 4)
	    {
		gsc_fatal ("GLK: Types sized incorrectly,"
					" recompilation is needed");
		glk_exit ();
	    }

	/* Create the Glk window, and set its stream as the current one. */
	gsc_main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
	if (gsc_main_window == NULL)
	    {
		gsc_fatal ("GLK: Can't open main window");
		glk_exit ();
	    }
	glk_window_clear (gsc_main_window);
	glk_set_window (gsc_main_window);
	glk_set_style (style_Normal);

	/* If there's a problem with the game file, complain now. */
	if (gsc_game == NULL)
	    {
		assert (gsc_game_message != NULL);
		gsc_header_string ("Glk SCARE Error\n\n");
		gsc_normal_string (gsc_game_message);
		gsc_normal_char ('\n');
		glk_exit ();
	    }

	/* Try to create a one-line status window.  We can live without it. */
	gsc_status_window = glk_window_open (gsc_main_window,
					winmethod_Above|winmethod_Fixed,
					1, wintype_TextGrid, 0);

	/* Repeat the game until no more restarts requested. */
	running = TRUE;
	while (running)
	    {
		/* Run the game until it ends, or the user quits. */
		gsc_status_notify ();
		sc_interpret_game (gsc_game);

		/*
		 * If the game did not complete, the user quit explicitly,
		 * so leave the game repeat loop.
		 */
		if (!sc_has_game_completed (gsc_game))
		    {
			running = FALSE;
			break;
		    }

		/*
		 * Get user selection of restart, undo a turn, or quit
		 * completed game.  If undo is unavailable (this should not
		 * be possible), degrade to restart.
		 */
		switch (gsc_get_ending_option ())
		    {
		    case GAME_RESTART:
			gsc_short_delay ();
			sc_restart_game (gsc_game);
			break;

		    case GAME_UNDO:
			if (sc_is_game_undo_available (gsc_game))
			    {
				sc_undo_game_turn (gsc_game);
				gsc_normal_string ("The previous turn has"
							" been undone.\n");
			    }
			else
			    {
				gsc_normal_string ("Sorry, no undo is"
							" available.\n");
				gsc_short_delay ();
				sc_restart_game (gsc_game);
			    }
			break;

		    case GAME_QUIT:
			running = FALSE;
			break;
		    }
	    }

	/* All done -- release game resources. */
	sc_free_game (gsc_game);
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the real interpreter      */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int		gsc_startup_called	= FALSE;
static int		gsc_main_called		= FALSE;

/*
 * glk_main()
 *
 * Main entry point for Glk.  Here, all startup is done, and we call our
 * function to run the game, or to report errors if gsc_game_message is set.
 */
void
glk_main (void)
{
	assert (gsc_startup_called && !gsc_main_called);
	gsc_main_called = TRUE;

	/* Call the generic interpreter main function. */
	gsc_main ();
}


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the UNIX platform                     */
/*---------------------------------------------------------------------*/
#ifdef __unix

#include "glkstart.h"

/*
 * Glk arguments for UNIX versions of the Glk interpreter.
 */
glkunix_argumentlist_t glkunix_arguments[] = {
	{ (char *) "-nc", glkunix_arg_NoValue,
	    (char *) "-nc        No local handling for Glk special commands" },
	{ (char *) "-na", glkunix_arg_NoValue,
	    (char *) "-na        Turn off abbreviation expansions" },
#ifdef LINUX_GRAPHICS
	{ (char *) "-ng", glkunix_arg_NoValue,
	    (char *) "-ng        Turn off attempts at game graphics" },
#endif
	{ (char *) "", glkunix_arg_ValueCanFollow,
	    (char *) "filename   game to run" },
	{ NULL, glkunix_arg_End, NULL }
};


/*
 * glkunix_startup_code()
 *
 * Startup entry point for UNIX versions of Glk interpreter.  Glk will call
 * glkunix_startup_code() to pass in arguments.  On startup, parse arguments
 * and open a Glk stream to the game, then call the generic gsc_startup_code()
 * to build a game from the stream.  On error, set the message in
 * gsc_game_message; the core gsc_main() will report it when it's called.
 */
int
glkunix_startup_code (glkunix_startup_t *data)
{
	int		argc	= data->argc;	/* Virtual main argc. */
	sc_char		**argv	= data->argv;	/* Virtual main argv[]. */
	int		argv_index;		/* Argument iterator. */
	strid_t		stream;			/* TAF file read stream. */
	sc_uint32	trace_flags;		/* SCARE trace flags. */
	sc_bool		enable_debugger;	/* SCARE debugger control. */
	assert (!gsc_startup_called);
	gsc_startup_called = TRUE;

#ifdef GARGLK
	garglk_set_program_name("SCARE " SCARE_VERSION);
	garglk_set_program_info("SCARE " SCARE_VERSION
		" by Simon Baldwin and Mark J. Tilford");
#endif

	/* Handle command line arguments. */
	for (argv_index = 1;
		argv_index < argc && argv[argv_index][0] == '-'; argv_index++)
	    {
		if (strcmp (argv[argv_index], "-nc") == 0)
		    {
			gsc_commands_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[argv_index], "-na") == 0)
		    {
			gsc_abbreviations_enabled = FALSE;
			continue;
		    }
#ifdef LINUX_GRAPHICS
		if (strcmp (argv[argv_index], "-ng") == 0)
		    {
			gsclinux_graphics_enabled = FALSE;
			continue;
		    }
#endif
		return FALSE;
	    }

	/* On invalid usage, set a complaint message and return. */
	if (argv_index != argc - 1)
	    {
		gsc_game = NULL;
		if (argv_index < argc - 1)
			gsc_game_message = "More than one game file"
					" was given on the command line.";
		else
			gsc_game_message = "No game file was given"
					" on the command line.";
		return TRUE;
	    }

	/* Open a stream to the TAF file, complain if this fails. */
	stream = glkunix_stream_open_pathname (argv[argv_index], FALSE, 0);
	if (stream == NULL)
	    {
		gsc_game = NULL;
		gsc_game_message = "Unable to open the requested game file.";
		return TRUE;
	    }

	/* Set SCARE trace flags and enable debugger from the environment. */
	if (getenv ("SC_TRACE_FLAGS") != NULL)
		trace_flags = strtoul (getenv ("SC_TRACE_FLAGS"), NULL, 0);
	else
		trace_flags = 0;
	enable_debugger = (getenv ("SC_DEBUGGER_ENABLED") != NULL);

#ifdef LINUX_GRAPHICS
	/* Note the path to the game file for graphics extraction. */
	gsclinux_game_file = argv[argv_index];
#endif

	/* Use the generic startup code to complete startup. */
	return gsc_startup_code (stream, trace_flags, enable_debugger);
}
#endif /* __unix */


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the Windows platform                  */
/*---------------------------------------------------------------------*/
#ifdef _WIN32

#include <windows.h>

#include "WinGlk.h"
#include "resource.h"

/* Windows constants and external definitions. */
static const unsigned int	GSCWIN_GLK_INIT_VERSION	= 0x601;
extern int			InitGlk (unsigned int iVersion);

/*
 * WinMain()
 *
 * Entry point for all Glk applications.
 */
int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
			LPSTR lpCmdLine, int nCmdShow)
{
	/* Attempt to initialize both the Glk library and SCARE. */
	if (!(InitGlk (GSCWIN_GLK_INIT_VERSION)
			&& winglk_startup_code (lpCmdLine)))
		return 0;

	/* Run the application; no return from this routine. */
	glk_main ();
	glk_exit ();
	return 0;
}


/*
 * winglk_startup_code()
 *
 * Startup entry point for Windows versions of Glk interpreter.
 */
int
winglk_startup_code (const char* cmdline)
{
	const char*	filename;		/* Game filename. */
	frefid_t	fileref;		/* Glk fileref to filename. */
	strid_t		stream;			/* Glk stream for fileref. */
	sc_uint32	trace_flags;		/* SCARE trace flags. */
	sc_bool		enable_debugger;	/* SCARE debugger control. */
	assert (!gsc_startup_called);
	gsc_startup_called = TRUE;

	/* Set up application and window. */
	winglk_app_set_name ("Scare");
	winglk_set_menu_name ("&Scare");
	winglk_window_set_title ("Scare Adrift Interpreter");
	winglk_set_about_text ("Windows Scare 1.3.3-WinGlk1.25");
	winglk_set_gui (IDI_SCARE);

	/* Open a stream to the game. */
	filename = winglk_get_initial_filename (cmdline,
			"Select an Adrift game to run",
			"Adrift Files (.taf)|*.taf;All Files (*.*)|*.*||");
	if (filename == NULL)
		return 0;

	fileref = glk_fileref_create_by_name
				(fileusage_BinaryMode|fileusage_Data,
				(char*) filename, 0);
	if (fileref == NULL)
		return 0;

	stream = glk_stream_open_file (fileref, filemode_Read, 0);
	glk_fileref_destroy (fileref);
	if (stream == NULL)
		return 0;

	/* Set trace and debugger flags. */
	if (getenv ("SC_TRACE_FLAGS") != NULL)
		trace_flags = strtoul (getenv ("SC_TRACE_FLAGS"), NULL, 0);
	else
		trace_flags = 0;
	enable_debugger = (getenv ("SC_DEBUGGER_ENABLED") != NULL);

	/* Use the generic startup code to complete startup. */
	return gsc_startup_code (stream, trace_flags, enable_debugger);
}
#endif /* _WIN32 */

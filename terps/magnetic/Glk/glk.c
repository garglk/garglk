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
 * Glk interface for Magnetic Scrolls 2.2
 * --------------------------------------
 *
 * This module contains the the Glk porting layer for the Magnetic
 * Scrolls interpreter.  It defines the Glk arguments list structure,
 * the entry points for the Glk library framework to use, and all
 * platform-abstracted I/O to link to Glk's I/O.
 *
 * The following items are omitted from this Glk port:
 *
 *  o Glk tries to assert control over _all_ file I/O.  It's just too
 *    disruptive to add it to existing code, so for now, the Magnetic
 *    Scrolls interpreter is still dependent on stdio and the like.
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

#include "defs.h"

#include "glk.h"

#if TARGET_OS_MAC
#	include "macglk_startup.h"
#else
#	include "glkstart.h"
#endif


/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous other stuff                        */
/*---------------------------------------------------------------------*/

/* Glk Magnetic Scrolls port version number. */
static const glui32	GMS_PORT_VERSION		= 0x00010504;

/*
 * We use a maximum of five Glk windows, one for status, one for pictures,
 * two for hints, and one for everything else.  The status and pictures
 * windows may be NULL, depending on user selections and the capabilities
 * of the Glk library.  The hints windows will normally be NULL, except
 * when in the hints subsystem.
 */
static winid_t		gms_main_window			= NULL;
static winid_t		gms_status_window		= NULL;
static winid_t		gms_graphics_window		= NULL;
static winid_t		gms_hint_menu_window		= NULL;
static winid_t		gms_hint_text_window		= NULL;

/*
 * Transcript stream and input log.  These are NULL if there is no current
 * collection of these strings.
 */
static strid_t		gms_transcript_stream		= NULL;
static strid_t		gms_inputlog_stream		= NULL;

/* Input read log stream, for reading back an input log. */
static strid_t		gms_readlog_stream		= NULL;

/* Note about whether graphics is possible, or not. */
static int		gms_graphics_possible		= TRUE;

/* Options that may be turned off or set by command line flags. */
static int		gms_graphics_enabled		= TRUE;
static enum		{ GAMMA_OFF, GAMMA_NORMAL, GAMMA_HIGH }
			gms_gamma_mode			= GAMMA_NORMAL;
static int		gms_animation_enabled		= TRUE;
static int		gms_prompt_enabled		= TRUE;
static int		gms_abbreviations_enabled	= TRUE;
static int		gms_commands_enabled		= TRUE;

/* Magnetic Scrolls standard input prompt string. */
static const char	*GMS_INPUT_PROMPT		= ">";

/* Forward declaration of event wait function. */
static void		gms_event_wait (glui32 wait_type, event_t *event);

/*---------------------------------------------------------------------*/
/*  Glk arguments list                                                 */
/*---------------------------------------------------------------------*/

#if !TARGET_OS_MAC
glkunix_argumentlist_t glkunix_arguments[] = {
    { (char *) "-nc", glkunix_arg_NoValue,
	(char *) "-nc        No local handling for Glk special commands" },
    { (char *) "-na", glkunix_arg_NoValue,
	(char *) "-na        Turn off abbreviation expansions" },
    { (char *) "-np", glkunix_arg_NoValue,
	(char *) "-np        Turn off pictures" },
    { (char *) "-ng", glkunix_arg_NoValue,
	(char *) "-ng        Turn off automatic gamma correction on pictures" },
    { (char *) "-nx", glkunix_arg_NoValue,
	(char *) "-nx        Turn off picture animations" },
    { (char *) "-ne", glkunix_arg_NoValue,
	(char *) "-ne        Turn off additional interpreter prompt" },
    { (char *) "", glkunix_arg_ValueCanFollow,
	(char *) "filename   game to run" },
{ NULL, glkunix_arg_End, NULL }
};
#endif


/*---------------------------------------------------------------------*/
/*  Glk port utility functions                                         */
/*---------------------------------------------------------------------*/

/*
 * gms_fatal()
 *
 * Fatal error handler.  The function returns, expecting the caller to
 * abort() or otherwise handle the error.
 */
void
gms_fatal (const char *string)
{
	/*
	 * If the failure happens too early for us to have a window, print
	 * the message to stderr.
	 */
	if (gms_main_window == NULL)
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

	/*
	 * Cancel all possible pending window input events, and close
	 * any hints windows that may be overlaying the main window.
	 */
	glk_cancel_line_event (gms_main_window, NULL);
	glk_cancel_char_event (gms_main_window);
	if (gms_hint_menu_window != NULL)
	    {
		glk_cancel_char_event (gms_hint_menu_window);
		glk_window_close (gms_hint_menu_window, NULL);
	    }
	if (gms_hint_text_window != NULL)
	    {
		glk_cancel_char_event (gms_hint_text_window);
		glk_window_close (gms_hint_text_window, NULL);
	    }

	/* Print a message indicating the error, and exit. */
	glk_set_window (gms_main_window);
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
 * gms_malloc()
 * gms_realloc()
 *
 * Non-failing malloc and realloc; call gms_fatal and exit if memory
 * allocation fails.
 */
static void *
gms_malloc (size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Malloc, and call gms_fatal if the malloc fails. */
	pointer = malloc (size);
	if (pointer == NULL)
	    {
		gms_fatal ("GLK: Out of system memory");
		glk_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}

static void *
gms_realloc (void *ptr, size_t size)
{
	void		*pointer;		/* Return value pointer. */

	/* Realloc, and call gms_fatal() if the realloc fails. */
	pointer = realloc (ptr, size);
	if (pointer == NULL)
	    {
		gms_fatal ("GLK: Out of system memory");
		glk_exit ();
	    }

	/* Return the allocated pointer. */
	return pointer;
}


/*
 * gms_strncasecmp()
 * gms_strcasecmp()
 *
 * Strncasecmp and strcasecmp are not ANSI functions, so here are local
 * definitions to do the same jobs.
 */
static int
gms_strncasecmp (const char *s1, const char *s2, size_t n)
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
gms_strcasecmp (const char *s1, const char *s2)
{
	size_t		s1len, s2len;		/* String lengths. */
	int		result;			/* Result of strncasecmp. */

	/* Note the string lengths. */
	s1len = strlen (s1);
	s2len = strlen (s2);

	/* Compare first to shortest length, return any definitive result. */
	result = gms_strncasecmp (s1, s2, (s1len < s2len) ? s1len : s2len);
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

/*
 * If we're working with a very stripped down, old, or lazy Glk library
 * that neither offers Glk graphics nor graphics stubs functions, here
 * we define our own stubs, to avoid link-time errors.
 */
#ifndef GLK_MODULE_IMAGE
static glui32	glk_image_draw (winid_t win, glui32 image,
			glsi32 val1, glsi32 val2)	 { return FALSE; }
static glui32	glk_image_draw_scaled (winid_t win, glui32 image, 
			glsi32 val1, glsi32 val2,
			glui32 width, glui32 height)	 { return FALSE; }
static glui32	glk_image_get_info (glui32 image,
			glui32 *width, glui32 *height)	 { return FALSE; }
static void	glk_window_flow_break (winid_t win)			{}
static void	glk_window_erase_rect (winid_t win, 
			glsi32 left, glsi32 top,
			glui32 width, glui32 height)			{}
static void	glk_window_fill_rect (winid_t win, glui32 color, 
			glsi32 left, glsi32 top,
			glui32 width, glui32 height)			{}
static void	glk_window_set_background_color (winid_t win,
			glui32 color)					{}
#endif


/*---------------------------------------------------------------------*/
/*  Glk port CRC functions                                             */
/*---------------------------------------------------------------------*/

/* CRC table initialization magic number, and all-ones value. */
static const glui32	GMS_CRC_MAGIC			= 0xEDB88320;
static const glui32	GMS_CRC_ALLONES			= 0xFFFFFFFF;


/*
 * gms_update_crc()
 *
 * Update a running CRC with the bytes buffer[0..length-1] -- the CRC should
 * be initialized to all 1's, and the transmitted value is the 1's complement
 * of the final running CRC.
 *
 * This algorithm is taken from the PNG specification, version 1.0.
 */
static glui32
gms_update_crc (glui32 crc, const unsigned char *buffer, size_t length)
{
	static int	initialized	= FALSE;/* First call flag. */
	static glui32	crc_table[UCHAR_MAX+1];	/* CRC table for all bytes. */

	glui32		c;			/* Updated CRC, for return. */
	size_t		index;			/* Buffer index. */

	/* Build the CRC lookup table if this is the first call. */
	if (!initialized)
	    {
		int	n, k;			/* General loop indexes. */

		/* Create a table entry for each byte value. */
		for (n = 0; n < UCHAR_MAX + 1; n++)
		    {
			c = (glui32) n;
			for (k = 0; k < CHAR_BIT; k++)
			    {
				if (c & 1)
					c = GMS_CRC_MAGIC ^ (c >> 1);
				else
					c = c >> 1;
			    }
			crc_table[ n ] = c;
		    }

		/* Note the table is built. */
		initialized = TRUE;
	    }

	/* Update the CRC with each character in the buffer. */
	c = crc;
	for (index = 0; index < length; index++)
		c = crc_table[ (c ^ buffer[ index ]) & UCHAR_MAX ]
							^ (c >> CHAR_BIT);

	/* Return the updated CRC. */
	return c;
}


/*
 * gms_buffer_crc()
 *
 * Return the CRC of the bytes buffer[0..length-1].
 */
static glui32
gms_buffer_crc (const unsigned char *buffer, size_t length)
{
	glui32		c;			/* CRC intermediate value. */

	/* Calculate and return the CRC. */
	c = gms_update_crc (GMS_CRC_ALLONES, buffer, length);
	return c ^ GMS_CRC_ALLONES;
}


/*---------------------------------------------------------------------*/
/*  Glk port picture functions                                         */
/*---------------------------------------------------------------------*/

/*
 * Color conversions lookup tables, and a word about gamma corrections.
 *
 * When uncorrected, some game pictures can look dark (Corruption, Won-
 * derland), whereas others look just fine (Guild Of Thieves, Jinxter).
 *
 * The standard general-purpose gamma correction is around 2.1, with
 * specific values, normally, of 2.5-2.7 for IBM PC systems, and 1.8 for
 * Macintosh.  However, applying even the low end of this range can make
 * some pictures look washed out, yet improve others nicely.
 *
 * To try to solve this, here we'll set up a precalculated table with
 * discrete gamma values.  On displaying a picture, we'll try to find a
 * gamma correction that seems to offer a reasonable level of contrast
 * for the picture.
 *
 * Here's an AWK script to create the gamma table:
 *
 * BEGIN { max=255.0; step=max/7.0
 *         for (gamma=0.9; gamma<=2.7; gamma+=0.05) {
 *             printf "{ \"%2.2f\", {   0, ", gamma
 *             for (i=1; i<8; i++) {
 *                 printf "%3.0f", (((step*i / max) ^ (1.0/gamma)) * max)
 *                 printf "%s", (i<7) ? ", " : " "
 *             }
 *             printf "}, "
 *             printf "%s },\n", (gamma>0.99 && gamma<1.01) ? "FALSE" : "TRUE "
 *         } }
 *
 */
struct gms_gamma {
	const char     		*level;		/* Gamma correction level. */
	const unsigned char	table[8];	/* Color lookup table. */
	const int      		corrected;	/* Flag if non-linear. */
};
typedef const struct gms_gamma*		gms_gammaref_t;
static const struct gms_gamma		GMS_GAMMA_TABLE[] = {
	{ "0.90", {   0,  29,  63,  99, 137, 175, 215, 255 }, TRUE  },
	{ "0.95", {   0,  33,  68, 105, 141, 179, 217, 255 }, TRUE  },
	{ "1.00", {   0,  36,  73, 109, 146, 182, 219, 255 }, FALSE },
	{ "1.05", {   0,  40,  77, 114, 150, 185, 220, 255 }, TRUE  },
	{ "1.10", {   0,  43,  82, 118, 153, 188, 222, 255 }, TRUE  },
	{ "1.15", {   0,  47,  86, 122, 157, 190, 223, 255 }, TRUE  },
	{ "1.20", {   0,  50,  90, 126, 160, 193, 224, 255 }, TRUE  },
	{ "1.25", {   0,  54,  94, 129, 163, 195, 225, 255 }, TRUE  },
	{ "1.30", {   0,  57,  97, 133, 166, 197, 226, 255 }, TRUE  },
	{ "1.35", {   0,  60, 101, 136, 168, 199, 227, 255 }, TRUE  },
	{ "1.40", {   0,  64, 104, 139, 171, 201, 228, 255 }, TRUE  },
	{ "1.45", {   0,  67, 107, 142, 173, 202, 229, 255 }, TRUE  },
	{ "1.50", {   0,  70, 111, 145, 176, 204, 230, 255 }, TRUE  },
	{ "1.55", {   0,  73, 114, 148, 178, 205, 231, 255 }, TRUE  },
	{ "1.60", {   0,  76, 117, 150, 180, 207, 232, 255 }, TRUE  },
	{ "1.65", {   0,  78, 119, 153, 182, 208, 232, 255 }, TRUE  },
	{ "1.70", {   0,  81, 122, 155, 183, 209, 233, 255 }, TRUE  },
	{ "1.75", {   0,  84, 125, 157, 185, 210, 233, 255 }, TRUE  },
	{ "1.80", {   0,  87, 127, 159, 187, 212, 234, 255 }, TRUE  },
	{ "1.85", {   0,  89, 130, 161, 188, 213, 235, 255 }, TRUE  },
	{ "1.90", {   0,  92, 132, 163, 190, 214, 235, 255 }, TRUE  },
	{ "1.95", {   0,  94, 134, 165, 191, 215, 236, 255 }, TRUE  },
	{ "2.00", {   0,  96, 136, 167, 193, 216, 236, 255 }, TRUE  },
	{ "2.05", {   0,  99, 138, 169, 194, 216, 237, 255 }, TRUE  },
	{ "2.10", {   0, 101, 140, 170, 195, 217, 237, 255 }, TRUE  },
	{ "2.15", {   0, 103, 142, 172, 197, 218, 237, 255 }, TRUE  },
	{ "2.20", {   0, 105, 144, 173, 198, 219, 238, 255 }, TRUE  },
	{ "2.25", {   0, 107, 146, 175, 199, 220, 238, 255 }, TRUE  },
	{ "2.30", {   0, 109, 148, 176, 200, 220, 238, 255 }, TRUE  },
	{ "2.35", {   0, 111, 150, 178, 201, 221, 239, 255 }, TRUE  },
	{ "2.40", {   0, 113, 151, 179, 202, 222, 239, 255 }, TRUE  },
	{ "2.45", {   0, 115, 153, 180, 203, 222, 239, 255 }, TRUE  },
	{ "2.50", {   0, 117, 154, 182, 204, 223, 240, 255 }, TRUE  },
	{ "2.55", {   0, 119, 156, 183, 205, 223, 240, 255 }, TRUE  },
	{ "2.60", {   0, 121, 158, 184, 206, 224, 240, 255 }, TRUE  },
	{ "2.65", {   0, 122, 159, 185, 206, 225, 241, 255 }, TRUE  },
	{ "2.70", {   0, 124, 160, 186, 207, 225, 241, 255 }, TRUE  },
	{ NULL,   {   0,   0,   0,   0,   0,   0,   0,   0 }, FALSE }
};

/* R,G,B color triple definition. */
typedef struct {
	int	red, green, blue;		/* Color attributes. */
} gms_rgb_t;
typedef	gms_rgb_t*		gms_rgbref_t;

/*
 * Weighting values for calculating the luminance of a color.  There are
 * two commonly used sets of values for these -- 299,587,114, taken from
 * NTSC (Never The Same Color) 1953 standards, and 212,716,72, which is the
 * set that modern CRTs tend to match.  The NTSC ones seem to give the best
 * subjective results.
 */
static const gms_rgb_t	GMS_LUMINANCE_WEIGHTS		= { 299, 587, 114 };

/*
 * Maximum number of regions to consider in a single repaint pass.  A
 * couple of hundred seems to strike the right balance between not too
 * sluggardly picture updates, and responsiveness to input during graphics
 * rendering, when combined with short timeouts.
 */
static const int	GMS_REPAINT_LIMIT		= 256;

/*
 * Graphics timeout; we like an update call after this period (ms).  In
 * practice, this timeout may actually be shorter than the time taken
 * to reach the limit on repaint regions, but because Glk guarantees that
 * user interactions (in this case, line events) take precedence over
 * timeouts, this should be okay; we'll still see a game that responds to
 * input each time the background repaint function yields.
 *
 * Setting this value is tricky.  We'd like it to be the shortest possible
 * consistent with getting other stuff done, say 10ms.  However, Xglk has
 * a granularity of 50ms on checking for timeouts, as it uses a 1/20s
 * timeout on X select.  This means that the shortest timeout we'll ever
 * get from Xglk will be 50ms, so there's no point in setting this shorter
 * than that.  With luck, other Glk libraries will be more efficient than
 * this, and can give us higher timer resolution; we'll set 50ms here, and
 * hope that no other Glk library is worse.
 */
static const glui32	GMS_GRAPHICS_TIMEOUT		= 50;

/*
 * Count of timeouts to wait in between animation paints, and to wait on
 * repaint request.  Waiting for 2 timeouts of around 50ms, gets us to the
 * 100ms recommended animation frame rate.  Waiting after a repaint smooths
 * the display where the frame is being resized, by helping to avoid
 * graphics output while more resize events are received; around 1/2 second
 * seems okay.
 */
static const int	GMS_GRAPHICS_ANIMATION_WAIT	= 2;
static const int	GMS_GRAPHICS_REPAINT_WAIT	= 10;

/* Pixel size multiplier for image size scaling. */
static const int	GMS_GRAPHICS_PIXEL		= 2;

/* Proportion of the display to use for graphics. */
static const glui32	GMS_GRAPHICS_PROPORTION		= 60;

/*
 * Border and shading control.  For cases where we can't detect the back-
 * ground color of the main window, there's a default, white, background.
 * Bordering is black, with a 1 pixel border, 2 pixel shading, and 8 steps
 * of shading fade.
 */
static const glui32	GMS_GRAPHICS_DEFAULT_BACKGROUND	= 0x00FFFFFF;
static const glui32	GMS_GRAPHICS_BORDER_COLOR	= 0x00000000;
static const int	GMS_GRAPHICS_BORDER		= 1;
static const int	GMS_GRAPHICS_SHADING		= 2;
static const int	GMS_GRAPHICS_SHADE_COLOR	= 0x00707070;

/*
 * Guaranteed unused pixel value.  This value is used to fill the on-screen
 * buffer on new pictures or repaints, resulting in a full paint of all
 * pixels since no off-screen, real picture, pixel will match it.
 */
static const int	GMS_GRAPHICS_UNUSED_PIXEL	= 0xFF;

/*
 * The current picture bitmap being displayed, its width, height, palette,
 * animation flag, and picture id.
 */
enum {			GMS_PALETTE_SIZE		= 16 };
static type8		*gms_graphics_bitmap		= NULL;
static type16		gms_graphics_width		= 0;
static type16		gms_graphics_height		= 0;
static type16		gms_graphics_palette[GMS_PALETTE_SIZE];
						/*	= { 0, ... }; */
static type8		gms_graphics_animated		= FALSE;
static type32		gms_graphics_picture		= 0;

/*
 * Flags set on new picture, and on resize or arrange events, and a flag
 * to indicate whether background repaint is stopped or active.
 */
static int		gms_graphics_new_picture	= FALSE;
static int		gms_graphics_repaint		= FALSE;
static int		gms_graphics_active		= FALSE;

/* Flag to try to monitor the state of interpreter graphics. */
static int		gms_graphics_interpreter	= FALSE;

/*
 * Pointer to the two graphics buffers, one the off-screen representation
 * of pixels, and the other tracking on-screen data.  These are temporary
 * graphics malloc'ed memory, and should be free'd on exit.
 */
static type8		*gms_graphics_off_screen	= NULL;
static type8		*gms_graphics_on_screen		= NULL;

/*
 * Pointer to the current active gamma table entry.  Because of the way
 * it's queried, this may not be NULL, otherwise we risk a race, with
 * admittedly a very low probability, with the updater.  So, it's init-
 * ialized instead to the gamma table.  The real value in use is inserted
 * on the first picture update timeout call for a new picture.
 */
static gms_gammaref_t	gms_graphics_current_gamma	= GMS_GAMMA_TABLE;

/*
 * The number of colors used in the palette by the current picture.  This
 * value is also at risk of a race with the updater, so it too has a mild
 * lie for a default value.
 */
static int		gms_graphics_color_count	= GMS_PALETTE_SIZE;


/*
 * gms_graphics_open()
 *
 * If it's not open, open the graphics window.  Returns TRUE if graphics
 * was successfully started, or already on.
 */
static int
gms_graphics_open (void)
{
	int height = gms_graphics_height * GMS_GRAPHICS_PIXEL + 16;

	/* If graphics are off, turn them back on now. */
	if (gms_graphics_window == NULL)
	    {
		/* (Re-)open a graphics window. */
		gms_graphics_window = glk_window_open
				(gms_main_window,
				winmethod_Above|winmethod_Fixed,
				height,
				wintype_Graphics, 0);

		/* If the graphics fails, return FALSE. */
		if (gms_graphics_window == NULL)
			return FALSE;
	    }
	else
	    {
		winid_t parent;
		parent = glk_window_get_parent (gms_graphics_window);
		glk_window_set_arrangement (parent,
			winmethod_Above|winmethod_Fixed,
			height, NULL);
	    }

	/* Graphics opened successfully, or already open. */
	return TRUE;
}


/*
 * gms_graphics_close()
 *
 * If it is open, close the graphics window.
 */
static void
gms_graphics_close (void)
{
	/* If the graphics window is open, close it and set NULL. */
	if (gms_graphics_window != NULL)
	    {
		glk_window_close (gms_graphics_window, NULL);
		gms_graphics_window = NULL;
	    }
}


/*
 * gms_graphics_start()
 *
 * Start any background picture update processing.
 */
static void
gms_graphics_start (void)
{
	/* Ignore the call if pictures are disabled. */
	if (!gms_graphics_enabled)
		return;

	/* If not running, start the updating "thread". */
	if (!gms_graphics_active)
	    {
		glk_request_timer_events (GMS_GRAPHICS_TIMEOUT);
		gms_graphics_active = TRUE;
	    }
}


/*
 * gms_graphics_stop()
 *
 * Stop any background picture update processing.
 */
static void
gms_graphics_stop (void)
{
	/* If running, stop the updating "thread". */
	if (gms_graphics_active)
	    {
		glk_request_timer_events (0);
		gms_graphics_active = FALSE;
	    }
}


/*
 * gms_graphics_are_displayed()
 *
 * Return TRUE if graphics are currently being displayed, FALSE otherwise.
 */
static int
gms_graphics_are_displayed (void)
{
	return (gms_graphics_window != NULL);
}


/*
 * gms_graphics_paint()
 *
 * Set up a complete repaint of the current picture in the graphics window.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
static void
gms_graphics_paint (void)
{
	/*
	 * Ignore the call if pictures are disabled, or if there is no
	 * graphics window currently displayed.
	 */
	if (!gms_graphics_enabled
			|| !gms_graphics_are_displayed ())
		return;

	/* Set the repaint flag, and start graphics. */
	gms_graphics_repaint = TRUE;
	gms_graphics_start ();
}


/*
 * gms_graphics_restart()
 *
 * Restart graphics as if the current picture is a new picture.  This
 * function should be called whenever graphics is re-enabled after being
 * disabled, on change of gamma color correction policy, and on change
 * of animation policy.
 */
static void
gms_graphics_restart (void)
{
	/*
	 * Ignore the call if pictures are disabled, or if there is no
	 * graphics window currently displayed.
	 */
	if (!gms_graphics_enabled
			|| !gms_graphics_are_displayed ())
		return;

	/*
	 * If the picture is animated, we'll need to be able to re-get the
	 * first animation frame so that the picture can be treated as if
	 * it is a new one.  So here, we'll try to re-extract the current
	 * picture to do this.  Calling ms_extract() is safe because we
	 * don't get here unless graphics are displayed, and graphics aren't
	 * displayed until there's a valid picture loaded, and ms_showpic
	 * only loads a picture after it's called ms_extract and set the
	 * picture id into gms_graphics_picture.
	 *
	 * The bitmap and other picture stuff can be ignored because it's
	 * the precise same stuff as we already have in picture details
	 * variables.  If the ms_extract() fails, we'll carry on regardless,
	 * which may, or may not, result in the ideal picture display.
	 *
	 * One or two non-animated pictures return NULL from ms_extract()
	 * being re-called, so we'll restrict calls to animations only.
	 * And just to be safe, we'll also call only if we're already
	 * holding a bitmap (and we should be; how else could the graphics
	 * animation flag be set?...).
	 */
	if (gms_graphics_animated
			&& gms_graphics_bitmap != NULL)
	    {
		type8		*bitmap;		/* Dummy bitmap */
		type16		width, height;		/* Dummy dimensions */
		type16		palette[GMS_PALETTE_SIZE];
							/* Dummy palette */
		type8		animated;		/* Dummy animation */

		/* Extract the bitmap into dummy variables. */
		bitmap = ms_extract (gms_graphics_picture,
					&width, &height, palette, &animated);
	    }

	/* Set the new picture flag, and start graphics. */
	gms_graphics_new_picture = TRUE;
	gms_graphics_start ();
}


/*
 * gms_graphics_count_colors()
 *
 * Analyze an image, and return the usage count of each palette color, and
 * an overall count of how many colors out of the palette are used.  Color
 * usage may be NULL if only the color count is required.
 */
static void
gms_graphics_count_colors (type8 bitmap[], type16 width, type16 height,
		long color_usage[], int *color_count)
{
	int	index;				/* Palette iterator index */
	int	x, y;				/* Image iterators */
	long	usage[GMS_PALETTE_SIZE];	/* Color usage counts */
	int	count;				/* Colors count */
	long	index_row;			/* Optimization variable */
	assert (bitmap != NULL && color_count != NULL);

	/* Clear initial usage counts. */
	for (index = 0; index < GMS_PALETTE_SIZE; index++)
		usage[ index ] = 0;

	/*
	 * Traverse the image, counting each pixel usage.  For the y
	 * iterator, maintain an index row as an optimization to avoid
	 * multiplications in the loop.
	 */
	for (y = 0, index_row = 0;
			y < height; y++, index_row += width)
	    {
		for (x = 0; x < width; x++)
		    {
			long	index;		/* x,y pixel index */

			/* Get the index for this pixel. */
			index = index_row + x;

			/* Update the count for this palette color. */
			usage[ bitmap[ index ]]++;
		    }
	    }

	/* Transfer the usage counts to the output array if required. */
	if (color_usage != NULL)
	    {
		for (index = 0; index < GMS_PALETTE_SIZE; index++)
			color_usage[ index ] = usage[ index ];
	    }

	/* Count and return colors used overall in the palette. */
	count = 0;
	for (index = 0; index < GMS_PALETTE_SIZE; index++)
	    {
		if (usage[ index ] > 0)
			count++;
	    }
	*color_count = count;
}


/*
 * gms_graphics_game_to_rgb_color()
 * gms_graphics_split_color()
 * gms_graphics_combine_color()
 * gms_graphics_color_luminance()
 *
 * General graphics helper functions, to convert between Magnetic Scrolls
 * and RGB color representations, and between RGB and Glk glui32 color
 * representations, and to calculate color luminance.
 */
static void
gms_graphics_game_to_rgb_color (type16 color,
			gms_gammaref_t gamma, gms_rgbref_t rgb_color)
{
	assert (gamma != NULL && rgb_color != NULL);

	/*
	 * Convert Magnetic Scrolls color, through gamma, into RGB.  This
	 * splits the color into components based on the 3-bits used in the
	 * game palette, and gamma-corrects and rescales each to the range
	 * 0-255, using the given correction.
	 */
	rgb_color->red	 = gamma->table[ (color & 0x700) >> 8 ];
	rgb_color->green = gamma->table[ (color & 0x070) >> 4 ];
	rgb_color->blue	 = gamma->table[ (color & 0x007)      ];
}

static void
gms_graphics_split_color (glui32 color, gms_rgbref_t rgb_color)
{
	assert (rgb_color != NULL);

	/* Split color into its three components. */
	rgb_color->red		= (color >> 16) & 0xFF;
	rgb_color->green	= (color >>  8) & 0xFF;
	rgb_color->blue		= (color      ) & 0xFF;
}

static glui32
gms_graphics_combine_color (gms_rgbref_t rgb_color)
{
	glui32		color;			/* Return color. */
	assert (rgb_color != NULL);

	/* Combine RGB color from its three components. */
	color =		  (rgb_color->red   << 16)
			| (rgb_color->green << 8 )
			| (rgb_color->blue       );
	return color;
}

static int
gms_graphics_color_luminance (gms_rgbref_t rgb_color)
{
	static int	initialized	= FALSE;/* First call flag. */
	static int	weighting	= 0;	/* RGB luminance weighting. */

	long		luminance;		/* Unscaled luminance. */
	int		scaled_luminance;	/* Return luminance. */

	/* On the first call, calculate the overall weighting. */
	if (!initialized)
	    {
		weighting = GMS_LUMINANCE_WEIGHTS.red
				+ GMS_LUMINANCE_WEIGHTS.green
				+ GMS_LUMINANCE_WEIGHTS.blue;
		initialized = TRUE;
	    }

	/* Calculate the luminance and scale back by 1000 to 0-255. */
	assert (weighting > 0);
	luminance =
		 ((long) rgb_color->red   * (long) GMS_LUMINANCE_WEIGHTS.red
		+ (long) rgb_color->green * (long) GMS_LUMINANCE_WEIGHTS.green
		+ (long) rgb_color->blue  * (long) GMS_LUMINANCE_WEIGHTS.blue);
	scaled_luminance = luminance / weighting;

	/* Return the scaled luminance. */
	return scaled_luminance;
}


/*
 * gms_graphics_swap_array()
 *
 * Swap two elements of an array of long integers.
 */
static void
gms_graphics_swap_array (long longint_array[], int index_a, int index_b)
{
	long	temporary;			/* Temporary swap value */

	/* Swap values. */
	temporary			= longint_array[ index_a ];
	longint_array[ index_a ]	= longint_array[ index_b ];
	longint_array[ index_b ]	= temporary;
}


/*
 * gms_graphics_equal_contrast_gamma()
 *
 * Helper for automatic gamma correction.  This function tries to find a
 * gamma correction for the given picture, palette, and color usage that
 * gives relatively equal contrast among the displayed colors.
 *
 * This function searches the gamma tables, computing color luminance for
 * each color in the palette given this gamma.  From luminances, it then
 * computes the contrasts between the colors, and settles on the gamma
 * correction that gives the most even and well-distributed picture contrast.
 * The function ignores colors not used in the palette.
 *
 * Note that the function doesn't consider how often a palette color is
 * used, only whether it's represented, or not.  Some weighting might improve
 * things, but the simple method seems to work adequately.  In practice,
 * since there are only 16 colors in a palette, most pictures use most
 * colors in a relatively well distributed manner.  This function probably
 * wouldn't work well on real photographs, though.
 */
static gms_gammaref_t
gms_graphics_equal_contrast_gamma (type8 bitmap[],
				type16 palette[], long color_usage[])
{
	gms_gammaref_t	gamma, result;		/* Table iterator, result */
	int		lowest_variance;	/* Lowest contrast variance */
	assert (bitmap != NULL && palette != NULL);

	/* Initialize the search result, and lowest contrast variance found. */
	result		= NULL;
	lowest_variance	= INT_MAX;

	/* Iterate over the gamma corrections table. */
	for (gamma = GMS_GAMMA_TABLE; gamma->level != NULL; gamma++)
	    {
		int	index, outer;		/* Array iterators */
		int	luminances;		/* Luminance count and index */
		long	luminance[GMS_PALETTE_SIZE + 1];
						/* Luminance for each color,
						   plus one extra for black */
		int	has_black;		/* Palette uses black flag */
		long	contrast[GMS_PALETTE_SIZE];
						/* Contrast between colors */
		long	sum, mean;		/* Contrast sum and mean */
		int	variance;		/* Variance in contrast */

		/* Calculate the energy of each color at this gamma. */
		has_black = FALSE;
		luminances = 0;
		for (index = 0; index < GMS_PALETTE_SIZE; index++)
		    {
			type16		color;		/* Raw picture color */
			gms_rgb_t	rgb_color;	/* Corrected color */

			/* Only work on palette colors represented. */
			if (color_usage[ index ] == 0)
				continue;

			/* Find the 16-bit base picture color. */
			color = palette[ index ];

			/*
			 * Convert this base color to RGB using the gamma
			 * currently under consideration.
			 */
			gms_graphics_game_to_rgb_color
						(color, gamma, &rgb_color);

			/* If this is black, note that we've seen it. */
			if (rgb_color.red == 0
					&& rgb_color.green == 0
					&& rgb_color.blue == 0)
				has_black = TRUE;

			/*
			 * Calculate luminance for this color, and store in
			 * the next available array entry.
			 */
			luminance[ luminances++ ] =
				gms_graphics_color_luminance (&rgb_color);
		    }

		/*
		 * For best results, we want to anchor contrast calculations
		 * to black, so if black is not represented in the palette,
		 * add it as an extra luminance.
		 */
		if (!has_black)
			luminance[ luminances++ ] = 0;

		/*
		 * Sort luminance values into inverse order, so that the
		 * darkest color is at index 0.
		 */
		for (outer = 0; outer < luminances - 1; outer++)
		    {
			int	is_sorted;	/* Array sorted flag */

			is_sorted = TRUE;
			for (index = 0; index < luminances - outer - 1; index++)
			    {
				if (luminance[ index + 1 ] < luminance[ index ])
				    {
					gms_graphics_swap_array
						(luminance, index, index + 1);

					/* Flag as not yet sorted. */
					is_sorted = FALSE;
				    }
			    }
			if (is_sorted)
				break;
		    }

		/*
		 * Calculate the difference in luminance between adjacent
		 * luminances in the sorted array, as contrast, and at the
		 * same time sum contrasts to calculate the mean.
		 */
		sum = 0;
		for (index = 0; index < luminances - 1; index++)
		    {
			contrast[ index ] = luminance[ index + 1 ]
						- luminance[ index ];
			sum += contrast[ index ];
		    }
		mean = sum / (luminances - 1);

		/* From the mean, find the variance in contrasts. */
		sum = 0;
		for (index = 0; index < luminances - 1; index++)
		    {
			long	delta;		/* Contrast difference */

			/* Sum the square of contrast differences from mean */
			delta	= contrast[ index ] - mean;
			sum	+= delta * delta;
		    }
		variance = sum / (luminances - 1);

		/*
		 * Compare the variance to the lowest so far, and if it is
		 * lower, note the gamma entry that produced it as being
		 * the current best found.
		 */
		if (variance < lowest_variance)
		    {
			result		= gamma;
			lowest_variance	= variance;
		    }
	    }

	/*
	 * Return the gamma table entry that produced the most even
	 * picture contrast.
	 */
	assert (result != NULL);
	return result;
}


/*
 * gms_graphics_select_gamma()
 *
 * Select a suitable gamma for the picture, based on the current gamma
 * mode.
 *
 * The function returns either the linear gamma, a gamma value half way
 * between linear and the gamma that gives the most even contrast, or
 * just the gamma that gives the most even contrast.
 *
 * In the normal case, a value half way to the extreme case of making
 * color contrast equal for all colors is, subjectively, a reasonable
 * value to use.  The problem cases are the darkest pictures, and sele-
 * cting this value brightens them while at the same time not making
 * them look overbright or too "sunny".
 */
static gms_gammaref_t
gms_graphics_select_gamma (type8 bitmap[],
			type16 width, type16 height, type16 palette[])
{
	static int	initialized	= FALSE;/* First call flag. */
	static gms_gammaref_t
			linear_gamma	= NULL;	/* Linear table entry */

	long		color_usage[GMS_PALETTE_SIZE];
						/* Colors usage counts */
	int		color_count;		/* Count represented colors */
	gms_gammaref_t	contrast_gamma;		/* Equal contrast gamma */

	/* On first call, find and cache the uncorrected gamma table entry. */
	if (!initialized)
	    {
		gms_gammaref_t	gamma;		/* Table iterator */

		/* Scan the table for the uncorrected entry. */
		for (gamma = GMS_GAMMA_TABLE; gamma->level != NULL; gamma++)
		    {
			if (!gamma->corrected)
			    {
				linear_gamma = gamma;
				break;
			    }
		    }

		/* Set initialized flag. */
		initialized = TRUE;
	    }

	/* Check to see if automated correction is turned off. */
	if (gms_gamma_mode == GAMMA_OFF)
	    {
		/*
		 * If automatic correction is completely disabled, just
		 * return the linear gamma, and we're done.
		 */
		assert (linear_gamma != NULL);
		return linear_gamma;
	    }

	/*
	 * Obtain the color usage and count of total colors represented.
	 */
	gms_graphics_count_colors (bitmap, width, height,
					color_usage, &color_count);
	if (color_count <= 1)
	    {
		/*
		 * For a degenerate picture with one color or less, return the
		 * linear gamma regardless.
		 */
		assert (linear_gamma != NULL);
		return linear_gamma;
	    }

	/*
	 * Now calculate a gamma setting to give the most equal contrast
	 * across the picture colors.  We'll return either half this gamma,
	 * or all of it.
	 */
	contrast_gamma = gms_graphics_equal_contrast_gamma
						(bitmap, palette, color_usage);

	/* Check to see if automated correction is normal. */
	if (gms_gamma_mode == GAMMA_NORMAL)
	    {
		/*
		 * Return a gamma value half way between the linear gamma
		 * and the equal contrast gamma.
		 */
		assert (linear_gamma != NULL);
		return linear_gamma
			+ (contrast_gamma - linear_gamma) / 2;
	    }

	/* Correction must be high; return the equal contrast gamma. */
	assert (gms_gamma_mode == GAMMA_HIGH);
	return contrast_gamma;
}


/*
 * gms_graphics_clear_and_border()
 *
 * Clear the graphics window, and border and shade the area where the
 * picture is going to be rendered.  This attempts a small raised effect
 * for the picture, in keeping with modern trends.
 */
static void
gms_graphics_clear_and_border (winid_t glk_window,
			int x_offset, int y_offset,
			int pixel_size, type16 width, type16 height)
{
	glui32		background;		/* Window background color */
	assert (glk_window != NULL);

	/*
	 * Try to detect the background color of the main window, by getting
	 * the background for Normal style (Glk offers no way to directly
	 * get a window's background color).  If we can get it, we'll match
	 * the graphics window background to it.  If we can't, we'll default
	 * the color to white.
	 */
	if (!glk_style_measure (gms_main_window,
			style_Normal, stylehint_BackColor, &background))
	    {
		/*
		 * Unable to get the main window background, so assume, and
		 * default graphics to white.
		 */
		background = GMS_GRAPHICS_DEFAULT_BACKGROUND;
	    }

	/*
	 * Set the graphics window background to match the main window
	 * background, as best as we can tell, and clear the window.
	 */
	glk_window_set_background_color (glk_window, background);
	glk_window_clear (glk_window);

	/*
	 * Paint a rectangle bigger than the picture by border pixels all
	 * round, and with additional shading pixels right and below.
	 * The picture will sit over this rectangle.
	 */
#if 0
	glk_window_fill_rect (glk_window,
		GMS_GRAPHICS_SHADE_COLOR,
		x_offset - GMS_GRAPHICS_BORDER + GMS_GRAPHICS_SHADING,
		y_offset - GMS_GRAPHICS_BORDER + GMS_GRAPHICS_SHADING,
		width  * pixel_size + GMS_GRAPHICS_BORDER * 2,
		height * pixel_size + GMS_GRAPHICS_BORDER * 2);
#endif
	glk_window_fill_rect (glk_window,
		GMS_GRAPHICS_BORDER_COLOR,
		x_offset - GMS_GRAPHICS_BORDER,
		y_offset - GMS_GRAPHICS_BORDER,
		width  * pixel_size + GMS_GRAPHICS_BORDER * 2,
		height * pixel_size + GMS_GRAPHICS_BORDER * 2);
}


/*
 * gms_graphics_convert_palette()
 *
 * Convert a Magnetic Scrolls color palette to a Glk one, using the given
 * gamma corrections.
 */
static void
gms_graphics_convert_palette (type16 ms_palette[],
			gms_gammaref_t gamma, glui32 glk_palette[])
{
	int		index;			/* Palette color index */
	assert (ms_palette != NULL && glk_palette != NULL);
	assert (gamma != NULL);

	/* Convert each color in the palette. */
	for (index = 0; index < GMS_PALETTE_SIZE; index++)
	    {
		type16		color;		/* Raw picture color. */
		gms_rgb_t	rgb_color;	/* Converted RGB color. */

		/* Find the 16-bit base picture color. */
		color = ms_palette[ index ];

		/* Convert through gamma to a 32-bit RGB color. */
		gms_graphics_game_to_rgb_color (color, gamma, &rgb_color);

		/* Combine RGB into a Glk color, held in the Glk palette. */
		glk_palette[ index ] =
				gms_graphics_combine_color (&rgb_color);
	    }
}


/*
 * gms_graphics_position_picture()
 *
 * Given a picture width and height, return the x and y offsets to center
 * this picture in the current graphics window.
 */
static void
gms_graphics_position_picture (winid_t glk_window,
			int pixel_size, type16 width, type16 height,
			int *x_offset, int *y_offset)
{
	glui32		window_width, window_height;
					/* Graphics window dimensions */
	assert (glk_window != NULL);
	assert (x_offset != NULL && y_offset != NULL);

	/* Measure the current graphics window dimensions. */
	glk_window_get_size (glk_window, &window_width, &window_height);

	/*
	 * Calculate and return an x and y offset to use on point plotting,
	 * so that the image centers inside the graphical window.
	 */
	*x_offset = ((int) window_width  - width  * pixel_size) / 2;
	*y_offset = ((int) window_height - height * pixel_size) / 2;
}


/*
 * gms_graphics_apply_animation_frame()
 *
 * This function applies a single animation frame to the given off-screen
 * image buffer.  It's handed the frame bitmap, width, height and mask,
 * the off-screen buffer, and the width and height of the main picture.
 *
 * Note that 'mask' may be NULL, implying that no frame pixel is transparent.
 */
static void
gms_graphics_apply_animation_frame (type8 bitmap[],
			type16 frame_width, type16 frame_height, type8 mask[],
			int frame_x, int frame_y,
			type8 off_screen[], type16 width, type16 height)
{
	int		mask_width;		/* Width of frame mask */
	type8		mask_hibit;		/* High bit only set */
	int		x, y;			/* Frame pixel iterators */
	long		frame_row, buffer_row;	/* Optimization variables */
	long		mask_row;		/* Ditto */
	assert (bitmap != NULL && off_screen != NULL);

	/*
	 * It turns out that the mask isn't quite as described in defs.h,
	 * and thanks to Torbjorn Andersson and his Gtk port of Magnetic
	 * for illuminating this.  The mask is made up of lines of 16-bit
	 * words, so the mask width is always even.  Here we'll calculate
	 * the real width of a mask, and also set a high bit for later on.
	 */
	mask_width = (((frame_width - 1) / CHAR_BIT) + 2) & (~1);
	mask_hibit = 1 << (CHAR_BIT - 1);

	/*
	 * Initialize row index components; these are optimizations to
	 * avoid the need for multiplications in the frame iteration loop.
	 */
	frame_row  = 0;
	buffer_row = frame_y * width;
	mask_row   = 0;

	/*
	 * Iterate over each frame row, clipping where y lies outside the
	 * main picture area.
	 */
	for (y = 0; y < frame_height; y++)
	    {
		/* Clip if y is outside the main picture area. */
		if (y + frame_y < 0 || y + frame_y >= height)
		    {
			/* Update optimization variables as if not clipped. */
			frame_row  += frame_width;
			buffer_row += width;
			mask_row   += mask_width;
			continue;
		    }

		/* Iterate over each frame column, clipping again. */
		for (x = 0; x < frame_width; x++)
		    {
			long	frame_index, buffer_index;

			/* Clip if x is outside the main picture area. */
			if (x + frame_x < 0 || x + frame_x >= width)
				continue;

			/*
			 * If there's a mask, check the bit associated with
			 * this x,y, and ignore any transparent pixels.
			 */
			if (mask != NULL)
			    {
				type8	mask_byte;	/* Mask byte */

				/* Isolate the mask byte. */
				mask_byte = mask[ mask_row + (x / CHAR_BIT) ];

				/* Test the bit for pixel transparency. */
				if ((mask_byte
					 & (mask_hibit >> (x % CHAR_BIT))) != 0)
					continue;
			    }

			/*
			 * Calculate indexes for this pixel into the frame,
			 * and into the main off-screen buffer.
			 */
			frame_index  = frame_row + x;
			buffer_index = buffer_row + (x + frame_x);

			/*
			 * Transfer the frame pixel to the off-screen buffer.
			 */
			off_screen[ buffer_index ] = bitmap[ frame_index ];
		    }

		/* Update row index components on change of y. */
		frame_row  += frame_width;
		buffer_row += width;
		mask_row   += mask_width;
	    }
}


/*
 * gms_graphics_animate()
 *
 * This function finds and applies the next set of animation frames to the
 * given off-screen image buffer.  It's handed the width and height of the
 * main picture, and the off-screen buffer.
 *
 * It returns FALSE if at the end of animations, TRUE if more animations
 * remain.
 */
static int
gms_graphics_animate (type8 off_screen[], type16 width, type16 height)
{
	struct ms_position	*positions;	/* Animation frame array */
	type16			count;		/* Count of animation frames */
	type8			status;		/* ms_animate status */
	int			frame;		/* Frame iterator */
	assert (off_screen != NULL);

	/* Search for more animation frames, and return zero if none. */
	status = ms_animate (&positions, &count);
	if (status == 0)
		return FALSE;

	/* Apply each animation frame to the off-screen buffer. */
	for (frame = 0; frame < count; frame++)
	    {
		type8	*bitmap;		/* Frame bitmap */
		type8	*mask;			/* Frame mask */
		type16	frame_width, frame_height;
						/* Frame width and height */

		/*
		 * Get the bitmap and other details for this frame.  If we
		 * can't get this animation frame, skip it and see if any
		 * others are available.
		 */
		bitmap = ms_get_anim_frame (positions[ frame ].number,
					&frame_width, &frame_height, &mask);
		if (bitmap != NULL)
		    {
			gms_graphics_apply_animation_frame (bitmap,
					frame_width, frame_height, mask,
					positions[ frame ].x,
					positions[ frame ].y,
					off_screen, width, height);
		    }
	    }

	/* Return TRUE since more animation frames remain. */
	return TRUE;
}


/*
 * gms_graphics_is_vertex()
 *
 * Given a point, return TRUE if that point is the vertex of a fillable
 * region.  This is a helper function for layering pictures.  When assign-
 * ing layers, we want to weight the colors that have the most complex
 * shapes, or the largest count of isolated areas, heavier than simpler
 * areas.
 *
 * By painting the colors with the largest number of isolated areas or
 * the most complex shapes first, we help to minimize the number of fill
 * regions needed to render the complete picture.
 */
static int
gms_graphics_is_vertex (type8 off_screen[],
			type16 width, type16 height, int x, int y)
{
	type8	pixel;				/* Reference pixel */
	int	above, below, left, right;	/* Neighbor difference flags */
	long	index_row;			/* Optimization variable */
	assert (off_screen != NULL);

	/* Use an index row to cut down on multiplications. */
	index_row = y * width;

	/* Find the color of the reference pixel. */
	pixel = off_screen[ index_row + x ];
	assert (pixel < GMS_PALETTE_SIZE);

	/*
	 * Detect differences between the reference pixel and its upper,
	 * lower, left and right neighbors.  Mark as different if the
	 * neighbor doesn't exist, that is, at the edge of the picture.
	 */
	above = (y == 0
		|| off_screen[ index_row - width + x ] == pixel);
	below = (y == height - 1
		|| off_screen[ index_row + width + x ] == pixel);
	left  = (x == 0
		|| off_screen[ index_row + x - 1 ] == pixel);
	right = (x == width - 1
		|| off_screen[ index_row + x + 1 ] == pixel);

	/*
	 * Return TRUE if this pixel lies at the vertex of a rectangular,
	 * fillable, area.  That is, if two adjacent neighbors aren't the
	 * same color (or if absent -- at the edge of the picture).
	 */
	return ((above || below) && (left || right));
}


/*
 * gms_graphics_assign_layers()
 *
 * Given two sets of image bitmaps, and a palette, this function will
 * assign layers palette colors.
 *
 * Layers are assigned by first counting the number of vertices in the
 * color plane, to get a measure of the complexity of shapes displayed in
 * this color, and also the raw number of times each palette color is
 * used.  This is then sorted, so that layers are assigned to colors, with
 * the lowest layer being the color with the most complex shapes, and
 * within this (or where the count of vertices is zero, as it could be
 * in some animation frames) the most used color.
 *
 * The function compares pixels in the two image bitmaps given, these
 * being the off-screen and on-screen buffers, and generates counts only
 * where these bitmaps differ.  This ensures that only pixels not yet
 * painted are included in layering.
 *
 * As well as assigning layers, this function returns a set of layer usage
 * flags, to help the rendering loop to terminate as early as possible.
 *
 * By painting lower layers first, the paint can take in larger areas if
 * it's permitted to include not-yet-validated higher levels.  This helps
 * minimize the amount of Glk areas fills needed to render a picture.
 */
static void
gms_graphics_assign_layers (type8 off_screen[], type8 on_screen[],
			type16 width, type16 height,
			int layers[], long layer_usage[])
{
	int	index, outer;			/* Palette iterators */
	int	x, y;				/* Image iterators */
	long	usage[GMS_PALETTE_SIZE];	/* Color use counts */
	long	complexity[GMS_PALETTE_SIZE];	/* Color vertex counts */
	long	colors[GMS_PALETTE_SIZE];	/* Temporary color indexes */
	long	index_row;			/* Optimization variable */
	assert (off_screen != NULL && on_screen != NULL);
	assert (layers != NULL && layer_usage != NULL);

	/* Clear initial complexity and usage counts. */
	for (index = 0; index < GMS_PALETTE_SIZE; index++)
	    {
		complexity[ index ] = 0;
		usage[ index ]      = 0;
	    }

	/*
	 * Traverse the image, counting vertices and pixel usage where the
	 * pixels differ between the off-screen and on-screen buffers.
	 * Optimize by maintaining an index row to avoid multiplications.
	 */
	for (y = 0, index_row = 0;
			y < height; y++, index_row += width)
	    {
		for (x = 0; x < width; x++)
		    {
			long	index;		/* x,y pixel index */

			/* Get the index for this pixel. */
			index = index_row + x;

			/* Update complexity and usage if pixels differ. */
			if (on_screen[ index ] != off_screen[ index ])
			    {
				if (gms_graphics_is_vertex (off_screen,
							width, height, x, y))
					complexity[ off_screen[ index ]]++;

				usage[ off_screen[ index ]]++;
			    }
		    }
	    }

	/*
	 * Sort counts to form color indexes.  The primary sort is on the
	 * shape complexity, and within this, on color usage.
	 *
	 * Some colors may have no vertices at all when doing animation
	 * frames, but rendering optimization relies on the first layer that
	 * contains no areas to fill halting the rendering loop.  So it's
	 * important here that we order indexes so that colors that render
	 * complex shapes come first, non-empty, but simpler shaped colors
	 * next, and finally all genuinely empty layers.
	 */
	for (index = 0; index < GMS_PALETTE_SIZE; index++)
		colors[ index ] = index;
	for (outer = 0; outer < GMS_PALETTE_SIZE - 1; outer++)
	    {
		int	is_sorted;		/* Array sorted flag */

		is_sorted = TRUE;
		for (index = 0;
			index < GMS_PALETTE_SIZE - outer - 1; index++)
		    {
			if (complexity[ index + 1 ] > complexity[ index ]
			    || (complexity[ index + 1 ] == complexity[ index ]
					&& usage[ index + 1 ] > usage[ index ]))
			    {
				/* Swap colors and matching counts. */
				gms_graphics_swap_array
						(colors, index, index + 1);
				gms_graphics_swap_array
						(complexity, index, index + 1);
				gms_graphics_swap_array
						(usage, index, index + 1);

				/* Flag as not yet sorted. */
				is_sorted = FALSE;
			    }
		    }
		if (is_sorted)
			break;
	    }

	/*
	 * Assign a layer to each palette color, and also return the layer
	 * usage for each layer.
	 */
	for (index = 0; index < GMS_PALETTE_SIZE; index++)
	    {
		layers[ colors[ index ]] = index;
		layer_usage[ index ]     = usage[ index ];
	    }
}


/*
 * gms_graphics_paint_region()
 *
 * This is a partially optimized point plot.  Given a point in the
 * graphics bitmap, it tries to extend the point to a color region, and
 * fill a number of pixels in a single Glk rectangle fill.  The goal
 * here is to reduce the number of Glk rectangle fills, which tend to be
 * extremely inefficient operations for generalized point plotting.
 *
 * The extension works in image layers; each palette color is assigned
 * a layer, and we paint each layer individually, starting at the lowest.
 * So, the region is free to fill any invalidated pixel in a higher layer,
 * and all pixels, invalidated or already validated, in the same layer.
 * In practice, it is good enough to look for either invalidated pixels
 * or pixels in the same layer, and construct a region as large as
 * possible from these, then on marking points as validated, mark only
 * those in the same layer as the initial point.
 *
 * The optimization here is not the best possible, but is reasonable.
 * What we do is to try and stretch the region horizontally first, then
 * vertically.  In practice, we might find larger areas by stretching
 * vertically and then horizontally, or by stretching both dimensions at
 * the same time.  In mitigation, the number of colors in a picture is
 * small (16), and the aspect ratio of pictures makes them generally
 * wider than they are tall.
 *
 * Once we've found the region, we render it with a single Glk rectangle
 * fill, and mark all the pixels in this region that match the layer of
 * the initial given point as validated.
 */
static void
gms_graphics_paint_region (winid_t glk_window,
			glui32 palette[], int layers[],
			type8 off_screen[], type8 on_screen[],
			int x, int y, int x_offset, int y_offset,
			int pixel_size, type16 width, type16 height)
{
	type8		pixel;			/* Reference pixel color */
	int		layer;			/* Reference pixel layer */
	int		x_min, x_max;		/* X region extent */
	int		y_min, y_max;		/* Y region extent */
	int		stop;			/* Stretch stop flag */
	int		x_index, y_index;	/* Region iterator indexes */
	long		index_row;		/* Optimization variable */
	assert (glk_window != NULL && palette != NULL && layers != NULL);
	assert (off_screen != NULL && on_screen != NULL);

	/* Find the color and layer for the initial pixel. */
	pixel = off_screen[ y * width + x ];
	layer = layers[ pixel ];
	assert (pixel < GMS_PALETTE_SIZE);

	/*
	 * Start by finding the extent to which we can pull the x coordinate
	 * and still find either invalidated pixels, or pixels in this layer.
	 *
	 * Use an index row to remove multiplications from the loops.
	 */
	index_row = y * width;
	for (x_min = x, stop = FALSE; x_min - 1 >= 0 && !stop; )
	    {
		long	index = index_row + x_min - 1;

		if (on_screen[ index ] == off_screen[ index ]
				&& layers[ off_screen[ index ]] != layer)
			stop = TRUE;
		if (!stop)
			x_min--;
	    }
	for (x_max = x, stop = FALSE; x_max + 1 < width && !stop; )
	    {
		long	index = index_row + x_max + 1;

		if (on_screen[ index ] == off_screen[ index ]
				&& layers[ off_screen[ index ]] != layer)
			stop = TRUE;
		if (!stop)
			x_max++;
	    }

	/*
	 * Now try to stretch the height of the region, by extending the
	 * y coordinate as much as possible too.  Again, we're looking
	 * for pixels that are invalidated or ones in the same layer.  We
	 * need to check across the full width of the current region.
	 *
	 * As above, an index row removes multiplications from the loops.
	 */
	for (y_min = y, index_row = (y - 1) * width, stop = FALSE;
			y_min - 1 >= 0 && !stop; index_row -= width)
	    {
		for (x_index = x_min; x_index <= x_max && !stop; x_index++)
		    {
			long	index = index_row + x_index;

			if (on_screen[ index ] == off_screen[ index ]
				    && layers[ off_screen[ index ]] != layer)
				stop = TRUE;
		    }
		if (!stop)
			y_min--;
	    }
	for (y_max = y, index_row = (y + 1) * width, stop = FALSE;
			y_max + 1 < height && !stop; index_row += width)
	    {
		for (x_index = x_min; x_index <= x_max && !stop; x_index++)
		    {
			long	index = index_row + x_index;

			if (on_screen[ index ] == off_screen[ index ]
				    && layers[ off_screen[ index ]] != layer)
				stop = TRUE;
		    }
		if (!stop)
			y_max++;
	    }

	/* Fill the region using Glk's rectangle fill. */
	glk_window_fill_rect (glk_window,
			palette[ pixel ],
			x_min * pixel_size + x_offset,
			y_min * pixel_size + y_offset,
			(x_max - x_min + 1) * pixel_size,
			(y_max - y_min + 1) * pixel_size);

	/*
	 * Validate each pixel in the reference layer that was rendered by
	 * the rectangle fill.  We don't validate pixels that are not in
	 * this layer (and are by definition in higher layers, as we've
	 * validated all lower layers), since although we colored them,
	 * we did it for optimization reasons, and they're not yet colored
	 * correctly.
	 *
	 * Maintain an index row as an optimization to avoid multiplication.
	 */
	index_row = y_min * width;
	for (y_index = y_min; y_index <= y_max; y_index++)
	    {
		for (x_index = x_min; x_index <= x_max; x_index++)
		    {
			long	index;			/* x,y pixel index */

			/* Get the index for x_index,y_index. */
			index = index_row + x_index;

			/* If the layers match, update the on-screen buffer. */
			if (layers[ off_screen[ index ]] == layer)
			    {
				assert (off_screen[ index ] == pixel);
				on_screen[ index ] = off_screen[ index ];
			    }
		    }

		/* Update row index component on change of y. */
		index_row += width;
	    }
}

static void
gms_graphics_paint_everything (winid_t glk_window,
			glui32 palette[],
			type8 off_screen[],
			int x_offset, int y_offset,
			type16 width, type16 height)
{
	type8		pixel;			/* Reference pixel color */
	int		x, y;

	for (y = 0; y < height; y++)
	{
	    for (x = 0; x < width; x ++)
	    {
		pixel = off_screen[ y * width + x ];
		glk_window_fill_rect (glk_window,
			palette[ pixel ],
			x * GMS_GRAPHICS_PIXEL + x_offset,
			y * GMS_GRAPHICS_PIXEL + y_offset,
			GMS_GRAPHICS_PIXEL, GMS_GRAPHICS_PIXEL);
	    }
	}
}

/*
 * gms_graphics_timeout()
 *
 * This is a background function, called on Glk timeouts.  Its job is to
 * repaint some of the current graphics image.  On successive calls, it
 * does a part of the repaint, then yields to other processing.  This is
 * useful since the Glk primitive to plot points in graphical windows is
 * extremely slow; this way, the repaint doesn't block game play.
 *
 * The function should be called on Glk timeout events.  When the repaint
 * is complete, the function will turn off Glk timers.
 *
 * The function uses double-buffering to track how much of the graphics
 * buffer has been rendered.  This helps to minimize the amount of point
 * plots required, as only the differences between the two buffers need
 * to be rendered.
 */
static void
gms_graphics_timeout (void)
{
	static glui32	palette[GMS_PALETTE_SIZE];
						/* Precomputed Glk palette */
	static int	layers[GMS_PALETTE_SIZE];
						/* Assigned image layers */
	static long	layer_usage[GMS_PALETTE_SIZE];
						/* Image layer occupancies */
	static type8	*on_screen	= NULL;	/* On-screen image buffer */
	static type8	*off_screen	= NULL;	/* Off-screen image buffer */

	static int	deferred_repaint = FALSE;
						/* Local delayed repaint flag */
	static int	ignore_counter;		/* Count of calls ignored */

	static int	x_offset, y_offset;	/* Point plot offsets */
	static int	yield_counter;		/* Yields in rendering */
	static int	saved_layer;		/* Saved current layer */
	static int	saved_x, saved_y;	/* Saved x,y coord */

	static int	total_regions;		/* Debug statistic */

	long		picture_size;		/* Picture size in pixels */
	int		layer;			/* Image layer iterator */
	int		x, y;			/* Image iterators */
	int		regions;		/* Count of regions painted */

	/* Ignore the call if the current graphics state is inactive. */
	if (!gms_graphics_active)
		return;
	assert (gms_graphics_window != NULL);

	/*
	 * On detecting a repaint request, note the flag in a local static
	 * variable, then set up a graphics delay to wait until, hopefully,
	 * the resize, if that's what caused it, is complete, and return.
	 * This makes resizing the window a lot smoother, since it prevents
	 * unnecessary region paints where we are receiving consecutive Glk
	 * arrange or redraw events.
	 */
	if (gms_graphics_repaint)
	    {
		deferred_repaint	= TRUE;
		gms_graphics_repaint	= FALSE;
		ignore_counter		= GMS_GRAPHICS_REPAINT_WAIT - 1;
		return;
	    }

	/*
	 * If asked to ignore a given number of calls, decrement the ignore
	 * counter and return having done nothing more.  This lets us delay
	 * graphics operations by a number of timeouts, providing animation
	 * timing and partial protection from resize event "storms".
	 *
	 * Note -- to wait for N timeouts, set the count of timeouts to be
	 * ignored to N-1.
	 */
	assert (ignore_counter >= 0);
	if (ignore_counter > 0)
	    {
		ignore_counter--;
		return;
	    }

	/* Calculate the picture size, useful throughout. */
	picture_size = gms_graphics_width * gms_graphics_height;

	/*
	 * If we received a new picture, set up the local static variables
	 * for that picture -- decide on gamma correction, convert the
	 * color palette, and initialize the off_screen buffer to be the
	 * base picture.
	 */
	if (gms_graphics_new_picture)
	    {
		/*
		 * Initialize the off_screen buffer to be a copy of the base
		 * picture.
		 */
		if (off_screen != NULL)
			free (off_screen);
		off_screen = gms_malloc (picture_size * sizeof (*off_screen));
		memcpy (off_screen, gms_graphics_bitmap,
					picture_size * sizeof (*off_screen));

		/* Note the buffer for freeing on cleanup. */
		gms_graphics_off_screen = off_screen;

		/*
		 * If the picture is animated, apply the first animation
		 * frames now.  This is important, since they form an
		 * intrinsic part of the first displayed image (in type2
		 * animation cases, perhaps _all_ of the first displayed
		 * image).
		 */
		if (gms_graphics_animated)
		    {
			gms_graphics_animate (off_screen,
				gms_graphics_width, gms_graphics_height);
		    }

		/*
		 * Select a suitable gamma for the picture, taking care to
		 * use the off-screen buffer.
		 */
		gms_graphics_current_gamma =
				gms_graphics_select_gamma
			  			(off_screen,
						gms_graphics_width,
						gms_graphics_height,
						gms_graphics_palette);

		/*
		 * Pre-convert all the picture palette colors into their
		 * corresponding Glk colors.
		 */
		gms_graphics_convert_palette (gms_graphics_palette,
					gms_graphics_current_gamma, palette);

		/* Save the color count for possible queries later. */
		gms_graphics_count_colors (off_screen,
					gms_graphics_width,
					gms_graphics_height,
					NULL, &gms_graphics_color_count);
	    }

	/*
	 * For a new picture, or a repaint of a prior one, calculate new
	 * values for the x and y offsets used to draw image points, and
	 * set the on-screen buffer to an unused pixel value, in effect
	 * invalidating all on-screen data.  Also, reset the saved image
	 * scan coordinates so that we scan for unpainted pixels from top
	 * left starting at layer zero, and clear the graphics window.
	 */
	if (gms_graphics_new_picture
			|| deferred_repaint)
	    {
		/*
		 * Calculate the x and y offset to center the picture in
		 * the graphics window.
		 */
		gms_graphics_position_picture (gms_graphics_window,
				GMS_GRAPHICS_PIXEL,
				gms_graphics_width, gms_graphics_height,
				&x_offset, &y_offset);

		/*
		 * Reset all on-screen pixels to an unused value, guaranteed
		 * not to match any in a real picture.  This forces all
		 * pixels to be repainted on a buffer/on-screen comparison.
		 */
		if (on_screen != NULL)
			free (on_screen);
		on_screen = gms_malloc (picture_size * sizeof (*on_screen));
		memset (on_screen, GMS_GRAPHICS_UNUSED_PIXEL,
					picture_size * sizeof (*on_screen));

		/* Note the buffer for freeing on cleanup. */
		gms_graphics_on_screen = on_screen;

		/*
		 * Assign new layers to the current image.  This sorts
		 * colors by usage and puts the most used colors in the
		 * lower layers.  It also hands us a count of pixels in
		 * each layer, useful for knowing when to stop scanning for
		 * layers in the rendering loop.
		 */
#ifndef GARGLK
		gms_graphics_assign_layers (off_screen, on_screen,
				gms_graphics_width, gms_graphics_height,
							layers, layer_usage);
#endif

		/* Clear the graphics window. */
		gms_graphics_clear_and_border (gms_graphics_window,
						x_offset, y_offset,
						GMS_GRAPHICS_PIXEL,
						gms_graphics_width,
						gms_graphics_height);

		/* Start a fresh picture rendering pass. */
		yield_counter	= 0;
		saved_layer	= 0;
		saved_x		= 0;
		saved_y		= 0;
		total_regions	= 0;

		/* Clear the new picture and deferred repaint flags. */
		gms_graphics_new_picture	= FALSE;
		deferred_repaint		= FALSE;
	    }

#ifndef GARGLK
	/*
	 * Make a portion of an image pass, from lower to higher image layers,
	 * scanning for invalidated pixels that are in the current image layer
	 * we are painting.  Each invalidated pixel gives rise to a region
	 * paint, which equates to one Glk rectangle fill.
	 *
	 * When the limit on regions is reached, save the current image pass
	 * layer and coordinates, and yield control to the main game playing
	 * code by returning.  On the next call, pick up where we left off.
	 *
	 * As an optimization, we can leave the loop on the first empty layer
	 * we encounter.  Since layers are ordered by complexity and color
	 * usage, all layers higher than the first unused one will also be
	 * empty, so we don't need to scan them.
	 */
	regions = 0;
	for (layer = saved_layer;
		layer < GMS_PALETTE_SIZE && layer_usage[ layer ] > 0;
		layer++)
	    {
		long	index_row;		/* Optimization variable */

		/*
		 * As an optimization to avoid multiplications in the loop,
		 * maintain a separate index row.
		 */
		index_row = saved_y * gms_graphics_width;
		for (y = saved_y; y < gms_graphics_height; y++)
		    {
			for (x = saved_x; x < gms_graphics_width; x++)
			    {
				long	index;		/* x,y pixel index */

				/* Get the index for this pixel. */
				index = index_row + x;
				assert (index < picture_size
							* sizeof (*off_screen));

				/*
				 * Ignore pixels not in the current layer, and
				 * pixels not currently invalid (that is, ones
				 * whose on-screen representation matches the
				 * off-screen buffer).
				 */
				if (layers[ off_screen[ index ]] == layer
					&& on_screen[ index ]
							!= off_screen[ index ])
				    {
					/*
					 * Rather than painting just one pixel,
					 * here we try to paint the maximal
					 * region we can for the layer of the
					 * given pixel.
					 */
					gms_graphics_paint_region
						(gms_graphics_window,
						palette, layers,
						off_screen, on_screen,
						x, y, x_offset, y_offset,
						GMS_GRAPHICS_PIXEL,
						gms_graphics_width,
						gms_graphics_height);

					/*
					 * Increment count of regions handled,
					 * and yield, by returning, if the
					 * limit on paint regions is reached.
					 * Before returning, save the current
					 * layer and scan coordinates, so we
					 * can pick up here on the next call.
					 */
					regions++;
					if (regions >= GMS_REPAINT_LIMIT)
					    {
						yield_counter++;
						saved_layer	= layer;
						saved_x		= x;
						saved_y		= y;
						total_regions	+= regions;
						return;
					    }
				    }
			    }

			/* Reset the saved x coordinate on y increment. */
			saved_x = 0;

			/* Update the index row on change of y. */
			index_row += gms_graphics_width;
		    }

		/* Reset the saved y coordinate on layer change. */
		saved_y = 0;
	    }

	/*
	 * If we reach this point, then we didn't get to the limit on regions
	 * painted on this pass.  In that case, we've finished rendering the
	 * image.
	 */
	assert (regions < GMS_REPAINT_LIMIT);
	total_regions += regions;

#else
	gms_graphics_paint_everything
	    (gms_graphics_window,
	     palette, off_screen,
	     x_offset, y_offset,
	     gms_graphics_width,
	     gms_graphics_height);
#endif

	/*
	 * If animated, and if animations are enabled, handle further
	 * animation frames, if any.
	 */
	if (gms_animation_enabled
			&& gms_graphics_animated)
	    {
		int	more_animation;		/* Animation status */

		/*
		 * Reset the off-screen buffer to a copy of the base picture.
		 * This is the correct state for applying animation frames.
		 */
		memcpy (off_screen, gms_graphics_bitmap,
					picture_size * sizeof (*off_screen));

		/*
		 * Apply any further animations.  If none, then stop the
		 * graphics "thread" and return.  There's no more to be done
		 * until something restarts us.
		 */
		more_animation = gms_graphics_animate (off_screen,
				gms_graphics_width, gms_graphics_height);
		if (!more_animation)
		    {
			/*
			 * There's one extra wrinkle here.  The base picture
			 * we've just put into the off-screen buffer isn't
			 * really complete (and for type2 animations, might
			 * be pure garbage), so if we happen to get a repaint
			 * after an animation has ended, the off-screen data
			 * we'll be painting could well look wrong.
			 *
			 * So... here we want to set the off-screen buffer to
			 * contain the final animation frame.  Fortunately,
			 * we still have it in the on-screen buffer.
			 */
			memcpy (off_screen, on_screen,
					picture_size * sizeof (*off_screen));
			gms_graphics_stop ();
			return;
		    }

		/*
		 * Re-assign layers based on animation changes to the off-
		 * screen buffer.
		 */
#ifndef GARGLK
		gms_graphics_assign_layers (off_screen, on_screen,
				gms_graphics_width, gms_graphics_height,
							layers, layer_usage);
#endif

		/*
		 * Set up an animation wait, adjusted here by the number of
		 * times we had to yield while rendering, as we're now that
		 * late with animations, and capped at zero, as we can't do
		 * anything to compensate for being too late.  In practice,
		 * we're running too close to the edge to have much of an
		 * effect here, but nevertheless...
		 */
		ignore_counter = GMS_GRAPHICS_ANIMATION_WAIT - 1;
		if (yield_counter > ignore_counter)
			ignore_counter = 0;
		else
			ignore_counter -= yield_counter;

		/* Start a fresh picture rendering pass. */
		yield_counter	= 0;
		saved_layer	= 0;
		saved_x		= 0;
		saved_y		= 0;
		total_regions	= 0;
	    }
	else
	    {
		/*
		 * Not an animated picture, so just stop graphics, as again,
		 * there's no more to be done until something restarts us.
		 */
		gms_graphics_stop ();
	    }
}


/*
 * ms_showpic()
 *
 * Called by the main interpreter when it wants us to display a picture.
 * The function gets the picture bitmap, palette, and dimensions, and
 * saves them, and the picture id, in module variables for the background
 * rendering function.
 *
 * The graphics window is opened if required, or closed if mode is zero.
 *
 * The function checks for changes of actual picture by calculating the
 * CRC for picture data; this helps to prevent unnecessary repaints in
 * cases where the interpreter passes us the same picture as we're already
 * displaying.  There is a less than 1 in 4,294,967,296 chance that a new
 * picture will be missed.  We'll live with that.
 *
 * Why use CRCs, rather than simply storing the values of picture passed in
 * a static variable?  Because some games, typically Magnetic Windows, use
 * the picture argument as a form of string pointer, and can pass in the
 * same value for several, perhaps all, game pictures.  If we just checked
 * for a change in the picture argument, we'd never see one.  So we must
 * instead look for changes in the real picture data.
 */
void
ms_showpic (type32 picture, type8 mode)
{
	static glui32	current_crc	= 0;	/* CRC of the current picture */

	type8		*bitmap;		/* New picture bitmap */
	type16		width, height;		/* New picture dimensions */
	type16		palette[GMS_PALETTE_SIZE];
						/* New picture palette */
	type8		animated;		/* New picture animation */
	long		picture_bytes;		/* Picture size in bytes */
	glui32		crc;			/* New picture's CRC */

	/* See if the mode indicates no graphics. */
	if (mode == 0)
	    {
		/* Note that the interpreter turned graphics off. */
		gms_graphics_interpreter = FALSE;

		/*
		 * If we are currently displaying the graphics window, stop
		 * any update "thread" and turn off graphics.
		 */
		if (gms_graphics_enabled
				&& gms_graphics_are_displayed ())
		    {
			gms_graphics_stop ();
			gms_graphics_close ();
		    }

		/* Nothing more to do now graphics are off. */
		return;
	    }

	/* Note that the interpreter turned graphics on. */
	gms_graphics_interpreter = TRUE;

	/*
	 * Obtain the image details for the requested picture.  The call
	 * returns NULL if there's a problem with the picture.
	 */
	bitmap = ms_extract (picture, &width, &height, palette, &animated);
	if (bitmap == NULL)
		return;

	/*
	 * Note the last thing passed to ms_extract, in case of graphics
	 * restarts.
	 */
	gms_graphics_picture = picture;

	/* Calculate the picture size, and the CRC for the bitmap data. */
	picture_bytes = width * height * sizeof (*bitmap);
	crc = gms_buffer_crc (bitmap, picture_bytes);

	/*
	 * If there is no change of picture, we might be able to largely
	 * ignore the call.  Check for a change, and if we don't see one,
	 * and if graphics are enabled and being displayed, we can safely
	 * ignore the call.
	 */
	if (width == gms_graphics_width
				&& height == gms_graphics_height
				&& crc == current_crc
				&& gms_graphics_enabled
				&& gms_graphics_are_displayed ())
		return;

	/*
	 * We know now that this is either a genuine change of picture, or
	 * graphics were off and have been turned on.  So, record picture
	 * details, ensure graphics is on, set the flags, and start the
	 * background graphics update.
	 */

	/*
	 * Save the picture details for the update code.  Here we take a
	 * complete local copy of the bitmap, since the interpreter core may
	 * reuse part of its memory for animations.
	 */
	if (gms_graphics_bitmap != NULL)
		free (gms_graphics_bitmap);
	gms_graphics_bitmap		= gms_malloc (picture_bytes);
	memcpy (gms_graphics_bitmap, bitmap, picture_bytes);
	gms_graphics_width		= width;
	gms_graphics_height		= height;
	memcpy (gms_graphics_palette, palette, sizeof (palette));
	gms_graphics_animated		= animated;

	/* Retain the new picture CRC. */
	current_crc = crc;

	/*
	 * If graphics are enabled, ensure the window is displayed, set the
	 * appropriate flags, and start graphics update.  If they're not
	 * enabled, the picture details will simply stick around in module
	 * variables until they are required.
	 */
	if (gms_graphics_enabled)
	    {
		/* Ensure graphics on, and return (fail) if not possible. */
		if (!gms_graphics_open ())
			return;

		/*
		 * Set the new picture flag, and start the updating
		 * "thread".
		 */
		gms_graphics_new_picture = TRUE;
		gms_graphics_start ();
	    }
}


/*
 * gms_graphics_picture_is_available()
 *
 * Return TRUE if the graphics module data is loaded with a usable picture,
 * FALSE if there is no picture available to display.
 */
static int
gms_graphics_picture_is_available (void)
{
	return (gms_graphics_bitmap != NULL);
}


/*
 * gms_graphics_get_picture_details()
 *
 * Return the width, height, and animation flag of the currently loaded
 * picture.  The function returns FALSE if no picture is loaded, otherwise
 * TRUE, with picture details in the return arguments.
 */
static int
gms_graphics_get_picture_details (int *width, int *height, int *is_animated)
{
	/* If no picture available, return FALSE. */
	if (!gms_graphics_picture_is_available ())
		return FALSE;

	/* If requested, return width, height, and animation directly. */
	if (width != NULL)
		*width		= gms_graphics_width;
	if (height != NULL)
		*height		= gms_graphics_height;
	if (is_animated != NULL)
		*is_animated	= gms_graphics_animated;

	/* Details returned. */
	return TRUE;
}


/*
 * gms_graphics_get_rendering_details()
 *
 * Returns the current level of applied gamma correction, as a string, the
 * count of colors in the picture, and a flag indicating if graphics is
 * active (busy).  The function return FALSE if graphics is not enabled or
 * if not being displayed, otherwise TRUE with the gamma, color count, and
 * active flag in the return arguments.
 *
 * This function races with the graphics timeout, as it returns information
 * set up by the first timeout following a new picture.  There's a very
 * very small chance that it might win the race, in which case out-of-date
 * gamma and color count values are returned.
 */
static int
gms_graphics_get_rendering_details (const char **gamma, int *color_count,
			int *is_active)
{
	/* If graphics isn't enabled and displayed, return FALSE. */
	if (!gms_graphics_enabled
			|| !gms_graphics_are_displayed ())
		return FALSE;

	/*
	 * Return the string representing the gamma correction.  If racing
	 * with timeouts, we might return the gamma for the last picture.
	 */
	if (gamma != NULL)
	    {
		assert (gms_graphics_current_gamma != NULL);
		*gamma = gms_graphics_current_gamma->level;
	    }

	/*
	 * Return the color count noted by timeouts on the first timeout
	 * following a new picture.  Again, we might return the one for
	 * the prior picture.
	 */
	if (color_count != NULL)
		*color_count = gms_graphics_color_count;

	/* Return graphics active flag. */
	if (is_active != NULL)
		*is_active = gms_graphics_active;

	/* Details returned. */
	return TRUE;
}


/*
 * gms_graphics_interpreter_enabled()
 *
 * Return TRUE if it looks like interpreter graphics are turned on, FALSE
 * otherwise.
 */
static int
gms_graphics_interpreter_enabled (void)
{
	return gms_graphics_interpreter;
}


/*
 * gms_graphics_cleanup()
 *
 * Free memory resources allocated by graphics functions.  Called on game
 * end.
 */
static void
gms_graphics_cleanup (void)
{
	if (gms_graphics_bitmap != NULL)
	    {
		free (gms_graphics_bitmap);
		gms_graphics_bitmap = NULL;
	    }
	if (gms_graphics_off_screen != NULL)
	    {
		free (gms_graphics_off_screen);
		gms_graphics_off_screen = NULL;
	    }
	if (gms_graphics_on_screen != NULL)
	    {
		free (gms_graphics_on_screen);
		gms_graphics_on_screen = NULL;
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk port status line functions                                     */
/*---------------------------------------------------------------------*/

/*
 * The interpreter feeds us status line characters one at a time, with
 * Tab indicating right justify, and CR indicating the line is complete.
 * To get this to fit with the Glk event and redraw model, here we'll
 * buffer each completed status line, so we have a stable string to
 * output when needed.  It's also handy to have this buffer for Glk lib-
 * raries that don't support separate windows.
 */
enum {			GMS_STATBUFFER_LENGTH		= 1024 };
static char		gms_status_buffer[GMS_STATBUFFER_LENGTH];
static int		gms_status_length		= 0;

/* Default width used for non-windowing Glk status lines. */
static const int	GMS_DEFAULT_STATUS_WIDTH	= 74;


/*
 * ms_statuschar()
 *
 * Receive one status character from the interpreter.  Characters are
 * buffered internally, and on CR, the buffer is copied to the main
 * static status buffer for use by the status line printing function.
 */
void
ms_statuschar (type8 c)
{
	static char	buffer[GMS_STATBUFFER_LENGTH];
						/* Local line buffer. */
	static int	length		= 0;	/* Local buffered data len. */

	/* If the status character is newline, transfer buffered data. */
	if (c == '\n')
	    {
		/* Transfer locally buffered string. */
		memcpy (gms_status_buffer, buffer, length);
		gms_status_length = length;

		/* Empty the local buffer. */
		length = 0;
		return;
	    }

	/* If there is space, add the character to the local buffer. */
	if (length < sizeof (buffer))
		buffer[ length++ ] = c;
}


/*
 * gms_status_update()
 *
 * Update the information in the status window with the current contents of
 * the completed status line buffer, or a default string if no completed
 * status line.
 */
static void
gms_status_update (void)
{
	glui32		width, height;		/* Status window dimensions. */
	strid_t		status_stream;		/* Status window stream. */
	int		index;			/* Buffer character index. */
	assert (gms_status_window != NULL);

	static int sizedstatus = 0;
	if (gms_status_length > 0 && !sizedstatus)
	    {
		winid_t parent;
		parent = glk_window_get_parent (gms_status_window);
		glk_window_set_arrangement (parent,
			winmethod_Above|winmethod_Fixed,
			1, NULL);
		sizedstatus = 1;
	    }

	/* Measure the status window, and do nothing if height is zero. */
	glk_window_get_size (gms_status_window, &width, &height);
	if (height == 0)
		return;

	/* Clear the current status window contents, and position cursor. */
	glk_window_clear (gms_status_window);
	status_stream = glk_window_get_stream (gms_status_window);

	glk_set_style_stream(status_stream, style_User1);
	for (index = 0; index < width; index++)
	    glk_put_char_stream(status_stream, ' ');

	width --;

	glk_window_move_cursor (gms_status_window, 1, 0);

	/* See if we have a completed status line to display. */
	if (gms_status_length > 0)
	    {
		/* Output each character from the status line buffer. */
		for (index = 0; index < gms_status_length; index++)
		    {
			/*
			 * If the character is Tab, position the cursor to
			 * eleven characters shy of the status window right.
			 */
			if (gms_status_buffer[ index ] == '\t')
				glk_window_move_cursor
					(gms_status_window, width - 11, 0);
			else
				/* Just add the character at this position. */
				glk_put_char_stream (status_stream,
						gms_status_buffer[ index ]);
		    }
	    }
	else
	    {
		/*
		 * We have no status line information to display, so just
		 * print a standard message.  This happens with Magnetic
		 * Windows games, which don't, in general, use a status line.
		 */
/*
		glk_put_string_stream (status_stream,
					"Glk Magnetic version 2.2");
*/
	    }
}


/*
 * gms_status_print()
 *
 * Print the current contents of the completed status line buffer out in the
 * main window, if it has changed since the last call.  This is for non-
 * windowing Glk libraries.
 */
static void
gms_status_print (void)
{
	static char	buffer[GMS_STATBUFFER_LENGTH];
						/* Local line buffer. */
	static int	length		= 0;	/* Local buffered data len. */

	int		index;			/* Buffer character index. */
	int		column;			/* Printout column. */

	/*
	 * Do nothing if there is no status line to print, or if the status
	 * line hasn't changed since last printed.
	 */
	if (gms_status_length == 0
			|| (gms_status_length == length
			    && !strncmp (buffer, gms_status_buffer, length)))
		return;

	/* Set fixed width font to try to preserve status line formatting. */
	glk_set_style (style_Preformatted);

	/* Bracket, and output the status line buffer. */
	glk_put_string ("[ ");
	column = 1;
	for (index = 0; index < gms_status_length; index++)
	    {
		/*
		 * If the character is Tab, position the cursor to eleven
		 * characters shy of the right edge.  In the absence of the
		 * real window dimensions, we'll select 74 characters, which
		 * gives us a 78 character status line; pretty standard.
		 */
		if (gms_status_buffer[ index ] == '\t')
		    {
			/* Output padding spaces. */
			while (column <= GMS_DEFAULT_STATUS_WIDTH - 11)
			    {
				glk_put_char (' ');
				column++;
			    }
		    }
		else
		    {
			/* Print the character, and update column. */
			glk_put_char (gms_status_buffer[ index ]);
			column++;
		    }
	    }

	/* Pad to default status width. */
	while (column <= GMS_DEFAULT_STATUS_WIDTH)
	    {
		glk_put_char (' ');
		column++;
	    }
	glk_put_string (" ]\n");

	/* Save the details of the printed status buffer. */
	memcpy (buffer, gms_status_buffer, gms_status_length);
	length = gms_status_length;
}


/*
 * gms_status_notify()
 *
 * Front end function for updating status.  Either updates the status window
 * or prints the status line to the main window.
 */
static void
gms_status_notify (void)
{
	/*
	 * If there is a status window, update it; if not, print the status
	 * line to the main window.
	 */
	if (gms_status_window != NULL)
		gms_status_update ();
	else
		gms_status_print ();
}


/*
 * gms_status_redraw()
 *
 * Redraw the contents of any status window with the buffered status string.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
static void
gms_status_redraw (void)
{
	/* If there is a status window, update it. */
	if (gms_status_window != NULL)
	    {
		winid_t		parent;		/* Status window parent. */

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
		parent = glk_window_get_parent (gms_status_window);
		glk_window_set_arrangement (parent,
					winmethod_Above|winmethod_Fixed,
		 			1, NULL);
		 */

		/* ...now update the status window. */
		gms_status_update ();
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk port output functions                                          */
/*---------------------------------------------------------------------*/

/* Kill extra newline that portable code prints thinking that Glk forgot */
static int		gms_kill_newline = 0;

/*
 * Output buffer.  We receive characters one at a time, and it's a bit
 * more efficient for everyone if we buffer them, and output a complete
 * string on a flush call.
 */
static const int	GMS_BUFFER_INCREMENT		= 1024;
static char		*gms_output_buffer		= NULL;
static int		gms_output_size			= 0;
static int		gms_output_length		= 0;

/*
 * Flag to indicate if the last buffer flushed looked like it ended in a
 * ">" prompt.
 */
static int		gms_output_prompt		= FALSE;


/*
 * gms_game_prompted()
 *
 * Return TRUE if the last game output appears to have been a ">" prompt.
 * Once called, the flag is reset to FALSE, and requires more game output
 * to set it again.
 */
static int
gms_game_prompted (void)
{
	int		result;				/* Return value. */

	/* Save the current flag value, and reset the main flag. */
	result = gms_output_prompt;
	gms_output_prompt = FALSE;

	/* Return the old value. */
	return result;
}


/*
 * gms_detect_game_prompt()
 *
 * See if the last non-newline-terminated line in the output buffer seems
 * to be a prompt, and set the game prompted flag if it does, otherwise
 * clear it.
 */
static void
gms_detect_game_prompt (void)
{
	int		index;			/* Output buffer iterator. */

	/* Begin with a clear prompt flag. */
	gms_output_prompt = FALSE;

	/* Search across any last unterminated buffered line. */
	for (index = gms_output_length - 1;
		index >= 0 && gms_output_buffer[ index ] != '\n';
		index--)
	    {
		/* Looks like a prompt if a non-space character found. */
		if (gms_output_buffer[ index ] != ' ')
		    {
			gms_output_prompt = TRUE;
			break;
		    }
	    }
}


/*
 * gms_output_delete()
 *
 * Delete all buffered output text.  Free all malloc'ed buffer memory, and
 * return the buffer variables to their initial values.
 */
static void
gms_output_delete (void)
{
	/* Clear and free the buffer of current contents. */
	if (gms_output_buffer != NULL)
		free (gms_output_buffer);
	gms_output_buffer = NULL;
	gms_output_size   = 0;
	gms_output_length = 0;
}


/*
 * gms_output_flush()
 *
 * Flush any buffered output text to the Glk main window, and clear the
 * buffer.
 */
static void
gms_output_flush (void)
{
	assert (glk_stream_get_current () != NULL);

	/* Do nothing if the buffer is currently empty. */
	if (gms_output_length > 0)
	    {
		/* See if the game issued a standard prompt. */
		gms_detect_game_prompt ();

		/*
		 * Print the buffer to the stream for the main window, in
		 * game output style.
		 */
		glk_set_style (style_Normal);
		glk_put_buffer (gms_output_buffer, gms_output_length);

		/* Clear and free the buffer of current contents. */
		gms_output_delete ();
	    }
}


/*
 * ms_putchar()
 *
 * Buffer a character for eventual printing to the main window.
 */
void
ms_putchar (type8 c)
{
	assert (gms_output_length <= gms_output_size);

	/* kill extraneous newline */
	if (c == '\n' && gms_kill_newline)
	{
	    gms_kill_newline = 0;
	    return;
	}

	gms_kill_newline = 0;

	/*
	 * See if the character is a backspace.  Magnetic Scrolls games
	 * can send backspace characters to the display.  We'll need to
	 * handle such characters specially.
	 */
	if (c == '\b')
	    {
		/* For a backspace, take a character out of the buffer. */
		if (gms_output_length > 0)
			gms_output_length--;

		/* Nothing more to do with this character. */
		return;
	    }

	/* Grow the output buffer if necessary. */
	if (gms_output_length == gms_output_size)
	    {
		gms_output_size += GMS_BUFFER_INCREMENT;
		gms_output_buffer = gms_realloc (gms_output_buffer,
							gms_output_size);
	    }

	/* Add the new character to the buffer. */
	gms_output_buffer[ gms_output_length++ ] = c;
}


/*
 * gms_standout_string()
 * gms_standout_char()
 *
 * Print a string in a style that stands out from game text.
 */
static void
gms_standout_string (const char *message)
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
gms_standout_char (char c)
{
	/* Print the character, in a message style. */
	glk_set_style (style_Emphasized);
	glk_put_char (c);
	glk_set_style (style_Normal);
}


/*
 * gms_normal_string()
 * gms_normal_char()
 *
 * Print a string in normal text style.
 */
static void
gms_normal_string (const char *message)
{
	assert (message != NULL);

	/* Print the message, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_string ((char *) message);
}

static void
gms_normal_char (char c)
{
	/* Print the character, in normal text style. */
	glk_set_style (style_Normal);
	glk_put_char (c);
}


/*
 * gms_header_string()
 * gms_banner_string()
 *
 * Print text messages for the banner at the start of a game run.
 */
static void
gms_header_string (const char *banner)
{
	assert (banner != NULL);

	/* Print the string in a header style. */
	glk_set_style (style_Header);
	glk_put_string ((char *) banner);
	glk_set_style (style_Normal);
}

static void
gms_banner_string (const char *banner)
{
	assert (banner != NULL);

	/* Print the banner in a subheader style. */
	glk_set_style (style_Subheader);
	glk_put_string ((char *) banner);
	glk_set_style (style_Normal);
}


/*
 * ms_fatal()
 *
 * Handle fatal interpreter error message.
 */
void
ms_fatal (type8s *string)
{
	/* Handle any updated status and pending buffered output. */
	gms_status_notify ();
	gms_output_flush ();

	/* Report the error, and exit. */
	gms_fatal (string);
	glk_exit ();
}


/*
 * ms_flush()
 *
 * Handle a core interpreter call to flush the output buffer.  Because
 * Glk only flushes its buffers and displays text on glk_select(), we
 * can ignore these calls as long as we call gms_output_flush() when
 * reading line input.
 *
 * Taking ms_flush() at face value can cause game text to appear before
 * status line text where we are working with a non-windowing Glk, so
 * it's best ignored where we can.
 */
void ms_flush (void)
{
}


/*---------------------------------------------------------------------*/
/*  Glk port hint functions                                            */
/*---------------------------------------------------------------------*/

/* Hint type definitions. */
enum {			GMS_HINT_TYPE_FOLDER		= 1,
			GMS_HINT_TYPE_TEXT		= 2 };

/* Success and fail return codes from hint functions. */
static const type8	GMS_HINT_SUCCESS		= 1;
static const type8	GMS_HINT_ERROR			= 0;

/* Default window sizes for non-windowing Glk libraries. */
static const glui32	GMS_HINT_DEFAULT_WIDTH		= 72;
static const glui32	GMS_HINT_DEFAULT_HEIGHT		= 25;

/*
 * Special hint nodes indicating the root hint node, and a value to signal
 * quit from hints subsystem.
 */
static const type16	GMS_HINT_ROOT_NODE		= 0;
static const type16	GMS_HINTS_DONE			= USHRT_MAX;

/* Generic hint topic for the root hints node. */
static const char	*GMS_GENERIC_TOPIC		= "Hints Menu";

/*
 * Note of the interpreter's hints array.  Note that keeping its address
 * like this assumes that it's either static or heap in the interpreter.
 */
static struct ms_hint	*gms_hints			= NULL;

/* Details of the current hint node on display from the hints array. */
static type16		gms_current_hint_node		= 0;

/*
 * Array of cursors for each hint.  The cursor indicates the current hint
 * position in a folder, and the last hint shown in text hints.  Space
 * is allocated as needed for a given set of hints, and needs to be freed
 * on interpreter exit.
 */
static int		*gms_hint_cursor		= NULL;


/*
 * gms_hint_max_node()
 *
 * Return the maximum hint node referred to by the tree under the given
 * node.  The result is the largest index found, or node, if greater.
 * Because the interpreter doesn't supply it, we need to uncover it the
 * hard way.  The function is recursive, and since it is a tree search,
 * assumes that hints is a tree, not a graph.
 */
static type16
gms_hint_max_node (struct ms_hint hints[], type16 node)
{
	int		index;		/* Folder element iterator. */
	type16		max_node = 0;	/* Largest linked node. */
	assert (hints != NULL);

	/* Handle according to the hint node type. */
	switch (hints[ node ].nodetype)
	    {
	    case GMS_HINT_TYPE_TEXT:
		/* The largest node here is simply this node. */
		max_node = node;
		break;

	    case GMS_HINT_TYPE_FOLDER:
		/* Initialize result to be the node handed in. */
		max_node = node;

		/* Recurse for each link in this node. */
		for (index = 0; index < hints[ node ].elcount; index++)
		    {
			type16	link_max;	/* Max for each link. */

			/*
			 * Find the maximum node reference for each link, and
			 * update the result if greater.
			 */
			link_max = gms_hint_max_node
					(hints, hints[ node ].links[ index ]);
			if (link_max > max_node)
				max_node = link_max;
		    }
		break;

	    default:
		gms_fatal ("GLK: Invalid hints node type encountered");
		glk_exit ();
	    }

	/*
	 * Return the largest node reference found, capped to avoid
	 * overlapping the special end-hints value.
	 */
	return (max_node < GMS_HINTS_DONE) ? max_node : GMS_HINTS_DONE - 1;
}


/*
 * gms_hint_content()
 *
 * Return the content string for a given hint number within a given node.
 * This counts over 'number' ASCII NULs in the node's content, returning
 * the address of the string located this way.
 */
static char *
gms_hint_content (struct ms_hint hints[], type16 node, int number)
{
	int		offset;		/* Content string offset. */
	int		count;		/* Content strings count. */
	assert (hints != NULL);

	/* Run through content until 'number' strings found. */
	offset = 0;
	for (count = 0; count < number; count++)
		offset += strlen (hints[ node ].content + offset) + 1;

	/* Return the start of the number'th string encountered. */
	return hints[ node ].content + offset;
}


/*
 * gms_hint_topic()
 *
 * Return the topic string for a given hint node.  This is found by searching
 * the parent node for a link to the node handed in.  For the root node, the
 * string is defaulted, since the root node has no parent.
 */
static const char *
gms_hint_topic (struct ms_hint hints[], type16 node)
{
	type16		parent;			/* Node's parent node. */
	int		index;			/* Node link iterator. */
	char		*topic;			/* Hint node's topic string. */
	assert (hints != NULL);

	/* If the node is the root node, return a generic string. */
	if (node == GMS_HINT_ROOT_NODE)
		return GMS_GENERIC_TOPIC;

	/* Locate the node's parent. */
	parent = hints[ node ].parent;

	/* Iterate over the parent's links. */
	topic = NULL;
	for (index = 0; index < hints[ parent ].elcount; index++)
	    {
		/* Check for a link match. */
		if (hints[ parent ].links[ index ] == node)
		    {
			topic = gms_hint_content (hints, parent, index);
			break;
		    }
	    }

	/* Return the topic, or if no match, a generic string. */
	return (topic != NULL) ? topic : GMS_GENERIC_TOPIC;
}


/*
 * gms_hint_open()
 *
 * If not already open, open the hints windows.  Returns TRUE if the windows
 * opened, or were already open.
 *
 * The function creates two hints windows -- a text grid on top, for menus,
 * and a text buffer below for hints.
 */
static int
gms_hint_open (void)
{
	/* If not already open, open the hints menu and text windows. */
	if (gms_hint_menu_window == NULL)
	    {
		assert (gms_hint_text_window == NULL);

		/*
		 * Open the hint menu window.  The initial size is two lines,
		 * but we'll change this later to suit the hint.
		 */
		gms_hint_menu_window = glk_window_open (gms_main_window,
					winmethod_Above|winmethod_Fixed,
					2, wintype_TextGrid, 0);

		/* If the window open fails, return FALSE. */
		if (gms_hint_menu_window == NULL)
			return FALSE;

		/*
		 * Now open the hints text window.  This is set to be 100% of
		 * the size of the main window, so should cover what remains
		 * of it completely.
		 */
		gms_hint_text_window = glk_window_open (gms_main_window,
					winmethod_Above|winmethod_Proportional,
					100, wintype_TextBuffer, 0);

		/* If this fails, close the menu window and return FALSE. */
		if (gms_hint_text_window == NULL)
		    {
			glk_window_close (gms_hint_menu_window, NULL);
			gms_hint_menu_window = NULL;
			return FALSE;
		    }
	    }

	/* Both hints windows opened successfully. */
	return TRUE;
}


/*
 * gms_hint_close()
 *
 * If open, close the hints windows.
 */
static void
gms_hint_close (void)
{
	/* If open, close the hints menu and text windows. */
	if (gms_hint_menu_window != NULL)
	    {
		assert (gms_hint_text_window != NULL);

		/* Close both the menu and the text hint windows. */
		glk_window_close (gms_hint_menu_window, NULL);
		gms_hint_menu_window = NULL;
		glk_window_close (gms_hint_text_window, NULL);
		gms_hint_text_window = NULL;
	    }
}


/*
 * gms_hint_windows_available()
 *
 * Return TRUE if hints windows are available.  If they're not, the hints
 * system will need to use alternative output methods.
 */
static int
gms_hint_windows_available (void)
{
	return (gms_hint_menu_window != NULL
			&& gms_hint_text_window != NULL);
}


/*
 * gms_hint_menu_print()
 * gms_hint_menu_header()
 * gms_hint_menu_justify()
 * gms_hint_text_print()
 * gms_hint_menutext_done()
 * gms_hint_menutext_start()
 *
 * Output functions for writing hints.  These functions will write to hints
 * windows where available, and to the main window where not.  When writing
 * to hints windows, they also take care not to line wrap in the menu window.
 * Limited formatting is available.
 */
static void
gms_hint_menu_print (int line, int column, const char *string,
			glui32 width, glui32 height)
{
	assert (string != NULL);

	/* Ignore the call if the text position is outside the window. */
	if (line > height
			|| column > width)
		return;

	/* See if hints windows are available. */
	if (gms_hint_windows_available ())
	    {
		strid_t		hint_stream;	/* Hint menu window stream */
		int		posn, index; 	/* Line and string positions */

		/* Get the hint menu window stream. */
		hint_stream = glk_window_get_stream (gms_hint_menu_window);

		/*
		 * Write characters until the end of the string, or the end of
		 * the window.
		 */
		glk_window_move_cursor (gms_hint_menu_window, column, line);
		for (posn = column, index = 0;
				posn < width && index < strlen (string);
				posn++, index++)
		    {
			/* Append character here, implies update to posn. */
			glk_put_char_stream (hint_stream, string[ index ]);
		    }
	    }
	else
	    {
		static int	current_line	= 0;	/* Retained line num */
		static int	current_column	= 0;	/* Retained col num */

		int		index;			/* Line/string index */

		/*
		 * Check the line number against the last one output.  If it
		 * is less, assume the start of a new block.  In this case,
		 * perform a hokey type of screen clear.
		 */
		if (line < current_line)
		    {
			/* Output a number of newlines. */
			for (index = 0; index < height; index++)
				gms_normal_char ('\n');

			/* Set the current "cursor" position to the origin. */
			current_line	= 0;
			current_column	= 0;
		    }

		/* Print blank lines until the target line is reached. */
		for (; current_line < line; current_line++)
		    {
			gms_normal_char ('\n');
			current_column	= 0;
		    }

		/* Now print spaces until the target column is reached. */
		for (; current_column < column; current_column++)
			gms_normal_char (' ');

		/*
		 * Write characters until the end of the string, or the end of
		 * the (self-imposed not-really-there) window.
		 */
		for (index = 0;
			current_column < width && index < strlen (string);
			current_column++, index++)
		    {
			/* Add character, implies update to current_column. */
			gms_normal_char (string[ index ]);
		    }
	    }
}

static void
gms_hint_menu_header (int line, const char *string,
			glui32 width, glui32 height)
{
	int		posn;			/* Line positioning */
	assert (string != NULL);

	/* Calculate the positioning for centered text. */
	posn = (strlen (string) < width)
				? (width - strlen (string)) / 2 : 0;

	/* Output the text in the approximate line center. */
	gms_hint_menu_print (line, posn, string, width, height);
}

static void
gms_hint_menu_justify (int line,
			const char *left_string, const char *right_string,
			glui32 width, glui32 height)
{
	int		posn;			/* Line positioning */
	assert (left_string != NULL && right_string != NULL);

	/* Write left text normally to window left. */
	gms_hint_menu_print (line, 0, left_string, width, height);

	/* Calculate the positioning for the right justified text. */
	posn = (strlen (right_string) < width)
				? (width - strlen (right_string)) : 0;

	/* Output the right text flush with the right of the window. */
	gms_hint_menu_print (line, posn, right_string, width, height);
}

static void
gms_hint_text_print (const char *string)
{
	assert (string != NULL);

	/* See if hints windows are available. */
	if (gms_hint_windows_available ())
	    {
		strid_t		hint_stream;	/* Hint text window stream */

		/* Output the string to the hint text window. */
		hint_stream = glk_window_get_stream (gms_hint_text_window);
		glk_put_string_stream (hint_stream, (char *) string);
	    }
	else
	    {
		/* Simply output the text to the main window. */
		gms_normal_string (string);
	    }
}

static void
gms_hint_menutext_start (void)
{
	/*
	 * Twiddle for non-windowing libraries; 'clear' the main window
	 * by writing a null string at line 1, then a null string at line
	 * 0.  This works because we know the current output line in
	 * gms_hint_menu_print() is zero, since we set it that way
	 * with gms_hint_menutext_done(), or if this is the first call,
	 * then that's its initial value.
	 */
	if (!gms_hint_windows_available ())
	    {
		gms_hint_menu_print (1, 0, "",
			GMS_HINT_DEFAULT_WIDTH, GMS_HINT_DEFAULT_HEIGHT);
		gms_hint_menu_print (0, 0, "",
			GMS_HINT_DEFAULT_WIDTH, GMS_HINT_DEFAULT_HEIGHT);
	    }
}

static void
gms_hint_menutext_done (void)
{
	/*
	 * Twiddle for non-windowing libraries; 'clear' the main window
	 * by writing an empty string to line zero.  For windowing Glk
	 * libraries, this function does nothing.
	 */
	if (!gms_hint_windows_available ())
	    {
		gms_hint_menu_print (0, 0, "",
			GMS_HINT_DEFAULT_WIDTH, GMS_HINT_DEFAULT_HEIGHT);
	    }
}


/*
 * gms_hint_menutext_char_event()
 *
 * Request and return a character event from the hints windows.  In practice,
 * this means either of the hints windows if available, or the main window
 * if not.
 */
static void
gms_hint_menutext_char_event (event_t *event)
{
	assert (event != NULL);

	/* See if hints windows are available. */
	if (gms_hint_windows_available ())
	    {
		/* Request a character from either hint window. */
		glk_request_char_event (gms_hint_menu_window);
		glk_request_char_event (gms_hint_text_window);

		/* Wait for either event to arrive. */
		gms_event_wait (evtype_CharInput, event);
		assert (event->win == gms_hint_menu_window
				|| event->win == gms_hint_text_window);

		/* Cancel the unused event request. */
		if (event->win == gms_hint_text_window)
			glk_cancel_char_event (gms_hint_menu_window);
		else
			if (event->win == gms_hint_menu_window)
				glk_cancel_char_event (gms_hint_text_window);
	    }
	else
	    {
		/* Request a character from the main window, and wait. */
		glk_request_char_event (gms_main_window);
		gms_event_wait (evtype_CharInput, event);
	    }
}


/*
 * gms_hint_arrange_windows()
 *
 * Arrange the hints windows so that the hint menu window has the requested
 * number of lines.  Returns the actual hint menu window width and height,
 * or defaults if no hints windows are available.
 */
static void
gms_hint_arrange_windows (int requested_lines, glui32 *width, glui32 *height)
{
	/* If we have hints windows, resize them as requested. */
	if (gms_hint_windows_available ())
	    {
		winid_t		parent;		/* Hint menu parent. */

		/* Resize the hint menu window to fit the current hint. */
		parent = glk_window_get_parent (gms_hint_menu_window);
		glk_window_set_arrangement (parent,
						winmethod_Above|winmethod_Fixed,
			 			requested_lines, NULL);

		/* Measure, and return the size of the hint menu window. */
		glk_window_get_size (gms_hint_menu_window, width, height);

		/* Clear both the hint menu and the hint text window. */
		glk_window_clear (gms_hint_menu_window);
		glk_window_clear (gms_hint_text_window);
	    }
	else
	    {
		/*
		 * No hints windows, so default width and height.  The hints
		 * output functions will cope with this.
		 */
		if (width != NULL)
			*width	= GMS_HINT_DEFAULT_WIDTH;
		if (height != NULL)
			*height	= GMS_HINT_DEFAULT_HEIGHT;
	    }

}


/*
 * gms_hint_display_folder()
 *
 * Update the hints windows for the given folder hint node.
 */
static void
gms_hint_display_folder (struct ms_hint hints[], int cursor[], type16 node)
{
	glui32		width, height;		/* Menu window dimensions */
	int		line;			/* Menu display line number */
	int		index;			/* General loop index */
	assert (hints != NULL && cursor != NULL);

	/*
	 * Arrange windows to suit the hint folder.  For a folder menu
	 * window we use one line for each element, three for the controls,
	 * and two spacers, making a total of five additional lines.  Width
	 * and height receive the actual menu window dimensions.
	 */
	gms_hint_arrange_windows (hints[ node ].elcount + 5, &width, &height);

	/* Paint in the menu header. */
	line = 0;
	gms_hint_menu_header (line++,
				gms_hint_topic (hints, node),
				width, height);
	gms_hint_menu_justify (line++,
				" N = next subject  ",
				"  P = previous ",
				width, height);
	gms_hint_menu_justify (line++,
				" RETURN = read subject  ",
				(node == GMS_HINT_ROOT_NODE)
					? "  Q = resume game "
					: "  Q = previous menu ",
				width, height);

	/* Output a blank line, then the menu for the node's folder hint. */
	line++;
	for (index = 0; index < hints[ node ].elcount; index++)
	    {
		/* Set pointer to the selected hint. */
		if (index == cursor[ node ])
			gms_hint_menu_print (line, 3, ">", width, height);

		/* Print the folder text. */
		gms_hint_menu_print (line++, 5,
				gms_hint_content (hints, node, index),
				width, height);
	    }

	/*
	 * Terminate with a blank line; using a single space here improves
	 * cursor positioning for optimized output libraries (for example,
	 * without it, curses output will leave the cursor at the end of
	 * the previous line).
	 */
	gms_hint_menu_print (line, 0, " ", width, height);
}


/*
 * gms_hint_display_text()
 *
 * Update the hints windows for the given text hint node.
 */
static void
gms_hint_display_text (struct ms_hint hints[], int cursor[], type16 node)
{
	glui32		width, height;		/* Menu window dimensions */
	int		line;			/* Menu display line number */
	int		index;			/* General loop index */
	assert (hints != NULL && cursor != NULL);

	/*
	 * Arrange windows to suit the hint text.  For a hint menu, we use
	 * a simple two-line set of controls; everything else is in the
	 * hints text window.  Width and height receive the actual menu
	 * window dimensions.
	 */
	gms_hint_arrange_windows (2, &width, &height);

	/* Paint in a short menu header. */
	line = 0;
	gms_hint_menu_header (line++,
				gms_hint_topic (hints, node),
				width, height);
	gms_hint_menu_justify (line++,
				" RETURN = read hint  ",
				"  Q = previous menu ",
				width, height);

	/*
	 * Output hints to the hints text window.  Hints not yet exposed are
	 * indicated by the cursor for the hint, and are displayed as a dash.
	 */
	gms_hint_text_print ("\n");
	for (index = 0; index < hints[ node ].elcount; index++)
	    {
		char	buffer[16];		/* Conversion buffer */

		/* Output the hint index number. */
		sprintf (buffer, "%3d.  ", index + 1);
		gms_hint_text_print (buffer);

		/* Output the hint, or a dash if it's not yet exposed. */
		if (index < cursor[ node ])
			gms_hint_text_print
				(gms_hint_content (hints, node, index));
		else
			gms_hint_text_print ("-");
		gms_hint_text_print ("\n");
	    }
}


/*
 * gms_hint_display()
 *
 * Display the given hint using the appropriate display function.
 */
static void
gms_hint_display (struct ms_hint hints[], int cursor[], type16 node)
{
	assert (hints != NULL && cursor != NULL);

	/* Call the right display function for the current hint. */
	switch (hints[ node ].nodetype)
	    {
	    case GMS_HINT_TYPE_TEXT:
		gms_hint_display_text (hints, cursor, node);
		break;

	    case GMS_HINT_TYPE_FOLDER:
		gms_hint_display_folder (hints, cursor, node);
		break;

	    default:
		gms_fatal ("GLK: Invalid hints node type encountered");
		glk_exit ();
	    }
}


/*
 * gms_hint_handle_folder()
 *
 * Handle a Glk keycode for the given folder hint.  Return the next node to
 * handle, or the special end-hints on Quit at the root node.
 */
static type16
gms_hint_handle_folder (struct ms_hint hints[], int cursor[], type16 node,
			glui32 keycode)
{
	unsigned char	response;		/* Response character */
	type16		next_node;		/* Return value */
	assert (hints != NULL && cursor != NULL);

	/* Convert key code into a single response character. */
	switch (keycode)
	    {
	    case keycode_Down:		response = 'N';  break;
	    case keycode_Up:		response = 'P';  break;
	    case keycode_Right:
	    case keycode_Return:	response = '\n'; break;
	    case keycode_Left:
	    case keycode_Escape:	response = 'Q';  break;
	    default:
		response = (keycode <= UCHAR_MAX) ?
				glk_char_to_upper (keycode) : 0;
		break;
	    }

	/*
	 * Now handle the response character.  We'll default the next node
	 * to be this node, but a response case can change it.
	 */
	next_node = node;
	switch (response)
	    {
	    case 'N':
		/* Advance the hint cursor, wrapping at the folder end. */
		if (cursor[ node ] < hints[ node ].elcount - 1)
			cursor[ node ]++;
		else
			cursor[ node ] = 0;
		break;

	    case 'P':
		/* Regress the hint cursor, wrapping at the folder start. */
		if (cursor[ node ] > 0)
			cursor[ node ]--;
		else
			cursor[ node ] = hints[ node ].elcount - 1;
		break;

	    case '\n':
		/* The next node is the hint node at the cursor position. */
		next_node = hints[ node ].links[ cursor[ node ]];
		break;

	    case 'Q':
		/* If root, we're done; if not, next node is node's parent. */
		if (node == GMS_HINT_ROOT_NODE)
			next_node = GMS_HINTS_DONE;
		else
			next_node = hints[ node ].parent;
		break;

	    default:
		break;
	    }

	/* Return the next node identified for handling. */
	return next_node;
}


/*
 * gms_hint_handle_text()
 *
 * Handle a Glk keycode for the given text hint.  Return the next node to
 * handle.
 */
static type16
gms_hint_handle_text (struct ms_hint hints[], int cursor[], type16 node,
			glui32 keycode)
{
	unsigned char	response;		/* Response character */
	type16		next_node;		/* Return value */
	assert (hints != NULL && cursor != NULL);

	/* Convert key code into a single response character. */
	switch (keycode)
	    {
	    case keycode_Right:
	    case keycode_Return:	response = '\n'; break;
	    case keycode_Left:
	    case keycode_Escape:	response = 'Q';  break;
	    default:
		response = (keycode <= UCHAR_MAX) ?
				glk_char_to_upper (keycode) : 0;
		break;
	    }

	/*
	 * Now handle the response character.  We'll default the next node
	 * to be this node, but a response case can change it.
	 */
	next_node = node;
	switch (response)
	    {
	    case '\n':
		/* If not at end of the hint, advance the hint cursor. */
		if (cursor[ node ] < hints[ node ].elcount)
			cursor[ node ]++;
		break;

	    case 'Q':
		/* Done with this hint node, so next node is its parent. */
		next_node = hints[ node ].parent;
		break;

	    default:
		break;
	    }

	/* Return the next node identified for handling. */
	return next_node;
}


/*
 * gms_hint_handle()
 *
 * Handle a Glk keycode for the given hint using the appropriate handler
 * function.  Return the next node to handle.
 */
static type16
gms_hint_handle (struct ms_hint hints[], int cursor[], type16 node,
			glui32 keycode)
{
	type16	next_node	= GMS_HINT_ROOT_NODE;	/* Return value */
	assert (hints != NULL && cursor != NULL);

	/* Call the right handler function for the current hint. */
	switch (hints[ node ].nodetype)
	    {
	    case GMS_HINT_TYPE_TEXT:
		next_node = gms_hint_handle_text
					(hints, cursor, node, keycode);
		break;

	    case GMS_HINT_TYPE_FOLDER:
		next_node = gms_hint_handle_folder
					(hints, cursor, node, keycode);
		break;

	    default:
		gms_fatal ("GLK: Invalid hints node type encountered");
		glk_exit ();
	    }

	/* Return the next node identified for handling. */
	return next_node;
}


/*
 * ms_showhints()
 *
 * Start game hints.  These are modal, though there's no overriding Glk
 * reason why.  It's just that this matches the way they're implemented by
 * most Inform games.  This may not be the best way of doing help, but at
 * least it's likely to be familiar, and anything more ambitious may be
 * beyond the current Glk capabilities.
 *
 * This function uses CRCs to detect any change of hints data.  Normally,
 * we'd expect none, at least within a given game run, but we can probably
 * handle it okay if it happens.
 */
type8
ms_showhints (struct ms_hint *hints)
{
	static int	initialized	= FALSE;/* First call flag. */
	static glui32	current_crc	= 0;	/* CRC of the current hints */

	type16		hint_count;		/* Count of hints in array */
	glui32		crc;			/* Hints array CRC */
	assert (hints != NULL);

	/*
	 * Find the number of hints in the array.  To do this, we'll visit
	 * every node in a tree search, starting at the root, to locate the
	 * maximum node number found, then add one to that.  It's a pity
	 * that the interpreter doesn't hand us this information directly.
	 */
	hint_count = gms_hint_max_node (hints, GMS_HINT_ROOT_NODE) + 1;

	/* Calculate a CRC for the hints array data. */
	crc = gms_buffer_crc ((unsigned char *) hints,
					hint_count * sizeof (*hints));

	/*
	 * If the CRC has changed, or this is the first call, assign a new
	 * cursor array.
	 */
	if (crc != current_crc
			|| !initialized)
	    {
		type16	index;			/* Array iterator */

		/* Free any current cursor array, and create a new one. */
		if (gms_hint_cursor != NULL)
			free (gms_hint_cursor);
		gms_hint_cursor = gms_malloc (hint_count
						* sizeof (*gms_hint_cursor));

		/* Reset all hint cursors to an initial state. */
		for (index = 0; index < hint_count; index++)
			gms_hint_cursor[ index ] = 0;

		/*
		 * Retain the hints CRC, for later comparisons, and set the
		 * initialized flag.
		 */
		current_crc = crc;
		initialized = TRUE;
	    }

	/*
	 * Save the hints array passed in.  This is done here since even
	 * if the data remains the same (found by the CRC check above),
	 * the pointer to it might have changed.
	 */
	gms_hints = hints;

	/*
	 * Try to create the hints windows.  If they can't be created,
	 * perhaps because the Glk library doesn't support it, the output
	 * functions will work around this.
	 */
	gms_hint_open ();
	gms_hint_menutext_start ();

	/*
	 * Begin hints display at the root node, and navigate until the
	 * user exit hints.
	 */
	gms_current_hint_node = GMS_HINT_ROOT_NODE;
	while (gms_current_hint_node != GMS_HINTS_DONE)
	    {
		event_t		event;		/* Glk event buffer */

		/* Display the node being visited. */
		assert (gms_current_hint_node < hint_count);
		gms_hint_display (gms_hints,
				gms_hint_cursor, gms_current_hint_node);

		/* Get a character key event for hint navigation. */
		gms_hint_menutext_char_event (&event);
		assert (event.type == evtype_CharInput);

		/* Handle the keycode for the hint, and advance node. */
		gms_current_hint_node = gms_hint_handle
					(gms_hints,
					gms_hint_cursor,
					gms_current_hint_node, event.val1);
	    }

	/* Done with hint windows. */
	gms_hint_menutext_done ();
	gms_hint_close ();

	/* Return successfully. */
	return GMS_HINT_SUCCESS;
}


/*
 * gms_hint_redraw()
 *
 * Update the hints windows for the current hint.  This function should be
 * called from the event handler on resize events, to repaint the hints
 * display.  It does nothing if no hints windows have been opened, since
 * in this case, there's no resize action required -- either we're not in
 * the hints subsystem, or hints are being displayed in the main game
 * window, for whatever reason.
 */
static void
gms_hint_redraw (void)
{
	/* If no hint windows are open, ignore the call. */
	if (gms_hint_windows_available ())
	    {
		/* Draw in the current hint. */
		assert (gms_hints != NULL && gms_hint_cursor != NULL);
		gms_hint_display (gms_hints,
				gms_hint_cursor, gms_current_hint_node);
	    }
}


/*
 * gms_hints_cleanup()
 *
 * Free memory resources allocated by hints functions.  Called on game
 * end.
 */
static void
gms_hints_cleanup (void)
{
	if (gms_hint_cursor != NULL)
	    {
		free (gms_hint_cursor);
		gms_hint_cursor = NULL;
	    }
}


/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/* Valid command control values. */
static const char	*GMS_COMMAND_ON			= "on";
static const char	*GMS_COMMAND_OFF		= "off";


/*
 * gms_command_undo()
 *
 * Stub function for the undo command.  The real work is to return the
 * undo code to the input functions.
 */
static void
gms_command_undo (const char *argument)
{
}


/*
 * gms_command_script()
 *
 * Turn game output scripting (logging) on and off.
 */
static void
gms_command_script (const char *argument)
{
	assert (argument != NULL);

	/* Set up a transcript according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a transcript is already active. */
		if (gms_transcript_stream != NULL)
		    {
			gms_normal_string ("Glk transcript is already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a transcript. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_Transcript | fileusage_TextMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gms_standout_string ("Glk transcript failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gms_transcript_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gms_transcript_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gms_standout_string ("Glk transcript failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Set the new transcript stream as the main echo stream. */
		glk_window_set_echo_stream (gms_main_window,
						gms_transcript_stream);

		/* Confirm action. */
		gms_normal_string ("Glk transcript is now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* If a transcript is active, close it. */
		if (gms_transcript_stream != NULL)
		    {
			glk_stream_close (gms_transcript_stream, NULL);
			gms_transcript_stream = NULL;

			/* Clear the main echo stream. */
			glk_window_set_echo_stream (gms_main_window, NULL);

			/* Confirm action. */
			gms_normal_string ("Glk transcript is now ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
		    }
		else
		    {
			/* Note that scripts are already disabled. */
			gms_normal_string ("Glk transcript is already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * transcript mode.
		 */
		gms_normal_string ("Glk transcript is ");
		if (gms_transcript_stream != NULL)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk transcript can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}



/*
 * gms_command_inputlog()
 *
 * Turn game input logging on and off.
 */
static void
gms_command_inputlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up an input log according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if an input log is already active. */
		if (gms_inputlog_stream != NULL)
		    {
			gms_normal_string ("Glk input logging is already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for an input log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_WriteAppend, 0);
		if (fileref == NULL)
		    {
			gms_standout_string ("Glk input logging failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gms_inputlog_stream = glk_stream_open_file
					(fileref, filemode_WriteAppend, 0);
		if (gms_inputlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gms_standout_string ("Glk input logging failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gms_normal_string ("Glk input logging is now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* If an input log is active, close it. */
		if (gms_inputlog_stream != NULL)
		    {
			glk_stream_close (gms_inputlog_stream, NULL);
			gms_inputlog_stream = NULL;

			/* Confirm action. */
			gms_normal_string ("Glk input log is now ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current input log. */
			gms_normal_string ("Glk input logging is already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * input logging mode.
		 */
		gms_normal_string ("Glk input logging is ");
		if (gms_inputlog_stream != NULL)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk input logging can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/*
 * gms_command_readlog()
 *
 * Set the game input log, to read input from a file.
 */
static void
gms_command_readlog (const char *argument)
{
	assert (argument != NULL);

	/* Set up a read log according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		frefid_t	fileref;	/* Glk file reference. */

		/* See if a read log is already active. */
		if (gms_readlog_stream != NULL)
		    {
			gms_normal_string ("Glk read log is already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/* Get a Glk file reference for a read log. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_InputRecord | fileusage_BinaryMode,
					filemode_Read, 0);
		if (fileref == NULL)
		    {
			gms_standout_string ("Glk read log failed.\n");
			return;
		    }

		/*
		 * Reject the file reference if we're expecting to read
		 * from it, and the referenced file doesn't exist.
		 */
		if (!glk_fileref_does_file_exist (fileref))
		    {
			glk_fileref_destroy (fileref);
			gms_standout_string ("Glk read log failed.\n");
			return;
		    }

		/* Open a Glk stream for the fileref. */
		gms_readlog_stream = glk_stream_open_file
					(fileref, filemode_Read, 0);
		if (gms_readlog_stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			gms_standout_string ("Glk read log failed.\n");
			return;
		    }
		glk_fileref_destroy (fileref);

		/* Confirm action. */
		gms_normal_string ("Glk read log is now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* If a read log is active, close it. */
		if (gms_readlog_stream != NULL)
		    {
			glk_stream_close (gms_readlog_stream, NULL);
			gms_readlog_stream = NULL;

			/* Confirm action. */
			gms_normal_string ("Glk read log is now ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
		    }
		else
		    {
			/* Note that there is no current read log. */
			gms_normal_string ("Glk read log is already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
		    }
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * read logging mode.
		 */
		gms_normal_string ("Glk read log is ");
		if (gms_readlog_stream != NULL)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk read log can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/*
 * gms_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gms_command_abbreviations (const char *argument)
{
	assert (argument != NULL);

	/* Set up abbreviation expansions according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		/* See if expansions are already on. */
		if (gms_abbreviations_enabled)
		    {
			gms_normal_string ("Glk abbreviation expansions"
					" are already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions on. */
		gms_abbreviations_enabled = TRUE;
		gms_normal_string ("Glk abbreviation expansions are now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* See if expansions are already off. */
		if (!gms_abbreviations_enabled)
		    {
			gms_normal_string ("Glk abbreviation expansions"
					" are already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
			return;
		    }

		/* The user has turned expansions off. */
		gms_abbreviations_enabled = FALSE;
		gms_normal_string ("Glk abbreviation expansions are now ");
		gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * expansion mode.
		 */
		gms_normal_string ("Glk abbreviation expansions are ");
		if (gms_abbreviations_enabled)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk abbreviation expansions can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/*
 * gms_command_print_version_number()
 * gms_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gms_command_print_version_number (glui32 version)
{
	char		buffer[64];		/* Output buffer string. */

	/* Print out the three version number component parts. */
	sprintf (buffer, "%lu.%lu.%lu",
			(version & 0xFFFF0000) >> 16,
			(version & 0x0000FF00) >>  8,
			(version & 0x000000FF)      );
	gms_normal_string (buffer);
}
static void
gms_command_version (const char *argument)
{
	glui32		version;		/* Glk lib version number. */

	/* Get the Glk library version number. */
	version = glk_gestalt (gestalt_Version, 0);

	/* Print the Glk library and port version numbers. */
	gms_normal_string ("The Glk library version is ");
	gms_command_print_version_number (version);
	gms_normal_string (".\n");
	gms_normal_string ("This is version ");
	gms_command_print_version_number (GMS_PORT_VERSION);
	gms_normal_string (" of the Glk Magnetic port.\n");
}


/*
 * gms_command_graphics()
 *
 * Enable or disable graphics more permanently than is done by the
 * main interpreter.  Also, print out a few brief details about the
 * graphics state of the program.
 */
static void
gms_command_graphics (const char *argument)
{
	assert (argument != NULL);

	/* If graphics is not possible, simply say so and return. */
	if (!gms_graphics_possible)
	    {
		gms_normal_string ("Glk graphics are not available.\n");
		return;
	    }

	/* Set up command handling according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		/* See if graphics are already on. */
		if (gms_graphics_enabled)
		    {
			gms_normal_string ("Glk graphics are already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/* Set the graphics enabled flag to on. */
		gms_graphics_enabled = TRUE;

		/*
		 * If there is a picture loaded and ready, call the
		 * restart function to repaint it.
		 */
		if (gms_graphics_picture_is_available ())
		    {
			if (!gms_graphics_open ())
			    {
				gms_normal_string ("Glk graphics error.\n");
				return;
			    }
			gms_graphics_restart ();
		    }

		/* Confirm actions. */
		gms_normal_string ("Glk graphics are now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* See if graphics are already off. */
		if (!gms_graphics_enabled)
		    {
			gms_normal_string ("Glk graphics are already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
			return;
		    }

		/*
		 * Set graphics to disabled, and stop any graphics
		 * processing.  Close the graphics window.
		 */
		gms_graphics_enabled = FALSE;
		gms_graphics_stop ();
		gms_graphics_close ();

		/* Confirm actions. */
		gms_normal_string ("Glk graphics are now ");
		gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/* Print details about graphics availability. */
		gms_normal_string ("Glk graphics are available,");
		if (gms_graphics_enabled)
			gms_normal_string (" and enabled.\n");
		else
			gms_normal_string (" but disabled.\n");

		/* See if there is any current loaded picture. */
		if (gms_graphics_picture_is_available ())
		    {
			int	status;		/* Status of picture query */
			int	width, height;	/* Current picture size */
			int	is_animated;	/* Animation flag */

			/* Print the picture's dimensions and animation. */
			status = gms_graphics_get_picture_details
						(&width, &height, &is_animated);
			if (status)
			    {
				char	buffer[16];

				gms_normal_string ("There is a"
							" picture loaded, ");
				sprintf (buffer, "%d", width);
				gms_normal_string (buffer);
				gms_normal_string (" by ");
				sprintf (buffer, "%d", height);
				gms_normal_string (buffer);
				gms_normal_string (" pixels");
				if (is_animated)
					gms_normal_string (", animated");
				gms_normal_string (".\n");
			    }
		    }

		/* Indicate the state of interpreter graphics. */
		if (!gms_graphics_interpreter_enabled ())
			gms_normal_string ("Interpreter graphics are"
						" disabled.\n");

		/* Show if graphics are displayed, or not. */
		if (gms_graphics_enabled
				&& gms_graphics_are_displayed ())
		    {
			int		status;		/* Query status */
			const char	*gamma;		/* Current gamma */
			int		color_count;	/* Count of colors */
			int		is_active;	/* Graphics busy flag */

			/* Indicate graphics display. */
			status = gms_graphics_get_rendering_details
					(&gamma, &color_count, &is_active);
			if (status)
			    {
				char	buffer[16];

				/* Print activity and colors count. */
				gms_normal_string ("Graphics are ");
				if (is_active)
					gms_normal_string ("active, ");
				else
					gms_normal_string ("displayed, ");
				sprintf (buffer, "%d", color_count);
				gms_normal_string (buffer);
				gms_normal_string (" colours");

				/* Add a note about gamma corrections. */
				if (gms_gamma_mode != GAMMA_OFF)
				    {
					gms_normal_string (", with gamma ");
					gms_normal_string (gamma);
					gms_normal_string (" correction");
				    }
				else
				    {
					gms_normal_string (", without gamma"
							" correction");
				    }
				gms_normal_string (".\n");
			    }
			else
				gms_normal_string ("Graphics are"
							" being displayed.\n");
		    }
		if (gms_graphics_enabled
				&& !gms_graphics_are_displayed ())
		    {
			/* Indicate no current graphics display. */
			gms_normal_string ("Graphics are"
						" not being displayed.\n");
		    }
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk graphics can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/* List of additional valid gamma control values. */
static const char	*GMS_COMMAND_NONE		= "none";
static const char	*GMS_COMMAND_NORMAL		= "normal";
static const char	*GMS_COMMAND_HIGH		= "high";

/*
 * gms_command_gamma()
 *
 * Enable or disable picture gamma corrections.
 */
static void
gms_command_gamma (const char *argument)
{
	assert (argument != NULL);

	/*
	 * If graphics is not possible, there is no point fiddling about
	 * with gamma corrections.
	 */
	if (!gms_graphics_possible)
	    {
		gms_normal_string ("Glk automatic gamma correction is"
							" not available.\n");
		return;
	    }

	/* Set up gamma correction according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_HIGH))
	    {
		/* See if gamma correction is already high. */
		if (gms_gamma_mode == GAMMA_HIGH)
		    {
			gms_normal_string ("Glk automatic gamma correction"
						" mode is already '");
			gms_normal_string (GMS_COMMAND_HIGH);
			gms_normal_string ("'.\n");
			return;
		    }

		/* Set the gamma to high, and restart graphics. */
		gms_gamma_mode = GAMMA_HIGH;
		gms_graphics_restart ();

		/* Confirm actions. */
		gms_normal_string ("Glk automatic gamma"
						" correction mode is now '");
		gms_normal_string (GMS_COMMAND_HIGH);
		gms_normal_string ("'.\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_NORMAL)
			|| !gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		/* See if gamma correction is already normal. */
		if (gms_gamma_mode == GAMMA_NORMAL)
		    {
			gms_normal_string ("Glk automatic gamma correction"
						" mode is already '");
			gms_normal_string (GMS_COMMAND_NORMAL);
			gms_normal_string ("'.\n");
			return;
		    }

		/* Set the gamma to normal, and restart graphics. */
		gms_gamma_mode = GAMMA_NORMAL;
		gms_graphics_restart ();

		/* Confirm actions. */
		gms_normal_string ("Glk automatic gamma"
						" correction mode is now '");
		gms_normal_string (GMS_COMMAND_NORMAL);
		gms_normal_string ("'.\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_NONE)
			|| !gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* See if gamma correction is already off. */
		if (gms_gamma_mode == GAMMA_OFF)
		    {
			gms_normal_string ("Glk automatic gamma correction"
						" mode is already '");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string ("'.\n");
			return;
		    }

		/* Set the gamma to off, and restart graphics. */
		gms_gamma_mode = GAMMA_OFF;
		gms_graphics_restart ();

		/* Confirm actions. */
		gms_normal_string ("Glk automatic gamma"
						" correction mode is now '");
		gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string ("'.\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * gamma correction mode.
		 */
		gms_normal_string ("Glk automatic gamma"
						" correction mode is '");
		switch (gms_gamma_mode)
		    {
		    case GAMMA_OFF:
			gms_normal_string (GMS_COMMAND_OFF);
			break;
		    case GAMMA_NORMAL:
			gms_normal_string (GMS_COMMAND_NORMAL);
			break;
		    case GAMMA_HIGH:
			gms_normal_string (GMS_COMMAND_HIGH);
			break;
		    }
		gms_normal_string ("'.\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string
				("Glk automatic gamma correction mode can be ");
		gms_standout_string (GMS_COMMAND_HIGH);
		gms_normal_string (", ");
		gms_standout_string (GMS_COMMAND_NORMAL);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/*
 * gms_command_animations()
 *
 * Enable or disable picture animations.
 */
static void
gms_command_animations (const char *argument)
{
	assert (argument != NULL);

	/*
	 * If graphics is not possible, there is no point fiddling about
	 * with animations.
	 */
	if (!gms_graphics_possible)
	    {
		gms_normal_string ("Glk graphics animations are"
							" not available.\n");
		return;
	    }

	/* Set up animations according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		/* See if animations are already on. */
		if (gms_animation_enabled)
		    {
			gms_normal_string
				("Glk graphics animations are already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/*
		 * Set the animations to on, and restart graphics if the
		 * current picture is animated; if it isn't, we can leave
		 * it displayed as is, since changing animation mode doesn't
		 * affect this picture.
		 */
		gms_animation_enabled = TRUE;
		if (gms_graphics_animated)
			gms_graphics_restart ();

		/* Confirm actions. */
		gms_normal_string ("Glk graphics animations are now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* See if animations are already off. */
		if (!gms_animation_enabled)
		    {
			gms_normal_string
				("Glk graphics animations are already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
			return;
		    }

		/* Set the animations to off, and maybe restart graphics. */
		gms_animation_enabled = FALSE;
		if (gms_graphics_animated)
			gms_graphics_restart ();

		/* Confirm actions. */
		gms_normal_string ("Glk graphics animations are now ");
		gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * animations mode.
		 */
		gms_normal_string ("Glk graphics animations are ");
		if (gms_animation_enabled)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string
				("Glk graphics animations can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/*
 * gms_command_prompts()
 *
 * Turn the extra "> " prompt output on and off.
 */
static void
gms_command_prompts (const char *argument)
{
	assert (argument != NULL);

	/* Set up prompt according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		/* See if prompt is already on. */
		if (gms_prompt_enabled)
		    {
			gms_normal_string ("Glk extra prompts are already ");
			gms_normal_string (GMS_COMMAND_ON);
			gms_normal_string (".\n");
			return;
		    }

		/* The user has turned prompt on. */
		gms_prompt_enabled = TRUE;
		gms_normal_string ("Glk extra prompts are now ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");

		/* Check for a game prompt to clear the flag. */
		gms_game_prompted ();
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* See if prompt is already off. */
		if (!gms_prompt_enabled)
		    {
			gms_normal_string ("Glk extra prompts are already ");
			gms_normal_string (GMS_COMMAND_OFF);
			gms_normal_string (".\n");
			return;
		    }

		/* The user has turned prompt off. */
		gms_prompt_enabled = FALSE;
		gms_normal_string ("Glk extra prompts are now ");
		gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * extra prompt mode.
		 */
		gms_normal_string ("Glk extra prompts are ");
		if (gms_prompt_enabled)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk extra prompts can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/*
 * gms_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gms_command_commands (const char *argument)
{
	assert (argument != NULL);

	/* Set up command handling according to the argument given. */
	if (!gms_strcasecmp (argument, GMS_COMMAND_ON))
	    {
		/* Commands must already be on. */
		gms_normal_string ("Glk commands are already ");
		gms_normal_string (GMS_COMMAND_ON);
		gms_normal_string (".\n");
	    }
	else if (!gms_strcasecmp (argument, GMS_COMMAND_OFF))
	    {
		/* The user has turned commands off. */
		gms_commands_enabled = FALSE;
		gms_normal_string ("Glk commands are now ");
		gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else if (strlen (argument) == 0)
	    {
		/*
		 * There was no argument on the line, so print out the current
		 * command mode.
		 */
		gms_normal_string ("Glk commands are ");
		if (gms_commands_enabled)
			gms_normal_string (GMS_COMMAND_ON);
		else
			gms_normal_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
	else
	    {
		/*
		 * The command argument isn't a valid one, so print a list of
		 * valid command arguments.
		 */
		gms_normal_string ("Glk commands can be ");
		gms_standout_string (GMS_COMMAND_ON);
		gms_normal_string (", or ");
		gms_standout_string (GMS_COMMAND_OFF);
		gms_normal_string (".\n");
	    }
}


/* Escape introducer string, and undo for special recognition. */
static const char	*GMS_COMMAND_ESCAPE		= "glk";
static const char	*GMS_COMMAND_UNDO		= "undo";

/* Small table of Glk subcommands and handler functions. */
struct gms_command {
	const char	*command;		/* Glk subcommand. */
	void		(*handler) (const char *argument);
						/* Subcommand handler. */
	const int	takes_argument;		/* Argument flag. */
	const int	undo_return;		/* "Undo" return value. */
};
typedef const struct gms_command*	gms_commandref_t;
static const struct gms_command		GMS_COMMAND_TABLE[] = {
	{ "undo",		gms_command_undo,		FALSE,	TRUE  },
	{ "script",		gms_command_script,		TRUE,	FALSE },
	{ "inputlog",		gms_command_inputlog,		TRUE,	FALSE },
	{ "readlog",		gms_command_readlog,		TRUE,	FALSE },
	{ "abbreviations",	gms_command_abbreviations,	TRUE,	FALSE },
	{ "graphics",		gms_command_graphics,		TRUE,	FALSE },
	{ "gamma",		gms_command_gamma,		TRUE,	FALSE },
	{ "animations",		gms_command_animations,		TRUE,	FALSE },
	{ "prompts",		gms_command_prompts,		TRUE,	FALSE },
	{ "version",		gms_command_version,		FALSE,	FALSE },
	{ "commands",		gms_command_commands,		TRUE,	FALSE },
	{ NULL,			NULL,				FALSE,	FALSE }
};

/* List of whitespace command-argument separator characters. */
static const char	*GMS_COMMAND_WHITESPACE		= "\t ";


/*
 * gms_command_dispatch()
 *
 * Given a command string and an argument, this function finds and runs any
 * handler for it.
 *
 * It searches for the first unambiguous command to match the string passed
 * in.  The return is the count of command matches found; zero represents no
 * match (fail), one represents an unambiguous match (success, and handler
 * run), and more than one represents an ambiguous match (fail).
 *
 * On unambiguous returns, it will also set the value for undo_command to the
 * table undo return value.
 */
static int
gms_command_dispatch (const char *command, const char *argument,
			int *undo_command)
{
	gms_commandref_t	entry;		/* Table search entry. */
	gms_commandref_t	matched;	/* Matched table entry. */
	int			matches;	/* Count of command matches. */
	assert (command != NULL && argument != NULL);
	assert (undo_command != NULL);

	/*
	 * Search for the first unambiguous table command string matching
	 * the command passed in.
	 */
	matches = 0;
	matched = NULL;
	for (entry = GMS_COMMAND_TABLE;
				entry->command != NULL; entry++)
	    {
		if (!gms_strncasecmp (command,
					entry->command, strlen (command)))
		    {
			matches++;
			matched = entry;
		    }
	    }

	/* If the match was unambiguous, call the command handler. */
	if (matches == 1)
	    {
		gms_normal_char ('\n');
		(matched->handler) (argument);

		/* Issue a brief warning if an argument was ignored. */
		if (!matched->takes_argument && strlen (argument) > 0)
		    {
			gms_normal_string ("[The ");
			gms_standout_string (matched->command);
			gms_normal_string (" command ignores arguments.]\n");
		    }

		/* Set the undo command code. */
		*undo_command = matched->undo_return;
	    }

	/* Return the count of matching table entries. */
	return matches;
}


/*
 * gms_command_usage()
 *
 * On an empty, invalid, or ambiguous command, print out a list of valid
 * commands and perhaps some Glk status information.
 */
static void
gms_command_usage (const char *command, int is_ambiguous)
{
	gms_commandref_t	entry;	/* Command table iteration entry. */
	assert (command != NULL);

	/* Print a blank line separator. */
	gms_normal_char ('\n');

	/* If the command isn't empty, indicate ambiguous or invalid. */
	if (strlen (command) > 0)
	    {
		gms_normal_string ("The Glk command ");
		gms_standout_string (command);
		if (is_ambiguous)
			gms_normal_string (" is ambiguous.\n");
		else
			gms_normal_string (" is not valid.\n");
	    }

	/* Print out a list of valid commands. */
	gms_normal_string ("Glk commands are");
	for (entry = GMS_COMMAND_TABLE; entry->command != NULL; entry++)
	    {
		gms_commandref_t	next;	/* Next command table entry. */

		next = entry + 1;
		gms_normal_string (next->command != NULL ? " " : " and ");
		gms_standout_string (entry->command);
		gms_normal_string (next->command != NULL ? "," : ".\n");
	    }

	/* Write a note about abbreviating commands. */
	gms_normal_string ("Glk commands may be abbreviated, as long as");
	gms_normal_string (" the abbreviations are unambiguous.\n");

	/*
	 * If no command was given, call each command handler function with
	 * an empty argument to prompt each to report the current setting.
	 */
	if (strlen (command) == 0)
	    {
		gms_normal_char ('\n');
		for (entry = GMS_COMMAND_TABLE;
					entry->command != NULL; entry++)
			(entry->handler) ("");
	    }
}


/*
 * gms_skip_characters()
 *
 * Helper function for command escapes.  Skips over either whitespace or
 * non-whitespace in string, and returns the revised string pointer.
 */
static char *
gms_skip_characters (char *string, int skip_whitespace)
{
	char		*result;		/* Return string pointer. */
	assert (string != NULL);

	/* Skip over leading characters of the specified type. */
	for (result = string; *result != '\0'; result++)
	    {
		int	is_whitespace;		/* Whitespace flag. */

		/* Break if encountering a character not the required type. */
		is_whitespace =
			(strchr (GMS_COMMAND_WHITESPACE, *result) != NULL);
		if ((skip_whitespace && !is_whitespace)
				|| (!skip_whitespace && is_whitespace))
			break;
	    }

	/* Return the revised pointer. */
	return result;
}


/*
 * gms_command_undo_special()
 *
 * This function makes a special case of the input line containing the
 * single word "undo", treating it as if it is "glk undo".  This makes
 * life a bit more convenient for the player, since it's the same behavior
 * that most other IF systems have.  It returns TRUE if "undo" was found,
 * and sets undo_command, FALSE otherwise.
 */
static int
gms_command_undo_special (char *string, int *undo_command)
{
	char	*first, *trailing;	/* Start and end of the first word */
	char	*end;			/* Skipped spaces after first word */
	assert (string != NULL && undo_command != NULL);

	/*
	 * Find the first significant character in string, and the space
	 * or NUL after the first word.
	 */
	first    = gms_skip_characters (string, TRUE);
	trailing = gms_skip_characters (first, FALSE);

	/*
	 * Find the first character, or NUL, following the whitespace
	 * after the first word.
	 */
	end = gms_skip_characters (trailing, TRUE);

	/* See if string is the single word "undo". */
	if (strlen (end) == 0
		&& !gms_strncasecmp (first, GMS_COMMAND_UNDO,
						strlen (GMS_COMMAND_UNDO))
			&& first + strlen (GMS_COMMAND_UNDO) == trailing)
	    {
		/* Lone "Undo" word found. */
		*undo_command = TRUE;
		return TRUE;
	    }

	/* String is something other than "undo". */
	return FALSE;
}


/*
 * gms_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 * Undo_command is set if the command is recognized as a request to undo a
 * game move.
 */
static int
gms_command_escape (char *string, int *undo_command)
{
	char		*temporary;		/* Temporary string pointer */
	char		*string_copy;		/* Destroyable string copy. */
	char		*command;		/* Glk subcommand. */
	char		*argument;		/* Glk subcommand argument. */
	int		matches;		/* Dispatcher matches. */
	assert (string != NULL && undo_command != NULL);

	/*
	 * Special-case the single-word string "undo" to be equivalent
	 * to "glk undo", for player convenience.
	 */
	if (gms_command_undo_special (string, undo_command))
		return TRUE;

	/*
	 * Return FALSE if the string doesn't begin with the Glk command
	 * escape introducer.
	 */
	temporary = gms_skip_characters (string, TRUE);
	if (gms_strncasecmp (temporary, GMS_COMMAND_ESCAPE,
					strlen (GMS_COMMAND_ESCAPE)))
		return FALSE;

	/* Take a copy of the string, without any leading space. */
	string_copy = gms_malloc (strlen (temporary) + 1);
	strcpy (string_copy, temporary);

	/* Find the subcommand; the word after the introducer. */
	command = gms_skip_characters (string_copy
					+ strlen (GMS_COMMAND_ESCAPE), TRUE);

	/* Skip over command word, be sure it terminates with NUL. */
	temporary = gms_skip_characters (command, FALSE);
	if (*temporary != '\0')
	    {
		*temporary = '\0';
		temporary++;
	    }

	/* Now find any argument data for the command. */
	argument = gms_skip_characters (temporary, TRUE);

	/* Ensure that argument data also terminates with a NUL. */
	temporary = gms_skip_characters (argument, FALSE);
	*temporary = '\0';

	/*
	 * Try to handle the command and argument as a Glk subcommand.  If
	 * it doesn't run unambiguously, print command usage.
	 */
	matches = gms_command_dispatch (command, argument, undo_command);
	if (matches != 1)
	    {
		if (matches == 0)
			gms_command_usage (command, FALSE);
		else
			gms_command_usage (command, TRUE);
		*undo_command = FALSE;
	    }

	/* Done with the copy of the string. */
	free (string_copy);

	/* Return TRUE to indicate string contained a Glk command. */
	return TRUE;
}


/*---------------------------------------------------------------------*/
/*  Glk port input functions                                           */
/*---------------------------------------------------------------------*/

/* Quote used to suppress abbreviation expansion and local commands. */
static const char	GMS_QUOTED_INPUT		= '\'';

/* Definition of whitespace characters to skip over. */
static const char	*GMS_WHITESPACE			= "\t ";

/*
 * Input buffer allocated for reading input lines.  The buffer is filled
 * from either an input log, if one is currently being read, or from Glk
 * line input.  We also need an "undo" notification flag.
 */
enum {			GMS_INPUTBUFFER_LENGTH		= 256 };
static char		gms_input_buffer[GMS_INPUTBUFFER_LENGTH];
static int		gms_input_length		= 0;
static int		gms_input_cursor		= 0;
static int		gms_undo_notification		= FALSE;


/*
 * gms_char_is_whitespace()
 *
 * Check for ASCII whitespace characters.  Returns TRUE if the character
 * qualifies as whitespace (NUL is not whitespace).
 */
static int
gms_char_is_whitespace (char c)
{
	return (c != '\0' && strchr (GMS_WHITESPACE, c) != NULL);
}


/*
 * gms_skip_leading_whitespace()
 *
 * Skip over leading whitespace, returning the address of the first non-
 * whitespace character.
 */
static char *
gms_skip_leading_whitespace (char *string)
{
	char	*result;			/* Return string pointer. */
	assert (string != NULL);

	/* Move result over leading whitespace. */
	for (result = string; gms_char_is_whitespace (*result); )
		result++;
	return result;
}


/* Table of single-character command abbreviations. */
struct gms_abbreviation {
	const char	abbreviation;		/* Abbreviation character. */
	const char	*expansion;		/* Expansion string. */
};
typedef const struct gms_abbreviation*		gms_abbreviationref_t;
static const struct gms_abbreviation		GMS_ABBREVIATIONS[] = {
	{ 'c',	"close"   },	{ 'g',	"again" },	{ 'i',	"inventory" },
	{ 'k',	"attack"  },	{ 'l',	"look"  },	{ 'p',	"open"      },
	{ 'q',	"quit"    },	{ 'r',	"drop"  },	{ 't',	"take"      },
	{ 'x',	"examine" },	{ 'y',	"yes"   },	{ 'z',	"wait"      },
	{ '\0',	NULL }
};

/*
 * gms_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems, but not always normal in Magnetic Scrolls games.
 */
static void
gms_expand_abbreviations (char *buffer, int size)
{
	char		*command;	/* Single-character command. */
	gms_abbreviationref_t
			entry, match;	/* Table search entry, and match. */
	assert (buffer != NULL);

	/* Skip leading spaces to find command, and return if nothing. */
	command = gms_skip_leading_whitespace (buffer);
	if (strlen (command) == 0)
		return;

	/* If the command is not a single letter one, do nothing. */
	if (strlen (command) > 1
			&& !gms_char_is_whitespace (command[1]))
		return;

	/* Scan the abbreviations table for a match, return if none. */
	match = NULL;
	for (entry = GMS_ABBREVIATIONS; entry->expansion != NULL; entry++)
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
	gms_standout_string ("[");
	gms_standout_char   (match->abbreviation);
	gms_standout_string (" -> ");
	gms_standout_string (match->expansion);
	gms_standout_string ("]\n");
#endif
}


/*
 * gms_buffer_input
 *
 * Read and buffer a line of input.  If there is an input log active, then
 * data is taken by reading this first.  Otherwise, the function gets a
 * line from Glk.
 *
 * It also makes special cases of some lines read from the user, either
 * handling commands inside them directly, or expanding abbreviations as
 * appropriate.  This is not reflected in the buffer, which is adjusted as
 * required before returning.
 */
static void
gms_buffer_input (void)
{
	event_t		event;			/* Glk event buffer. */

	/*
	 * Update the current status line display, and flush any pending
	 * buffered output.
	 */
	gms_status_notify ();
	gms_output_flush ();

	/*
	 * Magnetic Windows games tend not to issue a prompt after reading
	 * an empty line of input.  This can make for a very blank looking
	 * screen.
	 *
	 * To slightly improve things, if it looks like we didn't get a
	 * prompt from the game, do our own.
	 */
	if (gms_prompt_enabled
			&& !gms_game_prompted ())
	    {
		gms_normal_char ('\n');
		gms_normal_string (GMS_INPUT_PROMPT);
	    }

	/*
	 * If we have an input log to read from, use that until it is
	 * exhausted.  On end of file, close the stream and resume input
	 * from line requests.
	 */
	if (gms_readlog_stream != NULL)
	    {
		glui32		chars;		/* Characters read. */

		/* Get the next line from the log stream. */
		chars = glk_get_line_stream
				(gms_readlog_stream, gms_input_buffer,
						sizeof (gms_input_buffer));
		if (chars > 0)
		    {
			/* Echo the line just read in input style. */
			glk_set_style (style_Input);
			glk_put_buffer (gms_input_buffer, chars);
			glk_set_style (style_Normal);

			/* Note how many characters buffered, and return. */
			gms_input_length = chars;
			return;
		    }

		/*
		 * We're at the end of the log stream.  Close it, and then
		 * continue on to request a line from Glk.
		 */
		glk_stream_close (gms_readlog_stream, NULL);
		gms_readlog_stream = NULL;
	    }

	/*
	 * No input log being read, or we just hit the end of file on one.
	 * Revert to normal line input; start by getting a new line from Glk.
	 */
	glk_request_line_event (gms_main_window, gms_input_buffer,
					sizeof (gms_input_buffer) - 1, 0);
	gms_event_wait (evtype_LineInput, &event);

	/* Terminate the input line with a NUL. */
	assert (event.val1 <= sizeof (gms_input_buffer) - 1);
	gms_input_buffer[ event.val1 ] = '\0';

	/*
	 * If neither abbreviations nor local commands are enabled, use the
	 * data read above without further massaging.
	 */
	if (gms_abbreviations_enabled
				|| gms_commands_enabled)
	    {
		char		*command;	/* Command part of buffer. */

		/* Find the first non-space character in the input buffer. */
		command = gms_skip_leading_whitespace (gms_input_buffer);

		/*
		 * If the first non-space input character is a quote, bypass
		 * all abbreviation expansion and local command recognition,
		 * and use the unadulterated input, less introductory quote.
		 */
		if (command[0] == GMS_QUOTED_INPUT)
		    {
			/* Delete the quote with memmove(). */
			memmove (command, command + 1, strlen (command));
		    }
		else
		    {
			/* Check for, and expand, any abbreviated commands. */
			if (gms_abbreviations_enabled)
				gms_expand_abbreviations
						(gms_input_buffer,
						sizeof (gms_input_buffer));

			/*
			 * Check for Glk port special commands, and if found
			 * then suppress the interpreter's use of this input.
			 */
			if (gms_commands_enabled
				&& gms_command_escape (gms_input_buffer,
						&gms_undo_notification))
			    {
				/*
				 * Overwrite the buffer with an empty line if
				 * we saw a command.
				 */
				gms_input_buffer[0] = '\n';
				gms_input_length    = 1;

				/* Return the overwritten input line. */
				return;
			    }
		    }
	    }

	/*
	 * If there is an input log active, log this input string to it.
	 * Note that by logging here we get any abbreviation expansions but
	 * we won't log glk special commands, nor any input read from a
	 * current open input log.
	 */
	if (gms_inputlog_stream != NULL)
	    {
		glk_put_string_stream (gms_inputlog_stream, gms_input_buffer);
		glk_put_char_stream (gms_inputlog_stream, '\n');
	    }

	/*
	 * Now append a newline to the buffer, since Glk line input doesn't
	 * provide one, and in any case, abbreviation expansion may have
	 * edited the buffer contents (and in particular, changed the length).
	 */
	gms_input_buffer[ strlen (gms_input_buffer) + 1 ] = '\0';
	gms_input_buffer[ strlen (gms_input_buffer) ]     = '\n';

	/* Note how many characters are buffered after all of the above. */
	gms_input_length = strlen (gms_input_buffer);

	gms_kill_newline = 1;
}


/*
 * ms_getchar()
 *
 * Return the single next character to the interpreter.  This function
 * extracts characters from the input buffer until empty, when it then
 * tries to buffer more data.
 */
type8
ms_getchar (type8 trans)
{
	/* See if we are at the end of the input buffer. */
	if (gms_input_cursor == gms_input_length)
	    {
		/*
		 * Try to read in more data, and rewind buffer cursor.  As
		 * well as reading input, this may set an undo notification.
		 */
		gms_buffer_input ();
		gms_input_cursor = 0;

		/* See if there is any undo notification pending. */
		if (gms_undo_notification)
		    {
			/*
			 * Clear the undo notification, and discard buffered
			 * input (typically just the '\n' placed there when
			 * the undo command was recognized).
			 */
			gms_undo_notification = FALSE;
			gms_input_length = 0;

			/*
			 * Return the special 0, or a blank line if no undo
			 * is allowed at this point.
			 */
			return trans ? 0 : '\n';
		    }
	    }

	/* Return the next character from the input buffer. */
	assert (gms_input_cursor < gms_input_length);
	return gms_input_buffer[ gms_input_cursor++ ];
}


/*
 * gms_confirm()
 *
 * Print a confirmation prompt, and read a single input character, taking
 * only [YyNn] input.  If the character is 'Y' or 'y', return TRUE.
 */
static int
gms_confirm (const char *prompt)
{
	event_t		event;			/* Glk event buffer. */
	unsigned char	response;		/* Response character. */
	assert (prompt != NULL);

	/*
	 * Print the confirmation prompt, in a style that hints that it's
	 * from the interpreter, not the game.
	 */
	gms_standout_string (prompt);

	/* Wait for a single 'Y' or 'N' character response. */
	do
	    {
		/* Wait for a standard key, ignoring Glk special keys. */
		do
		    {
			glk_request_char_event (gms_main_window);
			gms_event_wait (evtype_CharInput, &event);
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
 * gms_event_wait()
 *
 * Process Glk events until one of the expected type arrives.  Return
 * the event of that type.
 */
static void
gms_event_wait (glui32 wait_type, event_t *event)
{
	assert (event != NULL);

	/* Get events, until one matches the requested type. */
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
			gms_status_redraw ();
			gms_hint_redraw ();
			gms_graphics_paint ();
			break;

		    case evtype_Timer:
			/* Do background graphics updates on timeout. */
			gms_graphics_timeout ();
			break;
		    }
	    }
	while (event->type != wait_type);
}


/*---------------------------------------------------------------------*/
/*  Glk port file functions                                            */
/*---------------------------------------------------------------------*/

/* Success and fail return codes from file functions. */
static const type8	GMS_FILE_SUCCESS		= 0;
static const type8	GMS_FILE_ERROR			= 1;


/*
 * ms_save_file ()
 * ms_load_file ()
 *
 * Save the current game state to a file, and load a game state.
 */
type8
ms_save_file (type8s *name, type8 *ptr, type16 size)
{
	assert (ptr != NULL);

	/* Flush any pending buffered output. */
	gms_output_flush ();

	/* If there is no name, use Glk to prompt for one, and save. */
	if (name == NULL)
	    {
		frefid_t	fileref;	/* Glk file reference. */
		strid_t		stream;		/* Glk stream reference. */

		/* Get a Glk file reference for a game save file. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_SavedGame, filemode_Write, 0);
		if (fileref == NULL)
			return GMS_FILE_ERROR;

		/* Open a Glk stream for the fileref. */
		stream = glk_stream_open_file (fileref, filemode_Write, 0);
		if (stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			return GMS_FILE_ERROR;
		    }

		/* Write the game state data. */
		glk_put_buffer_stream (stream, ptr, size);

		/* Close and destroy the Glk stream and fileref. */
		glk_stream_close (stream, NULL);
		glk_fileref_destroy (fileref);
	    }
	else
	    {
		FILE		*stream;	/* Stdio file reference. */

		/*
		 * If openable for read, assume the file already exists
		 * and confirm overwrite.
		 */
		stream = fopen (name, "r");
		if (stream != NULL)
		    {
			fclose (stream);

			/*
			 * Confirm overwrite, clearing the game prompted
			 * flag beforehand so we're sure to issue a new
			 * prompt on the next line input.
			 */
			gms_game_prompted ();
			if (!gms_confirm ("Overwrite existing file? [y/n] "))
				return GMS_FILE_ERROR;
		    }

		/*
		 * Open the output file directly.  There's no Glk way to
		 * achieve this.
		 */
		stream = fopen (name, "wb");
		if (stream == NULL)
			return GMS_FILE_ERROR;

		/* Write saved game data. */
		if (fwrite (ptr, 1, size, stream) != size)
		    {
			fclose (stream);
			return GMS_FILE_ERROR;
		    }

		/* Close the stdio stream. */
		fclose (stream);
	    }

	/* All done. */
	return GMS_FILE_SUCCESS;
}

type8
ms_load_file (type8s *name, type8 *ptr, type16 size)
{
	assert (ptr != NULL);

	/* Flush any pending buffered output. */
	gms_output_flush ();

	/* If there is no name, use Glk to prompt for one, and load. */
	if (name == NULL)
	    {
		frefid_t	fileref;	/* Glk file reference. */
		strid_t		stream;		/* Glk stream reference. */

		/* Get a Glk file reference for a game save file. */
		fileref = glk_fileref_create_by_prompt
				(fileusage_SavedGame, filemode_Read, 0);
		if (fileref == NULL)
			return GMS_FILE_ERROR;

		/*
		 * Reject the file reference if we're expecting to read
		 * from it, and the referenced file doesn't exist.
		 */
		if (!glk_fileref_does_file_exist (fileref))
		    {
			glk_fileref_destroy (fileref);
			return GMS_FILE_ERROR;
		    }

		/* Open a Glk stream for the fileref. */
		stream = glk_stream_open_file (fileref, filemode_Read, 0);
		if (stream == NULL)
		    {
			glk_fileref_destroy (fileref);
			return GMS_FILE_ERROR;
		    }

		/* Read back the game state data. */
		glk_get_buffer_stream (stream, ptr, size);

		/* Close and destroy the Glk stream and fileref. */
		glk_stream_close (stream, NULL);
		glk_fileref_destroy (fileref);
	    }
	else
	    {
		FILE		*stream;	/* Stdio file reference. */

		/*
		 * Open the input file directly.  As above, there's no
		 * Glk way to achieve this.
		 */
		stream = fopen (name, "rb");
		if (stream == NULL)
			return GMS_FILE_ERROR;

		/* Read saved game data. */
		if (fread (ptr, 1, size, stream) != size)
		    {
			fclose (stream);
			return GMS_FILE_ERROR;
		    }

		/* Close the stdio stream. */
		fclose (stream);
	    }

	/* All done. */
	return GMS_FILE_SUCCESS;
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
 * The following values need to be passed between the startup_code and main
 * functions.
 */
static char	*gms_gamefile		= NULL;		/* Name of game file. */
static char	*gms_game_message	= NULL;		/* Error message. */


/*
 * gms_nuance_filenames()
 *
 * Given a game name, try to nuance three filenames from it - the main game
 * text file, the (optional) graphics data file, and the (optional) hints
 * file.  Given an input "file" X, the function looks for X.MAG or X.mag for
 * game data, X.GFX or X.gfx for graphics, and X.HNT or X.hnt for hints.
 * If the input file already ends with .MAG, .GFX, or .HNT, the extension
 * is stripped first.
 *
 * The function returns NULL for filenames not available.  It's not fatal
 * if the graphics filename or hints filename is NULL, but it is if the main
 * game filename is NULL.  Filenames are malloc'ed, and need to be freed by
 * the caller.
 *
 * The function uses fopen() rather than access() since fopen() is an ANSI
 * standard function, and access() isn't.
 */
static void
gms_nuance_filenames (char *name, char **text, char **graphics, char **hints)
{
	char		*base;			/* Copy of the base string. */
	char		*text_file;		/* Game text file path. */
	char		*graphics_file;		/* Game graphics file path. */
	char		*hints_file;		/* Game hints file path. */
	FILE		*stream;		/* Test file stream. */
	assert (name != NULL && text != NULL
			&& graphics != NULL && hints != NULL);

	/* First, take a destroyable copy of the input filename. */
	base = gms_malloc (strlen (name) + 1);
	strcpy (base, name);

	/* Now, if base has an extension .MAG, .GFX, or .HNT, remove it. */
	if (strlen (base) > strlen (".XXX"))
	    {
		if (!gms_strcasecmp (base + strlen (base)
						- strlen (".MAG"), ".MAG")
				|| !gms_strcasecmp (base + strlen (base)
						- strlen (".GFX"), ".GFX")
				|| !gms_strcasecmp (base + strlen (base)
						- strlen (".HNT"), ".HNT"))
			base[ strlen (base) - strlen (".XXX") ] = '\0';
	    }

	/* Malloc space for the return text file. */
	text_file = gms_malloc (strlen (base) + strlen (".MAG") + 1);

	/* Form a candidate text file, by adding a .MAG extension. */
	strcpy (text_file, base);
	strcat (text_file, ".MAG");
	stream = fopen (text_file, "rb");
	if (stream == NULL)
	    {
		/* Retry, using a .mag extension instead. */
		strcpy (text_file, base);
		strcat (text_file, ".mag");
		stream = fopen (text_file, "rb");
		if (stream == NULL)
		    {
			/*
			 * No access to a usable game text file.  Return
			 * immediately, without bothering to look for any
			 * associated graphics or hints files.
			 */
			*text		= NULL;
			*graphics	= NULL;
			*hints		= NULL;

			/* Free malloced memory and return. */
			free (text_file);
			free (base);
			return;
		    }
	    }
	if (stream != NULL)
		fclose (stream);

	/* Now malloc space for the return graphics file. */
	graphics_file = gms_malloc (strlen (base) + strlen (".GFX") + 1);

	/* As above, form a candidate graphics file, using a .GFX extension. */
	strcpy (graphics_file, base);
	strcat (graphics_file, ".GFX");
	stream = fopen (graphics_file, "rb");
	if (stream == NULL)
	    {
		/* Retry, using a .gfx extension instead. */
		strcpy (graphics_file, base);
		strcat (graphics_file, ".gfx");
		stream = fopen (graphics_file, "rb");
		if (stream == NULL)
		    {
			/*
			 * No access to any graphics file.  In this case,
			 * free memory and reset graphics file to NULL.
			 */
			free (graphics_file);
			graphics_file = NULL;
		    }
	    }
	if (stream != NULL)
		fclose (stream);

	/* Now malloc space for the return hints file. */
	hints_file = gms_malloc (strlen (base) + strlen (".HNT") + 1);

	/* As above, form a candidate graphics file, using a .HNT extension. */
	strcpy (hints_file, base);
	strcat (hints_file, ".HNT");
	stream = fopen (hints_file, "rb");
	if (stream == NULL)
	    {
		/* Retry, using a .hnt extension instead. */
		strcpy (hints_file, base);
		strcat (hints_file, ".hnt");
		stream = fopen (hints_file, "rb");
		if (stream == NULL)
		    {
			/*
			 * No access to any hints file.  In this case,
			 * free memory and reset hints file to NULL.
			 */
			free (hints_file);
			hints_file = NULL;
		    }
	    }
	if (stream != NULL)
		fclose (stream);

	/* Return the text file, and graphics and hints, which may be NULL. */
	*text		= text_file;
	*graphics	= graphics_file;
	*hints		= hints_file;

	/* Finished with base. */
	free (base);
}


/*
 * gms_startup_code()
 * gms_main()
 *
 * Together, these functions take the place of the original main().  The
 * first one is called from glkunix_startup_code(), to parse and generally
 * handle options.  The second is called from glk_main(), and does the real
 * work of running the game.
 */
static int
gms_startup_code (int argc, char *argv[])
{
	int		argv_index;			/* Argument iterator. */

	/* Handle command line arguments. */
	for (argv_index = 1;
		argv_index < argc && argv[ argv_index ][0] == '-'; argv_index++)
	    {
		if (strcmp (argv[ argv_index ], "-nc") == 0)
		    {
			gms_commands_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-na") == 0)
		    {
			gms_abbreviations_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-np") == 0)
		    {
			gms_graphics_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-ng") == 0)
		    {
			gms_gamma_mode = GAMMA_OFF;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-nx") == 0)
		    {
			gms_animation_enabled = FALSE;
			continue;
		    }
		if (strcmp (argv[ argv_index ], "-ne") == 0)
		    {
			gms_prompt_enabled = FALSE;
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
		gms_gamefile = argv[ argv_index ];
		gms_game_message = NULL;
#ifdef GARGLK
		{
			char *s;
			s = strrchr(gms_gamefile, '\\');
			if (s) garglk_set_story_name(s+1);
			s = strrchr(gms_gamefile, '/');
			if (s) garglk_set_story_name(s+1);
		}
#endif
	    }
	else
	    {
		gms_gamefile = NULL;
		if (argv_index < argc - 1)
			gms_game_message = "More than one game file"
					" was given on the command line.";
		else
			gms_game_message = "No game file was given"
					" on the command line.";
	    }

	/* All startup options were handled successfully. */
	return TRUE;
}

static void
gms_main (void)
{
	char		*text_file	= NULL;	/* Text file path */
	char		*graphics_file	= NULL;	/* Graphics file path */
	char		*hints_file	= NULL;	/* Hints file path */
	int		ms_init_status;		/* ms_init status code */

	/* Verify CRC tables build correctly, with a standard test. */
	assert (gms_buffer_crc ("123456789", 9) == 0xCBF43926);

	/* Ensure Magnetic Scrolls types have the right sizes. */
	if (sizeof (type8) != 1 || sizeof (type8s) != 1
			|| sizeof (type16) != 2 || sizeof (type16s) != 2
			|| sizeof (type32) != 4 || sizeof (type32s) != 4)
	    {
		gms_fatal ("GLK: Types sized incorrectly,"
					" recompilation is needed");
		glk_exit ();
	    }

	/* Create the main Glk window, and set its stream as current. */
	gms_main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
	if (gms_main_window == NULL)
	    {
		gms_fatal ("GLK: Can't open main window");
		glk_exit ();
	    }
	glk_window_clear (gms_main_window);
	glk_set_window (gms_main_window);
	glk_set_style (style_Normal);

	/* If there's a problem with the game file, complain now. */
	if (gms_gamefile == NULL)
	    {
		assert (gms_game_message != NULL);
		gms_header_string ("Glk Magnetic Error\n\n");
		gms_normal_string (gms_game_message);
		gms_normal_char ('\n');
		glk_exit ();
	    }

	/*
	 * Given the basic game name, try to come up with usable text,
	 * graphics, and hints filenames.  The graphics and hints files may
	 * be null, but the text file may not.
	 */
	errno = 0;
	gms_nuance_filenames (gms_gamefile,
				&text_file, &graphics_file, &hints_file);
	if (text_file == NULL)
	    {
		/* Report the error, including any details in errno. */
		assert (graphics_file == NULL && hints_file == NULL);
		gms_header_string ("Glk Magnetic Error\n\n");
		gms_normal_string ("Can't find or open game '");
		gms_normal_string (gms_gamefile);
		gms_normal_string ("[.mag|.MAG]'");
		if (errno != 0)
		    {
			gms_normal_string (": ");
			gms_normal_string (strerror (errno));
		    }
		gms_normal_char ('\n');

		/* Nothing more to be done. */
		glk_exit ();
	    }

	/* Set the possibility of pictures depending on graphics file. */
	if (graphics_file != NULL)
	    {
		/*
		 * Check Glk library capabilities, and note pictures are
		 * impossible if the library can't offer both graphics and
		 * timers.  We need timers to create the background
		 * "thread" for picture updates.
		 */
		if (glk_gestalt (gestalt_Graphics, 0)
					&& glk_gestalt (gestalt_Timer, 0))
			gms_graphics_possible = TRUE;
		else
			gms_graphics_possible = FALSE;
	    }
	else
		gms_graphics_possible = FALSE;


	/*
	 * If pictures are impossible, clear pictures enabled flag.  That
	 * is, act as if -np was given on the command line, even though
	 * it may not have been.  If pictures are impossible, they can
	 * never be enabled.
	 */
	if (!gms_graphics_possible)
		gms_graphics_enabled = FALSE;

	/* Try to create a one-line status window.  We can live without it. */
	gms_status_window = glk_window_open (gms_main_window,
					winmethod_Above|winmethod_Fixed,
					0, wintype_TextGrid, 0);

	/* Seed the random number generator. */
	ms_seed (time (NULL));

	/*
	 * Load the game.  If no graphics are possible, then passing the
	 * NULL to ms_init() runs a game without graphics.
	 */
	errno = 0;
	if (gms_graphics_possible)
	    {
		/* Initialize a game with graphics, and maybe hints. */
		assert (graphics_file != NULL);
		ms_init_status = ms_init (text_file, graphics_file, hints_file);
	    }
	else
	    {
		/* Initialize a game without graphics, and maybe hints. */
		ms_init_status = ms_init (text_file, NULL, hints_file);
	    }

	/* Look for a complete failure to load the game. */
	if (ms_init_status == 0)
	    {
		/* Report the error, including any details in errno. */
		if (gms_status_window != NULL)
			glk_window_close (gms_status_window, NULL);
		gms_header_string ("Glk Magnetic Error\n\n");
		gms_normal_string ("Can't load game '");
		gms_normal_string (gms_gamefile);
		gms_normal_char ('\'');
		if (errno != 0)
		    {
			gms_normal_string (": ");
			gms_normal_string (strerror (errno));
		    }
		gms_normal_char ('\n');

		/*
		 * Free the text file path, any graphics/hints file path,
		 * and interpreter allocated memory.
		 */
		free (text_file);
		if (graphics_file != NULL)
			free (graphics_file);
		if (hints_file != NULL)
			free (hints_file);
		ms_freemem ();

		/* Nothing more to be done. */
		glk_exit ();
	    }

	/* Print out a short banner. */
	gms_header_string ("\nMagnetic Scrolls Interpreter, version 2.2\n");
	gms_banner_string ("Written by Niclas Karlsson\n"
					"Glk interface by Simon Baldwin\n\n");

	/* Look for failure to load just game graphics. */
	if (gms_graphics_possible
			&& ms_init_status == 1)
	    {
		/*
		 * Output a warning if graphics failed, but the main game
		 * text initialized okay.
		 */
		gms_standout_string ("Error: Unable to open graphics file\n");
		gms_standout_string ("Continuing without pictures...\n\n");

		/* Reset pictures possible flag. */
		gms_graphics_possible = FALSE;
	    }

	/* Run the game opcodes. */
	while (TRUE)
	    {
		/* Execute an opcode - returns FALSE if game stops. */
		if (!ms_rungame ())
			break;
		glk_tick ();
	    }

	/* Handle any updated status and pending buffered output. */
	gms_status_notify ();
	gms_output_flush ();

	/* Turn off any background graphics "thread". */
	gms_graphics_stop ();

	/* Free interpreter allocated memory. */
	ms_freemem ();

	/*
	 * Free any temporary memory that may have been used by graphics
	 * and hints.
	 */
	gms_graphics_cleanup ();
	gms_hints_cleanup ();

	/* Close any open transcript, input log, and/or read log. */
	if (gms_transcript_stream != NULL)
		glk_stream_close (gms_transcript_stream, NULL);
	if (gms_inputlog_stream != NULL)
		glk_stream_close (gms_inputlog_stream, NULL);
	if (gms_readlog_stream != NULL)
		glk_stream_close (gms_readlog_stream, NULL);

	/* Free the text file path, and any graphics/hints file path. */
	free (text_file);
	if (graphics_file != NULL)
		free (graphics_file);
	if (hints_file != NULL)
		free (hints_file);
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the real interpreter      */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int		gms_startup_called	= FALSE;
static int		gms_main_called		= FALSE;

#if TARGET_OS_MAC
/* Additional Mac variables. */
static strid_t		gms_mac_gamefile	= NULL;
static short		gms_savedVRefNum	= 0;
static long		gms_savedDirID		= 0;


/*
 * gms_mac_whenselected()
 * gms_mac_whenbuiltin()
 * macglk_startup_code()
 *
 * Startup entry points for Mac versions of Glk interpreter.  Glk will
 * call macglk_startup_code() for details on what to do when the app-
 * lication is selected.  On selection, an argv[] vector is built, and
 * passed to the normal interpreter startup code, after which, Glk will
 * call glk_main().
 */
static Boolean
gms_mac_whenselected (FSSpec *file, OSType filetype)
{
	static char*		argv[2];
	assert (!gms_startup_called);
	gms_startup_called = TRUE;

	/* Set the WD to where the file is, so later fopens work. */
	if (HGetVol (0, &gms_savedVRefNum, &gms_savedDirID) != 0)
	    {
		gms_fatal ("GLK: HGetVol failed");
		return FALSE;
	    }
	if (HSetVol (0, file->vRefNum, file->parID) != 0);
	    {
		gms_fatal ("GLK: HSetVol failed");
		return FALSE;
	    }

	/* Put a CString version of the PString name into argv[1]. */
	argv[1] = gms_malloc (file->name[0] + 1);
	BlockMoveData (file->name + 1, argv[1], file->name[0]);
	argv[1][file->name[0]] = '\0';
	argv[2] = NULL;

	return gms_startup_code (2, argv);
}

static Boolean
gms_mac_whenbuiltin (void)
{
	/* Not implemented yet. */
	return TRUE;
}

Boolean
macglk_startup_code (macglk_startup_t *data)
{
	static OSType		gms_mac_gamefile_types[] = { 'MaSc' };

	data->startup_model	 = macglk_model_ChooseOrBuiltIn;
	data->app_creator	 = 'cAGL';
	data->gamefile_types	 = gms_mac_gamefile_types;
	data->num_gamefile_types = sizeof (gms_mac_gamefile_types)
					/ sizeof (*gms_mac_gamefile_types);
	data->savefile_type	 = 'BINA';
	data->datafile_type	 = 0x3F3F3F3F;
	data->gamefile		 = &gms_mac_gamefile;
	data->when_selected	 = gms_mac_whenselected;
	data->when_builtin	 = gms_mac_whenbuiltin;
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
	assert (!gms_startup_called);
	gms_startup_called = TRUE;

#ifdef GARGLK
	garglk_set_program_name("Magnetic 2.2");
	garglk_set_program_info(
		"Magnetic 2.2 by Niclas Karlsson, David Kinder,\n"
		"Stefan Meier, and Paul David Doherty.\n"
		"Glk port by Simon Baldwin.\n"
		"Gargoyle tweaks by Tor Andersson.\n");
#endif

	return gms_startup_code (data->argc, data->argv);
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
	assert (gms_startup_called && !gms_main_called);
	gms_main_called = TRUE;

	/* Call the interpreter main function. */
	gms_main ();
}

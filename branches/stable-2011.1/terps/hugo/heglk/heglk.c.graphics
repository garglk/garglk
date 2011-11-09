/*
	HEGLK.C

	Glk interface functions for the Hugo Engine

	Copyright (c) 1999-2004 by Kent Tessman
	
	Graphics support added by Simon Baldwin
*/


#include "heheader.h"
#include "glk.h"
#include "gi_blorb.h"


int hugo_color(int c);
int hugo_displaypicture(HUGO_FILE infile, long reslength);


/* Glk main window: */
static winid_t mainwin = NULL;

/* Usually the statusline window: */
static winid_t secondwin = NULL;

/* In case we try to print positioned text to the main window,
   as in a menu: */
static winid_t auxwin = NULL;

/* A virtual window that points to one of the three above: */
static winid_t currentwin = NULL;

#define CHARWIDTH 1
int in_valid_window = false;
int glk_fcolor = DEF_FCOLOR, glk_bgcolor = DEF_BGCOLOR;
int mainwin_bgcolor;
int glk_current_font;
char just_cleared_screen = false;
char just_displayed_something = false;

#define STAT_UNAVAILABLE (-1)


/* glk_main

	The entry point for Glk:  starts up the Hugo Engine.
	The arguments glk_argc and glk_argv must be made up before
	glk_main() is called.
*/
void _glk_handler(void)
{
	hugo_displaypicture(NULL, 0);
}
void glk_main(void)
{
	/* Open the main window... */
	mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);

	/* ...and set it up for default output */
	glk_set_window(currentwin = mainwin);

	/* It's possible that the main window failed to open. There's
	   nothing we can do without it, so exit */
	if (!mainwin)
		return; 

	glk_set_interrupt_handler(_glk_handler);
	he_main(0, NULL);	/* no argc, argv */
	glk_set_interrupt_handler(NULL);

	/* Clear up any resources used by picture functions. */
	hugo_displaypicture(NULL, 0);
}


/*
 * MEMORY ALLOCATION:
 *
 */

void *hugo_blockalloc(long num)
{
	return malloc(num * sizeof(char));
}

void hugo_blockfree(void *block)
{
	free(block);                           /* i.e. free() */
}


/*
 * FILE MANAGEMENT:
 *
 */

/* Glk Hugo doesn't need these; they're handled by the Glk library:
void hugo_splitpath(char *path, char *drive, char *dir, char *fname, char *ext)
{}

void hugo_makepath(char *path, char *drive, char *dir, char *fname, char *ext)
{}

void hugo_getfilename(char *a, char *b)
{}

int hugo_overwrite(char *f)
{}
*/

void hugo_closefiles()
{
	/* Glk closes all files at glk_exit() */
}


/*
 * KEYBOARD INPUT:
 *
 */

/* hugo_getkey */

int hugo_getkey(void)
{
	/* Not needed here--single-character events are handled
	   solely by hugo_waitforkey(), below */

	return 0;
}


/* hugo_getline

	Gets a line of input from the keyboard, storing it in <buffer>.
*/

void hugo_getline(char *prmpt)
{
	event_t ev;
	char gotline = 0;

	/* Just in case we try to get line input from a Glk-illegal
	   window that hasn't been created, switch as a failsafe
	   to mainwin
	*/
	if (currentwin==NULL)
		glk_set_window(currentwin = mainwin);

	/* Print prompt */
	glk_put_string(prmpt);

	/* Request line input */
	glk_request_line_event(currentwin, buffer, MAXBUFFER, 0);

        while (!gotline)
	{
		/* Grab an event */
		glk_select(&ev);

		switch (ev.type)
		{
 			case evtype_LineInput:
				/* (Will always be currentwin, but anyway) */
				if (ev.win==currentwin)
				{
					gotline = true;
				}
				break;
		}
        }
        
	/* The line we have received in commandbuf is not null-terminated */
	buffer[ev.val1] = '\0';	/* i.e., the length */

	/* Copy the input to the script file (if open) */
	if (script)
	{
		/* This looks inefficient, but it's necessary because
		   of the way Glk functions are mapped by stdio
		*/
		fprintf(script, "%s", prmpt);
		fprintf(script, "%s", buffer);
		fprintf(script, "%s", "\n");
	}
}


/* hugo_waitforkey

	Provided to be replaced by multitasking systems where cycling while
	waiting for a keystroke may not be such a hot idea.
*/

int hugo_waitforkey(void)
{
	event_t ev;
	char gotchar = 0;

	/* Ignore calls where there's been no recent display. */
	if (!just_displayed_something)
		return 0;
	just_displayed_something = false;

	/* Just in case we try to get key input from a Glk-illegal
	   window that hasn't been created, switch as a failsafe
	   to mainwin
	*/
	if (currentwin==NULL)
		glk_set_window(currentwin = mainwin);

#if defined (NO_KEYPRESS_CURSOR)
	if (currentwin!=mainwin)
	{
		glk_window_move_cursor(currentwin, currentpos/CHARWIDTH, currentline-1);
		hugo_print("*");
		glk_window_move_cursor(currentwin, currentpos/CHARWIDTH, currentline-1);
	}
#endif
	
	glk_request_char_event(currentwin);

        while (!gotchar)
	{
		/* Grab an event */
		glk_select(&ev);
            
		switch (ev.type)
		{
 			case evtype_CharInput:
				/* (Will always be mainwin, but anyway) */
				if (ev.win==currentwin)
				{
					gotchar = true;
				}
				break;
		}
        }

	/* Convert Glk special keycodes: */
	switch (ev.val1)
	{
		case keycode_Left:	ev.val1 = 8;	break;
		case keycode_Right:	ev.val1 = 21;	break;
		case keycode_Up:	ev.val1 = 11;	break;
		case keycode_Down:	ev.val1 = 10;	break;
		case keycode_Return:	ev.val1 = 13;	break;
		case keycode_Escape:	ev.val1 = 27;	break;
	}

#if defined (NO_KEYPRESS_CURSOR)
	if (currentwin!=mainwin)
	{
		glk_window_move_cursor(currentwin, currentpos/CHARWIDTH, currentline-1);
		hugo_print(" ");
		glk_window_move_cursor(currentwin, currentpos/CHARWIDTH, currentline-1);
	}
#endif

	return ev.val1;
}


/* hugo_iskeywaiting

	Returns true if a keypress is waiting to be retrieved.
*/

int hugo_iskeywaiting(void)
{
	var[system_status] = STAT_UNAVAILABLE;
	return 0;
}


/* hugo_timewait

    Waits for 1/n seconds.  Returns false if waiting is unsupported.
*/

int hugo_timewait(int n)
{
	glui32 millisecs;
	event_t ev;
    
	if (!glk_gestalt(gestalt_Timer, 0))
		return false;
	if (n==0) return true;

        
	millisecs = 1000 / n;
	if (millisecs == 0)
		millisecs = 1;

	// For the time being, we're going to disallow
	// millisecond delays in Glk (1) because there's no
	// point, and (2) so that we can tell we're running
	// under Glk.
	if (millisecs < 1000) return false;

	glk_request_timer_events(millisecs);
	while (1)
	{
		glk_select(&ev);
		if (ev.type == evtype_Timer)
			break;
	}
	glk_request_timer_events(0);
	return true;
}


/*
 * DISPLAY CONTROL:
 *
 */

void hugo_init_screen(void)
{
/* Does whatever has to be done to initially set up the display. */

	/* By setting the width and height so high, we're basically
	   forcing the Glk library to deal with text-wrapping and
	   page ends
	*/
	SCREENWIDTH = 0x7fff;
	SCREENHEIGHT = 0x7fff;
	FIXEDCHARWIDTH = 1;
	FIXEDLINEHEIGHT = 1;

	hugo_settextwindow(1, 1,
		SCREENWIDTH/FIXEDCHARWIDTH, SCREENHEIGHT/FIXEDLINEHEIGHT);
}


void hugo_cleanup_screen(void)
{
/* Does whatever has to be done to clean up the display pre-termination. */
}


#if !defined (PRINTFATALERROR_SUPPLIED)
void heglk_printfatalerror(char *err)
{
	static winid_t win = NULL;
	
	if (!win)
	{
		winid_t rootwin = glk_window_get_root();
		if (!rootwin)
			win = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
		else
			win = glk_window_open(rootwin, winmethod_Below | winmethod_Fixed, 
				3, wintype_TextBuffer, 0);
	}
	
	if (win)
	{
		glk_put_string_stream(glk_window_get_stream(win), err);
	}
}
#endif	/* PRINTFATALERROR_SUPPLIED */


void hugo_clearfullscreen(void)
{
/* Clears everything on the screen, moving the cursor to the top-left
   corner of the screen */

	glk_window_clear(mainwin);
	if (secondwin) glk_window_clear(secondwin);
	if (auxwin) glk_window_clear(auxwin);

	/* See hugo_print() for the need for this */
	if (currentwin==mainwin) mainwin_bgcolor = glk_bgcolor;

	/* Must be set: */
	currentpos = 0;
	currentline = 1;

	if (!inwindow) just_cleared_screen = true;
}


void hugo_clearwindow(void)
{
/* Clears the currently defined window, moving the cursor to the top-left
   corner of the window */

	/* If the engine thinks we're in a window, but Glk was
	   unable to comply, don't clear the window, because it's
	   not really a window
	*/
	if (inwindow && currentwin==mainwin) return;
	if (currentwin==NULL) return;

	glk_window_clear(currentwin);

	/* See hugo_print() for the need for this */
	if (currentwin==mainwin) mainwin_bgcolor = glk_bgcolor;

	/* If we're in a fixed-font (i.e., textgrid) auxiliary
	   window when we call for a clear, close auxwin and reset
	   the current window to mainwin
	*/
	if (auxwin)
	{
		stream_result_t sr;

		glk_window_close(auxwin, &sr);
		auxwin = NULL;
		glk_set_window(currentwin = mainwin);
	}

	/* Must be set: */
	currentpos = 0;
	currentline = 1;

	if (!inwindow) just_cleared_screen = true;
}


void hugo_settextmode(void)
{
/* This function does whatever is necessary to set the system up for
   a standard text display */

	charwidth = FIXEDCHARWIDTH;
	lineheight = FIXEDLINEHEIGHT;

	return;
}


void hugo_settextwindow(int left, int top, int right, int bottom)
{
	static int secondwin_bottom = 0;

	/* Hugo's arbitrarily positioned windows don't currently
	   mesh with what Glk has to offer, so we have to ignore any
	   non-Glk-ish Windows and just maintain the current
	   parameters
	*/
	if ((top!=1 || bottom>=physical_windowbottom/FIXEDLINEHEIGHT+1)
		/* Pre-v2.4 didn't support proper windowing */
		&& (game_version>=24 || !inwindow))
	{
		in_valid_window = false;
		
		/* Glk-illegal floating window; setting currentwin
		   to NULL will tell hugo_print() not to print in it:
		*/
		if (bottom<physical_windowbottom/FIXEDLINEHEIGHT+1)
		{
			currentwin = NULL;
			glk_set_window(mainwin);
			return;
		}
		else
			glk_set_window(currentwin = mainwin);
	}

	/* Otherwise this is a valid window (positioned along the
	   top of the screen a la a status window), so... */
	else
	{
		/* Arbitrary height of 4 lines for pre-v2.4 windows */
		if (game_version < 24) bottom = 4;

		/* ...either create a new window if none exists... */
		if (!secondwin)
		{
			winid_t p;

			p = glk_window_get_parent(mainwin);
			secondwin = glk_window_open(mainwin,//p,
				winmethod_Above | winmethod_Fixed,
				bottom,
				wintype_TextGrid,
				0);
		}

		/* ...or resize the existing one if necessary */
		else if (bottom!=secondwin_bottom)
		{
			winid_t p;

			p  = glk_window_get_parent(secondwin);
			glk_window_set_arrangement(p, 
				winmethod_Above | winmethod_Fixed,
				bottom,
				secondwin);
		}

		if (secondwin)
		{
			if (game_version < 24)
				glk_window_clear(secondwin);

			glk_set_window(currentwin = secondwin);
			in_valid_window = true;
			secondwin_bottom = bottom;
		}
		else
		{
			currentwin = NULL;
			glk_set_window(mainwin);
			secondwin_bottom = 0;
			return;
		}
	}

	physical_windowleft = (left-1)*FIXEDCHARWIDTH;
	physical_windowtop = (top-1)*FIXEDLINEHEIGHT;
	physical_windowright = right*FIXEDCHARWIDTH-1;
	physical_windowbottom = bottom*FIXEDLINEHEIGHT-1;
	physical_windowwidth = (right-left+1)*FIXEDCHARWIDTH;
	physical_windowheight = (bottom-top+1)*FIXEDLINEHEIGHT;
}


/* heglk_get_linelength

	Specially accommodated in GetProp() in heobject.c.
	While the engine thinks that the linelength is 0x7fff,
	this tells things like the display object the actual
	length.  (Defined as ACTUAL_LINELENGTH in heheader.h.)
*/

int heglk_get_linelength(void)
{
	static glui32 width;

	/* Try to use whatever fixed-width linelength is available */
	if (secondwin)
		glk_window_get_size(secondwin, &width, NULL);
	else if (auxwin)
		glk_window_get_size(auxwin, &width, NULL);

	/* Otherwise try to approximate it by the proportionally
	   spaced linelength
	*/
	else
		glk_window_get_size(mainwin, &width, NULL);

	/* -1 to override automatic line wrapping */
	return width-1;
}


/* heglk_get_screenheight

	Similar to heglk_get_linelength().  (Defined as
	ACTUAL_SCREENHEIGHT in heheader.h.)
*/

int heglk_get_screenheight(void)
{
	static glui32 height = 0, mainheight = 0;

	if (secondwin)
		glk_window_get_size(secondwin, NULL, &height);
	else if (auxwin)
		glk_window_get_size(auxwin, NULL, &height);

	glk_window_get_size(mainwin, NULL, &mainheight);

	return height+mainheight;
}


void hugo_settextpos(int x, int y)
{
/* The top-left corner of the current active window is (1, 1). */

	if (currentwin==NULL) return;

	/* Try to determine if we're trying to position fixed-width
	   text in the main window, as in a menu, for example
	*/
	if (!just_cleared_screen && !inwindow &&
		!(glk_current_font & PROP_FONT)
		&& y!=1			/* not just cls */
		&& y < SCREENHEIGHT-0x0f)	/* 0x0f is arbitrary */
	{
		/* See if we're already in the auxiliary window */
		if (currentwin!=auxwin)
		{
			/* If not, create it, making it 100% of
			   mainwin's height
			*/
			if (auxwin==NULL)
				auxwin = glk_window_open(mainwin,
					winmethod_Below|winmethod_Proportional,
					100,
					wintype_TextGrid,
					0);
			else
				glk_window_clear(auxwin);

			glk_set_window(currentwin = auxwin);
		}
	}

	/* On the other hand, if we were in a textgrid window and
	   no longer need to be, get out
	*/
	else if (auxwin)
	{
		stream_result_t sr;

		/* Close auxwin */
		glk_window_close(auxwin, &sr);
		auxwin = NULL;

		/* Clear the screen (both windows) */
		glk_window_clear(mainwin);
		glk_window_clear(secondwin);

		glk_set_window(currentwin = mainwin);
	}

	just_cleared_screen = false;

	/* Can only move the Glk cursor in a textgrid window */
	if (currentwin!=mainwin)
		glk_window_move_cursor(currentwin, x-1, y-1);

	/* Must be set: */
	currentline = y;
	currentpos = (x-1)*CHARWIDTH;   /* Note:  zero-based */
}


void hugo_print(char *a)
{
/* Essentially the same as printf() without formatting, since printf()
   generally doesn't take into account color setting, font changes,
   windowing, etc.

   The newline character '\n' must be explicitly included at the end of
   a line in order to produce a linefeed.  The new cursor position is set
   to the end of this printed text.  Upon hitting the right edge of the
   screen, the printing position wraps to the start of the next line.
*/
	static char just_printed_linefeed = false;
	/* static already_modified_style = false; */

	/* Can't print in a Glk-illegal window since it hasn't been
	   created
	*/
	if (currentwin==NULL) return;

	/* In lieu of colors, in case we're highlighting something
	   such as a menu selection:
	*/
/*
	if (!inwindow and glk_bgcolor!=mainwin_bgcolor)
	{
		if (!already_modified_style)
		{
			if (glk_current_font & BOLD_FONT)
				glk_set_style(style_Normal);
			else
				glk_set_style(style_Emphasized);
		}
		already_modified_style = true;
	}
	else
		already_modified_style = false;
*/

	if (a[0]=='\n')
	{
		if (!just_printed_linefeed)
		{
			glk_put_string("\n");
		}
		else
			just_printed_linefeed = false;
	}
	else if (a[0]=='\r')
	{
		if (!just_printed_linefeed)
		{
			glk_put_string("\n");
			just_printed_linefeed = true;
		}
		else
			just_printed_linefeed = false;
	}
	else
	{
		glk_put_string(a);
		just_printed_linefeed = false;
	}

	just_displayed_something = true;
}


void hugo_scrollwindowup()
{
	/* Glk should look after this for us */
}


void hugo_font(int f)
{
	static char using_prop_font = false;

/* The <f> argument is a mask containing any or none of:
   BOLD_FONT, UNDERLINE_FONT, ITALIC_FONT, PROP_FONT.
*/
	glk_current_font = f;

	glk_set_style(style_Normal);

	if (f & BOLD_FONT)
		glk_set_style(style_Emphasized);

	if (f & UNDERLINE_FONT)
		glk_set_style(style_Emphasized);

	if (f & ITALIC_FONT)
		glk_set_style(style_Emphasized);

	if (f & PROP_FONT)
		using_prop_font = true;

/* Have to comment this out, it seems, because it will mess up the
   alignment of the input in the main window
	if (!(f & PROP_FONT))
		glk_set_style(style_Preformatted);
*/

/* Workaround to decide if we have to open auxwin for positioned
   non-proportional text:
*/
	if (!(f & PROP_FONT))
	{
		/* If at top of screen, and changing to a fixed-
		   width font (a situation which wouldn't normally
		   be adjusted for by hugo_settextpos())
		*/
		if (!inwindow && currentline==1 && currentpos==0 && using_prop_font)
		{
			just_cleared_screen = false;
			hugo_settextpos(1, 2);
			glk_window_move_cursor(currentwin, 0, 0);
		}
	}
}


void hugo_settextcolor(int c)           /* foreground (print) color */
{
/* Set the foreground color to hugo_color(c) */

	glk_fcolor = hugo_color(c);
}


void hugo_setbackcolor(int c)           /* background color */
{
/* Set the background color to hugo_color(c) */

	glk_bgcolor = hugo_color(c);
}


int hugo_color(int c)
{
	/* Color-setting functions should always pass the color through
	   hugo_color() in order to properly set default fore/background
	   colors:
	*/

	if (c==16)      c = DEF_FCOLOR;
	else if (c==17) c = DEF_BGCOLOR;
	else if (c==18) c = DEF_SLFCOLOR;
	else if (c==19) c = DEF_SLBGCOLOR;
	else if (c==20) c = hugo_color(fcolor);	/* match foreground */

/* Uncomment this block of code and change "c = ..." values if the system
   palette differs from the Hugo palette.

   If colors are unavailable on the system in question, it may suffice
   to have black, white, and brightwhite (i.e. boldface).  It is expected
   that colored text will be visible on any other-colored background.

	switch (c)
	{
		case HUGO_BLACK:	 c = 0;  break;
		case HUGO_BLUE:		 c = 1;  break;
		case HUGO_GREEN:	 c = 2;  break;
		case HUGO_CYAN:		 c = 3;  break;
		case HUGO_RED:		 c = 4;  break;
		case HUGO_MAGENTA:	 c = 5;  break;
		case HUGO_BROWN:	 c = 6;  break;
		case HUGO_WHITE:	 c = 7;  break;
		case HUGO_DARK_GRAY:	 c = 8;  break;
		case HUGO_LIGHT_BLUE:	 c = 9;  break;
		case HUGO_LIGHT_GREEN:	 c = 10; break;
		case HUGO_LIGHT_CYAN:	 c = 11; break;
		case HUGO_LIGHT_RED:	 c = 12; break;
		case HUGO_LIGHT_MAGENTA: c = 13; break;
		case HUGO_YELLOW:	 c = 14; break;
		case HUGO_BRIGHT_WHITE:	 c = 15; break;
*/
	return c;
}


/* CHARACTER AND TEXT MEASUREMENT

	For non-proportional printing, screen dimensions will be given
	in characters, not pixels.

	For proportional printing, screen dimensions need to be in
	pixels, and each width routine must take into account the
	current font and style.

	The hugo_strlen() function is used to give the length of
	the string not including any non-printing control characters.
*/

int hugo_charwidth(char a)
{
	/* As given here, this function works only for non-proportional
	   printing.  For proportional printing, hugo_charwidth() should
	   return the width of the supplied character in the current
	   font and style.
	*/

	if (a==FORCED_SPACE)
		return CHARWIDTH;         /* same as ' ' */

	else if ((unsigned char)a >= ' ') /* alphanumeric characters */

		return CHARWIDTH;         /* for non-proportional */

	return 0;
}

int hugo_textwidth(char *a)
{
	int i, slen, len = 0;

	slen = (int)strlen(a);

	for (i=0; i<slen; i++)
	{
		if (a[i]==COLOR_CHANGE) i+=2;
		else if (a[i]==FONT_CHANGE) i++;
		else
			len += hugo_charwidth(a[i]);
	}

	return len;
}

int hugo_strlen(char *a)
{
	int i, slen, len = 0;

	slen = (int)strlen(a);

	for (i=0; i<slen; i++)
	{
		if (a[i]==COLOR_CHANGE) i+=2;
		else if (a[i]==FONT_CHANGE) i++;
		else len++;
	}

	return len;
}


/* GRAPHICS
  
   The tack taken here is to try to build a blorb set of resources for
   each Hugo picture available on the first call to hugo_displaypicture().

   Thereafter, each picture requested is translated into a blorb image
   resource number, and this one is used to display the image.
*/

#if defined (GLK_MODULE_IMAGE)
int hugo_hasgraphics(void)
{
/* Returns true if the current display is capable of graphics display */
	return glk_gestalt(gestalt_Graphics, 0)
		&& glk_gestalt(gestalt_DrawImage,
				glk_window_get_type(mainwin));
}

void _glk_put_uint32_stream(strid_t stream, glui32 uint32)
{
/* Write a uint32, big-endian, to a stream. */
	char buf[4];

	buf[0] = (char)((uint32 >> 24) & 0xff);
	buf[1] = (char)((uint32 >> 16) & 0xff);
	buf[2] = (char)((uint32 >>  8) & 0xff);
	buf[3] = (char)((uint32      ) & 0xff);
	glk_put_buffer_stream(stream, buf, sizeof (buf));
}

/* Glk doesn't like to display pictures that exceed the window size, yet
   is reticent about telling you just what the window size is.
*/
#define PIC_MAX_WIDTH	640
#define PIC_MAX_HEIGHT	480

int hugo_displaypicture(HUGO_FILE infile, long reslength)
{
	static int initialized = false;
	static struct descriptor {
		long resposition;
		long reslength;
		int valid;
	} *descriptors = NULL;
	static int rescount = 0;
	static frefid_t fileref = NULL;

	long resposition;
	int image;

	/* Clean up any resources in use if called on leaving glk_main(). */
	if (infile==NULL && reslength==0)
	{
		/* Clean up only if something exists to clean up. */
		if (initialized)
		{
			giblorb_map_t	*map;

			/* Try to remove the set blorb resources map. */
			map = giblorb_get_resource_map ();
			if (map!=NULL)
				giblorb_destroy_map(map);

			/* Deallocate descriptors. */
			if (descriptors!=NULL)
			{
				hugo_blockfree(descriptors);
				descriptors = NULL;
			}
			rescount = 0;

			/* Glk doesn't always clear away temporary files
			   (Xglk is poor at this), so take care of it here.
			*/
			if (fileref!=NULL)
			{
				glk_fileref_delete_file(fileref);
				glk_fileref_destroy(fileref);
				fileref = NULL;
			}
			initialized = false;
		}
		return true;
	}

	/* Ignore the call if the current window is set elsewhere. */
	if (currentwin!=NULL && currentwin!=mainwin) {
		fclose(infile);
		return false;
	}

	/* Not cleaning up, so note the current position; along with the
	   resource length, it identifies the picture.
	*/
	resposition = ftell(infile);

	/* On first call, encapsulate all available pictures in a blorb
	   file wrapper.
	*/
	if (!initialized)
	{
		int i, res_32bits, resfileversion;
		unsigned int startofdata;
		long resposition, reslength, total, offset;
		strid_t stream;

		/* Rewind the file, then read the resourcefile header */
		fseek(infile, 0, SEEK_SET);

		i = fgetc(infile);
		if (i=='r')
			res_32bits = true;
		else if (i=='R')
			res_32bits = false;
		else
		{
			fclose(infile);
			return false;
		}

		resfileversion = fgetc(infile);
		rescount = fgetc(infile);
		rescount += fgetc(infile)*256;
		startofdata = fgetc(infile);
		startofdata += (unsigned int)fgetc(infile)*256;

		/* Allocate descriptors for each resource, and get the
		   position and length for every one available.  Assume
		   valid JPEG images for now.
		*/
		descriptors = hugo_blockalloc(rescount * sizeof (*descriptors));
		for (i=0; i<rescount; i++)
		{
			int len;

			len = fgetc(infile);
			fseek(infile, len, SEEK_CUR);

			resposition = (long)fgetc(infile);
			resposition += (long)fgetc(infile)*256L;
			resposition += (long)fgetc(infile)*65536L;
			if (res_32bits)
			{
				resposition +=
					(long)fgetc(infile)*16777216L;
			}

			reslength = (long)fgetc(infile);
			reslength += (long)fgetc(infile)*256L;
			reslength += (long)fgetc(infile)*65536L;
			if (res_32bits)
			{
				reslength +=
					(long)fgetc(infile)*16777216L;
			}

			descriptors[i].resposition = resposition + startofdata;
			descriptors[i].reslength = reslength;
			descriptors[i].valid = true;
		}

		/* Build a blorb file out of the pictures.  Start by opening
		   a stream for the file.
		*/
//		fileref = glk_fileref_create_by_name
//					(fileusage_Data, "BLORB", 0);
		fileref = glk_fileref_create_temp (fileusage_Data, 0);
		if (fileref==NULL)
		{
			hugo_blockfree(descriptors);
			fclose(infile);
			return false;
		}
		stream = glk_stream_open_file(fileref, filemode_Write, 0);
		if (stream==NULL)
		{
			glk_fileref_destroy(fileref); fileref = NULL;
			hugo_blockfree(descriptors);
			fclose(infile);
			return false;
		}

		/* Total up data for the blorb header, adding one padding
		   byte where the length is odd.
		*/
		total = 0;
		for (i=0; i<rescount; i++)
		{
			total += 8 + descriptors[i].reslength
					+ descriptors[i].reslength % 2;
		}

		/* Write the main blorb header. */
		glk_put_string_stream(stream, "FORM");
		_glk_put_uint32_stream(stream, 4 + 12 + rescount * 12 + total);
		glk_put_string_stream(stream, "IFRS");

		/* Write the blorb resources index. */
		glk_put_string_stream(stream, "RIdx");
		_glk_put_uint32_stream(stream, 4 + rescount * 12);
		_glk_put_uint32_stream(stream, rescount);

		offset = 12 + 12 + rescount * 12;
		for (i=0; i<rescount; i++)
		{
			/* Write resource index entry. */
			glk_put_string_stream(stream, "Pict");
			_glk_put_uint32_stream(stream, i);
			_glk_put_uint32_stream(stream, offset);

			/* Update notional offset, again with the padding. */
			offset += 8 + descriptors[i].reslength
					+ descriptors[i].reslength % 2;
		}

		/* Write individual entry chunks. */
		for (i=0; i<rescount; i++)
		{
			unsigned char buf[10240];
			long remaining, len;

			/* Write chunk introducer. */
			glk_put_string_stream(stream, "JPEG");
			_glk_put_uint32_stream(stream,
						descriptors[i].reslength);

			/* Read the first data byte for a JPEG check, and
			   invalidate non-JPEG resources.
			*/
			fseek(infile, descriptors[i].resposition, SEEK_SET);
			if (fgetc(infile)!=0xff)
				descriptors[i].valid = false;
			fseek(infile, -1, SEEK_CUR);

			/* Copy the picture data into the blorb file. */
			remaining = descriptors[i].reslength;
			do {
				len = fread (buf, 1,
					remaining < sizeof (buf)
						? remaining : sizeof (buf),
						infile);
				glk_put_buffer_stream(stream, buf, len);
				remaining -= len;
			} while (remaining > 0 && len > 0);

			/* Add padding byte on odd length resources. */
			if (descriptors[i].reslength % 2)
				glk_put_char_stream(stream, 0);
		}

		/* Close blorb file, then reopen for read and pass to Glk
		   as a resources set.
		*/
		glk_stream_close(stream, NULL);
		stream = glk_stream_open_file(fileref, filemode_Read, 0);
		if (stream==0)
		{
			glk_fileref_delete_file(fileref);
			glk_fileref_destroy(fileref); fileref = NULL;
			hugo_blockfree(descriptors);
			fclose(infile);
			return false;
		}
		if (giblorb_set_resource_map(stream)!=giblorb_err_None)
		{
			glk_stream_close(stream, NULL);
			glk_fileref_delete_file(fileref);
			glk_fileref_destroy(fileref); fileref = NULL;
			hugo_blockfree(descriptors);
			fclose(infile);
			return false;
		}

		/* Note that the blorb resources are safely constructed. */
		initialized = true;
	}

	/* Close infile now -- we use blorb for image data. */
	fclose(infile);

	/* Locate the blorb resource that matches the picture requested. */
	for (image=0; image<rescount; image++)
	{
		if (descriptors[image].valid
				&& descriptors[image].resposition==resposition
				&& descriptors[image].reslength==reslength)
			break;
	}
	if (image<rescount)
	{
		glui32 width, height;

		/* Get picture width and height for scaling. */
		if (glk_image_get_info(image, &width, &height))
		{
			/* Scale large images. */
			if (width>PIC_MAX_WIDTH)
			{
				height = height * PIC_MAX_WIDTH / width;
		       		width=PIC_MAX_WIDTH;
			}
			if (height>PIC_MAX_HEIGHT)
			{
				width = width * PIC_MAX_HEIGHT / height;
		       		height=PIC_MAX_HEIGHT;
			}

			/* Draw, then move cursor down to the next line. */
			glk_image_draw_scaled(mainwin, image,
					imagealign_InlineUp, 0, width, height);
			glk_put_char('\n');
		}

		just_displayed_something = true;
		return true;
	}
	else
		return false;
}
#else
int hugo_hasgraphics(void)
{
	return false;
}

int hugo_displaypicture(HUGO_FILE infile, long reslength)
{
	fclose(infile);
	return true;
}
#endif


/*
 * Replacements for things the Glk port doesn't support:
 *
 */

#if !defined (SETTITLE_SUPPORTED)
void hugo_setgametitle(char *t)
{}
#endif

#if !defined (COMPILE_V25)
int hugo_hasvideo(void)
{
	return false;
}

int hugo_playvideo(HUGO_FILE infile, long reslength,
	char loop_flag, char background, int volume)
{
	fclose(infile);
	return true;
}

void hugo_stopvideo(void)
{}
#endif

#if !defined (SOUND_SUPPORTED)
int hugo_playmusic(HUGO_FILE infile, long reslength, char loop_flag)
{
	fclose(infile);
	return true;	/* not an error */
}

void hugo_musicvolume(int vol)
{}

void hugo_stopmusic(void)
{}

int hugo_playsample(HUGO_FILE infile, long reslength, char loop_flag)
{
	fclose(infile);
	return true;	/* not an error */
}

void hugo_samplevolume(int vol)
{}

void hugo_stopsample(void)
{}
#endif

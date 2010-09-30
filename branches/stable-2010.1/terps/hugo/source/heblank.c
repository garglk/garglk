/*
	HEBLANK.C

	Template for non-portable system-specific functions:

		hugo_blockalloc         hugo_clearfullscreen
		hugo_blockfree          hugo_clearwindow
					hugo_settextmode
		hugo_splitpath          hugo_settextwindow
		hugo_makepath           hugo_settextpos
		hugo_getfilename        hugo_scrollwindowup
		hugo_overwrite          hugo_font
		hugo_closefiles         hugo_settextcolor
					hugo_setbackcolor
		hugo_getkey             hugo_color
		hugo_getline
		hugo_waitforkey         hugo_print
		hugo_iskeywaiting	hugo_charwidth
		hugo_timewait		hugo_textwidth
					hugo_strlen
		hugo_addcommand
		hugo_restorecommand

		hugo_init_screen
		hugo_hasgraphics
		hugo_setgametitle
		hugo_cleanup_screen

	for the Hugo Engine

	Copyright (c) 1995-2005 by Kent Tessman

	NOTE:  In the simplest case, this should be portable to any
	system by replacing each "..." section below with a function
	that accomplishes the described task under the compiler/
	system in question.
	
	Where possible, simple stdio routines are provided.
*/

#include "heheader.h"

/* For the stdio version, the ARBITRARY_SCREEN_WIDTH must be equal
   to the width of the target text display in order for proper line-
   wrapping (since printf() wraps the line automatically at the
   right edge)
*/
#define ARBITRARY_SCREEN_WIDTH 80
#define ARBITRARY_SCREEN_HEIGHT 24


/* Function prototypes: */

/* These should be provided by all ports: */
int hugo_color(int c);
int hugo_hasgraphics(void);


/* Definitions and variables: */

/* Defined Hugo colors: */
#define HUGO_BLACK         0
#define HUGO_BLUE          1
#define HUGO_GREEN         2
#define HUGO_CYAN          3
#define HUGO_RED           4
#define HUGO_MAGENTA       5
#define HUGO_BROWN         6
#define HUGO_WHITE         7
#define HUGO_DARK_GRAY     8
#define HUGO_LIGHT_BLUE    9
#define HUGO_LIGHT_GREEN   10
#define HUGO_LIGHT_CYAN    11
#define HUGO_LIGHT_RED     12
#define HUGO_LIGHT_MAGENTA 13
#define HUGO_YELLOW        14
#define HUGO_BRIGHT_WHITE  15

/* Since we provide our own command history: */
#define HISTORY_SIZE    16              /* for command-line editing */
int hcount = 0;
char *history[HISTORY_SIZE];


/*
    MEMORY ALLOCATION:

    hugo_blockalloc(), and hugo_blockfree() are necessary because not
    all systems (MS-DOS, for one) can handle memory allocation of more
    than 64K.  For most systems, these will simply be normal ANSI
    function calls.
*/

void *hugo_blockalloc(long num)
{
	return malloc(num * sizeof(char));
}

void hugo_blockfree(void *block)
{
	free(block);
}


/*
    FILENAME MANAGEMENT:

    Different operating systems have different ways of naming files.
    The following routines are simply required to know and be able to
    dissect/build the components of a particular filename,
    storing/restoring the components via the specified char arrays.

    For example, in MS-DOS:

	hugo_splitpath("C:\HUGO\FILES\HUGOLIB.H", ...)
		becomes:  C:, HUGO\FILES, HUGOLIB, H

    and

	hugo_makepath(..., "C:", "HUGO\FILES", "HUGOLIB", "H")
		becomes:  C:\HUGO\FILES\HUGOLIB.H

    The appropriate equivalent nomenclature should be used for the
    operating system in question.
*/

/* The following supplied functions will work for Unix-style pathnames: */

void hugo_splitpath(char *path, char *drive, char *dir, char *fname, char *ext)
{
char *file;
char *extension;

        strcpy(drive,"");
        strcpy(dir,"");
        strcpy(fname,"");
        strcpy(ext,"");

        if ((file = strrchr(path,'/')) == 0)
        {
                if ((file = strrchr(path,':')) == 0) file = path;
        }
        strncpy(dir,path,strlen(path)-strlen(file));
        *(dir+strlen(path)-strlen(file)) = 0;
        extension = strrchr(file,'.');
        if (extension != 0)
        {
                strncpy(fname,file,strlen(file)-strlen(extension));
                *(fname+strlen(file)-strlen(extension)) = 0;
                strcpy(ext,extension+1);
        }
        else strcpy(fname,file);

        if (strcmp(dir, "") && fname[0]=='/') strcpy(fname, fname+1);
}

void hugo_makepath(char *path, char *drive, char *dir, char *fname, char *ext)
{
        if (*ext == '.') ext++;
        strcpy(path,drive);
        strcat(path,dir);
        switch (*(path+strlen(path)))
        {
        case '/':
        case ':':
/*        case 0: */
                break;
        default:
                if (strcmp(path, "")) strcat(path,"/");
                break;
        }
        strcat(path,fname);
        if (strcmp(ext, "")) strcat(path,".");
        strcat(path,strlwr(ext));
}


/* hugo_getfilename

    Loads the name of the filename to save or restore (as specified by
    the argument <a>) into the line[] array.

    The reason this is in the system-specific file is because it may be
    preferable to replace it with, for example, a dialog-based file
    selector.
*/

void hugo_getfilename(char *a, char *b)
{
	unsigned int i, p;

	sprintf(line, "Enter path and filename %s.", a);

	AP(line);

	sprintf(line,"%c(Default is %s): \\;", NO_CONTROLCHAR, b);
	AP(line);

	p = var[prompt];
	var[prompt] = 0;        /* null string */

	RunInput();

	var[prompt] = p;

	remaining = 0;

	strcpy(line, "");
	if (words==0)
		strcpy(line, b);
	else
	{
		for (i=1; i<=(unsigned int)words; i++)
			strcat(line, word[i]);
	}
}


/* hugo_overwrite

    Checks to see if the given filename already exists, and prompts to
    replace it.  Returns true if file may be overwritten.

    Again, it may be preferable to replace this with something fancier.
*/

int hugo_overwrite(char *f)
{
	FILE *tempfile;

	tempfile = fopen(f, "rb");
	if (tempfile==NULL)                     /* if file doesn't exist */
		return true;

	fclose(tempfile);

	sprintf(pbuffer, "Overwrite existing \"%s\" (Y or N)?", f);
	RunInput();

	if (words==1 && (!strcmp(strupr(word[1]), "Y") || !strcmp(strupr(word[1]), "YES")))
		return true;

	return false;
}


/* hugo_closefiles

    Closes all open files.  NOTE:  If the operating system automatically
    closes any open streams upon exit from the program, this function may
    be left empty.
*/

void hugo_closefiles()
{
/*
	fclose(game);
	if (script) fclose(script);
	if (io) fclose(io);
	if (record) fclose(record);
*/
}


/* hugo_getkey

    Returns the next keystroke waiting in the keyboard buffer.  It is
    expected that hugo_getkey() will return the following modified
    keystrokes:

	up-arrow        11 (CTRL-K)
	down-arrow      10 (CTRL-J)
	left-arrow       8 (CTRL-H)
	right-arrow     21 (CTRL-U)
*/

int hugo_getkey(void)
{
	/* int b = fgetc(stdin); */
	fgets(buffer, MAXBUFFER, stdin);
/*
	if (...b is a function key...)
	{
		switch (...the function given by b...)
		{
			case ...up-arrow...:
				{b = 11;
				break;}
			case ...left-arrow...:
				{b = 8;
				break;}
			case ...right-arrow...:
				{b = 21;
				break;}
			case ...down-arrow...:
				{b = 10;
				break;}
		}
	}
*/
	/* return b; */
	if (buffer[0]=='\0' || buffer[0]=='\n' || buffer[0]=='\r') buffer[0] = 13;
	return buffer[0];
}


/* hugo_getline

    Gets a line of input from the keyboard, storing it in <buffer>.
*/

void hugo_getline(char *p)
{
	/* stdio implementation */
	hugo_print(p);
	fgets(buffer, MAXBUFFER, stdin);
	buffer[strlen(buffer)-1] = '\0';
}


/* hugo_sendtoscrollback

   Stores a given line in the scrollback buffer (optional).
*/

void hugo_sendtoscrollback(char *a)
{
	/* not implemented here */
}


/* hugo_waitforkey

    Provided to be replaced by multitasking systems where cycling while
    waiting for a keystroke may not be such a hot idea.

    If kbhit() doesn't exist, has a different name, or has functional
    implications (i.e., on a multitasking system), this will have to
    be modified.
*/

int hugo_waitforkey(void)
{
	/* stdio implementation */
	return hugo_getkey();
}


/* hugo_iskeywaiting

    Returns true if a keypress is waiting to be retrieved.
*/

int hugo_iskeywaiting(void)
{
	var[system_status] = -1;	/* STAT_UNAVAILABLE */
	return false;	
}


/* hugo_timewait

    Waits for 1/n seconds.  Returns false if waiting is unsupported.
*/

int hugo_timewait(int n)
{
	/* No stdio implementation */
	return false;
}


/* DISPLAY CONTROL:

   Briefly, the variables required to interface with the engine's output
   functions are:

   The currently defined window, in zero-based coordinates (either
   character coordinates or pixel coordinates, as appropriate):

	physical_windowleft, physical_windowtop,
	physical_windowright, physical_windowbottom,
	physical_windowwidth, physical_windowheight

   The currently selected font is described by the ..._FONT bitmasks in:

	currentfont

   The currently selected font's width and height:

	charwidth, lineheight

   The non-proportional/fixed-width font's width and height (i.e., equal
   to charwidth and lineheight when the current font is the fixed-width
   font):

	FIXEDCHARWIDTH, FIXEDLINEHEIGHT

   Must be set by hugo_settextpos(), hugo_clearfullscreen(), and
   hugo_clearwindow():

	currentpos, currentline
*/

void hugo_init_screen(void)
{
/* Does whatever has to be done to initially set up the display. */
}

int hugo_hasgraphics(void)
{
/* Returns true if the current display is capable of graphics display;
   returns 2 if graphics are being routed to an external window other
   than the main display. */

	/* no graphics under stdio */
	return false;
}

void hugo_setgametitle(char *t)
{
/* If the system doesn't provide any way to set the caption for the
   screen/window, this can be empty */
}

void hugo_cleanup_screen(void)
{
/* Does whatever has to be done to clean up the display pre-termination. */
}

void hugo_clearfullscreen(void)
{
/* Clears everything on the screen, moving the cursor to the top-left
   corner of the screen */

	/* no screen-clearing under stdio */
	
	/* Must be set: */
	currentpos = 0;
	currentline = 1;
}

void hugo_clearwindow(void)
{
/* Clears the currently defined window, moving the cursor to the top-left
   corner of the window */

	/* no screen-clearing under stdio */

	/* Must be set: */
	currentpos = 0;
	currentline = 1;
}

void hugo_settextmode(void)
{
/* This function does whatever is necessary to set the system up for
   a standard text display */

/*
				Pixel-based		Character-based
				-----------		---------------

	FIXEDCHARWIDTH =	...font width...		 1
	FIXEDLINEHEIGHT =	...font height...		 1

	SCREENWIDTH = 		...# x pixels...		80
	SCREENHEIGHT =  	...# y pixels...		25

	As an example of how character-based and pixel-based
	systems might provide the same parameters, the following
	two sets of parameters are identical:

		FIXEDCHARWIDTH		  8	   1
		FIXEDLINEHEIGHT		 16	   1

		SCREENWIDTH		640	  80  (640 /  8 = 80)
		SCREENHEIGHT		400	  25  (400 / 16 = 25)

	Then set:
	
	charwidth = current font width, in pixels or 1
	lineheight = current font height, in pixels or 1

	Both charwidth and lineheight must change dynamically if the
	metrics for the currently selected font change
*/

	/* stdio implementation: */
	charwidth = lineheight = FIXEDCHARWIDTH = FIXEDLINEHEIGHT = 1;
	SCREENWIDTH = ARBITRARY_SCREEN_WIDTH;
	SCREENHEIGHT = ARBITRARY_SCREEN_HEIGHT;

	/* Must be set: */
	hugo_settextwindow(1, 1,
		SCREENWIDTH/FIXEDCHARWIDTH, SCREENHEIGHT/FIXEDLINEHEIGHT);
}

void hugo_settextwindow(int left, int top, int right, int bottom)
{
/* Once again, the arguments for the window are passed using character
   coordinates--a system setting a window using pixel coordinates will
   have to make the necessary conversions using FIXEDCHARWIDTH and 
   FIXEDLINEHEIGHT.

   The text window, once set, represents the scrolling/bottom part of the
   screen.  It is also assumed that the text window will constrain the
   cursor location--see hugo_settextposition(), below.

   This function must also set physical_window... parameters using
   the transformations given.
*/
	/* Create a text window from (column, row) character-coordinates
	(left, top) to (right, bottom)
	*/
	
	/* No window setting under stdio, but print a separator: */
#ifndef NO_STDIO_WINDOWS
	line[0] = '\n';
	memset(line+1, '-', ARBITRARY_SCREEN_WIDTH);
	line[ARBITRARY_SCREEN_WIDTH+1] = '\0';
	printf(line);
#endif

	/* Must be set: */
	physical_windowleft = (left-1)*FIXEDCHARWIDTH;
	physical_windowtop = (top-1)*FIXEDLINEHEIGHT;
	physical_windowright = right*FIXEDCHARWIDTH-1;
	physical_windowbottom = bottom*FIXEDLINEHEIGHT-1;
	physical_windowwidth = (right-left+1)*FIXEDCHARWIDTH;
	physical_windowheight = (bottom-top+1)*FIXEDLINEHEIGHT;

	/* Note that, e.g., a full-screen window on an 80x25 text screen,
	   would result in:

		physical_windowleft = 0
		physical_windowtop = 0
		physical_windowright = 79
		physical_windowbottom = 24

	   On a 640x480 graphics screen,

		physical_windowright = 639
		physical_windowbottom = 479
	*/
}


void hugo_settextpos(int x, int y)
{
/* The top-left corner of the current active window is (1, 1).

   (In other words, if the screen is being windowed so that the top row
   of the window is row 4 on the screen, the (1, 1) refers to the 4th
   row on the screen, and (1, 2) refers to the 5th.)

   All cursor-location is based on FIXEDCHARWIDTH and FIXEDLINEHEIGHT.

   This function must also properly set currentline and currentpos (where
   currentline is a the current character line, and currentpos may be
   either in pixels or characters, depending on the measure being used).

   Note that the Hugo function call uses x and y directly as text-
   screen coordinates; pixel-based systems will likely have to make a
   calculation to pixel coordinates.  In other words, pixel-based
   screen location will need to calculate pixel coordinates to
   simulate text-screen coordinates.
*/
	/* no stdio implementation, but keep things from running
	   together */
	if (inwindow) printf(" ");

	/* Must be set: */
	currentline = y;
	currentpos = (x-1)*charwidth;   /* Note:  zero-based */
}

void PrintFatalError(char *a)
{
	/* PRINTFATALERROR may be #defined in heheader.h */

	hugo_print(a);
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

	/* Output <a>, taking into account fore/background color,
	   font, current window, etc.
	*/
	
	/* stdio implementation */
#ifdef NO_STDIO_WINDOWS
	if (inwindow) return;
#endif
	printf(a);
}

void hugo_scrollwindowup()
{
	/* Scroll the current text window up one line */
	
	/* stdio implementation */
	printf("\n");
}

void hugo_font(int f)
{
/* The <f> argument is a mask containing any or none of:
   BOLD_FONT, UNDERLINE_FONT, ITALIC_FONT, PROP_FONT.

   If charwidth and lineheight change with a font change, these must be
   reset here as well.
*/

/*
	if (f & UNDERLINE_FONT)
		...do font underlining...

	if (f & ITALIC_FONT)
		...use italic font...

	if (f & BOLD_FONT)
		...use bold font...

	if (f & PROP_FONT)
		...use proportional font...
*/

	/* no stdio implementation */
}

void hugo_settextcolor(int c)           /* foreground (print) color */
{
	/* Set the foreground color to hugo_color(c) */
	
	/* no stdio implementation */
}

void hugo_setbackcolor(int c)           /* background color */
{
	/* Set the background color to hugo_color(c) */
	
	/* no stdio implementation */
}

int hugo_color(int c)
{
	/* no stdio implementation */
	
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
		return FIXEDCHARWIDTH;		/* same as ' ' */

	else if ((unsigned char)a >= ' ')	/* alphanumeric characters */

		return FIXEDCHARWIDTH;		/* for non-proportional */

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


/* Resource functions: */

int hugo_displaypicture(FILE *infile, long len)
{
        fclose(infile);		/* since infile will be open */

        return 1;
}

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

#endif	/* !defined (SOUND_SUPPORTED) */

#if !defined (COMPILE_V25)

int hugo_hasvideo(void)
{
	return false;
}

void hugo_stopvideo(void)
{}

int hugo_playvideo(HUGO_FILE infile, long reslength, char loop_flag)
{
	fclose(infile);
	return true;	/* not an error */
}

#endif	/* !defined (COMPILE_V25) */

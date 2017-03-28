#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OSGEN.C,v 1.3 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1990, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osgen  - Operating System dependent functions, general implementation
Function
  This module contains certain OS-dependent functions that are common
  between several systems.  Routines in this file are selectively enabled
  according to macros defined in os.h:

    USE_STDIO     - implement os_print, os_flush, os_gets with stdio functions
    USE_DOSEXT    - implement os_remext, os_defext using MSDOS-like filename
                    conventions
    USE_NULLINIT  - implement os_init and os_term as do-nothing routines
    USE_NULLPAUSE - implement os_expause as a do-nothing routine
    USE_EXPAUSE   - use an os_expause that prints a 'strike any key' message
                    and calls os_waitc
    USE_TIMERAND  - implement os_rand using localtime() as a seed
    USE_NULLSTAT  - use a do-nothing os_status function
    USE_NULLSCORE - use a do-nothing os_score function
    RUNTIME       - enable character-mode console implementation  
    USE_STATLINE  - implement os_status and os_score using character-mode
                    status line implementation
    USE_OVWCHK    - implements default saved file overwrite check
    USE_NULLSTYPE - use a dummy os_settype routine
    USE_NULL_SET_TITLE - use an empty os_set_title() implementation

    If USE_STDIO is defined, we'll implicitly define USE_STDIO_INPDLG.

    If USE_STATLINE is defined, certain subroutines must be provided for
    your platform that handle the character-mode console:
        ossclr - clears a portion of the screen
        ossdsp - displays text in a given color at a given location
        ossscr - scroll down (i.e., moves a block of screen up)
        ossscu - scroll up (i.e., moves a block of screen down)
        ossloc - locate cursor

    If USE_STATLINE is defined, certain sub-options can be enabled:
        USE_SCROLLBACK - include output buffer capture in console system
        USE_HISTORY    - include command editing and history in console system
Notes

Modified
  01/01/98 MJRoberts     - moved certain osgen.c routines to osnoui.c  
  04/24/93 JEras         - add os_locate() for locating tads-related files
  04/12/92 MJRoberts     - add os_strsc (string score) function
  03/26/92 MJRoberts     - add os_setcolor function
  09/26/91 MJRoberts     - os/2 user exit support
  09/04/91 MJRoberts     - stop reading resources if we find '$eof' resource
  08/28/91 MJRoberts     - debugger bug fix
  08/01/91 MJRoberts     - make runstat work correctly
  07/30/91 MJRoberts     - add debug active/inactive visual cue
  05/23/91 MJRoberts     - add user exit reader
  04/08/91 MJRoberts     - add full-screen debugging support
  03/10/91 MJRoberts     - integrate John's qa-scripter mods
  11/27/90 MJRoberts     - use time() not localtime() in os_rand; cast time_t
  11/15/90 MJRoberts     - created (split off from os.c)
*/

#define OSGEN_INIT
# include "os.h"
#undef OSGEN_INIT

#include "osgen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "run.h"

#if defined(TURBO) || defined(DJGPP)
#include "io.h"
#endif    

#include "lib.h"
#include "tio.h"

/* global "plain mode" flag */
int os_f_plain = 0;

#ifdef RUNTIME
# ifdef USE_SCROLLBACK
int osssbmode();
void scrpgup();
void scrpgdn();
void scrlnup();
void scrlndn();
static void ossdosb();

/*
 *   Screen size variables.  The underlying system-specific "oss" code must
 *   initialize these during startup and must keep them up-to-date if the
 *   screen size ever changes.  
 */
int G_oss_screen_width = 80;
int G_oss_screen_height = 24;

# endif /* USE_SCROLLBACK */
#endif /* RUNTIME */

/* forward declare the low-level display routine */
void ossdspn(int y, int x, int color, char *p);

/*
 *   The special character codes for controlling color. 
 */

/* 
 *   Set text attributes: the next byte has the new text attributes value,
 *   with 1 added to it to ensure it's never zero.  (A zero in the buffer has
 *   a special meaning, so we want to ensure we never have an incidental
 *   zero.  Zero happens to be a valid attribute value, though, so we have to
 *   encode attributes to avoid this possibility.  Our simple "plus one"
 *   encoding ensures we satisfy the never-equals-zero rule.)  
 */
#define OSGEN_ATTR            1

/* 
 *   explicit colored text: this is followed by two bytes giving the
 *   foreground and background colors as OSGEN_COLOR_xxx codes 
 */
#define OSGEN_COLOR           2


/*
 *   If this port is to use the default saved file overwrite check, define
 *   USE_OVWCHK.  This routine tries to open the file; if successful, the
 *   file is closed and we ask the user if they're sure they want to overwrite
 *   the file.
 */
#ifdef USE_OVWCHK
int os_chkovw(char *filename)
{
    FILE *fp;
    
    if ((fp = fopen( filename, "r" )) != 0)
    {
        char buf[128];
        
        fclose(fp);
        os_printz("That file already exists.  Overwrite it? (y/n) >");
        os_gets((uchar *)buf, sizeof(buf));
        if (buf[0] != 'y' && buf[0] != 'Y')
            return 1;
    }
    return 0;
}
#endif /* USE_OVWCHK */

/* 
 *   non-stop mode does nothing in character-mode implementations, since the
 *   portable console layer handles MORE mode 
 */
void os_nonstop_mode(int flag)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Ports can implement os_flush and os_gets as calls to the stdio routines
 *   of the same name, and os_printz and os_print using the stdio routine
 *   printf, by defining USE_STDIO.  These definitions can be used for any
 *   port for which the standard C run-time library is available.  
 */

#ifdef USE_STDIO

/*
 *   print a null-terminated string the console 
 */
void os_printz(const char *str)
{
    /* write the string to stdout */
    fputs(str, stdout);
}

/*
 *   print a counted-length string, which isn't necessarily null-terminated 
 */
void os_print(const char *str, size_t len)
{
    /* write the string to stdout, limiting the length */
    printf("%.*s", (int)len, str);
}

/*
 *   os_flush forces output of anything buffered for standard output.  It
 *   is generally used prior to waiting for a key (so the normal flushing
 *   may not occur, as it does when asking for a line of input).  
 */
void os_flush(void)
{
    fflush( stdout );
}

/* 
 *   update the display - since we're using text mode, there's nothing we
 *   need to do 
 */
void os_update_display(void)
{
}

/*
 *   os_gets performs the same function as gets().  It should get a
 *   string from the keyboard, echoing it and allowing any editing
 *   appropriate to the system, and return the null-terminated string as
 *   the function's value.  The closing newline should NOT be included in
 *   the string.  
 */
uchar *os_gets(uchar *s, size_t bufl)
{
    return((uchar *)fgets((char *)s, bufl, stdin));
}

/*
 *   The default stdio implementation does not support reading a line of
 *   text with timeout.  
 */
int os_gets_timeout(unsigned char *buf, size_t bufl,
                    unsigned long timeout, int resume_editing)
{
    /* tell the caller this operation is not supported */
    return OS_EVT_NOTIMEOUT;
}

/* 
 *   since we don't support os_gets_timeout(), we don't need to do anything
 *   in the cancel routine
 */
void os_gets_cancel(int reset)
{
    /* os_gets_timeout doesn't do anything, so neither do we */
}

/*
 *   Get an event - stdio version.  This version does not accept a timeout
 *   value, and can only get a keystroke.  
 */
int os_get_event(unsigned long timeout, int use_timeout,
                 os_event_info_t *info)
{
    /* if there's a timeout, return an error indicating we don't allow it */
    if (use_timeout)
        return OS_EVT_NOTIMEOUT;

    /* get a key the normal way */
    info->key[0] = os_getc();

    /* if it's an extended key, get the other key */
    if (info->key[0] == 0)
    {
        /* get the extended key code */
        info->key[1] = os_getc();

        /* if it's EOF, return an EOF event rather than a key event */
        if (info->key[1] == CMD_EOF)
            return OS_EVT_EOF;
    }

    /* return the keyboard event */
    return OS_EVT_KEY;
}

#endif /* USE_STDIO */

/******************************************************************************
* Ports without any special initialization/termination requirements can define
* USE_NULLINIT to pick up the default definitions below.  These do nothing, so
* ports requiring special handling at startup and/or shutdown time must define
* their own versions of these routines.
******************************************************************************/

#ifdef USE_NULLINIT
/* os_init returns 0 for success, 1 for failure.  The arguments are &argc, the
*  address of the count of arguments to the program, and argv, the address of
*  an array of up to 10 pointers to those arguments.  For systems which don't
*  pass a standard command line (such as the Mac Finder), the arguments should
*  be read here using some alternate mechanism (an alert box, for instance),
*  and the values of argc and argv[] updated accordingly.  Note that a maximum
*  of 10 arguments are allowed to be stored in the argv[] array.  The command
*  line itself can be stored in buf, which is a buffer passed by the caller
*  guaranteed to be bufsiz bytes long.
*
*  Unix conventions are followed, so argc is 1 when no arguments are present.
*  The final argument is a prompt string which can be used to ask the user for
*  a command line; its use is not required, but may be desirable for producing
*  a relevant prompt message.  See the Mac implementation for a detailed
*  example of how this mechanism is used.
*/
int os_init(int *argc, char *argv[], const char *prompt,
            char *buf, int bufsiz)
{
    return 0;
}

/*
 *   uninitialize 
 */
void os_uninit(void)
{
}

/* 
 *   os_term should perform any necessary cleaning up, then terminate the
 *   program.  The int argument is a return code to be passed to the
 *   caller, generally 0 for success and other for failure.  
 */
void os_term(int rc)
{
    exit(rc);
}
#endif /* USE_NULLINIT */

/* ------------------------------------------------------------------------ */
/* 
 *   Ports can define USE_NULLPAUSE if no pause is required on exit.
 *   
 *   Ports needing an exit pause, and can simply print a message (with
 *   os_print) and wait for a key (with os_getc) can define USE_EXPAUSE.  
 */

#ifdef USE_NULLPAUSE
void os_expause(void)
{
    /* does nothing */
}
#endif /* USE_NULLPAUSE */

#ifdef USE_EXPAUSE
void os_expause(void)
{
    os_printz("(Strike any key to exit...)");
    os_flush();
    os_waitc();
}
#endif /* USE_EXPAUSE */


#ifdef USE_NULLSTAT
/*
 *   USE_NULLSTAT defines a do-nothing version of os_status.
 */
void os_status(int stat)
{
    /* ignore the new status */
}

int os_get_status()
{
    return 0;
}
#endif /* USE_NULLSTAT */

#ifdef USE_NULLSCORE
/*
 *   USE_NULLSCORE defines a do-nothing version of os_score.
 */
void os_score(int cur, int turncount)
{
    /* ignore the score information */
}

void os_strsc(const char *p)
{
    /* ignore */
}
#endif /* USE_NULLSCORE */

#ifdef USE_STATLINE

/* saved main text area column when drawing in status line */
static osfar_t int S_statline_savecol;

/* saved text column when we're drawing in status line */
static osfar_t int text_lastcol;

/* 
 *   first line of text area - the status line is always one line high, so
 *   this is always simply 1 
 */
static osfar_t int text_line = 1;

/* the column in the status line for the score part of the display */
static osfar_t int score_column = 0;

/*
 *   Define some macros in terms of "oss" globals that tell us the size of
 *   the screen.  
 *   
 *   max_line is the maximum row number of the text area, as a zero-based row
 *   number.  The text area goes from the second row (row 1) to the bottom of
 *   the screen, so this is simply the screen height minus one.
 *   
 *   max_column is the maximum column number of the text area, as a
 *   zero-based column number.  The text area goes from the first column
 *   (column 0) to the rightmost column, so this is simply the screen width
 *   minus one.
 *   
 *   text_line and text_column are the starting row and column number of the
 *   text area.  These are always row 1, column 0.
 *   
 *   sdesc_line and sdesc_column are the starting row and column of the
 *   status line.  These are always row 0, column 0.
 */
#define max_line ((G_oss_screen_height) - 1)
#define max_column ((G_oss_screen_width) - 1)
#define text_line 1
#define text_column 0
#define sdesc_line 0
#define sdesc_column 0

/*
 *   Status line buffer.  Each time we display text to the status line,
 *   we'll keep track of the text here.  This allows us to refresh the
 *   status line whenever we overwrite it (during scrollback mode, for
 *   example).  
 */
static osfar_t char S_statbuf[OS_MAXWIDTH + 1];

/* pointer to the next free character of the status line buffer */
static osfar_t char *S_statptr = S_statbuf;


/*
 *   The special run-time version displays a status line and the ldesc before
 *   each command line is read.  To accomplish these functions, we need to
 *   redefine os_gets and os_print with our own special functions.
 *   
 *   Note that os_init may have to modify some of these values to suit, and
 *   should set up the screen appropriately.  
 */
void os_status(int stat)
{
    /* if we're leaving the status line, restore the old main text column */
    if (stat != status_mode && (status_mode == 1 || status_mode == 2))
    {
        /* 
         *   we're leaving status-line (or post-status-line) mode - restore
         *   the main text column that was in effect before we drew the
         *   status line 
         */
        text_lastcol = S_statline_savecol;
    }

    /* switch to the new mode */
    status_mode = stat;

    /* check the mode */
    if (status_mode == 1)
    {
        /* 
         *   we're entering the status line - start writing at the start
         *   of the status line 
         */
        S_statptr = S_statbuf;

        /* 
         *   clear out any previously-saved status line text, since we're
         *   starting a new status line 
         */
        *S_statptr = '\0';

        /* 
         *   remember the current text area display column so that we can
         *   restore it when we finish with the status line 
         */
        S_statline_savecol = text_lastcol;
    }
    else if (status_mode == 2)
    {
        /* 
         *   entering post-status-line mode - remember the text column so
         *   that we can restore it when we return to normal mode 
         */
        S_statline_savecol = text_lastcol;
    }
}

int os_get_status()
{
    return status_mode;
}

/* Set score to a string value provided by the caller */
void os_strsc(const char *p)
{
    static osfar_t char lastbuf[135];
    int  i;
    int  x;

    /* ignore score strings in plain mode, there's no status line */
    if (os_f_plain)
        return;

    /* presume the score column will be ten spaces left of the right edge */
    score_column = max_column - 10;

    /* start out in the initial score column */
    x = score_column;

    /* 
     *   if we have a string, save the new value; if the string pointer is
     *   null, it means that we should redraw the string with the previous
     *   value 
     */
    if (p != 0)
        strcpy(lastbuf, p);
    else
        p = lastbuf;

    /* display enough spaces to right-justify the value */
    for (i = strlen(p) ; i + score_column <= max_column ; ++i)
        ossdsp(sdesc_line, x++, sdesc_color, " ");
    if (x + strlen(p) > (size_t)max_column)
        score_column = x = max_column - strlen(p);
    ossdsp(sdesc_line, x, sdesc_color, p);
    ossdsp(sdesc_line, max_column, sdesc_color, " ");
}

/*
 *   Set the score.  If cur == -1, the LAST score set with a non-(-1)
 *   cur is displayed; this is used to refresh the status line without
 *   providing a new score (for example, after exiting scrollback mode).
 *   Otherwise, the given current score (cur) and turncount are displayed,
 *   and saved in case cur==-1 on the next call.
 */
void os_score(int cur, int turncount)
{
    char buf[20];

    /* check for the special -1 turn count */
    if (turncount == -1)
    {
        /* it's turn "-1" - we're simply redrawing the score */
        os_strsc((char *)0);
    }
    else
    {
        /* format the score */
        sprintf(buf, "%d/%d", cur, turncount);

        /* display the score string */
        os_strsc(buf);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Scrollback 
 */
# ifdef USE_SCROLLBACK

/*
 *   Pointers for scrollback buffers.
 *   
 *   We store the text that has been displayed to the screen in a fixed
 *   buffer.  We allocate out of the buffer circularly; when we reach the end
 *   of the buffer, we wrap around and allocate out of the beginning of the
 *   buffer, freeing old records as necessary to make space.
 *   
 *   'scrbuf' is a pointer to the buffer.  'scrbuf_free' is a pointer to the
 *   next byte of the buffer available to be allocated.  
 */
static osfar_t char *scrbuf;
static osfar_t unsigned scrbufl;

/* 
 *   Head/tail of linked list of scrollback lines.  Lines are separated by
 *   null bytes.  Note that 'scrbuf_tail' is a pointer to the START of the
 *   last line in the buffer (which is usually the line still under
 *   construction).  
 */
static char *scrbuf_head;
static char *scrbuf_tail;

/* allocation head */
static osfar_t char *scrbuf_free;

/* number of lines in scrollback buffer */
static osfar_t int os_line_count;

/* line number of top line on the screen */
static osfar_t int os_top_line;

/* current column in the scrollback buffer */
static osfar_t int sb_column;

/* 
 *   Current screen color - this is the color to use for the blank areas of
 *   the screen.  We use this to clear areas of the screen, to fill in blank
 *   areas left over after scrolling, and as the default background color to
 *   display for characters with a "transparent" background.
 *   
 *   Note that osssb_screen_color is an OSGEN_COLOR_xxx value, and
 *   osssb_oss_screen_color is the corresponding ossxxx color code for a
 *   character with that background color.  We keep both values for
 *   efficiency.  
 */
static osfar_t int osssb_screen_color;
static osfar_t int osssb_oss_screen_color;

/* current color/attribute setting */
static osfar_t int osssb_cur_fg;
static osfar_t int osssb_cur_bg;
static osfar_t int osssb_cur_attrs;

/* color/attribute settings at start of current scrollback line */
static osfar_t int osssb_sol_fg;
static osfar_t int osssb_sol_bg;
static osfar_t int osssb_sol_attrs;

/*
 *   Receive notification of a screen size change. 
 */
void osssb_on_resize_screen()
{
    /* 
     *   Note the page length of the main text area - this is the distance
     *   between 'more' prompts.  Use the screen height, minus one for the
     *   status line, minus two extra lines so we keep a line of context at
     *   the top of the screen while leaving a line for a "more" prompt at
     *   the bottom.  
     */
    G_os_pagelength = (G_oss_screen_height > 3
                       ? G_oss_screen_height - 3
                       : G_oss_screen_height);

    /* the main text area gets the full screen width */
    G_os_linewidth = G_oss_screen_width;
}

/*
 *   Initialize the scrollback buffer - allocates memory for the buffer.
 *   
 *   Important: when this routine is called, the text_color global variable
 *   MUST be initialized to the ossdsp-style color code to use for the
 *   default text area.
 */
void osssbini(unsigned int size)
{
    /* calculate the page size and width based on the screen size */
    osssb_on_resize_screen();

    /* remember the size of the buffer */
    scrbufl = size;

    /* allocate the scrollback buffer */
    if ((scrbuf = calloc(scrbufl, 1)) != 0)
    {
        /* set up the first and only (and thus last) line pointers */
        scrbuf_head = scrbuf_tail = scrbuf;

        /* set up the free pointer */
        scrbuf_free = scrbuf;

        /* we have one line in the buffer now */
        os_line_count = 1;

        /* start out in normal text color */
        osssb_cur_fg = OSGEN_COLOR_TEXT;
        osssb_cur_bg = OSGEN_COLOR_TRANSPARENT;
        osssb_cur_attrs = 0;

        /* the start of the scrollback buffer line is the same */
        osssb_sol_fg = OSGEN_COLOR_TEXT;
        osssb_sol_bg = OSGEN_COLOR_TRANSPARENT;
        osssb_sol_attrs = 0;

        /* set the base screen color to the normal text background color */
        osssb_screen_color = OSGEN_COLOR_TEXTBG;
        osssb_oss_screen_color = ossgetcolor(OSGEN_COLOR_TEXT,
                                             OSGEN_COLOR_TEXTBG, 0, 0);
    }
    else
    {
        /* mention the problem */
        os_printz("\nSorry, there is not enough memory available "
                  "for review mode.\n\n");

        /* we have no line records */
        scrbuf_head = 0;
        scrbuf_tail = 0;
    }

    /* clear the status line area */
    ossclr(sdesc_line, sdesc_column, sdesc_line, max_column, sdesc_color);
}

/*
 *   delete the scrollback buffer 
 */
void osssbdel(void)
{
    /* free the screen buffer */
    if (scrbuf != 0)
        free(scrbuf);
}

/*
 *   advance to the next byte of the scrollback buffer 
 */
static char *ossadvsp(char *p)
{
    /* move to the next byte */
    ++p;

    /* if we've passed the end of the buffer, wrap to the beginning */
    if (p >= scrbuf + scrbufl)
        p = scrbuf;

    /* return the pointer */
    return p;
}

/*
 *   decrement to the previous byte of the buffer 
 */
static char *ossdecsp(char *p)
{
    /* if we're at the start of the buffer, wrap to just past the end */
    if (p == scrbuf)
        p = scrbuf + scrbufl;

    /* move to the previous byte */
    --p;

    /* return the pointer */
    return p;
}

/*
 *   Add a byte to the scrollback buffer 
 */
static void osssb_add_byte(char c)
{
    /* add the character to the buffer */
    *scrbuf_free = c;

    /* advance the free pointer */
    scrbuf_free = ossadvsp(scrbuf_free);

    /* 
     *   if the free pointer has just collided with the start of the oldest
     *   line, delete the oldest line 
     */
    if (scrbuf_free == scrbuf_head)
    {
        /* 
         *   delete the oldest line to make room for the new data, by
         *   advancing the head pointer to just after the next null byte 
         */
        while (*scrbuf_head != '\0')
            scrbuf_head = ossadvsp(scrbuf_head);

        /* skip the null pointer that marks the end of the old first line */
        scrbuf_head = ossadvsp(scrbuf_head);

        /* note the loss of a line */
        --os_line_count;
    }
}

/*
 *   Add an appropriate color code to the scrollback buffer to yield the
 *   current color setting. 
 */
static void osssb_add_color_code()
{
    /* if we have any attributes, add an attribute code */
    if (osssb_cur_attrs != 0)
    {
        /* 
         *   add the attribute, encoded with our plus-one rule (to avoid
         *   storing zero bytes as attribute arguments) 
         */
        osssb_add_byte(OSGEN_ATTR);
        osssb_add_byte((char)(osssb_cur_attrs + 1));
    }

    /* if we're using the plain text color, add a color code */
    if (osssb_cur_fg != OSGEN_COLOR_TEXT
        || osssb_cur_bg != OSGEN_COLOR_TRANSPARENT)
    {
        /* add the explicit color code */
        osssb_add_byte(OSGEN_COLOR);
        osssb_add_byte((char)osssb_cur_fg);
        osssb_add_byte((char)osssb_cur_bg);
    }
}

/*
 *   Scan a character, counting its contribution to the display column in the
 *   scrollback. 
 */
static void osssb_scan_char(char **p, int *col)
{
    /* see what we have */
    switch(**p)
    {
    case '\n':
    case '\r':
    case '\0':
        /* 
         *   these are all one-byte character sequences that don't contribute
         *   to the display column - simply skip the input byte 
         */
        ++(*p);
        break;

    case OSGEN_ATTR:
        /* this is a two-byte non-printing escape sequence */
        *p += 2;
        break;

    case OSGEN_COLOR:
        /* this is a three-byte non-printing escape sequence */
        *p += 3;
        break;

    default:
        /* 
         *   anything else is a one-byte display character, so count its
         *   contribution to the display column and skip the byte 
         */
        ++(*col);
        ++(*p);
        break;
    }
}

/*
 *   Start a new line in the scrollback buffer 
 */
static void osssb_new_line()
{
    /* add a null byte to mark the end of the current line */
    osssb_add_byte(0);

    /* remember the new final line - it starts at the free pointer */
    scrbuf_tail = scrbuf_free;

    /* count the added line */
    ++os_line_count;

    /* the new line starts at the first column */
    sb_column = 0;

    /* add a color code to the start of the new line, if necessary */
    osssb_add_color_code();

    /* remember the text color at the start of this line */
    osssb_sol_fg = osssb_cur_fg;
    osssb_sol_bg = osssb_cur_bg;
    osssb_sol_attrs = osssb_cur_attrs;
}

/*
 *   Add text to the scrollback buffer.  Returns the number of lines that the
 *   added text spans.  
 */
static int ossaddsb(const char *p, size_t len)
{
    int line_cnt;
    
    /* if there's no scrollback buffer, ignore it */
    if (scrbuf == 0)
        return 0;

    /* presume the text will all fit on one line */
    line_cnt = 1;

    /*
     *   Copy the text into the screen buffer, respecting the circular nature
     *   of the screen buffer.  If the given text wraps lines, enter an
     *   explicit carriage return into the text to ensure that users can
     *   correctly count lines.  
     */
    while(len != 0)
    {
        /* check for color control codes */
        switch(*p)
        {
        case OSGEN_ATTR:
            /* 
             *   switch to the new attributes (decoding from the plus-one
             *   storage code) 
             */
            osssb_cur_attrs = ((unsigned char)*(p+1)) - 1;
            break;

        case OSGEN_COLOR:
            /* switch to the explicit colors */
            osssb_cur_fg = (unsigned char)*(p+1);
            osssb_cur_bg = (unsigned char)*(p+2);
            break;
        }

        /* check for special characters */
        switch(*p)
        {
        case '\n':
            /* add the new line */
            osssb_new_line();

            /* skip this character */
            ++p;
            --len;

            /* count the new line */
            ++line_cnt;

            /* done */
            break;

        case '\r':
            /*
             *   We have a plain carriage return, which indicates that we
             *   should go back to the start of the current line and
             *   overwrite it.  (This is most likely to occur for the
             *   "[More]" prompt.)  Go back to the first column.  
             */
            sb_column = 0;

            /* set the free pointer back to the start of the current line */
            scrbuf_free = scrbuf_tail;

            /* switch back to the color as of the start of the line */
            osssb_cur_fg = osssb_sol_fg;
            osssb_cur_bg = osssb_sol_bg;
            osssb_cur_attrs = osssb_sol_attrs;

            /* 
             *   since we're back at the start of the line, add a color code
             *   if necessary 
             */
            osssb_add_color_code();

            /* skip this character */
            ++p;
            --len;

            /* done */
            break;

        case OSGEN_ATTR:
            /* it's a two-byte attribute sequence - add the bytes */
            osssb_add_byte(*p);
            osssb_add_byte(*(p+1));

            /* skip the sequence */
            p += 2;
            len -= 2;

            /* done (since this doesn't print) */
            break;

        case OSGEN_COLOR:
            /* it's a three-byte color sequence - add the bytes */
            osssb_add_byte(*p);
            osssb_add_byte(*(p+1));
            osssb_add_byte(*(p+2));

            /* skip the sequence */
            p += 3;
            len -= 3;

            /* 
             *   done (this doesn't print, so it has no effect on the column
             *   position or wrapping) 
             */
            break;
            
        default:
            /* for anything else, simply store the byte in the buffer */
            osssb_add_byte(*p);

            /* skip the character */
            ++p;
            --len;

            /* advance to the next column */
            ++sb_column;

            /* check for wrapping */
            if (sb_column > max_column)
            {
                /* start a new line */
                osssb_new_line();

                /* count the new line */
                ++line_cnt;
            }

            /* done */
            break;
        }
    }

    /* 
     *   Always make sure we have a null terminator after each addition, in
     *   case we need to look at the buffer before this line is finished.
     *   However, since we're just tentatively writing the null terminator,
     *   back up the free pointer to point to it after we write it, so that
     *   the next character we write will go here.  
     */
    osssb_add_byte(0);
    scrbuf_free = ossdecsp(scrbuf_free);

    /* return the number of lines we added */
    return line_cnt;
}

/*
 *   A slight complication: if we're not on the very bottom line, and we
 *   don't even have any text displayed on the bottom line (example: the
 *   input line grew to over 80 characters, scrolling up a line, but shrunk
 *   again due to deletions), we must add blank lines to fill out the unused
 *   space, so that the scrollback line counter doesn't get confused (it
 *   counts backwards from the bottom of the screen, where it assumes we are
 *   right now).  Figure out if the current buffer will go to the end of the
 *   screen.
 *   
 *   This routine adds the given text to the screen buffer, then adds as many
 *   blank lines as are necessary to fill the screen to the bottom line.  buf
 *   is the buffer to add; p is a pointer into the buffer corresponding to
 *   screen position (x,y).  
 */
static void ossaddsbe(char *buf, char *p, int x, int y)
{
    int extra_lines;
    int actual_lines;

    /* 
     *   compute the number of blank lines we need to add - we need one blank
     *   line for each line on the screen beyond the display position 
     */
    extra_lines = max_line - y;

    /* add the text */
    actual_lines = ossaddsb(buf, strlen(buf));

    /* 
     *   if we added more than one line already, those count against the
     *   extra lines we need to add 
     */
    extra_lines -= (actual_lines - 1);

    /* add the extra blank lines */
    for ( ; extra_lines > 0 ; --extra_lines)
        osssb_new_line();

    /* add a tentative null terminator */
    osssb_add_byte(0);
    scrbuf_free = ossdecsp(scrbuf_free);
}

/*
 *   Scrollback mode variables.  These track the current position while
 *   in scrollback mode.
 */
char osfar_t *scrtop;                 /* first line displayed on the screen */
static osfar_t char *scrbot;                /* last line displayed on sreen */
static osfar_t char *scrlast;               /* first line of last screenful */

/*
 *   scrdsp  - display a line of text from the scrollback buffer on the
 *   screen.  It is the responsibility of the caller to ensure that the
 *   line on the screen has been cleared beforehand.
 */
static void scrdsp(char *p, int y)
{
    char  buf[127];
    char *q = buf;
    int   x = text_column;
    int   fg;
    int   bg;
    int   attrs;
    int   oss_color;

    /* start out in normal text color */
    fg = OSGEN_COLOR_TEXT;
    bg = OSGEN_COLOR_TRANSPARENT;
    attrs = 0;
    oss_color = ossgetcolor(fg, bg, attrs, osssb_screen_color);

    /* keep going until we reach the end of the line */
    while (*p != '\0')
    {
        /* check the character */
        switch(*p)
        {
        case OSGEN_ATTR:
            /* set the new attributes (decoding from the plus-one value) */
            attrs = (unsigned char)*(p = ossadvsp(p)) - 1;

            /* handle it via the common color-switching code */
            goto change_color;

        case OSGEN_COLOR:
            /* explicit color code - read the two extra bytes */
            fg = *(p = ossadvsp(p));
            bg = *(p = ossadvsp(p));

            /* handle it via the common color-switching code */
            goto change_color;

        change_color:
            /* null-terminate the buffer and display it in the old color */
            *q = '\0';
            ossdsp(y, x, oss_color, buf);

            /* switch to the new color */
            oss_color = ossgetcolor(fg, bg, attrs, osssb_screen_color);

            /* advance the column counter */
            x += strlen(buf);

            /* go back to start of buffer */
            q = buf;
            break;
            
        default:
            /* add this byte to our buffer */
            *q++ = *p;
            break;
        }
        
        /* advance to next character no matter what happened */
        p = ossadvsp(p);

        /* if the buffer's full, flush it */
        if (q == buf + sizeof(buf) - 1)
        {
            /* flush out the buffer */
            *q = '\0';
            ossdsp(y, x, oss_color, buf);

            /* advance the column counter */
            x += strlen(buf);

            /* go back to start of buffer */
            q = buf;
        }
    }

    /* flush out what's left in the buffer */
    *q = '\0';
    ossdsp(y, x, oss_color, buf);
}

/*
 *   scrdspscr - display a screenful of text starting at a given location
 *   in the scrollback buffer.
 */
static void scrdspscr(char *p)
{
    int y;

    /* clear the screen area of our display */
    ossclr(text_line, text_column, max_line, max_column,
           osssb_oss_screen_color);

    /* display each line of this area */
    for (y = text_line ; y <= max_line ; ++y)
    {
        /* display this line */
        scrdsp(p, y);

        /* if we just displayed the last line, we're done */
        if (p == scrbuf_tail)
            break;

        /* advance to the next line */
        while (*p != '\0')
            p = ossadvsp(p);

        /* skip the line separator */
        p = ossadvsp(p);
    }
}

/*
 *   Advance a buffer pointer until it's pointing to the start of the next
 *   line.  Returns true if we advanced the pointer, false if we were already
 *   at the end of the buffer.  
 */
static int osssb_next_line(char **p)
{
    /* if we're at the last line already, there's nothing to do */
    if (*p == scrbuf_tail)
        return FALSE;

    /* advance until we find the current line's terminating null byte */
    while (**p != '\0')
        *p = ossadvsp(*p);

    /* skip the null byte */
    *p = ossadvsp(*p);

    /* we did advance by a line */
    return TRUE;
}

/*
 *   Move a buffer pointer backward to the start of the current line. 
 */
static char *osssb_to_start(char *p)
{
    /* keep going as long as we don't reach the start of the first line */
    while (p != scrbuf_head)
    {
        /* move back a byte */
        p = ossdecsp(p);

        /* if it's a null byte, we've found the end of the previous line */
        if (*p == '\0')
            return ossadvsp(p);
    }

    /* 
     *   return the pointer (which is at the start of the first line if we
     *   got here) 
     */
    return p;
}

/*
 *   Move a buffer pointer backward to the start of the previous line.
 *   Returns true if we moved the pointer, false if we were already in the
 *   first line.  
 */
static int osssb_prev_line(char **p)
{
    /* first, make sure we're at the start of the current line */
    *p = osssb_to_start(*p);

    /* if we're at the start of the first line, we can't go back */
    if (*p == scrbuf_head)
        return FALSE;

    /* move back to the null byte at the end of the previous line */
    *p = ossdecsp(*p);

    /* go back to the start of this line */
    *p = osssb_to_start(*p);

    /* we moved the pointer */
    return TRUE;
}

/* 
 *   move forward by a number of lines, and redisplays the sreen 
 */
static void scrfwd(int n)
{
    /* if we're already on the last screen, there's nothing to do */
    if (scrtop == scrlast)
        return;

    /* 
     *   Move forward by the requested amount.  Stop if we reach the top of
     *   the bottom-most screen.  
     */
    while (n != 0)
    {
        /* move to the next line at the bottom */
        if (!osssb_next_line(&scrbot))
            break;

        /* move to the next line at the top as well */
        osssb_next_line(&scrtop);

        /* update our display counter */
        ++os_top_line;

        /* that's one less line to advance by */
        --n;

    }

    /* re-display the screen with the new top line */
    scrdspscr(scrtop);
}

/* 
 *   move back by a number of lines, redisplaying the screen 
 */
static void scrback(int n)
{
    /* if we can't go back any further, we're done */
    if (scrtop == scrbuf_head)
        return;

    /* keep going until we satisfy the request */
    while (n != 0 && scrtop != scrbuf_head)
    {
        /* 
         *   back up one line - if we can't (because we're already at the
         *   first line), stop trying 
         */
        if (!osssb_prev_line(&scrtop))
            break;

        /* back up one at the bottom */
        osssb_prev_line(&scrbot);
        
        /* that's one less to look for */
        --n;

        /* adjust our line position */
        --os_top_line;
    }

    /* redisplay the screen */
    scrdspscr(scrtop); 
}

/*
 *   scrto moves to a selected line
 */
void scrto(int lin)
{
    if (lin > os_top_line)
        scrfwd(lin - os_top_line);
    else if (lin < os_top_line)
        scrback(os_top_line - lin);
}

/*
 *   scrpgup scrolls back a page
 */
void scrpgup(void)
{
    scrback(max_line - text_line);
}

/*
 *   scrpgdn scrolls forward a page
 */
void scrpgdn(void)
{
    scrfwd(max_line - text_line);
}

/*
 *   scrlnup scrolls up one line
 */
void scrlnup(void)
{
    /* move the top pointer back a line */
    if (osssb_prev_line(&scrtop))
    {
        /* success - move the bottom pointer back a line as well */
        osssb_prev_line(&scrbot);

        /* adjust our line counter */
        --os_top_line;

        /* scroll the display area */
        ossscu(text_line, text_column, max_line, max_column,
               osssb_oss_screen_color);

        /* redisplay the top line */
        scrdsp(scrtop, text_line);
    }
}

/*
 *   scrlndn scrolls down one line
 */
void scrlndn(void)
{
    /* if we're already at the last screenful, there's nothing to do */
    if (scrtop == scrlast)
        return;

    /* move the bottom pointer up a line */
    if (osssb_next_line(&scrbot))
    {
        /* success - move the top pointer up a line as well */
        osssb_next_line(&scrtop);

        /* adjust our line counter */
        ++os_top_line;

        /* scroll the display area */
        ossscr(text_line, text_column, max_line, max_column,
               osssb_oss_screen_color);

        /* redisplay the bottom line */
        scrdsp(scrbot, max_line);
    }
}

/* redraw the screen's text region */
void os_redraw(void)
{
    int y;
    char *p;

    /* clear the area */
    ossclr(text_line, text_column, max_line, max_column,
           osssb_oss_screen_color);

    /* find the top line in the display area */
    p = scrbuf_tail;
    for (y = max_line ; y >= text_line ; --y)
    {
        /* display the current line at the current position */
        scrdsp(p, y);

        /* move back a line if possible */
        if (!osssb_prev_line(&p))
            break;
    }
}

/*
 *   osssbmode toggles scrollback mode.  Once in scrollback mode, calls
 *   can be made to osssbup, osssbdn, osssbpgup, and osspgdn to scroll
 *   through the saved text.  We also put up a scrollback-mode version of
 *   the status line, showing keys that should be used for scrollback.
 *   When the user wants to exit scrollback mode, this routine should
 *   be called again.
 *
 *   Returns:  0 if scrollback mode is entered/exited successfully, 1 if
 *   an error occurs.  Note that it may not be possible to enter scrollback
 *   mode, since insufficient saved text may have been accumulated.
 */
int osssbmode(int mode_line)
{
    /* if there's no buffer, we can't enter scrollback mode */
    if (scrbuf == 0)
        return 1;

    /* 
     *   if we're not in scrollback mode, enter scrollback mode; otherwise,
     *   return to normal mode 
     */
    if (scrtop == 0)
    {
        int y;
        int i;
        char buf[135];
        
        /*
         *   Enter scrollback mode.  Figure out what scrtop should be, and
         *   put up the scrollback status line.  If insufficient saved text
         *   is around for scrollback mode, return 1.  
         */
        for (os_top_line = os_line_count, scrtop = scrbuf_tail,
             y = max_line ; y > text_line ; --y, --os_top_line)
        {
            /* back up one line */
            if (!osssb_prev_line(&scrtop))
            {
                /* there wasn't enough text, so abort and return failure */
                scrtop = 0;
                return 1;
            }
        }

        /* 
         *   if the top of the screen is the very first line in the buffer,
         *   there's still not enough scrollback information 
         */
        if (scrtop == scrbuf_head)
        {
            /* no point in scrollback when we have exactly one screen */
            scrtop = 0;
            return 1;
        }

        /* remember where current screen starts and ends */
        scrlast = scrtop;
        scrbot = scrbuf_tail;

        /* display instructions if we have a status line */
        if (mode_line)
        {
            strcpy(buf, OS_SBSTAT);
            for (i = strlen(buf) ; i < max_column ; buf[i++] = ' ') ;
            buf[i] = '\0';
            ossdsp(sdesc_line, sdesc_column+1, sdesc_color, buf);
        }

        /* successfully entered scrollback mode */
        return 0;
    }
    else
    {
        /*
         *   Exit scrollback mode.  Show the last page of text, and put
         *   up the normal status bar.  Also clear our scrollback mode
         *   variables.
         */
        if (scrlast != scrtop)
            scrdspscr(scrlast);

        /* re-display the status line, if appropriate */
        if (mode_line)
        {
            /* display the last status line buffer */
            ossdspn(sdesc_line, sdesc_column, sdesc_color, " ");
            ossdspn(sdesc_line, sdesc_column + 1, sdesc_color, S_statbuf);

            /* 
             *   refresh the right-hand side with the score part - do this
             *   by drawing the score with a special turn counter of -1 to
             *   indicate that we just want to refresh the previous score
             *   value 
             */
            os_score(-1, -1);
        }
        scrtop = 0;
        return( 0 );
    }
}

/*
 *   ossdosb runs a scrollback session.  When entered, osssbmode()
 *   must already have been called.  When we're done, we'll be out
 *   of scrollback mode.
 */
static void ossdosb(void)
{
    for ( ;; )
    {
        if (!os_getc())
        {
            switch(os_getc())
            {
            case CMD_SCR:
            case CMD_KILL:         /* leave scrollback via 'escape' as well */
            case CMD_EOF:                            /* stop on end of file */
                osssbmode(1);
                return;
            case CMD_UP:
                scrlnup();
                break;
            case CMD_DOWN:
                scrlndn();
                break;
            case CMD_PGUP:
                scrpgup();
                break;
            case CMD_PGDN:
                scrpgdn();
                break;
            }
        }
    }
}

# else /* USE_SCROLLBACK */
static void ossaddsb(char *p, size_t len)
{
    /* We're not saving output - do nothing */
}
# endif /* USE_SCROLLBACK */

/* display with no highlighting - intercept and ignore highlight codes */
void ossdspn(int y, int x, int color, char *p)
{
    char *q;
    int esc_len;

    /* scan the string */
    for (q = p ; *q ; ++q)
    {
        switch(*q)
        {
        case OSGEN_COLOR:
            /* three-byte escape sequence */
            esc_len = 3;
            goto skip_esc;

        case OSGEN_ATTR:
            /* two-byte escape sequence */
            esc_len = 2;
            goto skip_esc;
            
        skip_esc:
            /* end the buffer here and display what we have so far */
            *q = '\0';
            ossdsp(y, x, color, p);

            /* adjust the column position for the display */
            x += strlen(p);
            
            /* advance past what we've displayed plus the escape sequence */
            p = q + esc_len;
            break;

        default:
            /* ordinary character - keep going */
            break;
        }
    }

    /* display the remainder of the buffer, if there's anything left */
    if (q != p)
        ossdsp(y, x, color, p);
}

/* display with highlighting enabled */
int ossdsph(int y, int x, int color, char *p)
{
    char *q;
    int len;
    int esc_len;

    /* get length of entire string */
    len = strlen(p);

    /* scan the input string */
    for (q = p ; *q != '\0' ; ++q)
    {
        /* check for special escape codes */
        switch(*q)
        {
        case OSGEN_ATTR:
            /* set text attributes (decoding from the plus-one code) */
            osssb_cur_attrs = (unsigned char)*(q+1) - 1;
            esc_len = 2;
            goto change_color;

        case OSGEN_COLOR:
            /* set explicit color */
            osssb_cur_fg = *(q+1);
            osssb_cur_bg = *(q+2);
            esc_len = 3;
            goto change_color;
            
        change_color:
            /* display the part up to the escape code in the old color */
            *q = '\0';
            ossdsp(y, x, text_color, p);

            /* adjust the column position for the display */
            x += strlen(p);

            /* note the new text color */
            text_color = ossgetcolor(osssb_cur_fg, osssb_cur_bg,
                                     osssb_cur_attrs, osssb_screen_color);

            /* move past the escape sequence */
            p = q + esc_len;
            q += esc_len - 1;

            /* don't count the escape character in the length displayed */
            len -= esc_len;
            break;

        default:
            break;
        }
    }

    /* display the last portion of the line if anything's left */
    if (q != p)
        ossdsp(y, x, text_color, p);

    /* return the length */
    return len;
}

/*
 *   Set the terminal into 'plain' mode: disables status line,
 *   scrollback, command editing.
 */
void os_plain(void)
{
    /* set the 'plain' mode flag */
    os_f_plain = 1;

    /* 
     *   if we're running without a stdin, turn off pagination - since the
     *   user won't be able to respond to [more] prompts, there's no reason
     *   to show them 
     */
    if (oss_eof_on_stdin())
        G_os_moremode = FALSE;
}

void os_printz(const char *str)
{
    /* use our base counted-length routine */
    os_print(str, strlen(str));
}

void os_print(const char *str, size_t len)
{
    /* 
     *   save the output in the scrollback buffer if we're displaying to
     *   the main text area (status_mode == 0) and we're not in plain
     *   stdio mode (in which case there's no scrollback support) 
     */
    if (status_mode == 0 && !os_f_plain)
        ossaddsb(str, len);

    /* determine what to do based on the status mode */
    switch(status_mode)
    {
    case 2:
        /* we're in the post-status-line mode - suppress all output */
        break; 
        
    case 0:
        /* normal main text area mode */
        {
            const char *p;
            size_t rem;
            char *dst;
            char buf[128];
            
            /* scan the buffer */
            for (p = str, rem = len, dst = buf ; ; ++p, --rem)
            {
                /* 
                 *   if we're out of text, or the buffer is full, or we're at
                 *   a newline or carriage return, flush the buffer 
                 */
                if (rem == 0
                    || dst == buf + sizeof(buf) - 2
                    || *p == '\n' || *p == '\r')
                {
                    /* write out the buffer */
                    if (os_f_plain)
                    {
                        /* write the buffer, INCLUDING the LF/CR */
                        if (*p == '\n' || *p == '\r')
                            *dst++ = *p;
                        *dst = '\0';
                        fputs(buf, stdout);
                    }
                    else
                    {
                        size_t len;

                        /* display the buffer */
                        *dst = '\0';
                        len = ossdsph(max_line, text_lastcol,
                                      text_color, buf);

                        /* move the cursor right */
                        text_lastcol += len;

                        /* scroll on a newline */
                        if (*p == '\n')
                        {
                            /* scroll up a line */
                            ossscr(text_line, text_column,
                                   max_line, max_column,
                                   osssb_oss_screen_color);
                        }

                        /* move the cursor to the left column on LF/CR */
                        if (*p == '\n' || *p == '\r')
                            text_lastcol = text_column;
                    }

                    /* reset the buffer */
                    dst = buf;

                    /* if we're out of source text, we're done */
                    if (rem == 0)
                        break;
                }

                /* see what we have */
                switch(*p)
                {
                case '\n':
                case '\r':
                    /* don't buffer newlines/carriage returns */
                    break;

                default:
                    /* buffer this character */
                    *dst++ = *p;
                    break;
                }
            }

            /* put the cursor at the end of the text */
            if (status_mode == 0 && !os_f_plain)
                ossloc(max_line, text_lastcol);
        }

        /* done */
        break;

    case 1:
        /* status-line mode - ignore in 'plain' mode */
        if (!os_f_plain)
        {
            size_t rem;
            const char *p;
            int i;

            /* 
             *   Skip leading newlines at the start of the statusline output.
             *   Only do this if we don't already have anything buffered,
             *   since a newline after some other text indicates the end of
             *   the status line and thus can't be ignored.  
             */
            p = str;
            rem = len;
            if (S_statptr == S_statbuf)
            {
                /* the buffer is empty, so skip leading newlines */
                for ( ; rem != 0 && *p == '\n' ; ++p, --rem) ;
            }

            /* 
             *   Add this text to our private copy, so that we can refresh
             *   the display later if necessary.  If we reach a newline,
             *   stop.  
             */
            for ( ; (rem != 0 && *p != '\n'
                     && S_statptr < S_statbuf + sizeof(S_statbuf)) ;
                  ++p, --rem)
            {
                /* omit special characters from the status buffer */
                if (*p == OSGEN_ATTR)
                {
                    /* skip an extra byte for the attribute code */
                    ++p;
                    continue;
                }
                else if (*p == OSGEN_COLOR)
                {
                    /* skip two extra bytes for the color codes */
                    p += 2;
                    continue;
                }

                /* copy this character */
                *S_statptr++ = *p;
            }

            /* if we reached a newline, we're done with the status line */
            if (rem != 0 && *p == '\n')
                os_status(2);

            /* add spaces up to the score location */
            for (i = S_statptr - S_statbuf ; i < score_column - 1 ; ++i)
                S_statbuf[i] = ' ';

            /* null-terminate the buffer */
            S_statbuf[i] = '\0';

            /* display a leading space */
            ossdspn(sdesc_line, sdesc_column, sdesc_color, " ");

            /* display the string */
            ossdspn(sdesc_line, sdesc_column + 1, sdesc_color, S_statbuf);
        }
        
        /* done */
        break;
    }
}

void os_flush(void)
{
    /* we don't buffer our output, so there's nothing to do here */
}

void os_update_display(void)
{
    /* there's nothing we need to do */
}

# ifdef USE_HISTORY
/*
 *   For command line history, we must have some buffer space to store
 *   past command lines.  We will use a circular buffer:  when we move
 *   the pointer past the end of the buffer, it wraps back to the start
 *   of the buffer.  A "tail" indicates the oldest line in the buffer;
 *   when we need more room for new text, we advance the tail and thereby
 *   lose the oldest text in the buffer.
 */
static osfar_t unsigned char *histbuf = 0;
static osfar_t unsigned char *histhead = 0;
static osfar_t unsigned char *histtail = 0;

/*
 *   ossadvhp advances a history pointer, and returns the new pointer.
 *   This function takes the circular nature of the buffer into account
 *   by wrapping back to the start of the buffer when it hits the end.
 */
uchar *ossadvhp(uchar *p)
{
    if (++p >= histbuf + HISTBUFSIZE)
        p = histbuf;
    return p;
}

/*
 *   ossdechp decrements a history pointer, wrapping the pointer back
 *   to the top of the buffer when it reaches the bottom.
 */
uchar *ossdechp(uchar *p)
{
    if (p == histbuf)
        p = histbuf + HISTBUFSIZE;
    return p - 1;
}

/*
 *  osshstcpy copies from a history buffer into a contiguous destination
 *  buffer, wrapping the history pointer if need be.  One null-terminated
 *  string is copied.
 */
void osshstcpy(uchar *dst, uchar *hst)
{
    while( *hst )
    {
        *dst++ = *hst;
        hst = ossadvhp( hst );
    }
    *dst = '\0';
}

/*
 *   ossprvcmd returns a pointer to the previous history command, given
 *   a pointer to a history command.  It returns a null pointer if the
 *   given history command is the first in the buffer.
 */
uchar *ossprvcmd(uchar *hst)
{
    if (hst == histtail)
        return 0;                              /* no more previous commands */
    hst = ossdechp( hst );                                  /* back onto nul */
    do
    {
        hst = ossdechp( hst );                        /* back one character */
    } while (*hst && hst != histtail);         /* go until previous command */
    if (*hst == 0)
        hst = ossadvhp(hst);                         /* step over null byte */
    return hst;
}

/*
 *   ossnxtcmd returns a pointer to the next history command, given
 *   a pointer to a history command.  It returns a null pointer if the
 *   given command is already past the last command.
 */
uchar *ossnxtcmd(uchar *hst)
{
    if ( hst == histhead ) return( 0 );         /* past the last one already */
    while( *hst ) hst = ossadvhp( hst );             /* scan forward to null */
    hst = ossadvhp( hst );                /* scan past null onto new command */
    return( hst );
}
# endif /* USE_HISTORY */

/* ------------------------------------------------------------------------ */
/*
 *   Input buffer state.  This information is defined statically because
 *   os_gets_timeout() can carry the information from invocation to
 *   invocation when input editing is interrupted by a tmieout. 
 */
static osfar_t char S_gets_internal_buf[256];       /* internal save buffer */
static osfar_t char *S_gets_buf = S_gets_internal_buf;  /* current save buf */
static osfar_t size_t S_gets_buf_siz = sizeof(S_gets_internal_buf); /* size */
static osfar_t int S_gets_ofs;       /* offset in buffer of insertion point */
static osfar_t unsigned char *S_gets_curhist;    /* current history pointer */
static osfar_t int S_gets_x, S_gets_y;             /* saved cursor position */

# ifdef USE_HISTORY
/* save buffer for line being edited before history recall began */
static osfar_t char S_hist_sav_internal[256]; 
static osfar_t char *S_hist_sav = S_hist_sav_internal;
static osfar_t size_t S_hist_sav_siz = sizeof(S_hist_sav_internal);
# endif /* USE_HISTORY */

/* strcpy with destination buffer size limit */
extern void safe_strcpy(char *dst, size_t dstlen, const char *src);

/*
 *   Flag: input is already in progress.  When os_gets_timeout() returns
 *   with OS_EVT_TIMEOUT, it sets this flag to true.  os_gets_cancel() sets
 *   this flag to false.
 *   
 *   When os_gets_timeout() is called again, it checks this flag to see if
 *   the input session was cancelled; if not, the routine knows that the
 *   partially-edited input line is already displayed where it left off,
 *   because the display has not been modified since the interrupted call to
 *   os_gets_timeout() returned.  
 */
static osfar_t int S_gets_in_progress = FALSE;

/* ------------------------------------------------------------------------ */
/*
 *   Display a string from the given character position.  This does NOT
 *   scroll the screen.  
 */
static void ossdsp_str(int y, int x, int color,
                       unsigned char *str, size_t len)
{
    /* keep going until we exhaust the string */
    while (len != 0)
    {
        size_t cur;
        unsigned char oldc;
        
        /* display as much as will fit on the current line */
        cur = max_column - x + 1;
        if (cur > len)
            cur = len;

        /* null-terminate the chunk, but save the original character */
        oldc = str[cur];
        str[cur] = '\0';

        /* display this chunk */
        ossdsp(y, x, color, (char *)str);

        /* restore the character where we put our null */
        str[cur] = oldc;

        /* move our string counters past this chunk */
        str += cur;
        len -= cur;

        /* advance the x position */
        x += cur;
        if (x > max_column)
        {
            x = 0;
            ++y;
        }
    }
}

/*
 *   Display a string from the given character position, scrolling the
 *   screen if necessary. 
 */
static void ossdsp_str_scr(int *y, int *x, int color,
                           unsigned char *str, size_t len)
{
    /* keep going until we exhaust the string */
    while (len != 0)
    {
        size_t cur;
        unsigned char oldc;

        /* display as much as will fit on the current line */
        cur = max_column - *x + 1;
        if (cur > len)
            cur = len;

        /* null-terminate the chunk, but save the original character */
        oldc = str[cur];
        str[cur] = '\0';

        /* display this chunk */
        ossdsp(*y, *x, color, (char *)str);

        /* restore the character where we put our null */
        str[cur] = oldc;

        /* move our string counters past this chunk */
        str += cur;
        len -= cur;

        /* advance the x position */
        *x += cur;
        if (*x > max_column)
        {
            /* reset to the first column */
            *x = 0;

            /* scroll the screen if necessary */
            if (*y == max_line)
                ossscr(text_line, text_column,
                       max_line, max_column, osssb_oss_screen_color);
            else
                ++*y;
        }
    }
}

/*
 *   Delete a character in the buffer, updating the display. 
 */
static void oss_gets_delchar(unsigned char *buf, unsigned char *p,
                             unsigned char **eol, int x, int y)
{
    if (p < *eol)
    {
        /* delete the character and close the gap */
        --*eol;
        if (p != *eol)
            memmove(p, p + 1, *eol - p);
        
        /* 
         *   replace the last character with a blank, so that we overwrite
         *   its presence on the display 
         */
        **eol = ' ';
        
        /* re-display the changed part of the string */
        ossdsp_str(y, x, text_color, p, *eol - p + 1);
        
        /* null-terminate the shortened buffer */
        **eol = '\0';
    }
}

/*
 *   Backspace in the buffer, updating the display and adjusting the cursor
 *   position. 
 */
static void oss_gets_backsp(unsigned char *buf, unsigned char **p,
                            unsigned char **eol, int *x, int *y)
{
    /* if we can back up, do so */
    if (*p > buf)
    {
        /* move our insertion point back one position */
        --*p;
        
        /* the line is now one character shorter */
        --*eol;

        /* shift all of the characters down one position */
        if (*p != *eol)
            memmove(*p, *p + 1, *eol - *p);

        /* move the cursor back, wrapping if at the first column */
        if (--*x < 0)
        {
            *x = max_column;
            --*y;
        }
        
        /* 
         *   replace the trailing character with a space, so that we
         *   overwrite its screen position with a blank 
         */
        **eol = ' ';

        /* 
         *   display the string from the current position, so that we update
         *   the display for the moved characters 
         */
        ossdsp_str(*y, *x, text_color, *p, *eol - *p + 1);
        
        /* null-terminate the shortened buffer */
        **eol = '\0';
    }
}

/*
 *   Move the cursor left by the number of characters. 
 */
static void oss_gets_csrleft(int *y, int *x, size_t len)
{
    for ( ; len != 0 ; --len)
    {
        /* move left one character, wrapping at the end of the line */
        if (--*x < 0)
        {
            /* move up a line */
            --*y;

            /* move to the end of the line */
            *x = max_column;
        }
    }
}

/*
 *   Move the cursor right by the number of characters.  
 */
static void oss_gets_csrright(int *y, int *x, size_t len)
{
    for ( ; len != 0 ; --len)
    {
        /* move right one character, wrapping at the end of the line */
        if (++*x > max_column)
        {
            /* move down a line */
            ++*y;

            /* move to the left column */
            *x = 0;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   cancel interrupted input 
 */
void os_gets_cancel(int reset)
{
    int x, y;

    /* 
     *   if we interrupted a previous line, apply display effects as though
     *   the user had pressed return 
     */
    if (S_gets_in_progress)
    {
        /* move to the end of the input line */
        x = S_gets_x;
        y = S_gets_y;
        oss_gets_csrright(&y, &x, strlen(S_gets_buf + S_gets_ofs));
        
        /* add a newline, scrolling if necessary */
        if (y == max_line)
            ossscr(text_line, text_column, max_line, max_column,
                   osssb_oss_screen_color);
        
        /* set the cursor to the new position */
        ossloc(y, x);
        
        /* move to the left column for the next display */
        text_lastcol = 0;
        
        /* copy the buffer to the screen save buffer, adding a newline */
        ossaddsbe(S_gets_buf, S_gets_buf + strlen(S_gets_buf), x, y);
        if (y == max_line)
            ossaddsb("\n", 1);

        /* we no longer have an input in progress */
        S_gets_in_progress = FALSE;
    }
    
    /* if we're resetting, clear our saved buffer */
    if (reset)
        S_gets_buf[0] = '\0';
}

/* ------------------------------------------------------------------------ */
/*
 *   Common routine to read a command from the keyboard.  This
 *   implementation provides command editing and history, as well as timeout
 *   capabilities.
 */
int os_gets_timeout(unsigned char *buf, size_t bufl,
                    unsigned long timeout, int use_timeout)
{
    unsigned char *p;
    unsigned char *eol;
    unsigned char *eob = buf + bufl - 1;
    int x;
    int y;
    int origx;
    long end_time;

    /* if we're in 'plain' mode, simply use stdio input */
    if (os_f_plain)
    {
        size_t len;
        
        /* we don't support the timeout feature in plain mode */
        if (use_timeout)
            return OS_EVT_NOTIMEOUT;

        /* 
         *   get input from stdio, and translate the result code - if gets()
         *   returns null, it indicates an error of some kind, so return an
         *   end-of-file indication 
         */
        if (fgets((char *)buf, bufl, stdin) == 0)
            return OS_EVT_EOF;

        /* remove the trailing newline from the buffer, if present */
        if ((len = strlen((char *)buf)) != 0 && buf[len-1] == '\n')
            buf[len-1] = '\0';

        /* indicate that we read a line */
        return OS_EVT_LINE;
    }

# ifdef USE_HISTORY
    /* allocate the history buffer if it's not already allocated */
    if (histbuf == 0)
    {
        histbuf = (unsigned char *)osmalloc(HISTBUFSIZE);
        histhead = histtail = histbuf;
        S_gets_curhist = histhead;
    }
# endif /* USE_HISTORY */

    /*
     *   If we have a timeout, calculate the system clock time at which the
     *   timeout expires.  This is simply the current system clock time plus
     *   the timeout interval.  Since we might need to process a series of
     *   events, we'll need to know how much time remains at each point we
     *   get a new event.  
     */
    end_time = os_get_sys_clock_ms() + timeout;
    
    /* set up at the last output position, after the prompt */
    x = text_lastcol;
    y = max_line;
    origx = x;

    /*
     *   If we have saved input state from a previous interrupted call,
     *   restore it now.  Otherwise, initialize everything.  
     */
    if (S_gets_buf[0] != '\0' || S_gets_in_progress)
    {
        size_t len;
        
        /* limit the restoration length to the new buffer length */
        len = strlen((char *)buf);
        if (len > bufl - 1)
            len = bufl - 1;

        /* copy the saved buffer to the caller's buffer */
        memcpy(buf, S_gets_buf, len);

        /* set up our pointer to the end of the text */
        eol = buf + len;

        /* null-terminate the text */
        *eol = '\0';

        /* 
         *   if we cancelled the previous input, we must re-display the
         *   buffer under construction, since we have displayed something
         *   else in between and have re-displayed the prompt 
         */
        if (!S_gets_in_progress)
        {
            /* re-display the buffer */
            ossdsp_str_scr(&y, &x, text_color, buf, len);

            /* 
             *   move back to the original insertion point, limiting the
             *   move to the available buffer size for the new caller 
             */
            if (S_gets_ofs < (int)len)
                oss_gets_csrleft(&y, &x, len - S_gets_ofs);
            else
                S_gets_ofs = len;
        }
        else
        {
            /* 
             *   input is still in progress, so the old text is still on the
             *   screen - simply move to the original cursor position
             */
            x = S_gets_x;
            y = S_gets_y;
        }

        /* get our pointer into the buffer */
        p = buf + S_gets_ofs;
    }
    else
    {
        /* set up our buffer pointers */
        p = buf;
        eol = buf;

        /* initialize our history recall pointer */
        S_gets_curhist = histhead;

        /* clear out the buffer */
        buf[0] = '\0';
    }

    /* process keystrokes until we're done entering the command */
    for ( ;; )
    {
        unsigned char c;
        os_event_info_t event_info;
        int event_type;
        
        /* move to the proper position on the screen */
        ossloc(y, x);

        /* if we're using a timeout, check for expiration */
        if (use_timeout)
        {
            long cur_clock;

            /* note the current system clock time */
            cur_clock = os_get_sys_clock_ms();

            /* 
             *   if we're past the timeout expiration time already,
             *   interrupt with timeout now 
             */
            if (cur_clock >= end_time)
                goto timeout_expired;

            /* note the interval remaining to the timeout expiration */
            timeout = end_time - cur_clock;
        }
        
        /* get an event */
        event_type = os_get_event(timeout, use_timeout, &event_info);

        /* handle the event according to the event type */
        switch(event_type)
        {
        case OS_EVT_KEY:
            /* 
             *   Keystroke event.  First, translate the key from the "raw"
             *   keystroke that os_get_event() returns to the "processed"
             *   (CMD_xxx) representation.  This lets the oss layer map
             *   special keystrokes to generic editing commands in a
             *   system-specific manner.  
             */
            oss_raw_key_to_cmd(&event_info);

            /* get the primary character from the event */
            c = event_info.key[0];
            break;

        case OS_EVT_TIMEOUT:
        timeout_expired:
            /*
             *   The timeout expired.  Copy the current input state into the
             *   save area so that we can resume the input later if desired.
             */
            strcpy(S_gets_buf, (char *)buf);
            S_gets_ofs = p - buf;
            S_gets_x = x;
            S_gets_y = y;

            /* note that input was interrupted */
            S_gets_in_progress = TRUE;

            /* return the timeout status to the caller */
            return OS_EVT_TIMEOUT;

        case OS_EVT_NOTIMEOUT:
            /* 
             *   we can't handle events with timeouts, so we can't provide
             *   line reading with timeouts, either
             */
            return OS_EVT_NOTIMEOUT;

        case OS_EVT_EOF:
            /* end of file - return the same error to our caller */
            return OS_EVT_EOF;

        default:
            /* ignore any other events */
            continue;
        }
            
        /* 
         *   Check the character we got.  Note that we must interpret
         *   certain control characters explicitly, because os_get_event()
         *   returns raw keycodes (untranslated into CMD_xxx codes) for
         *   control characters.  
         */
        switch(c)
        {
        case 8:
            /* backspace one character */
            oss_gets_backsp(buf, &p, &eol, &x, &y);
            break;

        case 13:
            /* null-terminate the input */
            *eol = '\0';

            /* move to the end of the line */
            oss_gets_csrright(&y, &x, eol - p);
            p = eol;

            /*
             *   Scroll the screen to account for the carriage return,
             *   position the cursor at the end of the new line, and
             *   null-terminate the line.  
             */
            ossloc(y, x);
            if (y == max_line)
                ossscr(text_line, text_column, max_line, max_column,
                       osssb_oss_screen_color);

            /* move to the left column for the next display */
            text_lastcol = 0;

# ifdef USE_HISTORY
            /*
             *   Save the line in our history buffer.  If we don't have
             *   enough room, lose some old text by advancing the tail
             *   pointer far enough.  Don't save it if it's a blank line,
             *   though, or if it duplicates the most recent previous
             *   command.
             */
            if (strlen((char *)buf) != 0)
            {
                uchar *q;
                int    advtail;
                int    saveflag = 1;         /* assume we will be saving it */
                
                if (q = ossprvcmd(histhead))
                {
                    uchar *p = buf;
                    
                    while (*p == *q && *p != '\0' && *q != '\0')
                    {
                        ++p;
                        q = ossadvhp(q);
                    }
                    if (*p == *q)           /* is this a duplicate command? */
                        saveflag = 0;               /* if so, don't save it */
                }

                if (saveflag)
                {
                    for (q = buf, advtail = 0 ; q <= eol ; ++q)
                    {
                        *histhead = *q;
                        histhead = ossadvhp(histhead);
                        if (histhead == histtail)
                        {
                            histtail = ossadvhp(histtail);
                            advtail = 1;
                        }
                    }
                    
                    /*
                     *   If we have encroached on space that was already
                     *   occupied, throw away the entire command we have
                     *   partially trashed; to do so, advance the tail
                     *   pointer to the next null byte.  
                     */
                    if (advtail)
                    {
                        while(*histtail)
                            histtail = ossadvhp(histtail);
                        histtail = ossadvhp(histtail);
                    }
                }
            }
# endif /* USE_HISTORY */

            /*
             *   Finally, copy the buffer to the screen save buffer (if
             *   applicable), and return the contents of the buffer.  Note
             *   that we add an extra carriage return if we were already
             *   on the max_line, since we scrolled the screen in this
             *   case; otherwise, ossaddsbe will add all the blank lines
             *   that are necessary.  
             */
            ossaddsbe((char *)buf, (char *)p, x, y);
            if (y == max_line)
                ossaddsb("\n", 1);

            /* input is no longer in progress */
            S_gets_in_progress = FALSE;
            S_gets_buf[0] = '\0';

            /* return success */
            return OS_EVT_LINE;

        case 0:
            /* extended key code - get the second half of the code */
            c = event_info.key[1];

            /* handle the command key code */
            switch(c)
            {
# ifdef USE_SCROLLBACK
            case CMD_SCR:
                {
                    char *old_scrbuf_free;
                    
                    /*
                     *   Add the contents of the line buffer, plus any blank
                     *   lines, to the screen buffer, filling the screen to
                     *   the bottom.  Before we do, though, save the current
                     *   scrollback buffer free pointer, so we can take our
                     *   buffer back out of the scrollback buffer when we're
                     *   done - this is just temporary so that the current
                     *   command shows up in the buffer while we're in
                     *   scrollback mode and is redrawn when we're done.  
                     */
                    old_scrbuf_free = scrbuf_free;
                    ossaddsbe((char *)buf, (char *)p, x, y);

                    /* run scrollback mode */
                    if (!osssbmode(1))
                        ossdosb();

                    /* restore the old free pointer */
                    scrbuf_free = old_scrbuf_free;
                    *scrbuf_free = '\0';

                    /* go back to our original column */
                    sb_column = origx;
                    break;
                }
# endif /* USE_SCROLLBACK */

            case CMD_LEFT:
                if (p > buf)
                {
                    --p;
                    --x;
                    if (x < 0)
                    {
                        x = max_column;
                        --y;
                    }
                }
                break;

            case CMD_WORD_LEFT:
                if (p > buf)
                {
                    --p;
                    --x;
                    if (x < 0)
                        x = max_column, --y;
                    while (p > buf && t_isspace(*p))
                    {
                        --p, --x;
                        if (x < 0)
                            x = max_column, --y;
                    }
                    while (p > buf && !t_isspace(*(p-1)))
                    {
                        --p, --x;
                        if (x < 0)
                            x = max_column, --y;
                    }
                }
                break;

            case CMD_RIGHT:
                if (p < eol)
                {
                    ++p;
                    ++x;
                    if (x > max_column)
                    {
                        x = 0;
                        ++y;
                    }
                }
                break;

            case CMD_WORD_RIGHT:
                while (p < eol && !t_isspace(*p))
                {
                    ++p, ++x;
                    if (x > max_column)
                        x = 0, ++y;
                }
                while (p < eol && t_isspace(*p))
                {
                    ++p, ++x;
                    if (x > max_column)
                        x = 0, ++y;
                }
                break;

            case CMD_DEL:
                /* delete a character */
                oss_gets_delchar(buf, p, &eol, x, y);
                break;

#ifdef UNIX               
            case CMD_WORDKILL:
                {
                    /* remove spaces preceding word */
                    while (p >= buf && *p <= ' ')
                        oss_gets_backsp(buf, &p, &eol, &x, &y);
                    
                    /* remove previous word (i.e., until we get a space) */
                    while (p >= buf && *p > ' ')
                        oss_gets_backsp(buf, &p, &eol, &x, &y);

                    /* that's it */
                    break;
                }
#endif /* UNIX */

            case CMD_KILL:
            case CMD_HOME:
# ifdef USE_HISTORY
            case CMD_UP:
            case CMD_DOWN:
                if (c == CMD_UP && !ossprvcmd(S_gets_curhist))
                    break;                /* no more history - ignore arrow */
                if (c == CMD_DOWN && !ossnxtcmd(S_gets_curhist))
                    break;                /* no more history - ignore arrow */
                if (c == CMD_UP && !ossnxtcmd(S_gets_curhist))
                {
                    /* first Up arrow - save current buffer */
                    safe_strcpy(S_hist_sav, S_hist_sav_siz, (char *)buf);
                }
# endif /* USE_HISTORY */
                while(p > buf)
                {
                    --p;
                    if (--x < 0)
                    {
                        x = max_column;
                        --y;
                    }
                }
                if (c == CMD_HOME)
                    break;

                /*
                 *   We're at the start of the line now; fall through for
                 *   KILL, UP, and DOWN to the code which deletes to the
                 *   end of the line.  
                 */
            case CMD_DEOL:
                if (p < eol)
                {
                    /* 
                     *   write spaces to the end of the line, to clear out
                     *   the screen display of the old characters 
                     */
                    if (p != eol)
                    {
                        memset(p, ' ', eol - p);
                        ossdsp_str(y, x, text_color, p, eol - p);
                    }

                    /* truncate the buffer at the insertion point */
                    eol = p;
                    *p = '\0';
                }
# ifdef USE_HISTORY
                if (c == CMD_UP)
                {
                    S_gets_curhist = ossprvcmd(S_gets_curhist);
                    osshstcpy(buf, S_gets_curhist);
                }
                else if (c == CMD_DOWN)
                {
                    if (!ossnxtcmd(S_gets_curhist))
                        break;                                   /* no more */
                    S_gets_curhist = ossnxtcmd(S_gets_curhist);
                    if (ossnxtcmd(S_gets_curhist))    /* on a valid command */
                        osshstcpy(buf, S_gets_curhist);    /* ... so use it */
                    else
                    {
                        /* no more history - restore original line */
                        safe_strcpy((char *)buf, bufl, S_hist_sav);
                    }
                }
                if ((c == CMD_UP || c == CMD_DOWN)
                    && strlen((char *)buf) != 0)
                {
                    /* get the end pointer based on null termination */
                    eol = buf + strlen((char *)buf);

                    /* display the string */
                    ossdsp_str_scr(&y, &x, text_color, p, eol - p);

                    /* move to the end of the line */
                    p = eol;
                }
# endif /* USE_HISTORY */
                break;
            case CMD_END:
                while (p < eol)
                {
                    ++p;
                    if (++x > max_column)
                    {
                        x = 0;
                        ++y;
                    }
                }
                break;

            case CMD_EOF:
                /* on end of file, return null */
                return OS_EVT_EOF;
            }
            break;

        default:
            if (c >= ' ' && eol < eob)
            {
                /* open up the line and insert the character */
                if (p != eol)
                    memmove(p + 1, p, eol - p);
                ++eol;
                *p = c;
                *eol = '\0';

                /* write the updated part of the line */
                ossdsp_str_scr(&y, &x, text_color, p, eol - p);

                /* move the cursor back to the insertion point */
                ++p;
                oss_gets_csrleft(&y, &x, eol - p);
            }
            break;
        }
    }
}

/*
 *   Read a line of input.  We implement this in terms of the timeout input
 *   line reader, passing an infinite timeout to that routine.
 */
uchar *os_gets(unsigned char *buf, size_t bufl)
{
    int evt;

    /* cancel any previous input, clearing the buffer */
    os_gets_cancel(TRUE);

    /* get a line of input, with no timeout */
    evt = os_gets_timeout(buf, bufl, 0, FALSE);

    /* translate the event code to the appropriate return value */
    switch(evt)
    {
    case OS_EVT_LINE:
        /* we got a line of input - return a pointer to our buffer */
        return buf;

    case OS_EVT_EOF:
        /* end of file - return null */
        return 0;

    default:
        /* we don't expect any other results */
        assert(FALSE);
        return 0;
    }
}

#else /* USE_STATLINE */

#endif /* USE_STATLINE */

/* ------------------------------------------------------------------------ */
/*
 *   Highlighting and colors 
 */

#ifdef STD_OS_HILITE

#ifdef RUNTIME
/*
 *   Set text attributes 
 */
void os_set_text_attr(int attr)
{
    char buf[3];

    /* if we're in plain mode, ignore it */
    if (os_f_plain)
        return;

    /* if the attributes aren't changing, do nothing */
    if (osssb_cur_attrs == attr)
        return;

    /* 
     *   add the attribute sequence to the scrollback buffer (encoding with
     *   our plus-one code, to avoid storing null bytes)
     */
    buf[0] = OSGEN_ATTR;
    buf[1] = (char)(attr + 1);
    buf[2] = '\0';
    ossaddsb(buf, 2);

    /* translate the codes to an internal ossdsp color */
    text_color = ossgetcolor(osssb_cur_fg, osssb_cur_bg, attr,
                             osssb_screen_color);
}

/*
 *   Translate a color from the os_color_t encoding to an OSGEN_xxx color.  
 */
static char osgen_xlat_color_t(os_color_t color)
{
    size_t i;
    struct color_map_t
    {
        /* the OSGEN_COLOR_xxx value */
        char id;

        /* the RGB components for the color */
        unsigned char rgb[3];
    };
    struct color_map_t *p;
    struct color_map_t *bestp;
    static struct color_map_t color_map[] =
    {
        { OSGEN_COLOR_BLACK,   { 0x00, 0x00, 0x00 } },
        { OSGEN_COLOR_WHITE,   { 0xFF, 0xFF, 0xFF } },
        { OSGEN_COLOR_RED,     { 0xFF, 0x00, 0x00 } },
        { OSGEN_COLOR_BLUE,    { 0x00, 0x00, 0xFF } },
        { OSGEN_COLOR_GREEN,   { 0x00, 0x80, 0x00 } },
        { OSGEN_COLOR_YELLOW,  { 0xFF, 0xFF, 0x00 } },
        { OSGEN_COLOR_CYAN,    { 0x00, 0xFF, 0xFF } },
        { OSGEN_COLOR_SILVER,  { 0xC0, 0xC0, 0xC0 } },
        { OSGEN_COLOR_GRAY,    { 0x80, 0x80, 0x80 } },
        { OSGEN_COLOR_MAROON,  { 0x80, 0x00, 0x00 } },
        { OSGEN_COLOR_PURPLE,  { 0x80, 0x00, 0x80 } },
        { OSGEN_COLOR_MAGENTA, { 0xFF, 0x00, 0xFF } },
        { OSGEN_COLOR_LIME,    { 0x00, 0xFF, 0x00 } },
        { OSGEN_COLOR_OLIVE,   { 0x80, 0x80, 0x00 } },
        { OSGEN_COLOR_NAVY,    { 0x00, 0x00, 0x80 } },
        { OSGEN_COLOR_TEAL,    { 0x00, 0x80, 0x80 } }
    };
    unsigned char r, g, b;
    unsigned long best_dist;

    /* 
     *   If it's parameterized, map it by shifting the parameter code (in
     *   the high-order 8 bits of the os_color_t) to our single-byte code,
     *   which is defined as exactly the same code as the os_color_t values
     *   but shifted into the low-order 8 bits.  
     */
    if (os_color_is_param(color))
        return (char)((color >> 24) & 0xFF);

    /* break the color into its components */
    r = os_color_get_r(color);
    g = os_color_get_g(color);
    b = os_color_get_b(color);

    /* search for the closest match among our 16 ANSI colors */
    for (i = 0, p = color_map, bestp = 0, best_dist = 0xFFFFFFFF ;
         i < sizeof(color_map)/sizeof(color_map[0]) ; ++i, ++p)
    {
        unsigned long dist;
        int rd, gd, bd;

        /* calculate the delta for each component */
        rd = r - p->rgb[0];
        gd = g - p->rgb[1];
        bd = b - p->rgb[2];

        /* calculate the "distance" in RGB space */
        dist = rd*rd + gd*gd + bd*bd;

        /* if it's an exact match, we need look no further */
        if (dist == 0)
            return p->id;

        /* if it's the smallest distance so far, note it */
        if (dist < best_dist)
        {
            best_dist = dist;
            bestp = p;
        }
    }

    /* return the OSGEN_COLOR_xxx ID of the best match we found */
    return bestp->id;
}

/*
 *   Set the text colors.
 *   
 *   The foreground and background colors apply to subsequent characters
 *   displayed via os_print() (etc).  If the background color is set to zero,
 *   it indicates "transparent" drawing: subsequent text is displayed with
 *   the "screen" color.  
 */
void os_set_text_color(os_color_t fg, os_color_t bg)
{
    char buf[4];

    /* if we're in plain mode, ignore it */
    if (os_f_plain)
        return;

    /* add the color sequence to the scrollback buffer */
    buf[0] = OSGEN_COLOR;
    buf[1] = osgen_xlat_color_t(fg);
    buf[2] = osgen_xlat_color_t(bg);
    buf[3] = '\0';
    ossaddsb(buf, 3);

    /* translate the codes to an internal ossdsp color */
    text_color = ossgetcolor(fg, bg, osssb_cur_attrs, osssb_screen_color);
}

/*
 *   Set the screen color 
 */
void os_set_screen_color(os_color_t color)
{
    /* if we're in plain mode, ignore it */
    if (os_f_plain)
        return;

    /* set the new background color */
    osssb_screen_color = color;
    osssb_oss_screen_color = ossgetcolor(OSGEN_COLOR_TEXT,
                                         osgen_xlat_color_t(color), 0, 0);

    /* 
     *   recalculate the current text color, since it will be affected by the
     *   background change if its background is transparent 
     */
    text_color = ossgetcolor(osssb_cur_fg, osssb_cur_bg, osssb_cur_attrs,
                             osssb_screen_color);

    /* redraw the screen in the new background color */
    os_redraw();
}

/* redraw the screen if necessary */
void osssb_redraw_if_needed()
{
    /* this implementation never defers redrawing - do nothing */
}

/* set the default cursor position */
void osssb_cursor_to_default_pos()
{
    /* we don't need to do anything special here */
}

#else /* RUNTIME */

void os_set_text_attr(int attr)
{
    /* attributes are not supported in non-RUNTIME mode */
}

void os_set_text_color(os_color_t fg, os_color_t bg)
{
    /* colors aren't supported in non-RUNTIME mode - ignore it */
}

void os_set_screen_color(os_color_t color)
{
    /* colors aren't supported in non-RUNTIME mode - ignore it */
}

#endif /* RUNTIME */

#endif /* STD_OS_HILITE */

/* ------------------------------------------------------------------------ */
/* 
 *   clear the screen, deleting all scrollback information
 */
#ifdef STD_OSCLS

void oscls(void)
{
#ifdef RUNTIME
    /* do nothing in 'plain' mode */
    if (os_f_plain)
        return;

    /* forget the scrollback buffer's contents */
    scrbuf_free = scrbuf;
    scrbuf_head = scrbuf_tail = scrbuf;
    scrtop = scrbot = scrlast = 0;
    os_line_count = 1;
    memset(scrbuf, 0, (size_t)scrbufl);
    os_redraw();
#endif
}

#endif /* STD_OSCLS */

/* ------------------------------------------------------------------------ */
/*
 *   Simple implementation of os_get_sysinfo.  This can be used for any
 *   non-HTML version of the system, since all sysinfo codes currently
 *   pertain to HTML features.  Note that new sysinfo codes may be added
 *   in the future which may be relevant to non-html versions, so the
 *   sysinfo codes should be checked from time to time to ensure that new
 *   codes relevant to this system version are handled correctly here.  
 */
int os_get_sysinfo(int code, void *param, long *result)
{
#ifdef RUNTIME
    /* if the oss layer recognizes the code, defer to its judgment */
    if (oss_get_sysinfo(code, param, result))
        return TRUE;
#endif

    /* check the type of information they're requesting */
    switch(code)
    {
    case SYSINFO_INTERP_CLASS:
        /* we're a character-mode text-only interpreter */
        *result = SYSINFO_ICLASS_TEXT;
        return TRUE;
        
    case SYSINFO_HTML:
    case SYSINFO_JPEG:
    case SYSINFO_PNG:
    case SYSINFO_WAV:
    case SYSINFO_MIDI:
    case SYSINFO_WAV_MIDI_OVL:
    case SYSINFO_WAV_OVL:
    case SYSINFO_MPEG:
    case SYSINFO_MPEG1:
    case SYSINFO_MPEG2:
    case SYSINFO_MPEG3:
    case SYSINFO_PREF_IMAGES:
    case SYSINFO_PREF_SOUNDS:
    case SYSINFO_PREF_MUSIC:
    case SYSINFO_PREF_LINKS:
    case SYSINFO_LINKS_HTTP:
    case SYSINFO_LINKS_FTP:
    case SYSINFO_LINKS_NEWS:
    case SYSINFO_LINKS_MAILTO:
    case SYSINFO_LINKS_TELNET:
    case SYSINFO_PNG_TRANS:
    case SYSINFO_PNG_ALPHA:
    case SYSINFO_OGG:
    case SYSINFO_MNG:
    case SYSINFO_MNG_TRANS:
    case SYSINFO_MNG_ALPHA:
    case SYSINFO_BANNERS:
        /* 
         *   we don't support any of these features - set the result to 0
         *   to indicate this 
         */
        *result = 0;

        /* return true to indicate that we recognized the code */
        return TRUE;

    default:
        /* not recognized */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the saved-game extension.  Most platforms don't need to do
 *   anything with this information, and in fact most platforms won't even
 *   have a way of letting the game author set the saved game extension,
 *   so this trivial implementation is suitable for most systems.
 *   
 *   The purpose of setting a saved game extension is to support platforms
 *   (such as Windows) where the filename suffix is used to associate
 *   document files with applications.  Each stand-alone executable
 *   generated on such platforms must have a unique saved game extension,
 *   so that the system can associate each game's saved position files
 *   with that game's executable.  
 */
void os_set_save_ext(const char *ext)
{
    /* ignore the setting */
}

const char *os_get_save_ext()
{
    /* we ignore the setting, so always return null */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set the game title.  Most platforms have no use for this information,
 *   so they'll just ignore it.  This trivial implementation simply
 *   ignores the title. 
 */
#ifdef USE_NULL_SET_TITLE

void os_set_title(const char *title)
{
    /* ignore the information */
}

#endif /* USE_NULL_SET_TITLE */

/* ------------------------------------------------------------------------ */
/*
 *   The "banner" window functions are not supported in this implementation.
 */
void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units, unsigned long style)
{
    return 0;
}

void os_banner_delete(void *banner_handle)
{
}

void os_banner_orphan(void *banner_handle)
{
}

void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
}

void os_banner_set_attr(void *banner_handle, int attr)
{
}

void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg)
{
}

void os_banner_set_screen_color(void *banner_handle, os_color_t color)
{
}

void os_banner_flush(void *banner_handle)
{
}

void os_banner_set_size(void *banner_handle, int siz, int siz_units,
                        int is_advisory)
{
}

void os_banner_size_to_contents(void *banner_handle)
{
}

void os_banner_start_html(void *banner_handle)
{
}

void os_banner_end_html(void *banner_handle)
{
}

void os_banner_goto(void *banner_handle, int row, int col)
{
}

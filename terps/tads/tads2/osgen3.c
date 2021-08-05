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
  osgen3  - Operating System dependent functions, general implementation
            TADS 3 Version, with new "banner" interface
Function
  This module contains certain OS-dependent functions that are common
  to many character-mode platforms.  ("Character mode" means that the
  display is organized as a rectangular grid of characters in a monospaced
  font.  This module isn't usable for most GUI systems, because it doesn't
  have any support for variable-pitch fonts - GUI ports generally need to
  provide their own custom versions of the os_xxx() functions this module
  module provides.  On GUI systems you should simply omit this entire
  module from the build, and instead substitute a new module of your own
  creation that defines your custom versions of the necessary os_xxx()
  functions.)

  Some routines in this file are selectively enabled according to macros
  defined in os.h, since some ports that use this file will want to provide
  their own custom versions of these routines instead of the ones defined
  here.  The macros and associated functions are:

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

/*
 *   Flag: use "plain" mode.  If this is set, we'll use plain stdio output
 *   rather than our window-oriented display.  
 */
int os_f_plain = 0;

#ifdef RUNTIME
# ifdef USE_SCROLLBACK

/*
 *   Screen size variables.  The underlying system-specific "oss" code must
 *   initialize these during startup and must keep them up-to-date if the
 *   screen size ever changes.  
 */
int G_oss_screen_width = 80;
int G_oss_screen_height = 24;


# endif /* USE_SCROLLBACK */
#endif /* RUNTIME */

/*
 *   The special character codes for controlling color. 
 */

/* 
 *   set text attributes: this is followed by one byte giving the new
 *   attribute codes 
 */
#define OSGEN_ATTR            1

/* 
 *   Set text color: this is followed by two bytes giving the foreground and
 *   background colors as OSGEN_COLOR_xxx codes.
 *   
 *   Note well that the colors encoded in this escape sequence are
 *   OSGEN_COLOR_xxx values, not os_color_t values.  The latter require 32
 *   bits because they can store 24-bit RGB values plus some special
 *   parameter codes, while our internal OSGEN_COLOR_xxx values are only a
 *   byte each.  
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
 *   of the same name, and os_print and os_printz using the fputs() to
 *   stdout, by defining USE_STDIO.  These definitions can be used for any
 *   port for which the standard C run-time library is available.  
 */

#ifdef USE_STDIO

/* 
 *   os_printz works just like fputs() to stdout: we write a null-terminated
 *   string to the standard output.  
 */
void os_printz(const char *str)
{
    fputs(str, stdout);
}

/*
 *   os_puts works like fputs() to stdout, except that we are given a
 *   separate length, and the string might not be null-terminated 
 */
void os_print(const char *str, size_t len)
{
    printf("%.*s", (int)len, str);
}

/*
 *   os_flush forces output of anything buffered for standard output.  It
 *   is generally used prior to waiting for a key (so the normal flushing
 *   may not occur, as it does when asking for a line of input).  
 */
void os_flush(void)
{
    fflush(stdout);
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
    /* make sure everything we've displayed so far is actually displayed */
    fflush(stdout);

    /* get a line of input from the standard input */
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

/* ------------------------------------------------------------------------ */
/*
 *   Scrollback 
 */

/* forward-declare the window control structure type */
typedef struct osgen_win_t osgen_win_t;

/*
 *   We can be compiled with or without scrollback.  The first version
 *   defines the scrollback implementation; the second just defines some
 *   dummy functions for the non-scrollback implementation.  
 */
# ifdef USE_SCROLLBACK

/* ------------------------------------------------------------------------ */
/* 
 *   Character color structure 
 */
typedef struct osgen_charcolor_t osgen_charcolor_t;
struct osgen_charcolor_t
{
    /* foreground color, as an OSGEN_COLOR_xxx value */
    char fg;

    /* background color, as an OSGEN_COLOR_xxx value */
    char bg;
};

/*
 *   Window control structure.  Each on-screen window is represented by one
 *   of these structures.  
 */
struct osgen_win_t
{
    /* type of the window - this is an OS_BANNER_TYPE_xxx code */
    int win_type;

    /* flags */
    unsigned int flags;

    /* next window in window list */
    osgen_win_t *nxt;

    /* 
     *   Parent window.  We take our display area out of the parent window's
     *   allocated space at the time we lay out this window.  
     */
    osgen_win_t *parent;

    /* head of list of children of this window */
    osgen_win_t *first_child;

    /* 
     *   The window's alignment type - this determines where the window goes
     *   relative to the main window.  This is an OS_BANNER_ALIGN_xxx
     *   alignment type code.  
     */
    int alignment;

    /* size type (as an OS_BANNER_SIZE_xxx value) */
    int size_type;

    /* 
     *   The window's size.  If size_type is OS_BANNER_SIZE_ABS, this is the
     *   size of the window in character cells.  If size_type is
     *   OS_BANNER_SIZE_PCT, this is given as a percentage of the full screen
     *   size.  
     */
    int size;

    /* upper-left corner position of window on screen */
    int winx;
    int winy;

    /* size of window on screen */
    size_t wid;
    size_t ht;

    /* 
     *   Cursor position (location of next output).  These are given in
     *   "document coordinates", which is to say that they're relative to
     *   the start of the text in the buffer.
     *   
     *   To translate to "window coordinates", simply subtract the scrolling
     *   offsets, which give the document coordinates of the first character
     *   displayed at the upper left corner of the window.
     *   
     *   To translate to absolute screen coordinates, first subtract the
     *   scrolling offsets to get window coordinates, then add the window's
     *   screen position (winx,winy).  
     */
    int x;
    int y;

    /* 
     *   maximum row and line where we've actually written any text (this
     *   can be used for purposes like setting horizontal scrollbar limits
     *   and sizing a window horizontally to its contents) 
     */
    int xmax;
    int ymax;

    /* scrolling offset of text displayed in window */
    int scrollx;
    int scrolly;

    /* 
     *   current text foreground and background colors (as OSGEN_COLOR_xxx
     *   values) 
     */
    char txtfg;
    char txtbg;

    /* current text attributes */
    int txtattr;

    /* window fill color (and oss color code for same) */
    char fillcolor;
    int oss_fillcolor;
};

/*
 *   Ordinary text stream window.  This is a subclass of the basic
 *   osgen_win_t window type, used when win_type is OS_BANNER_TYPE_TEXT.
 *   
 *   Ordinary text windows keep a circular buffer of scrollback text.  We
 *   optimize space by using escape codes embedded in the saved text stream
 *   to select colors and attributes.
 *   
 *   In the circular text buffer, each line ends with a null byte.  We keep
 *   an array of line-start pointers to make it fast and easy to find the
 *   first byte of a particular line.  The line-start array is also
 *   circular, and is organized in ascending order of row number.  
 */
typedef struct osgen_txtwin_t osgen_txtwin_t;
struct osgen_txtwin_t
{
    /* embed the base class */
    osgen_win_t base;

    /* text color and attributes at start of latest line */
    int solfg;
    int solbg;
    int solattr;
            
    /* window text buffer, and size of the buffer */
    char *txtbuf;
    size_t txtbufsiz;
    
    /* next free byte of window text buffer */
    char *txtfree;
    
    /* circular array of line-start pointers */
    char **line_ptr;
    size_t line_ptr_cnt;
    
    /* index of first line-start pointer in use */
    size_t first_line;
    
    /* number of lines of text stored in the buffer */
    size_t line_count;
};

/*
 *   Text Grid window.  This is a subclass of the basic window type,
 *   osgen_win_t, used when win_type is OS_BANNER_TYPE_TEXTGRID.
 *   
 *   A text grid window keeps a simple rectangular array of text, and a
 *   corresponding array of the color of each character.  The size of each
 *   array is at least as large as the window's actual area on the screen;
 *   when we resize the window, we'll reallocate the arrays at a larger size
 *   if the window has expanded beyond the stored size.  We don't keep any
 *   scrollback information in a text grid; we only keep enough to cover
 *   what's actually on the screen.  
 */
typedef struct osgen_gridwin_t osgen_gridwin_t;
struct osgen_gridwin_t
{
    /* embed the base class */
    osgen_win_t base;
    
    /* width and height of the text and color arrays */
    size_t grid_wid;
    size_t grid_ht;

    /* text array */
    char *grid_txt;
    
    /* color array */
    osgen_charcolor_t *grid_color;
};

/*
 *   Window flags 
 */

/* keep the cursor visible when adding text to the window */
#define OSGEN_AUTO_VSCROLL    0x0001

/* the window buffer is full and we're allowing nothing more to enter it */
#define OSGEN_FULL            0x0002

/* the window is in deferred-redraw mode */
#define OSGEN_DEFER_REDRAW    0x0004

/* 
 *   MORE mode in the banner.  Note that we keep track of this only so that
 *   we can indicate it on queries for the banner style; we count on the
 *   caller to handle the actual prompting for us.  
 */
#define OSGEN_MOREMODE        0x0008

/* child banner "strut" flags */
#define OSGEN_VSTRUT          0x0010
#define OSGEN_HSTRUT          0x0020

/* 
 *   The main text area window.  This window is special, because it's the
 *   root of the window tree.  
 */
static osfar_t osgen_txtwin_t *S_main_win = 0;

/* default status line window */
static osfar_t osgen_txtwin_t *S_status_win = 0;

/* default input/output window (for os_print, os_gets, etc) */
static osfar_t osgen_txtwin_t *S_default_win = 0;

/* current scrollback-mode window */
static osfar_t osgen_txtwin_t *S_sbmode_win = 0;

/* scrollback mode settings */
static osfar_t int S_sbmode_orig_scrolly;
static osfar_t int S_sbmode_orig_x;
static osfar_t char **S_sbmode_orig_last_line;
static osfar_t char *S_sbmode_orig_txtfree;

/* 
 *   flag: we're using a special cursor position; we use this to override our
 *   normal default cursor position 
 */
static osfar_t int S_special_cursor_pos = FALSE;
static osfar_t int S_special_cursor_x = 0;
static osfar_t int S_special_cursor_y = 0;

/* 
 *   Flag: deferred redraw required.  This indicates that something happened
 *   that requires redrawing the screen, but we didn't bother actually doing
 *   the redrawing immediately in case other things that would also require
 *   redrawing were to occur shortly. 
 */
static osfar_t int S_deferred_redraw = FALSE;

/*
 *   Input buffer state.  This information is defined statically because
 *   os_gets_timeout() can carry the information from invocation to
 *   invocation when input editing is interrupted by a tmieout.  
 */
static osfar_t char S_gets_internal_buf[256];       /* internal save buffer */
static osfar_t char *S_gets_buf = S_gets_internal_buf;  /* current save buf */
static osfar_t char *S_gets_buf_end = 0;             /* end of input buffer */
static osfar_t size_t S_gets_buf_siz = sizeof(S_gets_internal_buf); /* size */
static osfar_t int S_gets_ofs;       /* offset in buffer of insertion point */
static osfar_t char *S_gets_curhist;             /* current history pointer */
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

/* forward declarations */
static void osssb_add_color_code(osgen_txtwin_t *win);
static void osgen_gridwin_clear(osgen_gridwin_t *win, size_t ofs, size_t len);
static void osgen_redraw_win(osgen_win_t *win);
static void osgen_scrdisp(osgen_win_t *win, int x, int y, int len);
static void osgen_gets_redraw_cmdline(void);

/*
 *   Delete a window 
 */
static void osgen_delete_win(osgen_win_t *win)
{
    osgen_win_t *prv;
    osgen_win_t *cur;
    osgen_txtwin_t *twin;
    osgen_gridwin_t *gwin;

    /* if we have a parent, remove ourselves from our parent's child list */
    if (win->parent != 0)
    {
        /* scan our parent's child list */
        for (prv = 0, cur = win->parent->first_child ;
             cur != 0 && cur != win ;
             prv = cur, cur = cur->nxt) ;

        /* if we found it, unlink it */
        if (cur != 0)
        {
            /* set the previous item's forward pointer */
            if (prv == 0)
                win->parent->first_child = win->nxt;
            else
                prv->nxt = win->nxt;
        }
    }

    /* 
     *   Remove the parent reference from each child of this window.  We're
     *   going to be deleted, so we can't keep references from our children
     *   to us.  
     */
    for (cur = win->first_child ; cur != 0 ; cur = cur->nxt)
        cur->parent = 0;

    /* delete the window according to its type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        /* get the text window subclass data */
        twin = (osgen_txtwin_t *)win;

        /* delete its scrollback buffer, if it has one */
        if (twin->txtbuf != 0)
            osfree(twin->txtbuf);

        /* delete the line pointers */
        if (twin->line_ptr != 0)
            osfree(twin->line_ptr);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* get the grid window subclass data */
        gwin = (osgen_gridwin_t *)win;

        /* delete the character and color arrays */
        if (gwin->grid_txt != 0)
            osfree(gwin->grid_txt);
        if (gwin->grid_color != 0)
            osfree(gwin->grid_color);
        break;
    }

    /* delete the window itself */
    osfree(win);
}

/*
 *   Delete a window and all of its children 
 */
static void osgen_delete_win_tree(osgen_win_t *win)
{
    osgen_win_t *chi;
    osgen_win_t *nxt;

    /* delete the children first */
    for (chi = win->first_child ; chi != 0 ; chi = nxt)
    {
        /* remember the next one before we delete the current one */
        nxt = chi->nxt;

        /* delete this one */
        osgen_delete_win_tree(chi);
    }

    /* delete this window */
    osgen_delete_win(win);
}

/*
 *   Create a window and link it into our list.  We initialize the window
 *   and allocate its display buffer, but we do NOT set the window's size or
 *   position on the screen.  
 */
static osgen_win_t *osgen_create_win(int win_type, int where, void *other,
                                     osgen_win_t *parent)
{
    osgen_win_t *win;
    osgen_win_t *prv;
    osgen_win_t *cur;
    size_t struct_siz;

    /* figure the structure size based on the window type */
    switch(win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        struct_siz = sizeof(osgen_txtwin_t);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        struct_siz = sizeof(osgen_gridwin_t);
        break;

    default:
        /* unrecognized type - return failure */
        return 0;
    }

    /* create the window object */
    win = (osgen_win_t *)osmalloc(struct_siz);
    if (win == 0)
        return 0;

    /* remember the type */
    win->win_type = win_type;

    /* it's not in a window list yet */
    win->nxt = 0;

    /* initialize with default colors */
    win->txtfg = OSGEN_COLOR_TEXT;
    win->txtbg = OSGEN_COLOR_TRANSPARENT;
    win->txtattr = 0;
    win->fillcolor = OSGEN_COLOR_TEXTBG;

    /* cache the oss translation of the fill color */
    win->oss_fillcolor = ossgetcolor(OSGEN_COLOR_TEXT, OSGEN_COLOR_TEXTBG,
                                     0, 0);

    /* start at the upper left corner */
    win->x = 0;
    win->y = 0;
    win->scrollx = 0;
    win->scrolly = 0;

    /* 
     *   the window's position on screen will eventually be set by
     *   osgen_recalc_layout(), but initialize the position to a reasonable
     *   value for now in case anyone looks at it before then 
     */
    win->winx = 0;
    win->winy = 0;

    /* we haven't seen any text in the window yet */
    win->xmax = 0;
    win->ymax = 0;

    /* clear the flags */
    win->flags = 0;

    /* remember our parent */
    win->parent = parent;

    /* we have no children yet */
    win->first_child = 0;

    /* if there's a parent, insert the window into the parent's child list */
    if (parent != 0)
    {
        /* insert into the parent's child list at the proper point */
        switch(where)
        {
        case OS_BANNER_FIRST:
            /* link it at the head of the list */
            win->nxt = parent->first_child;
            parent->first_child = win;
            break;

        case OS_BANNER_LAST:
        default:
            /* find the end of the parent's list */
            for (cur = parent->first_child ; cur != 0 && cur->nxt != 0 ;
                 cur = cur->nxt) ;

            /* link it after the last element */
            win->nxt = 0;
            if (cur != 0)
                cur->nxt = win;
            else
                parent->first_child = win;

            /* done */
            break;

        case OS_BANNER_BEFORE:
        case OS_BANNER_AFTER:
            /* scan the parent's child list, looking for 'other' */
            for (prv = 0, cur = parent->first_child ;
                 cur != 0 && cur != other ;
                 prv = cur, cur = cur->nxt) ;

            /* 
             *   if we didn't find 'other', link the new window at the tail
             *   of the list by default; since 'prv' will be the last item if
             *   we didn't find 'other', we can simply set the link mode to
             *   'before' to link before the placeholder null at the end of
             *   the list 
             */
            if (cur == 0)
                where = OS_BANNER_BEFORE;

            /* if we're linking after 'cur', advance one position */
            if (where == OS_BANNER_AFTER)
            {
                prv = cur;
                cur = cur->nxt;
            }

            /* link before 'cur', which is right after 'prv' */
            win->nxt = cur;
            if (prv != 0)
                prv->nxt = win;
            else
                parent->first_child = win;

            /* done */
            break;
        }
    }

    /* return the new window */
    return win;
}

/*
 *   Create a text window 
 */
static osgen_txtwin_t *osgen_create_txtwin(int where, void *other,
                                           void *parent,
                                           unsigned int buf_size,
                                           unsigned int buf_lines)
{
    osgen_txtwin_t *win;
    
    /* create the base window */
    win = (osgen_txtwin_t *)osgen_create_win(OS_BANNER_TYPE_TEXT,
                                             where, other, parent);

    /* if that failed, give up now */
    if (win == 0)
        return 0;

    /* allocate the window's buffer */
    win->txtbufsiz = buf_size;
    win->txtbuf = (char *)osmalloc(buf_size);

    /* allocate the line starts */
    win->line_ptr_cnt = buf_lines;
    win->line_ptr = (char **)osmalloc(buf_lines * sizeof(char *));

    /* make sure we allocated everything properly */
    if (win->txtbuf == 0 || win->line_ptr == 0)
    {
        /* free anything we allocated */
        osgen_delete_win(&win->base);
        
        /* return failure */
        return 0;
    }

    /* set up the buffer free pointer */
    win->txtfree = win->txtbuf;

    /* start out with a single line in the buffer */
    win->line_count = 1;

    /* set up the first line start */
    win->first_line = 0;
    win->line_ptr[0] = win->txtbuf;

    /* initialize the start-of-line colors */
    win->solfg = win->base.txtfg;
    win->solbg = win->base.txtbg;
    win->solattr = win->base.txtattr;

    /* start the new first line with the current text color */
    osssb_add_color_code(win);

    /* return the new window */
    return win;
}

/*
 *   Create a text grid window 
 */
static osgen_gridwin_t *osgen_create_gridwin(int where, void *other,
                                             void *parent,
                                             int grid_wid, int grid_ht)
{
    osgen_gridwin_t *win;

    /* create the base window */
    win = (osgen_gridwin_t *)osgen_create_win(OS_BANNER_TYPE_TEXTGRID,
                                              where, other, parent);

    /* if that failed, give up now */
    if (win == 0)
        return 0;
    
    /* allocate the grid to the requested size */
    win->grid_wid = grid_wid;
    win->grid_ht = grid_ht;
    win->grid_txt = (char *)osmalloc(grid_wid * grid_ht);
    win->grid_color = (osgen_charcolor_t *)osmalloc(
        grid_wid * grid_ht * sizeof(osgen_charcolor_t));

    /* if we failed to allocate, delete the window and abort */
    if (win->grid_txt == 0 || win->grid_color == 0)
    {
        /* free anything we allocated */
        osgen_delete_win(&win->base);
        
        /* return failure */
        return 0;
    }

    /* clear the grid text and color arrays */
    osgen_gridwin_clear(win, 0, win->grid_wid * win->grid_ht);

    /* return the new window */
    return win;
}

/*
 *   Clear a window 
 */
static void osgen_clear_win(osgen_win_t *win)
{
    osgen_txtwin_t *twin;
    osgen_gridwin_t *gwin;

    /* check the type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        /* get the text window subclass data */
        twin = (osgen_txtwin_t *)win;

        /* reset the window's scrollback buffer */
        twin->txtfree = twin->txtbuf;
        twin->line_count = 1;
        twin->first_line = 0;
        twin->line_ptr[0] = twin->txtbuf;

        /* start the new first line with the current text color */
        osssb_add_color_code(twin);

        /* done */
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* get the grid window subclass data */
        gwin = (osgen_gridwin_t *)win;

        /* clear the entire grid */
        osgen_gridwin_clear(gwin, 0, gwin->grid_wid * gwin->grid_ht);

        /* done */
        break;
    }

    /* put the cursor at the top left of the window */
    win->x = 0;
    win->y = 0;

    /* set the scrolling position to the top of the window */
    win->scrollx = 0;
    win->scrolly = 0;

    /* the content size is now zero */
    win->xmax = 0;
    win->ymax = 0;

    /* if the buffer was full, it's full no longer */
    win->flags &= ~OSGEN_FULL;

    /*
     *   If this window is not auto-scrolling, don't actually clear it
     *   visually right now, but just set a deferred-redraw on the window;
     *   clearing a window that doesn't automatically scroll usually means
     *   that we're updating some live status information, so we use deferred
     *   drawing on these windows to reduce flicker during updates.  If the
     *   window automatically scrolls, then the caller presumably is
     *   displaying more sizable data, and thus would want the user to see
     *   the text output as it occurs.
     *   
     *   Note that both approaches (deferred vs immediate redraw) yield the
     *   same results in the end, because we always stop deferring redraws
     *   when we stop to ask for input or simply pause for a time delay; the
     *   only difference is that deferred redrawing reduces flicker by
     *   gathering all the updates into a single operation, while immediate
     *   update might flicker more but shows individual bits of text output
     *   as they occur.  
     */
    if (!(win->flags & OSGEN_AUTO_VSCROLL))
    {
        /* defer redrawing until we need to */
        win->flags |= OSGEN_DEFER_REDRAW;
    }
    else
    {
        /* clear the window's on-screen area with its fill color */
        ossclr(win->winy, win->winx,
               win->winy + win->ht - 1, win->winx + win->wid - 1,
               win->oss_fillcolor);
    }
}

/*
 *   Display text in a window, all in a single color, truncating the display
 *   if we start before the left edge of the window or go past the right
 *   edge of the window.  The x,y coordinates are given in window
 *   coordinates.  
 */
static void osgen_disp_trunc(osgen_win_t *win, int y, int x, int oss_color,
                             const char *p)
{
    char buf[64];
    size_t chars_rem;
    size_t wid_rem;

    /* if we have deferred redrawing pending, don't bother drawing now */
    if (S_deferred_redraw || (win->flags & OSGEN_DEFER_REDRAW) != 0)
        return;

    /* get the number of characters to be displayed */
    chars_rem = strlen(p);

    /* calculate the amount of space we have to work with on the screen */
    wid_rem = win->wid - (x >= 0 ? x : 0);

    /* make sure at least some of the text overlaps the window */
    if (y < 0 || y >= (int)win->ht
        || x + (int)chars_rem <= 0 || x >= (int)win->wid)
        return;

    /* 
     *   if we're starting to the left of the window, skip characters up to
     *   the first character visible in the window 
     */
    if (x < 0)
    {
        /* 
         *   get the number of characters to skip - this is simply the
         *   negative of the x position, since x position zero is the first
         *   column to display 
         */
        x = -x;

        /* 
         *   if we don't have enough characters to reach the left edge of
         *   the window, we have nothing to do 
         */
        if (chars_rem <= (size_t)x)
            return;

        /* skip the desired number of characters */
        chars_rem -= x;
        p += x;

        /* we've skipped up to column zero, so proceed from there */
        x = 0;
    }

    /* if the text entirely fits, display it and we're done */
    if (chars_rem < wid_rem)
    {
        /* display the entire string */
        ossdsp(win->winy + y, win->winx + x, oss_color, p);

        /* done */
        return;
    }

    /* 
     *   we have too much to display, so display as much as will fit - keep
     *   going until we run out of space for the display (we know we'll run
     *   out of space before we run out of text, because we know we have too
     *   much text to fit) 
     */
    while (wid_rem != 0)
    {
        size_t cur;

        /* display as much as will fit, up to our buffer length */
        cur = wid_rem;
        if (cur > sizeof(buf) - 1)
            cur = sizeof(buf) - 1;

        /* copy this portion to our buffer and null-terminate it */
        memcpy(buf, p, cur);
        buf[cur] = '\0';

        /* display it */
        ossdsp(win->winy + y, win->winx + x, oss_color, buf);

        /* advance our screen position */
        x += cur;

        /* advance past the source material we just displayed */
        p += cur;

        /* deduct the on-screen space we consumed from the remaining space */
        wid_rem -= cur;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the scrollback buffer.  This initializes the main text area
 *   window.
 *   
 *   The oss code should call this during initialization to set up the
 *   osgen3-layer display management; note that this routine must be called
 *   from the oss code AFTER the screen size has been determined and stored
 *   in the global variables G_oss_screen_width and G_oss_screen_height.  
 */
void osssbini(unsigned int size)
{
    osgen_txtwin_t *win;

    /* allocate our main window if we're not in 'plain' mode */
    if (!os_f_plain)
    {
        /* 
         *   Initialize the main window.  The main window has no parent,
         *   since it's the root of the window tree.  
         */
        S_main_win = win = osgen_create_txtwin(
            OS_BANNER_FIRST, 0, 0, size, size/40);
        if (win == 0)
        {
            /* show a message and give up */
            printf("There is not enough memory to run this program.\n");
            exit(1);
        }
        
        /* make this the default window */
        S_default_win = win;
        
        /* scroll to keep the cursor visible when writing to this window */
        win->base.flags |= OSGEN_AUTO_VSCROLL;
        
        /* initially give the window the entire screen */
        win->base.winy = 0;
        win->base.winx = 0;
        win->base.wid = G_oss_screen_width;
        win->base.ht = G_oss_screen_height;
        
        /* set the initial page size */
        G_os_linewidth = win->base.wid;
        G_os_pagelength = (win->base.ht > 2
                           ? win->base.ht - 2
                           : win->base.ht > 1
                           ? win->base.ht - 1 : win->base.ht);

        /* set the initial color scheme to the default text color */
        os_set_text_color(OS_COLOR_P_TEXT, OS_COLOR_P_TRANSPARENT);
        os_set_screen_color(OS_COLOR_P_TEXTBG);

        /* clear the window */
        osgen_clear_win(&win->base);
    }
    else
    {
        /* use the entire available display */
        G_os_linewidth = G_oss_screen_width;
        G_os_pagelength = G_oss_screen_height - 2;
    }
}

/*
 *   Delete the scrollback buffer.  The oss code should call this during
 *   program termination to free memory allocated by the osgen3 layer.  
 */
void osssbdel(void)
{
    /* delete the main window and its children */
    if (S_main_win != 0)
        osgen_delete_win_tree(&S_main_win->base);
}

/* ------------------------------------------------------------------------ */
/*
 *   Scrollback operations.  Scrollback applies only to ordinary text
 *   windows.  
 */

/*
 *   advance a pointer in a scrollback buffer 
 */
static char *ossadvsp(osgen_txtwin_t *win, char *p)
{
    /* move to the next byte */
    ++p;

    /* if we've passed the end of the buffer, wrap to the beginning */
    if (p >= win->txtbuf + win->txtbufsiz)
        p = win->txtbuf;

    /* return the pointer */
    return p;
}

/*
 *   decrement a scrollback buffer pointer 
 */
static char *ossdecsp(osgen_txtwin_t *win, char *p)
{
    /* if we're at the start of the buffer, wrap to just past the end */
    if (p == win->txtbuf)
        p = win->txtbuf + win->txtbufsiz;

    /* move to the previous byte */
    --p;

    /* return the pointer */
    return p;
}

/*
 *   Get the line pointer for the given line 
 */
static char **osgen_get_line_ptr(osgen_txtwin_t *win, int y)
{
    size_t idx;

    /* if the line doesn't exist, return null */
    if (y < 0 || (size_t)y >= win->line_count)
        return 0;

    /* add the line number to the base index to get the linear index */
    idx = y + win->first_line;

    /* wrap back to the start of the array if necessary */
    if (idx >= win->line_ptr_cnt)
        idx -= win->line_ptr_cnt;

    /* return the line at this index */
    return &win->line_ptr[idx];
}

/*
 *   Get a pointer to the text of the line in the given window at the given
 *   document y coordinate.  
 */
static char *osgen_get_line(osgen_txtwin_t *win, int y)
{
    char **line_ptr;

    /* get the line pointer for the given line */
    line_ptr = osgen_get_line_ptr(win, y);

    /* return the text it points to, or null if there's no such line */
    return (line_ptr != 0 ? *line_ptr : 0);
}

/*
 *   Add a byte to the scrollback buffer.  Returns true if the byte was
 *   successfully added, false if not.  
 */
static int osssb_add_byte(osgen_txtwin_t *win, char c)
{
    char *nxtfree;

    /* if the window is full, indicate failure */
    if ((win->base.flags & OSGEN_FULL) != 0)
        return FALSE;

    /* 
     *   If there's no room, and we're not in auto-vscroll mode, simply
     *   discard the text.  A window that is not in auto-vscroll mode is
     *   designed to always show the oldest text, so if we overflow the
     *   buffer, then we want to start discarding the newest test.  
     */
    nxtfree = ossadvsp(win, win->txtfree);
    if (nxtfree == win->line_ptr[win->first_line]
        && !(win->base.flags & OSGEN_AUTO_VSCROLL))
    {
        /* 
         *   The buffer is full, and we're not in auto-vscroll mode -
         *   discard the new text.  Before we do, set the current last byte
         *   to null, to terminate the current line at this point.  
         */
        *win->txtfree = '\0';

        /* set the 'full' flag */
        win->base.flags |= OSGEN_FULL;

        /* indicate failure */
        return FALSE;
    }

    /* add the character to the buffer */
    *win->txtfree = c;

    /* advance the free pointer */
    win->txtfree = nxtfree;

    /* 
     *   if the free pointer has just collided with the start of the oldest
     *   line, delete the oldest line 
     */
    if (win->txtfree == win->line_ptr[win->first_line])
    {
        /* delete the oldest line to make room for the new data */
        win->first_line++;
        if (win->first_line >= win->line_ptr_cnt)
            win->first_line = 0;

        /* we have one less line in the buffer */
        win->line_count--;

        /* adjust our document cooordinates for the loss of a line */
        win->base.y--;

        /* 
         *   Adjust the scrolling position for the loss of a line.  If the
         *   window is displaying the line that we just deleted (i.e., the
         *   window's vertical scroll offset is zero), we must redraw the
         *   window; otherwise, the change won't affect the display, so we
         *   can merely adjust the scrolling offset.  
         */
        if (win->base.scrolly == 0)
            win->base.flags |= OSGEN_DEFER_REDRAW;
        else
            win->base.scrolly--;
    }

    /* we successfully added the byte */
    return TRUE;
}

/*
 *   Add a given number of bytes.  The bytes will be added as a unit - if we
 *   can't add all of the bytes, we won't add any of them.  
 */
static int osssb_add_bytes(osgen_txtwin_t *win, const char *p, size_t len)
{
    char *orig_free;

    /* note the original free pointer */
    orig_free = win->txtfree;

    /* add the bytes */
    for ( ; len != 0 ; --len)
    {
        /* try adding this byte */
        if (!osssb_add_byte(win, *p++))
        {
            /* 
             *   failure - forget everything we've added by resetting the
             *   free pointer to its original value on entry 
             */
            win->txtfree = orig_free;

            /* indicate failure */
            return FALSE;
        }
    }

    /* we added all of the bytes successfully */
    return TRUE;
}

/*
 *   Add a "safety" null byte.  This adds a null byte at the end of the
 *   buffer, in case we need to inspect the current line before we add any
 *   more text.  
 */
static void osssb_add_safety_null(osgen_txtwin_t *win)
{
    /* add the null */
    if (osssb_add_byte(win, '\0'))
    {
        /* 
         *   we added the null - decrement the free pointer so we overwrite
         *   the null byte if we add any more text to the current line 
         */
        win->txtfree = ossdecsp(win, win->txtfree);
    }
}

/*
 *   Add an appropriate color code to the scrollback buffer to yield the
 *   current color setting.  
 */
static void osssb_add_color_code(osgen_txtwin_t *win)
{
    char buf[10];

    /* if we have any attributes, add an attribute code */
    if (win->base.txtattr != 0)
    {
        /* add the attribute code sequence */
        buf[0] = OSGEN_ATTR;
        buf[1] = (char)win->base.txtattr;
        osssb_add_bytes(win, buf, 2);
    }

    /* if we're using the plain text color, add a color code */
    if (win->base.txtfg != OSGEN_COLOR_TEXT
        || win->base.txtbg != OSGEN_COLOR_TRANSPARENT)
    {
        /* add the explicit color code */
        buf[0] = OSGEN_COLOR;
        buf[1] = win->base.txtfg;
        buf[2] = win->base.txtbg;
        osssb_add_bytes(win, buf, 3);
    }

    /* add a safety null terminator */
    osssb_add_safety_null(win);
}

/*
 *   Start a new line in the scrollback buffer.  Returns true on success,
 *   false if there's no space to add the newline character.  
 */
static int osssb_new_line(osgen_txtwin_t *win)
{
    char **line_ptr;

    /* add a null byte to mark the end of the current line */
    if (!osssb_add_byte(win, '\0'))
        return FALSE;

    /* if the window is full, indicate failure */
    if ((win->base.flags & OSGEN_FULL) != 0)
        return FALSE;

    /* 
     *   If the line buffer is full, and this window doesn't automatically
     *   scroll vertically, drop the new text.  When we're not in
     *   auto-vscroll mode, we drop text from the end of the buffer rather
     *   than from the beginning.  
     */
    if (win->line_count + 1 > win->line_ptr_cnt
        && !(win->base.flags & OSGEN_AUTO_VSCROLL))
    {
        /* set the 'full' flag */
        win->base.flags |= OSGEN_FULL;

        /* indicate failure */
        return FALSE;
    }

    /* move to the left column */
    win->base.x = 0;

    /* add a line start */
    if (win->line_count >= win->line_ptr_cnt)
    {
        /* forget the original first line */
        win->first_line++;
        if (win->first_line == win->line_ptr_cnt)
            win->first_line = 0;

        /* 
         *   Since we took away one line and we're adding another, the total
         *   line count and the document 'y' position aren't changing - but
         *   the line at the document 'y' position now belongs to the new
         *   last line.
         *   
         *   Adjust the scrolling position for the loss of a line at the
         *   start of the buffer.  This doesn't change what we're displaying
         *   on the screen, because we're simply reducing the amount of text
         *   in the buffer before the top of the screen, so we must
         *   compensate by decrementing the document coordinate of the top
         *   line in the window.  However, if the line we're taking away is
         *   showing in the window, then we do need to redraw the contents,
         *   because the change will be visible; set the deferred redraw
         *   flag for the window in this case so that we eventually redraw
         *   the whole thing.  
         */
        if (win->base.scrolly == 0)
            win->base.flags |= OSGEN_DEFER_REDRAW;
        else
            win->base.scrolly--;
    }
    else
    {
        /* count the new line */
        win->line_count++;

        /* note the highest output 'y' position if this is it */
        if (win->base.y > win->base.ymax)
            win->base.ymax = win->base.y;

        /* increase the document y position for the new line */
        win->base.y++;
    }

    /* get the line pointer for the new last line */
    line_ptr = osgen_get_line_ptr(win, win->base.y);

    /* 
     *   set the new line pointer to point to the first free byte of the
     *   buffer 
     */
    *line_ptr = win->txtfree;

    /* add a color code to the start of the new line, if necessary */
    osssb_add_color_code(win);

    /* remember the text color at the start of this line */
    win->solfg = win->base.txtfg;
    win->solbg = win->base.txtbg;
    win->solattr = win->base.txtattr;

    /* add a safety null terminator */
    osssb_add_safety_null(win);

    /* success */
    return TRUE;
}

/*
 *   Scroll the window forward by the given number of lines 
 */
static void osgen_scroll_win_fwd(osgen_txtwin_t *win, size_t lines)
{
    /* limit the scrolling to the available number of lines */
    if (lines > win->line_count - win->base.scrolly - win->base.ht)
        lines = win->line_count - win->base.scrolly - win->base.ht;

    /* 
     *   If we're scrolling more than a few lines, or more than the entire
     *   window's height, just redraw the entire window.  If it's a small
     *   number of lines, scroll the screen one line at a time, so that we
     *   don't have to redraw as much.  
     */
    if (lines > 3 || (int)lines >= win->base.ht)
    {
        /* adjust the scroll position */
        win->base.scrolly += lines;

        /* redraw the entire window */
        osgen_redraw_win(&win->base);
    }
    else
    {
        /* scroll one line at a time */
        for ( ; lines != 0 ; --lines)
        {
            /* scroll the window's on-screen area one line */
            ossscr(win->base.winy, win->base.winx,
                   win->base.winy + win->base.ht - 1,
                   win->base.winx + win->base.wid - 1,
                   win->base.oss_fillcolor);

            /* adjust the scroll position */
            win->base.scrolly++;

            /* redraw the bottom line */
            osgen_scrdisp(&win->base, 0, win->base.ht - 1, win->base.wid);
        }
    }
}

/*
 *   Scroll the window back by the given number of lines 
 */
static void osgen_scroll_win_back(osgen_txtwin_t *win, int lines)
{
    /* limit scrolling to the available number of lines above the window */
    if (lines > win->base.scrolly)
        lines = win->base.scrolly;

    /* 
     *   If we're scrolling more than a few lines, or by the full window
     *   height or more, just redraw the entire window.  Otherwise, scroll a
     *   line at a time to minimize redrawing.  
     */
    if (lines > 3)
    {
        /* adjust the scroll position */
        win->base.scrolly -= lines;

        /* redraw the entire window */
        osgen_redraw_win(&win->base);
    }
    else
    {
        /* scroll up by one line at a time */
        for ( ; lines != 0 ; --lines)
        {
            /* scroll the window's on-screen area one line */
            ossscu(win->base.winy, win->base.winx,
                   win->base.winy + win->base.ht - 1,
                   win->base.winx + win->base.wid - 1,
                   win->base.oss_fillcolor);

            /* adjust the scroll position */
            win->base.scrolly--;

            /* redraw the top line */
            osgen_scrdisp(&win->base, 0, 0, win->base.wid);
        }
    }
}

/*
 *   If a window is in auto-vscroll mode, bring the cursor into view in the
 *   window. 
 */
static void osgen_auto_vscroll(osgen_txtwin_t *win)
{
    /* if the window isn't in auto-vscroll mode, ignore this */
    if (!(win->base.flags & OSGEN_AUTO_VSCROLL))
        return;
    
    /* 
     *   scroll the window forward if the cursor is outside the visible area
     *   of the window 
     */
    if (win->base.y >= win->base.scrolly + (int)win->base.ht)
        osgen_scroll_win_fwd(win, win->base.y
                             - (win->base.scrolly + win->base.ht) + 1);
}

/*
 *   Add text to the scrollback buffer. 
 */
static void ossaddsb(osgen_txtwin_t *win, const char *p, size_t len, int draw)
{
    int startx;
    int starty;
    int line_len;

    /* if there's no scrollback buffer, ignore it */
    if (win->txtbuf == 0)
        return;

    /* note the starting x,y position, for redrawing purposes */
    startx = win->base.x - win->base.scrollx;
    starty = win->base.y - win->base.scrolly;

    /* we haven't added any characters to the line yet */
    line_len = 0;

    /*
     *   Copy the text into the screen buffer, respecting the circular
     *   nature of the screen buffer.  If the given text wraps lines,
     *   enter an explicit carriage return into the text to ensure that
     *   users can correctly count lines.
     */
    while (len != 0)
    {
        /* check what we have */
        switch(*p)
        {
        case OSGEN_ATTR:
            /* switch to the new attributes */
            win->base.txtattr = (unsigned char)*(p+1);

            /* 
             *   if we're at the start of the line, this is the start-of-line
             *   attribute - this will ensure that if we back up with '\r',
             *   we'll re-apply this attribute change 
             */
            if (win->base.x == 0)
                win->solattr = win->base.txtattr;

            /* add the two-byte sequence */
            osssb_add_bytes(win, p, 2);
            p += 2;
            len -= 2;
            break;

        case OSGEN_COLOR:
            /* switch to the new colors */
            win->base.txtfg = (unsigned char)*(p+1);
            win->base.txtbg = (unsigned char)*(p+2);

            /* 
             *   if we're at the start of the line, this is the new
             *   start-of-line color 
             */
            if (win->base.x == 0)
            {
                win->solfg = win->base.txtfg;
                win->solbg = win->base.txtbg;
            }

            /* add and skip the three-byte color sequence */
            osssb_add_bytes(win, p, 3);
            p += 3;
            len -= 3;
            break;
            
        case '\n':
            /* add the new line */
            osssb_new_line(win);

            /* draw the part of the line we added */
            if (draw)
                osgen_scrdisp(&win->base, startx, starty, line_len);

            /* skip this character */
            ++p;
            --len;

            /* reset to the next line */
            startx = win->base.x - win->base.scrollx;
            ++starty;
            line_len = 0;

            /* bring the cursor onto the screen if auto-vscrolling */
            if (draw)
                osgen_auto_vscroll(win);

            /* done */
            break;

        case '\r':
            /*
             *   We have a plain carriage return, which indicates that we
             *   should go back to the start of the current line and
             *   overwrite it.  (This is most likely to occur for the
             *   "[More]" prompt.)
             */

            /* set the free pointer back to the start of the current line */
            win->txtfree = osgen_get_line(win, win->base.y);

            /* switch back to the color as of the start of the line */
            win->base.txtfg = win->solfg;
            win->base.txtbg = win->solbg;
            win->base.txtattr = win->solattr;

            /* 
             *   since we're back at the start of the line, add a color code
             *   if necessary 
             */
            osssb_add_color_code(win);

            /* clear the window area of the line we're overwriting */
            if (draw && starty >= 0 && starty < (int)win->base.ht)
                ossclr(win->base.winy + starty, win->base.winx,
                       win->base.winy + starty,
                       win->base.winx + win->base.wid - 1,
                       win->base.oss_fillcolor);

            /* reset to the start of the line */
            line_len = 0;
            startx = 0;

            /* move to the left column */
            win->base.x = 0;

            /* skip this character */
            ++p;
            --len;

            /* done */
            break;

        default:
            /* for anything else, simply store the byte in the buffer */
            if (osssb_add_byte(win, *p))
            {
                /* note the maximum output position if appropriate */
                if (win->base.x > win->base.xmax)
                    win->base.xmax = win->base.x;

                /* adjust the output x position */
                win->base.x++;

                /* count the ordinary character in the display length */
                ++line_len;
            }
            
            /* skip the character */
            ++p;
            --len;
                
            /* done */
            break;
        }
    }

    /* 
     *   Add a safety null terminator after each addition, in case we need
     *   to look at the buffer before this line is finished.  
     */
    osssb_add_safety_null(win);

    /* display the added text */
    if (draw)
    {
        /* show the added text */
        osgen_scrdisp(&win->base, startx, starty, line_len);

        /* bring the cursor onto the screen if auto-vscrolling */
        osgen_auto_vscroll(win);
    }
}

/*
 *   Add an input line to the scrollback buffer.
 */
void ossaddsb_input(osgen_txtwin_t *win, char *p, int add_nl)
{
    size_t chars_rem;
    
    /* get the number of characters in the input to add */
    chars_rem = strlen(p);

    /* 
     *   add the input in chunks, wrapping whenever we reach the right edge
     *   of the window 
     */
    while (chars_rem != 0)
    {
        size_t wid_rem;
        size_t cur;
        
        /* if we're already past the edge, add a newline */
        if (win->base.x >= (int)win->base.wid)
            osssb_new_line(win);

        /* figure out how much space we have */
        wid_rem = win->base.wid - win->base.x;

        /* add as much as we can, up to the width remaining */
        cur = chars_rem;
        if (cur > wid_rem)
            cur = wid_rem;

        /* add the text */
        ossaddsb(win, p, cur, FALSE);

        /* skip this chunk */
        p += cur;
        chars_rem -= cur;
    }

    /* add a newline after the input if desired */
    if (add_nl)
        osssb_new_line(win);
    else
        osssb_add_safety_null(win);
}

/* ------------------------------------------------------------------------ */
/*
 *   End scrollback mode. 
 */
static void osgen_sb_mode_end()
{
    osgen_txtwin_t *win = S_sbmode_win;
    
    /* restore our original scroll position */
    if (win->base.scrolly > S_sbmode_orig_scrolly)
        osgen_scroll_win_back(win, win->base.scrolly - S_sbmode_orig_scrolly);
    else if (win->base.scrolly < S_sbmode_orig_scrolly)
        osgen_scroll_win_fwd(win, S_sbmode_orig_scrolly - win->base.scrolly);

    /* 
     *   adjust the input editing cursor position if the scrolling position
     *   doesn't match the original position 
     */
    S_gets_y += S_sbmode_orig_scrolly - win->base.scrolly;
    
    /* 
     *   restore our full window area, adding back in the top line that we
     *   removed to make room for the mode line 
     */
    win->base.winy -= 1;
    win->base.ht += 1;
    win->base.scrolly -= 1;
    
    /* redraw the top line, where we drew the mode line */
    ossclr(win->base.winy, win->base.winx,
           win->base.winy, win->base.winx + win->base.wid - 1,
           win->base.oss_fillcolor);
    osgen_scrdisp(&win->base, 0, 0, win->base.wid);

    /* 
     *   Delete the temporary copy of the input from the scrollback buffer.
     *   To do this, restore the x,y position and free pointer, then delete
     *   extra lines from the end of the buffer until the last line points
     *   to the same text it did before we added the command input text.  
     */
    win->base.x = S_sbmode_orig_x;
    win->txtfree = S_sbmode_orig_txtfree;
    while (win->line_count > 1
           && osgen_get_line_ptr(win, win->base.y) != S_sbmode_orig_last_line)
    {
        /* this is an added line - delete the line */
        win->line_count--;
        win->base.y--;
    }

    /* we're no longer in scrollback mode */
    S_sbmode_win = 0;
}

/*
 *   Draw the scrollback mode status line.  This is a line we draw at the top
 *   of the window to give a visual indication that we're in scrollback mode.
 *   (Actually, we draw this one line above the current top of the window,
 *   because we shrink the window vertically by one line while it's in
 *   scrollback mode specifically to make room for the mode line.)  
 */
static void osgen_draw_sb_mode_line(void)
{
    osgen_txtwin_t *win;
    int oss_color;

    /* get the current scrollback window */
    win = S_sbmode_win;

    /* there's nothing to do if there's no scrollback window */
    if (win == 0)
        return;

    /* get the color - use the reverse of the window's current color scheme */
    oss_color = ossgetcolor(win->base.fillcolor, win->base.txtfg, 0,
                            OSGEN_COLOR_TRANSPARENT);

    /* clear the area of the mode line */
    ossclr(win->base.winy - 1, win->base.winx,
           win->base.winy - 1, win->base.winx + win->base.wid - 1, oss_color);

    /* momentarily bring the mode line back into the window's area */
    win->base.winy -= 1;
    win->base.ht += 1;

    /* draw the mode line text */
    osgen_disp_trunc(&win->base, 0, 0, oss_color, OS_SBSTAT);

    /* undo our temporary adjustment of the window size */
    win->base.winy += 1;
    win->base.ht -= 1;
}

/*
 *   Process the event in scrollback mode.  If we're not already in
 *   scrollback mode, we'll enter scrollback mode; otherwise, we'll process
 *   the event in the already running scrollback.
 *   
 *   Returns true if we processed the event, false if we have terminated
 *   scrollback mode and want the caller to process the event as it normally
 *   would outside of scrollback mode.  
 */
static int osgen_sb_mode(osgen_txtwin_t *win, int event_type,
                         os_event_info_t *event_info)
{
    int ret;
    char c;
    int just_started;

    /* if we were doing scrollback in a different window, cancel it */
    if (S_sbmode_win != 0 && S_sbmode_win != win)
        osgen_sb_mode_end();

    /* note if we're just entering scrollback mode */
    just_started = (S_sbmode_win == 0);

    /* if scrollback mode isn't already in progress, set it up */
    if (just_started)
    {
        /* if there's no buffer, we can't enter scrollback mode */
        if (win->txtbuf == 0)
            return FALSE;

        /* 
         *   If the number of lines in the scrollback buffer doesn't exceed
         *   the number of lines displayed in the window, then don't bother
         *   entering scrollback mode.  If the window isn't at least two
         *   lines high, don't allow it either, as we need space to draw our
         *   mode line.  
         */
        if (win->line_count <= win->base.ht || win->base.ht < 2)
            return FALSE;

        /* remember the window involved in scrollback */
        S_sbmode_win = win;
        
        /* 
         *   Shrink the window one line at the top, to make room for the mode
         *   line.  Compensate by bumping the scrolling position forward a
         *   line, to keep the lines that will remain visible in the rest of
         *   the window at the same place on the screen.  
         */
        win->base.winy += 1;
        win->base.ht -= 1;
        win->base.scrolly += 1;

        /*
         *   Temporarily add the command buffer to the scrollback buffer, so
         *   that it shows up when we're scrolling.  To facilitate removing
         *   the added text later, remember the current free pointer, last
         *   line pointer, x document position.  
         */
        S_sbmode_orig_x = win->base.x;
        S_sbmode_orig_txtfree = win->txtfree;
        S_sbmode_orig_last_line = osgen_get_line_ptr(win, win->base.y);
        ossaddsb_input(win, S_gets_buf, FALSE);

        /* remember the original scrolling position */
        S_sbmode_orig_scrolly = win->base.scrolly;

        /* draw the mode line */
        osgen_draw_sb_mode_line();
    }

    /* presume we'll handle the event */
    ret = TRUE;

    /* check the event */
    switch(event_type)
    {
    case OS_EVT_KEY:
        /* 
         *   If it's a regular key, exit scrollback mode immediately, and
         *   let the caller handle the event normally.  This allows the user
         *   to resume typing even while scrollback is active; we'll simply
         *   cancel the scrollback and resume editing with the event that
         *   canceled the scrollback.  
         */
        c = event_info->key[0];
        if ((unsigned char)c >= 32 || c == '\n' || c == '\r' || c == 8)
        {
            /* it's a regular key - terminate scrollback mode */
            osgen_sb_mode_end();

            /* let the caller handle the event */
            ret = FALSE;
        }

        /* if it's the escape key, we're done */
        if (c == 27)
        {
            osgen_sb_mode_end();
            break;
        }

        /* if it's not a special keystroke, we're done with it */
        if (c != '\0')
            break;

        /* it's a special keystroke - check the command ID */
        switch(event_info->key[1])
        {
        case CMD_SCR:
            /* if we're not just toggling in to scrollback mode, toggle out */
            if (!just_started)
                osgen_sb_mode_end();
            break;
            
        case CMD_KILL:
        case CMD_EOF:
            /* it's a termination key - cancel scrollback mode */
            osgen_sb_mode_end();
            break;

        case CMD_UP:
            /* scroll back a line */
            osgen_scroll_win_back(win, 1);
            break;

        case CMD_DOWN:
            /* scroll forward a line */
            osgen_scroll_win_fwd(win, 1);
            break;
            
        case CMD_PGUP:
            /* move back by one window height */
            osgen_scroll_win_back(win, win->base.ht > 1
                                  ? win->base.ht - 1 : 1);
            break;
            
        case CMD_PGDN:
            /* move forward by one window height */
            osgen_scroll_win_fwd(win, win->base.ht > 1
                                 ? win->base.ht - 1 : 1);
            break;
        }

        /* done with the key */
        break;

    default:
        /* 
         *   ignore any other events we receive, but consider them processed
         *   - even though we're ignoring the event, we're deliberately
         *   ignoring the event as our way of processing it 
         */
        break;
    }

    /* return the 'processed' indication to the caller */
    return ret;
}

/*
 *   Display a line of text in the given ordinary text window 
 */
static void osgen_scrdisp_txt(osgen_txtwin_t *win, int x, int y, int len)
{
    int fg;
    int bg;
    int attr;
    int oss_color;
    char buf[64];
    char *bufp;
    char *p;
    int scanx;

    /* start out in ordinary text mode */
    fg = OSGEN_COLOR_TEXT;
    bg = OSGEN_COLOR_TRANSPARENT;
    attr = 0;
    oss_color = ossgetcolor(fg, bg, attr, win->base.fillcolor);

    /* get a pointer to the start of the desired line */
    p = osgen_get_line(win, y + win->base.scrolly);
    if (p == 0)
        return;

    /* get the window-relative x coordinate of the start of the line */
    scanx = -win->base.scrollx;

    /* 
     *   Scan the line.  We must scan from the start of the line, even if we
     *   don't want to display from the start of the line, to make sure we
     *   take into account any color and attribute settings stored in the
     *   line.  
     */
    for (bufp = buf ; *p != '\0' && len != 0 ; p = ossadvsp(win, p))
    {
        /* check for special escape codes */
        switch(*p)
        {
        case OSGEN_ATTR:
            /* skip the escape byte */
            p = ossadvsp(win, p);

            /* get the new attribute code */
            attr = *p;

            /* set the new colors */
            goto change_color;

        case OSGEN_COLOR:
            /* skip the escape byte */
            p = ossadvsp(win, p);

            /* get the foreground color and skip it */
            fg = *p;
            p = ossadvsp(win, p);

            /* get the background color */
            bg = *p;
            goto change_color;

        change_color:
            /* flush the buffer */
            if (bufp != buf)
            {
                /* display the contents of the buffer */
                *bufp = '\0';
                osgen_disp_trunc(&win->base, y, x, oss_color, buf);

                /* adjust the column position for the display */
                x += bufp - buf;

                /* reset the buffer */
                bufp = buf;
            }

            /* translate the new text color */
            oss_color = ossgetcolor(fg, bg, attr, win->base.fillcolor);
            break;

        default:
            /* if the buffer is full, flush it */
            if (bufp == buf + sizeof(buf) - 1)
            {
                /* display the buffer */
                *bufp = '\0';
                osgen_disp_trunc(&win->base, y, x, oss_color, buf);

                /* adjust the column position for the display */
                x += bufp - buf;

                /* empty the buffer */
                bufp = buf;
            }

            /* 
             *   if we've reached the starting x coordinate, add this
             *   character to the buffer; if we haven't, we can ignore the
             *   character, since in that case we're just scanning for
             *   escape codes in the line before the part we want to display 
             */
            if (scanx >= x)
            {
                /* add it to the buffer */
                *bufp++ = *p;

                /* count it against the length remaining to be displayed */
                --len;
            }

            /* adjust the x coordinate of our scan */
            ++scanx;

            /* done */
            break;
        }
    }

    /* display the last portion of the line if anything's left */
    if (bufp != buf)
    {
        /* null-terminate at the current point */
        *bufp = '\0';

        /* display it */
        osgen_disp_trunc(&win->base, y, x, oss_color, buf);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Special routines for Text Grid windows
 */

/*
 *   Clear the text and color arrays for a section of a grid window.  Sets
 *   each cleared character's text to a space, and sets the color to
 *   text/transparent.  
 */
static void osgen_gridwin_clear_ptr(char *txtp, osgen_charcolor_t *colorp,
                                    size_t len)
{
    /* loop through the grid from the starting offset */
    for ( ; len != 0 ; --len, ++txtp, ++colorp)
    {
        /* set this character's text to a space */
        *txtp = ' ';

        /* set this character's color to text/transparent */
        colorp->fg = OSGEN_COLOR_TEXT;
        colorp->bg = OSGEN_COLOR_TRANSPARENT;
    }
}

/*
 *   Clear the text and color arrays for a section of a grid window.  Starts
 *   at the given character offset in the grid arrays, and clears for the
 *   given number of character positions.  Sets each cleared character's
 *   text to a space, and sets the color to text/transparent.  
 */
static void osgen_gridwin_clear(osgen_gridwin_t *win, size_t ofs, size_t len)
{
    /* clear our array in the selected areas */
    osgen_gridwin_clear_ptr(win->grid_txt + ofs, win->grid_color + ofs, len);
}


/*
 *   Resize a text grid window.  This must be called whenever the on-screen
 *   size of the window is changed, or we need to write outside the current
 *   bounds of the array, so that we can expand our internal size allocation
 *   if the window is now bigger than our internal text/color arrays.  
 */
static void osgen_gridwin_resize(osgen_gridwin_t *win,
                                 size_t new_wid, size_t new_ht)
{
    /* if the window's size is now bigger than the grid, expand the grid */
    if (new_wid > win->grid_wid || new_ht > win->grid_ht)
    {
        char *new_txt;
        osgen_charcolor_t *new_color;
        char *tsrc, *tdst;
        osgen_charcolor_t *csrc, *cdst;
        size_t y;

        /*  
         *   use the larger of the old size and the new size, so that the
         *   window only expands (this somewhat simplifies copying the old
         *   to the new) 
         */
        if (win->grid_wid > new_wid)
            new_wid = win->grid_wid;
        if (win->grid_ht > new_ht)
            new_ht = win->grid_ht;

        /* allocate the new arrays */
        new_txt = (char *)osmalloc(new_wid * new_ht);
        new_color = (osgen_charcolor_t *)osmalloc(
            new_wid * new_ht * sizeof(new_color[0]));

        /* copy the old grid to the new grid */
        tsrc = win->grid_txt;
        csrc = win->grid_color;
        tdst = new_txt;
        cdst = new_color;
        for (y = 0 ; y < win->grid_ht ; ++y)
        {
            /* copy the old text and color data */
            memcpy(tdst, tsrc, win->grid_wid);
            memcpy(cdst, csrc, win->grid_wid * sizeof(cdst[0]));

            /* clear the rest of the line if expanding the width */
            osgen_gridwin_clear_ptr(tdst + win->grid_wid,
                                    cdst + win->grid_wid,
                                    new_wid - win->grid_wid);

            /* advance the pointers */
            tsrc += win->grid_wid;
            csrc += win->grid_wid;
            tdst += new_wid;
            cdst += new_wid;
        }

        /* clear all remaining lines */
        for ( ; y < new_ht ; ++y, tdst += new_wid, cdst += new_wid)
            osgen_gridwin_clear_ptr(tdst, cdst, new_wid);

        /* delete the old buffers */
        osfree(win->grid_txt);
        osfree(win->grid_color);

        /* set the new buffers and sizes */
        win->grid_txt = new_txt;
        win->grid_color = new_color;
        win->grid_wid = new_wid;
        win->grid_ht = new_ht;
    }
}

/*
 *   Display a line of text in the given grid window 
 */
static void osgen_scrdisp_grid(osgen_gridwin_t *win, int x, int y, int len)
{
    char *txtp;
    osgen_charcolor_t *colorp;
    size_t ofs;
    char buf[64];
    char *dst;
    char fg, bg;
    int oss_color;

    /* 
     *   calculate the offset into our arrays: get the document coordinates
     *   (by adjusting for the scrolling offset), multiply the document row
     *   number by the row width, and add the document column number 
     */
    ofs = ((y - win->base.scrolly) * win->grid_wid) + (x - win->base.scrollx);

    /* start at the calculated offset */
    txtp = win->grid_txt + ofs;
    colorp = win->grid_color + ofs;

    /* start with the first character's color */
    fg = colorp->fg;
    bg = colorp->bg;

    /* calculate the starting oss-level color */
    oss_color = ossgetcolor(fg, bg, 0, win->base.fillcolor);

    /* 
     *   scan the text, flushing when we fill up the buffer or encounter a
     *   new text color 
     */
    for (dst = buf ; ; --len, ++txtp, ++colorp)
    {
        /* 
         *   if we have a color change, or we've exhausted the requested
         *   length, or the buffer is full, display what we have in the
         *   buffer so far 
         */
        if (len == 0
            || dst == buf + sizeof(buf) - 1
            || colorp->fg != fg || colorp->bg != bg)
        {
            /* null-terminate the buffer and display it */
            *dst = '\0';
            osgen_disp_trunc(&win->base, y, x, oss_color, buf);

            /* count the output column change */
            x += dst - buf;

            /* reset the buffer write pointer */
            dst = buf;

            /* if we have a color change, calculate the new color */
            if (colorp->fg != fg || colorp->bg != bg)
            {
                /* remember the new color */
                fg = colorp->fg;
                bg = colorp->bg;

                /* calculate the new oss-level color */
                oss_color = ossgetcolor(fg, bg, 0, win->base.fillcolor);
            }
        }

        /* if we've exhausted the request, we're done */
        if (len == 0)
            break;

        /* add this character from the text grid to our output buffer */
        *dst++ = *txtp;
    }
}

/*
 *   Write text into a grid window
 */
static void osgen_gridwin_write(osgen_gridwin_t *win,
                                const char *txt, size_t len)
{
    const char *p;
    size_t rem;
    int xmax, ymax;
    int x, y;
    int startx;
    int winx;
    size_t ofs;
    char *tdst;
    osgen_charcolor_t *cdst;

    /*
     *   First, scan the text to check for writing beyond the end of the
     *   current text array.  If we write anything beyond the bounds of the
     *   current array, we'll need to expand the array accordingly. 
     */
    xmax = x = win->base.x;
    ymax = y = win->base.y;
    for (p = txt, rem = len ; rem != 0 ; ++p, --rem)
    {
        /* check what we have */
        switch(*p)
        {
        case '\n':
            /* move to the start of the next line */
            x = 0;
            ++y;
            break;

        case '\r':
            /* move to the start of the current line */
            x = 0;
            break;

        default:
            /* 
             *   everything else takes up a character cell; if this is the
             *   highest x/y position so far, note it 
             */
            if (y > ymax)
                ymax = y;
            if (x > xmax)
                xmax = x;

            /* move the cursor right */
            ++x;
            break;
        }
    }

    /* expand our text array if we're writing outside the current bounds */
    if (xmax >= (int)win->grid_wid || ymax >= (int)win->grid_ht)
        osgen_gridwin_resize(win, xmax + 1, ymax + 1);

    /* start at the current window coordinates */
    x = win->base.x;
    y = win->base.y;

    /* calculate the starting offset in the array for writing */
    ofs = y * win->grid_wid + x;
    tdst = win->grid_txt + ofs;
    cdst = win->grid_color + ofs;

    /* note the new maximum write positions */
    if (xmax > win->base.xmax)
        win->base.xmax = xmax;
    if (ymax > win->base.ymax)
        win->base.ymax = ymax;

    /* start at the current column */
    startx = x;
    winx = x - win->base.scrollx;

    /* now scan the text again, writing it to the grid and displaying it */
    for (p = txt, rem = len ; ; ++p, --rem)
    {
        /* 
         *   if we're at a newline or the end of the text to display,
         *   display the section of the current line that we just built 
         */
        if (rem == 0 || *p == '\n' || *p == '\r')
        {
            /* if we're not in deferred redraw mode, draw this section */
            if (!S_deferred_redraw
                && !(win->base.flags & OSGEN_DEFER_REDRAW)
                && x != startx)
                osgen_scrdisp(&win->base, winx, y, x - startx);

            /* if we're out of text to display, we're done */
            if (rem == 0)
                break;

            /* apply any newline */
            if (*p == '\n')
            {
                /* move to the next line */
                x = 0;
                ++y;
            }
            else if (*p == '\r')
            {
                /* move to the start of the current line */
                x = 0;
            }

            /* note the starting coordinates of the next chunk */
            startx = x;
            winx = x - win->base.scrollx;

            /* calculate the new array output offset */
            ofs = y * win->grid_wid + x;
            tdst = win->grid_txt + ofs;
            cdst = win->grid_color + ofs;
        }
        else
        {
            /* add this character to our array */
            *tdst = *p;
            cdst->fg = win->base.txtfg;
            cdst->bg = win->base.txtbg;

            /* ajust the output pointers */
            ++tdst;
            ++cdst;

            /* adjust the column counter */
            ++x;
        }
    }

    /* set the new output position in the window */
    win->base.x = x;
    win->base.y = y;
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic banner window routines 
 */

/*
 *   Display a window's text from the given starting position for the given
 *   number of characters (counting only displayed characters; escape
 *   sequences don't count).  The starting position is given in
 *   window-relative coordinates.  
 */
static void osgen_scrdisp(osgen_win_t *win, int x, int y, int len)
{
    /* 
     *   if the text is entirely outside the window's display area, there's
     *   nothing to display, so ignore the request 
     */
    if (y < 0 || (size_t)y >= win->ht
        || x + len <= 0 || (size_t)x >= win->wid)
        return;

    /* draw according to window type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        osgen_scrdisp_txt((osgen_txtwin_t *)win, x, y, len);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        osgen_scrdisp_grid((osgen_gridwin_t *)win, x, y, len);
        break;
    }
}


/* 
 *   redraw a window 
 */
static void osgen_redraw_win(osgen_win_t *win)
{
    size_t y;

    /* clear the window's area on the screen */
    ossclr(win->winy, win->winx,
           win->winy + win->ht - 1, win->winx + win->wid - 1,
           win->oss_fillcolor);

    /* display each line in the window */
    for (y = 0 ; y < win->ht ; ++y)
        osgen_scrdisp(win, 0, y, win->wid);
}

/* redraw the entire screen */
void os_redraw(void)
{
    /* 
     *   force a redraw of the entire screen by setting the global
     *   pending-redraw flag 
     */
    S_deferred_redraw = TRUE;

    /* go redraw it */
    osssb_redraw_if_needed();
    
}

/* redraw a window, if it needs redrawing */
static void osgen_redraw_win_if_needed(int global_deferred, osgen_win_t *win)
{
    osgen_win_t *chi;

    /* 
     *   if this window needs redrawing, or we have a global deferred redraw,
     *   redraw the window 
     */
    if (global_deferred || (win->flags & OSGEN_DEFER_REDRAW) != 0)
    {
        /* clear the window-specific deferred-redraw flag */
        win->flags &= ~OSGEN_DEFER_REDRAW;

        /* redraw the window */
        osgen_redraw_win(win);
    }

    /* redraw this window's children if necessary */
    for (chi = win->first_child ; chi != 0 ; chi = chi->nxt)
        osgen_redraw_win_if_needed(global_deferred, chi);
}

/* redraw the screen if necessary */
void osssb_redraw_if_needed()
{
    int global_deferred = S_deferred_redraw;

    /* we're explicitly redrawing, so cancel any pending deferred redraw */
    S_deferred_redraw = FALSE;

    /* redraw the root window, which will redraw its children */
    if (S_main_win != 0)
        osgen_redraw_win_if_needed(global_deferred, &S_main_win->base);

    /* if the redraw is global, redraw other global features */
    if (global_deferred)
    {
        /* redraw the scrollback mode line, if appropriate */
        osgen_draw_sb_mode_line();

        /* redraw any command line under construction */
        osgen_gets_redraw_cmdline();
    }
}

/* move the cursor to the default position */
void osssb_cursor_to_default_pos(void)
{
    osgen_win_t *win;

    /* if we're in plain mode, ignore it */
    if (os_f_plain)
        return;

    /* if we're using a special cursor position, do nothing */
    if (S_sbmode_win != 0)
    {
        /* locate at the scrollback mode line */
        ossloc(S_sbmode_win->base.winy - 1, S_sbmode_win->base.winx);
    }
    else if (S_special_cursor_pos)
    {
        /* locate at the special cursor position */
        ossloc(S_special_cursor_y, S_special_cursor_x);
    }
    else
    {
        /* 
         *   if we have a default window, put the cursor at the last text
         *   position in the default window 
         */
        if ((win = &S_default_win->base) != 0)
            ossloc(win->winy + win->y - win->scrolly,
                   win->winx + win->x - win->scrollx);
    }
}

/*
 *   Lay out the given window 
 */
static void oss_lay_out_window(osgen_win_t *win)
{
    osgen_win_t *chi;

    /* 
     *   if we have a parent, take space from our parent window; otherwise,
     *   assume that the caller has already laid out our main area, in which
     *   case we just have to adjust for our children 
     */
    if (win->parent != 0)
    {
        int siz;
        int base_size;
        int x, y, wid, ht;

        /* 
         *   our size and area will be taken from our parent's, so get the
         *   parent window's current area 
         */
        x = win->parent->winx;
        y = win->parent->winy;
        wid = win->parent->wid;
        ht = win->parent->ht;

        /* get the window's size in character cells */
        switch(win->size_type)
        {
        case OS_BANNER_SIZE_ABS:
            /* the size is given in character cells */
            siz = win->size;
            break;

        case OS_BANNER_SIZE_PCT:
            /* 
             *   the size is given as a percentage of the parent's size - get
             *   the appropriate dimension from the parent's size 
             */
            base_size = (win->alignment == OS_BANNER_ALIGN_LEFT
                         || win->alignment == OS_BANNER_ALIGN_RIGHT
                         ? wid : ht);

            /* calculate the percentage of the full size */
            siz = (win->size * base_size) / 100;
            break;
        }

        /* allocate space to the window according to its alignment */
        switch(win->alignment)
        {
        case OS_BANNER_ALIGN_TOP:
            /* 
             *   assign the window the full width of the parent, and give it
             *   the requested height, up to the available height 
             */
            win->wid = wid;
            win->winx = x;
            win->ht = (siz <= ht ? siz : ht);
            win->winy = y;

            /* take the window's space away from the top of the parent */
            ht -= win->ht;
            y += win->ht;
            break;

        case OS_BANNER_ALIGN_BOTTOM:
            /* give the window space at the bottom of the parent window */
            win->wid = wid;
            win->winx = x;
            win->ht = (siz <= ht ? siz : ht);
            win->winy = y + ht - win->ht;

            /* deduct the window from the parent area */
            ht -= win->ht;
            break;

        case OS_BANNER_ALIGN_LEFT:
            /* give the window space at the left of the parent area */
            win->wid = (siz <= wid ? siz : wid);
            win->winx = x;
            win->ht = ht;
            win->winy = y;

            /* deduct the window from the parent area */
            wid -= win->wid;
            x += win->wid;
            break;

        case OS_BANNER_ALIGN_RIGHT:
            /* give the window space at the right of the remaining area */
            win->wid = (siz <= wid ? siz : wid);
            win->winx = x + wid - win->wid;
            win->ht = ht;
            win->winy = y;

            /* deduct the window from the parent area */
            wid -= win->wid;
            break;
        }

        /* adjust our parent's area for the removal of our area */
        win->parent->winx = x;
        win->parent->winy = y;
        win->parent->wid = wid;
        win->parent->ht = ht;
    }

    /* lay out our children */
    for (chi = win->first_child ; chi != 0 ; chi = chi->nxt)
        oss_lay_out_window(chi);

    /* make any necessary adjustments */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        /*
         *   If this is the current active scrollback-mode window, we must
         *   make the same adjustment that we make on entering scrollback
         *   mode: shrink the window one line from the top, to make room for
         *   the scrollback-mode status line.  
         */
        if (win == &S_sbmode_win->base)
        {
            /* it's the scrollback window - take out the mode line */
            win->winy += 1;
            win->ht -= 1;
        }
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* make sure the window's text/color buffers are large enough */
        osgen_gridwin_resize((osgen_gridwin_t *)win, win->wid, win->ht);
        break;
    }
}

/*
 *   Recalculate the window layout and redraw the screen 
 */
static void osgen_recalc_layout()
{
    /* start at the root window */
    if (S_main_win != 0)
    {
        int ht;

        /* start by giving the entire screen to the main window */
        S_main_win->base.winx = 0;
        S_main_win->base.winy = 0;
        S_main_win->base.wid = G_oss_screen_width;
        S_main_win->base.ht = G_oss_screen_height;

        /* lay out the main window and its children */
        oss_lay_out_window(&S_main_win->base);

        /* recalculate the main window's page size */
        G_os_linewidth = S_main_win->base.wid;
        ht = S_main_win->base.ht;
        G_os_pagelength = (ht > 2 ? ht - 2 : ht > 1 ? ht - 1 : ht);
    }

    /* schedule a redraw of the entire screen */
    S_deferred_redraw = TRUE;
}

/*
 *   Receive notification that the screen was resized.  We'll recalculate
 *   the banner window layout.  
 */
void osssb_on_resize_screen()
{
    /* recalculate the window layout */
    osgen_recalc_layout();

    /* immediately update the screen */
    osssb_redraw_if_needed();

    /* set the cursor back to the default position */
    osssb_cursor_to_default_pos();
}

# else /* USE_SCROLLBACK */

/* 
 *   for the non-scrollback version, there's nothing we need to do on
 *   resizing the screen, as we don't do anything fancy with the layout 
 */
void osssb_on_resize_screen()
{
    /* do nothing */
}

# endif /* USE_SCROLLBACK */

/* ------------------------------------------------------------------------ */

#ifdef USE_STATLINE

/* current statusline score-area string */
static osfar_t char S_scorebuf[135];

/* statusline left-string reset point */
static osfar_t char *S_stat_reset_free;
static osfar_t int S_stat_reset_x;

/* update the right portion of the statusline */
static void osgen_update_stat_right()
{
    osgen_txtwin_t *win;
    int wid_rem;
    size_t char_len;
    
    /* 
     *   if there's no statusline window, or no reset point, we can't do
     *   anything right now 
     */
    if ((win = S_status_win) == 0 || S_stat_reset_free == 0)
        return;

    /* 
     *   set the statusline position back to where it was when we finished
     *   with the statusline itself
     */
    win->txtfree = S_stat_reset_free;
    win->base.x = S_stat_reset_x;

    /* if there's no space, don't draw anything */
    if (win->base.x > (int)win->base.wid)
        return;

    /* figure out how much space in the window we have */
    wid_rem = win->base.wid - win->base.x;

    /* figure out how much we're adding */
    char_len = strlen(S_scorebuf);

    /* 
     *   if we're adding more than we have room for, add a space and then
     *   add the buffer contents 
     */
    if ((int)char_len + 1 >= wid_rem)
    {
        /* add a space */
        ossaddsb(win, " ", 1, TRUE);

        /* add the right-half string */
        ossaddsb(win, S_scorebuf, strlen(S_scorebuf), TRUE);
    }
    else
    {
        /* add spaces to right-align the scorebuf text */
        while (wid_rem > (int)char_len + 1)
        {
            char buf[64];
            size_t cur;
            
            /* add the remaining spaces, up to a buffer-full */
            cur = wid_rem - char_len - 1;
            if (cur > sizeof(buf) - 1)
                cur = sizeof(buf) - 1;

            /* make a buffer-full of spaces */
            memset(buf, ' ', cur);

            /* display it */
            ossaddsb(win, buf, cur, TRUE);

            /* deduct the amount we displayed from the width remaining */
            wid_rem -= cur;
        }

        /* add the right-half string */
        ossaddsb(win, S_scorebuf, strlen(S_scorebuf), TRUE);
    }
}

/*
 *   Set the status mode.  In status mode 0, text displayed via os_print and
 *   the like will display to the main window; in mode 1, text displayed will
 *   go to the default status line window, until we reach a newline, at which
 *   point we'll switch to status mode 2 and text will go nowhere.  
 */
void os_status(int stat)
{
    /* if we're in 'plain' mode, suppress all statusline output */
    if (os_f_plain)
    {
        /* check the requested mode */
        switch(stat)
        {
        case 0:
            /* switching to main text mode */
            status_mode = 0;
            break;

        case 1:
            /* switching to statusline mode - suppress all output */
            status_mode = 2;
            break;

        case 2:
            /* suppress mode */
            status_mode = 2;
            break;
        }

        /* done */
        return;
    }

    /* if there's no statusline window, create one */
    if (S_status_win == 0)
    {
        /* create the statusline window as a child of the main window */
        S_status_win = osgen_create_txtwin(OS_BANNER_FIRST, 0, S_main_win,
                                           512, 1);

        /* if that succeeded, set up the window */
        if (S_status_win != 0)
        {
            /* set up the statusline window at the top of the screen */
            S_status_win->base.alignment = OS_BANNER_ALIGN_TOP;
            S_status_win->base.size = 1;
            S_status_win->base.size_type = OS_BANNER_SIZE_ABS;

            /* use the default statusline color in this window */
            S_status_win->base.txtfg = OSGEN_COLOR_STATUSLINE;
            S_status_win->base.txtbg = OSGEN_COLOR_TRANSPARENT;
            S_status_win->base.fillcolor = OSGEN_COLOR_STATUSBG;

            /* cache the oss translations of the colors */
            S_status_win->base.oss_fillcolor =
                ossgetcolor(OSGEN_COLOR_STATUSLINE, OSGEN_COLOR_STATUSBG,
                            0, 0);

            /* recalculate the window layout */
            osgen_recalc_layout();
        }
    }

    /* if entering status mode 1, clear the statusline */
    if (stat == 1 && status_mode != 1 && S_status_win != 0)
    {
        /* switch the default window to the status line */
        S_default_win = S_status_win;

        /* clear the statusline window */
        osgen_clear_win(&S_status_win->base);

        /* forget the score reset point */
        S_stat_reset_free = 0;
    }

    /* if we're leaving status mode 1, finish the statusline */
    if (status_mode == 1 && stat != 1 && S_status_win != 0)
    {
        /* 
         *   remember the current statusline window settings as the "reset"
         *   point - this is where we reset the buffer contents whenever we
         *   want to add the right-half string 
         */
        S_stat_reset_free = S_status_win->txtfree;
        S_stat_reset_x = S_status_win->base.x;

        /* update the right-half string */
        osgen_update_stat_right();
    }

    /* switch to the new mode */
    status_mode = stat;

    /* check the mode */
    if (status_mode == 0)
    {
        /* switching to the main window */
        S_default_win = S_main_win;
    }
    else if (status_mode == 2)
    {
        /* 
         *   entering post-status mode - ignore everything, so set the
         *   default window to null 
         */
        S_default_win = 0;
    }
}

int os_get_status()
{
    return status_mode;
}

/* Set score to a string value provided by the caller */
void os_strsc(const char *p)
{
    size_t copy_len;

    /* copy the score, if a value was given */
    if (p != 0)
    {
        /* limit the copying length to our buffer size */
        copy_len = strlen(p);
        if (copy_len > sizeof(S_scorebuf) - 1)
            copy_len = sizeof(S_scorebuf) - 1;
        
        /* copy the text and null-terminate it */
        memcpy(S_scorebuf, p, copy_len);
        S_scorebuf[copy_len] = '\0';
    }

    /* update the statusline window */
    osgen_update_stat_right();
}

/*
 *   Set the score.  If cur == -1, the LAST score set with a non-(-1) cur is
 *   displayed; this is used to refresh the status line without providing a
 *   new score (for example, after exiting scrollback mode).  Otherwise, the
 *   given current score (cur) and turncount are displayed, and saved in
 *   case cur==-1 on the next call.  
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

/*
 *   display text to the default window 
 */
void os_printz(const char *str)
{
    /* write using the base counted-length routine */
    os_print(str, strlen(str));
}

/*
 *   display text to the default window
 */
void os_print(const char *str, size_t len)
{
    osgen_txtwin_t *win;
    const char *p;
    const char *startp;
    size_t rem;

    /* determine what to do based on the status mode */
    switch(status_mode)
    {
    case 2:
        /* we're in the post-status-line mode - suppress all output */
        break; 
        
    case 0:
        /* we're showing the text in the default window */
        if (os_f_plain)
        {
            /* plain mode - simply write it to stdout */
            printf("%.*s", (int)len, str);
        }
        else
        {
            /* normal mode - write to the default window, if there is one */
            win = S_default_win;
            if (win != 0)
            {
                /* write the text to the window buffer */
                ossaddsb(win, str, len, TRUE);

                /* 
                 *   move the cursor to the new location, if we're not hiding
                 *   updates for the moment 
                 */
                if (!S_deferred_redraw
                    && !(win->base.flags & OSGEN_DEFER_REDRAW))
                    ossloc(win->base.winy + win->base.y - win->base.scrolly,
                           win->base.winx + win->base.x - win->base.scrollx);
            }
        }

        /* done */
        break;
        
    case 1:
        /* 
         *   Status line contents.  Ignore the status line in 'plain' mode
         *   or if there's no statusline window.  
         */
        if (os_f_plain || (win = S_status_win) == 0)
            break;

        /* 
         *   Skip leading newlines at the start of the statusline output.
         *   Only do this if we don't already have anything buffered, since
         *   a newline after some other text indicates the end of the status
         *   line and thus can't be ignored.  
         */
        p = str;
        rem = len;
        if (win->base.winy == 0 || win->base.winx == 0)
        {
            /* the buffer is empty, so skip leading newlines */
            for ( ; rem != 0 && *p == '\n' ; ++p, --rem) ;

            /* if that leaves nothing, we're done */
            if (rem == 0)
                break;

            /* 
             *   add a space before the first character, so that we always
             *   have a space at the left edge of the status line 
             */
            ossaddsb(win, " ", 1, TRUE);
        }

        /* scan for a newline; if we find one, it's the end of the status */
        for (startp = p ; rem != 0 && *p != '\n' ; ++p, --rem)
        {
            /* skip escapes */
            switch (*p)
            {
            case OSGEN_ATTR:
                /* skip the extra byte */
                p += 1;
                rem -= 1;
                break;
                
            case OSGEN_COLOR:
                /* skip the extra two bytes */
                p += 2;
                rem -= 2;
                break;

            default:
                /* everything else is one byte long */
                break;
            }
        }

        /* add this text to the statusline window */
        ossaddsb(win, startp, p - startp, TRUE);

        /* finish up if we found a newline */
        if (*p == '\n')
        {
            /* switch to status mode 2 */
            os_status(2);
        }
        
        /* done */
        break;
    }
}

#ifndef FROBTADS          /* hack - should refactor to avoid need for ifdef */
void os_flush(void)
{
    /* 
     *   we don't buffer output ourselves, so there's normally nothing to do
     *   here; but if we're in 'plain' mode, let stdio know about the flush,
     *   since it might be buffering output on our behalf 
     */
    if (os_f_plain)
        fflush(stdout);
}
#endif

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
static osfar_t char *histbuf = 0;
static osfar_t char *histhead = 0;
static osfar_t char *histtail = 0;

/*
 *   ossadvhp advances a history pointer, and returns the new pointer.
 *   This function takes the circular nature of the buffer into account
 *   by wrapping back to the start of the buffer when it hits the end.
 */
char *ossadvhp(char *p)
{
    if (++p >= histbuf + HISTBUFSIZE)
        p = histbuf;
    return p;
}

/*
 *   ossdechp decrements a history pointer, wrapping the pointer back
 *   to the top of the buffer when it reaches the bottom.
 */
char *ossdechp(char *p)
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
void osshstcpy(char *dst, char *hst)
{
    while (*hst != '\0')
    {
        *dst++ = *hst;
        hst = ossadvhp(hst);
    }
    *dst = '\0';
}

/*
 *   ossprvcmd returns a pointer to the previous history command, given
 *   a pointer to a history command.  It returns a null pointer if the
 *   given history command is the first in the buffer.
 */
char *ossprvcmd(char *hst)
{
    /* check to see if we're already at the fist command */
    if (hst == histtail)
        return 0;

    /* back up to the previous null byte */
    hst = ossdechp(hst);

    /* scan back to the previous line */
    do
    {
        hst = ossdechp(hst);
    } while (*hst && hst != histtail);

    /* step over the null byte to the start of the following line */
    if (*hst == 0)
        hst = ossadvhp(hst);

    /* return the result */
    return hst;
}

/*
 *   ossnxtcmd returns a pointer to the next history command, given
 *   a pointer to a history command.  It returns a null pointer if the
 *   given command is already past the last command.
 */
char *ossnxtcmd(char *hst)
{
    /* check to see if we're already past the last line */
    if (hst == histhead)
        return 0;

    /* scan forward to the next null byte */
    while(*hst != '\0')
        hst = ossadvhp(hst);

    /* scan past the null onto the new command */
    hst = ossadvhp(hst);

    /* return the pointer */
    return hst;
}
# endif /* USE_HISTORY */

/* ------------------------------------------------------------------------ */
/*
 *   Display an input line under construction from the given character
 *   position.  If 'delta_yscroll' is non-null, then we'll scroll the window
 *   vertically if necessary and fill in '*delta_yscroll' with the number of
 *   lines we scrolled by.  
 */
static void ossdsp_str(osgen_win_t *win, int y, int x, int color,
                       char *str, size_t len, int *delta_yscroll)
{
    /* presume we won't scroll */
    if (delta_yscroll != 0)
        *delta_yscroll = 0;

    /* keep going until we exhaust the string */
    while (len != 0)
    {
        size_t cur;
        unsigned char oldc;

        /* display as much as will fit on the current line before wrapping */
        cur = win->winx + win->wid - x;
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

        /* if we've reached the right edge of the window, wrap the line */
        if (x >= win->winx + (int)win->wid)
        {
            /* wrap to the left edge of the window */
            x = win->winx;

            /* advance to the next line */
            ++y;

            /* 
             *   if this puts us past the bottom of the window, and we're
             *   allowed to scroll the window, do so
             */
            if (y >= win->winy + (int)win->ht && delta_yscroll != 0)
            {
                /* scroll by one line */
                ossscr(win->winy, win->winx,
                       win->winy + win->ht - 1, win->winx + win->wid - 1,
                       win->oss_fillcolor);

                /* adjust the scroll position of the window */
                win->scrolly++;

                /* count the scrolling */
                ++(*delta_yscroll);

                /* move back a line */
                --y;
            }
        }
    }
}

/*
 *   Move the cursor left by the number of characters.  
 */
static void oss_gets_csrleft(osgen_txtwin_t *win, int *y, int *x, size_t len)
{
    for ( ; len != 0 ; --len)
    {
        /* move left one character, wrapping at the end of the line */
        if (--*x < win->base.winx)
        {
            /* move up a line */
            --*y;

            /* move to the end of the line */
            *x = win->base.winx + win->base.wid - 1;
        }
    }
}

/*
 *   Move the cursor right by the number of characters.  
 */
static void oss_gets_csrright(osgen_txtwin_t *win, int *y, int *x, size_t len)
{
    for ( ; len != 0 ; --len)
    {
        /* move right one character, wrapping at the end of the line */
        if (++*x >= win->base.winx + (int)win->base.wid)
        {
            /* move down a line */
            ++*y;

            /* move to the left column */
            *x = win->base.winx;
        }
    }
}

/*
 *   clear an area of the display formerly occupied by some input text 
 */
static void oss_gets_clear(osgen_txtwin_t *win, int y, int x, size_t len)
{
    /* if we don't even reach the left edge of the window, ignore it */
    if (x + (int)len <= win->base.winx)
        return;

    /* skip anything to the left of the window */
    if (x < win->base.winx)
    {
        /* deduct the unseen part from the length */
        len -= (win->base.winx - x);

        /* start at the left edge */
        x = win->base.winx;
    }

    /* clear one line at a time */
    while (len != 0)
    {
        size_t cur;

        /* calculate how much we have left to the right edge of the window */
        cur = win->base.winx + win->base.wid - x;

        /* limit the clearing to the requested length */
        if (cur > len)
            cur = len;

        /* clear this chunk */
        ossclr(y, x, y, x + cur - 1, win->base.oss_fillcolor);

        /* deduct this chunk from the remaining length */
        len -= cur;

        /* move to the start of the next line */
        x = win->base.winx;
        ++y;

        /* if we're past the bottom of the window, stop */
        if (y >= win->base.winy + (int)win->base.ht)
            break;
    }
}

/*
 *   Delete a character in the buffer, updating the display. 
 */
static void oss_gets_delchar(osgen_txtwin_t *win,
                             char *buf, char *p, char **eol, int x, int y)
{
    int color;
    
    /* get the oss color for the current text in the window */
    color = ossgetcolor(win->base.txtfg, win->base.txtbg,
                        win->base.txtattr, win->base.fillcolor);

    /* if the character is within the buffer, delete it */
    if (p < *eol)
    {
        /* delete the character and close the gap */
        --*eol;
        if (p != *eol)
            memmove(p, p + 1, *eol - p);

        /* null-terminate the shortened buffer */
        **eol = '\0';
        
        /* re-display the changed part of the string */
        ossdsp_str(&win->base, y, x, color, p, *eol - p, 0);

        /* move to the position of the former last character */
        oss_gets_csrright(win, &y, &x, *eol - p);

        /* clear the screen area where the old last character was displayed */
        ossclr(y, x, y, x, win->base.oss_fillcolor);
    }
}

/*
 *   Backspace in the buffer, updating the display and adjusting the cursor
 *   position. 
 */
static void oss_gets_backsp(osgen_txtwin_t *win,
                            char *buf, char **p,
                            char **eol, int *x, int *y)
{
    int color;

    /* get the oss color for the current text in the window */
    color = ossgetcolor(win->base.txtfg, win->base.txtbg,
                        win->base.txtattr, win->base.fillcolor);

    /* if we can back up, do so */
    if (*p > buf)
    {
        int tmpy;
        int tmpx;

        /* move our insertion point back one position */
        --*p;
        
        /* the line is now one character shorter */
        --*eol;

        /* shift all of the characters down one position */
        if (*p != *eol)
            memmove(*p, *p + 1, *eol - *p);

        /* move the cursor back, wrapping if at the first column */
        if (--*x < win->base.winx)
        {
            *x = win->base.winx + win->base.wid - 1;
            --*y;
        }

        /* null-terminate the shortened buffer */
        **eol = '\0';

        /* 
         *   display the string from the current position, so that we update
         *   the display for the moved characters 
         */
        tmpy = *y;
        tmpx = *x;
        ossdsp_str(&win->base, tmpy, tmpx, color, *p, *eol - *p, 0);

        /* clear the screen area where the old last character was shown */
        oss_gets_csrright(win, &tmpy, &tmpx, *eol - *p);
        ossclr(tmpy, tmpx, tmpy, tmpx, win->base.oss_fillcolor);
    }
}

/*
 *   Redraw any command line under construction 
 */
static void osgen_gets_redraw_cmdline(void)
{
    osgen_txtwin_t *win;
    int x, y;
    int color;

    /* 
     *   get the default window; if we don't have one, or there's no command
     *   line editing in progress, there's nothing to do 
     */
    if ((win = S_default_win) == 0 || !S_gets_in_progress)
        return;

    /* set up at the current cursor position */
    x = S_gets_x;
    y = S_gets_y;

    /* move to the start of the command */
    oss_gets_csrleft(win, &y, &x, S_gets_ofs);

    /* get the color of the command text */
    color = ossgetcolor(win->base.txtfg, win->base.txtbg,
                        win->base.txtattr, win->base.fillcolor);

    /* draw the command line */
    ossdsp_str(&win->base, y, x, color, S_gets_buf, strlen(S_gets_buf), 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   cancel interrupted input 
 */
void os_gets_cancel(int reset)
{
    int x, y;

    /* if we're doing scrollback, cancel it */
    if (S_sbmode_win != 0)
        osgen_sb_mode_end();

    /* do any deferred redrawing */
    osssb_redraw_if_needed();

    /* 
     *   if we interrupted a previous line, apply display effects as though
     *   the user had pressed return 
     */
    if (S_gets_in_progress)
    {
        osgen_txtwin_t *win;

        /* use the main window */
        win = S_main_win;

        /* move to the end of the input line */
        x = S_gets_x;
        y = S_gets_y;
        oss_gets_csrright(win, &y, &x, strlen(S_gets_buf + S_gets_ofs));
        
        /* set the cursor to the new position */
        ossloc(y, x);
        
        /* copy the buffer to the screen save buffer, adding a newline */
        ossaddsb_input(win, (char *)S_gets_buf, TRUE);

        /* we no longer have an input in progress */
        S_gets_in_progress = FALSE;
    }
    
    /* if we're resetting, clear our saved buffer */
    if (reset)
        S_gets_buf[0] = '\0';
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize input line editing mode.  Returns true if we can
 *   successfully set up input line editing mode, false if not.  
 */
int os_gets_begin(size_t max_line_len)
{
    osgen_txtwin_t *win;

    /* if there's no default window, there's nothing we can do */
    if ((win = S_default_win) == 0)
        return FALSE;

    /* do any deferred redrawing */
    osssb_redraw_if_needed();

    /* if the cursor if off the screen vertically, scroll to show it */
    if (win->base.y >= win->base.scrolly + (int)win->base.ht)
        osgen_scroll_win_fwd(win, win->base.y
                             - (win->base.scrolly + win->base.ht) + 1);

    /* if we're horizontally scrolled, scroll to the left edge */
    if (win->base.scrollx != 0)
    {
        /* scroll to the left edge */
        win->base.scrollx = 0;

        /* redraw the window */
        osgen_redraw_win(&win->base);
    }

# ifdef USE_HISTORY
    /* allocate the history buffer if it's not already allocated */
    if (histbuf == 0)
    {
        histbuf = (char *)osmalloc(HISTBUFSIZE);
        histhead = histtail = histbuf;
        S_gets_curhist = histhead;
    }
# endif /* USE_HISTORY */

    /*
     *   If we have saved input state from a previous interrupted call,
     *   restore it now.  Otherwise, initialize everything.  
     */
    if (S_gets_buf[0] != '\0' || S_gets_in_progress)
    {
        /* 
         *   if we cancelled the previous input, we must re-display the
         *   buffer under construction, since we have displayed something
         *   else in between and have re-displayed the prompt 
         */
        if (!S_gets_in_progress)
        {
            int x;
            int y;
            int deltay;
            int color;

            /* set up at the window's output position, in screen coords */
            x = win->base.winx + win->base.x - win->base.scrollx;
            y = win->base.winy + win->base.y - win->base.scrolly;

            /* get the current color in the window */
            color = ossgetcolor(win->base.txtfg, win->base.txtbg,
                                win->base.txtattr, win->base.fillcolor);

            /* re-display the buffer */
            ossdsp_str(&win->base, y, x, color,
                       S_gets_buf, strlen(S_gets_buf), &deltay);

            /* adjust our y position for any scrolling we just did */
            y -= deltay;

            /* limit the initial offset to the available buffer length */
            if (S_gets_ofs > (int)max_line_len)
                S_gets_ofs = max_line_len;

            /* move back to the original insertion point */
            oss_gets_csrright(win, &y, &x, S_gets_ofs);

            /* note the current position as the new editing position */
            S_gets_x = x;
            S_gets_y = y;
        }
    }
    else
    {
        /* initialize our history recall pointer */
        S_gets_curhist = histhead;

        /* set up at the window's output position, in screen coords */
        S_gets_x = win->base.winx + win->base.x - win->base.scrollx;
        S_gets_y = win->base.winy + win->base.y - win->base.scrolly;

        /* we're at offset zero in the input line */
        S_gets_ofs = 0;
    }

    /* 
     *   set the buffer end pointer to limit input to the maximum size the
     *   caller has requested, or the maximum size of our internal buffer,
     *   whichever is smaller 
     */
    if (max_line_len > S_gets_buf_siz)
        S_gets_buf_end = S_gets_buf + S_gets_buf_siz - 1;
    else
        S_gets_buf_end = S_gets_buf + max_line_len - 1;

    /* override the default cursor position logic while reading */
    S_special_cursor_pos = TRUE;
    S_special_cursor_x = S_gets_x;
    S_special_cursor_y = S_gets_y;

    /* note that input is in progress */
    S_gets_in_progress = TRUE;

    /* successfully set up */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Process an event in input line editing mode.  Returns true if the user
 *   explicitly ended input line editing by pressing Return or something
 *   similar, false if input line editing mode remains active.  
 */
int os_gets_process(int event_type, os_event_info_t *event_info)
{
    unsigned char c;
    char *p;
    char *eol;
    char *buf;
    size_t bufsiz;
    int x;
    int y;
    osgen_txtwin_t *win;
    int color;

    /* use the default window */
    if ((win = S_default_win) == 0)
        return TRUE;

    /* if it's a keystroke event, convert from "raw" to a CMD_xxx code */
    if (event_type == OS_EVT_KEY)
        oss_raw_key_to_cmd(event_info);

    /* 
     *   if we're in scrollback mode, run the event through the scrollback
     *   handler rather than handling it directly 
     */
    if (S_sbmode_win != 0)
    {
        /* run the event through scrollback mode */
        if (osgen_sb_mode(win, event_type, event_info))
        {
            /* 
             *   scrollback mode fully handled the event, so we're done;
             *   tell the caller we didn't finish command input editing 
             */
            return FALSE;
        }
    }

    /* ignore everything except keystroke events */
    if (event_type != OS_EVT_KEY)
        return FALSE;

    /* get the key from the event */
    c = (unsigned char)event_info->key[0];

    /* set up at the current position */
    x = S_gets_x;
    y = S_gets_y;

    /* set up our buffer pointers */
    p = S_gets_buf + S_gets_ofs;
    buf = S_gets_buf;
    bufsiz = S_gets_buf_siz;
    eol = p + strlen(p);

    /* get the current color in the window */
    color = ossgetcolor(win->base.txtfg, win->base.txtbg,
                        win->base.txtattr, win->base.fillcolor);

    /* 
     *   Check the character we got.  Note that we must interpret certain
     *   control characters explicitly, because os_get_event() returns raw
     *   keycodes (untranslated into CMD_xxx codes) for control characters.  
     */
    switch(c)
    {
    case 8:
        /* backspace one character */
        oss_gets_backsp(win, buf, &p, &eol, &x, &y);
        break;
        
    case 13:
        /* Return/Enter key - we're done.  Null-terminate the input. */
        *eol = '\0';
        
        /* move to the end of the line */
        oss_gets_csrright(win, &y, &x, eol - p);
        p = eol;

# ifdef USE_HISTORY
        /*
         *   Save the line in our history buffer.  If we don't have enough
         *   room, lose some old text by advancing the tail pointer far
         *   enough.  Don't save it if it's a blank line, though, or if it
         *   duplicates the most recent previous command.  
         */
        if (strlen(buf) != 0)
        {
            char *q;
            int advtail;
            int saveflag = 1;                /* assume we will be saving it */
            
            if ((q = ossprvcmd(histhead)) != 0)
            {
                char *p = buf;
                
                while (*p == *q && *p != '\0' && *q != '\0')
                {
                    ++p;
                    q = ossadvhp(q);
                }
                if (*p == *q)               /* is this a duplicate command? */
                    saveflag = 0;                   /* if so, don't save it */
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
                 *   partially trashed; to do so, advance the tail pointer
                 *   to the next null byte.  
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

        /* add the text to the scrollback buffer */
        ossaddsb_input(win, buf, TRUE);

        /* done with the special cursor position */
        S_special_cursor_pos = FALSE;

        /* 
         *   immediately scroll the window if necessary, and make sure the
         *   cursor is showing at the right location 
         */
        osgen_auto_vscroll(win);
        osssb_cursor_to_default_pos();

        /* input is no longer in progress */
        S_gets_in_progress = FALSE;

        /* tell the caller we're done */
        return TRUE;

    case 0:
        /* extended key code - get the second half of the code */
        c = (unsigned char)event_info->key[1];

        /* handle the command key code */
        switch(c)
        {
# ifdef USE_SCROLLBACK
        case CMD_SCR:
        case CMD_PGUP:
        case CMD_PGDN:
            /* run the event through scrollback mode */
            osgen_sb_mode(win, event_type, event_info);

            /* done */
            break;
# endif /* USE_SCROLLBACK */

        case CMD_LEFT:
            /* move the cursor left */
            if (p > buf)
            {
                --p;
                oss_gets_csrleft(win, &y, &x, 1);
            }
            break;

        case CMD_WORD_LEFT:
            /*
             *   Move back one word.  This moves the cursor back a
             *   character, then seeks back until we're on a non-space
             *   character, then seeks back until we're on a character
             *   preceded by a space character.  
             */
            if (p > buf)
            {
                /* back up one character */
                --p;
                oss_gets_csrleft(win, &y, &x, 1);

                /* back up until we're on a non-space character */
                while (p > buf && t_isspace(*p) && !t_isspace(*(p-1)))
                {
                    --p;
                    oss_gets_csrleft(win, &y, &x, 1);
                }

                /* 
                 *   back up again until we're on a character preceded by a
                 *   space character 
                 */
                while (p > buf && !t_isspace(*(p-1)))
                {
                    --p;
                    oss_gets_csrleft(win, &y, &x, 1);
                }
            }
            break;

        case CMD_RIGHT:
            /* move the cursor right */
            if (p < eol)
            {
                ++p;
                oss_gets_csrright(win, &y, &x, 1);
            }
            break;

        case CMD_WORD_RIGHT:
            /*
             *   Move right one word.  This moves the cursor right until
             *   we're on a space character, then moves the cursor right
             *   again until we're on a non-space character.  First, move
             *   right until we're between words (i.e., until we're on a
             *   space character).  
             */
            while (p < eol && !t_isspace(*p))
            {
                ++p;
                oss_gets_csrright(win, &y, &x, 1);
            }

            /* now move right until we're on a non-space character */
            while (p < eol && t_isspace(*p))
            {
                ++p;
                oss_gets_csrright(win, &y, &x, 1);
            }
            break;

        case CMD_DEL:
            /* delete a character */
            oss_gets_delchar(win, buf, p, &eol, x, y);
            break;

#ifdef UNIX               
        case CMD_WORDKILL:
            {
                /* remove spaces preceding word */
                while (p >= buf && *p <= ' ')
                    oss_gets_backsp(win, buf, &p, &eol, &x, &y);

                /* remove previous word (i.e., until we get a space) */
                while (p >= buf && *p > ' ')
                    oss_gets_backsp(win, buf, &p, &eol, &x, &y);

                /* that's it */
                break;
            }
#endif /* UNIX */

        case CMD_KILL:
        case CMD_HOME:
# ifdef USE_HISTORY
        case CMD_UP:
        case CMD_DOWN:
            /*
             *   Home, Kill (delete entire line), History Up, History Down -
             *   what these all have in common is that we move to the start
             *   of the line before doing anything else.  
             */

            /* if 'up', make sure we have more history to traverse */
            if (c == CMD_UP && !ossprvcmd(S_gets_curhist))
                break;

            /* if 'down', make sure there's more history to traverse */
            if (c == CMD_DOWN && !ossnxtcmd(S_gets_curhist))
                break;

            /* 
             *   if this is the first 'up', save the current buffer, so that
             *   we can reinstate it if we traverse back 'down' until we're
             *   back at the original buffer (the active buffer essentially
             *   becomes a temporary history entry that we can recover by
             *   history-scrolling back down to it) 
             */
                if (c == CMD_UP && !ossnxtcmd(S_gets_curhist))
                    safe_strcpy(S_hist_sav, S_hist_sav_siz, buf);

# endif /* USE_HISTORY */

                /* move to the start of the line */
                if (p != buf)
                {
                    /* move the cursor */
                    oss_gets_csrleft(win, &y, &x, p - buf);

                    /* move the insertion pointer */
                    p = buf;
                }

                /* if it was just a 'home' command, we're done */
                if (c == CMD_HOME)
                    break;

                /*
                 *   We're at the start of the line now; fall through for
                 *   KILL, UP, and DOWN to the code which deletes to the end
                 *   of the line.  
                 */

        case CMD_DEOL:
            /* clear the remainder of the line on the display */
            oss_gets_clear(win, y, x, eol - p);

            /* truncate the buffer at the insertion point */
            eol = p;
            *p = '\0';

# ifdef USE_HISTORY
            if (c == CMD_UP)
            {
                S_gets_curhist = ossprvcmd(S_gets_curhist);
                osshstcpy(buf, S_gets_curhist);
            }
            else if (c == CMD_DOWN)
            {
                if (!ossnxtcmd(S_gets_curhist))
                    break;                                       /* no more */
                S_gets_curhist = ossnxtcmd(S_gets_curhist);
                if (ossnxtcmd(S_gets_curhist))        /* on a valid command */
                    osshstcpy(buf, S_gets_curhist);        /* ... so use it */
                else
                {
                    /* no more history - restore original line */
                    safe_strcpy(buf, bufsiz, S_hist_sav);
                }
            }
            if ((c == CMD_UP || c == CMD_DOWN)
                && strlen(buf) != 0)
            {
                int deltay;

                /* get the end pointer based on null termination */
                eol = buf + strlen(buf);

                /* display the string */
                ossdsp_str(&win->base, y, x, color, p, eol - p, &deltay);
                y -= deltay;

                /* move to the end of the line */
                oss_gets_csrright(win, &y, &x, eol - p);
                p = eol;
            }
# endif /* USE_HISTORY */
            break;
        case CMD_END:
            while (p < eol)
            {
                ++p;
                if (++x >= win->base.winx + (int)win->base.wid)
                {
                    x = win->base.winx;
                    ++y;
                }
            }
            break;
        }
        break;

    default:
        if (c >= ' ' && eol < S_gets_buf_end)
        {
            int deltay;

            /* open up the line and insert the character */
            if (p != eol)
                memmove(p + 1, p, eol - p);
            ++eol;
            *p = (char)c;
            *eol = '\0';

            /* write the updated part of the line */
            ossdsp_str(&win->base, y, x, color, p, eol - p, &deltay);
            y -= deltay;

            /* move the cursor right one character */
            oss_gets_csrright(win, &y, &x, 1);

            /* advance the buffer pointer one character */
            ++p;
        }
        break;
    }

    /* remember the current editing position */
    S_special_cursor_x = S_gets_x = x;
    S_special_cursor_y = S_gets_y = y;
    S_gets_ofs = p - S_gets_buf;

    /* we didn't finish editing */
    return FALSE;
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
    long end_time;
    osgen_txtwin_t *win;
    
    /* if we're in 'plain' mode, simply use stdio input */
    if (os_f_plain)
    {
        size_t len;

        /* make sure the standard output is flushed */
        fflush(stdout);

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

    /* begin input editing mode */
    os_gets_begin(bufl);

    /*
     *   If we have a timeout, calculate the system clock time at which the
     *   timeout expires.  This is simply the current system clock time plus
     *   the timeout interval.  Since we might need to process a series of
     *   events, we'll need to know how much time remains at each point we
     *   get a new event.  
     */
    end_time = os_get_sys_clock_ms() + timeout;
    
    /* use the default window for input */
    if ((win = S_default_win) == 0)
        return OS_EVT_EOF;

    /* process keystrokes until we're done entering the command */
    for ( ;; )
    {
        int event_type;
        os_event_info_t event_info;
        
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
        
        /* move to the proper position on the screen before pausing */
        if (S_sbmode_win != 0)
            ossloc(S_sbmode_win->base.winy, S_sbmode_win->base.winx);
        else
            ossloc(S_gets_y, S_gets_x);

        /* get an event */
        event_type = os_get_event(timeout, use_timeout, &event_info);

        /* handle the event according to the event type */
        switch(event_type)
        {
        case OS_EVT_TIMEOUT:
        timeout_expired:
            /* done with the overridden cursor position */
            S_special_cursor_pos = FALSE;

            /* return the timeout status to the caller */
            return OS_EVT_TIMEOUT;

        case OS_EVT_NOTIMEOUT:
            /* 
             *   we can't handle events with timeouts, so we can't provide
             *   line reading with timeouts, either
             */
            S_special_cursor_pos = FALSE;
            return OS_EVT_NOTIMEOUT;

        case OS_EVT_EOF:
            /* end of file - end input and return the EOF to our caller */
            S_special_cursor_pos = FALSE;
            S_gets_in_progress = FALSE;
            return OS_EVT_EOF;

        default:
            /* process anything else through the input line editor */
            if (os_gets_process(event_type, &event_info))
            {
                /* 
                 *   Copy the result to the caller's buffer.  Note that we
                 *   know the result will fit, because we always limit the
                 *   editing process to the caller's buffer size.  
                 */
                strcpy((char *)buf, S_gets_buf);

                /* clear the input buffer */
                S_gets_buf[0] = '\0';

                /* input is no longer in progress */
                S_gets_in_progress = FALSE;

                /* return success */
                return OS_EVT_LINE;
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
    osgen_txtwin_t *win;

    /* if there's no default output window, do nothing */
    if ((win = S_default_win) == 0)
        return;

    /* 
     *   if the attributes are different from the old attributes, add an
     *   attribute-change sequence to the display buffer 
     */
    if (attr != win->base.txtattr)
    {
        char buf[3];

        /* set up the attribute-change sequence */
        buf[0] = OSGEN_ATTR;
        buf[1] = (char)attr;
        ossaddsb(win, buf, 2, TRUE);
    }
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
 *   displayed via os_print().  If the background color is set to zero, it
 *   indicates "transparent" drawing: subsequent text is displayed with the
 *   "screen" color.  
 */
void os_set_text_color(os_color_t fg, os_color_t bg)
{
    char buf[4];

    /* if we're in plain mode, ignore it */
    if (os_f_plain || S_default_win == 0)
        return;

    /* add the color sequence to the default window's scrollback buffer */
    buf[0] = OSGEN_COLOR;
    buf[1] = osgen_xlat_color_t(fg);
    buf[2] = osgen_xlat_color_t(bg);
    ossaddsb(S_default_win, buf, 3, TRUE);
}

/*
 *   Set the screen color 
 */
void os_set_screen_color(os_color_t color)
{
    /* if we're in plain mode, ignore it */
    if (os_f_plain || S_default_win == 0)
        return;

    /* set the new background color in the default buffer */
    S_default_win->base.fillcolor = osgen_xlat_color_t(color);
    S_default_win->base.oss_fillcolor =
        ossgetcolor(OSGEN_COLOR_TEXT, osgen_xlat_color_t(color), 0, 0);

    /* redraw the window if we don't have a scheduled redraw already */
    if (!S_deferred_redraw
        && !(S_default_win->base.flags & OSGEN_DEFER_REDRAW))
        osgen_redraw_win(&S_default_win->base);
}

/* ------------------------------------------------------------------------ */
/*
 *   Banners 
 */

/*
 *   create a banner window 
 */
void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units, unsigned long style)
{
    osgen_win_t *win;

    /* we don't support banners in plain mode */
    if (os_f_plain)
        return 0;

    /* if the parent is null, it means that it's a child of the main window */
    if (parent == 0)
        parent = &S_main_win->base;

    /* check for a supported window type */
    switch(wintype)
    {
    case OS_BANNER_TYPE_TEXT:
        /* 
         *   Create a text window.  We don't support scrollback in the UI in
         *   banner windows, so we only need enough for what's on the screen
         *   for redrawing.  Overallocate by a bit, though, to be safe in
         *   case the screen grows later.  
         */
        win = (osgen_win_t *)osgen_create_txtwin(where, other, parent,
            G_oss_screen_height * G_oss_screen_width * 2,
            G_oss_screen_height * 2);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /*
         *   Create a text grid window.  Make it ten lines high at the
         *   current screen width; we'll automatically expand this
         *   allocation as needed later, so this size doesn't have to be a
         *   perfect guess; but the closer we get the better, as it's more
         *   efficient to avoid reallocating if possible.  
         */
        win = (osgen_win_t *)osgen_create_gridwin(where, other, parent,
            G_oss_screen_width, 10);
        break;

    default:
        /* unsupported type - return failure */
        return 0;
    }
    
    /* if that failed, return null */
    if (win == 0)
        return 0;

    /* set the alignment */
    win->alignment = align;

    /*
     *   Start out width a zero size in the settable dimension, and the
     *   current main text area size in the constrained dimension. 
     */
    if (align == OS_BANNER_ALIGN_LEFT || align == OS_BANNER_ALIGN_RIGHT)
    {
        /* the width is the settable dimension for a left/right banner */
        win->wid = 0;
        win->ht = (S_main_win != 0
                   ? S_main_win->base.ht : G_oss_screen_height);
    }
    else
    {
        /* the height is the settable dimension for a top/bottom banner */
        win->wid = (S_main_win != 0
                    ? S_main_win->base.wid : G_oss_screen_width);
        win->ht = 0;
    }

    /* set auto-vscroll mode if they want it */
    if ((style & OS_BANNER_STYLE_AUTO_VSCROLL) != 0)
        win->flags |= OSGEN_AUTO_VSCROLL;

    /* 
     *   Note the MORE mode style, if specified.  MORE mode implies
     *   auto-vscroll, so add that style as well if MORE mode is requested.  
     */
    if ((style & OS_BANNER_STYLE_MOREMODE) != 0)
        win->flags |= OSGEN_AUTO_VSCROLL | OSGEN_MOREMODE;

    /* note the "strut" style flags, if specified */
    if ((style & OS_BANNER_STYLE_VSTRUT) != 0)
        win->flags |= OSGEN_VSTRUT;
    if ((style & OS_BANNER_STYLE_HSTRUT) != 0)
        win->flags |= OSGEN_HSTRUT;

    /* remember the requested size */
    win->size = siz;
    win->size_type = siz_units;

    /* 
     *   if the window has a non-zero size, recalculate the layout; if the
     *   size is zero, we don't have to bother, since the layout won't affect
     *   anything on the display 
     */
    if (siz != 0)
        osgen_recalc_layout();

    /* return the window */
    return win;
}

/*
 *   delete a banner 
 */
void os_banner_delete(void *banner_handle)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* delete the window */
    osgen_delete_win(win);

    /* recalculate the display layout */
    osgen_recalc_layout();
}

/*
 *   orphan a banner - treat this exactly like delete 
 */
void os_banner_orphan(void *banner_handle)
{
    os_banner_delete(banner_handle);
}

/*
 *   get information on the banner 
 */
int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* set the alignment */
    info->align = win->alignment;

    /* set the flags */
    info->style = 0;
    if ((win->flags & OSGEN_AUTO_VSCROLL) != 0)
        info->style |= OS_BANNER_STYLE_AUTO_VSCROLL;
    if ((win->flags & OSGEN_MOREMODE) != 0)
        info->style |= OS_BANNER_STYLE_MOREMODE;
    if ((win->flags & OSGEN_HSTRUT) != 0)
        info->style |= OS_BANNER_STYLE_HSTRUT;
    if ((win->flags & OSGEN_VSTRUT) != 0)
        info->style |= OS_BANNER_STYLE_VSTRUT;

    /* set the character size */
    info->rows = win->ht;
    info->columns = win->wid;

    /* we're a character-mode platform, so we don't have a pixel size */
    info->pix_width = 0;
    info->pix_height = 0;

    /* 
     *   We are designed for fixed-pitch character-mode displays only, so we
     *   support <TAB> alignment by virtue of our fixed pitch.
     */
    info->style |= OS_BANNER_STYLE_TAB_ALIGN;

    /* we do not do our own line wrapping */
    info->os_line_wrap = FALSE;

    /* indicate success */
    return TRUE;
}

/*
 *   clear the contents of a banner 
 */
void os_banner_clear(void *banner_handle)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* clear the window */
    osgen_clear_win(win);
}

/*
 *   display text in a banner 
 */
void os_banner_disp(void *banner_handle, const char *txt, size_t len)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* write the text according to the window type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        /* normal text window - write the text into the scrollback buffer */
        ossaddsb((osgen_txtwin_t *)win, txt, len, TRUE);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* text grid - write the text into the grid */
        osgen_gridwin_write((osgen_gridwin_t *)win, txt, len);
        break;
    }
}

/*
 *   set the text attributes in a banner 
 */
void os_banner_set_attr(void *banner_handle, int attr)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;
    char buf[3];

    /* if the attributes aren't changing, ignore it */
    if (attr == win->txtattr)
        return;

    /* set the attribute according to the window type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        /* add the color sequence to the window's scrollback buffer */
        buf[0] = OSGEN_ATTR;
        buf[1] = (char)attr;
        ossaddsb((osgen_txtwin_t *)win, buf, 2, TRUE);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* text grid windows don't use attributes - ignore it */
        break;
    }
}

/*
 *   set the text color in a banner
 */
void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;
    char buf[4];

    /* set the color according to the window type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXT:
        /* add the color sequence to the window's scrollback buffer */
        buf[0] = OSGEN_COLOR;
        buf[1] = osgen_xlat_color_t(fg);
        buf[2] = osgen_xlat_color_t(bg);
        ossaddsb((osgen_txtwin_t *)win, buf, 3, TRUE);
        break;

    case OS_BANNER_TYPE_TEXTGRID:
        /* simply set the current color in the window */
        win->txtfg = osgen_xlat_color_t(fg);
        win->txtbg = osgen_xlat_color_t(bg);
        break;
    }
}

/*
 *   set the window color in a banner 
 */
void os_banner_set_screen_color(void *banner_handle, os_color_t color)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* set the new background color in the window */
    win->fillcolor = osgen_xlat_color_t(color);
    win->oss_fillcolor = ossgetcolor(OSGEN_COLOR_TEXT, win->fillcolor, 0, 0);

    /* redraw the window if we don't have a redraw scheduled already */
    if (!S_deferred_redraw && !(win->flags & OSGEN_DEFER_REDRAW))
        osgen_redraw_win(win);
}

/*
 *   flush text in a banner 
 */
void os_banner_flush(void *banner_handle)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* if we're deferring redrawing, redraw now */
    osgen_redraw_win_if_needed(FALSE, win);
}

/*
 *   set the size 
 */
void os_banner_set_size(void *banner_handle, int siz, int siz_units,
                        int is_advisory)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* if the size isn't changing, do nothing */
    if (win->size == siz && win->size_type == siz_units)
        return;

    /* 
     *   if the size is only advisory, ignore it, since we do implement
     *   size-to-contents 
     */
    if (is_advisory)
        return;

    /* set the new size */
    win->size = siz;
    win->size_type = siz_units;

    /* recalculate the layout */
    osgen_recalc_layout();
}

/*
 *   calculate the content height, for os_banner_size_to_contents()
 */
static size_t oss_get_content_height(osgen_win_t *win)
{
    size_t y;
    osgen_win_t *chi;
    
    /* start with our own maximum 'y' size */
    y = win->ymax + 1;

    /* scan children for vertical strut styles, and include any we find */
    for (chi = win->first_child ; chi != 0 ; chi = chi->nxt)
    {
        /* if this is a vertical strut, include its height as well */
        if ((chi->flags & OSGEN_VSTRUT) != 0)
        {
            /* calculate the child height */
            size_t chi_y = oss_get_content_height(chi);

            /* 
             *   if the child is horizontal, add its height to the parent's;
             *   otherwise, it shares the same height, so use the larger of
             *   the parent's natural height or the child's natural height 
             */
            if (chi->alignment == OS_BANNER_ALIGN_TOP
                || chi->alignment == OS_BANNER_ALIGN_BOTTOM)
            {
                /* it's horizontal - add its height to the parent's */
                y += chi_y;
            }
            else
            {
                /* it's vertical - they share a common height */
                if (y < chi_y)
                    y = chi_y;
            }
        }
    }

    /* return the result */
    return y;
}

/*
 *   calculate the content width, for os_banner_size_to_contents() 
 */
static size_t oss_get_content_width(osgen_win_t *win)
{
    size_t x;
    osgen_win_t *chi;
    
    /* start with the maximum 'x' size we've seen in the window */
    x = win->xmax + 1;

    /* scan children for horizontal strut styles, and include any we find */
    for (chi = win->first_child ; chi != 0 ; chi = chi->nxt)
    {
        /* if this is a horizontal strut, include its height as well */
        if ((chi->flags & OSGEN_HSTRUT) != 0)
        {
            /* calculate the child width */
            size_t chi_x = oss_get_content_width(chi);

            /* 
             *   if the child is vertical, add its width to the parent's;
             *   otherwise, it shares the same width, so use the larger of
             *   the parent's natural width or the child's natural width 
             */
            if (chi->alignment == OS_BANNER_ALIGN_LEFT
                || chi->alignment == OS_BANNER_ALIGN_RIGHT)
            {
                /* it's vertical - add its width to the parent's */
                x += chi_x;
            }
            else
            {
                /* it's horizontal - they share a common width */
                if (x < chi_x)
                    x = chi_x;
            }
        }
    }

    /* return the result */
    return x;
}

/*
 *   size a banner to its contents 
 */
void os_banner_size_to_contents(void *banner_handle)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* the sizing depends on the window's alignment */
    if (win->alignment == OS_BANNER_ALIGN_TOP
        || win->alignment == OS_BANNER_ALIGN_BOTTOM)
    {
        size_t newy;

        /* calculate the new height */
        newy = oss_get_content_height(win);

        /* 
         *   if this is the same as the current height, there's no need to
         *   redraw anything 
         */
        if (win->ht == newy)
            return;

        /* set the new size as a fixed character-cell size */
        win->size = newy;
        win->size_type = OS_BANNER_SIZE_ABS;
    }
    else
    {
        size_t newx;

        /* calculate the new width */
        newx = oss_get_content_width(win);

        /* if the size isn't changing, there's no need to redraw */
        if (win->wid == newx)
            return;

        /* set the new size as a fixed character-cell size */
        win->size = newx;
        win->size_type = OS_BANNER_SIZE_ABS;
    }

    /* recalculate the window layout */
    osgen_recalc_layout();
}

/*
 *   get the width, in characters, of the banner window 
 */
int os_banner_get_charwidth(void *banner_handle)
{
    /* return the current width from the window */
    return ((osgen_win_t *)banner_handle)->wid;
}

/*
 *   get the height, in characters, of the banner window 
 */
int os_banner_get_charheight(void *banner_handle)
{
    /* return the current height from the window */
    return ((osgen_win_t *)banner_handle)->ht;
}

/*
 *   start HTML mode in a banner 
 */
void os_banner_start_html(void *banner_handle)
{
    /* we don't support HTML mode, so there's nothing to do */
}

/*
 *   end HTML mode in a banner 
 */
void os_banner_end_html(void *banner_handle)
{
    /* we don't support HTML mode, so there's nothing to do */
}

/*
 *   set the output position in a text grid window 
 */
void os_banner_goto(void *banner_handle, int row, int col)
{
    osgen_win_t *win = (osgen_win_t *)banner_handle;

    /* check the window type */
    switch(win->win_type)
    {
    case OS_BANNER_TYPE_TEXTGRID:
        /* it's a text grid - move the output position */
        win->y = row;
        win->x = col;
        break;

    default:
        /* 
         *   this operation is meaningless with other window types - simply
         *   ignore it 
         */
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Non-RUNTIME version 
 */
#else /* RUNTIME */

void os_set_text_attr(int attr)
{
    /* attributes aren't supported in non-RUNTIME mode - ignore it */
}

void os_set_text_color(os_color_t fg, os_color_t bg)
{
    /* colors aren't supported in non-RUNTIME mode - ignore it */
}

void os_set_screen_color(os_color_t color)
{
    /* colors aren't supported in non-RUNTIME mode - ignore it */
}

/*
 *   Banners aren't supported in plain mode 
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

int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
    return FALSE;
}

void os_banner_clear(void *banner_handle)
{
}

int os_banner_get_charwidth(void *banner_handle)
{
    return 0;
}

int os_banner_get_charheight(void *banner_handle)
{
    return 0;
}

void os_banner_disp(void *banner_handle, const char *txt, size_t len)
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
    if (os_f_plain || S_default_win == 0)
        return;

    /* clear the default window */
    osgen_clear_win(&S_default_win->base);
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
        /* 
         *   we don't support any of these features - set the result to 0
         *   to indicate this 
         */
        *result = 0;

        /* return true to indicate that we recognized the code */
        return TRUE;

    case SYSINFO_INTERP_CLASS:
        /* we're a text-only character-mode interpreter */
        *result = SYSINFO_ICLASS_TEXT;
        return TRUE;

#ifdef RUNTIME

    case SYSINFO_BANNERS:
        /* 
         *   we support the os_banner_xxx() interfaces, as long as we're not
         *   in "plain" mode 
         */
        *result = !os_f_plain;
        return TRUE;

#endif /* RUNTIME */

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


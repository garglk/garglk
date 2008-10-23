/*  
 *   Copyright (c) 1987, 1988 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*

Name
----

  osatari  - Operating System dependent definitions for Atari ST/TT/Falcon

Notes
-----

   This version of osatari.c is a complete rewrite of the version 1.X
file.  The output code has been changed from VDI to VT52, resulting in
a substantial speed improvement when under running software screen
accelerators, and improved portability.  This version only assumes a VT52
window that is at least 80x24.

   This file was designed and tested with GCC 2.3.1 Patchlevel 1, but
in theory should work with most Atari compilers since it uses very few
OS fuctions.

Dave Baggett
dmb@ai.mit.edu

Worries
-------

o Older versions of the desktop upcase all the command line args. This will
  cause problems in tcd.c since it only checks for lower case args.  

o Should be able to disable VT52 stuff in compiler so errors can be saved
  to a file.

o tc -p does not work for some reason, though tc -p -q does and tc -p uu2.t
  does as well.  This is a problem with tcd.c


Modifications
-------------

 26-Jul-93      dmb     First attempt at fixing monochrome problems.
                        Updated to GCC 2.4.4 P1.  (But it wouldn't
                        build with it, so went back to 2.3.1)
 23-Jan-93      dmb     Added file selector code.
 19-Jan-93      dmb     Curses turned out to be too slow, so I wrote the
                        os_ routins using VT52 codes.  Even supports
                        the DOS colors!
 18-Jan-93      dmb     Updated for TADS 2 and GCC 2.3.1 P1
*
*/

#include "os.h"         /* will include osatari.h for us since -DATARI */

#include <alloc.h>
#include <ctype.h>
#include <osbind.h>
#include <stdio.h>
#include <string.h>
#include <aesbind.h>    /* Get shel_read */
#include <gemfast.h>    /* Get graf_mouse */

/*
 * Use VT52 shortcuts whenever possible instead of repainting the screen
 * "by hand."  This makes text output go much faster when a software screen
 * accelerator is installed.  If screen output is somehow hosed, try
 * turning this off; when it's off, ALL screen manipulation is handled
 * by vt_redraw.
 */
#define VT_OPTIMIZE

/*
 * Big stack -- extra safe
 */
long _stksize = 48000L;

/*
 * Ctrl-C hit?
 */
static int break_set = 0;

/*
 * Assume medium rez, where we have four colors.
 * Our palette is chosen simply so that we can get the same color 
 * combinations we have on the PC.
 *
 * A fancier implementation would read the trcolor.dat file like
 * the DOS version does.
 */
static int drez;
static int dpalette[16];
static int palette[16] = {
        0x000, 0x007, 0x777, 0x555,
        0x000, 0x000, 0x000, 0x000,
        0x000, 0x000, 0x000, 0x000,
        0x000, 0x000, 0x000, 0x000
};
#define osBLACK 0
#define osBLUE 1
#define osBRIGHTWHITE 2
#define osWHITE 3

/*
 * In monochrome mode, the color indices are different.
 */
#define osmNORMAL 0
#define osmREVERSE 1

/*
 * We combine two colors into a color byte (foreground and background).
 */
#define colbyte(f, b) (((f & 3) << 2) | (b & 3))
#define bytecol(byte, f, b) ((f = (byte >> 2) & 3), (b = (byte & 3)))

#define VTC (drez == 2 ? 0 : 64)

/*
 * Our own private concept of the screen and cursor.
 */
#define ROWS 24
#define COLS 80
static char     *screen[ROWS];
static char     *colors[ROWS];
static int      X, Y;

static char pathbuf[70];

static void     break_handler();
static char     *file_select();
static char     *path2fname();
static void     remove_wildcard();

/*
 * Make sure chars get typecast to int for this function.
 */
int vt_color(int c);

curs_hide()
{
        Cursconf(0, 0);
}

curs_show()
{
        Cursconf(1, 0);
}

/*
 * Set up for VT52 screen handling.  Called by os_init.
 */
vt_init()
{
        register int    row, col;

        /*
         * Init globals used by the system.
         */
        score_column = COLS - 10;
        max_line = ROWS - 1;
        max_column = COLS - 1;
        G_os_pagelength = max_line - text_line - 1;
        G_os_linewidth = COLS + 1;

        /*
         * Allocate our private screen data structure.
         */     
        for (row = 0; row < ROWS; row++) {
                screen[row] = (char *) calloc(COLS + 1, 1);
                colors[row] = (char *) calloc(COLS, 1);
                for (col = 0; col < COLS; col++) {
                        screen[row][col] = ' ';
                        colors[row][col] = colbyte(osWHITE, osBLACK);
                }
                screen[row][COLS] = 0;
        }

        vt_setup();
}

/*
 * Set up screen for VT52.
 * This is called by os_askfile to restore the screen to the normal
 * state, so it shouldn't allocate data structures.
 */
vt_setup()
{
        int     i;

        /*
         * Get rid of the mouse
         */
        graf_mouse(M_OFF, NULL);

        /*
         * Get desktop resolution.  If monochrome, note the fact so that
         * we can make sure our color choices get set correctly later on.
         * If low resolution, change to medium resolution automagically.
         *
         * If any other resolution, don't do anything different, and hope
         * things work out OK.
         */
        drez = Getrez();        
        if (drez == 0)
                Setscreen(-1L, -1L, 1);

        if (drez != 2) {
                /*
                 * Get desktop palette.
                 */
                for (i = 0; i < 16; i++)
                        dpalette[i] = Setcolor(i, -1);

                /*
                 * Set our palette.
                 */ 
                Setpalette(palette);

                /*
                 * Tell the rest of the system what good colors
                 * are for various things.
                 */
                sdesc_color = colbyte(osWHITE, osBLUE);
                ldesc_color = colbyte(osBLACK, osWHITE);
                debug_color = colbyte(osBRIGHTWHITE, osBLACK);
                text_color  = colbyte(osWHITE, osBLACK);
                text_bold_color = colbyte(osBRIGHTWHITE, osBLACK);
                text_normal_color = text_color;
        }
        else {
                /*
                 * Monochrome -- note different color name constants
                 */
                sdesc_color = colbyte(osmREVERSE, osmREVERSE);
                ldesc_color = colbyte(osmREVERSE, osmREVERSE);
                debug_color = colbyte(osmNORMAL, osmNORMAL);
                text_color  = colbyte(osmNORMAL, osmNORMAL);
                text_bold_color = colbyte(osmREVERSE, osmREVERSE);
                text_normal_color = text_color;
        }

        /*
         * Set default text color
         */
        vt_color(text_color);

        /*
         * Clear the screen, set to no-wrap mode, and enable cursor.
         * Also, set drawing colors to normal.
         */
        Cconws("\033E\033w\033e");
        X = 0;
        Y = 0;

        /*
         * Make the cursor not blink.
         */ 
        Cursconf(2, 0);
}

vt_term()
{
        vt_cleanup();
}

/*
 * Cleanup -- restore palette and sane VT52 state.
 */
vt_cleanup()
{
        /*
         * Restore desktop resolution (if we switched).
         */
        if (drez != Getrez())
                Setscreen(-1L, -1L, drez);

        if (drez != 2) {
                /*
                 * Restore desktop palette.
                 */ 
                Setpalette(dpalette);

                Cconws("\033b");
                Cconout(3 + VTC);
                Cconws("\033c");
                Cconout(0 + VTC);       
        }

        /*
         * Bring the mouse back
         */
        graf_mouse(M_ON, NULL);
        
        /*
         * Make the cursor visible
         */
        curs_show();
}

vt_putcolor(buf, pos, c)
        char    *buf;
        int     pos;
        int     c;
{       
        register int    f, b;

        bytecol(c, f, b);
        if (drez == 2) {
                if (f == osmREVERSE) {
                        buf[pos++] = '\033';
                        buf[pos++] = 'p';
                }
                else {
                        buf[pos++] = '\033';
                        buf[pos++] = 'q';
                }
        }
        else {
                buf[pos++] = '\033';
                buf[pos++] = 'b';
                buf[pos++] = f + VTC;
                buf[pos++] = '\033';
                buf[pos++] = 'c';
                buf[pos++] = b + VTC;
        }                       

        return pos;
}

/*
 * Set the color of subsequent text.
 */
vt_color(c)
        register int    c;
{
        register int    f, b;

        bytecol(c, f, b);

        /*
         * In monochrome mode, we never set colors.
         * Instead, we turn on and off reverse video mode.
         */
        if (drez == 2) {
                if (f == osmREVERSE)
                        Cconws("\033p");
                else
                        Cconws("\033q");
        }
        else {
                Cconws("\033b");
                Cconout(f + VTC);
                Cconws("\033c");
                Cconout(b + VTC);       
        }
}

/*
 * Update the real screen so it matches our idea of what's on the screen.
 */
vt_flush()
{
        vt_redraw(0, ROWS - 1);
}

/*
 * Redraw a portion of the screen.  This is a bit tricky because we have
 * to make sure we get the colors right, but we don't want to output a
 * character at a time.  We build a string for each line of the region
 * that contains all the text and the minimum necessary color change
 * escape sequences.
 */
vt_redraw(top, bottom)
        int     top, bottom;
{
        static char     ds[7 * COLS + 1] = {0};
        
        register int    row, col, pos;
        register char   **s = screen;
        register char   **c = colors;
        int             color;  

        curs_hide();
        
        /*
         * Position to top line, column zero.
         */
        vt_loc(top, 0);

        /*
         * Redraw each line between top line and bottom line.
         * We have to build a string from our text and color
         * information so we can write the output a line at a time
         * rather than a character at a time.
         */
        color = c[top][0];
        vt_color(color);
        for (row = top; row <= bottom; row++) {
                Cconws("\033j");        /* save cursor position */
                
                /*
                 * Combine color and text information into a single
                 * string for this line.
                 */
                for (col = 0, pos = 0; col < COLS; col++) {
                        /*
                         * Do we have to change text color? 
                         * If so, add the appropriate escape sequence
                         * to our string for this line, before the 
                         * first character to be printed in the new
                         * color.
                         */
                        if (c[row][col] != color) {
                                register int    f, b, nc;

                                nc = c[row][col];
                                pos = vt_putcolor(ds, pos, nc);
                                color = nc;
                        }

                        ds[pos++] = s[row][col];
                }       
                ds[pos] = 0;

                Cconws(ds);             /* blast the string */
                Cconws("\033k");        /* restore cursor position */
                Cconws("\033B");        /* down a line */
        }

        /*
         * Reposition cursor to its former location
         */
        vt_loc(Y, X);
        
        curs_show();
}

/*
 * Move the VT52 cursor.  Note that this has nothing to do with the
 * virtual cursor that TADS tells us to move around -- that's updated
 * by ossloc.
 */
vt_loc(y, x)
        int     y, x;
{
        Cconws("\033Y");
        Cconout(y + ' ');
        Cconout(x + ' ');
}

/*
 * Scroll region down a line.
 * This is equivalent to inserting a line at the top of the region.
 */
ossscu(top, left, bottom, right, blank_color)
        int     top, left, bottom, right;
        int     blank_color;
{
        register int    r1, r2, col;
        register char   **s = screen;
        register char   **c = colors;
        
        curs_hide();

        /*
         * Update our internal version
         */
        for (r1 = bottom - 1, r2 = bottom; r1 >= top; r1--, r2--)
                for (col = left; col <= right; col++) {
                        s[r2][col] = s[r1][col];
                        c[r2][col] = c[r1][col];
                }       
        for (col = left; col <= right; col++) {
                s[r2][col] = ' ';
                c[r2][col] = blank_color;
        }

        /*
         * If we can duplicate the effect of this scroll on the screen
         * with a simple VT52 command, do so; otherwise, refresh by
         * redrawing every affected line.
         */
#ifdef  VT_OPTIMIZE
        if (left == 0 && right == COLS - 1 && bottom == ROWS - 1) {
                vt_loc(top, 0);
                vt_color(blank_color);
                Cconws("\033L");
                vt_loc(Y, X);
        }
        else {
#endif  
                vt_redraw(top, bottom);
#ifdef  VT_OPTIMIZE
        }
#endif  

        curs_show();
}

/*
 * Scroll region up a line
 * This is equivalent to deleting a line at the top of the region and pulling
 * everything below it up.
 */
ossscr(top, left, bottom, right, blank_color)
        int     top, left, bottom, right;
        int     blank_color;
{
        register int    r1, r2, col;
        register char   **s = screen;
        register char   **c = colors;
        
        curs_hide();
        
        /*
         * Update our internal version
         */
        for (r1 = top, r2 = top + 1; r2 <= bottom; r1++, r2++)
                for (col = left; col <= right; col++) {
                        s[r1][col] = s[r2][col];
                        c[r1][col] = c[r2][col];
                }       
        for (col = left; col <= right; col++) {
                s[r1][col] = ' ';
                c[r1][col] = blank_color;
        }

        /*
         * If we can duplicate the effect of this scroll on the screen
         * with a simple VT52 command, do so; otherwise, refresh by
         * redrawing every affected line.
         */
#ifdef  VT_OPTIMIZE
        if (left == 0 && right == COLS - 1 && bottom == ROWS - 1) {
                vt_loc(top, 0);
                vt_color(blank_color);
                Cconws("\033M");
                vt_loc(Y, X);
        }
        else {
#endif  
                vt_redraw(top, bottom);
#ifdef  VT_OPTIMIZE
        }
#endif  

        curs_show();
}

/*
 * Clear region (fill with spaces)
 */
ossclr(top, left, bottom, right, blank_color)
        int     top, left, bottom, right;
        int     blank_color;
{
        register int    row, col;
        register char   **s = screen;
        register char   **c = colors;

        curs_hide();

        /*
         * Update our internal version
         */
        for (row = top; row <= bottom; row++)
                for (col = left; col <= right; col++) {
                        s[row][col] = ' ';
                        c[row][col] = blank_color;
                }

        /*
         * If we can duplicate the effect of this scroll on the screen
         * with a simple VT52 command, do so; otherwise, refresh by
         * redrawing every affected line.
         */
#ifdef  VT_OPTIMIZE
        if (left == 0 && right == COLS - 1 && bottom == ROWS - 1) {
                vt_color(blank_color);
                if (top == 0) {
                        Cconws("\033E");
                        vt_loc(Y, X);
                }
                else {
                        vt_loc(top, 0);
                        Cconws("\033J");
                        vt_loc(Y, X);
                }       
        }
        else {
#endif  
                vt_redraw(top, bottom);
#ifdef  VT_OPTIMIZE
        }
#endif  

        curs_show();
}

/*
 * Locate (input) cursor at given row and column.
 * Nore that this is never used to determine where things are drawn.
 * It's only used for aesthetics; i.e., showing the user where input
 * will be taken from next.
 */ 
ossloc(row, col)
        int     row, col;
{
        vt_loc(row, col);
        
        /*
         * Update internal cursor position
         */
        X = col;
        Y = row;        
}

/*
 * Display msg with color at coordinates (y, x).
 * The color must be in the range 0 <= color <= 16, and specifies both
 * foreground and background colors.
 */
ossdsp(y, x, color, msg)
        int             y, x;   
        int             color;
        register char   *msg;
{
        register int    col;
        register char   *s, *m = msg, *c;

        curs_hide();

        if (y < ROWS && x < COLS) {
                s = &screen[y][x];
                c = &colors[y][x];
        }       
        else
                return; 
        
        /*
         * Update our own version of the screen.
         */
        for (col = x; *m && col < COLS; col++) {
                *s++ = *m++;
                *c++ = color;
        }

#ifdef  VT_OPTIMIZE
        /*
         * Now update the real screen using VT52 codes.
         */
        vt_loc(y, x);
        vt_color(color);
        Cconws(msg);
        vt_loc(Y, X);
#else
        vt_redraw(y, y);
#endif

        curs_show();
}

/*
 * Show the user where we are in the compilation.
 */
os_progress(fname, linenum)
        const char *fname;
        unsigned long linenum;
{
        static char     sl[COLS - 32 + 1];
        register int    i;
        register char   *s = sl;

        sprintf(sl, "File: %-12s Line: %-7ld", fname, linenum);
        ossdsp(sdesc_line, COLS - 32, sdesc_color, sl);
}

/*
 *   os_init calls the windowing support function that initializes
 *   the standard output window.
 */
int 
os_init(argc, argv, prompt, buf, bufsiz)
        int     *argc;
        char    *argv[];
        char    *prompt;
        char    *buf;
        int     bufsiz;
{
        char    *mybuf;
        char    tail[128];
        void    os_remext();
        void    os_defext();
        char    *p;
        int     l;
        FILE    *fp;

        appl_init();
        vt_init();

        /*
         *   Clear the screen and put an introductory message on the screen
         */
        ossclr(0, 0, max_line, max_column, text_color);

        /*
         * Make the status line the right color
         */
        ossclr(sdesc_line, 0, sdesc_line, max_column, sdesc_color);
        ossdsp(sdesc_line, 0, sdesc_color, 
                "TADS: The Text Adventure Development System");
        text_lastcol = text_column;

        /*
         *   Initialize pathbuf with current directory
         */
        mybuf = pathbuf;
        *mybuf++ = Dgetdrv() + 'A';
        *mybuf++ = ':';
        Dgetpath( mybuf, 0 );
        mybuf += strlen( mybuf );
        if ( *( mybuf-1 ) != '\\' ) *mybuf++ = '\\';
        strcpy( mybuf, "*.SAV" );

#ifdef USE_SCROLLBACK

    osssbini(32767);      /* allocate a scrollback buffer */

#endif /* USE_SCROLLBACK */

    /*
     *   Set up the command line's first argument with the root name of
     *   the executable, or command tail if present.  This allows users to
     *   do one of three things:
     *     - Run from shells.  If we see something in the basepage command
     *       line, we'll use that.
     *     - Call the run-time TR.TTP, and type the name of the game to play
     *       into the TOS-Takes-Parameters dialogue box.
     *     - Rename the run-time XYZ.PRG, and call the data file XYZ.GAM;
     *       upon double-clicking, we'll try to read XYZ.GAM.
     *   Note that the first method overrides the second; the two are
     *   mutually exclusive.  Note that this bit is skipped if we didn't
     *   get enough valid arguments (non-null argv and buf).
     */
    if ( argv && buf )
    {
        *argc = 2;      /* set the new argument count - we'll find something */
        shel_read( buf, tail );    /* get the program name and argument list */

#if 0
        /*
         *   Check the basepage entry, because shells will write that.  If
         *   nothing's there, check the command tail from shel_read.  If we
         *   find nothing in either place, we'll use the program's name as
         *   our argument.
         */     
        if ( BP->p_cmdlin[1] )       /* use basepage command line if present */
            strcpy( buf, BP->p_cmdlin+1 );
        else 
#endif  
             if ( tail[0] )                   /* use command tail if present */
            strcpy( buf, tail+1 );
        /*
         *   Clean up the argument - remove leading and trailing junk
         */
        for ( p=buf ; isspace( *p ) ; ++p );
        while (( l=strlen( p )) && ( p[l-1] <= ' ' )) p[l-1] = '\0';
        argv[1] = p;
        os_remext( buf );
        
        /*
         *   Now see if we can find a .GAM file for this game.  If not,
         *   ask for a filename in the usual manner.
         */
        os_defext( buf, "GAM" );
        if ( fp = fopen( buf, "rb" ))
        {
            fclose( fp );
        }
        else
        {
            char savpath[128];
            
            strcpy( savpath, pathbuf );
            os_remext( pathbuf );
            if (!strncmp(prompt, "Debug", 5)) os_defext(pathbuf, "TD0");
            else os_defext( pathbuf, "GAM" );
            if ( os_askfile( (char *)0, buf, 128, OS_AFP_OPEN, OSFTGAME )
                 != OS_AFE_SUCCESS )
                return( 1 );
            strcpy( pathbuf, savpath );
        }
    }

    return 0;
}

void
os_uninit()
{
}

void
os_term(rc)
        int rc;
{
        vt_term();
        exit(rc);
}

/*
 *  os_waitc waits for a character to be typed.  It is used to allow the user
 *  to say it's OK to continue after a "[More]" prompt.  Acceptable keys are
 *  OS dependent; ideally, any key will do.  The key should not be echoed.
 *
 *  Note!  This routine must NOT allow scrollback mode to be entered.  The
 *  problem is that out put may be done on exiting scrollback mode, to restore
 *  the status line, which would trash the output now in progress.  Hence,
 *  do not honor the scrollback key.
 */
void
os_waitc()
{
    int c;

    c = os_getc();               /* get a single character from the keyboard */
    if (!c)
            c = os_getc();  /* if command character, fetch second part, too */
}

/*
 *   os_getc waits for a single character to be typed and returns its value.
 *   The character should not be echoed.  It should not be necessary to press
 *   the ENTER or RETURN key to terminate the input.
 *   
 *   When used in RUNTIME mode, we want command line editing.  To accommodate
 *   this feature, we must return special codes when arrow keys and other
 *   command keys are hit.  When one of these special command keys is used,
 *   os_getc() returns 0, and will return the CMD_xxx value (defined earlier
 *   in this file) appropriate to the key on the next call.  
 */
int os_getc()
{
        static int      cmdval;
        long            d;

tryAgain:
        if (cmdval) {
                int tmpcmd = cmdval;    /* save command for a moment */
                cmdval = 0;     /* clear the flag - next call gets another key */
                return(tmpcmd); /* return the command that we saved */
        }
        else
                d = Bconin(2);  /* no command pending - get another key */

#ifdef RUNTIME

        switch ((int)(d >> 16)) {
                case 0x4b:
                        cmdval = CMD_LEFT;
                        break;
                case 0x4d:
                        cmdval = CMD_RIGHT;
                        break;
                case 0x48:
                        cmdval = CMD_UP;
                        break;
                case 0x50:
                        cmdval = CMD_DOWN;
                        break;
                case 0x53:
                        cmdval = CMD_DEL;
                        break;
                case 0x47:
                        cmdval = CMD_HOME;
                        break;
                case 0x3b:  /* F1 */
                        cmdval = CMD_SCR;
                        break;
                case 0x43:  /* F9 - page up */
                        cmdval = CMD_PGUP;
                        break;
                case 0x44:  /* F10 - page down */
                        cmdval = CMD_PGDN;
                        break;

                default:
                switch ((int)(d & 0xff)) {
                        case 1:     /* ^A */
                                cmdval = CMD_HOME;
                                break;
                        case 2:     /* ^B */
                                cmdval = CMD_LEFT;
                                break;
                        case 4:     /* ^D */
                                cmdval = CMD_DEL;
                                break;
                        case 5:     /* ^E */
                                cmdval = CMD_END;
                                break;
                        case 6:     /* ^F */
                                cmdval = CMD_RIGHT;
                                break;
                        case 11:    /* ^K */
                                cmdval = CMD_DEOL;
                                break;
                        case 14:    /* ^N */
                                cmdval = CMD_DOWN;
                                break;
                        case 16:    /* ^P */
                                cmdval = CMD_UP;
                                break;
                        case 21:    /* ^U */
                                cmdval = CMD_KILL;
                                break;
                }
        }

#endif /* RUNTIME */

        if (cmdval)
                return 0;
        else if (d & 0xff)
                return (int) (d & 0xff);
        else
                goto tryAgain;
}

/*
 *   os_getc_raw() works like os_getc(), but doesn't translate keys to
 *   high-level CMD_xxx codes.  
 */
int os_getc_raw()
{
        static int      cmdval;
        long            d;

tryAgain:
        if (cmdval) {
                int tmpcmd = cmdval;           /* save command for a moment */
                cmdval = 0;  /* clear the flag - next call gets another key */
                return(tmpcmd);         /* return the command that we saved */
        }
        else
                d = Bconin(2);      /* no command pending - get another key */

#ifdef RUNTIME

        switch ((int)(d >> 16)) {
        case 0x4b:
                cmdval = CMD_LEFT;
                break;
        case 0x4d:
                cmdval = CMD_RIGHT;
                break;
        case 0x48:
                cmdval = CMD_UP;
                break;
        case 0x50:
                cmdval = CMD_DOWN;
                break;
        case 0x53:
                cmdval = CMD_DEL;
                break;
        case 0x47:
                cmdval = CMD_HOME;
                break;
        case 0x3b:                                                    /* F1 */
                cmdval = CMD_F1;
                break;
        case 0x3c:                                                    /* F2 */
                cmdval = CMD_F2;
                break;
        case 0x3d:                                                    /* F3 */
                cmdval = CMD_F3;
                break;
        case 0x3e:                                                    /* F4 */
                cmdval = CMD_F4;
                break;
        case 0x3f:                                                    /* F5 */
                cmdval = CMD_F5;
                break;
        case 0x40:                                                    /* F6 */
                cmdval = CMD_F6;
                break;
        case 0x41:                                                    /* F7 */
                cmdval = CMD_F7;
                break;
        case 0x42:                                                    /* F8 */
                cmdval = CMD_F8;
                break;
        case 0x43:                                                    /* F9 */
                cmdval = CMD_F9;
                break;
        case 0x44:                                                   /* F10 */
                cmdval = CMD_F10;
                break;

        default:
                /* return other keys unchanged */
                break;
        }

#endif /* RUNTIME */

        if (cmdval)
                return 0;
        else if (d & 0xff)
                return (int) (d & 0xff);
        else
                goto tryAgain;
}

/*
 *   Check for control-break.  Returns status of break flag, and clears
 *   the flag. 
 */
int
os_break()
{
        int     ret;
        
        ret = break_set;
        break_set = 0;
        return ret;
}

/*
 * Handle ^C.  Currently, this doesn't do anything.
 */
static void
break_handler()
{
        break_set = 1;
}

int
os_paramfile(buf)
{
        return 0;
}

/*
 *   os_exeseek  - finds datafile at end of executable.  Note that, on the
 *   ST, the exename will be a flag simply to tell us to use this feature.
 *   We actually need to find the executable's name through an OS call.
 *
 * This is commented out for TADS 2 because I think it may not work with
 * future versions of TOS.
 */
FILE *
os_exeseek( exename )
        char *exename;
{
#ifdef RUNTIME
    FILE *fp, *fopen();
    char  cmd[128], tail[128];
    int   i;
    long  segsiz[4];
    
    shel_read( cmd, tail );
    if (!( fp = fopen( cmd, "rb" ))) return( (FILE *)0 );
    if (fseek( fp, 2L, 0 ))
    {
        fclose( fp );
        return( (FILE *)0 );
    }
    for ( i=0 ; i<4 ; ++i )
    {
        if ( fread( &segsiz[i], sizeof( segsiz[i] ), 1, fp ) != 1 )
        {
            fclose( fp );
            return( (FILE *)0 );
        }
    }
    if (fseek( fp, 10L+segsiz[0]+segsiz[1], 1 ))
    {
        fclose( fp );
        return( (FILE *)0 );
    }
    return( fp );
#else /* RUNTIME */
    return( (FILE *)0 );
#endif
}

int (*os_exfld(fp, siz))()
FILE     *fp;
unsigned  siz;
{
    int      (*extfn)(/*_ void _*/);
    
    if (!(extfn = malloc(siz))) return( (int (*)(/*_ void _*/))0 );
    fread(extfn, siz, 1, fp);
    return(extfn);
}

int (*os_exfil(name))()
char *name;
{
    FILE      *fp;
    unsigned   len;
    int      (*extfn)(/*_ void _*/);
    
    /* open the file and see how big it is to determine our memory needs */
    if (!(fp = fopen(name, "rb"))) return( (int (*)(/*_ void _*/))0 );
    (void)fseek(fp, 0L, 2);
    len = (unsigned)ftell(fp);                   /* get total length of file */
    
    (void)fseek(fp, 0L, 0);
    extfn = os_exfld(fp, len);
    
    fclose(fp);
    return(extfn);
}

int os_excall(extfn, ctx)
int  (*extfn)(/*_ void _*/);
char  *ctx;
{
    return((*extfn)(ctx));
}

#ifndef STD_ASKFILE

int
os_askfile(prompt, reply, replen, prompt_type, file_type)
        char    *prompt;
        char    *reply;
        int     replen;
        int     prompt_type;
        int     file_type;
{
        static char     *r;
        static char     path[255] = ".\\", filename[20] = "";

        r = file_select(path, filename, prompt);
        if (r) {
                if (strlen(r) < replen) {
                        strcpy(reply, r);
                        return OS_AFE_SUCCESS;
                }
        }       

        return OS_AFE_CANCEL;
}

/*
 * Put up a file selector box with prompting text.
 *
 * A pointer to the selected pathname is returned.  If no filename was
 * selected (i.e., the user pressed CANCEL), a NULL is returned.
 *
 * The path and filename strings will be modified.
 */
char *
file_select(path, filename, message)
        char    *path;
        char    *filename;
        char    *message;
{
        static char     p[255];
        int             button;
        
        if (path[0] == '.') {
                /*
                 * Expand current working directory for brain-damaged
                 * GEM file selector.
                 */
                p[0] = (char) Dgetdrv() + 'A';
                p[1] = ':';
                Dgetpath(&p[2], 0);
                strcat(p, &path[1]);
        }
        else {
                strcpy(p, path);
        }

        vt_cleanup();
        Cconws("\033f\033E\033bC\033c@");
        if (message) {
                Cconws(message);
                Cconout('?');
        }
        graf_mouse(M_ON, NULL);
        fsel_input(p, filename, &button);
        vt_setup();
        vt_flush();

        if (button) {
                strcpy(path, p);
                remove_wildcard(p);
                strcat(p, filename);
                return p;
        }
        else
                return NULL;
}

/*
 * Remove wildcards at the end of a pathspec returned by fsel_input
 */
static void
remove_wildcard(p)
        register char   *p;
{
        register char   *base = p;

        /*
         * Find terminating null
         */
        while (*p) p++;

        /*
         * Delete characters until either \ or : are reached.
         */
        while (p != base) {
                if (*p == '\\' || *p == ':')
                        break;

                *p-- = '\0';
        }
}

/*
 * Return a pointer to the start of the filename (8.3) in a complete path.
 */
char *
path2fname(path)
        char    *path;
{
        char    *p;     

        /*
         * Find end of string
         */
        p = path;
        while (*p)
                p++;

        /*
         * Search backwards until we find \, :, /, or beginning of string
         */
        while (*p != '\\' && *p != ':' && *p != '/' && p != path)
                p--;

        if (*p == '\\' || *p == ':' || *p == '/')
                p++;

        return p;
}

#endif  /* STD_ASKFILE */

/*
 * Provide memicmp since it's not a standard libc routine.
 */
memicmp(s1, s2, len)
        char    *s1, *s2;
        int     len;
{
        char    *x1, *x2;       
        int     result;
        int     i;

        x1 = malloc(len);
        x2 = malloc(len);

        if (!x1 || !x2) {
                printf("Out of memory!\n");
                exit(-1);
        }

        for (i = 0; i < len; i++) {
                if (isupper(s1[i]))
                        x1[i] = tolower(s1[i]);
                else
                        x1[i] = s1[i];

                if (isupper(s2[i]))
                        x2[i] = tolower(s2[i]);
                else
                        x2[i] = s2[i];
        }

        result = memcmp(x1, x2, len);
        free(x1);
        free(x2);
        return result;
}


/*
$Header: d:/cvsroot/tads/TADS2/msdos/OSDBG.H,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdbg.h - DOS interface definitions for os routines for debugger
Function
  DOS interface definitions for os routines for debugger
Notes
  none
Modified
  04/21/92 MJRoberts     - creation
*/

#ifndef OSDBG_INCLUDED
#define OSDBG_INCLUDED

/* window definiton */
struct oswdef
{
    int   oswx;                                       /* current x location */
    int   oswy;                                       /* current y location */
    int   oswcolor;                                   /* current text color */
    int   oswx1; /* left edge of window (this column is included in window) */
    int   oswy1;                                      /* top line of window */
    int   oswx2;                                    /* right edge of window */
    int   oswy2;                                   /* bottom line of window */
    int   oswflg;                                       /* flags for window */
#   define OSWFCLIP  0x01                          /* don't wrap long lines */
#   define OSWFMORE  0x02                 /* use [more] mode in this window */
    int   oswmore;                     /* number of lines since last [more] */
};
typedef struct oswdef oswdef;

/* print to the debugger window */
void osdbgpt(oswdef *, char *fmt, ...);

/* locate in the debugger window */
void ossdbgloc(char y, char x);

/* scroll a window up a line */
static void osdbgsc(oswdef *win);

/* clear a window */
void osdbgclr(oswdef *win);

/* initialize debugger UI */
int osdbgini(int rows, int cols);

/* set video page */
int ossvpg(char pg);

/* open a window - set up location */
void osdbgwop(oswdef *win, int x1, int y1, int x2, int y2, int color);

/* get some text */
int osdbggts(oswdef *win, char *buf, int (*cmdfn)(void *, char), void *cmdctx);




#endif /* OSDBG_INCLUDED */

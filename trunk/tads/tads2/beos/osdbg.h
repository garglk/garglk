/*
$Header: d:/cvsroot/tads/TADS2/unix/osdbg.h,v 1.2 1999/05/17 02:52:20 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdbg.h - interface definitions for os routines for debugger
Function
  interface definitions for os routines for debugger
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

#endif /* OSDBG_INCLUDED */

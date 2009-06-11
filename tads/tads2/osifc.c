#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osifc.c - operating system interface - portable definitions
Function
  This module contains portable code for the TADS OS interface.  This
  code is portable - it doesn't need to be modified for any platform,
  and it should be linked into the executables on all platforms.
Notes
  
Modified
  09/27/99 MJRoberts  - Creation
*/

#include "std.h"
#include "os.h"

/* ------------------------------------------------------------------------ */
/*
 *   Global variables 
 */

/*
 *   Page length and line width of the display area.  We default to a 24x80
 *   display area; the OS code should change these values during startup to
 *   reflect the actual display area size, if it can be determined.
 *   
 *   The portable code can use these values to determine the screen size for
 *   layout purposes (such as word-wrapping and "more" prompt insertions).  
 */
int G_os_pagelength = 24;
int G_os_linewidth = 80;


/*
 *   MORE mode flag
 */
int G_os_moremode = TRUE;

/*
 *   Name of the loaded game file, if applicable.  The application should set
 *   this during start-up (or wherever else is appropriate).  For the
 *   interpreter, this is the .gam or .t3 file being executed.  For
 *   executables other than interpreters, this should simply be left empty.  
 */
char G_os_gamename[OSFNMAX];


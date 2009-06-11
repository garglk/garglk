#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/osdoscon.c,v 1.2 1999/05/17 02:52:19 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdoscon.c - DOS console routines for 32-bit console apps
Function
  Implements the console interface routines for the 32-bit console
  version
Notes
  
Modified
  11/22/97 MJRoberts  - Creation
*/

#include <conio.h>

#include "os.h"

int os_break(void)
{
    return 0;
}

void os_instbrk(int install)
{
}


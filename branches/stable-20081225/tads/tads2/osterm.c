#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/osterm.c,v 1.2 1999/05/17 02:52:14 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osterm.c - simple os_term() implementation
Function
  This os_term() implementation is used for some of the stand-alone
  tools (such as TADSRSC), which aren't linked to a full set of OS
  object files.

  Most platforms should be able to use this implementation (which
  simply calls exit() in the standard C library) for linking with
  TADSRSC and the other simple command-line tools.
Notes
  
Modified
  01/23/99 MJRoberts  - Creation
*/

#include <stdlib.h>

void os_term(int exit_code)
{
    exit(exit_code);
}


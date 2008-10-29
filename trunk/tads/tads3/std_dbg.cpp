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
  std_dbg.cpp - T3 standard libarary - debugging routines
Function
  Provides debug versions of memory-management functions.  These functions
  are separated from the basic std.cpp routines so that they can be
  omitted entirely if they're provided by another subsystem (for example,
  the HTML TADS code has its own debug memory management routines; only
  one set is required or allowed in a given executable, so we can omit
  this version when linking with the HTML TADS code).
Notes
  
Modified
  10/08/99 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>

#include "os.h"
#include "t3std.h"
#include "utf8.h"

/* ------------------------------------------------------------------------ */
/*
 *   Debugging routines for memory management 
 */

#ifdef T3_DEBUG

void *operator new(size_t siz)
{
    return t3malloc(siz);
}

void operator delete(void *ptr)
{
    t3free(ptr);
}

void *operator new[](size_t siz)
{
    return t3malloc(siz);
}

void operator delete[](void *ptr)
{
    t3free(ptr);
}

#endif /* T3_DEBUG */

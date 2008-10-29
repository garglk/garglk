#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/LST.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  lst.c - list manipulation routines
Function
  Routines to manipulate TADS run-time lists
Notes
  None
Modified
  08/13/91 MJRoberts     - creation
*/

#include <assert.h>
#include "os.h"
#include "lst.h"
#include "dat.h"

void lstadv(uchar **lstp, uint *sizp)
{
    uint siz;
    
    siz = datsiz(**lstp, (*lstp)+1) + 1;
    assert(siz <= *sizp);
    *lstp += siz;
    *sizp -= siz;
}


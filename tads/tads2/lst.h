/*
$Header: d:/cvsroot/tads/TADS2/LST.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  lst.h - list definitions
Function
  Run-time list definitions
Notes
  A TADS run-time list is essentially a packed counted array.
  The first thing in a list is a ushort, which specifies the
  number of elements in the list.  The list elements are then
  packed into the list immediately following.
Modified
  08/13/91 MJRoberts     - creation
*/

#ifndef LST_INCLUDED
#define LST_INCLUDED

#ifndef DAT_INCLUDED
#include "dat.h"
#endif

/* advance a list pointer/size pair to the next element of a list */
void lstadv(uchar **lstp, uint *sizp);

#endif /* LST_INCLUDED */

/*
$Header: d:/cvsroot/tads/TADS2/DAT.H,v 1.2 1999/05/17 02:52:11 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  dat.h - datatype definitions
Function
  Defines datatypes for TADS
Notes
  None
Modified
  08/11/91 MJRoberts     - creation
*/

#ifndef DAT_INCLUDED
#define DAT_INCLUDED

#ifndef STD_INCLUDED
#include "std.h"
#endif

/* a type of a piece of data */
typedef int dattyp;

/* datatype numbers */
#define DAT_NUMBER  1
#define DAT_OBJECT  2
#define DAT_SSTRING 3
#define DAT_BASEPTR 4
#define DAT_NIL     5                     /* nil, as in FALSE or empty list */
#define DAT_CODE    6
#define DAT_LIST    7
#define DAT_TRUE    8                                     /* inverse of nil */
#define DAT_DSTRING 9
#define DAT_FNADDR  10                                /* a function address */
#define DAT_TPL     11                             /* template list pointer */
#define DAT_PROPNUM 13                                 /* a property number */
#define DAT_DEMAND  14          /* special flag: use callback to set on use */
#define DAT_SYN     15               /* synonym to indicated property value */
#define DAT_REDIR   16                   /* redirection to different object */
#define DAT_TPL2    17                                /* new-style template */

/* determine the size of a piece of data */
uint datsiz(dattyp typ, void *valptr);

#endif /* DAT_INCLUDED */

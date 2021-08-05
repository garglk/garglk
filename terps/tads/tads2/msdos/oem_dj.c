#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/oem_dj.c,v 1.2 1999/05/17 02:52:19 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oem_dj.c - OEM definitions for DOS DJGPP run-time
Function
  
Notes
  
Modified
  10/12/98 MJRoberts  - Creation
*/

#include "std.h"
#include "oem.h"

char G_tads_oem_app_name[] = "tadsr";
char G_tads_oem_dbg_name[] = "tdb";
char G_tads_oem_display_mode[] = "text-only";
char G_tads_oem_author[] =
  "MSDOS/DJGPP port maintained by Jim Dunleavy <jim.dunleavy@erha.ie>";
int G_tads_oem_copyright_prefix = FALSE;

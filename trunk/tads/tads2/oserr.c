#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OSERR.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oserr.c - find error message file
Function
  Looks in executable's directory for TADSERR.MSG, and opens it
Notes
  None
Modified
  04/04/99 CNebel        - Improve const-ness.
  04/24/93 JEras         - use new os_locate() to find tadserr.msg
  04/27/92 MJRoberts     - creation
*/

#include <string.h>
#include "os.h"

osfildef *oserrop(const char *arg0)
{
    char  buf[128];

    if ( !os_locate("tadserr.msg", 11, arg0, buf, sizeof(buf)) )
        return((osfildef *)0);
    return(osfoprb(buf, OSFTERRS));
}

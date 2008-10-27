#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OUT.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  out.c - output formatter
Function
  Formats output text:  word wrap, etc.
Notes
  None
Modified
  10/27/91 MJRoberts     - creation
*/

#include <string.h>
#include "os.h"
#include "tio.h"

/*
 *   write out a runtime length-prefixed string 
 */
void outfmt(tiocxdef *ctx, uchar *txt)
{
    uint len;

    VARUSED(ctx);

    /* read the length prefix */
    len = osrp2(txt) - 2;
    txt += 2;

    /* write out the string */
    tioputslen(ctx, (char *)txt, len);
}


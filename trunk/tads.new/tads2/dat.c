#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/DAT.C,v 1.2 1999/05/17 02:52:11 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  dat.c - datatype manipulation routines
Function
  Functions to operate on TADS run-time datatypes
Notes
  Datatypes are portable, hence the hard-coded values for data
  sizes.
Modified
  08/13/91 MJRoberts     - creation
*/

#include "os.h"
#include "std.h"
#include "dat.h"
#include "lst.h"
#include "prp.h"
#include "obj.h"
#include "voc.h"

/* return size of a data value */
uint datsiz(dattyp typ, void *val)
{
    switch(typ)
    {
    case DAT_NUMBER:
        return(4);                /* numbers are in 4-byte lsb-first format */

    case DAT_OBJECT:
        return(2);         /* object numbers are in 2-byte lsb-first format */

    case DAT_SSTRING:
    case DAT_DSTRING:
    case DAT_LIST:
        return(osrp2((char *)val));

    case DAT_NIL:
    case DAT_TRUE:
        return(0);

    case DAT_PROPNUM:
    case DAT_SYN:
    case DAT_FNADDR:
    case DAT_REDIR:
        return(2);
        
    case DAT_TPL:
        /* template is counted array of 10-byte entries, plus length byte */
        return(1 + ((*(uchar *)val) * VOCTPLSIZ));

    case DAT_TPL2:
        return(1 + ((*(uchar *)val) * VOCTPL2SIZ));

    default:
        return(0);
    }
}

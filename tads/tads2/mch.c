#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/MCH.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  mch.c - memory cache manager:  low-level heap manager
Function
  Low-level heap management functions
Notes
  This is a cover for malloc that uses proper error signalling
  conventions
Modified
  08/20/91 MJRoberts     - creation
*/

#include "os.h"
#include "std.h"
#include "mch.h"
#include "err.h"

/* global to keep track of all allocations */
IF_DEBUG(ulong mchtotmem;)

uchar *mchalo(errcxdef *ctx, size_t siz, char *comment)
{
    uchar *ret;

    VARUSED(comment);
    IF_DEBUG(mchtotmem += siz;)

    ret = (uchar *)osmalloc(siz);
    if (ret)
        return(ret);
    else
    {
        errsig(ctx, ERR_NOMEM);
        NOTREACHEDV(uchar *);
        return 0;
    }
}

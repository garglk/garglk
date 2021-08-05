/*
$Header: d:/cvsroot/tads/TADS2/MCH.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  mch.h - memory cache heap manager
Function
  Low-level heap management functions
Notes
  This is the low-level heap manager, which maintains a list of
  non-relocatable, non-swappable blocks of memory.  The cache
  manager uses the heap manager for its basic storage needs.
Modified
  08/03/91 MJRoberts     - creation
*/

#ifndef MCH_INCLUDED
#define MCH_INCLUDED

#include <stdlib.h>

#ifndef STD_INCLUDED
#include "std.h"
#endif
#ifndef ERR_INCLUDED
#include "err.h"
#endif

/*
 *   Allocate a block of memory; returns pointer to the block.
 *   An out-of-memory error is signalled if insufficient memory
 *   is available.  The comment is for debugging purposes only.
 */
uchar *mchalo(errcxdef *ctx, size_t siz, char *comment);

/* allocate a structure */
#define MCHNEW(errctx, typ, comment) \
 ((typ *)mchalo(errctx, sizeof(typ), comment))

/* free a block of memory */
/* void mchfre(uchar *ptr); */
#define mchfre(ptr) (osfree(ptr))

#endif /* MCH_INCLUDED */

#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmres.cpp,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmres.cpp - VM resource class
Function
  
Notes
  
Modified
  04/03/99 MJRoberts  - Creation
*/

#include "vmres.h"

/*
 *   Allocate the resource.
 */
CVmResource::CVmResource(long seek_pos, uint32_t len, size_t name_len)
{
    /* store the seek and length information */
    seek_pos_ = seek_pos;
    len_ = len;

    /* allocate space for the name */
    name_ = (char *)t3malloc(name_len + 1);
    name_[0] = '\0';

    /* nothing else in the list yet */
    nxt_ = 0;
}

/*
 *   Delete the resource 
 */
CVmResource::~CVmResource()
{
    /* delete the name */
    t3free(name_);
}


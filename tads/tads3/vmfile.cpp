#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMFILE.CPP,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfile.cpp - VM file implementation
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmfile.h"
#include "vmerr.h"
#include "vmdatasrc.h"


/*
 *   delete the file object 
 */
CVmFile::~CVmFile()
{
    /* if we still have an underlying OS file, close it */
    if (fp_ != 0)
        osfcls(fp_);
}

/*
 *   open for reading 
 */
void CVmFile::open_read(const char *fname, os_filetype_t typ)
{
    /* try opening the underlying OS file for binary reading */
    fp_ = osfoprb(fname, typ);

    /* if that failed, throw an error */
    if (fp_ == 0)
        err_throw(VMERR_FILE_NOT_FOUND);
}

/*
 *   open for writing 
 */
void CVmFile::open_write(const char *fname, os_filetype_t typ)
{
    /* try opening the underlying OS file for binary writing */
    fp_ = osfopwb(fname, typ);

    /* if that failed, throw an error */
    if (fp_ == 0)
        err_throw(VMERR_CREATE_FILE);
}

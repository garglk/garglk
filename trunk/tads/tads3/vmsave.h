/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmsave.h - save/restore VM state
Function
  Saves/restores VM state to/from a file.
Notes
  
Modified
  08/02/99 MJRoberts  - Creation
*/

#ifndef VMSAVE_H
#define VMSAVE_H

#include "vmglob.h"

class CVmSaveFile
{
public:
    /* save state to a file */
    static void save(VMG_ class CVmFile *fp);

    /* 
     *   given a saved state file, read the name of the image file that
     *   created it 
     */
    static int restore_get_image(osfildef *fp,
                                 char *fname_buf, size_t fname_buf_len);

    /* 
     *   Restore state from a file.  Returns zero on success, or a
     *   VMERR_xxx code indicating the problem on failure.  
     */
    static int restore(VMG_ class CVmFile *fp);

    /* reset the VM to the initial image file state */
    static void reset(VMG0_);
};

#endif /* VMSAVE_H */

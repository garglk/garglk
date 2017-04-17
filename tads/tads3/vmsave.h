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

/*
 *   Save/restore API.  (This class is all static methods, so it's never
 *   instatiated; it's really just a namespace.)
 */
class CVmSaveFile
{
public:
    /* 
     *   Save state to a file.  Writes the state information to the given
     *   open file stream.
     *   
     *   'metadata' is an optional LookupTable object containing
     *   string->string associations.  We'll write each key/value pair to the
     *   save file in the metadata header section.  This section allows the
     *   interpreter and external tools to display user-readable information
     *   on the saved game.  For example, you could store things like the
     *   current location, chapter number, score, etc - things that would
     *   help the user identify the context of the saved game when looking
     *   for the one he/she wishes to restore.  
     */
    static void save(VMG_ class CVmFile *fp,
                     class CVmObjLookupTable *metadata);

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

protected:
};

#endif /* VMSAVE_H */

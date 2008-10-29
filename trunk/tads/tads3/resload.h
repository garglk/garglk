/* $Header: d:/cvsroot/tads/tads3/resload.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  resload.h - resource loader
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#ifndef RESLOAD_H
#define RESLOAD_H

#include "t3std.h"

/* ------------------------------------------------------------------------ */
/*
 *   Resource loader.  
 */

class CResLoader
{
public:
    /* create the loader */
    CResLoader();

    /* 
     *   create the loader with a root directory - we'll look for external
     *   files under this directory 
     */
    CResLoader(const char *root_dir);

    /* set the executable file name */
    void set_exe_filename(const char *exe_filename)
    {
        /* discard the old executable filename, if there is one */
        lib_free_str(exe_filename_);

        /* remember the new path */
        exe_filename_ = lib_copy_str(exe_filename);
    }

    /* delete */
    ~CResLoader();

    /* 
     *   Load a resource given a resource path.
     *   
     *   We'll start by looking for a file in the local file system matching
     *   the name 'respath'.  We interpret 'respath' as a relative URL; we'll
     *   convert it to local system conventions before looking for the local
     *   file.  The path is relative to the root resource directory, which is
     *   the directory specified when the resource loader was created.
     *   
     *   If we fail to find a local file matching the given name, we'll look
     *   for a T3 resource library file with name 'deflib', if that argument
     *   is non-null.  'deflib' is given as a URL to a file relative to the
     *   root resource directory, and should be specified WITHOUT the default
     *   ".t3r" suffix - we'll add that automatically using the appropriate
     *   local filename conventions.  If we can find this resource library,
     *   we'll look for 'respath' within the library.  Note that 'deflib' is
     *   given in URL notation - we'll convert this to local path conventions
     *   before attempting to locate the file.
     *   
     *   If we still can't find the file, and 'exerestype' is non-null, we'll
     *   look in the executable file for a resource of the given type (this
     *   should be a four-byte identifier, as used with MAKETRX).  This
     *   allows resources of different types to be bundled into the
     *   executable file.  Note that resources bundled into the executable
     *   must use the standard T3 resource-image file format.  
     */
    osfildef *open_res_file(const char *respath,
                            const char *deflib,
                            const char *exerestype);

protected:
    /*
     *   Load a resource from the executable file.  If we find the resource,
     *   returns a file handle with its seek position set to the start of
     *   the resource data.  If we can't find the resource in the
     *   executable, returns null.  
     */
    osfildef *open_exe_res(const char *respath, const char *restype);

    /*
     *   Load a resource from a resource library.  If we find the resource,
     *   returns a file handle with its seek position set to the start of the
     *   resource data.  If we can't find the library or we can't find the
     *   resource in the library, returns null. 
     */
    osfildef *open_lib_res(const char *libfile, const char *respath);
    
    /* root directory for external file searches */
    char *root_dir_;

    /* path to executable file */
    char *exe_filename_;
};

#endif /* RESLOAD_H */


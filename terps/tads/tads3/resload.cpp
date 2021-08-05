#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/resload.cpp,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  resload.cpp - resource loader
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#include <stddef.h>
#include <string.h>

#include "t3std.h"
#include "resload.h"

/* ------------------------------------------------------------------------ */
/*
 *   create resource loader 
 */
CResLoader::CResLoader()
{
    /* we have no root directory */
    root_dir_ = 0;

    /* no executable path yet */
    exe_filename_ = 0;
}

/*
 *   create a resource loader given a root directory 
 */
CResLoader::CResLoader(const char *root_dir)
{
    /* remember the root directory for file searches */
    root_dir_ = lib_copy_str(root_dir);

    /* no executable path yet */
    exe_filename_ = 0;
}

/*
 *   delete resource loader 
 */
CResLoader::~CResLoader()
{
    /* free our root directory string and executable path string */
    lib_free_str(root_dir_);
    lib_free_str(exe_filename_);
}

/*
 *   Open a resource file given the resource path.
 */
osfildef *CResLoader::open_res_file(const char *respath,
                                    const char *deflib,
                                    const char *exerestype)
{
    char filepath[OSFNMAX];
    osfildef *fp;
    
    /* 
     *   Look for the resource as an external file.  If we have a root
     *   directory, look for the file under that directory; otherwise,
     *   look for it in the current directory.  
     */
    if (root_dir_ != 0)
    {
        /* get the resource name as a file path */
        char fname[OSFNMAX];
        os_cvt_url_dir(fname, sizeof(fname), respath);

        /* build a full path from the root directory and the resource path */
        os_build_full_path(filepath, sizeof(filepath), root_dir_, fname);
    }
    else
    {
        /* get the resource name as a file path */
        os_cvt_url_dir(filepath, sizeof(filepath), respath);
    }

    /* try opening the file */
    fp = osfoprb(filepath, OSFTBIN);

    /* if we didn't find it, try looking in the default library */
    if (fp == 0 && deflib != 0)
    {
        char fname[OSFNMAX];

        /* convert from URL notation to local path conventions */
        os_cvt_url_dir(fname, sizeof(fname), deflib);

        /* build the full path, starting in the root resource directory */
        os_build_full_path(filepath, sizeof(filepath), root_dir_, fname);

        /* add the default resource library extension */
        os_defext(filepath, "t3r");

        /* try opening the file */
        fp = open_lib_res(filepath, respath);
    }

    /* if we still didn't find it, try looking in the executable */
    if (fp == 0 && exerestype != 0)
        fp = open_exe_res(respath, exerestype);

    /* return the result */
    return fp;
}

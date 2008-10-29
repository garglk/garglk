/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  resnoexe.cpp - null implementation of executable file resource loader
Function
  Provides a null implementation of CResLoader::open_exe_res().  Programs
  that do not want to be able to load resources from the executable file
  can link this module (rather than resldexe.cpp); this avoids dragging
  in additional osifc routines that are superfluous when executable
  resource loading is not required.
Notes
  
Modified
  11/03/01 MJRoberts  - Creation
*/

#include "resload.h"

/*
 *   Try loading a resource from the executable file 
 */
osfildef *CResLoader::open_exe_res(const char *respath,
                                   const char *restype)
{
    /* this version is a null implementation - return failure */
    return 0;
}

/*
 *   load a resource from a library 
 */
osfildef *CResLoader::open_lib_res(const char *libfile,
                                   const char *respath)
{
    /* this version is a null implementation - return failure */
    return 0;
}

#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3nl.cpp - T3 NO LINKER module
Function
  This module can be linked with programs that don't require any
  linker functionality.  This module contains dummy entrypoints
  for functions and methods that would normally be linked from
  tct3img.cpp, which contains the actual implementations of these
  methods.

Notes
  
Modified
  11/22/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "os.h"
#include "tct3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Symbol table entry routines for writing a symbol to the global symbol
 *   table in the debug records in the image file.  When we don't include
 *   linking functionality in a program, these entrypoints will never be
 *   called (but they're needed to link anything that references a
 *   CTcSymbol subclass, since they're virtual functions in those classes) 
 */

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymFunc::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymObj::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

/*
 *   build the dictionary 
 */
void CTcSymObj::build_dictionary()
{
    assert(FALSE);
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymProp::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymEnum::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymMetaclass::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymBif::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymExtfn::write_to_image_file_global(class CVmImageWriter *)
{
    assert(FALSE);
    return FALSE;
}

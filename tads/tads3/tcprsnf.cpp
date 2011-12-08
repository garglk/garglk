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
  tcprsnf.cpp - TADS 3 Compiler Parser - "no files" module
Function
  This module can be linked with programs that don't require any symbol or
  object file read/write functionality.  This module contains dummy
  entrypoints for functions and methods that would normally be linked from
  tcprsfil.cpp, which contains the actual implementations of these methods.
  This is needed for interpreter builds that link in the compiler for
  "eval()" functionality, which doesn't require any intermediate file
  creation or loading.
Notes

Modified
  04/30/99 MJRoberts  - Creation
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "os.h"
#include "t3std.h"
#include "tcprs.h"
#include "tctarg.h"
#include "tcgen.h"
#include "vmhash.h"
#include "tcmain.h"
#include "vmfile.h"
#include "tcmake.h"

/* ------------------------------------------------------------------------ */
/*
 *   Object file readers 
 */
void CTcSymObjBase::load_refs_from_obj_file(
    CVmFile *, const char *, tctarg_obj_id_t *, tctarg_prop_id_t *)
{
    assert(FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Object file writers 
 */
int CTcSymbolBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymObjBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymObjBase::write_refs_to_obj_file(class CVmFile *)
{
    assert(FALSE);
    return FALSE;
}
int CTcSymPropBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymFuncBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymEnumBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymMetaclassBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymBifBase::write_to_obj_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Symbol file writers 
 */
int CTcSymbolBase::write_to_sym_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymObjBase::write_to_sym_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymFuncBase::write_to_sym_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymEnumBase::write_to_sym_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymPropBase::write_to_sym_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}
int CTcSymMetaclassBase::write_to_sym_file(CVmFile *)
{
    assert(FALSE);
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   build the dictionary 
 */
void CTcSymObjBase::build_dictionary()
{
    assert(FALSE);
}



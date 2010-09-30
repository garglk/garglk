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
  tcprsnl.cpp - TADS 3 Compiler Parser - NO LINKER module
Function
  This module can be linked with programs that don't require any
  linker functionality.  This module contains dummy entrypoints
  for functions and methods that would normally be linked from
  tcprsimg.cpp, which contains the actual implementations of these
  methods.
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

void CTcSymObjBase::apply_internal_fixups()
{
    assert(FALSE);
}

void CTcSymObj::delete_prop_from_mod_base(tctarg_prop_id_t)
{
    assert(FALSE);
}

void CTcSymObj::mark_compiled_as_class()
{
    assert(FALSE);
}

int CTcSymbolBase::load_from_obj_file(CVmFile *, const textchar_t *,
                                      tctarg_obj_id_t *,
                                      tctarg_prop_id_t *,
                                      ulong *)
{
    assert(FALSE);
    return 0;
}

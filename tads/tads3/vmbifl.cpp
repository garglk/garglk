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
  vmbifl.cpp - built-in function - Load-time resolution
Function
  This is a version of the built-in function interface for resolving
  built-ins on loading the image file.  This version makes no checks
  on the availability of a function when it's invoked.

  This version can be used in a normal stand-alone interpreter.  A
  version of the interpreter that's used to complete compilation by
  running 'preinit' should use the call-time resolution version instead.
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmbif.h"
#include "vmbifreg.h"
#include "vmstr.h"
#include "vmobj.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   Call the given function from the given function set.  
 */
void CVmBifTable::call_func(VMG_ uint set_index, uint func_index, uint argc)
{
    /* 
     *   find the function set in the registration table, get the function
     *   descriptor from the set's function table, and invoke the function
     *   pointer from the function descriptor 
     */
    (*table_[set_index]->func[func_index].func)(vmg_ argc);
}

/*
 *   Get a function's descriptor 
 */
const vm_bif_desc *CVmBifTable::get_desc(uint set_index, uint func_index)
{
    /* 
     *   find the function in the registration table, and return the function
     *   descriptor from the set's function table 
     */
    return &table_[set_index]->func[func_index];
}


/*
 *   Handle adding a function set entry that's unresolvable at load-time 
 */
void CVmBifTable::add_entry_unresolved(VMG_ const char *func_set_id)
{
    /* this is the static-link version, so an unresolved entry is an error */
    err_throw_a(VMERR_UNKNOWN_FUNC_SET, 3,
                ERR_TYPE_TEXTCHAR, func_set_id,
                ERR_TYPE_FUNCSET, func_set_id,
                ERR_TYPE_VERSION_FLAG);
}

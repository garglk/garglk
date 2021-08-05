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
  vmbifc.cpp - built-in function - Call-time resolution
Function
  This is a version of the built-in function interface for resolving
  built-ins only when each built-in is invoked.  This version allows
  loading an image file with unresolved function sets, then checks
  on each built-in function's invocation to make sure the function is
  available.

  This version can be used in a version of the interpreter used by
  the compiler for running 'preinit' or similar situations in which it
  is desirable to be able to load and run a program with unresolved
  function sets.  This version is less efficient than the load-time
  resolver, so normal stand-alone interpreters should use the load-time
  version instead.
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
    vm_bif_entry_t *entry;
    vm_bif_desc *desc;
    
    /* get the function set */
    entry = table_[set_index];

    /* if the function set is null, we can't call the function */
    if (entry == 0)
        err_throw_a(VMERR_UNKNOWN_FUNC_SET, 1,
                    ERR_TYPE_TEXTCHAR, names_[set_index]);

    /* get the function pointer */
    desc = &entry->func[func_index];

    /* if the function is null, we can't call it */
    if (desc->func == 0)
        err_throw_a(VMERR_UNAVAIL_INTRINSIC, 2,
                    ERR_TYPE_TEXTCHAR, names_[set_index],
                    ERR_TYPE_INT, func_index);

    /* call the function */
    (*desc->func)(vmg_ argc);
}

/*
 *   Handle adding a function set entry that's unresolvable at load-time 
 */
void CVmBifTable::add_entry_unresolved(VMG_ const char *func_set_id)
{
    /*
     *   Since this is the call-time resolver, allow loading of the image
     *   file even though this function set is unresolved.  Store a null
     *   entry in the function set table, and store the name of the
     *   function set - we'll need this in case the program attempts to
     *   invoke a function in this function set, so that we can generate
     *   an error containing the unresolved function set name. 
     */
    table_[count_] = 0;
    names_[count_] = lib_copy_str(func_set_id);

    /* count the new entry */
    ++count_;
}

/*
 *   Get a function's descriptor 
 */
const vm_bif_desc *CVmBifTable::get_desc(uint set_index, uint func_index)
{
    vm_bif_entry_t *entry;
    vm_bif_desc *desc;

    /* validate that the set index is in range */
    if (set_index >= count_)
        return 0;

    /* get the function set, and make sure it's valid */
    entry = table_[set_index];
    if (entry == 0)
        return 0;

    /* validate that the function index is in range */
    if (func_index > entry->func_count)
        return 0;

    /* get the descriptor, and make sure there's a function pointer */
    desc = &entry->func[func_index];
    if (desc->func == 0)
        return 0;

    /* return the descriptor */
    return desc;
}


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
  vminitim.cpp - VM initialization - in-memory pool implementation
Function
  
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vminit.h"
#include "vmpool.h"

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the VM with in-memory page pools 
 */
void vm_init_in_mem(vm_globals **vmg, const vm_init_options *opts)
{
    /* initialize the base VM structures */
    vm_init_base(vmg, opts);

    /* 
     *   assign the global pointer to the special vmg__ local for
     *   globals-on-stack configuration 
     */
    vm_globals *vmg__ = *vmg;

    /* 
     *   explicitly mark vmg__ as reference, in case we don't happen to use
     *   it in this build configuration 
     */
    (void)vmg__;

    /* create the in-memory pools */
    VM_IF_ALLOC_PRE_GLOBAL(G_code_pool = new CVmPoolInMem());
    VM_IF_ALLOC_PRE_GLOBAL(G_const_pool = new CVmPoolInMem());
}

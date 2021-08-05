/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vminitim.cpp - VM initialization - "flat" pool implementation
Function

Notes

Modified
  07/21/99 MJRoberts  - Creation
*/

#include "vminit.h"
#include "vmpool.h"
#include "vmglob.h"

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the VM with the "flat" pool manager
 */
void vm_init_flat(vm_globals **vmg, const vm_init_options *opts)
{
    vm_globals *vmg__;

    /* initialize the base VM structures */
    vm_init_base(vmg, opts);

    /* 
     *   assign the global pointer to the special vmg__ local for
     *   globals-on-stack configuration 
     */
    vmg__ = *vmg;

    /* create the flat pools */
    VM_IF_ALLOC_PRE_GLOBAL(G_code_pool = new CVmPoolFlat());
    VM_IF_ALLOC_PRE_GLOBAL(G_const_pool = new CVmPoolFlat());
}


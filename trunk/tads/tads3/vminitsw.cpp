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
  vminitsw.cpp - VM initialization - swapping pool implementation
Function
  
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#include "vminit.h"
#include "vmpool.h"


/* ------------------------------------------------------------------------ */
/*
 *   Initialize the VM with swapping page pools 
 */
void vm_init_swap(vm_globals **vmg, size_t max_pages_in_mem,
                  const vm_init_options *opts)
{
    vm_globals *vmg__;

    /* initialize the base VM structures */
    vm_init_base(vmg, opts);

    /* 
     *   assign the global pointer to the special vmg__ local for
     *   globals-on-stack configuration 
     */
    vmg__ = *vmg;

    /* create the in-memory pools */
    G_code_pool = new CVmPoolSwap(max_pages_in_mem);
    G_const_pool = new CVmPoolSwap(max_pages_in_mem);
}


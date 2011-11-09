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
  vmcfgsw.cpp - T3 VM Configuration - Swapping memory manager
Function
  
Notes
  
Modified
  10/08/99 MJRoberts  - Creation
*/

#error THIS FILE IS DEPRECATED - please replace vmcfgsw.cpp with \
vmcfgmem.cpp in your makefile or equivalent

#include "vminit.h"

/*
 *   initialize 
 */
void vm_initialize(struct vm_globals **vmg, const vm_init_options *opts)
{
    vm_init_swap(vmg, 10, opts);
}


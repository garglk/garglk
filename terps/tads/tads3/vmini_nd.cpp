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
  vmini_nd.cpp - VM initialization - non-debug version
Function
  Stub routines for initialization related to the debugger.  This
  module should be linked into non-debug builds of the interpreter,
  so that debugger-related files are omitted from the executable.
Notes
  
Modified
  12/03/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vminit.h"
#include "vmparam.h"

/*
 *   Initialize the debugger 
 */
void vm_init_debugger(VMG0_)
{
    /* non-debug version - nothing to do */
}

/*
 *   Initialization, phase 2: just before loading image file 
 */
void vm_init_before_load(VMG_ const char *image_filename)
{
    /* non-debug version - nothing to do */
}

/*
 *   Initialization, phase 3: just after loading the image file 
 */
void vm_init_after_load(VMG0_)
{
    /* non-debug version - nothing to do */
}


/*
 *   Termination - shut down the debugger 
 */
void vm_terminate_debug_shutdown(VMG0_)
{
    /* non-debug version - nothing to do */
}

/*
 *   Termination - delete debugger objects 
 */
void vm_terminate_debug_delete(VMG0_)
{
    /* non-debug version - nothing to do */
}

/*
 *   In the non-debug version, use the basic stack reserve parameter.  
 */
size_t vm_init_stack_reserve()
{
    return VM_STACK_RESERVE;
}

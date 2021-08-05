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
  vmini_d.cpp - T3 VM - debugger-related initialization 
Function
  Isolates initialization required only for debugger-enabled versions.
  This module can be replaced with a stub module when linking a
  stand-alone, non-debug version of the VM interpreter, to avoid
  dragging in lots of debugger-related modules.
Notes
  
Modified
  12/03/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vminit.h"
#include "vmparam.h"
#include "vmdbg.h"

/*
 *   Initialize the debugger 
 */
void vm_init_debugger(VMG0_)
{
    /* create the debugger interface object */
    G_debugger = new CVmDebug(vmg0_);
}

/*
 *   Initialization, phase 2: just before loading image file 
 */
void vm_init_before_load(VMG_ const char *image_filename)
{
    /* initialize the debugger */
    G_debugger->init(vmg_ image_filename);
}

/*
 *   Initialization, phase 3: just after loading the image file 
 */
void vm_init_after_load(VMG0_)
{
    /* finish initializing the debugger */
    G_debugger->init_after_load(vmg0_);
}


/*
 *   Termination - shut down the debugger
 */
void vm_terminate_debug_shutdown(VMG0_)
{
    /* if the debugger is present, tell it to shut down */
    G_debugger->terminate(vmg0_);
}

/*
 *   Termination - delete debugger objects 
 */
void vm_terminate_debug_delete(VMG0_)
{
    /* delete the debugger interface */
    delete G_debugger;
}

/*
 *   Use a stack-overflow reserve in the debug version 
 */
size_t vm_init_stack_reserve()
{
    return VM_STACK_DBG_RESERVE;
}

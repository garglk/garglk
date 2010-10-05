/* $Header: d:/cvsroot/tads/tads3/vmbiftad.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbift3.h - function set definition - T3 VM system interface
Function
  
Notes
  
Modified
  12/06/98 MJRoberts  - Creation
*/

#ifndef VMBIFTAD_H
#define VMBIFTAD_H

#include "vmbif.h"

#endif /* VMBIFTAD_H */

/* ------------------------------------------------------------------------ */
/*
 *   T3 VM function set intrinsics
 */
class CVmBifT3: public CVmBif
{
public:
    /* run the garbage collector */
    static void run_gc(VMG_ uint argc);

    /* set the SAY instruction's handler function */
    static void set_say(VMG_ uint argc);

    /* get the VM version number */
    static void get_vm_vsn(VMG_ uint argc);

    /* get the VM identification string */
    static void get_vm_id(VMG_ uint argc);

    /* get the VM banner string */
    static void get_vm_banner(VMG_ uint argc);

    /* get the 'preinit' status - true if preinit, nil if normal */
    static void get_vm_preinit_mode(VMG_ uint argc);

    /* debug trace functions */
    static void debug_trace(VMG_ uint argc);

    /* get the global symbol table, if available */
    static void get_global_symtab(VMG_ uint argc);

    /* allocate a new property ID */
    static void alloc_new_prop(VMG_ uint argc);

    /* get a stack trace */
    static void get_stack_trace(VMG_ uint argc);

protected:
    /* 
     *   service routine - retrieve source information for a given code
     *   location 
     */
    static void get_source_info(VMG_ ulong entry_addr, ulong method_ofs,
                                vm_val_t *retval);
};

/*
 *   T3 VM Test function set - test and debug function set
 */
class CVmBifT3Test: public CVmBif
{
public:
    /* get an instance's unique identifier number */
    static void get_obj_id(VMG_ uint argc);

    /* get an instance's garbage collection state */
    static void get_obj_gc_state(VMG_ uint argc);

    /* get the Unicode character code for the first character of a string */
    static void get_charcode(VMG_ uint argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   debug_trace mode flags 
 */

/* check to see if the debugger is present */
#define T3DBG_CHECK     1

/* break into the debugger */
#define T3DBG_BREAK     2



/* ------------------------------------------------------------------------ */
/*
 *   Function set vector.  Define this only if VMBIF_DEFINE_VECTOR has
 *   been defined, so that this file can be included for the prototypes
 *   alone without defining the function vector.  
 */
#ifdef VMBIF_DEFINE_VECTOR

void (*G_bif_t3[])(VMG_ uint) =
{
    &CVmBifT3::run_gc,
    &CVmBifT3::set_say,
    &CVmBifT3::get_vm_vsn,
    &CVmBifT3::get_vm_id,
    &CVmBifT3::get_vm_banner,
    &CVmBifT3::get_vm_preinit_mode,
    &CVmBifT3::debug_trace,
    &CVmBifT3::get_global_symtab,
    &CVmBifT3::alloc_new_prop,
    &CVmBifT3::get_stack_trace
};

void (*G_bif_t3_test[])(VMG_ uint) =
{
    &CVmBifT3Test::get_obj_id,
    &CVmBifT3Test::get_obj_gc_state,
    &CVmBifT3Test::get_charcode
};

#endif /* VMBIF_DEFINE_VECTOR */

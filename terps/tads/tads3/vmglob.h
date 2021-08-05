/* $Header: d:/cvsroot/tads/tads3/vmglob.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmglob.h - T3 VM globals
Function
  The T3 VM requires a number of static variables that are shared
  by several subsystems.  This file defines the globals and a scheme
  for accessing them.

  Important: note that some VM globals aren't part of the scheme
  described and defined below.  In particular, the try-catch-throw
  exception handling subsystem doesn't use it; the dynamic compiler
  doesn't use it; read-only statics don't use it; and some global
  caches don't use it.  The exception subsystem opts out because
  its globals constitute a critical performance bottleneck, plus
  they need to be per-thread for multi-threaded builds; so they use
  native thread-local storage if threads are used, otherwise they're
  just ordinary globals.  The dynamic compiler doesn't use the VM
  globals scheme because the compiler wasn't originally designed to
  be used within the VM at all; by the time we adapted it, it was too
  late and would have required too much work to bring it into the
  scheme.  Read-only statics can be excluded because they can be
  safely shared among threads and/or instances without any extra
  work.  And caches can safely opt out as long as the information
  cached is truly global, not private to a VM instance; but note that
  caches might need to use per-thread storage, or else mutexes, if
  they can be accessed from multiple threads.

  The globals can be configured in four different ways, depending on
  how the T3 VM is to be used:

    1. As individual external variables.  This is the most efficient
       way to configure the globals, but this can't be used when
       multiple independent VM instances are needed in a single process,
       because each VM would need its own copy of the globals.  This
       is the fastest way to lay out the globals, because it allows
       the compiler to generate code that accesses internal members of
       performance-critical global objects directly, without any pointer
       dereferences.

    2. As members of a static global structure.  This is the second
       most efficient way to configure the globals, but this can't be
       used when multiple independent VM instances are needed,
       because each VM would need its own copy of the globals, which
       isn't possible in this configuration.  (Note that accessing
       members of a global static structure is identical to accessing
       global variables, since the compiler can compute the relative
       address of a structure member at compile time and hence does
       not need to perform any run-time pointer arithmetic or any other
       computation beyond what it would normally use for global variable
       access.  However, in this configuration, it's not possible to
       allocate any of the global objects in-line within the master
       global structure, so each global object's internal members must
       be accessed through one pointer dereference.)

    3. As members of an allocated structure whose address is contained
       in a global static variable.  This is slightly less efficient
       than method 1, in that the global pointer must be dereferenced
       on each global variable access.  This method allows multiple VM
       instances to be used, as long as the host application specifically
       stores the correct structure pointer in the global static variable
       before each call to the VM; this can be used when the host system
       uses the VM in only one thread.

    4. As members of an allocated structure whose address is passed as
       a parameter to each function that needs global variable access.
       As with method 2, the host application creates a structure per
       VM instance it requires, and then passes a pointer to the correct
       structure on each call to a VM function; the VM internally passes
       this pointer in its own function calls where needed.  This is the
       most flexible method, in that the VM can be used in multiple
       thread simultaneously (as long as each thread has a separate VM
       global structure instance), but is the least efficient, because
       the structure pointer must be passed around on all calls.

  To select a configuration, define one of the following preprocessor
  symbols in the makefile:

     VMGLOB_VARS    - method 1 - individual global variables
     VMGLOB_STRUCT  - method 2 - global static structure
     VMGLOB_POINTER - method 3 - global static pointer to allocated structure
     VMGLOB_PARAM   - method 4 - pointer to structure passed as parameter

  We provide all of these different possible configurations because of
  the trade-offs involved in selecting one.  The host application's needs
  should be determined, and the most efficient configuration that meets
  those needs should be chosen.
Notes
  
Modified
  11/28/98 MJRoberts  - Creation
*/

#ifndef VMGLOB_H
#define VMGLOB_H

#include <stddef.h>

#include "t3std.h"
#include "vmpoolsl.h"


/* ------------------------------------------------------------------------ */
/*
 *   HOST SYSTEM GLOBAL INITIALIZATION
 *   
 *   The host system should declare a local variable as follows:
 *   
 *   vm_globals *vmg__; // two underscores
 *   
 *   The program must call vmglob_alloc() each time it wants to initialize a
 *   global variable context.  This routine must always be called at least
 *   once.  (In the VMGLOB_STRUCT or VMGLOB_VARS configuration, this routine
 *   has no effect, but should still be called so that the code will work
 *   unchanged in other configurations.)  Calling this routine will overwrite
 *   the current static global variable pointer in the VMGLOB_POINTER
 *   configuration, so the caller must take care to keep track of each set of
 *   globals it allocates for later restoration.
 *   
 *   When calling vmglob_alloc(), assign the return value to vmg__.  
 */

/*
 *   HOST SYSTEM GLOBAL TERMINATION
 *   
 *   The caller should invoke vmglob_delete() for each set of globals it
 *   allocated.  
 */

/*   
 *   HOST SYSTEM VM FUNCTION CALLS
 *   
 *   In each call to a VM function, the host system should include the
 *   macro "vmg_" (one underscore) at the start of each parameter list; do
 *   not put a comma after vmg_.  Call functions that take no other
 *   parameters with vmg0_ instead of vmg_.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   INTERNAL FUNCTION DECLARATION AND CALLING
 *   
 *   Every function that requires global access, or which calls a function
 *   that requires global access, must put "VMG_" at the very start of its
 *   formal parameters declaration.  Do not put a comma after VMG_, but
 *   simply list the first parameter.  If the function has no parameters
 *   at all, use VMG0_ instead of VMG_.  For example:
 *   
 *   void CVmClass::func1(VMG_ const textchar_t *str, size_t len) { ... }
 *.  void CVmClass::func2(VMG0_) { ... }
 *   
 *   In each call to a function that uses global variables or calls
 *   functions that use global variables, put "vmg_" at the very start of
 *   the actual parmaeters list in the call; do not put a comma after the
 *   vmg_.  If the function takes no parameters at all, use vmg0_ instead
 *   of vmg_.
 *   
 *   func1(vmg_ "test", 4);
 *.  func2(vmg0_); 
 */


/* ------------------------------------------------------------------------ */
/*
 *   Functions that themselves can't participate in the prototype
 *   mechanism, because they present an external API to other subsystems,
 *   must keep track of the globals on their own.  To do this, they must
 *   stash the global pointer in their own context structure - use
 *   VMGLOB_ADDR to get the address of the global structure.  When passing
 *   this stashed pointer back to function calls, use vmg_var(x) (for the
 *   first argument to a function with multiple arguments) or vmg_var0(x)
 *   (for a single-argument call), as appropriate, where x is the local
 *   copy.
 *   
 *   To access globals from this type of routine, put the VMGLOB_PTR(x)
 *   macro in with the local variable decalarations for the function,
 *   passing as the argument the stashed address of the global structure.
 *   This will set things up so that the G_xxx variables are accessible
 *   through a local pointer, when necessary, and will also make the vmg_
 *   and vmg0_ argument macros work properly.  See the examples below.
 */
#if 0
/* EXAMPLES ONLY - THIS CODE IS FOR DOCUMENTATION ONLY */

struct my_context_def
{
    /* holder for VM globals structure pointer */
    vm_globals *vmg;
};

int func_calling_external_interfaces(VMG_ int x, int y, int z)
{
    my_context_def ctx;
    
    /* stash the address of the VM globals in my private context */
    ctx.vmg = VMGLOB_ADDR;

    /* call my external API function */
    external_api_func(&ctx);
}

int func_called_from_external_interface(my_context_def *ctx, int x, int y)
{
    /* set up access to the VM globals through my stashed context */
    VMGLOB_PTR(ctx->vmg);

    /* access a global variable */
    G_myvar->do_something();

    /* call a function using a VMG_ prototype */
    G_myvar->do_something_else(vmg_ x, y);
}

#endif /* 0 */


/* ------------------------------------------------------------------------ */
/*
 *   Conditional access to globals.
 *   
 *   Some code that accesses global variables might need to be able to run
 *   during startup or shutdown.  On platforms where the globals are
 *   dynamically allocated, such code might need to test to see not only
 *   whether or not particular global variable has been allocated, but also
 *   whether or not the memory containing the global variables themselves
 *   exists.  E.g., if you want to access the console object in code that
 *   might be called during early startup or late termination, it's not good
 *   enough to test if G_console constains a non-null object pointer, since
 *   merely accessing G_console itself will dereference the global variable
 *   structure pointer, and this pointer might be null on platforms where the
 *   globals are allocated.
 *   
 *   For such situations, use VMGLOB_IF_AVAIL(varname) to cover the global
 *   variable name.  On platforms where the globals are allocated, this will
 *   return null if the globals themselves aren't yet allocated or have
 *   already been freed, otherwise it'll return the variable's value.
 */
#if 0

/* EXAMPLE ONLY - THIS CODE IS FOR DOCUMENTATION PURPOSES */
CVmConsole *con = VMGLOB_IF_AVAIL(G_console);

#endif


/* ------------------------------------------------------------------------ */
/*
 *   Set up to define the global variables.  For the POINTER and PARAM
 *   configurations, put pointers to the global structures in a structure.
 *   For the static STRUCT configuration, actually allocate all of the
 *   objects statically.
 *   
 *   Use VM_GLOBAL_OBJDEF() to define the entry for an object (struct or
 *   class) type.  The global variable will be a pointer to this type.
 *   
 *   Use VM_GLOBAL_PREOBJDEF to define the entry for an object type that's to
 *   be defined as a pre-allocated static object in the VARS configuration.
 *   This can be used for objects that (1) have very frequent access and thus
 *   should have their fields reachable directly as statics rather than via
 *   static pointers, and (2) need no constructor parameters and thus can be
 *   pre-allocated at compile-time.  In the VARS configuration, these
 *   variables can be allocated at compile-time, which allows the compiler to
 *   generate code that accesses the objects' internal members without any
 *   pointer dereferences.
 *   
 *   Any variable defined with VM_GLOBAL_PREOBJDEF MUST have its accessor
 *   defined through VMGLOB_PREACCESS() rather than VMGLOB_ACCESS().
 *   
 *   Use VM_GLOBAL_VARDEF to define a scalar variable.  
 */
#if defined(VMGLOB_POINTER) || defined(VMGLOB_PARAM) || defined(VMGLOB_STRUCT)
#define VM_GLOBALS_BEGIN  struct vm_globals {
#define VM_GLOBAL_OBJDEF(typ, var) typ *var;
#define VM_GLOBAL_PREOBJDEF(typ, var) typ *var;
#define VM_GLOBAL_PRECOBJDEF(typ, var, ctor_args) typ *var;
#define VM_GLOBAL_VARDEF(typ, var) typ var;
#define VM_GLOBAL_ARRAYDEF(typ, var, eles) typ var[eles];
#define VM_GLOBALS_END    };

/* 
 *   we do allocate all global objects, including external objects; hence
 *   external globals are non-static 
 */
#define VM_IF_ALLOC_PRE_GLOBAL(x)          x
#define VM_IFELSE_ALLOC_PRE_GLOBAL(x, y)   x
#define VM_PRE_GLOBALS_ARE_STATIC   FALSE
#endif

#if defined(VMGLOB_VARS)
#define VM_GLOBALS_BEGIN

#define VM_GLOBAL_OBJDEF(typ, var) extern typ *G_##var##_X;
#define VM_GLOBAL_PREOBJDEF(typ, var) extern typ G_##var##_X;
#define VM_GLOBAL_PRECOBJDEF(typ, var, ctor_args) extern typ G_##var##_X;
#define VM_GLOBAL_VARDEF(typ, var) extern typ G_##var##_X;
#define VM_GLOBAL_ARRAYDEF(typ, var, eles) extern typ G_##var##_X[eles];

#define VM_GLOBALS_END

/* we don't actually need a structure for the globals; use a dummy */
struct vm_globals { int x; };

/* external global objects are statically allocated */
#define VM_IF_ALLOC_PRE_GLOBAL(x)
#define VM_IFELSE_ALLOC_PRE_GLOBAL(x, y)   y
#define VM_PRE_GLOBALS_ARE_STATIC  TRUE
#endif


/* define the globals */
#include "vmglobv.h"

/* ------------------------------------------------------------------------ */
/*
 *   If we're not including from the global-defining source file, merely
 *   declare the globals. 
 */
#ifndef VMGLOB_DECLARE
#define VMGLOB_DECLARE extern
#endif


/* ------------------------------------------------------------------------ */
/*
 *   INDIVIDUAL STATIC GLOBAL VARIABLES configuration.  In this
 *   configuration, the globals are defined as individual global variables.
 *   This is the most efficient configuration, but it only allows one VM
 *   instance in a given process.  
 */
#ifdef VMGLOB_VARS

/* initialization - this has no effect in this mode */
inline vm_globals *vmglob_alloc() { return 0; }

/* termination - this has no effect in this mode */
inline void vmglob_delete(vm_globals *) { }

/* 
 *   we don't require anything for the parameter declaration or usage, since
 *   we don't use the local parameter mechanism at all 
 */
#define VMG_
#define VMG0_
#define vmg_
#define vmg0_
#define VMGNULL_
#define VMG0NULL_
#define Pvmg0_P             /* "vmg0_" in parens, for constructor arguments */

/* 
 *   get the address of the globals - this doesn't do anything in this
 *   configuration, as there's not really a global variables structure 
 */
#define VMGLOB_ADDR   0

/* pass a stashed copy of the global pointer, if necessary */
#define vmg_var(x)
#define vmg_var0(x)

/* declare a local variable to access the globals */
#define VMGLOB_PTR(x)

/* global variables are statically declared so they're always available */
#define VMGLOB_IF_AVAIL(x) x

/* we access globals directly as individual statics */
#define VMGLOB_ACCESS(var) (G_##var##_X)
#define VMGLOB_PREACCESS(var) (&G_##var##_X)

#endif /* VMGLOB_VARS */

/* ------------------------------------------------------------------------ */
/*
 *   STATIC GLOBAL STRUCTURE configuration.  In this configuration, the
 *   globals are defined in a single static global structure.  This is the
 *   second most efficient configuration, but it only allows one VM instance
 *   per process.  
 */
#ifdef VMGLOB_STRUCT

/* define the global variables structure */
VMGLOB_DECLARE vm_globals G_vmglobals;

/* initialization - this has no effect in this mode */
inline vm_globals *vmglob_alloc()
{
    return &G_vmglobals;
}

/* termination - this has no effect in this mode */
inline void vmglob_delete(vm_globals *) { }

/* 
 *   we don't require anything for the parameter declaration or usage,
 *   since we don't use the local parameter mechanism at all 
 */
#define VMG_
#define VMG0_
#define vmg_
#define vmg0_
#define VMGNULL_
#define VMG0NULL_
#define Pvmg0_P

/* get the address of the globals */
#define VMGLOB_ADDR   (&G_vmglobals)

/* pass a stashed copy of the global pointer, if necessary */
#define vmg_var(x)
#define vmg_var0(x)

/* declare a local variable to access the globals */
#define VMGLOB_PTR(x)

/* global variables are statically declared so they're always available */
#define VMGLOB_IF_AVAIL(x) x

/* we access globals directly as individual statics */
#define VMGLOB_ACCESS(var)    (G_vmglobals.var)
#define VMGLOB_PREACCESS(var) (G_vmglobals.var)

#endif /* VMGLOB_STRUCT */

/* ------------------------------------------------------------------------ */
/*
 *   STATIC GLOBAL POINTER configuration.  In this configuration, the
 *   globals are stored in an allocated structure whose address is stored
 *   in a global static pointer variable.
 */
#ifdef VMGLOB_POINTER

/* define our global static pointer to the global variables */
VMGLOB_DECLARE vm_globals *G_vmglobals;

/* initialization - allocate a new set of globals */
inline vm_globals *vmglob_alloc()
{
    G_vmglobals = new vm_globals();
    return G_vmglobals;
}

/* termination - delete a set of globals */
inline void vmglob_delete(vm_globals *glob) { delete glob; }

/* 
 *   we don't require anything for the parameter declaration or usage,
 *   since we don't use the local parameter mechanism at all 
 */
#define VMG_
#define VMG0_
#define vmg_
#define vmg0_
#define VMGNULL_
#define VMG0NULL_
#define Pvmg0_P

/* get the address of the globals */
#define VMGLOB_ADDR   G_vmglobals

/* pass a stashed copy of the global pointer, if necessary */
#define vmg_var(x)
#define vmg_var0(x)

/* declare a local variable to access the globals */
#define VMGLOB_PTR(x)

/* test to see if a global variable is available */
#define VMGLOB_IF_AVAIL(x) (G_vmglobals != 0 ? x : 0)

/* accessing a global requires dereferencing the global pointer */
#define VMGLOB_ACCESS(var) (G_vmglobals->var)
#define VMGLOB_PREACCESS(var) (G_vmglobals_var)

#endif /* VMGLOB_POINTER */

/* ------------------------------------------------------------------------ */
/*
 *   PARAMETER configuration.  In this configuration, the globals are
 *   stored in an allocated structure whose address is passed to each VM
 *   function as a parameter. 
 */
#ifdef VMGLOB_PARAM

/* initialization - allocate a new set of globals */
inline vm_globals *vmglob_alloc() { return new vm_globals(); }

/* termination - delete a set of globals */
inline void vmglob_delete(vm_globals *glob) { delete glob; }

/* function declaration for the global pointer parameter */
#define VMG_   vm_globals *vmg__,
#define VMG0_  vm_globals *vmg__

/* parameter reference for passing to a function */
#define vmg_   vmg__,
#define vmg0_  vmg__
#define VMGNULL_  ((vm_globals *)NULL),
#define VMG0NULL_ ((vm_globals *)NULL)
#define Pvmg0_P  (vmg__)

/* get the address of the globals */
#define VMGLOB_ADDR   vmg__

/* pass a stashed copy of the global pointer, if necessary */
#define vmg_var(x)         x,
#define vmg_var0(x)        x

/* declare a local variable to access the globals */
#define VMGLOB_PTR(x) vm_globals *vmg__ = x

/* test to see if a global variable is available */
#define VMGLOB_IF_AVAIL(x) (vmg__ != 0 ? x : 0)

/* accessing a global variable requires dereferencing the parameter */
#define VMGLOB_ACCESS(var) (vmg__->var)
#define VMGLOB_PREACCESS(var) (vmg__->var)

#endif /* VMGLOB_PARAM */

/* ------------------------------------------------------------------------ */
/*
 *   Global variable accessors.  For convenience, we define these cover
 *   macros that access the global variables in the appropriate manner for
 *   our configuration.  Code can use these G_xxx symbols syntactically as
 *   though they were normal global variables.  
 */
#define G_mem         VMGLOB_ACCESS(mem)
#define G_undo        VMGLOB_ACCESS(undo)
#define G_meta_table  VMGLOB_ACCESS(meta_table)
#define G_bif_table   VMGLOB_ACCESS(bif_table)
#define G_varheap     VMGLOB_ACCESS(varheap)
#define G_preinit_mode VMGLOB_ACCESS(preinit_mode)
#define G_bif_tads_globals VMGLOB_ACCESS(bif_tads_globals)
#define G_host_ifc    VMGLOB_ACCESS(host_ifc)
#define G_image_loader VMGLOB_ACCESS(image_loader)
#define G_disp_cset_name  VMGLOB_ACCESS(disp_cset_name)
#define G_cmap_from_fname VMGLOB_ACCESS(cmap_from_fname)
#define G_cmap_to_fname VMGLOB_ACCESS(cmap_to_fname)
#define G_cmap_from_ui VMGLOB_ACCESS(cmap_from_ui)
#define G_cmap_to_ui  VMGLOB_ACCESS(cmap_to_ui)
#define G_cmap_from_file VMGLOB_ACCESS(cmap_from_file)
#define G_cmap_to_file VMGLOB_ACCESS(cmap_to_file)
#define G_cmap_to_log VMGLOB_ACCESS(cmap_to_log)
#define G_console     VMGLOB_ACCESS(console)
#define G_exc_entry_size VMGLOB_ACCESS(exc_entry_size)
#define G_line_entry_size VMGLOB_ACCESS(line_entry_size)
#define G_dbg_hdr_size VMGLOB_ACCESS(dbg_hdr_size)
#define G_srcf_table  VMGLOB_ACCESS(srcf_table)
#define G_dbg_lclsym_hdr_size VMGLOB_ACCESS(dbg_lclsym_hdr_size)
#define G_dbg_fmt_vsn VMGLOB_ACCESS(dbg_fmt_vsn)
#define G_dbg_frame_size VMGLOB_ACCESS(dbg_frame_size)
#define G_dyncomp     VMGLOB_ACCESS(dyncomp)
#define G_net_queue   VMGLOB_ACCESS(net_queue)
#define G_net_config  VMGLOB_ACCESS(net_config)
#define G_iter_get_next  VMGLOB_ACCESS(iter_get_next)
#define G_iter_next_avail  VMGLOB_ACCESS(iter_next_avail)
#define G_tadsobj_queue  VMGLOB_PREACCESS(tadsobj_queue)
#define G_predef      VMGLOB_PREACCESS(predef)
#define G_stk         G_interpreter
#define G_interpreter VMGLOB_PREACCESS(interpreter)
#define G_const_pool  VMGLOB_PREACCESS(const_pool)
#define G_code_pool   VMGLOB_PREACCESS(code_pool)
#define G_obj_table   VMGLOB_PREACCESS(obj_table)
#define G_syslogfile  VMGLOB_ACCESS(syslogfile)
#define G_file_path   VMGLOB_ACCESS(file_path)
#define G_sandbox_path VMGLOB_ACCESS(sandbox_path)
#define G_tzcache     VMGLOB_ACCESS(tzcache)
#define G_debugger    VMGLOB_ACCESS(debugger)

#endif /* VMGLOB_H */


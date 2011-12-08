/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmprof.h - T3 VM Profiler
Function
  Definitions for the execution profiler, which captures information on
  the time the program spends in each function.  Profiling information is
  useful for performance optimization, because it identifies the areas
  where the program is spending all of its time.

  Profiling can be optionally included when the VM is compiled.  Builds
  of the VM intended for use by end users should normally not include
  the profiler, because they will have no use for profiling, and
  including the profiler in the build enlarges the code size and adds
  run-time overhead.  Normally, the profiler should be included only in
  builds that include the interactive debugger.
Notes
  
Modified
  08/03/02 MJRoberts  - Creation
*/

#ifndef VMPROF_H
#define VMPROF_H

#include "vmtype.h"
#include "vmprofty.h"


/* ------------------------------------------------------------------------ */
/*
 *   Enable the profiler when compiling the VM by #defining VM_PROFILER on
 *   the compiler command line (usually done through the makefile's CFLAGS
 *   compiler options variable or equivalent).  When VM_PROFILE is not
 *   defined at compile-time, the profiling code will be omitted, making for
 *   a smaller executable and less run-time overhead.  
 */
#ifdef VM_PROFILER

/* include profiler-only code, and do not include non-profiler-only code */
#define VM_IF_PROFILER(x)      x
#define VM_IF_NOT_PROFILER(x)

#else /* VM_PROFILER */

/* do not include profiler code; do include non-profiler code */
#define VM_IF_PROFILER(x)
#define VM_IF_NOT_PROFILER(x)  x

#endif /* VM_PROFILER */

/* ------------------------------------------------------------------------ */
/*
 *   Profiler function call record.
 *   
 *   We keep a stack of these records alongside the regular VM stack, to
 *   track the amount of time spent in a function.  A record in this stack
 *   corresponds to an activation frame in the regular VM stack.
 *   
 *   We also keep a master table of these entries indexed by obj.prop/func
 *   identifier.  These records keep the running totals for each function.  
 */
struct vm_profiler_rec
{
    /* the object.property of the function, for a method */
    vm_obj_id_t obj;
    vm_prop_id_t prop;

    /* the entrypoint of the method */
    const uchar *func;

    /* the cumulative time DIRECTLY in this function (not in children) */
    vm_prof_time sum_direct;

    /* 
     *   the cumulative time in this function's children (but not directly in
     *   the function itself) 
     */
    vm_prof_time sum_chi;

    /* 
     *   invocation count (this is used in the running total records only,
     *   not in the stack) 
     */
    unsigned long call_cnt;
};

/* ------------------------------------------------------------------------ */
/*
 *   OS-specific profiler functions.  These must be provided by the OS
 *   implementation.  
 */

/*
 *   Get the current high-precision timer, filling in *t with the current
 *   time.  This indicates the time that has elapsed since an arbitrary zero
 *   point, defined by the OS code.  We only care about deltas from one time
 *   to another, so the zero point doesn't matter to us, as long as all times
 *   have a consistent zero time and can thus be subtracted to find elapsed
 *   time values.  The units are arbitrary; because this routine will be
 *   called very frequently, it should be as fast as possible, and thus
 *   should return the time in the native units the OS uses.  
 */
void os_prof_curtime(vm_prof_time *t);

/*
 *   Convert the elapsed time value to microseconds and return it as an
 *   unsigned long.  This should simply multiply the time value by the number
 *   of microseconds per OS-dependent unit that os_prof_curtime() returns,
 *   and return the value as a 32-bit unsigned long.
 *   
 *   We use a separate routine to convert the OS time units to microseconds
 *   so that we don't have to perform the conversion every time we note the
 *   current time, which we must do very frequently; instead, we defer the
 *   conversions until we're finished with a profiler run, at which point we
 *   have a set of summary data that is typically much smaller than the raw
 *   set we collect throughout execution.
 *   
 *   Note that our return precision of 32 bits can only represent time values
 *   up to about 4000 seconds, or about 70 minutes.  This is obviously not
 *   enough for complete generality, but it's a fairly long time by profiler
 *   standards, since the values we're measuring are the cumulative elapsed
 *   time spent in particular functions.  Given how much easier it is to work
 *   with 32-bit values than anything longer, and given that 70 minutes is a
 *   fairly generous ceiling for this particular application, we accept the
 *   loss of generality in exchange for the simplifaction.  
 */
unsigned long os_prof_time_to_ms(const vm_prof_time *t);


#endif /* VMPROF_H */


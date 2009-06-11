/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmprofty.h - basic type definitions for the profiler
Function
  This defines some basic types for the profiler.  We use a separate header
  to allow finer-grained inclusions.
Notes
  
Modified
  08/03/02 MJRoberts  - Creation
*/

#ifndef VMPROFTY_H
#define VMPROFTY_H

/* ------------------------------------------------------------------------ */
/*
 *   Profiler time record.  We use a 64-bit value for the time; this gives
 *   the time as a delta from some arbitrary zero point, defined by the OS,
 *   as a 64-bit quantity in units defined by the OS.
 *   
 *   We use a 64-bit value to allow for OS-provided timers with very high
 *   precision.  The OS doesn't necessarily have to use the full 64 bits; if
 *   only 32-bit timer values are available, the OS code can simply set the
 *   high-order part to zero.
 */
struct vm_prof_time
{
    /* the high-order and low-order 32 bits of the time value */
    unsigned long hi;
    unsigned long lo;
};


#endif /* VMPROFTY_H */

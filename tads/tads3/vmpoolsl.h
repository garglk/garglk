/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpoolsl.h - pool manager selector definitions
Function
  This header makes some definitions that vary according to which type of
  memory pool manager is selected.  This can be included without including
  the entire memory pool subsystem header.
Notes
  As of this writing, only the all-in-memory pool manager is supported.
  The swapping pool manager is no longer supported and is not planned to be
  supported in the future.  However, we are retaining the selection mechanism
  for now, in the event that we want to restore the swapping pool, or add
  other pool configuration options, in the future (which seems unlikely, but
  still).
Modified
  08/17/02 MJRoberts  - Creation
*/

#ifndef VMPOOLSL_H
#define VMPOOLSL_H

/* ------------------------------------------------------------------------ */
/*
 *   Conditionally include code needed for the swapping pool manager.  When
 *   the swapping pool manager is used, code that translates memory addresses
 *   must be mindful of the fact that translating one memory address can
 *   render previously-translated addresses invalid.  Such code is
 *   unnecessary with non-swapping pool managers.
 *   
 *   To include the swapping pool manager in the build, you must #define
 *   VM_SWAPPING_POOL globally for all modules - normally, this should be
 *   done in the CFLAGS in the makefile, or with the equivalent local
 *   convention, so that every module has the symbol defined.  
 */

/* ------------------------------------------------------------------------ */
#ifdef VM_SWAPPING_POOL

/* 
 *   swapping mode - include swapping-specific code 
 */
#define VM_IF_SWAPPING_POOL(x)  x

/* the final pool manager subclass - use the swapping pool class */
#define CVmPool_CLASS CVmPoolSwap

/* ------------------------------------------------------------------------ */
#else /* VM_SWAPPING_POOL */

/* 
 *   non-swapping mode - omit swapping-specific code 
 */
#define VM_IF_SWAPPING_POOL(x)

/* 
 *   The non-swapping pool comes in two varieties.  Select the FLAT or PAGED
 *   pool, as desired.  The FLAT pool is slightly faster, but it doesn't have
 *   any dynamic memory capabilities, which are required for the debugger.  
 */
#ifdef VM_FLAT_POOL

/* select the non-swapped FLAT pool */
#define CVmPool_CLASS CVmPoolFlat

#else /* VM_FLAT_POOL */

/* select the non-swapped PAGED pool */
#define CVmPool_CLASS CVmPoolInMem

#endif /* VM_FLAT_POOL */

#endif /* VM_SWAPPING_POOL */

#endif /* VMPOOLSL_H */

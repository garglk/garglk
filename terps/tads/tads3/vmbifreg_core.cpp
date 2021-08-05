#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmbifreg.cpp,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbifreg_core.cpp - core built-in function set registry (no UI)
Function
  Defines the built-in functions that are linked in to this
  implementation of the VM.  This is the "core" set, which includes the
  normal TADS 3 sets but excludes the user interface (specifically, we
  exclude tads-io, the console I/O function set that is part of the standard
  TADS 3 function set complement).

  This core function set is specifically designed to be extended by the
  host application, since it is expected that applications will want to add
  one or more new function sets to provide a user interface.  To extend this
  registration table with application-specific function sets, follow these
  steps:

  1.  Make a copy of this file (DO NOT MODIFY THE ORIGINAL).

  2.  Remove this file (and/or derived files, such as object files) from
  your makefile, and add your modified version instead.

  3.  In the table below, add an entry for each of your extra function
  sets.  (Of course, for each of your added function sets, you must
  implement the function set and link the implementation into your host
  application executable.)

Notes
  
Modified
  04/05/02 MJRoberts  - creation (from vmbifreg.cpp)
*/

#include "vmbifreg.h"

/* ------------------------------------------------------------------------ */
/*
 *   Include the function set vector definitions.  Define
 *   VMBIF_DEFINE_VECTOR so that the headers all generate vector
 *   definitions. 
 */
#define VMBIF_DEFINE_VECTOR

#include "vmbiftad.h"
#include "vmbift3.h"

// Include the header for our sample t3core function set
#include "vmcore.h"

// !!! INCLUDE HOST-SPECIFIC FUNCTION SET HEADERS HERE ("vmbifxxx.h")

/* done with the vector definition */
#undef VMBIF_DEFINE_VECTOR

#define MAKE_ENTRY(entry_name, cls) \
    { entry_name, countof(cls::bif_table), cls::bif_table, \
      &cls::attach, &cls::detach }

/* ------------------------------------------------------------------------ */
/*
 *   The function set registration table.  Each entry in the table
 *   provides the definition of one function set, keyed by the function
 *   set's universally unique identifier.  
 */
vm_bif_entry_t G_bif_reg_table[] =
{
    /* T3 VM system function set, v1 */
    MAKE_ENTRY("t3vm/010003", CVmBifT3),

    /* T3 VM Testing interface, v1 */
    MAKE_ENTRY("t3vmTEST/010000", CVmBifT3Test),
    
    /* TADS generic data manipulation functions */
    MAKE_ENTRY("tads-gen/030008", CVmBifTADS),

    // Our sample function set for the t3core build
    MAKE_ENTRY("core-sample/010000", CVmBifSample),

    // !!! ADD ANY HOST-SPECIFIC FUNCTION SETS HERE

    /* end of table marker */
    { 0, 0, 0 }
};

/* we don't need the MAKE_ENTRY macro any longer */
#undef MAKE_ENTRY


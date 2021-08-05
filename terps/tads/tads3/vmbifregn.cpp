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
  vmbifreg.cpp - built-in function set registry
Function
  Defines the built-in functions that are linked in to this implementation
  of the VM.

  This file is dependent on the host application environment configuration.
  This particular file includes a table for the base set of T3 VM
  built-in functions.  Some host application environments may provide
  additional function sets; if you're building a host system with its own
  extra function sets, do the following:

  1.  Make a copy of this file (DO NOT MODIFY THE ORIGINAL).

  2.  Remove this file (and/or derived files, such as object files) from
      your makefile, and add your modified version instead.

  3.  In the table below, add an entry for each of your extra function
      sets.  (Of course, for each of your added function sets, you must
      implement the function set and link the implementation into your
      host application executable.)

Notes
  
Modified
  12/05/98 MJRoberts  - Creation
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
#include "vmbiftio.h"
#include "vmbift3.h"
#include "vmbifnet.h"


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
    MAKE_ENTRY("t3vm/010006", CVmBifT3),

    /* T3 VM Testing interface, v1 */
    MAKE_ENTRY("t3vmTEST/010000", CVmBifT3Test),
    
    /* TADS generic data manipulation functions */
    MAKE_ENTRY("tads-gen/030008", CVmBifTADS),

    /* TADS input/output functions */
    MAKE_ENTRY("tads-io/030007", CVmBifTIO),

    /* TADS input/output functions */
    MAKE_ENTRY("tads-net/030001", CVmBifNet),

    // !!! ADD ANY HOST-SPECIFIC FUNCTION SETS HERE

    /* end of table marker */
    { 0, 0, 0 }
};

/* we don't need the MAKE_ENTRY macro any longer */
#undef MAKE_ENTRY


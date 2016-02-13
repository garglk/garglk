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
  vmbifregx.cpp - built-in function set registry, with tads-io-ext
Function
  Defines the built-in functions that are linked in to this implementation
  of the VM.  This version of the registry includes tads-io-ext, the extended
  I/O function set.
Notes
  
Modified
  03/02/2005 MJRoberts  - Creation
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
#include "vmbiftix.h"

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
    /* T3 VM system function set */
    MAKE_ENTRY("t3vm/010006", CVmBifT3),

    /* T3 VM Testing interface */
    MAKE_ENTRY("t3vmTEST/010000", CVmBifT3Test),
    
    /* TADS generic data manipulation functions */
    MAKE_ENTRY("tads-gen/030008", CVmBifTADS),

    /* TADS input/output functions */
    MAKE_ENTRY("tads-io/030007", CVmBifTIO),

    /* TADS extended input/output functions (if the platform supports them) */
    MAKE_ENTRY("tads-io-ext/030000", CVmBifTIOExt),
    
    // !!! ADD ANY HOST-SPECIFIC FUNCTION SETS HERE

    /* end of table marker */
    { 0, 0, 0 }
};

/* we don't need the MAKE_ENTRY macro any longer */
#undef MAKE_ENTRY


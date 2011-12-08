/* $Header: d:/cvsroot/tads/tads3/VMMCREG.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmcreg.h - Metaclass Registry definitions
Function
  
Notes
  
Modified
  12/01/98 MJRoberts  - Creation
*/

#ifndef VMMCREG_H
#define VMMCREG_H

/* ------------------------------------------------------------------------ */
/*
 *   Define the NORMAL version of the metaclass registration macro.  This
 *   version is used on all inclusions of metaclass header files in normal
 *   contexts, and does nothing.
 *   
 *   This macro is redefined for SPECIAL versions, where metaclass header
 *   files are included in order to build certain tables, such as the
 *   central registration table.
 */
#define VM_REGISTER_METACLASS(metaclass)


#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"

/*
 *   Include all of the core headers first, to ensure that we skip
 *   everything but the registration macros on subsequent inclusions while
 *   building the tables.  (Since each header is protected against
 *   multiple inclusion, except for the registration macros, we need only
 *   include each file once up front to ensure that these definitions
 *   won't show up again.) 
 */
#include "vmmccore.h"


/* ------------------------------------------------------------------------ */
/*
 *   Metaclass registration entry.  Each registered metaclass defines an
 *   entry like this. 
 */
struct vm_meta_reg_t
{
    /*
     *   The CVmMetaclass object that describes the metaclass 
     */
    class CVmMetaclass **meta;
};

/* ------------------------------------------------------------------------ */
/*
 *   Declare the global static table 
 */
extern vm_meta_reg_t G_meta_reg_table[];


/* ------------------------------------------------------------------------ */
/*
 *   Register metaclasses.  This must be called during VM initialization
 *   to assign each metaclass its registration index.  
 */
void vm_register_metaclasses();


#endif /* VMMCREG_H */


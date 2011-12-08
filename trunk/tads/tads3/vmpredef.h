/* $Header: d:/cvsroot/tads/tads3/VMPREDEF.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpredef.h - pre-defined objects and properties
Function
  Defines a global structure containing the pre-defined objects
  and properties.  These values are provided by the load image file's
  predefined value symbol table; at load time, we cache these values
  for quick access.
Notes
  
Modified
  12/09/98 MJRoberts  - Creation
*/

#ifndef VMPREDEF_H
#define VMPREDEF_H

#include "t3std.h"
#include "vmtype.h"

/*
 *   some special import names - these are exports that we might need to
 *   synthesize, so we need to know their names in the image loader code as
 *   well as in the imports table 
 */
#define VM_IMPORT_NAME_LASTPROPOBJ   "*LastPropObj"
#define VM_IMPORT_NAME_RTERRMSG      "exceptionMessage"
#define VM_IMPORT_NAME_CONSTSTR      "*ConstStrObj"
#define VM_IMPORT_NAME_CONSTLST      "*ConstLstObj"
#define VM_IMPORT_NAME_ISARGTABLE    "_isNamedArgTable"

/*
 *   pre-defined values 
 */
struct CVmPredef
{
    /* initialize */
    CVmPredef()
    {
        /* 
         *   the pre-defined variables are all undefined initially
         */
        reset();
    }

    /*
     *   Reset the predef variables to their initial undefined values 
     */
    void reset()
    {
        /* 
         *   include the import list to generate initializations for the
         *   predef variables 
         */
#define VM_IMPORT_OBJ(sym, mem) mem = VM_INVALID_OBJ;
#define VM_NOIMPORT_OBJ(sym, mem) VM_IMPORT_OBJ(sym, mem)

#define VM_IMPORT_PROP(sym, mem) mem = VM_INVALID_PROP;
#define VM_NOIMPORT_PROP(sym, mem) VM_IMPORT_PROP(sym, mem)

#define VM_IMPORT_FUNC(sym, mem) mem = 0;

#include "vmimport.h"
    }

    /*
     *   Now include the import list to generate the actual member variables
     *   for the imported symbols.  During image file load, the loader will
     *   set these members to the actual values imported from the image
     *   file.  
     */
#define VM_IMPORT_OBJ(sym, mem) vm_obj_id_t mem;
#define VM_NOIMPORT_OBJ(sym, mem) VM_IMPORT_OBJ(sym, mem)

#define VM_IMPORT_PROP(sym, mem) vm_prop_id_t mem;
#define VM_NOIMPORT_PROP(sym, mem) VM_IMPORT_PROP(sym, mem)

#define VM_IMPORT_FUNC(sym, mem) pool_ofs_t mem;

#include "vmimport.h"
};

#endif /* VMPREDEF_H */


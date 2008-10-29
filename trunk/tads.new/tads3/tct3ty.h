/* $Header: d:/cvsroot/tads/tads3/TCT3TY.H,v 1.1 1999/07/11 00:46:57 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3ty.h - T3 Target-specific Type definitions
Function
  
Notes
  
Modified
  07/09/99 MJRoberts  - Creation
*/

#ifndef TCT3TY_H
#define TCT3TY_H

#include "vmtype.h"

/* ------------------------------------------------------------------------ */
/*
 *   target type specifications 
 */

/* global object ID */
typedef vm_obj_id_t tctarg_obj_id_t;

/* global property ID */
typedef vm_prop_id_t tctarg_prop_id_t;

/* invalid object/property ID's */
const tctarg_obj_id_t TCTARG_INVALID_OBJ = VM_INVALID_OBJ;
const tctarg_prop_id_t TCTARG_INVALID_PROP = VM_INVALID_PROP;


#endif /* TCT3TY_H */


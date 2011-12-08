#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMTYPEDH.CPP,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtypedh.cpp - type manipulations for data holders
Function
  
Notes
  This is separated from vmtype.cpp so that these functions can be linked
  into an application without dragging in a bunch of things that other
  vmtype.cpp code depends upon.
Modified
  05/11/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmstack.h"


/* ------------------------------------------------------------------------ */
/*
 *   Portable data holder manipulation 
 */

/* 
 *   store a vm_val_t value in a portable data holder 
 */
void vmb_put_dh(char *buf, const vm_val_t *val)
{
    /* store the type code */
    buf[0] = (char)val->typ;

    /* store the value, in the appropriate type-dependent format */
    switch(val->typ)
    {
    case VM_OBJ:
    case VM_OBJX:
        /* store the object ID as a UINT4 */
        oswp4(buf+1, val->val.obj);
        break;

    case VM_PROP:
        /* store the property ID as a UINT2 */
        oswp2(buf+1, val->val.prop);
        break;

    case VM_INT:
        oswp4(buf+1, val->val.intval);
        break;

    case VM_BIFPTR:
    case VM_BIFPTRX:
        /* store the integer as two UINT2s: function index, set index */
        oswp2(buf+1, val->val.bifptr.func_idx);
        oswp2(buf+3, val->val.bifptr.set_idx);
        break;

    case VM_ENUM:
        /* store the enumerated constant value as a UINT4 */
        oswp4(buf+1, val->val.enumval);
        break;

    case VM_SSTRING:
    case VM_DSTRING:
    case VM_LIST:
    case VM_CODEOFS:
    case VM_FUNCPTR:
        /* store the offset value as a UINT4 */
        oswp4(buf+1, val->val.ofs);
        break;

    default:
        /* other types have no extra data or cannot be put into a DH */
        break;
    }
}

/*
 *   Get only the value portion of a vm_val_t from a portable data holder 
 */
void vmb_get_dh_val(const char *buf, vm_val_t *val)
{
    /* read the format appropriate to the type */
    switch((vm_datatype_t)buf[0])
    {
    case VM_OBJ:
    case VM_OBJX:
        /* get the object ID from the UINT4 */
        val->val.obj = (vm_obj_id_t)t3rp4u(buf+1);
        break;

    case VM_PROP:
        /* get the property ID from the UINT2 */
        val->val.prop = (vm_prop_id_t)osrp2(buf+1);
        break;

    case VM_INT:
        val->val.intval = osrp4(buf+1);
        break;

    case VM_BIFPTR:
    case VM_BIFPTRX:
        /* read the function index and set index as UINT2s */
        val->val.bifptr.func_idx = osrp2(buf+1);
        val->val.bifptr.set_idx = osrp2(buf+3);
        break;

    case VM_ENUM:
        /* get the enumerated constant value from the UINT4 */
        val->val.enumval = t3rp4u(buf+1);
        break;

    case VM_SSTRING:
    case VM_DSTRING:
    case VM_LIST:
    case VM_CODEOFS:
    case VM_FUNCPTR:
        /* get the offset value from the UINT4 */
        val->val.ofs = t3rp4u(buf+1);
        break;

    default:
        /* other types have no additional data or cannot be put in a DH */
        break;
    }
}


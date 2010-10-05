#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMTYPE.CPP,v 1.3 1999/05/17 02:52:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtype.cpp - VM types
Function
  
Notes
  
Modified
  11/18/98 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmpool.h"

/* ------------------------------------------------------------------------ */
/*
 *   Compare this value to another value to determine if the two values
 *   are equal. 
 */
int vm_val_t::equals(VMG_ const vm_val_t *v, int depth) const
{
    /* 
     *   if the second value is an object and the first isn't, use the object
     *   comparison of the second 
     */
    if (v->typ == VM_OBJ && typ != VM_OBJ)
        return vm_objp(vmg_ v->val.obj)
            ->equals(vmg_ v->val.obj, this, depth);

    /* figure out what to do based on my type */
    switch(typ)
    {
    case VM_NIL:
    case VM_TRUE:
        /* we match only if the other value is the same boolean value */
        return (v->typ == typ);

    case VM_STACK:
    case VM_CODEPTR:
        /* 
         *   we match only if the other value has the same type and its
         *   pointer value matches 
         */
        return (v->typ == typ && v->val.ptr == this->val.ptr);

    case VM_OBJ:
        /* use the object's polymorphic equality test routine */
        return vm_objp(vmg_ this->val.obj)
            ->equals(vmg_ this->val.obj, v, depth);

    case VM_PROP:
        /* we match if the other value is the same property ID */
        return (v->typ == VM_PROP && v->val.prop == this->val.prop);

    case VM_INT:
        /* we match if the other value is the same integer */
        return (v->typ == VM_INT && v->val.intval == this->val.intval);

    case VM_ENUM:
        return (v->typ == VM_ENUM && v->val.enumval == this->val.enumval);

    case VM_SSTRING:
        /* use the standard string comparison routine */
        return CVmObjString::
            const_equals(vmg_ G_const_pool->get_ptr(this->val.ofs), v);

    case VM_LIST:
        /* use the standard list comparison routine */
        return CVmObjList::
            const_equals(vmg_ this,
                         G_const_pool->get_ptr(this->val.ofs), v, depth);

    case VM_CODEOFS:
    case VM_FUNCPTR:
        /* we match if the other value is the same code offset */
        return (v->typ == typ && v->val.ofs == this->val.ofs);

    case VM_EMPTY:
        /* empty never matches anything */
        return FALSE;

    case VM_DSTRING:
        /* dstrings have no value, and are thus never equal to anything */
        return FALSE;

    default:
        /* other types are not recognized */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Calculate a hash value 
 */
uint vm_val_t::calc_hash(VMG_ int depth) const
{
    /* see what we have */
    switch(typ)
    {
    case VM_NIL:
        /* this is rather arbitrary */
        return 0;
        
    case VM_TRUE:
        /* this is arbitrary, but at least make it different from nil */
        return 1;
        
    case VM_EMPTY:
        /* also arbitrary */
        return 2;
        
    case VM_CODEOFS:
    case VM_FUNCPTR:
        /* use a 16-bit hash of the code address */
        return (uint)((val.ofs & 0xffff)
                      ^ ((val.ofs & 0xffff0000) >> 16));

    case VM_PROP:
        /* use the property ID as the hash */
        return (uint)val.prop;

    case VM_INT:
        /* use a 16-bit hash of the int */
        return (uint)((val.intval & 0xffff)
                      ^ ((val.intval & 0xffff0000) >> 16));

    case VM_ENUM:
        /* use a 16-bit hash of the enum value */
        return (uint)((val.enumval & 0xffff)
                      ^ ((val.enumval & 0xffff0000) >> 16));

    case VM_OBJ:
        /* ask the object to calculate its hash value */
        return vm_objp(vmg_ val.obj)->calc_hash(vmg_ val.obj, depth);
        break;

    case VM_SSTRING:
        /* get the hash of the constant string */
        return CVmObjString::
            const_calc_hash(G_const_pool->get_ptr(val.ofs));
        break;

    case VM_LIST:
        /* get the hash of the constant list */
        return CVmObjList::
            const_calc_hash(vmg_ this, G_const_pool->get_ptr(val.ofs), depth);

    default:
        /* return an arbitrary value for any other type */
        return 3;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare this value to the given value.  Returns a positive value if
 *   this value is greater than 'val', a negative value if this value is
 *   less than 'val', and 0 if the two values are equal.  Throws an error
 *   if a magnitude comparison is not meaningful for the involved types.  
 */
int vm_val_t::gen_compare_to(VMG_ const vm_val_t *v) const
{
    /* 
     *   if the other value is of type object and I'm not, let the object
     *   perform the comparison, and invert the sense of the result 
     */
    if (typ != VM_OBJ && v->typ == VM_OBJ)
        return -(v->compare_to(vmg_ this));

    /* the comparison depends on my type */
    switch(typ)
    {
    case VM_OBJ:
        /* let the object perform the comparison */
        return vm_objp(vmg_ this->val.obj)
            ->compare_to(vmg_ this->val.obj, v);

    case VM_SSTRING:
        /* compare the string */
        return CVmObjString::
            const_compare(vmg_ G_const_pool->get_ptr(this->val.ofs), v);

    case VM_INT:
        {
            int32 a, b;
            
            /* the comparison is legal only for another number */
            if (!v->is_numeric())
                err_throw(VMERR_INVALID_COMPARISON);
            
            /* get the integers */
            a = this->val.intval;
            b = v->val.intval;

            /* compare them and return the results */
            if (a > b)
                return 1;
            else if (a < b)
                return -1;
            else
                return 0;
        }

    default:
        /* other types cannot be compared for magnitude */
        err_throw(VMERR_INVALID_COMPARISON);

        /* this code is never reached, but the compiler doesn't know that */
        AFTER_ERR_THROW(return 0;)
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the underlying string constant value. 
 */
const char *vm_val_t::get_as_string(VMG0_) const
{
    /* check my type */
    if (typ == VM_SSTRING)
    {
        /* it's a constant string - return its text from the constant pool */
        return G_const_pool->get_ptr(val.ofs);
    }
    else if (typ == VM_OBJ)
    {
        /* it's an object - ask for its underlying string, if any */
        return vm_objp(vmg_ val.obj)->get_as_string(vmg0_);
    }
    else
    {
        /* other types do not have underlying strings */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the underlying list constant value.  
 */
const char *vm_val_t::get_as_list(VMG0_) const
{
    /* check my type */
    if (typ == VM_LIST)
    {
        /* it's a constant list - return its data from the constant pool */
        return G_const_pool->get_ptr(val.ofs);
    }
    else if (typ == VM_OBJ)
    {
        /* it's an object - ask for its underlying list, if any */
        return vm_objp(vmg_ val.obj)->get_as_list();
    }
    else
    {
        /* other types do not have underlying lists */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   get the effective number of elements when this value is used as the
 *   right-hand side of a '+' or '-' operator whose left-hand side is a
 *   collection type
 */
size_t vm_val_t::get_coll_addsub_rhs_ele_cnt(VMG0_) const
{
    /* the handling depends on the type */
    switch(typ)
    {
    case VM_LIST:
        /* it's a static list - return the number of elements in the list */
        return vmb_get_len(G_const_pool->get_ptr(val.ofs));

    case VM_OBJ:
        /* ask the object what it thinks */
        return vm_objp(vmg_ val.obj)->get_coll_addsub_rhs_ele_cnt(vmg0_);

    default:
        /* for any other type, there's only one element */
        return 1;
    }
}

/*
 *   get the nth element of a collection add/sub operation of which we're
 *   the right-hand side 
 */
void vm_val_t::get_coll_addsub_rhs_ele(VMG_ size_t idx,
                                       vm_val_t *result) const
{
    /* the handling depends on the type */
    switch(typ)
    {
    case VM_LIST:
        /* it's a static list - get the nth element from the list */
        CVmObjList::index_list(vmg_ result,
                               G_const_pool->get_ptr(val.ofs), idx);
        break;

    case VM_OBJ:
        /* ask the object for its indexed value */
        vm_objp(vmg_ val.obj)->get_coll_addsub_rhs_ele(
            vmg_ result, val.obj, idx);
        break;

    default:
        /* for anything else, we have just our own value */
        *result = *this;
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set to an integer giving the datatype code for the given value 
 */
void vm_val_t::set_datatype(VMG_ const vm_val_t *v)
{
    /* check for an object */
    if (v->typ == VM_OBJ)
    {
        /* check for List and String values */
        if (v->get_as_string(vmg0_) != 0)
        {
            /* treat this the same as a string constant */
            set_int((int)VM_SSTRING);
        }
        else if (v->get_as_list(vmg0_) != 0)
        {
            /* treat this the same as a list constant */
            set_int((int)VM_LIST);
        }
        else
        {
            /* any other type of object is simply an object */
            set_int((int)VM_OBJ);
        }
    }
    else
    {
        /* for any other type, return our internal type code */
        set_int((int)v->typ);
    }
}

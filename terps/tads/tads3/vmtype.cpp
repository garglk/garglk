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

    case VM_BIFPTR:
    case VM_BIFPTRX:
        /* we match if the other value refers to the same function */
        return (v->typ == VM_BIFPTR
                && v->val.bifptr.set_idx == this->val.bifptr.set_idx
                && v->val.bifptr.func_idx == this->val.bifptr.func_idx);

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

    case VM_OBJX:
        /* match if it's the same object */
        return (v->typ == typ && v->val.obj == this->val.obj);
        
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

    case VM_OBJX:
        /* use a 16-bit hash of the object ID */
        return (uint)((val.obj & 0xffff)
                      ^ ((val.obj & 0xffff0000) >> 16));

    case VM_PROP:
        /* use the property ID as the hash */
        return (uint)val.prop;

    case VM_INT:
    case VM_BIFPTR:
    case VM_BIFPTRX:
        /* 
         *   the set index and function index both tend to be small integers;
         *   multiply them and keep the low-order 16 bits 
         */
        return (uint)(val.bifptr.set_idx * val.bifptr.func_idx) & 0xffff;

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
        if (v->typ == VM_INT)
        {
            /* comparing two integers */
            int32_t a = this->val.intval, b = v->val.intval;
            return a - b;
        }
        else if (v->typ == VM_OBJ)
        {
            /* the other value is an object, so let it do the comparison */
            return -(vm_objp(vmg_ v->val.obj)->
                     compare_to(vmg_ v->val.obj, this));
        }
        else
        {
            /* other types can't be compared to integer */
            err_throw(VMERR_INVALID_COMPARISON);
            AFTER_ERR_THROW(return 0;)
        }

    default:
        /* if the other value is an object, let it do the comparison */
        if (v->typ == VM_OBJ)
        {
            return -(vm_objp(vmg_ v->val.obj)->
                     compare_to(vmg_ v->val.obj, this));
        }
        else
        {
            /* other types cannot be compared for magnitude */
            err_throw(VMERR_INVALID_COMPARISON);
            AFTER_ERR_THROW(return 0;)
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Is this a numeric value?  Returns true for integers and BigNumber
 *   values.
 */
int vm_val_t::nonint_is_numeric(VMG0_) const
{
    /* some objects represent numeric types (e.g., BigNumber) */
    if (typ == VM_OBJ)
        return vm_objp(vmg_ val.obj)->is_numeric();

    /* we shouldn't be called with an int, but just in case */
    if (typ == VM_INT)
        return TRUE;

    /* other types are non-numeric */
    return FALSE;
}


/*
 *   Convert a numeric type to integer 
 */
int32_t vm_val_t::nonint_num_to_int(VMG0_) const
{
    /* if it's an object, ask the object for its integer value */
    long l;
    if (typ == VM_OBJ && vm_objp(vmg_ val.obj)->get_as_int(&l))
        return l;

    /* we shouldn't be called with an int type, but just in case */
    if (typ == VM_INT)
        return val.intval;

    /* other types are not convertible */
    err_throw(VMERR_NUM_VAL_REQD);
    AFTER_ERR_THROW(return 0);
}

/*
 *   Convert a numeric type to doule
 */
double vm_val_t::nonint_num_to_double(VMG0_) const
{
    /* if it's an object, ask the object for its integer value */
    double d;
    if (typ == VM_OBJ && vm_objp(vmg_ val.obj)->get_as_double(vmg_ &d))
        return d;

    /* we shouldn't be called with an int type, but just in case */
    if (typ == VM_INT)
        return (double)val.intval;

    /* other types are not convertible */
    err_throw(VMERR_NUM_VAL_REQD);
    AFTER_ERR_THROW(return 0.);
}

/*
 *   Cast to integer 
 */
int32_t vm_val_t::nonint_cast_to_int(VMG0_) const
{
    /* check the type */
    switch (typ)
    {
    case VM_TRUE:
        return 1;

    case VM_NIL:
        return 0;

    case VM_INT:
        /* we shouldn't ever be called with an int value, but just in case */
        return val.intval;

    case VM_SSTRING:
        {
            /* get the string constant */
            const char *p = G_const_pool->get_ptr(val.ofs);
            
            /* parse it as an integer */
            vm_val_t i;
            CVmObjString::parse_num_val(
                vmg_ &i, p + VMB_LEN, vmb_get_len(p), 10, TRUE);

            /* return the integer */
            return i.val.intval;
        }

    case VM_OBJ:
        /* ask the object to do the conversion */
        return vm_objp(vmg_ val.obj)->cast_to_int(vmg0_);

    default:
        /* there's no integer conversion */
        err_throw(VMERR_NO_INT_CONV);
        AFTER_ERR_THROW(return 0;)
    }
}

/*
 *   Cast to numeric (integer or BigNumber)
 */
void vm_val_t::cast_to_num(VMG_ vm_val_t *retval) const
{
    /* check the type */
    switch (typ)
    {
    case VM_TRUE:
        retval->set_int(1);
        break;

    case VM_NIL:
        retval->set_int(0);
        break;
        
    case VM_INT:
        /* it's already numeric */
        retval->set_int(val.intval);
        break;

    case VM_SSTRING:
        {
            /* get the string constant */
            const char *p = G_const_pool->get_ptr(val.ofs);

            /* parse it as an integer */
            CVmObjString::parse_num_val(
                vmg_ retval, p + VMB_LEN, vmb_get_len(p), 10, FALSE);
        }
        break;

    case VM_OBJ:
        /* ask the object to do the conversion */
        vm_objp(vmg_ val.obj)->cast_to_num(vmg_ retval, val.obj);
        break;

    default:
        /* there's no numeric conversion */
        err_throw(VMERR_NO_NUM_CONV);
    }
}

/*
 *   Promote an integer to match my type 
 */
void vm_val_t::promote_int(VMG_ vm_val_t *val) const
{
    if (typ == VM_OBJ)
    {
        /* ask the object to perform the promotion */
        vm_objp(vmg_ this->val.obj)->promote_int(vmg_ val);
    }
    else
    {
        /* 
         *   we can't promote integers to other types; this is a "numeric
         *   value required" error, since we only try to promote integers
         *   when performing arithmetic involving an int and another type
         */
        err_throw(VMERR_NUM_VAL_REQD);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Cast to string 
 */
const char *vm_val_t::cast_to_string(VMG_ vm_val_t *new_str) const
{
    /* presume the value is just our own value */
    *new_str = *this;

    switch (typ)
    {
    case VM_SSTRING:
        /* return the string as is */
        return get_as_string(vmg0_);

    case VM_OBJ:
        /* cast the object to string */
        return vm_objp(vmg_ val.obj)->cast_to_string(vmg_ val.obj, new_str);

    case VM_INT:
        /* create a printable decimal representation of the string */
        {
            /* format the number into a temporary buffer */
            char buf[20];
            sprintf(buf, "%ld", (long)val.intval);

            /* create a new string object for the number */
            new_str->set_obj(
                CVmObjString::create(vmg_ FALSE, buf, strlen(buf)));

            /* return the string buffer */
            return new_str->get_as_string(vmg0_);
        }

    case VM_NIL:
        /* return an empty string */
        new_str->set_obj(CVmObjString::create(vmg_ FALSE, 0));
        return new_str->get_as_string(vmg0_);

    case VM_TRUE:
        /* return 'true' */
        new_str->set_obj(CVmObjString::create(vmg_ FALSE, "true", 4));
        return new_str->get_as_string(vmg0_);

    case VM_LIST:
        /* join the list into a string, separating items with commas */
        CVmObjList::join(vmg_ new_str, this, ",", 1);
        return new_str->get_as_string(vmg0_);
        
    default:
        err_throw(VMERR_NO_STR_CONV);
        AFTER_ERR_THROW(return 0;)
    }
}

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
 *   Get as a code pointer 
 */
const uchar *vm_val_t::get_as_codeptr(VMG0_) const
{
    /* check my type */
    switch (typ)
    {
    case VM_CODEOFS:
    case VM_FUNCPTR:
        /* 
         *   these types contain code pool offsets - translate the offset to
         *   a physical pointer 
         */
        return (const uchar *)G_code_pool->get_ptr(val.ofs);

    case VM_CODEPTR:
        /* it's already a physical code pointer */
        return (const uchar *)val.ptr;
        
    default:
        /* other types do not contain code pointers */
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
 *   Is this indexable as a list? 
 */
int vm_val_t::is_listlike(VMG0_) const
{
    return (get_as_list(vmg0_) != 0
            || (typ == VM_OBJ
                && (vm_objp(vmg_ val.obj)->get_as_list()
                    || vm_objp(vmg_ val.obj)->is_listlike(vmg_ val.obj))));
}

/*
 *   Get the number of elements in a list-like object 
 */
int vm_val_t::ll_length(VMG0_) const
{
    /* try it as a regular list first */
    const char *p;
    if ((p = get_as_list(vmg0_)) != 0)
        return vmb_get_len(p);

    /* try it as an object */
    if (typ == VM_OBJ)
        return vm_objp(vmg_ val.obj)->ll_length(vmg_ val.obj);

    /* we don't have a length */
    return -1;
}

/*
 *   Get an indexed value 
 */
void vm_val_t::ll_index(VMG_ vm_val_t *ret, const vm_val_t *idx) const
{
    /* try it as a regular list first, then as an object */
    const char *p;
    if ((p = get_as_list(vmg0_)) != 0)
    {
        /* get the index as an integer value */
        int i = idx->cast_to_int(vmg0_);

        /* check the range */
        if (i < 1 || i > (int)vmb_get_len(p))
            err_throw(VMERR_INDEX_OUT_OF_RANGE);

        /* it's in range - retrieve the element */
        vmb_get_dh(p + VMB_LEN + (size_t)((i - 1) * VMB_DATAHOLDER), ret);
    }
    else if (typ == VM_OBJ)
    {
        /* use the object method, allowing operator overloading */
        vm_objp(vmg_ val.obj)->index_val_ov(vmg_ ret, val.obj, idx);
    }
    else if ((p = get_as_string(vmg0_)) != 0)
    {
        /* string - parse the string as an integer in decimal format */
        CVmObjString::parse_num_val(
            vmg_ ret, p + VMB_LEN, vmb_get_len(p), 10, TRUE);
    }
    else
    {
        /* no value */
        ret->set_nil();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Is this an object of the given class? 
 */
int vm_val_t::is_instance_of(VMG_ vm_obj_id_t cls) const
{
    return (typ == VM_OBJ
            && cls != VM_INVALID_OBJ
            && vm_objp(vmg_ val.obj)->is_instance_of(vmg_ cls));
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

/* ------------------------------------------------------------------------ */
/*
 *   Is this a function pointer of some kind?
 */
int vm_val_t::is_func_ptr(VMG0_) const
{
    return (typ == VM_FUNCPTR
            || typ == VM_BIFPTR
            || (typ == VM_OBJ
                && vm_objp(vmg_ val.obj)->get_invoker(vmg_ 0)));
}

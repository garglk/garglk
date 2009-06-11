#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMSTR.CPP,v 1.3 1999/05/17 02:52:28 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstr.cpp - VM string metaclass implementation
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "t3std.h"
#include "vmmcreg.h"
#include "vmobj.h"
#include "vmstr.h"
#include "utf8.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmfile.h"
#include "vmstack.h"
#include "vmpool.h"
#include "vmmeta.h"
#include "vmrun.h"
#include "vmbif.h"
#include "vmpredef.h"
#include "vmlst.h"
#include "vmuni.h"
#include "vmcset.h"
#include "vmbytarr.h"
#include "charmap.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassString metaclass_reg_obj;
CVmMetaclass *CVmObjString::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (*CVmObjString::func_table_[])(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *str, uint *argc) =
{
    &CVmObjString::getp_undef,
    &CVmObjString::getp_len,
    &CVmObjString::getp_substr,
    &CVmObjString::getp_upper,
    &CVmObjString::getp_lower,
    &CVmObjString::getp_find,
    &CVmObjString::getp_to_uni,
    &CVmObjString::getp_htmlify,
    &CVmObjString::getp_starts_with,
    &CVmObjString::getp_ends_with,
    &CVmObjString::getp_to_byte_array,
    &CVmObjString::getp_replace
};

/* ------------------------------------------------------------------------ */
/*
 *   Static creation methods 
 */


/* create dynamically using stack arguments */
vm_obj_id_t CVmObjString::create_from_stack(VMG_ const uchar **, uint)
{
    /* dynamic string construction is not currently supported */
    err_throw(VMERR_BAD_DYNAMIC_NEW);
    
    /* the compiler doesn't know we won't make it here */
    AFTER_ERR_THROW(return VM_INVALID_OBJ;)
}

/* create a string with no initial contents */
vm_obj_id_t CVmObjString::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjString();
    return id;
}

/* create with a given buffer size */
vm_obj_id_t CVmObjString::create(VMG_ int in_root_set, size_t byte_size)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjString(vmg_ byte_size);
    return id;
}

/* create from a constant UTF-8 string */
vm_obj_id_t CVmObjString::create(VMG_ int in_root_set,
                                 const char *str, size_t bytelen)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjString(vmg_ str, bytelen);
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Constructors 
 */

/*
 *   create a string object with a given buffer size
 */
CVmObjString::CVmObjString(VMG_ size_t len)
{
    /* 
     *   the length is limited to an unsigned 16-bit value (NB: it really is
     *   65535 on ALL PLATFORMS - this is a portable limit imposed by the
     *   portable storage format, not a local platform limit) 
     */
    if (len > 65535)
    {
        ext_ = 0;
        err_throw(VMERR_STR_TOO_LONG);
    }
    
    /* 
     *   allocate space for the buffer plus the length prefix in the
     *   variable heap 
     */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(len + VMB_LEN, this);

    /* set the length */
    vmb_put_len(ext_, len);
}

/*
 *   create a string object from a given UTF8 string constant
 */
CVmObjString::CVmObjString(VMG_ const char *str, size_t len)
{
    /* check for the length limit */
    if (len > 65535)
    {
        ext_ = 0;
        err_throw(VMERR_STR_TOO_LONG);
    }        

    /* 
     *   allocate space for the string plus the length prefix in the
     *   variable heap 
     */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(len + VMB_LEN, this);

    /* 
     *   store the length prefix in portable format (so that we can easily
     *   write our contents to a saved state file) 
     */
    vmb_put_len(ext_, len);

    /* copy the string's bytes */
    memcpy(ext_ + VMB_LEN, str, len);
}

/* ------------------------------------------------------------------------ */
/*
 *   receive notification of deletion 
 */
void CVmObjString::notify_delete(VMG_ int in_root_set)
{
    /* free our extension */
    if (ext_ != 0 && !in_root_set)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set a property.  Strings have no settable properties, so simply
 *   signal an error indicating that the set-prop call is invalid.  
 */
void CVmObjString::set_prop(VMG_ CVmUndo *, vm_obj_id_t,
                            vm_prop_id_t, const vm_val_t *)
{
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/*
 *   Save the object to a file 
 */
void CVmObjString::save_to_file(VMG_ CVmFile *fp)
{
    size_t len;
    
    /* get our length */
    len = vmb_get_len(ext_);

    /* write the length prefix and the string */
    fp->write_bytes(ext_, len + VMB_LEN);
}

/*
 *   Restore the object from a file 
 */
void CVmObjString::restore_from_file(VMG_ vm_obj_id_t,
                                     CVmFile *fp, CVmObjFixup *)
{
    size_t len;
    
    /* read the length prefix */
    len = fp->read_uint2();

    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* 
     *   allocate our extension - make room for the length prefix plus the
     *   bytes of the string
     */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(len + VMB_LEN, this);

    /* store our length prefix */
    vmb_put_len(ext_, len);

    /* read the string */
    fp->read_bytes(ext_ + VMB_LEN, len);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a value to this string 
 */
void CVmObjString::add_val(VMG_ vm_val_t *result,
                           vm_obj_id_t self, const vm_val_t *val)
{
    /* 
     *   Use the generic string adder, using my extension as the constant
     *   string.  We store our extension in the general string format
     *   required by the static adder. 
     */
    add_to_str(vmg_ result, self, ext_, val);
}

/*
 *   Static string adder.  This creates a new string object that results
 *   from appending the given value to the given string constant.  This is
 *   defined statically so that this same code can be shared for adding to
 *   constant pool strings and adding to CVmObjString objects.
 *   
 *   'strval' must point to a constant string.  The first two bytes of the
 *   string are stored in portable UINT2 format and give the length in
 *   bytes of the string, not including the length prefix; immediately
 *   following the length prefix are the bytes of the string.
 *   
 *   Note that we *always* create a new object to hold the result, even if
 *   the new string is identical to the first, so that we consistently
 *   return a distinct reference from the original.  
 */
void CVmObjString::add_to_str(VMG_ vm_val_t *result,
                              vm_obj_id_t self, const char *strval1,
                              const vm_val_t *val)
{
    const char *strval2;
    char buf[128];
    vm_obj_id_t obj;
    size_t len1, len2;
    CVmObjString *objptr;
    vm_val_t new_obj2;
        
    /* convert the value to be appended to a string */
    strval2 = cvt_to_str(vmg_ &new_obj2, buf, sizeof(buf), val, 10);

    /* 
     *   push the new string (if any) and self, to protect the two strings
     *   from garbage collection 
     */
    G_stk->push()->set_obj(self);
    G_stk->push(&new_obj2);

    /* get the lengths of the two strings */
    len1 = vmb_get_len(strval1);
    len2 = vmb_get_len(strval2);

    /* create a new string object to hold the result */
    obj = create(vmg_ FALSE, len1 + len2);
    objptr = (CVmObjString *)vm_objp(vmg_ obj);

    /* copy the two strings into the new object's string buffer */
    objptr->copy_into_str(0, strval1 + VMB_LEN, len1);
    objptr->copy_into_str(len1, strval2 + VMB_LEN, len2);

    /* we're done with the garbage collection protection */
    G_stk->discard(2);

    /* return the new object in the result */
    result->set_obj(obj);
}


/* ------------------------------------------------------------------------ */
/*
 *   Allocate a string buffer large enough to hold a given value.  We'll
 *   use the provided buffer if possible.
 *   
 *   If the provided buffer is null or is not large enough, we'll allocate
 *   a new string object with a large enough buffer to hold the value, and
 *   return the object's extension as the buffer.  This object will never
 *   be referenced by anyone, so it will be deleted at the next garbage
 *   collection.
 *   
 *   The buffer size and requested size are in bytes.  
 */
char *CVmObjString::alloc_str_buf(VMG_ vm_val_t *new_obj,
                                  char *buf, size_t buf_size,
                                  size_t required_size)
{
    vm_obj_id_t obj;
    
    /* if the provided buffer is large enough, use it */
    if (buf != 0 && buf_size >= required_size)
    {
        /* there's no new object */
        new_obj->set_nil();
        
        /* return the buffer */
        return buf;
    }

    /* allocate a new string object */
    obj = create(vmg_ FALSE, required_size);

    /* return the new object's string buffer */
    return (char *)vm_objp(vmg_ obj)->cast_to_string(vmg_ obj, new_obj);
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert a value to a string 
 */
const char *CVmObjString::cvt_to_str(VMG_ vm_val_t *new_str,
                                     char *result_buf,
                                     size_t result_buf_size,
                                     const vm_val_t *val, int radix)
{
    /* presume we won't need to create a new string object */
    new_str->set_nil();
    
    /* check the type of the value */
    switch(val->typ)
    {
    case VM_SSTRING:
        /* it's a string constant - no conversion is necessary */
        return G_const_pool->get_ptr(val->val.ofs);

    case VM_OBJ:
        /* it's an object - ask it for its string representation */
        return vm_objp(vmg_ val->val.obj)
            ->cast_to_string(vmg_ val->val.obj, new_str);
        break;

    case VM_INT:
        /* 
         *   It's a number - convert it to a string.  Use the provided result
         *   buffer if possible, but make sure we have room for the number.
         *   The unicode values we're storing are in the ascii range, so we
         *   only need one byte per character.  The longest buffer we'd need,
         *   then, is 32 bytes, for a conversion to a binary digit string.
         *   The conversion also needs two bytes for the length prefix; give
         *   it a few extra bytes as insurance against future algorithm
         *   changes that might need more padding.  
         */
        result_buf = alloc_str_buf(vmg_ new_str,
                                   result_buf, result_buf_size, 40);

        /* generate the string */
        return cvt_int_to_str(result_buf, 40, val->val.intval, radix);

    case VM_NIL:
        /* nil - use the literal string "nil" */
        return "\003\000nil";
        break;

    case VM_TRUE:
        /* true - use the literal string "true" */
        return "\004\000true";
        break;

    default:
        /* other types cannot be added to a string */
        err_throw(VMERR_NO_STR_CONV);

        /* we never really get here, but the compiler doesn't know that */
        AFTER_ERR_THROW(return 0;)
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert an integer to a string, storing the result in the given
 *   buffer in portable string format (with length prefix).  The radix
 *   must be 8, 10, or 16.  
 *   
 *   Decimal numbers are treated as signed, and a leading dash is included
 *   if the number is negative.  Octal and hex numbers are treated as
 *   unsigned.
 *   
 *   For efficiency, we store the number at the end of the buffer (this
 *   makes it easy to generate the number, since we need to generate
 *   numerals in reverse order).  We return a pointer to the result, which
 *   may not start at the beginning of the buffer.  
 */
char *CVmObjString::cvt_int_to_str(char *buf, size_t buflen,
                                   int32 inval, int radix)
{
    int neg;
    uint32 val;
    char *p;
    size_t len;

    /* start at the end of the buffer */
    p = buf + buflen;

    /* 
     *   if it's negative, and we're converting to decimal representation,
     *   treat the value as signed and use a leading minus sign;
     *   otherwise, treat the value as unsigned 
     */
    if (radix == 10 && inval < 0)
    {
        /* note that we need a minus sign */
        neg = TRUE;

        /* use the positive value for the conversion */
        val = (uint32)(-inval);
    }
    else
    {
        /* the value is positive (or at least unsigned) */
        neg = FALSE;

        /* use the value as-is */
        val = (uint32)inval;
    }

    /* store numerals in reverse order */
    do
    {
        char c;

        /* if we have no more room, throw an error */
        if (p == buf)
            err_throw(VMERR_CONV_BUF_OVF);
        
        /* move on to the next available character in the buffer */
        --p;

        /* figure the character representation of this numeral */
        c = (char)(val % radix);
        if (c < 10)
            c += '0';
        else
            c += 'A' - 10;

        /* store the numeral at the current location */
        *p = c;

        /* divide the remaining number by the radix */
        val /= radix;
    } while (val != 0);

    /* store the leading minus sign if necessary */
    if (neg)
    {
        /* if we don't have room, throw an error */
        if (p == buf)
            err_throw(VMERR_CONV_BUF_OVF);

        /* move to the next byte */
        --p;

        /* store the minus sign */
        *p = '-';
    }

    /* calculate the length */
    len = buflen - (p - buf);

    /* make sure we have room for the length prefix */
    if (p < buf + 2)
        err_throw(VMERR_CONV_BUF_OVF);

    /* store the length prefix */
    p -= 2;
    vmb_put_len(p, len);

    /* return the pointer to the start of the number */
    return p;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check a value for equality 
 */
int CVmObjString::equals(VMG_ vm_obj_id_t self,
                         const vm_val_t *val, int /*depth*/) const
{
    /* if the other value is a reference to myself, we certainly match */
    if (val->typ == VM_OBJ && val->val.obj == self)
        return TRUE;

    /* 
     *   use the constant string comparison routine, using our underlying
     *   string as the constant string data 
     */
    return const_equals(vmg_ ext_, val);
}

/*
 *   Constant string equality test
 */
int CVmObjString::const_equals(VMG_ const char *str, const vm_val_t *val)
{
    const char *str2;
    size_t len;

    /* get the other value as a string */
    str2 = val->get_as_string(vmg0_);

    /* if the object doesn't have an underlying string, we don't match */
    if (str2 == 0)
        return FALSE;

    /* 
     *   if their lengths match, and the bytes match exactly, we have a
     *   match; otherwise, they're not equal 
     */
    len = vmb_get_len(str);
    return (len == vmb_get_len(str2)
            && memcmp(str + VMB_LEN, str2 + VMB_LEN, len) == 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash value 
 */
uint CVmObjString::calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const
{
    return const_calc_hash(ext_);
}

/*
 *   Hash value calculation 
 */
uint CVmObjString::const_calc_hash(const char *str)
{
    size_t len;
    uint hash;
    utf8_ptr p;
    
    /* get and skip the length prefix */
    len = vmb_get_len(str);
    str += VMB_LEN;

    /* scan the string and calculate the hash */
    for (p.set((char *)str), hash = 0 ; len != 0 ; p.inc(&len))
        hash += p.getch();

    /* return the result */
    return hash;
}


/* ------------------------------------------------------------------------ */
/*
 *   Compare this string to another value 
 */
int CVmObjString::compare_to(VMG_ vm_obj_id_t /*self*/,
                             const vm_val_t *val) const
{
    /* use the static string magnitude comparison routine */
    return const_compare(vmg_ ext_, val);
}

/*
 *   Compare a constant string value to another value.  Returns a positive
 *   number if the constant string is lexically greater than the other
 *   value, a negative number if the constant string is lexically less
 *   than the other value, or zero if the constant string is lexically
 *   identical to the other value.
 *   
 *   The other value must be a string constant or an object with an
 *   underlying string value.  We'll throw an error for any other type of
 *   value.  
 */
int CVmObjString::const_compare(VMG_ const char *str1, const vm_val_t *val)
{
    const char *str2;
    size_t len1, len2;

    /* get the other value as a string */
    str2 = val->get_as_string(vmg0_);

    /* if it's not a string, we can't compare it */
    if (str2 == 0)
        err_throw(VMERR_INVALID_COMPARISON);

    /* get the lengths of the two strings */
    len1 = vmb_get_len(str1);
    len2 = vmb_get_len(str2);

    /* perform a lexical comparison and return the result */
    return utf8_ptr::s_compare_to(str1 + VMB_LEN, len1, str2 + VMB_LEN, len2);
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a substring within a string 
 */
const char *CVmObjString::find_substr(VMG_ const char *str, int start_idx,
                                      const char *substr, size_t *idxp)
{
    utf8_ptr p;
    size_t rem;
    size_t sublen;
    size_t char_ofs;
    int i;
    
    /* get the lengths */
    rem = vmb_get_len(str);
    sublen = vmb_get_len(substr);

    /* set up utf8 pointer into the string */
    p.set((char *)str + 2);

    /* skip to the starting index */
    for (i = start_idx ; i > 0 && rem >= sublen ; --i, p.inc(&rem)) ;

    /* scan for the substring */
    for (char_ofs = 0 ; rem != 0 && rem >= sublen ; ++char_ofs, p.inc(&rem))
    {
        /* check for a match */
        if (memcmp(p.getptr(), substr + VMB_LEN, sublen) == 0)
        {
            /* it's a match - set the return index if they are interested */
            if (idxp != 0)
                *idxp = char_ofs + start_idx;

            /* return the current pointer */
            return p.getptr();
        }
    }

    /* we didn't find it - so indicate by returning null */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate a property 
 */
int CVmObjString::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                           vm_obj_id_t self, vm_obj_id_t *source_obj,
                           uint *argc)
{
    vm_val_t self_val;
    
    /* use the constant evaluator */
    self_val.set_obj(self);
    if (const_get_prop(vmg_ retval, &self_val, ext_, prop, source_obj, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling from the base object class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate a property of a constant string value 
 */
int CVmObjString::const_get_prop(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val, const char *str,
                                 vm_prop_id_t prop, vm_obj_id_t *src_obj,
                                 uint *argc)
{
    uint func_idx;

    /* presume no source object */
    *src_obj = VM_INVALID_OBJ;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function */
    if ((*func_table_[func_idx])(vmg_ retval, self_val, str, argc))
        return TRUE;

    /* 
     *   If this is a constant string (which is indicated by an invalid
     *   'self' object ID), try inheriting the default object
     *   interpretation, passing the constant string placeholder object
     *   for its type information.  
     */
    if (self_val->typ != VM_OBJ)
    {
        /* try going to CVmObject directly */
        if (vm_objp(vmg_ G_predef->const_str_obj)
            ->CVmObject::get_prop(vmg_ prop, retval, G_predef->const_str_obj,
                                  src_obj, argc))
            return TRUE;
    }

    /* not handled */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - get the length 
 */
int CVmObjString::getp_len(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                           const char *str, uint *argc)
{
    utf8_ptr p;
    static CVmNativeCodeDesc desc(0);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set up a utf-8 pointer to the string's contents */
    p.set((char *)str + VMB_LEN);

    /* return the character length of the string */
    retval->set_int(p.len(vmb_get_len(str)));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - extract a substring
 */
int CVmObjString::getp_substr(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                              const char *str, uint *in_argc)
{
    long start;
    ulong len = 0;
    size_t rem;
    utf8_ptr p;
    utf8_ptr start_p;
    size_t start_rem;
    size_t new_len;
    vm_obj_id_t obj;
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    static CVmNativeCodeDesc desc(1, 1);

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* pop the starting index */
    start = CVmBif::pop_long_val(vmg0_);

    /* pop the length, if present */
    if (argc >= 2)
        len = CVmBif::pop_long_val(vmg0_);

    /* push a self-reference to protect against GC */
    G_stk->push(self_val);

    /* set up a utf8 pointer to traverse the string */
    p.set((char *)str + VMB_LEN);

    /* get the byte length of the string */
    rem = vmb_get_len(str);

    /* 
     *   Skip ahead to the starting index.  If the index is positive, it's
     *   an index from the start of the string; if it's negative, it's an
     *   offset from the end of the string.  
     */
    if (start > 0)
    {
        /* 
         *   it's an index from the start - skip ahead by start-1 characters
         *   (since a start value of 1 tells us to start at the first
         *   character) 
         */
        for ( ; start > 1 && rem != 0 ; --start)
            p.inc(&rem);
    }
    else if (start < 0)
    {
        /*
         *   It's an index from the end of the string: -1 tells us to start
         *   at the last character, -2 at the second to last, and so on.
         *   Move to the first byte past the end of the string, and work
         *   backwards by the given number of characters.  
         */
        for (p.set((char *)str + VMB_LEN + rem), rem = 0 ;
             start < 0 && p.getptr() != (char *)str + VMB_LEN ; ++start)
        {
            /* move back one character */
            p.dec(&rem);
        }
    }

    /* this is the starting position */
    start_p = p;
    start_rem = rem;

    /* 
     *   if a length was specified, calculate the number of bytes in the
     *   given length; otherwise, use the entire remainder of the string 
     */
    if (argc >= 2)
    {
        /* keep skipping ahead by the desired length */
        for ( ; len > 0 && rem != 0 ; --len)
            p.inc(&rem);

        /* use the difference in lengths from the starting point to here */
        new_len = start_rem - rem;
    }
    else
    {
        /* use the entire remainder of the string */
        new_len = start_rem;
    }

    /* create the new string */
    obj = CVmObjString::create(vmg_ FALSE, start_p.getptr(), new_len);

    /* return the new object */
    retval->set_obj(obj);

    /* discard the GC protection references */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - toUpper
 */
int CVmObjString::getp_upper(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *str, uint *argc)
{
    size_t srclen;
    size_t dstlen;
    size_t rem;
    utf8_ptr srcp;
    utf8_ptr dstp;
    vm_obj_id_t result_obj;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get my length */
    srclen = vmb_get_len(str);

    /* leave the string on the stack as GC protection */
    G_stk->push(self_val);

    /* 
     *   Scan the string to determine how long the result will be.  The
     *   result won't necessarily be the same length as the original,
     *   because a two-byte character in the original could turn into a
     *   three-byte character in the result, and vice versa.  (We could
     *   allocate a result buffer three times the length of the original,
     *   but this seems more wasteful of space than scanning the string
     *   twice is wasteful of time.  It's a trade-off, though.)  
     */
    for (dstlen = 0, srcp.set((char *)str + VMB_LEN), rem = srclen ;
         rem != 0 ; srcp.inc(&rem))
    {
        /* get the size of the mapping for this character */
        dstlen += utf8_ptr::s_wchar_size(t3_to_upper(srcp.getch()));
    }

    /* allocate the result string */
    result_obj = CVmObjString::create(vmg_ FALSE, dstlen);

    /* get a pointer to the result buffer */
    dstp.set(((CVmObjString *)vm_objp(vmg_ result_obj))->cons_get_buf());

    /* write the string */
    for (srcp.set((char *)str + VMB_LEN), rem = srclen ;
         rem != 0 ; srcp.inc(&rem))
    {
        /* write the next character */
        dstp.setch(t3_to_upper(srcp.getch()));
    }

    /* return the value */
    retval->set_obj(result_obj);

    /* discard GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - toLower
 */
int CVmObjString::getp_lower(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *str, uint *argc)
{
    size_t srclen;
    size_t dstlen;
    size_t rem;
    utf8_ptr srcp;
    utf8_ptr dstp;
    vm_obj_id_t result_obj;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get my length */
    srclen = vmb_get_len(str);

    /* leave the string on the stack as GC protection */
    G_stk->push(self_val);

    /* 
     *   Scan the string to determine how long the result will be.  The
     *   result won't necessarily be the same length as the original,
     *   because a two-byte character in the original could turn into a
     *   three-byte character in the result, and vice versa.  (We could
     *   allocate a result buffer three times the length of the original,
     *   but this seems more wasteful of space than scanning the string
     *   twice is wasteful of time.  It's a trade-off, though.)  
     */
    for (dstlen = 0, srcp.set((char *)str + VMB_LEN), rem = srclen ;
         rem != 0 ; srcp.inc(&rem))
    {
        /* get the size of the mapping for this character */
        dstlen += utf8_ptr::s_wchar_size(t3_to_lower(srcp.getch()));
    }

    /* allocate the result string */
    result_obj = CVmObjString::create(vmg_ FALSE, dstlen);

    /* get a pointer to the result buffer */
    dstp.set(((CVmObjString *)vm_objp(vmg_ result_obj))->cons_get_buf());

    /* write the string */
    for (srcp.set((char *)str + VMB_LEN), rem = srclen ;
         rem != 0 ; srcp.inc(&rem))
    {
        /* write the next character */
        dstp.setch(t3_to_lower(srcp.getch()));
    }

    /* return the value */
    retval->set_obj(result_obj);

    /* discard GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - find
 */
int CVmObjString::getp_find(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                            const char *str, uint *argc)
{
    const char *str2;
    size_t idx;
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    int start_idx;
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the string to find */
    str2 = CVmBif::pop_str_val(vmg0_);

    /* if there's a starting index, retrieve it */
    start_idx = (orig_argc >= 2 ? CVmBif::pop_int_val(vmg0_) - 1 : 0);
    
    /* find the substring */
    if (find_substr(vmg_ str, start_idx, str2, &idx) != 0)
    {
        /* we found it - adjust to a 1-based value for return */
        retval->set_int(idx + 1);
    }
    else
    {
        /* didn't find it - return nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   replace flags 
 */
#define GETP_RPL_ALL    0x0001

/*
 *   property evaluator - replace 
 */
int CVmObjString::getp_replace(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *str, uint *argc)
{
    vm_val_t arg1;
    vm_val_t arg2;
    const char *substr;
    const char *rplstr;
    size_t sublen;
    size_t rpllen;
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(3, 1);
    int flags;
    utf8_ptr p;
    size_t rem;
    size_t new_len;
    int found;
    int start_idx;
    const char *rpl_start;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   make copies of the string references, so we can put them back on the
     *   stack as gc protection while we're working 
     */
    arg1 = *G_stk->get(0);
    arg2 = *G_stk->get(1);

    /* retrieve the search and replacement substrings */
    substr = CVmBif::pop_str_val(vmg0_);
    rplstr = CVmBif::pop_str_val(vmg0_);

    /* note the string lengths */
    sublen = vmb_get_len(substr);
    rpllen = vmb_get_len(rplstr);

    /* get the flags */
    flags = CVmBif::pop_int_val(vmg0_);

    /* if there's a starting index, retrieve it */
    start_idx = (orig_argc >= 4 ? CVmBif::pop_int_val(vmg0_) - 1 : 0);

    /* put the string references back on the stack for gc protection */
    G_stk->push(&arg1);
    G_stk->push(&arg2);

    /* start at the beginning of the string to search */
    rem = new_len = vmb_get_len(str);
    p.set((char *)str + 2);

    /* skip ahead to the starting index */
    for ( ; start_idx > 0 && rem >= sublen ; --start_idx, p.inc(&rem)) ;

    /* 
     *   note the starting index for replacements - we don't want to replace
     *   anything before this point 
     */
    rpl_start = p.getptr();

    /* 
     *   Scan for instances of the substring, so we can figure out how big
     *   the result string will be.  Don't actually do any replacements yet;
     *   we'll scan again once we know how the result size.  
     */
    for (found = FALSE ; rem >= sublen ; )
    {
        /* if this is a match for the substring, note it */
        if (memcmp(p.getptr(), substr + VMB_LEN, vmb_get_len(substr)) == 0)
        {
            /* note the find */
            found = TRUE;

            /* it's a match - adjust the result length for the replacement */
            new_len += rpllen - sublen;

            /* if we're replacing one instance only, look no further */
            if ((flags & GETP_RPL_ALL) == 0)
                break;

            /* skip the entire substring in the source */
            p.set(p.getptr() + sublen);
            rem -= sublen;
        }
        else
        {
            /* skip one character */
            p.inc(&rem);
        }
    }

    /* 
     *   if we found no instances of the search substring, the result is
     *   simply the source string; otherwise, we must create a new string
     *   with the substitution(s) 
     */
    if (found)
    {
        utf8_ptr dst;

        /* allocate the new string */
        retval->set_obj(create(vmg_ FALSE, new_len));

        /* get a pointer to the buffer */
        dst.set(((CVmObjString *)vm_objp(vmg_ retval->val.obj))
                ->cons_get_buf());

        /* scan the string for replacements */
        for (p.set((char *)str + 2), rem = vmb_get_len(str) ;
             rem >= sublen ; )
        {
            /* 
             *   If this is a match for the substring, and we've reached the
             *   starting point for replacements, replace the substring. 
             */
            if (p.getptr() >= rpl_start
                && memcmp(p.getptr(), substr + VMB_LEN, sublen) == 0)
            {
                /* it's a match - copy the replacement into the result */
                memcpy(dst.getptr(), rplstr + VMB_LEN, rpllen);

                /* move past the replacement in the result */
                dst.set(dst.getptr() + rpllen);

                /* move past the search substring in the source */
                p.set(p.getptr() + sublen);
                rem -= sublen;

                /* if we're replacing one instance only, look no further */
                if ((flags & GETP_RPL_ALL) == 0)
                    break;
            }
            else
            {
                /* copy the current character to the result */
                dst.setch(p.getch());

                /* skip the current character of input */
                p.inc(&rem);
            }
        }

        /* copy the remaining source into the result */
        if (rem != 0)
            memcpy(dst.getptr(), p.getptr(), rem);
    }
    else
    {
        /* we didn't find it - the result is simply the original string */
        *retval = *self_val;
    }

    /* discard the gc protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - convert to unicode
 */
int CVmObjString::getp_to_uni(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *in_argc)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    size_t bytelen;
    ulong idx = 0;
    utf8_ptr p;
    static CVmNativeCodeDesc desc(0, 1);

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* retrieve the index argument if present */
    if (argc >= 1)
        idx = CVmBif::pop_long_val(vmg0_);

    /* push a self-reference as GC protection */
    G_stk->push(self_val);

    /* get and skip the string's length prefix */
    bytelen = vmb_get_len(str);
    str += VMB_LEN;

    /* set up a utf8 pointer to the string */
    p.set((char *)str);

    /* check for an index argument */
    if (argc >= 1)
    {
        /* skip through the string until we get to the desired index */
        for ( ; idx > 1 && bytelen != 0 ; --idx, p.inc(&bytelen)) ;

        /* check to see if we have a character available */
        if (idx == 1 && bytelen != 0)
        {
            /* the index is valid - return the character here */
            retval->set_int((long)p.getch());
        }
        else
        {
            /* 
             *   the index is past the end of the string or is less than 1
             *   - return nil to indicate that there's no character here 
             */
            retval->set_nil();
        }
    }
    else
    {
        size_t charlen;
        vm_obj_id_t lst_obj;
        CVmObjList *lst;
        size_t i;

        /* 
         *   There's no index argument - they want a list of all of the
         *   code points in the string.  First, get the number of
         *   characters in the string.  
         */
        charlen = p.len(bytelen);

        /* create a list to hold the results */
        lst_obj = CVmObjList::create(vmg_ FALSE, charlen);
        lst = (CVmObjList *)vm_objp(vmg_ lst_obj);

        /* set the list's elements to the unicode characters values */
        for (i = 0 ; i < charlen ; ++i, p.inc())
        {
            wchar_t ch;
            vm_val_t ele_val;

            /* get this character */
            ch = p.getch();

            /* set this list element */
            ele_val.set_int((long)ch);
            lst->cons_set_element(i, &ele_val);
        }

        /* return the list object */
        retval->set_obj(lst_obj);
    }
    
    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - htmlify
 */

/* 
 *   htmlify flags 
 */

/* preserve spaces */
#define VMSTR_HTMLIFY_KEEP_SPACES   0x0001

/* preserve newlines */
#define VMSTR_HTMLIFY_KEEP_NEWLINES 0x0002

/* preserve tabs */
#define VMSTR_HTMLIFY_KEEP_TABS     0x0004

/*
 *   htmlify implementation 
 */
int CVmObjString::getp_htmlify(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *str, uint *in_argc)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    size_t bytelen;
    utf8_ptr p;
    utf8_ptr dstp;
    size_t rem;
    size_t extra;
    long flags;
    vm_obj_id_t result_obj;
    int prv_was_sp;
    static CVmNativeCodeDesc desc(0, 1);

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* if they specified flags, pop them */
    if (argc >= 1)
    {
        /* retrieve the flags */
        flags = CVmBif::pop_long_val(vmg0_);
    }
    else
    {
        /* no flags */
        flags = 0;
    }

    /* push a self-reference as GC protection */
    G_stk->push(self_val);

    /* get and skip the string's length prefix */
    bytelen = vmb_get_len(str);
    str += VMB_LEN;

    /* 
     *   scan the string to determine how much space we'll have to add to
     *   generate the htmlified version 
     */
    for (prv_was_sp = FALSE, extra = 0, p.set((char *)str), rem = bytelen ;
         rem != 0 ; p.inc(&rem))
    {
        int this_is_sp;

        /* presume it's not a space */
        this_is_sp = FALSE;
        
        /* check what we have */
        switch(p.getch())
        {
        case '&':
            /* we must replace '&' with '&amp;' - this adds four bytes */
            extra += 4;
            break;
            
        case '<':
            /* we must replace '<' with '&lt;' - this adds three bytes */
            extra += 3;
            break;

        case ' ':
            /* 
             *   If we're in preserve-spaces mode, and the previous space
             *   was some kind of whitespace character, change this to
             *   '&nbsp;' - this adds five bytes 
             */
            if (prv_was_sp && (flags & VMSTR_HTMLIFY_KEEP_SPACES) != 0)
                extra += 5;

            /* note that this was a whitespace character */
            this_is_sp = TRUE;
            break;

        case '\t':
            /* if we're in preserve-tabs mode, change this to '<tab>' */
            if ((flags & VMSTR_HTMLIFY_KEEP_TABS) != 0)
                extra += 4;

            /* note that this was a whitespace character */
            this_is_sp = TRUE;
            break;

        case '\n':
        case 0x2028:
            /* if we're in preserve-newlines mode, change this to '<br>' */
            if ((flags & VMSTR_HTMLIFY_KEEP_NEWLINES) != 0)
                extra += 3;

            /* note that this was a whitespace character */
            this_is_sp = TRUE;
            break;
        }

        /* for next time, remember whether this is a space */
        prv_was_sp = this_is_sp;
    }

    /* allocate space for the new string */
    result_obj = create(vmg_ FALSE, bytelen + extra);

    /* get a pointer to the result buffer */
    dstp.set(((CVmObjString *)vm_objp(vmg_ result_obj))->cons_get_buf());

    /* translate the string and write the result */
    for (prv_was_sp = FALSE, p.set((char *)str), rem = bytelen ;
         rem != 0 ; p.inc(&rem))
    {
        wchar_t ch;
        int this_is_sp;

        /* get this character */
        ch = p.getch();

        /* presume it's not a space */
        this_is_sp = FALSE;
        
        /* check what we have */
        switch(ch)
        {
        case '&':
            /* replace '&' with '&amp;' */
            dstp.setch_str("&amp;");
            break;

        case '<':
            /* we must replace '<' with '&lt;' - this adds three bytes */
            dstp.setch_str("&lt;");
            break;

        case ' ':
            /* note that this was a whitespace character */
            this_is_sp = TRUE;

            /* 
             *   ignore it if not in preserve-spaces mode, or if the
             *   previous character wasn't whitespace of some kind 
             */
            if (!prv_was_sp || (flags & VMSTR_HTMLIFY_KEEP_SPACES) == 0)
                goto do_default;

            /* add the nbsp */
            dstp.setch_str("&nbsp;");
            break;

        case '\t':
            /* note that this was a whitespace character */
            this_is_sp = TRUE;

            /* ignore if not in preserve-tabs mode */
            if ((flags & VMSTR_HTMLIFY_KEEP_TABS) == 0)
                goto do_default;

            /* add the <tab> */
            dstp.setch_str("<tab>");
            break;

        case '\n':
        case 0x2028:
            /* note that this was a whitespace character */
            this_is_sp = TRUE;

            /* if we're not in preserve-newlines mode, ignore it */
            if ((flags & VMSTR_HTMLIFY_KEEP_NEWLINES) == 0)
                goto do_default;

            /* add the <br> */
            dstp.setch_str("<br>");
            break;

        default:
        do_default:
            /* copy this character unchanged */
            dstp.setch(ch);
            break;
        }

        /* for next time, remember whether this is a space */
        prv_was_sp = this_is_sp;
    }

    /* return the new string */
    retval->set_obj(result_obj);
    
    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - startsWith 
 */
int CVmObjString::getp_starts_with(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *str, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    const char *str2;
    size_t len;
    size_t len2;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the other string */
    str2 = CVmBif::pop_str_val(vmg0_);

    /* get the lengths of the two strings */
    len = vmb_get_len(str);
    len2 = vmb_get_len(str2);

    /* move to the contents of each string */
    str += VMB_LEN;
    str2 += VMB_LEN;

    /* 
     *   if the other string is no longer than our string, and the other
     *   string matches our string exactly for the other string's entire
     *   length, we start with the other string 
     */
    retval->set_logical(len2 <= len && memcmp(str, str2, len2) == 0);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - endsWith 
 */
int CVmObjString::getp_ends_with(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *str, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    const char *str2;
    size_t len;
    size_t len2;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the other string */
    str2 = CVmBif::pop_str_val(vmg0_);

    /* get the lengths of the two strings */
    len = vmb_get_len(str);
    len2 = vmb_get_len(str2);

    /* move to the contents of each string */
    str += VMB_LEN;
    str2 += VMB_LEN;

    /* 
     *   If the other string is no longer than our string, and the other
     *   string matches our string at the end exactly for the other string's
     *   entire length, we start with the other string.  Note we don't need
     *   to worry about finding a valid character index in our string for
     *   the ending offset, because all we care about is whether or not we
     *   have an exact byte match between our suffix and the other string.  
     */
    retval->set_logical(len2 <= len
                        && memcmp(str + len - len2, str2, len2) == 0);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - mapToByteArray 
 */
int CVmObjString::getp_to_byte_array(VMG_ vm_val_t *retval,
                                     const vm_val_t *self_val,
                                     const char *str, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    size_t len;
    CCharmapToLocal *mapper;
    vm_val_t *arg;
    size_t byte_len;
    size_t src_bytes_used;
    size_t out_idx;
    CVmObjByteArray *arr;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the CharacterSet object and make sure it's valid */
    arg = G_stk->get(0);
    if (arg->typ != VM_OBJ || !CVmObjCharSet::is_charset(vmg_ arg->val.obj))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the to-local mapping from the character set */
    mapper = ((CVmObjCharSet *)vm_objp(vmg_ arg->val.obj))
             ->get_to_local(vmg0_);

    /* get my length and skip the length prefix */
    len = vmb_get_len(str);
    str += VMB_LEN;

    /* 
     *   first, do a mapping with a null output buffer to determine how many
     *   bytes we need for the mapping 
     */
    byte_len = mapper->map_utf8(0, 0, str, len, &src_bytes_used);

    /* allocate a new ByteArray with the required number of bytes */
    retval->set_obj(CVmObjByteArray::create(vmg_ FALSE, byte_len));
    arr = (CVmObjByteArray *)vm_objp(vmg_ retval->val.obj);

    /* convert it again, this time storing the bytes */
    for (out_idx = 1 ; len != 0 ; )
    {
        char buf[128];

        /* convert a buffer-full */
        byte_len = mapper->map_utf8(buf, sizeof(buf), str, len,
                                    &src_bytes_used);

        /* store the bytes in the byte array */
        arr->cons_copy_from_buf((unsigned char *)buf, out_idx, byte_len);

        /* advance past the output bytes we used */
        out_idx += byte_len;

        /* advance past the source bytes we used */
        str += src_bytes_used;
        len -= src_bytes_used;
    }

    /* discard arguments */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Constant-pool string object 
 */

/*
 *   create 
 */
vm_obj_id_t CVmObjStringConst::create(VMG_ const char *const_ptr)
{
    /* create our new ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* create our string object, pointing directly to the constant pool */
    new (vmg_ id) CVmObjStringConst(vmg_ const_ptr);

    /* return the new ID */
    return id;
}

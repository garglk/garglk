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
#include <limits.h>
#include <stdarg.h>

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
#include "vmstrbuf.h"
#include "charmap.h"
#include "vmnet.h"
#include "vmstrref.h"
#include "sha2.h"
#include "md5.h"
#include "vmpack.h"
#include "vmdatasrc.h"
#include "vmpat.h"
#include "vmregex.h"
#include "vmbiftad.h"
#include "vmfindrep.h"


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
    &CVmObjString::getp_undef,                                         /* 0 */
    &CVmObjString::getp_len,                                           /* 1 */
    &CVmObjString::getp_substr,                                        /* 2 */
    &CVmObjString::getp_upper,                                         /* 3 */
    &CVmObjString::getp_lower,                                         /* 4 */
    &CVmObjString::getp_find,                                          /* 5 */
    &CVmObjString::getp_to_uni,                                        /* 6 */
    &CVmObjString::getp_htmlify,                                       /* 7 */
    &CVmObjString::getp_starts_with,                                   /* 8 */
    &CVmObjString::getp_ends_with,                                     /* 9 */
    &CVmObjString::getp_to_byte_array,                                /* 10 */
    &CVmObjString::getp_replace,                                      /* 11 */
    &CVmObjString::getp_splice,                                       /* 12 */
    &CVmObjString::getp_split,                                        /* 13 */
    &CVmObjString::getp_specialsToHtml,                               /* 14 */
    &CVmObjString::getp_specialsToText,                               /* 15 */
    &CVmObjString::getp_urlEncode,                                    /* 16 */
    &CVmObjString::getp_urlDecode,                                    /* 17 */
    &CVmObjString::getp_sha256,                                       /* 18 */
    &CVmObjString::getp_md5,                                          /* 19 */
    &CVmObjString::getp_packBytes,                                    /* 20 */
    &CVmObjString::getp_unpackBytes,                                  /* 21 */
    &CVmObjString::getp_toTitleCase,                                  /* 22 */
    &CVmObjString::getp_toFoldedCase,                                 /* 23 */
    &CVmObjString::getp_compareTo,                                    /* 24 */
    &CVmObjString::getp_compareIgnoreCase,                            /* 25 */
    &CVmObjString::getp_findLast,                                     /* 26 */
    &CVmObjString::getp_findAll,                                      /* 27 */
    &CVmObjString::getp_match                                         /* 28 */
};

/* static property indices */
const int PROPIDX_packBytes = 20;

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

/* create from a wide-character string */
vm_obj_id_t CVmObjString::create(VMG_ int in_root_set,
                                 const wchar_t *str, size_t charlen)
{
    /* get the utf8 byte length of the string */
    size_t i, bytelen;
    const wchar_t *src;
    for (src = str, i = 0, bytelen = 0 ; i < charlen ; ++src, ++i)
        bytelen += utf8_ptr::s_wchar_size(*src);

    /* allocate space */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjString(vmg_ bytelen);
    CVmObjString *s = (CVmObjString *)vm_objp(vmg_ id);

    /* build the string */
    utf8_ptr dst(s->cons_get_buf());
    for (src = str, i = 0 ; i < charlen ; ++src, ++i)
        dst.setch(*src);

    /* return the new string object ID */
    return id;
}

/* create from the given character set */
vm_obj_id_t CVmObjString::create(VMG_ int in_root_set,
                                 const char *src, size_t srclen,
                                 CCharmapToUni *cmap)
{
    /* figure the size needed for the conversion */
    size_t ulen = cmap->map_str(0, 0, src, srclen);

    /* create a string of the mapped length */
    vm_obj_id_t ret = create(vmg_ in_root_set, ulen);
    CVmObjString *retstr = (CVmObjString *)vm_objp(vmg_ ret);

    /* map into the string buffer */
    cmap->map_str(retstr->cons_get_buf(), ulen, src, srclen);

    /* return the new string */
    return ret;
}

/* create from a byte stream, interpreting the bytes as Latin-1 characters */
vm_obj_id_t CVmObjString::create_latin1(VMG_ int in_root_set,
                                        CVmDataSource *src)
{
    /* scan the memory stream to determine the string byte length */
    long len = copy_latin1_to_string(0, 0, src);

    /* make sure the length is within the acceptable range */
    if (len > 65535)
        err_throw(VMERR_STR_TOO_LONG);

    /* allocate the string object */
    vm_obj_id_t id = create(vmg_ FALSE, len);
    CVmObjString *str = (CVmObjString *)vm_objp(vmg_ id);

    /* copy the stream to the string */
    copy_latin1_to_string(str->cons_get_buf(), len, src);

    /* return the new object ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Copy a byte stream to a string, treating each byte of the source stream
 *   as a Latin-1 character.
 *   
 *   If the output buffer is null or is too short, we'll just count up the
 *   length without storing anything.  Returns the total number of bytes
 *   needed to store the full source byte array.  
 */
size_t CVmObjString::copy_latin1_to_string(char *str, size_t len,
                                           CVmDataSource *src)
{
    /* we haven't copied any bytes out yet */
    size_t tot = 0;

    /* seek to the start of the source */
    src->seek(0, OSFSK_SET);

    /* keep going until we reach EOF */
    for (;;)
    {
        /* read the next chunk of bytes from the source */
        char buf[1024];
        int cur = src->readc(buf, sizeof(buf));

        /* if there's nothing left, we're done */
        if (cur == 0)
            return tot;

        /* scan the buffer */
        for (char *p = buf ; cur != 0 ; ++p, --cur)
        {
            /* 
             *   if this character is from 0 to 127, store one byte;
             *   otherwise store it as two bytes in UTF-8 format 
             */
            unsigned char c = *(unsigned char *)p;
            if (c <= 127)
            {
                /* 0..127 -> single byte UTF-8 character */
                tot += 1;
                if (str != 0 && tot <= len)
                    *str++ = c;
            }
            else
            {
                /* 128.255 -> two-byte UTF-8 character */
                tot += 2;
                if (str != 0 && tot <= len)
                {
                    *str++ = (char)(0xC0 | ((c >> 6) & 0x1F));
                    *str++ = (char)(0x80 | (c & 0x3F));
                }
            }
        }
    }
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
 *   Ensure space for construction 
 */
char *CVmObjString::cons_ensure_space(VMG_ char *ptr, size_t len,
                                      size_t margin)
{
    /* figure the offset of the pointer into the current buffer */
    int ofs = ptr - (ext_ + VMB_LEN);

    /* calculate the needed size: this is the offset plus the added length */
    size_t need = ofs + len;

    /* if we already have enough space, we're done */
    if (vmb_get_len(ext_) >= need)
        return ptr;

    /* expand to the needed size plus the margin */
    size_t newlen = need + margin;
    ext_ = (char *)G_mem->get_var_heap()->realloc_mem(
        newlen + VMB_LEN, ext_, this);

    /* set the new buffer size */
    vmb_put_len(ext_, newlen);

    /* return the pointer to the new buffer at the original offset */
    return ext_ + VMB_LEN + ofs;
}

/*
 *   Append a string during construction 
 */
char *CVmObjString::cons_append(VMG_ char *ptr,
                                const char *addstr, size_t addlen,
                                size_t margin)
{
    /* ensure we have space for the added string */
    ptr = cons_ensure_space(vmg_ ptr, addlen, margin);

    /* add the new string onto the end of the buffer */
    memcpy(ptr, addstr, addlen);

    /* return a pointer to the end of the added data */
    return ptr + addlen;
}

/*
 *   Append a character during construction 
 */
char *CVmObjString::cons_append(VMG_ char *ptr, wchar_t ch, size_t margin)
{
    /* encode the character as utf8 */
    char buf[3];
    size_t len = utf8_ptr::s_putch(buf, ch);

    /* append the utf8 string for the character */
    return cons_append(vmg_ ptr, buf, len, margin);
}

/*
 *   Shrink the buffer to the actual final size 
 */
void CVmObjString::cons_shrink_buffer(VMG_ char *ptr)
{
    /* figure the offset of the pointer into the current buffer */
    size_t siz = (size_t)(ptr - (ext_ + VMB_LEN));

    /* 
     *   If the savings are substantial enough, reallocate.  If the size is
     *   unchanged, or it's within a reasonable margin of the target size, do
     *   nothing.  Reallocation isn't free (we have to find memory and make a
     *   copy of the current buffer), so it's more efficient to waste a
     *   little memory if the savings are substantial.  
     */
    if (vmb_get_len(ext_) - siz >= 256)
    {
        /* reallocate at the new size */
        ext_ = (char *)G_mem->get_var_heap()->realloc_mem(
            siz + VMB_LEN, ext_, this);
    }

    /* write the final length */
    vmb_put_len(ext_, siz);
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
 *   call a static property 
 */
int CVmObjString::call_stat_prop(VMG_ vm_val_t *result,
                                 const uchar **pc_ptr, uint *argc,
                                 vm_prop_id_t prop)
{
    /* get the function table index */
    int idx = G_meta_table->prop_to_vector_idx(
        metaclass_reg_->get_reg_idx(), prop);

    /* check for static methods */
    switch (idx)
    {
    case PROPIDX_packBytes:
        return static_getp_packBytes(vmg_ result, argc);

    default:
        /* inherit the default handling */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
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
    /* get our length */
    size_t len = vmb_get_len(ext_);

    /* write the length prefix and the string */
    fp->write_bytes(ext_, len + VMB_LEN);
}

/*
 *   Restore the object from a file 
 */
void CVmObjString::restore_from_file(VMG_ vm_obj_id_t,
                                     CVmFile *fp, CVmObjFixup *)
{
    /* read the length prefix */
    size_t len = fp->read_uint2();

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
 *   Cast to integer.  We parse the string as a numeric value, using the same
 *   rules as toInteger(). 
 */
long CVmObjString::cast_to_int(VMG0_) const
{
    /* 
     *   parse the string as an integer in decimal format; this is an
     *   explicit int cast, so don't allow BigNumber promotions 
     */
    vm_val_t val;
    parse_num_val(vmg_ &val, ext_ + VMB_LEN, vmb_get_len(ext_), 10, TRUE);

    /* return the integer value */
    return val.val.intval;
}

/*
 *   Cast to number. 
 */
void CVmObjString::cast_to_num(VMG_ vm_val_t *val, vm_obj_id_t self) const
{
    /* 
     *   Parse the string as a number in decimal format.  We're allowed to
     *   return whatever numeric type is needed to represent the value, so
     *   allow BigNumber promotions if necessary. 
     */
    parse_num_val(vmg_ val, ext_ + VMB_LEN, vmb_get_len(ext_), 10, FALSE);
}

/*
 *   Parse a string as an integer value 
 */
void CVmObjString::parse_num_val(VMG_ vm_val_t *retval,
                                 const char *str, size_t len,
                                 int radix, int int_only)
{
    utf8_ptr p;
    size_t rem;

    /* skip leading spaces */
    for (p.set((char *)str), rem = len ;
         rem != 0 && t3_is_whitespace(p.getch()) ; p.inc(&rem)) ;
    
    /* presume it's positive */
    int is_neg = FALSE;
    
    /* check for a leading + or - */
    if (rem != 0)
    {
        if (p.getch() == '-')
        {
            /* note the sign and skip the character */
            is_neg = TRUE;
            p.inc(&rem);
        }
        else if (p.getch() == '+')
        {
            /* skip the character */
            p.inc(&rem);
        }
    }

    /* skip any additional spaces after the sign */
    for ( ; rem != 0 && t3_is_whitespace(p.getch()) ; p.inc(&rem)) ;
    
    /* clear the accumulator */
    unsigned long acc = 0;
    
    /* scan the digits */
    for ( ; rem != 0 ; p.inc(&rem))
    {
        /* get the next digit */
        wchar_t ch = p.getch();

        /* 
         *   If we're allowed to promote to BigNumber, and the radix is
         *   decimal, check for '.' and 'E' (for exponent) notation.  These
         *   could indicate a floating point value.  
         */
        if (radix == 10 && !int_only && (ch == '.' || ch == 'e' || ch == 'E'))
        {
            /* presume we're going to promote to BigNumber */
            int promote = TRUE;
            
            /* 
             *   If it's an exponent, parse further to make sure it looks
             *   like a valid exponent: we can have an optional + or - sign,
             *   then need at least one digit.  
             */
            if (ch == 'e' || ch == 'E')
            {
                /* set up a second pointer */
                utf8_ptr p2(p);
                size_t rem2 = rem;

                /* skip the 'e' */
                p2.inc(&rem);

                /* if there's a sign, skip it */
                if (rem2 != 0 && (p2.getch() == '+' || p2.getch() == '-'))
                    p2.inc(&rem2);

                /* we need a digit at this point, or it's not an exponent */
                promote = (rem2 != 0 && is_digit(p2.getch()));
            }

            /* if we're promoting after all, go do the promotion */
            if (promote)
            {
                /* re-parse it as a BigNumber value */
                retval->set_obj(CVmObjBigNum::create_radix(
                    vmg_ FALSE, str, len, radix));

                /* return this value */
                return;
            }
        }

        /* get the integer value of the digit, treating A-Z as digits 10-35 */
        int dig;
        if (ch >= '0' && ch <= '9')
            dig = (int)(ch - '0');
        else if (ch >= 'a' && ch <= 'z')
            dig = (int)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'Z')
            dig = (int)(ch - 'A' + 10);
        else
            break;

        /* if it's outside the radix limit, it's not a digit after all */
        if (dig >= radix)
            break;

        /* 
         *   Make sure we're not going to overflow.  If the base is 2, 8, or
         *   16, and there wasn't a "-" sign, allow the value to go up to the
         *   unsigned 32-bit limit, 0xffffffff.  Otherwise, limit it to
         *   32-bit signed, -0x8000000 to +0x7ffffff.  
         */
        unsigned long limit =
            (!is_neg && (radix == 2 || radix == 8 || radix == 16))
            ? 0xffffffff
            : is_neg ? 0x80000000 : 0x7fffffff;
        
        /* shift the accumulator by the radix, checking for overflow */
        if (acc > limit/radix)
            goto overflow;
        acc *= radix;

        /* add the digit, checking for overflow */
        if (acc > limit - dig)
            goto overflow;
        acc += dig;
    }
    
    /* apply the sign, if appropriate, and set the return value */
    retval->set_int(is_neg && acc <= 0x7FFFFFFF ? -(long)acc : (long)acc);
    return;

overflow:
    /*
     *   We overflowed the plain integer type.  If we're allowed to promote
     *   to a BigNumber, re-parse the value as a BigNumber.  If not, it's an
     *   error.
     */
    if (int_only)
    {
        /* we can't promote to BigNumber - throw an overflow error */
        err_throw(VMERR_NUM_OVERFLOW);
    }
    else
    {
        /* re-parse it as a BigNumber value */
        retval->set_obj(CVmObjBigNum::create_radix(
            vmg_ FALSE, str, len, radix));
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a value to this string 
 */
int CVmObjString::add_val(VMG_ vm_val_t *result,
                          vm_obj_id_t self, const vm_val_t *val)
{
    /* set up a 'self' value holder */
    vm_val_t selfval;
    selfval.set_obj(self);

    /* 
     *   Use the generic string adder, using my extension as the constant
     *   string.  We store our extension in the general string format
     *   required by the static adder. 
     */
    add_to_str(vmg_ result, &selfval, val);

    /* handled */
    return TRUE;
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
                              const vm_val_t *self, const vm_val_t *val)
{
    const char *strval1, *strval2;
    char buf[128];
    vm_obj_id_t obj;
    size_t len1, len2;
    CVmObjString *objptr;
    vm_val_t new_obj2;

    /* 
     *   Get the string buffer pointers.  The left value is already a string,
     *   or we wouldn't be here.  The right value can be anything, though, so
     *   we need to apply an implicit string conversion if it's another type.
     */
    strval1 = self->get_as_string(vmg0_);
    strval2 = cvt_to_str(vmg_ &new_obj2, buf, sizeof(buf), val, 10, 0);

    /* get the lengths of the two strings */
    len1 = vmb_get_len(strval1);
    len2 = vmb_get_len(strval2);

    /*
     *   If the right-hand value is zero length, or it's nil, simply return
     *   the left-hand value.  If the left-hand value is zero length AND the
     *   right-hand value is already a string (constant or object), return
     *   the right-hand value unchanged.  Otherwise, we actually need to
     *   build a whole new string with the concatenated texts.  
     */
    if (len2 == 0 || val->typ == VM_NIL)
    {
        /* we're appending nothing to the string; just return 'self' */
        *result = *self;
    }
    else if (len1 == 0 && val->get_as_string(vmg0_) != 0)
    {
        /* 
         *   we're appending the right value to an empty string, AND the
         *   right value is already a string itself, so we can simply return
         *   the right value unchanged 
         */
        *result = *val;
    }
    else
    {
        /* 
         *   push the new string (if any) and self, to protect the two
         *   strings from garbage collection 
         */
        G_stk->push(self);
        G_stk->push(&new_obj2);

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
    /* if the provided buffer is large enough, use it */
    if (buf != 0 && buf_size >= required_size)
    {
        /* there's no new object */
        new_obj->set_nil();
        
        /* return the buffer */
        return buf;
    }

    /* allocate a new string object */
    new_obj->set_obj(CVmObjString::create(vmg_ FALSE, required_size));

    /* return the new object's full string buffer from the length prefix */
    return ((CVmObjString *)vm_objp(vmg_ new_obj->val.obj))->ext_;
}


/* ------------------------------------------------------------------------ */
/*
 *   Try converting a value to a string via reflection services, imported
 *   from the bytecode program.  Failing that, use a boilerplate format.
 */
const char *CVmObjString::reflect_to_str(
    VMG_ vm_val_t *new_str, char *result_buf, size_t result_buf_size,
    const vm_val_t *val, const char *fmt, ...)
{
    /* look for an exported reflectionServices object from the byteocde */
    vm_val_t refl;
    refl.set_obj(G_predef->reflection_services);
    vm_prop_id_t valToSym = G_predef->reflection_valToSymbol;
    if (refl.val.obj != VM_INVALID_OBJ && valToSym != VM_INVALID_PROP)
    {
        /* invoke reflectionServices.valToSymbol(val) */
        G_stk->push(val);
        G_interpreter->get_prop(vmg_ 0, &refl, valToSym, &refl, 1, 0);

        /* if we got a string, return it */
        const char *str = G_interpreter->get_r0()->get_as_string(vmg0_);
        if (str != 0)
        {
            /* set the return value */
            *new_str = *G_interpreter->get_r0();
            
            /* return the string buffer */
            return str;
        }
    }

    /* 
     *   No reflection services - fall back on the boilerplate format.  Start
     *   by calculating the amount of space we need for the formatted string,
     *   adding a byte for the trailing null.
     */
    va_list args;
    va_start(args, fmt);
    size_t need = t3vsprintf(0, 0, fmt, args) + 1;
    va_end(args);

    /* allocate space, including room for the length prefix */
    result_buf = alloc_str_buf(
        vmg_ new_str, result_buf, result_buf_size, need + VMB_LEN);

    /* do the formatting */
    va_start(args, fmt);
    t3vsprintf(result_buf + VMB_LEN, need, fmt, args);
    va_end(args);

    /* set the length prefix */
    vmb_put_len(result_buf, need);

    /* return the buffer */
    return result_buf;
}


/* ------------------------------------------------------------------------ */
/*
 *   Convert a value to a string 
 */
const char *CVmObjString::cvt_to_str(VMG_ vm_val_t *new_str,
                                     char *result_buf,
                                     size_t result_buf_size,
                                     const vm_val_t *val,
                                     int radix, int flags)
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
        return vm_objp(vmg_ val->val.obj)->explicit_to_string(
            vmg_ val->val.obj, new_str, radix, flags);

    case VM_INT:
        /* 
         *   It's a number - convert it to a string.  Use the provided result
         *   buffer if possible, but make sure we have room for the number.
         *   The unicode values we're storing are in the ascii range, so we
         *   only need one byte per character.  The longest buffer we'd need,
         *   then, is 33 bytes, for a conversion to a bit string (i.e., base
         *   2: 32 1s or 0s, plus a sign).  The conversion also needs two
         *   bytes for the length prefix; add a few extra bytes as insurance
         *   against future algorithm changes that might need more padding.  
         */
        result_buf = alloc_str_buf(vmg_ new_str,
                                   result_buf, result_buf_size, 40);

        /* generate the string */
        return cvt_int_to_str(result_buf, 40, val->val.intval, radix, flags);

    case VM_NIL:
        /* nil - use the literal string "nil" */
        return "\003\000nil";

    case VM_TRUE:
        /* true - use the literal string "true" */
        return "\004\000true";

    case VM_LIST:
        return CVmObjList::list_to_string(vmg_ new_str, val, radix, flags);

    case VM_PROP:
        return reflect_to_str(vmg_ new_str, result_buf, result_buf_size, val,
                              "property#%d", (int)val->val.prop);

    case VM_FUNCPTR:
        return reflect_to_str(vmg_ new_str, result_buf, result_buf_size, val,
                              "function#%lx", (long)val->val.ofs);

    case VM_ENUM:
        return reflect_to_str(vmg_ new_str, result_buf, result_buf_size, val,
                              "enum#%ld", (long)val->val.enumval);

    case VM_BIFPTR:
        return reflect_to_str(vmg_ new_str, result_buf, result_buf_size, val,
                              "builtin#%d.%d",
                              (int)val->val.bifptr.set_idx,
                              (int)val->val.bifptr.func_idx);

    default:
        /* other types cannot be added to a string */
        err_throw(VMERR_NO_STR_CONV);

        /* we never really get here, but the compiler doesn't know that */
        AFTER_ERR_THROW(return 0;)
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert an integer to a string, storing the result in the given buffer
 *   in portable string format (with length prefix).  We accept any radix
 *   from 2 to 36 (36 can be represented with "digits" 0-9A-Z).
 *   
 *   If the value is signed, a leading dash is included if the number is
 *   negative.  Otherwise we treat the original value as unsigned, which on
 *   most hardware means we'll show the two's complement value.
 *   
 *   For efficiency, we store the number at the end of the buffer (this makes
 *   it easy to generate the number, since we need to generate numerals in
 *   reverse order).  We return a pointer to the result, which may not start
 *   at the beginning of the buffer.  
 */
char *CVmObjString::cvt_int_to_str(char *buf, size_t buflen,
                                   int32_t inval, int radix, int flags)
{
    int neg;
    uint32_t val;
    char *p;
    size_t len;

    /* start at the end of the buffer */
    p = buf + buflen;

    /* 
     *   if it's negative, and we're converting to a signed representation,
     *   use a leading minus sign plus the absolute value; otherwise, treat
     *   the value as unsigned 
     */
    if (inval >= 0 || (flags & TOSTR_UNSIGNED) != 0)
    {
        /* 
         *   the value is non-negative, either because the signed
         *   interpretation is non-negative or because we're using an
         *   unsigned interpretation (in which case there's no such thing as
         *   negative) 
         */
        neg = FALSE;

        /* use the value as-is */
        val = (uint32_t)inval;
    }
    else
    {   
        /* it's signed and negative - note that we need a minus sign */
        neg = TRUE;

        /* use the positive value for the conversion */
        val = (uint32_t)(-inval);
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

    /* if the other value is a string buffer, compare to its contents */
    if (val->typ == VM_OBJ
        && CVmObjStringBuffer::is_string_buffer_obj(vmg_ val->val.obj))
        return ((CVmObjStringBuffer *)vm_objp(vmg_ val->val.obj))
            ->equals_str(str);

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

    /* if the other value is a string buffer, compare to its contents */
    if (val->typ == VM_OBJ
        && CVmObjStringBuffer::is_string_buffer_obj(vmg_ val->val.obj))
    {
        /* 
         *   ask the string buffer to do the comparison, but reverse the
         *   sense of the comparison, since from its perspective the
         *   comparison is in the opposite order 
         */
        return -((CVmObjStringBuffer *)vm_objp(vmg_ val->val.obj))
            ->compare_str(str1);
    }

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
const char *CVmObjString::find_substr(VMG_ const char *str, int32_t start_idx,
                                      const char *substr, size_t *idxp)
{
    /* get the lengths */
    size_t rem = vmb_get_len(str);
    size_t sublen = vmb_get_len(substr);

    /* set up utf8 pointer into the string */
    utf8_ptr p((char *)str + 2);

    /* if the index is negative, it's from the end of the string */
    start_idx += (start_idx < 0 ? (int)p.len(rem) : -1);

    /* skip to the starting index */
    for (int32_t i = start_idx ; i > 0 && rem >= sublen ; --i, p.inc(&rem)) ;

    /* scan for the substring */
    for (size_t char_ofs = 0 ; rem != 0 && rem >= sublen ;
         ++char_ofs, p.inc(&rem))
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

/*
 *   Find a substring or pattern.  'basestr' is the entire string, and 'str'
 *   is a pointer into the string giving the starting position for the
 *   search; this can be the same as 'baseptr' to search from the very
 *   beginning of the string, or to a character in the middle of the string.
 *   
 *   The returned 'match_idx' value is the character index relative to the
 *   starting search position 'str'.  If the match starts at the beginning of
 *   'str' then 'match_idx' is returned as 0.  Note that this is relative to
 *   the starting point 'str' rather than the overall string 'basestr'.
 */
const char *CVmObjString::find_substr(
    VMG_ const vm_val_t *strval,
    const char *basestr, const char *str, size_t len,
    const char *substr, CVmObjPattern *pat, int *match_idx, int *match_len)
{
    /* search for the string or pattern */
    if (substr != 0)
    {
        /* get the substring length and buffer pointer */
        size_t sublen = vmb_get_len(substr);
        substr += VMB_LEN;

        /* search for the substring */
        utf8_ptr p((char *)str);
        for (int i = 0 ; len >= sublen ; p.inc(&len), ++i)
        {
            /* check for a match */
            if (memcmp(p.getptr(), substr, sublen) == 0)
            {
                /* got it - the match length is simply the substring length */
                *match_idx = i;
                *match_len = sublen;
                return p.getptr();
            }
        }
    }
    else if (pat != 0)
    {
        /* get the compiled pattern */
        re_compiled_pattern *cpat = pat->get_pattern(vmg0_);

        /* save the last search source string */
        G_bif_tads_globals->last_rex_str->val = *strval;
        G_bif_tads_globals->rex_searcher->clear_group_regs();

        /* search for the pattern */
        int idx = G_bif_tads_globals->rex_searcher->search_for_pattern(
            cpat, basestr, str, len, match_len);

        /* if we found the match, return it */
        if (idx >= 0)
        {
            /* set the match index and length */
            *match_idx = utf8_ptr::s_len(str, idx);
            return str + idx;
        }
    }

    /* no match */
    *match_idx = -1;
    *match_len = 0;
    return 0;
}

/*
 *   Find a substring, searching from the end of the string toward the
 *   beginning.  This works like find_substr(), but searchs backwards.  The
 *   match must end before the starting position 'str' - it can't overlap
 *   this character.  To search the entire string from the end, set 'str' to
 *   the byte just after the last character of the string.
 *   
 *   As with find_substr(), we return with 'match_idx' set to the character
 *   index of the start of the match relative to 'str'.  This means that
 *   'match_idx' will be zero or negative on a successful match.  (And the
 *   only way it returns with zero is when the matched text is zero length,
 *   because of the requirement that the match can't overlap the starting
 *   character.)
 */
const char *CVmObjString::find_last_substr(
    VMG_ const vm_val_t *strval,
    const char *basestr, const char *str, size_t len,
    const char *substr, CVmObjPattern *pat, int *match_idx, int *match_len)
{
    /* search for the string or pattern */
    if (substr != 0)
    {
        /* get the substring length and buffer pointer */
        size_t sublen = vmb_get_len(substr);
        substr += VMB_LEN;

        /* get the character length of the search string */
        size_t subclen = utf8_ptr::s_len(substr, sublen);

        /* set up at the starting position */
        utf8_ptr p((char *)str);

        /* 
         *   back up by the number of characters 'c' in the search string -
         *   the match can't include the from_idx position, so we can't match
         *   anything later in the string than 'c' characters before from_idx
         */
        size_t c;
        for (c = subclen ; c != 0 && p.getptr() != basestr ; --c, p.dec()) ;

        /* if we couldn't back up far enough, there's no chance of a match */
        if (c > 0)

        {
            *match_idx = -1;
            *match_len = 0;
            return 0;
        }

        /* search backwards for the string */
        for (int i = -(int)subclen ; ; p.dec(), --i)
        {
            /* check for a match at the current position */
            if (memcmp(p.getptr(), substr, sublen) == 0)
            {
                /* 
                 *   got it - the match index is the negative of the
                 *   character offset from the match position to the search
                 *   starting position, and the match length is simply
                 *   substring byte length 
                 */
                *match_idx = -(int)utf8_ptr::s_len(
                    p.getptr(), str - p.getptr());
                *match_len = sublen;
                return p.getptr();
            }

            /* if we're at the start of the base string, we're done */
            if (p.getptr() == basestr)
                break;
        }
    }
    else if (pat != 0)
    {
        /* get the compiled pattern */
        re_compiled_pattern *cpat = pat->get_pattern(vmg0_);

        /* save the last search source string */
        G_bif_tads_globals->last_rex_str->val = *strval;
        G_bif_tads_globals->rex_searcher->clear_group_regs();

        /* search for the pattern */
        int idx = G_bif_tads_globals->rex_searcher->search_back_for_pattern(
            cpat, basestr, str, len, match_len);

        /* if we found the match, return it */
        if (idx >= 0)
        {
            /* set the match index and length */
            *match_idx = -(int)utf8_ptr::s_len(str - idx, idx);
            return str + idx;
        }
    }

    /* no match */
    *match_idx = -1;
    *match_len = 0;
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
    /* presume no source object */
    *src_obj = VM_INVALID_OBJ;

    /* translate the property index to an index into our function table */
    uint func_idx = G_meta_table
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
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set up a utf-8 pointer to the string's contents */
    utf8_ptr p;
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
    long len = 0;
    size_t rem;
    utf8_ptr p;
    utf8_ptr start_p;
    size_t start_rem;
    size_t new_len;
    vm_obj_id_t obj;

    /* check arguments */
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    static CVmNativeCodeDesc desc(1, 1);
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
        /* figure the positive or negative length */
        if (len >= 0)
        {
            /* 
             *   Positive length - this specifies the number of characters to
             *   keep starting at the starting index.  Skip ahead by the
             *   desired length to figure the end pointer. 
             */
            for ( ; len > 0 && rem != 0 ; --len)
                p.inc(&rem);
        }
        else
        {
            /*
             *   Negative length - this specifies the number of characters to
             *   remove from the end of the string.  Move to the end of the
             *   string, then skip back by |len| characters to find the end
             *   pointer.  
             */
            for (p.set(p.getptr() + rem), rem = 0 ;
                 len < 0 && rem < start_rem ; ++len)
                 p.dec(&rem);
        }

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
    return gen_getp_case_conv(vmg_ retval, self_val, str, argc, &t3_to_upper);
}

/*
 *   property evaluator - toLower
 */
int CVmObjString::getp_lower(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *str, uint *argc)
{
    return gen_getp_case_conv(vmg_ retval, self_val, str, argc, &t3_to_lower);
}

/*
 *   property evaluator - toTitleCase
 */
int CVmObjString::getp_toTitleCase(
    VMG_ vm_val_t *retval, const vm_val_t *self_val,
    const char *str, uint *argc)
{
    return gen_getp_case_conv(vmg_ retval, self_val, str, argc, &t3_to_title);
}

/*
 *   property evaluator - toFoldedCase
 */
int CVmObjString::getp_toFoldedCase(
    VMG_ vm_val_t *retval, const vm_val_t *self_val,
    const char *str, uint *argc)
{
    return gen_getp_case_conv(vmg_ retval, self_val, str, argc, &t3_to_fold);
}

/*
 *   General case converter for toUpper, toLower, toTitleCase, toFoldedCase 
 */
int CVmObjString::gen_getp_case_conv(
    VMG_ vm_val_t *retval, const vm_val_t *self_val,
    const char *str, uint *argc, const wchar_t *(*conv)(wchar_t))
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get my length */
    size_t srclen = vmb_get_len(str);

    /* leave the string on the stack as GC protection */
    G_stk->push(self_val);

    /* 
     *   Scan the string to determine how long the result will be.  The
     *   result won't necessarily be the same length as the original: some
     *   case conversions are 1:N characters, and even when they're 1:1, a
     *   two-byte character in the original could turn into a three-byte
     *   character in the result, and vice versa.
     */
    size_t dstlen;
    utf8_ptr srcp, dstp;
    size_t rem;
    for (dstlen = 0, srcp.set((char *)str + VMB_LEN), rem = srclen ;
         rem != 0 ; srcp.inc(&rem))
    {
        /* get the mapping for this character */
        wchar_t ch = srcp.getch();
        const wchar_t *u = conv(ch);

        /* add up the byte length of the mapping */
        dstlen += (u != 0
                   ? utf8_ptr::s_wstr_size(u)
                   : utf8_ptr::s_wchar_size(ch));
    }

    /* allocate the result string */
    vm_obj_id_t result_obj = CVmObjString::create(vmg_ FALSE, dstlen);

    /* get a pointer to the result buffer */
    dstp.set(((CVmObjString *)vm_objp(vmg_ result_obj))->cons_get_buf());

    /* write the string */
    for (srcp.set((char *)str + VMB_LEN), rem = srclen ;
         rem != 0 ; srcp.inc(&rem))
    {
        /* get the mapping for this character */
        wchar_t ch = srcp.getch();
        const wchar_t *u = conv(ch);

        /* put the character(s) */
        if (u != 0)
            dstlen -= dstp.setwcharsz(u, dstlen);
        else
            dstp.setch(ch, &dstlen);
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
 *   Common handler for find() and findLast() methods.  'dir' specifies the
 *   search direction: 1 for forward (left to right), -1 for reverse (right
 *   to left).
 */
template<int dir> inline int CVmObjString::find_common(
    VMG_ vm_val_t *retval,
    const vm_val_t *self_val, const char *str,
    uint *argc)
{
    /* check arguments */
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the string length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* remember where the base string strings */
    const char *basestr = str;

    /* retrieve the string or regex pattern to find */
    vm_val_t *val2 = G_stk->get(0);
    const char *str2 = val2->get_as_string(vmg0_);
    CVmObjPattern *pat2 = 0;
    if (str2 != 0)
    {
        /* we have a simple string to find */
    }
    else if (val2->typ == VM_OBJ
             && CVmObjPattern::is_pattern_obj(vmg_ val2->val.obj))
    {
        /* it's a pattern */
        pat2 = (CVmObjPattern *)vm_objp(vmg_ val2->val.obj);
    }
    else
    {
        /* we need a string or a pattern; other values are invalid */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* if there's a starting index, skip that many characters */
    int32_t start_idx = 0;
    if (orig_argc >= 2 && G_stk->get(1)->typ != VM_NIL)
    {
        /* get the starting index value */
        start_idx = G_stk->get(1)->num_to_int(vmg0_);

        /* set up a UTF-8 pointer for traversing the string */
        utf8_ptr strp((char *)str);

        /*
         *   A positive index value is a 1-based index from the start of the
         *   string.  A negative index is from the end of the string, with -1
         *   pointing to the last character.  For reverse searches, 0 is the
         *   position past the last character.  In any case, adjust to a
         *   zero-based index.
         */
        start_idx += (start_idx == 0 && dir < 0 ? (int)strp.len(len) :
                      start_idx < 0 ? (int)strp.len(len) :
                      -1);

        /* if there's a substring, we can limit the skip */
        size_t min_len = (dir > 0 && str2 != 0 ? vmb_get_len(str2) : 0);

        /* skip that many characters */
        int32_t i;
        for (i = 0 ; i < start_idx && len > min_len ; ++i, strp.inc(&len)) ;

        /* 
         *   if the start index was past the end of the string (or past the
         *   point where the remaining subject string is too short for the
         *   target string), we definitely can't match 
         */
        if (i < start_idx)
        {
            retval->set_nil();
            goto done;
        }

        /* start the search here */
        str = strp.getptr();
    }
    else if (dir < 0)
    {
        /* 
         *   we're searching backwards, and there's no starting index, so the
         *   starting point is the end of the string 
         */
        str = str + len;
        start_idx = utf8_ptr::s_len(basestr, len);
        len = 0;
    }

    /* find the substring */
    int match_idx, match_len;
    if (dir > 0
        ? find_substr(vmg_ self_val, basestr, str, len, str2, pat2,
                      &match_idx, &match_len) != 0
        : find_last_substr(vmg_ self_val, basestr, str, len, str2, pat2,
                           &match_idx, &match_len) != 0)
    {
        /* found it - adjust to a 1-based value for return */
        retval->set_int(start_idx + match_idx + 1);
    }
    else
    {
        /* didn't find it - return nil */
        retval->set_nil();
    }

done:
    /* discard arguments */
    G_stk->discard(orig_argc);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - find
 */
int CVmObjString::getp_find(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val, const char *str,
                            uint *argc)
{
    return find_common<1>(vmg_ retval, self_val, str, argc);
}

/*
 *   property evaluator - findLast 
 */
int CVmObjString::getp_findLast(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val, const char *str,
                                uint *argc)
{
    return find_common<-1>(vmg_ retval, self_val, str, argc);
}


/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - findAll 
 */
int CVmObjString::getp_findAll(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val, const char *str,
                               uint *argc)
{
    /* check arguments */
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the string length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* remember the base string */
    const char *basestr = str;

    /* retrieve the string or regex pattern to find */
    vm_val_t *targ = G_stk->get(0);
    const char *targstr = 0;
    CVmObjPattern *targpat = 0;
    if ((targstr = targ->get_as_string(vmg0_)) != 0)
    {
        /* we have a simple string to find */
    }
    else if ((targpat = vm_val_cast(CVmObjPattern, targ)) != 0)
    {
        /* it's a pattern */
    }
    else
    {
        /* we need a string or a pattern; other values are invalid */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* retrieve the filter callback function, if present */
    vm_val_t *func = 0;
    int func_argc_min = 0, func_argc_max = 0;
    vm_rcdesc rc;
    if (orig_argc >= 2 && (func = G_stk->get(1))->typ != VM_NIL)
    {
        /* make sure it's a function */
        if (!func->is_func_ptr(vmg0_))
            err_throw(VMERR_BAD_TYPE_BIF);

        /* note how many arguments the function accepts */
        CVmFuncPtr fd(vmg_ func);
        func_argc_min = fd.get_min_argc();
        func_argc_max = fd.is_varargs() ? -1 : fd.get_max_argc();
    }

    /* 
     *   create a tentative list for the results (we'll resize this as needed
     *   as we add elements) 
     */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, 10));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* clear it initially and push it for gc protection */
    lst->cons_clear();
    G_stk->push(retval);

    /* repeatedly search for the pattern */
    int cnt;
    for (cnt = 0 ; len != 0 ; ++cnt)
    {
        /* search for the next element */
        int match_cofs, match_len, match_bofs;
        const char *nxt = find_substr(
            vmg_ self_val, basestr, str, len, targstr, targpat,
            &match_cofs, &match_len);

        /* if we didn't find a match, we're done */
        if (nxt == 0)
            break;

        /* figure the byte offset of the match */
        match_bofs = nxt - str;

        /* call the callback, or just return the match text directly */
        vm_val_t ele;
        if (func == 0)
        {
            /* no callback - just use the match text, as a new string */
            ele.set_obj(CVmObjString::create(vmg_ FALSE, nxt, match_len));
        }
        else
        {
            /* we have a callback - push arguments */
            int fargc = 0;

            /*
             *   Figure out how many groups are populated, and how many the
             *   function expects.  If the function takes varargs, it expects
             *   all of the groups.  If it takes fixed arguments, it expects
             *   at most max_argc-2.  In addition, the function expects a
             *   minimum of min_argc-2 groups, and expects us to add nil
             *   arguments for any unpopulated groups to fill out that
             *   minimum.
             */
            int gc_actual = G_bif_tads_globals->rex_searcher->get_group_cnt();
            int gc_wanted = (func_argc_max == -1
                             ? gc_actual : func_argc_max - 2);
            if (gc_wanted < func_argc_min - 2)
                gc_wanted = func_argc_min - 2;

            /* push the groups, last to first */
            for (int i = gc_wanted ; i > 0 ; )
            {
                /* get the group, if it exists */
                const re_group_register *reg = 0;
                if (--i < gc_actual)
                {
                    /* valid group - get the group information */
                    reg = G_bif_tads_globals->rex_searcher->get_group_reg(i);
                }

                /* if the group is populated, push its text, otherwise nil */
                if (reg != 0 && reg->start_ofs >= 0 && reg->end_ofs >= 0)
                {
                    /* push the group text as a new string */
                    G_stk->push()->set_obj(CVmObjString::create(
                        vmg_ FALSE, basestr + reg->start_ofs,
                        reg->end_ofs - reg->start_ofs));
                }
                else
                {
                    /* unpopulated group - push nil */
                    G_stk->push()->set_nil();
                }

                /* count it */
                ++fargc;
            }

            /* if the index value is desired, push it */
            if (func_argc_max == -1 || func_argc_max >= 2)
            {
                /* push the index of the match relative to basestr */
                G_stk->push()->set_int(
                    utf8_ptr::s_len(basestr, nxt - basestr) + 1);
                ++fargc;
            }

            /* if the match string value is desired, push it */
            if (func_argc_max == -1 || func_argc_max >= 1)
            {
                /* push the match text */
                G_stk->push()->set_obj(
                    CVmObjString::create(vmg_ FALSE, nxt, match_len));
                ++fargc;
            }

            /* call the callback */
            G_interpreter->call_func_ptr(vmg_ func, fargc, &rc, 0);

            /* retrieve the return value */
            ele = *G_interpreter->get_r0();
        }

        /* add it to the list */
        lst->cons_ensure_space(vmg_ cnt, 0);
        lst->cons_set_element(cnt, &ele);

        /* skip the matched text */
        size_t skip = match_bofs + match_len;
        str += skip;
        len -= skip;

        /* 
         *   if we matched zero characters, skip one character to prevent an
         *   infinite loop 
         */
        if (match_len == 0 && len != 0)
            str += utf8_ptr::s_bytelen(str, 1);
    }

    /* finalize the list length */
    lst->cons_set_len(cnt);

    /* discard arguments and gc protection */
    G_stk->discard(orig_argc + 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - match
 */
int CVmObjString::getp_match(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val, const char *str,
                             uint *argc)
{
    /* check arguments */
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the string length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* remember the base string */
    const char *basestr = str;
    
    /* retrieve the string or regex pattern to match */
    vm_val_t *targ = G_stk->get(0);
    const char *targstr = 0;
    CVmObjPattern *targpat = 0;
    size_t targstrlen = 0;
    if ((targstr = targ->get_as_string(vmg0_)) != 0)
    {
        /* it's a literal string - get its length and text pointer */
        targstrlen = vmb_get_len(targstr);
        targstr += VMB_LEN;
    }
    else if ((targpat = vm_val_cast(CVmObjPattern, targ)) != 0)
    {
        /* it's a pattern */
    }
    else
    {
        /* we need a string or a pattern; other values are invalid */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* retrieve the starting index, if present */
    int32_t start_idx = 0;
    if (orig_argc >= 2 && G_stk->get(1)->typ != VM_NIL)
    {
        /* get the starting index value */
        start_idx = G_stk->get(1)->num_to_int(vmg0_);

        /* set up a UTF-8 pointer for traversing the string */
        utf8_ptr strp((char *)str);

        /*
         *   A positive index value is a 1-based index from the start of the
         *   string.  A negative index is from the end of the string, with -1
         *   pointing to the last character. 
         */
        start_idx += (start_idx < 0 ? (int)strp.len(len) : -1);

        /* skip that many characters */
        int32_t i;
        for (i = 0 ; i < start_idx && len > targstrlen ; ++i, strp.inc(&len)) ;

        /* 
         *   if the start index was past the end of the string (or past the
         *   point where the remaining subject string is too short for the
         *   target string), we definitely can't match 
         */
        if (i < start_idx)
        {
            retval->set_nil();
            goto done;
        }

        /* start the search here */
        str = strp.getptr();
    }

    /* match the string or pattern */
    if (targstr != 0)
    {
        /* make sure it's long enough, then match the text literally */
        if (len >= targstrlen && memcmp(targstr, str, targstrlen) == 0)
        {
            /* matched - return the match length */
            retval->set_int(targstrlen);
        }
        else
        {
            /* no match */
            retval->set_nil();
        }
    }
    else
    {
        /* RexPattern - get the compiled pattern */
        re_compiled_pattern *cpat = targpat->get_pattern(vmg0_);

        /* save the last search source string */
        G_bif_tads_globals->last_rex_str->val = *self_val;
        G_bif_tads_globals->rex_searcher->clear_group_regs();

        /* match the pattern */
        int matchlen = G_bif_tads_globals->rex_searcher->match_pattern(
            cpat, basestr, str, len);

        /* if it matched (len >= 0), return the length, otherwise nil */
        if (matchlen >= 0)
            retval->set_int(matchlen);
        else
            retval->set_nil();
    }

done:
    /* discard arguments */
    G_stk->discard(orig_argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   findReplace() method - replace one or all occurrences of a given
 *   substring or regular expression pattern within the subject string (self)
 *   with a given replacement string.  This uses the common find/replace
 *   handler defined in vmfindrep.h.
 */
int CVmObjString::getp_replace(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *str, uint *argc)
{
    /* check arguments */
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(2, 3);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* do the search-and-replace */
    vm_find_replace<VMFINDREPLACE_StringFindReplace>(
        vmg_ retval, orig_argc, self_val, str);

    /* discard arguments */
    G_stk->discard(orig_argc);

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
    size_t bytelen;
    long idx = 1;
    utf8_ptr p;

    /* check arguments */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(0, 1);
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

    /* if the index is negative, it's an index from the end of the string */
    if (idx < 0)
        idx += p.len(bytelen) + 1;

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
    size_t bytelen;
    utf8_ptr p;
    utf8_ptr dstp;
    size_t rem;
    size_t extra;
    long flags;
    vm_obj_id_t result_obj;
    int prv_was_sp;

    /* check arguments */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(0, 1);
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

        case '>':
            /* replace '>' with '&gt;' */
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

        case '>':
            dstp.setch_str("&gt;");
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
    const char *str2;
    size_t len;
    size_t len2;

    /* check arguments */
    static CVmNativeCodeDesc desc(1);
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
    const char *str2;
    size_t len;
    size_t len2;

    /* check arguments */
    static CVmNativeCodeDesc desc(1);
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
                                     const char *str, uint *oargc)
{
    /* check arguments */
    int argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* note the mapper argument */
    const vm_val_t *cmap = (argc >= 1 ? G_stk->get(0) : 0);

    /* save the string for gc protection */
    G_stk->push(self_val);

    /* create the byte array */
    retval->set_obj(CVmObjByteArray::create_from_string(
        vmg_ self_val, str, cmap));

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}        

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - splice
 */
int CVmObjString::getp_splice(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc)
{
    char ins_buf[128];

    /* check arguments */
    uint oargc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(2, 1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the starting index and deletion length */
    int start_idx = CVmBif::pop_int_val(vmg0_);
    int del_len = CVmBif::pop_int_val(vmg0_);

    /* get and skip our length */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;
    utf8_ptr p((char *)str);
    int charlen = p.len(len);

    /* adjust the index to 0-based, and figure end-of-string offsets */
    start_idx += (start_idx < 0 ? charlen : -1);

    /* make sure the starting index and deletion length are in bounds */
    start_idx = (start_idx < 0 ? 0 :
                 start_idx > (int)charlen ? charlen :
                 start_idx);
    del_len = (del_len < 0 ? 0 :
               del_len > (int)(charlen - start_idx) ? charlen - start_idx :
               del_len);

    /* get the starting byte index */
    size_t start_byte_idx = p.bytelen(start_idx);

    /* save 'self' as gc protection */
    G_stk->push(self_val);
    
    /* 
     *   If there's an insertion string, get it as a string.  Treat nil as an
     *   empty insertion.  
     */
    size_t ins_len = 0;
    const char *ins = 0;
    vm_val_t new_ins_str;
    new_ins_str.set_nil();
    if (oargc >= 3 && G_stk->get(1)->typ != VM_NIL)
    {
        /* 
         *   Leave the insertion string on the stack for gc protection, and
         *   do an explicit conversion to string.  
         */
        ins = cvt_to_str(vmg_ &new_ins_str, ins_buf, sizeof(ins_buf),
                         G_stk->get(1), 10, 0);
        ins_len = vmb_get_len(ins);
        ins += VMB_LEN;
    }

    /* push the new insertion string (if any) for gc protection */
    G_stk->push(&new_ins_str);

    /* check to see if we're making any changes */
    if (del_len != 0 || ins_len != 0)
    {
        /* allocate a new string at the proper length */
        retval->set_obj(create(vmg_ FALSE, len + ins_len - del_len));
        CVmObjString *new_str = (CVmObjString *)vm_objp(vmg_ retval->val.obj);
        char *newp = new_str->cons_get_buf();

        /* copy the part of our string up to the starting index */
        if (start_byte_idx > 0)
        {
            memcpy(newp, str, start_byte_idx);
            newp += start_byte_idx;
        }

        /* if we have an insertion string, copy it */
        if (ins_len != 0)
        {
            memcpy(newp, ins, ins_len);
            newp += ins_len;
        }

        /* skip past the deleted material in the original string */
        p.set((char *)str + start_byte_idx);
        for (len -= start_byte_idx ; del_len != 0 && len != 0 ; --del_len)
            p.inc(&len);
                
        /* if we have any more of the spliced string, copy it */
        if (len != 0)
            memcpy(newp, p.getptr(), len);
    }
    else
    {
        /* we're making no changes - return the original string */
        *retval = *self_val;
    }

    /* 
     *   discard the remaining arguments and the gc protection (original
     *   argc, minus 2 arg pops, plus 2 gc protection pushes -> oargc) 
     */
    G_stk->discard(oargc); 

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - split
 */
int CVmObjString::getp_split(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val,
                             const char *str, uint *argcp)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0, 2);
    uint argc = (argcp != 0 ? *argcp : 0);
    if (get_prop_check_argc(retval, argcp, &desc))
        return TRUE;

    /* get the string buffer and length */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* remember the start of the base string */
    const char *basestr = str;

    /* 
     *   Get the delimiter or split size, leaving it on the stack for gc
     *   protection.  This can be a string, a RexPattern, or an integer.  
     */
    const vm_val_t *delim = G_stk->get(0);
    const char *delim_str = 0;
    CVmObjPattern *delim_pat = 0;
    int split_len = 0;
    if (argc < 1 || delim->typ == VM_NIL)
    {
        /* there's no delimiter at all, so use the default split length 1 */
        split_len = 1;
    }
    else if (delim->typ == VM_INT)
    {
        /* it's a simple split length */
        split_len = delim->val.intval;
        if (split_len <= 0)
            err_throw(VMERR_BAD_VAL_BIF);
    }
    else if ((delim_str = delim->get_as_string(vmg0_)) == 0)
    {
        /* if it's not a length or string, it has to be a RexPattern object */
        if (delim->typ != VM_OBJ
            || !CVmObjPattern::is_pattern_obj(vmg_ delim->val.obj))
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get the pattern object, properly cast */
        delim_pat = (CVmObjPattern *)vm_objp(vmg_ delim->val.obj);
    }

    /* get the split count limit, if there is one */
    int32_t limit = -1;
    if (argc >= 2 && G_stk->get(1)->typ != VM_NIL)
    {
        /* there's an explicit limit; fetch it and make sure it's at least 1 */
        if ((limit = G_stk->get(1)->num_to_int(vmg0_)) < 1)
            err_throw(VMERR_BAD_VAL_BIF);
    }

    /* push 'self' for gc protection */
    G_stk->push(self_val);

    /* 
     *   Set up a return list.  If we have a limit, the list can't go over
     *   that length, so just create the list at the limit.  Otherwise, if we
     *   have a split length, we can figure the required list length by
     *   dividing the string length by the split length (rounding up).
     *   Otherwise we have no idea how many list elements we'll need, so just
     *   create the list at an arbitrary default length and expand later if
     *   needed.  
     */
    int init_list_len;
    if (limit > 0)
    {
        /* there's a limit, so we won't need more than this many elements */
        init_list_len = limit;
    }
    else if (split_len > 0)
    {
        /* split length -> divide the string length by the split length */
        init_list_len = (len + split_len - 1)/split_len;

        /* set the length to at least one, unless it's an empty string */
        if (init_list_len < 1 && len != 0)
            init_list_len = 1;
    }
    else
    {
        /* no limit or length, so start with an arbitrary guess */
        init_list_len = 10;
    }

    /* create the list */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, init_list_len));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* clear the list to nils, since we're building it incrementally */
    lst->cons_clear();

    /* push it for gc protection */
    G_stk->push(retval);

    /* repeatedly search for the delimiter and split the string */
    int cnt;
    vm_val_t ele;
    for (cnt = 0 ; (limit < 0 || cnt + 1 < limit) && len != 0 ; ++cnt)
    {
        /* search for the next delimiter or next 'split_len' characters */
        int match_ofs, match_len;
        if (split_len > 0)
        {
            /* figure the number of bytes in split_len characters */
            match_ofs = utf8_ptr::s_bytelen(str, split_len);

            /* if that uses the whole rest of the string, we're done */
            if (match_ofs >= (int)len)
                break;

            /* there's no delimiter, so the delimiter match length is zero */
            match_len = 0;
        }
        else
        {
            /* search for the substring or pattern */
            const char *nxt = find_substr(
                vmg_ self_val, basestr, str, len, delim_str, delim_pat,
                &match_ofs, &match_len);

            /* if we didn't find it, we're done */
            if (nxt == 0)
                break;

            /* figure the byte offset to the match */
            match_ofs = nxt - str;
        }

        /* add the substring from 'str' to 'p' to the list */
        lst->cons_ensure_space(vmg_ cnt, 0);
        ele.set_obj(CVmObjString::create(vmg_ FALSE, str, match_ofs));
        lst->cons_set_element(cnt, &ele);

        /* skip to the position after the match */
        size_t skip = match_ofs + match_len;
        str += skip;
        len -= skip;

        /* 
         *   if we matched zero characters, skip one character to prevent an
         *   infinite loop 
         */
        if (split_len == 0 && match_len == 0 && len != 0)
            str += utf8_ptr::s_bytelen(str, 1);
    }

    /* add a final element for the remainder of the string */
    if (len != 0)
    {
        lst->cons_ensure_space(vmg_ cnt, 0);
        ele.set_obj(CVmObjString::create(vmg_ FALSE, str, len));
        lst->cons_set_element(cnt, &ele);

        /* count it */
        ++cnt;
    }

    /* set the final size of the list */
    lst->cons_set_len(cnt);

    /* discard arguments plus gc protection */
    G_stk->discard(argc + 2);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Helper routines for specialsToHtml
 */

/* find the tag name within a tag */
static void find_tag_name(StringRef *tag, const char *&name, size_t &len,
                          int &is_end_tag)
{
    const char *p;

    /* skip leading spaces */
    for (p = tag->get() ; isspace(*p) ; ++p) ;

    /* if it's an end tag, skip the '/' and any intervening spaces */
    if ((is_end_tag = (*p == '/')) != 0)
        for (++p ; isspace(*p) ; ++p) ;

    /* scan the name - it's from here to the next space */
    for (name = p ; *p != '\0' && !isspace(*p) ; ++p) ;

    /* note the length */
    len = p - name;
}

/* parse the name from a tag */
static void parse_tag_name(StringRef *tag, char *&name, int &is_end_tag)
{
    /* get the tag name */
    size_t len;
    const char *namec;
    find_tag_name(tag, namec, len, is_end_tag);

    /* allocate a copy of the tag name */
    name = lib_copy_str(namec, len);
}

/* find an attribute value, given a pointer to the end of the attr name */
static int find_attr_val(const char *name, size_t name_len,
                         const char *&val, size_t &val_len)
{
    const char *p;

    /* go to the end of the attribute name */
    p = name + name_len;
    
    /* if there's an '=', there's a value */
    if (*p == '=')
    {
        /* the attribute value starts after the '=' */
        val = ++p;

        /* skip the token */
        for (char qu = 0 ; *p != 0 && (!isspace(*p) || qu != 0) ; ++p)
        {
            /* note if entering or leaving a quoted section */
            if (qu == 0 && (*p == '"' || *p == '\''))
                qu = *p;
            else if (*p == qu)
                qu = 0;
        }

        /* note the length of the value */
        val_len = p - val;

        /* there's an explicit value for this attribute */
        return TRUE;
    }
    else
    {
        /* the attribute name is its own implied value */
        val = name;
        val_len = name_len;

        /* there's no explicit value */
        return FALSE;
    }
}

/* get an attribute value in a tag */
static char *parse_attr_val(StringRef *tag, const char *attr_name)
{
    const char *p;
    size_t len;
    int is_end_tag;
    size_t attr_len = strlen(attr_name);
    
    /* find and skip the tag name */
    find_tag_name(tag, p, len, is_end_tag);
    p += len;
    
    /* keep going until we find the attribute or run out of text */
    for (;;)
    {
        /* skip leading spaces */
        for ( ; isspace(*p) ; ++p) ;

        /* if we're at the end of the text, we're done */
        if (*p == '\0')
            break;

        /* scan the attribute name */
        const char *an;
        size_t anlen;
        for (an = p ; *p != '\0' && *p != '=' && !isspace(*p) ; ++p) ;
        anlen = p - an;

        /* get the attribute value */
        const char *av;
        size_t avlen;
        find_attr_val(an, anlen, av, avlen);

        /* check for a match */
        if (anlen == attr_len && memicmp(attr_name, an, attr_len) == 0)
        {
            /* it's our attribute name - allocate a copy */
            char *retval = lib_copy_str(av, avlen);

            /* remove quotes */
            char *dst, qu;
            for (p = dst = retval, qu = 0 ; *p != '\0' ; ++p)
            {
                /* check for entering/leaving a quoted section */
                if (qu == 0 && (*p == '"' || *p == '\''))
                {
                    /* entering quotes - note the quote, don't copy it */
                    qu = *p;
                }
                else if (qu == *p)
                {
                    /* leaving quotes - don't copy the quote */
                    qu = 0;
                }
                else
                {
                    /* copy anything else as given */
                    *dst++ = *p;
                }
            }

            /* null-terminate the updated string and return it */
            *dst = '\0';
            return retval;
        }

        /* skip the value and move on to the next attribute */
        p = av + avlen;
    }

    /* didn't find the value */
    return 0;
}

/*
 *   property evaluator - specialsToHtml
 */
int CVmObjString::getp_specialsToHtml(VMG_ vm_val_t *retval,
                                      const vm_val_t *self_val,
                                      const char *str, uint *argc)
{
    return specialsTo(vmg_ retval, self_val, str, argc, TRUE);
}

/*
 *   property evaluator - specialsToText 
 */
int CVmObjString::getp_specialsToText(VMG_ vm_val_t *retval,
                                      const vm_val_t *self_val,
                                      const char *str, uint *argc)
{
    return specialsTo(vmg_ retval, self_val, str, argc, FALSE);
}

/*
 *   common property evaluator for specialsToText and specialsToHtml 
 */
int CVmObjString::specialsTo(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val,
                             const char *str, uint *argc,
                             int html)
{
    vm_rcdesc rc;
    vm_prop_id_t flags_prop = VM_INVALID_OBJ, tag_prop = VM_INVALID_OBJ;

    /* check arguments */
    uint oargc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set up the default initial state */
    int in_line = FALSE;
    int caps = FALSE, nocaps = FALSE;
    int space = FALSE, qspace = FALSE;
    int in_tag = FALSE, in_entity = FALSE;
    int qlevel = 0;
    char attrq = 0;
    int col = 0;
    StringRef *tag = new StringRef(128);

    /* get the string length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* set up a buffer for the output - anticipate a little expansion */
    StringRef *buf = new StringRef(len*5/4);

    err_try
    {
        /* get the state object, if present */
        vm_val_t stateobj;
        stateobj.set_nil();
        if (oargc >= 1)
        {
            /* the argument is there - get the object or nil */
            if (G_stk->get(0)->typ == VM_OBJ)
            {
                /* get the state object */
                stateobj = *G_stk->get(0);

                /* set up the recursive evaluation descriptor */
                rc.init(vmg_ "String.specialsToHtml", self_val, 13,
                        G_stk->get(0), oargc);

                /* get the 'flags_' and 'tag_' property IDs */
                flags_prop = G_predef->string_sth_flags;
                tag_prop = G_predef->string_sth_tag;

                /* get the flags */
                int32_t flags = 0;
                if (flags_prop != VM_INVALID_OBJ)
                {
                    /* get the property */
                    G_interpreter->get_prop(vmg_ 0, &stateobj, flags_prop,
                                            &stateobj, 0, &rc);

                    /* if it's an integer, get its value */
                    if (G_interpreter->get_r0()->typ == VM_INT)
                        flags = G_interpreter->get_r0()->val.intval;
                }

                /* decode the flags */
                in_line = (flags & 0x0001) != 0;
                caps = (flags & 0x0002) != 0;
                nocaps = (flags & 0x0004) != 0;
                in_tag = (flags & 0x0008) != 0;
                attrq = (flags & 0x0010 ? '"' : flags & 0x0020 ? '\'' : 0);
                space = (flags & 0x0040) != 0;
                qspace = (flags & 0x0080) != 0;
                qlevel = (flags & 0x0100) != 0;
                in_entity = (flags & 0x200) != 0;
                col = (flags & 0x3000) >> 12;

                /* get the tag value */
                const char *tagp;
                if (tag_prop != VM_INVALID_OBJ)
                {
                    /* get the property */
                    G_interpreter->get_prop(vmg_ 0, &stateobj, tag_prop,
                                            &stateobj, 0, &rc);

                    /* get the string value, if any */
                    tagp = G_interpreter->get_r0()->get_as_string(vmg0_);

                    /* copy the tag string, if we found it */
                    if (tagp != 0)
                        tag->append(tagp + VMB_LEN, vmb_get_len(tagp));
                }
            }
            else if (G_stk->get(0)->typ != VM_NIL)
            {
                /* anything other than an object or nil is an error */
                err_throw(VMERR_BAD_TYPE_BIF);
            }
        }

        /* parse the string */
        utf8_ptr p;
        for (p.set((char *)str) ; len != 0 ; p.inc(&len))
        {
            /* get the next character */
            wchar_t ch = p.getch();

            /* if we're in a tag, add this character to the tag */
            if (in_tag)
            {
                /* convert specials in the tag */
                switch (ch)
                {
                case '\n':
                case 0x0B:
                case 0x15:
                case '\t':
                    /* convert newlines, quoted spaces, and tabs to spaces */
                    tag->append(" ");
                    break;

                case 0x0E:
                case 0x0F:
                    /* \^ and \v have no place in tags */
                    break;

                case '"':
                case '\'':
                    /* note that we're entering or leaving quoted text */
                    if (attrq == 0)
                        attrq = (char)ch;
                    else if (attrq == ch)
                        attrq = 0;

                    /* append the character */
                    tag->append_utf8(ch);
                    break;

                case '>':
                    /* ignore it if we're in quoted text */
                    if (attrq != 0)
                    {
                        tag->append_utf8(ch);
                    }
                    else
                    {
                        /* end the tag */
                        in_tag = FALSE;
                        
                        /* parse the tag name */
                        char *tagname;
                        int is_end_tag;
                        parse_tag_name(tag, tagname, is_end_tag);

                        /* assume we'll copy it verbatim */
                        int keep_tag = TRUE;

                        /* check to see if it's one of our special tags */
                        if (stricmp(tagname, "q") == 0)
                        {
                            /* 
                             *   <Q> or </Q> - translate to &ldquo..&rdquo or
                             *   &lsquo..&rsquo 
                             */
                            static const char *qentity[] = {
                                "&ldquo;", "&rdquo;",
                                "&lsquo;", "&rsquo;"
                            };
                            static const char *qtxt[] = {
                                "\"", "\"", "'", "'"
                            };
                            
                            /* if it's </Q>, pre-decrement the level */
                            if (is_end_tag)
                                qlevel = !qlevel;
                            
                            /* 
                             *   figure which type of quote to use: double
                             *   quotes at even levels, single quotes at odd
                             *   levels; left quotes for open tags, right
                             *   quotes for close tags 
                             */
                            int idx = (qlevel ? 2 : 0)
                                      + (is_end_tag ? 1 : 0);
                            
                            /* add the quote */
                            buf->append(html ? qentity[idx] : qtxt[idx]);
                            col++;
                            
                            /* if it's <Q>, post-increment the level */
                            if (!is_end_tag)
                                qlevel = !qlevel;

                            /* we've translated the tag */
                            keep_tag = FALSE;
                        }
                        else if (stricmp(tagname, "br") == 0)
                        {
                            /* 
                             *   if there's a HEIGHT attribute, it's special;
                             *   otherwise it's just a regular <br> 
                             */
                            char *htv = parse_attr_val(tag, "height");
                            if (htv != 0)
                            {
                                /* get the numeric value */
                                int ht = atoi(htv);

                                /* 
                                 *   if we're at the beginning of a line,
                                 *   translate this to 'ht' <BR> tags; if
                                 *   we're within a line, use 'ht+1' <BR>
                                 *   tags 
                                 */
                                if (in_line)
                                    ht += 1;

                                /* write out the <BR> tags */
                                while (ht-- > 0)
                                    buf->append(html ? "<br>" : "\n");

                                /* done with the height value */
                                lib_free_str(htv);

                                /* we've translated the tag */
                                keep_tag = FALSE;
                            }
                            else if (!html)
                            {
                                /* regular text mode - convert to \n */
                                buf->append("\n");
                            }

                            /* we're no longer in a line */
                            in_line = FALSE;
                            col = 0;
                        }
                        else if (stricmp(tagname, "p") == 0)
                        {
                            /* in text mode, add a newline or two */
                            if (!html)
                            {
                                buf->append("\n");
                                if (in_line)
                                    buf->append("\n");
                            }

                            /* this counts as a line break */
                            in_line = FALSE;
                            col = 0;
                        }
                        else if (stricmp(tagname, "div") == 0
                                 || stricmp(tagname, "center") == 0
                                 || stricmp(tagname, "table") == 0
                                 || stricmp(tagname, "td") == 0
                                 || stricmp(tagname, "th") == 0
                                 || stricmp(tagname, "caption") == 0)
                        {
                            /* this counts as a line break */
                            in_line = FALSE;
                            col = 0;

                            /* in text mode, add a newline */
                            if (!html)
                                buf->append("\n");
                        }

                        /* 
                         *   if we're keeping the original tag, copy it to
                         *   the output 
                         */
                        if (html && keep_tag)
                        {
                            buf->append("<");
                            buf->append(tag->get(), tag->getlen());
                            buf->append(">");
                        }
                        
                        /* done with the tag name */
                        lib_free_str(tagname);

                        /* we're done with the tag - clear the buffer */
                        tag->truncate(0);
                    }
                    break;

                default:
                    /* append anything else as-is */
                    tag->append_utf8(ch);
                    break;
                }

                /* we're done processing this character */
                continue;
            }

            /* continue gathering entity text if applicable */
            if (in_entity)
            {
                /* 
                 *   If the entity is more than 10 character long, or this is
                 *   a semicolon, end the entity.  Also stop if we're
                 *   processing a &# entity and this isn't a digit.  
                 */
                if (ch == ';'
                    || tag->getlen() >= 10
                    || (tag->get()[0] == '#' && !is_digit(ch)))
                {
                    /* check what we have in the tag buffer */
                    char *n = tag->get();
                    if (n[0] == '#')
                    {
                        /* get the decimal unicode value */
                        buf->append_utf8((wchar_t)atoi(n+1));

                        /* 
                         *   if we stopped on something other than a ';', add
                         *   it as well 
                         */
                        if (ch != ';')
                            buf->append_utf8(ch);
                    }
                    else if (stricmp(n, "nbsp") == 0)
                        buf->append_utf8(' ');
                    else if (stricmp(n, "gt") == 0)
                        buf->append_utf8('>');
                    else if (stricmp(n, "lt") == 0)
                        buf->append_utf8('<');
                    else if (stricmp(n, "amp") == 0)
                        buf->append_utf8('&');
                    else if (stricmp(n, "ldquo") == 0
                             || stricmp(n, "rdquo") == 0
                             || stricmp(n, "quot") == 0)
                        buf->append_utf8('\"');
                    else if (stricmp(n, "lsquo") == 0
                             || stricmp(n, "rsquo") == 0)
                        buf->append_utf8('\'');
                    else
                    {
                        /* unknown entity - copy it as-is */
                        buf->append(n);
                    }

                    /* we're no longer in an entity */
                    in_entity = FALSE;
                    tag->truncate(0);
                }
                else
                {
                    /* 
                     *   we're still working on gathering this entity -
                     *   simply buffer the character for now 
                     */
                    tag->append_utf8(ch);
                }

                /* we've handled this character */
                continue;
            }

            /* if we have a pending space, write it out */
            if (space)
            {
                /* 
                 *   We have a pending regular space.  If the next character
                 *   is a quoted space, omit the regular space entirely, as
                 *   regular spaces combine with adjacent quoted spaces.
                 *   Otherwise, write it as a regular space.  
                 */
                if (ch != 0x15)
                {
                    buf->append(" ");
                    col++;
                }

                /* we've now processed this pending space */
                space = FALSE;
            }
            else if (qspace)
            {
                /* 
                 *   We have a pending quoted space.
                 *   
                 *   If the next character is a regular space, skip the space
                 *   and keep the pending quoted space flag set - the regular
                 *   space combines with the quoted space, so we can remove
                 *   it, but we don't yet know how to handle the quoted space
                 *   itself since we have to see the next non-space character
                 *   to determine that.
                 *   
                 *   If the next character is another quoted space, write the
                 *   pending quoted space as &nbsp;, since adjacent quoted
                 *   spaces don't combine.
                 *   
                 *   If the next character is anything else (not a space or
                 *   quoted space), write the pending quoted space as a
                 *   regular space, since the html renderer won't combine the
                 *   space with the following non-space character, and we do
                 *   want to allow a line break after a quoted space.  
                 */
                if (ch == ' ')
                    continue;
                else if (html)
                {
                    buf->append(ch == 0x15 ? "&nbsp;" : " ");
                    col++;
                }
                else
                {
                    buf->append(" ");
                    col++;
                }
                
                /* we've now processed this pending quoted space */
                qspace = FALSE;
            }

            /* handle the character */
            switch (ch)
            {
            case '\n':
                /* add a <br> if we're within a line */
                if (in_line)
                    buf->append(html ? "<br>" : "\n");
                
                /* we're now at the start of a new line */
                in_line = FALSE;
                col = 0;
                caps = nocaps = FALSE;
                break;

            case 0x0B:
                /* 
                 *   '\b' - blank line: add two <BR>'s if we're within a
                 *   line, otherwise just add one 
                 */
                buf->append(html ? "<br>" : "\n");
                if (in_line)
                    buf->append(html ? "<br>" : "\n");
                
                /* we're now at the start of a new line */
                in_line = FALSE;
                col = 0;
                caps = nocaps = FALSE;
                break;

            case 0x0F:
                /* 
                 *   '\^' - this doesn't write anything to the output, but
                 *   simply sets a flag that tells us to capitalize the next
                 *   printing character 
                 */
                caps = TRUE;
                nocaps = FALSE;
                break;

            case 0x0E:
                /* 
                 *   '\v' - this doesn't write anything to the output, but
                 *   simply sets a flag that tells us to lower-case the next
                 *   printing character 
                 */
                caps = FALSE;
                nocaps = TRUE;
                break;

            case 0x15:
                /* 
                 *   '\ ' - quoted space: this is a non-combining space (it
                 *   can't be consolidated with adjacent spaces) that allows
                 *   line breaks.  Don't write anything now, but set the
                 *   quoted space flag for the next character: if the next
                 *   character out is a regular or quoted space, we'll write
                 *   the quoted space as a non-breaking space; otherwise
                 *   we'll write the quoted space as a regular space, so that
                 *   we can break after it.
                 */
                qspace = TRUE;
                break;

            case ' ':
                /* 
                 *   Regular space.  Don't actually write anything yet, since
                 *   we'll have to turn this into a non-breaking space if the
                 *   next character turns out to be a quoted space.  Just set
                 *   the space-pending flag for now.
                 */
                space = TRUE;
                break;

            case '\t':
                /* 
                 *   Tab - append spaces to take us out to the next 4-column
                 *   tab stop.  We want the renderer to obey the spaces
                 *   (rather than consolidating them), so use non-breaking
                 *   spaces (&nbsp;).  However, we want to allow a line break
                 *   after the tab, so use an ordinary space for the last
                 *   one.  The renderer won't combine an ordinary space with
                 *   an &nbsp, so it'll render the number of spaces we ask
                 *   for, but we'll be able to break the line after that
                 *   last, ordinary space.
                 *   
                 *   In regular text mode, just write out the tab character.
                 */
                if (html)
                {
                    col = ((col + 1) & 3) - 1;
                    for ( ; col + 1 < 3 ; ++col)
                        buf->append("&nbsp;");
                    buf->append(" ");
                    ++col;
                }
                else
                    buf->append("\t");
                break;

            case '<':
                /* start a tag */
                in_tag = TRUE;
                break;

            case '&':
                /* if in text mode, gather the entity information */
                if (!html)
                    in_entity = TRUE;
                else
                    buf->append("&");
                break;

            default:
                /* Ordinary character.  Check for caps/nocaps conversions. */
                {
                    const wchar_t *u = 0;
                    if (caps)
                    {
                        u = t3_to_upper(ch);
                        caps = FALSE;
                    }
                    else if (nocaps)
                    {
                        u = t3_to_lower(ch);
                        nocaps = FALSE;
                    }

                    /* we're now within a line */
                    in_line = TRUE;

                    /* add this character or string to the output */
                    if (u != 0)
                    {
                        for ( ; *u != 0 ; buf->append_utf8(*u++)) ;
                    }
                    else
                    {
                        buf->append_utf8(ch);
                    }

                    /* advance the tab-stop column */
                    col++;
                }
                break;
            }
        }

        /*
         *   If we have a state object, write the final state back to the
         *   state object, so that the next call can pick up where we left
         *   off.  
         */
        if (stateobj.typ == VM_OBJ)
        {
            /* encode the flags */
            vm_val_t v;
            v.set_int((in_line ? 0x0001 : 0)
                      | (caps ? 0x0002 : 0)
                      | (nocaps ? 0x0004 : 0)
                      | (in_tag ? 0x0008 : 0)
                      | (attrq == '"' ? 0x0010 : attrq == '\'' ? 0x0020 : 0)
                      | (space ? 0x0040 : 0)
                      | (qspace ? 0x0080 : 0)
                      | (qlevel ? 0x0100 : 0)
                      | (in_entity ? 0x0200 : 0)
                      | ((col & 3) << 12));

            /* set the flags */
            if (flags_prop != VM_INVALID_OBJ)
            {
                G_interpreter->set_prop(
                    vmg_ stateobj.val.obj, flags_prop, &v);
            }

            /* set the tag */
            if (tag_prop != VM_INVALID_OBJ)
            {
                if (in_tag || in_entity)
                {
                    /* we have a tag - set stateobj.tags_ to the string */
                    G_interpreter->push_string(
                        vmg_ tag->get(), tag->getlen());
                    G_interpreter->set_prop(vmg_ stateobj.val.obj, tag_prop,
                                            G_stk->get(0));

                    /* done with the stacked string */
                    G_stk->discard();
                }
                else
                {
                    /* no tag - set stateobj.tags_ to nil */
                    v.set_nil();
                    G_interpreter->set_prop(
                        vmg_ stateobj.val.obj, tag_prop, &v);
                }
            }
        }

        /* return the result as a string value */
        G_interpreter->push_string(vmg_ buf->get(), buf->getlen());
        G_stk->pop(retval);
    }
    err_finally
    {
        /* release resources */
        tag->release_ref();
        buf->release_ref();
    }
    err_end;

    /* discard arguments and gc protection items */
    G_stk->discard(oargc); 

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - urlEncode()
 */
int CVmObjString::getp_urlEncode(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* save my own value for gc protection */
    G_stk->push(self_val);

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* scan the string and determine the size of the expansion */
    size_t outlen;
    const char *p;
    size_t rem;
    for (outlen = 0, p = str, rem = len ; rem != 0 ; ++p, --rem)
    {
        /* 
         *   if this is a space, letter, digit, or '-' or '_', it requires
         *   one byte in the output; otherwise it requires three bytes for
         *   the "%xx" sequence 
         */
        char c = *p;
        if (c == ' ' || c == '-' || c == '_'
            || (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9'))
            outlen += 1;
        else
            outlen += 3;
    }

    /* allocate the return string */
    retval->set_obj(create(vmg_ FALSE, outlen));
    char *dst =
        ((CVmObjString *)vm_objp(vmg_ retval->val.obj))->cons_get_buf();

    /* build the return string */
    for (p = str, rem = len ; rem != 0 ; ++p, --rem)
    {
        /* translate this byte */
        char c = *p;
        if (c == ' ')
        {
            /* translate ' ' to '+' */
            *dst++ = '+';
        }
        else if (c == '-' || c == '_'
                 || (c >= 'a' && c <= 'z')
                 || (c >= 'A' && c <= 'Z')
                 || (c >= '0' && c <= '9'))
        {
            /* letter, digit, '-', '_' - leave unchanged */
            *dst++ = c;
        }
        else
        {
            /* translate anything else to a %xx sequence */
            *dst++ = '%';
            byte_to_xdigits(dst, (unsigned char)c);
            dst += 2;
        }
    }

    /* discard gc protection */
    G_stk->discard(1);

    /* done */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - urlDecode() 
 */
int CVmObjString::getp_urlDecode(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* save my own value for gc protection */
    G_stk->push(self_val);

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* scan the string and determine the size of the result */
    size_t outlen;
    const char *p;
    size_t rem;
    for (outlen = 0, p = str, rem = len ; rem != 0 ; ++p, --rem, ++outlen)
    {
        /* 
         *   if this is a '%xx' sequence, it translates to a single byte;
         *   otherwise just copy the byte as-is 
         */
        if (*p == '%' && rem >= 3 && is_xdigit(*(p+1)) && is_xdigit(*(p+2)))
        {
            /* 
             *   it's a %xx sequence - skip the whole thing and count is as
             *   just one byte of output 
             */
            p += 2;
            rem -= 2;
        }
    }

    /* allocate the return string */
    retval->set_obj(create(vmg_ FALSE, outlen));
    char *outbuf =
        ((CVmObjString *)vm_objp(vmg_ retval->val.obj))->cons_get_buf();
    char *dst = outbuf;

    /* build the return string */
    for (p = str, rem = len ; rem != 0 ; ++p, --rem, ++dst)
    {
        /* 
         *   if it's a '+', translate to a space; otherwise, if this is a
         *   '%xx' sequence, it translates to a single byte; otherwise just
         *   copy the byte as-is 
         */
        if (*p == '+')
        {
            *dst = ' ';
        }
        else if (*p == '%' && rem >= 3
                 && is_xdigit(*(p+1)) && is_xdigit(*(p+2)))
        {
            /* it's a %xx sequence - translate to a byte */
            *dst = (value_of_xdigit(*(p+1)) << 4)
                   | value_of_xdigit(*(p+2));

            /* skip the %xx sequence in the input */
            p += 2;
            rem -= 2;
        }
        else
        {
            /* copy anything else as-is */
            *dst = *p;
        }
    }

    /* validate the resulting UTF8 */
    CCharmapToUni::validate(outbuf, outlen);

    /* discard gc protection */
    G_stk->discard(1);

    /* done */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - sha256()
 */
int CVmObjString::getp_sha256(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* calculate the hash */
    char hash[65];
    sha256_ez(hash, str, len);

    /* allocate the return string */
    retval->set_obj(create(vmg_ FALSE, hash, 64));

    /* done */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - digestMD5()
 */
int CVmObjString::getp_md5(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* calculate the hash */
    char hash[33];
    md5_ez(hash, str, len);

    /* allocate the return string */
    retval->set_obj(create(vmg_ FALSE, hash, 32));

    /* done */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - packBytes() 
 */
int CVmObjString::static_getp_packBytes(VMG_ vm_val_t *retval, uint *pargc)
{
    /* check arguments */
    uint argc = (pargc != 0 ? *pargc : 0);
    static CVmNativeCodeDesc desc(1, 0, TRUE);
    if (get_prop_check_argc(retval, pargc, &desc))
        return TRUE;

    /* set up an in-memory data stream to receive the packed data */
    CVmMemorySource *dst = new CVmMemorySource(0L);

    err_try
    {
        /* do the packing */
        CVmPack::pack(vmg_ 0, argc, dst);

        /* create a string from the byte stream */
        retval->set_obj(create_latin1(vmg_ FALSE, dst));
    }
    err_finally
    {
        /* done with the in-memory data source */
        delete dst;
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* done */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   UTF-8 String data source.  This translates characters from the string to
 *   bytes, treating the character code as the byte value.  If we try to read
 *   a character outside of the 0..255 range, we'll throw an error.  
 */
class CVmUTF8Source: public CVmDataSource
{
public:
    CVmUTF8Source(const char *str, size_t len)
    {
        /* remember the string and its byte length */
        this->str = str;
        this->bytelen = len;

        /* calculate the character length */
        utf8_ptr p((char *)str);
        this->charlen = p.len(len);

        /* start at the beginning of the string */
        this->charidx = 0;
        this->byteidx = 0;
    }

    virtual CVmDataSource *clone(VMG_ const char * /*mode*/) { return 0; }

    /* read bytes - returns 0 on success, non-zero on EOF or error */
    virtual int read(void *buf, size_t len)
    {
        /* do the read; if the request isn't filled exactly, return failure */
        return (size_t)readc(buf, len) != len;
    }

    /* read bytes - returns the number of bytes read; 0 means EOF or error */
    virtual int readc(void *buf0, size_t len)
    {
        /* get the output buffer pointer as a character pointer */
        char *buf = (char *)buf0;
        
        /* 
         *   limit the length to the remaining number of characters from the
         *   current position 
         */
        size_t rem = charlen - charidx;
        if (len > rem)
            len = rem;

        /* do the copy */
        utf8_ptr p((char *)str + byteidx);
        for (size_t i = 0 ; i < len ; ++i, p.inc(), ++charidx)
        {
            /* get this character */
            wchar_t ch = p.getch();

            /* if it's outside of the 0..255 range, it's an error */
            if (ch > 255)
                err_throw(VMERR_NUM_OVERFLOW);

            /* store it */
            *buf++ = (char)ch;
        }

        /* calculate the new byte index */
        byteidx = (size_t)(p.getptr() - str);

        /* return the length we copied */
        return len;
    }

    /* write bytes - we're read-only, so just return an error */
    virtual int write(const void *, size_t) { return 1; }

    /* 
     *   Get the length of the file in bytes.  We translate each character
     *   from the string into into one byte in the stream, so our byte length
     *   equals our character length. 
     */
    virtual long get_size() { return charlen; }

    /* get the current seek location */
    virtual long get_pos() { return charidx; }

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode)
    {
        /* figure the absolute position based on the mode */
        switch (mode)
        {
        case OSFSK_SET:
            /* set - the offset is already absolute */
            break;

        case OSFSK_CUR:
            /* relative to current index */
            ofs += charidx;
            break;

        case OSFSK_END:
            /* relative to the end of file */
            ofs += charlen;
            break;
            
        default:
            /* invalid mode - return an error */
            return 1;
        }

        /* limit the position to the bounds of the string */
        if (ofs < 0)
            ofs = 0;
        else if (ofs > (long)charlen)
            ofs = (long)charlen;

        /* 
         *   The caller is working with the stream: the byte index in the
         *   stream equals the character index in the string.
         */
        charidx = (size_t)ofs;

        /* calculate the byte index for the new character index */
        utf8_ptr p((char *)str);
        byteidx = p.bytelen(charidx);

        /* success */
        return 0;
    }

    /* flush - no effect, since we don't buffer anything */
    virtual int flush() { return 0; }

    /* close - we have no system resources, so this is a no-op */
    virtual void close() { }

protected:
    /* the string */
    const char *str;

    /* byte and character length of the string */
    size_t bytelen, charlen;

    /* current byte and character seek position in the string */
    size_t byteidx, charidx;
};

/*
 *   property evaluator - unpackBytes()
 */
int CVmObjString::getp_unpackBytes(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* get the format string (leave it on the stack for gc protection) */
    const char *fmt = G_stk->get(0)->get_as_string(vmg0_);
    if (fmt == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get its buffer and length */
    size_t fmtlen = vmb_get_len(fmt);
    fmt += VMB_LEN;

    /* unpack the string */
    CVmUTF8Source src(str, len);
    CVmPack::unpack(vmg_ retval, fmt, fmtlen, &src);

    /* discard arguments */
    G_stk->discard(1);

    /* done */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - compareTo()
 */
int CVmObjString::getp_compareTo(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* get the other string (leave it on the stack for gc protection) */
    const char *other = G_stk->get(0)->get_as_string(vmg0_);
    if (other == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get its buffer and length */
    size_t olen = vmb_get_len(other);
    other += VMB_LEN;

    /* no result yet */
    retval->set_nil();

    /* compare character by character */
    utf8_ptr a((char *)str);
    utf8_ptr b((char *)other);
    for ( ; len != 0 && olen != 0 ; a.inc(&len), b.inc(&olen))
    {
        int delta = (int)a.getch() - (int)b.getch();
        if (delta != 0)
        {
            retval->set_int(delta);
            break;
        }
    }

    /* if we didn't find any differences, the shorter string sorts first */
    if (retval->typ == VM_NIL)
        retval->set_int(len - olen);

    /* discard arguments */
    G_stk->discard(1);

    /* done */
    return TRUE;
}

/*
 *   property evaluator - compareIgnoreCase()
 */
int CVmObjString::getp_compareIgnoreCase(VMG_ vm_val_t *retval,
                                         const vm_val_t *self_val,
                                         const char *str, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the length and buffer pointer */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /* get the other string (leave it on the stack for gc protection) */
    const char *other = G_stk->get(0)->get_as_string(vmg0_);
    if (other == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get its buffer and length */
    size_t olen = vmb_get_len(other);
    other += VMB_LEN;

    /* compare the strings */
    retval->set_int(t3_compare_case_fold(str, len, other, olen, 0));

    /* discard arguments */
    G_stk->discard(1);

    /* done */
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

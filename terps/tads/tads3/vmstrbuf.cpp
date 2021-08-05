#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstrbuf.cpp - StringBuffer object
Function
  
Notes
  
Modified
  12/13/09 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmstrbuf.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmmcreg.h"
#include "vmfile.h"
#include "vmbif.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmstr.h"
#include "utf8.h"

/* ------------------------------------------------------------------------ */
/*
 *   The largest buffer length we can store is the smaller of OSMALMAX or the
 *   maximum (positive) value for an int32_t (2^31-1).
 */
const int32_t STRBUF_MAX_LEN = (OSMALMAX < 0x7fffffff ? OSMALMAX : 0x7fffffff);


/* ------------------------------------------------------------------------ */
/*
 *   Extended undo record.  We attach this record to the standard undo record
 *   via the 'ptrval' field.  
 */
struct strbuf_undo_rec
{
    /* action type */
    enum strbuf_undo_action action;

    /* index of start of replacement */
    int32_t idx;

    /* length of old replaced data */
    int32_t old_len;

    /* length of new data */
    int32_t new_len;

    /* old data */
    wchar_t str[1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_strbuf_ext *vm_strbuf_ext::alloc_ext(VMG_ CVmObjStringBuffer *self,
                                        int32_t alo, int32_t inc)
{
    /* calculate how much space we need */
    int32_t siz = sizeof(vm_strbuf_ext) + (alo-1)*sizeof(wchar_t);

    /* allocate the memory */
    vm_strbuf_ext *ext = (vm_strbuf_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);

    /* remember the sizes */
    ext->alo = alo;
    ext->inc = inc;

    /* the buffer is currently empty */
    ext->len = 0;

    /* return the new extension */
    return ext;
}

/*
 *   Expand an extension structure to make room for the given string length
 */
vm_strbuf_ext *vm_strbuf_ext::expand_ext(VMG_ CVmObjStringBuffer *self,
                                         vm_strbuf_ext *old_ext,
                                         int32_t new_len)
{
    /* if the buffer is big enough to hold the new length, we're done */
    if (old_ext->alo >= new_len)
        return old_ext;

    /* 
     *   figure the required new allocation size: we allocate in increments
     *   of 'inc' from the old extension, so allocate enough for the new
     *   length, rounded up 
     */
    int32_t new_alo = ((new_len + old_ext->inc - 1) / old_ext->inc)
                      * old_ext->inc;
    vm_strbuf_ext *new_ext = alloc_ext(vmg_ self, new_alo, old_ext->inc);

    /* copy the current string buffer to the new extension */
    new_ext->len = old_ext->len;
    memcpy(new_ext->buf, old_ext->buf,
           old_ext->len * sizeof(old_ext->buf[0]));

    /* delete the old memory */
    G_mem->get_var_heap()->free_mem(old_ext);

    /* return the new extension */
    return new_ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   StringBuffer object statics 
 */

/* metaclass registration object */
static CVmMetaclassStringBuffer metaclass_reg_obj;
CVmMetaclass *CVmObjStringBuffer::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjStringBuffer::
    *CVmObjStringBuffer::func_table_[])(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *argc) =
{
    &CVmObjStringBuffer::getp_undef,                                   /* 0 */
    &CVmObjStringBuffer::getp_len,                                     /* 1 */
    &CVmObjStringBuffer::getp_char_at,                                 /* 2 */
    &CVmObjStringBuffer::getp_append,                                  /* 3 */
    &CVmObjStringBuffer::getp_insert,                                  /* 4 */
    &CVmObjStringBuffer::getp_copyChars,                               /* 5 */
    &CVmObjStringBuffer::getp_delete,                                  /* 6 */
    &CVmObjStringBuffer::getp_splice,                                  /* 7 */
    &CVmObjStringBuffer::getp_substr                                   /* 8 */
};

/* ------------------------------------------------------------------------ */
/*
 *   StringBuffer metaclass implementation 
 */

/*
 *   construction
 */
CVmObjStringBuffer::CVmObjStringBuffer(VMG_ int32_t alo, int32_t inc)
{
    /* allocate our extension structure */
    ext_ = (char *)vm_strbuf_ext::alloc_ext(vmg_ this, alo, inc);
}

/*
 *   create a StringBuffer with a given allocation size
 */
vm_obj_id_t CVmObjStringBuffer::create(VMG_ int in_root_set,
                                       int32_t alo, int32_t inc)
{
    /* allocate the object ID */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);

    /* create the C++ object */
    new (vmg_ id) CVmObjStringBuffer(vmg_ alo, inc);

    /* return the new ID */
    return id;
}

/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjStringBuffer::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    vm_obj_id_t id;
    int alo, inc;

    /* get the arguments */
    if (argc == 0)
    {
        /* no arguments - use a default size */
        alo = 256;
        inc = 256;
    }
    else if (argc == 1)
    {
        /* one argument - it's the initial size */
        alo = CVmBif::pop_int_val(vmg0_);

        /* 
         *   figure a default increment: for small initial buffers, expand by
         *   the buffer size; for medium sizes, expand by half the buffer
         *   size; for large sizes, expand by a fixed maximum 
         */
        if (alo < 256)
            inc = alo;
        else if (alo < 4096)
            inc = alo/2;
        else
            inc = 2048;
    }
    else if (argc == 2)
    {
        /* the initial size and increment are both specified */
        alo = CVmBif::pop_int_val(vmg0_);
        inc = CVmBif::pop_int_val(vmg0_);
    }
    else
    {
        /* invalid arguments */
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
    }

    /* enforce minimum and maximum sizes */
    if (alo < 16)
        alo = 16;
    else if (alo > STRBUF_MAX_LEN)
        alo = STRBUF_MAX_LEN;
    if (inc < 16)
        inc = 16;
    else if (inc > STRBUF_MAX_LEN)
        inc = STRBUF_MAX_LEN;

    /* 
     *   allocate the object ID - this type of construction never creates a
     *   root object 
     */
    id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* create the object */
    new (vmg_ id) CVmObjStringBuffer(vmg_ alo, inc);

    /* return the new ID */
    return id;
}

/* 
 *   notify of deletion 
 */
void CVmObjStringBuffer::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* 
 *   set a property 
 */
void CVmObjStringBuffer::set_prop(VMG_ class CVmUndo *undo,
                                  vm_obj_id_t self, vm_prop_id_t prop,
                                  const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* 
 *   get a property 
 */
int CVmObjStringBuffer::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                                 vm_obj_id_t self, vm_obj_id_t *source_obj,
                                 uint *argc)
{
    uint func_idx;
    
    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    
    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }
    
    /* inherit default handling from our base class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/*
 *   apply an undo record 
 */
void CVmObjStringBuffer::apply_undo(VMG_ struct CVmUndoRecord *undo_rec)
{
    if (undo_rec->id.ptrval != 0)
    {
        strbuf_undo_rec *rec;
        vm_obj_id_t self = undo_rec->obj;

        /* get our private record from the standard undo record */
        rec = (strbuf_undo_rec *)undo_rec->id.ptrval;

        /* check the action in the record */
        switch(rec->action)
        {
        case STRBUF_UNDO_INS:
            /* we inserted text into the buffer; delete the new text */
            delete_text(vmg_ self, rec->idx, rec->new_len, FALSE);
            break;

        case STRBUF_UNDO_DEL:
            /* we deleted text; re-insert the old text */
            insert_text(vmg_ self, rec->idx, rec->str, rec->old_len, FALSE);
            break;

        case STRBUF_UNDO_REPL:
            /* we replaced text; splice the old text in place of the new */
            splice_text(vmg_ self,
                        rec->idx, rec->new_len, rec->str, rec->old_len,
                        FALSE);
            break;
        }

        /* discard the private record */
        t3free(rec);

        /* clear the pointer in the main record so we know it's gone */
        undo_rec->id.ptrval = 0;
    }
}

/*
 *   discard extra undo information 
 */
void CVmObjStringBuffer::discard_undo(VMG_ CVmUndoRecord *rec)
{
    /* delete our extra information record */
    if (rec->id.ptrval != 0)
    {
        /* free the record */
        t3free((strbuf_undo_rec *)rec->id.ptrval);

        /* clear the pointer so we know it's gone */
        rec->id.ptrval = 0;
    }
}

/* 
 *   load from an image file 
 */
void CVmObjStringBuffer::load_from_image(VMG_ vm_obj_id_t self,
                                         const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image file 
 */
void CVmObjStringBuffer::reload_from_image(VMG_ vm_obj_id_t self,
                                           const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjStringBuffer::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the string and allocation data from the image data */
    uint32_t alo = osrp4(ptr);
    uint32_t inc = osrp4(ptr + 4);
    uint32_t len = osrp4(ptr + 8);

    /* limit the sizes */
    if (alo < 16)
        alo = 16;
    else if (alo > STRBUF_MAX_LEN)
        alo = STRBUF_MAX_LEN;

    if (inc < 16)
        inc = 16;
    else if (inc > STRBUF_MAX_LEN)
        inc = STRBUF_MAX_LEN;

    if (len > alo)
        len = alo;

    /* skip to the string characters */
    ptr += 12;

    /* allocate the extension */
    vm_strbuf_ext *ext = vm_strbuf_ext::alloc_ext(vmg_ this, alo, inc);
    ext_ = (char *)ext;

    /* set the length */
    ext->len = len;

    /* read the string data into the buffer */
    for (wchar_t *dst = ext->buf ; len != 0 ;
         *dst++ = osrp2(ptr), ptr += 2, --len) ;
}


/* 
 *   save to a file 
 */
void CVmObjStringBuffer::save_to_file(VMG_ class CVmFile *fp)
{
    vm_strbuf_ext *ext = get_ext();

    /* write our allocation and string length data */
    fp->write_uint4(ext->alo);
    fp->write_uint4(ext->inc);
    fp->write_uint4(ext->len);

    /* write the string contents */
    const wchar_t *src;
    int i;
    for (i = ext->len, src = ext->buf ; i != 0 ; --i)
        fp->write_uint2(*src++);
}

/* 
 *   restore from a file 
 */
void CVmObjStringBuffer::restore_from_file(VMG_ vm_obj_id_t self,
                                           CVmFile *fp, CVmObjFixup *fixups)
{
    const int32_t bufchars = 256;
    char buf[bufchars * sizeof(uint16_t)];
    
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the fixed fields */
    fp->read_bytes(buf, 12);
    int32_t alo = vmb_get_int4(buf);
    int32_t inc = vmb_get_int4(buf + 4);
    int32_t len = vmb_get_int4(buf + 8);

    /* allocate the extension structure */
    vm_strbuf_ext *ext = vm_strbuf_ext::alloc_ext(vmg_ this, alo, inc);
    ext_ = (char *)ext;

    /* limit the length */
    if (len > alo)
        len = alo;

    /* store it in the extension */
    ext->len = len;

    /* read the string contents */
    wchar_t *dst;
    int32_t rem;
    for (dst = ext->buf, rem = len ; rem != 0 ; )
    {
        /* read one buffer-full, or 'rem', whichever is less */
        int32_t cur = rem > bufchars ? bufchars : rem;
        fp->read_bytes(buf, cur * sizeof(uint16_t));

        /* deduct this read from the total */
        rem -= cur;

        /* decode the characters */
        for (const char *src = buf ; cur != 0 ;
             *dst++ = osrp2(src), src += 2, --cur) ;
    }
}

/*
 *   add an undo record 
 */
void CVmObjStringBuffer::add_undo_rec(
    VMG_ vm_obj_id_t self, strbuf_undo_action action,
    int32_t idx, int32_t old_len, int32_t new_len)
{
    /* 
     *   if we're not inserting or deleting anything, there's no change, so
     *   there's no undo information to save 
     */
    if (old_len == 0 && new_len == 0)
        return;

    /* 
     *   replacing zero bytes is simply an insertion; substituting zero bytes
     *   is a deletion 
     */
    if (action == STRBUF_UNDO_REPL && old_len == 0)
        action = STRBUF_UNDO_INS;
    if (action == STRBUF_UNDO_REPL && new_len == 0)
        action = STRBUF_UNDO_DEL;

    /* 
     *   Figure the size for the extension record.  For an insertion, we
     *   don't need any extra data, since we're replacing zero characters.
     *   For a deletion or replacement, we need to make a copy of the
     *   characters we're removing, so we need old_len extra wchar_t's.  
     */
    int32_t siz = sizeof(strbuf_undo_rec);
    if (action == STRBUF_UNDO_DEL || action == STRBUF_UNDO_REPL)
        siz += (old_len - 1) * sizeof(wchar_t);

    /* allocate our record extension */
    strbuf_undo_rec *rec = (strbuf_undo_rec *)t3malloc(siz);

    /* set up the record */
    rec->action = action;
    rec->idx = idx;
    rec->old_len = old_len;
    rec->new_len = new_len;

    /* if we're deleting or replacing text, save the old text */
    if (action == STRBUF_UNDO_DEL || action == STRBUF_UNDO_REPL)
        memcpy(rec->str, get_ext()->buf + idx, old_len * sizeof(rec->str[0]));
    
    /* 
     *   Add the record to the global undo stream.  (We don't have anything
     *   to store in the 'value' field, so just store nil.) 
     */
    vm_val_t nilval;
    nilval.set_nil();
    if (!G_undo->add_new_record_ptr_key(vmg_ self, rec, &nilval))
    {
        /* 
         *   we didn't add an undo record, so our extra undo information
         *   isn't actually going to be stored in the undo system - hence we
         *   must delete our extra information 
         */
        t3free(rec);
    }
}

/*
 *   Create a string representation of the number 
 */
const char *CVmObjStringBuffer::cast_to_string(VMG_ vm_obj_id_t self,
                                               vm_val_t *new_str) const
{
    /* make a string out of the whole buffer */
    return substr_to_string(vmg_ new_str, 0, get_ext()->len);
}

/*
 *   Test for equality 
 */
int CVmObjStringBuffer::equals(VMG_ vm_obj_id_t self,
                               const vm_val_t *val, int /*depth*/) const
{
    /* 
     *   we can compare our contents to another string buffer, or to a
     *   regular string object 
     */
    const char *str;
    if (val->typ == VM_OBJ && is_string_buffer_obj(vmg_ val->val.obj))
    {
        /* compare the buffers character by character */
        CVmObjStringBuffer *other =
            (CVmObjStringBuffer *)vm_objp(vmg_ val->val.obj);
        int32_t len1 = get_ext()->len, len2 = other->get_ext()->len;
        const wchar_t *buf1 = get_ext()->buf, *buf2 = other->get_ext()->buf;

        /* if the lengths aren't equal, they're not equal */
        if (len1 != len2)
            return FALSE;

        /* they're equal if the contents match exactly */
        return memcmp(buf1, buf2, len1*sizeof(*buf1)) == 0;
    }
    else if ((str = val->get_as_string(vmg0_)) != 0)
    {
        /* compare to a constant string */
        return equals_str(str);
    }
    else
    {
        /* the other value isn't comparable to string */
        err_throw(VMERR_INVALID_COMPARISON);
        AFTER_ERR_THROW(return 0;)
    }
}

/* 
 *   test for equality against a constant string 
 */
int CVmObjStringBuffer::equals_str(const char *str) const
{
    /* get the buffer pointers and character lengths */
    const wchar_t *p1 = get_ext()->buf;
    utf8_ptr p2((char *)str + VMB_LEN);
    int32_t len1 = get_ext()->len, len2 = p2.len(vmb_get_len(str));

    /* if the lengths don't match, we can't be equal */
    if (len1 != len2)
        return FALSE;

    /* compare character by character */
    for (int32_t i = 0 ; i < len1 ; ++i, ++p1, p2.inc())
    {
        if (*p1 != p2.getch())
            return FALSE;
    }

    /* the characters match */
    return TRUE;
}

/*
 *   Compare value 
 */
int CVmObjStringBuffer::compare_to(VMG_ vm_obj_id_t self,
                                   const vm_val_t *val) const
{
    /* 
     *   we can compare our contents to another string buffer, or to a
     *   regular string object 
     */
    const char *str;
    if (val->typ == VM_OBJ && is_string_buffer_obj(vmg_ val->val.obj))
    {
        /* compare the buffers character by character */
        CVmObjStringBuffer *other =
            (CVmObjStringBuffer *)vm_objp(vmg_ val->val.obj);
        int32_t len1 = get_ext()->len, len2 = other->get_ext()->len;
        const wchar_t *buf1 = get_ext()->buf, *buf2 = other->get_ext()->buf;
        for (int32_t i = 0 ; i < len1 && i < len2 ; ++i, ++buf1, ++buf2)
        {
            if (*buf1 < *buf2)
                return -1;
            else if (*buf1 > *buf2)
                return 1;
        }

        /* 
         *   They're equal up to the full length of the shorter string.  If
         *   we ran out of strings at the same time, they're equal; otherwise
         *   the longer string compares greater 
         */
        return len1 - len2;
    }
    else if ((str = val->get_as_string(vmg0_)) != 0)
    {
        /* compare to a constant string */
        return compare_str(str);
    }
    else
    {
        /* the other value isn't comparable to string */
        err_throw(VMERR_INVALID_COMPARISON);
        AFTER_ERR_THROW(return 0;)
    }
}

/* 
 *   compare to a constant string 
 */
int CVmObjStringBuffer::compare_str(const char *str) const
{
    /* get the buffer pointers and character lengths */
    const wchar_t *p1 = get_ext()->buf;
    utf8_ptr p2((char *)str + VMB_LEN);
    int32_t len1 = get_ext()->len, len2 = p2.len(vmb_get_len(str));

    /* compare character by character */
    for (int32_t i = 0 ; i < len1 ; ++i, ++p1, p2.inc())
    {
        wchar_t c1 = *p1, c2 = p2.getch();
        if (c1 < c2)
            return -1;
        else if (c1 > c2)
            return 1;
    }

    /* 
     *   the strings are equal up to the length of the shorter string; the
     *   longer string compares higher 
     */
    return len1 - len2;
}

/*
 *   Calculate my hash value.  We yield the same hash value as a regular
 *   string with our contents.  (This is a requirement, since we compare
 *   equal to a regular string with the same contents.)  
 */
uint CVmObjStringBuffer::calc_hash(VMG_ vm_obj_id_t self, int) const
{
    uint hash = 0;
    const wchar_t *p = get_ext()->buf;
    int32_t len = get_ext()->len;

    /* calculate the hash */
    for (int32_t i = 0 ; i < len ; ++i)
        hash += *p++;

    /* return the result */
    return hash;
}

/*
 *   Adjust arguments to limits.  '*idx' is the 0-based index of the start of
 *   the segment we're extracting or replacing.  If 'len' is non-null, '*len'
 *   is the length of the existing chunk we're extracting, deleting, or
 *   replacing.  If 'ins' is non-null, '*ins' is the length of the new chunk
 *   we're inserting or substituting for the deleted chunk.
 *   
 *   Note that if both 'len' and 'ins' are provided, we assume the operation
 *   will delete '*len' characters and insert '*ins' characters in their
 *   place.  
 */
void CVmObjStringBuffer::adjust_args(
    int32_t *idx, int32_t *len, int32_t *ins) const
{
    vm_strbuf_ext *ext = get_ext();
    
    /* make sure the index is in the range 1..len */
    if (*idx < 0)
        *idx = 0;
    else if (*idx > ext->len)
        *idx = ext->len;
    
    /* make sure the length is in range 0..(len-idx) */
    if (len != 0)
    {
        if (*len < 0)
            *len = 0;
        else if (*idx + *len > ext->len)
            *len = ext->len - *idx;
    }

    /* make sure the insertion wouldn't push us past the limit */
    if (ins != 0)
    {
        int32_t del = (len != 0 ? *len : 0);
        if (*ins < 0)
            *ins = 0;
        else if (*idx + *ins + ext->len - del > STRBUF_MAX_LEN)
            err_throw(VMERR_STR_TOO_LONG);
    }
}

/*
 *   Calculate the length in bytes in UTF-8 format 
 */
int32_t CVmObjStringBuffer::utf8_length() const
{
    vm_strbuf_ext *ext = get_ext();

    /* get our character length */
    int32_t rem = ext->len;

    /* start with a zero length */
    int32_t bytelen = 0;

    /* scan our characters */
    for (const wchar_t *p = ext->buf ; rem > 0 ; ++p, --rem)
        bytelen += utf8_ptr::s_wchar_size(*p);

    /* return the result */
    return bytelen;
}


/*
 *   Copy a portion of the string to a buffer as UTF-8 bytes.
 *   
 *   'idx' is an in-out variable.  On input, it's the starting character
 *   index requested for the copy.  On output, it's updated to the index of
 *   the next character after the last character copied.  This can be used
 *   for piecewise copies, since it's updated to point to the start of the
 *   next piece after copying each piece.
 *   
 *   'bytelen' is an in-out variable.  On input, this is the number of bytes
 *   requested to copy to the buffer.  On output, it's the actual number of
 *   bytes copied.  This might be smaller than the request size, because (a)
 *   we won't copy past the end of the string, and (b) we'll only copy whole,
 *   well-formed character sequences, so if the requested number of bytes
 *   would copy a fractional character, we'll omit that fractional character
 *   and stop at the previous whole character.  
 */
void CVmObjStringBuffer::to_utf8(char *buf, int32_t &idx, int32_t &bytelen)
{
    vm_strbuf_ext *ext = get_ext();

    /* keep the index within our limits */
    adjust_args(&idx, 0, 0);

    /* we haven't copied any bytes yet */
    int32_t actual = 0;

    /* set up a utf8 pointer for the output buffer */
    utf8_ptr dst(buf);

    /* 
     *   copy characters until we reach the end of the string, or exhaust the
     *   requested number of bytes 
     */
    for (actual = 0 ; actual < bytelen && idx < (int32_t)ext->len ; ++idx)
    {
        /* get the next source character */
        wchar_t c = ext->buf[idx];
        int clen = utf8_ptr::s_wchar_size(c);
        
        /* make sure it fits in the remaining output buffer space */
        if (clen > bytelen - actual)
            break;

        /* store the character */
        dst.setch(c);

        /* count it in the output size */
        actual += clen;
    }

    /* return the actual length copied in bytelen */
    bytelen = actual;
}

/*
 *   Make a String object out of a substring of the buffer 
 */
const char *CVmObjStringBuffer::substr_to_string(
    VMG_ vm_val_t *new_str, int32_t idx, int32_t len) const
{
    vm_strbuf_ext *ext = get_ext();

    /* keep the arguments within our limits */
    adjust_args(&idx, &len, 0);

    /* calculate the size in bytes of the UTF-8 version of the string */
    utf8_ptr p;
    int32_t bytelen = p.setwchars(ext->buf + idx, len, 0);

    /* allocate a string */
    new_str->set_obj(CVmObjString::create(vmg_ FALSE, bytelen));
    CVmObjString *str = (CVmObjString *)vm_objp(vmg_ new_str->val.obj);

    /* translate the string to UTF-8 into the buffer */
    p.set(str->cons_get_buf());
    p.setwchars(ext->buf + idx, len, bytelen);

    /* return the new string buffer */
    return str->get_as_string(vmg0_);
}

/*
 *   Index the buffer - returns the character at the given index 
 */
int CVmObjStringBuffer::index_val_q(VMG_ vm_val_t *result, vm_obj_id_t self,
                                    const vm_val_t *index_val)
{
    /* get the index as an integer */
    int32_t idx = index_val->num_to_int(vmg0_);

    /* adjust to zero-based indexing or a negative end-based index */
    idx += (idx < 0 ? get_ext()->len : -1);

    /* ensure idx is in range */
    if (idx < 0 || idx >= (int32_t)get_ext()->len)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* return a one-character string of the character at this index */
    substr_to_string(vmg_ result, idx, 1);

    /* handled */
    return TRUE;
}


/*
 *   Set a character in the buffer by index 
 */
int CVmObjStringBuffer::set_index_val_q(VMG_ vm_val_t *result,
                                        vm_obj_id_t self,
                                        const vm_val_t *index_val,
                                        const vm_val_t *new_val)
{
    /* the index must be an integer */
    int32_t idx = index_val->num_to_int(vmg0_);

    /* adjust to zero-based indexing or a negative end-based index */
    idx += (idx < 0 ? get_ext()->len : -1);

    /* ensure idx is in range */
    if (idx < 0 || idx >= (int32_t)get_ext()->len)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* 
     *   If the new value is an integer, interpret it as a unicode character
     *   value to set at the given character.  Otherwise, cast it to a string
     *   and take its first character.  
     */
    wchar_t ch;
    if (new_val->is_numeric(vmg0_))
    {
        /* it's an integer - treat it as a unicode character value */
        int32_t ich = new_val->num_to_int(vmg0_);
        if (ich < 0 || ich > 65535)
            err_throw(VMERR_BAD_VAL_BIF);

        /* it's in range - cast to wchar_t */
        ch = (wchar_t)ich;
    }
    else
    {
        /* treat it as a string */
        const char *str = new_val->get_as_string(vmg0_);

        /* get the first character */
        utf8_ptr p((char *)str + VMB_LEN);
        ch = (vmb_get_len(str) > 0 ? p.getch() : 0);
    }

    /* splice the character at the desired index */
    splice_text(vmg_ self, (int)idx, 1, &ch, 1, TRUE);
    
    /* we change in place, so the result is 'self' */
    result->set_obj(self);

    /* handled */
    return TRUE;
}

/*
 *   Ensure that the buffer is big enough to hold the given number of
 *   characters 
 */
void CVmObjStringBuffer::ensure_space(VMG_ int32_t len)
{
    /* if the desired new length exceeds the limit, it's an error */
    if (len > STRBUF_MAX_LEN)
        err_throw(VMERR_STR_TOO_LONG);

    /* if the desired new length exceeds the current allocation, expand it */
    if (len > (int32_t)get_ext()->alo)
    {
        ext_ = (char *)vm_strbuf_ext::expand_ext(
            vmg_ this, get_ext(), len);
    }
}

/*
 *   Open a gap for an insertion and/or delete characters for a deletion. 
 */
void CVmObjStringBuffer::splice_move(
    VMG_ int32_t idx, int32_t del, int32_t ins)
{
    /* 
     *   if we're adding more than we're deleting, expand the buffer if
     *   necessary to accommodate the added length 
     */
    if (ins > del)
        ensure_added_space(vmg_ ins - del);

    /* 
     *   if we're doing a net insertion or deletion, move the part of the
     *   string after the deleted text so that it'll be aligned at the end of
     *   the inserted text 
     */
    if (ins != del && idx + del < get_ext()->len)
        memmove(get_ext()->buf + idx + ins,
                get_ext()->buf + idx + del,
                (get_ext()->len - (idx + del)) * sizeof(wchar_t));

    /* adjust the buffer length */
    get_ext()->len += ins - del;
}

/* 
 *   splice text into the buffer
 */
void CVmObjStringBuffer::splice_text(VMG_ vm_obj_id_t self,
                                     int32_t idx, int32_t del_chars,
                                     const wchar_t *src, int32_t ins_chars,
                                     int undo)
{
    /* check arguments */
    adjust_args(&idx, &del_chars, &ins_chars);

    /* if desired, save undo */
    if (undo)
        add_undo_rec(vmg_ self, STRBUF_UNDO_REPL, idx, del_chars, ins_chars);

    /* delete the old characters and open a gap for the new characters */
    splice_move(vmg_ idx, del_chars, ins_chars);

    /* copy in the characters */
    memcpy(get_ext()->buf + idx, src, ins_chars * sizeof(get_ext()->buf[0]));
}

/* 
 *   splice UTF-8 text into the buffer
 */
void CVmObjStringBuffer::splice_text(VMG_ vm_obj_id_t self,
                                     int32_t idx, int32_t del_chars,
                                     const char *src, int32_t ins_bytes,
                                     int undo)
{
    /* figure the character length of the string */
    utf8_ptr p((char *)src);
    int32_t ins_chars = p.len(ins_bytes);

    /* check arguments */
    adjust_args(&idx, &del_chars, &ins_chars);

    /* if desired, save undo */
    if (undo)
        add_undo_rec(vmg_ self, STRBUF_UNDO_REPL, idx, del_chars, ins_chars);

    /* open up a gap in the buffer */
    splice_move(vmg_ idx, del_chars, ins_chars);

    /* decode characters into the buffer */
    p.to_wchar(get_ext()->buf + idx, ins_chars, src, ins_bytes);
}

/* 
 *   property evaluator - get length 
 */
int CVmObjStringBuffer::getp_len(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the content length */
    retval->set_int(get_ext()->len);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - charAt
 */
int CVmObjStringBuffer::getp_char_at(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the index and adjust to 0-based or end-based */
    int32_t idx = CVmBif::pop_int_val(vmg0_);
    idx += (idx < 0 ? get_ext()->len : -1);

    /* make sure it's in range */
    if (idx < 0 || idx >= (int32_t)get_ext()->len)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* return the character at the given index, as an integer Unicode code */
    retval->set_int(get_ext()->buf[idx]);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - append text
 */
int CVmObjStringBuffer::getp_append(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get or cast the string; leave it on the stack for gc protection */
    vm_val_t strval;
    const char *str = G_stk->get(0)->cast_to_string(vmg_ &strval);
    *G_stk->get(0) = strval;

    /* insert the string at the end of the buffer */
    insert_text(vmg_ self, get_ext()->len,
                str + VMB_LEN, vmb_get_len(str), TRUE);

    /* discard gc protection */
    G_stk->discard();

    /* handled */
    retval->set_obj(self);
    return TRUE;
}

/* 
 *   property evaluator - insert
 */
int CVmObjStringBuffer::getp_insert(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the insertion index; adjust to 0-based or end-based indexing */
    int32_t idx = CVmBif::pop_int_val(vmg0_);
    idx += (idx < 0 ? get_ext()->len : -1);

    /* get or cast the string; leave it on the stack for gc protection */
    vm_val_t strval;
    const char *str = G_stk->get(0)->cast_to_string(vmg_ &strval);
    *G_stk->get(0) = strval;

    /* do the insert */
    insert_text(vmg_ self, idx, str + VMB_LEN, vmb_get_len(str), TRUE);

    /* discard gc protection */
    G_stk->discard();

    /* handled */
    retval->set_obj(self);
    return TRUE;
}

/* 
 *   property evaluator - copyChars 
 */
int CVmObjStringBuffer::getp_copyChars(VMG_ vm_obj_id_t self,
                                       vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the insertion index; adjust to 0-based or end-based indexing */
    int32_t idx = CVmBif::pop_int_val(vmg0_);
    idx += (idx < 0 ? get_ext()->len : -1);

    /* get or cast the string; leave it on the stack for gc protection */
    vm_val_t srcval;
    const char *src = G_stk->get(0)->cast_to_string(vmg_ &srcval);
    *G_stk->get(0) = srcval;

    /* 
     *   Get the character length of the new text.  This is the number of
     *   characters we're overwriting (effectively deleting) in the current
     *   buffer.  
     */
    size_t src_bytes = vmb_get_len(src);
    src += VMB_LEN;
    
    /* get the new text's character length */
    utf8_ptr srcp((char *)src);
    size_t src_chars = srcp.len(src_bytes);

    /* splice the text */
    splice_text(vmg_ self, idx, src_chars, src, src_bytes, TRUE);

    /* discard gc protection */
    G_stk->discard();

    /* handled */
    retval->set_obj(self);
    return TRUE;
}

/* 
 *   property evaluator - delete
 */
int CVmObjStringBuffer::getp_delete(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* pop the deletion index and adjust to 0-based or end-based */
    int32_t idx = CVmBif::pop_int_val(vmg0_);
    idx += (idx < 0 ? get_ext()->len : -1);

    /* pop the length, if provided; if not, delete the rest of the string */
    int32_t len;
    if (argc >= 2)
        len = CVmBif::pop_int_val(vmg0_);
    else
        len = get_ext()->len;

    /* do the delete */
    delete_text(vmg_ self, idx, len, TRUE);

    /* handled */
    retval->set_obj(self);
    return TRUE;
}

/* 
 *   property evaluator - splice
 */
int CVmObjStringBuffer::getp_splice(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(3);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the insertion index and adjust to 0-based or end-based */
    int32_t idx = CVmBif::pop_int_val(vmg0_);
    idx += (idx < 0 ? get_ext()->len : -1);

    /* pop the deletion length */
    int32_t del = CVmBif::pop_int_val(vmg0_);

    /* get or cast the string; leave it on the stack for gc protection */
    vm_val_t strval;
    const char *str = G_stk->get(0)->cast_to_string(vmg_ &strval);
    *G_stk->get(0) = strval;

    /* do the insert */
    splice_text(vmg_ self, idx, del, str + VMB_LEN, vmb_get_len(str), TRUE);

    /* discard gc protection */
    G_stk->discard();

    /* handled */
    retval->set_obj(self);
    return TRUE;
}

/* 
 *   property evaluator - substr
 */
int CVmObjStringBuffer::getp_substr(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* remember the argument count */
    uint argc = (oargc != 0 ? *oargc : 0);
    
    /* check arguments */
    static CVmNativeCodeDesc desc(1, 2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the starting index, and adjust for 0-based or end-based */
    int32_t idx = CVmBif::pop_long_val(vmg0_);
    idx += (idx < 0 ? get_ext()->len : -1);

    /* if there's a length, pop it; otherwise use the rest of the string */
    int32_t len = get_ext()->len;
    if (argc >= 2)
        len = CVmBif::pop_long_val(vmg0_);

    /* retrieve the substring */
    substr_to_string(vmg_ retval, idx, len);

    /* handled */
    return TRUE;
}

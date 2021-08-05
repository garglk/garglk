/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstrcmp.cpp - T3 String Comparator intrinsic class
Function
  
Notes
  
Modified
  09/05/02 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <os.h>
#include "utf8.h"
#include "vmuni.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmmeta.h"
#include "vmglob.h"
#include "vmstrcmp.h"
#include "vmstack.h"
#include "vmbif.h"
#include "vmfile.h"
#include "vmlst.h"
#include "vmstr.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   Statics 
 */

/* metaclass registration */
static CVmMetaclassStrComp metaclass_reg_obj;
CVmMetaclass *CVmObjStrComp::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjStrComp::
     *CVmObjStrComp::func_table_[])(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc) =
{
    &CVmObjStrComp::getp_undef,
    &CVmObjStrComp::getp_calc_hash,
    &CVmObjStrComp::getp_match_values
};

/* ------------------------------------------------------------------------ */
/*
 *   notify of deletion 
 */
void CVmObjStrComp::notify_delete(VMG_ int in_root_set)
{
    /* delete my extension data */
    delete_ext(vmg0_);
}

/*
 *   Delete my extension data 
 */
void CVmObjStrComp::delete_ext(VMG0_)
{
    vmobj_strcmp_ext *ext = get_ext();

    /* if I have an extension, delete it */
    if (ext != 0)
    {
        size_t i;
        
        /* delete each first-level mapping table */
        for (i = 0 ; i < countof(ext->equiv) ; ++i)
        {
            /* if this table is present, delete it */
            if (ext->equiv[i] != 0)
                t3free(ext->equiv[i]);
        }

        /* delete and forget our extension */
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjStrComp::set_prop(VMG_ class CVmUndo *,
                             vm_obj_id_t, vm_prop_id_t,
                             const vm_val_t *)
{
    /* we have no properties to set */
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a property 
 */
int CVmObjStrComp::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                            vm_obj_id_t self, vm_obj_id_t *source_obj,
                            uint *argc)
{
    uint func_idx;

    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the function, if we found it */
    if ((this->*func_table_[func_idx])(vmg_ self, val, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* not found - inherit default handling */
    return CVmObject::get_prop(vmg_ prop, val, self, source_obj, argc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Abstract equivalence mapping reader.  This can be implemented on
 *   different underlying data sources.  
 */
class CVmObjStrCompMapReader
{
public:
    /* 
     *   Read the next equivalence mapping.  We fill in *ref_ch with the
     *   reference character, *uc_result_flags with the upper-case result
     *   flags, and *lc_result_flags with the lower-case result flags.  On
     *   input, *value_ch_cnt is the maximum number of value characters we
     *   can store in the buffer at val_buf; on return, this is the number
     *   of characters in the mapping, which might be higher than the number
     *   originally in the buffer.  In any case, we will write no more than
     *   the allowed buffer size as given by *value_ch_cnt on input (so we
     *   might indicate a higher number than we actually wrote: we always
     *   return the actual value mapping size, even if we couldn't store the
     *   whole thing because of a lack of buffer space).
     *   
     *   This routine should retrieve one mapping each time it's called, and
     *   then move on to the next mapping.  The caller is responsible for
     *   making sure that this routine is called the correct number of
     *   times, so we don't have to worry about running out of mappings.  
     */
    virtual void read_mapping(VMG_ wchar_t *ref_ch,
                              unsigned long *uc_result_flags,
                              unsigned long *lc_result_flags,
                              wchar_t *val_buf, size_t *value_ch_cnt) = 0;
};

/*
 *   Stream-based mapping reader.  This reads mappings from a stream that
 *   uses our serialization format for image and saved-state files.  
 */
class CVmObjStrCompMapReaderStream: public CVmObjStrCompMapReader
{
public:
    CVmObjStrCompMapReaderStream(CVmStream *str) { str_ = str; }

    /* read a mapping */
    virtual void read_mapping(VMG_ wchar_t *ref_ch,
                              unsigned long *uc_result_flags,
                              unsigned long *lc_result_flags,
                              wchar_t *val_buf, size_t *value_ch_cnt)
    {
        size_t copy_limit;
        size_t copy_size;
        size_t i;

        /* limit our value character copying to the actual buffer size */
        copy_limit = *value_ch_cnt;
        
        /* read the header values */
        *ref_ch = (wchar_t)str_->read_uint2();
        copy_size = *value_ch_cnt = str_->read_byte();
        *uc_result_flags = str_->read_uint4();
        *lc_result_flags = str_->read_uint4();

        /* limit copying to the actual buffer size */
        if (copy_size > copy_limit)
            copy_size = copy_limit;

        /* read the values */
        for (i = 0 ; i < copy_size ; ++i)
            *val_buf++ = (wchar_t)str_->read_uint2();

        /* skip any values from the input that we weren't able to store */
        for ( ; i < *value_ch_cnt ; ++i)
            str_->read_uint2();
    }

protected:
    /* my stream */
    CVmStream *str_;
};

/*
 *   Constructor-list mapping reader.  This reads mappings from list data in
 *   the format that we use in our constructor. 
 */
class CVmObjStrCompMapReaderList: public CVmObjStrCompMapReader
{
public:
    CVmObjStrCompMapReaderList(const vm_val_t *lst)
    {
        /* remember the list */
        lst_ = lst;

        /* start at the first element */
        idx_ = 1;
    }
    
    /* read a mapping */
    virtual void read_mapping(VMG_ wchar_t *ref_ch,
                              unsigned long *uc_result_flags,
                              unsigned long *lc_result_flags,
                              wchar_t *val_buf, size_t *value_ch_cnt)
    {
        vm_val_t val;
        vm_val_t sublst;
        const char *refstr;
        const char *valstr;
        size_t copy_rem;
        size_t rem;
        utf8_ptr p;

        /* note the size limit of the caller's value string buffer */
        copy_rem = *value_ch_cnt;

        /* retrieve the next element of the list */
        lst_->ll_index(vmg_ &sublst, idx_);

        /* get the value as a sublist */
        if (!sublst.is_listlike(vmg0_) || sublst.ll_length(vmg0_) < 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* retrieve the reference character string (sublst's 1st element) */
        sublst.ll_index(vmg_ &val, 1);
        refstr = val.get_as_string(vmg0_);

        /* retrieve the value string (sublst's 2nd element) */
        sublst.ll_index(vmg_ &val, 2);
        valstr = val.get_as_string(vmg0_);

        /* make sure the reference and value strings are indeed strings */
        if (refstr == 0 || valstr == 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* we need at least one character in each string */
        if (vmb_get_len(refstr) == 0 || vmb_get_len(valstr) == 0)
            err_throw(VMERR_BAD_VAL_BIF);

        /* fill in the caller's reference character from the ref string */
        *ref_ch = utf8_ptr::s_getch(refstr + VMB_LEN);

        /* store the characters of the value string, up to the buffer limit */
        p.set((char *)valstr + VMB_LEN);
        rem = vmb_get_len(valstr);
        for (*value_ch_cnt = 0 ; rem != 0 ; p.inc(&rem))
        {
            /* if we have room, copy this character */
            if (copy_rem != 0)
            {
                /* copy the character */
                *val_buf++ = p.getch();

                /* count it */
                --copy_rem;
            }

            /* count it in the actual length */
            ++(*value_ch_cnt);
        }

        /* get the upper-case flags (sublst's 3rd element) */
        sublst.ll_index(vmg_ &val, 3);
        if (val.typ != VM_INT)
            err_throw(VMERR_BAD_TYPE_BIF);
        *uc_result_flags = val.val.intval;

        /* get the lower-case flags (sublst's 4th element) */
        sublst.ll_index(vmg_ &val, 4);
        if (val.typ != VM_INT)
            err_throw(VMERR_BAD_TYPE_BIF);
        *lc_result_flags = val.val.intval;

        /* we're done with this mapping, so advance to the next one */
        ++idx_;
    }
    
protected:
    /* my list data */
    const vm_val_t *lst_;

    /* the list index of the next mapping to retrieve */
    size_t idx_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Create from stack.  We take the following constructor arguments:
 *   
 *   trunc_len (int: truncation length)
 *.  case_sensitive (bool: case sensitivity flag)
 *.  mappings (list: equivalence mappings)
 *   
 *   Our equivalence mappings are given as a list of lists; the main list
 *   consists of one sublist per mapping.  Each mapping sublist looks like
 *   this:
 *   
 *   ['ref_char', 'val_string', uc_flags, lc_flag]
 *   
 *   The 'ref_char' is a one-character string giving the reference character
 *   of the mapping, and 'val_string' is a string of one or more characters
 *   that can match the reference character in a value (input) string.
 *   uc_flags and lc_flags are integers giving the upper-case and lower-case
 *   flags (respectively) that are to be added to the match result code when
 *   the mapping is used to match a pair of strings.  
 */
vm_obj_id_t CVmObjStrComp::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    size_t trunc_len;
    int case_sensitive;
    const vm_val_t *lst;
    vm_obj_id_t id;
    CVmObjStrComp *obj;
    size_t equiv_cnt;
    size_t total_chars;
    
    /* check arguments */
    if (argc != 3)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* pop the truncation length parameter */
    if (G_stk->get(0)->typ == VM_NIL)
    {
        /* it's nil, so truncation is not allowed */
        trunc_len = 0;
        G_stk->discard();
    }
    else
    {
        /* retrieve the truncation length as an integer */
        trunc_len = CVmBif::pop_int_val(vmg0_);
    }

    /* get the case sensitivity flag */
    case_sensitive = CVmBif::pop_bool_val(vmg0_);

    /* 
     *   retrieve the mapping list, but leave it on the stack (for gc
     *   protection) 
     */
    if (G_stk->get(0)->typ == VM_NIL)
    {
        /* there are no mappings */
        lst = 0;
        equiv_cnt = 0;
        total_chars = 0;
    }
    else
    {
        size_t i;
        int ec;
        
        /* get the list value from the argument */
        lst = G_stk->get(0);
        if (!lst->is_listlike(vmg0_)
            || (ec = lst->ll_length(vmg0_)) < 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* run through the list and count the value string characters */
        for (i = 1, total_chars = 0, equiv_cnt = ec ; i <= equiv_cnt ; ++i)
        {
            vm_val_t sublst;
            vm_val_t val;
            const char *strp;
            utf8_ptr ustrp;

            /* get this mapping from the list */
            lst->ll_index(vmg_ &sublst, i);

            /* make sure it's a sublist */
            if (!sublst.is_listlike(vmg0_))
                err_throw(VMERR_BAD_TYPE_BIF);

            /* 
             *   get the second element of the mapping sublist - this is the
             *   value string 
             */
            sublst.ll_index(vmg_ &val, 2);
            if ((strp = val.get_as_string(vmg0_)) == 0)
                err_throw(VMERR_BAD_TYPE_BIF);

            /* add the character length of the string to the total */
            ustrp.set((char *)strp + VMB_LEN);
            total_chars += ustrp.len(vmb_get_len(strp));
        }
    }

    /* create the new object */
    id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
    obj = new (vmg_ id) CVmObjStrComp();

    /* set up a list-based mapping reader */
    CVmObjStrCompMapReaderList reader(lst);

    /* allocate and initialize the new object's extension */
    obj->alloc_ext(vmg_ trunc_len, case_sensitive, equiv_cnt, total_chars,
                   &reader);

    /* discard the gc protection */
    G_stk->discard();

    /* return the new object */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Load from an image file 
 */
void CVmObjStrComp::load_from_image(VMG_ vm_obj_id_t /*self*/,
                                    const char *ptr, size_t len)
{
    /* load my image data */
    CVmReadOnlyMemoryStream str(ptr, len);
    load_from_stream(vmg_ &str);
}

/*
 *   Load from an abstract stream 
 */
void CVmObjStrComp::load_from_stream(VMG_ CVmStream *str)
{
    unsigned int trunc_len;
    unsigned int flags;
    unsigned int equiv_cnt;
    unsigned int total_chars;
    
    /* load the fixed header */
    trunc_len = str->read_uint2();
    flags = str->read_uint2();
    equiv_cnt = str->read_uint2();
    total_chars = str->read_uint2();

    /* set up a stream-based mapping reader */
    CVmObjStrCompMapReaderStream reader(str);

    /* allocate and initialize our extension */
    alloc_ext(vmg_ trunc_len, (flags & 0x0001) != 0, equiv_cnt, total_chars,
              &reader);
}

/*
 *   Allocate and initialize our extension 
 */
void CVmObjStrComp::alloc_ext(VMG_ size_t trunc_len, int case_sensitive,
                              size_t equiv_cnt, size_t total_chars,
                              CVmObjStrCompMapReader *reader)
{
    size_t siz;
    vmobj_strcmp_ext *ext;
    vmobj_strcmp_equiv *nxt_equiv;
    wchar_t *nxt_ch;
    size_t ch_rem;
    size_t i;
    size_t idx1, idx2;

    /* delete my extension, if I have one already */
    delete_ext(vmg0_);

    /* 
     *   Calculate how much space we need for our extension.  In addition to
     *   the fixed part, allocate space for one vmobj_strcmp_equiv structure
     *   per mapping, plus the wchar_t's for the value mappings.  
     */
    siz = sizeof(vmobj_strcmp_ext)
          + (equiv_cnt * sizeof(vmobj_strcmp_equiv))
          + (total_chars * sizeof(wchar_t));

    /* allocate our new extension */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(siz, this);
    ext = get_ext();

    /* 
     *   set up our suballocation pool pointers: put the equivalence mapping
     *   structures after the fixed part of the extension, and put the
     *   wchar_t's after the equivalence mappings 
     */
    nxt_equiv = (vmobj_strcmp_equiv *)(ext + 1);
    nxt_ch = (wchar_t *)(&nxt_equiv[equiv_cnt]);
    ch_rem = total_chars;

    /* initialize the extension structure */
    ext->trunc_len = trunc_len;
    ext->case_sensitive = case_sensitive;

    /* 
     *   we have no equivalence mappings installed yet, so clear out the
     *   first tier of the mapping array 
     */
    for (i = 0 ; i < countof(ext->equiv) ; ++i)
        ext->equiv[i] = 0;

    /* load the mappings */
    for (i = 0 ; i < equiv_cnt ; ++i, ++nxt_equiv)
    {
        wchar_t ref_ch;

        /* 
         *   set up our equivalent's value buffer with the remainder of our
         *   main buffer 
         */
        nxt_equiv->val_ch = nxt_ch;
        nxt_equiv->val_ch_cnt = ch_rem;

        /* read the mapping */
        reader->read_mapping(vmg_ &ref_ch,
                             &nxt_equiv->uc_result_flags,
                             &nxt_equiv->lc_result_flags,
                             nxt_equiv->val_ch, &nxt_equiv->val_ch_cnt);

        /* deduct this mapping from our main value character buffer */
        nxt_ch += nxt_equiv->val_ch_cnt;
        ch_rem -= nxt_equiv->val_ch_cnt;

        /* if we don't have a first-tier table for this character, add one */
        idx1 = (ref_ch >> 8) & 0xFF;
        idx2 = (ref_ch & 0xFF);
        if (ext->equiv[idx1] == 0)
        {
            vmobj_strcmp_equiv **p;
            size_t j;
            
            /* allocate a first-tier table for this index */
            ext->equiv[idx1] = p = (vmobj_strcmp_equiv **)t3malloc(
                256 * sizeof(vmobj_strcmp_equiv *));

            /* clear out the first-tier table */
            for (j = 0 ; j < 256 ; ++j, ++p)
                *p = 0;
        }

        /* set the mapping for this character */
        ext->equiv[idx1][idx2] = nxt_equiv;
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjStrComp::save_to_file(VMG_ class CVmFile *fp)
{
    /* write our data to the file */
    CVmFileStream str(fp);
    write_to_stream(vmg_ &str, 0);
}

/*
 *   Serialize to a stream 
 */
ulong CVmObjStrComp::write_to_stream(VMG_ CVmStream *str, ulong *bytes_avail)
{
    wchar_t ref_ch_base;
    vmobj_strcmp_ext *ext = get_ext();
    size_t i;
    vmobj_strcmp_equiv ***p;
    size_t total_value_ch;
    size_t equiv_cnt;
    size_t need_size;

    /* get the mapping totals */
    count_equiv_mappings(&equiv_cnt, &total_value_ch);

    /* 
     *   Calculate our space needs.  We need 8 bytes for the fixed header,
     *   11 bytes per equivalent mapping, and 2 bytes per value string
     *   character. 
     */
    need_size = 8 + (11 * equiv_cnt) + (2 * total_value_ch);
    
    /* if we have a size limit, check to make sure we can abide by it */
    if (bytes_avail != 0 && need_size > *bytes_avail)
    {
        /* 
         *   there's not enough space in the output stream for us, so don't
         *   write anything at all; simply return the amount of space we
         *   need 
         */
        return need_size;
    }

    /* write out the serialization structure header */
    str->write_uint2(ext->trunc_len);
    str->write_uint2(ext->case_sensitive ? 0x0001 : 0x0000);
    str->write_uint2(equiv_cnt);
    str->write_uint2(total_value_ch);

    /* run through our equivalence table again and write the mappings */
    for (ref_ch_base = 0, i = 0, p = ext->equiv ; i < countof(ext->equiv) ;
         ++i, ++p, ref_ch_base += 256)
    {
        vmobj_strcmp_equiv **ep;
        size_t j;

        /* if this first-tier mapping is unused, skip it */
        if (*p == 0)
            continue;

        /* run through our second-level table */
        for (j = 0, ep = *p ; j < 256 ; ++j, ++ep)
        {
            /* if this mapping is used, write it out */
            if (*ep != 0)
            {
                size_t k;
                wchar_t *vp;
                
                /* write the fixed part of the mapping */
                str->write_uint2(ref_ch_base + j);
                str->write_byte((uchar)(*ep)->val_ch_cnt);
                str->write_uint4((*ep)->uc_result_flags);
                str->write_uint4((*ep)->lc_result_flags);

                /* write the value mapping characters */
                for (k = (*ep)->val_ch_cnt, vp = (*ep)->val_ch ; k != 0 ;
                     --k, ++vp)
                {
                    /* write this character */
                    str->write_uint2(*vp);
                }
            }
        }
    }

    /* return our space needs */
    return need_size;
}

/*
 *   Count the equivalence mappings. 
 */
void CVmObjStrComp::count_equiv_mappings(size_t *equiv_cnt,
                                         size_t *total_value_ch)
{
    vmobj_strcmp_ext *ext = get_ext();
    size_t i;
    vmobj_strcmp_equiv ***p;

    /* run through our table and count up the mappings */
    for (*total_value_ch = 0, *equiv_cnt = 0, i = 0, p = ext->equiv ;
         i < countof(ext->equiv) ; ++i, ++p)
    {
        vmobj_strcmp_equiv **ep;
        size_t j;

        /* if this first-tier mapping is unused, skip it */
        if (*p == 0)
            continue;

        /* run through our second-level table */
        for (j = 0, ep = *p ; j < 256 ; ++j, ++ep)
        {
            /* if this mapping is used, count it */
            if (*ep != 0)
            {
                /* count this equivalent mapping */
                ++(*equiv_cnt);

                /* count its value mapping characters in the total */
                *total_value_ch += (*ep)->val_ch_cnt;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   restore from a file 
 */
void CVmObjStrComp::restore_from_file(VMG_ vm_obj_id_t /*self*/,
                                      class CVmFile *fp, CVmObjFixup *)
{
    /* load from the file */
    CVmFileStream str(fp);
    load_from_stream(vmg_ &str);
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - calculate a hash value
 */
int CVmObjStrComp::getp_calc_hash(VMG_ vm_obj_id_t /*self*/,
                                  vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    const char *strp;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   retrieve the string argument (it must be a string), but leave it on
     *   the stack for now, for gc protection 
     */
    strp = G_stk->get(0)->get_as_string(vmg0_);
    if (strp == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* calculate the hash value and return it */
    retval->set_int(calc_str_hash(strp + VMB_LEN, vmb_get_len(strp)));

    /* discard gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Hash adder 
 */
struct StrCompHashAdder
{
    StrCompHashAdder(size_t trunc_len)
    {
        this->trunc_len = trunc_len;
        this->has_trunc = (trunc_len != 0);
        hash = 0;
    }

    int add(wchar_t ch)
    {
        /* add the character to the hash */
        hash += ch;
        hash &= 0xFFFF;

        /* if there's a truncation limit, count this against the limit */
        if (has_trunc && --trunc_len == 0)
        {
            /* we've reached the limit */
            return FALSE;
        }
        else
        {
            /* haven't reached the limit yet */
            return TRUE;
        }
    }

    int done() const { return has_trunc && trunc_len == 0; }

    /* truncation limit */
    size_t trunc_len;

    /* is there a truncation limit? */
    int has_trunc;

    /* the hash code */
    unsigned int hash;
};

/*
 *   Calculate a hash value 
 */
unsigned int CVmObjStrComp::calc_str_hash(const char *strp, size_t len)
{
    /* get my extension */
    vmobj_strcmp_ext *ext = get_ext();

    /* set up to scan the string */
    utf8_ptr p((char *)strp);

    /* 
     *   Scan the string.  Limit the scan to our truncation length, counting
     *   substitution expansions and case folding expansions, because we
     *   don't want to distinguish hash buckets beyond the truncation point.
     *   If we did, a truncated string wouldn't hash into the same bucket as
     *   a longer string it matches; all matching strings are required to go
     *   into the same bucket, so we can't have such a hash mismatch.
     */
    StrCompHashAdder hash(ext->trunc_len);
    for ( ; len != 0 ; p.inc(&len))
    {
        /* get the current character */
        wchar_t ch = p.getch();
        
        /* check for a substitution mapping for this character */
        vmobj_strcmp_equiv **t1, *eq;
        if ((t1 = ext->equiv[(ch >> 8) & 0xFF]) != 0
            && (eq = t1[ch & 0xFF]) != 0)
        {
            /* 
             *   This character has a mapping, so add the contribution from
             *   the canonical form of the character, which is the value
             *   side of the mapping.  
             */
            wchar_t *vp;
            size_t vlen;
            for (vp = eq->val_ch, vlen = eq->val_ch_cnt ; vlen != 0 ;
                 ++vp, --vlen)
            {
                /* get this character */
                ch = *vp;

                /* 
                 *   use case folding if we're insensitive to case and the
                 *   character has a case folding
                 */
                const wchar_t *f;
                if (!ext->case_sensitive && (f = t3_to_fold(ch)) != 0)
                {
                    /* add the folded expansion to the hash */
                    for (const wchar_t *f = t3_to_fold(ch) ; *f != 0 ; ++f)
                    {
                        if (!hash.add(*f))
                            return hash.hash;
                    }
                }
                else
                {
                    if (!hash.add(ch))
                        return hash.hash;
                }
            }
        }
        else
        {
            /* 
             *   if we're not sensitive to case, and there's a case folding
             *   available, use the case-folded representation of a character
             *   for its hash value 
             */
            const wchar_t *f;
            if (!ext->case_sensitive && (f = t3_to_fold(ch)) != 0)
            {
                /* add the folded expansion to the hash */
                for (const wchar_t *f = t3_to_fold(ch) ; *f != 0 ; ++f)
                {
                    if (!hash.add(*f))
                        return hash.hash;
                }
            }
            else
            {
                /* exact case only - add the character as-is */
                if (!hash.add(ch))
                    return hash.hash;
            }
        }
    }

    /* return the hash code */
    return hash.hash;
}

/* ------------------------------------------------------------------------ */
/*
 *   Pre-defined return flag values. 
 */

#define RF_MATCH     0x0001                           /* the string matched */
#define RF_CASEFOLD  0x0002                    /* matched with case folding */
#define RF_TRUNC     0x0004                      /* matched with truncation */


/* 
 *   property evaluator - calculate a hash value 
 */
int CVmObjStrComp::getp_match_values(VMG_ vm_obj_id_t /*self*/,
                                     vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(2);
    const char *valstr;
    const char *refstr;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the strings, but leave them on the stack for gc protection */
    valstr = G_stk->get(0)->get_as_string(vmg0_);
    refstr = G_stk->get(1)->get_as_string(vmg0_);

    /* make sure they're valid strings */
    if (valstr == 0 || refstr == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* compare the strings and return the result */
    retval->set_int(match_strings(valstr + VMB_LEN, vmb_get_len(valstr),
                                  refstr + VMB_LEN, vmb_get_len(refstr)));

    /* discard the gc protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/*
 *   Match two strings
 */
unsigned long CVmObjStrComp::match_strings(const char *valstr, size_t vallen,
                                           const char *refstr, size_t reflen)
{
    vmobj_strcmp_ext *ext = get_ext();
    utf8_ptr valp;
    utf8_ptr refp;
    unsigned long ret;
    int fold_case = !(ext->case_sensitive);

    /* set up to scan the strings */
    valp.set((char *)valstr);
    refp.set((char *)refstr);

    /* start with no return flags */
    ret = 0;

    /* scan the strings */
    while (vallen != 0 && reflen != 0)
    {
        /* get each character */
        wchar_t valch = valp.getch();
        wchar_t refch = refp.getch();

        /* check for an exact match first */
        if (refch == valch)
        {
            /* it's an exact match - skip this character in each string */
            valp.inc(&vallen);
            refp.inc(&reflen);
            continue;
        }

        /* check for a case-folded match if we're insensitive to case */
        if (fold_case
            && t3_compare_case_fold_min(valp, vallen, refp, reflen) == 0)
        {
            /* note in the flags that we have differing cases in the match */
            ret |= RF_CASEFOLD;

            /* keep going */
            continue;
        }

        /* check for a reference equivalence mapping */
        vmobj_strcmp_equiv **t1;
        vmobj_strcmp_equiv *eq;
        if ((t1 = ext->equiv[(refch >> 8) & 0xFF]) != 0
            && (eq = t1[refch & 0xFF]) != 0)
        {
            /* 
             *   In case we match, apply the appropriate flags added for the
             *   equivalence mapping, based on the case of the first value
             *   character we're testing.  (If we don't match, we'll simply
             *   return failure, so it won't matter that we messed with the
             *   flags.)  
             */
            ret |= (t3_is_upper(valch)
                    ? eq->uc_result_flags
                    : eq->lc_result_flags);

            /* match each character from the mapping string */
            const wchar_t *vp;
            size_t vlen;
            for (vp = eq->val_ch, vlen = eq->val_ch_cnt ;
                 vallen != 0 && vlen != 0 ; )
            {
                /* get this character */
                refch = *vp;
                
                /* if we have an exact match, keep going */
                if (refch == valch)
                {
                    /* matched - skip this character in each string */
                    valp.inc(&vallen);
                    ++vp, --vlen;

                    /* keep going */
                    continue;
                }

                /* check for a case-folded match if appropriate */
                if (fold_case
                    && t3_compare_case_fold_min(valp, vallen, vp, vlen) == 0)
                {
                    /* note the case-folded match and keep going */
                    ret |= RF_CASEFOLD;
                    continue;
                }

                /* no match */
                return 0;
            }

            /* 
             *   if we didn't make it to the end of the equivalence string,
             *   we must have run out of source before we matched the whole
             *   string, so we don'to have a match 
             */
            if (vlen != 0)
                return 0;

            /* 
             *   If we make it here, we matched the equivalence mapping.
             *   We've already skipped the input we matched, so skip the
             *   reference character and keep going.  
             */
            refp.inc(&reflen);
            continue;
        }

        /* we don't have anything else to try, so we don't have a match */
        return 0;
    }

    /* 
     *   If we ran out of reference string before we ran out of value
     *   string, we definitely do not have a match.  If we ran out of value
     *   string before we ran out reference string, we have a match as long
     *   as we matched at least the truncation length. 
     */
    if (reflen == 0 && vallen == 0)
    {
        /* 
         *   We ran out of both at the same time - it's a match.  Return the
         *   result code up to this point OR'd with RF_MATCH, which is our
         *   pre-defined bit that we set for every match.  
         */
        return (ret | RF_MATCH);
    }
    else if (vallen != 0)
    {
        /* we ran out of reference string first - it's not a match */
        return 0;
    }
    else
    {
        /* 
         *   We ran out of value string first, so it's a truncated match if
         *   we matched at least up to the truncation length (assuming we
         *   allow truncation at all).  If we didn't make it to the
         *   truncation length, or we don't allow truncation, it's not a
         *   match. 
         */
        size_t valcharlen = utf8_ptr::s_len(valstr, valp.getptr() - valstr);
        if (ext->trunc_len != 0 && valcharlen >= ext->trunc_len)
        {
            /* 
             *   it's a truncated match - return the result code up to this
             *   point, OR'd with RF_MATCH (our pre-defined bit we set for
             *   every match) and RF_TRUNC (our pre-defined bit we set for
             *   truncated matches) 
             */
            return (ret | RF_MATCH | RF_TRUNC);
        }
        else
        {
            /* didn't make it to the truncation length, so it's not a match */
            return 0;
        }
    }
}

/* 
 *   Check for an approximation match to a given character.  This checks the
 *   given input string for a match to the approximation for a given
 *   reference character.  Returns the number of characters in the match, or
 *   zero if there's no match.  
 */
size_t CVmObjStrComp::match_chars(const wchar_t *valstr, size_t vallen,
                                  wchar_t refch)
{
    vmobj_strcmp_ext *ext = get_ext();
    int fold_case = !(ext->case_sensitive);
    vmobj_strcmp_equiv **t1;
    vmobj_strcmp_equiv *eq;
    wchar_t valch = *valstr;
    const wchar_t *start = valstr;

    /* check for an exact match first */
    if (refch == valch)
        return 1;
            
    /* check for a case-folded match if we're insensitive to case */
    if (fold_case)
    {
        /* try a minimum case-folded match to the one-character ref string */
        size_t ovallen = vallen, reflen = 1;
        const wchar_t *refstr = &refch;
        if (t3_compare_case_fold_min(valstr, vallen, refstr, reflen) == 0)
        {
            /* got it - return the length we consumed from the value string */
            return ovallen - vallen;
        }
    }

    /* check for a reference equivalence mapping */
    if ((t1 = ext->equiv[(refch >> 8) & 0xFF]) != 0
        && (eq = t1[refch & 0xFF]) != 0)
    {
        /* match each character from the mapping string */
        const wchar_t *vp;
        size_t vlen;
        for (vp = eq->val_ch, vlen = eq->val_ch_cnt ;
             vallen != 0 && vlen != 0 ; )
        {
            /* get this mapping character */
            refch = *vp;

            /* if we have an exact match, keep going */
            if (refch == valch)
            {
                /* matched exactly - skip this character and keep going */
                ++valstr, --vallen;
                ++vp, --vlen;
                continue;
            }

            /* check for a case-folded match if appropriate */
            if (fold_case
                && t3_compare_case_fold_min(valstr, vallen, vp, vlen) == 0)
            {
                /* matched - keep going */
                continue;
            }

            /* no match */
            return 0;
        }

        /* we've matched the mapping - return the match length */
        return valstr - start;
    }

    /* it doesn't match any of the possibilities */
    return 0;
}

/*
 *   Get the truncation length 
 */
size_t CVmObjStrComp::trunc_len() const
{
    return get_ext()->trunc_len;
}

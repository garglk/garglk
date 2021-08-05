/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbytarr.cpp - TADS 3 ByteArray intrinsic class
Function
  
Notes
  
Modified
  06/05/01 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <assert.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"
#include "vmbytarr.h"
#include "vmbif.h"
#include "vmfile.h"
#include "vmerrnum.h"
#include "vmerr.h"
#include "vmstack.h"
#include "vmmeta.h"
#include "vmundo.h"
#include "vmrun.h"
#include "charmap.h"
#include "utf8.h"
#include "vmstr.h"
#include "vmcset.h"
#include "vmdatasrc.h"
#include "vmpack.h"
#include "sha2.h"
#include "md5.h"


/* ------------------------------------------------------------------------ */
/*
 *   Byte Array data source 
 */
class CVmByteArraySource: public CVmDataSource
{
public:
    /* set up a data source on the entire range of the array */
    CVmByteArraySource(VMG_ vm_obj_id_t arr)
    {
        /* remember the array and globals */
        this->vmg = VMGLOB_ADDR;
        this->arr_id = arr;
        this->arr = (CVmObjByteArray *)vm_objp(vmg_ arr);

        /* we provide access to the entire range of the array */
        this->idx = 0;
        this->start_idx = 0;
        this->end_idx = this->arr->get_element_count();
    }

    /* set up a data source on a slice of the array */
    CVmByteArraySource(VMG_ vm_obj_id_t arr, long start_idx, long len)
    {
        /* remember the array and globals */
        this->vmg = VMGLOB_ADDR;
        this->arr_id = arr;
        this->arr = (CVmObjByteArray *)vm_objp(vmg_ arr);

        /* remember our window */
        this->idx = start_idx;
        this->start_idx = start_idx;
        this->end_idx = start_idx + len;

        /* limit the window to the actual available size */
        if ((unsigned long)end_idx > this->arr->get_element_count())
            end_idx = this->arr->get_element_count();
    }

    CVmDataSource *clone(VMG_ const char * /*mode*/)
    {
        return new CVmByteArraySource(
            vmg_ arr_id, start_idx, end_idx - start_idx);
    }

    /* read bytes; returns 0 on success, non-zero on error */
    virtual int read(void *buf, size_t len)
    {
        /* limit the read to the available length */
        size_t rdlen = (idx + (long)len > end_idx
                        ? (size_t)(end_idx - idx) : len);

        /* copy the requested bytes starting at the current index */
        size_t copied = arr->copy_to_buf((unsigned char *)buf, idx + 1, rdlen);

        /* move past the bytes copied */
        idx += copied;

        /* we were successful if we copied exactly the requested length */
        return copied != len;
    }

    /* read bytes; returns the number of bytes read */
    virtual int readc(void *buf, size_t len)
    {
        /* limit the read to the available length */
        size_t rdlen = (idx + (long)len > end_idx
                        ? (size_t)(end_idx - idx) : len);

        /* copy the requested bytes */
        size_t copied = arr->copy_to_buf((unsigned char *)buf, idx + 1, rdlen);

        /* move past the bytes copied */
        idx += copied;

        /* return the number of bytes actually read */
        return copied;
    }

    /* write bytes; returns 0 on success, non-zero on error */
    virtual int write(const void *buf, size_t len)
    {
        /* establish global access */
        VMGLOB_PTR(vmg);
        
        /* limit the write to the available length */
        size_t wrtlen = (idx + (long)len > end_idx
                         ? (size_t)(end_idx - idx) : len);

        /* copy the requested bytes (saving undo) */
        size_t copied = arr->copy_from_buf_undo(
            vmg_ arr_id, (const unsigned char *)buf, idx + 1, wrtlen);

        /* move past the bytes copied */
        idx += copied;

        /* we were successful if we copied exactly the requested length */
        return copied != len;
    }

    /* get the length of our slice of the array in bytes */
    virtual long get_size() { return end_idx - start_idx; }

    /* get the current seek location */
    virtual long get_pos() { return idx - start_idx; }

    /* set the current seek location - 'mode' is an OSFSK_xxx mode */
    virtual int seek(long ofs, int mode)
    {
        switch (mode)
        {
        case OSFSK_SET:
            idx = start_idx + ofs;
            break;

        case OSFSK_CUR:
            idx += ofs;
            break;

        case OSFSK_END:
            idx = end_idx + ofs;
            break;

        default:
            return 1;
        }

        /* make sure it's in range */
        if (idx < start_idx)
            idx = start_idx;
        else if (idx > end_idx)
            idx = end_idx;

        /* success */
        return 0;
    }

    /* flush - returns 0 on success, non-zero on error */
    virtual int flush() { return 0; }

    /* close the underlying system resource */
    virtual void close() { }

protected:
    /* our underlying array */
    vm_obj_id_t arr_id;
    CVmObjByteArray *arr;

    /* current read/write index in the array */
    long idx;

    /* the bounds of the slice of the array we present as the file */
    long start_idx;
    long end_idx;

    /* we need globals in some interface routines without the VMG_ param */
    vm_globals *vmg;
};



/* ------------------------------------------------------------------------ */
/*
 *   Integer format codes.  A full format code is given by a bitwise
 *   combination of size, byte order, and signedness.  
 */

/* integer sizes */
#define FmtSizeMask           0x000F
#define FmtInt8               0x0000
#define FmtInt16              0x0001
#define FmtInt32              0x0002

/* integer byte orders */
#define FmtOrderMask          0x00F0
#define FmtLittleEndian       0x0000
#define FmtBigEndian          0x0010

/* integer signedness */
#define FmtSignedMask         0x0F00
#define FmtSigned             0x0000
#define FmtUnsigned           0x0100

/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassByteArray metaclass_reg_obj;
CVmMetaclass *CVmObjByteArray::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjByteArray::
     *CVmObjByteArray::func_table_[])(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *argc) =
{
    &CVmObjByteArray::getp_undef,                                      /* 0 */
    &CVmObjByteArray::getp_length,                                     /* 1 */
    &CVmObjByteArray::getp_subarray,                                   /* 2 */
    &CVmObjByteArray::getp_copy_from,                                  /* 3 */
    &CVmObjByteArray::getp_fill_val,                                   /* 4 */
    &CVmObjByteArray::getp_to_string,                                  /* 5 */
    &CVmObjByteArray::getp_read_int,                                   /* 6 */
    &CVmObjByteArray::getp_write_int,                                  /* 7 */
    &CVmObjByteArray::getp_packBytes,                                  /* 8 */
    &CVmObjByteArray::getp_unpackBytes,                                /* 9 */
    &CVmObjByteArray::getp_sha256,                                    /* 10 */
    &CVmObjByteArray::getp_digestMD5                                  /* 11 */
};

/* static property indices */
const int PROPIDX_packBytes = 8;


/* ------------------------------------------------------------------------ */
/*
 *   Create from stack 
 */
vm_obj_id_t CVmObjByteArray::create_from_stack(VMG_ const uchar **pc_ptr,
                                               uint argc)
{
    vm_obj_id_t id = VM_INVALID_OBJ;
    CVmObjByteArray *arr;
    unsigned long cnt;
    vm_val_t *arg1;
    const char *str;

    /* check our arguments */
    if (argc < 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* see what we have for the first argument */
    arg1 = G_stk->get(0);
    if (arg1->typ == VM_INT)
    {
        /* 
         *   it's a simple count argument - make sure we only have one
         *   argument 
         */
        if (argc != 1)
            err_throw(VMERR_WRONG_NUM_OF_ARGS);

        /* get the number of elements */
        cnt = (unsigned long)arg1->val.intval;

        /* create the array with the given number of elements */
        id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
        arr = new (vmg_ id) CVmObjByteArray(vmg_ cnt);

        /* set each element to zero */
        arr->fill_with(0, 1, cnt);
    }
    else if ((str = arg1->get_as_string(vmg0_)) != 0)
    {
        /*
         *   String argument - we're creating a byte array from the string.
         *   We can map it from a character set, or we can just stuff the
         *   character values into bytes.  Check arguments - we must have one
         *   or two of them (the string, and the optional character set) 
         */
        if (argc < 1 || argc > 2)
            err_throw(VMERR_WRONG_NUM_OF_ARGS);

        /* fetch the character mapper, if there is one */
        const vm_val_t *cmap = (argc >= 2 ? G_stk->get(1) : 0);

        /* create the byte array */
        id = create_from_string(vmg_ G_stk->get(0), str, cmap);
    }
    else if (arg1->typ == VM_OBJ && is_byte_array(vmg_ arg1->val.obj))
    {
        unsigned long src_idx;
        unsigned long src_cnt;

        /* remember the source array */
        CVmObjByteArray *src_arr =
            (CVmObjByteArray *)vm_objp(vmg_ arg1->val.obj);

        /* get the count from the array */
        src_cnt = src_arr->get_element_count();

        /* 
         *   check for the optional actual element count, and the optional
         *   starting index 
         */
        if (argc == 1)
        {
            /* no size specified - use the same size as the original */
            cnt = src_cnt;

            /* start at the first element */
            src_idx = 1;
        }
        else
        {
            /* make sure it's an integer */
            if (G_stk->get(1)->typ != VM_INT)
                err_throw(VMERR_INT_VAL_REQD);

            /* use the specified starting index */
            src_idx = (unsigned long)G_stk->get(1)->val.intval;

            /* if the index is below 1, force it to 1 */
            if (src_idx < 1)
                src_idx = 1;

            /* check for the starting element argument */
            if (argc >= 3)
            {
                /* make sure it's an integer */
                if (G_stk->get(2)->typ != VM_INT)
                    err_throw(VMERR_INT_VAL_REQD);

                /* use the specified element count */
                cnt = (unsigned long)G_stk->get(2)->val.intval;

                /* make sure we don't have any extra arguments */
                if (argc > 3)
                    err_throw(VMERR_WRONG_NUM_OF_ARGS);
            }
            else
            {
                /* 
                 *   no count specified - use the number of elements in the
                 *   original remaining after the starting index 
                 */
                if (src_idx > src_cnt)
                    cnt = 0;
                else
                    cnt = src_cnt + 1 - src_idx;
            }
        }

        /* create the new array */
        id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
        arr = new (vmg_ id) CVmObjByteArray(vmg_ cnt);

        /* copy the source array */
        arr->copy_from(1, src_arr, src_idx, cnt);
    }
    else
    {
        /* invalid argument */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new object */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a new byte array from a string, mapping through the given
 *   character set object if present, or just stuffing the character values
 *   into bytes if not.  
 */
vm_obj_id_t CVmObjByteArray::create_from_string(
    VMG_ const vm_val_t *strval, const char *str, const vm_val_t *mapval)
{
    /* we can't allocate our object until we know the mapped byte count */
    vm_obj_id_t id = VM_INVALID_OBJ;
    CVmObjByteArray *arr = 0;
    
    /* get the string's length and skip the length prefix */
    size_t len = vmb_get_len(str);
    str += VMB_LEN;

    /*
     *   We have two modes: map to bytes via a character set, or stuff
     *   unmapped character values into bytes.  Check which mode we're using.
     */
    if (mapval == 0 || mapval->typ == VM_NIL)
    {
        /* 
         *   No character mapper - just stuff each character into a byte.  If
         *   a character is outside the 0..255 range, it's an error.  
         */

        /* count the number of characters */
        utf8_ptr p((char *)str);
        size_t clen = p.len(len);

        /* allocate the array - each string character is one array byte */
        id = create(vmg_ FALSE, clen);
        arr = (CVmObjByteArray *)vm_objp(vmg_ id);

        /* copy bytes into the array */
        for (int idx = 1 ; len != 0 ; )
        {
            /* translate a buffer-full */
            unsigned char buf[256];
            size_t cur = 0;
            for ( ; len != 0 && cur < sizeof(buf) ; p.inc(&len))
            {
                /* get the next character; make sure it fits in a byte */
                wchar_t ch = p.getch();
                if (ch > 255)
                    err_throw(VMERR_NUM_OVERFLOW);
                
                /* save it */
                buf[cur++] = (unsigned char)ch;
            }
            
            /* copy this chunk to the array */
            arr->cons_copy_from_buf(buf, idx, cur);
            idx += cur;
        }
    }
    else
    {
        /* interpret the character set argument */
        vm_obj_id_t mapid = CVmBif::get_charset_obj(vmg_ mapval);

        /* get the to-local mapping from the character set */
        CCharmapToLocal *mapper =
            ((CVmObjCharSet *)vm_objp(vmg_ mapid))->get_to_local(vmg0_);

        /* 
         *   first, do a mapping with a null output buffer to determine how
         *   many bytes we need for the mapping 
         */
        size_t src_bytes_used;
        size_t byte_len = mapper->map_utf8(0, 0, str, len, &src_bytes_used);
        
        /* allocate a new ByteArray with the required number of bytes */
        id = CVmObjByteArray::create(vmg_ FALSE, byte_len);
        arr = (CVmObjByteArray *)vm_objp(vmg_ id);
        
        /* convert it again, this time storing the bytes */
        for (size_t out_idx = 1 ; len != 0 ; )
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
    }

    /* return the new array's object ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create from binary data 
 */
vm_obj_id_t CVmObjByteArray::create_from_bytes(
    VMG_ int in_root_set, const char *buf, size_t len)
{
    /* allocate the array */
    vm_obj_id_t id = create(vmg_ in_root_set, len);
    CVmObjByteArray *arr = (CVmObjByteArray *)vm_objp(vmg_ id);

    /* copy the bytes into the array */
    arr->cons_copy_from_buf((const unsigned char *)buf, 1, len);

    /* return the new array's object ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create with no contents 
 */
vm_obj_id_t CVmObjByteArray::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjByteArray();
    return id;
}

/*
 *   Create with the given element count 
 */
vm_obj_id_t CVmObjByteArray::create(VMG_ int in_root_set,
                                    unsigned long ele_count)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjByteArray(vmg_ ele_count);
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Instantiate 
 */
CVmObjByteArray::CVmObjByteArray(VMG_ unsigned long ele_count)
{
    /* allocate space */
    alloc_array(vmg_ ele_count);
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate space 
 */
void CVmObjByteArray::alloc_array(VMG_ unsigned long ele_count)
{
    unsigned long ele_rem;
    size_t slot_cnt;
    size_t alloc_size;
    size_t i;
    
    /* 
     *   figure out how many first-level page tables we need - each
     *   first-level page table refers to 256M bytes (8k pages per
     *   second-level page table times 32k bytes per page), so we need one
     *   page table pointer per 256 megabytes == 2^15*2^32 == 2^28.  
     */
    slot_cnt = (ele_count == 0 ? 0 : ((size_t)((ele_count - 1) >> 28) + 1));

    /* 
     *   allocate the extension - 4 bytes for the element count plus one
     *   (char **) per slot 
     */
    alloc_size = 4 + slot_cnt*sizeof(char **);
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(alloc_size, this);

    /* set the element count */
    set_element_count(ele_count);

    /* allocate the second-level page tables */
    for (ele_rem = ele_count, i = 0 ; i < slot_cnt ; ++i)
    {
        unsigned char **pgtab;
        unsigned long pgcnt;
        size_t pg;
        
        /* 
         *   Determine how many pages we need in this page table.  Each page
         *   holds 32k bytes, so we need one page per 32k bytes remaining to
         *   allocate.  However, each page table can hold only up to 8k page
         *   pointers, so limit this table to 8k pages. 
         */
        pgcnt = ((ele_rem - 1) >> 15) + 1;
        if (pgcnt > 8*1024)
            pgcnt = 8*1024;

        /* allocate this second-level page table */
        pgtab = (unsigned char **)t3malloc(
            (size_t)(pgcnt * sizeof(unsigned char *)));

        /* set this page table pointer */
        get_page_table_array()[i] = pgtab;

        /* allocate the pages in this table */
        for (pg = 0 ; pg < pgcnt ; ++pg)
        {
            size_t pgsiz;

            /* 
             *   calculate the size for this page - the maximum size of a
             *   page is 32k, but allocate only the amount remaining if we
             *   have less than 32k left to allocate 
             */
            pgsiz = 32*1024;
            if (pgsiz > ele_rem)
                pgsiz = (size_t)ele_rem;

            /* deduct this page's size from the remaining element count */
            ele_rem -= pgsiz;

            /* allocate and store this page */
            pgtab[pg] = (unsigned char *)t3malloc(pgsiz);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Notify of deletion
 */
void CVmObjByteArray::notify_delete(VMG_ int /*in_root_set*/)
{
    size_t slot;
    unsigned char ***slotp;
    size_t slot_cnt;
    size_t bytes_rem;

    /* if we have no extension, there's nothing to do */
    if (ext_ == 0)
        return;

    /* calculate the number of second-level page table slots */
    slot_cnt = (size_t)(get_element_count() == 0
                        ? 0
                        : ((get_element_count() >> 28) + 1));

    /* we have all of the bytes in the array left do delete */
    bytes_rem = get_element_count();

    /* traverse the list of second-level page tables and delete each one */
    for (slot = 0, slotp = get_page_table_array() ; slot < slot_cnt ;
         ++slot, ++slotp)
    {
        size_t pg;
        unsigned char **pgp;
        
        /* traverse each page in this page table */
        for (pg = 0, pgp = *slotp ; pg < 8*1024 && bytes_rem != 0 ;
             ++pg, ++pgp)
        {
            /* delete this page */
            t3free(*pgp);

            /* 
             *   deduct this page's size from the remaining bytes to delete
             *   - the page size is up to 32k, but no more than the
             *   remaining size
             */
            if (bytes_rem > 32*1024)
                bytes_rem -= 32*1024;
            else
                bytes_rem = 0;
        }

        /* delete this page table */
        t3free(*slotp);
    }

    /* free our extension */
    G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Cast to string.  Maps to string treating the bytes as Latin-1
 *   characters. 
 */
const char *CVmObjByteArray::cast_to_string(
    VMG_ vm_obj_id_t self, vm_val_t *new_str) const
{
    /* set up a source stream to read from the byte array into the string */
    CVmByteArraySource src(vmg_ self);

    /* create the string */
    new_str->set_obj(CVmObjString::create_latin1(vmg_ FALSE, &src));

    /* return the new string's byte buffer */
    return new_str->get_as_string(vmg0_);
}


/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjByteArray::set_prop(VMG_ class CVmUndo *,
                               vm_obj_id_t, vm_prop_id_t,
                               const vm_val_t *)
{
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a static property 
 */
int CVmObjByteArray::call_stat_prop(VMG_ vm_val_t *result,
                                    const uchar **pc_ptr, uint *argc,
                                    vm_prop_id_t prop)
{
    /* get the function table index */
    int idx = G_meta_table->prop_to_vector_idx(
        metaclass_reg_->get_reg_idx(), prop);

    /* check for static methods we define in ByteArray */
    switch (idx)
    {
    case PROPIDX_packBytes:
        /* the static version of packBytes() */
        return static_packBytes(vmg_ result, argc);

    default:
        /* not one of ours - defer to the superclass */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjByteArray::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                              vm_obj_id_t self, vm_obj_id_t *source_obj,
                              uint *argc)
{
    uint func_idx;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    
    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }
    
    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   ByteArray private undo record - saved byte block 
 */
struct bytearray_undo_bytes
{
    /* next block in the list */
    bytearray_undo_bytes *nxt_;

    /* the bytes - overallocated to the required size */
    unsigned char buf_[1];
};

/*
 *   ByteArray private undo record 
 */
class bytearray_undo_rec
{
public:
    bytearray_undo_rec(CVmObjByteArray *arr, unsigned long start_idx,
                       unsigned long cnt)
    {
        unsigned long idx;
        unsigned long rem;
        bytearray_undo_bytes *tail;

        /* remember the starting index and length of the saved data */
        idx_ = start_idx;
        cnt_ = cnt;
        
        /* store the original data in a linked list of byte blocks */
        for (idx = start_idx, rem = cnt, save_head_ = tail = 0 ;
             rem != 0 ; )
        {
            bytearray_undo_bytes *p;
            size_t cur;

            /* allocate up to 32k, or the amount remaining if lower */
            cur = 32768;
            if (cur > rem)
                cur = (size_t)rem;
            
            /* allocate a new block */
            p = (bytearray_undo_bytes *)t3malloc(
                sizeof(bytearray_undo_bytes) + cur - 1);

            /* link this block into our list */
            if (tail != 0)
                tail->nxt_ = p;
            else
                save_head_ = p;
            tail = p;
            p->nxt_ = 0;
                
            /* copy bytes into this block */
            arr->copy_to_buf(p->buf_, idx, cur);

            /* move past the copied bytes */
            idx += cur;
            rem -= cur;
        }
    }

    ~bytearray_undo_rec()
    {
        bytearray_undo_bytes *cur;
        bytearray_undo_bytes *nxt;

        /* delete our list of saved byte blocks */
        for (cur = save_head_ ; cur != 0 ; cur = nxt)
        {
            /* 
             *   remember the next one, as we're about to lose the current
             *   one's memory 
             */
            nxt = cur->nxt_;

            /* delete this one */
            t3free(cur);
        }
    }

    /* copy our data back into an array */
    void apply_undo(CVmObjByteArray *arr)
    {
        bytearray_undo_bytes *cur;
        unsigned long idx;
        unsigned long rem;

        /* delete our list of saved byte blocks */
        for (cur = save_head_, idx = idx_, rem = cnt_ ; rem != 0 ; )
        {
            size_t copy_len;
            
            /* limit the copy to our block size (32k) */
            copy_len = 32768;
            if (copy_len > rem)
                copy_len = (size_t)rem;

            /* copy this block into the array */
            arr->copy_from_buf(cur->buf_, idx, copy_len);

            /* move past the copied data */
            cur = cur->nxt_;
            idx += copy_len;
            rem -= copy_len;
        }
    }

    /* starting index */
    unsigned long idx_;

    /* length */
    unsigned long cnt_;

    /* head of list of blocks of saved data */
    bytearray_undo_bytes *save_head_;
};

/*
 *   Save undo for a change to a range of the array 
 */
void CVmObjByteArray::save_undo(VMG_ vm_obj_id_t self,
                                unsigned long start_idx,
                                unsigned long cnt)
{
    bytearray_undo_rec *rec;
    vm_val_t oldval;
    
    /* create our key record - this contains the entire original value */
    rec = new bytearray_undo_rec(this, start_idx, cnt);

    /* we don't use the old value for anything; use nil as a dummy */
    oldval.set_nil();

    /* add the undo record */
    if (!G_undo->add_new_record_ptr_key(vmg_ self, rec, &oldval))
    {
        /* failed to save the undo - discard our private key record */
        delete rec;
    }
}

/*
 *   Apply undo 
 */
void CVmObjByteArray::apply_undo(VMG_ struct CVmUndoRecord *gen_rec)
{
    /* apply our private undo record, if present */
    if (gen_rec->id.ptrval != 0)
    {
        bytearray_undo_rec *rec;

        /* get my private record */
        rec = (bytearray_undo_rec *)gen_rec->id.ptrval;
        
        /* apply the undo in the record to self */
        rec->apply_undo(this);

        /* delete the record now that it's been applied */
        delete rec;

        /* clear the pointer so we know it's gone */
        gen_rec->id.ptrval = 0;
    }
}

/*
 *   Discard undo 
 */
void CVmObjByteArray::discard_undo(VMG_ struct CVmUndoRecord *rec)
{
    /* delete our extra information record if present */
    if (rec->id.ptrval != 0)
    {
        /* free the record */
        t3free((bytearray_undo_rec *)rec->id.ptrval);

        /* clear the pointer so we know it's gone */
        rec->id.ptrval = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   load from an image file 
 */
void CVmObjByteArray::load_from_image(VMG_ vm_obj_id_t self,
                                      const char *ptr, size_t siz)
{
    /* load from the image data */
    load_image_data(vmg_ ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload the object from image data 
 */
void CVmObjByteArray::reload_from_image(VMG_ vm_obj_id_t,
                                        const char *ptr, size_t siz)
{
    /* load the image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   Load from image data 
 */
void CVmObjByteArray::load_image_data(VMG_ const char *ptr, size_t siz)
{
    unsigned long cnt;
    unsigned long idx;

    /* if we already have memory allocated, free it */
    notify_delete(vmg_ FALSE);

    /* get the new array size */
    cnt = t3rp4u(ptr);

    /* make sure the size isn't larger than we'd expect */
    if (siz > 4 + cnt)
        siz = 4 + cnt;

    /* allocate memory at the new size as indicated in the image data */
    alloc_array(vmg_ cnt);

    /* if the size is smaller than we'd expect, set extra elements to nil */
    if (siz < VMB_LEN + (VMB_DATAHOLDER * cnt))
    {
        /* fill everything with zeros to start with */
        fill_with(0, 1, cnt);
    }

    /* copy the bytes */
    for (ptr += 4, siz -= 4, idx = 1 ; siz != 0 ; )
    {
        unsigned char *dstp;
        size_t chunk_size;
        size_t avail;

        /* get the next chunk */
        dstp = get_ele_ptr(idx, &avail);

        /* limit this chunk size to the remaining copy size */
        chunk_size = avail;
        if (chunk_size > siz)
            chunk_size = siz;

        /* copy this chunk */
        memcpy(dstp, ptr, chunk_size);

        /* advance past this chunk */
        idx += chunk_size;
        ptr += chunk_size;
        siz -= chunk_size;
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjByteArray::save_to_file(VMG_ class CVmFile *fp)
{
    unsigned long rem;
    unsigned long idx;
    
    /* write the element count */
    fp->write_int4(get_element_count());

    /* write the bytes in chunks */
    for (idx = 1, rem = get_element_count() ; rem != 0 ; )
    {
        size_t avail;
        size_t chunk;
        unsigned char *p;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit this copy to the remaining bytes */
        chunk = avail;
        if (chunk > rem)
            chunk = (size_t)rem;

        /* write this chunk */
        fp->write_bytes((char *)p, chunk);

        /* advance past this chunk */
        idx += chunk;
        rem -= chunk;
    }
}

/* 
 *   restore from a file 
 */
void CVmObjByteArray::restore_from_file(VMG_ vm_obj_id_t self,
                                        CVmFile *fp, CVmObjFixup *)
{
    unsigned long ele_cnt;
    unsigned long idx;
    unsigned long rem;

    /* read the element count */
    ele_cnt = fp->read_uint4();

    /* allocate or reallocate as needed */
    if (ext_ == 0)
    {
        /* we're not yet allocated - allocate now */
        alloc_array(vmg_ ele_cnt);
    }
    else
    {
        /* already allocated - if it's a different size, reallocate */
        if (get_element_count() != ele_cnt)
        {
            /* delete the old array */
            notify_delete(vmg_ FALSE);

            /* allocate a new one */
            alloc_array(vmg_ ele_cnt);
        }
    }

    /* read the data */
    for (idx = 1, rem = ele_cnt ; rem != 0 ; )
    {
        size_t avail;
        size_t chunk;
        unsigned char *p;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit this copy to the remaining bytes */
        chunk = avail;
        if (chunk > rem)
            chunk = (size_t)rem;

        /* read this chunk */
        fp->read_bytes((char *)p, chunk);

        /* advance past this chunk */
        idx += chunk;
        rem -= chunk;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Retrieve the value at the given index 
 */
int CVmObjByteArray::index_val_q(VMG_ vm_val_t *result, vm_obj_id_t self,
                                 const vm_val_t *index_val)
{
    /* get the index */
    int32_t idx = index_val->num_to_int(vmg0_);

    /* make sure it's in range */
    if (idx < 1 || (uint32_t)idx > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);
    
    /* get a pointer to the desired element */
    size_t avail;
    unsigned char *p = get_ele_ptr(idx, &avail);

    /* return the value as an integer */
    result->set_int((int)(unsigned short)*p);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   set an indexed element of the array 
 */
int CVmObjByteArray::set_index_val_q(VMG_ vm_val_t *new_container,
                                     vm_obj_id_t self,
                                     const vm_val_t *index_val,
                                     const vm_val_t *new_val)
{
    /* get the index value as an integer */
    int32_t idx = index_val->num_to_int(vmg0_);

    /* make sure it's in range - 1 to our element count, inclusive */
    if (idx < 1 || (uint32_t)idx > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* save undo for the change */
    save_undo(vmg_ self, idx, 1);

    /* get the new value as an integer */
    int32_t new_byte = new_val->num_to_int(vmg0_);

    /* make sure it's in range */
    if (new_byte < 0 || new_byte > 255)
        err_throw(VMERR_OUT_OF_RANGE);

    /* get a pointer to the desired element */
    size_t avail;
    unsigned char *p = get_ele_ptr(idx, &avail);

    /* set the value */
    *p = (unsigned char)new_byte;

    /* the result is the original array value */
    new_container->set_obj(self);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare for equality 
 */
int CVmObjByteArray::equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                            int /*depth*/) const
{
    CVmObjByteArray *other;
    unsigned long idx;
    unsigned long rem;

    /* if it's a self-reference, it's certainly equal */
    if (val->typ == VM_OBJ && val->val.obj == self)
        return TRUE;

    /* if it's not another byte array, it's not equal */
    if (val->typ != VM_OBJ || !is_byte_array(vmg_ val->val.obj))
        return FALSE;

    /* we know it's another byte array - cast it */
    other = (CVmObjByteArray *)vm_objp(vmg_ val->val.obj);

    /* if it's not of the same length, it's not equal */
    if (other->get_element_count() != get_element_count())
        return FALSE;

    /* compare the arrays */
    for (idx = 1, rem = get_element_count() ; rem != 0 ; )
    {
        unsigned char *p1;
        unsigned char *p2;
        size_t avail1;
        size_t avail2;
        size_t chunk;
        
        /* get the next chunk of each array */
        p1 = get_ele_ptr(idx, &avail1);
        p2 = other->get_ele_ptr(idx, &avail2);

        /* if the chunk sizes aren't the same, there's some problem */
        assert(avail1 == avail2);

        /* limit the chunk size to the remaining size */
        chunk = avail1;
        if (chunk > rem)
            chunk = (size_t)rem;

        /* if the chunks differ, the arrays differ */
        if (memcmp(p1, p2, chunk) != 0)
            return FALSE;

        /* advance past the chunk */
        idx += avail1;
        rem -= avail1;
    }

    /* we found no differences */
    return TRUE;
}

/*
 *   Calculate a hash value 
 */
uint CVmObjByteArray::calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const
{
    unsigned long idx;
    unsigned long rem;
    uint hash;

    /* add up the bytes in the array */
    for (hash = 0, idx = 1, rem = get_element_count() ; rem != 0 ; )
    {
        unsigned char *p;
        size_t avail;
        size_t chunk;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit the chunk size to the remaining size */
        chunk = avail;
        if (chunk > rem)
            chunk = (size_t)rem;

        /* advance our counters for this chunk */
        idx += chunk;
        rem -= chunk;

        /* add up the bytes in this part */
        for ( ; chunk != 0 ; --chunk, ++p)
            hash += *p;
    }

    /* return the result */
    return hash;
}

/* ------------------------------------------------------------------------ */
/*
 *   Copy bytes from an array
 */
void CVmObjByteArray::copy_from(unsigned long dst_idx,
                                CVmObjByteArray *src_arr,
                                unsigned long src_idx,
                                unsigned long cnt)
{
    /* if we're moving zero bytes, there's nothing to do */
    if (cnt == 0)
        return;

    /* make sure we don't overrun our array */
    if (dst_idx > get_element_count())
        cnt = 0;
    else if (dst_idx + cnt - 1 > get_element_count())
        cnt = get_element_count() + 1 - dst_idx;

    /*
     *   If the source and destination objects are the same, and the source
     *   and destination regions overlap, we must take care to move the
     *   bytes in such a way that we don't overwrite parts of the source in
     *   the course of moving the bytes. 
     */
    if (src_arr == this
        && src_idx + cnt - 1 >= dst_idx && src_idx <= dst_idx + cnt - 1)
    {
        /* the regions overlap - use the overlap-safe move routine */
        move_bytes(dst_idx, src_idx, cnt);

        /* done */
        return;
    }
        
    /* continue until we exhaust the count */
    while (cnt != 0)
    {
        size_t src_avail;
        size_t src_chunk_size;
        unsigned char *srcp;
        
        /* if we're past the end of the source array, stop copying */
        if (src_idx > src_arr->get_element_count())
            break;

        /* get the next source chunk */
        srcp = src_arr->get_ele_ptr(src_idx, &src_avail);

        /* limit this chunk size to the remaining copy size */
        src_chunk_size = src_avail;
        if (src_chunk_size > cnt)
            src_chunk_size = (size_t)cnt;

        /* limit this chunk to the remaining source array size */
        if (src_idx + src_chunk_size - 1 > src_arr->get_element_count())
            src_chunk_size = src_arr->get_element_count() + 1 - src_idx;

        /* copy this chunk into the destination */
        copy_from_buf(srcp, dst_idx, src_chunk_size);

        /* move past this chunk in the source */
        cnt -= src_chunk_size;
        src_idx += src_chunk_size;

        /* move past this chunk in the destination */
        dst_idx += src_chunk_size;
    }

    /* 
     *   if there's any copying size left, we ran out of source array bytes
     *   - fill the balance of the destination array with zeros
     */
    if (cnt != 0)
        fill_with(0, dst_idx, cnt);
}

/*
 *   Move bytes within our array.  This is safe even if the source and
 *   destination regions overlap.  
 */
void CVmObjByteArray::move_bytes(unsigned long dst_idx,
                                 unsigned long src_idx,
                                 unsigned long cnt)
{
    size_t src_avail;
    size_t dst_avail;
    unsigned char *srcp;
    unsigned char *dstp;

    /* 
     *   If the destination is before the source, we're moving bytes down,
     *   so we must start at the low end and work forwards through the
     *   array.  If the destination is after the source, we're moving bytes
     *   up, so we must start at the high end and work backwards through the
     *   array.  
     */
    if (dst_idx < src_idx)
    {

        /* 
         *   Moving bytes down in the array - start at the low end and work
         *   forwards through the array.  Get the starting pointers and
         *   available lengths.  
         */
        srcp = get_ele_ptr(src_idx, &src_avail);
        dstp = get_ele_ptr(dst_idx, &dst_avail);

        /* keep going until we've moved all of the bytes requested */
        while (cnt != 0)
        {
            size_t move_len;

            /* 
             *   figure the largest amount we can move - we can move the
             *   smallest of the remaining requested move size, the source
             *   chunk, and the destination chunk 
             */
            move_len = cnt;
            if (move_len > src_avail)
                move_len = src_avail;
            if (move_len > dst_avail)
                move_len = dst_avail;

            /* move the data */
            memmove(dstp, srcp, move_len);

            /* advance all of the counters by the move size */
            srcp += move_len;
            dstp += move_len;
            cnt -= move_len;
            src_avail -= move_len;
            dst_avail -= move_len;
            src_idx += move_len;
            dst_idx += move_len;

            /* stop if we're done */
            if (cnt == 0)
                break;

            /* if the source chunk is at an end, get the next one */
            if (src_avail == 0)
                srcp = get_ele_ptr(src_idx, &src_avail);

            /* if the destination chunk is at an end, get the next one */
            if (dst_avail == 0)
                dstp = get_ele_ptr(dst_idx, &dst_avail);
        }
    }
    else
    {
        /* 
         *   We're to move bytes up in the array - start at the high end and
         *   work backwards through the array.  Advance each index to one
         *   past the last byte of its range.  
         */
        src_idx += cnt;
        dst_idx += cnt;

        /* get the chunk pointers */
        srcp = get_ele_ptr(src_idx - 1, &src_avail) + 1;
        dstp = get_ele_ptr(dst_idx - 1, &dst_avail) + 1;

        /* 
         *   since we're working backwards, we actually want to know the
         *   number of bytes on the page *before* the current pointers, so
         *   subtract the available spaces from the size of the page to get
         *   the available space preceding each 
         */
        src_avail = 32*1024 - src_avail + 1;
        dst_avail = 32*1024 - dst_avail + 1;

        /* keep going until we've moved all of the requested bytes */
        while (cnt != 0)
        {
            size_t move_len;

            /* 
             *   figure the largest amount we can move - we can move the
             *   smallest of the remaining requested move size, the source
             *   chunk, and the destination chunk 
             */
            move_len = cnt;
            if (move_len > src_avail)
                move_len = src_avail;
            if (move_len > dst_avail)
                move_len = dst_avail;

            /* move the data */
            memmove(dstp - move_len, srcp - move_len, move_len);

            /* advance all of the counters by the move size */
            srcp -= move_len;
            dstp -= move_len;
            cnt -= move_len;
            src_avail -= move_len;
            dst_avail -= move_len;
            src_idx -= move_len;
            dst_idx -= move_len;

            /* stop if we're done */
            if (cnt == 0)
                break;

            /* if we've exhausted the source chunk, get the next one */
            if (src_avail == 0)
            {
                srcp = get_ele_ptr(src_idx - 1, &src_avail) + 1;
                src_avail = 32*1024 - src_avail + 1;
            }

            /* if we've exhausted the destination chunk, get the next one */
            if (dst_avail == 0)
            {
                dstp = get_ele_ptr(dst_idx - 1, &dst_avail) + 1;
                dst_avail = 32*1024 - dst_avail + 1;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Fill a 1-based index range with the given value 
 */
void CVmObjByteArray::fill_with(unsigned char val, unsigned long start_idx,
                                unsigned long cnt)
{
    unsigned long idx;
    unsigned long rem;
    
    /* ensure we don't overrun the array */
    if (start_idx > get_element_count())
        cnt = 0;
    else if (start_idx + cnt - 1 > get_element_count())
        cnt = get_element_count() + 1 - start_idx;

    /* continue until we exhaust the count */
    for (idx = start_idx, rem = cnt ; rem != 0 ; )
    {
        size_t avail;
        size_t chunk_size;
        unsigned char *p;
        
        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit this chunk size to the remaining size to fill */
        chunk_size = avail;
        if (chunk_size > rem)
            chunk_size = (size_t)rem;

        /* fill this chunk */
        memset(p, val, chunk_size);

        /* skip this chunk */
        rem -= chunk_size;
        idx += chunk_size;
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - length 
 */
int CVmObjByteArray::getp_length(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the length */
    retval->set_int(get_element_count());

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - subarray 
 */
int CVmObjByteArray::getp_subarray(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *in_argc)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    unsigned long idx;
    unsigned long cnt;
    CVmObjByteArray *arr;
    
    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the starting index */
    idx = CVmBif::pop_int_val(vmg0_);

    /* force it to be in range */
    if (idx < 1)
        idx = 1;
    else if (idx > get_element_count() + 1)
        idx = get_element_count() + 1;

    /* if there's a count, get it */
    if (argc >= 2)
    {
        /* get the explicit count */
        cnt = CVmBif::pop_int_val(vmg0_);
    }
    else
    {
        /* use the entire rest of the array */
        cnt = get_element_count();
    }

    /* limit the count to the available size */
    if (idx > get_element_count())
        cnt = 0;
    else if (idx + cnt - 1 > get_element_count())
        cnt = get_element_count() + 1 - idx;

    /* push a self-reference while we're working for gc protection */
    G_stk->push()->set_obj(self);

    /* allocate a new array to hold the result */
    retval->set_obj(create(vmg_ FALSE, cnt));
    arr = (CVmObjByteArray *)vm_objp(vmg_ retval->val.obj);

    /* copy the data from our array into the new one */
    arr->copy_from(1, this, idx, cnt);

    /* discard our self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - copy from another byte array 
 */
int CVmObjByteArray::getp_copy_from(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(4);
    unsigned long dst_idx;
    unsigned long src_idx;
    unsigned long cnt;
    vm_obj_id_t src_arr_id;
    CVmObjByteArray *src_arr;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the source array */
    src_arr_id = CVmBif::pop_obj_val(vmg0_);

    /* make sure it is indeed an array */
    if (!is_byte_array(vmg_ src_arr_id))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* we know it's the right type, so cast it */
    src_arr = (CVmObjByteArray *)vm_objp(vmg_ src_arr_id);

    /* get the starting source index */
    src_idx = CVmBif::pop_int_val(vmg0_);

    /* force it to be in range */
    if (src_idx < 1)
        src_idx = 1;
    else if (src_idx > src_arr->get_element_count() + 1)
        src_idx = src_arr->get_element_count() + 1;

    /* get the destination index */
    dst_idx = CVmBif::pop_int_val(vmg0_);

    /* force it to be within range */
    if (dst_idx < 1)
        dst_idx = 1;
    else if (dst_idx > get_element_count() + 1)
        dst_idx = get_element_count() + 1;

    /* get the count */
    cnt = CVmBif::pop_int_val(vmg0_);

    /* limit the copying to the available destination space */
    if (dst_idx > get_element_count())
        cnt = 0;
    else if (dst_idx + cnt - 1 > get_element_count())
        cnt = get_element_count() + 1 - dst_idx;

    /* save undo for the change */
    save_undo(vmg_ self, dst_idx, cnt);

    /* copy the data from the source array into our array */
    copy_from(dst_idx, src_arr, src_idx, cnt);

    /* the result is 'self' */
    retval->set_obj(self);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - fill with a value 
 */
int CVmObjByteArray::getp_fill_val(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *in_argc)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(1, 2);
    unsigned long idx;
    unsigned long cnt;
    long fill_val;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the value with which to fill elements */
    fill_val = CVmBif::pop_int_val(vmg0_);
    if (fill_val < 0 || fill_val > 255)
        err_throw(VMERR_OUT_OF_RANGE);

    /* get the starting index, if provided */
    if (argc >= 2)
    {
        /* get the index */
        idx = CVmBif::pop_int_val(vmg0_);

        /* force it to be in range */
        if (idx < 1)
            idx = 1;
        else if (idx > get_element_count() + 1)
            idx = get_element_count() + 1;
    }
    else
    {
        /* fill from the first element */
        idx = 1;
    }

    /* get the count, if provided */
    if (argc >= 3)
    {
        /* get the count */
        cnt = CVmBif::pop_int_val(vmg0_);
    }
    else
    {
        /* fill the entire array */
        cnt = get_element_count();
    }

    /* force the length to be in range */
    if (idx > get_element_count())
        cnt = 0;
    else if (idx + cnt - 1 > get_element_count())
        cnt = get_element_count() + 1 - idx;

    /* save undo for the change */
    save_undo(vmg_ self, idx, cnt);

    /* fill with the given value */
    fill_with((unsigned char)fill_val, idx, cnt);

    /* the result is 'self' */
    retval->set_obj(self);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - convert to string 
 */
int CVmObjByteArray::getp_to_string(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *in_argc)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(0, 3);
    unsigned long idx;
    unsigned long cnt;
    vm_val_t charset;
    CCharmapToUni *mapper = 0;
    size_t str_len;
    CVmObjString *str;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the character set object */
    charset.set_nil();
    if (argc >= 1)
        charset.set_obj_or_nil(CVmBif::pop_charset_obj(vmg0_));

    /* if we got a CharacterSet, get its to-unicode mapper */
    if (charset.typ != VM_NIL)
        mapper = ((CVmObjCharSet *)vm_objp(vmg_ charset.val.obj))
                 ->get_to_uni(vmg0_);

    /* if there's a starting index, retrieve it */
    if (argc >= 2)
    {
        /* retrieve the starting index */
        idx = CVmBif::pop_int_val(vmg0_);

        /* force it to be in range */
        if (idx < 1)
            idx = 1;
        else if (idx > get_element_count() + 1)
            idx = get_element_count() + 1;
    }
    else
    {
        /* start at the first byte */
        idx = 1;
    }

    /* if there's a length, retrieve it */
    if (argc >= 3)
    {
        /* retrieve the length */
        cnt = CVmBif::pop_int_val(vmg0_);
    }
    else
    {
        /* use all remaining characters */
        cnt = get_element_count();
    }

    /* force the length to be in range */
    if (idx > get_element_count())
        cnt = 0;
    else if (idx + cnt - 1 > get_element_count())
        cnt = get_element_count() + 1 - idx;

    /* push a self-ref for gc protection, and a ref to the character set */
    G_stk->push()->set_obj(self);
    G_stk->push(&charset);

    /* map via the character mapper, or as raw unicode character values */
    if (mapper != 0)
    {
        /*
         *   We have a character set mapper.  First, measure the required
         *   buffer length by mapping to a null buffer.  
         */
        str_len = map_to_string(idx, cnt, 0, 0, mapper);
        
        /* allocate a string of the required length */
        retval->set_obj(CVmObjString::create(vmg_ FALSE, str_len));
        str = (CVmObjString *)vm_objp(vmg_ retval->val.obj);

        /* map the string, actually storing the bytes this time */
        map_to_string(idx, cnt, str, str_len, mapper);
    }
    else
    {
        /* there's no mapper - treat our bytes as unicode character codes */
        CVmByteArraySource src(vmg_ self, idx - 1, cnt);
        retval->set_obj(CVmObjString::create_latin1(vmg_ FALSE, &src));
    }

    /* discard the gc protection */
    G_stk->discard(2);
    
    /* handled */
    return TRUE;
}

/*
 *   Run a range of bytes through a character mapper to produce string data,
 *   optionally storing the string data in a string object.  Returns the
 *   number of bytes in the resulting string.  
 */
size_t CVmObjByteArray::map_to_string(unsigned long idx,
                                      unsigned long len,
                                      CVmObjString *str, size_t str_len,
                                      CCharmapToUni *mapper)
{
    size_t str_total;
    size_t buf_len;
    unsigned char buf[128];
    char *dst;

    /* get the string's buffer pointer */
    dst = (str != 0 ? str->cons_get_buf() : 0);
    
    /* go through the source bytes a bit at a time */
    for (str_total = 0, buf_len = 0 ; len != 0 ; )
    {
        size_t copy_len;
        size_t str_cur;
        size_t partial_len;
        
        /* 
         *   fill up the buffer, up to the remaining source length or the
         *   buffer's capacity, whichever is lower 
         */
        copy_len = sizeof(buf) - buf_len;
        if (copy_len > len)
            copy_len = (size_t)len;

        /* copy the bytes to our staging buffer */
        copy_to_buf(buf, idx, copy_len);

        /* add the copied bytes into the buffer length */
        buf_len += copy_len;

        /* advance past the copied bytes in the source */
        idx += copy_len;
        len -= copy_len;

        /* translate the bytes through the character set mapping */
        str_cur = mapper->map2(&dst, &str_len, (char *)buf, buf_len,
                               &partial_len);

        /* 
         *   if this would push us over the maximum string size, we can't
         *   convert the data 
         */
        if (str_cur > OSMALMAX - str_total - VMB_LEN)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* add the current length into the total string length */
        str_total += str_cur;

        /* copy the partial last character bytes to the start of the buffer */
        if (partial_len != 0)
            memmove(buf, buf + buf_len - partial_len, partial_len);

        /* the buffer now contains only the partial character bytes */
        buf_len = partial_len;
    }

    /* return the total string length */
    return str_total;
}

/*
 *   Copy bytes from the array to a buffer 
 */
size_t CVmObjByteArray::copy_to_buf(unsigned char *buf,
                                    unsigned long idx, size_t len) const
{
    /* we haven't copied any bytes so far */
    size_t actual = 0;

    /* limit it to the actual available space */
    unsigned long contlen = get_element_count();
    if (idx > contlen)
        len = 0;
    else if (idx + len > contlen)
        len = contlen - idx + 1;

    /* keep going until we satisfy the request */
    while (len != 0)
    {
        size_t avail;
        unsigned char *p;
        size_t copy_len;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* if there's nothing left, stop */
        if (avail == 0)
            break;

        /* copy the available bytes or the reamining desired bytes */
        copy_len = avail;
        if (copy_len > len)
            copy_len = len;

        /* copy the bytes */
        memcpy(buf, p, copy_len);

        /* advance past the copied bytes */
        buf += copy_len;
        idx += copy_len;
        len -= copy_len;
        actual += copy_len;
    }

    /* return the actual size copied */
    return actual;
}

/*
 *   Copy bytes from a buffer into the array, saving undo 
 */
size_t CVmObjByteArray::copy_from_buf_undo(VMG_ vm_obj_id_t self,
                                           const unsigned char *buf,
                                           unsigned long idx, size_t len)
{
    /* limit the size to the remaining length from 'idx' */
    unsigned long avail_len = get_element_count() - idx + 1;
    if (len > avail_len)
        len = (size_t)avail_len;

    /* save undo */
    save_undo(vmg_ self, idx, len);

    /* copy the bytes */
    copy_from_buf(buf, idx, len);

    /* return the copy length */
    return len;
}

/*
 *   Copy bytes from a buffer into the array
 */
void CVmObjByteArray::copy_from_buf(const unsigned char *buf,
                                    unsigned long idx, size_t len)
{
    /* keep going until we satisfy the request */
    while (len != 0)
    {
        size_t avail;
        unsigned char *p;
        size_t copy_len;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* copy the available bytes or the remaining desired bytes */
        copy_len = avail;
        if (copy_len > len)
            copy_len = len;

        /* copy the bytes */
        memcpy(p, buf, copy_len);

        /* advance past the copied bytes */
        buf += copy_len;
        idx += copy_len;
        len -= copy_len;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Swap byte order 
 */
static void swap_bytes(unsigned char *buf, int len)
{
    int i, j;
    for (i = 0, j = len - 1 ; i < j ; ++i, --j)
    {
        unsigned char tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - read an integer 
 */
int CVmObjByteArray::getp_read_int(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(2);
    unsigned int idx;
    unsigned int fmt;
    long result = 0;
    size_t siz;
    unsigned char cbuf[4];

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the starting index and format code */
    idx = CVmBif::pop_int_val(vmg0_);
    fmt = CVmBif::pop_int_val(vmg0_);

    /* get the size from the format */
    switch (fmt & FmtSizeMask)
    {
    case FmtInt8:
    default:
        siz = 1;
        break;

    case FmtInt16:
        siz = 2;
        break;

    case FmtInt32:
        siz = 4;
        break;
    }

    /* check that the index is in range */
    if (idx < 1 || idx + siz - 1 > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* make a copy of the bytes in cbuf[] for easy access */
    copy_to_buf(cbuf, idx, siz);

    /* read the value */
    switch (siz)
    {
    case 1:
        /* 8-bit integer - all that matters is the signedness */
        if ((fmt & FmtSignedMask) == FmtSigned)
            result = (long)(int)(char)cbuf[0];
        else
            result = cbuf[0];
        break;

    case 2:
        /* 
         *   16-bit integer.  Our standard portable order is little-endian,
         *   so swap bytes if it's big-endian.  
         */
        if ((fmt & FmtOrderMask) == FmtBigEndian)
            swap_bytes(cbuf, 2);

        /* it's little-endian now, so pull out the signed or unsigned value */
        if ((fmt & FmtSignedMask) == FmtSigned)
            result = osrp2s(cbuf);
        else
            result = osrp2(cbuf);

        break;

    case 4:
        /* 
         *   32-bit integer.  Convert to our standard little-endian ordering
         *   if it's currently big-endian, by swapping the byte order.
         */
        if ((fmt & FmtOrderMask) == FmtBigEndian)
            swap_bytes(cbuf, 4);

        /* it's little-endian now, so pull out the signed or unsigned value */
        if ((fmt & FmtSignedMask) == FmtSigned)
            result = osrp4s(cbuf);
        else
            result = osrp4(cbuf);
        break;
    }

    /* return the result */
    retval->set_int(result);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - write an integer 
 */
int CVmObjByteArray::getp_write_int(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(3);
    unsigned int idx;
    unsigned int fmt;
    long val;
    size_t siz;
    unsigned char cbuf[4];

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the starting index, format code, and value to write */
    idx = CVmBif::pop_int_val(vmg0_);
    fmt = CVmBif::pop_int_val(vmg0_);
    val = CVmBif::pop_long_val(vmg0_);

    /* get the size from the format */
    switch (fmt & FmtSizeMask)
    {
    case FmtInt8:
    default:
        siz = 1;
        break;

    case FmtInt16:
        siz = 2;
        break;

    case FmtInt32:
        siz = 4;
        break;
    }

    /* check that the index is in range */
    if (idx < 1 || idx + siz - 1 > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* write the value to cbuf[] */
    switch (siz)
    {
    case 1:
        /* 8-bit integer */
        cbuf[0] = (char)(val & 0xFF);
        break;

    case 2:
        /* 16-bit integer - start with our standard little-endian format */
        if ((fmt & FmtSignedMask) == FmtSigned)
            oswp2s(cbuf, (int)val);
        else
            oswp2(cbuf, (int)val);
        break;

    case 4:
        /* 32-bit integer - start with our standard little-endian format */
        if ((fmt & FmtSignedMask) == FmtSigned)
            oswp4s(cbuf, val);
        else
            oswp4(cbuf, val);
        break;
    }

    /* swap bytes if they want big-endian */
    if (siz > 1 && (fmt & FmtOrderMask) == FmtBigEndian)
        swap_bytes(cbuf, siz);

    /* save undo for the change */
    save_undo(vmg_ self, idx, siz);

    /* store the byte representation we've constructed */
    copy_from_buf(cbuf, idx, siz);
    
    /* there's no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}



/* ------------------------------------------------------------------------ */
/*
 *   Write bytes from the specified region of the array to a file.  Returns
 *   zero on success, non-zero on failure.  
 */
int CVmObjByteArray::write_to_file(CVmDataSource *fp, unsigned long start_idx,
                                   unsigned long len) const
{
    unsigned long rem;
    unsigned long idx;

    /* make sure the starting index is valid */
    if (start_idx < 1)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);
    
    /* 
     *   if the starting index is past the end of the array, there's nothing
     *   to do - just return success
     */
    if (start_idx > get_element_count())
        return 0;

    /* 
     *   limit the request to the number of bytes available after the
     *   starting index 
     */
    if (start_idx + len - 1 > get_element_count())
        len = get_element_count() - start_idx + 1;

    /* keep going until we satisfy the request or run into a problem */
    for (idx = start_idx, rem = len ; rem != 0 ; )
    {
        unsigned char *p;
        size_t avail;
        size_t chunk;
        
        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit writing to the amount remaining of the requested size */
        chunk = avail;
        if (chunk > rem)
            chunk = (size_t)rem;

        /* 
         *   write out this chunk - if an error occurs, abort with a failure
         *   indication 
         */
        if (fp->write(p, chunk))
            return 1;

        /* move our counters past this chunk */
        idx += chunk;
        rem -= chunk;
    }

    /* we satisfied the request without problems - indicate success */
    return 0;
}

/*
 *   Read bytes from the file into the specified region of the array.
 *   Returns the number of bytes actually read.  
 */
unsigned long CVmObjByteArray::read_from_file(
    VMG_ vm_obj_id_t self, CVmDataSource *fp,
    unsigned long start_idx, unsigned long len, int undo)
{
    unsigned long rem;
    unsigned long idx;
    unsigned long total;

    /* make sure the starting index is at least 1 */
    if (start_idx < 1)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* 
     *   if the starting index is past the end of the array, there's nothing
     *   to do - just return success 
     */
    if (start_idx > get_element_count())
        return 0;

    /* if the requested length is zero, consider it a success */
    if (len == 0)
        return 0;

    /* 
     *   limit the request to the number of bytes available after the
     *   starting index 
     */
    if (start_idx + len - 1 > get_element_count())
        len = get_element_count() - start_idx + 1;

    /* save undo */
    if (undo)
        save_undo(vmg_ self, start_idx, len);

    /* keep going until we satisfy the request or run into a problem */
    for (idx = start_idx, rem = len, total = 0 ; rem != 0 ; )
    {
        unsigned char *p;
        size_t avail;
        size_t chunk;
        size_t cur_read;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit reading to the amount of the request remaining */
        chunk = avail;
        if (chunk > rem)
            chunk = (size_t)rem;

        /* read as much as we can of this chunk */
        cur_read = fp->readc(p, chunk);

        /* add this amount into the total so far */
        total += cur_read;

        /* 
         *   if we didn't get as much as we asked for, we must have reached
         *   the end of the file before satisfying the request - there's
         *   nothing more to be read in this case, so we can stop looping 
         */
        if (cur_read != chunk)
            break;

        /* move our counters past this chunk */
        idx += chunk;
        rem -= chunk;
    }

    /* return the total amount we read */
    return total;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write to a 'data' mode file 
 */
int CVmObjByteArray::write_to_data_file(CVmDataSource *fp)
{
    char buf[16];

    /* write the number of bytes in our array */
    oswp4(buf, get_element_count());
    if (fp->write(buf, 4))
        return 1;

    /* write the bytes */
    return write_to_file(fp, 1, get_element_count());
}

/*
 *   Read from a 'data' mode file 
 */
int CVmObjByteArray::read_from_data_file(VMG_ vm_val_t *retval,
                                         CVmDataSource *fp)
{
    char buf[16];
    CVmObjByteArray *arr;
    unsigned long len;

    /* read the number of bytes in the array */
    if (fp->read(buf, 4))
        return 1;
    len = t3rp4u(buf);

    /* create a new ByteArray to hold the result */
    retval->set_obj(create(vmg_ FALSE, len));
    arr = (CVmObjByteArray *)vm_objp(vmg_ retval->val.obj);

    /* read the bytes (it's a new object, so don't save undo) */
    if (arr->read_from_file(vmg_ retval->val.obj, fp, 1, len, FALSE) != len)
    {
        /* 
         *   we didn't manage to read all of the bytes - since the value was
         *   tagged with the correct number of bytes, end-of-file in the
         *   middle of the bytes indicates a corrupted file, so return
         *   failure 
         */
        return 1;
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - packBytes 
 */
int CVmObjByteArray::getp_packBytes(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = oargc != 0 ? *oargc : 0;
    static CVmNativeCodeDesc desc(2, 0, TRUE);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the starting index from the first argument */
    long idx = CVmBif::pop_long_val(vmg0_);
    --argc;

    /* limit the index to 1..length */
    if (idx < 1)
        idx = 1;
    else if ((ulong)idx > get_element_count())
        idx = get_element_count();

    /* adjust to a zero-based index */
    --idx;

    /* set up a data source for the byte array */
    CVmByteArraySource dst(vmg_ self);

    /* seek to the starting index */
    dst.seek(idx, OSFSK_SET);

    /* do the packing */
    CVmPack::pack(vmg_ 0, argc, &dst);

    /* discard the arguments */
    G_stk->discard(argc);

    /* return the number of bytes written */
    retval->set_int(dst.get_pos() - idx);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - static ByteArray.packBytes().  This version of the
 *   method creates a new ByteArray object containing the packed bytes.  
 */
int CVmObjByteArray::static_packBytes(VMG_ vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = oargc != 0 ? *oargc : 0;
    static CVmNativeCodeDesc desc(1, 0, TRUE);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* set up an in-memory data stream to receive the packed data */
    CVmMemorySource *dst = new CVmMemorySource(0L);

    err_try
    {
        /* do the packing */
        CVmPack::pack(vmg_ 0, argc, dst);

        /* create a byte array to hold the packed data */
        long len = dst->get_size();
        retval->set_obj(create(vmg_ FALSE, len));
        CVmObjByteArray *arr = (CVmObjByteArray *)vm_objp(vmg_ retval->val.obj);

        /* 
         *   copy the bytes from the stream to the byte array; it's a new
         *   object, so there's no undo to save 
         */
        dst->seek(0, OSFSK_SET);
        arr->read_from_file(vmg_ retval->val.obj, dst, 1, len, FALSE);
    }
    err_finally
    {
        /* done with the data stream */
        delete dst;
    }
    err_end;

    /* discard the arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - unpackBytes 
 */
int CVmObjByteArray::getp_unpackBytes(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = oargc != 0 ? *oargc : 0;
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the starting index from the first argument */
    long idx = CVmBif::pop_long_val(vmg0_);
    --argc;

    /* limit the index to 1..length */
    if (idx < 1)
        idx = 1;
    else if ((ulong)idx > get_element_count())
        idx = get_element_count();

    /* get the format string, but leave it on the stack for gc protection */
    const char *fmt = G_stk->get(0)->get_as_string(vmg0_);

    /* get the format string length and buffer pointer */
    size_t fmtlen = vmb_get_len(fmt);
    fmt += VMB_LEN;

    /* set up a data source for the byte array */
    CVmByteArraySource src(vmg_ self);

    /* seek to the starting index */
    src.seek(idx - 1, OSFSK_SET);

    /* do the unpacking */
    CVmPack::unpack(vmg_ retval, fmt, fmtlen, &src);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - sha256
 */
int CVmObjByteArray::getp_sha256(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = oargc != 0 ? *oargc : 0;
    static CVmNativeCodeDesc desc(0, 2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the starting index and length */
    long cnt = get_element_count();
    long idx = argc >= 1 ? CVmBif::pop_int_val(vmg0_) : 1;
    long len = argc >= 2 ? CVmBif::pop_int_val(vmg0_) : cnt;

    /* set up a data source on the array, and seek to the starting index */
    CVmByteArraySource src(vmg_ self);
    src.seek(idx - 1, OSFSK_SET);

    /* calculate the hash */
    char buf[65];
    sha256_datasrc(buf, &src, len);

    /* return the string */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, buf, 64));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - digestMD5
 */
int CVmObjByteArray::getp_digestMD5(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = oargc != 0 ? *oargc : 0;
    static CVmNativeCodeDesc desc(0, 2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the starting index and length */
    long cnt = get_element_count();
    long idx = argc >= 1 ? CVmBif::pop_int_val(vmg0_) : 1;
    long len = argc >= 2 ? CVmBif::pop_int_val(vmg0_) : cnt;

    /* limit the starting index to the available range */
    if (idx < 1)
        idx = 1;
    else if (idx > cnt + 1)
        idx = cnt + 1;

    /* limit the length to the range remaining */
    if (len < 0)
        len = 0;
    else if (len > cnt - idx + 1)
        len = cnt - idx + 1;

    /* set up the hash accumulator */
    md5_state_t ctx;
    md5_init(&ctx);

    /* feed the selected range of bytes into the hash */
    while (len > 0)
    {
        /* get the next chunk */
        size_t avail;
        unsigned char *p = get_ele_ptr(idx, &avail);

        /* use the whole chunk, up to the amount remaining in the request */
        size_t cur = (len < (long)avail ? (size_t)len : avail);

        /* feed this chunk into the hash */
        md5_append(&ctx, p, cur);

        /* deduct this chunk from the remaining length */
        len -= cur;
    }

    /* calculate the hash result */
    const int HASH_BYTES = 16;
    unsigned char hash[HASH_BYTES];
    md5_finish(&ctx, hash);

    /* convert the binary hash to printable hex digits */
    char buf[HASH_BYTES*2], *bufp = buf;
    for (int i = 0 ; i < HASH_BYTES ; ++i, bufp += 2)
        byte_to_xdigits(bufp, hash[i]);

    /* return the string */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, buf, HASH_BYTES*2));

    /* handled */
    return TRUE;
}


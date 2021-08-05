#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmimgrb.cpp - T3 Image File Re-Builder
Function
  This module re-builds an image file from the contents of memory after
  the program has been "pre-initialized."  This allows a program to
  run through its static state initialization during the compilation
  process, then store the result as a new image file with pre-initialized
  state.  Any initialization that must always happen for every run of
  the program can be performed during this pre-initialization pass,
  saving the time of doing the work each time the program is run.

  Mostly, we just copy the old image file to the new image file; most
  parts of the image file are copied without changes.  We update the
  object stream, replacing the original objects with the objects in
  their pre-initialized state, and we add any new strings dynamically
  created during pre-initialization to the constant pool.
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>

#include "t3std.h"
#include "vmfile.h"
#include "vmimage.h"
#include "vmpool.h"
#include "vmglob.h"
#include "vmwrtimg.h"
#include "vmobj.h"
#include "vmtobj.h"
#include "vmdict.h"
#include "vmhash.h"
#include "vmlst.h"
#include "vmbignum.h"
#include "vmstr.h"
#include "vmgram.h"
#include "vmmeta.h"
#include "vmimgrb.h"
#include "vmintcls.h"
#include "vmiter.h"
#include "vmvec.h"
#include "vmlookup.h"
#include "vmstack.h"
#include "vmbytarr.h"
#include "vmcset.h"
#include "vmfilobj.h"
#include "vmtmpfil.h"
#include "vmfilnam.h"
#include "vmpat.h"
#include "vmstrcmp.h"
#include "vmstrbuf.h"
#include "vmdynfunc.h"
#include "vmfref.h"
#include "vmdate.h"
#include "vmtzobj.h"
#include "vmtz.h"

#ifdef TADSNET
#include "vmhttpsrv.h"
#include "vmhttpreq.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Write a string to a buffer with a one-byte prefix 
 */
static char *write_str_byte_prefix(char *ptr, const char *str, size_t len)
{
    /* make sure it fits the byte prefix */
    if (len > 255)
        err_throw(VMERR_STR_TOO_LONG);

    /* write the prefix and the string */
    oswp1(ptr, (char)len);
    memcpy(ptr + 1, str, len);

    /* return the pointer to the next byte after the copied data */
    return ptr + len + 1;
}

#if 0 // not currently needed
static char *write_str_byte_prefix(char *ptr, const char *str)
{
    return write_str_byte_prefix(ptr, str, strlen(str));
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Rebuild the OBJS blocks and write the data to the file.  This goes
 *   through the objects in memory and constructs fixed image-file
 *   versions of the objects, then writes them to OBJS blocks.  
 */
static void vm_rewrite_objs_blocks(VMG_ CVmImageWriter *writer,
                                   class CVmConstMapper *mapper)
{
    size_t i;
    
    /* rewrite the image block for each of our defined metaclasses */
    for (i = 0 ; i < G_meta_table->get_count() ; ++i)
    {
        /* write this metaclass's objects */
        G_obj_table->rebuild_image(vmg_ i, writer, mapper);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Re-write the image file 
 */
void vm_rewrite_image(VMG_ CVmFile *origfp, CVmFile *newfp,
                      ulong static_cs_ofs)
{
    char buf[4096];
    int done;
    CVmImageWriter *writer;
    CVmConstMapper *const_mapper;
    uchar xor_mask;
    ulong code_page_size;

    /* we don't know the code page size yet */
    code_page_size = 0;

    /* choose an arbitrary XOR mask for our pages */
    xor_mask = 0xEF;

    /* create an image writer to help us write the output file */
    writer = new CVmImageWriter(newfp);

    /* create our constant mapper */
    const_mapper = new CVmConstMapper(vmg0_);

    /* 
     *   clear all undo information - we don't save undo with the rebuilt
     *   file, so there's no reason to keep any of the objects that are
     *   referenced only in the undo records 
     */
    G_undo->drop_undo(vmg0_);

    /* discard everything on the stack */
    G_stk->discard(G_stk->get_depth());

    /* 
     *   perform a full garbage collection pass, to make sure we don't
     *   include any unreachable objects in the new image file 
     */
    G_obj_table->gc_full(vmg0_);

    /* add any metaclasses that weren't in the dependency table originally */
    G_obj_table->add_metadeps_for_instances(vmg0_);

    /* convert objects to constant data to the extent possible */
    G_obj_table->rebuild_image_convert_const_data(vmg_ const_mapper);

    /* 
     *   copy the header (signature, UINT2 format version number, 32
     *   reserved bytes, 24-byte compilation timestamp) to the new file 
     */
    origfp->read_bytes(buf, sizeof(VMIMAGE_SIG) - 1 + 2 + 32 + 24);
    newfp->write_bytes(buf, sizeof(VMIMAGE_SIG) - 1 + 2 + 32 + 24);

    /* copy or replace the data blocks */
    for (done = FALSE ; !done ; )
    {
        ulong siz;

        /* read the next block header */
        origfp->read_bytes(buf, 10);

        /* get the size */
        siz = t3rp4u(buf + 4);

        /* check the block type */
        if (CVmImageLoader::block_type_is(buf, "OBJS")
            || CVmImageLoader::block_type_is(buf, "MCLD")
            || CVmImageLoader::block_type_is(buf, "SINI"))
        {
            /* 
             *   Simply skip all of the original OBJS (object data) or
             *   MCLD (metaclass dependency table) blocks -- we'll replace
             *   them with our re-built blocks.
             *   
             *   Also skip SINI (static initializer) blocks, since these
             *   are only needed for pre-initialization and can be
             *   discarded from the final image file.  
             */
            origfp->set_pos(origfp->get_pos() + (long)siz);
        }
        else if (CVmImageLoader::block_type_is(buf, "CPDF"))
        {
            char cpdf_buf[20];
            uint pool_id;
            ulong pgcnt;
            ulong pgsiz;
            
            /* read the pool definition */
            origfp->read_bytes(cpdf_buf, 10);

            /* get the ID, page count, and page size from the definition */
            pool_id = osrp2(cpdf_buf);
            pgcnt = t3rp4u(cpdf_buf + 2);
            pgsiz = t3rp4u(cpdf_buf + 6);

            /* 
             *   if this is the constant pool (pool ID = 2), increase the
             *   page count by the extra constant pool pages we need for
             *   converting new object data to constants 
             */
            if (pool_id == 2)
            {
                /* add in our new count */
                pgcnt += const_mapper->get_page_count();

                /* write the new count */
                oswp4(cpdf_buf + 2, pgcnt);
            }

            /*
             *   if this is the code pool (pool ID = 1), and we have
             *   static initializers, decrease the page count to exclude
             *   the static initializer pages (all of the static
             *   initializers are grouped at the high end of the code
             *   pool, so we can discard them and only them by truncating
             *   the code pool before the page containing the first static
             *   initializer) 
             */
            if (pool_id == 1 && static_cs_ofs != 0)
            {
                /* 
                 *   calculate the new count - it's the offset to the
                 *   first static initializer divided by the size of each
                 *   code page 
                 */
                pgcnt = static_cs_ofs / pgsiz;
                
                /* write the new count */
                oswp4(cpdf_buf + 2, pgcnt);

                /* 
                 *   remember the code page size for later, when we're
                 *   scanning the code pages themselves 
                 */
                code_page_size = pgsiz;
            }
            
            /* update the constant block definition */
            newfp->write_bytes(buf, 10);
            newfp->write_bytes(cpdf_buf, 10);
        }
        else if (CVmImageLoader::block_type_is(buf, "CPPG"))
        {
            char cppg_buf[20];
            long start_pos;
            uint pool_id;
            ulong page_idx;
            int keep_block;

            /* presume we're going to keep this block */
            keep_block = TRUE;
            
            /* 
             *   This is a constant page - if it's in pool 2 (constants),
             *   use its XOR mask for any new pages we write.  First, read
             *   the pool header, then seek back so we can copy the block
             *   unchanged.  
             */
            start_pos = origfp->get_pos();
            origfp->read_bytes(cppg_buf, 7);
            origfp->set_pos(start_pos);

            /* get the pool ID and the page's index */
            pool_id = osrp2(cppg_buf);
            page_idx = t3rp4u(cppg_buf + 2);

            /* if it's pool 2 (constants), read its XOR mask byte */
            if (pool_id == 2)
                xor_mask = cppg_buf[6];

            /* 
             *   if it's pool 1 (code), and it's above the start of the
             *   static initializers, skip it - we don't want to copy
             *   static initializer code to the final image file, since
             *   they're only needed for static initialization, which we
             *   necessarily have finished by the time we reach this point 
             */
            if (pool_id == 1
                && static_cs_ofs != 0
                && page_idx * code_page_size >= static_cs_ofs)
            {
                /* this is a static initializer block - skip it */
                keep_block = FALSE;
            }

            /* keep or skip the block, as appropriate */
            if (keep_block)
            {
                /* 
                 *   we only wanted to get information from this block, so
                 *   go copy it as-is to the output 
                 */
                goto copy_block;
            }
            else
            {
                /* skip past the block */
                origfp->set_pos(origfp->get_pos() + (long)siz);
            }
        }
        else if (CVmImageLoader::block_type_is(buf, "EOF "))
        {
            /* end of file - note that we're done after this block */
            done = TRUE;

            /* re-write the metaclass dependency block */
            G_meta_table->rebuild_image(writer);

            /* write our new OBJS blocks */
            vm_rewrite_objs_blocks(vmg_ writer, const_mapper);

            /* write our new constant pool pages */
            const_mapper->write_to_image_file(writer, xor_mask);

            /* copy the EOF block to the new file unchanged */
            goto copy_block;
        }
        else
        {
        copy_block:
            /*
             *   For anything else, we'll simply copy the original block
             *   from the original image file unchanged. 
             */
            
            /* write the block header unchanged */
            newfp->write_bytes(buf, 10);

            /* copy the block in chunks */
            while (siz != 0)
            {
                size_t cur;

                /* 
                 *   read as much as we can, up to the amount remaining or
                 *   the buffer size, whichever is smaller 
                 */
                cur = sizeof(buf);
                if (cur > siz)
                    cur = (size_t)siz;

                /* read the data and write it out */
                origfp->read_bytes(buf, cur);
                newfp->write_bytes(buf, cur);

                /* deduct this chunk from the amount remaining */
                siz -= cur;
            }
        }
    }

    /* we're done with the image writer */
    delete writer;

    /* we're done with the constant mapper */
    delete const_mapper;
}

/* ------------------------------------------------------------------------ */
/*
 *   Object Table extension - rebuild the image file representations for
 *   all objects of a particular metaclass.
 */
void CVmObjTable::rebuild_image(VMG_ int meta_dep_idx, CVmImageWriter *writer,
                                class CVmConstMapper *mapper)
{
    int pass;
    
    /* write persistent and transient objects separately */
    for (pass = 1 ; pass <= 2 ; ++pass)
    {
        int trans;

        /* write persistent on pass 1, transient on pass 2 */
        trans = (pass == 2);

        /* write the objects for this pass */
        rebuild_image(vmg_ meta_dep_idx, writer, mapper, trans);
    }
}

/*
 *   Write all of the transient or persistent objects of a given metaclass. 
 */
void CVmObjTable::rebuild_image(VMG_ int meta_dep_idx, CVmImageWriter *writer,
                                class CVmConstMapper *mapper, int trans)
{
    CVmObjPageEntry **pg;
    size_t i;
    vm_obj_id_t id;
    char *buf;
    ulong bufsiz;
    int block_cnt;
    ulong block_size;
    int large_objects;

    /* we haven't written anything to the file yet */
    block_cnt = 0;
    block_size = 0;

    /* presume we'll use small objects */
    large_objects = FALSE;

    /* 
     *   allocate an initial object buffer - we'll expand this as needed
     *   if we find an object that doesn't fit 
     */
    bufsiz = 4096;
    buf = (char *)t3malloc((size_t)bufsiz);
    if (buf == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* go through each page in the object table */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        size_t j;
        CVmObjPageEntry *entry;

        /* start at the start of the page */
        j = VM_OBJ_PAGE_CNT;
        entry = *pg;

        /* go through each entry on this page */
        for ( ; j > 0 ; --j, ++entry, ++id)
        {
            /* 
             *   if this entry is in use, and its transient/persistent type
             *   matches the type we're writing, and its metaclass matches
             *   the one we're writing, write it out 
             */
            if (!entry->free_
                && (entry->transient_ != 0) == (trans != 0)
                && (G_meta_table->get_dependency_index(
                    entry->get_vm_obj()->get_metaclass_reg()->get_reg_idx())
                    == meta_dep_idx))
            {
                ulong objsiz;

                /* 
                 *   if this object has been mapped to a constant value,
                 *   there's no need to store it, since it is no longer
                 *   reachable 
                 */
                if (mapper->get_pool_addr(id) != 0)
                    continue;

                /* try building the object */
                objsiz = entry->get_vm_obj()->rebuild_image(vmg_ buf, bufsiz);

                /* if they need more space, reallocate the buffer */
                if (objsiz > bufsiz)
                {
                    /* if the object is too large, throw an error */
                    if (objsiz > OSMALMAX)
                        err_throw(VMERR_OBJ_SIZE_OVERFLOW);
                    
                    /* reallocate to next 4k increment */
                    bufsiz = (objsiz + 4095) & ~4095;
                    buf = (char *)t3realloc(buf, (size_t)bufsiz);
                    if (buf == 0)
                        err_throw(VMERR_OUT_OF_MEMORY);

                    /* try it again */
                    objsiz = entry->get_vm_obj()->
                             rebuild_image(vmg_ buf, bufsiz);
                }

                /* if the object is not empty, write it out */
                if (objsiz != 0)
                {
                    char prefix[20];

                    /* 
                     *   if this object's size exceeds 64k, and the
                     *   current OBJS block is a small block, end this
                     *   OBJS block and begin a large OBJS block 
                     */
                    if (objsiz > 65535 && !large_objects)
                    {
                        /* if we have a block open, end it */
                        if (block_cnt != 0)
                            writer->end_objs_block(block_cnt);

                        /* reset the count and size for the new block */
                        block_cnt = 0;
                        block_size = 0;

                        /* make the next block a large block */
                        large_objects = TRUE;
                    }
                    
                    /* 
                     *   If this object plus its prefix would push this OBJS
                     *   block over 64k, and we have a small-object block
                     *   open, close it off and start a new block.  Don't do
                     *   this if we're already in a large-objects block,
                     *   since that means we have a single object that we
                     *   can't fit in 64k, meaning that it would be
                     *   impossible to keep the block itself to within 64k.  
                     */
                    if (block_size + objsiz + 6 > 64000L
                        && block_cnt != 0
                        && !large_objects)
                    {
                        /* close this block */
                        writer->end_objs_block(block_cnt);

                        /* reset the count and size for the new block */
                        block_cnt = 0;
                        block_size = 0;

                        /* 
                         *   go bak to the small-object block format if this
                         *   object doesn't itself require a large-objects
                         *   block 
                         */
                        if (objsiz <= 65535)
                            large_objects = FALSE;
                    }
                    
                    /* if this is the first object, write the header */
                    if (block_cnt == 0)
                        writer->begin_objs_block(meta_dep_idx, large_objects,
                                                 trans);

                    /* write the prefix information */
                    oswp4(prefix, id);
                    if (large_objects)
                    {
                        /* write the 32-bit object size */
                        oswp4(prefix + 4, objsiz);

                        /* write the header */
                        writer->write_objs_bytes(prefix, 8);
                    }
                    else
                    {
                        /* write the 16-bit object size */
                        oswp2(prefix + 4, objsiz);

                        /* write the header */
                        writer->write_objs_bytes(prefix, 6);
                    }

                    /* write the object data */
                    writer->write_objs_bytes(buf, objsiz);

                    /* count the object */
                    ++block_cnt;

                    /* count the size (including the prefix) */
                    block_size += objsiz + 6;
                }
            }
        }
    }

    /* if we wrote any objects, end the OBJS block */
    if (block_cnt != 0)
        writer->end_objs_block(block_cnt);

    /* delete our buffer */
    t3free(buf);
}

/*
 *   Scan all active objects and convert objects to constant data where
 *   possible.  Certain object metaclasses, such as strings and lists, can
 *   be represented in a rebuilt image file as constant data; this routine
 *   makes all of these conversions.  
 */
void CVmObjTable::rebuild_image_convert_const_data
   (VMG_ CVmConstMapper *const_mapper)
{
    CVmObjPageEntry **pg;
    size_t i;
    vm_obj_id_t id;

    /* 
     *   First pass: go through each page in the object table, and reserve
     *   space for the constant conversion.  This assigns each object that
     *   can be converted its address in the new constant pool pages. 
     */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        size_t j;
        CVmObjPageEntry *entry;

        /* start at the start of the page, but skip object ID = 0 */
        j = VM_OBJ_PAGE_CNT;
        entry = *pg;

        /* go through each entry on this page */
        for ( ; j > 0 ; --j, ++entry, ++id)
        {
            /* 
             *   if this entry is in use, tell it to reserve space for
             *   conversion to constant data 
             */
            if (!entry->free_)
                entry->get_vm_obj()
                    ->reserve_const_data(vmg_ const_mapper, id);
        }
    }

    /* prepare the constant mapper to begin storing */
    const_mapper->prepare_to_store_data();

    /*
     *   Second pass: go through the objects once again and make the
     *   conversions.  We must do this on a separate second pass because
     *   we must fix up all references to objects being converted, hence
     *   we must know the conversion status of every object to be
     *   converted before we can fix up anything.  
     */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        size_t j;
        CVmObjPageEntry *entry;

        /* start at the start of the page, but skip object ID = 0 */
        j = VM_OBJ_PAGE_CNT;
        entry = *pg;

        /* go through each entry on this page */
        for ( ; j > 0 ; --j, ++entry, ++id)
        {
            /* if this entry is in use, convert it if possible */
            if (!entry->free_)
                entry->get_vm_obj()
                    ->convert_to_const_data(vmg_ const_mapper, id);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert a value to constant data.  If the value is an object that has
 *   reserved constant data space for itself, we'll update the value to
 *   reflect the constant data conversion for the value.  
 */
static void convert_dh_to_const_data(VMG_ CVmConstMapper *mapper, char *p)
{
    /* if the value is an object, check for conversion */
    if (vmb_get_dh_type(p) == VM_OBJ)
    {
        vm_obj_id_t obj;
        ulong addr;

        /* get the object value */
        obj = vmb_get_dh_obj(p);

        /* look up the object's converted constant pool address */
        if ((addr = mapper->get_pool_addr(obj)) != 0)
        {
            vm_val_t val;

            /* this value has been converted - set the new value */
            val.typ = vm_objp(vmg_ obj)->get_convert_to_const_data_type();
            val.val.ofs = addr;
            vmb_put_dh(p, &val);
        }
    }
}

/*
 *   Convert a vm_val_t value to constant data if appropriate. 
 */
static void convert_val_to_const_data(VMG_ CVmConstMapper *mapper,
                                      vm_val_t *val)
{
    /* if it's an object, check for conversion */
    if (val->typ == VM_OBJ)
    {
        ulong addr;

        /* look up the object's converted constant pool address */
        if ((addr = mapper->get_pool_addr(val->val.obj)) != 0)
        {
            /* this value has been converted - set the new value */
            val->typ = vm_objp(vmg_ val->val.obj)
                       ->get_convert_to_const_data_type();
            val->val.ofs = addr;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   List Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record for the list.  We need this because we
 *   can't always convert a list to a constant value when rebuilding an
 *   image file; in particular, if the list exceeds the size of a constant
 *   pool page (unlikely but possible), we will have to store the list as
 *   object data.  
 */
ulong CVmObjList::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;

    /* calculate how much space we need to store the data */
    copy_size = calc_alloc(vmb_get_len(ext_));
    
    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* copy the data */
    memcpy(buf, ext_, copy_size);

    /* return the size */
    return copy_size;
}

/*
 *   Reserve space in a rebuilt constant pool for converting our data to a
 *   constant value.
 */
void CVmObjList::reserve_const_data(VMG_ CVmConstMapper *mapper,
                                    vm_obj_id_t self)
{
    /* reserve the space for our list data */
    mapper->alloc_pool_space(self, calc_alloc(vmb_get_len(ext_)));
}


/*
 *   Convert to constant data.  We must check each object that we
 *   reference via a modified property and convert it to a constant data
 *   item if necessary.  Then, we must store our data in the constant
 *   pool, if we successfully reserved an address for it.  
 */
void CVmObjList::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                       vm_obj_id_t self)
{
    size_t cnt;
    char *p;

    /* get my element count */
    cnt = vmb_get_len(ext_);

    /* mark as referenced each object in our list */
    for (p = get_element_ptr(0) ; cnt != 0 ; --cnt, inc_element_ptr(&p))
    {
        /* convert the value to constant data if possible */
        convert_dh_to_const_data(vmg_ mapper, p);
    }

    /* 
     *   if we managed to convert our object to constant data, store our
     *   value 
     */
    if (mapper->get_pool_addr(self))
        mapper->store_data(self, ext_, calc_alloc(vmb_get_len(ext_)));
}


/* ------------------------------------------------------------------------ */
/*
 *   String Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record for the string.  We need this because we
 *   can't always convert a string to a constant value when rebuilding an
 *   image file; in particular, if the string exceeds the size of a
 *   constant pool page (unlikely but possible), we will have to store the
 *   string as object data.  
 */
ulong CVmObjString::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;

    /* calculate how much space we need to store the data */
    copy_size = vmb_get_len(ext_) + VMB_LEN;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* copy the data */
    memcpy(buf, ext_, copy_size);

    /* return the size */
    return copy_size;
}

/*
 *   Reserve space in a rebuilt constant pool for converting our data to a
 *   constant value.
 */
void CVmObjString::reserve_const_data(VMG_ CVmConstMapper *mapper,
                                      vm_obj_id_t self)
{
    /* reserve the space for our string data */
    mapper->alloc_pool_space(self, vmb_get_len(ext_) + VMB_LEN);
}


/*
 *   Convert to constant data.  We don't reference any objects, so we need
 *   simply store our data in the constant pool, if we successfully
 *   reserved an address for it.  
 */
void CVmObjString::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                         vm_obj_id_t self)
{
    /* 
     *   if we managed to convert our object to constant data, store our
     *   value 
     */
    if (mapper->get_pool_addr(self))
        mapper->store_data(self, ext_, vmb_get_len(ext_) + VMB_LEN);
}


/* ------------------------------------------------------------------------ */
/*
 *   TADS Object implementation - image rebuilding 
 */


/* property comparison callback for qsort() */
extern "C"
{
    static int prop_compare(const void *p1, const void *p2)
    {
        uint id1, id2;

        /* get the ID's */
        id1 = osrp2(p1);
        id2 = osrp2(p2);

        /* compare them and return the result */
        return (id1 < id2 ? -1 : id1 == id2 ? 0 : 1);
    }
}

/*
 *   build an image file record for the object 
 */
ulong CVmObjTads::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* 
     *   Make sure the buffer is big enough.  Start out with worst-case
     *   assumption that we'll need every allocated property slot; we might
     *   actually need fewer, since some of the slots could be empty by
     *   virtue of having been undone.  We need space for our own header
     *   (UINT2 superclass count, UINT2 property count, UINT2 flags), plus a
     *   UINT4 per superclass, plus a (UINT2 + DATAHOLDER) per property.  
     */
    size_t max_size = (2 + 2 + 2)
                      + get_sc_count() * 4
                      + (get_hdr()->prop_entry_free * 7);

    /* if it's more than we have available, ask for more space */
    if (max_size > buflen)
        return max_size;

    /* 
     *   set up our header - use a placeholder 0 for the property count
     *   for now, until we calculate the real value 
     */
    oswp2(buf, get_sc_count());
    oswp2(buf+2, 0);
    oswp2(buf+4, get_li_obj_flags());
    char *p = buf + 6;

    /* copy the superclass list */
    for (size_t i = 0 ; i < get_sc_count() ; ++i, p += 4)
        oswp4(p, get_sc(i));

    /* remember where the properties start */
    char *props = p;

    /* copy the non-empty property slots */
    vm_tadsobj_prop *entry = get_hdr()->prop_entry_arr;
    size_t prop_cnt = 0;
    for (size_t i = get_hdr()->prop_entry_free ; i != 0 ; --i, ++entry)
    {
        /* if this slot is non-empty, store it */
        if (entry->val.typ != VM_EMPTY)
        {
            /* set up this slot */
            oswp2(p, entry->prop);
            vmb_put_dh(p + 2, &entry->val);

            /* count the additional property */
            ++prop_cnt;

            /* move past it in the buffer */
            p += 7;
        }
    }

    /* fill in actual the property count now that we know it */
    oswp2(buf+2, prop_cnt);

    /* sort the new table */
    qsort(props, prop_cnt, 7, &prop_compare);

    /* return the final size */
    return p - buf;
}

/*
 *   Convert to constant data.  We must check each object that we
 *   reference via a modified property and convert it to a constant data
 *   item if necessary.  
 */
void CVmObjTads::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                       vm_obj_id_t /*self*/)
{
    size_t i;
    vm_tadsobj_prop *entry;

    /* 
     *   Scan the property entries.  Note that we don't have to worry about
     *   the original properties, since they can only refer to data that was
     *   in the original image file, and hence never need to be converted --
     *   if it was good enough for the original image file, it's good enough
     *   for us.  
     */
    for (i = get_hdr()->prop_entry_free, entry = get_hdr()->prop_entry_arr ;
         i != 0 ; --i, ++entry)
    {
        /* if this slot is modified, convert it */
        if ((entry->flags & VMTO_PROP_MOD) != 0
            && entry->val.typ != VM_EMPTY)
        {
            /* convert the value */
            convert_val_to_const_data(vmg_ mapper, &entry->val);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Dictionary object implementation - image rebuilding 
 */

/* 
 *   callback context for image rebuild 
 */
struct rebuild_ctx
{
    /* space needed so far */
    size_t space_needed;

    /* number of entries */
    size_t entry_cnt;

    /* next available output pointer */
    char *dst;
};

/* 
 *   rebuild for image file 
 */
ulong CVmObjDict::rebuild_image(VMG_ char *buf, ulong buflen)
{
    rebuild_ctx ctx;
    
    /* 
     *   calculate the amount of space we need - start with the comparator
     *   object, which needs four bytes, and the entry count, which needs two
     *   bytes 
     */
    ctx.space_needed = 6;

    /* enumerate the entries to count space */
    ctx.entry_cnt = 0;
    get_ext()->hashtab_->enum_entries(&rebuild_cb_1, &ctx);

    /* if we need more space than is available, ask for more */
    if (ctx.space_needed > buflen)
        return ctx.space_needed;

    /* write the comparator object and the entry count */
    oswp4(buf, get_ext()->comparator_);
    oswp2(buf + 4, ctx.entry_cnt);

    /* start writing after the count */
    ctx.dst = buf + 6;

    /* enumerate the entries to write to the image buffer */
    get_ext()->hashtab_->enum_entries(&rebuild_cb_2, &ctx);

    /* return the size */
    return ctx.dst - buf;
}

/*
 *   enumeration callback - rebuild phase 1: count the space needed for
 *   this table entry 
 */
void CVmObjDict::rebuild_cb_1(void *ctx0, class CVmHashEntry *entry0)
{
    rebuild_ctx *ctx = (rebuild_ctx *)ctx0;
    CVmHashEntryDict *entry = (CVmHashEntryDict *)entry0;
    vm_dict_entry *cur;

    /* 
     *   count the space needed for this string - one byte for the length
     *   prefix, the bytes of the name string, and two bytes for the item
     *   count 
     */
    ctx->space_needed += 1 + entry->getlen() + 2;

    /* count this entry */
    ++ctx->entry_cnt;

    /* count the items */
    for (cur = entry->get_head() ; cur != 0 ; cur = cur->nxt_)
    {
        /* 
         *   add the space for this item - four byte for the object ID,
         *   two bytes for the property ID 
         */
        ctx->space_needed += 6;
    }
}

/*
 *   enumeration callback - rebuild phase 2: write the entries to the
 *   image buffer 
 */
void CVmObjDict::rebuild_cb_2(void *ctx0, class CVmHashEntry *entry0)
{
    rebuild_ctx *ctx = (rebuild_ctx *)ctx0;
    CVmHashEntryDict *entry = (CVmHashEntryDict *)entry0;
    vm_dict_entry *cur;
    size_t cnt;
    char *p;
    size_t rem;

    /* count the entries in our list */
    for (cnt = 0, cur = entry->get_head() ; cur != 0 ;
         cur = cur->nxt_, ++cnt) ;

    /* write the length of the key string followed by the key string */
    *ctx->dst++ = (char)entry->getlen();
    memcpy(ctx->dst, entry->getstr(), entry->getlen());

    /* xor the key string's bytes */
    for (p = ctx->dst, rem = entry->getlen() ; rem != 0 ;
         --rem, ++p)
        *p ^= 0xBD;

    /* move past the key string */
    ctx->dst += entry->getlen();

    /* write the item count */
    oswp2(ctx->dst, cnt);
    ctx->dst += 2;

    /* write the items */
    for (cur = entry->get_head() ; cur != 0 ; cur = cur->nxt_)
    {
        /* write the object ID and property ID for this item */
        oswp4(ctx->dst, (ulong)cur->obj_);
        oswp2(ctx->dst + 4, (uint)cur->prop_);
        ctx->dst += 6;
    }
}

/*
 *   callback context for constant data conversion 
 */
struct cvt_const_ctx
{
    /* constant mapper */
    CVmConstMapper *mapper;
};

/* 
 *   convert to constant data 
 */
void CVmObjDict::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                       vm_obj_id_t self)
{
    cvt_const_ctx ctx;
    
    /* make sure the comparator object isn't mappable to a constant */
    if (mapper->get_pool_addr(get_ext()->comparator_) != 0)
        err_throw_a(VMERR_DICT_NO_CONST, 1, ERR_TYPE_TEXTCHAR_LEN,
                    "<comparator>", 12);

    /* 
     *   Go through our dictionary and make sure we don't have any
     *   references to constant data.  We don't actually have to perform
     *   any conversions, because we simply don't allow references to
     *   anything but TADS-object objects in the dictionary.  
     */
    ctx.mapper = mapper;
    get_ext()->hashtab_->enum_entries(&cvt_const_cb, &ctx);
}

/*
 *   enumeration callback - convert to constant data 
 */
void CVmObjDict::cvt_const_cb(void *ctx0, class CVmHashEntry *entry0)
{
    cvt_const_ctx *ctx = (cvt_const_ctx *)ctx0;
    CVmHashEntryDict *entry = (CVmHashEntryDict *)entry0;
    vm_dict_entry *cur;

    /* inspect the items in this entry's list */
    for (cur = entry->get_head() ; cur != 0 ; cur = cur->nxt_)
    {
        /* if this item refers to constant data, it's an error */
        if (ctx->mapper->get_pool_addr(cur->obj_) != 0)
            err_throw_a(VMERR_DICT_NO_CONST, 1, ERR_TYPE_TEXTCHAR_LEN,
                        entry->getstr(), entry->getlen());
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production object - image rebuilding operations 
 */

/* 
 *   rebuild for image file 
 */
ulong CVmObjGramProd::rebuild_image(VMG_ char *buf, ulong buflen)
{
    if (get_ext()->modified_)
    {
        /* 
         *   We've been modified at run-time, so we need to rebuild the image
         *   data from the current alt list.  First, save to a counting
         *   stream just to determine how much space we need.  
         */
        CVmCountingStream cstr;
        save_to_stream(vmg_ &cstr);

        /* make sure we have enough space */
        if ((ulong)cstr.get_len() > buflen)
            return cstr.get_len();

        /* save to the buffer */
        CVmMemoryStream mstr(buf, buflen);
        save_to_stream(vmg_ &mstr);

        /* return the stream length */
        return cstr.get_len();
    }
    else
    {
        /* 
         *   We weren't modified, so simply copy back our original image data
         *   unchanged. 
         */

        /* make sure we have room */
        if (get_ext()->image_data_size_ > buflen)
            return get_ext()->image_data_size_;
        
        /* copy the data */
        memcpy(buf, get_ext()->image_data_, get_ext()->image_data_size_);

        /* return the size */
        return get_ext()->image_data_size_;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   BigNumber Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record
 */
ulong CVmObjBigNum::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;

    /* calculate how much space we need to store the data */
    copy_size = calc_alloc(get_prec(ext_));

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* copy the data */
    memcpy(buf, ext_, copy_size);

    /* return the size */
    return copy_size;
}

/*
 *   Reserve space in a rebuilt constant pool for converting our data to a
 *   constant value.
 */
void CVmObjBigNum::reserve_const_data(VMG_ CVmConstMapper *mapper,
                                      vm_obj_id_t self)
{
    /* BigNumber values cannot be converted to constants */
}


/*
 *   Convert to constant data.  We don't reference any other objects, so
 *   we simply need to store our data in the constant pool, if we
 *   successfully reserved an address for it.  
 */
void CVmObjBigNum::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                         vm_obj_id_t self)
{
    /* 
     *   if we managed to convert our object to constant data, store our
     *   value 
     */
    if (mapper->get_pool_addr(self))
        mapper->store_data(self, ext_, calc_alloc(get_prec(ext_)));
}


/* ------------------------------------------------------------------------ */
/*
 *   Object-to-Constant mapper 
 */

/*
 *   initialize 
 */
CVmConstMapper::CVmConstMapper(VMG0_)
{
    vm_obj_id_t max_id;
    size_t i;
    
    /* get the maximum object ID we've allocated */
    max_id = G_obj_table->get_max_used_obj_id();

    /* 
     *   Allocate our master translation page list so that we have room
     *   for enough pages to store the maximum object ID.  We need one
     *   slot for each page of 1024 translation elements.  
     */
    obj_addr_cnt_ = max_id / 1024;
    obj_addr_ = (ulong **)t3malloc(obj_addr_cnt_ * sizeof(obj_addr_[0]));
    if (obj_addr_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* clear out the page slots - we haven't allocated any pages yet */
    for (i = 0 ; i < obj_addr_cnt_ ; ++i)
        obj_addr_[i] = 0;

    /* get the constant pool's page size */
    page_size_ = G_const_pool->get_page_size();

    /* 
     *   our first page is the next page after the last page in the
     *   current pool 
     */
    first_page_idx_ = G_const_pool->get_page_count();

    /* 
     *   get the starting address - we'll start writing our data at the
     *   first page after all existing pages in the pool 
     */
    base_addr_ = page_size_ * G_const_pool->get_page_count();

    /* allocate from our base address */
    next_free_ = base_addr_;

    /* we have the entire first page available */
    rem_ = page_size_;

    /* 
     *   we haven't allocated any page data yet (we'll do this after we've
     *   reserved all space, when the client invokes
     *   prepare_to_store_data()) 
     */
    pages_ = 0;
    pages_cnt_ = 0;
}

/*
 *   delete 
 */
CVmConstMapper::~CVmConstMapper()
{
    size_t i;
    
    /* delete each page in our mapping table */
    for (i = 0 ; i < obj_addr_cnt_ ; ++i)
    {
        /* free this page if we allocated it */
        if (obj_addr_[i] != 0)
            t3free(obj_addr_[i]);
    }

    /* delete our mapping table */
    t3free(obj_addr_);

    /* delete each constant pool page */
    for (i = 0 ; i < pages_cnt_ ; ++i)
        t3free(pages_[i]);

    /* free the page list */
    if (pages_ != 0)
        t3free(pages_);
}

/*
 *   Get an object's pool address, if it has one 
 */
ulong CVmConstMapper::get_pool_addr(vm_obj_id_t obj_id)
{
    /* determine if the page is mapped - if not, the object isn't mapped */
    if (obj_addr_[obj_id / 1024] == 0)
        return 0;

    /* return the mapping entry */
    return obj_addr_[obj_id / 1024][obj_id % 1024];
}


/*
 *   Allocate space in the pool for an object's data
 */
ulong CVmConstMapper::alloc_pool_space(vm_obj_id_t obj_id, size_t len)
{
    ulong ret;
    
    /* 
     *   if the data block is too large to fit on a constant pool page, we
     *   can't store it 
     */
    if (len > page_size_)
        return 0;
    
    /* if the translation page isn't mapped yet, map it */
    if (obj_addr_[obj_id / 1024] == 0)
    {
        /* allocate a new page */
        obj_addr_[obj_id / 1024] =
            (ulong *)t3malloc(1024 * sizeof(obj_addr_[0][0]));

        /* if that failed, throw an error */
        if (obj_addr_[obj_id / 1024] == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* clear the new page */
        memset(obj_addr_[obj_id / 1024], 0, 1024 * sizeof(obj_addr_[0][0]));
    }

    /* if the block doesn't fit on this page, skip to the next page */
    if (len > rem_)
    {
        /* skip past the remainder of this page */
        next_free_ += rem_;

        /* we have the whole next page available now */
        rem_ = page_size_;
    }

    /* remember the object ID's address in the translation list */
    ret = obj_addr_[obj_id / 1024][obj_id % 1024] = next_free_;

    /* skip past the data */
    next_free_ += len;
    rem_ -= len;

    /* return the new address */
    return ret;
}

/*
 *   Prepare to begin storing data 
 */
void CVmConstMapper::prepare_to_store_data()
{
    size_t i;
    
    /* figure out how many pages we need */
    pages_cnt_ = calc_page_count();

    /* allocate space for the list of pages */
    pages_ = (vm_const_mapper_page **)
             t3malloc(pages_cnt_ * sizeof(pages_[0]));
    if (pages_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* allocate each page */
    for (i = 0 ; i < pages_cnt_ ; ++i)
    {
        /* 
         *   allocate this page - allocate the header structure plus space
         *   for the page data 
         */
        pages_[i] = (vm_const_mapper_page *)
                    t3malloc(sizeof(pages_[i]) + page_size_);
        if (pages_[i] == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* the page has nothing stored yet */
        pages_[i]->max_ofs_used = 0;
    }
}

/*
 *   Store an object's data 
 */
void CVmConstMapper::store_data(vm_obj_id_t obj_id,
                                const void *ptr, size_t len)
{
    ulong addr;
    size_t page_idx;
    size_t page_ofs;

    /* get the pool address that was reserved for the object */
    addr = get_pool_addr(obj_id);

    /* if the object had no space reserved, ignore the request */
    if (addr == 0)
        return;

    /* figure out which page this address is in, and the offset in the page */
    page_idx = (size_t)((addr - base_addr_) / page_size_);
    page_ofs = (size_t)((addr - base_addr_) % page_size_);

    /* 
     *   if this address takes us above the high-water mark for the page,
     *   move the page's marker accordingly 
     */
    if (page_ofs + len > pages_[page_idx]->max_ofs_used)
        pages_[page_idx]->max_ofs_used = page_ofs + len;

    /* copy the data */
    memcpy(pages_[page_idx]->buf + page_ofs, ptr, len);
}

/*
 *   Write the pool pages to an image file 
 */
void CVmConstMapper::write_to_image_file(CVmImageWriter *writer,
                                         uchar xor_mask)
{
    size_t i;
    
    /* go through each of our pages */
    for (i = 0 ; i < pages_cnt_ ; ++i)
    {
        /* write this page - it's in pool 2 (the constant data pool) */
        writer->write_pool_page(2, first_page_idx_ + i,
                                pages_[i]->buf, pages_[i]->max_ofs_used,
                                TRUE, xor_mask);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   metaclass table - image rewriter implementation 
 */

/*
 *   write the new metaclass dependency table 
 */
void CVmMetaTable::rebuild_image(CVmImageWriter *writer)
{
    size_t i;
    
    /* begin the new metaclass dependency table */
    writer->begin_meta_dep(get_count());

    /* write the new metaclass dependency table items */
    for (i = 0 ; i < get_count() ; ++i)
    {
        vm_meta_entry_t *entry;
        ushort j;

        /* get this entry */
        entry = get_entry(i);
        
        /* write this metaclass name */
        writer->write_meta_dep_item(entry->image_meta_name_);

        /* 
         *   Write the property translation list.  Note that xlat_func()
         *   requires a 1-based index, so we loop from 1 to the count,
         *   rather than following the usual C-style 0-based conventions.  
         */
        for (j = 1 ; j <= entry->func_xlat_cnt_ ; ++j)
            writer->write_meta_item_prop(entry->xlat_func(j));
    }
    
    /* end the metaclass dependency table */
    writer->end_meta_dep();
}


/* ------------------------------------------------------------------------ */
/*
 *   Hashtable Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjLookupTable::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;
    vm_lookup_ext *ext = get_ext();
    uint i;
    vm_lookup_val **bp;
    vm_lookup_val *val;
    char *dst;
    uint idx;

    /* 
     *   we need space for the fixed header (6 bytes), 2 bytes per bucket,
     *   the entries themselves, and the default value 
     */
    copy_size = 6
                + (ext->bucket_cnt * 2)
                + (ext->value_cnt * VMLOOKUP_VALUE_SIZE)
                + VMB_DATAHOLDER;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* write the fixed data */
    oswp2(buf, ext->bucket_cnt);
    oswp2(buf + 2, ext->value_cnt);
    idx = ext->val_to_img_idx(ext->first_free);
    oswp2(buf + 4, idx);
    dst = buf + 6;

    /* write the buckets */
    for (i = ext->bucket_cnt, bp = ext->buckets ; i != 0 ; --i, ++bp)
    {
        /* write this bucket's index */
        idx = ext->val_to_img_idx(*bp);
        oswp2(dst, idx);
        dst += 2;
    }

    /* write the values */
    for (i = ext->value_cnt, val = ext->idx_to_val(0) ; i != 0 ; --i, ++val)
    {
        /* store the key, value, and index */
        vmb_put_dh(dst, &val->key);
        vmb_put_dh(dst + VMB_DATAHOLDER, &val->val);
        idx = ext->val_to_img_idx(val->nxt);
        oswp2(dst + VMB_DATAHOLDER*2, idx);

        /* skip the data in the output */
        dst += VMLOOKUP_VALUE_SIZE;
    }

    /* write the default value */
    vmb_put_dh(dst, &ext->default_value);
    dst += VMB_DATAHOLDER;

    /* return the size */
    return copy_size;
}

/*
 *   Convert to constant data.  We must convert each object we reference in
 *   a key or in a value to constant data if possible.
 */
void CVmObjLookupTable::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                              vm_obj_id_t self)
{
    uint i;
    vm_lookup_val *entry;

    /* run through each entry */
    for (i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; --i, ++entry)
    {
        /* convert the key */
        convert_val_to_const_data(vmg_ mapper, &entry->key);

        /* convert the value */
        convert_val_to_const_data(vmg_ mapper, &entry->val);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   StringBuffer metaclass - image rebuilding 
 */
ulong CVmObjStringBuffer::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;
    vm_strbuf_ext *ext = get_ext();

    /* get our size */
    copy_size = 12 + ext->len*2;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* write the header data */
    oswp4(buf, ext->alo);
    oswp4(buf+4, ext->inc);
    oswp4(buf+8, ext->len);

    /* write the string */
    memcpy(buf+12, ext->buf, ext->len*2);

    /* return the size */
    return copy_size;
}


/* ------------------------------------------------------------------------ */
/*
 *   IntrinsicClass Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjClass::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* figure our size requirement: meta idx, mod id, state value */
    size_t copy_size = 2 + 2 + 4 + VMB_DATAHOLDER;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* copy the data */
    vm_intcls_ext *ext = get_ext();
    oswp2(buf, (short)copy_size);
    oswp2(buf + 2, ext->meta_idx);
    oswp4(buf + 4, ext->mod_obj);
    vmb_put_dh(buf + 8, &ext->class_state);

    /* return the size */
    return copy_size;
}

/* ------------------------------------------------------------------------ */
/*
 *   Indexed Iterator
 */

/*
 *   Build an image file record 
 */
ulong CVmObjIterIdx::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* calculate our data size - just store our entire extension */
    size_t copy_size = VMOBJITERIDX_EXT_SIZE;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* copy the data */
    memcpy(buf, ext_, copy_size);

    /* return the size */
    return copy_size;
}

/*
 *   Convert to constant data.  We must convert our collection object
 *   reference to constant data if possible.  
 */
void CVmObjIterIdx::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                          vm_obj_id_t self)
{
    /* convert our collection reference to constant data if possible */
    convert_dh_to_const_data(vmg_ mapper, ext_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Hashtable Iterator 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjIterLookupTable::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;

    /* calculate our data size - just store our entire extension */
    copy_size = VMOBJITERLOOKUPTABLE_EXT_SIZE;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* copy the data */
    memcpy(buf, ext_, copy_size);

    /* return the size */
    return copy_size;
}

/*
 *   Convert to constant data.  We must convert our collection object
 *   reference to constant data if possible.  
 */
void CVmObjIterLookupTable::convert_to_const_data(
    VMG_ CVmConstMapper *mapper, vm_obj_id_t self)
{
    /* convert our collection reference to constant data if possible */
    convert_dh_to_const_data(vmg_ mapper, ext_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Vector Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjVector::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t copy_size;
    size_t ele_cnt;
    size_t alloc_cnt;

    /* 
     *   calculate how much space we need to store the data - store only
     *   the data, not the undo bits 
     */
    ele_cnt = get_element_count();
    alloc_cnt = get_allocated_count();
    copy_size = 2*VMB_LEN + calc_alloc_ele(ele_cnt);

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* save the allocation count and in-use element count */
    vmb_put_len(buf, alloc_cnt);
    vmb_put_len(buf + VMB_LEN, ele_cnt);

    /* copy the element data */
    memcpy(buf + 2*VMB_LEN, get_element_ptr(0), calc_alloc_ele(ele_cnt));

    /* return the size */
    return copy_size;
}

/*
 *   Reserve space in a rebuilt constant pool for converting our data to a
 *   constant value.
 */
void CVmObjVector::reserve_const_data(VMG_ CVmConstMapper *mapper,
                                      vm_obj_id_t self)
{
    /* Vector values cannot be converted to constants */
}


/*
 *   Convert to constant data.  We must convert each object we reference to
 *   constant data if possible; we use the same algorithm as CVmObjList.  
 */
void CVmObjVector::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                        vm_obj_id_t self)
{
    size_t cnt;
    char *p;

    /* get my element count */
    cnt = get_element_count();

    /* mark as referenced each object in our list */
    for (p = get_element_ptr(0) ; cnt != 0 ; --cnt, inc_element_ptr(&p))
    {
        /* convert the value to constant data if possible */
        convert_dh_to_const_data(vmg_ mapper, p);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   ByteArray Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjByteArray::rebuild_image(VMG_ char *buf, ulong buflen)
{
    ulong copy_size;
    ulong rem;
    ulong idx;

    /* we need four bytes for our count plus space for our byte array */
    copy_size = 4 + get_element_count();

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* store our element count */
    oswp4(buf, get_element_count());
    buf += 4;

    /* copy our data in chunks */
    for (idx = 1, rem = get_element_count() ; rem != 0 ; )
    {
        unsigned char *p;
        size_t avail;
        size_t chunk;

        /* get the next chunk */
        p = get_ele_ptr(idx, &avail);

        /* limit copying to the remaining size */
        chunk = avail;
        if (chunk > rem)
            chunk = rem;

        /* store this chunk */
        memcpy(buf, p, chunk);

        /* skip this chunk */
        buf += chunk;
        idx += chunk;
        rem -= chunk;
    }

    /* return the size */
    return copy_size;
}

/* ------------------------------------------------------------------------ */
/*
 *   CharacterSet Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjCharSet::rebuild_image(VMG_ char *buf, ulong buflen)
{
    ulong copy_size;

    /* we need two bytes for the name length count plus the name's bytes */
    copy_size = 2 + get_ext_ptr()->charset_name_len;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* store the length of the name, and the name itself */
    oswp2(buf, get_ext_ptr()->charset_name_len);
    memcpy(buf + 2, get_ext_ptr()->charset_name,
           get_ext_ptr()->charset_name_len);

    /* return the size */
    return copy_size;
}

/* ------------------------------------------------------------------------ */
/*
 *   File Metaclass implementation - image rebuilding 
 */

/*
 *   Build an image file record 
 */
ulong CVmObjFile::rebuild_image(VMG_ char *buf, ulong buflen)
{
    ulong copy_size;

    /* 
     *   we need the character set object ID, the mode byte, the access
     *   byte, and the uint32 flags 
     */
    copy_size = VMB_OBJECT_ID + 1 + 1 + 4;

    /* make sure we have room for our data */
    if (copy_size > buflen)
        return copy_size;

    /* store the character set object ID */
    vmb_put_objid(buf, get_ext()->charset);
    buf += VMB_OBJECT_ID;

    /* store the mode and access values */
    *buf++ = get_ext()->mode;
    *buf++ = get_ext()->access;

    /* store the flags */
    oswp4(buf, get_ext()->flags);

    /* return the size */
    return copy_size;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTemporaryFile intrinsic object 
 */
ulong CVmObjTemporaryFile::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* we don't store any extra data */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjFileName intrinsic object 
 */
ulong CVmObjFileName::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* get my extension */
    vm_filnam_ext *ext = get_ext();

    /* 
     *   if we're going to store the filename, we'll store it in universal
     *   notation; do the conversion first so we can figure the length
     */
    char *uni = 0;
    ulong needlen;
    size_t unilen = 0;
    err_try
    {
        if (ext->sfid == 0)
        {
            uni = to_universal(vmg0_);
            unilen = strlen(uni);
        }
        
        /* 
         *   figure the required size: special file ID, plus the universal
         *   name with length prefix if the sfid is zero 
         */
        needlen = 4 + (ext->sfid != 0 ? 0 : VMB_LEN + unilen);

        /* if we have room, store the data */
        if (buflen >= needlen)
        {
            /* write the special file ID first */
            oswp4s(buf, ext->sfid);
            buf += 4;

            /* if it's not a special file, store the universal name */
            if (ext->sfid == 0)
            {
                vmb_put_len(buf, unilen);
                memcpy(buf + VMB_LEN, uni, unilen);
            }
        }
    }
    err_finally
    {
        lib_free_str(uni);
    }
    err_end;

    /* return the length */
    return needlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   Pattern metaclass 
 */

/*
 *   build an image file record for the object 
 */
ulong CVmObjPattern::rebuild_image(VMG_ char *buf, ulong buflen)
{
    size_t need_size;

    /* we need a DATAHOLDER to store the original pattern string reference */
    need_size = VMB_DATAHOLDER;

    /* if we need more space, just return our size requirements */
    if (need_size > buflen)
        return need_size;

    /* write our value */
    vmb_put_dh(buf, get_orig_str());

    /* return our size */
    return need_size;
}

/*
 *   Convert to constant data.
 */
void CVmObjPattern::convert_to_const_data(VMG_ CVmConstMapper *mapper,
                                          vm_obj_id_t /*self*/)
{
    /* check our original source string value */
    convert_val_to_const_data(vmg_ mapper, &get_ext()->str);
}

/* ------------------------------------------------------------------------ */
/*
 *   StringComparator intrinsic class 
 */

/*
 *   build the image data 
 */
ulong CVmObjStrComp::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* set up a stream writer on the buffer, and write it out */
    CVmMemoryStream str(buf, buflen);
    return write_to_stream(vmg_ &str, &buflen);
}

/* ------------------------------------------------------------------------ */
/*
 *   DynamicFunc intrinsic class 
 */

/*
 *   build the image data 
 */
ulong CVmDynamicFunc::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* get my extension */
    vm_dynfunc_ext *ext = get_ext();

    /* figure the required size */
    ulong needlen = 2 + 2 + VMB_DATAHOLDER + VMB_DATAHOLDER
                    + (ext->obj_ref_cnt * 2)
                    + ext->bytecode_len;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;
    
    /* save the sizes: bytecode length, object reference count */
    oswp2(buf, ext->bytecode_len);
    oswp2(buf, ext->obj_ref_cnt);
    buf += 4;

    /* save the method context object */
    vmb_put_dh(buf, &ext->method_ctx);
    buf += VMB_DATAHOLDER;

    /* save the source object value */
    vmb_put_dh(buf, &ext->src);
    buf += VMB_DATAHOLDER;

    /* save the byte code */
    memcpy(buf, ext->get_bytecode_ptr(), ext->bytecode_len);
    buf += ext->bytecode_len;

    /* save the object reference list */
    for (int i = 0 ; i < ext->obj_ref_cnt ; ++i, buf += 2)
        oswp2(buf, ext->obj_refs[i]);

    /* return the length */
    return needlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   Stack Frame Descriptor intrinsic object 
 */
ulong CVmObjFrameDesc::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* get my extension */
    vm_framedesc_ext *ext = get_ext();

    /* figure the required size */
    ulong needlen = VMB_OBJECT_ID + 2 + 2;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;

    /* save the underlying StackFrameRef reference */
    vmb_put_objid(buf, ext->fref);
    buf += VMB_OBJECT_ID;

    /* save the frame index and return address */
    oswp2(buf, ext->frame_idx);
    oswp2(buf + 2, ext->ret_ofs);
    buf += 4;

    /* return the length */
    return needlen;
}


/* ------------------------------------------------------------------------ */
/*
 *   Stack Frame Reference intrinsic object 
 */
ulong CVmObjFrameRef::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* get my extension */
    vm_frameref_ext *ext = get_ext();

    /* figure the required size */
    ulong needlen = 2 + 2 + VMB_DATAHOLDER
                    + VMB_DATAHOLDER
                    + VMB_OBJECT_ID
                    + VMB_OBJECT_ID
                    + VMB_PROP_ID
                    + (ext->nlocals + ext->nparams)*VMB_DATAHOLDER;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;

    /* if our frame is still active, build the local snapshot */
    if (ext->fp != 0)
        make_snapshot(vmg0_);

    /* save the variable counts */
    oswp2(buf, ext->nlocals);
    oswp2(buf+2, ext->nparams);
    buf += 4;

    /* write the entry pointer */
    vmb_put_dh(buf, &ext->entry);
    buf += VMB_DATAHOLDER;

    /* write the method context variables */
    vmb_put_dh(buf, &ext->self);
    buf += VMB_DATAHOLDER;

    vmb_put_objid(buf, ext->defobj);
    buf += VMB_OBJECT_ID;

    vmb_put_objid(buf, ext->targobj);
    buf += VMB_OBJECT_ID;

    vmb_put_propid(buf, ext->targprop);
    buf += VMB_PROP_ID;

    /* write the variable snapshot values */
    int i;
    const vm_val_t *v;
    for (i = ext->nlocals + ext->nparams, v = ext->vars ; i > 0 ; --i, ++v)
    {
        vmb_put_dh(buf, v);
        buf += VMB_DATAHOLDER;
    }

    /* return the length */
    return needlen;
}


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjDate intrinsic object 
 */
ulong CVmObjDate::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* get my extension */
    vm_date_ext *ext = get_ext();

    /* figure the required size */
    ulong needlen = 8;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;

    /* write our data */
    oswp4s(buf, ext->dayno);
    oswp4(buf+4, ext->daytime);

    /* return the length */
    return needlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTimeZone intrinsic object 
 */
ulong CVmObjTimeZone::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* get my timezone object and query its default settings */
    CVmTimeZone *tz = get_ext()->tz;
    vmtzquery q;
    tz->query(&q);

    /* get the zone name */
    size_t namelen;
    const char *name = tz->get_name(namelen);

    /* if this is the special local zone object, use ":local" instead */
    if (G_tzcache->is_local_zone(tz))
        name = ":local", namelen = 6;

    /* 
     *   figure the required size - int32 gmt offset, int32 dst offset,
     *   abbreviation (with one-byte prefix), name (with one-byte prefix)
     */
    size_t abbrlen = strlen(q.abbr);
    ulong needlen = 4 + 4 + 1 + namelen + 1 + abbrlen;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;

    /* copy the data */
    oswp4s(buf, q.gmtofs);
    oswp4s(buf+4, q.save);
    char *p = write_str_byte_prefix(buf+8, name, namelen);
    write_str_byte_prefix(p, q.abbr, abbrlen);

    /* return the length */
    return needlen;
}

#ifdef TADSNET
/* ------------------------------------------------------------------------ */
/*
 *   HTTP Server intrinsic object
 */
ulong CVmObjHTTPServer::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* we don't store anything in the image file */
    ulong needlen = 0;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;

    /* return the length */
    return needlen;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPRequest intrinsic object 
 */
ulong CVmObjHTTPRequest::rebuild_image(VMG_ char *buf, ulong buflen)
{
    /* we don't store anything in the image file */
    ulong needlen = 0;

    /* make sure there's room */
    if (needlen > buflen)
        return needlen;

    /* return the length */
    return needlen;
}

#endif /* TADSNET */


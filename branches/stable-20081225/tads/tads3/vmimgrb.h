/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmimgrb.h - image rebuilder
Function
  Re-builds an image file from the loaded program's state.  Useful
  for writing an image file after pre-initialization has bee completed.
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#ifndef VMIMGRB_H
#define VMIMGRB_H

#include "vmtype.h"

/* ------------------------------------------------------------------------ */
/*
 *   Rewrite the image file.  Copies an original image file to the given
 *   file.  
 */
void vm_rewrite_image(VMG_ CVmFile *origfp, CVmFile *newfp,
                      ulong static_cs_start_ofs);


/* ------------------------------------------------------------------------ */
/*
 *   Object-to-Constant Mapper.
 *   
 *   When we're rebuilding an image file, we might want to convert all
 *   string and list objects (i.e., instances of metaclass String or List)
 *   into constant string and list values, respectively, adding these
 *   values to the image file's constant pool rather than storing them as
 *   object instances.  Using constant pool data in an image file
 *   increases a program's efficiency by reducing the number of active
 *   instances required at run-time.
 *   
 *   During the conversion process, we need to store the constant data
 *   that will go in the additional constant pool pages.  We also need to
 *   maintain a translation table that gives us the constant pool address
 *   of each converted string or list value given its object ID.  This
 *   object maintains these tables.  
 */

class CVmConstMapper
{
public:
    CVmConstMapper(VMG0_);
    ~CVmConstMapper();

    /* 
     *   Get an object's constant pool address.  Returns zero if the
     *   object does not have an address mapped yet. 
     */
    ulong get_pool_addr(vm_obj_id_t obj_id);

    /*
     *   Reserve space for an object's data in the new constant pool.
     *   Returns the constant pool address of the reserved space, and adds
     *   a translation mapping for the object to our table, so that future
     *   calls to get_pool_addr(obj_id) will return this address.
     *   
     *   If the object is too large to fit on a constant pool page, we'll
     *   return zero to indicate that the object cannot be stored in the
     *   constant pool; in this case, the object must remain an object
     *   instance rather than a constant item.
     *   
     *   This doesn't actually copy any data, but merely reserves space.  
     */
    ulong alloc_pool_space(vm_obj_id_t obj_id, size_t len);

    /*
     *   Prepare to begin storing data.  This must be called once, after
     *   all pool space has been reserved and before the first object's
     *   data are actually stored. 
     */
    void prepare_to_store_data();

    /*
     *   Store the object's data in its pre-reserved space in the rebuilt
     *   constant pool.  The space must have previously been allocated
     *   with alloc_pool_space(), and the size used must be exactly the
     *   same as the space allocated originally.  
     */
    void store_data(vm_obj_id_t obj_id, const void *ptr, size_t len);

    /* 
     *   get the number of pages we've stored - this is valid only after
     *   prepare_to_store_data() has been called 
     */
    size_t get_page_count() const { return pages_cnt_; }

    /*
     *   Write our pages to an image file.  This routine can be called
     *   only after all of the objects have been stored in the constant
     *   pool. 
     */
    void write_to_image_file(class CVmImageWriter *writer, uchar xor_mask);

protected:
    /*
     *   Determine how many extra pages we need to add to the original
     *   image file's constant pool for our mapped data.  
     */
    size_t calc_page_count() const
    {
        /* 
         *   calculate the total number of pages by dividing the page size
         *   into the total number of bytes we've allocated, rounded up to
         *   the next page whole page 
         */
        return (size_t)((next_free_ + (page_size_ - 1) - base_addr_)
                        / page_size_);
    }

    /* 
     *   index of our first page - this will be the next page after the
     *   last page used in the original image file 
     */
    size_t first_page_idx_;

    /* constant pool page size */
    size_t page_size_;

    /* first address that we allocated */
    ulong base_addr_;

    /* next free address for the allocation pass */
    ulong next_free_;

    /* amount of space remaining on current pool allocation page */
    size_t rem_;

    /* 
     *   Translation page array - each element of this array points to a
     *   page that contains 1024 object ID translations.  
     */
    ulong **obj_addr_;

    /* number of page slots in obj_addr_ array */
    size_t obj_addr_cnt_;

    /* 
     *   constant pool page data - we allocate the pages in
     *   prepare_to_store_data(), which is called after we know how many
     *   pages we'll need 
     */
    struct vm_const_mapper_page **pages_;
    size_t pages_cnt_;
};

/*
 *   mapper constant pool page 
 */
struct vm_const_mapper_page
{
    /* 
     *   high-water mark for data stored in the page, as an offset from
     *   the start of the page's data 
     */
    size_t max_ofs_used;

    /* the page's data */
    char buf[1];
};


#endif /* VMIMGRB_H */

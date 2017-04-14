/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbytarr.h - T3 ByteArray metaclass
Function
  
Notes
  
Modified
  06/05/01 MJRoberts  - Creation
*/

#ifndef VMBYTARR_H
#define VMBYTARR_H

#include <stdlib.h>
#include "os.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"

/* ------------------------------------------------------------------------ */
/*
 *   A ByteArray is simply an array of byte values.  This class provides a
 *   simple, fast mechanism to store blocks of binary data, so it is not a
 *   subclass of Array and is not a Collection.
 *   
 *   The image file data for a byte array is simple:
 *   
 *   UINT4 number of bytes
 *.  BYTE bytes[1..number_of_bytes]
 *   
 *   Internally, we store the array data in chunks of 32k each.  Our
 *   extension is a first-level page table, pointing to the chunks:
 *   
 *   UINT4 number of elements
 *.  unsigned char **page0
 *.  unsigned char **page1
 *.  ...
 *   
 *   Each pageN pointer points to a second-level page table, which consists
 *   of (up to) 8192 pointers to the actual pages.  Since a page is 32k, and
 *   we can store 8k pointers per second-level table, each second-level
 *   table is capable of referencing 256MB.  By design, we can store up to
 *   4GB, so we need at most 16 second-level tables.
 *   
 *   The extension is allocated according to the actual number of
 *   second-level tables we require for the element count.  Each
 *   second-level page is allocated to 8192*sizeof(char *), except the last
 *   second-level page, which is allocated to N*sizeof(char *) where N is
 *   the number of elements required in the last second-level table.  Each
 *   page is allocated to 32K bytes, except the last, which is allocated to
 *   the actual size needed.
 *   
 *   To access an element at index i, we calculate s1 (the page table
 *   selector) as i/(32k*8k) == i/256M; s2 (the page selector within the
 *   selected page table) as (i%256M)/32k; and s3 (the byte selector within
 *   the page) as i%32k.  The byte is then accessed as
 *   
 *   page[s1][s2][s3] 
 */
class CVmObjByteArray: public CVmObject
{
    friend class CVmMetaclassByteArray;
    friend class bytearray_undo_rec;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self)
    {
        /* 
         *   we reference no other objects and cannot ourselves be converted
         *   to constant data, so there's nothing to do here 
         */
    }

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self)
    {
        /* 
         *   we reference no data and cannot be converted to constant data,
         *   so there's nothing to do 
         */
    }

    /* cast to string - assumes the bytes as Latin-1 characters */
    virtual const char *cast_to_string(VMG_ vm_obj_id_t self,
                                       vm_val_t *new_str) const;

    /* create an array with no initial contents */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* create an array with a given number of elements */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              unsigned long element_count);

    /* create an array by mapping a string through a character set */
    static vm_obj_id_t create_from_string(
        VMG_ const vm_val_t *strval, const char *str, const vm_val_t *mapval);

    /* create from binary data */
    static vm_obj_id_t create_from_bytes(
        VMG_ int in_root_set, const char *buf, size_t len);

    /* 
     *   determine if an object is a byte array - it is if the object's
     *   virtual metaclass registration index matches the class's static
     *   index 
     */
    static int is_byte_array(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* undo operations */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *rec);
    void discard_undo(VMG_ struct CVmUndoRecord *);

    /* our data are just bytes - we reference nothing */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }
    void mark_refs(VMG_ uint /*state*/) { }
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* reload from the image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixup);

    /* index the array */
    int index_val_q(VMG_ vm_val_t *result, vm_obj_id_t self,
                    const vm_val_t *index_val);

    /* set an indexed element of the array */
    int set_index_val_q(VMG_ vm_val_t *new_container, vm_obj_id_t self,
                        const vm_val_t *index_val, const vm_val_t *new_val);

    /* 
     *   Check a value for equality.  We will match another byte array with
     *   the same number of elements and the same value for each element.  
     */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;

    /* calculate a hash value for the array */
    uint calc_hash(VMG_ vm_obj_id_t self, int depth) const;

    /* 
     *   assume that we've been changed since loading, if we came from the
     *   image file 
     */
    int is_changed_since_load() const { return TRUE; }

    /* get the number of elements in the array */
    unsigned long get_element_count() const
        { return t3rp4u(get_ext_ptr()); }

    /* get the byte at a given 1-based index */
    unsigned char get_element(unsigned long idx) const
    {
        size_t avail;
        return *get_ele_ptr(idx, &avail);
    }

    /* 
     *   construction: copy (without undo) bytes from a buffer into the byte
     *   array 
     */
    void cons_copy_from_buf(const unsigned char *buf,
                            unsigned long idx, size_t cnt)
    {
        /* copy the bytes into our array */
        copy_from_buf(buf, idx, cnt);
    }

    /* 
     *   Write the specified region of the array to a file.  Returns zero on
     *   success, non-zero on failure. 
     */
    int write_to_file(class CVmDataSource *fp, unsigned long start_idx,
                      unsigned long len) const;

    /* 
     *   Read bytes from a file into a region of the array, replacing
     *   existing bytes in the array; saves undo for the change.  Returns
     *   the number of bytes actually read from the file, which will be less
     *   than the number of bytes requested if we reach the end of the file
     *   before satisfying the request.  
     */
    unsigned long read_from_file(VMG_ vm_obj_id_t self,
                                 class CVmDataSource *fp,
                                 unsigned long start_idx, unsigned long len,
                                 int save_undo);

    /* 
     *   write to a 'data' mode file - returns zero on success, non-zero on
     *   failure 
     */
    int write_to_data_file(class CVmDataSource *fp);

    /* 
     *   read from a 'data' mode file, creating a new ByteArray object to
     *   hold the bytes from the file 
     */
    static int read_from_data_file(VMG_ vm_val_t *retval,
                                   class CVmDataSource *fp);

    /* 
     *   Copy bytes into a buffer, starting at the given 1-based index.
     *   Returns the number of bytes actually copied (this might be less than
     *   the requested size, because the ByteArray might not have enough
     *   contents to fill the request). 
     */
    size_t copy_to_buf(unsigned char *buf, unsigned long idx, size_t cnt)
        const;

    /* 
     *   Copy bytes from a buffer into the array at the given 1-based index,
     *   saving undo.  Returns the actual number of bytes copied, which might
     *   be less than the requested size, since the ByteArray might not be
     *   big enough to accommodate the write. 
     */
    size_t copy_from_buf_undo(VMG_ vm_obj_id_t self, const unsigned char *buf,
                              unsigned long idx, size_t cnt);

protected:
    /* load image data */
    virtual void load_image_data(VMG_ const char *ptr, size_t siz);

    /* create a list with no initial contents */
    CVmObjByteArray() { ext_ = 0; }

    /* 
     *   create a list with a given number of elements, for construction
     *   of the list element-by-element 
     */
    CVmObjByteArray(VMG_ unsigned long byte_count);

    /* get a pointer to my extension */
    const char *get_ext_ptr() const { return ext_; }

    /* 
     *   get my extension data pointer for construction purposes - this is
     *   a writable pointer, so that a caller can fill our data buffer
     *   during construction 
     */
    char *cons_get_ext_ptr() const { return ext_; }

    /* allocate space for the array, given the number of elements */
    void alloc_array(VMG_ unsigned long element_count);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - length */
    int getp_length(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - subarray */
    int getp_subarray(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - copy from another byte array */
    int getp_copy_from(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - fill with a value */
    int getp_fill_val(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - convert to string */
    int getp_to_string(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - read integer */
    int getp_read_int(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - write integer */
    int getp_write_int(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - packBytes */
    int getp_packBytes(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - unpackBytes */
    int getp_unpackBytes(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - static ByteArray.unpackBytes */
    static int static_packBytes(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluator - sha256 */
    int getp_sha256(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - digestMD5 */
    int getp_digestMD5(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* 
     *   Given a 1-based index, get a pointer to the byte at the index, and
     *   the number of contiguous bytes available starting with that byte.
     *   The available byte count doesn't take into account a short last
     *   page, but simply returns the maximum number of bytes that would be
     *   available on the page if it were allocated to full size; the caller
     *   is responsible for ensuring that there is no reading or writing
     *   past the end of the array.  
     */
    unsigned char *get_ele_ptr(unsigned long idx, size_t *bytes_avail) const
    {
        size_t s1;
        size_t s2;
        size_t s3;

        /* convert to a zero-based index */
        --idx;
        
        /* 
         *   calculate the page table index - since each page holds 32k
         *   bytes and each page table points to 8k pages, divide by 32k*8k
         *   == 2^15*2^13 == 2^28 
         */
        s1 = idx >> 28;

        /* 
         *   calculate the page index within the page table - each page
         *   holds 32k, so calculate the excess from the page table selector
         *   (i.e, idx % 32k*8k) and then divide by 32k == 2^15 
         */
        s2 = (idx & 0x0FFFFFFF) >> 15;

        /* 
         *   calculate the page offset - this is simply the excess from the
         *   page index 
         */
        s3 = idx & 0x7FFF;

        /*
         *   Each page holds 32k, so the number of contiguous bytes starting
         *   at this byte is 32k less the index.  
         */
        *bytes_avail = (32*1024) - s3;

        /* 
         *   dereference the extension to get the page table, deference the
         *   page table to get the page, and index the page by the offset 
         */
        return get_page_table_ptr(s1)[s2] + s3;
    }

    /*
     *   Given a page table selector, return a pointer to the selected page
     *   table.  
     */
    unsigned char **get_page_table_ptr(size_t s) const
    {
        return get_page_table_array()[s];
    }

    /*
     *   Get a pointer to the page table array 
     */
    unsigned char ***get_page_table_array() const
    {
        /* the page table array starts after the element count */
        return (unsigned char ***)(ext_ + 4);
    }

    /* fill the given (1-based index) range with the given byte value */
    void fill_with(unsigned char val, unsigned long start_idx,
                   unsigned long cnt);

    /* copy bytes from another byte array into this one */
    void copy_from(unsigned long dst_idx,
                   CVmObjByteArray *src_arr,
                   unsigned long src_start_idx, unsigned long cnt);

    /* move bytes within this array */
    void move_bytes(unsigned long dst_idx, unsigned long src_idx,
                    unsigned long cnt);

    /* copy bytes from a buffer into the array */
    void copy_from_buf(const unsigned char *buf,
                       unsigned long idx, size_t cnt);

    /* map to a string */
    size_t map_to_string(unsigned long idx, unsigned long len,
                         class CVmObjString *str, size_t str_len,
                         class CCharmapToUni *mapper);
    
    /* save undo for a change to a range of the array */
    void save_undo(VMG_ vm_obj_id_t self, unsigned long start_idx,
                   unsigned long cnt);

    /* set the number of bytes in the array */
    void set_element_count(unsigned long cnt)
        { oswp4(cons_get_ext_ptr(), cnt); }

    /* property evaluation function table */
    static int (CVmObjByteArray::*func_table_[])(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassByteArray: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "bytearray/030002"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjByteArray();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjByteArray();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjByteArray::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjByteArray::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMBYTARR_H */


/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjByteArray)

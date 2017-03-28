/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmvec.h - T3 VM Vector class
Function
  A Vector is a subclass of Collection.  A Vector is a List whose
  elements can be changed in-place, and whose size can change.
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#ifndef VMVEC_H
#define VMVEC_H

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmcoll.h"
#include "vmglob.h"
#include "vmstack.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   Vectors are very similar to lists, but a vector's contents can be
 *   modified during execution.  Because vectors are dynamic, a vector's
 *   image file data are copied into the heap.
 *   
 *   A vector's image file data structure looks like this:
 *   
 *   UINT2 elements_allocated 
 *.  UINT2 number_of_elements_used
 *.  DATAHOLDER element[1]
 *.  DATAHOLDER element[2]
 *.  etc, up to number_of_elements_used (NOT the number allocated)
 *   
 *   The object extension for a vector is almost identical, but adds a prefix
 *   giving the "allocated" size of the vector, and a suffix with a pointer
 *   to our original image data (if applicable) plus one bit per element
 *   after the end of the element[] list for undo bits:
 *   
 *   UINT2 allocated_elements
 *.  UINT2 number_of_elements
 *.  DATAHOLDER element[1]
 *.  DATAHOLDER element[2]
 *.  etc
 *.  undo_bits
 *   
 *   The allocated_elements counter gives the total number of elements
 *   allocated in the vector; this can be greater than the number of elements
 *   actually in use, in which case we have free space that we can use to add
 *   elements without reallocating the vector's memory.
 *   
 *   The undo_bits are packed 8 bits per byte.  Note that the undo_bits
 *   follow all allocated slots, so these do not change location when we
 *   change the number of elements actually in use.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   Vector metaclass 
 */
class CVmObjVector: public CVmObjCollection
{
    friend class CVmMetaclassVector;
    friend class CVmQSortVector;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObjCollection::is_of_metaclass(meta));
    }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* create with no initial contents */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* create with a given number of elements */
    static vm_obj_id_t create(VMG_ int in_root_set, size_t element_count);

    /* create with initial contents */
    static vm_obj_id_t create_fill(VMG_ int in_root_set, size_t element_count,
                                   ...);

    /* 
     *   determine if an object is a vector - it is if the object's
     *   virtual metaclass registration index matches the class's static
     *   index 
     */
    static int is_vector_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* vectors are list-like for read access */
    int is_listlike(VMG_ vm_obj_id_t) { return TRUE; }
    int ll_length(VMG_ vm_obj_id_t) { return get_element_count(); }

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* call a static property */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* cast to string */
    const char *cast_to_string(VMG_ vm_obj_id_t self, vm_val_t *new_str) const
    {
        /* join the vector into a string, using commas as separators */
        join(vmg_ new_str, self, ",", 1);

        /* return the new string */
        return new_str->get_as_string(vmg0_);
    }

    virtual const char *explicit_to_string(
        VMG_ vm_obj_id_t self, vm_val_t *new_str, int radix, int flags) const;
    
    /* add a value to the vector, yielding a new vector */
    int add_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* subtract a value from the vector, yielding a new vector */
    int sub_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* undo operations */
    void notify_new_savept();
    void apply_undo(VMG_ struct CVmUndoRecord *rec);
    void mark_undo_ref(VMG_ struct CVmUndoRecord *rec);

    /* we keep only strong references */
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark references */
    void mark_refs(VMG_ uint state);

    /* 
     *   remove weak references - we keep only normal (strong) references,
     *   so this routine doesn't need to do anything 
     */
    void remove_stale_weak_refs(VMG0_) { }

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* reload from the image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* index the vector */
    int index_val_q(VMG_ vm_val_t *result, vm_obj_id_t self,
                    const vm_val_t *index_val);

    /* set an indexed element of the vector */
    int set_index_val_q(VMG_ vm_val_t *new_container, vm_obj_id_t self,
                        const vm_val_t *index_val, const vm_val_t *new_val);

    /* 
     *   Check a value for equality.  We will match any list or vector with
     *   the same number of elements and the same value for each element.  
     */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;

    /* calculate a hash value for the vector */
    uint calc_hash(VMG_ vm_obj_id_t self, int depth) const;

    /* 
     *   assume that we've been changed since loading, if we came from the
     *   image file 
     */
    int is_changed_since_load() const { return TRUE; }

    /* 
     *   get the allocated number of elements of the vector - this will be
     *   greater than or equal to get_element_count(), and reflects the
     *   actual allocated size 
     */
    size_t get_allocated_count() const
        { return vmb_get_len(get_vector_ext_ptr()); }

    /* get the number of elements in the vector */
    size_t get_element_count() const
        { return vmb_get_len(get_vector_ext_ptr() + 2); }

    /* get an element without any range checking; 0-based index */
    void get_element(size_t idx, vm_val_t *val) const
    {
        /* get the data from the data holder in our extension */
        vmb_get_dh(get_element_ptr(idx), val);
    }

    /* append an element, with no undo */
    void append_element(VMG_ vm_obj_id_t self, const vm_val_t *val);

    /* set an element; 0-based index; no bounds checking or undo */
    void set_element(size_t idx, const vm_val_t *val)
    {
        /* set the element's data holder from the value */
        vmb_put_dh(get_element_ptr(idx), val);
    }

    /* set an element, recording undo for the change; 0-based index */
    void set_element_undo(VMG_ vm_obj_id_t self,
                          size_t idx, const vm_val_t *val);

    /* 
     *   join the list into a string - this is the C++ interface to the
     *   self.join() method 
     */
    void join(VMG_ vm_val_t *retval, vm_obj_id_t self,
              const char *sep, size_t sep_len) const;

protected:
    /* load image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* for construction, remove all redundant elements */
    void cons_uniquify(VMG0_);

    /* uniquify the value for an appendUnique operation */
    void uniquify_for_append(VMG_ vm_obj_id_t self);

    /* create an iterator */
    virtual void new_iterator(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val);

    /* create a live iterator */
    virtual void new_live_iterator(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val);

    /* create a copy of the object */
    vm_obj_id_t create_copy(VMG_ const vm_val_t *self_val);

    /* create with no initial contents */
    CVmObjVector() { ext_ = 0; }

    /* create with a given number of elements */
    CVmObjVector(VMG_ size_t element_count);

    /* get a pointer to my vector data */
    const char *get_vector_ext_ptr() const { return ext_; }

    /* 
     *   get my extension data pointer for construction purposes - this is a
     *   writable pointer, so that a caller can fill our data buffer during
     *   construction 
     */
    char *cons_get_vector_ext_ptr() const { return ext_; }

    /*
     *   Calculate the amount of space we need to store a list of a given
     *   length.  We require two bytes for the length prefix, plus the space
     *   for each element, plus the space for the undo bits (one bit per
     *   element, but we pack eight bits per byte).  
     */
    static size_t calc_alloc(size_t elecnt)
    {
        return (calc_alloc_prefix_and_ele(elecnt)
                + ((elecnt + 7) & ~7));
    }

    /* 
     *   calculate the allocation amount, including only the count prefix
     *   and element data 
     */
    static size_t calc_alloc_prefix_and_ele(size_t elecnt)
    {
        return (2*VMB_LEN + calc_alloc_ele(elecnt));
    }

    /* calculate the allocation amount, including only the element storage */
    static size_t calc_alloc_ele(size_t elecnt)
    {
        return elecnt * VMB_DATAHOLDER;
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* allocate space for the vector, given the number of elements */
    void alloc_vector(VMG_ size_t element_count);

    /* expand to accommodate the given number of new elements */
    void expand_by(VMG_ vm_obj_id_t self, size_t added_elements);

    /* get the allocation count increment */
    size_t get_alloc_count_increment() const
    {
        /* 
         *   we might want to make this parameterizable; for now use a
         *   fixed increment size 
         */
        return 16;
    }

    /* push an element onto the stack */
    void push_element(VMG_ size_t idx) const
    {
        /* push a stack element */
        vm_val_t *p = G_stk->push();

        /* 
         *   get the data from the data holder in our extension directly into
         *   our new stack element 
         */
        vmb_get_dh(get_element_ptr(idx), p);
    }

    /* set the allocated size */
    void set_allocated_count(size_t cnt);

    /* given an index, get a pointer to the element's data in the list */
    char *get_element_ptr(size_t idx) const
    {
        /* 
         *   figure out where this element's data holder is by skipping the
         *   count prefix, then skipping past preceding data holders 
         */
        return cons_get_vector_ext_ptr() + 2*VMB_LEN + (idx * VMB_DATAHOLDER);
    }

    /* get a constant pointer to an element */
    static const char *get_element_ptr_const(const char *ext, size_t idx)
    {
        return ext + 2*VMB_LEN + (idx * VMB_DATAHOLDER);
    }

    /* increment an element pointer */
    static void inc_element_ptr(char **ptr) { *ptr += VMB_DATAHOLDER; }

    /* get a given undo bit */
    int get_undo_bit(size_t idx) const
    {
        char *p;
        int bitnum;

        /* get the byte containing the bit for this index */
        p = get_undo_ptr() + (idx >> 3);

        /* get the bit number within the byte */
        bitnum = (idx & 7);

        /* get this bit */
        return ((*p & (1 << bitnum)) != 0);
    }

    /* set a given undo bit */
    void set_undo_bit(size_t idx, int val)
    {
        char *p;
        int bitnum;

        /* get the byte containing the bit for this index */
        p = get_undo_ptr() + (idx >> 3);

        /* get the bit number within the byte */
        bitnum = (idx & 7);

        /* set or clear the bit */
        if (val)
            *p |= (1 << bitnum);
        else
            *p &= ~(1 << bitnum);
    }

    /* clear all undo bits */
    void clear_undo_bits()
    {
        memset(get_undo_ptr(), 0, ((get_allocated_count() + 7) & ~7));
    }

    /* get a pointer to the first byte of the undo bits */
    char *get_undo_ptr() const
    {
        /* the undo bits follow the last element data */
        return (cons_get_vector_ext_ptr()
                + 2*VMB_LEN
                + get_allocated_count()*VMB_DATAHOLDER);
    }

    /* set the number of elements in the vector; no undo or size checking */
    void set_element_count(size_t cnt)
        { vmb_put_len(cons_get_vector_ext_ptr() + 2, cnt); }

    /* set the element count, saving undo for the change */
    void set_element_count_undo(VMG_ vm_obj_id_t self,
                                size_t new_element_count);

    /* insert elements into the vector, keeping undo */
    void insert_elements_undo(VMG_ vm_obj_id_t self,
                              int ins_idx, int add_cnt);

    /* delete elements from the vector, keeping undo */
    void remove_elements_undo(VMG_ vm_obj_id_t self,
                              size_t start_idx, size_t del_cnt);

    /* general handler for indexOf, lastIndexOf, and countOf */
    int gen_index_of(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc,
                     int forward, int count_only);

    /* general handler for indexWhich, lastIndexWhich, and countWhich */
    int gen_index_which(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc,
                        int forward, int count_only, vm_rcdesc *rc);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - convert to a list */
    int getp_to_list(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - get the number of elements */
    int getp_get_size(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - copy from another vector or list */
    int getp_copy_from(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - fill with a value */
    int getp_fill_val(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - select a subset */
    int getp_subset(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - apply a callback to each element */
    int getp_apply_all(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - find the first element satisfying a condition */
    int getp_index_which(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - call a callback on each element */
    int getp_for_each(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - call a callback(key,val) on each element */
    int getp_for_each_assoc(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* general processor for forEach/forEachAssoc */
    int for_each_gen(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc,
                     int pass_key_to_cb, vm_rcdesc *rc);

    /* property evaluator - mapAll */
    int getp_map_all(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - indexOf */
    int getp_index_of(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - valWhich */
    int getp_val_which(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - lastIndexOf */
    int getp_last_index_of(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - lastIndexWhich */
    int getp_last_index_which(VMG_ vm_obj_id_t self,
                              vm_val_t *val, uint *argc);

    /* property evaluator - valWhich */
    int getp_last_val_which(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - countOf */
    int getp_count_of(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - countWhich */
    int getp_count_which(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - getUnique */
    int getp_get_unique(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - appendUnique */
    int getp_append_unique(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - sort */
    int getp_sort(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - set the length */
    int getp_set_length(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - insertAt */
    int getp_insert_at(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - remove a single element at a given index */
    int getp_remove_element_at(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc);

    /* property evaluator - removeRange */
    int getp_remove_range(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - append a value */
    int getp_append(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - prepend a value */
    int getp_prepend(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - append everything from a collection */
    int getp_append_all(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - remove any elements with a given value */
    int getp_remove_element(VMG_ vm_obj_id_t self, vm_val_t *retval,
                            uint *argc);

    /* property evaluator - join into a string */
    int getp_join(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - generate */
    int getp_generate(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc)
        { return static_getp_generate(vmg_ retval, argc); }

    /* static property evaluator - generate */
    static int static_getp_generate(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluator - splice */
    int getp_splice(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluator - get the index of the minimum element */
    int getp_indexOfMin(VMG_ vm_obj_id_t self, vm_val_t *retval,
                        uint *argc);

    /* property evaluator - get the minimum value in the list */
    int getp_minVal(VMG_ vm_obj_id_t self, vm_val_t *retval,
                    uint *argc);

    /* property evaluator - get the index of the maximum element */
    int getp_indexOfMax(VMG_ vm_obj_id_t self, vm_val_t *retval,
                        uint *argc);

    /* property evaluator - get the maximum value in the list */
    int getp_maxVal(VMG_ vm_obj_id_t self, vm_val_t *retval,
                    uint *argc);

    /* general handler for the various min/max methods */
    int get_minmax(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc,
                   const vm_rcdesc *rc, int sense, int want_index);

    /* property evaluation function table */
    static int (CVmObjVector::*func_table_[])(VMG_ vm_obj_id_t self,
                                              vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassVector: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "vector/030005"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjVector();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjVector();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjVector::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjVector::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* I'm a Collection object */
    CVmMetaclass *get_supermeta_reg() const
        { return CVmObjCollection::metaclass_reg_; }
};

#endif /* VMVEC_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjVector)

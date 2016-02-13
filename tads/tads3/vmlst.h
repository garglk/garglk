/* $Header: d:/cvsroot/tads/tads3/VMLST.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmlst.h - VM dynamic list implementation
Function
  
Notes
  
Modified
  10/29/98 MJRoberts  - Creation
*/

#ifndef VMLST_H
#define VMLST_H

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmcoll.h"
#include "vmglob.h"
#include "vmstack.h"
#include "vmrun.h"


class CVmObjList: public CVmObjCollection
{
    friend class CVmMetaclassList;

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

    /* 
     *   Create dynamically from parameters in the stack; we do not remove
     *   the elements from the stack, but simply create a list from the
     *   parameters.  'idx' is the parameter index of the first parameter,
     *   and 'cnt' is the number of parameters to use.  
     */
    static vm_obj_id_t create_from_params(VMG_ uint idx, uint cnt);

    /* call a static property */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* get my datatype when converted to constant data */
    virtual vm_datatype_t get_convert_to_const_data_type() const
      { return VM_LIST; }

    /* create a list with no initial contents */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* 
     *   create a list with a given number of elements, for construction
     *   of the list element-by-element 
     */
    static vm_obj_id_t create(VMG_ int in_root_set, size_t element_count);

    /* 
     *   create a list from a constant list, for construction of the list
     *   as a modified copy of an original list 
     */
    static vm_obj_id_t create(VMG_ int in_root_set, const char *lst);

    /* get an element, given a zero-based index */
    void get_element(size_t idx, vm_val_t *val) const
    {
        /* get the data from the data holder in our extension */
        vmb_get_dh(get_element_ptr(idx), val);
    }

    /*
     *   List construction: set an element.  List contents are immutable, so
     *   they cannot be changed after the list is constructed.  However, it
     *   is often convenient to construct a list one element at a time, so a
     *   caller can create the list with the appropriate number of elements,
     *   then use this routine to set each element of the list individually.
     *   
     *   idx is the index of the element in the list; the first element is at
     *   index zero.  Note that this routine does *not* allocate memory; the
     *   list must be pre-allocated to its full number of elements.  Use
     *   cons_ensure_space to expand the list as needed.  
     */
    void cons_set_element(size_t idx, const vm_val_t *val);

    /* update the list in place so that each value is unique */
    void cons_uniquify(VMG0_);

    /* 
     *   Copy an existing list into our list, starting at a given index.  The
     *   caller must ensure that our list buffer is large enough to
     *   accommodate the new elements.  The 'orig_list' value must point to a
     *   standard list constant value: a UINT2 element count prefix followed
     *   by DATAHOLDER elements.  
     */
    void cons_copy_elements(size_t start_idx, const char *orig_list);

    /*
     *   Copy existing list elements into our list, starting at the given
     *   index.  The caller must ensure that our list buffer is large enough
     *   to accommodate the new elements.  The 'ele_array' is an array of
     *   DATAHOLDER values.  
     */
    void cons_copy_data(size_t start_idx, const char *ele_array,
                        size_t ele_count);

    /*
     *   Set the length of the list.  This can be used when constructing a
     *   list, and the actual number of elements is unknown before
     *   construction is complete (however, the maximum number of elements
     *   must be known in advance, since this merely sets the length, and
     *   does NOT reallocate the list -- hence, this call can only be used
     *   to shrink the list below its allocated size, never to expand it).
     */
    void cons_set_len(size_t len)
        { vmb_put_len(ext_, len); }

    /*
     *   During construction, ensure there's enough space in the list to hold
     *   an added item at the given index (0-based).  If not, expands the
     *   list to make it large enough to hold the given new elements plus the
     *   margin.  The margin is to ensure that we don't keep reallocating one
     *   element at a time when the caller is adding items iteratively.  The
     *   added elements are automatically set to nil.  
     */
    void cons_ensure_space(VMG_ size_t idx, size_t margin);

    /* 
     *   During construction, clear a range of the list by setting each
     *   element in the range (inclusive of the endpoints) to nil.  If the
     *   construction process could trigger garbage collection, it's
     *   important for the caller to clear the list first.  Uninitialized
     *   elements could happen to look like pointers to invalid objects, so
     *   if the gc runs, it could try to follow one of these invalid pointers
     *   and cause a crash.  The index values are 0-based.  
     */
    void cons_clear(size_t start_idx, size_t end_idx);

    /* clear the entire list (set the whole list to nil) */
    void cons_clear()
    {
        if (get_ele_count() != 0)
            cons_clear(0, get_ele_count() - 1);
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* undo operations - lists are immutable and hence keep no undo */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }    

    /* mark references */
    void mark_refs(VMG_ uint state);

    /* 
     *   remove weak references - we keep only normal (strong) references,
     *   so this routine doesn't need to do anything 
     */
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t)
        { ext_ = (char *)ptr; }

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* 
     *   cast to string - returns a string consisting of the list elements
     *   converted to strings and concatenated together with commas
     *   separating elements; the result is the same as self.join(',')
     */
    const char *cast_to_string(VMG_ vm_obj_id_t self,
                               vm_val_t *new_str) const
    {
        /* set up a 'self' value */
        vm_val_t self_val;
        self_val.set_obj(self);

        /* join the list elements into a string, using commas as separators */
        join(vmg_ new_str, &self_val, ",", 1);

        /* return the string result */
        return new_str->get_as_string(vmg0_);
    }

    /* explicitly convert to string using toString() semantics */
    virtual const char *explicit_to_string(
        VMG_ vm_obj_id_t self, vm_val_t *new_str, int radix, int flags) const
    {
        /* set up a 'self' value */
        vm_val_t self_val;
        self_val.set_obj(self);

        /* convert to string */
        return list_to_string(vmg_ new_str, &self_val, radix, flags);
    }

    /* 
     *   explicitly convert a list or list-like value to a string using
     *   toString() semantics 
     */
    static const char *list_to_string(
        VMG_ vm_val_t *result, const vm_val_t *self, int radix, int flags);
    

    /* 
     *   Add a value to the list.  If the value to add is a list (constant
     *   or object), we'll append each element of the list to this list;
     *   otherwise, we'll just append the value itself to the list.  In
     *   any case, we don't modify this list itself, but create a new list
     *   object to hold the result.
     */
    int add_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /*
     *   Index the list 
     */
    int index_val_q(VMG_ vm_val_t *result, vm_obj_id_t self,
                    const vm_val_t *index_val);

    /*
     *   Set an indexed element of the list.  Since the contents of a list
     *   object cannot be changed, we'll return in *new_container a new
     *   list object that we create with the modified contents.  
     */
    int set_index_val_q(VMG_ vm_val_t *new_container, vm_obj_id_t self,
                        const vm_val_t *index_val, const vm_val_t *new_val);

    /* 
     *   Subtract a value from the list.  This creates a new list with the
     *   element matching the given value removed from the original list.
     *   We do not modify the original list; instead, we create a new list
     *   object with the new value.  
     */
    int sub_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* 
     *   get as a list - simply return our extension, which is in the
     *   required portable list format 
     */
    const char *get_as_list() const { return ext_; }

    /* I'm the original list-like object */
    int is_listlike(VMG_ vm_obj_id_t /*self*/) { return TRUE; }
    int ll_length(VMG_ vm_obj_id_t /*self*/) { return get_ele_count(); }
    

    /* 
     *   Check a value for equality.  We will match any constant list that
     *   contains the same data as our list, and any other list object
     *   with the same underlying data.  
     */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;

    /*
     *   Static list adder.  This creates a new list object that results
     *   from appending the given value to the given list constant.  This
     *   is defined statically so that this code can be shared for adding
     *   to constant pool lists and adding to CVmObjList objects.
     *   
     *   'lstval' must point to a constant list.  The first two bytes of
     *   the list are stored in portable UINT2 format and give the number
     *   of elements in the list; this is immediately followed by a packed
     *   array of data holders in portable format.  
     */
    static void add_to_list(VMG_ vm_val_t *result,
                            vm_obj_id_t self, const char *lstval,
                            const vm_val_t *val);

    /*
     *   Static list subtraction routine. This creates a new list object
     *   that results from removing the given value from the list
     *   constant.  This is defined statically so that this code can be
     *   shared for subtracting from constant pool lists and subtracting
     *   from CVmObjList objects.
     *   
     *   'lstmem' must point to a constant list in the same format as
     *   required for add_to_list.  If 'lstmem' comes from the constant
     *   pool, then 'lstval' must be provided to give us the constant pool
     *   address; otherwise, 'lstval' should be null.  
     */
    static void sub_from_list(VMG_ vm_val_t *result,
                              const vm_val_t *lstval, const char *lstmem,
                              const vm_val_t *val);

    /*
     *   Constant list comparison routine.  Compares the given list
     *   constant (in portable format, with leading UINT2 element count
     *   prefix followed by the list's elements in portable data holder
     *   format) to the other value.  Returns true if the other value is a
     *   list constant or object whose contents match the list constant,
     *   false if not.
     *   
     *   If 'lstmem' comes from the constant pool, then 'lstval' must be
     *   provided to give us the constant pool address; otherwise,
     *   'lstval' should be null.  
     */
    static int const_equals(VMG_ const vm_val_t *lstval, const char *lstmem,
                            const vm_val_t *val, int depth);

    /*
     *   Calculate a hash value for the list 
     */
    uint calc_hash(VMG_ vm_obj_id_t self, int depth) const;

    /*
     *   Constant list hash value calculation
     */
    static uint const_calc_hash(VMG_ const vm_val_t *self_val,
                                const char *lst, int depth);

    /*
     *   Constant list indexing routine.  Indexes the given constant list
     *   (which must be in portable format, with leading UINT2 element
     *   count followed by the list's elements in portable data holder
     *   format), looking up the value at the index number given by the
     *   index value, and puts the result in *result. 
     */
    static void index_list(VMG_ vm_val_t *result,
                           const char *lst, const vm_val_t *index_val);

    /* index a list, using a 1-based index */
    static void index_list(VMG_ vm_val_t *result, const char *lst, uint idx);

    /* push the indexed element, using a 1-based index */
    static void index_and_push(VMG_ const char *lst, uint idx)
    {
        vm_val_t *p;

        /* push a new stack element */
        p = G_stk->push();

        /* index the list and store the value directly in the stack */
        index_list(vmg_ p, lst, idx);
    }

    /*
     *   Constant list set-index routine.  Creates a new list object as a
     *   copy of this list, with the element at the given index set to the
     *   given new value. 
     */
    static void set_index_list(VMG_ vm_val_t *result,
                               const char *lst, const vm_val_t *index_val,
                               const vm_val_t *new_val);

    /*
     *   Find a value within a list.  If we find the value, we'll set *idxp
     *   to the index (starting at 1 for the first element) of the item we
     *   found, and we'll return true; if we don't find the value, we'll
     *   return false.  
     */
    static int find_in_list(VMG_ const vm_val_t *lst,
                            const vm_val_t *val, size_t *idxp);

    /* find the last match for a value */
    static int find_last_in_list(VMG_ const vm_val_t *lst,
                                 const vm_val_t *val, size_t *idxp);

    /*
     *   Evaluate a property of a constant list value.  Returns true if we
     *   successfully evaluated the property, false if the property is not
     *   one of the properties that the list class defines.  
     */
    static int const_get_prop(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                              const char *lst, vm_prop_id_t prop, 
                              vm_obj_id_t *srcobj, uint *argc);

    /* property evaluator - undefined property */
    static int getp_undef(VMG_ vm_val_t *, const vm_val_t *,
                          const char *, uint *)
        { return FALSE; }

    /* property evaluator - select a subset through a callback */
    static int getp_subset(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                           const char *lst, uint *argc);

    /* property evaluator - apply a callback to each element */
    static int getp_map(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                        const char *lst, uint *argc);

    /* get the length */
    static int getp_len(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                        const char *lst, uint *argc);

    /* sublist */
    static int getp_sublist(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                            const char *lst, uint *argc);

    /* intersect */
    static int getp_intersect(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *lst, uint *argc);

    /* indexOf */
    static int getp_index_of(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *lst, uint *argc);

    /* car */
    static int getp_car(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                        const char *lst, uint *argc);

    /* cdr */
    static int getp_cdr(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                        const char *lst, uint *argc);

    /* indexWhich */
    static int getp_index_which(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *argc);

    /* forEach */
    static int getp_for_each(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *lst, uint *argc);

    /* forEachAssoc */
    static int getp_for_each_assoc(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *lst, uint *argc);

    /* valWhich */
    static int getp_val_which(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *lst, uint *argc);

    /* lastIndexOf */
    static int getp_last_index_of(VMG_ vm_val_t *retval,
                                  const vm_val_t *self_val,
                                  const char *lst, uint *argc);

    /* lastIndexWhich */
    static int getp_last_index_which(VMG_ vm_val_t *retval,
                                     const vm_val_t *self_val,
                                     const char *lst, uint *argc);

    /* lastValWhich */
    static int getp_last_val_which(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *lst, uint *argc);

    /* countOf */
    static int getp_count_of(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val,
                             const char *lst, uint *argc);

    /* countWhich */
    static int getp_count_which(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *argc);

    /* general routine for indexWhich and lastIndexWhich */
    static int gen_index_which(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *argc,
                               int forward, vm_rcdesc *rc);

    /* getUnique */
    static int getp_get_unique(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *argc);

    /* appendUnique */
    static int getp_append_unique(VMG_ vm_val_t *retval,
                                  const vm_val_t *self_val,
                                  const char *lst, uint *argc);

    /* appendUnique - internal interface */
    static void append_unique(VMG_ vm_val_t *retval,
                              const vm_val_t *self, const vm_val_t *other);

    /* append */
    static int getp_append(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *lst, uint *argc);

    /* sort */
    static int getp_sort(VMG_ vm_val_t *retval,
                         const vm_val_t *self_val,
                         const char *lst, uint *argc);

    /* insertAt */
    static int getp_insert_at(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *lst, uint *argc);

    /* prepend */
    static int getp_prepend(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val,
                            const char *lst, uint *argc);

    /* property evaluator - remove a single element at a given index */
    static int getp_remove_element_at(VMG_ vm_val_t *retval,
                                      const vm_val_t *self_val,
                                      const char *lst, uint *argc);

    /* property evaluator - removeRange */
    static int getp_remove_range(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *lst, uint *argc);

    /* property evaluator - splice */
    static int getp_splice(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *lst, uint *argc);

    /* property evaluator - join */
    static int getp_join(VMG_ vm_val_t *retval,
                         const vm_val_t *self_val,
                         const char *lst, uint *argc);

    /* 
     *   join the elements of a list into a string; this is equivalent to the
     *   bytecode-level join() method, but can be called more conveniently
     *   from C++ code 
     */
    static void join(VMG_ vm_val_t *retval,
                     const vm_val_t *self_val,
                     const char *sep, size_t sep_len);

    /* property evaluator - generate */
    static int getp_generate(VMG_ vm_val_t *retval,
                             const vm_val_t * /*self*/,
                             const char * /*lst*/, uint *argc)
        { return static_getp_generate(vmg_ retval, argc); }

    /* static property evaluator - generate */
    static int static_getp_generate(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluator - index of minimum value */
    static int getp_indexOfMin(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *argc);

    /* property evaluator - minimum value in list */
    static int getp_minVal(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *lst, uint *argc);

    /* property evaluator - index of maximum value in list */
    static int getp_indexOfMax(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *argc);

    /* property evaluator - maximum value in list */
    static int getp_maxVal(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *lst, uint *argc);
    

protected:
    /* general processor for forEach and forEachAssoc */
    static int for_each_gen(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val,
                            const char *lst, uint *argc,
                            int send_idx_to_cb, vm_rcdesc *rc);

    /* general min/max handler, for minIndex, minVal, maxIndex, maxVal */
    static int get_minmax(VMG_ vm_val_t *retval,
                          const vm_val_t *self_val,
                          const char *lst, uint *in_argc,
                          const vm_rcdesc *rc,
                          int sense, int want_index);

    /*
     *   Compute the intersection of two lists.  Returns a new list with the
     *   elements that occur in both lists.  
     */
    static vm_obj_id_t intersect(VMG_ const vm_val_t *l1,
                                 const vm_val_t *l2);

    /* insert the arguments into the list at the given index */
    static void insert_elements(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint argc, int idx);

    /* remove elements */
    static void remove_range(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val,
                             const char *lst, int start_idx, int del_cnt);

    /* create an iterator */
    virtual void new_iterator(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val);

    /* 
     *   create a live iterator - for a list, there is no difference
     *   between snapshot and live iterators, since a list is immutable 
     */
    virtual void new_live_iterator(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val)
        { new_iterator(vmg_ retval, self_val); }

    /* get the number of elements in the list */
    size_t get_ele_count() const { return vmb_get_len(ext_); }

    /* get the number of elements in a constant list */
    static size_t get_ele_count_const(const char *lstval)
        { return vmb_get_len(lstval); }

    /* get an element from a constant list, given a zero-based index */
    static void get_element_const(const char *lstval, size_t idx,
                                  vm_val_t *val)
    {
        /* get the data from the data holder in the constant list */
        vmb_get_dh(get_element_ptr_const(lstval, idx), val);
    }

    /* given an index, get a pointer to the element's data in the list */
    char *get_element_ptr(size_t idx) const
    {
        /* 
         *   figure out where this element's data holder is by skipping
         *   the count prefix, then skipping past preceding data holders 
         */
        return ext_ + VMB_LEN + (idx * VMB_DATAHOLDER);
    }

    /* 
     *   given an index, and a pointer to a constant list, get a pointer
     *   to the element's data in the list constant 
     */
    static const char *get_element_ptr_const(const char *lstval, size_t idx)
    {
        /* 
         *   figure out where this element's data holder is by skipping
         *   the count prefix, then skipping past preceding data holders 
         */
        return lstval + VMB_LEN + (idx * VMB_DATAHOLDER);
    }

    /* 
     *   given a pointer to a list element, increment the pointer so that
     *   it points to the next element 
     */
    static void inc_element_ptr(char **p)
    {
        /* add the size of a data holder to the current pointer */
        *p += VMB_DATAHOLDER;
    }

    /* increment a constant element pointer */
    static void inc_const_element_ptr(const char **p)
    {
        /* add the size of a data holder to the current pointer */
        *p += VMB_DATAHOLDER;
    }

    /* create a list with no initial contents */
    CVmObjList() { ext_ = 0; }
    
    /* 
     *   create a list with a given number of elements, for construction
     *   of the list element-by-element 
     */
    CVmObjList(VMG_ size_t element_count);
    
    /* create a list from a constant list */
    CVmObjList(VMG_ const char *lst);

    /*
     *   Calculate the amount of space we need to store a list of a given
     *   length.  We require two bytes for the length prefix, plus the
     *   space for each element.
     */
    static size_t calc_alloc(size_t elecnt)
        { return (VMB_LEN + (elecnt * VMB_DATAHOLDER)); }

    /* allocate space for the list, given the number of elements */
    void alloc_list(VMG_ size_t element_count);

    /* property evaluation function table */
    static int (*func_table_[])(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   A constant list is exactly like an ordinary list, except that our
 *   contents come from the constant pool.  We store a pointer directly to
 *   our constant pool data rather than making a separate copy.  The only
 *   thing we have to do differently from an ordinary list is that we don't
 *   delete our extension when we're deleted, since our extension is really
 *   just a pointer into the constant pool.  
 */
class CVmObjListConst: public CVmObjList
{
public:
    /* notify of deletion */
    void notify_delete(VMG_ int /*in_root_set*/)
    {
        /* 
         *   do nothing, since our extension is just a pointer into the
         *   constant pool 
         */
    }

    /* create from constant pool data */
    static vm_obj_id_t create(VMG_ const char *const_ptr);

protected:
    /* construct from constant pool data */
    CVmObjListConst(VMG_ const char *const_ptr)
    {
        /* point our extension directly to the constant pool data */
        ext_ = (char *)const_ptr;
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassList: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "list/030008"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjList();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjList();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjList::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjList::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* I'm a Collection object */
    CVmMetaclass *get_supermeta_reg() const
        { return CVmObjCollection::metaclass_reg_; }
};

#endif /* VMLST_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjList)


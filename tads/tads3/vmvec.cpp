#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmvec.cpp - T3 VM Vector metaclass
Function
  
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#include <stdlib.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmmcreg.h"
#include "vmmeta.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmvec.h"
#include "vmlst.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmfile.h"
#include "vmstack.h"
#include "vmbif.h"
#include "vmundo.h"
#include "vmrun.h"
#include "vmiter.h"
#include "vmsort.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassVector metaclass_reg_obj;
CVmMetaclass *CVmObjVector::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjVector::
     *CVmObjVector::func_table_[])(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *argc) =
{
    &CVmObjVector::getp_undef,
    &CVmObjVector::getp_to_list,
    &CVmObjVector::getp_get_size,
    &CVmObjVector::getp_copy_from,
    &CVmObjVector::getp_fill_val,
    &CVmObjVector::getp_subset,
    &CVmObjVector::getp_apply_all,
    &CVmObjVector::getp_index_which,
    &CVmObjVector::getp_for_each,
    &CVmObjVector::getp_for_each_assoc,
    &CVmObjVector::getp_map_all,
    &CVmObjVector::getp_index_of,
    &CVmObjVector::getp_val_which,
    &CVmObjVector::getp_last_index_of,
    &CVmObjVector::getp_last_index_which,
    &CVmObjVector::getp_last_val_which,
    &CVmObjVector::getp_count_of,
    &CVmObjVector::getp_count_which,
    &CVmObjVector::getp_get_unique,
    &CVmObjVector::getp_append_unique,
    &CVmObjVector::getp_sort,
    &CVmObjVector::getp_set_length,
    &CVmObjVector::getp_insert_at,
    &CVmObjVector::getp_remove_element_at,
    &CVmObjVector::getp_remove_range,
    &CVmObjVector::getp_append,
    &CVmObjVector::getp_prepend,
    &CVmObjVector::getp_append_all,
    &CVmObjVector::getp_remove_element
};

/* ------------------------------------------------------------------------ */
/* 
 *   create with a given number of elements 
 */
CVmObjVector::CVmObjVector(VMG_ size_t element_count)
{
    /* allocate space */
    alloc_vector(vmg_ element_count);

    /* 
     *   a vector intially has no elements in use; the element_count
     *   merely gives the number of slots to be allocated initially (this
     *   won't affect the allocation count, which is stored separately) 
     */
    set_element_count(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   allocate space 
 */
void CVmObjVector::alloc_vector(VMG_ size_t cnt)
{
    /* allocate space for the given number of elements */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(calc_alloc(cnt), this);

    /* set the element count and allocated count */
    set_allocated_count(cnt);
    set_element_count(cnt);

    /* clear the undo bits */
    clear_undo_bits();
}

/* ------------------------------------------------------------------------ */
/*
 *   notify of deletion 
 */
void CVmObjVector::notify_delete(VMG_ int)
{
    /* free our extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/* 
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjVector::create_from_stack(VMG_ const uchar **pc_ptr,
                                            uint argc)
{
    vm_val_t *arg1;
    vm_obj_id_t id;
    CVmObjVector *vec;
    size_t cnt = 0;
    size_t copy_cnt;
    const char *src_lst;
    CVmObjVector *src_vec;
    size_t i;
    
    /* check arguments */
    if (argc < 1 || argc > 2)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* get the first argument */
    arg1 = G_stk->get(0);

    /* presume we will have no source argument */
    copy_cnt = 0;
    src_lst = 0;
    src_vec = 0;

    /* check what kind of argument we have */
    if (arg1->typ == VM_INT)
    {
        /* get the number of elements to allocate */
        cnt = (size_t)arg1->val.intval;
    }
    else
    {
        /* anything else is invalid */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* if there's a second argument, it's a source list/vector */
    if (argc >= 2)
    {
        vm_val_t *arg2;

        /* get the second argument */
        arg2 = G_stk->get(1);

        /* see what we have */
        if ((src_lst = arg2->get_as_list(vmg0_)) != 0)
        {
            /* it's a list - copy its elements */
            copy_cnt = vmb_get_len(src_lst);
        }
        else if (arg2->typ == VM_OBJ
                 && (vm_objp(vmg_ arg2->val.obj)
                     ->is_of_metaclass(metaclass_reg_)))
        {
            /* it's a vector */
            src_vec = (CVmObjVector *)vm_objp(vmg_ arg2->val.obj);
            copy_cnt = src_vec->get_element_count();
        }
        else if (arg2->typ == VM_INT)
        {
            /* they want an explicit initial size */
            copy_cnt = (size_t)arg2->val.intval;
        }
        else
        {
            /* invalid source type */
            err_throw(VMERR_BAD_TYPE_BIF);
        }

        /* 
         *   make sure we allocate enough initial space to store the source
         *   object we're copying 
         */
        if (copy_cnt > cnt)
            cnt = copy_cnt;
    }

    /* create the vector with the given number of elements */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);
    vec = new (vmg_ id) CVmObjVector(vmg_ cnt);

    /* copy or initialize the elements */
    for (i = 0 ; i < copy_cnt ; ++i)
    {
        vm_val_t ele;

        /* 
         *   get my element at this index: if we have a source list or
         *   vector, take the current element from that list or vector;
         *   otherwise, initialize the element to nil 
         */
        if (src_lst != 0)
            CVmObjList::index_list(vmg_ &ele, src_lst, i + 1);
        else if (src_vec != 0)
            src_vec->get_element(i, &ele);
        else
            ele.set_nil();

        /* 
         *   set my element at this index - this is construction, so there's
         *   no need to save undo information 
         */
        vec->set_element(i, &ele);
    }

    /* set the initial size */
    vec->set_element_count(copy_cnt);

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new object */
    return id;
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjVector::save_to_file(VMG_ class CVmFile *fp)
{
    size_t ele_cnt;
    size_t alloc_cnt;

    /* get our element count and full allocation count */
    alloc_cnt = get_allocated_count();
    ele_cnt = get_element_count();

    /* write the counts and the elements */
    fp->write_int2(alloc_cnt);
    fp->write_int2(ele_cnt);
    fp->write_bytes(get_element_ptr(0), calc_alloc_ele(ele_cnt));
}

/* 
 *   restore from a file 
 */
void CVmObjVector::restore_from_file(VMG_ vm_obj_id_t self,
                                     CVmFile *fp, CVmObjFixup *fixups)
{
    size_t ele_cnt;
    size_t alloc_cnt;

    /* read the element count and the full allocation count */
    alloc_cnt = fp->read_uint2();
    ele_cnt = fp->read_uint2();

    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* allocate the space at the allocation count */
    alloc_vector(vmg_ alloc_cnt);

    /* set the actual in-use element count */
    set_element_count(ele_cnt);

    /* read the contents */
    fp->read_bytes(get_element_ptr(0), calc_alloc_ele(ele_cnt));

    /* fix up object references in the values */
    fixups->fix_dh_array(vmg_ get_element_ptr(0), ele_cnt);
}

/* ------------------------------------------------------------------------ */
/* 
 *   create with no initial contents 
 */
vm_obj_id_t CVmObjVector::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjVector();
    return id;
}

/* 
 *   create with a given number of elements 
 */
vm_obj_id_t CVmObjVector::create(VMG_ int in_root_set, size_t element_count)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjVector(vmg_ element_count);
    return id;
}


/* ------------------------------------------------------------------------ */
/*
 *   Create a copy of this object 
 */
vm_obj_id_t CVmObjVector::create_copy(VMG_ const vm_val_t *self_val)
{
    vm_obj_id_t new_id;
    CVmObjVector *new_vec;
    
    /* save the original object on the stack for gc protection */
    G_stk->push(self_val);

    /* create a new vector with the same parameters as this one */
    new_id = vm_new_id(vmg_ FALSE, TRUE, FALSE);
    new_vec = new (vmg_ new_id) CVmObjVector(vmg_ get_allocated_count());
    new_vec->set_element_count(get_element_count());

    /* copy the elements from the old vector to the new vector */
    memcpy(new_vec->get_element_ptr(0), get_element_ptr(0),
           calc_alloc_ele(get_element_count()));

    /* done with the gc protection - discard it */
    G_stk->discard(1);

    /* return the new vector's object ID */
    return new_id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the allocation size - this is the number of elements we've allocated
 *   space for, which might exceed the number of elements we're actually
 *   storing currently.  
 */
void CVmObjVector::set_allocated_count(size_t cnt)
{
    /* if the new size exceeds the maximum allowed size, throw an error */
    if (cnt > 65535)
        err_throw(VMERR_OUT_OF_RANGE);

    /* set the new allocated size */
    vmb_put_len(cons_get_vector_ext_ptr(), cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create an iterator 
 */
void CVmObjVector::new_iterator(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val)
{
    vm_val_t copy_val;

    /* 
     *   Create a copy of the vector - since a vector can change, we must
     *   always create an iterator based on a copy, so that the iterator has
     *   a consistent view of the original as of the time the iterator was
     *   created.  
     */
    copy_val.set_obj(create_copy(vmg_ self_val));

    /* push the copy for gc protection */
    G_stk->push(&copy_val);

    /* 
     *   Set up a new indexed iterator object.  The first valid index for a
     *   vector is always 1, and the last valid index is the same as the
     *   number of elements.  
     */
    retval->set_obj(CVmObjIterIdx::create_for_coll(vmg_ &copy_val,
        1, get_element_count()));

    /* discard gc protection */
    G_stk->discard();
}

/*
 *   Create a live iterator 
 */
void CVmObjVector::new_live_iterator(VMG_ vm_val_t *retval,
                                     const vm_val_t *self_val)
{
    /* 
     *   Set up a new indexed iterator object.  The first valid index for a
     *   vector is always 1, and the last valid index is the same as the
     *   number of elements.  Since we want a "live" iterator, create the
     *   iterator with a direct reference to the vector.  
     */
    retval->set_obj(CVmObjIterIdx::create_for_coll(vmg_ self_val,
        1, get_element_count()));
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjVector::set_prop(VMG_ class CVmUndo *,
                            vm_obj_id_t, vm_prop_id_t,
                            const vm_val_t *)
{
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjVector::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
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

    /* inherit our base class handling */
    return CVmObjCollection::
        get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Expand the vector to accommodate the given number of new elements.
 *   We'll increase the element count, and we'll save undo for the change.
 *   If necessary, we'll re-allocate our memory extension block at a
 *   larger size. 
 */
void CVmObjVector::expand_by(VMG_ vm_obj_id_t self, size_t added_elements)
{
    size_t new_ele_cnt;

    /* calculate the element count */
    new_ele_cnt = get_element_count() + added_elements;
    
    /* remember the new size, saving undo */
    set_element_count_undo(vmg_ self, new_ele_cnt);
}


/* ------------------------------------------------------------------------ */
/*
 *   Set the element count, keeping undo for the change 
 */
void CVmObjVector::set_element_count_undo(VMG_ vm_obj_id_t self,
                                          size_t new_ele_cnt)
{
    /* add undo if necessary */
    if (G_undo != 0)
    {
        size_t old_ele_cnt;
        vm_val_t old_val;
        size_t idx;
        vm_val_t nil_val;

        /* get the old size */
        old_ele_cnt = get_element_count();

        /* 
         *   If we're shrinking the vector, save undo for each element we're
         *   dropping off the end; to do this, simply set each element we're
         *   losing to nil, since this will add an undo record with the
         *   original value of the element.  
         *   
         *   Note that we must save undo for dropped elements BEFORE we set
         *   the vector's new sizd.  We must perform the steps in this order
         *   because undo is applied in reverse order: when we read back the
         *   undo, we'll first change the vector size to its old size (since
         *   that's the last saved undo instruction), then we'll set the
         *   elements to their old values.  
         */
        nil_val.set_nil();
        for (idx = new_ele_cnt ; idx < old_ele_cnt ; ++idx)
            set_element_undo(vmg_ self, idx, &nil_val);

        /* set up an integer with the pre-modification element count */
        old_val.set_int(old_ele_cnt);

        /* 
         *   add the undo record - use the special index value 0xffffffff
         *   as the key; this will never be a valid index for a real
         *   element, because we could never address an element with an
         *   index this high in a 32-bit address space 
         */
        G_undo->add_new_record_int_key(vmg_ self, 0xffffffffU, &old_val);
    }

    /* 
     *   if the new size is larger than our current allocation size,
     *   increase the allocated size 
     */
    if (new_ele_cnt > get_allocated_count())
    {
        size_t new_alloc_cnt;
        size_t new_mem_size;

        /* bump up the allocated size */
        new_alloc_cnt = get_allocated_count() + get_alloc_count_increment();

        /* if that's still not big enough, go up to the requested size */
        if (new_alloc_cnt < new_ele_cnt)
            new_alloc_cnt = new_ele_cnt;

        /* get the new size we need */
        new_mem_size = calc_alloc(new_alloc_cnt);

        /* reallocate our memory at the new, larger size */
        ext_ = (char *)G_mem->get_var_heap()
               ->realloc_mem(new_mem_size, ext_, this);

        /* set the new allocation size */
        set_allocated_count(new_alloc_cnt);

        /* 
         *   clear the undo bits (it's easier than moving them, and the only
         *   cost is that we might generate some redundant undo records) 
         */
        clear_undo_bits();
    }

    /* 
     *   if we're expanding the in-use size of the vector, set each
     *   newly-added element to nil 
     */
    if (new_ele_cnt > get_element_count())
    {
        size_t i;
        vm_val_t nil_val;

        /* 
         *   set the old value for each new element to nil (we don't have to
         *   save undo for this, since the undo operation for the change in
         *   size will simply discard all of the new elements) 
         */
        nil_val.set_nil();
        for (i = get_element_count() ; i < new_ele_cnt ; ++i)
            set_element(i, &nil_val);
    }

    /* set the new element count */
    set_element_count(new_ele_cnt);
}

/* ------------------------------------------------------------------------ */
/* 
 *   notify of new undo savepoint 
 */
void CVmObjVector::notify_new_savept()
{
    /* 
     *   clear the undo bits - we obviously have no undo for the new
     *   savepoint yet 
     */
    clear_undo_bits();
}

/* ------------------------------------------------------------------------ */
/*
 *   apply undo 
 */
void CVmObjVector::apply_undo(VMG_ struct CVmUndoRecord *rec)
{
    /* 
     *   if the record refers to index 0xffffffff, this is a size-change
     *   record; otherwise, it's an ordinary element change record 
     */
    if ((size_t)rec->id.intval == 0xffffffffU)
    {
        /* 
         *   it's a size change - apply the new size, which is stored as
         *   an integer in the old value record 
         */
        set_element_count((size_t)rec->oldval.val.intval);
    }
    else
    {
        /* it's an ordinary index record - set the element from undo data */
        set_element((size_t)rec->id.intval, &rec->oldval);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   mark references from an undo record 
 */
void CVmObjVector::mark_undo_ref(VMG_ CVmUndoRecord *rec)
{
    /* if the undo record refers to an object, mark the object */
    if (rec->oldval.typ == VM_OBJ)
        G_obj_table->mark_all_refs(rec->oldval.val.obj, VMOBJ_REACHABLE);
}

/* ------------------------------------------------------------------------ */
/* 
 *   mark references 
 */
void CVmObjVector::mark_refs(VMG_ uint state)
{
    size_t cnt;
    char *p;

    /* get my element count */
    cnt = get_element_count();

    /* mark as referenced each object in the vector */
    for (p = get_element_ptr(0) ; cnt != 0 ; --cnt, inc_element_ptr(&p))
    {
        /* 
         *   if this is an object, mark it as referenced, and mark its
         *   references as referenced 
         */
        if (vmb_get_dh_type(p) == VM_OBJ)
            G_obj_table->mark_all_refs(vmb_get_dh_obj(p), state);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   load from an image file 
 */
void CVmObjVector::load_from_image(VMG_ vm_obj_id_t self,
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
void CVmObjVector::reload_from_image(VMG_ vm_obj_id_t,
                                     const char *ptr, size_t siz)
{
    /* load the image data */
    load_image_data(vmg_ ptr, siz);
}

/* 
 *   load the data from an image file 
 */
void CVmObjVector::load_image_data(VMG_ const char *ptr, size_t siz)
{
    size_t alloc_cnt;
    size_t ele_cnt;

    /* if we already have memory allocated, free it */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* get the allocation size and the actual in-use size */
    alloc_cnt = vmb_get_len(ptr);
    ele_cnt = vmb_get_len(ptr + VMB_LEN);

    /* make sure we allocate at least as much as is actually in use */
    if (alloc_cnt < ele_cnt)
        alloc_cnt = ele_cnt;

    /* make sure the size isn't larger than we'd expect */
    if (siz > VMB_LEN*2 + (VMB_DATAHOLDER * ele_cnt))
        siz = VMB_LEN*2 + (VMB_DATAHOLDER * ele_cnt);

    /* 
     *   allocate memory at the new allocation size as indicated in the
     *   image data 
     */
    alloc_vector(vmg_ alloc_cnt);

    /* set the in-use element count */
    set_element_count(ele_cnt);

    /* 
     *   if the size is smaller than we'd expect from the initialized
     *   element count, set extra elements to nil 
     */
    if (siz < VMB_LEN*2 + (VMB_DATAHOLDER * ele_cnt))
    {
        size_t i;
        vm_val_t nil_val;

        /* set all elements to nil */
        nil_val.set_nil();
        for (i = 0 ; i < ele_cnt ; ++i)
            set_element(i, &nil_val);
    }

    /* copy the initialized elements from the image data */
    memcpy(get_element_ptr(0), ptr + VMB_LEN*2, siz - VMB_LEN*2);

    /* we're resetting to initial state, so forget all undo */
    clear_undo_bits();
}

/* ------------------------------------------------------------------------ */
/* 
 *   index the vector
 */
void CVmObjVector::index_val(VMG_ vm_val_t *result, vm_obj_id_t,
                             const vm_val_t *index_val)
{
    uint32 idx;

    /* get the index value as an integer */
    idx = index_val->num_to_int();

    /* make sure it's in range - 1 to our element count, inclusive */
    if (idx < 1 || idx > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* 
     *   get the indexed element and store it in the result, adjusting the
     *   index to the C-style 0-based range 
     */
    get_element(idx - 1, result);
}

/* ------------------------------------------------------------------------ */
/* 
 *   set an indexed element of the vector
 */
void CVmObjVector::set_index_val(VMG_ vm_val_t *new_container,
                                 vm_obj_id_t self,
                                 const vm_val_t *index_val,
                                 const vm_val_t *new_val)
{
    uint32 idx;

    /* get the index value as an integer */
    idx = index_val->num_to_int();

    /* make sure it's at least 1 */
    if (idx < 1)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* 
     *   if it's higher than the current length, extend the vector with nil
     *   entries to the requested size 
     */
    if (idx > get_element_count())
    {
        size_t i;
        vm_val_t nil_val;
        
        /* note the first new element index */
        i = get_element_count();

        /* extend the vector */
        set_element_count_undo(vmg_ self, idx);

        /* 
         *   Fill in entries between the old length and the new length with
         *   nil.  Note that we don't have to fill in the very last element,
         *   since we'll explicitly set it to the new value momentarily
         *   anyway.  Note also that we don't need to keep undo for the
         *   initializations, since on undo we'll truncate the vector to
         *   remove the newly-added elements and thus won't need to restore
         *   any values for the slots.  
         */
        for (nil_val.set_nil() ; i < (size_t)idx - 1 ; ++i)
            set_element(i, &nil_val);

        /* 
         *   set the new value - this doesn't require undo since we had to
         *   expand the vector to make room for it 
         */
        set_element(idx - 1, new_val);
    }
    else
    {
        /* set the element and record undo, using a zero-based index */
        set_element_undo(vmg_ self, (size_t)idx - 1, new_val);
    }

    /* the result is the original vector value */
    new_container->set_obj(self);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set an element and record undo for the change - takes a C-style 0-based
 *   index.  
 */
void CVmObjVector::set_element_undo(VMG_ vm_obj_id_t self,
                                    size_t idx, const vm_val_t *new_val)
{
    /* if we don't have undo for this element already, save undo now */
    if (G_undo != 0 && !get_undo_bit(idx))
    {
        vm_val_t old_val;

        /* get the pre-modification value of this element */
        get_element(idx, &old_val);

        /* add the undo record */
        G_undo->add_new_record_int_key(vmg_ self, idx, &old_val);

        /* 
         *   set the undo bit to indicate that we now have undo for this
         *   element 
         */
        set_undo_bit(idx, TRUE);
    }

    /* get the indexed element and store it in the result */
    set_element(idx, new_val);
}

/* ------------------------------------------------------------------------ */
/* 
 *   Check a value for equality.  We will match any list or vector with the
 *   same number of elements and the same value for each element.  
 */
int CVmObjVector::equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                         int depth) const
{
    const char *p;
    int p_is_list;
    CVmObjVector *vec2;
    size_t cnt;
    size_t cnt2;
    size_t idx;

    /* if the recursion depth is excessive, throw an error */
    if (depth > VM_MAX_TREE_DEPTH_EQ)
        err_throw(VMERR_TREE_TOO_DEEP_EQ);

    /* if the other value is a reference to myself, we certainly match */
    if (val->typ == VM_OBJ && val->val.obj == self)
    {
        /* no need to look at the contents if this is a reference to me */
        return TRUE;
    }

    /* try it as a list and as a vector */
    if ((p = val->get_as_list(vmg0_)) != 0)
    {
        /* it's a list */
        p_is_list = TRUE;

        /* get the number of elements in the list */
        cnt2 = vmb_get_len(p);

        /* the second element isn't a vector */
        vec2 = 0;
    }
    else
    {
        /* it's not a list */
        p_is_list = FALSE;

        /* check to see if it's a vector */
        if (val->typ == VM_OBJ
            && (vm_objp(vmg_ val->val.obj)->is_of_metaclass(metaclass_reg_)))
        {
            /* it's a vector - cast it accordingly */
            vec2 = (CVmObjVector *)vm_objp(vmg_ val->val.obj);

            /* get the number of elements */
            cnt2 = vec2->get_element_count();
        }
        else
        {
            /* it's some other type, so it can't be equal */
            return FALSE;
        }
    }

    /* if the sizes don't match, the values are not equal */
    cnt = get_element_count();
    if (cnt != cnt2)
        return FALSE;

    /* compare element by element */
    for (idx = 0 ; idx < cnt ; ++idx)
    {
        vm_val_t val1;
        vm_val_t val2;
        
        /* get this element of self */
        get_element(idx, &val1);

        /* get this element of the other value */
        if (p_is_list)
            CVmObjList::index_list(vmg_ &val2, p, idx + 1);
        else
            vec2->get_element(idx, &val2);
        
        /* if these elements aren't equal, our values aren't equal */
        if (!val1.equals(vmg_ &val2, depth + 1))
            return FALSE;

        /* 
         *   if the other value is a list, re-translate it in case we did
         *   any constant data swapping 
         */
        if (p_is_list)
            p = val->get_as_list(vmg0_);
    }

    /* we didn't find any differences, so the values are equal */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash value calculation 
 */
uint CVmObjVector::calc_hash(VMG_ vm_obj_id_t self, int depth) const
{
    uint hash;
    uint i;

    /* if the recursion depth is excessive, throw an error */
    if (depth > VM_MAX_TREE_DEPTH_EQ)
        err_throw(VMERR_TREE_TOO_DEEP_EQ);
    
    /* combine the hash values of the members of the vector */
    for (hash = 0, i = 0 ; i < get_element_count() ; ++i)
    {
        vm_val_t ele;
        
        /* get this element */
        get_element(i, &ele);

        /* 
         *   Calculate its hash value and add it into the combined hash.
         *   
         *   Note that we have to increase the recursion depth, because we're
         *   recursively calculating the hash value of our contents, and our
         *   contents can refer (directly or indirectly) back to this object.
         *   In other words, we can have cycles in the reference tree from
         *   this object back to itself, so we need to keep track of the
         *   recursion depth to ensure we don't loop forever.  
         */
        hash += ele.calc_hash(vmg_ depth + 1);
    }

    /* return the combined hash */
    return hash;
}

/* ------------------------------------------------------------------------ */
/* 
 *   add a value to the vector
 */
void CVmObjVector::add_val(VMG_ vm_val_t *result,
                           vm_obj_id_t self, const vm_val_t *val)
{
    size_t idx;
    size_t rhs_cnt;
    size_t i;
    CVmObjVector *new_vec;

    /* push the value to append, for gc protection */
    G_stk->push(val);

    /* 
     *   remember the index of the first new element - this is simply one
     *   higher than the last valid current index 
     */
    idx = get_element_count();

    /* get the number of elements to add */
    rhs_cnt = val->get_coll_addsub_rhs_ele_cnt(vmg0_);

    /* 
     *   allocate a copy of the vector for the result - make it big enough
     *   for my elements plus the elements to be appended 
     */
    result->set_obj(create(vmg_ FALSE, idx + rhs_cnt));

    /* get the return value as a vector */
    new_vec = (CVmObjVector *)vm_objp(vmg_ result->val.obj);

    /* 
     *   set the element count in the new vector (we're creating it anew, so
     *   there's no need to save undo) 
     */
    new_vec->set_element_count(idx + rhs_cnt);

    /* push the result for gc protection */
    G_stk->push(result);

    /* copy my elements into the result */
    memcpy(new_vec->get_element_ptr(0), get_element_ptr(0),
           calc_alloc_ele(idx));

    /* add the new elements */
    for (i = 0 ; i < rhs_cnt ; ++i, ++idx)
    {
        vm_val_t ele;

        /* get this element from the right-hand side */
        val->get_coll_addsub_rhs_ele(vmg_ i + 1, &ele);

        /* store the element in the result */
        new_vec->set_element(idx, &ele);
    }

    /* discard the gc protect */
    G_stk->discard(2);
}

/* ------------------------------------------------------------------------ */
/* 
 *   subtract a value from the vector
 */
void CVmObjVector::sub_val(VMG_ vm_val_t *result,
                           vm_obj_id_t self, const vm_val_t *val)
{
    size_t ele_cnt;
    size_t rhs_cnt;
    size_t i;
    CVmObjVector *new_vec;
    size_t new_cnt;

    /* push the value to append, for gc protection */
    G_stk->push(val);

    /* note the initial element count */
    ele_cnt = get_element_count();

    /* get the number of elements to remove */
    rhs_cnt = val->get_coll_addsub_rhs_ele_cnt(vmg0_);

    /* 
     *   allocate a new vector for the result - the new object will be no
     *   larger than the current object, so allocate at the same size 
     */
    result->set_obj(create(vmg_ FALSE, ele_cnt));

    /* push the result for gc protection */
    G_stk->push(result);

    /* get the return value as a vector */
    new_vec = (CVmObjVector *)vm_objp(vmg_ result->val.obj);

    /* 
     *   copy each element from me to the new vector, but don't copy
     *   elements found in the right-hand side 
     */
    for (i = 0, new_cnt = 0 ; i < ele_cnt ; ++i)
    {
        vm_val_t ele;
        size_t j;
        int found;

        /* get this element of self */
        get_element(i, &ele);

        /* scan the right side for the element */
        for (found = FALSE, j = 0 ; j < rhs_cnt ; ++j)
        {
            vm_val_t rh_ele;
            
            /* get this right-hand element */
            val->get_coll_addsub_rhs_ele(vmg_ j + 1, &rh_ele);

            /* 
             *   if this right-hand element matches this left-hand element,
             *   note that we have a match 
             */
            if (rh_ele.equals(vmg_ &ele))
            {
                /* note that we found it */
                found = TRUE;

                /* no need to look any further - one match is enough */
                break;
            }
        }

        /* 
         *   If we didn't find the value, include it in the result vector.
         *   Note that we don't have to save undo, since we're still
         *   constructing the new vector. 
         */
        if (!found)
        {
            /* store the element in the result vector */
            new_vec->set_element(new_cnt, &ele);

            /* count it */
            ++new_cnt;
        }
    }

    /* 
     *   update the new vector's size - we don't have to save undo for this
     *   change, because we're still constructing the new vector 
     */
    new_vec->set_element_count(new_cnt);

    /* discard the gc protection */
    G_stk->discard(2);
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - convert to a list 
 */
int CVmObjVector::getp_to_list(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc)
{
    size_t src_cnt;
    size_t dst_cnt;
    size_t start_idx;
    CVmObjList *lst;
    size_t idx;
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(0, 2);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* note our size */
    src_cnt = get_element_count();

    /* 
     *   if there's a starting index, retrieve it; otherwise, start at our
     *   first element 
     */
    if (orig_argc >= 1)
        start_idx = (size_t)CVmBif::pop_int_val(vmg0_);
    else
        start_idx = 1;

    /* if the starting index is below 1, force it to 1 */
    if (start_idx < 1)
        start_idx = 1;

    /* adjust the starting index to a 0-based index */
    --start_idx;

    /* 
     *   if there's a size argument, retrieve it; if it's not specified,
     *   use our actual size for the output size 
     */
    if (orig_argc >= 2)
        dst_cnt = (size_t)CVmBif::pop_int_val(vmg0_);
    else
        dst_cnt = src_cnt;

    /* 
     *   in no case will the result list have more elements than we can
     *   actually supply 
     */
    if (start_idx >= src_cnt)
    {
        /* we're starting past our last element - we can't supply anything */
        dst_cnt = 0;
    }
    else if (src_cnt - start_idx < dst_cnt)
    {
        /* we can't supply as many values as requested - lower the size */
        dst_cnt = src_cnt - start_idx;
    }

    /* push a self-reference for garbage collection protection */
    G_stk->push()->set_obj(self);

    /* create the new list */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, dst_cnt));
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* set the list elements */
    for (idx = 0 ; idx < dst_cnt ; ++idx)
    {
        vm_val_t val;

        /* get my element at this index */
        get_element(idx + start_idx, &val);

        /* store the element in the list */
        lst->cons_set_element(idx, &val);
    }

    /* discard the self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - copy from another vector or list 
 */
int CVmObjVector::getp_copy_from(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    vm_val_t src_val;
    size_t src_cnt = 0;
    size_t dst_cnt;
    size_t src_start;
    size_t dst_start;
    size_t copy_cnt;
    const char *src_lst;
    int src_is_list = FALSE;
    CVmObjVector *src_vec = 0;
    size_t i;
    static CVmNativeCodeDesc desc(4);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the source value */
    G_stk->pop(&src_val);

    /* get the source starting index */
    src_start = (size_t)CVmBif::pop_int_val(vmg0_);

    /* get the destination starting index */
    dst_start = (size_t)CVmBif::pop_int_val(vmg0_);

    /* get the number of elements to copy */
    copy_cnt = (size_t)CVmBif::pop_int_val(vmg0_);

    /* if either starting index is below 1, force it to 1 */
    if (src_start < 1)
        src_start = 1;
    if (dst_start < 1)
        dst_start = 1;

    /* adjust the starting indices to 0-based values */
    --src_start;
    --dst_start;

    /* note the destination length (i.e., my element count) */
    dst_cnt = get_element_count();

    /* get the source value */
    if ((src_lst = src_val.get_as_list(vmg0_)) != 0)
    {
        /* note that it's a list */
        src_is_list = TRUE;

        /* note the source size */
        src_cnt = vmb_get_len(src_lst);
    }
    else if (src_val.typ == VM_OBJ
             && (vm_objp(vmg_ src_val.val.obj)->get_metaclass_reg()
                 == metaclass_reg_))
    {
        /* it's a vector */
        src_vec = (CVmObjVector *)vm_objp(vmg_ src_val.val.obj);

        /* note the soure size */
        src_cnt = src_vec->get_element_count();
    }
    else
        err_throw(VMERR_BAD_TYPE_BIF);

    /* limit our copying to the remaining elements of the source */
    if (src_start >= src_cnt)
        copy_cnt = 0;
    else if (src_start + copy_cnt >= src_cnt)
        copy_cnt = src_cnt - src_start;

    /* expand the vector if necessary to make room for added elements */
    if (dst_start + copy_cnt > dst_cnt)
        set_element_count_undo(vmg_ self, dst_start + copy_cnt);

    /* set the list elements */
    for (i = 0 ; i < copy_cnt ; ++i)
    {
        vm_val_t val;

        /* get my element at this index */
        if (src_is_list)
            CVmObjList::index_list(vmg_ &val, src_lst, i + src_start + 1);
        else
            src_vec->get_element(i + src_start, &val);

        /* set my element at this index, recording undo for the change */
        set_element_undo(vmg_ self, i + dst_start, &val);
    }

    /* the return value is 'self' */
    retval->set_obj(self);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - fill with a value 
 */
int CVmObjVector::getp_fill_val(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc)
{
    vm_val_t fill_val;
    size_t cnt;
    size_t start_idx;
    size_t end_idx;
    size_t idx;
    uint orig_argc = (argc != 0 ? *argc : 0);
    static CVmNativeCodeDesc desc(1, 2);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* note our size */
    cnt = get_element_count();

    /* the return value is 'self' */
    retval->set_obj(self);

    /* get the fill value */
    G_stk->pop(&fill_val);

    /* 
     *   if there's a starting index, retrieve it; otherwise, start at our
     *   first element 
     */
    if (orig_argc >= 2)
        start_idx = (size_t)CVmBif::pop_int_val(vmg0_);
    else
        start_idx = 1;

    /* if the starting index is below 1, force it to 1 */
    if (start_idx < 1)
        start_idx = 1;

    /* adjust the starting index to a 0-based index */
    --start_idx;

    /* 
     *   if there's a count argument, retrieve it; if it's not specified,
     *   use our actual size for the count 
     */
    if (orig_argc >= 3)
        end_idx = start_idx + (size_t)CVmBif::pop_int_val(vmg0_);
    else
        end_idx = cnt;

    /* push self and the fill value for gc protection */
    G_stk->push(retval);
    G_stk->push(&fill_val);

    /* expand the vector to the requested size */
    if (end_idx > cnt)
        set_element_count_undo(vmg_ self, end_idx);

    /* set the elements to the fill value */
    for (idx = start_idx ; idx < end_idx ; ++idx)
    {
        /* set the element to the fill value, recording undo for the change */
        set_element_undo(vmg_ self, idx, &fill_val);
    }

    /* discard GC protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - get the number of elements 
 */
int CVmObjVector::getp_get_size(VMG_ vm_obj_id_t, vm_val_t *retval,
                                uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return my element count */
    retval->set_int(get_element_count());

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - select a subset; returns a new vector consisting
 *   of the subset of the original vector 
 */
int CVmObjVector::getp_subset(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc)
{
    const vm_val_t *func_val;
    size_t src;
    size_t dst;
    size_t cnt;
    CVmObjVector *new_vec;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while allocating to protect from gc */
    G_interpreter->push_obj(vmg_ self);

    /* 
     *   allocate the new vector that we'll return - all we know at this
     *   point is that the new vector won't be larger than the current
     *   vector, so allocate at the current size 
     */
    cnt = get_element_count();
    retval->set_obj(create(vmg_ FALSE, cnt));

    /* get the return value as an vector */
    new_vec = (CVmObjVector *)vm_objp(vmg_ retval->val.obj);

    /* 
     *   push a reference to the new list to protect it from the garbage
     *   collector, which could be invoked in the course of executing the
     *   user callback 
     */
    G_stk->push(retval);

    /*
     *   Go through each element of our list, and invoke the callback on
     *   each element.  If the element passes, write it to the current
     *   output location in the list; otherwise, just skip it.
     *   
     *   Note that we're using the same list as source and destination,
     *   which is easy because the list will either shrink or stay the
     *   same - we'll never need to insert new elements.  
     */
    for (src = dst = 0 ; src < cnt ; ++src)
    {
        vm_val_t ele;
        const vm_val_t *val;

        /* get this element from the source vector */
        get_element(src, &ele);

        /* push the element as the callback's argument */
        G_stk->push(&ele);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, "Vector.subset", 0);

        /* get the result from R0 */
        val = G_interpreter->get_r0();

        /* 
         *   if the callback returned non-nil and non-zero, include this
         *   element in the result 
         */
        if (val->typ == VM_NIL
            || (val->typ == VM_INT && val->val.intval == 0))
        {
            /* it's nil or zero - don't include it in the result */
        }
        else
        {
            /* 
             *   include this element in the result (there's no need to
             *   save undo, since the whole vector is new) 
             */
            new_vec->set_element(dst, &ele);

            /* advance the output index */
            ++dst;
        }
    }

    /* set the actual number of elements in the result */
    new_vec->set_element_count(dst);

    /* discard our gc protection (self, return value) and our arguments */
    G_stk->discard(3);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - apply a callback to each element
 */
int CVmObjVector::getp_apply_all(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    const vm_val_t *func_val;
    size_t idx;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while working to protect from gc */
    G_interpreter->push_obj(vmg_ self);

    /* 
     *   we're going to return the 'self' object, since we update the vector
     *   in-place 
     */
    retval->set_obj(self);

    /*
     *   Go through each element of our vector, invoking the callback on
     *   each element.  Replace each element with the result of the
     *   callback.  Note that we intentionally re-check the element count on
     *   each iteration, in case the callback changes the number of
     *   elements.  
     */
    for (idx = 0 ; idx < get_element_count() ; ++idx)
    {
        /* push this element as the callback's argument */
        push_element(vmg_ idx);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, "Vector.applyAll", 0);

        /* replace this element with the result */
        set_element_undo(vmg_ self, idx, G_interpreter->get_r0());
    }

    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - find the first element matching a condition
 */
int CVmObjVector::getp_index_which(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc)
{
    /* use our general handler, working forwards */
    return gen_index_which(vmg_ self, retval, argc, TRUE, FALSE);
}

/*
 *   property evaluator - lastIndexWhich 
 */
int CVmObjVector::getp_last_index_which(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *argc)
{
    /* use our general handler, working backwards */
    return gen_index_which(vmg_ self, retval, argc, FALSE, FALSE);
}

/*
 *   general handler for indexWhich and lastIndexWhich 
 */
int CVmObjVector::gen_index_which(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                  uint *argc, int forward, int count_only)
{
    const vm_val_t *func_val;
    size_t cnt;
    size_t idx;
    int val_cnt;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while working to protect from gc */
    G_interpreter->push_obj(vmg_ self);

    /* presume we won't find a matching element */
    retval->set_nil();
    val_cnt = 0;

    /* get the number of elements to visit */
    cnt = get_element_count();

    /* start at the first or last element, depending on direction */
    idx = (forward ? 0 : cnt);

    /*
     *   Go through each element of our vector, and invoke the callback on
     *   each element, looking for the first one that returns true.  
     */
    for (;;)
    {
        /* if we've reached the last element, we can stop looking */
        if (forward ? idx >= cnt : idx == 0)
            break;

        /* if we're going backwards, decrement the index */
        if (!forward)
            --idx;

        /* push the element as the callback's argument */
        push_element(vmg_ idx);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1,
                                     "Vector.indexWhich", 0);

        /* 
         *   if the callback returned true, we've found the element we're
         *   looking for 
         */
        if (G_interpreter->get_r0()->typ == VM_NIL
            || (G_interpreter->get_r0()->typ == VM_INT
                && G_interpreter->get_r0()->val.intval == 0))
        {
            /* nil or zero - this one failed the test, so keep looking */
        }
        else
        {
            /* it passed the test - check what we're doing */
            if (count_only)
            {
                /* we only want the count */
                ++val_cnt;
            }
            else
            {
                /* we want the (1-based) index - return it */
                retval->set_int(idx + 1);
            
                /* found it - no need to keep looking */
                break;
            }
        }

        /* if we're going forwards, increment the index */
        if (forward)
            ++idx;
    }
    
    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);

    /* return the count if desired */
    if (count_only)
        retval->set_int(val_cnt);
    
    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - lastValWhich 
 */
int CVmObjVector::getp_last_val_which(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *argc)
{
    /* get the index of the value using lastIndexWhich */
    getp_last_index_which(vmg_ self, retval, argc);

    /* if the return value is a valid index, get the value at the index */
    if (retval->typ == VM_INT)
    {
        int idx;
        
        /* get the element as the return value */
        idx = (int)retval->val.intval;
        get_element(idx - 1, retval);
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - valWhich 
 */
int CVmObjVector::getp_val_which(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    /* get the index of the value using indexWhich */
    getp_index_which(vmg_ self, retval, argc);

    /* if the return value is a valid index, get the value at the index */
    if (retval->typ == VM_INT)
    {
        int idx;

        /* get the element as the return value */
        idx = (int)retval->val.intval;
        get_element(idx - 1, retval);
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - call a callback for each element
 */
int CVmObjVector::getp_for_each(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc)
{
    /* use the general forEach processor */
    return for_each_gen(vmg_ self, retval, argc, FALSE);
}

/*
 *   property evaluator - call a callback for each element 
 */
int CVmObjVector::getp_for_each_assoc(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *argc)
{
    /* use the general forEach processor */
    return for_each_gen(vmg_ self, retval, argc, TRUE);
}


/*
 *   General forEach/forEachAssoc processor 
 */
int CVmObjVector::for_each_gen(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc,
                               int pass_key_to_cb)
{
    const vm_val_t *func_val;
    size_t idx;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while working to protect from gc */
    G_interpreter->push_obj(vmg_ self);

    /* no return value */
    retval->set_nil();

    /* 
     *   Invoke the callback on each element.  Note that we intentionally do
     *   not cache the element count, since it is possible that a program
     *   could change the vector size in the course of an iteration; if we
     *   cached the size, and the actual size were reduced during the
     *   iteration, we would visit invalid elements past the new end of the
     *   vector.  To avoid this possibility, we re-check the current element
     *   count on each iteration to make sure we haven't run off the end of
     *   the vector.  
     */
    for (idx = 0 ; idx < get_element_count() ; ++idx)
    {
        /* push the element as the callback's argument */
        push_element(vmg_ idx);

        /* push the index argument if desired */
        if (pass_key_to_cb)
            G_stk->push()->set_int(idx + 1);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, pass_key_to_cb ? 2 : 1,
                                     "Vector.forEach", 0);
    }

    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);
    
    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - mapAll
 */
int CVmObjVector::getp_map_all(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc)
{
    const vm_val_t *func_val;
    size_t idx;
    CVmObjVector *new_vec;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while working to protect from gc */
    G_interpreter->push_obj(vmg_ self);

    /* 
     *   allocate a new vector for the return value - the new vector will
     *   have the same size as the original, since we're mapping each
     *   element of the old vector to the corresponding element of the new
     *   vector 
     */
    retval->set_obj(create(vmg_ FALSE, get_allocated_count()));

    /* get the return value as an vector */
    new_vec = (CVmObjVector *)vm_objp(vmg_ retval->val.obj);

    /* set the result element count the same as my own */
    new_vec->set_element_count(get_element_count());

    /* 
     *   push a reference to the new list to protect it from the garbage
     *   collector, which could be invoked in the course of executing the
     *   user callback 
     */
    G_stk->push(retval);

    /*
     *   Go through each element of our vector, and invoke the callback on
     *   each element, storing the result in the corresponding element of
     *   the new vector.  Note that we re-check the element count on each
     *   iteration, in case the callback changes it on us.  
     */
    for (idx = 0 ; idx < get_element_count() ; ++idx)
    {
        /* push the element as the callback's argument */
        push_element(vmg_ idx);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, "Vector.mapAll", 0);

        /* 
         *   replace this element with the result (there's no need to save
         *   undo, since the whole vector is new) 
         */
        new_vec->set_element(idx, G_interpreter->get_r0());
    }

    /* discard our gc protection (self, new vector) and our arguments */
    G_stk->discard(3);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - indexOf 
 */
int CVmObjVector::getp_index_of(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc)
{
    /* use our general handler, going forwards */
    return gen_index_of(vmg_ self, retval, argc, TRUE, FALSE);
}

/*
 *   property evaluator - lastIndexOf 
 */
int CVmObjVector::getp_last_index_of(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                     uint *argc)
{
    /* use our general handler, going backwards */
    return gen_index_of(vmg_ self, retval, argc, FALSE, FALSE);
}

/*
 *   general handler for indexOf and lastIndexOf - searches the vector
 *   either forwards of backwards for a given value 
 */
int CVmObjVector::gen_index_of(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc, int forward, int count_only)
{
    const vm_val_t *val;
    size_t cnt;
    size_t idx;
    int val_cnt;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the value, but leave it on the stack */
    val = G_stk->get(0);

    /* push a self-reference while working to protect from gc */
    G_interpreter->push_obj(vmg_ self);

    /* presume we won't find a matching element */
    retval->set_nil();
    val_cnt = 0;

    /* get the number of elements to visit */
    cnt = get_element_count();

    /* start at the first or last element, depending on the direction */
    idx = (forward ? 0 : cnt);

    /* scan the list, looking for the element */
    for (;;)
    {
        vm_val_t ele;

        /* if we've reached the last element, stop looking */
        if (forward ? idx >= cnt : idx == 0)
            break;

        /* if we're going backwards, move to the next element position */
        if (!forward)
            --idx;

        /* get this element */
        get_element(idx, &ele);

        /* if the element matches the search value, return its index */
        if (ele.equals(vmg_ val))
        {
            /* it matches - see what we're doing */
            if (count_only)
            {
                /* they only want the count */
                ++val_cnt;
            }
            else
            {
                /* this is the one - return its 1-based index */
                retval->set_int(idx + 1);
                
                /* foind it - no need to keep searching */
                break;
            }
        }

        /* if we're going forwards, move to the next element */
        if (forward)
            ++idx;
    }

    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);

    /* return the count if desired */
    if (count_only)
        retval->set_int(val_cnt);
    
    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - countOf
 */
int CVmObjVector::getp_count_of(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc)
{
    /* use our general handler to obtain the count */
    return gen_index_of(vmg_ self, retval, argc, TRUE, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - countWhich
 */
int CVmObjVector::getp_count_which(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc)
{
    /* use our general handler to obtain the count */
    return gen_index_which(vmg_ self, retval, argc, TRUE, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - getUnique; returns a new vector consisting of the
 *   unique elements of the original vector 
 */
int CVmObjVector::getp_get_unique(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                  uint *argc)
{
    CVmObjVector *new_vec;
    size_t cnt;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* put myself on the stack for GC protection */
    G_interpreter->push_obj(vmg_ self);

    /* 
     *   allocate a new vector for the return value - the new vector will
     *   never be any larger than the original 
     */
    cnt = get_element_count();
    retval->set_obj(create(vmg_ FALSE, cnt));

    /* push a reference to the new list for gc protection */
    G_stk->push(retval);

    /* get the return value as a vector */
    new_vec = (CVmObjVector *)vm_objp(vmg_ retval->val.obj);

    /* start out with the result element count the same as my own */
    new_vec->set_element_count(cnt);

    /* copy my elements to the new vector */
    memcpy(new_vec->get_element_ptr(0), get_element_ptr(0),
           calc_alloc_ele(cnt));

    /* uniquify the result */
    new_vec->cons_uniquify(vmg0_);

    /* discard the gc protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/*
 *   For construction, eliminate repeated elements of the vector, leaving
 *   only the unique elements.  Reduces the size of the vector to the size
 *   required to accommodate the unique elements.  
 */
void CVmObjVector::cons_uniquify(VMG0_)
{
    size_t cnt;
    size_t src, dst;

    /* get the number of elements */
    cnt = get_element_count();

    /* loop through the list and look for repeated values */
    for (src = dst = 0 ; src < cnt ; ++src)
    {
        size_t idx;
        vm_val_t src_val;
        int found;

        /* 
         *   look for a copy of this source value already in the output list 
         */
        get_element(src, &src_val);
        for (idx = 0, found = FALSE ; idx < dst ; ++idx)
        {
            vm_val_t idx_val;

            /* get this value */
            get_element(idx, &idx_val);

            /* if it's equal to the current source value, note it */
            if (src_val.equals(vmg_ &idx_val))
            {
                /* note that we found it */
                found = TRUE;

                /* no need to look any further */
                break;
            }
        }

        /* if we didn't find the value, copy it to the output list */
        if (!found)
        {
            /* 
             *   add it to the output vector - since this is a
             *   construction-only method, there's no need to save undo (if
             *   we apply undo, we'll undo the entire construction of the
             *   vector, hence there's no need to track changes made since
             *   creation) 
             */
            set_element(dst, &src_val);

            /* count it in the output */
            ++dst;
        }
    }

    /* adjust the size of the result list */
    set_element_count(dst);
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - appendUnique 
 */
int CVmObjVector::getp_append_unique(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                     uint *argc)
{
    vm_val_t *val2;
    size_t cnt2;
    size_t cnt;
    static CVmNativeCodeDesc desc(1);
    size_t i;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the other value, but leave it on the stack */
    val2 = G_stk->get(0);

    /* the return value is 'self' */
    retval->set_obj(self);

    /* put myself on the stack for GC protection */
    G_stk->push(retval);

    /* get the number of elements in the original */
    cnt = get_element_count();

    /* get the number of elements to append */
    cnt2 = val2->get_coll_addsub_rhs_ele_cnt(vmg0_);

    /* expand myself to make room for the new elements */
    expand_by(vmg_ self, cnt2);

    /* append each element of the right-hand side */
    for (i = 0 ; i < cnt2 ; ++i)
    {
        vm_val_t ele;
        
        /* get this element */
        val2->get_coll_addsub_rhs_ele(vmg_ i + 1, &ele);

        /* store it */
        set_element_undo(vmg_ self, cnt + i, &ele);
    }
    
    /* uniquify the result */
    uniquify_for_append(vmg_ self);

    /* discard the gc protection and arguments */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* 
 *   uniquify after an appendUnique operation 
 */
void CVmObjVector::uniquify_for_append(VMG_ vm_obj_id_t self)
{
    size_t cnt;
    size_t src, dst;

    /* get the number of elements */
    cnt = get_element_count();

    /* loop through the list and look for repeated values */
    for (src = dst = 0 ; src < cnt ; ++src)
    {
        size_t idx;
        vm_val_t src_val;
        int found;

        /* look for a copy of this value elsewhere in the vector */
        get_element(src, &src_val);
        for (idx = 0, found = FALSE ; idx < dst ; ++idx)
        {
            vm_val_t idx_val;

            /* get this value */
            get_element(idx, &idx_val);

            /* if it's equal to the current source value, note it */
            if (src_val.equals(vmg_ &idx_val))
            {
                /* note that we found it */
                found = TRUE;

                /* no need to look any further */
                break;
            }
        }

        /* if we didn't find the value, copy it to the output list */
        if (!found)
        {
            /* 
             *   Add it to the output vector if we're actually changing this
             *   element (i.e., the destination and source indices are
             *   unequal).  
             */
            if (dst != src)
                set_element_undo(vmg_ self, dst, &src_val);

            /* count it in the output */
            ++dst;
        }
    }

    /* adjust the size of the result list */
    set_element_count_undo(vmg_ self, dst);
}

/* ------------------------------------------------------------------------ */
/*
 *   sorter for vector
 */
class CVmQSortVector: public CVmQSortVal
{
public:
    CVmQSortVector()
    {
        vec_ = 0;
        self_ = VM_INVALID_OBJ;
    }

    /* get an element from the vector */
    void get_ele(VMG_ size_t idx, vm_val_t *val)
        { vec_->get_element(idx, val); }

    /* store an element */
    void set_ele(VMG_ size_t idx, const vm_val_t *val)
        { vec_->set_element_undo(vmg_ self_, idx, val); }

    /* our vector object */
    CVmObjVector *vec_;
    vm_obj_id_t self_;
};

/*
 *   property evaluator - sort 
 */
int CVmObjVector::getp_sort(VMG_ vm_obj_id_t self, vm_val_t *retval,
                            uint *in_argc)
{
    size_t len;
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    CVmQSortVector sorter;    
    static CVmNativeCodeDesc desc(0, 2);

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* remember the length of my list */
    len = get_element_count();

    /* set the vector object in the sorter */
    sorter.vec_ = this;
    sorter.self_ = self;

    /* if we have an 'descending' flag, note it */
    if (argc >= 1)
        sorter.descending_ = (G_stk->get(0)->typ != VM_NIL);

    /* 
     *   if we have a comparison function, note it, but leave it on the
     *   stack for gc protection 
     */
    if (argc >= 2)
        sorter.compare_fn_ = *G_stk->get(1);

    /* put myself on the stack for GC protection */
    G_interpreter->push_obj(vmg_ self);

    /* sort the vector, if we have any elements */
    if (len != 0)
        sorter.sort(vmg_ 0, len - 1);

    /* discard the gc protection and arguments */
    G_stk->discard(1 + argc);

    /* return myself */
    retval->set_obj(self);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - set the length 
 */
int CVmObjVector::getp_set_length(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                  uint *argc)
{
    size_t old_len;
    size_t new_len;
    size_t idx;
    vm_val_t nil_val;
    static CVmNativeCodeDesc desc(1);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* the return value is 'self' */
    retval->set_obj(self);

    /* note the old length */
    old_len = get_element_count();

    /* get the new length */
    new_len = (size_t)CVmBif::pop_int_val(vmg0_);

    /* can't go less than zero */
    if (new_len < 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* set the vector to its new size */
    set_element_count_undo(vmg_ self, new_len);

    /* 
     *   Set each newly added element to nil.  Note that we don't bother
     *   saving undo for these changes: to undo this change, the only thing
     *   we'll have to do is reduce the vector size to its previous size,
     *   and we've already saved undo for the size change.  
     */
    nil_val.set_nil();
    for (idx = old_len ; idx < new_len ; ++idx)
        set_element(idx, &nil_val);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - insert one or more elements at a given index
 */
int CVmObjVector::getp_insert_at(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *in_argc)
{
    size_t idx;
    size_t ins_idx;
    size_t old_cnt;
    size_t add_cnt;
    size_t new_cnt;
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(2, 0, TRUE);

    /* we must have at least two arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the starting insertion index */
    ins_idx = (size_t)CVmBif::pop_int_val(vmg0_);

    /* the return value is 'self' */
    retval->set_obj(self);

    /* note the original size of the vector */
    old_cnt = get_element_count();

    /* 
     *   note the number of items we're adding - we're adding each
     *   argument, other than the first 
     */
    add_cnt = argc - 1;

    /* 
     *   note the new size - we're inserting one element for each argument
     *   past the first one 
     */
    new_cnt = old_cnt + add_cnt;

    /* 
     *   if the starting index is out of range, it's an error - it can
     *   range from 1 to one higher than the current element count (the
     *   top end of the range allows us to insert elements after all of
     *   the current elements) 
     */
    if (ins_idx < 1 || ins_idx > get_element_count() + 1)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* adjust the starting index to a zero-based value */
    --ins_idx;

    /* create the new elements */
    set_element_count_undo(vmg_ self, new_cnt);

    /* 
     *   Move the existing elements to their new slots.  Start with the
     *   starting index, and move it to its new position, since the starting
     *   index is the first element that needs to be moved.  Note that we
     *   start at the top and work down, to avoid overwriting any
     *   overlapping part of the new position block and the old position
     *   block.
     *   
     *   If we're inserting after the last element, there's no moving that
     *   needs to be done.  
     */
    if (ins_idx < old_cnt)
    {
        idx = old_cnt;
        for (;;)
        {
            size_t new_idx;
            vm_val_t ele;
            
            /* move to the previous element */
            --idx;
            
            /* get the value at this slot */
            get_element(idx, &ele);
            
            /* calculate the new slot */
            new_idx = idx + add_cnt;
            
            /* 
             *   if the new index is within the old size range, keep undo
             *   for the change; otherwise, no undo is necessary, since when
             *   we undo we'll be completely dropping this new slot 
             */
            if (new_idx < old_cnt)
                set_element_undo(vmg_ self, new_idx, &ele);
            else
                set_element(new_idx, &ele);
            
            /* if this was the last element, we're done */
            if (idx == ins_idx)
                break;
        }
    }

    /* add the new items */
    for (idx = ins_idx ; add_cnt != 0 ; ++idx, --add_cnt)
    {
        vm_val_t val;
        
        /* pop the next argument value */
        G_stk->pop(&val);

        /* store the item, keeping undo only if it's in the old size range */
        if (idx < old_cnt)
            set_element_undo(vmg_ self, idx, &val);
        else
            set_element(idx, &val);
    }

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - append an element 
 */
int CVmObjVector::getp_prepend(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc)
{
    vm_val_t val;
    size_t cnt;
    size_t i;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the old size */
    cnt = get_element_count();

    /* expand the vector by one element */
    set_element_count_undo(vmg_ self, cnt + 1);

    /* move each existing element up one slot to make room for the new one */
    for (i = cnt ; i != 0 ; --i)
    {
        /* get this element */
        get_element(i - 1, &val);
        
        /* 
         *   bump it up to the next slot - keep undo except for the last
         *   slot, which is newly added and thus doesn't have an old value
         *   that we need to keep
         */
        if (i == cnt)
            set_element(i, &val);
        else
            set_element_undo(vmg_ self, i, &val);
    }

    /* retrieve the value for the new element */
    G_stk->pop(&val);

    /* 
     *   set the first element - note that there's no need to save undo,
     *   since undoing this operation will simply discard this element (in
     *   other words, there's no previous value for this element since it's
     *   being created anew here) 
     */
    set_element(0, &val);

    /* the return value is 'self' */
    retval->set_obj(self);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - remove an element at the given position
 */
int CVmObjVector::getp_remove_element_at(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *argc)
{
    size_t start_idx;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the deletion index */
    start_idx = (size_t)CVmBif::pop_int_val(vmg0_);

    /* make sure the index is in range */
    if (start_idx < 1 || start_idx > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* adjust to a zero-based index */
    --start_idx;

    /* the return value is 'self' */
    retval->set_obj(self);

    /* delete one element */
    remove_elements_undo(vmg_ self, start_idx, 1);


    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - remove element(s) matching a given value 
 */
int CVmObjVector::getp_remove_element(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *argc)
{
    vm_val_t *del_val;
    static CVmNativeCodeDesc desc(1);
    size_t src;
    size_t dst;
    size_t old_cnt;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the value to be removed */
    del_val = G_stk->get(0);

    /* the return value is 'self'; leave it on the stack */
    retval->set_obj(self);
    G_stk->push(retval);

    /* get the original number of elements */
    old_cnt = get_element_count();

    /* delete each element matching the given value */
    for (src = dst = 0 ; src < old_cnt ; ++src)
    {
        vm_val_t ele_val;

        /* get this element */
        get_element(src, &ele_val);

        /* 
         *   if this element doesn't match the value to be deleted, add it
         *   to the result vector 
         */
        if (!ele_val.equals(vmg_ del_val))
        {
            /* 
             *   This element is a keeper - if we're moving it to a new
             *   position (i.e., we've deleted any elements before this
             *   one), store it at its new position, keeping undo
             *   information.  If we're storing it at its same position,
             *   there's no need to re-store the value or keep any undo,
             *   since no change is involved.  
             */
            if (dst != src)
                set_element_undo(vmg_ self, dst, &ele_val);

            /* increment the write index past this element */
            ++dst;
        }
    }

    /* if the element count changed, set the new element count */
    if (dst != old_cnt)
        set_element_count_undo(vmg_ self, dst);

    /* discard gc protection and arguments */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - remove a range of elements
 */
int CVmObjVector::getp_remove_range(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    size_t start_idx;
    size_t end_idx;
    static CVmNativeCodeDesc desc(2);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the starting index */
    start_idx = (size_t)CVmBif::pop_int_val(vmg0_);

    /* 
     *   make sure the index is in range - it must refer to an existing
     *   element 
     */
    if (start_idx < 1 || start_idx > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* get the ending index */
    end_idx = (size_t)CVmBif::pop_int_val(vmg0_);

    /* 
     *   make sure it's in range - it must refer to an existing element,
     *   and it must be greater than or equal to the starting index 
     */
    if (end_idx < start_idx || end_idx > get_element_count())
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* adjust to zero-based indices */
    --start_idx;
    --end_idx;

    /* the return value is 'self' */
    retval->set_obj(self);

    /* 
     *   delete the elements - the number of elements we're deleting is
     *   one higher than the difference of the starting and ending indices
     *   (if the two indices are the same, we're deleting just the one
     *   element) 
     */
    remove_elements_undo(vmg_ self, start_idx, end_idx - start_idx + 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - append an element 
 */
int CVmObjVector::getp_append(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc)
{
    vm_val_t *valp;
    size_t cnt;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the value to append, but leave it on the stack */
    valp = G_stk->get(0);

    /* get the old size */
    cnt = get_element_count();

    /* the result object is 'self' */
    retval->set_obj(self);

    /* push a self-reference for gc protection */
    G_stk->push(retval);

    /* expand myself by one element to make room for the addition */
    expand_by(vmg_ self, 1);

    /* add the new element, saving undo */
    set_element_undo(vmg_ self, cnt, valp);

    /* discard the argument and gc protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - append elements from a collection, or append a
 *   single non-collection element 
 */
int CVmObjVector::getp_append_all(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                  uint *argc)
{
    vm_val_t *valp;
    size_t cnt;
    static CVmNativeCodeDesc desc(1);
    size_t add_cnt;
    size_t i;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the value to append, but leave it on the stack */
    valp = G_stk->get(0);

    /* the result object is 'self' */
    retval->set_obj(self);

    /* push a self-reference for gc protection */
    G_stk->push(retval);

    /* get the old size */
    cnt = get_element_count();

    /* get the number of items to add */
    add_cnt = valp->get_coll_addsub_rhs_ele_cnt(vmg0_);

    /* expand myself to make room for the new elements */
    expand_by(vmg_ self, add_cnt);

    /* add the new elements */
    for (i = 0 ; i < add_cnt ; ++i)
    {
        vm_val_t ele;
        
        /* get the next new element */
        valp->get_coll_addsub_rhs_ele(vmg_ i + 1, &ele);
        
        /* 
         *   add the new element - note that we don't need to keep undo
         *   because we created this new element in this same operation, and
         *   hence undoing the operation will truncate the vector to exclude
         *   the element, and hence we don't need an old value for the
         *   element 
         */
        set_element(cnt + i, &ele);
    }

    /* discard the argument and gc protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete a range of elements 
 */
void CVmObjVector::remove_elements_undo(VMG_ vm_obj_id_t self,
                                        size_t start_idx, size_t del_cnt)
{
    size_t idx;
    size_t cnt;
    
    /* note the original size of the vector */
    cnt = get_element_count();

    /* move all of the elements to their new slots, keeping undo */
    for (idx = start_idx ; idx < cnt ; ++idx)
    {
        vm_val_t val;
        size_t move_idx;
        
        /* 
         *   calculate the index of the element to replace this one - it's
         *   past us by the number of items being deleted 
         */
        move_idx = idx + del_cnt;

        /* 
         *   if we're moving from a valid index in the old vector, get the
         *   element; otherwise, replace the value with nil (but still
         *   replace the value, since we want to keep undo for its
         *   original value) 
         */
        if (move_idx < cnt)
            get_element(move_idx, &val);
        else
            val.set_nil();

        /* store the element at its new position */
        set_element_undo(vmg_ self, idx, &val);
    }

    /*
     *   Reduce the size, keeping undo.  Note that it's important we do
     *   this last: undo is applied in reverse order, so when applying
     *   undo, we first want to increase the vector's size to its original
     *   size, then we want to apply the element value restorations.
     */
    set_element_count_undo(vmg_ self, cnt - del_cnt);
}


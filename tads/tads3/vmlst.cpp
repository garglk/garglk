#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMLST.CPP,v 1.3 1999/05/17 02:52:28 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmlst.cpp - list metaclass
Function
  
Notes
  
Modified
  10/29/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "vmmcreg.h"
#include "vmtype.h"
#include "vmlst.h"
#include "vmobj.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmfile.h"
#include "vmpool.h"
#include "vmstack.h"
#include "vmmeta.h"
#include "vmrun.h"
#include "vmbif.h"
#include "vmpredef.h"
#include "vmiter.h"
#include "vmsort.h"
#include "vmstr.h"
#include "vmbiftad.h"



/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassList metaclass_reg_obj;
CVmMetaclass *CVmObjList::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (*CVmObjList::func_table_[])(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *lst, uint *argc) =
{
    &CVmObjList::getp_undef,                                           /* 0 */
    &CVmObjList::getp_subset,                                          /* 1 */
    &CVmObjList::getp_map,                                             /* 2 */
    &CVmObjList::getp_len,                                             /* 3 */
    &CVmObjList::getp_sublist,                                         /* 4 */
    &CVmObjList::getp_intersect,                                       /* 5 */
    &CVmObjList::getp_index_of,                                        /* 6 */
    &CVmObjList::getp_car,                                             /* 7 */
    &CVmObjList::getp_cdr,                                             /* 8 */
    &CVmObjList::getp_index_which,                                     /* 9 */
    &CVmObjList::getp_for_each,                                       /* 10 */
    &CVmObjList::getp_val_which,                                      /* 11 */
    &CVmObjList::getp_last_index_of,                                  /* 12 */
    &CVmObjList::getp_last_index_which,                               /* 13 */
    &CVmObjList::getp_last_val_which,                                 /* 14 */
    &CVmObjList::getp_count_of,                                       /* 15 */
    &CVmObjList::getp_count_which,                                    /* 16 */
    &CVmObjList::getp_get_unique,                                     /* 17 */
    &CVmObjList::getp_append_unique,                                  /* 18 */
    &CVmObjList::getp_append,                                         /* 19 */
    &CVmObjList::getp_sort,                                           /* 20 */
    &CVmObjList::getp_prepend,                                        /* 21 */
    &CVmObjList::getp_insert_at,                                      /* 22 */
    &CVmObjList::getp_remove_element_at,                              /* 23 */
    &CVmObjList::getp_remove_range,                                   /* 24 */
    &CVmObjList::getp_for_each_assoc,                                 /* 25 */
    &CVmObjList::getp_generate,                                       /* 26 */
    &CVmObjList::getp_splice,                                         /* 27 */
    &CVmObjList::getp_join,                                           /* 28 */
    &CVmObjList::getp_indexOfMin,                                     /* 29 */
    &CVmObjList::getp_minVal,                                         /* 30 */
    &CVmObjList::getp_indexOfMax,                                     /* 31 */
    &CVmObjList::getp_maxVal                                          /* 32 */
};

/* static property indices */
const int PROPIDX_generate = 26;


/* ------------------------------------------------------------------------ */
/*
 *   Static creation methods.  These routines allocate an object ID and
 *   create a new list object. 
 */

/* create dynamically using stack arguments */
vm_obj_id_t CVmObjList::create_from_stack(VMG_ const uchar **pc_ptr,
                                          uint argc)
{
    vm_obj_id_t id;
    CVmObjList *lst;
    size_t idx;
    
    /* 
     *   create the list - this type of construction is never used for
     *   root set objects
     */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* create a list with one element per argument */
    lst = new (vmg_ id) CVmObjList(vmg_ argc);

    /* add each argument */
    for (idx = 0 ; idx < argc ; ++idx)
    {
        /* retrieve the next element from the stack and add it to the list */
        lst->cons_set_element(idx, G_stk->get(idx));
    }

    /* discard the stack parameters */
    G_stk->discard(argc);

    /* return the new object */
    return id;
}

/* 
 *   create dynamically from the current method's parameters 
 */
vm_obj_id_t CVmObjList::create_from_params(VMG_ uint param_idx, uint cnt)
{
    vm_obj_id_t id;
    CVmObjList *lst;
    size_t idx;

    /* create the new list object as a non-root-set object */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);
    lst = new (vmg_ id) CVmObjList(vmg_ cnt);

    /* copy each parameter into the new list */
    for (idx = 0 ; cnt != 0 ; --cnt, ++param_idx, ++idx)
    {
        /* retrieve the next element and add it to the list */
        lst->cons_set_element(
            idx, G_interpreter->get_param(vmg_ param_idx));
    }

    /* return the new object */
    return id;
}

/* create a list with no initial contents */
vm_obj_id_t CVmObjList::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjList();
    return id;
}

/* create a list with a given number of elements */
vm_obj_id_t CVmObjList::create(VMG_ int in_root_set,
                               size_t element_count)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjList(vmg_ element_count);
    return id;
}

/* create a list by copying a constant list */
vm_obj_id_t CVmObjList::create(VMG_ int in_root_set, const char *lst)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjList(vmg_ lst);
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Constructors.  These are called indirectly through our static
 *   creation methods.  
 */

/*
 *   create a list object from a constant list 
 */
CVmObjList::CVmObjList(VMG_ const char *lst)
{
    /* get the element count from the original list */
    size_t cnt = vmb_get_len(lst);

    /* allocate space */
    alloc_list(vmg_ cnt);

    /* copy the list's contents */
    memcpy(ext_, lst, calc_alloc(cnt));
}

/*
 *   Create a list with a given number of elements.  This can be used to
 *   construct a list element-by-element. 
 */
CVmObjList::CVmObjList(VMG_ size_t cnt)
{
    /* allocate space */
    alloc_list(vmg_ cnt);

    /* 
     *   Clear the list.  Since the caller is responsible for populating the
     *   list in this version of the constructor, it's possible that GC will
     *   run between now and the time the list is fully populated.  We must
     *   initialize the list to ensure that we don't misinterpret the
     *   contents as valid should we run GC between now and the time the
     *   caller has finished populating the list.  It's adequate to set the
     *   list to all zeros, since we won't try to interpret the contents as
     *   valid if the type markers are all invalid.  
     */
    memset(ext_ + VMB_LEN, 0, calc_alloc(cnt) - VMB_LEN);
}

/* ------------------------------------------------------------------------ */
/*
 *   allocate space for a list with a given number of elements 
 */
void CVmObjList::alloc_list(VMG_ size_t cnt)
{
    /* calculate the allocation size */
    size_t alo = calc_alloc(cnt);

    /* 
     *   ensure we're within the limit (NB: this really is 65535 on ALL
     *   PLATFORMS - this is a portable limit imposed by the storage format,
     *   not a platform-specific size limit 
     */
    if (alo > 65535)
    {
        ext_ = 0;
        err_throw(VMERR_LIST_TOO_LONG);
    }

    /* allocate space for the given number of elements */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(alo, this);

    /* set the element count */
    vmb_put_len(ext_, cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   construction: set an element 
 */
void CVmObjList::cons_set_element(size_t idx, const vm_val_t *val)
{
    /* set the element's value */
    vmb_put_dh(get_element_ptr(idx), val);
}

/*
 *   construction: copy a list into our list 
 */
void CVmObjList::cons_copy_elements(size_t idx, const char *orig_list)
{
    /* copy the elements */
    memcpy(get_element_ptr(idx), orig_list + VMB_LEN,
           (vmb_get_len(orig_list) * VMB_DATAHOLDER));
}

/*
 *   construction: copy element data into our list 
 */
void CVmObjList::cons_copy_data(size_t idx, const char *ele_array,
                                size_t ele_count)
{
    /* copy the elements */
    memcpy(get_element_ptr(idx), ele_array, ele_count * VMB_DATAHOLDER);
}

/*
 *   construction: ensure there's enough space for the given number of
 *   elements, expanding the list if necessary 
 */
void CVmObjList::cons_ensure_space(VMG_ size_t idx, size_t margin)
{
    /* if there's not enough space, expand the list */
    size_t old_cnt = vmb_get_len(ext_);
    if (idx >= old_cnt)
    {
        /* calculate the new size */
        size_t new_cnt = idx + 1 + margin;
        size_t new_siz = calc_alloc(new_cnt);

        /* make sure it's in range */
        if (new_siz > 65535)
            err_throw(VMERR_LIST_TOO_LONG);
        
        /* reallocate the list at the new size */
        ext_ = (char *)G_mem->get_var_heap()->realloc_mem(new_siz, ext_, this);

        /* set the new length */
        vmb_put_len(ext_, new_cnt);

        /*
         *   Because the caller is iteratively building the list, we should
         *   assume that garbage collection might be triggered during the
         *   process.  Make sure that if the gc visits our elements during
         *   construction, it finds only valid data (not, for example, stray
         *   pointers to invalid objects). 
         */
        cons_clear(old_cnt, new_cnt - 1);
    }
}

/*
 *   construction: clear a portion of the list, setting each element in the
 *   range (inclusive of the endpoints) to nil 
 */
void CVmObjList::cons_clear(size_t start_idx, size_t end_idx)
{
    /* proceed only if we have a non-zero number of elements */
    size_t cnt = get_ele_count();
    if (cnt != 0)
    {
        /* limit end_idx to the last element */
        if (end_idx >= cnt)
            end_idx = cnt - 1;

        /* clear the range */
        size_t i;
        char *p;
        for (i = start_idx, p = get_element_ptr(i) ; i <= end_idx ;
             ++i, p += VMB_DATAHOLDER)
            vmb_put_dh_nil(p);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   receive notification of deletion 
 */
void CVmObjList::notify_delete(VMG_ int in_root_set)
{
    /* free our extension */
    if (ext_ != 0 && !in_root_set)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/* 
 *   call a static property 
 */
int CVmObjList::call_stat_prop(VMG_ vm_val_t *result,
                               const uchar **pc_ptr, uint *argc,
                               vm_prop_id_t prop)
{
    /* get the function table index */
    int idx = G_meta_table->prop_to_vector_idx(
        metaclass_reg_->get_reg_idx(), prop);

    /* check for static methods */
    switch (idx)
    {
    case PROPIDX_generate:
        return static_getp_generate(vmg_ result, argc);

    default:
        /* inherit the default handling */
        return CVmObjCollection::call_stat_prop(
            vmg_ result, pc_ptr, argc, prop);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Set a property.  Lists have no settable properties, so simply signal
 *   an error indicating that the set-prop call is invalid.  
 */
void CVmObjList::set_prop(VMG_ CVmUndo *, vm_obj_id_t,
                          vm_prop_id_t, const vm_val_t *)
{
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/*
 *   Save the object to a file 
 */
void CVmObjList::save_to_file(VMG_ CVmFile *fp)
{
    size_t cnt;

    /* get our element count */
    cnt = vmb_get_len(ext_);

    /* write the count and the elements */
    fp->write_bytes(ext_, calc_alloc(cnt));
}

/*
 *   Restore the object from a file 
 */
void CVmObjList::restore_from_file(VMG_ vm_obj_id_t,
                                   CVmFile *fp, CVmObjFixup *fixups)
{
    size_t cnt;

    /* read the element count */
    cnt = fp->read_uint2();

    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* allocate the space */
    alloc_list(vmg_ cnt);

    /* store our element count */
    vmb_put_len(ext_, cnt);

    /* read the contents, if there are any elements */
    fp->read_bytes(ext_ + VMB_LEN, cnt * VMB_DATAHOLDER);

    /* fix object references */
    fixups->fix_dh_array(vmg_ ext_ + VMB_LEN, cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Mark references 
 */
void CVmObjList::mark_refs(VMG_ uint state)
{
    size_t cnt;
    char *p;

    /* get my element count */
    cnt = vmb_get_len(ext_);

    /* mark as referenced each object in our list */
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
 *   Add a value to the list.  This yields a new list, with the value
 *   appended to the existing list.  If the value to be appended is itself
 *   a list (constant or object), we'll append each element of that list
 *   to our list (rather than appending a single element containing a
 *   sublist).  
 */
int CVmObjList::add_val(VMG_ vm_val_t *result,
                        vm_obj_id_t self, const vm_val_t *val)
{
    /* 
     *   Use the generic list adder, using my extension as the constant
     *   list.  We store our extension in the general list format required
     *   by the static adder. 
     */
    add_to_list(vmg_ result, self, ext_, val);

    /* handled */
    return TRUE;
}

/*
 *   Static list adder.  This creates a new list object that results from
 *   appending the given value to the given list constant.  This is
 *   defined statically so that this code can be shared for adding to
 *   constant pool lists and adding to CVmObjList objects.
 *   
 *   'lstval' must point to a constant list.  The first two bytes of the
 *   list are stored in portable UINT2 format and give the number of
 *   elements in the list; this is immediately followed by a packed array
 *   of data holders in portable format. 
 *   
 *   Note that we *always* create a new object to hold the result, even if
 *   the new string is identical to the first, so that we consistently
 *   return a distinct reference from the original.  
 */
void CVmObjList::add_to_list(VMG_ vm_val_t *result,
                             vm_obj_id_t self, const char *lstval,
                             const vm_val_t *rhs)
{
    int lhs_cnt, rhs_cnt, alo_cnt;
    vm_obj_id_t obj;
    CVmObjList *objptr;

    /* push self and the other list for protection against GC */
    G_stk->push()->set_obj(self);
    G_stk->push(rhs);

    /* get the number of elements in the left-hand ('self') side */
    lhs_cnt = vmb_get_len(lstval);

    /* get the number of elements the right-hand side concatenates */
    rhs_cnt = (rhs->is_listlike(vmg0_) ? rhs->ll_length(vmg0_) : -1);

    /* if it's not a list, allocate on element */
    alo_cnt = (rhs_cnt < 0 ? 1 : rhs_cnt);

    /* allocate a new object to hold the new list */
    obj = create(vmg_ FALSE, lhs_cnt + alo_cnt);
    objptr = (CVmObjList *)vm_objp(vmg_ obj);

    /* copy the first list into the new object's list buffer */
    objptr->cons_copy_elements(0, lstval);

    /* add the value or its contents */
    if (rhs_cnt < 0)
    {
        /* single value - add it as-is */
        objptr->cons_set_element(lhs_cnt, rhs);
    }
    else
    {
        /* 
         *   clear the rest of the list, in case gc runs while retrieving
         *   elements from the rhs 
         */
        objptr->cons_clear(lhs_cnt, lhs_cnt + alo_cnt - 1);

        /* add each element from the right-hand side */
        for (int i = 1 ; i <= rhs_cnt ; ++i)
        {
            /* retrieve this element of the rhs */
            vm_val_t val;
            rhs->ll_index(vmg_ &val, i);
                
            /* store the element in the new list */
            objptr->cons_set_element(lhs_cnt + i - 1, &val);
        }
    }

    /* set the result to the new list */
    result->set_obj(obj);

    /* discard the GC protection items */
    G_stk->discard(2);
}

/* ------------------------------------------------------------------------ */
/*
 *   Subtract a value from the list.
 */
int CVmObjList::sub_val(VMG_ vm_val_t *result,
                        vm_obj_id_t self, const vm_val_t *val)
{
    vm_val_t self_val;
    
    /* 
     *   Invoke our static list subtraction routine, using our extension
     *   as the constant list.  Our extension is stored in the same format
     *   as a constant list, so we can use the same code to handle
     *   subtraction from a list object as we would for subtraction from a
     *   constant list. 
     */
    self_val.set_obj(self);
    sub_from_list(vmg_ result, &self_val, ext_, val);

    /* handled */
    return TRUE;
    
}

/*
 *   Subtract a value from a constant list. 
 */
void CVmObjList::sub_from_list(VMG_ vm_val_t *result,
                               const vm_val_t *lstval, const char *lstmem,
                               const vm_val_t *rhs)
{
    int lhs_cnt, rhs_cnt;
    vm_obj_id_t obj;
    CVmObjList *objptr;
    char *dst;
    size_t dst_cnt;
    int i;

    /* push self and the other list for protection against GC */
    G_stk->push(lstval);
    G_stk->push(rhs);

    /* get the number of elements in the right-hand side */
    lhs_cnt = vmb_get_len(lstmem);

    /* 
     *   allocate a new object to hold the new list, which will be no
     *   bigger than the original left-hand side, since we're doing
     *   nothing but (possibly) taking elements out 
     */
    obj = create(vmg_ FALSE, lhs_cnt);
    objptr = (CVmObjList *)vm_objp(vmg_ obj);

    /* get the number of elements to consider from the right-hand side */
    rhs_cnt = (rhs->is_listlike(vmg0_) ? rhs->ll_length(vmg0_) : -1);

    /* copy the first list into the new object's list buffer */
    objptr->cons_copy_elements(0, lstmem);

    /* consider each element of the left-hand side */
    for (i = 0, dst = objptr->get_element_ptr(0), dst_cnt = 0 ;
         i < lhs_cnt ; ++i)
    {
        vm_val_t src_val;
        int keep;

        /* 
         *   if our list is from constant memory, get its address again --
         *   the address could have changed due to swapping if we
         *   traversed into another list 
         */
        VM_IF_SWAPPING_POOL(if (lstval != 0 && lstval->typ == VM_LIST)
            lstmem = G_const_pool->get_ptr(lstval->val.ofs));

        /* get this element */
        vmb_get_dh(get_element_ptr_const(lstmem, i), &src_val);

        /* presume we'll keep it */
        keep = TRUE;

        /* 
         *   scan the right side to see if we can find this value - if we
         *   can, it's to be removed, so we don't want to copy it to the
         *   result list 
         */
        if (rhs_cnt < 0)
        {
            /* the rhs isn't a list - consider the value directly */
            if (rhs->equals(vmg_ &src_val))
            {
                /* it matches, so we're removing it */
                keep = FALSE;
            }
        }
        else
        {
            /* the rhs is a list - consider each element */
            for (int j = 1 ; j <= rhs_cnt ; ++j)
            {
                /* retrieve this rhs value */
                vm_val_t rem_val;
                rhs->ll_index(vmg_ &rem_val, j);
                
                /* if this value matches, we're removing it */
                if (rem_val.equals(vmg_ &src_val))
                {
                    /* it's to be removed */
                    keep = FALSE;

                    /* no need to look any further in the rhs list */
                    break;
                }
            }
        }

        /* if we're keeping the value, put it in the result list */
        if (keep)
        {
            /* store it in the result list */
            vmb_put_dh(dst, &src_val);

            /* advance the result pointer */
            inc_element_ptr(&dst);

            /* count it */
            ++dst_cnt;
        }
    }

    /* set the length of the result list */
    objptr->cons_set_len(dst_cnt);

    /* set the result to the new list */
    result->set_obj(obj);

    /* discard the GC protection items */
    G_stk->discard(2);
}

/* ------------------------------------------------------------------------ */
/*
 *   Index the list 
 */
int CVmObjList::index_val_q(VMG_ vm_val_t *result, vm_obj_id_t /*self*/,
                            const vm_val_t *index_val)
{
    /* 
     *   use the constant list indexing routine, using our extension data
     *   as the list data 
     */
    index_list(vmg_ result, ext_, index_val);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Index a constant list 
 */
void CVmObjList::index_list(VMG_ vm_val_t *result, const char *lst,
                            const vm_val_t *index_val)
{
    /* get the index value as an integer */
    uint32_t idx = index_val->num_to_int(vmg0_);

    /* index the list */
    index_list(vmg_ result, lst, idx);
}

/*
 *   Index a constant list by an integer value
 */
void CVmObjList::index_list(VMG_ vm_val_t *result, const char *lst, uint idx)
{
    /* make sure it's in range - 1 to our element count, inclusive */
    if (idx < 1 || idx > vmb_get_len(lst))
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* 
     *   get the indexed element and store it in the result, adjusting the
     *   index to the C-style 0-based range 
     */
    get_element_const(lst, idx - 1, result);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set an element of the list
 */
int CVmObjList::set_index_val_q(VMG_ vm_val_t *result, vm_obj_id_t self,
                                const vm_val_t *index_val,
                                const vm_val_t *new_val)
{
    /* put the list on the stack to avoid garbage collection */
    G_stk->push()->set_obj(self);
    
    /* 
     *   use the constant list set-index routine, using our extension data
     *   as the list data 
     */
    set_index_list(vmg_ result, ext_, index_val, new_val);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set an element in a constant list 
 */
void CVmObjList::set_index_list(VMG_ vm_val_t *result, const char *lst,
                                const vm_val_t *index_val,
                                const vm_val_t *new_val)
{
    /* get the index value as an integer */
    uint32_t idx = index_val->num_to_int(vmg0_);

    /* push the new value for gc protection during the create */
    G_stk->push(new_val);

    /* make sure it's in range - 1 to our element count, inclusive */
    if (idx < 1 || idx > vmb_get_len(lst))
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* create a new list as a copy of this list */
    result->set_obj(create(vmg_ FALSE, lst));

    /* get the new list object */
    CVmObjList *obj = (CVmObjList *)vm_objp(vmg_ result->val.obj);

    /* update the element of the new list */
    obj->cons_set_element(idx - 1, new_val);

    /* discard our gc protection */
    G_stk->discard();
}

/* ------------------------------------------------------------------------ */
/*
 *   Check a value for equality 
 */
int CVmObjList::equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                       int depth) const
{
    /* if it's a reference to myself, we certainly match */
    if (val->typ == VM_OBJ && val->val.obj == self)
        return TRUE;

    /* 
     *   compare via the constant list comparison routine, using our
     *   extension data as the list data 
     */
    return const_equals(vmg_ 0, ext_, val, depth);
}

/* ------------------------------------------------------------------------ */
/*
 *   Constant list comparison routine 
 */
int CVmObjList::const_equals(VMG_ const vm_val_t *lstval, const char *lstmem,
                             const vm_val_t *val, int depth)
{
    size_t cnt;
    size_t idx;
    
    /* if the other value isn't a list, it's no match */
    if (!val->is_listlike(vmg0_))
        return FALSE;

    /* if the lists don't have the same length, they don't match */
    cnt = vmb_get_len(lstmem);
    if ((int)cnt != val->ll_length(vmg0_))
        return FALSE;

    /* compare each element in the list */
    for (idx = 0 ; idx < cnt ; ++idx)
    {
        vm_val_t val1;
        vm_val_t val2;

        /* 
         *   if either list comes from constant memory, re-translate its
         *   pointer, in case we did any swapping while traversing the
         *   previous element 
         */
        VM_IF_SWAPPING_POOL(if (lstval != 0 && lstval->typ == VM_LIST)
            lstmem = G_const_pool->get_ptr(lstval->val.ofs));
        
        /* get the two elements */
        vmb_get_dh(get_element_ptr_const(lstmem, idx), &val1);
        val->ll_index(vmg_ &val2, idx + 1);

        /* 
         *   If these elements don't match, the lists don't match.  Note that
         *   lists can't contain circular references (by their very nature as
         *   immutable objects), so we don't need to increase the depth. 
         */
        if (!val1.equals(vmg_ &val2, depth))
            return FALSE;
    }

    /* if we got here, we didn't find any differences, so they match */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Hash value calculation 
 */
uint CVmObjList::calc_hash(VMG_ vm_obj_id_t self, int depth) const
{
    vm_val_t self_val;

    /* set up our 'self' value pointer */
    self_val.set_obj(self);

    /* calculate the value */
    return const_calc_hash(vmg_ &self_val, ext_, depth);
}

/*
 *   Hash value calculation 
 */
uint CVmObjList::const_calc_hash(VMG_ const vm_val_t *self_val,
                                 const char *lst, int depth)
{
    size_t len;
    size_t i;
    uint hash;

    /* get and skip the length prefix */
    len = vmb_get_len(lst);
    
    /* calculate a hash combining the hash of each element in the list */
    for (hash = 0, i = 0 ; i < len ; ++i)
    {
        vm_val_t ele;
        
        /* re-translate in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = G_const_pool->get_ptr(self_val->val.ofs));

        /* get this element */
        get_element_const(lst, i, &ele);

        /* 
         *   Compute its hash value and add it into the total.  Note that
         *   even though we're recursively calculating the hash of an
         *   element, we don't need to increase the recursion depth, because
         *   it's impossible for a list to have cycles.
         *   
         *   (It's not possible for a list to have cycles because a list is
         *   always constructed with its contents, and can never be changed.
         *   This means that there's no possibility of storing a reference to
         *   the new list inside the list itself, or inside any other list
         *   the list refers to.  It *is* possible to put the reference to
         *   the new list in a mutable object to which the list refers, but
         *   in such cases, that mutable object will be capable of having
         *   cycles in its references, so it will be responsible for
         *   increasing the depth counter when it recurses.)  
         */
        hash += ele.calc_hash(vmg_ depth);
    }

    /* return the hash value */
    return hash;
}


/* ------------------------------------------------------------------------ */
/*
 *   Find a value in a list 
 */
int CVmObjList::find_in_list(VMG_ const vm_val_t *lst,
                             const vm_val_t *val, size_t *idxp)
{
    int cnt;
    int idx;
    
    /* get the length of the list */
    cnt = lst->ll_length(vmg0_);

    /* scan the list for the value */
    for (idx = 1 ; idx <= cnt ; ++idx)
    {
        vm_val_t curval;
        
        /* get this list element */
        lst->ll_index(vmg_ &curval, idx);

        /* compare this value to the one we're looking for */
        if (curval.equals(vmg_ val))
        {
            /* this is the one - set the return index */
            if (idxp != 0)
                *idxp = idx;

            /* indicate that we found the value */
            return TRUE;
        }
    }

    /* we didn't find the value */
    return FALSE;
}

/*
 *   Find the last match for a value in a list 
 */
int CVmObjList::find_last_in_list(VMG_ const vm_val_t *lst,
                                  const vm_val_t *val, size_t *idxp)
{
    int cnt;
    int idx;

    /* get the length of the list */
    cnt = lst->ll_length(vmg0_);

    /* scan the list for the value */
    for (idx = cnt ; idx > 0 ; --idx)
    {
        vm_val_t curval;

        /* get this list element */
        lst->ll_index(vmg_ &curval, idx);

        /* compare this value to the one we're looking for */
        if (curval.equals(vmg_ val))
        {
            /* this is the one - set the return index */
            if (idxp != 0)
                *idxp = idx;

            /* indicate that we found the value */
            return TRUE;
        }
    }

    /* we didn't find the value */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute the intersection of two lists.  Returns a new list with the
 *   elements that occur in both lists.  
 */
vm_obj_id_t CVmObjList::intersect(VMG_ const vm_val_t *l1,
                                  const vm_val_t *l2)
{
    int cnt1;
    int cnt2;
    int idx;
    vm_obj_id_t resobj;
    CVmObjList *reslst;
    size_t residx;

    /* get the lengths of the lists */
    cnt1 = l1->ll_length(vmg0_);
    cnt2 = l2->ll_length(vmg0_);

    /* if the first list is larger than the second, swap them */
    if (cnt1 > cnt2)
    {
        /* swap the vm_val_t pointers */
        const vm_val_t *tmp = l1;
        l1 = l2;
        l2 = tmp;

        /* forget the larger count; just copy the smaller list */
        cnt1 = cnt2;
    }

    /* 
     *   Allocate our result list.  The result list can't have any more
     *   elements in it than the shorter of the two lists, whose length is
     *   now in cnt1. 
     */
    resobj = create(vmg_ FALSE, cnt1);
    reslst = (CVmObjList *)vm_objp(vmg_ resobj);
    reslst->cons_clear();

    /* we haven't put any elements in the result list yet */
    residx = 0;

    /* 
     *   for each element in the first list, find the element in the
     *   second list 
     */
    for (idx = 1 ; idx <= cnt1 ; ++idx)
    {
        vm_val_t curval;
        
        /* get this element from the first list */
        l1->ll_index(vmg_ &curval, idx);
        
        /* find the element in the second list */
        if (find_in_list(vmg_ l2, &curval, 0))
        {
            /* we found it - copy it into the result list */
            reslst->cons_set_element(residx, &curval);

            /* count the new entry in the result list */
            ++residx;
        }
    }

    /* 
     *   set the actual result length, which might be shorter than the
     *   amount we allocated 
     */
    reslst->cons_set_len(residx);

    /* return the result list */
    return resobj;
}

/* ------------------------------------------------------------------------ */
/*
 *   Uniquify the list; modifies the list in place, so this can only be
 *   used during construction of a new list 
 */
void CVmObjList::cons_uniquify(VMG0_)
{
    size_t cnt;
    size_t src, dst;

    /* get the length of the list */
    cnt = vmb_get_len(ext_);

    /* loop through the list and look for repeated values */
    for (src = dst = 0 ; src < cnt ; ++src)
    {
        size_t idx;
        vm_val_t src_val;
        int found;

        /* 
         *   look for a copy of this source value already in the output
         *   list 
         */
        index_list(vmg_ &src_val, ext_, src + 1);
        for (idx = 0, found = FALSE ; idx < dst ; ++idx)
        {
            vm_val_t idx_val;

            /* get this value */
            index_list(vmg_ &idx_val, ext_, idx + 1);

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
            /* add it to the output list */
            cons_set_element(dst, &src_val);

            /* count it */
            ++dst;
        }
    }

    /* adjust the size of the result list */
    cons_set_len(dst);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create an iterator 
 */
void CVmObjList::new_iterator(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val)
{
    size_t len;
    
    /* get the number of elements in the list */
    len = vmb_get_len(self_val->get_as_list(vmg0_));

    /* 
     *   Set up a new indexed iterator object.  The first valid index for
     *   a list is always 1, and the last valid index is the same as the
     *   number of elements in the list. 
     */
    retval->set_obj(CVmObjIterIdx::create_for_coll(vmg_ self_val, 1, len));
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate a property 
 */
int CVmObjList::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
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
    return CVmObjCollection::get_prop(vmg_ prop, retval, self,
                                      source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate a property of a constant list value 
 */
int CVmObjList::const_get_prop(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val, const char *lst,
                               vm_prop_id_t prop, vm_obj_id_t *src_obj,
                               uint *argc)
{
    uint func_idx;

    /* presume no source object */
    *src_obj = VM_INVALID_OBJ;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    
    /* call the appropriate function */
    if ((*func_table_[func_idx])(vmg_ retval, self_val, lst, argc))
        return TRUE;

    /* 
     *   If this is a constant list (which is indicated by a non-object type
     *   'self'), try inheriting the default object interpretation, passing
     *   the constant list placeholder object for its type information.  
     */
    if (self_val->typ != VM_OBJ)
    {
        /* try going to our base class, CVmCollection */
        if (((CVmObjCollection *)vm_objp(vmg_ G_predef->const_lst_obj))
            ->const_get_coll_prop(vmg_ prop, retval, self_val, src_obj, argc))
            return TRUE;

        /* try going to our next base class, CVmObject */
        if (vm_objp(vmg_ G_predef->const_lst_obj)
            ->CVmObject::get_prop(vmg_ prop, retval, G_predef->const_lst_obj,
                                  src_obj, argc))
            return TRUE;
    }

    /* not handled */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - select a subset through a callback
 */
int CVmObjList::getp_subset(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                            const char *lst, uint *argc)
{
    uint orig_argc = (argc != 0 ? *argc : 0);
    const vm_val_t *func_val;
    size_t src;
    size_t dst;
    size_t cnt;
    char *new_lst;
    CVmObjList *new_lst_obj;
    static CVmNativeCodeDesc desc(1);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set up the recursive caller context */
    vm_rcdesc rc(vmg_ "List.subset", self_val, 1, G_stk->get(0), orig_argc);

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while allocating to protect from gc */
    G_stk->push(self_val);

    /* 
     *   Make a copy of our list for the return value.  The result value
     *   will be at most the same size as our current list; since we have
     *   no way of knowing exactly how large it will be, and since we
     *   don't want to run through the selection functions twice, we'll
     *   just allocate at the maximum size and leave it partially unused
     *   if we don't need all of the space.  By making a copy of the input
     *   list, we also can avoid worrying about whether the input list was
     *   a constant, and hence we don't have to worry about the
     *   possibility of constant page swapping.  
     */
    retval->set_obj(create(vmg_ FALSE, lst));

    /* get the return value list data */
    new_lst_obj = (CVmObjList *)vm_objp(vmg_ retval->val.obj);
    new_lst = new_lst_obj->ext_;

    /* get the length of the list */
    cnt = vmb_get_len(new_lst);

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
        
        /* 
         *   get this element (using a 1-based index), and push it as the
         *   callback's argument 
         */
        index_list(vmg_ &ele, new_lst, src + 1);
        G_stk->push(&ele);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, &rc, 0);

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
            /* include this element in the result */
            new_lst_obj->cons_set_element(dst, &ele);

            /* advance the output index */
            ++dst;
        }
    }

    /* 
     *   set the result list length to the number of elements we actually
     *   copied 
     */
    new_lst_obj->cons_set_len(dst);

    /* discard our gc protection (self, return value) and our arguments */
    G_stk->discard(3);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - map through a callback
 */
int CVmObjList::getp_map(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                         const char *lst, uint *argc)
{
    uint orig_argc = (argc != 0 ? *argc : 0);
    const vm_val_t *func_val;
    size_t cnt;
    size_t idx;
    char *new_lst;
    CVmObjList *new_lst_obj;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set up the recursive caller context */
    vm_rcdesc rc(vmg_ "List.mapAll", self_val, 2, G_stk->get(0), orig_argc);

    /* get the function pointer argument, but leave it on the stack */
    func_val = G_stk->get(0);

    /* push a self-reference while allocating to protect from gc */
    G_stk->push(self_val);

    /* 
     *   Make a copy of our list for the return value, since the result
     *   value is always the same size as our current list.  By making a
     *   copy of the input list, we also can avoid worrying about whether
     *   the input list was a constant, and hence we don't have to worry
     *   about the possibility of constant page swapping - we'll just
     *   update elements of the copy in-place.  
     */
    retval->set_obj(create(vmg_ FALSE, lst));

    /* get the return value list data */
    new_lst_obj = (CVmObjList *)vm_objp(vmg_ retval->val.obj);
    new_lst = new_lst_obj->ext_;

    /* get the length of the list */
    cnt = vmb_get_len(new_lst);

    /* 
     *   push a reference to the new list to protect it from the garbage
     *   collector, which could be invoked in the course of executing the
     *   user callback 
     */
    G_stk->push(retval);

    /*
     *   Go through each element of our list, and invoke the callback on
     *   each element.  Replace each element with the result of the
     *   callback.  
     */
    for (idx = 0 ; idx < cnt ; ++idx)
    {
        /* 
         *   get this element (using a 1-based index), and push it as the
         *   callback's argument 
         */
        index_and_push(vmg_ new_lst, idx + 1);
        
        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, &rc, 0);

        /* store the result in the list */
        new_lst_obj->cons_set_element(idx, G_interpreter->get_r0());
    }

    /* discard our gc protection (self, return value) and our arguments */
    G_stk->discard(3);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - length
 */
int CVmObjList::getp_len(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                         const char *lst, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the element count */
    retval->set_int(vmb_get_len(lst));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - sublist
 */
int CVmObjList::getp_sublist(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *lst, uint *in_argc)
{
    /* check arguments */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the original element count */
    int32_t old_cnt = vmb_get_len(lst);

    /* pop the starting index; negative counts from the end of the list */
    int32_t start = CVmBif::pop_long_val(vmg0_);
    if (start < 0)
        start += old_cnt + 1;
    
    /* 
     *   pop the length, if present; if not, use the current element
     *   count, which will ensure that we use all available elements of
     *   the sublist 
     */
    int32_t len = (argc >= 2 ? CVmBif::pop_long_val(vmg0_) : old_cnt);

    /* push the 'self' as protection from GC */
    G_stk->push(self_val);

    /* skip to the first element */
    lst += VMB_LEN;

    /* skip to the desired first element */
    int32_t new_cnt;
    if (start >= 1 && start <= old_cnt)
    {
        /* it's in range - skip to the desired first element */
        lst += (start - 1) * VMB_DATAHOLDER;
        new_cnt = old_cnt - (start - 1);
    }
    else
    {
        /* there's nothing left */
        new_cnt = 0;
    }

    /* 
     *   limit the result to the desired new count, if it's shorter than
     *   what we have left (we obviously can't give them more elements
     *   than we have remaining) 
     */
    if (len < 0)
        new_cnt += len;
    else if (len < new_cnt)
        new_cnt = len;

    /* make sure the new length is non-negative */
    if (new_cnt < 0)
        new_cnt = 0;

    /* create the new list */
    vm_obj_id_t obj = create(vmg_ FALSE, new_cnt);

    /* copy the elements */
    ((CVmObjList *)vm_objp(vmg_ obj))->cons_copy_data(0, lst, new_cnt);

    /* return the new object */
    retval->set_obj(obj);

    /* discard GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - intersect
 */
int CVmObjList::getp_intersect(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *argc)
{
    vm_val_t val2;
    vm_obj_id_t obj;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the second list, but leave it on the stack for GC protection */
    val2 = *G_stk->get(0);

    /* put myself on the stack for GC protection as well */
    G_stk->push(self_val);

    /* compute the intersection */
    obj = intersect(vmg_ self_val, &val2);

    /* discard the argument lists */
    G_stk->discard(2);

    /* return the new object */
    retval->set_obj(obj);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - indexOf
 */
int CVmObjList::getp_index_of(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                              const char *lst, uint *argc)
{
    vm_val_t subval;
    size_t idx;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the value to find */
    G_stk->pop(&subval);

    /* find the value in the list */
    if (find_in_list(vmg_ self_val, &subval, &idx))
    {
        /* found it - adjust to 1-based index for return */
        retval->set_int(idx);
    }
    else
    {
        /* didn't find it - return nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - car
 */
int CVmObjList::getp_car(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                         const char *lst, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   if the list has at least one element, return it; otherwise return
     *   nil 
     */
    if (vmb_get_len(lst) == 0)
    {
        /* no elements - return nil */
        retval->set_nil();
    }
    else
    {
        /* it has at least one element - return the first element */
        vmb_get_dh(lst + VMB_LEN, retval);
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - cdr
 */
int CVmObjList::getp_cdr(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                         const char *lst, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push a self-reference for GC protection */
    G_stk->push(self_val);

    /* 
     *   if the list has no elements, return nil; otherwise, return the
     *   sublist starting with the second element (thus return an empty
     *   list if the original list has only one element) 
     */
    if (vmb_get_len(lst) == 0)
    {
        /* no elements - return nil */
        retval->set_nil();
    }
    else
    {
        vm_obj_id_t obj;
        size_t new_cnt;

        /* reduce the list count by one */
        new_cnt = vmb_get_len(lst) - 1;

        /* skip past the first element */
        lst += VMB_LEN + VMB_DATAHOLDER;

        /* create the new list */
        obj = create(vmg_ FALSE, new_cnt);

        /* copy the elements */
        ((CVmObjList *)vm_objp(vmg_ obj))->cons_copy_data(0, lst, new_cnt);

        /* return the new object */
        retval->set_obj(obj);
    }

    /* discard the stack protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - indexWhich
 */
int CVmObjList::getp_index_which(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *lst, uint *argc)
{
    /* use the generic index-which routine, stepping forward */
    vm_rcdesc rc(vmg_ "List.indexWhich", self_val, 9, G_stk->get(0), argc);
    return gen_index_which(vmg_ retval, self_val, lst, argc, TRUE, &rc);
}

/*
 *   general index finder for indexWhich and lastIndexWhich - steps either
 *   forward or backward through the list 
 */
int CVmObjList::gen_index_which(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *argc,
                                int forward, vm_rcdesc *rc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    const vm_val_t *func_val = G_stk->get(0);

    /* push a self-reference while allocating to protect from gc */
    G_stk->push(self_val);

    /* get the length of the list */
    size_t cnt = vmb_get_len(lst);

    /* presume that we won't find any element that satisfies the condition */
    retval->set_nil();

    /* 
     *   start at either the first or last index, depending on which way
     *   we're stepping
     */
    size_t idx = (forward ? 1 : cnt);

    /*
     *   Go through each element of our list, and invoke the callback on
     *   the element.  Stop when we reach the first element that returns
     *   true, or when we run out of elements.  
     */
    for (;;)
    {
        /* if we're out of elements, stop now */
        if (forward ? idx > cnt : idx == 0)
            break;

        /* re-translate the list address in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = self_val->get_as_list(vmg0_);)

        /* get this element, and push it as the callback's argument */
        index_and_push(vmg_ lst, idx);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, rc, 0);

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
            /* it passed the test - return its index */
            retval->set_int(idx);

            /* no need to keep searching - we found what we're looking for */
            break;
        }

        /* advance to the next element */
        if (forward)
            ++idx;
        else
            --idx;
    }

    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - forEach
 */
int CVmObjList::getp_for_each(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *lst, uint *argc)
{
    /* use the generic forEach/forEachAssoc processor */
    vm_rcdesc rc(vmg_ "List.forEach", self_val, 10, G_stk->get(0), argc);
    return for_each_gen(vmg_ retval, self_val, lst, argc, FALSE, &rc);
}

/*
 *   property evaluator - forEachAssoc 
 */
int CVmObjList::getp_for_each_assoc(VMG_ vm_val_t *retval,
                                    const vm_val_t *self_val,
                                    const char *lst, uint *argc)
{
    /* use the generic forEach/forEachAssoc processor */
    vm_rcdesc rc(vmg_ "List.forEachAssoc", self_val, 25, G_stk->get(0), argc);
    return for_each_gen(vmg_ retval, self_val, lst, argc, TRUE, &rc);
}

/*
 *   General forEach processor - combines the functionality of forEach and
 *   forEachAssoc, using a flag to specify whether or not to pass the index
 *   of each element to the callback. 
 */
int CVmObjList::for_each_gen(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val,
                             const char *lst, uint *argc,
                             int send_idx_to_cb, vm_rcdesc *rc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    const vm_val_t *func_val = G_stk->get(0);

    /* push a self-reference while allocating to protect from gc */
    G_stk->push(self_val);

    /* get the length of the list */
    size_t cnt = vmb_get_len(lst);

    /* no return value */
    retval->set_nil();

    /* invoke the callback on each element */
    for (size_t idx = 1 ; idx <= cnt ; ++idx)
    {
        /* re-translate the list address in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = self_val->get_as_list(vmg0_);)

        /* 
         *   get this element (using a 1-based index) and push it as the
         *   callback's argument 
         */
        index_and_push(vmg_ lst, idx);

        /* push the index, if desired */
        if (send_idx_to_cb)
            G_stk->push()->set_int(idx);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, send_idx_to_cb ? 2 : 1,
                                     rc, 0);
    }

    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - valWhich
 */
int CVmObjList::getp_val_which(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *argc)
{
    /* get the index of the value using indexWhich */
    getp_index_which(vmg_ retval, self_val, lst, argc);

    /* if the return value is a valid index, get the value at the index */
    if (retval->typ == VM_INT)
    {
        int idx;
        
        /* re-translate the list address in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = self_val->get_as_list(vmg0_);)

        /* get the element as the return value */
        idx = (int)retval->val.intval;
        index_list(vmg_ retval, lst, idx);
    }
    
    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - lastIndexOf
 */
int CVmObjList::getp_last_index_of(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *lst, uint *argc)
{
    vm_val_t subval;
    size_t idx;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the value to find */
    G_stk->pop(&subval);

    /* find the value in the list */
    if (find_last_in_list(vmg_ self_val, &subval, &idx))
    {
        /* found it - set the return value*/
        retval->set_int(idx);
    }
    else
    {
        /* didn't find it - return nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - lastIndexWhich
 */
int CVmObjList::getp_last_index_which(VMG_ vm_val_t *retval,
                                      const vm_val_t *self_val,
                                      const char *lst, uint *argc)
{
    /* use the generic index-which routine, stepping backward */
    vm_rcdesc rc(vmg_ "List.lastIndexWhich", self_val, 13,
                 G_stk->get(0), argc);
    return gen_index_which(vmg_ retval, self_val, lst, argc, FALSE, &rc);
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - lastValWhich
 */
int CVmObjList::getp_last_val_which(VMG_ vm_val_t *retval,
                                    const vm_val_t *self_val,
                                    const char *lst, uint *argc)
{
    /* get the index of the value using lastIndexWhich */
    getp_last_index_which(vmg_ retval, self_val, lst, argc);

    /* if the return value is a valid index, get the value at the index */
    if (retval->typ == VM_INT)
    {
        int idx;
        
        /* re-translate the list address in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = self_val->get_as_list(vmg0_);)

        /* get the element as the return value */
        idx = (int)retval->val.intval;
        index_list(vmg_ retval, lst, idx);
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - countOf
 */
int CVmObjList::getp_count_of(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *lst, uint *argc)
{
    vm_val_t *val;
    size_t idx;
    size_t cnt;
    size_t val_cnt;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the value to find, but leave it on the stack for gc protection */
    val = G_stk->get(0);

    /* lave the self value on the stack for gc protection */
    G_stk->push(self_val);

    /* get the number of elements in the list */
    cnt = vmb_get_len(lst);

    /* scan the list and count the elements */
    for (idx = 0, val_cnt = 0 ; idx < cnt ; ++idx)
    {
        vm_val_t ele;
        
        /* re-translate the list address in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = self_val->get_as_list(vmg0_);)

        /* get this list element */
        vmb_get_dh(get_element_ptr_const(lst, idx), &ele);

        /* if it's the one we're looking for, count it */
        if (ele.equals(vmg_ val))
            ++val_cnt;
    }

    /* discard our gc protection */
    G_stk->discard(2);

    /* return the count */
    retval->set_int(val_cnt);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - countWhich
 */
int CVmObjList::getp_count_which(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *lst, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the function pointer argument, but leave it on the stack */
    const vm_val_t *func_val = G_stk->get(0);

    /* set up a native callback descriptor */
    vm_rcdesc rc(vmg_ "List.countWhich", self_val, 16, G_stk->get(0), argc);

    /* push a self-reference while allocating to protect from gc */
    G_stk->push(self_val);

    /* get the length of the list */
    size_t cnt = vmb_get_len(lst);

    /* no return value */
    retval->set_nil();

    /* invoke the callback on each element */
    size_t idx;
    int val_cnt;
    for (idx = 1, val_cnt = 0 ; idx <= cnt ; ++idx)
    {
        vm_val_t *val;

        /* re-translate the list address in case of swapping */
        VM_IF_SWAPPING_POOL(if (self_val->typ == VM_LIST)
            lst = self_val->get_as_list(vmg0_);)

        /* 
         *   get this element (using a 1-based index), and push it as the
         *   callback's argument 
         */
        index_and_push(vmg_ lst, idx);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ func_val, 1, &rc, 0);

        /* get the result from R0 */
        val = G_interpreter->get_r0();

        /* if the callback returned non-nil and non-zero, count it */
        if (val->typ == VM_NIL
            || (val->typ == VM_INT && val->val.intval == 0))
        {
            /* it's nil or zero - don't include it in the result */
        }
        else
        {
            /* count it */
            ++val_cnt;
        }
    }

    /* discard our gc protection (self) and our arguments */
    G_stk->discard(2);

    /* return the count */
    retval->set_int(val_cnt);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - getUnique
 */
int CVmObjList::getp_get_unique(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *argc)
{
    CVmObjList *new_lst_obj;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* put myself on the stack for GC protection */
    G_stk->push(self_val);

    /* 
     *   Make a copy of our list for the return value, since the result
     *   value will never be larger than the original list.  By making a
     *   copy of the input list, we also can avoid worrying about whether
     *   the input list was a constant, and hence we don't have to worry
     *   about the possibility of constant page swapping - we'll just
     *   update elements of the copy in-place.  
     */
    retval->set_obj(create(vmg_ FALSE, lst));

    /* get the return value list data */
    new_lst_obj = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* push a reference to the new list for gc protection */
    G_stk->push(retval);

    /* uniquify the list */
    new_lst_obj->cons_uniquify(vmg0_);

    /* discard the gc protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - appendUnique 
 */
int CVmObjList::getp_append_unique(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *lst, uint *argc)
{
    vm_val_t val2;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the second list, but leave it on the stack for GC protection */
    val2 = *G_stk->get(0);

    /* put myself on the stack for GC protection as well */
    G_stk->push(self_val);

    /* do the append */
    append_unique(vmg_ retval, self_val, &val2);

    /* discard the gc protection and arguments */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/*
 *   Append unique (internal interface) 
 */
void CVmObjList::append_unique(VMG_ vm_val_t *retval,
                               const vm_val_t *self, const vm_val_t *val2)
{
    const char *lst;
    int lst2_len;
    size_t lst_len;
    CVmObjList *new_lst;

    /* get my internal list pointer */
    if ((lst = self->get_as_list(vmg0_)) == 0)
        err_throw(VMERR_LIST_VAL_REQD);

    /* remember the length of my list */
    lst_len = vmb_get_len(lst);

    /* make sure the other value is indeed a list */
    if (!val2->is_listlike(vmg0_) || (lst2_len = val2->ll_length(vmg0_)) < 0)
        err_throw(VMERR_LIST_VAL_REQD);

    /*
     *   Create a new list for the return value.  Allocate space for the
     *   current list plus the list to be added - this is an upper bound,
     *   since the actual result list can be shorter 
     */
    retval->set_obj(create(vmg_ FALSE, lst_len + lst2_len));

    /* push a reference to the new list for gc protection */
    G_stk->push(retval);

    /* get the return value list data */
    new_lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* 
     *   copy the first list into the result list (including only the data
     *   elements, not the length prefix) 
     */
    new_lst->cons_copy_elements(0, lst);

    /* clear the remainder of the list */
    new_lst->cons_clear(lst_len, lst_len + lst2_len - 1);

    /* append the second list to the result list */
    for (int i = 1 ; i <= lst2_len ; ++i)
    {
        vm_val_t ele;
        val2->ll_index(vmg_ &ele, i);
        new_lst->cons_set_element(lst_len + i - 1, &ele);
    }

    /* make the list unique */
    new_lst->cons_uniquify(vmg0_);

    /* discard the gc protection */
    G_stk->discard();
}


/* ------------------------------------------------------------------------ */
/*
 *   General insertion routine - this is used to handle append, prepend, and
 *   insertAt property evaluators.  Inserts elements from the argument list
 *   at the given index, with zero indicating insertion before the first
 *   existing element.  
 */
void CVmObjList::insert_elements(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *lst, uint argc, int idx)
{
    /* number of temporary gc items we leave on the stack */
    const int stack_temp_cnt = 2;

    /* remember the length of my list */
    size_t lst_len = vmb_get_len(lst);

    /* the index must be in the range 0 to the number of elements */
    if (idx < 0 || (size_t)idx > lst_len)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* put myself on the stack for GC protection as well */
    G_stk->push(self_val);

    /*
     *   Create a new list for the return value.  Allocate space for the
     *   current list plus one new element for each argument.  
     */
    retval->set_obj(create(vmg_ FALSE, lst_len + argc));

    /* push a reference to the new list for gc protection */
    G_stk->push(retval);

    /* get the return value list data */
    CVmObjList *new_lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* get the original list data */
    lst = self_val->get_as_list(vmg0_);

    /* 
     *   Copy the first list into the result list (including only the data
     *   elements, not the length prefix).  Copy it in two pieces: first,
     *   copy the elements before the insertion point.  
     */
    if (idx != 0)
        new_lst->cons_copy_data(0, get_element_ptr_const(lst, 0), idx);

    /* second, copy the elements after the insertion point */
    if ((size_t)idx != lst_len)
        new_lst->cons_copy_data(idx + argc, get_element_ptr_const(lst, idx),
                                lst_len - idx);

    /* copy each argument into the proper position in the new list */
    for (uint i = 0 ; i < argc ; ++i)
    {
        /* 
         *   get a pointer to this argument value - the arguments are just
         *   after the temporary items we've pushed onto the stack 
         */
        const vm_val_t *argp = G_stk->get(stack_temp_cnt + i);
        
        /* copy the argument into the list */
        new_lst->cons_set_element((uint)idx + i, argp);
    }

    /* discard the gc protection and arguments */
    G_stk->discard(argc + stack_temp_cnt);
}


/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - append 
 */
int CVmObjList::getp_append(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val,
                            const char *lst, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* insert the element (there's just one) at the end of the list */
    insert_elements(vmg_ retval, self_val, lst, 1, vmb_get_len(lst));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - prepend 
 */
int CVmObjList::getp_prepend(VMG_ vm_val_t *retval,
                             const vm_val_t *self_val,
                             const char *lst, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* insert the element (there's just one) at the start of the list */
    insert_elements(vmg_ retval, self_val, lst, 1, 0);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - insert new elements
 */
int CVmObjList::getp_insert_at(VMG_ vm_val_t *retval,
                               const vm_val_t *self_val,
                               const char *lst, uint *in_argc)
{
    /* check arguments - we need at least two */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(2, 0, TRUE);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* pop the index value; adjust negative values */
    int idx = CVmBif::pop_int_val(vmg0_);
    if (idx <= 0)
        idx += vmb_get_len(lst) + 1;

    /* adjust to zero-based indexing */
    idx -= 1;

    /* 
     *   Insert the element (there's just one) at the start of the list.
     *   Note that we must decrement the argument count we got, since we
     *   already took off the first argument (the index value). 
     */
    insert_elements(vmg_ retval, self_val, lst, argc - 1, idx);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   General property evaluator for removing a range of elements - this
 *   handles removeElementAt and removeRange. 
 */
void CVmObjList::remove_range(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *lst, int start_idx, int del_cnt)
{
    size_t lst_len;
    CVmObjList *new_lst;

    /* push myself onto the stack for GC protection */
    G_stk->push(self_val);

    /* get the original list length */
    lst_len = vmb_get_len(lst);
    
    /* 
     *   allocate a new list with space for the original list minus the
     *   elements to be deleted 
     */
    retval->set_obj(create(vmg_ FALSE, lst_len - del_cnt));

    /* push a reference to teh new list for gc protection */
    G_stk->push(retval);

    /* get the return value list data */
    new_lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* get the original list data as well */
    lst = self_val->get_as_list(vmg0_);

    /* 
     *   copy elements from the original list up to the first item to be
     *   removed 
     */
    if (start_idx != 0)
        new_lst->cons_copy_data(0, get_element_ptr_const(lst, 0), start_idx);

    /* 
     *   copy elements of the original list following the last item to be
     *   removed 
     */
    if ((size_t)(start_idx + del_cnt) < lst_len)
        new_lst->
            cons_copy_data(start_idx,
                           get_element_ptr_const(lst, start_idx + del_cnt),
                           lst_len - (start_idx + del_cnt));

    /* discard the gc protection */
    G_stk->discard(2);
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - remove the element at the given index
 */
int CVmObjList::getp_remove_element_at(VMG_ vm_val_t *retval,
                                       const vm_val_t *self_val,
                                       const char *lst, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the index value, and adjust for negative values */
    int idx = CVmBif::pop_int_val(vmg0_);
    if (idx < 0)
        idx += vmb_get_len(lst) + 1;

    /* adjust to zero-based indexing */
    --idx;

    /* make sure it's in range - it must refer to a valid element */
    if (idx < 0 || idx >= (int)vmb_get_len(lst))
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* remove one element at the given index */
    remove_range(vmg_ retval, self_val, lst, idx, 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - remove the element at the given index 
 */
int CVmObjList::getp_remove_range(VMG_ vm_val_t *retval,
                                  const vm_val_t *self_val,
                                  const char *lst, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the starting and ending index values */
    int start_idx = CVmBif::pop_int_val(vmg0_);
    int end_idx = CVmBif::pop_int_val(vmg0_);

    /* negative index values count from the end of the list */
    if (start_idx < 0)
        start_idx += vmb_get_len(lst) + 1;
    if (end_idx < 0)
        end_idx += vmb_get_len(lst) + 1;

    /* adjust to zero-based indexing */
    --start_idx;
    --end_idx;

    /* 
     *   make sure the index values are in range - both must refer to valid
     *   elements, and the ending index must be at least as high as the
     *   starting index 
     */
    if (start_idx < 0 || (size_t)start_idx >= vmb_get_len(lst)
          || end_idx < 0 || (size_t)end_idx >= vmb_get_len(lst)
          || end_idx < start_idx)
        err_throw(VMERR_INDEX_OUT_OF_RANGE);

    /* remove the specified elements */
    remove_range(vmg_ retval, self_val, lst, start_idx,
                 end_idx - start_idx + 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   sorter for list data 
 */
class CVmQSortList: public CVmQSortVal
{
public:
    CVmQSortList()
    {
        lst_ = 0;
    }
    
    /* get an element */
    void get_ele(VMG_ size_t idx, vm_val_t *val)
    {
        vmb_get_dh(get_ele_ptr(idx), val);
    }

    /* set an element */
    void set_ele(VMG_ size_t idx, const vm_val_t *val)
    {
        vmb_put_dh(get_ele_ptr(idx), val);
    }

    /* get an element pointer */
    char *get_ele_ptr(size_t idx)
        { return lst_ + VMB_LEN + (idx * VMB_DATAHOLDER); }

    /* our list data */
    char *lst_;
};

/*
 *   property evaluator - sort
 */
int CVmObjList::getp_sort(VMG_ vm_val_t *retval,
                          const vm_val_t *self_val,
                          const char *lst, uint *in_argc)
{
    size_t lst_len;
    CVmObjList *new_lst;
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    CVmQSortList sorter;
    static CVmNativeCodeDesc desc(0, 2);

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* remember the length of my list */
    lst_len = vmb_get_len(lst);

    /* if we have an 'descending' flag, note it */
    if (argc >= 1)
        sorter.descending_ = (G_stk->get(0)->typ != VM_NIL);

    /* 
     *   if we have a comparison function, note it, but leave it on the
     *   stack for gc protection 
     */
    if (argc >= 2)
    {
        /* get the function */
        sorter.compare_fn_ = *G_stk->get(1);

        /* initialize the native caller context */
        sorter.rc.init(vmg_ "List.sort", self_val, 20, G_stk->get(0), argc);
    }

    /* put myself on the stack for GC protection as well */
    G_stk->push(self_val);

    /* create a copy of the list as the return value */
    retval->set_obj(create(vmg_ FALSE, lst));

    /* push a reference to the new list for gc protection */
    G_stk->push(retval);

    /* get the return value list data */
    new_lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* set the list pointer in the sorter */
    sorter.lst_ = new_lst->ext_;

    /* sort the new list if it has any elements */
    if (lst_len != 0)
        sorter.sort(vmg_ 0, lst_len - 1);

    /* discard the gc protection and arguments */
    G_stk->discard(2 + argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - splice
 */
int CVmObjList::getp_splice(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val,
                            const char *lst, uint *in_argc)
{
    /* check arguments */
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    static CVmNativeCodeDesc desc(2, 0, TRUE);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* remember the length of my list */
    int lst_cnt = vmb_get_len(lst);

    /* retrieve the starting index and deletion length arguments */
    int start_idx = CVmBif::pop_int_val(vmg0_);
    int del_cnt = CVmBif::pop_int_val(vmg0_);

    /* a negative starting index counts from the end of the list */
    if (start_idx <= 0)
        start_idx += lst_cnt + 1;

    /* adjust to zero-based indexing */
    --start_idx;

    /* make sure the values are in range */
    start_idx = (start_idx < 0 ? 0 :
                 start_idx > lst_cnt ? lst_cnt :
                 start_idx);
    del_cnt = (del_cnt < 0 ? 0 :
               del_cnt > lst_cnt - start_idx ? lst_cnt - start_idx :
               del_cnt);

    /* figure the insertion size: each argument is a value to insert */
    int ins_cnt = argc - 2;

    /* if we're making any changes, build the new list */
    if (del_cnt != 0 || ins_cnt != 0)
    {
        /* put myself on the stack for GC protection */
        G_stk->push(self_val);

        /* create a list to store the return value */
        retval->set_obj(create(vmg_ FALSE, lst_cnt + ins_cnt - del_cnt));
        CVmObjList *new_lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

        /* push a reference to the new list for gc protection */
        G_stk->push(retval);
        
        /* copy elements from the original up to the starting index */
        int dst = 0;
        if (start_idx > 0)
        {
            new_lst->cons_copy_data(0, lst + VMB_LEN, start_idx);
            dst = start_idx;
        }
        
        /* 
         *   Add the inserted items from the arguments.  We popped our first
         *   two arguments, but then we pushed two items for gc protection,
         *   so conveniently the inserted items are just args 2..argc-1 
         */
        for (uint i = 2 ; i < argc ; ++i)
            new_lst->cons_set_element(dst++, G_stk->get(i));
        
        /* copy the remaining elements after the deleted segment, if any */
        if (lst_cnt > start_idx + del_cnt)
        {
            new_lst->cons_copy_data(
                dst, get_element_ptr_const(lst, start_idx + del_cnt),
                lst_cnt - (start_idx + del_cnt));
        }   
        
        /* 
         *   Discard the gc protection and insertion arguments.  We added two
         *   elements for gc protection, and popped two arguments, so the net
         *   is the original argument count.  
         */
        G_stk->discard(argc);
    }
    else
    {
        /* 
         *   There's nothing to do - just return the original list.  (There's
         *   nothing to discard from the stack: we have no more arguments,
         *   since ins_cnt is zero, and we haven't pushed any gc protection
         *   elements.) 
         */
        *retval = *self_val;
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - join.  This is a generic routine that can be called
 *   with any list-like object.  
 */
int CVmObjList::getp_join(VMG_ vm_val_t *retval,
                          const vm_val_t *self_val,
                          const char *lst, uint *in_argc)
{
    /* check arguments */
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* make sure the 'separator' argument is valid */
    int sep_len = 0;
    const char *sep = 0;
    if (argc >= 1 && G_stk->get(0)->typ != VM_NIL)
    {
        /* it's there - make sure it's a string */
        if ((sep = G_stk->get(0)->get_as_string(vmg0_)) == 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* note its length, and get a pointer to its string bytes */
        sep_len = vmb_get_len(sep);
        sep += VMB_LEN;
    }

    /* do the join */
    join(vmg_ retval, self_val, sep, sep_len);

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/*
 *   Join list elements into a string 
 */
void CVmObjList::join(VMG_ vm_val_t *retval,
                      const vm_val_t *self_val,
                      const char *sep, size_t sep_len)
{
    /* push 'self' for gc protection */
    G_stk->push(self_val);

    /* 
     *   Do a first pass through the list to measure the size we'll need.
     *   For string elements we'll know exactly what we need; for
     *   non-strings, make a guess, and we'll expand the buffer later if
     *   needed.  
     */
    vm_val_t ele;
    size_t alo_len = 0;
    int i, lst_len = self_val->ll_length(vmg0_);
    for (i = 1 ; i <= lst_len ; ++i)
    {
        /* get this element */
        self_val->ll_index(vmg_ &ele, i);

        /* if it's a string, get its length; for other types, make a guess */
        const char *es = ele.get_as_string(vmg0_);
        if (es != 0)
        {
            /* it's a string - add its length to the running total */
            alo_len += vmb_get_len(es);
        }
        else
        {
            /* for other types, guess based on the type */
            switch (ele.typ)
            {
            case VM_INT:
                /* integers - leave room for 10 characters */
                alo_len += 10;
                break;

            case VM_OBJ:
                /* 
                 *   object conversion could take almost any amount of space;
                 *   make a wild guess for now 
                 */
                alo_len += 128;
                break;

            default:
                /* assume nothing for other types */
                break;
            }
        }
    }

    /* add in the space needed for the separators */
    if (lst_len > 0)
        alo_len += sep_len * (lst_len - 1);

    /* create a string with the allocated size */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, alo_len));
    CVmObjString *strp = (CVmObjString *)vm_objp(vmg_ retval->val.obj);

    /* get a write pointer to the buffer */
    char *dst = strp->cons_get_buf();

    /* push it for gc protection */
    G_stk->push(retval);

    /* build the string */
    for (i = 1 ; i <= lst_len ; ++i)
    {
        char buf[128];
        vm_val_t tmp_str;

        /* get this element */
        self_val->ll_index(vmg_ &ele, i);

        /* assume we won't need a temporary string */
        tmp_str.set_nil();

        /* try getting it as a string directly */
        const char *es = ele.get_as_string(vmg0_);
        size_t es_len;
        if (es != 0)
        {
            /* it's a string - get its length and buffer */
            es_len = vmb_get_len(es);
            es += VMB_LEN;
        }
        else
        {
            /* for other types, do the implied string conversion */
            switch (ele.typ)
            {
            case VM_INT:
                /* integer - format in decimal */
                sprintf(buf, "%ld", (long)ele.val.intval);
                es = buf;
                es_len = strlen(es);
                break;

            case VM_NIL:
                /* nil - use an empty string */
                es = "";
                es_len = 0;
                break;

            default:
                /* for anything else, see what casting comes up with */
                if ((es = ele.cast_to_string(vmg_ &tmp_str)) == 0)
                    err_throw(VMERR_BAD_TYPE_BIF);

                /* get the length and buffer */
                es_len = vmb_get_len(es);
                es += VMB_LEN;
                break;
            }
        }

        /* push the temp string (if any) for gc protection */
        G_stk->push(&tmp_str);

        /* append the current string, making sure we have enough space */
        dst = strp->cons_ensure_space(vmg_ dst, es_len, 256);
        memcpy(dst, es, es_len);
        dst += es_len;

        /* if there's another element, append the separator */
        if (i < lst_len && sep_len != 0)
        {
            dst = strp->cons_ensure_space(vmg_ dst, sep_len, 32);
            memcpy(dst, sep, sep_len);
            dst += sep_len;
        }

        /* discard the temp string */
        G_stk->discard(1);
    }

    /* set the string to its final length */
    strp->cons_shrink_buffer(vmg_ dst);

    /* remove gc protection */
    G_stk->discard(2);
}

/*
 *   explicit string conversion, using toString() semantics 
 */
const char *CVmObjList::list_to_string(
    VMG_ vm_val_t *retval, const vm_val_t *self, int radix, int flags)
{
    /* count our elements */
    int n = self->ll_length(vmg0_);

    /* create a list with the same number of elements */
    vm_val_t newlstval;
    newlstval.set_obj(create(vmg_ FALSE, n));
    CVmObjList *newlst = vm_objid_cast(CVmObjList, newlstval.val.obj);
    newlst->cons_clear();

    /* push the new list for gc protection */
    G_stk->push(&newlstval);

    /* set up the new list as self.mapAll({x: toString(x)}) */
    for (int i = 0 ; i < n ; ++i)
    {
        /* get this element of the old list */
        vm_val_t ele;
        self->ll_index(vmg_ &ele, i+1);

        /* convert it to a string */
        vm_val_t newele;
        CVmBifTADS::toString(vmg_ &newele, &ele, radix, flags);

        /* store the result in the new list */
        newlst->cons_set_element(i, &newele);
    }

    /* convert the new list to a string using the basic join with commas */
    join(vmg_ retval, &newlstval, ",", 1);

    /* discard gc protection */
    G_stk->discard(1);

    /* return the new string */
    return retval->get_as_string(vmg0_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - generate (static property)
 */
int CVmObjList::static_getp_generate(VMG_ vm_val_t *retval, uint *in_argc)
{
    /* check arguments */
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the callback function argument */
    vm_val_t *func = G_stk->get(0);

    /* make sure it's a function; set up a header pointer if it is */
    CVmFuncPtr funcdesc;
    if (!funcdesc.set(vmg_ func))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the count */
    int32_t cnt = G_stk->get(1)->num_to_int(vmg0_);

    /* make sure it's not negative */
    if (cnt < 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* create the list */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, cnt));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* push the list value for gc protection */
    G_stk->push(retval);

    /* zero the element count, so that the gc doesn't deref garbage */
    lst->cons_set_len(0);

    /* find out how many arguments the function wants */
    int fargc = funcdesc.is_varargs() ? -1 : funcdesc.get_max_argc();

    /* set up our recursive call descriptor */
    vm_rcdesc rc(vmg_ "List.generate",
                 CVmObjList::metaclass_reg_->get_class_obj(vmg0_),
                 PROPIDX_generate, func, argc);

    /* generate the elements */
    for (int i = 0 ; i < cnt ; ++i)
    {
        /* push the callback arguments */
        const int pushed_argc = 1;
        G_stk->push()->set_int(i+1);

        /* adjust the arguments for what the callback actually wants */
        int sent_argc = fargc < 0 || fargc > pushed_argc ? pushed_argc : fargc;

        /* call the callback */
        G_interpreter->call_func_ptr(vmg_ func, sent_argc, &rc, 0);

        /* discard excess arguments */
        G_stk->discard(pushed_argc - sent_argc);

        /* add the result to the list */
        lst->cons_set_len(i+1);
        lst->cons_set_element(i, G_interpreter->get_r0());
    }

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator: get the index of the element with the minimum value 
 */
int CVmObjList::getp_indexOfMin(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *in_argc)
{
    /* do the general calculation to select the minimum value ("-1") */
    vm_rcdesc rc(vmg_ "List.indexOfMin", self_val, 29, G_stk->get(0),
                 in_argc != 0 ? *in_argc : 0);
    return get_minmax(vmg_ retval, self_val, lst, in_argc, &rc, -1, TRUE);
}

/*
 *   Property evaluator: get the value of the element with the minimum value 
 */
int CVmObjList::getp_minVal(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val,
                            const char *lst, uint *in_argc)
{
    /* do the general calculation to select the minimum value ("-1") */
    vm_rcdesc rc(vmg_ "List.minVal", self_val, 30, G_stk->get(0),
                 in_argc != 0 ? *in_argc : 0);
    return get_minmax(vmg_ retval, self_val, lst, in_argc, &rc, -1, FALSE);
}

/*
 *   Property evaluator: get the index of the element with the maximum value 
 */
int CVmObjList::getp_indexOfMax(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *lst, uint *in_argc)
{
    /* do the general calculation to select the maximum value ("+1") */
    vm_rcdesc rc(vmg_ "List.indexOfMax", self_val, 31, G_stk->get(0),
                 in_argc != 0 ? *in_argc : 0);
    return get_minmax(vmg_ retval, self_val, lst, in_argc, &rc, 1, TRUE);
}

/*
 *   Property evaluator: get the value of the element with the maximum value 
 */
int CVmObjList::getp_maxVal(VMG_ vm_val_t *retval,
                            const vm_val_t *self_val,
                            const char *lst, uint *in_argc)
{
    /* do the general calculation to select the maximum value ("+1") */
    vm_rcdesc rc(vmg_ "List.maxVal", self_val, 32, G_stk->get(0),
                 in_argc != 0 ? *in_argc : 0);
    return get_minmax(vmg_ retval, self_val, lst, in_argc, &rc, 1, FALSE);
}

/*
 *   General handling for minIndex, minVal, maxIndex, and maxVal.  This finds
 *   the highest/lowest value in the list, optionally mapped through a
 *   callback function.  Returns the winning element's index and value.
 *   
 *   'sense' is the sense of the search: -1 for minimum, 1 for maximum.
 *   'want_index' is true if 'retval' should be set to the index of the
 *   winning element, false if 'retval' should be set to the winning
 *   element's value.  
 */
int CVmObjList::get_minmax(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *lst, uint *in_argc,
                           const vm_rcdesc *rc, int sense, int want_index)
{
    /* check arguments */
    uint argc = (in_argc == 0 ? 0 : *in_argc);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* make sure the 'func' argument is valid */
    const vm_val_t *cb = 0;
    if (argc >= 1)
    {
        cb = G_stk->get(0);
        if (!cb->is_func_ptr(vmg0_))
            err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* push 'self' for gc protection */
    G_stk->push(self_val);

    /* if there are no elements, this is an error */
    int cnt = vmb_get_len(lst);
    if (cnt == 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* we don't have a winner yet */
    int winner_idx = 0;
    vm_val_t winner;

    /* run through the list */
    for (int i = 0 ; i < cnt ; ++i)
    {
        /* get this element */
        vm_val_t ele;
        get_element_const(lst, i, &ele);

        /* if there's a callback, invoke it */
        if (cb != 0)
        {
            /* invoke the callback with the element value as the argument */
            G_stk->push(&ele);
            G_interpreter->call_func_ptr(vmg_ cb, 1, rc, 0);

            /* use the result as the comparison value */
            ele = *G_interpreter->get_r0();
        }

        /* compare it to the winner so far, and keep the lower value */
        if (i == 0)
        {
            /* it's the first element, so it's the default winner so far */
            winner_idx = i;
            winner = ele;
        }
        else
        {
            /* compare it to the winner so far */
            int cmp = ele.compare_to(vmg_ &winner);

            /* keep it if it's the min/max so far */
            if ((sense > 0 && cmp > 0) || (sense < 0 && cmp < 0))
            {
                winner_idx = i;
                winner = ele;
            }
        }
    }

    /* remove gc protection and arguments */
    G_stk->discard(argc + 1);
    
    /* return the winning index (note: 1-based) or value, as desired */
    if (want_index)
        retval->set_int(winner_idx + 1);
    else
        *retval = winner;
        
    /* handled */
    return TRUE;
}



/* ------------------------------------------------------------------------ */
/*
 *   Constant-pool list object 
 */

/*
 *   create 
 */
vm_obj_id_t CVmObjListConst::create(VMG_ const char *const_ptr)
{
    /* create our new ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* create our list object, pointing directly to the constant pool */
    new (vmg_ id) CVmObjListConst(vmg_ const_ptr);

    /* return the new ID */
    return id;
}

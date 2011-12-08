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
  vmiter.cpp - T3 iterator metaclass
Function
  
Notes
  
Modified
  04/22/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmiter.h"
#include "vmglob.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmundo.h"
#include "vmlst.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   Base Iterator metaclass 
 */

/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassIter iter_metaclass_reg_obj;
CVmMetaclass *CVmObjIter::metaclass_reg_ = &iter_metaclass_reg_obj;

/* function table */
int (CVmObjIter::
     *CVmObjIter::func_table_[])(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc) =
{
    &CVmObjIter::getp_undef,
    &CVmObjIter::getp_get_next,
    &CVmObjIter::getp_is_next_avail,
    &CVmObjIter::getp_reset_iter,
    &CVmObjIter::getp_get_cur_key,
    &CVmObjIter::getp_get_cur_val
};

/* 
 *   get a property 
 */
int CVmObjIter::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                         vm_obj_id_t self, vm_obj_id_t *source_obj,
                         uint *argc)
{
    /* translate the property into a function vector index */
    uint func_idx = G_meta_table->prop_to_vector_idx(
        metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling from the base object class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Indexed Iterator metaclass 
 */

/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassIterIdx idx_metaclass_reg_obj;
CVmMetaclass *CVmObjIterIdx::metaclass_reg_ = &idx_metaclass_reg_obj;

/* create a list with no initial contents */
vm_obj_id_t CVmObjIterIdx::create_for_coll(VMG_ const vm_val_t *coll,
                                           long first_valid_index,
                                           long last_valid_index)
{
    vm_obj_id_t id;

    /* push the collection object reference for gc protection */
    G_stk->push(coll);

    /* create a non-root-set object */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* instantiate the iterator */
    new (vmg_ id) CVmObjIterIdx(vmg_ coll, first_valid_index,
                                last_valid_index);

    /* done with the gc protection */
    G_stk->discard();

    /* return the new object's ID */
    return id;
}

/*
 *   constructor 
 */
CVmObjIterIdx::CVmObjIterIdx(VMG_ const vm_val_t *coll,
                             long first_valid_index, long last_valid_index)
{
    /* allocate space for our extension data */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(VMOBJITERIDX_EXT_SIZE, this);

    /* save the collection value */
    vmb_put_dh(ext_, coll);

    /* 
     *   set the current index to the first index minus 1, so that we start
     *   with the first element when we make our first call to getNext() 
     */
    set_cur_index_no_undo(first_valid_index - 1);

    /* remember the first and last valid index values */
    set_first_valid(first_valid_index);
    set_last_valid(last_valid_index);

    /* clear the flags */
    set_flags(0);
}

/*
 *   notify of deletion 
 */
void CVmObjIterIdx::notify_delete(VMG_ int)
{
    /* free our extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/*
 *   property evaluator - get the current item's value
 */
int CVmObjIterIdx::getp_get_cur_val(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                    uint *argc)
{
    long idx;
    static CVmNativeCodeDesc desc(0);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the current index */
    idx = get_cur_index();

    /* if the current value is out of range, throw an error */
    if (idx < 1 || idx > get_last_valid())
        err_throw(VMERR_OUT_OF_RANGE);

    /* retrieve the value for this index */
    get_indexed_val(vmg_ idx, retval);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - get the current item's key
 */
int CVmObjIterIdx::getp_get_cur_key(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                    uint *argc)
{
    long idx;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the current index */
    idx = get_cur_index();

    /* if the current value is out of range, throw an error */
    if (idx < 1 || idx > get_last_valid())
        err_throw(VMERR_OUT_OF_RANGE);

    /* return the index */
    retval->set_int(idx);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - get the next item
 */
int CVmObjIterIdx::getp_get_next(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the next index, which is one higher than the current index */
    long idx = get_cur_index() + 1;

    /* if the current value is out of range, throw an error */
    if (idx > get_last_valid())
        err_throw(VMERR_OUT_OF_RANGE);

    /* retrieve the value */
    get_indexed_val(vmg_ idx, retval);

    /* save the current index in our internal state */
    set_cur_index(vmg_ self, idx);

    /* handled */
    return TRUE;
}

/*
 *   get the next iteration item 
 */
int CVmObjIterIdx::iter_next(VMG_ vm_obj_id_t self, vm_val_t *val)
{
    /* get the next index, which is one higher than the current index */
    long idx = get_cur_index() + 1;

    /* if the current value is out of range, throw an error */
    if (idx <= get_last_valid())
    {
        /* retrieve the value */
        get_indexed_val(vmg_ idx, val);

        /* save the current index in our internal state */
        set_cur_index(vmg_ self, idx);

        /* we got a value */
        return TRUE;
    }
    else
    {
        /* no more values available */
        return FALSE;
    }
}


/*
 *   Retrieve an indexed value from my collection 
 */
void CVmObjIterIdx::get_indexed_val(VMG_ long idx, vm_val_t *retval)
{
    vm_val_t coll;
    vm_val_t idx_val;

    /* get my collection value and the next index value */
    get_coll_val(&coll);

    /* check to see if we have an object or a constant list */
    switch(coll.typ)
    {
    case VM_LIST:
        /* it's a constant list - index the constant value */
        CVmObjList::index_list(vmg_ retval, coll.get_as_list(vmg0_), idx);
        break;

    case VM_OBJ:
        /* it's an object - index it */
        idx_val.set_int(idx);
        vm_objp(vmg_ coll.val.obj)
            ->index_val_ov(vmg_ retval, coll.val.obj, &idx_val);
        break;

    default:
        /* 
         *   Anything else is an error.  We really should never be able to
         *   get here, since the only way to instantiate an iterator should
         *   be via the collection object's createIter() method; so if we
         *   get here it must be an internal error, hence we could probably
         *   assert failure here.  Nonetheless, just throw an error, since
         *   this will make for a more pleasant indication of the problem.  
         */
        err_throw(VMERR_CANNOT_INDEX_TYPE);
        break;
    }
}

/* 
 *   property evaluator - is next value available? 
 */
int CVmObjIterIdx::getp_is_next_avail(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                      uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   if the current index plus one is less than or equal to the last
     *   valid index, another item is available 
     */
    retval->set_logical(get_cur_index() + 1 <= get_last_valid());

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - reset to first item 
 */
int CVmObjIterIdx::getp_reset_iter(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set the index to the first valid index minus one */
    set_cur_index(vmg_ self, get_first_valid() - 1);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   Set the current index value, saving undo if necessary 
 */
void CVmObjIterIdx::set_cur_index(VMG_ vm_obj_id_t self, long idx)
{
    /* save undo if necessary */
    if (G_undo != 0 && !(get_flags() & VMOBJITERIDX_UNDO))
    {
        vm_val_t dummy;
        
        /* 
         *   Add the undo record.  Note that the only information we need
         *   to store is the index value, so we can store this as the key
         *   value - supply a dummy payload, since we have no use for it. 
         */
        dummy.set_nil();
        G_undo->add_new_record_int_key(vmg_ self, get_cur_index(), &dummy);

        /* 
         *   set the undo bit so we don't save redundant undo for this
         *   savepoint 
         */
        set_flags(get_flags() | VMOBJITERIDX_UNDO);
    }

    /* set the index */
    set_cur_index_no_undo(idx);
}

/*
 *   apply undo 
 */
void CVmObjIterIdx::apply_undo(VMG_ CVmUndoRecord *rec)
{
    /* 
     *   the integer key in the undo record is my saved index value (and
     *   is the only thing in an indexed iterator that can ever change) 
     */
    set_cur_index_no_undo(rec->id.intval);
}

/*
 *   mark references 
 */
void CVmObjIterIdx::mark_refs(VMG_ uint state)
{
    vm_val_t coll;
    
    /* if my collection is an object, mark it as referenced */
    get_coll_val(&coll);
    if (coll.typ == VM_OBJ)
        G_obj_table->mark_all_refs(coll.val.obj, state);
}

/*
 *   load from an image file 
 */
void CVmObjIterIdx::load_from_image(VMG_ vm_obj_id_t self,
                                    const char *ptr, size_t siz)
{
    /* if we already have memory allocated, free it */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* 
     *   Allocate a new extension.  Make sure it's at least as large as
     *   the current standard extension size.  
     */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(siz < VMOBJITERIDX_EXT_SIZE
                       ? VMOBJITERIDX_EXT_SIZE : siz, this);

    /* copy the image data to our extension */
    memcpy(ext_, ptr, siz);

    /* clear the undo flag */
    set_flags(get_flags() & ~VMOBJITERIDX_UNDO);

    /* save our image data pointer, so we can use it for reloading */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from an image file 
 */
void CVmObjIterIdx::reload_from_image(VMG_ vm_obj_id_t,
                                      const char *ptr, size_t siz)
{
    /* copy the image data over our data */
    memcpy(ext_, ptr, siz);

    /* clear the undo flag */
    set_flags(get_flags() & ~VMOBJITERIDX_UNDO);
}


/*
 *   save 
 */
void CVmObjIterIdx::save_to_file(VMG_ CVmFile *fp)
{
    /* write my extension */
    fp->write_bytes(ext_, VMOBJITERIDX_EXT_SIZE);
}

/*
 *   restore 
 */
void CVmObjIterIdx::restore_from_file(VMG_ vm_obj_id_t,
                                      CVmFile *fp, CVmObjFixup *fixups)
{
    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* allocate a new extension */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(VMOBJITERIDX_EXT_SIZE, this);

    /* read my extension */
    fp->read_bytes(ext_, VMOBJITERIDX_EXT_SIZE);

    /* fix up my collection object reference */
    fixups->fix_dh(vmg_ ext_);
}


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
  vmcoll.cpp - collection metaclass
Function
  
Notes
  
Modified
  04/22/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmcoll.h"
#include "vmglob.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics
 */

/* metaclass registration object */
static CVmMetaclassCollection metaclass_reg_obj;
CVmMetaclass *CVmObjCollection::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjCollection::
     *CVmObjCollection::func_table_[])(VMG_ vm_val_t *retval,
                                       const vm_val_t *self_val,
                                       uint *argc) =
{
    &CVmObjCollection::getp_undef,
    &CVmObjCollection::getp_create_iter,
    &CVmObjCollection::getp_create_live_iter
};



/* ------------------------------------------------------------------------ */
/*
 *   Get a property 
 */
int CVmObjCollection::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                               vm_obj_id_t self, vm_obj_id_t *source_obj,
                               uint *argc)
{
    vm_val_t self_val;

    /* set up the 'self' value */
    self_val.set_obj(self);

    /* use the constant collection version */
    if (const_get_coll_prop(vmg_ prop, retval, &self_val, source_obj, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/*
 *   Get a property of a constant value
 */
int CVmObjCollection::const_get_coll_prop(VMG_ vm_prop_id_t prop,
                                          vm_val_t *retval,
                                          const vm_val_t *self_val,
                                          vm_obj_id_t *src_obj,
                                          uint *argc)
{
    uint func_idx;

    /* presume no source object */
    *src_obj = VM_INVALID_OBJ;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ retval, self_val, argc))
        return TRUE;

    /* not found */
    return FALSE;
}

/*
 *   Create an iterator 
 */
int CVmObjCollection::getp_create_iter(VMG_ vm_val_t *retval,
                                       const vm_val_t *self_val, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push a self-reference for gc protection */
    G_stk->push(self_val);

    /* create the iterator */
    new_iterator(vmg_ retval, self_val);

    /* discard the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Create a live iterator 
 */
int CVmObjCollection::getp_create_live_iter(VMG_ vm_val_t *retval,
                                            const vm_val_t *self_val,
                                            uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push a self-reference for gc protection */
    G_stk->push(self_val);

    /* create the "live" iterator */
    new_live_iterator(vmg_ retval, self_val);

    /* discard the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}


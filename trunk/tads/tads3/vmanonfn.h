/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmanonfn.h - anonymous function metaclass
Function
  The anonymous function metaclass is a subclass of the Vector metaclass
  that provides encapsulation of an anonymous function and the context it
  shares with its enclosing scope.
Notes
  
Modified
  03/21/00 MJRoberts  - Creation
*/

#ifndef VMANONFN_H
#define VMANONFN_H

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmvec.h"
#include "vmglob.h"

/*
 *   Anonymous Function Metaclass 
 */
class CVmObjAnonFn: public CVmObjVector
{
    friend class CVmMetaclassAnonFn;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObjVector::is_of_metaclass(meta));
    }

    /* is the given object an anonymous function object? */
    static int is_anonfn_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* we don't look like a list to user code */
    int is_listlike(VMG_ vm_obj_id_t) { return FALSE; }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* invoke */
    int get_invoker(VMG_ vm_val_t *val);

    /* check for equality - compare strictly by reference */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int) const
    {
        /* return true if the other value is a reference to this object */
        return (val->typ == VM_OBJ && val->val.obj == self);
    }

    /* calculate a hash value */
    uint calc_hash(VMG_ vm_obj_id_t self, int) const
    {
        /* we compare by reference, so hash by object ID */
        return (uint)(((ulong)self & 0xffff)
                      ^ (((ulong)self & 0xffff0000) >> 16));
    }

protected:
    /* create an empty object */
    CVmObjAnonFn() { ext_ = 0; }

    /* create with the given number of elements */
    CVmObjAnonFn(VMG_ size_t cnt)
        : CVmObjVector(vmg_ cnt)
    {
        /* set to our full initial size */
        set_element_count(cnt);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table objects
 */
class CVmMetaclassAnonFn: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "anon-func-ptr/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjAnonFn();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjAnonFn();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjAnonFn::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        /* fall back directly on the vector static property evaluator */
        return CVmObjVector::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* I'm a Vector subclass */
    CVmMetaclass *get_supermeta_reg() const
        { return CVmObjVector::metaclass_reg_; }
};

#endif /* VMANONFN_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjAnonFn)



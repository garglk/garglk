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
  vmintcls.cpp - T3 metaclass - intrinsic class
Function
  
Notes
  
Modified
  03/08/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <assert.h>

#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"
#include "vmintcls.h"
#include "vmmeta.h"
#include "vmfile.h"
#include "vmlst.h"
#include "vmstack.h"
#include "vmtobj.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassClass metaclass_reg_obj;
CVmMetaclass *CVmObjClass::metaclass_reg_ = &metaclass_reg_obj;


/* ------------------------------------------------------------------------ */
/* 
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjClass::create_from_stack(VMG_ const uchar **pc_ptr,
                                           uint argc)
{
    /* it is illegal to create this type of object dynamically */
    err_throw(VMERR_ILLEGAL_NEW);
    AFTER_ERR_THROW(return VM_INVALID_OBJ;)
}

/* 
 *   create with no initial contents 
 */
vm_obj_id_t CVmObjClass::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjClass();
    return id;
}

/*
 *   create with a given dependency index 
 */
vm_obj_id_t CVmObjClass::create_dyn(VMG_ uint meta_idx)
{
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, TRUE, FALSE);
    new (vmg_ id) CVmObjClass(vmg_ FALSE, meta_idx, id);
    return id;
}

/* allocate our extension */
vm_intcls_ext *CVmObjClass::alloc_ext(VMG0_)
{
    /* if I already have an extension, return it */
    if (ext_ != 0)
        return (vm_intcls_ext *)ext_;

    /* allocate the new extension */
    vm_intcls_ext *ext = (vm_intcls_ext *)G_mem->get_var_heap()->alloc_mem(
        sizeof(vm_intcls_ext), this);

    /* save it */
    ext_ = (char *)ext;

    /* return it */
    return ext;
}

/*
 *   create with a given dependency index 
 */
CVmObjClass::CVmObjClass(VMG_ int in_root_set, uint meta_idx,
                         vm_obj_id_t self)
{
    /* calls of this form can't come from the image file */
    assert(!in_root_set);

    /* allocate the extension */
    ext_ = 0;
    vm_intcls_ext *ext = alloc_ext(vmg0_);

    /* set up the extension */
    ext->meta_idx = meta_idx;
    ext->mod_obj = VM_INVALID_OBJ;
    ext->class_state.set_nil();
    ext->changed = FALSE;

    /* register myself with the metaclass table */
    register_meta(vmg_ self);
}

/* 
 *   notify of deletion 
 */
void CVmObjClass::notify_delete(VMG_ int in_root_set)
{
    /* free our extension */
    if (ext_ != 0 && !in_root_set)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* set a property */
void CVmObjClass::set_prop(VMG_ CVmUndo *undo,
                           vm_obj_id_t self, vm_prop_id_t prop,
                           const vm_val_t *val)
{
    /* try treating the request as a static property of the metaclass */
    vm_meta_entry_t *entry = get_meta_entry(vmg0_);
    if (entry != 0
        && entry->meta_->set_stat_prop(
            vmg_ undo, self, &get_ext()->class_state, prop, val))
        return;

    /* the class doesn't handle it, so check for a modifier object */
    vm_obj_id_t mod_obj = get_mod_obj();
    if (mod_obj != VM_INVALID_OBJ)
    {
        /* we have a modifier - set the property in the modifier */
        vm_objp(vmg_ mod_obj)->set_prop(vmg_ undo, mod_obj, prop, val);
    }
    else
    {
        /* if we don't have a modifier, we can't set the property */
        err_throw(VMERR_INVALID_SETPROP);
    }
}

/*
 *   Get a list of our properties 
 */
void CVmObjClass::build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval)
{
    vm_obj_id_t mod_obj;
    vm_meta_entry_t *entry;
    size_t my_prop_cnt;
    size_t mod_prop_cnt;
    vm_val_t mod_val;
    CVmObjList *lst;
    CVmObjList *mod_lst;

    /* presume we won't find any static properties of our own */
    my_prop_cnt = 0;

    /* get my metaclass table entry */
    entry = get_meta_entry(vmg0_);

    /* if we have an entry, count the properties */
    if (entry != 0)
        my_prop_cnt = list_class_props(vmg_ self, entry, 0, 0, FALSE);

    /* if we have a modifier object, get its property list */
    if ((mod_obj = get_mod_obj()) != VM_INVALID_OBJ)
    {
        /* get the modifier's property list - we'll add it to our own */
        vm_objp(vmg_ mod_obj)->build_prop_list(vmg_ self, &mod_val);

        /* get the result as a list object, properly cast */
        mod_lst = (CVmObjList *)vm_objp(vmg_ mod_val.val.obj);

        /*
         *   As an optimization, if we don't have any properties of our own
         *   to add to the modifier list, just return the modifier list
         *   directly (thus avoiding unnecessarily creating another copy of
         *   the list with no changes).
         */
        if (my_prop_cnt == 0)
        {
            /* the modifier list is the entire return value */
            *retval = mod_val;
            return;
        }

        /* get the size of the modifier list */
        mod_prop_cnt = vmb_get_len(mod_lst->get_as_list());
    }
    else
    {
        /* 
         *   we have no modifier object - for the result list, we simply
         *   need our own list, so set the modifier list to nil 
         */
        mod_val.set_nil();
        mod_prop_cnt = 0;
        mod_lst = 0;
    }

    /* for gc protection, push the modifier's list */
    G_stk->push(&mod_val);

    /*
     *   Allocate a list big enough to hold the modifier's list plus our own
     *   list.  
     */
    retval->set_obj(CVmObjList::
                    create(vmg_ FALSE, my_prop_cnt + mod_prop_cnt));

    /* push the return value list for gc protection */
    G_stk->push(retval);

    /* get it as a list object, properly cast */
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);
    lst->cons_clear();

    /* start the list with our own properties */
    if (entry != 0)
        list_class_props(vmg_ self, entry, lst, 0, FALSE);

    /* copy the modifier list into the results, if there is a modifier list */
    if (mod_prop_cnt != 0)
        lst->cons_copy_elements(my_prop_cnt, mod_lst->get_as_list());

    /* done with the gc protection */
    G_stk->discard(2);
}

/*
 *   mark references for garbage collection
 */
void CVmObjClass::mark_refs(VMG_ uint state)
{
    /* if we have an object in our class state value, mark it */
    vm_intcls_ext *ext = get_ext();
    if (ext->class_state.typ == VM_OBJ)
        G_obj_table->mark_all_refs(ext->class_state.val.obj, state);
}


/*
 *   List my metaclass's properties.  Returns the number of properties we
 *   find.  We'll add the properties to the given list; if the list is null,
 *   we'll simply return the count.  
 */
size_t CVmObjClass::list_class_props(VMG_ vm_obj_id_t self,
                                      vm_meta_entry_t *entry,
                                      CVmObjList *lst, size_t starting_idx,
                                      int static_only)
{
    ushort i;
    size_t cnt;
    size_t idx;
    
    /* count the valid entries */
    for (i = 1, idx = starting_idx, cnt = 0 ;
         i <= entry->func_xlat_cnt_ ; ++i)
    {
        vm_prop_id_t prop;
        vm_val_t val;
        vm_obj_id_t source_obj;

        /* get this property */
        prop = entry->xlat_func(i);
        
        /* 
         *   If this one's valid, check to see if it's we're interested in
         *   it.  If we want only statics, include it only if it's
         *   implemented as a static method on this intrinsic class object;
         *   otherwise, include it unconditionally.
         *   
         *   To determine if the property is implemented as a static, call
         *   the property on self without an argc pointer - this is the
         *   special form of the call which merely retrieves the value of the
         *   property without evaluating it.  If the property is defined on
         *   the object, and the source of the definition is self, include
         *   this one in the list of directly-defined properties.  
         */
        if (prop != VM_INVALID_PROP
            && (!static_only
                || (get_prop(vmg_ prop, &val, self, &source_obj, 0)
                    && source_obj == self)))
        {
            /* we're interested - add it to the list if we have one */
            if (lst != 0)
            {
                /* set up a value containing the property pointer */
                val.set_propid(prop);

                /* add it to the list at the next free slot */
                lst->cons_set_element(idx, &val);

                /* advance to the next slot */
                ++idx;
            }

            /* count it */
            ++cnt;
        }
    }

    /* return the number of properties we found */
    return cnt;
}


/*
 *   get a static property 
 */
int CVmObjClass::call_stat_prop(VMG_ vm_val_t *result, const uchar **pc_ptr,
                                uint *argc, vm_prop_id_t prop)
{
    /* translate the property into a function vector index */
    switch(G_meta_table
           ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop))
    {
    case 1:
        /* isIntrinsicClass(x) */
        return s_getp_is_int_class(vmg_ result, argc);

    default:
        /* 
         *   we don't define this one - inherit the default from the base
         *   object metaclass 
         */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
}

/*
 *   static method: isIntrinsicClass(x) 
 */
int CVmObjClass::s_getp_is_int_class(VMG_ vm_val_t *result, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(result, argc, &desc))
        return TRUE;

    /* return true if we have an object of type intrinsic class */
    vm_val_t *v = G_stk->get(0);
    result->set_logical(v->typ == VM_OBJ && is_intcls_obj(vmg_ v->val.obj));

    /* discard the argument */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* 
 *   get a property 
 */
int CVmObjClass::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                          vm_obj_id_t self, vm_obj_id_t *source_obj,
                          uint *argc)
{
    /* 
     *   pass the property request as a static property of the metaclass
     *   with which we're associated 
     */
    vm_meta_entry_t *entry = get_meta_entry(vmg0_);
    if (entry != 0 && entry->meta_->call_stat_prop(vmg_ val, 0, argc, prop))
    {
        /* the metaclass object handled it, so we are the definer */
        *source_obj = self;
        return TRUE;
    }

    /* 
     *   Try handling it through the modifier object, if we have one.  Note
     *   that we must search our own intrinsic superclass chain, because our
     *   own true intrinsic class is 'intrinsic class,' but we want to expose
     *   the nominal superclass hierarchy of the intrinsic class itself (for
     *   example, we want to search List->Collection->Object, not
     *   List->IntrinsicClass->Object).  
     */
    if (get_prop_from_mod(vmg_ prop, val, self, source_obj, argc))
        return TRUE;

    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, val, self, source_obj, argc);
}

/*
 *   Get a property from an intrinsic class modifier object.  Look up the
 *   property in my own modifier, and recursively in my superclass modifiers.
 */
int CVmObjClass::get_prop_from_mod(VMG_ vm_prop_id_t prop, vm_val_t *val,
                                   vm_obj_id_t self, vm_obj_id_t *source_obj,
                                   uint *argc)
{
    vm_obj_id_t sc;
    vm_obj_id_t mod_obj;

    /* if we have a modifier object, look it up in the modifier */
    if ((mod_obj = get_mod_obj()) != VM_INVALID_OBJ
        && vm_objp(vmg_ mod_obj)->get_prop(vmg_ prop, val, mod_obj,
                                           source_obj, argc))
    {
        /* got it - return it from the modifier */
        return TRUE;
    }

    /* 
     *   it's not in our modifier(s); check with any intrinsic superclass
     *   modifiers 
     */
    if (get_superclass_count(vmg_ self) != 0
        && (sc = get_superclass(vmg_ self, 0)) != VM_INVALID_OBJ)
    {
        /* we have a superclass - check it recursively */
        return ((CVmObjClass *)vm_objp(vmg_ sc))
            ->get_prop_from_mod(vmg_ prop, val, sc, source_obj, argc);
    }

    /* 
     *   we didn't find it, and we have no superclass, so there's nowhere
     *   else to look - return failure 
     */
    return FALSE;
}

/*
 *   Find the intrinsic class which the given modifier object modifies.  This
 *   can only be used with a modifier that modifies me or one of my
 *   superclasses.
 */
vm_obj_id_t CVmObjClass::find_mod_src_obj(VMG_ vm_obj_id_t self,
                                          vm_obj_id_t mod_obj)
{
    vm_obj_id_t my_mod_obj;
    vm_obj_id_t sc;
    
    /* 
     *   Is this one of my modifier objects?  It is if it's my most
     *   specialized modifier object (i.e., get_mod_obj()), or if my most
     *   specialized modifier object descends from the given object. 
     */
    my_mod_obj = get_mod_obj();
    if (my_mod_obj != VM_INVALID_OBJ
        && (mod_obj == my_mod_obj
            || vm_objp(vmg_ my_mod_obj)->is_instance_of(vmg_ mod_obj)))
    {
        /* it's one of mine, so I'm the intrinsic class mod_obj modifies */
        return self;
    }

    /* 
     *   It's not one of mine, so check my superclasses recursively.  If we
     *   have no direct superclass, we've failed to find the object.  
     */
    if (get_superclass_count(vmg_ self) == 0
        || (sc = get_superclass(vmg_ self, 0)) == VM_INVALID_OBJ)
        return VM_INVALID_OBJ;
    
    /* ask the superclass to find the modifier */
    return ((CVmObjClass *)vm_objp(vmg_ sc))
        ->find_mod_src_obj(vmg_ sc, mod_obj);
}

/*
 *   Get my metaclass table entry 
 */
vm_meta_entry_t *CVmObjClass::get_meta_entry(VMG0_) const
{
    /* get my metaclass table index */
    uint meta_idx = get_meta_idx();
    
    /* look up my metaclass table entry, if we have one */
    if (meta_idx < G_meta_table->get_count())
        return G_meta_table->get_entry(meta_idx);
    else
        return 0;
}

/* 
 *   save to a file 
 */
void CVmObjClass::save_to_file(VMG_ class CVmFile *fp)
{
    /* write our data: data size, meta table index, mod ID, class state */
    vm_intcls_ext *ext = get_ext();
    fp->write_uint2(2 + 2 + 4 + VMB_DATAHOLDER);
    fp->write_uint2(ext->meta_idx);
    fp->write_uint4(ext->mod_obj);

    char buf[VMB_DATAHOLDER];
    vmb_put_dh(buf, &ext->class_state);
    fp->write_bytes(buf, VMB_DATAHOLDER);
}

/* 
 *   restore from a file 
 */
void CVmObjClass::restore_from_file(VMG_ vm_obj_id_t self,
                                    CVmFile *fp, CVmObjFixup *fixups)
{
    /* allocate/reallocate the extension */
    vm_intcls_ext *ext = alloc_ext(vmg0_);

    /* read the length */
    size_t len = fp->read_uint2();
    const size_t expected_len = 2+2+4+VMB_DATAHOLDER;

    /* 
     *   read the metaclass index and modifier object ID; note that modifier
     *   objects are always root set objects, so there's no need to worry
     *   about fixups 
     */
    ext->meta_idx = fp->read_uint2();
    ext->mod_obj = (vm_obj_id_t)fp->read_uint4();

    /* if we have a class state value, read it */
    ext->class_state.set_nil();
    if (len >= 2+2+4+VMB_DATAHOLDER)
    {
        char buf[VMB_DATAHOLDER];
        fp->read_bytes(buf, VMB_DATAHOLDER);
        fixups->fix_dh(vmg_ buf);
        vmb_get_dh(buf, &ext->class_state);
    }

    /* skip any extra data we don't recognize */
    if (len > expected_len)
        fp->set_pos_from_cur(len - expected_len);

    /* register myself with the metaclass table */
    register_meta(vmg_ self);
}

/* load from an image file */
void CVmObjClass::load_from_image(VMG_ vm_obj_id_t self,
                                  const char *ptr, size_t siz)
{
    /* load the image data */
    load_image_data(vmg_ self, ptr, siz);

    /* we need the image pointer saved for re-loading on RESTART */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/* reload from an image file */
void CVmObjClass::reload_from_image(VMG_ vm_obj_id_t self,
                                    const char *ptr, size_t siz)
{
    /* load the image data */
    load_image_data(vmg_ self, ptr, siz);
}

/* load image data */
void CVmObjClass::load_image_data(VMG_ vm_obj_id_t self,
                                  const char *ptr, size_t siz)
{
    /* make sure the length is valid */
    if (siz < 8)
        err_throw(VMERR_INVAL_METACLASS_DATA);

    /* allocate or reallocate the extension */
    vm_intcls_ext *ext = alloc_ext(vmg0_);

    /* 
     *   read the metaclass index and modifier object ID; note that modifier
     *   objects are always root set objects, so there's no need to worry
     *   about fixups 
     */
    ext->meta_idx = osrp2(ptr+2);
    ext->mod_obj = (vm_obj_id_t)t3rp4u(ptr+4);

    /* if we have a class state value, read it */
    ext->class_state.set_nil();
    if (siz >= 2+4+VMB_DATAHOLDER)
        vmb_get_dh(ptr+8, &ext->class_state);
        
    /* register myself */
    register_meta(vmg_ self);
}

/*
 *   Register myself with the metaclass dependency table - this establishes
 *   the association between the metaclass in the dependency table and this
 *   instance, so that the metaclass knows about the instance that
 *   represents it.  
 */
void CVmObjClass::register_meta(VMG_ vm_obj_id_t self)
{
    /* get my metaclass table entry */
    vm_meta_entry_t *entry = get_meta_entry(vmg0_);

    /* 
     *   if we have a valid entry, store a reference to myself in the
     *   metaclass table - this will let the metaclass that we represent find
     *   us when asked for its class object 
     */
    if (entry != 0)
        entry->class_obj_ = self;
}

/*
 *   Get the number of superclasses 
 */
int CVmObjClass::get_superclass_count(VMG_ vm_obj_id_t) const
{
    /* get my metaclass table entry */
    vm_meta_entry_t *entry = get_meta_entry(vmg0_);

    /* 
     *   if we have a valid entry, ask the metaclass object to tell us the
     *   superclass count 
     */
    if (entry != 0)
        return entry->meta_->get_supermeta_count(vmg0_);
    else
        return 0;
}

/*
 *   Get a superclass 
 */
vm_obj_id_t CVmObjClass::get_superclass(VMG_ vm_obj_id_t, int sc_idx) const
{
    /* get my metaclass table entry */
    vm_meta_entry_t *entry = get_meta_entry(vmg0_);

    /* 
     *   if we have a valid entry, ask the metaclass object to retrieve the
     *   superclass 
     */
    if (entry != 0)
        return entry->meta_->get_supermeta(vmg_ sc_idx);
    else
        return VM_INVALID_OBJ;
}

/*
 *   Determine if I'm an instance of the given object 
 */
int CVmObjClass::is_instance_of(VMG_ vm_obj_id_t obj)
{
    /* get my metaclass table entry */
    vm_meta_entry_t *entry = get_meta_entry(vmg0_);

    /* if we have a valid entry, ask the metaclass object */
    if (entry != 0)
    {
        /* ask the metaclass if it's an instance of the given object */
        return entry->meta_->is_meta_instance_of(vmg_ obj);
    }
    else
    {
        /* not a valid metaclass - we can't make sense of this */
        return FALSE;
    }
}

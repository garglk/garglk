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
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjClass();
    return id;
}

/*
 *   create with a given dependency index 
 */
vm_obj_id_t CVmObjClass::create_dyn(VMG_ uint meta_idx)
{
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
    new (vmg_ id) CVmObjClass(vmg_ FALSE, meta_idx, id);
    return id;
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
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(8, this);

    /* set up the extension - write the length and dependency index */
    oswp2(ext_, 8);
    oswp2(ext_ + 2, meta_idx);
    oswp4(ext_ + 4, VM_INVALID_OBJ);

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
    vm_obj_id_t mod_obj;
    
    /* if I have a modifier object, pass the setprop to the modifier */
    if ((mod_obj = get_mod_obj()) != VM_INVALID_OBJ)
    {
        /* set the property in the modifier object */
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
 *   get a property 
 */
int CVmObjClass::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                          vm_obj_id_t self, vm_obj_id_t *source_obj,
                          uint *argc)
{
    vm_meta_entry_t *entry;

    /* 
     *   pass the property request as a static property of the metaclass
     *   with which we're associated 
     */
    entry = get_meta_entry(vmg0_);
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
    uint meta_idx;

    /* get my metaclass table index */
    meta_idx = get_meta_idx();
    
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
    size_t len;
    
    /* write our data */
    len = osrp2(ext_);
    fp->write_bytes(ext_, len);
}

/* 
 *   restore from a file 
 */
void CVmObjClass::restore_from_file(VMG_ vm_obj_id_t self,
                                    CVmFile *fp, CVmObjFixup *)
{
    size_t len;

    /* read the length */
    len = fp->read_uint2();

    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* allocate the space */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(len, this);

    /* store our length */
    vmb_put_len(ext_, len);

    /* 
     *   read the contents (note that we've already read the length prefix,
     *   so subtract it out of the total remaining to be read) 
     */
    fp->read_bytes(ext_ + VMB_LEN, len - VMB_LEN);

    /* register myself with the metaclass table */
    register_meta(vmg_ self);
}

/* load from an image file */
void CVmObjClass::load_from_image(VMG_ vm_obj_id_t self,
                                  const char *ptr, size_t siz)
{
    /* save a pointer to the image file data as our extension */
    ext_ = (char *)ptr;

    /* make sure the length is valid */
    if (siz < 8)
        err_throw(VMERR_INVAL_METACLASS_DATA);

    /* register myself */
    register_meta(vmg_ self);
}

/* 
 *   reset to the initial load state 
 */
void CVmObjClass::reset_to_image(VMG_ vm_obj_id_t self)
{
    /* re-register myself for the re-load */
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
    vm_meta_entry_t *entry;

    /* get my metaclass table entry */
    entry = get_meta_entry(vmg0_);

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
    vm_meta_entry_t *entry;

    /* get my metaclass table entry */
    entry = get_meta_entry(vmg0_);

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
    vm_meta_entry_t *entry;

    /* get my metaclass table entry */
    entry = get_meta_entry(vmg0_);

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
    vm_meta_entry_t *entry;

    /* get my metaclass table entry */
    entry = get_meta_entry(vmg0_);

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

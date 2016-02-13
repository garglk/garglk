/* $Header$ */

/* Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmintcls.h - T3 metaclass - intrinsic class
Function
  
Notes
  
Modified
  03/08/00 MJRoberts  - Creation
*/

#ifndef VMINTCLS_H
#define VMINTCLS_H


#include <stdlib.h>

#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"

/*
 *   An IntrinsicClass object represents the class of an instance of an
 *   intrinsic class.  For example, if we create a BigNumber instance, then
 *   ask for its class, the result is the IntrinsicClass object associated
 *   with the BigNumber intrinsic class:
 *   
 *   Each metaclass in the metaclass dependency table will be associated with
 *   an IntrinsicClass object.
 *   
 *   The image file format for an IntrinsicClass object consists of the
 *   following:
 *   
 *     UINT2 byte_count (currently, this is always 8)
 *.    UINT2 metaclass_dependency_table_index
 *.    UINT4 modifier_object_id
 *.    DATAHOLDER class_state
 *   
 *   The 'class_state' entry is a generic data holder value for use by the
 *   intrinsic class for storing static class state.  We save and restore
 *   this value automatically and can keep undo information for it.  This can
 *   be used to implement static class-level setprop/getprop to store special
 *   values associated with the class object.
 */

/* intrinsic class extension */
struct vm_intcls_ext
{
    /* metaclass dependency table index */
    int meta_idx;

    /* modifier object */
    vm_obj_id_t mod_obj;

    /* class state */
    vm_val_t class_state;

    /* has the class state value changed since load time? */
    int changed;
};

/*
 *   intrinsic class object 
 */
class CVmObjClass: public CVmObject
{
    friend class CVmMetaclassClass;

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

    /* is the given object an IntrinsicClass object? */
    static int is_intcls_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* 
     *   get my metaclass registration table index - this can be compared to
     *   the metaclass_reg_ element for a given C++ intrinsic class
     *   implementation to determine if this intrinsic class object is the
     *   intrinsic class object for a given C++ intrinsic class 
     */
    uint get_meta_idx() const { return get_ext()->meta_idx; }

    /* get my metaclass table entry */
    struct vm_meta_entry_t *get_meta_entry(VMG0_) const;

    /* 
     *   Get the class state value.  This is for use by the class
     *   implementation to store static (class) state.
     */
    const vm_val_t *get_class_state() { return &get_ext()->class_state; }

    /* Set the class state value, without saving undo. */
    void set_class_state(const vm_val_t *val)
    {
        get_ext()->class_state = *val;
        get_ext()->changed = TRUE;
    }

    /*
     *   Set the class state value with undo.  This is unimplemented for now,
     *   since no one needs it.  In principle it could be useful to be able
     *   to store a scalar value in the class state slot, in which case we'd
     *   need to have special code here in order to undo changes.  So far,
     *   though, we always store an object reference in class_state, such as
     *   a Vector or TadsObject, since we need to be able to store more than
     *   just a single scalar value.  Since all of the state data are then
     *   stored within those container objects, and the container classes we
     *   use all handle undo themselves, there's no need for any undo action
     *   at the class_state level.
     */
    // Unimplemented!
    void set_class_state_undo(VMG_ class CVmUndo *undo, const vm_val_t *val);

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* create for a given dependency table index */
    static vm_obj_id_t create_dyn(VMG_ uint meta_idx);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* determine if I'm an instance of the given object */
    virtual int is_instance_of(VMG_ vm_obj_id_t obj);

    /* get superclass information */
    int get_superclass_count(VMG_ vm_obj_id_t self) const;
    vm_obj_id_t get_superclass(VMG_ vm_obj_id_t self, int idx) const;

    /*
     *   Determine if this is a class object.  All intrinsic class objects
     *   indicate true.  
     */
    virtual int is_class_object(VMG_ vm_obj_id_t /*self*/) const
        { return TRUE; }

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self)
    {
        /* 
         *   we reference no other data and cannot be converted to constant
         *   data ourselves, so there's nothing to do here 
         */
    }

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self)
    {
        /* 
         *   we reference no other data and cannot be converted to constant
         *   data ourselves, so there's nothing to do here 
         */
    }

    /* create with no initial contents */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* build a list of my properties */
    void build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval);

    /* undo operations - classes are immutable and hence keep no undo */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }    

    /* mark references */
    void mark_refs(VMG_ uint state);

    /* remove weak references - we have none */
    void remove_stale_weak_refs(VMG0_) { }

    /* have we changed since load time? */
    int is_changed_since_load() const { return get_ext()->changed; }

    /* load/reload from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);
    void reload_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* get the user modifier object for the intrinsic class */
    vm_obj_id_t get_mod_obj() const { return get_ext()->mod_obj; }

    /* 
     *   find the intrinsic class for the given modifier object, searching
     *   myself and my intrinsic superclasses 
     */
    vm_obj_id_t find_mod_src_obj(VMG_ vm_obj_id_t self, vm_obj_id_t mod_obj);

    /* static method: isIntrinsicClass(x) */
    static int s_getp_is_int_class(VMG_ vm_val_t *result, uint *argc);

protected:
    /* create with no initial contents */
    CVmObjClass() { ext_ = 0; }

    /* create with a given dependency table index */
    CVmObjClass(VMG_ int in_root_set, uint meta_idx, vm_obj_id_t self);

    /* allocate or reallocate my extension  */
    vm_intcls_ext *alloc_ext(VMG0_);

    /* get my extension */
    vm_intcls_ext *get_ext() const { return (vm_intcls_ext *)ext_; }

    /* load/reload the image data */
    void load_image_data(VMG_ vm_obj_id_t self,
                         const char *ptr, size_t siz);

    /* list our intrinsic class's properties */
    size_t list_class_props(VMG_ vm_obj_id_t self,
                            struct vm_meta_entry_t *entry,
                            class CVmObjList *lst, size_t starting_idx,
                            int static_only);

    /* 
     *   Find the intrinsic class for the given modifier object.  We override
     *   this because we want an intrinsic class's effective intrinsic class
     *   to be its intrinsic superclass, not its metaclass.  The metaclass of
     *   an intrinsic class object is always IntrinsicClass; instead, we want
     *   to see, for example, List->Collection->Object, which is the
     *   intrinsic superclass hierarchy.  
     */
    vm_obj_id_t find_intcls_for_mod(VMG_ vm_obj_id_t self,
                                    vm_obj_id_t mod_obj)
    {
        /*
         *   The implementation is very simple: just look for a modifier
         *   object attached to this object or one of its intrinsic
         *   superclasses.  The difference between this and the regular
         *   CVmObject implementation is that the CVmObject implementation
         *   looks in the object's metaclass; we simply look in our intrinsic
         *   superclasses directly, since, for reflection purposes, we are
         *   our own metaclass.  
         */
        return find_mod_src_obj(vmg_ self, mod_obj);
    }

    /* 
     *   search for a property among our modifiers, searching our own
     *   modifier and modifiers for our superclasses 
     */
    int get_prop_from_mod(VMG_ vm_prop_id_t prop, vm_val_t *val,
                          vm_obj_id_t self, vm_obj_id_t *source_obj,
                          uint *argc);

    /* register myself with the dependency table */
    void register_meta(VMG_ vm_obj_id_t self);
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassClass: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "intrinsic-class/030001"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjClass();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjClass();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjClass::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjClass::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMINTCLS_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjClass)

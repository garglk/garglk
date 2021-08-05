/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmiter.h - Iterator metaclass
Function
  
Notes
  
Modified
  04/22/00 MJRoberts  - Creation
*/

#ifndef VMITER_H
#define VMITER_H

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"

/* ------------------------------------------------------------------------ */
/*
 *   Base Iterator class 
 */
class CVmObjIter: public CVmObject
{
    friend class CVmMetaclassIter;
    
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

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
        { return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop); }

    /* set a property */
    void set_prop(VMG_ class CVmUndo *,
                  vm_obj_id_t, vm_prop_id_t, const vm_val_t *)
    {
        /* cannot set iterator properties */
        err_throw(VMERR_INVALID_SETPROP);
    }
    
    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

protected:
    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - get next value */
    virtual int getp_get_next(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc) = 0;

    /* property evaluator - is next value available? */
    virtual int getp_is_next_avail(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc) = 0;

    /* property evaluator - reset to first item */
    virtual int getp_reset_iter(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc) = 0;

    
    /* property evaluator - get current key */
    virtual int getp_get_cur_key(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc) = 0;

    /* property evaluator - get current value */
    virtual int getp_get_cur_val(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc) = 0;

    /* function table */
    static int (CVmObjIter::*func_table_[])(VMG_ vm_obj_id_t self,
                                            vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   Indexed Iterator subclass.  An indexed iterator works with arrays,
 *   lists, and other collections that can be accessed via an integer
 *   index.  
 */

/*
 *   The extension data for an indexed iterator consists of a reference to
 *   the associated indexed collection, the index value of the next item
 *   to be retrieved, and the first and last valid index values: Note that
 *   the collection value can be an object ID or a constant VM_LIST value.
 *   
 *   DATAHOLDER collection_value
 *.  UINT4 cur_index
 *.  UINT4 first_valid
 *.  UINT4 last_valid
 *.  UINT4 flags
 *   
 *   The flag values are:
 *   
 *   VMOBJITERIDX_UNDO - we've saved undo for this savepoint.  If this is
 *   set, we won't save additional undo for the same savepoint.
 */

/* total extension size */
#define VMOBJITERIDX_EXT_SIZE  (VMB_DATAHOLDER + 16)

/* 
 *   flag bits 
 */

/* we've saved undo for the current savepoint */
#define VMOBJITERIDX_UNDO   0x0001

/*
 *   indexed iterator class 
 */
class CVmObjIterIdx: public CVmObjIter
{
    friend class CVmMetaclassIterIdx;

public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObjIter::is_of_metaclass(meta));
    }

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        return CVmObjIter::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /*
     *   Create an indexed iterator.  This method is to be called by a
     *   list, array, or other indexed collection object to create an
     *   iterator for its value.  
     */
    static vm_obj_id_t create_for_coll(VMG_ const vm_val_t *coll,
                                       long first_valid_index,
                                       long last_valid_index);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* 
     *   notify of a new savepoint - clear the 'undo' flag, since we
     *   cannot have created any undo information yet for the new
     *   savepoint 
     */
    void notify_new_savept()
        { set_flags(get_flags() & ~VMOBJITERIDX_UNDO); }

    /* apply undo */
    void apply_undo(VMG_ struct CVmUndoRecord *rec);

    /* mark references */
    void mark_refs(VMG_ uint state);

    /* there are no references in our undo stream */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* we keep only strong references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* restore to image file state */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* 
     *   determine if the object has been changed since it was loaded -
     *   assume we have, since saving and reloading are very cheap
     */
    int is_changed_since_load() const { return TRUE; }

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* get the next iteration item */
    virtual int iter_next(VMG_ vm_obj_id_t self, vm_val_t *val);

protected:
    /* create */
    CVmObjIterIdx() { ext_ = 0; }
    
    /* create */
    CVmObjIterIdx(VMG_ const vm_val_t *coll, long first_valid_index,
                  long last_valid_index);

    /* get the value from the collection for a given index */
    void get_indexed_val(VMG_ long idx, vm_val_t *retval);

    /* property evaluator - get next value */
    virtual int getp_get_next(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc);

    /* property evaluator - is next value available? */
    virtual int getp_is_next_avail(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc);

    /* property evaluator - reset to first item */
    virtual int getp_reset_iter(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc);

    /* property evaluator - get current key */
    virtual int getp_get_cur_key(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc);

    /* property evaluator - get current value */
    virtual int getp_get_cur_val(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc);

    /* get my collection value */
    void get_coll_val(vm_val_t *val) { vmb_get_dh(ext_, val); }

    /* get/set the current index (without saving undo) */
    long get_cur_index() const
        { return t3rp4u(ext_ + VMB_DATAHOLDER); }
    void set_cur_index_no_undo(long idx)
        { oswp4(ext_ + VMB_DATAHOLDER, idx); }

    /* set the index value, saving undo if necessary */
    void set_cur_index(VMG_ vm_obj_id_t self, long idx);

    /* get my first/last valid index values */
    long get_first_valid() const
        { return t3rp4u(ext_ + VMB_DATAHOLDER + 4); }
    long get_last_valid() const
        { return t3rp4u(ext_ + VMB_DATAHOLDER + 8); }

    /* set my first/last valid index values - for construction only */
    void set_first_valid(long idx) const
        { oswp4(ext_ + VMB_DATAHOLDER + 4, idx); }
    void set_last_valid(long idx) const
        { oswp4(ext_ + VMB_DATAHOLDER + 8, idx); }

    /* get/set the flags */
    unsigned long get_flags() const
        { return t3rp4u(ext_ + VMB_DATAHOLDER + 12); }
    void set_flags(unsigned long flags) const
        { oswp4(ext_ + VMB_DATAHOLDER + 12, flags); }
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table object for the base iterator class
 */
class CVmMetaclassIter: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "iterator/030001"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
        { err_throw(VMERR_BAD_STATIC_NEW); }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
        { err_throw(VMERR_BAD_STATIC_NEW); }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
    {
        err_throw(VMERR_BAD_DYNAMIC_NEW);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjIter::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

/*
 *   Registration table object for indexed iterators
 */
class CVmMetaclassIterIdx: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "indexed-iterator/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjIterIdx();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjIterIdx();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
    {
        err_throw(VMERR_BAD_DYNAMIC_NEW);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjIterIdx::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMITER_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjIter)
VM_REGISTER_METACLASS(CVmObjIterIdx)


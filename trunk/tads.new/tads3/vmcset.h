/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmcset.h - T3 CharacterSet metaclass
Function
  
Notes
  
Modified
  06/06/01 MJRoberts  - Creation
*/

#ifndef VMCSET_H
#define VMCSET_H

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"

/* ------------------------------------------------------------------------ */
/*
 *   A CharacterSet is a simple encapsulation of a pair of CCharmap
 *   character mappings: one mapping from Unicode to a local character set,
 *   and one mapping in the reverse direction.  A CharacterSet is
 *   parameterized on creation by the name of the mapping, using the
 *   standard CCharmap names.
 *   
 *   In an image file, a CharacterSet contains simply the standard CCharmap
 *   name of the mapping:
 *   
 *   UINT2 length-in-bytes
 *.  BYTE name[]
 *   
 *   On creation, we will create the pair of CCharmap objects, if the name
 *   of the mapping is valid.  It is legal to create a CharacterSet with an
 *   unknown mapping, but such a character set object cannot be used to
 *   perform mappings.
 *   
 *   CharacterSet objects are constants at run-time.  
 */
class CVmObjCharSet: public CVmObject
{
    friend class CVmMetaclassCharSet;
    
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

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        /* explicitly inherit our superclass handling */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *,
                                    vm_obj_id_t /*self*/)
    {
        /* we can't be converted to constant data */
    }

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *,
                                       vm_obj_id_t /*self*/)
    {
        /* 
         *   we don't reference any data and can't be converted to constant
         *   data ourselves, so there's nothing to do here 
         */
    }

    /* create with no initial contents */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* create with the given character set name */
    static vm_obj_id_t create(VMG_ int in_root_set, const char *charset_name,
                              size_t charset_name_len);

    /* determine if an object is a CharacterSet */
    static int is_charset(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* undo operations */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }

    /* we reference nothing */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }
    void mark_refs(VMG_ uint /*state*/) { }
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* 
     *   Check a value for equality.  We will match another byte array with
     *   the same number of elements and the same value for each element.  
     */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;

    /* calculate a hash value for the array */
    uint calc_hash(VMG_ vm_obj_id_t self, int depth) const;

    /* our data are constant - we never change */
    int is_changed_since_load() const { return FALSE; }

    /*
     *   Get the to-local and to-unicode mappers.  If the mapper isn't
     *   available, we'll throw an UnknownCharacterSetException. 
     */
    class CCharmapToLocal *get_to_local(VMG0_) const;
    class CCharmapToUni *get_to_uni(VMG0_) const;

protected:
    /* create with no initial contents */
    CVmObjCharSet() { ext_ = 0; }

    /* create from the given character set name */
    CVmObjCharSet(VMG_ const char *charset_name, size_t charset_name_len);

    /* allocate and initialize */
    void alloc_ext(VMG_ const char *charset_name, size_t charset_name_len);

    /* get a pointer to my extension */
    const struct vmobj_charset_ext_t *get_ext_ptr() const
        { return (vmobj_charset_ext_t *)ext_; }

    /* does the given unicode character have a round-trip mapping? */
    static int is_rt_mappable(wchar_t c, class CCharmapToLocal *to_local,
                              class CCharmapToUni *to_uni);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - get the character set name */
    int getp_get_name(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* determine if the mapping is known */
    int getp_is_known(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);
    
    /* 
     *   property evaluator - determine if the a character code (given as an
     *   integer) or the characters in a string can be mapped from Unicode
     *   to this local character set 
     */
    int getp_is_mappable(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* 
     *   property evaluator - determine if the character code (given as an
     *   integer) or the characters in a string have a round-trip mapping
     *   from Unicode to local and back 
     */
    int getp_is_rt_mappable(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluation function table */
    static int (CVmObjCharSet::*func_table_[])(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);
};

/*
 *   Our extension structure 
 */
struct vmobj_charset_ext_t
{
    /* unicode-to-local mapping object */
    class CCharmapToLocal *to_local;

    /* local-to-unicode mapping object */
    class CCharmapToUni *to_uni;

    /* length of character set name */
    size_t charset_name_len;

    /* name of the character set */
    char charset_name[1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassCharSet: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "character-set/030001"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjCharSet();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjCharSet();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjCharSet::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjCharSet::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMCSET_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjCharSet)

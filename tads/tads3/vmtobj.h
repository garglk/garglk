/* $Header: d:/cvsroot/tads/tads3/VMTOBJ.H,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtobj.h - VM TADS Object implementation
Function
  
Notes
  This implementation assumes a non-relocating memory manager, both for
  the fixed part (the CVmObject part) and the variable part (the
  "extension," located in the variable-part heap) of our objects.  In the
  present implementation, the memory manager satisfies this requirement, and
  there are no plans to change this.

  The memory manager is designed *in principal* to allow for object
  relocation (specifically as a means to reduce heap fragmentation), so it's
  not a foregone conclusion that such a thing will never be implemented.
  However, given the large memories of modern machines (especially relative
  to the size of a typical tads application), and given that recent academic
  research has been calling into question the conventional wisdom that heap
  fragmentation is actually a problem in practice, we consider the
  probability that we will want to implement a relocation memory manager
  low, and thus we feel it's better to exploit the efficiencies of using and
  storing direct object pointers in some places in this code.
Modified
  10/30/98 MJRoberts  - Creation
*/

#ifndef VMTOBJ_H
#define VMTOBJ_H

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"

/* forward-declare our main class */
class CVmObjTads;

/* ------------------------------------------------------------------------ */
/*
 *   TADS-Object image file data.  The image file state is loaded into an
 *   image object data block, and we set up our own internal data based on
 *   it at load time.  The image file data block is arranged as follows:
 *   
 *.  UINT2 superclass_count
 *.  UINT2 load_image_property_count
 *.  UINT2 flags
 *.  UINT4 superclass_1
 *.  ...
 *.  UINT4 superclass_N
 *.  UINT2 load_image_property_ID_1
 *.  DATAHOLDER load_image_property_value_1
 *.  ...
 *.  UINT2 load_image_property_ID_N
 *.  DATAHOLDER load_image_property_value_N 
 */  

/* superclass structure for object extension */
struct vm_tadsobj_sc
{
    vm_obj_id_t id;
    CVmObjTads *objp;
};


/*
 *   For our in-memory object extension, we use a structure that stores the
 *   object data.  We store the properties in a hash table keyed on property
 *   ID.  
 */
struct vm_tadsobj_hdr
{
    /* allocate */
    static vm_tadsobj_hdr *alloc(VMG_ class CVmObjTads *self,
                                 unsigned short sc_cnt,
                                 unsigned short prop_cnt);

    /* delete */
    void free_mem();

    /* reallocate an existing object to expand its property table */
    static vm_tadsobj_hdr *expand(VMG_ class CVmObjTads *self,
                                  vm_tadsobj_hdr *obj);

    /* 
     *   reallocate an existing object to expand its property table to the
     *   given minimum number of property entries 
     */
    static vm_tadsobj_hdr *expand_to(VMG_ class CVmObjTads *self,
                                     vm_tadsobj_hdr *obj,
                                     size_t new_sc_cnt, size_t min_prop_cnt);

    /* invalidate the cached inheritance path, if any */
    void inval_inh_path()
    {
        /* if we have an inheritance path cached, forget it */
        if (inh_path != 0)
        {
            /* forget the path, so that we recalculate it on demand */
            t3free(inh_path);
            inh_path = 0;
        }
    }

    /* find a property entry */
    struct vm_tadsobj_prop *find_prop_entry(uint prop);

    /* allocate a new hash entry */
    vm_tadsobj_prop *alloc_prop_entry(vm_prop_id_t prop,
                                      const vm_val_t *val,
                                      unsigned int flags);

    /* calculate the hash code for a property */
    unsigned int calc_hash(uint prop) const
    {
        /* simply take the property ID modulo the table size */
        return (unsigned int)(prop & (hash_siz - 1));
    }

    /* check to see if we have the required number of free entries */
    int has_free_entries(size_t cnt) const
        { return cnt <= (size_t)(prop_entry_free - prop_entry_cnt); }
    
    /* cache and return the inheritance search path for this object */
    struct tadsobj_objid_and_ptr *get_inh_search_path(VMG0_);

    /* 
     *   Inheritance search table.  We build and save the search path for any
     *   class with multiple superclasses, because the inheritance path for a
     *   class with multiple base classes can be somewhat time-consuming to
     *   determine.  For objects with only one base class, we don't bother
     *   caching a path, since the path is trivial to calculate in these
     *   cases.  
     */
    struct tadsobj_objid_and_ptr *inh_path;

    /* load image object flags (a combination of VMTOBJ_OBJF_xxx values) */
    unsigned short li_obj_flags;

    /* internal object flags (a combination of VMTO_OBJ_xxx values) */
    unsigned short intern_obj_flags;

    /* 
     *   Number of hash buckets, and a pointer to the bucket array.  (The
     *   hash bucket array is allocated as part of the same memory block as
     *   this structure - we suballocate it from the memory block when
     *   allocating the structure.)  'hash_arr[hash]' points to the head of
     *   a list of property entries with the given hash value.
     */
    unsigned short hash_siz;
    struct vm_tadsobj_prop **hash_arr;

    /*
     *   Pointer to our allocation array of hash buckets.  We suballocate
     *   this out of our allocation block.  (Note that this isn't the hash
     *   table; this is the pool of elements out of which hash table entries
     *   - not buckets, but the entries in the lists pointed to by the
     *   buckets - are allocated.)  
     */
    struct vm_tadsobj_prop *prop_entry_arr;

    /* total number of hash entries allocated */
    unsigned short prop_entry_cnt;

    /* 
     *   Index of next available hash entry.  Hash entries are never
     *   deleted, so we don't have to worry about returning entries to the
     *   free pool.  So, the free pool simply consists of entries from this
     *   index to the maximum index (prop_entry_cnt - 1).
     *   
     *   When we run out of entries, we must reallocate this entire
     *   structure to make room for more.  This means that reallocation is
     *   fairly expensive, but this is acceptable because we will always
     *   want to resize the hash table at the same time anyway.  We always
     *   resize the hash table on exhausting our current allocation size
     *   because we pick the hash table size based on the expected maximum
     *   number of entries; once we exceed that maximum, we must reconsider
     *   the hash table size.  
     */
    unsigned short prop_entry_free;
    
    /* 
     *   Number of superclasses, and the array of superclasses.  We
     *   overallocate the structure to make room for enough superclasses.
     *   (Note that this means the 'sc' field must be the last thing in the
     *   structure.)  
     */
    unsigned short sc_cnt;
    vm_tadsobj_sc sc[1];
};

/*
 *   Tads-object property entry.  Each hash table entry points to a linked
 *   list of these entries.  
 */
struct vm_tadsobj_prop
{
    /* my property ID */
    vm_prop_id_t prop;

    /* pointer to the next entry at the same hash value */
    vm_tadsobj_prop *nxt;

    /* flags */
    unsigned char flags;

    /* my value */
    vm_val_t val;
};


/*
 *   Internal object flags 
 */

/* from load image - object originally came from image file */
#define VMTO_OBJ_IMAGE   0x0001

/* modified - object has been modified since being loaded from image */
#define VMTO_OBJ_MOD     0x0002


/*
 *   Property entry flags 
 */

/* modified - this property is not from the load image file */
#define VMTO_PROP_MOD     0x01

/* we've stored undo for this property since the last savepoint */
#define VMTO_PROP_UNDO    0x02

/* ------------------------------------------------------------------------ */
/*
 *   Load Image Object Flag Values - these values are stored in the image
 *   file object header.  
 */

/* class - the object represents a class, not an instance */
#define VMTOBJ_OBJF_CLASS    0x0001


/* ------------------------------------------------------------------------ */
/*
 *   Initial empty property table size.  When we initially load an object,
 *   we'll allocate this many empty slots for modifiable properties. 
 */
const ushort VMTOBJ_PROP_INIT = 16;


/* ------------------------------------------------------------------------ */
/*
 *   TADS object interface.
 */
class CVmObjTads: public CVmObject
{
    friend class CVmMetaclassTads;
    friend struct tadsobj_sc_search_ctx;
    friend struct vm_tadsobj_hdr;
    
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

    /* is the given object a TadsObject object? */
    static int is_tadsobj_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc)
        { return create_from_stack_intern(vmg_ pc_ptr, argc, FALSE); }

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* create an object with no initial extension */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* 
     *   Create an object with a given number of superclasses, and a given
     *   number of property slots.  The property slots are all initially
     *   allocated to modified properties.  
     */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              ushort superclass_count, ushort prop_slots);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* create an instance of this object */
    void create_instance(VMG_ vm_obj_id_t self,
                         const uchar **pc_ptr, uint argc);

    /* determine if the object has a finalizer method */
    virtual int has_finalizer(VMG_ vm_obj_id_t /*self*/);

    /* invoke the object's finalizer */
    virtual void invoke_finalizer(VMG_ vm_obj_id_t self);

    // $$$ testing
    virtual int equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                       int depth) const
    {
        if (((vm_tadsobj_hdr *)ext_)->hash_siz == 0)
            return TRUE;
        else
            return (val->typ == VM_OBJ && val->val.obj == self);
    }

    virtual uint calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const
    {
        if (((vm_tadsobj_hdr *)ext_)->hash_siz == 0)
            return 0;
        else
            return (uint)(((ulong)self & 0xffff)
                          ^ (((ulong)self & 0xffff0000) >> 16));
    }
    // $$$ end testing

    /* get the number of superclasses of this object */
    virtual int get_superclass_count(VMG_ vm_obj_id_t self) const
    {
        /* 
         *   if we have no superclass, inherit the default, since we
         *   inherit from the system TadsObject class; if we have our own
         *   superclasses, return them 
         */
        if (get_sc_count() == 0)
            return CVmObject::get_superclass_count(vmg_ self);
        else
            return get_sc_count();
    }

    /* get the nth superclass of this object */
    virtual vm_obj_id_t get_superclass(VMG_ vm_obj_id_t self, int idx) const
    {
        /* 
         *   if we have no superclass, inherit the default, since we
         *   inherit from the system TadsObject class; if we have our own
         *   superclasses, return them 
         */
        if (get_sc_count() == 0)
            return CVmObject::get_superclass(vmg_ self, idx);
        else if (idx >= get_sc_count())
            return VM_INVALID_OBJ;
        else
            return get_sc(idx);
    }

    /* determine if I'm a class object */
    virtual int is_class_object(VMG_ vm_obj_id_t /*self*/) const
        { return (get_li_obj_flags() & VMTOBJ_OBJF_CLASS) != 0; }

    /* determine if I'm an instance of the given object */
    int is_instance_of(VMG_ vm_obj_id_t obj);

    /* this object type provides properties */
    int provides_props(VMG0_) const { return TRUE; }

    /* enumerate properties */
    void enum_props(VMG_ vm_obj_id_t self,
                    void (*cb)(VMG_ void *ctx,
                               vm_obj_id_t self, vm_prop_id_t prop,
                               const vm_val_t *val),
                    void *cbctx);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* inherit a property */
    int inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self,
                 vm_obj_id_t orig_target_obj,
                 vm_obj_id_t defining_obj,
                 vm_obj_id_t *source_obj, uint *argc);

    /* build my property list */
    void build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval);

    /* 
     *   Receive notification of a new savepoint.  We keep track of
     *   whether or not we've saved undo information for each modifiable
     *   property in the current savepoint, so that we can avoid saving
     *   redundant undo information when repeatedly changing a property
     *   value (since only the first change in a given savepoint needs to
     *   be recorded).  When we start a new savepoint, we obviously
     *   haven't yet stored any undo information for the new savepoint, so
     *   we can simply clear all of the undo records.  
     */
    void notify_new_savept()
        { clear_undo_flags(); }

    /* apply undo */
    void apply_undo(VMG_ struct CVmUndoRecord *rec);

    /* mark a reference in an undo record */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *undo);

    /* 
     *   remove stale weak references from an undo record -- we keep only
     *   normal strong references, so we don't need to do anything here 
     */
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark references */
    void mark_refs(VMG_ uint state);

    /* 
     *   remove weak references - we keep only normal (strong) references,
     *   so this routine doesn't need to do anything 
     */
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* restore to image file state */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* perform post-load initialization */
    void post_load_init(VMG_ vm_obj_id_t self);

    /* determine if the object has been changed since it was loaded */
    int is_changed_since_load() const;

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

    /* get the nth superclass */
    vm_obj_id_t get_sc(uint n) const
        { return get_hdr()->sc[n].id; }

    /* get a pointer to the object for the nth superclass */
    CVmObjTads *get_sc_objp(VMG_ ushort n) const
        { return get_hdr()->sc[n].objp; }

    /* set the nth superclass to the given object */
    void set_sc(VMG_ ushort n, vm_obj_id_t obj)
    {
        get_hdr()->sc[n].id = obj;
        get_hdr()->sc[n].objp = (CVmObjTads *)vm_objp(vmg_ obj);
    }

    /* static class initialization/termination */
    static void class_init(VMG0_);
    static void class_term(VMG0_);

protected:
    /* create an object with no initial extension */
    CVmObjTads() { ext_ = 0; }

    /* 
     *   Create an object with a given number of superclasses, and a given
     *   number of property slots.  All property slots are initially
     *   allocated to the modifiable property list.  
     */
    CVmObjTads(VMG_ ushort superclass_count, ushort prop_count);

    /* internal handler to create from stack arguments */
    static vm_obj_id_t create_from_stack_intern(VMG_ const uchar **pc_ptr,
                                                uint argc, int is_transient);

    /* 
     *   internal handler to create with multiple inheritance from arguments
     *   passed on the stack 
     */
    static vm_obj_id_t create_from_stack_multi(VMG_ uint argc,
                                               int is_transient);
    
    /* get the load image object flags */
    uint get_li_obj_flags() const
        { return get_hdr()->li_obj_flags; }

    /* set the object flags */
    void set_li_obj_flags(ushort flags)
        { get_hdr()->li_obj_flags = flags; }

    /* 
     *   mark the object as modified; sets the VMTO_OBJ_MOD flag, and adds an
     *   undo record for the transition if the flag wasn't previously set 
     */
    void mark_modified(VMG_ class CVmUndo *undo, vm_obj_id_t self);

    /* 
     *   Allocate memory - this replaces any existing extension, so the
     *   caller must take care to free the extension (if one has already
     *   been allocated) before calling this routine.
     *   
     *   If 'from_image' is true, we're allocating memory for use with an
     *   object loaded from an image file, so we'll ignore the superclass
     *   count and leave the image_data pointer in the header unchanged.
     *   If 'from_image' is false, we're allocating memory for a dynamic
     *   object that does not have a presence in the image file, so we'll
     *   allocate space for the superclass list as part of the extension
     *   and set the image_data pointer in the header to refer the extra
     *   space after the modifiable property array and undo bit array.  
     */
    void alloc_mem(VMG_ ushort sc_count, ushort mod_prop_count,
                   int from_image);

    /* get a property from the intrinsic class */
    int get_prop_intrinsic(VMG_ vm_prop_id_t prop, vm_val_t *val,
                           vm_obj_id_t self, vm_obj_id_t *source_obj,
                           uint *argc);

    /*
     *   Search for a property, continuing a previous search from the given
     *   point.  defining_obj is the starting point for the search: we start
     *   searching in the target object's inheritance tree after
     *   defining_obj.  This is used to continue an inheritance search from
     *   a given point, as needed for the 'inherited' operator, for example.
     */
    static int search_for_prop_from(VMG_ uint prop,
                                    vm_val_t *val,
                                    vm_obj_id_t orig_target_obj,
                                    vm_obj_id_t *source_obj,
                                    vm_obj_id_t defining_obj);

    /* load the image file properties and superclasses */
    void load_image_props_and_scs(VMG_ const char *ptr, size_t siz);

    /* get/set the superclass count */
    ushort get_sc_count() const
        { return get_hdr()->sc_cnt; }
    void set_sc_count(ushort cnt)
        { get_hdr()->sc_cnt = cnt; }

    /* change the superclass list */
    void change_superclass_list(VMG_ const vm_val_t *lst, int cnt);

    /* clear all undo flags */
    void clear_undo_flags();


    /* -------------------------------------------------------------------- */
    /*
     *   Low-level format management - these routines encapsulate the byte
     *   layout of the object in memory.  This is a bit nasty because we
     *   keep the object's contents in the portable image format.  
     */

    /* get my header */
    inline struct vm_tadsobj_hdr *get_hdr() const
        { return (vm_tadsobj_hdr *)ext_; }

    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - createInstance */
    int getp_create_instance(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - createClone */
    int getp_create_clone(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - createTransientInstance */
    int getp_create_trans_instance(VMG_ vm_obj_id_t, vm_val_t *retval,
                                   uint *argc);

    /* property evaluator - createInstanceOf */
    int getp_create_instance_of(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *in_argc);

    /* property evaluator - createTransientInstanceOf */
    int getp_create_trans_instance_of(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *in_argc);

    /* common handler for createInstance and createTransientInstance */
    int getp_create_common(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc,
                           int is_transient);

    /* common handler for createInstanceOf and createTransientInstanceOf */
    int getp_create_multi_common(VMG_ vm_obj_id_t, vm_val_t *retval,
                                 uint *argc, int is_transient);

    /* property evaluator - setSuperclassList */
    int getp_set_sc_list(VMG_ vm_obj_id_t self,
                         vm_val_t *retval, uint *in_argc);

    /* object table iteration callback for setSuperclassList */
    static void set_sc_cb(VMG_ vm_obj_id_t obj, void *ctx);

    /* property evaluator - getMethod */
    int getp_get_method(VMG_ vm_obj_id_t self,
                        vm_val_t *retval, uint *in_argc);

    /* property evaluator - setMethod */
    int getp_set_method(VMG_ vm_obj_id_t self,
                        vm_val_t *retval, uint *in_argc);

    /* property evaluation function table */
    static int (CVmObjTads::*func_table_[])(VMG_ vm_obj_id_t self,
                                            vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassTads: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "tads-object/030005"; }
    
    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjTads();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }
    
    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjTads();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }
    
    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjTads::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjTads::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Intrinsic class modifier object.  This object is for use as a modifier
 *   object for an intrinsic class.
 *   
 *   This is a simple subclass of the regular TADS-Object class.  The only
 *   difference is that we resolve properties a little differently: unlike
 *   regular TADS Objects, this class is essentially a mix-in, and has no
 *   intrinsic superclass at all.  This means that the only place we look
 *   for a property in get_prop is in our property list; we specifically do
 *   not look for an intrinsic property, nor do we look for a superclass
 *   that provides an intrinsic property.  
 */
class CVmObjIntClsMod: public CVmObjTads
{
    friend class CVmMetaclassIntClsMod;
    
public:
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObjTads::is_of_metaclass(meta));
    }

    /* is the given object an intrinsic class modifier object? */
    static int is_intcls_mod_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc)
    {
        /* can't create instances of intrinsic class modifiers */
        err_throw(VMERR_ILLEGAL_NEW);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        return CVmObjTads::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* inherit a property */
    int inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self,
                 vm_obj_id_t orig_target_obj,
                 vm_obj_id_t defining_obj,
                 vm_obj_id_t *source_obj, uint *argc);

    /* build my property list */
    void build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval);

    /* create an object with no initial extension */
    CVmObjIntClsMod() { ext_ = 0; }

    /* 
     *   Create an object with a given number of superclasses, and a given
     *   number of property slots.  All property slots are initially
     *   allocated to the modifiable property list.  
     */
    CVmObjIntClsMod(VMG_ ushort superclass_count, ushort prop_count)
        : CVmObjTads(vmg_ superclass_count, prop_count) { }
};

/*
 *   Registration table object 
 */
class CVmMetaclassIntClsMod: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "int-class-mod/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjIntClsMod();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjIntClsMod();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjIntClsMod::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjIntClsMod::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   object ID + pointer structure 
 */
struct tadsobj_objid_and_ptr
{
    vm_obj_id_t id;
    CVmObjTads *objp;
};

/* ------------------------------------------------------------------------ */
/*
 *   Queue element for the inheritance path search queue 
 */
struct pfq_ele
{
    /* object ID of this element */
    vm_obj_id_t obj;

    /* pointer to the object */
    CVmObjTads *objp;

    /* next queue element */
    pfq_ele *nxt;
};

/* allocation page */
struct pfq_page
{
    /* next page in the list */
    pfq_page *nxt;

    /* the elements for this page */
    pfq_ele eles[50];
};

/* ------------------------------------------------------------------------ */
/*
 *   Queue for search_for_prop().  This implements a special-purpose work
 *   queue that we use to keep track of the objects yet to be processed in
 *   our depth-first search across the inheritance tree.  
 */
class CVmObjTadsInhQueue
{
public:
    CVmObjTadsInhQueue()
    {
        init();
    }

    void init()
    {
        /* there's nothing in the free list or the queue yet */
        head_ = 0;
        free_ = 0;

        /* we have no elements yet */
        alloc_ = 0;
    }

    ~CVmObjTadsInhQueue()
    {
        pfq_page *cur;
        pfq_page *nxt;

        /* delete all of the allocated pages */
        for (cur = alloc_ ; cur != 0 ; cur = nxt)
        {
            /* remember the next page */
            nxt = cur->nxt;

            /* free this page */
            t3free(cur);
        }
    }

    /* get the head of the queue */
    pfq_ele *get_head() const { return head_; }

    /* remove the head of the queue and return the object ID */
    vm_obj_id_t remove_head()
    {
        /* if there's a head element, remove it */
        if (head_ != 0)
        {
            pfq_ele *ele;

            /* note the element */
            ele = head_;

            /* unlink it from the list */
            head_ = head_->nxt;

            /* link the element into the free list */
            ele->nxt = free_;
            free_ = ele;

            /* return the object ID from the element we removed */
            return ele->obj;
        }
        else
        {
            /* there's nothing in the queue */
            return VM_INVALID_OBJ;
        }
    }

    /* clear the queue */
    void clear()
    {
        /* move everything from the queue to the free list */
        while (head_ != 0)
        {
            pfq_ele *cur;

            /* unlink this element from the queue */
            cur = head_;
            head_ = cur->nxt;

            /* link it into the free list */
            cur->nxt = free_;
            free_ = cur;
        }
    }

    /* determine if the queue is empty */
    int is_empty() const
    {
        /* we're empty if there's no head element in the list */
        return (head_ == 0);
    }

    /* allocate a path from the contents of the queue */
    tadsobj_objid_and_ptr *create_path() const
    {
        /* count the elements in the queue */
        int cnt;
        pfq_ele *cur;
        for (cnt = 0, cur = head_ ; cur != 0 ; cur = cur->nxt)
        {
            /* only non-nil elements count */
            if (cur->obj != VM_INVALID_OBJ)
                ++cnt;
        }

        /* allocate the path */
        tadsobj_objid_and_ptr *path = (tadsobj_objid_and_ptr *)t3malloc(
            (cnt + 1) * sizeof(tadsobj_objid_and_ptr));

        /* initialize the path */
        tadsobj_objid_and_ptr *dst;
        for (dst = path, cur = head_ ; cur != 0 ; cur = cur->nxt)
        {
            /* only store non-nil elements */
            if (cur->obj != VM_INVALID_OBJ)
            {
                dst->id = cur->obj;
                dst->objp = cur->objp;
                ++dst;
            }
        }

        /* set the null last entry */
        dst->id = VM_INVALID_OBJ;
        dst->objp = 0;

        /* return the new path */
        return path;
    }

    /*
     *   Insert an object into the queue.  We'll insert after the given
     *   element (null indicates that we insert at the head of the queue).
     *   Returns a pointer to the newly-inserted element.  
     */
    pfq_ele *insert_obj(VMG_ vm_obj_id_t obj, CVmObjTads *objp,
                        pfq_ele *ins_pt)
    {
        pfq_ele *ele;

        /*
         *   If the exact same element is already in the queue, delete the
         *   old copy.  This will happen in situations where we have multiple
         *   superclasses that all inherit from a common base class: we want
         *   the common base class to come in inheritance order after the
         *   last superclass that inherits from the common base.  By deleting
         *   previous queue entries that match new queue entries, we ensure
         *   that the common class will move to follow (in inheritance order)
         *   the last class that derives from it.  
         */
        for (ele = head_ ; ele != 0 ; ele = ele->nxt)
        {
            /* if this is the same thing we're inserting, remove it */
            if (ele->obj == obj)
            {
                /* 
                 *   clear the element (don't unlink it, as this could cause
                 *   confusion for the caller, who's tracking an insertion
                 *   point and traversal point) 
                 */
                ele->obj = VM_INVALID_OBJ;
                ele->objp = 0;

                /* 
                 *   no need to look any further - we know we can never have
                 *   the same element appear twice in the queue, thanks to
                 *   this very code 
                 */
                break;
            }
        }

        /* allocate our new element */
        ele = alloc_ele();
        ele->obj = obj;
        ele->objp = objp;

        /* insert it at the insertion point */
        if (ins_pt == 0)
        {
            /* insert at the head */
            ele->nxt = head_;
            head_ = ele;
        }
        else
        {
            /* insert after the selected item */
            ele->nxt = ins_pt->nxt;
            ins_pt->nxt = ele;
        }

        /* return the new element */
        return ele;
    }

protected:
    /* allocate a new element */
    pfq_ele *alloc_ele()
    {
        pfq_ele *ele;

        /* if we have nothing in the free list, allocate more elements */
        if (free_ == 0)
        {
            pfq_page *pg;
            size_t i;

            /* allocate another page */
            pg = (pfq_page *)t3malloc(sizeof(pfq_page));

            /* link it into our master page list */
            pg->nxt = alloc_;
            alloc_ = pg;

            /* link all of its elements into the free list */
            for (ele = pg->eles, i = sizeof(pg->eles)/sizeof(pg->eles[0]) ;
                 i != 0 ; --i, ++ele)
            {
                /* link this one into the free list */
                ele->nxt = free_;
                free_ = ele;
            }
        }

        /* take the next element off the free list */
        ele = free_;
        free_ = free_->nxt;

        /* return the element */
        return ele;
    }

    /* head of the active queue */
    pfq_ele *head_;

    /* head of the free element list */
    pfq_ele *free_;

    /*
     *   Linked list of element pages.  We allocate memory for elements in
     *   blocks, to reduce allocation overhead.  
     */
    pfq_page *alloc_;
};




#endif /* VMTOBJ_H */

/*
 *   Register the classes 
 */
VM_REGISTER_METACLASS(CVmObjTads)
VM_REGISTER_METACLASS(CVmObjIntClsMod)

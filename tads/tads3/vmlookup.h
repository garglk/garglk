/* $Header$ */

/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmlookup.h - LookupTable metaclass
Function
  
Notes
  
Modified
  02/06/01 MJRoberts  - Creation
*/

#ifndef VMLOOKUP_H
#define VMLOOKUP_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmcoll.h"
#include "vmiter.h"


/* ------------------------------------------------------------------------ */
/*
 *   The image file data block is arranged as follows:
 *   
 *.  UINT2 bucket_count
 *.  UINT2 value_count
 *.  UINT2 first_free_index
 *.  UINT2 bucket_index[1]
 *.  UINT2 bucket_index[2]
 *.  ...
 *.  UINT2 bucket_index[N]
 *.  value[1]
 *.  value[2]
 *.  ...
 *.  value[N]
 *.  --- the following are only used in version 030003 and later ---
 *.  DATAHOLDER default_value
 *   
 *   value_count gives the number of value slots allocated.  Free value slots
 *   are kept in a linked list, the head of which is at the 1-based index
 *   given by first_free_index.  If first_free_index is zero, there are no
 *   free value slots.
 *   
 *   Each bucket_index[i] is the 1-based index of the first value in the
 *   chain for that hash bucket.  If the value in a bucket_index[i] is zero,
 *   there are values for that bucket.
 *   
 *   Each free entry has a VM_EMPTY value stored in its key to indicate that
 *   it's empty.
 *   
 *   Each value[i] looks like this:
 *   
 *.  DATAHOLDER key
 *.  DATAHOLDER value
 *.  UINT2 next_index
 *   
 *   next_index gives the 1-based index of the next value in the chain for
 *   that bucket; a value of zero indicates that this is the last value in
 *   the chain.
 *   
 *   For version 030003 and later, additional data MAY be supplied.  This can
 *   be detected by checking the data block size: if more bytes follow the
 *   last value slot, the extended version 030003 elements are present.
 *   
 *   default_value is the default value to return when the table is indexed
 *   by a key that doesn't exist in the table.  This is nil by default.  
 */

/* value entry size */
#define VMLOOKUP_VALUE_SIZE  (VMB_DATAHOLDER + VMB_DATAHOLDER + VMB_UINT2)

/* ------------------------------------------------------------------------ */
/*
 *   in-memory value entry structure 
 */
struct vm_lookup_val
{
    /* the key */
    vm_val_t key;

    /* the value */
    vm_val_t val;

    /* next entry in same hash bucket */
    vm_lookup_val *nxt;
};

/*
 *   Our in-memory extension data structure, which mimics the image file
 *   structure but uses native types.  
 */
struct vm_lookup_ext
{
    /* allocate the structure, given the number of buckets and values */
    static vm_lookup_ext *alloc_ext(VMG_ class CVmObjLookupTable *self,
                                    uint bucket_cnt, uint value_cnt);

    /* 
     *   Initialize the extension - puts all values into the free list and
     *   clears all buckets.  We don't do this automatically as part of
     *   allocation, because some types of allocation set up the buckets and
     *   free list from a known data set and thus are more efficient if they
     *   skip the initialization step. 
     */
    void init_ext();

    /* 
     *   Reallocate the structure with a larger number of values.  Copies
     *   all of the data from the original hash table into the new hash
     *   table, and deletes the old structure.  
     */
    static vm_lookup_ext *expand_ext(VMG_ class CVmObjLookupTable *self,
                                     vm_lookup_ext *old_ext,
                                     uint new_value_cnt);

    /* 
     *   Copy the given extension's data into myself.  This can only be used
     *   when we have the same bucket count as the original (the entry count
     *   need not be the same, but it must be large enough to hold all of
     *   the data from the original).
     *   
     *   This loses any data previously in the table.  
     */
    void copy_ext_from(vm_lookup_ext *old_ext);

    /* allocate a value entry out of my free list */
    vm_lookup_val *alloc_val_entry()
    {
        /* if the free list is empty, return failure */
        if (first_free == 0)
            return 0;

        /* take the first item off the free list */
        vm_lookup_val *entry = first_free;

        /* unlink it from the free list */
        first_free = first_free->nxt;

        /* return the allocated item */
        return entry;
    }

    /* 
     *   Add a value into the given hash bucket.  The caller is responsible
     *   for ensuring there's enough room. 
     */
    void add_val(uint hash, const vm_val_t *key, const vm_val_t *val)
    {
        /* allocate a new entry */
        vm_lookup_val *entry = alloc_val_entry();

        /* set it up with the new data */
        entry->key = *key;
        entry->val = *val;

        /* link it into the given bucket */
        entry->nxt = buckets[hash];
        buckets[hash] = entry;
    }

    /* 
     *   Given a pool index, retrieve a value entry from our pool of value
     *   entries.  This has nothing to do with the hash bucket lists or the
     *   free list - this is simply the nth entry in the master pool of all
     *   values.  
     */
    vm_lookup_val *idx_to_val(uint idx) const
    {
        vm_lookup_val *pool;

        /* the pool of values starts immediately after the buckets */
        pool = (vm_lookup_val *)(void *)&buckets[bucket_cnt];

        /* return the nth element of the pool */
        return &pool[idx];
    }

    /* given a value entry, get the pool index */
    uint val_to_idx(vm_lookup_val *val) const
    {
        return (val - idx_to_val(0));
    }

    /* 
     *   Convert an image-file or save-file index to a value pointer.  These
     *   are given as 1-based pointers, with the special value zero used to
     *   indicate a null pointer. 
     */
    vm_lookup_val *img_idx_to_val(uint idx) const
    {
        if (idx == 0)
            return 0;
        else
            return idx_to_val(idx - 1);
    }

    /* convert a value pointer to an image file index */
    uint val_to_img_idx(vm_lookup_val *val)
    {
        /* 
         *   use zero for a null pointer; otherwise, use a 1-based index in
         *   our master value pool 
         */
        if (val == 0)
            return 0;
        else
            return (val - idx_to_val(0)) + 1;
    }

    /* number of buckets and number of allocated value entries */
    uint bucket_cnt;
    uint value_cnt;

    /* pointer to the first free value */
    vm_lookup_val *first_free;

    /* 
     *   default value - this is returned when we index the table by a key
     *   that doesn't exist in the table 
     */
    vm_val_t default_value;

    /* 
     *   buckets (we overallocate the structure to make room): each bucket
     *   points to the first entry in the list of entries at this hash value 
     */
    vm_lookup_val *buckets[1];
};


/* ------------------------------------------------------------------------ */
/* 
 *   undo action codes 
 */
enum lookuptab_undo_action
{
    /* 
     *   null record - we use this to mark a record that has become
     *   irrelevant because of a stale weak reference 
     */
    LOOKUPTAB_UNDO_NULL,

    /* we added this word to the dictionary (undo by deleting it) */
    LOOKUPTAB_UNDO_ADD,

    /* we deleted this word from the dictionary (undo by adding it back) */
    LOOKUPTAB_UNDO_DEL,

    /* we modified the value for a given key */
    LOOKUPTAB_UNDO_MOD,

    /* we changed the default value for the table */
    LOOKUPTAB_UNDO_DEFVAL
};


/* ------------------------------------------------------------------------ */
/*
 *   LookupTable metaclass 
 */

class CVmObjLookupTable: public CVmObjCollection
{
    friend class CVmObjIterLookupTable;
    friend class CVmMetaclassLookupTable;
    friend class CVmObjWeakRefLookupTable;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObjCollection::is_of_metaclass(meta));
    }

    /* is this a lookup table object? */
    static int is_lookup_table_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              uint bucket_count, uint init_capacity);

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
        return CVmObjCollection::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self)
    {
        /* we cannot be converted to constant data */
    }

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* 
     *   receive savepoint notification - we don't keep any
     *   savepoint-relative records, so we don't need to do anything here 
     */
    void notify_new_savept() { }

    /* apply an undo record */
    void apply_undo(VMG_ struct CVmUndoRecord *rec);

    /* discard an undo record */
    void discard_undo(VMG_ struct CVmUndoRecord *);

    /* mark undo references */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *rec);

    /* mark references */
    void mark_refs(VMG_ uint);

    /* we keep only strong references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* reload from an image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* 
     *   determine if we've been changed since loading - assume we have (if
     *   we haven't, the only harm is the cost of unnecessarily reloading or
     *   saving) 
     */
    int is_changed_since_load() const { return TRUE; }

    /* get an entry; returns true if the entry exists, false if not */
    int index_check(VMG_ vm_val_t *result, const vm_val_t *index_val);

    /* get a value by index */
    int index_val_q(VMG_ vm_val_t *result,
                    vm_obj_id_t self,
                    const vm_val_t *index_val);

    /* set a value by index */
    int set_index_val_q(VMG_ vm_val_t *new_container,
                        vm_obj_id_t self,
                        const vm_val_t *index_val,
                        const vm_val_t *new_val);
    
    /* add an entry - does not generate undo */
    void add_entry(VMG_ const vm_val_t *key, const vm_val_t *val);

    /* set or add an entry - does not generate undo */
    void set_or_add_entry(VMG_ const vm_val_t *key, const vm_val_t *val);

    /* make a list of keys/values in the table */
    void keys_to_list(VMG_ vm_val_t *lst,
                      int (*filter)(VMG_ const vm_val_t *, const vm_val_t *)
                      = 0)
        { make_list(vmg_ lst, TRUE, filter); }
    void vals_to_list(VMG_ vm_val_t *lst,
                      int (*filter)(VMG_ const vm_val_t *, const vm_val_t *)
                      = 0)
        { make_list(vmg_ lst, FALSE, filter); }

    /* iterate over the table's contents through a callback */
    void for_each(VMG_ void (*cb)(VMG_ const vm_val_t *key,
                                  const vm_val_t *val, void *ctx), void *ctx);

    /* get the default value */
    void get_default_val(vm_val_t *val);

protected:
    /* get and range-check the constructor arguments */
    static void get_constructor_args(VMG_ uint argc,
                                     size_t *bucket_count,
                                     size_t *init_capacity,
                                     vm_val_t *src_obj);

    /* populate the table from a Key->Value list or vector */
    void populate_from_list(VMG_ const vm_val_t *src_obj);

    /* load or reload image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* create a new object as a copy of this object */
    vm_obj_id_t create_copy(VMG0_);
    
    /* add an entry, generating undo */
    void add_entry_undo(VMG_ vm_obj_id_t self,
                        const vm_val_t *key, const vm_val_t *val);

    /* delete an entry - does not generate undo */
    void del_entry(VMG_ const vm_val_t *key);

    /* 
     *   Unlink an entry - does not generate undo.  'prv_entry' is the
     *   previous entry in the hash chain containing this entry, and
     *   'hashval' is the bucket containing the entry.  Pass null for
     *   'prv_entry' when the entry to unlink is the first entry in its hash
     *   chain.  
     */
    void unlink_entry(VMG_ vm_lookup_val *entry, uint hashval,
                      vm_lookup_val *prv_entry);
    
    /* 
     *   modify an entry - changes the value associated with the given key;
     *   does not generate undo 
     */
    void mod_entry(VMG_ const vm_val_t *key, const vm_val_t *val);

    /* find an entry */
    vm_lookup_val *find_entry(VMG_ const vm_val_t *key,
                              uint *hashval_p, vm_lookup_val **prv_entry_p);
    
    /*
     *   Check the table to make sure there's enough free space to add one
     *   new item, and expand the table if necessary.  
     */
    void expand_if_needed(VMG0_);

    /* allocate a new entry, expanding the table if necessary */
    vm_lookup_val *alloc_new_entry(VMG0_);

    /* calculate a value's hash code */
    uint calc_key_hash(VMG_ const vm_val_t *key);

    /* get my extension data */
    vm_lookup_ext *get_ext() const { return (vm_lookup_ext *)ext_; }

    /* get the hash bucket count */
    uint get_bucket_count() const { return get_ext()->bucket_cnt; }

    /* get the value entry count */
    uint get_entry_count() const { return get_ext()->value_cnt; }

    /* get/set the first-free item */
    vm_lookup_val *get_first_free() const { return get_ext()->first_free; }
    void set_first_free(vm_lookup_val *p) { get_ext()->first_free = p; }

    /* get a bucket's first entry given a hash code */
    vm_lookup_val **get_bucket(uint hash) const
        { return &get_ext()->buckets[hash]; }

    /* set a bucket's contents given a hash code */
    void set_bucket(uint hash, vm_lookup_val *p)
        { get_ext()->buckets[hash] = p; }

    /* set an entry, keeping undo for the change */
    void set_entry_val_undo(VMG_ vm_obj_id_t self,
                            vm_lookup_val *entry, const vm_val_t *val);

    /* create an iterator */
    virtual void new_iterator(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val);

    /* create a live iterator */
    virtual void new_live_iterator(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val);

    /* create a lookup table with no initial contents */
    CVmObjLookupTable() { ext_ = 0; }

    /* 
     *   Create a lookup table with a given number of hash table buckets,
     *   and the given number of entry slots.  The hash table bucket count
     *   is fixed for the life of the object.  The entry slot count is
     *   merely advisory: the table size will be increased as necessary to
     *   accommodate new elements.  
     */
    CVmObjLookupTable(VMG_ size_t hash_count, size_t entry_count);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - remove an entry given the key */
    int getp_remove_entry(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - determine if a given key is in the table */
    int getp_key_present(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - apply a callback to each element */
    int getp_apply_all(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - call a callback on each element */
    int getp_for_each(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - call a callback on each element */
    int getp_for_each_assoc(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* general forEach/forEachAssoc processor */
    int for_each_gen(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc,
                     int pass_key_to_cb, struct vm_rcdesc *rc);

    /* get the number of buckets in the table */
    int getp_count_buckets(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* get the number of entries in the table */
    int getp_count_entries(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* make a list of all of the keys in the table */
    int getp_keys_to_list(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* make a list of all of the values in the table */
    int getp_vals_to_list(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);
    
    /* get the default value */
    int getp_get_def_val(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* set the default value */
    int getp_set_def_val(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* general handler for making a list of keys or values */
    int getp_make_list(VMG_ vm_obj_id_t self,
                       vm_val_t *retval, uint *argc, int store_keys);

    /* get the nth key */
    int getp_nthKey(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* get the nth value */
    int getp_nthVal(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* 
     *   Get the nth element's key and/or value.  This only counts in-use
     *   buckets.  If idx is zero, we return the default value.  If idx is
     *   out of range, we throw an error. 
     */
    void get_nth_ele(VMG_ vm_val_t *key, vm_val_t *val,
                     vm_obj_id_t self, long idx);

    /* make a list of the keys or values in the table */
    void make_list(VMG_ vm_val_t *retval, int store_keys,
                   int (*filter)(VMG_ const vm_val_t *, const vm_val_t *));

    /* add a record to the global undo stream */
    void add_undo_rec(VMG_ vm_obj_id_t self,
                      enum lookuptab_undo_action action,
                      const vm_val_t *key,
                      const vm_val_t *old_entry_val);

    /* property evaluation function table */
    static int (CVmObjLookupTable::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   WeakRefLookupTable - a subclass of LookupTable that places weak
 *   references on its values.  The keys are still strong references. 
 */
class CVmObjWeakRefLookupTable: public CVmObjLookupTable
{
    friend class CVmMetaclassWeakRefLookupTable;

public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObjLookupTable::is_of_metaclass(meta));
    }

    /* create */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              uint bucket_count, uint init_capacity);

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
        return CVmObjLookupTable::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* mark references */
    void mark_refs(VMG_ uint);

    /* mark undo references */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *rec);

    /* remove stale weak references */
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *rec);

    /* remove stale weak references */
    void remove_stale_weak_refs(VMG0_);

protected:
    /* create a lookup table with no initial contents */
    CVmObjWeakRefLookupTable() { ext_ = 0; }

    /* construct */
    CVmObjWeakRefLookupTable(VMG_ size_t hash_count, size_t entry_count)
        : CVmObjLookupTable(vmg_ hash_count, entry_count) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   LookupTable Registration table object 
 */
class CVmMetaclassLookupTable: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "lookuptable/030003"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjLookupTable();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjLookupTable();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjLookupTable::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjLookupTable::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* I'm a Collection object */
    CVmMetaclass *get_supermeta_reg() const
        { return CVmObjCollection::metaclass_reg_; }
};

/*
 *   WeakRefLookupTable registration object
 */
class CVmMetaclassWeakRefLookupTable: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "weakreflookuptable/030001"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
        { new (vmg_ id) CVmObjWeakRefLookupTable(); }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
        { new (vmg_ id) CVmObjWeakRefLookupTable(); }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjWeakRefLookupTable::
        create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjWeakRefLookupTable::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* I'm a LookupTable object */
    CVmMetaclass *get_supermeta_reg() const
        { return CVmObjLookupTable::metaclass_reg_; }
};

/* ------------------------------------------------------------------------ */
/*
 *   LookupTable Iterator subclass.  This iterator is tightly coupled with
 *   the LookupTable class, since we must look into the internal
 *   implementation of the LookupTable to implement an iterator on it.  
 */

/*
 *   The extension data for an lookup table iterator consists of a reference
 *   to the associated LookupTable object, the current bucket index, and the
 *   index of the current entry.
 *   
 *   DATAHOLDER lookuptable_value
 *.  UINT2 cur_entry_index (1-based; zero is invalid)
 *.  UINT2 flags
 *   
 *   The flag values are:
 *   
 *   VMOBJITERLOOKUPTABLE_UNDO - we've saved undo for this savepoint.  If
 *   this is set, we won't save additional undo for the same savepoint.  
 */

/* total extension size */
#define VMOBJITERLOOKUPTABLE_EXT_SIZE  (VMB_DATAHOLDER + 2 + 2)

/* 
 *   flag bits 
 */

/* we've saved undo for the current savepoint */
#define VMOBJITERLOOKUPTABLE_UNDO   0x0001

/*
 *   LookupTable iterator class 
 */
class CVmObjIterLookupTable: public CVmObjIter
{
    friend class CVmMetaclassIterLookupTable;

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
     *   Create a lookup table iterator.  This method is to be called by the
     *   lookup table to create an iterator for its value.  
     */
    static vm_obj_id_t create_for_coll(VMG_ const vm_val_t *coll);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* 
     *   notify of a new savepoint - clear the 'undo' flag, since we cannot
     *   have created any undo information yet for the new savepoint 
     */
    void notify_new_savept()
        { set_flags(get_flags() & ~VMOBJITERLOOKUPTABLE_UNDO); }

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

    /* reload from an image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* 
     *   determine if the object has been changed since it was loaded -
     *   assume we have 
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

protected:
    /* create */
    CVmObjIterLookupTable() { ext_ = 0; }

    /* create */
    CVmObjIterLookupTable(VMG_ const vm_val_t *coll);

    /* find the first valid entry, starting at the given index */
    uint find_first_valid_entry(VMG_ uint entry) const;

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

    /* get information on the current entry */
    void get_cur_entry(VMG_ vm_val_t *valp, vm_val_t *keyp) const;

    /* get my collection value */
    void get_coll_val(vm_val_t *val) const { vmb_get_dh(ext_, val); }

    /* get/set the current entry index (without saving undo) */
    long get_entry_index() const
        { return osrp2(ext_ + VMB_DATAHOLDER); }
    void set_entry_index(uint idx)
        { oswp2(ext_ + VMB_DATAHOLDER, idx); }

    /* update the entry index value, saving undo if necessary */
    void set_entry_index_undo(VMG_ vm_obj_id_t self, uint entry);

    /* get/set the flags */
    uint get_flags() const
        { return osrp2(ext_ + VMB_DATAHOLDER + 2); }
    void set_flags(uint flags) const
        { oswp2(ext_ + VMB_DATAHOLDER + 2, flags); }
};

/*
 *   Registration table object for lookup table iterators
 */
class CVmMetaclassIterLookupTable: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const
        { return "lookuptable-iterator/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjIterLookupTable();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjIterLookupTable();
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
        return CVmObjIterLookupTable::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};


#endif /* VMLOOKUP_H */

/*
 *   Register the classes
 */
VM_REGISTER_METACLASS(CVmObjLookupTable)
VM_REGISTER_METACLASS(CVmObjWeakRefLookupTable)
VM_REGISTER_METACLASS(CVmObjIterLookupTable)


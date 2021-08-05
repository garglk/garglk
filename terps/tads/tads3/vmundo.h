/* $Header: d:/cvsroot/tads/tads3/VMUNDO.H,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmundo.h - VM undo manager
Function
  
Notes
  
Modified
  10/29/98 MJRoberts  - Creation
*/

#ifndef VMUNDO_H
#define VMUNDO_H

#include "t3std.h"
#include "vmtype.h"


/* ------------------------------------------------------------------------ */
/*
 *   Undo record.  The VM undo manager maintains a list of these records;
 *   each record contains the information necessary to undo one particular
 *   data change in an object.  Each object class (TADS object, array,
 *   etc) keeps a linked list of undo records for its own actions.  
 */
struct CVmUndoRecord
{
    /*
     *   Object ID.  All undoable state is stored in objects, hence each
     *   undo record is associated with an object.
     */
    vm_obj_id_t obj;

    /* 
     *   Identifier - the meaning of this member is defined by the object
     *   that created the record.  For TADS objects, this is a property ID
     *   specifying the property that was changed; for arrays, it's the
     *   index of the array element that changed.  Other object
     *   metaclasses may assign different meanings.  
     */
    union
    {
        /* property value key */
        vm_prop_id_t prop;

        /* integer value key */
        uint32_t intval;

        /* pointer value key */
        void *ptrval;
    } id;

    /*
     *   Data value.  This is the data value prior to the change.  In some
     *   cases, the type may be 'empty' to indicate that the original
     *   value did not exist; for example, when a property is added to a
     *   TADS object and the property was not present previously, we
     *   create an undo record with type 'empty' to indicate that the
     *   property must be deleted on undo.  
     */
    vm_val_t oldval;
};


/*
 *   Undo meta-record.  Each slot in the undo array is one of these
 *   meta-records, which is a union of the normal undo record and an undo
 *   link pointer.  The first undo record in any savepoint is always a
 *   link pointer; all of the other records are normal undo records. 
 */
union CVmUndoMeta
{
    /* the first entry in each savepoint is a link entry */
    struct
    {
        /* pointer to the first record in the previous savepoint */
        CVmUndoMeta *prev_first;

        /* pointer to the first record in the next savepoint */
        CVmUndoMeta *next_first;
    } link;
    
    /* every entry but the first in a savepoint is an ordinary undo record */
    CVmUndoRecord rec;
};


/* ------------------------------------------------------------------------ */
/*
 *   Undo manager 
 */
class CVmUndo
{
public:
    /* 
     *   create the undo manager, specifying the upper limit for memory
     *   usage and retained savepoints 
     */
    CVmUndo(size_t undo_record_cnt, uint max_savepts);

    /* delete the undo manager */
    ~CVmUndo();

    /* 
     *   Create a savepoint.  If doing so would exceed the maximum number
     *   of savepoints, we'll discard the oldest savepoint. 
     */
    void create_savept(VMG0_);

    /* get the current savepoint ID */
    vm_savept_t get_cur_savept() const { return cur_savept_; }

    /* determine how many savepoints exist */
    uint get_savept_cnt() const { return savept_cnt_; }

    /* drop all undo information */
    void drop_undo(VMG0_);

    /*
     *   Allocate and initialize an undo record with a property key or
     *   with an integer key.
     *   
     *   We check that the undo manager has an active savepoint.  It may
     *   not have an active savepoint if no savepoint has been created,
     *   all undo records have been applied, or we've run out of space in
     *   the current savepoint; in any of these cases, since there's no
     *   savepoint, we don't need to keep any undo records.  
     */
    void add_new_record_prop_key(VMG_ vm_obj_id_t obj, vm_prop_id_t key,
                                 const vm_val_t *val);
    void add_new_record_int_key(VMG_ vm_obj_id_t obj, uint32_t key,
                                const vm_val_t *val);

    /* 
     *   Add a new undo record with a pointer key; returns true if the
     *   record was created, false if not.  (This version of
     *   add_new_record_xxx_key returns an indication of success because
     *   the pointer value might have been allocated, in which case the
     *   caller must deallocate the value if we didn't add an undo
     *   record.) 
     */
    int add_new_record_ptr_key(VMG_ vm_obj_id_t obj, void *key,
                               const vm_val_t *val);

    /* add a new record with a pointer key and no separate value data */
    int add_new_record_ptr_key(VMG_ vm_obj_id_t obj, void *key)
    {
        /* set up a nil value to fill the value slot in the record */
        vm_val_t nilval;
        nilval.set_nil();

        /* save the record */
        return add_new_record_ptr_key(vmg_ obj, key, &nilval);
    }

    /*
     *   Apply undo to the latest savepoint.  After applying the undo
     *   records, we delete all undo records to the savepoint; this leaves
     *   the previous savepoint as the new most recent savepoint, so
     *   repeating this will undo to the previous savepoint.  
     */
    void undo_to_savept(VMG0_);

    /*
     *   Garbage collection: mark referenced objects.  This goes through
     *   all of the undo records; for each undo record, we have the object
     *   that owns the undo record mark as referenced any object to which
     *   the record refers. 
     */
    void gc_mark_refs(VMG0_);

    /*
     *   Garbage collection: delete obsolete weak references.  This goes
     *   through all of the undo records; for each undo record, if the
     *   object that owns the record is unreachable, we'll delete the undo
     *   record; otherwise, we'll tell the object that owns the record to
     *   clean up the record if it contains an unreachable weak reference. 
     */
    void gc_remove_stale_weak_refs(VMG0_);

private:
    /*
     *   Allocate an undo record.  Returns null if no records are
     *   available, in which case the caller should simply skip saving
     *   undo.  
     */
    CVmUndoMeta *alloc_rec(VMG0_);

    /* 
     *   increment a record pointer, wrapping back at the end of the array
     *   to the first record 
     */
    void inc_rec_ptr(CVmUndoMeta **rec)
    {
        /* increment the record pointer */
        ++(*rec);

        /* if it's at the end of the array, wrap back to the start */
        if (*rec == rec_arr_ + rec_arr_size_)
            *rec = rec_arr_;
    }

    /*
     *   Add a new record and return the new record.  If we don't have an
     *   active savepoint, this will return null, since there's no need to
     *   keep undo.  
     */
    CVmUndoRecord *add_new_record(VMG_ vm_obj_id_t controlling_obj);

    /* discard the oldest savepoint */
    void drop_oldest_savept(VMG0_);
    
    /* current savepoint ID */
    vm_savept_t cur_savept_;

    /* 
     *   Number of active savepoint ID's.  If this is zero, it means that
     *   we have no undo information and we're not keeping any undo
     *   information.  If this is one, it means that we only have the
     *   current savepoint stored.  
     */
    uint savept_cnt_;

    /*
     *   The nominal maximum number of savepoints to keep.  Any time that
     *   we attempt to create a new savepoint, and doing so would exceed
     *   this number of stored savepoints, we will discard the oldest
     *   savepoint.  Note that we may not always store this many
     *   savepoints, since we will also discard old savepoints when we run
     *   out of available undo records attempting to create a new
     *   savepoint.  
     */
    vm_savept_t max_savepts_;
    
    /* 
     *   Pointer to the first record in the current savepoint.  This isn't
     *   a real undo record -- it's a link record.  
     */
    CVmUndoMeta *cur_first_;

    /*
     *   Pointer to the first undo record in the oldest savepoint.  This
     *   is a link record. 
     */
    CVmUndoMeta *oldest_first_;

    /* pointer to the next free undo record */
    CVmUndoMeta *next_free_;

    /*
     *   Master array of undo records, and the number of records in this
     *   list.  We pre-allocate a fixed array of these records.  If we run
     *   out, we start discarding old undo.  
     */
    CVmUndoMeta *rec_arr_;
    size_t rec_arr_size_;
};


#endif /* VMUNDO_H */


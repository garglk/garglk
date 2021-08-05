#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMUNDO.CPP,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmundo.cpp - undo manager
Function
  
Notes
  
Modified
  10/29/98 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmundo.h"

/*
 *   create the undo manager 
 */
CVmUndo::CVmUndo(size_t undo_record_cnt, uint max_savepts)
{
    /* remember the maximum number of savepoints */
    max_savepts_ = (vm_savept_t)max_savepts;

    /* 
     *   initialize the savepoint to number 0 (this is arbitrary, but we
     *   might as well start here) 
     */
    cur_savept_ = 0;

    /* no savepoints have been created yet */
    savept_cnt_ = 0;

    /* create the undo record array */
    rec_arr_size_ = undo_record_cnt;
    rec_arr_ = (CVmUndoMeta *)t3malloc(rec_arr_size_
                                       * sizeof(rec_arr_[0]));

    /* we have no records at all yet, so we have no "firsts" */
    cur_first_ = 0;
    oldest_first_ = 0;

    /* start allocating from the first entry in the array */
    next_free_ = rec_arr_;
}

/*
 *   delete the undo manager 
 */
CVmUndo::~CVmUndo()
{
    /* delete the array of undo records */
    t3free(rec_arr_);
}

/*
 *   create a savepoint 
 */
void CVmUndo::create_savept(VMG0_)
{
    CVmUndoMeta *rec;

    /* 
     *   Allocate an undo record to serve as the link pointer for this
     *   savepoint.  If this fails, we cannot create a savepoint; we won't
     *   need to do anything else in this case, since we will have deleted
     *   all existing savepoints if we ran out of records.  
     */
    rec = alloc_rec(vmg0_);
    if (rec == 0)
        return;

    /* if we have an old savepoint, the new record starts the next one */
    if (cur_first_ != 0)
        cur_first_->link.next_first = rec;

    /* the old savepoint (if any) is our previous savepoint */
    rec->link.prev_first = cur_first_;

    /* there's nothing after us yet */
    rec->link.next_first = 0;

    /* this record is now the current savepoint's first record */
    cur_first_ = rec;

    /* 
     *   if we didn't have any savepoints recorded yet, this is now the
     *   first record in the oldest savepoint 
     */
    if (oldest_first_ == 0)
        oldest_first_ = rec;

    /* 
     *   increment the savepoint number, wrapping to zero if we reach the
     *   highest value we can store 
     */
    if (cur_savept_ == VM_SAVEPT_MAX)
        cur_savept_ = 0;
    else
        ++cur_savept_;

    /* 
     *   if we're already storing the maximum number of savepoints we're
     *   allowed to store simultaneously, drop the oldest savepoint;
     *   otherwise, simply increment the savepoint counter 
     */
    if (savept_cnt_ == max_savepts_)
    {
        /* we have the maximum number of savepoints; discard the oldest */
        drop_oldest_savept(vmg0_);
    }

    /* count the new savepoint */
    ++savept_cnt_;

    /* notify the objects that we're starting a new savepoint */
    G_obj_table->notify_new_savept();
}

/*
 *   Discard the oldest savepoint 
 */
void CVmUndo::drop_oldest_savept(VMG0_)
{
    /* if there are no savepoints, there's nothing to do */
    if (savept_cnt_ == 0)
        return;

    /* decrement the savepoint count, now that this one is gone */
    --savept_cnt_;

    /* forget the oldest lead pointer */
    if (oldest_first_ != 0)
    {
        /* 
         *   discard records from the oldest lead pointer to the next
         *   oldest lead pointer 
         */
        CVmUndoMeta *meta;
        for (meta = oldest_first_, inc_rec_ptr(&meta) ;
             meta != oldest_first_->link.next_first && meta != next_free_ ; )
        {
            /* discard this entry, if it still exists */
            if (meta->rec.obj != VM_INVALID_OBJ)
                vm_objp(vmg_ meta->rec.obj)->discard_undo(vmg_ &meta->rec);
            
            /* advance to the next record */
            inc_rec_ptr(&meta);
        }
        
        /* advance the oldest pointer to the next savepoint's first record */
        oldest_first_ = oldest_first_->link.next_first;

        /* there's now nothing before this one */
        if (oldest_first_ != 0)
            oldest_first_->link.prev_first = 0;
    }

    /* if we don't have an oldest, we also don't have a current */
    if (oldest_first_ == 0)
        cur_first_ = 0;
}

/*
 *   Drop all undo information
 */
void CVmUndo::drop_undo(VMG0_)
{
    /* delete each savepoint until we have none left */
    while (savept_cnt_ != 0)
        drop_oldest_savept(vmg0_);

    /* reset the savepoint number */
    cur_savept_ = 0;

    /* we have no "firsts" */
    cur_first_ = 0;
    oldest_first_ = 0;

    /* start allocating from the start of the array */
    next_free_ = rec_arr_;
}

/*
 *   Allocate an undo record.  If we run out of free records, delete
 *   savepoints, starting with the oldest savepoint, until we have a free
 *   record.  
 */
CVmUndoMeta *CVmUndo::alloc_rec(VMG0_)
{
    /* 
     *   keep trying until we find an entry or run out of savepoints to
     *   delete 
     */
    for (;;)
    {
        /*
         *   If we have no savepoints at all, all records are free.
         *   Otherwise, if the next free pointer hasn't bumped up against
         *   the oldest allocated record yet, we can allocate another
         *   record.  
         */
        if (savept_cnt_ == 0 || next_free_ != oldest_first_)
        {
            CVmUndoMeta *ret;
            
            /* remember the current free record */
            ret = next_free_;

            /* 
             *   advance to the next record, wrapping if we reach the end
             *   of the array (since we treat the array as circular) 
             */
            inc_rec_ptr(&next_free_);

            /* return the record */
            return ret;
        }
        
        /* 
         *   If we have at least one old savepoint, discard the oldest
         *   savepoint, which may free up some records, and try again;
         *   otherwise, give up.  
         */
        if (savept_cnt_ > 1)
        {
            /* 
             *   we have at least one old savepoint we can discard - get
             *   rid of it and try again 
             */
            drop_oldest_savept(vmg0_);
        }
        else
        {
            /* 
             *   we are down to our last savepoint (or none at all) - it's
             *   no longer valid (since we can't keep it complete due to
             *   the lack of available records), so delete it and return
             *   failure 
             */
            drop_oldest_savept(vmg0_);
            return 0;
        }
    }
}

/*
 *   Add a new record with a property ID key 
 */
void CVmUndo::add_new_record_prop_key(VMG_ vm_obj_id_t obj, vm_prop_id_t key,
                                      const vm_val_t *val)
{
    CVmUndoRecord *rec;

    /* allocate the new record */
    rec = add_new_record(vmg_ obj);

    /* if we successfully allocated a record, set it up */
    if (rec != 0)
    {
        /* set the object */
        rec->obj = obj;
        
        /* set the key */
        rec->id.prop = key;

        /* set the value */
        rec->oldval = *val;
    }
}

/*
 *   Add a new record with an integer key 
 */
void CVmUndo::add_new_record_int_key(VMG_ vm_obj_id_t obj, uint32_t key,
                                     const vm_val_t *val)
{
    CVmUndoRecord *rec;

    /* allocate the new record */
    rec = add_new_record(vmg_ obj);

    /* if we successfully allocated a record, set it up */
    if (rec != 0)
    {
        /* set the object */
        rec->obj = obj;
        
        /* set the key */

        rec->id.intval = key;

        /* set the value */
        rec->oldval = *val;
    }
}

/*
 *   Add a new record with a pointer key
 */
int CVmUndo::add_new_record_ptr_key(VMG_ vm_obj_id_t obj, void *key,
                                    const vm_val_t *val)
{
    CVmUndoRecord *rec;

    /* allocate the new record */
    rec = add_new_record(vmg_ obj);

    /* if we successfully allocated a record, set it up */
    if (rec != 0)
    {
        /* set the object */
        rec->obj = obj;

        /* set the key */
        rec->id.ptrval = key;

        /* set the value */
        rec->oldval = *val;
    }

    /* return an indication as to whether the record was created */
    return (rec != 0);
}

/*
 *   Add a new undo record 
 */
CVmUndoRecord *CVmUndo::add_new_record(VMG_ vm_obj_id_t controlling_obj)
{
    CVmUndoMeta *meta;
    
    /* if there is not an active savepoint, do not keep undo */
    if (savept_cnt_ == 0)
        return 0;

    /* 
     *   if the object was created after the current savepoint was created,
     *   there is no need to keep undo for it during this savepoint, since
     *   rolling back to the savepoint will roll back to a point at which
     *   the object didn't even exist 
     */
    if (!G_obj_table->is_obj_in_undo(controlling_obj))
        return 0;
    
    /* allocate a new record */
    meta = alloc_rec(vmg0_);
    
    /* return the undo record, if we succeeded */
    return (meta == 0 ? 0 : &meta->rec);
}

/*
 *   Apply undo to the most recent savepoint.
 */
void CVmUndo::undo_to_savept(VMG0_)
{
    CVmUndoMeta *meta;
    
    /* if we don't have any savepoints, there's nothing to do */
    if (savept_cnt_ == 0)
        return;

    /* 
     *   Starting with the most recently-added record, apply each record
     *   in sequence until we reach the first savepoint in the undo list.  
     */
    meta = next_free_;
    for (;;)
    {
        /* 
         *   move to the previous record, wrapping to the end of the array
         *   at the first record (since we treat the array as circular) 
         */
        if (meta == rec_arr_)
            meta = rec_arr_ + rec_arr_size_;
        --meta;

        /* 
         *   if we're at the first record in the current savepoint, we're
         *   done 
         */
        if (meta == cur_first_)
            break;

        /* apply this undo record */
        G_obj_table->apply_undo(vmg_ &meta->rec);
    }

    /*
     *   Unwind the undo stack -- get the first record in the previous
     *   savepoint from the link pointer. 
     */
    cur_first_ = cur_first_->link.prev_first;

    /* 
     *   there's nothing following the current savepoint any more -- the
     *   savepoint we just applied is gone now 
     */
    if (cur_first_ != 0)
        cur_first_->link.next_first = 0;
    else
        oldest_first_ = 0;

    /* that's one less savepoint */
    --savept_cnt_;

    /* make the previous savepoint current */
    --cur_savept_;
    if (cur_savept_ == 0)
        cur_savept_ = VM_SAVEPT_MAX;

    /* the savepoint link we just removed is now the next free record */
    next_free_ = meta;

    /* 
     *   notify objects that a new savepoint is in effect - the savepoint
     *   isn't new in the sense of being newly created, but in the sense of
     *   being a different savepoint than was previously in effect 
     */
    G_obj_table->notify_new_savept();
}

/*
 *   Garbage collection: mark referenced objects.  This goes through all
 *   of the undo records; for each undo record, we have the object that
 *   owns the undo record mark as referenced any object to which the
 *   record refers.  
 */
void CVmUndo::gc_mark_refs(VMG0_)
{
    CVmUndoMeta *cur;
    CVmUndoMeta *next_link;

    /* if we don't have any records, there's nothing to do */
    if (oldest_first_ == 0)
        return;

    /* the first record is a linking record */
    next_link = oldest_first_;

    /* start at the first record */
    cur = oldest_first_;

    /* 
     *   Go through all undo records, from the oldest to the newest.  Note
     *   that we must keep track of each "link" record, because these are
     *   not ordinary undo records and must be handled differently.  
     */
    for (;;)
    {
        /* check to see if this is a linking record or an ordinary record */
        if (cur == next_link)
        {
            /* 
             *   this is a linking record - simply note the next linking
             *   record and otherwise ignore this record 
             */
            next_link = cur->link.next_first;
        }
        else if (cur->rec.obj != VM_INVALID_OBJ)
        {
            /* 
             *   It's an ordinary undo record -- mark its references.
             *   Note that the fact that an object has undo records does
             *   not mean that the object is reachable -- this is
             *   effectively a weak link to the object, because if it's
             *   not reachable through other means, there's no need to
             *   keep any undo information for it; so, we don't mark the
             *   owner object itself as reachable at this point.  
             */
            G_obj_table->mark_obj_undo_rec(vmg_ cur->rec.obj, &cur->rec);
        }
        
        /* advance to the next record, wrapping at the end of the array */
        ++cur;
        if (cur == rec_arr_ + rec_arr_size_)
            cur = rec_arr_;

        /* stop if we've reached the last record */
        if (cur == next_free_)
            break;
    }
}

/*
 *   Garbage collection: delete obsolete weak references.  This goes
 *   through all of the undo records; for each undo record, if the object
 *   that owns the record is unreachable, we'll delete the undo record;
 *   otherwise, we'll tell the object that owns the record to clean up the
 *   record if it contains an unreachable weak reference.
 */
void CVmUndo::gc_remove_stale_weak_refs(VMG0_)
{
    CVmUndoMeta *cur;
    CVmUndoMeta *next_link;

    /* if we don't have any records, there's nothing to do */
    if (oldest_first_ == 0)
        return;

    /* the first record is a linking record */
    next_link = oldest_first_;

    /* start at the first record */
    cur = oldest_first_;

    /* 
     *   Go through all undo records, from the oldest to the newest.  Note
     *   that we must keep track of each "link" record, because these are
     *   not ordinary undo records and must be handled differently.  
     */
    for (;;)
    {
        /* check to see if this is a linking record or an ordinary record */
        if (cur == next_link)
        {
            /* 
             *   this is a linking record - simply note the next linking
             *   record and otherwise ignore this record 
             */
            next_link = cur->link.next_first;
        }
        else if (cur->rec.obj != VM_INVALID_OBJ)
        {
            /* 
             *   It's an ordinary undo record -- delete its stale
             *   references.  If the record owner is itself about to be
             *   deleted, don't bother with the object, but rather delete
             *   the entire undo record -- undo records themselves only
             *   weakly reference their owning objects.  
             */
            if (!G_obj_table->is_obj_deletable(cur->rec.obj))
            {
                /* it's not ready for deletion - clean up its weak refs */
                G_obj_table->remove_obj_stale_undo_weak_ref(
                    vmg_ cur->rec.obj, &cur->rec);
            }
            else
            {
                /* 
                 *   it's no longer reachable - delete the undo record by
                 *   setting the owning object to 'invalid' 
                 */
                cur->rec.obj = VM_INVALID_OBJ;
            }
        }

        /* advance to the next record, wrapping at the end of the array */
        ++cur;
        if (cur == rec_arr_ + rec_arr_size_)
            cur = rec_arr_;

        /* stop if we've reached the last record */
        if (cur == next_free_)
            break;
    }
}


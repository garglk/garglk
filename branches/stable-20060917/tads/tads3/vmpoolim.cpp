/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpool.cpp - constant pool - in-memory (non-swapping) implementation
Function
  
Notes
  
Modified
  10/20/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>

#include "t3std.h"
#include "vmpool.h"


/* ------------------------------------------------------------------------ */
/*
 *   In-memory pool implementation.  This pool implementation pre-loads
 *   all available pages in the pool and keeps the complete pool in memory
 *   at all times. 
 */

/*
 *   delete the pool's resources - this is called from our destructor, and
 *   can also be called explicitly to reset the pool 
 */
void CVmPoolInMem::terminate_nv()
{
    /* free any pages we allocated from the backing store */
    free_backing_pages();

    /* free all of the dynamic object handles */
    while (dyn_head_ != 0)
    {
        CVmPoolDynObj *nxt;

        /* note the next object */
        nxt = dyn_head_->get_next();

        /* delete this object */
        t3free(dyn_head_);

        /* move on to the next one */
        dyn_head_ = nxt;
    }

    /* we no longer have anything in the list */
    dyn_head_ = dyn_tail_ = 0;

    /* we no longer have any dynamic pages */
    first_dyn_page_ = 0;
}

/*
 *   free pages that we allocated from the backing store 
 */
void CVmPoolInMem::free_backing_pages()
{
    size_t i;
    pool_ofs_t ofs;
    CVmPool_pg *info;

    /* 
     *   delete any dynamically-allocated pages (these are not managed by
     *   the backing store) 
     */
    for (i = first_dyn_page_, info = &pages_[i] ; i < page_slots_ ;
         ++i, ++info)
    {
        /* if this slot was allocated, delete it */
        if (info->mem != 0)
        {
            /* free the memory */
            t3free((char *)info->mem);

            /* note that we've freed it */
            info->mem = 0;
        }
    }

    /* if there's no backing store, there's nothing to do */
    if (backing_store_ == 0)
        return;

    /* 
     *   Run through the page array and delete each allocated page.  Since
     *   we allocate pages through the backing store, delete pages through
     *   the backing store.  
     */
    for (i = 0, info = pages_, ofs = 0 ; i < first_dyn_page_ ;
         ++i, ++info, ofs += page_size_)
    {
        /* if this slot was allocated, delete it */
        if (info->mem != 0)
        {
            /* delete the page */
            backing_store_->vmpbs_free_page(info->mem, ofs, page_size_);

            /* forget it */
            info->mem = 0;
        }
    }
}

/*
 *   initialize 
 */
void CVmPoolInMem::attach_backing_store(CVmPoolBackingStore *backing_store)
{
    size_t ofs;
    size_t i;
    CVmPool_pg *info;
    
    /* do the normal initialization to allocate the page slots */
    CVmPoolPaged::attach_backing_store(backing_store);

    /* 
     *   set the initial dynamic page index - this will be the first page
     *   after all of the fixed pages we load from the backing store 
     */
    first_dyn_page_ = page_slots_;

    /* load all of the pages */
    for (i = 0, info = pages_, ofs = 0 ; i < page_slots_ ;
         ++i, ++info, ofs += page_size_)
    {
        size_t load_size;
        
        /* determine how much memory we really need for this page */
        load_size = backing_store_->vmpbs_get_page_size(ofs, page_size_);
        
        /* allocate and load the page */
        info->mem = backing_store_->vmpbs_alloc_and_load_page(
            ofs, page_size_, load_size);
    }
}

/*
 *   Detach from the backing store 
 */
void CVmPoolInMem::detach_backing_store()
{
    /* release the backing pages */
    free_backing_pages();

    /* inherit default */
    CVmPoolPaged::detach_backing_store();
}

/* ------------------------------------------------------------------------ */
/*
 *   Dynamic interface implementation 
 */

/*
 *   Allocate space in the dynamic pool 
 */
CVmPoolDynObj *CVmPoolInMem::dynpool_alloc(size_t len)
{
    CVmPoolDynObj *cur;

    /* 
     *   if the requested size exceeds the page size, we can't allocate
     *   it, since each request must fit within a single page 
     */
    if (len > page_size_)
        return 0;
    
    /*
     *   First, see if we can find a free pool object that we've already
     *   allocated that will fill the request 
     */
    for (cur = dyn_head_ ; cur != 0 ; cur = cur->get_next())
    {
        /* if this object is free, and it's big enough, use it */
        if (cur->is_free() && cur->get_len() >= len)
        {
            /* 
             *   if this object is at least a little bigger than the
             *   request, create a new object to hold the balance of this
             *   object, so that the balance can be allocated separately 
             */
            if (cur->get_len() > len + 63)
            {
                CVmPoolDynObj *new_obj;

                /* 
                 *   create a new object, using the space remaining in the
                 *   old object after the requested amount of space 
                 */
                new_obj = new CVmPoolDynObj(cur->get_ofs() + len,
                                            cur->get_len() - len);

                /* reduce the old object to the requested size */
                cur->set_len(len);

                /* link the new object in after the old object */
                insert_dyn(cur, new_obj);

                /* the new object is free */
                new_obj->set_free(TRUE);
            }

            /* this object is now in use */
            cur->set_free(FALSE);

            /* return the object we found */
            return cur;
        }
    }

    /*
     *   We didn't find any free memory in any existing pages where we
     *   could fill the request.  So, we must allocate a new page.  First,
     *   allocate a new page slot.  
     */
    alloc_page_slots(page_slots_ + 1);

    /* allocate space for the page data */
    pages_[page_slots_ - 1].mem = (char *)t3malloc(page_size_);

    /* 
     *   if the requested size wouldn't leave much additional space on the
     *   page, simply give the entire page to the new object 
     */
    if (len + 63 >= page_size_)
        len = page_size_;

    /* create a new dynamic pool handle for the new object */
    cur = new CVmPoolDynObj(get_page_start_ofs(page_slots_ - 1), len);

    /* link it in at the end of the list */
    append_dyn(cur);

    /* 
     *   if there's any space left over, create yet another object to
     *   cover the free space remaining on the page 
     */
    if (len < page_size_)
    {
        CVmPoolDynObj *f;
        
        /* create a new dynamic pool handle for the new object */
        f = new CVmPoolDynObj(get_page_start_ofs(page_slots_ - 1) + len,
                              page_size_ - len);

        /* mark it as free */
        f->set_free(TRUE);

        /* link it in at the end of the list */
        append_dyn(f);
    }

    /* return the new object */
    return cur;
}

/*
 *   Delete a dynamic object 
 */
void CVmPoolInMem::dynpool_delete(CVmPoolDynObj *obj)
{
    CVmPoolDynObj *cur;
    size_t pg;

    /* mark this object as free */
    obj->set_free(TRUE);

    /* note the page containing this object */
    pg = get_page_for_ofs(obj->get_ofs());
    
    /*
     *   Combine this object with any consecutive objects in the same
     *   page.  First, look for objects before this object. 
     */
    for (cur = obj->get_prev() ;
         cur != 0 && cur->is_free()
             && get_page_for_ofs(cur->get_ofs()) == pg ;
         cur = cur->get_prev())
    {
        /* 
         *   cur precedes obj, is on the same page, and is also free -
         *   combine cur and obj into cur
         */
        cur->set_len(cur->get_len() + obj->get_len());

        /* unlink obj from the list, since it's no longer needed */
        unlink_dyn(obj);

        /* delete the original object */
        delete obj;

        /* forget about obj - cur now encompasses it */
        obj = cur;
    }

    /*
     *   Now combine any consecutive objects that follow 
     */
    for (cur = obj->get_next() ;
         cur != 0 && cur->is_free()
             && get_page_for_ofs(cur->get_ofs()) == pg ; )
    {
        /* 
         *   cur follows obj, is on the same page, and is also free -
         *   combine cur and obj into obj
         */
        obj->set_len(obj->get_len() + cur->get_len());

        /* unlink cur from the list, since it's no longer needed */
        unlink_dyn(cur);
        
        /* delete the second object */
        delete cur;

        /* move on to the next object */
        cur = obj->get_next();
    }
}

/*
 *   Compress the dynamic portion of the pool 
 */
void CVmPoolInMem::dynpool_compress()
{
    // $$$
}

/*
 *   Append a dynamic handle to the end of our list 
 */
void CVmPoolInMem::append_dyn(CVmPoolDynObj *obj)
{
    /* this goes at the end, so there's nothing after this object */
    obj->set_next(0);

    /* the old tail will precede this object in the list */
    obj->set_prev(dyn_tail_);

    /* 
     *   if the list is empty, this is the new head; otherwise, set the
     *   old tail's forward pointer to the new item 
     */
    if (dyn_tail_ == 0)
        dyn_head_ = obj;
    else
        dyn_tail_->set_next(obj);

    /* this is now the tail item */
    dyn_tail_ = obj;
}

/*
 *   Unlink a dynamic handle from the list 
 */
void CVmPoolInMem::unlink_dyn(CVmPoolDynObj *obj)
{
    /* 
     *   if there's a previous item, set its forward pointer; otherwise,
     *   advance the list head past the item we're unlinking 
     */
    if (obj->get_prev() != 0)
        obj->get_prev()->set_next(obj->get_next());
    else
        dyn_head_ = obj->get_next();

    /* 
     *   if there's a following item, set its back pointer; otherwise,
     *   move the list tail back over the item we're unlinking 
     */
    if (obj->get_next() != 0)
        obj->get_next()->set_prev(obj);
    else
        dyn_tail_ = obj->get_prev();
}

/*
 *   Insert a dynamic handle into the list after the given object
 */
void CVmPoolInMem::insert_dyn(CVmPoolDynObj *obj, CVmPoolDynObj *new_obj)
{
    /* the old following object (after obj) will now follow new_obj */
    new_obj->set_next(obj->get_next());

    /* obj will precede new_obj */
    new_obj->set_prev(obj);

    /* new_obj will follow the old object */
    obj->set_next(new_obj);

    /* 
     *   if another object follows, set its back pointer to point to the
     *   new object; otherwise, set the list tail pointer 
     */
    if (new_obj->get_next() != 0)
        new_obj->get_next()->set_prev(new_obj);
    else
        dyn_tail_ = new_obj;
}

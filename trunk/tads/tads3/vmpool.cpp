/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpool.cpp - constant pool implementation
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
 *   Basic pool implementation 
 */

/*
 *   Get the number of pages in the pool 
 */
size_t CVmPool::get_page_count() const
{
    /* get the page count from the backing store */
    return backing_store_->vmpbs_get_page_count();
}

/*
 *   Attach to a backing store 
 */
void CVmPool::attach_backing_store(CVmPoolBackingStore *backing_store)
{
    /* remember the backing store */
    backing_store_ = backing_store;

    /* get the page size from the backing store */
    page_size_ = backing_store->vmpbs_get_common_page_size();
}

/* ------------------------------------------------------------------------ */
/*
 *   Base paged pool implementation
 */

/*
 *   delete our page list 
 */
void CVmPoolPaged::delete_page_list()
{
    /* if there's a page list, delete it */
    if (pages_ != 0)
    {
        /* free the page array */
        t3free(pages_);

        /* forget it */
        pages_ = 0;
        page_slots_ = 0;
        page_slots_max_ = 0;
    }

    /* we can no longer have a backing store */
    backing_store_ = 0;
}

/*
 *   Attach to a backing store
 */
void CVmPoolPaged::attach_backing_store(CVmPoolBackingStore *backing_store)
{
    size_t cur;
    size_t log2;

    /* delete any existing page list */
    delete_page_list();

    /* inherit default handling */
    CVmPool::attach_backing_store(backing_store);

    /* 
     *   if the page size is zero, there must not be any pages at all -
     *   use a dummy default page size
     */
    if (page_size_ == 0)
        page_size_ = 1024;

    /*
     *   Compute log2 of the page size.  If the page size isn't a power of
     *   two, throw an error. 
     */
    for (cur = page_size_, log2 = 0 ; (cur & 1) == 0 ; cur >>= 1, ++log2) ;
    if (cur != 1)
        err_throw(VMERR_BAD_POOL_PAGE_SIZE);

    /* store log2(page_size_) in log2_page_size_ */
    log2_page_size_ = log2;

    /* allocate the pages */
    alloc_page_slots(backing_store_->vmpbs_get_page_count());
}

/*
 *   Allocate a page slot 
 */
void CVmPoolPaged::alloc_page_slots(size_t nslots)
{
    size_t old_slots;
    size_t i;

    /* note the old slot count */
    old_slots = page_slots_;

    /* if the new size isn't bigger than the old size, ignore the request */
    if (nslots <= page_slots_)
        return;

    /* if necessary, expand the master page array */
    if (nslots > page_slots_max_)
    {
        size_t siz;

        /* 
         *   Increase the maximum, leaving some room for dynamically added
         *   pages. 
         */
        page_slots_max_ = nslots + 10;

        /* calculate the new allocation size */
        siz = page_slots_max_ * sizeof(pages_[0]);

        /* allocate or reallocate at the new size */
        if (pages_ == 0)
            pages_ = (CVmPool_pg *)t3malloc(siz);
        else
            pages_ = (CVmPool_pg *)t3realloc(pages_, siz);
    }

    /* set the new size */
    page_slots_ = nslots;

    /* clear the new subarrays */
    for (i = old_slots ; i < page_slots_ ; ++i)
        pages_[i].mem = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Two-level paged pool implementation.  This is a variation of the regular
 *   paged pool that uses a two-level page table.  This implementation is not
 *   currently used, because it is less efficient than the single-page pool
 *   and is not needed for modern machines with large contiguous address
 *   spaces; however, we retain it in the event it's needed for 16-bit
 *   segmented architectures, where it might not be possible or convenient to
 *   allocate a sufficiently large master page table and thus a two-level
 *   table is needed.  
 */
#if 0

/*
 *   delete the pool - deletes all allocated pages 
 */
CVmPoolPaged2::~CVmPoolPaged2()
{
    /* free our page memory */
    delete_page_list();
}

/*
 *   delete our page list 
 */
void CVmPoolPaged2::delete_page_list()
{
    /* if there's a page list, delete it */
    if (pages_ != 0)
    {
        size_t i;
        size_t cnt;

        /* free each subarray */
        cnt = get_subarray_count();
        for (i = 0 ; i < cnt ; ++i)
            t3free(pages_[i]);

        /* free the master array */
        t3free(pages_);

        /* forget about the page list */
        pages_ = 0;
    }
}

/*
 *   Attach to a backing store 
 */
void CVmPoolPaged2::attach_backing_store(CVmPoolBackingStore *backing_store)
{
    size_t cur;
    size_t log2;

    /* delete any existing page list */
    delete_page_list();

    /* remember the backing store */
    backing_store_ = backing_store;

    /* get the page size from the backing store */
    page_size_ = backing_store_->vmpbs_get_common_page_size();

    /* 
     *   if the page size is zero, there must not be any pages at all - use a
     *   dummy default page size 
     */
    if (page_size_ == 0)
        page_size_ = 1024;

    /*
     *   Compute log2 of the page size.  If the page size isn't a power of
     *   two, throw an error.  
     */
    for (cur = page_size_, log2 = 0 ; (cur & 1) == 0 ; cur >>= 1, ++log2) ;
    if (cur != 1)
        err_throw(VMERR_BAD_POOL_PAGE_SIZE);

    /* store log2(page_size_) in log2_page_size_ */
    log2_page_size_ = log2;

    /* allocate the pages */
    alloc_page_slots(backing_store_->vmpbs_get_page_count());
}

/*
 *   Allocate a page slot 
 */
void CVmPoolPaged2::alloc_page_slots(size_t nslots)
{
    size_t old_slots;
    size_t old_sub_cnt;
    size_t new_sub_cnt;

    /* note the old slot count */
    old_slots = page_slots_;

    /* if the new size isn't bigger than the old size, ignore the request */
    if (nslots <= page_slots_)
        return;

    /* note the original subarray count */
    old_sub_cnt = get_subarray_count();

    /* set the new size */
    page_slots_ = nslots;

    /* note the new subarray count */
    new_sub_cnt = get_subarray_count();

    /* allocate or expand the master array */
    if (new_sub_cnt > old_sub_cnt)
    {
        size_t siz;
        size_t i;

        /* figure the new size */
        siz = new_sub_cnt * sizeof(pages_[0]);

        /* allocate or re-allocate the master array */
        if (pages_ == 0)
            pages_ = (CVmPool_pg **)t3malloc(siz);
        else
            pages_ = (CVmPool_pg **)t3realloc(pages_, siz);

        /* throw an error if that failed */
        if (pages_ == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* clear the new slots */
        memset(pages_ + old_sub_cnt, 0,
               (new_sub_cnt - old_sub_cnt) * sizeof(pages_[0]));

        /* allocate the subarrays */
        for (i = old_sub_cnt ; i < new_sub_cnt ; ++i)
        {
            /* allocate this subarray */
            pages_[i] = (CVmPool_pg *)t3malloc(VMPOOL_SUBARRAY_SIZE
                                               * sizeof(pages_[i][0]));

            /* make sure we got the space */
            if (pages_[i] == 0)
                err_throw(VMERR_OUT_OF_MEMORY);

            /* clear the page */
            memset(pages_[i], 0, VMPOOL_SUBARRAY_SIZE * sizeof(pages_[i][0]));
        }
    }
}

#endif /* removed 2-paged pool */

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
}

/*
 *   free pages that we allocated from the backing store 
 */
void CVmPoolInMem::free_backing_pages()
{
    size_t i;
    pool_ofs_t ofs;
    CVmPool_pg *info;

    /* if there's no backing store, there's nothing to do */
    if (backing_store_ == 0)
        return;

    /* 
     *   Run through the page array and delete each allocated page.  Since
     *   we allocate pages through the backing store, delete pages through
     *   the backing store.  
     */
    for (i = 0, info = pages_, ofs = 0 ; i < page_slots_ ;
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

    /* load all of the pages */
    for (i = 0, info = pages_, ofs = 0 ; i < page_slots_ ;
         ++i, ++info, ofs += page_size_)
    {
        /* determine how much memory we really need for this page */
        info->siz = backing_store_->vmpbs_get_page_size(ofs, page_size_);
        
        /* allocate and load the page */
        info->mem = backing_store_->vmpbs_alloc_and_load_page(
            ofs, page_size_, info->siz);
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

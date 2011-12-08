/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpoolfl.cpp - "flat" memory pool
Function
  Implements the flat memory pool, which allocates the entire pool's
  memory in a single contiguous chunk.
Notes
  
Modified
  09/18/02 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>

#include "t3std.h"
#include "vmpool.h"

/*
 *   delete 
 */
CVmPoolFlat::~CVmPoolFlat()
{
    /* delete our memory block */
    if (mem_ != 0)
        t3free(mem_);
}

/*
 *   Attach to the backing store 
 */
void CVmPoolFlat::attach_backing_store(CVmPoolBackingStore *backing_store)
{
    size_t i;
    char *dst;
    size_t pg_cnt;
    pool_ofs_t ofs;
    
    /* inherit the base implementation */
    CVmPool::attach_backing_store(backing_store);

    /* delete any existing memory */
    if (mem_ != 0)
        t3free(mem_);

    /* 
     *   Allocate our memory.  We need to allocate a single chunk large
     *   enough for all of the pages. 
     */
    pg_cnt = backing_store_->vmpbs_get_page_count();
    siz_ = page_size_ * pg_cnt;
    mem_ = (char *)t3malloc(siz_);
    if (mem_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* load all of the pages */
    for (i = 0, ofs = 0, dst = mem_ ; i < pg_cnt ;
         ++i, dst += page_size_, ofs += page_size_)
    {
        size_t cur_pg_siz;

        /* get the load size for this page */
        cur_pg_siz = backing_store_->vmpbs_get_page_size(ofs, page_size_);
        
        /* load this page from the backing store directly into our memory */
        backing_store_->vmpbs_load_page(ofs, page_size_, cur_pg_siz, dst);
    }
}

/*
 *   detach from the backing store 
 */
void CVmPoolFlat::detach_backing_store()
{
    /* free our memory */
    if (mem_ != 0)
    {
        t3free(mem_);
        mem_ = 0;
    }

    /* inherit default */
    CVmPool::detach_backing_store();
}


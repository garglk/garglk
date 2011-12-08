/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpool.h - VM constant pool manager
Function
  
Notes
  
Modified
  10/20/98 MJRoberts  - Creation
*/

#ifndef VMPOOL_H
#define VMPOOL_H

#include <stdlib.h>
#include <memory.h>

#include "vmtype.h"

/* include the pool selection mechanism */
#include "vmpoolsl.h"

/* ------------------------------------------------------------------------ */
/*
 *   Constant pool page information.  This structure tracks memory for one
 *   page.  
 */
struct CVmPool_pg
{
    /* memory containing the data in this page */
    const char *mem;

    /* actual size of the page data */
    size_t siz;
};

   
/* ------------------------------------------------------------------------ */
/*
 *   Constant Pool Backing Store Interface.  This is an abstract interface
 *   that pool clients must implement to provide the pool with the means
 *   of loading pages.
 *   
 *   Note that the backing store, like the pool itself, is considered
 *   read-only by the pool manager.  The pool manager never needs to write
 *   data to the backing store, and expects the backing store to remain
 *   constant throughout the existence of the pool (hence the pool never
 *   needs to reload any data from the backing store that it already has
 *   in cache).  
 */
class CVmPoolBackingStore
{
public:
    /* 
     *   since this class is abstract, make sure all subclasses have virtual
     *   destructors 
     */
    virtual ~CVmPoolBackingStore() { }
    
    /*
     *   Determine the total number of pages that are available to be
     *   loaded.  Implementations of the pool manager that pre-load all
     *   pages use this function to determine how many pages are available
     *   for loading.  
     */
    virtual size_t vmpbs_get_page_count() = 0;

    /*
     *   Get the common page size in the underying store.  Individual
     *   pages may not use the entire page size, but no page may be larger
     *   than the common size.  
     */
    virtual size_t vmpbs_get_common_page_size() = 0;

    /*
     *   Given a starting offset and a page size, calculate how much space is
     *   actually needed for the page at the offset.  This is provided to
     *   allow for partial pages, which don't need the full page size
     *   allocated.  Simple implementations can simply always return the full
     *   page size.  
     */
    virtual size_t vmpbs_get_page_size(pool_ofs_t ofs, size_t page_size) = 0;

    /*
     *   Given a starting offset, allocate space for the given page and
     *   load it into memory.  page_size is the normal page size in bytes,
     *   and load_size is the actual number of bytes to be allocated and
     *   loaded (this will be the value previously returned by
     *   vmpbs_get_page_size for the page).
     *   
     *   This should throw an exception if an error occurs.  
     */
    virtual const char
        *vmpbs_alloc_and_load_page(pool_ofs_t ofs, size_t page_size,
                                   size_t load_size) = 0;

    /*
     *   Delete memory allocated by vmpbs_alloc_and_load_page().  The pool
     *   will call this for each page previously allocated.  'page_size'
     *   is the normal page size in bytes for the entire pool.  
     */
    virtual void vmpbs_free_page(const char *mem, pool_ofs_t ofs,
                                 size_t page_size) = 0;

    /*
     *   Given a starting offset, load the page into the given memory,
     *   which is allocated and managed by the caller.  page_size is the
     *   normal page size in bytes, and load_size is the actual number of
     *   bytes to be loaded (this will be the value previously returned by
     *   vmpbs_get_page_size for the page).
     *   
     *   This should throw an exception if an error occurs.  
     */
    virtual void vmpbs_load_page(pool_ofs_t ofs, size_t page_size,
                                 size_t load_size, char *mem) = 0;

    /*
     *   Determine if my pages are writable.  Returns true if so, false if
     *   not.  If the pool pages are directly mapped to the underlying
     *   data file, this should return false.  For example, an
     *   implementation for a palm-top computer without an external
     *   storage device might simply store the image file directly in
     *   memory, and the backing store would map directly onto this memory
     *   so that the original copy of the image file in memory can be used
     *   as the loaded version as well.  In such cases, the backing store
     *   should certainly not be writable.  For an implementation that
     *   copies data from an external storage device (typically a hard
     *   disk), writing to the backing store copy would cause no change to
     *   the original image file data, hence this can return true in such
     *   cases.  
     */
    virtual int vmpbs_is_writable() { return FALSE; }
};


/* ------------------------------------------------------------------------ */
/*
 *   Base constant memory pool class 
 */
class CVmPool
{
public:
    CVmPool() { }
    virtual ~CVmPool() { }

    /* 
     *   Attach to the given backing store to provide the the page data.  
     */
    virtual void
        attach_backing_store(class CVmPoolBackingStore *backing_store) = 0;

    /* 
     *   Detach from backing store - this must be called before the backing
     *   store object can be deleted.  
     */
    virtual void detach_backing_store() { backing_store_ = 0; }

    /* 
     *   Translate an address from a pool offset to a physical location.
     *   Note that translating an address may invalidate a previously
     *   translated address in a swapping implementation of the pool manager,
     *   so callers should take care to assume only one translated address in
     *   a given pool is valid at a time.
     *   
     *   Because this routine is called extremely frequently, we don't make
     *   it a virtual.  Instead, we depend upon the final subclass to define
     *   the method as a non-virtual, so that it can be in-lined.  This means
     *   that pool object references must all be declared with the final
     *   subclass.  
     */
    /* virtual const char *get_ptr(pool_ofs_t ofs) = 0; */

    /*
     *   Given a pointer into physical memory, get the pool offset.  Returns
     *   true if the pointer is a valid pointer into the pool, false if not.
     */
    /* virtual int get_ofs(const char *p, pool_ofs_t *ofs) = 0; */

    /* 
     *   Get the page size.  This reflects the size of the pages in the
     *   backing store (usually the image file); this doesn't necessarily
     *   indicate anything about the way we manage the pool memory. 
     */
    size_t get_page_size() const { return page_size_; }

    /* get the number of pages in the pool */
    size_t get_page_count() const;

    /* validate an offset */
    /* virtual int validate_ofs(pool_ofs_t ofs) = 0; */

protected:
    /* 
     *   page size in bytes - this is simply the number of bytes on each page
     *   (each page in the pool has the same number of bytes) 
     */
    size_t page_size_;

    /* backing store */
    class CVmPoolBackingStore *backing_store_;
};

/* ------------------------------------------------------------------------ */
/*
 *   "Flat" memory pool.  This type of pool loads all pages into a single
 *   contiguous chunk of memory.  This is suitable for platforms with large
 *   linear memory spaces, such as 32-bit platforms.
 */
class CVmPoolFlat: public CVmPool
{
public:
    CVmPoolFlat()
    {
        /* we don't have our memory chunk yet */
        mem_ = 0;
    }
    ~CVmPoolFlat();

    /* terminate - we don't need to do anything here */
    void terminate() { }

    /* attach to the backing store - loads all pages */
    void attach_backing_store(class CVmPoolBackingStore *backing_store);

    /* detach from the backing store */
    void detach_backing_store();

    /* 
     *   Translate an address.  Since all of our memory is in one large
     *   contiguous chunk, this is extremely simple: just return the base of
     *   our memory block, offset by the pool offset.  
     */
    inline const char *get_ptr(pool_ofs_t ofs) { return mem_ + ofs; }

    /* validate an address */
    inline int validate_ofs(pool_ofs_t ofs)
        { return (ofs > 0 && ofs < (pool_ofs_t)siz_); }

    /* get the pool offset given a pointer */
    inline int get_ofs(const char *p, pool_ofs_t *ofs)
    {
        /* if it's in our memory block, it's a valid pointer */
        if (p >= mem_ && p < mem_ + siz_)
        {
            *ofs = p - mem_;
            return TRUE;
        }
        else
            return FALSE;
    }

    /* our single contiguous allocation block */
    char *mem_;

    /* total size of the memory block */
    long siz_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Paged constant pool.
 *   
 *   This type of pool is divided into pages.  A given object must be
 *   entirely contained in a single page.
 *   
 *   Each object is referenced by its address in the constant pool.  An
 *   object address is simply an offset into the pool.  
 */
class CVmPoolPaged: public CVmPool
{
public:
    /* create the pool */
    CVmPoolPaged()
    {
        /* no page slots allocated yet */
        pages_ = 0;
        page_slots_ = 0;
        page_slots_max_ = 0;

        /* we don't have a backing store yet */
        backing_store_ = 0;

        /* we don't know the page size yet */
        page_size_ = 0;
        log2_page_size_ = 0;
    }

    /* 
     *   Delete the pool.  Call our non-virtual termination routine, as we
     *   can't use virtuals in destructors (not in the normal fashion,
     *   anyway).  
     */
    virtual ~CVmPoolPaged() { terminate_nv(); }

    /* delete everything in the pool using our base terminator routine */
    virtual void terminate() { terminate_nv(); }

    /* 
     *   Attach to the given backing store to provide the the page data.  
     */
    virtual void
        attach_backing_store(class CVmPoolBackingStore *backing_store);

protected:
    /* non-virtual termination routine */
    void terminate_nv()
    {
        /* free our page memory */
        delete_page_list();
    }
    
    /* delete our page list, if any */
    void delete_page_list();

    /* allocate or expand the page slot list */
    void alloc_page_slots(size_t slots);
    
    /* 
     *   Calculate which page we need, and the offset within the page, for
     *   a given pool offset.  The page is the offset divided by the page
     *   size; since the page size is a power of two, this is simply a bit
     *   shift by log2(page_size).  The page offset is the remainder after
     *   dividing the offset by the page size; again because the page size
     *   is a power of two, we can calculate this remainder simply by
     *   AND'ing the offset with the page size minus one.  (Using these
     *   shift and mask operations may seem a little obscure, but it's so
     *   much faster on most machines than integer division that we're
     *   willing to be a little obscure in exchange for the performance
     *   gain.)  
     */
    inline size_t get_page_for_ofs(pool_ofs_t ofs) const
    {
        return (size_t)(ofs >> log2_page_size_);
    }

    inline size_t get_ofs_for_ofs(pool_ofs_t ofs) const
    {
        return (size_t)(ofs & (page_size_ - 1));
    }

    /* get the starting offset on the given page */
    pool_ofs_t get_page_start_ofs(size_t pg) const
    {
        return ((pool_ofs_t)pg) << log2_page_size_;
    }

    /*
     *   The page list.  This is an array of CVmPool_pg structures; each
     *   structure keeps track of one page in the pool. 
     *   
     *   The page identified by the first page information structure contains
     *   pool offsets 0 through (page_size - 1); the next contains offsets
     *   page_size through (2*page_size - 1); and so on.  
     */
    CVmPool_pg *pages_;

    /* 
     *   The number of page slots in the page list.  This starts at the
     *   initial page size and can grow dynamically as more pages are added.
     */
    size_t page_slots_;

    /* 
     *   The maximum of allocated pages_ array entries.  This might be larger
     *   than page_slots_, because we sometimes allocate more slots than we
     *   currently need to avoid having to allocate on every new page
     *   addition.  
     */
    size_t page_slots_max_;

    /* log2 of the page size */
    int log2_page_size_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Two-tiered paged pool.  This is similar to the paged pool
 *   implementation, but uses a two-level page table: the first-level page
 *   table containers pointers to the second-level tables, and the
 *   second-level tables contain the pointers to the actual pages.
 *   
 *   This class is not currently used, because the two-level scheme isn't
 *   required in practice for modern machines and is less efficient than the
 *   single-level page table implemented in CVmPoolPaged.  We retain this
 *   two-level code in case it's ever needed, though, because the two-level
 *   scheme might be useful for 16-bit segmented architectures.
 *   
 *   The advantage of the two-level scheme is that it allows very large
 *   memory spaces to be addressable without any single large allocations;
 *   the single-tier paged pool requires a single allocation equal to the
 *   total aggregate memory size divided by the page size times the size of a
 *   page pointer, which could be a fairly large single allocation for an
 *   extremely large aggregate pool size.  However, it doesn't currently
 *   appear that the single-tier paging scheme will impose any limits that
 *   will be encountered in actual practice.  
 */
#if 0

/* number of page information structures in each subarray */
const size_t VMPOOL_SUBARRAY_SIZE = 4096;

class CVmPoolPaged2
{
public:
    /* create the pool */
    CVmPoolPaged2()
    {
        /* no page slots allocated yet */
        pages_ = 0;
        page_slots_ = 0;

        /* we don't have a backing store yet */
        backing_store_ = 0;

        /* we don't know the page size yet */
        page_size_ = 0;
        log2_page_size_ = 0;
    }

    /* delete the pool */
    virtual ~CVmPoolPaged2();

    /* 
     *   Attach to the given backing store to provide the the page data.  
     */
    virtual void
        attach_backing_store(class CVmPoolBackingStore *backing_store);

protected:
    /* delete our page list, if any */
    void delete_page_list();

    /* allocate or expand the page slot list */
    void alloc_page_slots(size_t slots);

    /* 
     *   Calculate which page we need, and the offset within the page, for a
     *   given pool offset.  The page is the offset divided by the page size;
     *   since the page size is a power of two, this is simply a bit shift by
     *   log2(page_size).  The page offset is the remainder after dividing
     *   the offset by the page size; again because the page size is a power
     *   of two, we can calculate this remainder simply by AND'ing the offset
     *   with the page size minus one.  (Using these shift and mask
     *   operations may seem a little obscure, but it's so much faster on
     *   most machines than integer division that we're willing to be a
     *   little obscure in exchange for the performance gain.)  
     */
    inline size_t get_page_for_ofs(pool_ofs_t ofs) const
    {
        return (size_t)(ofs >> log2_page_size_);
    }

    inline size_t get_ofs_for_ofs(pool_ofs_t ofs) const
    {
        return (size_t)(ofs & (page_size_ - 1));
    }

    /* get the starting offset on the given page */
    pool_ofs_t get_page_start_ofs(size_t pg) const
    {
        return ((pool_ofs_t)pg) << log2_page_size_;
    }

    /* get the number of subarrays */
    size_t get_subarray_count() const
        { return ((page_slots_ + VMPOOL_SUBARRAY_SIZE - 1)
                  / VMPOOL_SUBARRAY_SIZE); }

    /* get the page information structure for a given index */
    inline CVmPool_pg *get_page_info(size_t idx) const
        { return &(pages_[idx / VMPOOL_SUBARRAY_SIZE]
                   [idx % VMPOOL_SUBARRAY_SIZE]); }

    /*
     *   The page list.  Each entry in this array is a subarray containing
     *   VMPOOL_SUBARRAY_SIZE page information structures, each of contains
     *   information on a page.  Conceptually, the two-tiered array forms a
     *   single array; we keep two levels of arrays to accommodate 16-bit
     *   machines where a single large could be too large for a single 64k
     *   segment.
     *   
     *   The page identified by the first page information structure contains
     *   pool offsets 0 through (page_size - 1); the next contains offsets
     *   page_size through (2*page_size - 1); and so on.  
     */
    CVmPool_pg **pages_;

    /* 
     *   The number of slots allocated in the page list.  This starts at
     *   the initial page size and can grow dynamically as more pages are
     *   added. 
     */
    size_t page_slots_;

    /* log2 of the page size */
    int log2_page_size_;
};
#endif /* 0 */

/* ------------------------------------------------------------------------ */
/*
 *   In-memory pool implementation.  This pool implementation pre-loads
 *   all available pages in the pool and keeps the complete pool in memory
 *   at all times.  
 */
class CVmPoolInMem: public CVmPoolPaged
{
public:
    CVmPoolInMem() { }

    /* 
     *   delete - call our non-virtual terminator (use the non-virtual
     *   version, as this will just do our local termination; since we'll
     *   implicitly inherit the base class destructor, we don't want to
     *   explicitly inherit its termination as well) 
     */
    ~CVmPoolInMem() { terminate_nv(); }

    /* terminate */
    void terminate()
    {
        /* call our own non-virtual termination routine */
        terminate_nv();

        /* inherit our base class handling */
        CVmPoolPaged::terminate();
    }

    /* attach to the backing store - loads all pages */
    void attach_backing_store(class CVmPoolBackingStore *backing_store);

    /* detach from the backing store */
    void detach_backing_store();

    /* 
     *   translate an address - since the pool is always in memory, we can
     *   translate an address simply by doing the arithmetic and finding
     *   the needed page, which is always loaded 
     */
    inline const char *get_ptr(pool_ofs_t ofs)
    {
        /* translate the address through our page array */
        return (pages_[get_page_for_ofs(ofs)].mem + get_ofs_for_ofs(ofs));
    }

    /* validate an offset value */
    inline int validate_ofs(pool_ofs_t ofs)
    {
        /* get the page and the offset in the page */
        size_t pg = get_page_for_ofs(ofs);
        size_t pgofs = get_ofs_for_ofs(ofs);

        /* 
         *   to be valid, it must be within the range of valid pages, the
         *   page must be allocated, and the offset in the page must be
         *   within the page's actual allocated size 
         */
        return (pg < page_slots_
                && pages_[pg].mem != 0
                && pgofs < pages_[pg].siz);
    }

    /* get the pool offset given a pointer */
    inline int get_ofs(const char *p, pool_ofs_t *ofs)
    {
        /* check each page */
        pool_ofs_t page_ofs = 0;
        for (size_t i = 0 ; i < page_slots_ ; ++i, page_ofs += page_size_)
        {
            /* if it's in this page, it's a valid address */
            const char *mem = pages_[i].mem;
            if (p >= mem && p < mem + pages_[i].siz)
            {
                *ofs = (p - mem) + page_ofs;
                return TRUE;
            }
        }

        /* didn't find it */
        return FALSE;
    }

private:
    /* non-virtual termination */
    void terminate_nv();

    /* free any pages we allocated from the backing store */
    void free_backing_pages();
};

#endif /* VMPOOL_H */


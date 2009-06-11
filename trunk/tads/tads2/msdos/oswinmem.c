#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/oswinmem.c,v 1.1 1999/05/29 15:51:03 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oswinmem.c - windows memory management functions
Function
  
Notes
  
Modified
  05/25/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "os.h"

/* ------------------------------------------------------------------------ */
/*
 *   Windows memory management functions.  Our memory manager keeps track
 *   of all allocations made with osmalloc(); this allows us to free all
 *   such allocations, so that we can start over with all memory freed.  
 */

/*
 *   memory block prefix - each block we allocate has this prefix attached
 *   just before the pointer that we return to the program 
 */
typedef struct mem_prefix_t
{
    long id;
    size_t siz;
    struct mem_prefix_t *nxt;
    struct mem_prefix_t *prv;
} mem_prefix_t;

/* head and tail of memory allocation linked list */
static mem_prefix_t *mem_head = 0;
static mem_prefix_t *mem_tail = 0;

/*
 *   Allocate a block, storing it in a doubly-linked list of blocks and
 *   giving the block a unique ID.  
 */
void *oss_win_malloc(size_t siz)
{
    static long id;

    mem_prefix_t *mem = (mem_prefix_t *)malloc(siz + sizeof(mem_prefix_t));
    if (mem == 0)
        return 0;
    
    mem->id = id++;
    mem->siz = siz;
    mem->prv = mem_tail;
    mem->nxt = 0;
    if (mem_tail)
        mem_tail->nxt = mem;
    else
        mem_head = mem;
    mem_tail = mem;

    return (void *)(mem + 1);
}

/*
 *   reallocate a block - to simplify, we'll allocate a new block, copy
 *   the old block up to the smaller of the two block sizes, and delete
 *   the old block 
 */
void *oss_win_realloc(void *oldptr, size_t newsiz)
{
    void *newptr;
    size_t oldsiz;

    /* allocate a new block */
    newptr = oss_win_malloc(newsiz);

    /* copy the old block into the new block */
    oldsiz = (((mem_prefix_t *)oldptr) - 1)->siz;
    memcpy(newptr, oldptr, (oldsiz <= newsiz ? oldsiz : newsiz));

    /* free the old block */
    oss_win_free(oldptr);

    /* return the new block */
    return newptr;
}


/* free a block, removing it from the allocation block list */
void oss_win_free(void *ptr)
{
    static int check = 0;
    mem_prefix_t *mem = ((mem_prefix_t *)ptr) - 1;

    if (check)
    {
        mem_prefix_t *p;
        for (p = mem_head ; p ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        assert(p != 0);
    }

    if (mem->prv)
        mem->prv->nxt = mem->nxt;
    else
        mem_head = mem->nxt;

    if (mem->nxt)
        mem->nxt->prv = mem->prv;
    else
        mem_tail = mem->prv;

    free((void *)mem);
}

/*
 *   Free all memory blocks 
 */
void oss_win_free_all()
{
    /* free all blocks in our list */
    while (mem_head != 0)
    {
        mem_prefix_t *nxt;

        /* remember the next block for after when we free this block */
        nxt = mem_head->nxt;

        /* free this block */
        free((void *)mem_head);

        /* move on to the next block */
        mem_head = nxt;
    }

    /* there's no tail any more */
    mem_tail = 0;
}


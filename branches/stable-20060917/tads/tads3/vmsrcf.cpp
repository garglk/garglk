#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmsrcf.cpp - T3 VM source file list
Function
  
Notes
  
Modified
  12/01/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmsrcf.h"

/* ------------------------------------------------------------------------ */
/*
 *   Source file table implementation 
 */

/*
 *   Initialize 
 */
CVmSrcfTable::CVmSrcfTable()
{
    /* no entries yet */
    list_ = 0;
    list_used_ = 0;
    list_alloc_ = 0;
}

/*
 *   Delete 
 */
CVmSrcfTable::~CVmSrcfTable()
{
    /* delete the list if we allocated one */
    if (list_ != 0)
    {
        /* clear the table */
        clear();

        /* delete the list */
        t3free(list_);
    }
}

/*
 *   Clear the table
 */
void CVmSrcfTable::clear()
{
    size_t i;
    
    /* delete each entry */
    for (i = 0 ; i < list_used_ ; ++i)
        delete(list_[i]);

    /* 
     *   we no longer have any entries, but leave the list allocated in
     *   case we add entries again 
     */
    list_used_ = 0;
}

/*
 *   Add a new entry to the table 
 */
CVmSrcfEntry *CVmSrcfTable::add_entry(int orig_index, size_t name_len)
{
    CVmSrcfEntry *entry;
    
    /* allocate the new entry */
    entry = new CVmSrcfEntry(orig_index, (uint)orig_index == list_used_,
                             name_len);

    /* 
     *   make sure we have room in our list for a new entry; if not,
     *   expand the list 
     */
    if (list_used_ >= list_alloc_)
    {
        size_t siz;
        
        /* calculate the expanded list size */
        list_alloc_ += 10;
        siz = list_alloc_ * sizeof(list_[0]);

        /* allocate or reallocate the list */
        if (list_ == 0)
            list_ = (CVmSrcfEntry **)t3malloc(siz);
        else
            list_ = (CVmSrcfEntry **)t3realloc(list_, siz);
    }

    /* add the new entry */
    list_[list_used_] = entry;

    /* count the new entry */
    ++list_used_;

    /* return the new entry */
    return entry;
}

/* ------------------------------------------------------------------------ */
/*
 *   Source file record implementation 
 */

/*
 *   Allocate or expand the line records array
 */
void CVmSrcfEntry::alloc_line_records(ulong cnt)
{
    ulong siz;
    
    /* 
     *   if the current array size is already big enough for the given
     *   count, there's nothing to do 
     */
    if (cnt <= lines_alo_)
        return;

    /* calculate the allocation size */
    siz = cnt * sizeof(lines_[0]);

    /* 
     *   if the new size exceeds the maximum local system's architectural
     *   limit for a single allocation, restrict the size to the maximum
     *   allocation 
     */
    if (siz > OSMALMAX)
    {
        /* recalculate the size for the maximum architectural allocation */
        cnt = OSMALMAX / sizeof(lines_[0]);
        siz = cnt * sizeof(lines_[0]);

        /* if we're already there, ignore the request */
        if (cnt <= lines_alo_)
            return;
    }

    /* allocate or reallocate the line record array */
    if (lines_ == 0)
        lines_ = (CVmSrcfLine *)t3malloc((size_t)siz);
    else
        lines_ = (CVmSrcfLine *)t3realloc(lines_, (size_t)siz);

    /* remember the new array size */
    lines_alo_ = cnt;
}

/*
 *   add a line record
 */
void CVmSrcfEntry::add_line_record(ulong linenum, ulong code_addr)
{
    /* make sure we have enough space */
    if (lines_cnt_ >= lines_alo_)
    {
        /* expand the line records array */
        alloc_line_records(lines_alo_ + 1024);

        /* if that didn't create enough space, ignore the new record */
        if (lines_cnt_ >= lines_alo_)
            return;
    }

    /* add the new record */
    lines_[lines_cnt_].linenum = linenum;
    lines_[lines_cnt_].code_addr = code_addr;

    /* count the new record */
    ++lines_cnt_;
}

/*
 *   Find a source line 
 */
ulong CVmSrcfEntry::find_src_addr(ulong *linenum, int exact)
{
    long hi, lo, cur;

    /* if there are no line records, return failure */
    if (lines_cnt_ == 0)
        return 0;

    /* perform a binary search of the line record array */
    lo = 0;
    hi =  (long)lines_cnt_ - 1;
    while (lo <= hi)
    {
        int match;
        
        /* split the difference */
        cur = lo + (hi - lo)/2;

        /* 
         *   Check for a match.  If they require an exact match, we must
         *   find the line number exactly.  Otherwise, check to see if
         *   this is the next executable line after the given line. 
         */
        if (exact)
        {
            /* exact match required */
            match = (lines_[cur].linenum == *linenum);
        }
        else
        {
            /* 
             *   exact match not required - match if this is the next
             *   executable line after the requested line, or this is the
             *   last executable line and the requested line is higher 
             */
            if (cur == 0)
            {
                /* 
                 *   this is the first executable line - if the requested
                 *   line is before this one, this is our match 
                 */
                match = (*linenum <= lines_[cur].linenum);
            }
            else if (cur == (long)lines_cnt_ - 1)
            {
                /* 
                 *   this is the last executable line - if the requested
                 *   line is after this one, this is our match 
                 */
                match = (*linenum >= lines_[cur].linenum);
            }
            else
            {
                /* 
                 *   we're somewhere in the middle of the file - if the
                 *   requested line is before this one, but after the
                 *   previous executable line, this is our match 
                 */
                match = (*linenum <= lines_[cur].linenum
                         && *linenum > lines_[cur - 1].linenum);
            }
        }

        /* if we have a match, return it; otherwise, keep searching */
        if (match)
        {
            /* 
             *   if this is a non-exact match, update the caller's line
             *   number with the actual line number we found 
             */
            *linenum = lines_[cur].linenum;
            
            /* return our code address */
            return lines_[cur].code_addr;
        }
        else if (*linenum > lines_[cur].linenum)
        {
            /* we need to go higher */
            lo = (cur == lo ? cur + 1 : cur);
        }
        else
        {
            /* we need to go lower */
            hi = (cur == hi ? hi - 1 : cur);
        }
    }

    /* failure */
    return 0;
}

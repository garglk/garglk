#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmsort.cpp - T3 VM quicksort implementation
Function
  Implements quicksort.  We use our own implementation rather than the
  standard C library's qsort() routine for two reasons.  First, we might
  want to throw an exception out of the comparison routine, and it is
  not clear that it is safe to longjmp() past qsort() on every type of
  machine and every C run-time implementation.  Second, the standard C
  library's qsort() routine doesn't provide any means to pass a context
  to the comparison callback, and further insists that the data to be
  sorted be arranged as an array; we provide a higher-level abstraction
  for the comparison callback.
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include "t3std.h"
#include "vmglob.h"
#include "vmsort.h"


/* ------------------------------------------------------------------------ */
/*
 *   perform a quicksort
 */
void CVmQSortData::sort(VMG_ size_t l, size_t r)
{
    /* proceed if we have a non-empty range */
    if (r > l)
    {
        size_t i, j;
        size_t v_idx = r;

        /* start at the ends of the range */
        i = l - 1;
        j = r;

        /* partition the range */
        do
        {
            /* find the leftmost element >= the right element */
            do
            {
                ++i;
            } while (i != r && compare(vmg_ i, v_idx) < 0);

            /* find the rightmost element <= the right element */
            do
            {
                --j;
            } while (j != l && compare(vmg_ j, v_idx) > 0);

            /* exchange elements i and j */
            exchange(vmg_ i, j);

            /* if we moved the 'v' element, follow that in the index */
            if (v_idx == i)
                v_idx = j;
            else if (v_idx == j)
                v_idx = i;

        } while (j > i);

        /* undo the last exchange */
        exchange(vmg_ i, j);

        /* exchange the right value into the pivot point */
        exchange(vmg_ i, r);

        /* recursively sort the subranges */
        if (i > l)
            sort(vmg_ l, i - 1);
        if (i < r)
            sort(vmg_ i + 1, r);
    }
}


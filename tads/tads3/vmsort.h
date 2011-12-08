/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmsort.h - T3 VM quicksort implementation
Function
  
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#ifndef VMSORT_H
#define VMSORT_H

#include <stdlib.h>
#include "t3std.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   Quicksort data interface 
 */
class CVmQSortData
{
public:
    /* 
     *   sort a range; to sort the entire array, provide the indices of
     *   the first and last elements of the array, inclusive 
     */
    void sort(VMG_ size_t l, size_t r);
    
    /* 
     *   compare two elements by index - returns -1 if the first element
     *   is less than the second, 0 if they're equal, 1 if the first is
     *   greater than the second 
     */
    virtual int compare(VMG_ size_t idx_a, size_t idx_b) = 0;

    /* exchange two elements */
    virtual void exchange(VMG_ size_t idx_a, size_t idx_b) = 0;
};

/* ------------------------------------------------------------------------ */
/*
 *   Sorter implementation for sets of vm_val_t data 
 */
class CVmQSortVal: public CVmQSortData
{
public:
    CVmQSortVal()
    {
        compare_fn_.set_nil();
        descending_ = FALSE;
    }
    
    /* get/set an element */
    virtual void get_ele(VMG_ size_t idx, vm_val_t *val) = 0;
    virtual void set_ele(VMG_ size_t idx, const vm_val_t *val) = 0;
    
    /* compare */
    virtual int compare(VMG_ size_t idx_a, size_t idx_b);

    /* exchange */
    virtual void exchange(VMG_ size_t idx_a, size_t idx_b);

    /* 
     *   comparison function - if this is nil, we'll compare the values as
     *   ordinary values 
     */
    vm_val_t compare_fn_;

    /* flag: sort descending */
    int descending_;

    /* recursive native caller context */
    vm_rcdesc rc;
};

#endif /* VMSORT_H */

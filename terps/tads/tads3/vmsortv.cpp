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
  vmsortv.cpp - CVmSortVal implementation
Function
  
Notes
  
Modified
  05/14/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include "t3std.h"
#include "vmglob.h"
#include "vmsort.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmerr.h"
#include "vmerrnum.h"


/* ------------------------------------------------------------------------ */
/*
 *   compare two vm_val_t values 
 */
int CVmQSortVal::compare(VMG_ size_t a, size_t b)
{
    int result;
    vm_val_t val_a;
    vm_val_t val_b;

    /* get the two values */
    get_ele(vmg_ a, &val_a);
    get_ele(vmg_ b, &val_b);

    /* check for an explicit comparison function */
    if (compare_fn_.typ != VM_NIL)
    {
        vm_val_t val;

        /* push the values (in reverse order) */
        G_stk->push(&val_b);
        G_stk->push(&val_a);

        /* invoke the callback */
        G_interpreter->call_func_ptr(vmg_ &compare_fn_, 2, &rc, 0);

        /* get the result */
        val = *G_interpreter->get_r0();

        /* get the result value */
        result = val.num_to_int(vmg0_);
    }
    else
    {
        /* compare the values */
        result = val_a.compare_to(vmg_ &val_b);
    }

    /* if we're sorting in descending order, reverse the result */
    if (descending_)
        result = -result;

    /* return the result */
    return result;
}

/*
 *   exchange two vm_val_t elements 
 */
void CVmQSortVal::exchange(VMG_ size_t a, size_t b)
{
    vm_val_t val_a;
    vm_val_t val_b;

    /* get the two elements */
    get_ele(vmg_ a, &val_a);
    get_ele(vmg_ b, &val_b);

    /* store the two elements, swapping the positions */
    set_ele(vmg_ b, &val_a);
    set_ele(vmg_ a, &val_b);
}


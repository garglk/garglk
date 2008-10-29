#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMSTACK.CPP,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstack.cpp - VM stack implementation
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmtype.h"
#include "vmstack.h"
#include "vmfile.h"

/*
 *   allocate the stack 
 */
CVmStack::CVmStack(size_t max_depth, size_t reserve)
{
    /* 
     *   Allocate the array of stack elements.  Allocate the requested
     *   maximum depth plus the requested reserve space.  Overallocate by a
     *   few elements to leave ourselves a little buffer against mild
     *   overages - for the most part, we count on the compiler to check for
     *   proper stack usage at entry to each function, but intrinsics
     *   sometimes push a few elements without checking.  
     */
    arr_ = (vm_val_t *)t3malloc((max_depth + reserve + 25) * sizeof(arr_[0]));
    
    /* remember the maximum depth and the reserve depth */
    max_depth_ = max_depth;
    reserve_depth_ = reserve;

    /* the reserve is not yet in use */
    reserve_in_use_ = FALSE;

    /* initialize the stack pointer */
    init();
}

/*
 *   delete the stack 
 */
CVmStack::~CVmStack()
{
    /* delete the stack element array */
    t3free(arr_);
}


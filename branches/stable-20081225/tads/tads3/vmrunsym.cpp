#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmrunsym.cpp - runtime symbol table
Function
  
Notes
  
Modified
  02/17/01 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmtype.h"
#include "vmrunsym.h"

/*
 *   delete - deletes all of our symbol list entries
 */
CVmRuntimeSymbols::~CVmRuntimeSymbols()
{
    vm_runtime_sym *sym;
    vm_runtime_sym *nxt;
    
    /* delete each symbol */
    for (sym = head_ ; sym != 0 ; sym = nxt)
    {
        /* 
         *   remember the next one, since we're deleting our link pointer
         *   along with the structure 
         */
        nxt = sym->nxt;

        /* delete it */
        t3free(sym);
    }
}

/*
 *   add a symbol 
 */
void CVmRuntimeSymbols::add_sym(const char *sym, size_t len,
                                const vm_val_t *val)
{
    vm_runtime_sym *new_sym;
    
    /* allocate a new structure */
    new_sym = (vm_runtime_sym *)t3malloc(sizeof(vm_runtime_sym) + len - 1);

    /* copy the data */
    new_sym->val = *val;
    new_sym->len = len;
    memcpy(new_sym->sym, sym, len);

    /* link it into our list */
    new_sym->nxt = 0;
    if (tail_ == 0)
        head_ = new_sym;
    else
        tail_->nxt = new_sym;

    /* it's the new tail */
    tail_ = new_sym;

    /* count it */
    ++cnt_;
}

/*
 *   Find the name for a value 
 */
const char *CVmRuntimeSymbols::find_val_name(VMG_ const vm_val_t *val,
                                             size_t *name_len) const
{
    vm_runtime_sym *sym;

    /* search the table */
    for (sym = head_ ; sym != 0 ; sym = sym->nxt)
    {
        /* check for a match to the value */
        if (val->equals(vmg_ &sym->val))
        {
            /* it's a match - return the name */
            *name_len = sym->len;
            return sym->sym;
        }
    }

    /* didn't find it - return null */
    *name_len = 0;
    return 0;
}

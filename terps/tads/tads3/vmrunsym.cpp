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
    new_sym = (vm_runtime_sym *)t3malloc(osrndsz(
        sizeof(vm_runtime_sym) + len));

    /* copy the data */
    new_sym->val = *val;
    new_sym->len = len;
    memcpy(new_sym->sym = (char *)(new_sym + 1), sym, len);

    /* there are no macro parameters */
    new_sym->macro_expansion = 0;
    new_sym->macro_exp_len = 0;
    new_sym->macro_argc = 0;
    new_sym->macro_args = 0;
    new_sym->macro_flags = 0;

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
 *   Add a macro definition 
 */
vm_runtime_sym *CVmRuntimeSymbols::add_macro(const char *sym, size_t len,
                                             size_t explen,
                                             unsigned int flags, 
                                             int argc, size_t arglen)
{
    /* allocate the structure */
    vm_runtime_sym *new_sym = (vm_runtime_sym *)t3malloc(osrndsz(
        sizeof(vm_runtime_sym)
        + (argc * sizeof(char*))
        + len
        + explen
        + arglen));

    /* set up the internal allocation pointers within the structure */
    if (argc != 0)
    {
        new_sym->macro_args = (char **)(new_sym + 1);
        new_sym->macro_expansion = (char *)(new_sym->macro_args + argc);
        new_sym->macro_args[0] = new_sym->macro_expansion + explen;
        new_sym->sym = new_sym->macro_args[0] + arglen;
    }
    else
    {
        new_sym->macro_args = 0;
        new_sym->macro_expansion = (char *)(new_sym + 1);
        new_sym->sym = new_sym->macro_expansion + explen;
    }

    /* copy the basic data */
    new_sym->val.set_nil();
    new_sym->len = len;
    memcpy(new_sym->sym, sym, len);

    /* copy the basic macro data */
    new_sym->macro_argc = argc;
    new_sym->macro_flags = flags;
    new_sym->macro_exp_len = explen;

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

    /* return the new symbol */
    return new_sym;
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

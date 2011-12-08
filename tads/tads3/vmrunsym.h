/* $Header$ */

/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmrunsym.h - run-time global symbol table
Function
  Defines a symbol table structure that allows us to convey the global
  symbols from the compiler or image loader to the interpreter.

  When the compiler runs pre-initialization, or when the image loader finds
  debug symbols in a program, the interpreter builds a LookupTable object
  containing the global symbol table.  This provides a "reflection" mechanism
  that lets the running program inspect its own symbol table.

  Because we have two very different ways of getting the symbol information
  -- from the compiler, or from the image file's debug records -- we define
  this class as an intermediary that provides a common mechanism for
  conveying the information to the interpreter.  Note that, when the
  information is coming from the compiler, there's usually not a debug symbol
  table in the image file, because the compiler usually only runs
  pre-initialization on programs compiled for release (i.e., with no debug
  information), so we can't count on debug records in the image file as a
  common conveyance mechanism.

  This is a very simple linked list storage class, because we have no need
  to search this symbol table.  The only things we do with this type of
  symbol table are to load it with all of the symbols from the compiler or
  image file's debug records, and then enumerate all of our symbols to build
  a run-time LookupTable.

  One final note: we could conceivably avoid having this other data
  representation by having the compiler build a LookupTable directly and
  storing it in the image file.  However, this would put the LookupTable into
  the root set, so it could never be deleted.  By building the LookupTable
  dynamically during pre-initialization, the table will be automatically
  garbage collected before the image file is finalized if the program doesn't
  retain a reference to it in a location accessible from the root set.  This
  allows the program to control the presence of this extra information in a
  very natural way.
Notes
  
Modified
  02/17/01 MJRoberts  - Creation
*/

#ifndef VMRUNSYM_H
#define VMRUNSYM_H

#include "vmtype.h"

class CVmRuntimeSymbols
{
public:
    /* construct */
    CVmRuntimeSymbols()
    {
        /* nothing in the list yet - empty the list and clear the count */
        head_ = tail_ = 0;
        cnt_ = 0;
    }

    /* delete */
    ~CVmRuntimeSymbols();

    /* add a symbol */
    void add_sym(const char *sym, size_t len, const vm_val_t *val);

    /* add a macro definition */
    struct vm_runtime_sym *add_macro(const char *sym, size_t len,
                                     size_t explen, unsigned int flags,
                                     int argc, size_t arglen);

    /* get the first symbol */
    struct vm_runtime_sym *get_head() const { return head_; }

    /* get the number of symbols */
    size_t get_sym_count() const { return cnt_; }

    /* find an object name - returns null if the object isn't found */
    const char *find_obj_name(VMG_ vm_obj_id_t obj, size_t *name_len) const
    {
        vm_val_t val;

        /* set up an object value */
        val.set_obj(obj);

        /* find the value */
        return find_val_name(vmg_ &val, name_len);
    }

    /* find a property name - returns null if not found */
    const char *find_prop_name(VMG_ vm_prop_id_t prop, size_t *name_len) const
    {
        vm_val_t val;

        /* set up a property ID value */
        val.set_propid(prop);

        /* find the value */
        return find_val_name(vmg_ &val, name_len);
    }

    /* 
     *   Find the name for a value.  Fills in the length pointer with the
     *   length of the return string, which is not null-terminated.
     *   Returns null if no such value is present in the table.  
     */
    const char *find_val_name(VMG_ const vm_val_t *val,
                              size_t *name_len) const;

protected:
    /* head and tail of symbol list */
    struct vm_runtime_sym *head_;
    struct vm_runtime_sym *tail_;

    /* number of symbols in the list */
    size_t cnt_;
};

/*
 *   A Symbol
 */
struct vm_runtime_sym
{
    /* next symbol in the list */
    vm_runtime_sym *nxt;

    /* the symbol name */
    char *sym;

    /* the length of the name */
    size_t len;

    /* the value of the symbol */
    vm_val_t val;

    /* for a macro, the flags from the MACR record */
    unsigned int macro_flags;

    /* for a macro, the expansion of the macro */
    char *macro_expansion;
    size_t macro_exp_len;

    /* for a macro, the number of formal parameters */
    int macro_argc;

    /* for a macro, the argument list */
    char **macro_args;

    /* 
     *   Commit storage for a macro argument.  Arguments MUST be stored
     *   sequentially, starting from number 0.  The caller must
     *   null-terminate each name, AND must include the null byte in the
     *   'len' value.  
     */
    void commit_macro_arg(int i, size_t len)
    {
        /* if this isn't the last argument, set the next allocation pointer */
        if (i + 1 < macro_argc)
            macro_args[i + 1] = macro_args[i] + len;
    }
};

#endif /* VMRUNSYM_H */


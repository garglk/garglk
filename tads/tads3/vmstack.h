/* $Header: d:/cvsroot/tads/tads3/VMSTACK.H,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstack.h - VM stack manager
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#ifndef VMSTACK_H
#define VMSTACK_H

#include "vmtype.h"
#include "vmerr.h"
#include "vmerrnum.h"


/* ------------------------------------------------------------------------ */
/*
 *   The stack pointer and a few other VM registers are critical to
 *   performance, since they're accessed so frequently.  If we're compiling
 *   in VMGLOB_VARS or VMGLOB_STRUCT mode, pull out the key registers as
 *   global static variables; this results in smaller and faster code on most
 *   machines than when it's part of the CVmStack structure.  Don't do this
 *   when compiling in other global modes, since the local implementation
 *   must have a reason to keep everything in parameters (probably a shared
 *   memory situation, such as threads or a shared library).
 */
#if defined(VMGLOB_VARS) || defined(VMGLOB_STRUCT)
/* global variable mode - define registers as separate globals */
# define VM_IF_REGS_IN_STRUCT(decl)
# define VM_IF_REGS_IN_GLOBALS(decl) decl
# define VM_REG_ACCESS static
# define VM_REG_CONST
#else
/* structure mode - define registers as part of CVmStack/CVmRun */
# define VM_IF_REGS_IN_STRUCT(decl)  decl
# define VM_IF_REGS_IN_GLOBALS(decl)
# define VM_REG_ACCESS
# define VM_REG_CONST const
#endif

/* declare the stack pointer register as an extern global, if appropriate */
VM_IF_REGS_IN_GLOBALS(extern vm_val_t *sp_;)


/* ------------------------------------------------------------------------ */
/*
 *   VM stack interface
 */

class CVmStack
{
public:
    /* 
     *   Allocate the stack.  The maximum depth that the stack can achieve
     *   is fixed when the stack is created. 
     */
    CVmStack(size_t max_depth, size_t reserve_depth);

    /* initialize */
    void init()
    {
        /* start the stack pointer at the first element */
        sp_ = arr_;
    }

    /* delete the stack */
    ~CVmStack();

    /* 
     *   get the current stack depth - this gives the number of active
     *   elements in the stack
     */
    size_t get_depth() const { return sp_ - arr_; }

    /*
     *   Translate between pointer and index values.  An index value is
     *   simply an integer giving the index in the stack of the given
     *   pointer; this value can be used, for example, for saving a stack
     *   location persistently.
     *   
     *   We return zero for a null stack pointer; we always return
     *   non-zero for a non-null stack pointer.  
     */
    ulong ptr_to_index(vm_val_t *p) const
    {
        /* if it's null, return index 0; otherwise, return a non-zero index */
        return (p == 0 ? 0 : p - arr_ + 1);
    }
    vm_val_t *index_to_ptr(ulong idx) const
    {
        /* if the index is zero, it's null; otherwise, get the pointer */
        return (idx == 0 ? 0 : arr_ + idx - 1);
    }

    /*
     *   Get the depth relative to a given frame pointer.  This returns
     *   the number of items on the stack beyond the given pointer.  If
     *   the given frame pointer is beyond the current stack pointer
     *   (i.e., values have been popped since the frame pointer equalled
     *   the stack pointer), the return value will be negative. 
     */
    VM_REG_ACCESS int get_depth_rel(vm_val_t *fp) VM_REG_CONST
        { return sp_ - fp; }

    /* get the current stack pointer */
    VM_REG_ACCESS vm_val_t *get_sp() VM_REG_CONST
        { return sp_; }

    /* 
     *   Set the current stack pointer.  The pointer must always be a
     *   value previously returned by get_sp().  
     */
    VM_REG_ACCESS void set_sp(vm_val_t *p) { sp_ = p; }

    /*
     *   Get an element relative to a frame pointer (a frame pointer is a
     *   stack position that was previously obtained via get_sp() and
     *   stored by the caller).  The offset is negative for a value pushed
     *   prior to the frame pointer, zero for the value at the frame
     *   pointer, or positive for values pushed after the frame pointer.  
     */
    static vm_val_t *get_from_frame(vm_val_t *fp, int i)
        { return (fp + i - 1); }

    /* push a value */
    VM_REG_ACCESS void push(const vm_val_t *val) { *sp_++ = *val; }

    /* 
     *   Push an element, returning a pointer to the element; this can be
     *   used to fill in a new stack element directly, without copying a
     *   value.  The new element is not filled in yet on return, so the
     *   caller should immediately fill in the element with a valid value.  
     */
    VM_REG_ACCESS vm_val_t *push() { return sp_++; }

    /* 
     *   Push a number of elements: this allocates a block of contiguous
     *   stack elements that the caller can fill in individually.  The stack
     *   elements are uninitialized, so the caller must set the values
     *   immediately on return.  A pointer to the first pushed element is
     *   returned; subsequent elements are addressed at the return value
     *   plus 1, plus 2, and so on.  
     */
    VM_REG_ACCESS vm_val_t *push(unsigned int n)
    {
        /* remember the current stack pointer, which is what we return */
        vm_val_t *ret = sp_;

        /* allocate the elements */
        sp_ += n;

        /* return the base of the allocated block */
        return ret;
    }

    /* push, checking space */
    void push_check(const vm_val_t *val) { *push_check() = *val; }
    vm_val_t *push_check() { check_throw(1); return push(); }
    vm_val_t *push_check(unsigned int n) { check_throw(n); return push(n); }

    /*
     *   Insert space for 'num' slots at index 'idx'.  If 'idx' is zero, this
     *   is the same as pushing 'num' slots.  Returns a pointer to the first
     *   slot allocated.  
     */
    vm_val_t *insert(size_t idx, size_t num)
    {
        /* make sure there's room */
        check_space(num);

        /* add the space */
        sp_ += num;

        /* if idx is non-zero, move the idx slots by num to make room */
        if (idx != 0)
            memmove(sp_ - idx, sp_ - idx - num, idx*sizeof(*sp_));

        /* return the start of the inserted block */
        return sp_ - idx - num;
    }

    /* 
     *   Get an element.  Elements are numbered from zero to (depth - 1).
     *   Element number zero is the item most recently pushed onto the
     *   stack; element (depth-1) is the oldest element on the stack.  
     */
    VM_REG_ACCESS vm_val_t *get(size_t i) VM_REG_CONST
        { return (sp_ - i - 1); }

    /* pop the top element off the stack */
    VM_REG_ACCESS void pop(vm_val_t *val) { *val = *--sp_; }

    /* discard the top element */
    VM_REG_ACCESS void discard() { --sp_; }

    /* discard a given number of elements from the top of the stack */
    VM_REG_ACCESS void discard(int n) { sp_ -= n; }

    /*
     *   Probe the stack for a given allocation.  Returns true if the given
     *   number of slots are available, false if not.  Does NOT actually
     *   allocate the space; merely checks for availability.
     *   
     *   Compilers are expected to produce function headers that check for
     *   the maximum amount of stack space needed locally in the function on
     *   entry, which allows us to check once at the start of the function
     *   for available stack space, relieving us of the need to check for
     *   available space in every push operation.
     *   
     *   Returns true if the required amount of space is available, false if
     *   not.
     *   
     *   (NB: 'slots' really should be a size_t, as it's not meaningful to
     *   reserve negative space.  However, a compiler bug in 3.0.17,
     *   3.0.17.1, and 3.0.18 caused the compiler to generate the wrong
     *   reservation sizes in headers; affected code occasionally makes
     *   OPC_MAKELSTPAR compute a negative value for its size request here.
     *   Making 'slots' signed makes the result of (get_depth() + <negative>)
     *   less than the current depth, as it should be, whereas a size_t type
     *   makes it yield a huge positive value on some systems, which looks
     *   like a stack overflow.  These faulty .t3 files are generally benign
     *   despite the stack size miscalculation, because they're usually not
     *   so far off that we'd actually blow past the real memory limits of
     *   the stack, thanks to the exception-handling reserve.  In other
     *   words, we'd usually catch an actual stack overflow in a faulty .t3
     *   before it crashed the interpreter.  So it's desirable to be
     *   compatible with such files.)  
     */
    int check_space(int nslots) const
        { return (get_depth() + nslots <= max_depth_); }

    /* check space for 'nslots' new slots, throwing an error on overflow */
    void check_throw(int nslots) const
    {
        if (!check_space(nslots))
            err_throw(VMERR_STACK_OVERFLOW);
    }

    /* 
     *   Release the reserve.  Debuggers can use this to allow manual
     *   recovery from stack overflows, by making some extra stack
     *   temporarily available for the debugger's use in handling the
     *   overflow.  This releases the reserve, if available, that was
     *   specified when the stack was allocated.  Returns true if reserve
     *   space is available for release, false if not.  
     */
    int release_reserve()
    {
        /* if the reserve is already in use, we can't release it again */
        if (reserve_in_use_)
            return FALSE;

        /* add the reserve space to the maximum stack depth */
        max_depth_ += reserve_depth_;

        /* note that the reserve has been released */
        reserve_in_use_ = TRUE;

        /* indicate that we successfully released the reserve */
        return TRUE;
    }

    /*
     *   Recover the reserve.  If the debugger releases the reserve to handle
     *   a stack overflow, it can call this once the situation has been dealt
     *   with to take the reserve back out of play, so that the debugger can
     *   deal with any future overflow in the same manner.  
     */
    void recover_reserve()
    {
        /* if the reserve is in use, put it back in reserve */
        if (reserve_in_use_)
        {
            /* remove the reserve from the stack */
            max_depth_ -= reserve_depth_;

            /* mark the reserve as available again */
            reserve_in_use_ = FALSE;
        }
    }
        

private:
    /* the array of value holders making up the stack */
    vm_val_t *arr_;

    /* 
     *   Next available stack slot - the stack pointer starts out pointing at
     *   arr_[0], and is incremented after storing each element.  This is
     *   defined as a member variable only if we're not defining it as a
     *   separate static global.
     */
    VM_IF_REGS_IN_STRUCT(vm_val_t *sp_;)

    /* maximum depth that the stack is capable of holding */
    size_t max_depth_;

    /* extra reserve space */
    size_t reserve_depth_;

    /* flag: the reserve has been released for VM use */
    int reserve_in_use_;
};

#endif /* VMSTACK_H */


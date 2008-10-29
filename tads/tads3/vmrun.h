/* $Header: d:/cvsroot/tads/tads3/vmrun.h,v 1.4 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmrun.h - VM Execution
Function
  
Notes
  
Modified
  11/12/98 MJRoberts  - Creation
*/

#ifndef VMRUN_H
#define VMRUN_H

#include "vmglob.h"
#include "vmtype.h"
#include "vmstack.h"
#include "vmpool.h"
#include "vmobj.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmprofty.h"


/* ------------------------------------------------------------------------ */
/*
 *   for debugger use - interpreter context save structure 
 */
struct vmrun_save_ctx
{
    pool_ofs_t entry_ptr_;
    vm_val_t *frame_ptr_;
    size_t old_stack_depth_;
    const uchar **pc_ptr_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Procedure activation frame.  The activation frame is arranged as
 *   follows (the stack index increases reading down the list):
 *   
 *.  argument N
 *.  argument N-1
 *.  ...
 *.  argument 2
 *.  argument 1
 *.  target property
 *.  original target object
 *.  defining object
 *.  self
 *.  offset in calling method of next instruction to execute
 *.  caller's entry pointer (EP) register value
 *.  actual parameter count
 *.  caller's frame pointer <<<--- CURRENT FRAME POINTER
 *.  local variable 1
 *.  local variable 2
 *.  local variable 3
 *   
 *   So, local variable 1 is at (FP+1), local variable 2 is at (FP+2), and
 *   so on; the argument count is at (FP-1); 'self' is at (FP-4); argument 1
 *   is at (FP-5), argument 2 is at (FP-6), and so on.  
 */

/* offset from FP of first argument */
const int VMRUN_FPOFS_ARG1 = -8;

/* offset from FP of target property */
const int VMRUN_FPOFS_PROP = -7;

/* offset from FP of original target object */
const int VMRUN_FPOFS_ORIGTARG = -6;

/* offset from FP of defining object (definer of current method) */
const int VMRUN_FPOFS_DEFOBJ = -5;

/* offset from FP of 'self' */
const int VMRUN_FPOFS_SELF = -4;

/* offset from FP of return address */
const int VMRUN_FPOFS_RET = -3;

/* offset from FP of enclosing entry pointer */
const int VMRUN_FPOFS_ENC_EP = -2;

/* offset from FP of argument count */
const int VMRUN_FPOFS_ARGC = -1;

/* offset from FP of enclosing frame pointer */
const int VMRUN_FPOFS_ENC_FP = 0;

/* offset from FP of first local variable */
const int VMRUN_FPOFS_LCL1 = 1;


/* ------------------------------------------------------------------------ */
/*
 *   VM Execution Engine class.  This class handles execution of byte
 *   code.  
 */
class CVmRun
{
    friend class CVmDebug;
    
public:
    CVmRun();
    ~CVmRun();

    /* initialize */
    void init();

    /* terminate */
    void terminate();

    /*
     *   Get/set the method header size.  This size is stored in the image
     *   file; the image loader sets this at load time to the value
     *   retrieved from the image file.  All method headers in an image
     *   file use the same size.  
     */
    void set_funchdr_size(size_t siz);
    size_t get_funchdr_size() const { return funchdr_size_; }
    
    /* 
     *   Call a function or method.  'ofs' is the offset (in the code pool)
     *   of the code to invoke, and 'argc' is the number of arguments that
     *   the caller has pushed onto the stack.
     *   
     *   'caller_ofs' is the method offset (the byte code offset from the
     *   current entry pointer) in the caller.  If 'caller_ofs' is non-zero,
     *   we'll set up to begin execution in the target code and return the
     *   new program counter.  If 'caller_ofs' is zero, we'll invoke the VM
     *   byte code interpreter recursively, so this function will return
     *   only after the called code returns.  When calling recursively, set
     *   'recurse_calling' to a descriptive string that can be used to show
     *   the system code calling the recursive code in case of error.
     *   
     *   When calling a function, 'self' should be VM_INVALID_OBJ.
     *   Otherwise, this value gives the object whose method is being
     *   invoked.  
     *   
     *   The return value is the new program counter.  For recursive
     *   invocations, this will simply return null.  
     */
    const uchar *do_call(VMG_ uint caller_ofs, pool_ofs_t ofs, uint argc,
                         vm_obj_id_t self, vm_prop_id_t target_prop,
                         vm_obj_id_t orig_target_obj,
                         vm_obj_id_t defining_obj,
                         const char *recurse_calling);

    /* call a function, non-recursively */
    const uchar *do_call_func_nr(VMG_ uint caller_ofs, pool_ofs_t ofs,
                                 uint argc);

    /*
     *   Call a function pointer value.  If 'funcptr' contains a function
     *   pointer, we'll simply call the function; if it contains an
     *   anonymous function object, we'll call the anonymous function.  
     */
    const uchar *call_func_ptr(VMG_ const vm_val_t *funcptr, uint argc,
                               const char *recurse_name, uint caller_ofs);
        
    /*
     *   Get the descriptive message, if any, from an exception object.
     *   The returned string will not be null-terminated, but the length
     *   will be stored in *msg_len.  The returned string might point to
     *   constant pool data or data in an object, so it might not remain
     *   valid after a constant pool translation or garbage collection
     *   operation.  If the exception has no message, we will return a
     *   null pointer.  
     */
    static const char *get_exc_message(VMG_ const CVmException *exc,
                                       size_t *msg_len);
    static const char *get_exc_message(VMG_ vm_obj_id_t obj, size_t *msg_len);

    /* 
     *   Get the descriptive message from an exception.  If the exception
     *   has a program-generated exception object, we'll try to get the
     *   message from that object; if it's a VM exception with no
     *   underlying object, we'll retrieve the VM message.
     *   
     *   If add_unh_prefix is true, we'll add an "unhandled exception:"
     *   prefix to the message if we retrieve the message from a
     *   program-defined exception.  Otherwise, if it's a program
     *   exception, we won't add any prefix at all.  
     */
    static void get_exc_message(VMG_ const CVmException *exc,
                                char *buf, size_t buflen, int add_unh_prefix);

    /*
     *   Evaluate a property of an object.  'target_obj' is the object whose
     *   property is to be evaluated, 'target_prop' is the ID of the
     *   property to evaluate, and 'argc' is the number of arguments to the
     *   method.
     *   
     *   'caller_ofs' is the current method offset (the offset from the
     *   current entry pointer to the current program counter) in the
     *   caller.  If this is zero, we'll make a recursive call to the
     *   interpreter loop to execute any method code; thus, any method code
     *   will have run to completion by the time we return in this case.
     *   
     *   'self' is the object in whose context we're to perform the code
     *   execution, if the property is a method.  Note that 'self' may
     *   differ from 'target_obj' in some cases, particularly when
     *   inheriting.
     *   
     *   The return value is the new program counter from which execution
     *   should resume.  This will be null (and can be ignored) for
     *   recursive invocations.  
     */
    const uchar *get_prop(VMG_ uint caller_ofs,
                          const vm_val_t *target_obj,
                          vm_prop_id_t target_prop,
                          const vm_val_t *self, uint argc);

    /*
     *   Set a property of an object 
     */
    void set_prop(VMG_ vm_obj_id_t obj, vm_prop_id_t prop,
                  const vm_val_t *new_val);

    /* get data register 0 (R0) */
    vm_val_t *get_r0() { return &r0_; }

    /* set the default "say" function */
    void set_say_func(VMG_ const vm_val_t *val);

    /* get the current default "say" function */
    void get_say_func(vm_val_t *val) const;

    /* set the default "say" method */
    void set_say_method(vm_prop_id_t prop)
    {
        /* remember the property */
        say_method_ = prop;
    }

    /* get the current "say" method */
    vm_prop_id_t get_say_method() const { return say_method_; }

    /* pop an integer value; throws an error if the value is not an integer */
    void pop_int(VMG_ vm_val_t *val)
    {
        G_stk->pop(val);
        if (val->typ != VM_INT)
            err_throw(VMERR_INT_VAL_REQD);
    }

    /* 
     *   Pop a numeric value; throws an error if the value is not numeric.
     *   (At the moment, this is equivalent to pop_int, since int is the
     *   only numeric type; however, we distinguish between numbers in
     *   general and integers in particular, in case additional numeric
     *   types [such as floating-point numbers] are added in the future.) 
     */
    void pop_num(VMG_ vm_val_t *val)
    {
        G_stk->pop(val);
        if (!val->is_numeric())
            err_throw(VMERR_NUM_VAL_REQD);
    }

    /* pop an object value */
    void pop_obj(VMG_ vm_val_t *val)
    {
        G_stk->pop(val);
        if (val->typ != VM_OBJ)
            err_throw(VMERR_OBJ_VAL_REQD);
    }

    /* pop a property pointer value */
    void pop_prop(VMG_ vm_val_t *val)
    {
        G_stk->pop(val);
        if (val->typ != VM_PROP)
            err_throw(VMERR_PROPPTR_VAL_REQD);
    }

    /* pop a function pointer value */
    void pop_funcptr(VMG_ vm_val_t *val)
    {
        G_stk->pop(val);
        if (val->typ != VM_FUNCPTR)
            err_throw(VMERR_FUNCPTR_VAL_REQD);
    }

    /* 
     *   Pop two values from the stack.  The values are popped in reverse
     *   order, so val2 has the value at the top of the stack.  If the
     *   left operand was pushed first, this results in placing the left
     *   operand in val1 and the right operand in val2.  
     */
    void popval_2(VMG_ vm_val_t *val1, vm_val_t *val2)
    {
        popval(vmg_ val2);
        popval(vmg_ val1);
    }

    /* 
     *   Pop two integers, throwing an error if either value is not an
     *   integer.  Pops the item at the top of the stack into val2, and
     *   the next value into val1. 
     */
    void pop_int_2(VMG_ vm_val_t *val1, vm_val_t *val2)
    {
        popval(vmg_ val2);
        popval(vmg_ val1);
        if (val1->typ != VM_INT || val2->typ != VM_INT)
            err_throw(VMERR_INT_VAL_REQD);
    }

    /*
     *   Pop two numbers, throwing an error if either value is not
     *   numeric. 
     */
    void pop_num_2(VMG_ vm_val_t *val1, vm_val_t *val2)
    {
        popval(vmg_ val2);
        popval(vmg_ val1);
        if (!val1->is_numeric() || !val2->is_numeric())
            err_throw(VMERR_NUM_VAL_REQD);
    }

    /* 
     *   get the active function's argument count - we read the value from
     *   the first item below the frame pointer in the current frame 
     */
    int get_cur_argc(VMG0_) const
    {
        return G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_ARGC)
            ->val.intval;
    }
    
    /*
     *   Get a parameter value; 0 is the first parameter, 1 is the second,
     *   and so on.  
     */
    vm_val_t *get_param(VMG_ int idx) const
        { return get_param_from_frame(vmg_ frame_ptr_, idx); }

    /* get a parameter from a given frame */
    vm_val_t *get_param_from_frame(VMG_ vm_val_t *fp, int idx) const
        { return G_stk->get_from_frame(fp, VMRUN_FPOFS_ARG1 - idx); }

    /* get a parameter at the given stack level */
    vm_val_t *get_param_at_level(VMG_ int idx, int level) const
    {
        return get_param_from_frame(vmg_ get_fp_at_level(vmg_ level), idx);
    }

    /* 
     *   get a local variable's value; 0 is the first local variable, 1 is
     *   the second, and so on 
     */
    vm_val_t *get_local(VMG_ int idx) const
        { return get_local_from_frame(vmg_ frame_ptr_, idx); }

    /* get a local from a given frame */
    vm_val_t *get_local_from_frame(VMG_ vm_val_t *fp, int idx) const
        { return G_stk->get_from_frame(fp, VMRUN_FPOFS_LCL1 + idx); }

    /* get a local at the given stack level */
    vm_val_t *get_local_at_level(VMG_ int idx, int level) const
    {
        return get_local_from_frame(vmg_ get_fp_at_level(vmg_ level), idx);
    }


    /*
     *   Get the frame pointer at the given stack level.  Level 0 is the
     *   currently active frame, 1 is the first enclosing level, and so
     *   on.  Throws an error if the enclosing frame is invalid. 
     */
    vm_val_t *get_fp_at_level(VMG_ int level) const;

    /*
     *   Get the current frame depth.  This is the stack depth of the
     *   current frame pointer.  This can be used to compare two frame
     *   pointers to determine if one encloses the other - the pointer
     *   with the smaller depth value encloses the larger one.  
     */
    size_t get_frame_depth(VMG0_) const
        { return G_stk->ptr_to_index(frame_ptr_); }

    /* get the current frame pointer */
    vm_val_t *get_frame_ptr() const { return frame_ptr_; }

    /* given a frame pointer, get the enclosing frame pointer */
    static vm_val_t *get_enclosing_frame_ptr(VMG_ vm_val_t *fp)
    {
        return (vm_val_t *)
            G_stk->get_from_frame(fp, VMRUN_FPOFS_ENC_FP)->val.ptr;
    }

    /* get the number of arguments from a given frame */
    int get_argc_from_frame(VMG_ vm_val_t *fp) const
        { return G_stk->get_from_frame(fp, VMRUN_FPOFS_ARGC)->val.intval; }

    /* get the argument counter from a given stack level */
    int get_argc_at_level(VMG_ int level) const
        { return get_argc_from_frame(vmg_ get_fp_at_level(vmg_ level)); }

    /* given a frame pointer, get the 'self' object for the frame */
    static vm_obj_id_t get_self_from_frame(VMG_ vm_val_t *fp)
    {
        vm_val_t *self_val;

        /* get the 'self' slot on the stack */
        self_val = G_stk->get_from_frame(fp, VMRUN_FPOFS_SELF);

        /* return the appropriate value */
        return (self_val->typ == VM_NIL ? VM_INVALID_OBJ : self_val->val.obj);
    }

    /* get the 'self' object at a given stack level */
    vm_obj_id_t get_self_at_level(VMG_ int level) const
        { return get_self_from_frame(vmg_ get_fp_at_level(vmg_ level)); }

    /* given a frame pointer, get the target property for the frame */
    static vm_prop_id_t get_target_prop_from_frame(VMG_ vm_val_t *fp)
    {
        vm_val_t *val;

        /* get the 'self' slot on the stack */
        val = G_stk->get_from_frame(fp, VMRUN_FPOFS_PROP);

        /* return the appropriate value */
        return (val->typ == VM_NIL ? VM_INVALID_PROP : val->val.prop);
    }

    /* get the target property at a given stack level */
    vm_prop_id_t get_target_prop_at_level(VMG_ int level) const
    {
        return get_target_prop_from_frame(vmg_ get_fp_at_level(vmg_ level));
    }

    /* given a frame pointer, get the defining object from the frame */
    vm_obj_id_t get_defining_obj_from_frame(VMG_ vm_val_t *fp) const
    {
        vm_val_t *val;

        /* get the defining object slot on the stack */
        val = G_stk->get_from_frame(fp, VMRUN_FPOFS_DEFOBJ);

        /* return the appropriate value */
        return (val->typ == VM_NIL ? VM_INVALID_OBJ : val->val.obj);
    }

    /* get the defining object at a given stack level */
    vm_obj_id_t get_defining_obj_at_level(VMG_ int level) const
    {
        return get_defining_obj_from_frame(vmg_ get_fp_at_level(vmg_ level));
    }

    /* given a frame pointer, get the original target object */
    vm_obj_id_t get_orig_target_obj_from_frame(VMG_ vm_val_t *fp) const
    {
        vm_val_t *val;

        /* get the original target object slot on the stack */
        val = G_stk->get_from_frame(fp, VMRUN_FPOFS_ORIGTARG);

        /* return the appropriate value */
        return (val->typ == VM_NIL ? VM_INVALID_OBJ : val->val.obj);
    }

    /* get the current original target object at a given stack level */
    vm_obj_id_t get_orig_target_obj_at_level(VMG_ int level) const
    {
        return get_orig_target_obj_from_frame(
            vmg_ get_fp_at_level(vmg_ level));
    }

    /* 
     *   get the enclosing entry pointer from a given frame (it's the
     *   second item pushed before the enclosing frame ponter, hence it's
     *   at offset -2 in the frame) 
     */
    static pool_ofs_t get_enclosing_entry_ptr_from_frame(VMG_ vm_val_t *fp)
        { return G_stk->get_from_frame(fp, VMRUN_FPOFS_ENC_EP)->val.ofs; }

    /*
     *   Get the return offset from a given frame.  This is the offset of
     *   the return address from the start of the method header for the
     *   frame. 
     */
    static ulong get_return_ofs_from_frame(VMG_ vm_val_t *fp)
        { return G_stk->get_from_frame(fp, VMRUN_FPOFS_RET)->val.ofs; }

    /* 
     *   Get the return address from a given frame.  (The return address
     *   is the third item pushed before the enclosing frame pointer,
     *   hence it's at offset -3 from the frame pointer.)  Returns zero if
     *   we were called by recursive invocation of the VM - this is not
     *   ambiguous with an actual return address of zero, since zero is
     *   never a valid code address.
     */
    static pool_ofs_t get_return_addr_from_frame(VMG_ vm_val_t *fp)
    {
        pool_ofs_t ofs;

        /* get the return method offset from the stack */
        ofs = get_return_ofs_from_frame(vmg_ fp);

        /* 
         *   zero is never a valid method offset, so if the offset is zero
         *   it means that we're at a recursive invocation of the VM -
         *   indicate this by returning zero as the absolute pool address.
         */
        if (ofs == 0)
            return 0;

        /* 
         *   add the offset to the enclosing entry pointer to yield the
         *   absolute pool address of the return point 
         */
        return ofs + get_enclosing_entry_ptr_from_frame(vmg_ fp);
    }
                 
    /* 
     *   Given a frame pointer, set up a function pointer for the return
     *   address from the frame.  
     */
    static void set_return_funcptr_from_frame(VMG_ class CVmFuncPtr *func_ptr,
                                              vm_val_t *frame_ptr);

    /*
     *   Determine if we're in a recursive VM invocation.  Returns true if
     *   we're in a recursive invocation, false if we're in the top-level
     *   invocation.  (A recursive invocation occurs when native code
     *   called from byte code calls back into the VM to call byte code
     *   itself; because the invocation is recursive, we must return to
     *   the native code when the inner byte code returns rather than
     *   continuing with enclosing byte code directly.)
     */
    int is_recursive_invocation(VMG0_) const;

    /* reset the machine registers to the initial conditions */
    void reset(VMG0_);
    
    /*
     *   Get the current "self" object.  The "self" object is always the
     *   implicit first parameter to any method.  Note that this version of
     *   the method *doesn't* check for nil - it assumes that the caller
     *   knows for sure that there's a valid "self", so dispenses with any
     *   checks to save time.  
     */
    vm_obj_id_t get_self(VMG0_) const
    {
        /* get the object value of the 'self' slot in the current frame */
        return G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_SELF)->val.obj;
    }

    /* get the pointer to the current "self" value */
    vm_val_t *get_self_val(VMG0_) const
        { return G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_SELF); }

    /*
     *   Get the current "self" object, checking for nil.  If "self" is nil,
     *   we'll return VM_INVALID_OBJ.  This version (not get_self()) should
     *   be used whenever it's not certain from context that there's a valid
     *   "self".  
     */
    vm_obj_id_t get_self_check(VMG0_) const
    {
        /* get the 'self' slot from the stack frame */
        vm_val_t *valp = G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_SELF);

        /* if it's nil, return VM_INVALID_OBJ; otherwise, return the obj ID */
        return (valp->typ == VM_NIL ? VM_INVALID_OBJ : valp->val.obj);
    }

    /* set the current 'self' object */
    void set_self(VMG_ const vm_val_t *val)
    {
        /* store the given value in the 'self' slot in the current frame */
        *G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_SELF) = *val;
    }

    /*
     *   Set the current execution context: the 'self' value, the target
     *   property, the original target object, and the defining object.  
     */
    void set_method_ctx(VMG_ vm_obj_id_t new_self,
                        vm_prop_id_t new_target_prop,
                        vm_obj_id_t new_target_obj,
                        vm_obj_id_t new_defining_obj)
    {
        /* set the "self" slot in the current stack frame */
        G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_SELF)
            ->set_obj(new_self);

        /* set the target property slot in the frame */
        G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_PROP)
            ->set_propid(new_target_prop);

        /* set the original target object slot in the frame */
        G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_ORIGTARG)
            ->set_obj(new_target_obj);

        /* set the defining object slot in the frame */
        G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_DEFOBJ)
            ->set_obj(new_defining_obj);
    }

    /* get the current target property value */
    vm_prop_id_t get_target_prop(VMG0_) const
    {
        return G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_PROP)->val.prop;
    }

    /* get the current defining object */
    vm_obj_id_t get_defining_obj(VMG0_) const
    {
        return G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_DEFOBJ)->val.obj;
    }

    /* get the current original target object */
    vm_obj_id_t get_orig_target_obj(VMG0_) const
    {
        return G_stk->get_from_frame(frame_ptr_, VMRUN_FPOFS_ORIGTARG)
            ->val.obj;
    }

    /* push an object ID */
    static void push_obj(VMG_ vm_obj_id_t obj)
        { G_stk->push()->set_obj(obj); }

    /* push a property ID */
    static void push_prop(VMG_ vm_prop_id_t prop)
        { G_stk->push()->set_propid(prop); }

    /* push a boolean value */
    static void push_bool(VMG_ int flag)
        { G_stk->push()->set_logical(flag); }

    /* push nil */
    static void push_nil(VMG0_)
        { G_stk->push()->set_nil(); }

    /* push a code offset value */
    static void push_codeofs(VMG_ pool_ofs_t ofs)
        { G_stk->push()->set_codeofs(ofs); }

    /* push a stack pointer value */
    static void push_stackptr(VMG_ vm_val_t *stack_ptr)
        { G_stk->push()->set_stack((void *)stack_ptr); }

    /* push an integer value */
    static void push_int(VMG_ int32 intval)
        { G_stk->push()->set_int(intval); }

    /* push an enumerator value */
    static void push_enum(VMG_ uint32 intval)
        { G_stk->push()->set_enum(intval); }

    /*
     *   Touch the code page containing the currently executing code.  This
     *   should be called just after any code translates another code page
     *   pointer, to ensure that the current method's code page is
     *   re-established as the most recently used and is thus not swapped
     *   out.  
     */
    void touch_entry_ptr_page(VMG0_)
    {
        /* translate the entry pointer */
        G_code_pool->get_ptr(entry_ptr_);
    }

    /* 
     *   get the current entry pointer (the code pool offset of the start of
     *   the current method) 
     */
    pool_ofs_t get_entry_ptr() const { return entry_ptr_; }

    /* get the current program counter offset from the entry pointer */
    uint get_method_ofs() const
    {
        /* 
         *   Return the current program counter minus the current entry
         *   pointer.  If there is no current program counter, we're not
         *   executing in byte code, so there's no method offset.  
         */
        if (pc_ptr_ != 0)
            return *pc_ptr_ - entry_ptr_native_;
        else
            return 0;
    }

    /*
     *   Convert a pointer to the currently executing method into an offset
     *   from the start of the current method.  
     */
    ulong pc_to_method_ofs(const uchar *p)
    {
        /* 
         *   get the memory address of the current entry pointer, and
         *   subtract that from the given memory pointer to get an offset
         *   from the start of the current method 
         */
        return p - entry_ptr_native_;
    }

    /*
     *   Create an exception of the given imported class and throw it.  If
     *   the class is not exported, we'll create a basic run-time exception;
     *   if that's not defined, we'll create an arbitrary object.
     *   
     *   Arguments to the exception constructor are on the stack, with the
     *   argument count in argc.  If the imported class doesn't exist, we'll
     *   instead throw an intrinsic-class-general-error exception with the
     *   given fallback message as explanatory text.  
     */
    void throw_new_class(VMG_ vm_obj_id_t cls, uint argc,
                         const char *fallback_msg);

    /*
     *   Save/restore the interpreter context.  This is for use by the
     *   debugger when evaluating an expression in the course of execution,
     *   to ensure that everything is reset properly to the enclosing
     *   execution context when it's finished.  
     */
    void save_context(VMG_ vmrun_save_ctx *ctx);
    void restore_context(VMG_ vmrun_save_ctx *ctx);

    /*
     *   Get the boundaries of the given source-code statement in the given
     *   function.  Fills in the line pointer and *stm_start and *stm_end
     *   with information on the source line containing the given offset in
     *   the given method.  Returns true if source information is
     *   successfully located for the given machine code address, false if
     *   not.
     *   
     *   If no debugging information is available for the given code
     *   location, this function cannot get the source-code statement
     *   bounds, and returns false.  
     */
    static int get_stm_bounds(VMG_ const class CVmFuncPtr *func_ptr,
                              ulong pc_ofs,
                              class CVmDbgLinePtr *line_ptr,
                              ulong *stm_start, ulong *stm_end);


    /* -------------------------------------------------------------------- */
    /* 
     *   Set the HALT VM flag.  This allows the debugger to terminate the
     *   program immediately, without allowing any more byte-code
     *   instructions to execute.  
     */
    void set_halt_vm(int f) { halt_vm_ = f; }

    /* -------------------------------------------------------------------- */
    /*
     *   Start profiling.  This deletes any old profiling records and starts
     *   a new profiling session.  We'll capture profiling data until
     *   end_profiling() is called.  This function is only included in the
     *   build if the profiler is included in the build.  
     */
    void start_profiling();

    /* end profiling */
    void end_profiling();

    /* 
     *   get the profiling data - we'll invoke the callback once for each
     *   function in our table of data 
     */
    void get_profiling_data(VMG_
                            void (*cb)(void *ctx, const char *func_name,
                                       unsigned long time_direct,
                                       unsigned long time_in_children,
                                       unsigned long call_cnt),
                            void *cb_ctx);

protected:
    /* 
     *   Execute byte code starting at a given address.  This function
     *   retains control until the byte code function invoked returns or
     *   throws an unhandled exception.
     *   
     *   If an exception occurs and is not handled by the byte code, we'll
     *   throw VMERR_UNHANDLED_EXC with the exception object as the first
     *   parameter.  
     */
    void run(VMG_ const uchar *p);

    /* 
     *   Display a dstring via the default string display function.  This
     *   function pushes a string value (with the given constant pool
     *   offset), then does the same work as do_call() to invoke a function
     *   with one argument.
     *   
     *   The string is identified by its offset in the constant pool.  
     */
    const uchar *disp_dstring(VMG_ pool_ofs_t ofs, uint caller_ofs,
                              vm_obj_id_t self);

    /*
     *   Display the value at top of stack via the default string display
     *   function.  does the same work as do_call() to invoke the function
     *   with one argument, which must already be on the stack.  
     */
    const uchar *disp_string_val(VMG_ uint caller_ofs, vm_obj_id_t self);

    /*
     *   Convert a pointer to the currently executing method into a code
     *   pool offset. 
     */
    pool_ofs_t pc_to_code_ofs(VMG_ const uchar *p)
    {
        /* 
         *   get the offset from the start of the current method, and add
         *   it to the code pool offset of the start of the current method
         *   to yield the code pool offset of the pointer 
         */
        return (pool_ofs_t)pc_to_method_ofs(p) + entry_ptr_;
    }

    /*
     *   Set up a function header pointer for the current function 
     */
    void set_current_func_ptr(VMG_ class CVmFuncPtr *func_ptr);

    /* call a built-in function */
    void call_bif(VMG_ uint set_index, uint func_index, uint argc);

    /* 
     *   Throw an exception.  Returns a non-null program counter if a
     *   handler was found, false if not.  If a handler was found, byte-code
     *   execution can proceed; if not, the byte-code execution loop must
     *   pass the exception up to its caller.  
     */
    const uchar *do_throw(VMG_ const uchar *pc, vm_obj_id_t exception_obj);
    
    /* 
     *   Inherit a property - this is essentially the same as get_prop,
     *   but rather than getting the property from the given object, this
     *   ignores any such property defined directly by the object and goes
     *   directly to the inherited definition.  However, the "self" object
     *   is still the same as the current "self" object, since we want to
     *   evaluate the inherited method in the context of the original
     *   target "self" object.  
     */
    const uchar *inh_prop(VMG_ uint caller_ofs, vm_prop_id_t prop, uint argc);

    /*
     *   Look up a property value without evaluating it.  Returns true if we
     *   found the property, false if not.  
     */
    inline static int get_prop_no_eval(VMG_ const vm_val_t **target_obj,
                                       vm_prop_id_t target_prop,
                                       uint *argc, vm_obj_id_t *srcobj,
                                       vm_val_t *val,
                                       const vm_val_t **self,
                                       vm_val_t *new_self);

    /*
     *   Evaluate a property value.  If the value contains code, we'll
     *   execute the code; if it contains a self-printing string, we'll
     *   display the string; otherwise, we'll push the value onto the stack.
     *   
     *   'found' indicates whether or not the property is defined by the
     *   object.  False indicates that the property is not defined, true
     *   that it is defined.  If the property isn't defined, we'll simply
     *   discard arguments and push nil.
     *   
     *   If 'caller_ofs' is zero, we'll recursively invoke the interpreter
     *   loop if it's necessary to run a method; otherwise, we'll set up at
     *   the beginning of the method's code and let the caller proceed into
     *   the code.  
     */
    inline const uchar *eval_prop_val(VMG_ int found, uint caller_ofs,
                                      const vm_val_t *val, vm_obj_id_t self,
                                      vm_prop_id_t target_prop,
                                      const vm_val_t *orig_target_obj,
                                      vm_obj_id_t defining_obj,
                                      uint argc);

    /*
     *   Check a property for validity in a speculative evaluation.  If
     *   evaulating the property would cause any side effects, we'll throw
     *   an error (VMERR_BAD_SPEC_EXPR); otherwise, we won't do anything.
     *   Side effects include displaying a dstring or invoking a method. 
     */
    void check_prop_spec_eval(VMG_ vm_obj_id_t obj, vm_prop_id_t prop);

    /*
     *   Return from a function or method.  Returns the new program counter
     *   at which to continue execution, and restore machine registers to
     *   the enclosing frame.
     *   
     *   Returns a non-null program counter if execution should proceed,
     *   null if we're returning from the outermost stack level.  When we
     *   return null, the caller must return control to the host
     *   environment, since the host environment called the function from
     *   which we're returning.  
     */
    const uchar *do_return(VMG0_);

    /*
     *   Enter/leave a recursive call frame.  These functions are used only
     *   when a debugger is present.  'pc_ptr' is a pointer to the program
     *   counter in the calling byte-code frame.  
     */
    void enter_recursive_frame(VMG_ int argc, const uchar **pc_ptr);
    void leave_recursive_frame(VMG0_);

    /* 
     *   append a stack trace to the given string, returning a new string
     *   object 
     */
    vm_obj_id_t append_stack_trace(VMG_ vm_obj_id_t str_obj);

    /* push a value onto the stack */
    static void pushval(VMG_ const vm_val_t *val) { G_stk->push(val); }

    /* pop a value off the stack */
    void popval(VMG_ vm_val_t *val) { G_stk->pop(val); }
  
    /* add two values, leaving the result in *val1 */
    void compute_sum(VMG_ vm_val_t *val1, vm_val_t *val2);

    /* subtract one value from another, leaving the result in *val1 */
    void compute_diff(VMG_ vm_val_t *val1, vm_val_t *val2);

    /* compute the product, leaving the result in *val1 */
    void compute_product(VMG_ vm_val_t *val1, vm_val_t *val2);

    /* compute the quotient val1/val2, leaving the result in *val2 */
    void compute_quotient(VMG_ vm_val_t *val1, vm_val_t *val2);

    /* XOR two values and push the result */
    void xor_and_push(VMG_ vm_val_t *val1, vm_val_t *val2);

    /* 
     *   index container_val by index_val (i.e., compute
     *   container_val[index_val]), storing the result at *result 
     */
    void apply_index(VMG_ vm_val_t *result,
                     const vm_val_t *container_val,
                     const vm_val_t *index_val);

    /* 
     *   Set the element at index index_val in container_val to new_val,
     *   and push the new container value.  The container may be a new
     *   object, since some types (lists, for example) cannot have their
     *   values changed but instead create new objects when an indexed
     *   element is modified.  
     */
    void set_index(VMG_ vm_val_t *container_val,
                   const vm_val_t *index_val,
                   const vm_val_t *new_val);

    /* 
     *   create a new object of the given index into the metaclass
     *   dependency table for the load image file, using the given number
     *   of parameters; removes the parameters from the stack, and leaves
     *   the new object reference in register R0 
     */
    const uchar *new_and_store_r0(VMG_ const uchar *pc,
                                  uint metaclass_idx, uint argc,
                                  int is_transient);

    /* 
     *   Compare the two values at top of stack for equality; returns true
     *   if the values are equal, false if not.  Removes the two values from
     *   the stack.  
     */
    int pop2_equal(VMG0_)
    {
        /* compare the values and return the result */
        int ret = G_stk->get(1)->equals(vmg_ G_stk->get(0));

        /* discard the values */
        G_stk->discard(2);

        /* return the result */
        return ret;
    }

    /* 
     *   Compare the magnitude of the two values at the top of the stack.
     *   Returns 1 if the value at (TOS-1) is greater than the value at TOS,
     *   -1 if (TOS-1) is less than (TOS), and 0 if the two value are equal.
     *   Removes the two values from the stack.  
     */
    int pop2_compare(VMG0_)
    {
        /* compare the values and return the result */
        int ret = G_stk->get(1)->compare_to(vmg_ G_stk->get(0));

        /* discard the values */
        G_stk->discard(2);

        /* return the result */
        return ret;
    }

    /* is TOS-1 < TOS ? */
    int pop2_compare_lt(VMG0_)
    {
        /* compare the values and return the result */
        int ret = G_stk->get(1)->is_lt(vmg_ G_stk->get(0));

        /* discard the values */
        G_stk->discard(2);

        /* return the result */
        return ret;
    }

    /* is TOS-1 <= TOS ? */
    int pop2_compare_le(VMG0_)
    {
        /* compare the values and return the result */
        int ret = G_stk->get(1)->is_le(vmg_ G_stk->get(0));

        /* discard the values */
        G_stk->discard(2);

        /* return the result */
        return ret;
    }

    /* is TOS-1 > TOS ? */
    int pop2_compare_gt(VMG0_)
    {
        /* compare the values and return the result */
        int ret = G_stk->get(1)->is_gt(vmg_ G_stk->get(0));

        /* discard the values */
        G_stk->discard(2);

        /* return the result */
        return ret;
    }

    /* is TOS-1 >= TOS ? */
    int pop2_compare_ge(VMG0_)
    {
        /* compare the values and return the result */
        int ret = G_stk->get(1)->is_ge(vmg_ G_stk->get(0));

        /* discard the values */
        G_stk->discard(2);

        /* return the result */
        return ret;
    }

    /* given a constant pool offset, get a pointer to the constant data */
    const char *get_const_ptr(VMG_ pool_ofs_t ofs) const
        { return G_const_pool->get_ptr(ofs); }

    /* 
     *   get a signed 16-bit byte-code operand, incrementing the
     *   instruction pointer past the operand 
     */
    int16 get_op_int16(const uchar **p)
    {
        int16 ret = (int16)osrp2s(*p);
        *p += 2;
        return ret;
    }

    /* get an unsigned 16-bit byte-code operand */
    uint16 get_op_uint16(const uchar **p)
    {
        uint16 ret = (uint16)osrp2(*p);
        *p += 2;
        return ret;
    }

    /* get a signed 32-bit byte-code operand */
    int32 get_op_int32(const uchar **p)
    {
        int32 ret = (int32)osrp4(*p);
        *p += 4;
        return ret;
    }

    /* get an unsigned 32-bit byte-code operand */
    uint32 get_op_uint32(const uchar **p)
    {
        uint32 ret = (uint32)t3rp4u(*p);
        *p += 4;
        return ret;
    }

    /* get a signed 8-bit byte-code operand */
    int get_op_int8(const uchar **p)
    {
        int ret = (int)(signed char)**p;
        ++(*p);
        return ret;
    }

    /* get an unsigned 8-bit byte-code operand */
    uint get_op_uint8(const uchar **p)
    {
        uint ret = (uint)**p;
        ++(*p);
        return ret;
    }

    /* record a function or method entry in the profiler data */
    void prof_enter(pool_ofs_t call_ofs,
                    vm_obj_id_t obj, vm_prop_id_t prop);

    /* record a function or method exit in the profiler data */
    void prof_leave();

    /* find or create a function entry in the master profiler table */
    class CVmHashEntryProfiler
        *prof_find_master_rec(const struct vm_profiler_rec *p);

    /* calculate an elapsed time */
    void prof_calc_elapsed(vm_prof_time *diff, const vm_prof_time *a,
                           const vm_prof_time *b);

    /* add an elapsed time value to a cumulative elapsed time value */
    void prof_add_elapsed(vm_prof_time *sum, const vm_prof_time *val);

    /* hash table enumeration callback for dumping profiler data */
    static void prof_enum_cb(void *ctx0, class CVmHashEntry *entry0);

    /*
     *   Function header size - obtained from the image file upon loading 
     */
    size_t funchdr_size_;

    /*
     *   A pointer to a global variable in the object table (CVmObjTable)
     *   containing the function to invoke for the SAY and SAYVAL opcodes.
     *   This can be a function pointer, a function object, or nil.  If this
     *   is nil, the SAY opcode will throw an error.  
     */
    struct vm_globalvar_t *say_func_;

    /*
     *   The method to invoke for the SAY and SAYVAL opcodes when a valid
     *   "self" object is available.  If no method is defined, this will
     *   be set to VM_INVALID_PROP.  
     */
    vm_prop_id_t say_method_;

    /*
     *   R0 - data register 0.  This register stores function return
     *   values. 
     */
    vm_val_t r0_;

    /* 
     *   EP - entrypoint register.  This register stores the code offset
     *   of the method header of the currently executing code.  
     */
    pool_ofs_t entry_ptr_;

    /* 
     *   native entry pointer value - this is simply the translated value of
     *   the entry pointer (i.e., G_code_pool->get_ptr(entry_ptr_)) 
     */
    const uchar *entry_ptr_native_;

    /*
     *   FP - frame pointer register.  This points to the base of the
     *   current stack activation frame.  Local variables and parameters
     *   are reachable relative to this register. 
     */
    vm_val_t *frame_ptr_;

    /* 
     *   Pointer to program counter - we use this in the debugger to create
     *   pseudo-stack frames for system code when we recursively invoke the
     *   VM, and for finding the current PC from intrinsic function code.  
     */
    const uchar **pc_ptr_;

    /* 
     *   Flag: VM is halting.  This is used by the debugger to force the
     *   program to stop executing.  This is not used except with the
     *   debug-mode interpreter.  
     */
    int halt_vm_;

    /* flag: profiling is active */
    int profiling_;

    /* in case we have a profiler, include the profiler stack */
    struct vm_profiler_rec *prof_stack_;
    size_t prof_stack_max_;

    /* next available index in the profiler stack */
    size_t prof_stack_idx_;

    /*
     *   Start of execution in the currently active function, since the last
     *   call or return.  This uses the OS-specific high-precision timer
     *   (defined by os_prof_curtime() in vmprof.h).
     *   
     *   Each time we call a function or return from a function, we measure
     *   the delta from this value, then add that in to the cumulative time
     *   for the function.  
     */
    vm_prof_time prof_start_;

    /* 
     *   profiler master hash table - this is a table with one entry for
     *   every function and method called since we began profiling, keyed by
     *   method or function ID (the object.property for a method, or the
     *   entrypoint code offset for a function) 
     */
    class CVmHashTable *prof_master_table_;
};

#endif /* VMRUN_H */


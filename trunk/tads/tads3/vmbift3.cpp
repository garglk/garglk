#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMBIFTAD.CPP,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbift3.cpp - T3 VM system interface function set
Function
  
Notes
  
Modified
  04/05/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>

#include "utf8.h"
#include "vmbif.h"
#include "vmbift3.h"
#include "vmstack.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmpool.h"
#include "vmobj.h"
#include "vmrun.h"
#include "vmstr.h"
#include "vmvsn.h"
#include "vmimage.h"
#include "vmlst.h"
#include "vmtobj.h"
#include "vmfunc.h"
#include "vmpredef.h"
#include "vmsrcf.h"
#include "charmap.h"


/*
 *   run the garbage collector
 */
void CVmBifT3::run_gc(VMG_ uint argc)
{
    /* no arguments are allowed */
    check_argc(vmg_ argc, 0);

    /* run the garbage collector */
    G_obj_table->gc_full(vmg0_);
}

/*
 *   set the SAY instruction's handler function 
 */
#define SETSAY_NO_FUNC    1
#define SETSAY_NO_METHOD  2
void CVmBifT3::set_say(VMG_ uint argc)
{
    vm_val_t *arg = G_stk->get(0);
    vm_val_t val;
    
    /* one argument is required */
    check_argc(vmg_ argc, 1);

    /* check to see if we're setting the default display method */
    if (arg->typ == VM_PROP
        || (arg->typ == VM_INT && arg->val.intval == SETSAY_NO_METHOD))
    {
        vm_prop_id_t prop;
        
        /* 
         *   the return value is the old property pointer (or
         *   SETSAY_NO_METHOD if there was no valid property set previously) 
         */
        prop = G_interpreter->get_say_method();
        if (prop != VM_INVALID_PROP)
            retval_prop(vmg_ prop);
        else
            retval_int(vmg_ SETSAY_NO_METHOD);

        /* get the new value */
        G_stk->pop(&val);

        /* if it's SETSAY_NO_METHOD, set it to the invalid prop ID */
        if (val.typ == VM_INT)
            val.set_propid(VM_INVALID_PROP);

        /* set the method */
        G_interpreter->set_say_method(val.val.prop);
    }
    else if (arg->typ == VM_FUNCPTR
             || arg->typ == VM_OBJ
             || (arg->typ == VM_INT && arg->val.intval == SETSAY_NO_FUNC))
    {
        /* 
         *   the return value is the old function (or SETSAY_NO_FUNC if the
         *   old function was nil) 
         */
        G_interpreter->get_say_func(&val);
        if (val.typ != VM_NIL)
            retval(vmg_ &val);
        else
            retval_int(vmg_ SETSAY_NO_FUNC);

        /* get the new function value */
        G_stk->pop(&val);

        /* if it's SETSAY_NO_FUNC, set the function to nil */
        if (val.typ == VM_INT)
            val.set_nil();

        /* set the new function */
        G_interpreter->set_say_func(vmg_ &val);
    }
    else
    {
        /* invalid type */
        err_throw(VMERR_BAD_TYPE_BIF);
    }
}

/*
 *   get the VM version number
 */
void CVmBifT3::get_vm_vsn(VMG_ uint argc)
{
    /* no arguments are allowed */
    check_argc(vmg_ argc, 0);

    /* set the integer return value */
    retval_int(vmg_ T3VM_VSN_NUMBER);
}

/*
 *   get the VM identification string
 */
void CVmBifT3::get_vm_id(VMG_ uint argc)
{
    /* no arguments are allowed */
    check_argc(vmg_ argc, 0);

    /* set the integer return value */
    retval_str(vmg_ T3VM_IDENTIFICATION);
}


/*
 *   get the VM banner string
 */
void CVmBifT3::get_vm_banner(VMG_ uint argc)
{
    /* no arguments are allowed */
    check_argc(vmg_ argc, 0);

    /* return the string */
    retval_str(vmg_ T3VM_BANNER_STRING);
}

/* 
 *   get the 'preinit' status - true if preinit, nil if normal 
 */
void CVmBifT3::get_vm_preinit_mode(VMG_ uint argc)
{
    /* no arguments allowed */
    check_argc(vmg_ argc, 0);

    /* return the preinit mode */
    retval_int(vmg_ G_preinit_mode);
}

/*
 *   get the runtime symbol table 
 */
void CVmBifT3::get_global_symtab(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* return the loader's symbol table object, if any */
    retval_obj(vmg_ G_image_loader->get_reflection_symtab());
}

/* 
 *   allocate a new property ID 
 */
void CVmBifT3::alloc_new_prop(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);

    /* allocate and return a new property ID */
    retval_prop(vmg_ G_image_loader->alloc_new_prop(vmg0_));
}

/*
 *   get a stack trace 
 */
void CVmBifT3::get_stack_trace(VMG_ uint argc)
{
    int single_level;
    int level;
    vm_val_t *fp;
    vm_val_t lst_val;
    CVmObjList *lst;
    pool_ofs_t entry_addr;
    ulong method_ofs;
    vm_val_t stack_info_cls;

    /* check arguments */
    check_argc_range(vmg_ argc, 0, 1);

    /* get the imported stack information class */
    stack_info_cls.set_obj(G_predef->stack_info_cls);
    if (stack_info_cls.val.obj == VM_INVALID_OBJ)
    {
        /* 
         *   there's no stack information class - we can't return any
         *   meaningful information, so just return nil 
         */
        retval_nil(vmg0_);
        return;
    }

    /* check to see if we're fetching a single level or the full trace */
    if (argc >= 1)
    {
        /* get the single level, and adjust to a 0 base */
        single_level = pop_int_val(vmg0_) - 1;

        /* make sure it's in range */
        if (single_level < 0)
            err_throw(VMERR_BAD_VAL_BIF);

        /* we won't need a return list */
        lst_val.set_nil();
        lst = 0;
    }
    else
    {
        /* 
         *   We're returning a full list, so we need to allocate the list for
         *   the return value.  First, count stack levels to see how big a
         *   list we'll need.  
         */

        /* start at the current function */
        fp = G_interpreter->get_frame_ptr();

        /* traverse the stack to determine the frame depth */
        for (level = 0 ; fp != 0 ;
             fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp), ++level) ;

        /* create the list */
        lst_val.set_obj(CVmObjList::create(vmg_ FALSE, level));
        lst = (CVmObjList *)vm_objp(vmg_ lst_val.val.obj);
        
        /* protect the list from garbage collection while we work */
        G_stk->push(&lst_val);

        /* flag that we're doing the whole stack */
        single_level = -1;
    }

    /* set up at the current function */
    fp = G_interpreter->get_frame_ptr();
    entry_addr = G_interpreter->get_entry_ptr();
    method_ofs = G_interpreter->get_method_ofs();

    /* traverse the frames */
    for (level = 0 ; fp != 0 ;
         fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp), ++level)
    {
        int fr_argc;
        int i;
        vm_obj_id_t def_obj;
        vm_val_t info_self;
        vm_val_t info_func;
        vm_val_t info_obj;
        vm_val_t info_prop;
        vm_val_t info_args;
        vm_val_t info_srcloc;
        CVmObjList *arglst;
        vm_val_t ele;
        CVmFuncPtr func_ptr;

        /* if we're looking for a single level, and this isn't it, skip it */
        if (single_level >= 0 && level != single_level)
            goto done_with_level;
       
        /* 
         *   start with the information values to nil - we'll set the
         *   appropriate ones when we find out what we have 
         */
        info_func.set_nil();
        info_obj.set_nil();
        info_prop.set_nil();
        info_self.set_nil();

        /* get the number of arguments to the function in this frame */
        fr_argc = G_interpreter->get_argc_from_frame(vmg_ fp);

        /* set up a function pointer for the method's entry address */
        func_ptr.set((const uchar *)G_code_pool->get_ptr(entry_addr));

        /* 
         *   to ensure we don't flush the caller out of the code pool cache,
         *   resolve the current entrypoint address immediately - we always
         *   have room for at least two code pages in the cache, so we know
         *   resolving just one won't throw the previous one out, so we
         *   simply need to make the current one most recently used by
         *   resolving it 
         */
        G_code_pool->get_ptr(G_interpreter->get_entry_ptr());

        /* get the current frame's defining object */
        def_obj = G_interpreter->get_defining_obj_from_frame(vmg_ fp);

        /* determine whether it's an object.prop or a function call */
        if (method_ofs == 0)
        {
            /* 
             *   a zero method offset indicates a recursive VM invocation
             *   from a native function, so we have no information on the
             *   call at all 
             */
            fr_argc = 0;
        }
        else if (def_obj == VM_INVALID_OBJ)
        {
            /* it's a function call */
            info_func.set_fnptr(entry_addr);
        }
        else
        {
            /* it's an object.prop invocation */
            info_obj.set_obj(def_obj); // $$$ walk up to base modified obj?
            info_prop.set_propid(
                G_interpreter->get_target_prop_from_frame(vmg_ fp));

            /* get the 'self' in this frame */
            info_self.set_obj(G_interpreter->get_self_from_frame(vmg_ fp));
        }

        /* 
         *   build the argument list and source location, except for system
         *   routines 
         */
        if (method_ofs != 0)
        {
            /* allocate a list object to store the argument list */
            info_args.set_obj(CVmObjList::create(vmg_ FALSE, fr_argc));
            arglst = (CVmObjList *)vm_objp(vmg_ info_args.val.obj);
            
            /* push the argument list for gc protection */
            G_stk->push(&info_args);
            
            /* build the argument list */
            for (i = 0 ; i < fr_argc ; ++i)
            {
                /* add this element to the argument list */
                arglst->cons_set_element(
                    i, G_interpreter->get_param_from_frame(vmg_ fp, i));
            }

            /* get the source location */
            get_source_info(vmg_ entry_addr, method_ofs, &info_srcloc);
        }
        else
        {
            /* 
             *   it's a system routine - no argument information is
             *   available, so return nil rather than an empty list to to
             *   indicate the absence 
             */
            info_args.set_nil();

            /* there's obviously no source location for system code */
            info_srcloc.set_nil();
        }

        /* 
         *   We have all of the information on this level now, so create the
         *   information object for the level.  This is an object of the
         *   exported stack-info class, which is a TadsObject type.  
         */
        G_stk->push(&info_srcloc);
        G_stk->push(&info_args);
        G_stk->push(&info_self);
        G_stk->push(&info_prop);
        G_stk->push(&info_obj);
        G_stk->push(&info_func);
        G_stk->push(&stack_info_cls);
        ele.set_obj(CVmObjTads::create_from_stack(vmg_ 0, 7));

        /* 
         *   the argument list is safely stashed away in the stack info
         *   object, so we can discard our gc protection for it now 
         */
        if (method_ofs != 0)
            G_stk->discard();

        /* 
         *   if we're fetching a single level, this is it - return the new
         *   stack info object and we're done
         */
        if (single_level >= 0)
        {
            /* return the single level object */
            retval_obj(vmg_ ele.val.obj);

            /* we're done */
            return;
        }

        /* add the new element to our list */
        lst->cons_set_element(level, &ele);

    done_with_level:
        /* move on to the enclosing frame */
        entry_addr =
            G_interpreter->get_enclosing_entry_ptr_from_frame(vmg_ fp);
        method_ofs = G_interpreter->get_return_ofs_from_frame(vmg_ fp);
    }

    /* return the list */
    retval_obj(vmg_ lst_val.val.obj);

    /* discard our gc protection */
    G_stk->discard();
}

/*
 *   Get the source file information for a given code pool offset.  If debug
 *   records aren't available for the given location, returns nil.  Returns
 *   a list containing the source file information: the first element is a
 *   string giving the name of the file, and the second element is an
 *   integer giving the line number in the file.  Returns nil if no source
 *   information is available for the given byte code location.  
 */
void CVmBifT3::get_source_info(VMG_ ulong entry_addr, ulong method_ofs,
                               vm_val_t *retval)
{
    CVmFuncPtr func_ptr;
    CVmDbgLinePtr line_ptr;
    ulong stm_start;
    ulong stm_end;
    CVmObjList *lst;
    vm_val_t ele;
    CVmSrcfEntry *srcf;
    CVmObjString *str;
    const char *fname;
    size_t map_len;

    /* presume we won't be able to find source information for the location */
    retval->set_nil();

    /* set up a debug table pointer for the function or method */
    func_ptr.set((const uchar *)G_code_pool->get_ptr(entry_addr));

    /* 
     *   resolve the current caller's entry code page to ensure it isn't
     *   flushed out of the code pool cache 
     */
    G_code_pool->get_ptr(G_interpreter->get_entry_ptr());

    /* get the debug information for the given location */
    if (!CVmRun::get_stm_bounds(vmg_ &func_ptr, method_ofs,
                                &line_ptr, &stm_start, &stm_end))
    {
        /* no source information available - return failure */
        return;
    }

    /* get the source file record - if we can't find it, return failure */
    srcf = (G_srcf_table != 0
            ? G_srcf_table->get_entry(line_ptr.get_source_id()) : 0);
    if (srcf == 0)
        return;

    /* 
     *   Create a list for the return value.  The return list has two
     *   elements: the name of the source file containing this code, and the
     *   line number in the file. 
     */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, 2));
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* push the list for gc protection */
    G_stk->push(retval);

    /* get the filename string */
    fname = srcf->get_name();

    /* 
     *   determine how long the string will be when translated to utf8 from
     *   the local filename character set 
     */
    map_len = G_cmap_from_fname->map_str(0, 0, fname);

    /* 
     *   create a string value to hold the filename, and store it in the
     *   first element of the return list (note that this automatically
     *   protects the new string from garbage collection, by virtue of the
     *   list referencing the string and the list itself being protected) 
     */
    ele.set_obj(CVmObjString::create(vmg_ FALSE, map_len));
    lst->cons_set_element(0, &ele);

    /* map the string into the buffer we allocated for it */
    str = (CVmObjString *)vm_objp(vmg_ ele.val.obj);
    G_cmap_from_fname->map_str(str->cons_get_buf(), map_len, fname);

    /* set the second element of the list to the source line number */
    ele.set_int(line_ptr.get_source_line());
    lst->cons_set_element(1, &ele);

    /* discard our gc protection */
    G_stk->discard();
}



/* ------------------------------------------------------------------------ */
/*
 *   T3 VM Test function set.  This function set contains internal test
 *   and debug functions.  These functions are not meant for use by
 *   "normal" programs - they provide internal access to certain VM state
 *   that is not useful or meaningful except for testing and debugging the
 *   VM itself.  
 */

/*
 *   Get an object's internal ID.  Takes an object instance and returns an
 *   integer giving the object's VM ID number.  This is effectively an
 *   address that can be used to refer to the object.  Because this value
 *   is returned as an integer, it is NOT a reference to the object for
 *   the purposes of garbage collection or finalization.  
 */
void CVmBifT3Test::get_obj_id(VMG_ uint argc)
{
    vm_val_t val;
    
    /* one argument required */
    check_argc(vmg_ argc, 1);

    /* get the object value */
    G_interpreter->pop_obj(vmg_ &val);

    /* return the object ID as an integer */
    retval_int(vmg_ (long)val.val.obj);
}

/*
 *   Get an object's garbage collection state.  Takes an object ID (NOT an
 *   object reference -- this is the integer value returned by get_obj_id)
 *   and returns a bit mask with the garbage collector state.
 *   
 *   (retval & 0x000F) gives the free state.  0 is free, 1 is in use.
 *   
 *   (retval & 0x00F0) gives the reachable state.  0x00 is unreachable,
 *   0x10 is finalizer-reachable, and 0x20 is fully reachable.
 *   
 *   (retval & 0x0F00) gives the finalizer state.  0x000 is unfinalizable,
 *   0x100 is finalizable, and 0x200 is finalized.
 *   
 *   (retval & 0xF000) gives the object ID validity.  0 is valid, 0xF000
 *   is invalid.  
 */
void CVmBifT3Test::get_obj_gc_state(VMG_ uint argc)
{
    vm_val_t val;

    /* one argument required */
    check_argc(vmg_ argc, 1);

    /* pop the string */
    G_interpreter->pop_int(vmg_ &val);

    /* return the internal garbage collector state of the object */
    retval_int(vmg_
               (long)G_obj_table->get_obj_internal_state(val.val.intval));
}

/*
 *   Get the Unicode character code of the first character of a string 
 */
void CVmBifT3Test::get_charcode(VMG_ uint argc)
{
    const char *str;

    /* one argument required */
    check_argc(vmg_ argc, 1);

    /* get the object ID as an integer */
    str = pop_str_val(vmg0_);

    /* 
     *   if the string is empty, return nil; otherwise, return the Unicode
     *   character code of the first character 
     */
    if (vmb_get_len(str) == 0)
    {
        /* empty string - return nil */
        retval_nil(vmg0_);
    }
    else
    {
        /* 
         *   get the character code of the first character and return it
         *   as an integer 
         */
        retval_int(vmg_ (int)utf8_ptr::s_getch(str + VMB_LEN));
    }
}

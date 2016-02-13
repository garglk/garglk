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
#include "vmlookup.h"
#include "vmimport.h"
#include "vmmeta.h"
#include "vmfref.h"


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
             || arg->typ == VM_BIFPTR
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
    check_argc_range(vmg_ argc, 0, 1);

    /* if there's an argument, it specifies which table to retrieve */
    int which = 1;
    if (argc >= 1)
        which = pop_int_val(vmg0_);

    /* return the desired table */
    switch (which)
    {
    case 1:
        /* return the loader's symbol table object, if any */
        retval_obj(vmg_ G_image_loader->get_reflection_symtab());
        break;

    case 2:
        /* return the macro table, if any */
        retval_obj(vmg_ G_image_loader->get_reflection_macros());
        break;

    case 3:
        /* other values are allowed but simply return nil */
        retval_nil(vmg0_);
        break;
    }
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
#define T3_GST_LOCALS   0x0001
#define T3_GST_FREFS    0x0002
void CVmBifT3::get_stack_trace(VMG_ uint argc)
{
    int single_level = 0;
    int level;
    vm_val_t *fp;
    vm_val_t lst_val;
    CVmObjList *lst;
    const uchar *entry_addr;
    ulong method_ofs;
    vm_val_t stack_info_cls;
    int want_named_args = FALSE;
    int want_locals = FALSE;
    int want_frefs = FALSE;
    int flags = 0;
    const vm_rcdesc *rc;

    /* check arguments */
    check_argc_range(vmg_ argc, 0, 2);

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

    /* 
     *   look up T3StackInfo.construct() to determine how many arguments it
     *   wants 
     */
    {
        int min_args, opt_args, varargs;
        if (vm_objp(vmg_ stack_info_cls.val.obj)->get_prop_interface(
            vmg_ stack_info_cls.val.obj, G_predef->obj_construct,
            min_args, opt_args, varargs))
        {
            /* check to see how many extra arguments they want */
            want_named_args = (min_args + opt_args >= 7 || varargs);
            want_locals = (min_args + opt_args >= 8 || varargs);
            want_frefs = (min_args + opt_args >= 9 || varargs);
        }
    }

    /* check to see if we're fetching a single level or the full trace */
    if (argc >= 1)
    {
        /* 
         *   Get the single level, and adjust to a 0 base.  If the level is
         *   nil, we're still getting all levels. 
         */
        if (G_stk->get(0)->typ == VM_NIL)
        {
            /* it's nil - get all levels */
            G_stk->discard();
        }
        else
        {
            /* get the level number */
            single_level = pop_int_val(vmg0_);

            /* make sure it's in range */
            if (single_level <= 0)
                err_throw(VMERR_BAD_VAL_BIF);
            
            /* we won't need a return list */
            lst_val.set_obj_or_nil(VM_INVALID_OBJ);
            lst = 0;
        }
    }

    /* get the flags argument, if present */
    if (argc >= 2)
        flags = pop_int_val(vmg0_);

    /* if we're not doing a single level, we need a list for the result */
    if (!single_level)
    {
        /* 
         *   We're returning a full list, so we need to allocate the list for
         *   the return value.  First, count stack levels to see how big a
         *   list we'll need.  
         */
        fp = G_interpreter->get_frame_ptr();
        entry_addr = G_interpreter->get_entry_ptr();
        method_ofs = G_interpreter->get_method_ofs();
        for (level = 0 ; fp != 0 ;
             fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp), ++level)
        {
            /* add an extra level for each system call */
            if (method_ofs == 0 && entry_addr != 0)
                ++level;

            /* get the return address */
            entry_addr =
                G_interpreter->get_enclosing_entry_ptr_from_frame(vmg_ fp);
            method_ofs = G_interpreter->get_return_ofs_from_frame(vmg_ fp);
        }

        /* create the list */
        lst_val.set_obj(CVmObjList::create(vmg_ FALSE, level));
        lst = (CVmObjList *)vm_objp(vmg_ lst_val.val.obj);

        /* 
         *   we create other objects while building this list, so the gc
         *   could run - clear the list to ensure it contains valid data 
         */
        lst->cons_clear();
        
        /* protect the list from garbage collection while we work */
        G_stk->push(&lst_val);

        /* flag that we're doing the whole stack */
        single_level = -1;
    }
    else
    {
        /* adjust the level to a 0-based index */
        single_level -= 1;
    }

    /* set up at the current function */
    fp = G_interpreter->get_frame_ptr();
    entry_addr = G_interpreter->get_entry_ptr();
    method_ofs = G_interpreter->get_method_ofs();
    rc = 0;

    /* traverse the frames */
    for (level = 0 ; fp != 0 ; ++level)
    {
        int fr_argc;
        int i;
        vm_obj_id_t def_obj;
        vm_val_t info_self;
        vm_val_t info_func;
        vm_val_t info_obj;
        vm_val_t info_prop;
        vm_val_t info_args;
        vm_val_t info_locals;
        vm_val_t info_srcloc;
        vm_val_t info_frameref;
        CVmObjList *arglst;
        vm_val_t ele;
        CVmFuncPtr func_ptr;
        int gc_cnt = 0;
        int info_argc = 0;

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
        info_locals.set_nil();
        info_frameref.set_nil();

        /* get the number of arguments to the function in this frame */
        fr_argc = G_interpreter->get_argc_from_frame(vmg_ fp);

        /* set up a function pointer for the method's entry address */
        func_ptr.set(entry_addr);

        /* get the current frame's defining object */
        def_obj = G_interpreter->get_defining_obj_from_frame(vmg_ fp);

        /* check for special method offsets */
        switch (method_ofs)
        {
        case VMRUN_RET_OP:
            /* the real return address is one past the last argument */
            method_ofs = G_interpreter->get_param_from_frame(vmg_ fp, argc)
                         ->val.intval;
            break;

        case VMRUN_RET_OP_ASILCL:
            /* the real return address is two past the last argument */
            method_ofs = G_interpreter->get_param_from_frame(vmg_ fp, argc+1)
                         ->val.intval;
            break;
        }

        /* determine whether it's an object.prop or a function call */
        if (method_ofs == 0)
        {
            /* 
             *   A zero method offset indicates a recursive VM invocation
             *   from a native function.  Presume we have no information on
             *   the caller.  
             */
            info_self.set_nil();
            fr_argc = 0;

            /* check for a native caller context */
            if (rc != 0)
            {
                /* check which kind of native caller we have */
                if (rc->bifptr.typ != VM_NIL)
                {
                    /* we have a built-in function at this level */
                    info_func = rc->bifptr;
                }
                else if (rc->self.typ != VM_NIL)
                {
                    /* it's an intrinsic class method - get the 'self' */
                    info_obj = info_self = rc->self;

                    /* get the metaclass */
                    CVmMetaclass *mc;
                    switch (info_obj.typ)
                    {
                    case VM_OBJ:
                        /* get the metaclass from the object */
                        mc = vm_objp(vmg_ info_obj.val.obj)
                             ->get_metaclass_reg();
                        break;

                    case VM_LIST:
                        /* list constant - use the List metaclass */
                        mc = CVmObjList::metaclass_reg_;
                        break;

                    case VM_SSTRING:
                        /* string constant - use the String metaclass */
                        mc = CVmObjString::metaclass_reg_;
                        break;

                    default:
                        /* other types don't have metaclasses */
                        mc = 0;
                        break;
                    }

                    /* get the registration table entry */
                    vm_meta_entry_t *me =
                        mc == 0 ? 0 :
                        G_meta_table->get_entry_from_reg(mc->get_reg_idx());

                    /* get the metaclass and property from the entry */
                    if (me != 0)
                    {
                        /* set 'obj' to the IntrinsicClass object */
                        info_obj.set_obj(me->class_obj_);

                        /* get the property ID */
                        info_prop.set_propid(me->xlat_func(rc->method_idx));
                    }
                }
            }
        }
        else if (def_obj == VM_INVALID_OBJ)
        {
            /* there's no defining object, so this is a function call */
            func_ptr.get_fnptr(vmg_ &info_func);
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
        if (method_ofs != 0 || rc != 0)
        {
            /* allocate a list object to store the argument list */
            int ac = (rc != 0 ? rc->argc : fr_argc);
            info_args.set_obj(CVmObjList::create(vmg_ FALSE, ac));
            arglst = (CVmObjList *)vm_objp(vmg_ info_args.val.obj);
            
            /* push the argument list for gc protection */
            G_stk->push(&info_args);
            ++gc_cnt;
            
            /* build the argument list */
            for (i = 0 ; i < ac ; ++i)
            {
                /* add this element to the argument list */
                const vm_val_t *v =
                    (rc != 0
                     ? rc->argp - i
                     : G_interpreter->get_param_from_frame(vmg_ fp, i));
                arglst->cons_set_element(i, v);
            }

            /* get the source location */
            get_source_info(vmg_ entry_addr, method_ofs, &info_srcloc);

            /* 
             *   if they want locals, and this isn't a recursive native
             *   caller, retrieve them 
             */
            if (rc == 0
                && (((flags & T3_GST_LOCALS) != 0 && want_locals)
                    || ((flags & T3_GST_FREFS) != 0 && want_frefs)))
            {
                /* get the locals */
                get_stack_locals(vmg_ fp, entry_addr, method_ofs,
                                 (flags & T3_GST_LOCALS) != 0 && want_locals
                                 ? &info_locals : 0,
                                 (flags & T3_GST_FREFS) != 0 && want_frefs
                                 ? &info_frameref : 0);

                /* 
                 *   that leaves the LookupTable and StackFrameDesc on the
                 *   stack, so note that we need to discard the stack level
                 *   when we're done with it 
                 */
                if (info_locals.typ == VM_OBJ)
                    ++gc_cnt;
                if (info_frameref.typ == VM_OBJ)
                    ++gc_cnt;
            }
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

        /* start with the original complement of arguments */
        info_argc = 7;

        /* 
         *   if we have a modern T3StackInfo object, push the locals,
         *   named argument elements, and frame reference object
         */
        if (want_frefs)
        {
            G_stk->push(&info_frameref);
            ++info_argc;
        }
        if (want_named_args)
        {
            /* 
             *   the constructor has a slot for named arguments - push either
             *   a table or nil, depending...
             */
            vm_val_t *argp;
            const uchar *t = 0;

            /* if the flags request locals, retrieve the named arguments */
            if ((flags & T3_GST_LOCALS) != 0)
                t = CVmRun::get_named_args_from_frame(vmg_ fp, &argp);

            /* 
             *   if we do in fact have named arguments, build a lookup table
             *   copy and push it; otherwise just push nil 
             */
            if (t != 0)
            {
                /* get the number of table entries */
                int n = osrp2(t);
                t += 2;
                
                /* create a lookup table for the arguments */
                G_stk->push()->set_obj(CVmObjLookupTable::create(
                    vmg_ FALSE, n <= 8 ? 8 : n <= 32 ? 32 : 64, n));
                CVmObjLookupTable *lt = (CVmObjLookupTable *)vm_objp(
                    vmg_ G_stk->get(0)->val.obj);
                
                /* 
                 *   Populate the lookup table with the named arguments.  The
                 *   compiler builds the table in the order pushed, which is
                 *   right to left.  Lookup tables preserve the order in
                 *   which elements are added, and reflect this order in key
                 *   lists, so to that extent the order of building the
                 *   lookup table matters.  For readability of the generated
                 *   list, in case it's presented to the user, build the
                 *   table in left-to-right order, which is the reverse of
                 *   the table order in the bytecode table.  
                 */
                argp += n - 1;
                for (int i = (n-1)*2 ; i >= 0 ; i -= 2, --argp)
                {
                    /* get the name pointer and length from the index */
                    uint ofs = osrp2(t + i), nxtofs = osrp2(t + i + 2);
                    const char *name = (const char *)t + ofs;
                    size_t len = nxtofs - ofs;
                    
                    /* create a string from the name */
                    vm_val_t str;
                    str.set_obj(CVmObjString::create(vmg_ FALSE, name, len));
                    
                    /* add it to the table */
                    lt->add_entry(vmg_ &str, argp);
                }
            }
            else
            {
                /* there are no named arguments - push nil */
                G_stk->push()->set_nil();
            }

            /* count the argument */
            ++info_argc;
        }
        if (want_locals)
        {
            G_stk->push(&info_locals);
            ++info_argc;
        }

        /* push the rest of the arguments */
        G_stk->push(&info_srcloc);
        G_stk->push(&info_args);
        G_stk->push(&info_self);
        G_stk->push(&info_prop);
        G_stk->push(&info_obj);
        G_stk->push(&info_func);
        G_stk->push(&stack_info_cls);
        ele.set_obj(CVmObjTads::create_from_stack(vmg_ 0, info_argc));

        /* discard the gc protection items */
        G_stk->discard(gc_cnt);

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
        /*
         *   If this is a system call level, and we're not in debug mode,
         *   this recursive frame contains the entry address for the caller,
         *   but not the calling byte-code address.  Stay on the current
         *   level in this case.  
         */
        if (method_ofs == 0 && entry_addr != 0)
        {
            /* 
             *   This is a recursive caller, and we have a valid entry
             *   address for the prior frame.  Stay in the current frame, and
             *   retrieve the actual return address from the calling frame.  
             */
            if (rc != 0)
            {
                /* get the actual return address from the recursive context */
                method_ofs = rc->get_return_addr() - entry_addr;

                /* 
                 *   we're now in the bytecode part of the frame, so forget
                 *   the recursive context 
                 */
                rc = 0;
            }
            else
            {
                /* no recursive context - use a fake return address */
                method_ofs = G_interpreter->get_funchdr_size();
            }
        }
        else
        {
            /* move up to the enclosing frame */
            entry_addr =
                G_interpreter->get_enclosing_entry_ptr_from_frame(vmg_ fp);
            method_ofs = G_interpreter->get_return_ofs_from_frame(vmg_ fp);
            rc = G_interpreter->get_rcdesc_from_frame(vmg_ fp);
            fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp);
        }
    }

    /* return the list */
    retval_obj(vmg_ lst_val.val.obj);

    /* discard our gc protection */
    G_stk->discard();
}

/*
 *   Get the local variable table for a given stack level.  We'll fill in
 *   'retval' with a LooupTable object containing the local values keyed by
 *   symbol name, or nil if we can't find the locals.  We'll also push the
 *   object onto the stack for gc protection, so the caller must pop it when
 *   done with it.  
 */
void CVmBifT3::get_stack_locals(VMG_ vm_val_t *fp,
                                const uchar *entry_addr, ulong method_ofs,
                                vm_val_t *local_tab, vm_val_t *frameref_obj)
{
    /* presume we won't be able to find source information for the location */
    if (local_tab != 0)
        local_tab->set_nil();
    if (frameref_obj != 0)
        frameref_obj->set_nil();

    /* we haven't found any relevant frames yet */
    int best = 0;

    /* set up a debug table pointer for the function or method */
    CVmDbgTablePtr dp;
    if (dp.set(entry_addr))
    {
        /* search for the innermost frame containing the method offset */
        int frame_cnt = dp.get_frame_count(vmg0_);
        for (int i = 1 ; i <= frame_cnt ; ++i)
        {
            /* get this frame */
            CVmDbgFramePtr fp;
            dp.set_frame_ptr(vmg_ &fp, i);
            
            /* check to see if this contains method_ofs */
            uint start_ofs = fp.get_start_ofs(vmg0_);
            uint end_ofs = fp.get_end_ofs(vmg0_);
            if (method_ofs >= start_ofs && method_ofs <= end_ofs)
            {
                /* 
                 *   This is a relevant frame.  If we don't have any frames
                 *   yet, or this one is nested within the best we've found
                 *   so far, this is the best frame so far.  
                 */
                if (best == 0 || fp.is_nested_in(vmg_ &dp, best))
                    best = i;
            }
        }
    }

    /* get the frame-reference, if the caller wants one */
    if (frameref_obj != 0)
    {
        /* get the slot */
        vm_val_t *fro = G_interpreter->get_frameref_slot(vmg_ fp);

        /* if there's not already a StackFrameRef object there, create one */
        if (fro->typ == VM_NIL)
        {
            /* create the StackFrameRef and store it in the stack slot */
            fro->set_obj(CVmObjFrameRef::create(vmg_ fp, entry_addr));
        }

        /* create the StackFrameDesc to return to the caller */
        frameref_obj->set_obj(CVmObjFrameDesc::create(
            vmg_ fro->val.obj, best, method_ofs));

        /* push it for gc protection */
        G_stk->push(frameref_obj);
    }

    /* if they didn't want the local variable table, we're done */
    if (local_tab == 0)
        return;

    /* create a lookup table for the locals */
    local_tab->set_obj(CVmObjLookupTable::create(vmg_ FALSE, 32, 64));
    CVmObjLookupTable *t =
        (CVmObjLookupTable *)vm_objp(vmg_ local_tab->val.obj);

    /* save it on the stack for gc protection */
    G_stk->push(local_tab);

    /* if we didn't find a best frame, there are no locals to retrieve */
    if (best == 0)
        return;

    /* set up a pointer to our winning frame */
    CVmDbgFramePtr dfp;
    dp.set_frame_ptr(vmg_ &dfp, best);

    /* walk up the list of frames from the innermost to outermost */
    for (;;)
    {
        /* set up a pointer to the first symbol */
        CVmDbgFrameSymPtr symp;
        dfp.set_first_sym_ptr(vmg_ &symp);

        /* scan this frame's local symbol list */
        int sym_cnt = dfp.get_sym_count();
        for (int i = 0 ; i < sym_cnt ; ++i, symp.inc(vmg0_))
        {
            vm_val_t name, val;
            
            /* get the symbol name */
            symp.get_str_val(vmg_ &name);

            /* 
             *   if this symbol is already in the table, skip it - we work
             *   from inner to outer frames, and inner frames hide symbols in
             *   outer frames 
             */
            if (t->index_check(vmg_ &val, &name))
                continue;

            /* get the value from the frame */
            G_interpreter->get_local_from_frame(vmg_ &val, fp, &symp);
            
            /* add this to the lookup table */
            t->add_entry(vmg_ &name, &val);
        }

        /* get the enclosing frame - 0 means no parent */
        int parent = dfp.get_enclosing_frame();
        if (parent == 0)
            break;

        /* set up dfp to point to the parent frame */
        dp.set_frame_ptr(vmg_ &dfp, parent);
    }
}

/*
 *   Get the source file information for a given code pool offset.  If debug
 *   records aren't available for the given location, returns nil.  Returns
 *   a list containing the source file information: the first element is a
 *   string giving the name of the file, and the second element is an
 *   integer giving the line number in the file.  Returns nil if no source
 *   information is available for the given byte code location.  
 */
void CVmBifT3::get_source_info(VMG_ const uchar *entry_addr, ulong method_ofs,
                               vm_val_t *retval)
{
    CVmFuncPtr func_ptr;
    CVmDbgLinePtr line_ptr;
    const uchar *stm_start, *stm_end;
    CVmObjList *lst;
    vm_val_t ele;
    CVmSrcfEntry *srcf;
    CVmObjString *str;
    const char *fname;
    size_t map_len;

    /* presume we won't be able to find source information for the location */
    retval->set_nil();

    /* set up a debug table pointer for the function or method */
    func_ptr.set(entry_addr);

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

    /* clear the list, in case gc runs during construction */
    lst->cons_clear();

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


/*
 *   Look up a named argument.  If 'mandatory' is set, we throw an error if
 *   we can't find a resolution. 
 */
void CVmBifT3::get_named_arg(VMG_ uint argc)
{
    /* check arguments */
    check_argc_range(vmg_ argc, 1, 2);

    /* get the name we're looking for */
    const char *name = G_stk->get(0)->get_as_string(vmg0_);
    if (name == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the length and buffer pointer */
    size_t namelen = vmb_get_len(name);
    name += VMB_LEN;

    /* 
     *   Scan the stack for named parameter tables.  A named parameter table
     *   is always in the calling frame at the stack slot just beyond the
     *   last argument.  
     */
    for (vm_val_t *fp = G_interpreter->get_frame_ptr() ; fp != 0 ;
         fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp))
    {
        /* check for a table in this frame */
        vm_val_t *argp;
        const uchar *t = CVmRun::get_named_args_from_frame(vmg_ fp, &argp);
        if (t != 0)
        {
            /* get the number of table entries */
            int n = osrp2(t);
            t += 2;

            /* scan the table for the name */
            for (int i = 0 ; n >= 0 ; --n, i += 2, ++argp)
            {
                /* get this element's offset, and figure its length */
                uint eofs = osrp2(t + i);
                uint elen = osrp2(t + i + 2) - eofs;

                /* check for a match */
                if (elen == namelen && memcmp(name, t + eofs, elen) == 0)
                {
                    /* found it - return the value */
                    retval(vmg_ argp);

                    /* discard arguments and return */
                    G_stk->discard(argc);
                    return;
                }
            }
        }
    }

    /* 
     *   The argument is undefined.  If a default value was supplied, simply
     *   return the default value.  Otherwise throw an error.  
     */
    if (argc >= 2)
    {
        /* a default value was supplied - simply return it */
        retval(vmg_ G_stk->get(1));

        /* discard arguments */
        G_stk->discard(argc);
    }
    else
    {
        /* no default value - throw an error */
        err_throw_a(VMERR_MISSING_NAMED_ARG, 1, ERR_TYPE_TEXTCHAR_LEN,
                    name, namelen);
    }
}

/*
 *   Retrieve a list of the named argument names.  
 */
void CVmBifT3::get_named_arg_list(VMG_ uint argc)
{
    /* check arguments */
    check_argc(vmg_ argc, 0);
    
    /* create the result list; we'll expand as necessary later */
    G_stk->push()->set_obj(CVmObjList::create(vmg_ FALSE, 10));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ G_stk->get(0)->val.obj);

    /* clear it out, since we're building it incrementally */
    lst->cons_clear();

    /* we haven't added any elements yet */
    int idx = 0;

    /* scan the stack and populate the name list from the tables we find */
    for (vm_val_t *fp = G_interpreter->get_frame_ptr() ; fp != 0 ;
         fp = G_interpreter->get_enclosing_frame_ptr(vmg_ fp))
    {
        /* look for a named argument table in this frame */
        vm_val_t *argp;
        const uchar *t = CVmRun::get_named_args_from_frame(vmg_ fp, &argp);
        if (t != 0)
        {
            /* get the number of table entries */
            int n = osrp2(t);
            t += 2;

            /* 
             *   Build the list.  The compiler generates the list in
             *   right-to-left order (the order of pushing the arguments).
             *   For readability, reverse this: generate the list left to
             *   right, so that it appears in the original source code order.
             */
            argp += n - 1;
            for (int i = (n-1)*2 ; i >= 0 ; i -= 2, --argp)
            {
                /* get this string's offset and figure its length */
                uint ofs = osrp2(t + i);
                uint len = osrp2(t + i + 2) - ofs;
                
                /* create a string from the name */
                vm_val_t str;
                str.set_obj(CVmObjString::create(
                    vmg_ FALSE, (const char *)t + ofs, len));

                /* add it to the list */
                lst->cons_ensure_space(vmg_ idx, 10);
                lst->cons_set_element(idx, &str);
                ++idx;
            }
        }
    }

    /* set the final list length */
    lst->cons_set_len(idx);

    /* keep only the unique elements */
    lst->cons_uniquify(vmg0_);

    /* return the results */
    retval_pop(vmg0_);
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


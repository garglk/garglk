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
  vmanonfn.cpp - anonymous function metaclass
Function
  
Notes
  
Modified
  03/21/00 MJRoberts  - Creation
*/

#include "t3std.h"
#include "vmobj.h"
#include "vmanonfn.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmrun.h"
#include "vmstack.h"
#include "vmpredef.h"


/* ------------------------------------------------------------------------ */
/*
 *   Statics 
 */
/* metaclass registration object */
static CVmMetaclassAnonFn metaclass_reg_obj;
CVmMetaclass *CVmObjAnonFn::metaclass_reg_ = &metaclass_reg_obj;


/* ------------------------------------------------------------------------ */
/*
 *   create from stack arguments
 */
vm_obj_id_t CVmObjAnonFn::create_from_stack(VMG_ const uchar **pc_ptr,
                                            uint argc)
{
    vm_obj_id_t id;
    vm_val_t funcptr;
    CVmObjAnonFn *new_obj;
    uint idx;
    
    /* at least one argument is required (the function pointer) */
    if (argc < 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* retrieve our function pointer argument */
    G_stk->pop(&funcptr);
    if (funcptr.typ == VM_FUNCPTR)
    {
        /* it's a regular function pointer - accept it */
    }
    else if (funcptr.typ == VM_OBJ
             && vm_objp(vmg_ funcptr.val.obj)->get_invoker(vmg_ 0))
    {
        /* it's a pointer to an invokable object - accept it */
    }
    else
    {
        /* it's not a valid function pointer */
        err_throw(VMERR_FUNCPTR_VAL_REQD);
    }

    /* create the new object */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* create the new object, giving it one slot per constructor argument */
    new_obj = new (vmg_ id) CVmObjAnonFn(vmg_ argc);

    /* set the first element to our function pointer */
    new_obj->set_element(0, &funcptr);

    /* set the remaining elements to the context objects */
    for (idx = 1 ; idx < argc ; ++idx)
    {
        vm_val_t val;
        
        /* pop this value */
        G_stk->pop(&val);

        /* set the element */
        new_obj->set_element(idx, &val);
    }

    /* return the new object ID */
    return id;
}

/*
 *   Invoke 
 */
int CVmObjAnonFn::get_invoker(VMG_ vm_val_t *val)
{
    /* our first vector element is our function pointer */
    if (val != 0)
    {
        /* get the function pointer */
        get_element(0, val);

        /* if this is itself an invokable value, get its invoker */
        if (val->typ == VM_OBJ)
            return vm_objp(vmg_ val->val.obj)->get_invoker(vmg_ val);
    }

    /* we are indeed invokable */
    return TRUE;
}


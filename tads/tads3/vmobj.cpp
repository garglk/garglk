#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMOBJ.CPP,v 1.4 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmobj.cpp - VM object manager
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmstack.h"
#include "vmundo.h"
#include "vmrun.h"
#include "vmfile.h"
#include "vmmeta.h"
#include "vmlst.h"
#include "vmstr.h"
#include "vmintcls.h"
#include "vmpool.h"
#include "vmfunc.h"
#include "vmpredef.h"
#include "vmhash.h"
#include "vmtobj.h"
#include "vmanonfn.h"
#include "vmdynfunc.h"



/* ------------------------------------------------------------------------ */
/*
 *   Statistics gathering 
 */
#ifdef VMOBJ_GC_STATS
# define IF_GC_STATS(x) x

struct
{
    void gc_stats()
    {
        runs = 0;
        tot_freed = 0;
        cur_freed = 0;
        max_freed = 0;
        t = 0;
    }

    void begin_pass()
    {
        t0 = os_get_sys_clock_ms();
        runs++;
        pass_start_bytes = cur_bytes;
        cur_freed = 0;
    }

    void end_pass()
    {
        t += os_get_sys_clock_ms() - t0;
        if (cur_freed > max_freed)
            max_freed = cur_freed;
        long garbage_bytes = pass_start_bytes - cur_bytes;
        if (garbage_bytes > max_garbage_bytes)
            max_garbage_bytes = garbage_bytes;
    }

    void count_free()
    {
        ++cur_freed;
        ++tot_freed;
    }

    void count_alloc_bytes(size_t siz)
    {
        cur_bytes += siz;
        if (cur_bytes > max_bytes)
            max_bytes = cur_bytes;
    }

    void count_realloc_bytes(size_t oldsiz, size_t newsiz)
    {
        count_alloc_bytes(newsiz);
        count_free_bytes(oldsiz);
    }

    void count_free_bytes(size_t siz)
    {
        cur_bytes -= siz;
    }

    void display()
    {
        printf("Garbage collection statistics:\n"
               "  collection runs:       %ld\n"
               "  objects freed:         %ld\n"
               "  average freed per run: %ld\n"
               "  max freed in one run:  %ld\n"
               "  peak heap bytes:       %ld\n"
               "  peak garbage bytes:    %ld\n"
               "  total gc time (ms):    %ld\n"
               "  average gc time (ms):  %ld\n",
               runs,
               tot_freed,
               runs != 0 ? tot_freed/runs : 0,
               max_freed,
               max_bytes,
               max_garbage_bytes,
               t,
               runs != 0 ? t/runs : 0);
    }

    /* number of times the gc has run */
    long runs;

    /* total number of objects collected */
    long tot_freed;

    /* number of objects collected on this pass */
    long cur_freed;

    /* maximum number collected on any one pass */
    long max_freed;

    /* current total heap manager allocated bytes */
    long cur_bytes;

    /* allocated bytes at start of last gc pass */
    long pass_start_bytes;

    /* peak allocated bytes */
    long max_bytes;

    /* peak garbage bytes */
    long max_garbage_bytes;

    /* elapsed time in garbage collector */
    long t;

    /* starting time in ticks of current run */
    long t0;

} gc_stats;

#else /* VMOBJ_GC_STATS */
# define IF_GC_STATS(x)
#endif /* VMOBJ_GC_STATS */


/* ------------------------------------------------------------------------ */
/*
 *   Base fixed-size object entry implementation 
 */

/* 
 *   Metaclass registration object for the root object class.  Note that a
 *   root object can never be instantiated; this entry is purely for the
 *   use of the type system.  
 */
static CVmMetaclassRoot metaclass_reg_obj;
CVmMetaclass *CVmObject::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObject::
     *CVmObject::func_table_[])(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc,
                                vm_prop_id_t prop, vm_obj_id_t *source_obj) =
{
    &CVmObject::getp_undef,
    &CVmObject::getp_of_kind,
    &CVmObject::getp_sclist,
    &CVmObject::getp_propdef,
    &CVmObject::getp_proptype,
    &CVmObject::getp_get_prop_list,
    &CVmObject::getp_get_prop_params,
    &CVmObject::getp_is_class,
    &CVmObject::getp_propinh,
    &CVmObject::getp_is_transient
};

/*
 *   Allocate space for an object from a page table, given the object ID.
 *   The caller must allocate the object ID prior to new'ing the object;
 *   operator new will store the memory for the new object in the object
 *   slot the caller allocated.
 */
void *CVmObject::operator new(size_t siz, VMG_ vm_obj_id_t obj_id)
{
    /* 
     *   The size must be the size of an object entry.  This size never
     *   changes, even for subclasses of the object type, since all
     *   variable-size data must be stored in the variable-size portion of
     *   the object.  Here we are only concerned with allocating the
     *   fixed-size object descriptor. 
     */
    assert(siz == sizeof(CVmObject));

    /* return the memory contained in the object entry */
    return G_obj_table->get_obj(obj_id);
}

/*
 *   Determine if this object is an instance of the given object.  By
 *   default, we will simply check to see if the given object is the
 *   IntrinsicClass instance that represents our metaclass or one of its
 *   superclasses.  
 */
int CVmObject::is_instance_of(VMG_ vm_obj_id_t obj)
{
    vm_meta_entry_t *entry;

    /* 
     *   we can only be an instance of the object if the object is an
     *   IntrinsicClass instance, since by default we have only intrinsic
     *   classes among our superclasses 
     */
    if (!CVmObjClass::is_intcls_obj(vmg_ obj))
        return FALSE;
    
    /* 
     *   look up my metaclass in the metaclass dependency table, and
     *   determine if my dependency table entry's record of the
     *   IntrinsicClass object for the metaclass matches the given object 
     */
    entry = (G_meta_table
             ->get_entry_from_reg(get_metaclass_reg()->get_reg_idx()));

    /* 
     *   if we have an entry, ask our superclass object if it is an
     *   instance of the given object; otherwise, we must not be an
     *   instance 
     */
    if (entry != 0)
    {
        /* if this is our direct superclass, we're an instance */
        if (entry->class_obj_ == obj)
            return TRUE;

        /* if there's no intrinsic class object, return false */
        if (entry->class_obj_ == VM_INVALID_OBJ)
            return FALSE;

        /* ask the superclass if it inherits from the given object */
        return vm_objp(vmg_ entry->class_obj_)->is_instance_of(vmg_ obj);
    }
    else
    {
        /* 
         *   no metaclass table entry - we can't really make any
         *   determination, so indicate that we're not an instance 
         */
        return FALSE;
    }
}

/*
 *   Get the nth superclass.  By default, an object's superclass is
 *   represented by the intrinsic class object for the metaclass. 
 */
vm_obj_id_t CVmObject::get_superclass(VMG_ vm_obj_id_t /*self*/,
                                      int sc_idx) const
{
    vm_meta_entry_t *entry;

    /* we only have one superclass */
    if (sc_idx != 0)
        return VM_INVALID_OBJ;

    /* look up the metaclass entry */
    entry = (G_meta_table
             ->get_entry_from_reg(get_metaclass_reg()->get_reg_idx()));

    /* return the IntrinsicClass object that represents this metaclass */
    return (entry != 0 ? entry->class_obj_ : VM_INVALID_OBJ);
}

/*
 *   Get a property 
 */
int CVmObject::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                        vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc)
{
    uint func_idx;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* if we find it, the source object is the 'Object' intrinsic class */
    *source_obj = metaclass_reg_->get_class_obj(vmg0_);

    /* call the appropriate function */
    return (this->*func_table_[func_idx])(vmg_ self, retval, argc,
                                          prop, source_obj);
}

/*
 *   Inherit a property 
 */
int CVmObject::inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                        vm_obj_id_t self, vm_obj_id_t orig_target_obj,
                        vm_obj_id_t defining_obj, vm_obj_id_t *source_obj,
                        uint *argc)
{
    uint func_idx;

    /*
     *   We're inheriting.  This is never called from native code, as native
     *   code does its inheriting directly through C++ calls to base class
     *   native code; hence, we can only be called from a byte-code modifier
     *   object.
     *   
     *   First, try looking for a native implementation.  We can reach this
     *   point if a byte-code object overrides an intrinsic method, then
     *   inherits from the byte-code override.  
     */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    if (func_idx != 0)
    {
        /* the source object is the 'Object' intrinsic class */
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);

        /* call the native implementation */
        return (this->*func_table_[func_idx])(vmg_ self, retval, argc,
                                              prop, source_obj);
    }

    /*
     *   We didn't find it among the intrinsic methods, so look at the
     *   modifier objects.  
     */
    return find_modifier_prop(vmg_ prop, retval, self, orig_target_obj,
                              defining_obj, source_obj, argc);
}

/*
 *   Cast the object to a string.  If we got here, it means that there's no
 *   metaclass override, so use reflection services if available, or the
 *   basic "object#xxx" template.
 */
const char *CVmObject::cast_to_string(VMG_ vm_obj_id_t self,
                                      vm_val_t *new_str) const
{
    /* set up a vm_val_t for 'self' */
    vm_val_t val;
    val.set_obj(self);

    /* try calling self.objToString */
    vm_prop_id_t objToString = G_predef->objToString;
    if (objToString != VM_INVALID_PROP)
    {
        /* call self.objToString */
        G_interpreter->get_prop(vmg_ 0, &val, objToString, &val, 0, 0);

        /* if that yielded a string, return it */
        const char *str = G_interpreter->get_r0()->get_as_string(vmg0_);
        if (str != 0)
        {
            /* set the return value */
            *new_str = *G_interpreter->get_r0();

            /* return the string buffer */
            return str;
        }
    }

    /* try getting a symbolic name, otherwise use the object#xxx template */
    return CVmObjString::reflect_to_str(
        vmg_ new_str, 0, 0, &val, "object#%ld", (long)self);
}


/* 
 *   Any object can be listlike, even if it doesn't natively implement
 *   indexing, if it defines operator[] and 'length' as user-code methods.  
 */
int CVmObject::is_listlike(VMG_ vm_obj_id_t self)
{
    vm_val_t v;
    vm_obj_id_t o;
    int minargs, optargs, varargs;

    /* check for the required imports */
    if (G_predef->operator_idx == VM_INVALID_PROP
        || G_predef->length_prop == VM_INVALID_PROP)
        return FALSE;

    /* check for operator [] */
    if (!get_prop(vmg_ G_predef->operator_idx, &v, self, &o, 0))
        return FALSE;

    /* check for a 'length' method taking no arguments */
    if (!get_prop_interface(vmg_ self, G_predef->length_prop,
                            minargs, optargs, varargs)
        || minargs > 0)
        return FALSE;

    /* it's list-like */
    return TRUE;
}

/*
 *   An object that doesn't have a native list-like interface can still
 *   provide list-like operations by defining operator[] and 'length' in user
 *   code.  
 */
int CVmObject::ll_length(VMG_ vm_obj_id_t self)
{
    /* make a recursive call to 'length' */
    vm_val_t vself;
    vself.set_obj(self);
    vm_rcdesc rc("Object.ll_length");
    G_interpreter->get_prop(vmg_ 0, &vself, G_predef->length_prop,
                            &vself, 0, &rc);

    /* the return value in R0 is the length */
    vm_val_t *r0 = G_interpreter->get_r0();
    if (r0->typ == VM_INT)
        return r0->val.intval;
    else
        return -1;
}

/*
 *   Index the object, with overloading if there's no native implementation. 
 */
void CVmObject::index_val_ov(VMG_ vm_val_t *result, vm_obj_id_t self,
                             const vm_val_t *index_val)
{
    /* try the native operation first */
    if (!index_val_q(vmg_ result, self, index_val))
    {
        /* no native indexing - try the operator[] overload */
        vm_val_t vself;
        vself.set_obj(self);
        G_stk->push(index_val);
        G_interpreter->op_overload(vmg_ 0, -1, &vself, G_predef->operator_idx,
                                   1, VMERR_CANNOT_INDEX_TYPE);

        /* return the result from R0 */
        *result = *G_interpreter->get_r0();
    }
}

/*
 *   Index the object, with overloading if there's no native implementation. 
 */
void CVmObject::set_index_val_ov(VMG_ vm_val_t *new_container,
                                 vm_obj_id_t self,
                                 const vm_val_t *index_val,
                                 const vm_val_t *new_val)
{
    /* try the native indexing first */
    if (!set_index_val_q(vmg_ new_container, self, index_val, new_val))
    {
        /* no native indexing - try the operator[]= overload */
        vm_val_t vself;
        vself.set_obj(self);
        G_stk->push(new_val);
        G_stk->push(index_val);
        G_interpreter->op_overload(vmg_ 0, -1,
                                   &vself, G_predef->operator_setidx,
                                   2, VMERR_CANNOT_INDEX_TYPE);

        /* return the new container result from R0 */
        *new_container = *G_interpreter->get_r0();
    }
}

/*
 *   Get the next value from an iteration 
 */
int CVmObject::iter_next(VMG_ vm_obj_id_t selfobj, vm_val_t *val)
{
    /* check that the iterator properties are defined by the program */
    if (G_iter_next_avail == VM_INVALID_PROP
        || G_iter_get_next == VM_INVALID_PROP)
        return FALSE;

    /* call isNextAvailable */
    vm_rcdesc rc("for..in");
    vm_val_t self;
    self.set_obj(selfobj);
    G_interpreter->get_prop(vmg_ 0, &self, G_iter_next_avail, &self, 0, &rc);

    /* if that returned false (nil or 0), return false */
    vm_val_t *ret = G_interpreter->get_r0();
    if (ret->typ == VM_NIL || (ret->typ == VM_INT && ret->val.intval == 0))
        return FALSE;

    /* call getNext and return the result */
    G_interpreter->get_prop(vmg_ 0, &self, G_iter_get_next, &self, 0, &rc);
    *val = *G_interpreter->get_r0();
    return TRUE;
}


/*
 *   Get a property that isn't defined in our property table 
 */
int CVmObject::getp_undef(VMG_ vm_obj_id_t self,
                          vm_val_t *retval, uint *argc,
                          vm_prop_id_t prop, vm_obj_id_t *source_obj)
{
    /* 
     *   We didn't find a native implementation of the method, but there's
     *   still one more place to look: the "modifier" object for our class
     *   tree.  Modifier objects are byte-code objects that can provide
     *   implementations of methods that add to intrinsic classes (modifiers
     *   can't override intrinsic methods, but they can add new methods).
     *   
     *   Since we're looking for a property on an initial get-property call
     *   (not an inheritance call), we don't yet have a defining object to
     *   find and skip in the inheritance tree, so use VM_INVALID_OBJ as the
     *   defining object.  In addition, we're directly calling the method, so
     *   the target object is the same as the 'self' object.  
     */
    return find_modifier_prop(vmg_ prop, retval, self, self,
                              VM_INVALID_OBJ, source_obj, argc);
}

/*
 *   Find a modifier property.  
 */
int CVmObject::find_modifier_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                                  vm_obj_id_t self,
                                  vm_obj_id_t orig_target_obj,
                                  vm_obj_id_t defining_obj,
                                  vm_obj_id_t *source_obj,
                                  uint *argc)
{
    vm_meta_entry_t *entry;
    int found_def_obj;

    /* we haven't yet found the defining superclass */
    found_def_obj = FALSE;

    /* get my metaclass from the dependency table */
    entry = (G_meta_table
             ->get_entry_from_reg(get_metaclass_reg()->get_reg_idx()));

    /* 
     *   if there's an associated intrinsic class object, check to see if
     *   it provides a user modifier object for this intrinsic class 
     */
    while (entry != 0 && entry->class_obj_ != VM_INVALID_OBJ)
    {
        vm_obj_id_t mod_obj;
        
        /* ask the intrinsic class object for the user modifier object */
        mod_obj = ((CVmObjClass *)vm_objp(vmg_ entry->class_obj_))
                  ->get_mod_obj();

        /* 
         *   If we have a defining object, we must ignore objects in the
         *   superclass tree until we find the defining object.  Therefore,
         *   scan up the superclass tree for mod_obj and see if we can find
         *   the defining object; when we find it, we can start looking at
         *   objects for real at the defining object's superclass.
         *   
         *   (Superclasses in modifier objects aren't real superclasses,
         *   because modifier objects are classless.  Instead, the superclass
         *   list simply implements the 'modify' chain.)  
         */
        if (mod_obj != VM_INVALID_OBJ
            && defining_obj != VM_INVALID_OBJ && !found_def_obj)
        {
            /* 
             *   if the defining object isn't among the byte-code
             *   superclasses of the modifier object, we must skip this
             *   entire intrinsic class and move to the intrinsic superclass 
             */
            if (mod_obj == defining_obj
                || vm_objp(vmg_ mod_obj)->is_instance_of(vmg_ defining_obj))
            {
                /* 
                 *   the defining object is among my modifier family - this
                 *   means that this is the intrinsic superclass where we
                 *   found the modifier method 
                 */
                found_def_obj = TRUE;
            }
            else
            {
                /*
                 *   The current defining object is not part of the modifier
                 *   chain for this intrinsic class, so we've already skipped
                 *   past this point in the intrinsic superclass tree on past
                 *   inheritances.  Simply move to the next intrinsic class
                 *   and look at its modifier.  
                 */
                goto next_intrinsic_sc;
            }
        }

        /* 
         *   If there's a modifier object, send the property request to it.
         *   We are effectively delegating the method call to the modifier
         *   object, so we must use the "inherited property" call, not the
         *   plain get_prop() call: 'self' is the original self, but the
         *   target object is the intrinsic class modifier object.  
         */
        if (mod_obj != VM_INVALID_OBJ
            && vm_objp(vmg_ mod_obj)->inh_prop(
                vmg_ prop, retval, self, mod_obj, defining_obj,
                source_obj, argc))
            return TRUE;

        /* we didn't find it in this object, so look at its super-metaclass */
    next_intrinsic_sc:
        if (entry->meta_->get_supermeta_reg() != 0)
        {
            /* get the super-metaclass ID */
            entry = (G_meta_table
                     ->get_entry_from_reg(entry->meta_
                                          ->get_supermeta_reg()
                                          ->get_reg_idx()));

            /* 
             *   if we've already found the previous defining intrinsic
             *   class, we can forget about the previous defining modifier
             *   object now: since we're moving to a new intrinsic
             *   superclass, we will have no superclass relation to the
             *   previous defining object in the new modifier family, so we
             *   can simply use the next definition of the property we find 
             */
            if (found_def_obj)
                defining_obj = VM_INVALID_OBJ;
        }
        else
        {
            /* no super-metaclass - give up */
            break;
        }
    }

    /* we don't have a modifier object, so the property is undefined */
    return FALSE;
}

/* 
 *   property evaluator - ofKind
 */
int CVmObject::getp_of_kind(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc,
                            vm_prop_id_t, vm_obj_id_t *)
{
    vm_val_t sc;
    static CVmNativeCodeDesc desc(1);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the superclass, and make sure it's an object */
    G_stk->pop(&sc);
    if (sc.typ != VM_OBJ)
        err_throw(VMERR_OBJ_VAL_REQD);

    /* check for an identity test */
    if (sc.val.obj == self)
    {
        /* x.ofKind(x) == true */
        retval->set_true();
    }
    else
    {
        /* check to see if the object is a superclass of ours */
        retval->set_logical(is_instance_of(vmg_ sc.val.obj));
    }
    
    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - isClass
 */
int CVmObject::getp_is_class(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *argc,
                             vm_prop_id_t, vm_obj_id_t *)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* indicate whether or not we're a class object */
    retval->set_logical(is_class_object(vmg_ self));

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - isTransient
 */
int CVmObject::getp_is_transient(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc,
                                 vm_prop_id_t, vm_obj_id_t *)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* indicate whether or not we're transient */
    retval->set_logical(G_obj_table->is_obj_transient(self));

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - getSuperclassList 
 */
int CVmObject::getp_sclist(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc,
                           vm_prop_id_t, vm_obj_id_t *)
{
    size_t sc_cnt;
    vm_obj_id_t lst_obj;
    CVmObjList *lstp;
    size_t i;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push a self-reference for GC protection */
    G_interpreter->push_obj(vmg_ self);

    /* get the number of superclasses */
    sc_cnt = get_superclass_count(vmg_ self);

    /* allocate a list for the results */
    lst_obj = CVmObjList::create(vmg_ FALSE, sc_cnt);
    lstp = (CVmObjList *)vm_objp(vmg_ lst_obj);

    /* build the superclass list */
    for (i = 0 ; i < sc_cnt ; ++i)
    {
        vm_val_t ele_val;

        /* get this superclass */
        ele_val.set_obj(get_superclass(vmg_ self, i));

        /* set the list element */
        lstp->cons_set_element(i, &ele_val);
    }

    /* discard our GC protection */
    G_stk->discard();

    /* return the list */
    retval->set_obj(lst_obj);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - propDefined
 */
int CVmObject::getp_propdef(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *in_argc,
                            vm_prop_id_t, vm_obj_id_t *)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    vm_val_t val;
    int flags;
    int found;
    vm_prop_id_t prop;
    vm_obj_id_t source_obj;
    static CVmNativeCodeDesc desc(1, 1);
    
    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* pop the property address to test */
    G_interpreter->pop_prop(vmg_ &val);
    prop = val.val.prop;

    /* if we have the flag argument, get it; otherwise, use the default */
    if (argc >= 2)
    {
        /* get the flag value */
        G_interpreter->pop_int(vmg_ &val);
        flags = (int)val.val.intval;
    }
    else
    {
        /* use the default flags */
        flags = VMOBJ_PROPDEF_ANY;
    }

    /* presume we won't find a valid source object */
    source_obj = VM_INVALID_OBJ;

    /* look up the property */
    found = get_prop(vmg_ prop, &val, self, &source_obj, 0);

    /* 
     *   If we found a result, check to see if it's an intrinsic class
     *   modifier object.  If it is, replace it with its intrinsic class:
     *   modifier objects are invisible through the reflection mechanism, and
     *   appear to be the actual intrinsic classes they modify.  
     */
    if (found && CVmObjIntClsMod::is_intcls_mod_obj(vmg_ source_obj))
        source_obj = find_intcls_for_mod(vmg_ self, source_obj);

    /* check the flags */
    switch(flags)
    {
    case VMOBJ_PROPDEF_ANY:
        /* return true if the property is defined */
        retval->set_logical(found);
        break;

    case VMOBJ_PROPDEF_DIRECTLY:
        /* return true if the property is defined directly */
        retval->set_logical(found && source_obj == self);
        break;
        
    case VMOBJ_PROPDEF_INHERITS:
        /* return true if the property is inherited only */
        retval->set_logical(found && source_obj != self);
        break;

    case VMOBJ_PROPDEF_GET_CLASS:
        /* return the defining class, or nil if it's not defined */
        if (found)
        {
            /* 
             *   If we got a valid source object, return it.  If we didn't
             *   get a valid source object, but we found the property,
             *   return 'self' as the result; this isn't exactly right, but
             *   this should only be possible when the source object is an
             *   intrinsic class for which no intrinsic class object is
             *   defined, in which case the best we can do is provide 'self'
             *   as the answer.  
             */
            retval->set_obj(source_obj != VM_INVALID_OBJ ? source_obj : self);
        }
        else
        {
            /* didn't find it - the return value is nil */
            retval->set_nil();
        }
        break;

    default:
        /* other flags are invalid */
        err_throw(VMERR_BAD_VAL_BIF);
        break;
    }

    /* handled */
    return TRUE;
}

/*
 *   Find the intrinsic class which the given modifier object modifies.  This
 *   can only be used with a modifier that modifies my intrinsic class or one
 *   of its intrinsic superclasses.  
 */
vm_obj_id_t CVmObject::find_intcls_for_mod(VMG_ vm_obj_id_t self,
                                           vm_obj_id_t mod_obj)
{
    /* get my metaclass from the dependency table */
    vm_meta_entry_t *entry = (G_meta_table
             ->get_entry_from_reg(get_metaclass_reg()->get_reg_idx()));

    /* 
     *   if there's an intrinsic class object for the metaclass, ask it to do
     *   the work 
     */
    if (entry != 0 && entry->class_obj_ != VM_INVALID_OBJ)
        return (((CVmObjClass *)vm_objp(vmg_ entry->class_obj_))
                ->find_mod_src_obj(vmg_ entry->class_obj_, mod_obj));

    /* there's no intrinsic metaclass, so we can't find what we need */
    return VM_INVALID_OBJ;
}

/* 
 *   property evaluator - propInherited
 */
int CVmObject::getp_propinh(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *in_argc,
                            vm_prop_id_t, vm_obj_id_t *)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    vm_val_t val;
    int flags;
    int found;
    vm_prop_id_t prop;
    vm_obj_id_t source_obj;
    vm_obj_id_t orig_target_obj;
    vm_obj_id_t defining_obj;
    static CVmNativeCodeDesc desc(3, 1);

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* pop the property address to test */
    G_interpreter->pop_prop(vmg_ &val);
    prop = val.val.prop;

    /* get the original target object */
    G_interpreter->pop_obj(vmg_ &val);
    orig_target_obj = val.val.obj;

    /* get the defining object */
    G_interpreter->pop_obj(vmg_ &val);
    defining_obj = val.val.obj;

    /* if we have the flag argument, get it; otherwise, use the default */
    if (argc >= 4)
    {
        /* get the flag value */
        G_interpreter->pop_int(vmg_ &val);
        flags = (int)val.val.intval;
    }
    else
    {
        /* use the default flags */
        flags = VMOBJ_PROPDEF_ANY;
    }

    /* presume we won't find a valid source object */
    source_obj = VM_INVALID_OBJ;

    /* look up the property */
    found = inh_prop(vmg_ prop, &val, self, orig_target_obj, defining_obj,
                     &source_obj, 0);

    /* check the flags */
    switch(flags)
    {
    case VMOBJ_PROPDEF_ANY:
        /* return true if the property is defined */
        retval->set_logical(found);
        break;

    case VMOBJ_PROPDEF_GET_CLASS:
        /* return the defining class, or nil if it's not defined */
        if (found)
        {
            /* return the source object, or 'self' if we didn't find one */
            retval->set_obj(source_obj != VM_INVALID_OBJ ? source_obj : self);
        }
        else
        {
            /* didn't find it - the return value is nil */
            retval->set_nil();
        }
        break;

    default:
        /* other flags are invalid */
        err_throw(VMERR_BAD_VAL_BIF);
        break;
    }

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - propType
 */
int CVmObject::getp_proptype(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *argc,
                             vm_prop_id_t, vm_obj_id_t *)
{
    vm_val_t val;
    vm_prop_id_t prop;
    vm_obj_id_t source_obj;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the property address to test */
    G_interpreter->pop_prop(vmg_ &val);
    prop = val.val.prop;

    /* get the property value */
    if (!get_prop(vmg_ prop, &val, self, &source_obj, 0))
    {
        /* the property isn't defined on the object - the result is nil */
        retval->set_nil();
    }
    else
    {
        /* 
         *   Fix up OBJX (execute-on-eval object reference) and BIFPTRX
         *   (execute-on-eval built-in function pointer) values to reflect
         *   the underlying datatype.  Treat an anonymous function or dynamic
         *   function as a CODEOFS value; treat a string object as a DSTRING
         *   value.  
         */
        if (val.typ == VM_OBJX)
        {
            if (CVmObjAnonFn::is_anonfn_obj(vmg_ val.val.obj)
                || CVmDynamicFunc::is_dynfunc_obj(vmg_ val.val.obj))
                val.typ = VM_CODEOFS;
            else if (CVmObjString::is_string_obj(vmg_ val.val.obj))
                val.typ = VM_DSTRING;
            else
                val.typ = VM_OBJ;
        }
        else if (val.typ == VM_BIFPTRX)
            val.typ = VM_BIFPTR;

        /* set the return value to the property's datatype value */
        retval->set_datatype(vmg_ &val);
    }

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - getPropList 
 */
int CVmObject::getp_get_prop_list(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *argc,
                                  vm_prop_id_t, vm_obj_id_t *)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* build my property list */
    build_prop_list(vmg_ self, retval);

    /* discard the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Build a list of properties directly defined by this instance 
 */
void CVmObject::build_prop_list(VMG_ vm_obj_id_t /*self*/, vm_val_t *retval)
{
    /* 
     *   by default, object instances have no directly defined properties,
     *   so create and return an empty list 
     */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, (size_t)0));
}

/* 
 *   property evaluator - getPropParams
 */
int CVmObject::getp_get_prop_params(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc,
                                    vm_prop_id_t, vm_obj_id_t *)
{
    vm_val_t val;
    vm_prop_id_t prop;
    int min_args, opt_args, varargs;
    static CVmNativeCodeDesc desc(1);
    CVmObjList *lst;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the property address to test */
    G_interpreter->pop_prop(vmg_ &val);
    prop = val.val.prop;

    /* push a self-reference while we're working */
    G_stk->push()->set_obj(self);

    /* get the property information */
    get_prop_interface(vmg_ self, prop, min_args, opt_args, varargs);

    /* 
     *   Allocate our return list.  We need three elements: [minArgs,
     *   optionalArgs, isVarargs]. 
     */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, 3));

    /* get the list object, properly cast */
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* set the minimum argument count */
    val.set_int(min_args);
    lst->cons_set_element(0, &val);

    /* set the optional argument count */
    val.set_int(opt_args);
    lst->cons_set_element(1, &val);

    /* set the varargs flag */
    val.set_logical(varargs);
    lst->cons_set_element(2, &val);

    /* discard our self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Get the method interface to a given object property 
 */
int CVmObject::get_prop_interface(VMG_ vm_obj_id_t self, vm_prop_id_t prop,
                                  int &min_args, int &opt_args, int &varargs)
{
    vm_val_t val;
    vm_obj_id_t source_obj;

    /* presume we won't find an argument interface */
    min_args = opt_args = 0;
    varargs = FALSE;

    /* get the property value */
    if (!get_prop(vmg_ prop, &val, self, &source_obj, 0))
    {
        /* we didn't find the property at all */
        return FALSE;
    }
    else if (val.typ == VM_CODEOFS
             || val.typ == VM_OBJX
             || val.typ == VM_BIFPTRX)
    {
        /* 
         *   It's a direct pointer to static code, an object that we execute
         *   on evaluation, or a built-in function pointer.  Try getting a
         *   pointer to the method header.  CODEOFS and BIFPTRX values always
         *   have method headers; OBJX values do if the underlying object is
         *   invokable.  If the object *isn't* invokable, it won't have a
         *   method header; the only way this can happen is that we have a
         *   String object as an OBJX property, which acts like a DSTRING
         *   when evaluated.  That has the default zero-argument interface,
         *   so we can simply leave the return value with the defaults we've
         *   already set up in this case.  
         */
        CVmFuncPtr func;
        if (func.set(vmg_ &val))
        {
            /* it's an invokable - get the interface info from the header */
            min_args = func.get_min_argc();
            opt_args = func.get_opt_argc();
            varargs = func.is_varargs();
        }

        /* got it */
        return TRUE;
    }
    else if (val.typ == VM_NATIVE_CODE)
    {
        /* get the arguments from the native code descriptor */
        min_args = val.val.native_desc->min_argc_;
        opt_args = val.val.native_desc->opt_argc_;
        varargs = val.val.native_desc->varargs_;

        /* we found the property */
        return TRUE;
    }
    else
    {
        /* 
         *   it's not a function, so there are no arguments, but we did find
         *   the property 
         */
        return TRUE;
    }
}

/*
 *   Call a static property 
 */
int CVmObject::call_stat_prop(VMG_ vm_val_t *retval, const uchar **pc_ptr,
                              uint *argc, vm_prop_id_t prop)
{
    /* not handled */
    return FALSE;
}

/*
 *   Get my image file version string 
 */
const char *CVmObject::get_image_file_version(VMG0_) const
{
    /* get the metaclass table entry */
    vm_meta_entry_t *entry = G_meta_table->get_entry_from_reg(
        get_metaclass_reg()->get_reg_idx());

    /* if we found it, get the image metaclass string from the entry */
    if (entry != 0)
    {
        /* get the image metaclass name - "name/version" */
        const char *n = entry->image_meta_name_;

        /* scan for the version slash */
        n = strchr(n, '/');

        /* if we found it, the version follows the slash */
        if (n != 0)
            return n + 1;
    }

    /* we couldn't find the version data, so return an empty string */
    return "";
}


/* ------------------------------------------------------------------------ */
/*
 *   CVmMetaclass implementation 
 */

/*
 *   Get a metaclass's super-metaclass.  We'll look up our super-metaclass
 *   in the metaclass registration table and return the IntrinsicClass
 *   object we find referenced there.  
 */
vm_obj_id_t CVmMetaclass::get_supermeta(VMG_ int idx) const
{
    vm_meta_entry_t *entry;

    /* we only have one supermetaclass */
    if (idx != 0)
        return VM_INVALID_OBJ;

    /* if I don't have a supermetaclass at all, return nil */
    if (get_supermeta_reg() == 0)
        return VM_INVALID_OBJ;

    /* look up my supermetaclass entry */
    entry = (G_meta_table->get_entry_from_reg(
        get_supermeta_reg()->get_reg_idx()));

    /* return the IntrinsicClass object that represents this metaclass */
    return (entry != 0 ? entry->class_obj_ : VM_INVALID_OBJ);
}

/*
 *   Determine if I'm an instance of the given object.  Most metaclasses
 *   inherit directly from CVmObject, so we'll return true only if the
 *   object is the CVmObject IntrinsicClass object 
 */
int CVmMetaclass::is_meta_instance_of(VMG_ vm_obj_id_t obj) const
{
    /* iterate over my supermetaclasses */
    for (CVmMetaclass *sc = get_supermeta_reg() ; sc != 0 ;
         sc = sc->get_supermeta_reg())
    {
        /* look up the metaclass entry for this supermetaclass */
        vm_meta_entry_t *entry =
            (G_meta_table->get_entry_from_reg(sc->get_reg_idx()));

        /* 
         *   if the object matches the current superclass's IntrinsicClass
         *   object, we're a subclass of that object; otherwise we're not 
         */
        if (entry != 0 && entry->class_obj_ == obj)
            return TRUE;
    }

    /* it's not one of my superclasses */
    return FALSE;
}

/*
 *   Get my intrinsic class object 
 */
vm_obj_id_t CVmMetaclass::get_class_obj(VMG0_) const
{
    /* get my metacalss entry */
    vm_meta_entry_t *entry = G_meta_table->get_entry_from_reg(get_reg_idx());

    /* if we found our entry, return the class from the entry */
    return (entry != 0 ? entry->class_obj_ : VM_INVALID_OBJ);
}

/* ------------------------------------------------------------------------ */
/*
 *   The nil object.  This is a special pseudo-object we create for object
 *   #0, to provide graceful error handling if byte code tries to dereference
 *   nil in any way.  
 */
class CVmObjNil: public CVmObject
{
public:
    virtual class CVmMetaclass *get_metaclass_reg() const
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return 0;)
    }

    virtual void set_prop(VMG_ class CVmUndo *, vm_obj_id_t, vm_prop_id_t,
                          const vm_val_t *)
        { err_throw(VMERR_NIL_DEREF); }
    virtual int get_prop(VMG_ vm_prop_id_t, vm_val_t *,
                         vm_obj_id_t, vm_obj_id_t *, uint *)
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return FALSE;)
    }
    virtual int is_instance_of(VMG_ vm_obj_id_t)
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return FALSE;)
    }
    virtual int get_superclass_count(VMG_ vm_obj_id_t) const
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return 0;)
    }
    virtual vm_obj_id_t get_superclass(VMG_ vm_obj_id_t, int) const
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }
    virtual void enum_props(VMG_ vm_obj_id_t,
                            void (*cb)(VMG_ void *, vm_obj_id_t,
                                       vm_prop_id_t, const vm_val_t *),
                            void *)
        { err_throw(VMERR_NIL_DEREF); }
    virtual int get_prop_interface(VMG_ vm_obj_id_t, vm_prop_id_t,
                                   int &, int &, int &)
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return FALSE;)
    }
    virtual int inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                         vm_obj_id_t self, vm_obj_id_t orig_target_obj,
                         vm_obj_id_t defining_obj, vm_obj_id_t *source_obj,
                         uint *argc)
    {
        err_throw(VMERR_NIL_DEREF);
        AFTER_ERR_THROW(return FALSE;)
    }
    virtual void build_prop_list(VMG_ vm_obj_id_t, vm_val_t *)
        { err_throw(VMERR_NIL_DEREF); }

    virtual void notify_delete(VMG_ int) { }
    virtual void mark_refs(VMG_ uint) { }
    virtual void remove_stale_weak_refs(VMG0_) { }
    virtual void notify_new_savept() { }
    virtual void apply_undo(VMG_ struct CVmUndoRecord *) { }
    virtual void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    virtual void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }
    virtual void load_from_image(VMG_ vm_obj_id_t, const char *, size_t) { }
    virtual void save_to_file(VMG_ class CVmFile *) { }
    virtual void restore_from_file(VMG_ vm_obj_id_t, class CVmFile *,
                                   class CVmObjFixup *) { }
    
};


/* ------------------------------------------------------------------------ */
/*
 *   object table implementation 
 */

/*
 *   construction - create an empty table 
 */
CVmObjTable::CVmObjTable()
{
    pages_ = 0;
    page_slots_ = 0;
    pages_used_ = 0;
    image_ptr_head_ = 0;
    globals_ = 0;
    global_var_head_ = 0;
    post_load_init_table_ = 0;
}

/*
 *   allocate object table 
 */
void CVmObjTable::init(VMG0_)
{
    /* allocate the initial set of page slots */
    page_slots_ = 10;
    pages_ = (CVmObjPageEntry **)t3malloc(page_slots_ * sizeof(*pages_));

    /* if that failed, throw an error */
    if (pages_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* no pages are in use yet */
    pages_used_ = 0;
    
    /* there are no free objects yet */
    first_free_ = 0;

    /* there's nothing in the GC work queue yet */
    gc_queue_head_ = VM_INVALID_OBJ;

    /* nothing in the finalizer work queue yet */
    finalize_queue_head_ = VM_INVALID_OBJ;

    /* we haven't allocated anything yet */
    allocs_since_gc_ = 0;
    bytes_since_gc_ = 0;

    /* 
     *   Set the upper limit for new objects and allocated bytes between
     *   garbage collection passes.
     *   
     *   In choosing these parameters, there's a trade-off.  Running more
     *   frequently reduces GC "stall time" of each pass (the time that the
     *   system is unresponsive while scanning for unreachable objects),
     *   because the total scanning on each pass will be smaller.  More
     *   frequent GC'ing also minimizes the working memory size by freeing
     *   garbage objects sooner after they become unreachable.  But running
     *   more frequently reduces *overall* program speed, since we spend more
     *   time in the collector in aggregate.  We have to scan the reachable
     *   objects on every pass, so the more frequently we run, the more
     *   unproductive time we'll spend scanning the same reachable objects
     *   over and over.
     *   
     *   TADS is specialized for highly interactive programs, so minimizing
     *   stall time is the overriding priority.  In practical testing, the
     *   stall time is imperceptible up to at least 30,000 objects between
     *   passes.  (It stays below 5 ms per run on an average 2007 desktop.
     *   The usual threshold of perceptibility is about 50-100 ms.)  Overall
     *   throughput seems to level out at about 20,000 objects between
     *   passes.  Beyond that, decreasing the frequency of gc runs yields
     *   diminishing returns.  The knee is the point where we're waiting long
     *   enough between passes that unreachable objects dominate.  Once
     *   you're at that frequency, going longer between passes won't improve
     *   speed much because you're typically just adding more garbage for the
     *   next pass.  The time you save by not doing a pass now gets canceled
     *   out by making the next pass longer by giving it more garbage to
     *   collect.  This is in contrast to running too frequently, where
     *   scanning reachable objects dominates: this time typically plateaus
     *   the longer you go between passes, since the reachable set itself
     *   tends to plateau in size over time, so while we're still in this
     *   zone, the longer you go between passes the better.)
     *   
     *   Although 20,000 allocations seems to be the best frequency for
     *   throughput for our test set, we currently set a somewhat more
     *   conservative limit (i.e., collect garbage more frequently).  This is
     *   because the assumptions in our test set don't apply to all programs.
     *   Some programs might allocate larger objects more frequently than our
     *   tests, and in those cases we don't want the working set to grow too
     *   quickly.  More frequenty GC will help avoid this.  
     *   
     *   In addition to the simple object allocation count, we also count
     *   bytes allocated between passes, and collect garbage if we go above a
     *   byte threshold independently of the object count.  This is to ensure
     *   that we collect garbage more frequently when very large objects are
     *   being used, to keep the working set from growing too fast.  Very
     *   large working sets can hurt performance by reducing locality of
     *   reference (for CPU caching) and even triggering OS-level disk
     *   swapping.
     */
//    max_allocs_between_gc_ = 17500;
    max_allocs_between_gc_ = 10000;
    max_bytes_between_gc_ = 6*1024*1024;

    /* enable the garbage collector */
    gc_enabled_ = TRUE;

    /* there are no saved image data pointers yet */
    image_ptr_head_ = 0;
    image_ptr_tail_ = 0;
    image_ptr_last_cnt_ = 0;

    /* no global pages yet */
    globals_ = 0;

    /* no global variables yet */
    global_var_head_ = 0;

    /* create the post_load_init() request table */
    post_load_init_table_ = new CVmHashTable(128, new CVmHashFuncCS(), TRUE);

    /* allocate the first object page */
    alloc_new_page();

    /* 
     *   Initialize object #0 with the nil pseudo-object.  This isn't an
     *   actual object, but it provides graceful error handling if the byte
     *   code attempts to invoke any methods on nil. 
     */
    new (vmg_ VM_INVALID_OBJ) CVmObjNil;
    CVmObjPageEntry *obj0 = get_entry(0);
    obj0->free_ = TRUE;
    obj0->in_root_set_ = TRUE;
    obj0->reachable_ = VMOBJ_REACHABLE;
    obj0->finalize_state_ = VMOBJ_UNFINALIZABLE;
    obj0->in_undo_ = FALSE;
    obj0->transient_ = FALSE;
    obj0->requested_post_load_init_ = FALSE;
    obj0->can_have_refs_ = FALSE;
    obj0->can_have_weak_refs_ = FALSE;
}

/*
 *   delete object table 
 */
CVmObjTable::~CVmObjTable()
{
    /* show statistics if applicable */
    IF_GC_STATS(gc_stats.display());
}

/*
 *   Clear the object table.  This deletes all garbage-collected objects in
 *   preparation for VM shutdown.
 */
void CVmObjTable::clear_obj_table(VMG0_)
{
    /* delete all entries in the post-load initialization table */
    post_load_init_table_->delete_all_entries();

    /* go through the pages and delete the entries */
    for (size_t i = 0 ; i < pages_used_ ; ++i)
    {
        /* delete all of the objects on the page */
        int j;
        CVmObjPageEntry *entry;
        for (j = 0, entry = pages_[i] ; j < VM_OBJ_PAGE_CNT ; ++j, ++entry)
        {
            /* if this entry is still in use, delete it */
            if (!entry->free_)
                entry->get_vm_obj()->notify_delete(vmg_ entry->in_root_set_);
        }
    }

}

/*
 *   Delete the table.  (We need to separate this out into a method so
 *   that we can get access to the global variables.)  
 */
void CVmObjTable::delete_obj_table(VMG0_)
{
    /* delete each page we've allocated */
    for (size_t i = 0 ; i < pages_used_ ; ++i)
        t3free(pages_[i]);

    /* free the master page table */
    t3free(pages_);

    /* we no longer have any pages */
    pages_ = 0;
    page_slots_ = 0;
    pages_used_ = 0;

    /* empty out the object lists */
    first_free_ = VM_INVALID_OBJ;
    gc_queue_head_ = VM_INVALID_OBJ;
    finalize_queue_head_ = VM_INVALID_OBJ;

    /* clear the gc statistics */
    allocs_since_gc_ = 0;
    bytes_since_gc_ = 0;

    /* delete each object image data pointer page */
    for (vm_image_ptr_page *ip_page = image_ptr_head_, *ip_next = 0 ;
         ip_page != 0 ; ip_page = ip_next)
    {
        /* remember the next page (before we delete this one) */
        ip_next = ip_page->next_;

        /* delete this page */
        t3free(ip_page);
    }

    /* there's nothing left in the image data list */
    image_ptr_head_ = image_ptr_tail_ = 0;

    /* delete the linked list of globals */
    if (globals_ != 0)
    {
        delete globals_;
        globals_ = 0;
    }

    /* 
     *   delete any left-over global variables (these should always be
     *   deleted by subsystems before we get here, but this is the last
     *   chance, so clean them up manually) 
     */
    while (global_var_head_ != 0)
        delete_global_var(global_var_head_);

    /* delete the post-load initialization table */
    delete post_load_init_table_;
    post_load_init_table_ = 0;
}

/*
 *   Enable or disable garbage collection 
 */
int CVmObjTable::enable_gc(VMG_ int enable)
{
    /* remember the old status for returning */
    int old_enable = gc_enabled_;

    /* set the new status */
    gc_enabled_ = enable;

    /* 
     *   if we're enabling GC after it was previously disabled, check to
     *   see if we should perform a GC pass now (but don't count this as a
     *   separate allocation) 
     */
    if (!old_enable && enable)
        alloc_check_gc(vmg_ FALSE);

    /* return the previous status */
    return old_enable;
}

/*
 *   allocate a new object ID 
 */
vm_obj_id_t CVmObjTable::alloc_obj(VMG_ int in_root_set,
                                   int can_have_refs, int can_have_weak_refs)
{
    /* count the allocation and maybe perform garbage collection */
    alloc_check_gc(vmg_ TRUE);

    /* if the free list is empty, allocate a new page of object entries */
    if (first_free_ == VM_INVALID_OBJ)
        alloc_new_page();
        
    /* remember the first item in the free list - this is our result */
    vm_obj_id_t ret = first_free_;
    
    /* get the object table entry for the ID */
    CVmObjPageEntry *entry = get_entry(ret);

    /* unlink new entry from the free list */
    first_free_ = entry->next_obj_;
    if (entry->next_obj_ != VM_INVALID_OBJ)
        get_entry(entry->next_obj_)->ptr_.prev_free_ = entry->ptr_.prev_free_;

    /* initialize the entry */
    init_entry_for_alloc(ret, entry, in_root_set,
                         can_have_refs, can_have_weak_refs);

    /* return the free object */
    return ret;
}

/*
 *   Run garbage collection before allocating an object 
 */
void CVmObjTable::gc_before_alloc(VMG0_)
{
    /* count it if in statistics mode */
    IF_GC_STATS(gc_stats.begin_pass());

    /* run a full garbage collection pass */
    gc_pass_init(vmg0_);
    gc_pass_finish(vmg0_);

    /* count it if in statistics mode */
    IF_GC_STATS(gc_stats.end_pass());
}

/*
 *   Allocate an object with a particular ID 
 */
void CVmObjTable::alloc_obj_with_id(vm_obj_id_t id, int in_root_set,
                                    int can_have_refs, int can_have_weak_refs)
{
    CVmObjPageEntry *entry;
    
    /* 
     *   if the page containing the given object ID hasn't been allocated,
     *   allocate pages until it's available 
     */
    while (id >= pages_used_ * VM_OBJ_PAGE_CNT)
        alloc_new_page();

    /* get the object table entry for the ID */
    entry = get_entry(id);

    /* if the desired object ID is already taken, it's an error */
    if (!entry->free_)
        err_throw(VMERR_OBJ_IN_USE);

    /* unlink the item - set the previous item's forward pointer... */
    if (entry->ptr_.prev_free_ == VM_INVALID_OBJ)
        first_free_ = entry->next_obj_;
    else
        get_entry(entry->ptr_.prev_free_)->next_obj_ = entry->next_obj_;

    /* ...and the next items back pointer */
    if (entry->next_obj_ != VM_INVALID_OBJ)
        get_entry(entry->next_obj_)->ptr_.prev_free_ = entry->ptr_.prev_free_;

    /* initialize the entry for allocation */
    init_entry_for_alloc(id, entry, in_root_set,
                         can_have_refs, can_have_weak_refs);
}

/*
 *   Initialize an object table entry that we've just allocated 
 */
void CVmObjTable::init_entry_for_alloc(vm_obj_id_t id,
                                       CVmObjPageEntry *entry,
                                       int in_root_set,
                                       int can_have_refs,
                                       int can_have_weak_refs)
{
    /* mark the entry as in use */
    entry->free_ = FALSE;

    /* no undo savepoint has been created since the object was created */
    entry->in_undo_ = FALSE;

    /* mark the object as being in the root set if appropriate */
    entry->in_root_set_ = in_root_set;

    /* presume it's an ordinary persistent object */
    entry->transient_ = FALSE;

    /* presume it won't need post-load initialization */
    entry->requested_post_load_init_ = FALSE;

    /* set the GC characteristics as requested */
    entry->can_have_refs_ = can_have_refs;
    entry->can_have_weak_refs_ = can_have_weak_refs;

    /* 
     *   Mark the object as initially unreachable and unfinalizable.  It's
     *   not necessarily really unreachable at this point, but we mark it
     *   as such because the garbage collector hasn't explicitly traced
     *   the object to be reachable.  The initial conditions for garbage
     *   collection are that all objects not in the root set and not
     *   finalizable are marked as unreachable; since we're not in a gc
     *   pass right now (we can't be - memory cannot be allocated during a
     *   gc pass), we know that we must establish initial gc conditions
     *   for the next time we start a gc pass.  
     */
    entry->reachable_ = VMOBJ_UNREACHABLE;
    entry->finalize_state_ = VMOBJ_UNFINALIZABLE;

    /* add it to the GC work queue for the next GC pass */
    if (in_root_set)
        add_to_gc_queue(id, entry, VMOBJ_REACHABLE);
}

#if 0 // moved to in-line in header, since it's called *a lot*
/*
 *   get the page entry for a given ID 
 */
CVmObjPageEntry *CVmObjTable::get_entry(vm_obj_id_t id) const
{
    size_t main_idx;
    size_t page_idx;

    /* get the index of the page in the main array */
    main_idx = (size_t)(id >> VM_OBJ_PAGE_CNT_LOG2);

    /* get the index within the page */
    page_idx = (size_t)(id & (VM_OBJ_PAGE_CNT - 1));

    /* get the object */
    return &pages_[main_idx][page_idx];
}
#endif

/*
 *   Delete an object, given the object table entry.
 */
void CVmObjTable::delete_entry(VMG_ vm_obj_id_t id, CVmObjPageEntry *entry)
{
    /* mark the object table entry as free */
    entry->free_ = TRUE;

    /* it's not in the root set if it's free */
    entry->in_root_set_ = FALSE;

    /* 
     *   notify the object that it's being deleted - this will let the
     *   object release any additional resources (such as variable-size
     *   heap space) that it's holding 
     */
    entry->get_vm_obj()->notify_delete(vmg_ FALSE);

    /* 
     *   remove any post-load initialization request for the object, if it
     *   ever requested post-load initialization 
     */
    if (entry->requested_post_load_init_)
        remove_post_load_init(id);

    /* link this object into the head of the free list */
    entry->next_obj_ = first_free_;

    /* link the previous head back to this object */
    if (first_free_ != VM_INVALID_OBJ)
        get_entry(first_free_)->ptr_.prev_free_ = id;

    /* this object doesn't have a previous entry */
    entry->ptr_.prev_free_ = VM_INVALID_OBJ;
    
    /* it's now the first entry in the list */
    first_free_ = id;
}


/*
 *   allocate a new page of objects 
 */
void CVmObjTable::alloc_new_page()
{
    size_t i;
    vm_obj_id_t id;
    CVmObjPageEntry *entry;

    /* first, make sure we have room in the master page list */
    if (pages_used_ == page_slots_)
    {
        /* increase the number of page slots */
        page_slots_ += 10;
        
        /* allocate space for the increased number of slots */
        pages_ = (CVmObjPageEntry **)t3realloc(
            pages_, page_slots_ * sizeof(*pages_));
    }
    
    /* allocate a new page */
    pages_[pages_used_] =
        (CVmObjPageEntry *)t3malloc(VM_OBJ_PAGE_CNT * sizeof(*pages_[0]));

    /* if that failed, throw an error */
    if (pages_[pages_used_] == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* 
     *   initialize the new page to be entirely free - add each element of
     *   the page to the free list 
     */
    entry = pages_[pages_used_];
    i = 0;
    id = pages_used_ * VM_OBJ_PAGE_CNT;

    /* 
     *   if this is the start of the very first page, leave off the first
     *   object, since its ID is invalid 
     */
    if (id == VM_INVALID_OBJ)
    {
        /* mark the object as free so we don't try to free it later */
        entry->free_ = TRUE;

        /* ...and it's certainly not a collectable object */
        entry->in_root_set_ = TRUE;

        /* skip it for the impending free list construction */
        ++i;
        ++entry;
        ++id;
    }

    /* loop over each object in this page */
    for ( ; i < VM_OBJ_PAGE_CNT ; ++i, ++entry, ++id)
    {
        /* set the forward pointer in this object */
        entry->next_obj_ = first_free_;

        /* set the back pointer in the previous object in the list */
        if (first_free_ != VM_INVALID_OBJ)
            get_entry(first_free_)->ptr_.prev_free_ = id;

        /* there's nothing before this entry yet */
        entry->ptr_.prev_free_ = VM_INVALID_OBJ;

        /* this is now the head of the list */
        first_free_ = id;

        /* mark it free */
        entry->free_ = TRUE;

        /* presume it's not part of the root set */
        entry->in_root_set_ = FALSE;

        /* 
         *   mark it initially unreachable and finalized, since it's not
         *   even allocated yet 
         */
        entry->reachable_ = VMOBJ_UNREACHABLE;
        entry->finalize_state_ = VMOBJ_FINALIZED;
    }
    
    /* count the new page we've allocated */
    ++pages_used_;
}

/*
 *   Add an object to the list of machine globals.  
 */
void CVmObjTable::add_to_globals(vm_obj_id_t obj)
{
    CVmObjGlobPage *pg;

    /* 
     *   if this is a root set object, there is no need to mark it as
     *   global, since it is inherently uncollectable as an image file
     *   object to begin with 
     */
    if (get_entry(obj)->in_root_set_)
        return;

    /* if we have any global pages allocated, try adding to the head page */
    if (globals_ != 0 && globals_->add_entry(obj))
    {
        /* we successfully added it to the head page - we're done */
        return;
    }

    /* 
     *   either the head page is full, or we haven't allocated any global
     *   pages at all yet; in either case, allocate a new page and link it
     *   at the head of our list 
     */
    pg = new CVmObjGlobPage();
    pg->nxt_ = globals_;
    globals_ = pg;

    /* 
     *   add the object to the new page - it must fit, since the new page is
     *   empty 
     */
    globals_->add_entry(obj);
}

/*
 *   Collect all garbage.  This does a complete garbage collection pass,
 *   returning only after all unreachable objects have been collected.  If
 *   incremental garbage collection is not required, the caller can simply
 *   invoke this routine to do the entire operation in a single call.  
 */
void CVmObjTable::gc_full(VMG0_)
{
    /* count it if in statistics mode */
    IF_GC_STATS(gc_stats.begin_pass());

    /* 
     *   run the initial pass to mark globally-reachable objects, then run
     *   the garbage collector to completion 
     */
    gc_pass_init(vmg0_);
    gc_pass_finish(vmg0_);

    /* count it if in statistics mode */
    IF_GC_STATS(gc_stats.end_pass());
}

/*
 *   Garbage collector - initialize.  Add all globally-reachable objects
 *   to the work queue. 
 *   
 *   We assume that the following initial conditions hold: all objects
 *   except root set objects are marked as unreferenced, and all root set
 *   objects are marked as referenced; all root set objects are in the GC
 *   work queue.  So, we don't need to worry about finding root objects or
 *   initializing the other objects at this point.  
 */
void CVmObjTable::gc_pass_init(VMG0_)
{
    /* 
     *   reset the allocations-since-gc counter, since this is now the
     *   last gc pass, and we obviously haven't performed any allocations
     *   since this gc pass yet 
     */
    allocs_since_gc_ = 0;
    bytes_since_gc_ = 0;

    /* trace objects reachable from the stack */
    gc_trace_stack(vmg0_);

    /* trace objects reachable from imports */
    gc_trace_imports(vmg0_);

    /* trace objects reachable from machine globals */
    gc_trace_globals(vmg0_);

    /*
     *   Process undo records - for each undo record, mark any referenced
     *   objects as reachable.  Undo records are part of the root set.  
     */
    G_undo->gc_mark_refs(vmg0_);
}

/*
 *   Garbage collection - continue processing the work queue.  This
 *   processes a set of entries from the work queue.  This routine can be
 *   used for incremental garbage collection: after calling
 *   gc_pass_init(), the caller can repeatedly invoke this routine until
 *   it returns false.  Since this routine will return after a short time
 *   even if there's more work left to do, other operations (such as
 *   processing user input) can be interleaved in a single thread with
 *   garbage collection.
 *   
 *   The actual number of entries that we process is configurable at VM
 *   compile-time via VM_GC_WORK_INCREMENT.  The point of running the GC
 *   incrementally is to allow GC work to be interleaved with long-running
 *   user I/O operations (such as reading a line of text from the
 *   keyboard) in the foreground thread, so the work increment should be
 *   chosen so that each call to this routine completes quickly enough
 *   that the user will perceive no delay.
 *   
 *   Returns true if more work remains to be done, false if not.  The
 *   caller should invoke this routine repeatedly until it returns false.  
 */
int CVmObjTable::gc_pass_continue(VMG_ int trace_transient)
{
    int cnt;
        
    /* 
     *   keep going until we exhaust the queue or run for our maximum
     *   number of iterations 
     */
    for (cnt = VM_GC_WORK_INCREMENT ;
         cnt != 0 && gc_queue_head_ != VM_INVALID_OBJ ; --cnt)
    {
        vm_obj_id_t cur;
        CVmObjPageEntry *entry;
        
        /* get the next item from the work queue */
        cur = gc_queue_head_;

        /* get this object's entry */
        entry = get_entry(cur);

        /* remove this entry from the work queue */
        gc_queue_head_ = entry->next_obj_;

        /* 
         *   Tell this object to mark its references.  Mark the referenced
         *   objects with the same state as this object, if they're not
         *   already marked with a stronger state.
         *   
         *   If we're not tracing transients, do not trace this object if
         *   it's transient.  
         */
        if (trace_transient || !entry->transient_)
            entry->get_vm_obj()->mark_refs(vmg_ entry->reachable_);
    }

    /* 
     *   return true if there's more work to do; there's more work to do
     *   if we have any objects left in the gc work queue 
     */
    return gc_queue_head_ != VM_INVALID_OBJ;
}

/*
 *   Finish garbage collection.  We'll finish any work remaining in the
 *   work queue, so this is safe to call at any time after gc_pass_init(),
 *   after any number of calls (even zero) to gc_pass_continue().  
 */
void CVmObjTable::gc_pass_finish(VMG0_)
{
    CVmObjPageEntry **pg;
    CVmObjPageEntry *entry;
    size_t i;
    size_t j;
    vm_obj_id_t id;

    /* 
     *   Make sure we're done processing the work queue -- keep calling
     *   gc_pass_continue() until it indicates that it's finished.  If
     *   we're skipping finalizers, stop as soon as the state structure
     *   indicates that we've started running finalizers.  
     */
    gc_trace_work_queue(vmg_ TRUE);

    /*
     *   We've now marked everything that's reachable from the root set as
     *   VMOBJ_REACHABLE.  We can therefore determine the set of objects
     *   that are newly 'finalizable' - an object becomes finalizable when
     *   it was previously 'unfinalizable' and is not reachable, because we
     *   can finalize an object any time after it first becomes unreachable.
     *   So, scan all objects for eligibility for the 'finalizable'
     *   transition, and make the transition in those objects.  
     */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* go through each entry on this page */
        for (j = VM_OBJ_PAGE_CNT, entry = *pg ; j > 0 ; --j, ++entry, ++id)
        {
            /* 
             *   if this entry is not free and is not in the root set, check
             *   to see if its finalization status is changing 
             */
            if (!entry->free_ && !entry->in_root_set_)
            {
                /*
                 *   If the entry is not reachable, and was previously
                 *   unfinalizable, it is now finalizable.
                 *   
                 *   Note that an object must actually be reachable to avoid
                 *   become finalizable at this point.  Not only do
                 *   unreachable objects become finalizable, but
                 *   'f-reachable' objects do, too, since these are only
                 *   reachable from other finalizable objects.  
                 */
                if (entry->reachable_ != VMOBJ_REACHABLE
                    && entry->finalize_state_ == VMOBJ_UNFINALIZABLE)
                {
                    /*
                     *   This object is newly finalizable.  If it has no
                     *   finalizer, the object can go directly to the
                     *   'finalized' state; otherwise, add it to the queue
                     *   of objects with pending finalizers.  
                     */
                    if (entry->get_vm_obj()->has_finalizer(vmg_ id))
                    {
                        /* 
                         *   This object is not reachable from the root set
                         *   and was previously unfinalizable.  Make the
                         *   object finalizable.  
                         */
                        entry->finalize_state_ = VMOBJ_FINALIZABLE;
                    }
                    else
                    {
                        /* 
                         *   the entry has no finalizer, so we can make this
                         *   object 'finalized' immediately 
                         */
                        entry->finalize_state_ = VMOBJ_FINALIZED;
                    }

                    /* count it as a collected object */
                    IF_GC_STATS(gc_stats.count_free());
                }

                /*
                 *   If this object is finalizable, add it to the work queue
                 *   in state "f-reachable."  We must mark everything this
                 *   object references, directly or indirectly, as
                 *   f-reachable, which we'll do with another pass through
                 *   the gc queue momentarily.  
                 */
                if (entry->finalize_state_ == VMOBJ_FINALIZABLE)
                {
                    /* 
                     *   this object and all of the objects it references
                     *   are "f-reachable" 
                     */
                    add_to_gc_queue(id, entry, VMOBJ_F_REACHABLE);
                }
            }
        }
    }

    /*
     *   During the scan above, we put all of the finalizable objects in the
     *   work queue in reachability state 'f-reachable'.  (Actually,
     *   finalizable objects that were fully reachable were not put in the
     *   work queue, because they are in a stronger reachability state that
     *   we've already fully scanned.)  Trace the work queue so that we mark
     *   everything reachable indirectly from an f-reachable object as also
     *   being at least f-reachable.  
     */
    gc_trace_work_queue(vmg_ TRUE);

    /*
     *   We have now marked everything that's fully reachable as being in
     *   state 'reachable', and everything that's reachable from a
     *   finalizable object as being in state 'f-reachable'.  Anything that
     *   is still in state 'unreachable' is garbage and can be collected.  
     */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* go through each entry on this page */
        for (j = VM_OBJ_PAGE_CNT, entry = *pg ; j > 0 ; --j, ++entry, ++id)
        {
            /* if it's not already free, process it */
            if (!entry->free_)
            {
                /* if the object is deletable, delete it */
                if (entry->is_deletable())
                {
                    /*
                     *   This object is completely unreachable, and it has
                     *   already been finalized.  This means there is no
                     *   possibility that the object could ever become
                     *   reachable again, hence we can discard the object.
                     */
                    delete_entry(vmg_ id, entry);
                }
                else
                {
                    /*
                     *   The object is reachable, so keep it around.  Since
                     *   it's staying around, and since we know which
                     *   objects we're deleting and which are staying, we
                     *   can ask this object to remove all of its "stale
                     *   weak references" - that is, weak references to
                     *   objects that we're about to delete.  Don't bother
                     *   notifying the object if it's incapable of keeping
                     *   weak references.  
                     */
                    if (entry->can_have_weak_refs_)
                        entry->get_vm_obj()->remove_stale_weak_refs(vmg0_);

                    /*
                     *   If this object is finalizable, put it in the
                     *   finalizer queue, so that we can run its finalizer
                     *   when we're done scanning the table.  
                     */
                    if (entry->finalize_state_ == VMOBJ_FINALIZABLE)
                        add_to_finalize_queue(id, entry);

                    /* 
                     *   restore initial conditions for this object, so that
                     *   we're properly set up for the next GC pass 
                     */
                    gc_set_init_conditions(id, entry);
                }
            }
        }
    }
    
    /*
     *   Go through the undo records and clear any stale weak references
     *   contained in the undo list.  
     */
    G_undo->gc_remove_stale_weak_refs(vmg0_);

    /*
     *   All of the finalizable objects are now in the finalizer queue.  Run
     *   through the finalizer queue and run each such object's finalizer. 
     */
    run_finalizers(vmg0_);
}

/*
 *   Trace all objects reachable from the work queue. 
 */
void CVmObjTable::gc_trace_work_queue(VMG_ int trace_transient)
{
    /* 
     *   trace everything reachable directly from the work queue, until we
     *   exhaust the queue 
     */
    while (gc_pass_continue(vmg_ trace_transient)) ;
}

/*
 *   Garbage collection: trace objects reachable from the stack 
 */
void CVmObjTable::gc_trace_stack(VMG0_)
{
    size_t i;
    size_t depth;
    vm_val_t *val;

    /*   
     *   Process the stack.  For each stack element that refers to an
     *   object, mark the object as referenced and add it to the work
     *   queue.
     *   
     *   Note that it makes no difference in what order we process the
     *   stack elements; we go from depth down to 0 merely as a trivial
     *   micro-optimization to avoid evaluating the stack depth on every
     *   iteration of the loop.  
     */
    for (i = 0, depth = G_stk->get_depth() ; i < depth ; ++i)
    {
        /* 
         *   If this element refers to an object, and the object hasn't
         *   already been marked as referenced, mark it as reachable and
         *   add it to the work queue.
         *   
         *   Note that we only have to worry about objects here.  We don't
         *   have to worry about constant lists, even though they can
         *   contain object references, because any object reference in a
         *   constant list must be a root set object, and we've already
         *   processed all root set objects.  
         */
        val = G_stk->get(i);
        if (val->typ == VM_OBJ && val->val.obj != VM_INVALID_OBJ)
            add_to_gc_queue(val->val.obj, VMOBJ_REACHABLE);
    }
}

/*
 *   Trace objects reachable from imports 
 */
void CVmObjTable::gc_trace_imports(VMG0_)
{
    /*
     *   generate the list of object imports; for each one, if we have a
     *   valid object for the import, mark it as reachable 
     */
#define VM_IMPORT_OBJ(sym, mem) \
    if (G_predef->mem != VM_INVALID_OBJ) \
        add_to_gc_queue(G_predef->mem, VMOBJ_REACHABLE);
#define VM_NOIMPORT_OBJ(sym, mem) VM_IMPORT_OBJ(sym, mem)
#include "vmimport.h"
}

/*
 *   Garbage collection: trace objects reachable from the machine globals 
 */
void CVmObjTable::gc_trace_globals(VMG0_)
{
    CVmObjGlobPage *pg;
    vm_val_t *val;
    vm_globalvar_t *var;

    /* trace each page of globals */
    for (pg = globals_ ; pg != 0 ; pg = pg->nxt_)
    {
        size_t i;
        vm_obj_id_t *objp;

        /* trace each item on this page */
        for (objp = pg->objs_, i = pg->used_ ; i != 0 ; ++objp, --i)
        {
            /* trace this global */
            add_to_gc_queue(*objp, VMOBJ_REACHABLE);
        }
    }

    /* the return value register (R0) is a machine global */
    val = G_interpreter->get_r0();
    if (val->typ == VM_OBJ && val->val.obj != VM_INVALID_OBJ)
        add_to_gc_queue(val->val.obj, VMOBJ_REACHABLE);

    /* trace the global variables defined by other subsystems */
    for (var = global_var_head_ ; var != 0 ; var = var->nxt)
    {
        /* if this global variable contains an object, trace it */
        if (var->val.typ == VM_OBJ && var->val.val.obj != VM_INVALID_OBJ)
            add_to_gc_queue(var->val.val.obj, VMOBJ_REACHABLE);
    }
}

#if 0 // moved inline, as it's small and is called a fair amount
/*
 *   Set the initial conditions for an object, in preparation for the next
 *   GC pass. 
 */
void CVmObjTable::gc_set_init_conditions(vm_obj_id_t id,
                                         CVmObjPageEntry *entry)
{
    /* 
     *   Mark the object as unreachable -- at the start of each GC pass,
     *   all non-root-set objects must be marked unreachable.
     */
    entry->reachable_ = VMOBJ_UNREACHABLE;

    /* 
     *   If it's in the root set, add it to the GC work queue -- all
     *   root-set objects must be in the work queue and marked as reachable
     *   at the start of each GC pass.
     *   
     *   If the object is not in the root set, check to see if it's
     *   finalizable.  If so, add it to the finalizer queue, so that we
     *   eventually run its finalizer.  
     */
    if (entry->in_root_set_)
        add_to_gc_queue(id, entry, VMOBJ_REACHABLE);
}
#endif

/*
 *   Run finalizers 
 */
void CVmObjTable::run_finalizers(VMG0_)
{
    /* keep going until we run out of work or reach our work limit */
    while (finalize_queue_head_ != VM_INVALID_OBJ)
    {
        CVmObjPageEntry *entry;
        vm_obj_id_t id;

        /* get the next object from the queue */
        id = finalize_queue_head_;

        /* get the entry */
        entry = get_entry(id);

        /* remove the entry form the queue */
        finalize_queue_head_ = entry->next_obj_;

        /* mark the object as finalized */
        entry->finalize_state_ = VMOBJ_FINALIZED;

        /* 
         *   the entry is no longer in any queue, so we must mark it as
         *   unreachable -- this ensures that the initial conditions are
         *   correct for the next garbage collection pass, since all
         *   objects not in the work queue must be marked as unreachable
         *   (it doesn't matter whether the object is actually reachable;
         *   the garbage collector will make that determination when it
         *   next runs) 
         */
        entry->reachable_ = VMOBJ_UNREACHABLE;
        
        /* invoke its finalizer */
        entry->get_vm_obj()->invoke_finalizer(vmg_ id);
    }
}


/*
 *   Receive notification that we're creating a new undo savepoint
 */
void CVmObjTable::notify_new_savept()
{
    CVmObjPageEntry **pg;
    CVmObjPageEntry *entry;
    size_t i;
    size_t j;
    vm_obj_id_t id;

    /* go through each page of objects */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* go through each entry on this page */
        for (j = VM_OBJ_PAGE_CNT, entry = *pg ; j > 0 ; --j, ++entry, ++id)
        {
            /* if this entry is active, tell it about the new savepoint */
            if (!entry->free_)
            {
                /* 
                 *   the object existed at the start of this savepoint, so
                 *   it must keep undo information throughout the savepoint 
                 */
                entry->in_undo_ = TRUE;
                
                /* notify the object of the new savepoint */
                entry->get_vm_obj()->notify_new_savept();
            }
        }
    }
}

/*
 *   Call a callback for each object in the table 
 */
void CVmObjTable::for_each(VMG_ void (*func)(VMG_ vm_obj_id_t, void *),
                           void *ctx)
{
    CVmObjPageEntry **pg;
    size_t i;
    vm_obj_id_t id;

    /* go through each page in the object table */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* start at the start of the page */
        size_t j = VM_OBJ_PAGE_CNT;
        CVmObjPageEntry *entry = *pg;

        /* go through each entry on this page */
        for ( ; j > 0 ; --j, ++entry, ++id)
        {
            /* if this entry is in use, invoke the callback */
            if (!entry->free_)
                (*func)(vmg_ id, ctx);
        }
    }
}

/*
 *   Apply undo 
 */
void CVmObjTable::apply_undo(VMG_ CVmUndoRecord *rec)
{
    /* tell the object to apply the undo */
    if (rec->obj != VM_INVALID_OBJ)
        get_obj(rec->obj)->apply_undo(vmg_ rec);
}


/*
 *   Scan all objects and add metaclass entries to the metaclass
 *   dependency table for any metaclasses of which there are existing
 *   instances.  
 */
void CVmObjTable::add_metadeps_for_instances(VMG0_)
{
    CVmObjPageEntry **pg;
    size_t i;
    vm_obj_id_t id;

    /* go through each page in the object table */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        size_t j;
        CVmObjPageEntry *entry;

        /* start at the start of the page, but skip object ID = 0 */
        j = VM_OBJ_PAGE_CNT;
        entry = *pg;

        /* go through each entry on this page */
        for ( ; j > 0 ; --j, ++entry, ++id)
        {
            /* if this entry is in use, add its metaclass if necessary */
            if (!entry->free_)
                G_meta_table->add_entry_if_new(
                    entry->get_vm_obj()->get_metaclass_reg()->get_reg_idx(),
                    0, VM_INVALID_PROP, VM_INVALID_PROP);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   creation 
 */
CVmObjFixup::CVmObjFixup(ulong entry_cnt)
{
    uint i;

    /* remember the number of entries */
    cnt_ = entry_cnt;
    
    /* no entries are used yet */
    used_ = 0;
    
    /* if we have no entries, there's nothing to do */
    if (cnt_ == 0)
    {
        arr_ = 0;
        pages_ = 0;
        return;
    }

    /* calculate the number of subarrays we need */
    pages_ = (entry_cnt + VMOBJFIXUP_SUB_SIZE - 1) / VMOBJFIXUP_SUB_SIZE;
    
    /* allocate the necessary number of subarrays */
    arr_ = (obj_fixup_entry **)t3malloc(pages_ * sizeof(arr_[0]));
    
    /* allocate the subarrays */
    for (i = 0 ; i < pages_ ; ++i)
    {
        size_t cur_cnt;
        
        /* 
         *   allocate a full page, except for the last page, which might be
         *   only partially used 
         */
        cur_cnt = VMOBJFIXUP_SUB_SIZE;
        if (i + 1 == pages_)
            cur_cnt = ((entry_cnt - 1) % VMOBJFIXUP_SUB_SIZE) + 1;
        
        /* allocate it */
        arr_[i] = (obj_fixup_entry *)t3malloc(cur_cnt * sizeof(arr_[i][i]));
    }
}

/*
 *   deletion 
 */
CVmObjFixup::~CVmObjFixup()
{
    uint i;

    /* if we never allocated an array, there's nothing to do */
    if (arr_ == 0)
        return;
    
    /* delete each subarray */
    for (i = 0 ; i < pages_ ; ++i)
        t3free(arr_[i]);
    
    /* delete the main array */
    t3free(arr_);
}

/*
 *   add a fixup 
 */
void CVmObjFixup::add_fixup(vm_obj_id_t old_id, vm_obj_id_t new_id)
{
    obj_fixup_entry *entry;

    /* allocate the next available entry */
    entry = get_entry(used_++);
    
    /* store it */
    entry->old_id = old_id;
    entry->new_id = new_id;
}

/*
 *   translate an ID 
 */
vm_obj_id_t CVmObjFixup::get_new_id(VMG_ vm_obj_id_t old_id)
{
    /* 
     *   if it's a root-set object, don't bother even trying to translate
     *   it, because root-set objects have stable ID's that never change on
     *   saving or restoring 
     */
    if (G_obj_table->is_obj_id_valid(old_id)
        && G_obj_table->is_obj_in_root_set(old_id))
        return old_id;

    /* find the entry by the object ID */
    obj_fixup_entry *entry = find_entry(old_id);
    
    /* 
     *   if we found it, return the new ID; otherwise, return the old ID
     *   unchanged, since the ID evidently doesn't require mapping 
     */
    return (entry != 0 ? entry->new_id : old_id);
}

/*
 *   find an entry given the old object ID 
 */
obj_fixup_entry *CVmObjFixup::find_entry(vm_obj_id_t old_id)
{
    /* do a binary search for the entry */
    ulong cur;
    ulong lo = 0;
    ulong hi = cnt_ - 1;
    while (lo <= hi)
    {
        /* split the difference */
        cur = lo + (hi - lo)/2;
        obj_fixup_entry *entry = get_entry(cur);
        
        /* is it the one we're looking for? */
        if (entry->old_id == old_id)
        {
            /* it's the one - return the entry */
            return entry;
        }
        else if (old_id > entry->old_id)
        {
            /* we need to go higher */
            lo = (cur == lo ? cur + 1 : cur);
        }
        else
        {
            /* we need to go lower */
            hi = (cur == hi ? cur - 1 : cur);
        }
    }
    
    /* didn't find it */
    return 0;
}

/*
 *   Fix a DATAHOLDER value 
 */
void CVmObjFixup::fix_dh(VMG_ char *dh)
{
    /* if it's an object, translate the ID */
    if (vmb_get_dh_type(dh) == VM_OBJ)
    {
        vm_obj_id_t id;
        
        /* get the object value */
        id = vmb_get_dh_obj(dh);

        /* translate it */
        id = get_new_id(vmg_ id);
        
        /* 
         *   if it's invalid, set the dataholder value to nil; otherwise,
         *   set it to the new object ID 
         */
        if (id == VM_INVALID_OBJ)
            vmb_put_dh_nil(dh);
        else
            vmb_put_dh_obj(dh, id);
    }
}

/*
 *   Fix up an array of DATAHOLDER values. 
 */
void CVmObjFixup::fix_dh_array(VMG_ char *arr, size_t cnt)
{
    /* scan the array of dataholders */
    for ( ; cnt != 0 ; --cnt, arr += VMB_DATAHOLDER)
        fix_dh(vmg_ arr);
}

/*
 *   Fix a portable VMB_OBJECT_ID field 
 */
void CVmObjFixup::fix_vmb_obj(VMG_ char *p)
{
    vm_obj_id_t id;

    /* get the old ID */
    id = vmb_get_objid(p);

    /* fix it up */
    id = get_new_id(vmg_ id);

    /* store it back */
    vmb_put_objid(p, id);
}

/*
 *   Fix an array of portable VMB_OBJECT_ID fields 
 */
void CVmObjFixup::fix_vmb_obj_array(VMG_ char *p, size_t cnt)
{
    /* scan the array */
    for ( ; cnt != 0 ; --cnt, p += VMB_OBJECT_ID)
        fix_vmb_obj(vmg_ p);
}


/* ------------------------------------------------------------------------ */
/*
 *   Save/restore 
 */

/* 
 *   table-of-contents flags 
 */

/* object is transient (so the object isn't saved to the state file) */
#define VMOBJ_TOC_TRANSIENT 0x0001


/*
 *   Save state to a file 
 */
void CVmObjTable::save(VMG_ CVmFile *fp)
{
    CVmObjPageEntry **pg;
    CVmObjPageEntry *entry;
    size_t i;
    size_t j;
    vm_obj_id_t id;
    size_t toc_cnt;
    size_t save_cnt;
    long toc_cnt_pos;
    long end_pos;

    /*
     *   Before we save, perform a full GC pass.  This will ensure that we
     *   have removed all objects that are referenced only weakly, and
     *   cleaned up the weak references to them; this is important because
     *   we don't trace weak references for the purposes of calculating the
     *   set of objects that must be saved, and hence won't save any objects
     *   that are only weakly referenced, which would leave dangling
     *   references in the saved state if those weak references weren't
     *   cleaned up before the objects containing them are saved. 
     */
    gc_full(vmg0_);

    /* 
     *   Make sure that all of the metaclasses that we are actually using
     *   are in the metaclass dependency table.  We store the table in the
     *   file, because the table provides the mapping from file-local
     *   metaclass ID's to actual metaclasses; we must make sure that the
     *   table is complete (i.e., contains an entry for each metaclass of
     *   which there is an instance) before storing the table. 
     */
    add_metadeps_for_instances(vmg0_);

    /* save the metaclass table */
    G_meta_table->write_to_file(fp);

    /* 
     *   Figure out what objects we need to save.  We only need to save
     *   objects that are directly reachable from the root object set, from
     *   the imports, or from the globals.
     *   
     *   We don't need to save objects that are only accessible from the
     *   undo, because we don't save any undo information in the file.  We
     *   also don't need to save any objects that are reachable only from
     *   the stack, since the stack is inherently transient.
     *   
     *   Note that we don't need to trace from transient objects, since we
     *   won't be saving the transient objects and thus won't need to save
     *   anything referenced only from transient objects.
     *   
     *   So, we merely trace objects reachable from the imports, globals,
     *   and work queue.  At any time between GC passes, the work queue
     *   contains the complete list of root-set objects, hence we can simply
     *   trace from the current work queue.  
     */
    gc_trace_imports(vmg0_);
    gc_trace_globals(vmg0_);
    gc_trace_work_queue(vmg_ FALSE);

    /*
     *   Before we save the objects themselves, save a table of contents of
     *   the dynamically-allocated objects to be saved.  This table of
     *   contents will allow us to fix up references to objects on reloading
     *   the file with the new object numbers we assign them at that time.
     *   First, write a placeholder for the table of contents entry count.
     *   
     *   Note that we must store the table of contents in ascending order of
     *   object ID.  This happens naturally, since we scan the table in
     *   order of object ID.  
     */
    toc_cnt = 0;
    save_cnt = 0;
    toc_cnt_pos = fp->get_pos();
    fp->write_uint4(0);

    /* now scan the object pages and save the table of contents */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* scan all objects on this page */
        for (j = VM_OBJ_PAGE_CNT, entry = *pg ; j > 0 ; -- j, ++entry, ++id)
        {
            /* 
             *   If the entry is currently reachable, and it was dynamically
             *   allocated (which means it's not in the root set), then add
             *   it to the table of contents.  Note that we won't
             *   necessarily be saving the object, because the object could
             *   be transient - but if so, then we still want the record of
             *   the transient object, so we'll know on reloading that the
             *   object is no longer available.  
             */
            if (!entry->free_
                && entry->reachable_ == VMOBJ_REACHABLE
                && !entry->in_root_set_)
            {
                ulong flags;
                
                /* set up the flags */
                flags = 0;
                if (entry->transient_)
                    flags |= VMOBJ_TOC_TRANSIENT;

                /* write the object ID and flags */
                fp->write_uint4(id);
                fp->write_uint4(flags);

                /* count it */
                ++toc_cnt;
            }

            /* if it's saveable, count it among the saveable objects */
            if (entry->is_saveable())
                ++save_cnt;
        }
    }

    /* go back and fix up the size prefix for the table of contents */
    end_pos = fp->get_pos();
    fp->set_pos(toc_cnt_pos);
    fp->write_uint4(toc_cnt);
    fp->set_pos(end_pos);

    /* write the saveable object count, which we calculated above */
    fp->write_uint4(save_cnt);

    /* scan all object pages, and save each reachable object */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* scan all objects on this page */
        for (j = VM_OBJ_PAGE_CNT, entry = *pg ; j > 0 ; -- j, ++entry, ++id)
        {
            /* if this object is saveable, save it */
            if (entry->is_saveable())
            {
                uint idx;
                char buf[2];
                
                /* write the object ID */
                fp->write_uint4(id);

                /* store the root-set flag */
                buf[0] = (entry->in_root_set_ != 0);

                /* store the dependency table index */
                idx = entry->get_vm_obj()->
                      get_metaclass_reg()->get_reg_idx();
                buf[1] = (char)G_meta_table->get_dependency_index(idx);

                /* write the data */
                fp->write_bytes(buf, 2);

                /* save the metaclass-specific state */
                entry->get_vm_obj()->save_to_file(vmg_ fp);
            }
            
            /* 
             *   restore this object to the appropriate conditions in
             *   preparation for the next GC pass, so that we leave things
             *   as we found them -- saving the VM's state thus has no
             *   effect on the VM's state 
             */
            gc_set_init_conditions(id, entry);
        }
    }
}

/*
 *   Restore state from a file 
 */
int CVmObjTable::restore(VMG_ CVmFile *fp, CVmObjFixup **fixups)
{
    int err;
    ulong cnt;

    /* presume we won't create a fixup table */
    *fixups = 0;
    
    /* load the metaclass table */
    if ((err = G_meta_table->read_from_file(fp)) != 0)
        return err;

    /* 
     *   Reset all objects to the initial image file load state.  Note that
     *   we wait until after we've read the metaclass table to reset the
     *   objects, because any intrinsic class objects in the root set will
     *   need to re-initialize their presence in the metaclass table, which
     *   they can't do until after the metaclass table has itself been
     *   reloaded. 
     */
    G_obj_table->reset_to_image(vmg0_);

    /* read the size of the table of contents */
    cnt = fp->read_uint4();

    /* allocate the fixup table */
    *fixups = new CVmObjFixup(cnt);

    /* read the fixup table */
    for ( ; cnt != 0 ; --cnt)
    {
        vm_obj_id_t old_id;
        vm_obj_id_t new_id;
        ulong flags;
        
        /* read the next entry */
        old_id = (vm_obj_id_t)fp->read_uint4();
        flags = fp->read_uint4();
        
        /* 
         *   Allocate a new object ID for this entry.  If the object was
         *   transient, then it won't actually have been saved, so we must
         *   fix up references to the object to nil.  
         */
        if (!(flags & VMOBJ_TOC_TRANSIENT))
            new_id = vm_new_id(vmg_ FALSE);
        else
            new_id = VM_INVALID_OBJ;
        
        /* 
         *   Add the entry.  Note that the table of contents is stored in
         *   ascending order of old ID (i.e., the ID's in the saved state
         *   file's numbering system); this is the same ordering required by
         *   the fixup table, so we can simply read entries from the file
         *   and add them directly to the fixup table.  
         */
        (*fixups)->add_fixup(old_id, new_id);
    }
    
    /* read the number of saved objects */
    cnt = fp->read_uint4();
    
    /* read the objects */
    for ( ; cnt != 0 ; --cnt)
    {
        char buf[2];
        vm_obj_id_t id;
        int in_root_set;
        uint meta_idx;
        CVmObject *obj;
        
        /* read the original object ID */
        id = (vm_obj_id_t)fp->read_uint4();

        /* read the root-set flag and dependency table index */
        fp->read_bytes(buf, 2);
        in_root_set = buf[0];
        meta_idx = (uchar)buf[1];

        /* 
         *   if it's not in the root set, we need to create a new object;
         *   otherwise, the object must already exist, since it came from
         *   the object file 
         */
        if (in_root_set)
        {
            /* 
             *   make sure the object is valid - since it's supposedly in
             *   the root set, it must already exist 
             */
            if (!is_obj_id_valid(id) || get_entry(id)->free_)
                return VMERR_SAVED_OBJ_ID_INVALID;
        }
        else
        {
            /* 
             *   the object was dynamically allocated, so it will have a new
             *   object number - fix up the object ID to the new numbering
             *   system 
             */
            id = (*fixups)->get_new_id(vmg_ id);

            /* create the object */
            G_meta_table->create_for_restore(vmg_ meta_idx, id);
        }

        /* read the object's data */
        obj = get_obj(id);
        obj->restore_from_file(vmg_ id, fp, *fixups);
    }
    
    /* success */
    return 0;
}

/*
 *   Save an image data pointer 
 */
void CVmObjTable::save_image_pointer(vm_obj_id_t obj_id, const char *ptr,
                                     size_t siz)
{
    vm_image_ptr *slot;
    
    /* allocate a new page if we're out of slots on the current page */
    if (image_ptr_head_ == 0)
    {
        /* no pages yet - allocate the first page */
        image_ptr_head_ = (vm_image_ptr_page *)
                          t3malloc(sizeof(vm_image_ptr_page));
        if (image_ptr_head_ == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* it's also the last page */
        image_ptr_tail_ = image_ptr_head_;
        image_ptr_tail_->next_ = 0;

        /* no slots used on this page yet */
        image_ptr_last_cnt_ = 0;
    }
    else if (image_ptr_last_cnt_ == VM_IMAGE_PTRS_PER_PAGE)
    {
        /* the last page is full - allocate another one */
        image_ptr_tail_->next_ = (vm_image_ptr_page *)
                                 t3malloc(sizeof(vm_image_ptr_page));
        if (image_ptr_tail_->next_ == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* it's the new last page */
        image_ptr_tail_ = image_ptr_tail_->next_;
        image_ptr_tail_->next_ = 0;

        /* no slots used on this page yet */
        image_ptr_last_cnt_ = 0;
    }

    /* get the next available slot */
    slot = &image_ptr_tail_->ptrs_[image_ptr_last_cnt_];

    /* save the data */
    slot->obj_id_ = obj_id;
    slot->image_data_ptr_ = ptr;
    slot->image_data_len_ = siz;

    /* count the new record */
    ++image_ptr_last_cnt_;
}

/*
 *   Reset to initial image file state
 */
void CVmObjTable::reset_to_image(VMG0_)
{
    CVmObjPageEntry **pg;
    CVmObjPageEntry *entry;
    size_t i;
    size_t j;
    vm_obj_id_t id;
    vm_image_ptr_page *ip_page;

    /* 
     *   Drop all undo information.  Since we're resetting to the initial
     *   state, the undo for our outgoing state will no longer be
     *   relevant. 
     */
    G_undo->drop_undo(vmg0_);

    /* delete all of the globals */
    if (globals_ != 0)
    {
        delete globals_;
        globals_ = 0;
    }

    /* 
     *   Go through the object table and reset each non-transient object in
     *   the root set to its initial conditions.  
     */
    for (id = 0, i = pages_used_, pg = pages_ ; i > 0 ; ++pg, --i)
    {
        /* scan all objects on this page */
        for (j = VM_OBJ_PAGE_CNT, entry = *pg ; j > 0 ; --j, ++entry, ++id)
        {
            /* 
             *   if it's not free, and it's in the root set, and it's not
             *   transient, reset it 
             */
            if (!entry->free_ && entry->in_root_set_ && !entry->transient_)
            {
                /*
                 *   This object is part of the root set, so it's part of
                 *   the state immediately after loading the image.  Reset
                 *   the object to its load file conditions.  
                 */
                entry->get_vm_obj()->reset_to_image(vmg_ id);
            }
        }
    }

    /*
     *   Go through all of the objects for which we've explicitly saved the
     *   original image file location, and ask them to reset using the image
     *   data. 
     */
    for (ip_page = image_ptr_head_ ; ip_page != 0 ; ip_page = ip_page->next_)
    {
        size_t cnt;
        vm_image_ptr *slot;
        
        /* 
         *   get the count for this page - if this is the last page, it's
         *   the last page counter; otherwise, it's a full page, since we
         *   fill up each page before creating a new one 
         */
        if (ip_page->next_ == 0)
            cnt = image_ptr_last_cnt_;
        else
            cnt = VM_IMAGE_PTRS_PER_PAGE;

        /* go through the records on the page */
        for (slot = ip_page->ptrs_ ; cnt != 0 ; --cnt, ++slot)
        {
            /* it this object is non-transient, reload it */
            if (!get_entry(slot->obj_id_)->transient_)
            {
                /* reload it using the saved image data */
                vm_objp(vmg_ slot->obj_id_)
                    ->reload_from_image(vmg_ slot->obj_id_,
                                        slot->image_data_ptr_,
                                        slot->image_data_len_);
            }
        }
    }
}

/*
 *   Create a global variable 
 */
vm_globalvar_t *CVmObjTable::create_global_var()
{
    vm_globalvar_t *var;

    /* create the new variable */
    var = new vm_globalvar_t();

    /* initialize the variable's value to nil */
    var->val.set_nil();

    /* link it into our list of globals */
    var->nxt = global_var_head_;
    var->prv = 0;
    if (global_var_head_ != 0)
        global_var_head_->prv = var;
    global_var_head_ = var;

    /* return the variable */
    return var;
}

/*
 *   Delete a global variable 
 */
void CVmObjTable::delete_global_var(vm_globalvar_t *var)
{
    /* unlink it from the list of globals */
    if (var->nxt != 0)
        var->nxt->prv = var->prv;

    if (var->prv != 0)
        var->prv->nxt = var->nxt;
    else
        global_var_head_ = var->nxt;

    /* delete the memory */
    delete var;
}


/* ------------------------------------------------------------------------ */
/*
 *   post-load initialization status 
 */
enum pli_stat_t
{
    PLI_UNINITED,                                    /* not yet initialized */
    PLI_IN_PROGRESS,                          /* initialization in progress */
    PLI_INITED                                  /* initialization completed */
};

/*
 *   Post-load initialization hash table entry.  We use the object ID,
 *   treating the binary representation as a string of bytes, as the hash
 *   key.  
 */
class CVmHashEntryPLI: public CVmHashEntryCS
{
public:
    CVmHashEntryPLI(vm_obj_id_t id)
        : CVmHashEntryCS((char *)&id, sizeof(id), TRUE)
    {
        /* 
         *   remember our object ID for easy access (technically, it's stored
         *   as our key value as well, so this is redundant; but it's
         *   transformed into a block of bytes for the key, so it's easier to
         *   keep a separate copy of the true type) 
         */
        id_ = id;

        /* initialize our status */
        status = PLI_UNINITED;
    }

    /* our object ID */
    vm_obj_id_t id_;

    /* status for current operation */
    pli_stat_t status;
};

/*
 *   Request post-load initialization.  
 */
void CVmObjTable::request_post_load_init(vm_obj_id_t obj)
{
    CVmHashEntryPLI *entry;

    /* check for an existing entry - if there's not one already, add one */
    entry = (CVmHashEntryPLI *)
            post_load_init_table_->find((char *)&obj, sizeof(obj));
    if (entry == 0)
    {
        /* it's not there yet - add a new entry */
        post_load_init_table_->add(new CVmHashEntryPLI(obj));

        /* mark the object as having requested post-load initialization */
        get_entry(obj)->requested_post_load_init_ = TRUE;
    }
}

/*
 *   Explicitly invoke post-load initialization on a given object 
 */
void CVmObjTable::ensure_post_load_init(VMG_ vm_obj_id_t obj)
{
    CVmHashEntryPLI *entry;

    /* find the entry */
    entry = (CVmHashEntryPLI *)
            post_load_init_table_->find((char *)&obj, sizeof(obj));

    /* if we found it, invoke its initialization method */
    if (entry != 0)
        call_post_load_init(vmg_ entry);
}

/*
 *   Invoke post-load initialization on the given object, if appropriate 
 */
void CVmObjTable::call_post_load_init(VMG_ CVmHashEntryPLI *entry)
{
    /* check the status */
    switch (entry->status)
    {
    case PLI_UNINITED:
        /*  
         *   It's not yet initialized, so we need to initialize it now.  Mark
         *   it as having its initialization in progress.  
         */
        entry->status = PLI_IN_PROGRESS;

        /* 
         *   push the entry on the stack to protect it from gc while we're
         *   initializing it 
         */
        G_stk->push()->set_obj(entry->id_);

        /* invoke its initialization */
        vm_objp(vmg_ entry->id_)->post_load_init(vmg_ entry->id_);

        /* mark the object as having completed initialization */
        entry->status = PLI_INITED;

        /* discard our GC protection */
        G_stk->discard();

        /* done */
        break;

    case PLI_IN_PROGRESS:
        /*
         *   The object is in the process of being initialized.  This must
         *   mean that the object's initializer is calling us indirectly,
         *   probably through another object's initializer that it invoked
         *   explicitly as a dependency, which in turn means that we have a
         *   circular dependency.  This is illegal, so abort with an error.  
         */
        err_throw(VMERR_CIRCULAR_INIT);
        break;

    case PLI_INITED:
        /* it's already been initialized, so there's nothing to do */
        break;
    }
}

/*
 *   Remove a post-load initialization record 
 */
void CVmObjTable::remove_post_load_init(vm_obj_id_t obj)
{
    CVmHashEntryPLI *entry;

    /* find the entry */
    entry = (CVmHashEntryPLI *)
            post_load_init_table_->find((char *)&obj, sizeof(obj));

    /* if we found the entry, remove it from the table and delete it */
    if (entry != 0)
    {
        /* remove it */
        post_load_init_table_->remove(entry);

        /* delete it */
        delete entry;

        /* mark the entry as no longer being registered for post-load init */
        get_entry(obj)->requested_post_load_init_ = FALSE;
    }
}

/*
 *   post-load initialization enumeration context 
 */
struct pli_enum_ctx
{
    vm_globals *globals;
};

/*
 *   Invoke post-load initialization on all objects that have requested it.
 *   This must be called after initial program load, restarts, and restore
 *   operations.  
 */
void CVmObjTable::do_all_post_load_init(VMG0_)
{
    pli_enum_ctx ctx;

    /* set up our context */
    ctx.globals = VMGLOB_ADDR;

    /* first, mark all entries as having status 'uninitialized' */
    post_load_init_table_->enum_entries(&pli_status_cb, &ctx);

    /* next, invoke the initializer method for each entry */
    post_load_init_table_->enum_entries(&pli_invoke_cb, &ctx);
}

/* 
 *   post-load initialization enumeration callback: mark entries as having
 *   status 'uninitialized' 
 */
void CVmObjTable::pli_status_cb(void *ctx0, CVmHashEntry *entry0)
{
    CVmHashEntryPLI *entry = (CVmHashEntryPLI *)entry0;

    /* mark the entry as having status 'uninitialized' */
    entry->status = PLI_UNINITED;
}

/* 
 *   post-load initialization enumeration callback: mark entries as having
 *   status 'uninitialized' 
 */
void CVmObjTable::pli_invoke_cb(void *ctx0, CVmHashEntry *entry0)
{
    CVmHashEntryPLI *entry = (CVmHashEntryPLI *)entry0;
    VMGLOB_PTR(((pli_enum_ctx *)ctx0)->globals);

    /* invoke post-load initialization on the object */
    call_post_load_init(vmg_ entry);
}


/* ------------------------------------------------------------------------ */
/*
 *   Memory manager implementation 
 */

CVmMemory::CVmMemory(VMG_ CVmVarHeap *varheap)
{
    /* remember our variable-size heap */
    varheap_ = varheap;
    
    /* initialize the variable-size heap */
    varheap_->init(vmg0_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Hybrid cell/malloc memory manager - cell sub-block manager
 */

/*
 *   Allocate an object.  Since we always allocate blocks of a fixed size,
 *   we can ignore the size parameter; the heap manager will only route us
 *   requests that fit in our blocks.  
 */
CVmVarHeapHybrid_hdr *CVmVarHeapHybrid_head::alloc(size_t)
{
    /* if there isn't an entry, allocate another array */
    if (first_free_ == 0)
    {
        CVmVarHeapHybrid_array *arr;
        size_t real_cell_size;
        char *p;
        size_t cnt;
        
        /* 
         *   Allocate another page.  We need space for the array header
         *   itself, plus space for all of the cells.  We want page_count_
         *   cells, each of size cell_size_ plus the size of the per-cell
         *   header, rounded to the worst-case alignment size for the
         *   platform.  
         */
        real_cell_size = osrndsz(cell_size_
                                 + osrndsz(sizeof(CVmVarHeapHybrid_hdr)));
        arr = (CVmVarHeapHybrid_array *)
              t3malloc(sizeof(CVmVarHeapHybrid_array)
                       + (page_count_ * real_cell_size));

        /* if that failed, throw an error */
        if (arr == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* link the array into the master list */
        arr->next_array = mem_mgr_->first_array_;
        mem_mgr_->first_array_ = arr;

        /* 
         *   Build the free list.  Each cell goes into the free list; the
         *   'next' pointer is stored in the data area of the cell. 
         */
        for (p = arr->mem, cnt = page_count_ ; cnt > 0 ;
             p += real_cell_size, --cnt)
        {
            /* link this one into the free list */
            *(void **)p = first_free_;
            first_free_ = (void *)p;
        }
    }
    
    /* remember the return value */
    CVmVarHeapHybrid_hdr *ret = (CVmVarHeapHybrid_hdr *)first_free_;

    /* 
     *   when we initialized or last freed this entry, we stored a pointer
     *   to the next item in the free list in the object's data - retrieve
     *   this pointer now, and update our free list head to point to the
     *   next item 
     */
    first_free_ = *(void **)first_free_;

    /* fill in the block's pointer to the allocating heap (i.e., this) */
    ret->block = this;

    /* return the item */
    return ret;
}

/*
 *   Reallocate 
 */
void *CVmVarHeapHybrid_head::realloc(CVmVarHeapHybrid_hdr *mem, size_t siz,
                                     CVmObject *obj)
{
    /* 
     *   if the new block fits in our cell size, return the original
     *   memory unchanged; note that we must adjust the pointer so that we
     *   return the client-visible portion 
     */
    if (siz <= cell_size_)
        return (void *)(mem + 1);

    /*
     *   The memory won't fit in our cell size, so not only can't we
     *   re-use the existing cell, but we can't allocate the memory from
     *   our own sub-block at all.  Allocate an entirely new block from
     *   the heap manager.
     */
    void *new_mem = mem_mgr_->alloc_mem(siz, obj);

    /* 
     *   Copy the old cell's contents to the new memory.  Note that the
     *   user-visible portion of the old cell starts immediately after the
     *   header; don't copy the old header, since it's not applicable to
     *   the new object.  Note also that we got a pointer directly to the
     *   user-visible portion of the new object, so we don't need to make
     *   any adjustments to the new pointer.  
     */
    memcpy(new_mem, (void *)(mem + 1), cell_size_);

    /* free the old memory */
    free(mem);

    /* return the new memory */
    return new_mem;
}

/*
 *   Release memory 
 */
void CVmVarHeapHybrid_head::free(CVmVarHeapHybrid_hdr *mem)
{
    /* link the block into our free list */
    *(void **)mem = first_free_;
    first_free_ = (void *)mem;
}

/* ------------------------------------------------------------------------ */
/*
 *   Hybrid cell/malloc heap manager 
 */

/*
 *   construct
 */
CVmVarHeapHybrid::CVmVarHeapHybrid(CVmObjTable *objtab)
{
    /* remember my object table */
    objtab_ = objtab;

    /* set the cell heap count */
    cell_heap_cnt_ = 5;

    /* allocate our cell heap pointer array */
    cell_heaps_ = (CVmVarHeapHybrid_head **)
                  t3malloc(cell_heap_cnt_ * sizeof(CVmVarHeapHybrid_head *));

    /* if that failed, throw an error */
    if (cell_heaps_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* 
     *   Allocate our cell heaps.  Set up the heaps so that the pages run
     *   about 32k each.  
     */
    cell_heaps_[0] = new CVmVarHeapHybrid_head(this, 32, 850);
    cell_heaps_[1] = new CVmVarHeapHybrid_head(this, 64, 400);
    cell_heaps_[2] = new CVmVarHeapHybrid_head(this, 128, 200);
    cell_heaps_[3] = new CVmVarHeapHybrid_head(this, 256, 100);
    cell_heaps_[4] = new CVmVarHeapHybrid_head(this, 512, 50);

    /* allocate our malloc heap manager */
    malloc_heap_ = new CVmVarHeapHybrid_malloc();

    /* we haven't allocated any cell array pages yet */
    first_array_ = 0;
}

/*
 *   delete 
 */
CVmVarHeapHybrid::~CVmVarHeapHybrid()
{
    size_t i;
    
    /* delete our cell heaps */
    for (i = 0 ; i < cell_heap_cnt_ ; ++i)
        delete cell_heaps_[i];

    /* delete the cell heap pointer array */
    t3free(cell_heaps_);

    /* delete all of the arrays */
    while (first_array_ != 0)
    {
        CVmVarHeapHybrid_array *nxt;

        /* remember the next one */
        nxt = first_array_->next_array;

        /* delete this one */
        t3free(first_array_);

        /* move on to the next one */
        first_array_ = nxt;
    }

    /* delete the malloc-based subheap manager */
    delete malloc_heap_;
}

/*
 *   allocate memory 
 */
void *CVmVarHeapHybrid::alloc_mem(size_t siz, CVmObject *)
{
    CVmVarHeapHybrid_head **subheap;
    size_t i;

    /* count the gc statistics if desired */
    IF_GC_STATS(gc_stats.count_alloc_bytes(siz));

    /* count the allocation */
    objtab_->count_alloc(siz);

    /* scan for a cell-based subheap that can handle the request */
    for (i = 0, subheap = cell_heaps_ ; i < cell_heap_cnt_ ; ++i, ++subheap)
    {
        /* 
         *   If it will fit in this one's cell size, allocate it from this
         *   subheap.  Note that we must adjust the return pointer so that
         *   it points to the caller-visible portion of the block returned
         *   from the subheap, which immediately follows the internal
         *   header.  
         */
        if (siz <= (*subheap)->get_cell_size())
        {
            CVmVarHeapHybrid_hdr *hdr = (*subheap)->alloc(siz);
            IF_GC_STATS(hdr->siz = siz);
            return (void *)(hdr + 1);
        }
    }

    /*
     *   We couldn't find a cell-based manager that can handle a block
     *   this large.  Allocate the block from the default malloc heap.
     *   Note that the caller-visible block is the part that immediately
     *   follows our internal header, so we must adjust the return pointer
     *   accordingly.  
     */
    CVmVarHeapHybrid_hdr *hdr = malloc_heap_->alloc(siz);
    IF_GC_STATS(hdr->siz = siz);
    return (void *)(hdr + 1);
}

/*
 *   reallocate memory 
 */
void *CVmVarHeapHybrid::realloc_mem(size_t siz, void *mem,
                                    CVmObject *obj)
{
    CVmVarHeapHybrid_hdr *hdr;

    /* 
     *   Count the allocation.  This isn't really a new allocation of 'siz'
     *   bytes, since we're only expanding an object that already has a
     *   non-zero size.  But in many cases this will simply allocate new
     *   memory anyway, copying the old object into the new memory, so it
     *   could actually count against our OS-level working set.  That makes
     *   it worthwhile to count it against the GC quota.  
     */
    objtab_->count_alloc(siz);

    /* 
     *   get the block header, which immediately precedes the
     *   caller-visible block 
     */
    hdr = ((CVmVarHeapHybrid_hdr *)mem) - 1;

    /* count the gc statistics if desired */
    IF_GC_STATS((gc_stats.count_realloc_bytes(hdr->siz, siz), hdr->siz = siz));

    /*
     *   read the header to get the block manager that originally
     *   allocated the memory, and ask it to reallocate the memory 
     */
    return hdr->block->realloc(hdr, siz, obj);
}

/*
 *   free memory 
 */
void CVmVarHeapHybrid::free_mem(void *mem)
{
    CVmVarHeapHybrid_hdr *hdr;

    /* 
     *   get the block header, which immediately precedes the
     *   caller-visible block 
     */
    hdr = ((CVmVarHeapHybrid_hdr *)mem) - 1;

    /* count the gc statistics if desired */
    IF_GC_STATS(gc_stats.count_free_bytes(hdr->siz));

    /* 
     *   read the header to get the block manager that originally
     *   allocated the memory, and ask it to free the memory 
     */
    hdr->block->free(hdr);
}


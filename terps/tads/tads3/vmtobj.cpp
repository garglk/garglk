#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMTOBJ.CPP,v 1.3 1999/05/17 02:52:28 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtobj.cpp - TADS object implementation
Function
  
Notes
  
Modified
  10/30/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#include "t3std.h"
#include "vmglob.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmobj.h"
#include "vmtobj.h"
#include "vmundo.h"
#include "vmtype.h"
#include "vmfile.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmpredef.h"
#include "vmmeta.h"
#include "vmlst.h"
#include "vmintcls.h"
#include "vmbif.h"
#include "vmstr.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate a new object header 
 */
vm_tadsobj_hdr *vm_tadsobj_hdr::alloc(VMG_ CVmObjTads *self,
                                      unsigned short sc_cnt,
                                      unsigned short prop_cnt)
{
    ushort hash_siz;
    size_t siz;
    size_t i;
    vm_tadsobj_hdr *hdr;
    char *mem;
    vm_tadsobj_prop **hashp;
    
    /* 
     *   Figure the size of the hash table to allocate.  Empirically, a good
     *   size seems to be about one hash slot per property. 
     */
    if (prop_cnt <= 2)
        hash_siz = 2;
    else if (prop_cnt <= 4)
        hash_siz = 4;
    else if (prop_cnt <= 8)
        hash_siz = 8;
    else if (prop_cnt <= 16)
        hash_siz = 16;
    else if (prop_cnt <= 32)
        hash_siz = 32;
    else if (prop_cnt <= 64)
        hash_siz = 64;
    else if (prop_cnt <= 128)
        hash_siz = 128;
    else
        hash_siz = 256;

    /* 
     *   increase the requested property count to the hash size at a minimum
     *   - this will avoid the need to reallocate the object to make room
     *   for more properties until we'd have to resize the hash table, at
     *   which point we have to reallocate the object anyway 
     */
    if (prop_cnt < hash_siz)
        prop_cnt = hash_siz;

    /* figure the size of the structure we need */
    siz = sizeof(vm_tadsobj_hdr)
          + (sc_cnt - 1) * sizeof(hdr->sc[0])
          + (hash_siz) * sizeof(hdr->hash_arr[0])
          + prop_cnt * sizeof(hdr->prop_entry_arr[0]);

    /* allocate the memory */
    hdr = (vm_tadsobj_hdr *)G_mem->get_var_heap()->alloc_mem(siz, self);

    /* 
     *   Set up to suballocate out of this block.  Free memory in the block
     *   starts after our structure and the array of superclass entries. 
     */
    mem = (char *)&hdr->sc[sc_cnt];

    /* clear our flags and load-image flags */
    hdr->li_obj_flags = 0;
    hdr->intern_obj_flags = 0;

    /* the object has no precalculated inheritance path yet */
    hdr->inh_path = 0;

    /* suballocate the hash buckets */
    hdr->hash_siz = hash_siz;
    hdr->hash_arr = (vm_tadsobj_prop **)mem;

    /* clear out the hash buckets */
    for (hashp = hdr->hash_arr, i = hash_siz ; i != 0 ; ++hashp, --i)
        *hashp = 0;

    /* move past the memory taken by the hash buckets */
    mem = (char *)(hdr->hash_arr + hash_siz);

    /* suballocate the array of hash entries */
    hdr->prop_entry_cnt = prop_cnt;
    hdr->prop_entry_arr = (vm_tadsobj_prop *)mem;

    /* all entries are currently free, so point to the first entry */
    hdr->prop_entry_free = 0;

    /* remember the superclass count */
    hdr->sc_cnt = sc_cnt;

    /* return the new object */
    return hdr;
}

/*
 *   Free 
 */
void vm_tadsobj_hdr::free_mem()
{
    /* if I have a precalculated inheritance path, delete it */
    if (inh_path != 0)
        t3free(inh_path);
}

/*
 *   Expand an existing object header to make room for more properties 
 */
vm_tadsobj_hdr *vm_tadsobj_hdr::expand(VMG_ CVmObjTads *self,
                                       vm_tadsobj_hdr *hdr)
{
    unsigned short prop_cnt;

    /* 
     *   Move up to the next property count increment.  If we're not huge,
     *   simply double the current size.  If we're getting large, expand by
     *   50%.  
     */
    prop_cnt = hdr->prop_entry_cnt;
    if (prop_cnt <= 128)
        prop_cnt *= 2;
    else
        prop_cnt += prop_cnt/2;

    /* expand to the new size */
    return expand_to(vmg_ self, hdr, hdr->sc_cnt, prop_cnt);
}

/*
 *   Expand an existing header to the given minimum property table size 
 */
vm_tadsobj_hdr *vm_tadsobj_hdr::expand_to(VMG_ CVmObjTads *self,
                                          vm_tadsobj_hdr *hdr,
                                          size_t new_sc_cnt,
                                          size_t new_prop_cnt)
{
    vm_tadsobj_hdr *new_hdr;
    size_t i;
    vm_tadsobj_prop *entryp;

    /* allocate a new object at the expanded property table size */
    new_hdr = alloc(vmg_ self, (ushort)new_sc_cnt, (ushort)new_prop_cnt);

    /* copy the superclasses from the original object */
    memcpy(new_hdr->sc, hdr->sc,
           (hdr->sc_cnt < new_sc_cnt ? hdr->sc_cnt : new_sc_cnt)
           * sizeof(hdr->sc[0]));

    /* use the same flags from the original object */
    new_hdr->li_obj_flags = hdr->li_obj_flags;
    new_hdr->intern_obj_flags = hdr->intern_obj_flags;

    /* 
     *   if the superclass count is changing, we're obviously changing the
     *   inheritance structure, in which case the old cached inheritance path
     *   is invalid - delete it if so 
     */
    if (new_sc_cnt != hdr->sc_cnt)
        hdr->inval_inh_path();

    /* copy the old inheritance path (if we still have one) */
    new_hdr->inh_path = hdr->inh_path;

    /* 
     *   Run through all of the existing properties and duplicate them in the
     *   new object, to build the new object's hash table.  Note that the
     *   free index is inherently equivalent to the count of properties in
     *   use.  
     */
    for (i = hdr->prop_entry_free, entryp = hdr->prop_entry_arr ; i != 0 ;
         --i, ++entryp)
    {
        /* add this property to the new table */
        new_hdr->alloc_prop_entry(entryp->prop, &entryp->val, entryp->flags);
    }

    /* delete the old header */
    G_mem->get_var_heap()->free_mem(hdr);
    
    /* return the new header */
    return new_hdr;
}

/*
 *   Allocate an entry for given property from the free pool.  The caller is
 *   responsible for checking that there's space in the free pool.  We do
 *   not check for an existing entry with the same caller ID, so the caller
 *   is responsible for making sure the property doesn't already exist in
 *   our table.  
 */
vm_tadsobj_prop *vm_tadsobj_hdr::alloc_prop_entry(
    vm_prop_id_t prop, const vm_val_t *val, unsigned int flags)
{
    /* get the hash code for the property */
    unsigned int hash = calc_hash(prop);

    /* use the next free entry */
    vm_tadsobj_prop *entry = &prop_entry_arr[prop_entry_free];

    /* link this entry into the list for its hash bucket */
    entry->nxt = hash_arr[hash];
    hash_arr[hash] = entry;

    /* count our use of the free entry */
    ++prop_entry_free;
    
    /* set the new entry's property ID */
    entry->prop = prop;

    /* set the value and flags */
    entry->val = *val;
    entry->flags = (unsigned char)flags;

    /* return the entry */
    return entry;
}

/*
 *   Find an entry 
 */
vm_tadsobj_prop *vm_tadsobj_hdr::find_prop_entry(uint prop)
{
    /* scan the list of entries in this bucket */
    for (vm_tadsobj_prop *entry = hash_arr[calc_hash(prop)] ;
         entry != 0 ; entry = entry->nxt)
    {
        /* if this entry matches, return it */
        if (entry->prop == prop)
            return entry;
    }

    /* didn't find it */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassTads metaclass_reg_obj;
CVmMetaclass *CVmObjTads::metaclass_reg_ = &metaclass_reg_obj;


/* function table */
int (CVmObjTads::
     *CVmObjTads::func_table_[])(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc) =
{
    &CVmObjTads::getp_undef,
    &CVmObjTads::getp_create_instance,
    &CVmObjTads::getp_create_clone,
    &CVmObjTads::getp_create_trans_instance,
    &CVmObjTads::getp_create_instance_of,
    &CVmObjTads::getp_create_trans_instance_of,
    &CVmObjTads::getp_set_sc_list,
    &CVmObjTads::getp_get_method,
    &CVmObjTads::getp_set_method
};

/*
 *   Function table indices.  We only need constant definitions for these
 *   for our static methods, since in other cases we translate through the
 *   function table.  
 */
const int PROPIDX_CREATE_INSTANCE = 1;
const int PROPIDX_CREATE_CLONE = 2;
const int PROPIDX_CREATE_TRANS_INSTANCE = 3;
const int PROPIDX_CREATE_INSTANCE_OF = 4;
const int PROPIDX_CREATE_TRANS_INSTANCE_OF = 5;

/* ------------------------------------------------------------------------ */
/*
 *   Static class initialization 
 */
void CVmObjTads::class_init(VMG0_)
{
    /* allocate the inheritance analysis object */
    VM_IFELSE_ALLOC_PRE_GLOBAL(
        G_tadsobj_queue = new CVmObjTadsInhQueue(),
        G_tadsobj_queue->init());
}

/*
 *   Static class termination 
 */
void CVmObjTads::class_term(VMG0_)
{
    /* delete the inheritance analysis object */
    VM_IF_ALLOC_PRE_GLOBAL(
        delete G_tadsobj_queue;
        G_tadsobj_queue = 0;
    )
}

/* ------------------------------------------------------------------------ */
/*
 *   Static creation methods 
 */

/* create dynamically using stack arguments */
vm_obj_id_t CVmObjTads::create_from_stack_intern(
    VMG_ const uchar **pc_ptr, uint argc, int is_transient)
{
    vm_obj_id_t id;
    CVmObjTads *obj;
    vm_val_t val;
    vm_obj_id_t srcobj;

    /* check arguments */
    if (argc == 0)
    {
        /* no superclass argument - create a base object */
        val.set_nil();
    }
    else
    {
        /* 
         *   We have arguments.  The first is the superclass argument, which
         *   must be an object or nil.  Retrieve it and make sure it's
         *   valid.  
         */
        G_stk->pop(&val);
        if (val.typ != VM_OBJ && val.typ != VM_NIL)
            err_throw(VMERR_OBJ_VAL_REQD_SC);

        /* if it's the invalid object, treat it as nil */
        if (val.typ == VM_OBJ && val.val.obj == VM_INVALID_OBJ)
            val.set_nil();

        /* we cannot create an instance of a transient object */
        if (val.typ != VM_NIL
            && G_obj_table->is_obj_transient(val.val.obj))
            err_throw(VMERR_BAD_DYNAMIC_NEW);

        /* count the removal of the first argument */
        --argc;
    }

    /* 
     *   create the object - this type of construction is never used for
     *   root set objects 
     */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* make the object transient if desired */
    if (is_transient)
        G_obj_table->set_obj_transient(id);

    /* 
     *   create a TADS object with the appropriate number of superclasses
     *   (0 if no superclass was specified, 1 if one was), and the default
     *   number of initial mutable properties 
     */
    obj = new (vmg_ id) CVmObjTads(vmg_ (val.typ == VM_NIL ? 0 : 1),
                                   VMTOBJ_PROP_INIT);

    /* set the object's superclass */
    if (val.typ != VM_NIL)
        obj->set_sc(vmg_ 0, val.val.obj);

    /* 
     *   Invoke the object's "construct" method, passing it the arguments
     *   that are still on the stack.  If the new object doesn't define or
     *   inherit the "construct" method, simply push the new object
     *   reference onto the stack directly.  
     */
    if (obj->get_prop(vmg_ G_predef->obj_construct, &val, id, &srcobj, 0))
    {
        vm_val_t srcobj_val;
        vm_val_t id_val;
        const uchar *dummy_pc_ptr;
        uint caller_ofs;
        vm_rcdesc rc, *rcp;
        
        /* use the null PC pointer if the caller didn't supply one */
        if (pc_ptr == 0)
        {
            /* there's no caller PC pointer - use a dummy value */
            pc_ptr = &dummy_pc_ptr;
            caller_ofs = 0;

            /* set up the recursive context */
            rcp = &rc;
            rc.init_ret(vmg_ "TadsObject.construct");
        }
        else
        {
            /* get the caller's offset */
            caller_ofs = G_interpreter->pc_to_method_ofs(*pc_ptr);
            rcp = 0;
        }

        /* 
         *   A "construct" method is defined - have the interpreter invoke
         *   it, which will set up the interpreter to start executing its
         *   byte-code.  This is all we need to do, since we assume and
         *   require that the constructor will return the new object as
         *   its return value when it's done.  
         */
        srcobj_val.set_obj(srcobj);
        id_val.set_obj(id);
        *pc_ptr = G_interpreter->get_prop(vmg_ caller_ofs, &srcobj_val,
                                          G_predef->obj_construct,
                                          &id_val, argc, rcp);
    }
    else
    {
        /* 
         *   there's no "construct" method defined - if we have any
         *   arguments, its an error 
         */
        if (argc != 0)
            err_throw(VMERR_WRONG_NUM_OF_ARGS);

        /* leave the new object value in R0 */
        G_interpreter->get_r0()->set_obj(id);
    }

    /* return the new object */
    return id;
}

/* create an object with no initial extension */
vm_obj_id_t CVmObjTads::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjTads();
    return id;
}

/* 
 *   Create an object with a given number of superclasses, and a given
 *   property table size.  Each superclass must be set before the object
 *   can be used, and the property table is initially empty.
 *   
 *   This form is used to create objects dynamically; this call is never
 *   used to load an object from an image file.  
 */
vm_obj_id_t CVmObjTads::create(VMG_ int in_root_set,
                               ushort superclass_count, ushort prop_count)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjTads(vmg_ superclass_count, prop_count);
    return id;
}

/*
 *   Create an instance based on multiple superclasses, using the
 *   createInstanceOf() interface.  Arguments are passed on the stack.  Each
 *   argument gives a superclass, and optionally the arguments for its
 *   inherited constructor.  If an argument is a simple object/class, then we
 *   won't inherit that object's constructor at all.  If an argument is a
 *   list, then the first element of the list gives the class, and the
 *   remaining elements of the list give the arguments to pass to that
 *   class's inherited constructor.  
 */
vm_obj_id_t CVmObjTads::create_from_stack_multi(
    VMG_ uint argc, int is_transient)
{
    /* allocate an object ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, TRUE, FALSE);
    if (is_transient)
        G_obj_table->set_obj_transient(id);

    /* create the new object */
    CVmObjTads *obj = new (vmg_ id) CVmObjTads(
        vmg_ (ushort)argc, VMTOBJ_PROP_INIT);

    /* push the new object, for garbage collector protection */
    G_interpreter->push_obj(vmg_ id);

    /* set the superclasses */
    for (ushort i = 0 ; i < argc ; ++i)
    {
        /* 
         *   get this argument (it's at i+1 because of the extra item we
         *   pushed for gc protection) 
         */
        vm_val_t *arg = G_stk->get(i + 1);

        /* 
         *   if it's a list, the superclass is the first element; otherwise,
         *   the argument is the superclass 
         */
        vm_val_t sc;
        const char *lstp = arg->get_as_list(vmg0_);
        if (lstp != 0)
        {
            /* it's a list - the first element is the superclass */
            CVmObjList::index_list(vmg_ &sc, lstp, 1);
        }
        else
        {
            /* not a list - the argument is the superclass */
            sc = *arg;
        }

        /* make sure it's a TadsObject */
        if (sc.typ != VM_OBJ || !is_tadsobj_obj(vmg_ sc.val.obj))
            err_throw(VMERR_BAD_TYPE_BIF);

        /* can't create an instance of a transient object */
        if (G_obj_table->is_obj_transient(sc.val.obj))
            err_throw(VMERR_BAD_DYNAMIC_NEW);

        /* set this superclass */
        obj->set_sc(vmg_ i, sc.val.obj);
    }

    /* set up the resursive invocation context */
    vm_rcdesc rc("TadsObject.contructMulti");

    /*
     *   The new object is ready to go.  All that remains is invoking any
     *   inherited construtors that the caller wants us to invoked.
     *   Constructor invocation is indicated by passing a list argument for
     *   the corresponding superclass, so run through the arguments and
     *   invoke each indicated constructor.  
     */
    for (ushort i = 0 ; i < argc ; ++i)
    {    
        /* get the next argument */
        vm_val_t *arg = G_stk->get(i + 1);

        /* if it's not a list, we don't want to invoke this constructor */
        const char *lstp = arg->get_as_list(vmg0_);
        if (lstp == 0)
        {
            /* no constructor call is wanted - just keep going */
            continue;
        }

        /* get the superclass from the list */
        vm_val_t sc;
        CVmObjList::index_list(vmg_ &sc, lstp, 1);

        /* get the number of list elements */
        uint lst_cnt = vmb_get_len(lstp);

        /* make sure we have room to push the arguments */
        if (!G_stk->check_space(lst_cnt - 1))
            err_throw(VMERR_STACK_OVERFLOW);

        /* 
         *   push the list elements in reverse order; don't push the first
         *   element, since it's the superclass itself rather than an
         *   argument to the constructor 
         */
        for (uint j = lst_cnt ; j > 1 ; --j)
            CVmObjList::index_and_push(vmg_ lstp, j);

        /* 
         *   Invoke the constructor via a recursive call into the VM.  Note
         *   that we're inheriting the property, so 'self' is the new object,
         *   but the 'target' object is the superclass whose constructor
         *   we're invoking. 
         */
        vm_val_t new_obj_val;
        new_obj_val.set_obj(id);
        G_interpreter->get_prop(vmg_ 0, &sc, G_predef->obj_construct,
                                &new_obj_val, lst_cnt - 1, &rc);
    }

    /* discard the arguments plus our own gc protection */
    G_stk->discard(argc + 1);

    /* return the new object */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Constructors 
 */

/*
 *   Create an object with a given number of superclasses, and a given
 *   property table size.  The superclasses must be individually set
 *   before the object can be used, and the property table is initially
 *   empty.
 *   
 *   This constructor is used only when creating a new object dynamically,
 *   and is never used to load an object from an image file.  
 */
CVmObjTads::CVmObjTads(VMG_ ushort superclass_count, ushort prop_count)
{
    /* allocate our header */
    ext_ = (char *)vm_tadsobj_hdr::alloc(vmg_ this, superclass_count,
                                         prop_count);
}


/* ------------------------------------------------------------------------ */
/*
 *   receive notification of deletion 
 */
void CVmObjTads::notify_delete(VMG_ int in_root_set)
{
    /* free our extension */
    if (ext_ != 0)
    {
        /* tell the header to delete its memory */
        get_hdr()->free_mem();

        /* delete the extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Create an instance of this class 
 */
void CVmObjTads::create_instance(VMG_ vm_obj_id_t self,
                                 const uchar **pc_ptr, uint argc) 
{
    /* push myself as the superclass */
    G_stk->push()->set_obj(self);

    /* use the normal stack creation routine */
    create_from_stack(vmg_ pc_ptr, argc+1);
}

/* ------------------------------------------------------------------------ */
/*
 *   Determine if the object has a finalizer method 
 */
int CVmObjTads::has_finalizer(VMG_ vm_obj_id_t self)
{
    vm_val_t val;
    vm_obj_id_t srcobj;

    /* 
     *   look up the finalization method - if it's defined, and it's a
     *   method, invoke it; otherwise do nothing 
     */
    return (G_predef->obj_destruct != VM_INVALID_PROP
            && get_prop(vmg_ G_predef->obj_destruct, &val, self, &srcobj, 0)
            && (val.typ == VM_CODEOFS
                || val.typ == VM_NATIVE_CODE
                || val.typ == VM_OBJX
                || val.typ == VM_BIFPTRX));
}

/* ------------------------------------------------------------------------ */
/*
 *   Invoke the object's finalizer 
 */
void CVmObjTads::invoke_finalizer(VMG_ vm_obj_id_t self)
{
    vm_val_t val;
    vm_obj_id_t srcobj;

    /* 
     *   look up the finalization method - if it's defined, and it's a
     *   method, invoke it; otherwise do nothing 
     */
    if (G_predef->obj_destruct != VM_INVALID_PROP
        && get_prop(vmg_ G_predef->obj_destruct, &val, self, &srcobj, 0)
        && (val.typ == VM_CODEOFS
            || val.typ == VM_NATIVE_CODE
            || val.typ == VM_OBJX
            || val.typ == VM_BIFPTRX))
    {
        /* 
         *   invoke the finalizer in a protected frame, to ensure that we
         *   catch any exceptions that are thrown out of the finalizer 
         */
        err_try
        {
            vm_val_t srcobj_val;
            vm_val_t self_val;

            /* 
             *   Invoke the finalizer.  Use a recursive VM invocation,
             *   since the VM must return to the garbage collector, not to
             *   what it was doing in the enclosing stack frame. 
             */
            srcobj_val.set_obj(srcobj);
            self_val.set_obj(self);
            vm_rcdesc rc("TadsObject.finalize");
            G_interpreter->get_prop(vmg_ 0, &srcobj_val,
                                    G_predef->obj_destruct, &self_val, 0, &rc);
        }
        err_catch_disc
        {
            /* silently ignore the error */
        }
        err_end;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Clear the undo flags for all properties 
 */
void CVmObjTads::clear_undo_flags()
{
    vm_tadsobj_prop *entry;
    uint i;
    vm_tadsobj_hdr *hdr = get_hdr();

    /* scan all property entries and clear their undo flags */
    for (i = hdr->prop_entry_free, entry = hdr->prop_entry_arr ;
         i != 0 ; --i, ++entry)
    {
        /* clear this entry's undo flag */
        entry->flags &= ~VMTO_PROP_UNDO;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set a property 
 */
void CVmObjTads::set_prop(VMG_ CVmUndo *undo, vm_obj_id_t self,
                          vm_prop_id_t prop, const vm_val_t *val)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* look for an existing property entry */
    vm_tadsobj_prop *entry = hdr->find_prop_entry(prop);

    /* check for an existing entry for the property */
    vm_val_t oldval;
    if (entry != 0)
    {
        /* found an existing entry - note the old value */
        oldval = entry->val;

        /* store the new value in the existing entry */
        entry->val = *val;
    }
    else
    {
        /* 
         *   We didn't find an existing entry for the property, so we have to
         *   add a new one.  If we don't have any free property slots left,
         *   expand the object to create some more property slots.  
         */
        if (!hdr->has_free_entries(1))
        {
            /* expand the extension to make room for more properties */
            ext_ = (char *)vm_tadsobj_hdr::expand(vmg_ this, hdr);

            /* get the reallocated header */
            hdr = get_hdr();
        }

        /* allocate a new entry */
        entry = hdr->alloc_prop_entry(prop, val, 0);

        /* 
         *   The old value didn't exist, so mark it emtpy, with an intval of
         *   zero.  The zero indicates that this is a newly created property
         *   entry, and thus should be deleted on undo.  
         */
        oldval.set_empty();
        oldval.val.intval = 0;
    }

    /*
     *   If we already have undo for this property for the current
     *   savepoint, as indicated by the undo flag for the property, we don't
     *   need to save undo for this change, since we already have an undo
     *   record in the current savepoint.  Otherwise, we need to add an undo
     *   record for this savepoint.  
     */
    if (undo != 0 && (entry->flags & VMTO_PROP_UNDO) == 0)
    {
        /* save the undo record */
        undo->add_new_record_prop_key(vmg_ self, prop, &oldval);

        /* mark the property as now having undo in this savepoint */
        entry->flags |= VMTO_PROP_UNDO;

        /* 
         *   If the entry wasn't previously marked as modified, remember this
         *   by storing an extra 'empty' undo record with intval 1 after the
         *   record we just saved.  When we see an 'empty' undo value with an
         *   intval of 1, we recognize it as this special marker that tells
         *   us to remove the 'modified' flag from the property.  Note that
         *   we don't need to bother if the old value was already empty,
         *   since that deletes the whole property on undo, making the
         *   'modified' flag irrelevant.  
         */
        if ((entry->flags & VMTO_PROP_MOD) == 0 && oldval.typ != VM_EMPTY)
        {
            /* store an empty undo record with intval 1 */
            oldval.set_empty();
            oldval.val.intval = 1;
            undo->add_new_record_prop_key(vmg_ self, prop, &oldval);
        }
    }

    /* mark the property entry as modified */
    entry->flags |= VMTO_PROP_MOD;

    /* mark the overall object as modified */
    mark_modified(vmg_ undo, self);
}

/* ------------------------------------------------------------------------ */
/*
 *   Mark the object as modified 
 */
void CVmObjTads::mark_modified(VMG_ CVmUndo *undo, vm_obj_id_t self)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* if we're not already marked as modified, set the flag */
    if ((hdr->intern_obj_flags & VMTO_OBJ_MOD) == 0)
    {
        /* add the modified flag */
        hdr->intern_obj_flags |= VMTO_OBJ_MOD;

        /* add an undo record for it */
        if (undo != 0)
        {
            /* 
             *   property == 'invalid' and value == int(1) is our special
             *   record indicating an unmodified-to-modified transition 
             */
            vm_val_t v;
            v.set_int(1);
            undo->add_new_record_prop_key(vmg_ self, VM_INVALID_PROP, &v);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Build a list of my properties 
 */
void CVmObjTads::build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval)
{
    size_t cnt;
    size_t idx;
    CVmObjList *lst;
    vm_tadsobj_prop *entry;
    vm_tadsobj_hdr *hdr = get_hdr();
    
    /* the next free index is also the number of properties we have */
    cnt = hdr->prop_entry_free;

    /* allocate a list big enough for all of our properties */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, cnt));

    /* get the list object, property cast */
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* add our image file properties to the list */
    for (idx = 0, entry = hdr->prop_entry_arr ; cnt != 0 ;
         --cnt, ++entry)
    {
        /* make a value for this property ID */
        vm_val_t val;
        val.set_propid(entry->prop);

        /* add it to the list */
        lst->cons_set_element(idx++, &val);
    }

    /* set the final length of the list */
    lst->cons_set_len(idx);
}


/* ------------------------------------------------------------------------ */
/*
 *   Call a static method. 
 */
int CVmObjTads::call_stat_prop(VMG_ vm_val_t *result,
                               const uchar **pc_ptr, uint *argc,
                               vm_prop_id_t prop)
{
    int idx;

    /* convert the property to an index in our method vector */
    idx = G_meta_table
          ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* check what property they're evaluating */
    switch(idx)
    {
    case PROPIDX_CREATE_INSTANCE:
    case PROPIDX_CREATE_TRANS_INSTANCE:
        {
            static CVmNativeCodeDesc desc(0);
            
            /* check arguments */
            if (get_prop_check_argc(result, argc, &desc))
                return TRUE;

            /*
             *   They want to create an instance of TadsObject, which is
             *   just a plain base object with no superclass.  Push null as
             *   the base class and call our from-stack constructor.  
             */
            result->set_obj(create_from_stack_intern(
                vmg_ pc_ptr, 0, idx == PROPIDX_CREATE_TRANS_INSTANCE));
        }

        /* handled */
        return TRUE;

    case PROPIDX_CREATE_INSTANCE_OF:
    case PROPIDX_CREATE_TRANS_INSTANCE_OF:
        {
            static CVmNativeCodeDesc desc(0, 0, TRUE);
            uint in_argc = (argc == 0 ? 0 : *argc);

            /* check arguments */
            if (get_prop_check_argc(result, argc, &desc))
                return TRUE;

            /*
             *   They want to create an instance of TadsObject, which is just
             *   a plain base object with no superclass.  Push null as the
             *   base class and call our from-stack constructor.  
             */
            result->set_obj(create_from_stack_multi(
                vmg_ in_argc, idx == PROPIDX_CREATE_TRANS_INSTANCE_OF));
        }

        /* handled */
        return TRUE;
        
    default:
        /* it's not one of ours; inherit the base class statics */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Superclass inheritance search context.  This keeps track of our position
 *   in searching the inheritance tree of a given class.  
 */
struct tadsobj_sc_search_ctx
{
    /* initialize at a given object */
    tadsobj_sc_search_ctx(VMG_ vm_obj_id_t obj, CVmObjTads *objp)
    {
        /* start at the given object */
        cur = obj;
        curhdr = objp->get_hdr();

        /* we have no path yet */
        path_sc = 0;
    }

    /* current object ID */
    vm_obj_id_t cur;

    /* if we're using a multiple-inheritance search path, the path */
    tadsobj_objid_and_ptr *path_sc;

    /* current object header */
    vm_tadsobj_hdr *curhdr;


    /*
     *   Find the given property, searching our superclass list until we find
     *   an object providing the property.  Returns true if found, and fills
     *   in *val and *source.  Returns false if not found.  
     */
    int find_prop(VMG_ uint prop, vm_val_t *val, vm_obj_id_t *source)
    {
        /* keep going until we find the property */
        do
        {
            /* look for this property in the current object */
            vm_tadsobj_prop *entry = curhdr->find_prop_entry(prop);
            if (entry != 0)
            {
                /* we found the property - return it */
                *val = entry->val;
                *source = cur;
                return TRUE;
            }
        }
        while (to_next(vmg0_));

        /* we've exhausted the search path - return failure */
        return FALSE;
    }

    /*  
     *   Move to the next superclass.  This updates 'cur' to refer to the
     *   next object in inheritance order.  Returns true if there is a next
     *   element, false if not.
     *   
     *   It's legal to call this with 'cur' uninitialized, as we don't need
     *   the old value of 'cur' to do our work.  This is important because it
     *   allows a search position to be initialized knowing only an object's
     *   'this' pointer, not its object ID.
     */
    int to_next(VMG0_)
    {
        /* check for a path */
    try_again:
        if (path_sc == 0)
        {
            /* 
             *   we're not working on a path at all - this means we're
             *   working directly on a (so far) single-inheritance superclass
             *   chain, so simply follow the chain up to the next superclass 
             */

            /* we have no path, so look at our object's superclasses */
            switch(curhdr->sc_cnt)
            {
            case 1:
                /* we have exactly one superclass, so traverse to it */
                {
                    const vm_tadsobj_sc *sc = curhdr->sc;
                    cur = sc->id;
                    curhdr = sc->objp->get_hdr();
                }
                return TRUE;

            case 0:
                /* we have no superclasses, so there's nowhere to go */
                return FALSE;

            default:
                /* we have multiple superclasses, so set up a search path */
                path_sc = curhdr->get_inh_search_path(vmg0_);

                /* start over with the search path */
                goto try_again;
            }
        }
        else if ((cur = path_sc->id) != VM_INVALID_OBJ)
        {
            /*
             *   we're working on a path, and we have elements remaining -
             *   move on to the next element 
             */
            curhdr = path_sc->objp->get_hdr();
            ++path_sc;

            /* got it */
            return TRUE;
        }
        else
        {
            /* 
             *   we're working on a path, and we're out of elements - we have
             *   nowhere else to go 
             */
            return FALSE;
        }
    }

    /*
     *   Skip to the given object.  If we find the object in the path, we'll
     *   leave the current position set to the given object and return true;
     *   if we fail to find the object, we'll return false.  
     */
    int skip_to(VMG_ vm_obj_id_t target)
    {
        /* keep going until the current object matches the target */
        while (cur != target)
        {
            /* move to the next element */
            if (!to_next(vmg0_))
            {
                /* there's nothing left - return failure */
                return FALSE;
            }
        }

        /* found it */
        return TRUE;
    }
};

/*
 *   Get the inheritance search path for this object 
 */
tadsobj_objid_and_ptr *vm_tadsobj_hdr::get_inh_search_path(VMG0_)
{
    /* if we have a cached path, return it */
    if (inh_path != 0)
        return inh_path;

    /* get the queue builder global */
    CVmObjTadsInhQueue *q = G_tadsobj_queue;

    /*
     *   We haven't already cached a search path for this object, so build
     *   the search path now and save it for future searches.  Start by
     *   clearing the work queue.  
     */
    q->clear();

    /* we're not yet processing the first element */
    pfq_ele *q_ele = 0;

    /* start with self */
    vm_tadsobj_hdr *curhdr = this;

    /* keep going until we run out of queue elements */
    for (;;)
    {
        /* get the superclass count for this object */
        uint cnt = curhdr->sc_cnt;

        /* insert my superclasses right after me */
        pfq_ele *q_ins = q_ele;

        /* enqueue the current object's superclasses */
        uint i;
        vm_tadsobj_sc *scp;
        for (i = 0, scp = curhdr->sc ; i < cnt ; ++i, ++scp)
        {
            /* get the current superclass */
            vm_obj_id_t sc = scp->id;
            CVmObjTads *scobj = scp->objp;

            /* if it's not a TadsObject, skip it */
            if (scobj->get_metaclass_reg() != CVmObjTads::metaclass_reg_)
                continue;

            /* enqueue this superclass */
            q_ins = q->insert_obj(vmg_ sc, scobj, q_ins);
        }

        /* move to the next valid element */
        do
        {
            /* get the next queue element */
            q_ele = (q_ele == 0 ? q->get_head() : q_ele->nxt);

            /* if we're out of elements, we're done */
            if (q_ele == 0)
                goto done;
        }
        while (q_ele->obj == VM_INVALID_OBJ);

        /* get the object header for the queue item */
        curhdr = q_ele->objp->get_hdr();
    }

done:
    /* create and cache a linearized path for the queue, and return it */
    return inh_path = q->create_path();
}

/*
 *   Search for a property via inheritance, starting after the given defining
 *   object.  
 */
int CVmObjTads::search_for_prop_from(VMG_ uint prop,
                                     vm_val_t *val,
                                     vm_obj_id_t orig_target_obj,
                                     vm_obj_id_t *source_obj,
                                     vm_obj_id_t defining_obj)
{
    /* set up a search position */
    tadsobj_sc_search_ctx curpos(vmg_ orig_target_obj,
                                 (CVmObjTads *)vm_objp(vmg_ orig_target_obj));

    /* if we have a starting point, skip past it */
    if (defining_obj != VM_INVALID_OBJ)
    {
        /* skip until we're at defining_obj */
        if (!curpos.skip_to(vmg_ defining_obj))
            return FALSE;

        /* skip defining_obj itself */
        if (!curpos.to_next(vmg0_))
            return FALSE;
    }

    /* find the property */
    return curpos.find_prop(vmg_ prop, val, source_obj);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a property.  We first look in this object; if we can't find the
 *   property here, we look for it in one of our superclasses.  
 */
int CVmObjTads::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                         vm_obj_id_t self, vm_obj_id_t *source_obj,
                         uint *argc)
{
    /* 
     *   try finding the property in my own direct property list or a
     *   superclass property list 
     */
    tadsobj_sc_search_ctx curpos(vmg_ self, this);
    if (curpos.find_prop(vmg_ prop, val, source_obj))
        return TRUE;

    /* 
     *   we didn't find the property in a property list, so try the
     *   intrinsic class methods
     */
    if (get_prop_intrinsic(vmg_ prop, val, self, source_obj, argc))
        return TRUE;

    /* 
     *   we didn't find the property among our methods, so try inheriting it
     *   from the base metaclass 
     */
    return CVmObject::get_prop(vmg_ prop, val, self, source_obj, argc);
}

/*
 *   Inherit a property.  
 */
int CVmObjTads::inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                         vm_obj_id_t self,
                         vm_obj_id_t orig_target_obj,
                         vm_obj_id_t defining_obj,
                         vm_obj_id_t *source_obj, uint *argc)
{
    /* 
     *   check to see if we're already inheriting from an intrinsic class or
     *   an intrinsic class modifier 
     */
    if (defining_obj == VM_INVALID_OBJ
        || (!CVmObjIntClsMod::is_intcls_mod_obj(vmg_ defining_obj)
            && !CVmObjClass::is_intcls_obj(vmg_ defining_obj)))
    {
        /* 
         *   The previous defining object wasn't itself an intrinsic class or
         *   modifier object, so continue searching for TadsObject
         *   superclasses.  
         */
        if (search_for_prop_from(vmg_ prop, val,
                                 orig_target_obj, source_obj, defining_obj))
            return TRUE;

        /* 
         *   We didn't find the property in a property list.  Since we were
         *   inheriting, we must have originally found it in a property list,
         *   but we've found no more inherited properties.  Next, check the
         *   intrinsic methods of the intrinsic class.  
         */
        if (get_prop_intrinsic(vmg_ prop, val, self, source_obj, argc))
            return TRUE;

        /*
         *   We didn't find it among our TadsObject superclasses or as an
         *   intrinsic method.  There's still one possibility: it could be
         *   defined in an intrinsic class modifier for TadsObject or one of
         *   its intrinsic superclasses (aka supermetaclasses).
         *   
         *   This represents a new starting point in the search.  No longer
         *   are we looking for TadsObject overrides; we're now looking for
         *   modifier objects.  The modifier objects effectively form a
         *   separate class hierarchy alongside the intrinsic class hierarchy
         *   they modify.  Since we're starting a new search in this new
         *   context, forget the previous defining object - it has a
         *   different meaning in the new context, and we want to start the
         *   new search from the beginning.
         *   
         *   Note that if this search does turn up a modifier object, and
         *   that modifier object further inherits, we'll come back through
         *   this method again to find the base class method.  At that point,
         *   however we'll notice that the previous defining object was a
         *   modifier, so we will not go through this branch again - we'll go
         *   directly to the base metaclass and continue the inheritance
         *   search there.  
         */
        defining_obj = VM_INVALID_OBJ;
    }

    /* continue searching via our base metaclass */
    return CVmObject::inh_prop(vmg_ prop, val, self, orig_target_obj,
                               defining_obj, source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a property from the intrinsic class. 
 */
int CVmObjTads::get_prop_intrinsic(VMG_ vm_prop_id_t prop, vm_val_t *val,
                                   vm_obj_id_t self, vm_obj_id_t *source_obj,
                                   uint *argc)
{
    uint func_idx;
    
    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function in our function vector */
    if ((this->*func_table_[func_idx])(vmg_ self, val, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* didn't find it */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Enumerate properties 
 */
void CVmObjTads::enum_props(VMG_ vm_obj_id_t self,
                            void (*cb)(VMG_ void *ctx, vm_obj_id_t self,
                                       vm_prop_id_t prop,
                                       const vm_val_t *val),
                            void *cbctx)
{
    size_t i;
    size_t sc_cnt;
    vm_tadsobj_prop *entry;
    vm_tadsobj_hdr *hdr = get_hdr();

    /* run through our properties */
    for (i = hdr->prop_entry_free, entry = hdr->prop_entry_arr ;
         i != 0 ; --i, ++entry)
    {
        /* invoke the callback */
        (*cb)(vmg_ cbctx, self, entry->prop, &entry->val);
    }

    /* enumerate properties in each superclass */
    sc_cnt = get_sc_count();
    for (i = 0 ; i < sc_cnt ; ++i)
    {
        vm_obj_id_t sc;

        /* get this superclass */
        sc = get_sc(i);

        /* enumerate its properties */
        vm_objp(vmg_ sc)->enum_props(vmg_ sc, cb, cbctx);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Determine if I'm an instance of the given object 
 */
int CVmObjTads::is_instance_of(VMG_ vm_obj_id_t obj)
{
    /* 
     *   Set up a superclass search position.  Since the first thing we'll
     *   do is call 'to_next', and since 'to_next' doesn't require a valid
     *   current object ID (only a valid 'this' pointer), we don't need to
     *   know our own object ID - simply set the initial object ID to the
     *   invalid ID.  
     */
    tadsobj_sc_search_ctx curpos(vmg_ VM_INVALID_OBJ, this);
    
    /* 
     *   scan through the search list, comparing each superclass to the
     *   object of interest; if we find it among our superclasses, we're an
     *   instance of the given object 
     */
    for (;;)
    {
        /* skip to the next object */
        if (!curpos.to_next(vmg0_))
        {
            /* we've run out of superclasses without finding it */
            break;
        }
        
        /* 
         *   if the current superclass is the object we're looking for, then
         *   we're an instance of that object 
         */
        if (curpos.cur == obj)
            return TRUE;
    }

    /* 
     *   None of our superclasses match the given object, and none of the
     *   superclasses derive from the given object, so we must not derive
     *   from the given object.  Our last recourse is to determine if the
     *   object represents our metaclass; inherit the default handling to
     *   make this check.  
     */
    return CVmObject::is_instance_of(vmg_ obj);
}

/* ------------------------------------------------------------------------ */
/*
 *   Apply undo 
 */
void CVmObjTads::apply_undo(VMG_ CVmUndoRecord *rec)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* 
     *   if the property is valid, it's a simple property change record;
     *   otherwise it's some other object-level change 
     */
    if (rec->id.prop == VM_INVALID_PROP)
    {
        /*
         *   Invalid property ID, so it's an object-level change:
         *   
         *   - int(1) -> object is newly modified (VMTO_OBJ_MOD flag was
         *   newly set)
         *   
         *   - list -> superclass list change
         */
        switch (rec->oldval.typ)
        {
        case VM_INT:
            switch (rec->oldval.val.intval)
            {
            case 1:
                /* VMTO_OBJ_MOD flag newly set - clear the flag */
                hdr->intern_obj_flags &= ~VMTO_OBJ_MOD;
                break;
            }
            break;

        default:
            /* check for a list, which indicates a superclass change */
            if (rec->oldval.is_listlike(vmg0_))
            {
                /* set the new superclass list */
                change_superclass_list(
                    vmg_ &rec->oldval, rec->oldval.ll_length(vmg0_));
            }
            break;
        }
    }
    else
    {
        /* find the property entry for the property being undone */
        vm_tadsobj_prop *entry = hdr->find_prop_entry(rec->id.prop);
        if (entry == 0)
        {
            /* can't find the property - something is out of whack */
            assert(FALSE);
            return;
        }
        
        /* restore the value from the record */
        entry->val = rec->oldval;
        
        /* if the old value is 'empty', it requires special handling */
        if (rec->oldval.typ == VM_EMPTY)
        {
            vm_tadsobj_prop *cur, **prv;
            unsigned int hash = hdr->calc_hash(rec->id.prop);
            
            /* 
             *   We use 'empty' records for multiple purposes, with the
             *   specific one indicated by the intval field. 
             */
            switch (rec->oldval.val.intval)
            {
            case 0:
                /*
                 *   Empty with intval 0 indicates a property addition, which
                 *   we undo by deleting the property.  First, find it in the
                 *   hash chain.  
                 */
                for (prv = &hdr->hash_arr[hash] ;
                     (cur = *prv) != 0 && cur != entry ;
                     prv = &cur->nxt) ;
                
                /* make sure we found it */
                if (cur == entry)
                {
                    /* unlink it */
                    *prv = entry->nxt;
                    
                    /* return it to the free list */
                    hdr->prop_entry_free -= 1;
                    assert(entry == &hdr->prop_entry_arr[hdr->prop_entry_free]);
                }
                else
                {
                    /* this should be impossible */
                    assert(FALSE);
                }
                break;
                
            case 1:
                /*
                 *   Empty with intval 1 indicates that we marked the slot as
                 *   newly modified, which we undo by clearing the modified
                 *   flag.
                 */
                entry->flags &= ~VMTO_PROP_MOD;
                break;
            }
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Mark as referenced all of the objects to which we refer 
 */
void CVmObjTads::mark_refs(VMG_ uint state)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();
    
    /* 
     *   Go through all of our property slots and mark each object value.
     *   Note that we only need to worry about the modified properties;
     *   everything referenced in the load image list is necessarily part of
     *   the root set, or it couldn't have been in the load image, so we
     *   don't need to bother marking any of those objects, since they can
     *   never be deleted by virtue of being in the root set.  
     */
    vm_tadsobj_prop *entry = hdr->prop_entry_arr;
    vm_tadsobj_prop *first_free = entry + hdr->prop_entry_free;
    for ( ; entry != first_free ; ++entry)
    {
        /* 
         *   if the slot is marked as modified and contains an object
         *   reference, mark the reference 
         */
        if ((entry->val.typ == VM_OBJ || entry->val.typ == VM_OBJX)
            && (entry->flags & VMTO_PROP_MOD) != 0)
        {
            /* mark the reference */
            G_obj_table->mark_all_refs(entry->val.val.obj, state);
        }
    }

    /* mark our superclasses as referenced */
    vm_tadsobj_sc *scp = hdr->sc, *sclast = scp + hdr->sc_cnt;
    for ( ; scp != sclast ; ++scp)
        G_obj_table->mark_all_refs(scp->id, state);
}


/* ------------------------------------------------------------------------ */
/*
 *   Mark a reference in an undo record 
 */
void CVmObjTads::mark_undo_ref(VMG_ CVmUndoRecord *undo)
{
    /* if the undo record refers to an object, mark the object */
    if (undo->oldval.typ == VM_OBJ || undo->oldval.typ == VM_OBJX)
        G_obj_table->mark_all_refs(undo->oldval.val.obj, VMOBJ_REACHABLE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Determine if the object has been changed since it was loaded from the
 *   image file.  If the object has no properties stored in the modified
 *   properties table, it is in exactly the same state as is stored in the
 *   image file.  
 */
int CVmObjTads::is_changed_since_load() const
{
    /* return our 'modified' flag */
    return ((get_hdr()->intern_obj_flags & VMTO_OBJ_MOD) != 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Save the object's state to a file.  We only need to save the modified
 *   property list, because the load image list never changes.  
 */
void CVmObjTads::save_to_file(VMG_ CVmFile *fp)
{
    size_t i;
    vm_tadsobj_prop *entry;
    uint cnt;
    vm_tadsobj_hdr *hdr = get_hdr();

    /* count the number of properties that have actually been modified */
    for (cnt = 0, i = hdr->prop_entry_free, entry = hdr->prop_entry_arr ;
         i != 0 ; --i, ++entry)
    {
        /* if the slot is modified, count it */
        if ((entry->flags & VMTO_PROP_MOD) != 0)
            ++cnt;
    }

    /* write the number of modified properties */
    fp->write_uint2(cnt);

    /* write the number of superclasses */
    fp->write_uint2(get_sc_count());

    /* write the superclasses */
    for (i = 0 ; i < get_sc_count() ; ++i)
        fp->write_uint4(get_sc(i));

    /* write each modified property */
    for (cnt = 0, i = hdr->prop_entry_free, entry = hdr->prop_entry_arr ;
         i != 0 ; --i, ++entry)
    {
        /* if the slot is modified, write it out */
        if ((entry->flags & VMTO_PROP_MOD) != 0)
        {
            char slot[16];

            /* prepare the slot data */
            oswp2(slot, entry->prop);
            vmb_put_dh(slot + 2, &entry->val);

            /* write the slot */
            fp->write_bytes(slot, 2 + VMB_DATAHOLDER);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Restore the object from a file 
 */
void CVmObjTads::restore_from_file(VMG_ vm_obj_id_t self,
                                   CVmFile *fp, CVmObjFixup *fixups)
{
    /* read number of modified properties */
    ushort mod_count = (ushort)fp->read_uint2();

    /* read the number of superclasses */
    ushort sc_cnt = (ushort)fp->read_uint2();

    /* 
     *   If we don't have an extension yet, allocate one.  The only way we
     *   won't have an extension is if we weren't loaded from the image
     *   file, since we always create the extension upon construction when
     *   loading from an image file.  
     */
    if (ext_ == 0)
    {
        /* allocate our extension */
        ext_ = (char *)vm_tadsobj_hdr::alloc(vmg_ this, sc_cnt, mod_count);
    }
    else
    {
        /* 
         *   We already have an extension, so we must have come from the
         *   image file.  Make sure we have enough memory to hold this many
         *   properties, and make sure we have space for the superclasses.
         */
        vm_tadsobj_hdr *hdr = get_hdr();
        if (!hdr->has_free_entries(mod_count) || sc_cnt > hdr->sc_cnt)
        {
            /* 
             *   we need to expand the header to accomodate the modified
             *   properties and/or the modified superclass list 
             */
            ext_ = (char *)vm_tadsobj_hdr::expand_to(
                vmg_ this, hdr, sc_cnt, hdr->prop_entry_cnt + mod_count);
        }
    }

    /* get the extension header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* read the superclass list */
    hdr->sc_cnt = sc_cnt;
    for (ushort i = 0 ; i < sc_cnt ; ++i)
    {
        /* read the next superclass */
        vm_obj_id_t sc = (vm_obj_id_t)fp->read_uint4();

        /* fix it up to the new (post-restore) memory numbering system */
        sc = fixups->get_new_id(vmg_ sc);

        /* 
         *   store it - as when loading from the image file, we can't count
         *   on the superclass having been loaded yet, so we can only store
         *   the superclass's ID, not its actual object pointer 
         */
        hdr->sc[i].id = sc;
        hdr->sc[i].objp = 0;
    }

    /* request post-load initialization, to set up the superclass list */
    G_obj_table->request_post_load_init(self);

    /* 
     *   invalidate any existing inheritance path, in case the superclass
     *   list changed 
     */
    hdr->inval_inh_path();

    /* read the modified properties */
    for (ushort i = 0 ; i < mod_count ; ++i)
    {
        /* read the next slot */
        char buf[32];
        fp->read_bytes(buf, 2 + VMB_DATAHOLDER);

        /* fix up this entry */
        fixups->fix_dh(vmg_ buf + 2);

        /* decode the entry */
        vm_prop_id_t prop = (vm_prop_id_t)osrp2(buf);
        vm_val_t val;
        vmb_get_dh(buf + 2, &val);

        /* 
         *   store the entry (don't save any undo for the operation, as we
         *   can't undo a load) 
         */
        set_prop(vmg_ 0, self, prop, &val);
    }

    /* clear all undo information */
    clear_undo_flags();

    /*
     *   If we were saved to a file in the first place, it's because we were
     *   modified or newly created relative to the image file data.  So on
     *   restore, we're necessarily a modified object that will need to be
     *   saved again, even if we're not modified between now and the next
     *   save.
     */
    get_hdr()->intern_obj_flags |= VMTO_OBJ_MOD;
}

/* ------------------------------------------------------------------------ */
/*
 *   Post-load initialization: cache the object pointers for our
 *   superclasses. 
 */
void CVmObjTads::post_load_init(VMG_ vm_obj_id_t self)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* cache the superclass object pointers */
    for (int i = 0 ; i < hdr->sc_cnt ; ++i)
        hdr->sc[i].objp = (CVmObjTads *)vm_objp(vmg_ hdr->sc[i].id);
}

/* ------------------------------------------------------------------------ */
/*
 *   Load the object from an image file
 */
void CVmObjTads::load_from_image(VMG_ vm_obj_id_t self,
                                 const char *ptr, size_t siz)
{
    /* save our image data pointer for reloading */
    G_obj_table->save_image_pointer(self, ptr, siz);

    /* if we already have memory allocated, free it */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* get the number of superclasses */
    ushort sc_cnt = osrp2(ptr);

    /* get the number of load image properties */
    ushort li_cnt = osrp2(ptr + 2);

    /* allocate our header */
    ext_ = (char *)vm_tadsobj_hdr::alloc(vmg_ this, sc_cnt, li_cnt);
    vm_tadsobj_hdr *hdr = get_hdr();

    /* read the object flags from the image file and store them */
    hdr->li_obj_flags = osrp2(ptr + 4);

    /* 
     *   set our internal flags - we come from the load image file, and we're
     *   not yet modified from the load image data
     */
    hdr->intern_obj_flags |= VMTO_OBJ_IMAGE;
    hdr->intern_obj_flags &= ~VMTO_OBJ_MOD;

    /* load the image file properties */
    load_image_props_and_scs(vmg_ ptr, siz);

    /* request post-load initialization, to set up the superclass list */
    G_obj_table->request_post_load_init(self);
}

/*
 *   Reset to image file state.  Discards all modified properties, so that
 *   we have only the image file properties.
 */
void CVmObjTads::reload_from_image(VMG_ vm_obj_id_t self,
                                   const char *ptr, size_t siz)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* get the number of superclasses */
    ushort sc_cnt = osrp2(ptr);

    /* 
     *   Clear the property table.  We don't have to worry about the new
     *   property table being larger than the existing property table,
     *   because we can't have shrunk since we were originally loaded.  So,
     *   all we need to do is mark all property entries as free and clear
     *   out the hash table.  
     */
    hdr->prop_entry_free = 0;
    memset(hdr->hash_arr, 0, hdr->hash_siz * sizeof(hdr->hash_arr[0]));

    /* if we need space for more superclasses, reallocate the header */
    if (sc_cnt > hdr->sc_cnt)
    {
        /* allocate the new header */
        ext_ = (char *)vm_tadsobj_hdr::expand_to(
            vmg_ this, hdr, sc_cnt, hdr->prop_entry_cnt);
    }

    /* reload the image properties and superclasses */
    load_image_props_and_scs(vmg_ ptr, siz);

    /* we're now unmodified from the image file state */
    hdr->intern_obj_flags &= ~VMTO_OBJ_MOD;

    /* request post-load initialization, to set up the superclass list */
    G_obj_table->request_post_load_init(self);
}

/*
 *   Load the property list from the image data 
 */
void CVmObjTads::load_image_props_and_scs(VMG_ const char *ptr, size_t siz)
{
    /* get my header */
    vm_tadsobj_hdr *hdr = get_hdr();

    /* get the number of superclasses */
    ushort sc_cnt = osrp2(ptr);

    /* get the number of load image properties */
    ushort li_cnt = osrp2(ptr + 2);

    /* read the superclasses from the load image and store them */
    ushort i;
    const char *p;
    for (i = 0, p = ptr + 6 ; i < sc_cnt ; ++i, p += 4)
    {
        /* store the object ID */
        hdr->sc[i].id = (vm_obj_id_t)t3rp4u(p);

        /* 
         *   We can't store the superclass pointer yet, as the superclass
         *   object might not be loaded yet.  Store null for now; we'll fix
         *   this up in post_load_init() after all the other objects have
         *   been loaded.  
         */
        hdr->sc[i].objp = 0;
    }

    /* read the properties from the load image and store them */
    for (i = 0 ; i < li_cnt ; ++i, p += 2 + VMB_DATAHOLDER)
    {
        /* decode the property data */
        vm_prop_id_t prop = (vm_prop_id_t)osrp2(p);
        vm_val_t val;
        vmb_get_dh(p + 2, &val);

        /* store the property */
        hdr->alloc_prop_entry(prop, &val, 0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - createInstance 
 */
int CVmObjTads::getp_create_instance(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *in_argc)
{
    /* create a persistent instance */
    return getp_create_common(vmg_ self, retval, in_argc, FALSE);
}

/*
 *   Property evaluator - createTransientInstance 
 */
int CVmObjTads::getp_create_trans_instance(VMG_ vm_obj_id_t self,
                                           vm_val_t *retval, uint *in_argc)
{
    /* create a transient instance */
    return getp_create_common(vmg_ self, retval, in_argc, TRUE);
}

/*
 *   Common handler for createInstance() and createTransientInstance() 
 */
int CVmObjTads::getp_create_common(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *in_argc,
                                   int is_transient)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(0, 0, TRUE);

    /* check arguments - any number are allowed */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;
    
    /* 
     *   push myself as the first argument - 'self' is the superclass of the
     *   object to be created 
     */
    G_interpreter->push_obj(vmg_ self);

    /* 
     *   Create an instance - this will recursively execute the new object's
     *   constructor, if it has one.  Note that we have one more argument
     *   than provided by the caller, because we've pushed the implicit
     *   argument ('self') that create_from_stack uses to identify the
     *   superclass.  
     */
    retval->set_obj(create_from_stack_intern(vmg_ 0, argc + 1,
                                             is_transient));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - createClone
 */
int CVmObjTads::getp_create_clone(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);
    vm_obj_id_t new_obj;
    CVmObjTads *tobj;
    vm_tadsobj_prop *entry;
    ushort i;
    vm_tadsobj_hdr *hdr = get_hdr();

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   create a new object with the same number of superclasses as I have,
     *   and with space for all of my properties 
     */
    new_obj = create(vmg_ FALSE, get_sc_count(), hdr->prop_entry_free);
    tobj = (CVmObjTads *)vm_objp(vmg_ new_obj);

    /* copy my superclass list to the new object */
    for (i = 0 ; i < get_sc_count() ; ++i)
        tobj->set_sc(vmg_ i, get_sc(i));

    /* copy my properties to the new object */
    for (i = hdr->prop_entry_free, entry = hdr->prop_entry_arr ;
         i != 0 ; --i, ++entry)
    {
        /* 
         *   Store the property in the new object.  We don't need to store
         *   undo for the property, as the object is entirely new since the
         *   last savepoint (as there can't have been a savepoint while we've
         *   been working, obviously).
         */
        tobj->set_prop(vmg_ 0, self, entry->prop, &entry->val);
    }

    /* the return value is the new object ID */
    retval->set_obj(new_obj);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - createInstanceOf
 */
int CVmObjTads::getp_create_instance_of(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *in_argc)
{
    /* create a persistent instance */
    return getp_create_multi_common(vmg_ self, retval, in_argc, FALSE);
}

/*
 *   Property evaluator - createTransientInstanceOf 
 */
int CVmObjTads::getp_create_trans_instance_of(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *in_argc)
{
    /* create a persistent instance */
    return getp_create_multi_common(vmg_ self, retval, in_argc, TRUE);
}

/*
 *   Common handler for createInstanceOf() and createTransientInstanceOf() 
 */
int CVmObjTads::getp_create_multi_common(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *in_argc,
                                         int is_transient)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(0, 0, TRUE);

    /* check arguments - any number are allowed */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* create the new instance */
    retval->set_obj(create_from_stack_multi(vmg_ argc, is_transient));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Iteration callback for getp_set_sc_list.  On changing an object's
 *   superclass list, we have to rebuild the inheritance path cache for any
 *   object that has the modified object anywhere in its path.  
 */
struct set_sc_cb_ctx
{
    set_sc_cb_ctx(vm_obj_id_t obj) { this->obj = obj; }
    vm_obj_id_t obj;
};

void CVmObjTads::set_sc_cb(VMG_ vm_obj_id_t obj, void *ctx0)
{
    /* if this is a TadsObject instance, update it */
    if (CVmObjTads::is_tadsobj_obj(vmg_ obj))
    {
        /* cast the context to our private structure */
        set_sc_cb_ctx *ctx = (set_sc_cb_ctx *)ctx0;

        /* get this object's header */
        vm_tadsobj_hdr *hdr = ((CVmObjTads *)vm_objp(vmg_ obj))->get_hdr();
        
        /* 
         *   if it has a cached inheritance path, check to see if it contains
         *   the object being changed 
         */
        tadsobj_objid_and_ptr *path = hdr->inh_path;
        if (path != 0)
        {
            /* scan the path for 'obj' */
            for ( ; path->id != VM_INVALID_OBJ ; ++path)
            {
                /* if this superclass is 'obj', we must delete the path */
                if (path->id == ctx->obj)
                {
                    /* we need to drop this path */
                    hdr->inval_inh_path();
                    
                    /* no need to look any further */
                    break;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - setSuperclassList 
 */
int CVmObjTads::getp_set_sc_list(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *in_argc)
{
    static CVmNativeCodeDesc desc(1);
    int cnt;
    int i;
    vm_val_t ele;
    int sc_cnt;
    vm_tadsobj_hdr *hdr = get_hdr();
    vm_val_t *lst;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the list argument (but leave it on the stack for now) */
    lst = G_stk->get(0);
    if (!lst->is_listlike(vmg0_) || (cnt = lst->ll_length(vmg0_)) < 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* we need at least one argument - the minimal root is TadsObject */
    if (cnt < 1)
        err_throw(VMERR_BAD_VAL_BIF);

    /*
     *   Check for a special case: our entire superclass list consists of
     *   [TadsObject].  In this case, we have nothing in our internal
     *   superclass list, since our only superclass is our metaclass.  
     */
    lst->ll_index(vmg_ &ele, 1);
    if (cnt == 1
        && ele.typ == VM_OBJ
        && ele.val.obj == metaclass_reg_->get_class_obj(vmg0_))
    {
        /* use an empty internal superclass list */
        sc_cnt = 0;
    }
    else
    {
        /* 
         *   Scan the superclasses.  Each superclass must be a TadsObject,
         *   with the one exception that if we have only one superclass, it
         *   can be the TadsObject intrinsic class itself, signifying that we
         *   have no superclasses.  
         */
        for (i = 1 ; i <= cnt ; ++i)
        {
            /* get this element from the list */
            lst->ll_index(vmg_ &ele, i);

            /* it has to be an object of type TadsObject */
            if (ele.typ != VM_OBJ || !is_tadsobj_obj(vmg_ ele.val.obj))
                err_throw(VMERR_BAD_VAL_BIF);

            /* 
             *   make sure that this superclass doesn't inherit from 'self' -
             *   if it does, that would create a circular inheritance
             *   hierarchy, which is illegal 
             */
            if (vm_objp(vmg_ ele.val.obj)->is_instance_of(vmg_ self))
                err_throw(VMERR_BAD_VAL_BIF);
        }

        /* the list is valid - we need one superclass per list element */
        sc_cnt = cnt;
    }

    /* mark the overall object as modified if it's not already */
    mark_modified(vmg_ G_undo, self);

    /* if there's a system undo object, add undo for the change */
    if (G_undo != 0)
    {
        /* allocate a list for the results */
        vm_val_t oldv;
        oldv.set_obj(CVmObjList::create(vmg_ FALSE, hdr->sc_cnt));
        CVmObjList *oldp = (CVmObjList *)vm_objp(vmg_ oldv.val.obj);

        /* build the superclass list */
        for (i = 0 ; i < hdr->sc_cnt ; ++i)
        {
            /* add this superclass to the list */
            ele.set_obj(hdr->sc[i].id);
            oldp->cons_set_element(i, &ele);
        }

        /* 
         *   Add an undo record with the original superclass list as the old
         *   value.  Use the 'invalid' property as the property key; when
         *   this property is used with a list value, it indicates a
         *   superclass list change.
         */
        G_undo->add_new_record_prop_key(vmg_ self, VM_INVALID_PROP, &oldv);
    }

    /* update the superclass list with the given list */
    change_superclass_list(vmg_ lst, sc_cnt);

    /* discard arguments */
    G_stk->discard();

    /* no return value */
    retval->set_nil();

    /* we need to clear all cached superclass path lists involving 'self' */
    set_sc_cb_ctx ctx(self);
    G_obj_table->for_each(vmg_ &set_sc_cb, &ctx);

    /* handled */
    return TRUE;
}

/*
 *   Change the superclass list to the given list.  'lstp' is the new
 *   superclass list, in constant list format (i.e., a packed array of
 *   dataholder values).  
 */
void CVmObjTads::change_superclass_list(VMG_ const vm_val_t *lst, int cnt)
{
    vm_tadsobj_hdr *hdr = get_hdr();
    int i;

    /* keep the count within range */
    cnt = (cnt > UINT16MAXVAL ? UINT16MAXVAL : cnt < 0 ? 0 : cnt);
    
    /* 
     *   if we're increasing the number of superclasses, expand our object
     *   header to make room 
     */
    if (cnt > (int)hdr->sc_cnt)
    {
        /* expand the header to accomodate the new superclass list */
        ext_ = (char *)vm_tadsobj_hdr::expand_to(
            vmg_ this, hdr, cnt, hdr->prop_entry_cnt);

        /* get the new header */
        hdr = get_hdr();
    }

    /* set the new superclass count */
    hdr->sc_cnt = (ushort)cnt;

    /* set the new superclasses */
    for (i = 0 ; i < cnt ; ++i)
    {
        vm_val_t ele;

        /* get this element from the list */
        lst->ll_index(vmg_ &ele, i+1);

        /* set this superclass in the header */
        hdr->sc[i].id = ele.val.obj;
        hdr->sc[i].objp = (CVmObjTads *)vm_objp(vmg_ ele.val.obj);
    }

    /* invalidate the cached inheritance path */
    hdr->inval_inh_path();
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a method pointer.  This is almost like an ordinary getprop, but if
 *   the property contains a method, returns a function pointer to the method
 *   rather than evaluating it.  
 */
int CVmObjTads::getp_get_method(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(1);

    /* check arguments: getMethod(&propid) */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve the property ID */
    vm_obj_id_t prop = CVmBif::pop_propid_val(vmg0_);

    /* find the property */
    vm_obj_id_t source_obj;
    tadsobj_sc_search_ctx curpos(vmg_ self, this);
    if (curpos.find_prop(vmg_ prop, retval, &source_obj))
    {
        /* the property is definfed - check its type */
        switch (retval->typ)
        {
        case VM_CODEOFS:
            /* it's a direct call to code - convert to a function pointer */
            retval->typ = VM_FUNCPTR;
            break;

        case VM_DSTRING:
            /* it's a self-printing constant string - convert to string */
            retval->typ = VM_SSTRING;
            break;

        case VM_OBJX:
            /* 
             *   It's an executable object, which is either a self-printing
             *   non-constant string or an execute-on-invoke anonymous
             *   function.  In either case, simply convert it back to an
             *   ordinary value. 
             */
            retval->typ = VM_OBJ;
            break;

        case VM_BIFPTRX:
            /* 
             *   executable built-in function pointer - convert back to a
             *   regular bif pointer 
             */
            retval->typ = VM_BIFPTR;
            break;

        default:
            /* other types aren't executable methods, so return nil */
            retval->set_nil();
            break;
        }
    }
    else
    {
        /* this property isn't defined - return nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/*
 *   Set a method pointer.  This is almost like an ordinary setprop, but if
 *   the value to assign contains a function pointer, this sets the property
 *   to a method rather than to a pointer-to-function value. 
 */
int CVmObjTads::getp_set_method(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc)
{
    /* check arguments: setMethod(&propid, val) */
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   retrieve the property ID and the method value to set (leave the
     *   value on the stack as gc protection) 
     */
    vm_prop_id_t prop = CVmBif::pop_propid_val(vmg0_);
    vm_val_t mval = *G_stk->get(0);

    /* check the type */
    switch (mval.typ)
    {
    case VM_FUNCPTR:
        /* 
         *   a function pointer translates directly to a regular method, by
         *   directly calling the code body 
         */
        mval.typ = VM_CODEOFS;
        break;

    case VM_OBJ:
        /*
         *   We can accept string and invokable objects.  For either one of
         *   these, we store the object reference and mark it as an
         *   execute-on-eval object rather than an ordinary object value.  
         */
        if (CVmObjString::is_string_obj(vmg_ mval.val.obj)
            || vm_objp(vmg_ mval.val.obj)->get_invoker(vmg_ 0))
        {
            /* 
             *   We can handle this object by executing it on evaluation.
             *   Mark it as a special execute-on-eval object rather than an
             *   ordinary object. 
             */
            mval.typ = VM_OBJX;
        }
        else
        {
            /* it's not an acceptable type of object */
            err_throw(VMERR_BAD_TYPE_BIF);
        }
        break;

    case VM_BIFPTR:
        /* built-in function pointer - mark it as an execute-on-eval bif */
        mval.typ = VM_BIFPTRX;
        break;

    case VM_SSTRING:
        /* 
         *   A constant string translates into a self-printing string.
         *   SSTRING and DSTRING have the same data representation, so we
         *   merely need to switch the type to flag it as print-on-eval. 
         */
        mval.typ = VM_DSTRING;
        break;

    default:
        /* other types are not acceptable */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* set the property value */
    set_prop(vmg_ G_undo, self, prop, &mval);

    /* discard the method value */
    G_stk->discard();

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Intrinsic Class Modifier object implementation 
 */

/* metaclass registration object */
static CVmMetaclassIntClsMod metaclass_reg_obj_icm;
CVmMetaclass *CVmObjIntClsMod::metaclass_reg_ = &metaclass_reg_obj_icm;

/*
 *   Get a property.  Intrinsic class modifiers do not have intrinsic
 *   superclasses, because they're effectively mix-in classes.  Therefore,
 *   do not look for intrinsic properties or intrinsic superclass properties
 *   to resolve the property lookup.  
 */
int CVmObjIntClsMod::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                              vm_obj_id_t self, vm_obj_id_t *source_obj,
                              uint *argc)
{
    /* 
     *   try finding the property in our property list or a superclass
     *   property list 
     */
    tadsobj_sc_search_ctx curpos(vmg_ self, this);
    if (curpos.find_prop(vmg_ prop, val, source_obj))
        return TRUE;

    /*
     *   We didn't find it in our list, so we don't have the property.
     *   Because we're an intrinsic mix-in, we don't look for an intrinsic
     *   implementation or an intrinsic superclass implementation.
     */
    return FALSE;
}

/*
 *   Inherit a property.  As with get_prop(), we don't want to inherit from
 *   any intrinsic superclass if we don't find the property in our property
 *   list or an inherited property list.  
 */
int CVmObjIntClsMod::inh_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                              vm_obj_id_t self,
                              vm_obj_id_t orig_target_obj,
                              vm_obj_id_t defining_obj,
                              vm_obj_id_t *source_obj, uint *argc)
{
    /* 
     *   try finding the property in our property list or a superclass
     *   property list 
     */
    if (search_for_prop_from(vmg_ prop, val, orig_target_obj,
                             source_obj, defining_obj))
        return TRUE;

    /* 
     *   we didn't find it in our list, and we don't want to inherit from any
     *   intrinsic superclass, so we don't have the property 
     */
    return FALSE;
}

/* 
 *   Build my property list.  We build the complete list of methods defined
 *   in the intrinsic class modifier for all classes, including any modify
 *   base classes that we further modify.  
 */
void CVmObjIntClsMod::build_prop_list(VMG_ vm_obj_id_t self, vm_val_t *retval)
{
    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* build our own list */
    CVmObjTads::build_prop_list(vmg_ self, retval);

    /* if we have a base class that we further modify, add its list */
    if (get_sc_count() != 0)
    {
        vm_obj_id_t base_id;
        CVmObject *base_obj;

        /* get the base class */
        base_id = get_sc(0);
        base_obj = vm_objp(vmg_ base_id);

        /* get its list only if it's of our same metaclass */
        if (base_obj->get_metaclass_reg() == get_metaclass_reg())
        {
            vm_val_t base_val;

            /* save our list for gc protection */
            G_stk->push(retval);

            /* get our base class's list */
            base_obj->build_prop_list(vmg_ base_id, &base_val);

            /* add this list to our result list */
            vm_objp(vmg_ retval->val.obj)->
                add_val(vmg_ retval, retval->val.obj, &base_val);

            /* discard our gc protection */
            G_stk->discard();
        }
    }

    /* discard gc protection */
    G_stk->discard();
}

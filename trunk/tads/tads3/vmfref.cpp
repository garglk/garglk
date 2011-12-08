#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmfref.cpp - stack frame reference object
Function
  
Notes
  
Modified
  02/18/10 MJRoberts  - Creation
*/

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmfref.h"
#include "vmglob.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmundo.h"
#include "vmlst.h"
#include "vmfile.h"
#include "vmstr.h"
#include "vmvec.h"
#include "vmlookup.h"
#include "vmfunc.h"
#include "vmerr.h"
#include "vmerrnum.h"


/* ------------------------------------------------------------------------ */
/*
 *   Stack frame descriptor 
 */


/*
 *   statics 
 */
static CVmMetaclassFrameDesc desc_metaclass_reg_obj;
CVmMetaclass *CVmObjFrameDesc::metaclass_reg_ = &desc_metaclass_reg_obj;

/* 
 *   function table 
 */
int (CVmObjFrameDesc::
    *CVmObjFrameDesc::func_table_[])
    (VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjFrameDesc::getp_undef,
    &CVmObjFrameDesc::getp_is_active,
    &CVmObjFrameDesc::getp_get_vars,
    &CVmObjFrameDesc::getp_get_self,
    &CVmObjFrameDesc::getp_get_defobj,
    &CVmObjFrameDesc::getp_get_targobj,
    &CVmObjFrameDesc::getp_get_targprop,
    &CVmObjFrameDesc::getp_get_invokee,
};

/*
 *   construction 
 */
CVmObjFrameDesc::CVmObjFrameDesc(VMG_ vm_obj_id_t fref,
                                 int frame_idx, uint ret_ofs)
{
    /* allocate our extension */
    ext_ = 0;
    alloc_ext(vmg_ fref, frame_idx, ret_ofs);
}

/*
 *   create 
 */
vm_obj_id_t CVmObjFrameDesc::create(
    VMG_ vm_obj_id_t fref, int frame_idx, uint ret_ofs)
{
    /* allocate an object ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* instantiate the new object */
    new (vmg_ id) CVmObjFrameDesc(vmg_ fref, frame_idx, ret_ofs);

    /* return the new object's ID */
    return id;
}

/*
 *   allocate our extension 
 */
vm_framedesc_ext *CVmObjFrameDesc::alloc_ext(
    VMG_ vm_obj_id_t fref, int frame_idx, uint ret_ofs)
{
    /* delete any existing extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* allocate the extension */
    vm_framedesc_ext *ext =
        (vm_framedesc_ext *)G_mem->get_var_heap()->alloc_mem(
            sizeof(vm_framedesc_ext), this);

    /* save the data values */
    ext->fref = fref;
    ext->frame_idx = frame_idx;
    ext->ret_ofs = ret_ofs;

    /* save the new extension in 'self' */
    ext_ = (char *)ext;

    /* return the new extension */
    return ext;
}

/*
 *   notify of deletion 
 */
void CVmObjFrameDesc::notify_delete(VMG_ int)
{
    /* free our extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/*
 *   mark references
 */
void CVmObjFrameDesc::mark_refs(VMG_ uint state)
{
    /* get the extension */
    vm_framedesc_ext *ext = get_ext();

    /* mark our frame ref object */
    G_obj_table->mark_all_refs(ext->fref, state);
}


/*
 *   load from the image file 
 */
void CVmObjFrameDesc::load_from_image(VMG_ vm_obj_id_t self,
                                      const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image 
 */
void CVmObjFrameDesc::reload_from_image(VMG_ vm_obj_id_t self,
                                        const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);
}

/*
 *   load or reload our image data 
 */
void CVmObjFrameDesc::load_image_data(VMG_ vm_obj_id_t self,
                                      const char *ptr, size_t siz)
{
    /* read the frame information */
    vm_obj_id_t fref = vmb_get_objid(ptr);
    int frame_idx = osrp2(ptr + VMB_OBJECT_ID);
    uint ret_ofs = osrp2(ptr + VMB_OBJECT_ID + 2);

    /* allocate the extension */
    alloc_ext(vmg_ fref, frame_idx, ret_ofs);
}

/*
 *   save
 */
void CVmObjFrameDesc::save_to_file(VMG_ class CVmFile *fp)
{
    /* get our extension */
    vm_framedesc_ext *ext = get_ext();

    /* save our extension data */
    fp->write_uint4(ext->fref);
    fp->write_uint2(ext->frame_idx);
    fp->write_uint2(ext->ret_ofs);
}

/*
 *   restore 
 */
void CVmObjFrameDesc::restore_from_file(VMG_ vm_obj_id_t self,
                                        class CVmFile *fp,
                                        class CVmObjFixup *fixups)
{
    /* read the values */
    vm_obj_id_t fref = fixups->get_new_id(vmg_ (vm_obj_id_t)fp->read_uint4());
    int frame_idx = fp->read_int2();
    uint ret_ofs = fp->read_uint2();

    /* allocate the extension */
    alloc_ext(vmg_ fref, frame_idx, ret_ofs);
}

/*
 *   Find a symbol.  If we find the name, we fill in 'symp' with the
 *   description of the variable and return true; otherwise we return false.
 */
int CVmObjFrameDesc::find_local(VMG_ const textchar_t *name, size_t namelen,
                                CVmDbgFrameSymPtr *symp)
{

    /* get our extension and the underlying frame's extension */
    vm_framedesc_ext *ext = get_ext();
    vm_frameref_ext *fext = get_frame_ref(vmg0_)->get_ext();

    /* if the caller doesn't care about the symbol information, use a local */
    CVmDbgFrameSymPtr oursym;
    if (symp == 0)
        symp = &oursym;

    /* if there's no frame, there are no locals */
    if (ext->frame_idx == 0)
        return FALSE;

    /* set up pointer to this method's debug records */
    CVmDbgTablePtr dp;
    if (!dp.set(fext->entryp))
        return FALSE;

    /* set up a pointer to our frame */
    CVmDbgFramePtr dfp;
    dp.set_frame_ptr(vmg_ &dfp, ext->frame_idx);

    /* walk up the list of frames from innermost to outermost */
    for (;;)
    {
        /* set up a pointer to the first symbol */
        dfp.set_first_sym_ptr(vmg_ symp);

        /* scan this frame's local symbol list */
        int sym_cnt = dfp.get_sym_count();
        for (int i = 0 ; i < sym_cnt ; ++i, symp->inc(vmg0_))
        {
            /* check for a match */
            if (symp->get_sym_len(vmg0_) == namelen
                && memcmp(symp->get_sym(vmg0_), name, namelen) == 0)
            {
                /* this is it - return with symp pointing to the symbol */
                return TRUE;
            }
        }

        /* didn't find it - move up to the enclosing frame */
        int parent = dfp.get_enclosing_frame();
        if (parent == 0)
            break;

        /* set up dfp to point to the parent fraem */
        dp.set_frame_ptr(vmg_ &dfp, parent);
    }

    /* we didn't find the symbol */
    return FALSE;
}

/*
 *   find a local variable in our frame by name, given a VM string value 
 */
int CVmObjFrameDesc::find_local(VMG_ const vm_val_t *nval,
                               CVmDbgFrameSymPtr *symp)
{
    /* make sure the name is a string */
    const char *name = nval->get_as_string(vmg0_);
    if (name == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* parse the length */
    size_t namelen = vmb_get_len(name);
    name += VMB_LEN;

    /* look up the symbol */
    return find_local(vmg_ name, namelen, symp);
}

/*
 *   Get the value of a local by name 
 */
int CVmObjFrameDesc::get_local_val(VMG_ vm_val_t *result, const vm_val_t *name)
{
    /* look up the symbol by name */
    CVmDbgFrameSymPtr sym;
    if (!find_local(vmg_ name, &sym))
        return FALSE;

    /* retrieve the value based on the descriptor */
    get_frame_ref(vmg0_)->get_local_val(vmg_ result, &sym);

    /* found it */
    return TRUE;
}

/*
 *   Set the value of a local by name 
 */
int CVmObjFrameDesc::set_local_val(VMG_ const vm_val_t *name,
                                   const vm_val_t *new_val)
{
    /* look up the symbol by name */
    CVmDbgFrameSymPtr sym;
    if (!find_local(vmg_ name, &sym))
        return FALSE;

    /* retrieve the value based on the descriptor */
    get_frame_ref(vmg0_)->set_local_val(vmg_ &sym, new_val);

    /* found it */
    return TRUE;
}

/* 
 *   index the frame: this looks up a variable's value by name 
 */
int CVmObjFrameDesc::index_val_q(VMG_ vm_val_t *result,
                                 vm_obj_id_t self,
                                 const vm_val_t *index_val)
{
    /* check the index type */
    if (index_val->get_as_string(vmg0_))
    {
        /* it's a string - look up the value by name */
        if (!get_local_val(vmg_ result, index_val))
            err_throw(VMERR_INDEX_OUT_OF_RANGE);
    }
    else
    {
        /* invalid index type */
        err_throw(VMERR_INDEX_OUT_OF_RANGE);
    }

    /* handled */
    return TRUE;
}

/* 
 *   assign an indexed value: this sets a variable's value by name 
 */
int CVmObjFrameDesc::set_index_val_q(VMG_ vm_val_t *new_container,
                                    vm_obj_id_t self,
                                    const vm_val_t *index_val,
                                    const vm_val_t *new_val)
{
    /* check the index type */
    if (index_val->get_as_string(vmg0_))
    {
        /* it's a string - look up the value by name */
        if (!set_local_val(vmg_ index_val, new_val))
            err_throw(VMERR_INDEX_OUT_OF_RANGE);
    }
    else
    {
        /* invalid index type */
        err_throw(VMERR_INDEX_OUT_OF_RANGE);
    }

    /* the container doesn't change */
    new_container->set_obj(self);

    /* handled */
    return TRUE;
}

/* 
 *   get a property 
 */
int CVmObjFrameDesc::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                              vm_obj_id_t self, vm_obj_id_t *source_obj,
                              uint *argc)
{
    uint func_idx;

    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling from the base object class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/*
 *   property evaluator - determine if the frame is still alive
 */
int CVmObjFrameDesc::getp_is_active(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                    uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* the frame is valid if we still have a frame pointer */
    retval->set_logical(get_frame_ref(vmg0_)->is_active());

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - set 'self' from the frame
 */
int CVmObjFrameDesc::getp_get_self(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the 'self' object */
    get_frame_ref(vmg0_)->get_self(vmg_ retval);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - set 'definingobj' from the frame 
 */
int CVmObjFrameDesc::getp_get_defobj(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                     uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get 'definingobj' from the frame */
    get_frame_ref(vmg0_)->get_defobj(vmg_ retval);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - set 'targetobj' from the frame 
 */
int CVmObjFrameDesc::getp_get_targobj(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                      uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get 'targetobj' from the frame */
    get_frame_ref(vmg0_)->get_targobj(vmg_ retval);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - get 'targetprop' from the frame 
 */
int CVmObjFrameDesc::getp_get_targprop(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                       uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get 'targetprop' from the frame */
    get_frame_ref(vmg0_)->get_targprop(vmg_ retval);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - get 'invokee' from the frame 
 */
int CVmObjFrameDesc::getp_get_invokee(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                      uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get 'invokee' from the frame */
    get_frame_ref(vmg0_)->get_invokee(vmg_ retval);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - get a lookup table of the local variables, with
 *   their current values, keyed by name 
 */
int CVmObjFrameDesc::getp_get_vars(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                   uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* create our lookup table object */
    retval->set_obj(CVmObjLookupTable::create(vmg_ FALSE, 32, 64));
    CVmObjLookupTable *tab = (CVmObjLookupTable *)
                             vm_objp(vmg_ retval->val.obj);

    /* get our extension */
    vm_framedesc_ext *ext = get_ext();

    /* get our underlying frame ref and its extension */
    CVmObjFrameRef *fr = get_frame_ref(vmg0_);
    vm_frameref_ext *fext = fr->get_ext();

    /* set up pointer to this method's debug records */
    CVmDbgTablePtr dp;
    if (!dp.set(fext->entryp))
        return TRUE;

    /* set up a pointer to our frame */
    CVmDbgFramePtr dfp;
    dp.set_frame_ptr(vmg_ &dfp, ext->frame_idx);

    /* push the table for gc protection */
    G_stk->push(retval);

    /* walk up the list of frames from innermost to outermost */
    for (;;)
    {
        /* set up a pointer to the first symbol */
        CVmDbgFrameSymPtr sym;
        dfp.set_first_sym_ptr(vmg_ &sym);

        /* scan this frame's local symbol list */
        int sym_cnt = dfp.get_sym_count();
        for (int i = 0 ; i < sym_cnt ; ++i, sym.inc(vmg0_))
        {
            /* set up a string value for the key */
            vm_val_t key;
            sym.get_str_val(vmg_ &key);
            G_stk->push(&key);

            /* 
             *   If this entry isn't already in the table, add it.  Don't
             *   bother if it already exists: we work from inner to outer
             *   scopes, and inner scopes hide things in outer scopes, so if
             *   we find an entry in the table already it means that it was
             *   an inner-scope entry that hides the one we're processing
             *   now.  
             */
            vm_val_t val;
            if (!tab->index_check(vmg_ &val, &key))
            {
                /* get the value */
                fr->get_local_val(vmg_ &val, &sym);
                
                /* add the entry to the table */
                tab->add_entry(vmg_ &key, &val);
            }

            /* done with the key for gc protection */
            G_stk->discard();
        }

        /* move up to the enclosing frame */
        int parent = dfp.get_enclosing_frame();
        if (parent == 0)
            break;

        /* set up dfp to point to the parent fraem */
        dp.set_frame_ptr(vmg_ &dfp, parent);
    }

    /* discard the table ref we pushed for gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Stack frame reference 
 */

/*
 *   statics 
 */
static CVmMetaclassFrameRef ref_metaclass_reg_obj;
CVmMetaclass *CVmObjFrameRef::metaclass_reg_ = &ref_metaclass_reg_obj;

/*
 *   construction 
 */
CVmObjFrameRef::CVmObjFrameRef(VMG_ vm_val_t *fp,
                               const uchar *entry)
{
    /* set up a function pointer */
    CVmFuncPtr f(entry);

    /* 
     *   figure the total number of variable snapshot slots we need to
     *   allocate: this is the number of local variables plus the number of
     *   actual arguments 
     */
    int nlocals = f.get_local_cnt();
    int nparams = G_interpreter->get_argc_from_frame(vmg_ fp);

    /* allocate our extension */
    ext_ = 0;
    vm_frameref_ext *ext = alloc_ext(vmg_ nlocals, nparams);

    /* save the extra frame information */
    ext->fp = fp;
    ext->entryp = entry;

    /* get the function pointer value for the entry pointer */
    f.get_fnptr(vmg_ &ext->entry);

    /* initialize the variable slots to nil */
    for (int i = 0 ; i < nlocals + nparams ; ++i)
        ext->vars[i].set_nil();

    /* 
     *   save the method context variables - these are immutable, so we can
     *   save them immediately 
     */
    ext->self = *G_interpreter->get_self_val_from_frame(vmg_ fp);
    ext->defobj = G_interpreter->get_defining_obj_from_frame(vmg_ fp);
    ext->targobj = G_interpreter->get_orig_target_obj_from_frame(vmg_ fp);
    ext->defobj = G_interpreter->get_defining_obj_from_frame(vmg_ fp);
}

/*
 *   create 
 */
vm_obj_id_t CVmObjFrameRef::create(
    VMG_ vm_val_t *fp, const uchar *entry)
{
    /* allocate an object ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* instantiate the new object */
    new (vmg_ id) CVmObjFrameRef(vmg_ fp, entry);

    /* return the new object's ID */
    return id;
}

/*
 *   allocate our extension 
 */
vm_frameref_ext *CVmObjFrameRef::alloc_ext(VMG_ int nlocals, int nparams)
{
    /* delete any existing extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* figure the allocation size */
    size_t siz = sizeof(vm_frameref_ext)
                 + (nparams + nlocals - 1)*sizeof(vm_val_t);

    /* allocate the extension */
    vm_frameref_ext *ext =
        (vm_frameref_ext *)G_mem->get_var_heap()->alloc_mem(siz, this);

    /* save the variable counts */
    ext->nlocals = nlocals;
    ext->nparams = nparams;

    /* save the new extension in 'self' */
    ext_ = (char *)ext;

    /* return the new extension */
    return ext;
}

/*
 *   notify of deletion 
 */
void CVmObjFrameRef::notify_delete(VMG_ int)
{
    /* free our extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/*
 *   mark references
 */
void CVmObjFrameRef::mark_refs(VMG_ uint state)
{
    /* get the extension */
    vm_frameref_ext *ext = get_ext();

    /* mark the function */
    if (ext->entry.typ == VM_OBJ)
        G_obj_table->mark_all_refs(ext->entry.val.obj, state);

    /* mark the method context objects */
    if (ext->self.typ == VM_OBJ)
        G_obj_table->mark_all_refs(ext->self.val.obj, state);
    if (ext->defobj != VM_INVALID_OBJ)
        G_obj_table->mark_all_refs(ext->defobj, state);
    if (ext->targobj != VM_INVALID_OBJ)
        G_obj_table->mark_all_refs(ext->targobj, state);

    /* mark the invokee */
    if (ext->invokee.typ == VM_OBJ)
        G_obj_table->mark_all_refs(ext->invokee.val.obj, state);

    /* mark each snapshot variable */
    int i;
    const vm_val_t *v;
    for (i = ext->nlocals + ext->nparams, v = ext->vars ; i > 0 ; --i, ++v)
    {
        if (v->typ == VM_OBJ)
            G_obj_table->mark_all_refs(v->val.obj, state);
    }
}


/*
 *   load from the image file 
 */
void CVmObjFrameRef::load_from_image(VMG_ vm_obj_id_t self,
                                     const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image 
 */
void CVmObjFrameRef::reload_from_image(VMG_ vm_obj_id_t self,
                                       const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);
}

/*
 *   load or reload our image data 
 */
void CVmObjFrameRef::load_image_data(VMG_ vm_obj_id_t self,
                                     const char *ptr, size_t siz)
{
    /* read the number of variables */
    int nlocals = osrp2(ptr);
    int nparams = osrp2(ptr+2);
    ptr += 4;

    /* allocate the extension */
    vm_frameref_ext *ext = alloc_ext(vmg_ nlocals, nparams);

    /* 
     *   Since stack frames are inherently transient, a saved frame ref
     *   object can't point back to a live stack frame, so on restore we have
     *   to assume that our stack frame is inactive.  
     */
    ext->fp = 0;

    /* load the entry pointer */
    vmb_get_dh(ptr, &ext->entry);
    ptr += VMB_DATAHOLDER;

    /* after other objects are loaded, resolve the entry pointer */
    G_obj_table->request_post_load_init(self);

    /* load the method context variables */
    vmb_get_dh(ptr, &ext->self);
    ptr += VMB_DATAHOLDER;

    ext->defobj = vmb_get_objid(ptr);
    ptr += VMB_OBJECT_ID;

    ext->targobj = vmb_get_objid(ptr);
    ptr += VMB_OBJECT_ID;

    ext->targprop = vmb_get_propid(ptr);
    ptr += VMB_PROP_ID;

    vmb_get_dh(ptr, &ext->invokee);
    ptr += VMB_DATAHOLDER;

    /* read the variable snapshot values */
    int i;
    vm_val_t *v;
    for (i = nlocals + nparams, v = ext->vars ; i > 0 ; --i, ++v)
    {
        vmb_get_dh(ptr, v);
        ptr += VMB_DATAHOLDER;
    }
}

/*
 *   save
 */
void CVmObjFrameRef::save_to_file(VMG_ class CVmFile *fp)
{
    char buf[VMB_DATAHOLDER];
    
    /* get our extension */
    vm_frameref_ext *ext = get_ext();
    
    /* 
     *   If our frame is still active, make a snapshot of the variables.  The
     *   stack frame itself is inherently transient, so we can only save the
     *   snapshot version.  
     */
    if (ext->fp != 0)
        make_snapshot(vmg0_);

    /* save the variable counts */
    fp->write_uint2(ext->nlocals);
    fp->write_uint2(ext->nparams);

    /* save the entry pointer */
    vmb_put_dh(buf, &ext->entry);
    fp->write_bytes(buf, VMB_DATAHOLDER);

    /* save the method context values */
    vmb_put_dh(buf, &ext->self);
    fp->write_bytes(buf, VMB_DATAHOLDER);
    fp->write_uint4(ext->defobj);
    fp->write_uint4(ext->targobj);
    fp->write_uint2(ext->targprop);
    vmb_put_dh(buf, &ext->invokee);
    fp->write_bytes(buf, VMB_DATAHOLDER);

    /* save the variable snapshot values */
    int i;
    const vm_val_t *v;
    for (i = ext->nparams + ext->nlocals, v = ext->vars ; i > 0 ; --i, ++v)
    {
        vmb_put_dh(buf, v);
        fp->write_bytes(buf, VMB_DATAHOLDER);
    }
}

/*
 *   restore 
 */
void CVmObjFrameRef::restore_from_file(VMG_ vm_obj_id_t self,
                                       class CVmFile *fp,
                                       class CVmObjFixup *fixups)
{
    char buf[VMB_DATAHOLDER];
    
    /* read the variables counts */
    int nlocals = fp->read_int2();
    int nparams = fp->read_int2();

    /* allocate the extension */
    vm_frameref_ext *ext = alloc_ext(vmg_ nlocals, nparams);

    /* 
     *   Since stack frames are inherently transient, a saved frame ref
     *   object can't point back to a live stack frame, so on restore we have
     *   to assume that our stack frame is inactive. 
     */
    get_ext()->fp = 0;

    /* load the entry pointer */
    fp->read_bytes(buf, VMB_DATAHOLDER);
    fixups->fix_dh(vmg_ buf);
    vmb_get_dh(buf, &ext->entry);

    /* after other objects are loaded, resolve the entry pointer */
    G_obj_table->request_post_load_init(self);

    /* restore the method context values */
    fp->read_bytes(buf, VMB_DATAHOLDER);
    fixups->fix_dh(vmg_ buf);
    vmb_get_dh(buf, &ext->self);

    ext->defobj = (vm_obj_id_t)fp->read_uint4();
    if (ext->defobj != VM_INVALID_OBJ)
        ext->defobj = fixups->get_new_id(vmg_ ext->defobj);
    
    ext->targobj = (vm_obj_id_t)fp->read_uint4();
    if (ext->targobj != VM_INVALID_OBJ)
        ext->targobj = fixups->get_new_id(vmg_ ext->targobj);
    
    ext->targprop = (vm_prop_id_t)fp->read_uint2();

    fp->read_bytes(buf, VMB_DATAHOLDER);
    fixups->fix_dh(vmg_ buf);
    vmb_get_dh(buf, &ext->invokee);

    /* load the snapshot values */
    int i;
    vm_val_t *v;
    for (i = nlocals + nparams, v = ext->vars ; i > 0 ; --i, ++v)
    {
        fp->read_bytes(buf, VMB_DATAHOLDER);
        fixups->fix_dh(vmg_ buf);
        vmb_get_dh(buf, v);
    }
}

/*
 *   After loading, resolve the entry pointer.  We need to wait until loading
 *   is complete to do this, since the entry pointer might refer to another
 *   object, in which case the other object needs to be loaded before we can
 *   resolve a pointer to it.  
 */
void CVmObjFrameRef::post_load_init(VMG_ vm_obj_id_t self)
{
    /* get our extension */
    vm_frameref_ext *ext = get_ext();

    /* resolve the entry pointer */
    CVmFuncPtr f(vmg_ &ext->entry);
    ext->entryp = f.get();
}

/*
 *   Invalidate the frame.  The VM calls this just before our frame exits.
 *   We make a snapshot of the local variables in the frame, then we set our
 *   frame pointer to null to indicate that we no longer have an active frame
 *   in the stack.  
 */
void CVmObjFrameRef::inval_frame(VMG0_)
{
    if (ext_ != 0 && get_ext()->fp != 0)
    {
        /* make a snapshot of the frame */
        make_snapshot(vmg0_);
        
        /* forget our frame pointer */
        get_ext()->fp = 0;
    }
}

/*
 *   make a snapshot of the local variables in the frame 
 */
void CVmObjFrameRef::make_snapshot(VMG0_)
{
    int i;
    vm_val_t *v;
    vm_frameref_ext *ext = get_ext();
    vm_val_t *fp = ext->fp;
    
    /* make a copy of each local */
    for (i = 0, v = ext->vars ; i < ext->nlocals ; ++i, ++v)
        *v = *G_interpreter->get_local_from_frame(vmg_ fp, i);

    /* make a copy of each parameter */
    for (i = 0 ; i < ext->nparams ; ++i, ++v)
        *v = *G_interpreter->get_param_from_frame(vmg_ fp, i);
}

/* 
 *   index the frame: this looks up a variable by integer frame index
 */
int CVmObjFrameRef::index_val_q(VMG_ vm_val_t *result,
                                vm_obj_id_t self,
                                const vm_val_t *index_val)
{
    /* get our extension */
    vm_frameref_ext *ext = get_ext();

    /* check the type */
    if (index_val->typ == VM_INT)
    {
        /* 
         *   It's a direct index into the frame.  Make sure the value is in
         *   range.  
         */
        int n = index_val->val.intval;
        if (n < 0 || n >= ext->nlocals + ext->nparams)
            err_throw(VMERR_INDEX_OUT_OF_RANGE);

        /*  
         *   If we have an active stack frame, read the value from the stack;
         *   otherwise read it from the snapshot array.  
         */
        if (ext->fp != 0)
        {
            /* get it from the stack frame */
            if (n < ext->nlocals)
            {
                /* it's in the local variable range */
                *result =
                    *G_interpreter->get_local_from_frame(vmg_ ext->fp, n);
            }
            else
            {
                /* it's in the parameter range */
                *result = *G_interpreter->get_param_from_frame(
                    vmg_ ext->fp, n - ext->nlocals);
            }
        }
        else
        {
            /* there's no frame - get it from the snapshot array */
            *result = ext->vars[n];
        }
    }
    else
    {
        /* invalid index type */
        err_throw(VMERR_INDEX_OUT_OF_RANGE);
    }

    /* handled */
    return TRUE;
}

/* 
 *   assign an indexed value: this sets a variable's value by index
 */
int CVmObjFrameRef::set_index_val_q(VMG_ vm_val_t *new_container,
                                    vm_obj_id_t self,
                                    const vm_val_t *index_val,
                                    const vm_val_t *new_val)
{
    /* get our extension */
    vm_frameref_ext *ext = get_ext();

    /* check the type */
    if (index_val->typ == VM_INT)
    {
        /* 
         *   It's a direct index into the frame.  Make sure the value is in
         *   range.  
         */
        int n = index_val->val.intval;
        if (n < 0 || n >= ext->nlocals + ext->nparams)
            err_throw(VMERR_INDEX_OUT_OF_RANGE);

        /*  
         *   If we have an active stack frame, read the value from the stack;
         *   otherwise read it from the snapshot array.  
         */
        if (ext->fp != 0)
        {
            /* get it from the stack frame */
            if (n < ext->nlocals)
            {
                /* it's in the local variable range */
                *G_interpreter->get_local_from_frame(vmg_ ext->fp, n) =
                    *new_val;
            }
            else
            {
                /* it's in the parameter range */
                *G_interpreter->get_param_from_frame(
                    vmg_ ext->fp, n - ext->nlocals) = *new_val;
            }
        }
        else
        {
            /* there's no frame - get it from the snapshot array */
            ext->vars[n] = *new_val;
        }
    }
    else
    {
        /* invalid index type */
        err_throw(VMERR_INDEX_OUT_OF_RANGE);
    }

    /* the container doesn't change */
    new_container->set_obj(self);

    /* handled */
    return TRUE;
}

/*
 *   Get the frame index for a local.  This is the integer value that can be
 *   used with the [] operator to index the FrameRef object to retrieve or
 *   assign the value of the local.  For a regular local, this is simply the
 *   same as the local variable number in the frame.  For a parameter, this
 *   is 'n + nlocals', where 'n' is the parameter number in the frame.  
 */
int CVmObjFrameRef::get_var_frame_index(const CVmDbgFrameSymPtr *symp)
{
    /* check to see if it's a local or a parameter */
    if (symp->is_local())
    {
        /* it's a local - use the plain frame index */
        return symp->get_var_num();
    }
    else
    {
        /* it's a parameter - use the frame index plus the param offset */
        return symp->get_var_num() + get_ext()->nlocals;
    }
}

/*
 *   Get the value of a local given the variable descriptor 
 */
void CVmObjFrameRef::get_local_val(VMG_ vm_val_t *result,
                                   const CVmDbgFrameSymPtr *sym)
{
    /* 
     *   if we have an active stack frame, get the value from the frame;
     *   otherwise get the value from our local snapshot 
     */
    vm_frameref_ext *ext = get_ext();
    if (ext->fp != 0)
    {
        /* we have a frame - get the value from the frame */
        G_interpreter->get_local_from_frame(vmg_ result, ext->fp, sym);
    }
    else
    {
        /* 
         *   There's no frame, so we must retrieve the value from the
         *   snapshot.  First, get the value of the local or parameter.  Our
         *   snapshot array consists of all of the locals followed by all of
         *   the parameters, so local N is at vars[N] and parameter N is at
         *   vars[nlocals + N].  
         */
        vm_val_t *v = &ext->vars[sym->get_var_num()
                                 + (sym->is_param() ? ext->nlocals : 0)];

        /* 
         *   If it's a context local, index the local by the context index.
         *   Otherwise the value is simply the value in the snapshot array.  
         */
        if (sym->is_ctx_local())
            v->ll_index(vmg_ result, sym->get_ctx_arr_idx());
        else
            *result = *v;
    }
}

/*
 *   Set the value of a local given the variable descriptor 
 */
void CVmObjFrameRef::set_local_val(VMG_ const CVmDbgFrameSymPtr *sym,
                                   const vm_val_t *new_val)
{
    /* 
     *   if we have an active stack frame, get the value from the frame;
     *   otherwise get the value from our local snapshot 
     */
    vm_frameref_ext *ext = get_ext();
    if (ext->fp != 0)
    {
        /* we have a frame - get the value from the frame */
        G_interpreter->set_local_in_frame(vmg_ new_val, ext->fp, sym);
    }
    else
    {
        /* 
         *   There's no frame, so we must retrieve the value from the
         *   snapshot.  First, get the value of the local or parameter.  Our
         *   snapshot array consists of all of the locals followed by all of
         *   the parameters, so local N is at vars[N] and parameter N is at
         *   vars[nlocals + N].  
         */
        vm_val_t *v = &ext->vars[sym->get_var_num()
                                 + (sym->is_param() ? ext->nlocals : 0)];

        /* 
         *   If it's a context local, index the local by the context index.
         *   Otherwise the value is simply the value in the snapshot array.  
         */
        if (sym->is_ctx_local())
        {
            vm_val_t cont;
            vm_val_t ival;
            ival.set_int(sym->get_ctx_arr_idx());

            if (v->typ == VM_OBJ)
                vm_objp(vmg_ v->val.obj)->set_index_val_ov(
                    vmg_ &cont, v->val.obj, &ival, new_val);
            else
                err_throw(VMERR_CANNOT_INDEX_TYPE);
        }
        else
            *v = *new_val;
    }
}


/*
 *   Get 'self' from the frame 
 */
void CVmObjFrameRef::get_self(VMG_ vm_val_t *result)
{
    /* return the 'self' value from the frame, if active, or our snapshot */
    if (get_ext()->fp != 0)
        *result = *G_interpreter->get_self_val_from_frame(vmg_ get_ext()->fp);
    else
        *result = get_ext()->self;

    /* if the return value is an invalid object ID, return nil */
    if (result->typ == VM_OBJ && result->val.obj == VM_INVALID_OBJ)
        result->set_nil();
}

/*
 *   Get 'definingobj' from the frame 
 */
void CVmObjFrameRef::get_defobj(VMG_ vm_val_t *result)
{
    /* get it from the active frame, or our snapshot if the frame has exited */
    result->set_obj_or_nil(
        get_ext()->fp != 0
        ? G_interpreter->get_defining_obj_from_frame(vmg_ get_ext()->fp)
        : get_ext()->defobj);
}

/*
 *   Get 'targetobj' from the frame 
 */
void CVmObjFrameRef::get_targobj(VMG_ vm_val_t *result)
{
    /* get 'targetobj' from the frame, if active, or our snapshot */
    result->set_obj_or_nil(
        get_ext()->fp != 0
        ? G_interpreter->get_orig_target_obj_from_frame(vmg_ get_ext()->fp)
        : get_ext()->targobj);
}

/*
 *   Get 'targetprop' from the frame 
 */
void CVmObjFrameRef::get_targprop(VMG_ vm_val_t *result)
{
    /* get 'targetobj' from the frame, if active, or our snapshot */
    vm_prop_id_t prop =
        get_ext()->fp != 0
        ? G_interpreter->get_target_prop_from_frame(vmg_ get_ext()->fp)
        : get_ext()->targprop;

    /* set the return value to the property, or nil if there isn't one */
    if (prop != VM_INVALID_PROP)
        result->set_propid(prop);
    else
        result->set_nil();
}

/*
 *   Get 'invokee' from the frame 
 */
void CVmObjFrameRef::get_invokee(VMG_ vm_val_t *result)
{
    /* get 'invokee' from the frame, if active, or our snapshot */
    *result =
        get_ext()->fp != 0
        ? *G_interpreter->get_invokee_from_frame(vmg_ get_ext()->fp)
        : get_ext()->invokee;
}

/*
 *   Create a method context object 
 */
void CVmObjFrameRef::create_loadctx_obj(VMG_ vm_val_t *result)
{
    vm_frameref_ext *ext = get_ext();
    vm_val_t *fp = ext->fp;
    vm_obj_id_t self, defobj, targobj;
    vm_prop_id_t targprop;

    /* retrieve the method context elements */
    if (fp != 0)
    {
        /* get the context elements from the live stack frame */
        self = G_interpreter->get_self_from_frame(vmg_ fp);
        defobj = G_interpreter->get_defining_obj_from_frame(vmg_ fp);
        targobj = G_interpreter->get_orig_target_obj_from_frame(vmg_ fp);
        targprop = G_interpreter->get_target_prop_from_frame(vmg_ fp);
    }
    else
    {
        /* the stack frame has exited, so use our snapshot */
        self = (ext->self.typ == VM_OBJ ? ext->self.val.obj : VM_INVALID_OBJ);
        defobj = ext->defobj;
        targobj = ext->targobj;
        targprop = ext->targprop;
    }

    /* create the context object */
    CVmRun::create_loadctx_obj(vmg_ result, self, defobj, targobj, targprop);
}


/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfref.h - stack frame reference object
Function
  There are two objects comprising a stack frame reference:

  - A StackFrameDesc object.  This object is visible to the bytecode
  program and provides the API.  This object records a particular return
  location in a function; it stores a return address, scope index, and a
  pointer to a StackFrameRef object.

  - The StackFrameRef object.  This object is a reference to an activation
  frame.  It records the live stack frame pointer and entry address, and
  stores a snapshot of the local variables, parameters, and method context
  variables after the frame terminates.  This is an internal object with no
  public API; its interface is used only by StackFrameDesc.

  The reason we need two objects is that a given activation frame can have
  many execution points over the course of its lifetime.  Each execution
  point has to be recorded separately, since a single function can have
  multiple local variable scopes with different symbol bindings.
Notes
  
Modified
  04/22/00 MJRoberts  - Creation
*/

#ifndef VMFREF_H
#define VMFREF_H

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"

/* ------------------------------------------------------------------------ */
/*
 *   Extension for the frame descriptor object 
 */
struct vm_framedesc_ext
{
    /* the underlying StackFrameRef object */
    vm_obj_id_t fref;

    /* frame index in the debug table for the method */
    int frame_idx;

    /* the return offset */
    uint ret_ofs;
};

/*
 *   Extension for stack frame reference object 
 */
struct vm_frameref_ext
{
    /* 
     *   Stack frame pointer.  The VM automatically sets this to null when
     *   the frame exits.  This is also set to null when we restore a frame
     *   from a saved state file, since stack frames are inherently transient
     *   and thus show up as null references in a save file.  
     */
    vm_val_t *fp;

    /* method entry pointer value */
    vm_val_t entry;

    /* resolved pointer to the method entry byte code */
    const uchar *entryp;

    /* number of local variables and parameters */
    int nlocals;
    int nparams;

    /* 
     *   Snapshot copy of the method context variables.  We copy these values
     *   at the same time we copy the local variables when detaching the
     *   frame (see below). 
     */
    vm_val_t self;
    vm_obj_id_t defobj;
    vm_obj_id_t targobj;
    vm_prop_id_t targprop;
    vm_val_t invokee;

    /* 
     *   Snapshot copy of the locals and parameters.  Just before the
     *   associated routine returns to its caller, rendering the true stack
     *   frame invalid, we make a private copy of the locals and parameters
     *   from the frame here.  This allows callers to access the variable
     *   values after the frame becomes inactive, as long as 'self' continues
     *   to be reachable.  This effectively detaches the frame from the
     *   stack, analogously to the way "context locals" for anonymous
     *   functions work.  The difference from anonymous functions is that our
     *   mechanism here can detach any frame on the fly, without the compiler
     *   having to make special arrangements in advance.
     *   
     *   We overallocate the structure to make room for nparams + nlocals
     *   slots.  The locals come first, followed by the parameters.  
     */
    vm_val_t vars[1];
};


/* ------------------------------------------------------------------------ */
/*
 *   Stack frame descriptor object 
 */
class CVmObjFrameDesc: public CVmObject
{
    friend class CVmMetaclassFrameDesc;

public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* is 'obj' a frame descriptor object? */
    static int is_framedesc_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* get my StackFrameRef object */
    vm_obj_id_t get_frame_ref_id() const { return get_ext()->fref; }
    class CVmObjFrameRef *get_frame_ref(VMG0_) const
        { return (class CVmObjFrameRef *)vm_objp(vmg_ get_frame_ref_id()); }

    /* find a local in our frame */
    int find_local(VMG_ const vm_val_t *nval, class CVmDbgFrameSymPtr *symp);
    int find_local(VMG_ const textchar_t *sym, size_t len,
                   class CVmDbgFrameSymPtr *symp);

    /* get the value of a local by name */
    int get_local_val(VMG_ vm_val_t *result, const vm_val_t *name);

    /* set the value of a local by name */
    int set_local_val(VMG_ const vm_val_t *name, const vm_val_t *new_val);

    /* create */
    static vm_obj_id_t create(VMG_ vm_obj_id_t fref, int frame_idx,
                              uint ret_ofs);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
        { return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop); }

    /* set a property */
    void set_prop(VMG_ class CVmUndo *,
                  vm_obj_id_t, vm_prop_id_t, const vm_val_t *)
    {
        /* cannot set frame reference properties */
        err_throw(VMERR_INVALID_SETPROP);
    }

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);


    /* index the frame: this looks up a variable's value by name */
    virtual int index_val_q(VMG_ vm_val_t *result,
                            vm_obj_id_t self,
                            const vm_val_t *index_val);

    /* assign an indexed value: this sets a variable's value by name */
    virtual int set_index_val_q(VMG_ vm_val_t *new_container,
                                vm_obj_id_t self,
                                const vm_val_t *index_val,
                                const vm_val_t *new_val);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* undo methods - we don't participate in undo, so these are no-ops */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }

    /* mark references */
    void mark_refs(VMG_ uint);

    /* we're immutable, so we don't have to worry about undo */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* we don't have any weak references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* image file operations */
    void load_from_image(VMG_ vm_obj_id_t, const char *, size_t);
    void reload_from_image(VMG_ vm_obj_id_t, const char *, size_t);
    ulong rebuild_image(VMG_ char *, ulong);

    /* we're not loadable and we don't change */
    int is_changed_since_load() const { return FALSE; }

    /* save/restore */
    void save_to_file(VMG_ class CVmFile *fp);
    void restore_from_file(VMG_ vm_obj_id_t self, class CVmFile *fp,
                           class CVmObjFixup *fixups);

protected:
    /* construction */
    CVmObjFrameDesc() { ext_ = 0; }
    CVmObjFrameDesc(VMG_ vm_obj_id_t fref, int frame_idx, uint ret_ofs);

    /* allocate our extension */
    vm_framedesc_ext *alloc_ext(VMG_ vm_obj_id_t fref,
                                int frame_idx, uint ret_ofs);

    /* get my extension, properly cast */
    vm_framedesc_ext *get_ext() const { return (vm_framedesc_ext *)ext_; }

    /* load the image data */
    void load_image_data(VMG_ vm_obj_id_t self,
                         const char *ptr, size_t siz);

    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - is this frame still activie? */
    virtual int getp_is_active(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc);

    /* property evaluator - get 'self' in the frame */
    virtual int getp_get_self(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc);

    /* property evaluator - get 'definingobj' in the frame */
    virtual int getp_get_defobj(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc);

    /* property evaluator - get 'targetobj' in the frame */
    virtual int getp_get_targobj(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc);

    /* property evaluator - get 'targetprop' in the frame */
    virtual int getp_get_targprop(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                  uint *argc);

    /* property evaluator - get a lookup table of the variables */
    virtual int getp_get_vars(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc);

    /* property evaluator - get 'invokee' in the frame */
    virtual int getp_get_invokee(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc);

    /* function table */
    static int (CVmObjFrameDesc::*func_table_[])
        (VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   Stack frame reference object
 */
class CVmObjFrameRef: public CVmObject
{
    friend class CVmMetaclassFrameRef;
    friend class CVmObjFrameDesc;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* is 'obj' a frame reference object? */
    static int is_frameref_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create */
    static vm_obj_id_t create(VMG_ vm_val_t *fp, const uchar *entry);

    /* 
     *   Invalidate the frame reference.  The bytecode interpreter calls this
     *   when exiting a frame by 'return', 'throw', etc. 
     */
    void inval_frame(VMG0_);

    /* is the frame active? */
    int is_active() { return get_ext()->fp != 0; }

    /* get 'self' from the frame */
    void get_self(VMG_ vm_val_t *result);

    /* get 'definingobj' from the frame */
    void get_defobj(VMG_ vm_val_t *result);

    /* get 'targetobj' from the frame */
    void get_targobj(VMG_ vm_val_t *result);

    /* get 'targetprop' from the frame */
    void get_targprop(VMG_ vm_val_t *result);

    /* get 'invokee' from the frame */
    void get_invokee(VMG_ vm_val_t *result);

    /* create a method context object for the LOADCTX instruction */
    void create_loadctx_obj(VMG_ vm_val_t *result);

    /* get the value of a local given its frame descriptor */
    void get_local_val(VMG_ vm_val_t *result,
                       const class CVmDbgFrameSymPtr *sym);

    /* set the value of a local given its frame descriptor */
    void set_local_val(VMG_ const CVmDbgFrameSymPtr *sym,
                       const vm_val_t *new_val);

    /* get the integer frame index for a variable given its descriptor */
    int get_var_frame_index(const CVmDbgFrameSymPtr *symp);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
        { return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop); }

    /* set a property */
    void set_prop(VMG_ class CVmUndo *,
                  vm_obj_id_t, vm_prop_id_t, const vm_val_t *)
    {
        /* cannot set frame reference properties */
        err_throw(VMERR_INVALID_SETPROP);
    }
    
    /* index the frame: this looks up a variable by frame index */
    virtual int index_val_q(VMG_ vm_val_t *result,
                            vm_obj_id_t self,
                            const vm_val_t *index_val);

    /* assign an indexed value: this sets a variable by frame index */
    virtual int set_index_val_q(VMG_ vm_val_t *new_container,
                                vm_obj_id_t self,
                                const vm_val_t *index_val,
                                const vm_val_t *new_val);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* undo methods - we don't participate in undo, so these are no-ops */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }

    /* mark references */
    void mark_refs(VMG_ uint);

    /* we're immutable, so we don't have to worry about undo */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* we don't have any weak references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* image file operations */
    void load_from_image(VMG_ vm_obj_id_t, const char *, size_t);
    void reload_from_image(VMG_ vm_obj_id_t, const char *, size_t);
    ulong rebuild_image(VMG_ char *, ulong);

    /* we're not loadable and we don't change */
    int is_changed_since_load() const { return FALSE; }

    /* save/restore */
    void save_to_file(VMG_ class CVmFile *fp);
    void restore_from_file(VMG_ vm_obj_id_t self, class CVmFile *fp,
                           class CVmObjFixup *fixups);

    /* post-load initialization */
    void post_load_init(VMG_ vm_obj_id_t self);

protected:
    /* construction */
    CVmObjFrameRef() { ext_ = 0; }
    CVmObjFrameRef(VMG_ vm_val_t *fp, const uchar *entry);

    /* allocate our extension */
    vm_frameref_ext *alloc_ext(VMG_ int nlocals, int nparams);

    /* get my extension, properly cast */
    vm_frameref_ext *get_ext() const { return (vm_frameref_ext *)ext_; }

    /* make a snapshot of the locals */
    void make_snapshot(VMG0_);

    /* load the image data */
    void load_image_data(VMG_ vm_obj_id_t self,
                         const char *ptr, size_t siz);
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table objects
 */
class CVmMetaclassFrameDesc: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "stack-frame-desc/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFrameDesc();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFrameDesc();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
    {
        err_throw(VMERR_BAD_DYNAMIC_NEW);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjFrameDesc::call_stat_prop(
            vmg_ result, pc_ptr, argc, prop);
    }
};

class CVmMetaclassFrameRef: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "stack-frame-ref/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFrameRef();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFrameRef();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
    {
        err_throw(VMERR_BAD_DYNAMIC_NEW);
        AFTER_ERR_THROW(return VM_INVALID_OBJ;)
    }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjFrameRef::call_stat_prop(
            vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMFREF_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjFrameRef)
VM_REGISTER_METACLASS(CVmObjFrameDesc)

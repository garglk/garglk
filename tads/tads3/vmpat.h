/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpat.h - regular-expression compiled pattern object
Function
  Encapsulates a compiled regular expression as an object.  This object
  allows a compiled regular expression to be saved for re-use; since regex
  compilation is time-consuming, it's much more efficient to re-use a
  compiled pattern in repeated searches than to recompile the expression
  each time it's needed.
Notes
  
Modified
  08/27/02 MJRoberts  - Creation
*/

#ifndef VMPAT_H
#define VMPAT_H

#include <stdlib.h>
#include <os.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"
#include "vmregex.h"

/* ------------------------------------------------------------------------ */
/*
 *   Our serialized data stream, in both the image file and a saved file,
 *   consists of:
 *   
 *   DATAHOLDER src_val
 *   
 *   'src_val' is the source value - this is the string that was compiled to
 *   create the pattern.
 */

/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension consists of a simple structure with a pointer
 *   to the compiled pattern data (the re_compiled_pattern structure) and
 *   the original string value that was used to create the pattern (we hold
 *   onto the original string mostly for debugging purposes).  
 */
struct vmobj_pat_ext
{
    /* the compiled pattern data */
    re_compiled_pattern *pat;

    /* the original pattern source string */
    vm_val_t str;
};

/* ------------------------------------------------------------------------ */
/*
 *   Pattern intrinsic class 
 */
class CVmObjPattern: public CVmObject
{
    friend class CVmMetaclassPattern;

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

    /* is this a pattern object? */
    static int is_pattern_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        /* defer to our base class */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* cast to string - return the original pattern string */
    const char *cast_to_string(VMG_ vm_obj_id_t self,
                               vm_val_t *new_str) const
    {
        /* return the original string value */
        *new_str = *get_orig_str();
        return new_str->get_as_string(vmg0_);
    }

    /* undo operations - we are immutable and hence keep no undo */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }    

    /* mark references */
    void mark_refs(VMG_ uint);

    /* remove stale weak references - we have no weak references */
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t);

    /* perform post-load initialization */
    void post_load_init(VMG_ vm_obj_id_t self);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixup);

    /* get my compiled pattern */
    re_compiled_pattern *get_pattern(VMG0_) { return get_ext()->pat; }

    /* get my string object */
    const vm_val_t *get_orig_str() const { return &get_ext()->str; }

protected:
    /* create with no extension */
    CVmObjPattern() { ext_ = 0; }

    /* create with a given pattern object and source string value */
    CVmObjPattern(VMG_ re_compiled_pattern *pat, const vm_val_t *src_str);

    /* set my compiled pattern data structure */
    void set_pattern(re_compiled_pattern *pat) { get_ext()->pat = pat; }

    /* set my original source string value */
    void set_orig_str(const vm_val_t *val) { get_ext()->str = *val; }

    /* get my extension data */
    vmobj_pat_ext *get_ext() const { return (vmobj_pat_ext *)ext_; }
    
    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - get my original string */
    int getp_get_str(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc);

    /* property evaluation function table */
    static int (CVmObjPattern::*func_table_[])(VMG_ vm_obj_id_t self,
                                               vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassPattern: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "regex-pattern/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjPattern();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjPattern();
        G_obj_table->set_obj_gc_characteristics(id, TRUE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjPattern::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjPattern::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMPAT_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjPattern)


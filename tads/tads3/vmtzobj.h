/* 
 *   Copyright (c) 2012 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtzobj.h - CVmObjTimeZone object
Function
  
Notes
  
Modified
  02/06/12 MJRoberts  - Creation
*/

#ifndef VMTZOBJ_H
#define VMTZOBJ_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"


/* ------------------------------------------------------------------------ */
/*
 *   The image file data simply contains the name of the time zone, as an
 *   ASCII string, with a one-byte length prefix.  The block is arranged as
 *   follows:
 *   
 *.    UBYTE name_length
 *.    ASCII name
 */


/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension data structure.
 */
struct vm_tzobj_ext
{
    /* allocate the structure */
    static vm_tzobj_ext *alloc_ext(VMG_ class CVmObjTimeZone *self,
                                   class CVmTimeZone *tz);

    /* our CVmTimeZone object */
    class CVmTimeZone *tz;
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTimeZone intrinsic class definition
 */

class CVmObjTimeZone: public CVmObject
{
    friend class CVmMetaclassTimeZone;
    
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

    /* get my CVmTimeZone object */
    class CVmTimeZone *get_tz() const { return get_ext()->tz; }

    /* is this a CVmObjTimeZone object? */
    static int is_CVmObjTimeZone_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* 
     *   Parse TimeZone constructor arguments, returning a CVmTimeZone cache
     *   object if the arguments match, throwing an error if not.  Doesn't
     *   actually create a TimeZone object; simply finds the underlying cache
     *   object.
     */
    static class CVmTimeZone *parse_ctor_args(
        VMG_ const vm_val_t *argp, int argc);

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* create from a given CVmTimeZone object */
    static vm_obj_id_t create(VMG_ class CVmTimeZone *tz);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        return CVmObject::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* 
     *   receive savepoint notification - we don't keep any
     *   savepoint-relative records, so we don't need to do anything here 
     */
    void notify_new_savept() { }
    
    /* we're immutable, so there's no undo to mess with */
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void discard_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* we have no outgoing references */
    void mark_refs(VMG_ uint) { }
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* reload from an image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* time zone objects are immutable */
    int is_changed_since_load() const { return FALSE; }

    /* cast to a string - returns the time zone name */
    const char *cast_to_string(VMG_ vm_obj_id_t self, vm_val_t *newstr) const;

protected:
    /* get my extension data */
    vm_tzobj_ext *get_ext() const { return (vm_tzobj_ext *)ext_; }

    /* load or reload image data */
    void load_image_data(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* create a with no initial contents */
    CVmObjTimeZone() { ext_ = 0; }

    /* create with contents */
    CVmObjTimeZone(VMG_ class CVmTimeZone *tz);

    /* 
     *   parse a zone name by GMT offset, specified as a string in +-hhmm or
     *   +-hh[:mm[:ss]] format, with the given prefix
     */
    static class CVmTimeZone *parse_zone_hhmmss(
        VMG_ const char *prefix, const char *name, size_t len);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* getNames method */
    int getp_getNames(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getHistory method */
    int getp_getHistory(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getRules method */
    int getp_getRules(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getLocation method */
    int getp_getLocation(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluation function table */
    static int (CVmObjTimeZone::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTimeZone metaclass registration table object 
 */
class CVmMetaclassTimeZone: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "timezone/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjTimeZone();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjTimeZone();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjTimeZone::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjTimeZone::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMTZOBJ_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjTimeZone)

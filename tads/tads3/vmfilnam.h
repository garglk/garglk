/* $Header$ */

/* 
 *   Copyright (c) 2012 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfilnam.h - CVmObjFileName object
Function
  A FileName represents a local filename string.  This is essentially just
  a string containing the filename, but provides methods to manipulate the
  name portably, using the various osifc path manipulation routines.
Notes
  
Modified
  03/03/12 MJRoberts  - Creation
*/

#ifndef VMFILNAM_H
#define VMFILNAM_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"


/* ------------------------------------------------------------------------ */
/*
 *   The image file data block is arranged as follows:
 *   
 *.  UINT2 special_id  = special file ID (0 for regular named files)
 *.  UINT2 str_len     = length of the string in byts
 *.  UTF8 str          = the filename string in universal notation
 *   
 *   If the special file ID is non-zero, the str_len and str fields aren't
 *   stored.
 */


/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension data structure.  For most instances, this is
 *   simply the filename string in our usual VMB_LEN prefix format.
 */
struct vm_filnam_ext
{
    /* allocate the structure */
    static vm_filnam_ext *alloc_ext(VMG_ class CVmObjFileName *self,
                                    int32_t sfid, const char *str, size_t len);
    
    /* get the length of the string, and a pointer to the string buffer */
    size_t get_len() { return vmb_get_len(str); }
    const char *get_str() { return str + VMB_LEN; }

    /* special file ID */
    int32_t sfid;

    /* flag: is the sfid valid? */
    char sfid_valid;

    /* 
     *   Flag: the filename was manually selected by the user through a file
     *   dialog.  If this is non-zero, it's an OS_AFP_xxx value giving the
     *   type of dialog that was presented to the user to obtain this
     *   filename.  Files selected by the user override the file safety
     *   settings, since the user implicitly grants us permission to use a
     *   file by manually selecting it.
     */
    char from_ui;

    /* 
     *   the actual data is in the same format as an ordinary internal string
     *   - a two-byte length prefix in little-endian order, followed by the
     *   UTF8 bytes of the string 
     */
    char str[1];
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjFileName intrinsic class definition
 */

class CVmObjFileName: public CVmObject
{
    friend class CVmMetaclassFileName;
    
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

    /* is this a CVmObjFileName object? */
    static int is_vmfilnam_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* create from URL notation */
    static vm_obj_id_t create_from_url(VMG_ const char *str, size_t len);

    /* create from a local path */
    static vm_obj_id_t create_from_local(VMG_ const char *str, size_t len);

    /* create from a special file ID (and its resolved local path) */
    static vm_obj_id_t create_from_sfid(
        VMG_ int32_t sfid, const char *path, size_t len);

    /* 
     *   Combine two path local paths into a new FileName object.  If
     *   'literal' is true, it means that we build the literal combined path,
     *   otherwise we build the canonical path (e.g., resolving '.' and '..'
     *   relative links). 
     */
    static vm_obj_id_t combine_path(
        VMG_ const char *path, size_t pathl, const char *file, size_t filel,
        int literal);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* compare filenames by filename string */
    virtual int equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                       int depth) const;

    /* add - this appends a path element */
    virtual int add_val(VMG_ vm_val_t *result,
                        vm_obj_id_t self, const vm_val_t *val);

    /* cast to string */
    virtual const char *cast_to_string(VMG_ vm_obj_id_t /*self*/,
                                       vm_val_t * /*new_str*/) const
        { return get_path_string(); }

    /* get my path string */
    const char *get_path_string() const;

    /* get my special file information */
    int is_special_file() const { return get_ext()->sfid != 0; }
    int32_t get_sfid() const { return get_ext()->sfid; }

    /* 
     *   Get/set the "from UI" flag.  If the user manually selected the file
     *   through a file dialog, this is an OS_AFP_xxx value specifying the
     *   type of dialog that was presented.
     */
    void set_from_ui(int typ) { get_ext()->from_ui = (char)typ; }
    int get_from_ui() const { return get_ext()->from_ui; }

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
    
    /* apply an undo record (we're immutable -> no undo) */
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    
    /* discard an undo record */
    void discard_undo(VMG_ struct CVmUndoRecord *) { }
    
    /* mark our undo record references */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark our references (we have none) */
    void mark_refs(VMG_ uint) { }

    /* remove weak references */
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

    /* determine if we've changed since loading; we're immutable, so no */
    int is_changed_since_load() const { return FALSE; }

protected:
    /* get my extension data */
    vm_filnam_ext *get_ext() const { return (vm_filnam_ext *)ext_; }

    /* get the local path for a given string or FileName value */
    static const char *get_local_path(VMG_ const vm_val_t *val);

    /* load or reload image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* create a with no initial contents */
    CVmObjFileName() { ext_ = 0; }

    /* create with contents */
    CVmObjFileName(VMG_ int32_t sfid, const char *str, size_t len);

    /* convert a URL string to a local path string */
    static char *url_to_local(VMG_ const char *str, size_t len, int nullterm);

    /* 
     *   convert my filename to universal format, returning an allocated
     *   buffer (which the caller must free with lib_free_str() when done) 
     */
    char *to_universal(VMG0_) const;

    /* 
     *   get the special file type flags - this returns FileTypeSelfLink or
     *   FileTypeParentLink, if applicable, based on the root name 
     */
    int32_t special_filetype_flags(VMG0_);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* getName method */
    int getp_getName(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getRootName method */
    int getp_getRootName(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getPath method */
    int getp_getPath(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* fromUniversal static method */
    int getp_fromUniversal(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_fromUniversal(vmg_ retval, argc); }

    /* fromUniversal static handler */
    static int s_getp_fromUniversal(VMG_ vm_val_t *retval, uint *argc);

    /* toUniversal method */
    int getp_toUniversal(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* addToPath method */
    int getp_addToPath(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* isAbsolute method */
    int getp_isAbsolute(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getAbsolutePath method */
    int getp_getAbsolutePath(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getRootDirs method */
    int getp_getRootDirs(VMG_ vm_obj_id_t, vm_val_t *retval, uint *argc)
        { return s_getp_getRootDirs(vmg_ retval, argc); }

    /* static getRootDirs method */
    static int s_getp_getRootDirs(VMG_ vm_val_t *retval, uint *argc);

    /* getFileType method */
    int getp_getFileType(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getFileInfo method */
    int getp_getFileInfo(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* deleteFile method */
    int getp_deleteFile(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* renameFile method */
    int getp_renameFile(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* getFilesInDir method */
    int getp_listDir(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* forEachFile method */
    int getp_forEachFile(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* createDirectory method */
    int getp_createDirectory(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* removeDirectory method */
    int getp_removeDirectory(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluation function table */
    static int (CVmObjFileName::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjFileName metaclass registration table object 
 */
class CVmMetaclassFileName: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "filename/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFileName();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjFileName();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjFileName::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjFileName::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMFILNAM_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjFileName)

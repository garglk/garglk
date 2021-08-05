/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfilobj.h - File object metaclass
Function
  Implements an intrinsic class interface to operating system file I/O.
Notes

Modified
  06/28/01 MJRoberts  - Creation
*/

#include <assert.h>
#include "t3std.h"
#include "charmap.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmtype.h"
#include "vmbif.h"
#include "vmcset.h"
#include "vmstack.h"
#include "vmmeta.h"
#include "vmrun.h"
#include "vmglob.h"
#include "vmfile.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmdate.h"
#include "vmfilobj.h"
#include "vmpredef.h"
#include "vmundo.h"
#include "vmbytarr.h"
#include "vmbignum.h"
#include "vmbiftad.h"
#include "vmhost.h"
#include "vmimage.h"
#include "vmnetfil.h"
#include "vmnet.h"
#include "vmfilnam.h"
#include "vmhash.h"
#include "vmpack.h"
#include "sha2.h"
#include "md5.h"



/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassFile metaclass_reg_obj;
CVmMetaclass *CVmObjFile::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjFile::
     *CVmObjFile::func_table_[])(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc) =
{
    &CVmObjFile::getp_undef,                                     /* index 0 */
    &CVmObjFile::getp_open_text,                                       /* 1 */
    &CVmObjFile::getp_open_data,                                       /* 2 */
    &CVmObjFile::getp_open_raw,                                        /* 3 */
    &CVmObjFile::getp_get_charset,                                     /* 4 */
    &CVmObjFile::getp_set_charset,                                     /* 5 */
    &CVmObjFile::getp_close_file,                                      /* 6 */
    &CVmObjFile::getp_read_file,                                       /* 7 */
    &CVmObjFile::getp_write_file,                                      /* 8 */
    &CVmObjFile::getp_read_bytes,                                      /* 9 */
    &CVmObjFile::getp_write_bytes,                                    /* 10 */
    &CVmObjFile::getp_get_pos,                                        /* 11 */
    &CVmObjFile::getp_set_pos,                                        /* 12 */
    &CVmObjFile::getp_set_pos_end,                                    /* 13 */
    &CVmObjFile::getp_open_res_text,                                  /* 14 */
    &CVmObjFile::getp_open_res_raw,                                   /* 15 */
    &CVmObjFile::getp_get_size,                                       /* 16 */
    &CVmObjFile::getp_get_mode,                                       /* 17 */
    &CVmObjFile::getp_getRootName,                                    /* 18 */
    &CVmObjFile::getp_deleteFile,                                     /* 19 */
    &CVmObjFile::getp_setMode,                                        /* 20 */
    &CVmObjFile::getp_packBytes,                                      /* 21 */
    &CVmObjFile::getp_unpackBytes,                                    /* 22 */
    &CVmObjFile::getp_sha256,                                         /* 23 */
    &CVmObjFile::getp_digestMD5                                       /* 24 */
};

/*
 *   Vector indices - we only need to define these for the static functions,
 *   since we only need them to decode calls to call_stat_prop().  
 */
enum vmobjfil_meta_fnset
{
    VMOBJFILE_OPEN_TEXT = 1,
    VMOBJFILE_OPEN_DATA = 2,
    VMOBJFILE_OPEN_RAW = 3,
    VMOBJFILE_OPEN_RES_TEXT = 14,
    VMOBJFILE_OPEN_RES_RAW = 15,
    VMOBJFILE_GET_ROOT_NAME = 18,
    VMOBJFILE_DELETE_FILE = 19,
};

/* ------------------------------------------------------------------------ */
/*
 *   Create from stack 
 */
vm_obj_id_t CVmObjFile::create_from_stack(VMG_ const uchar **pc_ptr,
                                          uint argc)
{
    /* 
     *   we can't be created with 'new' - we can only be created via our
     *   static creator methods (openTextFile, openDataFile, openRawFile) 
     */
    err_throw(VMERR_BAD_DYNAMIC_NEW);

    /* not reached, but the compiler might not know that */
    AFTER_ERR_THROW(return VM_INVALID_OBJ;)
}

/* ------------------------------------------------------------------------ */
/*
 *   Create with no contents 
 */
vm_obj_id_t CVmObjFile::create(VMG_ int in_root_set)
{
    /* instantiate the object */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjFile();

    /* files are always transient */
    G_obj_table->set_obj_transient(id);

    /* return the new ID */
    return id;
}

/*
 *   Create with the given character set object and file handle.  
 */
vm_obj_id_t CVmObjFile::create(VMG_ int in_root_set,
                               CVmNetFile *netfile,
                               vm_obj_id_t charset, CVmDataSource *fp,
                               int mode, int access, int create_readbuf)
{
    /* instantiate the object */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
    new (vmg_ id) CVmObjFile(
        vmg_ netfile, charset, fp, mode, access, create_readbuf);

    /* files are always transient */
    G_obj_table->set_obj_transient(id);

    /* return the ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Instantiate 
 */
CVmObjFile::CVmObjFile(VMG_ CVmNetFile *netfile,
                       vm_obj_id_t charset, CVmDataSource *fp,
                       int mode, int access, int create_readbuf)
{
    /* allocate and initialize our extension */
    ext_ = 0;
    alloc_ext(vmg_ netfile, charset, fp, 0, mode, access, create_readbuf);
}

/*
 *   Allocate and initialize our extension 
 */
void CVmObjFile::alloc_ext(VMG_ CVmNetFile *netfile,
                           vm_obj_id_t charset, CVmDataSource *fp,
                           unsigned long flags, int mode, int access,
                           int create_readbuf)
{
    /* 
     *   if we already have an extension, delete it (and release our
     *   underlying system file, if any) 
     */
    notify_delete(vmg_ FALSE);

    /* 
     *   Figure the needed size.  We need at least the standard extension;
     *   if we also need a read buffer, figure in space for that as well. 
     */
    size_t siz = sizeof(vmobjfile_ext_t);
    if (create_readbuf)
        siz += sizeof(vmobjfile_readbuf_t);

    /* allocate space for our extension structure */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(siz, this);

    /* store the data we received from the caller */
    get_ext()->netfile = netfile;
    get_ext()->fp = fp;
    get_ext()->charset = charset;
    get_ext()->flags = flags;
    get_ext()->mode = (unsigned char)mode;
    get_ext()->access = (unsigned char)access;

    /* 
     *   point to our read buffer, for which we allocated space contiguously
     *   with and immediately following our extension, if we have one 
     */
    if (create_readbuf)
    {
        /* point to our read buffer object */
        vmobjfile_readbuf_t *rb = (vmobjfile_readbuf_t *)(get_ext() + 1);
        get_ext()->readbuf = rb;

        /* initialize the read buffer with no initial data */
        rb->rem = 0;
        rb->ptr = rb->buf;
    }
    else
    {
        /* there's no read buffer at all */
        get_ext()->readbuf = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Notify of deletion 
 */
void CVmObjFile::notify_delete(VMG_ int /*in_root_set*/)
{
    /* if we have an extension, clean it up */
    if (ext_ != 0)
    {
        /* close our file if we have one */
        if (get_ext()->fp != 0)
            delete get_ext()->fp;

        /* close the network file */
        err_try
        {
            if (get_ext()->netfile != 0)
                get_ext()->netfile->close(vmg0_);
        }
        err_catch_disc
        {
            /* 
             *   Since we're being deleted by the garbage collector, there's
             *   no way to relay this error to the bytecode program - it
             *   evidently lost track of the file, so it must not care about
             *   the ending disposition.  Just discard the error.
             */
        }
        err_end;

        /* free our extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the data source 
 */
CVmDataSource *CVmObjFile::get_datasource() const
{
    return get_ext()->fp;
}

/*
 *   Get the file spec from the netfile 
 */
void CVmObjFile::get_filespec(vm_val_t *val) const
{
    /* presume we won't find a value */
    val->set_nil();

    /* if we have a net file object, get its filespec */
    if (get_ext()->netfile != 0)
        val->set_obj(get_ext()->netfile->filespec);
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjFile::set_prop(VMG_ class CVmUndo *,
                             vm_obj_id_t, vm_prop_id_t,
                             const vm_val_t *)
{
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjFile::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                            vm_obj_id_t self, vm_obj_id_t *source_obj,
                            uint *argc)
{
    uint func_idx;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* 
 *   call a static property 
 */
int CVmObjFile::call_stat_prop(VMG_ vm_val_t *result,
                               const uchar **pc_ptr, uint *argc,
                               vm_prop_id_t prop)
{
    /* look up the function */
    uint midx = G_meta_table
                ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* 
     *   set up a blank recursive call descriptor, to be filled in when we
     *   know which function we're calling 
     */
    vm_rcdesc rc(vmg_ "",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 (ushort)midx, G_stk->get(0), argc != 0 ? *argc : 0);

    /* translate the property into a function vector index */
    switch(midx)
    {
    case VMOBJFILE_OPEN_TEXT:
        rc.name = "File.openTextFile";
        return s_getp_open_text(vmg_ result, argc, FALSE, &rc);

    case VMOBJFILE_OPEN_DATA:
        rc.name = "File.openDataFile";
        return s_getp_open_data(vmg_ result, argc, &rc);

    case VMOBJFILE_OPEN_RAW:
        rc.name = "File.openRawFile";
        return s_getp_open_raw(vmg_ result, argc, FALSE, &rc);

    case VMOBJFILE_OPEN_RES_TEXT:
        rc.name = "File.openTextResource";
        return s_getp_open_text(vmg_ result, argc, TRUE, &rc);

    case VMOBJFILE_OPEN_RES_RAW:
        rc.name = "File.openRawResource";
        return s_getp_open_raw(vmg_ result, argc, TRUE, &rc);

    case VMOBJFILE_GET_ROOT_NAME:
        return s_getp_getRootName(vmg_ result, argc);

    case VMOBJFILE_DELETE_FILE:
        return s_getp_deleteFile(vmg_ result, argc);

    default:
        /* it's not one of ours - inherit from the base object metaclass */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Static "open" methods 
 */

int CVmObjFile::getp_open_text(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc)
{
    vm_rcdesc rc(vmg_ "File.openTextFile",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 VMOBJFILE_OPEN_TEXT, G_stk->get(0), argc);
    return s_getp_open_text(vmg_ retval, argc, FALSE, &rc);
}

int CVmObjFile::getp_open_data(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc)
{
    vm_rcdesc rc(vmg_ "File.openDataFile",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 VMOBJFILE_OPEN_DATA, G_stk->get(0), argc);
    return s_getp_open_data(vmg_ retval, argc, &rc);
}

int CVmObjFile::getp_open_raw(VMG_ vm_obj_id_t self,
                              vm_val_t *retval, uint *argc)
{
    vm_rcdesc rc(vmg_ "File.openRawFile",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 VMOBJFILE_OPEN_RAW, G_stk->get(0), argc);
    return s_getp_open_raw(vmg_ retval, argc, FALSE, &rc);
}

int CVmObjFile::getp_open_res_text(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *argc)
{
    vm_rcdesc rc(vmg_ "File.openTextResource",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 VMOBJFILE_OPEN_RES_TEXT, G_stk->get(0), argc);
    return s_getp_open_text(vmg_ retval, argc, TRUE, &rc);
}

int CVmObjFile::getp_open_res_raw(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *argc)
{
    vm_rcdesc rc(vmg_ "File.openRawResource",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 VMOBJFILE_OPEN_RES_RAW, G_stk->get(0), argc);
    return s_getp_open_raw(vmg_ retval, argc, TRUE, &rc);
}



/* ------------------------------------------------------------------------ */
/*
 *   load from an image file 
 */
void CVmObjFile::load_from_image(VMG_ vm_obj_id_t self,
                                 const char *ptr, size_t siz)
{
    /* load from the image data */
    load_image_data(vmg_ ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload the object from image data 
 */
void CVmObjFile::reload_from_image(VMG_ vm_obj_id_t /*self*/,
                                   const char *ptr, size_t siz)
{
    /* load the image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or re-load image data 
 */
void CVmObjFile::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* read our character set */
    vm_obj_id_t charset = vmb_get_objid(ptr);
    ptr += VMB_OBJECT_ID;

    /* get the mode and access values */
    int mode = (unsigned char)*ptr++;
    int access = (unsigned char)*ptr++;

    /* get the flags */
    unsigned long flags = t3rp4u(ptr);

    /* 
     *   add in the out-of-sync flag, since we've restored the state from a
     *   past state and thus we're out of sync with the external file system
     *   environment 
     */
    flags |= VMOBJFILE_OUT_OF_SYNC;

    /* 
     *   Initialize our extension - we have no underlying network file
     *   descriptor or native file handle, since the file is out of sync by
     *   virtue of being loaded from a previously saved image state.  Note
     *   that we don't need a read buffer because the file is inherently out
     *   of sync and thus cannot be read.  
     */
    alloc_ext(vmg_ 0, charset, 0, flags, mode, access, FALSE);
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjFile::save_to_file(VMG_ class CVmFile *fp)
{
    /* files are always transient, so should never be saved */
    assert(FALSE);
}

/* 
 *   restore from a file 
 */
void CVmObjFile::restore_from_file(VMG_ vm_obj_id_t self,
                                   CVmFile *fp, CVmObjFixup *)
{
    /* files are always transient, so should never be saved */
}

/* ------------------------------------------------------------------------ */
/*
 *   Mark as referenced all of the objects to which we refer 
 */
void CVmObjFile::mark_refs(VMG_ uint state)
{
    /* mark our character set object, if we have one */
    if (get_ext()->charset != VM_INVALID_OBJ)
        G_obj_table->mark_all_refs(get_ext()->charset, state);

    /* mark our network file descriptor's references */
    if (get_ext()->netfile != 0)
        get_ext()->netfile->mark_refs(vmg_ state);
}

/* ------------------------------------------------------------------------ */
/*
 *   Note that we're seeking within the file.
 */
void CVmObjFile::note_file_seek(VMG_ int is_explicit)
{
    /* 
     *   if it's an explicit seek, invalidate our internal read buffer and
     *   note that the stdio buffers have been invalidated 
     */
    if (is_explicit)
    {
        /* the read buffer (if we have one) is invalid after a seek */
        if (get_ext()->readbuf != 0)
            get_ext()->readbuf->rem = 0;

        /* 
         *   mark the last operation as clearing stdio buffering - when we
         *   explicitly seek, stdio automatically invalidates its internal
         *   buffers 
         */
        get_ext()->flags &= ~VMOBJFILE_STDIO_BUF_DIRTY;
    }
}

/*
 *   Update stdio buffering for a switch between read and write mode, if
 *   necessary.  Any time we perform consecutive read and write operations,
 *   stdio requires us to explicitly seek when performing consecutive
 *   dissimilar operations.  In other words, if we read then write, we must
 *   seek before the write, and likewise if we write then read.  
 */
void CVmObjFile::switch_read_write_mode(VMG_ int writing)
{
    /* if we're writing, invalidate the read buffer */
    if (writing && get_ext()->readbuf != 0)
        get_ext()->readbuf->rem = 0;

    /* 
     *   if we just performed a read or write operation, we must seek if
     *   we're performing the opposite type of operation now 
     */
    if ((get_ext()->flags & VMOBJFILE_STDIO_BUF_DIRTY) != 0)
    {
        int was_writing;

        /* check what type of operation we did last */
        was_writing = ((get_ext()->flags & VMOBJFILE_LAST_OP_WRITE) != 0);

        /* 
         *   if we're switching operations, explicitly seek to the current
         *   location to flush the stdio buffers 
         */
        if ((writing && !was_writing) || (!writing && was_writing))
            get_ext()->fp->seek(get_pos(vmg0_), OSFSK_SET);
    }

    /* 
     *   remember that this operation is stdio-buffered, so that we'll know
     *   we need to seek if we perform the opposite operation after this one 
     */
    get_ext()->flags |= VMOBJFILE_STDIO_BUF_DIRTY;

    /* remember which type of operation we're performing */
    if (writing)
        get_ext()->flags |= VMOBJFILE_LAST_OP_WRITE;
    else
        get_ext()->flags &= ~VMOBJFILE_LAST_OP_WRITE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Retrieve the filename argument, and optionally the access mode argument.
 *   Sets up network storage server access to the file, if applicable.
 *   Returns a network/local file descriptor that the caller can use to open
 *   the local file system object. 
 *   
 *   If '*access' is zero at entry, we'll retrieve the access argument from
 *   the stack for a non-resource file.  (Resource files always use access
 *   mode READ, so there's no extra argument for those.)  If '*access' is
 *   non-zero, we'll simply use the given access mode.
 *   
 *   This leaves the argument values on the stack - the caller must discard
 *   when done with them.  
 */
CVmNetFile *CVmObjFile::get_filename_and_access(
    VMG_ const vm_rcdesc *rc, int *access, int is_resource_file,
    os_filetype_t file_type, const char *mime_type)
{
    /*
     *   If it's a resource file, the access mode is always READ, and there's
     *   no access mode argument.  If the caller provided a non-zero access
     *   mode, there's also no argument - we just use the caller's mode.  If
     *   the caller specified zero as the mode, it means we have to read the
     *   mode from an extra integer argument.  
     */
    if (is_resource_file)
    {
        /* resource file - the only access mode possible is READ */
        *access = VMOBJFILE_ACCESS_READ;
    }
    else if (*access == 0)
    {
        /* get the access mode from the stack */
        *access = G_stk->get(1)->num_to_int(vmg0_);

        /* make sure it's within the valid range for bytecode access codes */
        if (*access > VMOBJFILE_ACCESS_USERMAX)
            err_throw(VMERR_BAD_VAL_BIF);
    }

    /* get the filename from the first argument */
    return get_filename_arg(
        vmg_ G_stk->get(0), rc, *access, is_resource_file,
        file_type, mime_type);
}

/*
 *   Get the filename for a given argument value and access mode
 */
CVmNetFile *CVmObjFile::get_filename_from_obj(
    VMG_ vm_obj_id_t obj, const vm_rcdesc *rc, int access,
    os_filetype_t file_type, const char *mime_type)
{
    vm_val_t arg;
    arg.set_obj(obj);
    return get_filename_arg(vmg_ &arg, rc, access,
                            FALSE, file_type, mime_type);
}

/*
 *   Get the filename for a given argument value and access mode
 */
CVmNetFile *CVmObjFile::get_filename_arg(
    VMG_ const vm_val_t *arg, const vm_rcdesc *rc,
    int access, int is_resource_file,
    os_filetype_t file_type, const char *mime_type)
{
    /* figure the network file access mode */
    int nmode;
    switch (access)
    {
    case VMOBJFILE_ACCESS_READ:
        /* read access; file must exist */
        nmode = NETF_READ;
        break;

    case VMOBJFILE_ACCESS_WRITE:
        /* write access; create or replace existing file */ 
        nmode = NETF_WRITE | NETF_CREATE | NETF_TRUNC;
        break;

    case VMOBJFILE_ACCESS_RW_KEEP:
        /* read/write access; keep an existing file or create a new one */
        nmode = NETF_READ | NETF_WRITE | NETF_CREATE;
        break;

    case VMOBJFILE_ACCESS_RW_TRUNC:
        /* read/write access; truncate an existing file or create a new one */
        nmode = NETF_READ | NETF_WRITE | NETF_CREATE | NETF_TRUNC;
        break;

    case VMOBJFILE_ACCESS_DELETE:
        /* delete mode */
        nmode = NETF_DELETE;
        break;

    case VMOBJFILE_ACCESS_GETINFO:
    case VMOBJFILE_ACCESS_MKDIR:
    case VMOBJFILE_ACCESS_RMDIR:
    case VMOBJFILE_ACCESS_READDIR:
    case VMOBJFILE_ACCESS_RENAME_TO:
    case VMOBJFILE_ACCESS_RENAME_FROM:
        /* 
         *   getFileType/getFileInfo/rename/directory manipulation - we don't
         *   need to open the file at all for these operations
         */
        nmode = 0;
        break;

    default:
        /* invalid mode */
        err_throw(VMERR_BAD_VAL_BIF);
    }

    /* we don't have a network file descriptor yet */
    CVmNetFile *netfile = 0;
    char fname[OSFNMAX];

    /* check to see what kind of file spec we have */
    if (is_resource_file)
    {
        /* a resource file must be given as a string name - retrieve it */
        CVmBif::get_str_val_fname(vmg_ fname, sizeof(fname),
                                  CVmBif::get_str_val(vmg_ arg));

        /* resources are always local, so create a local file descriptor */
        netfile = CVmNetFile::open_local(vmg_ fname, 0, nmode, file_type);
    }
    else
    {
        /* regular file - create the network file descriptor */
        netfile = CVmNetFile::open(
            vmg_ arg, rc, nmode, file_type, mime_type);
    }
    
    /* 
     *   If this isn't a resource file, check the file safety mode to see if
     *   the operation is allowed.  Reading resources is always allowed,
     *   regardless of the safety mode, since resources are read-only and are
     *   inherently constrained in the paths they can access.  
     */
    if (!is_resource_file)
    {
        err_try
        {
            check_safety_for_open(vmg_ netfile, access);
        }
        err_catch_disc
        {
            /* abandon the net file descriptor and bubble up the error */
            netfile->abandon(vmg0_);
            err_rethrow();
        }
        err_end;
    }

    /* return the network file descriptor */
    return netfile;
}

/*
 *   Resolve a special file ID to a local filename path 
 */
int CVmObjFile::sfid_to_path(VMG_ char *buf, size_t buflen, int32_t sfid)
{
    const char *fname = 0;
    char path[OSFNMAX];
    switch (sfid)
    {
    case SFID_LIB_DEFAULTS:
        /* library defaults file - T3_APP_DATA/settings.txt */
        fname = "settings.txt";
        goto app_data_file;
        
    case SFID_WEBUI_PREFS:
        /* Web UI preferences - T3_APP_DATA/webprefs.txt */
        fname = "webprefs.txt";
        goto app_data_file;
        
    app_data_file:
        /* get the system application data path */
        G_host_ifc->get_special_file_path(
            path, sizeof(path), OS_GSP_T3_APP_DATA);
        
        /* add the filename */
        os_build_full_path(buf, buflen, path, fname);
        
        /* success */
        return TRUE;
        
    default:
        /* invalid ID value */
        buf[0] = '\0';
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Pop a character set mapper argument.  If there's no argument, we'll
 *   create and return a mapper for the default character set. 
 */
vm_obj_id_t CVmObjFile::get_charset_arg(VMG_ int argn, int argc)
{
    /* presume we won't find a valid argument value */
    vm_obj_id_t obj = VM_INVALID_OBJ;

    /* if there's an argument, pop it */
    if (argn < argc)
        obj = CVmBif::get_charset_obj(vmg_ argn);
    
    /* if we don't have a character set yet, create the default */
    if (obj == VM_INVALID_OBJ)
    {
        /* get the default local character set for file contents */
        char csname[32];
        os_get_charmap(csname, OS_CHARMAP_FILECONTENTS);
        
        /* create the mapper */
        obj = CVmObjCharSet::create(vmg_ FALSE, csname, strlen(csname));
    }

    /* return the character set */
    return obj;
}


/* ------------------------------------------------------------------------ */
/*
 *   Static property evaluator - open a text file 
 */
int CVmObjFile::s_getp_open_text(VMG_ vm_val_t *retval, uint *in_argc,
                                 int is_resource_file, const vm_rcdesc *rc)
{
    /* check arguments */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc_file(2, 1);
    static CVmNativeCodeDesc desc_res(1, 1);
    if (get_prop_check_argc(retval, in_argc,
                            is_resource_file ? &desc_res : &desc_file))
        return TRUE;

    /* 
     *   retrieve the filename and access mode arguments, and set up network
     *   storage server access if applicable 
     */
    int access = 0;
    CVmNetFile *netfile = get_filename_and_access(
        vmg_ rc, &access, is_resource_file, OSFTTEXT, "text/plain");

    err_try
    {
        /* presume we won't need a read buffer */
        int create_readbuf = FALSE;

        /* retrieve the character set object */
        vm_obj_id_t cset_obj = get_charset_arg(vmg_ 2, argc);

        /* push the character map object onto the stack for gc protection */
        G_stk->push()->set_obj(cset_obj);
        
        /* open the file for reading or writing, as appropriate */
        CVmDataSource *ds = 0;
        osfildef *fp = 0;
        switch(access)
        {
        case VMOBJFILE_ACCESS_READ:
            /* open a resource file or file system file, as appropriate */
            if (is_resource_file)
            {
                /* it's a resource - open it */
                unsigned long res_len;
                fp = G_host_ifc->find_resource(
                    netfile->lclfname, strlen(netfile->lclfname), &res_len);
                
                /* 
                 *   if we found the resource, note the start and end seek
                 *   positions, so we can limit reading of the underlying
                 *   file to the section that contains the resource data 
                 */
                if (fp != 0)
                {
                    /* 
                     *   find_resource() handed us back a file handle
                     *   positioned at the start of the resource data stream,
                     *   possibly within a larger file; note the current
                     *   offset as the starting offset in case we need to
                     *   seek within the resource later 
                     */
                    long res_start = osfpos(fp);
                    
                    /* 
                     *   note where the resource ends - it might be embedded
                     *   within a larger file, so we need to limit reading to
                     *   the extent from the resource map 
                     */
                    long res_end = res_start + res_len;
                    
                    /* create the data source */
                    ds = new CVmResFileSource(fp, res_start, res_end);
                }
            }
            else
            {
                /* 
                 *   It's not a resource - open an ordinary text file for
                 *   reading.  Even though we're going to treat the file as a
                 *   text file, open it in binary mode, since we'll do our
                 *   own universal newline translations; this allows us to
                 *   work with files in any character set, and using almost
                 *   any newline conventions, so files copied from other
                 *   systems will be fully usable even if they haven't been
                 *   fixed up to local conventions.  
                 */
                fp = osfoprb(netfile->lclfname, OSFTTEXT);

                /* if that worked, create the data source */
                if (fp != 0)
                    ds = new CVmFileSource(fp);
            }
            
            /* make sure we opened it successfully */
            if (fp == 0)
                G_interpreter->throw_new_class(
                    vmg_ G_predef->file_not_found_exc,
                    0, "file not found");

            /* we need a read buffer */
            create_readbuf = TRUE;
            break;
            
        case VMOBJFILE_ACCESS_WRITE:
            /* open for writing */
            fp = osfopwb(netfile->lclfname, OSFTTEXT);
            
            /* make sure we created it successfully */
            if (fp == 0)
                G_interpreter->throw_new_class(
                    vmg_ G_predef->file_creation_exc,
                    0, "error creating file");

            /* create the data source */
            ds = new CVmFileSource(fp);
            break;

        case VMOBJFILE_ACCESS_RW_KEEP:
            /* open for read/write, keeping existing contents */
            fp = osfoprwb(netfile->lclfname, OSFTTEXT);
            
            /* make sure we were able to find or create the file */
            if (fp == 0)
                G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                               0, "error opening file");
            
            /* create the data source */
            ds = new CVmFileSource(fp);

            /* we need a read buffer */
            create_readbuf = TRUE;
            break;
            
        case VMOBJFILE_ACCESS_RW_TRUNC:
            /* open for read/write, truncating existing contents */
            fp = osfoprwtb(netfile->lclfname, OSFTTEXT);
            
            /* make sure we were successful */
            if (fp == 0)
                G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                               0, "error opening file");
            
            /* create the data source */
            ds = new CVmFileSource(fp);

            /* we need a read buffer */
            create_readbuf = TRUE;
            break;
            
        default:
            /* invalid access mode */
            fp = 0;
            err_throw(VMERR_BAD_VAL_BIF);
        }
        
        /* create our file object */
        retval->set_obj(create(vmg_ FALSE, netfile, cset_obj, ds,
                               VMOBJFILE_MODE_TEXT, access, create_readbuf));

        /* 
         *   the network file descriptor is stored in the File object now, so
         *   we no longer need to delete it 
         */
        netfile = 0;

        /* discard arguments and gc protection */
        G_stk->discard(argc + 1);
    }
    err_finally
    {
        /* 
         *   If we created a network file descriptor and didn't hand it off
         *   to the File object, we're abandoning the object.  
         */
        if (netfile != 0)
            netfile->abandon(vmg0_);
    }
    err_end;

    /* handled */
    return TRUE;
}

/*
 *   Static property evaluator - open a data file
 */
int CVmObjFile::s_getp_open_data(VMG_ vm_val_t *retval, uint *argc,
                                 const vm_rcdesc *rc)
{
    /* use the generic binary file opener in 'data' mode */
    return open_binary(vmg_ retval, argc, VMOBJFILE_MODE_DATA, FALSE, rc);
}

/*
 *   Static property evaluator - open a raw file
 */
int CVmObjFile::s_getp_open_raw(VMG_ vm_val_t *retval, uint *argc,
                                int is_resource_file, const vm_rcdesc *rc)
{
    /* use the generic binary file opener in 'raw' mode */
    return open_binary(vmg_ retval, argc, VMOBJFILE_MODE_RAW,
                       is_resource_file, rc);
}

/*
 *   Generic binary file opener - common to 'data' and 'raw' files 
 */
int CVmObjFile::open_binary(VMG_ vm_val_t *retval, uint *in_argc, int mode,
                            int is_resource_file, const vm_rcdesc *rc)
{
    /* check arguments */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc file_desc(2);
    static CVmNativeCodeDesc res_desc(1);
    if (get_prop_check_argc(retval, in_argc,
                            is_resource_file ? &res_desc : &file_desc))
        return TRUE;

    /* retrieve the filename and access mode */
    int access = 0;
    CVmNetFile *netfile = get_filename_and_access(
        vmg_ rc, &access, is_resource_file,
        OSFTBIN, "application/octet-stream");

    err_try
    {
        osfildef *fp;
        CVmDataSource *ds = 0;

        /* open the file in binary mode, with the desired access type */
        switch(access)
        {
        case VMOBJFILE_ACCESS_READ:
            /* open the resource or ordinary file, as appropriate */
            if (is_resource_file)
            {
                unsigned long res_len;
                
                /* it's a resource - open it */
                fp = G_host_ifc->find_resource(
                    netfile->lclfname, strlen(netfile->lclfname), &res_len);

                /* 
                 *   if we found the resource, note the start and end seek
                 *   positions, so we can limit reading of the underlying
                 *   file to the section that contains the resource data 
                 */
                if (fp != 0)
                {
                    /* 
                     *   find_resource() hands us a file handle positioned at
                     *   the start of the resource data - note the seek
                     *   offset in case we need to seek around within the
                     *   resource stream later on 
                     */
                    long res_start = osfpos(fp);
                    
                    /* 
                     *   note the end of the resource, in case it's embedded
                     *   within a larger physical file 
                     */
                    long res_end = res_start + res_len;
                    
                    /* create the data source */
                    ds = new CVmResFileSource(fp, res_start, res_end);
                }
            }
            else
            {
                /* open the ordinary file in binary mode for reading only */
                fp = osfoprb(netfile->lclfname, OSFTBIN);
                
                /* create the data source */
                if (fp != 0)
                    ds = new CVmFileSource(fp);
            }
            
            /* make sure we were able to find it and open it */
            if (fp == 0)
                G_interpreter->throw_new_class(
                    vmg_ G_predef->file_not_found_exc, 0, "file not found");
            break;
            
        case VMOBJFILE_ACCESS_WRITE:
            /* open in binary mode for writing only */
            fp = osfopwb(netfile->lclfname, OSFTBIN);
            
            /* make sure we were able to create the file successfully */
            if (fp == 0)
                G_interpreter->throw_new_class(
                    vmg_ G_predef->file_creation_exc,
                    0, "error creating file");
            
            /* create the data source */
            ds = new CVmFileSource(fp);
            break;
            
        case VMOBJFILE_ACCESS_RW_KEEP:
            /* open for read/write, keeping existing contents */
            fp = osfoprwb(netfile->lclfname, OSFTBIN);
            
            /* make sure we were able to find or create the file */
            if (fp == 0)
                G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                               0, "error opening file");
            
            /* create the data source */
            ds = new CVmFileSource(fp);
            break;
            
        case VMOBJFILE_ACCESS_RW_TRUNC:
            /* open for read/write, truncating existing contents */
            fp = osfoprwtb(netfile->lclfname, OSFTBIN);
            
            /* make sure we were successful */
            if (fp == 0)
                G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                               0, "error opening file");
            
            /* create the data source */
            ds = new CVmFileSource(fp);
            break;
            
        default:
            fp = 0;
            err_throw(VMERR_BAD_VAL_BIF);
        }
        
        /* create our file object */
        retval->set_obj(create(
            vmg_ FALSE, netfile, VM_INVALID_OBJ, ds, mode, access, FALSE));

        /* 
         *   we've handed off the network file descriptor to the File object,
         *   so we no longer have responsibility for it 
         */
        netfile = 0;
    }
    err_finally
    {
        /* 
         *   if we created a network file descriptor and didn't hand it off
         *   to the new File object, we're abandoning it 
         */
        if (netfile != 0)
            netfile->abandon(vmg0_);
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check the safety settings to determine if an open operation is allowed.
 *   If the access is not allowed, we'll throw an error.  
 */
void CVmObjFile::check_safety_for_open(VMG_ CVmNetFile *nf, int access)
{
    /* 
     *   Special files are inherently sandboxed, since the system determines
     *   their paths.  These are always allowed for ordinary read/write.
     */
    if (nf->sfid != 0
        && (access == VMOBJFILE_ACCESS_READ
            || access == VMOBJFILE_ACCESS_WRITE
            || access == VMOBJFILE_ACCESS_RW_KEEP
            || access == VMOBJFILE_ACCESS_RW_TRUNC
            || access == VMOBJFILE_ACCESS_GETINFO
            || access == VMOBJFILE_ACCESS_READDIR))
        return;

    /*
     *   Temporary file paths can only be obtained from the VM.  Ordinary
     *   read/write operations are allowed, as is delete. 
     */
    if (nf->is_temp
        && (access == VMOBJFILE_ACCESS_READ
            || access == VMOBJFILE_ACCESS_WRITE
            || access == VMOBJFILE_ACCESS_RW_KEEP
            || access == VMOBJFILE_ACCESS_RW_TRUNC
            || access == VMOBJFILE_ACCESS_DELETE
            || access == VMOBJFILE_ACCESS_GETINFO
            || access == VMOBJFILE_ACCESS_READDIR))
        return;

    /*
     *   Network files are subject to separate permission rules enforced by
     *   the storage server.  Local file safety settings don't apply. 
     */
    if (nf->is_net_file())
        return;
    
    /*   
     *   If the file was specifically selected by the user via a file dialog,
     *   allow access of the type proposed by the dialog, even if it's
     *   outside the sandbox.  The user has implicitly granted us permission
     *   on this individual file by manually selecting it via a UI
     *   interaction.
     */
    CVmObjFileName *ofn;
    if ((ofn = vm_objid_cast(CVmObjFileName, nf->filespec)) != 0
        && ofn->get_from_ui() != 0)
    {
        /* grant permission based on the dialog mode */
        switch (ofn->get_from_ui())
        {
        case OS_AFP_OPEN:
            /* allow reading for a file selected with an Open dialog */
            if (access == VMOBJFILE_ACCESS_READ)
                return;
            break;

        case OS_AFP_SAVE:
            /* allow writing for a file selected with a Save dialog */
            if (access == VMOBJFILE_ACCESS_WRITE)
                return;
            break;
        }

        /* 
         *   For anything we didn't override above based on the dialog mode,
         *   don't give up - the file safety permissions might still allow
         *   it.  All we've determined at this point is that the dialog
         *   selection doesn't grant extraordinary permission for the
         *   attempted access mode.
         */
    }

    /*
     *   The file wasn't specified by a type that confers any special
     *   privileges, so determine if it's accessible according to the local
     *   file name's handling under the file safety and sandbox rules.
     */
    return check_safety_for_open(vmg_ nf->lclfname, access);
}

/*
 *   Check file safety for opening the given local file path. 
 */
void CVmObjFile::check_safety_for_open(VMG_ const char *lclfname, int access)
{
    /* check the safety settings */
    if (!query_safety_for_open(vmg_ lclfname, access))
    {
        /* this operation is not allowed - throw an error */
        G_interpreter->throw_new_class(vmg_ G_predef->file_safety_exc,
                                       0, "prohibited file access");
    }
}

/*
 *   Query the safety settings.  Returns true if the access is allowed, false
 *   if it's prohibited. 
 */
int CVmObjFile::query_safety_for_open(VMG_ const char *lclfname, int access)
{
    /* get the current file safety level from the host application */
    int read_level = G_host_ifc->get_io_safety_read();
    int write_level = G_host_ifc->get_io_safety_write();

    /* 
     *   First check to see if the safety level allows or disallows the
     *   requested access without regard to location.  If so, we can save a
     *   little work by skipping the sandbox resolution.
     */
    switch (access)
    {
    case VMOBJFILE_ACCESS_READ:
    case VMOBJFILE_ACCESS_GETINFO:
    case VMOBJFILE_ACCESS_READDIR:
        /* 
         *   Read a file or its metadata.  If the safety level is READ_ANY or
         *   below, we can read any file regardless of location.  If it's
         *   MAXIMUM, we can't read anything.
         */
        if (read_level <= VM_IO_SAFETY_READ_ANY_WRITE_CUR)
            return TRUE;
        if (read_level >= VM_IO_SAFETY_MAXIMUM)
            return FALSE;
        break;

    case VMOBJFILE_ACCESS_RMDIR:
    case VMOBJFILE_ACCESS_WRITE:
    case VMOBJFILE_ACCESS_RW_KEEP:
    case VMOBJFILE_ACCESS_RW_TRUNC:
    case VMOBJFILE_ACCESS_DELETE:
    case VMOBJFILE_ACCESS_MKDIR:
    case VMOBJFILE_ACCESS_RENAME_FROM:
    case VMOBJFILE_ACCESS_RENAME_TO:
        /* 
         *   Writing, deleting, renaming, creating/removing a directory.  If
         *   the safety level is MINIMUM, we're allowed to write anywhere.
         *   If it's READ_CUR or higher, writing isn't allowed anywhere.
         */
        if (write_level <= VM_IO_SAFETY_MINIMUM)
            return TRUE;
        if (write_level >= VM_IO_SAFETY_READ_CUR)
            return FALSE;
        break;
    }

    /* 
     *   If we get this far, it means we're in sandbox mode, so the access is
     *   conditional on the file's location relative to the sandbox folder.
     *   Get the sandbox path, defaulting to the file base path.
     */
    const char *sandbox = (G_sandbox_path != 0 ? G_sandbox_path : G_file_path);

    /* assume the file isn't in the sandbox */
    int in_sandbox = FALSE;

    /* 
     *   Most operations in sandbox mode don't consider the sandbox directory
     *   itself to be part of the sandbox - e.g., you can't delete the
     *   sandbox folder. 
     */
    int match_self = FALSE;

    /*
     *   The effective sandbox extends beyond the actual contents the sandbox
     *   directory for a couple of modes:
     *   
     *   - For READDIR mode (reading the contents of a directory), the
     *   sandbox directory itself is considered part of the sandbox, since
     *   the information we're accessing in creating a directory listing is
     *   really the directory's contents.
     *   
     *   - For GETINFO mode (read file metadata), extend the sandbox to
     *   include the sandbox directory itself, its parent, its parent's
     *   parent, etc.  It's pretty obvious that we should have metadata read
     *   access to the sandbox directory itself, given that we can list and
     *   access its contents.  The rationale for also including parents of
     *   the sandbox folder is that they're effectively part of its metadata,
     *   since they're visible in its full path name.
     */
    switch (access)
    {
    case VMOBJFILE_ACCESS_GETINFO:
        /*
         *   read metadata - the sandbox directory is part of the sandbox for
         *   this operation, as is its parent, its parent's parent, etc
         *   (which is to say, any folder that contains the sandbox)
         */
        in_sandbox |= os_is_file_in_dir(sandbox, lclfname, TRUE, TRUE);
        break;

    case VMOBJFILE_ACCESS_READDIR:
        /* 
         *   read directory contents - the sandbox directory itself is part
         *   of the sandbox for this mode 
         */
        match_self = TRUE;
        break;
    }

    /* 
     *   Check to see if the file is in the sandbox folder (or any subfolder
     *   thereof).  If not, we may have to disallow the operation based on
     *   safety level settings, but for now just note the location status.
     */
    in_sandbox |= os_is_file_in_dir(lclfname, sandbox, TRUE, match_self);

    /* if the file isn't in the sandbox, access isn't allowed */
    if (!in_sandbox)
        return FALSE;

    /* didn't find any problems - allow the access */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - get the character set 
 */
int CVmObjFile::getp_get_charset(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* if we have a character set object, return it; otherwise return nil */
    if (get_ext()->charset != VM_INVALID_OBJ)
        retval->set_obj(get_ext()->charset);
    else
        retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - set the character set 
 */
int CVmObjFile::getp_set_charset(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve and save the character set argument */
    get_ext()->charset = get_charset_arg(vmg_ 0, 1);

    /* discard arguments */
    G_stk->discard();

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check that we're in a valid state to perform file operations. 
 */
void CVmObjFile::check_valid_file(VMG0_)
{
    /* if we're 'out of sync', we can't perform any operations on it */
    if ((get_ext()->flags & VMOBJFILE_OUT_OF_SYNC) != 0)
        G_interpreter->throw_new_class(vmg_ G_predef->file_sync_exc,
                                       0, "file out of sync");

    /* if the file has been closed, we can't perform any operations on it */
    if (get_ext()->fp == 0)
        G_interpreter->throw_new_class(vmg_ G_predef->file_closed_exc, 0,
                                       "operation attempted on closed file");
}

/*
 *   Check that we have read access 
 */
void CVmObjFile::check_read_access(VMG0_)
{
    /* we have read access unless we're in write-only mode */
    if (get_ext()->access == VMOBJFILE_ACCESS_WRITE)
        G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc, 0,
                                       "wrong file mode");
}

/*
 *   Check that we have write access 
 */
void CVmObjFile::check_write_access(VMG0_)
{
    /* we have write access unless we're in read-only mode */
    if (get_ext()->access == VMOBJFILE_ACCESS_READ)
        G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc, 0,
                                       "wrong file mode");
}

/*
 *   Check that we're in raw mode 
 */
void CVmObjFile::check_raw_mode(VMG0_)
{
    /* if we're not in raw mode, throw an error */
    if (get_ext()->mode != VMOBJFILE_MODE_RAW)
        G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc,
                                       0, "wrong file mode");
}

/*
 *   Check that we've a valid raw readable file 
 */
void CVmObjFile::check_raw_read(VMG0_)
{
    check_valid_file(vmg0_);
    check_read_access(vmg0_);
    check_raw_mode(vmg0_);
}


/*
 *   Check that we've a valid raw writable file 
 */
void CVmObjFile::check_raw_write(VMG0_)
{
    check_valid_file(vmg0_);
    check_write_access(vmg0_);
    check_raw_mode(vmg0_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - close the file
 */
int CVmObjFile::getp_close_file(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* 
     *   take over the network file descriptor from the File object - we're
     *   going to either close it or abandon it, which deletes it, so we
     *   don't need to delete it again when the File object is collected 
     */
    CVmNetFile *netfile = get_ext()->netfile;
    get_ext()->netfile = 0;

    /* 
     *   Explicitly flush the file, to detect any errors flushing our final
     *   write buffers.  The regular osifc close ignores errors, so we have
     *   to check with an explicit flush if we want this information. 
     */
    err_try
    {
        /* flush buffers */
        get_ext()->fp->flush();
    }
    err_catch_disc
    {
        /* 
         *   The flush failed, so the close effectively failed.  Close the
         *   file anyway to release system resources.
         */
        delete get_ext()->fp;

        /* 
         *   Abandon the network file object.  Since we failed to close the
         *   local copy properly, we don't want to update the server copy (if
         *   it exists) with corrupt data. 
         */
        if (netfile != 0)
            netfile->abandon(vmg0_);

        /* rethrow the error */
        err_rethrow();
    }
    err_end;

    /* close and explicitly forget the data source */
    delete get_ext()->fp;
    get_ext()->fp = 0;

    /* if there's a network file, close it */
    if (netfile != 0)
        netfile->close(vmg0_);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - read from the file
 */
int CVmObjFile::getp_read_file(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* check that we have read access */
    check_read_access(vmg0_);

    /* note the implicit seeking */
    note_file_seek(vmg_ FALSE);

    /* flush stdio buffers as needed and note the read operation */
    switch_read_write_mode(vmg_ FALSE);

    /* read according to our mode */
    switch(get_ext()->mode)
    {
    case VMOBJFILE_MODE_TEXT:
        /* read a line of text */
        read_text_mode(vmg_ retval);
        break;

    case VMOBJFILE_MODE_DATA:
        /* read in data mode */
        read_data_mode(vmg_ retval);
        break;

    case VMOBJFILE_MODE_RAW:
        /* can't use this call on this type of file */
        G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc,
                                       0, "wrong file mode");
        break;
    }

    /* discard the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Read a value in text mode 
 */
void CVmObjFile::read_text_mode(VMG_ vm_val_t *retval)
{
    /* get our file data source and read buffer from the extension */
    CVmDataSource *fp = get_ext()->fp;
    vmobjfile_readbuf_t *readbuf = get_ext()->readbuf;

    /* get our character mapper */
    CCharmapToUni *charmap =
        ((CVmObjCharSet *)vm_objp(vmg_ get_ext()->charset))
        ->get_to_uni(vmg0_);

    /* replenish the buffer if it's empty */
    if (readbuf->rem == 0 && !readbuf->refill(fp))
    {
        /* end of file - return nil */
        retval->set_nil();
        return;
    }

    /* 
     *   Allocate a string object for the result.  We have no idea how long
     *   the line will be, so start with an arbitrary guess; we'll adjust the
     *   buffer up or down as needed. 
     */
    vm_obj_id_t strid = CVmObjString::create(vmg_ FALSE, 128);
    CVmObjString *str = (CVmObjString *)vm_objp(vmg_ strid);
    char *dst = str->cons_get_buf();

    /* this string will be the return value */
    retval->set_obj(strid);

    /* push it for gc protection */
    G_stk->push(retval);

    /*
     *   Read a line of text from the file into our buffer.  Keep going
     *   until we read an entire line; we might have to read the line in
     *   chunks, since the line might end up being longer than our buffer.  
     */
    for (;;)
    {
        /* read the next character; if we can't, we're at EOF */
        wchar_t ch;
        size_t chlen;
        if (!readbuf->getch(ch, fp, charmap))
            break;

        /* if this is a CR, LF, or U+2028, it's the end of the line */
        if (ch == '\n' || ch == '\r' || ch == 0x2028)
        {
            /* newline - skip CR-LF and LR-CR pairs */
            wchar_t nxt = (ch == '\n' ? '\r' : ch == '\r' ? '\n' : 0);
            size_t b;
            if (nxt != 0 && readbuf->peekch(ch, fp, charmap, b) && ch == nxt)
                readbuf->commit_peek(b);

            /* whichever type of newline we read, store it as LF */
            ch = 10;
            chlen = 1;
        }
        else
        {
            /* ordinary character - figure the output character length */
            chlen = utf8_ptr::s_wchar_size(ch);
        }
        
        /* ensure we have enough space for it in our string */
        dst = str->cons_ensure_space(vmg_ dst, chlen, 128);
            
        /* add this character to the string */
        dst += utf8_ptr::s_putch(dst, ch);

        /* if we just read a newline, we're done */
        if (ch == '\n')
            break;
    }

    /* close the string */
    str->cons_shrink_buffer(vmg_ dst);

    /* 
     *   we now can discard the string we've been keeping on the stack to
     *   for garbage collection protection 
     */
    G_stk->discard();
}

/*
 *   Read a value in 'data' mode 
 */
void CVmObjFile::read_data_mode(VMG_ vm_val_t *retval)
{
    char buf[32];
    CVmObjString *str_obj;
    vm_obj_id_t str_id;
    CVmDataSource *fp = get_ext()->fp;

    /* read the type flag */
    if (fp->read(buf, 1))
    {
        /* end of file - return nil */
        retval->set_nil();
        return;
    }

    /* see what we have */
    switch(buf[0])
    {
    case VMOBJFILE_TAG_INT:
        /* read the INT4 value */
        if (fp->read(buf, 4))
            goto io_error;

        /* set the integer value from the buffer */
        retval->set_int(osrp4s(buf));
        break;

    case VMOBJFILE_TAG_ENUM:
        /* read the UINT4 value */
        if (fp->read(buf, 4))
            goto io_error;

        /* set the 'enum' value */
        retval->set_enum(t3rp4u(buf));
        break;

    case VMOBJFILE_TAG_STRING:
        /* 
         *   read the string's length - note that this length is two
         *   higher than the actual length of the string, because it
         *   includes the length prefix bytes 
         */
        if (fp->read(buf, 2))
            goto io_error;

        /* 
         *   allocate a new string of the required size (deducting two
         *   bytes from the indicated size, since the string allocator
         *   only wants to know about the bytes of the string we want to
         *   store, not the length prefix part) 
         */
        str_id = CVmObjString::create(vmg_ FALSE, osrp2(buf) - 2);
        str_obj = (CVmObjString *)vm_objp(vmg_ str_id);

        /* read the bytes of the string into the object's buffer */
        if (fp->read(str_obj->cons_get_buf(), osrp2(buf) - 2))
            goto io_error;

        /* success - set the string return value, and we're done */
        retval->set_obj(str_id);
        break;

    case VMOBJFILE_TAG_TRUE:
        /* it's a simple 'true' value */
        retval->set_true();
        break;

    case VMOBJFILE_TAG_BIGNUM:
        /* read the BigNumber value and return a new BigNumber object */
        if (CVmObjBigNum::read_from_data_file(vmg_ retval, fp))
            goto io_error;
        break;

    case VMOBJFILE_TAG_BYTEARRAY:
        /* read the ByteArray value and return a new ByteArray object */
        if (CVmObjByteArray::read_from_data_file(vmg_ retval, fp))
            goto io_error;
        break;

    default:
        /* invalid data - throw an error */
        G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                       0, "file I/O error");
    }

    /* done */
    return;

io_error:
    /* 
     *   we'll come here if we read the type tag correctly but encounter
     *   an I/O error reading the value - this indicates a corrupted input
     *   stream, so throw an I/O error 
     */
    G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                   0, "file I/O error");
}

/*
 *   Internal read routine 
 */
int CVmObjFile::read_file(VMG_ char *buf, int32_t &len)
{
    /* get the data source */
    CVmDataSource *fp = get_ext()->fp;

    /* make sure it's valid */
    if ((get_ext()->flags & VMOBJFILE_OUT_OF_SYNC) != 0 || fp == 0)
        return FALSE;

    /* deal with stdio buffering if we're changing modes */
    switch_read_write_mode(vmg_ FALSE);

    /* read according to the mode */
    switch (get_ext()->mode)
    {
    case VMOBJFILE_MODE_TEXT:
        {
            /* get the read buffer and character mapper */
            vmobjfile_readbuf_t *readbuf = get_ext()->readbuf;
            CCharmapToUni *charmap =
                ((CVmObjCharSet *)vm_objp(vmg_ get_ext()->charset))
                ->get_to_uni(vmg0_);

            /* loop until we satisfy the request */
            int32_t actual;
            for (actual = 0 ; ; )
            {
                /* peek at the next character */
                wchar_t ch;
                size_t b;
                if (!readbuf->peekch(ch, fp, charmap, b))
                    break;

                /* make sure it'll fit */
                size_t csiz = utf8_ptr::s_wchar_size(ch);
                if (actual + (int32_t)csiz > len)
                    break;

                /* store this character */
                actual += utf8_ptr::s_putch(buf + actual, ch);

                /* advance to the next character */
                readbuf->commit_peek(b);
            }

            /* set the actual return length */
            len = actual;
        }
        break;

    case VMOBJFILE_MODE_RAW:
        /* do the direct read */
        len = fp->readc(buf, (size_t)len);
        break;

    default:
        /* can't handle others */
        return FALSE;
    }

    /* success */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - write to the file
 */
int CVmObjFile::getp_write_file(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    const vm_val_t *argval;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   get a pointer to the argument value, but leave it on the stack
     *   for now to protect against losing it in garbage collection 
     */
    argval = G_stk->get(0);

    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* check that we have write access */
    check_write_access(vmg0_);

    /* deal with stdio buffering if we're changing modes */
    switch_read_write_mode(vmg_ TRUE);

    /* read according to our mode */
    switch(get_ext()->mode)
    {
    case VMOBJFILE_MODE_TEXT:
        /* read a line of text */
        write_text_mode(vmg_ argval);
        break;

    case VMOBJFILE_MODE_DATA:
        /* read in data mode */
        write_data_mode(vmg_ argval);
        break;

    case VMOBJFILE_MODE_RAW:
        /* can't use this call on this type of file */
        G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc,
                                       0, "wrong file mode");
        break;
    }

    /* discard our gc protection and argument */
    G_stk->discard(2);

    /* no return value - return nil by default */
    retval->set_nil();
    
    /* handled */
    return TRUE;
}

/*
 *   Write a value in text mode
 */
void CVmObjFile::write_text_mode(VMG_ const vm_val_t *val)
{
    char conv_buf[128];
    vm_val_t new_str;
    CCharmapToLocal *charmap;
    const char *constp;
    const char *p, *startp;
    size_t rem;

    /* get our character mapper */
    charmap = ((CVmObjCharSet *)vm_objp(vmg_ get_ext()->charset))
              ->get_to_local(vmg0_);
    
    /* convert the value to a string */
    constp = CVmObjString::cvt_to_str(
        vmg_ &new_str, conv_buf, sizeof(conv_buf), val, 10, 0);

    /* protect the new string (if any) from gc while working */
    G_stk->push(&new_str);

    /* scan for newlines - we need to write newline sequences specially */
    for (startp = constp + VMB_LEN, rem = vmb_get_len(constp) ;
         rem != 0 ; )
    {
        /* scan to the next newline */
        for (p = startp ; rem != 0 && *p != '\n' ; ++p, --rem) ;

        /* write this chunk through the character mapper */
        if (p != startp
            && charmap->write_file(get_ext()->fp, startp, p - startp))
        {
            /* the write failed - throw an I/O exception */
            G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                           0, "file I/O error");
        }

        /* write the newline, if applicable */
        if (rem != 0
            && charmap->write_file(get_ext()->fp, OS_NEWLINE_SEQ,
                                   strlen(OS_NEWLINE_SEQ)))
        {
            /* the write failed - throw an I/O exception */
            G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                           0, "file I/O error");
        }

        /* if we're at a newline, skip it */
        if (rem != 0)
            ++p, --rem;

        /* note where the new chunk starts */
        startp = p;
    }

    /* done with the gc protection */
    G_stk->discard();

    /* invalidate any read buffer */
    note_file_seek(vmg_ FALSE);
}

/*
 *   Write a value in 'data' mode 
 */
void CVmObjFile::write_data_mode(VMG_ const vm_val_t *val)
{
    char buf[32];
    CVmDataSource *fp = get_ext()->fp;
    vm_val_t new_str;
    const char *constp;

    /* see what type of data we want to put */
    switch(val->typ)
    {
    case VM_INT:
        /* put the type in the buffer */
        buf[0] = VMOBJFILE_TAG_INT;
        
        /* add the value in INT4 format */
        oswp4s(buf + 1, val->val.intval);
        
        /* write out the type prefix plus the value */
        if (fp->write(buf, 5))
            goto io_error;

        /* done */
        break;

    case VM_ENUM:
        /* put the type in the buffer */
        buf[0] = VMOBJFILE_TAG_ENUM;

        /* add the value in INT4 format */
        oswp4(buf + 1, val->val.enumval);

        /* write out the type prefix plus the value */
        if (fp->write(buf, 5))
            goto io_error;

        /* done */
        break;

    case VM_SSTRING:
        /* get the string value pointer */
        constp = val->get_as_string(vmg0_);

    write_binary_string:
        /* write the type prefix byte */
        buf[0] = VMOBJFILE_TAG_STRING;
        if (fp->write(buf, 1))
            goto io_error;

        /* 
         *   write the length prefix - for TADS 2 compatibility, include
         *   the bytes of the prefix itself in the length count 
         */
        oswp2(buf, vmb_get_len(constp) + 2);
        if (fp->write(buf, 2))
            goto io_error;

        /* write the string's bytes */
        if (fp->write(constp + VMB_LEN, vmb_get_len(constp)))
            goto io_error;

        /* done */
        break;

    case VM_OBJ:
        /*
         *   Write BigNumber and ByteArray types in special formats.  For
         *   other types, try converting to a string. 
         */
        if (CVmObjBigNum::is_bignum_obj(vmg_ val->val.obj))
        {
            CVmObjBigNum *bignum;
            
            /* we know it's a BigNumber - cast it properly */
            bignum = (CVmObjBigNum *)vm_objp(vmg_ val->val.obj);

            /* write the type tag */
            buf[0] = VMOBJFILE_TAG_BIGNUM;
            if (fp->write(buf, 1))
                goto io_error;

            /* write it out */
            if (bignum->write_to_data_file(fp))
                goto io_error;
        }
        else if (CVmObjByteArray::is_byte_array(vmg_ val->val.obj))
        {
            CVmObjByteArray *bytarr;

            /* we know it's a ByteArray - cast it properly */
            bytarr = (CVmObjByteArray *)vm_objp(vmg_ val->val.obj);

            /* write the type tag */
            buf[0] = VMOBJFILE_TAG_BYTEARRAY;
            if (fp->write(buf, 1))
                goto io_error;
            
            /* write the array */
            if (bytarr->write_to_data_file(fp))
                goto io_error;
        }
        else
        {
            /* 
             *   Cast it to a string value and write that out.  Note that
             *   we can ignore garbage collection for any new string we've
             *   created, since we're just calling the OS-level file
             *   writer, which will never invoke garbage collection.  
             */
            constp = vm_objp(vmg_ val->val.obj)
                     ->cast_to_string(vmg_ val->val.obj, &new_str);
            goto write_binary_string;
        }
        break;

    case VM_TRUE:
        /* 
         *   All we need for this is the type tag.  Note that we can't
         *   write nil because we'd have no way of reading it back in - a
         *   nil return from file_read indicates that we've reached the
         *   end of the file.  So there's no point in writing nil to a
         *   file.  
         */
        buf[0] = VMOBJFILE_TAG_TRUE;
        if (fp->write(buf, 1))
            goto io_error;
        
        /* done */
        break;
        
    default:
        /* other types are not acceptable */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* done */
    return;

io_error:
    /* the write failed - throw an i/o exception */
    G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                   0, "file I/O error");
}


/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - read raw bytes from the file 
 */
int CVmObjFile::getp_read_bytes(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                uint *in_argc)
{
    static CVmNativeCodeDesc desc(1, 2);
    uint argc = (in_argc != 0 ? *in_argc : 0);
    vm_val_t arr_val;
    CVmObjByteArray *arr;
    unsigned long idx;
    unsigned long len;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* check that we have read access */
    check_read_access(vmg0_);

    /* we can only use this call on 'raw' files */
    if (get_ext()->mode != VMOBJFILE_MODE_RAW)
        G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc,
                                       0, "wrong file mode");

    /* retrieve the ByteArray destination */
    G_stk->pop(&arr_val);

    /* make sure it's really a ByteArray object */
    if (arr_val.typ != VM_OBJ
        || !CVmObjByteArray::is_byte_array(vmg_ arr_val.val.obj))
        err_throw(VMERR_BAD_TYPE_BIF);
    
    /* we know it's a byte array object, so cast it */
    arr = (CVmObjByteArray *)vm_objp(vmg_ arr_val.val.obj);

    /* presume we'll try to fill the entire array */
    idx = 1;
    len = arr->get_element_count();
    
    /* if we have a starting index argument, retrieve it */
    if (argc >= 2)
        idx = (unsigned long)CVmBif::pop_int_val(vmg0_);

    /* if we have a length argument, retrieve it */
    if (argc >= 3)
        len = (unsigned long)CVmBif::pop_int_val(vmg0_);
    
    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* note the implicit seeking */
    note_file_seek(vmg_ FALSE);

    /* deal with stdio buffering issues if necessary */
    switch_read_write_mode(vmg_ FALSE);

    /* 
     *   read the data into the array, and return the number of bytes we
     *   actually manage to read 
     */
    retval->set_int(arr->read_from_file(
        vmg_ arr_val.val.obj, get_ext()->fp, idx, len, TRUE));

    /* discard our gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - write raw bytes to the file 
 */
int CVmObjFile::getp_write_bytes(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *in_argc)
{
    static CVmNativeCodeDesc desc(1, 2);
    uint argc = (in_argc != 0 ? *in_argc : 0);
    vm_val_t srcval;
    CVmObjByteArray *arr = 0;
    CVmObjFile *file = 0;
    unsigned long idx, has_idx = FALSE;
    unsigned long len;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* make sure it's a valid file, with write access, in raw mode */
    check_valid_file(vmg0_);
    check_write_access(vmg0_);
    check_raw_mode(vmg0_);

    /* pop the source object argument */
    G_stk->pop(&srcval);
    if (srcval.typ != VM_OBJ)
        err_throw(VMERR_OBJ_VAL_REQD);

    /* check the object type */
    if (CVmObjByteArray::is_byte_array(vmg_ srcval.val.obj))
    {
        /* get the ByteArray object, suitably cast */
        arr = (CVmObjByteArray *)vm_objp(vmg_ srcval.val.obj);

        /* assume we'll write the entire byte array */
        idx = 1;
        len = arr->get_element_count();
    }
    else if (CVmObjFile::is_file_obj(vmg_ srcval.val.obj))
    {
        /* get the file object, suitably cast */
        file = (CVmObjFile *)vm_objp(vmg_ srcval.val.obj);

        /* make sure it's in raw mode */
        if (file->get_ext()->mode != VMOBJFILE_MODE_RAW)
            G_interpreter->throw_new_class(vmg_ G_predef->file_mode_exc,
                                           0, "wrong file mode");

        /* assume we'll copy the file from the current seek offset onward */
        idx = file->get_pos(vmg0_);
        len = file->get_file_size(vmg0_);
    }
    else
    {
        /* invalid source type */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* if we have a starting index, retrieve it */
    if (argc >= 2)
    {
        /* if it's nil, just ignore it, otherwise pop the int value */
        if (G_stk->get(0)->typ == VM_NIL)
            G_stk->discard();
        else
        {
            idx = (unsigned long)CVmBif::pop_int_val(vmg0_);
            has_idx = TRUE;
        }
    }

    /* if we have a length, retrieve it */
    if (argc >= 3)
    {
        /* if it's nil, just ignore it, otherwise pop the int value */
        if (G_stk->get(0)->typ == VM_NIL)
            G_stk->discard();
        else
            len = (unsigned long)CVmBif::pop_int_val(vmg0_);
    }

    /* push a self-reference and a source reference for gc protection */
    G_stk->push()->set_obj(self);
    G_stk->push(&srcval);

    /* flush stdio buffers as needed and note the read operation */
    switch_read_write_mode(vmg_ TRUE);

    /* do the copy according to the source type */
    if (arr != 0)
    {
        /* 
         *   write the bytes to the file - on success (zero write_to_file
         *   return), return nil, on failure (non-zero write_to_file return),
         *   return true 
         */
        if (arr->write_to_file(get_ext()->fp, idx, len))
        {
            /* we failed to write the bytes - throw an I/O exception */
            G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                           0, "file I/O error");
        }
    }
    else if (file != 0)
    {
        /* 
         *   if there's an explicit starting seek position, go there; if it
         *   wasn't specified, the default is the current seek location, so
         *   we just leave well enough alone in that case 
         */
        if (has_idx)
            file->set_pos(vmg_ idx);

        /* copy the requested byte range */
        while (len != 0)
        {
            /* read the next chunk of bytes */
            char buf[1024];
            int32_t cur = (len < (int32_t)sizeof(buf) ? len : sizeof(buf));
            if (!file->read_file(vmg_ buf, cur))
                G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                               0, "file I/O error");

            /* if we read zero bytes, we've reached EOF */
            if (cur == 0)
                break;

            /* write out this chunk */
            if (get_ext()->fp->write(buf, (size_t)cur))
                G_interpreter->throw_new_class(vmg_ G_predef->file_io_exc,
                                               0, "file I/O error");

            /* deduct this hunk from the remaining length */
            len -= cur;
        }
    }

    /* discard our gc protection */
    G_stk->discard(2);

    /* no return value - return nil by default */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - get the seek position
 */
int CVmObjFile::getp_get_pos(VMG_ vm_obj_id_t self, vm_val_t *retval,
                             uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* get the position */
    retval->set_int(get_pos(vmg0_));

    /* handled */
    return TRUE;
}

/*
 *   Get the current file position 
 */
long CVmObjFile::get_pos(VMG0_)
{
    /* get the seek position in the underlying file */
    long pos = get_ext()->fp->get_pos();

    /* 
     *   if we have a read buffer, adjust for buffering: the underlying file
     *   corresponds to the end of the read buffer, so subtract the number of
     *   bytes remaining in the read buffer 
     */
    if (get_ext()->readbuf != 0)
        pos -= get_ext()->readbuf->rem;

    /* return the result */
    return pos;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - set the seek position
 */
int CVmObjFile::getp_set_pos(VMG_ vm_obj_id_t self, vm_val_t *retval,
                             uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    unsigned long pos;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* note the seeking operation */
    note_file_seek(vmg_ TRUE);

    /* retrieve the target seek position */
    pos = CVmBif::pop_long_val(vmg0_);

    /* seek to the new position */
    get_ext()->fp->seek(pos, OSFSK_SET);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   Set position - internal version 
 */
int CVmObjFile::set_pos(VMG_ long pos)
{
    /* make sure it's valid */
    if ((get_ext()->flags & VMOBJFILE_OUT_OF_SYNC) != 0
        || get_ext()->fp == 0)
        return FALSE;

    /* note the seek */
    note_file_seek(vmg_ TRUE);

    /* set the seek position */
    return !get_ext()->fp->seek(pos, OSFSK_SET);
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - set position to end of file
 */
int CVmObjFile::getp_set_pos_end(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* note the seeking operation */
    note_file_seek(vmg_ TRUE);

    /* seek to the end position */
    get_ext()->fp->seek(0, OSFSK_END);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - get size 
 */
int CVmObjFile::getp_get_size(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* get the size */
    retval->set_int(get_file_size(vmg0_));

    /* handled */
    return TRUE;
}

/*
 *   Get the size, internal version 
 */
long CVmObjFile::get_file_size(VMG0_)
{
    /* make sure it's valid */
    if ((get_ext()->flags & VMOBJFILE_OUT_OF_SYNC) != 0
        || get_ext()->fp == 0)
        return -1;

    /* note the seeking operation */
    note_file_seek(vmg_ TRUE);

    /* get the size */
    return get_ext()->fp->get_size();
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - get the file mode
 */
int CVmObjFile::getp_get_mode(VMG_ vm_obj_id_t self, vm_val_t *retval,
                              uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* return the mode */
    retval->set_int(get_ext()->mode);

    /* handled */
    return TRUE;
}

/*
 *   Get the mode, internal version 
 */
int CVmObjFile::get_file_mode(VMG0_)
{
    return get_ext()->mode;
}


/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - setMode - change the file mode
 */
int CVmObjFile::getp_setMode(VMG_ vm_obj_id_t self, vm_val_t *retval,
                             uint *oargc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1, 1);
    int argc = (oargc != 0 ? *oargc : 0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;
    
    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);
    
    /* retrieve the mode argument */
    int mode = CVmBif::pop_int_val(vmg0_);
    --argc;

    /* presume no character set or read buffer */
    vm_obj_id_t cset = VM_INVALID_OBJ;
    int create_readbuf = FALSE;

    /* save the retained settings from the current extension */
    vmobjfile_ext_t *ext = get_ext();
    CVmDataSource *fp = ext->fp;
    CVmNetFile *netfile = ext->netfile;
    unsigned long flags = ext->flags;
    int access = ext->access;

    /* check the mode */
    switch (mode)
    {
    case VMOBJFILE_MODE_TEXT:
        /* text mode - we need a character mapper in this mode */
        cset = get_charset_arg(vmg_ 0, argc);

        /* 
         *   if we're open with read access, we need a read buffer in order
         *   to do the character set translation 
         */
        if (access == VMOBJFILE_ACCESS_READ)
            create_readbuf = TRUE;
        break;

    case VMOBJFILE_MODE_DATA:
    case VMOBJFILE_MODE_RAW:
        /* 
         *   data mode or raw mode - we don't use a character mapper for this
         *   mode, so if the argument is present, simply discard it 
         */
        if (argc >= 2)
            G_stk->discard();
        break;

    default:
        /* invalid mode */
        err_throw(VMERR_BAD_VAL_BIF);
    }
    
    /* push the character set object (if any) for gc protection */
    G_stk->push_obj_or_nil(vmg_ cset);

    /* 
     *   remove the data source and network file descriptor from the
     *   extension, so that they're not deleted when we reallocate it 
     */
    ext->fp = 0;
    ext->netfile = 0;

    /* 
     *   Reallocate the extension.  This will set up the correct structures
     *   for the new file mode. 
     */
    alloc_ext(vmg_ netfile, cset, fp, flags, mode, access, create_readbuf);

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}



/* ------------------------------------------------------------------------ */
/*
 *   getRootName property evaluator - get the root filename of a given
 *   string.  This is a static (class) method.  
 */
int CVmObjFile::s_getp_getRootName(VMG_ vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    char buf[OSFNMAX*2];
    const char *ret;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the string argument into our buffer */
    CVmBif::pop_fname_val(vmg_ buf, sizeof(buf));

    /* set up a no-access file descriptor to determine where the file is */
    CVmNetFile *nf = CVmNetFile::open(vmg_ buf, 0, 0, OSFTUNK, 0);

    /*
     *   If the file is on the network storage server, we always use the Unix
     *   convention, regardless of the local naming rules.  Otherwise, use
     *   the local OS API to get the root name.  
     */
    if (nf->is_net_file())
    {
        /* 
         *   We're in network mode, so use the Unix path conventions.  Simply
         *   scan for the last '/' character; if we find it, the root name is
         *   the part after the '/', otherwise it's the whole string.  
         */
        const char *sl = strrchr(buf, '/');
        ret = (sl != 0 ? sl + 1 : buf);
    }
    else
    {
        /* it's not a network file - use the local filename rules */
        ret = os_get_root_name(buf);
    }

    /* done with the net file desriptor */
    nf->abandon(vmg0_);

    /* return the string */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, ret, strlen(ret)));

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   deleteFile() property evaluator - deletes a file
 */
int CVmObjFile::s_getp_deleteFile(VMG_ vm_val_t *retval, uint *in_argc)
{
    /* check arguments */
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* get the filename; use the DELETE access mode */
    vm_rcdesc rc(vmg_ "File.deleteFile",
                 CVmObjFile::metaclass_reg_->get_class_obj(vmg0_),
                 VMOBJFILE_DELETE_FILE, G_stk->get(0), argc);
    int access = VMOBJFILE_ACCESS_DELETE;
    CVmNetFile *netfile = get_filename_and_access(
        vmg_ &rc, &access, FALSE, OSFTUNK, "application/octet-stream");

    /* close the network file descriptor - this will delete the file */
    netfile->close(vmg0_);

    /* discard arguments */
    G_stk->discard(1);

    /* no return */
    retval->set_nil();

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - unpackBytes 
 */
int CVmObjFile::getp_unpackBytes(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* check that the it's a valid file open with read access in "raw" mode */
    check_raw_read(vmg0_);

    /* get the format string, but leave it on the stack */
    const char *fmt = G_stk->get(0)->get_as_string(vmg0_);
    if (fmt == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the format string pointer and length */
    size_t fmtlen = vmb_get_len(fmt);
    fmt += VMB_LEN;

    /* save 'self' for gc protection */
    G_stk->push_obj(vmg_ self);

    /* unpack the data from our file stream into a list, returning the list */
    CVmPack::unpack(vmg_ retval, fmt, fmtlen, get_ext()->fp);

    /* discard our arguments and gc protection */
    G_stk->discard(argc + 1);
        
    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - packBytes
 */
int CVmObjFile::getp_packBytes(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 0, TRUE);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* check that the it's a valid file open with write access in "raw" mode */
    check_raw_write(vmg0_);

    /* save 'self' for gc protection */
    G_stk->push_obj(vmg_ self);

    /* remember the starting seek position */
    long start = get_pos(vmg0_);

    /* pack the arguments into our file data stream */
    CVmPack::pack(vmg_ 1, argc, get_ext()->fp);

    /* discard our gc protection and our arguments */
    G_stk->discard(argc + 1);

    /* return the number of bytes packed */
    retval->set_int(get_pos(vmg0_) - start);
    
    /* handled */
    return TRUE;
}


/* 
 *   property evaluator - sha256
 */
int CVmObjFile::getp_sha256(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* check that the it's a valid file open with read access in "raw" mode */
    check_raw_read(vmg0_);

    /* retrieve the length value, if present; if not, use the file size */
    long len = (argc >= 1 && G_stk->get(0)->typ != VM_NIL
                ? G_stk->get(0)->num_to_int(vmg0_)
                : get_file_size(vmg0_));

    /* save 'self' for gc protection */
    G_stk->push_obj(vmg_ self);

    /* calculate the hash */
    char hash[65];
    sha256_datasrc(hash, get_ext()->fp, len);

    /* return the hash value */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, hash, 64));

    /* discard our gc protection and our arguments */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}


/* 
 *   property evaluator - digestMD5
 */
int CVmObjFile::getp_digestMD5(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* check that the it's a valid file open with read access in "raw" mode */
    check_raw_read(vmg0_);

    /* retrieve the length value, if present; if not, use the file size */
    long len = (argc >= 1 && G_stk->get(0)->typ != VM_NIL
                ? G_stk->get(0)->num_to_int(vmg0_)
                : get_file_size(vmg0_));

    /* save 'self' for gc protection */
    G_stk->push_obj(vmg_ self);

    /* calculate the hash */
    char hash[33];
    md5_datasrc(hash, get_ext()->fp, len);

    /* return the hash value */
    retval->set_obj(CVmObjString::create(vmg_ FALSE, hash, 32));

    /* discard our gc protection and our arguments */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Refill the read buffer.  Returns true if the buffer contains any data
 *   on return, false if we're at end of file.  
 */
int vmobjfile_readbuf_t::refill(CVmDataSource *fp)
{
    /* if the buffer is already full, do nothing */
    if (rem == sizeof(buf))
        return FALSE;
    
    /* 
     *   if there's anything left in the buffer, move it to the start of the
     *   buffer 
     */
    if (rem != 0)
    {
        memmove(buf, ptr, rem);
        ptr = buf + rem;
    }
    else
        ptr = buf;

    /* fill up the rest of the buffer */
    size_t cur = fp->readc(ptr, sizeof(buf) - rem);
    rem += cur;

    /* return success if we managed to read any bytes on this round */
    return (cur != 0);
}

/*
 *   Read a character 
 */
int vmobjfile_readbuf_t::getch(wchar_t &ch,
                               CVmDataSource *fp,
                               class CCharmapToUni *charmap)
{
    /* keep going until we map a character or run out of input */
    for (;;)
    {
        /* try mapping a character - if we get one, return success */
        if (charmap->mapchar(ch, (const char *&)ptr, rem))
            return TRUE;

        /* 
         *   we don't have enough bytes to map a whole character, so refill
         *   our buffer; if that fails, we're at EOF 
         */
        if (!refill(fp))
            return FALSE;
    }
}

/*
 *   Peek at the next character
 */
int vmobjfile_readbuf_t::peekch(wchar_t &ch,
                                CVmDataSource *fp,
                                class CCharmapToUni *charmap,
                                size_t &in_bytes)
{
    /* keep going until we map a character or run out of input */
    for (;;)
    {
        /* try mapping a character - if we get one, return success */
        char *ptr2 = ptr;
        size_t rem2 = rem;
        if (charmap->mapchar(ch, (const char *&)ptr2, rem2))
        {
            in_bytes = rem - rem2;
            return TRUE;
        }

        /* 
         *   we don't have enough bytes to map a whole character, so refill
         *   our buffer; if that fails, we're at EOF 
         */
        if (!refill(fp))
            return FALSE;
    }
}

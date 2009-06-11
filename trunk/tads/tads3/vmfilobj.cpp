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
#include "vmfilobj.h"
#include "vmpredef.h"
#include "vmundo.h"
#include "vmbytarr.h"
#include "vmbignum.h"
#include "vmhost.h"


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
    &CVmObjFile::getp_undef,
    &CVmObjFile::getp_open_text,
    &CVmObjFile::getp_open_data,
    &CVmObjFile::getp_open_raw,
    &CVmObjFile::getp_get_charset,
    &CVmObjFile::getp_set_charset,
    &CVmObjFile::getp_close_file,
    &CVmObjFile::getp_read_file,
    &CVmObjFile::getp_write_file,
    &CVmObjFile::getp_read_bytes,
    &CVmObjFile::getp_write_bytes,
    &CVmObjFile::getp_get_pos,
    &CVmObjFile::getp_set_pos,
    &CVmObjFile::getp_set_pos_end,
    &CVmObjFile::getp_open_res_text,
    &CVmObjFile::getp_open_res_raw,
    &CVmObjFile::getp_get_size
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
    VMOBJFILE_OPEN_RES_RAW = 15
};

/* ------------------------------------------------------------------------ */
/*
 *   Special filename designators 
 */

/* library defaults file */
#define SFID_LIB_DEFAULTS    0x0001

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
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);

    /* instantiate the object */
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
                               vm_obj_id_t charset, osfildef *fp,
                               unsigned long flags, int mode, int access,
                               int create_readbuf,
                               unsigned long res_start, unsigned long res_end)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);

    /* instantiate the object */
    new (vmg_ id) CVmObjFile(vmg_ charset, fp, flags, mode, access,
                             create_readbuf, res_start, res_end);

    /* files are always transient */
    G_obj_table->set_obj_transient(id);

    /* return the ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Instantiate 
 */
CVmObjFile::CVmObjFile(VMG_ vm_obj_id_t charset, osfildef *fp,
                       unsigned long flags, int mode, int access,
                       int create_readbuf,
                       unsigned long res_start, unsigned long res_end)
{
    /* allocate and initialize our extension */
    ext_ = 0;
    alloc_ext(vmg_ charset, fp, flags, mode, access, create_readbuf,
              res_start, res_end);
}

/*
 *   Allocate and initialize our extension 
 */
void CVmObjFile::alloc_ext(VMG_ vm_obj_id_t charset, osfildef *fp,
                           unsigned long flags, int mode, int access,
                           int create_readbuf,
                           unsigned long res_start, unsigned long res_end)
{
    size_t siz;

    /* 
     *   if we already have an extension, delete it (and release our
     *   underlying system file, if any) 
     */
    notify_delete(vmg_ FALSE);

    /* 
     *   Figure the needed size.  We need at least the standard extension;
     *   if we also need a read buffer, figure in space for that as well. 
     */
    siz = sizeof(vmobjfile_ext_t);
    if (create_readbuf)
        siz += sizeof(vmobjfile_readbuf_t);

    /* allocate space for our extension structure */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(siz, this);

    /* store the data we received from the caller */
    get_ext()->fp = fp;
    get_ext()->charset = charset;
    get_ext()->flags = flags;
    get_ext()->mode = (unsigned char)mode;
    get_ext()->access = (unsigned char)access;
    get_ext()->res_start = res_start;
    get_ext()->res_end = res_end;

    /* 
     *   point to our read buffer, for which we allocated space contiguously
     *   with and immediately following our extension, if we have one 
     */
    if (create_readbuf)
    {
        /* point to our read buffer object */
        get_ext()->readbuf = (vmobjfile_readbuf_t *)(get_ext() + 1);

        /* initialize the read buffer with no initial data */
        get_ext()->readbuf->rem = 0;
        get_ext()->readbuf->ptr.set(0);
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
            osfcls(get_ext()->fp);

        /* free our extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
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
    /* translate the property into a function vector index */
    switch(G_meta_table
           ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop))
    {
    case VMOBJFILE_OPEN_TEXT:
        return s_getp_open_text(vmg_ result, argc, FALSE);

    case VMOBJFILE_OPEN_DATA:
        return s_getp_open_data(vmg_ result, argc);

    case VMOBJFILE_OPEN_RAW:
        return s_getp_open_raw(vmg_ result, argc, FALSE);

    case VMOBJFILE_OPEN_RES_TEXT:
        return s_getp_open_text(vmg_ result, argc, TRUE);

    case VMOBJFILE_OPEN_RES_RAW:
        return s_getp_open_raw(vmg_ result, argc, TRUE);

    default:
        /* it's not one of ours - inherit from the base object metaclass */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
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
    vm_obj_id_t charset;
    unsigned long flags;
    int mode;
    int access;

    /* read our character set */
    charset = vmb_get_objid(ptr);
    ptr += VMB_OBJECT_ID;

    /* get the mode and access values */
    mode = (unsigned char)*ptr++;
    access = (unsigned char)*ptr++;

    /* get the flags */
    flags = t3rp4u(ptr);

    /* 
     *   add in the out-of-sync flag, since we've restored the state from a
     *   past state and thus we're out of sync with the external file system
     *   environment 
     */
    flags |= VMOBJFILE_OUT_OF_SYNC;

    /* 
     *   Initialize our extension - we have no underlying native file
     *   handle, since the file is out of sync by virtue of being loaded
     *   from a previously saved image state.  Note that we don't need a
     *   read buffer because the file is inherently out of sync and thus
     *   cannot be read.  
     */
    alloc_ext(vmg_ charset, 0, flags, mode, access, FALSE, 0, 0);
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
    /* files are always transient, so should never be savd */
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
}

/* ------------------------------------------------------------------------ */
/*
 *   Note that we're seeking within the file.
 */
void CVmObjFile::note_file_seek(VMG_ vm_obj_id_t self, int is_explicit)
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
void CVmObjFile::switch_read_write_mode(int writing)
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
            osfseek(get_ext()->fp, osfpos(get_ext()->fp), OSFSK_SET);
    }

    /* 
     *   remember that this operation is stdio-buffered, so that we'll know
     *   we need to seek if we perform the opposite type of application
     *   after this one 
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
 *   Retrieve the filename and access mode arguments. 
 */
void CVmObjFile::get_filename_and_access(VMG_ char *fname, size_t fname_siz,
                                         int *access, int is_resource_file)
{
    int is_special_file = FALSE;
    
    /* 
     *   check to see if we have an explicit filename string, or an integer
     *   giving a special system file ID 
     */
    if (G_stk->get(0)->typ == VM_INT)
    {
        char path[OSFNMAX];

        /* start with no path, in case we have trouble retrieving it */
        path[0] = '\0';
        
        /* we have an integer, which is a special file designator */
        switch (CVmBif::pop_int_val(vmg0_))
        {
        case SFID_LIB_DEFAULTS:
            /* get the system application data path */
            G_host_ifc->get_special_file_path(path, sizeof(path),
                                              OS_GSP_T3_APP_DATA);

            /* add the filename */
            os_build_full_path(fname, fname_siz, path, "settings.txt");
            break;

        default:
            /* invalid filename value */
            err_throw(VMERR_BAD_VAL_BIF);
        }

        /* note that we have a special file, for file safety purposes */
        is_special_file = TRUE;
    }
    else
    {
        /* we must have an explicit filename string - pop it */
        CVmBif::pop_str_val_fname(vmg_ fname, fname_siz);
    }

    /* 
     *   retrieve the access mode; if it's a resource file, the mode is
     *   implicitly 'read' 
     */
    if (is_resource_file)
        *access = VMOBJFILE_ACCESS_READ;
    else
        *access = CVmBif::pop_int_val(vmg0_);

    /*
     *   If this isn't a special file, then check the file safety mode to
     *   ensure this operation is allowed.  Reading resources is always
     *   allowed, regardless of the safety mode, since resources are
     *   read-only and are inherently constrained in the paths they can
     *   access.  Likewise, special files bypass the safety settings, because
     *   the interpreter controls the names and locations of these files,
     *   ensuring that they're inherently safe.  
     */
    if (!is_resource_file && !is_special_file)
        check_safety_for_open(vmg_ fname, *access);
}

/* ------------------------------------------------------------------------ */
/*
 *   Static property evaluator - open a text file 
 */
int CVmObjFile::s_getp_open_text(VMG_ vm_val_t *retval, uint *in_argc,
                                 int is_resource_file)
{
    uint argc = (in_argc != 0 ? *in_argc : 0);
    static CVmNativeCodeDesc desc_file(2, 1);
    static CVmNativeCodeDesc desc_res(1, 1);
    char fname[OSFNMAX];
    int access;
    vm_obj_id_t cset_obj;
    osfildef *fp;
    int create_readbuf;
    unsigned long res_start;
    unsigned long res_end;
    unsigned int flags;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc,
                            is_resource_file ? &desc_res : &desc_file))
        return TRUE;

    /* initialize the flags to indicate a text-mode file */
    flags = 0;

    /* add the resource-file flag if appropriate */
    if (is_resource_file)
        flags |= VMOBJFILE_IS_RESOURCE;

    /* presume we can use the entire file */
    res_start = 0;
    res_end = 0;

    /* retrieve the filename */
    get_filename_and_access(vmg_ fname, sizeof(fname),
                            &access, is_resource_file);

    /* presume we won't need a read buffer */
    create_readbuf = FALSE;

    /* if there's a character set name or object, retrieve it */
    if (argc > 2)
    {
        /* 
         *   check to see if it's a CharacterSet object; if it's not, it
         *   must be a string giving the character set name 
         */
        if (G_stk->get(0)->typ == VM_OBJ
            && CVmObjCharSet::is_charset(vmg_ G_stk->get(0)->val.obj))
        {
            /* retrieve the CharacterSet reference */
            cset_obj = CVmBif::pop_obj_val(vmg0_);
        }
        else
        {
            const char *str;
            size_t len;
            
            /* it's not a CharacterSet, so it must be a character set name */
            str = G_stk->get(0)->get_as_string(vmg0_);
            if (str == 0)
                err_throw(VMERR_BAD_TYPE_BIF);

            /* get the length and skip the length prefix */
            len = vmb_get_len(str);
            str += VMB_LEN;

            /* create a mapper for the given name */
            cset_obj = CVmObjCharSet::create(vmg_ FALSE, str, len);
        }
    }
    else
    {
        /* no character set is specified - use US-ASCII by default */
        cset_obj = CVmObjCharSet::create(vmg_ FALSE, "us-ascii", 8);
    }

    /* push the character map object onto the stack for gc protection */
    G_stk->push()->set_obj(cset_obj);

    /* open the file for reading or writing, as appropriate */
    switch(access)
    {
    case VMOBJFILE_ACCESS_READ:
        /* open a resource file or file system file, as appropriate */
        if (is_resource_file)
        {
            unsigned long res_len;
            
            /* it's a resource - open it */
            fp = G_host_ifc->find_resource(fname, strlen(fname), &res_len);

            /* 
             *   if we found the resource, note the start and end seek
             *   positions, so we can limit reading of the underlying file
             *   to the section that contains the resource data 
             */
            if (fp != 0)
            {
                /* the file is initially at the start of the resource data */
                res_start = osfpos(fp);

                /* note the offset of the first byte after the resource */
                res_end = res_start + res_len;
            }
        }
        else
        {
            /* 
             *   Not a resource - open an ordinary text file for reading.
             *   Even though we're going to treat the file as a text file,
             *   open it in binary mode, since we'll do our own universal
             *   newline translations; this allows us to work with files in
             *   any character set, and using almost any newline
             *   conventions, so files copied from other systems will be
             *   fully usable even if they haven't been fixed up to local
             *   conventions.  
             */
            fp = osfoprb(fname, OSFTTEXT);
        }

        /* make sure we opened it successfully */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_not_found_exc,
                                           0, "file not found");

        /* we need a read buffer */
        create_readbuf = TRUE;
        break;

    case VMOBJFILE_ACCESS_WRITE:
        /* open for writing */
        fp = osfopwb(fname, OSFTTEXT);

        /* make sure we created it successfully */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_creation_exc,
                                           0, "error creating file");
        break;

    case VMOBJFILE_ACCESS_RW_KEEP:
        /* open for read/write, keeping existing contents */
        fp = osfoprwb(fname, OSFTTEXT);

        /* make sure we were able to find or create the file */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                           0, "error opening file");
        break;

    case VMOBJFILE_ACCESS_RW_TRUNC:
        /* open for read/write, truncating existing contents */
        fp = osfoprwtb(fname, OSFTTEXT);

        /* make sure we were successful */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                           0, "error opening file");
        break;

    default:
        fp = 0;
        err_throw(VMERR_BAD_VAL_BIF);
    }

    /* create our file object */
    retval->set_obj(create(vmg_ FALSE, cset_obj, fp, flags,
                           VMOBJFILE_MODE_TEXT, access, create_readbuf,
                           res_start, res_end));

    /* discard gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Static property evaluator - open a data file
 */
int CVmObjFile::s_getp_open_data(VMG_ vm_val_t *retval, uint *argc)
{
    /* use the generic binary file opener in 'data' mode */
    return open_binary(vmg_ retval, argc, VMOBJFILE_MODE_DATA, FALSE);
}

/*
 *   Static property evaluator - open a raw file
 */
int CVmObjFile::s_getp_open_raw(VMG_ vm_val_t *retval, uint *argc,
                                int is_resource_file)
{
    /* use the generic binary file opener in 'raw' mode */
    return open_binary(vmg_ retval, argc, VMOBJFILE_MODE_RAW,
                       is_resource_file);
}

/*
 *   Generic binary file opener - common to 'data' and 'raw' files 
 */
int CVmObjFile::open_binary(VMG_ vm_val_t *retval, uint *argc, int mode,
                            int is_resource_file)
{
    static CVmNativeCodeDesc file_desc(2);
    static CVmNativeCodeDesc res_desc(1);
    char fname[OSFNMAX];
    int access;
    osfildef *fp;
    unsigned long res_start;
    unsigned long res_end;
    unsigned int flags;

    /* check arguments */
    if (get_prop_check_argc(retval, argc,
                            is_resource_file ? &res_desc : &file_desc))
        return TRUE;

    /* initialize the flags */
    flags = 0;

    /* set the resource-file flag, if appropriate */
    if (is_resource_file)
        flags |= VMOBJFILE_IS_RESOURCE;

    /* presume we can use the entire file */
    res_start = 0;
    res_end = 0;

    /* retrieve the filename and access mode */
    get_filename_and_access(vmg_ fname, sizeof(fname),
                            &access, is_resource_file);

    /* open the file in binary mode, with the desired access type */
    switch(access)
    {
    case VMOBJFILE_ACCESS_READ:
        /* open the resource or ordinary file, as appropriate */
        if (is_resource_file)
        {
            unsigned long res_len;

            /* it's a resource - open it */
            fp = G_host_ifc->find_resource(fname, strlen(fname), &res_len);

            /* 
             *   if we found the resource, note the start and end seek
             *   positions, so we can limit reading of the underlying file
             *   to the section that contains the resource data 
             */
            if (fp != 0)
            {
                /* the file is initially at the start of the resource data */
                res_start = osfpos(fp);

                /* note the offset of the first byte after the resource */
                res_end = res_start + res_len;
            }
        }
        else
        {
            /* open the ordinary file in binary mode for reading only */
            fp = osfoprb(fname, OSFTBIN);
        }

        /* make sure we were able to find it and open it */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_not_found_exc,
                                           0, "file not found");
        break;

    case VMOBJFILE_ACCESS_WRITE:
        /* open in binary mode for writing only */
        fp = osfopwb(fname, OSFTBIN);

        /* make sure we were able to create the file successfully */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_creation_exc,
                                           0, "error creating file");
        break;

    case VMOBJFILE_ACCESS_RW_KEEP:
        /* open for read/write, keeping existing contents */
        fp = osfoprwb(fname, OSFTBIN);

        /* make sure we were able to find or create the file */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                           0, "error opening file");
        break;

    case VMOBJFILE_ACCESS_RW_TRUNC:
        /* open for read/write, truncating existing contents */
        fp = osfoprwtb(fname, OSFTBIN);

        /* make sure we were successful */
        if (fp == 0)
            G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                           0, "error opening file");
        break;

    default:
        fp = 0;
        err_throw(VMERR_BAD_VAL_BIF);
    }

    /* create our file object */
    retval->set_obj(create(vmg_ FALSE, VM_INVALID_OBJ, fp,
                           flags, mode, access, FALSE, res_start, res_end));

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check the safety settings to determine if an open operation is allowed.
 *   If the access is not allowed, we'll throw an error.  
 */
void CVmObjFile::check_safety_for_open(VMG_ const char *fname, int access)
{
    int safety;
    int in_same_dir;
    
    /* get the current file safety level from the host application */
    safety = G_host_ifc->get_io_safety();

    /* 
     *   Check to see if the file is in the current directory - if not, we
     *   may have to disallow the operation based on safety level settings.
     *   If the file has any sort of directory prefix, assume it's not in
     *   the same directory; if not, it must be.  This is actually overly
     *   conservative, since the path may be a relative path or even an
     *   absolute path that points to the current directory, but the
     *   important thing is whether we're allowing files to specify paths at
     *   all.  
     */
    in_same_dir = (os_get_root_name((char *)fname) == fname);

    /* check for conformance with the safety level setting */
    switch (access)
    {
    case VMOBJFILE_ACCESS_READ:
        /*
         *   we want only read access - we can't read at all if the safety
         *   level isn't READ_CUR or below, and we must be at level
         *   READ_ANY_WRITE_CUR or lower to read from a file not in the
         *   current directory 
         */
        if (safety > VM_IO_SAFETY_READ_CUR
            || (!in_same_dir && safety > VM_IO_SAFETY_READ_ANY_WRITE_CUR))
        {
            /* this operation is not allowed - throw an error */
            G_interpreter->throw_new_class(vmg_ G_predef->file_safety_exc,
                                           0, "prohibited file access");
        }
        break;
        
    case VMOBJFILE_ACCESS_WRITE:
    case VMOBJFILE_ACCESS_RW_KEEP:
    case VMOBJFILE_ACCESS_RW_TRUNC:
        /* 
         *   writing - we must be safety level of at least READWRITE_CUR to
         *   write at all, and we must be at level MINIMUM to write a file
         *   that's not in the current directory 
         */
        if (safety > VM_IO_SAFETY_READWRITE_CUR
            || (!in_same_dir && safety > VM_IO_SAFETY_MINIMUM))
        {
            /* this operation is not allowed - throw an error */
            G_interpreter->throw_new_class(vmg_ G_predef->file_safety_exc,
                                           0, "prohibited file access");
        }
        break;
    }
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
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure it's really a character set object */
    if (G_stk->get(0)->typ != VM_NIL
        && (G_stk->get(0)->typ != VM_OBJ
            || !CVmObjCharSet::is_charset(vmg_ G_stk->get(0)->val.obj)))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* remember the new character set */
    if (G_stk->get(0)->typ == VM_NIL)
        get_ext()->charset = VM_INVALID_OBJ;
    else
        get_ext()->charset = G_stk->get(0)->val.obj;

    /* discard the argument */
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

    /* close the underlying system file */
    osfcls(get_ext()->fp);

    /* forget the underlying system file, since it's no longer valid */
    get_ext()->fp = 0;

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
    note_file_seek(vmg_ self, FALSE);

    /* flush stdio buffers as needed and note the read operation */
    switch_read_write_mode(FALSE);

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
    CVmObjString *str;
    size_t str_len;
    osfildef *fp = get_ext()->fp;
    vmobjfile_readbuf_t *readbuf = get_ext()->readbuf;
    CCharmapToUni *charmap;
    int is_res_file = ((get_ext()->flags & VMOBJFILE_IS_RESOURCE) != 0);

    /* we haven't yet constructed a string */
    str = 0;
    str_len = 0;

    /* get our character mapper */
    charmap = ((CVmObjCharSet *)vm_objp(vmg_ get_ext()->charset))
              ->get_to_uni(vmg0_);

    /* assume we'll fail to read anything, in which case we'll return nil */
    retval->set_nil();

    /* 
     *   push the nil value - we'll always keep our intermediate value on
     *   the stack so that the garbage collector will know it's referenced 
     */
    G_stk->push(retval);

    /*
     *   Read a line of text from the file into our buffer.  Keep going
     *   until we read an entire line; we might have to read the line in
     *   chunks, since the line might end up being longer than our buffer.  
     */
    for (;;)
    {
        wchar_t found_nl;
        char *start;
        size_t new_len;
        size_t nl_len;

        /* replenish the read buffer if it's empty */
        if (readbuf->rem == 0
            && !readbuf->refill(charmap, fp, is_res_file, get_ext()->res_end))
            break;

        /* note where we started this chunk */
        start = readbuf->ptr.getptr();

        /* scan for and remove any trailing newline */
        for (found_nl = '\0' ; readbuf->rem != 0 ;
             readbuf->ptr.inc(&readbuf->rem))
        {
            wchar_t cur;
            
            /* get the current character */
            cur = readbuf->ptr.getch();

            /* 
             *   check for a newline (note that 0x2028 is the unicode line
             *   separator character) 
             */
            if (cur == '\n' || cur == '\r' || cur == 0x2028)
            {
                /* note the newline */
                found_nl = cur;
                
                /* no need to look any further */
                break;
            }
        }
        
        /* note the length of the current segment */
        new_len = readbuf->ptr.getptr() - start;
        
        /* 
         *   if there's a newline character, include an extra byte for the
         *   '\n' we'll include in the result 
         */
        nl_len = (found_nl != '\0');
        
        /* 
         *   If this is our first segment, construct a new string from this
         *   chunk; otherwise, add to the existing string.
         *   
         *   Note that in either case, if we found a newline in the buffer,
         *   we will NOT add the actual newline we found to the result
         *   string.  Rather, we'll add a '\n' character to the result
         *   string, no matter what kind of newline we found.  This ensures
         *   that the data read uses a consistent format, regardless of the
         *   local system convention where the file was created.  
         */
        if (str == 0)
        {
            /* create our first segment's string */
            retval->set_obj(CVmObjString::
                            create(vmg_ FALSE, new_len + nl_len));
            str = (CVmObjString *)vm_objp(vmg_ retval->val.obj);
            
            /* copy the segment into the string object */
            memcpy(str->cons_get_buf(), start, new_len);

            /* add a '\n' if we found a newline */
            if (found_nl != '\0')
                *(str->cons_get_buf() + new_len) = '\n';
            
            /* this is the length of the string so far */
            str_len = new_len + nl_len;
            
            /* 
             *   replace the stack placeholder with our string, so the
             *   garbage collector will know it's still in use 
             */
            G_stk->discard();
            G_stk->push(retval);
        }
        else
        {
            CVmObjString *new_str;
            
            /* 
             *   create a new string to hold the contents of the old string
             *   plus the new buffer 
             */
            retval->set_obj(CVmObjString::create(vmg_ FALSE,
                str_len + new_len + nl_len));
            new_str = (CVmObjString *)vm_objp(vmg_ retval->val.obj);
            
            /* copy the old string into the new string */
            memcpy(new_str->cons_get_buf(),
                   str->get_as_string(vmg0_) + VMB_LEN, str_len);

            /* add the new chunk after the copy of the old string */
            memcpy(new_str->cons_get_buf() + str_len, start, new_len);

            /* add the newline if necessary */
            if (found_nl != '\0')
                *(new_str->cons_get_buf() + str_len + new_len) = '\n';
            
            /* the new string now replaces the old string */
            str = new_str;
            str_len += new_len + nl_len;
            
            /* 
             *   replace our old intermediate value on the stack with the
             *   new string - the old string isn't needed any more, so we
             *   can leave it unreferenced, but we are still using the new
             *   string 
             */
            G_stk->discard();
            G_stk->push(retval);
        }
        
        /* if we found a newline in this segment, we're done */
        if (found_nl != '\0')
        {
            /* skip the newline in the input */
            readbuf->ptr.inc(&readbuf->rem);

            /* replenish the read buffer if it's empty */
            if (readbuf->rem == 0)
                readbuf->refill(charmap, fp, is_res_file, get_ext()->res_end);

            /* 
             *   check for a complementary newline character, for systems
             *   that use \n\r or \r\n pairs 
             */
            if (readbuf->rem != 0)
            {
                wchar_t nxt;
                
                /* get the next character */
                nxt = readbuf->ptr.getch();
                
                /* check for a complementary character */
                if ((found_nl == '\n' && nxt == '\r')
                    || (found_nl == '\r' && nxt == '\n'))
                {
                    /* 
                     *   we have a pair sequence - skip the second character
                     *   of the sequence 
                     */
                    readbuf->ptr.inc(&readbuf->rem);
                }
            }
            
            /* we've found the newline, so we're done with the string */
            break;
        }
    }

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
    osfildef *fp = get_ext()->fp;

    /* read the type flag */
    if (osfrb(fp, buf, 1))
    {
        /* end of file - return nil */
        retval->set_nil();
        return;
    }

    /* see what we have */
    switch((vm_datatype_t)buf[0])
    {
    case VMOBJFILE_TAG_INT:
        /* read the INT4 value */
        if (osfrb(fp, buf, 4))
            goto io_error;

        /* set the integer value from the buffer */
        retval->set_int(osrp4(buf));
        break;

    case VMOBJFILE_TAG_ENUM:
        /* read the UINT4 value */
        if (osfrb(fp, buf, 4))
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
        if (osfrb(fp, buf, 2))
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
        if (osfrb(fp, str_obj->cons_get_buf(), osrp2(buf) - 2))
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
    switch_read_write_mode(TRUE);

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
    constp = CVmObjString::cvt_to_str(vmg_ &new_str,
                                      conv_buf, sizeof(conv_buf),
                                      val, 10);

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
    }
}

/*
 *   Write a value in 'data' mode 
 */
void CVmObjFile::write_data_mode(VMG_ const vm_val_t *val)
{
    char buf[32];
    osfildef *fp = get_ext()->fp;
    vm_val_t new_str;
    const char *constp;

    /* see what type of data we want to put */
    switch(val->typ)
    {
    case VM_INT:
        /* put the type in the buffer */
        buf[0] = VMOBJFILE_TAG_INT;
        
        /* add the value in INT4 format */
        oswp4(buf + 1, val->val.intval);
        
        /* write out the type prefix plus the value */
        if (osfwb(fp, buf, 5))
            goto io_error;

        /* done */
        break;

    case VM_ENUM:
        /* put the type in the buffer */
        buf[0] = VMOBJFILE_TAG_ENUM;

        /* add the value in INT4 format */
        oswp4(buf + 1, val->val.enumval);

        /* write out the type prefix plus the value */
        if (osfwb(fp, buf, 5))
            goto io_error;

        /* done */
        break;

    case VM_SSTRING:
        /* get the string value pointer */
        constp = val->get_as_string(vmg0_);

    write_binary_string:
        /* write the type prefix byte */
        buf[0] = VMOBJFILE_TAG_STRING;
        if (osfwb(fp, buf, 1))
            goto io_error;

        /* 
         *   write the length prefix - for TADS 2 compatibility, include
         *   the bytes of the prefix itself in the length count 
         */
        oswp2(buf, vmb_get_len(constp) + 2);
        if (osfwb(fp, buf, 2))
            goto io_error;

        /* write the string's bytes */
        if (osfwb(fp, constp + VMB_LEN, vmb_get_len(constp)))
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
            if (osfwb(fp, buf, 1))
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
            if (osfwb(fp, buf, 1))
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
        if (osfwb(fp, buf, 1))
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
    int is_res_file = ((get_ext()->flags & VMOBJFILE_IS_RESOURCE) != 0);

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
    note_file_seek(vmg_ self, FALSE);

    /* deal with stdio buffering issues if necessary */
    switch_read_write_mode(FALSE);

    /* 
     *   limit the reading to the remaining data in the file, if it's a
     *   resource file 
     */
    if (is_res_file)
    {
        unsigned long cur_seek_pos;

        /* check to see where we are relative to the end of the resource */
        cur_seek_pos = osfpos(get_ext()->fp);
        if (cur_seek_pos >= get_ext()->res_end)
        {
            /* we're already past the end - there's nothing left */
            len = 0;
        }
        else
        {
            unsigned long limit;

            /* calculate the limit */
            limit = get_ext()->res_end - cur_seek_pos;

            /* apply the limit if the request exceeds it */
            if (len > limit)
                len = limit;
        }
    }

    /* 
     *   read the data into the array, and return the number of bytes we
     *   actually manage to read 
     */
    retval->set_int(arr->read_from_file(get_ext()->fp, idx, len));

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
    vm_val_t arr_val;
    CVmObjByteArray *arr;
    unsigned long idx;
    unsigned long len;

    /* check arguments */
    if (get_prop_check_argc(retval, in_argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* check that we have write access */
    check_write_access(vmg0_);

    /* make sure the byte array argument is really a byte array */
    G_stk->pop(&arr_val);
    if (arr_val.typ != VM_OBJ
        || !CVmObjByteArray::is_byte_array(vmg_ arr_val.val.obj))
        err_throw(VMERR_BAD_TYPE_BIF);
    
    /* we know it's a byte array, so we can simply cast it */
    arr = (CVmObjByteArray *)vm_objp(vmg_ arr_val.val.obj);

    /* assume we'll write the entire byte array */
    idx = 1;
    len = arr->get_element_count();

    /* if we have a starting index, retrieve it */
    if (argc >= 2)
        idx = (unsigned long)CVmBif::pop_int_val(vmg0_);

    /* if we have a length, retrieve it */
    if (argc >= 3)
        len = (unsigned long)CVmBif::pop_int_val(vmg0_);

    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* flush stdio buffers as needed and note the read operation */
    switch_read_write_mode(TRUE);

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

    /* discard our gc protection */
    G_stk->discard();

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
    unsigned long cur_pos;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* get the current seek position */
    cur_pos = osfpos(get_ext()->fp);

    /* if this is a resource file, adjust for the base offset */
    cur_pos -= get_ext()->res_start;

    /* return the seek position */
    retval->set_int(cur_pos);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - set the seek position
 */
int CVmObjFile::getp_set_pos(VMG_ vm_obj_id_t self, vm_val_t *retval,
                             uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    int is_res_file = ((get_ext()->flags & VMOBJFILE_IS_RESOURCE) != 0);
    unsigned long pos;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* note the seeking operation */
    note_file_seek(vmg_ self, TRUE);

    /* retrieve the target seek position */
    pos = CVmBif::pop_long_val(vmg0_);

    /* adjust for the resource base offset */
    pos += get_ext()->res_start;

    /* 
     *   if this is a resource file, move the position at most to the first
     *   byte after the end of the resource 
     */
    if (is_res_file && pos > get_ext()->res_end)
        pos = get_ext()->res_end;

    /* seek to the new position */
    osfseek(get_ext()->fp, pos, OSFSK_SET);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - set position to end of file
 */
int CVmObjFile::getp_set_pos_end(VMG_ vm_obj_id_t self, vm_val_t *retval,
                                 uint *argc)
{
    static CVmNativeCodeDesc desc(0);
    int is_res_file = ((get_ext()->flags & VMOBJFILE_IS_RESOURCE) != 0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* note the seeking operation */
    note_file_seek(vmg_ self, TRUE);

    /* handle according to whether it's a resource or not */
    if (is_res_file)
    {
        /* resource - seek to the first byte after the resource data */
        osfseek(get_ext()->fp, get_ext()->res_end, OSFSK_SET);
    }
    else
    {
        /* normal file - simply seek to the end of the file */
        osfseek(get_ext()->fp, 0, OSFSK_END);
    }

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
    int is_res_file = ((get_ext()->flags & VMOBJFILE_IS_RESOURCE) != 0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* make sure we are allowed to perform operations on the file */
    check_valid_file(vmg0_);

    /* note the seeking operation */
    note_file_seek(vmg_ self, TRUE);

    /* handle according to whether it's a resource or not */
    if (is_res_file)
    {
        /* resource - we know the size from the resource descriptor */
        retval->set_int(get_ext()->res_end - get_ext()->res_start + 1);
    }
    else
    {
        osfildef *fp = get_ext()->fp;
        unsigned long cur_pos;
        
        /* 
         *   It's a normal file.  Remember the current seek position, then
         *   seek to the end of the file.  
         */
        cur_pos = osfpos(fp);
        osfseek(fp, 0, OSFSK_END);

        /* the current position gives us the length of the file */
        retval->set_int(osfpos(fp));

        /* seek back to where we started */
        osfseek(fp, cur_pos, OSFSK_SET);
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Refill the read buffer.  Returns true if the buffer contains any data
 *   on return, false if we're at end of file.  
 */
int vmobjfile_readbuf_t::refill(CCharmapToUni *charmap,
                                osfildef *fp, int is_res_file,
                                unsigned long res_seek_end)
{
    unsigned long read_limit;
    
    /* if the buffer isn't empty, ignore the request */
    if (rem != 0)
        return TRUE;

    /* presume there's no read limit */
    read_limit = 0;

    /* if it's a resource file, limit the size */
    if (is_res_file)
    {
        unsigned long cur_seek_ofs;

        /* make sure we're not already past the end */
        cur_seek_ofs = osfpos(fp);
        if (cur_seek_ofs >= res_seek_end)
            return FALSE;

        /* calculate the amount of data remaining in the resource */
        read_limit = res_seek_end - cur_seek_ofs;
    }
    
    /* read the text */
    rem = charmap->read_file(fp, buf, sizeof(buf), read_limit);

    /* read from the start of the buffer */
    ptr.set(buf);

    /* indicate that we have more data to read */
    return (rem != 0);
}



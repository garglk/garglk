#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtmpfil.cpp - CVmObjTemporaryFile object
Function
  
Notes
  
Modified
  11/20/10 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmtmpfil.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmmcreg.h"
#include "vmfile.h"
#include "vmbif.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmstr.h"
#include "utf8.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_tmpfil_ext *vm_tmpfil_ext::alloc_ext(VMG_ CVmObjTemporaryFile *self,
                                        const char *fname)
{
    /* calculate how much space we need */
    size_t siz = sizeof(vm_tmpfil_ext) + strlen(fname);

    /* allocate the memory */
    vm_tmpfil_ext *ext = (vm_tmpfil_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);

    /* copy the filename */
    strcpy(ext->filename, fname);

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTemporaryFile object statics 
 */

/* metaclass registration object */
static CVmMetaclassTemporaryFile metaclass_reg_obj;
CVmMetaclass *CVmObjTemporaryFile::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjTemporaryFile::*CVmObjTemporaryFile::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjTemporaryFile::getp_undef,                                  /* 0 */
    &CVmObjTemporaryFile::getp_getFilename,                            /* 1 */
    &CVmObjTemporaryFile::getp_deleteFile                              /* 2 */
};

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTemporaryFile intrinsic class implementation 
 */

/*
 *   construction
 */
CVmObjTemporaryFile::CVmObjTemporaryFile(VMG_ const char *fname)
{
    /* allocate our extension structure */
    ext_ = (char *)vm_tmpfil_ext::alloc_ext(vmg_ this, fname);
}

/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjTemporaryFile::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    /* check arguments */
    if (argc != 0)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* generate a temporary filename in the local file system */
    char fname[OSFNMAX];
    if (!os_gen_temp_filename(fname, sizeof(fname)))
        err_throw(VMERR_CREATE_FILE);

    /* allocate the object ID and create the object */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
    new (vmg_ id) CVmObjTemporaryFile(vmg_ fname);

    /* temporary file objects are always transient */
    G_obj_table->set_obj_transient(id);

    /* return the new ID */
    return id;
}

/* 
 *   notify of deletion 
 */
void CVmObjTemporaryFile::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
    {
        /* delete the local file system object */
        if (get_ext()->filename[0] != '\0')
            osfdel(get_ext()->filename);

        /* free the extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* 
 *   set a property 
 */
void CVmObjTemporaryFile::set_prop(VMG_ class CVmUndo *undo,
                                   vm_obj_id_t self, vm_prop_id_t prop,
                                   const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* 
 *   get a property 
 */
int CVmObjTemporaryFile::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
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
    
    /* inherit default handling from our base class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/*
 *   apply an undo record 
 */
void CVmObjTemporaryFile::apply_undo(VMG_ struct CVmUndoRecord *)
{
    /* we're immutable, so there's no undo to worry about */
}

/*
 *   discard extra undo information 
 */
void CVmObjTemporaryFile::discard_undo(VMG_ CVmUndoRecord *)
{
    /* we don't create undo records */
}

/* 
 *   load from an image file 
 */
void CVmObjTemporaryFile::load_from_image(VMG_ vm_obj_id_t self,
                                          const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image file 
 */
void CVmObjTemporaryFile::reload_from_image(VMG_ vm_obj_id_t self,
                                            const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjTemporaryFile::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
    
    /* 
     *   Allocate the extension.  A temp file object loaded from the image is
     *   invalid; these objects are inherently transient.  So simply use an
     *   empty filename to indicate that the temp file is not openable. 
     */
    vm_tmpfil_ext *ext = vm_tmpfil_ext::alloc_ext(vmg_ this, "");
    ext_ = (char *)ext;
}


/* 
 *   save to a file 
 */
void CVmObjTemporaryFile::save_to_file(VMG_ class CVmFile *)
{
    /* temporary files are always transient, so should never be saved */
    assert(FALSE);
}

/* 
 *   restore from a file 
 */
void CVmObjTemporaryFile::restore_from_file(VMG_ vm_obj_id_t /*self*/,
                                            CVmFile *, CVmObjFixup *)
{
    /* temporary files are transient, so should never be saved */
}

/* ------------------------------------------------------------------------ */
/*
 *   getFilename() method - get the local file system object's name.
 */
int CVmObjTemporaryFile::getp_getFilename(VMG_ vm_obj_id_t self,
                                          vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* if we have a filename, return it, otherwise return nil */
    vm_tmpfil_ext *ext = get_ext();
    if (ext->filename[0] != '\0')
    {
        /* return the filename */
        retval->set_obj(CVmBif::str_from_ui_str(vmg_ ext->filename));
    }
    else
    {
        /* no filename */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   deleteFile() method.  This explicitly releases the temp file, deleting
 *   the underlying file system object.  It's not necessary for the program
 *   to call this, as we'll automatically release the file when this object
 *   is deleted by the garbage collector, but this method allows the program
 *   to explicitly release the resource as soon as it's done with it.  
 */
int CVmObjTemporaryFile::getp_deleteFile(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* if we have a filename, delete the file */
    vm_tmpfil_ext *ext = get_ext();
    if (ext->filename[0] != '\0')
    {
        /* delete the system file */
        osfdel(ext->filename);
    }

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

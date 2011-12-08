#ifdef RCSID
static char RCSid[] =
    "$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnetfil.cpp - network file operations, local mode 
Function
  This module contains network file functions that are needed in both
  server and local modes.  This file should be linked into all builds,
  whether or not the server mode is used.
Notes

Modified
  09/08/10 MJRoberts  - Creation
*/

#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnetfil.h"
#include "vmnet.h"
#include "vmfile.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmrun.h"
#include "vmglob.h"
#include "vmpredef.h"
#include "vmimport.h"
#include "sha2.h"
#include "vmhash.h"
#include "vmbif.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmtmpfil.h"
#include "vmtobj.h"
#include "vmimage.h"


/* ------------------------------------------------------------------------ */
/*
 *   Open a network file 
 */
CVmNetFile *CVmNetFile::open(VMG_ const vm_val_t *val, const vm_rcdesc *rc,
                             int mode, os_filetype_t typ,
                             const char *mime_type)
{
    vm_val_t filespec;
    
    /* check for a TadsObject implementing getFilename */
    if (G_predef->filespec_getFilename != VM_INVALID_PROP
        && val->typ == VM_OBJ
        && CVmObjTads::is_tadsobj_obj(vmg_ val->val.obj))
    {
        /* call getFilename - the return value is the file spec */
        G_interpreter->get_prop(
            vmg_ 0, val, G_predef->filespec_getFilename, val, 0, rc);

        /* the result is the real file spec */
        filespec = *G_interpreter->get_r0();
    }
    else
    {
        /* it's not a TadsObject, so it must directly have the file name */
        filespec = *val;
    }

    /* check the argument type */
    CVmNetFile *nf = 0;
    if (filespec.typ == VM_OBJ
        && CVmObjTemporaryFile::is_tmpfil_obj(vmg_ filespec.val.obj))
    {
        /* temporary file object - get the object, properly cast */
        CVmObjTemporaryFile *tmp = (CVmObjTemporaryFile *)vm_objp(
            vmg_ filespec.val.obj);

        /* if the temporary file object is invalid, it's an error */
        if (tmp->get_fname() == 0)
            err_throw(VMERR_CREATE_FILE);

        /* create the local file descriptor for the temp file path */
        nf = open_local(vmg_ tmp->get_fname(), 0, mode, typ);

        /* mark it as a temp file */
        nf->is_temp = TRUE;
    }
    else
    {
        /* anything else has to be a string */
        char fname[OSFNMAX];
        CVmBif::get_str_val_fname(vmg_ fname, sizeof(fname),
                                  CVmBif::get_str_val(vmg_ &filespec));

        /* 
         *   if it's a local file, and it has a relative path, explicitly
         *   apply the image file path as the default working directory 
         */
        if (!os_is_file_absolute(fname) && !is_net_mode(vmg0_))
        {
            /* build the full, absolute name based on the image file path */
            char fname_abs[OSFNMAX];
            os_build_full_path(fname_abs, sizeof(fname_abs),
                               G_image_loader->get_path(), fname);

            /* replace the relative path with the new absolute path */
            lib_strcpy(fname, sizeof(fname), fname_abs);
        }

        /* create the regular network file descriptor */
        nf = open(vmg_ fname, 0, mode, typ, mime_type);
    }

    /* if they gave us an object as our file spec, remember it */
    if (nf != 0 && val->typ == VM_OBJ)
        nf->filespec = val->val.obj;

    /* return the network file descriptor */
    return nf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Mark references for the garbage collector 
 */
void CVmNetFile::mark_refs(VMG_ uint state)
{
    /* if we have a filespec object, mark it */
    if (filespec != VM_INVALID_OBJ)
        G_obj_table->mark_all_refs(filespec, state);
}


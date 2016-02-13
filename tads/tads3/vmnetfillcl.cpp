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
#include "vmstack.h"
#include "charmap.h"
#include "vmlst.h"
#include "vmfilnam.h"
#include "vmfilobj.h"
#include "vmstr.h"


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
        && vm_val_cast_ok(CVmObjTads, val))
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

    /* check the file spec argument type */
    CVmNetFile *nf = 0;
    CVmObjTemporaryFile *tmp = 0;
    CVmObjFileName *ofn = 0;
    if ((tmp = vm_val_cast(CVmObjTemporaryFile, &filespec)) != 0)
    {
        /* if the temporary file object is invalid, it's an error */
        if (tmp->get_fname() == 0)
            err_throw(VMERR_CREATE_FILE);

        /* create the local file descriptor for the temp file path */
        nf = open_local(vmg_ tmp->get_fname(), 0, mode, typ);

        /* mark it as a temp file */
        nf->is_temp = TRUE;
    }
    else if (filespec.is_numeric(vmg0_)
             || ((ofn = vm_val_cast(CVmObjFileName, &filespec)) != 0
                 && ofn->is_special_file()))
    {
        /* 
         *   It's a special file ID, either as an integer or as a FileName
         *   wrapping a special file int.  Get the value. 
         */
        int32_t sfid = (ofn != 0 ? ofn->get_sfid()
                                 : filespec.num_to_int(vmg0_));

        /* resolve the file system path for the given special file ID */
        char fname[OSFNMAX] = { '\0' };
        if (!CVmObjFile::sfid_to_path(vmg_ fname, sizeof(fname), sfid))
            err_throw(VMERR_BAD_VAL_BIF);

        /* create the special file descriptor */
        nf = open(vmg_ fname, sfid, mode, typ, mime_type);
    }
    else
    {
        /* anything else has to be a string */
        char fname[OSFNMAX];
        CVmBif::get_fname_val(vmg_ fname, sizeof(fname), &filespec);

        /* 
         *   if it's a local file, and it has a relative path, explicitly
         *   apply the image file path as the default working directory 
         */
        if (!os_is_file_absolute(fname) && !is_net_mode(vmg0_))
        {
            /* build the full, absolute name based on the file base path */
            char fnabs[OSFNMAX];
            os_build_full_path(fnabs, sizeof(fnabs), G_file_path, fname);

            /* replace the relative path with the new absolute path */
            lib_strcpy(fname, sizeof(fname), fnabs);
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
 *   Rename a file
 */
void CVmNetFile::rename_to_local(VMG_ CVmNetFile *newname)
{
    /* if the new name isn't local, this isn't supported */
    if (newname->is_net_file())
        err_throw(VMERR_RENAME_FILE);

    /* if the destination file already exists, it's an error */
    if (!osfacc(newname->lclfname))
        err_throw(VMERR_RENAME_FILE);

    /* do the rename */
    if (!os_rename_file(lclfname, newname->lclfname))
        err_throw(VMERR_RENAME_FILE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a local directory 
 */
void CVmNetFile::mkdir_local(VMG_ int create_parents)
{
    /* try creating the directory */
    if (!os_mkdir(lclfname, create_parents))
        err_throw(VMERR_CREATE_FILE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Empty a local directory 
 */
static void empty_dir(VMG_ const char *dir)
{
    /* open the directory search */
    osdirhdl_t dirhdl;
    if (os_open_dir(dir, &dirhdl))
    {
        err_try
        {
            /* keep going until we're out of files */
            char fname[OSFNMAX];
            while (os_read_dir(dirhdl, fname, sizeof(fname)))
            {
                /* get the full path */
                char path[OSFNMAX];
                os_build_full_path(path, sizeof(path), dir, fname);

                /* get the mode */
                unsigned long fmode;
                unsigned long fattr;
                if (osfmode(path, FALSE, &fmode, &fattr))
                {
                    /* check whether it's a directory or an ordinary file */
                    if ((fmode & OSFMODE_DIR) != 0)
                    {
                        /* 
                         *   directory - skip the special '.' and '..' links,
                         *   since they'd get us stuck in a loop 
                         */
                        os_specfile_t st = os_is_special_file(fname);
                        if (st != OS_SPECFILE_SELF && st != OS_SPECFILE_PARENT)
                        {
                            /* recursively empty the directory */
                            empty_dir(vmg_ path);
                            
                            /* remove this directory */
                            if (!os_rmdir(path))
                                err_throw(VMERR_DELETE_FILE);
                        }
                    }
                    else
                    {
                        /* ordinary file - delete it */
                        if (osfdel(path))
                            err_throw(VMERR_DELETE_FILE);
                    }
                }
            }
        }
        err_finally
        {
            /* close the directory search handle */
            os_close_dir(dirhdl);
        }
        err_end;
    }
}       

/*
 *   Remove a local directory 
 */
void CVmNetFile::rmdir_local(VMG_ int remove_contents)
{
    /* if desired, recursively remove the directory's contents */
    if (remove_contents)
        empty_dir(vmg_ lclfname);

    /* try removing the directory */
    if (!os_rmdir(lclfname))
        err_throw(VMERR_DELETE_FILE);
}

/* ------------------------------------------------------------------------ */
/* 
 *   static directory lister 
 */
static int s_readdir_local(VMG_ const char *lclfname,
                           const char *nominal_path,
                           vm_val_t *retval, const vm_rcdesc *rc,
                           const vm_val_t *cb, int recursive)
{
    /* note the nominal path string length */
    size_t nominal_path_len = strlen(nominal_path);

    /* 
     *   If the caller wants a result list, create a list for the return
     *   value.  We don't know how many elements the list will need, so
     *   create it with an arbitrary initial size, and expand it later as
     *   needed.
     */
    CVmObjList *lst = 0;
    if (retval != 0)
    {
        /* create the list */
        const size_t initlen = 32;
        retval->set_obj(CVmObjList::create(vmg_ FALSE, initlen));
        lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

        /* clear it */
        lst->cons_clear(0, initlen - 1);

        /* save the new list on the stack for gc protection */
        G_stk->push(retval);
    }

    /* presume success */
    int ok = TRUE;

    /* open the directory */
    osdirhdl_t dirhdl;
    if (os_open_dir(lclfname, &dirhdl))
    {
        err_try
        {
            /* iterate over the directory's contents */
            int idx = 0;
            char curname[OSFNMAX];
            while (os_read_dir(dirhdl, curname, sizeof(curname)))
            {
                char *unm = 0;
                vm_obj_id_t fnobj = VM_INVALID_OBJ;

                err_try
                {
                    /* map the filename to UTF8 */
                    size_t unmlen = G_cmap_from_fname->map_str_alo(
                        &unm, curname);
                
                    /* create the FileName object */
                    fnobj = CVmObjFileName::combine_path(
                        vmg_ nominal_path, nominal_path_len,
                        unm, unmlen, TRUE);
                
                    /* push it for gc protection */
                    G_stk->push()->set_obj(fnobj);
                }
                err_finally
                {
                    if (unm != 0)
                        t3free(unm);
                }
                err_end;
            
                /* if we're building a list, add the file to the list */
                if (retval != 0)
                {
                    vm_val_t ele;
                    ele.set_obj(fnobj);
                    lst->cons_ensure_space(vmg_ idx, 16);
                    lst->cons_set_element(idx, &ele);
                    lst->cons_set_len(++idx);
                }
            
                /* if there's a callback, invoke it */
                if (cb != 0)
                {
                    G_stk->push()->set_obj(fnobj);
                    G_interpreter->call_func_ptr(vmg_ cb, 1, rc, 0);
                }
            
                /* 
                 *   If we're doing a recursive listing, and this is a
                 *   directory, list its ocntents.  Skip self and parent
                 *   directory links ('.'  and '..' on Unix), since those
                 *   would cause infinite recursion.
                 */
                os_specfile_t st;
                if (recursive
                    && (st = os_is_special_file(curname)) != OS_SPECFILE_SELF
                    && st != OS_SPECFILE_PARENT)
                {
                    /* build the full path name */
                    char fullname[OSFNMAX];
                    os_build_full_path(fullname, sizeof(fullname),
                                       lclfname, curname);

                    /* check to see if it's a directory */
                    unsigned long fmode;
                    unsigned long fattr;
                    if (osfmode(fullname, FALSE, &fmode, &fattr)
                        && (fmode & OSFMODE_DIR) != 0)
                    {
                        /* get the combined path from the FileName object */
                        const char *path = vm_objid_cast(CVmObjFileName, fnobj)
                                           ->get_path_string();
                
                        /* build the actual combined path */
                        char subfname[OSFNMAX];
                        os_build_full_path(
                            subfname, sizeof(subfname), lclfname, curname);
                
                        /* do the recursive listing */
                        ok |= s_readdir_local(vmg_ subfname, path + VMB_LEN,
                                              retval, rc, cb, TRUE);
                    }
                }
            
                /* we're done with the FileName object for this iteration */
                G_stk->discard(1);
            }
        }
        err_finally
        {
            /* close the directory handle */
            os_close_dir(dirhdl);
        }
        err_end;
    }

    /* discard the gc protection for the list, if applicable */
    if (retval != 0)
        G_stk->discard(1);

    /* return the result */
    return ok;
}

/*
 *   Get a local directory listing, returning a list of FileName objects
 *   and/or invoking a callback for each file found.
 */
int CVmNetFile::readdir_local(VMG_ const char *nominal_path,
                              vm_val_t *retval,
                              const struct vm_rcdesc *rc,
                              const vm_val_t *cb, int recursive)
{
    /* verify that the path exists and refers to a directory */
    unsigned long mode;
    unsigned long attr;
    if (!osfmode(lclfname, TRUE, &mode, &attr)
        || (mode & OSFMODE_DIR) == 0)
        return FALSE;

    /* 
     *   if the caller didn't specify a nominal path to use in constructed
     *   FileName objects, use the actual local path 
     */
    if (nominal_path == 0)
        nominal_path = lclfname;

    /* call our static implementation with our local filename path */
    return s_readdir_local(vmg_ lclfname, nominal_path,
                           retval, rc, cb, recursive);
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


#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2012 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfilnam.cpp - CVmObjFileName object
Function
  
Notes
  
Modified
  03/03/12 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmfilnam.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmmcreg.h"
#include "vmfile.h"
#include "vmfilobj.h"
#include "vmbif.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmstr.h"
#include "utf8.h"
#include "charmap.h"
#include "vmnetfil.h"
#include "vmpredef.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_filnam_ext *vm_filnam_ext::alloc_ext(
    VMG_ CVmObjFileName *self, int32_t sfid, const char *str, size_t len)
{
    /* 
     *   Calculate how much space we need - we need space for the name string
     *   plus its length prefix; store the name string with a null terminator
     *   for convenience in calling osifc routines.
     */
    size_t siz = (sizeof(vm_filnam_ext)-1) + (VMB_LEN + len + 1);

    /* allocate the memory */
    vm_filnam_ext *ext = (vm_filnam_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);

    /* store the special file ID; assume it's valid */
    ext->sfid = sfid;
    ext->sfid_valid = TRUE;

    /* assume that the file didn't come from a manual user interaction */
    ext->from_ui = 0;

    /* set the string length and null terminator */
    vmb_put_len(ext->str, len);
    ext->str[VMB_LEN + len] = '\0';

    /* copy the string, if provided */
    if (str != 0)
        memcpy(ext->str + VMB_LEN, str, len);

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjFileName object statics 
 */

/* metaclass registration object */
static CVmMetaclassFileName metaclass_reg_obj;
CVmMetaclass *CVmObjFileName::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjFileName::*CVmObjFileName::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjFileName::getp_undef,                                       /* 0 */
    &CVmObjFileName::getp_getName,                                     /* 1 */
    &CVmObjFileName::getp_getRootName,                                 /* 2 */
    &CVmObjFileName::getp_getPath,                                     /* 3 */
    &CVmObjFileName::getp_fromUniversal,                               /* 4 */
    &CVmObjFileName::getp_toUniversal,                                 /* 5 */
    &CVmObjFileName::getp_addToPath,                                   /* 6 */
    &CVmObjFileName::getp_isAbsolute,                                  /* 7 */
    &CVmObjFileName::getp_getAbsolutePath,                             /* 8 */
    &CVmObjFileName::getp_getRootDirs,                                 /* 9 */
    &CVmObjFileName::getp_getFileType,                                /* 10 */
    &CVmObjFileName::getp_getFileInfo,                                /* 11 */
    &CVmObjFileName::getp_deleteFile,                                 /* 12 */
    &CVmObjFileName::getp_renameFile,                                 /* 13 */
    &CVmObjFileName::getp_listDir,                                    /* 14 */
    &CVmObjFileName::getp_forEachFile,                                /* 15 */
    &CVmObjFileName::getp_createDirectory,                            /* 16 */
    &CVmObjFileName::getp_removeDirectory                             /* 17 */
};

/* property indices */
const int VMOFN_FROMUNIVERSAL = 4;
const int VMOFN_GETROOTDIRS = 9;
const int VMOFN_GETFILETYPE = 10;
const int VMOFN_GETFILEINFO = 11;
const int VMOFN_DELETEFILE = 12;
const int VMOFN_RENAMEFILE = 13;
const int VMOFN_LISTDIR = 14;
const int VMOFN_FOREACHFILE = 15;
const int VMOFN_CREATEDIR = 16;
const int VMOFN_REMOVEDIR = 17;



/* ------------------------------------------------------------------------ */
/*
 *   Cover functions for the various OS path operations.  These test the
 *   network storage mode: if we're in network mode, we use the storage
 *   server syntax, which follows Unix rules regardless of the local OS.
 *   Otherwise we use the local OS rules, via the osifc functions.
 */

/* canonicalize a network path: resolve . and .. links */
static void nf_canonicalize(char *path)
{
    /* note if it's an absolute path */
    int abs = path[0] == '/';

    /* make a list of the path elements */
    char **ele = new char*[strlen(path)];
    int cnt;
    for (cnt = 0, ele[0] = strtok(path, "/") ; ele[cnt] != 0 ;
         ele[++cnt] = strtok(0, "/")) ;

    /* remove '.' and empty elements, and cancel '..' against prior elements */
    int src, dst;
    for (src = dst = 0 ; src < cnt ; )
    {
        if (ele[src][0] == '\0' || strcmp(ele[src], ".") == 0)
        {
            /* empty or '.' - simply omit it */
            ++src;
        }
        else if (strcmp(ele[src], "..") == 0)
        {
            /* 
             *   '..' - cancel against the previous element if there is one
             *   and it's not '..', otherwise keep it 
             */
            if (dst > 0 && strcmp(ele[dst-1], "..") != 0)
                --dst, ++src;
            else
                ele[dst++] = ele[src++];
        }
        else
        {
            /* ordinary element - copy it */
            ele[dst++] = ele[src++];
        }
    }

    /* reconstruct the string */
    char *p = path;
    if (abs)
        *p++ = '/';
    for (int i = 0 ; i < dst ; ++i)
    {
        size_t l = strlen(ele[i]);
        memcpy(p, ele[i], l);
        p += l;
        if (i + 1 < dst)
            *p++ = '/';
    }
    *p = '\0';

    /* if it's complete empty, make it '.' */
    if (path[0] == '\0')
        path[0] = '.', path[1] = '\0';

    /* done with the path list */
    delete [] ele;
}

/* build a full path */
static void fn_build_full_path(
    VMG_ char *buf, size_t buflen,
    const char *dir, const char *fname, int literal)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* 
         *   Network storage mode - use Unix rules.  First, if the filename
         *   is in absolute format already, use it unchanged.  If not, append
         *   the filename to the directory, adding a '/' between the two if
         *   the directory doesn't already end with a slash.
         */
        if (fname[0] == '/')
        {
            /* the file is absolute - return it unchanged */
            lib_strcpy(buf, buflen, fname);
        }
        else
        {
            /* note the last character of the directory path */
            size_t dirlen = strlen(dir);
            char dirlast = dirlen != 0 ? dir[dirlen-1] : 0;

            /* build the result */
            t3sprintf(buf, buflen, "%s%s%s",
                      dir, dirlast == '/' ? "" : "/", fname);
        }

        /* if not in literal mode, canonicalize the result */
        if (!literal)
            nf_canonicalize(buf);
    }
    else
    {
        /* local storage mode - use local OS rules */
        if (literal)
            os_combine_paths(buf, buflen, dir, fname);
        else
            os_build_full_path(buf, buflen, dir, fname);
    }
}

/* get the root name from a path string */
static const char *fn_get_root_name(VMG_ const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* 
         *   network storage mode - use Unix rules, so simply find the
         *   rightmost '/' and return the portion after it (or the whole
         *   name, if there's no '/') 
         */
        const char *r = strrchr(str, '/');
        return r != 0 ? r + 1 : str;
    }
    else
    {
        /* local storage mode - use local OS rules */
        return os_get_root_name(str);
    }
}

/* get the path name portion from a filename string */
static void fn_get_path_name(VMG_ char *buf, size_t buflen, const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* 
         *   Network storage mode - use Unix rules, so simply find the
         *   rightmost '/' and return the portion before it (or an empty
         *   string, if there's no '/').  If the right most '/' is the first
         *   character, or is only preceded by '/' characters, it's a root
         *   path, so return '/' as the path.
         */
        const char *r = strrchr(str, '/');
        if (r == 0)
        {
            /* there's no '/' at all, so there's no path */
            buf[0] = '\0';
        }
        else
        {
            /* skip any more '/'s immediately preceding the one we found */
            for ( ; r > str && *(r-1) == '/' ; --r) ;

            /* if we're at the first character, it's a root path */
            if (r == str)
            {
                /* return "/" for the root path */
                lib_strcpy(buf, buflen, "/");
            }
            else
            {
                /* return the portion up to but not including the '/' */
                lib_strcpy(buf, buflen, str, r - str);
            }
        }
    }
    else
    {
        /* local storage mode - use local OS rules */
        return os_get_path_name(buf, buflen, str);
    }
}

/* is the file in absolute path format? */
static int fn_is_file_absolute(VMG_ const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* network storage mode - it's absolute if it starts with '/' */
        return str[0] == '/';
    }
    else
    {
        /* local storage mode - use local OS rules */
        return os_is_file_absolute(str);
    }
}

/* get the absolute path */
static int fn_get_abs_filename(VMG_ char *buf, size_t buflen, const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* 
         *   network storage mode - the storage server doesn't have such a
         *   thing as an absolute path; all paths are relative to the game's
         *   sandbox folder
         */
        return FALSE;
    }
    else
    {
        /* local storage mode - use local OS rules */
        return os_get_abs_filename(buf, buflen, str);
    }
}

/* check for a special filename */
static int fn_is_special_file(VMG_ const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* network storage mode treat "." and ".." as special */
        if (strcmp(str, ".") == 0)
            return OS_SPECFILE_SELF;
        else if (strcmp(str, "..") == 0)
            return OS_SPECFILE_PARENT;
        else
            return OS_SPECFILE_NONE;
    }
    else
    {
        /* local storage mode - use local OS rules */
        return os_is_special_file(str);
    }
}

/* get the special directory path for the working directory */
static const char *fn_pwd_dir(VMG0_)
{
    if (CVmNetFile::is_net_mode(vmg0_))
        return ".";
    else
        return OSPATHPWD;
}

/* convert from universal notation to local notation */
static void fn_cvt_url_dir(VMG_ char *buf, size_t buflen, const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* 
         *   network storage mode - the universal and local notation are the
         *   same, since the storage server uses unix-style syntax
         */
        lib_strcpy(buf, buflen, str);
    }
    else
    {
        /* local storage mode - use the local OS conversion */
        os_cvt_url_dir(buf, buflen, str);
    }
}

/* convert from local notation to universal notation */
static void fn_cvt_dir_url(VMG_ char *buf, size_t buflen, const char *str)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* network storage mode - local and universal notation are the same */
        lib_strcpy(buf, buflen, str);
    }
    else
    {
        /* local storage mode - use the local OS conversion */
        os_cvt_dir_url(buf, buflen, str);
    }
}

/* compare filenames */
static int fn_file_names_equal(VMG_ const char *a, const char *b)
{
    if (CVmNetFile::is_net_mode(vmg0_))
    {
        /* 
         *   network storage mode - compare using Unix rules; this is a
         *   simple case-sensitive string comparison 
         */
        return strcmp(a, b) == 0;
    }
    else
    {
        /* local storage mode - use the local OS routine */
        return os_file_names_equal(a, b);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjFileName intrinsic class implementation 
 */

/*
 *   construction
 */
CVmObjFileName::CVmObjFileName(VMG_ int32_t sfid, const char *str, size_t len)
{
    /* allocate our extension structure */
    ext_ = (char *)vm_filnam_ext::alloc_ext(vmg_ this, sfid, str, len);
}

/*
 *   Get the local path string for a given value.  If the value is a string,
 *   we'll treat it as a local path name.  If it's a FileName, we'll return
 *   its local file name.  In either case, returns a pointer to the string in
 *   our usual VMB_LEN prefix format.
 */
const char *CVmObjFileName::get_local_path(VMG_ const vm_val_t *val)
{
    CVmObjFileName *fn;
    const char *str;
    if ((fn = vm_val_cast(CVmObjFileName, val)) != 0)
    {
        /* it's a FileName object - return a copy of its filename string */
        return fn->get_ext()->str;
    }
    else if ((str = val->get_as_string(vmg0_)) != 0)
    {
        /* it's a string, so convert from URL notation to local path syntax */
        return str;
    }
    else
    {
        /* it's not a string or FileName */
        return 0;
    }
}

/*
 *   Combine two local path elements to create a full local path.  Returns a
 *   new object representing the combined path.
 */
vm_obj_id_t CVmObjFileName::combine_path(
    VMG_ const char *path, size_t pathl, const char *file, size_t filel,
    int literal)
{
    /* 
     *   Estimate the space we'll need for the combined result.  This is just
     *   the sum of the two sizes on most systems, plus a path separator.  On
     *   some older systems this might add a bit more overhead, such as VMS
     *   bracket notation for the directory.  Pad it out a bit to be safe.
     */
    size_t buflen = pathl + filel + 32;
    if (buflen < OSFNMAX)
        buflen = OSFNMAX;

    char *buf = 0, *pathz = 0, *filez = 0;
    vm_obj_id_t id = VM_INVALID_OBJ;
    err_try
    {
        /* allocate null-terminated copies of the source strings */
        pathz = lib_copy_str(path, pathl);
        filez = lib_copy_str(file, filel);

        /* allocate the result buffer */
        buf = lib_alloc_str(buflen);

        /* build the full path */
        fn_build_full_path(vmg_ buf, buflen, pathz, filez, literal);

        /* create the FileName for the combined path */
        id = create_from_local(vmg_ buf, strlen(buf));
    }
    err_finally
    {
        /* delete the allocated strings */
        lib_free_str(buf);
        lib_free_str(pathz);
        lib_free_str(filez);
    }
    err_end;

    /* return the new object */
    return id;
}

/*
 *   Convert a string from URL to local filename notation.  Allocates the
 *   result string; the caller must free the string with lib_free_str(). 
 */
char *CVmObjFileName::url_to_local(
    VMG_ const char *str, size_t len, int nullterm)
{
    char *strz = 0, *buf = 0;
    err_try
    {
        /* 
         *   Estimate the length of the return string we'll need.  Most
         *   systems these days have Unix-like file naming, where the result
         *   path will be the same length as the source path.  There are
         *   older systems with more complex conventions; for example, VMS
         *   puts directories in brackets.  To allow for this kind of
         *   variation, allocate space for the combined string plus some
         *   overhead.  In any case, use the local maximum filename length as
         *   the lower limit.
         */
        size_t buflen = len + 32;
        if (buflen < OSFNMAX)
            buflen = OSFNMAX;
        
        /* make a null-terminated copy of the string if necessary */
        if (!nullterm)
            strz = lib_copy_str(str, len);
        
        /* allocate a conversion buffer */
        buf = lib_alloc_str(buflen);
        
        /* convert the name */
        fn_cvt_url_dir(vmg_ buf, buflen, nullterm ? str : strz);
    }
    err_catch_disc
    {
        /* delete the buffer and re-throw the error */
        lib_free_str(buf);
        err_rethrow();
    }
    err_finally
    {
        /* we're done with copy of the source string (if we made one) */
        lib_free_str(strz);
    }
    err_end;
    
    /* return the buffer */
    return buf;
}

/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjFileName::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    /* parse arguments */
    const char *path, *file;
    vm_obj_id_t id = VM_INVALID_OBJ;
    if (argc == 0)
    {
        /* no arguments - create a file representing the working directory */
        const char *pwd = fn_pwd_dir(vmg0_);
        id = create_from_local(vmg_ pwd, strlen(pwd));
    }
    else if (argc == 1 && (file = G_stk->get(0)->get_as_string(vmg0_)) != 0)
    {
        /* it's a string in local filename notation */
        id = create_from_local(vmg_ file + VMB_LEN, vmb_get_len(file));
    }
    else if (argc == 1 && G_stk->get(0)->is_numeric(vmg0_))
    {
        /* it's a special file ID - resolve to a path */
        int32_t sfid = G_stk->get(0)->num_to_int(vmg0_);
        char buf[OSFNMAX];
        if (!CVmObjFile::sfid_to_path(vmg_ buf, sizeof(buf), sfid))
            err_throw(VMERR_BAD_VAL_BIF);

        /* create the file */
        id = create_from_sfid(vmg_ sfid, buf, strlen(buf));
    }
    else if (argc == 2
             && (path = get_local_path(vmg_ G_stk->get(0))) != 0
             && (file = get_local_path(vmg_ G_stk->get(1))) != 0)
    {
        /* create a FileName from the combination of the two path strings */
        id = combine_path(vmg_ path + VMB_LEN, vmb_get_len(path),
                          file + VMB_LEN, vmb_get_len(file), FALSE);
    }
    else if (argc >= 3)
    {
        /* invalid number of arguments */
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
    }
    else
    {
        /* wrong types */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new ID */
    return id;
}

/*
 *   create from URL notation 
 */
vm_obj_id_t CVmObjFileName::create_from_url(VMG_ const char *str, size_t len)
{
    char *lcl = 0;
    vm_obj_id_t fn = VM_INVALID_OBJ;
    err_try
    {
        /* convert to local notation */
        lcl = url_to_local(vmg_ str, len, FALSE);

        /* create the FileName object */
        fn = create_from_local(vmg_ lcl, strlen(lcl));
    }
    err_finally
    {
        /* we're done with the conversion buffer and path */
        lib_free_str(lcl);
    }
    err_end;

    /* return the filename object */
    return fn;
}

/*
 *   create from a local path string 
 */
vm_obj_id_t CVmObjFileName::create_from_local(VMG_ const char *str, size_t len)
{
    /* allocate the new ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* create the object */
    new (vmg_ id) CVmObjFileName(vmg_ 0, str, len);

    /* return the new ID */
    return id;
}

/*
 *   create from a special file ID 
 */
vm_obj_id_t CVmObjFileName::create_from_sfid(VMG_ int32_t sfid,
                                             const char *str, size_t len)
{
    /* allocate the new ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* create the object */
    new (vmg_ id) CVmObjFileName(vmg_ sfid, str, len);

    /* return the new ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/* 
 *   notify of deletion 
 */
void CVmObjFileName::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjFileName::set_prop(VMG_ class CVmUndo *undo,
                              vm_obj_id_t self, vm_prop_id_t prop,
                              const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjFileName::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                             vm_obj_id_t self, vm_obj_id_t *source_obj,
                             uint *argc)
{
    /* translate the property into a function vector index */
    uint func_idx = G_meta_table->prop_to_vector_idx(
        metaclass_reg_->get_reg_idx(), prop);
    
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
 *   call a static property 
 */
int CVmObjFileName::call_stat_prop(VMG_ vm_val_t *result,
                                   const uchar **pc_ptr, uint *argc,
                                   vm_prop_id_t prop)
{
    /* look up the function */
    uint midx = G_meta_table
                ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    switch (midx)
    {
    case VMOFN_FROMUNIVERSAL:
        return s_getp_fromUniversal(vmg_ result, argc);

    case VMOFN_GETROOTDIRS:
        return s_getp_getRootDirs(vmg_ result, argc);

    default:
        /* it's not ours - inherit the default handling */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
}


/* ------------------------------------------------------------------------ */
/* 
 *   load from an image file 
 */
void CVmObjFileName::load_from_image(VMG_ vm_obj_id_t self,
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
void CVmObjFileName::reload_from_image(VMG_ vm_obj_id_t self,
                                       const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjFileName::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* get the special file ID */
    int32_t sfid = osrp4s(ptr);

    /* if there's a special file ID, look it up; otherwise load the name */
    vm_filnam_ext *ext;
    if (sfid != 0)
    {
        /* translate the special file ID to a path */
        char sfbuf[OSFNMAX];
        int ok = CVmObjFile::sfid_to_path(vmg_ sfbuf, sizeof(sfbuf), sfid);

        /* allocate the extension */
        ext = vm_filnam_ext::alloc_ext(vmg_ this, sfid, sfbuf, strlen(sfbuf));

        /* note whether the special file ID is valid */
        ext->sfid_valid = (char)ok;
    }
    else
    {
        /* get the filename pointer and length */
        const char *str = ptr + 4;
        size_t len = vmb_get_len(str);
        str += VMB_LEN;

        char *lcl = 0;
        err_try
        {
            /* convert the universal path string to local notation */
            lcl = url_to_local(vmg_ str, len, FALSE);

            /* allocate the extension */
            ext = vm_filnam_ext::alloc_ext(vmg_ this, 0, lcl, strlen(lcl));
        }
        err_finally
        {
            lib_free_str(lcl);
        }
        err_end;
    }

    /* store the extension */
    ext_ = (char *)ext;
}


/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjFileName::save_to_file(VMG_ class CVmFile *fp)
{
    /* get our extension */
    vm_filnam_ext *ext = get_ext();

    /* write the special file ID */
    fp->write_int4(ext->sfid);

    /* if it's an ordinary file, write the path name */
    if (ext->sfid == 0)
    {
        char *u = 0;
        err_try
        {
            /* write the path in universal notation */
            u = to_universal(vmg0_);
            fp->write_str_short_prefix(u, strlen(u));
        }
        err_finally
        {
            lib_free_str(u);
        }
        err_end;
    }
}

/* 
 *   restore from a file 
 */
void CVmObjFileName::restore_from_file(VMG_ vm_obj_id_t self,
                                       CVmFile *fp, CVmObjFixup *fixups)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the special file ID */
    int32_t sfid = fp->read_int4();

    /* if there's a special file ID, look it up; otherwise read the name */
    vm_filnam_ext *ext;
    if (sfid != 0)
    {
        /* get the special file path */
        char sfbuf[OSFNMAX];
        int ok = CVmObjFile::sfid_to_path(vmg_ sfbuf, sizeof(sfbuf), sfid);

        /* create the extension */
        ext = vm_filnam_ext::alloc_ext(vmg_ this, sfid, sfbuf, strlen(sfbuf));

        /* note whether the ID is valid on this platform */
        ext->sfid_valid = (char)ok;
    }
    else
    {
        char *uni = 0, *lcl = 0;
        err_try
        {
            /* read the filename length */
            size_t len = fp->read_uint2();
            
            /* load the saved universal path */
            uni = lib_alloc_str(len + 1);
            fp->read_bytes(uni, len);
            uni[len] = '\0';
            
            /* convert the saved universal path to local notation */
            lcl = url_to_local(vmg_ uni, len, TRUE);
        
            /* allocate the extension structure */
            ext = vm_filnam_ext::alloc_ext(vmg_ this, 0, lcl, strlen(lcl));
        }
        err_finally
        {
            /* we're done with the allocated buffers */
            lib_free_str(lcl);
            lib_free_str(uni);
        }
        err_end;
    }

    /* remember the new extension */
    ext_ = (char *)ext;
}


/* ------------------------------------------------------------------------ */
/*
 *   Get my path string.  This validates the special file ID, and throws an
 *   error if it's not valid.  It's possible to create a FileName with an
 *   invalid special file ID by loading a saved game that was created on a
 *   platform with a newer version that includes new SFIDs that don't exist
 *   in this interpreter version.  In such a case we load the special file,
 *   and it can be saved again, but it's not usable locally and has no path
 *   information.
 */
const char *CVmObjFileName::get_path_string() const
{
    /* make sure this isn't an invalid special file */
    if (get_ext()->sfid && !get_ext()->sfid_valid)
        err_throw(VMERR_FILE_NOT_FOUND);

    /* return my path */
    return get_ext()->str;
}


/* ------------------------------------------------------------------------ */
/*
 *   Compare filenames by the path strings 
 */
int CVmObjFileName::equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                           int /*depth*/) const
{
    /* check the type of the other value */
    CVmObjFileName *fn;
    const char *str;
    if ((fn = vm_val_cast(CVmObjFileName, val)) != 0)
    {
        /* it's another FileName object - compare its path */
        return fn_file_names_equal(
            vmg_ get_ext()->get_str(), fn->get_ext()->get_str());
    }
    else if ((str = val->get_as_string(vmg0_)) != 0)
    {
        char *strz = 0;
        int eq;
        err_try
        {
            /* make a null-terminated copy of the other name */
            strz = lib_copy_str(str + VMB_LEN, vmb_get_len(str));
            
            /* compare the names */
            eq = fn_file_names_equal(vmg_ get_ext()->get_str(), strz);
        }
        err_finally
        {
            lib_free_str(strz);
        }
        err_end;

        /* return the result */
        return eq;
    }
    else
    {
        /* other types are definitely not equal */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a value - this appends a path element 
 */
int CVmObjFileName::add_val(VMG_ vm_val_t *result,
                            vm_obj_id_t self, const vm_val_t *val)
{
    /* 
     *   get the other value's path - this can come from a FileName object or
     *   a string 
     */
    const char *other = get_local_path(vmg_ val);
    if (other == 0)
        err_throw(VMERR_BAD_TYPE_ADD);

    /* build a new object from the combined paths */
    result->set_obj(combine_path(
        vmg_ get_ext()->get_str(), get_ext()->get_len(),
        other + VMB_LEN, vmb_get_len(other), FALSE));

    /* success */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   fromUniversal static method
 */
int CVmObjFileName::s_getp_fromUniversal(VMG_ vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename argument as a string */
    const char *str = G_stk->get(0)->get_as_string(vmg0_);
    if (str == 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* create and return the new FileName from the url syntax */
    retval->set_obj(create_from_url(vmg_ str + VMB_LEN, vmb_get_len(str)));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the filename string in univeral (URL) format.  This allocates a
 *   buffer, which the caller must free with lib_free_str() when done.
 */
char *CVmObjFileName::to_universal(VMG0_) const
{
    /* get our local filename string */
    const char *fname = get_ext()->get_str();
    size_t fnamel = get_ext()->get_len();

    /* 
     *   estimate the result size - the URL string should be about the same
     *   length as the source string, but leave a little extra space just in
     *   case we have to add some path separators that are implied in the
     *   local format
     */
    size_t buflen = fnamel + 32;
    if (buflen < OSFNMAX)
        buflen = OSFNMAX;

    /* allocate buffer */
    char *buf = lib_alloc_str(buflen);
    
    /* do the conversion */
    fn_cvt_dir_url(vmg_ buf, buflen, fname);

    /* return the new string */
    return buf;
}

/* ------------------------------------------------------------------------ */
/*
 *   toUniversal method
 */
int CVmObjFileName::getp_toUniversal(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    char *buf = 0;
    err_try
    {
        /* allocate the temporary result buffer */
        buf = to_universal(vmg0_);

        /* return a new String object from the result */
        retval->set_obj(CVmObjString::create(vmg_ FALSE, buf, strlen(buf)));
    }
    err_finally
    {
        /* free the temp buffer */
        lib_free_str(buf);
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getName method
 */
int CVmObjFileName::getp_getName(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get our extension */
    vm_filnam_ext *ext = get_ext();

    /* return the path string */
    retval->set_obj(CVmObjString::create(
        vmg_ FALSE, ext->get_str(), ext->get_len()));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getRootName method
 */
int CVmObjFileName::getp_getRootName(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* presume we won't find a root name */
    retval->set_nil();

    /* extract the root name from our filename string */
    const char *r = fn_get_root_name(vmg_ get_ext()->get_str());
    
    /* if we found a root name, return it as a string */
    if (r != 0 && r[0] != '\0')
        retval->set_obj(CVmObjString::create(vmg_ FALSE, r, strlen(r)));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getPath method
 */
int CVmObjFileName::getp_getPath(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* presume we'll return nil */
    retval->set_nil();

    /* get our extension */
    vm_filnam_ext *ext = get_ext();

    /* get our filename string */
    const char *fname = ext->get_str();
    size_t fnamel = ext->get_len();
        
    /*
     *   Estimate how much space we'll need for the extracted path name.  The
     *   path portion should be shorter than the whole name, but add some
     *   padding for overhead in case the local system needs to add any
     *   syntax (e.g., VMS directory brackets).
     */
    size_t buflen = fnamel + 32;
    if (buflen < OSFNMAX)
        buflen = OSFNMAX;
    
    char *buf = 0;
    err_try
    {
        /* allocate a temporary result buffer */
        buf = lib_alloc_str(buflen);
        
        /* extract the path name */
        fn_get_path_name(vmg_ buf, buflen, fname);
        
        /* return the path as a FileName, or nil if there's no path */
        if (buf[0] != '\0')
            retval->set_obj(create_from_local(vmg_ buf, strlen(buf)));
    }
    err_finally
    {
        /* free the temp buffer */
        lib_free_str(buf);
    }
    err_end;
    
    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   addToPath method
 */
int CVmObjFileName::getp_addToPath(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the target path - it can be a FileName or string */
    const char *file = get_local_path(vmg_ G_stk->get(0));
    if (file == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* create a new object from the combined path */
    retval->set_obj(combine_path(
        vmg_ get_ext()->get_str(), get_ext()->get_len(),
        file + VMB_LEN, vmb_get_len(file), FALSE));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   isAbsolute method
 */
int CVmObjFileName::getp_isAbsolute(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* ask the OS if we're absolute, and set the return value accordingly */
    retval->set_logical(fn_is_file_absolute(vmg_ get_ext()->get_str()));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getAbsolutePath method
 */
int CVmObjFileName::getp_getAbsolutePath(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get our local filename string */
    const char *fname = get_ext()->get_str();
    size_t fnamel = get_ext()->get_len();

    /* we haven't allocated any strings yet */
    char *buf = 0, *outbuf = 0;

    err_try
    {
        /* 
         *   if it's not absolute already, combine it with our internal
         *   working directory to get the fully qualified path
         */
        const char *src = fname;
        if (!fn_is_file_absolute(vmg_ fname))
        {
            /* allocate space for the combined path */
            size_t buflen = fnamel + strlen(G_file_path) + 32;
            if (buflen < OSFNMAX)
                buflen = OSFNMAX;

            /* build the combined path */
            buf = lib_alloc_str(buflen);
            fn_build_full_path(vmg_ buf, buflen, G_file_path, fname, TRUE);

            /* use this as the new source path */
            src = buf;
        }

        /* 
         *   Even though the name should already be absolute, run it through
         *   os_get_abs_filename() anyway, since that will convert the name
         *   to a more canonical format on many systems. 
         */

        /* figure how much space we'll need, adding some padding */
        size_t buflen = strlen(src) + 32;
        if (buflen < OSFNMAX)
            buflen = OSFNMAX;

        /* allocate the buffer and do the conversion */
        outbuf = lib_alloc_str(buflen);
        if (fn_get_abs_filename(vmg_ outbuf, buflen, src))
        {
            /* success - return the result string */
            retval->set_obj(CVmObjString::create(
                vmg_ FALSE, outbuf, strlen(outbuf)));
        }
        else
        {
            /* failure - return nil */
            retval->set_nil();
        }
    }
    err_finally
    {
        /* free the allocated string */
        lib_free_str(buf);
        lib_free_str(outbuf);
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getRootDirs method
 */
int CVmObjFileName::s_getp_getRootDirs(VMG_ vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* 
     *   See how much space we need for the root list.  If we're in network
     *   storage mode, there's no concept of a root folder, since everything
     *   is relative to the user+game folder.
     */
    size_t buflen;
    if (!CVmNetFile::is_net_mode(vmg0_)
        && (buflen = os_get_root_dirs(0, 0)) != 0)
    {
        /* allocate space */
        char *buf = new char[buflen];
        char *lclstr = 0;
        err_try
        {
            /* get the root list for real this time */
            os_get_root_dirs(buf, buflen);

            /* count the strings in the list, filtered for file safety */
            int cnt = 0;
            char *p;
            for (p = buf ; *p != '\0' ; p += strlen(p) + 1)
            {
                /* if we're allowed to get metadata on this path, count it */
                if (CVmObjFile::query_safety_for_open(
                    vmg_ p, VMOBJFILE_ACCESS_GETINFO))
                    ++cnt;
            }

            /* create the list; clear it and push it for gc protection */
            retval->set_obj(CVmObjList::create(vmg_ FALSE, cnt));
            CVmObjList *lst = vm_objid_cast(CVmObjList, retval->val.obj);
            lst->cons_clear();
            G_stk->push(retval);

            /* build the list */
            for (p = buf, cnt = 0 ; *p != '\0' ; p += strlen(p) + 1)
            {
                /* check to see if we're allowed to access this root */
                if (CVmObjFile::query_safety_for_open(
                    vmg_ p, VMOBJFILE_ACCESS_GETINFO))
                {
                    /* map the string to utf8 */
                    size_t len = G_cmap_from_fname->map_str_alo(&lclstr, p);

                    /* create the FileName object */
                    vm_val_t ele;
                    ele.set_obj(create_from_local(vmg_ lclstr, len));

                    /* done with the mapped string */
                    t3free(lclstr);
                    lclstr = 0;
                    
                    /* add it to the list */
                    lst->cons_set_element(cnt++, &ele);
                }
            }

            /* done with our gc protection for the list */
            G_stk->discard();
        }
        err_finally
        {
            /* free the root list buffer */
            delete [] buf;

            /* free the mapped filename string */
            if (lclstr != 0)
                t3free(lclstr);
        }
        err_end;
    }
    else
    {
        /* no root list - return an empty list */
        retval->set_obj(CVmObjList::create(vmg_ FALSE, (size_t)0));
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   deleteFile method
 */
int CVmObjFileName::getp_deleteFile(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use the DELETE access mode */
    vm_rcdesc rc(vmg_ "FileName.deleteFile",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_DELETEFILE, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_DELETE,
        OSFTUNK, "application/octet-stream");

    /* close the network file descriptor - this will delete the file */
    netfile->close(vmg0_);

    /* no return value */
    retval->set_nil();

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   renameFile method
 */
int CVmObjFileName::getp_renameFile(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* set up our recursive callback descriptor in case netfile needs it */
    vm_rcdesc rc(vmg_ "FileName.renameFile",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_RENAMEFILE, G_stk->get(0), argc);

    CVmNetFile *oldfile = 0, *newfile = 0;
    err_try
    {
        /* get the old filename from self; use the RENAME FROM access mode */
        oldfile = CVmObjFile::get_filename_from_obj(
            vmg_ self, &rc, VMOBJFILE_ACCESS_RENAME_FROM,
            OSFTUNK, "application/octet-stream");
        
        /* get the new filename argument; use RENAME TO access mode */
        newfile = CVmObjFile::get_filename_arg(
            vmg_ G_stk->get(0), &rc, VMOBJFILE_ACCESS_RENAME_TO, FALSE,
            OSFTUNK, "application/octet-stream");

        /* rename the file */
        oldfile->rename_to(vmg_ newfile);
    }
    err_finally
    {
        /* done with the netfile objects */
        if (oldfile != 0)
            oldfile->abandon(vmg0_);
        if (newfile != 0)
            newfile->abandon(vmg0_);
    }
    err_end;

    /* no return value */
    retval->set_nil();

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Translate an os_filemode_t value to a FileTypeXxx value 
 */
static int32_t filemode_to_filetype(int osmode)
{
    /* 
     *   mappings from OSFMODE_xxx values to FileTypeXxx values (see
     *   include/filename.h in the system headers) 
     */
    static const struct {
        unsigned long osmode;
        int32_t tadsmode;
    } xlat[] = {
        { OSFMODE_FILE,   0x0001 },   // FileTypeFile
        { OSFMODE_DIR,    0x0002 },   // FileTypeDir
        { OSFMODE_CHAR,   0x0004 },   // FileTypeChar
        { OSFMODE_BLK,    0x0008 },   // FileTypeBlock
        { OSFMODE_PIPE,   0x0010 },   // FileTypePipe
        { OSFMODE_SOCKET, 0x0020 },   // FileTypeSocket
        { OSFMODE_LINK,   0x0040 }    // FileTypeLink
    };

    /* run through the bit flags, and set the translated bits */
    int32_t tadsmode = 0;
    for (int i = 0 ; i < countof(xlat) ; ++i)
    {
        /* if this OS mode bit is set, set the corresponding TADS bit */
        if ((osmode & xlat[i].osmode) != 0)
            tadsmode |= xlat[i].tadsmode;
    }

    /* return the result */
    return tadsmode;
}

/*
 *   Translate OSFATTR_xxx values to FileAttrXxx values
 */
static int32_t map_file_attrs(unsigned long osattrs)
{
    /* 
     *   mappings from OSFATTR_xxx values to FileAttrXxx values (see
     *   include/filename.h in the system headers) 
     */
    static const struct {
        unsigned long osattr;
        int32_t tadsattr;
    } xlat[] = {
        { OSFATTR_HIDDEN,   0x0001 },   // FileTypeHidden
        { OSFATTR_SYSTEM,   0x0002 },   // FileTypeSystem
        { OSFATTR_READ,     0x0004 },   // FileTypeRead
        { OSFATTR_WRITE,    0x0008 }    // FileTypeWrite
    };

    /* run through the bit flags, and set the translated bits */
    int32_t tadsattrs = 0;
    for (int i = 0 ; i < countof(xlat) ; ++i)
    {
        /* if this OS mode bit is set, set the corresponding TADS bit */
        if ((osattrs & xlat[i].osattr) != 0)
            tadsattrs |= xlat[i].tadsattr;
    }

    /* return the result */
    return tadsattrs;
}

/*
 *   Push a file timestamp value 
 */
static void push_file_time(VMG_ os_time_t t)
{
    /* 
     *   if the time value is zero, it means that the local system doesn't
     *   support this type of timestamp; push it as nil 
     */
    if (t == 0)
    {
        /* no time value - push nil */
        G_stk->push()->set_nil();
    }
    else
    {
        /* we have a valid time, so create a Date object to represent it */
        G_stk->push()->set_obj(CVmObjDate::create_from_time_t(vmg_ FALSE, t));
    }
}

/*
 *   Get my special file type flags, if applicable.  This inspects the root
 *   name to see if it's a special relative link, such as Unix "." or "..".
 */
int32_t CVmObjFileName::special_filetype_flags(VMG0_)
{
    /* no special flags so far */
    int32_t flags = 0;

    /* get my extension */
    vm_filnam_ext *ext = get_ext();

    /* get the root name from my filename string */
    const char *root = fn_get_root_name(vmg_ (char *)ext->get_str());

    /* check the root name to see if it's special */
    switch (fn_is_special_file(vmg_ root))
    {
    case OS_SPECFILE_SELF:
        flags |= 0x0080; // FileTypeSelfLink
        break;

    case OS_SPECFILE_PARENT:
        flags |= 0x0100; // FileTypeParentLink
        break;

    default:
        break;
    }

    /* return the flags */
    return flags;
}

/* ------------------------------------------------------------------------ */
/*
 *   getFileType method
 */
int CVmObjFileName::getp_getFileType(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use the GETINFO access mode */
    vm_rcdesc rc(vmg_ "FileName.getFileType",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_GETFILETYPE, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_GETINFO,
        OSFTUNK, "application/octet-stream");

    /* check to see if we're following links (follow them by default) */
    int follow_links = TRUE;
    if (argc >= 1)
    {
        /* 
         *   note that our 'asLink' parameter has the opposite sense from
         *   'follow_links', so invert the parameter value 
         */
        follow_links = !G_stk->get(0)->get_logical();
        if (!G_stk->get(0)->is_logical())
            err_throw(VMERR_BAD_VAL_BIF);
    }

    int ok;
    unsigned long osmode, osattr;
    err_try
    {
        /* get the file type from the netfile object */
        ok = netfile->get_file_mode(vmg_ &osmode, &osattr, follow_links);
    }
    err_finally
    {
        /* done with the netfile object */
        netfile->abandon(vmg0_);
    }
    err_end;

    /* figure the return value */
    if (ok)
    {
        /* success - return the file mode, translated to FileTypeXxx bits */
        retval->set_int(
            filemode_to_filetype(osmode) | special_filetype_flags(vmg0_));
    }
    else
    {
        /* failed - simply return nil */
        retval->set_nil();
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getFileInfo method
 */
int CVmObjFileName::getp_getFileInfo(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use the GETINFO access mode */
    vm_rcdesc rc(vmg_ "FileName.getFileInfo",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_GETFILETYPE, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_GETINFO,
        OSFTUNK, "application/octet-stream");

    /* check to see if we're following links (follow them by default) */
    int follow_links = TRUE;
    if (argc >= 1)
    {
        /* 
         *   note that our 'asLink' parameter has the opposite sense from
         *   'follow_links', so invert the parameter value 
         */
        follow_links = G_stk->get(0)->get_logical();
        if (!G_stk->get(0)->is_logical())
            err_throw(VMERR_BAD_VAL_BIF);
    }

    int ok;
    os_file_stat_t stat;
    err_try
    {
        /* get the file type from the netfile object */
        ok = netfile->get_file_stat(vmg_ &stat, follow_links);
    }
    err_finally
    {
        /* done with the netfile object */
        netfile->abandon(vmg0_);
    }
    err_end;

    /* build the return value */
    if (ok)
    {
        /* 
         *   Success - build a new FileInfo(mode, size, ctime, mtime, atime,
         *   target, attrs).  Start by pushing the constructor arguments
         *   (last to first).
         */

        /* push the file attributes */
        G_stk->push()->set_int(map_file_attrs(stat.attrs));

        /* if this is a symbolic link file, get the target path */
        char target[OSFNMAX];
        if ((stat.mode & OSFMODE_LINK) != 0
            && netfile->resolve_symlink(vmg_ target, sizeof(target)))
        {
            /* push the name as a string in UTF8 */
            G_stk->push()->set_obj(CVmObjString::create(
                vmg_ FALSE, target, strlen(target), G_cmap_from_fname));
        }
        else
        {
            /* no symbolic link name - push nil as the symlink argument */
            G_stk->push()->set_nil();
        }

        /* push the timestamps */
        push_file_time(vmg_ stat.acc_time);
        push_file_time(vmg_ stat.mod_time);
        push_file_time(vmg_ stat.cre_time);

        /* 
         *   Push the file size.  If the high part is non-zero, or the low
         *   part is greater than 0x7fffffff, the value won't fit in an
         *   int32_t, so we need to push this as a BigNumber value. 
         */
        if (stat.sizehi != 0 || stat.sizelo > 0x7FFFFFFFU)
        {
            /* push it as a BigNumber value */
            G_interpreter->push_obj(vmg_ CVmObjBigNum::create_int64(
                vmg_ FALSE, stat.sizehi, stat.sizelo));
        }
        else
        {
            /* it'll fit in a simple integer value */
            G_interpreter->push_int(vmg_ (int32_t)stat.sizelo);
        }

        /* push the file mode, translated to FileTypeXxx bits */
        G_interpreter->push_int(
            vmg_ filemode_to_filetype(stat.mode)
            | special_filetype_flags(vmg0_));

        /* 
         *   Call the constructor.  If there's a FileInfo object exported,
         *   create one of those.  Otherwise just create a list from the
         *   values. 
         */
        const int argc = 7;
        vm_obj_id_t fi = G_predef->file_info;
        if (fi != VM_INVALID_OBJ)
        {
            /* create the FileInfo instance */
            vm_objp(vmg_ fi)->create_instance(vmg_ fi, 0, argc);

            /* make sure we got an object back */
            if (G_interpreter->get_r0()->typ != VM_OBJ)
                err_throw(VMERR_OBJ_VAL_REQD);

            /* set the object return value */
            retval->set_obj(G_interpreter->get_r0()->val.obj);
        }
        else
        {
            /* return the information as a list */
            retval->set_obj(CVmObjList::create_from_stack(vmg_ 0, argc));
        }
    }
    else
    {
        /* failed - simply return nil */
        retval->set_nil();
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   listDir method
 */
int CVmObjFileName::getp_listDir(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use the READ DIR access mode */
    vm_rcdesc rc(vmg_ "FileName.listDir",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_LISTDIR, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_READDIR,
        OSFTUNK, "application/octet-stream");
    
    err_try
    {
        /* get the directory listing */
        if (!netfile->readdir(vmg_ get_ext()->get_str(), retval))
            G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                           0, "error reading directory");
    }
    err_finally
    {
        /* done with the netfile object */
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
 *   forEachFile method
 */
int CVmObjFileName::getp_forEachFile(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use the READ DIR access mode */
    vm_rcdesc rc(vmg_ "FileName.forEachFile",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_FOREACHFILE, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_READDIR,
        OSFTUNK, "application/octet-stream");

    /* get the 'recursive' flag, if present */
    int recursive = FALSE;
    if (argc >= 2)
    {
        if (G_stk->get(1)->is_logical())
            recursive = G_stk->get(1)->get_logical();
        else
            err_throw(VMERR_BAD_TYPE_BIF);
    }

    err_try
    {
        /* do the directory enumeration through the callback */
        if (!netfile->readdir_cb(vmg_ get_ext()->get_str(),
                                 &rc, G_stk->get(0), recursive))
            G_interpreter->throw_new_class(vmg_ G_predef->file_open_exc,
                                           0, "error reading directory");
    }
    err_finally
    {
        /* done with the netfile object */
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
 *   createDirectory method
 */
int CVmObjFileName::getp_createDirectory(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use MKDIR access mode */
    vm_rcdesc rc(vmg_ "FileName.createDirectory",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_CREATEDIR, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_MKDIR,
        OSFTUNK, "application/octet-stream");

    err_try
    {
        /* get the "create parents" parameter */
        int create_parents = FALSE;
        if (argc >= 1)
        {
            /* check that it's a true/nil value */
            if (!G_stk->get(0)->is_logical())
                err_throw(VMERR_BAD_TYPE_BIF);

            /* retrieve the flag */
            create_parents = G_stk->get(0)->get_logical();
        }

        /* create the directory */
        netfile->mkdir(vmg_ create_parents);
    }
    err_finally
    {
        /* done with the netfile object */
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
 *   removeDirectory method
 */
int CVmObjFileName::getp_removeDirectory(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the filename; use RMDIR access mode */
    vm_rcdesc rc(vmg_ "FileName.removeDirectory",
                 metaclass_reg_->get_class_obj(vmg0_),
                 VMOFN_REMOVEDIR, G_stk->get(0), argc);
    CVmNetFile *netfile = CVmObjFile::get_filename_from_obj(
        vmg_ self, &rc, VMOBJFILE_ACCESS_RMDIR,
        OSFTUNK, "application/octet-stream");

    err_try
    {
        /* get the "remove contents" parameter */
        int remove_contents = FALSE;
        if (argc >= 1)
        {
            /* check that it's a true/nil value */
            if (!G_stk->get(0)->is_logical())
                err_throw(VMERR_BAD_TYPE_BIF);

            /* retrieve the flag */
            remove_contents = G_stk->get(0)->get_logical();
        }

        /* remove the directory */
        netfile->rmdir(vmg_ remove_contents);
    }
    err_finally
    {
        /* done with the netfile object */
        netfile->abandon(vmg0_);
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}


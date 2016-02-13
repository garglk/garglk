#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) <year> Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhttpdum.cpp - dummy HTTPServer and HTTPRequest classes
Function
  This is a dummy implementation HTTPServer and HTTPRequest, for linking
  into the compiler (and any other executables where the classes are needed
  for linking, but aren't instantiable at run-time).  This version simply
  throws an error if the byte-code program attempts to create one of these
  classes.
Notes
  
Modified
  08/07/10 MJRoberts  - creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmhttpsrv.h"
#include "vmhttpreq.h"
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
#include "vmnetfil.h"


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPServer
 */

/* metaclass registration object */
static CVmMetaclassHTTPServer metaclass_reg_obj_srv;
CVmMetaclass *CVmObjHTTPServer::metaclass_reg_ = &metaclass_reg_obj_srv;

vm_obj_id_t CVmObjHTTPServer::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    err_throw(VMERR_UNKNOWN_METACLASS);
    AFTER_ERR_THROW(return VM_INVALID_OBJ;)
}

void CVmObjHTTPServer::notify_delete(VMG_ int /*in_root_set*/)
{
}

void CVmObjHTTPServer::set_prop(VMG_ class CVmUndo *undo,
                                vm_obj_id_t self, vm_prop_id_t prop,
                                const vm_val_t *val)
{
}

int CVmObjHTTPServer::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                               vm_obj_id_t self, vm_obj_id_t *source_obj,
                               uint *argc)
{
    return FALSE;
}

void CVmObjHTTPServer::load_from_image(VMG_ vm_obj_id_t self,
                                       const char *ptr, size_t siz)
{
}

void CVmObjHTTPServer::reload_from_image(VMG_ vm_obj_id_t self,
                                         const char *ptr, size_t siz)
{
}

void CVmObjHTTPServer::save_to_file(VMG_ class CVmFile *fp)
{
}

void CVmObjHTTPServer::restore_from_file(VMG_ vm_obj_id_t self,
                                         CVmFile *fp, CVmObjFixup *fixups)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPRequest
 */

/* metaclass registration object */
static CVmMetaclassHTTPRequest metaclass_reg_obj_req;
CVmMetaclass *CVmObjHTTPRequest::metaclass_reg_ = &metaclass_reg_obj_req;

vm_obj_id_t CVmObjHTTPRequest::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    err_throw(VMERR_UNKNOWN_METACLASS);
    AFTER_ERR_THROW(return VM_INVALID_OBJ;)
}

void CVmObjHTTPRequest::notify_delete(VMG_ int /*in_root_set*/)
{
}

void CVmObjHTTPRequest::mark_refs(VMG_ uint state)
{
}

void CVmObjHTTPRequest::set_prop(VMG_ class CVmUndo *undo,
                                 vm_obj_id_t self, vm_prop_id_t prop,
                                 const vm_val_t *val)
{
}

int CVmObjHTTPRequest::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                                vm_obj_id_t self, vm_obj_id_t *source_obj,
                                uint *argc)
{
    return FALSE;
}

void CVmObjHTTPRequest::load_from_image(VMG_ vm_obj_id_t self,
                                        const char *ptr, size_t siz)
{
}

void CVmObjHTTPRequest::reload_from_image(VMG_ vm_obj_id_t self,
                                          const char *ptr, size_t siz)
{
}

void CVmObjHTTPRequest::save_to_file(VMG_ class CVmFile *fp)
{
}

void CVmObjHTTPRequest::restore_from_file(VMG_ vm_obj_id_t self,
                                          CVmFile *fp, CVmObjFixup *fixups)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   HTTP client request object 
 */
int OS_HttpClient::request(int, const char *, unsigned short,
                           const char *, const char *,
                           const char *, size_t,
                           class OS_HttpPayload *, class CVmDataSource *,
                           char **, char **, const char *)
{
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   HTTP request payload 
 */
OS_HttpPayload::OS_HttpPayload() { }
OS_HttpPayload::~OS_HttpPayload() { }
void OS_HttpPayload::add(const char *, const char *) { }
void OS_HttpPayload::add(const char *, const char *,
                         const char *, CVmDataSource *) { }





/* ------------------------------------------------------------------------ */
/*
 *   Network configuration object 
 */
const char *TadsNetConfig::get(const char *) const
{
    return 0;
}

TadsNetConfig::~TadsNetConfig()
{
}

void vmnet_check_storagesrv_reply(VMG_ int, class CVmStream *)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   local Web UI askfile 
 */
int osnet_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
                  int prompt_type, int file_type)
{
    return OS_AFE_FAILURE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Network storage server file operations - force local mode
 */
CVmNetFile *CVmNetFile::open(VMG_ const char *fname, int sfid, int mode,
                             os_filetype_t typ, const char *mime_type)
{
    return new CVmNetFile(fname, sfid, 0, mode, typ, 0);
}

void CVmNetFile::close(VMG0_)
{
    delete this;
}

int CVmNetFile::is_net_mode(VMG0_)
{
    return FALSE;
}

int CVmNetFile::exists(VMG_ const char *fname, int sfid)
{
    return !osfacc(fname);
}

int CVmNetFile::can_write(VMG_ const char *fname, int sfid)
{
    /* note whether the file already exists */
    int existed = !osfacc(fname);

    /* try opening it (keeping existing contents) or creating it */
    osfildef *fp = osfoprwb(fname, OSFTBIN);
    if (fp != 0)
    {
        /* successfully opened it - close it */
        osfcls(fp);

        /* if it didn't already exist, delete it */
        if (!existed)
            osfdel(fname);

        /* success */
        return TRUE;
    }
    else
    {
        /* couldn't open the file */
        return FALSE;
    }
}

int CVmNetFile::get_file_mode(
    VMG_ unsigned long *mode, unsigned long *attr, int follow_links)
{
    return osfmode(lclfname, follow_links, mode, attr);
}

int CVmNetFile::get_file_stat(VMG_ os_file_stat_t *stat, int follow_links)
{
    return os_file_stat(lclfname, follow_links, stat);
}

int CVmNetFile::resolve_symlink(VMG_ char *target, size_t target_size)
{
    return os_resolve_symlink(lclfname, target, target_size);
}

void CVmNetFile::rename_to(VMG_ CVmNetFile *newname)
{
    rename_to_local(vmg_ newname);
}

void CVmNetFile::mkdir(VMG_ int create_parents)
{
    mkdir_local(vmg_ create_parents);
}

void CVmNetFile::rmdir(VMG_ int remove_contents)
{
    rmdir_local(vmg_ remove_contents);
}

int CVmNetFile::readdir(VMG_ const char *nominal_path, vm_val_t *ret)
{
    ret->set_nil();
    return FALSE;
}

int CVmNetFile::readdir_cb(VMG_ const char *nominal_path,
                           const struct vm_rcdesc *, const vm_val_t *, int)
{
    return FALSE;
}


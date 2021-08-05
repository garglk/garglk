#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMINIT.CPP,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vminit.cpp - initialize and terminate VM processing
Function
  Provides functions to create and destroy global objects for a VM
  session.
Notes
  
Modified
  04/06/99 MJRoberts  - Creation
*/

#include <string.h>

#include "t3std.h"
#include "charmap.h"
#include "vminit.h"
#include "vmerr.h"
#include "vmfile.h"
#include "vmimage.h"
#include "vmpool.h"
#include "vmobj.h"
#include "vmstack.h"
#include "vmundo.h"
#include "vmmeta.h"
#include "vmbif.h"
#include "vmrun.h"
#include "vmpredef.h"
#include "vmmcreg.h"
#include "vmbiftad.h"
#include "resload.h"
#include "vmhost.h"
#include "vmconsol.h"
#include "vmbignum.h"
#include "vmsrcf.h"
#include "vmparam.h"
#include "vmmain.h"
#include "vmtobj.h"
#include "osifcnet.h"
#include "vmhash.h"
#include "vmtz.h"



/* ------------------------------------------------------------------------ */
/*
 *   Perform base initialization.  This is an internal routine called only
 *   by higher-level initialization routines; we perform all of the
 *   generic, configuration-independent initialization.
 */
void vm_init_base(vm_globals **vmg, const vm_init_options *opts)
{
    /* 
     *   Allocate globals according to build-time configuration, then
     *   assign the global pointer to a local named vmg__.  This will
     *   ensure that the globals are accessible for all of the different
     *   build-time configurations.  
     */
    vm_globals *vmg__ = *vmg = vmglob_alloc();

    /* 
     *   in configurations where globals aren't passed as parameters, vmg__
     *   will never be referenced; explicitly reference it so that the
     *   compiler knows the assignment above is intentional even if it's not
     *   ever used in this particular build configuration
     */
    (void)vmg__;

    /* initialize the error stack */
    err_init(VM_ERR_STACK_BYTES);

    /* get the system resource loader from the host interface */
    CResLoader *map_loader = opts->hostifc->get_sys_res_loader();

    /* if an external message set hasn't been loaded, try loading one */
    if (!err_is_message_file_loaded() && map_loader != 0)
    {
        /* try finding a message file */
        osfildef *fp = map_loader->open_res_file(
            VM_ERR_MSG_FNAME, 0, VM_ERR_MSG_RESTYPE);
        if (fp != 0)
        {
            /* 
             *   try loading it - if that fails, we'll just be left with
             *   the built-in messages, so we won't have lost anything for
             *   trying 
             */
            err_load_vm_message_file(fp);
            
            /* we're done with the file */
            osfcls(fp);
        }
    }

    /* remember the host interface */
    G_host_ifc = opts->hostifc;

    /* initialize the system debug log file name */
    char path[OSFNMAX];
    opts->hostifc->get_special_file_path(path, sizeof(path), OS_GSP_LOGFILE);
    os_build_full_path(G_syslogfile, sizeof(G_syslogfile),
                       path, "tadslog.txt");

    /* create the object table */
    VM_IF_ALLOC_PRE_GLOBAL(G_obj_table = new CVmObjTable());
    G_obj_table->init(vmg0_);

    /* 
     *   Create the memory manager.  Empirically, our hybrid heap allocator
     *   is faster than the standard C++ run-time library's allocator on many
     *   platforms, so use it instead of hte basic 'malloc' allocator. 
     */
    G_varheap = new CVmVarHeapHybrid(G_obj_table);
    // G_varheap = new CVmVarHeapMalloc(); to use the system 'malloc' instead
    G_mem = new CVmMemory(vmg_ G_varheap);

    /* create the undo manager */
    G_undo = new CVmUndo(VM_UNDO_MAX_RECORDS, VM_UNDO_MAX_SAVEPTS);

    /* create the metafile and function set tables */
    G_meta_table = new CVmMetaTable(5);
    G_bif_table = new CVmBifTable(5);

    /* create the time zone cache */
    G_tzcache = new CVmTimeZoneCache();

    /* initialize the metaclass registration tables */
    vm_register_metaclasses();

    /* initialize the TadsObject class */
    CVmObjTads::class_init(vmg0_);

    /* create the byte-code interpreter */
    VM_IFELSE_ALLOC_PRE_GLOBAL(
        G_interpreter = new CVmRun(VM_STACK_SIZE, vm_init_stack_reserve()),
        G_interpreter->init());

    /* presume we won't create debugger information */
    G_debugger = 0;
    G_srcf_table = 0;

    /* presume we don't have a network configuration */
    G_net_config = 0;

    /* initialize the debugger if present */
    vm_init_debugger(vmg0_);

    /* create the source file table */
    G_srcf_table = new CVmSrcfTable();

    /* create the pre-defined object mapper */
    VM_IFELSE_ALLOC_PRE_GLOBAL(G_predef = new CVmPredef, G_predef->reset());

    /* presume we're in normal execution mode (not preinit) */
    G_preinit_mode = FALSE;

    /* allocate the TADS intrinsic function set's globals */
    G_bif_tads_globals = new CVmBifTADSGlobals(vmg0_);

    /* allocate the BigNumber register cache */
    CVmObjBigNum::init_cache();

    /* no image loader yet */
    G_image_loader = 0;

    /*
     *   If the caller explicitly specified a character set, use it.
     *   Otherwise, ask the OS layer for the default character set we
     *   should use. 
     */
    char disp_mapname[32];
    const char *charset = opts->charset;
    if (charset == 0)
    {
        /* the user did not specify a mapping - ask the OS for the default */
        os_get_charmap(disp_mapname, OS_CHARMAP_DISPLAY);

        /* use the name we got from the OS */
        charset = disp_mapname;

        /* there's no explicit global character set name setting to store */
        G_disp_cset_name = 0;
    }
    else
    {
        /* save the global character set name */
        G_disp_cset_name = lib_copy_str(charset);
    }

    /* create the display character maps */
    G_cmap_from_ui = CCharmapToUni::load(map_loader, charset);
    G_cmap_to_ui = CCharmapToLocal::load(map_loader, charset);

    /* create the filename character maps */
    char filename_mapname[32];
    os_get_charmap(filename_mapname, OS_CHARMAP_FILENAME);
    G_cmap_from_fname = CCharmapToUni::load(map_loader, filename_mapname);
    G_cmap_to_fname = CCharmapToLocal::load(map_loader, filename_mapname);

    /* create the file-contents character maps */
    char filecont_mapname[32];
    os_get_charmap(filecont_mapname, OS_CHARMAP_FILECONTENTS);
    G_cmap_from_file = CCharmapToUni::load(map_loader, filecont_mapname);
    G_cmap_to_file = CCharmapToLocal::load(map_loader, filecont_mapname);

    /* 
     *   If the caller specified a separate log-file character set, create
     *   the mapping.  Otherwise, just use the to-file mapper for log files.
     */
    if (opts->log_charset != 0)
    {
        /* load the specified log file output mapping */
        G_cmap_to_log = CCharmapToLocal::load(map_loader, opts->log_charset);
    }
    else
    {
        /* no log file mapping is specified, so use the generic file map */
        if ((G_cmap_to_log = G_cmap_to_file) != 0)
            G_cmap_to_log->add_ref();
    }

    /* make a note of whether we had any problems loading the maps */
    int disp_map_err = (G_cmap_from_ui == 0 || G_cmap_to_ui == 0);

    /* if we failed to create any of the maps, load defaults */
    if (G_cmap_from_ui == 0)
        G_cmap_from_ui = CCharmapToUni::load(map_loader, "us-ascii");
    if (G_cmap_to_ui == 0)
        G_cmap_to_ui = CCharmapToLocal::load(map_loader, "us-ascii");
    if (G_cmap_from_fname == 0)
        G_cmap_from_fname = CCharmapToUni::load(map_loader, "us-ascii");
    if (G_cmap_to_fname == 0)
        G_cmap_to_fname = CCharmapToLocal::load(map_loader, "us-ascii");
    if (G_cmap_from_file == 0)
        G_cmap_from_file = CCharmapToUni::load(map_loader, "us-ascii");
    if (G_cmap_to_file == 0)
        G_cmap_to_file = CCharmapToLocal::load(map_loader, "us-ascii");
    if (G_cmap_to_log == 0)
        G_cmap_to_log = CCharmapToLocal::load(map_loader, "us-ascii");

    /* create the primary console */
    G_console = opts->clientifc->create_console(VMGLOB_ADDR);

    /* 
     *   if we had any trouble opening the display character mapping file,
     *   make a note that we are using a default mapping 
     */
    if (disp_map_err)
    {
        const char *msg;
        char buf[256];

        /* get the message */
        msg = err_get_msg(vm_messages, vm_message_count,
                          VMERR_NO_CHARMAP_FILE, TRUE);

        /* format it */
        sprintf(buf, msg, charset);

        /* display it */
        opts->clientifc->display_error(VMGLOB_ADDR, 0, buf, TRUE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Terminate the VM 
 */
void vm_terminate(vm_globals *vmg__, CVmMainClientIfc *clientifc)
{
    /* tell the debugger to shut down, if necessary */
    vm_terminate_debug_shutdown(vmg0_);
    
    /* drop all undo information */
    G_undo->drop_undo(vmg0_);

    /* delete the main console */
    clientifc->delete_console(VMGLOB_ADDR, G_console);

    /* release references on the character mappers */
    G_cmap_from_fname->release_ref();
    G_cmap_to_fname->release_ref();
    G_cmap_from_ui->release_ref();
    G_cmap_to_ui->release_ref();
    G_cmap_from_file->release_ref();
    G_cmap_to_file->release_ref();
    G_cmap_to_log->release_ref();

    /* delete the saved UI character set name, if any */
    lib_free_str(G_disp_cset_name);

    /* delete the BigNumber register cache */
    CVmObjBigNum::term_cache();

    /* delete the TADS intrinsic function set's globals */
    delete G_bif_tads_globals;

    /* delete the predefined object table */
    VM_IF_ALLOC_PRE_GLOBAL(delete G_predef);

    /* delete the interpreter */
    VM_IFELSE_ALLOC_PRE_GLOBAL(delete G_interpreter,
                               G_interpreter->terminate());

    /* terminate the TadsObject class */
    CVmObjTads::class_term(vmg0_);

    /* delete the source file table */
    delete G_srcf_table;

    /* delete debugger objects */
    vm_terminate_debug_delete(vmg0_);

    /* delete dynamic compilation objects */
    if (G_dyncomp != 0)
    {
        delete G_dyncomp;
        G_dyncomp = 0;
    }

    /* delete the constant pools */
    VM_IFELSE_ALLOC_PRE_GLOBAL(delete G_code_pool,
                               G_code_pool->terminate());
    VM_IFELSE_ALLOC_PRE_GLOBAL(delete G_const_pool,
                               G_const_pool->terminate());

    /* 
     *   Clear the object table.  This deletes garbage-collected objects but
     *   doesn't delete the object table itself; we'll do that after we've
     *   cleared out the metaclass and function-set tables.  We need to
     *   remove the gc objects before the metaclasses and function tables
     *   because the gc objects sometimes depend on their metaclasses.  But
     *   the metaclasses and function sets sometimes keep VM globals, so we
     *   need to keep the object table around until after they're cleaned up.
     */
    G_obj_table->clear_obj_table(vmg0_);

    /* delete the dependency tables */
    G_bif_table->clear(vmg0_);
    delete G_meta_table;
    delete G_bif_table;

    /* delete the undo manager */
    delete G_undo;

    /* delete the object table */
    G_obj_table->delete_obj_table(vmg0_);
    VM_IF_ALLOC_PRE_GLOBAL(delete G_obj_table);

    /* delete the memory manager and heap manager */
    delete G_mem;
    delete G_varheap;

    /* delete the time zone cache */
    delete G_tzcache;

    /* delete the error context */
    err_terminate();

    /* delete the globals */
    vmglob_delete(vmg__);
}


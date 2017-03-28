#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmimage.cpp,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmimage.cpp - VM image file loader
Function
  
Notes
  
Modified
  12/12/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "os.h"
#include "t3std.h"
#include "vmtype.h"
#include "vminit.h"
#include "vmimage.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmfile.h"
#include "vmmeta.h"
#include "vmbif.h"
#include "vmpredef.h"
#include "vmrun.h"
#include "vmtobj.h"
#include "vmhost.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmsave.h"
#include "vmrunsym.h"
#include "vmlookup.h"
#include "vmstack.h"
#include "tcprstyp.h"
#include "vmhash.h"
#include "vmsrcf.h"
#include "vmvec.h"
#include "charmap.h"


/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for the exported symbols table
 */
class CVmHashEntryExport: public CVmHashEntryCS
{
public:
    CVmHashEntryExport(const char *nm, size_t len, int copy_nm,
                       const vm_val_t *val)
        : CVmHashEntryCS(nm, len, copy_nm)
    {
        /* remember the value */
        val_ = *val;
    }

    /* the value of this symbol */
    vm_val_t val_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Implementation of CVmImageLoaderMres interface for regular image file
 *   resource loading 
 */
class CVmImageLoaderMres_std: public CVmImageLoaderMres
{
public:
    CVmImageLoaderMres_std(int fileno, CVmHostIfc *hostifc)
    {
        fileno_ = fileno;
        hostifc_ = hostifc;
    }

    /* add a resource */
    void add_resource(uint32_t seek_ofs, uint32_t siz,
                      const char *res_name, size_t res_name_len)
    {
        /* call the host system interface to add the resource */
        hostifc_->add_resource(seek_ofs, siz, res_name, res_name_len,
                               fileno_);
    }

    /* add a local file resource link */
    void add_resource(const char *fname, size_t fnamelen,
                      const char *res_name, size_t res_name_len)
    {
        /* call the host system inetrface to add it */
        hostifc_->add_resource(fname, fnamelen, res_name, res_name_len);
    }

private:
    /* host interface object */
    CVmHostIfc *hostifc_;

    /* file number to pass to host interface */
    int fileno_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image file loader implementation 
 */

/*
 *   instantiation 
 */
CVmImageLoader::CVmImageLoader(CVmImageFile *fp, const char *fname,
                               long base_ofs)
{
    size_t i;
    char fname_abs[OSFNMAX], path[OSFNMAX];
    
    /* remember the underlying image file interface */
    fp_ = fp;

    /* remember the image filename and the base seek offset */
    fname_ = lib_copy_str(fname);
    base_seek_ofs_ = base_ofs;

    /* save the file's fully-qualified, absolute directory path */
    os_get_abs_filename(fname_abs, sizeof(fname_abs), fname);
    os_get_path_name(path, sizeof(path), fname_abs);
    path_ = lib_copy_str(path);

    /* we don't know the file version yet */
    ver_ = 0;

    /* allocate the pool tracking objects */
    for (i = 0 ; i < sizeof(pools_)/sizeof(pools_[0]) ; ++i)
        pools_[i] = new CVmImagePool();

    /* no metaclass dependency table loaded yet */
    loaded_meta_dep_ = FALSE;

    /* no function set dependency table loaded yet */
    loaded_funcset_dep_ = FALSE;

    /* no entrypoint loaded yet */
    loaded_entrypt_ = FALSE;

    /* no GSYM block yet */
    has_gsym_ = FALSE;

    /* no runtime symbols loaded yet */
    runtime_symtab_ = 0;
    runtime_macros_ = 0;

    /* there's no reflection LookupTable yet */
    reflection_symtab_ = VM_INVALID_OBJ;
    reflection_macros_ = VM_INVALID_OBJ;

    /* create the exported symbol hash table */
    exports_ = new CVmHashTable(64, new CVmHashFuncCS(), TRUE);

    /* create the synthesized exports hash table */
    synth_exports_ = new CVmHashTable(16, new CVmHashFuncCS(), TRUE);

    /* no static initializer pages yet */
    static_head_ = static_tail_ = 0;

    /* we don't have a static initializer code offset yet */
    static_cs_ofs_ = 0;
}

/*
 *   destruction 
 */
CVmImageLoader::~CVmImageLoader()
{
    size_t i;

    /* delete the pool tracking objects */
    for (i = 0 ; i < sizeof(pools_)/sizeof(pools_[0]) ; ++i)
        delete pools_[i];

    /* delete the filename string and path */
    lib_free_str(fname_);
    lib_free_str(path_);

    /* if we have our own runtime symbol/macro tables, delete them */
    if (runtime_symtab_ != 0)
        delete runtime_symtab_;
    if (runtime_macros_ != 0)
        delete runtime_macros_;

    /* delete the exports tables */
    delete exports_;
    delete synth_exports_;

    /* delete the static initializer pages */
    while (static_head_ != 0)
    {
        CVmStaticInitPage *nxt;
        
        /* remember the next one */
        nxt = static_head_->nxt_;

        /* free the image-allocated data associated with the page */
        fp_->free_mem(static_head_->data_);

        /* delete this one */
        delete static_head_;

        /* move on to the next */
        static_head_ = nxt;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   load the image 
 */
void CVmImageLoader::load(VMG0_)
{
    char buf[128];
    int done;

    /* set myself to be the global image loader */
    G_image_loader = this;

    /* 
     *   perform additional initialization now that we're about to load
     *   the image file 
     */
    vm_init_before_load(vmg_ fname_);

    /* tell the host application the name of the image file */
    G_host_ifc->set_image_name(fname_);

    /* read and validate the header */
    read_image_header();

    /* load the data blocks */
    for (done = FALSE ; !done ; )
    {
        ulong siz;
        uint flags;
        
        /* read the next data block header */
        fp_->copy_data(buf, 10);

        /* get the size */
        siz = t3rp4u(buf + 4);

        /* get the flags */
        flags = osrp2(buf + 8);

        /* load the block according to its type */
        if (block_type_is(buf, "CPPG"))
        {
            /* load the constant pool page block */
            load_const_pool_page(siz);
        }
        else if (block_type_is(buf, "ENTP"))
        {
            /* load the entrypoint */
            load_entrypt(vmg_ siz);
        }
        else if (block_type_is(buf, "OBJS"))
        {
            /* load static object block */
            load_static_objs(vmg_ siz);
        }
        else if (block_type_is(buf, "CPDF"))
        {
            /* load the constant pool definition block */
            load_const_pool_def(siz);
        }
        else if (block_type_is(buf, "MRES"))
        {
            /* 
             *   load the multimedia resource block (the main image file
             *   is always file number zero) 
             */
            CVmImageLoaderMres_std res_ifc(0, G_host_ifc);
            load_mres(siz, &res_ifc);
        }
        else if (block_type_is(buf, "MREL"))
        {
            /* load the multimedia resource link block */
            CVmImageLoaderMres_std res_ifc(0, G_host_ifc);
            load_mres_link(siz, &res_ifc);
        }
        else if (block_type_is(buf, "MCLD"))
        {
            /* load metaclass dependency block */
            load_meta_dep(vmg_ siz);
        }
        else if (block_type_is(buf, "FNSD"))
        {
            /* load function set dependency list block */
            load_funcset_dep(vmg_ siz);
        }
        else if (block_type_is(buf, "SYMD"))
        {
            /* load symbolic names export block */
            load_sym_names(vmg_ siz);
        }
        else if (block_type_is(buf, "SRCF"))
        {
            /* load the source file list */
            load_srcfiles(vmg_ siz);
        }
        else if (block_type_is(buf, "GSYM"))
        {
            /* load the global symbols block */
            load_gsym(vmg_ siz);
        }
        else if (block_type_is(buf, "MACR"))
        {
            /* load the macro symbols block */
            load_macros(vmg_ siz);
        }
        else if (block_type_is(buf, "MHLS"))
        {
            /* load the method header list block */
            load_mhls(vmg_ siz);
        }
        else if (block_type_is(buf, "SINI"))
        {
            /* load the static initializer block */
            load_sini(vmg_ siz);
        }
        else if (block_type_is(buf, "EOF "))
        {
            /* end of file - we can stop looking now */
            done = TRUE;
        }
        else
        {
            /*
             *   This block type is unknown, so ignore it.  If a new block
             *   type is added in the future, an older VM version won't
             *   recognize the new block, but it can still load the image
             *   file simply by omitting any unrecognized new blocks.  Since
             *   the image file will not be complete in this case, it may not
             *   work properly.
             *   
             *   To allow for future block types which contain advisory data,
             *   which can safely be ignored by older VM versions, while also
             *   allowing for the possibility of changes that create
             *   incompatibilities, we have a flag in the header that
             *   indicates whether the block is required or not.  If this
             *   block is marked as mandatory, throw an error, since we don't
             *   recognize the block.  Note that this is a version-related
             *   error (i.e., a VM update should fix it), so set the version
             *   flag in the exception.  
             */
            if ((flags & VMIMAGE_DBF_MANDATORY) != 0)
                err_throw_a(VMERR_UNKNOWN_IMAGE_BLOCK, 1,
                            ERR_TYPE_VERSION_FLAG);

            /* skip past the block */
            fp_->skip_ahead(siz);
        }
    }

    /* the image file is required to contain an entrypoint definition */
    if (!loaded_entrypt_)
        err_throw(VMERR_IMAGE_NO_ENTRYPT);

    /* the image file is required to contain a metaclass dependency table */
    if (!loaded_meta_dep_)
        err_throw(VMERR_IMAGE_NO_METADEP);

    /* the image is required to have a function set dependency table */
    if (!loaded_funcset_dep_)
        err_throw(VMERR_IMAGE_NO_FUNCDEP);

    /* complete the dynamic linking */
    do_dynamic_link(vmg0_);

    /* fix up the global symbol table metaclasses, if applicable */
    fix_gsym_meta(vmg0_);

    /* 
     *   create an IntrinsicClass instance for each metaclass that the
     *   image file is using and for which the compiler didn't supply its
     *   own IntrinsicClass object 
     */
    G_meta_table->create_intrinsic_class_instances(vmg0_);

    /* 
     *   Cache certain metaclass method table entries that are used by the
     *   VM: Iterator.getNext [Iterator#1], Iterator.isNextAvailable
     *   [Iterator#2].  
     */
    vm_meta_entry_t *mcoll = G_meta_table->get_entry_by_id("iterator");
    if (mcoll != 0)
    {
        G_iter_get_next = mcoll->xlat_func(1);
        G_iter_next_avail = mcoll->xlat_func(2);
    }

    /* 
     *   Attach the code pool and constant pool to their backing stores,
     *   which are the pool objects we loaded from the image file. 
     */
    G_code_pool->attach_backing_store(pools_[0]);
    G_const_pool->attach_backing_store(pools_[1]);

    /* perform any requested post-load object initializations */
    G_obj_table->do_all_post_load_init(vmg0_);

    /* load external resource files if possible */
    if (G_host_ifc->can_add_resfiles())
        load_ext_resfiles(vmg0_);

    /* 
     *   perform additional initialization now that we've finished
     *   loading the image file
     */
    vm_init_after_load(vmg0_);

    /* 
     *   Let the UI inspect the built-ins linked by the program.  The UI
     *   might use different initial display configurations depending on the
     *   function sets and/or intrinsic classes the program uses.  For
     *   example, Workbench on Windows initially hides the HTML TADS window
     *   if the network functions are linked, since these are normally used
     *   to implement a Web UI, which uses a separate display window.
     */
    os_init_ui_after_load(G_bif_table, G_meta_table);

    /* forget the image loader */
    G_image_loader = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Load external resource files associated with the image file 
 */
void CVmImageLoader::load_ext_resfiles(VMG0_)
{
    int i;
    char suffix_lc[4];
    char suffix_uc[4];

    /* set up the templates for the resource file suffix */
    strcpy(suffix_lc, "3r0");
    strcpy(suffix_uc, "3R0");
    
    /* 
     *   Search for resource files with the same name as the image file, but
     *   with the extension replaced with .3r0 through .3r9.  Try both
     *   lower-case and upper-case names (in that order), in case the file
     *   system is case-sensitive.  
     */
    for (i = 0 ; i < 10 ; ++i)
    {
        char resfile[OSFNMAX];
        
        /* substitute the current suffix number */
        suffix_lc[2] = suffix_uc[2] = i + '0';

        /* 
         *   if there's an explicit resource path, use it, otherwise use
         *   the same directory that contains the image file 
         */
        if (G_host_ifc->get_res_path() != 0)
        {
            /* 
             *   there's an explicit resource path - build the full path to
             *   the resource file using the resource path and the root name
             *   of the image file 
             */
            os_build_full_path(resfile, sizeof(resfile),
                               G_host_ifc->get_res_path(),
                               os_get_root_name(fname_));
        }
        else
        {
            /* 
             *   there's no resoruce path - use the image file full name,
             *   including any directory path information 
             */
            strcpy(resfile, fname_);
        }

        /* replace the old image file extension with the resource suffix */
        os_remext(resfile);
        os_addext(resfile, suffix_lc);

        /* if this file doesn't exist, try the upper-case name */
        if (osfacc(resfile))
        {
            /* replace the suffix with the upper-case version */
            os_remext(resfile);
            os_addext(resfile, suffix_uc);
        }

        /* check to see if this file exists */
        if (!osfacc(resfile))
        {
            int fileno;
            CVmFile *fp;
            CVmImageFile *volatile imagefp = 0;
            CVmImageLoader *volatile loader = 0;
            
            /* ask the host system to assign a file number */
            fileno = G_host_ifc->add_resfile(resfile);

            /* create a file object for reading the file */
            fp = new CVmFile();

            err_try
            {
                CVmImageLoaderMres_std res_ifc(fileno, G_host_ifc);

                /* open the file */
                fp->open_read(resfile, OSFTT3IMG);

                /* set up the loader */
                imagefp = new CVmImageFileExt(fp);
                loader = new CVmImageLoader(imagefp, resfile, 0);

                /* load the resource-only file */
                loader->load_resource_file(&res_ifc);
            }
            err_finally
            {
                /* delete the objects we created */
                if (loader != 0)
                    delete loader;
                if (imagefp != 0)
                    delete imagefp;
                delete fp;
            }
            err_end;
        }
    }
}

/*
 *   Load a resource file from the current seek location in the given file
 *   handle 
 */
void CVmImageLoader::load_resources_from_fp(osfildef *fp,
                                            const char *fname,
                                            CVmHostIfc *hostifc)
{
    int fileno;

    /* ask the host system to assign a number to the file */
    fileno = hostifc->add_resfile(fname);

    /* set up a resource interface with the file number */
    CVmImageLoaderMres_std res_ifc(fileno, hostifc);

    err_try
    {
        /* load the resources */
        load_resources_from_fp(fp, fname, &res_ifc);
    }
    err_catch_disc
    {
        /* ignore the error */
    }
    err_end;
}

/*
 *   Load a resource file from the current seek location in the given file
 *   handle 
 */
void CVmImageLoader::load_resources_from_fp(osfildef *fp,
                                            const char *fname,
                                            CVmImageLoaderMres *res_ifc)
{
    CVmFile *file;
    CVmImageFile *volatile imagefp = 0;
    CVmImageLoader *volatile loader = 0;

    /* create a file object for reading the file */
    file = new CVmFile();

    /* set up the file with our file handler */
    file->set_file(fp, 0);
    
    err_try
    {
        /* set up the loader */
        imagefp = new CVmImageFileExt(file);
        loader = new CVmImageLoader(imagefp, fname, 0);

        /* load the resource-only file */
        loader->load_resource_file(res_ifc);
    }
    err_finally
    {
        /* 
         *   detach our CVmFile object from the caller's file handle,
         *   since we want to leave the caller's file handle open 
         */
        file->detach_file();
        
        /* delete the objects we created */
        if (loader != 0)
            delete loader;
        if (imagefp != 0)
            delete imagefp;
        delete file;
    }
    err_end;
}


/* ------------------------------------------------------------------------ */
/*
 *   Load a resource-only file.  'fileno' is the file number assigned by
 *   the host application (via the add_resfile() interface).  
 */
void CVmImageLoader::load_resource_file(CVmImageLoaderMres *res_ifc)
{
    int done;

    /* read and validate the image header */
    read_image_header();

    /* load the blocks */
    for (done = FALSE ; !done ; )
    {
        ulong siz;
        uint flags;
        char buf[128];

        /* read the next data block header */
        fp_->copy_data(buf, 10);

        /* get the size */
        siz = t3rp4u(buf + 4);

        /* get the flags */
        flags = osrp2(buf + 8);

        /* check the block type */
        if (block_type_is(buf, "EOF "))
        {
            /* we're done */
            done = TRUE;
        }
        else if (block_type_is(buf, "MRES"))
        {
            /* load the multimedia resource block */
            load_mres(siz, res_ifc);
        }
        else if (block_type_is(buf, "MREL"))
        {
            /* load the multimedia resource link block */
            load_mres_link(siz, res_ifc);
        }
        else
        {
            /* 
             *   Unknown block type - ignore it, unless it's mandatory, in
             *   which case this is an error.  This is a version-related
             *   error (fixable by updating to the latest version), so set
             *   the version flag in the exception, if we throw one.  
             */
            if ((flags & VMIMAGE_DBF_MANDATORY) != 0)
                err_throw_a(VMERR_UNKNOWN_IMAGE_BLOCK, 1,
                            ERR_TYPE_VERSION_FLAG);

            /* skip past the block */
            fp_->skip_ahead(siz);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Read and validate an image file header.
 */
void CVmImageLoader::read_image_header()
{
    char buf[128];

    /* 
     *   Read the header.  The header consists of the signature string, a
     *   UINT2 with the file format version number, 32 reserved bytes,
     *   then the compilation timestamp (24 bytes).  
     */
    fp_->copy_data(buf, sizeof(VMIMAGE_SIG)-1 + 2 + 32 + 24);

    /* verify the signature */
    if (memcmp(buf, VMIMAGE_SIG, sizeof(VMIMAGE_SIG)-1) != 0)
        err_throw(VMERR_NOT_AN_IMAGE_FILE);

    /* get the version number */
    ver_ = osrp2(buf + sizeof(VMIMAGE_SIG)-1);

    /* store the timestamp */
    memcpy(timestamp_, buf + sizeof(VMIMAGE_SIG)-1 + 2 + 32, 24);

    /* 
     *   Check the version to ensure that it's within the range that this
     *   loader implementation supports.  If not, throw an error, and mark is
     *   as a version-related error.  
     */
    if (ver_ > 1)
        err_throw_a(VMERR_IMAGE_INCOMPAT_VSN, 1, ERR_TYPE_VERSION_FLAG);
}

/* ------------------------------------------------------------------------ */
/*
 *   Execute the image 
 */
void CVmImageLoader::run(VMG_ const char *const *argv, int argc,
                         CVmRuntimeSymbols *global_symtab,
                         CVmRuntimeSymbols *macro_symtab,
                         const char *saved_state)
{
    CVmRuntimeSymbols *orig_runtime_symtab, *orig_runtime_macros;
    pool_ofs_t entry_code_ofs;
    int entry_code_argc;
    CCharmapToUni *argmap = 0;
    char *argstr = 0;
    
    /* make sure we found a code pool definition */
    if (pools_[0]->vmpbs_get_page_count() < 1)
        err_throw(VMERR_IMAGE_NO_CODE);

    /* 
     *   Find the entrypoint.  If we have a saved state, call the function
     *   given by the exported symbol "mainRestore".  Otherwise, call the
     *   exported main entrypoint function.  
     */
    if (saved_state != 0)
    {
        /* 
         *   We're restoring a saved state file immediately on startup - call
         *   the exported "mainRestore" function.  If there is no such
         *   export, we can't run the program with an initial saved state.  
         */
        if (G_predef->main_restore_func == 0)
            err_throw(VMERR_NO_MAINRESTORE);

        /* use the mainRestore export */
        entry_code_ofs = G_predef->main_restore_func;
    }
    else
    {
        /* ordinary startup - use the exported primary entrypoint */
        entry_code_ofs = entrypt_;
    }

    /* we have no arguments to the entrypoint function yet */
    entry_code_argc = 0;

    /* set myself as the global image loader */
    G_image_loader = this;

    /* remember the original runtime symbol table */
    orig_runtime_symtab = runtime_symtab_;
    orig_runtime_macros = runtime_macros_;

    /* we haven't set the file path */
    int file_path_set = FALSE;

    /* catch any errors so we restore globals on the way out */
    err_try
    {
        int i;
        
        /* if there's no file path, use the image file folder by default */
        if (G_file_path == 0)
        {
            G_file_path = lib_copy_str(get_path());
            file_path_set = TRUE;
        }

        /* 
         *   if the caller gave us a runtime symbol table, use it over any
         *   we have already stored 
         */
        if (global_symtab != 0)
            runtime_symtab_ = global_symtab;
        if (macro_symtab != 0)
            runtime_macros_ = macro_symtab;

        /* create a LookupTable for the reflection symbols, if any */
        create_global_symtab_lookup_table(vmg0_);

        /* 
         *   if we successfully created a reflection symbol table, push it
         *   onto the stack - this will ensure that the object won't be
         *   discarded as long as the program is running 
         */
        if (reflection_symtab_ != VM_INVALID_OBJ)
        {
            /* set up an object value for the table and push it */
            G_stk->push()->set_obj(reflection_symtab_);
        }

        /* create and stack the reflection LookupTable for macros */
        create_macro_symtab_lookup_table(vmg0_);
        if (reflection_macros_ != VM_INVALID_OBJ)
            G_stk->push()->set_obj(reflection_macros_);

        /* 
         *   run static initializers (do this after creating the symbol
         *   table, in case any of the initializers want to access the
         *   symbol table) 
         */
        run_static_init(vmg0_);

        /* if there's a saved state file to restore, push it */
        if (saved_state != 0)
        {
            /* create a string object for the filename, and push it */
            G_stk->push()->set_obj(
                CVmObjString::create(vmg_ FALSE,
                                     saved_state, strlen(saved_state)));

            /* count the extra startup argument */
            ++entry_code_argc;
        }

        /* get a character mapper for the command-line arguments */
        if (argc > 0)
        {
            /* 
             *   If we have a net configuration, assume that the arguments
             *   are in UTF-8 format, since they're coming from a web server.
             *   Otherwise, ask the OS which character set to use for
             *   command-line parameters.
             */
            char argcs[40];
            if (G_net_config != 0)
                strcpy(argcs, "utf8");
            else
                os_get_charmap(argcs, OS_CHARMAP_CMDLINE);

            /* load the character set */
            argmap = CCharmapToUni::load(
                G_host_ifc->get_sys_res_loader(), argcs);
        }

        /* 
         *   push a string object for each argument - push them onto the
         *   stack to ensure that they're referenced in case garbage
         *   collection occurs while we're working 
         */
        for (i = 0 ; i < argc ; ++i)
        {
            /* if we have a character mapper, map the argument string */
            size_t slen;
            if (argmap != 0)
            {
                /* map the argument to UTF8 */
                slen = argmap->map_str_alo(&argstr, argv[i]);
            }
            else
            {
                /* make a copy of the string */
                slen = strlen(argv[i]);
                argstr = (char *)t3malloc(slen + 1);
                memcpy(argstr, argv[i], slen + 1);

                /* make sure it's well-formed UTF-8 */
                CCharmapToUni::validate(argstr, slen);
            }

            /* push the mapped/adjusted string argument */
            G_interpreter->push_string(vmg_ argstr, slen);

            /* done with the mapped/adjusted string */
            t3free(argstr);
            argstr = 0;
        }

        /* create a list to hold the strings */
        vm_obj_id_t lst_obj = CVmObjList::create(vmg_ FALSE, argc);
        CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ lst_obj);

        /* set the list elements to the strings */
        for (i = 0 ; i < argc ; ++i)
        {
            vm_val_t *strp;
            
            /* 
             *   Get this item from the stack.  Note that the most recently
             *   pushed item is number 0, then number 1, and so on - the
             *   last argument string (at index argc) is thus number 0, and
             *   the first (at index 0) is number argc.  
             */
            strp = G_stk->get(argc - i - 1);

            /* set this list element */
            lst->cons_set_element(i, strp);
        }

        /* 
         *   we don't need the strings themselves any more, as we can reach
         *   them through the list, so drop the strings from the stack 
         */
        G_stk->discard(argc);
        
        /* push the list - it's the argument to the program entrypoint */
        G_stk->push()->set_obj(lst_obj);
        ++entry_code_argc;

        /* 
         *   Invoke the entrypoint function - it takes two arguments (the
         *   list of argument strings, and the name of the initial saved
         *   state file to restore, if any).  
         */
        G_stk->push()->set_propid(VM_INVALID_PROP);
        G_stk->push()->set_nil();
        G_stk->push()->set_nil();
        G_stk->push()->set_nil_obj();
        G_stk->push()->set_fnptr(entry_code_ofs);
        vm_rcdesc rc("Main entrypoint");
        G_interpreter->do_call(
            vmg_ 0,
            (const uchar *)G_code_pool->get_ptr(entry_code_ofs),
            entry_code_argc, &rc);

        /* 
         *   if we pushed a global symbol table LookupTable object, pop it -
         *   we left it on the stack to protect it from the garbage
         *   collector, since it's always implicitly referenced from the
         *   intrinsic function that retrieves the symbol table object 
         */
        if (reflection_macros_ != VM_INVALID_OBJ)
            G_stk->discard();
        if (reflection_symtab_ != VM_INVALID_OBJ)
            G_stk->discard();
    }
    err_finally
    {
        /* forget the image loader */
        G_image_loader = 0;

        /* restore the original runtime symbol tables */
        runtime_symtab_ = orig_runtime_symtab;
        runtime_macros_ = orig_runtime_macros;

        /* release the command-line character mapper */
        if (argmap != 0)
            argmap->release_ref();

        /* release any argument string we were working on */
        if (argstr != 0)
            t3free(argstr);

        /* 
         *   if we set the file path, un-set it, so that callers don't think
         *   they have to do so 
         */
        if (file_path_set)
        {
            lib_free_str(G_file_path);
            G_file_path = 0;
        }
    }
    err_end;
}

/*
 *   Run static initializers 
 */
void CVmImageLoader::run_static_init(VMG0_)
{
    CVmStaticInitPage *pg;

    /* run through the list of static initializers */
    for (pg = static_head_ ; pg != 0 ; pg = pg->nxt_)
    {
        size_t i;

        /* run through the initializers on this page */
        for (i = 0 ; i < pg->cnt_ ; ++i)
        {
            vm_obj_id_t obj;
            vm_prop_id_t prop;
            vm_val_t target;

            /* get this initializer's object and property ID */
            obj = pg->get_obj_id(i);
            prop = pg->get_prop_id(i);
            target.set_obj(obj);

            /* invoke this object.property */
            err_try
            {
                /* evaluate the property */
                G_interpreter->get_prop(vmg_ 0, &target, prop, &target, 0, 0);
            }
            err_catch(exc)
            {
                /* 
                 *   If the "error" is a debugger terminate, pass it through
                 *   - this simply means that the user is manually ending the
                 *   program via the debugger UI.  Likewise for restarts or
                 *   interrupts.
                 */
                if (exc->get_error_code() == VMERR_DBG_HALT
                    || exc->get_error_code() == VMERR_DBG_RESTART
                    || exc->get_error_code() == VMERR_DBG_INTERRUPT)
                    err_rethrow();
                
                /* get the message for the exception that occurred */
                char errbuf[512];
                CVmRun::get_exc_message(vmg_ exc, errbuf, sizeof(errbuf),
                                        FALSE);

                /* presume we won't find names */
                size_t obj_len = 0, prop_len = 0;
                const char *obj_name = 0, *prop_name = 0;

                /* find the obj and prop names in the global symbols */
                if (runtime_symtab_ != 0)
                {
                    /* find the object name */
                    obj_name = runtime_symtab_
                               ->find_obj_name(vmg_ obj, &obj_len);

                    /* find the property name */
                    prop_name = runtime_symtab_
                                ->find_prop_name(vmg_ prop, &prop_len);
                }

                /* if we didn't find the names, use placeholders */
                if (obj_name == 0)
                {
                    obj_name = "[object name not available]";
                    obj_len = strlen(obj_name);
                }
                if (prop_name == 0)
                {
                    prop_name = "[property name not available]";
                    prop_len = strlen(prop_name);
                }

                /* throw a new error for the static initializer failure */
                err_throw_a(VMERR_EXC_IN_STATIC_INIT, 3,
                            ERR_TYPE_TEXTCHAR_LEN, obj_name, obj_len,
                            ERR_TYPE_TEXTCHAR_LEN, prop_name, prop_len,
                            ERR_TYPE_CHAR, errbuf);
            }
            err_end;
        }
    }
}

/*
 *   Unload the image.  This detaches the pools from the backing stores,
 *   which must be done before the loaded copy of the image file can be
 *   deleted.  
 */
void CVmImageLoader::unload(VMG0_)
{
    /* detach the code pool from its backing store */
    G_code_pool->detach_backing_store();

    /* detach the constant pool from its backing store */
    G_const_pool->detach_backing_store();
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a LookupTable to hold the symbols in the global symbol table. 
 */
void CVmImageLoader::create_global_symtab_lookup_table(VMG0_)
{
    CVmObjLookupTable *lookup;
    vm_runtime_sym *sym;

    /* 
     *   if we don't have a runtime symbol table, we can't create the
     *   reflection LookupTable 
     */
    if (runtime_symtab_ == 0)
        return;

    /* 
     *   if we already have a reflection symbol table, there's no need to
     *   create another 
     */
    if (reflection_symtab_ != VM_INVALID_OBJ)
        return;
    
    /* create a LookupTable to hold the symbols */
    reflection_symtab_ = CVmObjLookupTable::
        create(vmg_ FALSE, 256, runtime_symtab_->get_sym_count());

    /* get the object, properly cast */
    lookup = (CVmObjLookupTable *)vm_objp(vmg_ reflection_symtab_);

    /* push the lookup table onto the stack for gc protection */
    G_stk->push()->set_obj(reflection_symtab_);

    /* run through the symbols and populate the LookupTable */
    for (sym = runtime_symtab_->get_head() ; sym != 0 ; sym = sym->nxt)
    {
        vm_val_t str;

        /* 
         *   skip intrinsic class modifier objects (for the same reason we
         *   filter these out of firstObj/nextObj iterations: they're not
         *   meaningful objects as far as the program is concerned, so
         *   there's no reason for the program to see them) 
         */
        if (sym->val.typ == VM_OBJ
            && CVmObjIntClsMod::is_intcls_mod_obj(vmg_ sym->val.val.obj))
        {
            /* it's an intrinsic class modifier - skip it */
            continue;
        }

        /* create a string object to hold this symbol */
        str.set_obj(CVmObjString::create(vmg_ FALSE, sym->sym, sym->len));

        /* 
         *   add it to the lookup table - the symbol string is the key, and
         *   the symbol's value is the lookup entry value 
         */
        lookup->add_entry(vmg_ &str, &sym->val);
    }

    /* done working - discard our gc protection */
    G_stk->discard();
}

/*
 *   Create a LookupTable to hold the symbols in the macro table
 */
void CVmImageLoader::create_macro_symtab_lookup_table(VMG0_)
{
    CVmObjLookupTable *lookup;
    vm_runtime_sym *sym;

    /* 
     *   if we don't have a runtime macro table, we can't create the
     *   reflection LookupTable 
     */
    if (runtime_macros_ == 0)
        return;

    /* if we already created the table, there's no need to create another */
    if (reflection_macros_ != VM_INVALID_OBJ)
        return;

    /* create a LookupTable to hold the symbols */
    reflection_macros_ = CVmObjLookupTable::
        create(vmg_ FALSE, 256, runtime_macros_->get_sym_count());

    /* get the object, properly cast */
    lookup = (CVmObjLookupTable *)vm_objp(vmg_ reflection_macros_);

    /* push the lookup table onto the stack for gc protection */
    G_stk->push()->set_obj(reflection_macros_);

    /* run through the symbols and populate the LookupTable */
    for (sym = runtime_macros_->get_head() ; sym != 0 ; sym = sym->nxt)
    {
        vm_val_t str;
        vm_val_t lst;
        vm_val_t args;
        vm_val_t v;
        int i;

        /* create a string object to hold this symbol */
        str.set_obj(CVmObjString::create(vmg_ FALSE, sym->sym, sym->len));

        /* stack it for gc protection */
        G_stk->push(&str);

        /* 
         *   Create a list to hold the definition.  We represent this as
         *   [expansion_string, [argument_list], flags]. 
         */
        lst.set_obj(CVmObjList::create(vmg_ FALSE, 3));
        G_stk->push(&lst);
        CVmObjList *lstp = (CVmObjList *)vm_objp(vmg_ lst.val.obj);
        lstp->cons_clear();

        /* create and store the expansion string */
        v.set_obj(CVmObjString::create(
            vmg_ FALSE, sym->macro_expansion, sym->macro_exp_len));
        lstp->cons_set_element(0, &v);

        /* create the argument list */
        args.set_obj(CVmObjList::create(vmg_ FALSE, sym->macro_argc));
        CVmObjList *argp = (CVmObjList *)vm_objp(vmg_ args.val.obj);
        argp->cons_clear();

        /* store the argument list in the definition list */
        lstp->cons_set_element(1, &args);

        /* store the arguments */
        for (i = 0 ; i < sym->macro_argc ; ++i)
        {
            /* create the string */
            char *p = sym->macro_args[i];
            size_t len = strlen(p);
            v.set_obj(CVmObjString::create(vmg_ FALSE, p, len));

            /* store it in the argument list */
            argp->cons_set_element(i, &v);
        }

        /* store the flags */
        v.set_int(sym->macro_flags);
        lstp->cons_set_element(2, &v);

        /* 
         *   add the macro to the lookup table - the symbol string is the
         *   key, and the symbol's value is the definition list
         */
        lookup->add_entry(vmg_ &str, &lst);

        /* done with the symbol and list gc protection */
        G_stk->discard(2);
    }

    /* done working - discard our gc protection */
    G_stk->discard();
}


/* ------------------------------------------------------------------------ */
/*
 *   Load an Entrypoint (ENTP) block
 */
void CVmImageLoader::load_entrypt(VMG_ ulong siz)
{
    char buf[32];
    
    /* if we've already loaded an entrypoint, throw an error */
    if (loaded_entrypt_)
        err_throw(VMERR_IMAGE_ENTRYPT_REDEF);

    /* read the entrypoint offset */
    read_data(buf, 16, &siz);

    /* set the entrypoint */
    entrypt_ = t3rp4u(buf);

    /* set the method header size in the interpreter */
    G_interpreter->set_funchdr_size(osrp2(buf+4));

    /* set the exception table entry size global */
    G_exc_entry_size = osrp2(buf+6);

    /* set the debugger source line record size global */
    G_line_entry_size = osrp2(buf+8);

    /* set the debug table header size */
    G_dbg_hdr_size = osrp2(buf+10);

    /* set the debug local symbol header size */
    G_dbg_lclsym_hdr_size = osrp2(buf+12);

    /* set the debug version ID */
    G_dbg_fmt_vsn = osrp2(buf+14);

    /* set the frame size */
    G_dbg_frame_size = 4;
    if (siz >= 2)
    {
        read_data(buf, 2, &siz);
        G_dbg_frame_size = osrp2(buf);
    }

    /* note that we've loaded it */
    loaded_entrypt_ = TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a Constant Pool Definition (CPDF) data block 
 */
void CVmImageLoader::load_const_pool_def(ulong siz)
{
    char buf[32];
    uint pool_id;
    ulong page_count;
    ulong page_size;
    
    /* read the block data */
    read_data(buf, 10, &siz);

    /* decode the pool identifier, page count, and page size */
    pool_id = osrp2(buf);
    page_count = t3rp4u(buf + 2);
    page_size = t3rp4u(buf + 6);

    /* ensure that the pool ID is valid */
    if (pool_id < 1 || pool_id > sizeof(pools_)/sizeof(pools_[0]))
        err_throw(VMERR_IMAGE_BAD_POOL_ID);

    /* adjust the pool to 0-based range */
    --pool_id;

    /* initialize the pool */
    pools_[pool_id]->init(fp_, page_count, page_size);
}


/* ------------------------------------------------------------------------ */
/*
 *   Load a Constant Pool Page (CPPG) data block 
 */
void CVmImageLoader::load_const_pool_page(ulong siz)
{
    char buf[32];
    uint pool_id;
    ulong idx;
    uchar xor_mask;

    /* read the header */
    read_data(buf, 7, &siz);

    /* decode the pool ID and page index */
    pool_id = osrp2(buf);
    idx = t3rp4u(buf + 2);
    xor_mask = buf[6];

    /* ensure that the pool ID is valid */
    if (pool_id < 1 || pool_id > sizeof(pools_)/sizeof(pools_[0]))
        err_throw(VMERR_IMAGE_BAD_POOL_ID);

    /* adjust the pool to 0-based range */
    --pool_id;

    /* set the page information */
    pools_[pool_id]->set_page_info(idx, fp_->get_seek(), siz, xor_mask);

    /* 
     *   Skip past the page data, which follow the header - we don't need
     *   to load the page data right now, but merely note where it is for
     *   later loading.  The caller may choose to load page data only as
     *   required, rather than all at once, so we don't want to bring it
     *   into memory right now. 
     */
    fp_->skip_ahead(siz);
}


/* ------------------------------------------------------------------------ */
/*
 *   Load a Static Object (OBJS) data block 
 */
void CVmImageLoader::load_static_objs(VMG_ ulong siz)
{
    char buf[32];
    uint obj_count;
    uint meta_idx;
    int header_size;
    int large_objs;
    uint flags;
    int trans;

    /* read the number of objects, the metaclass index, and the flags */
    read_data(buf, 6, &siz);

    /* decode the object count and metaclass index values */
    obj_count = osrp2(buf);
    meta_idx = osrp2(buf + 2);

    /* decode the flags */
    flags = osrp2(buf + 4);
    large_objs = ((flags & 1) != 0);
    trans = ((flags & 2) != 0);

    /* calculate the per-object header size */
    header_size = 4 + (large_objs ? 4 : 2);

    /* read the objects */
    for ( ; obj_count != 0 ; --obj_count)
    {
        vm_obj_id_t obj_id;
        size_t obj_size;
        const char *data_ptr;
        
        /* read the object ID and data size */
        read_data(buf, header_size, &siz);

        /* get the object ID */
        obj_id = (vm_obj_id_t)t3rp4u(buf);

        /* get the size */
        if (large_objs)
        {
            /* 
             *   Make sure the object size doesn't overflow the local
             *   hardware limits.  (This test is only necessary if such an
             *   overflow is possible given the datatypes.  On most modern
             *   architectures, it simply isn't possible because OSMALMAX is
             *   at least as large as any 32-bit value we could read here.
             *   Note that the value we read here is explicitly a 32-bit
             *   value, so the biggest it could possibly be is 0xFFFFFFFF.)  
             */
#if 0xFFFFFFFFU > OSMALMAX
            if (t3rp4u(buf + 4) > (unsigned long)OSMALMAX)
                err_throw(VMERR_OBJ_SIZE_OVERFLOW);
#endif
            
            /* read the object size */
            obj_size = (size_t)t3rp4u(buf + 4);
        }
        else
        {
            /* read the 16-bit object size */
            obj_size = osrp2(buf + 4);
        }

        /* load the data, allocating space for it */
        data_ptr = alloc_and_read(obj_size, &siz);

        /* create a new object of the required metaclass */
        G_meta_table->create_from_image(vmg_ meta_idx, obj_id,
                                        data_ptr, obj_size);

        /* mark the object as transient, if appropriate */
        if (trans)
            G_obj_table->set_obj_transient(obj_id);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a multi-media resource (MRES) block 
 */
void CVmImageLoader::load_mres(ulong siz, CVmImageLoaderMres *res_ifc)
{
    char buf[16];
    uint entry_cnt;
    long base_ofs;
    uint i;

    /* 
     *   Note the current physical seek position - each entry's seek position
     *   is stored in the table of contents as an offset from this location.
     *   
     *   Compute the physical base offset: this is the logical base offset in
     *   our image stream, plus the offset of the image stream within the
     *   image file.  We need the physical base offset because the underlying
     *   interpreter's resource loader works with the physical file, not the
     *   logical image stream.  
     */
    base_ofs = fp_->get_seek() + base_seek_ofs_;
    
    /* read the entry count and size of the table of contents */
    read_data(buf, 2, &siz);
    entry_cnt = osrp2(buf);

    /* read the entries */
    for (i = 0 ; i < entry_cnt ; ++i)
    {
        uint32_t entry_ofs;
        uint32_t entry_size;
        uint entry_name_len;
        char name_buf[256];
        char *p;
        size_t rem;

        /* read the fixed part of the table-of-contents entry */
        read_data(buf, 9, &siz);
        entry_ofs = t3rp4u(buf);
        entry_size = t3rp4u(buf + 4);
        entry_name_len = (uchar)*(buf + 8);

        /* read the name */
        read_data(name_buf, entry_name_len, &siz);

        /* null-terminate the name */
        name_buf[entry_name_len] = '\0';

        /* XOR the bytes of the name with 0xFF */
        for (p = name_buf, rem = entry_name_len ; rem != 0 ; ++p, --rem)
             *p ^= 0xFF;

        /* add the resource to the resource interface */
        res_ifc->add_resource(base_ofs + entry_ofs, entry_size,
                              name_buf, entry_name_len);
    }

    /* 
     *   skip the data portion of the block, since we now have a map of
     *   the data and can load individual resources on demand 
     */
    if (siz != 0)
        fp_->skip_ahead(siz);
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a multi-media resource link (MREL) block 
 */
void CVmImageLoader::load_mres_link(ulong siz, CVmImageLoaderMres *res_ifc)
{
    char buf[16];
    uint entry_cnt;
    uint i;

    /* read the entry count and size of the table of contents */
    read_data(buf, 2, &siz);
    entry_cnt = osrp2(buf);

    /* read the entries */
    for (i = 0 ; i < entry_cnt ; ++i)
    {
        char buf[1];
        uint resname_len, fname_len;
        char resname_buf[256], fname_buf[256];

        /* read the resource name */
        read_data(buf, 1, &siz);
        resname_len = (uchar)buf[0];
        read_data(resname_buf, resname_len, &siz);
        resname_buf[resname_len] = '\0';

        /* read the local filename this resource is linked to */
        read_data(buf, 1, &siz);
        fname_len = (uchar)buf[0];
        read_data(fname_buf, fname_len, &siz);
        fname_buf[fname_len] = '\0';

        /* add the resource to the resource interface */
        res_ifc->add_resource(fname_buf, fname_len, resname_buf, resname_len);
    }

    /* 
     *   skip the data portion of the block, since we now have a map of
     *   the data and can load individual resources on demand 
     */
    if (siz != 0)
        fp_->skip_ahead(siz);
}

/* ------------------------------------------------------------------------ */
/* 
 *   load a Metaclass Dependency block 
 */
void CVmImageLoader::load_meta_dep(VMG_ ulong siz)
{
    char buf[256 + 10];
    uint entry_cnt;
    uint i;

    /* it's an error if we've already seen a dependency block */
    if (loaded_meta_dep_)
        err_throw(VMERR_IMAGE_METADEP_REDEF);

    /* note that we've loaded a dependency block */
    loaded_meta_dep_ = TRUE;

    /* 
     *   reset the global metaclass table, in case there was anything
     *   defined by a previously-loaded image - since a metaclass's index
     *   in the table is implicit in the load order, we need to make sure
     *   we're starting with a completely clean configuration 
     */
    G_meta_table->clear();
    
    /* read the number of entries in the table */
    read_data(buf, 2, &siz);
    entry_cnt = osrp2(buf);

    /* read the entries */
    for (i = 0 ; i < entry_cnt ; ++i)
    {
        ushort j;
        uint len;
        uint prop_cnt;
        uint prop_len;
        vm_meta_entry_t *entry;
        char *p;
        ulong done_size;
        long prop_start_ofs;
        ulong prop_start_siz;
        vm_prop_id_t max_prop;
        vm_prop_id_t min_prop;

        /* 
         *   read the record size, and calculate the value of 'siz' that
         *   we expect when we've parsed this entire record - it's the
         *   current size minus the size of the record (note that we have
         *   to add back in the two bytes of the size record itself, since
         *   'siz' will have been moved past the size record once we've
         *   read it) 
         */
        read_data(buf, 2, &siz);
        done_size = (siz + 2) - osrp2(buf);
        
        /* read the length of the entry */
        read_data(buf, 1, &siz);
        len = (uchar)buf[0];

        /* read the name and null-terminate it */
        read_data(buf, len, &siz);
        buf[len] = '\0';
        p = buf + len + 1;

        /* read the property table information */
        read_data(p, 4, &siz);
        prop_cnt = osrp2(p);
        prop_len = osrp2(p + 2);

        /* 
         *   set min and max to 'invalid' to start with, in case there are
         *   no properties at all in the table
         */
        min_prop = max_prop = VM_INVALID_PROP;

        /* 
         *   Read the properties - pass one: find the minimum and maximum
         *   property ID in the table.  Before we begin, remember where
         *   the properties start in the file, so we can go back and read
         *   the table again on the second pass.  
         */
        prop_start_ofs = fp_->get_seek();
        prop_start_siz = siz;
        for (j = 0 ; j < prop_cnt ; ++j)
        {
            vm_prop_id_t cur;
            
            /* read the next entry */
            read_data(p, 2, &siz);

            /* skip any additional data in the record */
            if (prop_len > 2)
                skip_data(prop_len - 2, &siz);

            /* get the property */
            cur = (vm_prop_id_t)osrp2(p);

            /* 
             *   if this is the first property, it's the max and min so
             *   far; otherwise, remember it if it's the highest or lowest
             *   so far 
             */
            if (j == 0)
            {
                /* this is the first one, so note it unconditionally */
                min_prop = max_prop = cur;
            }
            else
            {
                /* if it's below the current minimum, it's the new minimum */
                if (cur < min_prop)
                    min_prop = cur;

                /* if it's above the current maximum, it's the new maximum */
                if (cur > max_prop)
                    max_prop = cur;
            }
        }

        /* 
         *   Now that we know the minimum and maximum properties in the
         *   table, we can create the dependency record. 
         */
        G_meta_table->add_entry(buf, prop_cnt, min_prop, max_prop);

        /* get a pointer to my new entry */
        entry = G_meta_table->get_entry(i);

        /*
         *   Pass two: read the actual properties into the translation
         *   table.  Seek back to the start of the properties in the file,
         *   then read the ID's.
         *   
         *   Note that add_prop_xlat() expects a 1-based function table
         *   index.  We are in fact reading a function table, so run our
         *   function index counter from 1 to the entry count to yield the
         *   proper 1-based index values.  
         */
        fp_->seek(prop_start_ofs);
        siz = prop_start_siz;
        for (j = 1 ; j <= prop_cnt ; ++j)
        {
            /* read the next entry */
            read_data(p, 2, &siz);

            /* skip any additional data in the record */
            if (prop_len > 2)
                skip_data(prop_len - 2, &siz);

            /* add this entry */
            entry->add_prop_xlat((vm_prop_id_t)osrp2(p), j);
        }

        /* skip any remaining data in the record */
        if (siz > done_size)
            skip_data(siz - done_size, &siz);
    }

    /* 
     *   always add the root object metaclass to the table, whether the
     *   image file defines it or not 
     */
    G_meta_table
        ->add_entry_if_new(CVmObject::metaclass_reg_->get_reg_idx(),
                           0, VM_INVALID_PROP, VM_INVALID_PROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   load a Function Set Dependency block 
 */
void CVmImageLoader::load_funcset_dep(VMG_ ulong siz)
{
    char buf[256];
    uint entry_cnt;
    uint i;

    /* it's an error if we've already seen a dependency block */
    if (loaded_funcset_dep_)
        err_throw(VMERR_IMAGE_FUNCDEP_REDEF);

    /* note that we've loaded a dependency block */
    loaded_funcset_dep_ = TRUE;

    /* 
     *   reset the global function set dependency table, in case there was
     *   anything defined by a previously-loaded image - since a function
     *   set index in the table is implicit in the load order, we need to
     *   make sure we're starting with a completely clean configuration 
     */
    G_bif_table->clear(vmg0_);

    /* read the number of entries in the table */
    read_data(buf, 2, &siz);
    entry_cnt = osrp2(buf);

    /* read the entries */
    for (i = 0 ; i < entry_cnt ; ++i)
    {
        uint len;

        /* read the length of the entry */
        read_data(buf, 1, &siz);
        len = (uchar)buf[0];

        /* read the name and null-terminate it */
        read_data(buf, len, &siz);
        buf[len] = '\0';

        /* add the dependency */
        G_bif_table->add_entry(vmg_ buf);
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   load a Symbolic Names export block 
 */
void CVmImageLoader::load_sym_names(VMG_ ulong siz)
{
    char buf[256];
    uint entry_cnt;
    uint i;
    
    /* read the number of entries */
    read_data(buf, 2, &siz);
    entry_cnt = osrp2(buf);

    /* read the entries */
    for (i = 0 ; i < entry_cnt ; ++i)
    {
        vm_val_t val;
        uint namelen;

        /* read the data holder and name length */
        read_data(buf, VMB_DATAHOLDER + 1, &siz);
        vmb_get_dh(buf, &val);
        namelen = (uchar)*(buf + VMB_DATAHOLDER);

        /* read the name */
        read_data(buf, namelen, &siz);

        /* add this as a new entry to our exports table */
        exports_->add(new CVmHashEntryExport(buf, namelen, TRUE, &val));
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a Global Symbols (GSYM) block into the runtime symbol table 
 */
void CVmImageLoader::load_runtime_symtab_from_gsym(VMG_ ulong siz)
{
    const size_t TOK_SYM_MAX_LEN = 80;
    char buf[TOK_SYM_MAX_LEN + 128];
    ulong cnt;

    /* allocate the symbol table if we haven't already done so */
    if (runtime_symtab_ == 0)
        runtime_symtab_ = new CVmRuntimeSymbols();

    /* read the symbol count */
    read_data(buf, 4, &siz);
    cnt = t3rp4u(buf);

    /* read the symbols and populate the symbol table */
    for ( ; cnt != 0 ; --cnt)
    {
        char *sym_name;
        size_t sym_len;
        size_t dat_len;
        tc_symtype_t sym_type;
        char *dat;
        vm_val_t val;

        /* read the symbol's length, extra data length, and type code */
        read_data(buf, 6, &siz);
        sym_len = osrp2(buf);
        dat_len = osrp2(buf + 2);
        sym_type = (tc_symtype_t)osrp2(buf + 4);

        /* check the lengths to make sure they don't overflow our buffer */
        if (sym_len > TOK_SYM_MAX_LEN)
        {
            /* 
             *   this symbol name is too long - skip the symbol and its
             *   extra data entirely 
             */
            skip_data(sym_len + dat_len, &siz);

            /* go on to the next symbol */
            continue;
        }
        else if (dat_len + sym_len > sizeof(buf))
        {
            /* 
             *   The extra data block is too long for our fixed buffer.
             *   Truncate the extra data so that we don't overflow our
             *   buffer, but proceed anyway with the truncated extra data.
             *   Note that we won't lose any data we care about, since any
             *   extra data beyond our buffer size is additional future
             *   format data that we don't know how to handle anyway.  By
             *   design, we're allowed to skip extra data that we don't know
             *   how to read.  
             */
            read_data(buf, sizeof(buf), &siz);

            /* skip the remainder of the extra data */
            skip_data(sym_len + dat_len - sizeof(buf), &siz);
        }
        else
        {
            /* read the symbol's name and its type-specific data */
            read_data(buf, sym_len + dat_len, &siz);
        }

        /* the symbol name is at the start of the buffer */
        sym_name = buf;

        /* get the pointer to the extra data (it comes after the name) */
        dat = buf + sym_len;

        /* create the new symbol table entry, depending on its type */
        switch(sym_type)
        {
        case TC_SYM_FUNC:
            /* set up the function pointer value */
            val.set_fnptr(t3rp4u(dat));
            break;

        case TC_SYM_OBJ:
            /* set up the object value */
            val.set_obj(t3rp4u(dat));
            break;

        case TC_SYM_PROP:
            /* set up the property value */
            val.set_propid(osrp2(dat));

            /* check the 'dictionary property' flag */
            if (dat_len >= 3 && (dat[0] & 0x01) != 0)
            {
                // $$$ it's a dictionary property
            }
            break;

        case TC_SYM_ENUM:
            /* set up the enum value */
            val.set_enum(t3rp4u(dat));

            /* check the 'enum token' flag */
            if (dat_len >= 5 && (dat[0] & 0x01) != 0)
            {
                // $$$ it's an 'enum token'
            }
            break;

        case TC_SYM_METACLASS:
            /* set up the metaclass object value */
            val.set_obj(t3rp4u(dat + 2));
            break;

        case TC_SYM_BIF:
            /* built-in function */
            val.set_bifptr(osrp2(dat + 2), osrp2(dat));
            break;

        default:
            /* 
             *   ignore other types; mark the value as 'empty' so we know we
             *   don't have anything to add to the table 
             */
            val.set_empty();
            break;
        }

        /* if we found a valid type, add it to the table */
        if (val.typ != VM_EMPTY)
            runtime_symtab_->add_sym(sym_name, sym_len, &val);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a Macro Definitions (MACR) block into the runtime symbol table 
 */
void CVmImageLoader::load_runtime_symtab_from_macr(VMG_ ulong siz)
{
    const size_t TOK_SYM_MAX_LEN = 80;
    char buf[10];
    char sym[TOK_SYM_MAX_LEN];
    ulong cnt;

    /* allocate the macro table if we haven't already done so */
    if (runtime_macros_ == 0)
        runtime_macros_ = new CVmRuntimeSymbols();

    /* read the symbol count */
    read_data(buf, 4, &siz);
    cnt = t3rp4u(buf);

    /* read the symbols and populate the symbol table */
    for ( ; cnt != 0 ; --cnt)
    {
        size_t sym_len;
        size_t exp_len;
        int argc;
        unsigned int flags;
        int i;

        /* read the symbol's length */
        read_data(buf, 2, &siz);
        sym_len = osrp2(buf);

        /* if it fits, read it */
        if (sym_len <= TOK_SYM_MAX_LEN)
        {
            /* read the name */
            read_data(sym, sym_len, &siz);
        }
        else
        {
            /* it's too long - skip it */
            skip_data(sym_len, &siz);
        }

        /* read the flags and argument count */
        read_data(buf, 4, &siz);
        flags = osrp2(buf);
        argc = osrp2(buf + 2);

        /* remember the current position */
        long start_pos = fp_->get_seek();
        ulong start_siz = siz;

        /* count up the argument sizes */
        size_t arg_len = 0;
        for (i = 0 ; i < argc ; ++i)
        {
            /* read this argument length */
            read_data(buf, 2, &siz);
            size_t l = osrp2(buf);

            /* add it to the total, plus a null byte */
            arg_len += l + 1;

            /* skip it in the file */
            skip_data(l, &siz);
        }

        /* read the expansion size */
        read_data(buf, 4, &siz);
        exp_len = osrp4(buf);

        /* allocate the symbol entry */
        vm_runtime_sym *entry = runtime_macros_->add_macro(
            sym, sym_len, exp_len, flags, argc, arg_len);

        /* seek back to the start of the argument list */
        fp_->seek(start_pos);
        siz = start_siz;

        /* read the arguments */
        for (i = 0 ; i < argc ; ++i)
        {
            /* read the argument length */
            read_data(buf, 2, &siz);
            size_t l = osrp2(buf);

            /* read this argument, and null-terminate it */
            read_data(entry->macro_args[i], l, &siz);
            entry->macro_args[i][l] = '\0';

            /* commit the storage */
            entry->commit_macro_arg(i, l + 1);
        }

        /* read the expansion */
        skip_data(4, &siz);
        read_data(entry->macro_expansion, exp_len, &siz);
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   load a Source File List block 
 */
void CVmImageLoader::load_srcfiles(VMG_ ulong siz)
{
    size_t i;
    char buf[64];
    size_t entry_cnt;
    size_t line_size;

    /* 
     *   if there's no source file table, we're not in debug mode and hence
     *   have no use for this type of information, so skip the block 
     */
    if (G_srcf_table == 0)
    {
        /* skip the entire block in the file */
        fp_->skip_ahead(siz);

        /* we're done */
        return;
    }

    /* clear any existing information in the table */
    G_srcf_table->clear();

    /* read the number of entries in the table, and the line record size */
    read_data(buf, 4, &siz);
    entry_cnt = osrp2(buf);
    line_size = osrp2(buf + 2);

    /* read the entries */
    for (i = 0 ; i < entry_cnt ; ++i)
    {
        long start_pos;
        long next_pos;
        int orig_index;
        size_t len;
        CVmSrcfEntry *entry;
        ulong line_cnt;
        ulong skip_amt;

        /* note the starting file location of this record */
        start_pos = fp_->get_seek();

        /* 
         *   read the size of this file record, the original index, and the
         *   length of the filename 
         */
        read_data(buf, 8, &siz);
        orig_index = osrp2(buf + 4);
        len = osrp2(buf + 6);

        /* 
         *   calculate the file location of the start of the next block by
         *   adding the starting position and size of this block 
         */
        next_pos = start_pos + t3rp4u(buf);

        /* allocate the entry */
        entry = G_srcf_table->add_entry(orig_index, len);

        /* read the data into this entry's buffer */
        read_data(entry->get_name_buf(), len, &siz);

        /* null-terminate the buffer */
        entry->get_name_buf()[len] = '\0';

        /* read the number of line records */
        read_data(buf, 4, &siz);
        line_cnt = t3rp4u(buf);

        /* 
         *   if the line records are too big for our buffer, or smaller than
         *   our required minimum size, don't bother reading them - we'll
         *   just skip them entirely 
         */
        if (line_size <= sizeof(buf) && line_size >= 8)
        {
            /* tell the source file table entry how many lines we have */
            entry->alloc_line_records(line_cnt);

            /* read the line records */
            for ( ; line_cnt != 0 ; --line_cnt)
            {
                ulong linenum;
                ulong code_addr;

                /* read the record */
                read_data(buf, line_size, &siz);

                /* get the source line number and byte code offset */
                linenum = t3rp4u(buf);
                code_addr = t3rp4u(buf + 4);

                /* add the line to the source file table entry */
                entry->add_line_record(linenum, code_addr);
            }
        }

        /* skip ahead to the start of the next record */
        skip_amt = next_pos - fp_->get_seek();
        if (skip_amt != 0)
            skip_data(skip_amt, &siz);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Perform dynamic linking after loading.  
 */
void CVmImageLoader::do_dynamic_link(VMG0_)
{
    struct sym_assoc_t
    {
        /* the symbol name */
        const char *sym;
        
        /* datatype */
        vm_datatype_t typ;
        
        /* address of predef table entry for this value */
        void *addr;

        /* 
         *   flag: this is a synthetic import, which means that we don't
         *   look for it in the image file, but only in the restored state 
         */
        int is_synthetic;
    };
    sym_assoc_t imports[] =
    {
        /* build the table by including the import list */
#define VM_IMPORT_OBJ(sym, mem)  { sym, VM_OBJ,  &G_predef->mem, FALSE },
#define VM_NOIMPORT_OBJ(sym, mem)  { sym, VM_OBJ,  &G_predef->mem, TRUE },
#define VM_IMPORT_PROP(sym, mem) { sym, VM_PROP, &G_predef->mem, FALSE },
#define VM_NOIMPORT_PROP(sym, mem) { sym, VM_PROP, &G_predef->mem, TRUE },
#define VM_IMPORT_FUNC(sym, mem) { sym, VM_FUNCPTR, &G_predef->mem, FALSE },
#include "vmimport.h"

        /* end the table */
        { 0, VM_NIL, 0, FALSE }
    };
    const sym_assoc_t *ip;

    /* reset all of the predefs to undefined */
    G_predef->reset();

    /*
     *   Run through our list of symbols to import from the load file, and
     *   load each one. 
     */
    for (ip = imports ; ip->sym != 0 ; ++ip)
    {
        CVmHashEntryExport *entry;

        /* presume we won't find it */
        entry = 0;

        /* 
         *   if it's not synthetic, look up this symbol in the image file's
         *   export table (don't look up synthetic symbols, because we
         *   always want to create these ourselves, not load them from an
         *   image file) 
         */
        if (!ip->is_synthetic)
        {
            /* it's a regular import - look for it in the image exports */
            entry = (CVmHashEntryExport *)exports_
                    ->find(ip->sym, strlen(ip->sym));
        }

        /* 
         *   if we didn't find the entry in the image file's exports, look
         *   in the synthesized exports, in case we're restoring state from
         *   a previous session 
         */
        if (entry == 0)
            entry = (CVmHashEntryExport *)synth_exports_
                    ->find(ip->sym, strlen(ip->sym));
        
        /* if the symbol isn't exported, skip it and proceed to the next */
        if (entry == 0)
            continue;

        /* 
         *   make sure the type of the symbol exported by the image file
         *   matches the type of value we want to import for the symbol 
         */
        if (ip->typ != entry->val_.typ)
        {
            /* the exported symbol has the wrong type - throw an error */
            err_throw_a(VMERR_INVAL_EXPORT_TYPE, 1, ERR_TYPE_CHAR, ip->sym);
        }

        /* set the value according to its type */
        switch(ip->typ)
        {
        case VM_OBJ:
            /* store the object value */
            *(vm_obj_id_t *)ip->addr = entry->val_.val.obj;
            break;

        case VM_PROP:
            *(vm_prop_id_t *)ip->addr = entry->val_.val.prop;
            break;

        case VM_FUNCPTR:
            *(pool_ofs_t *)ip->addr = entry->val_.val.ofs;
            break;

        default:
            /* we don't import any other types */
            break;
        }
    }

    /*
     *   If we don't already have one, create a vector to keep track of the
     *   last property ID allocated.  We store this in a vector object so
     *   that the property allocation mechanism easily integrates with the
     *   undo and save/restore mechanisms.  
     */
    if (G_predef->last_prop_obj == VM_INVALID_OBJ)
    {
        /* create the object */
        G_predef->last_prop_obj = CVmObjVector::create(vmg_ FALSE, 1);

        /* add it to the synthesized export list */
        add_synth_export_obj(VM_IMPORT_NAME_LASTPROPOBJ,
                             G_predef->last_prop_obj);

        /* 
         *   set up the object with the current last property, as indicated
         *   in the image file 
         */
        set_last_prop(vmg_ G_predef->last_prop);
    }

    /* 
     *   if the image didn't define an exceptionMessage property, allocate a
     *   new property ID for it 
     */
    if (G_predef->rterrmsg_prop == VM_INVALID_PROP)
    {
        /* allocate the property ID */
        G_predef->rterrmsg_prop = alloc_new_prop(vmg0_);

        /* add it to the synthesized export list */
        add_synth_export_prop(VM_IMPORT_NAME_RTERRMSG,
                              G_predef->rterrmsg_prop);
    }

    /* 
     *   Create the constant string and list placeholder objects - these are
     *   never imported from the image file, but must be dynamically created
     *   after loading is complete.
     *   
     *   Create these objects without values, since the values are
     *   irrelevant - they're needed only for their type information.  
     */
    if (G_predef->const_str_obj == VM_INVALID_OBJ)
    {
        /* allocate it */
        G_predef->const_str_obj = CVmObjString::create(vmg_ FALSE, 0);

        /* add it to the synthesized export list */
        add_synth_export_obj(VM_IMPORT_NAME_CONSTSTR,
                             G_predef->const_str_obj);
    }
    if (G_predef->const_lst_obj == VM_INVALID_OBJ)
    {
        /* allocate it */
        G_predef->const_lst_obj = CVmObjList::create(vmg_ FALSE, (size_t)0);

        /* add it to the synthesized export list */
        add_synth_export_obj(VM_IMPORT_NAME_CONSTLST,
                             G_predef->const_lst_obj);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Load a static initializer list block 
 */
void CVmImageLoader::load_sini(VMG_ ulong siz)
{
    char hdr[12];
    ulong siz_rem;
    ulong hdr_siz;
    ulong init_cnt;
    ulong init_rem;
    
    /* 
     *   read the header size, static code starting offset, and
     *   initializer count 
     */
    fp_->copy_data(hdr, 12);
    hdr_siz = t3rp4u(hdr);
    static_cs_ofs_ = t3rp4u(hdr + 4);
    init_cnt = t3rp4u(hdr + 8);

    /* skip any extra header information */
    if (hdr_siz > 12)
        fp_->skip_ahead(hdr_siz - 12);

    /* account for having read the header in our size remaining */
    siz_rem = siz - hdr_siz;

    /* read in chunks of VM_STATIC_INIT_PAGE_MAX initializers */
    for (init_rem = init_cnt ; init_rem != 0 ; )
    {
        size_t cur;
        CVmStaticInitPage *pg;

        /* read the page max, or the actual number remaining if fewer */
        cur = VM_STATIC_INIT_PAGE_MAX;
        if (init_rem < cur)
            cur = (size_t)init_rem;

        /* allocate the next page */
        pg = new CVmStaticInitPage(cur);

        /* link in the page */
        if (static_tail_ != 0)
            static_tail_->nxt_ = pg;
        else
            static_head_ = pg;
        static_tail_ = pg;

        /* read the page */
        pg->data_ = fp_->alloc_and_read(cur * 6, 0, siz_rem);

        /* adjust our counters for the chunk */
        init_rem -= cur;
        siz_rem -= cur * 6;
    }

    /* skip any data we don't use in this version */
    if (siz_rem != 0)
        fp_->skip_ahead(siz_rem);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a synthesized object export 
 */
void CVmImageLoader::add_synth_export_obj(const char *nm, vm_obj_id_t obj)
{
    vm_val_t val;

    /* add the new entry with an object value */
    val.set_obj(obj);
    synth_exports_->add(new CVmHashEntryExport(nm, strlen(nm), TRUE, &val));
}

/*
 *   Add a synthesized property export 
 */
void CVmImageLoader::add_synth_export_prop(const char *nm, vm_prop_id_t prop)
{
    vm_val_t val;

    /* add the new entry with a property value */
    val.set_propid(prop);
    synth_exports_->add(new CVmHashEntryExport(nm, strlen(nm), TRUE, &val));
}

/*
 *   Discard all synthesized exports.  When we're resetting to the image
 *   file state, or when we're about to restore a saved state, we must
 *   discard the synthesized exports from the prior load so that we start
 *   from the base image file state again. 
 */
void CVmImageLoader::discard_synth_exports()
{
    /* delete all of the entries in the table */
    synth_exports_->delete_all_entries();
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context for enumerating the synthesized export symbols for
 *   saving the list to a saved state file 
 */
struct synth_save_cb_ctx
{
    /* the file to which we're writing */
    CVmFile *fp;

    /* number of entries */
    long cnt;
};

/*
 *   Save the synthesized exports to a saved state file 
 */
void CVmImageLoader::save_synth_exports(VMG_ CVmFile *fp)
{
    synth_save_cb_ctx ctx;
    long pos;
    long end_pos;

    /* set up our context */
    ctx.fp = fp;
    ctx.cnt = 0;

    /* 
     *   write a placeholder count of zero, but remember where it is so we
     *   can come back and fix it up later 
     */
    pos = fp->get_pos();
    fp->write_uint4(0);

    /* enumerate all of the entries through our save callback */
    synth_exports_->enum_entries(&save_synth_export_cb, &ctx);

    /* go back and fix up the count, then seek back to the end */
    end_pos = fp->get_pos();
    fp->set_pos(pos);
    fp->write_uint4(ctx.cnt);
    fp->set_pos(end_pos);
}

/*
 *   Synthesized export enumeration callback - save to file 
 */
void CVmImageLoader::save_synth_export_cb(void *ctx0, CVmHashEntry *entry0)
{
    synth_save_cb_ctx *ctx = (synth_save_cb_ctx *)ctx0;
    CVmHashEntryExport *entry = (CVmHashEntryExport *)entry0;
    char buf[VMB_DATAHOLDER];

    /* write this entry's length and name to the file */
    ctx->fp->write_uint2(entry->getlen());
    ctx->fp->write_bytes(entry->getstr(), entry->getlen());

    /* write the value as a dataholder */
    vmb_put_dh(buf, &entry->val_);
    ctx->fp->write_bytes(buf, VMB_DATAHOLDER);

    /* count the entry */
    ++(ctx->cnt);
}

/*
 *   Restore the synthesized exports from a saved state file 
 */
int CVmImageLoader::restore_synth_exports(VMG_ CVmFile *fp,
                                          CVmObjFixup *fixups)
{
    long cnt;
    
    /* 
     *   discard the old synthesized exports - we want to entirely replace
     *   these with what we're about to load from the file 
     */
    G_image_loader->discard_synth_exports();

    /* read the number of synthesized exports */
    cnt = fp->read_uint4();

    /* read the exports */
    for ( ; cnt != 0 ; --cnt)
    {
        size_t len;
        char buf[256];
        char dh[VMB_DATAHOLDER];
        vm_val_t val;
        
        /* read the length of the name and the name */
        len = fp->read_uint2();
        fp->read_bytes(buf, len > sizeof(buf) ? sizeof(buf) : len);

        /* skip any part of the name that didn't fit in the buffer */
        if (len > sizeof(buf))
        {
            /* skip the extra bytes */
            fp->set_pos(fp->get_pos() + (len - sizeof(buf)));

            /* limit the length we use to the buffer size */
            len = sizeof(buf);
        }

        /* read the value */
        fp->read_bytes(dh, VMB_DATAHOLDER);

        /* if the value is an object reference, fix it up */
        fixups->fix_dh(vmg_ dh);

        /* get the value */
        vmb_get_dh(dh, &val);

        /* add the export */
        synth_exports_->add(new CVmHashEntryExport(buf, len, TRUE, &val));
    }

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate a new property ID. 
 */
vm_prop_id_t CVmImageLoader::alloc_new_prop(VMG0_)
{
    CVmObjVector *vec;
    vm_val_t idx_val;
    vm_val_t val;
    vm_val_t new_cont;

    /* get the LastPropObj vector */
    vec = (CVmObjVector *)vm_objp(vmg_ G_predef->last_prop_obj);

    /* 
     *   get the element at index 1 - our last property value is always at
     *   the first element 
     */
    idx_val.set_int(1);

    /* get the value from the vector */
    vec->index_val(vmg_ &val, G_predef->last_prop_obj, &idx_val);

    /* if we've allocated every available property ID, throw an error */
    if (val.val.prop == 65535)
        err_throw(VMERR_OUT_OF_PROPIDS);

    /* allocate the new property ID by incrementing the last used ID */
    ++val.val.prop;

    /* 
     *   update the vector with the new value (this new property ID is now
     *   the last property ID in use) 
     */
    vec->set_index_val(vmg_ &new_cont, G_predef->last_prop_obj,
                       &idx_val, &val);

    /* return the new last ID */
    return val.val.prop;
}

/*
 *   Get the last property ID.  This returns the last valid property ID
 *   currently in use in the program. 
 */
vm_prop_id_t CVmImageLoader::get_last_prop(VMG0_)
{
    /* get LastPropObj[1] */
    vm_val_t vec, ele;
    vec.set_obj(G_predef->last_prop_obj);
    vec.ll_index(vmg_ &ele, 1);

    /* return the value */
    return ele.val.prop;
}

/*
 *   Set the last property ID.  This updates the LastPropObj synthetic
 *   export object, so that the last property ID is properly integrated with
 *   the undo and save/restore mechanisms.  
 */
void CVmImageLoader::set_last_prop(VMG_ vm_prop_id_t last_prop)
{
    /* get the LastPropObj vector */
    CVmObjVector *vec = (CVmObjVector *)vm_objp(vmg_ G_predef->last_prop_obj);

    /* 
     *   set up the values - the index is always 1, since the vector
     *   contains only one element; and the value contains the new last
     *   property ID 
     */
    vm_val_t idx_val, new_val;
    idx_val.set_int(1);
    new_val.set_propid(G_predef->last_prop);

    /* update the vector */
    vm_val_t new_cont;
    vec->set_index_val(vmg_ &new_cont, G_predef->last_prop_obj,
                       &idx_val, &new_val);
}

/* ------------------------------------------------------------------------ */
/* 
 *   Copy data from the file into a buffer, decrementing a size counter.
 *   We'll throw a BLOCK_TOO_SMALL error if the read length exceeds the
 *   remaining size.  
 */
void CVmImageLoader::read_data(char *buf, size_t read_len,
                               ulong *remaining_size)
{
    /* ensure we have enough data left in our block */
    if (read_len > *remaining_size)
        err_throw(VMERR_IMAGE_BLOCK_TOO_SMALL);

    /* decrement the remaining size counter for the data we just read */
    *remaining_size -= read_len;

    /* read the data */
    fp_->copy_data(buf, read_len);
}

/* 
 *   skip data 
 */
void CVmImageLoader::skip_data(size_t skip_len, ulong *remaining_size)
{
    /* ensure we have enough data left in our block */
    if (skip_len > *remaining_size)
        err_throw(VMERR_IMAGE_BLOCK_TOO_SMALL);

    /* decrement the remaining size counter for the data we're skipping */
    *remaining_size -= skip_len;

    /* skip ahead in the underlying file */
    fp_->skip_ahead(skip_len);
}

/* 
 *   Allocate memory for data and read the data from the file,
 *   decrementing the amount read from a size counter.  Throws
 *   BLOCK_TOO_SMALL if the read length exceeds the remaining size.  
 */
const char *CVmImageLoader::alloc_and_read(size_t read_len,
                                           ulong *remaining_size)
{
    const char *p;
    
    /* ensure we have enough data left in our block */
    if (read_len > *remaining_size)
        err_throw(VMERR_IMAGE_BLOCK_TOO_SMALL);

    /* allocate space, and read the data into the new space */
    p = fp_->alloc_and_read(read_len, 0, *remaining_size);

    /* decrement the remaining size counter for the data we just read */
    *remaining_size -= read_len;

    /* return the pointer to the new space */
    return p;
}


/* ------------------------------------------------------------------------ */
/*
 *   Image Pool tracker implementation 
 */

CVmImagePool::CVmImagePool()
{
    /* we don't have any information about the pool yet */
    page_count_ = 0;
    page_size_ = 0;
    page_info_ = 0;
    fp_ = 0;
}

CVmImagePool::~CVmImagePool()
{
    /* if we allocated a set of page offset arrays, delete them */
    if (page_info_ != 0)
    {
        size_t i;
        size_t cnt;

        /* compute the number of subarrays we have */
        cnt = get_subarray_count();

        /* delete each subarray */
        for (i = 0 ; i < cnt ; ++i)
            t3free(page_info_[i]);

        /* delete the master array */
        t3free(page_info_);
    }
}

/*
 *   initialize 
 */
void CVmImagePool::init(CVmImageFile *fp, ulong page_count, ulong page_size)
{
    size_t cnt;
    size_t i;

    /* remember the page count and page size */
    page_count_ = page_count;
    page_size_ = page_size;

    /* remember the underlying image file */
    fp_ = fp;

    /* if this pool has already been defined, throw an error */
    if (page_info_ != 0)
        err_throw(VMERR_IMAGE_POOL_REDEF);
    
    /* 
     *   Allocate the main page array.  We need one entry for each
     *   subarray, so figure the subarray count and allocate the
     *   appropriate amount of space.  
     */
    cnt = get_subarray_count();
    page_info_ = (CVmImagePool_pg **)t3malloc(cnt * sizeof(page_info_[0]));

    /* throw an error if that failed */
    if (page_info_ == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* clear the array */
    memset(page_info_, 0, cnt * sizeof(page_info_[0]));

    /* allocate the subarrays */
    for (i = 0 ; i < cnt ; ++i)
    {
        /* allocate this page, which contains file offsets */
        page_info_[i] =
            (CVmImagePool_pg *)t3malloc(VMIMAGE_POOL_SUBARRAY_SIZE
                                        * sizeof(page_info_[i][0]));

        /* throw an error if that failed */
        if (page_info_[i] == 0)
            err_throw(VMERR_OUT_OF_MEMORY);

        /* clear the memory */
        memset(page_info_[i], 0,
               VMIMAGE_POOL_SUBARRAY_SIZE * sizeof(page_info_[i][0]));
    }
}

/*
 *   Set a page seek offset 
 */
void CVmImagePool::set_page_info(ulong page_idx, long seek_pos,
                                 size_t page_size, uchar xor_mask)
{
    CVmImagePool_pg *info;
    
    /* 
     *   make sure we've allocated the page array and that this index is
     *   in range -- throw an error if not 
     */
    if (page_info_ == 0)
        err_throw(VMERR_IMAGE_POOL_BEFORE_DEF);
    if (page_idx >= page_count_)
        err_throw(VMERR_IMAGE_POOL_BAD_PAGE);

    /* get the entry for this page */
    info = get_page_info(page_idx);

    /* set the information */
    info->seek_pos = seek_pos;
    info->page_size = page_size;
    info->xor_mask = xor_mask;
}

/*
 *   allocate and load a page 
 */
const char *CVmImagePool::
   vmpbs_alloc_and_load_page(pool_ofs_t ofs, size_t /*page_size*/,
                             size_t load_size)
{
    CVmImagePool_pg *info;
    
    /* get the page information */
    info = get_page_info_ofs(ofs);
    
    /* seek to the correct location in the image file */
    seek_page_ofs(ofs);

    /* ask the underlying file to load the data */
    return fp_->alloc_and_read(load_size, info->xor_mask, load_size);
}

/*
 *   load a page into the given memory block
 */
void CVmImagePool::vmpbs_load_page(pool_ofs_t ofs, size_t /*page_size*/,
                                   size_t load_size, char *mem)
{
    CVmImagePool_pg *info;

    /* get the page information */
    info = get_page_info_ofs(ofs);

    /* seek to the correct location in the image file */
    seek_page_ofs(ofs);

    /* ask the underlying file to load the data */
    fp_->copy_data(mem, load_size);

    /* apply the XOR mask to the loaded data */
    apply_xor_mask(mem, load_size, info->xor_mask);
}

/* 
 *   free a page previously loaded 
 */
void CVmImagePool::vmpbs_free_page(const char *mem, pool_ofs_t /*ofs*/,
                                   size_t /*page_size*/)
{
    /* tell the file to free the memory */
    fp_->free_mem(mem);
}

/*
 *   Determine if the backing store pages are writable.  Let the
 *   underlying file decide. 
 */
int CVmImagePool::vmpbs_is_writable()
{
    /* 
     *   if the underlying file allows writing to its allocated pages,
     *   allow writing 
     */
    return fp_->allow_write_to_alloc();
}

/*
 *   seek to the image file data for the page at the given pool offset 
 */
void CVmImagePool::seek_page_ofs(pool_ofs_t ofs)
{
    ulong page_idx;
    CVmImagePool_pg *info;
    
    /* 
     *   get the page index - pages are all of a fixed length, so we can
     *   find the page index simply by dividing the offset by the page
     *   size 
     */
    page_idx = ofs / page_size_;

    /* make sure the page is valid */
    if (page_idx >= page_count_)
        err_throw(VMERR_LOAD_BAD_PAGE_IDX);

    /* make sure the page has been defined */
    info = get_page_info(page_idx);

    /* make sure this page's information has been loaded */
    if (info->seek_pos == 0)
        err_throw(VMERR_LOAD_UNDEF_PAGE);
    
    /* seek to this block's position in the file */
    fp_->seek(info->seek_pos);
}


/* ------------------------------------------------------------------------ */
/*
 *   Image file interface - external disk file implementation 
 */

/*
 *   Delete the image file loader.  This deletes all of the memory
 *   associated with loaded objects, so the image file loader must not be
 *   deleted until all of the loaded root-set objects are deleted. 
 */
CVmImageFileExt::~CVmImageFileExt()
{
    CVmImageFileExt_blk *cur;
    CVmImageFileExt_blk *nxt;
    
    /* run through the list of allocated blocks and delete each one */
    for (cur = mem_head_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next block, since we're deleting this one */
        nxt = cur->nxt_;

        /* delete this one */
        delete cur;
    }
}


/*
 *   copy data to the caller's buffer 
 */
void CVmImageFileExt::copy_data(char *buf, size_t len)
{
    /* read directly from the file into the caller's buffer */
    fp_->read_bytes(buf, len);
}

/*
 *   allocate memory for and read data 
 */
const char *CVmImageFileExt::alloc_and_read(size_t len, uchar xor_mask,
                                            ulong remaining_in_page)
{
    char *mem;
    
    /* allocate memory */
    mem = alloc_mem(len, remaining_in_page);
    if (mem == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* read the data */
    fp_->read_bytes(mem, len);

    /* if there's an XOR mask, apply it to each byte we read */
    CVmImagePool::apply_xor_mask(mem, len, xor_mask);

    /* return the memory block */
    return mem;
}

/*
 *   seek 
 */
void CVmImageFileExt::seek(long pos)
{
    /* seek to the given position in the underlying file */
    fp_->set_pos(pos);
}

/* 
 *   get current seek position 
 */
long CVmImageFileExt::get_seek() const
{
    /* get the position from the underlying file */
    return fp_->get_pos();
}

/*
 *   skip bytes 
 */
void CVmImageFileExt::skip_ahead(long len)
{
    /* seek ahead in the underlying file */
    fp_->set_pos_from_cur(len);
}

/*
 *   Allocate data for loading 
 */
char *CVmImageFileExt::alloc_mem(size_t siz, ulong remaining_in_page)
{
    /* 
     *   If the requested size is more than an eigth of our block size,
     *   give the new allocation its own block.  Our blocks are intended
     *   to reduce overhead for small blocks; large blocks not only won't
     *   create a lot of overhead as individual "malloc" allocations, but
     *   would probably leave our block underutilized by leaving a lot of
     *   it free, defeating the purpose. 
     */
    if (siz > VMIMAGE_EXT_BLK_SIZE / 8)
    {
        CVmImageFileExt_blk *new_block;

        /* it's a big request, so give it its own block */
        new_block = new CVmImageFileExt_blk(siz);

        /* 
         *   link it at the tail of the list (we don't want to suballocate
         *   anything more out of this, obviously, since we're going to
         *   consume the entire block, but we do want to keep it in our
         *   list so that we can track and eventually delete the memory) 
         */
        new_block->nxt_ = 0;
        new_block->prv_ = mem_tail_;

        /* set the forward pointer of the previous entry */
        if (mem_tail_ != 0)
            mem_tail_->nxt_ = new_block;
        else
            mem_head_ = new_block;

        /* it's the new tail */
        mem_tail_ = new_block;

        /* "suballocate" out of the new block */
        return mem_tail_->suballoc(siz);
    }
    else
    {
        /* 
         *   It's a small request, so suballocate it out of a block.
         *   First, make sure we have space in the current block; if not,
         *   allocate a new block. 
         */
        if (mem_head_ == 0 || siz > mem_head_->rem_)
        {
            CVmImageFileExt_blk *new_block;
            size_t new_block_size;
            
            /* 
             *   There's no space left in the current block - allocate a new
             *   block.  Leave space in the new block for further
             *   allocations, so we don't have to allocate lots of little
             *   blocks; however, as an upper bound, allocate only as much
             *   space as remains in the current page being read out of the
             *   image file, so that we don't leave a bunch of unused space
             *   after reading the last objects from the image.  
             */
            new_block_size = VMIMAGE_EXT_BLK_SIZE;
            if (new_block_size > remaining_in_page)
                new_block_size = (size_t)remaining_in_page;

            /* allocate the new block */
            new_block = new CVmImageFileExt_blk(new_block_size);

            /* link it in at the head of our list */
            new_block->prv_ = 0;
            new_block->nxt_ = mem_head_;

            /* set the back pointer of the next entry */
            if (mem_head_ != 0)
                mem_head_->prv_ = new_block;
            else
                mem_tail_ = new_block;

            /* it's the new head */
            mem_head_ = new_block;
        }

        /* suballocate it */
        return mem_head_->suballoc(siz);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Memory tracker for external file loader 
 */

CVmImageFileExt_blk::CVmImageFileExt_blk(size_t siz)
{
    /* allocate the associated memory block */
    block_ptr_ = (char *)t3malloc(siz);

    /* remember the amount of space remaining */
    rem_ = siz;

    /* the next byte free is the first byte */
    free_ptr_ = block_ptr_;

    /* nothing else is in our list yet */
    nxt_ = prv_ = 0;
}

CVmImageFileExt_blk::~CVmImageFileExt_blk()
{
    /* free our memory block */
    t3free(block_ptr_);
}

char *CVmImageFileExt_blk::suballoc(size_t siz)
{
    char *ret;
    
    /* if we don't have enough memory available, fail the request */
    if (rem_ < siz)
        return 0;

    /* our return value will be the current free pointer */
    ret = free_ptr_;

    /* move the free pointer past the allocated block */
    free_ptr_ += siz;

    /* deduct the amount we just allocated from the space available */
    rem_ -= siz;

    /* return the memory */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Image file interface - memory-mapped file implementation 
 */

/*
 *   copy data to the caller's buffer 
 */
void CVmImageFileMem::copy_data(char *buf, size_t len)
{
    /* if we're past the end of the file, throw an error */
    if (pos_ + len > len_)
        err_throw(VMERR_READ_PAST_IMG_END);

    /* copy data into the caller's buffer */
    memcpy(buf, mem_ + pos_, len);

    /* seek past the data */
    pos_ += len;
}

/*
 *   allocate memory for and read data 
 */
const char *CVmImageFileMem::alloc_and_read(size_t len, uchar xor_mask,
                                            ulong /*remaining_in_page*/)
{
    const char *ret;
    
    /* if we're past the end of the file, throw an error */
    if (pos_ + len > len_)
        err_throw(VMERR_READ_PAST_IMG_END);

    /* the data are already in memory - figure out where */
    ret = mem_ + pos_;

    /* seek past the data */
    pos_ += len;

    /* 
     *   we can't apply an xor mask to an in-memory page - if the mask is
     *   non-zero, it's an error 
     */
    if (xor_mask != 0)
        err_throw(VMERR_XOR_MASK_BAD_IN_MEM);

    /* return the pointer */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic stream implementation for an image file block 
 */

/* 
 *   read bytes from the stream 
 */
void CVmImageFileStream::read_bytes(char *buf, size_t len)
{
    /* if we don't have enough data to satisfy the request, it's an error */
    if (len > len_)
        err_throw(VMERR_IMAGE_BLOCK_TOO_SMALL);

    /* deduct the space we're reading from the remaining data size */
    len_ -= len;

    /* copy the data from the image file to the caller's buffer */
    fp_->copy_data(buf, len);
}

/*
 *   read bytes, allowing short reads without error 
 */
size_t CVmImageFileStream::read_nbytes(char *buf, size_t len)
{
    /* limit the read to the available size */
    if (len > len_)
        len = len_;

    /* deduct the space we're reading from the remaining data size */
    len_ -= len;

    /* copy the data from the image file to the caller's buffer */
    if (len != 0)
        fp_->copy_data(buf, len);

    /* return the length actually read */
    return len;
}

/*
 *   write bytes to the stream 
 */
void CVmImageFileStream::write_bytes(const char *, size_t)
{
    /* this is a read-only stream, so writing isn't allowed */
    err_throw(VMERR_WRITE_FILE);
}

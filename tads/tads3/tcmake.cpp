#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCMAKE.CPP,v 1.1 1999/07/11 00:46:53 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcmake.cpp - TADS 3 Compiler "Make" Engine
Function
  The TADS 3 compiler provides automatic dependency maintenance of
  derived files through this "make" engine.  The TADS 3 compilation
  process, when a program is constructed from several source files
  that are compiled separately, is as follows:

   1.  For each source file, create a symbol export file.
   2.  For each source file, load all other symbol export files, then
       create an object file from the source file.
   3.  Load all object files, and create the image file.
   4.  If not compiling for debugging, load the image file, run
       preinitialization, and write the updated image file.

  The dependency tree is as follows:

   - the ultimate target is the image file
   - the image file depends upon all of the object files
   - each object file depends upon all of the symbol export files,
     plus the single corresponding source file
   - each symbol export file depends upon the single corresponding
     source file

Notes
  
Modified
  07/09/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "os.h"
#include "t3std.h"
#include "tctok.h"
#include "resload.h"
#include "tcmain.h"
#include "tchost.h"
#include "tcglob.h"
#include "tcprs.h"
#include "tctarg.h"
#include "vmfile.h"
#include "tcmake.h"
#include "vmpreini.h"
#include "vmhostsi.h"
#include "tcvsn.h"
#include "vmmaincn.h"
#include "vmrunsym.h"
#include "vmbignum.h"
#include "tcunas.h"
#include "vmcrc.h"
#include "rcmain.h"


/* ------------------------------------------------------------------------ */
/*
 *   Host interface mapping from our tcmain host interface to the rcmain host
 *   interface.  
 */
class CRcHostIfcTcmake: public CRcHostIfc
{
public:
    CRcHostIfcTcmake(CTcHostIfc *ifc) { ifc_ = ifc; }

    /* 
     *   map the rcmain host interface methods to the equivalents in the
     *   tcmain host interface 
     */
    virtual void display_error(const char *msg)
        { ifc_->print_err("%s\n", msg); }
    virtual void display_status(const char *msg)
        { ifc_->print_step("%s\n", msg); }

private:
    /* our underlying tcmain host interface */
    CTcHostIfc *ifc_;
};

/* ------------------------------------------------------------------------ */
/*
 *   TADS 3 Compiler "Make" engine 
 */

/*
 *   initialize 
 */
CTcMake::CTcMake()
{
    /* no modules yet */
    mod_head_ = mod_tail_ = 0;

    /* no #include paths yet */
    inc_head_ = inc_tail_ = nonsys_inc_tail_ = 0;

    /* no source paths yet */
    src_head_ = src_tail_ = nonsys_src_tail_ = 0;

    /* no preprocessor symbols defined yet */
    def_head_ = def_tail_ = 0;

    /* assume non-verbose mode */
    verbose_ = FALSE;

    /* assume we won't show error numbers */
    show_err_numbers_ = FALSE;

    /* assume we'll show standard but not pedantic warnings */
    show_warnings_ = TRUE;
    pedantic_ = FALSE;
    warnings_as_errors_ = FALSE;

    /* we don't have a list of warnings to suppress yet */
    suppress_list_ = 0;
    suppress_cnt_ = 0;

    /* assume no debug information */
    debug_ = FALSE;

    /* 
     *   assume we're actually compiling, not just preprocessing, listing
     *   included files, or cleaning up derived files 
     */
    pp_only_ = FALSE;
    list_includes_mode_ = FALSE;
    clean_mode_ = FALSE;

    /* assume we're not in test reporting mode */
    test_report_mode_ = FALSE;

    /* assume we're not in status percentage mode */
    status_pct_mode_ = FALSE;

    /* assume we won't want quoted filenames */
    quoted_fname_mode_ = FALSE;

    /* assume we'll link the program after compiling */
    do_link_ = TRUE;

    /* assume we'll use default preinit mode based on debug mode */
    explicit_preinit_ = FALSE;

    /* set an arbitrary data pool XOR mask */
    data_xor_mask_ = 0xDF;

    /* no source character set has been specified yet */
    source_charset_ = 0;

    /* presume we won't be capturing strings */
    string_fp_ = 0;

    /* presume we won't be generating an assembly listing */
    assembly_listing_fp_ = 0;

    /* assume we won't generate sourceTextGroup properties */
    src_group_mode_ = FALSE;

    /* presume we won't create output directories */
    create_dirs_ = FALSE;

    /* initialize the BigNumber internal cache */
    CVmObjBigNum::init_cache();
}

/*
 *   delete 
 */
CTcMake::~CTcMake()
{
    /* delete the modules */
    while (mod_head_ != 0)
    {
        CTcMakeModule *nxt;

        /* remember the next module */
        nxt = mod_head_->get_next();

        /* delete this one */
        delete mod_head_;

        /* move on to the next one */
        mod_head_ = nxt;
    }

    /* delete the #include path list */
    while (inc_head_ != 0)
    {
        CTcMakePath *nxt;

        /* remember the next one */
        nxt = inc_head_->get_next();

        /* delete this one */
        delete inc_head_;

        /* move on to the next one */
        inc_head_ = nxt;
    }

    /* delete the source path list */
    while (src_head_ != 0)
    {
        CTcMakePath *nxt;

        /* remember the next one */
        nxt = src_head_->get_next();

        /* delete this one */
        delete src_head_;

        /* move on to the next one */
        src_head_ = nxt;
    }

    /* delete the preprocessor symbol list */
    while (def_head_ != 0)
    {
        CTcMakeDef *nxt;

        /* remember the next one */
        nxt = def_head_->get_next();

        /* delete this one */
        delete def_head_;

        /* move on to the next one */
        def_head_ = nxt;
    }

    /* delete the source character set name */
    lib_free_str(source_charset_);

    /* delete the BigNumber cache */
    CVmObjBigNum::term_cache();
}

/*
 *   Add a preprocessor symbol definition 
 */
void CTcMake::def_pp_sym(const char *sym, const char *expan)
{
    /* if there's no expansion, use a default value of "1" */
    if (expan == 0)
        expan = "1";

    /* add the symbol to our list */
    add_pp_def(sym, expan, TRUE);
}

/*
 *   Undefine a preprocessor symbol 
 */
void CTcMake::undef_pp_sym(const char *sym)
{
    /* add the symbol to our list */
    add_pp_def(sym, 0, FALSE);
}

/*
 *   Add a preprocessor symbol define/undefine to our list 
 */
void CTcMake::add_pp_def(const char *sym, const char *expan, int is_def)
{
    CTcMakeDef *def;

    /* create the new list entry */
    def = new CTcMakeDef(sym, expan, is_def);

    /* link it in at the end of our list */
    if (def_tail_ != 0)
        def_tail_->set_next(def);
    else
        def_head_ = def;
    def_tail_ = def;
}

/*
 *   look up a preprocess symbol definition 
 */
const char *CTcMake::look_up_pp_sym(const char *sym, size_t len)
{
    CTcMakeDef *def;
    const char *found_val;

    /* iterate over our symbols to find this definition */
    for (found_val = 0, def = def_head_ ; def != 0 ; def = def->get_next())
    {
        /* if this one matches, note it */
        if (strlen(def->get_sym()) == len
            && memcmp(def->get_sym(), sym, len) == 0)
        {
            /* 
             *   it's a match - if this is a definition, remember it as the
             *   latest definition; if it's an undefinition, forget any
             *   previous definition we've noted 
             */
            found_val = (def->is_def() ? def->get_expan() : 0);
        }
    }

    /* return the definition we found, if any */
    return found_val;
}

/*
 *   Add a module at the tail of the list
 */
void CTcMake::add_module(CTcMakeModule *mod)
{
    /* add it at the end of our list */
    if (mod_tail_ != 0)
        mod_tail_->set_next(mod);
    else
        mod_head_ = mod;
    mod_tail_ = mod;
    mod->set_next(0);
}

/*
 *   Add a module at the head of the list
 */
void CTcMake::add_module_first(CTcMakeModule *mod)
{
    /* add it at the end of our list */
    mod->set_next(mod_head_);
    mod_head_ = mod;

    /* if it's the first thing in the list, it's the tail as well */
    if (mod_tail_ == 0)
        mod_tail_ = mod;
}

/*
 *   Add a module by filename 
 */
CTcMakeModule *CTcMake::add_module(const char *src_name,
                                   const char *sym_name,
                                   const char *obj_name,
                                   int first)
{
    /* create a module object */
    CTcMakeModule *mod = new CTcMakeModule();

    /* set the module name */
    mod->set_module_name(src_name);

    /* set the symbol file name if specified */
    if (sym_name != 0)
        mod->set_symbol_name(sym_name);

    /* set the object file name if specified */
    if (obj_name != 0)
        mod->set_object_name(obj_name);

    /* add it to the module list */
    if (first)
        add_module_first(mod);
    else
        add_module(mod);

    /* return the new module */
    return mod;
}

/*
 *   Add a #include path entry
 */
void CTcMake::add_include_path(const textchar_t *path)
{
    CTcMakePath *inc;
    
    /* create the entry */
    inc = new CTcMakePath(path);
    
    /* 
     *   add it at the end of our non-system list - note that system files
     *   might follow this point, so we need to insert the item into the
     *   list after the last existing non-system item 
     */
    if (nonsys_inc_tail_ == 0)
    {
        /* no non-system path yet - insert at the head of the list */
        inc->set_next(inc_head_);
        inc_head_ = inc;
    }
    else
    {
        /* insert after the last non-system item */
        inc->set_next(nonsys_inc_tail_->get_next());
        nonsys_inc_tail_->set_next(inc);
    }

    /* this is the last non-system item */
    nonsys_inc_tail_ = inc;

    /* if this is the last item overall, advance the list tail */
    if (inc->get_next() == 0)
        inc_tail_ = inc;
}

/*
 *   Add a #include path entry if it's not already in our list 
 */
void CTcMake::maybe_add_include_path(const textchar_t *path)
{
    CTcMakePath *inc;

    /* 
     *   scan our existing list of include paths for a match to this path -
     *   if the path is already in our list, we don't want to bother adding
     *   it again 
     */
    for (inc = inc_head_ ; inc != 0 ; inc = inc->get_next())
    {
        /* 
         *   If this existing path matches the new path, we don't need to
         *   bother adding the new path.  Note that we just do a
         *   simple-minded string comparison of the paths, which might not
         *   always detect equivalent paths (this strategy can be fooled by
         *   hard/soft links, differing case in a case-insensitive file
         *   system, and relative vs. absolute notation, among other
         *   things); this isn't too important, though, because the worst
         *   that will happen is that we'll lose a little efficiency by
         *   searching the same directory twice.  
         */
        if (strcmp(inc->get_path(), path) == 0)
            return;
    }

    /* didn't find an exact match for the path, so add it */
    add_include_path(path);
}

/*
 *   Add a system #include path entry 
 */
CTcMakePath *CTcMake::add_sys_include_path(const textchar_t *path)
{
    CTcMakePath *inc;

    /* create the entry */
    inc = new CTcMakePath(path);

    /* add it at the end of our list */
    if (inc_tail_ != 0)
        inc_tail_->set_next(inc);
    else
        inc_head_ = inc;
    inc_tail_ = inc;
    inc->set_next(0);

    /* return the new path object */
    return inc;
}

/*
 *   Add a source path entry
 */
void CTcMake::add_source_path(const textchar_t *path)
{
    CTcMakePath *src;

    /* create the entry */
    src = new CTcMakePath(path);
    
    /* 
     *   add it at the end of our non-system list - note that system files
     *   might follow this point, so we need to insert the item into the
     *   list after the last existing non-system item 
     */
    if (nonsys_src_tail_ == 0)
    {
        /* no non-system path yet - insert at the head of the list */
        src->set_next(src_head_);
        src_head_ = src;
    }
    else
    {
        /* insert after the last non-system item */
        src->set_next(nonsys_src_tail_->get_next());
        nonsys_src_tail_->set_next(src);
    }

    /* this is the last non-system item */
    nonsys_src_tail_ = src;

    /* if this is the last item overall, advance the list tail */
    if (src->get_next() == 0)
        src_tail_ = src;
}

/*
 *   Add a system source path entry 
 */
CTcMakePath *CTcMake::add_sys_source_path(const textchar_t *path)
{
    CTcMakePath *src;

    /* create the entry */
    src = new CTcMakePath(path);

    /* add it at the end of our list */
    if (src_tail_ != 0)
        src_tail_->set_next(src);
    else
        src_head_ = src;
    src_tail_ = src;
    src->set_next(0);

    /* return the object */
    return src;
}

/*
 *   Check a new module we're adding to make sure its name doesn't collide
 *   with an existing module.  Each module is required to have a unique root
 *   filename, so flag an error if the module name matches one that's already
 *   in the list.  
 */
void CTcMake::check_module_collision(CTcMakeModule *new_mod)
{
    const char *new_root;
    CTcMakeModule *cur_mod;

    /* get the root name of this module */
    new_root = os_get_root_name((char *)new_mod->get_source_name());

    /* 
     *   Scan the list of modules for a collision.  Stop when we get to this
     *   module, because we want to report the collision at the later module,
     *   so that we report basically in the order of adding modules to the
     *   build.  
     */
    for (cur_mod = mod_head_ ; cur_mod != 0 && cur_mod != new_mod ;
         cur_mod = cur_mod->get_next())
    {
        const char *cur_root;

        /* 
         *   Compare this module's root name.  Since some file systems are
         *   insensitive to case, ignore case ourselves; this is more
         *   restrictive than is actually necessary on some systems, but will
         *   flag the potential for problems in case the project is later
         *   moved to a different operating system.  
         */
        cur_root = os_get_root_name((char *)cur_mod->get_source_name());
        if (stricmp(cur_root, new_root) == 0)
        {
            char new_name_buf[OSFNMAX*2 + 10];

            /* if the new module is from a library, so note */
            if (new_mod->get_source_type() == TCMOD_SOURCE_NORMAL)
            {
                /* it's not from a library - just show the module name */
                sprintf(new_name_buf, "\"%s\"", new_mod->get_source_name());
            }
            else
            {
                /* it is from a library - show the library name as well */
                sprintf(new_name_buf,
                        tcerr_get_msg(TCERR_SOURCE_FROM_LIB, FALSE),
                        new_mod->get_source_name(), new_mod->get_from_lib());
            }

            /*
             *   Flag a warning.  If the older module is from a library, use
             *   a separate version of the message, so that we can mention
             *   the library it's from.  
             */
            if (cur_mod->get_source_type() == TCMOD_SOURCE_NORMAL)
            {
                /* the older module isn't from a library */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_MODULE_NAME_COLLISION,
                                    new_name_buf);
            }
            else
            {
                /* the older module is in a library, so mention the source */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_MODULE_NAME_COLLISION_WITH_LIB,
                                    new_name_buf, cur_mod->get_from_lib());
            }

            /* 
             *   there's no need to find yet another module with the same
             *   name - it's good enough to flag the error once 
             */
            break;
        }
    }
}

/*
 *   Check all modules for name collisions.
 */
void CTcMake::check_all_module_collisions(CTcHostIfc *hostifc,
                                          CResLoader *res_loader,
                                          int *err_cnt, int *warn_cnt)
{
    err_try
    {
        CTcMakeModule *cur;
        
        /* initialize the compiler */
        CTcMain::init(hostifc, res_loader, source_charset_);

        /* set options */
        set_compiler_options();

        /* scan all of the modules in our list */
        for (cur = mod_head_ ; cur != 0 ; cur = cur->get_next())
            check_module_collision(cur);
    }
    err_finally
    {
        /* update the caller's error and warning counters */
        *err_cnt += G_tcmain->get_error_count();
        *warn_cnt += G_tcmain->get_warning_count();

        /* terminate the compiler */
        CTcMain::terminate();
    }
    err_end;
}

/*
 *   Build the program 
 */
void CTcMake::build(CTcHostIfc *hostifc, int *errcnt, int *warncnt,
                    int force_build, int force_link,
                    CRcResList *res_list, const char *argv0)
{
    CResLoader *res_loader;
    int fatal_error_count = 0;
    CTcMakeModule *mod;
    os_file_stat_t imgstat;
    int build_image;
    int run_preinit;
    char exe_path[OSFNMAX];
    CVmRuntimeSymbols *volatile runtime_symtab = 0;
    CVmRuntimeSymbols *volatile runtime_macros = 0;
    int seqno;
    
    /* no warnings or errors so far */
    *errcnt = 0;
    *warncnt = 0;

    /* create a resource loader */
    os_get_special_path(exe_path, sizeof(exe_path), argv0, OS_GSP_T3_RES);
    res_loader = new CResLoader(exe_path);

    /* set the exectuable filename in the loader, if available */
    if (os_get_exe_filename(exe_path, sizeof(exe_path), argv0))
        res_loader->set_exe_filename(exe_path);

    /* 
     *   if we don't have an explicit preinit setting, turn preinit on if
     *   and only if we're not debugging 
     */
    if (!explicit_preinit_)
        run_preinit = !debug_;
    else
        run_preinit = preinit_;

    err_try
    {
        textchar_t qu_buf[OSFNMAX*2 + 2], qu_buf_out[OSFNMAX*2 + 2];
        int step_cnt, step_cur = 0;
        char img_tool_data[4];
        CVmCRC32 mod_crc;

        /* check for module name collisions - give up if we find an error */
        check_all_module_collisions(hostifc, res_loader, errcnt, warncnt);
        if (*errcnt != 0 || (warnings_as_errors_ && *warncnt != 0))
            goto done;

        /*
         *   If we're merely running the preprocessor to generate the
         *   preprocessed source or to generate a list of #include files, go
         *   through each non-excluded module and preprocess it.  
         */
        if (pp_only_ || list_includes_mode_)
        {
            /* preprocess each module */
            for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
            {
                textchar_t srcfile[OSFNMAX];

                /* if it's an excluded module, skip it */
                if (mod->is_excluded())
                    continue;

                /* derive the source file name */
                get_srcfile(srcfile, mod);

                /* preprocess it */
                preprocess_source(hostifc, res_loader, srcfile, mod,
                                  errcnt, warncnt);
            }

            /* that's all we want to do */
            goto done;
        }

        /*
         *   If we're in "clean" mode, do nothing except deleting the
         *   derived files implied by the command line.  
         */
        if (clean_mode_)
        {
            /* clean up each module's derived files */
            for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
            {
                textchar_t fname[OSFNMAX];

                /* skip any excluded modules */
                if (mod->is_excluded())
                    continue;

                /* delete the module's derived symbol file */
                get_symfile(fname, mod);
                if (!osfacc(fname))
                {
                    /* report our progress */
                    hostifc->print_step("deleting symbol file %s\n",
                                        get_step_fname(qu_buf, fname));

                    /* delete the file */
                    if (osfdel(fname))
                        hostifc->print_err(
                            "error: cannot delete %s\n", fname);
                }

                /* delete the module's derived object file */
                get_objfile(fname, mod);
                if (!osfacc(fname))
                {
                    /* report our progress */
                    hostifc->print_step("deleting object file %s\n",
                                        get_step_fname(qu_buf, fname));

                    /* delete the file */
                    if (osfdel(fname))
                        hostifc->print_err(
                            "error: cannot delete %s\n", fname);
                }
            }

            /* delete the image file */
            if (!osfacc(image_fname_.get()))
            {
                /* report our progress */
                hostifc->print_step(
                    "deleting image file %s\n",
                    get_step_fname(qu_buf, image_fname_.get()));

                /* delete the file */
                if (osfdel(image_fname_.get()))
                    hostifc->print_err("error: cannot delete %s\n",
                                       image_fname_.get());
            }

            /* that's all we want to do */
            goto done;
        }

        /*
         *   If desired, create the output directories.
         */
        if (create_dirs_)
        {
            /* initialize the error subsystem for this work */
            CTcMain::init(hostifc, res_loader, source_charset_);
            
            /* create the symbol file directory */
            if (symdir_.is_set())
                create_dir(hostifc, symdir_.get(), FALSE,
                           &fatal_error_count);

            /* create the object file directory */
            if (objdir_.is_set())
                create_dir(hostifc, objdir_.get(),
                           FALSE, &fatal_error_count);

            /* create the image file directory */
            if (image_fname_.is_set())
                create_dir(hostifc, image_fname_.get(), TRUE,
                           &fatal_error_count);

            /* done with the error subsystem for now */
            CTcMain::terminate();

            /* if any of those failed, give up */
            if (fatal_error_count != 0)
                goto done;
        }

        /*
         *   First, run through the files and determine what work we'll need
         *   to do. 
         */
        for (mod = mod_head_, step_cnt = 0 ; mod != 0 ; mod = mod->get_next())
        {
            textchar_t srcfile[OSFNMAX];
            textchar_t symfile[OSFNMAX];
            textchar_t objfile[OSFNMAX];
            os_file_stat_t srcstat, symstat, objstat;
            int sym_recomp;
            int obj_recomp;

            /* if this module is excluded, skip it */
            if (mod->is_excluded())
                continue;

            /* derive the source, symbol, and object file names */
            get_srcfile(srcfile, mod);
            get_symfile(symfile, mod);
            get_objfile(objfile, mod);

            /* 
             *   add this module's source file name into the module-list
             *   checksum - we use this to determine if there have been any
             *   changes to the module list since the last time we linked the
             *   image file, so we can tell if we need to re-link 
             */
            mod_crc.scan_bytes(srcfile, get_strlen(srcfile));

            /* presume we will not need to recompile this file */
            sym_recomp = FALSE;
            obj_recomp = FALSE;

            /*
             *   Check to see if we need to rebuild this symbol file.
             *   
             *   Get the file times for the source and symbol files.  If
             *   either file doesn't exist, or the source modification time
             *   is later than the symbol file's modification time, then
             *   we'll need to build the symbol file.  
             */
            if (force_build
                || !os_file_stat(srcfile, TRUE, &srcstat)
                || !os_file_stat(symfile, TRUE, &symstat)
                || srcstat.mod_time > symstat.mod_time)
            {
                /* this symbol file requires recompilation */
                sym_recomp = TRUE;
            }
            else
            {
                osfildef *fp;
                textchar_t sym_fname[OSFNMAX];

                /*
                 *   On the basis of the file system timestamps alone, the
                 *   symbol file doesn't require rebuilding.  Check to see if
                 *   the configuration data stored in the symbol file is
                 *   different from our current configuration - if so, we'll
                 *   need to rebuild the symbol file.  
                 */
                get_symfile(sym_fname, mod);
                fp = osfoprb(sym_fname, OSFTT3SYM);
                if (fp == 0)
                {
                    /* there's no symbol file - definitely recompile */
                    sym_recomp = TRUE;
                }
                else
                {

                    /* set up the symbol file descriptor */
                    CVmFile *sym_file = new CVmFile();
                    sym_file->set_file(fp, 0);

                    /* seek to the start of our config data */
                    ulong siz =
                        CTcParser::seek_sym_file_build_config_info(sym_file);

                    /* compare the configuration */
                    int same_config;
                    err_try
                    {
                        /* compare the configuration */
                        same_config =
                            (siz != 0
                             && compare_build_config_from_sym_file(
                                 sym_fname, sym_file));
                    }
                    err_catch_disc
                    {
                        /* 
                         *   the configuration information is invalid -
                         *   assume that recompilation is required 
                         */
                        same_config = FALSE;
                    }
                    err_end;

                    /* 
                     *   if we don't have the same configuration, we must
                     *   recompile this source file 
                     */
                    if (!same_config)
                    {
                        /* note that recompilation is necessary */
                        sym_recomp = TRUE;
                    }

                    /* delete the CVmFile object and close the file */
                    delete sym_file;
                }
            }

            /* note whether or not we have to build the symbol file */
            mod->set_needs_sym_recompile(sym_recomp);

            /* check to see if we need to rebuild it */
            if (sym_recomp)
            {
                /* we do - count the step */
                ++step_cnt;

                /* 
                 *   delete the current symbol file - if our build attempt
                 *   fails before we get here, this will ensure that we'll
                 *   pick up this dependency again on subsequent builds 
                 */
                osfdel(symfile);
            }

            /*
             *   Check to see if we need to rebuild this object file.
             *   
             *   Check the file times - if the object file doesn't exist, or
             *   the source modification time is later than the object file's
             *   modification time, we need to build the object file.
             *   
             *   In addition, if we had to compile the symbol file, it
             *   doesn't matter whether the object file is up-to-date with
             *   the source file - always rebuild the object file if we had
             *   to rebuild the symbol file.
             *   
             *   Finally, we need to recompile the object file if the symbol
             *   file has a more recent timestamp than the object file.  This
             *   can happen when a previous build got as far as our symbol
             *   file before encountering an error, in which case the object
             *   file could appear up-to-date.  
             */
            if (force_build
                || sym_recomp
                || !os_file_stat(srcfile, TRUE, &srcstat)
                || !os_file_stat(objfile, TRUE, &objstat)
                || !os_file_stat(symfile, TRUE, &symstat)
                || srcstat.mod_time > objstat.mod_time
                || symstat.mod_time > objstat.mod_time)
            {
                /* we do have to rebuild the object file */
                obj_recomp = TRUE;
            }

            /* note whether or not we have to build the object file */
            mod->set_needs_obj_recompile(obj_recomp);

            /* check to see if we need to rebuild the object file */
            if (obj_recomp)
            {
                /* we do - count the step */
                ++step_cnt;

                /* 
                 *   delete the object file, to ensure we eventually compile
                 *   it even if this attempt fails 
                 */
                osfdel(objfile);
            }
        }

        /* 
         *   We've scanned all of the modules now, so we have the checksum
         *   value for the source file list.  Store this in our special
         *   reserved tool-data field in the image file signature, so that we
         *   can check on the next build to see if the module list has
         *   changed. 
         */
        oswp4(img_tool_data, mod_crc.get_crc_val());

        /*
         *   Determine if we need to build the image file.  If any of the
         *   existing object files are newer than the image file, we need to
         *   build the image.  If we're going to build any object files,
         *   they'll definitely be newer, because they don't even exist yet.
         *   Build the image regardless of dates if we're forcing a full
         *   build or forcing a link.
         *   
         *   First, check to see if the user even wants us to perform linking
         *   - if not, don't build the image file.  Next, check to see if we
         *   even need to look at at object file; if the image file doesn't
         *   exist, or we're forcing a full build, we don't need to bother
         *   checking timestamps.  
         */
        if (!do_link_)
        {
            /* the user specifically wants to skip linking the image file */
            build_image = FALSE;
        }
        else if (force_build || force_link
                 || !os_file_stat(image_fname_.get(), TRUE, &imgstat))
        {
            /* we definitely need to build the image file */
            build_image = TRUE;
        }
        else
        {
            char sig_buf[45];
            osfildef *img_fp;

            /* presume we won't need to rebuild the image file */
            build_image = FALSE;
            
            /* 
             *   Read the "tool data" bytes stored in the image file.  We
             *   store a CRC-32 checksum of the names of all of the source
             *   files in this field in the image file, so that we can tell
             *   if the module list has changed since the last time we
             *   linked.  If the module list has changed, we need to rebuild
             *   the image file. 
             */
            if ((img_fp = osfoprb(image_fname_.get(), OSFTT3IMG)) == 0
                || osfrb(img_fp, sig_buf, 45))
            {
                /* couldn't open/read the file - definitely rebuild */
                build_image = TRUE;
            }
            else
            {
                unsigned long old_crc;
                
                /* read the CRC value from the image file */
                old_crc = t3rp4u(sig_buf + 41);

                /* rebuild if it doesn't match the current signature */
                if (old_crc != mod_crc.get_crc_val())
                    build_image = TRUE;
            }

            /* close the file if we opened it */
            if (img_fp != 0)
                osfcls(img_fp);

            /* if we didn't just decide to rebuild, check module dates */
            if (!build_image)
            {
                /* scan each object module to see if any are more recent */
                for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
                {
                    textchar_t objfile[OSFNMAX];
                    os_file_stat_t objstat;
                    
                    /* if this module is excluded, skip it */
                    if (mod->is_excluded())
                        continue;
                    
                    /* 
                     *   if this module is being recompiled, it'll definitely
                     *   be newer than the image file, since we haven't even
                     *   started building the new object file yet 
                     */
                    if (mod->get_needs_obj_recompile())
                    {
                        /* we need to build the image */
                        build_image = TRUE;
                        
                        /* no need to look any further */
                        break;
                    }
                    
                    /* derive the object file name */
                    get_objfile(objfile, mod);
                    
                    /* if it's more recent, we need to build the image */
                    if (!os_file_stat(objfile, TRUE, &objstat)
                        || objstat.mod_time > imgstat.mod_time)
                    {
                        /* we need to build the image */
                        build_image = TRUE;

                        /* no need to continue scanning object files */
                        break;
                    }
                }
            }

            /* 
             *   if we're still not building the image file, check to see if
             *   any resource files are newer - if so, we'll need to relink
             *   to get a clean slate for refreshing the resources 
             */
            if  (!build_image && res_list != 0 && res_list->get_count() != 0)
            {
                /* scan each resource file */
                for (CRcResEntry *r = res_list->get_head() ; r != 0 ;
                     r = r->get_next())
                {
                    /* check this timestamp */
                    os_file_stat_t resstat;
                    if (!os_file_stat(r->get_fname(), TRUE, &resstat)
                        || resstat.mod_time > imgstat.mod_time)
                    {
                        /* we need to rebuild the image */
                        build_image = TRUE;
                        break;
                    }
                }
            }
        }

        /* if we have to link the image file, count the step */
        if (build_image)
        {
            /* count the link step */
            ++step_cnt;

            /* count the preinit step if necessary */
            if (run_preinit)
                ++step_cnt;

            /* count the resource bundler step if necessary */
            if (res_list != 0 && res_list->get_count() != 0)
                ++step_cnt;
        }
        
        /* display the initial estimate of the number of build steps */
        if (status_pct_mode_)
            hostifc->print_step("%%PCT:0/%d\n", step_cnt);
        if (step_cnt != 0)
            hostifc->print_step("Files to build: %d\n", step_cnt);

        /* assign sequence numbers to the modules */
        for (mod = mod_head_, seqno = 1 ; mod != 0 ; mod = mod->get_next())
            mod->set_seqno(seqno++);

        /*
         *   Build the symbol files.  Go through our list of source files.
         *   For each source file that is more recent than its symbol file,
         *   or for which no symbol file exists, build the symbol file.  
         */
        for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
        {
            /* recompile if necessary */
            if (!mod->is_excluded() && mod->get_needs_sym_recompile())
            {
                textchar_t srcfile[OSFNMAX];
                textchar_t symfile[OSFNMAX];

                /* derive the source and symbol file names */
                get_srcfile(srcfile, mod);
                get_symfile(symfile, mod);

                /* display what we're doing */
                hostifc->print_step("symbol_export %s -> %s\n",
                                    get_step_fname(qu_buf, srcfile),
                                    get_step_fname(qu_buf_out, symfile));

                /* add the percentage display, if desired */
                if (status_pct_mode_)
                    hostifc->print_step("%%PCT:%d/%d\n", step_cur, step_cnt);
                ++step_cur;
                
                /* we need to build the symbol file */
                build_symbol_file(hostifc, res_loader, srcfile, symfile,
                                  mod, errcnt, warncnt);

                /* if any errors occurred, stop now */
                if (*errcnt != 0 || (warnings_as_errors_ && *warncnt != 0))
                    goto done;
            }
        }
        
        /*
         *   Build object files.  Go through our list of source files.
         *   For each source file that is more recent than its object
         *   file, or for which no object file exists, build the object
         *   file.  
         */
        for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
        {
            /* if this module is excluded, skip it */
            if (!mod->is_excluded() && mod->get_needs_obj_recompile())
            {
                textchar_t srcfile[OSFNMAX];
                textchar_t objfile[OSFNMAX];

                /* derive the source and object file names */
                get_srcfile(srcfile, mod);
                get_objfile(objfile, mod);
            
                /* display what we're doing */
                hostifc->print_step("compile %s -> %s\n",
                                    get_step_fname(qu_buf, srcfile),
                                    get_step_fname(qu_buf_out, objfile));
                
                /* add the percentage display, if desired */
                if (status_pct_mode_)
                    hostifc->print_step("%%PCT:%d/%d\n", step_cur, step_cnt);
                ++step_cur;

                /* we need to build the object file */
                build_object_file(hostifc, res_loader, srcfile, objfile,
                                  mod, errcnt, warncnt);
                
                /* if any errors occurred, stop now */
                if (*errcnt != 0 || (warnings_as_errors_ && *warncnt != 0))
                    goto done;
            }
        }
        
        /* if we're building the image file, do so now */
        if (build_image)
        {
            const char *link_output;
            char inter_file[OSFNMAX + 10];

            /* 
             *   if we're running preinit, link to an intermediate file,
             *   so that we can wait to store the final preinitialized
             *   version in the actual image file; otherwise, link
             *   directly to the final image filename 
             */
            if (run_preinit)
            {
                /* we'll run preinit - link to an intermediate file */
                lib_strcpy(inter_file, OSFNMAX, image_fname_.get());
                os_remext(inter_file);
                os_addext(inter_file, "t3p");

                /* use the intermediate file as the linker output */
                link_output = inter_file;

                /* 
                 *   we're running preinit, so we need to build a global
                 *   symbol table and a macro table to pass to preinit 
                 */
                runtime_symtab = new CVmRuntimeSymbols();
                runtime_macros = new CVmRuntimeSymbols();
            }
            else
            {
                /* no preinit - link directly to the image filename */
                link_output = image_fname_.get();
            }
            
            /* display what we're doing */
            hostifc->print_step("link -> %s\n",
                                get_step_fname(qu_buf, link_output));

            /* add the percentage display, if desired */
            if (status_pct_mode_)
                hostifc->print_step("%%PCT:%d/%d\n", step_cur, step_cnt);
            ++step_cur;

            /* build it */
            build_image_file(hostifc, res_loader, link_output,
                             errcnt, warncnt, runtime_symtab, runtime_macros,
                             img_tool_data);

            /* if an error occurred, don't bother with preinit */
            if (*errcnt != 0 || (warnings_as_errors_ && *warncnt != 0))
                goto done;

            /* run preinit if desired */
            if (run_preinit)
            {
                CVmFile *file_in;
                CVmFile *file_out;
                osfildef *fp_in;
                osfildef *fp_out;
                CVmHostIfc *vmhostifc;
                CVmMainClientConsole clientifc;
                
                /* add the percentage display, if desired */
                if (status_pct_mode_)
                    hostifc->print_step("%%PCT:%d/%d\n", step_cur, step_cnt);
                ++step_cur;

                /* note what we're doing */
                hostifc->print_step(
                    "preinit -> %s\n",
                    get_step_fname(qu_buf, image_fname_.get()));

                /* open the linker output file for reading */
                if ((fp_in = osfoprb(link_output, OSFTT3IMG)) == 0)
                {
                    /* can't open the input file - throw an error */
                    err_throw_a(TCERR_MAKE_CANNOT_OPEN_IMG, 1,
                                ERR_TYPE_CHAR, link_output);
                }

                /* open the final image file for writing */
                if ((fp_out = osfopwb(image_fname_.get(), OSFTT3IMG)) == 0)
                {
                    /* close the input file */
                    osfcls(fp_in);

                    /* throw an error */
                    err_throw_a(TCERR_MAKE_CANNOT_CREATE_IMG, 1,
                                ERR_TYPE_CHAR, image_fname_.get());
                }

                /* set up the input file object */
                file_in = new CVmFile();
                file_in->set_file(fp_in, 0);

                /* set up the output file object */
                file_out = new CVmFile();
                file_out->set_file(fp_out, 0);

                /* create a stdio host interface */
                vmhostifc = new CVmHostIfcStdio(argv0);

                /* catch errors that occur during preinit */
                err_try
                {
                    /* run preinit to build the image file */
                    vm_run_preinit(file_in, link_output,
                                   file_out, vmhostifc, &clientifc, 0, 0,
                                   runtime_symtab, runtime_macros);
                }
                err_finally
                {
                    /* delete our VM host interface */
                    delete vmhostifc;

                    /* close and delete our files */
                    delete file_in;
                    delete file_out;
                }
                err_end;

                /* delete the intermediate file */
                osfdel(link_output);
            }

            /* 
             *   if we have any resources, add them to the image (or just add
             *   links to them, if compiling for debugging) 
             */
            if (res_list != 0 && res_list->get_count() != 0)
            {
                CRcHostIfcTcmake rc_hostifc(hostifc);

                /* add the percentage display, if desired */
                if (status_pct_mode_)
                    hostifc->print_step("%%PCT:%d/%d\n", step_cur, step_cnt);
                ++step_cur;

                /* mention what we're doing */
                hostifc->print_step(
                    "add_resource%ss -> %s\n",
                    debug_ ? " link" : "",
                    get_step_fname(qu_buf, image_fname_.get()));

                /* 
                 *   Add the resources.  If we're in debug mode, add them in
                 *   "link mode," meaning that we merely add links from the
                 *   resources to the equivalent local filenames. 
                 */
                if (CResCompMain::add_resources(
                    image_fname_.get(), res_list, &rc_hostifc,
                    FALSE, OSFTT3IMG, debug_))
                {
                    /* failed - delete the image file */
                    osfdel(image_fname_.get());
                }
            }
        }

    done: ;
    }
    err_catch(exc)
    {
        /* 
         *   if it's not a general internal or fatal error, log it; don't
         *   log general errors, since these will have been logged as
         *   specific internal errors before being thrown 
         */
        if (exc->get_error_code() != TCERR_INTERNAL_ERROR
            && exc->get_error_code() != TCERR_FATAL_ERROR)
        {
            /* make sure the host interface is set up */
            G_hostifc = hostifc;

            /* show the error */
            CTcMain::S_log_error(
                0, 0, errcnt, warncnt,
                (verbose_ ? TCMAIN_ERR_VERBOSE : 0)
                | (show_err_numbers_ ? TCMAIN_ERR_NUMBERS : 0)
                | (show_warnings_ ? TCMAIN_ERR_WARNINGS : 0)
                | (pedantic_ ? TCMAIN_ERR_PEDANTIC : 0)
                | (test_report_mode_ ? TCMAIN_ERR_TESTMODE : 0)
                | (quoted_fname_mode_ ? TCMAIN_ERR_FNAME_QU : 0),
                suppress_list_, suppress_cnt_, TC_SEV_FATAL, exc);
        }

        /* count the fatal error */
        ++fatal_error_count;
    }
    err_end;

    /* if we created a runtime symbol table, delete it */
    if (runtime_symtab != 0)
        delete runtime_symtab;

    /* likewise the macro table */
    if (runtime_macros != 0)
        delete runtime_macros;

    /* done with the resource loader */
    delete res_loader;

    /* if any fatal errors occurred, include them in the error count */
    *errcnt += fatal_error_count;
}

/*
 *   Create a directory 
 */
void CTcMake::create_dir(CTcHostIfc *hostifc,
                         const char *path, int is_file, int *errcnt)
{
    char dir[OSFNMAX];

    /*
     *   If the path includes a filename portion, remove it to get the parent
     *   directory path.
     */
    if (is_file)
    {
        /* extract the directory path portion */
        os_get_path_name(dir, sizeof(dir), path);
        path = dir;

        /* don't do anything if there's no path */
        if (strlen(dir) == 0)
            return;
    }

    /* if the directory doesn't already exist, create it */
    if (osfacc(path))
    {
        /* mention it */
        hostifc->print_step("creating output directory %s\n", path);
        
        /* try creating the folder */
        if (!os_mkdir(path, TRUE))
        {
            /* failed - log the error */
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_CANNOT_CREATE_DIR, path);
            
            /* count it */
            *errcnt += 1;
        }
    }
}


/*
 *   Build the version of a filename to show in a progress report. 
 */
const char *CTcMake::get_step_fname(char *buf, const char *fname)
{
    /* 
     *   If we're in test-report mode, show only the root name.  We don't
     *   want to show any paths in this mode, because we want our test output
     *   to be independent of the local directory structure; this ensures
     *   that tests can be run on any machine and then diff'ed against
     *   portable reference logs.  
     */
    if (test_report_mode_)
        fname = os_get_root_name((char *)fname);

    /* if we want quoted filenames, quote the filename */
    if (quoted_fname_mode_)
    {
        const char *src;
        char *dst;

        /* add quotes around the filename, and stutter any quotes within */
        for (src = fname, dst = buf + 1, buf[0] = '"' ; *src != '\0' ; )
        {
            /* if this is a quote, stutter it */
            if (*src == '"')
                *dst++ = '"';

            /* add this character */
            *dst++ = *src++;
        }

        /* add the close quote and null terminator */
        *dst++ = '"';
        *dst = '\0';

        /* use the copy in the buffer */
        fname = buf;
    }

    /* return the final filename */
    return fname;
}

/* 
 *   preprocess a source file to standard output
 */
void CTcMake::preprocess_source(CTcHostIfc *hostifc,
                                CResLoader *res_loader,
                                const textchar_t *src_fname,
                                CTcMakeModule *src_mod,
                                int *error_count, int *warning_count)
{
    CTcTokFileDesc *desc;
    long linenum;

    err_try
    {
        int err;

        /* initialize the compiler */
        CTcMain::init(hostifc, res_loader, source_charset_);

        /* set options */
        set_compiler_options();

        /* set up the tokenizer with the main input file */
        err = G_tok->set_source(src_fname, src_mod->get_orig_name());
        if (err != 0)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR, err,
                                (int)strlen(src_fname), src_fname);
            goto done;
        }

        /* 
         *   Put the tokenizer into the appropriate preprocessing mode -
         *   this makes changes to the way it expands certain directives to
         *   make the output suitable for external consumption rather than
         *   internal use.  
         */
        G_tok->set_mode_pp_only(pp_only_);
        G_tok->set_list_includes_mode(list_includes_mode_);

        /* we haven't read anything yet, so we have no previous line info */
        desc = 0;
        linenum = 0;

        /* 
         *   if we're writing out the preprocessed source, write a #charset
         *   directive to indicate that our output is in utf8 format 
         */
        if (pp_only_)
            printf("#charset \"utf8\"\n");

        /* read the file */
        for (;;)
        {
            /* read the next line, and stop if we've reached end of file */
            if (G_tok->read_line_pp())
                break;

            /* 
             *   If we're generating the preprocessed source, and we're in a
             *   different stream than for the last line, or the new line
             *   number is more than the last line number plus 1, add a
             *   #line directive to the output stream 
             */
            if (pp_only_
                && (G_tok->get_last_desc() != desc
                    || G_tok->get_last_linenum() != linenum + 1))
            {
                /* we've jumped to a new line - add a #line directive */
                printf("#line %ld %s\n", G_tok->get_last_linenum(),
                       (test_report_mode_
                        ? G_tok->get_last_desc()->get_dquoted_rootname()
                        : G_tok->get_last_desc()->get_dquoted_fname()));
            }

            /* remember the last line we read */
            desc = G_tok->get_last_desc();
            linenum = G_tok->get_last_linenum();
            
            /* show this line if we're generating preprocessed output */
            if (pp_only_)
                printf("%s\n", G_tok->get_cur_line());
        }

    done: ;
    }
    err_finally
    {
        /* update the caller's error and warning counters */
        *error_count += G_tcmain->get_error_count();
        *warning_count += G_tcmain->get_warning_count();

        /* terminate the compiler */
        CTcMain::terminate();
    }
    err_end;
}

/* 
 *   build a symbol file 
 */
void CTcMake::build_symbol_file(CTcHostIfc *hostifc,
                                CResLoader *res_loader,
                                const textchar_t *src_fname,
                                const textchar_t *sym_fname,
                                CTcMakeModule *src_mod,
                                int *error_count, int *warning_count)
{
    osfildef *fpout;
    CVmFile *volatile fp = 0;

    err_try
    {
        int err;
        
        /* initialize the compiler */
        CTcMain::init(hostifc, res_loader, source_charset_);

        /* set options */
        set_compiler_options();
        
        /* set up the tokenizer with the main input file */
        err = G_tok->set_source(src_fname, src_mod->get_orig_name());
        if (err != 0)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR, err,
                                (int)strlen(src_fname), src_fname);
            goto done;
        }
        
        /* parse in syntax-only mode */
        G_prs->set_syntax_only(TRUE);

        /* read the first token */
        G_tok->next();

        /* give the parser the module information */
        G_prs->set_module_info(
            src_mod->get_orig_name(), src_mod->get_seqno());

        /* set the sourceTextGroup mode */
        G_prs->set_source_text_group_mode(src_group_mode_);

        /* parse at the top level */
        G_prs->parse_top();

        /* if no errors occurred, write the symbol file */
        if (G_tcmain->get_error_count() == 0)
        {
            /* set up the output file */
            fpout = osfopwb(sym_fname, OSFTT3SYM);
            if (fpout == 0)
            {
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_MAKE_CANNOT_CREATE_SYM, sym_fname);
                goto done;
            }
            fp = new CVmFile();
            fp->set_file(fpout, 0);

            /* write the symbol file */
            G_prs->write_symbol_file(fp, this);
        }

    done: ;
    }
    err_finally
    {
        /* set the caller's error and warning counters */
        *error_count += G_tcmain->get_error_count();
        *warning_count += G_tcmain->get_warning_count();

        /* close our symbol file */
        if (fp != 0)
        {
            delete fp;
            fp = 0;
        }

        /* 
         *   if we weren't successful, delete the symbol file, so that we
         *   don't leave around a partially-built file 
         */
        if (*error_count != 0 || (warnings_as_errors_ && *warning_count != 0))
            osfdel(sym_fname);
        
        /* terminate the compiler */
        CTcMain::terminate();
    }
    err_end;
}

/* 
 *   build an object file
 */
void CTcMake::build_object_file(CTcHostIfc *hostifc,
                                CResLoader *res_loader,
                                const textchar_t *src_fname,
                                const textchar_t *obj_fname,
                                CTcMakeModule *src_mod,
                                int *error_count, int *warning_count)
{
    osfildef *fpout;
    CVmFile *volatile fp = 0;
    CVmFile *volatile symfile = 0;
    int err;

    err_try
    {
        CTPNStmProg *node;
        CTcMakeModule *mod;
        
        /* initialize the compiler */
        CTcMain::init(hostifc, res_loader, source_charset_);

        /* 
         *   keep object and property fixups, so that we can link the
         *   object file with other object files - each object file will
         *   use its own object and property numbering system, and we must
         *   reconcile the systems when we link 
         */
        G_keep_objfixups = TRUE;
        G_keep_propfixups = TRUE;
        G_keep_enumfixups = TRUE;

        /* set options */
        set_compiler_options();

        /* tell the tokenizer to capture strings to the string file */
        if (string_fp_ != 0)
            G_tok->set_string_capture(string_fp_);

        /* tell the code generator to write an assembly listing */
        if (assembly_listing_fp_ != 0)
        {
            /* set up the disassembly output stream */
            G_disasm_out = new CTcUnasOutFile(assembly_listing_fp_);

            /* introduce the file */
            G_disasm_out->print("[%s]\n\n", src_fname);
        }

        /* 
         *   load each module's symbols (including our own - this provides
         *   us with forward definitions of any symbols defined in our own
         *   source module but referenced prior to the definitions) 
         */
        for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
        {
            osfildef *symfp;
            char sym_fname[OSFNMAX];

            /* if this module is excluded, skip it */
            if (mod->is_excluded())
                continue;
            
            /* derive the symbol filename */
            get_symfile(sym_fname, mod);

            /* open the file */
            symfp = osfoprb(sym_fname, OSFTT3SYM);
            if (symfp == 0)
            {
                /* log the error */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_MAKE_CANNOT_OPEN_SYM, sym_fname);
            }
            else
            {
                /* set up the file object */
                symfile = new CVmFile();
                symfile->set_file(symfp, 0);

                /* load it */
                G_prs->read_symbol_file(symfile);

                /* done with the symbol file - close it */
                delete symfile;
                symfile = 0;
            }
        }

        /* if we encountered errors reading symbol files, give up now */
        if (G_tcmain->get_error_count() != 0)
            goto done;

        /* set up the tokenizer with the main input file */
        err = G_tok->set_source(src_fname, src_mod->get_orig_name());
        if (err != 0)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR, err,
                                (int)strlen(src_fname), src_fname);
            goto done;
        }

        /* read the first token */
        G_tok->next();

        /* give the parser the module information */
        G_prs->set_module_info(
            src_mod->get_orig_name(), src_mod->get_seqno());

        /* set the sourceTextGroup mode */
        G_prs->set_source_text_group_mode(src_group_mode_);

        /* parse at the top level */
        node = G_prs->parse_top();
        if (G_tcmain->get_error_count() != 0)
            goto done;

        /* fold symbolic constants for all nodes */
        node->fold_constants(G_prs->get_global_symtab());
        if (G_tcmain->get_error_count() != 0)
            goto done;

        /* set up the output file */
        fpout = osfopwb(obj_fname, OSFTT3OBJ);
        if (fpout == 0)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_MAKE_CANNOT_CREATE_OBJ, obj_fname);
            goto done;
        }
        fp = new CVmFile();
        fp->set_file(fpout, 0);

        /* generate code and write the object file */
        node->build_object_file(fp, this);

        /* add an extra blank line in the assembly listing file */
        if (G_disasm_out != 0)
            G_disasm_out->print("\n");

    done: ;
    }
    err_finally
    {
        /* set the caller's error and warning counters */
        *error_count += G_tcmain->get_error_count();
        *warning_count += G_tcmain->get_warning_count();

        /* close our output file */
        if (fp != 0)
        {
            delete fp;
            fp = 0;
        }

        /* if we still have a symbol file open, close it */
        if (symfile != 0)
        {
            delete symfile;
            symfile = 0;
        }

        /* 
         *   if any errors occurred, delete the object file in the
         *   external file system - this prevents us from leaving around
         *   an incomplete or corrupted object file when compilation
         *   fails, and helps 'make'-type tools realize that they must
         *   generate the object file target again on the next build, even
         *   if source file didn't change 
         */
        if (*error_count != 0 || (warnings_as_errors_ && *warning_count != 0))
            osfdel(obj_fname);

        /* terminate the compiler */
        CTcMain::terminate();
    }
    err_end;
}

/*
 *   Read our build configuration information from a symbol file and compare
 *   it to the current configuration.  Returns true if the configurations are
 *   identical, false if not.
 *   
 *   If we find any differences, we won't bother reading further in the file,
 *   so the file's seek offset might be left in the middle of the
 *   configuration data block on return.  
 */
int CTcMake::compare_build_config_from_sym_file(
    const char *sym_fname, CVmFile *fp)
{
    CTcMakeDef *def;
    CTcMakePath *path;
    size_t cnt;
    char buf[OSFNMAX + 1];
    os_file_stat_t sym_stat;

    /* check the compiler version */
    fp->read_bytes(buf, 5);
    if (buf[0] != TC_VSN_MAJOR
        || buf[1] != TC_VSN_MINOR
        || buf[2] != TC_VSN_REV
        || buf[3] != TC_VSN_PATCH
        || buf[4] != TC_VSN_DEVBUILD)
    {
        /* 
         *   the compiler has been updated since this file was compiled;
         *   force a recompilation in case anything has changed in the
         *   compiler 
         */
        return FALSE;
    }

    /* 
     *   read the debug mode - if the debug mode has changed, the
     *   configuration has changed 
     */
    fp->read_bytes(buf, 1);
    if ((buf[0] != 0) != (debug_ != 0))
        return FALSE;

    /* read the -U/-D options */
    for (def = def_head_, cnt = fp->read_int2() ; cnt != 0 ;
         --cnt, def = def->get_next())
    {
        /* 
         *   if we're out of symbols in our list, we seem to have left out
         *   some symbols this time, so the configuration has changed 
         */
        if (def == 0)
            return FALSE;

        /* read and compare the symbol */
        if (!read_and_compare_config_str(fp, def->get_sym()))
            return FALSE;

        /* read and compare the expansion */
        if (!read_and_compare_config_str(fp, def->get_expan()))
            return FALSE;

        /* read and compare the define/undefine flag */
        fp->read_bytes(buf, 1);
        if ((buf[0] != 0) != (def->is_def() != 0))
            return FALSE;
    }

    /* read the #include search path list */
    for (path = inc_head_, cnt = fp->read_int2() ; cnt != 0 ;
         --cnt, path = path->get_next())
    {
        /* if we're out of items in our list, the config has changed */
        if (path == 0)
            return FALSE;

        /* compare this entry */
        if (!read_and_compare_config_str(fp, path->get_path()))
            return FALSE;
    }

    /* 
     *   note the timestamp of the symbol file - if that fails, return a
     *   mismatch, because we won't be able to compare the include file
     *   timestamps to the symbol file timestamp 
     */
    if (!os_file_stat(sym_fname, TRUE, &sym_stat))
        return FALSE;

    /* read the #include file list */
    for (cnt = fp->read_int2() ; cnt != 0 ; --cnt)
    {
        size_t len;
        os_file_stat_t inc_stat;
        
        /* 
         *   read the next name's length - if it's longer than our buffer,
         *   ignore it and return a mismatch 
         */
        len = fp->read_int2();
        if (len > sizeof(buf) - 1)
            return FALSE;

        /* read the name */
        fp->read_bytes(buf, len);
        buf[len] = '\0';

        /* 
         *   get this file's timestamp - if that fails, the original
         *   include file doesn't even seem to be accessible to us any
         *   more, so we have to try a recompile 
         */
        if (!os_file_stat(buf, TRUE, &inc_stat))
            return FALSE;

        /* 
         *   if the include file has been modified more recently than the
         *   symbol file, we must recompile the source 
         */
        if (inc_stat.mod_time > sym_stat.mod_time)
            return FALSE;
    }

    /* 
     *   we didn't find any differences in the configuration - indicate
     *   that the configuration is matches 
     */
    return TRUE;
}

/*
 *   Read and compare a single configuration string 
 */
int CTcMake::read_and_compare_config_str(CVmFile *fp, const textchar_t *str)
{
    size_t len;

    /* read the length */
    len = (size_t)fp->read_int2();

    /* if the source string is null, the length must be zero to match */
    if (str == 0 && len != 0)
        return FALSE;

    /* if the lengths don't match, the strings can't match */
    if (len != get_strlen(str))
        return FALSE;

    /* keep going until we exhaust the string */
    while (len != 0)
    {
        char buf[256];
        size_t cur;

        /* read up to a buffer-full, or whatever remains if less */
        cur = sizeof(buf);
        if (len < cur)
            cur = len;

        /* read the data */
        fp->read_bytes(buf, cur);

        /* compare these bytes - if they differ, we don't have a match */
        if (memcmp(str, buf, len) != 0)
            return FALSE;

        /* skip past these bytes of the comparison string */
        str += len;

        /* deduct this amount from the remaining total length */
        len -= cur;
    }

    /* we didn't find any differences - they match */
    return TRUE;
}

/*
 *   Write our build configuration information to a symbol file 
 */
void CTcMake::write_build_config_to_sym_file(CVmFile *fp)
{
    CTcMakeDef *def;
    CTcMakePath *inc;
    CTcTokFileDesc *desc;
    size_t cnt;
    char buf[32];

    /* write the compiler version */
    buf[0] = TC_VSN_MAJOR;
    buf[1] = TC_VSN_MINOR;
    buf[2] = TC_VSN_REV;
    buf[3] = TC_VSN_PATCH;
    buf[4] = TC_VSN_DEVBUILD;
    fp->write_bytes(buf, 5);

    /* write the debug mode */
    buf[0] = (char)debug_;
    fp->write_bytes(buf, 1);

    /* 
     *   Write our list of pre-defined and pre-undefined symbols (-D and
     *   -U options).  If any changes are made to the -D/-U list, we'll
     *   have to rebuild, since these options can change the meaning of
     *   the source code.  
     */

    /* count the symbols in our list */
    for (cnt = 0, def = def_head_ ; def != 0 ;
         def = def->get_next(), ++cnt) ;

    /* write the count */
    fp->write_uint2(cnt);

    /* write each option */
    for (def = def_head_ ; def != 0 ; def = def->get_next())
    {
        /* write the symbol */
        fp->write_uint2(get_strlen(def->get_sym()));
        fp->write_bytes(def->get_sym(), get_strlen(def->get_sym()));

        /* write the expansion */
        if (def->get_expan() != 0)
        {
            fp->write_uint2(get_strlen(def->get_expan()));
            fp->write_bytes(def->get_expan(), get_strlen(def->get_expan()));
        }
        else
            fp->write_uint2(0);

        /* write the define/undefine flag */
        buf[0] = (char)def->is_def();
        fp->write_bytes(buf, 1);
    }
    
    /* 
     *   Write the #include search list.  If the search list changes, the
     *   location in which we find a particular header file could change,
     *   hence the text inserted by a #include directive could change,
     *   hence we'd have to rebuild.  
     */

    /* count the list */
    for (cnt = 0, inc = inc_head_ ; inc != 0 ;
         inc = inc->get_next(), ++cnt) ;

    /* write the count */
    fp->write_uint2(cnt);

    /* write the list */
    for (inc = inc_head_ ; inc != 0 ; inc = inc->get_next())
    {
        /* write the entry */
        fp->write_uint2(get_strlen(inc->get_path()));
        fp->write_bytes(inc->get_path(), get_strlen(inc->get_path()));
    }

    /*
     *   Write the actual list of #include files that were included in the
     *   program.  We'll have to check each of these to see if any of them
     *   have been modified more recently than the symbol file.
     */

    /* get the count */
    cnt = G_tok->get_filedesc_count();

    /* 
     *   start with the second descriptor, because the first is the source
     *   file itself, which isn't part of the include list 
     */
    desc = G_tok->get_first_filedesc();
    if (desc != 0)
    {
        /* skip the first descriptor */
        desc = desc->get_next();

        /* we're not writing it, so don't count it */
        --cnt;
    }

    /* write the count, excluding the actual source file */
    fp->write_uint2(cnt);

    /* write the descriptors */
    for ( ; desc != 0 ; desc = desc->get_next())
    {
        /* 
         *   Write the filename string.  Store the fully resolved local
         *   filename, not the original unresolved name, because we want to
         *   be able to detect a change in the configuration that points us
         *   to a different resolved local file.  
         */
        fp->write_uint2(get_strlen(desc->get_fname()));
        fp->write_bytes(desc->get_fname(), get_strlen(desc->get_fname()));
    }
}

/*
 *   build an image file 
 */
void CTcMake::build_image_file(CTcHostIfc *hostifc,
                               CResLoader *res_loader,
                               const char *image_fname,
                               int *error_count, int *warning_count,
                               CVmRuntimeSymbols *runtime_symtab,
                               CVmRuntimeSymbols *runtime_macros,
                               const char tool_data[4])
{
    osfildef *fpout;
    CVmFile *volatile fp = 0;
    CVmFile *volatile objfile = 0;

    err_try
    {
        CTcMakeModule *mod;

        /* initialize the compiler */
        CTcMain::init(hostifc, res_loader, source_charset_);

        /* set options */
        set_compiler_options();

        /* load each object module */
        for (mod = mod_head_ ; mod != 0 ; mod = mod->get_next())
        {
            osfildef *objfp;
            char obj_fname[OSFNMAX];

            /* if this module is excluded, skip it */
            if (mod->is_excluded())
                continue;

            /* derive the object filename */
            get_objfile(obj_fname, mod);

            /* open the file */
            objfp = osfoprb(obj_fname, OSFTT3OBJ);
            if (objfp == 0)
            {
                /* log the error */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_MAKE_CANNOT_OPEN_OBJ, obj_fname);
            }
            else
            {
                const char *report_fname;

                /* 
                 *   if we're in test reporting mode, use the root name only
                 *   in error reports 
                 */
                if (test_report_mode_)
                    report_fname = os_get_root_name((char *)obj_fname);
                else
                    report_fname = obj_fname;
                
                /* set up the file object */
                objfile = new CVmFile();
                objfile->set_file(objfp, 0);

                /* load it */
                G_cg->load_object_file(objfile, report_fname);

                /* done with the file - close it */
                delete objfile;
                objfile = 0;
            }
        }

        /* if we encountered errors loading object files, give up */
        if (G_tcmain->get_error_count() != 0)
            goto done;

        /* check for unresolved externals */
        if (G_prs->check_unresolved_externs())
            goto done;

        /* set up the output file */
        fpout = osfopwb(image_fname, OSFTT3IMG);
        if (fpout == 0)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_MAKE_CANNOT_CREATE_IMG, image_fname);
            goto done;
        }
        fp = new CVmFile();
        fp->set_file(fpout, 0);

        /* write the image file */
        G_cg->write_to_image(fp, data_xor_mask_, tool_data);

    done: ;
    }
    err_finally
    {
        /* set the caller's error and warning counters */
        *error_count += G_tcmain->get_error_count();
        *warning_count += G_tcmain->get_warning_count();

        /*
         *   If the build succeeded, build the global symbol table to pass
         *   to the pre-initializer, if the caller wants us to.  (We must
         *   build a table separate from the compiler global symbols, since
         *   terminating the compiler will delete the compiler global symbol
         *   table, and because we need the symbols in a different format
         *   for the interpreter anyway.)
         *   
         *   There's no need to build this table if we got any errors during
         *   the build, since we're not producing a valid image file in this
         *   case anyway.  
         */
        if (*error_count == 0)
        {
            /* build the global symbol table if desired */
            if (runtime_symtab != 0)
            {
                /* enumerate the global symbols into our builder callback */
                G_prs->get_global_symtab()
                    ->enum_entries(&build_runtime_symtab_cb, runtime_symtab);
            }

            /* build the macro table if desired */
            if (runtime_macros != 0)
            {
                /* enumerate the macros into our builder callback */
                G_tok->get_defines_table()
                    ->enum_entries(&build_runtime_macro_cb, runtime_macros);
            }
        }
            
        /* close our output file */
        if (fp != 0)
        {
            delete fp;
            fp = 0;
        }

        /* if we still have an object file open, close it */
        if (objfile != 0)
        {
            delete objfile;
            objfile = 0;
        }

        /* 
         *   if any errors occurred, delete the image file in the external
         *   file system - this prevents us from leaving around an
         *   incomplete or corrupted output file when compilation fails,
         *   and helps 'make'-type tools realize that they must generate
         *   the image file target again on the next build, even if source
         *   files didn't change 
         */
        if (*error_count != 0 || (warnings_as_errors_ && *warning_count != 0))
            osfdel(image_fname);

        /* terminate the compiler */
        CTcMain::terminate();
    }
    err_end;
}

/*
 *   Enumeration callback for building the runtime symbol table from the
 *   compiler global symbol table 
 */
void CTcMake::build_runtime_symtab_cb(void *ctx, class CTcSymbol *sym)
{
    /* get our runtime symbol table object (it's the context) */
    CVmRuntimeSymbols *symtab = (CVmRuntimeSymbols *)ctx;

    /* build the value for this symbol according to its type */
    sym->add_runtime_symbol(symtab);
}

/*
 *   Enumeration callback for building the runtime macro table from the
 *   compiler macro table 
 */
void CTcMake::build_runtime_macro_cb(void *ctx, class CVmHashEntry *e)
{
    int i;
    size_t arglen;
    
    /* cast the context and hash entry */
    CVmRuntimeSymbols *symtab = (CVmRuntimeSymbols *)ctx;
    CTcHashEntryPp *macro = (CTcHashEntryPp *)e;

    /* figure the flags - these are the same as the image file MACR flags */
    uint flags = 0;
    if (macro->has_args())
        flags |= 0x0001;
    if (macro->has_varargs())
        flags |= 0x0002;

    /* compute the total string length of the argument names */
    for (i = 0, arglen = 0 ; i < macro->get_argc() ; ++i)
        arglen += strlen(macro->get_arg_name(i)) + 1;

    /* add the macro entry to the table */
    vm_runtime_sym *sym = symtab->add_macro(
        macro->getstr(), macro->getlen(),
        macro->get_orig_expan_len(), flags,
        macro->get_argc(), arglen);

    /* store the expansion */
    memcpy(sym->macro_expansion, macro->get_orig_expansion(),
           macro->get_orig_expan_len());

    /* store the argument names */
    for (i = 0 ; i < macro->get_argc() ; ++i)
    {
        /* copy the argument name */
        const char *p = macro->get_arg_name(i);
        size_t len = strlen(p) + 1;
        memcpy(sym->macro_args[i], p, len);

        /* commit the space */
        sym->commit_macro_arg(i, len);
    }
}

/*
 *   Set the compiler options
 */
void CTcMake::set_compiler_options()
{
    CTcMakePath *inc;
    CTcMakeDef *def;
    char buf[256];
    
    /* set the error verbosity and warning level modes */
    G_tcmain->set_verbosity(verbose_);
    G_tcmain->set_show_err_numbers(show_err_numbers_);
    G_tcmain->set_pedantic(pedantic_);
    G_tcmain->set_suppress_list(suppress_list_, suppress_cnt_);
    G_tcmain->set_warnings(show_warnings_);
    G_tcmain->set_test_report_mode(test_report_mode_);
    G_tcmain->set_quote_filenames(quoted_fname_mode_);

    /* set debug mode */
    G_debug = debug_;

    /* add the #include path entries */
    for (inc = inc_head_ ; inc != 0 ; inc = inc->get_next())
        G_tok->add_inc_path(inc->get_path());

    /* if we're in debug mode, add the __DEBUG pre-defined macro */
    if (debug_)
        G_tok->add_define("__DEBUG", "1");

    /* 
     *   if we're in test report mode, generate __FILE__ expansions with
     *   root names only 
     */
    if (test_report_mode_)
        G_tok->set_test_report_mode(TRUE);

    /* add the system-name pre-defined macro */
    sprintf(buf, "__TADS_SYS_%s", OS_SYSTEM_NAME);
    G_tok->add_define(buf, "1");

    /* add the pre-defined macro expanding to the system name */
    sprintf(buf, "'%s'", OS_SYSTEM_NAME);
    G_tok->add_define("__TADS_SYSTEM_NAME", buf);

    /* add the major and minor version number macros */
    sprintf(buf, "%d", TC_VSN_MAJOR);
    G_tok->add_define("__TADS_VERSION_MAJOR", buf);
    sprintf(buf, "%d", TC_VSN_MINOR);
    G_tok->add_define("__TADS_VERSION_MINOR", buf);

    /* add the preprocess macro format version number */
    sprintf(buf, "%d", TCTOK_MACRO_FORMAT_VERSION);
    G_tok->add_define("__TADS_MACRO_FORMAT_VERSION", buf);

    /* add a symbol for __TADS3 */
    G_tok->add_define("__TADS3", "1");

    /* add the preprocessor symbol definitions */
    for (def = def_head_ ; def != 0 ; def = def->get_next())
    {
        /* define or undefine the symbol as appropriate */
        if (def->is_def())
            G_tok->add_define(def->get_sym(), def->get_expan());
        else
            G_tok->undefine(def->get_sym());
    }
}

/*
 *   Get the source filename for a module 
 */
void CTcMake::get_srcfile(textchar_t *dst, CTcMakeModule *mod)
{
    CTcMakePath *path;
    
    /* 
     *   if the file has an absolute path already, use the name without
     *   further modification; otherwise, apply our default path 
     */
    if (os_is_file_absolute(mod->get_search_source_name()))
    {
        /* it's absolute - ignore the search path */
        lib_strcpy(dst, OSFNMAX, mod->get_source_name());
        return;
    }

    /*
     *   If the file exists with the name as given (in other words, relative
     *   to the current working directory), use the name as given. 
     */
    if (!osfacc(mod->get_source_name()))
    {
        /* the file exists in the current directory - use the name as given */
        lib_strcpy(dst, OSFNMAX, mod->get_source_name());
        return;
    }

    /* search the source path for the file */
    for (path = src_head_ ; path != 0 ; path = path->get_next())
    {
        /* build the full path for this source path */
        os_build_full_path(dst, OSFNMAX, path->get_path(),
                           mod->get_search_source_name());

        /* if the resulting file exists, return it */
        if (!osfacc(dst))
            return;
    }

    /* 
     *   we failed to find the file anywhere - return the name as given,
     *   which will let the caller fail when it can't open the file 
     */
    lib_strcpy(dst, OSFNMAX, mod->get_source_name());
}

/*
 *   Get the symbol filename for a module 
 */
void CTcMake::get_symfile(textchar_t *dst, CTcMakeModule *mod)
{
    /* 
     *   If a symbol path is specified, remove the path part of the module
     *   name and add the root filename to the symbol path, since the symbol
     *   path overrides any path specified for the source file itself.
     *   Otherwise, use the name as given.  
     */
    if (symdir_.is_set())
        os_build_full_path(dst, OSFNMAX, symdir_.get(),
                           os_get_root_name((char *)mod->get_symbol_name()));
    else
        lib_strcpy(dst, OSFNMAX, mod->get_symbol_name());
}

/*
 *   Get the object filename for a module 
 */
void CTcMake::get_objfile(textchar_t *dst, CTcMakeModule *mod)
{
    /*
     *   If an object path is specified, remove the path part of the module
     *   name and add the root filename to the object path, since the object
     *   path overrides the source file path.  Otherwise, use the name as
     *   given.  
     */
    if (objdir_.is_set())
        os_build_full_path(dst, OSFNMAX, objdir_.get(),
                           os_get_root_name((char *)mod->get_object_name()));
    else
        lib_strcpy(dst, OSFNMAX, mod->get_object_name());
}


/* ------------------------------------------------------------------------ */
/*
 *   Module Object
 */

/*
 *   Set the module name.  Sets the source, symbol, and object filenames
 *   if these are not already set. 
 */
void CTcMakeModule::set_module_name(const textchar_t *modname)
{
    textchar_t buf[OSFNMAX];

    /* set the source name, if it's not already set */
    if (!src_.is_set())
    {
        /* add a default extension of ".t" to make the source name */
        lib_strcpy(buf, sizeof(buf), modname);
        os_defext(buf, "t");
        src_.set(buf);
    }

    /* set the symbol file name, if it's not already set */
    if (!sym_.is_set())
    {
        /* add a default extension of ".t3s" to make the symbol file name */
        lib_strcpy(buf, sizeof(buf), modname);
        os_remext(buf);
        os_addext(buf, "t3s");
        sym_.set(buf);
    }

    /* set the object file name, if it's not already set */
    if (!obj_.is_set())
    {
        /* add a default extension of ".t3o" to make the object name */
        lib_strcpy(buf, sizeof(buf), modname);
        os_remext(buf);
        os_addext(buf, "t3o");
        obj_.set(buf);
    }
}


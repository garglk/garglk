/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcmake.h - TADS 3 Compiler "Make" Engine
Function
  The "Make" engine doesn't have a main program entrypoint; instead,
  this class is meant to make it easy to write a main entrypoint.  The
  program main function must parse command line arguments, read a config
  file, get the parameters from a dialog, or use whatever other OS-specific
  mechanism it desires to obtain the compilation parameters.  Given the
  parameters, this class makes it easy to compile the program.

  To compile a TADS 3 program, create a CTcMake object, then perform
  the following steps:

   - set the build options (source/symbol/object paths, debug mode,
     error options, etc)
   
   - add a module object for each source file

   - set the image file name

   - invoke build()

Notes
  
Modified
  07/11/99 MJRoberts  - Creation
*/

#ifndef TCMAKE_H
#define TCMAKE_H

#include "t3std.h"

/* ------------------------------------------------------------------------ */
/*
 *   String buffer object 
 */
class CTcMakeStr
{
public:
    CTcMakeStr() { buf_ = 0; }
    ~CTcMakeStr() { lib_free_str(buf_); }

    /* get the string */
    const textchar_t *get() const { return (buf_ != 0 ? buf_ : ""); }

    /* set the string */
    void set(const textchar_t *str)
        { set(str, str == 0 ? 0 : strlen(str)); }
    void set(const textchar_t *str, size_t len)
    {
        lib_free_str(buf_);
        buf_ = lib_copy_str(str, len);
    }

    /* check to see if the string has been set - returns true if so */
    int is_set() const { return buf_ != 0; }

private:
    /* our string buffer */
    textchar_t *buf_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Module source types.  This tells us how we resolved the given name of
 *   the module to a filename.  
 */
enum tcmod_source_t
{
    /* "normal" - the module is found on the ordinary search path */
    TCMOD_SOURCE_NORMAL = 1,

    /* the module comes from the system library */
    TCMOD_SOURCE_SYSLIB = 2,

    /* the module comes from a specific user library */
    TCMOD_SOURCE_LIB = 3
};


/*
 *   Module list entry.  Each module is represented by one of these
 *   entries.
 *   
 *   A module defines a source file and its directly derived files: a
 *   symbol file, and an object file.  
 */
class CTcMakeModule
{
public:
    CTcMakeModule()
    {
        /* not in a list yet */
        nxt_ = 0;

        /* presume we won't need recompilation */
        needs_sym_recompile_ = FALSE;
        needs_obj_recompile_ = FALSE;

        /* by default, include all modules in the compilation */
        exclude_ = FALSE;

        /* presume the module is from the ordinary search path */
        source_type_ = TCMOD_SOURCE_NORMAL;

        /* we don't have a sequence number yet */
        seqno_ = 0;
    }
    ~CTcMakeModule() { }

    /* get/set the next list entry */
    CTcMakeModule *get_next() const { return nxt_; }
    void set_next(CTcMakeModule *nxt) { nxt_ = nxt; }

    /* get/set the sequence number */
    int get_seqno() const { return seqno_; }
    void set_seqno(int n) { seqno_ = n; }

    /* 
     *   Set the module name.  This will fill in the source, symbol, and
     *   object filenames with names derived from the source name if those
     *   names are not already defined.  If any of the names are already
     *   explicitly defined, this won't affect them.  
     */
    void set_module_name(const textchar_t *modname);

    /* 
     *   Get/set the original module name.  The original module name is the
     *   name as it was given on the command line, in the library source, or
     *   wherever else it was originally specified.  This is the name before
     *   conversion to local filename conventions and before resolving to a
     *   specific directory; this is useful in debugging records because it
     *   can be more portable than the resolved filename.  
     */
    const textchar_t *get_orig_name() const { return orig_name_.get(); }
    void set_orig_name(const textchar_t *name) { orig_name_.set(name); }

    /* get/set the source filename */
    const textchar_t *get_source_name() const { return src_.get(); }
    void set_source_name(const textchar_t *fname) { src_.set(fname); }

    /* 
     *   Get/set the "search" source filename.  This is the filename we use
     *   when we're trying to find the file in a search path.  In some cases,
     *   this might not be identical to the given filename; for example, when
     *   we assume a relative path for a file, we won't use the assumed
     *   relative path in the search name, since we only assumed the relative
     *   path because we thought that would tell us where the file is. 
     */
    const textchar_t *get_search_source_name() const
    {
        /* 
         *   if there's an explicit search name, use it; otherwise, simply
         *   use the normal source name 
         */
        if (search_src_.get() != 0 && search_src_.get()[0] != '\0')
            return search_src_.get();
        else
            return src_.get();
    }
    void set_search_source_name(const textchar_t *fname)
        { search_src_.set(fname); }

    /* get/set the symbol filename */
    const textchar_t *get_symbol_name() const { return sym_.get(); }
    void set_symbol_name(const textchar_t *fname) { sym_.set(fname); }

    /* get/set the object filename */
    const textchar_t *get_object_name() const { return obj_.get(); }
    void set_object_name(const textchar_t *fname) { obj_.set(fname); }

    /* get/set the symbol file recompilation flag */
    int get_needs_sym_recompile() const { return needs_sym_recompile_; }
    void set_needs_sym_recompile(int flag) { needs_sym_recompile_ = flag; }

    /* get/set the object file recompilation flag */
    int get_needs_obj_recompile() const { return needs_obj_recompile_; }
    void set_needs_obj_recompile(int flag) { needs_obj_recompile_ = flag; }

    /* get/set the exclusion flag */
    int is_excluded() const { return exclude_; }
    void set_excluded(int exc) { exclude_ = exc; }

    /* get/set the library member URL */
    const char *get_url() const { return url_.get(); }
    void set_url(const char *url) { url_.set(url); }

    /* get the module's source type */
    tcmod_source_t get_source_type() const { return source_type_; }

    /* set the module's source to the system library */
    void set_from_syslib() { source_type_ = TCMOD_SOURCE_SYSLIB; }

    /* 
     *   set the module's source to the given library, specified via
     *   URL-style library path (so a module in library 'bar' that was in
     *   turn in library 'foo' will have library path 'foo/bar') 
     */
    void set_from_lib(const textchar_t *libname, const textchar_t *url)
    {
        /* save the library's name and URL */
        lib_name_.set(libname);
        lib_url_.set(url);

        /* remember that we're from a library */
        source_type_ = TCMOD_SOURCE_LIB;
    }

    /* get our library URL */
    const textchar_t *get_from_lib() const { return lib_name_.get(); }

protected:
    /* next entry in the list */
    CTcMakeModule *nxt_;

    /* source filename */
    CTcMakeStr src_;

    /* 
     *   sequence number - this is simply an ordinal giving our position in
     *   the list of modules making up the build 
     */
    int seqno_;

    /* path-searching source filename */
    CTcMakeStr search_src_;

    /* the original name, before local file path resolution */
    CTcMakeStr orig_name_;

    /* symbol filename */
    CTcMakeStr sym_;

    /* object filename */
    CTcMakeStr obj_;

    /* flag: requires recompilation of symbol/object file */
    int needs_sym_recompile_;
    int needs_obj_recompile_;

    /* 
     *   flag: module is excluded from compilation (this is set when a
     *   module is included by a library and then explicitly excluded from
     *   the build) 
     */
    int exclude_;

    /* 
     *   Library member URL - this is the URL-style string naming the module
     *   if it is a member of a library.  This is not used except for
     *   library members.  This value is formed by adding the library prefix
     *   for each enclosing sublibrary (not including the top-level library,
     *   included from the command line or equivalent) to the "source:"
     *   variable value that included this module from its library.  A
     *   library prefix is formed by adding the library prefix for each
     *   enclosing sublibrary to the "library:" variable name that included
     *   the library, plus a terminating "/".  
     */
    CTcMakeStr url_;

    /* the name of the enclosing library */
    CTcMakeStr lib_name_;

    /* 
     *   library URL - this is the URL to our enclosing library, not
     *   including the module name itself 
     */
    CTcMakeStr lib_url_;

    /* the source of the module */
    enum tcmod_source_t source_type_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Search path list entry - each entry in this list is a directory that
 *   we'll search for a certain type of file (#include files, source files) 
 */
class CTcMakePath
{
public:
    CTcMakePath(const textchar_t *path)
    {
        /* remember the path */
        path_.set(path);

        /* we're not in a list yet */
        nxt_ = 0;
    }

    /* get/set the path */
    const textchar_t *get_path() const { return path_.get(); }
    void set_path(const textchar_t *path) { path_.set(path); }

    /* get/set the next list entry */
    CTcMakePath *get_next() const { return nxt_; }
    void set_next(CTcMakePath *nxt) { nxt_ = nxt; }

protected:
    /* our path string */
    CTcMakeStr path_;

    /* next include path in the list */
    CTcMakePath *nxt_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Preprocessor symbol definition or undefinition item 
 */
class CTcMakeDef
{
public:
    CTcMakeDef(const textchar_t *sym, const textchar_t *expan, int is_def)
    {
        /* remember the symbol and its expansion */
        sym_.set(sym);
        expan_.set(expan);

        /* remember whether it's a definition or un-definition */
        is_def_ = (is_def != 0);

        /* not in a list yet */
        nxt_ = 0;
    }

    /* get my symbol */
    const textchar_t *get_sym() const { return sym_.get(); }

    /* get my expansion text */
    const textchar_t *get_expan() const { return expan_.get(); }

    /* get my define/undefine flag */
    int is_def() const { return is_def_ != 0; }

    /* get/set the next list entry */
    CTcMakeDef *get_next() const { return nxt_; }
    void set_next(CTcMakeDef *nxt) { nxt_ = nxt; }

protected:
    /* next in the list */
    CTcMakeDef *nxt_;
    
    /* the symbol to define or undefine */
    CTcMakeStr sym_;

    /* the expansion text */
    CTcMakeStr expan_;

    /* flag: true -> define the symbol, false -> undefine it */
    unsigned int is_def_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   The program maintenance facility class.  The program main entrypoint
 *   constructs one of these objects, sets its parameters, then calls the
 *   "build()" entrypoint to carry out the build instructions.
 *   
 *   The caller should not initialize or use compiler globals.  This
 *   object owns the compiler globals, and will create and destroy
 *   compiler globals in the course of its processing.  
 */
class CTcMake
{
public:
    CTcMake();
    ~CTcMake();

    /*
     *   Set the default source file character set.  Source and header
     *   files that don't specify a character set (using a #charset
     *   directive at the very start of the file) will be read using this
     *   character set.  If this isn't specified, we'll use the default
     *   character set obtained from the OS.  
     */
    void set_source_charset(const textchar_t *charset)
    {
        /* delete any previous character set string */
        lib_free_str(source_charset_);

        /* store the new character set name */
        source_charset_ = lib_copy_str(charset);
    }

    /* 
     *   turn on/off source-level debugging - we'll generate the extra
     *   information necessary for debugging the program 
     */
    void set_debug(int debug) { debug_ = debug; }

    /* set preprocess-only mode */
    void set_pp_only(int pp_only) { pp_only_ = pp_only; }

    /* set list-include-files mode */
    void set_list_includes(int f) { list_includes_mode_ = f; }

    /* set "clean" mode */
    void set_clean_mode(int f) { clean_mode_ = f; }

    /* 
     *   set test reporting mode - in this mode, we suppress path names in
     *   filenames in progress reports, so that the output is independent of
     *   local path conventions 
     */
    void set_test_report_mode(int flag) { test_report_mode_ = flag; }

    /* 
     *   set status percentage mode - in this mode, we'll output special
     *   status update lines with a percent-done indication, for use by
     *   container environments such as Workbench 
     */
    void set_status_pct_mode(int flag) { status_pct_mode_ = flag; }

    /* set quoted filenames mode for error messages */
    void set_err_quoted_fnames(int flag) { quoted_fname_mode_ = flag; }

    /*
     *   turn on/off linking - by default, we'll compile and link, but the
     *   linking phase can be turned off so we just compile sources to
     *   object files 
     */
    void set_do_link(int do_link) { do_link_ = do_link; }

    /* 
     *   turn on preinit mode - by default, we'll run preinit if and only
     *   if we're not in debug mode 
     */
    void set_preinit(int preinit)
    {
        preinit_ = preinit;
        explicit_preinit_ = TRUE;
    }

    /* turn sourceTextGroup property generation on or off */
    void set_source_text_group_mode(int f) { src_group_mode_ = f; }

    /* turn verbose error messages on or off */
    void set_verbose(int verbose) { verbose_ = verbose; }

    /* turn error number display on or off */
    void set_show_err_numbers(int show) { show_err_numbers_ = show; }

    /* turn all warning messages on or off */
    void set_warnings(int show) { show_warnings_ = show; }

    /* treat warnings as errors mode */
    void set_warnings_as_errors(int f) { warnings_as_errors_ = f; }

    /* turn pedantic messages on or off */
    void set_pedantic(int show) { pedantic_ = show; }

    /* 
     *   Set the list of warning messages to suppress.  The caller is
     *   responsible for maintaining this memory, keeping it valid as long as
     *   we have a reference to it and deleting the memory when it's no
     *   longer needed.  We merely keep a reference to the caller's memory.  
     */
    void set_suppress_list(const int *lst, size_t cnt)
    {
        /* remember the caller's suppress list */
        suppress_list_ = lst;
        suppress_cnt_ = cnt;
    }

    /* set the constant pool data XOR mask */
    void set_data_xor_mask(uchar mask) { data_xor_mask_ = mask; }

    /* add a #include path entry */
    void add_include_path(const textchar_t *path);

    /* add a #include path entry, if it's not already in our list */
    void maybe_add_include_path(const textchar_t *path);

    /* add a source file path */
    void add_source_path(const textchar_t *dir);

    /* 
     *   add a system include/source file path - system paths are always
     *   searched after all of the regular paths, so effectively the items
     *   in these lists come after all items in the add_include_path and
     *   add_source_path lists, respectively 
     */
    class CTcMakePath *add_sys_include_path(const textchar_t *dir);
    class CTcMakePath *add_sys_source_path(const textchar_t *dir);

    /*
     *   Set the "create directories" flag.  This is false by default.  If
     *   set, we'll create the directories named in the various output
     *   file/path options if they don't already exist.  Specifically, we'll
     *   create the directories named in set_symbol_dir(), set_object_dir(),
     *   and set_image_file() options.
     */
    void set_create_dirs(int flag) { create_dirs_ = flag; }

    /*
     *   Set the symbol file directory.  Any symbol file that doesn't have
     *   an absolute path will default to this directory. 
     */
    void set_symbol_dir(const textchar_t *dir) { symdir_.set(dir); }

    /*
     *   Set the object file directory.  Any object file that doesn't have
     *   an absolute path will default to this directory. 
     */
    void set_object_dir(const textchar_t *dir) { objdir_.set(dir); }

    /* set the assembly listing file */
    void set_assembly_listing(osfildef *fp) { assembly_listing_fp_ = fp; }

    /*
     *   Set the image file name 
     */
    void set_image_file(const textchar_t *fname) { image_fname_.set(fname); }

    /* get the image filename */
    const char *get_image_file() const { return image_fname_.get(); }

    /*
     *   Add a module (a module defines a source file and its directly
     *   derived files: a symbol file and an object file).
     *   
     *   Once a module has been added to our list, we own the module.
     *   We'll delete the module object when we're deleted.  
     */
    void add_module(CTcMakeModule *mod);
    void add_module_first(CTcMakeModule *mod);

    /* get the head and tail of the module list */
    CTcMakeModule *get_first_module() const { return mod_head_; }
    CTcMakeModule *get_last_module() const { return mod_tail_; }

    /* 
     *   Add a module by filename.  src_name must be specified, but
     *   sym_name and obj_name can be null, in which case we'll use the
     *   standard algorithm to derive these names from the source file
     *   name.  
     */
    CTcMakeModule *add_module(const char *src_name,
                              const char *sym_name,
                              const char *obj_name)
        { return add_module(src_name, sym_name, obj_name, FALSE); }

    /* add a module at the head of the module list */
    CTcMakeModule *add_module_first(const char *src_name,
                                    const char *sym_name,
                                    const char *obj_name)
        { return add_module(src_name, sym_name, obj_name, TRUE); }


    /* add a module at either the start or end of the module list */
    CTcMakeModule *add_module(const char *src_name, const char *sym_name,
                              const char *obj_name, int first);

    /*
     *   Add a preprocessor symbol definition.  If the expansion text is
     *   null, we'll set the expansion text to "1" by default.  
     */
    void def_pp_sym(const textchar_t *sym, const textchar_t *expan);

    /*
     *   Un-define a preprocessor symbol.
     */
    void undef_pp_sym(const textchar_t *sym);

    /* look up a preprocessor symbol definition */
    const char *look_up_pp_sym(const textchar_t *sym, size_t sym_len);

    /*
     *   Build the program.  This can be invoked once the options are set
     *   and all of the modules have been added.  This routine checks
     *   dependencies and performs the build:
     *   
     *   - for each source file, generates a symbol file if the symbol file
     *   is not up to date
     *   
     *   - after creating all symbol files: for each source file, compiles
     *   the source to an object file if the object file isn't up to date
     *   
     *   - after creating all object files: links the object files to create
     *   an image file, if the image file isn't up to date
     *   
     *   If any errors occur, we'll stop after the step in which the errors
     *   occur.  For example, if errors occur compiling an object file,
     *   we'll stop after finishing with the object file and will not
     *   proceed to any additional object file compilations.
     *   
     *   The 'host_interface' object must be provided by the caller.  This
     *   tells the compiler how to report error messages and otherwise
     *   interact with the host environment.
     *   
     *   Fills in '*error_count' and '*warning_count' with the number of
     *   errors and warnings, respectively, that occur during the
     *   compilation.
     *   
     *   If 'force_build' is set, we'll build all derived files (symbols,
     *   objects, and image), regardless of whether it appears necessary
     *   based on file times.
     *   
     *   'argv0' is the main program's argv[0], if available; a null pointer
     *   can be passed in if argv[0] is not available (for example, if we're
     *   not running in a command-line environment and the program's
     *   executable filename is not available) 
     */
    void build(class CTcHostIfc *host_interface,
               int *error_count, int *warning_count,
               int force_build, int force_link,
               class CRcResList *res_list,
               const textchar_t *argv0);

    /*
     *   Write our build configuration information to a symbol file.  The
     *   symbol file builder calls this to give us a chance to insert our
     *   build configuration information into the symbol file.  We include
     *   all of the information we will need when re-loading the symbol file
     *   to determine if the symbol file is up-to-date, so that we can
     *   determine if we must rebuild the symbol file from the source or can
     *   use it without rebuilding.  
     */
    void write_build_config_to_sym_file(class CVmFile *fp);

    /*
     *   Compare our build configuration to the information saved in an
     *   symbol file.  Returns true if the configuration in the symbol file
     *   matches our current configuration, false if not.  A false return
     *   indicates that recompilation is necessary, because something in the
     *   configuration has changed.  
     */
    int compare_build_config_from_sym_file(const char *sym_fname,
                                           class CVmFile *fp);

    /* 
     *   Set the string capture file.  If this is set, we'll write each
     *   string token in the compiled text to this file, one string per
     *   line. 
     */
    void set_string_capture(osfildef *fp) { string_fp_ = fp; }

    /*
     *   Derive the source/symbol/object filenames for a module.  Fills in
     *   the buffer with the derived name.  If the filenames don't have
     *   paths explicitly specified, we'll use our default path settings to
     *   build the full filename.
     *   
     *   The buffers are assumed to be OSFNMAX characters long, which should
     *   be large enough for any valid filename.  
     */
    void get_srcfile(textchar_t *dst, CTcMakeModule *mod);
    void get_symfile(textchar_t *dst, CTcMakeModule *mod);
    void get_objfile(textchar_t *dst, CTcMakeModule *mod);

    /*
     *   Create a directory if it doesn't already exist.  If 'is_file' is
     *   true, the path includes both a directory path and a filename, in
     *   which case we'll create the directory containing the file as
     *   specified in the path.  If 'is_file' is false, the path specifies a
     *   directory name directly, with no filename attached.  If an error
     *   occurs, we'll generate a message and count it in the error count.
     */
    void create_dir(class CTcHostIfc *hostifc,
                    const char *path, int is_file, int *errcnt);

private:
    /* scan all modules for name collisions with other modules */
    void check_all_module_collisions(class CTcHostIfc *hostifc,
                                     class CResLoader *res_loader,
                                     int *err_cnt, int *warn_cnt);

    /* check the given module for name collisions with other modules */
    void check_module_collision(CTcMakeModule *mod);

    /*
     *   read and compare a configuration string; returns true if the
     *   string matches what's stored in the file, false if not 
     */
    int read_and_compare_config_str(CVmFile *fp, const textchar_t *str);

    /* preprocess a source file */
    void preprocess_source(class CTcHostIfc *hostifc,
                           class CResLoader *res_loader,
                           const textchar_t *src_fname,
                           CTcMakeModule *src_mod,
                           int *error_count, int *warning_count);

    /* build a symbol file */
    void build_symbol_file(class CTcHostIfc *hostifc,
                           class CResLoader *res_loader,
                           const textchar_t *src_fname,
                           const textchar_t *sym_fname,
                           CTcMakeModule *src_mod,
                           int *error_count, int *warning_count);

    /* build an object file */
    void build_object_file(class CTcHostIfc *hostifc,
                           class CResLoader *res_loader,
                           const textchar_t *src_fname,
                           const textchar_t *obj_fname,
                           CTcMakeModule *src_mod,
                           int *error_count, int *warning_count);

    /* build the image file */
    void build_image_file(class CTcHostIfc *hostifc,
                          class CResLoader *res_loader,
                          const textchar_t *image_fname,
                          int *error_count, int *warning_count,
                          class CVmRuntimeSymbols *runtime_symtab,
                          class CVmRuntimeSymbols *runtime_macros,
                          const char tool_data[4]);

    /* symbol enumeration callback: build runtime symbol table */
    static void build_runtime_symtab_cb(void *ctx, class CTcSymbol *sym);

    /* symbol enumeration callback: build runtime macro table */
    static void build_runtime_macro_cb(void *ctx, class CVmHashEntry *e);

    /* set compiler options in the G_tcmain object */
    void set_compiler_options();

    /* add a preprocessor symbol definition or undefinition */
    void add_pp_def(const textchar_t *sym, const textchar_t *expan,
                    int is_def);

    /*
     *   Get the string to report for a filename in a progress report.  If
     *   we're in test reporting mode, we'll return the root name so that we
     *   suppress all paths in progress reports; otherwise, we'll just return
     *   the name as given.  If we're quoting filenames in messages, we'll
     *   quote the filename.  
     */
    const char *get_step_fname(char *buf, const char *fname);

    /* default source file character set */
    char *source_charset_;

    /* flag: create output directories if they don't exist */
    int create_dirs_;

    /* symbol file directory */
    CTcMakeStr symdir_;

    /* object file diretory */
    CTcMakeStr objdir_;

    /* assembly listing file */
    osfildef *assembly_listing_fp_;

    /* image file name */
    CTcMakeStr image_fname_;

    /* head and tail of module list */
    CTcMakeModule *mod_head_;
    CTcMakeModule *mod_tail_;

    /* head and tail of source path list */
    CTcMakePath *src_head_;
    CTcMakePath *src_tail_;

    /* 
     *   tail of regular source path list - this is where we insert regular
     *   source path entries, which come before all system path entries 
     */
    CTcMakePath *nonsys_src_tail_;

    /* head and tail of #include path list */
    CTcMakePath *inc_head_;
    CTcMakePath *inc_tail_;

    /* tail of regular (non-system) include path list */
    CTcMakePath *nonsys_inc_tail_;

    /* head and tail of preprocessor symbol list */
    CTcMakeDef *def_head_;
    CTcMakeDef *def_tail_;

    /* string capture file */
    osfildef *string_fp_;

    /* true -> generate sourceTextGroup properties */
    int src_group_mode_;

    /* true -> show verbose error messages */
    int verbose_;

    /* true -> show error numbers with error messages */
    int show_err_numbers_;

    /* true -> show warnings, false -> suppress all warnings */
    int show_warnings_;

    /* true -> treat warnings as errors */
    int warnings_as_errors_;

    /* true -> show "pedantic" warning messages */
    int pedantic_;

    /* list of warning numbers to suppress */
    const int *suppress_list_;
    size_t suppress_cnt_;

    /* true -> debug mode */
    int debug_;

    /* true -> preprocess only */
    int pp_only_;

    /* true -> #include list mode */
    int list_includes_mode_;

    /* true -> "clean" mode */
    int clean_mode_;

    /* true -> do linking after compiling */
    int do_link_;

    /* true -> preinit mode */
    int preinit_;

    /* 
     *   true -> obey 'preinit_' setting; otherwise, use default based on
     *   'debug_' setting 
     */
    int explicit_preinit_;

    /* 
     *   data pool XOR mask - we'll mask each byte of each constant data
     *   pool with this byte when writing the image file, to obscure any
     *   text strings in the constant data 
     */
    uchar data_xor_mask_;

    /* 
     *   true -> test reporting mode: suppress all paths from filenames
     *   displayed in progress reports, in order to make the output
     *   independent of local path name conventions
     */
    int test_report_mode_;

    /* true -> percent-done reporting mode */
    int status_pct_mode_;

    /* true -> use quoted filenames in error messages */
    int quoted_fname_mode_;
};

#endif /* TCMAKE_H */


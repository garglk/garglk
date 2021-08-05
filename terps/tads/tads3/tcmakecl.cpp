#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcmakecl.cpp - TADS Compiler "Make" command line tool
Function
  Command-line interface to "make" program build utility
Notes
  
Modified
  07/11/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "t3_os.h"
#include "t3std.h"
#include "tcmake.h"
#include "tchostsi.h"
#include "vmerr.h"
#include "tcvsn.h"
#include "resload.h"
#include "tcmain.h"
#include "tclibprs.h"
#include "t3test.h"
#include "tccmdutl.h"
#include "rcmain.h"

/*
 *   Library parser.  This is a specialized version of the library parser
 *   that we use to expand library arguments into their corresponding source
 *   file lists.  
 */
class CTcLibParserCmdline: public CTcLibParser
{
public:
    /*
     *   Process a command-line argument that refers to a library.  We'll
     *   read the library and add each file mentioned in the library to the
     *   source module list.
     *   
     *   Returns zero on success, non-zero if any errors occur.  Fills in
     *   '*nodef' on return to indicate whether or not a "nodef" flag
     *   appeared in the library.  
     */
    static int process_lib_arg(CTcHostIfc *hostifc, CTcMake *mk,
                               CRcResList *res_list,
                               const char *lib_name, const char *lib_url,
                               int *nodef)
    {
        char path[OSFNMAX];
        char full_name[OSFNMAX];

        /* add a default "tl" extension to the library name */
        strcpy(full_name, lib_name);
        os_defext(full_name, "tl");

        /* extract the path name from the library's name */
        os_get_path_name(path, sizeof(path), lib_name);

        /* 
         *   add the directory containing the library to the include path,
         *   if it's not already there 
         */
        mk->maybe_add_include_path(path);

        /* set up our library parser object */
        CTcLibParserCmdline parser(hostifc, mk, res_list, full_name, lib_url);

        /* scan the library and add its files to the command line */
        parser.parse_lib();

        /* set the 'nodef' indication for the caller */
        *nodef = parser.nodef_;

        /* if there are any errors, return non-zero */
        return parser.get_err_cnt();
    }

protected:
    /* instantiate */
    CTcLibParserCmdline(CTcHostIfc *hostifc, CTcMake *mk,
                        CRcResList *res_list,
                        const char *lib_name, const char *lib_url)
        : CTcLibParser(lib_name)
    {
        /* remember our host interface */
        hostifc_ = hostifc;

        /* remember our 'make' control object */
        mk_ = mk;

        /* remember our resource list container */
        res_list_ = res_list;

        /* remember the URL to the library */
        lib_name_ = lib_copy_str(lib_name);
        lib_url_ = lib_copy_str(lib_url);

        /* no "nodef" flag yet */
        nodef_ = FALSE;
    }

    ~CTcLibParserCmdline()
    {
        /* delete the library name and URL strings */
        lib_free_str(lib_name_);
        lib_free_str(lib_url_);
    }

    /* process a source file entry in the library */
    void scan_full_source(const char *val, const char *fname)
    {
        char url[OSFNMAX*2 + 20];
        CTcMakeModule *mod;
        
        /* add the source module to our module list */
        mod = mk_->add_module(fname, 0, 0);

        /* 
         *   build the full URL for the module - this is the library URL
         *   prefix plus the "source:" value (not the full filename - simply
         *   the original unretouched value of the "source:" variable) 
         */
        sprintf(url, "%.*s%.*s", (int)OSFNMAX, lib_url_, (int)OSFNMAX, val);

        /* set the module's URL */
        mod->set_url(url);

        /* set the module's original name and library name */
        mod->set_orig_name(val);
        mod->set_from_lib(lib_name_, lib_url_);
    }

    /* process a sub-library entry in the library */
    void scan_full_library(const char *val, const char *fname)
    {
        char url[OSFNMAX*2 + 20];
        int nodef;
        
        /* 
         *   build the library URL prefix - this is the parent library URL
         *   prefix, plus our "library:" value (not the full filename -
         *   simply the original unretouched "library:" variable value),
         *   plus a slash 
         */
        sprintf(url, "%.*s%.*s/", (int)OSFNMAX, lib_url_, (int)OSFNMAX, val);

        /* parse the sub-library */
        if (process_lib_arg(hostifc_, mk_, res_list_, fname, url, &nodef))
        {
            /* 
             *   error parsing the sub-library - count it in our own error
             *   count, so we know the parsing failed 
             */
            ++err_cnt_;
        }

        /* 
         *   if the sub-library had a 'nodef' flag, count it as a 'nodef'
         *   flag in this library 
         */
        if (nodef)
            nodef_ = TRUE;
    }

    /* process a resource entry in the library */
    void scan_full_resource(const char *val, const char *fname)
    {
        /* add the resource to the list */
        res_list_->add_file(fname, val, TRUE);
    }

    /* display an error */
    void err_msg(const char *msg, ...)
    {
        va_list args;

        /* display the error */
        va_start(args, msg);
        hostifc_->v_print_err(msg, args);
        hostifc_->print_err("\n");
        va_end(args);
    }

    /* look up a preprocessor symbol */
    int get_pp_symbol(char *dst, size_t dst_len,
                      const char *sym_name, size_t sym_len)
    {
        const char *val;
        size_t val_len;

        /* ask the 'make' control object for the expansion */
        val = mk_->look_up_pp_sym(sym_name, sym_len);

        /* if we didn't find a value, return undefined */
        if (val == 0)
            return -1;

        /* if it fits in the caller's result buffer, copy it */
        val_len = strlen(val);
        if (val_len <= dst_len)
            memcpy(dst, val, val_len);

        /* return the value length */
        return val_len;
    }

    /* scan a "nodef" flag */
    void scan_nodef() { nodef_ = TRUE; }

    /* our 'make' control object */
    CTcMake *mk_;

    /* our resource list container */
    CRcResList *res_list_;

    /* host system interface */
    CTcHostIfc *hostifc_;

    /* the library name */
    char *lib_name_;

    /* 
     *   the library URL - this is the common prefix for the URL of every
     *   member of the library 
     */
    char *lib_url_;

    /* flag: we've seen a "nodef" flag */
    int nodef_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Helper object for CTcCommandUtil::parse_opt_file 
 */
class CMainOptHelper: public CTcOptFileHelper
{
public:
    /* allocate an option string */
    char *alloc_opt_file_str(size_t len) { return lib_alloc_str(len); }

    /* free an option string previously allocated */
    void free_opt_file_str(char *str) { lib_free_str(str); }

    /* process a comment (we ignore comments) */
    void process_comment_line(const char *) { }

    /* process a non-comment line (ignore it) */
    void process_non_comment_line(const char *) { }

    /* process a configuration section line (ignore it) */
    void process_config_line(const char *, const char *, int) { }
};

/* ------------------------------------------------------------------------ */
/*
 *   Make a file path relative to the option file path, if we indeed have an
 *   option file path.  'buf' is a buffer the caller provides where we can
 *   build the full path, if necessary.  We'll return a pointer either to the
 *   buffer containing the combined path, or to the original filename if we
 *   decided not to use the relative path.  
 */
static char *make_opt_file_relative(char *buf, size_t buflen,
                                    int read_opt_file,
                                    const char *opt_file_path,
                                    char *fname)
{
    /* 
     *   if we haven't read an option file, we don't have an option file path
     *   to use as the relative root, so use the original filename unchanged 
     */
    if (!read_opt_file)
        return fname;

    /* convert the name to local conventions */
    char lcl[OSFNMAX];
    os_cvt_url_dir(buf, buflen < sizeof(lcl) ? buflen : sizeof(lcl), fname);

    /* if the filename is absolute, use it as given */
    if (os_is_file_absolute(buf))
        return buf;

    /* 
     *   we have a relative filename and an option file, so build the given
     *   name relative to the option file path, using the version that we
     *   converted to local conventions 
     */
    lib_strcpy(lcl, sizeof(lcl), buf);
    os_build_full_path(buf, buflen, opt_file_path, lcl);

    /* return the caller's buffer where we built the full path */
    return buf;
}
                                    


/* ------------------------------------------------------------------------ */
/*
 *   Program main entrypoint 
 */
int main(int argc, char **argv)
{
    CTcHostIfcStdio *hostifc = new CTcHostIfcStdio();
    int curarg;
    char *p;
    int force_build = FALSE;
    int force_link = FALSE;
    CTcMake *mk;
    int err_cnt, warn_cnt;
    int image_specified = FALSE;
    char dirbuf[OSFNMAX + 4096];
    int show_banner;
    const char *opt_file = 0;
    char opt_file_path[OSFNMAX];
    int read_opt_file = FALSE;
    int usage_err = FALSE;
    int add_def_mod = TRUE;
    int compile_only = FALSE;
    int pp_only = FALSE;
    osfildef *string_fp = 0;
    osfildef *assembly_fp = 0;
    int sym_dir_set = FALSE;
    int obj_dir_set = FALSE;
    int lib_err = FALSE;
    CMainOptHelper opt_helper;
    int opt_file_path_warning = FALSE;
    int need_opt_file_path_warning;
    const size_t SUPPRESS_LIST_MAX = 100;
    int suppress_list[SUPPRESS_LIST_MAX];
    size_t suppress_cnt;
    CTcMakePath *sys_inc_entry;
    CTcMakePath *sys_lib_entry;
    int verbose = FALSE;
    int pedantic = FALSE;
    int res_recurse = TRUE;
    CRcResList *res_list;
    int warnings_as_errors = FALSE;

    /* we don't have any warning codes to suppress yet */
    suppress_cnt = 0;

    /* no errors or warnings yet */
    err_cnt = warn_cnt = 0;

    /* create a resource list, in case we have resource options */
    res_list = new CRcResList();

    /* initialize the error subsystem */
    {
        CResLoader *res_loader;
        char buf[OSFNMAX];

        /* create a resource loader */
        os_get_special_path(buf, sizeof(buf), argv[0], OS_GSP_T3_RES);
        res_loader = new CResLoader(buf);

        /* tell the resource loader the executable filename, if possible */
        if (os_get_exe_filename(buf, sizeof(buf), argv[0]))
            res_loader->set_exe_filename(buf);

        /* initialize the error subsystem */
        CTcMain::tc_err_init(1024, res_loader);

        /* done with the resource loader */
        delete res_loader;
    }

    /* create the 'make' object */
    mk = new CTcMake();

    /* presume we'll show the banner and progress messages */
    show_banner = TRUE;

    /* 
     *   Add the default system header directory to the system include path.
     *   System include paths always follow any user-specified paths in the
     *   search order.  
     */
    os_get_special_path(dirbuf, sizeof(dirbuf), argv[0], OS_GSP_T3_INC);
    sys_inc_entry = mk->add_sys_include_path(dirbuf);

    /*
     *   Add the user library search path list to the source search path.
     *   This list comes from platform-specific global configuration data
     *   (such as a environment variable on Unix), so we want it to come
     *   after any search list specified in the command-line options; ensure
     *   that we search these locations last by making them "system" paths,
     *   since system paths follow all command-line paths.  
     */
    os_get_special_path(dirbuf, sizeof(dirbuf), argv[0], OS_GSP_T3_USER_LIBS);
    for (p = dirbuf ; *p != '\0' ; )
    {
        char *start;
        char *nxt;

        /* remember where this list element starts */
        start = p;

        /* find the next path separator character */
        for ( ; *p != '\0' && *p != OSPATHSEP ; ++p) ;

        /* if there's another path element, note its start */
        nxt = p;
        if (*nxt == OSPATHSEP)
            ++nxt;

        /* null-terminate the path at the separator */
        *p = '\0';

        /* add this path */
        mk->add_sys_source_path(start);

        /* continue scanning at the next list element */
        p = nxt;
    }

    /*
     *   Add the default system library directory to the source path.
     *   System source paths always follow user-specified paths in the
     *   search order.  
     */
    os_get_special_path(dirbuf, sizeof(dirbuf), argv[0], OS_GSP_T3_LIB);
    sys_lib_entry = mk->add_sys_source_path(dirbuf);

parse_options:
    /* read the options */
    for (curarg = 1 ; curarg < argc && argv[curarg][0] == '-' ; ++curarg)
    {
        char subopt;
        char relbuf[OSFNMAX];

        /* 
         *   if we have a "-source" or "-lib" argument, we're done with
         *   options, and we're on to the module list 
         */
        if (strcmp(argv[curarg], "-source") == 0
            || strcmp(argv[curarg], "-lib") == 0
            || strcmp(argv[curarg], "-res") == 0)
            break;

        /* 
         *   figure out which option we have, starting with the first letter
         *   after the '-' 
         */
        switch(argv[curarg][1])
        {
        case 'f':
            /* read an options file - make sure we don't have one already */
            if (opt_file != 0)
            {
                /* can't read a file from a file */
                printf("error: only one option file (-f) is allowed\n");
                err_cnt = 1;
                goto done;
            }

            /* 
             *   remember the options file name - we'll get around to the
             *   actual reading of the file after we finish with the
             *   command line options 
             */
            opt_file = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 1);
            if (opt_file == 0)
                goto missing_option_arg;
            break;
            
        case 'd':
            /* set debug mode */
            mk->set_debug(TRUE);
            break;

        case 'D':
            /* add preprocessor symbol definition */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 1);
            if (p != 0)
            {
                char *eq;

                /* see if there's an '=' in the string */
                for (eq = p ; *eq != '\0' && *eq != '=' ; ++eq) ;

                /* if the '=' is present, replace it with a null byte */
                if (*eq == '=')
                {
                    /* replace it */
                    *eq = '\0';

                    /* skip to the start of the replacement text */
                    ++eq;
                }

                /* add the symbol definition */
                mk->def_pp_sym(p, eq);
            }
            else
                goto missing_option_arg;
            break;

        case 'e':
            /* check for longer option names */
            if (strlen(argv[curarg]) >= 7
                && memcmp(argv[curarg], "-errnum", 7) == 0)
            {
                /* check for "+" or "-" suffixes */
                if (strcmp(argv[curarg] + 7, "+") == 0
                    || argv[curarg][7] == '\0')
                {
                    /* turn on error number display */
                    mk->set_show_err_numbers(TRUE);
                }
                else if (strcmp(argv[curarg] + 7, "-") == 0)
                {
                    /* turn off error number display */
                    mk->set_show_err_numbers(FALSE);
                }
                else
                {
                    /* invalid option */
                    goto bad_option;
                }
            }
            else
            {
                /* invalid option */
                goto bad_option;
            }
            break;

        case 'U':
            /* add preprocess symbol un-definition */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 1);
            if (p != 0)
                mk->undef_pp_sym(p);
            else
                goto missing_option_arg;
            break;

        case 'c':
            /* see what follows */
            switch (argv[curarg][2])
            {
            case 's':
                /* it's a character set specification */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 2);
                if (p != 0)
                    mk->set_source_charset(p);
                else
                    goto missing_option_arg;
                break;

            case 'l':
                /* check for "-clean" */
                if (strcmp(argv[curarg], "-clean") == 0)
                {
                    /* set "clean" mode */
                    mk->set_clean_mode(TRUE);
                }
                else
                {
                    /* invalid option */
                    goto bad_option;
                }
                break;

            case '\0':
                /* set compile-only (no link) mode */
                mk->set_do_link(FALSE);
                compile_only = TRUE;
                break;

            default:
                /* invalid option */
                goto bad_option;
            }
            break;

        case 'a':
            /* see what follows */
            switch(argv[curarg][2])
            {
            case 'l':
                /* force only the link phase */
                force_link = TRUE;
                break;

            case '\0':
                force_build = TRUE;
                break;

            default:
                /* invalid option */
                goto bad_option;
            }
            break;
            
        case 'v':
            /* set verbose mode */
            mk->set_verbose(TRUE);
            verbose = TRUE;
            break;
            
        case 'I':
            /* add a #include path */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 1);
            if (p != 0)
            {
                /* make it relative to the option file path if appropriate */
                p = make_opt_file_relative(relbuf, sizeof(relbuf),
                                           read_opt_file, opt_file_path, p);
                
                /* add the path */
                mk->add_include_path(p);
            }
            else
                goto missing_option_arg;
            break;
            
        case 'o':
            /* set the image file name */
            p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 1);
            if (p != 0)
            {
                /* make it relative to the option file path if appropriate */
                p = make_opt_file_relative(relbuf, sizeof(relbuf),
                                           read_opt_file, opt_file_path, p);

                /* set the image file name */
                mk->set_image_file(p);

                /* note that we have an explicit image file name */
                image_specified = TRUE;
            }
            else
                goto missing_option_arg;
            break;

        case 'O':
            switch(argv[curarg][2])
            {
            case 's':
                /* 
                 *   if we already have a string capture file, close the
                 *   old one 
                 */
                if (string_fp != 0)
                {
                    osfcls(string_fp);
                    string_fp = 0;
                }
                
                /* set the string file */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 2);
                if (p == 0)
                    goto missing_option_arg;

                /* make it relative to the option file path if appropriate */
                p = make_opt_file_relative(relbuf, sizeof(relbuf),
                                           read_opt_file, opt_file_path, p);

                /* open the string file */
                string_fp = osfopwt(p, OSFTTEXT);
                if (string_fp == 0)
                {
                    printf("error: unable to create string capture file "
                           "\"%s\"\n", p);
                    goto done;
                }

                /* set the string capture file in the build object */
                mk->set_string_capture(string_fp);

                /* done */
                break;

            default:
                /* invalid suboption */
                goto bad_option;
            }
            break;

        case 'F':
            /* presume we won't need an option file path warning */
            need_opt_file_path_warning = FALSE;

            /* note the sub-option letter */
            subopt = argv[curarg][2];

            /* 
             *   If applicable, get the filename/path argument.  Most of the
             *   -F suboptions take an argument, but some don't, so check for
             *   the ones that don't. 
             */
            p = 0;
            if (strchr("C", subopt) == 0)
            {
                p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 2);
                if (p == 0)
                    goto missing_option_arg;

                /* make it relative to the option file path */
                p = make_opt_file_relative(relbuf, sizeof(relbuf),
                                           read_opt_file, opt_file_path, p);

                /* 
                 *   if this is an absolute path, note it so we can warn
                 *   about it if this is in an option file 
                 */
                need_opt_file_path_warning = os_is_file_absolute(p);
            }

            /* check the suboption */
            switch(subopt)
            {
            case 'L':
                /* override the system library path */
                sys_lib_entry->set_path(p);
                break;

            case 'I':
                /* override the system include path */
                sys_inc_entry->set_path(p);
                break;

            case 's':
                /* add the source path */
                mk->add_source_path(p);
                break;
                
            case 'y':
                /* set the symbol path */
                mk->set_symbol_dir(p);

                /* remember that we have a symbol directory */
                sym_dir_set = TRUE;
                break;
            
            case 'o':
                /* set the object file directory */
                mk->set_object_dir(p);

                /* remember that we've set this path */
                obj_dir_set = TRUE;
                break;

            case 'C':
                /* set the "create output directories" flag */
                mk->set_create_dirs(TRUE);
                break;

            case 'a':
                /* 
                 *   Set the assembly listing file.  Note that this is an
                 *   undocumented option - at the moment, the assembly
                 *   listing is a bit rough, and is meant for internal TADS 3
                 *   development use, to facilitate analysis of the
                 *   compiler's code generation.  
                 */

                /* close any previous assembly listing file */
                if (assembly_fp != 0)
                {
                    osfcls(assembly_fp);
                    assembly_fp = 0;
                }

                /* open the listing file */
                assembly_fp = osfopwt(p, OSFTTEXT);
                if (assembly_fp == 0)
                {
                    printf("error: unable to create assembly listing file "
                           "\"%s\"\n", p);
                    goto done;
                }

                /* set the listing file */
                mk->set_assembly_listing(assembly_fp);

                /* done */
                break;

            default:
                /* invalid option */
                goto bad_option;
            }

            /* 
             *   if we're reading from a command file, and we haven't already
             *   found reason to warn about absolute paths, note that we need
             *   to warn now 
             */
            if (read_opt_file
                && need_opt_file_path_warning)
                opt_file_path_warning = TRUE;

            /* done */
            break;

        case 'G':
            /* code generation options */
            if (strcmp(argv[curarg], "-Gstg") == 0)
            {
                /* turn on sourceTextGroup mode */
                mk->set_source_text_group_mode(TRUE);
            }
            else
                goto bad_option;
            break;

        case 'n':
            /* check what we have */
            if (strcmp(argv[curarg], "-nopre") == 0)
            {
                /* explicitly turn off preinit mode */
                mk->set_preinit(FALSE);
            }
            else if (strcmp(argv[curarg], "-nobanner") == 0)
            {
                /* turn off the banner */
                show_banner = FALSE;
            }
            else if (strcmp(argv[curarg], "-nodef") == 0)
            {
                /* don't add the default modules */
                add_def_mod = FALSE;
            }
            else
                goto bad_option;
            break;

        case 'p':
            /* check what we have */
            if (strcmp(argv[curarg], "-pre") == 0)
            {
                /* explicitly turn on preinit mode */
                mk->set_preinit(TRUE);
            }
            else
                goto bad_option;
            break;

        case 'P':
            /* 
             *   A preprocess-only mode.  The plain -P generates
             *   preprocessed source to standard output; -Pi generates only
             *   a list of names of #included files to standard output.  
             */
            if (strcmp(argv[curarg], "-P") == 0)
            {
                /* set preprocess-only mode */
                mk->set_pp_only(TRUE);
                pp_only = TRUE;
            }
            else if (strcmp(argv[curarg], "-Pi") == 0)
            {
                /* set list-includes mode */
                mk->set_list_includes(TRUE);
                pp_only = TRUE;
            }
            break;

        case 'q':
            /* check the full option name */
            if (strcmp(argv[curarg], "-quotefname") == 0)
            {
                /* they want quoted filenames in error messages */
                mk->set_err_quoted_fnames(TRUE);
            }
            else if (strcmp(argv[curarg], "-q") == 0)
            {
                /* quiet mode - turn off banner and progress messages */
                show_banner = FALSE;
                hostifc->set_show_progress(FALSE);
            }
            else
                goto bad_option;
            break;

        case 's':
            /* check the full option name */
            if (strcmp(argv[curarg], "-statprefix") == 0)
            {
                /* get the argument */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 10);
                
                /* set the status message prefix text */
                if (p != 0)
                    hostifc->set_status_prefix(p);
                else
                    goto missing_option_arg;
            }
            else if (strcmp(argv[curarg], "-statpct") == 0)
            {
                /* note that we want status percentage updates */
                mk->set_status_pct_mode(TRUE);
            }
            else
            {
                /* unknown option - report usage error */
                goto bad_option;
            }
            break;

        case 't':
            /* check the full option name */
            if (strcmp(argv[curarg], "-test") == 0)
            {
                /* 
                 *   it's the secret test-mode option - the test scripts use
                 *   this to set the reporting mode so that we suppress path
                 *   names in progress reports, so that the test logs can be
                 *   insensitive to local path name conventions 
                 */
                mk->set_test_report_mode(TRUE);

                /* perform any necessary test initialization */
                test_init();
            }
            else
            {
                /* unknown option - report usage error */
                goto bad_option;
            }
            break;

        case 'w':
            /* warning level/mode - see what level they're setting */
            switch(argv[curarg][2])
            {
                int num;
                int enable;

            case '0':
                /* no warnings */
                mk->set_warnings(FALSE);
                mk->set_pedantic(FALSE);
                break;

            case '1':
                /* normal warnings, but not pedantic warnings */
                mk->set_warnings(TRUE);
                mk->set_pedantic(FALSE);
                break;

            case '2':
                /* show all warnings, even pedantic ones */
                mk->set_warnings(TRUE);
                mk->set_pedantic(TRUE);
                pedantic = TRUE;
                break;

            case '+':
            case '-':
                /* 
                 *   Add/remove a warning to/from the list of warnings to
                 *   suppress.  First, note whether we're enabling or
                 *   disabling the warning.  
                 */
                enable = (argv[curarg][2] == '+');

                /* get the warning number */
                p = CTcCommandUtil::get_opt_arg(argc, argv, &curarg, 2);
                if (p == 0)
                    goto missing_option_arg;

                /* convert it to a number */
                num = atoi(p);

                /* 
                 *   add it to the list if we're disabling it, otherwise
                 *   remove it from the list (since every warning is enabled
                 *   by default) 
                 */
                if (!enable)
                {
                    /* if we don't have room, we can't add it */
                    if (suppress_cnt >= SUPPRESS_LIST_MAX)
                    {
                        /* tell them what's wrong */
                        printf("Too many -w-# options - maximum %d.\n",
                               (int)SUPPRESS_LIST_MAX);

                        /* abort */
                        goto done;
                    }

                    /* add the new list entry */
                    suppress_list[suppress_cnt++] = num;
                }
                else
                {
                    size_t rem;
                    int *src;
                    int *dst;

                    /* 
                     *   scan the list for an existing instance of this
                     *   number, and remove any we find 
                     */
                    for (rem = suppress_cnt, src = dst = suppress_list ;
                         rem != 0 ; ++src, --rem)
                    {
                        /* if this one isn't to be suppressed, keep it */
                        if (*src != num)
                            *dst++ = *src;
                    }

                    /* calculate the new count */
                    suppress_cnt = dst - suppress_list;
                }

                /* remember the updated list */
                mk->set_suppress_list(suppress_list, suppress_cnt);

                /* done */
                break;

            case 'e':
                /* turn "warnings as errors" mode on or off */
                switch (argv[curarg][3])
                {
                case '\0':
                case '+':
                    warnings_as_errors = TRUE;
                    mk->set_warnings_as_errors(TRUE);
                    break;

                case '-':
                    warnings_as_errors = FALSE;
                    mk->set_warnings_as_errors(FALSE);
                    break;

                default:
                    goto bad_option;
                }
                break;

            default:
                /* invalid option */
                goto bad_option;
            }
            break;

        case '?':
            /* take '?' as an explicit request to show the usage message */
            goto show_usage;

        case 'h':
            /* check for "-help" */
            if (strcmp(argv[curarg], "-help") == 0)
                goto show_usage;

            /* other '-h*' options are unrecognized */
            goto bad_option;
            
        default:
        bad_option:
            /* invalid - describe the problem */
            printf("Error: Invalid option: \"%s\"\n"
                   "(Type \"t3make -help\" for a summary of the "
                   "command syntax)\n", argv[curarg]);

            /* abort */
            goto done;

        missing_option_arg:
            /* missing option argument */
            printf("Error: Missing argument for option \"%s\"\n"
                   "(Type \"t3make -help\" for a summary of the "
                   "command syntax)\n", argv[curarg]);

            /* abort */
            goto done;
        }
    }

    /* 
     *   if there are no module arguments, and no project file was
     *   specified, look for a default project file 
     */
    if (curarg >= argc && !usage_err && opt_file == 0)
    {
        /* 
         *   if the default project file exists, try reading it; otherwise,
         *   show a usage error 
         */
        if (!osfacc(T3_DEFAULT_PROJ_FILE))
            opt_file = T3_DEFAULT_PROJ_FILE;
        else
        {
            /* 
             *   if there are no arguments at all, just show the usage
             *   message; otherwise, explain that we need some source files 
             */
            if (argc == 1)
            {
                /* just fall through to the usage message */
                usage_err = TRUE;
            }
            else
            {
                /* explain that we need source files */
                printf("Error: No source file(s) specified.\n"
                       "(Type \"t3make -help\" for help with the "
                       "command syntax.\n");
                goto done;
            }
        }
    }

    /* 
     *   Show the banner, unless they explicitly asked us not to (and even
     *   then, show it anyway if we're going to show the "usage" message).
     */
    if ((show_banner || usage_err) && (opt_file == 0 || read_opt_file))
    {
        char patch_id[10];
        
        /* 
         *   Generate the patch number or development build iteration number,
         *   if appropriate.  If there's a patch number, give it precedence,
         *   appending it as an extra dot-number suffix; if there's a build
         *   number, show it as an alphabetic suffix.  If neither is defined,
         *   add nothing, so we'll just have a three-part dotted version
         *   number.  
         */
        if (TC_VSN_PATCH != 0)
            sprintf(patch_id, ".%d", TC_VSN_PATCH);
        else if (TC_VSN_DEVBUILD != 0)
            sprintf(patch_id, "%c", TC_VSN_DEVBUILD + 'a' - 1);
        else
            patch_id[0] = '\0';

        /* show the banner */
        /* copyright-date-string */
        printf("TADS Compiler %d.%d.%d%s  "
               "Copyright 1999, 2012 Michael J. Roberts\n",
               TC_VSN_MAJOR, TC_VSN_MINOR, TC_VSN_REV, patch_id);
    }

    /* 
     *   warn about absolute paths in the options file, if necessary; do this
     *   only in pedantic mode, though, since it's only a usage suggestion 
     */
    if (opt_file_path_warning && pedantic)
    {
        /* show the warning */
        printf("Warning: absolute path found in -Fs/-Fo/-Fy option "
               "in options (-f) file.\n");

        /* add more explanation if in verbose mode */
        if (verbose)
            printf("It's better not to use absolute paths in the "
                   "options file, because this\n"
                   "ties the options file to the particular "
                   "directory layout of your machine.\n"
                   "If possible, refer only to subfolders of the "
                   "folder containing the options\n"
                   "file, and refer to the subfolders using "
                   "relative path notation.\n");
    }

    /* if a usage error occurred, display the usage message */
    if (usage_err)
    {
    show_usage:
        printf("usage: t3make [options] module ... [-res resources]\n"
               "Options:\n"
               "  -a      - rebuild all files, "
               "even if up to date\n"
               "  -al     - relink, even if image file is up to date\n"
               "  -c      - compile only (do not create image file)\n"
               "  -clean  - delete all derived (symbol, object, image) "
               "files\n"
               "  -cs xxx - source file character set is 'xxx'\n"
               "  -d      - compile for debugging (include symbols)\n"
               "  -D x=y  - define preprocessor symbol 'x' with value 'y'\n"
               "  -errnum - show numeric error codes with error messages\n"
               "  -f file - read command line options from 'file'\n"
               "  -I dir  - add 'dir' to #include search path\n"
               "  -FC     - create output directories (for -Fy, -Fo) if they "
               "don't exist\n"
               "  -Fs dir - add 'dir' to source file search path\n"
               "  -Fy dir - put symbol files in directory 'dir'\n"
               "  -Fo dir - put object files in directory 'dir'\n"
               "  -FI dir - override the standard system include path\n"
               "  -FL dir - override the standard system library path\n"
               "  -Gstg   - generate sourceTextGroup properties\n"
               "  -nobanner - suppress version/copyright banner\n"
               "  -nodef  - do not include default library modules in build\n"
               "  -nopre  - do not run pre-initialization, regardless of "
               "debug mode\n"
               "  -o img  - set image file name to 'img'\n"
               "  -Os str - write all strings found in compiled files to "
               "text file 'str'\n"
               "  -P      - preprocess only; write preprocessed source "
               "to console\n"
               "  -Pi     - preprocess only; write only list of #include "
               "files to console\n"
               "  -pre    - run pre-initialization, regardless of "
               "debug mode\n"
               "  -q      - quiet mode: suppress banner and "
               "progress reports\n"
               "  -quotefname - quote filenames in error messages\n"
               "  -statprefix txt - set status-message prefix text\n"
               "  -U x    - un-define preprocessor symbol 'x'\n"
               "  -v      - display verbose error messages\n"
               "  -w#     - warning level: 0=none, 1=standard "
               "(default), 2=pedantic\n"
               "  -w-#    - suppress the given warning number\n"
               "  -w+#    - enable the given warning number\n"
               "  -we     - treat warnings as errors (-we- to disable)\n"
               "\n"
               "Modules: Each entry in the module list can have one "
               "of these forms:\n"
               "\n"
               "  filename           - specify the name of a source or "
               "library file; the\n"
               "                       file's type is inferred from its "
               "filename suffix\n"
               "  -source filename   - specify the name of a source file\n"
               "  -lib filename      - specify the name of a library\n"
               "\n"
               "The default filename suffix for a source file is \".t\", "
               "and the default for a\n"
               "library is \".tl\".  You can omit the \"-source\" or "
               "\"-lib\" specifier for any\n"
               "file if its name includes the correct default suffix for "
               "its type, and its\n"
               "name doesn't begin with a \"-\".\n"
               "Immediately following each library, you can add one or more "
               "\"-x filename\"\n"
               "options to exclude library members.  Each \"-x\" excludes "
               "one library member.\n"
               "The filename specified in a \"-x\" option must exactly "
               "match the filename as it\n"
               "appears in the library \"source:\" line.\n"
               "\n"
               "If no modules are specified, the compiler will attempt to "
               "find '" T3_DEFAULT_PROJ_FILE "'\n"
               "in the current directory, and will read it to obtain build "
               "instructions.\n"
               "\n"
               "Resources: Multi-media resources can be bundled into the "
               "image file using the\n"
               "-res option.  After the -res option, specify any desired "
               "resource options:\n"
               "\n"
               "  -recurse   - recurse into subdirectories for subsequent "
               "items (default)\n"
               "  -norecurse - do not recurse into subdirectories for "
               "subsequent items\n"
               "  file       - add 'file' as a multimedia resource\n"
               "  file=alias - add 'file', using 'alias' as the resource "
               "name\n"
               "  dir        - add all files within directory 'dir' (and all "
               "files in all\n"
               "               subdirectories of 'dir', if -recurse option "
               "is in effect)\n"
               "\n"
               "When a file is specified without an '=alias' name, the "
               "stored resource name is\n"
               "the original filename converted to URL-style notation.  If "
               "an '=alias' name is\n"
               "specified, then the stored resource will be named with the "
               "alias name.\n"
               "\n"
               "Note that resource options are ignored when compiling "
               "for debugging.\n");

        /* terminate */
        goto done;
    }

    /* add the modules */
    for ( ; curarg < argc ; ++curarg)
    {
        int is_source;
        int is_lib;
        char relbuf[OSFNMAX];

        /* we don't know the file type yet */
        is_source = FALSE;
        is_lib = FALSE;

        /* check to see if we have a module type option */
        if (argv[curarg][0] == '-')
        {
            if (strcmp(argv[curarg], "-source") == 0)
            {
                /* note that we have a source file, and skip the option */
                is_source = TRUE;
                ++curarg;
            }
            else if (strcmp(argv[curarg], "-lib") == 0)
            {
                /* note that we have a library, and skip the option */
                is_lib = TRUE;
                ++curarg;
            }
            else if (strcmp(argv[curarg], "-res") == 0)
            {
                /* 
                 *   Resource arguments follow - we're done with modules.
                 *   Skip the "-res" argument and exit the module loop.  
                 */
                ++curarg;
                break;
            }
            else
            {
                /* invalid option in the module list */
                goto bad_option;
            }

            /* 
             *   if we have no filename argument following the type
             *   specifier, it's an error 
             */
            if (curarg == argc)
            {
                --curarg;
                goto missing_option_arg;
            }
        }

        /* 
         *   if we didn't find an explicit type specifier, infer the type
         *   from the filename suffix 
         */
        if (!is_source && !is_lib)
        {
            size_t len;

            /* 
             *   if we have a ".tl" suffix, assume it's a library file;
             *   otherwise, assume it's a source file 
             */
            if ((len = strlen(argv[curarg])) > 3
                && stricmp(argv[curarg] + len - 3, ".tl") == 0)
            {
                /* ".tl" - it's a library file */
                is_lib = TRUE;
            }
            else
            {
                /* something else - assume it's a source file */
                is_source = TRUE;
            }
        }

        /* make the filename relative to the option file path */
        p = make_opt_file_relative(relbuf, sizeof(relbuf),
                                   read_opt_file, opt_file_path,
                                   argv[curarg]);

        /* process the file according to its type */
        if (is_source)
        {
            /* add this file to the module list */
            CTcMakeModule *mod = mk->add_module(p, 0, 0);

            /* set the module's original name to the name as given */
            mod->set_orig_name(argv[curarg]);

            /* 
             *   Also use the original name (with a default .t suffix) as the
             *   search name, in case it's a file we're going to search for
             *   using the source search path.  When searching, we want to
             *   search for the original name, not the project-relative name.
             */
            char sbuf[OSFNMAX];
            lib_strcpy(sbuf, sizeof(sbuf), argv[curarg]);
            os_defext(sbuf, "t");
            mod->set_search_source_name(sbuf);

            /* 
             *   if no image has been specified yet already, use this
             *   module's name as the image name, with the suffix ".t3" 
             */
            if (!image_specified)
            {
                char buf[OSFNMAX];

                /* 
                 *   build the default image filename by changing the
                 *   module's extension to "t3" (or adding the "t3"
                 *   extension if the module didn't have one to begin with) 
                 */
                strcpy(buf, p);
                os_remext(buf);
                os_addext(buf, "t3");

                /* set the filename */
                mk->set_image_file(buf);

                /* note that we know the image file's name now */
                image_specified = TRUE;
            }
        }
        else
        {
            CTcMakeModule lib_mod;
            CTcMakeModule *last_pre_lib_mod;
            char fname[OSFNMAX];
            char orig_fname[OSFNMAX];
            int nodef;

            /* add a default "tl" extension */
            strcpy(fname, p);
            os_defext(fname, "tl");

            /* likewise, add a default "tl" extension to the original name */
            strcpy(orig_fname, argv[curarg]);
            os_defext(orig_fname, "tl");

            /* 
             *   set up a module for the library, so we can easily search
             *   the source path for the module 
             */
            lib_mod.set_module_name(fname);
            lib_mod.set_search_source_name(orig_fname);

            /* get the module name by searching the path if necessary */
            mk->get_srcfile(fname, &lib_mod);

            /* remember the last module before this library's modules */
            last_pre_lib_mod = mk->get_last_module();
                
            /* 
             *   Process the library.  Since this is a top-level library
             *   (rather than a sub-library included from within another
             *   library), there is no prefix to the URL's of the member
             *   items. 
             */
            if (CTcLibParserCmdline::process_lib_arg(
                hostifc, mk, res_list, fname, "", &nodef))
                lib_err = TRUE;

            /* if there was a 'nodef' flag, note it */
            if (nodef)
                add_def_mod = FALSE;

            /* process any exclusion options */
            while (curarg + 1 < argc && strcmp(argv[curarg+1], "-x") == 0)
            {

                /* skip to the "-x" */
                ++curarg;
                
                /* if the filename is missing, report a usage error */
                if (curarg + 1 >= argc)
                    goto missing_option_arg;

                /* skip the -x and get its argument (the URL to exclude) */
                const char *xurl = argv[++curarg];

                /* 
                 *   Start with the next module after the last module before
                 *   this library.  If there was nothing before this library,
                 *   then this library's first module is the first module in
                 *   the entire program,.  
                 */
                CTcMakeModule *mod = last_pre_lib_mod;
                if (mod != 0)
                    mod = mod->get_next();
                else
                    mod = mk->get_first_module();

                /* scan the list of library modules for a match */
                for ( ; mod != 0 ; mod = mod->get_next())
                {
                    /* if this module has the excluded URL, exclude it */
                    if (stricmp(mod->get_url(), xurl) == 0)
                    {
                        /* mark this module as excluded */
                        mod->set_excluded(TRUE);

                        /* 
                         *   no need to look any further - assume each URL
                         *   is unique 
                         */
                        break;
                    }
                }
            }
        }
    }

    /* if we have any more arguments, they're for resource files */
    for ( ; curarg < argc ; ++curarg)
    {
        char relbuf[OSFNMAX];

        /* check for a resource bundler option */
        if (argv[curarg][0] == '-')
        {
            /* check the argument */
            switch(argv[curarg][1])
            {
            case 'n':
                /* check for '-norecurse' */
                if (strcmp(argv[curarg], "-norecurse") == 0)
                    res_recurse = FALSE;
                else
                    goto bad_option;
                break;

            case 'r':
                /* check for '-recurse' */
                if (strcmp(argv[curarg], "-recurse") == 0)
                    res_recurse = TRUE;
                else
                    goto bad_option;
                break;

            default:
                /* unknown option */
                goto bad_option;
            }
        }
        else
        {
            char *p;
            char *fname;
            char *alias;

            /* 
             *   It's not an option, so it must be a file.  Scan for an
             *   alias, which is introduced with an '=' character.  
             */
            for (p = fname = argv[curarg] ; *p != '\0' && *p != '=' ; ++p) ;
            if (*p == '=')
            {
                /* 
                 *   overwrite the '=' with a null byte, so that the filename
                 *   ends here 
                 */
                *p = '\0';

                /* the alias starts after the '=' */
                alias = p + 1;
            }
            else
            {
                /* there's no alias */
                alias = 0;
            }

            /* make the filename relative to the option file path */
            fname = make_opt_file_relative(relbuf, sizeof(relbuf),
                                           read_opt_file, opt_file_path,
                                           fname);

            /* add this file to the resource list*/
            res_list->add_file(fname, alias, res_recurse);
        }
    }

    /*
     *   If an option file was specified or implied, and we haven't read
     *   it yet, read it now.  
     */
    if (opt_file != 0 && !read_opt_file)
    {
        osfildef *fp;
        int new_argc;
        char **new_argv;
        char new_opt_file[OSFNMAX];

        /* if the options file doesn't exist, try adding the suffix 't3m' */
        if (osfacc(opt_file))
        {
            strcpy(new_opt_file, opt_file);
            os_defext(new_opt_file, "t3m");
            opt_file = new_opt_file;
        }

        /* open the options file */
        fp = osfoprt(opt_file, OSFTTEXT);
        if (fp == 0)
        {
            printf("error: unable to read option file \"%s\"\n", opt_file);
            err_cnt = 1;
            goto done;
        }

        /* 
         *   First, parse the file simply to count the arguments.  Add in one
         *   extra argument to make room for making a copy of argv[0].  
         */
        new_argc = CTcCommandUtil::parse_opt_file(fp, 0, &opt_helper) + 1;

        /* rewind the file to parse it again */
        osfseek(fp, 0, OSFSK_SET);

        /* allocate a new argument list */
        new_argv = (char **)t3malloc(new_argc * sizeof(new_argv[0]));

        /* copy the program name from argv[0] */
        new_argv[0] = argv[0];

        /* 
         *   Re-read the file, saving the arguments.  Note that we want to
         *   start saving the arguments from the file at index 1 in the new
         *   argv array, because we reserve argv[0] to hold the original
         *   program name argument.  
         */
        CTcCommandUtil::parse_opt_file(fp, new_argv + 1, &opt_helper);

        /* done with the file - close it */
        osfcls(fp);

        /* start over with the new argument vector */
        argv = new_argv;
        argc = new_argc;
        
        /* 
         *   note that we've read the options file - we can only do this
         *   once per run 
         */
        read_opt_file = TRUE;

        /* 
         *   Note the option file's path prefix.  We'll assume that any
         *   relative filename paths mentioned in the option file are meant
         *   to be relative to the folder containing the option file itself. 
         */
        os_get_path_name(opt_file_path, sizeof(opt_file_path), opt_file);

        /* go add the new command options */
        goto parse_options;
    }

    /* 
     *   Add the default object modules, if appropriate.  Don't add the
     *   default modules unless we're linking. 
     */
    if (add_def_mod && !compile_only && !pp_only)
    {
        /* build the full path to the "_main" library module */
        char srcname[OSFNMAX];
        os_build_full_path(srcname, sizeof(srcname),
                           sys_lib_entry->get_path(), "_main");

        /* add this as the first module of our compilation */
        CTcMakeModule *mod = mk->add_module_first(srcname, 0, 0);

        /* set its original name, and note it's from the system library */
        mod->set_orig_name("_main");
        mod->set_from_syslib();
    }

    /*
     *   If the symbol file and object file directory paths weren't
     *   explicitly set, set these to the same path used by the image file.
     *   This will put all of the output files in the same place, if they
     *   didn't explicitly tell us where to put the different kinds of
     *   generated files.  
     */
    os_get_path_name(dirbuf, sizeof(dirbuf), mk->get_image_file());
    if (!sym_dir_set)
        mk->set_symbol_dir(dirbuf);
    if (!obj_dir_set)
        mk->set_object_dir(dirbuf);

    /* if we encountered any errors parsing a library, we can't proceed */
    if (lib_err)
        goto done;

    /* build the program */
    mk->build(hostifc, &err_cnt, &warn_cnt,
              force_build, force_link, res_list, argv[0]);

done:
    /* terminate the error subsystem */
    CTcMain::tc_err_term();

    /* delete our 'make' object */
    delete mk;

    /* show the error and warning counts if non-zero */
    if (err_cnt != 0 || warn_cnt != 0)
        printf("Errors:   %d\n"
               "Warnings: %d\n", err_cnt, warn_cnt);

    /* if we read an options file, delete the memory used for the options */
    if (read_opt_file)
    {
        int i;
        
        /* 
         *   delete the argv strings (except for argv[0], which came from
         *   the caller)
         */
        for (i = 1 ; i < argc ; ++i)
            opt_helper.free_opt_file_str(argv[i]);

        /* delete the argv vector itself */
        t3free(argv);
    }

    /* if we have a string capture file, close it */
    if (string_fp != 0)
        osfcls(string_fp);

    /* if we have an aassembly listing file, close it */
    if (assembly_fp != 0)
        osfcls(assembly_fp);

    /* delete the host interface */
    delete hostifc;

    /* delete the resource list */
    delete res_list;

    /* show any unfreed memory (if we're in a debug build) */
    t3_list_memory_blocks(0);

    /* 
     *   exit with an appropriate exit code, depending on whether we had
     *   any errors or not 
     */
    return (err_cnt != 0 || (warnings_as_errors && warn_cnt != 0)
            ? OSEXFAIL : OSEXSUCC);
}


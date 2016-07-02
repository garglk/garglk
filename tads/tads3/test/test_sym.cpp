#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/TEST_SYM.CPP,v 1.1 1999/07/11 00:47:03 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_sym.cpp - parser test: parse a source file and create a
                 symbol export file
Function
  
Notes
  
Modified
  05/01/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>

#include "os.h"
#include "t3std.h"
#include "tctok.h"
#include "resload.h"
#include "tcmain.h"
#include "tchostsi.h"
#include "tcglob.h"
#include "tcprs.h"
#include "vmfile.h"
#include "tct3.h"
#include "vmimage.h"
#include "vmrunsym.h"
#include "vmmeta.h"
#include "t3test.h"
#include "tcmake.h"


static void errexit(const char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

int main(int argc, char **argv)
{
    CResLoader *res_loader;
    CTcHostIfc *hostifc;
    int curarg;
    int fatal_error_count = 0;
    osfildef *fpout = 0;
    CTPNStmProg *node;
    const char *sym_fname;
    CVmFile *sym_file = 0;
    int success;
    char pathbuf[OSFNMAX];

    /* initialize for testing */
    test_init();

    /* create the host interface object */
    hostifc = new CTcHostIfcStdio();

    /* create a resource loader */
    os_get_special_path(pathbuf, sizeof(pathbuf), argv[0], OS_GSP_T3_RES);
    res_loader = new CResLoader(pathbuf);

    /* initialize the compiler */
    CTcMain::init(hostifc, res_loader, 0);

    err_try
    {
        /* scan -I arguments */
        for (curarg = 1 ; curarg < argc ; ++curarg)
        {
            char *p;
            
            /* get the argument string for easy reference */
            p = argv[curarg];
            
            /* if it's not an option, we're done */
            if (*p != '-')
                break;
            
            /* if it's a -I argument, use it */
            if (*(p + 1) == 'I')
            {
                char *path;
                
                /* 
                 *   if it's with this argument, read it, otherwise move
                 *   on to the next argument 
                 */
                if (*(p + 2) == '\0')
                    path = argv[++curarg];
                else
                    path = p + 2;
                
                /* add the directory to the include path list */
                G_tok->add_inc_path(path);
            }
            else if (*(p + 1) == 'v')
            {
                /* set verbose mode */
                G_tcmain->set_verbosity(TRUE);
            }
            else
            {
                /* 
                 *   invalid usage - consume all the arguments and fall
                 *   through to the usage checker 
                 */
                curarg = argc;
                break;
            }
        }
        
        /* check arguments */
        if (curarg + 2 != argc)
        {
            /* terminate the compiler */
            CTcMain::terminate();
            
            /* delete our objects */
            delete res_loader;
            
            /* exit with an error */
            errexit("usage: test_sym [options] <source-file> <symbol-file>\n"
                    "options:\n"
                    "   -Idir  - add dir to include path\n"
                    "   -v     - verbose error messages");
        }
        
        /* set up the tokenizer with the main input file */
        if (G_tok->set_source(argv[curarg], argv[curarg]))
            errexit("unable to open source file");

        /* set up the output symbol file */
        sym_fname = argv[curarg+1];
        fpout = osfopwb(sym_fname, OSFTT3SYM);
        if (fpout == 0)
            errexit("unable to open image file");
        sym_file = new CVmFile();
        sym_file->set_file(fpout, 0);

        /* parse in syntax-only mode */
        G_prs->set_syntax_only(TRUE);

        /* read the first token */
        G_tok->next();
        
        /* parse at the top level */
        node = G_prs->parse_top();

        /* if errors occurred during parsing, stop here */
        if (G_tcmain->get_error_count() != 0 || node == 0)
            goto done;

        /* write the symbol file */
        G_prs->write_symbol_file(sym_file, 0);

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
            G_tok->log_error(TC_SEV_FATAL, exc->get_error_code());

        /* count the fatal error */
        ++fatal_error_count;
    }
    err_end;

    /* report errors */
    fprintf(stderr,
            "Warnings: %d\n"
            "Errors:   %d\n"
            "Longest string: %lu, longest list: %lu\n",
            G_tcmain->get_warning_count(),
            G_tcmain->get_error_count() + fatal_error_count,
            (unsigned long)G_cg->get_max_str_len(),
            (unsigned long)G_cg->get_max_list_cnt());

    /* 
     *   note whether or not the compilation was successful - it succeeded
     *   if we had no errors or fatal errors 
     */
    success = (G_tcmain->get_error_count() + fatal_error_count == 0);

    /* delete the image file object (this closes the file) */
    delete sym_file;

    /* 
     *   if any errors occurred, delete the image file in the external
     *   file system - this prevents us from leaving around an incomplete
     *   or corrupted image file when compilation fails, and helps
     *   'make'-type tools realize that they must generate the image file
     *   target again on the next build, even if source files didn't
     *   change 
     */
    if (!success)
        osfdel(sym_fname);

    /* shut down the compiler */
    CTcMain::terminate();

    /* done with the res loader */
    delete res_loader;

    /* delete the host interface */
    delete hostifc;

    /* show any unfreed memory */
    t3_list_memory_blocks(0);

    /* 
     *   terminate - exit with a success indication if we had no errors
     *   (other than warnings); exit with an error indication otherwise 
     */
    return (success ? OSEXSUCC : OSEXFAIL);
}

/* ------------------------------------------------------------------------ */
/*
 *   dummy implementation of runtime symbol table 
 */
void CVmRuntimeSymbols::add_sym(const char *, size_t,
                                const vm_val_t *)
{
}

void CTcGenTarg::add_debug_line_table(ulong)
{
}

void CTPNObjProp::check_locals()
{
}

/* dummy implementation of runtime metaclass table */
vm_meta_entry_t *CVmMetaTable::get_entry_by_id(const char *id) const
{
    return 0;
}

#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/TEST_TOK.CPP,v 1.4 1999/07/11 00:47:03 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_tok.cpp - tokenizer test
Function
  
Notes
  
Modified
  04/16/99 MJRoberts  - Creation
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
#include "vmimage.h"
#include "vmrunsym.h"
#include "vmmeta.h"
#include "t3test.h"
#include "tct3drv.h"


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
    CTcTokFileDesc *desc;
    long linenum;
    int pp_mode = FALSE;
    char pathbuf[OSFNMAX];

    /* initialize for testing */
    test_init();

    /* create the host interface object */
    hostifc = new CTcHostIfcStdio();

    /* create a resource loader */
    os_get_special_path(pathbuf, sizeof(pathbuf), argv[0], OS_GSP_T3_RES);
    res_loader = new CResLoader(pathbuf);

    /* initialize the compiler */
    CTcMain::init(hostifc, res_loader, "us-ascii");

    /* set test reporting mode */
    G_tok->set_test_report_mode(TRUE);
    G_tcmain->set_test_report_mode(TRUE);

    err_try
    {
        /* add some pre-defined symbols for testing */
        G_tok->add_define("_MSC_VER", "1100");
        G_tok->add_define("_WIN32", "1");
        G_tok->add_define("_M_IX86", "500");
        G_tok->add_define("__STDC__", "0");
        G_tok->add_define("_INTEGRAL_MAX_BITS", "64");
        G_tok->add_define("__cplusplus", "1");
        
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
            else if (*(p + 1) == 'P')
            {
                /* set preprocess-only mode */
                G_tok->set_mode_pp_only(TRUE);
                pp_mode = TRUE;
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
        if (curarg + 1 != argc)
        {
            /* terminate the compiler */
            CTcMain::terminate();
            
            /* delete our objects */
            delete res_loader;
            
            /* exit with an error */
            errexit("usage: test_tok [options] <source-file>\n"
                    "options:\n"
                    "   -Idir   - add dir to include path\n"
                    "   -P      - preprocess to standard output\n"
                    "   -v      - verbose error messages");
        }
        
        /* set up the tokenizer with the main input file */
        if (G_tok->set_source(argv[curarg], argv[curarg]))
            errexit("unable to open source file");
        
        /* start out with no stream */
        desc = 0;
        linenum = 0;
        
        /* read lines of input */
        for (;;)
        {
            /* read the next line, and stop if we've reached end of file */
            if (G_tok->read_line_pp())
                break;
            
            /* 
             *   If we're in a different stream than for the last line, or
             *   the new line number is more than the last line number plus
             *   1, add a #line directive to the output stream.
             *   
             *   In order to make test log output independent of local path
             *   naming conventions and the local directory structure, use
             *   only the root filename in the #line directive.  
             */
            if (pp_mode
                && (G_tok->get_last_desc() != desc
                    || G_tok->get_last_linenum() != linenum + 1))
                printf("#line %ld %s\n", G_tok->get_last_linenum(),
                       G_tok->get_last_desc()->get_dquoted_rootname());
            
            /* remember the last line we read */
            desc = G_tok->get_last_desc();
            linenum = G_tok->get_last_linenum();
            
            /* show this line */
            printf("%s\n", G_tok->get_cur_line());
        }
        
        /* dump the hash table, to see what it looks like */
        G_tok->get_defines_table()->debug_dump();
    }
    err_catch(exc)
    {
        /* 
         *   if it's not the general internal error, log it; don't log
         *   general internal errors, since these will have been logged as
         *   specific internal errors before being thrown 
         */
        if (exc->get_error_code() != TCERR_INTERNAL_ERROR)
            printf("exception caught: %d\n", exc->get_error_code());
    }
    err_end;

    /* shut down the compiler */
    CTcMain::terminate();

    /* done with the res loader */
    delete res_loader;

    /* delete host interface */
    delete hostifc;

    /* show any unfreed memory */
    t3_list_memory_blocks(0);

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   dummy implementation of runtime symbol table 
 */
void CVmRuntimeSymbols::add_sym(const char *, size_t,
                                const vm_val_t *)
{
}

/* dummy implementation of runtime metaclass table */
vm_meta_entry_t *CVmMetaTable::get_entry_by_id(const char *id) const
{
    return 0;
}

CTcSymObj *CTcParser::add_gen_obj_stat(CTcSymObj *)
{
    return 0;
}

void CTcParser::add_gen_obj_prop_stat(
    CTcSymObj *, CTcSymProp *, const CTcConstVal *)
{
}

void CTcParser::set_source_text_group_mode(int)
{
}

class CTPNInlineObject *CTcPrsOpUnary::parse_inline_object(int)
{
    return 0;
}

CTPNAnonFunc *CTcPrsOpUnary::parse_anon_func(int, int)
{
    return 0;
}

void CTcPrsSymtab::add_to_global_symtab(CTcPrsSymtab *, CTcSymbol *)
{
}

int CTcParser::alloc_ctx_arr_idx()
{
    return 0;
}

void CTcParser::init_local_ctx()
{
}

void CTPNStmObjectBase::add_implicit_constructor()
{
}

void CTPNObjDef::fold_proplist(CTcPrsSymtab *)
{
}

void CTPNStmObjectBase::add_prop_entry(CTPNObjProp *prop, int replace)
{
}

void CTPNStmObjectBase::delete_property(CTcSymProp *)
{
}

int CTPNStmObjectBase::parse_nested_obj_prop(
    CTPNObjProp* &, int *, tcprs_term_info *, CTcToken const *, int)
{
    return FALSE;
}

void CTPNStmBase::add_debug_line_rec()
{
}

void CTPNStmBase::add_debug_line_rec(CTcTokFileDesc *, long)
{
}

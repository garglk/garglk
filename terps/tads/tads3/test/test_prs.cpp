#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/TEST_PRS.CPP,v 1.4 1999/07/11 00:47:03 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_prs.cpp - parser test
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
#include "tctarg.h"
#include "vmfile.h"
#include "tcunas.h"
#include "tct3unas.h"
#include "tcmake.h"
#include "vmimage.h"
#include "vmrunsym.h"
#include "vmmeta.h"
#include "t3test.h"


static void errexit(const char *msg)
{
    printf("%s\n", msg);
    exit(1);
}

static void show_const(int level, CTcPrsNode *result)
{
    CTPNListEle *ele;
    int i;
    
    for (i = level*3 ; i != 0 ; --i)
        printf(" ");
    
    if (result == 0)
    {
        printf("no value\n");
    }
    else if (result->is_const())
    {
        switch(result->get_const_val()->get_type())
        {
        case TC_CVT_UNK:
            printf("unknown\n");
            break;

        case TC_CVT_NIL:
            printf("nil\n");
            break;

        case TC_CVT_TRUE:
            printf("true\n");
            break;

        case TC_CVT_INT:
            printf("int = %ld\n",
                   result->get_const_val()->get_val_int());
            break;

        case TC_CVT_SSTR:
            printf("sstr = %.*s\n",
                   (int)result->get_const_val()->get_val_str_len(),
                   result->get_const_val()->get_val_str());
            break;

        case TC_CVT_LIST:
            printf("list: %d elements\n",
                   result->get_const_val()->get_val_list()
                   ->get_count());
            for (ele = result->get_const_val()->get_val_list()->get_head() ;
                 ele != 0 ; ele = ele->get_next())
                show_const(level + 1, ele->get_expr());
            break;

        case TC_CVT_OBJ:
            printf("object: %ld\n", result->get_const_val()->get_val_obj());
            break;

        case TC_CVT_PROP:
            printf("property id: %d\n",
                   result->get_const_val()->get_val_prop());
            break;

        case TC_CVT_FUNCPTR:
            printf("function pointer: %.*s\n",
                   (int)result->get_const_val()->get_val_funcptr_sym()
                   ->get_sym_len(),
                   result->get_const_val()->get_val_funcptr_sym()
                   ->get_sym());
            break;

        case TC_CVT_ANONFUNCPTR:
            printf("anonymous function pointer\n");
            break;

        default:
            break;
        }
    }
    else
        printf("Non-constant value\n");
}

/*
 *   node list entry 
 */
struct node_entry
{
    node_entry(CTcPrsNode *n, CTcTokFileDesc *d, long l)
    {
        node = n;
        desc = d;
        linenum = l;
    }
    
    /* top-level parse node for this line */
    CTcPrsNode *node;

    /* file and line of source where node started */
    CTcTokFileDesc *desc;
    long linenum;

    /* next entry in list */
    node_entry *nxt;
};

int main(int argc, char **argv)
{
    CResLoader *res_loader;
    CTcHostIfc *hostifc;
    int curarg;
    int fatal_error_count = 0;
    node_entry *node_head = 0;
    node_entry *node_tail = 0;
    osfildef *fpout = 0;
    CVmFile *imgfile = 0;
    ulong next_obj_id = 1;
    int next_local = 0;
    CTcTokFileDesc *desc;
    long linenum;
    CTcUnasSrcCodeStr *unas_in;
    CTcUnasOutStdio unas_out;
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

    /* create the disassembler input stream */
    unas_in = new CTcUnasSrcCodeStr(G_cs);

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
            errexit("usage: test_prs [options] <source-file> <image-file>\n"
                    "options:\n"
                    "   -Idir  - add dir to include path\n"
                    "   -v     - verbose error messages");
        }
        
        /* set up the tokenizer with the main input file */
        if (G_tok->set_source(argv[curarg], argv[curarg]))
            errexit("unable to open source file");

        /* set up an output file */
        fpout = osfopwb(argv[curarg+1], OSFTT3IMG);
        if (fpout == 0)
            errexit("unable to open image file");
        imgfile = new CVmFile();
        imgfile->set_file(fpout, 0);

        /* read the first token */
        G_tok->next();
        
        /* parse expressions */
        for (;;)
        {
            CTcPrsNode *result;
            CTcSymbol *entry;

            /* if we're at end of file, we're done */
            if (G_tok->getcur()->gettyp() == TOKT_EOF)
                break;

            /* check for our fake declarations */
            switch(G_tok->getcur()->gettyp())
            {
            case TOKT_OBJECT:
                /* add an object symbol */
                G_tok->next();
                entry = new CTcSymObj(G_tok->getcur()->get_text(),
                                      G_tok->getcur()->get_text_len(),
                                      FALSE, next_obj_id++, FALSE,
                                      TC_META_TADSOBJ, 0);
                G_prs->get_global_symtab()->add_entry(entry);

                /* skip the object name */
                G_tok->next();
                break;

            case TOKT_FUNCTION:
                /* add a function symbol */
                G_tok->next();
                entry = new CTcSymFunc(G_tok->getcur()->get_text(),
                                       G_tok->getcur()->get_text_len(),
                                       FALSE, 0, 0, FALSE, TRUE,
                                       FALSE, FALSE, FALSE, TRUE);
                G_prs->get_global_symtab()->add_entry(entry);

                /* skip the function name */
                G_tok->next();
                break;

            case TOKT_LOCAL:
                /* add a local variable symbol */
                G_tok->next();
                entry = new CTcSymLocal(G_tok->getcur()->get_text(),
                                        G_tok->getcur()->get_text_len(),
                                        FALSE, FALSE, next_local++);
                G_prs->get_global_symtab()->add_entry(entry);

                /* skip the function name */
                G_tok->next();
                break;
                
            default:
                /* note the starting line */
                desc = G_tok->get_last_desc();
                linenum = G_tok->get_last_linenum();
                
                /* parse an expression */
                result = G_prs->parse_expr();
                
                /* add it to our list */
                if (result != 0)
                {
                    node_entry *cur;
                    
                    /* create a new list entry */
                    cur = new node_entry(result, desc, linenum);
                    
                    /* link it at the end of our list */
                    cur->nxt = 0;
                    if (node_tail != 0)
                        node_tail->nxt = cur;
                    else
                        node_head = cur;
                    node_tail = cur;
                }
            }

            /* parse a semicolon */
            if (G_prs->parse_req_sem())
                break;
        }

        /* 
         *   if there were no parse errors, run through our node list and
         *   generate code 
         */
        if (G_tcmain->get_error_count() == 0)
        {
            /* 
             *   loop through our node list; generate code and then delete
             *   each list entry 
             */
            while (node_head != 0)
            {
                node_entry *nxt;

                /* remember the next entry */
                nxt = node_head->nxt;

                /* 
                 *   set this line's descriptor as current, for error
                 *   reporting purposes 
                 */
                G_tok->set_line_info(node_head->desc, node_head->linenum);

                /* fold symbolic constants */
                node_head->node =
                    node_head->node
                    ->fold_constants(G_prs->get_global_symtab());

                /* if it's a constant value, display it */
                show_const(0, node_head->node);

                /* 
                 *   generate code; for testing purposes, don't discard
                 *   anything, to ensure we perform all generation 
                 */
                node_head->node->gen_code(FALSE, FALSE);

                /* disassemble this much */
                unas_out.print("// line %lu\n", node_head->linenum);
                CTcT3Unasm::disasm(unas_in, &unas_out);

                /* delete this entry */
                delete node_head;

                /* move on to the next entry */
                node_head = nxt;
            }
        }
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
    
    /* delete the disassembler input object */
    delete unas_in;

    /* shut down the compiler */
    CTcMain::terminate();

    /* done with the res loader */
    delete res_loader;

    /* delete the image file */
    delete imgfile;

    /* delete the host interface */
    delete hostifc;

    /* show any unfreed memory */
    t3_list_memory_blocks(0);

    /* success */
    return 0;
}

/*
 *   dummy 'make' object implementation
 */
void CTcMake::write_build_config_to_sym_file(class CVmFile *)
{
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

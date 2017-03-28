#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/test_comp_obj.cpp,v 1.1 1999/07/11 00:47:03 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_link.cpp - parser test: link object files to image file
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

int main(int argc, char **argv)
{
    CResLoader *res_loader;
    CTcHostIfc *hostifc;
    int curarg;
    int fatal_error_count = 0;
    osfildef *fpout = 0;
    CVmFile *imgfile = 0;
    CVmFile *objfile = 0;
    const char *imgfname;
    int success;
    char pathbuf[OSFNMAX];
    static const char tool_data[4] = { 't', 's', 't', 'L' };

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
        /* scan arguments */
        for (curarg = 1 ; curarg < argc ; ++curarg)
        {
            char *p;
            
            /* get the argument string for easy reference */
            p = argv[curarg];
            
            /* if it's not an option, we're done */
            if (*p != '-')
                break;
            
            if (*(p + 1) == 'v')
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
        if (curarg + 2 > argc)
        {
            /* terminate the compiler */
            CTcMain::terminate();
            
            /* delete our objects */
            delete res_loader;
            
            /* exit with an error */
            errexit("usage: test_link [options] obj-file [obj-file [...]] "
                    "image-file\n"
                    "options:\n"
                    "   -v     - verbose error messages");
        }
        
        /* set up an output file */
        imgfname = argv[argc - 1];
        fpout = osfopwb(imgfname, OSFTT3IMG);
        if (fpout == 0)
            errexit("unable to open image file");
        imgfile = new CVmFile();
        imgfile->set_file(fpout, 0);

        /* read the object files */
        for ( ; curarg < argc - 1 ; ++curarg)
        {
            osfildef *fpobj;
            
            /* open this object file */
            fpobj = osfoprb(argv[curarg], OSFTT3OBJ);
            if (fpobj == 0)
            {
                printf("unable to open object file \"%s\"\n", argv[curarg]);
                goto done;
            }

            /* note the loading */
            printf("loading %s\n", argv[curarg]);

            /* set up the CVmFile object for it */
            objfile = new CVmFile();
            objfile->set_file(fpobj, 0);

            /* read the object file */
            G_cg->load_object_file(objfile, argv[curarg]);

            /* done with the object file */
            delete objfile;
            objfile = 0;
        }

        /* check for unresolved externals */
        if (G_prs->check_unresolved_externs())
            goto done;

        /* write the image file */
        G_cg->write_to_image(imgfile, 0, tool_data);

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

    /* delete the object file object (this closes the file) */
    delete imgfile;

    /* if we have an open object file, close it */
    if (objfile != 0)
        delete objfile;

    /* 
     *   if any errors occurred, delete the object file in the external
     *   file system - this prevents us from leaving around an incomplete
     *   or corrupted image file when compilation fails, and helps
     *   'make'-type tools realize that they must generate the image file
     *   target again on the next build, even if source files didn't
     *   change 
     */
    if (!success)
        osfdel(imgfname);

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

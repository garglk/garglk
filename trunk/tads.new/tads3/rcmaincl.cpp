#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  rcmaincl.cpp - resource compiler - command line interface
Function
  
Notes
  
Modified
  01/03/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "os.h"
#include "t3std.h"
#include "rcmain.h"

/*
 *   Host interface 
 */
class MyHostIfc: public CRcHostIfc
{
public:
    /* show an error - display it on standard output */
    void display_error(const char *msg) { printf("%s\n", msg); }

    /* show a status message - display it on standard output */
    void display_status(const char *msg) { printf("%s\n", msg); }
};

/*
 *   Main program entrypoint 
 */
int main(int argc, char **argv)
{
    int curarg;
    int create;
    int recurse = TRUE;
    const char *image_fname;
    CRcResList *res_list = 0;
    rcmain_res_op_mode_t op_mode;
    int exit_stat;
    int err;
    MyHostIfc hostifc;
    char image_buf[OSFNMAX];

    /* assume we will operate on an existing file */
    create = FALSE;

    /* start out in add-recursive mode */
    op_mode = RCMAIN_RES_OP_MODE_ADD;

    /* scan options */
    for (curarg = 1 ; curarg < argc && argv[curarg][0] == '-' ; ++curarg)
    {
        switch(argv[curarg][1])
        {
        case 'c':
            if (strcmp(argv[curarg], "-create") == 0)
                create = TRUE;
            else
                goto bad_option;
            break;
            
        default:
        bad_option:
            /* invalid option - skip all arguments so we go to usage */
            curarg = argc;
            break;
        }
    }

    /* if there's nothing left, show usage */
    if (curarg >= argc)
    {
    show_usage:
        /* display usage */
        printf("usage: t3res [options] <image-file> [operations]\n"
               "Options:\n"
               "  -create      - create a new resource file\n"
               "Operations:\n"
               "  -add         - add the following resource files (default)\n"
               "  -recurse     - recursive - include files in "
               "subdirectories (default)\n"
               "  -norecurse   - do not include files in subdirectories\n"
               "  <file>       - add the file\n"
               "  <dir>        - add files in the directory\n"
               "  <file>=<res> - add file, using <res> as resource name\n"
               "\n"
               "-add is assumed if no conflicting option is specified.\n"
               "If no resource name is explicitly provided for a file, "
               "the resource is named\n"
               "by converting the filename to a URL-style resource name.\n");

        /* give up */
        exit_stat = OSEXFAIL;
        goto done;
    }

    /* get the image filename */
    image_fname = argv[curarg];

    /* create our resource list */
    res_list = new CRcResList();

    /* parse the operations list */
    for (++curarg ; curarg < argc ; ++curarg)
    {
        /* check for an option */
        if (argv[curarg][0] == '-')
        {
            /* see what we have */
            if (strcmp(argv[curarg], "-add"))
            {
                /* set 'add' mode */
                op_mode = RCMAIN_RES_OP_MODE_ADD;
            }
            else if (strcmp(argv[curarg], "-recurse"))
            {
                /* set recursive mode */
                recurse = TRUE;
            }
            else if (strcmp(argv[curarg], "-norecurse"))
            {
                /* set non-recursive mode */
                recurse = FALSE;
            }
            else
            {
                /* invalid option */
                goto show_usage;
            }
        }
        else
        {
            char *p;
            char *alias;
            
            /* check for an alias */
            for (p = argv[curarg] ; *p != '\0' && *p != '=' ; ++p) ;
            if (*p == '=')
            {
                /* 
                 *   overwrite the '=' with a null byte so that the
                 *   filename ends here 
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
            
            /* it's a file - add the file to the operations list */
            res_list->add_file(argv[curarg], alias, recurse);
        }
    }

    /* 
     *   if we're not creating, and the image doesn't exist, try adding
     *   the default image file extension 
     */
    if (!create && osfacc(image_fname))
    {
        strcpy(image_buf, image_fname);
        os_defext(image_buf, "t3");                       /* formerly "t3x" */
        image_fname = image_buf;
    }

    /* we've parsed the arguments - go apply the operations list */
    err = CResCompMain::add_resources(image_fname, res_list,
                                      &hostifc, create, OSFTT3IMG, FALSE);

    /* set the appropriate exit status */
    exit_stat = (err ? OSEXFAIL : OSEXSUCC);

done:
    /* delete the resource list if we created one */
    if (res_list != 0)
        delete res_list;
    
    /* show any unfreed memory (if we're in a debug build) */
    t3_list_memory_blocks(0);

    /* exit with current status */
    return exit_stat;
}


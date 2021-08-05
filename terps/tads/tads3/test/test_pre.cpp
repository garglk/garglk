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
  test_pre.cpp - preinit builder command line interface
Function
  Command line interface for preinitialization.  Allows a program
  previously compiled to be preinitialized and saved into a final
  image file.
Notes
  This was originally written as a stand-alone utility, but since
  preinitialization was integrated a long time ago into t3make and
  separate preinitialization is of no real value, this has been
  eliminated except as a test driver.  (This file's original name
  was vmprecl.cpp - it was renamed to conform with the test program
  naming conventions.)
Modified
  07/21/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>

#include "t3std.h"
#include "os.h"
#include "vmpreini.h"
#include "vmfile.h"
#include "vmerr.h"
#include "vmhost.h"
#include "vmhostsi.h"
#include "vmmaincn.h"
#include "t3test.h"


int main(int argc, char **argv)
{
    osfildef *fpin;
    osfildef *fpout;
    CVmFile *file_in;
    CVmFile *file_out;
    int status;
    CVmHostIfc *hostifc;
    CVmMainClientConsole clientifc;
    int curarg;

    /* initialize for testing */
    test_init();

    /* initialize the error stack */
    err_init(1024);
    
    /* start at the first argument */
    curarg = 1;

    /* check usage */
    if (curarg + 2 > argc)
    {
        printf("usage: t3pre <original-image> <new-image> [program-args]\n"
               "\n"
               "Runs preinitialization on the image file, writing a new "
               "image file\n"
               "with the state of the program after preinitialization.\n");
        return OSEXFAIL;
    }

    /* open the files */
    if ((fpin = osfoprb(argv[curarg], OSFTT3IMG)) == 0)
    {
        printf("Error opening original image file \"%s\"\n", argv[1]);
        return OSEXFAIL;
    }

    if ((fpout = osfopwb(argv[curarg + 1], OSFTT3IMG)) == 0)
    {
        printf("Error opening new image file \"%s\"\n", argv[2]);
        return OSEXFAIL;
    }

    /* create the CVmFile objects */
    file_in = new CVmFile();
    file_in->set_file(fpin, 0);
    file_out = new CVmFile();
    file_out->set_file(fpout, 0);

    /* create our host interface */
    hostifc = new CVmHostIfcStdio(argv[0]);

    /* run preinit and write the new image file */
    err_try
    {
        /* load, run, and write */
        vm_run_preinit(file_in, argv[1], file_out, hostifc, &clientifc,
                       argv + curarg + 1, argc - curarg - 1, 0, 0);
        
        /* note the success status */
        status = OSEXSUCC;
    }
    err_catch(exc)
    {
        const char *msg;
        char buf[128];

        /* look up the message */
        msg = err_get_msg(vm_messages, vm_message_count,
                          exc->get_error_code(), FALSE);

        /* if that failed, just show the error number */
        if (msg == 0)
        {
            sprintf(buf, "[no message: code %d]", exc->get_error_code());
            msg = buf;
        }

        /* show the message */
        printf("VM Error: %s\n", msg);

        /* note the failure status */
        status = OSEXFAIL;
    }
    err_end;

    /* delete the file objects, which will close the files */
    delete file_in;
    delete file_out;

    /* terminate the error mechanism */
    err_terminate();

    /* delete our host interface object */
    delete hostifc;

    /* log any memory leaks */
    t3_list_memory_blocks(0);

    /* exit with appropriate results */
    return status;
}


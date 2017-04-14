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
  vmpreini.cpp - preinitializer
Function
  Main entrypoint for compile-time preinitialization.  Loads an image
  file, runs the main entrypoint in pre-init mode, then rewrites the
  image file in its new state after execution.
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#include <stdlib.h>

#include "t3std.h"
#include "vminit.h"
#include "vmerr.h"
#include "vmfile.h"
#include "vmimage.h"
#include "vmrun.h"
#include "vmimgrb.h"
#include "vmpreini.h"
#include "vmconsol.h"

/*
 *   Run pre-initialization 
 */
void vm_run_preinit(CVmFile *origfp, const char *image_fname,
                    CVmFile *newfp, class CVmHostIfc *hostifc,
                    class CVmMainClientIfc *clientifc, 
                    const char *const *argv, int argc,
                    class CVmRuntimeSymbols *runtime_symtab,
                    class CVmRuntimeSymbols *runtime_macros)
{
    vm_globals *vmg__;
    CVmImageLoader *volatile loader = 0;
    CVmImageFile *volatile imagefp = 0;

    /* initialize the VM */
    vm_init_options opts(hostifc, clientifc);
    vm_initialize(&vmg__, &opts);

    /* 
     *   turn off "more" on the console - when running preinitialization,
     *   any output is purely diagnostic information for the programmer
     *   and thus should be formatted as simple stdio-style console output 
     */
    G_console->set_more_state(FALSE);

    err_try
    {
        /* note where the image file starts */
        long start_pos = origfp->get_pos();
        
        /* create the loader */
        imagefp = new CVmImageFileExt(origfp);
        loader = new CVmImageLoader(imagefp, image_fname, 0);

        /* load the image */
        loader->load(vmg0_);

        /* set pre-init mode */
        G_preinit_mode = TRUE;

        /* run it, using the runtime symbols the caller sent us */
        loader->run(vmg_ argv, argc, runtime_symtab, runtime_macros, 0);

        /* 
         *   seek back to the start of the image file, since we need to
         *   copy parts of the original file to the new file 
         */
        origfp->set_pos(start_pos);

        /* save the new image file */
        vm_rewrite_image(vmg_ origfp, newfp, loader->get_static_cs_ofs());
    }
    err_finally
    {
        /* detach the pools from the image file */
        if (loader != 0)
            loader->unload(vmg0_);

        /* delete the loader and the image file object */
        if (loader != 0)
            delete loader;
        if (imagefp != 0)
            delete imagefp;

        /* terminate the VM */
        vm_terminate(vmg__, clientifc);
    }
    err_end;
}


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
  vmmaincl.cpp - T3 VM main entrypoint - command-line version
Function
  This is a command-line entrypoint wrapper for executing a VM program.
  This can be used on command-line OS's (DOS, Unix, etc) where a graphical
  user interface is not needed.

  If a graphical user interface to execute a VM program is required, this
  module should not be used.  Instead, an OS-specific version must be
  written; the OS version obtain the name of the program to run (via a
  file selector or by communication with the graphical shell, for example),
  then must initialize the VM, invoke vm_run_image(), then terminate the
  VM.  The OS version must, in other words, do pretty much everything this
  version does, but should use its own OS-specific means to obtain the name
  of the image file to run rather than reading it from argc/argv.
Notes
  
Modified
  10/07/99 MJRoberts  - Creation
*/

#include <stdlib.h>

#include "t3std.h"
#include "vmmain.h"
#include "vmmaincn.h"
#include "vmhostsi.h"


/*
 *   Main entrypoint 
 */
int main(int argc, char **argv)
{
    int stat;
    int i;
    CVmHostIfc *hostifc;
    CVmMainClientConsole clientifc;

    /* 
     *   Check for a "-plain" option; if it's there, set the terminal to
     *   plain text mode.  We must make this check before doing anything
     *   else, because os_plain() must be called prior to os_init() if
     *   it's going to be called at all.  
     */
    for (i = 1 ; i < argc ; ++i)
    {
        if (strcmp(argv[i], "-plain") == 0)
        {
            /* set plain text mode in the OS layer */
            os_plain();

            /* we've found what we're looking for - no need to look further */
            break;
        }
    }
    
    /* create the host interface */
    hostifc = new CVmHostIfcStdio(argv[0]);

    /* 
     *   Initialize the OS layer.  Since this is a command-line-only
     *   implementation, there's no need to ask the OS layer to try to get
     *   us a filename to run, so pass in null for the prompt and filename
     *   buffer.  
     */
    os_init(&argc, argv, 0, 0, 0);

    /* install the OS break handler while we're running */
    os_instbrk(1);

    /* invoke the basic entrypoint */
    stat = vm_run_image_main(&clientifc, "t3run", argc, argv,
                             TRUE, FALSE, hostifc);

    /* remove the OS break handler */
    os_instbrk(0);

    /* uninitialize the OS layer */
    os_uninit();

    /* delete the host interface object */
    delete hostifc;

    /* show any unfreed memory */
    t3_list_memory_blocks(0);

    /* exit with status code */
    os_term(stat);

    /* we shouldn't get here, but in case os_term doesn't really exit... */
    return stat;
}


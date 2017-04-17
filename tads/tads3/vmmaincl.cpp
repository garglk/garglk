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
  user interface isn't needed, AND where the OS (or C run-time library) calls
  'main()' with the conventional C-style argv vector consisting of a list of
  space delimited tokens.

  If your system doesn't do the standard argv vector construction, you must
  replace this module with a custom, OS-specific module that constructs the
  C-style argument vector.  The OS version should obtain the name of the
  program to run (via a file selector or by communication with the graphical
  shell, for example), then initialize the VM, invoke vm_run_image(), then
  terminate the VM.  The OS version must, in other words, do pretty much
  everything this version does, but should use its own OS-specific means to
  obtain the name of the image file to run rather than reading it from
  argc/argv.  You must also synthesize an argv vector using the standard C
  conventions, minimally specifying one argument (in addition to the standard
  argv[0] program name argument) specifying the name of the .t3 file to run.
  If you provide a UI to let the user specify other VM startup options and/or
  command-line arguments to pass to the .t3 program, you'll have to
  synthesize the argv vector with the correct switch syntax described in
  vmmaincl.cpp.  The layout of the argv vector is: [0]=t3run executable
  filename, followed by t3run switches, the .t3 file name, and finally
  any user arguments to pass to the .t3 file.
Notes
  
Modified
  10/07/99 MJRoberts  - Creation
*/

#include <stdlib.h>

#include "t3std.h"
#include "vmmain.h"
#include "vmmaincn.h"
#include "vmhostsi.h"
#include "osifcnet.h"


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

    /* 
     *   Initialize the OS layer.  Since this is a command-line-only
     *   implementation, there's no need to ask the OS layer to try to get us
     *   a filename to run, so pass in null for the prompt and filename
     *   buffer.  
     */
    os_init(&argc, argv, 0, 0, 0);

    /* install the OS break handler while we're running */
    os_instbrk(1);

    /* 
     *   If possible, get the full path to the executable.  This makes any
     *   future references to the exe file or its location independent of the
     *   working directory context.
     */
    char exe[OSFNMAX];
    if (os_get_exe_filename(exe, sizeof(exe), argv[0])
        || os_get_abs_filename(exe, sizeof(exe), argv[0]))
        argv[0] = exe;

    /* create the host interface */
    hostifc = new CVmHostIfcStdio(exe);

    /* invoke the basic entrypoint */
    stat = vm_run_image_main(&clientifc, "t3run", argc, argv,
                             TRUE, FALSE, hostifc);

#ifdef TADSNET
    /* 
     *   Disconnect the Web UI, if applicable.  Leave any final UI window
     *   state displayed until the user manually closes it, so that the user
     *   can read any final messages displayed when the game program
     *   terminated. 
     */
    osnet_disconnect_webui(FALSE);

    /* shut down the network layer, if applicable */
    os_net_cleanup();
#endif

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
    AFTER_OS_TERM(return stat;)
}

/*
 *   For command-line builds, we don't have any special UI setup that depends
 *   on the loaded intrinsics, so we can stub out this routine.  
 */
void os_init_ui_after_load(class CVmBifTable *, class CVmMetaTable *)
{
}

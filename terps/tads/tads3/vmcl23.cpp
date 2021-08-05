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
  vmcl23.cpp - command line entrypoint for combined TADS 2/3 interpreter
Function
  This is a command-line entrypoint wrapper for executing a compiled
  TADS 2 or TADS 3 program.  This can be used on command-line OS's (DOS,
  Unix, etc) where a graphical user interface is not needed.

  If a graphical user interface to execute a VM program is required,
  this module should not be used.  Instead, an OS-specific version must be
  written; the OS version obtain the name of the program to run (via a file
  selector or by communication with the graphical shell, for example), then
  must initialize the VM, invoke vm_run_image(), then terminate the VM.  The
  OS version must, in other words, do pretty much everything this version
  does, but should use its own OS-specific means to obtain the name of the
  image file to run rather than reading it from argc/argv.
Notes

Modified
  10/07/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>

#include "os.h"

/* tads 2 headers */
#include "trd.h"

/* tads 3 headers */
#include "t3std.h"
#include "vmmain.h"
#include "vmvsn.h"
#include "vmmaincn.h"
#include "vmhostsi.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/* 
 *   Check for a "-plain" option; if it's there, set the terminal to plain
 *   text mode.  We must make this check before doing anything else, because
 *   os_plain() must be called prior to os_init() if it's going to be called
 *   at all.  
 */
static void check_plain_option(int argc, char **argv)
{
    int i;

    /* scan the argument list for the "-plain" option */
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
}


/* ------------------------------------------------------------------------ */
/*
 *   Invoke the tads 2 main entrypoint with the given command-line arguments 
 */
static int main_t2(int argc, char **argv)
{
    int stat;
    
    /* check for a "-plain" option */
    check_plain_option(argc, argv);

    /* initialize the OS layer */
    os_init(&argc, argv, 0, 0, 0);

    /* install the break handler */
    os_instbrk(1);

    /* invoke the tads 2 main entrypoint */
    stat = os0main2(argc, argv, trdmain, "", 0, 0);

#ifdef TADSNET
    /* shut down the network layer */
    os_net_cleanup();
#endif

    /* done with the break handler */
    os_instbrk(0);

    /* uninitialize the OS layer */
    os_uninit();

    /* return the status */
    return stat;
}

/* ------------------------------------------------------------------------ */
/*
 *   Invoke the T3 VM with the given command-line arguments
 */
static int main_t3(int argc, char **argv)
{
    CVmMainClientConsole clientifc;
    int stat;
    CVmHostIfc *hostifc = new CVmHostIfcStdio(argv[0]);

    /* check for a "-plain" option */
    check_plain_option(argc, argv);

    /* 
     *   Initialize the OS layer.  Since this is a command-line-only
     *   implementation, there's no need to ask the OS layer to try to get
     *   us a filename to run, so pass in null for the prompt and filename
     *   buffer.  
     */
    os_init(&argc, argv, 0, 0, 0);

    /* invoke the basic entrypoint */
    stat = vm_run_image_main(&clientifc, "t3run", argc, argv,
                             TRUE, FALSE, hostifc);

#ifdef TADSNET
    /* shut down the network layer */
    os_net_cleanup();
#endif

    /* uninitialize the OS layer */
    os_uninit();

    /* done with the host interface */
    delete hostifc;

    /* show any unfreed memory */
    t3_list_memory_blocks(0);

    /* exit with status code */
    os_term(stat);

    /* we shouldn't get here, but in case os_term doesn't really exit... */
    AFTER_OS_TERM(return stat;)
}

/* ------------------------------------------------------------------------ */
/*
 *   Main entrypoint 
 */
int main(int argc, char **argv)
{
    int stat;
    static const char *defexts[] = { "gam", "t3" };
    char prog_arg[OSFNMAX];
    char fname[OSFNMAX];
    int engine_ver;

    /* 
     *   if one of our special usage message arguments was given, show the
     *   usage 
     */
    if (argc == 2 && stricmp(argv[1], "-help2") == 0)
    {
        /* invoke the tads 2 main entrypoint with no arguments */
        main_t2(1, argv);
        
        /* that's all we need to do with this option */
        AFTER_OS_TERM(os_term(OSEXSUCC);)
    }
    else if (argc == 2 && stricmp(argv[1], "-help3") == 0)
    {
        /* invoke the tads 3 main entrypoint with no arguments */
        main_t3(1, argv);
        
        /* that's all we need to do with this option */
        AFTER_OS_TERM(os_term(OSEXSUCC);)
    }

    /* look at the arguments and try to find the program name */
    if (!vm_get_game_arg(argc, argv, prog_arg, sizeof(prog_arg), &engine_ver))
    {
        /* 
         *   there's no game file name specified or implied - show the
         *   generic combined v2/v3 usage message 
         */
        /* copyright-date-string */
        printf("TADS Interpreter - "
               "Copyright (c) 1993, 2012 Michael J. Roberts\n"
               "TADS 2 VM version " TADS_RUNTIME_VERSION " / "
               "T3 VM version " T3VM_VSN_STRING "\n\n");

        printf("Error: you didn't specify a game file name on the command "
               "line, or the command\n"
               "options are incorrect.  You must specify the name of "
               "the game file you would\n"
               "like to run.\n"
               "\n"
               "If you'd like a list of command-line options for TADS 2 "
               "games, specify -help2\n"
               "instead of giving a game file name.  Or, if you'd like a "
               "list of command-line\n"
               "options for TADS 3, specify -help3.\n");

        /* pause (if desired by OS layer) and exit */
        os_expause();
        os_term(OSEXFAIL);
    }

    /* determine the type of the game we have */
    if (prog_arg[0] != '\0')
        engine_ver = vm_get_game_type(prog_arg, fname, sizeof(fname),
                                      defexts,
                                      sizeof(defexts)/sizeof(defexts[0]));

    /* presume failure */
    stat = OSEXFAIL;

    /* see what we have */
    switch(engine_ver)
    {
    case VM_GGT_TADS2:
        /* run the game using the TADS 2 engine */
        stat = main_t2(argc, argv);
        break;

    case VM_GGT_TADS3:
        /* run the game using the TADS 3 engine */
        AFTER_OS_TERM(stat =) main_t3(argc, argv);
        AFTER_OS_TERM(break;)

    case VM_GGT_INVALID:
        /* invalid file type */
        printf("The file you have selected (%s) is not a valid game file.\n",
               fname);
        break;

    case VM_GGT_NOT_FOUND:
        /* file not found */
        printf("The game file (%s) cannot be found.\n", prog_arg);
        break;

    case VM_GGT_AMBIG:
        /* ambiguous file */
        printf("The game file (%s) cannot be found exactly as given, "
               "but multiple game\n"
               "files with this name and different default suffixes "
               "(.gam, .t3) exist.\n"
               "Please specify the full name of the file, including the "
               "suffix, that you\n"
               "wish to use.\n",
               prog_arg);
        break;
    }

    /* pause (if desired by OS layer) and terminate with our status code */
    os_expause();
    os_term(stat);
    AFTER_OS_TERM(return stat;)
}

/*
 *   For command-line builds, we don't have any special UI setup that depends
 *   on the loaded intrinsics, so we can stub out this routine.  
 */
void os_init_ui_after_load(class CVmBifTable *, class CVmMetaTable *)
{
}

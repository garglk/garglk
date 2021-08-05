#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/OS0TR.C,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os0tr.c - os mainline for tads2 run-time
Function
  invokes runtime from operating system command line
Notes
  none
Modified
  04/04/92 MJRoberts     - creation
*/

#include "os.h"
#include "std.h"
#ifdef __DPMI16__
#include "ltk.h"
#endif
#include "trd.h"

int main(argc, argv)
int   argc;
char *argv[];
{
    int err;
    char *config_name;

    config_name = "config.tr";
#ifdef __DPMI16__
    ltkini(16384);
/*    config_name = "config16.tr"; */
#endif

    /* scan for a "-plain" option */
    {
        int i;
        for (i = 1 ; i < argc ; ++i)
        {
            if (!strcmp(argv[i], "-plain"))
                os_plain();
        }
    }

    /* run OS initialization */
    os_init(&argc, argv, (char *)0, (char *)0, 0);

    /* install the break handler */
    os_instbrk(1);

    /* call the main routine */
    err = os0main2(argc, argv, trdmain, "", config_name, 0);

    /* remove the break handler */
    os_instbrk(0);

    /* uninitialize the OS layer */
    os_uninit();
    
    return(err);
}


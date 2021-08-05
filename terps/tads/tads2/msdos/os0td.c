#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/OS0TD.C,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os0td.c - os mainline for tads2 debugger
Function
  invokes debugger from operating system command line
Notes
  none
Modified
  04/04/92 MJRoberts     - creation
*/

#include "os.h"
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
    
    config_name = "config.tdb";
#ifdef __DPMI16__
    ltkini(16384);
    /*config_name = "config16.tdb";*/
#else
#endif

    os_init(&argc, argv, (char *)0, (char *)0, 0);
    os_instbrk(1);
    err = os0main2(argc, argv, tddmain, "i", config_name, 0);
    os_instbrk(0);
    os_uninit();

    return err;
}


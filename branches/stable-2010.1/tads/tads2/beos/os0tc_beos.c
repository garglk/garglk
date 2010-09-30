static char RCSid[] =
"$Header$";

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os0tc.c - os mainline for tads2 compiler
Function
  invokes compiler from operating system command line
Notes
  none
Modified
  04/02/92 MJRoberts     - creation
*/

#include "os.h"

int main(int argc, char** argv)
{
    int tcdmain(/*_ int argc, char *argv[] _*/);
    int err;

    os_init(&argc, argv, (char *)0, (char *)0, 0);
    err = os0main(argc, argv, tcdmain, "i", "config.tc");
    os_uninit();
    os_term(err);
        return err;
}

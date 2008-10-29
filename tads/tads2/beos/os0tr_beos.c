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
  os0tc.c - os mainline for tads2 runtime
Function
  invokes runtime from operating system command line
Notes
  none
Modified
  04/02/92 MJRoberts     - creation
*/

#include "os.h"

int main(int argc, char** argv)
{
    int trdmain(/*_ int argc, char *argv[] _*/);
    int err;

    os_init(&argc, argv, (char *)0, (char *)0, 0);
    err = os0main2(argc, argv, trdmain, "", "config.tr", 0);
    os_uninit();
    os_term(err);
        return err;
}

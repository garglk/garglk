/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcd.h - Header for TADS 2 compiler main
Function
  Main entrypoint for TADS 2 compiler
Notes
  None
Modified
  08/23/98 CNebel     - creation
*/

#ifndef TCD_INCLUDED
#define TCD_INCLUDED

/* Define default memory sizes if no one else has. */
#ifndef TCD_POOLSIZ
# define TCD_POOLSIZ  (6 * 1024)
#endif
#ifndef TCD_LCLSIZ
# define TCD_LCLSIZ   2048
#endif
#ifndef TCD_HEAPSIZ
# define TCD_HEAPSIZ  4096
#endif
#ifndef TCD_STKSIZ
# define TCD_STKSIZ       50
#endif
#ifndef TCD_LABSIZ
# define TCD_LABSIZ   1024
#endif

/* Main entry point for compiler. */
int tcdmain(int argc, char **argv, char *save_ext);

#endif

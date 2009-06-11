/* 
 *   Copyright (c) 1998 by Christopher Nebel.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  argize.h

Function
  Break a string into argc/argv-style arguments.

Notes
  none

Modified
  07/04/98 CNebel     - Created based on older Mac-specific argize source.
*/


#ifndef _ARGIZE_
#define _ARGIZE_

#include <stddef.h>


/*
 *   Return what argc would be if this string were fed to argize.  
 */
int countargs(const char *cmdline);

/*
 *   Break a string <cmdline> into argc/argv arguments, removing quotes in
 *   the process.  Returns 0 on success, 1 if there were too many
 *   arguments to fit into argv.  
 */
int argize(char *cmdline, int * const argc, char *argv[], 
           const size_t argvlen);

#endif

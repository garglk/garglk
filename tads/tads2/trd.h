/* $Header: d:/cvsroot/tads/TADS2/trd.h,v 1.6 1999/07/11 00:46:35 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  trd.h - TADS runtime application definitions
Function
  Defines structures and functions related to the TADS runtime application
Notes
  
Modified
  04/11/99 CNebel     - Move appctx definition to its own header.
  11/25/97 MJRoberts  - Creation
*/

#ifndef TRD_H
#define TRD_H

#include "os.h"
#include "appctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* forward-declare structure types */
struct runcxdef;

/*
 *   Run-time version number 
 */
#define  TADS_RUNTIME_VERSION   "2.5.17"

/*
 *   Main run-time subsystem entrypoint.  Runs the game specified in the
 *   argument vector; does not return until the game terminates.  The
 *   application container context is optional; pass null if no context is
 *   required.  
 */
int trdmain(int argc, char **argv, appctxdef *appctx, char *save_ext);

/*
 *   Main debugger subsystem entrypoint.  Works like trdmain(), but starts
 *   the game under the debugger.  
 */
int tddmain(int argc, char **argv, appctxdef *appctx, char *save_ext);

/*
 *   close and delete the swap file 
 */
void trd_close_swapfile(struct runcxdef *runctx);


#ifdef __cplusplus
}
#endif

/*
 *   Define default memory sizes if no one else has.
 */
#ifndef TRD_HEAPSIZ
# define TRD_HEAPSIZ  4096
#endif
#ifndef TRD_STKSIZ
# define TRD_STKSIZ   200
#endif
#ifndef TRD_UNDOSIZ
# define TRD_UNDOSIZ  (16 * 1024)
#endif


#ifndef TDD_HEAPSIZ
# define TDD_HEAPSIZ  4096
#endif
#ifndef TDD_STKSIZ
# define TDD_STKSIZ   200
#endif
#ifndef TDD_UNDOSIZ
# define TDD_UNDOSIZ  (16 * 1024)
#endif
#ifndef TDD_POOLSIZ
# define TDD_POOLSIZ  (2 * 1024)
#endif
#ifndef TDD_LCLSIZ
# define TDD_LCLSIZ   0
#endif

/*
 *   If the OS headers haven't defined any system-specific option usage
 *   messages, set up a dummy list.  The usage display routine will show
 *   messages starting from the lower number up to and including the higher
 *   number; by default we'll make the ending number lower than the starting
 *   number so that we don't display any messages at all.  
 */
#ifndef ERR_TRUS_OS_FIRST
# define ERR_TRUS_OS_FIRST    100
# define ERR_TRUS_OS_LAST      99
#endif


#endif /* TRD_H */


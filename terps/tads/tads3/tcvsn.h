/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcvsn.h - TADS 3 Compiler version information
Function
  Defines the version number information for the TADS 3 compiler.
  Note that the compiler version numbers need not match the VM or
  interpreter version numbers; we maintain a separate version numbering
  system for the compiler.
Notes
  
Modified
  01/18/00 MJRoberts  - Creation
*/

#ifndef TCVSN_H
#define TCVSN_H

/* compiler major/minor/revision version number */
#define TC_VSN_MAJOR  3
#define TC_VSN_MINOR  1
#define TC_VSN_REV    3

/*
 *   Patch number.  This is an even finer release number that can be used for
 *   bug-fix releases or other releases that don't warrant a revision number
 *   increase, but still must be identified as separate builds.  This is only
 *   shown if it's non-zero.  
 */
#define TC_VSN_PATCH  0

/* 
 *   Development build iteration - this is a sub-sub-sub version number that
 *   we use to indicate unofficial interim builds that don't get their own
 *   release numbers.  We use this to distinguish these interim builds
 *   without incrementing the official release numbers.  This is only
 *   displayed if it's non-zero, and is displayed as a lower-case letter
 *   (1='a' 2='b', etc).  For official releases, this is set to zero.  
 */
#define TC_VSN_DEVBUILD  0

#endif /* TCVSN_H */


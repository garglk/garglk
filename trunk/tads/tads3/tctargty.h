/* $Header: d:/cvsroot/tads/tads3/TCTARGTY.H,v 1.1 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tctarg.h - TADS 3 Compiler Target Selector - target types
Function
  
Notes
  
Modified
  04/30/99 MJRoberts  - Creation
*/

#ifndef TCTARGTY_H
#define TCTARGTY_H

/* ------------------------------------------------------------------------ */
/*
 *   Target Selection - type definitions
 *   
 *   This is a target selector for the target types header.  This follows
 *   the same mechanism that tctarg.h uses - refer to that file for
 *   details.  
 */

/* 
 *   make sure TC_TARGET_DEFINED__ isn't defined, so we can use it to
 *   sense whether we defined a code generator or not 
 */
#undef TCTARGTY_TARGET_DEFINED__

/* ------------------------------------------------------------------------ */
/*
 *   T3 Virtual Machine Code Generator 
 */
#ifdef TC_TARGET_T3
#include "tct3ty.h"
#define TCTARGTY_TARGET_DEFINED__
#endif

/*
 *   Javascript Code Generator 
 */
#ifdef TC_TARGET_JS
#include "tcjsty.h"
#define TCTARGTY_TARGET_DEFINED__
#endif


/* ------------------------------------------------------------------------ */
/*
 *   ensure that a code generator was defined - if not, the compilation
 *   cannot proceed 
 */
#ifndef TCTARGTY_TARGET_DEFINED__
#error No code generator target is defined.  A code generator must be defined in your makefile.  See tctarg.h for details.
#endif


#endif /* TCTARGTY_H */


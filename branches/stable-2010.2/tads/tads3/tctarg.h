/* $Header: d:/cvsroot/tads/tads3/TCTARG.H,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tctarg.h - TADS 3 Compiler Target Selector
Function
  
Notes
  
Modified
  04/30/99 MJRoberts  - Creation
*/

#ifndef TCTARG_H
#define TCTARG_H

/* ------------------------------------------------------------------------ */
/*
 *   Target Selection.
 *   
 *   SEE ALSO: tctargty.h - target type header selector
 *   
 *   As the parser scans the source stream, it constructs a parse tree
 *   that represent the parsed form of the source.  After the parsing
 *   pass, the parse tree contains all of the information necessary to
 *   generate code for the translation.
 *   
 *   The parse tree objects are actually the code generators.  So, as the
 *   scanner parses the source file, it must create parse tree objects for
 *   the appropriate target architecture.  However, we want to keep the
 *   scanner independent of the target architecture -- we want the same
 *   scanner to be usable for any target architecture for which we can
 *   provide a code generator.
 *   
 *   To accomplish this, we define a base class for parse tree nodes; the
 *   scanner is only interested in this base class interface.  Then, for
 *   each target architecture, we create a subclass of each parse tree
 *   node that contains the code generator for that node type for the
 *   target.
 *   
 *   However, the scanner must still know enough to create the appropriate
 *   subclass of each parse tree node.  This file contains the target
 *   selection switch that mediates the independence of the scanner from
 *   the target code generator, but still allows the scanner to create the
 *   correct type of parse tree nodes for the desired target.  For each
 *   parse tree node type that the scanner must create, we #define a
 *   generic symbol to a target-specific subclass.  The scanner uses the
 *   generic symbol, but we expand the macro when compiling the compiler
 *   to the correct target.
 *   
 *   Because the target selection is made through macros, the target
 *   architecture is fixed at compile time.  However, the same scanner
 *   source code can be compiled into multiple target compilers.  
 */

/*
 *   Before including this file, #define the appropriate target type.
 *   This should usually be done in the makefile, since this is a
 *   compile-time selection.
 *   
 *   To add a new code generator, define the appropriate subclasses in a
 *   new file.  Add a new #ifdef-#include sequence below that includes the
 *   subclass definitions for your code generator.
 *   
 *   Note that each target uses the same names for the final subclasses.
 *   We choose the target at link time when building the compiler
 *   executable, so we must compile and link only a single target
 *   architecture into a given build of the compiler.  
 */

/* 
 *   make sure TC_TARGET_DEFINED__ isn't defined, so we can use it to
 *   sense whether we defined a code generator or not
 */
#undef TCTARG_TARGET_DEFINED__


/* ------------------------------------------------------------------------ */
/*
 *   T3 Virtual Machine Code Generator 
 */
#ifdef TC_TARGET_T3
#include "tct3.h"
#define TCTARG_TARGET_DEFINED__
#endif

/* ------------------------------------------------------------------------ */
/*
 *   JavaScript code generator
 */
#ifdef TC_TARGET_JS
#include "tcjs.h"
#define TCTARG_TARGET_DEFINED__
#endif


/* ------------------------------------------------------------------------ */
/*
 *   ensure that a code generator was defined - if not, the compilation
 *   cannot proceed 
 */
#ifndef TCTARG_TARGET_DEFINED__
#error No code generator target is defined.  A code generator must be defined in your makefile.  See tctarg.h for details.
#endif


#endif /* TCTARG_H */


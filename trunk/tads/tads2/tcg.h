/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcg.h - stub for tcg routines
Function
  Declaration of TADS/Graphic functions.
Notes
  These routines do nothing; they are merely to allow linking the
  normal TADS/Compiler without having to have separate drivers for
  the graphical and non-graphical versions.
*/

#ifndef TCG_H
#define TCG_H

#include "os.h"
#include "err.h"
#include "prs.h"

/* tcgcomp - graphical phase of compilation: do nothing in text version */
void tcgcomp(errcxdef *ec, prscxdef *pctx, const char *infile);

#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcgdum.c - stub for tcg routines
Function
  Provides link-time resolution of tcg symbols
Notes
  These routines do nothing; they are merely to allow linking the
  normal TADS/Compiler without having to have separate drivers for
  the graphical and non-graphical versions.
Modified
  04/11/99 CNebel        - Improve const-ness; use new headers.
  12/16/92 MJRoberts     - creation
*/

#include "os.h"
#include "err.h"
#include "prs.h"
#include "tcg.h"

/* name of the compiler (for banner) - this is the normal text compiler */
char tcgname[] = "TADS Compiler";

/* tcgcomp - graphical phase of compilation: do nothing in text version */
void tcgcomp(errcxdef *ec, prscxdef *pctx, const char *infile)
{
}


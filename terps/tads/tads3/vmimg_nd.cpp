#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmimg_nd.cpp - T3 VM image file reader - NON-DEBUG implementation
Function
  This file implements certain image file reader functions for 
  non-debug versions of the interpreter.  This implementation allows
  linking a smaller version of the interpreter executable that doesn't
  require a bunch of functionality only needed for an interactive
  debugger.
Notes
  
Modified
  12/03/99 MJRoberts  - Creation
*/

#include <stdlib.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmimage.h"
#include "vmrunsym.h"


/* ------------------------------------------------------------------------ */
/*
 *   load a Method Header List block - this block is needed only for
 *   interactive debugging, so we can simply ignore this information 
 */
void CVmImageLoader::load_mhls(VMG_ ulong siz)
{
    /* skip the block */
    fp_->skip_ahead(siz);
}

/* ------------------------------------------------------------------------ */
/*
 *   load a Global Symbols block
 */
void CVmImageLoader::load_gsym(VMG_ ulong siz)
{
    /* load the data into the runtime reflection symbol table */
    load_runtime_symtab_from_gsym(vmg_ siz);
}

/*
 *   Fix up the global symbol table 
 */
void CVmImageLoader::fix_gsym_meta(VMG0_)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   load a Macro Symbols block - this block is used only in interactive
 *   debugging, so simply ignore this information in this implementation 
 */
void CVmImageLoader::load_macros(VMG_ ulong siz)
{
    /* load the data into the runtime reflection macro table */
    load_runtime_symtab_from_macr(vmg_ siz);
}

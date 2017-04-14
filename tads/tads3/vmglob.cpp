#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmglob.cpp,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmglob.cpp - global definitions
Function
  Defines the global variables.
Notes
  
Modified
  11/28/98 MJRoberts  - Creation
*/

/* actually define the variables (i.e., don't make them 'extern') */
#define VMGLOB_DECLARE

/* include the globals header */
#include "t3std.h"
#include "vmglob.h"

/* and some other headers that have special global definitions */
#include "vmerr.h"
#include "vmstack.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   In the VARS configuration, we need to provide storage for all of the
 *   variables.  
 */
#ifdef VMGLOB_VARS

/* we need to include headers for objects we define in-line */
#include "vmrun.h"
#include "vmstack.h"
#include "vmpool.h"
#include "vmparam.h"
#include "vmpredef.h"
#include "vminit.h"
#include "vmtobj.h"

/* remove the declaring macros for the globals */
#undef VM_GLOBAL_OBJDEF
#undef VM_GLOBAL_PREOBJDEF
#undef VM_GLOBAL_PRECOBJDEF
#undef VM_GLOBAL_VARDEF
#undef VM_GLOBAL_ARRAYDEF

/* provide new defining macros for the globals */
#define VM_GLOBAL_OBJDEF(typ, var) typ *G_##var##_X;
#define VM_GLOBAL_PREOBJDEF(typ, var) typ G_##var##_X;
#define VM_GLOBAL_PRECOBJDEF(typ, var, ctor_args) typ G_##var##_X ctor_args;
#define VM_GLOBAL_VARDEF(typ, var) typ G_##var##_X;
#define VM_GLOBAL_ARRAYDEF(typ, var, eles) typ G_##var##_X[eles];

/* include the variable definitions */
#include "vmglobv.h"


#endif /* VMGLOB_VARS */

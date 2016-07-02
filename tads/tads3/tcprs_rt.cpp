#ifdef RCSID
static char RCSid[] =
    "$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcprs_rt.cpp - stubs for parser functions not needed in run-time 
Function
  Interpreters that include "eval()" functionality require a portion of the
  compiler, including code body and expression compilation.  This module
  provides stubs for functions that aren't needed for interpreter
  configurations, but which are referenced from the parts of the compiler we
  do need, to resolve symbols at link time.  Many of these stubs are simply
  never called in interpreter builds, because the conditions that would
  trigger their invocation can't occur; others just need to provide defaults
  or other simplified functionality when called; others generate errors,
  because the functionality they provide is specifically not supported in
  interpreter builds.
Notes

Modified
  02/02/00 MJRoberts  - Creation
*/

#include "tctarg.h"
#include "tcprs.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "tctok.h"


/* ------------------------------------------------------------------------ */
/*
 *   Simple stubs for functions not used in the dynamic compiler 
 */

/*
 *   Debug records aren't needed when we're compiling code in the debugger
 *   itself - debugger-generated code can't itself be debugged 
 */
void CTPNStmBase::add_debug_line_rec()
{
}

void CTPNStmBase::add_debug_line_rec(CTcTokFileDesc *, long)
{
}

/*
 *   Add an entry to the global symbol table.  We can't define new global
 *   symbols in an interactive debug session, so this does nothing.  
 */
void CTcPrsSymtab::add_to_global_symtab(CTcPrsSymtab *, CTcSymbol *)
{
}

int CTPNStmObjectBase::parse_nested_obj_prop(
    CTPNObjProp* &, int *, tcprs_term_info *, const CTcToken *, int)
{
    return FALSE;
}


/*
 *   Set the sourceTextGroup mode.  This doesn't apply in the debugger, since
 *   #pragma isn't allowed.
 */
void CTcParser::set_source_text_group_mode(int)
{
}

/* 
 *   we don't need static object generation, since we use G_vmifc interface
 *   to generate live objects in the VM 
 */
CTcSymObj *CTcParser::add_gen_obj_stat(CTcSymObj *)
{
    return 0;
}

void CTcParser::add_gen_obj_prop_stat(
    CTcSymObj *, CTcSymProp *, const CTcConstVal *)
{
}


// NOTE - this file is obsolete as of TADS 3.1.2
#error This file is obsolete - it should be removed from the makefile.  \
       The only dependency should have been test/test_tok.cpp, which now \
       has the encessary dummy functions defined internally.

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
  [OBSOLETE] tcprs_d.cpp - stubs for parser functions not needed in debugger
Function
  
Notes
  
Modified
  02/02/00 MJRoberts  - Creation
*/

#include "tctarg.h"
#include "tcprs.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "tctok.h"


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
 *   Anonymous functions are illegal in the debugger 
 */
CTcPrsNode *CTcPrsOpUnary::parse_anon_func(
    int /*short_form*/, int /*is_method*/)
{
    /* 
     *   we can't parse these - generate an error, consume the token, and
     *   return failure 
     */
    G_tok->log_error(TCERR_DBG_NO_ANON_FUNC);
    G_tok->next();
    return 0;
}

/*
 *   local contexts aren't needed in the debugger, because anonymous
 *   functions aren't allowed 
 */
void CTcParser::init_local_ctx()
{
}

int CTcParser::alloc_ctx_arr_idx()
{
    return 0;
}

void CTPNStmObjectBase::add_implicit_constructor()
{
}

/*
 *   Add an entry to the global symbol table.  We can't define new global
 *   symbols in an interactive debug session, so this does nothing.  
 */
void CTcPrsSymtab::add_to_global_symtab(CTcPrsSymtab *, CTcSymbol *)
{
}

/*
 *   Set the sourceTextGroup mode.  This doesn't apply in the debugger, since
 *   #pragma isn't allowed.
 */
void CTcParser::set_source_text_group_mode(int)
{
}

CTcPrsNode *CTPNStmLabelBase::fold_constants(CTcPrsSymtab *)
{
    return 0;
}
unsigned long CTPNStmLabelBase::get_control_flow(int) const
{
    return 0;
}


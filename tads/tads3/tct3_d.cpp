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
  tct3_d.cpp - stubs for functions not needed in debugger builds
Function
  
Notes
  
Modified
  02/02/00 MJRoberts  - Creation
*/

#include "tctarg.h"
#include "tcprs.h"

/*
 *   Build the debug table - not required when we're compiling code in the
 *   debugger, since debugger-generated code can't itself be debugged 
 */
void CTPNCodeBody::build_debug_table(ulong)
{
}

void CTPNObjProp::check_locals()
{
}


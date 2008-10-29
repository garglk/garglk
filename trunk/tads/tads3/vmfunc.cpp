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
  vmfunc.cpp - T3 VM function header operations
Function
  
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/

#include "vmfunc.h"

/* ------------------------------------------------------------------------ */
/* 
 *   Set up an exception table pointer for this function.  Returns true if
 *   successful, false if there's no exception table.  
 */
int CVmFuncPtr::set_exc_ptr(CVmExcTablePtr *exc_ptr) const
{
    /* set the exception pointer based on my address */
    return exc_ptr->set(p_);
}

/*
 *   Set up a debug table pointer for this function.  Returns true if
 *   successful, false if there's no debug table.  
 */
int CVmFuncPtr::set_dbg_ptr(CVmDbgTablePtr *dbg_ptr)const
{
    /* set the debug pointer based on my address */
    return dbg_ptr->set(p_);
}

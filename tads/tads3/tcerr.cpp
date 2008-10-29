#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCERR.CPP,v 1.5 1999/07/11 00:46:52 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcerr.cpp - TADS 3 Compiler Messages
Function
  
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/

#include "tcerr.h"
#include "tcerrnum.h"

/* ------------------------------------------------------------------------ */
/*
 *   Look up a message 
 */
const char *tcerr_get_msg(int msgnum, int verbose)
{
    const char *msg;
    
    /* look up the message in the compiler message array */
    msg = err_get_msg(tc_messages, tc_message_count,
                      msgnum, verbose);
    if (msg != 0)
        return msg;

    /* look up the message in the interpreter message array */
    msg = err_get_msg(vm_messages, vm_message_count,
                      msgnum, verbose);
    if (msg != 0)
        return msg;

    /* there's nowhere else to look - return failiure */
    return 0;
}


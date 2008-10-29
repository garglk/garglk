/* $Header: d:/cvsroot/tads/tads3/TCERR.H,v 1.3 1999/05/17 02:52:27 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcerr.h - TADS 3 Compiler Error Management
Function
  
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/

#ifndef TCERR_H
#define TCERR_H

#include "vmerr.h"

/*
 *   Error Severity Levels.  
 */
enum tc_severity_t
{
    /* information only - not an error */
    TC_SEV_INFO,

    /* 
     *   Pedantic warning - doesn't prevent compilation from succeeding,
     *   and doesn't even necessarily indicate anything is wrong.  We use
     *   a separate severity level for these because some users will want
     *   to be able to filter these out.  
     */
    TC_SEV_PEDANTIC,

    /* 
     *   warning - does not prevent compilation from succeeding, but
     *   indicates a potential problem 
     */
    TC_SEV_WARNING,

    /* 
     *   error - compilation cannot be completed successfully, although it
     *   may be possible to continue with compilation to the extent
     *   necessary to check for any additional errors 
     */
    TC_SEV_ERROR,

    /* fatal - compilation must be immediately aborted */
    TC_SEV_FATAL,

    /* internal error - compilation must be immediately aborted */
    TC_SEV_INTERNAL
};


/* English version of message array */
extern const err_msg_t tc_messages_english[];
extern size_t tc_message_count_english;

/* error message array */
extern const err_msg_t *tc_messages;
extern size_t tc_message_count;

/* look up a message */
const char *tcerr_get_msg(int msgnum, int verbose);

#endif /* TCERR_H */


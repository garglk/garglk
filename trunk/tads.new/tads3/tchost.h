/* $Header: d:/cvsroot/tads/tads3/TCHOST.H,v 1.2 1999/05/17 02:52:27 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tchost.h - TADS 3 Compiler Host System Interface
Function
  The host system interface is an abstract interface to services
  provided by the compiler host system.
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/

#ifndef TCHOST_H
#define TCHOST_H

#include <stdarg.h>
#include "tcerr.h"

/* ------------------------------------------------------------------------ */
/*
 *   Abstract Host Interface 
 */
class CTcHostIfc
{
public:
    virtual ~CTcHostIfc() { }

    /* 
     *   Display a message, taking varargs parameters.  The message is a
     *   UTF-8 string.  The message string is a printf-style format
     *   string.  This is used to display informational messages.  
     */
    void print_msg(const char *msg, ...)
    {
        va_list args;

        va_start(args, msg);
        v_print_msg(msg, args);
        va_end(args);
    }

    /* display a message with va_list-style varargs */
    virtual void v_print_msg(const char *msg, va_list args) = 0;

    /*
     *   Display a process step message.  These work the same as
     *   print_msg() and v_print_msg(), respectively, but display a
     *   message indicating a process step.  Some implementations might
     *   want to display process step messages in a special manner; for
     *   example, a GUI implementation might put the message in a status
     *   bar or similar type of display rather than intermixed with other
     *   messages.  
     */
    void print_step(const char *msg, ...)
    {
        va_list args;

        va_start(args, msg);
        v_print_step(msg, args);
        va_end(args);
    }

    /* display a process step message with va_list-style varargs */
    virtual void v_print_step(const char *msg, va_list args) = 0;

    /* 
     *   Display an error message.  These work the same way as print_msg()
     *   and v_print_msg(), respectively, but display error messages
     *   instead of informational messages.  Some implementations might
     *   want to direct the different types of messages to different
     *   places; for example, a stdio implementation may want to send
     *   print_msg messages to stdout, but print_err messages to stderr.  
     */
    void print_err(const char *msg, ...)
    {
        va_list args;

        va_start(args, msg);
        v_print_err(msg, args);
        va_end(args);
    }

    /* display an error with va_list arguments */
    virtual void v_print_err(const char *msg, va_list args) = 0;
};


#endif /* TCHOST_H */


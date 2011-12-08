/* $Header: d:/cvsroot/tads/tads3/TCHOSTSI.H,v 1.2 1999/05/17 02:52:27 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tchostsi.h - stdio implementation of host interface
Function
  
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/

#ifndef TCHOSTSI_H
#define TCHOSTSI_H

#include "t3std.h"
#include "tchost.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   stdio host interface 
 */
class CTcHostIfcStdio: public CTcHostIfc
{
public:
    CTcHostIfcStdio()
    {
        /* by default, show progress messages */
        show_progress_ = TRUE;

        /* there's no status prefix string yet */
        status_prefix_ = 0;
    }

    ~CTcHostIfcStdio()
    {
        /* free our status prefix string */
        lib_free_str(status_prefix_);
    }

    /* set the status message prefix string */
    void set_status_prefix(const char *str)
    {
        /* delete any former prefix string */
        lib_free_str(status_prefix_);

        /* store a copy of the string */
        status_prefix_ = lib_copy_str(str);
    }

    /* display information messages */
    void v_print_msg(const char *msg, va_list args);

    /* display a process step message */
    void v_print_step(const char *msg, va_list args);

    /* display error messages */
    void v_print_err(const char *msg, va_list args);

    /* turn status (step) messages on/off */
    void set_show_progress(int flag) { show_progress_ = flag; }

protected:
    /* 
     *   internal display routine - formats the message, converts it to
     *   the console character set and displays the result on the standard
     *   output 
     */
    void v_printf(const char *msg, va_list args);

    /* flag: show progress (step) messages */
    int show_progress_;

    /* status prefix message */
    char *status_prefix_;
};

#endif /* TCHOSTSI_H */

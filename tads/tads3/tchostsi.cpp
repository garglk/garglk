#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCHOSTSI.CPP,v 1.2 1999/05/17 02:52:27 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tchostsi.cpp - stdio implementation of host interface
Function
  
Notes
  
Modified
  04/22/99 MJRoberts  - Creation
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "tchostsi.h"
#include "charmap.h"
#include "tcmain.h"
#include "tcglob.h"


/* ------------------------------------------------------------------------ */
/*
 *   stdio host interface 
 */

/* 
 *   display a message 
 */
void CTcHostIfcStdio::v_print_msg(const char *msg, va_list args)
{
    /* display the message on the standard output */
    v_printf(msg, args);
}

/* 
 *   display a process step message
 */
void CTcHostIfcStdio::v_print_step(const char *msg, va_list args)
{
    /* if we're showing progress messages, proceed */
    if (show_progress_)
    {
        /* show the status message prefix (use a tab by default) */
        printf("%s", status_prefix_ != 0 ? status_prefix_ : "\t");

        /* display the message on the standard output */
        v_printf(msg, args);
    }
}

/* 
 *   display an error message
 */
void CTcHostIfcStdio::v_print_err(const char *msg, va_list args)
{
    /* display the message on the standard error output */
    v_printf(msg, args);
}

/*
 *   display a message - internal interface; this routine formats the
 *   message and converts it to the console character set for display 
 */
void CTcHostIfcStdio::v_printf(const char *msg, va_list args)
{
    char buf[1024];
    
    /* format the message to our internal buffer */
    t3vsprintf(buf, sizeof(buf), msg, args);

    /* translate to the local character set if we have a mapper */
    if (G_tcmain->get_console_mapper() != 0)
    {
        char xlat_buf[1024];
        utf8_ptr p;
        size_t len;

        /* translate it */
        p.set(buf);
        len = G_tcmain->get_console_mapper()
              ->map_utf8z_esc(xlat_buf, sizeof(xlat_buf), p,
                              &CCharmapToLocal::source_esc_cb);

        /* if there's room, null-terminate the buffer */
        if (len < sizeof(xlat_buf))
            xlat_buf[len] = '\0';
        
        /* display it on the standard output */
        printf("%s", xlat_buf);
    }
    else
    {
        /* there's no character mapper - display the untranslated buffer */
        printf("%s", buf);
    }

    /* 
     *   Flush the standard output immediately.  If our output is being
     *   redirected to a pipe that another program is reading, this will
     *   ensure that the other program will see our output as soon as we
     *   produce it, which might be important in some cases.  In any case,
     *   we don't generally produce so much console output that this should
     *   significantly affect performance.  
     */
    fflush(stdout);
}


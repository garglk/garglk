#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2011 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmlog.cpp - system log file
Function
  
Notes
  
Modified
  12/01/11 MJRoberts  - Creation
*/

#include <time.h>

#include "t3std.h"
#include "vmglob.h"
#include "vmlog.h"
#include "vmfile.h"
#include "vmdatasrc.h"
#include "charmap.h"


/* ------------------------------------------------------------------------ */
/*
 *   System log file 
 */

/*
 *   Log a message with sprintf-style formatting 
 */
void vm_log_fmt(VMG_ const char *fmt, ...)
{
    /* format the message */
    va_list args;
    va_start(args, fmt);
    char *str = t3vsprintf_alloc(fmt, args);
    va_end(args);

    /* log the message */
    vm_log(vmg_ str, strlen(str));

    /* done with the string */
    t3free(str);
}

/*
 *   Log a string with a given length
 */
void vm_log(VMG_ const char *str, size_t len)
{
    /* open the system log file */
    osfildef *fp = osfoprwt(G_syslogfile, OSFTTEXT);
    if (fp != 0)
    {
        /* wrap it in a data source */
        CVmFileSource ds(fp);

        /* get a printable timestamp */
        os_time_t timer = os_time(0);
        struct tm *tblk = os_localtime(&timer);
        char *tmsg = asctime(tblk);

        /* remove the trailing '\n' from the asctime message */
        size_t tmsgl = strlen(tmsg);
        if (tmsgl > 0 && tmsg[tmsgl-1] == '\n')
            tmsg[--tmsgl] = '\0';

        /* build the full message: [<timestamp>] <message> <newline> */
        char *msg = t3sprintf_alloc("[%s] %.*s\n", tmsg, (int)len, str);
        size_t msglen = strlen(msg);

        /* seek to the end of the file */
        ds.seek(0, OSFSK_END);

        /* if we can convert to a local character set, do so */
        if (G_cmap_to_log != 0)
        {
            /* write the message in the local character set */
            G_cmap_to_file->write_file(&ds, msg, msglen);
        }
        else
        {
            /* write the message with no character set conversion */
            (void)osfwb(fp, msg, msglen);
        }

        /* done with the formatted text string */
        t3free(msg);
    }
}



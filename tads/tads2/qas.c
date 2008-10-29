/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All rights reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  qas  - qa scripter
Function
  Allows TADS to read part or all of the commands from a session from a
  file.
Notes
  Some operating systems (e.g., Mac) obtain user input in ways that don't
  involve the command line.  For these systems to work properly, the os_xxx
  routines that invoke other input methods must be "qa scripter aware"; for
  example, the Mac os_askfile() routine must put the filename it gets back
  in the command log file, or must read directly from the command log file,
  or both.
Modified
  03/10/91 MJRoberts   - created
*/

#include <stdio.h>
#include <string.h>
#include "os.h"
#include "run.h"
#include "tio.h"

/*
 *  Globals for the script reader
 */
osfildef *scrfp = (osfildef *)0;                             /* script file */
int scrquiet = 0;             /* flag: true ==> script is NOT shown as read */

/*
 *   open script file
 */
int qasopn(char *scrnam, int quiet)
{
    if (scrfp) return 1;                     /* already reading from script */
    if ((scrfp = osfoprt(scrnam, OSFTCMD)) == 0) return 1;
    scrquiet = quiet;
    return 0;
}

/*
 *   close script file
 */
void qasclose()
{
    /* only close the script file if there's one open */
    if (scrfp)
    {
        osfcls(scrfp);
        scrfp = 0;                                   /* no more script file */
        scrquiet = 0;
    }
}

/*
 *   Read the next line from the script file (this is essentially the
 *   script-redirected os_gets).  Only lines starting with '>' are
 *   considered script input lines; all other lines are comments, and are
 *   ignored.  
 */
char *qasgets(char *buf, int bufl)
{
    /* shouldn't be here at all if there's no script file */
    if (scrfp == 0)
        return 0;

    /* update status line */
    runstat();

    /* keep going until we find something we like */
    for (;;)
    {
        char c;
        
        /*
         *   Read the next character of input.  If it's not a newline,
         *   there's more on the same line, so read the rest and see what
         *   to do.  
         */
        c = osfgetc(scrfp);
        if (c != '\n' && c != '\r')
        {
            /* read the rest of the line */
            if (!osfgets(buf, bufl, scrfp))
            {
                /* end of file:  close the script and return eof */
                qasclose();
                return 0;
            }
            
            /* if the line started with '>', strip '\n' and return line */
            if (c == '>')
            {
                int l;

                /* remove the trailing newline */
                if ((l = strlen(buf)) > 0
                    && (buf[l-1] == '\n' || buf[l-1] == '\r'))
                    buf[l-1] = 0;

                /* 
                 *   if we're not in quiet mode, echo the command to the
                 *   display 
                 */
                if (!scrquiet)
                    outformat(buf);

                /* flush the current line without adding any blank lines */
                outflushn(1);

                /* return the command */
                return buf;
            }
        }
        else if (c == EOF )
        {
            /* end of file - close the script and return eof */
            qasclose();
            return 0;
        }
    }
}

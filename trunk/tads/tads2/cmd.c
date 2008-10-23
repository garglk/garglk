#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/CMD.C,v 1.2 1999/05/17 02:52:11 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  cmd.c - command line option reader
Function
  service functions for interpreting command line options
Notes
  none
Modified
  04/04/92 MJRoberts     - creation
*/

#include "os.h"
#include "std.h"
#include "err.h"
#include "cmd.h"

/* get a toggle argument */
int cmdtog(errcxdef *ec, int prv, char *argp, int ofs,
           void (*usagefn)(errcxdef *))
{
    switch(argp[ofs + 1])
    {
    case '+':
        return(TRUE);
        
    case '-':
        return(FALSE);
        
    case '\0':
        return(!prv);
        
    default:
        /* invalid - display usage if we have a callback for it */
        if (usagefn != 0)
            (*usagefn)(ec);
        NOTREACHEDV(int);
        return 0;
    }
}

/* get an argument to a switch */
char *cmdarg(errcxdef *ec, char ***argpp, int *ip, int argc, int ofs,
             void (*usagefn)(errcxdef *))
{
    char *ret;

    /* 
     *   check to see if the argument is appended directly to the option;
     *   if not, look at the next string 
     */
    ret = (**argpp) + ofs + 1;
    if (*ret == '\0')
    {
        /* 
         *   it's not part of this string - get the argument from the next
         *   string in the vector 
         */
        ++(*ip);
        ++(*argpp);
        ret = (*ip >= argc ? 0 : **argpp);
    }

    /* 
     *   if we didn't find the argument, it's an error - display usage if
     *   we have a valid usage callback
     */
    if ((ret == 0 || *ret == 0) && usagefn != 0)
        (*usagefn)(ec);

    return ret;
}


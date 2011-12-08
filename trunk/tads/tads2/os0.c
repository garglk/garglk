#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OS0.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os0.c - argument configuration utility
Function
  This module allows a "main" routine to get additional arguments
  from a configuration file.  We look for the given configuration
  file in the current directory, and if not found, in the executable's
  directory.  The arguments it contains are added before the command
  line arguments, except that any argument flags (along with an extra
  argument string, if any) listed in the "before" list are moved to
  the beginning of the argument list.  For example:

     before list = "i"
     config file contains "-m 128000 -1+ -tf- -i /tads/include"
     command line contains "-1- -i ../include -o test.gam test.t"
     translation:  "-i ../include -m 128000 -1+ -f- -i /tads/include
                    -o test.gam test.t"

   This allows arguments to be sorted according to precedence so that
   an argument in the command line overrides an argument in the config
   file.  The "before" list contains options that need to come first
   to override subsequent instances of the same option; other options
   override by coming later in the string.
Notes
  
Modified
  04/04/99 CNebel        - Use new argize function; fix Metrowerks errors.
  04/24/93 JEras         - use new os_locate() to find config file
  04/22/92 MJRoberts     - creation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "os.h"
#include "appctx.h"
#include "argize.h"


static int os0main_internal(int oargc, char **oargv,
                            int (*mainfn1)(int, char **, char *),
                            int (*mainfn2)(int, char **, struct appctxdef *,
                                           char *), 
                            const char *before, const char *config,
                            struct appctxdef *appctx)
{
    int       argc = 0;
    int       fargc = 0;
    char    **argv;
    char     *configbuf = 0;
    int       i;
    osfildef *fp;
    long      fsiz;
    char      buf[256];
    char     *save_ext = 0;
    int       ret;

    /*
     *   Check for an embedded SAVX resource, which specifies the default
     *   saved game file suffix. 
     */
    if (oargv != 0 && oargv[0] != 0
        && (fp = os_exeseek(oargv[0], "SAVX")) != 0)
    {
        unsigned short len;
        
        /* read the length and then read the name */
        if (!osfrb(fp, &len, sizeof(len))
            && len < sizeof(buf)
            && !osfrb(fp, buf, len))
        {
            /* null-terminate the value */
            buf[len] = '\0';

            /* tell the OS layer about it */
            os_set_save_ext(buf);

            /* save a copy of the extension */
            save_ext = (char *)osmalloc(len + 1);
            strcpy(save_ext, buf);
        }

        /* done with it */
        osfcls(fp);
        fp = 0;
    }

    /*
     *   Try for an embedded configuration file.  If we don't find one,
     *   try locating an external configuration file. 
     */
    if (oargv != 0 && oargv[0] != 0
        && (fp = os_exeseek(oargv[0], "RCFG")) != 0)
    {
        (void)osfrb(fp, &fsiz, sizeof(fsiz));
    }
    else if (config != 0
             && os_locate(config, (int)strlen(config), oargv[0], buf,
                       (size_t)sizeof(buf))
             && (fp = osfoprb(buf, OSFTTEXT)) != 0)
    {
        osfseek(fp, 0L, OSFSK_END);
        fsiz = osfpos(fp);
        osfseek(fp, 0L, OSFSK_SET);
    }
    else
    {
        fsiz = 0;
        fp = 0;
    }

    /* read the file if we found anything */
    if (fsiz != 0)
    {
        configbuf = (char *)osmalloc((size_t)(fsiz + 1));
        if (configbuf != 0)
        {
            /* read the configuration file into the buffer */
            (void)osfrb(fp, configbuf, (size_t)fsiz);

            /* null-terminate it */
            configbuf[fsiz] = '\0';

            /* count arguments */
            fargc = countargs(configbuf);
        }
    }

    /* close the file if we opened one */
    if (fp != 0)
        osfcls(fp);

    /* allocate space for the argument vector */
    argv = (char **)osmalloc((size_t)((oargc + fargc + 1) * sizeof(*argv)));

    /* first argument is always original argv[0] */
    argv[argc++] = oargv[0];

    /* put all user -i flags next */
    for (i = 0 ; i < oargc ; ++i)
    {
        if (oargv[i][0] == '-' && strchr(before, oargv[i][1]))
        {
            argv[argc++] = oargv[i];
            if (oargv[i][2] == '\0' && i+1 < oargc) argv[argc++] = oargv[++i];
        }
    }

    /* put all config file flags next */
    if (configbuf != 0)
    {
        int ret, foundargc;

        /* parse the arguments */
        ret = argize(configbuf, &foundargc, &argv[argc], fargc);

        /* add them into our arguments */
        argc += foundargc;

        /* make sure we scanned the same number of args we anticipated */
        assert(ret == 0);
        assert(foundargc == fargc);
    }

    /* put all user parameters other than -i flags last */
    for (i = 1 ; i < oargc ; ++i)
    {
        if (oargv[i][0] == '-' && strchr(before, oargv[i][1]))
        {
            if (oargv[i][2] == '\0' && i+1 < oargc) ++i;
        }
        else
        {
            argv[argc++] = oargv[i];
        }
    }

    /* call appropriate real main with the modified argument list */
    if (mainfn1)
        ret = (*mainfn1)(argc, argv, save_ext);
    else
        ret = (*mainfn2)(argc, argv, appctx, save_ext);

    /* forget our saved extension */
    if (save_ext != 0)
        osfree(save_ext);

    /* return the result */
    return ret;
}


/*
 *   Old-style os0main: call with main function that doesn't take a host
 *   container application context 
 */
int os0main(int oargc, char **oargv,
            int (*mainfn)(int, char **, char *),
            const char *before, const char *config)
{
    return os0main_internal(oargc, oargv, mainfn, 0, before, config, 0);
}

/*
 *   New-style os0main: call with main function that takes a host
 *   container application context 
 */
int os0main2(int oargc, char **oargv,
             int (*mainfn)(int, char **, struct appctxdef *, char *),
             const char *before, const char *config,
             struct appctxdef *appctx)
{
    return os0main_internal(oargc, oargv, 0, mainfn, before, config, appctx);
}


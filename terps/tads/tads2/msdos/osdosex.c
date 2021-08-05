#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/OSDOSEX.C,v 1.2 1999/05/17 02:52:18 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdosex.c - external function implementation for DOS
Function
  
Notes
  
Modified
  11/02/97 MJRoberts  - Creation
*/

#include <string.h>
#ifndef DJGPP
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <share.h>
#endif /* DJGPP */

#include "os.h"
#include <assert.h>

#if defined(__DPMI16__) || defined(T_WIN32)
# include <windows.h>
#endif

/* forward declarations */
static const char *oss_get_exe(const char *argv0);


#ifdef DJGPP

/*
 *   os_exeseek - opens the given .EXE file and seeks to the end of the
 *   executable part of it, on the presumption that a datafile is to be
 *   found there.  
 */
osfildef *os_exeseek(const char *exefile, const char *typ)
{
    return((osfildef *)0);
}

/* retrieve the full name of the program, given argv[0] */
static const char *oss_get_exe(const char *argv0)
{
    /* 
     *   djgpp reliably passes the fully-qualified path to the executable in
     *   argv[0], so we can simply return argv[0] 
     */
    return argv0;
}

#else /* DJGPP */

/*
 *   os_exeseek - opens the given .EXE file and seeks to the end of the
 *   executable part of it, on the presumption that a datafile is to be
 *   found there.  
 */
osfildef *os_exeseek(const char *argv0, const char *typ)
{
    const char *exefile;
    osfildef *fp;
    int       tags_found;
    unsigned  ofs, segcnt;
    unsigned  long seekpt;
    unsigned  long check;
    unsigned  long startofs;
    unsigned  long end;
    char      typbuf[4];
    static    int no_data_present = FALSE;
    static    int tried_old_way = FALSE;

    /* 
     *   if we've already looked here and found no data, don't bother trying
     *   again 
     */
    if (no_data_present)
        return 0;

    /* get the name of the executable file from the given argv0 */
    exefile = oss_get_exe(argv0);

    /* open the file */
    if (!(fp = _fsopen(exefile, "rb", SH_DENYWR)))
        return((FILE *)0);

    /* seek to the end of the file */
    fseek(fp, 0L, SEEK_END);

    /* look through tagged blocks until we find the type we want */
    for (tags_found = 0 ; ; ++tags_found)
    {
        /* seek back past the descriptor block */
        fseek(fp, -12L, SEEK_CUR);
        seekpt = ftell(fp);

        /* read the elements of the descriptor block */
        if (fread(&check, sizeof(check), 1, fp) != 1
            || fread(typbuf, sizeof(typbuf), 1, fp) != 1
            || fread(&startofs, sizeof(startofs), 1, fp) != 1)
            break;

        /* check the signature to make sure we're looking at a valid block */
        if (check != ~seekpt)
            break;

        /* seek to the start of the data for this resource */
        fseek(fp, startofs, SEEK_SET);

        /* if this is the one we want, return it, otherwise keep looking */
        if (!memcmp(typ, typbuf, sizeof(typbuf)))
        {
            /* check the header to make sure it matches */
            if (fread(&check, sizeof(check), 1, fp) != 1
                || fread(typbuf, sizeof(typbuf), 1, fp) != 1
                || check != ~startofs
                || memcmp(typ, typbuf, sizeof(typbuf)))
                break;
            return fp;
        }
    }

    /*
     *   We didn't find it - if the caller is asking for the game file
     *   segment ("TGAM"), try the old way, in case this got built with the
     *   old maketrx program.  If we've already tried this before in this
     *   session, don't bother doing so again, since nothing's going to
     *   change.  
     */
    if (!tried_old_way)
    {
        /* note that we've tried this now */
        tried_old_way = TRUE;

        /* read the header */
        fseek(fp, 0L, SEEK_SET);
        if (fread(&ofs, sizeof(ofs), 1, fp) != 1 ||
            fread(&ofs, sizeof(ofs), 1, fp) != 1 ||
            fread(&segcnt, sizeof(segcnt), 1, fp) != 1)
            goto no_data;
        
        seekpt = ((unsigned long)segcnt - 1)*512L + (unsigned long)ofs;
        if (fseek(fp, seekpt, SEEK_SET)) goto no_data;
        
        /* check for the signature, to make sure it's not the symbol table */
        if (fread(&end, sizeof(end), 1, fp) != 1
            || fseek(fp, 0L, SEEK_END)
            || (unsigned long)ftell(fp) != end
            || fseek(fp, seekpt + sizeof(end), SEEK_SET))
            goto no_data;
        
        /* success! */
        return(fp);
    }

no_data:
    /* 
     *   if we didn't find any tags at all, note that there's no data
     *   present, so we don't try reading fruitlessly again 
     */
    if (tags_found == 0)
        no_data_present = TRUE;

    /* we didn't find anything - close the file and return failure */
    fclose(fp);
    return((FILE *)0);
}

/* ------------------------------------------------------------------------ */
#ifdef MICROSOFT

/* 
 *   retrieve the full name of the program, given argv[0] 
 */
static const char *oss_get_exe(const char *argv0)
{
    /*
     *   The MSVC run-time library keeps the full filename of the executable
     *   in the global variable '_pgmptr'.  
     */
    return _pgmptr;
}

#else

/* 
 *   retrieve the full name of the program, given argv[0] 
 */
static const char *oss_get_exe(const char *argv0)
{
    /* static path buffer, in case we need to build the path ourselves */
    static char path[OSFNMAX];
    char *p;
    
    /* 
     *   For non-microsoft compilers, we don't know of any RTL mechanisms for
     *   getting the full exe path.  Some compilers always give us a
     *   fully-qualified path in argv[0], so check that first - if it looks
     *   like an absolute path, just return it. 
     */
    if (os_is_file_absolute(argv0))
        return argv0;

    /* 
     *   Okay, the RTL startup routine didn't give us any help - it looks
     *   like we have a relative filename, so the RTL probably just passed us
     *   the first token on the DOS/cmd.exe command line.  The shell will
     *   have searched for the given filename first in the current directory,
     *   then in each directory listed in the PATH environment variable.  So,
     *   first build out the full absolute path to the file as though it were
     *   in the current working directory; if the resulting file exists,
     *   return that path.  
     */
    os_get_abs_filename(path, sizeof(path), argv0);
    if (!osfacc(path))
        return path;

    /* 
     *   The file doesn't exist in the working directory, so it must be
     *   somewhere in the PATH list.  Search the path.
     */
#ifdef TURBO
    /* 
     *   borland version - RTL searchpath() searches the PATH and returns a
     *   static buffer with the full path, if we found it 
     */
    if ((p = searchpath(argv0)) != 0)
    {
        /* 
         *   Found it.  Build out the full absolute path, since the PATH
         *   variable could itself have relative path references. 
         */
        os_get_abs_filename(path, sizeof(path), p);
        return path;
    }
#endif

    /* 
     *   We didn't find the file on the path.  Give up and just return the
     *   original filenamem. 
     */
    return argv0;
}

#endif /* MICROSOFT */

#endif /* DJGPP */

/* ------------------------------------------------------------------------ */
/*
 *   Get the executable filename given the main program's argv[0].  On DOS
 *   and Windows, argv[0] always contains the full path to the executable
 *   file, regardless of what the user typed on the command line - the shell
 *   always expands relative names to absolute paths, and inserts the PATH
 *   variable expansion as needed.  So, we simply copy the argv[0] value
 *   into the caller's buffer.  
 */
int os_get_exe_filename(char *buf, size_t buflen, const char *argv0)
{
    /*
     *   If the name string is too long, fail.  Note that the name is too
     *   long if its strlen is so much as equal to the buffer length,
     *   because we need one extra byte for a null terminator in the copied
     *   result.  
     */
    if (strlen(oss_get_exe(argv0)) >= buflen)
        return FALSE;

    /* copy it and return success */
    strcpy(buf, oss_get_exe(argv0));
    return TRUE;
}

/*
 *   Get a special path.  
 */
void os_get_special_path(char *buf, size_t buflen, const char *argv0, int id)
{
    char tmp[OSFNMAX];

    /* determine which path we're retrieving */
    switch(id)
    {
    case OS_GSP_T3_RES:
    case OS_GSP_T3_APP_DATA:
    case OS_GSP_T3_SYSCONFIG:
    case OS_GSP_LOGFILE:
        /* 
         *   For DOS/Windows, we keep these items in the root install
         *   directory.  This is the same directory that contains the
         *   executable, so we simply need to get the path name of the
         *   executable.  
         */
        os_get_path_name(buf, buflen, oss_get_exe(argv0));

        /* 
         *   canonicalize the path by converting it to all lower-case (since
         *   DOS/Win file systems are not sensitive to case, this won't
         *   change the meaning of the filename, but it will ensure that it's
         *   stable for comparisons to the same path obtained by other means)
         */
        strlwr(buf);
        return;

    case OS_GSP_T3_LIB:
        /*
         *   For DOS/Windows, we keep TADS 3 library source files in the
         *   "lib" subdirectory of the install directory.  
         */
        os_get_path_name(tmp, sizeof(tmp), oss_get_exe(argv0));
        os_build_full_path(buf, buflen, tmp, "lib");

        /* return in all lower-case for consistency */
        strlwr(buf);
        return;

    case OS_GSP_T3_INC:
        /*
         *   For DOS/Windows, we keep TADS 3 library header files in the
         *   "include" subdirectory of the install directory.  
         */
        os_get_path_name(tmp, sizeof(tmp), oss_get_exe(argv0));
        os_build_full_path(buf, buflen, tmp, "include");

        /* return in all lower-case for consistency */
        strlwr(buf);
        return;

    case OS_GSP_T3_USER_LIBS:
        /*
         *   For DOS/Windows, the user library path is taken from the
         *   environment variable TADSLIB.  If this environment variable
         *   isn't set, there's no default value.  
         */
        {
            char *p;

            /* try getting the environment variable */
            p = getenv("TADSLIB");

            /* if it exists, set it; otherwise, return an empty string */
            if (p != 0)
            {
                size_t copy_len;

                /* copy as much as we can fit in the buffer */
                copy_len = strlen(p);
                if (copy_len > buflen - 1)
                    copy_len = buflen - 1;

                /* 
                 *   copy the (possibly truncated) string, and null terminate
                 *   the result 
                 */
                memcpy(buf, p, copy_len);
                buf[copy_len] = '\0';
            }
            else
            {
                /* the variable isn't set - return empty */
                buf[0] = '\0';
            }
        }
        return;

    default:
        /*
         *   If we're called with another identifier, it must mean that
         *   we're out of date.  Fail with an assertion. 
         */
        assert(FALSE);
        break;
    }
}

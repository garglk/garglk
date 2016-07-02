#if 0
// THIS FILE IS DEPRECATED.  The MSDOS-specific oss_get_rel_path() has
// been promoted (as of TADS 3.1.1, March 2012) to a general-purpose
// osifc routine, os_get_rel_path(), so the implementation has been moved
// to tads2/osnoui.c.

#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ossrelpa.c - DOS routine - get a relative path
Function
  
Notes
  
Modified
  11/17/99 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "os.h"

/*
 *   Create the relative form of a path name.  Given a filename and a
 *   working directory, get the name of the file with its path expressed
 *   relative to the working directory, if possible.  Both the filename
 *   and working directory paths should be expressed as absolute paths.
 *   
 *   If the filename and working directory are on different drives, we'll
 *   simply return the filename unchanged, since it can't be expressed
 *   relative to the directory.  
 */
void oss_get_rel_path(char *result, size_t result_len,
                      const char *filename, const char *working_dir)
{
    const char *fp;
    const char *wp;
    const char *fsep;
    const char *wsep;
    size_t rem;
    char *rp;
    
    /* 
     *   Determine if the paths are on the same drive.  If they're not, we
     *   can't express the filename relative to the path, so return the
     *   filename unchanged. 
     */
    if (toupper(filename[0]) != toupper(working_dir[0]))
    {
        /* different drives - copy the filename unchanged */
        strncpy(result, filename, result_len - 1);
        result[result_len - 1] = '\0';
        return;
    }

    /* 
     *   They're on the same drive, so we can express the paths
     *   relatively.  First, find out how much of the paths match, since
     *   we can simply lop off the common prefix.  
     */
    for (fp = filename, wp = working_dir, fsep = 0, wsep = 0 ;
         ((toupper(*fp) == toupper(*wp))
          || ((*fp == '/' || *fp == '\\') && (*wp == '/' || *wp == '\\'))) ;
         ++fp, ++wp)
    {
        /* 
         *   if this is a separator character, note it - we want to keep
         *   track of the last separator in the common portion, since this
         *   is the end of the common path prefix 
         */
        if (*fp == '\\' || *fp == '/' || *fp == '\0')
        {
            fsep = fp;
            wsep = wp;
        }

        /* stop at the end of the strings */
        if (*fp == '\0' || *wp == '\0')
            break;
    }

    /* if we didn't find any separators, we can't relativize the paths */
    if (wsep == 0 || fsep == 0)
    {
        /* nothing in common - return the filename unchanged */
        strncpy(result, filename, result_len - 1);
        result[result_len - 1] = '\0';
        return;
    }
        

    /*
     *   If we reached the end of the working directory string, and we're
     *   at a path separator in the filename string, then the entire
     *   working directory prefix is common 
     */
    if (*wp == '\0' && (*fp == '/' || *fp == '\\' || *fp == '\0'))
    {
        fsep = fp;
        wsep = wp;
    }

    /* if we're at a path separator in the working directory, skip it */
    if (*wsep != '\0')
        ++wsep;

    /* if we're at a path separator in the filename, skip it */
    if (*fsep != '\0')
        ++fsep;

    /*
     *   Everything up to fsep and wsep can be dropped, because it's a
     *   common directory path prefix.  We must now add the relative
     *   adjustment portion: add a ".." directory for each remaining
     *   directory in the working path, since we must move from the
     *   working diretory up to the common ancestor; then add the rest of
     *   the filename path.
     */

    /* 
     *   first, set up to copy into the result buffer - leave space for
     *   null termination
     */
    rp = result;
    rem = result_len - 1;

    /* 
     *   Add a ".." for each remaining directory in the working directory
     *   string 
     */
    while (*wsep != '\0')
    {
        /* add ".." and a path separator */
        if (rem > 3)
        {
            *rp++ = '.';
            *rp++ = '.';
            *rp++ = '\\';
            rem -= 3;
        }
        else
        {
            /* no more room - give up */
            rem = 0;
            break;
        }

        /* scan to the next path separator */
        while (*wsep != '\0' && *wsep != '/' && *wsep != '\\')
            ++wsep;

        /* if this is a path separator, skip it */
        if (*wsep != '\0')
            ++wsep;
    }

    /*
     *   Copy the remainder of the filename, or as much as will fit, and
     *   ensure that the result is properly null-terminated 
     */
    strncpy(rp, fsep, rem);
    rp[rem] = '\0';

    /* if the result is empty, return "." to represent the current dir */
    if (result[0] == '\0')
        strcpy(rp, ".");
}

/* ------------------------------------------------------------------------ */
/*
 *   Test routine 
 */

#ifdef TEST

int main(int argc, char **argv)
{
    char working_dir[OSFNMAX];
    char filename[OSFNMAX];
    char rel_path[OSFNMAX];

    for (;;)
    {
        printf("Working dir> ");
        if (gets(working_dir) == 0)
            break;
        
        printf("Filename   > ");
        if (gets(filename) == 0)
            break;

        oss_get_rel_path(rel_path, sizeof(rel_path), filename, working_dir);
        printf("Relative   = \"%s\"\n\n", rel_path);
    }

    return 0;
}

#endif

#endif // #if 0 - FILE DEPRECATED

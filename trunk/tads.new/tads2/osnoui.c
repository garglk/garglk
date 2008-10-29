#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OSNOUI.C,v 1.3 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osnoui.c - general-purpose implementations of OS routines with no UI
Function
  This file provides implementations for certain OS routines that have
  no UI component and can be implemented in general for a range of
  operating systems.
Notes
  
Modified
  04/11/99 CNebel        - Improve const-ness; fix C++ errors.
  11/02/97 MJRoberts  - Creation
*/

#include <stdio.h>
#ifdef MSDOS
#include <io.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "os.h"
#include "run.h"

/*
 *   Ports with MS-DOS-like file systems (Atari ST, OS/2, Macintosh, and,
 *   of course, MS-DOS itself) can use the os_defext and os_remext
 *   routines below by defining USE_DOSEXT.  Unix and VMS filenames will
 *   also be parsed correctly by these implementations, but untranslated
 *   VMS logical names may not be.  
 */

#ifdef USE_DOSEXT
/* 
 *   os_defext(fn, ext) should append the default extension ext to the
 *   filename in fn.  It is assumed that the buffer at fn is big enough to
 *   hold the added characters in the extension.  The result should be
 *   null-terminated.  When an extension is already present in the
 *   filename at fn, no action should be taken.  On systems without an
 *   analogue of extensions, this routine should do nothing.  
 */
void os_defext(char *fn, const char *ext)
{
    char *p;

    /* 
     *   Scan backwards from the end of the string, looking for the last
     *   dot ('.') in the filename.  Stop if we encounter a path separator
     *   character of some kind, because that means we reached the start
     *   of the filename without encountering a period.  
     */
    p = fn + strlen(fn);
    while (p > fn)
    {
        /* on to the previous character */
        p--;

        /* 
         *   if it's a period, return without doing anything - this
         *   filename already has an extension, so don't apply a default 
         */
        if (*p == '.')
            return;

        /* 
         *   if this is a path separator character, we're no longer in the
         *   filename, so stop looking for a period 
         */
        if (*p == OSPATHCHAR || strchr(OSPATHALT, *p) != 0)
            break;
    }

    /* we didn't find an extension - add the dot and the extension */
    strcat(fn, ".");
    strcat(fn, ext);
}

/*
 *   Add an extension, even if the filename currently has one 
 */
void os_addext(char *fn, const char *ext)
{
    strcat(fn, ".");
    strcat(fn, ext);
}

/* 
 *   os_remext(fn) removes the extension from fn, if present.  The buffer
 *   at fn should be modified in place.  If no extension is present, no
 *   action should be taken.  For systems without an analogue of
 *   extensions, this routine should do nothing.  
 */
void os_remext(char *fn)
{
    char *p;

    /* scan backwards from the end of the string, looking for a dot */
    p = fn + strlen(fn);
    while ( p>fn )
    {
        /* move to the previous character */
        p--;

        /* if it's a period, we've found the extension */
        if ( *p=='.' )
        {
            /* terminate the string here to remove the extension */
            *p = '\0';

            /* we're done */
            return;
        }

        /* 
         *   if this is a path separator, there's no extension, so we can
         *   stop looking 
         */
        if (*p == OSPATHCHAR || strchr(OSPATHALT, *p) != 0)
            return;
    }
}

/*
 *   Get a pointer to the root name portion of a filename.  Note that this
 *   implementation is included in the ifdef USE_DOSEXT section, since it
 *   seems safe to assume that if a platform has filenames that are
 *   sufficiently DOS-like for the extension parsing routines, the same
 *   will be true of path parsing.  
 */
char *os_get_root_name(char *buf)
{
    char *rootname;

    /* scan the name for path separators */
    for (rootname = buf ; *buf != '\0' ; ++buf)
    {
        /* if this is a path separator, remember it */
        if (*buf == OSPATHCHAR || strchr(OSPATHALT, *buf) != 0)
        {
            /* 
             *   It's a path separators - for now, assume the root will
             *   start at the next character after this separator.  If we
             *   find another separator later, we'll forget about this one
             *   and use the later one instead.  
             */
            rootname = buf + 1;
        }
    }

    /* return the last root name candidate */
    return rootname;
}

/*
 *   Extract the path from a filename 
 */
void os_get_path_name(char *pathbuf, size_t pathbuflen, const char *fname)
{
    const char *lastsep;
    const char *p;
    size_t len;
    int root_path;
    
    /* find the last separator in the filename */
    for (p = fname, lastsep = fname ; *p != '\0' ; ++p)
    {
        /* 
         *   if it's a path separator character, remember it as the last one
         *   we've found so far 
         */
        if (*p == OSPATHCHAR || strchr(OSPATHALT, *p)  != 0)
            lastsep = p;
    }
    
    /* get the length of the prefix, not including the separator */
    len = lastsep - fname;
    
    /*
     *   Normally, we don't include the last path separator in the path; for
     *   example, on Unix, the path of "/a/b/c" is "/a/b", not "/a/b/".
     *   However, on Unix/DOS-like file systems, a root path *does* require
     *   the last path separator: the path of "/a" is "/", not an empty
     *   string.  So, we need to check to see if the file is in a root path,
     *   and if so, include the final path separator character in the path.  
     */
    for (p = fname, root_path = FALSE ; p != lastsep ; ++p)
    {
        /*
         *   if this is NOT a path separator character, we don't have all
         *   path separator characters before the filename, so we don't have
         *   a root path 
         */
        if (*p != OSPATHCHAR && strchr(OSPATHALT, *p) == 0)
        {
            /* note that we don't have a root path */
            root_path = FALSE;
            
            /* no need to look any further */
            break;
        }
    }

    /* if we have a root path, keep the final path separator in the path */
    if (root_path)
        ++len;

#ifdef MSDOS
    /*
     *   On DOS, we have a special case: if the path is of the form "x:\",
     *   where "x" is any letter, then we have a root filename and we want to
     *   include the backslash.  
     */
    if (lastsep == fname + 2
        && isalpha(fname[0]) && fname[1] == ':' && fname[2] == '\\')
    {
        /* we have an absolute path - use the full "x:\" sequence */
        len = 3;
    }
#endif
    
    /* make sure it fits in our buffer (with a null terminator) */
    if (len > pathbuflen - 1)
        len = pathbuflen - 1;

    /* copy it and null-terminate it */
    memcpy(pathbuf, fname, len);
    pathbuf[len] = '\0';
}

/*
 *   Canonicalize a path: remove ".." and "." relative elements 
 */
void canonicalize_path(char *path)
{
    char *p;
    char *start;
    
    /* keep going until we're done */
    for (start = p = path ; ; ++p)
    {
        /* if it's a separator, note it and process the path element */
        if (*p == '\\' || *p == '/' || *p == '\0')
        {
            /* 
             *   check the path element that's ending here to see if it's a
             *   relative item - either "." or ".." 
             */
            if (p - start == 1 && *start == '.')
            {
                /* 
                 *   we have a '.' element - simply remove it along with the
                 *   path separator that follows 
                 */
                if (*p == '\\' || *p == '/')
                    memmove(start, p + 1, strlen(p+1) + 1);
                else if (start > path)
                    *(start - 1) = '\0';
                else
                    *start = '\0';
            }
            else if (p - start == 2 && *start == '.' && *(start+1) == '.')
            {
                char *prv;
                
                /* 
                 *   we have a '..' element - find the previous path element,
                 *   if any, and remove it, along with the '..' and the
                 *   subsequent separator 
                 */
                for (prv = start ;
                     prv > path && (*(prv-1) != '\\' || *(prv-1) == '/') ;
                     --prv) ;

                /* if we found a separator, remove the previous element */
                if (prv > start)
                {
                    if (*p == '\\' || *p == '/')
                        memmove(prv, p + 1, strlen(p+1) + 1);
                    else if (start > path)
                        *(start - 1) = '\0';
                    else
                        *start = '\0';
                }
            }

            /* note the start of the next element */
            start = p + 1;
        }

        /* stop at the end of the string */
        if (*p == '\0')
            break;
    }
}

/*
 *   Build a full path name given a path and a filename 
 */
void os_build_full_path(char *fullpathbuf, size_t fullpathbuflen,
                        const char *path, const char *filename)
{
    size_t plen, flen;
    int add_sep;

    /* 
     *   Note whether we need to add a separator.  If the path prefix ends
     *   in a separator, don't add another; otherwise, add the standard
     *   system separator character.
     *   
     *   Do not add a separator if the path is completely empty, since
     *   this simply means that we want to use the current directory.  
     */
    plen = strlen(path);
    add_sep = (plen != 0
               && path[plen-1] != OSPATHCHAR
               && strchr(OSPATHALT, path[plen-1]) == 0);
    
    /* copy the path to the full path buffer, limiting to the buffer length */
    if (plen > fullpathbuflen - 1)
        plen = fullpathbuflen - 1;
    memcpy(fullpathbuf, path, plen);

    /* add the path separator if necessary (and if there's room) */
    if (add_sep && plen + 2 < fullpathbuflen)
        fullpathbuf[plen++] = OSPATHCHAR;

    /* add the filename after the path, if there's room */
    flen = strlen(filename);
    if (flen > fullpathbuflen - plen - 1)
        flen = fullpathbuflen - plen - 1;
    memcpy(fullpathbuf + plen, filename, flen);

    /* add a null terminator */
    fullpathbuf[plen + flen] = '\0';

    /* canonicalize the result */
    canonicalize_path(fullpathbuf);
}

/*
 *   Determine if a path is absolute or relative.  If the path starts with
 *   a path separator character, we consider it absolute, otherwise we
 *   consider it relative.
 *   
 *   Note that, on DOS, an absolute path can also follow a drive letter.
 *   So, if the path contains a letter followed by a colon, we'll consider
 *   the path to be absolute. 
 */
int os_is_file_absolute(const char *fname)
{
    /* if the name starts with a path separator, it's absolute */
    if (fname[0] == OSPATHCHAR || strchr(OSPATHALT, fname[0])  != 0)
        return TRUE;

#ifdef MSDOS
    /* on DOS, a file is absolute if it starts with a drive letter */
    if (isalpha(fname[0]) && fname[1] == ':')
        return TRUE;
#endif /* MSDOS */

    /* the path is relative */
    return FALSE;
}


#endif /* USE_DOSEXT */

/* ------------------------------------------------------------------------ */

/*
 *   A port can define USE_TIMERAND if it wishes to randomize from the
 *   system clock.  This should be usable by most ports.  
 */
#ifdef USE_TIMERAND
# include <time.h>

void os_rand(long *seed)
{
    time_t t;
    
    time( &t );
    *seed = (long)t;
}
#endif /* USE_TIMERAND */

#ifdef USE_PATHSEARCH
/* search a path specified in the environment for a tads file */
static int pathfind(const char *fname, int flen, const char *pathvar,
                    char *buf, size_t bufsiz)
{
    char   *e;
    
    if ( !(e = getenv(pathvar)) )
        return(0);
    for ( ;; )
    {
        char   *sep;
        size_t  len;
        
        if ( (sep = strchr(e, OSPATHSEP)) )
        {
            len = (size_t)(sep-e);
            if (!len) continue;
        }
        else
        {
            len = strlen(e);
            if (!len) break;
        }
        memcpy(buf, e, len);
        if (buf[len-1] != OSPATHCHAR && !strchr(OSPATHALT, buf[len-1]))
            buf[len++] = OSPATHCHAR;
        memcpy(buf+len, fname, flen);
        buf[len+flen] = 0;
        if (osfacc(buf) == 0) return(1);
        if (!sep) break;
        e = sep+1;
    }
    return(0);
}
#endif /* USE_PATHSEARCH */

/*
 *   Look for a tads-related file in the standard locations and, if the
 *   search is successful, store the result file name in the given buffer.
 *   Return 1 if the file was located, 0 if not.
 *   
 *   Search the following areas for the file: current directory, program
 *   directory (as derived from argv[0]), and the TADS path.  
 */
int os_locate(const char *fname, int flen, const char *arg0,
              char *buf, size_t bufsiz)
{
    /* Check the current directory */
    if (osfacc(fname) == 0)
    {
        memcpy(buf, fname, flen);
        buf[flen] = 0;
        return(1);
    }
    
    /* Check the program directory */
    if (arg0 && *arg0)
    {
        const char *p;
        
        /* find the end of the directory name of argv[0] */
        for ( p = arg0 + strlen(arg0);
              p > arg0 && *(p-1) != OSPATHCHAR && !strchr(OSPATHALT, *(p-1));
              --p )
            ;
        
        /* don't bother if there's no directory on argv[0] */
        if (p > arg0)
        {
            size_t  len = (size_t)(p - arg0);
            
            memcpy(buf, arg0, len);
            memcpy(buf+len, fname, flen);
            buf[len+flen] = 0;
            if (osfacc(buf) == 0) return(1);
        }
    }
    
#ifdef USE_PATHSEARCH
    /* Check TADS path */
    if ( pathfind(fname, flen, "TADS", buf, bufsiz) )
        return(1);
#endif /* USE_PATHSEARCH */
    
    return(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Temporary files
 *   
 *   This default implementation is layered on the normal osifc file APIs, so
 *   our temp files are nothing special: they're just ordinary files that we
 *   put in a special directory (the temp path), and which we count on our
 *   caller to delete before the app terminates (which assumes that the app
 *   terminates normally, AND that our portable caller correctly keeps track
 *   of every temp file it creates and explicitly deletes it).
 *   
 *   On systems that have native temp file support, you might want to use the
 *   native API instead, since native temp file APIs often have the useful
 *   distinction that they'll automatically delete any outstanding temp files
 *   on application termination, even on crashy exits.  To remove these
 *   default implementations from the build, #define OSNOUI_OMIT_TEMPFILE
 *   in your makefile.  
 */
#ifndef OSNOUI_OMIT_TEMPFILE

/*
 *   Create and open a temporary file 
 */
osfildef *os_create_tempfile(const char *swapname, char *buf)
{
    osfildef *fp;

    /* if a name wasn't provided, generate a name */
    if (swapname == 0)
    {
        int     attempt;
        size_t  len;
        time_t  timer;
        int     found;
        int     tries;

        /* 
         *   try once with the temporary file path; if that fails, simply
         *   try using the current working directory 
         */
        for (found = FALSE, tries = 0 ; !found && tries < 2 ; ++tries)
        {
            /* 
             *   If this is the first try, get the appropriate path for a
             *   temp file.  If that failed, try using the current
             *   directory. 
             */
            if (tries == 0)
            {
                /* get the temporary path */
                os_get_tmp_path(buf);
                len = strlen(buf);
            }
            else
            {
                /* 
                 *   the temporary path didn't work, so this time try it in
                 *   the current directory 
                 */
                buf[0] = '\0';
                len = 0;
            }

            /* get the current time, as a basis for a unique identifier */
            time(&timer);

            /* try until we find a non-existent filename */
            for (attempt = 0 ; attempt < 100 ; ++attempt)
            {
                /* generate a name based on time and try number */
                sprintf(buf + len, "SW%06ld.%03d",
                        (long)timer % 999999, attempt);
                
                /* check to see if a file by this name already exists */
                if (osfacc(buf))
                {
                    /* the file doesn't exist - try creating it */
                    fp = osfoprwtb(buf, OSFTSWAP);

                    /* if that succeeded, return this file */
                    if (fp != 0)
                    {
                        /* set the file's type in the OS, if necessary */
                        os_settype(buf, OSFTSWAP);

                        /* we're done - return the open file handle */
                        return fp;
                    }
                }
            }
        }

        /* we failed to find a name we could use - give up */
        return 0;
    }
    else
    {
        /* open the file they gave us */
        fp = osfoprwtb(swapname, OSFTSWAP);

        /* set the file's type in the OS, if necessary */
        os_settype(swapname, OSFTSWAP);

        /* return the file pointer */
        return fp;
    }
}

/*
 *   Delete a temporary file.  Since our generic implementation of
 *   os_create_tempfile() simply uses osfoprwtb() to open the file, a
 *   temporary file's handle is not any different from any other file
 *   handle - in particular, the OS doesn't automatically delete the file
 *   when closed.  Hence, we need to delete the file explicitly here. 
 */
int osfdel_temp(const char *fname)
{
    /* delete the file using the normal mechanism */
    return osfdel(fname);
}

#endif /* OSNOUI_OMIT_TEMPFILE */

/* ------------------------------------------------------------------------ */

/*
 *   print a null-terminated string to osfildef* file 
 */
void os_fprintz(osfildef *fp, const char *str)
{
    fprintf(fp, "%s", str);
}

/* 
 *   print a counted-length string (which might not be null-terminated) to a
 *   file 
 */
void os_fprint(osfildef *fp, const char *str, size_t len)
{
    fprintf(fp, "%.*s", (int)len, str);
}

#ifdef MSDOS
/*
 *   MS-DOS implementation - Get the temporary file path.  Tries getting
 *   the values of various environment variables that are typically used
 *   to define the location for temporary files.  
 */
void os_get_tmp_path(char *buf)
{
    static char *vars[] =
    {
        "TEMPDIR",
        "TMPDIR",
        "TEMP",
        "TMP",
        0
    };
    char **varp;

    /* look for an environment variable from our list */
    for (varp = vars ; *varp ; ++varp)
    {
        char *val;

        /* get this variable's value */
        val = getenv(*varp);
        if (val)
        {
            size_t  len;

            /* use this value */
            strcpy(buf, val);

            /* add a backslash if necessary */
            if ((len = strlen(buf)) != 0
                && buf[len-1] != '/' && buf[len-1] != '\\')
            {
                buf[len] = '\\';
                buf[len+1] = '\0';
            }

            /* use this value */
            return;
        }
    }

    /* didn't find anything - leave the prefix empty */
    buf[0] = '\0';
}
#endif /* MSDOS */

#ifdef USE_NULLSTYPE
void os_settype(const char *f, os_filetype_t t)
{
    /* nothing needs to be done on this system */
}
#endif /* USE_NULLSTYPE */

/*
 *   Convert an OS filename path to a relative URL 
 */
void os_cvt_dir_url(char *result_buf, size_t result_buf_size,
                    const char *src_path, int end_sep)
{
    char *dst;
    const char *src;
    size_t rem;
    int last_was_sep;
    static const char quoted[] = ":%";

    /*
     *   Run through the source buffer, copying characters to the output
     *   buffer.  If we encounter a path separator character, replace it
     *   with a forward slash.  
     */
    for (last_was_sep = FALSE, dst = result_buf, src = src_path,
         rem = result_buf_size ;
         *src != '\0' && rem > 1 ; ++dst, ++src, --rem)
    {
        /* presume this will not be a path separator character */
        last_was_sep = FALSE;

        /* 
         *   If this is a local path separator character, replace it with the
         *   URL-style path separator character.  If it's an illegal URL
         *   character, quote it with "%" notation.  Otherwise, copy it
         *   unchanged.  
         */
        if (strchr(OSPATHURL, *src) != 0)
        {
            /* add the URL-style path separator instead of the local one */
            *dst = '/';

            /* note that we just added a path separator character */
            last_was_sep = TRUE;
        }
        else if (strchr(quoted, *src) != 0)
        {
            /* if we have room for three characters, add the "%" sequence */
            if (rem > 3)
            {
                int dig;
                
                /* add the '%' */
                *dst++ = '%';

                /* add the high-order digit */
                dig = ((int)(unsigned char)*src) >> 4;
                *dst++ = (dig < 10 ? dig + '0' : dig + 'A' - 10);

                /* add the low-order digit */
                dig = ((int)(unsigned char)*src) & 0x0F;
                *dst = (dig < 10 ? dig + '0' : dig + 'A' - 10);

                /* deduct the extra two characters beyond the expected one */
                rem -= 2;
            }
        }
        else
        {
            /* add the character unchanged */
            *dst = *src;
        }
    }

    /* 
     *   add an additional ending separator if desired and if the last
     *   character wasn't a separator 
     */
    if (end_sep && rem > 1 && !last_was_sep)
        *dst++ = '/';

    /* add a null terminator and we're done */
    *dst = '\0';
}

/*
 *   Convert a relative URL to a relative file system path name 
 */
void os_cvt_url_dir(char *result_buf, size_t result_buf_size,
                    const char *src_url, int end_sep)
{
    char *dst;
    const char *src;
    size_t rem;
    int last_was_sep;

    /*
     *   Run through the source buffer, copying characters to the output
     *   buffer.  If we encounter a '/', convert it to a path separator
     *   character.
     */
    for (last_was_sep = FALSE, dst = result_buf, src = src_url,
         rem = result_buf_size ;
         *src != '\0' && rem > 1 ; ++dst, ++src, --rem)
    {
        /* 
         *   replace slashes with path separators; expand '%' sequences; copy
         *   all other characters unchanged 
         */
        if (*src == '/')
        {
            *dst = OSPATHCHAR;
            last_was_sep = TRUE;
        }
        else if (*src == '%'
                 && isxdigit((uchar)*(src+1))
                 && isxdigit((uchar)*(src+2)))
        {
            unsigned char c;
            unsigned char acc;

            /* convert the '%xx' sequence to its character code */
            c = *++src;
            acc = (c - (c >= 'A' && c <= 'F'
                        ? 'A' - 10
                        : c >= 'a' && c <= 'f'
                        ? 'a' - 10
                        : '0')) << 4;
            c = *++src;
            acc += (c - (c >= 'A' && c <= 'F'
                         ? 'A' - 10
                         : c >= 'a' && c <= 'f'
                         ? 'a' - 10
                         : '0'));

            /* set the character */
            *dst = acc;

            /* it's not a separator */
            last_was_sep = FALSE;
        }
        else
        {
            *dst = *src;
            last_was_sep = FALSE;
        }
    }

    /* 
     *   add an additional ending separator if desired and if the last
     *   character wasn't a separator 
     */
    if (end_sep && rem > 1 && !last_was_sep)
        *dst++ = OSPATHCHAR;

    /* add a null terminator and we're done */
    *dst = '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   Service routine for searching - build the full output path name.  (This
 *   is used in the various DOS/Windows builds.)  
 */
#if defined(TURBO) || defined(DJGPP) || defined(MICROSOFT) || defined(MSOS2)

static void oss_build_outpathbuf(char *outpathbuf, size_t outpathbufsiz,
                                 const char *path, const char *fname)
{
    /* if there's a full path buffer, build the full path */
    if (outpathbuf != 0)
    {
        size_t lp;
        size_t lf;

        /* copy the path prefix */
        lp = strlen(path);
        if (lp > outpathbufsiz - 1)
            lp = outpathbufsiz - 1;
        memcpy(outpathbuf, path, lp);

        /* add the filename if there's any room */
        lf = strlen(fname);
        if (lf > outpathbufsiz - lp - 1)
            lf = outpathbufsiz - lp - 1;
        memcpy(outpathbuf + lp, fname, lf);

        /* null-terminate the result */
        outpathbuf[lp + lf] = '\0';
    }
}

#endif /* TURBO || DJGPP || MICROSOFT || MSOS2 */

/* ------------------------------------------------------------------------ */
/*
 *   Borland C implementation of directory functions 
 */
#if defined(TURBO) || defined(DJGPP)

#include <dir.h>
#include <dos.h>

/*
 *   search context structure 
 */
struct oss_find_ctx_t
{
    /* C library find-file block */
    struct ffblk ff;

    /* original search path prefix (we'll allocate more to fit the string) */
    char path[1];
};

/*
 *   find first matching file 
 */
void *os_find_first_file(const char *dir, const char *pattern, char *outbuf,
                         size_t outbufsiz, int *isdir,
                         char *outpathbuf, size_t outpathbufsiz)
{
    struct oss_find_ctx_t *ctx;
    char realpat[OSFNMAX];
    size_t l;
    size_t path_len;
    const char *lastsep;
    const char *p;

    /* 
     *   construct the filename pattern from the directory and pattern
     *   segments, using "*" as the default wildcard pattern if no
     *   explicit pattern was provided 
     */
    strcpy(realpat, dir);
    if ((l = strlen(realpat)) != 0 && realpat[l - 1] != '\\')
        realpat[l++] = '\\';
    if (pattern == 0)
        strcpy(realpat + l, "*.*");
    else
        strcpy(realpat + l, pattern);

    /* find the last separator in the original path */
    for (p = realpat, lastsep = 0 ; *p != '\0' ; ++p)
    {
        /* if this is a separator, remember it */
        if (*p == '\\' || *p == '/' || *p == ':')
            lastsep = p;
    }

    /* 
     *   if we found a separator, the path prefix is everything up to and
     *   including the separator; otherwise, there's no path prefix 
     */
    if (lastsep != 0)
    {
        /* the length of the path includes the separator */
        path_len = lastsep + 1 - realpat;
    }
    else
    {
        /* there's no path prefix - everything is in the current directory */
        path_len = 0;
    }

    /* allocate a context */
    ctx = (struct oss_find_ctx_t *)malloc(sizeof(struct oss_find_ctx_t)
                                          + path_len);

    /* copy the path to the context */
    memcpy(ctx->path, realpat, path_len);
    ctx->path[path_len] = '\0';

    /* call DOS to search for a matching file */
    if (findfirst(realpat, &ctx->ff, FA_DIREC))
    {
        /* no dice - delete the context and return failure */
        free(ctx);
        return 0;
    }

    /* skip files with the HIDDEN or SYSTEM attributes */
    while ((ctx->ff.ff_attrib & (FA_HIDDEN | FA_SYSTEM)) != 0)
    {
        /* skip to the next file */
        if (findnext(&ctx->ff))
        {
            /* no more files - delete the context and give up */
            free(ctx);
            return 0;
        }
    }

    /* copy the filename into the caller's buffer */
    l = strlen(ctx->ff.ff_name);
    if (l > outbufsiz - 1)
        l = outbufsiz - 1;
    memcpy(outbuf, ctx->ff.ff_name, l);
    outbuf[l] = '\0';

    /* build the full path, if desired */
    oss_build_outpathbuf(outpathbuf, outpathbufsiz,
                         ctx->path, ctx->ff.ff_name);

    /* return the directory indication  */
    *isdir = (ctx->ff.ff_attrib & FA_DIREC) != 0;

    /* 
     *   return the context - it will be freed later when find-next
     *   reaches the last file or find-close is called to cancel the
     *   search 
     */
    return ctx;
}

/* find the next file */
void *os_find_next_file(void *ctx0, char *outbuf, size_t outbufsiz,
                        int *isdir, char *outpathbuf, size_t outpathbufsiz)
{
    struct oss_find_ctx_t *ctx = (struct oss_find_ctx_t *)ctx0;
    size_t l;

    /* if the search has already ended, do nothing */
    if (ctx == 0)
        return 0;

    /* keep going until we find a non-hidden, non-system file */
    do
    {
        /* try searching for the next file */
        if (findnext(&ctx->ff))
        {
            /* no more files - delete the context and give up */
            free(ctx);
            
            /* return null to indicate that the search is finished */
            return 0;
        }
    } while ((ctx->ff.ff_attrib & (FA_HIDDEN | FA_SYSTEM)) != 0);
    
    /* return the name */
    l = strlen(ctx->ff.ff_name);
    if (l > outbufsiz - 1) l = outbufsiz - 1;
    memcpy(outbuf, ctx->ff.ff_name, l);
    outbuf[l] = '\0';

    /* return the directory indication  */
    *isdir = (ctx->ff.ff_attrib & FA_DIREC) != 0;

    /* build the full path, if desired */
    oss_build_outpathbuf(outpathbuf, outpathbufsiz,
                         ctx->path, ctx->ff.ff_name);

    /* 
     *   indicate that the search was successful by returning the non-null
     *   context pointer -- the context has been updated for the new
     *   position in the search, so we just return the same context
     *   structure that we've been using all along 
     */
    return ctx;
}

/* cancel a search */
void os_find_close(void *ctx0)
{
    struct oss_find_ctx_t *ctx = (struct oss_find_ctx_t *)ctx0;

    /* if the search was already cancelled, do nothing */
    if (ctx == 0)
        return;

    /* delete the search context */
    free(ctx);
}

/*
 *   Time-zone initialization 
 */
void os_tzset()
{
    /* let the run-time library initialize the time zone */
    tzset();
}

/* ------------------------------------------------------------------------ */
/*
 *   Get file times 
 */

/*
 *   get file creation time 
 */
int os_get_file_cre_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the creation time in the return structure */
    t->t = info.st_ctime;
    return 0;
}

/*
 *   get file modification time 
 */
int os_get_file_mod_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the modification time in the return structure */
    t->t = info.st_mtime;
    return 0;
}

/* 
 *   get file last access time 
 */
int os_get_file_acc_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the access time in the return structure */
    t->t = info.st_atime;
    return 0;
}

/*
 *   compare two file time structures 
 */
int os_cmp_file_times(const os_file_time_t *a, const os_file_time_t *b)
{
    if (a->t < b->t)
        return -1;
    else if (a->t == b->t)
        return 0;
    else
        return 1;
}

#endif /* TURBO || DJGPP */

/* ------------------------------------------------------------------------ */
/*
 *   Microsoft C implementation of directory functions 
 */
#ifdef MICROSOFT

typedef struct
{
    /* search handle */
    long handle;

    /* found data structure */
    struct _finddata_t data;

    /* full original search path */
    char path[1];
} ossfcx;

/*
 *   find first matching file 
 */
void *os_find_first_file(const char *dir, const char *pattern, char *outbuf,
                         size_t outbufsiz, int *isdir,
                         char *outpathbuf, size_t outpathbufsiz)
{
    ossfcx *ctx;
    char realpat[OSFNMAX];
    size_t l;
    size_t path_len;
    const char *lastsep;
    const char *p;
    
    /* 
     *   construct the filename pattern from the directory and pattern
     *   segments, using "*" as the default wildcard pattern if no
     *   explicit pattern was provided 
     */
    strcpy(realpat, dir);
    if ((l = strlen(realpat)) != 0 && realpat[l - 1] != '\\')
        realpat[l++] = '\\';
    if (pattern == 0)
        strcpy(realpat + l, "*");
    else
        strcpy(realpat + l, pattern);

    /* find the last separator in the original path */
    for (p = realpat, lastsep = 0 ; *p != '\0' ; ++p)
    {
        /* if this is a separator, remember it */
        if (*p == '\\' || *p == '/' || *p == ':')
            lastsep = p;
    }

    /* 
     *   if we found a separator, the path prefix is everything up to and
     *   including the separator; otherwise, there's no path prefix 
     */
    if (lastsep != 0)
    {
        /* the length of the path includes the separator */
        path_len = lastsep + 1 - realpat;
    }
    else
    {
        /* there's no path prefix - everything is in the current directory */
        path_len = 0;
    }

    /* allocate a context */
    ctx = (ossfcx *)malloc(sizeof(ossfcx) + path_len);

    /* copy the path to the context */
    memcpy(ctx->path, realpat, path_len);
    ctx->path[path_len] = '\0';

    /* call DOS to search for a matching file */
    ctx->handle = _findfirst(realpat, &ctx->data);
    if (ctx->handle == -1L)
    {
        /* no dice - delete the context and return failure */
        free(ctx);
        return 0;
    }

    /* skip files with HIDDEN or SYSTEM attributes */
    while ((ctx->data.attrib & (_A_HIDDEN | _A_SYSTEM)) != 0)
    {
        /* skip this file */
        if (_findnext(ctx->handle, &ctx->data) != 0)
        {
            /* no more files - close up shop and return failure */
            _findclose(ctx->handle);
            free(ctx);
            return 0;
        }
    }

    /* return the name */
    l = strlen(ctx->data.name);
    if (l > outbufsiz - 1) l = outbufsiz - 1;
    memcpy(outbuf, ctx->data.name, l);
    outbuf[l] = '\0';

    /* return the directory indication  */
    *isdir = (ctx->data.attrib & _A_SUBDIR) != 0;

    /* build the full path, if desired */
    oss_build_outpathbuf(outpathbuf, outpathbufsiz,
                         ctx->path, ctx->data.name);
    
    /* 
     *   return the context - it will be freed later when find-next
     *   reaches the last file or find-close is called to cancel the
     *   search 
     */
    return ctx;
}

/* find the next file */
void *os_find_next_file(void *ctx0, char *outbuf, size_t outbufsiz,
                        int *isdir, char *outpathbuf, size_t outpathbufsiz)
{
    ossfcx *ctx = (ossfcx *)ctx0;
    size_t l;

    /* if the search has already ended, do nothing */
    if (ctx == 0)
        return 0;

    /* keep going until we find a non-system, non-hidden file */
    do
    {
        /* try searching for the next file */
        if (_findnext(ctx->handle, &ctx->data) != 0)
        {
            /* no more files - close the search and delete the context */
            _findclose(ctx->handle);
            free(ctx);
            
            /* return null to indicate that the search is finished */
            return 0;
        }
    } while ((ctx->data.attrib & (_A_HIDDEN | _A_SYSTEM)) != 0);

    /* return the name */
    l = strlen(ctx->data.name);
    if (l > outbufsiz - 1) l = outbufsiz - 1;
    memcpy(outbuf, ctx->data.name, l);
    outbuf[l] = '\0';

    /* return the directory indication  */
    *isdir = (ctx->data.attrib & _A_SUBDIR) != 0;

    /* build the full path, if desired */
    oss_build_outpathbuf(outpathbuf, outpathbufsiz,
                         ctx->path, ctx->data.name);

    /* 
     *   indicate that the search was successful by returning the non-null
     *   context pointer -- the context has been updated for the new
     *   position in the search, so we just return the same context
     *   structure that we've been using all along 
     */
    return ctx;
}

/* cancel a search */
void os_find_close(void *ctx0)
{
    ossfcx *ctx = (ossfcx *)ctx0;
    
    /* if the search was already cancelled, do nothing */
    if (ctx == 0)
        return;

    /* close the search and delete the context structure */
    _findclose(ctx->handle);
    free(ctx);
}

/*
 *   Time-zone initialization 
 */
void os_tzset()
{
    /* let the run-time library initialize the time zone */
    tzset();
}

/* ------------------------------------------------------------------------ */
/*
 *   Get file times 
 */

/*
 *   get file creation time 
 */
int os_get_file_cre_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the creation time in the return structure */
    t->t = info.st_ctime;
    return 0;
}

/*
 *   get file modification time 
 */
int os_get_file_mod_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the modification time in the return structure */
    t->t = info.st_mtime;
    return 0;
}

/* 
 *   get file last access time 
 */
int os_get_file_acc_time(os_file_time_t *t, const char *fname)
{
    struct stat info;

    /* get the file information */
    if (stat(fname, &info))
        return 1;

    /* set the access time in the return structure */
    t->t = info.st_atime;
    return 0;
}

/*
 *   compare two file time structures 
 */
int os_cmp_file_times(const os_file_time_t *a, const os_file_time_t *b)
{
    if (a->t < b->t)
        return -1;
    else if (a->t == b->t)
        return 0;
    else
        return 1;
}

#endif /* MICROSOFT */

/* ------------------------------------------------------------------------ */
/*
 *   OS/2 implementation of directory functions 
 */
#ifdef MSOS2

/* C library context structure used for file searches */
#ifdef __32BIT__
# define DosFindFirst(a,b,c,d,e,f)  DosFindFirst(a,b,c,d,e,f,FIL_STANDARD)
typedef FILEFINDBUF3    oss_c_ffcx;
typedef ULONG           count_t;
#else /* !__32BIT__ */
# define DosFindFirst(a,b,c,d,e,f)  DosFindFirst(a,b,c,d,e,f,0L)
typedef FILEFINDBUF     oss_c_ffcx;
typedef USHORT          count_t;
#endif /* __32BIT__ */

typedef struct
{
    /* C library context structure */
    oss_c_ffcx ff;

    /* original search path */
    char path[1];
} ossfcx;

void *os_find_first_file(const char *dir, const char *pattern, char *outbuf,
                         size_t outbufsiz, int *isdir,
                         char *outpathbuf, size_t outpathbufsiz)
{
    ossfcx  *ctx;
    char     realpat[OSFNMAX];
    size_t   l;
    HDIR     hdir = HDIR_SYSTEM;
    count_t  scnt = 1;
    size_t path_len;
    const char *lastsep;
    const char *p;

    /* 
     *   construct the filename pattern from the directory and pattern
     *   segments, using "*" as the default wildcard pattern if no
     *   explicit pattern was provided 
     */
    strcpy(realpat, dir);
    if ((l = strlen(realpat)) != 0 && realpat[l - 1] != '\\')
        realpat[l++] = '\\';
    if (pattern == 0)
        strcpy(realpat + l, "*");
    else
        strcpy(realpat + l, pattern);

    /* find the last separator in the original path */
    for (p = realpat, lastsep = 0 ; *p != '\0' ; ++p)
    {
        /* if this is a separator, remember it */
        if (*p == '\\' || *p == '/' || *p == ':')
            lastsep = p;
    }

    /* 
     *   if we found a separator, the path prefix is everything up to and
     *   including the separator; otherwise, there's no path prefix 
     */
    if (lastsep != 0)
    {
        /* the length of the path includes the separator */
        path_len = lastsep + 1 - realpat;
    }
    else
    {
        /* there's no path prefix - everything is in the current directory */
        path_len = 0;
    }

    /* allocate a context */
    ctx = (ossfcx *)malloc(sizeof(ossfcx) + path_len);

    /* call DOS to search for a matching file */
    if (DosFindFirst(realpat, &hdir, FILE_DIRECTORY | FILE_NORMAL,
                     &ctx->ff, sizeof(ctx->ff), &scnt))
    {
        /* no dice - delete the context and return failure */
        free(ctx);
        return 0;
    }

    /* if it's a HIDDEN or SYSTEM file, skip it */
    while ((ctx->ff.attrFile & (FILE_HIDDEN | FILE_SYSTEM)) != 0)
    {
        /* skip to the next file */
        if (DosFindNext(HDIR_SYSTEM, &ctx->ff, sizeof(ctx->ff), &scnt))
        {
            /* no more files - delete the context and give up */
            free(ctx);
            return 0;
        }
    }

    /* copy the path to the context */
    memcpy(ctx->path, realpat, path_len);
    ctx->path[path_len] = '\0';

    /* return the name */
    l = ctx->ff.cchName;
    if (l > outbufsiz - 1) l = outbufsiz - 1;
    memcpy(outbuf, ctx->ff.achName, l);
    outbuf[l] = '\0';

    /* return the directory indication  */
    *isdir = (ctx->ff.attrFile & FILE_DIRECTORY) != 0;

    /* build the full path, if desired */
    oss_build_outpathbuf(outpathbuf, outpathbufsiz,
                         ctx->path, ctx->ff.data.name);

    /* 
     *   return the context - it will be freed later when find-next
     *   reaches the last file or find-close is called to cancel the
     *   search 
     */
    return ctx;
}

/* find the next file */
void *os_find_next_file(void *ctx0, char *outbuf, size_t outbufsiz,
                        int *isdir, char *outpathbuf, size_t outpathbufsiz)
{
    ossfcx *ctx = (ossfcx *)ctx0;
    size_t l;

    /* if the search has already ended, do nothing */
    if (ctx == 0)
        return 0;

    /* keep going until we find a non-system, non-hidden file */
    do
    {
        /* try searching for the next file */
        if (DosFindNext(HDIR_SYSTEM, &ctx->ff, sizeof(ctx->ff), &scnt))
        {
            /* no more files - delete the context and give up */
            free(ctx);
            
            /* return null to indicate that the search is finished */
            return 0;
        }
    } while ((ctx->ff.attrFile & (FILE_HIDDEN | FILE_SYSTEM)) != 0);

    /* return the name */
    l = ctx->ff.cchName;
    if (l > outbufsiz - 1) l = outbufsiz - 1;
    memcpy(outbuf, ctx->ff.achName, l);
    outbuf[l] = '\0';

    /* return the directory indication  */
    *isdir = (ctx->ff.attrFile & FILE_DIRECTORY) != 0;

    /* build the full path, if desired */
    oss_build_outpathbuf(outpathbuf, outpathbufsiz,
                         ctx->ff.path, ctx->ff.data.name);

    /* 
     *   indicate that the search was successful by returning the non-null
     *   context pointer -- the context has been updated for the new
     *   position in the search, so we just return the same context
     *   structure that we've been using all along 
     */
    return ctx;
}

/* cancel a search */
void os_find_close(void *ctx0)
{
    ossfcx *ctx = (ossfcx *)ctx0;

    /* if the search was already cancelled, do nothing */
    if (ctx == 0)
        return;

    /* delete the context structure */
    free(ctx);
}

#endif /* MSOS2 */

#ifdef MSDOS
/*
 *   Check for a special filename 
 */
enum os_specfile_t os_is_special_file(const char *fname)
{
    /* check for '.' */
    if (strcmp(fname, ".") == 0)
        return OS_SPECFILE_SELF;

    /* check for '..' */
    if (strcmp(fname, "..") == 0)
        return OS_SPECFILE_PARENT;

    /* not a special file */
    return OS_SPECFILE_NONE;
}

#endif /* MSDOS */

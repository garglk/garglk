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

#ifdef T_WIN32
#include <windows.h>
#include <direct.h>
#endif

#include "os.h"
#include "run.h"

/* ------------------------------------------------------------------------ */
/*
 *   Safe strcpy 
 */
void safe_strcpyl(char *dst, size_t dstlen,
                  const char *src, size_t srclen)
{
    size_t copylen;

    /* do nothing if there's no output buffer */
    if (dst == 0 || dstlen == 0)
        return;

    /* do nothing if the source and destination buffers are the same */
    if (dst == src)
        return;

    /* use an empty string if given a null string */
    if (src == 0)
        src = "";

    /* 
     *   figure the copy length - use the smaller of the actual string size
     *   or the available buffer size, minus one for the null terminator 
     */
    copylen = srclen;
    if (copylen > dstlen - 1)
        copylen = dstlen - 1;

    /* copy the string (or as much as we can) */
    memcpy(dst, src, copylen);

    /* null-terminate it */
    dst[copylen] = '\0';
}

void safe_strcpy(char *dst, size_t dstlen, const char *src)
{
    safe_strcpyl(dst, dstlen, src, src != 0 ? strlen(src) : 0);
}



/* ------------------------------------------------------------------------ */
#define ispathchar(c) \
    ((c) == OSPATHCHAR || ((c) != 0 && strchr(OSPATHALT, c) != 0))

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
#ifndef OSNOUI_OMIT_OS_DEFEXT
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
        if (ispathchar(*p))
            break;
    }

    /* we didn't find an extension - add the dot and the extension */
    strcat(fn, ".");
    strcat(fn, ext);
}
#endif

/*
 *   Add an extension, even if the filename currently has one 
 */
#ifndef OSNOUI_OMIT_OS_ADDEXT
void os_addext(char *fn, const char *ext)
{
    if (strlen(fn) + 1 + strlen(ext) + 1 < OSFNMAX)
    {
        strcat(fn, ".");
        strcat(fn, ext);
    }
}
#endif

/* 
 *   os_remext(fn) removes the extension from fn, if present.  The buffer
 *   at fn should be modified in place.  If no extension is present, no
 *   action should be taken.  For systems without an analogue of
 *   extensions, this routine should do nothing.  
 */
#ifndef OSNOUI_OMIT_OS_REMEXT
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
        if (ispathchar(*p))
            return;
    }
}
#endif

/*
 *   Get a pointer to the root name portion of a filename.  Note that this
 *   implementation is included in the ifdef USE_DOSEXT section, since it
 *   seems safe to assume that if a platform has filenames that are
 *   sufficiently DOS-like for the extension parsing routines, the same
 *   will be true of path parsing.  
 */
#ifndef OSNOUI_OMIT_OS_GET_ROOTNAME
char *os_get_root_name(const char *buf)
{
    const char *rootname;
#ifdef MSDOS
    const char *orig = buf;
#endif

    /* scan the name for path separators */
    for (rootname = buf ; *buf != '\0' ; ++buf)
    {
        /* if this is a path separator, remember it */
        if (ispathchar(*buf))
        {
#ifdef MSDOS
            /* 
             *   if this is the last character in the name, and it's beyond
             *   the second character, assume it's a device name colon rather
             *   than a path separator colon 
             */
            if (*buf == ':' && buf > orig + 1 && buf[1] == '\0')
                continue;
#endif

            /* 
             *   It's a path separators - for now, assume the root will
             *   start at the next character after this separator.  If we
             *   find another separator later, we'll forget about this one
             *   and use the later one instead.  
             */
            rootname = buf + 1;
        }
    }

    /* 
     *   Return the last root name candidate that we found.  (Cast it to
     *   non-const for the caller's convenience: *we're* not allowed to
     *   modify this buffer, but the caller is certainly free to pass in a
     *   writable buffer, and they're free to write to it after we return.)  
     */
    return (char *)rootname;
}
#endif

/*
 *   Extract the path from a filename 
 */
#ifndef OSNOUI_OMIT_OS_GET_PATH_NAME
void os_get_path_name(char *pathbuf, size_t pathbuflen, const char *fname)
{
    const char *lastsep;
    const char *p;
    size_t len;
    int root_path;
    
#ifdef MSDOS
    /* 
     *   Special case for DOS/Windows: If we have a bare drive letter, or an
     *   explicit current directory path on a drive letter (X:.), there is no
     *   parent directory.  Our normal algorithm won't catch this because of
     *   our special handling that adds the working directory to a bare drive
     *   letter (turning "X:" into "X:.").
     */
    if (isalpha(fname[0]) && fname[1] == ':'
        && (fname[2] == '\0' || (fname[2] == '.' && fname[3] == '\0')))
    {
        if (pathbuflen > 0)
            pathbuf[0] = '\0';
        return;
    }
#endif

    /* find the last separator in the filename */
    for (p = fname, lastsep = fname ; *p != '\0' ; ++p)
    {
        /* 
         *   If it's a path separator character, remember it as the last one
         *   we've found so far.  However, don't count it if it's the last
         *   separator - i.e., if only more path separators follow.
         */
        if (ispathchar(*p))
        {
            const char *q;
            
            /* skip any immediately adjacent path separators */
            for (q = p + 1 ; *q != '\0' && ispathchar(*q) ; ++q) ;

            /* if we found more following, *p is the last separator */
            if (*q != '\0')
                lastsep = p;
        }
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
    for (p = fname, root_path = TRUE ; p != lastsep ; ++p)
    {
        /*
         *   if this is NOT a path separator character, we don't have all
         *   path separator characters before the filename, so we don't have
         *   a root path 
         */
        if (!ispathchar(*p))
        {
            /* note that we don't have a root path */
            root_path = FALSE;
            
            /* no need to look any further */
            break;
        }
    }

    /* if we have a root path, keep the final path separator in the path */
    if (root_path && ispathchar(fname[len]))
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

        /* if this was the entire path name, we have no parent */
        if (fname[3] == '\0')
            len = 0;
    }

    /* if the path is of the form X:, keep the trailing colon */
    if (len == 1 && *lastsep == ':')
        len = 2;
#endif
    
    /* make sure it fits in our buffer (with a null terminator) */
    if (len > pathbuflen - 1)
        len = pathbuflen - 1;

    /* copy it and null-terminate it */
    memcpy(pathbuf, fname, len);
    pathbuf[len] = '\0';

#ifdef MSDOS
    /*
     *   Another DOS special case: if the path is of the form "X:", with no
     *   directory element, it refers to the current working directory on
     *   drive X: (DOS/Win keep a working directory per drive letter).  To
     *   make this more explicit, change it to "X:.", so that we actually
     *   have the name of a directory.  
     */
    if (lastsep == fname + 1 && pathbuf[1] == ':' && pathbuflen >= 4)
    {
        pathbuf[2] = '.';
        pathbuf[3] = '\0';
    }
#endif
}
#endif

/*
 *   Canonicalize a path: remove ".." and "." relative elements 
 */
#if !defined(OSNOUI_OMIT_OS_BUILD_FULL_PATH) && !defined(OSNOUI_OMIT_OS_COMBINE_PATHS)
void canonicalize_path(char *path)
{
    char *orig = path;
    char *p;
    char *ele, *prvele;
    char *trimpt = 0;
    int dotcnt = 0;

    /* 
     *   First, skip the root element.  This is unremovable.  For Unix, the
     *   root element is simply a leading slash (or series of slashes).  For
     *   DOS/Win, we can also have drive letters and UNC paths (as in
     *   \\MACHINE\ROOT).
     */
#ifdef MSDOS
    /* check for a UNC path or drive letter */
    if (path[0] == '\\' && path[1] == '\\')
    {
        /* skip to the \ after the machine name */
        for (path += 2 ; *path != '\0' && *path != '\\' && *path != '/' ;
             ++path) ;

        /* 
         *   further skip to the slash after that, as the drive name is part
         *   of the root as well 
         */
        if (*path != '\0')
        {
            /* find the slash */
            for (++path ; *path != '\0' && *path != '\\' && *path != '/' ;
                 ++path) ;

            /* we can trim this slash if the path ends here */
            trimpt = path;
        }
    }
    else if (path[1] == ':' && isalpha(path[0]))
    {
        /* skip the drive letter */
        path += 2;
    }
#endif

    /* skip leading slashes */
    for ( ; ispathchar(*path) ; ++path) ;

    /* 
     *   if we didn't choose a different trim starting point, trim from after
     *   the root slashes 
     */
    if (trimpt == 0)
        trimpt = path;

    /* scan elements */
    for (p = ele = path, prvele = 0 ; *ele != '\0' ; )
    {
        size_t ele_len;

        /* find the end of the current element */
        for ( ; *p != '\0' && !ispathchar(*p) ; ++p) ;
        ele_len = p - ele;

        /* skip adjacent path separators */
        for ( ; ispathchar(*p) ; ++p) ;

        /* check for special elements */
        if (ele_len == 1 && ele[0] == '.')
        {
            /* '.' -> current directory; simply remove this element */
            ++dotcnt;
            memmove(ele, p, strlen(p) + 1);

            /* revisit this element */
            p = ele;
        }
        else if (ele_len == 2 && ele[0] == '.' && ele[1] == '.')
        {
            /* 
             *   '..' -> parent directory; remove this element and the
             *   previous element.  If there is no previous element, or the
             *   previous element is a DOS drive letter or root path slash,
             *   we can't remove it.  Leaving a '..' at the root level will
             *   result in an invalid path, but it would change the meaning
             *   to remove it, so leave it intact.
             *   
             *   There's also a special case if the previous element is
             *   another '..'.  If so, it means this was an unremovable '..',
             *   so we can't do any combining - we need to keep this '..'
             *   AND the previous '..' to retain the meaning.
             */
            ++dotcnt;
            if (prvele == 0
                || ispathchar(*prvele)
                || (prvele[0] == '.' && prvele[1] == '.'
                    && ispathchar(prvele[2]))
#ifdef MSDOS
                || (prvele == path && path[1] == ':' && isalpha(path[0]))
#endif
               )
                prvele = 0;

            /* remove the previous element */
            if (prvele != 0)
            {
                /* remove the element */
                memmove(prvele, p, strlen(p) + 1);

                /* 
                 *   start the scan over from the beginning; this is a little
                 *   inefficient, but it's the easiest way to ensure
                 *   accuracy, since we'd otherwise have to back up to find
                 *   the previous element, which can be tricky 
                 */
                p = ele = path;
                prvele = 0;
            }
            else
            {
                /* 
                 *   no previous element, so keep the .. as it is and move on
                 *   to the next element 
                 */
                prvele = ele;
                ele = p;
            }
        }
        else
        {
            /* ordinary element - move on to the next element */
            prvele = ele;
            ele = p;
        }
    }

    /* if we left any trailing slashes, remove them */
    for (p = trimpt + strlen(trimpt) ;
         p > trimpt && ispathchar(*(p-1)) ;
         *--p = '\0') ;

    /*
     *   If that left us with an empty string, and we had one or more "." or
     *   ".."  elements, the "." and ".." elements must have canceled out the
     *   other elements.  In this case return "." as the result. 
     */
    if (orig[0] == '\0' && dotcnt != 0)
    {
        orig[0] = '.';
        orig[1] = '\0';
    }
}
#endif

/*
 *   General path builder for os_build_full_path() and os_combine_paths().
 *   The two versions do the same work, except that the former canonicalizes
 *   the result (resolving "." and ".." in the last element, for example),
 *   while the latter just builds the combined path literally.
 */
#if !defined(OSNOUI_OMIT_OS_BUILD_FULL_PATH) && !defined(OSNOUI_OMIT_OS_COMBINE_PATHS)
static void build_path(char *fullpathbuf, size_t fullpathbuflen,
                       const char *path, const char *filename, int canon)
{
    size_t plen, flen;
    int add_sep;

    /* presume we'll copy the entire path */
    plen = strlen(path);

#ifdef MSDOS
    /*
     *   On DOS, there's a special case involving root paths without drive
     *   letters.  If the filename starts with a slash, but doesn't look like
     *   a UNC-style name (\\MACHINE\NAME), it's an absolute path on a
     *   relative drive.  This means that we need to copy the drive letter or
     *   UNC prefix from 'path' and add the root path from 'filename'.
     */
    if ((filename[0] == '\\' || filename[0] == '/')
        && !(filename[1] == '\\' && filename[1] == '/'))
    {
        const char *p;
        
        /* 
         *   'filename' is a root path without a drive letter.  Determine if
         *   'path' starts with a drive letter or a UNC path. 
         */
        if (isalpha(path[0]) && path[1] == ':')
        {
            /* drive letter */
            plen = 2;
        }
        else if ((path[0] == '\\' || path[0] == '/')
                 && (path[1] == '\\' || path[1] == '/'))
        {
            /* 
             *   UNC-style name - copy the whole \\MACHINE\PATH part.  Look
             *   for the first path separator after the machine name... 
             */
            for (p = path + 2 ; *p != '\0' && *p != '\\' && *p != '/' ; ++p) ;

            /* ...now look for the next path separator after that */
            if (*p != '\0')
                for (++p ; *p != '\0' && *p != '\\' && *p != '/' ; ++p) ;

            /* copy everything up to but not including the second separator */
            plen = p - path;
        }
        else
        {
            /* 
             *   we have a root path on the filename side, but no drive
             *   letter or UNC prefix on the path side, so there's nothing to
             *   add to the filename 
             */
            plen = 0;
        }
    }

    /*
     *   There's a second special case for DOS.  If the filename has a drive
     *   letter with a relative path (e.g., "d:asdf" - no leading backslash),
     *   we can't apply any of the path.  This sort of path isn't absolute,
     *   in that it depends upon the working directory on the drive, but it's
     *   also not relative, in that it does specify a full directory rather
     *   than a path fragment to add to a root path.
     */
    if (isalpha(filename[0]) && filename[1] == ':'
        && filename[2] != '\\' && filename[2] != '/')
    {
        /* the file has a drive letter - we can't combine it with the path */
        plen = 0;
    }
#endif

    /* if the filename is an absolute path already, leave it as is */
    if (os_is_file_absolute(filename))
        plen = 0;

    /* 
     *   Note whether we need to add a separator.  If the path prefix ends in
     *   a separator, don't add another; otherwise, add the standard system
     *   separator character.
     *   
     *   Don't add a separator if the path is completely empty, since this
     *   simply means that we want to use the current directory.  
     */
    add_sep = (plen != 0 && path[plen] == '\0' && !ispathchar(path[plen-1]));
    
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

    /* if desired, canonicalize the result */
    if (canon)
        canonicalize_path(fullpathbuf);
}
#endif

/*
 *   Build a full path out of path and filename components, returning the
 *   result in canonical form (e.g., with '.' and '..' resolved).
 */
#ifndef OSNOUI_OMIT_OS_BUILD_FULL_PATH
void os_build_full_path(char *fullpathbuf, size_t fullpathbuflen,
                        const char *path, const char *filename)
{
    /* build the combined path, and canonicalize the result */
    build_path(fullpathbuf, fullpathbuflen, path, filename, TRUE);
}
#endif

/*
 *   Build a combined path, returning the literal combination without
 *   resolving any relative links. 
 */
#ifndef OSNOUI_OMIT_OS_COMBINE_PATHS
void os_combine_paths(char *fullpathbuf, size_t fullpathbuflen,
                      const char *path, const char *filename)
{
    /* build the path, without any canonicalization */
    build_path(fullpathbuf, fullpathbuflen, path, filename, FALSE);
}
#endif

/*
 *   Determine if a path is absolute or relative.  If the path starts with
 *   a path separator character, we consider it absolute, otherwise we
 *   consider it relative.
 *   
 *   Note that, on DOS, an absolute path can also follow a drive letter.
 *   So, if the path contains a letter followed by a colon, we'll consider
 *   the path to be absolute. 
 */
#ifndef OSNOUI_OMIT_IS_FILE_ABSOLUTE
int os_is_file_absolute(const char *fname)
{
#ifdef MSDOS
    /* 
     *   on DOS, a file is absolute if it starts with a drive letter followed
     *   by a path separator 
     */
    if (isalpha(fname[0]) && fname[1] == ':'
        && (fname[2] == '/' || fname[2] == '\\'))
        return TRUE;

    /* 
     *   it's also absolute if it starts with a UNC-style path
     *   (\\MACHINE\DRIVE)
     */
    if ((fname[0] == '\\' || fname[0] == '/')
        && (fname[1] == '\\' || fname[1] == '/'))
    {
        const char *p;
            
        /* make sure we have another \ sometime later */
        for (p = fname + 2 ; *p != '\0' && *p != '/' && *p != '\\' ; ++p) ;
        if (*p != '\0')
            return TRUE;
    }

#else /* MSDOS */

    /* if the name starts with a path separator, it's absolute */
    if (ispathchar(fname[0]))
        return TRUE;

#endif

    /* the path is relative */
    return FALSE;
}
#endif


/* 
 *   path letter/substring comparison; define this according to whether the
 *   local file system is case-sensitive (e.g., Unix) or not (DOS, Windows) 
 */
#if defined(MSDOS)
# define pathmemcmp(a, b, len) memicmp(a, b, len)
# define pathchareq(a, b) (toupper(a) == toupper(b))
#endif
#ifndef pathchareq
# define pathmemcmp(a, b, len) memcmp(a, b, len)
# define pathchareq(a, b) ((a) == (b))
#endif


#ifdef MSDOS
/*
 *   Parse a volume name in an absolute path.  DOS/Windows volume names are
 *   simply one-letter drive names - A:, C:, etc.  If there's a drive letter
 *   name, returns the next character after the ':'; if there's not a drive
 *   letter, returns null.
 */
static const char *oss_parse_volume(const char *path)
{
    if (isalpha(path[0]) && path[1] == ':')
        return path + 2;
    else
        return 0;
}
#endif

/*
 *   Get the relative form of a path name.  Given a base path and a filename,
 *   get the name of the file with its path expressed relative to the base
 *   path, if possible.  Both paths must be in absolute format.
 *   
 *   For DOS-like systems with drive letters, if the filename and base path
 *   are on different drives, there's no way to express the filename in
 *   relative terms, so we'll return it unchanged.
 */
#ifndef OSNOUI_OMIT_OS_GET_REL_PATH
int os_get_rel_path(char *result, size_t result_len,
                    const char *basepath, const char *filename)
{
    const char *fp;
    const char *bp;
    const char *fsep;
    const char *bsep;
    size_t rem;
    char *rp;

#if defined(MSDOS)
    /*
     *   DOS and some other systems (e.g., VMS) use drive letter, volume, or
     *   device prefixes, and each drive/volume/device is effectively its own
     *   separate namespace with its own root directory.  There's typically
     *   no syntax on these systems to refer to a container entity above the
     *   volume level, or the parent of a volume root, which means there's no
     *   way to write a filename in relative terms when it's on a different
     *   volume from the base path.
     *   
     *   So: start by checking for a drive/volume prefix.  If both paths have
     *   one (and they both should if the system uses them at all, as both
     *   paths are supposed to be handed to us in absolute format), make sure
     *   they match; if not, we can't form a relative name.
     */
    bp = oss_parse_volume(basepath);
    fp = oss_parse_volume(filename);
    if (bp != 0 && fp != 0
        && (bp - basepath != fp - filename
            || pathmemcmp(basepath, filename, bp - basepath) != 0))
    {
        /* we have volumes, and they don't match - we can't relativize it */
        strncpy(result, filename, result_len - 1);
        result[result_len - 1] = '\0';
        return FALSE;
    }
#endif

    /*
     *   Find the common path prefix.  We can lop off any leading elements
     *   that the two paths have in common.
     */
    for (fp = filename, bp = basepath, fsep = 0, bsep = 0 ;
         pathchareq(*fp, *bp) || (ispathchar(*fp) && ispathchar(*bp)) ;
         ++fp, ++bp)
    {
        /* 
         *   if this is a separator character, note it - we want to keep
         *   track of the last separator in the common portion, since this is
         *   the end of the common path prefix 
         */
        if (ispathchar(*fp) || *fp == '\0')
        {
            fsep = fp;
            bsep = bp;
        }

        /* stop at the end of the strings */
        if (*fp == '\0' || *bp == '\0')
            break;
    }

    /* if we didn't find any separators, we can't relativize the paths */
    if (bsep == 0 || fsep == 0)
    {
        /* nothing in common - return the filename unchanged */
        strncpy(result, filename, result_len - 1);
        result[result_len - 1] = '\0';
        return FALSE;
    }

    /*
     *   If we reached the end of the base path string, and we're at a path
     *   separator in the filename string, then the entire base path prefix
     *   is in common to both names.
     */
    if (*bp == '\0' && (ispathchar(*fp) || *fp == '\0'))
    {
        fsep = fp;
        bsep = bp;
    }

    /* if we're at a path separator in the base path, skip it */
    if (*bsep != '\0')
        ++bsep;

    /* if we're at a path separator in the filename, skip it */
    if (*fsep != '\0')
        ++fsep;

    /*
     *   Everything up to fsep and bsep can be dropped, because it's a common
     *   directory path prefix.  We must now add the relative adjustment
     *   portion: add a ".." directory for each remaining directory in the
     *   base path, since we must move from the base path up to the common
     *   ancestor; then add the rest of the filename path.
     */

    /* 
     *   first, set up to copy into the result buffer - leave space for null
     *   termination
     */
    rp = result;
    rem = result_len - 1;

    /* add a ".." for each remaining directory in the base path string */
    while (*bsep != '\0')
    {
        /* add ".." and a path separator */
        if (rem > 3)
        {
            *rp++ = '.';
            *rp++ = '.';
            *rp++ = OSPATHCHAR;
            rem -= 3;
        }
        else
        {
            /* no more room - give up */
            rem = 0;
            break;
        }

        /* scan to the next path separator */
        for ( ; *bsep != '\0' && !ispathchar(*bsep) ; ++bsep) ;

        /* if this is a path separator, skip it */
        if (*bsep != '\0')
            ++bsep;
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

    /* success */
    return TRUE;
}
#endif


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
    
    if ( (e = getenv(pathvar)) == 0 )
        return(0);
    for ( ;; )
    {
        char   *sep;
        size_t  len;
        
        if ( (sep = strchr(e, OSPATHSEP)) != 0 )
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
        if (!ispathchar(buf[len-1]))
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
        for (p = arg0 + strlen(arg0); p > arg0 && !ispathchar(*(p-1)); --p) ;
        
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
 *   Define the version of memcmp to use for comparing filename path elements
 *   for equality.  For case-sensitive file systems, use memcmp(); for
 *   systems that ignore case, use memicmp().
 */
#if defined(MSDOS) || defined(T_WIN32) || defined(DJGPP) || defined(MSOS2)
# define fname_memcmp memicmp
#endif
#if defined(UNIX)
# define fname_memcmp memcmp
#endif

#if defined(GARGOYLE) && !defined(fname_memcmp)
#  define fname_memcmp memcmp
#endif


/*
 *   Get the next earlier element of a path
 */
static const char *prev_path_ele(const char *start, const char *p,
                                 size_t *ele_len)
{
    int cancel = 0;
    const char *dotdot = 0;

    /* if we're at the start of the string, there are no more elements */
    if (p == start)
        return 0;

    /* keep going until we find a suitable element */
    for (;;)
    {
        const char *endp;

        /*
         *   If we've reached the start of the string, it means that we have
         *   ".."'s that canceled out every earlier element of the string.
         *   If the cancel count is non-zero, it means that we have one or
         *   more ".."'s that are significant (in that they cancel out
         *   relative elements before the start of the string).  If the
         *   cancel count is zero, it means that we've exactly canceled out
         *   all remaining elements in the string.
         */
        if (p == start)
        {
            *ele_len = (dotdot != 0 ? 2 : 0);
            return dotdot;
        }
        
        /* 
         *   back up through any adjacent path separators before the current
         *   element, so that we're pointing to the first separator after the
         *   previous element
         */
        for ( ; p != start && ispathchar(*(p-1)) ; --p) ;

        /*
         *   If we're at the start of the string, this is an absolute path.
         *   Treat it specially by returning a zero-length initial element. 
         */
        if (p == start)
        {
            *ele_len = 0;
            return p;
        }

        /* note where the current element ends */
        endp = p;

        /* now back up to the path separator before this element */
        for ( ; p != start && !ispathchar(*(p-1)) ; --p) ;

        /* 
         *   if this is ".", skip it, since this simply means that this
         *   element matches the same folder as the previous element 
         */
        if (endp - p == 1 && p[0] == '.')
            continue;

        /* 
         *   if this is "..", it cancels out the preceding non-relative
         *   element; up the cancel count and keep searching 
         */
        if (endp - p == 2 && p[0] == '.' && p[1] == '.')
        {
            /* up the cancel count */
            ++cancel;

            /* if this is the first ".." we've encountered, note it */
            if (dotdot == 0)
                dotdot = p;

            /* keep searching */
            continue;
        }

        /*
         *   This is an ordinary path element, not a relative "." or ".."
         *   link.  If we have a non-zero cancel count, we're still working
         *   on canceling out elements from ".."'s we found later in the
         *   string.
         */
        if (cancel != 0)
        {
            /* this absorbs one level of cancellation */
            --cancel;

            /* 
             *   if that's the last cancellation, we've absorbed all ".."
             *   effects, so the last ".." we found is no longer significant 
             */
            if (cancel == 0)
                dotdot = 0;

            /* keep searching */
            continue;
        }

        /* this item isn't canceled out by a "..", so it's our winner */
        *ele_len = endp - p;
        return p;
    }
}


/*
 *   Compare two paths for equality 
 */
int os_file_names_equal(const char *a, const char *b)
{
    /* start at the end of each name and work backwards */
    const char *pa = a + strlen(a), *pb = b + strlen(b);

    /* keep going until we reach the start of one or the other path */
    for (;;)
    {
        size_t lena, lenb;

        /* get the next earlier element of each path */
        pa = prev_path_ele(a, pa, &lena);
        pb = prev_path_ele(b, pb, &lenb);

        /* if one or the other ran out, we're done */
        if (pa == 0 || pb == 0)
        {
            /* the paths match if they ran out at the same time */
            return pa == pb;
        }

        /* if the two elements don't match, return unequal */
        if (lena != lenb || fname_memcmp(pa, pb, lena) != 0)
            return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   ISAAC random number generator.  This is a small, fast, cryptographic
 *   quality random number generator that we use internally for some generic
 *   purposes:
 *   
 *   - for os_gen_temp_filename(), we use it to generate a GUID-style random
 *   filename
 *   
 *   - for our generic implementation of os_gen_rand_bytes(), we use it as
 *   the source of the random bytes 
 */

/* 
 *   include ISAAC if we're using our generic temporary filename generator
 *   with long filenames, or we're using our generic os_gen_rand_bytes() 
 */
#if !defined(OSNOUI_OMIT_TEMPFILE) && (defined(T_WIN32) || !defined(MSDOS))
#define INCLUDE_ISAAC
#endif
#ifdef USE_GENRAND
#define INCLUDE_ISAAC
#endif

#ifdef INCLUDE_ISAAC
/*
 *   ISAAC random number generator implementation, for generating
 *   GUID-strength random temporary filenames 
 */
#define ISAAC_RANDSIZL   (8)
#define ISAAC_RANDSIZ    (1<<ISAAC_RANDSIZL)
static struct isaacctx
{
    /* RNG context */
    unsigned long cnt;
    unsigned long rsl[ISAAC_RANDSIZ];
    unsigned long mem[ISAAC_RANDSIZ];
    unsigned long a;
    unsigned long b;
    unsigned long c;
} *S_isaacctx;

#define _isaac_rand(r) \
    ((r)->cnt-- == 0 ? \
     (isaac_gen_group(r), (r)->cnt=ISAAC_RANDSIZ-1, (r)->rsl[(r)->cnt]) : \
     (r)->rsl[(r)->cnt])
#define isaac_rand() _isaac_rand(S_isaacctx)

#define isaac_ind(mm,x)  ((mm)[(x>>2)&(ISAAC_RANDSIZ-1)])
#define isaac_step(mix,a,b,mm,m,m2,r,x) \
    { \
        x = *m;  \
        a = ((a^(mix)) + *(m2++)) & 0xffffffff; \
        *(m++) = y = (isaac_ind(mm,x) + a + b) & 0xffffffff; \
        *(r++) = b = (isaac_ind(mm,y>>ISAAC_RANDSIZL) + x) & 0xffffffff; \
    }

#define isaac_mix(a,b,c,d,e,f,g,h) \
    { \
        a^=b<<11; d+=a; b+=c; \
        b^=c>>2;  e+=b; c+=d; \
        c^=d<<8;  f+=c; d+=e; \
        d^=e>>16; g+=d; e+=f; \
        e^=f<<10; h+=e; f+=g; \
        f^=g>>4;  a+=f; g+=h; \
        g^=h<<8;  b+=g; h+=a; \
        h^=a>>9;  c+=h; a+=b; \
    }

/* generate the group of numbers */
static void isaac_gen_group(struct isaacctx *ctx)
{
    unsigned long a;
    unsigned long b;
    unsigned long x;
    unsigned long y;
    unsigned long *m;
    unsigned long *mm;
    unsigned long *m2;
    unsigned long *r;
    unsigned long *mend;

    mm = ctx->mem;
    r = ctx->rsl;
    a = ctx->a;
    b = (ctx->b + (++ctx->c)) & 0xffffffff;
    for (m = mm, mend = m2 = m + (ISAAC_RANDSIZ/2) ; m<mend ; )
    {
        isaac_step(a<<13, a, b, mm, m, m2, r, x);
        isaac_step(a>>6,  a, b, mm, m, m2, r, x);
        isaac_step(a<<2,  a, b, mm, m, m2, r, x);
        isaac_step(a>>16, a, b, mm, m, m2, r, x);
    }
    for (m2 = mm; m2<mend; )
    {
        isaac_step(a<<13, a, b, mm, m, m2, r, x);
        isaac_step(a>>6,  a, b, mm, m, m2, r, x);
        isaac_step(a<<2,  a, b, mm, m, m2, r, x);
        isaac_step(a>>16, a, b, mm, m, m2, r, x);
    }
    ctx->b = b;
    ctx->a = a;
}

static void isaac_init(unsigned long *rsl)
{
    static int inited = FALSE;
    int i;
    unsigned long a;
    unsigned long b;
    unsigned long c;
    unsigned long d;
    unsigned long e;
    unsigned long f;
    unsigned long g;
    unsigned long h;
    unsigned long *m;
    unsigned long *r;
    struct isaacctx *ctx;

    /* allocate the context if we don't already have it set up */
    if ((ctx = S_isaacctx) == 0)
        ctx = S_isaacctx = (struct isaacctx *)malloc(sizeof(struct isaacctx));

    /* 
     *   If we're already initialized, AND the caller isn't re-seeding with
     *   explicit data, we're done.  
     */
    if (inited && rsl == 0)
        return;
    inited = TRUE;

    ctx->a = ctx->b = ctx->c = 0;
    m = ctx->mem;
    r = ctx->rsl;
    a = b = c = d = e = f = g = h = 0x9e3779b9;         /* the golden ratio */

    /* scramble the initial settings */
    for (i = 0 ; i < 4 ; ++i)
    {
        isaac_mix(a, b, c, d, e, f, g, h);
    }

    /* 
     *   if they sent in explicit initialization bytes, use them; otherwise
     *   seed the generator with truly random bytes from the system 
     */
    if (rsl != 0)
        memcpy(ctx->rsl, rsl, sizeof(ctx->rsl));
    else
        os_gen_rand_bytes((unsigned char *)ctx->rsl, sizeof(ctx->rsl));

    /* initialize using the contents of ctx->rsl[] as the seed */
    for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
    {
        a += r[i];   b += r[i+1]; c += r[i+2]; d += r[i+3];
        e += r[i+4]; f += r[i+5]; g += r[i+6]; h += r[i+7];
        isaac_mix(a, b, c, d, e, f, g, h);
        m[i] = a;   m[i+1] = b; m[i+2] = c; m[i+3] = d;
        m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }

    /* do a second pass to make all of the seed affect all of m */
    for (i = 0 ; i < ISAAC_RANDSIZ ; i += 8)
    {
        a += m[i];   b += m[i+1]; c += m[i+2]; d += m[i+3];
        e += m[i+4]; f += m[i+5]; g += m[i+6]; h += m[i+7];
        isaac_mix(a, b, c, d, e, f, g, h);
        m[i] = a;   m[i+1] = b; m[i+2] = c; m[i+3] = d;
        m[i+4] = e; m[i+5] = f; m[i+6] = g; m[i+7] = h;
    }

    /* fill in the first set of results */
    isaac_gen_group(ctx);

    /* prepare to use the first set of results */    
    ctx->cnt = ISAAC_RANDSIZ;
}
#endif /* INCLUDE_ISAAC */


/* ------------------------------------------------------------------------ */
/*
 *   Generic implementation of os_gen_rand_bytes().  This can be used when
 *   the operating system doesn't have a native source of true randomness,
 *   but prefereably only as a last resort - see below for why.
 *   
 *   This generator uses ISAAC to generate the bytes, seeded by the system
 *   time.  This algorithm isn't nearly as good as using a native OS-level
 *   randomness source, since any decent OS API will have access to
 *   considerably more sources of true entropy.  In a portable setting, we
 *   have very little in the way of true randomness available.  The most
 *   reliable portable source of randomness is the system clock; if it has
 *   resolution in the millisecond range, this gives us roughly 10-12 bits of
 *   real entropy if we assume that the user is running the program manually,
 *   in that there should be no pattern to the exact program start time -
 *   even a series of back-to-back runs would be pretty random in the part of
 *   the time below about 1 second.  We *might* also be able to get a little
 *   randomness from the memory environment, such as the current stack
 *   pointer, the state of the malloc() allocator as revealed by the address
 *   of a newly allocated block, the contents of uninitialized stack
 *   variables and uninitialized malloc blocks, and the contents of the
 *   system registers as saved by setjmp.  These memory settings are not
 *   entirely likely to vary much from one run to the next: most modern OSes
 *   virtualize the process environment to such an extent that each fresh run
 *   will start with exactly the same initial memory environment, including
 *   the stack address and malloc heap configuration.
 *   
 *   We make the most of these limited entropy sources by using them to seed
 *   an ISAAC RNG, then generating the returned random bytes via ISAAC.
 *   ISAAC's design as a cryptographic RNG means that it thoroughly mixes up
 *   the meager set of random bits we hand it, so the bytes returned will
 *   statistically look nice and random.  However, don't be fooled into
 *   thinking that this magnifies the basic entropy we have as inputs - it
 *   doesn't.  If we only have 10-12 bits of entropy from the timer, and
 *   everything else is static, our byte sequence will represent 10-12 bits
 *   of entropy scattered around through a large set of mathematically (and
 *   deterministically) derived bits.  The danger is that the "birthday
 *   problem" dictates that with 12 bits of variation from one run to the
 *   next, we'd have a good chance of seeing a repeat of the *exact same byte
 *   sequence* within about 100 runs.  This is why it's so much better to
 *   customize this routine using a native OS mechanism whenever possible.  
 */
#ifdef USE_GENRAND

void os_gen_rand_bytes(unsigned char *buf, size_t buflen)
{
    union
    {
        unsigned long r[ISAAC_RANDSIZ];
        struct
        {
            unsigned long r1[15];
            jmp_buf env;
        } o;
    } r;
    void *p, *q;
    
    /* 
     *   Seed ISAAC with what little entropy we have access to in a generic
     *   cross-platform implementation:
     *   
     *   - the current wall-clock time
     *.  - the high-precision (millisecond) system timer
     *.  - the current stack pointer
     *.  - an arbitrary heap pointer obtained from malloc()
     *.  - whatever garbage is in the random heap pointer from malloc()
     *.  - whatever random garbage is in the rest of our stack buffer 'r'
     *.  - the contents of system registers from 'setjmp'
     *   
     *   The millisecond timer is by far the most reliable source of real
     *   entropy we have available.  The wall clock time doesn't vary quickly
     *   enough to produce more than a few bits of entropy from run to run;
     *   all of the memory factors could be absolutely deterministic from run
     *   to run, depending on how thoroughly the OS virtualizes the process
     *   environment at the start of a run.  For example, some systems clear
     *   memory pages allocated to a process, as a security measure to
     *   prevent old data from one process from becoming readable by another
     *   process.  Some systems virtualize the memory space such that the
     *   program is always loaded at the same virtual address, always has its
     *   stack at the same virtual address, etc.
     *   
     *   Note that we try to add some variability to our malloc heap probing,
     *   first by making the allocation size vary according to the low bits
     *   of the system millisecond timer, then by doing a second allocation
     *   to take into account the effect of the randomized size of the first
     *   allocation.  This should avoid getting the exact same results every
     *   time we run, even if the OS arranges for the heap to have exactly
     *   the same initial layout with every run, since our second malloc will
     *   have initial conditions that vary according to the size our first
     *   malloc.  This arguably doesn't introduce a lot of additional real
     *   entropy, since we're already using the system timer directly in the
     *   calculation anyway: in a sufficiently predictable enough heap
     *   environment, our two malloc() calls will yield the same results for
     *   a given timer value, so we're effectively adding f(timer) for some
     *   deterministic function f(), which is the same in terms of additional
     *   real entropy as just adding the timer again, which is the same as
     *   adding nothing.  
     */
    r.r[0] = (unsigned long)time(0);
    r.r[1] = (unsigned long)os_get_sys_clock_ms();
    r.r[2] = (unsigned long)buf;
    r.r[3] = (unsigned long)&buf;
    p = malloc((size_t)(os_get_sys_clock_ms() & 1023) + 17);
    r.r[4] = (unsigned long)p;
    r.r[5] = *(unsigned long *)p;
    r.r[6] = *((unsigned long *)p + 10);
    q = malloc(((size_t)p & 1023) + 19);
    r.r[7] = (unsigned long)p;
    r.r[8] = *(unsigned long *)p;
    r.r[9] = *((unsigned long *)p + 10);
    setjmp(r.o.env);

    free(p);
    free(q);

    /* initialize isaac with our seed data */
    isaac_init(r.r);

    /* generate random bytes from isaac to fill the buffer */
    while (buflen > 0)
    {
        unsigned long n;
        size_t copylen;
        
        /* generate a number */
        n = isaac_rand();

        /* copy it into the buffer */
        copylen = buflen < sizeof(n) ? buflen : sizeof(n);
        memcpy(buf, &n, copylen);

        /* advance our buffer pointer */
        buf += copylen;
        buflen -= copylen;
    }
}

#endif /* USE_GENRAND */

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
    const char *fname;

    /* if a name wasn't provided, generate a name */
    if (swapname == 0)
    {
        /* generate a name for the file */
        os_gen_temp_filename(buf, OSFNMAX);

        /* use the generated name */
        fname = buf;
    }
    else
    {
        /* use the name they passed in */
        fname = swapname;
    }

    /* open the file in binary write/truncate mode */
    fp = osfoprwtb(fname, OSFTSWAP);

    /* set the file's type in the OS, if necessary */
    os_settype(fname, OSFTSWAP);

    /* return the file pointer */
    return fp;
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

#if defined(MSDOS) && !defined(T_WIN32)
#define SHORT_FILENAMES
#endif

#ifndef SHORT_FILENAMES
/*
 *   Generate a name for a temporary file.  This is the long filename
 *   version, suitable only for platforms that can handle filenames of at
 *   least 45 characters in just the root name portion.  For systems with
 *   short filenames (e.g., MS-DOS, this must use a different algorithm - see
 *   the MSDOS section below for a fairly portable "8.3" implementation.  
 */
int os_gen_temp_filename(char *buf, size_t buflen)
{
    char tmpdir[OSFNMAX], fname[50];

    /* get the system temporary directory */
    os_get_tmp_path(tmpdir);

    /* seed ISAAC with random data from the system */
    isaac_init(0);

    /* 
     *   Generate a GUID-strength random filename.  ISAAC is a cryptographic
     *   quality RNG, so the chances of collisions with other filenames
     *   should be effectively zero. 
     */
    sprintf(fname, "TADS-%08lx-%08lx-%08lx-%08lx.tmp",
            isaac_rand(), isaac_rand(), isaac_rand(), isaac_rand());

    /* build the full path */
    os_build_full_path(buf, buflen, tmpdir, fname);

    /* success */
    return TRUE;
}

#endif
#endif /* OSNOUI_OMIT_TEMPFILE */

/* ------------------------------------------------------------------------ */

/*
 *   print a null-terminated string to osfildef* file 
 */
#ifndef OSNOUI_OMIT_OS_FPRINTZ
void os_fprintz(osfildef *fp, const char *str)
{
    fprintf(fp, "%s", str);
}
#endif

/* 
 *   print a counted-length string (which might not be null-terminated) to a
 *   file 
 */
#ifndef OSNOUI_OMIT_OS_FPRINT
void os_fprint(osfildef *fp, const char *str, size_t len)
{
    fprintf(fp, "%.*s", (int)len, str);
}
#endif

/* ------------------------------------------------------------------------ */

#ifdef T_WIN32
/*
 *   Windows implementation - get the temporary file path. 
 */
void os_get_tmp_path(char *buf)
{
    GetTempPath(OSFNMAX, buf);
}
#endif

#if defined(MSDOS) && !defined(T_WIN32)
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

/*
 *   Generate a name for a temporary file.  This implementation is suitable
 *   for MS-DOS and other platforms with short filenames - for the simpler
 *   and more generic long-filename version, see above.  On systems where
 *   filenames are limited to short names, we can't pack enough randomness
 *   into a filename to guarantee uniqueness by virtue of randomness alone,
 *   so we cope by actually checking for an existing file for each random
 *   name we roll up, trying again if our selected name is in use.  
 */
int os_gen_temp_filename(char *buf, size_t buflen)
{
    osfildef *fp;
    int attempt;
    size_t len;
    time_t timer;
    int pass;

    /* 
     *   Fail if our buffer is smaller than OSFNMAX.  os_get_tmp_path()
     *   assumes an OSFNMAX-sized buffer, so we'll have problems if the
     *   passed-in buffer is shorter than that. 
     */
    if (buflen < OSFNMAX)
        return FALSE;

    /* 
     *   Try a few times with the temporary file path; if we can't find an
     *   available filename there, try again with the working directory.  
     */
    for (pass = 1 ; pass <= 2 ; ++pass)
    {
        /* get the directory path */
        if (pass == 1)
        {
            /* first pass - use the system temp directory */
            os_get_tmp_path(buf);
            len = strlen(buf);
        }
        else
        {
            /* 
             *   second pass - we couldn't find any free names in the system
             *   temp directory, so try the working directory 
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

                    /* close the file - it's just an empty placeholder */
                    osfcls(fp);

                    /* return success */
                    return TRUE;
                }
            }
        }
    }
    
    /* we couldn't find a free filename - return failure */
    return FALSE;
}

#endif /* MSDOS */

#ifdef USE_NULLSTYPE
void os_settype(const char *f, os_filetype_t t)
{
    /* nothing needs to be done on this system */
}
#endif /* USE_NULLSTYPE */

/* ------------------------------------------------------------------------ */
/*
 *   URL/local path conversion 
 */

#if defined(TURBO) || defined(DJGPP) || defined(MICROSOFT) || defined(MSOS2)
/* length-protected character adder for os_cvt_xxx */
static void cvtaddchar(char **dst, size_t *rem, char c)
{
    if (*rem > 1)
    {
        **dst = c;
        *dst += 1;
        *rem -= 1;
    }
}
static void cvtaddchars(char **dst, size_t *rem, const char *src, size_t len)
{
    for ( ; len > 0 ; --len)
        cvtaddchar(dst, rem, *src++);
}
#endif

/*
 *   Convert an OS filename path to a relative URL 
 */
void os_cvt_dir_url(char *result_buf, size_t result_buf_size,
                    const char *src_path)
{
    char *dst = result_buf;
    const char *src = src_path;
    size_t rem = result_buf_size;

#if defined(TURBO) || defined(DJGPP) || defined(MICROSOFT) || defined(MSOS2)
    /*
     *   If there's a DOS/Windows drive letter, start with the drive letter
     *   and leading '\', if present, as a separate path element.  If it's a
     *   UNC-style path, add the UNC \\MACHINE\SHARE as the first element.
     *   
     *   In either case, we'll leave the source pointer positioned at the
     *   rest of the path after the drive root or UNC share, which means
     *   we're pointing to the relative portion of the path that follows.
     *   The normal algorithm will simply convert this to a relative URL that
     *   will be tacked on to the absolute root URL portion that we generate
     *   here, so we'll have the correct overall format.
     */
    if (isalpha(src[0]) && src[1] == ':')
    {
        /* start with /X: */
        cvtaddchar(&dst, &rem, '/');
        cvtaddchars(&dst, &rem, src, 2);
        src += 2;

        /* 
         *   if it's just "X:" and not "X:\", translate it to "X:." to make
         *   it explicit that we're talking about the working directory on X:
         */
        if (*src != '\\' && *src != '/')
            cvtaddchars(&dst, &rem, "./", 2);
    }
    else if ((src[0] == '\\' || src[0] == '/')
             && (src[1] == '\\' || src[1] == '/'))
    {
        const char *p;

        /* 
         *   UNC-style path.  Find the next separator to get the end of the
         *   machine name.
         */
        for (p = src + 2 ; *p != '\0' && *p != '/' && *p != '\\' ; ++p) ;

        /* start with /\\MACHINE */
        cvtaddchar(&dst, &rem, '/');
        cvtaddchars(&dst, &rem, src, p - src);

        /* skip to the path separator */
        src = p;
    }
#endif /* DOS */

    /*
     *   Run through the source buffer, copying characters to the output
     *   buffer.  If we encounter a path separator character, replace it with
     *   a forward slash.  
     */
    for ( ; *src != '\0' && rem > 1 ; ++dst, ++src, --rem)
    {
        /* 
         *   If this is a local path separator character, replace it with the
         *   URL-style path separator character.  Otherwise, copy it
         *   unchanged.  
         */
        if (strchr(OSPATHURL, *src) != 0)
        {
            /* add the URL-style path separator instead of the local one */
            *dst = '/';
        }
        else
        {
            /* add the character unchanged */
            *dst = *src;
        }
    }

    /* remove any trailing separators (unless the whole path is "/") */
    while (dst > result_buf + 1 && *(dst-1) == '/')
        --dst;

    /* add a null terminator */
    *dst = '\0';
}

/*
 *   Convert a relative URL to a relative file system path name 
 */
#ifndef OSNOUI_OMIT_OS_CVT_URL_DIR
void os_cvt_url_dir(char *result_buf, size_t result_buf_size,
                    const char *src_url)
{
    char *dst = result_buf;
    const char *src = src_url;
    size_t rem = result_buf_size;

    /* 
     *   check for an absolute path 
     */
#if defined(TURBO) || defined(DJGPP) || defined(MICROSOFT) || defined(MSOS2)
    if (*src == '/')
    {
        const char *p;
        int is_drive, is_unc = FALSE;

        /* we have an absolute path - find the end of the first element */
        for (p = ++src ; *p != '\0' && *p != '/' ; ++p) ;

        /* check to see if it looks like a drive letter */
        is_drive = (isalpha(src[0]) && src[1] == ':'
                    && (p - src == 2
                        || (p - src == 3 && src[2] == '.')));

        /* check to see if it looks like a UNC-style path */
        is_unc = (src[0] == '\\' && src[1] == '\\');

        /* 
         *   if it's a drive letter or UNC path, it's a valid Windows root
         *   path element - copy it exactly, then decode the rest of the path
         *   as a simple relative path relative to this root 
         */
        if (is_drive || is_unc)
        {
            /* it's a drive letter or drive root path - copy it exactly */
            cvtaddchars(&dst, &rem, src, p - src);

            /* 
             *   if it's an X:. path, remove the . and the following path
             *   separator 
             */
            if (is_drive && p - src == 3 && src[2] == '.')
            {
                /* undo the '.' */
                --dst;
                ++rem;

                /* skip the '/' if present */
                if (*p == '/')
                    ++p;
            }

            /* skip to the '/' */
            src = p;
        }
        else
        {
            /* 
             *   It's not a valid DOS root element, so make this a
             *   non-drive-letter root path, converting the first element as
             *   a directory name.
             */
            cvtaddchar(&dst, &rem, '\\');
        }
    }
    else if (isalpha(src[0]) && src[1] == ':')
    {
        /*
         *   As a special case, assume that a path starting with "X:" (where
         *   X is any letter) is a Windows/DOS drive letter prefix.  This
         *   doesn't fit our new (as of Jan 2012) rules for converting paths
         *   to URLs, but it's what older versions did, so it provides
         *   compatibility.  There's a small price for this compatibility,
         *   which is that it's possible in principle for a Unix relative
         *   path to look the same way - you could have a Unix directory
         *   called "c:", so "c:/dir" would be a valid relative path.  But
         *   it's extremely uncommon for Unix users to use colons in
         *   directory names for a couple of reasons; one is that it creates
         *   interop problems because practically every other file system
         *   treats ':' as a special syntax element, and the other is that
         *   ':' is conventionally used on Unix itself as a delimiter in path
         *   lists, so while it isn't formally a special character in file
         *   names, it's effectively a special character.
         */
        cvtaddchars(&dst, &rem, src, 2);
        src += 2;
    }
#endif

    /*
     *   Run through the source buffer, copying characters to the output
     *   buffer.  If we encounter a '/', convert it to a path separator
     *   character.
     */
    for ( ; *src != '\0' && rem > 1 ; ++src, ++dst, --rem)
    {
        /* 
         *   replace slashes with path separators; expand '%' sequences; copy
         *   all other characters unchanged 
         */
        if (*src == '/')
        {
            /* change '/' to the local path separator */
            *dst = OSPATHCHAR;
        }
        else if ((unsigned char)*src < 32
#if defined(TURBO) || defined(DJGPP) || defined(MICROSOFT) || defined(MSOS2)
                 || strchr("*+?=[]\\&|\":<>", *src) != 0
#endif
                )
        {
            *dst = '_';
        }
        else
        {
            /* copy this character unchanged */
            *dst = *src;
        }
    }

    /* add a null terminator and we're done */
    *dst = '\0';
}
#endif


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

struct oss_find_ctx_t
{
    struct ffblk ff;
    int found;
};

int os_open_dir(const char *dir, osdirhdl_t *hdl)
{
    char *pat;
    size_t patsiz;
    struct oss_find_ctx_t *ctx;
    
    /* create a copy of the string with "\*.*" appended */
    patsiz = strlen(dir) + 32;
    pat = (char *)malloc(patsiz);
    os_build_full_path(pat, patsiz, dir, "*.*");
    if (pat == 0)
        return FALSE;

    /* allocate a search block structure */
    ctx = (struct oss_find_ctx_t *)malloc(sizeof(struct oss_find_ctx_t));
    if (ctx == 0)
    {
        free(pat);
        return FALSE;
    }

    /* begin the search */
    ctx->found = (findfirst(pat, &ctx->ff, FA_DIREC) == 0);

    /* done with the pattern buffer */
    free(pat);

    /* return the context as the search handle and indicate success */
    *hdl = ctx;
    return TRUE;
}

int os_read_dir(osdirhdl_t hdl, char *buf, size_t buflen)
{
    /* cast the handle to our private structure */
    struct oss_find_ctx_t *ctx = (struct oss_find_ctx_t *)hdl;
    
    /* if we're out of files, return failure */
    if (!ctx->found)
        return FALSE;

    /* return the current file */
    safe_strcpy(buf, buflen, ctx->ff.ff_name);

    /* move on to the next file */
    ctx->found = (findnext(&ctx->ff) == 0);

    /* success */
    return TRUE;
}

void os_close_dir(osdirhdl_t hdl)
{
    /* free the context structure */
    free(hdl);
}

#if 0 // the os_find_xxx family of functions has been DEPRECATED
/*
 *   search context structure - DEPRECATED
 */
struct oss_find_ctx_t
{
    /* C library find-file block */
    struct ffblk ff;

    /* original search path prefix (we'll allocate more to fit the string) */
    char path[1];
};

/*
 *   find first matching file - DEPRECATED
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

/* find the next file - DEPRECATED */
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

/* cancel a search - DEPRECATED */
void os_find_close(void *ctx0)
{
    struct oss_find_ctx_t *ctx = (struct oss_find_ctx_t *)ctx0;

    /* if the search was already cancelled, do nothing */
    if (ctx == 0)
        return;

    /* delete the search context */
    free(ctx);
}
#endif /* DEPRECATED */

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
 *   Get file attributes 
 */
unsigned long oss_get_file_attrs(const char *fname)
{
    struct ffblk ff;
    unsigned long attrs = 0;

    /* look up the file */
    if (!findfirst(fname, &ff, FA_DIREC))
    {
        /* translate the HIDDEN and SYSTEM bits */
        if ((ff.ff_attrib & FA_HIDDEN) != 0)
            attrs |= OSFATTR_HIDDEN;
        if ((ff.ff_attrib & FA_SYSTEM) != 0)
            attrs |= OSFATTR_SYSTEM;

        /* if the RDONLY bit is set, it's readable, otherwise read/write */
        attrs |= OSFATTR_READ;
        if ((ff.ff_attrib & FA_RDONLY) == 0)
            attrs |= OSFATTR_WRITE;
    }

    /* return the attributes */
    return attrs;
}

/*
 *   Get file status information 
 */
int os_file_stat(const char *fname, int follow_links, os_file_stat_t *s)
{
    struct stat info;
    struct ffblk ff;

    /* get the file information */
    if (stat(fname, &info))
    {
        /* 
         *   stat() doesn't work for devices; try running through osfmode(),
         *   which does special checks for reserved device names
         */
        if (osfmode(fname, follow_links, &s->mode, &s->attrs))
        {
            /* devices have no sizes or timestamps */
            s->sizehi = 0;
            s->sizelo = 0;
            s->cre_time = 0;
            s->mod_time = 0;
            s->acc_time = 0;
            
            /* success */
            return TRUE;
        }

        /* failed */
        return FALSE;
    }

    /* translate the status fields */
    if (sizeof(info.st_size) <= 4)
    {
        s->sizelo = info.st_size;
        s->sizehi = 0;
    }
    else
    {
        s->sizelo = (uint32_t)(info.st_size & 0xFFFFFFFF);
        s->sizehi = (uint32_t)(info.st_size >> 32);
    }
    s->cre_time = info.st_ctime;
    s->mod_time = info.st_mtime;
    s->acc_time = info.st_atime;
    s->mode = info.st_mode;

    /* get the DOS mode information */
    s->attrs = oss_get_file_attrs(fname);

    /* success */
    return TRUE;
}

/*
 *   Resolve a symbolic link 
 */
int os_resolve_symlink(const char *fname, char *target, size_t target_size)
{
    /* DOS doesn't support symbolic links */
    return FALSE;
}


#endif /* TURBO || DJGPP */

/* ------------------------------------------------------------------------ */
/*
 *   Microsoft C implementation of directory functions 
 */
#ifdef MICROSOFT

struct oss_find_ctx_t
{
    /* _findfirst/_findnext search handle */
    long handle;

    /* return data from _findfirst/_findnext */
    struct _finddata_t data;

    /* does 'data' contain a valid file description? */
    int found;
};

int os_open_dir(const char *dir, osdirhdl_t *hdl)
{
    char *pat;
    size_t patsiz;
    struct oss_find_ctx_t *ctx;

    /* create a copy of the string with "\*" appended */
    patsiz = strlen(dir) + 32;
    pat = (char *)malloc(patsiz);
    os_build_full_path(pat, patsiz, dir, "*");
    if (pat == 0)
        return FALSE;

    /* allocate a search block structure */
    ctx = (struct oss_find_ctx_t *)malloc(sizeof(struct oss_find_ctx_t));
    if (ctx == 0)
    {
        free(pat);
        return FALSE;
    }

    /* begin the search */
    ctx->handle = _findfirst(pat, &ctx->data);

    /* done with the pattern string */
    free(pat);

    /* if the search failed, free resources and return failure */
    if (ctx->handle == -1L)
    {
        free(ctx);
        return FALSE;
    }

    /* return the context as the search handle and indicate success */
    ctx->found = TRUE;
    *hdl = ctx;
    return TRUE;
}

int os_read_dir(osdirhdl_t hdl, char *buf, size_t buflen)
{
    /* cast the handle to our private structure */
    struct oss_find_ctx_t *ctx = (struct oss_find_ctx_t *)hdl;

    /* if we're out of files, return failure */
    if (!ctx->found)
        return FALSE;

    /* return the current file */
    safe_strcpy(buf, buflen, ctx->data.name);

    /* move on to the next file */
    ctx->found = (_findnext(ctx->handle, &ctx->data) == 0);

    /* success */
    return TRUE;
}

void os_close_dir(osdirhdl_t hdl)
{
    /* cast the handle to our private structure */
    struct oss_find_ctx_t *ctx = (struct oss_find_ctx_t *)hdl;

    /* close the system search handle */
    _findclose(ctx->handle);
    
    /* free the context structure */
    free(hdl);
}


#if 0 // the os_find_xxx family of functions has been DEPRECATED
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
#endif /* DEPCRECATED */

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
 *   Get the DOS attributes for a file
 */
unsigned long oss_get_file_attrs(const char *fname)
{
    unsigned long attrs = 0;
    struct _finddata_t ff;
    
    /* get the DOS attribute flags */
    ff.attrib = 0;
#ifdef T_WIN32
    ff.attrib = GetFileAttributes(fname);
#else
    {
        long h = _findfirst(fname, &ff);
        if (h != -1L)
            _findclose(h);
    }
#endif

    /* translate the HIDDEN and SYSTEM bits */
    if ((ff.attrib & _A_HIDDEN) != 0)
        attrs |= OSFATTR_HIDDEN;
    if ((ff.attrib & _A_SYSTEM) != 0)
        attrs |= OSFATTR_SYSTEM;

    /* if the RDONLY bit is set, it's readable, otherwise read/write */
    attrs |= OSFATTR_READ;
    if ((ff.attrib & _A_RDONLY) == 0)
        attrs |= OSFATTR_WRITE;

#ifdef T_WIN32
    /*
     *   On Windows, attempt to do a full security manager check to determine
     *   our actual access rights to the file.  If we fail to get the
     *   security info, we'll give up and return the simple RDONLY
     *   determination we just made above.  In most cases, failure to check
     *   the security information will simply be because the file has no ACL,
     *   which means that anyone can access it, hence our tentative result
     *   from the RDONLY attribute is correct after all.
     */
    {
        /* 
         *   get the file's DACL and owner/group security info; first, ask
         *   how much space we need to allocate for the returned information 
         */
        DWORD len = 0;
        SECURITY_INFORMATION info = (SECURITY_INFORMATION)(
            OWNER_SECURITY_INFORMATION
            | GROUP_SECURITY_INFORMATION
            | DACL_SECURITY_INFORMATION);
        GetFileSecurity(fname, info, 0, 0, &len);
        if (len != 0)
        {
            /* allocate a buffer for the security info */
            SECURITY_DESCRIPTOR *sd = (SECURITY_DESCRIPTOR *)malloc(len);
            if (sd != 0)
            {
                /* now actually retrieve the security info into our buffer */
                if (GetFileSecurity(fname, info, sd, len, &len))
                {
                    HANDLE ttok;

                    /* impersonate myself for security purposes */
                    ImpersonateSelf(SecurityImpersonation);

                    /* 
                     *   get the security token for the current thread, which
                     *   is the context in which the caller will presumably
                     *   eventually attempt to access the file 
                     */
                    if (OpenThreadToken(
                        GetCurrentThread(), TOKEN_ALL_ACCESS, TRUE, &ttok))
                    {
                        GENERIC_MAPPING genmap = {
                            FILE_GENERIC_READ,
                            FILE_GENERIC_WRITE,
                            FILE_GENERIC_EXECUTE,
                            FILE_ALL_ACCESS
                        };
                        PRIVILEGE_SET privs;
                        DWORD privlen = sizeof(privs);
                        BOOL granted = FALSE;
                        DWORD desired;
                        DWORD allowed;

                        /* test read access */
                        desired = GENERIC_READ;
                        MapGenericMask(&desired, &genmap);
                        if (AccessCheck(sd, ttok, desired, &genmap,
                                        &privs, &privlen, &allowed, &granted))
                        {
                            /* clear the read bit if reading isn't allowed */
                            if (allowed == 0)
                                attrs &= ~OSFATTR_READ;
                        }

                        /* test write access */
                        desired = GENERIC_WRITE;
                        MapGenericMask(&desired, &genmap);
                        if (AccessCheck(sd, ttok, desired, &genmap,
                                        &privs, &privlen, &allowed, &granted))
                        {
                            /* clear the read bit if reading isn't allowed */
                            if (allowed == 0)
                                attrs &= ~OSFATTR_WRITE;
                        }

                        /* done with the thread token */
                        CloseHandle(ttok);
                    }

                    /* terminate our security self-impersonation */
                    RevertToSelf();
                }

                /* free the allocated security info buffer */
                free(sd);
            }
        }
    }
#endif /* T_WIN32 */

    /* return the attributes */
    return attrs;
}


/*
 *   Get file status information 
 */
int os_file_stat(const char *fname, int follow_links, os_file_stat_t *s)
{
    struct __stat64 info;

    // $$$ we should support Windows symlinks and junction points

    /* get the file information */
    if (_stat64(fname, &info))
        return FALSE;

    /* translate the status fields */
    s->sizelo = (uint32_t)(info.st_size & 0xFFFFFFFF);
    s->sizehi = (uint32_t)(info.st_size >> 32);
    s->cre_time = (os_time_t)info.st_ctime;
    s->mod_time = (os_time_t)info.st_mtime;
    s->acc_time = (os_time_t)info.st_atime;
    s->mode = info.st_mode;

    /* get the attributes */
    s->attrs = oss_get_file_attrs(fname);

    /* success */
    return TRUE;
}

/*
 *   Resolve a symbolic link 
 */
int os_resolve_symlink(const char *fname, char *target, size_t target_size)
{
    /* 
     *   We don't currently support symbolic links.  (We should change this
     *   to support Windows symlinks and junction points. 
     */
    //$$$
    return FALSE;
}


#endif /* MICROSOFT */


/* ------------------------------------------------------------------------ */
/*
 *   Create a directory 
 */
#ifdef MSDOS

#ifdef TURBO
#define _mkdir(dir)  mkdir(dir)
#endif
#ifdef DJGPP
#define _mkdir(dir)  mkdir(dir, S_IWUSR)
#endif

int os_mkdir(const char *dir, int create_parents)
{
    /* 
     *   if we're to create the intermediate parent folders, and the path
     *   contains multiple elements, recursively create the parent
     *   directories first 
     */
    if (create_parents
        && (strchr(dir, ':') != 0
            || strchr(dir, '/') != 0
            || strchr(dir, '\\') != 0))
    {
        char par[OSFNMAX];

        /* extract the parent path */
        os_get_path_name(par, sizeof(par), dir);

        /* if the parent doesn't already exist, create it recursively */
        if (par[0] != '\0' && osfacc(par) && !os_mkdir(par, TRUE))
            return FALSE;
    }

    /* create the directory */
    return _mkdir(dir) == 0;
}
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Remove a directory 
 */
#ifdef MSDOS

#ifdef TURBO
#define _rmdir(dir)  rmdir(dir)
#endif
#ifdef DJGPP
#define _rmdir(dir)  rmdir(dir)
#endif

int os_rmdir(const char *dir)
{
    return _rmdir(dir) == 0;
}
#endif


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

struct oss_find_ctx_t
{
    oss_c_ffcx ff;
    int found;
};

int os_open_dir(const char *dir, osdirhdl_t *hdl)
{
    char *pat;
    size_t patsiz;
    struct oss_find_ctx_t *ctx;
    HDIR hdir = HDIR_SYSTEM;
    count_t scnt = 1;

    /* create a copy of the string with "\*.*" appended */
    patsiz = strlen(dir) + 32;
    pat = (char *)malloc(patsiz);
    os_build_full_path(pat, patsiz, dir, "*.*");
    if (pat == 0)
        return FALSE;

    /* allocate a search block structure */
    ctx = (struct oss_find_ctx_t *)malloc(sizeof(struct oss_find_ctx_t));
    if (ctx == 0)
    {
        free(pat);
        return FALSE;
    }

    /* begin the search */
    ctx->found = (DosFindFirst(pat, &hdir, FILE_DIRECTORY | FILE_NORMAL,
                               &ctx->ff, sizeof(ctx->ff), &scnt) == 0);

    /* done with the pattern buffer */
    free(pat);

    /* return the context as the search handle, and indicate success */
    *hdl = ctx;
    return TRUE;
}

int os_read_dir(osdirhdl_t hdl, char *buf, size_t buflen)
{
    count_t scnt = 1;

    /* cast the handle to our private structure */
    struct oss_find_ctx_t *ctx = (oss_find_ctx_t *)hdl;

    /* if we're out of files, return failure */
    if (!ctx->found)
        return FALSE;

    /* return the current file */
    safe_strcpyl(buf, buflen, ctx->ff.achName, ctx->ff.cchName);

    /* move on to the next file */
    ctx->found = (DosFindNext(
        HDIR_SYSTEM, &ctx->ff, sizeof(ctx->ff), &scnt) == 0);

    /* success */
    return TRUE;
}

void os_close_dir(osdirhdl_t hdl)
{
    /* free the context structure */
    free(hdl);
}

#if 0 // the os_find_xxx family of functions has been DEPRECATED
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
#endif /* DEPRECATED */

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


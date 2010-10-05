/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* osansi2.c -- file name mutilations */

#include "os.h"

#include <ctype.h>
#include <assert.h>

/* ------------------------------------------------------------------------ */
/*
 *   Special file and directory locations
 */

/*
 *   Get the full filename (including directory path) to the executable
 *   file, given the argv[0] parameter passed into the main program.  This
 *   fills in the buffer with a null-terminated string that can be used in
 *   osfoprb(), for example, to open the executable file.
 *   
 *   Returns non-zero on success.  If it is not possible to determine the
 *   name of the executable file, returns zero.
 *   
 *   Some operating systems might not provide access to the executable file
 *   information, so non-trivial implementation of this routine is optional;
 *   if the necessary information is not available, simply implement this to
 *   return zero.  If the information is not available, callers should offer
 *   gracefully degraded functionality if possible.  
 */
int os_get_exe_filename(char *buf, size_t buflen, const char *argv0)
{
	return 0;
}

/*
 *   Get a special directory path.  Returns the selected path, in a format
 *   suitable for use with os_build_full_path().  The main program's argv[0]
 *   parameter is provided so that the system code can choose to make the
 *   special paths relative to the program install directory, but this is
 *   entirely up to the system implementation, so the argv[0] parameter can
 *   be ignored if it is not needed.
 *   
 *   The 'id' parameter selects which special path is requested; this is one
 *   of the constants defined below.  If the id is not understood, there is
 *   no way of signalling an error to the caller; this routine can fail with
 *   an assert() in such cases, because it indicates that the OS layer code
 *   is out of date with respect to the calling code.
 *   
 *   This routine can be implemented using one of the strategies below, or a
 *   combination of these.  These are merely suggestions, though, and
 *   systems are free to ignore these and implement this routine using
 *   whatever scheme is the best fit to local conventions.
 *   
 *   - Relative to argv[0].  Some systems use this approach because it keeps
 *   all of the TADS files together in a single install directory tree, and
 *   doesn't require any extra configuration information to find the install
 *   directory.  Since we base the path name on the executable that's
 *   actually running, we don't need any environment variables or parameter
 *   files or registry entries to know where to look for related files.
 *   
 *   - Environment variables or local equivalent.  On some systems, it is
 *   conventional to set some form of global system parameter (environment
 *   variables on Unix, for example) for this sort of install configuration
 *   data.  In these cases, this routine can look up the appropriate
 *   configuration variables in the system environment.
 *   
 *   - Hard-coded paths.  Some systems have universal conventions for the
 *   installation configuration of compiler-like tools, so the paths to our
 *   component files can be hard-coded based on these conventions.  Note
 *   that it is common on some systems to use hard-coded paths by default
 *   but allow these to be overridden using environment variables or the
 *   like - this is often a good option because it makes life easy for most
 *   users, who use the default install configuration and thus do not need
 *   to set any environment variables, while still allowing for special
 *   cases where users cannot use the default configuration for some reason.
 *   
 *   
 */
void os_get_special_path(char *buf, size_t buflen,
                         const char *argv0, int id)
{
	char *str = NULL;

	switch (id)
	{
		case OS_GSP_T3_RES:
			str = getenv("TADS3_RESDIR");
			break;
		case OS_GSP_T3_LIB:
			str = getenv("TADS3_LIBDIR");
			break;
		case OS_GSP_T3_INC:
			str = getenv("TADS3_INCLUDEDIR");
			break;
		default:
			str = NULL;
			break;
	}

	if (!str)
		str = "";
	strcpy(buf, str);
}

/* 
 *   Seek to the resource file embedded in the current executable file,
 *   given the main program's argv[0].
 *   
 *   On platforms where the executable file format allows additional
 *   information to be attached to an executable, this function can be used
 *   to find the extra information within the executable.
 *   
 *   The 'typ' argument gives a resource type to find.  This is an arbitrary
 *   string that the caller uses to identify what type of object to find.
 *   The "TGAM" type, for example, is used by convention to indicate a TADS
 *   compiled GAM file.  
 */
osfildef *os_exeseek(const char *argv0, const char *typ)
{
	return NULL;
}



/* ------------------------------------------------------------------------ */
/*
 *   Look for a file in the "standard locations": current directory, program
 *   directory, PATH-like environment variables, etc.  The actual standard
 *   locations are specific to each platform; the implementation is free to
 *   use whatever conventions are appropriate to the local system.  On
 *   systems that have something like Unix environment variables, it might be
 *   desirable to define a TADS-specific variable (TADSPATH, for example)
 *   that provides a list of directories to search for TADS-related files.
 *   
 *   On return, fill in 'buf' with the full filename of the located copy of
 *   the file (if a copy was indeed found), in a format suitable for use with
 *   the osfopxxx() functions; in other words, after this function returns,
 *   the caller should be able to pass the contents of 'buf' to an osfopxxx()
 *   function to open the located file.
 *   
 *   Returns true (non-zero) if a copy of the file was located, false (zero)
 *   if the file could not be found in any of the standard locations.  
 */
int os_locate(const char *fname, int flen, const char *arg0,
              char *buf, size_t bufsiz)
{
	if (!osfacc(fname))
	{
		memcpy(buf, fname, flen);
		buf[flen] = 0;
		return TRUE;
	}
	return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Create and open a temporary file.  The file must be opened to allow
 *   both reading and writing, and must be in "binary" mode rather than
 *   "text" mode, if the system makes such a distinction.  Returns null on
 *   failure.
 *   
 *   If 'fname' is non-null, then this routine should create and open a file
 *   with the given name.  When 'fname' is non-null, this routine does NOT
 *   need to store anything in 'buf'.  Note that the routine shouldn't try
 *   to put the file in a special directory or anything like that; just open
 *   the file with the name exactly as given.
 *   
 *   If 'fname' is null, this routine must choose a file name and fill in
 *   'buf' with the chosen name; if possible, the file should be in the
 *   conventional location for temporary files on this system, and should be
 *   unique (i.e., it shouldn't be the same as any existing file).  The
 *   filename stored in 'buf' is opaque to the caller, and cannot be used by
 *   the caller except to pass to osfdel_temp().  On some systems, it may
 *   not be possible to determine the actual filename of a temporary file;
 *   in such cases, the implementation may simply store an empty string in
 *   the buffer.  (The only way the filename would be unavailable is if the
 *   implementation uses a system API that creates a temporary file, and
 *   that API doesn't return the name of the created temporary file.  In
 *   such cases, we don't need the name; the only reason we need the name is
 *   so we can pass it to osfdel_temp() later, but since the system is going
 *   to delete the file automatically, osfdel_temp() doesn't need to do
 *   anything and thus doesn't need the name.)
 *   
 *   After the caller is done with the file, it should close the file (using
 *   osfcls() as normal), then the caller MUST call osfdel_temp() to delete
 *   the temporary file.
 *   
 *   This interface is intended to take advantage of systems that have
 *   automatic support for temporary files, while allowing implementation on
 *   systems that don't have any special temp file support.  On systems that
 *   do have automatic delete-on-close support, this routine should use that
 *   system-level support, because it helps ensure that temp files will be
 *   deleted even if the caller fails to call osfdel_temp() due to a
 *   programming error or due to a process or system crash.  On systems that
 *   don't have any automatic delete-on-close support, this routine can
 *   simply use the same underlying system API that osfoprwbt() normally
 *   uses (although this routine must also generate a name for the temp file
 *   when the caller doesn't supply one).
 *   
 *   This routine can be implemented using ANSI library functions as
 *   follows: if 'fname' is non-null, return fopen(fname,"w+b"); otherwise,
 *   set buf[0] to '\0' and return tmpfile().  
 */
osfildef *os_create_tempfile(const char *fname, char *buf)
{
	assert(FALSE && "boom");
	return NULL;
}

/*
 *   Delete a temporary file - this is used to delete a file created with
 *   os_create_tempfile().  For most platforms, this can simply be defined
 *   the same way as osfdel().  For platforms where the operating system or
 *   file manager will automatically delete a file opened as a temporary
 *   file, this routine should do nothing at all, since the system will take
 *   care of deleting the temp file.
 *   
 *   Callers are REQUIRED to call this routine after closing a file opened
 *   with os_create_tempfile().  When os_create_tempfile() is called with a
 *   non-null 'fname' argument, the same value should be passed as 'fname' to
 *   this function.  When os_create_tempfile() is called with a null 'fname'
 *   argument, then the buffer passed in the 'buf' argument to
 *   os_create_tempfile() must be passed as the 'fname' argument here.  In
 *   other words, if the caller explicitly names the temporary file to be
 *   opened in os_create_tempfile(), then that same filename must be passed
 *   here to delete the named file; if the caller lets os_create_tempfile()
 *   generate a filename, then the generated filename must be passed to this
 *   routine.
 *   
 *   If os_create_tempfile() is implemented using ANSI library functions as
 *   described above, then this routine can also be implemented with ANSI
 *   library calls as follows: if 'fname' is non-null and fname[0] != '\0',
 *   then call remove(fname); otherwise do nothing.  
 */
int osfdel_temp(const char *fname)
{
	return TRUE;
}


/*
 *   Get the temporary file path.  This should fill in the buffer with a
 *   path prefix (suitable for strcat'ing a filename onto) for a good
 *   directory for a temporary file, such as the swap file.  
 */
void os_get_tmp_path(char *buf)
{
	strcpy(buf, "");
}


/* ------------------------------------------------------------------------ */
/*
 *   Switch to a new working directory.  
 */
void os_set_pwd(const char *dir);

/*
 *   Switch the working directory to the directory containing the given
 *   file.  Generally, this routine should only need to parse the filename
 *   enough to determine the part that's the directory path, then use
 *   os_set_pwd() to switch to that directory.  
 */
void os_set_pwd_file(const char *filename);


/* ------------------------------------------------------------------------ */
/*
 *   Filename manipulation routines
 */

/*
 *   Ports with MS-DOS-like file systems (Atari ST, OS/2, Macintosh, and,
 *   of course, MS-DOS itself) can use the os_defext and os_remext
 *   routines below by defining USE_DOSEXT.  Unix and VMS filenames will
 *   also be parsed correctly by these implementations, but untranslated
 *   VMS logical names may not be.  
 */

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

#ifdef _WIN32
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

#ifdef _WIN32
    /* on DOS, a file is absolute if it starts with a drive letter */
    if (isalpha(fname[0]) && fname[1] == ':')
        return TRUE;
#endif /* MSDOS */

    /* the path is relative */
    return FALSE;
}


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
                 && isxdigit((unsigned char)*(src+1))
                 && isxdigit((unsigned char)*(src+2)))
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


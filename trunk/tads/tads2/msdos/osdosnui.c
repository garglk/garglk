/* 
 *   Copyright (c) 1987, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdosnui.c - DOS/Win-specific functions with no user-interface component
Function
  Provides DOS/Windows implementations of a few miscellaneous osifc
  functions that have no user-interface component.  These are useful for
  linking versions of programs with alternative UI's outside of the set
  implemented in osdos.c and the like.
Notes

Modified
  04/05/02 MJRoberts  - Creation (from osdos.c)
*/

#ifdef __WIN32__
#include <windows.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "std.h"
#include "os.h"

#ifdef TURBO
#include <share.h>
#define _SH_DENYRW SH_DENYRW
#endif

#ifdef DJGPP
#include <fcntl.h>
#include <dpmi.h>
#endif

extern void canonicalize_path(char *);

/* ------------------------------------------------------------------------ */
/*
 *   For djgpp, we must define the _fsopen routine (fopen with share mode),
 *   since this is MS-specific and doesn't appear in the djgpp library.  
 */
#ifdef DJGPP
static FILE *_fsopen(const char *fname, const char *mode, int sh_flags)
{
    int old_flags;
    FILE *fp;

    /* set the new share flags, remembering the previous flags */
    old_flags = __djgpp_share_flags;
    __djgpp_share_flags = sh_flags;

    /* open the file */
    fp = fopen(fname, mode);

    /* restore the old share flags */
    __djgpp_share_flags = old_flags;

    /* return the result of the open */
    return fp;
}
#endif /* DJGPP */



/* ------------------------------------------------------------------------ */
/*
 *   File I/O 
 */

#ifdef __WIN32__

/*
 *   Open a file.  We'll open the low-level handle with the 'omode' mode,
 *   then open a FILE stream on the handle with the 'mode' string.  
 */
osfildef *os_fsopen(const char *fname, const char *mode, int omode, int sh)
{
    int fd;
    
    /* add the non-inheritable flag to the open mode */
    omode |= _O_NOINHERIT;

    /* try opening the file handle */
    fd = _sopen(fname, omode, sh, _S_IREAD | _S_IWRITE);

    /* if we didn't get a valid file, return null */
    if (fd == -1)
        return 0;

    /* return a FILE stream on the handle */
    return _fdopen(fd, mode);
}

#else

/*
 *   Open text file for reading and writing, keeping existing data, creating
 *   a new file if the file doesn't already exist.  
 */
osfildef *osfoprwt(const char *fname, os_filetype_t typ)
{
    osfildef *fp;

    VARUSED(typ);

    /* 
     *   try opening in "r+" mode - this will open an existing file in
     *   read/write mode, but will fail if the file doesn't already exist 
     */
    fp = _fsopen(fname, "r+", _SH_DENYRW);

    /* if that succeeded, return the file */
    if (fp != 0)
        return fp;

    /* 
     *   the file doesn't exist - create a new file in "w+" mode, which
     *   creates a new file and opens it in read/write mode 
     */
    return _fsopen(fname, "w+", _SH_DENYRW);
}

/*
 *   Open binary file for reading and writing, keeping existing data,
 *   creating a new file if the file doesn't already exist.  
 */
osfildef *osfoprwb(const char *fname, os_filetype_t typ)
{
    osfildef *fp;

    VARUSED(typ);

    /* 
     *   try opening in "r+b" mode - this will open an existing file in
     *   read/write mode, but will fail if the file doesn't already exist 
     */
    fp = _fsopen(fname, "r+b", _SH_DENYRW);

    /* if that succeeded, return the file */
    if (fp != 0)
        return fp;

    /* 
     *   the file doesn't exist - create a new file in "w+b" mode, which
     *   creates a new file and opens it in read/write mode 
     */
    return _fsopen(fname, "w+b", _SH_DENYRW);
}

#endif /* __WIN32__ */


/* ------------------------------------------------------------------------ */
/*
 *   Safe strcpy 
 */
static void safe_strcpy(char *dst, size_t dstlen, const char *src)
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
    copylen = strlen(src);
    if (copylen > dstlen - 1)
        copylen = dstlen - 1;

    /* copy the string (or as much as we can) */
    memcpy(dst, src, copylen);

    /* null-terminate it */
    dst[copylen] = '\0';
}

#ifdef __WIN32__
/* ------------------------------------------------------------------------ */
/*
 *   Windows-specific implementation 
 */

/* Windows crypto context */
static HCRYPTPROV oswin_hcp = 0;

/* internal - get a windows crypto context if we don't have one already */
static void init_crypto_ctx()
{
    /* set up a crypto context */
    if (oswin_hcp == 0)
        CryptAcquireContext(&oswin_hcp, 0, 0, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT);
}

/*
 *   initialize 
 */
int osnoui_init(int *argc, char *argv[], const char *prompt,
            char *buf, int bufsiz)
{
    /* initialize the windows crypto context */
    init_crypto_ctx();

    /* success */
    return 0;
}

/*
 *   uninitialize 
 */
void osnoui_uninit()
{
    /* done with our crypto context */
    if (oswin_hcp != 0)
    {
        CryptReleaseContext(oswin_hcp, 0);
        oswin_hcp = 0;
    }

}

/* ------------------------------------------------------------------------ */
/*
 *   Get the system clock in milliseconds.  This is extremely easy for Win32
 *   console apps, since there's an API that does exactly what we want.  
 */
long os_get_sys_clock_ms(void)
{
    return (long)GetTickCount();
}


/* ------------------------------------------------------------------------ */
/*
 *   Random numbers 
 */
void os_gen_rand_bytes(unsigned char *buf, size_t len)
{
    /* make sure we have a crypto context */
    init_crypto_ctx();

    /* generate bytes via the system crypto API */
    CryptGenRandom(oswin_hcp, len, buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the absolute path for the given filename 
 */
int os_get_abs_filename(char *buf, size_t buflen, const char *filename)
{
    DWORD len;

    /* get the full path name */
    len = GetFullPathName(filename, buflen, buf, 0);
    if (len == 0 || len > buflen)
    {
        /* 
         *   failed, or the buffer isn't big enough - copy the filename as-is
         *   and return failure 
         */
        safe_strcpy(buf, buflen, filename);
        return FALSE;
    }

    /* success */
    return TRUE;
}

#else /* __WIN32__ */
/* ------------------------------------------------------------------------ */
/*
 *   DOS (non-Windows) implementation 
 */

int osnoui_init(int *argc, char *argv[], const char *prompt,
                char *buf, int bufsiz)
{
    /* no module-specific initialization is needed */
}

void osnoui_uninit()
{
    /* no module-specific teardown is needed */
}


/*
 *   Get the absolute path for a filename.  This has slightly different
 *   implementations for Borland and DJGPP.  
 */
#ifdef DJGPP
int os_get_abs_filename(char *buf, size_t buflen, const char *filename)
{
    char tmp[FILENAME_MAX];
    char *p;

    /* "fix" the path - this expands the filename to an absolute path */
    _fixpath(filename, tmp);

    /* ...it also changes \ to /, so undo that to keep things in DOS format */
    for (p = tmp ; *p != '\0' ; ++p)
    {
        if (*p == '/')
            *p = '\\';
    }

    /* if the result fits the buffer, send it back, otherwise fail */
    if (strlen(tmp) < buflen)
    {
        strcpy(buf, tmp);
        return TRUE;
    }
    else
        return FALSE;
}
#endif /* DJGPP */
#ifdef TURBO
int os_get_abs_filename(char *buf, size_t buflen, const char *filename)
{
    /* expand the path to absolute notation */
    return (_fullpath(buf, filename, buflen) != 0);
}
#endif /* TURBO */

/*
 *   Calculate the current system time in milliseconds.  Returns a value
 *   relative to an arbitrary base time.  
 */
long os_get_sys_clock_ms(void)
{
    static unsigned long last_ticks;
    long curtime;

    /* get the current system clock reading in clock ticks */
#ifdef DJGPP
    __dpmi_regs regs;

    regs.h.ah = 0;
    __dpmi_int(0x1a, &regs);
    curtime = (regs.x.cx << 16) | regs.x.dx;
#else /* DJGPP */
    asm {
        push   bp
        mov    ah, 00h
        int    1ah
        pop    bp

        mov    word ptr [curtime], dx
        mov    word ptr [curtime+2], cx
    }
#endif /* DJGPP */

    /* 
     *   Check for clock rollover.  The clock resets to 0 ticks at midnight.
     *   If the new tick count is lower than the last tick count we read, add
     *   1573040 (the number of ticks in 24 hours) until it's greater.  This
     *   ensures that we have continuity between readings.  
     */
    while (curtime < last_ticks)
        curtime += 1573040;

    /* save the last tick count for checking rollover next time through */
    last_ticks = curtime;

    /* 
     *   Convert the tick count to milliseconds - there are 18.2065 ticks
     *   per second, hence 18.2065 ticks per 1000 milliseconds, hence
     *   1000/18.2065 milliseconds per tick.  This is approximately 55
     *   ticks per millisecond.  Return the result of the conversion.  
     */
    return curtime * 55;
}

#endif /* __WIN32__ */



/* ------------------------------------------------------------------------ */
/*
 *   Is the given file in the given directory?  
 */
int os_is_file_in_dir(const char *filename, const char *path,
                      int allow_subdirs)
{
    char filename_buf[OSFNMAX], path_buf[OSFNMAX];
    size_t flen, plen;

    /* absolute-ize the filename, if necessary */
    if (!os_is_file_absolute(filename))
    {
        os_get_abs_filename(filename_buf, sizeof(filename_buf), filename);
        filename = filename_buf;
    }

    /* absolute-ize the path, if necessary */
    if (!os_is_file_absolute(path))
    {
        os_get_abs_filename(path_buf, sizeof(path_buf), path);
        path = path_buf;
    }

    /* 
     *   canonicalize the paths, to remove .. and . elements - this will make
     *   it possible to directly compare the path strings 
     */
    safe_strcpy(filename_buf, sizeof(filename_buf), filename);
    canonicalize_path(filename_buf);
    filename = filename_buf;

    safe_strcpy(path_buf, sizeof(path_buf), path);
    canonicalize_path(path_buf);
    path = path_buf;

    /* get the length of the filename and the length of the path */
    flen = strlen(filename);
    plen = strlen(path);

    /* if the path ends in a separator character, ignore that */
    if (plen > 0 && (path[plen-1] == '\\' || path[plen-1] == '/'))
        --plen;

    /* 
     *   Check that the filename has 'path' as its path prefix.  First, check
     *   that the leading substring of the filename matches 'path', ignoring
     *   case.  Note that we need the filename to be at least two characters
     *   longer than the path: it must have a path separator after the path
     *   name, and at least one character for a filename past that.  
     */
    if (flen < plen + 2 || memicmp(filename, path, plen) != 0)
        return FALSE;

    /* 
     *   Okay, 'path' is the leading substring of 'filename'; next make sure
     *   that this prefix actually ends at a path separator character in the
     *   filename.  (This is necessary so that we don't confuse "c:\a\b.txt"
     *   as matching "c:\abc\d.txt" - if we only matched the "c:\a" prefix,
     *   we'd miss the fact that the file is actually in directory "c:\abc",
     *   not "c:\a".) 
     */
    if (filename[plen] != '\\' && filename[plen] != '/')
        return FALSE;

    /*
     *   We're good on the path prefix - we definitely have a file that's
     *   within the 'path' directory or one of its subdirectories.  If we're
     *   allowed to match on subdirectories, we already have our answer
     *   (true).  If we're not allowed to match subdirectories, we still have
     *   one more check, which is that the rest of the filename is free of
     *   path separator charactres.  If it is, we have a file that's directly
     *   in the 'path' directory; otherwise it's in a subdirectory of 'path'
     *   and thus isn't a match.  
     */
    if (allow_subdirs)
    {
        /* 
         *   filename is in the 'path' directory or one of its
         *   subdirectories, and we're allowed to match on subdirectories, so
         *   we have a match 
         */
        return TRUE;
    }
    else
    {
        const char *p;

        /* 
         *   We're not allowed to match subdirectories, so scan the rest of
         *   the filename for path separators.  If we find any, the file is
         *   in a subdirectory of 'path' rather than directly in 'path'
         *   itself, so it's not a match.  If we don't find any separators,
         *   we have a file directly in 'path', so it's a match. 
         */
        for (p = filename + plen + 1 ;
             *p != '\0' && *p != '/' && *p != '\\' ; ++p) ;

        /* 
         *   if we reached the end of the string without finding a path
         *   separator character, it's a match 
         */
        return (*p == '\0');
    }
}


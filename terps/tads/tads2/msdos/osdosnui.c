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

#ifdef T_WIN32
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
#include <sys/stat.h>
#include <time.h>
#define _SH_DENYRW SH_DENYRW
#define _SH_DENYNO SH_DENYNO
#endif

#ifdef DJGPP
#include <fcntl.h>
#include <dpmi.h>
#include <sys/stat.h>
#include <time.h>
#include "osstzprs.h"
#endif

extern void canonicalize_path(char *);
extern void safe_strcpy(char *dst, size_t dstlen, const char *src);
extern void safe_strcpyl(char *dst, size_t dstl, const char *src, size_t srcl);
extern unsigned long oss_get_file_attrs(const char *fname);

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

#ifdef T_WIN32

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

#endif /* T_WIN32 */


/* ------------------------------------------------------------------------ */
/*
 *   Duplicate a file handle 
 */
#if defined(TURBO) || defined(DJGPP)
# define ossfdup(fp, mode) fdopen(dup(fileno(fp)), mode)
#endif
#if defined(MICROSOFT)
# define ossfdup(fp, mode) _fdopen(_dup(_fileno(fp)), mode)
#endif

osfildef *osfdup(osfildef *orig, const char *mode)
{
    char realmode[5];
    char *p = realmode;
    const char *m;

    /* verify that there aren't any unrecognized mode flags */
    for (m = mode ; *m != '\0' ; ++m)
    {
        if (strchr("rw+bst", *m) == 0)
            return 0;
    }

    /* figure the read/write mode - translate r+ and w+ to r+ */
    if ((mode[0] == 'r' || mode[0] == 'w') && mode[1] == '+')
        *p++ = 'r', *p++ = '+';
    else if (mode[0] == 'r')
        *p++ = 'r';
    else if (mode[0] == 'w')
        *p++ = 'w';
    else
        return 0;

    /* add the format mode - use binary for 'b' and 's', 't' otherwise */
    if (strchr(mode, 'b') != 0 || strchr(mode, 's') != 0)
        *p++ = 'b';
    else
        *p++ = 't';

    /* end the mode string */
    *p = '\0';

    /* duplicate the handle in the given mode */
    return ossfdup(orig, mode);
}


#ifdef T_WIN32
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

    /* if the name is empty, use "." */
    if (filename[0] == '\0')
        filename = ".";

    /* check for a relative path on a specific drive (e.g., "c:test") */
    if (isalpha(filename[0]) && filename[1] == ':'
        && filename[2] != '\\' && filename[2] != '/')
    {
        /* 
         *   GetFullPathName doesn't handle relative paths with drive
         *   letters, because Windows doesn't use the DOS scheme of
         *   maintaining a separate working directory per drive.  First,
         *   check to see if we're talking about the working drive; if so,
         *   just remove the drive letter and get the full path as normal,
         *   since that's the drive we'd apply anyway.
         */
        len = GetFullPathName(".", buflen, buf, 0);
        if (len != 0 && len <= buflen
            && toupper(buf[0]) == toupper(filename[0]) && buf[1] == ':')
        {
            /* 
             *   it's on our working drive anyway, so we can just take off
             *   the drive letter and we'll get the right result - e.g.,
             *   "c:test" -> "test" -> "c:\working_dir\test" 
             */
            filename += 2;
        }
        else
        {
            /*
             *   It's on a different drive.  Windows doesn't keep separate
             *   working directories per drive, so we'll simply pretend that
             *   the working directory on a drive that's not the current
             *   drive is the root folder on that drive.  E.g., "k:test" ->
             *   "k:\test".
             */

            /* make sure we have room to add the "\" after the ":" */
            if (buflen < strlen(filename) + 2)
                return FALSE;

            /* rebuild the path as "k:\path" */
            buf[0] = filename[0];
            buf[1] = filename[1];
            buf[2] = '\\';
            strcpy(buf+3, filename+2);

            /* handled */
            return TRUE;
        }
    }

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

#else /* T_WIN32 */
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

    /* if the filename is empty, use "." */
    if (filename[0] == '\0')
        filename = ".";

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
    /* if the filename is empty, use "." */
    if (filename[0] == '\0')
        filename = ".";

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

#endif /* T_WIN32 */



/* ------------------------------------------------------------------------ */
/*
 *   Compare path strings.  For DOS/Windows, we treat upper and lower case as
 *   interchangeable, and we treat '/' and '\' as interchangeable.
 */
static int patheq(const char *a, const char *b, size_t len)
{
    /* compare character by character */
    for ( ; len != 0 ; ++a, ++b, --len)
    {
        /* convert both characters to lower case */
        char ca = isupper(*a) ? tolower(*a) : *a;
        char cb = isupper(*b) ? tolower(*b) : *b;

        /* convert forward slashes to backslashes */
        if (ca == '/') ca = '\\';
        if (cb == '/') cb = '\\';

        /* if they differ after all of that, we don't have a match */
        if (ca != cb)
            return FALSE;
    }

    /* no mismatches */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Is the given path a root path?  This returns true for a drive letter
 *   root (C: or C:\) or a UNC root (\\machine\root). 
 */
static int is_root_path(const char *path)
{
    /* note the length */
    size_t len = strlen(path);

    /* check for a drive letter root */
    if (isalpha(path[0])
        && path[1] == ':'
        && (len == 2
            || (len == 3 && (path[2] == '/' || path[2] == '\\'))))
        return TRUE;

    /* check for a UNC root */
    if ((path[0] == '\\' || path[0] == '/')
        && (path[1] == '\\' || path[1] == '/'))
    {
        /* it starts as a UNC path; count path elements */
        int eles;
        const char *p;
        for (p = path + 2, eles = 1 ; *p != '\0' ; ++p)
        {
            /* if it's a non-trailing slash, count a new element */
            if ((*p == '\\' || *p == '/')
                && p[1] != '\0' && p[1] != '\\' && p[1] != '/')
                ++eles;
        }

        /* 
         *   it's a root path if it has one or two elements (\\MACHINE or
         *   \\MACHINE\ROOT) 
         */
        return eles <= 2;
    }

    /* not a root path */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Is the given file in the given directory?  
 */
int os_is_file_in_dir(const char *filename, const char *path,
                      int allow_subdirs, int match_self)
{
    char filename_buf[OSFNMAX], path_buf[OSFNMAX];
    size_t flen, plen;
    int endsep = FALSE;

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

    /* 
     *   If the path ends in a separator character, ignore that.  Exception:
     *   if the path is a root path, leave the separator intact. 
     */
    if (plen > 0 && (path[plen-1] == '\\' || path[plen-1] == '/')
        && !is_root_path(path))
        --plen;

    /* if the path still ends in a path separator, so note */
    endsep = (plen > 0 && (path[plen-1] == '\\' || path[plen-1] == '/'));

    /* 
     *   if the names match, return true if and only if the caller wants
     *   us to match the directory to itself
     */
    if (flen == plen && memicmp(filename, path, flen) == 0)
        return match_self;

    /* 
     *   Check that the filename has 'path' as its path prefix.  First, check
     *   that the leading substring of the filename matches 'path', ignoring
     *   case.  Note that we need the filename to be at least two characters
     *   longer than the path: it must have a path separator after the path
     *   name, and at least one character for a filename past that.
     *   Exception: if the path already ends in a path separator, we don't
     *   need to add another one, so the filename just needs to be one
     *   character longer.
     */
    if (flen < plen + (endsep ? 1 : 2) || !patheq(filename, path, plen))
        return FALSE;

    /* 
     *   Okay, 'path' is the leading substring of 'filename'; next make sure
     *   that this prefix actually ends at a path separator character in the
     *   filename.  (This is necessary so that we don't confuse "c:\a\b.txt"
     *   as matching "c:\abc\d.txt" - if we only matched the "c:\a" prefix,
     *   we'd miss the fact that the file is actually in directory "c:\abc",
     *   not "c:\a".)  If the path itself ends in a path separator, we've
     *   made this check already simply by the substring match.
     */
    if (!endsep && (filename[plen] != '\\' && filename[plen] != '/'))
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
        for (p = filename + plen + (endsep ? 1 : 0) ;
             *p != '\0' && *p != '/' && *p != '\\' ; ++p) ;

        /* 
         *   if we reached the end of the string without finding a path
         *   separator character, it's a match 
         */
        return (*p == '\0');
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Match a string to a DOS device name.  If the reference name contains a
 *   trailing '#', we'll match any digit from 1 to 9 to the '#'.
 */
static int devname_eq(const char *str, const char *ref)
{
    /* compare each character */
    for ( ; *str != '\0' && *ref != '\0' ; ++str, ++ref)
    {
        /* get both characters in lower case */
        char strch = isupper(*str) ? tolower(*str) : *str;
        char refch = isupper(*ref) ? tolower(*ref) : *ref;

        /* if they match exactly, proceed to the next character */
        if (refch == strch)
            continue;

        /* 
         *   if the reference character is a trailing '#', match to any digit
         *   1-9 
         */
        if (refch == '#' && ref[1] == '\0'
            && (strch >= '1' && strch <= '9'))
            continue;

        /* no match */
        return FALSE;
    }

    /* if they didn't run out at the same time, we don't have a match */
    return *str == *ref;
}


#ifdef DJGPP
#define _stat stat
#define _S_IFCHR S_IFCHR
#endif
#ifdef TURBO
#define _stat stat
#define _S_IFCHR S_IFCHR
#endif

/*
 *   File mode 
 */
int osfmode(const char *fname, int follow_links,
            unsigned long *mode, unsigned long *attrs)
{
    struct _stat s;
    const char *root, *p;
    static const char *devices[] = {
        "con", "prn", "aux", "nul", "com#", "lpt#"
    };
    int i;
    char rbuf[32];
    unsigned long imode, iattrs;

    /* 
     *   if the caller doesn't want the 'mode' or 'attrs' values, set the
     *   pointers to point to internal buffers so that we don't have to worry
     *   about null OUT variables beyond this point 
     */
    if (mode == 0)
        mode = &imode;
    if (attrs == 0)
        attrs = &iattrs;

    /* clear the mode bits in case we don't find a match */
    *mode = 0;

    /* 
     *   Check for the reserved DOS device filenames.  Windows device name
     *   handling is a bit bizarre and, frankly, stupid: if the root filename
     *   (excluding the extension) is one of the special DOS device names,
     *   the entire rest of the name (drive, path, extension) is ignored, and
     *   the reserved name takes precedence.  So, pull out the root name
     *   after the path, then strip off any extension, so that we have the
     *   part of the name that Windows treats as a device name.  Run through
     *   the fixed list of reserved names.  If we find a match, the name is
     *   definitely a device name; but then we have to check if the device
     *   actually exists on this system to determine if we should return
     *   "device" or "unknown".
     */
    root = os_get_root_name((char *)fname);
    for (p = root ; *p && *p != ':' && *p != '.' ; ++p) ;

    /* extract the root name into a buffer */
    safe_strcpyl(rbuf, sizeof(rbuf), root, p - root);

    /* look for a device name match */
    for (i = 0 ; i < sizeof(devices)/sizeof(devices[0]) ; ++i)
    {
        /* check the root name against the current device name */
        if (devname_eq(rbuf, devices[i]))
        {
            FILE *fp;
            
            /* 
             *   We got a match, so this file refers to a device.  Now we
             *   have to check to see if the device actually exists on this
             *   system.  That's surprisingly difficult; there doesn't seem
             *   to exist a Windows API that returns consistent results.  The
             *   only sure way seems to be to try opening the file.  Some
             *   devices are read-only and some are write-only, so we need to
             *   try both modes.
             *   
             *   Opening the file as a device doesn't seem ideal for a number
             *   of reasons, such as the potential for false negatives due to
             *   sharing violations.  But there doesn't seem to be any other
             *   reliable way to find out if the file is openable.
             */
            *attrs = 0;
            fp = _fsopen(rbuf, "r", _SH_DENYNO);
            if (fp != 0)
            {
                *attrs |= OSFATTR_READ;
                fclose(fp);
            }
            fp = _fsopen(rbuf, "w", _SH_DENYNO);
            if (fp != 0)
            {
                *attrs |= OSFATTR_WRITE;
                fclose(fp);
            }
            if (*attrs != 0)
            {
                /* it exists - return "char mode device" */
                *mode = OSFMODE_CHAR;
                return TRUE;
            }

            /* it's a device name, but it doesn't exist - return failure */
            return FALSE;
        }
    }
    
    /* get the file status information; return false on failure */
    if (_stat(fname, &s) < 0)
        return FALSE;

    /* got it - return the mode information */
    *mode = (unsigned long)s.st_mode;

    /* get the file attributes */
    *attrs = oss_get_file_attrs(fname);

    /* indicate success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
#if (!defined(WINDOWS) && !defined(_Windows)) || defined(__DPMI16__)
# if defined(TURBO) || defined(DJGPP)

/* get bitmap of valid drives */
unsigned long ossgetdisks(void)
{
    unsigned long mask;
    unsigned long ret;
    int drive;

    /* iterate over drives A: to Z: and test each one for validity */
    for (drive = 'A', mask = 1, ret = 0 ; drive <= 'Z' ; ++drive, mask <<= 1)
    {
        char dname[3];

        /* build a filename string for the drive, like "X:" */
        dname[0] = drive;
        dname[1] = ':';
        dname[2] = '\0';
        {
#  ifdef DJGPP
            int max_file_len;
            int max_path_len;
            char fstype[64];

            /* 
             *   check the file system type on the drive to see if the drive
             *   is valid 
             */
            if ((_get_volume_info(dname, &max_file_len, &max_path_len,
                                  fstype) & _FILESYS_UNKNOWN) == 0)
            {
                /* 
                 *   it's not unknown, so it must be valid - include it in
                 *   the result vector 
                 */
                ret |= mask;
            }

#  else /* DJGPP */
            char fcb[44];

            /*          
             *   Ask DOS to parse the drive letter filename (via int 0x21,
             *   function 0x290E).  DOS will give us an error indication if
             *   the drive is invalid.  
             */
            asm {
                push ds
                    push es
                    push bp
                    push si
                    push di

                    mov  si, ss
                    mov  ds, si
                    push ds
                    pop  es
                    lea  si, dname
                    lea  di, fcb
                    mov  ax, 0x290E  // parse filename
                    int  0x21
                    cmp  al, 0xFF
                    je   bad_drive
            }

            /* if we got this far, the drive is good */
            ret |= mask;

bad_drive:
            asm {
                pop  di
                    pop  si
                    pop  bp
                    pop  es
                    pop  ds
            }
#  endif
        }
    }

    return ret;
}

# endif /* TURBO || DJGPP */

# ifdef MSOS2
/* get bitmap of valid drives */
unsigned long ossgetdisks(void)
{
    /* return zero to indicate this isn't implemented */
    return 0;
}

# else /* MSOS2 */
#  ifdef MICROSOFT

#   ifdef T_WIN32

#include <io.h>
#include <direct.h>
unsigned long ossgetdisks(void)
{
    return _getdrives();
}

#   else /* T_WIN32 */

/* get disk:  0 = A:, 1 = B:, 2 = C:, etc */
unsigned long ossgetdisks(void)
{
    return _dos_getdrives();
}

#   endif /* T_WIN32 */
#  endif /* MICROSOFT */
# endif /* MSOS2 */
#endif

/*
 *   Get the root directory list.  On DOS/Windows, this returns a list of
 *   drive root paths - C:\, D:\, etc. 
 */
#ifdef T_WIN32
size_t os_get_root_dirs(char *buf, size_t buflen)
{
    return GetLogicalDriveStrings(buflen, buf);
}
#else
size_t os_get_root_dirs(char *buf, size_t buflen)
{
    extern unsigned long ossgetdisks(void);
    unsigned long d = ossgetdisks();
    int i, cnt;
    unsigned long b;
    size_t l;
    char *p;

    /* count the drive bits set */
    for (i = 0, cnt = 0, b = 1 ; i < 32 ; ++i, b <<= 1)
    {
        if ((d & b) != 0)
            ++cnt;
    }

    /* 
     *   figure the length needed - C:\ -> 4 characters (with null) for each
     *   drive, plus an extra null at the end of the list 
     */
    l = 4*cnt + 1;

    /* if the buffer isn't big enough, just return the length */
    if (l > buflen)
        return l;

    /* fill in the buffer */
    for (p = buf, i = 0, b = 1 ; i < 32 ; ++i, b <<= 1)
    {
        if ((d & b) != 0)
        {
            *p++ = 'A' + i;
            *p++ = ':';
            *p++ = '\\';
            *p++ = '\0';
        }
    }

    /* add the trailing null */
    *p++ = '\0';

    /* return the length */
    return p - buf;
}

#endif

#ifdef T_WIN32
/*
 *   get the system time in nanoseconds 
 */
void os_time_ns(os_time_t *seconds, long *nanoseconds)
{
    FILETIME ft;
    ULARGE_INTEGER fti;

    /* get the system time in FILETIME format */
    GetSystemTimeAsFileTime(&ft);

    /* 
     *   FILETIME is essentially a 64-bit value giving 100-nanosecond
     *   intervals since the FILETIME Epoch, which is 1/1/1601 00:00:00 UTC.
     *   We need to make two adjustments.  First, subtract the number of
     *   100ns intervals between 1/1/1970 and 1/1/1601 to convert to the
     *   interval since the Unix Eopch of 1/1/1970.  (There are 134774 days
     *   between these dates.)  Second, mutliply by 100 to convert to
     *   nanoseconds.
     */
    fti.LowPart = ft.dwLowDateTime;
    fti.HighPart = ft.dwHighDateTime;
    fti.QuadPart -= 134774LL*24*60*60*10000000;
    fti.QuadPart *= 100;

    /* 
     *   Okay, we have nanoseconds since 1/1/1970 as a 64-bit value.  Divide
     *   by 1,000,000,000 to get seconds, and get the whole part and the
     *   remainder as seconds and nanoseconds, respectively.
     */
    *seconds = (os_time_t)(fti.QuadPart / 1000000000);
    *nanoseconds = (long)(fti.QuadPart % 1000000000);
}
#endif /* T_WIN32 */


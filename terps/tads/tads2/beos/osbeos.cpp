/* 
 *   osbeos.c -- copyright (c) 2000 by Christopher Tate, all rights reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */

#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <sys/stat.h>
#include <app/Application.h>
#include <app/Roster.h>
#include <kernel/OS.h>
#include <storage/File.h>
#include <storage/Path.h>
#include <storage/Resources.h>

#include "appctx.h"
#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* cbreak mode is useful; code borrowed from jzip's unix port */
void set_cbreak_mode (int mode);
void set_cbreak_mode (int mode)
{
    int status;
    struct termios new_termios;
    static struct termios old_termios;

    if (mode)
        status = tcgetattr (fileno (stdin), &old_termios);
    else
        status = tcsetattr (fileno (stdin), TCSANOW, &old_termios);
    if (status) {
        perror ("ioctl");
        exit (1);
    }

    if (mode) {
        status = tcgetattr (fileno (stdin), &new_termios);
        if (status) {
            perror ("ioctl");
            exit (1);
        }

        new_termios.c_lflag &= ~(ICANON | ECHO);

        /* the next two lines of code added by Mark Phillips.  The patch */
        /* was for sun and __hpux, active only if those were #defined,   */
        /* but most POSIX boxen (SunOS, HP-UX, Dec OSF, Irix for sure)   */
        /* can use this... It makes character input work.  VMIN and      */
        /* VTIME are reused on some systems, so when the mode is switched*/
        /* to RAW all character access is, by default, buffered wrong.   */
        /* For the curious: VMIN='\004' and VTIME='\005' octal on        */
        /* these systems.  VMIN is usually EOF and VTIME is EOL. (JDH)   */
        new_termios.c_cc[VMIN]=1;
        new_termios.c_cc[VTIME]=2;

        status = tcsetattr (fileno (stdin), TCSANOW, &new_termios);
        if (status) {
            perror ("ioctl");
            exit (1);
        }
    }

}/* set_cbreak_mode */

/* ------------------------------------------------------------------------ */
/*
 *   A note on character sets:
 *   
 *   Except where noted, all character strings passed to and from the
 *   osxxx functions defined herein use the local operating system
 *   representation.  On a Windows machine localized to Eastern Europe,
 *   for example, the character strings passed to and from the osxxx
 *   functions would use single-byte characters in the Windows code page
 *   1250 representation.
 *   
 *   Callers that use multiple character sets must implement mappings to
 *   and from the local character set when calling the osxxx functions.
 *   The osxxx implementations are thus free to ignore any issues related
 *   to character set conversion or mapping.
 *   
 *   The osxxx implementations are specifically not permitted to use
 *   double-byte Unicode as the native character set, nor any other
 *   character set where a null byte could appear as part of a non-null
 *   character.  In particular, callers may assume that null-terminated
 *   strings passed to and from the osxxx functions contain no embedded
 *   null bytes.  Multi-byte character sets (i.e., character sets with
 *   mixed single-byte and double-byte characters) may be used as long as
 *   a null byte is never part of any multi-byte character, since this
 *   would guarantee that a null byte could always be taken as a null
 *   character without knowledge of the encoding or context.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   "Far" Pointers.  Most platforms can ignore this.  For platforms with
 *   mixed-mode addressing models, where pointers of different sizes can
 *   be used within a single program and hence some pointers require
 *   qualification to indicate that they use a non-default addressing
 *   model, the keyword OSFAR should be defined to the appropriate
 *   compiler-specific extension keyword.
 *   
 *   If you don't know what I'm talking about here, you should just ignore
 *   it, because your platform probably doesn't have anything this
 *   sinister.  As of this writing, this applies only to MS-DOS, and then
 *   only to 16-bit implementations that must interact with other 16-bit
 *   programs via dynamic linking or other mechanisms.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Hardware Configuration.  Define the following functions appropriately
 *   for your hardware.  For efficiency, these functions should be defined
 *   as macros if possible.
 *   
 *   Note that these hardware definitions are independent of the OS, at
 *   least to the extent that your OS can run on multiple types of
 *   hardware.  So, rather than combining these definitions into your
 *   osxxx.h header file, we recommend that you put these definitions in a
 *   separate h_yyy.h header file, which can be configured into os.h with
 *   an appropriate "_M_yyy" preprocessor symbol.  Refer to os.h for
 *   details of configuring the hardware include file.  
 */

/* 
 *   Round a size up to worst-case alignment boundary.  For example, on a
 *   platform where the largest type must be aligned on a 4-byte boundary,
 *   this should round the value up to the next higher mutliple of 4 and
 *   return the result.  
 */
/* size_t osrndsz(size_t siz); */

/* 
 *   Round a pointer up to worst-case alignment boundary. 
 */
/* unsigned char *osrndpt(unsigned char *ptr); */

/* 
 *   Read an unaligned portable unsigned 2-byte value, returning an int
 *   value.  The portable representation has the least significant byte
 *   first, so the value 0x1234 is represented as the byte 0x34, followed
 *   by the byte 0x12.
 *   
 *   The source value must be treated as unsigned, but the result is
 *   signed.  This is significant on 32- and 64-bit platforms, because it
 *   means that the source value should never be sign-extended to 32-bits.
 *   For example, if the source value is 0xffff, the result is 65535, not
 *   -1.  
 */
/* int osrp2(unsigned char *p); */

/* 
 *   Read an unaligned portable signed 2-byte value, returning int.  This
 *   differs from osrp2() in that this function treats the source value as
 *   signed, and returns a signed result; hence, on 32- and 64-bit
 *   platforms, the result must be sign-extended to the int size.  For
 *   example, if the source value is 0xffff, the result is -1.  
 */
/* int osrp2s(unsigned char *p); */

/* 
 *   Write int to unaligned portable 2-byte value.  The portable
 *   representation stores the low-order byte first in memory, so
 *   oswp2(0x1234) should result in storing a byte value 0x34 in the first
 *   byte, and 0x12 in the second byte. 
 */
/* void oswp2(unsigned char *p, int i); */

/* 
 *   Read an unaligned portable 4-byte value, returning long.  The
 *   underlying value should be considered signed, and the result is
 *   signed.  The portable representation stores the bytes starting with
 *   the least significant: the value 0x12345678 is stored with 0x78 in
 *   the first byte, 0x56 in the second byte, 0x34 in the third byte, and
 *   0x12 in the fourth byte.  
 */
/* long osrp4(unsigned char *p); */

/* 
 *   Write a long to an unaligned portable 4-byte value.  The portable
 *   representation stores the low-order byte first in memory, so
 *   0x12345678 is written to memory as 0x78, 0x56, 0x34, 0x12.  
 */
/* void oswp2(unsigned char *p, long l); */



/* ------------------------------------------------------------------------ */
/*
 *   Platform Identifiers.  You must define the following macros in your
 *   osxxx.h header file:
 *   
 *   OS_SYSTEM_NAME - a string giving the system identifier.  This string
 *   must contain only characters that are valid in a TADS identifier:
 *   letters, numbers, and underscores; and must start with a letter or
 *   underscore.  For example, on MS-DOS, this string is "MSDOS".
 *   
 *   OS_SYSTEM_LDESC - a string giving the system descriptive name.  This
 *   is used in messages displayed to the user.  For example, on MS-DOS,
 *   this string is "MS-DOS".  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Message Linking Configuration.  You should #define ERR_LINK_MESSAGES
 *   in your osxxx.h header file if you want error messages linked into
 *   the application.  Leave this symbol undefined if you want an external
 *   message file. 
 */


/* ------------------------------------------------------------------------ */
/*
 *   Program Exit Codes.  These values are used for the argument to exit()
 *   to conform to local conventions.  Define the following values in your
 *   OS-specific header:
 *   
 *   OSEXSUCC - successful completion.  Usually defined to 0.
 *.  OSEXFAIL - failure.  Usually defined to 1.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Basic memory management interface.  These functions are merely
 *   documented here, but no prototypes are defined, because most
 *   platforms #define macros for these functions and types, mapping them
 *   to malloc or other system interfaces.  
 */

/*
 *   Theoretical maximum osmalloc() size.  This may be less than the
 *   capacity of the argument to osmalloc() on some systems.  For example,
 *   on segmented architectures (such as 16-bit x86), memory is divided
 *   into segments, so a single memory allocation can allocate only a
 *   subset of the total addressable memory in the system.  This value
 *   thus specifies the maximum amount of memory that can be allocated in
 *   one chunk.
 *   
 *   Note that this is an architectural maximum for the hardware and
 *   operating system.  It doesn't have anything to do with the total
 *   amount of memory actually available at run-time.
 *   
 *   OSMALMAX - #define to a constant long value with theoretical maximum
 *   osmalloc() argument value.  
 */


/*   
 *   Allocate a block of memory of the given size in bytes.  The actual
 *   allocation may be larger, but may be no smaller.  The block returned
 *   should be worst-case aligned (i.e., suitably aligned for any type).
 *   Return null if the given amount of memory is not available.  
 */
/* void *osmalloc(size_t siz); */

/*
 *   Free memory previously allocated with osmalloc().  
 */
/* void osfree(void *block); */

/* 
 *   Reallocate memory previously allocated with osmalloc() or
 *   osrealloc(), changing the block's size to the given number of bytes.
 *   If necessary, a new block at a different address can be allocated, in
 *   which case the data from the original block is copied (the lesser of
 *   the old block size and the new size is copied) to the new block, and
 *   the original block is freed.  If the new size is less than the old
 *   size, this need not do anything at all, since the returned block can
 *   be larger than the new requested size.  If the block cannot be
 *   enlarged to the requested size, return null.  
 */
/* void *osrealloc(void *block, size_t siz); */


/* ------------------------------------------------------------------------ */
/*
 *   Basic file I/O interface.  These functions are merely documented
 *   here, but no prototypes are defined, because most platforms #define
 *   macros for these functions and types, mapping them to stdio or other
 *   system I/O interfaces.  
 */


/*
 *   Define the following values in your OS header to indicate local
 *   conventions:
 *   
 *   OSFNMAX - integer indicating maximum length of a filename
 *   
 *   OSPATHCHAR - character giving the normal path separator character
 *.  OSPATHALT - string giving other path separator characters
 *.  OSPATHSEP - directory separator for PATH-style environment variables
 *   
 *   For example, on DOS, OSPATHCHAR is '\\', OSPATHALT is "/:", and
 *   OSPATHSEP is ';'.  On Unix, OSPATHCHAR is '\', OSPATHALT is "", and
 *   OSPATHSEP is ':'.  
 */

/*
 *   Define the type osfildef as the appropriate file handle structure for
 *   your osfxxx functions.  This type is always used as a pointer, but
 *   the value is always obtained from an osfopxxx call, and is never
 *   synthesized by portable code, so you can use essentially any type
 *   here that you want.
 *   
 *   For platforms that use C stdio functions to implement the osfxxx
 *   functions, osfildef can simply be defined as FILE.
 */
/* typedef FILE osfildef; */


/*
 *   File types - see osifctyp.h
 */


/* 
 *   Open text file for reading.  Returns NULL on error.
 *   
 *   A text file differs from a binary file in that some systems perform
 *   translations to map between C conventions and local file system
 *   conventions; for example, on DOS, the stdio library maps the DOS
 *   CR-LF newline convention to the C-style '\n' newline format.  On many
 *   systems (Unix, for example), there is no distinction between text and
 *   binary files.  
 */
/* osfildef *osfoprt(const char *fname, os_filetype_t typ); */

/* 
 *   Open text file for writing; returns NULL on error 
 */
/* osfildef *osfopwt(const char *fname, os_filetype_t typ); */

/* 
 *   Open text file for reading and writing; returns NULL on error 
 */
osfildef *osfoprwt(const char *fname, os_filetype_t /*typ*/)
{
    osfildef *fp;

    /* try opening an existing file in read/write mode */
    fp = fopen(fname, "r+");

    /* if that failed, create a new file in read/write mode */
    if (fp == 0)
        fp = fopen(fname, "w+");

    /* return the file */
    return fp;
}

/* 
 *   Open text file for reading/writing.  If the file already exists,
 *   truncate the existing contents.  Create a new file if it doesn't
 *   already exist.  Return null on error.  
 */
/* osfildef *osfoprwtt(const char *fname, os_filetype_t typ); */

/* 
 *   Open binary file for writing; returns NULL on error.  
 */
/* osfildef *osfopwb(const char *fname, os_filetype_t typ); */

/* 
 *   Open source file for reading - use the appropriate text or binary
 *   mode.  
 */
/* osfildef *osfoprs(const char *fname, os_filetype_t typ); */

/* 
 *   Open binary file for reading; returns NULL on error.  
 */
/* osfildef *osfoprb(const char *fname, os_filetype_t typ); */

/* 
 *   Get a line of text from a text file.  Uses fgets semantics.  
 */
/* char *osfgets(char *buf, size_t len, osfildef *fp); */

/* 
 *   Write a line of text to a text file.  Uses fputs semantics.  
 */
/* void osfputs(const char *buf, osfildef *fp); */

/* 
 *   Open binary file for reading/writing.  If the file already exists,
 *   keep the existing contents.  Create a new file if it doesn't already
 *   exist.  Return null on error.  
 */
osfildef *osfoprwb(const char *fname, os_filetype_t /*typ*/)
{
    osfildef *fp;

    /* try opening an existing file in read/write mode */
    fp = fopen(fname, "r+b");

    /* if that failed, create a new file in read/write mode */
    if (fp == 0)
        fp = fopen(fname, "w+b");

    /* return the file */
    return fp;
}

/* 
 *   Open binary file for reading/writing.  If the file already exists,
 *   truncate the existing contents.  Create a new file if it doesn't
 *   already exist.  Return null on error.  
 */
/* osfildef *osfoprwtb(const char *fname, os_filetype_t typ); */

/* 
 *   Write bytes to file.  Return 0 on success, non-zero on error.  
 */
/* int osfwb(osfildef *fp, const uchar *buf, int bufl); */

/* 
 *   Read bytes from file.  Return 0 on success, non-zero on error.  
 */
/* int osfrb(osfildef *fp, uchar *buf, int bufl); */

/* 
 *   Read bytes from file and return the number of bytes read.  0
 *   indicates that no bytes could be read. 
 */
/* size_t osfrbc(osfildef *fp, uchar *buf, size_t bufl); */

/* 
 *   Get the current seek location in the file.  The first byte of the
 *   file has seek position 0.  
 */
/* long osfpos(osfildef *fp); */

/* 
 *   Seek to a location in the file.  The first byte of the file has seek
 *   position 0.  Returns zero on success, non-zero on error.
 *   
 *   The following constants must be defined in your OS-specific header;
 *   these values are used for the "mode" parameter to indicate where to
 *   seek in the file:
 *   
 *   OSFSK_SET - set position relative to the start of the file
 *.  OSFSK_CUR - set position relative to the current file position
 *.  OSFSK_END - set position relative to the end of the file 
 */
/* int osfseek(osfildef *fp, long pos, int mode); */

/* 
 *   Close a file.  
 */
/* void osfcls(osfildef *fp); */

/* 
 *   Delete a file.  Returns zero on success, non-zero on error. 
 */
/* int osfdel(const char *fname); */

/* 
 *   Access a file - determine if the file exists.  Returns zero if the
 *   file exists, non-zero if not.  (The semantics may seem a little
 *   weird, but this is consistent with the conventions used by most of
 *   the other osfxxx calls: zero indicates success, non-zero indicates an
 *   error.  If the file exists, "accessing" it was successful, so osfacc
 *   returns zero; if the file doesn't exist, accessing it gets an error,
 *   hence a non-zero return code.)  
 */
/* int osfacc(const char *fname) */

/* 
 *   Get a character from a file.  Provides the same semantics as fgetc().
 */
/* int osfgetc(osfildef *fp); */

/* ------------------------------------------------------------------------ */
/*
 *   File time stamps
 */

/*
 *   File timestamp type.  This type must be defined by the
 *   system-specific header, and should map to a local type that can be
 *   used to obtain information on a file's creation, modification, or
 *   access time.  Generic code is not allowed to do anything with data of
 *   this type except pass them to the routines defined here that take
 *   values of this type as parameters.
 *   
 *   On Unix, for example, this structure can be defined as having a
 *   single member of type time_t.  (We define this as an incomplete
 *   structure type here so that we can refer to it in the prototypes
 *   below.)  
 */
typedef struct os_file_time_t os_file_time_t;

/*
 *   Get the creation/modification/access time for a file.  Fills in the
 *   os_file_time_t value with the time for the file.  Returns zero on
 *   success, non-zero on failure (the file doesn't exist, the program has
 *   insufficient privileges to access the file, etc).
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
 *   Compare two file time values.  These values must be obtained with one
 *   of the os_get_file_xxx_time() functions.  Returns 1 if the first time
 *   is later than the second time; 0 if the two times are the same; and
 *   -1 if the first time is earlier than the second time.  
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


/* ------------------------------------------------------------------------ */
/*
 *   Find the first file matching a given pattern.  The returned context
 *   pointer is a pointer to whatever system-dependent context structure
 *   is needed to continue the search with the next file, and is opaque to
 *   the caller.  The caller must pass the context pointer to the
 *   next-file routine.  The caller can optionally cancel a search by
 *   calling the close-search routine with the context pointer.  If the
 *   return value is null, it indicates that no matching files were found.
 *   If a file was found, outbuf will be filled in with its name, and
 *   isdir will be set to true if the match is a directory, false if it's
 *   a file.  If pattern is null, all files in the given directory should
 *   be returned; otherwise, pattern is a string containing '*' and '?' as
 *   wildcard characters, but not containing any directory separators, and
 *   all files in the given directory matching the pattern should be
 *   returned.
 *   
 *   Important: because this routine may allocate memory for the returned
 *   context structure, the caller must either call os_find_next_file
 *   until that routine returns null, or call os_find_close() to cancel
 *   the search, to ensure that the os code has a chance to release the
 *   allocated memory.
 *   
 *   'outbuf' should be set on output to the name of the matching file,
 *   without any path information.
 *   
 *   'outpathbuf' should be set on output to full path of the matching
 *   file.  If possible, 'outpathbuf' should use the same relative or
 *   absolute notation that the search criteria used on input.  For
 *   example, if dir = "resfiles", and the file found is "MyPic.jpg",
 *   outpathbuf should be set to "resfiles/MyPic.jpg" (or appropriate
 *   syntax for the local platform).  Similarly, if dir =
 *   "/home/tads/resfiles", outpath buf should be
 *   "/home/tads/resfiles/MyPic.jpg".  The result should always conform to
 *   correct local conventions, which may require some amount of
 *   manipulation of the filename; for example, on the Mac, if dir =
 *   "resfiles", the result should be ":resfiles:MyPic.jpg" (note the
 *   added leading colon) to conform to Macintosh relative path notation.
 *   
 *   Note that 'outpathbuf' may be null, in which case the caller is not
 *   interested in the full path information.  
 */
/*   
 *   Note the following possible ways this function may be called:
 *   
 *   dir = "", pattern = filename - in this case, pattern is the name of a
 *   file or directory in the current directory.  filename *might* be a
 *   relative path specified by the user (on a command line, for example);
 *   for instance, on Unix, it could be something like "resfiles/jpegs".
 *   
 *   dir = path, pattern = filname - same as above, but this time the
 *   filename or directory pattern is relative to the given path, rather
 *   than to the current directory.  For example, we could have dir =
 *   "/games/mygame" and pattern = "resfiles/jpegs".
 *   
 *   dir = path, pattern = 0 (NULL) - this should search for all files in
 *   the given path.  The path might be absolute or it might be relative.
 *   
 *   dir = path, pattern = "*" - this should have the same result as when
 *   pattern = 0.
 *   
 *   dir = path, pattern = "*.ext" - this should search for all files in
 *   the given path whose names end with ".ext".
 *   
 *   dir = path, pattern = "abc*" - this should search for all files in
 *   the given path whose names start with "abc".
 *   
 *   All of these combinations are possible because callers, for
 *   portability, must generally not manipulate filenames directly;
 *   instead, callers obtain paths and search strings from external
 *   sources, such as from the user, and present them to this routine with
 *   minimal manipulation.  
 */
void *os_find_first_file(const char *dir, const char *pattern,
                         char *outbuf, size_t outbufsiz, int *isdir,
                         char *outpathbuf, size_t outpathbufsiz)
{
        debugger("os_find_first_file() not implemented");
        return NULL;
}

/*
 *   Implementation notes for porting os_find_first_file:
 *   
 *   The algorithm for this routine should go something like this:
 *   
 *   - If 'path' is null, create a variable real_path and initialize it
 *   with the current directory.  Otherwise, copy path to real_path.
 *   
 *   - If 'pattern' contains any directory separators ("/" on Unix, for
 *   example), change real_path so that it reflects the additional leading
 *   subdirectories in the path in 'pattern', and remove the leading path
 *   information from 'pattern'.  For example, on Unix, if real_path
 *   starts out as "./subdir", and pattern is "resfiles/jpegs", change
 *   real_path to "./subdir/resfiles", and change pattern to "jpegs".
 *   Take care to add and remove path separators as needed to keep the
 *   path strings well-formed.
 *   
 *   - Begin a search using appropriate OS API's for all files in
 *   real_path.
 *   
 *   - Check each file found.  Skip any files that don't match 'pattern',
 *   treating "*" as a wildcard that matches any string of zero or more
 *   characters, and "?" as a wildcard that matches any single character
 *   (or matches nothing at the end of a string).  For example:
 *   
 *.  "*" matches anything
 *.  "abc?" matches "abc", "abcd", "abce", "abcf", but not "abcde"
 *.  "abc???" matches "abc", "abcd", "abcde", "abcdef", but not "abcdefg"
 *.  "?xyz" matches "wxyz", "axyz", but not "xyz" or "abcxyz"
 *   
 *   - Return the first file that matches, if any, by filling in 'outbuf'
 *   and 'isdir' with appropriate information.  Before returning, allocate
 *   a context structure (which is entirely for your own use, and opaque
 *   to the caller) and fill it in with the information necessary for
 *   os_find_next_file to get the next matching file.  If no file matches,
 *   return null.  
 */


/*
 *   Find the next matching file, continuing a search started with
 *   os_find_first_file().  Returns null if no more files were found, in
 *   which case the search will have been automatically closed (i.e.,
 *   there's no need to call os_find_close() after this routine returns
 *   null).  Returns a non-null context pointer, which is to be passed to
 *   this function again to get the next file, if a file was found.
 *   
 *   'outbuf' and 'outpathbuf' are filled in with the filename (without
 *   path) and full path (relative or absolute, as appropriate),
 *   respectively, in the same manner as they do for os_find_first_file().
 *   
 *   Implementation note: if os_find_first_file() allocated memory for the
 *   search context, this routine must free the memory if it returs null,
 *   because this indicates that the search is finished and the caller
 *   need not call os_find_close().  
 */
void *os_find_next_file(void *ctx, char *outbuf, size_t outbufsiz,
                        int *isdir, char *outpathbuf, size_t outpathbufsiz)
{
        debugger("os_find_next_file() not implemented");
        return NULL;
}

/*
 *   Cancel a search.  The context pointer returned by the last call to
 *   os_find_first_file() or os_find_next_file() is the parameter.  There
 *   is no need to call this function if find-first or find-next returned
 *   null, since they will have automatically closed the search.
 *   
 *   Implementation note: if os_find_first_file() allocated memory for the
 *   search context, this routine should release the memory.  
 */
void os_find_close(void *ctx)
{
        debugger("os_find_close() not implemented");
}

/*
 *   Determine if the given filename refers to a special file.  Returns
 *   the appropriate enum value if so, or OS_SPECFILE_NONE if not.  The
 *   given filename must be a root name - it must not contain a path
 *   prefix.  The primary purpose of this function is to classify the
 *   'outbuf' results from os_find_first/next_file().  
 */
enum os_specfile_t os_is_special_file(const char *fname)
{
        const char* lastchar = fname + strlen(fname) - 1;
        if ((lastchar < fname) || (*lastchar != '.')) return OS_SPECFILE_NONE;

        // it's either "." or "..", so check the next-to-last char
        lastchar--;
        if ((lastchar < fname) || (*lastchar != '.')) return OS_SPECFILE_SELF;
        else return OS_SPECFILE_PARENT;
}

/* ------------------------------------------------------------------------ */
/* 
 *   Convert string to all-lowercase. 
 */
char *os_strlwr(char *s)
{
    char *start;
    start = s;
    while (*s) {
        if (isupper(*s))
            *s = tolower(*s);

        s++;
    }
    return start;
}


/* ------------------------------------------------------------------------ */
/*
 *   Character classifications for quote characters.  os_squote() returns
 *   true if its argument is any type of single-quote character;
 *   os_dquote() returns true if its argument is any type of double-quote
 *   character; and os_qmatch(a, b) returns true if a and b are matching
 *   open- and close-quote characters.
 *   
 *   These functions allow systems with extended character codes with
 *   weird quote characters (such as the Mac) to match the weird
 *   characters, so that users can use the extended quotes in input.
 *   
 *   These are usually implemented as macros.  The most common
 *   implementation simply returns true for the standard ASCII quote
 *   characters:
 *   
 *   #define os_squote(c) ((c) == '\'')
 *.  #define os_dquote(c) ((c) == '"')
 *.  #define os_qmatch(a, b) ((a) == (b))
 *   
 *   These functions take int arguments to allow for the possibility of
 *   Unicode input.  
 */
/* int os_squote(int c); */
/* int os_dquote(int c); */
/* int os_qmatch(int a, int b); */


/* ------------------------------------------------------------------------ */
/* 
 *   Seek to the resource file embedded in the current executable file.
 *   On platforms where the executable file format allows additional
 *   information to be attached to an executable, this function can be
 *   used to find the extra information within the executable.
 *   
 *   The 'typ' argument gives a resource type to find.  This is an
 *   arbitrary string that the caller uses to identify what type of object
 *   to find.  The "TGAM" type, for example, is used by convention to
 *   indicate a TADS compiled GAM file.  
 */
osfildef *os_exeseek(const char* /*exefile*/, const char* /*typ*/)
{
        return NULL;
}


/* ------------------------------------------------------------------------ */
/*
 *   Load a string resource.  Given a string ID number, load the string
 *   into the given buffer.
 *   
 *   Returns zero on success, non-zero if an error occurs (for example,
 *   the buffer is too small, or the requested resource isn't present).
 *   
 *   Whenever possible, implementations should use an operating system
 *   mechanism for loading the string from a user-modifiable resource
 *   store; this will make localization of these strings easier, since the
 *   resource store can be modified without the need to recompile the
 *   application.  For example, on the Macintosh, the normal system string
 *   resource mechanism should be used to load the string from the
 *   application's resource fork.
 *   
 *   When no operating system mechanism exists, the resources can be
 *   stored as an array of strings in a static variable; this isn't ideal,
 *   because it makes it much more difficult to localize the application.
 *   
 *   Resource ID's are application-defined.  For example, for TADS 2,
 *   "res.h" defines the resource ID's.  
 */
int os_get_str_rsc(int id, char *buf, size_t buflen)
{
        app_info appInfo;
        be_app->GetAppInfo(&appInfo);
        BFile appFile(&appInfo.ref, B_READ_ONLY);
        BResources res(&appFile);

        size_t strSize;
        const char* str = (const char*) res.LoadResource(B_STRING_TYPE, id, &strSize);
        if (str)
        {
                if (strSize < buflen - 1)               // room for the trailing null?
                {
                        memcpy(buf, str, strSize);
                        buf[strSize] = '\0';                    // ensure that it's null terminated
                        return 0;                                                       // success!
                }
        }
        return -1;              // couldn't load the resource, or resource too small
}


/* ------------------------------------------------------------------------ */
/*
 *   External, dynamically-loaded code.  These routines allow us to load
 *   and invoke external native code.  
 */

/* load an external function from a file given the name of the file */
int (*os_exfil(const char */*name*/))(void *)
{
        return NULL;
}

/* load an external function from an open file */
int (*os_exfld(osfildef */*fp*/, unsigned /*len*/))(void *)
{
        return NULL;
}

/*
 *   call an external function, passing it an argument (a string pointer),
 *   and passing back the string pointer returned by the external function 
 */
int os_excall(int (*/*extfn*/)(void *), void */*arg*/)
{
        return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   look for a file in the standard locations: current directory, program
 *   directory, TADS path 
 */
int os_locate(const char *fname, int flen, const char *arg0,
              char *buf, size_t bufsiz);


/* ------------------------------------------------------------------------ */
/*
 *   Create and open a temporary file.  Creates and opens a temporary
 *   file.  If 'fname' is null, this routine must choose a file name and
 *   fill in 'buf' with the chosen name; if possible, the file should be
 *   in the conventional location for temporary files on this system, and
 *   should be unique (i.e., it shouldn't be the same as any existing
 *   file).
 *   
 *   The filename stored in 'buf' should not be used by the caller except
 *   to pass to osfdel_temp().  On some systems, it may not be possible to
 *   determine the actual filename of a temporary file; in such cases, the
 *   implementation may store an empty string in the buffer.
 *   
 *   After the caller is done with this file, it should close the file
 *   (using osfcls() as normal), then call osfdel_temp() to delete the
 *   temporary file.  
 */
osfildef *os_create_tempfile(const char *fname, char *buf);

/*
 *   Delete a temporary file - this is used to delete a file created with
 *   os_create_tempfile().  For most platforms, this can simply be defined
 *   the same way as osfdel().  For platforms where the operating system
 *   or file manager will automatically delete a file opened as a
 *   temporary file, this routine should do nothing at all, since the
 *   system will take care of deleting the temp file.  
 */
int osfdel_temp(const char *fname);


/*
 *   Get the temporary file path.  This should fill in the buffer with a
 *   path prefix (suitable for strcat'ing a filename onto) for a good
 *   directory for a temporary file, such as the swap file.  
 */
void os_get_tmp_path(char *buf)
{
        strcpy(buf, "/tmp/");
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

/* apply a default extension to a filename, if it doesn't already have one */
void os_defext(char *fname, const char *ext)
{
        char* p = fname + strlen(fname);                // find the end of the string
        if (p > fname)                                                                                  // did we legitimately find it?
        {
                p -= (strlen(ext) + 1);         // where the "." and the extension itself should be
                if (p > fname)                                  // still looking good?
                {
                        if ((*p == '.') && (!strcmp(p+1, ext)))
                        {
                                return;                 // that's it, fname already ends with the extension
                        }
                }
        }

        // if we get here, we need to append "." and the extension
        strcat(fname, ".");
        strcat(fname, ext);
}

/* unconditionally add an extention to a filename */
void os_addext(char *fname, const char *ext)
{
        strcat(fname, ".");
        strcat(fname, ext);
}

/* remove the extension from a filename */
void os_remext(char *fname)
{
        char* p = fname + strlen(fname) - 1;
        while ((p > fname) && (*p != '.')) p--;
        if (*p == '.') *p = '\0';
}

/*
 *   Get a pointer to the root name portion of a filename.  This is the
 *   part of the filename after any path or directory prefix.  For
 *   example, on Unix, given the string "/home/mjr/deep.gam", this
 *   function should return a pointer to the 'd' in "deep.gam".  If the
 *   filename doesn't appear to have a path prefix, it should simply
 *   return the argument unchanged.  
 */
char *os_get_root_name(char *buf)
{
        char* p = strrchr(buf, '/');
        if (p > buf) p++;
        else p = buf;
        return p;
}

/*
 *   Determine whether a filename specifies an absolute or relative path.
 *   This is used to analyze filenames provided by the user (for example,
 *   in a #include directive, or on a command line) to determine if the
 *   filename can be considered relative or absolute.  This can be used,
 *   for example, to determine whether to search a directory path for a
 *   file; if a given filename is absolute, a path search makes no sense.
 *   A filename that doesn't specify an absolute path can be combined with
 *   a path using os_build_full_path().
 *   
 *   Returns true if the filename specifies an absolute path, false if
 *   not.  
 */
int os_is_file_absolute(const char *fname)
{
        if (fname && (fname[0] == '/')) return TRUE;
        else return FALSE;
}

/*
 *   Extract the path from a filename.  Fills in pathbuf with the path
 *   portion of the filename.  If the filename has no path, the pathbuf
 *   should be set appropriately for the current directory (on Unix or
 *   DOS, for example, it can be set to an empty string).
 *   
 *   The result can end with a path separator character or not, depending
 *   on local OS conventions.  Paths extracted with this function can only
 *   be used with os_build_full_path(), so the conventions should match
 *   that function's.
 *   
 *   Unix examples:
 *   
 *   "/home/mjr/deep.gam" -> "/home/mjr"
 *.  "deep.gam" -> ""
 *.  "games/deep.gam" -> "games"
 *   
 *   Mac examples:
 *   
 *   ":home:mjr:deep.gam" -> ":home:mjr"
 *.  "Hard Disk:games:deep.gam" -> "Hard Disk:games"
 *.  "Hard Disk:deep.gam" -> "Hard Disk:"
 *   
 *   Note in the last example that we've retained the trailing colon in
 *   the path, whereas we didn't in the others; although the others could
 *   also retain the trailing colon, it's required only for the last case.
 *   The last case requires the colon because it would otherwise be
 *   impossible to determine whether "Hard Disk" was a local subdirectory
 *   or a volume name.  
 */
void os_get_path_name(char *pathbuf, size_t pathbuflen, const char *fname)
{
        BPath filepath(fname);
        BPath parent;
        status_t err = filepath.GetParent(&parent);
        if (!err)
        {
                const char* parentPath = parent.Path();
                if (strlen(parentPath) < pathbuflen)
                {
                        strcpy(pathbuf, parentPath);
                }
        }
}

/*
 *   Build a full path name, given a path and a filename.  The path may
 *   have been specified by the user, or may have been extracted from
 *   another file via os_get_path_name().  This routine must take care to
 *   add path separators as needed, but also must take care not to add too
 *   many path separators.
 *   
 *   Note that relative path names may require special care on some
 *   platforms.  For example, on the Macintosh, a path of "games" and a
 *   filename "deep.gam" should yield ":games:deep.gam" (note the addition
 *   of the leading colon).
 *   
 *   Unix examples:
 *   
 *   "/home/mjr", "deep.gam" -> "/home/mjr/deep.gam"
 *.  "/home/mjr/", "deep.gam" -> "/home/mjr/deep.gam"
 *.  "games", "deep.gam" -> "games/deep.gam"
 *.  "games/", "deep.gam" -> "games/deep.gam"
 *   
 *   Mac examples:
 *   
 *   "Hard Disk:", "deep.gam" -> "Hard Disk:deep.gam"
 *.  ":games:", "deep.gam" -> ":games:deep.gam"
 *.  "games", "deep.gam" -> ":games:deep.gam" 
 */
void os_build_full_path(char *fullpathbuf, size_t fullpathbuflen,
                        const char *path, const char *filename)
{
        const char* p;
        BPath pathobj(path, filename, true);                    // force normalization of the path
        p = (*path) ? pathobj.Path() : filename;                // only use the BPath for non-null paths
        if (strlen(p) < fullpathbuflen)
        {
                strcpy(fullpathbuf, p);
        }
}

/*
 *   Convert an OS filename path to a relative URL.  Paths provided to
 *   this function should always be relative to the current directory, so
 *   the resulting URL should be relative.  If end_sep is true, it means
 *   that the last character of the result should be a '/', even if the
 *   input path doesn't end with a path separator character.
 *   
 *   This routine should replace all system-specific path separator
 *   characters in the input name with forward slashes ('/').  In
 *   addition, if a relative path on the system starts with a path
 *   separator, that leading path separator should be removed; for
 *   example, on the Macintosh, a path of ":images:rooms:startroom.jpeg"
 *   should be converted to "images/rooms/startroom.jpeg".
 */
void os_cvt_dir_url(char *result_buf, size_t result_buf_size,
                    const char *src_path, int end_sep);

/*
 *   Convert a relative URL into a relative filename path.  Paths provided
 *   to this function should always be relative, so the resulting path
 *   should be relative to the current directory.  This function
 *   essentially reverses the transformation done by os_cvt_dir_url().  If
 *   end_sep is true, the path should end with a path separator character,
 *   so that filenames can be strcat'ed onto the result to form a full
 *   filename.
 */
void os_cvt_url_dir(char *result_buf, size_t result_buf_size,
                    const char *src_url, int end_sep);


/* ------------------------------------------------------------------------ */
/*
 *   get a suitable seed for a random number generator; should use the
 *   system clock or some other source of an unpredictable and changing
 *   seed value 
 */
void os_rand(long *val);


/* ------------------------------------------------------------------------ */
/*
 *   Display routines.
 *   
 *   Our display model is a simple stdio-style character stream.
 *   
 *   In addition, we provide an optional "status line," which is a
 *   non-scrolling area where a line of text can be displayed.  If the
 *   status line is supported, text should only be displayed in this area
 *   when os_status() is used to enter status-line mode; while in status
 *   line mode, text is written to the status line area, otherwise it's
 *   written to the normal scrolling area.  The status line is normally
 *   shown in a different color to set it off from the rest of the text.
 *   
 *   The OS layer can provide its own formatting (word wrapping in
 *   particular) if it wants, in which case it should also provide
 *   pagination using os_more_prompt().  
 */

/* redraw the screen */
void os_redraw(void)
{
        debugger("os_redraw() not implemented");
}

/*
 *   Show the system-specific MORE prompt, and wait for the user to
 *   respond.  Before returning, remove the MORE prompt from the screen.
 *   
 *   This routine is only used and only needs to be implemented when the
 *   OS layer takes responsibility for pagination; this will be the case
 *   on most systems that use proportional fonts or variable-sized
 *   windows, since on such platforms the OS layer must do most of the
 *   formatting work, leaving the standard output layer unable to guess
 *   where pagination should occur.
 *   
 *   If the portable output formatter handles the MORE prompt, which is
 *   the usual case for character-mode or terminal-style implementations,
 *   this routine is not used and you don't need to provide an
 *   implementation.  Note that HTML TADS provides an implementation of
 *   this routine, because the HTML renderer handles line breaking and
 *   thus must handle pagination.  
 */
void os_more_prompt();


/*
 *   Enter HTML mode.  This is only used when the run-time is compiled
 *   with the USE_HTML flag defined.  This call instructs the renderer
 *   that HTML sequences should be parsed; until this call is made, the
 *   renderer should not interpret output as HTML.  Non-HTML
 *   implementations do not need to define this routine, since the
 *   run-time will not call it if USE_HTML is not defined.  
 */
void os_start_html(void);

/* exit HTML mode */
void os_end_html(void);

/*
 *   Global variables with the height and width of the main text display
 *   area into which os_print displays.  The height and width are given
 *   in text lines and character columns, respectively.  The portable code
 *   can use these values to format text for display via os_print(); for
 *   example, the caller can use the width to determine where to put line
 *   breaks.
 *   
 *   These values are only needed for systems where os_print() doesn't
 *   perform its own word-wrap formatting.  On systems such as the Mac,
 *   where os_print() performs word wrapping, these sizes aren't really
 *   important because the portable code doesn't need to perform any real
 *   formatting.
 */
extern int G_os_pagelength;
extern int G_os_linewidth;

/*
 *   Global flag that tells the output formatter whether to count lines
 *   that it's displaying against the total on the screen so far.  If this
 *   variable is true, lines are counted, and the screen is paused with a
 *   [More] message when it's full.  When not in MORE mode, lines aren't
 *   counted.  This variable should be set to false when displaying text
 *   that doesn't count against the current page, such as status line
 *   information.
 *   
 *   This flag should not be modified by OS code.  Instead, the output
 *   formatter will set this flag according to its current state; the OS
 *   code can use this flag to determine whether or not to display a MORE
 *   prompt during os_print()-type operations.  Note that this flag is
 *   normally interesting to the OS code only when the OS code itself is
 *   handling the MORE prompt.  
 */
extern int G_os_moremode;

/* 
 *   Update progress display with current info, if appropriate.  This can
 *   be used to provide a status display during compilation.  Most
 *   command-line implementations will just ignore this notification; this
 *   can be used for GUI compiler implementations to provide regular
 *   display updates during compilation to show the progress so far.  
 */
/* void os_progress(const char *fname, unsigned long linenum); */

/* 
 *   Set busy cursor.  If 'flag' is true, provide a visual representation
 *   that the system or application is busy doing work.  If 'flag' is
 *   false, remove any visual "busy" indication and show normal status.
 *   
 *   We provide a prototype here if your osxxx.h header file does not
 *   #define a macro for os_csr_busy.  On many systems, this function has
 *   no effect at all, so the osxxx.h header file simply #define's it to
 *   do an empty macro.  
 */
#ifndef os_csr_busy
void os_csr_busy(int flag)
{
        // do nothing under BeOS
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   User Input Routines
 */

/*
 *   Ask the user for a filename, using a system-dependent dialog or other
 *   mechanism.  Returns one of the OS_AFE_xxx status codes (see below).
 *   
 *   prompt_type is the type of prompt to provide -- this is one of the
 *   OS_AFP_xxx codes (see below).  The OS implementation doesn't need to
 *   pay any attention to this parameter, but it can be used if desired to
 *   determine the type of dialog to present if the system provides
 *   different types of dialogs for different types of operations.
 *   
 *   file_type is one of the OSFTxxx codes for system file type.  The OS
 *   implementation is free to ignore this information, but can use it to
 *   filter the list of files displayed if desired; this can also be used
 *   to apply a default suffix on systems that use suffixes to indicate
 *   file type.  If OSFTUNK is specified, it means that no filtering
 *   should be performed, and no default suffix should be applied.  
 */
int os_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
               int prompt_type, os_filetype_t file_type)
{
        os_printz(prompt);
        os_printz("> ");
        os_gets((unsigned char*) fname_buf, fname_buf_len);
        return OS_AFE_SUCCESS;
}

/* ------------------------------------------------------------------------ */
/*
 *   Ask for input through a dialog.
 *   
 *   'prompt' is a text string to display as a prompting message.  For
 *   graphical systems, this message should be displayed in the dialog;
 *   for text systems, this should be displayed on the terminal after a
 *   newline.
 *   
 *   'standard_button_set' is one of the OS_INDLG_xxx values defined
 *   below, or zero.  If this value is zero, no standard button set is to
 *   be used; the custom set of buttons defined in 'buttons' is to be used
 *   instead.  If this value is non-zero, the appropriate set of standard
 *   buttons, with labels translated to the local language if possible, is
 *   to be used.
 *   
 *   'buttons' is an array of strings to use as button labels.
 *   'button_count' gives the number of entries in the 'buttons' array.
 *   'buttons' and 'button_count' are ignored in 'standard_button_set' is
 *   non-zero, since a standard set of buttons is used instead.  If
 *   'buttons' and 'button_count' are to be used, each entry contains the
 *   label of a button to show.  
 */
/*   
 *   An ampersand ('&') character in a label string indicates that the
 *   next character after the '&' is to be used as the short-cut key for
 *   the button, if supported.  The '&' should NOT be displayed in the
 *   string; instead, the character should be highlighted according to
 *   local system conventions.  On Windows, for example, the short-cut
 *   character should be shown underlined; on a text display, the response
 *   might be shown with the short-cut character enclosed in parentheses.
 *   If there is no local convention for displaying a short-cut character,
 *   then the '&' should simply be removed from the displayed text.  
 *   
 *   The short-cut key specified by each '&' character should be used in
 *   processing responses.  If the user presses the key corresponding to a
 *   button's short-cut, the effect should be the same as if the user
 *   clicked the button with the mouse.  If local system conventions don't
 *   allow for short-cut keys, any short-cut keys can be ignored.
 *   
 *   'default_index' is the 1-based index of the button to use as the
 *   default.  If this value is zero, there is no default response.  If
 *   the user performs the appropriate system-specific action to select
 *   the default response for the dialog, this is the response that is to
 *   be selected.  On Windows, for example, pressing the "Return" key
 *   should select this item.
 *   
 *   'cancel_index' is the 1-based index of the button to use as the
 *   cancel response.  If this value is zero, there is no cancel response.
 *   This is the response to be used if the user cancels the dialog using
 *   the appropriate system-specific action.  On Windows, for example,
 *   pressing the "Escape" key should select this item.  
 */
/*
 *   icon_id is one of the OS_INDLG_ICON_xxx values defined below.  If
 *   possible, an appropriate icon should be displayed in the dialog.
 *   This can be ignored in text mode, and also in GUI mode if there is no
 *   appropriate system icon.
 *   
 *   The return value is the 1-based index of the response selected.  If
 *   an error occurs, return 0.  
 */
int os_input_dialog(int icon_id, const char *prompt, int standard_button_set,
                    const char **buttons, int button_count,
                    int default_index, int cancel_index);

/*
 *   Standard button set ID's 
 */

/* OK */
#define OS_INDLG_OK            1

/* OK, Cancel */
#define OS_INDLG_OKCANCEL      2

/* Yes, No */
#define OS_INDLG_YESNO         3

/* Yes, No, Cancel */
#define OS_INDLG_YESNOCANCEL   4

/*
 *   Dialog icons 
 */

/* no icon */
#define OS_INDLG_ICON_NONE     0

/* warning */
#define OS_INDLG_ICON_WARNING  1

/* information */
#define OS_INDLG_ICON_INFO     2

/* question */
#define OS_INDLG_ICON_QUESTION 3

/* error */
#define OS_INDLG_ICON_ERROR    4



/* ------------------------------------------------------------------------ */
/*
 *   Get the current system high-precision timer.  This function returns a
 *   value giving the wall-clock ("real") time in milliseconds, relative
 *   to any arbitrary zero point.  It doesn't matter what this value is
 *   relative to -- the only important thing is that the values returned
 *   by two different calls should differ by the number of actual
 *   milliseconds that have elapsed between the two calls.  On most
 *   single-user systems, for example, this will probably return the
 *   number of milliseconds since the user turned on the computer.
 *   
 *   True millisecond precision is NOT required.  Each implementation
 *   should simply use the best precision available on the system.  If
 *   your system doesn't have any kind of high-precision clock, you can
 *   simply use the time() function and multiply the result by 1000 (but
 *   see the note below about exceeding 32-bit precision).
 *   
 *   However, it *is* required that the return value be in *units* of
 *   milliseconds, even if your system clock doesn't have that much
 *   precision; so on a system that uses its own internal clock units,
 *   this routine must multiply the clock units by the appropriate factor
 *   to yield milliseconds for the return value.
 *   
 *   It is also required that the values returned by this function be
 *   monotonically increasing.  In other words, each subsequent call must
 *   return a value that is equal to or greater than the value returned
 *   from the last call.  On some systems, you must be careful of two
 *   special situations.
 *   
 *   First, the system clock may "roll over" to zero at some point; for
 *   example, on some systems, the internal clock is reset to zero at
 *   midnight every night.  If this happens, you should make sure that you
 *   apply a bias after a roll-over to make sure that the value returned
 *   from this return continues to increase despite the reset of the
 *   system clock.
 *   
 *   Second, a 32-bit signed number can only hold about twenty-three days
 *   worth of milliseconds.  While it seems unlikely that a TADS game
 *   would run for 23 days without a break, it's certainly reasonable to
 *   expect that the computer itself may run this long without being
 *   rebooted.  So, if your system uses some large type (a 64-bit number,
 *   for example) for its high-precision timer, you may want to store a
 *   zero point the very first time this function is called, and then
 *   always subtract this zero point from the large value returned by the
 *   system clock.  If you're using time(0)*1000, you should use this
 *   technique, since the result of time(0)*1000 will almost certainly not
 *   fit in 32 bits in most cases.
 */
long os_get_sys_clock_ms(void)
{
        return system_time() / 1000;
}

/*
 *   Sleep for a while.  This should simply pause for the given number of
 *   milliseconds, then return.  On multi-tasking systems, this should use
 *   a system API to unschedule the current process for the desired delay;
 *   on single-tasking systems, this can simply sit in a wait loop until
 *   the desired interval has elapsed.  
 */
void os_sleep_ms(long delay_in_milliseconds)
{
        bigtime_t target = system_time() + 1000LL * delay_in_milliseconds;
        while (system_time() < target) snooze(target - system_time());
}

/* set a file's type information */
void os_settype(const char *f, os_filetype_t typ);

/* open the error message file for reading */
osfildef *oserrop(const char *arg0);

/* ------------------------------------------------------------------------ */
/* 
 *   OS main entrypoint 
 */
int os0main(int oargc, char **oargv,
            int (*mainfn)(int, char **, char *), 
            const char *before, const char *config);

/* 
 *   new-style OS main entrypoint - takes an application container context 
 */
int os0main2(int oargc, char **oargv,
             int (*mainfn)(int, char **, struct appctxdef *, char *),
             const char *before, const char *config,
             struct appctxdef *appctx);

/*
 *   get filename from startup parameter, if possible; returns true and
 *   fills in the buffer with the parameter filename on success, false if
 *   no parameter file could be found 
 */
int os_paramfile(char */*buf*/)
{
        return FALSE;
}

/* 
 *   Initialize.  This should be called during program startup to
 *   initialize the OS layer and check OS-specific command-line arguments.
 *   
 *   If 'prompt' and 'buf' are non-null, and there are no arguments on the
 *   given command line, the OS code can use the prompt to ask the user to
 *   supply a filename, then store the filename in 'buf' and set up
 *   argc/argv to give a one-argument command string.  (This mechanism for
 *   prompting for a filename is obsolescent, and is retained for
 *   compatibility with a small number of existing implementations only;
 *   new implementations should ignore this mechanism and leave the
 *   argc/argv values unchanged.)  
 */
int os_init(int */*argc*/, char **/*argv*/, const char */*prompt*/, char */*buf*/, int /*bufsiz*/)
{
        // this doesn't actually *do* anything except remember the original cbreak mode so
        // that we're guaranteed to properly restore it in os_uninit()
        set_cbreak_mode(1);
        set_cbreak_mode(0);

        // nothing else to do here on BeOS
        return 0;
}


/*
 *   Uninitialize.  This is called prior to progam termination to reverse
 *   the effect of any changes made in os_init().  For example, if
 *   os_init() put the terminal in raw mode, this should restore the
 *   previous terminal mode.  This routine should not terminate the
 *   program (so don't call exit() here) - the caller might have more
 *   processing to perform after this routine returns.  
 */
void os_uninit(void)
{
        set_cbreak_mode(0);
}

/* 
 *   Terminate.  This should exit the program with the given exit status.
 *   In general, this should be equivalent to the standard C library
 *   exit() function, but we define this interface to allow the OS code to
 *   do any necessary pre-termination cleanup.  
 */
void os_term(int status)
{
        exit(status);
}

/* 
 *   Install/uninstall the break handler.  If possible, the OS code should
 *   set (if 'install' is true) or clear (if 'install' is false) a signal
 *   handler for keyboard break signals (control-C, etc, depending on
 *   local convention).  The OS code should set its own handler routine,
 *   which should note that a break occurred with an internal flag; the
 *   portable code uses os_break() from time to time to poll this flag.  
 */
void os_instbrk(int /*install*/)
{
}

/*
 *   Yield CPU; returns TRUE if user requested interrupt, FALSE to
 *   continue.  Normally, this doesn't do anything on DOS.
 */
#ifndef os_yield
int os_yield(void);
#endif

/*
 *   Initialize the time zone.  This routine is meant to take care of any
 *   work that needs to be done prior to calling localtime() and other
 *   time-zone-dependent routines in the run-time library.  For DOS and
 *   Windows, we need to call the run-time library routine tzset() to set
 *   up the time zone from the environment; most systems shouldn't need to
 *   do anything in this routine.  
 */
#ifndef os_tzset
void os_tzset(void);
#endif

/*
 *   Set the default saved-game extension.  This routine will NOT be
 *   called when we're using the standard saved game extension; this
 *   routine will be invoked only if we're running as a stand-alone game,
 *   and the game author specified a non-standard saved-game extension
 *   when creating the stand-alone game.
 *   
 *   This routine is not required if the system does not use the standard,
 *   semi-portable os0.c implementation.  Even if the system uses the
 *   standard os0.c implementation, it can provide an empty routine here
 *   if the system code doesn't need to do anything special with this
 *   information.
 *   
 *   The extension is specified as a null-terminated string.  The
 *   extension does NOT include the leading period.  
 */
void os_set_save_ext(const char */*ext*/)
{
        // ignore
}

/*
 *   Generate a filename for a character-set mapping file.  This function
 *   should determine the current native character set in use, if
 *   possible, then generate a filename, according to system-specific
 *   conventions, that we should attempt to load to get a mapping between
 *   the current native character set and the internal character set
 *   identified by 'internal_id'.
 *   
 *   The internal character set ID is a string of up to 4 characters.
 *   
 *   On DOS, the native character set is a DOS code page.  DOS code pages
 *   are identified by 3- or 4-digit identifiers; for example, code page
 *   437 is the default US ASCII DOS code page.  We generate the
 *   character-set mapping filename by appending the internal character
 *   set identifier to the DOS code page number, then appending ".TCP" to
 *   the result.  So, to map between ISO Latin-1 (internal ID = "La1") and
 *   DOS code page 437, we would generate the filename "437La1.TCP".
 *   
 *   Note that this function should do only two things.  First, determine
 *   the current native character set that's in use.  Second, generate a
 *   filename based on the current native code page and the internal ID.
 *   This function is NOT responsible for figuring out the mapping or
 *   anything like that -- it's simply where we generate the correct
 *   filename based on local convention.
 *   
 *   'filename' is a buffer of at least OSFNMAX characters.
 *   
 *   'argv0' is the executable filename from the original command line.
 *   This parameter is provided so that the system code can look for
 *   mapping files in the original TADS executables directory, if desired.
 */
void os_gen_charmap_filename(char */*filename*/, char */*internal_id*/, char */*argv0*/)
{
        debugger("os_gen_charmap_filename() not implemented");
}

/*
 *   Receive notification that a character mapping file has been loaded.
 *   The caller doesn't require this routine to do anything at all; this
 *   is purely for the system-dependent code's use so that it can take
 *   care of any initialization that it must do after the caller has
 *   loaded a charater mapping file.  'id' is the character set ID, and
 *   'ldesc' is the display name of the character set.  'sysinfo' is the
 *   extra system information string that is stored in the mapping file;
 *   the interpretation of this information is up to this routine.
 *   
 *   For reference, the Windows version uses the extra information as a
 *   code page identifier, and chooses its default font character set to
 *   match the code page.  On DOS, the run-time requires the player to
 *   activate an appropriate code page using a DOS command (MODE CON CP
 *   SELECT) prior to starting the run-time, so this routine doesn't do
 *   anything at all on DOS. 
 */
void os_advise_load_charmap(char */*id*/, char */*ldesc*/, char */*sysinfo*/)
{
        // do nothing with charmaps under BeOS
}

/*
 *   Generate the name of the character set mapping table for Unicode
 *   characters to and from the given local character set.  Fills in the
 *   buffer with the implementation-dependent name of the desired
 *   character set map.  See below for the character set ID codes.
 *   
 *   For example, on Windows, the implementation would obtain the
 *   appropriate active code page (which is simply a Windows character set
 *   identifier number) from the operating system, and build the name of
 *   the Unicode mapping file for that code page, such as "CP1252".  On
 *   Macintosh, the implementation would look up the current script system
 *   and return the name of the Unicode mapping for that script system,
 *   such as "ROMAN" or "CENTEURO".
 *   
 *   If it is not possible to determine the specific character set that is
 *   in use, this function should return "asc7dflt" (ASCII 7-bit default)
 *   as the character set identifier on an ASCII system, or an appropriate
 *   base character set name on a non-ASCII system.  "asc7dflt" is the
 *   generic character set mapping for plain ASCII characters.
 *   
 *   The given buffer must be at least 32 bytes long; the implementation
 *   must limit the result it stores to 32 bytes.  (We use a fixed-size
 *   buffer in this interface for simplicity, and because there seems no
 *   need for greater flexibility in the interface; a character set name
 *   doesn't carry very much information so shouldn't need to be very
 *   long.  Note that this function doesn't generate a filename, but
 *   simply a mapping name; in practice, a map name will be used to
 *   construct a mapping file name.)
 *   
 *   Because this function obtains the Unicode mapping name, there is no
 *   need to specify the internal character set to be used: the internal
 *   character set is Unicode.  
 */
/*
 *   Implementation note: when porting this routine, the convention that
 *   you use to name your mapping files is up to you.  You should simply
 *   choose a convention for this implementation, and then use the same
 *   convention for packaging the mapping files for your OS release.  In
 *   most cases, the best convention is to use the names that the Unicode
 *   consortium uses in their published cross-mapping listings, since
 *   these listings can be used as the basis of the mapping files that you
 *   include with your release.  For example, on Windows, the convention
 *   is to use the code page number to construct the map name, as in
 *   CP1252 or CP1250.  
 */
void os_get_charmap(char *mapname, int /*charmap_id*/)
{
        // BeOS always uses native UTF-8, which makes TADS very happy.
        // "utf8" is a special name to TADS3, indicating that the runtime should
        // just trust the OS to Do The Right Thing (tm) with raw UTF-8 strings.
        strcpy(mapname, "utf8");
}

/*
 *   Get system information.  'code' is a SYSINFO_xxx code, which
 *   specifies what type of information to get.  The 'param' argument's
 *   meaning depends on which code is selected.  'result' is a pointer to
 *   an integer that is to be filled in with the result value.  If the
 *   code is not known, this function should return FALSE.  If the code is
 *   known, the function should fill in *result and return TRUE.
 */
int os_get_sysinfo(int code, void */*param*/, long *result)
{
    switch(code)
    {
    case SYSINFO_HTML:
    case SYSINFO_JPEG:
    case SYSINFO_PNG:
    case SYSINFO_WAV:
    case SYSINFO_MIDI:
    case SYSINFO_WAV_MIDI_OVL:
    case SYSINFO_WAV_OVL:
    case SYSINFO_PREF_IMAGES:
    case SYSINFO_PREF_SOUNDS:
    case SYSINFO_PREF_MUSIC:
    case SYSINFO_PREF_LINKS:
    case SYSINFO_MPEG:
    case SYSINFO_MPEG1:
    case SYSINFO_MPEG2:
    case SYSINFO_MPEG3:
    case SYSINFO_LINKS_HTTP:
    case SYSINFO_LINKS_FTP:
    case SYSINFO_LINKS_NEWS:
    case SYSINFO_LINKS_MAILTO:
    case SYSINFO_LINKS_TELNET:
    case SYSINFO_PNG_TRANS:
    case SYSINFO_PNG_ALPHA:
    case SYSINFO_OGG:
    case SYSINFO_MNG:
    case SYSINFO_MNG_TRANS:
    case SYSINFO_MNG_ALPHA:
    case SYSINFO_TEXT_HILITE:
    case SYSINFO_TEXT_COLORS:
    case SYSINFO_BANNERS:
        /* 
         *   we don't support any of these features - set the result to 0
         *   to indicate this 
         */
        *result = 0;

        /* return true to indicate that we recognized the code */
        return TRUE;

    case SYSINFO_INTERP_CLASS:
        /* we're a text-only interpreter in text-only mode */
        *result = SYSINFO_ICLASS_TEXT;
        return TRUE;

    default:
        /* we don't recognize other codes */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Integer division operators.  For any compiler that follows ANSI C
 *   rules, no definitions are required for these routine, since the
 *   standard definitions below will work properly.  However, if your
 *   compiler does not follow ANSI standards with respect to integer
 *   division of negative numbers, you must provide implementations of
 *   these routines that produce the correct results.
 *   
 *   Division must "truncate towards zero," which means that any
 *   fractional part is dropped from the result.  If the result is
 *   positive, the result must be the largest integer less than the
 *   algebraic result: 11/3 yields 3.  If the result is negative, the
 *   result must be the smallest integer less than the result: (-11)/3
 *   yields -3.
 *   
 *   The remainder must obey the relationship (a/b)*b + a%b == a, for any
 *   integers a and b (b != 0).
 *   
 *   If your compiler does not obey the ANSI rules for the division
 *   operators, make the following changes in your osxxx.h file
 *   
 *   - define the symbol OS_NON_ANSI_DIVIDE in the osxxx.h file
 *   
 *   - either define your own macros for os_divide_long() and
 *   os_remainder_long(), or put actual prototypes for these functions
 *   into your osxxx.h file and write appropriate implementations of these
 *   functions in one of your osxxx.c or .cpp files.
 */
/* long os_divide_long(long a, long b);    // returns (a/b) with ANSI rules */
/* long os_remainder_long(long a, long b); // returns (a%b) with ANSI rules */

/* standard definitions for any ANSI compiler */
#ifndef OS_NON_ANSI_DIVIDE
#define os_divide_long(a, b)     ((a) / (b))
#define os_remainder_long(a, b)  ((a) % (b))
#endif

/* ------------------------------------------------------------------------ */
/*
 *   TADS 2 swapping configuration.  Define OS_DEFAULT_SWAP_ENABLED to 0
 *   if you want swapping turned off, 1 if you want it turned on.  If we
 *   haven't defined a default swapping mode yet, turn swapping on by
 *   default.  
 */
#ifndef OS_DEFAULT_SWAP_ENABLED
# define OS_DEFAULT_SWAP_ENABLED   1
#endif

/*
 *   If the system "long description" (for the banner) isn't defined, make
 *   it the same as the platform ID string.  
 */
#ifndef OS_SYSTEM_LDESC
# define OS_SYSTEM_LDESC  OS_SYSTEM_NAME
#endif

/*
 *   TADS 2 Usage Lines
 *   
 *   If the "usage" lines (i.e., the descriptive lines of text describing
 *   how to run the various programs) haven't been otherwise defined,
 *   define defaults for them here.  Some platforms use names other than
 *   tc, tr, and tdb for the tools (for example, on Unix they're usually
 *   tadsc, tadsr, and tadsdb), so the usage lines should be adjusted
 *   accordingly; simply define them earlier than this point (in this file
 *   or in one of the files included by this file, such as osunixt.h) for
 *   non-default text.  
 */
#ifndef OS_TC_USAGE
# define OS_TC_USAGE  "usage: tc [options] file"
#endif
#ifndef OS_TR_USAGE
# define OS_TR_USAGE  "usage: tr [options] file"
#endif
#ifndef OS_TDB_USAGE
# define OS_TDB_USAGE  "usage: tdb [options] file"
#endif

/*
 *   Ports can define a special TDB startup message, which is displayed
 *   after the copyright/version banner.  If it's not defined at this
 *   point, define it to an empty string.  
 */
#ifndef OS_TDB_STARTUP_MSG
# define OS_TDB_STARTUP_MSG ""
#endif

/*
 *   If a system patch sub-level isn't defined, define it here as zero.
 *   The patch sub-level is used on some systems where a number of ports
 *   are derived from a base port (for example, a large number of ports
 *   are based on the generic Unix port).  For platforms like the Mac,
 *   where the porting work applies only to that one platform, this
 *   sub-level isn't meaningful.
 */
#ifndef OS_SYSTEM_PATCHSUBLVL
# define OS_SYSTEM_PATCHSUBLVL  "0"
#endif

/*
 * Provide memicmp since it's not a standard C or POSIX function.
 */
int memicmp(const char *s1, const char *s2, size_t len)
{
        char *x1, *x2;   
        size_t i;

        x1 = new char[len];
        x2 = new char[len];

        for (i = 0; i < len; i++) {
                x1[i] = tolower(s1[i]);
                x2[i] = tolower(s2[i]);
        }

        int result = memcmp(x1, x2, len);
        delete [] x1;
        delete [] x2;
        return result;
}

// BeOS R5 doesn't provide the proper wcs*() functions, so we do here
wchar_t* wcscpy(wchar_t* dest, const wchar_t* src)
{
        wchar_t* orig_dest = dest;
        while (*src)
        {
                *dest++ = *src++;
        }
        *dest = 0;
        return orig_dest;
}

size_t wcslen(const wchar_t* str)
{
        size_t len = 0;
        while (*str)
        {
                len++;
                str++;
        }
        return len;
}

// This wcstol() doesn't set the error conditions according to the C++ standard
long wcstol(const wchar_t* str, wchar_t** endptr, int base)
{
        long result = 0;
        while (*str && (*str >= '0') && (*str - '0' < base))
        {
                result = result*base + *str - '0';
                str++;
        }
        if (endptr)
        {
                *endptr = (wchar_t*) str;
        }
        return result;
}

#ifdef __cplusplus
}
#endif

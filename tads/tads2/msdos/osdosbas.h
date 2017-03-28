/* $Header: d:/cvsroot/tads/TADS2/msdos/OSDOSBAS.H,v 1.4 1999/07/11 00:46:37 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdosbas.h - DOS Base definitions
Function
  These definitions apply to DOS, Windows, and OS/2.
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#ifndef OSDOSBAS_H
#define OSDOSBAS_H

#include <time.h>
#include <stdarg.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/stat.h>
#ifdef TURBO
# include <conio.h>
#endif
#ifdef DJGPP
# include <dir.h>
# include <unistd.h>
# include <sys/stat.h>
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Define the maintainer for this platform 
 */
#define  TADS_OEM_NAME   "Mike Roberts <mjr_@hotmail.com>"

/* ------------------------------------------------------------------------ */
/*
 *   For the Microsoft compiler, turn on error C4057 ("type1 differs in
 *   indirection to slightly different base types from type2"), which
 *   catches implicit conversions of pointers from signed to unsigned
 *   types and vice versa.  The Microsoft compiler does not by default
 *   flag this condition as an error or warning, whereas many compilers on
 *   other platforms do.  We must thus explicitly enable this warning to
 *   catch these with the Microsoft compiler.  We turn on the error so
 *   that we catch these types of errors before anyone tries to port the
 *   code to a platform with a stricter compiler.
 *   
 *   To enable the error, use the warning pragma to assign warning level 2
 *   to message 4057.  This warning normally has warning level 4, which is
 *   far too pedantic to use globally (the biggest problem is that many of
 *   the system headers generate tons of warnings at this level); by
 *   moving the message to level 2, we will ensure that we see this
 *   warning, without having to see all of the excessively pedantic
 *   warnings at level 4.  
 */
#ifdef MICROSOFT
# pragma warning(2: 4057)
#endif

/* ------------------------------------------------------------------------ */
/*
 *   For the Microsoft compiler, fill out case tables fully where 'switch'
 *   performance is critical.
 *   
 *   Use the Microsoft-specific "__assume(0)" construct in the 'default' case
 *   to tell the compiler to assume that case tables are fully populated.  
 */
#ifdef MICROSOFT
# define OS_FILL_OUT_CASE_TABLES
# define OS_IMPOSSIBLE_DEFAULT_CASE default: __assume(0);
#endif

/* ------------------------------------------------------------------------ */
/*
 *   If TRUE and FALSE aren't defined, define them now 
 */
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Define the exact-size integer types.
 */

#if defined(MICROSOFT) || defined(_MSC_VER)
/* 
 *   Older Microsoft compilers don't provide <stdint.h> with the ANSI type
 *   definitions, but they do have built-in equivalents of their own. 
 */
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif /* MICROSOFT */

#if defined(TURBO) || defined(DJGPP)
/*
 *   Borland C doesn't have stdint.h equivalents, but since it only has a
 *   32-bit configuration, we know that 'short' is 16 bits and 'long' is 32
 *   bits.  The same goes for djgpp.
 */
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;
#endif /* TURBO || DJGPP */

#ifdef DJGPP
typedef long long int64_t;
typedef unsigned long long uint64_t;
#endif

/* ------------------------------------------------------------------------ */
/*
 *   osgen configuration options for this operating system.  These
 *   determine which routines from osgen are included.  Routines from
 *   osgen can be omitted by turning off various of these symbols, if the
 *   OS wants to provide its own more custom definition for the routine.  
 */
#define STD_OSCLS
#define STD_OS_HILITE
#define USEDOSCOMMON
#define USE_OVWCHK
#define USE_DOSEXT
#define USE_NULLPAUSE
#define USE_TIMERAND
#define USE_NULLSTYPE
#define USE_PATHSEARCH                  /* use search paths for tads files */
#define USE_STDARG

#if defined(TURBO) || defined(DJGPP)
#define USE_GENRAND
#endif


/* ------------------------------------------------------------------------ */
/*
 *   os_va_copy - on Intel platforms, va_list is a simple pointer type, so
 *   this can be handled with a simple assignment.  Don't define this for
 *   DJGPP, since we can use the compiler-provided va_copy instead (which
 *   we'll do in osifc.h).  
 */
#ifndef DJGPP
# define os_va_copy(dst, src) ((dst) = (src))
# define os_va_copy_end(dst)
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Display configuration.  For HTML TADS builds, use OS-level line wrapping
 *   and MORE mode; for text-only builds, use formatter-level line wrapping
 *   and MORE mode.  
 */
#ifdef USE_HTML
# define USE_OS_LINEWRAP /* HTML mode - use OS-level wrapping and MORE mode */
#else
# define USE_MORE /* text-only - use formatter-level wrapping and MORE mode */
#endif

/* maximum width (in characters) of a line of text */
#define OS_MAXWIDTH  135

#ifdef RUNTIME
# define USE_STDIO_INPDLG
# define USE_FSDBUG                               /* full-screen debug mode */
# define USE_STATLINE
# define USE_HISTORY
# define USE_SCROLLBACK

# ifdef USE_SCROLLBACK
#  define OS_SBSTAT "(Review Mode)  \030=Up  \031=Down  PgUp=Page Up  \
PgDn=Page Down  F1=Exit"
# endif /* USE_SCROLLBACK */

# ifdef USE_HISTORY
#  define HISTBUFSIZE 4096
# endif /* USE_HISTORY */

/*
 *   Some global variables needed for console implementation 
 */
# ifdef OSGEN_INIT
#  define E
#  define I(a) =(a)
# else /* OSGEN_INIT */
#  define E extern
#  define I(a)
# endif /* OSGEN_INIT */

E int status_mode;
E int ldesc_color I(112);
E int text_color I(0x17), text_bold_color I(0x1F), text_normal_color I(0x17);
E int sdesc_color I(0x70);
E int debug_color I(0xe);

# undef E
# undef I

#else /* RUNTIME */
# define USE_STDIO
# define USE_NULLSTAT
# define USE_NULLSCORE
#endif /* RUNTIME */


/* ------------------------------------------------------------------------ */
/*
 *   DOS-specific interfaces to low-level support routines 
 */
void oss_con_init(void);
void oss_con_uninit(void);
void ossgmx(int *max_line, int *max_column);
int ossmon(void);
void oss_win_resize_event(void);

/* 
 *   low-level console character reader - DOS-specific interface 
 */
char os_getch(void);

/*
 *   Get a character from the keyboard with a timeout.  The timeout is an
 *   interval in milliseconds; for example, a timeout of 1000 specifies
 *   that we should wait no more than one second for a character to be
 *   pressed.
 *   
 *   If a key is pressed before the timeout elapses, this should return
 *   OS_GETS_OK and put the character value in *ch. If the timeout elapses
 *   before a key is pressed, this returns OS_GETS_TIMEOUT.  If an error
 *   occurs, this returns OS_GETS_EOF.  
 */
int oss_getc_timeout(unsigned char *ch, unsigned long timeout);

/* status codes for timeout-based input functions */
#define OS_GETS_OK       0
#define OS_GETS_EOF      1
#define OS_GETS_TIMEOUT  2


/* ------------------------------------------------------------------------ */
/*
 *   Far pointers - any 32-bit version (DPMI32, Win32) makes no
 *   distinction between near and far pointers.  16-bit platforms
 *   distinguish near and far pointers; define the type 'osfar_t' to 'far'
 *   on those platforms.  
 */

#if defined(__DPMI32__) || defined(T_WIN32) || defined(WIN32) || defined(DJGPP)
#define __32BIT__
#define osfar_t
#endif

#ifndef osfar_t
#define osfar_t far
#endif


/* ------------------------------------------------------------------------ */
/*
 *   OS_LOADDS - special function qualifier needed for certain types of
 *   callback functions.
 */

/* define OS_LOADDS appropriately for the Microsoft compiler */
#ifdef MICROSOFT
# ifndef T_WIN32
#  define OS_LOADDS _loadds
# endif /* T_WIN32 */
#endif /* MICROSOFT */

/* if OS_LOADDS isn't defined, define it to nothing */
#ifndef OS_LOADDS
# define OS_LOADDS
#endif


/* ------------------------------------------------------------------------ */
/*
 *   OS_DECL_TLS - special syntax for declaring a global variable using
 *   thread-local storage.
 */
#if defined(MICROSOFT) || defined(TURBO) || defined(T_WIN32)
# define OS_DECLARATIVE_TLS
# define OS_DECL_TLS(typ, varname)  __declspec(thread) typ varname
#endif
#if defined(DJGPP)
# define OS_DECLARATIVE_TLS
# define OS_DECL_TLS(typ, varname)  __thread typ varname
#endif


/* ------------------------------------------------------------------------ */
/*
 *   General compiler configuration
 */

/* void a return value */
#define DISCARD (void)

/* copy a structure - dst and src are structures, not pointers */
#define OSCPYSTRUCT(dst, src) ((dst) = (src))

/* ANSI compiler */
#define OSANSI


/* ------------------------------------------------------------------------ */
/*
 *   General operating system interfaces
 */

/* allocate/release storage - malloc/free where supported */
/* void *osmalloc(size_t siz); */
/* void *osrealloc(void *block, size_t siz); */
/* void osfree(void *block); */

#if defined(_Windows) && !defined(__DPMI32__) && !defined(T_WIN32)

/*
 *   16-bit Windows - use our ltk_ routines 
 */
#include "ltk.h"

void  *ltk_alloc(size_t siz);
void   ltk_free(void *blk);
# define osmalloc(siz) ltk_alloc(siz)
# define osfree(block) ltk_free(block)
# define osrealloc(ptr, siz) ltk_realloc(ptr, siz)

#else /* _Windows */

# ifdef T_WIN32
/*
 *   Win32 - we'll define our own routines that implement the memory
 *   management functions 
 */
#  define osmalloc(siz)       oss_win_malloc(siz)
#  define osfree(ptr)         oss_win_free(ptr)
#  define osrealloc(ptr, siz) oss_win_realloc(ptr, siz)

/* our internal routines that implement osmalloc, etc */
void *oss_win_malloc(size_t siz);
void oss_win_free(void *ptr);
void *oss_win_realloc(void *ptr, size_t siz);

/* free all allocated memory blocks */
void oss_win_free_all();

/* 
 *   notify oss_win that we've reached the program's main entrypoing; this
 *   detaches any existing memory allocations from the master deletion list,
 *   since anything allocated before 'main' must have been allocated by a C++
 *   static object constructor, and thus must not be considered dynamic
 *   memory to be deleted between interpreter invocations 
 */
void oss_win_static_init_done();

# else /* T_WIN32 */

/*
 *   For any other case, use the standard library malloc functions 
 */
# define osmalloc(siz) malloc(siz)
# define osfree(block) free(block)
# define osrealloc(ptr, siz) realloc(ptr, siz)

# endif /* T_WIN32 */
#endif /* _Windows */

/* main program exit codes */
#define OSEXSUCC 0                                 /* successful completion */
#define OSEXFAIL 1                                        /* error occurred */


/* ------------------------------------------------------------------------ */
/*
 *   File system interface 
 */

/* maximum length of a filename */
#ifdef _MAX_PATH
/* this is the Microsoft macro for the maximum length of a file path */
# define OSFNMAX  _MAX_PATH
#else
/* this is the Borland macro for the maximum length of a file path */
# define OSFNMAX  MAXPATH
#endif

/* normal path separator character */
#define OSPATHCHAR '\\'

/* alternate path separator characters */
#define OSPATHALT "/:"

/* 
 *   URL path separators - do not include ":", since we don't want to convert
 *   this to a "/" in a URL 
 */
#define OSPATHURL "\\/"

/* directory separator character for PATH-style environment variables */
#define OSPATHSEP ';'                                        /* ':' on UNIX */

/* current working directory */
#define OSPATHPWD "."


/* 
 *   OS file structure type.  All files are manipulated through pointers
 *   to this type. 
 */
typedef FILE osfildef;

/* 
 *   Directory search handle, for os_open_dir() et al.  The file search
 *   mechanism we use varies by compiler, so we'll define this as an opaque
 *   pointer type; the concrete implementations for the different compilers
 *   use this to point to appropriate structure types.
 */
typedef void *osdirhdl_t;


#ifdef MICROSOFT
# define os_time_t        __time64_t
# define os_gmtime(t)     _gmtime64(t)
# define os_localtime(t)  _localtime64(t)
# define os_time(t)       _time64(t)
#else
/*
 *   OS file time structure 
 */
struct os_file_time_t
{
    time_t t;
};
#endif


/* get a line of text from a text file (fgets semantics) */
/* char *osfgets(char *buf, size_t len, osfildef *fp); */
#define osfgets(buf, len, fp) fgets(buf, len, fp)

/* write a line of text to a text file (fputs semantics) */
/* void osfputs(const char *buf, osfildef *fp); */
#define osfputs(buf, fp) fputs(buf, fp)

/* write bytes to file; TRUE ==> error */
/* int osfwb(osfildef *fp, const uchar *buf, int bufl); */
#define osfwb(fp, buf, bufl) (fwrite(buf, bufl, 1, fp) != 1)

/* flush buffers; TRUE ==> error */
/* void osfflush(osfildef *fp); */
#define osfflush(fp) fflush(fp)

/* read bytes from file; TRUE ==> error */
/* int osfrb(osfildef *fp, uchar *buf, int bufl); */
#define osfrb(fp, buf, bufl) (fread(buf, bufl, 1, fp) != 1)

/* read bytes from file and return count; returns # bytes read, 0=error */
/* size_t osfrbc(osfildef *fp, uchar *buf, size_t bufl); */
#define osfrbc(fp, buf, bufl) (fread(buf, 1, bufl, fp))

/* get position in file */
/* long osfpos(osfildef *fp); */
#define osfpos(fp) ftell(fp)

/* seek position in file; TRUE ==> error */
/* int osfseek(osfildef *fp, long pos, int mode); */
#define osfseek(fp, pos, mode) fseek(fp, pos, mode)
#define OSFSK_SET  SEEK_SET
#define OSFSK_CUR  SEEK_CUR
#define OSFSK_END  SEEK_END

/* close a file */
/* void osfcls(osfildef *fp); */
#define osfcls(fp) fclose(fp)

/* delete a file - TRUE if error */
/* int osfdel(const char *fname); */
#define osfdel(fname) remove(fname)

/* rename a file - true on success, false on failure */
/* int os_rename_file(const char *oldname, const char *newname); */
#define os_rename_file(oldname, newname) (rename(oldname, newname) == 0)

/* access a file - 0 if file exists */
/* int osfacc(const char *fname) */
#define osfacc(fname) access(fname, 0)

/* file mode */
int osfmode(const char *fname, int follow_links,
            unsigned long *mode, unsigned long *attrs);

#ifdef MICROSOFT
# define OSFMODE_FILE     _S_IFREG
# define OSFMODE_DIR      _S_IFDIR
# define OSFMODE_BLK      0
# define OSFMODE_CHAR     _S_IFCHR
# define OSFMODE_PIPE     _S_IFIFO
# define OSFMODE_SOCKET   0
# define OSFMODE_LINK     0
#endif
#if defined(DJGPP) || defined(TURBO)
# define OSFMODE_FILE     S_IFREG
# define OSFMODE_DIR      S_IFDIR
# define OSFMODE_BLK      0
# define OSFMODE_CHAR     S_IFCHR
# define OSFMODE_PIPE     S_IFIFO
# define OSFMODE_SOCKET   0
# define OSFMODE_LINK     0
#endif

/* 
 *   file attributes - we define our own bits for these, since the osifc set
 *   doesn't correspond exactly to the DOS/Windows set (DOS/Win has a
 *   READONLY flag, whereas osifc has separate READ and WRITE flags;
 *   furthermore, WRITE isn't merely the inverse of READONLY, but also
 *   conveys information on ACL permissions on NTFS volumes).
 */
# define OSFATTR_HIDDEN   0x0001
# define OSFATTR_SYSTEM   0x0002
# define OSFATTR_READ     0x0004
# define OSFATTR_WRITE    0x0008

/* get a character from a file */
/* int osfgetc(osfildef *fp); */
#define osfgetc(fp) fgetc(fp)

/*
 *   on 16-bit protected-mode DOS, we need to check manually for break, so
 *   do this in os_yield; on other DOS versions, we don't need to do
 *   anything in os_yield 
 */
#ifndef __DPMI16__
# define os_yield()  FALSE
#endif

/*
 *   Set "busy" cursor on/off.  For systems with a mouse cursor, set the
 *   cursor to an appropriate shape to notify the user that the system is
 *   busy.  Does nothing on DOS.
 */
/* void os_csr_busy(int show_as_busy); */
#define os_csr_busy(show_as_busy)



/* ------------------------------------------------------------------------ */
/*
 *   TADS 2 macro/function configuration.  Modern configurations always
 *   use the no-macro versions, so these definitions should always be set
 *   as shown below.  
 */
#define OS_MCM_NO_MACRO
#define ERR_NO_MACRO


#endif /* OSDOSBAS_H */


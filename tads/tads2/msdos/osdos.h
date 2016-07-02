/* $Header: d:/cvsroot/tads/TADS2/msdos/OSDOS.H,v 1.4 1999/07/11 00:46:37 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osdos.h - MS-DOS system definitions
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#ifndef OSDOS_H
#define OSDOS_H

#ifdef __cplusplus
extern "C" {
#endif

/* include general DOS/Windows definitions */
#include "osdosbas.h"

/* ------------------------------------------------------------------------ */
/*
 *   System name and long desription (for banner)
 */
#ifdef DJGPP
# define OS_SYSTEM_NAME  "DJGPP"
#else
# define OS_SYSTEM_NAME  "MSDOS"
#endif

#define OS_SYSTEM_LDESC "MS-DOS"


/* ------------------------------------------------------------------------ */
/*
 *   File Opening - use stdio
 */

/* newline sequence - DOS/Windows use CR-LF */
#define OS_NEWLINE_SEQ  "\r\n"

/* open text file for reading; returns NULL on error */
/* osfildef *osfoprt(const char *fname, os_filetype_t typ); */
#define osfoprt(fname, typ) fopen(fname, "r")

/* open text file for 'volatile' reading; returns NULL on error */
/* osfildef *osfoprtv(const char *fname, os_filetype_t typ); */
#define osfoprtv(fname, typ) osfoprt(fname, typ)

/* open text file for writing; returns NULL on error */
/* osfildef *osfopwt(const char *fname, os_filetype_t typ); */
#define osfopwt(fname, typ) fopen(fname, "w")

/* open text file for reading/writing; don't truncate */
osfildef *osfoprwt(const char *fname, os_filetype_t typ);

/* open text file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtt(const char *fname, os_filetype_t typ); */
#define osfoprwtt(fname, typ) fopen(fname, "w+")

/* open binary file for writing; returns NULL on error */
/* osfildef *osfopwb(const char *fname, os_filetype_t typ); */
#define osfopwb(fname, typ) fopen(fname, "wb")

/* open SOURCE file for reading - use appropriate text/binary mode */
/* osfildef *osfoprs(const char *fname, os_filetype_t typ); */
#define osfoprs(fname, typ) fopen(fname, "rb")

/* open binary file for reading; returns NULL on erorr */
/* osfildef *osfoprb(const char *fname, os_filetype_t typ); */
#define osfoprb(fname, typ) fopen(fname, "rb")

/* open binary file for 'volatile' reading; returns NULL on erorr */
/* osfildef *osfoprbv(const char *fname, os_filetype_t typ); */
#define osfoprbv(fname, typ) osfoprb(fname, typ)

/* open binary file for reading/writing; don't truncate */
osfildef *osfoprwb(const char *fname, os_filetype_t typ);

/* open binary file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtb(const char *fname, os_filetype_t typ); */
#define osfoprwtb(fname, typ) fopen(fname, "w+b")


/* ------------------------------------------------------------------------ */
/*
 *   Duplicate a file handle
 */
osfildef *osfdup(osfildef *fp, const char *mode);

/* ------------------------------------------------------------------------ */
/*
 *   sprintf equivalents with buffer allocation 
 */
int os_asprintf(char **bufptr, const char *fmt, ...);
int os_vasprintf(char **bufptr, const char *fmt, va_list ap);



/* ------------------------------------------------------------------------ */
/*
 *   Set "busy" cursor on/off.  For systems with a mouse cursor, set the
 *   cursor to an appropriate shape to notify the user that the system is
 *   busy.  Does nothing on DOS.
 */
/* void os_csr_busy(int show_as_busy); */
#define os_csr_busy(show_as_busy)


/* ------------------------------------------------------------------------ */
/*
 *   If error messages are to be included in the executable, define
 *   ERR_LINK_MESSAGES.  Otherwise, they'll be read from an external
 *   file that is to be opened with oserrop().
 */
#ifdef DJGPP
#define ERR_LINK_MESSAGES
#endif


/* ------------------------------------------------------------------------ */
/* 
 *   Update progress display with linfdef info, if appropriate.  This can
 *   be used to provide a status display during compilation.  Most
 *   command-line implementations will just ignore this notification; this
 *   can be used for GUI compiler implementations to provide regular
 *   display updates during compilation to show the progress so far.  
 */
#define os_progress(fname, linenum)


/* ------------------------------------------------------------------------ */
/*
 *   Single/double quote matching macros.  Used to allow systems with
 *   extended character codes with weird quote characters (such as Mac) to
 *   match the weird characters. 
 */
#define os_squote(c) ((c) == '\'')
#define os_dquote(c) ((c) == '"')
#define os_qmatch(a, b) ((a) == (b))


/* ------------------------------------------------------------------------ */
/*
 *   on 16-bit protected-mode DOS, we need to check manually for break, so
 *   do this in os_yield; on other DOS versions, we don't need to do
 *   anything in os_yield 
 */
#ifdef MSDOS
# ifndef __DPMI16__
#  define os_yield()  FALSE
# endif
#endif


/* ------------------------------------------------------------------------ */
/* 
 *   theoretical maximum osmalloc size is all of memory 
 */
#ifdef __32BIT__
# define OSMALMAX 0xffffffffL
#else /* !__32BIT__ */
# define OSMALMAX 0xffffL
#endif /* __32BIT__ */


/* ------------------------------------------------------------------------ */
/*
 *   Usage lines - for protected mode, we use different names for the
 *   executables, so reflect that in the usage lines.  This isn't needed
 *   for the real-mode DOS versions, since they use the deault command
 *   names.  
 */
#ifdef __DPMI16__
# define OS_TC_USAGE  "usage: tcx [options] file"
# define OS_TR_USGAE  "usage: trx [options] file"
# define OS_TDB_USAGE  "usage: tdbx [options] file"
#endif

/* add the special TR option messages */
#define ERR_TRUS_OS_FIRST    ERR_TRUS_DOS_1
#define ERR_TRUS_OS_LAST     ERR_TRUS_DOS_L

/*
 *   If we're in 16-bit mode, either real or protected, reduce the default
 *   stack size, since the real system stack is very constrained for these
 *   executables.  (The interpreter consumes system stack in proportion to
 *   TADS stack space, so limiting the TADS stack will help ensure we
 *   don't overflow system stack.)  
 */
#ifndef __32BIT__
# define TRD_STKSIZ   100
# define TRD_STKSIZ_MSG  "  -ms size      stack size (default 100 elements)"
#endif


#ifdef DJGPP
# define OS_TC_USAGE  "usage: tadsc [options] file"
# define OS_TR_USAGE  "usage: tadsr [options] file"

/*
 *   Make default buffer sizes huge, since we can be fairly liberal with
 *   memory on DJGPP 
 */
#include "osbigmem.h"

/*
 *   Swapping off by default, since DJGPP has virtual memory 
 */
# define OS_DEFAULT_SWAP_ENABLED  0

/*
 *   argv[0] required by dbgu.c 
 */
extern char *__dos_argv0;                                    /* from crt0.h */
# define ARG0 __dos_argv0

# ifndef TADS_OEM_NAME
#  define TADS_OEM_VERSION  "0"
#  define OS_SYSTEM_PATCHSUBLVL  "0"
#  define TADS_OEM_NAME  "jim.dunleavy@erha.ie"
# endif
#endif /* DJGPP */


#ifdef __cplusplus
}
#endif

#endif /* OSDOS_H */


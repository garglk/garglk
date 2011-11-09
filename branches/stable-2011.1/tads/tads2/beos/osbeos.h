/*
 *   osbeos.h - TADS2 operating system definitions for BeOS
 *   
 *   Copyright (c) 2000 by Christopher Tate.  All rights reserve
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software. d.  
 */

#ifndef OSBEOS_H
#define OSBEOS_H 1

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use BEOSPATCHLEVEL, as defined in the makefile, for the port patch level */
#define TADS_OEM_VERSION  BEOSPATCHLEVEL

/* Use SYSPL, as defined in the makefile, for the patch sub-level */
#define OS_SYSTEM_PATCHSUBLVL  SYSPL

/* Use PORTER, from the makefile, for the OEM maintainer name */
#define TADS_OEM_NAME  PORTER

/* Use SYSNAME, as defined in the makefile, for the system's banner ID */
#define OS_SYSTEM_NAME "BeOS"
#define OS_SYSTEM_LDESC  OS_SYSTEM_NAME

/*
 *   Define a startup message for the debugger warning that the debugger
 *   only works in 80x50 windows 
 */
#define OS_TDB_STARTUP_MSG \
   "NOTE: This program must be run in a 80x50 character window.\n"


/* maximum width (in characters) of a line of text */
#define OS_MAXWIDTH  1024

/*
 * Make default buffer sizes huge.  (Why not?  We have virtual memory.)
 */
#include "osbigmem.h"

/*
 *   Swapping off by default, since Unix machines tend to be big compared
 *   to adventure games 
 */
#define OS_DEFAULT_SWAP_ENABLED  0

/*
 *   Usage lines - we use non-default names for the executables on Unix,
 *   so we need to adjust the usage lines accordingly. 
 */
# define OS_TC_USAGE  "usage: tadsc [options] file"
# define OS_TR_USGAE  "usage: tadsr [options] file"


/*
 * Get GCC varags defs
 */

#include <stdarg.h>             /* get native stdargs */

//#define HAVE_TPARM      /* define if this system has the tparm routine */

#define OS_USHORT_DEFINED
#define OS_UINT_DEFINED
#define OS_ULONG_DEFINED

#define remove(filename) unlink(filename)

/*
 * BeOS has strcasecmp, not stricmp
 */
#define stricmp strcasecmp
#define strnicmp strncasecmp

/*
 * Prototype memicmp since it's not a standard C or POSIX function.
 */
int memicmp(const char *s1, const char *s2, size_t len);

/* far pointer type qualifier (null on most platforms) */
#  define osfar_t

/* maximum theoretical size of malloc argument */
#  define OSMALMAX ((size_t)0xffffffff)

/* cast an expression to void */
#  define DISCARD (void)

/* display lines on which errors occur */
/* #  define OS_ERRLINE 1 */

/*
 *   If long cache-manager macros are NOT allowed, define
 *   OS_MCM_NO_MACRO.  This forces certain cache manager operations to be
 *   functions, which results in substantial memory savings.  
 */
/* #define OS_MCM_NO_MACRO */

/* likewise for the error handling subsystem */
/* #define OS_ERR_NO_MACRO */

/*
 *   If error messages are to be included in the executable, define
 *   OS_ERR_LINK_MESSAGES.  Otherwise, they'll be read from an external
 *   file that is to be opened with oserrop().
 */
#define OS_ERR_LINK_MESSAGES
#define ERR_LINK_MESSAGES

/* round a size to worst-case alignment boundary */
#define osrndsz(s) (((s)+3) & ~3)

/* round a pointer to worst-case alignment boundary */
#define osrndpt(p) ((uchar *)((((ulong)(p)) + 3) & ~3))

/* service macros for osrp2 etc. */
#define osc2u(p, i) ((uint)(((uchar *)(p))[i]))
#define osc2l(p, i) ((ulong)(((uchar *)(p))[i]))

/*
 * The portable representation is little-endian, like x86s, so under
 * BeOS x86 these are simple macros
 */

/* read unaligned portable unsigned 2-byte value, returning int */
#define osrp2(p) ((int)*(unsigned short *)(p))

/* read unaligned portable signed 2-byte value, returning int */
#define osrp2s(p) ((int)*(short *)(p))

/* write int to unaligned portable 2-byte value */
#define oswp2(p, i) (*(unsigned short *)(p)=(unsigned short)(i))

/* read unaligned portable 4-byte value, returning long */
#define osrp4(p) (*(long *)(p))

/* write long to unaligned portable 4-byte value */
#define oswp4(p, l) (*(long *)(p)=(l))

/* use standard malloc/free/realloc functions */
#define osmalloc malloc
#define osfree free
#define osrealloc realloc

/* copy a structure - dst and src are structures, not pointers */
#define OSCPYSTRUCT(dst, src) ((dst) = (src))

/* maximum length of a filename */
#define OSFNMAX NAME_MAX

/* path separator characters */
#define OSPATHCHAR '/'
#define OSPATHALT "/"
#define OSPATHSEP ':'
#define OSPATHURL "/"

/* os file structure */
typedef FILE osfildef;

/* os file time structure */
struct os_file_time_t
{
    time_t t;
};

/* main program exit codes */
#define OSEXSUCC 0                                 /* successful completion */
#define OSEXFAIL 1                                        /* error occurred */

/* open text file for reading; returns NULL on error */
/* osfildef *osfoprt(char *fname, int typ); */
#define osfoprt(fname, typ) fopen(fname, "r")

/* open text file for reading in volatile ('deny none') mode */
/* osfildef *osfoprtv(char *fname, int typ); */
#define osfoprtv(fname, typ) osfoprt(fname, typ)

/* open text file for writing; returns NULL on error */
/* osfildef *osfopwt(char *fname, int typ); */
#define osfopwt(fname, typ) fopen(fname, "w")

/* open text file for reading/writing; don't truncate */
osfildef *osfoprwt(const char *fname, os_filetype_t typ);

/* open text file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtt(char *fname, int typ); */
#define osfoprwtt(fname, typ) fopen(fname, "w+")

/* open binary file for writing; returns NULL on error */
/* osfildef *osfopwb(char *fname, int typ); */
#define osfopwb(fname, typ) fopen(fname, "wb")

/* open source file for reading */
/* osfildef *osfoprs(char *fname, int typ); */
#define osfoprs(fname, typ) osfoprt(fname, typ)

/* open binary file for reading; returns NULL on erorr */
/* osfildef *osfoprb(char *fname, int typ); */
#define osfoprb(fname, typ) fopen(fname, "rb")

/* open binary file for 'volatile' reading; returns NULL on erorr */
/* osfildef *osfoprbv(char *fname, int typ); */
#define osfoprbv(fname, typ) osfoprb(fname, typ)

/* get a line of text from a text file (fgets semantics) */
/* char *osfgets(char *buf, size_t len, osfildef *fp); */
#define osfgets(buf, len, fp) fgets(buf, len, fp)

/* open binary file for reading/writing; don't truncate */
osfildef *osfoprwb(const char *fname, os_filetype_t typ);

/* open binary file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtb(char *fname, int typ); */
#define osfoprwtb(fname, typ) fopen(fname, "w+b")

/* write bytes to file; TRUE ==> error */
/* int osfwb(osfildef *fp, uchar *buf, int bufl); */
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
/* int osfdel(char *fname); */
#define osfdel(fname) remove(fname)

/* access a file - 0 if file exists */
/* int osfacc(char *fname) */
#define osfacc(fname) access(fname, 0)

/* get a character from a file */
/* int osfgetc(osfildef *fp); */
#define osfgetc(fp) fgetc(fp)

/* write a string to a file */
/* void osfputs(char *buf, osfildef *fp); */
#define osfputs(buf, fp) fputs(buf, fp)


/* ignore OS_LOADDS definitions */
# define OS_LOADDS

/* Show cursor as busy */
#define os_csr_busy(show_as_busy)

/* yield CPU; returns TRUE if user requested interrupt, FALSE to continue */
#define os_yield()  FALSE

/* update progress display with current info, if appropriate */
#define os_progress(fname, linenum)

#define os_tzset()

/*
 *   Single/double quote matching macros.  Used to allow systems with
 *   extended character codes with weird quote characters (such as Mac) to
 *   match the weird characters. 
 */
#define os_squote(c) ((c) == '\'')
#define os_dquote(c) ((c) == '"')
#define os_qmatch(a, b) ((a) == (b))

/*
 *   Options for this operating system
 */
# define USE_MORE        /* assume more-mode is desired (undef if not true) */
/* # define USEDOSCOMMON */
/* # define USE_OVWCHK */
/* # define USE_DOSEXT */
# define USE_NULLPAUSE
# define USE_TIMERAND
# define USE_NULLSTYPE
# define USE_PATHSEARCH
# define STD_OS_HILITE
# define STD_OSCLS


/*
 * Turning this off will enable termcap output.
 */
/* #define USE_STDIO */

#define USE_STDIO_INPDLG

#ifdef  USE_STDIO
/* #  define USE_NULLINIT */
#  define USE_NULLSTAT
#  define USE_NULLSCORE
#else   /* USE_STDIO */
#  define RUNTIME
#  define USE_FSDBUG
#  define USE_STATLINE
#  define USE_HISTORY
#  define USE_SCROLLBACK
#endif

#  ifdef USE_SCROLLBACK
#   define OS_SBSTAT "(Review Mode)  ^P=Up  ^N=Down  <=Page Up  \
>=Page Down  Esc=Exit"
#  endif /* USE_SCROLLBACK */

#  ifdef USE_HISTORY
#   define HISTBUFSIZE 4096
#  endif /* USE_HISTORY */

/*
 *   Some global variables needed for console implementation
 */
#  ifdef OSGEN_INIT
#   define E
#   define I(a) =(a)
#  else /* OSGEN_INIT */
#   define E extern
#   define I(a)
#  endif /* OSGEN_INIT */

E int status_mode;
E int score_column I(70);
E int sdesc_line I(0), sdesc_column I(0), sdesc_color I(23);
E int ldesc_line I(1), ldesc_column I(0), ldesc_color I(112);
E int debug_color I(0xe);

E int text_line I(1);
E int text_column I(0);
E int text_color I(7), text_bold_color I(15), text_normal_color I(7);
E int max_line I(24), max_column I(79);
E int ldesc_curline;
E int text_lastcol;

#  undef E
#  undef I

/* BeOS R5 doesn't provide the proper wcs*() functions, so we do here */
wchar_t* wcscpy(wchar_t* dest, const wchar_t* src);
size_t wcslen(const wchar_t* str);
long wcstol(const wchar_t* str, wchar_t** endptr, int base);

#ifdef __cplusplus
}
#endif

#endif /* OSUNIXT_INCLUDED */

/*
$Header: d:/cvsroot/tads/TADS2/atari/OSATARI.H,v 1.2 1999/05/17 02:52:16 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os(atari).h - operating system definitions for Atari ST running GCC 2.3.1 P1
Function
  Definitions that vary by operating system
Notes
  None
Modified
  20/01/93 David Baggett - rewrote for v2.0
  08/13/91 MJRoberts     - creation
*/

#ifndef OS_INCLUDED
#define OS_INCLUDED

/*
 * Declare memicmp -- not a std library function
 */
int memicmp(char *, char *, int);

/* far pointer type qualifier (null on most platforms) */
#  define osfar_t

/* maximum theoretical size of malloc argument */
#  define OSMALMAX ((size_t)0xffffffff)

#  include <stdlib.h>
#  include <stdio.h>
#  include <stddef.h>

/* void a return value */
#define DISCARD (void)

/*
 *   If long cache-manager macros are NOT allowed, define
 *   OS_MCM_NO_MACRO.  This forces certain cache manager operations to be
 *   functions, which results in substantial memory savings.  
 */
#define OS_MCM_NO_MACRO

/* likewise for the error handling subsystem */
#define OS_ERR_NO_MACRO

/*
 *   If error messages are to be included in the executable, define
 *   OS_ERR_LINK_MESSAGES.  Otherwise, they'll be read from an external
 *   file that is to be opened with oserrop().
 */
#define ERR_LINK_MESSAGES

/* round a size to worst-case alignment boundary */     /* DMB */
#define osrndsz(s) (ushort) (((s)+1) & ~1L)

/* round a pointer to worst-case alignment boundary */  /* DMB */
#define osrndpt(p) ((uchar *)((((ulong)(p)) + 1) & ~1L))

/* void pointer type */
typedef void dvoid;

/* service macros for osrp2 etc. */
#define osc2u(p, i) ((uint)(((uchar *)(p))[i]))
#define osc2l(p, i) ((ulong)(((uchar *)(p))[i]))

/* read unaligned portable 2-byte value, returning int */
#define osrp2(p) (osc2u(p, 0) + (osc2u(p, 1) << 8))

/* write int to unaligned portable 2-byte value */
#define oswp2(p, i) ((((uchar *)(p))[1] = (uchar) ((i)>>8)), \
                        (((uchar *)(p))[0] = (uchar) ((i)&255)))

/* read unaligned portable 4-byte value, returning long */
#define osrp4(p) \
 (osc2l(p, 0) + (osc2l(p, 1) << 8) + (osc2l(p, 2) << 16) + (osc2l(p, 3) << 24))

/* write long to unaligned portable 4-byte value */
#define oswp4(p, i) \
 ((((uchar *)(p))[0] = (i)), (((uchar *)(p))[1] = (i)>>8),\
  (((uchar *)(p))[2] = (i)>>16, (((uchar *)(p))[3] = (i)>>24)))

/*
 * Be sure to get correct parameter and return type for memory functions.
 */
#include <alloc.h>

/* allocate storage - malloc where supported */
/* dvoid *osmalloc(size_t siz); */
#define osmalloc(siz) malloc(siz)

/* free storage allocated with osmalloc */
/* void osfree(dvoid *block); */
#define osfree(block) free(block)

/* copy a structure - dst and src are structures, not pointers */
#define OSCPYSTRUCT(dst, src) ((dst) = (src))

/* ANSI compiler */
#define OSANSI

/* maximum length of a filename */
#define OSFNMAX  255

/* normal path separator character */
#define OSPATHCHAR '\\'

/* alternate path separator characters */
#define OSPATHALT "/:"

/* URL path separator */
#define OSPATHURL "\\/"

/* os file structure */
typedef FILE osfildef;

/* main program exit codes */
#define OSEXSUCC 0                                 /* successful completion */
#define OSEXFAIL 1                                        /* error occurred */

/* open text file for reading; returns NULL on error */
/* osfildef *osfoprt(char *fname); */
#define osfoprt(fname) fopen(fname, "r")

/* open binary file for writing; returns NULL on error */
/* osfildef *osfopwb(char *fname); */
#define osfopwb(fname) fopen(fname, "wb")

/* open SOURCE file for reading - use appropriate text/binary mode */
#define osfoprs(fname) fopen(fname, "rb")

/* open binary file for reading; returns NULL on erorr */
/* osfildef *osfoprb(char *fname); */
#define osfoprb(fname) fopen(fname, "rb")

/* get a line of text from a text file (fgets semantics) */
/* char *osfgets(char *buf, size_t len, osfildef *fp); */
#define osfgets(buf, len, fp) fgets(buf, len, fp)

/* open binary file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtb(char *fname); */
#define osfoprwtb(fname) fopen(fname, "w+b")

/* write bytes to file; TRUE ==> error */
/* int osfwb(osfildef *fp, uchar *buf, int bufl); */
#define osfwb(fp, buf, bufl) (fwrite(buf, bufl, 1, fp) != 1)

/* read bytes from file; TRUE ==> error */
/* int osfrb(osfildef *fp, uchar *buf, int bufl); */
#define osfrb(fp, buf, bufl) (fread(buf, bufl, 1, fp) != 1)

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


/* open error file, returning file handle, or null if not found */
osfildef *oserrop(/*_ void _*/);


/* yield CPU; returns TRUE if user requested interrupt, FALSE to continue */
#define os_yield()  FALSE

/*
 *   Options for this operating system
 */
# define USE_MORE        /* assume more-mode is desired (undef if not true) */
# define USEDOSCOMMON

/* #define USE_STDIO */
#define USE_DOSEXT 
/* #define USE_NULLINIT */
/* #define USE_NULLPAUSE */
#define USE_EXPAUSE
#define USE_TIMERAND
/* #define STD_ASKFILE */
/* #define USE_NULLSTAT do-nothing os_status */
/* #define USE_NULLSCORE - use a do-nothing os_score function */
#define USE_OVWCHK
#define USE_NULLSTYPE

#ifndef USE_STDIO

/*
 * Output options for VT52 output.
 */
#define RUNTIME
/* #define USE_FSDBUG */
#define USE_STATLINE
#define USE_HISTORY
/* #define USE_SCROLLBACK */

#define OS_SBSTAT \
"(Review Mode) Arrows=Up/Down F9=Page Up  F10=Page Down  F1=Exit"

#define HISTBUFSIZE 4096

#ifndef STAT_TYPE
#define STAT_TYPE 0
#endif


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

#endif  /* USE_STDIO */

#endif  /* OS_INCLUDED */

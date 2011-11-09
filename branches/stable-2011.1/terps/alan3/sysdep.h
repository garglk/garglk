/*----------------------------------------------------------------------*\

  sysdep.h

  System dependencies file for Alan Adventure Language system

  N.B. The test for symbols used here should really be of three types
  - processor name (like PC, x86, ...)
  - os name (DOS, WIN32, Solaris2, ...)
  - compiler name and version (DJGPP, CYGWIN, GCC271, THINK-C, ...)

  The set symbols should indicate if a feature is on or off like the GNU
  AUTOCONFIG package does.

  This is not completely done yet!

\*----------------------------------------------------------------------*/
#ifndef _SYSDEP_H_
#define _SYSDEP_H_


/* Place definitions of OS and compiler here if necessary */
#ifndef __sun__
#ifdef sun
#define __sun__
#endif
#endif

#ifdef _INCLUDE_HPUX_SOURCE
#define __hp__
#endif

#ifndef __unix__
#ifdef unix
#define __unix__
#endif
#endif

#ifdef vax
#define __vms__
#endif

#ifdef THINK_C
#define __mac__
#endif

#ifdef __APPLE__
// At least GCC 3.x does define this for Darwin
#define __macosx__
#define __unix__
#endif

#ifdef DOS
#define __dos__
#endif

#ifdef __BORLANDC__
#define __dos__
#endif

#ifdef __MINGW32__
#define __windows__
#endif

#ifdef __CYGWIN32__
#define __cygwin__
#endif

#ifdef __PACIFIC__
#define  __dos__
#define HAVE_SHORT_FILENAMES
#endif


#ifdef HAVE_WINGLK
#define HAVE_GLK
#endif

#ifdef HAVE_GARGLK
#define HAVE_GLK
#endif


/*----------------------------------------------------------------------

  Below follows OS and compiler dependent settings. They should not be
  changed except for introducing new sections when porting to new
  environments.

 */

/************/
/* Includes */
/************/

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>


#ifdef __STDC__
#define _PROTOTYPES_
#include <stdlib.h>
#include <string.h>
#endif


#ifdef __mac__
#define _PROTOTYPES_
#include <stdlib.h>
#include <string.h>
#endif

/***********************/
/* ISO character sets? */
/***********************/

/* Common case first */
#define ISO 1
#define NATIVECHARSET 0

#ifdef HAVE_GLK
#undef ISO
#define ISO 1
#undef NATIVECHARSET
#define NATIVECHARSET 0
#else	/* Glk is ISO, no matter what the OS */

#ifdef __dos__
#undef ISO
#define ISO 0
#undef NATIVECHARSET
#define NATIVECHARSET 2
#endif

#ifdef __win__
#undef ISO
#define ISO 1
#undef NATIVECHARSET
#define NATIVECHARSET 2
#endif

/* Old Macs uses other CHARSET, Mac OS X uses ISO */
#ifdef __mac__
#undef ISO
#define ISO 0
#undef NATIVECHARSET
#define NATIVECHARSET 1
#endif

#endif

/**************************/
/* Strings for file modes */
/**************************/
#define READ_MODE "rb"
#define WRITE_MODE "wb"


/****************************/
/* Allocates cleared bytes? */
/****************************/

#ifdef __CYGWIN__
#define NOTCALLOC
#endif

#ifdef __MINGW32__
#define NOTCALLOC
#endif

#ifdef __unix__
#define NOTCALLOC
#endif


/****************/
/* Have termio? */
/****************/

#ifdef HAVE_GLK
#  undef HAVE_TERMIO   /* don't need TERMIO */
#else
#  ifdef __CYGWIN__
#    define HAVE_TERMIO
#  endif
#  ifdef __unix__
#    define HAVE_TERMIO
#  endif
#endif

/*******************************/
/* Is ANSI control available? */
/*******************************/

#ifdef HAVE_GLK
#  undef HAVE_ANSI /* don't need ANSI */
#else
#  ifdef __CYGWIN__
#    define HAVE_ANSI
#  endif
#endif

/******************************/
/* Use the READLINE function? */
/******************************/
#define USE_READLINE
#ifdef SOME_PLATFORM_WHICH_CANT_USE_READLINE
#  undef USE_READLINE
#endif

/* Special cases and definition overrides */
#ifdef __unix__
#define MULTI
#endif


#ifdef __dos__

/* Return codes */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE  1

#endif


/* Native character functions */
extern int isSpace(unsigned int c);      /* IN - Native character to test */
extern int isLower(unsigned int c);      /* IN - Native character to test */
extern int isUpper(unsigned int c);      /* IN - Native character to test */
extern int isLetter(unsigned int c);     /* IN - Native character to test */
extern int toLower(unsigned int c);      /* IN - Native character to convert */
extern int toUpper(unsigned int c);      /* IN - Native character to convert */
extern char *strlow(char str[]); /* INOUT - Native string to convert */
extern char *strupp(char str[]); /* INOUT - Native string to convert */

/* ISO character functions */
extern int isISOLetter(int c);  /* IN - ISO character to test */
extern char IsoToLowerCase(int c); /* IN - ISO character to convert */
extern char IsoToUpperCase(int c); /* IN - ISO character to convert */
extern char *stringLower(char str[]); /* INOUT - ISO string to convert */
extern char *stringUpper(char str[]); /* INOUT - ISO string to convert */
extern int compareStrings(char str1[], char str2[]); /* Case-insensitive compare */

/* ISO string conversion functions */
extern void toIso(char copy[],  /* OUT - Mapped string */
		  char original[], /* IN - string to convert */
		  int charset);	/* IN - The current character set */

extern void fromIso(char copy[], /* OUT - Mapped string */
		    char original[]); /* IN - string to convert */

extern void toNative(char copy[], /* OUT - Mapped string */
		     char original[], /* IN - string to convert */
		     int charset); /* IN - current character set */

extern int littleEndian(void);

extern char *baseNameStart(char *fullPathName);

#endif                          /* -- sysdep.h -- */


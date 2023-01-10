/*----------------------------------------------------------------------*\

  sysdep.h

  System dependencies file for Alan Adventure Language system

  N.B. The test for symbols used here should really be of three types
  - processor name (like PC, x86, ...)
  - os name (WIN32, Cygwin, MSYS2...)
  - compiler name and version (DJGPP, CYGWIN, GCC271, THINK-C, ...)

\*----------------------------------------------------------------------*/
#ifndef _SYSDEP_H_
#define _SYSDEP_H_

#include <stdbool.h>

/* Place definitions of OS and compiler here if necessary */

/* For command line editing in "pure" (non-Glk, non-Gargoyle)
 * interpreters, we need termio, assume it's available */
#define HAVE_TERMIOS

#ifdef __MINGW32__
#define __windows__
#undef HAVE_TERMIOS
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

#ifdef __win__
#undef ISO
#define ISO 1
#undef NATIVECHARSET
#define NATIVECHARSET 2
#endif

#endif

/**************************/
/* Strings for file modes */
/**************************/
#define READ_MODE "rb"
#define WRITE_MODE "wb"


/****************/
/* Have termio? */
/****************/

#ifdef HAVE_GLK
#  undef HAVE_TERMIOS   /* don't need TERMIO */
#else
#  ifdef __CYGWIN__
#    define HAVE_TERMIOS
#  endif
#  ifdef __unix__
#    define HAVE_TERMIOS
#  endif
#endif

/*******************************/
/* Is ANSI control available? */
/*******************************/

#ifdef HAVE_GLK
#  undef HAVE_ANSI_CONTROL /* don't need ANSI */
#else
#  ifdef __CYGWIN__
#    define HAVE_ANSI_CONTROL
#  endif
#endif

/******************************/
/* Use the READLINE function? */
/******************************/
#define USE_READLINE
#ifdef SOME_PLATFORM_WHICH_CANT_USE_READLINE
#  undef USE_READLINE
#endif


/* Internal character functions */
extern int isSpace(unsigned int c);      /* IN - Internal character to test */
extern int isLower(unsigned int c);      /* IN - Internal character to test */
extern int isUpper(unsigned int c);      /* IN - Internal character to test */
extern int isLetter(unsigned int c);     /* IN - Internal character to test */
extern int toLower(unsigned int c);      /* IN - Internal character to convert */
extern int toUpper(unsigned int c);      /* IN - Internal character to convert */
extern void stringToLowerCase(char str[]); /* INOUT - Internal string to convert */

extern bool equalStrings(char str1[], char str2[]); /* Case-insensitive compare */

extern int littleEndian(void);

extern char *baseNameStart(char *fullPathName);

#endif

/* $Header: d:/cvsroot/tads/TADS2/msdos/OSWIN.H,v 1.4 1999/07/11 00:46:37 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oswin.h - OS definitions for 32-bit Windows (95/98/NT)
Function
  
Notes
  
Modified
  10/17/98 MJRoberts  - Creation
*/

#ifndef OSWIN_H
#define OSWIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <share.h>
#include <fcntl.h>

/* include general DOS/Windows definitions */
#include "osdosbas.h"

/* ------------------------------------------------------------------------ */
/*
 *   Application instance handle global variable.  The code with the
 *   WinMain() entrypoint function must provide a definition of this
 *   variable, and must initialize it to the application instance handle.
 *   
 *   We implement this as a global variable partly for simplicity, but
 *   mostly because it will make it evident at link time if the WinMain
 *   routine forgets to define this.
 *   
 *   (Note that the definition below is commented out so that we avoid
 *   using the windows.h type HINSTANCE in this portable interface file.
 *   The actual extern is in oswin.c; this comment is here for documentary
 *   purposes only.)  
 */
/* extern HINSTANCE oss_G_hinstance; */


/* ------------------------------------------------------------------------ */
/*
 *   System name and long description 
 */
#define OS_SYSTEM_NAME "WIN32"
#define OS_SYSTEM_LDESC "Windows"


/* ------------------------------------------------------------------------ */
/*
 *   Opening Files.  We use the special share-mode version of fopen so
 *   that we can enforce reasonable file-sharing rules: opening a file for
 *   reading locks out writers; opening a file for writing locks out
 *   readers as well as other writers.
 *   
 *   Other than the sharing modes, we'll use normal stdio routines for our
 *   file interfaces.  
 */

/* newline sequence - DOS/Windows use CR-LF */
#define OS_NEWLINE_SEQ  "\r\n"

/* internal file-open routine */
osfildef *os_fsopen(const char *fname, const char *mode, int omode, int sh);

/* open text file for reading; returns NULL on error */
/* osfildef *osfoprt(const char *fname, os_filetype_t typ); */
#define osfoprt(fname, typ) \
    os_fsopen(fname, "r", _O_RDONLY | _O_TEXT, _SH_DENYWR)

/* open text file for 'volatile' reading; returns NULL on error */
/* osfildef *osfoprtv(const char *fname, os_filetype_t typ); */
#define osfoprtv(fname, typ) \
    os_fsopen(fname, "r", _O_RDONLY | _O_TEXT, _SH_DENYNO)

/* open text file for writing; returns NULL on error */
/* osfildef *osfopwt(const char *fname, os_filetype_t typ); */
#define osfopwt(fname, typ) \
    os_fsopen(fname, "w", \
              _O_CREAT | _O_TRUNC | _O_WRONLY | _O_TEXT, _SH_DENYWR)

/* open text file for reading/writing; don't truncate */
#define osfoprwt(fname, typ) \
    os_fsopen(fname, "r+", _O_CREAT | _O_RDWR | _O_TEXT, _SH_DENYRW)

/* open text file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtt(const char *fname, os_filetype_t typ); */
#define osfoprwtt(fname, typ) \
    os_fsopen(fname, "w+", \
              _O_CREAT | _O_TRUNC | _O_RDWR | _O_TEXT, _SH_DENYWR)

/* open binary file for writing; returns NULL on error */
/* osfildef *osfopwb(const char *fname, os_filetype_t typ); */
#define osfopwb(fname, typ) \
    os_fsopen(fname, "wb", \
              _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _SH_DENYWR)

/* open SOURCE file for reading - use appropriate text/binary mode */
/* osfildef *osfoprs(const char *fname, os_filetype_t typ); */
#define osfoprs(fname, typ) \
    os_fsopen(fname, "rb", _O_RDONLY | _O_BINARY, _SH_DENYWR)

/* open binary file for reading; returns NULL on erorr */
/* osfildef *osfoprb(const char *fname, os_filetype_t typ); */
#define osfoprb(fname, typ) \
    os_fsopen(fname, "rb", _O_RDONLY | _O_BINARY, _SH_DENYWR)

/* open binary file for 'volatile' reading; returns NULL on erorr */
/* osfildef *osfoprbv(const char *fname, os_filetype_t typ); */
#define osfoprbv(fname, typ) \
    os_fsopen(fname, "rb", _O_RDONLY | _O_BINARY, _SH_DENYNO)

/* open binary file for reading/writing; don't truncate */
#define osfoprwb(fname, typ) \
    os_fsopen(fname, "r+b", _O_CREAT | _O_RDWR | _O_BINARY, _SH_DENYRW)

/* open binary file for reading/writing; truncate; returns NULL on error */
/* osfildef *osfoprwtb(const char *fname, os_filetype_t typ); */
#define osfoprwtb(fname, typ) \
    os_fsopen(fname, "w+b", \
              _O_CREAT | _O_TRUNC | _O_RDWR | _O_BINARY, _SH_DENYWR)


/* ------------------------------------------------------------------------ */
/*
 *   sprintf equivalents with buffer allocation 
 */
int os_asprintf(char **bufptr, const char *fmt, ...);
int os_vasprintf(char **bufptr, const char *fmt, va_list ap);


/* ------------------------------------------------------------------------ */
/*
 *   Set the initial directory for os_askfile dialogs 
 */
void oss_set_open_file_dir(const char *dir);


/* ------------------------------------------------------------------------ */
/*
 *   If error messages are to be included in the executable, define
 *   ERR_LINK_MESSAGES.  Otherwise, they'll be read from an external
 *   file that is to be opened with oserrop().
 */
/* #define ERR_LINK_MESSAGES */


/* ------------------------------------------------------------------------ */
/* 
 *   Update progress display with current info, if appropriate.  This can
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
 *   theoretical maximum osmalloc size is all of memory 
 */
#define OSMALMAX 0xffffffffL


/* ------------------------------------------------------------------------ */
/* 
 *   usage lines for 32-bit command-line tools 
 */
# define OS_TC_USAGE   "usage: tc32 [options] file"
# define OS_TR_USAGE   "usage: t2r32 [options] file"
# define OS_TDB_USAGE  "usage: tdb32 [options] file"

/* add the special console-mode options messages, if appropriate */
# ifndef HTMLTADS
#  define ERR_TRUS_OS_FIRST    ERR_TRUS_DOS32_1
#  define ERR_TRUS_OS_LAST     ERR_TRUS_DOS32_L
# endif


/* ------------------------------------------------------------------------ */
/*
 *   Make default buffer sizes huge, since we can be fairly liberal with
 *   memory on win95/nt 
 */
#include "osbigmem.h"

#ifdef __cplusplus
}
#endif

#endif /* OSWIN_H */


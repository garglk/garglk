/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* Adapted from osfrobtads.h from FrobTADS 1.2.3.
 * FrobTADS copyright (C) 2009 Nikos Chantziaras.
 */

#ifndef OSGLK_H
#define OSGLK_H

#define USE_OS_LINEWRAP	/* tell tads not to let os do paging */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* We don't support the Atari 2600. */
#include "osbigmem.h"

/* For strcasecmp() */
#include <strings.h>

#include "glk.h"
#include "glkstart.h"

#define TRUE 1
#define FALSE 0

#ifdef __cplusplus
extern "C" {
#endif

/* Used by the base code to inhibit "unused parameter" compiler warnings. */
#define VARUSED(var) (void)var

/* We assume that the C-compiler is mostly ANSI compatible. */
#define OSANSI

/* Special function qualifier needed for certain types of callback
 * functions.  This is for old 16-bit systems; we don't need it and
 * define it to nothing. */
#define OS_LOADDS

/* Unices don't suffer the near/far pointers brain damage (thank God) so
 * we make this a do-nothing macro. */
#define osfar_t

/* This is used to explicitly discard computed values (some compilers
 * would otherwise give a warning like "computed value not used" in some
 * cases).  Casting to void should work on every ANSI-Compiler. */
#define DISCARD (void)

/* Copies a struct into another.  ANSI C allows the assignment operator
 * to be used with structs. */
#define OSCPYSTRUCT(x,y) ((x)=(y))

/* Link error messages into the application. */
#define ERR_LINK_MESSAGES

/* Program Exit Codes. */
#define OSEXSUCC 0 /* Successful completion. */
#define OSEXFAIL 1 /* Failure. */

/* Here we configure the osgen layer; refer to tads2/osgen3.c for more
 * information about the meaning of these macros. */
#define USE_DOSEXT
#define USE_NULLSTYPE

/* Theoretical maximum osmalloc() size.
 * Unix systems have at least a 32-bit memory space.  Even on 64-bit
 * systems, 2^32 is a good value, so we don't bother trying to find out
 * an exact value. */
#define OSMALMAX 0xffffffffL

#ifdef _WIN32
struct Win32Dir {
    HANDLE hFindFile;
    char *dirname;
    WIN32_FIND_DATAA findFileData;
};
typedef struct Win32Dir *osdirhdl_t;
#else
/* Directory handle for searches via os_open_dir() et al. */
typedef DIR* osdirhdl_t;
#endif

/* file type/mode bits */
#define OSFMODE_FILE    S_IFREG
#define OSFMODE_DIR     S_IFDIR
#define OSFMODE_CHAR    S_IFCHR
#ifdef S_IFBLK
#define OSFMODE_BLK     S_IFBLK
#else
#define OSFMODE_BLK     0
#endif
#ifdef S_IFIFO
#define OSFMODE_PIPE    S_IFIFO
#else
#define OSFMODE_PIPE    0
#endif
#ifdef S_IFLNK
#define OSFMODE_LINK    S_IFLNK
#else
#define OSFMODE_LINK    0
#endif
#ifdef S_IFSOCK
#define OSFMODE_SOCKET  S_IFSOCK
#else
#define OSFMODE_SOCKET  0
#endif

/* File attribute bits. */
#define OSFATTR_HIDDEN  0x0001
#define OSFATTR_SYSTEM  0x0002
#define OSFATTR_READ    0x0004
#define OSFATTR_WRITE   0x0008

/* Get a file's stat() type. */
int osfmode( const char* fname, int follow_links, unsigned long* mode,
             unsigned long* attr );

/* Disable the Tads swap file; computers have plenty of RAM these days.
 */
#define OS_DEFAULT_SWAP_ENABLED 0

/* TADS 2 macro/function configuration.  Modern configurations always
 * use the no-macro versions, so these definitions should always be set
 * as shown below. */
#define OS_MCM_NO_MACRO
#define ERR_NO_MACRO

/* These values are used for the "mode" parameter of osfseek() to
 * indicate where to seek in the file. */
#define OSFSK_SET SEEK_SET /* Set position relative to the start of the file. */
#define OSFSK_CUR SEEK_CUR /* Set position relative to the current file position. */
#define OSFSK_END SEEK_END /* Set position relative to the end of the file. */


/* ============= Functions follow ================ */

/* Emglken needs its own implementation as it doesn't have full file system support */
#ifdef EMGLKEN

#include "emglken.h"
#include "osemglken.h"

#else /* EMGLKEN */

/* File handle structure for osfxxx functions. */
typedef FILE osfildef;

/* Open text file for reading. */
#define osfoprt(fname,typ) (fopen((fname),"r"))

/* Open text file for writing. */
#define osfopwt(fname,typ) (fopen((fname),"w"))

/* Open text file for reading/writing.  If the file already exists,
 * truncate the existing contents.  Create a new file if it doesn't
 * already exist. */
#define osfoprwtt(fname,typ) (fopen((fname),"w+"))

/* Open binary file for writing. */
#define osfopwb(fname,typ) (fopen((fname),"wb"))

/* Open source file for reading - use the appropriate text or binary
 * mode. */
#define osfoprs osfoprt

/* Open binary file for reading. */
#define osfoprb(fname,typ) (fopen((fname),"rb"))

/* Open binary file for reading/writing.  If the file already exists,
 * truncate the existing contents.  Create a new file if it doesn't
 * already exist. */
#define osfoprwtb(fname,typ) (fopen((fname),"w+b"))

/* Get a line of text from a text file. */
#define osfgets fgets

/* Write a line of text to a text file. */
#define osfputs fputs

/* Write bytes to file. */
#define osfwb(fp,buf,bufl) (fwrite((buf),(bufl),1,(fp))!=1)

/* Flush buffered writes to a file. */
#define osfflush fflush

/* Read bytes from file. */
#define osfrb(fp,buf,bufl) (fread((buf),(bufl),1,(fp))!=1)

/* Read bytes from file and return the number of bytes read. */
#define osfrbc(fp,buf,bufl) (fread((buf),1,(bufl),(fp)))

/* Get the current seek location in the file. */
#define osfpos ftell

/* Seek to a location in the file. */
#define osfseek fseek

/* Close a file. */
#define osfcls fclose

/* Delete a file. */
#define osfdel remove

/* Access a file - determine if the file exists.
 *
 * We map this to the access() function.  It should be available in
 * virtually every system out there, as it appears in many standards
 * (SVID, AT&T, POSIX, X/OPEN, BSD 4.3, DOS, MS Windows, maybe more). */
#define osfacc(fname) (access((fname), F_OK))

/* Rename a file. */
#define os_rename_file(from, to) (rename(from, to) == 0)

/* Get a file's stat() type. */
struct os_file_stat_t;
int os_file_stat( const char* fname, int follow_links,
                  struct os_file_stat_t* s );

/* Get a character from a file. */
#define osfgetc fgetc

#endif /* EMGLKEN */

/* Open text file for reading and writing, keeping the file's existing
 * contents if the file already exists or creating a new file if no
 * such file exists. */
osfildef*
osfoprwt( const char* fname, os_filetype_t typ );

/* Open binary file for reading/writing.  If the file already exists,
 * keep the existing contents.  Create a new file if it doesn't already
 * exist. */
osfildef*
osfoprwb( const char* fname, os_filetype_t typ );

/* Allocate a block of memory of the given size in bytes. */
#define osmalloc malloc

/* Free memory previously allocated with osmalloc(). */
#define osfree free

/* Reallocate memory previously allocated with osmalloc() or osrealloc(),
 * changing the block's size to the given number of bytes. */
#define osrealloc realloc

/* Set busy cursor.
 *
 * We don't have a mouse cursor so there's no need to implement this. */
#define os_csr_busy(a)

/* Update progress display.
 *
 * We don't provide any kind of "compilation progress display", so we
 * just define this as an empty macro.
 */
#define os_progress(fname,linenum)

/* Initialize the time zone.
 *
 * We don't need this (I think). */
#define os_tzset()

#define TADS_OEM_NAME   "Mr Oizo"

/* Replace stricmp with strcasecmp */
#define strnicmp strncasecmp
#define stricmp strcasecmp

#define memicmp os_memicmp

int os_memicmp(const char *a, const char *b, int n);

#define OS_SYSTEM_NAME "GlkTADS"

#define OSFNMAX 1024

#ifdef _WIN32
#define OSPATHCHAR '\\'
#define OSPATHALT "/:"
#define OSPATHURL "\\/"
#define OSPATHSEP ';'
#define OS_NEWLINE_SEQ  "\r\n"
#else
#define OSPATHCHAR '/'
#define OSPATHALT ""
#define OSPATHURL "/"
#define OSPATHSEP ':'
#define OS_NEWLINE_SEQ "\n"
#endif

#define OSPATHPWD "."

void os_put_buffer (unsigned char *buf, size_t len);
void os_get_buffer (unsigned char *buf, size_t len, size_t init);
unsigned char *os_fill_buffer (unsigned char *buf, size_t len);

#define OS_MAXWIDTH 255

#define OS_ATTR_HILITE  OS_ATTR_BOLD
#define OS_ATTR_EM      OS_ATTR_ITALIC
#define OS_ATTR_STRONG  OS_ATTR_BOLD

#define OS_DECLARATIVE_TLS
#define OS_DECL_TLS(t, v) t v

int os_vasprintf(char **bufptr, const char *fmt, va_list ap);

// Support both the old garglk_fileref_get_name() and the new
// glkunix_fileref_get_filename() functions.
#if defined(GLK_MODULE_FILEREF_GET_NAME) && !defined(GLKUNIX_FILEREF_GET_FILENAME)
#define glkunix_fileref_get_filename garglk_fileref_get_name
#define GLKUNIX_FILEREF_GET_FILENAME
#endif

#ifdef __cplusplus
}
#endif

#endif /* OSGLK_H */

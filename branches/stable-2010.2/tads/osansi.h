/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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

#ifndef OSANSI_H
#define OSANSI_H

#define OSANSI	/* hmm? */
#define MAC_OS	/* tell tads not to let os do paging */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TRUE 1
#define FALSE 0

#ifdef __cplusplus
extern "C" {
#endif

#include "osbigmem.h"

/* ------------------------------------------------------------------------ */
/*
 * All the stuff osifc.h forgot to tell me about...
 */

#define TADS_OEM_NAME   "Mr Oizo"

/* Replace stricmp with strcasecmp */
#define strnicmp strncasecmp
#define stricmp strcasecmp

/* far pointer type qualifier (null on most platforms) */
#define osfar_t
#define far

/* cast an expression to void */
#define DISCARD (void)

/* ignore OS_LOADDS definitions */
#define OS_LOADDS

/* copy a structure - dst and src are structures, not pointers */
#define OSCPYSTRUCT(dst,src) ((dst) = (src))

#define memicmp os_memicmp

int os_memicmp(const char *a, const char *b, int n);

/* ------------------------------------------------------------------------ */
/*
 *   Platform Identifiers.  You must define the following macros in your
 */

#define OS_SYSTEM_NAME "Generic"

/*
 *   Message Linking Configuration.  You should #define ERR_LINK_MESSAGES
 */

#define ERR_LINK_MESSAGES

/*
 *   Program Exit Codes.  These values are used for the argument to exit()
 */

#define OSEXSUCC 0
#define OSEXFAIL 1

/*
 *   Basic memory management interface.  These functions are merely
 */

#define OSMALMAX 0xffffffff
#define osmalloc malloc
#define osfree free
#define osrealloc realloc


/*
 *   Basic file I/O interface.  These functions are merely documented
 */

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

#define OSFSK_SET 0
#define OSFSK_CUR 1
#define OSFSK_END 2
#define OSFOPRB 3

typedef FILE osfildef;

osfildef *osfoprt(const char *fname, os_filetype_t typ);
osfildef *osfopwt(const char *fname, os_filetype_t typ);
osfildef *osfoprwt(const char *fname, os_filetype_t typ);
osfildef *osfoprwtt(const char *fname, os_filetype_t typ);
osfildef *osfopwb(const char *fname, os_filetype_t typ);
osfildef *osfoprs(const char *fname, os_filetype_t typ);
osfildef *osfoprb(const char *fname, os_filetype_t typ);
osfildef *osfoprwb(const char *fname, os_filetype_t typ);
osfildef *osfoprwtb(const char *fname, os_filetype_t typ);

char *osfgets(char *buf, size_t len, osfildef *fp);
int osfputs(const char *buf, osfildef *fp);
void os_fprintz(osfildef *fp, const char *str);
void os_fprint(osfildef *fp, const char *str, size_t len);
int osfwb(osfildef *fp, const void *buf, int bufl);
int osfrb(osfildef *fp, void *buf, int bufl);
size_t osfrbc(osfildef *fp, void *buf, size_t bufl);
long osfpos(osfildef *fp);
int osfseek(osfildef *fp, long pos, int mode);
void osfflush(osfildef *fp);
void osfcls(osfildef *fp);
int osfdel(const char *fname);
int osfacc(const char *fname);
int osfgetc(osfildef *fp);

/* 
 *   Convert string to all-lowercase. 
 */
char *os_strlwr(char *s);

/*
 *   Character classifications for quote characters.  os_squote() returns
 */

#define os_squote(c) ((c) == '\'')
#define os_dquote(c) ((c) == '"')
#define os_qmatch(a, b) ((a) == (b))

/*
 *   OS_MAXWIDTH - the maximum width of a line of text.  Most platforms use
 */
#define OS_MAXWIDTH 255

#define OS_ATTR_HILITE  OS_ATTR_BOLD
#define OS_ATTR_EM      OS_ATTR_ITALIC
#define OS_ATTR_STRONG  OS_ATTR_BOLD

/*
 *   TADS 2 swapping configuration.  Define OS_DEFAULT_SWAP_ENABLED to 0
 */
#define OS_DEFAULT_SWAP_ENABLED   0

#ifdef __cplusplus
}
#endif

#endif /* OSANSI_H */


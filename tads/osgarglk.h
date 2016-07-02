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

#ifndef OSGARGLK_H
#define OSGARGLK_H

#define USE_OS_LINEWRAP	/* tell tads not to let os do paging */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#define TRUE 1
#define FALSE 0

#ifdef __cplusplus
extern "C" {
#endif

#include "osfrobtads.h"

#define TADS_OEM_NAME   "Mr Oizo"

/* Replace stricmp with strcasecmp */
#define strnicmp strncasecmp
#define stricmp strcasecmp

#define memicmp os_memicmp

int os_memicmp(const char *a, const char *b, int n);

#define OS_SYSTEM_NAME "Gargoyle"

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

#ifdef __cplusplus
}
#endif

#endif /* OSGARGLK_H */

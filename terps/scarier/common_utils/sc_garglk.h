/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SC_GARGOYLE_H
#define SC_GARGOYLE_H

/* strcasecmp() and strncasecmp() are POSIX, declared in <strings.h>.  MSVC
   has neither that header nor those names, only _stricmp() and _strnicmp() in
   <string.h>; MinGW, like every other toolchain, has <strings.h>.  So include
   this file rather than <strings.h>. */
#ifdef _MSC_VER
#include <string.h>
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#else
#include <strings.h>
#endif

#ifdef GARGLK_NEEDS_STRNDUP

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *strndup (const char *s, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* GARGLK_NEEDS_STRNDUP */

#endif

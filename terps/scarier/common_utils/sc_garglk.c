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

#include "sc_garglk.h"

typedef int sc_garglk_translation_unit_not_empty_;

#ifdef GARGLK_NEEDS_STRNDUP

#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n) {
    const char *end = memchr(s, '\0', n);
    size_t len = end ? (end - s) : n;

    char *copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, s, len);
    copy[len] = '\0';

    return copy;
}

#endif /* GARGLK_NEEDS_STRNDUP */

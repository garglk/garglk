/******************************************************************************
 *                                                                            *
 * Copyright (C) 2010-2011 by Ben Cressey.                                    *
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

/* osbuffer.c - handle text i/o */

#include "os.h"
#include "glk.h"

extern winid_t mainwin;

#ifndef GLK_UNICODE

void os_put_buffer (unsigned char *buf, size_t len)
{
    glk_put_buffer(buf, len);
}

void os_get_buffer (unsigned char *buf, size_t len, size_t init)
{
    glk_request_line_event(mainwin, buf, len - 1, init);
}

unsigned char *os_fill_buffer (unsigned char *buf, size_t len)
{
    buf[len] = '\0';
    return buf;
}

#else

static glui32 *input = 0;
static glui32 max = 0;

extern glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);

extern glui32 os_prepare_chars (glui32 *buf, glui32 buflen,
                                unsigned char *out, glui32 outlen);

void os_put_buffer (unsigned char *buf, size_t len)
{
    glui32 *out;
    glui32 outlen;

    if (!len)
        return;

    out = malloc(sizeof(glui32)*(len+1));
    if (!out)
        return;
    outlen = len;

    outlen = os_parse_chars(buf, len, out, outlen);

    if (outlen)
        glk_put_buffer_uni(out, outlen);
    else
        glk_put_buffer((char *)buf, len);

    free(out);
}

void os_get_buffer (unsigned char *buf, size_t len, size_t init)
{
    input = malloc(sizeof(glui32)*(len+1));
    max = len;

    if (init)
        os_parse_chars(buf, init + 1, input, len);

    glk_request_line_event_uni(mainwin, input, len - 1, init);
}

unsigned char *os_fill_buffer (unsigned char *buf, size_t len)
{
    glui32 res = os_prepare_chars(input, len, buf, max);
    buf[res] = '\0';

    free(input);
    input = 0;
    max = 0;

    return buf;
}

#endif /* GLK_UNICODE */

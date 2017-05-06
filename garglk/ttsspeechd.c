/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2017 by Chris Spiegel.                                       *
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

#include <stdlib.h>
#include <stdio.h>

#include <libspeechd.h>

#include "glk.h"
#include "garglk.h"

#define TXTSIZE 4096
static glui32 txtbuf[TXTSIZE + 1];
static size_t txtlen;

static SPDConnection *spd;

void gli_initialize_tts(void)
{
    if (gli_conf_speak)
    {
        spd = spd_open("gargoyle", "main", NULL, SPD_MODE_SINGLE);

        if (spd != NULL && gli_conf_speak_language != NULL)
            spd_set_language(spd, gli_conf_speak_language);
    }

    txtlen = 0;
}

static char *unicode_to_utf8(const glui32 *src, size_t n)
{
    char *dst, *dstp;

    dst = dstp = malloc((4 * n) + 1);

    for (size_t i = 0; i < n; i++)
    {
        uint8_t hi  = (src[i] >> 16) & 0xff,
                mid = (src[i] >>  8) & 0xff,
                lo  = (src[i]      ) & 0xff;

        if (src[i] < 0x80)
        {
            *dstp++ = src[i];
        }
        else if (src[i] < 0x800)
        {
            *dstp++ = 0xc0 | (mid << 2) | (lo >> 6);
            *dstp++ = 0x80 | (lo & 0x3f);
        }
        else if (src[i] < 0x10000)
        {
            *dstp++ = 0xe0 | (mid >> 4);
            *dstp++ = 0x80 | ((mid << 2) & 0x3f) | (lo >> 6);
            *dstp++ = 0x80 | (lo & 0x3f);
        }
        else if (src[i] < 0x200000)
        {
            *dstp++ = 0xf0 | (hi >> 2);
            *dstp++ = 0x80 | ((hi << 4) & 0x30) | (mid >> 4);
            *dstp++ = 0x80 | ((mid << 2) & 0x3c) | (lo >> 6);
            *dstp++ = 0x80 | (lo & 0x3f);
        }
    }

    *dstp = 0;

    return dst;
}

void gli_tts_flush(void)
{
    if (spd != NULL && txtlen > 0)
    {
        char *utf8 = unicode_to_utf8(txtbuf, txtlen);
        spd_say(spd, SPD_MESSAGE, utf8);
        free(utf8);
    }

    txtlen = 0;
}

void gli_tts_purge(void)
{
    if (spd != NULL)
        spd_cancel(spd);
}

void gli_tts_speak(const glui32 *buf, size_t len)
{
    if (spd == NULL)
        return;

    for (size_t i = 0; i < len; i++)
    {
        if (txtlen >= TXTSIZE)
            gli_tts_flush();

        if (buf[i] == '>' || buf[i] == '*')
            continue;

        txtbuf[txtlen++] = buf[i];

        if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || buf[i] == '\n')
            gli_tts_flush();
    }
}

void gli_free_tts(void)
{
    if (spd != NULL)
        spd_close(spd);
}

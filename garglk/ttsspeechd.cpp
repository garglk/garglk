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

#include <vector>

#include <libspeechd.h>

#include "glk.h"
#include "garglk.h"

static SPDConnection *spd;
static std::vector<glui32> txtbuf;

void gli_initialize_tts(void)
{
    if (gli_conf_speak)
    {
        spd = spd_open("gargoyle", "main", nullptr, SPD_MODE_SINGLE);

        if (spd != nullptr && !gli_conf_speak_language.empty())
            spd_set_language(spd, gli_conf_speak_language.c_str());
    }

    txtbuf.clear();
}

static std::string unicode_to_utf8(const std::vector<glui32> &src)
{
    std::string dst;

    for (const auto &c : src)
    {
        uint8_t hi  = (c >> 16) & 0xff,
                mid = (c >>  8) & 0xff,
                lo  = (c      ) & 0xff;

        if (c < 0x80)
        {
            dst.push_back(c);
        }
        else if (c < 0x800)
        {
            dst.push_back(0xc0 | (mid << 2) | (lo >> 6));
            dst.push_back(0x80 | (lo & 0x3f));
        }
        else if (c < 0x10000)
        {
            dst.push_back(0xe0 | (mid >> 4));
            dst.push_back(0x80 | ((mid << 2) & 0x3f) | (lo >> 6));
            dst.push_back(0x80 | (lo & 0x3f));
        }
        else if (c < 0x200000)
        {
            dst.push_back(0xf0 | (hi >> 2));
            dst.push_back(0x80 | ((hi << 4) & 0x30) | (mid >> 4));
            dst.push_back(0x80 | ((mid << 2) & 0x3c) | (lo >> 6));
            dst.push_back(0x80 | (lo & 0x3f));
        }
    }

    return dst;
}

void gli_tts_flush(void)
{
    if (spd != nullptr && !txtbuf.empty())
    {
        auto utf8 = unicode_to_utf8(txtbuf);
        spd_say(spd, SPD_MESSAGE, utf8.c_str());
    }

    txtbuf.clear();
}

void gli_tts_purge(void)
{
    if (spd != nullptr)
        spd_cancel(spd);
}

void gli_tts_speak(const glui32 *buf, size_t len)
{
    if (spd == nullptr)
        return;

    for (size_t i = 0; i < len; i++)
    {
        if (buf[i] == '>' || buf[i] == '*')
            continue;

        txtbuf.push_back(buf[i]);

        if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || buf[i] == '\n')
            gli_tts_flush();
    }
}

void gli_free_tts(void)
{
    if (spd != nullptr)
        spd_close(spd);
}

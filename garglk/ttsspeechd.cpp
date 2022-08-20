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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "glk.h"
#include "garglk.h"

#ifdef GARGLK_CONFIG_DLOPEN_LIBSPEECHD

#include <dlfcn.h>
#include <iostream>

/* Redefine some types from libspeechd.h */

typedef enum {
    SPD_IMPORTANT = 1,
    SPD_MESSAGE = 2,
    SPD_TEXT = 3,
    SPD_NOTIFICATION = 4,
    SPD_PROGRESS = 5
} SPDPriority;

typedef enum {
    SPD_MODE_SINGLE = 0,
    SPD_MODE_THREADED = 1
} SPDConnectionMode;

struct SPDConnection;

/* Prefix names of libspeechd symbols with underscores
 * in this file to avoid any possible conflicts */

#define spd_open _spd_open
#define spd_set_language _spd_set_language
#define spd_say _spd_say
#define spd_cancel _spd_cancel
#define spd_close _spd_close

#define LIBSPEECHD_FUNCS \
    FUNC( \
        spd_open, \
        SPDConnection*, \
        (const char*, const char*, const char*, SPDConnectionMode) \
    ) \
    FUNC(spd_set_language, int, (SPDConnection*, const char*)) \
    FUNC(spd_say, int, (SPDConnection*, SPDPriority, const char*)) \
    FUNC(spd_cancel, int, (SPDConnection*)) \
    FUNC(spd_close, void, (SPDConnection*))

#define FUNC(name, type, params)      \
    typedef type(*name##_fn) params; \
    static name##_fn name;

LIBSPEECHD_FUNCS

#undef FUNC

#else

#include <libspeechd.h>

#endif

static SPDConnection *spd;
static std::vector<glui32> txtbuf;

void gli_initialize_tts()
{
    if (gli_conf_speak)
    {
#ifdef GARGLK_CONFIG_DLOPEN_LIBSPEECHD
        void *libspeechd = dlopen("libspeechd.so.2", RTLD_LAZY | RTLD_LOCAL);

        if (libspeechd == nullptr)
        {
            std::cerr <<
                "Failed to load libspeechd. "
                "Text-to-speech will be disabled." <<
                std::endl;
            return;
        }

        /* Firefox checks for ABI compatibility by checking whether the
         * spd_get_volume symbol exists, so we follow their lead. */
        if (dlsym(libspeechd, "spd_get_volume") == nullptr)
        {
            std::cerr <<
                "An unsupported version of libspeechd was found. "
                "Text-to-speech will be disabled." <<
                std::endl;
            return;
        }

#define FUNC(name, type, params) \
    name = (name##_fn) dlsym(libspeechd, #name); \
    if (name == nullptr) \
    { \
        std::cerr << \
            "The symbol \"" << #name << "\" wasn't found in libspeechd. " \
            "Text-to-speech will be disabled." << \
            std::endl; \
        return; \
    }

        LIBSPEECHD_FUNCS

#undef FUNC
#endif

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
        if (c < 0x80)
        {
            dst.push_back(c);
        }
        else if (c < 0x800)
        {
            dst.push_back(0xc0 | ((c >> 6) & 0x1f));
            dst.push_back(0x80 | ((c     ) & 0x3f));
        }
        else if (c < 0x10000)
        {
            dst.push_back(0xe0 | ((c >> 12) & 0x0f));
            dst.push_back(0x80 | ((c >>  6) & 0x3f));
            dst.push_back(0x80 | ((c      ) & 0x3f));
        }
        else if (c < 0x200000)
        {
            dst.push_back(0xf0 | ((c >> 18) & 0x07));
            dst.push_back(0x80 | ((c >> 12) & 0x3f));
            dst.push_back(0x80 | ((c >>  6) & 0x3f));
            dst.push_back(0x80 | ((c      ) & 0x3f));
        }
    }

    return dst;
}

void gli_tts_flush()
{
    if (spd != nullptr && !txtbuf.empty())
    {
        auto utf8 = unicode_to_utf8(txtbuf);
        spd_say(spd, SPD_MESSAGE, utf8.c_str());
    }

    txtbuf.clear();
}

void gli_tts_purge()
{
    if (spd != nullptr)
        spd_cancel(spd);
}

void gli_tts_speak(const glui32 *buf, std::size_t len)
{
    if (spd == nullptr)
        return;

    for (std::size_t i = 0; i < len; i++)
    {
        if (buf[i] == '>' || buf[i] == '*')
            continue;

        txtbuf.push_back(buf[i]);

        if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || buf[i] == '\n')
            gli_tts_flush();
    }
}

void gli_free_tts()
{
    if (spd != nullptr)
        spd_close(spd);
}

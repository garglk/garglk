// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2017 by Chris Spiegel.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <array>
#include <cstddef>

#include <windows.h>
#include <sapi.h>

#include "glk.h"
#include "garglk.h"

static ISpVoice *voice = nullptr;
#define TXTSIZE 4096
static std::array<WCHAR, TXTSIZE + 1> txtbuf;
static std::size_t txtlen;

void gli_initialize_tts()
{
    if (gli_conf_speak) {
        if (CoInitialize(nullptr) == S_OK) {
            CoCreateInstance(
                    CLSID_SpVoice,		// rclsid
                    nullptr,			// aggregate
                    CLSCTX_ALL,			// dwClsContext
                    IID_ISpVoice,		// riid
                    reinterpret_cast<void**>(&voice));
        }
    } else {
        voice = nullptr;
    }

    txtlen = 0;
}

void gli_tts_flush()
{
    if (voice != nullptr) {
        txtbuf[txtlen] = 0;
        voice->Speak(txtbuf.data(), SPF_ASYNC | SPF_IS_NOT_XML, nullptr);
    }

    txtlen = 0;
}

void gli_tts_purge()
{
    if (voice != nullptr) {
        voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    }
}

void gli_tts_speak(const glui32 *buf, std::size_t len)
{
    if (voice == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < len; i++) {
        if (txtlen >= TXTSIZE) {
            gli_tts_flush();
        }

        if (buf[i] == '>' || buf[i] == '*') {
            continue;
        }

        if (buf[i] < 0x10000) {
            txtbuf[txtlen++] = buf[i];
        }

        if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || buf[i] == '\n') {
            gli_tts_flush();
        }
    }
}

void gli_free_tts()
{
    if (voice != nullptr) {
        voice->Release();
        CoUninitialize();
    }
}

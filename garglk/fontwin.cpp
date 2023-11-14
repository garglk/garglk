// Copyright (C) 2010 by Ben Cressey.
// Copyright (C) 2021 by Chris Spiegel
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

#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "format.h"
#include "optional.hpp"

#include "font.h"
#include "glk.h"
#include "garglk.h"

#define FONT_SUBKEY ("Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

// forward declaration
static bool find_font_file(const std::string &facename, std::string &filepath);

static HDC hdc;

static std::unique_ptr<FontFiller> filler;

static int CALLBACK font_cb(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *, int, LPARAM)
{
    std::string filepath;
    std::string style = reinterpret_cast<char *>(lpelfe->elfStyle);

    if (!find_font_file(reinterpret_cast<char *>(lpelfe->elfFullName), filepath)) {
        return 1;
    }

    if (style == "Regular" || style == "Roman") {
        filler->add(FontFiller::Style::Regular, filepath);
    } else if (style == "Bold") {
        filler->add(FontFiller::Style::Bold, filepath);
    } else if (style == "Italic" || style == "Oblique") {
        filler->add(FontFiller::Style::Italic, filepath);
    } else if (style == "Bold Italic" || style == "Bold Oblique" || style == "BoldItalic" || style == "BoldOblique") {
        filler->add(FontFiller::Style::BoldItalic, filepath);
    }

    return 0;
}

static std::string make_font_filepath(const std::string &filename)
{
    // create the absolute path to the font file
    if (filename.find(':') == std::string::npos && std::getenv("SYSTEMROOT") != nullptr) {
        return Format("{}\\Fonts\\{}", std::getenv("SYSTEMROOT"), filename);
    } else {
        return filename;
    }
}

static bool find_font_file_with_key(HKEY key, const char *subkey, const std::string &facename, std::string &filepath)
{
    HKEY hkey;
    DWORD size;
    std::string face;
    char filename[256];

    // open the Fonts registry key
    if (RegOpenKeyExA(key, subkey, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS) {
        return false;
    }

    // check for a TrueType font
    face = Format("{} (TrueType)", facename);

    size = sizeof(filename);

    if (RegQueryValueExA(hkey, face.c_str(), nullptr, nullptr, (PBYTE)filename, &size) == ERROR_SUCCESS) {
        filepath = make_font_filepath(filename);
        RegCloseKey(hkey);
        return true;
    }

    // check for an OpenType font
    face = Format("{} (OpenType)", facename);

    size = sizeof(filename);

    if (RegQueryValueExA(hkey, face.c_str(), nullptr, nullptr, (PBYTE)filename, &size) == ERROR_SUCCESS) {
        filepath = make_font_filepath(filename);
        RegCloseKey(hkey);
        return true;
    }

    // TODO: support TrueType/OpenType collections (TTC/OTC)

    RegCloseKey(hkey);

    return false;
}

static bool find_font_file(const std::string &facename, std::string &filepath)
{
    // First, try the per-user key
    if (find_font_file_with_key(HKEY_CURRENT_USER, FONT_SUBKEY, facename, filepath)) {
        return true;
    }

    // Nope. Try the system-wide key.
    return find_font_file_with_key(HKEY_LOCAL_MACHINE, FONT_SUBKEY, facename, filepath);
}

void garglk::fontreplace(const std::string &font, FontType type)
{
    if (font.empty()) {
        return;
    }

    LOGFONTA logfont;

    ZeroMemory(&logfont, sizeof logfont);
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfPitchAndFamily = FF_DONTCARE;

    hdc = GetDC(0);

    filler = std::make_unique<FontFiller>(type);

    std::snprintf(logfont.lfFaceName, LF_FACESIZE, "%s", font.c_str());
    EnumFontFamiliesExA(hdc, &logfont, reinterpret_cast<FONTENUMPROCA>(font_cb), 0, 0);

    filler->fill();
    filler.reset();

    ReleaseDC(0, hdc);
}

void fontload()
{
}

void fontunload()
{
}

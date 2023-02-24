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

#include <windows.h>

#include <cstdio>

#include "optional.hpp"

#include "glk.h"
#include "garglk.h"

#define FONT_SUBKEY ("Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

// forward declaration
static bool find_font_file(const std::string &facename, std::string &filepath);

static HDC hdc;

struct Fonts {
    nonstd::optional<std::string> r, b, i, z;
} monofonts, propfonts;

static void fill_in_fonts(Fonts &fonts, std::string &r, std::string &b, std::string &i, std::string &z)
{
#define FILL(font) do { if (fonts.font.has_value()) font = *fonts.font; } while(0);
    FILL(r);
    FILL(b);
    FILL(i);
    FILL(z);
#undef FILL
}

static int CALLBACK cb_handler(ENUMLOGFONTEX *lpelfe, Fonts *fonts)
{
    std::string filepath;
    std::string style = reinterpret_cast<char *>(lpelfe->elfStyle);

    *fonts = Fonts();

    if (!find_font_file(reinterpret_cast<char *>(lpelfe->elfFullName), filepath)) {
        return 1;
    }

    if (!fonts->r.has_value() && (style == "Regular" || style == "Roman")) {
        fonts->r = filepath;

        if (!fonts->b.has_value()) {
            fonts->b = filepath;
        }

        if (!fonts->i.has_value()) {
            fonts->i = filepath;
        }

        if (!fonts->z.has_value() && !fonts->i.has_value() && !fonts->b.has_value()) {
            fonts->z = filepath;
        }
    }

    else if (!fonts->b.has_value() && style == "Bold") {
        fonts->b = filepath;

        if (!fonts->z.has_value() && !fonts->i.has_value()) {
            fonts->z = filepath;
        }
    }

    else if (!fonts->i.has_value() && (style == "Italic" || style == "Oblique")) {
        fonts->i = filepath;

        if (!fonts->z.has_value()) {
            fonts->z = filepath;
        }
    }

    else if (!fonts->z.has_value() && (style == "Bold Italic" || style == "Bold Oblique" || style == "BoldOblique" || style == "BoldItalic")) {
        fonts->z = filepath;
    }

    return 0;
}

static int CALLBACK monofont_cb(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, int FontType, LPARAM lParam)
{
    return cb_handler(lpelfe, &monofonts);
}

static int CALLBACK propfont_cb(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, int FontType, LPARAM lParam)
{
    return cb_handler(lpelfe, &propfonts);
}

static std::string make_font_filepath(const std::string &filename)
{
    // create the absolute path to the font file
    if (filename.find(':') == std::string::npos && std::getenv("SYSTEMROOT") != nullptr) {
        return std::string(std::getenv("SYSTEMROOT")) + "\\Fonts\\" + filename;
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
    if (RegOpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS) {
        return false;
    }

    // check for a TrueType font
    face = facename + " (TrueType)";

    size = sizeof(filename);

    if (RegQueryValueEx(hkey, face.c_str(), NULL, NULL, (PBYTE)filename, &size) == ERROR_SUCCESS) {
        filepath = make_font_filepath(filename);
        RegCloseKey(hkey);
        return true;
    }

    // check for an OpenType font
    face = facename + " (OpenType)";

    size = sizeof(filename);

    if (RegQueryValueEx(hkey, face.c_str(), NULL, NULL, (PBYTE)filename, &size) == ERROR_SUCCESS) {
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

    LOGFONT logfont;

    ZeroMemory(&logfont, sizeof logfont);
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfPitchAndFamily = FF_DONTCARE;

    hdc = GetDC(0);

    switch (type) {
    case FontType::Monospace:
        std::snprintf(logfont.lfFaceName, LF_FACESIZE, "%s", font.c_str());
        EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)monofont_cb, 0, 0);
        if (monofonts.r.has_value()) {
            fill_in_fonts(monofonts, gli_conf_mono.r, gli_conf_mono.b, gli_conf_mono.i, gli_conf_mono.z);
        }
        break;
    case FontType::Proportional:
        std::snprintf(logfont.lfFaceName, LF_FACESIZE, "%s", font.c_str());
        EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)propfont_cb, 0, 0);
        if (propfonts.r.has_value()) {
            fill_in_fonts(propfonts, gli_conf_prop.r, gli_conf_prop.b, gli_conf_prop.i, gli_conf_prop.z);
        }
        break;
    }

    ReleaseDC(0, hdc);
}

void fontload()
{
}

void fontunload()
{
}

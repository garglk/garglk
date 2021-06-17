/******************************************************************************
 *                                                                            *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 * Copyright (C) 2021 by Chris Spiegel                                        *
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

#include <windows.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

#define FONT_SUBKEY ("Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

// forward declaration
static bool find_font_file(const char *facename, char *filepath, size_t length);

static HDC hdc;

struct fonts
{
    const char *name;
    bool rset, bset, iset, zset;
    char *r, *b, *i, *z;
} monofonts, propfonts;

static void fill_in_fonts(struct fonts *fonts, char **r, char **b, char **i, char **z)
{
#define FILL(font) do { if (fonts->font != NULL) *(font) = fonts->font; } while(0);
    FILL(r);
    FILL(b);
    FILL(i);
    FILL(z);
#undef FILL
}

static int CALLBACK cb_handler(ENUMLOGFONTEX *lpelfe, struct fonts *fonts)
{
    char filepath[1024];

    if (fonts->name[0] == 0)
        return 0;

    if (!find_font_file(lpelfe->elfFullName, filepath, sizeof filepath))
        return 1;

    char *file = strdup(filepath);

    if (!fonts->rset &&
            (strcmp(lpelfe->elfStyle, "Regular") == 0 ||
             strcmp(lpelfe->elfStyle, "Roman") == 0))
    {
        fonts->r = file;
        fonts->rset = true;

        if (!fonts->bset)
            fonts->b = file;

        if (!fonts->iset)
            fonts->i = file;

        if (!fonts->zset && !fonts->iset && !fonts->bset)
            fonts->z = file;
    }

    else if (!fonts->bset && strcmp(lpelfe->elfStyle, "Bold") == 0)
    {
        fonts->b = file;
        fonts->bset = true;

        if (!fonts->zset && !fonts->iset)
            fonts->z = file;
    }

    else if (!fonts->iset &&
            (strcmp(lpelfe->elfStyle, "Italic") == 0 ||
             strcmp(lpelfe->elfStyle, "Oblique") == 0))
    {
        fonts->i = file;
        fonts->iset = true;

        if (!fonts->zset)
            fonts->z = file;
    }

    else if (!fonts->zset &&
            (strcmp(lpelfe->elfStyle, "Bold Italic") == 0 ||
             strcmp(lpelfe->elfStyle, "Bold Oblique") == 0 ||
             strcmp(lpelfe->elfStyle, "BoldOblique") == 0 ||
             strcmp(lpelfe->elfStyle, "BoldItalic") == 0))
    {
        fonts->z = file;
        fonts->zset = true;
    }

    else
    {
        free(file);
    }

    return 0;
}

static int CALLBACK monofont_cb(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, int FontType, LPARAM lParam)
{
    monofonts = (struct fonts){ .name = gli_conf_monofont };
    return cb_handler(lpelfe, &monofonts);
}

static int CALLBACK propfont_cb(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, int FontType, LPARAM lParam)
{
    propfonts = (struct fonts){ .name = gli_conf_propfont };
    return cb_handler(lpelfe, &propfonts);
}

static void make_font_filepath(const char *filename, char *filepath, size_t length)
{
    // create the absolute path to the font file
    filepath[0] = '\0';
    if (!strstr(filename, ":") && getenv("SYSTEMROOT"))
    {
        snprintf(filepath, length, "%s\\Fonts\\%s", getenv("SYSTEMROOT"), filename);
    }
    else
    {
        snprintf(filepath, length, "%s", filename);
    }
}

static bool find_font_file_with_key(HKEY key, const char *subkey, const char *facename, char *filepath, size_t length)
{
    HKEY hkey;
    DWORD size;
    char face[256];
    char filename[256];

    // open the Fonts registry key
    if (RegOpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
        return false;

    // check for a TrueType font
    snprintf(face, sizeof face, "%s (TrueType)", facename);

    size = sizeof(filename);

    if (RegQueryValueEx(hkey, face, NULL, NULL, (PBYTE)filename, &size) == ERROR_SUCCESS)
    {
        make_font_filepath(filename, filepath, length);
        RegCloseKey(hkey);
        return true;
    }

    // check for an OpenType font
    snprintf(face, sizeof face, "%s (OpenType)", facename);

    size = sizeof(filename);

    if (RegQueryValueEx(hkey, face, NULL, NULL, (PBYTE)filename, &size) == ERROR_SUCCESS)
    {
        make_font_filepath(filename, filepath, length);
        RegCloseKey(hkey);
        return true;
    }

    // TODO: support TrueType/OpenType collections (TTC/OTC)

    RegCloseKey(hkey);

    return false;
}

static bool find_font_file(const char *facename, char *filepath, size_t length)
{
    // First, try the per-user key
    if (find_font_file_with_key(HKEY_CURRENT_USER, FONT_SUBKEY, facename, filepath, length))
        return true;

    // Nope. Try the system-wide key.
    return find_font_file_with_key(HKEY_LOCAL_MACHINE, FONT_SUBKEY, facename, filepath, length);
}

void fontreplace(char *font, int type)
{
    if (!strlen(font))
        return;

    LOGFONT logfont;

    ZeroMemory(&logfont, sizeof logfont);
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfPitchAndFamily = FF_DONTCARE;

    hdc = GetDC(0);

    switch (type)
    {
    case MONOF:
        snprintf(logfont.lfFaceName, LF_FACESIZE, "%s", gli_conf_monofont);
        EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)monofont_cb, 0, 0);
        fill_in_fonts(&monofonts, &gli_conf_monor, &gli_conf_monob, &gli_conf_monoi, &gli_conf_monoz);
        break;
    case PROPF:
        snprintf(logfont.lfFaceName, LF_FACESIZE, "%s", gli_conf_propfont);
        EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)propfont_cb, 0, 0);
        fill_in_fonts(&propfonts, &gli_conf_propr, &gli_conf_propb, &gli_conf_propi, &gli_conf_propz);
        break;
    }

    ReleaseDC(0, hdc);
}

void fontload(void)
{
}

void fontunload(void)
{
}

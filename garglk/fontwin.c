/******************************************************************************
 *                                                                            *
 * Copyright (C) 2010 by Ben Cressey.                                         *
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

#include <stdio.h>

#include "glk.h"
#include "garglk.h"

#define FONT_SUBKEY ("Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts")

// forward declaration
static int find_font_file(const char *facename, char *filepath, size_t length);

static HDC hdc;

static int gli_sys_monor = FALSE;
static int gli_sys_monob = FALSE;
static int gli_sys_monoi = FALSE;
static int gli_sys_monoz = FALSE;

int CALLBACK monofont(
    ENUMLOGFONTEX    *lpelfe,   /* pointer to logical-font data */
    NEWTEXTMETRICEX  *lpntme,   /* pointer to physical-font data */
    int              FontType,  /* type of font */
    LPARAM           lParam     /* a combo box HWND */
    )
{
    char filepath[1024];

    if (!(strlen(gli_conf_monofont)))
        return 0;

    if (!find_font_file(lpelfe->elfFullName, filepath, sizeof filepath))
        return 1;

    char * file = malloc(strlen(filepath)+1);
    strcpy(file, filepath);

    if (!gli_sys_monor && (!(strcmp(lpelfe->elfStyle,"Regular"))
                || !(strcmp(lpelfe->elfStyle,"Roman"))))
    {
        gli_conf_monor = file;

        if (!gli_sys_monob)
            gli_conf_monob = file;

        if (!gli_sys_monoi)
            gli_conf_monoi = file;

        if (!gli_sys_monoz && !gli_sys_monoi && !gli_sys_monob)
            gli_conf_monoz = file;

        gli_sys_monor = TRUE;
    }

    else if (!gli_sys_monob && (!(strcmp(lpelfe->elfStyle,"Bold"))))
    {
        gli_conf_monob = file;

        if (!gli_sys_monoz && !gli_sys_monoi)
            gli_conf_monoz = file;

        gli_sys_monob = TRUE;
    }

    else if (!gli_sys_monoi && (!(strcmp(lpelfe->elfStyle,"Italic"))
                || !(strcmp(lpelfe->elfStyle,"Oblique"))))
    {
        gli_conf_monoi = file;

        if (!gli_sys_monoz)
            gli_conf_monoz = file;

        gli_sys_monoi = TRUE;
    }

    else if (!gli_sys_monoz && (!(strcmp(lpelfe->elfStyle,"Bold Italic"))
                || !(strcmp(lpelfe->elfStyle,"Bold Oblique"))
                || !(strcmp(lpelfe->elfStyle,"BoldOblique"))
                || !(strcmp(lpelfe->elfStyle,"BoldItalic"))
                ))
    {
        gli_conf_monoz = file;
        gli_sys_monoz = TRUE;
    }

    else
    {
        free(file);
    }

    return 1;
}

static int gli_sys_propr = FALSE;
static int gli_sys_propb = FALSE;
static int gli_sys_propi = FALSE;
static int gli_sys_propz = FALSE;

int CALLBACK propfont(
    ENUMLOGFONTEX    *lpelfe,   /* pointer to logical-font data */
    NEWTEXTMETRICEX  *lpntme,   /* pointer to physical-font data */
    int              FontType,  /* type of font */
    LPARAM           lParam     /* a combo box HWND */
    )
{
    char filepath[1024];

    if (!(strlen(gli_conf_propfont)))
        return 0;

    if (!find_font_file(lpelfe->elfFullName, filepath, sizeof filepath))
        return 1;

    char * file = malloc(strlen(filepath)+1);
    strcpy(file, filepath);

    if (!gli_sys_propr && (!(strcmp(lpelfe->elfStyle,"Regular"))
                || !(strcmp(lpelfe->elfStyle,"Roman"))))
    {
        gli_conf_propr = file;

        if (!gli_sys_propb)
            gli_conf_propb = file;

        if (!gli_sys_propi)
            gli_conf_propi = file;

        if (!gli_sys_propz && !gli_sys_propi && !gli_sys_propb)
            gli_conf_propz = file;

        gli_sys_propr = TRUE;
    }

    else if (!gli_sys_propb && (!(strcmp(lpelfe->elfStyle,"Bold"))))
    {
        gli_conf_propb = file;

        if (!gli_sys_propz && !gli_sys_propi)
            gli_conf_propz = file;

        gli_sys_propb = TRUE;
    }

    else if (!gli_sys_propi && (!(strcmp(lpelfe->elfStyle,"Italic"))
                || !(strcmp(lpelfe->elfStyle,"Oblique"))))
    {
        gli_conf_propi = file;

        if (!gli_sys_propz)
            gli_conf_propz = file;

        gli_sys_propi = TRUE;
    }

    else if (!gli_sys_propz && (!(strcmp(lpelfe->elfStyle,"Bold Italic"))
                || !(strcmp(lpelfe->elfStyle,"Bold Oblique"))
                || !(strcmp(lpelfe->elfStyle,"BoldOblique"))
                || !(strcmp(lpelfe->elfStyle,"BoldItalic"))
                ))
    {
        gli_conf_propz = file;
        gli_sys_propz = TRUE;
    }

    else
    {
        free(file);
    }

    return 1;
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

static int find_font_file_with_key(HKEY key, const char * subkey, const char *facename, char *filepath, size_t length)
{
    HKEY hkey;
    DWORD size;
    char face[256];
    char filename[256];

    // open the Fonts registry key
    if (RegOpenKeyEx(key, subkey, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
        return FALSE;

    // check for a TrueType font
    snprintf(face, sizeof face, "%s (TrueType)", facename);

    size = sizeof(filename);

    if (RegQueryValueEx(hkey, face, NULL, NULL, (PBYTE)filename, &size) == ERROR_SUCCESS)
    {
        make_font_filepath(filename, filepath, length);
        RegCloseKey(hkey);
        return TRUE;
    }

    // check for an OpenType font
    snprintf(face, sizeof face, "%s (OpenType)", facename);

    size = sizeof(filename);

    if (RegQueryValueEx(hkey, face, NULL, NULL, (PBYTE)filename, &size) == ERROR_SUCCESS)
    {
        make_font_filepath(filename, filepath, length);
        RegCloseKey(hkey);
        return TRUE;
    }

    // TODO: support TrueType/OpenType collections (TTC/OTC)

    RegCloseKey(hkey);

    return FALSE;
}

static int find_font_file(const char *facename, char *filepath, size_t length)
{
    // First, try the per-user key
    if (find_font_file_with_key(HKEY_CURRENT_USER, FONT_SUBKEY, facename, filepath, length))
        return TRUE;

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
        lstrcpy(logfont.lfFaceName, gli_conf_monofont);
        EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)monofont, 0, 0);
        break;

    case PROPF:
        lstrcpy(logfont.lfFaceName, gli_conf_propfont);
        EnumFontFamiliesEx(hdc, &logfont, (FONTENUMPROC)propfont, 0, 0);
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

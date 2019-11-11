/******************************************************************************
 *                                                                            *
 * Copyright (C) 2010 by Ben Cressey, Sylvain Beucler, Chris Spiegel.         *
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

#include <fontconfig/fontconfig.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

static bool findfont(const char *fontname, char *fontpath, size_t n)
{
    FcPattern *p = NULL;
    FcChar8 *strval = NULL;
    FcObjectSet *attr = NULL;
    FcFontSet *fs = NULL;
    bool success = false;

    if (!FcInit())
        return false;

    p = FcNameParse((FcChar8*)fontname);
    if (p == NULL)
        goto out;

    attr = FcObjectSetBuild (FC_FILE, (char *)NULL);
    fs = FcFontList (NULL, p, attr);
    if (fs->nfont == 0)
        goto out;

    if (FcPatternGetString(fs->fonts[0], FC_FILE, 0, &strval) == FcResultTypeMismatch
                || strval == NULL)
        goto out;

    success = true;
    snprintf(fontpath, n, "%s", strval);

out:
    if (fs != NULL)
        FcFontSetDestroy(fs);

    if (attr != NULL)
        FcObjectSetDestroy(attr);

    if (p != NULL)
        FcPatternDestroy(p);

    FcFini();

    return success;
}

#ifdef __GNUC__
__attribute__((__sentinel__))
#endif
static char *find_font_by_styles(const char *basefont, ...)
{
    va_list ap;
    char *font = NULL;

    va_start(ap, basefont);

    for (const char *style = va_arg(ap, const char *); style != NULL; style = va_arg(ap, const char *))
    {
        char fontname[1024];
        char fontpath[1024];

        snprintf(fontname, sizeof fontname, "%s:style=%s", basefont, style);
        if (findfont(fontname, fontpath, sizeof fontpath))
        {
            font = strdup(fontpath);
            break;
        }
    }

    va_end(ap);

    return font;
}

void fontreplace(char *font, int type)
{
    if (strlen(font) == 0)
        return;

    char *sysfont;
    char **r, **b, **i, **z;

    if (type == MONOF)
    {
        r = &gli_conf_monor;
        b = &gli_conf_monob;
        i = &gli_conf_monoi;
        z = &gli_conf_monoz;
    }
    else
    {
        r = &gli_conf_propr;
        b = &gli_conf_propb;
        i = &gli_conf_propi;
        z = &gli_conf_propz;
    }

    /* regular or roman */
    sysfont = find_font_by_styles(font, "Regular", "Roman", "Book", (char *)NULL);
    if (sysfont != NULL)
    {
        *r = sysfont;
        *b = sysfont;
        *i = sysfont;
        *z = sysfont;
    }

    /* bold */
    sysfont = find_font_by_styles(font, "Bold", (char *)NULL);
    if (sysfont != NULL)
    {
        *b = sysfont;
        *z = sysfont;
    }

    /* italic or oblique */
    sysfont = find_font_by_styles(font, "Italic", "Oblique", (char *)NULL);
    if (sysfont != NULL)
    {
        *i = sysfont;
        *z = sysfont;
    }

    /* bold italic or bold oblique */
    sysfont = find_font_by_styles(font, "BoldItalic", "Bold Italic", "BoldOblique", "Bold Oblique", (char *)NULL);
    if (sysfont != NULL)
    {
        *z = sysfont;
    }
}

void fontload(void)
{
}

void fontunload(void)
{
}

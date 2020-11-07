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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

static bool initialized = false;

static bool findfont(const char *fontname, char *fontpath, size_t n)
{
    FcPattern *p = NULL;
    FcChar8 *strval = NULL;
    FcObjectSet *attr = NULL;
    FcFontSet *fs = NULL;
    bool success = false;

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

    return success;
}

static char *find_font_by_styles(const char *basefont, const char **styles, const char **weights, const char **slants)
{
    // Prefer normal width fonts, but if there aren't any, allow whatever fontconfig finds.
    const char *widths[] = {":normal", "", NULL};

    for (const char **width = widths; *width != NULL; width++)
    {
        for (const char **style = styles; *style != NULL; style++)
        {
            for (const char **weight = weights; *weight != NULL; weight++)
            {
                for (const char **slant = slants; *slant != NULL; slant++)
                {
                    char fontname[1024];
                    char fontpath[1024];

                    snprintf(fontname, sizeof fontname, "%s:style=%s:%s:%s%s", basefont, *style, *weight, *slant, *width);
                    if (findfont(fontname, fontpath, sizeof fontpath))
                    {
                        return strdup(fontpath);
                    }
                }
            }
        }
    }

    return NULL;
}

void fontreplace(char *font, int type)
{
    if (!initialized || strlen(font) == 0)
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

    /* Although there are 4 "main" types of font (Regular, Italic, Bold, Bold
     * Italic), there are actually a whole lot more possibilities, and,
     * unfortunately, some fonts are inconsistent in what they report about
     * their attributes.
     *
     * It's possible to ask fontconfig for a particular style (e.g. "Regular"),
     * but that's not enough. The font Noto Serif, for example, has a font that
     * lists its style as both "Condensed ExtraLight" and "Regular". If
     * "style=Regular" were used to find regular fonts, this font would show up,
     * which is not desirable. So in addition to the style, the weight (an
     * integer) must also be requested.
     *
     * Going the other direction, Adobe Garamond has a font with the style
     * "Regular Expert", the expert character set being totally unusable in
     * Gargoyle. However, its integral weight is still regular. So asking just
     * for a regular weight isn't sufficient: the style needs to be taken into
     * account as well.
     *
     * In short, for each style, try various combinations of style name, weight,
     * and slant in hopes of getting the desired font. These are ordered from
     * highest to lowest priority, so "Italic regular italic" will be preferred
     * over "Medium Oblique book oblique", for example.
     */
    const char *regular_styles[] = {"Regular", "Book", "Medium", "Roman", NULL};
    const char *italic_styles[] = {"Italic", "Regular Italic", "Medium Italic", "Book Italic", "Oblique", "Regular Oblique", "Medium Oblique", "Book Oblique", NULL};
    const char *bold_styles[] = {"Bold", "Extrabold", "Semibold", "Black", NULL};
    const char *bold_italic_styles[] = {"Bold Italic", "Extrabold Italic", "Semibold Italic", "Black Italic", "Bold Oblique", "Extrabold Oblique", "Semibold Oblique", "Black Oblique", NULL};

    const char *regular_weights[] = {"regular", "book", "medium", NULL};
    const char *bold_weights[] = {"bold", "extrabold", "semibold", "black", "medium", NULL};

    const char *roman_slants[] = {"roman", NULL};
    const char *italic_slants[] = {"italic", "oblique", NULL};

    /* regular or roman */
    sysfont = find_font_by_styles(font, regular_styles, regular_weights, roman_slants);
    if (sysfont != NULL)
    {
        *r = sysfont;
        *b = sysfont;
        *i = sysfont;
        *z = sysfont;
    }

    /* bold */
    sysfont = find_font_by_styles(font, bold_styles, bold_weights, roman_slants);
    if (sysfont != NULL)
    {
        *b = sysfont;
        *z = sysfont;
    }

    /* italic or oblique */
    sysfont = find_font_by_styles(font, italic_styles, regular_weights, italic_slants);
    if (sysfont != NULL)
    {
        *i = sysfont;
        *z = sysfont;
    }

    /* bold italic or bold oblique */
    sysfont = find_font_by_styles(font, bold_italic_styles, bold_weights, italic_slants);
    if (sysfont != NULL)
    {
        *z = sysfont;
    }
}

void fontload(void)
{
    initialized = FcInit();
}

void fontunload(void)
{
    if (initialized)
        FcFini();
}

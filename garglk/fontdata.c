/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2008, 2009  Sylvain Beucler                                  *
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BUNDLED_FONTS

/* include hex-dumped font files */
#include "lmr.hex"
#include "lmb.hex"
#include "lmi.hex"
#include "lmz.hex"
#include "cbr.hex"
#include "cbb.hex"
#include "cbi.hex"
#include "cbz.hex"

void gli_get_builtin_font(int idx, unsigned char **ptr, unsigned int *len)
{
    switch (idx)
    {
    case 0:
        *ptr = LuxiMonoRegular_pfb;
        *len = LuxiMonoRegular_pfb_len;
        break;
    case 1:
        *ptr = LuxiMonoBold_pfb;
        *len = LuxiMonoBold_pfb_len;
        break;
    case 2:
        *ptr = LuxiMonoOblique_pfb;
        *len = LuxiMonoOblique_pfb_len;
        break;
    case 3:
        *ptr = LuxiMonoBoldOblique_pfb;
        *len = LuxiMonoBoldOblique_pfb_len;
        break;

    case 4:
        *ptr = CharterBT_Roman_ttf;
        *len = CharterBT_Roman_ttf_len;
        break;
    case 5:
        *ptr = CharterBT_Bold_ttf;
        *len = CharterBT_Bold_ttf_len;
        break;
    case 6:
        *ptr = CharterBT_Italic_ttf;
        *len = CharterBT_Italic_ttf_len;
        break;
    case 7:
        *ptr = CharterBT_BoldItalic_ttf;
        *len = CharterBT_BoldItalic_ttf_len;
        break;

    default:
        *ptr = 0;
        len = 0;
    }
}

#else /* no BUNDLED_FONTS */

/* If you are not bundling fonts with your Gargoyle package,
   you should provide substitutes for the two typefaces which
   were previously included by default. The choice is left up
   to individual discretion, but matching the general look of
   Luxi Mono and Bitstream Charter is a good idea. This should
   be something fontconfig can find in the system fonts.*/

void gli_get_builtin_font(int idx, unsigned char **ptr, unsigned int *len)
{
    switch (idx)
    {
    case 0:
        gli_get_system_font("Luxi Mono:style=Regular", ptr, len);
        break;
    case 1:
        gli_get_system_font("Luxi Mono:style=Bold", ptr, len);
        break;
    case 2:
        gli_get_system_font("Luxi Mono:style=Oblique", ptr, len);
        break;
    case 3:
        gli_get_system_font("Luxi Mono:style=Bold Oblique", ptr, len);
        break;

    case 4:
        gli_get_system_font("Bitstream Charter:style=Roman", ptr, len);
        break;
    case 5:
        gli_get_system_font("Bitstream Charter:style=Bold", ptr, len);
        break;
    case 6:
        gli_get_system_font("Bitstream Charter:style=Italic", ptr, len);
        break;
    case 7:
        gli_get_system_font("Bitstream Charter:style=BoldItalic", ptr, len);
        break;

    default:
        *ptr = 0;
        len = 0;
    }
}

#endif /* BUNDLED_FONTS */

/* Get font data for 'fontname'. This can either be a valid fontconfig
   string or a partial filename to match. If the font is found, it
   will be loaded into memory. Otherwise the program will terminate. */

int gli_get_system_font(char* fontname, unsigned char **ptr, unsigned int *len)
{
    *ptr = 0;
    *len = 0;
    FcPattern *p = NULL;
    FcChar8 *strval = NULL;
    FcObjectSet *attr = NULL;
    int i, ilen;
    int font_match = 255;
    int font_idx = 0;

    if (!FcInit())
    {
        winabort("Internal error: cannot initialize fontconfig");
        return 0;
    }

    /* Grab filename attribute */
    attr = FcObjectSetBuild (FC_FILE, (char *) 0);

    p = FcNameParse((FcChar8*)fontname);
    if (p == NULL)
    {
        winabort("Internal error: invalid font pattern: %s", fontname);
        return 0;
    }
    FcFontSet *fs = FcFontList (0, p, attr);

    if (fs->nfont == 0)
    {
        /* Try using the fontname as a filename instead */
        FcPatternDestroy(p);
        p = FcPatternCreate();
        if (!FcPatternAddInteger(p, FC_INDEX, 0)) {
            winabort("internal error: invalid font pattern: %s", fontname);
            return 0;
        }
        FcFontSetDestroy(fs);
        fs = FcFontList(0, p, attr);
        if (fs->nfont == 0) {
            winabort("gli_get_system_font: no matching font for %s", fontname);
            return 0;
        } else {
            for (i = 0; i < fs->nfont; i++) {
                if (FcPatternGetString(fs->fonts[i], FC_FILE, 0, &strval) == FcResultMatch
                        && strval != NULL && strstr((char *)strval, fontname)) {
                    /* Match the shortest filename that contains the supplied font name*/
                    ilen = strlen(strstr((char *)strval, fontname));
                    if (ilen < font_match) {
                        font_match = ilen;
                        font_idx = i;
                    }
                }
            }
             if (!font_idx) {
                winabort("gli_get_system_font: no matching file for %s", fontname);
                return 0;
             }
        }
    }

    if (FcPatternGetString(fs->fonts[font_idx], FC_FILE, 0, &strval) == FcResultTypeMismatch
        || strval == NULL)
    {
        winabort("get_fontconfig_path: cannot find font filename for %s", fontname);
        return 0;
    }

    FILE* fp = fopen(strval, "rb");
    fseek(fp, 0L, SEEK_END);
    *len = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    *ptr = malloc(*len);
    fread(*ptr, 1, *len, fp);
    fclose(fp);

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(attr);
    FcPatternDestroy(p);
    FcFini();

    return 1;
}

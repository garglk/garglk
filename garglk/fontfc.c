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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

static void findfont(char *fontname, char *fontpath)
{
    FcPattern *p = NULL;
    FcChar8 *strval = NULL;
    FcObjectSet *attr = NULL;

    if (!FcInit())
        return;

    attr = FcObjectSetBuild (FC_FILE, (char *) 0);

    p = FcNameParse((FcChar8*)fontname);
    if (p == NULL)
        return;

    FcFontSet *fs = FcFontList (0, p, attr);
    if (fs->nfont == 0)
        return;

    if (FcPatternGetString(fs->fonts[0], FC_FILE, 0, &strval) == FcResultTypeMismatch
                || strval == NULL)
        return;

    strcpy(fontpath, strval);

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(attr);
    FcPatternDestroy(p);
    FcFini();

    return;
}

void fontreplace(char *font, int type)
{
    if (!strlen(font))
        return;

    char fontname[256];
    char fontpath[1024];
    char *sysfont;

    switch (type)
    {
        case MONOF:
            /* regular or roman */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=Regular");
            findfont(fontname, fontpath);

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Roman");
                findfont(fontname, fontpath);
            }

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Book");
                findfont(fontname, fontpath);
            }

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_monor = sysfont;
                gli_conf_monob = sysfont;
                gli_conf_monoi = sysfont;
                gli_conf_monoz = sysfont;
            }

            /* bold */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=Bold");
            findfont(fontname, fontpath);

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_monob = sysfont;
                gli_conf_monoz = sysfont;

            }

            /* italic or oblique */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=Italic");
            findfont(fontname, fontpath);

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Oblique");
                findfont(fontname, fontpath);
            }

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_monoi = sysfont;
                gli_conf_monoz = sysfont;
            }

            /* bold italic or bold oblique */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=BoldItalic");
            findfont(fontname, fontpath);

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Bold Italic");
                findfont(fontname, fontpath);
            }

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=BoldOblique");
                findfont(fontname, fontpath);
            }

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Bold Oblique");
                findfont(fontname, fontpath);
            }

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_monoz = sysfont;
            }

            return;

        case PROPF:
            /* regular or roman */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=Regular");
            findfont(fontname, fontpath);

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Roman");
                findfont(fontname, fontpath);
            }

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Book");
                findfont(fontname, fontpath);
            }

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_propr = sysfont;
                gli_conf_propb = sysfont;
                gli_conf_propi = sysfont;
                gli_conf_propz = sysfont;
            }

            /* bold */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=Bold");
            findfont(fontname, fontpath);

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_propb = sysfont;
                gli_conf_propz = sysfont;

            }

            /* italic or oblique */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=Italic");
            findfont(fontname, fontpath);

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Oblique");
                findfont(fontname, fontpath);
            }

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_propi = sysfont;
                gli_conf_propz = sysfont;
            }

            /* bold italic or bold oblique */
            fontpath[0] = '\0';
            strcpy(fontname, font);
            strcat(fontname, ":style=BoldItalic");
            findfont(fontname, fontpath);

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Bold Italic");
                findfont(fontname, fontpath);
            }

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=BoldOblique");
                findfont(fontname, fontpath);
            }

            if (!strlen(fontpath))
            {
                strcpy(fontname, font);
                strcat(fontname, ":style=Bold Oblique");
                findfont(fontname, fontpath);
            }

            if (strlen(fontpath))
            {
                sysfont = malloc(strlen(fontpath)+1);
                strcpy(sysfont, fontpath);
                gli_conf_propz = sysfont;
            }

            return;
    }
}

void fontload(void)
{
}

void fontunload(void)
{
}

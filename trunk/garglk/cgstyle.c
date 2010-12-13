/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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

#include <stdio.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

static int isprop(int f)
{
    return f == PROPR || f == PROPI || f == PROPB || f == PROPZ;
}

static int isbold(int f)
{
    return f == PROPB || f == PROPZ || f == MONOB || f == MONOZ;
}

static int isitalic(int f)
{
    return f == PROPI || f == PROPZ || f == MONOI || f == MONOZ;
}

static int makefont(int p, int b, int i)
{
    if ( p && !b && !i) return PROPR;
    if ( p && !b &&  i) return PROPI;
    if ( p &&  b && !i) return PROPB;
    if ( p &&  b &&  i) return PROPZ;
    if (!p && !b && !i) return MONOR;
    if (!p && !b &&  i) return MONOI;
    if (!p &&  b && !i) return MONOB;
    if (!p &&  b &&  i) return MONOZ;
    return PROPR;
}

void glk_stylehint_set(glui32 wintype, glui32 style, glui32 hint, glsi32 val)
{
    style_t *styles;
    int p, b, i;

    if (wintype == wintype_AllTypes)
    {
        glk_stylehint_set(wintype_TextGrid, style, hint, val);
        glk_stylehint_set(wintype_TextBuffer, style, hint, val);
        return;
    }

    if (wintype == wintype_TextGrid)
        styles = gli_gstyles;
    else if (wintype == wintype_TextBuffer)
        styles = gli_tstyles;
    else
        return;

    if (!gli_conf_stylehint)
        return;

    switch (hint)
    {
        case stylehint_TextColor:
            styles[style].fg[0] = (val >> 16) & 0xff;
            styles[style].fg[1] = (val >> 8) & 0xff;
            styles[style].fg[2] = (val) & 0xff;
            break;

        case stylehint_BackColor:
            styles[style].bg[0] = (val >> 16) & 0xff;
            styles[style].bg[1] = (val >> 8) & 0xff;
            styles[style].bg[2] = (val) & 0xff;
            break;

        case stylehint_ReverseColor:
            styles[style].reverse = (val != 0);
            break;

        case stylehint_Proportional:
            if (wintype == wintype_TextBuffer)
            {
                p = val > 0;
                b = isbold(styles[style].font);
                i = isitalic(styles[style].font);
                styles[style].font = makefont(p, b, i);
            }
            break;

        case stylehint_Weight:
            p = isprop(styles[style].font);
            b = val > 0;
            i = isitalic(styles[style].font);
            styles[style].font = makefont(p, b, i);
            break;

        case stylehint_Oblique:
            p = isprop(styles[style].font);
            b = isbold(styles[style].font);
            i = val > 0;
            styles[style].font = makefont(p, b, i);
            break;
    }

    if (wintype == wintype_TextBuffer &&
            style == style_Normal &&
            hint == stylehint_BackColor)
    {
        memcpy(gli_window_color, styles[style].bg, 3);
    }

    if (wintype == wintype_TextBuffer &&
            style == style_Normal &&
            hint == stylehint_TextColor)
    {
        memcpy(gli_more_color, styles[style].fg, 3);
        memcpy(gli_caret_color, styles[style].fg, 3);
    }
}

void glk_stylehint_clear(glui32 wintype, glui32 style, glui32 hint)
{
    style_t *styles;
    style_t *defaults;

    if (wintype == wintype_AllTypes)
    {
        glk_stylehint_clear(wintype_TextGrid, style, hint);
        glk_stylehint_clear(wintype_TextBuffer, style, hint);
        return;
    }

    if (wintype == wintype_TextGrid)
    {
        styles = gli_gstyles;
        defaults = gli_gstyles_def;
    }
    else if (wintype == wintype_TextBuffer)
    {
        styles = gli_tstyles;
        defaults = gli_tstyles_def;
    }
    else
    {
        return;
    }

    if (!gli_conf_stylehint)
        return;

    switch (hint)
    {
        case stylehint_TextColor:
            styles[style].fg[0] = defaults[style].fg[0];
            styles[style].fg[1] = defaults[style].fg[1];
            styles[style].fg[2] = defaults[style].fg[2];
            break;

        case stylehint_BackColor:
            styles[style].bg[0] = defaults[style].bg[0];
            styles[style].bg[1] = defaults[style].bg[1];
            styles[style].bg[2] = defaults[style].bg[2];
            break;

        case stylehint_ReverseColor:
            styles[style].reverse = defaults[style].reverse;
            break;

        case stylehint_Proportional:
        case stylehint_Weight:
        case stylehint_Oblique:
            styles[style].font = defaults[style].font;
            break;
    }
}

glui32 glk_style_distinguish(winid_t win, glui32 styl1, glui32 styl2)
{
    if (win->type == wintype_TextGrid)
    {
        window_textgrid_t *dwin = win->data;
        return memcmp(dwin->styles+styl1, dwin->styles+styl2, sizeof(style_t));
    }
    if (win->type == wintype_TextBuffer)
    {
        window_textbuffer_t *dwin = win->data;
        return memcmp(dwin->styles+styl1, dwin->styles+styl2, sizeof(style_t));
    }
    return 0;
}

glui32 glk_style_measure(winid_t win, glui32 style, glui32 hint, glui32 *result)
{
    style_t *styles;

    if (win->type == wintype_TextGrid)
        styles = ((window_textgrid_t*)win->data)->styles;
    else if (win->type == wintype_TextBuffer)
        styles = ((window_textbuffer_t*)win->data)->styles;
    else
        return FALSE;

    switch (hint)
    {
        case stylehint_Indentation:
        case stylehint_ParaIndentation:
            *result = 0;
            return TRUE;

        case stylehint_Justification:
            *result = stylehint_just_LeftFlush;
            return TRUE;

        case stylehint_Size:
            *result = 1;
            return TRUE;

        case stylehint_Weight:
            *result =
                (styles[style].font == PROPB ||
                 styles[style].font == PROPZ ||
                 styles[style].font == MONOB ||
                 styles[style].font == MONOZ);
            return TRUE;

        case stylehint_Oblique:
            *result =
                (styles[style].font == PROPI ||
                 styles[style].font == PROPZ ||
                 styles[style].font == MONOI ||
                 styles[style].font == MONOZ);
            return TRUE;

        case stylehint_Proportional:
            *result =
                (styles[style].font == PROPR ||
                 styles[style].font == PROPI ||
                 styles[style].font == PROPB ||
                 styles[style].font == PROPZ);
            return TRUE;

        case stylehint_TextColor:
            *result =
                (styles[style].fg[0] << 16) |
                (styles[style].fg[1] << 8) |
                (styles[style].fg[2]);
            return TRUE;

        case stylehint_BackColor:
            *result =
                (styles[style].bg[0] << 16) |
                (styles[style].bg[1] << 8) |
                (styles[style].bg[2]);
            return TRUE;

        case stylehint_ReverseColor:
            *result = styles[style].reverse;
            return TRUE;
    }

    return FALSE;
}

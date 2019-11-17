/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Andrew Plotkin.                  *
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

/* cggestal.c: Gestalt functions for Glk API, version 0.7.0.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright 1998-2007 by Andrew Plotkin.
    You may copy, distribute, and incorporate it into your own programs,
    by any means and under any conditions, as long as you do not modify it.
    You may also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr,
    glui32 arrlen)
{
    switch (id)
    {
        case gestalt_Version:
            return 0x00000703;

        case gestalt_LineInput:
            if (val >= 32 && val < 0x10ffff)
                return TRUE;
            else
                return FALSE;

        case gestalt_CharInput:
            if (val >= 32 && val < 0x10ffff)
                return TRUE;
            else if (val == keycode_Return)
                return TRUE;
            else
                return FALSE;

        case gestalt_CharOutput:
            if (val >= 32 && val < 0x10ffff)
            {
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_ExactPrint;
            }
            else
            {
                /* cheaply, we don't do any translation of printed
                    characters, so the output is always one character
                    even if it's wrong. */
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_CannotPrint;
            }

        case gestalt_MouseInput:
            if (val == wintype_TextGrid)
                return TRUE;
            if (val == wintype_Graphics)
                return TRUE;
            return FALSE;

        case gestalt_Timer:
            return TRUE;

        case gestalt_Graphics:
        case gestalt_GraphicsTransparency:
            return gli_conf_graphics;
        case gestalt_DrawImage:
            if (val == wintype_TextBuffer)
                return gli_conf_graphics;
            if (val == wintype_Graphics)
                return gli_conf_graphics;
            return FALSE;

        case gestalt_Sound:
        case gestalt_SoundVolume:
        case gestalt_SoundMusic:
        case gestalt_SoundNotify:
            return gli_conf_sound;

        case gestalt_Sound2:
            return gli_conf_sound;

        case gestalt_Unicode:
            return TRUE;
        case gestalt_UnicodeNorm:
            return TRUE;

        case gestalt_Hyperlinks:
            return TRUE;
        case gestalt_HyperlinkInput:
            return TRUE;

        case gestalt_LineInputEcho:
            return TRUE;
        case gestalt_LineTerminators:
            return TRUE;
        case gestalt_LineTerminatorKey:
            return gli_window_check_terminator(val);

        case gestalt_DateTime:
            return TRUE;

        case gestalt_GarglkText:
            return TRUE;

        default:
            return 0;

    }
}

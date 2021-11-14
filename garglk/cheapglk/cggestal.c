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

/* cggestal.c: Gestalt functions for Glk API, version 0.7.5.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright 1998-2016 by Andrew Plotkin.
    It is distributed under the MIT license; see the "LICENSE" file.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

#ifdef GARGLK
#define TRUE true
#define FALSE false
#endif

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr, 
    glui32 arrlen)
{
    switch (id) {
        
        case gestalt_Version:
            /* This implements Glk spec version 0.7.5. */
            return 0x00000705;
        
        case gestalt_LineInput:
#ifdef GARGLK
            if (val >= 32 && val < 0x10ffff)
#else
            if (val >= 32 && val < 127)
#endif
                return TRUE;
            else
                return FALSE;
                
        case gestalt_CharInput: 
            if (val >= 32 && val < 127)
                return TRUE;
            else if (val == keycode_Return)
                return TRUE;
            else {
                /* If we're doing UTF-8 input, we can input any Unicode
                   character. Except control characters. */
                if (gli_utf8input) {
                    if (val >= 160 && val < 0x200000)
                        return TRUE;
                }
                /* If not, we can't accept anything non-ASCII */
                return FALSE;
            }
        
        case gestalt_CharOutput: 
            if (val >= 32 && val < 127) {
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_ExactPrint;
            }
            else {
                /* cheaply, we don't do any translation of printed
                    characters, so the output is always one character 
                    even if it's wrong. */
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                /* If we're doing UTF-8 output, we can print any Unicode
                   character. Except control characters. */
                if (gli_utf8output) {
                    if (val >= 160 && val < 0x200000)
                        return gestalt_CharOutput_ExactPrint;
                }
                return gestalt_CharOutput_CannotPrint;
            }
            
        case gestalt_MouseInput: 
#ifdef GARGLK
            if (val == wintype_TextGrid)
                return TRUE;
            if (val == wintype_Graphics)
                return TRUE;
#endif
            return FALSE;
            
        case gestalt_Timer: 
#ifdef GARGLK
            return TRUE;
#else
            return FALSE;
#endif

        case gestalt_Graphics:
        case gestalt_GraphicsTransparency:
#ifdef GARGLK
            return gli_conf_graphics;
#endif
        case gestalt_GraphicsCharInput:
            return FALSE;
            
        case gestalt_DrawImage:
#ifdef GARGLK
            if (val == wintype_TextBuffer)
                return gli_conf_graphics;
            if (val == wintype_Graphics)
                return gli_conf_graphics;
#endif
            return FALSE;
            
        case gestalt_Unicode:
#ifdef GLK_MODULE_UNICODE
            return TRUE;
#else
            return FALSE;
#endif /* GLK_MODULE_UNICODE */
            
        case gestalt_UnicodeNorm:
#ifdef GLK_MODULE_UNICODE_NORM
            return TRUE;
#else
            return FALSE;
#endif /* GLK_MODULE_UNICODE_NORM */
            
        case gestalt_Sound:
        case gestalt_SoundVolume:
        case gestalt_SoundNotify: 
        case gestalt_SoundMusic:
#ifdef GARGLK
            return gli_conf_sound;
#else
            return FALSE;
#endif
        case gestalt_Sound2: 
#ifdef GARGLK
            return gli_conf_sound;
#else
            /* Sound2 implies all the above sound options. But for
               cheapglk, they're all false. */
            return FALSE;
#endif

        case gestalt_LineInputEcho:
#ifdef GARGLK
            return TRUE;
#else
            return FALSE;
#endif

        case gestalt_LineTerminators:
#ifdef GARGLK
            return TRUE;
#endif
        case gestalt_LineTerminatorKey:
#ifdef GARGLK
            return gli_window_check_terminator(val);
#else
            return FALSE;
#endif

        case gestalt_DateTime:
            return TRUE;

        case gestalt_ResourceStream:
#ifdef GARGLK
            return TRUE;
#else
            return FALSE;
#endif

#ifdef GARGLK
        case gestalt_Hyperlinks:
            return TRUE;
        case gestalt_HyperlinkInput:
            return TRUE;

        case gestalt_GarglkText:
            return TRUE;
#endif

        default:
            return 0;

    }
}


/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2008-2009 by Sylvain Beucler.                                *
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
#ifdef BUNDLED_FONTS

#include <stdio.h>

#include "glk.h"
#include "garglk.h"

/* include hex-dumped font files */
#include "Go-Mono.hex"
#include "Go-Mono-Bold.hex"
#include "Go-Mono-Italic.hex"
#include "Go-Mono-Bold-Italic.hex"
#include "NotoSerif-Regular.hex"
#include "NotoSerif-Bold.hex"
#include "NotoSerif-Italic.hex"
#include "NotoSerif-BoldItalic.hex"

void gli_get_builtin_font(int idx, const unsigned char **ptr, unsigned int *len)
{
    switch (idx)
    {
        case 0:
            *ptr = Go_Mono_ttf;
            *len = Go_Mono_ttf_len;
            break;
        case 1:
            *ptr = Go_Mono_Bold_ttf;
            *len = Go_Mono_Bold_ttf_len;
            break;
        case 2:
            *ptr = Go_Mono_Italic_ttf;
            *len = Go_Mono_Italic_ttf_len;
            break;
        case 3:
            *ptr = Go_Mono_Bold_Italic_ttf;
            *len = Go_Mono_Bold_Italic_ttf_len;
            break;

        case 4:
            *ptr = NotoSerif_Regular_ttf;
            *len = NotoSerif_Regular_ttf_len;
            break;
        case 5:
            *ptr = NotoSerif_Bold_ttf;
            *len = NotoSerif_Bold_ttf_len;
            break;
        case 6:
            *ptr = NotoSerif_Italic_ttf;
            *len = NotoSerif_Italic_ttf_len;
            break;
        case 7:
            *ptr = NotoSerif_BoldItalic_ttf;
            *len = NotoSerif_BoldItalic_ttf_len;
            break;

        default:
            *ptr = 0;
            *len = 0;
    }
}

#endif

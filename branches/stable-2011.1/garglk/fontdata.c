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

#endif

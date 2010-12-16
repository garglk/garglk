/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Andrew Plotkin, Jesse McGrew.                   *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
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

/* osparse.c -- convert between character sets as needed */

#include "os.h"
#include "glk.h"

#ifdef GLK_UNICODE

enum CHARMAPS { OS_UTF8, OS_CP1251, OS_CP1252, OS_MACROMAN, OS_UNKNOWN };

static glui32 os_charmap = OS_UTF8;

glui32 os_parse_chars (unsigned char *buf, glui32 buflen,
                       glui32 *out, glui32 outlen);

glui32 is_cyrillic(unsigned char ch)
{
    if (ch >= 0xBC)
        return 1;

    switch (ch)
    {
        case 0x80:
        case 0x81:
        case 0x83:
        case 0x8A:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        case 0x90:
        case 0x9A:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA5:
        case 0xA8:
        case 0xAA:
        case 0xAF:
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB8:
        case 0xBA:
            return 1;

        default:
            return 0;
    }
}

glui32 is_macroman (unsigned char ch)
{
    switch (ch)
    {
        /* trademarks */
        case 0xA8:
        case 0xAA:
            return 1;

        /* dashes and right quotes */
        case 0xD0:
        case 0xD1:
        case 0xD3:
        case 0xD5:
            return 1;

        /* accents */
        case 0x8E:
        case 0x8F:
        case 0x9A:
            return 1;

        default:
            return 0;
    }
}

glui32 is_cp1252 (unsigned char ch)
{
    switch (ch)
    {
        /* trademarks */
        case 0x99:
        case 0xAE:
            return 1;

        /* dashes and right quotes */
        case 0x92:
        case 0x94:
        case 0x96:
        case 0x97:
            return 1;

        /* accents */
        case 0xE8:
        case 0xE9:
        case 0xF6:
            return 1;

        default:
            return 0;
    }
}

glui32 identify_chars (unsigned char *buf, glui32 buflen,
                       glui32 *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 val = 0;
    glui32 count_macroman = 0;
    glui32 count_cp1252 = 0;
    glui32 wordlen = 0;
    glui32 cyrilen = 0;
    glui32 charmap = OS_UNKNOWN;

    while (pos < buflen)
    {
        val = buf[pos++];

        count_macroman += is_macroman(val);
        count_cp1252 += is_cp1252(val);

        if (val != 0x20)
        {
            wordlen++;
            cyrilen += is_cyrillic(val);
        }
        else
        {
            if (wordlen == cyrilen)
            {
                charmap = OS_CP1251;
                break;
            }
            wordlen = 0;
            cyrilen = 0;
        }
    }

    if (charmap == OS_CP1251)
        os_charmap = OS_CP1251;
    else if (count_cp1252 >= count_macroman)
        os_charmap = OS_CP1252;
    else
        os_charmap = OS_MACROMAN;

    return os_parse_chars(buf, buflen, out, outlen);
}

glui32 parse_utf8(unsigned char *buf, glui32 buflen,
                     glui32 *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;
    glui32 val0, val1, val2, val3;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        val0 = buf[pos++];

        if (val0 < 0x80)
        {
            res = val0;
            out[outpos++] = res;
            continue;
        }

        if ((val0 & 0xe0) == 0xc0)
        {
            if (pos+1 > buflen)
            {
                //printf("incomplete two-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            val1 = buf[pos++];
            if ((val1 & 0xc0) != 0x80)
            {
                //printf("malformed two-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            res = (val0 & 0x1f) << 6;
            res |= (val1 & 0x3f);
            out[outpos++] = res;
            continue;
        }

        if ((val0 & 0xf0) == 0xe0)
        {
            if (pos+2 > buflen)
            {
                //printf("incomplete three-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            val1 = buf[pos++];
            val2 = buf[pos++];
            if ((val1 & 0xc0) != 0x80)
            {
                //printf("malformed three-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            if ((val2 & 0xc0) != 0x80)
            {
                //printf("malformed three-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            res = (((val0 & 0xf)<<12)  & 0x0000f000);
            res |= (((val1 & 0x3f)<<6) & 0x00000fc0);
            res |= (((val2 & 0x3f))    & 0x0000003f);
            out[outpos++] = res;
            continue;
        }

        if ((val0 & 0xf0) == 0xf0)
        {
            if ((val0 & 0xf8) != 0xf0)
            {
                //printf("malformed four-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            if (pos+3 > buflen)
            {
                //printf("incomplete four-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            val1 = buf[pos++];
            val2 = buf[pos++];
            val3 = buf[pos++];
            if ((val1 & 0xc0) != 0x80)
            {
                //printf("malformed four-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            if ((val2 & 0xc0) != 0x80)
            {
                //printf("malformed four-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            if ((val3 & 0xc0) != 0x80)
            {
                //printf("malformed four-byte character\n");
                return identify_chars(buf, buflen, out, outlen);
            }
            res = (((val0 & 0x7)<<18)   & 0x1c0000);
            res |= (((val1 & 0x3f)<<12) & 0x03f000);
            res |= (((val2 & 0x3f)<<6)  & 0x000fc0);
            res |= (((val3 & 0x3f))     & 0x00003f);
            out[outpos++] = res;
            continue;
        }

        //printf("malformed character\n");
        return identify_chars(buf, buflen, out, outlen);
    }

    return outpos;
}

glui32 prepare_utf8(glui32 *buf, glui32 buflen,
                    unsigned char *out, glui32 outlen)
{
    glui32 i=0, k=0;

    /*convert UTF-32 to UTF-8 */
    while (i < buflen && k < outlen)
    {
        if ((buf[i] < 0x80))
        {
            out[k] = buf[i];
            k++;
        }
        else if ((buf[i] < 0x800) && (k < outlen - 1))
        {
            out[k  ] = (0xC0 | ((buf[i] & 0x7C0) >> 6));
            out[k+1] = (0x80 |  (buf[i] & 0x03F)      );
            k = k + 2;
        }
        else if ((buf[i] < 0x10000) && (k < outlen - 2))
        {
            out[k  ] = (0xE0 | ((buf[i] & 0xF000) >> 12));
            out[k+1] = (0x80 | ((buf[i] & 0x0FC0) >>  6));
            out[k+2] = (0x80 |  (buf[i] & 0x003F)       );
            k = k + 3;
        }
        else if ((buf[i] < 0x200000) && (k < outlen - 3))
        {
            out[k  ] = (0xF0 | ((buf[i] & 0x1C0000) >> 18));
            out[k+1] = (0x80 | ((buf[i] & 0x03F000) >> 12));
            out[k+2] = (0x80 | ((buf[i] & 0x000FC0) >>  6));
            out[k+3] = (0x80 |  (buf[i] & 0x00003F)       );
            k = k + 4;
        }
        else
        {
            out[k] = '?';
            k++;
        }
        i++;
    }

    return k;
}

/* http://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1251.TXT */
glui32 parse_cp1251(unsigned char *buf, glui32 buflen,
                     glui32 *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        res = buf[pos++];

        if (res < 0x80)
        {
            out[outpos++] = res;
            continue;
        }

        switch (res)
        {
            case 0x80: out[outpos++] = 0x0402; break;
            case 0x81: out[outpos++] = 0x0403; break;
            case 0x82: out[outpos++] = 0x201A; break;
            case 0x83: out[outpos++] = 0x0453; break;
            case 0x84: out[outpos++] = 0x201E; break;
            case 0x85: out[outpos++] = 0x2026; break;
            case 0x86: out[outpos++] = 0x2020; break;
            case 0x87: out[outpos++] = 0x2021; break;
            case 0x88: out[outpos++] = 0x20AC; break;
            case 0x89: out[outpos++] = 0x2030; break;
            case 0x8A: out[outpos++] = 0x0409; break;
            case 0x8B: out[outpos++] = 0x2039; break;
            case 0x8C: out[outpos++] = 0x040A; break;
            case 0x8D: out[outpos++] = 0x040C; break;
            case 0x8E: out[outpos++] = 0x040B; break;
            case 0x8F: out[outpos++] = 0x040F; break;

            case 0x90: out[outpos++] = 0x0452; break;
            case 0x91: out[outpos++] = 0x2018; break;
            case 0x92: out[outpos++] = 0x2019; break;
            case 0x93: out[outpos++] = 0x201C; break;
            case 0x94: out[outpos++] = 0x201D; break;
            case 0x95: out[outpos++] = 0x2022; break;
            case 0x96: out[outpos++] = 0x2013; break;
            case 0x97: out[outpos++] = 0x2014; break;
            case 0x99: out[outpos++] = 0x2122; break;
            case 0x9A: out[outpos++] = 0x0459; break;
            case 0x9B: out[outpos++] = 0x203A; break;
            case 0x9C: out[outpos++] = 0x045A; break;
            case 0x9D: out[outpos++] = 0x045C; break;
            case 0x9E: out[outpos++] = 0x045B; break;
            case 0x9F: out[outpos++] = 0x045F; break;

            case 0xA0: out[outpos++] = 0x00A0; break;
            case 0xA1: out[outpos++] = 0x040E; break;
            case 0xA2: out[outpos++] = 0x045E; break;
            case 0xA3: out[outpos++] = 0x0408; break;
            case 0xA4: out[outpos++] = 0x00A4; break;
            case 0xA5: out[outpos++] = 0x0490; break;
            case 0xA6: out[outpos++] = 0x00A6; break;
            case 0xA7: out[outpos++] = 0x00A7; break;
            case 0xA8: out[outpos++] = 0x0401; break;
            case 0xA9: out[outpos++] = 0x00A9; break;
            case 0xAA: out[outpos++] = 0x0404; break;
            case 0xAB: out[outpos++] = 0x00AB; break;
            case 0xAC: out[outpos++] = 0x00AC; break;
            case 0xAD: out[outpos++] = 0x00AD; break;
            case 0xAE: out[outpos++] = 0x00AE; break;
            case 0xAF: out[outpos++] = 0x0407; break;

            case 0xB0: out[outpos++] = 0x00B0; break;
            case 0xB1: out[outpos++] = 0x00B1; break;
            case 0xB2: out[outpos++] = 0x0406; break;
            case 0xB3: out[outpos++] = 0x0456; break;
            case 0xB4: out[outpos++] = 0x0491; break;
            case 0xB5: out[outpos++] = 0x00B5; break;
            case 0xB6: out[outpos++] = 0x00B6; break;
            case 0xB7: out[outpos++] = 0x00B7; break;
            case 0xB8: out[outpos++] = 0x0451; break;
            case 0xB9: out[outpos++] = 0x2116; break;
            case 0xBA: out[outpos++] = 0x0454; break;
            case 0xBB: out[outpos++] = 0x00BB; break;
            case 0xBC: out[outpos++] = 0x0458; break;
            case 0xBD: out[outpos++] = 0x0405; break;
            case 0xBE: out[outpos++] = 0x0455; break;
            case 0xBF: out[outpos++] = 0x0457; break;

            case 0xC0: out[outpos++] = 0x0410; break;
            case 0xC1: out[outpos++] = 0x0411; break;
            case 0xC2: out[outpos++] = 0x0412; break;
            case 0xC3: out[outpos++] = 0x0413; break;
            case 0xC4: out[outpos++] = 0x0414; break;
            case 0xC5: out[outpos++] = 0x0415; break;
            case 0xC6: out[outpos++] = 0x0416; break;
            case 0xC7: out[outpos++] = 0x0417; break;
            case 0xC8: out[outpos++] = 0x0418; break;
            case 0xC9: out[outpos++] = 0x0419; break;
            case 0xCA: out[outpos++] = 0x041A; break;
            case 0xCB: out[outpos++] = 0x041B; break;
            case 0xCC: out[outpos++] = 0x041C; break;
            case 0xCD: out[outpos++] = 0x041D; break;
            case 0xCE: out[outpos++] = 0x041E; break;
            case 0xCF: out[outpos++] = 0x041F; break;

            case 0xD0: out[outpos++] = 0x0420; break;
            case 0xD1: out[outpos++] = 0x0421; break;
            case 0xD2: out[outpos++] = 0x0422; break;
            case 0xD3: out[outpos++] = 0x0423; break;
            case 0xD4: out[outpos++] = 0x0424; break;
            case 0xD5: out[outpos++] = 0x0425; break;
            case 0xD6: out[outpos++] = 0x0426; break;
            case 0xD7: out[outpos++] = 0x0427; break;
            case 0xD8: out[outpos++] = 0x0428; break;
            case 0xD9: out[outpos++] = 0x0429; break;
            case 0xDA: out[outpos++] = 0x042A; break;
            case 0xDB: out[outpos++] = 0x042B; break;
            case 0xDC: out[outpos++] = 0x042C; break;
            case 0xDD: out[outpos++] = 0x042D; break;
            case 0xDE: out[outpos++] = 0x042E; break;
            case 0xDF: out[outpos++] = 0x042F; break;

            case 0xE0: out[outpos++] = 0x0430; break;
            case 0xE1: out[outpos++] = 0x0431; break;
            case 0xE2: out[outpos++] = 0x0432; break;
            case 0xE3: out[outpos++] = 0x0433; break;
            case 0xE4: out[outpos++] = 0x0434; break;
            case 0xE5: out[outpos++] = 0x0435; break;
            case 0xE6: out[outpos++] = 0x0436; break;
            case 0xE7: out[outpos++] = 0x0437; break;
            case 0xE8: out[outpos++] = 0x0438; break;
            case 0xE9: out[outpos++] = 0x0439; break;
            case 0xEA: out[outpos++] = 0x043A; break;
            case 0xEB: out[outpos++] = 0x043B; break;
            case 0xEC: out[outpos++] = 0x043C; break;
            case 0xED: out[outpos++] = 0x043D; break;
            case 0xEE: out[outpos++] = 0x043E; break;
            case 0xEF: out[outpos++] = 0x043F; break;

            case 0xF0: out[outpos++] = 0x0440; break;
            case 0xF1: out[outpos++] = 0x0441; break;
            case 0xF2: out[outpos++] = 0x0442; break;
            case 0xF3: out[outpos++] = 0x0443; break;
            case 0xF4: out[outpos++] = 0x0444; break;
            case 0xF5: out[outpos++] = 0x0445; break;
            case 0xF6: out[outpos++] = 0x0446; break;
            case 0xF7: out[outpos++] = 0x0447; break;
            case 0xF8: out[outpos++] = 0x0448; break;
            case 0xF9: out[outpos++] = 0x0449; break;
            case 0xFA: out[outpos++] = 0x044A; break;
            case 0xFB: out[outpos++] = 0x044B; break;
            case 0xFC: out[outpos++] = 0x044C; break;
            case 0xFD: out[outpos++] = 0x044D; break;
            case 0xFE: out[outpos++] = 0x044E; break;
            case 0xFF: out[outpos++] = 0x044F; break;

            case 0x98:
            default:
                /* undefined */
                out[outpos++] = '?';
                break;
        }

        continue;
    }

    return outpos;
}

glui32 prepare_cp1251(glui32 *buf, glui32 buflen,
                      unsigned char *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        res = buf[pos++];

        if (res < 0x80)
        {
            out[outpos++] = res;
            continue;
        }

        switch (res)
        {
            case 0x0402: out[outpos++] = 0x80; break;
            case 0x0403: out[outpos++] = 0x81; break;
            case 0x201A: out[outpos++] = 0x82; break;
            case 0x0453: out[outpos++] = 0x83; break;
            case 0x201E: out[outpos++] = 0x84; break;
            case 0x2026: out[outpos++] = 0x85; break;
            case 0x2020: out[outpos++] = 0x86; break;
            case 0x2021: out[outpos++] = 0x87; break;
            case 0x20AC: out[outpos++] = 0x88; break;
            case 0x2030: out[outpos++] = 0x89; break;
            case 0x0409: out[outpos++] = 0x8A; break;
            case 0x2039: out[outpos++] = 0x8B; break;
            case 0x040A: out[outpos++] = 0x8C; break;
            case 0x040C: out[outpos++] = 0x8D; break;
            case 0x040B: out[outpos++] = 0x8E; break;
            case 0x040F: out[outpos++] = 0x8F; break;

            case 0x0452: out[outpos++] = 0x90; break;
            case 0x2018: out[outpos++] = 0x91; break;
            case 0x2019: out[outpos++] = 0x92; break;
            case 0x201C: out[outpos++] = 0x93; break;
            case 0x201D: out[outpos++] = 0x94; break;
            case 0x2022: out[outpos++] = 0x95; break;
            case 0x2013: out[outpos++] = 0x96; break;
            case 0x2014: out[outpos++] = 0x97; break;
            case 0x2122: out[outpos++] = 0x99; break;
            case 0x0459: out[outpos++] = 0x9A; break;
            case 0x203A: out[outpos++] = 0x9B; break;
            case 0x045A: out[outpos++] = 0x9C; break;
            case 0x045C: out[outpos++] = 0x9D; break;
            case 0x045B: out[outpos++] = 0x9E; break;
            case 0x045F: out[outpos++] = 0x9F; break;

            case 0x00A0: out[outpos++] = 0xA0; break;
            case 0x040E: out[outpos++] = 0xA1; break;
            case 0x045E: out[outpos++] = 0xA2; break;
            case 0x0408: out[outpos++] = 0xA3; break;
            case 0x00A4: out[outpos++] = 0xA4; break;
            case 0x0490: out[outpos++] = 0xA5; break;
            case 0x00A6: out[outpos++] = 0xA6; break;
            case 0x00A7: out[outpos++] = 0xA7; break;
            case 0x0401: out[outpos++] = 0xA8; break;
            case 0x00A9: out[outpos++] = 0xA9; break;
            case 0x0404: out[outpos++] = 0xAA; break;
            case 0x00AB: out[outpos++] = 0xAB; break;
            case 0x00AC: out[outpos++] = 0xAC; break;
            case 0x00AD: out[outpos++] = 0xAD; break;
            case 0x00AE: out[outpos++] = 0xAE; break;
            case 0x0407: out[outpos++] = 0xAF; break;

            case 0x00B0: out[outpos++] = 0xB0; break;
            case 0x00B1: out[outpos++] = 0xB1; break;
            case 0x0406: out[outpos++] = 0xB2; break;
            case 0x0456: out[outpos++] = 0xB3; break;
            case 0x0491: out[outpos++] = 0xB4; break;
            case 0x00B5: out[outpos++] = 0xB5; break;
            case 0x00B6: out[outpos++] = 0xB6; break;
            case 0x00B7: out[outpos++] = 0xB7; break;
            case 0x0451: out[outpos++] = 0xB8; break;
            case 0x2116: out[outpos++] = 0xB9; break;
            case 0x0454: out[outpos++] = 0xBA; break;
            case 0x00BB: out[outpos++] = 0xBB; break;
            case 0x0458: out[outpos++] = 0xBC; break;
            case 0x0405: out[outpos++] = 0xBD; break;
            case 0x0455: out[outpos++] = 0xBE; break;
            case 0x0457: out[outpos++] = 0xBF; break;

            case 0x0410: out[outpos++] = 0xC0; break;
            case 0x0411: out[outpos++] = 0xC1; break;
            case 0x0412: out[outpos++] = 0xC2; break;
            case 0x0413: out[outpos++] = 0xC3; break;
            case 0x0414: out[outpos++] = 0xC4; break;
            case 0x0415: out[outpos++] = 0xC5; break;
            case 0x0416: out[outpos++] = 0xC6; break;
            case 0x0417: out[outpos++] = 0xC7; break;
            case 0x0418: out[outpos++] = 0xC8; break;
            case 0x0419: out[outpos++] = 0xC9; break;
            case 0x041A: out[outpos++] = 0xCA; break;
            case 0x041B: out[outpos++] = 0xCB; break;
            case 0x041C: out[outpos++] = 0xCC; break;
            case 0x041D: out[outpos++] = 0xCD; break;
            case 0x041E: out[outpos++] = 0xCE; break;
            case 0x041F: out[outpos++] = 0xCF; break;

            case 0x0420: out[outpos++] = 0xD0; break;
            case 0x0421: out[outpos++] = 0xD1; break;
            case 0x0422: out[outpos++] = 0xD2; break;
            case 0x0423: out[outpos++] = 0xD3; break;
            case 0x0424: out[outpos++] = 0xD4; break;
            case 0x0425: out[outpos++] = 0xD5; break;
            case 0x0426: out[outpos++] = 0xD6; break;
            case 0x0427: out[outpos++] = 0xD7; break;
            case 0x0428: out[outpos++] = 0xD8; break;
            case 0x0429: out[outpos++] = 0xD9; break;
            case 0x042A: out[outpos++] = 0xDA; break;
            case 0x042B: out[outpos++] = 0xDB; break;
            case 0x042C: out[outpos++] = 0xDC; break;
            case 0x042D: out[outpos++] = 0xDD; break;
            case 0x042E: out[outpos++] = 0xDE; break;
            case 0x042F: out[outpos++] = 0xDF; break;

            case 0x0430: out[outpos++] = 0xE0; break;
            case 0x0431: out[outpos++] = 0xE1; break;
            case 0x0432: out[outpos++] = 0xE2; break;
            case 0x0433: out[outpos++] = 0xE3; break;
            case 0x0434: out[outpos++] = 0xE4; break;
            case 0x0435: out[outpos++] = 0xE5; break;
            case 0x0436: out[outpos++] = 0xE6; break;
            case 0x0437: out[outpos++] = 0xE7; break;
            case 0x0438: out[outpos++] = 0xE8; break;
            case 0x0439: out[outpos++] = 0xE9; break;
            case 0x043A: out[outpos++] = 0xEA; break;
            case 0x043B: out[outpos++] = 0xEB; break;
            case 0x043C: out[outpos++] = 0xEC; break;
            case 0x043D: out[outpos++] = 0xED; break;
            case 0x043E: out[outpos++] = 0xEE; break;
            case 0x043F: out[outpos++] = 0xEF; break;

            case 0x0440: out[outpos++] = 0xF0; break;
            case 0x0441: out[outpos++] = 0xF1; break;
            case 0x0442: out[outpos++] = 0xF2; break;
            case 0x0443: out[outpos++] = 0xF3; break;
            case 0x0444: out[outpos++] = 0xF4; break;
            case 0x0445: out[outpos++] = 0xF5; break;
            case 0x0446: out[outpos++] = 0xF6; break;
            case 0x0447: out[outpos++] = 0xF7; break;
            case 0x0448: out[outpos++] = 0xF8; break;
            case 0x0449: out[outpos++] = 0xF9; break;
            case 0x044A: out[outpos++] = 0xFA; break;
            case 0x044B: out[outpos++] = 0xFB; break;
            case 0x044C: out[outpos++] = 0xFC; break;
            case 0x044D: out[outpos++] = 0xFD; break;
            case 0x044E: out[outpos++] = 0xFE; break;
            case 0x044F: out[outpos++] = 0xFF; break;

            default:
                /* undefined */
                out[outpos++] = '?';
                break;
        }

        continue;
    }

    return outpos;
}

/* http://unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT */
glui32 parse_cp1252(unsigned char *buf, glui32 buflen,
                     glui32 *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        res = buf[pos++];

        if (res < 0x80)
        {
            out[outpos++] = res;
            continue;
        }

        switch (res)
        {
            case 0x80: out[outpos++] = 0x20AC; break;
            case 0x82: out[outpos++] = 0x201A; break;
            case 0x83: out[outpos++] = 0x0192; break;
            case 0x84: out[outpos++] = 0x201E; break;
            case 0x85: out[outpos++] = 0x2026; break;
            case 0x86: out[outpos++] = 0x2020; break;
            case 0x87: out[outpos++] = 0x2021; break;
            case 0x88: out[outpos++] = 0x02C6; break;
            case 0x89: out[outpos++] = 0x2030; break;
            case 0x8A: out[outpos++] = 0x0160; break;
            case 0x8B: out[outpos++] = 0x2039; break;
            case 0x8C: out[outpos++] = 0x0152; break;
            case 0x8E: out[outpos++] = 0x017D; break;

            case 0x91: out[outpos++] = 0x2018; break;
            case 0x92: out[outpos++] = 0x2019; break;
            case 0x93: out[outpos++] = 0x201C; break;
            case 0x94: out[outpos++] = 0x201D; break;
            case 0x95: out[outpos++] = 0x2022; break;
            case 0x96: out[outpos++] = 0x2013; break;
            case 0x97: out[outpos++] = 0x2014; break;
            case 0x98: out[outpos++] = 0x02DC; break;
            case 0x99: out[outpos++] = 0x2122; break;
            case 0x9A: out[outpos++] = 0x0161; break;
            case 0x9B: out[outpos++] = 0x203A; break;
            case 0x9C: out[outpos++] = 0x0153; break;
            case 0x9E: out[outpos++] = 0x017E; break;
            case 0x9F: out[outpos++] = 0x0178; break;

            case 0xA0: out[outpos++] = 0x00A0; break;
            case 0xA1: out[outpos++] = 0x00A1; break;
            case 0xA2: out[outpos++] = 0x00A2; break;
            case 0xA3: out[outpos++] = 0x00A3; break;
            case 0xA4: out[outpos++] = 0x00A4; break;
            case 0xA5: out[outpos++] = 0x00A5; break;
            case 0xA6: out[outpos++] = 0x00A6; break;
            case 0xA7: out[outpos++] = 0x00A7; break;
            case 0xA8: out[outpos++] = 0x00A8; break;
            case 0xA9: out[outpos++] = 0x00A9; break;
            case 0xAA: out[outpos++] = 0x00AA; break;
            case 0xAB: out[outpos++] = 0x00AB; break;
            case 0xAC: out[outpos++] = 0x00AC; break;
            case 0xAD: out[outpos++] = 0x00AD; break;
            case 0xAE: out[outpos++] = 0x00AE; break;
            case 0xAF: out[outpos++] = 0x00AF; break;

            case 0xB0: out[outpos++] = 0x00B0; break;
            case 0xB1: out[outpos++] = 0x00B1; break;
            case 0xB2: out[outpos++] = 0x00B2; break;
            case 0xB3: out[outpos++] = 0x00B3; break;
            case 0xB4: out[outpos++] = 0x00B4; break;
            case 0xB5: out[outpos++] = 0x00B5; break;
            case 0xB6: out[outpos++] = 0x00B6; break;
            case 0xB7: out[outpos++] = 0x00B7; break;
            case 0xB8: out[outpos++] = 0x00B8; break;
            case 0xB9: out[outpos++] = 0x00B9; break;
            case 0xBA: out[outpos++] = 0x00BA; break;
            case 0xBB: out[outpos++] = 0x00BB; break;
            case 0xBC: out[outpos++] = 0x00BC; break;
            case 0xBD: out[outpos++] = 0x00BD; break;
            case 0xBE: out[outpos++] = 0x00BE; break;
            case 0xBF: out[outpos++] = 0x00BF; break;

            case 0xC0: out[outpos++] = 0x00C0; break;
            case 0xC1: out[outpos++] = 0x00C1; break;
            case 0xC2: out[outpos++] = 0x00C2; break;
            case 0xC3: out[outpos++] = 0x00C3; break;
            case 0xC4: out[outpos++] = 0x00C4; break;
            case 0xC5: out[outpos++] = 0x00C5; break;
            case 0xC6: out[outpos++] = 0x00C6; break;
            case 0xC7: out[outpos++] = 0x00C7; break;
            case 0xC8: out[outpos++] = 0x00C8; break;
            case 0xC9: out[outpos++] = 0x00C9; break;
            case 0xCA: out[outpos++] = 0x00CA; break;
            case 0xCB: out[outpos++] = 0x00CB; break;
            case 0xCC: out[outpos++] = 0x00CC; break;
            case 0xCD: out[outpos++] = 0x00CD; break;
            case 0xCE: out[outpos++] = 0x00CE; break;
            case 0xCF: out[outpos++] = 0x00CF; break;

            case 0xD0: out[outpos++] = 0x00D0; break;
            case 0xD1: out[outpos++] = 0x00D1; break;
            case 0xD2: out[outpos++] = 0x00D2; break;
            case 0xD3: out[outpos++] = 0x00D3; break;
            case 0xD4: out[outpos++] = 0x00D4; break;
            case 0xD5: out[outpos++] = 0x00D5; break;
            case 0xD6: out[outpos++] = 0x00D6; break;
            case 0xD7: out[outpos++] = 0x00D7; break;
            case 0xD8: out[outpos++] = 0x00D8; break;
            case 0xD9: out[outpos++] = 0x00D9; break;
            case 0xDA: out[outpos++] = 0x00DA; break;
            case 0xDB: out[outpos++] = 0x00DB; break;
            case 0xDC: out[outpos++] = 0x00DC; break;
            case 0xDD: out[outpos++] = 0x00DD; break;
            case 0xDE: out[outpos++] = 0x00DE; break;
            case 0xDF: out[outpos++] = 0x00DF; break;

            case 0xE0: out[outpos++] = 0x00E0; break;
            case 0xE1: out[outpos++] = 0x00E1; break;
            case 0xE2: out[outpos++] = 0x00E2; break;
            case 0xE3: out[outpos++] = 0x00E3; break;
            case 0xE4: out[outpos++] = 0x00E4; break;
            case 0xE5: out[outpos++] = 0x00E5; break;
            case 0xE6: out[outpos++] = 0x00E6; break;
            case 0xE7: out[outpos++] = 0x00E7; break;
            case 0xE8: out[outpos++] = 0x00E8; break;
            case 0xE9: out[outpos++] = 0x00E9; break;
            case 0xEA: out[outpos++] = 0x00EA; break;
            case 0xEB: out[outpos++] = 0x00EB; break;
            case 0xEC: out[outpos++] = 0x00EC; break;
            case 0xED: out[outpos++] = 0x00ED; break;
            case 0xEE: out[outpos++] = 0x00EE; break;
            case 0xEF: out[outpos++] = 0x00EF; break;

            case 0xF0: out[outpos++] = 0x00F0; break;
            case 0xF1: out[outpos++] = 0x00F1; break;
            case 0xF2: out[outpos++] = 0x00F2; break;
            case 0xF3: out[outpos++] = 0x00F3; break;
            case 0xF4: out[outpos++] = 0x00F4; break;
            case 0xF5: out[outpos++] = 0x00F5; break;
            case 0xF6: out[outpos++] = 0x00F6; break;
            case 0xF7: out[outpos++] = 0x00F7; break;
            case 0xF8: out[outpos++] = 0x00F8; break;
            case 0xF9: out[outpos++] = 0x00F9; break;
            case 0xFA: out[outpos++] = 0x00FA; break;
            case 0xFB: out[outpos++] = 0x00FB; break;
            case 0xFC: out[outpos++] = 0x00FC; break;
            case 0xFD: out[outpos++] = 0x00FD; break;
            case 0xFE: out[outpos++] = 0x00FE; break;
            case 0xFF: out[outpos++] = 0x00FF; break;

            case 0x81:
            case 0x8D:
            case 0x8F:
            case 0x90:
            case 0x9D:
            default:
                /* undefined */
                out[outpos++] = '?';
                break;
        }

        continue;
    }

    return outpos;
}

glui32 prepare_cp1252(glui32 *buf, glui32 buflen,
                      unsigned char *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        res = buf[pos++];

        if (res < 0x80)
        {
            out[outpos++] = res;
            continue;
        }

        switch (res)
        {
            case 0x20AC: out[outpos++] = 0x80; break;
            case 0x201A: out[outpos++] = 0x82; break;
            case 0x0192: out[outpos++] = 0x83; break;
            case 0x201E: out[outpos++] = 0x84; break;
            case 0x2026: out[outpos++] = 0x85; break;
            case 0x2020: out[outpos++] = 0x86; break;
            case 0x2021: out[outpos++] = 0x87; break;
            case 0x02C6: out[outpos++] = 0x88; break;
            case 0x2030: out[outpos++] = 0x89; break;
            case 0x0160: out[outpos++] = 0x8A; break;
            case 0x2039: out[outpos++] = 0x8B; break;
            case 0x0152: out[outpos++] = 0x8C; break;
            case 0x017D: out[outpos++] = 0x8E; break;

            case 0x2018: out[outpos++] = 0x91; break;
            case 0x2019: out[outpos++] = 0x92; break;
            case 0x201C: out[outpos++] = 0x93; break;
            case 0x201D: out[outpos++] = 0x94; break;
            case 0x2022: out[outpos++] = 0x95; break;
            case 0x2013: out[outpos++] = 0x96; break;
            case 0x2014: out[outpos++] = 0x97; break;
            case 0x02DC: out[outpos++] = 0x98; break;
            case 0x2122: out[outpos++] = 0x99; break;
            case 0x0161: out[outpos++] = 0x9A; break;
            case 0x203A: out[outpos++] = 0x9B; break;
            case 0x0153: out[outpos++] = 0x9C; break;
            case 0x017E: out[outpos++] = 0x9E; break;
            case 0x0178: out[outpos++] = 0x9F; break;

            case 0x00A0: out[outpos++] = 0xA0; break;
            case 0x00A1: out[outpos++] = 0xA1; break;
            case 0x00A2: out[outpos++] = 0xA2; break;
            case 0x00A3: out[outpos++] = 0xA3; break;
            case 0x00A4: out[outpos++] = 0xA4; break;
            case 0x00A5: out[outpos++] = 0xA5; break;
            case 0x00A6: out[outpos++] = 0xA6; break;
            case 0x00A7: out[outpos++] = 0xA7; break;
            case 0x00A8: out[outpos++] = 0xA8; break;
            case 0x00A9: out[outpos++] = 0xA9; break;
            case 0x00AA: out[outpos++] = 0xAA; break;
            case 0x00AB: out[outpos++] = 0xAB; break;
            case 0x00AC: out[outpos++] = 0xAC; break;
            case 0x00AD: out[outpos++] = 0xAD; break;
            case 0x00AE: out[outpos++] = 0xAE; break;
            case 0x00AF: out[outpos++] = 0xAF; break;

            case 0x00B0: out[outpos++] = 0xB0; break;
            case 0x00B1: out[outpos++] = 0xB1; break;
            case 0x00B2: out[outpos++] = 0xB2; break;
            case 0x00B3: out[outpos++] = 0xB3; break;
            case 0x00B4: out[outpos++] = 0xB4; break;
            case 0x00B5: out[outpos++] = 0xB5; break;
            case 0x00B6: out[outpos++] = 0xB6; break;
            case 0x00B7: out[outpos++] = 0xB7; break;
            case 0x00B8: out[outpos++] = 0xB8; break;
            case 0x00B9: out[outpos++] = 0xB9; break;
            case 0x00BA: out[outpos++] = 0xBA; break;
            case 0x00BB: out[outpos++] = 0xBB; break;
            case 0x00BC: out[outpos++] = 0xBC; break;
            case 0x00BD: out[outpos++] = 0xBD; break;
            case 0x00BE: out[outpos++] = 0xBE; break;
            case 0x00BF: out[outpos++] = 0xBF; break;

            case 0x00C0: out[outpos++] = 0xC0; break;
            case 0x00C1: out[outpos++] = 0xC1; break;
            case 0x00C2: out[outpos++] = 0xC2; break;
            case 0x00C3: out[outpos++] = 0xC3; break;
            case 0x00C4: out[outpos++] = 0xC4; break;
            case 0x00C5: out[outpos++] = 0xC5; break;
            case 0x00C6: out[outpos++] = 0xC6; break;
            case 0x00C7: out[outpos++] = 0xC7; break;
            case 0x00C8: out[outpos++] = 0xC8; break;
            case 0x00C9: out[outpos++] = 0xC9; break;
            case 0x00CA: out[outpos++] = 0xCA; break;
            case 0x00CB: out[outpos++] = 0xCB; break;
            case 0x00CC: out[outpos++] = 0xCC; break;
            case 0x00CD: out[outpos++] = 0xCD; break;
            case 0x00CE: out[outpos++] = 0xCE; break;
            case 0x00CF: out[outpos++] = 0xCF; break;

            case 0x00D0: out[outpos++] = 0xD0; break;
            case 0x00D1: out[outpos++] = 0xD1; break;
            case 0x00D2: out[outpos++] = 0xD2; break;
            case 0x00D3: out[outpos++] = 0xD3; break;
            case 0x00D4: out[outpos++] = 0xD4; break;
            case 0x00D5: out[outpos++] = 0xD5; break;
            case 0x00D6: out[outpos++] = 0xD6; break;
            case 0x00D7: out[outpos++] = 0xD7; break;
            case 0x00D8: out[outpos++] = 0xD8; break;
            case 0x00D9: out[outpos++] = 0xD9; break;
            case 0x00DA: out[outpos++] = 0xDA; break;
            case 0x00DB: out[outpos++] = 0xDB; break;
            case 0x00DC: out[outpos++] = 0xDC; break;
            case 0x00DD: out[outpos++] = 0xDD; break;
            case 0x00DE: out[outpos++] = 0xDE; break;
            case 0x00DF: out[outpos++] = 0xDF; break;

            case 0x00E0: out[outpos++] = 0xE0; break;
            case 0x00E1: out[outpos++] = 0xE1; break;
            case 0x00E2: out[outpos++] = 0xE2; break;
            case 0x00E3: out[outpos++] = 0xE3; break;
            case 0x00E4: out[outpos++] = 0xE4; break;
            case 0x00E5: out[outpos++] = 0xE5; break;
            case 0x00E6: out[outpos++] = 0xE6; break;
            case 0x00E7: out[outpos++] = 0xE7; break;
            case 0x00E8: out[outpos++] = 0xE8; break;
            case 0x00E9: out[outpos++] = 0xE9; break;
            case 0x00EA: out[outpos++] = 0xEA; break;
            case 0x00EB: out[outpos++] = 0xEB; break;
            case 0x00EC: out[outpos++] = 0xEC; break;
            case 0x00ED: out[outpos++] = 0xED; break;
            case 0x00EE: out[outpos++] = 0xEE; break;
            case 0x00EF: out[outpos++] = 0xEF; break;

            case 0x00F0: out[outpos++] = 0xF0; break;
            case 0x00F1: out[outpos++] = 0xF1; break;
            case 0x00F2: out[outpos++] = 0xF2; break;
            case 0x00F3: out[outpos++] = 0xF3; break;
            case 0x00F4: out[outpos++] = 0xF4; break;
            case 0x00F5: out[outpos++] = 0xF5; break;
            case 0x00F6: out[outpos++] = 0xF6; break;
            case 0x00F7: out[outpos++] = 0xF7; break;
            case 0x00F8: out[outpos++] = 0xF8; break;
            case 0x00F9: out[outpos++] = 0xF9; break;
            case 0x00FA: out[outpos++] = 0xFA; break;
            case 0x00FB: out[outpos++] = 0xFB; break;
            case 0x00FC: out[outpos++] = 0xFC; break;
            case 0x00FD: out[outpos++] = 0xFD; break;
            case 0x00FE: out[outpos++] = 0xFE; break;
            case 0x00FF: out[outpos++] = 0xFF; break;

            default:
                /* undefined */
                out[outpos++] = '?';
                break;
        }

        continue;
    }

    return outpos;
}

/* http://unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT */
glui32 parse_mac(unsigned char *buf, glui32 buflen,
                     glui32 *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        res = buf[pos++];

        if (res < 0x80)
        {
            out[outpos++] = res;
            continue;
        }

        switch (res)
        {
            case 0x80: out[outpos++] = 0x00C4; break;
            case 0x81: out[outpos++] = 0x00C5; break;
            case 0x82: out[outpos++] = 0x00C7; break;
            case 0x83: out[outpos++] = 0x00C9; break;
            case 0x84: out[outpos++] = 0x00D1; break;
            case 0x85: out[outpos++] = 0x00D6; break;
            case 0x86: out[outpos++] = 0x00DC; break;
            case 0x87: out[outpos++] = 0x00E1; break;
            case 0x88: out[outpos++] = 0x00E0; break;
            case 0x89: out[outpos++] = 0x00E2; break;
            case 0x8A: out[outpos++] = 0x00E4; break;
            case 0x8B: out[outpos++] = 0x00E3; break;
            case 0x8C: out[outpos++] = 0x00E5; break;
            case 0x8D: out[outpos++] = 0x00E7; break;
            case 0x8E: out[outpos++] = 0x00E9; break;
            case 0x8F: out[outpos++] = 0x00E8; break;

            case 0x90: out[outpos++] = 0x00EA; break;
            case 0x91: out[outpos++] = 0x00EB; break;
            case 0x92: out[outpos++] = 0x00ED; break;
            case 0x93: out[outpos++] = 0x00EC; break;
            case 0x94: out[outpos++] = 0x00EE; break;
            case 0x95: out[outpos++] = 0x00EF; break;
            case 0x96: out[outpos++] = 0x00F1; break;
            case 0x97: out[outpos++] = 0x00F3; break;
            case 0x98: out[outpos++] = 0x00F2; break;
            case 0x99: out[outpos++] = 0x00F4; break;
            case 0x9A: out[outpos++] = 0x00F6; break;
            case 0x9B: out[outpos++] = 0x00F5; break;
            case 0x9C: out[outpos++] = 0x00FA; break;
            case 0x9D: out[outpos++] = 0x00F9; break;
            case 0x9E: out[outpos++] = 0x00FB; break;
            case 0x9F: out[outpos++] = 0x00FC; break;

            case 0xA0: out[outpos++] = 0x2020; break;
            case 0xA1: out[outpos++] = 0x00B0; break;
            case 0xA2: out[outpos++] = 0x00A2; break;
            case 0xA3: out[outpos++] = 0x00A3; break;
            case 0xA4: out[outpos++] = 0x00A7; break;
            case 0xA5: out[outpos++] = 0x2022; break;
            case 0xA6: out[outpos++] = 0x00B6; break;
            case 0xA7: out[outpos++] = 0x00DF; break;
            case 0xA8: out[outpos++] = 0x00AE; break;
            case 0xA9: out[outpos++] = 0x00A9; break;
            case 0xAA: out[outpos++] = 0x2122; break;
            case 0xAB: out[outpos++] = 0x00B4; break;
            case 0xAC: out[outpos++] = 0x00A8; break;
            case 0xAD: out[outpos++] = 0x2260; break;
            case 0xAE: out[outpos++] = 0x00C6; break;
            case 0xAF: out[outpos++] = 0x00D8; break;

            case 0xB0: out[outpos++] = 0x221E; break;
            case 0xB1: out[outpos++] = 0x00B1; break;
            case 0xB2: out[outpos++] = 0x2264; break;
            case 0xB3: out[outpos++] = 0x2265; break;
            case 0xB4: out[outpos++] = 0x00A5; break;
            case 0xB5: out[outpos++] = 0x00B5; break;
            case 0xB6: out[outpos++] = 0x2202; break;
            case 0xB7: out[outpos++] = 0x2211; break;
            case 0xB8: out[outpos++] = 0x220F; break;
            case 0xB9: out[outpos++] = 0x03C0; break;
            case 0xBA: out[outpos++] = 0x222B; break;
            case 0xBB: out[outpos++] = 0x00AA; break;
            case 0xBC: out[outpos++] = 0x00BA; break;
            case 0xBD: out[outpos++] = 0x03A9; break;
            case 0xBE: out[outpos++] = 0x00E6; break;
            case 0xBF: out[outpos++] = 0x00F8; break;

            case 0xC0: out[outpos++] = 0x00BF; break;
            case 0xC1: out[outpos++] = 0x00A1; break;
            case 0xC2: out[outpos++] = 0x00AC; break;
            case 0xC3: out[outpos++] = 0x221A; break;
            case 0xC4: out[outpos++] = 0x0192; break;
            case 0xC5: out[outpos++] = 0x2248; break;
            case 0xC6: out[outpos++] = 0x2206; break;
            case 0xC7: out[outpos++] = 0x00AB; break;
            case 0xC8: out[outpos++] = 0x00BB; break;
            case 0xC9: out[outpos++] = 0x2026; break;
            case 0xCA: out[outpos++] = 0x00A0; break;
            case 0xCB: out[outpos++] = 0x00C0; break;
            case 0xCC: out[outpos++] = 0x00C3; break;
            case 0xCD: out[outpos++] = 0x00D5; break;
            case 0xCE: out[outpos++] = 0x0152; break;
            case 0xCF: out[outpos++] = 0x0153; break;

            case 0xD0: out[outpos++] = 0x2013; break;
            case 0xD1: out[outpos++] = 0x2014; break;
            case 0xD2: out[outpos++] = 0x201C; break;
            case 0xD3: out[outpos++] = 0x201D; break;
            case 0xD4: out[outpos++] = 0x2018; break;
            case 0xD5: out[outpos++] = 0x2019; break;
            case 0xD6: out[outpos++] = 0x00F7; break;
            case 0xD7: out[outpos++] = 0x25CA; break;
            case 0xD8: out[outpos++] = 0x00FF; break;
            case 0xD9: out[outpos++] = 0x0178; break;
            case 0xDA: out[outpos++] = 0x2044; break;
            case 0xDB: out[outpos++] = 0x20AC; break;
            case 0xDC: out[outpos++] = 0x2039; break;
            case 0xDD: out[outpos++] = 0x203A; break;
            case 0xDE: out[outpos++] = 0xFB01; break;
            case 0xDF: out[outpos++] = 0xFB02; break;

            case 0xE0: out[outpos++] = 0x2021; break;
            case 0xE1: out[outpos++] = 0x00B7; break;
            case 0xE2: out[outpos++] = 0x201A; break;
            case 0xE3: out[outpos++] = 0x201E; break;
            case 0xE4: out[outpos++] = 0x2030; break;
            case 0xE5: out[outpos++] = 0x00C2; break;
            case 0xE6: out[outpos++] = 0x00CA; break;
            case 0xE7: out[outpos++] = 0x00C1; break;
            case 0xE8: out[outpos++] = 0x00CB; break;
            case 0xE9: out[outpos++] = 0x00C8; break;
            case 0xEA: out[outpos++] = 0x00CD; break;
            case 0xEB: out[outpos++] = 0x00CE; break;
            case 0xEC: out[outpos++] = 0x00CF; break;
            case 0xED: out[outpos++] = 0x00CC; break;
            case 0xEE: out[outpos++] = 0x00D3; break;
            case 0xEF: out[outpos++] = 0x00D4; break;

            case 0xF0: out[outpos++] = 0xF8FF; break;
            case 0xF1: out[outpos++] = 0x00D2; break;
            case 0xF2: out[outpos++] = 0x00DA; break;
            case 0xF3: out[outpos++] = 0x00DB; break;
            case 0xF4: out[outpos++] = 0x00D9; break;
            case 0xF5: out[outpos++] = 0x0131; break;
            case 0xF6: out[outpos++] = 0x02C6; break;
            case 0xF7: out[outpos++] = 0x02DC; break;
            case 0xF8: out[outpos++] = 0x00AF; break;
            case 0xF9: out[outpos++] = 0x02D8; break;
            case 0xFA: out[outpos++] = 0x02D9; break;
            case 0xFB: out[outpos++] = 0x02DA; break;
            case 0xFC: out[outpos++] = 0x00B8; break;
            case 0xFD: out[outpos++] = 0x02DD; break;
            case 0xFE: out[outpos++] = 0x02DB; break;
            case 0xFF: out[outpos++] = 0x02C7; break;

            default:
                /* undefined */
                out[outpos++] = '?';
                break;
        }

        continue;
    }

    return outpos;
}

glui32 prepare_mac(glui32 *buf, glui32 buflen,
                      unsigned char *out, glui32 outlen)
{
    glui32 pos = 0;
    glui32 outpos = 0;
    glui32 res;

    while (outpos < outlen)
    {
        if (pos >= buflen)
            break;

        res = buf[pos++];

        if (res < 0x80)
        {
            out[outpos++] = res;
            continue;
        }

        switch (res)
        {
            case 0x00C4: out[outpos++] = 0x80; break;
            case 0x00C5: out[outpos++] = 0x81; break;
            case 0x00C7: out[outpos++] = 0x82; break;
            case 0x00C9: out[outpos++] = 0x83; break;
            case 0x00D1: out[outpos++] = 0x84; break;
            case 0x00D6: out[outpos++] = 0x85; break;
            case 0x00DC: out[outpos++] = 0x86; break;
            case 0x00E1: out[outpos++] = 0x87; break;
            case 0x00E0: out[outpos++] = 0x88; break;
            case 0x00E2: out[outpos++] = 0x89; break;
            case 0x00E4: out[outpos++] = 0x8A; break;
            case 0x00E3: out[outpos++] = 0x8B; break;
            case 0x00E5: out[outpos++] = 0x8C; break;
            case 0x00E7: out[outpos++] = 0x8D; break;
            case 0x00E9: out[outpos++] = 0x8E; break;
            case 0x00E8: out[outpos++] = 0x8F; break;

            case 0x00EA: out[outpos++] = 0x90; break;
            case 0x00EB: out[outpos++] = 0x91; break;
            case 0x00ED: out[outpos++] = 0x92; break;
            case 0x00EC: out[outpos++] = 0x93; break;
            case 0x00EE: out[outpos++] = 0x94; break;
            case 0x00EF: out[outpos++] = 0x95; break;
            case 0x00F1: out[outpos++] = 0x96; break;
            case 0x00F3: out[outpos++] = 0x97; break;
            case 0x00F2: out[outpos++] = 0x98; break;
            case 0x00F4: out[outpos++] = 0x99; break;
            case 0x00F6: out[outpos++] = 0x9A; break;
            case 0x00F5: out[outpos++] = 0x9B; break;
            case 0x00FA: out[outpos++] = 0x9C; break;
            case 0x00F9: out[outpos++] = 0x9D; break;
            case 0x00FB: out[outpos++] = 0x9E; break;
            case 0x00FC: out[outpos++] = 0x9F; break;

            case 0x2020: out[outpos++] = 0xA0; break;
            case 0x00B0: out[outpos++] = 0xA1; break;
            case 0x00A2: out[outpos++] = 0xA2; break;
            case 0x00A3: out[outpos++] = 0xA3; break;
            case 0x00A7: out[outpos++] = 0xA4; break;
            case 0x2022: out[outpos++] = 0xA5; break;
            case 0x00B6: out[outpos++] = 0xA6; break;
            case 0x00DF: out[outpos++] = 0xA7; break;
            case 0x00AE: out[outpos++] = 0xA8; break;
            case 0x00A9: out[outpos++] = 0xA9; break;
            case 0x2122: out[outpos++] = 0xAA; break;
            case 0x00B4: out[outpos++] = 0xAB; break;
            case 0x00A8: out[outpos++] = 0xAC; break;
            case 0x2260: out[outpos++] = 0xAD; break;
            case 0x00C6: out[outpos++] = 0xAE; break;
            case 0x00D8: out[outpos++] = 0xAF; break;

            case 0x221E: out[outpos++] = 0xB0; break;
            case 0x00B1: out[outpos++] = 0xB1; break;
            case 0x2264: out[outpos++] = 0xB2; break;
            case 0x2265: out[outpos++] = 0xB3; break;
            case 0x00A5: out[outpos++] = 0xB4; break;
            case 0x00B5: out[outpos++] = 0xB5; break;
            case 0x2202: out[outpos++] = 0xB6; break;
            case 0x2211: out[outpos++] = 0xB7; break;
            case 0x220F: out[outpos++] = 0xB8; break;
            case 0x03C0: out[outpos++] = 0xB9; break;
            case 0x222B: out[outpos++] = 0xBA; break;
            case 0x00AA: out[outpos++] = 0xBB; break;
            case 0x00BA: out[outpos++] = 0xBC; break;
            case 0x03A9: out[outpos++] = 0xBD; break;
            case 0x00E6: out[outpos++] = 0xBE; break;
            case 0x00F8: out[outpos++] = 0xBF; break;

            case 0x00BF: out[outpos++] = 0xC0; break;
            case 0x00A1: out[outpos++] = 0xC1; break;
            case 0x00AC: out[outpos++] = 0xC2; break;
            case 0x221A: out[outpos++] = 0xC3; break;
            case 0x0192: out[outpos++] = 0xC4; break;
            case 0x2248: out[outpos++] = 0xC5; break;
            case 0x2206: out[outpos++] = 0xC6; break;
            case 0x00AB: out[outpos++] = 0xC7; break;
            case 0x00BB: out[outpos++] = 0xC8; break;
            case 0x2026: out[outpos++] = 0xC9; break;
            case 0x00A0: out[outpos++] = 0xCA; break;
            case 0x00C0: out[outpos++] = 0xCB; break;
            case 0x00C3: out[outpos++] = 0xCC; break;
            case 0x00D5: out[outpos++] = 0xCD; break;
            case 0x0152: out[outpos++] = 0xCE; break;
            case 0x0153: out[outpos++] = 0xCF; break;

            case 0x2013: out[outpos++] = 0xD0; break;
            case 0x2014: out[outpos++] = 0xD1; break;
            case 0x201C: out[outpos++] = 0xD2; break;
            case 0x201D: out[outpos++] = 0xD3; break;
            case 0x2018: out[outpos++] = 0xD4; break;
            case 0x2019: out[outpos++] = 0xD5; break;
            case 0x00F7: out[outpos++] = 0xD6; break;
            case 0x25CA: out[outpos++] = 0xD7; break;
            case 0x00FF: out[outpos++] = 0xD8; break;
            case 0x0178: out[outpos++] = 0xD9; break;
            case 0x2044: out[outpos++] = 0xDA; break;
            case 0x20AC: out[outpos++] = 0xDB; break;
            case 0x2039: out[outpos++] = 0xDC; break;
            case 0x203A: out[outpos++] = 0xDD; break;
            case 0xFB01: out[outpos++] = 0xDE; break;
            case 0xFB02: out[outpos++] = 0xDF; break;

            case 0x2021: out[outpos++] = 0xE0; break;
            case 0x00B7: out[outpos++] = 0xE1; break;
            case 0x201A: out[outpos++] = 0xE2; break;
            case 0x201E: out[outpos++] = 0xE3; break;
            case 0x2030: out[outpos++] = 0xE4; break;
            case 0x00C2: out[outpos++] = 0xE5; break;
            case 0x00CA: out[outpos++] = 0xE6; break;
            case 0x00C1: out[outpos++] = 0xE7; break;
            case 0x00CB: out[outpos++] = 0xE8; break;
            case 0x00C8: out[outpos++] = 0xE9; break;
            case 0x00CD: out[outpos++] = 0xEA; break;
            case 0x00CE: out[outpos++] = 0xEB; break;
            case 0x00CF: out[outpos++] = 0xEC; break;
            case 0x00CC: out[outpos++] = 0xED; break;
            case 0x00D3: out[outpos++] = 0xEE; break;
            case 0x00D4: out[outpos++] = 0xEF; break;

            case 0xF8FF: out[outpos++] = 0xF0; break;
            case 0x00D2: out[outpos++] = 0xF1; break;
            case 0x00DA: out[outpos++] = 0xF2; break;
            case 0x00DB: out[outpos++] = 0xF3; break;
            case 0x00D9: out[outpos++] = 0xF4; break;
            case 0x0131: out[outpos++] = 0xF5; break;
            case 0x02C6: out[outpos++] = 0xF6; break;
            case 0x02DC: out[outpos++] = 0xF7; break;
            case 0x00AF: out[outpos++] = 0xF8; break;
            case 0x02D8: out[outpos++] = 0xF9; break;
            case 0x02D9: out[outpos++] = 0xFA; break;
            case 0x02DA: out[outpos++] = 0xFB; break;
            case 0x00B8: out[outpos++] = 0xFC; break;
            case 0x02DD: out[outpos++] = 0xFD; break;
            case 0x02DB: out[outpos++] = 0xFE; break;
            case 0x02C7: out[outpos++] = 0xFF; break;

            default:
                /* undefined */
                out[outpos++] = '?';
                break;
        }

        continue;
    }

    return outpos;
}

glui32 os_parse_chars (unsigned char *buf, glui32 buflen,
                       glui32 *out, glui32 outlen)
{
    switch (os_charmap)
    {
        case OS_UTF8:
            return parse_utf8(buf, buflen, out, outlen);

        case OS_CP1251:
            return parse_cp1251(buf, buflen, out, outlen);

        case OS_CP1252:
            return parse_cp1252(buf, buflen, out, outlen);

        case OS_MACROMAN:
            return parse_mac(buf, buflen, out, outlen);

        default:
            return 0;
    }
}

glui32 os_prepare_chars (glui32 *buf, glui32 buflen,
                         unsigned char *out, glui32 outlen)
{
    switch (os_charmap)
    {
        case OS_UTF8:
            return prepare_utf8(buf, buflen, out, outlen);

        case OS_CP1251:
            return prepare_cp1251(buf, buflen, out, outlen);

        case OS_CP1252:
            return prepare_cp1252(buf, buflen, out, outlen);

        case OS_MACROMAN:
            return prepare_mac(buf, buflen, out, outlen);

        default:
            return 0;
    }
}

#endif /* GLK_UNICODE */

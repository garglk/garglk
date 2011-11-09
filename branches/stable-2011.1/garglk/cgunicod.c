/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2011 by Andrew Plotkin, Jesse McGrew.                   *
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

/* cgunicod.c: Unicode helper functions for Glk API, version 0.7.1.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright 1998-2011 by Andrew Plotkin.
    You may copy, distribute, and incorporate it into your own programs,
    by any means and under any conditions, as long as you do not modify it.
    You may also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "glk.h"
#include "garglk.h"

void gli_putchar_utf8(glui32 val, FILE *fl)
{
    if (val < 0x80)
    {
        putc(val, fl);
    }
    else if (val < 0x800)
    {
        putc((0xC0 | ((val & 0x7C0) >> 6)), fl);
        putc((0x80 |  (val & 0x03F)     ),  fl);
    }
    else if (val < 0x10000)
    {
        putc((0xE0 | ((val & 0xF000) >> 12)), fl);
        putc((0x80 | ((val & 0x0FC0) >>  6)), fl);
        putc((0x80 |  (val & 0x003F)      ),  fl);
    }
    else if (val < 0x200000)
    {
        putc((0xF0 | ((val & 0x1C0000) >> 18)), fl);
        putc((0x80 | ((val & 0x03F000) >> 12)), fl);
        putc((0x80 | ((val & 0x000FC0) >>  6)), fl);
        putc((0x80 |  (val & 0x00003F)      ),  fl);
    }
    else
    {
        putc('?', fl);
    }
}

static inline glui32 read_byte(FILE *fl)
{
    int c;

    c = getc(fl);

    if(c == EOF) return -1;

    return c;
}

glui32 gli_getchar_utf8(FILE *fl)
{
    glui32 res;
    glui32 val0, val1, val2, val3;

    val0 = read_byte(fl);
    if (val0 == (glui32)-1)
        return -1;

    if (val0 < 0x80)
    {
        res = val0;
        return res;
    }

    if ((val0 & 0xe0) == 0xc0)
    {
        val1 = read_byte(fl);
        if (val1 == (glui32)-1)
        {
            gli_strict_warning("incomplete two-byte character");
            return -1;
        }
        if ((val1 & 0xc0) != 0x80)
        {
            gli_strict_warning("malformed two-byte character");
            return '?';
        }
        res = (val0 & 0x1f) << 6;
        res |= (val1 & 0x3f);
        return res;
    }

    if ((val0 & 0xf0) == 0xe0)
    {
        val1 = read_byte(fl);
        val2 = read_byte(fl);
        if (val1 == (glui32)-1 || val2 == (glui32)-1)
        {
            gli_strict_warning("incomplete three-byte character");
            return -1;
        }
        if ((val1 & 0xc0) != 0x80)
        {
            gli_strict_warning("malformed three-byte character");
            return '?';
        }
        if ((val2 & 0xc0) != 0x80)
        {
            gli_strict_warning("malformed three-byte character");
            return '?';
        }
        res = (((val0 & 0xf)<<12)  & 0x0000f000);
        res |= (((val1 & 0x3f)<<6) & 0x00000fc0);
        res |= (((val2 & 0x3f))    & 0x0000003f);
        return res;
    }

    if ((val0 & 0xf0) == 0xf0)
    {
        if ((val0 & 0xf8) != 0xf0)
        {
            gli_strict_warning("malformed four-byte character");
            return '?';
        }
        val1 = read_byte(fl);
        val2 = read_byte(fl);
        val3 = read_byte(fl);
        if (val1 == (glui32)-1 || val2 == (glui32)-1 || val3 == (glui32)-1)
        {
            gli_strict_warning("incomplete four-byte character");
            return -1;
        }
        if ((val1 & 0xc0) != 0x80)
        {
            gli_strict_warning("malformed four-byte character");
            return '?';
        }
        if ((val2 & 0xc0) != 0x80)
        {
            gli_strict_warning("malformed four-byte character");
            return '?';
        }
        if ((val3 & 0xc0) != 0x80)
        {
            gli_strict_warning("malformed four-byte character");
            return '?';
        }
        res = (((val0 & 0x7)<<18)   & 0x1c0000);
        res |= (((val1 & 0x3f)<<12) & 0x03f000);
        res |= (((val2 & 0x3f)<<6)  & 0x000fc0);
        res |= (((val3 & 0x3f))     & 0x00003f);
        return res;
    }

    gli_strict_warning("malformed character");
    return '?';
}

glui32 gli_parse_utf8(unsigned char *buf, glui32 buflen,
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
                gli_strict_warning("incomplete two-byte character");
                break;
            }
            val1 = buf[pos++];
            if ((val1 & 0xc0) != 0x80)
            {
                gli_strict_warning("malformed two-byte character");
                break;
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
                gli_strict_warning("incomplete three-byte character");
                break;
            }
            val1 = buf[pos++];
            val2 = buf[pos++];
            if ((val1 & 0xc0) != 0x80)
            {
                gli_strict_warning("malformed three-byte character");
                break;
            }
            if ((val2 & 0xc0) != 0x80)
            {
                gli_strict_warning("malformed three-byte character");
                break;
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
                gli_strict_warning("malformed four-byte character");
                break;
            }
            if (pos+3 > buflen)
            {
                gli_strict_warning("incomplete four-byte character");
                break;
            }
            val1 = buf[pos++];
            val2 = buf[pos++];
            val3 = buf[pos++];
            if ((val1 & 0xc0) != 0x80)
            {
                gli_strict_warning("malformed four-byte character");
                break;
            }
            if ((val2 & 0xc0) != 0x80)
            {
                gli_strict_warning("malformed four-byte character");
                break;
            }
            if ((val3 & 0xc0) != 0x80)
            {
                gli_strict_warning("malformed four-byte character");
                break;
            }
            res = (((val0 & 0x7)<<18)   & 0x1c0000);
            res |= (((val1 & 0x3f)<<12) & 0x03f000);
            res |= (((val2 & 0x3f)<<6)  & 0x000fc0);
            res |= (((val3 & 0x3f))     & 0x00003f);
            out[outpos++] = res;
            continue;
        }

        gli_strict_warning("malformed character");
    }

    return outpos;
}

#ifdef GLK_MODULE_UNICODE

/* The cgunigen.c file is generated from Unicode data tables, and it's
   sort of enormous. Feel free to implement all these case-changing and
   normalization functions using your OS's native facilities. */

#include "cgunigen.c"

#define CASE_UPPER (0)
#define CASE_LOWER (1)
#define CASE_TITLE (2)
#define CASE_IDENT (3)

#define COND_ALL (0)
#define COND_LINESTART (1)

/* Apply a case change to the buffer. The len is the length of the buffer
   array; numchars is the number of characters originally in it. (This
   may be less than len.) The result will be clipped to fit len, but
   the return value will be the full number of characters that the
   converted string should have contained.
+*/

static glui32 gli_buffer_change_case(glui32 *buf, glui32 len,
    glui32 numchars, int destcase, int cond, int changerest)
{
    glui32 ix, jx;
    glui32 *outbuf;
    glui32 *newoutbuf;
    glui32 outcount;
    int dest_block_rest, dest_block_first;
    int dest_spec_rest, dest_spec_first;

    switch (cond)
    {
        case COND_ALL:
            dest_spec_rest = destcase;
            dest_spec_first = destcase;
            break;
        case COND_LINESTART:
            if (changerest)
                dest_spec_rest = CASE_LOWER;
            else
                dest_spec_rest = CASE_IDENT;
            dest_spec_first = destcase;
            break;
    }

    dest_block_rest = dest_spec_rest;
    if (dest_block_rest == CASE_TITLE)
        dest_block_rest = CASE_UPPER;
    dest_block_first = dest_spec_first;
    if (dest_block_first == CASE_TITLE)
        dest_block_first = CASE_UPPER;

    newoutbuf = NULL;
    outcount = 0;
    outbuf = buf;

    for (ix=0; ix<numchars; ix++)
    {
        int target;
        int isfirst;
        glui32 res;
        glui32 *special;
        glui32 *ptr;
        glui32 speccount;
        glui32 ch = buf[ix];

        isfirst = (ix == 0);

        target = (isfirst ? dest_block_first : dest_block_rest);

        if (target == CASE_IDENT)
        {
            res = ch;
        }
        else
        {
            gli_case_block_t *block;

            GET_CASE_BLOCK(ch, &block);
            if (!block)
                res = ch;
            else
                res = block[ch & 0xFF][target];
        }

        if (res != 0xFFFFFFFF || res == ch)
        {
            /* simple case */
            if (outcount < len)
                outbuf[outcount] = res;
            outcount++;
            continue;
        }

        target = (isfirst ? dest_spec_first : dest_spec_rest);

        /* complicated cases */
        GET_CASE_SPECIAL(ch, &special);
        if (!special)
        {
            gli_strict_warning("inconsistency in cgunigen.c");
            continue;
        }
        ptr = &unigen_special_array[special[target]];
        speccount = *(ptr++);

        if (speccount == 1)
        {
            /* simple after all */
            if (outcount < len)
                outbuf[outcount] = ptr[0];
            outcount++;
            continue;
        }

        /* Now we have to allocate a new buffer, if we haven't already. */
        if (!newoutbuf)
        {
            newoutbuf = malloc((len+1) * sizeof(glui32));
            if (!newoutbuf)
                return 0;
            if (outcount)
                memcpy(newoutbuf, buf, outcount * sizeof(glui32));
            outbuf = newoutbuf;
        }

        for (jx=0; jx<speccount; jx++)
        {
            if (outcount < len)
                outbuf[outcount] = ptr[jx];
            outcount++;
        }
    }

    if (newoutbuf)
    {
        glui32 finallen = outcount;
        if (finallen > len)
            finallen = len;
        if (finallen)
            memcpy(buf, newoutbuf, finallen * sizeof(glui32));
        free(newoutbuf);
    }

    return outcount;
}

glui32 glk_buffer_to_lower_case_uni(glui32 *buf, glui32 len,
    glui32 numchars)
{
    return gli_buffer_change_case(buf, len, numchars,
        CASE_LOWER, COND_ALL, TRUE);
}

glui32 glk_buffer_to_upper_case_uni(glui32 *buf, glui32 len,
    glui32 numchars)
{
    return gli_buffer_change_case(buf, len, numchars,
        CASE_UPPER, COND_ALL, TRUE);
}

glui32 glk_buffer_to_title_case_uni(glui32 *buf, glui32 len,
    glui32 numchars, glui32 lowerrest)
{
    return gli_buffer_change_case(buf, len, numchars,
        CASE_TITLE, COND_LINESTART, lowerrest);
}

#endif /* GLK_MODULE_UNICODE */

#ifdef GLK_MODULE_UNICODE_NORM

/* We're relying on the fact that cgunigen.c has already been included.
   So don't try to use GLK_MODULE_UNICODE_NORM without GLK_MODULE_UNICODE.
*/

static glui32 combining_class(glui32 ch)
{
    RETURN_COMBINING_CLASS(ch);
}

/* This returns a new buffer (possibly longer), containing the decomposed
   form of the original buffer. The caller must free the returned buffer.
   On exit, *numcharsref contains the size of the returned buffer.
   The original buffer is unchanged.
*/
static glui32 *gli_buffer_canon_decompose_uni(glui32 *buf,
    glui32 *numcharsref)
{
    /* The algorithm for the canonical decomposition of a string: For
       each character, look up the decomposition in the decomp table.
       Append the decomposition to the buffer. Finally, sort every
       substring of the buffer which is made up of combining
       characters (characters with a nonzero combining class). */

    glui32 numchars = *numcharsref;
    glui32 destsize = numchars * 2 + 16;
    glui32 *dest = (glui32 *)malloc(destsize * sizeof(glui32));
    glui32 destlen = 0;
    glui32 ix, jx;
    int anycombining = FALSE;

    if (!dest)
        return NULL;

    for (ix=0; ix<numchars; ix++)
    {
        glui32 ch = buf[ix];
        gli_decomp_block_t *block;
        glui32 count, pos;

        if (combining_class(ch))
            anycombining = TRUE;

        GET_DECOMP_BLOCK(ch, &block);
        if (block)
        {
            block += (ch & 0xFF);
            count = (*block)[0];
            pos = (*block)[1];
        }
        else
        {
            GET_DECOMP_SPECIAL(ch, &count, &pos);
        }

        if (!count)
        {
            /* The simple case: this character doesn't decompose. Push
               it straight into the destination. */
            if (destlen >= destsize)
            {
                destsize = destsize * 2;
                dest = (glui32 *)realloc(dest, destsize * sizeof(glui32));
                if (!dest)
                    return NULL;
            }
            dest[destlen] = ch;
            destlen++;
            continue;
        }

        /* Assume that a character with a decomposition has a
           combining class somewhere in there. Not always true, but
           it's simpler to assume it. */
        anycombining = TRUE;

        /* We now append count characters to the buffer, reading from
           unigen_decomp_data[pos] onwards. None of these characters
           are decomposable; that was already recursively expanded when
           unigen_decomp_data was generated. */

        if (destlen+count >= destsize)
        {
            /* Okay, that wasn't enough. Expand more. */
            destsize = destsize * 2 + count;
            dest = (glui32 *)realloc(dest, destsize * sizeof(glui32));
            if (!dest)
                return NULL;
        }
        for (jx=0; jx<count; jx++)
        {
            dest[destlen] = unigen_decomp_data[pos+jx];
            destlen++;
        }
    }

    if (anycombining)
    {
        /* Now we sort groups of combining characters. This should be a
           stable sort by the combining-class number. We're lazy and
           nearly all groups are short, so we'll just bubble-sort. */
        glui32 grpstart, grpend, kx;
        ix = 0;
        while (ix < destlen)
        {
            if (!combining_class(dest[ix]))
            {
                ix++;
                continue;
            }
            if (ix >= destlen)
                break;
            grpstart = ix;
            while (ix < destlen && combining_class(dest[ix]))
                ix++;
            grpend = ix;
            if (grpend - grpstart >= 2)
            {
                /* Sort this group. */
                for (jx = grpend-1; jx > grpstart; jx--)
                {
                    for (kx = grpstart; kx < jx; kx++)
                    {
                        if (combining_class(dest[kx]) > combining_class(dest[kx+1]))
                        {
                            glui32 tmp = dest[kx];
                            dest[kx] = dest[kx+1];
                            dest[kx+1] = tmp;
                        }
                    }
                }
            }
        }
    }

    *numcharsref = destlen;
    return dest;
}

static glui32 check_composition(glui32 ch1, glui32 ch2)
{
    RETURN_COMPOSITION(ch1, ch2);
}

/* This composes characters in the given buffer, in place. It returns the
   number of characters in the result, which will be less than or equal
   to len.
*/
static glui32 gli_buffer_canon_compose_uni(glui32 *buf, glui32 len)
{
    /* This algorithm is lifted from the Java sample code at
       <http://www.unicode.org/reports/tr15/Normalizer.html>.
       I apologize for the ugly.

       Roughly, pos is the position of the last base character;
       curch is that character in progress; ix is the next character
       to write (which may fly ahead of pos, as we encounter a string of
       combining chars); and jx is the position that we're scanning.
       In the simplest case, jx and ix stay together, with pos one behind. */

    glui32 curch, newch, curclass, newclass, res;
    glui32 ix, jx, pos;

    if (len == 0)
        return 0;

    pos = 0;
    curch = buf[0];
    curclass = combining_class(curch);
    if (curclass)
        curclass = 999; /* just in case the first character is a combiner */
    ix = 1;
    jx = ix;
    while (1)
    {
        if (jx >= len)
        {
            buf[pos] = curch;
            pos = ix;
            break;
        }
        newch = buf[jx];
        newclass = combining_class(newch);
        res = check_composition(curch, newch);
        if (res && (!curclass || curclass < newclass))
        {
            curch = res;
            buf[pos] = curch;
        }
        else
        {
            if (!newclass)
            {
                pos = ix;
                curch = newch;
            }
            curclass = newclass;
            buf[ix] = newch;
            ix++;
        }
        jx++;
    }

    return pos;
}

glui32 glk_buffer_canon_decompose_uni(glui32 *buf, glui32 len,
    glui32 numchars)
{
    glui32 *dest = gli_buffer_canon_decompose_uni(buf, &numchars);
    glui32 newlen;

    if (!dest)
        return 0;

    /* Copy the data back. */
    newlen = numchars;
    if (newlen > len)
        newlen = len;
    if (newlen)
        memcpy(buf, dest, newlen * sizeof(glui32));
    free(dest);

    return numchars;
}

glui32 glk_buffer_canon_normalize_uni(glui32 *buf, glui32 len,
    glui32 numchars)
{
    glui32 newlen;
    glui32 *dest = gli_buffer_canon_decompose_uni(buf, &numchars);

    if (!dest)
        return 0;

    numchars = gli_buffer_canon_compose_uni(dest, numchars);

    /* Copy the data back. */
    newlen = numchars;
    if (newlen > len)
        newlen = len;
    if (newlen)
        memcpy(buf, dest, newlen * sizeof(glui32));
    free(dest);

    return numchars;
}

#endif /* GLK_MODULE_UNICODE_NORM */


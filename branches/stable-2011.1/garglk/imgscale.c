/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
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

/*
 * Image scaling, based on pnmscale.c...
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "glk.h"
#include "garglk.h"

picture_t *
gli_picture_scale(picture_t *src, int newcols, int newrows)
{
    /* pnmscale.c - read a portable anymap and scale it
     *
     * Copyright (C) 1989, 1991 by Jef Poskanzer.
     *
     * Permission to use, copy, modify, and distribute this software and its
     * documentation for any purpose and without fee is hereby granted, provided
     * that the above copyright notice appear in all copies and that both that
     * copyright notice and this permission notice appear in supporting
     * documentation.  This software is provided "as is" without express or
     * implied warranty.
     */

#define SCALE 4096
#define HALFSCALE 2048
#define maxval 255

    picture_t *dst;

    dst = gli_picture_retrieve(src->id, 1);

    if (dst && dst->w == newcols && dst->h == newrows)
        return dst;

    unsigned char *xelrow;
    unsigned char *tempxelrow;
    unsigned char *newxelrow;
    register unsigned char *xP;
    register unsigned char *nxP;
    register int row, col;

    int rowsread, needtoreadrow;

    int cols = src->w;
    int rows = src->h;

    float xscale, yscale;
    long sxscale, syscale;

    register long fracrowtofill, fracrowleft;
    long *rs;
    long *gs;
    long *bs;
    long *as;

    /* Allocate destination image and scratch space */

    dst = malloc(sizeof(picture_t));
    dst->refcount = 1;
    dst->w = newcols;
    dst->h = newrows;
    dst->rgba = malloc(newcols * newrows * 4);
    dst->id = src->id;
    dst->scaled = TRUE;

    xelrow = src->rgba;
    newxelrow = dst->rgba;

    tempxelrow = malloc(cols * 4);
    rs = malloc((cols + 1) * sizeof(long));
    gs = malloc((cols + 1) * sizeof(long));
    bs = malloc((cols + 1) * sizeof(long));
    as = malloc((cols + 1) * sizeof(long));

    /* Compute all sizes and scales. */

    xscale = (float) newcols / (float) cols;
    yscale = (float) newrows / (float) rows;
    sxscale = xscale * SCALE;
    syscale = yscale * SCALE;

    rowsread = 1;
    fracrowleft = syscale;
    needtoreadrow = 0;

    for ( col = 0; col < cols; ++col )
        rs[col] = gs[col] = bs[col] = as[col] = HALFSCALE;
    fracrowtofill = SCALE;

    for ( row = 0; row < newrows; ++row )
    {
        /* First scale Y from xelrow into tempxelrow. */
        {
            while ( fracrowleft < fracrowtofill )
            {
                if ( needtoreadrow )
                    if ( rowsread < rows )
                    {
                        xelrow += src->w * 4;
                        ++rowsread;
                        /* needtoreadrow = 0; */
                    }

                for ( col = 0, xP = xelrow; col < cols; ++col, xP += 4 )
                {
                    rs[col] += fracrowleft * xP[0] * xP[3];
                    gs[col] += fracrowleft * xP[1] * xP[3];
                    bs[col] += fracrowleft * xP[2] * xP[3];
                    as[col] += fracrowleft * xP[3];
                }

                fracrowtofill -= fracrowleft;
                fracrowleft = syscale;
                needtoreadrow = 1;
            }

            /* Now fracrowleft is >= fracrowtofill, so we can produce a row. */
            if ( needtoreadrow )
                if ( rowsread < rows )
                {
                    xelrow += src->w * 4;
                    ++rowsread;
                    needtoreadrow = 0;
                }

            for ( col = 0, xP = xelrow, nxP = tempxelrow;
                    col < cols; ++col, xP += 4, nxP += 4)
            {
                register long r, g, b, a;
                r = rs[col] + fracrowtofill * xP[0] * xP[3];
                g = gs[col] + fracrowtofill * xP[1] * xP[3];
                b = bs[col] + fracrowtofill * xP[2] * xP[3];
                a = as[col] + fracrowtofill * xP[3];

                if (!a)
                {
                    r = g = b = a;
                }
                else
                {
                    r /= a;
                    if ( r > maxval ) r = maxval;
                    g /= a;
                    if ( g > maxval ) g = maxval;
                    b /= a;
                    if ( b > maxval ) b = maxval;
                    a /= SCALE;
                    if ( a > maxval ) a = maxval;
                }

                nxP[0] = r;
                nxP[1] = g;
                nxP[2] = b;
                nxP[3] = a;
                rs[col] = gs[col] = bs[col] = as[col] = HALFSCALE;
            }

            fracrowleft -= fracrowtofill;
            if ( fracrowleft == 0 )
            {
                fracrowleft = syscale;
                needtoreadrow = 1;
            }
            fracrowtofill = SCALE;
        }

        /* Now scale X from tempxelrow into newxelrow and write it out. */
        {
            register long r, g, b, a;
            register long fraccoltofill, fraccolleft;
            register int needcol;

            nxP = newxelrow;
            fraccoltofill = SCALE;
            r = g = b = a = HALFSCALE;
            needcol = 0;

            for ( col = 0, xP = tempxelrow; col < cols; ++col, xP += 4 )
            {
                fraccolleft = sxscale;
                while ( fraccolleft >= fraccoltofill )
                {
                    if ( needcol )
                    {
                        nxP += 4;
                        r = g = b = a = HALFSCALE;
                    }

                    r += fraccoltofill * xP[0] * xP[3];
                    g += fraccoltofill * xP[1] * xP[3];
                    b += fraccoltofill * xP[2] * xP[3];
                    a += fraccoltofill * xP[3];

                    if (!a)
                    {
                        r = g = b = a;
                    }
                    else
                    {
                        r /= a;
                        if ( r > maxval ) r = maxval;
                        g /= a;
                        if ( g > maxval ) g = maxval;
                        b /= a;
                        if ( b > maxval ) b = maxval;
                        a /= SCALE;
                        if ( a > maxval ) a = maxval;
                    }

                    nxP[0] = r;
                    nxP[1] = g;
                    nxP[2] = b;
                    nxP[3] = a;

                    fraccolleft -= fraccoltofill;
                    fraccoltofill = SCALE;
                    needcol = 1;
                }

                if ( fraccolleft > 0 )
                {
                    if ( needcol )
                    {
                        nxP += 4;
                        r = g = b = a = HALFSCALE;
                        needcol = 0;
                    }

                    r += fraccolleft * xP[0] * xP[3];
                    g += fraccolleft * xP[1] * xP[3];
                    b += fraccolleft * xP[2] * xP[3];
                    a += fraccolleft * xP[3];

                    fraccoltofill -= fraccolleft;
                }
            }

            if ( fraccoltofill > 0 )
            {
                xP -= 4;
                r += fraccoltofill * xP[0] * xP[3];
                g += fraccoltofill * xP[1] * xP[3];
                b += fraccoltofill * xP[2] * xP[3];
                a += fraccoltofill * xP[3];
            }

            if ( ! needcol )
            {
                if (!a)
                {
                    r = g = b = a;
                }
                else
                {
                    r /= a;
                    if ( r > maxval ) r = maxval;
                    g /= a;
                    if ( g > maxval ) g = maxval;
                    b /= a;
                    if ( b > maxval ) b = maxval;
                    a /= SCALE;
                    if ( a > maxval ) a = maxval;
                }

                nxP[0] = r;
                nxP[1] = g;
                nxP[2] = b;
                nxP[3] = a;
            }

            newxelrow += dst->w * 4;
        }
    }

    free(as);
    free(bs);
    free(gs);
    free(rs);
    free(tempxelrow);

    gli_picture_store(dst);

    return dst;
}

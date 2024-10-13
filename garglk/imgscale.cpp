// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2010 by Ben Cressey.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

// Image scaling, based on pnmscale.c...

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "glk.h"
#include "garglk.h"

#ifdef GARGLK_CONFIG_SCALERS
#include "hqx.h"
#include "xbrz.h"
#endif

std::shared_ptr<picture_t> gli_picture_scale(const picture_t *src, int newcols, int newrows)
{
    // pnmscale.c - read a portable anymap and scale it
    //
    // Copyright (C) 1989, 1991 by Jef Poskanzer.
    //
    // Permission to use, copy, modify, and distribute this software and its
    // documentation for any purpose and without fee is hereby granted, provided
    // that the above copyright notice appear in all copies and that both that
    // copyright notice and this permission notice appear in supporting
    // documentation.  This software is provided "as is" without express or
    // implied warranty.

    constexpr int SCALE = 4096;
    constexpr int HALFSCALE = 2048;
    constexpr int maxval = 255;

    auto dst = gli_picture_retrieve(src->id, true);

    if (dst && dst->w == newcols && dst->h == newrows) {
        return dst;
    }

#ifdef GARGLK_CONFIG_SCALERS
    int scaleby = std::ceil(std::max(static_cast<double>(newcols) / src->w, static_cast<double>(newrows) / src->h));

    dst.reset();

    if (scaleby > 1) {
        if (gli_conf_scaler == Scaler::HQX) {
            scaleby = std::min(scaleby, 4);

            static bool hqx_initialized = false;
            if (!hqx_initialized) {
                hqxInit();
                hqx_initialized = true;
            }

            auto hqx = scaleby == 4 ? hq4x_32 :
                       scaleby == 3 ? hq3x_32 :
                                      hq2x_32;

            Canvas<4> scaled_canvas(src->w * scaleby, src->h * scaleby);
            hqx(reinterpret_cast<const std::uint32_t *>(src->rgba.data()), reinterpret_cast<std::uint32_t *>(scaled_canvas.data()), src->w, src->h);
            dst = std::make_unique<picture_t>(src->id, std::move(scaled_canvas), true);
            src = dst.get();
        } else if (gli_conf_scaler == Scaler::XBRZ) {
            scaleby = std::min(scaleby, xbrz::SCALE_FACTOR_MAX);

            Canvas<4> scaled_canvas(src->w * scaleby, src->h * scaleby);
            if (xbrz::scale(scaleby, reinterpret_cast<const std::uint32_t *>(src->rgba.data()), reinterpret_cast<std::uint32_t *>(scaled_canvas.data()), src->w, src->h, xbrz::ColorFormat::ARGB)) {
                dst = std::make_unique<picture_t>(src->id, std::move(scaled_canvas), true);
                src = dst.get();
            }
        }
    }

    if (dst != nullptr && dst->w == newcols && dst->h == newrows) {
        gli_picture_store(dst);
        return dst;
    }
#endif

    int row, col;

    int rowsread;
    bool needtoreadrow;

    int cols = src->w;
    int rows = src->h;

    double xscale, yscale;
    long sxscale, syscale;

    long fracrowtofill, fracrowleft;

    // Allocate destination image and scratch space

    Canvas<4> rgba(newcols, newrows);

    std::vector<Pixel<4>> tempxelrow(cols, Pixel<4>(0, 0, 0, 0));
    std::vector<long> rs(cols, HALFSCALE);
    std::vector<long> gs(cols, HALFSCALE);
    std::vector<long> bs(cols, HALFSCALE);
    std::vector<long> as(cols, HALFSCALE);

    // Compute all sizes and scales.

    xscale = static_cast<double>(newcols) / static_cast<double>(cols);
    yscale = static_cast<double>(newrows) / static_cast<double>(rows);
    sxscale = xscale * SCALE;
    syscale = yscale * SCALE;

    rowsread = 1;
    fracrowleft = syscale;
    needtoreadrow = false;

    fracrowtofill = SCALE;

    for (row = 0; row < newrows; ++row) {
        // First scale Y from src->rgba into tempxelrow.
        {
            while (fracrowleft < fracrowtofill) {
                if (needtoreadrow && rowsread < rows) {
                    ++rowsread;
                }

                for (col = 0; col < cols; ++col) {
                    auto alpha = src->rgba[rowsread - 1][col][3];
                    rs[col] += fracrowleft * src->rgba[rowsread - 1][col][0] * alpha;
                    gs[col] += fracrowleft * src->rgba[rowsread - 1][col][1] * alpha;
                    bs[col] += fracrowleft * src->rgba[rowsread - 1][col][2] * alpha;
                    as[col] += fracrowleft * alpha;
                }

                fracrowtofill -= fracrowleft;
                fracrowleft = syscale;
                needtoreadrow = true;
            }

            // Now fracrowleft is >= fracrowtofill, so we can produce a row.
            if (needtoreadrow && rowsread < rows) {
                ++rowsread;
                needtoreadrow = false;
            }

            for (col = 0; col < cols; ++col) {
                auto alpha = src->rgba[rowsread - 1][col][3];
                long r, g, b, a;

                a = as[col] + fracrowtofill * alpha;

                if (a == 0) {
                    r = g = b = a;
                } else {
                    r = rs[col] + fracrowtofill * src->rgba[rowsread - 1][col][0] * alpha;
                    r /= a;
                    if (r > maxval) {
                        r = maxval;
                    }

                    g = gs[col] + fracrowtofill * src->rgba[rowsread - 1][col][1] * alpha;
                    g /= a;
                    if (g > maxval) {
                        g = maxval;
                    }

                    b = bs[col] + fracrowtofill * src->rgba[rowsread - 1][col][2] * alpha;
                    b /= a;
                    if (b > maxval) {
                        b = maxval;
                    }

                    a /= SCALE;
                    if (a > maxval) {
                        a = maxval;
                    }
                }

                tempxelrow[col] = Pixel<4>(r, g, b, a);
                rs[col] = gs[col] = bs[col] = as[col] = HALFSCALE;
            }

            fracrowleft -= fracrowtofill;
            if (fracrowleft == 0) {
                fracrowleft = syscale;
                needtoreadrow = true;
            }
            fracrowtofill = SCALE;
        }

        // Now scale X from tempxelrow into dst->rgba and write it out.
        {
            long r, g, b, a;
            long fraccoltofill, fraccolleft;
            bool needcol;

            fraccoltofill = SCALE;
            r = g = b = a = HALFSCALE;
            needcol = false;

            int dstcol = 0;
            for (col = 0; col < cols; ++col) {
                auto alpha = tempxelrow[col][3];
                auto tempxel_blended_r = tempxelrow[col][0] * alpha;
                auto tempxel_blended_g = tempxelrow[col][1] * alpha;
                auto tempxel_blended_b = tempxelrow[col][2] * alpha;

                fraccolleft = sxscale;
                while (fraccolleft >= fraccoltofill) {
                    if (needcol) {
                        dstcol++;
                        r = g = b = a = HALFSCALE;
                    }

                    a += fraccoltofill * alpha;

                    if (a == 0) {
                        r = g = b = a;
                    } else {
                        r += fraccoltofill * tempxel_blended_r;
                        r /= a;
                        if (r > maxval) {
                            r = maxval;
                        }

                        g += fraccoltofill * tempxel_blended_g;
                        g /= a;
                        if (g > maxval) {
                            g = maxval;
                        }

                        b += fraccoltofill * tempxel_blended_b;
                        b /= a;
                        if (b > maxval) {
                            b = maxval;
                        }

                        a /= SCALE;
                        if (a > maxval) {
                            a = maxval;
                        }
                    }

                    rgba[row][dstcol] = Pixel<4>(r, g, b, a);

                    fraccolleft -= fraccoltofill;
                    fraccoltofill = SCALE;
                    needcol = true;
                }

                if (fraccolleft > 0) {
                    if (needcol) {
                        dstcol++;
                        r = g = b = a = HALFSCALE;
                        needcol = false;
                    }

                    r += fraccolleft * tempxel_blended_r;
                    g += fraccolleft * tempxel_blended_g;
                    b += fraccolleft * tempxel_blended_b;
                    a += fraccolleft * alpha;

                    fraccoltofill -= fraccolleft;
                }
            }

            if (fraccoltofill > 0) {
                r += fraccoltofill * tempxelrow[cols - 1][0] * tempxelrow[cols - 1][3];
                g += fraccoltofill * tempxelrow[cols - 1][1] * tempxelrow[cols - 1][3];
                b += fraccoltofill * tempxelrow[cols - 1][2] * tempxelrow[cols - 1][3];
                a += fraccoltofill * tempxelrow[cols - 1][3];
            }

            if (!needcol) {
                if (a == 0) {
                    r = g = b = a;
                } else {
                    r /= a;
                    if (r > maxval) {
                        r = maxval;
                    }
                    g /= a;
                    if (g > maxval) {
                        g = maxval;
                    }
                    b /= a;
                    if (b > maxval) {
                        b = maxval;
                    }
                    a /= SCALE;
                    if (a > maxval) {
                        a = maxval;
                    }
                }

                rgba[row][dstcol] = Pixel<4>(r, g, b, a);
            }
        }
    }

    dst = std::make_shared<picture_t>(src->id, rgba, true);

    gli_picture_store(dst);

    return dst;
}

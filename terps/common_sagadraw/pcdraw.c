/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 *
 *	  Parts are based on code by David Lodge, which came with this notice:
 *
 *	  MIT License
 *
 *	  Copyright (c) 2018 D L
 *
 *	  Permission is hereby granted, free of charge, to any person obtaining a copy
 *	  of this software and associated documentation files (the "Software"), to deal
 *	  in the Software without restriction, including without limitation the rights
 *	  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	  copies of the Software, and to permit persons to whom the Software is
 *	  furnished to do so, subject to the following conditions:
 *
 *	  The above copyright notice and this permission notice shall be included in all
 *	  copies or substantial portions of the Software.
 *
 *	  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *	  SOFTWARE.
 */

//
//  pcdraw.c
//
//  Used by ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw IBM PC RLE bitmap graphics.
//  Based on Code by David Lodge.
//
//  Original code:
// https://github.com/tautology0/textadventuregraphics/blob/master/AdventureInternational/pcdraw.c
//

#include <stdint.h>
#include <stddef.h>

#include "glk.h"
#include "byteorder.h"
#include "sagadraw_host.h"

// Image header: offsets of its fields, and its length
#define DOS_HEADER_DATA_SIZE    0x05 // 16-bit length of the whole image
#define DOS_HEADER_STRIPED      0x0d // 0xff: double-width pixels
#define DOS_HEADER_START_OFFSET 0x0f // 16-bit CGA offset of the top left
#define DOS_HEADER_END_OFFSET   0x11 // 16-bit CGA offset of the bottom left
#define DOS_HEADER_WIDTH        0x13 // width in bytes, 4 pixels each
#define DOS_HEADER_LENGTH       0x17

// CGA bytes per scanline, in one of the two interleaved banks
#define CGA_BYTES_PER_ROW 80

// The image is stored interlaced, like CGA video memory: first the even
// scanlines from the top, then the odd ones. x and y are in pixels, `right`
// is the width in pixels and `bottom` the number of scanline pairs.
typedef struct {
    int x;
    int y;
    int right;
    int bottom;
    int left;
    int top;
    int ycount;      // scanline pairs done in the current pass
    int pixelwidth;
    int passes_done; // 0 while drawing even rows, 1 odd, 2 finished
} DrawState;

// Advance x by dx and handle end-of-line/last-line logic.
static inline void StepX(DrawState *st, int dx)
{
    st->x += dx;
    if (st->x >= st->right + st->left) {
        st->y += 2;
        st->x = st->left;
        st->ycount++;
    }
    if (st->ycount > st->bottom) {
        st->y = st->top + 1;
        st->passes_done++;
        st->ycount = 0;
    }
}

// Draw the 4 two-bit pixels packed in "pattern", each pixelwidth wide.
static inline void DrawPattern(DrawState *st, uint8_t pattern)
{
    uint8_t mask = 0xc0;

    for (int i = 6; i >= 0; i -= 2) {
        int color = (pattern & mask) >> i;
        PutPixelWithWidth(st->x, st->y, color, st->pixelwidth);
        StepX(st, st->pixelwidth);
        mask >>= 2;
    }
}

int DrawDOSImageFromData(uint8_t *ptr, size_t datasize)
{
    if (!ptr || datasize < DOS_HEADER_LENGTH)
        return 0;

    DrawState st = {0};

    // set up the palette
    const glui32 palette[4] = {
        0x000000, // black
        0x00ffff, // cyan
        0xff00ff, // magenta
        0xffffff  // white
    };

    for (int i = 0; i < 4; ++i) {
        SetColor(i, palette[i]);
    }

    // Get the size of the graphics chunk
    size_t size = READ_LE_UINT16(ptr + DOS_HEADER_DATA_SIZE);
    if (size > datasize)
        size = datasize;

    // Get whether it is striped
    st.pixelwidth = 1 + (ptr[DOS_HEADER_STRIPED] == 0xff);

    // Get the offset
    int rawoffset = READ_LE_UINT16(ptr + DOS_HEADER_START_OFFSET);
    st.left = ((rawoffset % CGA_BYTES_PER_ROW) * 4) - 24;
    st.top = rawoffset / 40;
    st.top -= (st.top & 1);
    st.x = st.left;
    st.y = st.top;

    // Get the height and width
    st.bottom = (READ_LE_UINT16(ptr + DOS_HEADER_END_OFFSET) - rawoffset) / CGA_BYTES_PER_ROW;
    st.right = ptr[DOS_HEADER_WIDTH] * 4;

    uint8_t *endptr = ptr + size;
    ptr += DOS_HEADER_LENGTH;

    while (ptr < endptr && st.passes_done < 2) {
        uint8_t count = *ptr++;
        int repeat = (count & 0x80) != 0;
        int reps = (count & 0x7f) + 1;

        if (repeat) {
            if (ptr >= endptr)
                break;
            uint8_t value = *ptr++;
            for (int i = 0; i < reps && st.passes_done < 2; ++i)
                DrawPattern(&st, value);
        } else {
            for (int i = 0; i < reps && ptr < endptr && st.passes_done < 2; ++i) {
                uint8_t value = *ptr++;
                DrawPattern(&st, value);
            }
        }
    }

    return 1;
}

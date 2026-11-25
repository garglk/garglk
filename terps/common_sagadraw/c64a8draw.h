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
//  c64a8draw.h
//
//  Used by ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw Atari 8-bit and C64 RLE bitmap graphics
//  Based on Code by David Lodge 29/04/2005
//
//  Original code at https://github.com/tautology0/textadventuregraphics

#ifndef c64a8draw_h
#define c64a8draw_h

#include <stddef.h>
#include <stdint.h>

// Which original interpreter an image was built for. It decides how the
// palette bytes translate into colours and, for The Count and Voodoo Castle,
// how the RLE data is laid out.
enum c64a8_variant {
    C64A8_ATARI8,              // Atari 8-bit, SagaPlus
    C64A8_ATARI8_COUNT_VOODOO, // Atari 8-bit, The Count and Voodoo Castle:
                               // every run is a count followed by two bytes,
                               // and the bottom edge is two rows too large
    C64A8_C64,                 // C64: Spider-Man, Buckaroo Banzai, Fantastic Four
    C64A8_C64_CLAYMORGUE,      // C64: Sorcerer of Claymorgue Castle
};

// The edges of an image as read from its header. left and right are in
// 8-pixel columns, top and bottom in rows. The host may change them before
// the image is drawn, typically to size its graphics window.
struct c64a8_bounds {
    int left;
    int top;
    int right;
    int bottom;
};

typedef void (*c64a8_adjust_fn)(struct c64a8_bounds *bounds);

int DrawC64A8ImageFromData(uint8_t *ptr, size_t datasize, c64a8_adjust_fn adjust, enum c64a8_variant variant);
// The colours the C64 variants draw with, by VIC-II colour number (for the
// tests, which look hardware screen dumps up in the palette the game uses).
extern const uint32_t c64colors[16];
// Draws one bitmap byte as four double-width pixels at (x, *y), two bits
// per pixel with the high pair leftmost, then moves *y down one row.
void DrawPatternAndAdvancePos(int x, int *y, uint8_t pattern);

#endif /* c64a8draw_h */

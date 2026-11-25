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
//  pcdraw.h
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

#ifndef pcdraw_h
#define pcdraw_h

#include <stddef.h>
#include <stdint.h>

// Draws a DOS (CGA) picture: the whole contents of a .PAK file. Returns 0 if
// the data is too short to hold the header, otherwise 1.
int DrawDOSImageFromData(uint8_t *ptr, size_t datasize);

#endif /* pcdraw_h */

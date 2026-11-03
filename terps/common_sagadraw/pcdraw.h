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

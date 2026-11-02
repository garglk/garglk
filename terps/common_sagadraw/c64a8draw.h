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
// Draws one bitmap byte as four double-width pixels at (x, *y), two bits
// per pixel with the high pair leftmost, then moves *y down one row.
void DrawPatternAndAdvancePos(int x, int *y, uint8_t pattern);

#endif /* c64a8draw_h */

//
//  alkatraz_screen.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Generic re-implementation of the Alkatraz Protection System's screen
//  address-order generator (the loader's "SCADDS" routine). The loader draws
//  a loading picture as a list of rectangular "windows", each a strip of
//  characters in one of four directions; simultaneous windows are drawn
//  byte-interleaved. Given a game's window descriptors this produces the exact
//  order in which the loader writes screen addresses, so the pixel bytes
//  streamed from tape can be placed correctly.
//

#ifndef alkatraz_screen_h
#define alkatraz_screen_h

#include <stdint.h>

/* Expand `nwin` 4-byte window descriptors into the screen-address draw order,
   writing up to `maxout` addresses into out[]. Returns the number written.

   Each descriptor is 4 bytes:
     [0..1] start screen address (little-endian)
     [2]    bits 6-7 = direction (0=right,1=left,2=up,3=down),
            bits 0-5 = width in characters
     [3]    bits 5-7 = (simultaneous windows in this group) - 1,
            bits 0-4 = height in characters (x8 = pixels)
   Bitmap (0x4000-0x57FF) and attribute (0x5800-0x5AFF) addresses are emitted
   interleaved, exactly as the loader's COLOUR/TABLE routines do. */
int AlkatrazScreenOrder(const uint8_t *descriptors, int nwin,
    uint16_t *out, int maxout);

#endif /* alkatraz_screen_h */

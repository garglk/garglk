//
//  aiukpalette.h
//  Part of the AI UK graphics library (shared by ScottFree and TaylorMade)
//
//  Created by Administrator on 2025-12-13.
//

#ifndef aiukpalette_h
#define aiukpalette_h

#include <stdint.h>

#include "glk.h"

/* DefinePalette() gives VGA, ZX and ZXOPT their own colours and
   everything else — the C64 variants, which differ only in how
   Remap() translates the Spectrum colours, but also NO_PALETTE —
   the C64 colours. */
typedef enum {
    NO_PALETTE,
    ZX,
    ZXOPT,
    C64A,
    C64B,
    C64C,
    C64D,
    C64QP3,
    C64TAYLOR,
    VGA,
    PALETTE_COUNT
} palette_type;

uint8_t Remap(uint8_t color);
void DefinePalette(void);

extern glui32 pal[16];
extern palette_type palchosen;

#endif /* aiukpalette_h */

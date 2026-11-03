//
//  aiukpalette.c
//  Part of the AI UK graphics library (shared by ScottFree and TaylorMade)
//
//  Created by Administrator on 2025-12-13.
//
//  Colour palette definitions and remapping for
//  Adventure International UK / Adventure Soft tile graphics.
//
//  The original games ran on two very different platforms — the ZX
//  Spectrum (8 colours × 2 brightness levels = 16 entries) and the
//  Commodore 64 (16 fixed colours, completely different ordering).
//  The image data always uses the Spectrum colour numbering (0–7
//  normal, 8–15 bright). On C64, a remap table translates those
//  indices into the closest C64 equivalents.
//
//  Several C64 remap variants exist because different game releases
//  chose slightly different colour substitutions.
//

#include <string.h>

#include "glk.h"
#include "aiukpalette.h"


/* The active 16-entry palette: pal[i] holds an 0xRRGGBB value. */
glui32 pal[16];

palette_type palchosen = NO_PALETTE;

/* The Spectrum-ordered palettes:
     0–7:  normal colours (black, blue, red, magenta, green, cyan, yellow, white)
     8–15: bright variants of the same colours */

static const glui32 vga_rgb[16] = {
    0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
    0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
};

/* Authentic Sinclair ZX Spectrum palette. The non-bright colours
   are noticeably dim (0x9A ≈ 60% intensity) compared to the
   bright variants — this matches real hardware output. */
static const glui32 zx_rgb[16] = {
    0x000000, 0x00009a, 0x9a0000, 0x9a009a, 0x009a00, 0x009a9a, 0x9a9a00, 0x9a9a9a,
    0x000000, 0x0000aa, 0xba0000, 0xce00ce, 0x00ce00, 0x00dfdf, 0xefef00, 0xffffff,
};

/* Brighter ZX palette matching the SPIN emulator's output.
   Less authentic than ZX but more visually appealing on
   modern displays. (The old David Lodge palette had 0xd6
   where the non-bright half has 0xca.) */
static const glui32 zxopt_rgb[16] = {
    0x000000, 0x0000ca, 0xca0000, 0xca00ca, 0x00ca00, 0x00caca, 0xcaca00, 0xcacaca,
    0x000000, 0x0000ff, 0xff0014, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
};

/* Commodore 64 palette based on pepto's analysis (used by the
   VICE emulator), in the C64's native colour order. That order has
   nothing in common with the Spectrum's, so Remap() translates
   Spectrum colour indices into C64 palette entries. */
static const glui32 c64_rgb[16] = {
    0x000000, /* black */
    0xffffff, /* white */
    0xbf6148, /* red */
    0x99e6f9, /* cyan */
    0xb159b9, /* purple */
    0x79d570, /* green */
    0x5f48e9, /* blue */
    0xf7ff6c, /* yellow */
    0xba8620, /* orange */
    0x746900, /* brown */
    0xe79a84, /* light red */
    0x454545, /* dark grey */
    0xa7a7a7, /* grey */
    0xc0ffb9, /* light green */
    0xa28fff, /* light blue */
    0xc8c8c8, /* light grey */
};

/* Populate the 16-entry pal[] array with RGB values for the chosen
   platform palette. Anything that is not VGA, ZX or ZXOPT gets the C64
   colours, NO_PALETTE included. */
void DefinePalette(void)
{
    const glui32 *rgb;

    switch (palchosen) {
    case VGA:
        rgb = vga_rgb;
        break;
    case ZX:
        rgb = zx_rgb;
        break;
    case ZXOPT:
        rgb = zxopt_rgb;
        break;
    default:
        rgb = c64_rgb;
        break;
    }
    memcpy(pal, rgb, sizeof(pal));
}

#define SPECTRUM_ORDER { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }

/* For every palette, the pal[] index of each of the 16 Spectrum colours
   (black, blue, red, magenta, green, cyan, yellow, white, then the
   bright ones). Several C64 tables exist because different game
   releases chose slightly different colour substitutions. */
static const uint8_t remap_tables[PALETTE_COUNT][16] = {
    [NO_PALETTE] = SPECTRUM_ORDER,
    [ZX] = SPECTRUM_ORDER,
    [ZXOPT] = SPECTRUM_ORDER,
    [VGA] = SPECTRUM_ORDER,

    /* Earliest C64 remap, determined from The Golden Baton.
       Also used by other early Mysterious Adventures (S1/S3/S13).
       These games only used 8 colours. */
    [C64A] = { 0, 6, 2, 4, 5, 3, 7, 1,
               8, 1, 1, 1, 7, 12, 8, 7 },

    /* Used by most C64 games released between the Mysterious
       Adventures and Seas of Blood (e.g. Gremlins, Secret Mission).
       The non-bright half maps differently from C64A. */
    [C64B] = { 0, 6, 9, 4, 5, 14, 8, 12,
               0, 6, 2, 4, 5, 3, 7, 1 },

    /* Determined from Spider-Man C64. Differs from C64B only in
       non-bright red, which is red (2) instead of brown (9). */
    [C64C] = { 0, 6, 2, 4, 5, 14, 8, 12,
               0, 6, 2, 4, 5, 3, 7, 1 },

    /* Determined from Seas of Blood C64. Non-bright yellow maps to
       orange (8) while bright yellow maps to yellow (7). */
    [C64D] = { 0, 6, 2, 4, 5, 3, 8, 1,
               0, 6, 2, 4, 5, 3, 7, 1 },

    /* Questprobe 3 (Fantastic Four) C64. Same as C64D (Seas of
       Blood) except non-bright yellow maps to yellow (7) instead
       of orange (8). */
    [C64QP3] = { 0, 6, 2, 4, 5, 3, 7, 1,
                 0, 6, 2, 4, 5, 3, 7, 1 },

    /* Used by TaylorMade C64 games (He-Man, Blizzard Pass,
       Rebel Planet). The bright half maps differently from
       the non-bright half, unlike C64D/C64QP3 which mirror. */
    [C64TAYLOR] = { 0, 6, 9, 4, 5, 12, 8, 15,
                    0, 14, 2, 10, 13, 3, 7, 1 },
};

/* Translate a Spectrum-format colour index (0–15) into the
   corresponding palette index for the active platform.

   For ZX/ZXOPT/VGA palettes the mapping is identity — the image data
   already uses Spectrum numbering. For C64 palettes, a remap table
   converts each Spectrum colour to its closest C64 equivalent.

   The result is always a valid index into pal[]: anything that is
   not a Spectrum colour comes out as 0 (black). */
uint8_t Remap(uint8_t color)
{
    if (color > 15 || (unsigned)palchosen >= PALETTE_COUNT)
        return 0;
    return remap_tables[palchosen][color];
}

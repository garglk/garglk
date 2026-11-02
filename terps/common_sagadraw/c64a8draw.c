//
//  c64a8draw.c
//
//  Atari 8-bit and Commodore 64 bitmap graphics for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Routines to draw Atari 8-bit and C64 RLE graphics
//  Based on Code by David Lodge 29/04/2005
//
//  Original code at https://github.com/tautology0/textadventuregraphics

#include "glk.h"
#include "read_le16.h"
#include "c64a8draw.h"
#include "sagadraw_host.h"

#include "atari8_palette.h"

// The original Atari interpreters write the colour values straight to the
// colour registers. Deliberate deviations from the palette, made by hand.
// Each comment gives the colour the palette has and the games that use the
// value.
struct atari8override {
    uint8_t value;
    glui32 rgb;
};

static const struct atari8override atari8overrides[] = {
    { 0x0e, 0xe0e0e0 }, // 0xd1d1d1: most games
    { 0x0f, 0xe0e0e0 }, // 0xd1d1d1: Count, Hulk, Voodoo Castle. Must match
                        // 0x0e, e.g. the Count inventory picture vs. the
                        // object overlays drawn on it
    { 0x12, 0xad5f64 }, // 0x521d00: Claymorgue
    { 0x25, 0xad5f64 }, // 0x762e00: Voodoo Castle
    { 0x32, 0xad5f64 }, // 0x54050b: Claymorgue, Hulk, Voodoo Castle
    { 0x34, 0xa23f40 }, // 0x712329: Claymorgue, Hulk
    { 0x36, 0xa23f40 }, // 0x8f4146: most games
    { 0x3a, 0xad5f64 }, // 0xcb7d82: Claymorgue, Hulk, Spider-Man
    { 0x56, 0x4b1ead }, // 0x693ccb: Claymorgue
    { 0x66, 0x7941c3 }, // 0x4f43ee: Count, Hulk
    { 0x85, 0x3468ee }, // 0x073bc5: Voodoo Castle
    { 0xc6, 0x088817 }, // 0x497600: Buckaroo Banzai, Claymorgue,
                        // Fantastic Four, Hulk, Spider-Man
    { 0xc7, 0x3a6700 }, // 0x497600: Claymorgue, Fantastic Four, Spider-Man
    { 0xc8, 0x088817 }, // 0x679405: Buckaroo Banzai, Hulk
    { 0xd8, 0x637000 }, // 0x818d00: Hulk
    { 0xe4, 0x944c02 }, // 0x5e4700: Claymorgue
    { 0xf7, 0xad5f64 }, // 0x8d5900: Count, Voodoo Castle
    { 0xf8, 0x8d5900 }, // 0xab7700: Claymorgue, Fantastic Four, Voodoo Castle
    { 0xff, 0xba8600 }, // 0xeed146: Claymorgue, Voodoo Castle
};

static void TranslateColorAtari8(uint8_t index, uint8_t value)
{
    for (size_t i = 0; i < sizeof(atari8overrides) / sizeof(atari8overrides[0]); i++) {
        if (atari8overrides[i].value == value) {
            SetColor(index, atari8overrides[i].rgb);
            return;
        }
    }
    // The GTIA ignores bit 0 of the luminance, so an odd value shows the
    // same colour as the even value below it.
    SetColor(index, ataricolors[value & 0xfe]);
}

// The 16 C64 colours, in the C64's own order
enum c64_color {
    C64_BLACK,
    C64_WHITE,
    C64_RED,
    C64_CYAN,
    C64_PURPLE,
    C64_GREEN,
    C64_BLUE,
    C64_YELLOW,
    C64_ORANGE,
    C64_BROWN,
    C64_LRED,
    C64_DGREY,
    C64_GREY,
    C64_LGREEN,
    C64_LBLUE,
    C64_LGREY
};

static const glui32 c64colors[16] = {
    [C64_BLACK]  = 0x000000,
    [C64_WHITE]  = 0xffffff,
    [C64_RED]    = 0xbf6148,
    [C64_CYAN]   = 0x99e6f9,
    [C64_PURPLE] = 0xb159b9,
    [C64_GREEN]  = 0x79d570,
    [C64_BLUE]   = 0x5f48e9,
    [C64_YELLOW] = 0xf7ff6c,
    [C64_ORANGE] = 0xba8620,
    [C64_BROWN]  = 0x837000,
    [C64_LRED]   = 0xe79a84,
    [C64_DGREY]  = 0x454545,
    [C64_GREY]   = 0xa7a7a7,
    [C64_LGREEN] = 0xc0ffb9,
    [C64_LBLUE]  = 0xa28fff,
    [C64_LGREY]  = 0xc8c8c8,
};

// The colour values in the image files are Atari 8-bit colours: hue in the
// high nibble, luminance in the low nibble. The original C64 interpreter
// (at $8C4A in the SagaPlus "prog" file) turns one into a C64 colour like
// this: luminance 0 is always black, otherwise the hue picks an entry from
// one table for luminance 1-8 and from another for luminance 9-15.
// The Claymorgue interpreter (at $7B71 in "saga.c64") has two entries
// different from the SagaPlus games.
static const uint8_t c64dark[2][16] = {
    { C64_BLACK, C64_GREEN, C64_BROWN, C64_ORANGE, C64_RED, C64_PURPLE, C64_PURPLE, C64_BLUE,
        C64_BLUE, C64_BLUE, C64_GREY, C64_GREEN, C64_GREEN, C64_GREEN, C64_YELLOW, C64_BROWN },
    { C64_WHITE, C64_GREEN, C64_BROWN, C64_ORANGE, C64_RED, C64_PURPLE, C64_PURPLE, C64_BLUE,
        C64_BLUE, C64_BLUE, C64_GREY, C64_GREEN, C64_GREEN, C64_GREEN, C64_YELLOW, C64_BROWN }
};

static const uint8_t c64bright[2][16] = {
    { C64_WHITE, C64_LGREEN, C64_YELLOW, C64_ORANGE, C64_LRED, C64_LRED, C64_PURPLE, C64_LBLUE,
        C64_WHITE, C64_LBLUE, C64_CYAN, C64_LGREEN, C64_LGREEN, C64_LGREEN, C64_YELLOW, C64_YELLOW },
    { C64_WHITE, C64_YELLOW, C64_YELLOW, C64_ORANGE, C64_LRED, C64_LRED, C64_PURPLE, C64_LBLUE,
        C64_WHITE, C64_LBLUE, C64_CYAN, C64_LGREEN, C64_LGREEN, C64_LGREEN, C64_YELLOW, C64_YELLOW }
};

// Deliberate deviations from the original colours, to make some pictures
// easier to read, one list for each interpreter. Each comment gives the
// colour the original draws.
struct c64override {
    uint8_t value;
    uint8_t color;
};

static const struct c64override c64overrides[2][8] = {
    {
        // Spider-Man, Buckaroo Banzai, Fantastic Four
        { 0x03, C64_WHITE },  // black: Buckaroo Banzai r024
        { 0x07, C64_BLUE },   // black: Buckaroo Banzai r014
        { 0x08, C64_BLUE },   // black: Spider-Man b053, b056
        { 0x11, C64_ORANGE }, // green: the Thing in Fantastic Four r013
        { 0x4d, C64_PURPLE }, // light red: Fantastic Four r022
        { 0x6e, C64_LBLUE },  // purple: Buckaroo Banzai r014
        { 0x89, C64_BLUE },   // white: the sky in Buckaroo Banzai r020
    },
    {
        // Sorcerer of Claymorgue Castle
        { 0x01, C64_BLUE },   // white: r13014
        { 0x12, C64_ORANGE }, // green: r13023, a wooden room
        { 0x15, C64_YELLOW }, // green: r13018, wooden planks
        { 0x18, C64_YELLOW }, // green: r13019, "LEAVE STARS HERE" in green
                              // on green
        { 0x2f, C64_ORANGE }, // yellow: r13035
        { 0x89, C64_BLUE },   // white: r13020, water on the kitchen floor
        { 0xff, C64_WHITE },  // yellow: b13030r
    }
};

static void TranslateColorC64(uint8_t index, uint8_t value, int claymorgue)
{
    // An entry with value 0 ends a list early. 0 is always black, so it
    // never needs an override.
    for (int i = 0; i < 8 && c64overrides[claymorgue][i].value != 0; i++) {
        if (c64overrides[claymorgue][i].value == value) {
            SetColor(index, c64colors[c64overrides[claymorgue][i].color]);
            return;
        }
    }

    int luminance = value & 0x0f;
    int hue = value >> 4;
    int color = C64_BLACK;
    if (luminance > 8)
        color = c64bright[claymorgue][hue];
    else if (luminance > 0)
        color = c64dark[claymorgue][hue];
    SetColor(index, c64colors[color]);
}

void DrawPatternAndAdvancePos(int x, int *y, uint8_t pattern)
{
    for (int shift = 6; shift >= 0; shift -= 2) {
        PutDoublePixel(x, *y, (pattern >> shift) & 3);
        x += 2;
    }
    (*y)++;
}

int DrawC64A8ImageFromData(uint8_t *ptr, size_t datasize, c64a8_adjust_fn adjust, enum c64a8_variant variant)
{
    uint8_t pattern_top = 0, pattern_bottom = 0;
    int repetitions;

    uint8_t *origptr = ptr;

    // The Count and Voodoo Castle: every run starts with a count byte,
    // 1-based, followed by the two bytes to repeat. The other games only do
    // that when bit 7 of the count is set; otherwise the count is the number
    // of literal byte pairs that follow, minus one.
    int counted_rle = variant == C64A8_ATARI8_COUNT_VOODOO;
    int c64 = variant == C64A8_C64 || variant == C64A8_C64_CLAYMORGUE;

    // Two bytes, the size, the edges and the palette
    if (datasize < 12)
        return 0;

    ptr += 2;

    // For Atari 8-bit, this value is already datasize
    // but on C64, datasize is the actual file size of the image
    // file on disk, so we shrink it here.
    size_t size = READ_LE_UINT16_AND_ADVANCE(&ptr);
    if (size < datasize - 2) {
        datasize = size + 2;
    }

    struct c64a8_bounds bounds;
    bounds.left = *ptr++ - 3;
    bounds.top = *ptr++;
    bounds.right = *ptr++;
    bounds.bottom = *ptr++;
    if (counted_rle)
        bounds.bottom -= 2;

    // Let the host (Plus or ScottFree) size its graphics window and
    // move the edges if it wants to.
    if (adjust != NULL)
        adjust(&bounds);

    int xpos = bounds.left * 8;
    int ypos = bounds.top;

    SetColor(0, 0); // Color 0 is always black

    // Get the palette
    for (int i = 1; i < 5; i++) {
        uint8_t color = *ptr++;
        if (c64) {
            TranslateColorC64(i, color, variant == C64A8_C64_CLAYMORGUE);
        } else {
            TranslateColorAtari8(i, color);
        }
    }

    while ((size_t)(ptr - origptr) < datasize - 2) {
        // First get repetitions
        repetitions = *ptr++;

        int repeat_next_two_bytes = 0;
        if (counted_rle) {
            repeat_next_two_bytes = 1;
            repetitions -= 1;
        } else if (repetitions & 0x80) {
            repeat_next_two_bytes = 1;
            repetitions &= 0x7f;
        }
        if (repeat_next_two_bytes) {
            pattern_top = *ptr++;
            pattern_bottom = *ptr++;
        }
        for (int i = 0; i < repetitions + 1; i++) {
            // If we are not repeating bytes,
            // we read two new bytes on every iteration
            if (!repeat_next_two_bytes) {
                if ((size_t)(ptr - origptr) >= datasize - 1)
                    return 1;
                pattern_top = *ptr++;
                pattern_bottom = *ptr++;
            }
            if (xpos <= (bounds.right - 3) * 8) {
                // Each byte pair is one column of 4 x 2 double-width pixels
                DrawPatternAndAdvancePos(xpos, &ypos, pattern_top);
                DrawPatternAndAdvancePos(xpos, &ypos, pattern_bottom);
                if (ypos > bounds.bottom) {
                    xpos += 8;
                    ypos = bounds.top;
                }
            }
        }
    }
    return 1;
}

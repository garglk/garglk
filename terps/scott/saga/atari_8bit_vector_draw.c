//
//  atari_8bit_vector_draw.c
//  scott
//
//  Created by Administrator on 2026-01-13.
//
//  Renderer for the Atari 8-bit vector pictures of the Scott Adams games. The
//  games' intro screens call the drawing code "Graphics Package for ATARI
//  S.A.G.A. developed by SCOTT ADAMS. Art by AD-VENTURE GRAPHICS". The pictures
//  were drawn with S.A.G.E., the Scott Adams Graphics Editor, which Adventure
//  International later sold (manual dated October 1983). The S.A.G.E. disk
//  includes the 6502 source of the drawing routine: GEDIT5.ASM ("MAIN GRAPHIC
//  ROUTINE FOR SAGA": command loop, colour pairs, fill patterns) and GEDIT4.ASM
//  (flood fill and line drawing). It is the routine the games use, except that
//  the editor's screens are at $7000/$8000. The opcode, pattern and variable
//  names below follow that source.
//  Not related to Penguin's Graphics Magician on the Apple II.
//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "apple2draw.h"
#include "common_file_utils.h"
#include "scott.h"
#include "vector_common.h"
#include "vector_oplist.h"

#include "atari_8bit_vector_draw.h"

#define A8_IMAGE_SIZE 0xf00 /* bytes per plane on the original: four pixels per byte */
#define A8_SCREEN_COLUMNS 40
#define A8_SCREEN_WIDTH 160
/* Width in output pixels: every Atari pixel is drawn two wide. */
#define A8_OUTPUT_WIDTH (A8_SCREEN_WIDTH * 2)
#define A8_SCREEN_HEIGHT 95
/* The planes hold 96 rows although A8_SCREEN_HEIGHT is 95: fills and lines can
   reach row 95, and the renderer draws it. */
#define A8_PIXEL_COUNT (A8_IMAGE_SIZE * 4)

typedef struct {
    uint8_t *imagedata;
    size_t datasize;

    // Screen memory offset in pixels: ypos * 160 + xpos, a linear index into
    // a8_pixels. Signed, as it goes negative above the top of the screen, and an
    // x past 159 spills into the next row. Anything outside 0..A8_PIXEL_COUNT-1
    // is off-screen.
    int pixel_offset;

    uint8_t color_a_index; // color A index used for pattern lookup
    uint8_t color_b_index; // color B index used for pattern lookup

    // The fill pattern as built by create_patterns(): a 2x2 tile of pixel values,
    // indexed [row parity][x parity]. Patterns 5..31 do not rebuild it, they
    // reuse the previous one.
    uint8_t tile[2][2];
    // The two scanline patterns the drawing routines write with, indexed
    // [0] = this scanline, [1] = the next one, then [x parity]. flood_fill()
    // swaps the rows as it descends, so [0] is not necessarily an even
    // scanline. A line is drawn with the solid colour in [0].
    uint8_t rows[2][2];
    // Lines-only mode: a line drawn in saved_pixel[0] is redrawn in saved_pixel[1].
    uint8_t saved_pixel[2];

    uint16_t pattern_start_pixel; // start offset in pixels for pattern fill

    uint8_t color_idx; // Current draw color
    uint8_t fill_background_color; // Any pixels of this color inside a shape will be replaced with the fill color/pattern by the flood fill routine.
    uint8_t fill_edge_color; // This is the color of "contours" that delimit the shapes that the flood fill routine will fill. The flood fill will never change this color.
    bool boundary_fill; // FFLAG in the S.A.G.E. source: fill over anything until the edge (line) color is met. Otherwise only the background color is replaced.
    bool lines_only_mode; // A special mode used to redraw the image lines on top in a second pass.

    // The segment the fill last painted: where it started (GEDIT4's LEFT, the
    // column the next row is entered at) and the column it stopped at (RIGHTX).
    uint16_t fill_left_pixoff;
    uint8_t fill_right_x;

    uint8_t start_x; // previously stored X position (for line drawing)
    uint8_t start_y; // previously stored Y position (for line drawing)
    uint8_t target_x; // current drawing target X coordinate
    uint8_t target_y; // current drawing target Y coordinate
    uint8_t ypos; // current y coordinate
    uint8_t xpos; // current x coordinate
} a8_draw_ctx;

/* The original draws on two screen planes, which GEDIT5 calls SCREEN1 and
   SCREEN2: $9000 and $A000 in the games ($7000 and $8000 in the editor). The
   display alternated them every frame to fake 16 colors, but nothing here reads
   one plane without the other. */
typedef enum {
    A8_SCREEN1 = 0,
    A8_SCREEN2 = 1,
} a8_screen;

/* The screen: one byte per pixel, holding the 4-bit hardware pixel value
   screen2 << 2 | screen1, the pixel's 2-bit values in the two planes. */
static uint8_t *a8_pixels = NULL;

/* The pixel value of each of the 16 color indices from the image data
   (the original's color_array, a pair of 2-bit values per index: SCREEN2,
   then SCREEN1). */
static const uint8_t a8_index_to_pixel[16] = {
    0xf, 0xe, 0xd, 0xc,
    0xa, 0x9, 0x8, 0x5,
    0x4, 0x0, 0xb, 0x7,
    0x3, 0x6, 0x2, 0x1
};

/* Indices come straight from the image data, so malformed data can point past
   the 16 colors; wrap them instead of reading past the end of the table. */
static inline uint8_t pixel_for_color_index(uint8_t index) {
    return a8_index_to_pixel[index & 0x0f];
}

typedef struct {
    uint16_t offset; // screen memory byte offset: pixel index / 4
    // The four pixels of that byte, as 4-bit pixel values: the first pixel in
    // the top nibble. All four are the fill color for a fill_bg op.
    uint16_t pixels;
    bool fill_bg;
    // Slow-draw timer ticks to pause for an A8_OP_DELAY op (see
    // atari8_delay_ticks()). Worst case is a few thousand ticks, so a uint16_t
    // holds it with room to spare and keeps the struct compact.
    uint16_t delay;
} a8_byte_to_write;

static VectorOpList write_ops = VECTOR_OPLIST(a8_byte_to_write);

/* Bytes emitted per slow-draw tick. Pairs with the graphics timer interval —
 * change in step with it. */
#define ATARI8_VECTOR_BYTES_PER_TICK 50

static inline bool is_valid_pixel_offset(int offset) {
    return offset >= 0 && offset < A8_PIXEL_COUNT;
}

static void push_op(uint16_t offset, uint16_t pixels, bool fill, uint16_t delay) {
    a8_byte_to_write *op = VectorOpListPush(&write_ops);
    op->offset = offset;
    op->pixels = pixels;
    op->fill_bg = fill;
    op->delay = delay;
}

/* Records that the screen byte containing the pixels at screen byte offset
   `offset` has been written, with the value it now holds. */
static void push_byte_op(uint16_t offset) {
    const uint8_t *p = &a8_pixels[offset * 4];
    push_op(offset, p[0] << 12 | p[1] << 8 | p[2] << 4 | p[3], false, 0);
}

static void fill_screen(uint8_t pixel) {
    memset(a8_pixels, pixel, A8_PIXEL_COUNT);
    push_op(0, pixel * 0x1111, true, 0);
}

static void push_delay_op(uint16_t ticks) {
    push_op(0, 0, false, ticks);
}

static void move_left(a8_draw_ctx *ctx) {
    ctx->xpos--;
    ctx->pixel_offset--;
}

static void move_right(a8_draw_ctx *ctx) {
    ctx->xpos++;
    ctx->pixel_offset++;
}

// Determine x and y coordinates from the pixel offset.
static void coordinates_from_pixel_offset(a8_draw_ctx *ctx) {
    ctx->ypos = ctx->pixel_offset / A8_SCREEN_WIDTH;
    ctx->xpos = ctx->pixel_offset % A8_SCREEN_WIDTH;
}

// Determine the pixel offset from x and y coordinates
static void offset_from_coordinates(a8_draw_ctx *ctx) {
    ctx->pixel_offset = ctx->ypos * A8_SCREEN_WIDTH + ctx->xpos;
}

static inline void plot_pixel(a8_draw_ctx *ctx) {
    if (!is_valid_pixel_offset(ctx->pixel_offset)) return;

    a8_pixels[ctx->pixel_offset] = ctx->rows[0][ctx->pixel_offset & 1];
    push_byte_op(ctx->pixel_offset >> 2);
}

static void draw_line(a8_draw_ctx *ctx) {
    int x0 = ctx->start_x, y0 = ctx->start_y;
    int x1 = ctx->target_x, y1 = ctx->target_y;
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    bool x_major = dy <= dx; /* GEDIT4 branches on DELTAX>=DELTAY */

    /* The minor axis only ever steps forward (down for x-major, right for
       y-major), so start from the endpoint with the smaller minor coordinate
       and step the major axis towards the other one, as GEDIT4 does. */
    if (x_major ? y1 < y0 : x1 < x0) {
        int t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
    }
    ctx->xpos = x0;
    ctx->ypos = y0;
    offset_from_coordinates(ctx);

    /* The loop only moves pixel_offset; nothing reads xpos/ypos after a line. */
    int major = x_major ? dx : dy;
    int minor = x_major ? dy : dx;
    int major_step = x_major ? (x1 < x0 ? -1 : 1)
                             : (y1 < y0 ? -A8_SCREEN_WIDTH : A8_SCREEN_WIDTH);
    int minor_step = x_major ? A8_SCREEN_WIDTH : 1;

    /* Bresenham. GEDIT4.ASM keeps the error in ERRLO/ERRHI and the doubled
       deltas in DXLO/DXHI and DYLO/DYHI, so the byte-sized endpoints never
       wrap it; plain int arithmetic plots the same pixels. */
    int error = 2 * minor - major;
    for (int i = 0; i < major; i++) {
        plot_pixel(ctx);
        if (error < 0) {
            error += 2 * minor;
        } else {
            ctx->pixel_offset += minor_step;
            error += 2 * (minor - major);
        }
        ctx->pixel_offset += major_step;
    }
    plot_pixel(ctx);
}

/*
 Pattern system overview:

 The Scott Adams Atari 8-bit vector draw code uses two screen memory planes ($9000 and $A000).
 Each byte holds four 2-bit pixels. The orignal code alternated between these on every screen update, giving the impression of a total of 16 colors (the color mixing being a side effect of the NTSC CRT hardware limitations, in combination with the perception limitations of the player.)
 We simulate this by storing each pixel as one 4-bit value, combining its two 2-bit values (SCREEN2 bits as high pair, SCREEN1 bits as low pair), and looking up the RGB color of the 16 possible values (a8_pixel_rgb()).

 On top of all this, additional colors are simulated by dithering.

 A dithering pattern is a 2x2 tile of two colors, A and B, built from the
 color_a_index and color_b_index of the last A8_OP_SET_COLOR. The tile is indexed
 by the parity of the scanline and of the x coordinate. Each pixel of it
 is either color A or color B, never a mix.

 Five pattern types (0-4) combine the colors in different ways.
*/

/* Which color (0 = A, 1 = B) goes where in the tile, [row parity][x parity].
   The names are the S.A.G.E. editor's.
   Pattern 0: SOLID (color A)
   Pattern 1: DIAMOND (alternating A/B, swapped on odd lines; wide Atari pixels
              make this pixel checkerboard look like diamonds)
   Pattern 2: HORIZONTAL (A on even, B on odd scanlines)
   Pattern 3: VERTICAL (alternating B/A on all lines)
   Pattern 4: CHECKERBOARD (solid A on even, alternating B/A on odd scanlines) */
static const uint8_t pattern_defs[5][2][2] = {
    {{0, 0}, {0, 0}},
    {{0, 1}, {1, 0}},
    {{0, 0}, {1, 1}},
    {{1, 0}, {1, 0}},
    {{0, 0}, {1, 0}},
};

/* Build the complete fill pattern from an opcode's parameters.
   Steps:
   1. Compute the seed pixel offset from the x/y arguments.
   2. Generate the pattern tile from color A and color B using the selected
      pattern type (0-4). Other types keep the previous tile.
   3. Copy the tile into the two scanline patterns that the drawing routines use. */
static void create_patterns(uint8_t pattern, uint8_t op_arg_a, uint8_t op_arg_b, a8_draw_ctx *ctx) {

    /* Convert the opcode's x/y arguments into a pixel offset for the flood fill
     seed point. The formula op_arg_b * 160 + op_arg_a computes the linear pixel
     position (160 pixels per scanline). */
    ctx->pattern_start_pixel = (op_arg_b << 7) + op_arg_b * 32 + op_arg_a;

    if (pattern < 5) {
        uint8_t color[2] = { pixel_for_color_index(ctx->color_a_index),
                             pixel_for_color_index(ctx->color_b_index) };
        for (int row = 0; row < 2; row++)
            for (int x = 0; x < 2; x++)
                ctx->tile[row][x] = color[pattern_defs[pattern][row][x]];
    }

    memcpy(ctx->rows, ctx->tile, sizeof ctx->rows);
}

// Pixels outside screen memory read as color 0 (and count as an edge in at_edge())
static uint8_t get_pixel_color(a8_draw_ctx *ctx) {
    if (!is_valid_pixel_offset(ctx->pixel_offset))
        return 0;
    return a8_pixels[ctx->pixel_offset];
}

static bool at_edge(a8_draw_ctx *ctx) {
    if (!is_valid_pixel_offset(ctx->pixel_offset))
        return true;
    uint8_t color = get_pixel_color(ctx);
    if (ctx->boundary_fill)
        return color == ctx->fill_edge_color;
    return color != ctx->fill_background_color;
}

// Scan straight up (one full row of pixels per step) until the border above the
// region is reached, or the top of the screen is passed (pixel_offset is signed
// so it goes negative there). Leaves pixel_offset on the topmost fillable row.
static void scan_up(a8_draw_ctx *ctx) {
    for (;;) {
        ctx->pixel_offset -= A8_SCREEN_WIDTH;
        if (ctx->pixel_offset < 0) return;
        if (at_edge(ctx))
            return;
    }
}

// From an edge pixel, scan right looking for a gap to continue the fill through.
// Returns false (and stops on it) as soon as a fillable pixel is found; returns
// true if the row stays blocked all the way to the screen edge or to the column
// where the segment above stopped (fill_right_x), meaning there is nothing more
// to fill below and the sweep should stop.
static bool scan_right(a8_draw_ctx *ctx) {
    for (;;) {
        move_right(ctx);
        if (ctx->xpos == A8_SCREEN_WIDTH || ctx->xpos == ctx->fill_right_x)
            return true;
        if (!at_edge(ctx))
            return false;
    }
}

static void swap_scanline_patterns(a8_draw_ctx *ctx) {
    uint8_t temp[2] = { ctx->rows[1][0], ctx->rows[1][1] };
    memcpy(ctx->rows[1], ctx->rows[0], sizeof ctx->rows[0]);
    memcpy(ctx->rows[0], temp, sizeof temp);
}

/* The byte the original wrote to the given screen plane for a scanline
   pattern: its two pixels repeated, four to a byte. */
static uint8_t plane_byte_of_row(const uint8_t row[2], a8_screen screen) {
    uint8_t a = (row[0] >> (screen * 2)) & 3;
    uint8_t b = (row[1] >> (screen * 2)) & 3;
    return a << 6 | b << 4 | a << 2 | b;
}

/* A Spatterlight shortcut with no counterpart in GEDIT4, which plots pixel by
   pixel: a plain fill can write a whole aligned byte (four pixels) at once when
   all four are still the background colour. */
static bool whole_byte_is_background(const a8_draw_ctx *ctx) {
    if ((ctx->pixel_offset & 3) != 0 || ctx->boundary_fill)
        return false;
    const uint8_t *byte = &a8_pixels[ctx->pixel_offset];
    for (int i = 0; i < 4; i++)
        if (byte[i] != ctx->fill_background_color)
            return false;
    return true;
}

/* Fill the region around the seed pixel with the current dither pattern,
   the way the original Atari S.A.G.A. graphics package does: scan straight up to the top
   border, then sweep downward one scanline at a time, filling the connected
   segment beneath the seed and dropping from its start column to the next line.

   A pixel is "fillable" until at_edge() says otherwise: normally that means
   anything still the background colour, or in a boundary fill anything that is not
   the contour (edge) colour. Positions are tracked as a pixel offset and as
   x/y, kept in sync by the move_* helpers. */
static void flood_fill(a8_draw_ctx *ctx) {
    ctx->pixel_offset = ctx->pattern_start_pixel;
    ctx->fill_background_color = get_pixel_color(ctx);

    /* Find top of unfilled region */
    scan_up(ctx);

    /* Move down one line to last non-filled pixel */
    ctx->pixel_offset += A8_SCREEN_WIDTH;
    coordinates_from_pixel_offset(ctx);

    /* Determine whether the two scanline patterns need swapping based on
       the parity of the starting line. On even lines the "first" pattern
       is the even one; on odd lines it is the odd one. We compare the first
       against the second and swap if the signed difference is negative.
       GEDIT4 compares the bytes it wrote to the screen planes (C1..C4) and
       swaps "SO THAT COLORS MATCH UP ROW BY ROW", so we compute them. rows[0]
       is the "even" pattern here and rows[1] the "odd" one, whatever the parity
       of the starting line. */
    const uint8_t *first = ctx->rows[ctx->ypos & 1];
    const uint8_t *second = ctx->rows[(ctx->ypos & 1) ^ 1];
    uint8_t first_screen1 = plane_byte_of_row(first, A8_SCREEN1);
    uint8_t second_screen1 = plane_byte_of_row(second, A8_SCREEN1);
    uint8_t first_screen2 = plane_byte_of_row(first, A8_SCREEN2);
    uint8_t second_screen2 = plane_byte_of_row(second, A8_SCREEN2);
    bool need_swap = (first_screen1 == second_screen1)
        ? (int8_t)(second_screen2 - first_screen2) < 0
        : (int8_t)(second_screen1 - first_screen1) < 0;
    if (need_swap) {
        swap_scanline_patterns(ctx);
    }

    /* Descend one scanline at a time, filling the connected segment beneath the
       seed. */
    for (;;) {
        /* Walk left to the segment's left border and stop on its first fillable
           pixel (GEDIT4 steps 4-8). */
        while (ctx->xpos > 0) {
            move_left(ctx);
            if (at_edge(ctx)) {
                move_right(ctx);
                break;
            }
        }
        ctx->fill_left_pixoff = ctx->pixel_offset;

        /* Fill the segment left -> right until the edge or the end of the row
           (steps 9-10). */
        do {
            if (!is_valid_pixel_offset(ctx->pixel_offset)) {
                fprintf(stderr, "flood_fill: Address out of bounds!\n");
                return;
            }
            if (whole_byte_is_background(ctx)) {
                uint8_t *byte = &a8_pixels[ctx->pixel_offset];
                byte[0] = byte[2] = ctx->rows[0][0];
                byte[1] = byte[3] = ctx->rows[0][1];
                push_byte_op(ctx->pixel_offset >> 2);
                ctx->xpos += 4;
                ctx->pixel_offset += 4;
            } else {
                if (at_edge(ctx))
                    break;
                plot_pixel(ctx);
                move_right(ctx);
            }
        } while (ctx->xpos != A8_SCREEN_WIDTH);

        /* Drop to the next line at the segment's start column, alternating the
           dither patterns. Stop at the bottom of the screen, or when the line
           below is blocked from the start column rightward (steps 11-19). */
        swap_scanline_patterns(ctx);
        ctx->fill_right_x = ctx->xpos;
        ctx->pixel_offset = ctx->fill_left_pixoff + A8_SCREEN_WIDTH;
        if (ctx->ypos == A8_SCREEN_HEIGHT)
            return;
        coordinates_from_pixel_offset(ctx);
        if (at_edge(ctx) && scan_right(ctx) == true)
            return;
    }
}

/* The RGB colour of each 4-bit pixel value (SCREEN2 bits high, SCREEN1 bits
   low): what the eye sees when the display alternates the two planes' colour
   registers every frame. The games never change the registers ($42,$B4,$E4
   on SCREEN1, $32,$84,$D4 on SCREEN2), and the only statement of how the
   mixes should look is the colour test picture (image 250), whose six blocks
   the engine labels WHITE BROWN ORANGE YELLOW BLUE GREEN and asks the player
   to tune the TV to. test/a8_palette_fit.py solves the six registers so
   that those blocks land on their names (linear-light mean of the two
   frames, BROWN being a checkerboard of ORANGE and YELLOW) and derives the
   other ten mixes from the same registers. The S.A.G.E. editor numbers the
   register pairs 0-15 in GEDIT5's COLORS table (10-15 are its "object
   colours") and never names a hue; the hue words are ours, the five from
   the test picture excepted. */
static const uint32_t a8_rgb_of_pixel[16] = {
    /*  0: SCREEN1 0 SCREEN2 0  S.A.G.E.  9  black */ 0x000000,
    /*  1: SCREEN1 1 SCREEN2 0  S.A.G.E. 15  burnt orange, same as 4 */ 0xbc3800,
    /*  2: SCREEN1 2 SCREEN2 0  S.A.G.E. 14  dark teal */ 0x006d71,
    /*  3: SCREEN1 3 SCREEN2 0  S.A.G.E. 12  mustard */ 0xbca839,
    /*  4: SCREEN1 0 SCREEN2 1  S.A.G.E.  8  burnt orange, same as 1 */ 0xbc3800,
    /*  5: SCREEN1 1 SCREEN2 1  S.A.G.E.  7  orange (test picture) */ 0xff5000,
    /*  6: SCREEN1 2 SCREEN2 1  S.A.G.E. 13  rose */ 0xbc7971,
    /*  7: SCREEN1 3 SCREEN2 1  S.A.G.E. 11  amber */ 0xffaf39,
    /*  8: SCREEN1 0 SCREEN2 2  S.A.G.E.  6  royal blue */ 0x3165bc,
    /*  9: SCREEN1 1 SCREEN2 2  S.A.G.E.  5  orchid */ 0xc172bc,
    /* 10: SCREEN1 2 SCREEN2 2  S.A.G.E.  4  blue (test picture) */ 0x3191d5,
    /* 11: SCREEN1 3 SCREEN2 2  S.A.G.E. 10  white (test picture) */ 0xc1bfc2,
    /* 12: SCREEN1 0 SCREEN2 3  S.A.G.E.  3  chartreuse */ 0x95ac00,
    /* 13: SCREEN1 1 SCREEN2 3  S.A.G.E.  2  gold */ 0xe7b300,
    /* 14: SCREEN1 2 SCREEN2 3  S.A.G.E.  1  green (test picture) */ 0x95c671,
    /* 15: SCREEN1 3 SCREEN2 3  S.A.G.E.  0  yellow (test picture) */ 0xe7e739,
};

static inline uint32_t a8_pixel_rgb(uint8_t pixel) {
    return a8_rgb_of_pixel[pixel];
}

// Draws the four pixels of one screen memory byte
static void DrawSingleByteAt(uint16_t pixels, glsi32 x, glsi32 y) {
    for (int shift = 12; shift >= 0; shift -= 4) {
        SetColor(1, a8_pixel_rgb((pixels >> shift) & 0xf));
        PutDoublePixel(x, y, 1);
        x += 2;
    }
}

static void DrawAtari8bitImageFromScreenmem(void) {
    glsi32 x = 0;
    glsi32 y = 0;
    for (int i = 0; i < A8_PIXEL_COUNT; i += 4) {
        const uint8_t *p = &a8_pixels[i];
        DrawSingleByteAt(p[0] << 12 | p[1] << 8 | p[2] << 4 | p[3], x, y);
        x += 8;
        if (x >= A8_OUTPUT_WIDTH) {
            x = 0;
            y++;
        }
    }
}

typedef enum  {
    A8_OP_END = 0,
    A8_OP_SET_BACKGROUND_OR_MODE = 1, // S.A.G.E.: 0 = quick clear, 1 = swap line color
    A8_OP_DELAY = 2,                  // S.A.G.E.: wait
    A8_OP_SET_COLOR = 3,
    A8_OP_MOVE_TO = 4,                // S.A.G.E.: new line
    A8_OP_DRAW_LINE = 5,
    A8_OP_BOUNDARY_FILL = 6,          // S.A.G.E.: fill until line color met ("U" key)
    A8_OP_FILL = 7,                   // S.A.G.E.: regular fill ("F" key)
} Opcode;

/* A8_OP_DELAY (2) faithful timing.
 *
 * On the Atari the delay opcode is a triple-nested 6502 busy-wait in MAIN_DRAW
 * ($86DB): DEC arg_a / DEC arg_b / DEC arg0 act as the inner / middle / outer
 * loop counters (~9 cycles per inner pass: DEC abs 6 + BNE taken 3), and the
 * whole thing is skipped in lines-only mode. With the 6502 "0 means 256" wrap,
 * the number of inner iterations is
 *
 *     inner = cA + (cB - 1) * 256 + (cO - 1) * 65536
 *
 * where cA = arg_a?:256, cB = arg_b?:256, cO = arg0?:256. Every shipped image
 * uses arg_a == arg_b == 0, so this reduces to arg0 * 65536 (~0.34 s per arg0
 * unit), but we honour all three operands regardless.
 *
 * We convert that cycle count into slow-draw timer ticks so the pause lasts the
 * same real time the hardware waited. The busy-wait runs at ~1.74 MHz (1.79 MHz
 * NTSC minus ANTIC DMA; measured in MAME across several images), and the
 * graphics timer fires every TimerDelay() ms, hence
 *
 *     ticks = inner * 9 / (1740 * TimerDelay_ms).
 */
#define A8_DELAY_CYCLES_PER_INNER 9
#define A8_DELAY_CYCLES_PER_MS    1740 /* effective 6502 rate during the wait */

static uint16_t atari8_delay_ticks(uint8_t arg0, uint8_t arg_a, uint8_t arg_b) {
    uint32_t cA = arg_a ? arg_a : 256;
    uint32_t cB = arg_b ? arg_b : 256;
    uint32_t cO = arg0  ? arg0  : 256;
    uint32_t inner = cA + (cB - 1) * 256 + (cO - 1) * 65536;
    uint32_t cycles = inner * A8_DELAY_CYCLES_PER_INNER;
    uint32_t divisor = (uint32_t)A8_DELAY_CYCLES_PER_MS * (uint32_t)TimerDelay();
    uint32_t ticks = (cycles + divisor / 2) / divisor; /* round to nearest tick */
    return ticks > UINT16_MAX ? UINT16_MAX : (uint16_t)ticks;
}


static void handle_draw_line(a8_draw_ctx *ctx) {
    /* Draw the line in a solid color */
    uint8_t line_pixel = pixel_for_color_index(ctx->color_idx);
    ctx->rows[0][0] = ctx->rows[0][1] = line_pixel;

    if (ctx->lines_only_mode == true) {
        /* Use saved colors in lines-only mode */
        /* Only draw line if its color matches saved color A,
           and then draw it in saved color B */
        if (line_pixel == ctx->saved_pixel[0]) {
            ctx->rows[0][0] = ctx->rows[0][1] = ctx->saved_pixel[1];
            draw_line(ctx);
        }
    } else {
        draw_line(ctx);
    }
    ctx->start_x = ctx->target_x;
    ctx->start_y = ctx->target_y;
}

static bool handle_fill_or_set_background(a8_draw_ctx *ctx,
                                          Opcode opcode,
                                          uint8_t arg_0,
                                          uint8_t arg_a, uint8_t arg_b) {
    ctx->boundary_fill = false;

    switch (opcode) {
        case A8_OP_BOUNDARY_FILL:
            if (ctx->lines_only_mode) return false;
            ctx->boundary_fill = true;
            ctx->fill_edge_color = pixel_for_color_index(ctx->color_idx);
            // fallthrough
        case A8_OP_FILL:
            if (ctx->lines_only_mode) return false;
            create_patterns(arg_0, arg_a, arg_b, ctx);
            flood_fill(ctx);
            break;
        case A8_OP_SET_BACKGROUND_OR_MODE:
            /* If opcode_arg == 0 do an immediate background fill with pattern 0
             (unless in lines-only mode) */
            if (arg_0 == 0) {
                if (ctx->lines_only_mode) return false;
                create_patterns(0, arg_a, arg_b, ctx);
                fill_screen(ctx->tile[0][0]);
                /* The "teleport animation" in Pirate Adventure, which mostly just does a
                   number of full-screen color changes in the Atari 8-bit version,
                   needs an extra delay in order to be noticeable */
                if (Game != NULL && CurrentGame == PIRATE_US && vector_image_shown == 90)
                    push_delay_op(10);
            } else {
                /* If arg_0 == 1 we store the current pattern and then redraw
                 the entire image in lines-only mode on top of the current one. */
                /* (This is a common trick for drawing room objects such as the dragon
                 on top of background images: draw a colored contour, fill it in boundary
                 mode, then draw a black contour on top in lines-only mode.) */

                /* If lines-only mode is aready on,
                   we switch it off instead. */
                ctx->lines_only_mode = !ctx->lines_only_mode;
                if (ctx->lines_only_mode) {
                    create_patterns(2, arg_a, arg_b, ctx);
                    ctx->saved_pixel[0] = ctx->tile[0][0];
                    ctx->saved_pixel[1] = ctx->tile[1][0];
                }
                return ctx->lines_only_mode;
            }
            break;
        default:
            fprintf(stderr, "Unhandled opcode: %d\n", opcode);
            break;
    }

    return false;
}

static bool process_opcode(a8_draw_ctx *ctx, Opcode opcode, uint8_t arg0, uint8_t arg_a, uint8_t arg_b) {
    bool start_over = false;
    switch (opcode) {
        case A8_OP_MOVE_TO:
            ctx->start_x = arg_a;
            ctx->start_y = arg_b;
            break;
        case A8_OP_SET_COLOR:
            ctx->color_idx = arg0;
            ctx->color_a_index = arg_a;
            ctx->color_b_index = arg_b;
            break;
        case A8_OP_DELAY:
            if (ctx->lines_only_mode == false) {
                uint16_t ticks = atari8_delay_ticks(arg0, arg_a, arg_b);
                debug_print("Delay arg0=%d arg_a=%d arg_b=%d -> %d ticks\n",
                            arg0, arg_a, arg_b, ticks);
                push_delay_op(ticks);
            }
            break;
        case A8_OP_DRAW_LINE:
            ctx->target_x = arg_a;
            ctx->target_y = arg_b;
            handle_draw_line(ctx);
            break;
        default:
            start_over = handle_fill_or_set_background(ctx, opcode, arg0, arg_a, arg_b);
            break;
    }
    return start_over;
}

static void DrawSingleAtari8ImageByte(const a8_byte_to_write *towrite) {
    if (towrite->fill_bg == true) {
        SetColor(1, a8_pixel_rgb(towrite->pixels & 0xf));
        RectFill(0, 0, ImageWidth, ImageHeight, 1);
    } else {
        glsi32 y = towrite->offset / A8_SCREEN_COLUMNS;
        glsi32 x = (towrite->offset % A8_SCREEN_COLUMNS) * 8;
        DrawSingleByteAt(towrite->pixels, x, y);
    }
}

static int delay_active = 0;

static VectorStepResult draw_or_delay(const void *op, size_t index) {
    (void)index;
    const a8_byte_to_write *towrite = op;
    if (towrite->delay > 0) {
        delay_active = towrite->delay;
        debug_print("DrawSomeAtari8VectorBytes: initiating a delay of %d\n", delay_active);
        return VECTOR_STEP_PAUSE;
    }
    DrawSingleAtari8ImageByte(towrite);
    return VECTOR_STEP_NEXT;
}

void DrawSomeAtari8VectorBytes(int from_start)
{
    if (from_start) {
        delay_active = 0;
    } else if (gli_slowdraw && delay_active > 0) {
        delay_active--;
        return;
    }
    VectorOpListPlayChunk(&write_ops, from_start, ATARI8_VECTOR_BYTES_PER_TICK,
        draw_or_delay, DrawAtari8bitImageFromScreenmem);
}

int DrawingAtari8Vector(void) {
    return VectorOpListDrawing(&write_ops);
}

uint8_t *DrawAtari8BitVectorImage(USImage *img) {

    if (!img || img->imagedata == NULL || img->datasize < 2)
        return NULL;

    a8_draw_ctx ctx = {};

    ctx.imagedata = img->imagedata;
    ctx.datasize = img->datasize;

    ctx.color_idx = 10; // initial color index. (SCREEN2 2, SCREEN1 3)

    if (a8_pixels == NULL)
        a8_pixels = MemCalloc(A8_PIXEL_COUNT);

    if (!VectorBeginImage(&write_ops, img, DrawAtari8bitImageFromScreenmem))
        return NULL;
    if (img->usage == IMG_ROOM)
        fill_screen(0);

    uint16_t offset = 0;

    while (offset + 2 < ctx.datasize) {
        Opcode opcode = ctx.imagedata[offset] >> 5;
        if (opcode == A8_OP_END) break;

        uint8_t arg_0 = ctx.imagedata[offset] & 0x1f;
        uint8_t arg_a = ctx.imagedata[offset + 1];
        uint8_t arg_b = ctx.imagedata[offset + 2];

        bool start_over = process_opcode(&ctx, opcode, arg_0, arg_a, arg_b);

        if (start_over) {
            offset = 0;
        } else {
            offset += 3;
        }
    }
    return ctx.imagedata + offset;
}

static int ReadAndDrawImageWithName(const char *name, const char *supportpath, USImageType usage) {
    USImage *image = LoadTestImage("atari8", name, supportpath);
    if (!image)
        return 0;
    image->usage = usage;
    DrawAtari8BitVectorImage(image);
    FreeTestImage(image);
    return 1;
}

/* The screen as the two byte planes the golden files hold (named .90 and .a0
   after the games' addresses). For byte i, the pixel in slot s (0..3) has its
   SCREEN1 bits at 6 - 2s of screen1_plane[i] and its SCREEN2 bits at the same
   place in screen2_plane[i]. */
static uint8_t screen1_plane[A8_IMAGE_SIZE];
static uint8_t screen2_plane[A8_IMAGE_SIZE];

static void pack_planes(void) {
    memset(screen1_plane, 0, sizeof screen1_plane);
    memset(screen2_plane, 0, sizeof screen2_plane);
    if (a8_pixels == NULL)
        return;
    for (int i = 0; i < A8_PIXEL_COUNT; i++) {
        int shift = 6 - 2 * (i & 3);
        screen1_plane[i >> 2] |= (a8_pixels[i] & 3) << shift;
        screen2_plane[i >> 2] |= (a8_pixels[i] >> 2) << shift;
    }
}

const uint8_t *GetAtari8Screen1Plane(void) { pack_planes(); return screen1_plane; }
const uint8_t *GetAtari8Screen2Plane(void) { pack_planes(); return screen2_plane; }

int TestAtari8ImageWithName(const char *name, const char *supportpath) {
    if (!ReadAndDrawImageWithName(name, supportpath, IMG_ROOM))
        return 0;
    pack_planes();
    if (!CompareWithTestFile("atari8", name, "90.result", supportpath, screen1_plane, A8_IMAGE_SIZE))
        return 0;
    return CompareWithTestFile("atari8", name, "A0.result", supportpath, screen2_plane, A8_IMAGE_SIZE);
}

int RunAtari8bitVectorTests(const char *supportpath) {

    gli_slowdraw = 0;

    if (!TestAtari8ImageWithName("mi_boom", supportpath))
        return 0;
    if (!TestAtari8ImageWithName("so_console", supportpath))
        return 0;
    if (!TestAtari8ImageWithName("so_painting", supportpath))
        return 0;

    if (!ReadAndDrawImageWithName("so_hexagonal", supportpath, IMG_ROOM))
        return 0;
    if (!ReadAndDrawImageWithName("so_rod", supportpath, IMG_INV_AND_ROOM_OBJ))
        return 0;
    if (!ReadAndDrawImageWithName("so_tint", supportpath, IMG_INV_AND_ROOM_OBJ))
        return 0;

    pack_planes();
    if (!CompareWithTestFile("tint", "90", ".result", supportpath, screen1_plane, A8_IMAGE_SIZE))
        return 0;
    return CompareWithTestFile("tint", "a0", ".result", supportpath, screen2_plane, A8_IMAGE_SIZE);
}

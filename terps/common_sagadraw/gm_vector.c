//
//  gm_vector.c
//
//  Shared Apple II Graphics Magician vector primitives. See gm_vector.h.
//
//  Both renderers that use this code are validated pixel-exact against
//  hardware captures, so it must not change behaviour: the cursor, line and
//  brush code reproduce the 6502 routines write for write, including their
//  byte-sized wraps at the screen edges.
//

#include "gm_vector.h"

#define BRUSH_PART_SIZE   8
#define BRUSH_TOTAL_PARTS 4
#define BRUSH_PART_HEIGHT 8

void gm_spread7(uint8_t src, uint8_t offset, uint8_t *mask_a, uint8_t *mask_b)
{
    // Rotate the 7-bit source left `offset` times, collecting bit 6 into the
    // carry byte that spills into the next column.
    uint8_t carry = 0;
    for (uint8_t n = offset; n != 0; n--) {
        carry = (uint8_t)((carry << 1) | ((src & 0x7f) >> 6));
        src = (uint8_t)(src << 1);
    }
    *mask_a = src & 0x7f; // bits painted into column col
    *mask_b = carry;      // bits carried into column col+1
}

// A coloured pattern (bits 5 and 6 differ) alternates between two bytes on
// even and odd columns, so stepping into the neighbouring column inverts its
// pixel bits. Black and white patterns are the same in every column.
static void flip_color_phase(gm_vector_ctx *c)
{
    if ((c->HGR_BITS * 2 + 0x40) & 0x80)
        c->HGR_BITS ^= 0x7f;
}

void gm_set_color(uint8_t color_index, gm_vector_ctx *c)
{
    static const uint8_t COLORTBL[8] = {0x00, 0x2a, 0x55, 0x7f, 0x80, 0xaa, 0xd5, 0xff};
    if (color_index < 8)
        c->HGR_COLOR = COLORTBL[color_index];
}

void gm_set_draw_position(uint16_t xpos, uint8_t ypos, gm_vector_ctx *c)
{
    c->HGR_X = xpos;
    c->HGR_Y = ypos;
    c->HBAS = gm_row_address(ypos);
    c->HGR_HORIZ = xpos / GM_PIXELS_PER_BYTE;
    static const uint8_t masktable[GM_PIXELS_PER_BYTE] = {0x81, 0x82, 0x84, 0x88, 0x90, 0xa0, 0xc0};
    c->HMASK = masktable[xpos % GM_PIXELS_PER_BYTE];
    c->HGR_BITS = c->HGR_COLOR;
    if ((c->HGR_HORIZ & 1) == 1)
        flip_color_phase(c);
}

static void move_left(gm_vector_ctx *c)
{
    if ((c->HMASK & 1) == 0) {
        // Shift mask right, moves dot left. Move sign bit back where it was.
        c->HMASK = c->HMASK >> 1 ^ 0xc0;
        return;
    }
    // moved to next byte, so decrease index
    c->HGR_HORIZ--;
    if ((int8_t)c->HGR_HORIZ < 0) {
        // off the left edge, wrap around to the right edge
        c->HGR_HORIZ = GM_HIRES_COLS - 1;
    }
    c->HMASK = 0xc0;
    flip_color_phase(c);
}

static void move_right(gm_vector_ctx *c)
{
    // shifting byte left moves pixel right
    c->HMASK = c->HMASK << 1 ^ 0x80;
    if ((int8_t)c->HMASK < 0) {
        return;
    }
    c->HMASK = 0x81;
    c->HGR_HORIZ++;
    if (c->HGR_HORIZ >= GM_HIRES_COLS) {
        // off the right edge, wrap around to the left edge
        c->HGR_HORIZ = 0;
    }
    flip_color_phase(c);
}

static void move_up_or_down(bool down, gm_vector_ctx *c)
{
    if (down) {
        c->HGR_Y++;
        if (c->HGR_Y >= GM_HIRES_ROWS)
            c->HGR_Y = 0;
    } else {
        c->HGR_Y--;
        // Note: `(int8_t)HGR_Y < 0` would wrongly fire for valid rows 128..191.
        if (c->HGR_Y >= GM_HIRES_ROWS)
            c->HGR_Y = GM_HIRES_ROWS - 1;
    }
    c->HBAS = gm_row_address(c->HGR_Y);
}

static void move_left_or_right(bool left, gm_vector_ctx *c)
{
    if (left)
        move_left(c);
    else
        move_right(c);
}

void gm_plot_pixel(gm_vector_ctx *c)
{
    uint16_t offset = c->HBAS + c->HGR_HORIZ;
    if (offset > GM_MAX_SCREEN_ADDR)
        return;
    c->write(offset,
        (uint8_t)(((c->screenmem[offset] ^ c->HGR_BITS) & c->HMASK) ^ c->screenmem[offset]),
        c->user);
}

// Applesoft-style HGLIN: dx + dy single-axis steps, plotting after each one.
// The 6502 keeps the error term as a 16-bit value plus the carry flag; the
// carry is clear exactly when that term, read as signed, is negative, so a
// plain signed error term reproduces it step for step.
void gm_draw_line(uint16_t target_x, uint8_t target_y, gm_vector_ctx *c)
{
    const bool left = target_x < c->HGR_X;
    const bool down = target_y > c->HGR_Y;
    const int dx = left ? c->HGR_X - target_x : target_x - c->HGR_X;
    const int dy = down ? target_y - c->HGR_Y : c->HGR_Y - target_y;

    gm_plot_pixel(c);

    int error = dx - dy;
    for (int steps = dx + dy; steps > 0; steps--) {
        if (error < 0) {
            move_up_or_down(down, c);
            error += dx;
        } else {
            move_left_or_right(left, c);
            error -= dy;
        }
        gm_plot_pixel(c);
    }
    // move_up_or_down() keeps HGR_Y, but nothing tracks HGR_X.
    c->HGR_X = target_x;
}

// ---- Brush / shape drawing ----------------------------------------------------

// Blend the pixels set in `mask` (low 7 bits) into the screen byte at
// `address`, taking their colour from `pattern`; the palette bit (bit 7)
// always comes from the pattern.
static void brush_blend(gm_vector_ctx *c, uint16_t address, uint8_t mask,
    uint8_t pattern)
{
    c->write(address,
        (uint8_t)(((mask ^ 0x7f) & c->screenmem[address]) | ((mask | 0x80) & pattern)),
        c->user);
}

// One 8x8 brush quadrant with its top-left pixel at column `column`, pixel
// `shift` within it, on row `scanline`. Each row is shifted across the column
// and the next one, then blended with the fill pattern: `pat_even` on even
// scanlines, `pat_odd` on odd ones, indexed by the column's position mod 4.
// `column` and `scanline` are bytes and wrap, as the 6502's do.
static void brush_quadrant(gm_vector_ctx *c, const uint8_t *bitmap,
    uint8_t column, uint8_t scanline, uint8_t shift,
    uint8_t pat_even, uint8_t pat_odd)
{
    for (int row = 0; row < BRUSH_PART_HEIGHT; row++, scanline++) {
        uint16_t address = (uint16_t)(gm_row_address(scanline) + column);
        if (bitmap[row] == 0 || address > GM_MAX_SCREEN_ADDR)
            continue;

        uint8_t mask_a, mask_b;
        gm_spread7(bitmap[row], shift, &mask_a, &mask_b);

        const uint8_t *pattern =
            c->pattern_data + (uint8_t)(((scanline & 1) ? pat_odd : pat_even) * 4);

        if (mask_a != 0)
            brush_blend(c, address, mask_a, pattern[column & 3]);
        if (mask_b != 0 && address + 1 <= GM_MAX_SCREEN_ADDR)
            brush_blend(c, (uint16_t)(address + 1), mask_b, pattern[(column + 1) & 3]);
    }
}

// Draw a brush as four quadrants: top-left, top-right, bottom-left, bottom-right.
// The current X,Y position determines the top-left corner, not the center.
void gm_draw_brush(uint16_t x, uint8_t y, uint8_t brush,
    uint8_t pat_even, uint8_t pat_odd, gm_vector_ctx *c)
{
    // The offset is a byte, so brush 8 and up wrap onto brushes 0..7,
    // as on the 6502.
    const uint8_t *bitmap =
        c->brush_bitmaps + (uint8_t)(brush * BRUSH_PART_SIZE * BRUSH_TOTAL_PARTS);
    const uint8_t column = (uint8_t)(x / GM_PIXELS_PER_BYTE);
    const uint8_t shift = x % GM_PIXELS_PER_BYTE;

    for (int part = 0; part < BRUSH_TOTAL_PARTS; part++)
        brush_quadrant(c, bitmap + part * BRUSH_PART_SIZE,
            (uint8_t)(column + (part & 1)),
            (uint8_t)(y + (part >> 1) * BRUSH_PART_HEIGHT),
            shift, pat_even, pat_odd);
}

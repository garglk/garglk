//
//  apple2_vector_draw.c
//  scott

//  Code to draw Apple 2 vector images in certain Scott Adams games.
//
//  Variants of this particular images format was used in many
//  Apple 2 games at the time. It was created by Penguin Software
//  (later Polarware) and marketed under names such as The Graphics
//  Magician and the Picture Painter tool.
//
//  The main purpose of this code is to stay pixel perfect with the
//  original code, so it closely follows the disassembly.
//  Parts are based on ScummVM's Comprehend engine.
//
//  The disassembly at
//  https://6502disassembly.com/a2-graphics-magician/PICDRAWH_1984.html
//  has been very helpful, but does not completely match this code.
//  Presumably it is based on a later version.
//
//  Created by Petter Sjölund on 2025-12-29.
//

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "scott.h"
#include "apple2draw.h"
#include "apple2_vector_draw.h"
#include "apple2detect.h"
#include "ciderpress.h"
#include "common_file_utils.h"
#include "vector_common.h"
#include "vector_oplist.h"
#include "gm_vector.h"

#define APPLE2_SCREEN_HEIGHT 192
#define APPLE2_SCREEN_COLS 40
#define COL_BITS 7
#define RIGHT_EXPAND_MASK 0x40
#define APPLE2_WHITE 0xff

// The ROM dispatcher (DRAW_OPCODE @ $8E30) selects on `byte & 0xe0`: a 3-bit
// opcode in the top bits, with the low nibble as the parameter and bit 4
// ignored. So only the even values below ever occur; doImageOp folds bit 4 out
// before switching. (The odd values are not real opcodes.)
enum Opcode {
    A2_OP_END = 0,
    A2_OP_SET_PEN_COLOR = 2,
    A2_OP_SET_SHAPE = 4,
    A2_OP_SET_FILL_COLOR = 6,
    A2_OP_MOVE_TO = 8,
    A2_OP_DRAW_LINE = 10,
    A2_OP_DRAW_SHAPE = 12,
    A2_OP_PAINT = 14
};

typedef enum {
    BRUSH_PIXEL = 0,
    BRUSH_CIRCLE_TINY = 1,
    BRUSH_CIRCLE_SMALLER = 2,
    BRUSH_CIRCLE_SMALL = 3,
    BRUSH_CIRCLE_MED = 4,
    BRUSH_CIRCLE_LARGE = 5,
    BRUSH_SRAY_SMALL = 6,
    BRUSH_SPRAY_LARGE = 7
} Brush;

typedef struct {
    gm_vector_ctx gv;   // shared cursor/colour + line-drawing state
    Brush BRUSH;        // current shape type
    uint8_t FILL_COLOR; // current fill
} a2_vector_ctx;

typedef struct {
    uint8_t scanline;
    uint8_t pattern_even;
    uint8_t pattern_odd;
    uint8_t fill_color;
    uint16_t screen_offset;
    uint16_t screen_row_base;
    uint8_t column;
    uint8_t seed_column;
    uint8_t existing_byte;
    uint8_t pixel_offset;
    uint8_t fill_mask;
    uint8_t pattern_base;
    uint8_t bitmap_index;
    Brush brush_type;
} a2_fill_ctx;


/* Single-line color patterns.  There are 30 patterns, 4 bytes each.
 When written to the hi-res screen, the 4-byte alignment is maintained so that
 overlapping areas drawn with the same pattern don't clash.
 */

static uint8_t pattern_data[120];

/* Sub-indexes for 108 patterns. Each entry is two adjacent  bytes, holding
 the sub-indices into the color pattern data table for even and odd rows.
 Values are in the range [0,29].

 For example, color pattern 8 uses sub-index $03 for even rows and $06 for odd.
 In the pattern table, $03 is white0, $06 is orange. The pattern drawn will
 have alternating rows of white and orange.
 */

static uint8_t color_pattern_subindices[216];

/* Brush bitmaps.  Each bitmap has four 8-byte pieces. */

static uint8_t brush_bitmaps[256];

#define NUM_PATTERNS (sizeof(pattern_data) / 4)

/* Returns the pattern sub-index for the even (half 0) or odd (half 1) rows of
   color pattern `color`. The color comes straight from the image data and the
   table from the game disk, so keep both inside their tables. */
static uint8_t pattern_subindex(uint8_t color, int half) {
    size_t index = (uint8_t)(color * 2 + half);
    if (index >= sizeof(color_pattern_subindices))
        return 0;
    uint8_t subindex = color_pattern_subindices[index];
    return subindex < NUM_PATTERNS ? subindex : 0;
}

void SetBrushBitmaps(const uint8_t *data) {
    memcpy(brush_bitmaps, data, sizeof(brush_bitmaps));
}

void SetPatternData(const uint8_t *data) {
    memcpy(pattern_data, data, sizeof(pattern_data));
}

void SetColorPatternSubindices(const uint8_t *data) {
    memcpy(color_pattern_subindices, data, sizeof(color_pattern_subindices));
}

typedef struct {
    uint16_t offset;
    uint8_t value;
    bool fill_bg;
} byte_to_write;

static uint8_t *slow_vector_screenmem = NULL;

static VectorOpList write_ops = VECTOR_OPLIST(byte_to_write);

/* Bytes emitted per slow-draw tick. Pairs with the graphics timer interval —
 * change in step with it. */
#define APPLE2_VECTOR_BYTES_PER_TICK 50


// set_color / set_draw_position now live in common_sagadraw/gm_vector.c.

static inline bool is_valid_screen_offset(uint16_t offset) {
    return offset <= GM_MAX_SCREEN_ADDR;
}

// MARK: SLOW DRAW

static void write_to_screenmem(uint16_t offset, uint8_t value, bool fill) {
    if (!is_valid_screen_offset(offset))
        return;
    if (fill) {
        // For now we assume that we only fill the background as the first instruction
        // and that it always is white (0xff) (and only when drawing room images)
        memset(screenmem, APPLE2_WHITE, A2_SCREEN_MEM_SIZE);
        memset(slow_vector_screenmem, APPLE2_WHITE, A2_SCREEN_MEM_SIZE);
    } else {
        screenmem[offset] = value;
    }
    byte_to_write *op = VectorOpListPush(&write_ops);
    op->offset = offset;
    op->value = value;
    op->fill_bg = fill;
}

// Write hook handed to the shared gm_vector line primitives.
static void scott_gm_write(uint16_t offset, uint8_t value, void *user) {
    (void)user;
    write_to_screenmem(offset, value, false);
}

// MARK: FLOOD FILL

/* Read the screen byte at the given column on the current scanline row.
   Sets ctx->screen_offset and ctx->existing_byte.
   Returns false if the offset is out of bounds. */
static bool read_screen_byte(a2_fill_ctx *ctx, uint8_t col) {
    ctx->screen_offset = ctx->screen_row_base + col;
    if (!is_valid_screen_offset(ctx->screen_offset))
        return false;
    ctx->existing_byte = screenmem[ctx->screen_offset];
    return true;
}

/* Select the pattern table base offset for the current scanline,
   choosing even or odd pattern based on scanline parity. */
static void select_pattern_for_scanline(a2_fill_ctx *ctx) {
    uint8_t pattern_index = (ctx->scanline & 1) ? ctx->pattern_odd : ctx->pattern_even;
    ctx->pattern_base = pattern_index * 4;
}

/* Tests the contents of the hi-res screen to see if a specific pixel is white.
 Used by the fill code.

 Pixels appear white when two adjacent bits are set.  This code does not check
 for set bits in adjacent bytes, which makes the code faster but slightly less
 precise.

 The double-bit masks are a little off - the first two entries are identical -
 because we only want to examine a single byte.  The high bit is excluded,
 because we're just testing visible pixels.

 00000011
 00000011
 00000110
 00001100
 00011000
 00110000
 01100000

 */

static bool is_seed_pixel_white(a2_fill_ctx *ctx) {
    ctx->screen_row_base = gm_row_address(ctx->scanline);
    if (!read_screen_byte(ctx, ctx->seed_column))
        return false;
    uint8_t two_bits[7] = {0x3, 0x3, 0x6, 0xC, 0x18, 0x30, 0x60};
    uint8_t mask = two_bits[ctx->pixel_offset];
    return (mask & ctx->existing_byte) == mask;
}

static void paint_column_pattern(a2_fill_ctx *ctx) {
    uint16_t offset = ((ctx->column & 3) | ctx->pattern_base);
    uint8_t color_mask = pattern_data[offset];
    ctx->existing_byte = (ctx->existing_byte ^ ctx->fill_mask) & 0x7f;
    if (!is_valid_screen_offset(ctx->screen_offset))
        return;
    write_to_screenmem(ctx->screen_offset, ((ctx->fill_mask | 0x80) & color_mask) |
                       ctx->existing_byte, false);
}

/* Convert an X pixel coordinate into a column number and pixel offset. */
static void x_to_column_and_pixel(a2_fill_ctx *ctx, uint16_t x) {
    ctx->seed_column = x / COL_BITS;
    ctx->pixel_offset = x % COL_BITS;
}

static bool scan_up_to_border(a2_fill_ctx *ctx)
{
    /* Move up until we find a pixel that's not white (0xff)
     * or we hit the top.
     */
    do {
        if (ctx->scanline == 0)
            return false; /* Found no non-white pixels */
        ctx->scanline--;
    } while (is_seed_pixel_white(ctx));
    return true;
}

/* Compute the right edge of the fill mask for the current scanline.
   Rotates the existing screen byte to determine
   how many pixels on the right side of the seed column are already filled.
   Sets ctx->fill_mask and returns the right edge pixel count. */
static uint8_t compute_right_edge(a2_fill_ctx *ctx)
{
    /* Simulate rotating left with carry (7 - ctx->pixel_offset) times */
    uint16_t rotate_word = ctx->existing_byte << (7 - ctx->pixel_offset);
    ctx->fill_mask = (rotate_word & 0xff) | (rotate_word >> 9);

    /* If the rotation left some leading set bits, ensure fill_mask is extended appropriately */
    uint8_t loop_byte = (rotate_word & 0x100) != 0;
    int8_t remaining_bits;
    for (remaining_bits = 6 - ctx->pixel_offset;
         remaining_bits > 0 && loop_byte != 0; remaining_bits--) {
        uint8_t carry = loop_byte << 7;
        loop_byte = ctx->fill_mask & 1;
        ctx->fill_mask = (ctx->fill_mask >> 1) | carry;
    }

    /* Align the byte and remember the shift used */
    ctx->fill_mask >>= remaining_bits;
    return remaining_bits;
}

/* Compute the left edge of the fill mask for the current scanline.
   Continues transforming ctx->fill_mask (set by compute_right_edge)
   to determine how many pixels on the left are already filled.
 Sets ctx->fill_mask and returns the left edge pixel count. */
static uint8_t compute_left_edge(a2_fill_ctx *ctx)
{
    /* Simulate rotating right with carry, (pixel_offset + 1) times */
    uint16_t rotate_word = (ctx->fill_mask << 8) >> (ctx->pixel_offset + 1);
    ctx->fill_mask = (rotate_word >> 8) | (rotate_word << 1);

    /* If pixel_offset > 0, ensure fill_mask is extended appropriately */
    uint8_t loop_byte = 1;
    int8_t remaining_bits;
    for (remaining_bits = ctx->pixel_offset;
         remaining_bits > 0 && loop_byte != 0; remaining_bits--) {
        loop_byte = ctx->fill_mask >> 7;
        ctx->fill_mask = (ctx->fill_mask << 1) | 1;
    }

    /* Align the byte and remember the shift used */
    ctx->fill_mask <<= remaining_bits;
    return remaining_bits;
}

/*  Expand right from the seed point.

    For each column to the right, rotate the screen byte right with carry
    to find the boundary between set and unset low bits. The rotation count
    determines the right edge, and the remaining upper bits determine
    whether to keep expanding further right. */
static uint8_t fill_span_right(a2_fill_ctx *ctx, uint8_t right_edge, bool has_more) {
    while (has_more && ctx->column + 1 < APPLE2_SCREEN_COLS) {
        ctx->column++;
        if (!read_screen_byte(ctx, ctx->column)) break;

        /* Rotate right with carry to find the first zero bit from the LSB.
           Carry chain feeds 1-bits back in from the top as long as the LSB
           keeps producing 1s. */
        ctx->fill_mask = ctx->existing_byte;
        bool carry = false;
        bool lsb_set = true;
        int remaining;
        for (remaining = 8; remaining > 0 && lsb_set; remaining--) {
            lsb_set = ctx->fill_mask & 1;
            ctx->fill_mask = (ctx->fill_mask >> 1) | (carry << 7);
            carry = lsb_set;
        }
        ctx->fill_mask >>= (remaining + 1);
        has_more = (ctx->fill_mask & RIGHT_EXPAND_MASK) != 0;
        right_edge = remaining;

        paint_column_pattern(ctx);
    }
    return right_edge;
}

/* Expand left from the seed point.
 
   Scans the screen byte at the column to the left, rotating bits left
   to find the first zero bit from the MSB. If found, has_more is cleared
   (stopping leftward expansion). */
static uint8_t fill_span_left(a2_fill_ctx *ctx, uint8_t left_edge, bool has_more) {
    while (has_more && ctx->column > 0) {
        ctx->column--;
        if (!read_screen_byte(ctx, ctx->column)) break;

        /* Rotate left through the byte to find the first zero from the MSB */
        ctx->fill_mask = ctx->existing_byte << 1;
        bool carry = false;
        bool msb_set = true;
        int remaining;
        for (remaining = COL_BITS - 1; remaining >= 0 && msb_set; remaining--) {
            msb_set = ctx->fill_mask & 0x80;
            ctx->fill_mask = (ctx->fill_mask << 1) | carry;
            carry = msb_set;
            if (!msb_set) {
                ctx->fill_mask <<= remaining;
                has_more = false;
                left_edge = remaining;
            }
        }
        ctx->fill_mask = (ctx->fill_mask << 1) | msb_set;
        paint_column_pattern(ctx);
    }
    return left_edge;
}

/* Re-seed at the horizontal midpoint of the span just filled, so the next
   scanline drops from its centre (this is what gives the fill its better
   handling of concave shapes). The span runs from ctx->column (the leftmost
   painted column) to rightmost_column, with left_edge_pixels / right_edge_pixels
   left unfilled at the two ends.

   The arithmetic is deliberately 8-bit to match the original 6502 routine,
   including the explicit borrow when right_edge_pixels exceeds the running
   value. */
static void advance_seed_to_midpoint(a2_fill_ctx *ctx,
                                     uint8_t rightmost_column,
                                     uint8_t right_edge_pixels,
                                     uint8_t left_edge_pixels)
{
    /* Total span width in pixels: the two partial end columns plus the fully
       painted columns between them. */
    int8_t filled_columns = rightmost_column - ctx->column - 1;
    uint8_t span_pixels = (filled_columns < 0) ? COL_BITS
                                               : (filled_columns + 2) * COL_BITS;

    /* Trim the unfilled pixels at each end, then halve: the offset of the
       midpoint from the span's left edge. */
    uint8_t half = (uint8_t)(span_pixels - right_edge_pixels) - (right_edge_pixels > span_pixels);
    half = (uint8_t)(half - left_edge_pixels) / 2;

    /* The left edge itself sits left_edge_pixels into ctx->column, so add that
       back when turning the midpoint offset into a column + pixel position. */
    uint8_t offset = left_edge_pixels + (half % COL_BITS);
    ctx->seed_column = ctx->column + (half / COL_BITS) + (offset / COL_BITS);
    ctx->pixel_offset = offset % COL_BITS;
}

/*

 Fills a region of the hi-res screen with a pattern.

 Algorithm description, from the manual:
  1) Scan directly up from the selected point until a border is found.
  2) Move down one line, filling to the left and right borders.
  3) Average the left and right borders to find the midpoint, and move down one line from there.
  4) Check to see if the point moved down to is a border. If not, go back to step 2.

  The midpoint adjustment provides better handling of concave shapes.
 */

static void apple2_flood_fill(uint16_t x, uint8_t y, uint8_t color)
{
    a2_fill_ctx ctx = {};

    ctx.fill_color = color;
    ctx.scanline = y;

    /* Configure pattern colors */
    ctx.pattern_even = pattern_subindex(ctx.fill_color, 0);
    ctx.pattern_odd  = pattern_subindex(ctx.fill_color, 1);

    /* Get col/pixel for this X position */
    x_to_column_and_pixel(&ctx, x);

    if (y == 0) {
        /* 6502 quirk: when the seed is already on row 0, FLOOD_FILL skips the
         * scan-up entirely (`goto FoundTop`) with the A register still holding
         * MODULO_AND_DIVIDE's result — the X remainder, i.e. pixel_offset. So a
         * row-0 seed begins filling at scanline = pixel_offset, leaving rows
         * 0..pixel_offset-1 untouched. (This differs from a seed started below
         * row 0 that scans all the way up to a white row 0, which does fill
         * from row 0.) */
        ctx.scanline = ctx.pixel_offset;
    } else if (scan_up_to_border(&ctx)) {
        /* Move back down to the last white pixel we saw.
         * (Unless we are at row 0 and it is white) */
        ctx.scanline++;
    }

    uint8_t right_edge_pixels = 0;
    uint8_t left_edge_pixels = 0;

    /* Main scan loop: paint horizontal spans, then move down */
    while (ctx.scanline < APPLE2_SCREEN_HEIGHT && is_seed_pixel_white(&ctx)) {

        select_pattern_for_scanline(&ctx);

        /* Work out fill mask and edge pixels for this scanline */
        right_edge_pixels = compute_right_edge(&ctx);
        left_edge_pixels = compute_left_edge(&ctx);

        bool has_more_left = (ctx.fill_mask & 1) != 0;
        bool has_more_right = (ctx.fill_mask & RIGHT_EXPAND_MASK) != 0;

        /* Paint starting column */
        ctx.column = ctx.seed_column;
        paint_column_pattern(&ctx);

        /* Expand right */
        right_edge_pixels = fill_span_right(&ctx, right_edge_pixels, has_more_right);
        uint8_t rightmost_column = ctx.column;

        ctx.column = ctx.seed_column;

        /* Expand left */
        left_edge_pixels = fill_span_left(&ctx, left_edge_pixels, has_more_left);

        /* Move seed position to the midpoint of the filled span */
        advance_seed_to_midpoint(&ctx, rightmost_column, right_edge_pixels, left_edge_pixels);
        ctx.scanline++;
    }
}

// MARK: Main draw routine

// Number of argument bytes that follow each (even) opcode
static const uint8_t opcode_arguments[16] = {
    [A2_OP_SET_FILL_COLOR] = 1,
    [A2_OP_MOVE_TO] = 2,
    [A2_OP_DRAW_LINE] = 2,
    [A2_OP_DRAW_SHAPE] = 2,
    [A2_OP_PAINT] = 2,
};

static bool doImageOp(const uint8_t **outptr, const uint8_t *end, a2_vector_ctx *ctx) {
    const uint8_t *ptr = *outptr;
    uint8_t opcode;
    uint16_t a, b;

    opcode = *ptr++;

    uint8_t param = opcode & 0xf;
    /* The ROM dispatcher (DRAW_OPCODE @ $8E30) selects the opcode with
       `byte & 0xe0` — a 3-bit selector in which bit 4 (0x10) is ignored. So an
       odd top-nibble aliases to the even opcode below it (e.g. 0x5x is
       SET_SHAPE, not a distinct "text outline" op that swallows an argument
       byte). Fold bit 4 out to match, which keeps the opcode stream in sync. */
    opcode = (opcode & 0xe0) >> 4;

    // A truncated final op ends the image
    if (ptr + opcode_arguments[opcode] > end) {
        *outptr = end;
        return true;
    }

    // After folding bit 4 out above, `opcode` is always even, so only these
    // eight cases are reachable — exactly the ROM dispatcher's branches.
    switch (opcode) {
        case A2_OP_END: // 0
            *outptr = ptr;
            return true;

        case A2_OP_SET_PEN_COLOR: // 2
            gm_set_color(param, &ctx->gv);
            break;

        case A2_OP_SET_SHAPE: // 4
            ctx->BRUSH = (Brush)param;
            break;

        case A2_OP_SET_FILL_COLOR: // 6
            a = *ptr++;
            ctx->FILL_COLOR = a;
            break;

        case A2_OP_MOVE_TO: // 8
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            gm_set_draw_position(a, b, &ctx->gv);
            break;

        case A2_OP_DRAW_LINE: // 10
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;

            gm_draw_line(a, b, &ctx->gv);
            break;

        case A2_OP_DRAW_SHAPE: // 12
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            gm_draw_brush(a, b, ctx->BRUSH,
                          pattern_subindex(ctx->FILL_COLOR, 0),
                          pattern_subindex(ctx->FILL_COLOR, 1),
                          &ctx->gv);
            break;

        case A2_OP_PAINT: // 14
            a = *ptr++ + (param & 1 ? 256 : 0);
            b = *ptr++;
            apple2_flood_fill(a, b, ctx->FILL_COLOR);
            break;
    }

    *outptr = ptr;
    return false;
}

static void white_background(void) {
    // Draw a white background with a black line to the left, to match
    // the artifact renderer: an all-0xff line is black at dot 0 only.
    SetColor(0, 0);
    SetColor(1, 0xffffff);
    RectFill(0, 0, 1, ImageHeight, 0);
    RectFill(1, 0, ImageWidth - 1, ImageHeight, 1);
}

static VectorStepResult write_pixel(const void *op, size_t current_op_index) {
    const byte_to_write *towrite = op;
    if (towrite->fill_bg) {
        white_background();
        return VECTOR_STEP_NEXT;
    }
    slow_vector_screenmem[towrite->offset] = towrite->value;
    DrawSingleApple2ImageByte(slow_vector_screenmem, towrite->offset);
    if (current_op_index + 1 < write_ops.total) {
        const byte_to_write *next_byte = VectorOpAt(&write_ops, current_op_index + 1);
        // Extend the chunk only across physically adjacent bytes, whose Apple II
        // artifact colours depend on each other — stopping between them would
        // flash a transient wrong colour at the seam. (A previous version also
        // extended across any two ops sharing a value, which made large
        // same-pattern fills snap in within a single tick instead of revealing
        // gradually like the rest of the picture.)
        if (next_byte->offset >= towrite->offset - 1 && next_byte->offset <= towrite->offset + 1) {
            return VECTOR_STEP_EXTEND;
        }
    }
    return VECTOR_STEP_NEXT;
}

void DrawSomeApple2VectorBytes(int from_start)
{
    VectorOpListPlayChunk(&write_ops, from_start, APPLE2_VECTOR_BYTES_PER_TICK,
        write_pixel, DrawApple2ImageFromVideoMem);
}

int DrawingApple2Vector(void) {
    return VectorOpListDrawing(&write_ops);
}

int DrawApple2VectorImage(USImage *img) {
    if (!img || !img->imagedata || img->datasize < 2)
        return 0;

    a2_vector_ctx ctx = {};

    const uint8_t *vecdata = img->imagedata + 4;
    int done = 0;
    if (screenmem == NULL) {
        screenmem = MemCalloc(A2_SCREEN_MEM_SIZE);
    }
    if (slow_vector_screenmem == NULL) {
        slow_vector_screenmem = MemAlloc(A2_SCREEN_MEM_SIZE);
    }

    if (!VectorBeginImage(&write_ops, img, DrawApple2ImageFromVideoMem))
        return 0;
    if (img->usage == IMG_ROOM)
        write_to_screenmem(0, 0, true);

    ctx.gv.screenmem = screenmem;
    ctx.gv.write = scott_gm_write;
    ctx.gv.user = NULL;
    ctx.gv.pattern_data = pattern_data;
    ctx.gv.brush_bitmaps = brush_bitmaps;

    gm_set_color(4, &ctx.gv);
    gm_set_draw_position(140, 96, &ctx.gv);
    ctx.FILL_COLOR = 0;
    ctx.BRUSH = BRUSH_CIRCLE_LARGE;

    while (done == 0 && vecdata < img->imagedata + img->datasize) {
        done = doImageOp(&vecdata, img->imagedata + img->datasize, &ctx);
    }
    return 1;
}

int TestApple2ImageWithName(const char *name, const char *supportpath) {
    USImage *image = LoadTestImage("apple2", name, supportpath);
    if (!image)
        return 0;
    image->usage = IMG_ROOM;
    DrawApple2VectorImage(image);
    FreeTestImage(image);
    return CompareWithTestFile("apple2", name, ".result", supportpath, screenmem, A2_SCREEN_MEM_SIZE);
}

int RunApple2VectorTests(const char *supportpath) {

    gli_slowdraw = 0;

    size_t disksize;
    uint8_t *diskdata = ReadTestDataFromFile("MI.dsk", supportpath, &disksize);
    if (diskdata) {
        InitDskImage(diskdata, disksize);
        LoadDrawingDataFromDisk(diskdata, disksize);
        FreeDiskImage();
        free(diskdata);
    }

    if (!TestApple2ImageWithName("mi_boom", supportpath)) {
        return 0;
    }
    if (!TestApple2ImageWithName("so_console", supportpath)) {
        return 0;
    }
    if (!TestApple2ImageWithName("so_painting", supportpath)) {
        return 0;
    }
    return 1;
}

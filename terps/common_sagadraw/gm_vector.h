//
//  gm_vector.h
//
//  Shared Apple II "Graphics Magician" (Penguin Software) vector primitives,
//  used by both the Scott Adams SAGA renderer (scott/saga/apple2_vector_draw.c)
//  and the Comprehend renderer (comprehend/graphics_magician.cpp).
//
//  These two engines were reverse-engineered from different 6502 interpreters
//  but the cursor, line and brush primitives are the same in both, so they
//  live here once. (The flood fill is NOT shared: the two products use
//  genuinely different fill routines.)
//
//  The caller owns the hi-res page and supplies a write hook, so each engine
//  keeps its own slow-draw / dirty-row bookkeeping.
//

#ifndef gm_vector_h
#define gm_vector_h

#include <stdint.h>
#include <stdbool.h>

#include "gm_hires.h"

#ifdef __cplusplus
extern "C" {
#endif

// Cursor + colour state for the Graphics Magician line primitives. The field
// names match the 6502 zero-page locations the disassembly uses.
typedef struct gm_vector_ctx {
    uint16_t HBAS;       // hi-res base address of row HGR_Y
    uint16_t HGR_X;      // current pen X (0..279)
    uint8_t  HGR_Y;      // current pen Y (0..191)
    uint8_t  HMASK;      // bit mask within the current column byte
    uint8_t  HGR_BITS;   // colour byte, phase-adjusted for the column
    uint8_t  HGR_HORIZ;  // current column, i.e. byte within the row (0..39)
    uint8_t  HGR_COLOR;  // current pen colour (hi-res byte)

    uint8_t *screenmem;  // hi-res page (>= 0x2000 bytes), owned by the caller
    void   (*write)(uint16_t offset, uint8_t value, void *user); // engine write hook
    void    *user;       // opaque, passed through to write()

    // Drawing tables (owned by the caller), needed by gm_draw_brush only.
    const uint8_t *pattern_data;   // 120 bytes: 30 colour patterns x 4
    const uint8_t *brush_bitmaps;  // 256 bytes: 8 brushes x 4 quadrants x 8 rows
} gm_vector_ctx;

// Spread an 8-bit source byte (brush or glyph row) across two adjacent hi-res
// columns by `offset` pixels: the low 7 bits land in *mask_a (column col), the
// pixels carried past bit 6 land in *mask_b (column col+1). Shared by the brush
// blitter and the Comprehend text-glyph blitter.
void gm_spread7(uint8_t src, uint8_t offset, uint8_t *mask_a, uint8_t *mask_b);

// Set the current pen colour from a 0..7 colour index. Other values leave the
// colour unchanged.
void gm_set_color(uint8_t color_index, gm_vector_ctx *c);

// Move the pen to (x, y), recomputing HBAS / HMASK / HGR_BITS for that position.
void gm_set_draw_position(uint16_t x, uint8_t y, gm_vector_ctx *c);

// Plot the single pixel at the current pen position (exposed for circle drawing).
void gm_plot_pixel(gm_vector_ctx *c);

// Draw a line from the current pen position to (target_x, target_y); leaves the
// pen at the target.
void gm_draw_line(uint16_t target_x, uint8_t target_y, gm_vector_ctx *c);

// Draw brush shape `brush` (0..7) with its top-left corner at (x, y), blended
// with the fill-colour pattern given as even/odd-row sub-indices. Requires
// c->pattern_data and c->brush_bitmaps to be set. Does not touch the pen cursor.
void gm_draw_brush(uint16_t x, uint8_t y, uint8_t brush,
    uint8_t pat_even, uint8_t pat_odd, gm_vector_ctx *c);

#ifdef __cplusplus
}
#endif

#endif /* gm_vector_h */

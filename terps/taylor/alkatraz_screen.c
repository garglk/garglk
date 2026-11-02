//
//  alkatraz_screen.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  See alkatraz_screen.h. This mirrors the loader's SCADDS/STYLER routines:
//  DECLI moves one pixel row (up/down), DECSI moves one character column
//  (left/right), and COLOUR emits the attribute address for a cell whenever
//  the draw crosses a character-row boundary.
//

#include "alkatraz_screen.h"

/* ZX Spectrum display-file address layout. A bitmap address packs three fields
   into its high byte and two into its low byte:

     high byte:  0 1 0 t t p p p   (t = third 0-2, p = pixel row in char 0-7)
     low  byte:  r r r c c c c c   (r = char row in third 0-7, c = column 0-31)

   so stepping down one pixel row is +0x100, and when the pixel-row field wraps
   the carry moves to the char-row field (+0x20 in the low byte), which in turn
   carries into the third (+0x800). */
#define ZX_PIXEL_ROW        0x0100  /* one pixel row down                     */
#define ZX_PIXEL_ROW_MASK   0x0700  /* pixel-row-in-char field (high byte)    */
#define ZX_CHAR_ROW         0x0020  /* one char row down (low byte)           */
#define ZX_CHAR_ROW_MASK    0x00e0  /* char-row-in-third field (low byte)     */
#define ZX_THIRD            0x0800  /* one third (8 char rows) down           */
#define ZX_THIRD_SHIFT      11      /* third field position in the address    */
#define ZX_THIRD_MASK       3       /* third field width (0-2)                */
#define ZX_ATTR_HI          0x58    /* attribute file base, high byte (0x5800)*/
#define ZX_CHAR_PIXELS      8       /* pixel rows per character               */

#define LOW_BYTE            0x00ff
#define HIGH_BYTE           0xff00

/* Draw directions, as stored in descriptor byte 2 bits 6-7. */
enum { ALK_RIGHT = 0, ALK_LEFT = 1, ALK_UP = 2, ALK_DOWN = 3 };

/* The line/column steppers treat ALK_LEFT and ALK_UP as the "decreasing"
   directions, mirroring the loader's DECLI/DECSI sign choice. */
static int alk_decreasing(int direction) { return direction == ALK_LEFT || direction == ALK_UP; }

/* DECLI: move one pixel row, with the Spectrum display file's character-row
   and third carries. */
static uint16_t alk_next_line(uint16_t addr, int direction)
{
    if (alk_decreasing(direction)) {            /* up */
        addr = (uint16_t)(addr - ZX_PIXEL_ROW);
        if ((~addr & ZX_PIXEL_ROW_MASK) != 0)   /* pixel-row field didn't wrap */
            return addr;
        addr = (uint16_t)(addr - ZX_CHAR_ROW);
        if ((~addr & ZX_CHAR_ROW_MASK) != 0)    /* char-row field didn't wrap  */
            return (uint16_t)(addr + ZX_THIRD);
        return (uint16_t)(addr + ZX_PIXEL_ROW);
    } else {                                    /* down */
        addr = (uint16_t)(addr + ZX_PIXEL_ROW);
        if ((addr & ZX_PIXEL_ROW_MASK) != 0)
            return addr;
        addr = (uint16_t)(addr + ZX_CHAR_ROW);
        if ((addr & ZX_CHAR_ROW_MASK) != 0)
            return (uint16_t)(addr - ZX_THIRD);
        return (uint16_t)(addr - ZX_PIXEL_ROW);
    }
}

/* DECSI: move one character column right (increasing) or left (decreasing). */
static uint16_t alk_next_col(uint16_t addr, int direction)
{
    int step = alk_decreasing(direction) ? -1 : 1;
    return (uint16_t)((addr & HIGH_BYTE) | ((addr + step) & LOW_BYTE));
}

/* COLOUR: emit the attribute address for `addr` if it sits on a character-row
   boundary. The attribute address keeps the low byte (the column) and maps the
   high byte to ZX_ATTR_HI + third. The boundary is the top pixel row of a cell
   when drawing down and the bottom pixel row when drawing up. */
static void alk_emit_attr(uint16_t addr, int direction, uint16_t *out, int *nout, int maxout)
{
    int pixel_row = (addr & ZX_PIXEL_ROW_MASK) >> 8;
    int boundary = alk_decreasing(direction) ? (pixel_row == ZX_CHAR_PIXELS - 1)
                                             : (pixel_row == 0);
    if (boundary && *nout < maxout) {
        int third = (addr >> ZX_THIRD_SHIFT) & ZX_THIRD_MASK;
        out[(*nout)++] = (uint16_t)(((ZX_ATTR_HI + third) << 8) | (addr & LOW_BYTE));
    }
}

/* Window descriptor layout (4 bytes each). */
#define DESC_BYTES        4
#define DESC_SIM_SHIFT    5     /* byte[3] bits 5-7: (simultaneous count) - 1   */
#define DESC_SIM_MASK     7
#define DESC_DIR_SHIFT    6     /* byte[2] bits 6-7: direction                  */
#define DESC_DIR_MASK     3
#define DESC_WIDTH_MASK   0x3f  /* byte[2] bits 0-5: width in characters        */
#define DESC_HEIGHT_MASK  0x1f  /* byte[3] bits 0-4: height in characters       */
#define ALK_MAX_SIM       8     /* most windows that can be drawn at once       */
#define ALK_GUARD_LIMIT   0x40000  /* runaway bound for malformed descriptors   */

/* One strip being drawn. `fast_axis_remaining` counts cells left along the
   strip's fast axis (reset to `fast_axis_length` at the end of each line or
   column); `slow_axis_remaining` counts how many lines/columns are left.
   (origin_low, origin_high) remembers the strip's origin along the slow axis
   so each new line/column starts from the right place. */
typedef struct {
    int direction;
    int vertical;               /* 1 = strip runs across rows; 0 = down columns */
    int fast_axis_remaining;
    int fast_axis_length;
    int slow_axis_remaining;
    uint16_t addr;
    uint8_t origin_low, origin_high;
    int done;
} AlkSlot;

/* Set up the draw state of one strip from its 4-byte descriptor. */
static void alk_load_slot(AlkSlot *slot, const uint8_t *desc)
{
    uint16_t addr = (uint16_t)(desc[0] | (desc[1] << 8));
    int direction = (desc[2] >> DESC_DIR_SHIFT) & DESC_DIR_MASK;
    int width = desc[2] & DESC_WIDTH_MASK;
    int height = (desc[3] & DESC_HEIGHT_MASK) * ZX_CHAR_PIXELS;
    slot->direction = direction;
    slot->addr = addr;
    slot->origin_low = addr & LOW_BYTE;          /* origin column   */
    slot->origin_high = (addr >> 8) & LOW_BYTE;  /* origin row       */
    slot->done = 0;
    if (direction == ALK_UP || direction == ALK_DOWN) {
        /* Vertical window: drawn row by row. Each row is `width`
           characters wide; there are `height` pixel rows. */
        slot->vertical = 1;
        slot->fast_axis_remaining = slot->fast_axis_length = width;
        slot->slow_axis_remaining = height;
    } else {
        /* Horizontal window: drawn column by column. Each column is
           `height` pixels tall; there are `width` columns. */
        slot->vertical = 0;
        slot->fast_axis_remaining = slot->fast_axis_length = height;
        slot->slow_axis_remaining = width;
    }
}

/* Move a strip on to its next cell, and mark it done after its last. */
static void alk_advance(AlkSlot *strip)
{
    if (strip->vertical) {
        /* Step along the row (a character column at a time). */
        strip->addr = alk_next_col(strip->addr, strip->direction);
        if (--strip->fast_axis_remaining != 0)
            return;
        /* Row finished: back to the row's start column, drop
           down/up one pixel row, and count the row off. */
        strip->addr = (uint16_t)((strip->addr & HIGH_BYTE) | strip->origin_low);
        strip->addr = alk_next_line(strip->addr, strip->direction);
        strip->origin_low = strip->addr & LOW_BYTE;
    } else {
        /* Step down the column (a pixel row at a time). */
        strip->addr = alk_next_line(strip->addr, strip->direction);
        if (--strip->fast_axis_remaining != 0)
            return;
        /* Column finished: back to the column's start row,
           move one character column across, count it off. */
        strip->addr = (uint16_t)((strip->origin_high << 8) | strip->origin_low);
        strip->addr = alk_next_col(strip->addr, strip->direction);
        strip->origin_low = strip->addr & LOW_BYTE;
        strip->origin_high = (strip->addr >> 8) & LOW_BYTE;
    }
    strip->fast_axis_remaining = strip->fast_axis_length;
    if (--strip->slow_axis_remaining == 0)
        strip->done = 1;
}

int AlkatrazScreenOrder(const uint8_t *descriptors, int nwin,
    uint16_t *out, int maxout)
{
    int nout = 0;        /* number of addresses emitted so far */
    int next_win = 0;    /* index of the next descriptor to consume */

    /* Walk the descriptors in groups. The first descriptor of each group says
       how many strips (windows) in that group are drawn at the same time. */
    while (next_win < nwin && nout < maxout) {
        const uint8_t *group_first = descriptors + next_win * DESC_BYTES;
        int group_count = ((group_first[3] >> DESC_SIM_SHIFT) & DESC_SIM_MASK) + 1; /* 1..8 */
        if (next_win + group_count > nwin)     /* clamp a malformed last group */
            group_count = nwin - next_win;

        /* Load the group's descriptors into draw-state slots. Each descriptor
           is 4 bytes: [0..1] start address (LE), [2] bits 6-7 direction +
           bits 0-5 width (chars), [3] bits 5-7 simultaneous-count + bits 0-4
           height (chars, x8 = pixels). Directions: 0=right,1=left,2=up,3=down. */
        AlkSlot slots[ALK_MAX_SIM];
        for (int i = 0; i < group_count; i++)
            alk_load_slot(&slots[i], descriptors + (next_win + i) * DESC_BYTES);
        next_win += group_count;

        /* Emit the group's cells. Within a group the simultaneous strips are
           interleaved one cell each per pass (this is what makes the loader's
           wipe look "woven"), so round-robin the slots until every strip is
           finished. `guard` just bounds a runaway on malformed data. */
        int active = 1, guard = 0;
        while (active && nout < maxout && guard++ < ALK_GUARD_LIMIT) {
            active = 0;
            for (int i = 0; i < group_count; i++) {
                AlkSlot *strip = &slots[i];
                if (strip->done)
                    continue;
                active = 1;

                /* Emit this strip's current cell: first its attribute address
                   if we just crossed onto a new character row, then the byte. */
                alk_emit_attr(strip->addr, strip->direction, out, &nout, maxout);
                if (nout < maxout)
                    out[nout++] = strip->addr;

                alk_advance(strip);
            }
        }
    }

    return nout;     /* total addresses written to out[] */
}

//  line_drawing.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  This file is based on code from the Level9 interpreter by Simon Baldwin

#include <stdlib.h>
#include <string.h>

#include "line_drawing.h"
#include "ringbuffer.h"
#include "sagadraw.h"
#include "sagagraphics.h"
#include "scott.h"
#include "vector_common.h"
#include "vector_oplist.h"

#define HOWARTH_OP_MOVE_TO  0xc0
#define HOWARTH_OP_FILL     0xc1
#define HOWARTH_OP_END      0xff

#define DRAW_HEIGHT_OFFSET 191

#define MYSTERIOUS_WIDTH 255
#define MYSTERIOUS_HEIGHT 97
#define MYSTERIOUS_CLIPHEIGHT 95

line_image *LineImages;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t colour;
} pixel_to_draw;

static VectorOpList pixels_to_draw = VECTOR_OPLIST(pixel_to_draw);
int vector_image_shown = -1;

/* Pixels emitted per slow-draw tick. Pairs with the 20 ms timer interval
 * used by DrawHowarthVectorPicture — change in step with it. */
#define SCOTT_VECTOR_PIXELS_PER_TICK 50

/* TRUE once the background fill has been issued for the current image. Reset
 * whenever the pixel list is released and whenever DrawSomeHowarthVectorPixels
 * is asked to start over. */
static int vector_background_painted = 0;

static uint8_t *picture_bitmap = NULL;

static int line_colour = 15;
static int bg_colour = 0;

/*
 * scott_linegraphics_plot_clip()
 * scott_linegraphics_draw_line()
 *
 * Draw a line from x1,y1 to x2,y2 in colour line_colour.
 * The function uses Bresenham's algorithm.
 * The second function, scott_graphics_plot_clip, is a line drawing helper;
 * it handles clipping.
 */

static void
scott_linegraphics_plot_clip(int x, int y, int colour)
{
    /*
     * Clip the plot if the value is outside the context.  Otherwise, plot the
     * pixel as colour.
     */
    if (x >= 0 && x <= MYSTERIOUS_WIDTH && y >= 0 && y < MYSTERIOUS_CLIPHEIGHT) {
        picture_bitmap[y * MYSTERIOUS_WIDTH + x] = colour;
        pixel_to_draw *todraw = VectorOpListPush(&pixels_to_draw);
        todraw->x = x;
        todraw->y = y;
        todraw->colour = colour;
    }
}

int DrawingHowarthVector(void)
{
    return VectorOpListDrawing(&pixels_to_draw);
}

void DrawSomeHowarthVectorPixels(int from_start)
{
    VectorState = DRAWING_VECTOR_IMAGE;
    if (from_start) {
        pixels_to_draw.current = 0;
        vector_background_painted = 0;
    }
    size_t i = pixels_to_draw.current;

    /* Paint the background once per image, before any pixels are plotted. */
    if (!vector_background_painted) {
        RectFill(0, 0, MYSTERIOUS_WIDTH, MYSTERIOUS_CLIPHEIGHT, Remap(bg_colour));
        vector_background_painted = 1;
    }

    /* Emit pixels until we run out or, in slow-draw mode, hit the chunk limit
     * that yields back to the timer loop. */
    size_t chunk_end = i + SCOTT_VECTOR_PIXELS_PER_TICK;
    for (; i < pixels_to_draw.total && (!gli_slowdraw || i < chunk_end); i++) {
        const pixel_to_draw *todraw = VectorOpAt(&pixels_to_draw, i);
        PutPixel(todraw->x, todraw->y, Remap(todraw->colour));
    }
    pixels_to_draw.current = i;

    /* All instructions consumed: stop the timer, transition to "showing",
     * and release the instruction buffer. */
    if (VectorOpListFinishIfDone(&pixels_to_draw))
        vector_background_painted = 0;
}

static void
scott_linegraphics_draw_line(int x1, int y1, int x2, int y2,
    int colour)
{
    /*
     * Byte-exact reimplementation of the original Digital Fantasia /
     * Mysterious Adventures ROM line routine (Golden Baton ram:0x6c06..0x6ca3,
     * reverse-engineered from the ZX Spectrum snapshot in Ghidra). It is a
     * centred Bresenham distinct from the generic Level9-derived one this file
     * used to carry:
     *
     *   - error is initialised to major/2 (round-to-nearest), not 2*minor-major;
     *   - the major axis steps every iteration and the loop runs exactly
     *     `major` times, plotting the NEW position each step, so the START
     *     point is NOT plotted (the previous segment's end already covers it)
     *     and the END point IS;
     *   - a zero-length segment (dx == dy == 0) plots nothing;
     *   - ties (dx == dy) take the x-major branch.
     *
     * The original works in raw coordinates (y up) and plots at screen row
     * 191-y; the caller here has already converted y to screen space, and the
     * iteration is invariant under that negation, so the placement matches
     * pixel-for-pixel. Verified byte-identical to the ROM over Golden Baton's
     * room 0 (1740/1740 line pixels land on the real Spectrum bitmap).
     *
     * The two axes are symmetric, so rather than duplicate the loop we swap
     * x<->y for steep lines and run a single x-major loop, swapping back at the
     * plot. After the swap dx/sx/a are the major axis and dy/sy/b the minor;
     * `steep` keeps the tie-break (dx == dy -> not steep -> x-major) and makes
     * the dx == 0 test fire only on a true zero-length segment.
     */
    int dx, dy, sx, sy, t, acc, a, b, i;

    if (x2 >= x1) { sx = 1;  dx = x2 - x1; } else { sx = -1; dx = x1 - x2; }
    if (y2 >= y1) { sy = 1;  dy = y2 - y1; } else { sy = -1; dy = y1 - y2; }

    int steep = dy > dx;       /* y is the major axis */
    if (steep) {
        t = x1; x1 = y1; y1 = t;
        t = sx; sx = sy; sy = t;
        t = dx; dx = dy; dy = t;
    }
    if (dx == 0)               /* dx == dy == 0: degenerate, plot nothing */
        return;

    acc = dx >> 1;
    a = x1;
    b = y1;
    for (i = 0; i < dx; i++) {
        acc += dy;
        a += sx;
        if (acc >= dx) { acc -= dx; b += sy; }
        if (steep) scott_linegraphics_plot_clip(b, a, colour);
        else       scott_linegraphics_plot_clip(a, b, colour);
    }
}

static int linegraphics_get_pixel(int x, int y)
{
    return picture_bitmap[y * MYSTERIOUS_WIDTH + x];
}

static void diamond_fill(uint8_t x, uint8_t y, int colour)
{
    uint8_t buffer[2048];
    cbuf_handle_t ringbuf = circular_buf_init(buffer, 2048);
    circular_buf_putXY(ringbuf, x, y);
    while (!circular_buf_empty(ringbuf)) {
        circular_buf_getXY(ringbuf, &x, &y);
        /* Match the original ROM fill's interior clamp (Golden Baton
         * ram:0x6cee/0x6cf9/0x6d04/0x6d0f): it never grows into the top screen
         * row or the extreme edge columns, so the picture border acts as an
         * implicit wall. The ROM-exact line rasterizer plots no pixel in row 0,
         * so without this clamp a 4-connected flood escapes along the top edge
         * and floods the whole image (the previous Level9 Bresenham only stayed
         * contained because its ~1px-wide misplacement happened to seal row 0). */
        if (x >= 1 && x < MYSTERIOUS_WIDTH - 1 && y >= 1 && y < MYSTERIOUS_CLIPHEIGHT && linegraphics_get_pixel(x, y) == bg_colour) {
            scott_linegraphics_plot_clip(x, y, colour);
            circular_buf_putXY(ringbuf, x, y + 1);
            circular_buf_putXY(ringbuf, x, y - 1);
            circular_buf_putXY(ringbuf, x + 1, y);
            circular_buf_putXY(ringbuf, x - 1, y);
        }
    }
    circular_buf_free(ringbuf);
}

static inline int ConvertY(int y) { return DRAW_HEIGHT_OFFSET - y; }

void DrawHowarthVectorPicture(int image)
{
    if (image < 0) {
        return;
    }

    // If this image is already shown:
    if (vector_image_shown == image && pixels_to_draw.ops) {
        if (VectorState == SHOWING_VECTOR_IMAGE) {
            return;
        } else {
            if (gli_slowdraw)
                glk_request_timer_events(20);
            DrawSomeHowarthVectorPixels(1);
            return;
        }
    }

    glk_request_timer_events(0);
    vector_image_shown = image;

    // Free any previous pixels and start a new list.
    // scott_linegraphics_plot_clip() will grow this as needed.
    VectorOpListStartSession(&pixels_to_draw);
    vector_background_painted = 0;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
        DefinePalette();
    }
    picture_bitmap = MemAlloc(MYSTERIOUS_WIDTH * MYSTERIOUS_HEIGHT);

    bg_colour = LineImages[image].bgcolour;
    memset(picture_bitmap, bg_colour, MYSTERIOUS_WIDTH * MYSTERIOUS_HEIGHT);

    if (bg_colour == 0)
        line_colour = 7;
    else
        line_colour = 0;

    int x = 0, y = 0, y2 = 0;
    uint8_t arg1, arg2, arg3;
    uint8_t *p = LineImages[image].data;
    uint8_t *end = (p != NULL) ? p + LineImages[image].size : NULL;
    uint8_t opcode = 0;

    while (p < end && opcode != HOWARTH_OP_END) {
        opcode = *(p++);
        /* A picture that is cut off at the end of the file may end in the
           middle of a command: stop rather than read its arguments from
           beyond the data. */
        ptrdiff_t args = (opcode == HOWARTH_OP_MOVE_TO) ? 2
            : (opcode == HOWARTH_OP_FILL) ? 3
            : (opcode == HOWARTH_OP_END) ? 0 : 1;
        if (end - p < args)
            break;
        switch (opcode) {
            case HOWARTH_OP_MOVE_TO:
                y = ConvertY(*(p++));
                x = *(p++);
                break;
            case HOWARTH_OP_FILL:
                arg1 = *(p++);
                arg2 = *(p++);
                arg3 = *(p++);
                diamond_fill(arg3, ConvertY(arg2), arg1);
                break;
            case HOWARTH_OP_END:
                break;
            default: // draw line
                arg1 = *(p++);
                y2 = ConvertY(opcode);
                scott_linegraphics_draw_line(x, y, arg1, y2, line_colour);
                x = arg1;
                y = y2;
                break;
        }
    }

    // Free bitmap memory
    if (picture_bitmap != NULL) {
        free(picture_bitmap);
        picture_bitmap = NULL;
    }

    VectorOpListShrink(&pixels_to_draw);

    // Either draw immediately or use a timer for slow, "animated" drawing
    if (gli_slowdraw)
        glk_request_timer_events(20);
    else
        DrawSomeHowarthVectorPixels(1);
}

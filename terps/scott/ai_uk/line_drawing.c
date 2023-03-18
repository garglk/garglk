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

struct line_image *LineImages;

struct pixel_to_draw {
    uint8_t x;
    uint8_t y;
    uint8_t colour;
};

VectorStateType VectorState = NO_VECTOR_IMAGE;

struct pixel_to_draw **pixels_to_draw = NULL;

int total_draw_instructions = 0;
int current_draw_instruction = 0;
int vector_image_shown = -1;

uint8_t *picture_bitmap = NULL;

int line_colour = 15;
int bg_colour = 0;

int scott_graphics_width = 255;
int scott_graphics_height = 94;

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
     * pixel as colour1 if it is currently colour2.
     */
    if (x >= 0 && x <= scott_graphics_width && y >= 0 && y < scott_graphics_height) {
        picture_bitmap[y * 255 + x] = colour;
        struct pixel_to_draw *todraw = MemAlloc(sizeof(struct pixel_to_draw));
        todraw->x = x;
        todraw->y = y;
        todraw->colour = colour;
        pixels_to_draw[total_draw_instructions++] = todraw;
    }
}

int DrawingVector(void)
{
    return (total_draw_instructions > current_draw_instruction);
}

void FreePixels(void)
{
    for (int i = 0; i < total_draw_instructions; i++)
        if (pixels_to_draw[i] != NULL)
            free(pixels_to_draw[i]);
    free(pixels_to_draw);
    pixels_to_draw = NULL;
}

#ifdef SPATTERLIGHT
extern int gli_slowdraw;
#else
static int gli_slowdraw = 0;
#endif

void DrawSomeVectorPixels(int from_start)
{
    VectorState = DRAWING_VECTOR_IMAGE;
    int i = current_draw_instruction;
    if (from_start)
        i = 0;
    if (i == 0)
        RectFill(0, 0, scott_graphics_width, scott_graphics_height, Remap(bg_colour));
    for (; i < total_draw_instructions && (!gli_slowdraw || i < current_draw_instruction + 50); i++) {
        struct pixel_to_draw todraw = *pixels_to_draw[i];
        PutPixel(todraw.x, todraw.y, Remap(todraw.colour));
    }
    current_draw_instruction = i;
    if (current_draw_instruction >= total_draw_instructions) {
        glk_request_timer_events(0);
        VectorState = SHOWING_VECTOR_IMAGE;
        FreePixels();
    }
}

static void
scott_linegraphics_draw_line(int x1, int y1, int x2, int y2,
    int colour)
{
    int x, y, dx, dy, incx, incy, balance;

    /* Normalize the line into deltas and increments. */
    if (x2 >= x1) {
        dx = x2 - x1;
        incx = 1;
    } else {
        dx = x1 - x2;
        incx = -1;
    }

    if (y2 >= y1) {
        dy = y2 - y1;
        incy = 1;
    } else {
        dy = y1 - y2;
        incy = -1;
    }

    /* Start at x1,y1. */
    x = x1;
    y = y1;

    /* Decide on a direction to progress in. */
    if (dx >= dy) {
        dy <<= 1;
        balance = dy - dx;
        dx <<= 1;

        /* Loop until we reach the end point of the line. */
        while (x != x2) {
            scott_linegraphics_plot_clip(x, y, colour);
            if (balance >= 0) {
                y += incy;
                balance -= dx;
            }
            balance += dy;
            x += incx;
        }
        scott_linegraphics_plot_clip(x, y, colour);
    } else {
        dx <<= 1;
        balance = dx - dy;
        dy <<= 1;

        /* Loop until we reach the end point of the line. */
        while (y != y2) {
            scott_linegraphics_plot_clip(x, y, colour);
            if (balance >= 0) {
                x += incx;
                balance -= dy;
            }
            balance += dx;
            y += incy;
        }
        scott_linegraphics_plot_clip(x, y, colour);
    }
}

static int linegraphics_get_pixel(int x, int y)
{
    return picture_bitmap[y * 255 + x];
}

static void diamond_fill(uint8_t x, uint8_t y, int colour)
{
    uint8_t buffer[2048];
    cbuf_handle_t ringbuf = circular_buf_init(buffer, 2048);
    circular_buf_putXY(ringbuf, x, y);
    while (!circular_buf_empty(ringbuf)) {
        circular_buf_getXY(ringbuf, &x, &y);
        if (x >= 0 && x < scott_graphics_width && y >= 0 && y < scott_graphics_height && linegraphics_get_pixel(x, y) == bg_colour) {
            scott_linegraphics_plot_clip(x, y, colour);
            circular_buf_putXY(ringbuf, x, y + 1);
            circular_buf_putXY(ringbuf, x, y - 1);
            circular_buf_putXY(ringbuf, x + 1, y);
            circular_buf_putXY(ringbuf, x - 1, y);
        }
    }
    circular_buf_free(ringbuf);
}

void DrawVectorPicture(int image)
{
    if (image < 0) {
        return;
    }

    if (vector_image_shown == image && pixels_to_draw) {
        if (VectorState == SHOWING_VECTOR_IMAGE) {
            return;
        } else {
            if (gli_slowdraw)
                glk_request_timer_events(20);
            DrawSomeVectorPixels(1);
            return;
        }
    }

    glk_request_timer_events(0);
    vector_image_shown = image;
    if (pixels_to_draw != NULL)
        FreePixels();
    pixels_to_draw = MemAlloc(255 * 97 * sizeof(struct pixel_to_draw *));
    memset(pixels_to_draw, 0, 255 * 97 * sizeof(struct pixel_to_draw *));
    total_draw_instructions = 0;
    current_draw_instruction = 0;

    if (palchosen == NO_PALETTE) {
        palchosen = Game->palette;
        DefinePalette();
    }
    picture_bitmap = MemAlloc(255 * 97);
    bg_colour = LineImages[image].bgcolour;
    memset(picture_bitmap, bg_colour, 255 * 97);
    if (bg_colour == 0)
        line_colour = 7;
    else
        line_colour = 0;
    int x = 0, y = 0, y2 = 0;
    uint8_t arg1, arg2, arg3;
    uint8_t *p = LineImages[image].data;
    uint8_t opcode = 0;
    while (p - LineImages[image].data < LineImages[image].size && opcode != 0xff) {
        if (p > entire_file + file_length) {
            fprintf(stderr, "Out of range! Opcode: %x. Image: %d. LineImages[%d].size: %zu\n", opcode, image,
                image, LineImages[image].size);
            break;
        }
        opcode = *(p++);
        switch (opcode) {
        case 0xc0:
            y = 190 - *(p++);
            x = *(p++);
            break;
        case 0xc1:
            arg1 = *(p++);
            arg2 = *(p++);
            arg3 = *(p++);
            diamond_fill(arg3, 190 - arg2, arg1);
            break;
        case 0xff:
            break;
        default:
            arg1 = *(p++);
            y2 = 190 - opcode;
            scott_linegraphics_draw_line(x, y, arg1, y2, line_colour);
            x = arg1;
            y = y2;
            break;
        }
    }
    if (picture_bitmap != NULL) {
        free(picture_bitmap);
        picture_bitmap = NULL;
    }
    if (gli_slowdraw)
        glk_request_timer_events(20);
    else
        DrawSomeVectorPixels(1);
}

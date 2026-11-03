//
//  zx_title.c
//  Shared ZX Spectrum loading-screen (SCREEN$) title renderer for the
//  TaylorMade (taylor) and unquill interpreters. See zx_title.h.
//

#include <stdlib.h>
#include <string.h>

#include "zx_title.h"

#ifdef SPATTERLIGHT
/* When the front end has graphics disabled (e.g. mid-session), bail out of the
 * redraw on resize rather than painting into a window that is going away. */
extern int gli_enable_graphics;
#endif

/* Reveal timing: ~ZX_REVEAL_TICKS timer steps to paint the whole screen. */
#define ZX_REVEAL_TICK_MS  30
#define ZX_REVEAL_TICKS    120

void ZXTitleRelayout(ZXTitle *title)
{
    glui32 win_w = 0, win_h = 0;
    glk_window_get_size(title->window, &win_w, &win_h);

    int sx = (int)win_w / ZX_SCREEN_WIDTH;
    int sy = (int)win_h / ZX_SCREEN_HEIGHT;
    int scale = (sx < sy) ? sx : sy;
    if (scale < 1)
        scale = 1;

    int div = (title->vcentre_div > 0) ? title->vcentre_div : 2;
    title->scale = scale;
    title->origin_x = ((int)win_w - ZX_SCREEN_WIDTH * scale) / 2;
    title->origin_y = ((int)win_h - ZX_SCREEN_HEIGHT * scale) / div;
    if (title->origin_x < 0)
        title->origin_x = 0;
    if (title->origin_y < 0)
        title->origin_y = 0;
}

/* Draw the 8 pixels of one display-file bitmap byte, coalescing adjacent pixels
 * of the same colour into a single fill_rect. */
static void draw_byte(ZXTitle *t, const uint8_t *screen, int bitmap_addr)
{
    int offset = bitmap_addr - ZX_BITMAP_BASE;
    int col = offset & 0x1f;
    int y = ZXBitmapRow(offset);
    uint8_t bits = screen[offset];
    uint8_t attr = screen[ZX_BITMAP_SIZE + (y >> 3) * ZX_SCREEN_COLS + col];
    int ink, paper;
    ZXDecodeAttr(attr, &ink, &paper);   /* the flash bit is ignored */
    glui32 ink_colour = t->palette[ink];
    glui32 paper_colour = t->palette[paper];

    int b = 0;
    while (b < 8) {
        int set = (bits >> (7 - b)) & 1;
        int start = b;
        while (b < 8 && (((bits >> (7 - b)) & 1) == set))
            b++;
        glk_window_fill_rect(t->window, set ? ink_colour : paper_colour,
            t->origin_x + (col * 8 + start) * t->scale,
            t->origin_y + y * t->scale,
            (b - start) * t->scale, t->scale);
    }
}

/* Redraw one screen address: a single bitmap byte, or (for an attribute
 * address) every bitmap byte of the 8x8 character cell it colours. */
static void redraw_cell(ZXTitle *t, const uint8_t *screen, int addr)
{
    if (addr < ZX_ATTR_BASE) {
        draw_byte(t, screen, addr);
        return;
    }
    int cell = addr - ZX_ATTR_BASE;
    int crow = cell >> 5, ccol = cell & 0x1f;
    for (int py = 0; py < 8; py++) {
        int y = crow * 8 + py;
        draw_byte(t, screen, ZX_BITMAP_BASE + ZXBitmapOffset(y, ccol));
    }
}

void ZXTitleDraw(ZXTitle *title, const uint8_t *screen)
{
    if (!screen || !title->window)
        return;
    for (int y = 0; y < ZX_SCREEN_HEIGHT; y++)
        for (int col = 0; col < ZX_SCREEN_COLS; col++)
            draw_byte(title, screen, ZX_BITMAP_BASE + ZXBitmapOffset(y, col));
}

/* Wait for a key on keywin, relaying out and redrawing the whole screen on
 * each resize. */
static void wait_for_key(ZXTitle *t, const uint8_t *screen, winid_t keywin)
{
    glk_request_char_event(keywin);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            ZXTitleRelayout(t);
            glk_window_clear(t->window);
            ZXTitleDraw(t, screen);
        }
    } while (ev.type != evtype_CharInput);
}

/* Reveal the screen progressively, in the given draw order (or linear top-to-
 * bottom when none), on a timer. A keypress dismisses it. */
static void slow_reveal(ZXTitle *t, const uint8_t *screen, winid_t keywin,
    const uint16_t *order, int order_len)
{
    static uint16_t linear[ZX_SCREEN_SIZE];
    if (!order || order_len <= 0) {
        for (int i = 0; i < ZX_SCREEN_SIZE; i++)
            linear[i] = (uint16_t)(ZX_BITMAP_BASE + i);
        order = linear;
        order_len = ZX_SCREEN_SIZE;
    }

    /* Background: the finished screen with every cell we are about to reveal
     * blanked out (bitmap to black, attribute to the screen's most common
     * attribute byte, so the unrevealed area shares the picture's border). */
    static uint8_t work[ZX_SCREEN_SIZE];
    memcpy(work, screen, ZX_SCREEN_SIZE);
    int counts[256] = { 0 };
    for (int i = ZX_BITMAP_SIZE; i < ZX_SCREEN_SIZE; i++)
        counts[screen[i]]++;
    int fill = 0;
    for (int v = 1; v < 256; v++)
        if (counts[v] > counts[fill])
            fill = v;
    for (int i = 0; i < order_len; i++) {
        int off = order[i] - ZX_BITMAP_BASE;
        work[off] = (order[i] < ZX_ATTR_BASE) ? 0x00 : (uint8_t)fill;
    }

    glk_window_clear(t->window);
    ZXTitleDraw(t, work);

    int per_tick = order_len / ZX_REVEAL_TICKS;
    if (per_tick < 1)
        per_tick = 1;
    int pos = 0;

    glk_request_char_event(keywin);
    glk_request_timer_events(ZX_REVEAL_TICK_MS);

    event_t ev;
    do {
        glk_select(&ev);
        if (ev.type == evtype_Timer) {
            int end = pos + per_tick;
            if (end > order_len)
                end = order_len;
            for (; pos < end; pos++) {
                int off = order[pos] - ZX_BITMAP_BASE;
                work[off] = screen[off];
                redraw_cell(t, work, order[pos]);
            }
            if (pos >= order_len)
                glk_request_timer_events(0);
        } else if (ev.type == evtype_Arrange) {
#ifdef SPATTERLIGHT
            if (!gli_enable_graphics)
                break;
#endif
            ZXTitleRelayout(t);
            glk_window_clear(t->window);
            ZXTitleDraw(t, work);
        }
    } while (ev.type != evtype_CharInput);

    glk_request_timer_events(0);
}

void ZXTitleShow(ZXTitle *title, const uint8_t *screen, winid_t keywin,
    int slow, const uint16_t *order, int order_len)
{
    if (!screen || !title->window || !keywin)
        return;

    ZXTitleRelayout(title);
    if (slow) {
        slow_reveal(title, screen, keywin, order, order_len);
    } else {
        ZXTitleDraw(title, screen);
        wait_for_key(title, screen, keywin);
    }
}

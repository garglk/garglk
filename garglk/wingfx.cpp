// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2010 by Ben Cressey.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <memory>

#include "glk.h"
#include "garglk.h"

#define mul255(a,b) (((short)(a) * (b) + 127) / 255)

static void
drawpicture(picture_t *src, window_graphics_t *dst,
    int x0, int y0, int width, int height, glui32 linkval);

static void win_graphics_touch(window_graphics_t *dest)
{
    dest->dirty = true;
    winrepaint(
            dest->owner->bbox.x0,
            dest->owner->bbox.y0,
            dest->owner->bbox.x1,
            dest->owner->bbox.y1);
}

window_graphics_t *win_graphics_create(window_t *win)
{
    if (!gli_conf_graphics) {
        return nullptr;
    }

    return new window_graphics_t(win);
}

void win_graphics_destroy(window_graphics_t *dwin)
{
    delete dwin;
}

void win_graphics_rearrange(window_t *win, rect_t *box)
{
    window_graphics_t *dwin = win->window.graphics;
    int newwid, newhgt;
    int oldw, oldh;

    win->bbox = *box;

    newwid = box->x1 - box->x0;
    newhgt = box->y1 - box->y0;
    oldw = dwin->w;
    oldh = dwin->h;

    if (newwid <= 0 || newhgt <= 0) {
        dwin->w = 0;
        dwin->h = 0;
        dwin->rgb.clear();
        return;
    }

    dwin->rgb.resize(newwid, newhgt, true);
    dwin->w = newwid;
    dwin->h = newhgt;

    if (newwid > oldw) {
        win_graphics_erase_rect(dwin, false, oldw, 0, newwid - oldw, newhgt);
    }
    if (newhgt > oldh) {
        win_graphics_erase_rect(dwin, false, 0, oldh, newwid, newhgt - oldh);
    }

    win_graphics_touch(dwin);
}

void win_graphics_get_size(window_t *win, glui32 *width, glui32 *height)
{
    window_graphics_t *dwin = win->window.graphics;
    *width = dwin->w;
    *height = dwin->h;
}

void win_graphics_redraw(window_t *win)
{
    window_graphics_t *dwin = win->window.graphics;
    int x, y;

    int x0 = win->bbox.x0;
    int y0 = win->bbox.y0;

    if (dwin->dirty || gli_force_redraw) {
        dwin->dirty = false;

        if (dwin->rgb.empty()) {
            return;
        }

        for (y = 0; y < dwin->h; y++) {
            for (x = 0; x < dwin->w; x++) {
                gli_draw_pixel(x + x0, y + y0, dwin->rgb[y][x]);
            }
        }
    }
}

void win_graphics_click(window_graphics_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;
    int x = sx - win->bbox.x0;
    int y = sy - win->bbox.y0;

    if (win->mouse_request) {
        gli_event_store(evtype_MouseInput, win, gli_unzoom_int(x), gli_unzoom_int(y));
        win->mouse_request = false;
        if (gli_conf_safeclicks) {
            gli_forceclick = true;
        }
    }

    if (win->hyper_request) {
        glui32 linkval = gli_get_hyperlink(gli_unzoom_int(sx), gli_unzoom_int(sy));
        if (linkval) {
            gli_event_store(evtype_Hyperlink, win, linkval, 0);
            win->hyper_request = false;
            if (gli_conf_safeclicks) {
                gli_forceclick = true;
            }
        }
    }
}

bool win_graphics_draw_picture(window_graphics_t *dwin,
    glui32 image,
    glsi32 xpos, glsi32 ypos,
    bool scale, glui32 imagewidth, glui32 imageheight)
{
    auto pic = gli_picture_load(image);
    glui32 hyperlink = dwin->owner->attr.hyper;
    xpos = gli_zoom_int(xpos);
    ypos = gli_zoom_int(ypos);

    if (!pic) {
        return false;
    }

    if (!dwin->owner->image_loaded) {
        gli_piclist_increment();
        dwin->owner->image_loaded = true;
    }

    if (!scale) {
        imagewidth = pic->w;
        imageheight = pic->h;
    }
    imagewidth = gli_zoom_int(imagewidth);
    imageheight = gli_zoom_int(imageheight);

    drawpicture(pic.get(), dwin, xpos, ypos, imagewidth, imageheight, hyperlink);

    win_graphics_touch(dwin);

    return true;
}

void win_graphics_erase_rect(window_graphics_t *dwin, bool whole,
    glsi32 x0, glsi32 y0, glui32 width, glui32 height)
{
    int x1 = x0 + width;
    int y1 = y0 + height;
    int x, y;
    int hx0, hx1, hy0, hy1;

    if (whole) {
        x0 = 0;
        y0 = 0;
        x1 = dwin->w;
        y1 = dwin->h;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x0 >= dwin->w) {
        x0 = dwin->w;
    }
    if (y0 >= dwin->h) {
        y0 = dwin->h;
    }
    if (x1 >= dwin->w) {
        x1 = dwin->w;
    }
    if (y1 >= dwin->h) {
        y1 = dwin->h;
    }

    hx0 = dwin->owner->bbox.x0 + x0;
    hx1 = dwin->owner->bbox.x0 + x1;
    hy0 = dwin->owner->bbox.y0 + y0;
    hy1 = dwin->owner->bbox.y0 + y1;

    // zero out hyperlinks for these coordinates
    gli_put_hyperlink(0, hx0, hy0, hx1, hy1);

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            dwin->rgb[y][x] = dwin->bgnd;
        }
    }

    win_graphics_touch(dwin);
}

void win_graphics_fill_rect(window_graphics_t *dwin, glui32 color,
    glsi32 x0, glsi32 y0, glui32 width, glui32 height)
{
    int x1 = x0 + width;
    int y1 = y0 + height;
    x0 = gli_zoom_int(x0);
    y0 = gli_zoom_int(y0);
    x1 = gli_zoom_int(x1);
    y1 = gli_zoom_int(y1);
    int x, y;
    int hx0, hx1, hy0, hy1;

    Pixel<3> col((color >> 16) & 0xff,
                 (color >> 8) & 0xff,
                 (color >> 0) & 0xff);

    x0 = garglk::clamp(x0, 0, dwin->w);
    y0 = garglk::clamp(y0, 0, dwin->h);
    x1 = garglk::clamp(x1, 0, dwin->w);
    y1 = garglk::clamp(y1, 0, dwin->h);

    hx0 = dwin->owner->bbox.x0 + x0;
    hx1 = dwin->owner->bbox.x0 + x1;
    hy0 = dwin->owner->bbox.y0 + y0;
    hy1 = dwin->owner->bbox.y0 + y1;

    // zero out hyperlinks for these coordinates
    gli_put_hyperlink(0, hx0, hy0, hx1, hy1);

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            dwin->rgb[y][x] = col;
        }
    }

    win_graphics_touch(dwin);
}

void win_graphics_set_background_color(window_graphics_t *dwin, glui32 color)
{
    dwin->bgnd = Pixel<3>((color >> 16) & 0xff,
                          (color >> 8) & 0xff,
                          (color >> 0) & 0xff);
}

static void drawpicture(picture_t *src, window_graphics_t *dst,
    int x0, int y0, int width, int height, glui32 linkval)
{
    int dx1, dy1, x1, y1, sx0, sy0, sx1, sy1;
    int w, h;
    int hx0, hx1, hy0, hy1;
    std::shared_ptr<picture_t> scaled;

    if (width != src->w || height != src->h) {
        scaled = gli_picture_scale(src, width, height);
        if (!scaled) {
            return;
        }
        src = scaled.get();
    }

    sx0 = 0;
    sy0 = 0;
    sx1 = src->w;
    sy1 = src->h;
    dx1 = dst->w;
    dy1 = dst->h;

    x1 = x0 + src->w;
    y1 = y0 + src->h;

    if (x1 <= 0 || x0 >= dx1) {
        return;
    }
    if (y1 <= 0 || y0 >= dy1) {
        return;
    }
    if (x0 < 0) {
        sx0 -= x0;
        x0 = 0;
    }
    if (y0 < 0) {
        sy0 -= y0;
        y0 = 0;
    }
    if (x1 > dx1) {
        sx1 += dx1 - x1;
        x1 = dx1;
    }
    if (y1 > dy1) {
        sy1 += dy1 - y1;
        y1 = dy1;
    }

    hx0 = dst->owner->bbox.x0 + x0;
    hx1 = dst->owner->bbox.x0 + x1;
    hy0 = dst->owner->bbox.y0 + y0;
    hy1 = dst->owner->bbox.y0 + y1;

    // zero out or set hyperlink for these coordinates
    gli_put_hyperlink(linkval, hx0, hy0, hx1, hy1);

    w = sx1 - sx0;
    h = sy1 - sy0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            auto existing = dst->rgb[y + y0][x + x0];
            unsigned char sa = src->rgba[y + sy0][x + sx0][3];
            unsigned char na = 255 - sa;
            unsigned char sr = mul255(src->rgba[y + sy0][x + sx0][0], sa);
            unsigned char sg = mul255(src->rgba[y + sy0][x + sx0][1], sa);
            unsigned char sb = mul255(src->rgba[y + sy0][x + sx0][2], sa);
            dst->rgb[y + y0][x + x0] =Pixel<3>(sr + mul255(existing[0], na),
                                               sg + mul255(existing[1], na),
                                               sb + mul255(existing[2], na));
        }
    }
}

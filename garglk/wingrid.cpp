// Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.
// Copyright (C) 2010 by Ben Cressey, Chris Spiegel, Jörg Walter.
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

#include <algorithm>

#include "glk.h"
#include "garglk.h"

// A grid of characters. We store the window as a list of lines.
// Within a line, just store an array of characters and an array
// of style bytes, the same size.

static void touch(window_textgrid_t *dwin, int line)
{
    window_t *win = dwin->owner;
    int y = win->bbox.y0 + line * gli_leading;
    dwin->lines[line].dirty = true;
    winrepaint(win->bbox.x0, y, win->bbox.x1, y + gli_leading);
}

window_textgrid_t *win_textgrid_create(window_t *win)
{
    return new window_textgrid_t(win, gli_gstyles);
}

void win_textgrid_destroy(window_textgrid_t *dwin)
{
    if (dwin->inbuf != nullptr) {
        if (gli_unregister_arr != nullptr) {
            const char *typedesc = (dwin->inunicode ? "&+#!Iu" : "&+#!Cn");
            (*gli_unregister_arr)(dwin->inbuf, dwin->inoriglen, const_cast<char *>(typedesc), dwin->inarrayrock);
        }
        dwin->inbuf = nullptr;
    }

    delete dwin;
}

void win_textgrid_rearrange(window_t *win, rect_t *box)
{
    int newwid, newhgt;
    int k;
    window_textgrid_t *dwin = win->window.textgrid;
    dwin->owner->bbox = *box;

    newwid = (box->x1 - box->x0) / gli_cellw;
    newhgt = (box->y1 - box->y0) / gli_cellh;

    if (newwid == dwin->width && newhgt == dwin->height) {
        return;
    }

    for (k = dwin->height; k < newhgt; k++) {
        dwin->lines[k].chars.fill(' ');
        dwin->lines[k].attrs.fill(attr_t{});
    }

    dwin->owner->attr.clear();
    dwin->width = newwid;
    dwin->height = newhgt;

    for (k = 0; k < dwin->height; k++) {
        touch(dwin, k);
        std::fill(dwin->lines[k].chars.begin() + dwin->width, dwin->lines[k].chars.end(), ' ');
        auto *attr_end = (&dwin->lines[k].attrs[0]) + dwin->lines[k].attrs.size();
        for (auto *attr = &dwin->lines[k].attrs[dwin->width]; attr < attr_end; ++attr) {
            attr->clear();
        }
    }
}

void win_textgrid_redraw(window_t *win)
{
    window_textgrid_t *dwin = win->window.textgrid;
    tgline_t *ln;
    int x0, y0;
    int x, y, w;
    int i, a, b, k, o;
    glui32 link;

    x0 = win->bbox.x0;
    y0 = win->bbox.y0;

    for (i = 0; i < dwin->height; i++) {
        ln = &dwin->lines[i];
        if (ln->dirty || gli_force_redraw) {
            ln->dirty = false;

            x = x0;
            y = y0 + i * gli_leading;

            // clear any stored hyperlink coordinates
            gli_put_hyperlink(0, x0, y, x0 + gli_cellw * dwin->width, y + gli_leading);

            a = 0;
            for (b = 0; b < dwin->width; b++) {
                if (ln->attrs[a] != ln->attrs[b]) {
                    link = ln->attrs[a].hyper;
                    auto font = ln->attrs[a].font(dwin->styles);
                    Color fgcolor = link != 0 ? gli_link_color : ln->attrs[a].fg(dwin->styles);
                    Color bgcolor = ln->attrs[a].bg(dwin->styles);
                    w = (b - a) * gli_cellw;
                    gli_draw_rect(x, y, w, gli_leading, bgcolor);
                    o = x;
                    for (k = a; k < b; k++) {
                        gli_draw_string_uni(o * GLI_SUBPIX,
                                y + gli_baseline, font, fgcolor,
                                &ln->chars[k], 1, -1);
                        o += gli_cellw;
                    }
                    if (link != 0) {
                        if (gli_underline_hyperlinks) {
                            gli_draw_rect(x, y + gli_baseline + 1, w,
                                        1, gli_link_color);
                        }
                        gli_put_hyperlink(link, x, y, x + w, y + gli_leading);
                    }
                    x += w;
                    a = b;
                }
            }
            link = ln->attrs[a].hyper;
            auto font = ln->attrs[a].font(dwin->styles);
            Color fgcolor = link != 0 ? gli_link_color : ln->attrs[a].fg(dwin->styles);
            Color bgcolor = ln->attrs[a].bg(dwin->styles);
            w = (b - a) * gli_cellw;
            w += win->bbox.x1 - (x + w);
            gli_draw_rect(x, y, w, gli_leading, bgcolor);
            o = x;
            for (k = a; k < b; k++) {
                gli_draw_string_uni(o * GLI_SUBPIX,
                                    y + gli_baseline, font, fgcolor,
                                    &ln->chars[k], 1, -1);
                o += gli_cellw;
            }
            if (link != 0) {
                if (gli_underline_hyperlinks) {
                    gli_draw_rect(x, y + gli_baseline + 1, w,
                                1, gli_link_color);
                }
                gli_put_hyperlink(link, x, y, x + w, y + gli_leading);
            }
        }
    }
}

void win_textgrid_putchar_uni(window_t *win, glui32 ch)
{
    window_textgrid_t *dwin = win->window.textgrid;
    tgline_t *ln;

    // Canonicalize the cursor position. That is, the cursor may have been
    // left outside the window area; wrap it if necessary.
    if (dwin->curx < 0) {
        dwin->curx = 0;
    } else if (dwin->curx >= dwin->width) {
        dwin->curx = 0;
        dwin->cury++;
    }
    if (dwin->cury < 0) {
        dwin->cury = 0;
    } else if (dwin->cury >= dwin->height) {
        return; // outside the window
    }

    if (ch == '\n') {
        // a newline just moves the cursor.
        dwin->cury++;
        dwin->curx = 0;
        return;
    }

    touch(dwin, dwin->cury);

    ln = &(dwin->lines[dwin->cury]);
    ln->chars[dwin->curx] = ch;
    ln->attrs[dwin->curx] = win->attr;

    dwin->curx++;
    // We can leave the cursor outside the window, since it will be
    // canonicalized next time a character is printed.
}

bool win_textgrid_unputchar_uni(window_t *win, glui32 ch)
{
    window_textgrid_t *dwin = win->window.textgrid;
    tgline_t *ln;
    int oldx = dwin->curx, oldy = dwin->cury;

    // Move the cursor back.
    if (dwin->curx >= dwin->width) {
        dwin->curx = dwin->width - 1;
    } else {
        dwin->curx--;
    }

    // Canonicalize the cursor position. That is, the cursor may have been
    // left outside the window area; wrap it if necessary.
    if (dwin->curx < 0) {
        dwin->curx = dwin->width - 1;
        dwin->cury--;
    }
    if (dwin->cury < 0) {
        dwin->cury = 0;
    } else if (dwin->cury >= dwin->height) {
        return false; // outside the window
    }

    if (ch == '\n') {
        // a newline just moves the cursor.
        if (dwin->curx == dwin->width - 1) {
            return true; // deleted a newline
        }
        dwin->curx = oldx;
        dwin->cury = oldy;
        return false; // it wasn't there
    }

    ln = &(dwin->lines[dwin->cury]);
    if (glk_char_to_upper(ln->chars[dwin->curx]) == glk_char_to_upper(ch)) {
        ln->chars[dwin->curx] = ' ';
        ln->attrs[dwin->curx].clear();
        touch(dwin, dwin->cury);
        return true; // deleted the char
    } else {
        dwin->curx = oldx;
        dwin->cury = oldy;
        return false; // it wasn't there
    }
}

void win_textgrid_clear(window_t *win)
{
    window_textgrid_t *dwin = win->window.textgrid;
    int k;

    win->attr.fgcolor = gli_override_fg;
    win->attr.bgcolor = gli_override_bg;
    win->attr.reverse = false;

    for (k = 0; k < dwin->height; k++) {
        touch(dwin, k);
        dwin->lines[k].chars.fill(' ');
        dwin->lines[k].attrs.fill(attr_t{});
    }

    dwin->curx = 0;
    dwin->cury = 0;
}

void win_textgrid_move_cursor(window_t *win, int xpos, int ypos)
{
    window_textgrid_t *dwin = win->window.textgrid;

    // If the values are negative, they're really huge positive numbers --
    // remember that they were cast from glui32. So set them huge and
    // let canonicalization take its course.
    if (xpos < 0) {
        xpos = 32767;
    }
    if (ypos < 0) {
        ypos = 32767;
    }

    dwin->curx = xpos;
    dwin->cury = ypos;
}

void win_textgrid_click(window_textgrid_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;
    int x = sx - win->bbox.x0;
    int y = sy - win->bbox.y0;

    if (win->line_request || win->char_request
        || win->line_request_uni || win->char_request_uni
        || win->more_request || win->scroll_request) {
        gli_focuswin = win;
    }

    if (win->mouse_request) {
        gli_event_store(evtype_MouseInput, win, x / gli_cellw, y / gli_leading);
        win->mouse_request = false;
        if (gli_conf_safeclicks) {
            gli_forceclick = true;
        }
    }

    if (win->hyper_request) {
        glui32 linkval = gli_get_hyperlink(sx, sy);
        if (linkval != 0) {
            gli_event_store(evtype_Hyperlink, win, linkval, 0);
            win->hyper_request = false;
            if (gli_conf_safeclicks) {
                gli_forceclick = true;
            }
        }
    }
}

// Prepare the window for line input.
static void win_textgrid_init_impl(window_t *win, void *buf, int maxlen, int initlen, bool unicode)
{
    window_textgrid_t *dwin = win->window.textgrid;

    dwin->inunicode = unicode;
    dwin->inoriglen = maxlen;
    if (maxlen > (dwin->width - dwin->curx)) {
        maxlen = (dwin->width - dwin->curx);
    }

    dwin->inbuf = buf;
    dwin->inmax = maxlen;
    dwin->inlen = 0;
    dwin->incurs = 0;
    dwin->inorgx = dwin->curx;
    dwin->inorgy = dwin->cury;
    dwin->origattr = win->attr;
    win->attr.set(style_Input);

    if (initlen > maxlen) {
        initlen = maxlen;
    }

    if (initlen != 0) {
        int ix;
        tgline_t *ln = &(dwin->lines[dwin->inorgy]);

        for (ix = 0; ix < initlen; ix++) {
            ln->attrs[dwin->inorgx + ix].set(style_Input);
            if (unicode) {
                ln->chars[dwin->inorgx + ix] = (static_cast<glui32 *>(buf))[ix];
            } else {
                ln->chars[dwin->inorgx + ix] = (static_cast<unsigned char *>(buf))[ix];
            }
        }

        dwin->incurs += initlen;
        dwin->inlen += initlen;
        dwin->curx = dwin->inorgx + dwin->incurs;
        dwin->cury = dwin->inorgy;

        touch(dwin, dwin->inorgy);
    }

    dwin->line_terminators = win->line_terminators;

    if (gli_register_arr != nullptr) {
        dwin->inarrayrock = (*gli_register_arr)(dwin->inbuf, dwin->inoriglen, const_cast<char *>(unicode ? "&+#!Iu" : "&+#!Cn"));
    }
}

void win_textgrid_init_line(window_t *win, char *buf, int maxlen, int initlen)
{
    win_textgrid_init_impl(win, buf, maxlen, initlen, false);
}

void win_textgrid_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen)
{
    win_textgrid_init_impl(win, buf, maxlen, initlen, true);
}

// Abort line input, storing whatever's been typed so far.
void win_textgrid_cancel_line(window_t *win, event_t *ev)
{
    int ix;
    void *inbuf;
    int inoriglen;
    bool inunicode;
    gidispatch_rock_t inarrayrock;
    window_textgrid_t *dwin = win->window.textgrid;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (dwin->inbuf == nullptr) {
        return;
    }

    inbuf = dwin->inbuf;
    inoriglen = dwin->inoriglen;
    inarrayrock = dwin->inarrayrock;
    inunicode = dwin->inunicode;

    if (!inunicode) {
        for (ix = 0; ix < dwin->inlen; ix++) {
            glui32 ch = ln->chars[dwin->inorgx + ix];
            if (ch > 0xff) {
                ch = '?';
            }
            (static_cast<char *>(inbuf))[ix] = static_cast<char>(ch);
        }
        if (win->echostr != nullptr) {
            gli_stream_echo_line(win->echostr, static_cast<char *>(inbuf), dwin->inlen);
        }
    } else {
        for (ix = 0; ix < dwin->inlen; ix++) {
            (static_cast<glui32 *>(inbuf))[ix] = ln->chars[dwin->inorgx + ix];
        }
        if (win->echostr != nullptr) {
            gli_stream_echo_line_uni(win->echostr, static_cast<glui32 *>(inbuf), dwin->inlen);
        }
    }

    dwin->cury = dwin->inorgy + 1;
    dwin->curx = 0;
    win->attr = dwin->origattr;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = dwin->inlen;
    ev->val2 = 0;

    win->line_request = false;
    win->line_request_uni = false;
    dwin->line_terminators.clear();
    dwin->inbuf = nullptr;
    dwin->inoriglen = 0;
    dwin->inmax = 0;
    dwin->inorgx = 0;
    dwin->inorgy = 0;

    if (gli_unregister_arr != nullptr) {
        const char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inoriglen, const_cast<char *>(typedesc), inarrayrock);
    }
}

// Keybinding functions.

// Any key, during character input. Ends character input.
void gcmd_grid_accept_readchar(window_t *win, glui32 arg)
{
    glui32 key;

    switch (arg) {
    case keycode_Erase:
        key = keycode_Delete;
        break;
    case keycode_MouseWheelUp:
    case keycode_MouseWheelDown:
        return;
    default:
        key = arg;
    }

    if (key > 0xff && key < (0xffffffff - keycode_MAXVAL + 1)) {
        if (!(win->char_request_uni) || key > 0x10ffff) {
            key = keycode_Unknown;
        }
    }

    win->char_request = false;
    win->char_request_uni = false;
    gli_event_store(evtype_CharInput, win, key, 0);
}

// Return or enter, during line input. Ends line input.
static void acceptline(window_t *win, glui32 keycode)
{
    int ix;
    void *inbuf;
    int inoriglen;
    bool inunicode;
    gidispatch_rock_t inarrayrock;
    window_textgrid_t *dwin = win->window.textgrid;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (dwin->inbuf == nullptr) {
        return;
    }

    inbuf = dwin->inbuf;
    inoriglen = dwin->inoriglen;
    inarrayrock = dwin->inarrayrock;
    inunicode = dwin->inunicode;

    if (!inunicode) {
        for (ix = 0; ix < dwin->inlen; ix++) {
            (static_cast<char *>(inbuf))[ix] = static_cast<char>(ln->chars[dwin->inorgx + ix]);
        }
        if (win->echostr != nullptr) {
            gli_stream_echo_line(win->echostr, static_cast<char *>(inbuf), dwin->inlen);
        }
    } else {
        for (ix = 0; ix < dwin->inlen; ix++) {
            (static_cast<glui32 *>(inbuf))[ix] = ln->chars[dwin->inorgx + ix];
        }
        if (win->echostr != nullptr) {
            gli_stream_echo_line_uni(win->echostr, static_cast<glui32 *>(inbuf), dwin->inlen);
        }
    }

    dwin->cury = dwin->inorgy + 1;
    dwin->curx = 0;
    win->attr = dwin->origattr;

    if (!dwin->line_terminators.empty()) {
        glui32 val2 = keycode;
        if (val2 == keycode_Return) {
            val2 = 0;
        }
        gli_event_store(evtype_LineInput, win, dwin->inlen, val2);
        dwin->line_terminators.clear();
    } else {
        gli_event_store(evtype_LineInput, win, dwin->inlen, 0);
    }
    win->line_request = false;
    win->line_request_uni = false;
    dwin->inbuf = nullptr;
    dwin->inoriglen = 0;
    dwin->inmax = 0;
    dwin->inorgx = 0;
    dwin->inorgy = 0;

    if (gli_unregister_arr != nullptr) {
        const char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inoriglen, const_cast<char *>(typedesc), inarrayrock);
    }
}

// Any regular key, during line input.
void gcmd_grid_accept_readline(window_t *win, glui32 arg)
{
    int ix;
    window_textgrid_t *dwin = win->window.textgrid;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (dwin->inbuf == nullptr) {
        return;
    }

    if (!dwin->line_terminators.empty() && gli_window_check_terminator(arg)) {
        if (std::find(dwin->line_terminators.begin(), dwin->line_terminators.end(), arg) != dwin->line_terminators.end()) {
            acceptline(win, arg);
            return;
        }
    }

    switch (arg) {

    // Delete keys, during line input.

    case keycode_Delete:
        if (dwin->inlen <= 0) {
            return;
        }
        if (dwin->incurs <= 0) {
            return;
        }
        for (ix = dwin->incurs; ix < dwin->inlen; ix++) {
            ln->chars[dwin->inorgx + ix - 1] = ln->chars[dwin->inorgx + ix];
        }
        ln->chars[dwin->inorgx + dwin->inlen - 1] = ' ';
        dwin->incurs--;
        dwin->inlen--;
        break;

    case keycode_Erase:
        if (dwin->inlen <= 0) {
            return;
        }
        if (dwin->incurs >= dwin->inlen) {
            return;
        }
        for (ix = dwin->incurs; ix < dwin->inlen - 1; ix++) {
            ln->chars[dwin->inorgx + ix] = ln->chars[dwin->inorgx + ix + 1];
        }
        ln->chars[dwin->inorgx + dwin->inlen - 1] = ' ';
        dwin->inlen--;
        break;

    case keycode_Escape:
        if (dwin->inlen <= 0) {
            return;
        }
        for (ix = 0; ix < dwin->inlen; ix++) {
            ln->chars[dwin->inorgx + ix] = ' ';
        }
        dwin->inlen = 0;
        dwin->incurs = 0;
        break;

    // Cursor movement keys, during line input.

    case keycode_Left:
        if (dwin->incurs <= 0) {
            return;
        }
        dwin->incurs--;
        break;

    case keycode_Right:
        if (dwin->incurs >= dwin->inlen) {
            return;
        }
        dwin->incurs++;
        break;

    case keycode_Home:
        if (dwin->incurs <= 0) {
            return;
        }
        dwin->incurs = 0;
        break;

    case keycode_End:
        if (dwin->incurs >= dwin->inlen) {
            return;
        }
        dwin->incurs = dwin->inlen;
        break;

    case keycode_Return:
        acceptline(win, arg);
        return;

    default:
        if (dwin->inlen >= dwin->inmax) {
            return;
        }

        if (arg < 32 || arg > 0xff) {
            return;
        }

        if (gli_conf_caps && (arg > 0x60 && arg < 0x7b)) {
            arg -= 0x20;
        }

        for (ix = dwin->inlen; ix > dwin->incurs; ix--) {
            ln->chars[dwin->inorgx + ix] = ln->chars[dwin->inorgx + ix - 1];
        }
        ln->attrs[dwin->inorgx + dwin->inlen].set(style_Input);
        ln->chars[dwin->inorgx + dwin->incurs] = arg;

        dwin->incurs++;
        dwin->inlen++;
    }

    dwin->curx = dwin->inorgx + dwin->incurs;
    dwin->cury = dwin->inorgy;

    touch(dwin, dwin->inorgy);
}

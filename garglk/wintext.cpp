// Copyright (C) 2006-2009 by Tor Andersson, Tara McGrew.
// Copyright (C) 2010 by Ben Cressey, Chris Spiegel.
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
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "glk.h"
#include "garglk.h"

// how many pixels we add to left/right margins
#define SLOP (2 * GLI_SUBPIX)

static void
put_text(window_textbuffer_t *dwin, const char *buf, int len, int pos, int oldlen);
static void
put_text_uni(window_textbuffer_t *dwin, glui32 *buf, int len, int pos, int oldlen);
static bool
put_picture(window_textbuffer_t *dwin, const std::shared_ptr<picture_t> &pic, glui32 align, glui32 linkval);
static void
scrolloneline(window_textbuffer_t *dwin, bool forced, bool flow_break);

static void touch(window_textbuffer_t *dwin, int line)
{
    window_t *win = dwin->owner;
    int y = win->bbox.y0 + gli_tmarginy + (dwin->height - line - 1) * gli_leading;
    dwin->lines[line].dirty = true;
    gli_clear_selection();
    winrepaint(win->bbox.x0, y - 2, win->bbox.x1, y + gli_leading + 2);

    // Rebuild the frame beneath a transparent overlay.
    if (win->is_transparent()) {
        gli_force_redraw = true;
    }
}

static void clear_unread_marker(window_textbuffer_t *dwin)
{
    // touch() assumes scrollpos == 0; all callers meet that condition.
    if (dwin->unread_marker_line.has_value() && *dwin->unread_marker_line < dwin->height) {
        touch(dwin, *dwin->unread_marker_line);
    }
    dwin->unread_marker_line.reset();
}

// Rectangles are not clipped to their window, so square mode needs a margin.
static int unread_marker_mode()
{
    if (gli_conf_unread_marker == 2 && gli_tmarginx <= 0) {
        return 0;
    }

    return gli_conf_unread_marker;
}

static void touchscroll(window_textbuffer_t *dwin)
{
    window_t *win = dwin->owner;
    int i;
    gli_clear_selection();
    winrepaint(win->bbox.x0, win->bbox.y0, win->bbox.x1, win->bbox.y1);
    for (i = 0; i < dwin->scrollmax; i++) {
        dwin->lines[i].dirty = true;
    }

    if (win->is_transparent()) {
        gli_force_redraw = true;
    }
}

std::vector<char> gli_get_text(window_textbuffer_t *dwin)
{
    int s = dwin->scrollmax < SCROLLBACK ? dwin->scrollmax : SCROLLBACK - 1;

    std::vector<char> text;
    for (int lineidx = s; lineidx >= 0; lineidx--) {
        auto line = dwin->lines[lineidx];
        for (int charidx = 0; charidx < line.len; charidx++) {
            std::array<char, 4> buf;
            auto n = gli_encode_utf8(line.chars[charidx], buf.data(), 4);
            for (int i = 0; i < n; i++) {
                text.push_back(buf[i]);
            }
        }
        // Only add newline if this was an actual paragraph break, not a wrapped line
        if (line.newline) {
            text.push_back(0x0a); // Unicode linefeed
        }
    }

    return text;
}

static void reflow(window_t *win)
{
    window_textbuffer_t *dwin = win->winbuffer();
    int inputbyte = -1;
    attr_t curattr;
    attr_t oldattr;
    int i, k, p, s;
    int x, f;

    if (dwin->height < 4 || dwin->width < 20) {
        return;
    }

    dwin->lines[0].len = dwin->numchars;

    std::vector<attr_t> attrbuf;
    std::vector<glui32> charbuf;
    std::vector<int> alignbuf;
    std::vector<std::shared_ptr<picture_t>> pictbuf;
    std::vector<glui32> hyperbuf;
    std::vector<int> offsetbuf;
    std::vector<int> flowbreakbuf;

    // allocate temp buffers
    try {
        attrbuf.resize(SCROLLBACK * TBLINELEN);
        charbuf.resize(SCROLLBACK * TBLINELEN);
        alignbuf.resize(SCROLLBACK);
        pictbuf.resize(SCROLLBACK);
        hyperbuf.resize(SCROLLBACK);
        offsetbuf.resize(SCROLLBACK);
        flowbreakbuf.resize(SCROLLBACK);
    } catch (const std::bad_alloc &) {
        return;
    }

    // copy text to temp buffers

    oldattr = win->attr;
    curattr.clear();

    x = 0;
    f = 0;
    p = 0;
    s = dwin->scrollmax < SCROLLBACK ? dwin->scrollmax : SCROLLBACK - 1;

    for (k = s; k >= 0; k--) {
        // Preserve pending line input across reflow.
        if (k == 0 && (win->line_request || win->line_request_uni)) {
            inputbyte = p + dwin->infence;
        }

        if (dwin->lines[k].lpic) {
            offsetbuf[x] = p;
            alignbuf[x] = imagealign_MarginLeft;
            pictbuf[x] = dwin->lines[k].lpic;
            hyperbuf[x] = dwin->lines[k].lhyper;
            x++;
        }

        if (dwin->lines[k].rpic) {
            offsetbuf[x] = p;
            alignbuf[x] = imagealign_MarginRight;
            pictbuf[x] = dwin->lines[k].rpic;
            hyperbuf[x] = dwin->lines[k].rhyper;
            x++;
        }

        auto flow_break_pos = dwin->lines[k].flow_break_pos;
        if (flow_break_pos.has_value()) {
            flowbreakbuf[f] = p + *flow_break_pos;
            f++;
        }

        for (i = 0; i < dwin->lines[k].len; i++) {
            attrbuf[p] = curattr = dwin->lines[k].attrs[i];
            charbuf[p] = dwin->lines[k].chars[i];
            p++;
        }

        if (dwin->lines[k].newline) {
            attrbuf[p] = curattr;
            charbuf[p] = '\n';
            p++;
        }
    }

    offsetbuf[x] = -1;
    flowbreakbuf[f] = -1;

    // clear window

    win_textbuffer_clear(win);

    // and dump text back

    x = 0;
    f = 0;
    for (i = 0; i < p; i++) {
        if (i == inputbyte) {
            break;
        }
        win->attr = attrbuf[i];

        if (offsetbuf[x] == i) {
            put_picture(dwin, pictbuf[x], alignbuf[x], hyperbuf[x]);
            x++;
        }

        if (flowbreakbuf[f] == i) {
            scrolloneline(dwin, false, true);
            f++;
        }

        win_textbuffer_putchar_uni(win, charbuf[i]);
    }

    // terribly sorry about this...
    dwin->lastseen = 0;
    dwin->scrollpos = 0;
    dwin->unread_marker_line.reset();

    if (inputbyte != -1) {
        dwin->infence = dwin->numchars;
        put_text_uni(dwin, charbuf.data() + inputbyte, p - inputbyte, dwin->numchars, 0);
        dwin->incurs = dwin->numchars;
    }

    win->attr = oldattr;

    touchscroll(dwin);
}

// Transparent overlays neither draw nor reserve space for a scrollbar.
static int scroll_width(const window_t *win)
{
    return win->is_transparent() ? 0 : gli_scroll_width;
}

void win_textbuffer_rearrange(window_t *win, rect_t *box)
{
    window_textbuffer_t *dwin = win->winbuffer();
    int newwid, newhgt, newpixwid;
    int rnd;

    dwin->owner->bbox = *box;

    newpixwid = box->x1 - box->x0 - gli_tmarginx * 2 - scroll_width(win);
    newwid = newpixwid / gli_cellw;
    newhgt = (box->y1 - box->y0 - gli_tmarginy * 2) / gli_cellh;

    // align text with bottom
    rnd = newhgt * gli_cellh + gli_tmarginy * 2;
    win->yadj = (box->y1 - box->y0 - rnd);
    dwin->owner->bbox.y0 += (box->y1 - box->y0 - rnd);

    // Text is wrapped against the exact pixel width (see
    // win_textbuffer_putchar_uni), so a resize of even a single pixel can
    // change where words break. Reflowing only when the character width
    // changes leaves the layout stale for up to a cell's worth of pixels,
    // and win_textbuffer_redraw then truncates the overhanging characters
    // instead of wrapping them.
    if (newwid != dwin->width || newpixwid != dwin->pixel_width) {
        dwin->width = newwid;
        dwin->pixel_width = newpixwid;
        reflow(win);
    }

    if (newhgt != dwin->height) {
        // scroll up if we obscure new lines
        if (dwin->lastseen >= newhgt - 1) {
            dwin->scrollpos += (dwin->height - newhgt);
        }

        dwin->height = newhgt;

        // keep window within 'valid' lines
        if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1) {
            dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
        }
        if (dwin->scrollpos < 0) {
            dwin->scrollpos = 0;
        }
        touchscroll(dwin);

        dwin->copybuf.clear();
    }
}

static int calcwidth(window_textbuffer_t *dwin,
    const glui32 *chars, const attr_t *attrs,
    int startchar, int numchars, int spw)
{
    int w = 0;
    int a, b;

    a = startchar;
    for (b = startchar; b < numchars; b++) {
        if (attrs[a] != attrs[b]) {
            w += gli_string_width_uni(attrs[a].font(dwin->styles),
                    chars + a, b - a, spw);
            a = b;
        }
    }

    w += gli_string_width_uni(attrs[a].font(dwin->styles),
            chars + a, b - a, spw);

    return w;
}

static int calcwidth(window_textbuffer_t *dwin,
    const std::array<glui32, TBLINELEN> &chars, const std::array<attr_t, TBLINELEN> &attrs,
    int startchar, int numchars, int spw)
{
    return calcwidth(dwin, chars.data(), attrs.data(), startchar, numchars, spw);
}

// Horizontal origin for a line's text, honoring Justification stylehints.
static int line_text_x0(window_textbuffer_t *dwin, const tbline_t &ln,
    int linelen, int x0, int x1, int spw)
{
    int text_x0 = x0 + SLOP + ln.lm;

    if (linelen <= 0) {
        return text_x0;
    }

    glui32 just = dwin->styles[ln.attrs[0].style].justification;
    if (just != stylehint_just_Centered && just != stylehint_just_RightFlush) {
        return text_x0;
    }

    int textw = calcwidth(dwin, ln.chars, ln.attrs, 0, linelen, spw);
    int avail = x1 - x0 - ln.lm - ln.rm - 2 * SLOP;
    if (textw >= avail) {
        return text_x0;
    }

    if (just == stylehint_just_Centered) {
        return text_x0 + (avail - textw) / 2;
    }

    return text_x0 + (avail - textw);
}

// Return the character's horizontal midpoint in subpixels, relative to first.
static int char_midpoint(window_textbuffer_t *dwin, const tbline_t &ln, int text_x0, int first, int index, int spw)
{
    int left = calcwidth(dwin, ln.chars, ln.attrs, first, index, spw);
    int right = calcwidth(dwin, ln.chars, ln.attrs, first, index + 1, spw);

    return text_x0 + (left + right) / 2;
}

static void scroll_input_into_view(window_textbuffer_t *dwin, int avail)
{
    if (dwin->inview > dwin->incurs) {
        dwin->inview = dwin->incurs;
    }

    while (dwin->inview < dwin->incurs && calcwidth(dwin, dwin->chars, dwin->attrs, dwin->inview, dwin->incurs, -1) >= avail) {
        dwin->inview++;
    }

    // Reveal text to the left when space permits.
    while (dwin->inview > 0 && calcwidth(dwin, dwin->chars, dwin->attrs, dwin->inview - 1, dwin->numchars, -1) < avail) {
        dwin->inview--;
    }
}

// Whether the next redraw will raise a [more] prompt. more_request is not set
// until drawing, which is too late for the overlay pass.
bool win_textbuffer_pages(window_t *win)
{
    if (win->type != wintype_TextBuffer) {
        return false;
    }

    const window_textbuffer_t *dwin = win->winbuffer();

    return dwin->scrollpos != 0 && dwin->height > 1;
}

void win_textbuffer_redraw(window_t *win)
{
    window_textbuffer_t *dwin = win->winbuffer();
    tbline_t ln;
    int linelen;
    int nsp, spw, pw;
    int x0, y0, x1, y1;
    int x, y, w;
    int a, b;
    glui32 link;
    int i;
    int hx0, hx1, hy0, hy1;
    bool selbuf, selrow, selchar;
    // NOTE: GCC complains these might be used uninitialized; they're
    // not, but do this to silence the warning.
    int sx0 = 0, sx1 = 0;
    bool selleft = false, selright = false;
    int tsc, lsc, rsc;

    dwin->lines[0].len = dwin->numchars;

    x0 = (win->bbox.x0 + gli_tmarginx) * GLI_SUBPIX;
    x1 = (win->bbox.x1 - gli_tmarginx - scroll_width(win)) * GLI_SUBPIX;
    y0 = win->bbox.y0 + gli_tmarginy;
    y1 = win->bbox.y1 - gli_tmarginy;

    pw = x1 - x0 - 2 * GLI_SUBPIX;

    // check if any part of buffer is selected
    selbuf = gli_check_selection(x0 / GLI_SUBPIX, y0, x1 / GLI_SUBPIX, y1);

    // Erase the old marker when the boundary moves.
    if (dwin->prev_unread_marker_line != dwin->unread_marker_line
            && dwin->prev_unread_marker_line.has_value()
            && *dwin->prev_unread_marker_line < dwin->scrollback) {
        dwin->lines[*dwin->prev_unread_marker_line].dirty = true;
        // Line repainting does not cover the margin square.
        if (unread_marker_mode() == 2 && !win->is_transparent()) {
            Color bg = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
            gli_draw_rect(win->bbox.x0, win->bbox.y0,
                    gli_tmarginx, win->bbox.y1 - win->bbox.y0, bg);
        }
    }

    // Remember selections confined to the current line input.
    std::optional<std::pair<int, int>> input_selection;
    bool selected_noninput = false;

    for (i = dwin->scrollpos + dwin->height - 1; i >= dwin->scrollpos; i--) {
        // top of line
        y = y0 + (dwin->height - (i - dwin->scrollpos) - 1) * gli_leading;

        // check if part of line is selected
        if (selbuf) {
            int ux, uy;
            selrow = gli_get_selection(x0 / GLI_SUBPIX, y,
                    x1 / GLI_SUBPIX, y + gli_leading,
                    &ux, &uy);
            sx0 = ux;
            sx1 = uy;
            selleft = (sx0 == x0 / GLI_SUBPIX);
            selright = (sx1 == x1 / GLI_SUBPIX);
        } else {
            selrow = false;
        }

        // mark selected line dirty
        if (selrow) {
            dwin->lines[i].dirty = true;
        }

        ln = dwin->lines[i];

        // skip if we can
        if (!ln.dirty && !ln.repaint && !gli_force_redraw && dwin->scrollpos == 0) {
            continue;
        }

        // repaint previously selected lines if needed
        if (ln.repaint && !gli_force_redraw) {
            gli_redraw_rect(x0 / GLI_SUBPIX, y, x1 / GLI_SUBPIX, y + gli_leading);
        }

        // keep selected line dirty and flag for repaint
        if (!selrow) {
            dwin->lines[i].dirty = false;
            dwin->lines[i].repaint = false;
        } else {
            dwin->lines[i].repaint = true;
        }

        // leave bottom line blank for [more] prompt
        if (i == dwin->scrollpos && i > 0) {
            continue;
        }

        linelen = ln.len;

        // kill spaces at the end unless they're a different color
        Color color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
        while (i > 0 && linelen > 1 && ln.chars[linelen - 1] == ' '
                && ln.attrs[linelen - 1].bgcolor == color
                && !ln.attrs[linelen - 1].reversed(dwin->styles)) {
            linelen--;
        }

        int first = 0;
        if (i == 0 && dwin->inbuf != nullptr) {
            scroll_input_into_view(dwin, pw - gli_caret_shape * 2 * GLI_SUBPIX);
            first = dwin->inview;
        }

        // kill characters that would overwrite the scroll bar
        while (linelen > first + 1 && calcwidth(dwin, ln.chars, ln.attrs, first, linelen, -1) >= pw) {
            linelen--;
        }

        // count spaces and width for full (left-right) justification
        glui32 line_just = linelen > 0
            ? dwin->styles[ln.attrs[0].style].justification
            : stylehint_just_LeftFlush;
        bool full_justify = (gli_conf_justify || line_just == stylehint_just_LeftRight)
            && line_just != stylehint_just_Centered
            && line_just != stylehint_just_RightFlush
            && !ln.newline && i > 0;
        if (full_justify) {
            for (a = 0, nsp = 0; a < linelen; a++) {
                if (ln.chars[a] == ' ') {
                    nsp++;
                }
            }
            w = calcwidth(dwin, ln.chars, ln.attrs, 0, linelen, 0);
            if (nsp != 0) {
                spw = (x1 - x0 - ln.lm - ln.rm - 2 * SLOP - w) / nsp;
            } else {
                spw = 0;
            }
        } else {
            spw = -1;
        }

        int text_x0 = line_text_x0(dwin, ln, linelen, x0, x1, spw);

        // find and highlight selected characters
        if (selrow && !gli_claimselect) {
            lsc = first;
            rsc = linelen - 1;
            if (!selleft) {
                while (lsc < linelen
                        && char_midpoint(dwin, ln, text_x0, first, lsc, spw) < sx0 * GLI_SUBPIX) {
                    lsc++;
                }
            }
            if (!selright) {
                rsc = lsc - 1;
                for (tsc = lsc; tsc < linelen; tsc++) {
                    if (char_midpoint(dwin, ln, text_x0, first, tsc, spw) >= sx1 * GLI_SUBPIX) {
                        break;
                    }
                    rsc = tsc;
                }
            }
            selchar = lsc <= rsc;
            // reverse colors for selected chars
            if (selchar) {
                for (tsc = lsc; tsc <= rsc; tsc++) {
                    ln.attrs[tsc].reverse = !ln.attrs[tsc].reverse;
                    dwin->copybuf.push_back(ln.chars[tsc]);
                }

                if (i == 0 && dwin->inbuf != nullptr && lsc >= dwin->infence) {
                    input_selection = {{lsc, rsc}};
                } else {
                    selected_noninput = true;
                }
            }
            // add newline only if this is a real paragraph break, not just a wrapped line
            if ((ln.len == 0 || (selchar && ln.len == rsc + 1)) && ln.newline) {
                dwin->copybuf.push_back('\n');
            }
        }

        // clear any stored hyperlink coordinates
        gli_put_hyperlink(0, x0 / GLI_SUBPIX, y,
                x1 / GLI_SUBPIX, y + gli_leading);

        // Derive widths from pixel endpoints so adjacent runs tile exactly.
        // Transparent overlays fill only reverse-video runs below.
        if (!win->is_transparent()) {
            color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
            gli_draw_rect(x0 / GLI_SUBPIX, y,
                    (x1 - x0) / GLI_SUBPIX, gli_leading,
                    color);
        }

        x = text_x0;
        a = first;
        for (b = first; b < linelen; b++) {
            if (ln.attrs[a] != ln.attrs[b]) {
                link = ln.attrs[a].hyperlink();
                auto font = ln.attrs[a].font(dwin->styles);
                color = ln.attrs[a].bg(dwin->styles);
                w = gli_string_width_uni(font, &ln.chars[a], b - a, spw);
                int rx0 = x / GLI_SUBPIX;
                int rx1 = (x + w) / GLI_SUBPIX;
                if (!win->is_transparent() || ln.attrs[a].reversed(dwin->styles)) {
                    gli_draw_rect(rx0, y,
                            rx1 - rx0, gli_leading,
                            color);
                }
                if (link != 0) {
                    if (gli_underline_hyperlinks) {
                        gli_draw_rect(rx0 + 1, y + gli_baseline + 1,
                                rx1 - rx0 + 1, 1,
                                gli_link_color);
                    }
                    gli_put_hyperlink(link, rx0, y,
                            rx1,
                            y + gli_leading);
                }
                x += w;
                a = b;
            }
        }
        link = ln.attrs[a].hyperlink();
        auto font = ln.attrs[a].font(dwin->styles);
        color = ln.attrs[a].bg(dwin->styles);
        w = gli_string_width_uni(font, &ln.chars[a], b - a, spw);
        int rx0 = x / GLI_SUBPIX;
        int rx1 = (x + w) / GLI_SUBPIX;
        if (!win->is_transparent() || ln.attrs[a].reversed(dwin->styles)) {
            gli_draw_rect(rx0, y, rx1 - rx0,
                    gli_leading, color);
        }
        if (link != 0) {
            if (gli_underline_hyperlinks) {
                gli_draw_rect(rx0 + 1, y + gli_baseline + 1,
                        rx1 - rx0 + 1, 1,
                        gli_link_color);
            }
            gli_put_hyperlink(link, rx0, y,
                    rx1,
                    y + gli_leading);
        }
        x += w;

        if (!win->is_transparent()) {
            color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
            gli_draw_rect(x / GLI_SUBPIX, y,
                    x1 / GLI_SUBPIX - x / GLI_SUBPIX, gli_leading,
                    color);
        }

        //
        // draw caret
        //

        if (gli_focuswin == win && i == 0 && (win->line_request || win->line_request_uni)) {
            w = calcwidth(dwin, dwin->chars, dwin->attrs, first, dwin->incurs, spw);
            if (w < pw - gli_caret_shape * 2 * GLI_SUBPIX) {
                gli_draw_caret(text_x0 + w, y + gli_baseline);
            }
        }

        //
        // draw text
        //

        x = text_x0;
        a = first;
        for (b = first; b < linelen; b++) {
            if (ln.attrs[a] != ln.attrs[b]) {
                link = ln.attrs[a].hyperlink();
                font = ln.attrs[a].font(dwin->styles);
                color = link != 0 ? gli_link_color : ln.attrs[a].fg(dwin->styles);
                x = gli_draw_string_uni(x, y + gli_baseline,
                        font, color, &ln.chars[a], b - a, spw);
                a = b;
            }
        }
        link = ln.attrs[a].hyperlink();
        font = ln.attrs[a].font(dwin->styles);
        color = link != 0 ? gli_link_color : ln.attrs[a].fg(dwin->styles);
        gli_draw_string_uni(x, y + gli_baseline,
                font, color, &ln.chars[a], linelen - a, spw);
    }

    //
    // draw unread marker
    //

    if (unread_marker_mode() != 0
            && dwin->unread_marker_line.has_value()
            && *dwin->unread_marker_line >= dwin->scrollpos
            && *dwin->unread_marker_line < dwin->scrollpos + dwin->height) {
        int marker_y = y0 + (dwin->height - (*dwin->unread_marker_line - dwin->scrollpos) - 1) * gli_leading;
        // Use effective Normal colors, including style hints and overrides.
        attr_t normal;
        Color tfg = normal.fg(dwin->styles);
        Color tbg = normal.bg(dwin->styles);
        Color marker_color((tfg[0] * 3 + tbg[0]) / 4,
                           (tfg[1] * 3 + tbg[1]) / 4,
                           (tfg[2] * 3 + tbg[2]) / 4);
        if (unread_marker_mode() == 1) {
            gli_draw_rect(x0 / GLI_SUBPIX, marker_y,
                    x1 / GLI_SUBPIX - x0 / GLI_SUBPIX, 1,
                    marker_color);
        } else {
            // Bottom-align a square about half the cap height.
            int marker_size = static_cast<int>(gli_conf_propsize * 0.4);
            if (marker_size < 3) {
                marker_size = 3;
            }
            if (marker_size > gli_tmarginx) {
                marker_size = gli_tmarginx;
            }
            int marker_x = win->bbox.x0 + (gli_tmarginx - marker_size) / 2;
            gli_draw_rect(marker_x, marker_y + gli_baseline - marker_size,
                    marker_size, marker_size,
                    marker_color);
        }
    }

    dwin->prev_unread_marker_line = dwin->unread_marker_line;

    //
    // draw more prompt
    //

    if (win_textbuffer_pages(win)) {
        x = x0 + SLOP;
        y = y0 + (dwin->height - 1) * gli_leading;

        gli_put_hyperlink(0, x0 / GLI_SUBPIX, y,
                x1/GLI_SUBPIX, y + gli_leading);

        // A transparent window shows the [more] prompt over whatever is
        // beneath it, with no backing fill of its own.
        if (!win->is_transparent()) {
            Color color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
            gli_draw_rect(x / GLI_SUBPIX, y,
                    x1 / GLI_SUBPIX - x / GLI_SUBPIX, gli_leading,
                    color);
        }

        w = gli_string_width_uni(gli_more_font,
                gli_more_prompt.data(), gli_more_prompt_len, -1);

        if (gli_more_align == 1) { // center
            x = x0 + SLOP + (x1 - x0 - w - SLOP * 2) / 2;
        } else if (gli_more_align == 2) { // right
            x = x1 - SLOP - w;
        }

        Color color = gli_override_fg.has_value() ? gli_more_color : win->fgcolor;
        gli_draw_string_uni(x, y + gli_baseline,
                gli_more_font, color,
                gli_more_prompt.data(), gli_more_prompt_len, -1);
        y1 = y; // don't want pictures overdrawing "[more]"

        // try to claim the focus
        dwin->owner->more_request = true;
        gli_more_focus = true;
    } else {
        dwin->owner->more_request = false;
        y1 = y0 + dwin->height * gli_leading;
    }

    //
    // draw the images
    //

    for (i = 0; i < dwin->scrollback; i++) {
        ln = dwin->lines[i];

        y = y0 + (dwin->height - (i - dwin->scrollpos) - 1) * gli_leading;

        if (ln.lpic) {
            if (y < y1 && y + ln.lpic->h > y0) {
                gli_draw_picture(ln.lpic.get(),
                        x0 / GLI_SUBPIX, y,
                        x0 / GLI_SUBPIX, y0, x1 / GLI_SUBPIX, y1);
                link = ln.lhyper;
                hy0 = y > y0 ? y : y0;
                hy1 = y + ln.lpic->h < y1 ? y + ln.lpic->h : y1;
                hx0 = x0 / GLI_SUBPIX;
                hx1 = x0 / GLI_SUBPIX + ln.lpic->w < x1 / GLI_SUBPIX
                            ? x0 / GLI_SUBPIX + ln.lpic->w
                            : x1 / GLI_SUBPIX;
                gli_put_hyperlink(link, hx0, hy0, hx1, hy1);
            }
        }

        if (ln.rpic) {
            if (y < y1 && y + ln.rpic->h > y0) {
                gli_draw_picture(ln.rpic.get(),
                        x1 / GLI_SUBPIX - ln.rpic->w, y,
                        x0 / GLI_SUBPIX, y0, x1 / GLI_SUBPIX, y1);
                link = ln.rhyper;
                hy0 = y > y0 ? y : y0;
                hy1 = y + ln.rpic->h < y1 ? y + ln.rpic->h : y1;
                hx0 = x1 / GLI_SUBPIX - ln.rpic->w > x0 / GLI_SUBPIX
                            ? x1 / GLI_SUBPIX - ln.rpic->w
                            : x0 / GLI_SUBPIX;
                hx1 = x1 / GLI_SUBPIX;
                gli_put_hyperlink(link, hx0, hy0, hx1, hy1);
            }
        }
    }

    //
    // Draw the scrollbar
    //

    // try to claim scroll keys
    dwin->owner->scroll_request = dwin->scrollmax > dwin->height;

    if (dwin->owner->scroll_request && scroll_width(win) != 0) {
        int t0, t1;
        x0 = win->bbox.x1 - gli_scroll_width;
        x1 = win->bbox.x1;
        y0 = win->bbox.y0 + gli_tmarginy;
        y1 = win->bbox.y1 - gli_tmarginy;

        gli_put_hyperlink(0, x0, y0, x1, y1);

        y0 += gli_scroll_width / 2;
        y1 -= gli_scroll_width / 2;

        // pos = thbot, pos - ht = thtop, max = wtop, 0 = wbot
        t0 = (dwin->scrollmax - dwin->scrollpos) - (dwin->height - 1);
        t1 = (dwin->scrollmax - dwin->scrollpos);
        if (dwin->scrollmax > dwin->height) {
            t0 = t0 * (y1 - y0) / dwin->scrollmax + y0;
            t1 = t1 * (y1 - y0) / dwin->scrollmax + y0;
        } else {
            t0 = t1 = y0;
        }

        gli_draw_rect(x0 + 1, y0, x1 - x0 - 2, y1 - y0, gli_scroll_bg);
        gli_draw_rect(x0 + 1, t0, x1 - x0 - 2, t1 - t0, gli_scroll_fg);

        for (i = 0; i < gli_scroll_width / 2 + 1; i++) {
            gli_draw_rect(x0 + gli_scroll_width / 2-i,
                    y0 - gli_scroll_width / 2 + i,
                    i * 2, 1, gli_scroll_fg);
            gli_draw_rect(x0 + gli_scroll_width / 2 - i,
                    y1 + gli_scroll_width / 2 - i,
                    i * 2, 1, gli_scroll_fg);
        }
    }

    dwin->input_selection = selected_noninput
        ? std::nullopt
        : input_selection;

    // send selected text to clipboard
    if (selbuf && !dwin->copybuf.empty()) {
        gli_claimselect = true;
        gli_clipboard_copy(dwin->copybuf);
        dwin->copybuf.clear();
    }

    // no more prompt means all text has been seen
    if (!dwin->owner->more_request) {
        dwin->lastseen = 0;
    }
}

static void scrollresize(window_textbuffer_t *dwin)
{
    int i;

    dwin->lines.resize(dwin->scrollback + SCROLLBACK);

    dwin->chars = dwin->lines[0].chars.data();
    dwin->attrs = dwin->lines[0].attrs.data();

    for (i = dwin->scrollback; i < (dwin->scrollback + SCROLLBACK); i++) {
        dwin->lines[i].dirty = false;
        dwin->lines[i].repaint = false;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].lpic.reset();
        dwin->lines[i].rpic.reset();
        dwin->lines[i].lhyper = 0;
        dwin->lines[i].rhyper = 0;
        dwin->lines[i].len = 0;
        dwin->lines[i].flow_break_pos.reset();
        dwin->lines[i].newline = false;
        dwin->lines[i].chars.fill(' ');
        dwin->lines[i].attrs.fill(attr_t{});
    }

    dwin->scrollback += SCROLLBACK;
}

static void scrolloneline(window_textbuffer_t *dwin, bool forced, bool flow_break = false)
{
    int i;
    int lines_to_scroll = 1;

    if (flow_break) {
        if (dwin->ladjn == 0 && dwin->radjn == 0) {
            auto &line = dwin->lines[0];
            line.flow_break_pos = line.len;
            return;
        }

        lines_to_scroll = std::max(dwin->ladjn, dwin->radjn);
    }

    dwin->lastseen += lines_to_scroll;
    dwin->scrollmax += lines_to_scroll;
    if (dwin->unread_marker_line.has_value()) {
        *dwin->unread_marker_line += lines_to_scroll;
    }

    if (dwin->scrollmax > dwin->scrollback - lines_to_scroll
            || dwin->lastseen > dwin->scrollback - lines_to_scroll) {
        scrollresize(dwin);
    }

    if (dwin->lastseen >= dwin->height) {
        dwin->scrollpos += lines_to_scroll;
    }

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + lines_to_scroll) {
        dwin->scrollpos = dwin->scrollmax - dwin->height + lines_to_scroll;
    }
    if (dwin->scrollpos < 0) {
        dwin->scrollpos = 0;
    }

    if (forced || flow_break) {
        dwin->dashed = 0;
    }
    dwin->spaced = 0;

    auto &line_0 = dwin->lines[0];
    line_0.len = dwin->numchars;
    line_0.newline = forced;
    if (flow_break) {
        line_0.flow_break_pos = line_0.len;
    }

    for (i = dwin->scrollback - 1; i > lines_to_scroll - 1; i--) {
        dwin->lines[i] = dwin->lines[i - lines_to_scroll];
        if (i < dwin->height) {
            touch(dwin, i);
        }
    }

    if (flow_break) {
        dwin->radjn = 0;
        dwin->ladjn = 0;
    }

    if (dwin->radjn != 0) {
        dwin->radjn--;
    }
    if (dwin->radjn == 0) {
        dwin->radjw = 0;
    }
    if (dwin->ladjn != 0) {
        dwin->ladjn--;
    }
    if (dwin->ladjn == 0) {
        dwin->ladjw = 0;
    }

    for (i = 0; i < lines_to_scroll; i++) {
        touch(dwin, i);
        dwin->lines[i].len = 0;
        dwin->lines[i].flow_break_pos.reset();
        dwin->lines[i].newline = false;
        dwin->lines[i].lm = dwin->ladjw;
        dwin->lines[i].rm = dwin->radjw;
        dwin->lines[i].lpic.reset();
        dwin->lines[i].rpic.reset();
        dwin->lines[i].lhyper = 0;
        dwin->lines[i].rhyper = 0;
        dwin->lines[i].chars.fill(' ');
        dwin->lines[i].attrs.fill(attr_t{});
    }

    dwin->numchars = 0;

    touchscroll(dwin);
}

// only for input text
static void put_text(window_textbuffer_t *dwin, const char *buf, int len, int pos, int oldlen)
{
    int diff = len - oldlen;

    if (dwin->numchars + diff >= TBLINELEN) {
        return;
    }

    if (diff != 0 && pos + oldlen < dwin->numchars) {
        std::memmove(dwin->chars + pos + len,
                dwin->chars + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * 4);
        std::memmove(dwin->attrs + pos + len,
                dwin->attrs + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * sizeof(attr_t));
    }
    if (len > 0) {
        int i;
        for (i = 0; i < len; i++) {
            dwin->chars[pos + i] = static_cast<unsigned char>(buf[i]);
            dwin->attrs[pos + i].set(style_Input);
        }
    }
    dwin->numchars += diff;

    if (dwin->inbuf != nullptr) {
        if (dwin->incurs >= pos + oldlen) {
            dwin->incurs += diff;
        } else if (dwin->incurs >= pos) {
            dwin->incurs = pos + len;
        }
    }

    touch(dwin, 0);
}

static void put_text_uni(window_textbuffer_t *dwin, glui32 *buf, int len, int pos, int oldlen)
{
    int diff = len - oldlen;

    if (dwin->numchars + diff >= TBLINELEN) {
        return;
    }

    if (diff != 0 && pos + oldlen < dwin->numchars) {
        std::memmove(dwin->chars + pos + len,
                dwin->chars + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * 4);
        std::memmove(dwin->attrs + pos + len,
                dwin->attrs + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * sizeof(attr_t));
    }
    if (len > 0) {
        int i;
        std::memmove(dwin->chars + pos, buf, len * 4);
        for (i = 0; i < len; i++) {
            dwin->attrs[pos + i].set(style_Input);
        }
    }
    dwin->numchars += diff;

    if (dwin->inbuf != nullptr) {
        if (dwin->incurs >= pos + oldlen) {
            dwin->incurs += diff;
        } else if (dwin->incurs >= pos) {
            dwin->incurs = pos + len;
        }
    }

    touch(dwin, 0);
}

// Return true if a following quotation mark should be an opening mark,
// false if it should be a closing mark. Opening quotation marks will
// appear following an open parenthesis, open square bracket, or
// whitespace.
static bool leftquote(std::uint32_t c)
{
    switch(c) {
    case '(': case '[':

    // The following are Unicode characters in the "Separator, Space" category.
    case 0x0020: case 0x00a0: case 0x1680: case 0x2000:
    case 0x2001: case 0x2002: case 0x2003: case 0x2004:
    case 0x2005: case 0x2006: case 0x2007: case 0x2008:
    case 0x2009: case 0x200a: case 0x202f: case 0x205f:
    case 0x3000:
        return true;
    default:
        return false;
    }
}

void win_textbuffer_putchar_uni(window_t *win, glui32 ch)
{
    window_textbuffer_t *dwin = win->winbuffer();
    std::array<glui32, TBLINELEN> bchars;
    std::array<attr_t, TBLINELEN> battrs;
    int pw;
    int bpoint;
    int saved;
    int i;
    int linelen;

    // Don't speak if the current text style is input, under the
    // assumption that the interpreter is trying to display the user's
    // input. This is how Bocfel uses style_Input, and without this
    // test, extraneous input text is spoken. Other formats/interpreters
    // don't have this issue, but since this affects all Z-machine
    // games, it's probably worth the hacky solution here. If there are
    // Glulx games which use input style for text that the user did not
    // enter, that text will not get spoken. If that turns out to be a
    // problem, a new Gargoyle-specific function will probably be needed
    // that Bocfel can use to signal that it's writing input text from
    // the user vs input text from elsewhere.
    //
    // Note that this already affects history playback in Bocfel: since
    // it styles previous user input with style_Input during history
    // playback, the user input won't be spoken. That's annoying but
    // probably not quite as important as getting the expected behavior
    // during normal gameplay.
    //
    // See https://github.com/garglk/garglk/issues/356
    if (win->attr.style != style_Input) {
        gli_tts_speak(&ch, 1);
    }

    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2 - scroll_width(win)) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw - dwin->ladjw;

    Color color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;

    // oops ... overflow
    if (dwin->numchars + 1 >= TBLINELEN) {
        scrolloneline(dwin, false);
    }

    if (ch == '\n') {
        scrolloneline(dwin, true);
        return;
    }

    if (gli_conf_quotes != 0) {
        // fails for 'tis a wonderful day in the '80s
        if (gli_conf_quotes == 2 && ch == '\'') {
            if (dwin->numchars == 0 || leftquote(dwin->chars[dwin->numchars - 1])) {
                ch = UNI_LSQUO;
            }
        }

        if (ch == '`') {
            ch = UNI_LSQUO;
        }

        if (ch == '\'') {
            ch = UNI_RSQUO;
        }

        if (ch == '"') {
            if (dwin->numchars == 0 || leftquote(dwin->chars[dwin->numchars - 1])) {
                ch = UNI_LDQUO;
            } else {
                ch = UNI_RDQUO;
            }
        }
    }

    // This tracks whether the font "should" be monospace, not whether
    // the font file itself is actually monospace: if the font is monor,
    // monob, monoi, or monoz, then this will be true, regardless of
    // what font the user actually set as the monospace font.
    bool monospace = gli_tstyles[win->attr.style].font.monospace;

    if (gli_conf_dashes != 0 && !monospace) {
        if (ch == '-') {
            dwin->dashed++;
            if (dwin->dashed == 2) {
                dwin->numchars--;
                if (gli_conf_dashes == 2) {
                    ch = UNI_NDASH;
                } else {
                    ch = UNI_MDASH;
                }
            }
            if (dwin->dashed == 3) {
                dwin->numchars--;
                ch = UNI_MDASH;
                dwin->dashed = 0;
            }
        } else {
            dwin->dashed = 0;
        }
    }

    if (gli_conf_spaces != 0 && !monospace
            && dwin->styles[win->attr.style].bg == color
            && !dwin->styles[win->attr.style].reverse) {
        // turn (period space space) into (period space)
        if (gli_conf_spaces == 1) {
            if (ch == '.') {
                dwin->spaced = 1;
            } else if (ch == ' ' && dwin->spaced == 1) {
                dwin->spaced = 2;
            } else if (ch == ' ' && dwin->spaced == 2) {
                dwin->spaced = 0;
                return;
            } else {
                dwin->spaced = 0;
            }
        }

        // turn (per sp x) into (per sp sp x)
        else if (gli_conf_spaces == 2) {
            if (ch == '.') {
                dwin->spaced = 1;
            } else if (ch == ' ' && dwin->spaced == 1) {
                dwin->spaced = 2;
            } else if (ch != ' ' && dwin->spaced == 2) {
                dwin->spaced = 0;
                win_textbuffer_putchar_uni(win, ' ');
            } else {
                dwin->spaced = 0;
            }
        }
    }

    dwin->chars[dwin->numchars] = ch;
    dwin->attrs[dwin->numchars] = win->attr;
    dwin->numchars++;

    // kill spaces at the end for line width calculation
    linelen = dwin->numchars;
    while (linelen > 1 && dwin->chars[linelen - 1] == ' '
            && dwin->attrs[linelen - 1].bgcolor == color
            && !dwin->attrs[linelen - 1].reversed(dwin->styles)) {
        linelen--;
    }

    if (calcwidth(dwin, dwin->chars, dwin->attrs, 0, linelen, -1) >= pw) {
        bpoint = dwin->numchars;

        for (i = dwin->numchars - 1; i > 0; i--) {
            if (dwin->chars[i] == ' ') {
                bpoint = i + 1; // skip space
                break;
            }
        }

        saved = dwin->numchars - bpoint;

        std::memcpy(bchars.data(), dwin->chars + bpoint, saved * 4);
        std::memcpy(battrs.data(), dwin->attrs + bpoint, saved * sizeof(attr_t));
        dwin->numchars = bpoint;

        scrolloneline(dwin, false);

        std::memcpy(dwin->chars, bchars.data(), saved * 4);
        std::memcpy(dwin->attrs, battrs.data(), saved * sizeof(attr_t));
        dwin->numchars = saved;
    }

    touch(dwin, 0);
}

bool win_textbuffer_unputchar_uni(window_t *win, glui32 ch)
{
    window_textbuffer_t *dwin = win->winbuffer();
    if (dwin->numchars > 0 && glk_char_to_upper(dwin->chars[dwin->numchars - 1]) == glk_char_to_upper(ch)) {
        dwin->numchars--;
        touch(dwin, 0);
        return true;
    }
    return false;
}

void win_textbuffer_clear(window_t *win)
{
    window_textbuffer_t *dwin = win->winbuffer();
    int i;

    win->attr.fgcolor = gli_override_fg;
    win->attr.bgcolor = gli_override_bg;
    win->attr.reverse = false;

    dwin->ladjw = dwin->radjw = 0;
    dwin->ladjn = dwin->radjn = 0;

    dwin->spaced = 0;
    dwin->dashed = 0;

    dwin->numchars = 0;
    dwin->inview = 0;

    for (i = 0; i < dwin->scrollback; i++) {
        dwin->lines[i].len = 0;
        dwin->lines[i].flow_break_pos.reset();

        dwin->lines[i].lpic.reset();
        dwin->lines[i].rpic.reset();

        dwin->lines[i].lhyper = 0;
        dwin->lines[i].rhyper = 0;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].newline = false;
        dwin->lines[i].dirty = true;
        dwin->lines[i].repaint = false;
    }

    dwin->lastseen = 0;
    dwin->scrollpos = 0;
    dwin->scrollmax = 0;
    dwin->unread_marker_line.reset();

    for (i = 0; i < dwin->height; i++) {
        touch(dwin, i);
    }
}

// Prepare the window for line input.
static void win_textbuffer_init_impl(window_t *win, void *buf, int maxlen, int initlen, bool unicode)
{
    window_textbuffer_t *dwin = win->winbuffer();
    int pw;

    // because '>' prompt is ugly without extra space
    if (dwin->numchars != 0 && dwin->chars[dwin->numchars - 1] == '>') {
        win_textbuffer_putchar_uni(win, ' ');
    }
     if (dwin->numchars != 0 && dwin->chars[dwin->numchars - 1] == '?') {
        win_textbuffer_putchar_uni(win, ' ');
    }

    // make sure we have some space left for typing...
    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw + dwin->ladjw;
    if (calcwidth(dwin, dwin->chars, dwin->attrs, 0, dwin->numchars, -1) >= pw * 3 / 4) {
        win_textbuffer_putchar_uni(win, '\n');
    }

    dwin->inbuf = buf;
    dwin->inunicode = unicode;
    dwin->inmax = maxlen;
    dwin->infence = dwin->numchars;
    dwin->incurs = dwin->numchars;
    dwin->inview = 0;
    dwin->origattr = win->attr;
    win->attr.set(style_Input);

    if (initlen != 0) {
        touch(dwin, 0);
        if (unicode) {
            put_text_uni(dwin, static_cast<glui32 *>(buf), initlen, dwin->incurs, 0);
        } else {
            put_text(dwin, static_cast<char *>(buf), initlen, dwin->incurs, 0);
        }
    }

    if (gli_register_arr != nullptr) {
        dwin->inarrayrock = (*gli_register_arr)(dwin->inbuf, maxlen, const_cast<char *>(unicode ? "&+#!Iu" : "&+#!Cn"));
    }
}

void win_textbuffer_init_line(window_t *win, char *buf, int maxlen, int initlen)
{
    win_textbuffer_init_impl(win, buf, maxlen, initlen, false);
}

void win_textbuffer_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen)
{
    win_textbuffer_init_impl(win, buf, maxlen, initlen, true);
}

// Abort line input, storing whatever's been typed so far.
void win_textbuffer_cancel_line(window_t *win, event_t *ev)
{
    window_textbuffer_t *dwin = win->winbuffer();
    gidispatch_rock_t inarrayrock;
    int ix;
    int len;
    void *inbuf;
    int inmax;
    bool inunicode;

    if (dwin->inbuf == nullptr) {
        return;
    }

    inbuf = dwin->inbuf;
    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;
    inunicode = dwin->inunicode;

    len = dwin->numchars - dwin->infence;
    if (win->echostr != nullptr) {
        gli_stream_echo_line_uni(win->echostr, dwin->chars + dwin->infence, len);
    }

    if (len > inmax) {
        len = inmax;
    }

    if (!inunicode) {
        for (ix = 0; ix < len; ix++) {
            glui32 ch = dwin->chars[dwin->infence + ix];
            if (ch > 0xff) {
                ch = '?';
            }
            (static_cast<char *>(inbuf))[ix] = static_cast<char>(ch);
        }
    } else {
        for (ix = 0; ix < len; ix++) {
            (static_cast<glui32 *>(inbuf))[ix] = dwin->chars[dwin->infence + ix];
        }
    }

    win->attr = dwin->origattr;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = len;
    ev->val2 = 0;

    win->line_request = false;
    win->line_request_uni = false;
    dwin->inbuf = nullptr;
    dwin->inmax = 0;

    if (win->echo_line_input) {
        win_textbuffer_putchar_uni(win, '\n');
    } else {
        dwin->numchars = dwin->infence;
        touch(dwin, 0);
    }

    clear_unread_marker(dwin);

    if (gli_unregister_arr != nullptr) {
        const char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inmax, const_cast<char *>(typedesc), inarrayrock);
    }
}

// Keybinding functions.

// Any key, when text buffer is scrolled.
bool gcmd_accept_scroll(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->winbuffer();
    int pageht = dwin->height - 2; // 1 for prompt, 1 for overlap
    int old_scrollpos = dwin->scrollpos;
    bool startpos = dwin->scrollpos != 0;

    switch (arg) {
    case keycode_PageUp:
        dwin->scrollpos += pageht;
        break;
    case keycode_End:
        dwin->scrollpos = 0;
        break;
    case keycode_Up:
        dwin->scrollpos++;
        break;
    case keycode_Down:
    case keycode_Return:
        dwin->scrollpos--;
        break;
    case keycode_MouseWheelUp:
        dwin->scrollpos += 3;
        startpos = true;
        break;
    case keycode_MouseWheelDown:
        dwin->scrollpos -= 3;
        startpos = true;
        break;
    case ' ':
    case keycode_PageDown:
        if (pageht != 0) {
            dwin->scrollpos -= pageht;
        } else {
            dwin->scrollpos = 0;
        }
        break;
    }

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1) {
        dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
    }
    if (dwin->scrollpos < 0) {
        dwin->scrollpos = 0;
    }
    if (dwin->scrollpos > old_scrollpos) {
        // Scrolling up retires the marker.
        dwin->unread_marker_line.reset();
    }
    bool is_more_ack = arg == ' ' || arg == keycode_PageDown || arg == keycode_End;
    if (is_more_ack && old_scrollpos > 0 && dwin->scrollpos == 0) {
        // The prompt occupies the bottom row, making old_scrollpos the
        // first newly visible buffer line.
        dwin->unread_marker_line = old_scrollpos;
    }
    touchscroll(dwin);

    return startpos || dwin->scrollpos != 0;
}

// Any key, during character input. Ends character input.
void gcmd_buffer_accept_readchar(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->winbuffer();
    glui32 key;

    if (dwin->height < 2) {
        dwin->scrollpos = 0;
    }

    if (dwin->scrollpos != 0
            || arg == keycode_PageUp
            || arg == keycode_MouseWheelUp) {
        gcmd_accept_scroll(win, arg);
        return;
    }

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

    gli_tts_purge();

    if (key > 0xff && key < (0xffffffff - keycode_MAXVAL + 1)) {
        if (!(win->char_request_uni) || key > 0x10ffff) {
            key = keycode_Unknown;
        }
    }

    win->char_request = false;
    win->char_request_uni = false;
    clear_unread_marker(dwin);
    gli_event_store(evtype_CharInput, win, key, 0);
}

// Return or enter, during line input. Ends line input.
static void acceptline(window_t *win, glui32 keycode)
{
    int ix;
    int len;
    void *inbuf;
    int inmax;
    bool inunicode;
    gidispatch_rock_t inarrayrock;
    window_textbuffer_t *dwin = win->winbuffer();

    if (dwin->inbuf == nullptr) {
        return;
    }

    inbuf = dwin->inbuf;
    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;
    inunicode = dwin->inunicode;

    len = dwin->numchars - dwin->infence;
    if (win->echostr != nullptr) {
        gli_stream_echo_line_uni(win->echostr, dwin->chars + dwin->infence, len);
    }

    gli_tts_purge();
    if (gli_conf_speak_input) {
        gli_tts_speak(dwin->chars + dwin->infence, len);
        std::array<glui32, 1> newline = {'\n'};
        gli_tts_speak(newline.data(), 1);
    }

    // Store in history.
    // A history entry should not repeat the string from the entry before it.
    if (len != 0) {
        // If the iterator's not at the beginning, that means the user is in the
        // middle of a history cycle. If that's the case, the first history
        // entry is the currently-typed text, which is no longer relevant. Drop it.
        if (dwin->history_it != dwin->history.begin()) {
            dwin->history.pop_front();
        }

        std::vector<glui32> line(&dwin->chars[dwin->infence], &dwin->chars[dwin->numchars]);
        if (dwin->history.empty() || dwin->history.front() != line) {
            dwin->history.push_front(line);
        }

        while (dwin->history.size() > HISTORYLEN) {
            dwin->history.pop_back();
        }

        dwin->history_it = dwin->history.begin();
    }

    // Store in event buffer.

    if (len > inmax) {
        len = inmax;
    }

    if (!inunicode) {
        for (ix = 0; ix < len; ix++) {
            glui32 ch = dwin->chars[dwin->infence + ix];
            if (ch > 0xff) {
                ch = '?';
            }
            (static_cast<char *>(inbuf))[ix] = static_cast<char>(ch);
        }
    } else {
        for (ix = 0; ix < len; ix++) {
            (static_cast<glui32 *>(inbuf))[ix] = dwin->chars[dwin->infence + ix];
        }
    }

    win->attr = dwin->origattr;

    if (!win->line_terminators.empty()) {
        glui32 val2 = keycode;
        if (val2 == keycode_Return) {
            val2 = 0;
        }
        gli_event_store(evtype_LineInput, win, len, val2);
    } else {
        gli_event_store(evtype_LineInput, win, len, 0);
    }
    win->line_request = false;
    win->line_request_uni = false;
    dwin->inbuf = nullptr;
    dwin->inmax = 0;

    if (win->echo_line_input) {
        win_textbuffer_putchar_uni(win, '\n');
    } else {
        dwin->numchars = dwin->infence;
        touch(dwin, 0);
    }

    clear_unread_marker(dwin);

    if (gli_unregister_arr != nullptr) {
        const char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inmax, const_cast<char *>(typedesc), inarrayrock);
    }
}

// Start of the word to the left of the cursor.
static long word_left(const window_textbuffer_t *dwin)
{
    long pos = dwin->incurs;

    while (pos > dwin->infence && dwin->chars[pos - 1] == ' ') {
        pos--;
    }
    while (pos > dwin->infence && dwin->chars[pos - 1] != ' ') {
        pos--;
    }

    return pos;
}

// End of the word to the right of the cursor.
static long word_right(const window_textbuffer_t *dwin)
{
    long pos = dwin->incurs;

    while (pos < dwin->numchars && dwin->chars[pos] != ' ') {
        pos++;
    }
    while (pos < dwin->numchars && dwin->chars[pos] == ' ') {
        pos++;
    }

    return pos;
}

// Text most recently removed by a kill command, restored by keycode_Yank.
static std::vector<glui32> killbuf;

static void kill_text(window_textbuffer_t *dwin, long start, long end)
{
    if (start >= end) {
        return;
    }

    killbuf.assign(&dwin->chars[start], &dwin->chars[end]);
    put_text_uni(dwin, nullptr, 0, start, end - start);
}

// Delete a completed selection confined to the current line input. The
// deletion clears the selection by way of touch(), so the same range
// cannot be deleted twice.
static bool delete_input_selection(window_textbuffer_t *dwin)
{
    if (!gli_selection_active() || !dwin->input_selection.has_value()) {
        return false;
    }

    auto [start, end] = *dwin->input_selection;

    put_text_uni(dwin, nullptr, 0, start, end - start + 1);

    return true;
}

// Any key, during line input.
void gcmd_buffer_accept_readline(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->winbuffer();

    if (dwin->height < 2) {
        dwin->scrollpos = 0;
    }

    if (dwin->scrollpos != 0
            || arg == keycode_PageUp
            || arg == keycode_MouseWheelUp) {
        gcmd_accept_scroll(win, arg);
        return;
    }

    if (dwin->inbuf == nullptr) {
        return;
    }

    if (!win->line_terminators.empty() && gli_window_check_terminator(arg)) {
        if (std::find(win->line_terminators.begin(), win->line_terminators.end(), arg) != win->line_terminators.end()) {
            acceptline(win, arg);
            return;
        }
    }

    switch (arg) {

    // History keys (up and down)

    case keycode_Up:
        // There is no stored history, so do nothing.
        if (dwin->history.empty()) {
            return;
        }

        // There is stored history, and this is the start of a cycle through
        // it. Store the currently-typed text (which may be empty) at the
        // front of the history buffer, and point the iterator there.
        if (dwin->history_it == dwin->history.begin()) {
            dwin->history.emplace_front(&dwin->chars[dwin->infence], &dwin->chars[dwin->numchars]);
            dwin->history_it = dwin->history.begin();
        }

        // The iterator is on the current history entry, so load the
        // previous (older) one, if any (if not, that means this is the end
        // of history, so do nothing).
        if (dwin->history_it + 1 != dwin->history.end()) {
            ++dwin->history_it;
            put_text_uni(dwin, dwin->history_it->data(), dwin->history_it->size(), dwin->infence, dwin->numchars - dwin->infence);
        }
        break;

    case keycode_Down:
        // Already at the beginning (i.e. not actively cycling through
        // history), so do nothing.
        if (dwin->history_it == dwin->history.begin()) {
            return;
        }

        // Load the next (newer) history entry.
        --dwin->history_it;
        put_text_uni(dwin, dwin->history_it->data(), dwin->history_it->size(), dwin->infence, dwin->numchars - dwin->infence);

        // If we're at the beginning now, we're done cycling, and have
        // reloaded the user's currently-typed text. Since cycling is over,
        // drop the text from the history and repoint the iterator to the
        // previous history entry.
        if (dwin->history_it == dwin->history.begin()) {
            dwin->history.pop_front();
            dwin->history_it = dwin->history.begin();
        }
        break;

    // Cursor movement keys, during line input.

    case keycode_Left:
        if (dwin->incurs <= dwin->infence) {
            // Reveal a prompt hidden to the left of the input fence.
            if (dwin->inview == 0) {
                return;
            }
            dwin->inview--;
            break;
        }
        dwin->incurs--;
        break;

    case keycode_Right:
        if (dwin->incurs >= dwin->numchars) {
            return;
        }
        dwin->incurs++;
        break;

    case keycode_Home:
        if (dwin->incurs <= dwin->infence && dwin->inview == 0) {
            return;
        }
        dwin->incurs = dwin->infence;
        dwin->inview = 0;
        break;

    case keycode_End:
        if (dwin->incurs >= dwin->numchars) {
            return;
        }
        dwin->incurs = dwin->numchars;
        break;

    case keycode_SkipWordLeft:
        while (dwin->incurs > dwin->infence && dwin->chars[dwin->incurs - 1] == ' ') {
            dwin->incurs--;
        }
        while (dwin->incurs > dwin->infence && dwin->chars[dwin->incurs - 1] != ' ') {
            dwin->incurs--;
        }
        break;

    case keycode_SkipWordRight:
        while (dwin->incurs < dwin->numchars && dwin->chars[dwin->incurs] != ' ') {
            dwin->incurs++;
        }
        while (dwin->incurs < dwin->numchars && dwin->chars[dwin->incurs] == ' ') {
            dwin->incurs++;
        }
        break;

    // Delete keys, during line input.

    case keycode_DeleteWordLeft:
        kill_text(dwin, word_left(dwin), dwin->incurs);
        break;

    case keycode_DeleteWordRight:
        kill_text(dwin, dwin->incurs, word_right(dwin));
        break;

    case keycode_KillLine:
        kill_text(dwin, dwin->incurs, dwin->numchars);
        break;

    case keycode_Delete:
        if (delete_input_selection(dwin)) {
            break;
        }
        if (dwin->incurs <= dwin->infence) {
            return;
        }
        put_text_uni(dwin, nullptr, 0, dwin->incurs - 1, 1);
        break;

    case keycode_Erase:
        if (delete_input_selection(dwin)) {
            break;
        }
        if (dwin->incurs >= dwin->numchars) {
            return;
        }
        put_text_uni(dwin, nullptr, 0, dwin->incurs, 1);
        break;

    case keycode_Escape:
        if (dwin->infence >= dwin->numchars) {
            return;
        }
        kill_text(dwin, dwin->infence, dwin->numchars);
        break;

    case keycode_Yank: {
        if (killbuf.empty()) {
            return;
        }
        delete_input_selection(dwin);
        int avail = dwin->inmax - (dwin->numchars - dwin->infence);
        int len = std::min<int>(killbuf.size(), avail);
        if (len <= 0) {
            return;
        }
        put_text_uni(dwin, killbuf.data(), len, dwin->incurs, 0);
        break;
    }

    // Regular keys

    case keycode_Return:
        acceptline(win, arg);
        break;

    default:
        if (arg >= 32 && arg <= 0x10FFFF) {
            delete_input_selection(dwin);
            if (dwin->numchars - dwin->infence >= dwin->inmax) {
                return;
            }
            if (gli_conf_caps && (arg > 0x60 && arg < 0x7b)) {
                arg -= 0x20;
            }
            put_text_uni(dwin, &arg, 1, dwin->incurs, 0);
        }
        break;
    }

    touch(dwin, 0);
}

static bool put_picture(window_textbuffer_t *dwin, const std::shared_ptr<picture_t> &pic, glui32 align, glui32 linkval)
{
    if (align == imagealign_MarginRight) {
        if (dwin->lines[0].rpic || dwin->numchars != 0) {
            return false;
        }

        dwin->radjw = (pic->w + gli_tmarginx) * GLI_SUBPIX;
        dwin->radjn = (pic->h + gli_cellh - 1) / gli_cellh;
        auto &line = dwin->lines[0];
        line.rpic = pic;
        line.rm = dwin->radjw;
        line.rhyper = linkval;
        line.flow_break_pos.reset();
    }

    else {
        if (align != imagealign_MarginLeft && dwin->numchars != 0) {
            win_textbuffer_putchar_uni(dwin->owner, '\n');
        }

        if (dwin->lines[0].lpic || dwin->numchars != 0) {
            return false;
        }

        dwin->ladjw = (pic->w + gli_tmarginx) * GLI_SUBPIX;
        dwin->ladjn = (pic->h + gli_cellh - 1) / gli_cellh;
        auto &line = dwin->lines[0];
        line.lpic = pic;
        line.lm = dwin->ladjw;
        line.lhyper = linkval;
        line.flow_break_pos.reset();

        if (align != imagealign_MarginLeft) {
            win_textbuffer_flow_break(dwin);
        }
    }

    return true;
}

bool win_textbuffer_draw_picture(std::shared_ptr<picture_t> pic, window_textbuffer_t *dwin, glui32 window_width,
    glui32 align, glui32 width, glui32 height, glui32 maxwidth)
{
    glui32 hyperlink;

    if (maxwidth != 0) {
        auto limit = window_width * (maxwidth / 65536.0);
        if (width > limit) {
            double scaleby = static_cast<double>(limit) / width;
            width *= scaleby;
            height *= scaleby;
        }
    }

    pic = gli_picture_scale(pic.get(), gli_zoom_int(width), gli_zoom_int(height));

    hyperlink = dwin->owner->attr.hyperlink();

    return put_picture(dwin, pic, align, hyperlink);
}

void win_textbuffer_flow_break(window_textbuffer_t *dwin)
{
    scrolloneline(dwin, false, true);
}

// Move the input cursor to the caret position nearest the click.
static void position_input_cursor(window_textbuffer_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;

    if ((!win->line_request && !win->line_request_uni) || dwin->scrollpos != 0) {
        return;
    }

    int liney = win->bbox.y0 + gli_tmarginy + (dwin->height - 1) * gli_leading;
    if (sy < liney || sy >= liney + gli_leading) {
        return;
    }

    int x0 = (win->bbox.x0 + gli_tmarginx) * GLI_SUBPIX;
    int x1 = (win->bbox.x1 - gli_tmarginx - scroll_width(win)) * GLI_SUBPIX;
    const tbline_t &ln = dwin->lines[0];
    int linelen = dwin->numchars;

    // Input lines are never fully justified.
    int click_x = sx * GLI_SUBPIX - line_text_x0(dwin, ln, linelen, x0, x1, -1);

    // Ignore cursor positions scrolled off the left.
    int first = dwin->inview;
    int lo = std::max(static_cast<int>(dwin->infence), first);

    int curs = lo;
    int best = std::abs(click_x - calcwidth(dwin, ln.chars, ln.attrs, first, curs, -1));

    for (int i = lo + 1; i <= dwin->numchars; i++) {
        int dist = std::abs(click_x - calcwidth(dwin, ln.chars, ln.attrs, first, i, -1));
        if (dist >= best) {
            break;
        }
        best = dist;
        curs = i;
    }

    dwin->incurs = curs;
    touch(dwin, 0);
}

void win_textbuffer_click(window_textbuffer_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;
    bool gh = false;
    bool gs = false;

    if (win->line_request || win->char_request
        || win->line_request_uni || win->char_request_uni
        || win->more_request || win->scroll_request) {
        gli_focuswin = win;
    }

    if (win->hyper_request) {
        glui32 linkval = gli_get_hyperlink(sx, sy);
        if (linkval != 0) {
            gli_event_store(evtype_Hyperlink, win, linkval, 0);
            win->hyper_request = false;
            if (gli_conf_safeclicks) {
                gli_forceclick = true;
            }
            gh = true;
        }
    }

    const int scrollw = scroll_width(win);

    if (scrollw != 0 && sx > win->bbox.x1 - scrollw) {
        if (sy < win->bbox.y0 + gli_tmarginy + scrollw) {
            gcmd_accept_scroll(win, keycode_Up);
        } else if (sy > win->bbox.y1 - gli_tmarginy - scrollw) {
            gcmd_accept_scroll(win, keycode_Down);
        } else if (sy < (win->bbox.y0 + win->bbox.y1) / 2) {
            gcmd_accept_scroll(win, keycode_PageUp);
        } else {
            gcmd_accept_scroll(win, keycode_PageDown);
        }
        gs = true;
    }

    if (!gh && !gs) {
        position_input_cursor(dwin, sx, sy);

        gli_copyselect = true;
        gli_start_selection(sx, sy);
    }
}

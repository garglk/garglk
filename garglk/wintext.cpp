// Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.
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

static void touch(window_textbuffer_t *dwin, int line)
{
    window_t *win = dwin->owner;
    int y = win->bbox.y0 + gli_tmarginy + (dwin->height - line - 1) * gli_leading;
    dwin->lines[line].dirty = true;
    gli_clear_selection();
    winrepaint(win->bbox.x0, y - 2, win->bbox.x1, y + gli_leading + 2);
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
}

window_textbuffer_t *win_textbuffer_create(window_t *win)
{
    return new window_textbuffer_t(win, gli_tstyles, SCROLLBACK);
}

void win_textbuffer_destroy(window_textbuffer_t *dwin)
{
    if (dwin->inbuf != nullptr) {
        if (gli_unregister_arr != nullptr) {
            const char *typedesc = (dwin->inunicode ? "&+#!Iu" : "&+#!Cn");
            (*gli_unregister_arr)(dwin->inbuf, dwin->inmax, const_cast<char *>(typedesc), dwin->inarrayrock);
        }
        dwin->inbuf = nullptr;
    }

    dwin->owner = nullptr;

    delete dwin;
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
        text.push_back(0x0a); // Unicode linefeed
    }

    return text;
}

static void reflow(window_t *win)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int inputbyte = -1;
    attr_t curattr;
    attr_t oldattr;
    int i, k, p, s;
    int x;

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

    // allocate temp buffers
    try {
        attrbuf.resize(SCROLLBACK * TBLINELEN);
        charbuf.resize(SCROLLBACK * TBLINELEN);
        alignbuf.resize(SCROLLBACK);
        pictbuf.resize(SCROLLBACK);
        hyperbuf.resize(SCROLLBACK);
        offsetbuf.resize(SCROLLBACK);
    } catch (const std::bad_alloc &) {
        return;
    }

    // copy text to temp buffers

    oldattr = win->attr;
    curattr.clear();

    x = 0;
    p = 0;
    s = dwin->scrollmax < SCROLLBACK ? dwin->scrollmax : SCROLLBACK - 1;

    for (k = s; k >= 0; k--) {
        if (k == 0 && win->line_request) {
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

    // clear window

    win_textbuffer_clear(win);

    // and dump text back

    x = 0;
    for (i = 0; i < p; i++) {
        if (i == inputbyte) {
            break;
        }
        win->attr = attrbuf[i];

        if (offsetbuf[x] == i) {
            put_picture(dwin, pictbuf[x], alignbuf[x], hyperbuf[x]);
            x++;
        }

        win_textbuffer_putchar_uni(win, charbuf[i]);
    }

    // terribly sorry about this...
    dwin->lastseen = 0;
    dwin->scrollpos = 0;

    if (inputbyte != -1) {
        dwin->infence = dwin->numchars;
        put_text_uni(dwin, charbuf.data() + inputbyte, p - inputbyte, dwin->numchars, 0);
        dwin->incurs = dwin->numchars;
    }

    win->attr = oldattr;

    touchscroll(dwin);
}

void win_textbuffer_rearrange(window_t *win, rect_t *box)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int newwid, newhgt;
    int rnd;

    dwin->owner->bbox = *box;

    newwid = (box->x1 - box->x0 - gli_tmarginx * 2 - gli_scroll_width) / gli_cellw;
    newhgt = (box->y1 - box->y0 - gli_tmarginy * 2) / gli_cellh;

    // align text with bottom
    rnd = newhgt * gli_cellh + gli_tmarginy * 2;
    win->yadj = (box->y1 - box->y0 - rnd);
    dwin->owner->bbox.y0 += (box->y1 - box->y0 - rnd);

    if (newwid != dwin->width) {
        dwin->width = newwid;
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

        // allocate copy buffer
        dwin->copybuf.resize(dwin->height * TBLINELEN);
        std::fill(dwin->copybuf.begin(), dwin->copybuf.end(), 0);

        dwin->copypos = 0;
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

void win_textbuffer_redraw(window_t *win)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
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
    int tx, tsc, tsw, lsc, rsc;

    dwin->lines[0].len = dwin->numchars;

    x0 = (win->bbox.x0 + gli_tmarginx) * GLI_SUBPIX;
    x1 = (win->bbox.x1 - gli_tmarginx - gli_scroll_width) * GLI_SUBPIX;
    y0 = win->bbox.y0 + gli_tmarginy;
    y1 = win->bbox.y1 - gli_tmarginy;

    pw = x1 - x0 - 2 * GLI_SUBPIX;

    // check if any part of buffer is selected
    selbuf = gli_check_selection(x0 / GLI_SUBPIX, y0, x1 / GLI_SUBPIX, y1);

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
                && !ln.attrs[linelen - 1].reverse) {
            linelen--;
        }

        // kill characters that would overwrite the scroll bar
        while (linelen > 1 && calcwidth(dwin, ln.chars, ln.attrs, 0, linelen, -1) >= pw) {
            linelen--;
        }

        // count spaces and width for justification
        if (gli_conf_justify && !ln.newline && i > 0) {
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

        // find and highlight selected characters
        if (selrow && !gli_claimselect) {
            lsc = 0;
            rsc = 0;
            selchar = false;
            // optimized case for all chars selected
            if (selleft && selright) {
                rsc = linelen > 0 ? linelen - 1 : 0;
                selchar = ((calcwidth(dwin, ln.chars, ln.attrs, lsc, rsc, spw) / GLI_SUBPIX) != 0);
            } else {
                // optimized case for leftmost char selected
                if (selleft) {
                    tsc = linelen > 0 ? linelen - 1 : 0;
                    selchar = ((calcwidth(dwin, ln.chars, ln.attrs, lsc, tsc, spw) / GLI_SUBPIX) != 0);
                } else {
                    // find the substring contained by the selection
                    tx = (x0 + SLOP + ln.lm) / GLI_SUBPIX;
                    // measure string widths until we find left char
                    for (tsc = 0; tsc < linelen; tsc++) {
                        tsw = calcwidth(dwin, ln.chars, ln.attrs, 0, tsc, spw) / GLI_SUBPIX;
                        if (tsw + tx >= sx0 ||
                                (tsw + tx + GLI_SUBPIX >= sx0 && ln.chars[tsc] != ' ')) {
                            lsc = tsc;
                            selchar = true;
                            break;
                        }
                    }
                }
                if (selchar) {
                    // optimized case for rightmost char selected
                    if (selright) {
                        rsc = linelen > 0 ? linelen - 1 : 0;
                    } else {
                        // measure string widths until we find right char
                        for (tsc = lsc; tsc < linelen; tsc++) {
                            tsw = calcwidth(dwin, ln.chars, ln.attrs, lsc, tsc, spw) / GLI_SUBPIX;
                            if (tsw + sx0 < sx1) {
                                rsc = tsc;
                            }
                        }
                        if (lsc != 0 && rsc == 0) {
                            rsc = lsc;
                        }
                    }
                }
            }
            // reverse colors for selected chars
            if (selchar) {
                for (tsc = lsc; tsc <= rsc; tsc++) {
                    ln.attrs[tsc].reverse = !ln.attrs[tsc].reverse;
                    dwin->copybuf[dwin->copypos] = ln.chars[tsc];
                    dwin->copypos++;
                }
            }
            // add newline if we reach the end of the line
            if (ln.len == 0 || ln.len == (rsc + 1)) {
                dwin->copybuf[dwin->copypos] = '\n';
                dwin->copypos++;
            }
        }

        // clear any stored hyperlink coordinates
        gli_put_hyperlink(0, x0 / GLI_SUBPIX, y,
                x1 / GLI_SUBPIX, y + gli_leading);

        // fill in background colors
        color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
        gli_draw_rect(x0 / GLI_SUBPIX, y,
                (x1 - x0) / GLI_SUBPIX, gli_leading,
                color);

        x = x0 + SLOP + ln.lm;
        a = 0;
        for (b = 0; b < linelen; b++) {
            if (ln.attrs[a] != ln.attrs[b]) {
                link = ln.attrs[a].hyper;
                auto font = ln.attrs[a].font(dwin->styles);
                color = ln.attrs[a].bg(dwin->styles);
                w = gli_string_width_uni(font, &ln.chars[a], b - a, spw);
                gli_draw_rect(x / GLI_SUBPIX, y,
                        w / GLI_SUBPIX, gli_leading,
                        color);
                if (link != 0) {
                    if (gli_underline_hyperlinks) {
                        gli_draw_rect(x / GLI_SUBPIX + 1, y + gli_baseline + 1,
                                w / GLI_SUBPIX + 1, 1,
                                gli_link_color);
                    }
                    gli_put_hyperlink(link, x / GLI_SUBPIX, y,
                            x / GLI_SUBPIX + w / GLI_SUBPIX,
                            y + gli_leading);
                }
                x += w;
                a = b;
            }
        }
        link = ln.attrs[a].hyper;
        auto font = ln.attrs[a].font(dwin->styles);
        color = ln.attrs[a].bg(dwin->styles);
        w = gli_string_width_uni(font, &ln.chars[a], b - a, spw);
        gli_draw_rect(x / GLI_SUBPIX, y, w / GLI_SUBPIX,
                gli_leading, color);
        if (link != 0) {
            if (gli_underline_hyperlinks) {
                gli_draw_rect(x / GLI_SUBPIX + 1, y + gli_baseline + 1,
                        w / GLI_SUBPIX + 1, 1,
                        gli_link_color);
            }
            gli_put_hyperlink(link, x / GLI_SUBPIX, y,
                    x / GLI_SUBPIX + w / GLI_SUBPIX,
                    y + gli_leading);
        }
        x += w;

        color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
        gli_draw_rect(x / GLI_SUBPIX, y,
                x1 / GLI_SUBPIX - x / GLI_SUBPIX, gli_leading,
                color);

        //
        // draw caret
        //

        if (gli_focuswin == win && i == 0 && (win->line_request || win->line_request_uni)) {
            w = calcwidth(dwin, dwin->chars, dwin->attrs, 0, dwin->incurs, spw);
            if (w < pw - gli_caret_shape * 2 * GLI_SUBPIX) {
                gli_draw_caret(x0 + SLOP + ln.lm + w, y + gli_baseline);
            }
        }

        //
        // draw text
        //

        x = x0 + SLOP + ln.lm;
        a = 0;
        for (b = 0; b < linelen; b++) {
            if (ln.attrs[a] != ln.attrs[b]) {
                link = ln.attrs[a].hyper;
                font = ln.attrs[a].font(dwin->styles);
                color = link != 0 ? gli_link_color : ln.attrs[a].fg(dwin->styles);
                x = gli_draw_string_uni(x, y + gli_baseline,
                        font, color, &ln.chars[a], b - a, spw);
                a = b;
            }
        }
        link = ln.attrs[a].hyper;
        font = ln.attrs[a].font(dwin->styles);
        color = link != 0 ? gli_link_color : ln.attrs[a].fg(dwin->styles);
        gli_draw_string_uni(x, y + gli_baseline,
                font, color, &ln.chars[a], linelen - a, spw);
    }

    //
    // draw more prompt
    //

    if (dwin->scrollpos != 0 && dwin->height > 1) {
        x = x0 + SLOP;
        y = y0 + (dwin->height - 1) * gli_leading;

        gli_put_hyperlink(0, x0 / GLI_SUBPIX, y,
                x1/GLI_SUBPIX, y + gli_leading);

        Color color = gli_override_bg.has_value() ? gli_window_color : win->bgcolor;
        gli_draw_rect(x / GLI_SUBPIX, y,
                x1 / GLI_SUBPIX - x / GLI_SUBPIX, gli_leading,
                color);

        w = gli_string_width_uni(gli_more_font,
                gli_more_prompt.data(), gli_more_prompt_len, -1);

        if (gli_more_align == 1) { // center
            x = x0 + SLOP + (x1 - x0 - w - SLOP * 2) / 2;
        } else if (gli_more_align == 2) { // right
            x = x1 - SLOP - w;
        }

        color = gli_override_fg.has_value() ? gli_more_color : win->fgcolor;
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

    if (dwin->owner->scroll_request && gli_scroll_width != 0) {
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

    // send selected text to clipboard
    if (selbuf && dwin->copypos != 0) {
        gli_claimselect = true;
        gli_clipboard_copy(dwin->copybuf.data(), dwin->copypos);
        for (i = 0; i < dwin->copypos; i++) {
            dwin->copybuf[i] = 0;
        }
        dwin->copypos = 0;
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
        dwin->lines[i].newline = false;
        dwin->lines[i].chars.fill(' ');
        dwin->lines[i].attrs.fill(attr_t{});
    }

    dwin->scrollback += SCROLLBACK;
}

static void scrolloneline(window_textbuffer_t *dwin, bool forced)
{
    int i;

    dwin->lastseen++;
    dwin->scrollmax++;

    if (dwin->scrollmax > dwin->scrollback - 1
            || dwin->lastseen > dwin->scrollback - 1) {
        scrollresize(dwin);
    }

    if (dwin->lastseen >= dwin->height) {
        dwin->scrollpos++;
    }

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1) {
        dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
    }
    if (dwin->scrollpos < 0) {
        dwin->scrollpos = 0;
    }

    if (forced) {
        dwin->dashed = 0;
    }
    dwin->spaced = 0;

    dwin->lines[0].len = dwin->numchars;
    dwin->lines[0].newline = forced;

    for (i = dwin->scrollback - 1; i > 0; i--) {
        dwin->lines[i] = dwin->lines[i - 1];
        if (i < dwin->height) {
            touch(dwin, i);
        }
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

    touch(dwin, 0);
    dwin->lines[0].len = 0;
    dwin->lines[0].newline = false;
    dwin->lines[0].lm = dwin->ladjw;
    dwin->lines[0].rm = dwin->radjw;
    dwin->lines[0].lpic.reset();
    dwin->lines[0].rpic.reset();
    dwin->lines[0].lhyper = 0;
    dwin->lines[0].rhyper = 0;
    dwin->lines[0].chars.fill(' ');
    dwin->lines[0].attrs.fill(attr_t{});

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
    window_textbuffer_t *dwin = win->window.textbuffer;
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

    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2 - gli_scroll_width) * GLI_SUBPIX;
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
            && !dwin->attrs[linelen - 1].reverse) {
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
    window_textbuffer_t *dwin = win->window.textbuffer;
    if (dwin->numchars > 0 && glk_char_to_upper(dwin->chars[dwin->numchars - 1]) == glk_char_to_upper(ch)) {
        dwin->numchars--;
        touch(dwin, 0);
        return true;
    }
    return false;
}

void win_textbuffer_clear(window_t *win)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int i;

    win->attr.fgcolor = gli_override_fg;
    win->attr.bgcolor = gli_override_bg;
    win->attr.reverse = false;

    dwin->ladjw = dwin->radjw = 0;
    dwin->ladjn = dwin->radjn = 0;

    dwin->spaced = 0;
    dwin->dashed = 0;

    dwin->numchars = 0;

    for (i = 0; i < dwin->scrollback; i++) {
        dwin->lines[i].len = 0;

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

    for (i = 0; i < dwin->height; i++) {
        touch(dwin, i);
    }
}

// Prepare the window for line input.
static void win_textbuffer_init_impl(window_t *win, void *buf, int maxlen, int initlen, bool unicode)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int pw;

    gli_tts_flush();

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

    dwin->echo_line_input = win->echo_line_input;
    dwin->line_terminators = win->line_terminators;

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
    window_textbuffer_t *dwin = win->window.textbuffer;
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
    dwin->line_terminators.clear();
    dwin->inbuf = nullptr;
    dwin->inmax = 0;

    if (dwin->echo_line_input) {
        win_textbuffer_putchar_uni(win, '\n');
    } else {
        dwin->numchars = dwin->infence;
        touch(dwin, 0);
    }

    if (gli_unregister_arr != nullptr) {
        const char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inmax, const_cast<char *>(typedesc), inarrayrock);
    }
}

// Keybinding functions.

// Any key, when text buffer is scrolled.
bool gcmd_accept_scroll(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
    int pageht = dwin->height - 2; // 1 for prompt, 1 for overlap
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
    touchscroll(dwin);

    return startpos || dwin->scrollpos != 0;
}

// Any key, during character input. Ends character input.
void gcmd_buffer_accept_readchar(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->window.textbuffer;
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
    window_textbuffer_t *dwin = win->window.textbuffer;

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

    if (!dwin->line_terminators.empty()) {
        glui32 val2 = keycode;
        if (val2 == keycode_Return) {
            val2 = 0;
        }
        gli_event_store(evtype_LineInput, win, len, val2);
        dwin->line_terminators.clear();
    } else {
        gli_event_store(evtype_LineInput, win, len, 0);
    }
    win->line_request = false;
    win->line_request_uni = false;
    dwin->inbuf = nullptr;
    dwin->inmax = 0;

    if (dwin->echo_line_input) {
        win_textbuffer_putchar_uni(win, '\n');
    } else {
        dwin->numchars = dwin->infence;
        touch(dwin, 0);
    }

    if (gli_unregister_arr != nullptr) {
        const char *typedesc = (inunicode ? "&+#!Iu" : "&+#!Cn");
        (*gli_unregister_arr)(inbuf, inmax, const_cast<char *>(typedesc), inarrayrock);
    }
}

// Any key, during line input.
void gcmd_buffer_accept_readline(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->window.textbuffer;

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

    if (!dwin->line_terminators.empty() && gli_window_check_terminator(arg)) {
        if (std::find(dwin->line_terminators.begin(), dwin->line_terminators.end(), arg) != dwin->line_terminators.end()) {
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
            return;
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
        if (dwin->incurs <= dwin->infence) {
            return;
        }
        dwin->incurs = dwin->infence;
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

    case keycode_Delete:
        if (dwin->incurs <= dwin->infence) {
            return;
        }
        put_text_uni(dwin, nullptr, 0, dwin->incurs - 1, 1);
        break;

    case keycode_Erase:
        if (dwin->incurs >= dwin->numchars) {
            return;
        }
        put_text_uni(dwin, nullptr, 0, dwin->incurs, 1);
        break;

    case keycode_Escape:
        if (dwin->infence >= dwin->numchars) {
            return;
        }
        put_text_uni(dwin, nullptr, 0, dwin->infence, dwin->numchars - dwin->infence);
        break;

    // Regular keys

    case keycode_Return:
        acceptline(win, arg);
        break;

    default:
        if (arg >= 32 && arg <= 0x10FFFF) {
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
        dwin->lines[0].rpic = pic;
        dwin->lines[0].rm = dwin->radjw;
        dwin->lines[0].rhyper = linkval;
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
        dwin->lines[0].lpic = pic;
        dwin->lines[0].lm = dwin->ladjw;
        dwin->lines[0].lhyper = linkval;

        if (align != imagealign_MarginLeft) {
            win_textbuffer_flow_break(dwin);
        }
    }

    return true;
}

bool win_textbuffer_draw_picture(window_textbuffer_t *dwin,
    glui32 image, glui32 align, bool scaled, glui32 width, glui32 height)
{
    glui32 hyperlink;

    auto pic = gli_picture_load(image);

    if (!pic) {
        return false;
    }

    if (!dwin->owner->image_loaded) {
        gli_piclist_increment();
        dwin->owner->image_loaded = true;
    }

    if (scaled) {
        pic = gli_picture_scale(pic.get(), width, height);
    } else {
        pic = gli_picture_scale(pic.get(), gli_zoom_int(pic->w), gli_zoom_int(pic->h));
    }

    hyperlink = dwin->owner->attr.hyper;

    return put_picture(dwin, pic, align, hyperlink);
}

void win_textbuffer_flow_break(window_textbuffer_t *dwin)
{
    while (dwin->ladjn != 0 || dwin->radjn != 0) {
        win_textbuffer_putchar_uni(dwin->owner, '\n');
    }
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

    if (sx > win->bbox.x1 - gli_scroll_width) {
        if (sy < win->bbox.y0 + gli_tmarginy + gli_scroll_width) {
            gcmd_accept_scroll(win, keycode_Up);
        } else if (sy > win->bbox.y1 - gli_tmarginy - gli_scroll_width) {
            gcmd_accept_scroll(win, keycode_Down);
        } else if (sy < (win->bbox.y0 + win->bbox.y1) / 2) {
            gcmd_accept_scroll(win, keycode_PageUp);
        } else {
            gcmd_accept_scroll(win, keycode_PageDown);
        }
        gs = true;
    }

    if (!gh && !gs) {
        gli_copyselect = true;
        gli_start_selection(sx, sy);
    }
}

/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.                    *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)

/* how many pixels we add to left/right margins */
#define SLOP (2 * GLI_SUBPIX)

static void
put_text(window_textbuffer_t *dwin, char *buf, int len, int pos, int oldlen);
static void
put_text_uni(window_textbuffer_t *dwin, glui32 *buf, int len, int pos, int oldlen);
static glui32
put_picture(window_textbuffer_t *dwin, picture_t *pic, glui32 align, glui32 linkval);

static void touch(window_textbuffer_t *dwin, int line)
{
    window_t *win = dwin->owner;
    int y = win->bbox.y0 + gli_tmarginy + (dwin->height - line - 1) * gli_leading;
//    if (dwin->scrollmax && dwin->scrollmax < dwin->height)
//        y -= (dwin->height - dwin->scrollmax) * gli_leading;
    dwin->lines[line].dirty = 1;
    gli_clear_selection();
    winrepaint(win->bbox.x0, y - 2, win->bbox.x1, y + gli_leading + 2);
}

static void touchscroll(window_textbuffer_t *dwin)
{
    window_t *win = dwin->owner;
    int i;
    gli_clear_selection();
    winrepaint(win->bbox.x0, win->bbox.y0, win->bbox.x1, win->bbox.y1);
    for (i = 0; i < dwin->scrollmax; i++)
        dwin->lines[i].dirty = 1;
}

window_textbuffer_t *win_textbuffer_create(window_t *win)
{
    window_textbuffer_t *dwin = malloc(sizeof(window_textbuffer_t));
    dwin->lines = malloc(sizeof(tbline_t) * SCROLLBACK);

    int i;

    dwin->owner = win;

    for (i = 0; i < HISTORYLEN; i++)
        dwin->history[i] = NULL;
    dwin->historypos = 0;
    dwin->historyfirst = 0;
    dwin->historypresent = 0;

    dwin->lastseen = 0;
    dwin->scrollpos = 0;
    dwin->scrollmax = 0;
    dwin->scrollback = SCROLLBACK;

    dwin->width = -1;
    dwin->height = -1;

    dwin->inbuf = NULL;
    dwin->line_terminators = NULL;
    dwin->echo_line_input = TRUE;

    dwin->ladjw = dwin->radjw = 0;
    dwin->ladjn = dwin->radjn = 0;

    dwin->numchars = 0;
    dwin->chars = dwin->lines[0].chars;
    dwin->attrs = dwin->lines[0].attrs;

    dwin->spaced = 0;
    dwin->dashed = 0;

    for (i = 0; i < dwin->scrollback; i++)
    {
        dwin->lines[i].dirty = 0;
        dwin->lines[i].repaint = 0;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].lpic = 0;
        dwin->lines[i].rpic = 0;
        dwin->lines[i].lhyper = 0;
        dwin->lines[i].rhyper = 0;
        dwin->lines[i].len = 0;
        dwin->lines[i].newline = 0;
        memset(dwin->lines[i].chars, ' ', sizeof dwin->lines[i].chars);
        memset(dwin->lines[i].attrs,   0, sizeof dwin->lines[i].attrs);
    }

    memcpy(dwin->styles, gli_tstyles, sizeof gli_tstyles);

    dwin->copybuf = 0;
    dwin->copypos = 0;

    return dwin;
}

void win_textbuffer_destroy(window_textbuffer_t *dwin)
{
    int i;

    if (dwin->inbuf)
    {
        if (gli_unregister_arr)
            (*gli_unregister_arr)(dwin->inbuf, dwin->inmax, "&+#!Cn", dwin->inarrayrock);
        dwin->inbuf = NULL;
    }

    dwin->owner = NULL;

    if (dwin->copybuf)
        free(dwin->copybuf);

    if (dwin->line_terminators)
        free(dwin->line_terminators);

    for (i = 0; i < dwin->scrollback; i++) {
        gli_picture_decrement(dwin->lines[i].lpic);
        gli_picture_decrement(dwin->lines[i].rpic);
    }

    free(dwin->lines);
    free(dwin);
}

static void reflow(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    int inputbyte = -1;
    attr_t curattr;
    attr_t oldattr;
    int i, k, p, s;
    int x;

    if (dwin->height < 4 || dwin->width < 20)
        return;

    dwin->lines[0].len = dwin->numchars;

    /* allocate temp buffers */
    attr_t *attrbuf = malloc(sizeof(attr_t) * SCROLLBACK * TBLINELEN);
    glui32 *charbuf = malloc(sizeof(glui32) * SCROLLBACK * TBLINELEN);
    int *alignbuf = malloc(sizeof(int) * SCROLLBACK);
    picture_t **pictbuf = malloc(sizeof(picture_t *) * SCROLLBACK);
    glui32 *hyperbuf = malloc(sizeof(glui32) * SCROLLBACK);
    int *offsetbuf = malloc(sizeof(int) * SCROLLBACK);

    if (!attrbuf || !charbuf || !alignbuf || !pictbuf || !hyperbuf || !offsetbuf)
    {
        free(attrbuf);
        free(charbuf);
        free(alignbuf);
        free(pictbuf);
        free(hyperbuf);
        free(offsetbuf);
        return;
    }

    /* copy text to temp buffers */

    oldattr = win->attr;
    attrclear(&curattr);

    x = 0;
    p = 0;
    s = dwin->scrollmax < SCROLLBACK ? dwin->scrollmax : SCROLLBACK - 1;

    for (k = s; k >= 0; k--)
    {
        if (k == 0 && win->line_request)
            inputbyte = p + dwin->infence;

        if (dwin->lines[k].lpic)
        {
            offsetbuf[x] = p;
            alignbuf[x] = imagealign_MarginLeft;
            pictbuf[x] = dwin->lines[k].lpic;
            gli_picture_increment(pictbuf[x]);
            hyperbuf[x] = dwin->lines[k].lhyper;
            x++;
        }

        if (dwin->lines[k].rpic)
        {
            offsetbuf[x] = p;
            alignbuf[x] = imagealign_MarginRight;
            pictbuf[x] = dwin->lines[k].rpic;
            gli_picture_increment(pictbuf[x]);
            hyperbuf[x] = dwin->lines[k].rhyper;
            x++;
        }

        for (i = 0; i < dwin->lines[k].len; i++)
        {
            attrbuf[p] = curattr = dwin->lines[k].attrs[i];
            charbuf[p] = dwin->lines[k].chars[i];
            p++;
        }

        if (dwin->lines[k].newline)
        {
            attrbuf[p] = curattr;
            charbuf[p] = '\n';
            p++;
        }
    }

    offsetbuf[x] = -1;

    /* clear window */

    win_textbuffer_clear(win);

    /* and dump text back */

    x = 0;
    for (i = 0; i < p; i++)
    {
        if (i == inputbyte)
            break;
        win->attr = attrbuf[i];

        if (offsetbuf[x] == i)
        {
            put_picture(dwin, pictbuf[x], alignbuf[x], hyperbuf[x]);
            x ++;
        }

        win_textbuffer_putchar_uni(win, charbuf[i]);
    }

    /* terribly sorry about this... */
    dwin->lastseen = 0;
    dwin->scrollpos = 0;

    if (inputbyte != -1)
    {
        dwin->infence = dwin->numchars;
        put_text_uni(dwin, charbuf + inputbyte, p - inputbyte, dwin->numchars, 0);
        dwin->incurs = dwin->numchars;
    }

    /* free temp buffers */
    free(attrbuf);
    free(charbuf);
    free(alignbuf);
    free(pictbuf);
    free(hyperbuf);
    free(offsetbuf);

    win->attr = oldattr;

    touchscroll(dwin);
}

void win_textbuffer_rearrange(window_t *win, rect_t *box)
{
    window_textbuffer_t *dwin = win->data;
    int newwid, newhgt;
    int rnd;
    int i;

    dwin->owner->bbox = *box;

    newwid = (box->x1 - box->x0 - gli_tmarginx * 2 - gli_scroll_width) / gli_cellw;
    newhgt = (box->y1 - box->y0 - gli_tmarginy * 2) / gli_cellh;

    /* align text with bottom */
    rnd = newhgt * gli_cellh + gli_tmarginy * 2;
    win->yadj = (box->y1 - box->y0 - rnd);
    dwin->owner->bbox.y0 += (box->y1 - box->y0 - rnd);

    if (newwid != dwin->width)
    {
        dwin->width = newwid;
        reflow(win);
    }

    if (newhgt != dwin->height)
    {
        /* scroll up if we obscure new lines */
        if (dwin->lastseen >= newhgt - 1)
            dwin->scrollpos += (dwin->height - newhgt);

        dwin->height = newhgt;

        /* keep window within 'valid' lines */
        if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1)
            dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
        if (dwin->scrollpos < 0)
            dwin->scrollpos = 0;
        touchscroll(dwin);

        /* allocate copy buffer */
        if (dwin->copybuf)
            free(dwin->copybuf);

        dwin->copybuf = malloc(sizeof(glui32) * dwin->height * TBLINELEN);

        for (i = 0; i < (dwin->height * TBLINELEN); i++)
            dwin->copybuf[i] = 0;

        dwin->copypos = 0;
    }
}

static int calcwidth(window_textbuffer_t *dwin,
    glui32 *chars, attr_t *attrs,
    int startchar, int numchars, int spw)
{
    int w = 0;
    int a, b;

    a = startchar;
    for (b = startchar; b < numchars; b++)
    {
        if (!attrequal(&attrs[a], &attrs[b]))
        {
            w += gli_string_width_uni(attrfont(dwin->styles, &attrs[a]),
                    chars + a, b - a, spw);
            a = b;
        }
    }

    w += gli_string_width_uni(attrfont(dwin->styles, &attrs[a]),
            chars + a, b - a, spw);

    return w;
}

void win_textbuffer_redraw(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    tbline_t *ln;
    int linelen;
    int nsp, spw, pw;
    int x0, y0, x1, y1;
    int x, y, w;
    int a, b;
    glui32 link;
    int font;
    unsigned char *color;
    int i;
    int hx0, hx1, hy0, hy1;
    int selbuf, selrow, selchar, sx0, sx1, selleft, selright;
    int tx, tsc, tsw, lsc, rsc;

    dwin->lines[0].len = dwin->numchars;

    ln = malloc(sizeof(tbline_t));
    if (!ln)
        return;

    x0 = (win->bbox.x0 + gli_tmarginx) * GLI_SUBPIX;
    x1 = (win->bbox.x1 - gli_tmarginx - gli_scroll_width) * GLI_SUBPIX;
    y0 = win->bbox.y0 + gli_tmarginy;
    y1 = win->bbox.y1 - gli_tmarginy;

    pw = x1 - x0 - 2 * GLI_SUBPIX;

//    if (dwin->scrollmax && dwin->scrollmax < dwin->height)
//        y0 -= (dwin->height - dwin->scrollmax) * gli_leading;

    /* check if any part of buffer is selected */
    selbuf = gli_check_selection(x0/GLI_SUBPIX,y0,x1/GLI_SUBPIX,y1);

    for (i = dwin->scrollpos + dwin->height - 1; i >= dwin->scrollpos; i--)
    {
        /* top of line */
        y = y0 + (dwin->height - (i - dwin->scrollpos) - 1) * gli_leading;

        /* check if part of line is selected */
        if (selbuf)
        {
            selrow = gli_get_selection(x0/GLI_SUBPIX, y,
                    x1/GLI_SUBPIX, y + gli_leading,
                    &sx0, &sx1);
            selleft = (sx0 == x0/GLI_SUBPIX);
            selright = (sx1 == x1/GLI_SUBPIX);
        }
        else
        {
            selrow = FALSE;
        }

        /* mark selected line dirty */
        if (selrow)
            dwin->lines[i].dirty = TRUE;

        memcpy(ln, dwin->lines + i, sizeof(tbline_t));

        /* skip if we can */
        if (!ln->dirty && !ln->repaint && !gli_force_redraw && dwin->scrollpos == 0)
            continue;

        /* repaint previously selected lines if needed */
        if (ln->repaint && !gli_force_redraw)
            gli_redraw_rect(x0/GLI_SUBPIX, y, x1/GLI_SUBPIX, y + gli_leading);

        /* keep selected line dirty and flag for repaint */
        if (!selrow)
        {
            dwin->lines[i].dirty = FALSE;
            dwin->lines[i].repaint = FALSE;
        }
        else
        {
            dwin->lines[i].repaint = TRUE;
        }

        /* leave bottom line blank for [more] prompt */
        if (i == dwin->scrollpos && i > 0)
            continue;

        linelen = ln->len;

        /* kill spaces at the end unless they're a different color*/
        color = gli_override_bg_set ? gli_window_color : win->bgcolor;
        while (i > 0 && linelen > 1 && ln->chars[linelen-1] == ' ' 
            && dwin->styles[ln->attrs[linelen-1].style].bg == color 
            && !dwin->styles[ln->attrs[linelen-1].style].reverse)
                linelen --;

        /* kill characters that would overwrite the scroll bar */
        while (linelen > 1 && calcwidth(dwin, ln->chars, ln->attrs, 0, linelen, -1) >= pw)
            linelen --;

        /*
         * count spaces and width for justification
         */
        if (gli_conf_justify && !ln->newline && i > 0)
        {
            for (a = 0, nsp = 0; a < linelen; a++)
                if (ln->chars[a] == ' ')
                    nsp ++;
            w = calcwidth(dwin, ln->chars, ln->attrs, 0, linelen, 0);
            if (nsp)
                spw = (x1 - x0 - ln->lm - ln->rm - 2 * SLOP - w) / nsp;
            else
                spw = 0;
        }
        else
        {
            spw = -1;
        }

        /* find and highlight selected characters */
        if (selrow && !gli_claimselect)
        {
            lsc = 0;
            rsc = 0;
            selchar = FALSE;
            /* optimized case for all chars selected */
            if (selleft && selright)
            {
                rsc = linelen > 0 ? linelen - 1 : 0;
                selchar = calcwidth(dwin, ln->chars, ln->attrs, lsc, rsc, spw)/GLI_SUBPIX;
            }
            else
            {
                /* optimized case for leftmost char selected */
                if (selleft)
                {
                    tsc = linelen > 0 ? linelen - 1 : 0;
                    selchar = calcwidth(dwin, ln->chars, ln->attrs, lsc, tsc, spw)/GLI_SUBPIX;
                }
                else
                {
                    /* find the substring contained by the selection */
                    tx = (x0 + SLOP + ln->lm)/GLI_SUBPIX;
                    /* measure string widths until we find left char */
                    for (tsc = 0; tsc < linelen; tsc++)
                    {
                        tsw = calcwidth(dwin, ln->chars, ln->attrs, 0, tsc, spw)/GLI_SUBPIX;
                        if (tsw + tx >= sx0 ||
                                tsw + tx + GLI_SUBPIX >= sx0 && ln->chars[tsc] != ' ')
                        {
                            lsc = tsc;
                            selchar = TRUE;
                            break;
                        }
                    }
                }
                if (selchar)
                {
                    /* optimized case for rightmost char selected */
                    if (selright)
                    {
                        rsc = linelen > 0 ? linelen - 1 : 0;
                    }
                    else
                    {
                    /* measure string widths until we find right char */
                        for (tsc = lsc; tsc < linelen; tsc++)
                        {
                            tsw = calcwidth(dwin, ln->chars, ln->attrs, lsc, tsc, spw)/GLI_SUBPIX;
                            if (tsw + sx0 < sx1)
                                rsc = tsc;
                        }
                        if (lsc && !rsc)
                            rsc = lsc;
                    }
                }
            }
            /* reverse colors for selected chars */
            if (selchar)
            {
                for (tsc = lsc; tsc <= rsc; tsc++)
                {
                    ln->attrs[tsc].reverse = !ln->attrs[tsc].reverse;
                    dwin->copybuf[dwin->copypos] = ln->chars[tsc];
                    dwin->copypos++;
                }
            }
            /* add newline to copy buffer */
            dwin->copybuf[dwin->copypos] = '\n';
            dwin->copypos++;
        }

        /* clear any stored hyperlink coordinates */
        gli_put_hyperlink(0, x0/GLI_SUBPIX, y,
                x1/GLI_SUBPIX, y + gli_leading);

        /*
         * fill in background colors
         */
        color = gli_override_bg_set ? gli_window_color : win->bgcolor;
        gli_draw_rect(x0/GLI_SUBPIX, y,
                (x1-x0) / GLI_SUBPIX, gli_leading,
                color);

        x = x0 + SLOP + ln->lm;
        a = 0;
        for (b = 0; b < linelen; b++)
        {
            if (!attrequal(&ln->attrs[a], &ln->attrs[b]))
            {
                link = ln->attrs[a].hyper;
                font = attrfont(dwin->styles, &ln->attrs[a]);
                color = attrbg(dwin->styles, &ln->attrs[a]);
                w = gli_string_width_uni(font, ln->chars + a, b - a, spw);
                gli_draw_rect(x/GLI_SUBPIX, y,
                        w/GLI_SUBPIX, gli_leading,
                        color);
                if (link)
                {
                    gli_draw_rect(x/GLI_SUBPIX + 1, y + gli_baseline + 1,
                            w/GLI_SUBPIX + 1, gli_link_style,
                            gli_link_color);
                    gli_put_hyperlink(link, x/GLI_SUBPIX, y,
                            x/GLI_SUBPIX + w/GLI_SUBPIX,
                            y + gli_leading);
                }
                x += w;
                a = b;
            }
        }
        link = ln->attrs[a].hyper;
        font = attrfont(dwin->styles, &ln->attrs[a]);
        color = attrbg(dwin->styles, &ln->attrs[a]);
        w = gli_string_width_uni(font, ln->chars + a, b - a, spw);
        gli_draw_rect(x/GLI_SUBPIX, y, w/GLI_SUBPIX,
                gli_leading, color);
        if (link)
        {
            gli_draw_rect(x/GLI_SUBPIX + 1, y + gli_baseline + 1,
                    w/GLI_SUBPIX + 1, gli_link_style,
                    gli_link_color);
            gli_put_hyperlink(link, x/GLI_SUBPIX, y,
                    x/GLI_SUBPIX + w/GLI_SUBPIX,
                    y + gli_leading);
        }
        x += w;

        color = gli_override_bg_set ? gli_window_color : win->bgcolor;
        gli_draw_rect(x/GLI_SUBPIX, y,
                x1/GLI_SUBPIX - x/GLI_SUBPIX, gli_leading,
                color);

        /*
         * draw caret
         */

        if (gli_focuswin == win && i == 0 && (win->line_request || win->line_request_uni))
        {
            w = calcwidth(dwin, dwin->chars, dwin->attrs, 0, dwin->incurs, spw);
            if (w < pw - gli_caret_shape * 2 * GLI_SUBPIX)
                gli_draw_caret(x0 + SLOP + ln->lm + w, y + gli_baseline);
        }

        /*
         * draw text
         */

        x = x0 + SLOP + ln->lm;
        a = 0;
        for (b = 0; b < linelen; b++)
        {
            if (!attrequal(&ln->attrs[a], &ln->attrs[b]))
            {
                link = ln->attrs[a].hyper;
                font = attrfont(dwin->styles, &ln->attrs[a]);
                color = link ? gli_link_color : attrfg(dwin->styles, &ln->attrs[a]);
                x = gli_draw_string_uni(x, y + gli_baseline,
                        font, color, ln->chars + a, b - a, spw);
                a = b;
            }
        }
        link = ln->attrs[a].hyper;
        font = attrfont(dwin->styles, &ln->attrs[a]);
        color = link ? gli_link_color : attrfg(dwin->styles, &ln->attrs[a]);
        gli_draw_string_uni(x, y + gli_baseline,
                font, color, ln->chars + a, linelen - a, spw);
    }

    /*
     * draw more prompt
     */
    if (dwin->scrollpos && dwin->height > 1)
    {
        x = x0 + SLOP;
        y = y0 + (dwin->height - 1) * gli_leading;

        gli_put_hyperlink(0, x0/GLI_SUBPIX, y,
                x1/GLI_SUBPIX, y + gli_leading);

        color = gli_override_bg_set ? gli_window_color : win->bgcolor;
        gli_draw_rect(x/GLI_SUBPIX, y,
                x1/GLI_SUBPIX - x/GLI_SUBPIX, gli_leading,
                color);

        w = gli_string_width(gli_more_font,
                gli_more_prompt, strlen(gli_more_prompt), -1);

        if (gli_more_align == 1)    /* center */
            x = x0 + SLOP + (x1 - x0 - w - SLOP * 2) / 2;
        if (gli_more_align == 2)    /* right */
            x = x1 - SLOP - w;

        color = gli_override_fg_set ? gli_more_color : win->fgcolor;
        gli_draw_string(x, y + gli_baseline, 
                gli_more_font, color,
                gli_more_prompt, strlen(gli_more_prompt), -1);
        y1 = y; /* don't want pictures overdrawing "[more]" */

        /* try to claim the focus */
        dwin->owner->more_request = TRUE;
        gli_more_focus = TRUE;
    }
    else
    {
        dwin->owner->more_request = FALSE;
        y1 = y0 + dwin->height * gli_leading;
    }

    /*
     * draw the images
     */
    for (i = 0; i < dwin->scrollback; i++)
    {
        memcpy(ln, dwin->lines + i, sizeof(tbline_t));

        y = y0 + (dwin->height - (i - dwin->scrollpos) - 1) * gli_leading;

        if (ln->lpic)
        {
            if (y < y1 && y + ln->lpic->h > y0)
            {
                gli_draw_picture(ln->lpic,
                        x0/GLI_SUBPIX, y,
                        x0/GLI_SUBPIX, y0, x1/GLI_SUBPIX, y1);
                link = ln->lhyper;
                hy0 = y > y0 ? y : y0;
                hy1 = y + ln->lpic->h < y1 ? y + ln->lpic->h : y1;
                hx0 = x0/GLI_SUBPIX;
                hx1 = x0/GLI_SUBPIX + ln->lpic->w < x1/GLI_SUBPIX
                            ? x0/GLI_SUBPIX + ln->lpic->w
                            : x1/GLI_SUBPIX;
                gli_put_hyperlink(link, hx0, hy0, hx1, hy1);
            }
        }

        if (ln->rpic)
        {
            if (y < y1 && y + ln->rpic->h > y0)
            {
                gli_draw_picture(ln->rpic,
                        x1/GLI_SUBPIX - ln->rpic->w, y,
                        x0/GLI_SUBPIX, y0, x1/GLI_SUBPIX, y1);
                link = ln->rhyper;
                hy0 = y > y0 ? y : y0;
                hy1 = y + ln->rpic->h < y1 ? y + ln->rpic->h : y1;
                hx0 = x1/GLI_SUBPIX - ln->rpic->w > x0/GLI_SUBPIX
                            ? x1/GLI_SUBPIX - ln->rpic->w
                            : x0/GLI_SUBPIX;
                hx1 = x1/GLI_SUBPIX;
                gli_put_hyperlink(link, hx0, hy0, hx1, hy1);
            }
        }
    }

    /*
     * Draw the scrollbar
     */

    /* try to claim scroll keys */
    dwin->owner->scroll_request = dwin->scrollmax > dwin->height;

    if (dwin->owner->scroll_request && gli_scroll_width)
    {
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
        if (dwin->scrollmax > dwin->height)
        {
            t0 = t0 * (y1 - y0) / dwin->scrollmax + y0;
            t1 = t1 * (y1 - y0) / dwin->scrollmax + y0;
        }
        else
        {
            t0 = t1 = y0;
        }

        gli_draw_rect(x0+1, y0, x1-x0-2, y1-y0, gli_scroll_bg);
        gli_draw_rect(x0+1, t0, x1-x0-2, t1-t0, gli_scroll_fg);

        for (i = 0; i < gli_scroll_width / 2 + 1; i++)
        {
            gli_draw_rect(x0+gli_scroll_width/2-i,
                    y0 - gli_scroll_width/2 + i,
                    i*2, 1, gli_scroll_fg);
            gli_draw_rect(x0+gli_scroll_width/2-i,
                    y1 + gli_scroll_width/2 - i,
                    i*2, 1, gli_scroll_fg);
        }
    }

    /* send selected text to clipboard */
    if (selbuf && dwin->copypos)
    {
        gli_claimselect = TRUE;
        gli_clipboard_copy((glui32 *)dwin->copybuf, dwin->copypos);
        for (i = 0; i < dwin->copypos; i++)
            dwin->copybuf[i] = 0;
        dwin->copypos = 0;
    }

    /* no more prompt means all text has been seen */
    if (!dwin->owner->more_request)
        dwin->lastseen = 0;

    free(ln);
}

static void scrollresize(window_textbuffer_t *dwin)
{
    int i;

    tbline_t *newlines = realloc(dwin->lines, sizeof(tbline_t) * (dwin->scrollback + SCROLLBACK));

    if (!newlines)
        return;

    dwin->lines = newlines;
    dwin->chars = dwin->lines[0].chars;
    dwin->attrs = dwin->lines[0].attrs;

    for (i = dwin->scrollback; i < (dwin->scrollback + SCROLLBACK); i++)
    {
        dwin->lines[i].dirty = 0;
        dwin->lines[i].repaint = 0;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].lpic = 0;
        dwin->lines[i].rpic = 0;
        dwin->lines[i].lhyper = 0;
        dwin->lines[i].rhyper = 0;
        dwin->lines[i].len = 0;
        dwin->lines[i].newline = 0;
        memset(dwin->lines[i].chars, ' ', sizeof dwin->lines[i].chars);
        memset(dwin->lines[i].attrs,   0, sizeof dwin->lines[i].attrs);
    }

    dwin->scrollback += SCROLLBACK;
}

static void scrolloneline(window_textbuffer_t *dwin, int forced)
{
    int i;

    dwin->lastseen ++;
    dwin->scrollmax ++;

    if (dwin->scrollmax > dwin->scrollback - 1
            || dwin->lastseen > dwin->scrollback - 1)
        scrollresize(dwin);

    if (dwin->lastseen >= dwin->height)
        dwin->scrollpos ++;

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1)
        dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
    if (dwin->scrollpos < 0)
        dwin->scrollpos = 0;

    if (forced)
        dwin->dashed = 0;
    dwin->spaced = 0;

    dwin->lines[0].len = dwin->numchars;
    dwin->lines[0].newline = forced;

    for (i = dwin->scrollback - 1; i > 0; i--)
    {
        memcpy(dwin->lines+i, dwin->lines+i-1, sizeof(tbline_t));
        if (i < dwin->height)
            touch(dwin, i);
    }

    if (dwin->radjn)
        dwin->radjn--;
    if (dwin->radjn == 0)
        dwin->radjw = 0;
    if (dwin->ladjn)
        dwin->ladjn--;
    if (dwin->ladjn == 0)
        dwin->ladjw = 0;

    touch(dwin, 0);
    dwin->lines[0].len = 0;
    dwin->lines[0].newline = 0;
    dwin->lines[0].lm = dwin->ladjw;
    dwin->lines[0].rm = dwin->radjw;
    dwin->lines[0].lpic = NULL;
    dwin->lines[0].rpic = NULL;
    dwin->lines[0].lhyper = 0;
    dwin->lines[0].rhyper = 0;
    memset(dwin->chars, ' ', TBLINELEN * 4);
    memset(dwin->attrs, 0, TBLINELEN * sizeof(attr_t));

    dwin->numchars = 0;

    touchscroll(dwin);
}

/* only for input text */
static void put_text(window_textbuffer_t *dwin, char *buf, int len, int pos, int oldlen)
{
    int diff = len - oldlen;

    if (dwin->numchars + diff >= TBLINELEN)
        return;

    if (diff != 0 && pos + oldlen < dwin->numchars)
    {
        memmove(dwin->chars + pos + len,
                dwin->chars + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * 4);
        memmove(dwin->attrs + pos + len,
                dwin->attrs + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * sizeof(attr_t));
    }
    if (len > 0)
    {
        int i;
        for (i = 0; i < len; i++)
        {
            dwin->chars[pos + i] = buf[i];
            attrset(&dwin->attrs[pos + i], style_Input);
        }
    }
    dwin->numchars += diff;

    if (dwin->inbuf)
    {
        if (dwin->incurs >= pos + oldlen)
            dwin->incurs += diff;
        else if (dwin->incurs >= pos)
            dwin->incurs = pos + len;
    }

    touch(dwin, 0);
}

static void put_text_uni(window_textbuffer_t *dwin, glui32 *buf, int len, int pos, int oldlen)
{
    int diff = len - oldlen;

    if (dwin->numchars + diff >= TBLINELEN)
        return;

    if (diff != 0 && pos + oldlen < dwin->numchars)
    {
        memmove(dwin->chars + pos + len,
                dwin->chars + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * 4);
        memmove(dwin->attrs + pos + len,
                dwin->attrs + pos + oldlen,
                (dwin->numchars - (pos + oldlen)) * sizeof(attr_t));
    }
    if (len > 0)
    {
        int i;
        memmove(dwin->chars + pos, buf, len * 4);
        for (i = 0; i < len; i++)
            attrset(&dwin->attrs[pos+i], style_Input);
    }
    dwin->numchars += diff;

    if (dwin->inbuf)
    {
        if (dwin->incurs >= pos + oldlen)
            dwin->incurs += diff;
        else if (dwin->incurs >= pos)
            dwin->incurs = pos + len;
    }

    touch(dwin, 0);
}

void win_textbuffer_putchar_uni(window_t *win, glui32 ch)
{
    window_textbuffer_t *dwin = win->data;
    glui32 bchars[TBLINELEN];
    attr_t battrs[TBLINELEN];
    int pw;
    int bpoint;
    int saved;
    int i;
    int linelen;
    unsigned char *color;

#ifdef USETTS
    { char b[1]; b[0] = ch; gli_speak_tts(b, 1, 0); }
#endif

    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2 - gli_scroll_width) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw - dwin->ladjw;

    color = gli_override_bg_set ? gli_window_color : win->bgcolor;

    /* oops ... overflow */
    if (dwin->numchars + 1 >= TBLINELEN)
        scrolloneline(dwin, 0);

    if (ch == '\n')
    {
        scrolloneline(dwin, 1);
        return;
    }

    if (gli_conf_quotes)
    {
#define LEFTQUOTE(c)	((c) == ' ' || (c) == '(' || (c) == '[')
        /* fails for 'tis a wonderful day in the '80s */
        if (gli_conf_quotes > 1 && ch == '\'')
        {
            if (dwin->numchars == 0 || LEFTQUOTE(dwin->chars[dwin->numchars-1]))
                ch = UNI_LSQUO;
        }

        if (ch == '`')
            ch = UNI_LSQUO;

        if (ch == '\'')
            ch = UNI_RSQUO;

        if (ch == '"')
        {
            if (dwin->numchars == 0 || LEFTQUOTE(dwin->chars[dwin->numchars-1]))
                ch = UNI_LDQUO;
            else
                ch = UNI_RDQUO;
        }
#undef LEFTQUOTE
    }

    if (gli_conf_dashes && win->attr.style != style_Preformatted)
    {
        if (ch == '-')
        {
            dwin->dashed ++;
            if (dwin->dashed == 2)
            {
                dwin->numchars--;
                if (gli_conf_dashes == 2)
                    ch = UNI_NDASH;
                else
                    ch = UNI_MDASH;
            }
            if (dwin->dashed == 3)
            {
                dwin->numchars--;
                ch = UNI_MDASH;
                dwin->dashed = 0;
            }
        }
        else
            dwin->dashed = 0;
    }

    if (gli_conf_spaces && win->attr.style != style_Preformatted 
        && dwin->styles[win->attr.style].bg == color 
        && !dwin->styles[win->attr.style].reverse)
    {
        /* turn (period space space) into (period space) */
        if (gli_conf_spaces == 1)
        {
            if (ch == '.')
                dwin->spaced = 1;
            else if (ch == ' ' && dwin->spaced == 1)
                dwin->spaced = 2;
            else if (ch == ' ' && dwin->spaced == 2)
            {
                dwin->spaced = 0;
                return;
            }
            else
                dwin->spaced = 0;
        }

        /* turn (per sp x) into (per sp sp x) */
        if (gli_conf_spaces == 2)
        {
            if (ch == '.')
                dwin->spaced = 1;
            else if (ch == ' ' && dwin->spaced == 1)
                dwin->spaced = 2;
            else if (ch != ' ' && dwin->spaced == 2)
            {
                dwin->spaced = 0;
                win_textbuffer_putchar_uni(win, ' ');
            }
            else
                dwin->spaced = 0;
        }
    }

    dwin->chars[dwin->numchars] = ch;
    dwin->attrs[dwin->numchars] = win->attr;
    dwin->numchars++;

    /* kill spaces at the end for line width calculation */
    linelen = dwin->numchars;
    while (linelen > 1 && dwin->chars[linelen-1] == ' ' 
        && dwin->styles[dwin->attrs[linelen-1].style].bg == color 
        && !dwin->styles[dwin->attrs[linelen-1].style].reverse)
        linelen --;

    if (calcwidth(dwin, dwin->chars, dwin->attrs, 0, linelen, -1) >= pw)
    {
        bpoint = dwin->numchars;

        for (i = dwin->numchars - 1; i > 0; i--)
            if (dwin->chars[i] == ' ')
            {
                bpoint = i + 1; /* skip space */
                break;
            }

        saved = dwin->numchars - bpoint;

        memcpy(bchars, dwin->chars + bpoint, saved * 4);
        memcpy(battrs, dwin->attrs + bpoint, saved * sizeof(attr_t));
        dwin->numchars = bpoint;

        scrolloneline(dwin, 0);

        memcpy(dwin->chars, bchars, saved * 4);
        memcpy(dwin->attrs, battrs, saved * sizeof(attr_t));
        dwin->numchars = saved;
    }

    touch(dwin, 0);
}

int win_textbuffer_unputchar_uni(window_t *win, glui32 ch)
{
    window_textbuffer_t *dwin = win->data;
    if (dwin->numchars > 0 && dwin->chars[dwin->numchars - 1] == ch)
    {
        dwin->numchars--;
        touch(dwin, 0);
        return TRUE;
    }
    return FALSE;
}

void win_textbuffer_clear(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    int i;

    win->attr.fgset = gli_override_fg_set;
    win->attr.bgset = gli_override_bg_set;
    win->attr.fgcolor = gli_override_fg_set ? gli_override_fg_val : 0;
    win->attr.bgcolor = gli_override_bg_set ? gli_override_bg_val : 0;
    win->attr.reverse = FALSE;

    dwin->ladjw = dwin->radjw = 0;
    dwin->ladjn = dwin->radjn = 0;

    dwin->spaced = 0;
    dwin->dashed = 0;

    dwin->numchars = 0;

    for (i = 0; i < dwin->scrollback; i++)
    {
        dwin->lines[i].len = 0;

        gli_picture_decrement(dwin->lines[i].lpic);
        dwin->lines[i].lpic = 0;
        gli_picture_decrement(dwin->lines[i].rpic);
        dwin->lines[i].rpic = 0;

        dwin->lines[i].lhyper = 0;
        dwin->lines[i].rhyper = 0;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].newline = 0;
        dwin->lines[i].dirty = 1;
        dwin->lines[i].repaint = 0;
    }

    dwin->lastseen = 0;
    dwin->scrollpos = 0;
    dwin->scrollmax = 0;

    for (i = 0; i < dwin->height; i++)
        touch(dwin, i);

    /* only need this because redraw won't touch lines below line 0,
     * and we scroll text up to top of window if it's too short. */
//    gli_draw_rect(win->bbox.x0, win->bbox.y0,
//            win->bbox.x1 - win->bbox.x0,
//            win->bbox.y1 - win->bbox.y0,
//            gli_window_color);
}

/* Prepare the window for line input. */
void win_textbuffer_init_line(window_t *win, char *buf, int maxlen, int initlen)
{
    window_textbuffer_t *dwin = win->data;
    int pw;

#ifdef USETTS
    gli_speak_tts("\n", 1, 0);
#endif

    /* because '>' prompt is ugly without extra space */
    if (dwin->numchars && dwin->chars[dwin->numchars-1] == '>')
        win_textbuffer_putchar_uni(win, ' ');
    if (dwin->numchars && dwin->chars[dwin->numchars-1] == '?')
        win_textbuffer_putchar_uni(win, ' ');

    /* make sure we have some space left for typing... */
    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw + dwin->ladjw;
    if (calcwidth(dwin, dwin->chars, dwin->attrs, 0, dwin->numchars, -1) >= pw * 3 / 4)
        win_textbuffer_putchar_uni(win, '\n');

    //dwin->lastseen = 0;

    dwin->inbuf = buf;
    dwin->inmax = maxlen;
    dwin->infence = dwin->numchars;
    dwin->incurs = dwin->numchars;
    dwin->origattr = win->attr;
    attrset(&win->attr, style_Input);

    dwin->historypos = dwin->historypresent;

    if (initlen)
    {
        touch(dwin, 0);
        put_text(dwin, buf, initlen, dwin->incurs, 0);
    }

    dwin->echo_line_input = win->echo_line_input;

    if (win->line_terminators && win->termct)
    {
        dwin->line_terminators = malloc((win->termct + 1) * sizeof(glui32));

        if (dwin->line_terminators)
        {
            memcpy(dwin->line_terminators, win->line_terminators, win->termct * sizeof(glui32));
            dwin->line_terminators[win->termct] = 0;
        }
    }

    if (gli_register_arr)
        dwin->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
}

void win_textbuffer_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen)
{
    window_textbuffer_t *dwin = win->data;
    int pw;

#ifdef USETTS
    gli_speak_tts("\n", 1, 0);
#endif

    /* because '>' prompt is ugly without extra space */
    if (dwin->numchars && dwin->chars[dwin->numchars-1] == '>')
        win_textbuffer_putchar_uni(win, ' ');
    if (dwin->numchars && dwin->chars[dwin->numchars-1] == '?')
        win_textbuffer_putchar_uni(win, ' ');

    /* make sure we have some space left for typing... */
    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw + dwin->ladjw;
    if (calcwidth(dwin, dwin->chars, dwin->attrs, 0, dwin->numchars, -1) >= pw * 3 / 4)
        win_textbuffer_putchar_uni(win, '\n');

    //dwin->lastseen = 0;

    dwin->inbuf = buf;
    dwin->inmax = maxlen;
    dwin->infence = dwin->numchars;
    dwin->incurs = dwin->numchars;
    dwin->origattr = win->attr;
    attrset(&win->attr, style_Input);

    dwin->historypos = dwin->historypresent;

    if (initlen)
    {
        touch(dwin, 0);
        put_text_uni(dwin, buf, initlen, dwin->incurs, 0);
    }

    dwin->echo_line_input = win->echo_line_input;

    if (win->line_terminators && win->termct)
    {
        dwin->line_terminators = malloc((win->termct + 1) * sizeof(glui32));

        if (dwin->line_terminators)
        {
            memcpy(dwin->line_terminators, win->line_terminators, win->termct * sizeof(glui32));
            dwin->line_terminators[win->termct] = 0;
        }
    }

    if (gli_register_arr)
        dwin->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Iu");
}

/* Abort line input, storing whatever's been typed so far. */
void win_textbuffer_cancel_line(window_t *win, event_t *ev)
{
    window_textbuffer_t *dwin = win->data;
    gidispatch_rock_t inarrayrock;
    int ix;
    int len;
    void *inbuf;
    int inmax;
    int unicode = win->line_request_uni;

    if (!dwin->inbuf)
        return;

    inbuf = dwin->inbuf;
    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;

    len = dwin->numchars - dwin->infence;
    if (win->echostr) 
        gli_stream_echo_line_uni(win->echostr, dwin->chars + dwin->infence, len);

    if (len > inmax)
        len = inmax;

    if (!unicode)
    {
        for (ix=0; ix<len; ix++)
        {
            glui32 ch = dwin->chars[dwin->infence+ix];
            if (ch > 0xff)
                ch = '?';
            ((char *)inbuf)[ix] = (char)ch;
        }
    }
    else
    {
        for (ix=0; ix<len; ix++)
            ((glui32 *)inbuf)[ix] = dwin->chars[dwin->infence+ix];
    }

    win->attr = dwin->origattr;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = len;
    ev->val2 = 0;

    win->line_request = FALSE;
    win->line_request_uni = FALSE;
    if (dwin->line_terminators)
    {
        free(dwin->line_terminators);
        dwin->line_terminators = NULL;
    }
    dwin->inbuf = NULL;
    dwin->inmax = 0;

    if (dwin->echo_line_input)
    {
        win_textbuffer_putchar_uni(win, '\n');
    }
    else
    {
        dwin->numchars = dwin->infence;
        touch(dwin, 0);
    }

    if (gli_unregister_arr)
        (*gli_unregister_arr)(inbuf, inmax, unicode ? "&+#!Iu" : "&+#!Cn", inarrayrock);
}

/* Keybinding functions. */

/* Any key, when text buffer is scrolled. */
int gcmd_accept_scroll(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->data;
    int pageht = dwin->height - 2;        /* 1 for prompt, 1 for overlap */
    int startpos = dwin->scrollpos;

    switch (arg)
    {
        case keycode_PageUp:
            dwin->scrollpos += pageht;
            break;
        case keycode_End:
            dwin->scrollpos = 0;
            break;
        case keycode_Up:
            dwin->scrollpos ++;
            break;
        case keycode_Down:
        case keycode_Return:
            dwin->scrollpos --;
            break;
        case keycode_MouseWheelUp:
            dwin->scrollpos += 3;
            startpos = TRUE;
            break;
        case keycode_MouseWheelDown:
            dwin->scrollpos -= 3;
            startpos = TRUE;
            break;
        case ' ':
        case keycode_PageDown:
        //default:
            if (pageht)
                dwin->scrollpos -= pageht;
            else
                dwin->scrollpos = 0;
            break;
    }

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1)
        dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
    if (dwin->scrollpos < 0)
        dwin->scrollpos = 0;
    touchscroll(dwin);

    return (startpos || dwin->scrollpos);
}

/* Any key, during character input. Ends character input. */
void gcmd_buffer_accept_readchar(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->data;
    glui32 key;

    if (dwin->height < 2)
        dwin->scrollpos = 0;

    if (dwin->scrollpos
            || arg == keycode_PageUp
            || arg == keycode_MouseWheelUp)
    {
        gcmd_accept_scroll(win, arg);
        return;
    }

    switch (arg)
    {
        case keycode_Erase:
            key = keycode_Delete;
            break;
        case keycode_MouseWheelUp:
        case keycode_MouseWheelDown:
            return;
        default:
            key = arg;
    }

#ifdef USETTS
    gli_speak_tts("", 0, 1);
#endif

    if (key > 0xff && key < (0xffffffff - keycode_MAXVAL + 1))
    {
        if (!(win->char_request_uni) || key > 0x10ffff)
            key = keycode_Unknown;
    }

    win->char_request = FALSE;
    win->char_request_uni = FALSE;
    gli_event_store(evtype_CharInput, win, key, 0);
}

/* Return or enter, during line input. Ends line input. */
static void acceptline(window_t *win, glui32 keycode)
{
    int ix;
    int len, olen;
    void *inbuf;
    glui32 *s, *o;
    int inmax;
    gidispatch_rock_t inarrayrock;
    window_textbuffer_t *dwin = win->data;
    int unicode = win->line_request_uni;

    if (!dwin->inbuf)
        return;

    inbuf = dwin->inbuf;
    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;

    len = dwin->numchars - dwin->infence;
    if (win->echostr) 
        gli_stream_echo_line_uni(win->echostr, dwin->chars+dwin->infence, len);

#ifdef USETTS
    gli_speak_tts(dwin->chars+dwin->infence, len, 1);
#endif

    /*
     * Store in history.
     * The history is a ring buffer, with historypresent being the index of the most recent
     * element and historyfirst the index of the oldest element.
     * A history entry should not repeat the string from the entry before it.
     */
    if (len)
    {
        s = malloc((len + 1) * sizeof(glui32));
        memcpy(s, dwin->chars + dwin->infence, len * sizeof(glui32));
        s[len] = 0;

        free(dwin->history[dwin->historypresent]);
        dwin->history[dwin->historypresent] = NULL;

        o = dwin->history[(dwin->historypresent == 0 ? HISTORYLEN : dwin->historypresent) - 1];
        olen = o ? strlen_uni(o) : 0;

        if (len != olen || memcmp(s, o, olen * sizeof(glui32)))
        {
            dwin->history[dwin->historypresent] = s;

            dwin->historypresent++;
            if (dwin->historypresent == HISTORYLEN)
                dwin->historypresent = 0;

            if (dwin->historypresent == dwin->historyfirst)
            {
                dwin->historyfirst++;
                if (dwin->historyfirst == HISTORYLEN)
                    dwin->historyfirst = 0;
            }
        }
        else
        {
            free(s);
        }
    }

    /* Store in event buffer. */

    if (len > inmax)
        len = inmax;

    if (!unicode)
    {
        for (ix=0; ix<len; ix++)
        {
            glui32 ch = dwin->chars[dwin->infence+ix];
            if (ch > 0xff)
                ch = '?';
            ((char *)inbuf)[ix] = (char)ch;
        }
    }
    else
    {
        for (ix=0; ix<len; ix++)
            ((glui32 *)inbuf)[ix] = dwin->chars[dwin->infence+ix];
    }

    win->attr = dwin->origattr;

    if (dwin->line_terminators)
    {
        glui32 val2 = keycode;
        if (val2 == keycode_Return)
            val2 = 0;
        gli_event_store(evtype_LineInput, win, len, val2);
        free(dwin->line_terminators);
        dwin->line_terminators = NULL;
    }
    else
    {
        gli_event_store(evtype_LineInput, win, len, 0);
    }
    win->line_request = FALSE;
    win->line_request_uni = FALSE;
    dwin->inbuf = NULL;
    dwin->inmax = 0;

    if (dwin->echo_line_input)
    {
        win_textbuffer_putchar_uni(win, '\n');
    }
    else
    {
        dwin->numchars = dwin->infence;
        touch(dwin, 0);
    }

    if (gli_unregister_arr)
        (*gli_unregister_arr)(inbuf, inmax, unicode ? "&+#!Iu" : "&+#!Cn", inarrayrock);
}

/* Any key, during line input. */
void gcmd_buffer_accept_readline(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->data;
    glui32 *cx;
    int len;

    if (dwin->height < 2)
        dwin->scrollpos = 0;

    if (dwin->scrollpos
        || arg == keycode_PageUp
        || arg == keycode_MouseWheelUp)
    {
        gcmd_accept_scroll(win, arg);
        return;
    }

    if (!dwin->inbuf)
        return;

    if (dwin->line_terminators && gli_window_check_terminator(arg))
    {
        for (cx = dwin->line_terminators; *cx; cx++)
        {
            if (*cx == arg)
            {
                acceptline(win, arg);
                return;
            }
        }
    }

    switch (arg)
    {

        /* History keys (up and down) */

        case keycode_Up:
            if (dwin->historypos == dwin->historyfirst)
                return;
            if (dwin->historypos == dwin->historypresent)
            {
                len = dwin->numchars - dwin->infence;
                if (len > 0)
                {
                    cx = malloc((len + 1) * 4);
                    memcpy(cx, &(dwin->chars[dwin->infence]), len * 4);
                    cx[len] = 0;
                }
                else
                {
                    cx = NULL;
                }
                if (dwin->history[dwin->historypos])
                    free(dwin->history[dwin->historypos]);
                dwin->history[dwin->historypos] = cx;
            }
            dwin->historypos--;
            if (dwin->historypos < 0)
                dwin->historypos += HISTORYLEN;
            cx = dwin->history[dwin->historypos];
            put_text_uni(dwin, cx, cx ? strlen_uni(cx) : 0, dwin->infence,
                    dwin->numchars - dwin->infence);
            break;

        case keycode_Down:
            if (dwin->historypos == dwin->historypresent)
                return;
            dwin->historypos++;
            if (dwin->historypos >= HISTORYLEN)
                dwin->historypos -= HISTORYLEN;
            cx = dwin->history[dwin->historypos];
            put_text_uni(dwin, cx, cx ? strlen_uni(cx) : 0, dwin->infence, 
                    dwin->numchars - dwin->infence);
            break;

            /* Cursor movement keys, during line input. */

        case keycode_Left:
            if (dwin->incurs <= dwin->infence)
                return;
            dwin->incurs--;
            break;

        case keycode_Right:
            if (dwin->incurs >= dwin->numchars)
                return;
            dwin->incurs++;
            break;

        case keycode_Home:
            if (dwin->incurs <= dwin->infence)
                return;
            dwin->incurs = dwin->infence;
            break;

        case keycode_End:
            if (dwin->incurs >= dwin->numchars)
                return;
            dwin->incurs = dwin->numchars;
            break;

            /* Delete keys, during line input. */

        case keycode_Delete:
            if (dwin->incurs <= dwin->infence)
                return;
            put_text_uni(dwin, NULL, 0, dwin->incurs-1, 1);
            break;

        case keycode_Erase:
            if (dwin->incurs >= dwin->numchars)
                return;
            put_text_uni(dwin, NULL, 0, dwin->incurs, 1);
            break;

        case keycode_Escape:
            if (dwin->infence >= dwin->numchars)
                return;
            put_text_uni(dwin, NULL, 0, dwin->infence, dwin->numchars - dwin->infence);
            break;

            /* Regular keys */

        case keycode_Return:
            acceptline(win, arg);
            break;

        default:
            if (arg >= 32 && arg <= 0x10FFFF)
            {
                if (gli_conf_caps && (arg > 0x60 && arg < 0x7b))
                    arg -= 0x20;
                put_text_uni(dwin, &arg, 1, dwin->incurs, 0);
            }
            break;
    }

    touch(dwin, 0);
}

static glui32
put_picture(window_textbuffer_t *dwin, picture_t *pic, glui32 align, glui32 linkval)
{
    if (align == imagealign_MarginRight)
    {
        if (dwin->lines[0].rpic || dwin->numchars)
            return FALSE;

        dwin->radjw = (pic->w + gli_tmarginx) * GLI_SUBPIX;
        dwin->radjn = (pic->h + gli_cellh - 1) / gli_cellh;
        dwin->lines[0].rpic = pic;
        dwin->lines[0].rm = dwin->radjw;
        dwin->lines[0].rhyper = linkval;
    }

    else
    {
        if (align != imagealign_MarginLeft && dwin->numchars)
            win_textbuffer_putchar_uni(dwin->owner, '\n');

        if (dwin->lines[0].lpic || dwin->numchars)
            return FALSE;

        dwin->ladjw = (pic->w + gli_tmarginx) * GLI_SUBPIX;
        dwin->ladjn = (pic->h + gli_cellh - 1) / gli_cellh;
        dwin->lines[0].lpic = pic;
        dwin->lines[0].lm = dwin->ladjw;
        dwin->lines[0].lhyper = linkval;

        if (align != imagealign_MarginLeft)
            win_textbuffer_flow_break(dwin);
    }

    return TRUE;
}

glui32 win_textbuffer_draw_picture(window_textbuffer_t *dwin,
    glui32 image, glui32 align, glui32 scaled, glui32 width, glui32 height)
{
    picture_t *pic;
    glui32 hyperlink;
    int error;

    pic = gli_picture_load(image);

    if (!pic)
        return FALSE;

    if (!dwin->owner->image_loaded)
    {
        gli_piclist_increment();
        dwin->owner->image_loaded = TRUE;
    }

    if (scaled)
    {
        picture_t *tmp;
        tmp = gli_picture_scale(pic, width, height);
        pic = tmp;
    }

    hyperlink = dwin->owner->attr.hyper;

    gli_picture_increment(pic);
    error = put_picture(dwin, pic, align, hyperlink);

    return error;
}

glui32 win_textbuffer_flow_break(window_textbuffer_t *dwin)
{
    while (dwin->ladjn || dwin->radjn)
        win_textbuffer_putchar_uni(dwin->owner, '\n');
    return TRUE;
}

void win_textbuffer_click(window_textbuffer_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;
    int gh = FALSE;
    int gs = FALSE;

    if (win->line_request || win->char_request
        || win->line_request_uni || win->char_request_uni
        || win->more_request || win->scroll_request)
        gli_focuswin = win;

    if (win->hyper_request)
    {
        glui32 linkval = gli_get_hyperlink(sx, sy);
        if (linkval)
        {
            gli_event_store(evtype_Hyperlink, win, linkval, 0);
            win->hyper_request = FALSE;
            if (gli_conf_safeclicks)
                gli_forceclick = 1;
            gh = TRUE;
        }
    }

    if (sx > win->bbox.x1 - gli_scroll_width)
    {
        if (sy < win->bbox.y0 + gli_tmarginy + gli_scroll_width)
            gcmd_accept_scroll(win, keycode_Up);
        else if (sy > win->bbox.y1 - gli_tmarginy - gli_scroll_width)
            gcmd_accept_scroll(win, keycode_Down);
        else if (sy < (win->bbox.y0 + win->bbox.y1) / 2)
            gcmd_accept_scroll(win, keycode_PageUp);
        else
            gcmd_accept_scroll(win, keycode_PageDown);
        gs = TRUE;
    }

    if (!gh && !gs)
    {
        gli_copyselect = TRUE;
        gli_start_selection(sx, sy);
    }
}

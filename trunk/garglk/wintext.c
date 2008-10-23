#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)

/* how many pixels we add to left/right margins */
#define SLOP (2 * GLI_SUBPIX)

static void put_text(window_textbuffer_t *dwin, char *buf, int len, int pos, int oldlen);
static void put_text_uni(window_textbuffer_t *dwin, glui32 *buf, int len, int pos, int oldlen);
static glui32 put_picture(window_textbuffer_t *dwin, picture_t *pic, glui32 align);

static void touch(window_textbuffer_t *dwin, int line)
{
    window_t *win = dwin->owner;
    int y = win->bbox.y0 + gli_tmarginy + (dwin->height - line - 1) * gli_leading;
//    if (dwin->scrollmax && dwin->scrollmax < dwin->height)
//        y -= (dwin->height - dwin->scrollmax) * gli_leading;
    dwin->lines[line].dirty = 1;
    winrepaint(win->bbox.x0, y - 2, win->bbox.x1, y + gli_leading + 2);
}

static void touchscroll(window_textbuffer_t *dwin)
{
    window_t *win = dwin->owner;
    int i;
    winrepaint(win->bbox.x0, win->bbox.y0, win->bbox.x1, win->bbox.y1);
    for (i = 0; i < dwin->scrollmax; i++)
        dwin->lines[i].dirty = 1;
}

window_textbuffer_t *win_textbuffer_create(window_t *win)
{
    window_textbuffer_t *dwin = malloc(sizeof(window_textbuffer_t));
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

    dwin->width = -1;
    dwin->height = -1;

    dwin->inbuf = NULL;
	dwin->uinbuf = NULL;

    dwin->ladjw = dwin->radjw = 0;
    dwin->ladjn = dwin->radjn = 0;

    dwin->numchars = 0;
    dwin->chars = dwin->lines[0].chars;
    dwin->attrs = dwin->lines[0].attrs;

    dwin->spaced = 0;
    dwin->dashed = 0;

    for (i = 0; i < SCROLLBACK; i++)
    {
        dwin->lines[i].dirty = 0;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].lpic = 0;
        dwin->lines[i].rpic = 0;
        dwin->lines[i].len = 0;
        dwin->lines[i].newline = 0;
        memset(dwin->lines[i].chars, ' ', TBLINELEN);
        memset(dwin->lines[i].attrs, style_Normal, TBLINELEN);
    }

    memcpy(dwin->styles, gli_tstyles, sizeof gli_tstyles);

    return dwin;
}

void win_textbuffer_destroy(window_textbuffer_t *dwin)
{
	if (dwin->inbuf && gli_unregister_arr)
		(*gli_unregister_arr)(dwin->inbuf, dwin->inmax, "&+#!Cn", dwin->inarrayrock);
	
	if (dwin->uinbuf && gli_unregister_arr)
		(*gli_unregister_arr)(dwin->uinbuf, dwin->inmax, "&+#!Iu", dwin->inarrayrock);

	dwin->inbuf = NULL;
	dwin->uinbuf = NULL;
    dwin->owner = NULL;

    free(dwin);
}

static void reflow(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    unsigned char attrbuf[TBLINELEN*SCROLLBACK];
    unsigned char charbuf[TBLINELEN*SCROLLBACK];
    int alignbuf[SCROLLBACK];
    picture_t *pictbuf[SCROLLBACK];
    int offsetbuf[SCROLLBACK];
    int inputbyte = -1;
    int curattr = -1;
    int oldstyle;
    int i, k, p;
    int x;

    if (dwin->height < 4 || dwin->width < 20)
        return;

    dwin->lines[0].len = dwin->numchars;

    /* copy text to temp buffers */

    oldstyle = win->style;

    x = 0;
    p = 0;
    for (k = dwin->scrollmax; k >= 0; k--)
    {
        if (k == 0 && win->line_request)
            inputbyte = p + dwin->infence;

        if (dwin->lines[k].lpic)
        {
            gli_picture_keep(dwin->lines[k].lpic);
            offsetbuf[x] = p;
            alignbuf[x] = imagealign_MarginLeft;
            pictbuf[x] = dwin->lines[k].lpic;
            x++;
        }

        if (dwin->lines[k].rpic)
        {
            gli_picture_keep(dwin->lines[k].rpic);
            offsetbuf[x] = p;
            alignbuf[x] = imagealign_MarginRight;
            pictbuf[x] = dwin->lines[k].rpic;
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
        win->style = attrbuf[i];

        if (offsetbuf[x] == i)
        {
            put_picture(dwin, pictbuf[x], alignbuf[x]);
            gli_picture_drop(pictbuf[x]);
            x ++;
        }

		win_textbuffer_putchar(win, charbuf[i]);
    }

    /* terribly sorry about this... */
    dwin->lastseen = 0;
    dwin->scrollpos = 0;

    if (inputbyte != -1)
    {
        dwin->infence = dwin->numchars;
        put_text(dwin, charbuf + inputbyte, p - inputbyte, dwin->numchars, 0);
        dwin->incurs = dwin->numchars;
    }

    win->style = oldstyle;

    touchscroll(dwin);
}

void win_textbuffer_rearrange(window_t *win, rect_t *box)
{
    window_textbuffer_t *dwin = win->data;
    int newwid, newhgt;
    int rnd;

    dwin->owner->bbox = *box;

    newwid = (box->x1 - box->x0 - gli_tmarginx * 2 - gli_scroll_width) / gli_cellw;
    newhgt = (box->y1 - box->y0 - gli_tmarginy * 2) / gli_cellh;

    /* align text with bottom */
    rnd = newhgt * gli_cellh + gli_tmarginy * 2;
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
    }
}

static int calcwidth(window_textbuffer_t *dwin,
        unsigned char *chars, unsigned char *attrs,
        int numchars, int spw)
{
    int w = 0;
    int a, b;

    a = 0;
    for (b = 0; b < numchars; b++)
    {
        if (attrs[a] != attrs[b])
        {
            w += gli_string_width(dwin->styles[attrs[a]].font,
                    chars + a, b - a, spw);
            a = b;
        }
    }

    w += gli_string_width(dwin->styles[attrs[a]].font,
            chars + a, b - a, spw);

    return w;
}

void win_textbuffer_redraw(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    int drawmore = win->line_request || win->char_request;
    tbline_t *ln;
    int linelen;
    int nsp, spw;
    int x0, y0, x1, y1;
    int x, y, w;
    int a, b;
    int i;

    dwin->lines[0].len = dwin->numchars;

    x0 = (win->bbox.x0 + gli_tmarginx) * GLI_SUBPIX;
    x1 = (win->bbox.x1 - gli_tmarginx - gli_scroll_width) * GLI_SUBPIX;
    y0 = win->bbox.y0 + gli_tmarginy;

//    if (dwin->scrollmax && dwin->scrollmax < dwin->height)
//        y0 -= (dwin->height - dwin->scrollmax) * gli_leading;

    for (i = dwin->scrollpos; i < dwin->height + dwin->scrollpos; i++)
    {
        ln = dwin->lines + i;

        /* skip if we can */
        if (!ln->dirty && !gli_force_redraw && dwin->scrollpos == 0)
            continue;

        ln->dirty = 0;

        /* leave bottom line blank for [more] prompt */
        if (drawmore && i == dwin->scrollpos && i > 0)
            continue;

        /* kill spaces at the end */
        linelen = ln->len;
        while (i > 0 && linelen > 1 && ln->chars[linelen-1] == ' ')
            linelen --;

        /* top of line */
        y = y0 + (dwin->height - (i - dwin->scrollpos) - 1) * gli_leading;

        /*
         * count spaces and width for justification
         */
        if (gli_conf_justify && !ln->newline && i > 0)
        {
            for (a = 0, nsp = 0; a < linelen; a++)
                if (ln->chars[a] == ' ')
                    nsp ++;
            w = calcwidth(dwin, ln->chars, ln->attrs, linelen, 0);
            if (nsp)
                spw = (x1 - x0 - ln->lm - ln->rm - 2 * SLOP - w) / nsp;
            else
                spw = 0;
        }
        else
        {
            spw = -1;
        }

        /*
         * fill in background colors
         */

        gli_draw_rect(x0/GLI_SUBPIX, y,
                (x1-x0) / GLI_SUBPIX, gli_leading,
                gli_window_color);

        x = x0 + SLOP + ln->lm;
        a = 0;
        for (b = 0; b < linelen; b++)
        {
            if (ln->attrs[a] != ln->attrs[b])
            {
                w = gli_string_width(dwin->styles[ln->attrs[a]].font,
                        ln->chars + a, b - a, spw);
                gli_draw_rect(x/GLI_SUBPIX, y,
                        w/GLI_SUBPIX, gli_leading,
                        dwin->styles[ln->attrs[a]].bg);
                x += w;
                a = b;
            }
        }

        w = gli_string_width(dwin->styles[ln->attrs[a]].font,
                ln->chars + a, b - a, spw);
        gli_draw_rect(x/GLI_SUBPIX, y,
                w/GLI_SUBPIX, gli_leading,
                dwin->styles[ln->attrs[a]].bg);
        x += w;

        gli_draw_rect(x/GLI_SUBPIX, y,
                x1/GLI_SUBPIX - x/GLI_SUBPIX, gli_leading,
                gli_window_color);

        /*
         * draw caret
         */

        if (gli_focuswin == win && i == 0 && win->line_request)
        {
            w = calcwidth(dwin, dwin->chars, dwin->attrs, dwin->incurs, spw);
            gli_draw_caret(x0 + SLOP + ln->lm + w, y + gli_baseline);
        }

        /*
         * draw text
         */

        x = x0 + SLOP + ln->lm;
        a = 0;
        for (b = 0; b < linelen; b++)
        {
            if (ln->attrs[a] != ln->attrs[b])
            {
                x = gli_draw_string(x, y + gli_baseline,
                        dwin->styles[ln->attrs[a]].font,
                        dwin->styles[ln->attrs[a]].fg,
                        ln->chars + a, b - a, spw);
                a = b;
            }
        }
        gli_draw_string(x, y + gli_baseline,
                dwin->styles[ln->attrs[a]].font,
                dwin->styles[ln->attrs[a]].fg,
                ln->chars + a, linelen - a, spw);
    }

    /*
     * draw more prompt
     */
    if (drawmore && dwin->scrollpos && dwin->height > 1)
    {
        x = x0 + SLOP;
        y = y0 + (dwin->height - 1) * gli_leading;

        gli_draw_rect(x/GLI_SUBPIX, y,
                x1/GLI_SUBPIX - x/GLI_SUBPIX, gli_leading,
                gli_window_color);

        w = gli_string_width(gli_more_font,
                gli_more_prompt, strlen(gli_more_prompt), -1);

        if (gli_more_align == 1)    /* center */
            x = x0 + SLOP + (x1 - x0 - w - SLOP * 2) / 2;
        if (gli_more_align == 2)    /* right */
            x = x1 - SLOP - w;

        gli_draw_string(x, y + gli_baseline, 
                gli_more_font, gli_more_color,
                gli_more_prompt, strlen(gli_more_prompt), -1);
        y1 = y; /* don't want pictures overdrawing "[more]" */
    }
    else
        y1 = y0 + dwin->height * gli_leading;

    /*
     * draw the images
     */
    for (i = 0; i < SCROLLBACK; i++)
    {
        ln = dwin->lines + i;

        y = y0 + (dwin->height - (i - dwin->scrollpos) - 1) * gli_leading;

        if (ln->lpic)
        {
            if (y < y1 && y + ln->lpic->h > y0)
                gli_draw_picture(ln->lpic,
                        x0/GLI_SUBPIX, y,
                        x0/GLI_SUBPIX, y0, x1/GLI_SUBPIX, y1);
        }

        if (ln->rpic)
        {
            if (y < y1 && y + ln->rpic->h > y0)
                gli_draw_picture(ln->rpic,
                        x1/GLI_SUBPIX - ln->rpic->w, y,
                        x0/GLI_SUBPIX, y0, x1/GLI_SUBPIX, y1);
        }
    }

    /*
     * Draw the scrollbar
     */
    if (gli_scroll_width)
    {
        int t0, t1;
        x0 = win->bbox.x1 - gli_scroll_width;
        x1 = win->bbox.x1;
        y0 = win->bbox.y0 + gli_tmarginy;
        y1 = win->bbox.y1 - gli_tmarginy;

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
        else { t0 = t1 = y0; }

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
}

static void scrolloneline(window_textbuffer_t *dwin, int forced)
{
    int i;

    if (dwin->lastseen >= dwin->height - 1)
        dwin->scrollpos ++;
    dwin->lastseen ++;
    dwin->scrollmax ++;

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1)
        dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
    if (dwin->scrollpos < 0)
        dwin->scrollpos = 0;

    dwin->scrollmax = MIN(dwin->scrollmax, SCROLLBACK-1);
    dwin->lastseen = MIN(dwin->lastseen, SCROLLBACK-1);

    if (forced)
        dwin->dashed = 0;
    dwin->spaced = 0;

    dwin->lines[0].len = dwin->numchars;
    dwin->lines[0].newline = forced;

    if (dwin->lines[SCROLLBACK-1].lpic)
        gli_picture_drop(dwin->lines[SCROLLBACK-1].lpic);
    if (dwin->lines[SCROLLBACK-1].rpic)
        gli_picture_drop(dwin->lines[SCROLLBACK-1].rpic);

    for (i = SCROLLBACK - 2; i > 0; i--)
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
    memset(dwin->chars, ' ', TBLINELEN);
    memset(dwin->attrs, style_Normal, TBLINELEN);

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
                dwin->numchars - (pos + oldlen));
        memmove(dwin->attrs + pos + len,
                dwin->attrs + pos + oldlen,
                dwin->numchars - (pos + oldlen));
    }
    if (len > 0)
    {
        memmove(dwin->chars + pos, buf, len);
        memset(dwin->attrs + pos, style_Input, len);
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

#ifdef GLK_MODULE_UNICODE

static void put_text_uni(window_textbuffer_t *dwin, glui32 *buf, int len, int pos, int oldlen)
{
    int diff = len - oldlen;

    if (dwin->numchars + diff >= TBLINELEN)
        return;

    if (diff != 0 && pos + oldlen < dwin->numchars)
    {
        memmove(dwin->chars + pos + len,
                dwin->chars + pos + oldlen,
                dwin->numchars - (pos + oldlen));
        memmove(dwin->attrs + pos + len,
                dwin->attrs + pos + oldlen,
                dwin->numchars - (pos + oldlen));
    }
    if (len > 0)
    {
        memmove(dwin->chars + pos, buf, len);
        memset(dwin->attrs + pos, style_Input, len);
    }
    dwin->numchars += diff;

    if (dwin->uinbuf)
    {
        if (dwin->incurs >= pos + oldlen)
            dwin->incurs += diff;
        else if (dwin->incurs >= pos)
            dwin->incurs = pos + len;
    }

    touch(dwin, 0);
}

#endif /* GLK_MODULE_UNICODE */

void win_textbuffer_putchar(window_t *win, char ch)
{
    window_textbuffer_t *dwin = win->data;
    unsigned char bchars[TBLINELEN];
    unsigned char battrs[TBLINELEN];
    int pw;
    int bpoint;
    int saved;
    int i;
    int linelen;

#ifdef USETTS
    { char b[1]; b[0] = ch; gli_speak_tts(b, 1, 0); }
#endif

    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2 - gli_scroll_width) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw - dwin->ladjw;

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
        /* fails for 'tis a wonderful day in the '80s */
        if (gli_conf_quotes > 1 && ch == '\'')
        {
            if (dwin->numchars == 0 || dwin->chars[dwin->numchars-1] == ' ')
                ch = UNI_LSQUO;
        }

        if (ch == '`')
            ch = UNI_LSQUO;

        if (ch == '\'')
            ch = UNI_RSQUO;

        if (ch == '"')
        {
            if (dwin->numchars == 0 || dwin->chars[dwin->numchars-1] == ' ')
                ch = UNI_LDQUO;
            else
                ch = UNI_RDQUO;
        }
    }

    if (gli_conf_quotes && win->style != style_Preformatted)
    {
        if (ch == '-')
        {
            dwin->dashed ++;
            if (dwin->dashed == 2)
            {
                dwin->numchars--;
                ch = UNI_NDASH;
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

    if (gli_conf_spaces && win->style != style_Preformatted)
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
                win_textbuffer_putchar(win, ' ');
            }
            else
                dwin->spaced = 0;
        }
    }

    dwin->chars[dwin->numchars] = ch;
    dwin->attrs[dwin->numchars] = win->style;
    dwin->numchars++;

    /* kill spaces at the end for line width calculation */
    linelen = dwin->numchars;
    while (linelen > 1 && dwin->chars[linelen-1] == ' ')
        linelen --;

    if (calcwidth(dwin, dwin->chars, dwin->attrs, linelen, -1) >= pw)
    {
        bpoint = dwin->numchars;

        for (i = dwin->numchars - 1; i > 0; i--)
            if (dwin->chars[i] == ' ')
            {
                bpoint = i + 1; /* skip space */
                break;
            }

        saved = dwin->numchars - bpoint;

        memcpy(bchars, dwin->chars + bpoint, saved);
        memcpy(battrs, dwin->attrs + bpoint, saved);
        dwin->numchars = bpoint;

        scrolloneline(dwin, 0);

        memcpy(dwin->chars, bchars, saved);
        memcpy(dwin->attrs, battrs, saved);
        dwin->numchars = saved;
    }

    touch(dwin, 0);
}

void win_textbuffer_putchar_uni(window_t *win, glui32 chu)
{
    window_textbuffer_t *dwin = win->data;
    unsigned char bchars[TBLINELEN];
    unsigned char battrs[TBLINELEN];
    int pw;
    int bpoint;
    int saved;
    int i;
    int linelen;

	unsigned char ch;
    if (chu >= 0x100)
        ch = '?';
	else
		ch = (unsigned char)chu;

#ifdef USETTS
    { char b[1]; b[0] = ch; gli_speak_tts(b, 1, 0); }
#endif

    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2 - gli_scroll_width) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw - dwin->ladjw;

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
        /* fails for 'tis a wonderful day in the '80s */
        if (gli_conf_quotes > 1 && ch == '\'')
        {
            if (dwin->numchars == 0 || dwin->chars[dwin->numchars-1] == ' ')
                ch = UNI_LSQUO;
        }

        if (ch == '`')
            ch = UNI_LSQUO;

        if (ch == '\'')
            ch = UNI_RSQUO;

        if (ch == '"')
        {
            if (dwin->numchars == 0 || dwin->chars[dwin->numchars-1] == ' ')
                ch = UNI_LDQUO;
            else
                ch = UNI_RDQUO;
        }
    }

    if (gli_conf_quotes && win->style != style_Preformatted)
    {
        if (ch == '-')
        {
            dwin->dashed ++;
            if (dwin->dashed == 2)
            {
                dwin->numchars--;
                ch = UNI_NDASH;
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

    if (gli_conf_spaces && win->style != style_Preformatted)
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
                win_textbuffer_putchar(win, ' ');
            }
            else
                dwin->spaced = 0;
        }
    }

    dwin->chars[dwin->numchars] = ch;
    dwin->attrs[dwin->numchars] = win->style;
    dwin->numchars++;

    /* kill spaces at the end for line width calculation */
    linelen = dwin->numchars;
    while (linelen > 1 && dwin->chars[linelen-1] == ' ')
        linelen --;

    if (calcwidth(dwin, dwin->chars, dwin->attrs, linelen, -1) >= pw)
    {
        bpoint = dwin->numchars;

        for (i = dwin->numchars - 1; i > 0; i--)
            if (dwin->chars[i] == ' ')
            {
                bpoint = i + 1; /* skip space */
                break;
            }

        saved = dwin->numchars - bpoint;

        memcpy(bchars, dwin->chars + bpoint, saved);
        memcpy(battrs, dwin->attrs + bpoint, saved);
        dwin->numchars = bpoint;

        scrolloneline(dwin, 0);

        memcpy(dwin->chars, bchars, saved);
        memcpy(dwin->attrs, battrs, saved);
        dwin->numchars = saved;
    }

    touch(dwin, 0);
}

void win_textbuffer_clear(window_t *win)
{
    window_textbuffer_t *dwin = win->data;
    int i;

    dwin->ladjw = dwin->radjw = 0;
    dwin->ladjn = dwin->radjn = 0;

    dwin->spaced = 0;
    dwin->dashed = 0;

    dwin->numchars = 0;

    for (i = 0; i < SCROLLBACK; i++)
    {
        if (dwin->lines[i].lpic)
            gli_picture_drop(dwin->lines[i].lpic);
        if (dwin->lines[i].rpic)
            gli_picture_drop(dwin->lines[i].rpic);
        dwin->lines[i].len = 0;
        dwin->lines[i].lpic = 0;
        dwin->lines[i].rpic = 0;
        dwin->lines[i].lm = 0;
        dwin->lines[i].rm = 0;
        dwin->lines[i].newline = 0;
        dwin->lines[i].dirty = 1;
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
		win_textbuffer_putchar(win, ' ');

	if (dwin->numchars && dwin->chars[dwin->numchars-1] == '?')
		win_textbuffer_putchar(win, ' ');

    /* make sure we have some space left for typing... */
    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw + dwin->ladjw;
	if (calcwidth(dwin, dwin->chars, dwin->attrs, dwin->numchars, -1) >= pw * 3 / 4)
		win_textbuffer_putchar(win, '\n');

    dwin->lastseen = 0;

    dwin->inbuf = buf;
    dwin->inmax = maxlen;
    dwin->infence = dwin->numchars;
    dwin->incurs = dwin->numchars;
    dwin->origstyle = win->style;
    win->style = style_Input;

    dwin->historypos = dwin->historypresent;

    if (initlen) {
        touch(dwin, 0);
        put_text(dwin, buf, initlen, dwin->incurs, 0);
    }

    if (gli_register_arr) {
        dwin->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
    }
}

#ifdef GLK_MODULE_UNICODE

void win_textbuffer_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen)
{
    window_textbuffer_t *dwin = win->data;
    int pw;

#ifdef USETTS
    gli_speak_tts("\n", 1, 0);
#endif

    /* because '>' prompt is ugly without extra space */
	if (dwin->numchars && dwin->chars[dwin->numchars-1] == '>')
		win_textbuffer_putchar_uni(win, (glui32)' ');

	if (dwin->numchars && dwin->chars[dwin->numchars-1] == '?')
		win_textbuffer_putchar_uni(win, (glui32)' ');

    /* make sure we have some space left for typing... */
    pw = (win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2) * GLI_SUBPIX;
    pw = pw - 2 * SLOP - dwin->radjw + dwin->ladjw;
	if (calcwidth(dwin, dwin->chars, dwin->attrs, dwin->numchars, -1) >= pw * 3 / 4)
		win_textbuffer_putchar_uni(win, (glui32)'\n');

    dwin->lastseen = 0;

    dwin->uinbuf = buf;
    dwin->inmax = maxlen;
    dwin->infence = dwin->numchars;
    dwin->incurs = dwin->numchars;
    dwin->origstyle = win->style;
    win->style = style_Input;

    dwin->historypos = dwin->historypresent;

    if (initlen) {
        touch(dwin, 0);
        put_text_uni(dwin, buf, initlen, dwin->incurs, 0);
    }

    if (gli_register_arr) {
        dwin->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Iu");
    }
}

#endif /* GLK_MODULE_UNICODE */

/* Abort line input, storing whatever's been typed so far. */
void win_textbuffer_cancel_line(window_t *win, event_t *ev)
{
    window_textbuffer_t *dwin = win->data;
    gidispatch_rock_t inarrayrock;
    int ix;
    int len;
    char *inbuf;
	char *linebuf;
	glui32 *uinbuf;
	glui32 *ulinebuf;
    int inmax;

    if (!dwin->inbuf && !dwin->uinbuf)
        return;

	if (!win->line_request_uni)
		inbuf = dwin->inbuf;
	else
		uinbuf = dwin->uinbuf;

    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;

    len = dwin->numchars - dwin->infence;

	linebuf = malloc(len);
	memcpy(linebuf, dwin->chars + dwin->infence, len);

	if (win->line_request_uni) {
		ulinebuf = malloc(len * sizeof(glui32));
		for (ix=0; ix<len; ix++)
			ulinebuf[ix] = (glui32)(((unsigned char *)linebuf)[ix]);
	}

	if (!win->line_request_uni) {
		if (win->echostr)
			gli_stream_echo_line(win->echostr, linebuf, len);

		if (len > inmax)
			len = inmax;

		for (ix=0; ix<len; ix++)
			inbuf[ix] = linebuf[ix];
		free(linebuf);
	}
	else {
		if (win->echostr)
			gli_stream_echo_line_uni(win->echostr, ulinebuf, len);

		if (len > inmax)
			len = inmax;

		for (ix=0; ix<len; ix++)
			uinbuf[ix] = ulinebuf[ix];
		free(linebuf);
		free(ulinebuf);
	}

    win->style = dwin->origstyle;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = len;

	if (gli_unregister_arr) {
		if (!win->line_request_uni)
			(*gli_unregister_arr)(inbuf, inmax, "&+#!Cn", inarrayrock);
		else
			(*gli_unregister_arr)(uinbuf, inmax, "&+#!Iu", inarrayrock);
	}

    win->line_request = FALSE;
	win->line_request_uni = FALSE;
    dwin->inbuf = NULL;
	dwin->uinbuf = NULL;
    dwin->inmax = 0;

	win_textbuffer_putchar(win, '\n');

}

/* Keybinding functions. */

/* Any key, when text buffer is scrolled. */
static void gcmd_accept_scroll(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->data;
    int oldpos = dwin->scrollpos;
    int pageht = dwin->height - 2;        /* 1 for prompt, 1 for overlap */
    int i;

    switch (arg)
    {
        case keycode_PageUp:
            dwin->scrollpos += pageht;
            break;
        case ' ':
        case keycode_PageDown:
            dwin->scrollpos -= pageht;
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
    }

    if (dwin->scrollpos > dwin->scrollmax - dwin->height + 1)
        dwin->scrollpos = dwin->scrollmax - dwin->height + 1;
    if (dwin->scrollpos < 0)
        dwin->scrollpos = 0;
    touchscroll(dwin);
}

/* Any key, during character input. Ends character input. */
void gcmd_buffer_accept_readchar(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->data;

    if (dwin->height < 2)
        dwin->scrollpos = 0;

    if (dwin->scrollpos || arg == keycode_PageUp)
    {
        gcmd_accept_scroll(win, arg);
        return;
    }

#ifdef USETTS
    gli_speak_tts("", 0, 1);
#endif

    dwin->lastseen = 0;
    win->char_request = FALSE; 
	win->char_request_uni = FALSE;
    gli_event_store(evtype_CharInput, win, arg, 0);
}

/* Return or enter, during line input. Ends line input. */
static void acceptline(window_t *win)
{
    int ix;
    int len;
    char *inbuf;
	char *linebuf;
	glui32 *uinbuf;
	glui32 *ulinebuf;
    char *s;
    int inmax;
    gidispatch_rock_t inarrayrock;
    window_textbuffer_t *dwin = win->data;

    if (!dwin->inbuf && !dwin->uinbuf)
        return;

	if (!win->line_request_uni)
		inbuf = dwin->inbuf;
	else
		uinbuf = dwin->uinbuf;

    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;

    len = dwin->numchars - dwin->infence;

	linebuf = malloc(len);
	memcpy(linebuf, dwin->chars + dwin->infence, len);

	if (win->line_request_uni) {
		ulinebuf = malloc(len * sizeof(glui32));
		for (ix=0; ix<len; ix++)
			ulinebuf[ix] = (glui32)(((unsigned char *)linebuf)[ix]);
	}

	if (win->echostr) {
		if (!win->line_request_uni)
			gli_stream_echo_line(win->echostr, linebuf, len);
		else
			gli_stream_echo_line_uni(win->echostr, ulinebuf, len);
	}

#ifdef USETTS
    gli_speak_tts(dwin->chars+dwin->infence, len, 1);
#endif

    /* Store in history. */
    if (len)
    {
        s = malloc(len + 1);
        memcpy(s, dwin->chars + dwin->infence, len);
        s[len] = '\0';

        if (dwin->history[dwin->historypresent])
        {
            free(dwin->history[dwin->historypresent]);
            dwin->history[dwin->historypresent] = NULL;
        }
        dwin->history[dwin->historypresent] = s;

        dwin->historypresent ++;
        if (dwin->historypresent >= HISTORYLEN)
            dwin->historypresent -= HISTORYLEN;

        if (dwin->historypresent == dwin->historyfirst)
        {
            dwin->historyfirst++;
            if (dwin->historyfirst >= HISTORYLEN)
                dwin->historyfirst -= HISTORYLEN;
        }

        if (dwin->history[dwin->historypresent])
        {
            free(dwin->history[dwin->historypresent]);
            dwin->history[dwin->historypresent] = NULL;
        }
    }

    /* Store in event buffer. */

    if (len > inmax)
        len = inmax;

	if (!win->line_request_uni) {
		for (ix=0; ix<len; ix++) {
			inbuf[ix] = linebuf[ix];
		}
		free(linebuf);
	}
	else {
		for (ix=0; ix<len; ix++) {
			uinbuf[ix] = ulinebuf[ix];
		}
		free(linebuf);
		free(ulinebuf);
	}

    win->style = dwin->origstyle;

    gli_event_store(evtype_LineInput, win, len, 0);

	if (gli_unregister_arr) {
		if (!win->line_request_uni)
			(*gli_unregister_arr)(inbuf, inmax, "&+#!Cn", inarrayrock);
		else
			(*gli_unregister_arr)(uinbuf, inmax, "&+#!Iu", inarrayrock);
	}

    win->line_request = FALSE;
	win->line_request_uni = FALSE;
    dwin->inbuf = NULL;
	dwin->uinbuf = NULL;
    dwin->inmax = 0;

	win_textbuffer_putchar(win, '\n');

}

/* Any key, during line input. */
void gcmd_buffer_accept_readline(window_t *win, glui32 arg)
{
    window_textbuffer_t *dwin = win->data;
    char ch = arg;

    char *cx;
    int len;

    if (dwin->height < 2)
        dwin->scrollpos = 0;

    if (dwin->scrollpos || arg == keycode_PageUp)
    {
        gcmd_accept_scroll(win, arg);
        return;
    }

    if (!dwin->inbuf && !dwin->uinbuf)
        return;

    switch (arg)
    {

        /* History keys (up and down) */

        case keycode_Up:
            if (dwin->historypos == dwin->historyfirst)
                return;
            if (dwin->historypos == dwin->historypresent) {
                len = dwin->numchars - dwin->infence;
                if (len > 0) {
                    cx = malloc(len + 1);
                    memcpy(cx, &(dwin->chars[dwin->infence]), len);
                    cx[len] = '\0';
                }
                else {
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
            if (!cx)
                cx = "";
			if (!win->line_request_uni)
				put_text(dwin, cx, strlen(cx), dwin->infence,
                    dwin->numchars - dwin->infence);
			else
				put_text_uni(dwin, (glui32 *)cx, strlen(cx), dwin->infence,
                    dwin->numchars - dwin->infence);
            break;

        case keycode_Down:
            if (dwin->historypos == dwin->historypresent)
                return;
            dwin->historypos++;
            if (dwin->historypos >= HISTORYLEN)
                dwin->historypos -= HISTORYLEN;
            cx = dwin->history[dwin->historypos];
            if (!cx)
                cx = "";
			if (!win->line_request_uni)
				put_text(dwin, cx, strlen(cx), dwin->infence, 
						dwin->numchars - dwin->infence);
			else
				put_text_uni(dwin, (glui32 *)cx, strlen(cx), dwin->infence, 
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
			if (!win->line_request_uni)
				put_text(dwin, "", 0, dwin->incurs-1, 1);
			else
				put_text_uni(dwin, (glui32 *)"", 0, dwin->incurs-1, 1);
            break;

        case keycode_Escape:
            if (dwin->infence >= dwin->numchars)
                return;
			if (!win->line_request_uni)
				put_text(dwin, "", 0, dwin->infence, dwin->numchars - dwin->infence);
			else
				put_text_uni(dwin, (glui32 *)"", 0, dwin->infence, dwin->numchars - dwin->infence);
            break;

            /* Regular keys */

        case keycode_Return:
            acceptline(win);
            break;

        default:
            if (arg >= 32 && arg <= 255)
				if (!win->line_request_uni)
					put_text(dwin, &ch, 1, dwin->incurs, 0);
				else
					put_text_uni(dwin, &arg, 1, dwin->incurs, 0);
            break;
    }

    touch(dwin, 0);
}

static glui32 put_picture(window_textbuffer_t *dwin, picture_t *pic, glui32 align)
{
    if (align == imagealign_MarginRight)
    {
        if (dwin->lines[0].rpic || dwin->numchars)
            return FALSE;

        dwin->radjw = (pic->w + gli_tmarginx) * GLI_SUBPIX;
        dwin->radjn = (pic->h + gli_cellh - 1) / gli_cellh;
        dwin->lines[0].rpic = pic;
        dwin->lines[0].rm = dwin->radjw;
    }

    else
    {
		if (align != imagealign_MarginLeft && dwin->numchars)
			win_textbuffer_putchar(dwin->owner, '\n');

        if (dwin->lines[0].lpic || dwin->numchars)
            return FALSE;

        dwin->ladjw = (pic->w + gli_tmarginx) * GLI_SUBPIX;
        dwin->ladjn = (pic->h + gli_cellh - 1) / gli_cellh;
        dwin->lines[0].lpic = pic;
        dwin->lines[0].lm = dwin->ladjw;

        if (align != imagealign_MarginLeft)
            win_textbuffer_flow_break(dwin);
    }

    gli_picture_keep(pic);

    return TRUE;
}

glui32 win_textbuffer_draw_picture(window_textbuffer_t *dwin,
        glui32 image, glui32 align, glui32 scaled, glui32 width, glui32 height)
{
    picture_t *pic;
    int error;

    pic = gli_picture_load(image);

    if (!pic)
        return FALSE;

    if (scaled)
    {
        picture_t *tmp;
        tmp = gli_picture_scale(pic, width, height);
        gli_picture_drop(pic);
        pic = tmp;
    }

    error = put_picture(dwin, pic, align);
    
    gli_picture_drop(pic);

    return error;
}

glui32 win_textbuffer_flow_break(window_textbuffer_t *dwin)
{
	while (dwin->ladjn || dwin->radjn) {
		win_textbuffer_putchar(dwin->owner, '\n');
	}
    return TRUE;
}

void win_textbuffer_click(window_textbuffer_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;
    if (win->line_request || win->char_request)
        gli_focuswin = win;
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
    }
}


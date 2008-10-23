#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

/* A grid of characters. We store the window as a list of lines.
 * Within a line, just store an array of characters and an array
 * of style bytes, the same size.
 */

static void touch(window_textgrid_t *dwin, int line)
{
    window_t *win = dwin->owner;
    int y = win->bbox.y0 + line * gli_leading;
    dwin->lines[line].dirty = 1;
    winrepaint(win->bbox.x0, y, win->bbox.x1, y + gli_leading);
}

window_textgrid_t *win_textgrid_create(window_t *win)
{
    window_textgrid_t *dwin = malloc(sizeof(window_textgrid_t));
    dwin->owner = win;

    dwin->width = 0;
    dwin->height = 0;

    dwin->curx = 0;
    dwin->cury = 0;

    dwin->inbuf = NULL;
	dwin->uinbuf = NULL;
    dwin->inorgx = 0;
    dwin->inorgy = 0;

    memcpy(dwin->styles, gli_gstyles, sizeof gli_gstyles);

    return dwin;
}

void win_textgrid_destroy(window_textgrid_t *dwin)
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

void win_textgrid_rearrange(window_t *win, rect_t *box)
{
    int newwid, newhgt;
    window_textgrid_t *dwin = win->data;
    dwin->owner->bbox = *box;
    int k, i;

    newwid = (box->x1 - box->x0) / gli_cellw;
    newhgt = (box->y1 - box->y0) / gli_cellh;

    if (newwid == dwin->width && newhgt == dwin->height)
		return;

    for (k = dwin->height; k < newhgt; k++)
    {
		memset(dwin->lines[k].chars, ' ', sizeof dwin->lines[k].chars);
		memset(dwin->lines[k].attrs, style_Normal, sizeof dwin->lines[k].attrs);
    }

    dwin->width = newwid;
    dwin->height = newhgt;

    for (k = 0; k < dwin->height; k++)
    {
		touch(dwin, k);
		for (i = dwin->width; i < sizeof dwin->lines[0].chars; i++)
		{
			dwin->lines[k].chars[i] = ' ';
			dwin->lines[k].attrs[i] = style_Normal;
		}
    }
}

void win_textgrid_redraw(window_t *win)
{
    window_textgrid_t *dwin = win->data;
    tgline_t *ln;
    int x0, y0, x1, y1;
    int x, y;
    int i, k;

    x0 = win->bbox.x0;
    x1 = win->bbox.x1;
    y0 = win->bbox.y0;
    y1 = win->bbox.y1;

    for (i = 0; i < dwin->height; i++)
    {
	ln = dwin->lines + i;
	if (ln->dirty || gli_force_redraw)
	{
	    ln->dirty = 0;

	    x = x0;
	    y = y0 + i * gli_leading;

	    for (k = 0; k < dwin->width; k++)
	    {
		x = x0 + k * gli_cellw;
		gli_draw_rect(x, y, gli_cellw, gli_leading,
			dwin->styles[ln->attrs[k]].bg);
		gli_draw_string(x * GLI_SUBPIX, y + gli_baseline,
			dwin->styles[ln->attrs[k]].font,
			dwin->styles[ln->attrs[k]].fg,	
			ln->chars + k, 1, -1);
	    }
	}
    }
}

void win_textgrid_putchar(window_t *win, char ch)
{
    window_textgrid_t *dwin = win->data;
    tgline_t *ln;

    /* Canonicalize the cursor position. That is, the cursor may have been
       left outside the window area; wrap it if necessary. */
    if (dwin->curx < 0)
		dwin->curx = 0;
    else if (dwin->curx >= dwin->width) {
		dwin->curx = 0;
		dwin->cury++;
	}
    if (dwin->cury < 0)
		dwin->cury = 0;
    else if (dwin->cury >= dwin->height)
		return; /* outside the window */

    if (ch == '\n') {
		/* a newline just moves the cursor. */
		dwin->cury++;
		dwin->curx = 0;
		return;
    }

    touch(dwin, dwin->cury);

    ln = &(dwin->lines[dwin->cury]);
    ln->chars[dwin->curx] = ch;
    ln->attrs[dwin->curx] = win->style;

    dwin->curx++;
    /* We can leave the cursor outside the window, since it will be
       canonicalized next time a character is printed. */
}

void win_textgrid_putchar_uni(window_t *win, glui32 chu)
{
    window_textgrid_t *dwin = win->data;
    tgline_t *ln;

	unsigned char ch;
    if (chu >= 0x100)
        ch = '?';
	else
		ch = (unsigned char)chu;

    /* Canonicalize the cursor position. That is, the cursor may have been
       left outside the window area; wrap it if necessary. */
    if (dwin->curx < 0)
		dwin->curx = 0;
    else if (dwin->curx >= dwin->width) {
		dwin->curx = 0;
		dwin->cury++;
	}
    if (dwin->cury < 0)
		dwin->cury = 0;
    else if (dwin->cury >= dwin->height)
		return; /* outside the window */

    if (ch == '\n') {
		/* a newline just moves the cursor. */
		dwin->cury++;
		dwin->curx = 0;
		return;
    }

    touch(dwin, dwin->cury);

    ln = &(dwin->lines[dwin->cury]);
    ln->chars[dwin->curx] = ch;
    ln->attrs[dwin->curx] = win->style;

    dwin->curx++;
    /* We can leave the cursor outside the window, since it will be
       canonicalized next time a character is printed. */
}

void win_textgrid_clear(window_t *win)
{
    window_textgrid_t *dwin = win->data;
    int k;

    for (k = 0; k < dwin->height; k++)
    {
		touch(dwin, k);
		memset(dwin->lines[k].chars, ' ', sizeof dwin->lines[k].chars);
		memset(dwin->lines[k].attrs, style_Normal, sizeof dwin->lines[k].attrs);
    }

    dwin->curx = 0;
    dwin->cury = 0;
}

void win_textgrid_move_cursor(window_t *win, int xpos, int ypos)
{
    window_textgrid_t *dwin = win->data;

    /* If the values are negative, they're really huge positive numbers -- 
       remember that they were cast from glui32. So set them huge and
       let canonicalization take its course. */
    if (xpos < 0)
		xpos = 32767;
    if (ypos < 0)
		ypos = 32767;

    dwin->curx = xpos;
    dwin->cury = ypos;
}

void win_textgrid_click(window_textgrid_t *dwin, int sx, int sy)
{
    window_t *win = dwin->owner;
    int x = sx - win->bbox.x0;
    int y = sy - win->bbox.y0;

    if (win->line_request || win->char_request)
		gli_focuswin = win;

    if (win->mouse_request)
		gli_event_store(evtype_MouseInput, win, x, y);
		win->mouse_request = FALSE;
}

/* Prepare the window for line input. */
void win_textgrid_init_line(window_t *win, char *buf, int maxlen, int initlen)
{
    window_textgrid_t *dwin = win->data;

    if (maxlen > (dwin->width - dwin->curx))
	maxlen = (dwin->width - dwin->curx);

    dwin->inbuf = buf;
    dwin->inmax = maxlen;
    dwin->inlen = 0;
    dwin->incurs = 0;
    dwin->inorgx = dwin->curx;
    dwin->inorgy = dwin->cury;
    dwin->origstyle = win->style;
    win->style = style_Input;

    if (initlen > maxlen)
	initlen = maxlen;

    if (initlen)
    {
	int ix;
	tgline_t *ln = &(dwin->lines[dwin->inorgy]);

	for (ix=0; ix<initlen; ix++) {
	    ln->attrs[dwin->inorgx+ix] = style_Input;
	    ln->chars[dwin->inorgx+ix] = buf[ix];
	}

	dwin->incurs += initlen;
	dwin->inlen += initlen;
	dwin->curx = dwin->inorgx+dwin->incurs;
	dwin->cury = dwin->inorgy;

	touch(dwin, dwin->inorgy);
    }

    if (gli_register_arr) {
		dwin->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
    }
}

#ifdef GLK_MODULE_UNICODE

void win_textgrid_init_line_uni(window_t *win, glui32 *buf, int maxlen, int initlen)
{
    window_textgrid_t *dwin = win->data;

    if (maxlen > (dwin->width - dwin->curx))
		maxlen = (dwin->width - dwin->curx);

    dwin->uinbuf = buf;
    dwin->inmax = maxlen;
    dwin->inlen = 0;
    dwin->incurs = 0;
    dwin->inorgx = dwin->curx;
    dwin->inorgy = dwin->cury;
    dwin->origstyle = win->style;
    win->style = style_Input;

    if (initlen > maxlen)
		initlen = maxlen;

    if (initlen)
    {
		int ix;
		tgline_t *ln = &(dwin->lines[dwin->inorgy]);

		for (ix=0; ix<initlen; ix++) {
			ln->attrs[dwin->inorgx+ix] = style_Input;
			ln->chars[dwin->inorgx+ix] = buf[ix];
		}

		dwin->incurs += initlen;
		dwin->inlen += initlen;
		dwin->curx = dwin->inorgx+dwin->incurs;
		dwin->cury = dwin->inorgy;

		touch(dwin, dwin->inorgy);
    }

    if (gli_register_arr) {
		dwin->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Iu");
    }
}

#endif /* GLK_MODULE_UNICODE */

/* Abort line input, storing whatever's been typed so far. */
void win_textgrid_cancel_line(window_t *win, event_t *ev)
{
    int ix;
    char *inbuf;
	glui32 *uinbuf;

    int inmax;
    gidispatch_rock_t inarrayrock;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (!dwin->inbuf && !dwin->uinbuf)
		return;

	if (!win->line_request_uni)
		inbuf = dwin->inbuf;
	else
		uinbuf = dwin->uinbuf;

    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;

	if (!win->line_request_uni) {
		for (ix=0; ix<dwin->inlen; ix++)
			inbuf[ix] = ln->chars[dwin->inorgx+ix];

		if (win->echostr)
			gli_stream_echo_line(win->echostr, inbuf, dwin->inlen);
	}
	else {
		for (ix=0; ix<dwin->inlen; ix++)
			uinbuf[ix] = (glui32)(((unsigned char *)ln->chars)[dwin->inorgx+ix]);

		if (win->echostr)
			gli_stream_echo_line_uni(win->echostr, uinbuf, dwin->inlen);
	}

    dwin->cury = dwin->inorgy+1;
    dwin->curx = 0;
    win->style = dwin->origstyle;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = dwin->inlen;

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
    dwin->inorgx = 0;
    dwin->inorgy = 0;

}

/* Keybinding functions. */

/* Any key, during character input. Ends character input. */
void gcmd_grid_accept_readchar(window_t *win, glui32 arg)
{
    win->char_request = FALSE;
	win->char_request_uni = FALSE;
    gli_event_store(evtype_CharInput, win, arg, 0);
}

/* Return or enter, during line input. Ends line input. */
static void acceptline(window_t *win)
{
    int ix;
    char *inbuf;
	glui32 *uinbuf;
    int inmax;
    gidispatch_rock_t inarrayrock;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (!dwin->inbuf && !dwin->uinbuf)
		return;

	if (!win->line_request_uni)
		inbuf = dwin->inbuf;
	else
		uinbuf = dwin->uinbuf;

    inmax = dwin->inmax;
    inarrayrock = dwin->inarrayrock;

	if (!win->line_request_uni) {
		for (ix=0; ix<dwin->inlen; ix++) {
			inbuf[ix] = ln->chars[dwin->inorgx+ix];
		}

		if (win->echostr)
			gli_stream_echo_line(win->echostr, inbuf, dwin->inlen);
	}
	else {
		for (ix=0; ix<dwin->inlen; ix++) {
			uinbuf[ix] = (glui32)(((unsigned char *)ln->chars)[dwin->inorgx+ix]);
		}

		if (win->echostr)
			gli_stream_echo_line_uni(win->echostr, uinbuf, dwin->inlen);
	}

    dwin->cury = dwin->inorgy+1;
    dwin->curx = 0;
    win->style = dwin->origstyle;

    gli_event_store(evtype_LineInput, win, dwin->inlen, 0);

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
    dwin->inorgx = 0;
    dwin->inorgy = 0;

}

/* Any regular key, during line input. */
void gcmd_grid_accept_readline(window_t *win, glui32 arg)
{
    int ix;
    window_textgrid_t *dwin = win->data;
    tgline_t *ln = &(dwin->lines[dwin->inorgy]);

    if (!dwin->inbuf && !dwin->uinbuf)
		return;

    switch (arg)
    {

	/* Delete keys, during line input. */

    case keycode_Delete:
		if (dwin->inlen <= 0)
			return;
		if (dwin->incurs <= 0)
			return;
		for (ix=dwin->incurs; ix<dwin->inlen; ix++) 
			ln->chars[dwin->inorgx+ix-1] = ln->chars[dwin->inorgx+ix];
		ln->chars[dwin->inorgx+dwin->inlen-1] = ' ';
		dwin->incurs--;
		dwin->inlen--;
	break;

    case keycode_Escape:
		if (dwin->inlen <= 0)
			return;
		for (ix=0; ix<dwin->inlen; ix++) 
			ln->chars[dwin->inorgx+ix] = ' ';
		dwin->inlen = 0;
		dwin->incurs = 0;
	break;

	/* Cursor movement keys, during line input. */

    case keycode_Left:
		if (dwin->incurs <= 0)
			return;
		dwin->incurs--;
	break;

    case keycode_Right:
		if (dwin->incurs >= dwin->inlen)
			return;
		dwin->incurs++;
	break;

    case keycode_Home:
		if (dwin->incurs <= 0)
			return;
		dwin->incurs = 0;
	break;

    case keycode_End:
		if (dwin->incurs >= dwin->inlen)
			return;
		dwin->incurs = dwin->inlen;
	break;

    case keycode_Return:
		acceptline(win);
	break;

    default:
	if (dwin->inlen >= dwin->inmax)
	    return;

	if (arg < 32 || arg > 0xff)
	    return;

	for (ix=dwin->inlen; ix>dwin->incurs; ix--) 
	    ln->chars[dwin->inorgx+ix] = ln->chars[dwin->inorgx+ix-1];
	ln->attrs[dwin->inorgx+dwin->inlen] = style_Input;
	ln->chars[dwin->inorgx+dwin->incurs] = arg;

	dwin->incurs++;
	dwin->inlen++;
    }

    dwin->curx = dwin->inorgx+dwin->incurs;
    dwin->cury = dwin->inorgy;

    touch(dwin, dwin->inorgy);
}


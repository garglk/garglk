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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

#define LINES 24
#define COLS 70

bool gli_force_redraw = true;
bool gli_more_focus = false;

/* Linked list of all windows */
static window_t *gli_windowlist = NULL;

window_t *gli_rootwin = NULL; /* The topmost window. */
window_t *gli_focuswin = NULL; /* The window selected by the player */

void (*gli_interrupt_handler)(void) = NULL;

/* record whether we've returned a click event */
bool gli_forceclick = false;

/* Set up the window system. This is called from main(). */
void gli_initialize_windows()
{
    gli_rootwin = NULL;
    gli_focuswin = NULL;
}

static void gli_windows_rearrange(void)
{
    if (gli_rootwin)
    {
        rect_t box;

        if (gli_conf_lockcols && gli_cols <= MAX_TEXT_COLUMNS )
        {
            /* Lock the number of columns */
            int desired_width = gli_wmarginx_save * 2 + gli_cellw * gli_cols;
            if (desired_width > gli_image_w)
                gli_wmarginx = gli_wmarginx_save;
            else
                gli_wmarginx = (gli_image_w - gli_cellw * gli_cols) / 2;
        }
        else
        {
            /* Limit the maximum number of columns */
            int max_width = gli_wmarginx_save * 2 + gli_cellw * MAX_TEXT_COLUMNS;
            if (max_width < gli_image_w)
                gli_wmarginx = (gli_image_w - gli_cellw * MAX_TEXT_COLUMNS) / 2;
            else
                gli_wmarginx = gli_wmarginx_save;
        }

        if (gli_conf_lockrows && gli_rows <= MAX_TEXT_ROWS)
        {
            /* Lock the number of rows */
            int desired_height = gli_wmarginy_save * 2 + gli_cellh * gli_rows;
            if (desired_height > gli_image_h)
                gli_wmarginy = gli_wmarginy_save;
            else
                gli_wmarginy = (gli_image_h - gli_cellh * gli_rows) / 2;
        }
        else
        {
            /* Limit the maximum number of rows */
            int max_height = gli_wmarginy_save * 2 + gli_cellh * MAX_TEXT_ROWS;
            if (max_height < gli_image_h)
                gli_wmarginy = (gli_image_h - gli_cellh * MAX_TEXT_ROWS) / 2;
            else
                gli_wmarginy = gli_wmarginy_save;
        }

        box.x0 = gli_wmarginx;
        box.y0 = gli_wmarginy;
        box.x1 = gli_image_w - gli_wmarginx;
        box.y1 = gli_image_h - gli_wmarginy;
        gli_window_rearrange(gli_rootwin, &box);
    }
}

/*
 * Create, destroy and arrange
 */

window_t *gli_new_window(glui32 type, glui32 rock)
{
    window_t *win = malloc(sizeof(window_t));
    if (!win)
        return NULL;

    win->magicnum = MAGIC_WINDOW_NUM;
    win->rock = rock;
    win->type = type;

    win->parent = NULL; /* for now */
    win->data = NULL; /* for now */
    win->yadj = 0;

    win->char_request = false;
    win->char_request_uni = false;
    win->line_request = false;
    win->line_request_uni = false;
    win->mouse_request = false;
    win->hyper_request = false;
    win->more_request = false;
    win->scroll_request = false;
    win->image_loaded = false;

    win->echo_line_input = true;
    win->line_terminators = NULL;
    win->termct = 0;

    attrclear(&win->attr);
    memcpy(win->bgcolor, gli_window_color, 3);
    memcpy(win->fgcolor, gli_more_color, 3);

    win->str = gli_stream_open_window(win);
    win->echostr = NULL;

    win->prev = NULL;
    win->next = gli_windowlist;
    gli_windowlist = win;
    if (win->next)
        win->next->prev = win;

    if (gli_register_obj)
        win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);

    return win;
}

void gli_delete_window(window_t *win)
{
    window_t *prev, *next;

    if (gli_unregister_obj)
        (*gli_unregister_obj)(win, gidisp_Class_Window, win->disprock);

    win->magicnum = 0;

    win->echostr = NULL;
    if (win->str)
    {
        gli_delete_stream(win->str);
        win->str = NULL;
    }

    if (win->line_terminators)
    {
        free(win->line_terminators);
        win->line_terminators = NULL;
    }

    prev = win->prev;
    next = win->next;
    win->prev = NULL;
    win->next = NULL;

    if (prev)
        prev->next = next;
    else
        gli_windowlist = next;
    if (next)
        next->prev = prev;

    free(win);
}

winid_t glk_window_open(winid_t splitwin,
        glui32 method, glui32 size,
        glui32 wintype, glui32 rock)
{
    window_t *newwin, *pairwin, *oldparent;
    window_pair_t *dpairwin;
    glui32 val;

    gli_force_redraw = true;

    if (!gli_rootwin)
    {
        if (splitwin)
        {
            gli_strict_warning("window_open: ref must be NULL");
            return 0;
        }

        /* ignore method and size now */
        oldparent = NULL;
    }

    else
    {
        if (!splitwin)
        {
            gli_strict_warning("window_open: ref must not be NULL");
            return 0;
        }

        val = (method & winmethod_DivisionMask);
        if (val != winmethod_Fixed && val != winmethod_Proportional)
        {
            gli_strict_warning("window_open: invalid method (not fixed or proportional)");
            return 0;
        }

        val = (method & winmethod_DirMask);
        if (val != winmethod_Above && val != winmethod_Below
            && val != winmethod_Left && val != winmethod_Right)
        {
            gli_strict_warning("window_open: invalid method (bad direction)");
            return 0;
        }

        oldparent = splitwin->parent;
        if (oldparent && oldparent->type != wintype_Pair)
        {
            gli_strict_warning("window_open: parent window is not Pair");
            return 0;
        }
    }

    newwin = gli_new_window(wintype, rock);
    if (!newwin)
    {
        gli_strict_warning("window_open: unable to create window");
        return 0;
    }

    switch (wintype)
    {
        case wintype_Blank:
            newwin->data = win_blank_create(newwin);
            break;
        case wintype_TextGrid:
            newwin->data = win_textgrid_create(newwin);
            break;
        case wintype_TextBuffer:
            newwin->data = win_textbuffer_create(newwin);
            break;
        case wintype_Graphics:
            newwin->data = win_graphics_create(newwin);
            if (method & winmethod_Fixed)
                size = gli_zoom_int(size);
            break;
        case wintype_Pair:
            gli_strict_warning("window_open: cannot open pair window directly");
            gli_delete_window(newwin);
            return 0;
        default:
            /* Unknown window type -- do not print a warning, just return 0
               to indicate that it's not possible. */
            gli_delete_window(newwin);
            return 0;
    }

    if (!newwin->data)
    {
        gli_strict_warning("window_open: unable to create window");
        return 0;
    }

    if (!splitwin)
    {
        gli_rootwin = newwin;
    }
    else
    {
        /* create pairwin, with newwin as the key */
        pairwin = gli_new_window(wintype_Pair, 0);
        dpairwin = win_pair_create(pairwin, method, newwin, size);
        pairwin->data = dpairwin;

        dpairwin->child1 = splitwin;
        dpairwin->child2 = newwin;

        splitwin->parent = pairwin;
        newwin->parent = pairwin;
        pairwin->parent = oldparent;

        if (oldparent)
        {
            window_pair_t *dparentwin = oldparent->data;
            if (dparentwin->child1 == splitwin)
                dparentwin->child1 = pairwin;
            else
                dparentwin->child2 = pairwin;
        }
        else
        {
            gli_rootwin = pairwin;
        }
    }

    gli_windows_rearrange();

    return newwin;
}

static void gli_window_close(window_t *win, bool recurse)
{
    window_t *wx;

    if (gli_focuswin == win)
        gli_focuswin = NULL;

    for (wx=win->parent; wx; wx=wx->parent)
    {
        if (wx->type == wintype_Pair)
        {
            window_pair_t *dwx = wx->data;
            if (dwx->key == win)
            {
                dwx->key = NULL;
                dwx->keydamage = true;
            }
        }
    }

    if (win->image_loaded)
        gli_piclist_decrement();

    switch (win->type)
    {
        case wintype_Blank:
        {
            window_blank_t *dwin = win->data;
            win_blank_destroy(dwin);
        }
        break;
        case wintype_Pair:
        {
            window_pair_t *dwin = win->data;
            if (recurse)
            {
                if (dwin->child1)
                    gli_window_close(dwin->child1, true);
                if (dwin->child2)
                    gli_window_close(dwin->child2, true);
            }
            win_pair_destroy(dwin);
        }
        break;
        case wintype_TextBuffer:
        {
            window_textbuffer_t *dwin = win->data;
            win_textbuffer_destroy(dwin);
        }
        break;
        case wintype_TextGrid:
        {
            window_textgrid_t *dwin = win->data;
            win_textgrid_destroy(dwin);
        }
        break;
        case wintype_Graphics:
        {
            window_graphics_t *dwin = win->data;
            win_graphics_destroy(dwin);
        }
        break;
    }

    gli_delete_window(win);
}

void glk_window_close(window_t *win, stream_result_t *result)
{
    gli_force_redraw = true;

    if (!win)
    {
        gli_strict_warning("window_close: invalid ref");
        return;
    }

    if (win == gli_rootwin || win->parent == NULL)
    {
        /* close the root window, which means all windows. */

        gli_rootwin = 0;

        /* begin (simpler) closation */

        gli_stream_fill_result(win->str, result);
        gli_window_close(win, true);
    }

    else
    {
        /* have to jigger parent */
        window_t *pairwin, *sibwin, *grandparwin;
        window_pair_t *dpairwin, *dgrandparwin;

        pairwin = win->parent;
        dpairwin = pairwin->data;
        if (win == dpairwin->child1)
        {
            sibwin = dpairwin->child2;
        }
        else if (win == dpairwin->child2)
        {
            sibwin = dpairwin->child1;
        }
        else
        {
            gli_strict_warning("window_close: window tree is corrupted");
            return;
        }

        grandparwin = pairwin->parent;
        if (!grandparwin)
        {
            gli_rootwin = sibwin;
            sibwin->parent = NULL;
        }
        else
        {
            dgrandparwin = grandparwin->data;
            if (dgrandparwin->child1 == pairwin)
                dgrandparwin->child1 = sibwin;
            else
                dgrandparwin->child2 = sibwin;
            sibwin->parent = grandparwin;
        }

        /* Begin closation */

        gli_stream_fill_result(win->str, result);

        /* Close the child window (and descendants), so that key-deletion can
            crawl up the tree to the root window. */
        gli_window_close(win, true);

        /* This probably isn't necessary, but the child *is* gone, so just
            in case. */
        if (win == dpairwin->child1)
            dpairwin->child1 = NULL;
        else if (win == dpairwin->child2)
            dpairwin->child2 = NULL;

        /* Now we can delete the parent pair. */
        gli_window_close(pairwin, false);

        /* Sort out the arrangements */
        gli_windows_rearrange();
    }
}

void glk_window_get_arrangement(window_t *win, glui32 *method, glui32 *size,
    winid_t *keywin)
{
    window_pair_t *dwin;
    glui32 val;

    if (!win)
    {
        gli_strict_warning("window_get_arrangement: invalid ref");
        return;
    }

    if (win->type != wintype_Pair)
    {
        gli_strict_warning("window_get_arrangement: not a Pair window");
        return;
    }

    dwin = win->data;

    val = dwin->dir | dwin->division;
    if (!dwin->wborder)
        val |= winmethod_NoBorder;

    if (size)
    {
        *size = dwin->size;
        if (dwin->key && (dwin->key->type == wintype_Graphics) && (dwin->dir == winmethod_Fixed))
            *size = gli_unzoom_int(*size);
    }
    if (keywin)
    {
        if (dwin->key)
            *keywin = dwin->key;
        else
            *keywin = NULL;
    }
    if (method)
        *method = val;
}

void glk_window_set_arrangement(window_t *win, glui32 method, glui32 size, winid_t key)
{
    window_pair_t *dwin;
    glui32 newdir;
    int newvertical, newbackward;

    gli_force_redraw = true;

    if (!win)
    {
        gli_strict_warning("window_set_arrangement: invalid ref");
        return;
    }

    if (win->type != wintype_Pair)
    {
        gli_strict_warning("window_set_arrangement: not a Pair window");
        return;
    }

    if (key)
    {
        window_t *wx;
        if (key->type == wintype_Pair)
        {
            gli_strict_warning("window_set_arrangement: keywin cannot be a Pair");
            return;
        }
        for (wx=key; wx; wx=wx->parent)
        {
            if (wx == win)
                break;
        }
        if (wx == NULL)
        {
            gli_strict_warning("window_set_arrangement: keywin must be a descendant");
            return;
        }
    }

    dwin = win->data;

    newdir = method & winmethod_DirMask;
    newvertical = (newdir == winmethod_Left || newdir == winmethod_Right);
    newbackward = (newdir == winmethod_Left || newdir == winmethod_Above);
    if (!key)
        key = dwin->key;

    if ((newvertical && !dwin->vertical) || (!newvertical && dwin->vertical))
    {
        if (!dwin->vertical)
            gli_strict_warning("window_set_arrangement: split must stay horizontal");
        else
            gli_strict_warning("window_set_arrangement: split must stay vertical");
        return;
    }

    if (key && key->type == wintype_Blank
        && (method & winmethod_DivisionMask) == winmethod_Fixed)
    {
        gli_strict_warning("window_set_arrangement: a Blank window cannot have a fixed size");
        return;
    }

    if ((newbackward && !dwin->backward) || (!newbackward && dwin->backward))
    {
        /* switch the children */
        window_t *tmpwin = dwin->child1;
        dwin->child1 = dwin->child2;
        dwin->child2 = tmpwin;
    }

    /* set up everything else */
    dwin->dir = newdir;
    dwin->division = method & winmethod_DivisionMask;
    dwin->key = key;
    dwin->size = size;
    if (key && (key->type == wintype_Graphics) && (newdir == winmethod_Fixed))
        dwin->size = gli_zoom_int(dwin->size);
    dwin->wborder = ((method & winmethod_BorderMask) == winmethod_Border);

    dwin->vertical = (dwin->dir == winmethod_Left || dwin->dir == winmethod_Right);
    dwin->backward = (dwin->dir == winmethod_Left || dwin->dir == winmethod_Above);

    gli_windows_rearrange();
}

void glk_window_get_size(window_t *win, glui32 *width, glui32 *height)
{
    glui32 wid = 0;
    glui32 hgt = 0;

    if (!win)
    {
        gli_strict_warning("window_get_size: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_Blank:
        case wintype_Pair:
            /* always zero */
            break;
        case wintype_TextGrid:
            wid = win->bbox.x1 - win->bbox.x0;
            hgt = win->bbox.y1 - win->bbox.y0;
            wid = wid / gli_cellw;
            hgt = hgt / gli_cellh;
            break;
        case wintype_TextBuffer:
            wid = win->bbox.x1 - win->bbox.x0 - gli_tmarginx * 2;
            hgt = win->bbox.y1 - win->bbox.y0 - gli_tmarginy * 2;
            wid = wid / gli_cellw;
            hgt = hgt / gli_cellh;
            break;
        case wintype_Graphics:
            wid = gli_unzoom_int((win->bbox.x1 - win->bbox.x0));
            hgt = gli_unzoom_int(win->bbox.y1 - win->bbox.y0);
            break;
    }

    if (width)
        *width = wid;
    if (height)
        *height = hgt;
}

void gli_calc_padding(window_t *win, int *x, int *y)
{
    window_pair_t *wp;
    if (!win)
        return;
    if (win->type == wintype_Pair)
    {
        wp = win->data;
        if (wp->vertical)
            *x += gli_wpaddingx;
        else
            *y += gli_wpaddingy;
        gli_calc_padding(wp->child1, x, y);
        gli_calc_padding(wp->child2, x, y);
    }
}

/*
 * Family matters
 */

winid_t glk_window_iterate(winid_t win, glui32 *rock)
{
    if (!win)
        win = gli_windowlist;
    else
        win = win->next;

    if (win)
    {
        if (rock)
            *rock = win->rock;
        return win;
    }

    if (rock)
        *rock = 0;
    return NULL;
}

window_t *gli_window_iterate_treeorder(window_t *win)
{
    if (!win)
        return gli_rootwin;

    if (win->type == wintype_Pair)
    {
        window_pair_t *dwin = win->data;
        if (!dwin->backward)
            return dwin->child1;
        else
            return dwin->child2;
    }
    else
    {
        window_t *parwin;
        window_pair_t *dwin;

        while (win->parent)
        {
            parwin = win->parent;
            dwin = parwin->data;
            if (!dwin->backward)
            {
                if (win == dwin->child1)
                    return dwin->child2;
            }
            else
            {
                if (win == dwin->child2)
                    return dwin->child1;
            }
            win = parwin;
        }

        return NULL;
    }
}

glui32 glk_window_get_rock(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_rock: invalid ref.");
        return 0;
    }

    return win->rock;
}

winid_t glk_window_get_root()
{
    if (!gli_rootwin)
        return NULL;
    return gli_rootwin;
}

winid_t glk_window_get_parent(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_parent: invalid ref");
        return 0;
    }
    if (win->parent)
        return win->parent;
    else
        return 0;
}

winid_t glk_window_get_sibling(window_t *win)
{
    window_pair_t *dparwin;

    if (!win)
    {
        gli_strict_warning("window_get_sibling: invalid ref");
        return 0;
    }
    if (!win->parent)
        return 0;

    dparwin = win->parent->data;
    if (dparwin->child1 == win)
        return dparwin->child2;
    else if (dparwin->child2 == win)
        return dparwin->child1;
    return 0;
}

glui32 glk_window_get_type(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_parent: invalid ref");
        return 0;
    }
    return win->type;
}

strid_t glk_window_get_stream(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_stream: invalid ref");
        return NULL;
    }

    return win->str;
}

strid_t glk_window_get_echo_stream(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_echo_stream: invalid ref");
        return 0;
    }

    if (win->echostr)
        return win->echostr;
    else
        return 0;
}

void glk_window_set_echo_stream(window_t *win, stream_t *str)
{
    if (!win)
    {
        gli_strict_warning("window_set_echo_stream: invalid window id");
        return;
    }

    win->echostr = str;
}

void glk_set_window(window_t *win)
{
    if (!win)
        gli_stream_set_current(NULL);
    else
        gli_stream_set_current(win->str);
}

void gli_windows_unechostream(stream_t *str)
{
    window_t *win;
    for (win=gli_windowlist; win; win=win->next)
    {
        if (win->echostr == str)
            win->echostr = NULL;
    }
}

/*
 * Size changes, rearrangement and redrawing.
 */

void gli_window_rearrange(window_t *win, rect_t *box)
{
    switch (win->type)
    {
        case wintype_Blank:
            win_blank_rearrange(win, box);
            break;
        case wintype_Pair:
            win_pair_rearrange(win, box);
            break;
        case wintype_TextGrid:
            win_textgrid_rearrange(win, box);
            break;
        case wintype_TextBuffer:
            win_textbuffer_rearrange(win, box);
            break;
        case wintype_Graphics:
            win_graphics_rearrange(win, box);
            break;
    }
}

void gli_windows_size_change()
{
    gli_windows_rearrange();
    gli_event_store(evtype_Arrange, NULL, 0, 0);
}

void gli_window_redraw(window_t *win)
{
    if (gli_force_redraw)
    {
        unsigned char *color = gli_override_bg_set ? gli_window_color : win->bgcolor;
        int y0 = win->yadj ? win->bbox.y0 - win->yadj : win->bbox.y0;
        gli_draw_rect(win->bbox.x0, y0,
                win->bbox.x1 - win->bbox.x0,
                win->bbox.y1 - y0,
                color);
    }

    switch (win->type)
    {
        case wintype_Blank:
            win_blank_redraw(win);
            break;
        case wintype_Pair:
            win_pair_redraw(win);
            break;
        case wintype_TextGrid:
            win_textgrid_redraw(win);
            break;
        case wintype_TextBuffer:
            win_textbuffer_redraw(win);
            break;
        case wintype_Graphics:
            win_graphics_redraw(win);
            break;
    }
}

void gli_window_refocus(window_t *win)
{
    window_t *focus = win;

    do
    {
        if (focus && focus->more_request)
        {
            gli_focuswin = focus;
            return;
        }

        focus = gli_window_iterate_treeorder(focus);
    }
    while (focus != win);

    gli_more_focus = false;
}

void gli_windows_redraw()
{
    gli_claimselect = false;

    if (gli_force_redraw)
    {
        winrepaint(0, 0, gli_image_w, gli_image_h);
        gli_draw_clear(gli_window_color);
    }

    if (gli_rootwin)
        gli_window_redraw(gli_rootwin);

    if (gli_more_focus)
        gli_window_refocus(gli_focuswin);

    gli_force_redraw = false;
}

void gli_redraw_rect(int x0, int y0, int x1, int y1)
{
    gli_drawselect = true;
    winrepaint(x0, y0, x1, y1);
}

/*
 * Input events
 */

void glk_request_char_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("request_char_event: invalid ref");
        return;
    }

    if (win->char_request || win->line_request || win->char_request_uni || win->line_request_uni)
    {
        gli_strict_warning("request_char_event: window already has keyboard request");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = true;
            break;
        default:
            gli_strict_warning("request_char_event: window does not support keyboard input");
            break;
    }

}

void glk_request_char_event_uni(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("request_char_event_uni: invalid ref");
        return;
    }

    if (win->char_request || win->line_request || win->char_request_uni || win->line_request_uni)
    {
        gli_strict_warning("request_char_event_uni: window already has keyboard request");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request_uni = true;
            break;
        default:
            gli_strict_warning("request_char_event_uni: window does not support keyboard input");
            break;
    }

}

void glk_request_line_event(window_t *win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    if (!win)
    {
        gli_strict_warning("request_line_event: invalid ref");
        return;
    }

    if (win->char_request || win->line_request || win->char_request_uni || win->line_request_uni)
    {
        gli_strict_warning("request_line_event: window already has keyboard request");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
            win->line_request = true;
            win_textbuffer_init_line(win, buf, maxlen, initlen);
            break;
        case wintype_TextGrid:
            win->line_request = true;
            win_textgrid_init_line(win, buf, maxlen, initlen);
            break;
        default:
            gli_strict_warning("request_line_event: window does not support keyboard input");
            break;
    }

}

void glk_request_line_event_uni(window_t *win, glui32 *buf, glui32 maxlen,
    glui32 initlen)
{
    if (!win)
    {
        gli_strict_warning("request_line_event_uni: invalid ref");
        return;
    }

    if (win->char_request || win->line_request || win->char_request_uni || win->line_request_uni)
    {
        gli_strict_warning("request_line_event_uni: window already has keyboard request");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
            win->line_request_uni = true;
            win_textbuffer_init_line_uni(win, buf, maxlen, initlen);
            break;
        case wintype_TextGrid:
            win->line_request_uni = true;
            win_textgrid_init_line_uni(win, buf, maxlen, initlen);
            break;
        default:
            gli_strict_warning("request_line_event_uni: window does not support keyboard input");
            break;
    }

}

void glk_set_echo_line_event(window_t *win, glui32 val)
{
    if (!win)
    {
        gli_strict_warning("set_echo_line_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
            win->echo_line_input = (val != 0);
            break;
        default:
            break;
    }
}

void glk_set_terminators_line_event(winid_t win, glui32 *keycodes, glui32 count)
{
    if (!win)
    {
        gli_strict_warning("set_terminators_line_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            break;
        default:
            gli_strict_warning("set_terminators_line_event: window does not support keyboard input");
            return;
    }

    if (win->line_terminators)
        free(win->line_terminators);

    if (!keycodes || count == 0)
    {
        win->line_terminators = NULL;
        win->termct = 0;
    }
    else
    {
        win->line_terminators = malloc((count + 1) * sizeof(glui32));
        if (win->line_terminators)
        {
            memcpy(win->line_terminators, keycodes, count * sizeof(glui32));
            win->line_terminators[count] = 0;
            win->termct = count;
        }
    }
}

bool gli_window_check_terminator(glui32 ch)
{
    if (ch == keycode_Escape)
        return true;
    else if (ch >= keycode_Func12 && ch <= keycode_Func1)
        return true;
    else
        return false;
}

void glk_request_mouse_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("request_mouse_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextGrid:
        case wintype_Graphics:
            win->mouse_request = true;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_request_hyperlink_event(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("request_hyperlink_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
        case wintype_Graphics:
            win->hyper_request = true;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_char_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("cancel_char_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = false;
            win->char_request_uni = false;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_line_event(window_t *win, event_t *ev)
{
    event_t dummyev;

    if (!ev)
        ev = &dummyev;

    gli_event_clearevent(ev);

    if (!win)
    {
        gli_strict_warning("cancel_line_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
            if (win->line_request || win->line_request_uni)
                win_textbuffer_cancel_line(win, ev);
            break;
        case wintype_TextGrid:
            if (win->line_request || win->line_request_uni)
                win_textgrid_cancel_line(win, ev);
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_mouse_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("cancel_mouse_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextGrid:
        case wintype_Graphics:
            win->mouse_request = false;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_hyperlink_event(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("cancel_hyperlink_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
        case wintype_Graphics:
            win->hyper_request = false;
            break;
        default:
            /* do nothing */
            break;
    }
}

void gli_window_click(window_t *win, int x, int y)
{
    switch (win->type)
    {
        case wintype_Pair:
            win_pair_click(win->data, x, y);
            break;
        case wintype_TextBuffer:
            win_textbuffer_click(win->data, x, y);
            break;
        case wintype_TextGrid:
            win_textgrid_click(win->data, x, y);
            break;
        case wintype_Graphics:
            win_graphics_click(win->data, x, y);
            break;
    }
}

/*
 * Text output and cursor positioning
 */

void gli_window_put_char_uni(window_t *win, glui32 ch)
{
    switch (win->type)
    {
        case wintype_TextBuffer:
            win_textbuffer_putchar_uni(win, ch);
            break;
        case wintype_TextGrid:
            win_textgrid_putchar_uni(win, ch);
            break;
    }
}

bool gli_window_unput_char_uni(window_t *win, glui32 ch)
{
    switch (win->type)
    {
        case wintype_TextBuffer:
            return win_textbuffer_unputchar_uni(win, ch);
        case wintype_TextGrid:
            return win_textgrid_unputchar_uni(win, ch);
        default:
            return false;
    }
}

void glk_window_clear(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_clear: invalid ref");
        return;
    }

    if (win->line_request || win->line_request_uni)
    {
        if (gli_conf_safeclicks && gli_forceclick)
        {
            glk_cancel_line_event(win, NULL);
            gli_forceclick = false;
        }
        else
        {
            gli_strict_warning("window_clear: window has pending line request");
            return;
        }
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
            win_textbuffer_clear(win);
            break;
        case wintype_TextGrid:
            win_textgrid_clear(win);
            break;
        case wintype_Graphics:
            win_graphics_erase_rect(win->data, true, 0, 0, 0, 0);
            break;
    }
}

void glk_window_move_cursor(window_t *win, glui32 xpos, glui32 ypos)
{
    if (!win)
    {
        gli_strict_warning("window_move_cursor: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextGrid:
            win_textgrid_move_cursor(win, xpos, ypos);
            break;
        default:
            gli_strict_warning("window_move_cursor: not a TextGrid window");
            break;
    }
}

/*
 * Graphics and Image drawing
 */

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
    if (!win)
    {
        gli_strict_warning("image_draw: invalid ref");
        return false;
    }

    if (!gli_conf_graphics)
        return false;

    switch (win->type)
    {
        case wintype_TextBuffer:
            return win_textbuffer_draw_picture(win->data, image, val1,
                    false, 0, 0);
        case wintype_Graphics:
            return win_graphics_draw_picture(win->data, image, val1, val2,
                    false, 0, 0);
    }
    return false;
}

glui32 glk_image_draw_scaled(winid_t win, glui32 image,
        glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
    if (!win)
    {
        gli_strict_warning("image_draw_scaled: invalid ref");
        return false;
    }

    if (!gli_conf_graphics)
        return false;

    switch (win->type)
    {
        case wintype_TextBuffer:
            return win_textbuffer_draw_picture(win->data, image, val1,
                    true, width, height);
        case wintype_Graphics:
            return win_graphics_draw_picture(win->data, image, val1, val2,
                    true, width, height);
    }

    return false;
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
    picture_t *pic;

    if (!gli_conf_graphics)
        return false;

    pic = gli_picture_load(image);
    if (!pic)
        return false;

    if (width)
        *width = pic->w;
    if (height)
        *height = pic->h;

    return true;
}

void glk_window_flow_break(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("window_erase_rect: invalid ref");
        return;
    }
    if (win->type != wintype_TextBuffer)
    {
        gli_strict_warning("window_flow_break: not a text buffer window");
        return;
    }
    win_textbuffer_flow_break(win->data);
}

void glk_window_erase_rect(winid_t win,
        glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    if (!win)
    {
        gli_strict_warning("window_erase_rect: invalid ref");
        return;
    }
    if (win->type != wintype_Graphics)
    {
        gli_strict_warning("window_erase_rect: not a graphics window");
        return;
    }
    win_graphics_erase_rect(win->data, false, left, top, width, height);
}

void glk_window_fill_rect(winid_t win, glui32 color,
        glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    if (!win)
    {
        gli_strict_warning("window_fill_rect: invalid ref");
        return;
    }
    if (win->type != wintype_Graphics)
    {
        gli_strict_warning("window_fill_rect: not a graphics window");
        return;
    }
    win_graphics_fill_rect(win->data, color, left, top, width, height);
}

void glk_window_set_background_color(winid_t win, glui32 color)
{
    if (!win)
    {
        gli_strict_warning("window_set_background_color: invalid ref");
        return;
    }
    if (win->type != wintype_Graphics)
    {
        gli_strict_warning("window_set_background_color: not a graphics window");
        return;
    }
    win_graphics_set_background_color(win->data, color);
}

void attrset(attr_t *attr, glui32 style)
{
    attr->fgset = false;
    attr->bgset = false;
    attr->fgcolor = 0;
    attr->bgcolor = 0;
    attr->reverse = false;
    attr->style = style;
}

void attrclear(attr_t *attr)
{
    attr->fgset = false;
    attr->bgset = false;
    attr->fgcolor = 0;
    attr->bgcolor = 0;
    attr->reverse = false;
    attr->hyper = 0;
    attr->style = 0;
}

int attrfont(style_t *styles, attr_t *attr)
{
    return styles[attr->style].font;
}

static unsigned char zcolor_LightGrey[3] = { 181, 181, 181 };
static unsigned char zcolor_Foreground[3] = { 0, 0, 0 };
static unsigned char zcolor_Background[3] = { 0, 0, 0 };
static unsigned char zcolor_Bright[3] = { 0, 0, 0 };

static unsigned int zcolor_fg = 0;
static unsigned int zcolor_bg = 0;

static unsigned char *rgbshift (unsigned char *rgb)
{
    zcolor_Bright[0] = (rgb[0] + 0x30) < 0xff ? (rgb[0] + 0x30) : 0xff;
    zcolor_Bright[1] = (rgb[1] + 0x30) < 0xff ? (rgb[1] + 0x30) : 0xff;
    zcolor_Bright[2] = (rgb[2] + 0x30) < 0xff ? (rgb[2] + 0x30) : 0xff;

    return zcolor_Bright;
}

unsigned char *attrbg(style_t *styles, attr_t *attr)
{
    int revset = attr->reverse || (styles[attr->style].reverse && !gli_override_reverse);

    int zfset = attr->fgset ? attr->fgset : gli_override_fg_set;
    int zbset = attr->bgset ? attr->bgset : gli_override_bg_set;

    int zfore = attr->fgset ? attr->fgcolor : gli_override_fg_val;
    int zback = attr->bgset ? attr->bgcolor : gli_override_bg_val;

    if (zfset && zfore != zcolor_fg)
    {
        zcolor_Foreground[0] = (zfore >> 16) & 0xff;
        zcolor_Foreground[1] = (zfore >> 8) & 0xff;
        zcolor_Foreground[2] = (zfore) & 0xff;
        zcolor_fg = zfore;
    }

    if (zbset && zback != zcolor_bg)
    {
        zcolor_Background[0] = (zback >> 16) & 0xff;
        zcolor_Background[1] = (zback >> 8) & 0xff;
        zcolor_Background[2] = (zback) & 0xff;
        zcolor_bg = zback;
    }

    if (!revset)
    {
        if (zbset)
            return zcolor_Background;
        else
            return styles[attr->style].bg;
    }
    else
    {
        if (zfset)
            if (zfore == zback)
                return rgbshift(zcolor_Foreground);
            else
                return zcolor_Foreground;
        else
            if (zbset && !memcmp(styles[attr->style].fg, zcolor_Background, 3))
                return zcolor_LightGrey;
            else
                return styles[attr->style].fg;
    }
}

unsigned char *attrfg(style_t *styles, attr_t *attr)
{
    int revset = attr->reverse || (styles[attr->style].reverse && !gli_override_reverse);

    int zfset = attr->fgset ? attr->fgset : gli_override_fg_set;
    int zbset = attr->bgset ? attr->bgset : gli_override_bg_set;

    int zfore = attr->fgset ? attr->fgcolor : gli_override_fg_val;
    int zback = attr->bgset ? attr->bgcolor : gli_override_bg_val;

    if (zfset && zfore != zcolor_fg)
    {
        zcolor_Foreground[0] = (zfore >> 16) & 0xff;
        zcolor_Foreground[1] = (zfore >> 8) & 0xff;
        zcolor_Foreground[2] = (zfore) & 0xff;
        zcolor_fg = zfore;
    }

    if (zbset && zback != zcolor_bg)
    {
        zcolor_Background[0] = (zback >> 16) & 0xff;
        zcolor_Background[1] = (zback >> 8) & 0xff;
        zcolor_Background[2] = (zback) & 0xff;
        zcolor_bg = zback;
    }

    if (!revset)
    {
        if (zfset)
            if (zfore == zback)
                return rgbshift(zcolor_Foreground);
            else
                return zcolor_Foreground;
        else
            if (zbset && !memcmp(styles[attr->style].fg, zcolor_Background, 3))
                return zcolor_LightGrey;
            else
                return styles[attr->style].fg;
    }
    else
    {
        if (zbset)
            return zcolor_Background;
        else
            return styles[attr->style].bg;
    }
}

bool attrequal(attr_t *a1, attr_t *a2)
{
    if (a1->style != a2->style)
        return false;
    if (a1->reverse != a2->reverse)
        return false;
    if (a1->fgset != a2->fgset)
        return false;
    if (a1->bgset != a2->bgset)
        return false;
    if (a1->fgcolor != a2->fgcolor)
        return false;
    if (a1->bgcolor != a2->bgcolor)
        return false;
    if (a1->hyper != a2->hyper)
        return false;
    return true;
}

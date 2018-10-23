/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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
#include "glk.h"
#include "garglk.h"

window_pair_t *win_pair_create(window_t *win, glui32 method, window_t *key, glui32 size)
{
    window_pair_t *dwin = (window_pair_t *)malloc(sizeof(window_pair_t));
    dwin->owner = win;

    dwin->dir = method & winmethod_DirMask; 
    dwin->division = method & winmethod_DivisionMask;
    dwin->key = key;
    dwin->keydamage = FALSE;
    dwin->size = size;
    dwin->wborder = ((method & winmethod_BorderMask) == winmethod_Border);

    dwin->vertical = (dwin->dir == winmethod_Left || dwin->dir == winmethod_Right);
    dwin->backward = (dwin->dir == winmethod_Left || dwin->dir == winmethod_Above);

    dwin->child1 = NULL;
    dwin->child2 = NULL;

    return dwin;
}

void win_pair_destroy(window_pair_t *dwin)
{
    dwin->owner = NULL;
    /* We leave the children untouched, because gli_window_close takes care
        of that if it's desired. */
    dwin->child1 = NULL;
    dwin->child2 = NULL;
    dwin->key = NULL;
    free(dwin);
}

void win_pair_rearrange(window_t *win, rect_t *box)
{
    window_pair_t *dwin = win->data;
    rect_t box1, box2;
    int min, diff, split, splitwid, max;
    window_t *key;
    window_t *ch1, *ch2;

    win->bbox = *box;

    if (dwin->vertical)
    {
        min = win->bbox.x0;
        max = win->bbox.x1;
    }
    else
    {
        min = win->bbox.y0;
        max = win->bbox.y1;
    }
    diff = max - min;

    /* We now figure split. */
    if (dwin->vertical)
        splitwid = gli_wpaddingx; /* want border? */
    else
        splitwid = gli_wpaddingy; /* want border? */

    switch (dwin->division)
    {
        case winmethod_Proportional:
            split = (diff * dwin->size) / 100;
            break;

        case winmethod_Fixed:
            key = dwin->key;
            if (!key)
            {
                split = 0;
            }
            else
            {
                switch (key->type)
                {
                    case wintype_TextBuffer:
                        if (dwin->vertical)
                            split = dwin->size * gli_cellw + gli_tmarginx * 2;
                        else
                            split = dwin->size * gli_cellh + gli_tmarginy * 2;
                        break;
                    case wintype_TextGrid:
                        if (dwin->vertical)
                            split = dwin->size * gli_cellw;
                        else
                            split = dwin->size * gli_cellh;
                        break;
                    case wintype_Graphics:
                        split = gli_zoom_int(dwin->size);
                        break;
                    default:
                        split = 0;
                        break;
                }
            }
            break;

        default:
            split = diff / 2;
            break;
    }

    if (!dwin->backward)
        split = max - split - splitwid;
    else
        split = min + split;

    if (min >= max)
    {
        split = min;
    }
    else
    {
      if (split < min)
          split = min;
      else if (split > max - splitwid)
          split = max - splitwid;
    }

    /* TODO: constrain bboxes by wintype */

    if (dwin->vertical)
    {
        box1.x0 = win->bbox.x0;
        box1.x1 = split;
        box2.x0 = split + splitwid;
        box2.x1 = win->bbox.x1;
        box1.y0 = win->bbox.y0;
        box1.y1 = win->bbox.y1;
        box2.y0 = win->bbox.y0;
        box2.y1 = win->bbox.y1;
    }
    else
    {
        box1.y0 = win->bbox.y0;
        box1.y1 = split;
        box2.y0 = split + splitwid;
        box2.y1 = win->bbox.y1;
        box1.x0 = win->bbox.x0;
        box1.x1 = win->bbox.x1;
        box2.x0 = win->bbox.x0;
        box2.x1 = win->bbox.x1;
    }

    if (!dwin->backward)
    {
        ch1 = dwin->child1;
        ch2 = dwin->child2;
    }
    else
    {
        ch1 = dwin->child2;
        ch2 = dwin->child1;
    }

    gli_window_rearrange(ch1, &box1);
    gli_window_rearrange(ch2, &box2);
}

void win_pair_redraw(window_t *win)
{
    window_pair_t *dwin;
    window_t *child;
    int x0, y0, x1, y1;

    if (!win)
        return;

    dwin = win->data;

    gli_window_redraw(dwin->child1);
    gli_window_redraw(dwin->child2);

    if (!dwin->backward)
        child = dwin->child1;
    else
        child = dwin->child2;

    x0 = child->bbox.x0;
    y0 = child->yadj ? child->bbox.y0 - child->yadj : child->bbox.y0;
    x1 = child->bbox.x1;
    y1 = child->bbox.y1;

    if (dwin->vertical)
    {
        int xbord = dwin->wborder ? gli_wborderx : 0;
        int xpad = (gli_wpaddingx - xbord) / 2;
        gli_draw_rect(x1 + xpad, y0, xbord, y1 - y0, gli_border_color);
    }
    else
    {
        int ybord = dwin->wborder ? gli_wbordery : 0;
        int ypad = (gli_wpaddingy - ybord) / 2;
        gli_draw_rect(x0, y1 + ypad, x1 - x0, ybord, gli_border_color);
    }
}

void win_pair_click(window_pair_t *dwin, int x, int y)
{
    int x0, y0, x1, y1;

    if (!dwin)
        return;

    x0 = dwin->child1->bbox.x0;
    y0 = dwin->child1->bbox.y0;
    x1 = dwin->child1->bbox.x1;
    y1 = dwin->child1->bbox.y1;
    if (x >= x0 && x <= x1 && y >= y0 && y <= y1)
        gli_window_click(dwin->child1, x, y);

    x0 = dwin->child2->bbox.x0;
    y0 = dwin->child2->bbox.y0;
    x1 = dwin->child2->bbox.x1;
    y1 = dwin->child2->bbox.y1;
    if (x >= x0 && x <= x1 && y >= y0 && y <= y1)
        gli_window_click(dwin->child2, x, y);
}

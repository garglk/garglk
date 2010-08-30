/******************************************************************************
 *                                                                            *
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

/* storage for hyperlink and selection coordinates */
static mask_t *gli_mask;

/* for copy selection */
int gli_copyselect = FALSE;
int gli_drawselect = FALSE;
int gli_claimselect = FALSE;

static int last_x = 0;
static int last_y = 0;

void gli_resize_mask(unsigned int x, unsigned int y)
{
    int i;

    if (!gli_mask)
    {
        gli_mask = (mask_t*) calloc(1, sizeof(mask_t));
        if (!gli_mask)
        {
            gli_strict_warning("resize_mask: out of memory");
            return;
        }
    }

    /* deallocate old storage */
    for (i = 0; i < gli_mask->hor; i++)
    {
        if (gli_mask->links[i])
            free(gli_mask->links[i]);
    }

    if (gli_mask->links)
        free(gli_mask->links);

    gli_mask->hor = x + 1;
    gli_mask->ver = y + 1;

    /* allocate new storage */
    gli_mask->links = (glui32**) calloc(gli_mask->hor, sizeof(glui32*));
    if (!gli_mask->links)
    {
        gli_strict_warning("resize_mask: out of memory");
        gli_mask->hor = 0;
        gli_mask->ver = 0;
        return;
    }

    for (i = 0; i < gli_mask->hor; i++)
    {
        gli_mask->links[i] = (glui32*) calloc(gli_mask->ver, sizeof(glui32));
        if (!gli_mask->links[i])
        {
            gli_strict_warning("resize_mask: could not allocate new memory");
            return;
        }
    }

    gli_mask->select.x0 = 0;
    gli_mask->select.y0 = 0;
    gli_mask->select.x1 = 0;
    gli_mask->select.y1 = 0;

    return;
}

void gli_put_hyperlink(glui32 linkval, unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1)
{
    int i, k;
    int tx0 = x0 < x1 ? x0 : x1;
    int tx1 = x0 < x1 ? x1 : x0;
    int ty0 = y0 < y1 ? y0 : y1;
    int ty1 = y0 < y1 ? y1 : y0;

    if (!gli_mask || !gli_mask->hor || !gli_mask->ver)
    {
        gli_strict_warning("set_hyperlink: struct not initialized");
        return;
    }

    if (tx0 >= gli_mask->hor
            || tx1 >= gli_mask->hor
            || ty0 >= gli_mask->ver  || ty1 >= gli_mask->ver
            || !gli_mask->links[tx0] || !gli_mask->links[tx1])
    {
        gli_strict_warning("set_hyperlink: invalid range given");
        return;
    }

    for (i = tx0; i < tx1; i++)
    {
        for (k = ty0; k < ty1; k++)
            gli_mask->links[i][k] = linkval;
    }

    return;
}

glui32 gli_get_hyperlink(unsigned int x, unsigned int y)
{
    if (!gli_mask || !gli_mask->hor || !gli_mask->ver)
    {
        gli_strict_warning("get_hyperlink: struct not initialized");
        return 0;
    }

    if (x >= gli_mask->hor
            || y >= gli_mask->ver
            || !gli_mask->links[x])
    {
        gli_strict_warning("get_hyperlink: invalid range given");
        return 0;
    }

    return gli_mask->links[x][y];
}

void gli_start_selection(int x, int y)
{
    int tx, ty;

    if (!gli_mask || !gli_mask->hor || !gli_mask->ver)
    {
        gli_strict_warning("start_selection: mask not initialized");
        return;
    }

    tx = x < gli_mask->hor ? x : gli_mask->hor;
    ty = y < gli_mask->ver ? y : gli_mask->ver;

    gli_mask->select.x0 = last_x = tx;
    gli_mask->select.y0 = last_y = ty;
    gli_mask->select.x1 = 0;
    gli_mask->select.y1 = 0;

    gli_claimselect = FALSE;
    gli_force_redraw = 1;
    gli_windows_redraw();
    return;
}

void gli_move_selection(int x, int y)
{
    int tx, ty;

    if (abs(x - last_x) < 5 && abs(y - last_y) < 5)
        return;

    if (!gli_mask || !gli_mask->hor || !gli_mask->ver)
    {
        gli_strict_warning("move_selection: mask not initialized");
        return;
    }

    tx = x < gli_mask->hor ? x : gli_mask->hor;
    ty = y < gli_mask->ver ? y : gli_mask->ver;

    gli_mask->select.x1 = last_x = tx;
    gli_mask->select.y1 = last_y = ty;

    gli_claimselect = FALSE;
    gli_windows_redraw();
    return;
}

void gli_clear_selection(void)
{
    if (!gli_mask)
    {
        gli_strict_warning("clear_selection: mask not initialized");
        return;
    }

    if (gli_mask->select.x0 || gli_mask->select.x1
            || gli_mask->select.y0 || gli_mask->select.y1)
            gli_force_redraw = TRUE;

    gli_mask->select.x0 = 0;
    gli_mask->select.y0 = 0;
    gli_mask->select.x1 = 0;
    gli_mask->select.y1 = 0;

    gli_claimselect = FALSE;
    return;
}

int gli_check_selection(unsigned int x0, unsigned int y0,
        unsigned int x1, unsigned int y1)
{
    int cx0, cx1, cy0, cy1;

    cx0 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x0
            : gli_mask->select.x1;

    cx1 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x1
            : gli_mask->select.x0;

    cy0 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y0
            : gli_mask->select.y1;

    cy1 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y1
            : gli_mask->select.y0;

    if (!cx0 || !cx1 || !cy0 || !cy1)
        return FALSE;

    if (cx0 >= x0 && cx0 <= x1
            && cy0 >= y0 && cy0 <= y1)
        return TRUE;

    if (cx0 >= x0 && cx0 <= x1
            && cy1 >= y0 && cy1 <= y1)
        return TRUE;

    if (cx1 >= x0 && cx1 <= x1
            && cy0 >= y0 && cy0 <= y1)
        return TRUE;

    if (cx1 >= x0 && cx1 <= x1
            && cy1 >= y0 && cy1 <= y1)
        return TRUE;

    return FALSE;
}

int gli_get_selection(unsigned int x0, unsigned int y0,
        unsigned int x1, unsigned int y1,
        unsigned int *rx0, unsigned int *rx1)
{
    unsigned int row, upper, lower, above, below;
    int row_selected, found_left, found_right;
    int from_right, from_below, is_above, is_below;
    int cx0, cx1, cy0, cy1;
    int i;

    row = (y0 + y1)/2;
    upper = row - (row - y0)/2;
    lower = row + (y1 - row)/2;
    above = upper - (gli_leading)/2;
    below = lower + (gli_leading)/2;

    cx0 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x0
            : gli_mask->select.x1;

    cx1 = gli_mask->select.x0 < gli_mask->select.x1
            ? gli_mask->select.x1
            : gli_mask->select.x0;

    cy0 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y0
            : gli_mask->select.y1;

    cy1 = gli_mask->select.y0 < gli_mask->select.y1
            ? gli_mask->select.y1
            : gli_mask->select.y0;

    row_selected = FALSE;

    if ((cy0 >= upper && cy0 <= lower)
        || (cy1 >= upper && cy1 <= lower))
        row_selected = TRUE;

    if (row >= cy0 && row <= cy1)
        row_selected = TRUE;

    if (!row_selected)
        return FALSE;

    from_right = (gli_mask->select.x0 != cx0);
    from_below = (gli_mask->select.y0 != cy0);
    is_above = (above >= cy0 && above <= cy1);
    is_below = (below >= cy0 && below <= cy1);

    *rx0 = 0;
    *rx1 = 0;

    found_left = FALSE;
    found_right = FALSE;

    if (is_above && is_below)
    {
        *rx0 = x0;
        *rx1 = x1;
        found_left = TRUE;
        found_right = TRUE;
    }
    else if (!is_above && is_below)
    {
        if (from_below)
        {
            if (from_right)
            {
                *rx0 = cx0;
                *rx1 = x1;
                found_left = TRUE;
                found_right = TRUE;
            }
            else 
            {
                *rx0 = cx1;
                *rx1 = x1;
                found_left = TRUE;
                found_right = TRUE;
            }
        }
        else
        {
            if (from_right)
            {
                *rx0 = cx1;
                *rx1 = x1;
                found_left = TRUE;
                found_right = TRUE;
            }
            else
            {
                *rx1 = x1;
                found_right = TRUE;
            }
        }
    }
    else if (is_above && !is_below)
    {
        if (from_below)
        {
            if (from_right)
            {
                *rx0 = x0;
                *rx1 = cx1;
                found_left = TRUE;
                found_right = TRUE;
            }
            else
            {
                *rx0 = x0;
                *rx1 = cx0;
                found_left = TRUE;
                found_right = TRUE;
            }
        }
        else
        {
            if (from_right)
            {
                if (x0 > cx0)
                    return FALSE;
                *rx0 = x0;
                *rx1 = cx0;
                found_left = TRUE;
                found_right = TRUE;
            }
            else
            {
                *rx0 = x0;
                found_left = TRUE;
            }
        }
    }

    if (found_left && found_right)
        return TRUE;

    for (i = x0; i <= x1; i++)
    {
        if (i >= cx0 && i <= cx1)
        {
            if (!found_left)
            {
                *rx0 = i;
                found_left = TRUE;
                if (found_right)
                    return TRUE;
            }
            else
            {
                if (!found_right)
                    *rx1 = i;
            }
        }
    }

    if (rx0 && !rx1)
        *rx1 = x1;

    return (rx0 && rx1);
}

void gli_clipboard_copy(glui32 *buf, int len)
{
    winclipstore(buf, len);
    return;
}

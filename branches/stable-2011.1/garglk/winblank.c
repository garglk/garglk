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

/* This code is just as simple as you think. A blank window does *nothing*. */

window_blank_t *win_blank_create(window_t *win)
{
    window_blank_t *dwin = (window_blank_t *)malloc(sizeof(window_blank_t));
    dwin->owner = win;
    return dwin;
}

void win_blank_destroy(window_blank_t *dwin)
{
    dwin->owner = NULL;
    free(dwin);
}

void win_blank_rearrange(window_t *win, rect_t *box)
{
    window_blank_t *dwin = win->data;
    dwin->owner->bbox = *box;
}

void win_blank_redraw(window_t *win)
{
}

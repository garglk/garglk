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
#include <string.h>
#include "glk.h"
#include "garglk.h"

#ifdef GLK_MODULE_IMAGE

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
    return FALSE;
}

glui32 glk_image_draw_scaled(winid_t win, glui32 image,
    glsi32 val1, glsi32 val2, glui32 width, glui32 height)
{
    return FALSE;
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
    return FALSE;
}

void glk_window_flow_break(winid_t win)
{
}

void glk_window_erase_rect(winid_t win,
    glsi32 left, glsi32 top, glui32 width, glui32 height)
{
}

void glk_window_fill_rect(winid_t win, glui32 color,
    glsi32 left, glsi32 top, glui32 width, glui32 height)
{
}

void glk_window_set_background_color(winid_t win, glui32 color)
{
}

#endif


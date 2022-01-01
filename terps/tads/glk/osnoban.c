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

/* osansi4.c -- dummy banner interface */

#include "os.h"

void *os_banner_create(void *parent, int where, void *other, int wintype,
                       int align, int siz, int siz_units,
                       unsigned long style)
{
    return 0;
}

int os_banner_get_charwidth(void *banner_handle) { return 80; }
int os_banner_get_charheight(void *banner_handle) { return 1; }

int os_banner_getinfo(void *banner_handle, os_banner_info_t *info)
{
    info->align = OS_BANNER_ALIGN_TOP;
    info->style = 0;
    info->rows = 80;
    info->columns = 1;
    info->pix_width = 0;
    info->pix_height = 0;
    info->os_line_wrap = 1;
    return 1;
}

void os_banner_delete(void *banner_handle) {}
void os_banner_orphan(void *banner_handle) {}
void os_banner_clear(void *banner_handle) {}
void os_banner_disp(void *banner_handle, const char *txt, size_t len) {}
void os_banner_set_attr(void *banner_handle, int attr) {}
void os_banner_set_color(void *banner_handle, os_color_t fg, os_color_t bg) {}
void os_banner_set_screen_color(void *banner_handle, os_color_t color) {}
void os_banner_flush(void *banner_handle) {}
void os_banner_set_size(void *banner_handle, int siz, int siz_units, int is_advisory) {}
void os_banner_size_to_contents(void *banner_handle) {}
void os_banner_start_html(void *banner_handle) {}
void os_banner_end_html(void *banner_handle) {}
void os_banner_goto(void *banner_handle, int row, int col) {}
void os_banners_redraw(void) {}


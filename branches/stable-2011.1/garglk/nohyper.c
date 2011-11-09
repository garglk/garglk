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
#include "glk.h"
#include "garglk.h"

#ifdef GLK_MODULE_HYPERLINKS

void glk_set_hyperlink(glui32 linkval)
{
    gli_strict_warning("set_hyperlink: hyperlinks not supported.");
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
    gli_strict_warning("set_hyperlink_stream: hyperlinks not supported.");
}

void glk_request_hyperlink_event(winid_t win)
{
    gli_strict_warning("request_hyperlink_event: hyperlinks not supported.");
}

void glk_cancel_hyperlink_event(winid_t win)
{
    gli_strict_warning("cancel_hyperlink_event: hyperlinks not supported.");
}

#endif /* GLK_MODULE_HYPERLINKS */

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

/* glk-frotz.h
 *
 * Frotz os functions for the Glk library version 0.7.0.
 */

#include "frotz.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "glk.h"

extern int curr_status_ht;
extern int mach_status_ht;

extern winid_t gos_status;
extern winid_t gos_upper;
extern winid_t gos_lower;
extern winid_t gos_curwin;
extern int gos_linepending;
extern zchar *gos_linebuf;
extern winid_t gos_linewin;

extern schanid_t gos_channel;

/* From input.c.  */
bool is_terminator (zchar);

/* from glkstuff */
void gos_update_width(void);
void gos_update_height(void);
void gos_cancel_pending_line(void);
void reset_status_ht(void);


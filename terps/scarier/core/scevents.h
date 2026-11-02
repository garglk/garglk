/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Prototypes for scevents.cpp: events.
 */

#ifndef SCARIER_EVENTS_H
#define SCARIER_EVENTS_H

#include "sctypes.h"

/* Event functions. */
extern scr_bool evt_can_see_event_in_room (scr_gameref_t game,
                                           scr_int event, scr_int room);
extern void evt_tick_events (scr_gameref_t game);
extern void evt_check_events_started_by_task (scr_gameref_t game,
                                              scr_int task);
extern void evt_clear_ticked_events (scr_gameref_t game);
extern void evt_start_load_events (scr_gameref_t game);
extern void evt_finish_load_events (scr_gameref_t game);
extern void evt_debug_trace (scr_bool flag);
extern void evt_forget_game (const void *game);

#endif

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
 * Prototypes for scdebug.cpp: the debugger.
 */

#ifndef SCARIER_DEBUG_H
#define SCARIER_DEBUG_H

#include "sctypes.h"

/* Debugger interface. */
typedef struct scr_debugger_s *scr_debuggerref_t;
extern scr_bool debug_run_command (scr_gameref_t game,
                                  const scr_char *debug_command);
extern scr_bool debug_cmd_debugger (scr_gameref_t game);
extern void debug_set_enabled (scr_gameref_t game, scr_bool enable);
extern scr_bool debug_get_enabled (scr_gameref_t game);
extern void debug_game_started (scr_gameref_t game);
extern void debug_game_ended (scr_gameref_t game);
extern void debug_turn_update (scr_gameref_t game);

#endif

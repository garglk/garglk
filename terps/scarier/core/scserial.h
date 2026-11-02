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
 * Prototypes for scserial.cpp: game serialisation.
 */

#ifndef SCARIER_SERIAL_H
#define SCARIER_SERIAL_H

#include "sctypes.h"

/* Game serialization functions. */
extern void ser_set_raw_memo (scr_bool raw);
extern void ser_save_game (scr_gameref_t game,
                           scr_write_callbackref_t callback, void *opaque);
extern void ser_save_game_to_file (scr_gameref_t game,
                                   scr_write_callbackref_t callback,
                                   void *opaque);
extern scr_bool ser_save_game_prompted (scr_gameref_t game);
extern scr_bool ser_load_game (scr_gameref_t game,
                              scr_read_callbackref_t callback, void *opaque);
extern scr_bool ser_load_game_prompted (scr_gameref_t game);
extern void ser_forget_game (const void *game);

#endif

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
 * Prototypes for scinterf.cpp: the OS interface the engine prints and
 * reads through.
 */

#ifndef SCARIER_INTERF_H
#define SCARIER_INTERF_H

#include "sctypes.h"

/* OS interface functions. */
extern scr_bool if_get_trace_flag (scr_uint bitmask);
extern void if_print_string (const scr_char *string);
extern void if_print_debug (const scr_char *string);
extern void if_print_character (scr_char character);
extern void if_print_debug_character (scr_char character);
extern void if_print_tag (scr_int tag, const scr_char *arg);
extern void if_read_line (scr_char *buffer, scr_int length);
extern void if_read_debug (scr_char *buffer, scr_int length);
extern scr_bool if_confirm (scr_int type);
extern void *if_open_saved_game (scr_bool is_save);
extern void if_write_saved_game (void *opaque,
                                 const scr_byte *buffer, scr_int length);
extern scr_int if_read_saved_game (void *opaque,
                                  scr_byte *buffer, scr_int length);
extern void if_close_saved_game (void *opaque);
extern void if_display_hints (scr_gameref_t game);
extern void if_update_sound (const scr_char *filepath,
                             scr_int sound_offset,
                             scr_int sound_length, scr_bool is_looping);
extern void if_update_graphic (const scr_char *filepath,
                               scr_int graphic_offset,
                               scr_int graphic_length);

#endif

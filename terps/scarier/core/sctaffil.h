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
 * Prototypes for sctaffil.cpp: the TAF reader / decompressor.
 */

#ifndef SCARIER_TAFFIL_H
#define SCARIER_TAFFIL_H

#include "sctypes.h"

/* TAF file reader/decompressor enumerations, opaque typedef and functions. */
enum
{ TAF_VERSION_NONE = 0,
  TAF_VERSION_400 = 400,
  TAF_VERSION_390 = 390,
  TAF_VERSION_380 = 380,
  TAF_VERSION_370 = 370
};

typedef struct scr_taf_s *scr_tafref_t;
extern void taf_destroy (scr_tafref_t taf);
extern scr_tafref_t taf_create (scr_read_callbackref_t callback, void *opaque);
extern scr_tafref_t taf_create_tas (scr_read_callbackref_t callback,
                                   void *opaque);
extern scr_tafref_t taf_create_tas_raw (scr_read_callbackref_t callback,
                                        void *opaque);
extern void taf_first_line (scr_tafref_t taf);
extern const scr_char *taf_next_line (scr_tafref_t taf);
extern scr_bool taf_more_lines (scr_tafref_t taf);
extern scr_int taf_get_game_data_length (scr_tafref_t taf);
extern scr_int taf_get_version (scr_tafref_t taf);
extern void taf_obfuscate_reset (void);
extern void taf_runtime_rnd_reset (void);
extern double taf_runtime_rnd (void);
extern void taf_obfuscate_buffer (scr_byte *buffer, scr_int length);
extern scr_bool taf_debug_is_taf_string (scr_tafref_t taf, const void *addr);
extern void taf_debug_dump (scr_tafref_t taf);

#endif

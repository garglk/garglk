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
 * Private shared declarations for the TAF parser's three translation units:
 * sctafpar.cpp (the parser proper), sctafpar_fixups.cpp (the version fixup
 * handlers) and sctafpar_patches.cpp (the targeted game patches).  NOT a
 * public interface -- host code uses the parse_* entries in sctafpar.h.
 */

#ifndef SCARIER_SCTAFPAR_INTERNAL_H
#define SCARIER_SCTAFPAR_INTERNAL_H

#include <stddef.h>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "sctafpar.h"

/*
 * sctafpar.cpp: the properties bundle being written and the trace flag,
 * both set before parsing, and the key-stack helpers that read and write
 * properties relative to the current parse position.
 */
extern scr_prop_setref_t parse_bundle;
extern scr_bool parse_trace;

extern void parse_push_key (scr_vartype_t vt_key, scr_char type);
extern void parse_pop_key (void);
extern size_t parse_checked_multiply (scr_int count, size_t element_size);
extern size_t parse_checked_count (scr_int count);
extern scr_int parse_get_keyed_integer (const scr_char *key);
extern scr_bool parse_get_keyed_boolean (const scr_char *key);
extern const scr_char *parse_get_keyed_string (const scr_char *key);
extern scr_int parse_get_keyed_child_count (const scr_char *key);
extern void parse_put_keyed_integer (const scr_char *key, scr_int value);
extern void parse_put_keyed_string (const scr_char *key, const scr_char *value);
extern scr_bool parse_get_global_boolean (const scr_char *name);
extern scr_int parse_get_top_child_count (scr_prop_setref_t bundle,
                                          const scr_char *name);
extern void parse_descriptor (const scr_char *descriptor);

/* sctafpar_fixups.cpp: the per-version '|...|' fixup special handlers. */
extern void parse_fixup_v400 (const scr_char *fixup);
extern void parse_fixup_v390 (const scr_char *fixup);
extern void parse_fixup_v380 (const scr_char *fixup);
extern void parse_fixup_v370 (const scr_char *fixup);

/* sctafpar_patches.cpp: apply any targeted game patch to a parsed bundle. */
extern void parse_apply_game_patches (scr_prop_setref_t bundle);

#endif

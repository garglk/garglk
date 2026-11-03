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
 * Prototypes for sclocale.cpp: locale detection and the locale-aware
 * ctype functions.
 */

#ifndef SCARIER_LOCALE_H
#define SCARIER_LOCALE_H

#include "scprops.h"

/* Locale support, and locale-sensitive functions. */
extern void loc_detect_game_locale (scr_prop_setref_t bundle);
extern scr_bool loc_set_locale (const scr_char *name);
extern const scr_char *loc_get_locale (void);
extern scr_bool scr_isspace (scr_char character);
extern scr_bool scr_isdigit (scr_char character);
extern scr_bool scr_isalpha (scr_char character);
extern scr_char scr_toupper (scr_char character);
extern scr_char scr_tolower (scr_char character);
extern void loc_debug_dump (void);

#endif

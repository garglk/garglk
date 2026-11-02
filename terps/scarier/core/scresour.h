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
 * Prototypes for scresour.cpp: sound and graphics resources.
 */

#ifndef SCARIER_RESOUR_H
#define SCARIER_RESOUR_H

#include "sctypes.h"

/* Resource opaque typedef and control functions. */
typedef struct scr_resource_s *scr_resourceref_t;
extern scr_bool res_has_sound (scr_gameref_t game);
extern scr_bool res_has_graphics (scr_gameref_t game);
extern void res_clear_resource (scr_resourceref_t resource);
extern scr_bool res_compare_resource (scr_resourceref_t from,
                                     scr_resourceref_t with);
extern void res_handle_resource (scr_gameref_t game,
                                 const scr_char *partial_format,
                                 const scr_vartype_t vt_partial[]);
extern void res_sync_resources (scr_gameref_t game);
extern void res_cancel_resources (scr_gameref_t game);

#endif

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
 * Prototypes for screstrs.cpp: task restrictions.
 */

#ifndef SCARIER_RESTRS_H
#define SCARIER_RESTRS_H

#include "scprops.h"

/* Task restriction functions. */
extern scr_bool restr_object_in_state (scr_gameref_t game,
                                       scr_int object, scr_int var2);
extern scr_bool restr_eval_task_restrictions (scr_gameref_t game,
                                             scr_int task, scr_bool *pass,
                                             const scr_char **fail_message);
extern scr_bool restr_eval_task_restrictions_cached (scr_gameref_t game,
                                       scr_int task, scr_bool *pass,
                                       const scr_char **fail_message);
extern scr_int restr_cache_fallback (scr_gameref_t game, scr_int task,
                                     const scr_char **fail_message);
extern void restr_cache_reset (void);
extern void restr_forget_bundle (scr_prop_setref_t bundle);
extern void restr_debug_trace (scr_bool flag);

#endif

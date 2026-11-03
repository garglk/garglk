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
 * Prototypes for tools/scdump.cpp and the SCR_DEVIATION macros, which
 * compile to nothing without SCARIER_DUMP_TOOLS.
 */

#ifndef SCARIER_DUMP_H
#define SCARIER_DUMP_H

#include <stdio.h>
#include <stdlib.h>

#include "sctypes.h"

/*
 * Developer dump/trace tooling (tools/scdump.cpp); compiled only with
 * SCARIER_DUMP_TOOLS.
 */
#ifdef SCARIER_DUMP_TOOLS
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
extern void scr_dump_log (const char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
#else
extern void scr_dump_log (const char *format, ...);
#endif
extern void scr_dump_flush (void);
extern void scr_dump_structure_once (scr_gameref_t game);
extern void scr_dump_npc_trace (scr_gameref_t game);
extern scr_bool scr_tracing_deviations (void);
extern void scr_trace_deviation (const char *id, const char *format, ...);
extern void scr_trace_deviation_line (void);
#define SCR_TRACING_DEVIATIONS() scr_tracing_deviations ()
#define SCR_DEVIATION(...) \
  (scr_tracing_deviations () ? scr_trace_deviation (__VA_ARGS__) : (void) 0)

/*
 * The SCR_TRACE_... switches of the guarded trace blocks.  SCR_TRACE_ON
 * (name) is TRUE when the environment variable is set, and SCR_TRACE_IF
 * (name, format, ...) prints to stderr when it is.  Each use reads the
 * environment once, the first time it is reached, and keeps the answer.
 */
#define SCR_TRACE_ON(name_) \
  ([] () -> scr_bool { \
     static const scr_bool on_ = getenv (name_) != nullptr; \
     return on_; } ())
#define SCR_TRACE_IF(name_, ...) \
  (SCR_TRACE_ON (name_) ? (void) fprintf (stderr, __VA_ARGS__) : (void) 0)
#else
#define SCR_TRACING_DEVIATIONS() FALSE
#define SCR_DEVIATION(...) ((void) 0)
#define SCR_TRACE_ON(name_) FALSE
#define SCR_TRACE_IF(name_, ...) ((void) 0)
#endif

#endif

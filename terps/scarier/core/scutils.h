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
 * Prototypes for scutils.cpp: tracing and errors, allocation, the RNG
 * selectors and the small string helpers.
 */

#ifndef SCARIER_UTILS_H
#define SCARIER_UTILS_H

#include <string>
#include "sctypes.h"

/*
 * Small utility and wrapper functions.  For printf wrappers, try to apply
 * gcc printf argument checking; this code is cautious about applying the
 * checks.
 */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
extern void scr_trace (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
extern void scr_error (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
extern void scr_fatal (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2), __noreturn__));
#else
extern void scr_trace (const scr_char *format, ...);
extern void scr_error (const scr_char *format, ...);
#ifdef _MSC_VER
extern __declspec (noreturn) void scr_fatal (const scr_char *format, ...);
#else
extern void scr_fatal (const scr_char *format, ...);
#endif
#endif

#ifdef __cplusplus
/*
 * Exception thrown by scr_fatal() instead of abort()-ing the host.  Every
 * engine fatal -- allocation failure, malformed-game consistency check, etc. --
 * raises this, and the public entry points in scinterf.cpp catch it at the host
 * boundary and return a clean failure rather than crashing Spatterlight.  The
 * engine still leaks on this path (raw malloc'd state is not unwound -- see
 * P3), but the whole game session is torn down at the catch site anyway.
 */
struct scr_fatal_error
{
  std::string message;
  explicit scr_fatal_error (const std::string &m) : message (m) {}
};
#endif
extern void *scr_malloc (size_t size);
extern void *scr_realloc (void *pointer, size_t size);
extern void scr_free (void *pointer);
extern scr_char *scr_strdup (const std::string &string);
extern scr_char *scr_strdup (const scr_char *string);

#ifdef __cplusplus
#include <memory>
/*
 * RAII owner for a scr_malloc'd C string.  Frees with scr_free() at scope exit,
 * so an scr_fatal_error / run_loop_halt thrown between acquiring the buffer and
 * the end of its scope does not leak it.  This keeps the engine's char*
 * contract intact: .get() hands the raw pointer back to the C call sites and
 * to the pointer-aliasing logic that decides which buffer "wins".
 */
struct scr_free_deleter
{
  void operator () (void *pointer) const { scr_free (pointer); }
};
typedef std::unique_ptr<scr_char, scr_free_deleter> scr_owned_string;
#endif

extern void scr_set_congruential_random (void);
extern void scr_set_platform_random (void);
extern void scr_set_runner_random (void);
extern scr_bool scr_is_congruential_random (void);
extern scr_bool scr_is_runner_random (void);
extern scr_bool scr_get_runner_random_state (scr_uint words[4],
                                             scr_uint *draws);
extern void scr_set_runner_random_state (const scr_uint words[4],
                                         scr_uint draws);
extern double scr_vb_rnd (void);
extern void scr_seed_random (scr_uint new_seed);
extern scr_int scr_randomint (scr_int low, scr_int high);
extern scr_int scr_randomint_exclusive (scr_int low, scr_int high);
extern scr_bool scr_strempty (const scr_char *string);
extern scr_char *scr_trim_string (scr_char *string);
extern scr_char *scr_normalize_string (scr_char *string);
extern scr_char *scr_lowercase (scr_char *string);
extern std::string &scr_lowercase (std::string &string);
extern scr_bool scr_compare_word (const scr_char *string,
                                const scr_char *word, scr_int length);
extern scr_uint scr_hash (const scr_char *string);

#endif

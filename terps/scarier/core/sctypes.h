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
 * Engine-wide definitions shared by every module header: the version
 * macros, TRUE / FALSE, the fast setjmp pair, the vartype union, the
 * reader / writer callback types and the opaque game and hint handles.
 */

#ifndef SCARIER_TYPES_H
#define SCARIER_TYPES_H

#include <stddef.h>
#include "scarier.h"

/* Runtime version and emulated version, for %version% variable and so on. */
#ifndef SCARIER_VERSION
# define SCARIER_VERSION "1.5.0"
#endif
#ifndef SCARIER_PATCH_LEVEL
# define SCARIER_PATCH_LEVEL ""
#endif
#ifndef SCARIER_EMULATION
# define SCARIER_EMULATION 4046
#endif

/* True and false, unless already defined. */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/*
 * The string terminator and the whitespace set, one definition for every
 * module.  WHITESPACE is what strspn()/strcspn() and the trims work with;
 * scr_isspace() (sclocale.cpp) is the locale's own predicate and also
 * counts 0xa0, the non-breaking space, so the two are not interchangeable.
 */
static const scr_char NUL = '\0';
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";

/*
 * Fast non-local jumps.
 *
 * The expression evaluator, command parser, and restriction evaluator each arm
 * a setjmp() recovery point on essentially every turn, and the runner arms one
 * per game.  The C standard setjmp()/longjmp() save and restore the process
 * signal mask -- on macOS/BSD via a sigprocmask() (and sigaltstack()) syscall
 * pair on *every* call.  SCARE never alters the signal mask between a setjmp()
 * and its matching longjmp(), so that work is pure waste: profiling the
 * walkthrough corpus showed the kernel mask handling to be the single largest
 * steady-state cost, ahead even of the property-tree hot path.  _setjmp() /
 * _longjmp() (POSIX) give byte-identical control flow but skip the mask, for a
 * measured ~5-13% whole-run speedup with no output change.  Every set/longjmp
 * pair in the engine is file-local, so the swap is self-contained.
 */
#ifdef _WIN32
/*
 * MSVC and MinGW have no _setjmp/_longjmp pair; there the plain ones are what
 * the platform gives, signal mask and all.
 */
#define scr_setjmp(buffer)        setjmp (buffer)
#define scr_longjmp(buffer, val)  longjmp ((buffer), (val))
#else
#define scr_setjmp(buffer)        _setjmp (buffer)
#define scr_longjmp(buffer, val)  _longjmp ((buffer), (val))
#endif

/* Vartype typedef, supports relaxed typing. */
typedef union
{
  scr_int integer;
  scr_bool boolean;
  const scr_char *string;
  scr_char *mutable_string;
  void *voidp;
} scr_vartype_t;

/* Standard reader and writer callback function typedefs. */
typedef scr_int (*scr_read_callbackref_t) (void *, scr_byte *, scr_int);
typedef void (*scr_write_callbackref_t) (void *, const scr_byte *, scr_int);

/* Game state structure for modules that use it. */
typedef struct scr_game_s *scr_gameref_t;

/* Hint type definition, a thinly disguised pointer to task entry. */
typedef struct scr_taskstate_s *scr_hintref_t;

#endif

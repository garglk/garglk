/* vi: set ts=8:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * Written from a reading of the ADRIFT 5 Runner source as released in
 * FrankenDrift, to reproduce its behaviour; no code from it is used.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * ADRIFT 5 support for Scarier -- tiny string helpers shared by every a5 file.
 *
 * These four were independently copy-pasted into most of the a5*.cpp files (five
 * copies of streq alone) before being collected here.  Deliberately free of any
 * engine dependency -- only <string.h>/<ctype.h>/<string> -- so that the
 * self-contained translation units (a5sexpr.cpp, which knows nothing of the
 * model) can include it as readily as the runner does.
 */

#ifndef SCARIER_A5UTIL_H
#define SCARIER_A5UTIL_H

#include <ctype.h>
#include <string.h>

#include <string>

/* strcmp equality that tolerates NULL on either side (a NULL never matches,
   not even another NULL): the XML accessors return NULL for an absent element,
   and nearly every comparison in the engine is against such a value. */
static inline int
streq (const char *a, const char *b)
{
  return a != nullptr && b != nullptr && strcmp (a, b) == 0;
}

/* LCase(): the Adrift 5 runner lowercases input, keys and match text alike. */
static inline std::string
lower (const std::string &s)
{
  std::string o = s;
  for (char &c : o) c = (char) tolower ((unsigned char) c);
  return o;
}

/* Global.ToProper (bForceRestLower=True): upper-case the first character and
   lower-case the rest.  DisplayObjectChildren runs its on-object list through
   this, so "The Yellow Note, The Silver Gun and The Silver Bullets" renders as
   "The yellow note, the silver gun and the silver bullets". */
static inline std::string
to_proper (const std::string &s)
{
  std::string o = lower (s);
  if (!o.empty ()) o[0] = (char) toupper ((unsigned char) o[0]);
  return o;
}

/* Trim spaces and tabs from both ends (not newlines -- callers rely on those
   surviving). */
static inline std::string
trim_ws (const std::string &s)
{
  size_t b = s.find_first_not_of (" \t");
  if (b == std::string::npos)
    return "";
  size_t e = s.find_last_not_of (" \t");
  return s.substr (b, e - b + 1);
}

#endif

/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
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
 */

/*
 * Session records: the framing both ADRIFT engines use for the state a
 * Spatterlight autosave carries beyond the saved game -- the ADRIFT 4
 * session state (run_session_state() in runner/scrunner.cpp) and the
 * ADRIFT 5 parser continuation (a5run_pending_save() in adrift5/run/a5run.cpp).
 *
 * The encoding is a run of "<key> <length>\n<bytes>\n" records.  A value is
 * length-prefixed, so it may hold any bytes, newlines included.  Which keys
 * exist, and what an unknown or missing one means, is the caller's business:
 * this only writes a record and reads the next one back.
 *
 * Deliberately free of Glk and of both engines, and header-only, so that the
 * self-contained ADRIFT 5 harnesses pick it up without linking anything.
 */

#ifndef SESSREC_H
#define SESSREC_H

#include <stdlib.h>

#include <string>

/* Append one "<key> <length>\n<bytes>\n" record to `out`. */
static inline void
sessrec_put (std::string &out, const char *key, const std::string &value)
{
  out += key;
  out += ' ';
  out += std::to_string ((unsigned long) value.size ());
  out += '\n';
  out += value;
  out += '\n';
}

/* Read the record starting at `*pos` into `key` and `value`, and step `*pos`
   past it.  Returns false, with nothing changed, if the framing is broken:
   no key, no length line, or a length that does not land on the record's
   closing newline. */
static inline bool
sessrec_next (const std::string &state, size_t *pos,
              std::string *key, std::string *value)
{
  const size_t space = state.find (' ', *pos);
  const size_t eol = (space == std::string::npos)
                     ? space : state.find ('\n', space);
  unsigned long length;

  if (eol == std::string::npos || state.size () - eol < 2)
    return false;
  length = strtoul (state.c_str () + space + 1, nullptr, 10);
  if (length > state.size () - eol - 2
      || state[eol + 1 + length] != '\n')
    return false;

  key->assign (state, *pos, space - *pos);
  value->assign (state, eol + 1, length);
  *pos = eol + 1 + length + 1;
  return true;
}

#endif

/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"

#ifdef HEADER
typedef enum { E_INSTR, E_OBJECT, E_STACK,  E_MEMORY,  E_MATH,    E_STRING,
	       E_OUTPUT, E_SOUND, E_SYSTEM, E_VERSION, E_CORRUPT, E_SAVE,
	       E_DEBUG } errortypes;
#endif

static const char *errortypenames[] = {
  "instruc",
  "object",
  "stack",
  "memory",
  "math",
  "string",
  "output",
  "sound",
  "system",
  "version",
  "corrupt",
  "save",
  "debug"
};


/* These all feature loop detection, so if error reporting spawns another
   error, we won't infinite loop.  Hopefully.
*/

void n_show_warn(errortypes type, const char *message, offset number)
{
  if(!ignore_errors && allow_output) {
    showstuff("WARN", errortypenames[type], message, number);
  }
}

void n_show_port(errortypes type, const char *message, offset number)
{
  if(!ignore_errors && allow_output) {
    showstuff("PORT", errortypenames[type], message, number);
  }
}

void n_show_error(errortypes type, const char *message, offset number)
{
  if(!ignore_errors && allow_output) {
    showstuff("ERROR", errortypenames[type], message, number);
  }
}

/* showstuff calls n_show_fatal if it's looping uncontrollably, but it
   disables its loopiness detector so it can show this fatal error.  So
   n_show_fatal needs its own loop detection. */
void n_show_fatal(errortypes type, const char *message, offset number)
{
  static BOOL loopy = FALSE;
  if(loopy) {
    /* puts("loopy"); */
    glk_exit();
  }
  loopy = TRUE;
  showstuff("FATAL", errortypenames[type], message, number);
  loopy = FALSE;

  glk_exit();
}

void n_show_debug(errortypes type, const char *message, offset number)
{
#ifndef HIDE_DEBUG
  static BOOL loopy = FALSE;
  if(loopy)
    n_show_fatal(E_SYSTEM, "loopy debug error", 0);
  loopy = TRUE;
  showstuff("E_DEBUG", errortypenames[type], message, number);
  loopy = FALSE;
#endif
}

zword z_range_error(offset p)
{
  if(!ignore_errors) {
    static BOOL loopy = FALSE;
    if(loopy)
      n_show_fatal(E_SYSTEM, "loopy range error", 0);
    loopy = TRUE;
    showstuff("RANGE", errortypenames[E_MEMORY], "invalid memory access", p);
    loopy = FALSE;
  }
  return 0;
}

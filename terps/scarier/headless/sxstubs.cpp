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
 * The do-nothing os layer for the headless harnesses that never talk to the
 * interpreter loop's i/o (battle_test, scproj_test): output is discarded,
 * every prompt is answered "quit", and no file is ever opened.  It is what is
 * left of SCARE's test-suite stub layer, whose script runner (and with it the
 * handler hooks, the tracing and the sx utility functions) is not part of this
 * tree.
 */

#include <assert.h>
#include <stdio.h>

#include "core/scarier.h"


/*
 * os_*()
 *
 * Stub functions called by the interpreter core.
 */
void
os_print_tag (scr_int /* tag */, const scr_char * /* argument */)
{
}

void
os_print_string (const scr_char * /* string */)
{
}

void
os_print_string_debug (const scr_char * /* string */)
{
}

void
os_play_sound (const scr_char * /* filepath */, scr_int /* offset */,
               scr_int /* length */, scr_bool /* is_looping */)
{
}

void
os_stop_sound (void)
{
}

void
os_show_graphic (const scr_char * /* filepath */,
                 scr_int /* offset */, scr_int /* length */)
{
}

scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  assert (buffer && length > 4);
  snprintf (buffer, (size_t) length, "%s", "quit");
  return 1;
}

scr_bool
os_read_line_debug (scr_char *buffer, scr_int length)
{
  assert (buffer && length > 8);
  snprintf (buffer, (size_t) length, "%s", "continue");
  return 1;
}

scr_bool
os_confirm (scr_int /* type */)
{
  return 1;
}

void *
os_open_file (scr_bool /* is_save */)
{
  return nullptr;
}

scr_int
os_read_file (void * /* opaque */, scr_byte * /* buffer */,
              scr_int /* length */)
{
  return 0;
}

void
os_write_file (void * /* opaque */, const scr_byte * /* buffer */,
               scr_int /* length */)
{
}

void
os_close_file (void * /* opaque */)
{
}

void
os_display_hints (scr_game /* game */)
{
}

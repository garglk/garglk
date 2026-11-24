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
 * The OS interface: everything the engine prints, reads, saves and plays
 * goes through the if_ functions here, which call the host's os_ ones.
 * The public scr_ entry points are in runner/scapi.cpp.
 */

#include <assert.h>
#include <string.h>

#include "scarier.h"
#include "sctypes.h"
#include "scgamest_api.h"
#include "scinterf.h"


/* Assorted definitions and constants. */
static const scr_char NEWLINE = '\n';
static const scr_char CARRIAGE_RETURN = '\r';


/* Global tracing flags. */
static scr_uint if_trace_flags = 0;


/*
 * if_set_trace_flags()
 * if_get_trace_flag()
 *
 * Keep and retrieve the tracing flags; scr_set_trace_flags() is what hands
 * them out to the modules.
 */
void
if_set_trace_flags (scr_uint trace_flags)
{
  if_trace_flags = trace_flags;
}

scr_bool
if_get_trace_flag (scr_uint bitmask)
{
  return (if_trace_flags & bitmask) ? TRUE : FALSE;
}


/*
 * if_print_string_common()
 * if_print_string()
 * if_print_debug()
 * if_print_character_common()
 * if_print_character()
 * if_print_debug_character()
 * if_print_tag()
 *
 * Call OS-specific print function for the given arguments.
 */
static void
if_print_string_common (const scr_char *string,
                        void (*print_string_function) (const scr_char *))
{
  assert (string);

  if (string[0] != NUL)
    print_string_function (string);
}

void
if_print_string (const scr_char *string)
{
  if_print_string_common (string, os_print_string);
}

void
if_print_debug (const scr_char *string)
{
  if_print_string_common (string, os_print_string_debug);
}

static void
if_print_character_common (scr_char character,
                           void (*print_string_function) (const scr_char *))
{
  if (character != NUL)
    {
      scr_char buffer[2];

      buffer[0] = character;
      buffer[1] = NUL;
      print_string_function (buffer);
    }
}

void
if_print_character (scr_char character)
{
  if_print_character_common (character, os_print_string);
}

void
if_print_debug_character (scr_char character)
{
  if_print_character_common (character, os_print_string_debug);
}

void
if_print_tag (scr_int tag, const scr_char *arg)
{
  assert (arg);

  os_print_tag (tag, arg);
}


/*
 * Set for scr_note_autorestored(), and cleared by the first prompt that
 * follows it; see if_read_line_common().
 */
static scr_bool if_autorestored = FALSE;

void
if_note_autorestored (void)
{
  if_autorestored = TRUE;
}


/*
 * if_read_line_common()
 * if_read_line()
 * if_read_debug()
 *
 * Call OS-specific line read function.  Clean up any read data a little
 * before returning it to the caller.
 */
static void
if_read_line_common (scr_char *buffer, scr_int length,
                     scr_bool (*read_line_function) (scr_char *, scr_int))
{
  scr_bool is_line_available;
  scr_int last;

  assert (buffer && length > 0);

  /* Loop until valid player input is available. */
  do
    {
      /*
       * Space first with a blank line, and clear the buffer.  Not on the
       * first prompt after an autorestore: the restored transcript ends with
       * this blank line already, since the autosave was taken between it and
       * the prompt that follows (which the port skips for the same reason).
       * Printing it again would leave the input cursor a line below the
       * prompt it belongs to.
       */
      if (if_autorestored)
        if_autorestored = FALSE;
      else
        if_print_character ('\n');
      memset (buffer, NUL, length);

      is_line_available = read_line_function (buffer, length);
    }
  while (!is_line_available);

  /* Drop any trailing newline/return. */
  last = strlen (buffer) - 1;
  while (last >= 0
         && (buffer[last] == CARRIAGE_RETURN || buffer[last] == NEWLINE))
    buffer[last--] = NUL;
}

void
if_read_line (scr_char *buffer, scr_int length)
{
  if_read_line_common (buffer, length, os_read_line);
}

void
if_read_debug (scr_char *buffer, scr_int length)
{
  if_read_line_common (buffer, length, os_read_line_debug);
}


/*
 * if_confirm()
 *
 * Call OS-specific confirm function.
 */
scr_bool
if_confirm (scr_int type)
{
  return os_confirm (type);
}


/*
 * if_open_saved_game()
 * if_write_saved_game()
 * if_read_saved_game()
 * if_close_saved_game()
 *
 * Call OS-specific functions for saving and restoring games.
 */
void *
if_open_saved_game (scr_bool is_save)
{
  return os_open_file (is_save);
}

void
if_write_saved_game (void *opaque, const scr_byte *buffer, scr_int length)
{
  assert (buffer);

  os_write_file (opaque, buffer, length);
}

scr_int
if_read_saved_game (void *opaque, scr_byte *buffer, scr_int length)
{
  assert (buffer);

  return os_read_file (opaque, buffer, length);
}

void
if_close_saved_game (void *opaque)
{
  os_close_file (opaque);
}


/*
 * if_display_hints()
 *
 * Call OS-specific hint display function.
 */
void
if_display_hints (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  os_display_hints ((scr_game) game);
}


/*
 * if_update_sound()
 * if_update_graphic()
 *
 * Call OS-specific sound and graphic handler functions.
 */
void
if_update_sound (const scr_char *filename,
                 scr_int sound_offset, scr_int sound_length,
                 scr_bool is_looping)
{
  if (strlen (filename) > 0)
    os_play_sound (filename, sound_offset, sound_length, is_looping);
  else
    os_stop_sound ();
}

void
if_update_graphic (const scr_char *filename,
                   scr_int graphic_offset, scr_int graphic_length)
{
  os_show_graphic (filename, graphic_offset, graphic_length);
}

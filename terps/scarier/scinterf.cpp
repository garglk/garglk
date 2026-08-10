/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char NEWLINE = '\n';
static const scr_char CARRIAGE_RETURN = '\r';
static const scr_char NUL = '\0';

/* Global tracing flags. */
static scr_uint if_trace_flags = 0;


/*
 * if_initialize()
 *
 * First-time runtime checks for the overall interpreter.  This function
 * tries to ensure correct compile options.
 */
static void
if_initialize (void)
{
  static scr_bool initialized = FALSE;

  /* Only do checks on the first call. */
  if (!initialized)
    {
      /* Make a few quick checks on types and type sizes. */
      if (sizeof (scr_byte) != 1 || sizeof (scr_char) != 1)
        {
          scr_error ("if_initialize: sizeof scr_byte or scr_char"
                    " is not 1, check compile options\n");
        }
      else if (sizeof (scr_uint) < 4 || sizeof (scr_int) < 4)
        {
          scr_error ("if_initialize: sizeof scr_uint or scr_int"
                    " is not at least 4, check compile options\n");
        }
      else if (sizeof (scr_uint) > 8 || sizeof (scr_int) > 8)
        {
          scr_error ("if_initialize: sizeof scr_uint or scr_int"
                    " is more than 8, check compile options\n");
        }
	  else if (!((scr_uint) -1 > /* DISABLES CODE */ (0)))
        {
          scr_error ("if_initialize: scr_uint appears not to be unsigned,"
                    " check compile options\n");
        }

      initialized = TRUE;
    }
}


/*
 * if_bool()
 * scr_set_trace_flags()
 * if_get_trace_flag()
 *
 * Set and retrieve tracing flags.  Setting new values propagates the new
 * tracing setting to all modules that support it.
 */
static scr_bool
if_bool (scr_uint flag)
{
  return flag ? TRUE : FALSE;
}

void
scr_set_trace_flags (scr_uint trace_flags)
{
  if_initialize ();

  /* Save the value for queries. */
  if_trace_flags = trace_flags;

  /* Propagate tracing to modules that support it. */
  parse_debug_trace (if_bool (trace_flags & SCR_TRACE_PARSE));
  prop_debug_trace (if_bool (trace_flags & SCR_TRACE_PROPERTIES));
  var_debug_trace (if_bool (trace_flags & SCR_TRACE_VARIABLES));
  uip_debug_trace (if_bool (trace_flags & SCR_TRACE_PARSER));
  lib_debug_trace (if_bool (trace_flags & SCR_TRACE_LIBRARY));
  evt_debug_trace (if_bool (trace_flags & SCR_TRACE_EVENTS));
  npc_debug_trace (if_bool (trace_flags & SCR_TRACE_NPCS));
  obj_debug_trace (if_bool (trace_flags & SCR_TRACE_OBJECTS));
  task_debug_trace (if_bool (trace_flags & SCR_TRACE_TASKS));
  restr_debug_trace (if_bool (trace_flags & SCR_TRACE_TASKS));
  pf_debug_trace (if_bool (trace_flags & SCR_TRACE_PRINTFILTER));
}

scr_bool
if_get_trace_flag (scr_uint bitmask)
{
  return if_bool (if_trace_flags & bitmask);
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


/* Set by scr_note_autorestored(), and cleared by the first prompt that
   follows it; see if_read_line_common(). */
static scr_bool if_autorestored = FALSE;


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
      /* Space first with a blank line, and clear the buffer.  Not on the
         first prompt after an autorestore: the restored transcript ends with
         this blank line already, since the autosave was taken between it and
         the prompt that follows (which the port skips for the same reason).
         Printing it again would leave the input cursor a line below the
         prompt it belongs to. */
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


/*
 * scr_scarier_version()
 * scr_scarier_emulation()
 *
 * Return a version string and Adrift emulation level.
 */
const scr_char *
scr_scarier_version (void)
{
  if_initialize ();
  return "SCARIER " SCARIER_VERSION SCARIER_PATCH_LEVEL;
}

scr_int
scr_scarier_emulation (void)
{
  if_initialize ();
  return SCARIER_EMULATION;
}


/*
 * if_file_read_callback()
 * if_file_write_callback()
 *
 * Standard FILE* reader and writer callback for constructing callback-style
 * calls from filename and stream variants.
 */
static scr_int
if_file_read_callback (void *opaque, scr_byte *buffer, scr_int length)
{
  FILE *stream = (FILE *) opaque;
  scr_int bytes;

  bytes = fread (buffer, 1, length, stream);
  if (ferror (stream))
    scr_error ("if_file_read_callback: warning: read error\n");

  return bytes;
}

static void
if_file_write_callback (void *opaque, const scr_byte *buffer, scr_int length)
{
  FILE *stream = (FILE *) opaque;

  fwrite (buffer, 1, length, stream);
  if (ferror (stream))
    scr_error ("if_file_write_callback: warning: write error\n");
}


/*
 * scr_game_from_filename()
 * scr_game_from_stream()
 * scr_game_from_callback()
 *
 * Called by the OS-specific layer to create a run context.  The _filename()
 * and _stream() variants are adapters for run_create().
 */
/* Defined below; logs a fatal engine error caught at the host boundary. */
static void if_report_fatal (const scr_char *function_name,
                             const scr_fatal_error &error);

scr_game
scr_game_from_filename (const scr_char *filename)
{
  FILE *stream;
  scr_game game;

  if_initialize ();
  if (!filename)
    {
      scr_error ("scr_game_from_filename: NULL filename\n");
      return NULL;
    }

  stream = fopen (filename, "rb");
  if (!stream)
    {
      scr_error ("scr_game_from_filename: fopen error\n");
      return NULL;
    }

  game = NULL;
  try
    {
      game = run_create (if_file_read_callback, stream);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_game_from_filename", error);
    }
  fclose (stream);

  return game;
}

scr_game
scr_game_from_stream (FILE *stream)
{
  if_initialize ();
  if (!stream)
    {
      scr_error ("scr_game_from_stream: NULL stream\n");
      return NULL;
    }

  try
    {
      return run_create (if_file_read_callback, stream);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_game_from_stream", error);
      return NULL;
    }
}

scr_game
scr_game_from_callback (scr_int (*callback) (void *, scr_byte *, scr_int),
                       void *opaque)
{
  if_initialize ();
  if (!callback)
    {
      scr_error ("scr_game_from_callback: NULL callback\n");
      return NULL;
    }

  try
    {
      return run_create (callback, opaque);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_game_from_callback", error);
      return NULL;
    }
}


/*
 * if_game_error()
 *
 * Common function to verify that the game passed in to functions below
 * is a valid game.  Returns TRUE on game error, FALSE if okay.
 */
static scr_bool
if_game_error (scr_gameref_t game, const scr_char *function_name)
{
  /* Check for invalid game -- null pointer or bad magic. */
  if (!gs_is_game_valid (game))
    {
      if (game)
        scr_error ("%s: invalid game\n", function_name);
      else
        scr_error ("%s: NULL game\n", function_name);
      return TRUE;
    }

  /* No game error. */
  return FALSE;
}


/*
 * if_report_fatal()
 *
 * Log a fatal engine error caught at the host boundary.  scr_fatal() now throws
 * scr_fatal_error instead of abort()-ing, so the public entry points below
 * catch it and return a clean failure (NULL / FALSE / no-op) rather than
 * crashing the host application on a corrupt or pathological game.  The engine
 * leaks its raw-malloc'd state on this path (it is not exception-safe -- see the
 * RAII phase, P3), but the whole game session is being torn down here anyway.
 */
static void
if_report_fatal (const scr_char *function_name, const scr_fatal_error &error)
{
  scr_error ("%s: fatal: %s", function_name, error.message.c_str ());
}


/*
 * scr_interpret_game()
 * scr_restart_game()
 * scr_save_game()
 * scr_load_game()
 * scr_undo_game_turn()
 * scr_quit_game()
 *
 * Called by the OS-specific layer to run a game loaded into a run context,
 * and to quit the interpreter on demand, if required.  scr_quit_game()
 * is implemented as a longjmp(), so never returns to the caller --
 * instead, the program behaves as if scr_interpret_game() had returned.
 * scr_load_game() will longjmp() if the restore is successful (thus
 * behaving like scr_restart_game()), but will return if the game could not
 * be restored.  scr_undo_game_turn() behaves like scr_load_game().
 */
void
scr_interpret_game (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_interpret_game"))
    return;

  try
    {
      run_interpret (game_);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_interpret_game", error);
    }
}

void
scr_restart_game (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_restart_game"))
    return;

  try
    {
      run_restart (game_);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_restart_game", error);
    }
}

scr_bool
scr_save_game (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_save_game"))
    return FALSE;

  try
    {
      return run_save_prompted (game_);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_save_game", error);
      return FALSE;
    }
}

scr_bool
scr_load_game (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_load_game"))
    return FALSE;

  try
    {
      return run_restore_prompted (game_);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_load_game", error);
      return FALSE;
    }
}

scr_bool
scr_undo_game_turn (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_undo_game_turn"))
    return FALSE;

  try
    {
      return run_undo (game_);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_undo_game_turn", error);
      return FALSE;
    }
}

void
scr_quit_game (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_quit_game"))
    return;

  run_quit (game_);
}


/*
 * scr_save_game_to_filename()
 * scr_save_game_to_stream()
 * scr_save_game_to_callback()
 * scr_load_game_from_filename()
 * scr_load_game_from_stream()
 * scr_load_game_from_callback()
 *
 * Low level game saving and loading functions.  The normal scr_save_game()
 * and scr_load_game() functions act exactly as the "save" and "restore"
 * game commands, in that they prompt the user for a stream to write or read.
 * These alternative forms allow the caller to directly specify the data
 * streams.
 */
scr_bool
scr_save_game_to_filename (scr_game game, const scr_char *filename)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  FILE *stream;

  if (if_game_error (game_, "scr_save_game_to_filename"))
    return FALSE;

  if (!filename)
    {
      scr_error ("scr_save_game_to_filename: NULL filename\n");
      return FALSE;
    }

  stream = fopen (filename, "wb");
  if (!stream)
    {
      scr_error ("scr_save_game_to_filename: fopen error\n");
      return FALSE;
    }

  scr_bool status = TRUE;
  try
    {
      run_save (game_, if_file_write_callback, stream);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_save_game_to_filename", error);
      status = FALSE;
    }
  fclose (stream);

  return status;
}

void
scr_save_game_to_stream (scr_game game, FILE *stream)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_save_game_to_stream"))
    return;

  if (!stream)
    {
      scr_error ("scr_save_game_to_stream: NULL stream\n");
      return;
    }

  try
    {
      run_save (game_, if_file_write_callback, stream);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_save_game_to_stream", error);
    }
}

void
scr_save_game_to_callback (scr_game game,
                          void (*callback) (void *, const scr_byte *, scr_int),
                          void *opaque)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_save_game_to_callback"))
    return;

  if (!callback)
    {
      scr_error ("scr_save_game_to_callback: NULL callback\n");
      return;
    }

  try
    {
      run_save (game_, callback, opaque);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_save_game_to_callback", error);
    }
}

scr_bool
scr_load_game_from_filename (scr_game game, const scr_char *filename)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  FILE *stream;
  scr_bool status = FALSE;   /* default when run_restore throws scr_fatal_error */

  if (if_game_error (game_, "scr_load_game_from_filename"))
    return FALSE;

  if (!filename)
    {
      scr_error ("scr_load_game_from_filename: NULL filename\n");
      return FALSE;
    }

  stream = fopen (filename, "rb");
  if (!stream)
    {
      scr_error ("scr_load_game_from_filename: fopen error\n");
      return FALSE;
    }

  try
    {
      status = run_restore (game_, if_file_read_callback, stream);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_load_game_from_filename", error);
    }
  fclose (stream);

  return status;
}

scr_bool
scr_load_game_from_stream (scr_game game, FILE *stream)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_load_game_from_stream"))
    return FALSE;

  if (!stream)
    {
      scr_error ("scr_load_game_from_stream: NULL stream\n");
      return FALSE;
    }

  try
    {
      return run_restore (game_, if_file_read_callback, stream);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_load_game_from_stream", error);
      return FALSE;
    }
}

scr_bool
scr_load_game_from_callback (scr_game game,
                            scr_int (*callback) (void *, scr_byte *, scr_int),
                            void *opaque)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_load_game_from_callback"))
    return FALSE;

  if (!callback)
    {
      scr_error ("scr_load_game_from_callback: NULL callback\n");
      return FALSE;
    }

  try
    {
      return run_restore (game_, callback, opaque);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_load_game_from_callback", error);
      return FALSE;
    }
}


/*
 * scr_save_undo_game_to_callback()
 * scr_load_undo_game_from_callback()
 *
 * Serialize and deserialize the one-turn-back undo buffer (game->undo, the
 * newest undo point, ahead of the older ones spilled into the memo ring).
 * Used by the Spatterlight autosave so the full undo chain survives a quit
 * and relaunch.  Saving returns FALSE, and writes nothing, when no undo
 * buffer is available; loading marks the buffer available on success.
 */
scr_bool
scr_save_undo_game_to_callback (scr_game game,
                                void (*callback) (void *, const scr_byte *,
                                                  scr_int),
                                void *opaque)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_save_undo_game_to_callback"))
    return FALSE;

  if (!callback)
    {
      scr_error ("scr_save_undo_game_to_callback: NULL callback\n");
      return FALSE;
    }
  if (!game_->undo_available)
    return FALSE;

  try
    {
      ser_save_game (game_->undo, callback, opaque);
      return TRUE;
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_save_undo_game_to_callback", error);
      return FALSE;
    }
}

scr_bool
scr_load_undo_game_from_callback (scr_game game,
                                  scr_int (*callback) (void *, scr_byte *,
                                                       scr_int),
                                  void *opaque)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool status = FALSE;

  if (if_game_error (game_, "scr_load_undo_game_from_callback"))
    return FALSE;

  if (!callback)
    {
      scr_error ("scr_load_undo_game_from_callback: NULL callback\n");
      return FALSE;
    }

  try
    {
      status = ser_load_game (game_->undo, callback, opaque);
    }
  catch (const scr_fatal_error &error)
    {
      if_report_fatal ("scr_load_undo_game_from_callback", error);
      return FALSE;
    }

  /*
   * Re-point the undo game's sibling references.  In normal play every
   * gs_copy() into the undo buffer comes from game->temporary, whose
   * temporary/undo pointers were copied from the main game -- so the undo
   * game's own undo field points at itself.  ser_load_game() instead
   * preserved whatever the freshly created undo game held (NULL), and a
   * later "undo" would gs_copy() that NULL into the main game's pointers.
   */
  if (status)
    {
      game_->undo->temporary = game_->temporary;
      game_->undo->undo = game_->undo;
      game_->undo->undo_available = FALSE;
      /* The buffered state was captured mid-play, and lib_cmd_undo's
       * gs_copy() propagates is_running into the main game -- but
       * ser_load_game() leaves it cleared, which would end the session on
       * the first undo. */
      game_->undo->is_running = TRUE;
    }
  game_->undo_available = status;
  return status;
}


/*
 * scr_note_resources_synced()
 *
 * Consider the game's currently requested sound and graphic as already
 * playing and displayed, so the next res_sync_resources() call starts and
 * stops nothing.  Used after a Spatterlight autorestore: the app resumes an
 * interrupted sound from its own GUI snapshot and restores the graphics
 * window pixels itself, so the interpreter must not replay them.
 */
void
scr_note_resources_synced (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_note_resources_synced"))
    return;

  game_->playing_sound = game_->requested_sound;
  game_->displayed_graphic = game_->requested_graphic;
  game_->stop_sound = FALSE;
}


/*
 * scr_note_autorestored()
 *
 * Note that the display has been rebuilt from a snapshot taken at a prompt,
 * so that the next prompt does not re-print the blank line that spaces it
 * from the previous turn's text; that line is already in the restored
 * transcript.  Used after a Spatterlight autorestore, where the port skips
 * the prompt itself for the same reason.
 */
void
scr_note_autorestored (void)
{
  if_autorestored = TRUE;
}


/*
 * scr_free_game()
 *
 * Called by the OS-specific layer to free run context memory.
 */
void
scr_free_game (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_free_game"))
    return;

  run_destroy (game_);
}


/*
 * scr_is_game_running()
 * scr_get_game_name()
 * scr_get_game_author()
 * scr_get_game_compile_date()
 * scr_get_game_turns()
 * scr_get_game_score()
 * scr_get_game_max_score()
 * scr_get_game_room ()
 * scr_get_game_status_line ()
 * scr_get_game_preferred_font ()
 * scr_get_game_bold_room_names()
 * scr_get_game_verbose()
 * scr_get_game_notify_score_change()
 * scr_has_game_completed()
 * scr_is_game_undo_available()
 *
 * Return a few attributes of a game.
 */
scr_bool
scr_is_game_running (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_is_game_running"))
    return FALSE;

  return run_is_running (game_);
}

scr_bool
scr_does_command_match (scr_game game, const scr_char *string)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_does_command_match"))
    return FALSE;
  if (!string)
    return FALSE;

  return run_does_command_match (game_, string);
}

const scr_char *
scr_get_game_name (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_char *retval;

  if (if_game_error (game_, "scr_get_game_name"))
    return "[invalid game]";

  run_get_attributes (game_, &retval,
                      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL);
  return retval;
}

const scr_char *
scr_get_game_author (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_char *retval;

  if (if_game_error (game_, "scr_get_game_author"))
    return "[invalid game]";

  run_get_attributes (game_, NULL, &retval,
                      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL);
  return retval;
}

const scr_char *
scr_get_game_compile_date (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_char *retval;

  if (if_game_error (game_, "scr_get_game_compile_date"))
    return "[invalid game]";

  run_get_attributes (game_, NULL, NULL, &retval,
                      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  return retval;
}

scr_int
scr_get_game_turns (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_int retval;

  if (if_game_error (game_, "scr_get_game_turns"))
    return 0;

  run_get_attributes (game_, NULL, NULL, NULL, &retval,
                      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  return retval;
}

scr_int
scr_get_game_score (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_int retval;

  if (if_game_error (game_, "scr_get_game_score"))
    return 0;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, &retval,
                      NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  return retval;
}

scr_int
scr_get_game_max_score (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_int retval;

  if (if_game_error (game_, "scr_get_game_max_score"))
    return 0;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, &retval,
                      NULL, NULL, NULL, NULL, NULL, NULL);
  return retval;
}

const scr_char *
scr_get_game_room (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_char *retval;

  if (if_game_error (game_, "scr_get_game_room"))
    return "[invalid game]";

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, &retval,
                      NULL, NULL, NULL, NULL, NULL);
  return retval;
}

const scr_char *
scr_get_game_status_line (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_char *retval;

  if (if_game_error (game_, "scr_get_game_status_line"))
    return "[invalid game]";

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      &retval, NULL, NULL, NULL, NULL);
  return retval;
}

const scr_char *
scr_get_game_preferred_font (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_char *retval;

  if (if_game_error (game_, "scr_get_game_preferred_font"))
    return "[invalid game]";

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, &retval, NULL, NULL, NULL);
  return retval;
}

scr_bool
scr_get_game_bold_room_names (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool retval;

  if (if_game_error (game_, "scr_get_game_bold_room_names"))
    return FALSE;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, &retval, NULL, NULL);
  return retval;
}

scr_bool
scr_get_game_verbose (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool retval;

  if (if_game_error (game_, "scr_get_game_verbose"))
    return FALSE;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, NULL, &retval, NULL);
  return retval;
}

scr_bool
scr_get_game_notify_score_change (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool retval;

  if (if_game_error (game_, "scr_get_game_notify_score_change"))
    return FALSE;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, NULL, NULL, &retval);
  return retval;
}

scr_bool
scr_has_game_completed (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_has_game_completed"))
    return FALSE;

  return run_has_completed (game_);
}

scr_bool
scr_is_game_undo_available (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_is_game_undo_available"))
    return FALSE;

  return run_is_undo_available (game_);
}


/*
 * scr_set_game_bold_room_names()
 * scr_set_game_verbose()
 * scr_set_game_notify_score_change()
 *
 * Set a few attributes of a game.
 */
void
scr_set_game_bold_room_names (scr_game game, scr_bool flag)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool bold, verbose, notify;

  if (if_game_error (game_, "scr_set_game_bold_room_names"))
    return;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, &bold, &verbose, &notify);
  run_set_attributes (game_, flag, verbose, notify);
}

void
scr_set_game_verbose (scr_game game, scr_bool flag)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool bold, verbose, notify;

  if (if_game_error (game_, "scr_set_game_verbose"))
    return;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, &bold, &verbose, &notify);
  run_set_attributes (game_, bold, flag, notify);
}

void
scr_set_game_notify_score_change (scr_game game, scr_bool flag)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  scr_bool bold, verbose, notify;

  if (if_game_error (game_, "scr_set_game_notify_score_change"))
    return;

  run_get_attributes (game_, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                      NULL, NULL, &bold, &verbose, &notify);
  run_set_attributes (game_, bold, verbose, flag);
}


/*
 * scr_get_game_capacity_recompute()
 * scr_set_game_capacity_recompute()
 *
 * Query and set how the player's carried load is accounted for.  When FALSE
 * (the default) SCARIER mirrors the real ADRIFT Runner, keeping a running total
 * updated on take/drop; when TRUE it recomputes the load afresh from currently
 * held objects on each check (legacy SCARIER behaviour).
 */
scr_bool
scr_get_game_capacity_recompute (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_get_game_capacity_recompute"))
    return FALSE;

  return game_->capacity_recompute;
}

void
scr_set_game_capacity_recompute (scr_game game, scr_bool flag)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_set_game_capacity_recompute"))
    return;

  game_->capacity_recompute = flag;
}


/*
 * scr_does_game_use_sounds()
 * scr_does_game_use_graphics()
 *
 * Indicate the game's use of resources.
 */
scr_bool
scr_does_game_use_sounds (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_does_game_use_sounds"))
    return FALSE;

  return res_has_sound (game_);
}

scr_bool
scr_does_game_use_graphics (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_does_game_use_graphics"))
    return FALSE;

  return res_has_graphics (game_);
}


/*
 * scr_get_first_game_hint()
 * scr_get_next_game_hint()
 * scr_get_game_hint_question()
 * scr_get_game_subtle_hint()
 * scr_get_game_sledgehammer_hint()
 *
 * Iterate currently available hints, and return strings for a hint.
 */
scr_game_hint
scr_get_first_game_hint (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_get_first_game_hint"))
    return NULL;

  return run_hint_iterate (game_, NULL);
}

scr_game_hint
scr_get_next_game_hint (scr_game game, scr_game_hint hint)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_hintref_t hint_ = (scr_hintref_t) hint;

  if (if_game_error (game_, "scr_get_next_game_hint"))
    return NULL;
  if (!hint_)
    {
      scr_error ("scr_get_next_game_hint: NULL hint\n");
      return NULL;
    }

  return run_hint_iterate (game_, hint_);
}

const scr_char *
scr_get_game_hint_question (scr_game game, scr_game_hint hint)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_hintref_t hint_ = (scr_hintref_t) hint;

  if (if_game_error (game_, "scr_get_game_hint_question"))
    return NULL;
  if (!hint_)
    {
      scr_error ("scr_get_game_hint_question: NULL hint\n");
      return NULL;
    }

  return run_get_hint_question (game_, hint_);
}

const scr_char *
scr_get_game_subtle_hint (scr_game game, scr_game_hint hint)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_hintref_t hint_ = (scr_hintref_t) hint;

  if (if_game_error (game_, "scr_get_game_subtle_hint"))
    return NULL;
  if (!hint_)
    {
      scr_error ("scr_get_game_subtle_hint: NULL hint\n");
      return NULL;
    }

  return run_get_subtle_hint (game_, hint_);
}

const scr_char *
scr_get_game_unsubtle_hint (scr_game game, scr_game_hint hint)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;
  const scr_hintref_t hint_ = (scr_hintref_t) hint;

  if (if_game_error (game_, "scr_get_game_unsubtle_hint"))
    return NULL;
  if (!hint_)
    {
      scr_error ("scr_get_game_unsubtle_hint: NULL hint\n");
      return NULL;
    }

  return run_get_unsubtle_hint (game_, hint_);
}


/*
 * scr_set_game_debugger_enabled()
 * scr_is_game_debugger_enabled()
 * scr_run_game_debugger_command()
 *
 * Enable, disable, and query game debugging, and run a single debug command.
 */
void
scr_set_game_debugger_enabled (scr_game game, scr_bool flag)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_set_game_debugger_enabled"))
    return;

  debug_set_enabled (game_, flag);
}

scr_bool
scr_get_game_debugger_enabled (scr_game game)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_get_game_debugger_enabled"))
    return FALSE;

  return debug_get_enabled (game_);
}

scr_bool
scr_run_game_debugger_command (scr_game game, const scr_char *debug_command)
{
  const scr_gameref_t game_ = (scr_gameref_t) game;

  if (if_game_error (game_, "scr_run_game_debugger_command"))
    return FALSE;

  return debug_run_command (game_, debug_command);
}


/*
 * scr_set_locale()
 * scr_get_locale()
 *
 * Set the interpreter locale, and get the currently set locale.
 */
scr_bool
scr_set_locale (const scr_char *name)
{
  if (!name)
    {
      scr_error ("scr_set_locale: NULL name\n");
      return FALSE;
    }

  return loc_set_locale (name);
}

const scr_char *
scr_get_locale (void)
{
  return loc_get_locale ();
}


/*
 * scr_set_portable_random()
 * scr_reseed_random_sequence()
 *
 * Turn portable random number generation on and off, and supply a new seed
 * for random number generators.
 */
void
scr_set_portable_random (scr_bool flag)
{
  if (flag)
    scr_set_congruential_random ();
  else
    scr_set_platform_random ();
}

void
scr_reseed_random_sequence (scr_uint new_seed)
{
  if (new_seed == 0)
    {
      scr_error ("scr_reseed_random_sequence: new_seed may not be 0\n");
      return;
    }

  scr_seed_random (new_seed);
}


/*
 * scr_set_combat_assist()
 *
 * Enable or disable the optional Battle-System "combat assist" mode.  When on,
 * games that leave every character's Accuracy and Agility unconfigured (0) get
 * an automatic hit roll, so their otherwise-dead combat plays out on the
 * author's intended strength-vs-defence basis.  Off by default; opt-in only, as
 * it deliberately diverges from the reference Runner.  Games that configure
 * accuracy/agility are unaffected.
 */
void
scr_set_combat_assist (scr_bool flag)
{
  battle_set_combat_assist (flag);
}


/*
 * scr_get_combat_assist()
 *
 * Return the current combat-assist setting (see scr_set_combat_assist()).
 */
scr_bool
scr_get_combat_assist (void)
{
  return battle_get_combat_assist ();
}


/*
 * scr_set_move_assist()
 *
 * Enable or disable the optional "move assist" mode.  A few native-4.0 games
 * were authored with a move task action's "To:" combo left at VB's default -1
 * (the destination room sitting in Var3); the reference Runner silently ignores
 * such a move, which in e.g. To Hell & Beyond leaves the game unwinnable.  When
 * on, an unset (-1) move whose Var3 names a real room is honoured as "to room".
 * Off by default; opt-in only, as it deliberately diverges from the Runner.
 */
void
scr_set_move_assist (scr_bool flag)
{
  task_set_move_assist (flag);
}


/*
 * scr_get_move_assist()
 *
 * Return the current move-assist setting (see scr_set_move_assist()).
 */
scr_bool
scr_get_move_assist (void)
{
  return task_get_move_assist ();
}

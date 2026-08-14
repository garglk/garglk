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
 * o This module represents just about the simplest platform input/output
 *   code possible for SCARIER.  Actually, it could be simplified still further
 *   by abandoning attempts to word wrap at 78 columns of text, and by
 *   ignoring all tags altogether, though this may stop some games playing
 *   quite like they should.
 *
 * o Feel free to use this code as a starting point for a platform port.
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"

enum { FALSE = 0, TRUE = !FALSE };

static scr_char line_buffer[79];
static scr_int line_length = 0;

static const scr_char *game_file;
static scr_game game;


/*
 * full_flush()
 * partial_flush()
 * append_character()
 */
static void
full_flush (void)
{
  if (line_length > 0)
    {
      fwrite (line_buffer, 1, line_length, stdout);
      line_length = 0;
    }
  fflush (stdout);
}

static void
partial_flush (void)
{
  if (line_length > 0)
    {
      const scr_char *line_break;

      line_buffer[line_length] = '\0';
      line_break = strrchr (line_buffer, ' ');
      if (line_break)
        {
          fwrite (line_buffer, 1, line_break - line_buffer, stdout);
          memmove (line_buffer, line_break + 1, strlen (line_break + 1) + 1);
          line_length = strlen (line_buffer);
        }
      else
        full_flush ();
    }
  fflush (stdout);
}

static void
append_character (scr_char c)
{
  if (c == '\n')
    {
      full_flush ();
      putchar ('\n');
    }
  else
    {
      line_buffer[line_length++] = c;
      if (line_length >= (scr_int) sizeof (line_buffer) - 1)
        {
          partial_flush ();
          putchar ('\n');
        }
    }
}


/*
 * os_print_tag()
 * os_print_string()
 * os_print_string_debug()
 */
void
os_print_tag (scr_int tag, const scr_char *argument)
{
  scr_int index_;
  const scr_char *unused;
  unused = argument;

  switch (tag)
    {
    case SCR_TAG_CLS:
      for (index_ = 0; index_ < 25; index_++)
        append_character ('\n');
      break;

    case SCR_TAG_CENTER:
    case SCR_TAG_RIGHT:
    case SCR_TAG_ENDCENTER:
    case SCR_TAG_ENDRIGHT:
      if (line_length > 0)
        append_character ('\n');
      break;

    case SCR_TAG_WAITKEY:
      {
        scr_char dummy[256];
        full_flush ();
        /*
         * A "press a key" pause.  Normally we consume one line of input to
         * stand in for the keypress.  For scripted/headless walkthrough
         * derivation, SCR_SKIP_WAITKEY=1 makes these pauses transparent so a
         * solution file maps one line to one game command regardless of how
         * many <waitkey> tags the game's text embeds.
         */
        /*
         * Derivation aid: SCR_MARK_WAITKEY=1 notes each pause on stderr (after
         * the flush above, so with 2>&1 the marker lands in transcript order).
         * Combined with SCR_SKIP_WAITKEY=1 -- which keeps the command list in
         * sync -- that turns "how many blank lines does this solution need, and
         * where?" into a read rather than a bisection.
         */
        if (getenv ("SCR_MARK_WAITKEY"))
          {
            fflush (stdout);
            fprintf (stderr, "[WAITKEY]\n");
          }
        if (getenv ("SCR_SKIP_WAITKEY"))
          break;
        if (!feof (stdin))
          fgets (dummy, sizeof (dummy), stdin);
        break;
      }
    }
}

void
os_print_string (const scr_char *string)
{
  scr_int index_;

  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      if (string[index_] == '\t')
        os_print_string ("        ");
      else
        append_character (string[index_]);
    }
}

void
os_print_string_debug (const scr_char *string)
{
  os_print_string (string);
}


/*
 * os_play_sound()
 * os_stop_sound()
 * os_show_graphic()
 */
void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  const scr_char *unused1;
  scr_int unused2, unused3;
  scr_bool unused4;
  unused1 = filepath;
  unused2 = offset;
  unused3 = length;
  unused4 = is_looping;
}

void
os_stop_sound (void)
{
}

void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
#ifdef LINUX_GRAPHICS
  const scr_char *unused1;
  unused1 = filepath;

  if (length > 0 && strlen (game_file) < 768)
    {
      scr_char buffer[1024];

      sprintf (buffer,
               "dd if=%s ibs=1c skip=%ld count=%ld obs=100k"
               " of=/tmp/scarier.jpg 2>/dev/null", game_file, offset, length);
      system (buffer);
      system ("xv /tmp/scarier.jpg >/dev/null 2>&1 &");
      system ("( sleep 10; rm /tmp/scarier.jpg ) >/dev/null 2>&1 &");
    }
#else
  const scr_char *unused1;
  scr_int unused2, unused3;
  unused1 = filepath;
  unused2 = offset;
  unused3 = length;
#endif
}


/*
 * os_read_line()
 * os_read_line_debug()
 */
scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  full_flush ();
  if (feof (stdin))
    {
      /*
       * Already at end of input.  If the game is still running, ask it to
       * quit (scr_quit_game longjmps out of the main loop and never returns).
       * If it does return, the game has already ended -- e.g. we're inside the
       * end-of-game debugger dialog -- so terminate the harness rather than
       * spin re-reading EOF (which previously looped forever printing the
       * debugger prompt and "run_quit: game is not running").
       */
      scr_quit_game (game);
      exit (EXIT_SUCCESS);
    }

  putchar ('>');
  fflush (stdout);
  if (!fgets (buffer, length, stdin))
    {
      /* EOF (or error) on this read with no data; quit cleanly as above. */
      scr_quit_game (game);
      exit (EXIT_SUCCESS);
    }

  /*
   * Scripting aid: treat a line whose first non-blank character is '#' as a
   * comment and skip it, reading the next line instead.  This lets walkthrough
   * solution files carry inline documentation without the SCARIER parser
   * pulling stray direction/verb tokens out of the prose and firing spurious,
   * timing-desyncing moves.  A '#' is never the start of a valid ADRIFT
   * command, so nothing legitimate is lost.
   *
   * This is UNCONDITIONAL (not gated behind SCARIER_DUMP_TOOLS): the commented
   * solution files in test/adrift4/ are the documented validation corpus,
   * and gating comment-skipping behind the dump build meant a plain build
   * silently mis-executed them -- the comment tokens desynced the route into a
   * spurious "death", which once led to a wrong "the walkthrough no longer wins,
   * re-derive it" diagnosis.  os_ansi is the headless dev/test player only
   * (Spatterlight ships os_glk), so there is no byte-faithfulness contract here
   * to protect.
   */
  while (buffer[strspn (buffer, " \t")] == '#')
    {
      if (!fgets (buffer, length, stdin))
        {
          scr_quit_game (game);
          exit (EXIT_SUCCESS);
        }
    }

  /*
   * Derivation aid: with SCR_ECHO_INPUT set, echo the command back after the
   * '>' prompt.  Piped input is not a terminal, so nothing else puts the
   * command into the transcript, and pairing a response with the command that
   * produced it otherwise means counting prompts by hand.
   */
  if (getenv ("SCR_ECHO_INPUT"))
    {
      fputs (buffer, stdout);
      if (buffer[0] != '\0' && buffer[strlen (buffer) - 1] != '\n')
        putchar ('\n');
      fflush (stdout);
    }

  return TRUE;
}

scr_bool
os_read_line_debug (scr_char *buffer, scr_int length)
{
  full_flush ();
  if (feof (stdin))
    scr_quit_game (game);

  printf ("[SCARIER debug]");
  return os_read_line (buffer, length);
}


/*
 * os_confirm()
 */
scr_bool
os_confirm (scr_int type)
{
  scr_char buffer[256];

  if (type == SCR_CONF_SAVE)
    return TRUE;

  full_flush ();
  if (feof (stdin))
    return type == SCR_CONF_QUIT;

  do
    {
      printf ("Do you really want to ");
      switch (type)
        {
        case SCR_CONF_QUIT:
          printf ("quit");
          break;
        case SCR_CONF_RESTART:
          printf ("restart");
          break;
        case SCR_CONF_RESTORE:
          printf ("restore");
          break;
        case SCR_CONF_VIEW_HINTS:
          printf ("view hints");
          break;
        default:
          printf ("do that");
          break;
        }
      printf ("? [Y/N] ");
      fflush (stdout);
      /* EOF mid-prompt leaves buffer stale; without this check the loop spun
       * forever reprinting the question.  Answer as the feof case above. */
      if (!fgets (buffer, sizeof (buffer), stdin))
        return type == SCR_CONF_QUIT;
    }
  while (toupper (buffer[0]) != 'Y' && toupper (buffer[0]) != 'N');

  return toupper (buffer[0]) == 'Y';
}


/*
 * os_open_file()
 * os_read_file()
 * os_write_file()
 * os_close_file()
 */
void *
os_open_file (scr_bool is_save)
{
  scr_char path[256];
  FILE *stream;

  full_flush ();
  if (feof (stdin))
    return NULL;

  printf ("Enter saved game to %s: ", is_save ? "save" : "load");
  fflush (stdout);
  if (!fgets (path, sizeof (path), stdin))
    return NULL;
  if (path[strlen (path) - 1] == '\n')
    path[strlen (path) - 1] = '\0';

  if (is_save)
    {
      stream = fopen (path, "rb");
      if (stream)
        {
          fclose (stream);
          printf ("File already exists.\n");
          return NULL;
        }
      stream = fopen (path, "wb");
    }
  else
    stream = fopen (path, "rb");

  if (!stream)
    {
      printf ("Error opening file.\n");
      return NULL;
    }
  return stream;
}

scr_int
os_read_file (void *opaque, scr_byte *buffer, scr_int length)
{
  FILE *stream = (FILE *) opaque;
  scr_int bytes;

  bytes = fread (buffer, 1, length, stream);
  if (ferror (stream))
    fprintf (stderr, "Read error: %s\n", strerror (errno));

  return bytes;
}

void
os_write_file (void *opaque, const scr_byte *buffer, scr_int length)
{
  FILE *stream = (FILE *) opaque;

  fwrite (buffer, 1, length, stream);
  if (ferror (stream))
    fprintf (stderr, "Write error: %s\n", strerror (errno));
}

void
os_close_file (void *opaque)
{
  FILE *stream = (FILE *) opaque;

  fclose (stream);
}


/*
 * os_display_hints()
 */
void
os_display_hints (scr_game game_)
{
  scr_game_hint hint;
  assert (game_ == game);

  full_flush ();
  for (hint = scr_get_first_game_hint (game);
       hint; hint = scr_get_next_game_hint (game, hint))
    {
      const scr_char *hint_text;

      printf ("%s\n", scr_get_game_hint_question (game, hint));

      hint_text = scr_get_game_subtle_hint (game, hint);
      if (hint_text)
        printf ("- %s\n", hint_text);

      hint_text = scr_get_game_unsubtle_hint (game, hint);
      if (hint_text)
        printf ("- %s\n", hint_text);
    }
}


/*
 * main()
 */
int
main (int argc, const char *argv[])
{
  FILE *stream;
  const char *trace_flags, *locale;
  assert (argc > 0 && argv);

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s taf_file\n", argv[0]);
      return EXIT_FAILURE;
    }

  stream = fopen (argv[1], "rb");
  if (!stream)
    {
      fprintf (stderr, "%s: %s: %s\n", argv[0], argv[1], strerror (errno));
      return EXIT_FAILURE;
    }

  trace_flags = getenv ("SCR_TRACE_FLAGS");
  if (trace_flags)
    scr_set_trace_flags (strtoul (trace_flags, NULL, 0));

  locale = getenv ("SCR_LOCALE");
  if (locale)
    scr_set_locale (locale);

  /*
   * Select portable, predictable random number generation *before* loading the
   * game.  Game creation (run_create -> gs_create) draws random initial event
   * times (scr_randomint, scgamest.cpp), so reseeding only after the load would
   * leave those initial times -- and hence the whole event schedule -- governed
   * by the unseeded, time-based RNG, making event-heavy games (Shadowpeak, …)
   * nondeterministic run to run despite "stable random".
   */
  if (getenv ("SCR_STABLE_RANDOM_ENABLED"))
    {
      scr_set_portable_random (TRUE);
      scr_reseed_random_sequence (1);
    }

  printf ("Loading game...\n");
  game = scr_game_from_stream (stream);
  if (!game)
    {
      fprintf (stderr,
               "%s: %s: Not a loadable Adrift game\n", argv[0], argv[1]);
      fclose (stream);
      return EXIT_FAILURE;
    }
  fclose (stream);

  if (getenv ("SCR_DEBUGGER_ENABLED"))
    scr_set_game_debugger_enabled (game, TRUE);

  game_file = argv[1];

  scr_interpret_game (game);
  scr_free_game (game);
  return EXIT_SUCCESS;
}

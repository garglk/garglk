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
 *   code possible for SCARE.  Actually, it could be simplified still further
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

#include "scare.h"

enum { FALSE = 0, TRUE = !FALSE };

static sc_char line_buffer[79];
static sc_int line_length = 0;

static const sc_char *game_file;
static sc_game game;


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
      const sc_char *line_break;

      line_buffer[line_length] = '\0';
      line_break = strrchr (line_buffer, ' ');
      if (line_break)
        {
          fwrite (line_buffer, 1, line_break - line_buffer, stdout);
          memmove (line_buffer, line_break + 1, strlen (line_break) + 1);
          line_length = strlen (line_buffer);
        }
      else
        full_flush ();
    }
  fflush (stdout);
}

static void
append_character (sc_char c)
{
  if (c == '\n')
    {
      full_flush ();
      putchar ('\n');
    }
  else
    {
      line_buffer[line_length++] = c;
      if (line_length >= (sc_int) sizeof (line_buffer) - 1)
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
os_print_tag (sc_int tag, const sc_char *argument)
{
  sc_int index_;
  const sc_char *unused;
  unused = argument;

  switch (tag)
    {
    case SC_TAG_CLS:
      for (index_ = 0; index_ < 25; index_++)
        append_character ('\n');
      break;

    case SC_TAG_CENTER:
    case SC_TAG_RIGHT:
    case SC_TAG_ENDCENTER:
    case SC_TAG_ENDRIGHT:
      if (line_length > 0)
        append_character ('\n');
      break;

    case SC_TAG_WAITKEY:
      {
        sc_char dummy[256];
        full_flush ();
        if (!feof (stdin))
          fgets (dummy, sizeof (dummy), stdin);
        break;
      }
    }
}

void
os_print_string (const sc_char *string)
{
  sc_int index_;

  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      if (string[index_] == '\t')
        os_print_string ("        ");
      else
        append_character (string[index_]);
    }
}

void
os_print_string_debug (const sc_char *string)
{
  os_print_string (string);
}


/*
 * os_play_sound()
 * os_stop_sound()
 * os_show_graphic()
 */
void
os_play_sound (const sc_char *filepath,
               sc_int offset, sc_int length, sc_bool is_looping)
{
  const sc_char *unused1;
  sc_int unused2, unused3;
  sc_bool unused4;
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
os_show_graphic (const sc_char *filepath, sc_int offset, sc_int length)
{
#ifdef LINUX_GRAPHICS
  const sc_char *unused1;
  unused1 = filepath;

  if (length > 0 && strlen (game_file) < 768)
    {
      sc_char buffer[1024];

      sprintf (buffer,
               "dd if=%s ibs=1c skip=%ld count=%ld obs=100k"
               " of=/tmp/scare.jpg 2>/dev/null", game_file, offset, length);
      system (buffer);
      system ("xv /tmp/scare.jpg >/dev/null 2>&1 &");
      system ("( sleep 10; rm /tmp/scare.jpg ) >/dev/null 2>&1 &");
    }
#else
  const sc_char *unused1;
  sc_int unused2, unused3;
  unused1 = filepath;
  unused2 = offset;
  unused3 = length;
#endif
}


/*
 * os_read_line()
 * os_read_line_debug()
 */
sc_bool
os_read_line (sc_char *buffer, sc_int length)
{
  full_flush ();
  if (feof (stdin))
    sc_quit_game (game);

  putchar ('>');
  fflush (stdout);
  fgets (buffer, length, stdin);
  return TRUE;
}

sc_bool
os_read_line_debug (sc_char *buffer, sc_int length)
{
  full_flush ();
  if (feof (stdin))
    sc_quit_game (game);

  printf ("[SCARE debug]");
  return os_read_line (buffer, length);
}


/*
 * os_confirm()
 */
sc_bool
os_confirm (sc_int type)
{
  sc_char buffer[256];

  if (type == SC_CONF_SAVE)
    return TRUE;

  full_flush ();
  if (feof (stdin))
    return type == SC_CONF_QUIT;

  do
    {
      printf ("Do you really want to ");
      switch (type)
        {
        case SC_CONF_QUIT:
          printf ("quit");
          break;
        case SC_CONF_RESTART:
          printf ("restart");
          break;
        case SC_CONF_RESTORE:
          printf ("restore");
          break;
        case SC_CONF_VIEW_HINTS:
          printf ("view hints");
          break;
        default:
          printf ("do that");
          break;
        }
      printf ("? [Y/N] ");
      fflush (stdout);
      fgets (buffer, sizeof (buffer), stdin);
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
os_open_file (sc_bool is_save)
{
  sc_char path[256];
  FILE *stream;

  full_flush ();
  if (feof (stdin))
    return NULL;

  printf ("Enter saved game to %s: ", is_save ? "save" : "load");
  fflush (stdout);
  fgets (path, sizeof (path), stdin);
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

sc_int
os_read_file (void *opaque, sc_byte *buffer, sc_int length)
{
  FILE *stream = (FILE *) opaque;
  sc_int bytes;

  bytes = fread (buffer, 1, length, stream);
  if (ferror (stream))
    fprintf (stderr, "Read error: %s\n", strerror (errno));

  return bytes;
}

void
os_write_file (void *opaque, const sc_byte *buffer, sc_int length)
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
os_display_hints (sc_game game_)
{
  sc_game_hint hint;
  assert (game_ == game);

  full_flush ();
  for (hint = sc_get_first_game_hint (game);
       hint; hint = sc_get_next_game_hint (game, hint))
    {
      const sc_char *hint_text;

      printf ("%s\n", sc_get_game_hint_question (game, hint));

      hint_text = sc_get_game_subtle_hint (game, hint);
      if (hint_text)
        printf ("- %s\n", hint_text);

      hint_text = sc_get_game_unsubtle_hint (game, hint);
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

  trace_flags = getenv ("SC_TRACE_FLAGS");
  if (trace_flags)
    sc_set_trace_flags (strtoul (trace_flags, NULL, 0));

  locale = getenv ("SC_LOCALE");
  if (locale)
    sc_set_locale (locale);

  printf ("Loading game...\n");
  game = sc_game_from_stream (stream);
  if (!game)
    {
      fprintf (stderr,
               "%s: %s: Not a loadable Adrift game\n", argv[0], argv[1]);
      fclose (stream);
      return EXIT_FAILURE;
    }
  fclose (stream);

  if (getenv ("SC_DEBUGGER_ENABLED"))
    sc_set_game_debugger_enabled (game, TRUE);
  if (getenv ("SC_STABLE_RANDOM_ENABLED"))
    {
      sc_set_portable_random (TRUE);
      sc_reseed_random_sequence (1);
    }

  game_file = argv[1];

  sc_interpret_game (game);
  sc_free_game (game);
  return EXIT_SUCCESS;
}

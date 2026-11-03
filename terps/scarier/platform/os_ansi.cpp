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
 * Module notes:
 *
 * o This module represents just about the simplest platform input/output
 *   code possible for Scarier.  Actually, it could be simplified still further
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

#include "core/scarier.h"

#ifdef SCARIER_DUMP_TOOLS
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

/* Defined in runner/scrun_match_run.cpp and declared in runner/scrunner.h, but
 * that header drags in scprotos.h, whose TRUE/FALSE macros clash with the
 * enum below; this front end stays off the engine headers on purpose. */
extern scr_bool run_probe_strict;
#endif

enum { FALSE = 0, TRUE = !FALSE };


/*
 * wrap_width()
 *
 * The harness wraps at 78 columns, as a terminal would.  SCR_WRAP_WIDTH turns
 * that off (or moves it): set it wide and every newline in the output is one
 * the engine meant, which is what the Runner-transcript line-structure sweep
 * needs -- see harness/sweep_wine_breaks.py.  The default is the historical
 * 79, so every golden in the suite is unaffected.
 */
static scr_char line_buffer[65536];
static scr_int line_length = 0;

static scr_int
wrap_width (void)
{
  static scr_int cached = 0;

  if (cached == 0)
    {
      const scr_char *env = getenv ("SCR_WRAP_WIDTH");

      cached = env ? atol (env) : 79;
      if (cached < 2 || cached > (scr_int) sizeof (line_buffer))
        cached = (scr_int) sizeof (line_buffer);
    }
  return cached;
}

static const scr_char *game_file;
static scr_game game;


/*
 * full_flush()
 * partial_flush()
 * append_character()
 *
 * The line wrapper: buffer output a character at a time, and flush it whole
 * at a newline, or at the last space before the wrap width.
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
      if (line_length >= wrap_width () - 1)
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
 *
 * Output entry points.  Most tags are ignored; <cls> clears by scrolling,
 * alignment tags break the line, and the two pauses are derivation aids.
 */
void
os_print_tag (scr_int tag, const scr_char *argument)
{
  scr_int index_;

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

    case SCR_TAG_WAIT:
      /*
       * A timed pause.  Nothing to wait for headless, but SCR_MARK_WAIT=1
       * notes it on stderr in transcript order, the way SCR_MARK_WAITKEY
       * does below: the real Runner drops every keystroke typed while one
       * runs, so a Wine replay has to sleep through it (see
       * test/adrift4/harness/make_wine_cmdfile.py).
       */
      if (getenv ("SCR_MARK_WAIT"))
        {
          full_flush ();
          fflush (stdout);
          fprintf (stderr, "[WAIT %s]\n", argument ? argument : "");
        }
      break;

    case SCR_TAG_WAITKEY:
      {
        scr_char dummy[256];
        scr_bool mark_waitkey;

        full_flush ();

        /*
         * A "press a key" pause.  Normally we consume one line of input to
         * stand in for the keypress.  For scripted/headless walkthrough
         * derivation, SCR_SKIP_WAITKEY=1 makes these pauses transparent so a
         * solution file maps one line to one game command regardless of how
         * many <waitkey> tags the game's text embeds.
         *
         * Derivation aid: SCR_MARK_WAITKEY=1 notes each pause on stderr (after
         * the flush above, so with 2>&1 the marker lands in transcript order).
         * Combined with SCR_SKIP_WAITKEY=1 -- which keeps the command list in
         * sync -- that turns "how many blank lines does this solution need, and
         * where?" into a read rather than a bisection.  Without the skip, the
         * marker also names the line the pause just ate, which is what tells a
         * deliberate filler apart from a real command going missing (see
         * harness/waitkey_audit.py).
         */
        mark_waitkey = getenv ("SCR_MARK_WAITKEY") != nullptr;
        if (mark_waitkey)
          {
            fflush (stdout);
            fprintf (stderr, "[WAITKEY]\n");
          }
        if (getenv ("SCR_SKIP_WAITKEY"))
          break;

        /*
         * Honour the '#' comment convention os_read_line applies below.  A
         * comment is documentation, not input, so a pause must not be able to
         * eat one: doing so hid the swallow (the route still ran in full,
         * because the free filler happened to be the header) and made the
         * behaviour depend on whether a solution file was commented -- 25 of
         * the wired rows were relying on exactly that accident.
         */
        dummy[0] = '\0';
        while (!feof (stdin) && fgets (dummy, sizeof (dummy), stdin)
               && dummy[strspn (dummy, " \t")] == '#')
          dummy[0] = '\0';
        if (mark_waitkey)
          {
            scr_int length = strlen (dummy);

            while (length > 0 && (dummy[length - 1] == '\n'
                                  || dummy[length - 1] == '\r'))
              dummy[--length] = '\0';
            fprintf (stderr, "[WAITKEY ate \"%s\"]\n", dummy);
          }
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
 *
 * Media entry points.  SCR_TRACE_SOUND=1 prints each sound request on a line
 * of its own where it falls in the output, in headless/sxstubs.cpp's
 * <<Sound: ...>> form, so a headless golden can pin which resource a task
 * plays and the offset and length the TAF parser gave it.  The id is the
 * path's last component: the rest is the author's directory, or for an
 * embedded resource this machine's path to the game.  Graphics are ignored.
 */
void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  if (getenv ("SCR_TRACE_SOUND"))
    {
      const scr_char *id = filepath, *p;
      scr_char buffer[96];

      for (p = filepath; *p != '\0'; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
          id = p + 1;
      os_print_string ("<<Sound: id=\"");
      os_print_string (id);
      snprintf (buffer, sizeof buffer,
                "\", offset=%ld, length=%ld, looping=%s>>\n",
                (long) offset, (long) length, is_looping ? "true" : "false");
      os_print_string (buffer);
    }
}

void
os_stop_sound (void)
{
  if (getenv ("SCR_TRACE_SOUND"))
    os_print_string ("<<Sound: stop>>\n");
}

void
os_show_graphic (const scr_char * /* filepath */, scr_int /* offset */,
                 scr_int /* length */)
{
}


/* Solution-file line counter for the SCR_TRACE_ADMIN derivation aid. */
static long os_ansi_input_line = 0;

#ifdef SCARIER_DUMP_TOOLS


/*
 * os_ansi_slurp()
 * os_ansi_report()
 * os_ansi_probe_run()
 * os_ansi_probe()
 *
 * Leniency audit, SCR_PROBE_LINES=<file>: before each walkthrough line runs,
 * try every line in the file (one per line, blank and '#' lines skipped) in
 * two forked children, one as Scarier plays it and one with no leniency at
 * all (run_probe_strict, the Runner way), and report the lines whose answers
 * differ on stderr:
 *
 *   PROBE\t<solution line>\t<probe line>
 *   L\t<lenient answer line>      (one per line of output)
 *   S\t<strict answer line>
 *   D\t<DEV trace line>           (the lenient child's, SCR_TRACE_DEVIATIONS)
 *
 * The children never touch the walkthrough's stdin, and each stops at its
 * next read, so the parent plays on as if nothing had happened.
 * SCR_PROBE_EVERY=N probes only every Nth solution line.  probe_lenient.sh
 * drives this with the census's cross lines.
 */
static std::vector<std::string> os_ansi_probe_lines;
static scr_bool os_ansi_probe_child = FALSE;

static std::string
os_ansi_slurp (const char *path)
{
  std::string text;
  FILE *stream = fopen (path, "rb");
  if (stream)
    {
      char chunk[4096];
      size_t count;
      while ((count = fread (chunk, 1, sizeof chunk, stream)) > 0)
        text.append (chunk, count);
      fclose (stream);
    }
  return text;
}

static void
os_ansi_report (const char *tag, const std::string &text)
{
  size_t start = 0;
  while (start < text.size ())
    {
      size_t end = text.find ('\n', start);
      if (end == std::string::npos)
        end = text.size ();
      std::string line = text.substr (start, end - start);
      if (tag[0] != 'D' || line.compare (0, 4, "DEV ") == 0)
        fprintf (stderr, "%s\t%s\n", tag, line.c_str ());
      start = end + 1;
    }
}

static scr_bool
os_ansi_probe_run (const std::string &probe, scr_bool strict,
                   const char *out_path, const char *err_path,
                   scr_char *buffer, scr_int length)
{
  /*
   * Run PROBE in a child, strictly or not, its stdout and stderr going to
   * OUT_PATH and ERR_PATH.  Returns TRUE in the child, with BUFFER holding
   * the line to play, and FALSE in the parent once the child is done.
   */
  fflush (stdout);
  fflush (stderr);
  pid_t child = fork ();
  if (child == 0)
    {
      int null_in = open ("/dev/null", O_RDONLY);
      int out = open (out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
      int err = open (err_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
      /* fd 0 away from the walkthrough, so no exit-time seek can move it. */
      dup2 (null_in, 0);
      dup2 (out, 1);
      dup2 (err, 2);
      close (null_in);
      close (out);
      close (err);
      alarm (10);
      os_ansi_probe_child = TRUE;
      run_probe_strict = strict;
      snprintf (buffer, length, "%s\n", probe.c_str ());
      return TRUE;
    }
  if (child > 0)
    {
      int status;
      while (waitpid (child, &status, 0) < 0 && errno == EINTR)
        ;
    }
  return FALSE;
}

static scr_bool
os_ansi_probe (scr_char *buffer, scr_int length)
{
  static scr_bool loaded = FALSE;
  static long every = 1;
  static std::string out_paths[2], err_paths[2];

  if (!loaded)
    {
      const char *path = getenv ("SCR_PROBE_LINES");
      loaded = TRUE;
      if (!path)
        return FALSE;
      std::string text = os_ansi_slurp (path);
      size_t start = 0;
      while (start < text.size ())
        {
          size_t end = text.find ('\n', start);
          if (end == std::string::npos)
            end = text.size ();
          std::string line = text.substr (start, end - start);
          if (!line.empty () && line[line.size () - 1] == '\r')
            line.erase (line.size () - 1);
          if (line.find_first_not_of (" \t") != std::string::npos
              && line[line.find_first_not_of (" \t")] != '#')
            os_ansi_probe_lines.push_back (line);
          start = end + 1;
        }
      if (getenv ("SCR_PROBE_EVERY"))
        every = strtol (getenv ("SCR_PROBE_EVERY"), nullptr, 10);
      if (every < 1)
        every = 1;
      for (int mode = 0; mode < 2; mode++)
        {
          std::string stem = getenv ("TMPDIR") ? getenv ("TMPDIR") : "/tmp";
          stem += "/scr_probe." + std::to_string ((long) getpid ()) + "."
                  + std::to_string (mode);
          out_paths[mode] = stem + ".out";
          err_paths[mode] = stem + ".err";
        }
    }
  if (os_ansi_probe_lines.empty () || os_ansi_input_line % every != 0)
    return FALSE;

  for (const std::string &probe : os_ansi_probe_lines)
    {
      std::string answers[2];
      for (int mode = 0; mode < 2; mode++)
        {
          if (os_ansi_probe_run (probe, mode == 1, out_paths[mode].c_str (),
                                 err_paths[mode].c_str (), buffer, length))
            return TRUE;
          answers[mode] = os_ansi_slurp (out_paths[mode].c_str ());
        }
      if (answers[0] != answers[1])
        {
          fprintf (stderr, "PROBE\t%ld\t%s\n", os_ansi_input_line,
                   probe.c_str ());
          os_ansi_report ("L", answers[0]);
          os_ansi_report ("S", answers[1]);
          os_ansi_report ("D", os_ansi_slurp (err_paths[0].c_str ()));
          fflush (stderr);
        }
    }
  for (int mode = 0; mode < 2; mode++)
    {
      unlink (out_paths[mode].c_str ());
      unlink (err_paths[mode].c_str ());
    }
  return FALSE;
}
#endif


/*
 * os_read_line()
 * os_read_line_debug()
 *
 * Read a command line from stdin, quitting the game cleanly at end of input.
 * The environment switches below are derivation and compare aids for the
 * walkthrough harnesses.
 */
scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  scr_bool echo_input, scripted;

  full_flush ();
#ifdef SCARIER_DUMP_TOOLS
  /* A probe child has answered its one line; that is all it is for. */
  if (os_ansi_probe_child)
    {
      fflush (stdout);
      fflush (stderr);
      _exit (EXIT_SUCCESS);
    }
#endif
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

  /*
   * Derivation aid: with SCR_ECHO_INPUT set, echo the command back after the
   * '>' prompt.  Piped input is not a terminal, so nothing else puts the
   * command into the transcript, and pairing a response with the command that
   * produced it otherwise means counting prompts by hand.
   *
   * The echoing prompt is laid out as "\n> command\n", which is the shape
   * a5run_dump gives the ADRIFT 5 goldens, so both corpora's transcripts read
   * the same way.  Without the echo the bare '>' stays glued to the front of
   * the game's reply (">You move north.") the way it always has.
   */
  echo_input = getenv ("SCR_ECHO_INPUT") != nullptr;

  if (echo_input)
    putchar ('\n');

  /*
   * Compare aid: with SCR_MARK_PROMPT set, a \x02 in front of the prompt
   * tells it apart from game text that begins a line with '>'.  3monkeys'
   * ending prints "> GIVE FINGER TO DR. WICKETT", which the Wine compare
   * otherwise split off as a turn of its own (2026-09-19).
   *
   * A `go <place>` walk types its own steps; show each as typed.  A step is
   * part of the typed turn, the Runner echoing it as "> East" inside the
   * walk, so it gets no \x02: outside's goto turns split in the compare.
   */
  scripted = scr_take_scripted_line (buffer, length);
  if (getenv ("SCR_MARK_PROMPT") && !scripted)
    putchar ('\x02');
  putchar ('>');
  if (echo_input)
    putchar (' ');

  if (scripted)
    {
      fputs (buffer, stdout);
      putchar ('\n');
      fflush (stdout);
      return TRUE;
    }

  fflush (stdout);
  os_ansi_input_line++;
  if (!fgets (buffer, length, stdin))
    {
      /* EOF (or error) on this read with no data; quit cleanly as above. */
      scr_quit_game (game);
      exit (EXIT_SUCCESS);
    }

  /*
   * Scripting aid: treat a line whose first non-blank character is '#' as a
   * comment and skip it, reading the next line instead.  This lets walkthrough
   * solution files carry inline documentation without the Scarier parser
   * pulling stray direction/verb tokens out of the prose and firing spurious,
   * timing-desyncing moves.  A '#' is never the start of a valid ADRIFT
   * command, so nothing legitimate is lost.
   *
   * This is UNCONDITIONAL (not gated behind SCARIER_DUMP_TOOLS): the commented
   * solution files in test/adrift4/ are the documented validation corpus,
   * and gating comment-skipping behind the dump build meant a plain build
   * silently mis-executed them -- the comment tokens desynced the route into
   * a spurious "death", which once led to a wrong "the walkthrough no longer
   * wins, re-derive it" diagnosis.  os_ansi is the headless dev/test player
   * only (Spatterlight ships os_glk), so there is no byte-faithfulness
   * contract here to protect.
   */
  while (buffer[strspn (buffer, " \t")] == '#')
    {
      os_ansi_input_line++;
      if (!fgets (buffer, length, stdin))
        {
          scr_quit_game (game);
          exit (EXIT_SUCCESS);
        }
    }

#ifdef SCARIER_DUMP_TOOLS
  /* The leniency probe; a child plays its probe line instead. */
  if (os_ansi_probe (buffer, length))
    return TRUE;

  /*
   * Derivation aid, paired with SCR_TRACE_ADMIN in run_main_loop(): name the
   * line just read (1-based, comments counted) so an "ADMIN" trace line can
   * be tied to the solution-file line that produced it.
   */
  {
    static const bool trace_admin = getenv ("SCR_TRACE_ADMIN") != nullptr;
    if (trace_admin)
      fprintf (stderr, "INPUT line=%ld %s", os_ansi_input_line, buffer);
  }
#endif

  /* The other half of the echo above: the command itself. */
  if (echo_input)
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

  printf ("[Scarier debug]");
  return os_read_line (buffer, length);
}


/*
 * os_confirm()
 *
 * Ask a yes/no question on stdout, answered from stdin.  Saving is always
 * confirmed, and end of input answers yes to quitting and no to the rest.
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
      /*
       * EOF mid-prompt leaves buffer stale; without this check the loop spun
       * forever reprinting the question.  Answer as the feof case above.
       */
      if (!fgets (buffer, sizeof (buffer), stdin))
        return type == SCR_CONF_QUIT;

      /*
       * Compare aid: SCR_MARK_CONFIRM=1 notes every line this question reads
       * on stderr, in transcript order, the way SCR_MARK_WAITKEY notes a
       * pause.  The answer is read without a prompt, so a tool that numbers
       * input lines by prompt drifts one line per question -- mould's
       * `hint`/`y` pairs threw the Wine compare's pause bookkeeping off
       * (test/adrift4/harness/compare_wine_transcript.py, 2026-09-21).
       */
      if (getenv ("SCR_MARK_CONFIRM"))
        {
          scr_int length = strlen (buffer);

          while (length > 0 && (buffer[length - 1] == '\n'
                                || buffer[length - 1] == '\r'))
            length--;
          fflush (stdout);
          fprintf (stderr, "[CONFIRM ate \"%.*s\"]\n", (int) length, buffer);
        }
    }
  while (toupper (buffer[0]) != 'Y' && toupper (buffer[0]) != 'N');

  return toupper (buffer[0]) == 'Y';
}


/*
 * os_open_file()
 * os_read_file()
 * os_write_file()
 * os_close_file()
 *
 * Saved game files, by path typed at a prompt; a save never overwrites.
 */
void *
os_open_file (scr_bool is_save)
{
  scr_char path[256];
  FILE *stream;

  full_flush ();
  if (feof (stdin))
    return nullptr;

  printf ("Enter saved game to %s: ", is_save ? "save" : "load");
  fflush (stdout);
  if (!fgets (path, sizeof (path), stdin))
    return nullptr;
  if (path[strlen (path) - 1] == '\n')
    path[strlen (path) - 1] = '\0';

  if (is_save)
    {
      stream = fopen (path, "rb");
      if (stream)
        {
          fclose (stream);
          printf ("File already exists.\n");
          return nullptr;
        }
      stream = fopen (path, "wb");
    }
  else
    stream = fopen (path, "rb");

  if (!stream)
    {
      printf ("Error opening file.\n");
      return nullptr;
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
 *
 * Print every hint question with its subtle and unsubtle answers.
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
 *
 * Load the game named on the command line and play it on stdin/stdout.  The
 * SCR_* environment variables select tracing, the locale, stable random
 * numbers and the debugger.
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
    scr_set_trace_flags (strtoul (trace_flags, nullptr, 0));

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
      scr_reseed_random_sequence (scr_default_random_seed ());
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

  /*
   * This port stands in for the Windows Runner's console the way the Wine
   * transcripts capture it, so it ends a completed game the way the Runner
   * does, with "[Press any key to end]" (see task_print_end_keyprompt()).  A
   * Glk host with its own RESTART/UNDO/QUIT offer leaves this off.
   */
  scr_set_end_keyprompt (TRUE);

  game_file = argv[1];

  scr_interpret_game (game);

  /*
   * The prompt above carries no terminator of its own -- the Runner blocks on
   * a keypress there -- so flush what the wrapper is still holding and end the
   * transcript on a line break like every other line.
   */
  if (line_length > 0)
    {
      full_flush ();
      putchar ('\n');
    }
  fflush (stdout);

  scr_free_game (game);
  return EXIT_SUCCESS;
}

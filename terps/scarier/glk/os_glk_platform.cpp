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
 * os_glk_platform.cpp: the Glk linkage that differs by host.
 *
 * Opening a game file on disk as a Glk stream: Windows Glk has no
 * glkunix_stream_open_pathname(); a fileref made from the path with
 * validation off opens the same file.  Every other host has the glkunix
 * call, Gargoyle on Windows included, since it starts up through
 * glkunix_startup_code().  The startup code below and the ADRIFT 5 resource
 * map in os_glk_a5.cpp both open files through here, so neither carries a
 * _WIN32 branch of its own.
 *
 * The entry points the Glk library calls: glk_main(), and the startup code
 * of each host, glkunix_startup_code() or WinMain() / winglk_startup_code().
 * They parse the command line, open the game, and hand over to
 * gsc_startup_code() and gsc_main() in os_glk.cpp.
 */

#include "os_glk_internal.h"

#if defined(_WIN32) && !defined(GARGLK)
#include "WinGlk.h"
#else
/* glkstart.h exists only on this side of the #if, which is why it is
 * included here rather than in os_glk_internal.h. */
extern "C" {
#include "glkstart.h"
}
#endif

/*
 * gsc_open_game_path()
 *
 * Open the file at path as a read-only binary Glk stream with the given rock.
 * Returns nullptr if the host cannot reach it: a Glk that cannot reopen files
 * by path, or a game that has moved since startup.
 */
strid_t
gsc_open_game_path (const char *path, glui32 rock)
{
#if defined(_WIN32) && !defined(GARGLK)
  frefid_t fileref;
  strid_t stream;

  fileref = winglk_fileref_create_by_name (fileusage_BinaryMode
                                           | fileusage_Data,
                                           (char *) path, 0, 0);
  if (fileref == nullptr)
    return nullptr;
  stream = glk_stream_open_file (fileref, filemode_Read, rock);
  glk_fileref_destroy (fileref);
  return stream;
#else
  return glkunix_stream_open_pathname ((char *) path, FALSE, rock);
#endif
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the real interpreter      */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int gsc_startup_called = FALSE,
           gsc_main_called = FALSE;


/*
 * glk_main()
 *
 * Main entry point for Glk.  Here, all startup is done, and we call our
 * function to run the game, or to report errors if gsc_game_message is set.
 */
void
glk_main (void)
{
  assert (gsc_startup_called && !gsc_main_called);
  gsc_main_called = TRUE;

  /* ADRIFT 5 games run the dedicated a5 turn loop; everything else (ADRIFT
   * <=4) uses the scare engine. */
  if (gsc_is_a5)
    gsc_a5_main ();
  else
    gsc_main ();
}


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the UNIX platform                     */
/*---------------------------------------------------------------------*/
/* Gargoyle starts up through glkunix_startup_code() on Windows too. */
#if !defined(_WIN32) || defined(GARGLK)

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
static const char *find_last_of(const char *str, const char *chars)
{
  const char *found = nullptr;
  while (*chars != 0) {
    const char *p = strrchr(str, *chars++);
    if (p != nullptr && (found == nullptr || p > found)) {
      found = p;
    }
  }

  return found;
}
#endif


/*
 * Glk arguments for UNIX versions of the Glk interpreter.
 */
glkunix_argumentlist_t glkunix_arguments[] = {
  {(char *) "-nc", glkunix_arg_NoValue,
   (char *) "-nc        No local handling for Glk special commands"},
  {(char *) "-na", glkunix_arg_NoValue,
   (char *) "-na        Turn off abbreviation expansions"},
  {(char *) "-nu", glkunix_arg_NoValue,
   (char *) "-nu        Turn off any use of Unicode output"},
#ifdef GSC_HAVE_ZCOLORS
  {(char *) "-c", glkunix_arg_NoValue,
   (char *) "-c         Start in the game's own Adrift colours"},
#endif
  {(char *) "-r", glkunix_arg_ValueFollows,
   (char *) "-r FILE    Restore from FILE on starting the game"},
  {(char *) "", glkunix_arg_ValueCanFollow,
   (char *) "filename   game to run"},
  {nullptr, glkunix_arg_End, nullptr}
};


/*
 * glkunix_startup_code()
 *
 * Startup entry point for UNIX versions of Glk interpreter.  Glk will call
 * glkunix_startup_code() to pass in arguments.  On startup, parse arguments
 * and open a Glk stream to the game, then call the generic gsc_startup_code()
 * to build a game from the stream.  On error, set the message in
 * gsc_game_message; the core gsc_main() will report it when it's called.
 */
int
glkunix_startup_code (glkunix_startup_t * data)
{
  int argc = data->argc;
  scr_char **argv = data->argv;
  int argv_index;
  scr_char *restore_from;
  const scr_char *locale;
  strid_t game_stream, restore_stream;
  scr_uint trace_flags;
  scr_bool enable_debugger, stable_random;
  assert (!gsc_startup_called);
  gsc_startup_called = TRUE;

#ifdef GARGLK
  garglk_set_program_name("Scarier " SCARIER_VERSION);
  garglk_set_program_info("Scarier " SCARIER_VERSION
      " by Simon Baldwin and Mark J. Tilford");
#endif

  /* Handle command line arguments. */
  restore_from = nullptr;
  for (argv_index = 1;
       argv_index < argc && argv[argv_index][0] == '-'; argv_index++)
    {
      if (strcmp (argv[argv_index], "-nc") == 0)
        {
          gsc_commands_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-na") == 0)
        {
          gsc_abbreviations_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-nu") == 0)
        {
          gsc_unicode_enabled = FALSE;
          continue;
        }
#ifdef GSC_HAVE_ZCOLORS
      /* Only offered where the Glk library has the zcolors extension, exactly
         as "glk colour" is -- accepting it elsewhere would promise a palette
         that could never arrive. */
      if (strcmp (argv[argv_index], "-c") == 0)
        {
          gsc_colour_startup = TRUE;
          continue;
        }
#endif
      if (strcmp (argv[argv_index], "-r") == 0)
        {
          restore_from = argv[++argv_index];
          continue;
        }
      return FALSE;
    }

  /* On invalid usage, set a complaint message and return. */
  if (argv_index != argc - 1)
    {
      gsc_game = nullptr;
      if (argv_index < argc - 1)
        gsc_game_message = "More than one game file"
                           " was given on the command line.";
      else
        gsc_game_message = "No game file was given on the command line.";
      return TRUE;
    }

  /* Remember the game file path for hosts that reopen it (graphics/sound via
   * gsc_open_game_path). ADRIFT 5 game data itself is loaded from the Glk
   * stream above via a5model_load_buffer. */
  snprintf (gsc_game_path, sizeof gsc_game_path, "%s", argv[argv_index]);

  /* Open a stream to the TAF file, complain if this fails. */
  game_stream = gsc_open_game_path (argv[argv_index], 0);
  if (!game_stream)
    {
      gsc_game = nullptr;
      gsc_game_message = "Unable to open the requested game file.";
      return TRUE;
    }
  else
    gsc_game_message = nullptr;

  /*
   * If a restore requested, open a stream to the TAF (TAS) file, and
   * again, complain if this fails.
   */
  if (restore_from)
    {
      restore_stream = gsc_open_game_path (restore_from, 0);
      if (!restore_stream)
        {
          glk_stream_close (game_stream, nullptr);
          gsc_game = nullptr;
          gsc_game_message = "Unable to open the requested restore file.";
          return TRUE;
        }
      else
        gsc_game_message = nullptr;
    }
  else
    restore_stream = nullptr;

  /* Set Scarier trace flags and other general setup from the environment. */
  if (getenv ("SCR_TRACE_FLAGS"))
    trace_flags = strtoul (getenv ("SCR_TRACE_FLAGS"), nullptr, 0);
  else
    trace_flags = 0;
  enable_debugger = (getenv ("SCR_DEBUGGER_ENABLED") != nullptr);
#if defined (SPATTERLIGHT)
  stable_random = gli_determinism;
#else
  stable_random = (getenv ("SCR_STABLE_RANDOM_ENABLED") != nullptr);
#endif
  locale = getenv ("SCR_LOCALE");

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
#ifdef _WIN32
    const char *sep = "/\\";
#else
    const char *sep = "/";
#endif
    const char *slash = find_last_of(argv[argv_index], sep);
    if (slash == nullptr) {
      snprintf(gsc_gamefile, sizeof gsc_gamefile, "%s", argv[argv_index]);
    } else {
      snprintf(gsc_gamefile, sizeof gsc_gamefile, "%s", slash + 1);
  }
#endif

#ifdef GARGLK
  glkunix_set_base_file(argv[argv_index]);
#endif

  /* Use the generic startup code to complete startup. */
  return gsc_startup_code (game_stream, restore_stream, trace_flags,
                           enable_debugger, stable_random, locale);
}
#endif /* !_WIN32 || GARGLK */


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the Windows platform                  */
/*---------------------------------------------------------------------*/
#if defined(_WIN32) && !defined(GARGLK)

#include <windows.h>

#include "resource.h"

/* Windows constants and external definitions. */
static const unsigned int GSCWIN_GLK_INIT_VERSION = 0x601;
extern int InitGlk (unsigned int iVersion);

/*
 * WinMain()
 *
 * Entry point for all Glk applications.
 */
int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
         LPSTR lpCmdLine, int nCmdShow)
{
  /* Attempt to initialize both the Glk library and Scarier. */
  if (!(InitGlk (GSCWIN_GLK_INIT_VERSION) && winglk_startup_code (lpCmdLine)))
    return 0;

  /* Run the application; no return from this routine. */
  glk_main ();
  glk_exit ();
  return 0;
}


/*
 * winglk_startup_code()
 *
 * Startup entry point for Windows versions of Glk interpreter.
 */
int
winglk_startup_code (const char *cmdline)
{
  const char *filename, *locale;
  strid_t game_stream;
  scr_uint trace_flags;
  scr_bool enable_debugger, stable_random;
  assert (!gsc_startup_called);
  gsc_startup_called = TRUE;

  /* Set up application and window. */
  winglk_app_set_name ("Scarier");
  winglk_set_menu_name ("&Scarier");
  winglk_window_set_title ("Scarier Adrift Interpreter");
  winglk_set_about_text ("Windows Scarier " SCARIER_VERSION);
  winglk_set_gui (IDI_SCARIER);
  glk_stylehint_set (wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);

  /* Open a stream to the game. */
  filename = winglk_get_initial_filename (cmdline,
                             "Select an Adrift game to run",
                             "Adrift Games (*.taf;*.blorb;*.blb)"
                             "|*.taf;*.blorb;*.blb|All Files (*.*)|*.*||");
  if (!filename)
    return 0;
  snprintf (gsc_game_path, sizeof gsc_game_path, "%s", filename);

  game_stream = gsc_open_game_path (filename, 0);
  if (!game_stream)
    return 0;

  /* Set trace, debugger, and portable random flags. */
  if (getenv ("SCR_TRACE_FLAGS"))
    trace_flags = strtoul (getenv ("SCR_TRACE_FLAGS"), nullptr, 0);
  else
    trace_flags = 0;
  enable_debugger = (getenv ("SCR_DEBUGGER_ENABLED") != nullptr);
  stable_random = (getenv ("SCR_STABLE_RANDOM_ENABLED") != nullptr);
  locale = getenv ("SCR_LOCALE");

  /* Use the generic startup code to complete startup. */
  return gsc_startup_code (game_stream, nullptr, trace_flags,
                           enable_debugger, stable_random, locale);
}
#endif /* _WIN32 && !GARGLK */

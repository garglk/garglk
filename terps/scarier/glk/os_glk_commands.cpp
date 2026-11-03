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
 * os_glk_commands.cpp: the "glk" command escapes, their table and their
 * help.  Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/* Glk Scarier interface version number.  Each byte is printed in DECIMAL by
   gsc_command_print_version_number (so 0x00010400 -> "1.4.0"); keep every
   component below 10, or encode it as its hex value (11 -> 0x0b).  SCARE
   1.3.10 wrote 0x...10 here and printed as "1.3.16" for years. */
static const glui32 GSC_PORT_VERSION = 0x00010400;

/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/* Print the one-line synopsis of what a Glk command accepts, held in the
   command table so that a command handed an argument it doesn't understand
   and that command's "glk help" entry always agree. */

/*
 * gsc_open_log_stream()
 *
 * Prompt for a log file and open a stream on it, in the way all three of the
 * logging commands below want it done.  `must_exist` is for the read log,
 * which reads back a file rather than writing one, and `is_unicode` for the
 * transcript, which is echoed every character the main window gets -- a
 * Latin-1 stream would write anything past U+00FF as '?', where a unicode
 * text stream writes UTF-8.  Returns NULL, having already complained under
 * `label`, where the player cancelled the prompt or the file would not open.
 */
static strid_t
gsc_open_log_stream (const char *label, glui32 usage, glui32 mode,
                     scr_bool must_exist, scr_bool is_unicode)
{
  frefid_t fileref;
  strid_t stream;

  fileref = glk_fileref_create_by_prompt (usage, (glui32) mode, 0);
  if (fileref && must_exist && !glk_fileref_does_file_exist (fileref))
    {
      glk_fileref_destroy (fileref);
      fileref = nullptr;
    }
  if (!fileref)
    {
      gsc_standout_string (label);
      gsc_standout_string (" failed.\n");
      return nullptr;
    }

  stream = is_unicode ? glk_stream_open_file_uni (fileref, (glui32) mode, 0)
                      : glk_stream_open_file (fileref, (glui32) mode, 0);
  glk_fileref_destroy (fileref);
  if (!stream)
    {
      gsc_standout_string (label);
      gsc_standout_string (" failed.\n");
      return nullptr;
    }

  return stream;
}


/*
 * gsc_command_logging()
 *
 * The shape all three logging commands take: "on" opens the log stream via
 * the file prompt and "off" closes it, each saying so unless the log was
 * already that way; a bare command acts rather than reports -- it is a
 * synonym for "on", since that is what someone typing it almost always wants;
 * "status" reports instead, and is what the summary polls; anything else is a
 * usage error.
 *
 * `stream` is the log's stream global, `label` opens every message, and
 * `is_transcript` additionally attaches or detaches the stream as the main
 * window's echo stream, which is what makes the transcript a transcript.
 */
static void
gsc_command_logging (const char *argument, const char *name,
                     const char *label, strid_t *stream,
                     glui32 usage, glui32 mode, scr_bool must_exist,
                     scr_bool is_transcript)
{
  assert (argument);

  if (scr_strcasecmp (argument, "on") == 0 || strlen (argument) == 0)
    {
      if (*stream)
        {
          gsc_normal_string (label);
          gsc_normal_string (" is already on.\n");
          return;
        }

      *stream = gsc_open_log_stream (label, usage, mode, must_exist,
                                     is_transcript);
      if (!*stream)
        return;

      if (is_transcript)
        glk_window_set_echo_stream (gsc_main_window, *stream);

      gsc_normal_string (label);
      gsc_normal_string (" is now on.\n");
    }

  else if (scr_strcasecmp (argument, "off") == 0)
    {
      if (!*stream)
        {
          gsc_normal_string (label);
          gsc_normal_string (" is already off.\n");
          return;
        }

      glk_stream_close (*stream, nullptr);
      *stream = nullptr;

      if (is_transcript)
        glk_window_set_echo_stream (gsc_main_window, nullptr);

      gsc_normal_string (label);
      gsc_normal_string (" is now off.\n");
    }

  else if (scr_strcasecmp (argument, "status") == 0)
    {
      gsc_normal_string (label);
      gsc_normal_string (" is ");
      gsc_normal_string (*stream ? "on" : "off");
      gsc_normal_string (".\n");
    }

  else
    {
      gsc_command_usage (name);
    }
}


/*
 * gsc_command_script()
 * gsc_command_inputlog()
 * gsc_command_readlog()
 *
 * Turn game output scripting (logging), game input logging, and input log
 * read-back on and off.
 */
static void
gsc_command_script (const char *argument)
{
  gsc_command_logging (argument, "script", "Glk transcript",
                       &gsc_transcript_stream,
                       fileusage_Transcript | fileusage_TextMode,
                       filemode_WriteAppend, FALSE, TRUE);
}

static void
gsc_command_inputlog (const char *argument)
{
  gsc_command_logging (argument, "inputlog", "Glk input logging",
                       &gsc_inputlog_stream,
                       fileusage_InputRecord | fileusage_BinaryMode,
                       filemode_WriteAppend, FALSE, FALSE);
}

static void
gsc_command_readlog (const char *argument)
{
  gsc_command_logging (argument, "readlog", "Glk read log",
                       &gsc_readlog_stream,
                       fileusage_InputRecord | fileusage_BinaryMode,
                       filemode_Read, TRUE, FALSE);
}


/*
 * gsc_command_toggle()
 *
 * The shape every on/off port option takes: "on" and "off" set it, saying so
 * unless it was already that way; a bare command reports the current setting;
 * anything else is a usage error.
 *
 * `label` opens each of those messages and carries its own verb, since some
 * of the options are plural ("Glk abbreviation expansions are ...", "Glk
 * combat assist is ..."), and `on_detail` and `off_detail` finish the two
 * "is now ..." messages -- for most options just ".\n", but the ones that
 * deviate from the original Runner say what they do and why there.
 *
 * `empty_acts` is for options where a bare command is worth treating as "on"
 * rather than as a question, and which therefore never report.
 */
static void
gsc_command_toggle (const char *argument, const char *name,
                    const char *label, scr_bool state,
                    void (*set_state) (scr_bool),
                    const char *on_detail, const char *off_detail,
                    scr_bool empty_acts)
{
  const scr_bool is_empty = strlen (argument) == 0;

  assert (argument);

  if (scr_strcasecmp (argument, "on") == 0 || (empty_acts && is_empty))
    {
      if (state)
        {
          gsc_normal_string (label);
          gsc_normal_string (" already on.\n");
          return;
        }

      set_state (TRUE);
      gsc_normal_string (label);
      gsc_normal_string (" now on");
      gsc_normal_string (on_detail);
    }

  else if (scr_strcasecmp (argument, "off") == 0)
    {
      if (!state)
        {
          gsc_normal_string (label);
          gsc_normal_string (" already off.\n");
          return;
        }

      set_state (FALSE);
      gsc_normal_string (label);
      gsc_normal_string (" now off");
      gsc_normal_string (off_detail);
    }

  else if (is_empty)
    {
      gsc_normal_string (label);
      gsc_normal_char (' ');
      gsc_normal_string (state ? "on" : "off");
      gsc_normal_string (".\n");
    }

  else
    {
      gsc_command_usage (name);
    }
}


/*
 * gsc_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gsc_set_abbreviations (scr_bool state)
{
  gsc_abbreviations_enabled = state;
}

static void
gsc_command_abbreviations (const char *argument)
{
  gsc_command_toggle (argument, "abbreviations",
                      "Glk abbreviation expansions are",
                      gsc_abbreviations_enabled, gsc_set_abbreviations,
                      ".\n", ".\n", FALSE);
}


/*
 * gsc_command_assist()
 *
 * The optional assists, deliberately non-faithful aids for games broken by
 * their own data, switched as "glk assist <name> [on | off]":
 *
 *  - combat: amateur games that left every character's Accuracy and Agility
 *    unconfigured (0), so the 4.0 "accuracy > agility" hit test (0 > 0) never
 *    lands and combat stalemates forever.  Such fully-unconfigured games get
 *    an automatic hit, letting combat play out on the author's
 *    strength-vs-defence basis; games that do configure combat (e.g. Sun
 *    Empire) are never affected.
 *  - move: native-4.0 games authored with a move task action's "To:" combo
 *    left at VB's default -1 (the destination room sitting in Var3).  The
 *    Runner silently ignores such a move, which in e.g. To Hell & Beyond
 *    traps the player in the mansion; an unset move whose Var3 names a real
 *    room is honoured as "to room".
 *  - repeat: pre-4.0 games where a finished task claims a command the player
 *    needs again, answering "You have already done that." (or the task's
 *    RepeatText) ahead of movement and the library, which in e.g. The Long
 *    Journey Home and Inverness Castle blocks the only way on.  Those
 *    commands go on to the ordinary handlers; 4.0 games are unaffected.
 *  - room: a task whose Where room list was left set to no rooms at all,
 *    which the Runner can never run ("You can't do that here!"); such tasks
 *    may run in every room.
 *  - capacity: how the player's carried load is accounted for.  Off, Scarier
 *    mirrors the Runner's running total, updated on take and drop; on, it
 *    recomputes the load afresh from the objects held on each check (legacy
 *    SCARE behaviour).  Only run400 keeps a running total, so the switch
 *    bites only in a 4.0 game; see obj_uses_running_load().  It helps one
 *    known game and breaks another's route, so it is left unlisted: not
 *    offered in the synopsis or help, and reported only while it is on.
 *
 * All are off by default, and switched on per game at startup by the
 * known-game table in os_glk.cpp.  Each was once a command of its own, and
 * the old names -- combatassist, moveassist, repeatassist, roomassist and
 * capacity -- remain as aliases.
 */
scr_bool
gsc_get_capacity (void)
{
  return scr_get_game_capacity_recompute (gsc_game);
}

void
gsc_set_capacity (scr_bool state)
{
  scr_set_game_capacity_recompute (gsc_game, state);
}

typedef const struct
{
  const char * const name;        /* Word after "glk assist". */
  const char * const alias;       /* The command it used to be. */
  const char * const label;       /* Opens its messages, with the verb. */
  scr_bool (* const get_state) (void);
  void (* const set_state) (scr_bool);
  const char * const on_detail;
  const char * const off_detail;
  const scr_bool unlisted;        /* Left out of the synopsis and help. */
} gsc_assist_t;

static gsc_assist_t GSC_ASSISTS[] = {
  {"combat", "combatassist", "Glk combat assist is",
   scr_get_combat_assist, scr_set_combat_assist,
   ".  Note this deviates from the original ADRIFT Runner and is intended"
   " only for games whose combat data is broken (every character's Accuracy"
   " and Agility left at 0).  It takes effect for the next fight; games that"
   " configure combat are unaffected.\n",
   "; combat matches the original ADRIFT Runner.\n", FALSE},
  {"move", "moveassist", "Glk move assist is",
   scr_get_move_assist, scr_set_move_assist,
   ".  Note this deviates from the original ADRIFT Runner and is intended"
   " only for games with a broken move task (a destination room left unset)"
   " that would otherwise be unwinnable.\n",
   "; moves match the original ADRIFT Runner.\n", FALSE},
  {"repeat", "repeatassist", "Glk repeat assist is",
   scr_get_repeat_assist, scr_set_repeat_assist,
   ".  Note this deviates from the original ADRIFT Runner and is intended"
   " only for pre-4.0 games where a finished task blocks a command, such as"
   " an exit, that the game needs again.\n",
   "; finished tasks behave as in the original ADRIFT Runner.\n", FALSE},
  {"room", "roomassist", "Glk room assist is",
   scr_get_room_assist, scr_set_room_assist,
   ".  Note this deviates from the original ADRIFT Runner and is intended"
   " only for games with a task that was left set to run in no room at"
   " all.\n",
   "; tasks run only where the original ADRIFT Runner runs them.\n", FALSE},
  {"capacity", "capacity", "Glk carrying capacity recompute is",
   gsc_get_capacity, gsc_set_capacity,
   "; the load is summed afresh from held objects on each check (legacy"
   " SCARE behaviour).\n",
   "; a running total is kept as the original ADRIFT Runner does.\n", TRUE},
  {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FALSE}
};

static void
gsc_assist_toggle (gsc_assist_t *assist, const char *argument,
                   const char *command)
{
  gsc_command_toggle (argument, command, assist->label, assist->get_state (),
                      assist->set_state, assist->on_detail, assist->off_detail,
                      FALSE);
}

static void
gsc_command_assist (const char *argument)
{
  gsc_assist_t *assist, *matched;
  size_t length;
  int matches;

  assert (argument);

  /* A bare "glk assist", or "status", reports each listed assist, and an
     unlisted one only while it is on, so a player who has it can see it. */
  if (strlen (argument) == 0 || scr_strcasecmp (argument, "status") == 0)
    {
      for (assist = GSC_ASSISTS; assist->name; assist++)
        {
          if (!assist->unlisted || assist->get_state ())
            gsc_assist_toggle (assist, "", "assist");
        }
      return;
    }

  /* The first word names the assist, allowing abbreviation; an exact
     spelling wins outright, as for the commands themselves. */
  length = strcspn (argument, "\t ");
  matched = nullptr;
  matches = 0;
  for (assist = GSC_ASSISTS; assist->name; assist++)
    {
      if (length == strlen (assist->name)
          && scr_strncasecmp (argument, assist->name, length) == 0)
        {
          matched = assist;
          matches = 1;
          break;
        }
      if (scr_strncasecmp (argument, assist->name, length) == 0)
        {
          matched = assist;
          matches++;
        }
    }
  if (matches != 1)
    {
      gsc_command_usage ("assist");
      return;
    }

  argument += length;
  argument += strspn (argument, "\t ");
  gsc_assist_toggle (matched, argument, "assist");
}

/* The old names, each one assist's toggle as a command of its own. */
static void
gsc_command_assist_alias (int index_, const char *argument)
{
  gsc_assist_toggle (GSC_ASSISTS + index_, argument,
                     GSC_ASSISTS[index_].alias);
}

static void
gsc_command_combat_assist (const char *argument)
{
  gsc_command_assist_alias (0, argument);
}

static void
gsc_command_move_assist (const char *argument)
{
  gsc_command_assist_alias (1, argument);
}

static void
gsc_command_repeat_assist (const char *argument)
{
  gsc_command_assist_alias (2, argument);
}

static void
gsc_command_room_assist (const char *argument)
{
  gsc_command_assist_alias (3, argument);
}

static void
gsc_command_capacity (const char *argument)
{
  gsc_command_assist_alias (4, argument);
}


/*
 * gsc_command_patches()
 *
 * Turn the engine's targeted game patches on and off.  A few published games
 * are unwinnable because of a bug in their own data -- a task action the
 * author never filled in, a variable index copied from the wrong task -- and
 * the engine carries the corrections for those, matched on the game's name
 * and author and on the broken data still being there.  On by default here;
 * a game outside the table, or one whose data has since been fixed, is never
 * touched.  Patching happens while the game is read, so a change takes effect
 * the next time the game is loaded.
 */
scr_bool gsc_patches_enabled = TRUE;

static void
gsc_set_patches (scr_bool state)
{
  gsc_patches_enabled = state;
  scr_set_game_patches (state);
}

static void
gsc_command_patches (const char *argument)
{
  gsc_command_toggle (argument, "patches", "Glk game patches are",
                      gsc_patches_enabled, gsc_set_patches,
                      "; a game known to be unwinnable because of a bug in its"
                      " own data is corrected as it loads.  Reload the game for"
                      " this to take effect.\n",
                      "; games load exactly as published.  Reload the game for"
                      " this to take effect.\n", FALSE);
}


/*
 * gsc_command_verbose()
 *
 * Turn the game's verbose room descriptions on and off.  This mirrors the
 * ADRIFT Runner's Verbose user-interface option: when on, the game always gives
 * long descriptions of locations, even ones already visited.  Handling it as a
 * Glk port command (rather than as a game command) means it works even when a
 * game defines its own "verbose" task that would otherwise shadow it.  The
 * plain "verbose" and "brief" game commands continue to work as before.
 */
static void
gsc_set_verbose (scr_bool state)
{
  scr_set_game_verbose (gsc_game, state);
}

static void
gsc_command_verbose (const char *argument)
{
  /* A bare "glk verbose" turns it on, as the game's "verbose" does, so the
     summary asks with "status" (GSC_CMD_STATUS) -- and reads as a bare
     command where that only reports. */
  const scr_bool is_status = scr_strcasecmp (argument, "status") == 0;

  gsc_command_toggle (is_status ? "" : argument, "verbose",
                      "Glk verbose descriptions are",
                      scr_get_game_verbose (gsc_game), gsc_set_verbose,
                      "; the game always gives long descriptions of locations,"
                      " even ones you've visited before.\n",
                      "; long descriptions are given for places never before"
                      " visited and short descriptions otherwise.\n",
                      !is_status);
}


/*
 * gsc_command_notify()
 *
 * Turn the game's score change notification on and off: SCARE's "notify",
 * here for the game whose own "notify" task shadows it, and for the build
 * without SCARE's meta-commands (SCARIER_NO_ABBREVIATIONS).
 */
static void
gsc_set_notify (scr_bool state)
{
  scr_set_game_notify_score_change (gsc_game, state);
}

static void
gsc_command_notify (const char *argument)
{
  gsc_command_toggle (argument, "notify",
                      "Glk score change notification is",
                      scr_get_game_notify_score_change (gsc_game),
                      gsc_set_notify,
                      "; the game will tell you of any changes in the"
                      " score.\n",
                      "; the game will be silent on changes in the score.\n",
                      FALSE);
}


/*
 * gsc_command_statusline()
 *
 * Print the status line as text, the way SCARE's "statusline" (and the
 * "stats" it once answered to) does: the room and the game's status, or the
 * score where the game sets none.  The status window shows the same thing
 * when there is one; this is for a Glk library with no room for it, and for
 * getting the line into a transcript.
 */
static void
gsc_command_statusline (const char *argument)
{
  const scr_char *room, *status;
  assert (argument);

  room = scr_get_game_room (gsc_game);
  if (room == nullptr || strspn (room, " \t\r\n") == strlen (room))
    {
      gsc_normal_string (scr_get_game_name (gsc_game));
      gsc_normal_string (" | ");
      gsc_normal_string (scr_get_game_author (gsc_game));
      gsc_normal_char ('\n');
      return;
    }

  gsc_normal_string (room);
  gsc_normal_string (" | ");
  status = scr_get_game_status_line (gsc_game);
  if (status != nullptr && strspn (status, " \t\r\n") != strlen (status))
    gsc_normal_string (status);
  else
    {
      char buffer[32];

      snprintf (buffer, sizeof (buffer), "Score: %ld",
                (long) scr_get_game_score (gsc_game));
      gsc_normal_string (buffer);
    }
  gsc_normal_char ('\n');
}


/*
 * gsc_set_colour()
 * gsc_command_colour()
 *
 * Turn Adrift colours on and off.
 *
 * Adrift games are written for a Runner that paints its output pane in a
 * palette of its own rather than in the interpreter's theme -- black behind,
 * green replies, red typed text for ADRIFT 4 (the Runner's defaults, and the
 * only place they live: nothing in the .taf carries a colour), the author's
 * own four colours for ADRIFT 5 -- and a game that writes white text, or
 * chooses colours to sit on black, needs that background to read at all.  Off
 * by default, since the interpreter's own theme is what most players want, and
 * on from the start for a game that shows it needs the palette to be read at
 * all (gsc_colour_detect) or when the player asked for the Runner's look with
 * "-c" (gsc_colour_startup_apply).
 *
 * Colour mode publishes the game palette as Normal TextColor/BackColor
 * stylehints so glk_style_measure (and libraries that honour hints) see it,
 * and uses zcolors for per-span output/input colours.  Mid-session toggles
 * rebuild the Glk window tree so open windows pick up the new hints
 * (libraries snapshot stylehints at window open).  Both directions wipe the
 * transcript: Glk gives no other way to repaint a window's background.
 */
#ifdef GSC_HAVE_ZCOLORS
/*
 * gsc_colour_set_normal_hints()
 *
 * Publish or withdraw the game's Normal colours as stylehints.  Set before
 * opening windows (or rebuild after) so measure and themed text agree.
 */
static void
gsc_colour_set_normal_hints (scr_bool on)
{
  if (on)
    {
      glk_stylehint_set (wintype_TextBuffer, style_Normal,
                         stylehint_TextColor, (glsi32) gsc_colour_output);
      glk_stylehint_set (wintype_TextBuffer, style_Normal,
                         stylehint_BackColor, (glsi32) gsc_colour_background);
      glk_stylehint_set (wintype_TextGrid, style_Normal,
                         stylehint_TextColor, (glsi32) gsc_colour_output);
      glk_stylehint_set (wintype_TextGrid, style_Normal,
                         stylehint_BackColor, (glsi32) gsc_colour_background);
    }
  else
    {
      glk_stylehint_clear (wintype_TextBuffer, style_Normal,
                           stylehint_TextColor);
      glk_stylehint_clear (wintype_TextBuffer, style_Normal,
                           stylehint_BackColor);
      glk_stylehint_clear (wintype_TextGrid, style_Normal,
                           stylehint_TextColor);
      glk_stylehint_clear (wintype_TextGrid, style_Normal,
                           stylehint_BackColor);
    }
}

/*
 * gsc_colour_rebuild_windows()
 *
 * Tear down the Glk window tree and recreate every window that was open, so
 * new Normal stylehints are snapshotted into open windows.  The panes reopen
 * in the order a session opens them -- title graphic, side window, then the
 * map -- because each split carves up the story window's remaining space:
 * reopening the side window after the map would land it to the map's left
 * instead of its right, and the player would find the layout rearranged by
 * a colour toggle.  The map redraws itself and the title graphic is redrawn
 * from its retained image number; the side window's text, like the story
 * window's, is gone -- the game reprints it the next time it writes there.
 */
static void
gsc_colour_rebuild_windows (void)
{
  int was_map = gsc_map_shown;
  int was_side = gsc_a5_side_window != nullptr;
#ifdef GSC_HAVE_TITLE_WINDOW
  int was_title = gsc_graphics_window != nullptr;
#endif
  winid_t root;

  root = glk_window_get_root ();
  if (root)
    glk_window_close (root, nullptr);

  gsc_main_window = nullptr;
  gsc_status_window = nullptr;
  gsc_map_window = nullptr;
  gsc_a5_side_window = nullptr;
#ifdef GSC_HAVE_TITLE_WINDOW
  gsc_graphics_window = nullptr;
#endif
  gsc_map_shown = FALSE;
  gsc_map_screen_drop ();

  gsc_open_main_window ();
  gsc_open_status_window ();

#ifdef GSC_HAVE_TITLE_WINDOW
  if (was_title && gsc_title_image != 0)
    gsc_show_title_graphic (gsc_title_image);
#endif
  if (was_side)
    gsc_a5_open_side_window ();
  if (was_map)
    gsc_map_show ();
}

/*
 * gsc_colour_repaint()
 *
 * Take a text window into the game's palette, or hand it back to the theme.
 * Both directions end in a clear, because a Glk window keeps the background a
 * clear gave it and there is no other way to repaint one.
 */
static void
gsc_colour_repaint (winid_t win, scr_bool state)
{
  strid_t stream;
  glui32 bg;

  if (win == nullptr)
    return;
  stream = glk_window_get_stream (win);

  if (state)
    {
      /* Set the colours first: a clear paints the window in the background
         colour currently in force, so the order is what makes it black. */
      gsc_colour_apply (win, GSC_COLOUR_NONE);
      glk_window_clear (win);
    }
  else
    {
      /* Handing a window back is fiddlier than taking it, because a window
         keeps the background a clear gave it: dropping the zcolors to Default
         stops later text being coloured, but leaves the window itself black.
         Only a clear made while a background zcolor is in force repaints it,
         so the theme's own background has to be named for one last clear.
         Three steps, and the order of the first two matters:

           - drop to Default first so any live zcolor is gone before measure;
           - measure style_Normal's background (style table / stylehints, not
             zcolors) and clear with that as the background zcolor;
           - drop to Default again, so text from here on follows the theme. */
      garglk_set_zcolors_stream (stream, zcolor_Default, zcolor_Default);
      if (glk_style_measure (win, style_Normal, stylehint_BackColor, &bg))
        garglk_set_zcolors_stream (stream, zcolor_Default, bg);
      glk_window_clear (win);
      garglk_set_zcolors_stream (stream, zcolor_Default, zcolor_Default);
    }
}

static void
gsc_set_colour (scr_bool state)
{
  scr_bool rebuild;

  gsc_colour_enabled = state;
  gsc_colour_set_normal_hints (state);
  gsc_normal_measure_state = -1;

  /* -c pre-applies hints before the first open; do not destroy that tree. */
  rebuild = gsc_main_window != nullptr
            && !(gsc_colour_hints_preapplied && state);
  gsc_colour_hints_preapplied = FALSE;

  if (rebuild)
    gsc_colour_rebuild_windows ();

  gsc_colour_repaint (gsc_main_window, state);

  /* An ADRIFT 5 game may keep a second pane of its own (Alien Diver's
     "Status"), and it is as much the game's window as the story one: it has to
     change palette with it, or the pane would sit there in the theme's colours
     with the game's text on top.  A rebuild reopens the pane already painted
     (gsc_a5_open_side_window paints when colour mode is on, and a fresh window
     starts in the theme when it is off), so this repaint matters only for a
     path that skipped the rebuild -- and costs nothing when it was redundant,
     the pane being empty until the game next writes there. */
  gsc_colour_repaint (gsc_a5_side_window, state);

  /* The status line has a window of its own, and it follows the story window
     into and out of colour mode whether or not it is asked to: Gargoyle's
     zcolors are global as well as per-stream (gli_override_fg/bg apply to
     every window at once).  Turning colour mode off therefore has to name
     Default on the status stream as well, or the bar would keep the colours
     it was last drawn in.  Going the other way needs nothing here, because
     gsc_status_begin() names the bar's colours on every redraw -- it has to,
     since a grid clear re-seeds the window's attributes.  The redraw below is
     what actually repaints the bar either way, a grid window keeping the
     pixels it was last given. */
  if (gsc_status_window)
    {
      if (!state)
        garglk_set_zcolors_stream (glk_window_get_stream (gsc_status_window),
                                   zcolor_Default, zcolor_Default);
      gsc_status_notify ();
    }

  /* The map pane is painted from style_Normal via measure, and it is drawn
     from the prompt the turn loop prints -- which the command loop that
     brought us here does not reach again until the next real turn.  Repaint
     it now, or the map would sit in the old palette for one command more. */
  gsc_map_screen_drop ();
  gsc_map_redraw ();

  gsc_main_at_line_start = TRUE;
  gsc_main_window_empty = TRUE;
}
#endif

/*
 * gsc_colour_startup_apply()
 *
 * Act on the "-c" switch, or on what the loader detected about the game's own
 * colours, once the story and status windows exist and the ADRIFT 5 loader (if
 * any) has replaced the palette globals with the adventure's own four colours
 * -- both true by the time either main() calls this.  Not called on an
 * autorestore: a restored session brings its own colour state back with it, and
 * neither a switch on the command line nor a guess about the game has any
 * business overriding what the player left the game in.
 */
void
gsc_colour_startup_apply (void)
{
#ifdef GSC_HAVE_ZCOLORS
  if (gsc_colour_startup)
    gsc_set_colour (TRUE);
#endif
}


/*
 * gsc_colour_startup_prepare()
 *
 * The half of starting in colour that comes before the first window is
 * opened: publish Normal colours so that the windows snapshot them, and
 * gsc_colour_startup_apply can then enable zcolors without tearing the tree
 * down again.  Nothing to do unless colour is wanted from the start.
 */
void
gsc_colour_startup_prepare (void)
{
#ifdef GSC_HAVE_ZCOLORS
  if (gsc_colour_startup)
    {
      gsc_colour_set_normal_hints (TRUE);
      gsc_colour_hints_preapplied = TRUE;
    }
#endif
}

static void
gsc_command_colour (const char *argument)
{
#ifdef GSC_HAVE_ZCOLORS
  /* A bare "glk colour" turns colours on rather than asking after them, so
     it is no use as a poll; "status" reports without acting, as it does for the
     logging commands, and is what the summary asks with. */
  const scr_bool poll = scr_strcasecmp (argument, "status") == 0;

  gsc_command_toggle (poll ? "" : argument, "colour",
                      "Glk ADRIFT colours are",
                      gsc_colour_enabled, gsc_set_colour,
                      "; text is drawn in the game's own colours, on the"
                      " black background it was written for.\n",
                      "; text follows the interpreter's own theme.\n", !poll);
#else
  assert (argument);
  gsc_normal_string ("Glk ADRIFT colours are not available with this"
                     " interpreter.\n");
#endif
}


/*
 * gsc_command_undo()
 * gsc_command_restore()
 * gsc_command_restart()
 * gsc_command_quit()
 *
 * UNDO, RESTORE, RESTART and QUIT as Glk commands.
 *
 * Every ADRIFT game understands these words already -- but only while its
 * parser is the thing reading the line.  A game that asks a question of its
 * own takes whatever is typed as the answer: the "please enter your name"
 * task many games open with, an ADRIFT 5 %PopUpInput% or %PopUpChoice%
 * dialog.  At such a prompt QUIT names your character "quit", and there is no
 * way out of a game gone wrong short of killing the interpreter.  Prefixed
 * with "glk" the four are caught by the front end before the game sees them,
 * and so work wherever a line of input is read.
 *
 * The handlers record what was asked and nothing more; the action itself is
 * carried out by the loop that read the line (gsc_meta_perform for scare,
 * gsc_a5_meta_perform for the a5 loop).  It cannot happen here: all four end
 * a turn abruptly -- scare's throw out of the interpreter's main loop, the a5
 * loop's wholesale replacement of the run -- and here we are still inside the
 * command dispatcher, which has the input line to free, and possibly inside
 * the engine itself, midway through rendering the text that asked the
 * question.
 */

/* The action a handler below asked for, awaiting a safe moment to happen. */
gsc_meta_t gsc_meta_pending = GSC_META_NONE;

/*
 * Something to say about an action once it has happened.  Held rather than
 * printed because a successful scare restore, undo-from-memo, or restart
 * never returns to the caller that asked for it; whichever of gsc_meta_report()
 * and the next prompt comes first prints it.
 */
static const char *gsc_meta_message = nullptr;

gsc_meta_t
gsc_meta_take (void)
{
  const gsc_meta_t action = gsc_meta_pending;

  gsc_meta_pending = GSC_META_NONE;
  return action;
}

void
gsc_meta_report (void)
{
  if (gsc_meta_message)
    {
      gsc_normal_string (gsc_meta_message);
      gsc_meta_message = nullptr;
    }
}

static void
gsc_command_undo (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_UNDO;
}

static void
gsc_command_restore (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_RESTORE;
}

static void
gsc_command_restart (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_RESTART;
}

static void
gsc_command_quit (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_QUIT;
}


/*
 * gsc_undo_refusal()
 *
 * What to say when an UNDO cannot be done, given the number of turns played
 * so far: the same two lines for either engine.
 */
const char *
gsc_undo_refusal (scr_int turns)
{
  return turns == 0 ? "You can't undo what hasn't been done.\n"
                    : "Sorry, no more undo is available.\n";
}


/*
 * gsc_meta_perform()
 *
 * Carry out a pending meta-command on the scare engine, called from
 * os_read_line once the line that asked for it has been dealt with.  Each of
 * the four does exactly what the game's own command of that name does,
 * confirmation and all -- the point of these is to reach the action, not to
 * behave differently once there.
 *
 * scr_quit_game(), and a successful scr_restart_game(), scr_load_game() or
 * scr_undo_game_turn() that falls back on a memo, unwind out of the
 * interpreter's main loop and never return here; anything to say about them is
 * left with gsc_meta_report() beforehand for the next prompt to print.
 */
void
gsc_meta_perform (void)
{
  const gsc_meta_t action = gsc_meta_take ();

  if (action == GSC_META_NONE)
    return;

  /* No game to act on.  Quitting is still meaningful -- it is the way out of
     an interpreter showing nothing but an error -- but the rest are not. */
  if (gsc_game == nullptr || !scr_is_game_running (gsc_game))
    {
      if (action == GSC_META_QUIT)
        glk_exit ();
      gsc_normal_string ("There is no game running to do that to.\n");
      return;
    }

  switch (action)
    {
    case GSC_META_UNDO:
      /* Unconfirmed, as the game's own UNDO is. */
      gsc_meta_message = "[The previous turn has been undone.]\n";
      if (!scr_undo_game_turn (gsc_game))
        {
          gsc_meta_message = nullptr;
          gsc_normal_string (gsc_undo_refusal (scr_get_game_turns (gsc_game)));
        }
      gsc_meta_report ();
      break;

    case GSC_META_RESTORE:
      if (os_confirm (SCR_CONF_RESTORE))
        {
          gsc_meta_message = "Ok.\n";
          if (!scr_load_game (gsc_game))
            {
              gsc_meta_message = nullptr;
              gsc_normal_string ("Restore failed.\n");
            }
          gsc_meta_report ();
        }
      break;

    case GSC_META_RESTART:
      /* Says nothing: the replayed opening is the report. */
      if (os_confirm (SCR_CONF_RESTART))
        scr_restart_game (gsc_game);
      break;

    case GSC_META_QUIT:
      if (os_confirm (SCR_CONF_QUIT))
        scr_quit_game (gsc_game);
      break;

    default:
      break;
    }

  /* A fatal engine error under any of those: the action failed, say why. */
  gsc_report_fatal (FALSE);
}


/*
 * gsc_command_print_version_number()
 * gsc_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gsc_command_print_version_number (glui32 version)
{
  char buffer[64];

  snprintf (buffer, sizeof(buffer), "%lu.%lu.%lu",
           (unsigned long) version >> 16,
           (unsigned long) (version >> 8) & 0xff,
           (unsigned long) version & 0xff);
  gsc_normal_string (buffer);
}

static void
gsc_command_version (const char *argument)
{
  glui32 version;
  assert (argument);

  gsc_normal_string ("This is version ");
  gsc_command_print_version_number (GSC_PORT_VERSION);
  gsc_normal_string (" of the Glk Scarier port.\n");

  version = glk_gestalt (gestalt_Version, 0);
  gsc_normal_string ("The Glk library version is ");
  gsc_command_print_version_number (version);
  gsc_normal_string (".\n");
}


/*
 * gsc_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gsc_command_commands (const char *argument)
{
  assert (argument);

  if (scr_strcasecmp (argument, "on") == 0)
    {
      gsc_normal_string ("Glk commands are already on.\n");
    }

  else if (scr_strcasecmp (argument, "off") == 0)
    {
      gsc_commands_enabled = FALSE;
      gsc_normal_string ("Glk commands are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gsc_normal_string ("Glk commands are ");
      gsc_normal_string (gsc_commands_enabled ? "on" : "off");
      gsc_normal_string (".\n");
    }

  else
    {
      gsc_command_usage ("commands");
    }
}


/*
 * gsc_command_license()
 *
 * Print licensing terms.
 */
static void
gsc_command_license (const char *argument)
{
  assert (argument);

  gsc_normal_string ("This program is free software; you can redistribute it"
                     " and/or modify it under the terms of version 2 of the"
                     " GNU General Public License as published by the Free"
                     " Software Foundation.\n\n");

  gsc_normal_string ("This program is distributed in the hope that it will be"
                     " useful, but ");
  gsc_standout_string ("WITHOUT ANY WARRANTY");
  gsc_normal_string ("; without even the implied warranty of ");
  gsc_standout_string ("MERCHANTABILITY");
  gsc_normal_string (" or ");
  gsc_standout_string ("FITNESS FOR A PARTICULAR PURPOSE");
  gsc_normal_string (".  See the GNU General Public License for more"
                     " details.\n\n");

  gsc_normal_string ("You should have received a copy of the GNU General"
                     " Public License along with this program; if not, write"
                     " to the Free Software Foundation, Inc., 51 Franklin"
                     " Street, Fifth Floor, Boston, MA 02110-1301 USA\n\n");

  gsc_normal_string ("Please report any bugs, omissions, or misfeatures to ");
  gsc_standout_string ("simon_baldwin@yahoo.com");
  gsc_normal_string (".\n");
}


/*
 * gsc_a5_hint_text()
 *
 * Render one of a clsHint's answer blocks (<Subtle>/<Sledgehammer>) to plain
 * text, or NULL when the game leaves it empty -- an author who wrote only a
 * subtle nudge should not be asked about a blank sledgehammer.  The caller
 * owns the returned string.
 */
static char *
gsc_a5_hint_text (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  char *text;

  if (wrapper == nullptr)
    return nullptr;

  text = a5text_describe (st, wrapper);
  if (text != nullptr && text[strspn (text, " \t\n\r")] == '\0')
    {
      free (text);
      return nullptr;
    }
  return text;
}


/*
 * gsc_a5_display_hints()
 *
 * The ADRIFT 5 hints display.  The runner keeps clsHint out of the parser
 * altogether and puts it behind a Hints window on the menu bar, so there is no
 * game command to collide with; here it hangs off "glk hints" instead, which
 * also leaves the ~40% of ADRIFT 5 games that implement a HINT task of their
 * own free to answer a bare "hint" themselves.
 *
 * A hint carries its own restriction block, which is how an author keeps the
 * list to the puzzles actually in play; hints whose block fails are passed
 * over exactly as though they were not there.
 */
static void
gsc_a5_display_hints (void)
{
  a5_state_t *st = a5run_state (gsc_a5_run);
  int index_, refused, offered;

  refused = 0;
  offered = 0;
  for (index_ = 0; index_ < gsc_a5_adv->n_hints; index_++)
    {
      const a5_hint_t *hint = &gsc_a5_adv->hints[index_];
      char *subtle, *sledgehammer;

      if (!a5restr_pass (st, hint->restrictions))
        continue;

      subtle = gsc_a5_hint_text (st, hint->subtle);
      sledgehammer = gsc_a5_hint_text (st, hint->sledgehammer);

      /* Games ship half-finished hints -- an editor row with an empty question
         and neither answer written (Death Shack's Hint2, Blender's Hint2).
         There is nothing to ask about, so pass over them silently rather than
         prompt against a blank heading. */
      if ((hint->question == nullptr || hint->question[0] == '\0')
          && subtle == nullptr && sledgehammer == nullptr)
        continue;

      /* If enough refusals, offer a way out of the loop. */
      if (refused >= GSC_HINT_REFUSAL_LIMIT)
        {
          if (!os_confirm (GSC_CONF_CONTINUE_HINTS))
            {
              free (subtle);
              free (sledgehammer);
              break;
            }
          refused = 0;
        }

      /* clsHint.Question is a plain String, not a Description: it is shown as
         the author typed it, with no segment selection or ALR pass. */
      if (!gsc_hint_present (hint->question ? hint->question : "", subtle,
                             sledgehammer))
        refused++;
      offered++;

      free (subtle);
      free (sledgehammer);
    }

  if (offered == 0)
    gsc_normal_string ("There are currently no hints available.\n");
}


/*
 * gsc_command_hints()
 *
 * Show the game's built-in hints, if it has any.
 */
static void
gsc_command_hints (const char *argument)
{
  assert (argument);

  if (gsc_is_a5)
    {
      if (gsc_a5_run == nullptr || gsc_a5_adv == nullptr
          || gsc_a5_adv->n_hints == 0)
        {
          gsc_normal_string ("This game does not have any hints.\n");
          return;
        }
      gsc_a5_display_hints ();
      return;
    }

  /* ADRIFT <=4 hints hang off tasks, and the game's own "hints" command is
     the usual way in; this is the same display, for the player who reached
     for the Glk command instead.  scr_get_first_game_hint filters to the
     hints currently in play, so an empty iteration is the "not yet" case
     rather than a game without hints. */
  if (gsc_game == nullptr)
    return;
  if (scr_get_first_game_hint (gsc_game) == nullptr)
    {
      gsc_normal_string ("There are currently no hints available.\n");
      return;
    }
  os_display_hints (gsc_game);
}


/* Glk subcommands and handler functions. */
/* Glk subcommand flags. */
enum
{
  GSC_CMD_ARGUMENT = 1 << 0,  /* The command takes an argument. */
  GSC_CMD_A5 = 1 << 1,        /* Offered in the a5 loop as well. */
  GSC_CMD_ALIAS = 1 << 2,     /* Another name for an entry listed above. */
  GSC_CMD_ACTION = 1 << 3,    /* Does something rather than carrying a
                                 setting, so has nothing to report. */
  GSC_CMD_STATUS = 1 << 4     /* Reports its setting on "status"; an empty
                                 argument makes it act instead. */
};

typedef const struct
{
  const char * const command;                     /* Glk subcommand. */
  void (* const handler) (const char *argument);  /* Subcommand handler. */
  const int flags;                                /* GSC_CMD_* flags. */
  const char * const usage_subject;               /* Noun for the synopsis. */
  const char * const * const usage_options;       /* Arguments accepted, NULL
                                                     terminated; NULL for a
                                                     command taking none. */
  const char * const help;                        /* Its "glk help" entry;
                                                     `...` marks the spans
                                                     shown in standout. */
} gsc_command_t;
typedef gsc_command_t *gsc_commandref_t;

/* Argument lists for the one-line synopsis printed by gsc_command_usage(),
   and at the foot of a command's entry in "glk help". */
static const char * const GSC_USAGE_ONOFFSTATUS[] = {"on", "off", "status",
                                                     nullptr};
static const char * const GSC_USAGE_ONOFF[] = {"on", "off", nullptr};
static const char * const GSC_USAGE_MAP[] = {"on", "off", "top", "right",
                                             "zoom [in | out | auto | N]",
                                             nullptr};
static const char * const GSC_USAGE_ASSIST[] = {"combat [on | off]",
                                               "move [on | off]",
                                               "repeat [on | off]",
                                               "room [on | off]", nullptr};
static const char * const GSC_USAGE_ZOOM[] = {"in", "out", "auto", "N",
                                              nullptr};

/* The "glk help" entry for each command, printed by gsc_command_help().  Text
   between backquotes is shown in standout, as a command to type. */
static const char GSC_HELP_SUMMARY[] =
  "Prints a summary of all the current Glk Scarier settings.\n";

static const char GSC_HELP_MAP[] =
  "Shows the game's map beside the story, as the ADRIFT Runner does: the"
  " rooms you have visited, the ways between them, and where you are"
  " now.\n\n"
  "Use `glk map on` to show the map and `glk map off` to hide it again;"
  " plain `map` toggles it too, unless the game uses MAP for something of"
  " its own.\n\n"
  "Some ADRIFT 5 games begin with their map already showing, and this"
  " one may be one of them; either way, whichever of `glk map on` or `glk"
  " map off` you use last is remembered for this game, and the next session"
  " starts that way.  A map with nothing on it yet -- during a title or"
  " options screen, say -- waits rather than opening empty, and appears as"
  " soon as you reach somewhere it can show.\n\n"
  "For games with wide maps, `glk map top` (or `glk map above`) moves the"
  " map to a band across the top of the screen, above the status line; `glk"
  " map right` puts it back beside the story.  This is remembered for the"
  " game as well, so the map comes back where you left it.\n\n"
  "The map zooms itself to fit its window.  Use `glk zoom in` and `glk zoom"
  " out` to zoom by hand instead; the view then pans to keep you on-screen."
  "  `glk zoom auto` restores the automatic fit.\n";

static const char GSC_HELP_ZOOM[] =
  "Zooms the game's map, which otherwise fits itself to its window.\n\n"
  "Use `glk zoom in` and `glk zoom out` to zoom by hand; the view then pans"
  " to keep you on-screen.  Plain `glk zoom` zooms in, and `glk zoom auto`"
  " (or `glk zoom default`) restores the automatic fit.  `glk zoom N` sets"
  " the scale to N pixels per map unit, from 3 to 32; below 10 the rooms are"
  " drawn without their names.  A map too big for its window can also be"
  " moved about, and zoomed, with the buttons at its top right.  Each command"
  " is also understood with a map prefix, as in `glk map zoom in`.\n";

static const char GSC_HELP_SCRIPT[] =
  "Logs the game's output to a file.\n\n"
  "Use `glk script on` to begin logging game output, and `glk script off`"
  " to end it; plain `glk script` begins logging too.  Glk Scarier will ask"
  " you for a file when you turn scripts on.  `glk script status` says"
  " whether logging is currently on.\n";

static const char GSC_HELP_INPUTLOG[] =
  "Records the commands you type into a game.\n\n"
  "Use `glk inputlog on`, to begin recording your commands, and `glk"
  " inputlog off` to turn off input logs; plain `glk inputlog` begins"
  " recording too, and `glk inputlog status` says whether recording is"
  " currently on.  You can play back recorded commands into a game with the"
  " `glk readlog` command.\n";

static const char GSC_HELP_READLOG[] =
  "Plays back commands recorded with `glk inputlog on`.\n\n"
  "Use `glk readlog on`, or just `glk readlog`.  Command play back stops at"
  " the end of the file.  You can also play back commands from a text file"
  " created using any standard editor.  `glk readlog status` says whether"
  " play back is currently on.\n";

static const char GSC_HELP_ABBREVIATIONS[] =
  "Controls abbreviation expansion.\n\n"
  "Glk Scarier automatically expands several standard single letter"
  " abbreviations for you; for example, \"x\" becomes \"examine\".  Use"
  " `glk abbreviations on` to turn this feature on, and `glk abbreviations"
  " off` to turn it off.  While the feature is on, you can bypass"
  " abbreviation expansion for an individual game command by prefixing it"
  " with a single quote.  Abbreviations never override the game's own"
  " commands: if the game already recognises the single letter you typed"
  " (for example as a battle or menu choice), it is passed through"
  " unchanged.\n";

static const char GSC_HELP_CAPACITY[] =
  "Controls how your carried load is accounted for.\n\n"
  "By default Scarier keeps a running total as you take and drop, like the"
  " ADRIFT Runner.  Use `glk capacity on` to recompute it instead from what"
  " you are holding (legacy SCARE behaviour), and `glk capacity off` to go"
  " back; `glk assist capacity on` and `off` do the same.  It changes when a"
  " take is refused as too much to carry, and what"
  " `count` reports.  Only a 4.0 Runner keeps such a total; earlier ones"
  " recompute anyway, so for a 3.7, 3.8 or 3.9 game the setting does"
  " nothing.  No known game needs it to be completed.\n";

static const char GSC_HELP_PATCHES[] =
  "Corrects games broken by their own data.\n\n"
  "A few published ADRIFT games cannot be finished because of a bug in the"
  " game file itself -- a task that describes handing you something but was"
  " left with no action to do it, a control that writes the wrong variable."
  "  Scarier carries the correction for each of those games and applies it"
  " as the game loads, which is what `glk patches on` (the default) does;"
  " use `glk patches off` to play the game exactly as published.  Only the"
  " handful of games in the engine's table are ever touched, and only while"
  " they still hold the broken value, so a later or already-fixed release"
  " runs unaltered.  Reload the game for a change to this setting to take"
  " effect.\n";

static const char GSC_HELP_ASSIST[] =
  "Helps with games broken by their own data.\n\n"
  "A few ADRIFT games cannot be finished in the original ADRIFT Runner"
  " because of the way they were written.  Each assist works around one"
  " such problem, and deliberately deviates from the Runner to do it:\n\n"
  "`combat` -- every character's Accuracy and Agility were left at 0, so no"
  " attack ever lands and combat stalemates forever.  The assist gives an"
  " automatic hit, letting combat play out on the author's"
  " strength-vs-defence basis.  Games that do configure combat are never"
  " affected.\n\n"
  "`move` -- a move's destination room was left unset, and the Runner"
  " ignores the move.  The assist honours it.\n\n"
  "`repeat` -- in games made with ADRIFT 3.9 or earlier, a task that has"
  " been done answers every later command that matches it with \"You have"
  " already done that.\", even a move the game needs again.  The assist"
  " lets such commands through to movement and the other built-in"
  " commands.  It does nothing in a 4.0 game.\n\n"
  "`room` -- a task was set to run in no room at all, and the Runner"
  " answers it with \"You can't do that here!\" wherever you are.  The"
  " assist lets such tasks run in every room.\n\n"
  "Use `glk assist combat on` to turn an assist on, and `glk assist combat"
  " off` to turn it off again; plain `glk assist` says which are on.  For a"
  " few games known to be uncompletable without them, the ones they need"
  " are switched on automatically at startup.\n";

static const char GSC_HELP_VERBOSE[] =
  "Controls verbose room descriptions.\n\n"
  "Use `glk verbose on` to make the game always give long descriptions of"
  " locations, even ones you have visited before, and `glk verbose off` to"
  " give long descriptions only for places never before visited.  This"
  " mirrors the ADRIFT Runner's Verbose option, and works even when a game"
  " defines its own \"verbose\" command.\n";

static const char GSC_HELP_NOTIFY[] =
  "Controls score change notification.\n\n"
  "Use `glk notify on` to have the game tell you whenever your score"
  " changes, and `glk notify off` to keep it silent.  Plain `glk notify`"
  " says which it is.  This is the setting the `notify` command changes,"
  " and works even when a game defines its own \"notify\" command.\n";

static const char GSC_HELP_STATUSLINE[] =
  "Prints the status line: where you are, and the game's own status or"
  " your score.\n\n"
  "This is what the status window shows, as a line of text -- for an"
  " interpreter with no status window, or to put it on record in a"
  " transcript.  `glk stats` is the same command.\n";

static const char GSC_HELP_VERSION[] =
  "Prints the version numbers of the Glk library and the Glk Scarier port.\n";

static const char GSC_HELP_COMMANDS[] =
  "Turn off Glk commands.\n\n"
  "Use `glk commands off` to disable all Glk commands, including this one. "
  " Once turned off, there is no way to turn Glk commands back on while"
  " inside the game.\n";

static const char GSC_HELP_COLOUR[] =
  "Shows the story in the colours ADRIFT would have used.\n\n"
  "Use `glk colour on` to clear the screen to black and draw what follows"
  " in the game's own colours -- the ADRIFT Runner's green replies and red"
  " typed text for an ADRIFT 4 game, or the colours the author chose for an"
  " ADRIFT 5 one -- honouring any colour the game asks for as it goes.  Use"
  " `glk colour off` to clear the screen again and go back to the colours"
  " of the interpreter's own theme. \n\n"
  "Games written for a black screen can be hard to read without this, so a"
  " game that sets colours of its own, or that would show text too close to"
  " the interpreter's own colours to read, starts with this turned on.\n";

static const char GSC_HELP_META[] =
  "Takes back a turn, restores a saved game, starts over, or stops"
  " playing.\n\n"
  "`glk undo`, `glk restore`, `glk restart` and `glk quit` do just what the"
  " game's own commands of those names do.  They are here for the places"
  " where those are out of reach: a game that asks a question of its own --"
  " for your name, say -- takes anything you type as the answer, and would"
  " read `quit` as the name you had chosen.  A Glk command is recognised at"
  " any prompt.\n";

static const char GSC_HELP_HINTS[] =
  "Shows the hints the game's author wrote, as the ADRIFT Runner's Hints"
  " window does.\n\n"
  "Each hint asks its question first, then offers a subtle answer and,"
  " after that, one that simply tells you; answer `N` to either and the"
  " next hint comes up.  Only the hints for puzzles you have reached are"
  " listed.\n\n"
  "`glk hint` abbreviates to the same command.  Many games answer a plain"
  " `hint` with hints of their own, which are a different thing and are"
  " worth trying too.\n";

static const char GSC_HELP_LICENSE[] =
  "Prints Glk Scarier's software license.\n";

static void gsc_command_summary (const char *argument);

/* Commands not flagged GSC_CMD_A5 are ADRIFT <=4 engine specifics:
   abbreviations (the ADRIFT 5 standard library already defines x/l/i/z...),
   assist and its aliases (4.0 Battle System / task quirks), verbose (a
   4.0 room-description mode; ADRIFT 5 leaves this to the game), and notify
   and statusline (SCARE's own, over the 4.x game state).

   Entries flagged GSC_CMD_ALIAS are alternative names for a command listed
   above them.  They are found by the dispatcher and by "glk help", but left
   out of the command listing and the summary poll, so that the alias neither
   pads the list nor makes its command report itself twice.

   "help" has no entry of its own in "glk help": asking for help on help
   prints the command list, as plain "glk help" does. */
static gsc_command_t GSC_COMMAND_TABLE[] = {
  {"summary", gsc_command_summary,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_SUMMARY},
  {"script", gsc_command_script,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_STATUS,
   "script", GSC_USAGE_ONOFFSTATUS, GSC_HELP_SCRIPT},
  {"transcript", gsc_command_script,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ALIAS | GSC_CMD_STATUS,
   "transcript", GSC_USAGE_ONOFFSTATUS, GSC_HELP_SCRIPT},
  {"inputlog", gsc_command_inputlog,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_STATUS,
   "input logging", GSC_USAGE_ONOFFSTATUS, GSC_HELP_INPUTLOG},
  {"readlog", gsc_command_readlog,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_STATUS,
   "read log", GSC_USAGE_ONOFFSTATUS, GSC_HELP_READLOG},
  {"abbreviations", gsc_command_abbreviations,
   GSC_CMD_ARGUMENT,
   "abbreviation expansions", GSC_USAGE_ONOFF, GSC_HELP_ABBREVIATIONS},
  {"assist", gsc_command_assist,
   GSC_CMD_ARGUMENT,
   "assist", GSC_USAGE_ASSIST, GSC_HELP_ASSIST},
  /* The assists' names from when each was a command of its own.  capacity
     is an alias too, though not of a listed assist: it is kept out of the
     command list and the synopsis, and "glk help capacity" is its only
     documentation. */
  {"combatassist", gsc_command_combat_assist,
   GSC_CMD_ARGUMENT | GSC_CMD_ALIAS,
   "combat assist", GSC_USAGE_ONOFF, GSC_HELP_ASSIST},
  {"moveassist", gsc_command_move_assist,
   GSC_CMD_ARGUMENT | GSC_CMD_ALIAS,
   "move assist", GSC_USAGE_ONOFF, GSC_HELP_ASSIST},
  {"repeatassist", gsc_command_repeat_assist,
   GSC_CMD_ARGUMENT | GSC_CMD_ALIAS,
   "repeat assist", GSC_USAGE_ONOFF, GSC_HELP_ASSIST},
  {"roomassist", gsc_command_room_assist,
   GSC_CMD_ARGUMENT | GSC_CMD_ALIAS,
   "room assist", GSC_USAGE_ONOFF, GSC_HELP_ASSIST},
  {"capacity", gsc_command_capacity,
   GSC_CMD_ARGUMENT | GSC_CMD_ALIAS,
   "carrying capacity recompute", GSC_USAGE_ONOFF, GSC_HELP_CAPACITY},
  {"patches", gsc_command_patches,
   GSC_CMD_ARGUMENT,
   "game patches", GSC_USAGE_ONOFF, GSC_HELP_PATCHES},
  {"verbose", gsc_command_verbose,
   GSC_CMD_ARGUMENT | GSC_CMD_STATUS,
   "verbose descriptions", GSC_USAGE_ONOFFSTATUS, GSC_HELP_VERBOSE},
  {"notify", gsc_command_notify,
   GSC_CMD_ARGUMENT,
   "score change notification", GSC_USAGE_ONOFF, GSC_HELP_NOTIFY},
  {"statusline", gsc_command_statusline,
   GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_STATUSLINE},
  {"stats", gsc_command_statusline,
   GSC_CMD_ALIAS | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_STATUSLINE},
  {"version", gsc_command_version,
   GSC_CMD_A5,
   nullptr, nullptr, GSC_HELP_VERSION},
  {"map", gsc_command_map,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ACTION,
   "map", GSC_USAGE_MAP, GSC_HELP_MAP},
  {"zoom", gsc_command_zoom,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ACTION,
   "zoom", GSC_USAGE_ZOOM, GSC_HELP_ZOOM},
  {"commands", gsc_command_commands,
   GSC_CMD_ARGUMENT | GSC_CMD_A5,
   "commands", GSC_USAGE_ONOFF, GSC_HELP_COMMANDS},
  /* "color" is a full alias rather than a prefix: neither spelling is a
     prefix of the other, so each resolves on its own, at the cost of "glk col"
     matching both and reporting itself ambiguous.  The plurals are what a
     player who has just read "Glk ADRIFT colours are..." is likely to type
     back; they are prefixed by the singulars, and rely on gsc_command_lookup()
     preferring an exact spelling to keep "glk colour" unambiguous. */
  {"colour", gsc_command_colour,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_STATUS,
   "ADRIFT colours", GSC_USAGE_ONOFFSTATUS, GSC_HELP_COLOUR},
  {"colours", gsc_command_colour,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ALIAS | GSC_CMD_STATUS,
   "ADRIFT colours", GSC_USAGE_ONOFFSTATUS, GSC_HELP_COLOUR},
  {"color", gsc_command_colour,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ALIAS | GSC_CMD_STATUS,
   "ADRIFT colours", GSC_USAGE_ONOFFSTATUS, GSC_HELP_COLOUR},
  {"colors", gsc_command_colour,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ALIAS | GSC_CMD_STATUS,
   "ADRIFT colours", GSC_USAGE_ONOFFSTATUS, GSC_HELP_COLOUR},
  /* The four meta-commands.  "restore" and "restart" share a prefix, so each
     needs five letters to resolve; "quit" answers to "glk q". */
  {"undo", gsc_command_undo,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_META},
  {"restore", gsc_command_restore,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_META},
  {"restart", gsc_command_restart,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_META},
  {"quit", gsc_command_quit,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_META},
  /* No "hint" alias row: it would be redundant, as a bare prefix of the sole
     "hints" row it already resolves. */
  {"hints", gsc_command_hints,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_HINTS},
  {"license", gsc_command_license,
   GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, GSC_HELP_LICENSE},
  {"help", gsc_command_help,
   GSC_CMD_ARGUMENT | GSC_CMD_A5 | GSC_CMD_ACTION,
   nullptr, nullptr, nullptr},
  {nullptr, nullptr, 0, nullptr, nullptr, nullptr}
};


/*
 * gsc_command_in_scope()
 *
 * Return TRUE if a Glk command table entry applies to the engine driving the
 * current game: everything for ADRIFT <=4 (scare), only the entries flagged
 * GSC_CMD_A5 for ADRIFT 5 (the a5 loop).
 */
static int
gsc_command_in_scope (gsc_commandref_t entry)
{
  return !gsc_is_a5 || (entry->flags & GSC_CMD_A5);
}


/*
 * gsc_command_lookup()
 *
 * Find the table entry for a Glk command name, which the player may have
 * abbreviated.  An abbreviation has to be a prefix of exactly one command --
 * of however many of its names: "stat" is "statusline" and "stats" both -- but
 * an exact spelling always wins outright, so that a command which is itself
 * the prefix of another -- "colour" of "colours" -- still resolves.
 *
 * Returns the entry, or NULL if nothing matched or an abbreviation was
 * ambiguous; the number of entries the name could have stood for is stored in
 * matches, so a caller can tell those two apart.
 */
static gsc_commandref_t
gsc_command_lookup (const char *command, int *matches)
{
  gsc_commandref_t entry, matched;
  int count;
  assert (command);

  count = 0;
  matched = nullptr;
  for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
    {
      if (!gsc_command_in_scope (entry))
        continue;

      if (scr_strcasecmp (command, entry->command) == 0)
        {
          if (matches)
            *matches = 1;
          return entry;
        }

      /* An alias of the command already found is that command again. */
      if (scr_strncasecmp (command, entry->command, strlen (command)) == 0
          && !(matched && entry->handler == matched->handler))
        {
          count++;
          matched = entry;
        }
    }

  if (matches)
    *matches = count;
  return count == 1 ? matched : nullptr;
}


/*
 * gsc_command_usage()
 *
 * Print the one-line synopsis of the arguments a Glk command accepts, for
 * example "Glk map can be on, off, top, right, or zoom [in | out | auto]."  Both
 * a command handed an argument it doesn't understand and the foot of that
 * command's "glk help" entry print this, so the two can never disagree.
 */
void
gsc_command_usage (const char *command)
{
  gsc_commandref_t entry;
  int count, index_;
  assert (command);

  for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
    {
      if (scr_strcasecmp (command, entry->command) == 0)
        break;
    }
  if (!entry->command || !entry->usage_options)
    return;

  gsc_normal_string ("Glk ");
  gsc_normal_string (entry->usage_subject);
  gsc_normal_string (" can be ");

  for (count = 0; entry->usage_options[count]; count++)
    ;
  for (index_ = 0; index_ < count; index_++)
    {
      if (index_ > 0)
        gsc_normal_string (index_ < count - 1 ? ", "
                                              : count > 2 ? ", or " : " or ");
      gsc_standout_string (entry->usage_options[index_]);
    }
  gsc_normal_string (".\n");
}


/*
 * gsc_command_summary()
 *
 * Report all current Glk settings.
 */
static void
gsc_command_summary (const char *argument)
{
  gsc_commandref_t entry;
  assert (argument);

  /*
   * Call handlers that have status to report with an empty argument,
   * prompting each to print its current setting.  The logging commands,
   * the colour mode and verbose act rather than report on an empty argument,
   * so those (GSC_CMD_STATUS) are polled with an explicit "status" instead.  The
   * commands that only ever act (GSC_CMD_ACTION) are not called at all:
   * asking after the settings must not display the help, or quit the game.
   */
  for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
    {
      if ((entry->flags & (GSC_CMD_ACTION | GSC_CMD_ALIAS))
          || !gsc_command_in_scope (entry))
        continue;

      entry->handler ((entry->flags & GSC_CMD_STATUS) ? "status" : "");
    }
}


/*
 * gsc_command_help_print()
 *
 * Print a command's "glk help" entry, showing the spans between backquotes in
 * standout and the rest as normal text.
 */
static void
gsc_command_help_print (const char *help)
{
  int standout;
  assert (help);

  standout = FALSE;
  while (*help)
    {
      size_t length;
      char *span;

      length = strcspn (help, "`");
      span = (char *) gsc_malloc (length + 1);
      memcpy (span, help, length);
      span[length] = '\0';
      if (standout)
        gsc_standout_string (span);
      else
        gsc_normal_string (span);
      free (span);

      help += length;
      if (*help == '`')
        {
          help++;
          standout = !standout;
        }
    }
}


/*
 * gsc_command_help()
 *
 * Document the available Glk commands.
 */
void
gsc_command_help (const char *command)
{
  gsc_commandref_t entry, matched;
  int matches;
  assert (command);

  if (strlen (command) == 0)
    {
      gsc_commandref_t last;

      /* Zoom is left off the list; it belongs to the map, and is documented
         under "glk help map" instead.  Aliases are left off too, and named in
         the help for the command they stand in for. */
      last = nullptr;
      for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
        {
          if (gsc_command_in_scope (entry)
              && !(entry->flags & GSC_CMD_ALIAS)
              && entry->handler != gsc_command_zoom)
            last = entry;
        }

      gsc_normal_string ("Glk commands are");
      for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
        {
          if (!gsc_command_in_scope (entry)
              || (entry->flags & GSC_CMD_ALIAS)
              || entry->handler == gsc_command_zoom)
            continue;

          gsc_normal_string (entry == last ? " and " : " ");
          gsc_standout_string (entry->command);
          gsc_normal_string (entry == last ? ".\n\n" : ",");
        }

      gsc_normal_string ("Glk commands may be abbreviated, as long as"
                         " the abbreviation is unambiguous.  Use ");
      gsc_standout_string ("glk help");
      gsc_normal_string (" followed by a Glk command name for help on that"
                         " command.\n");
      return;
    }

  matched = gsc_command_lookup (command, &matches);
  if (!matched)
    {
      gsc_normal_string ("The Glk command ");
      gsc_standout_string (command);
      gsc_normal_string (matches == 0 ? " is not valid.  Try "
                                      : " is ambiguous.  Try ");
      gsc_standout_string ("glk help");
      gsc_normal_string (" for more information.\n");
      return;
    }

  if (matched->handler == gsc_command_help)
    {
      gsc_command_help ("");
      return;
    }

  if (matched->help)
    gsc_command_help_print (matched->help);
  else
    gsc_normal_string ("There is no help available on that Glk command."
                       "  Sorry.\n");

  /* Close with the same synopsis the command itself prints when handed an
     argument it doesn't understand. */
  if (matched->usage_options)
    {
      gsc_normal_char ('\n');
      gsc_command_usage (matched->command);
    }
}


/*
 * gsc_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 */
int
gsc_command_escape (const char *string)
{
  int posn;
  char *string_copy, *command, *argument;
  assert (string);

  /*
   * Return FALSE if the string doesn't begin with the Glk command escape
   * introducer.
   */
  posn = strspn (string, "\t ");
  if (scr_strncasecmp (string + posn, "glk", strlen ("glk")) != 0)
    return FALSE;

  /* Take a copy of the string, without any leading space or introducer. */
  string_copy = (decltype(string_copy)) gsc_malloc (strlen (string + posn) + 1
                                                    - strlen ("glk"));
  strncpy (string_copy, string + posn + strlen ("glk"),
           strlen (string + posn) + 1 - strlen ("glk"));

  /*
   * Find the subcommand; the first word in the string copy.  Find its end,
   * and ensure it terminates with a NUL.
   */
  posn = strspn (string_copy, "\t ");
  command = string_copy + posn;
  posn += strcspn (string_copy + posn, "\t ");
  if (string_copy[posn] != '\0')
    string_copy[posn++] = '\0';

  /*
   * Now find any argument data for the command: the rest of the line, less
   * leading and trailing whitespace.  Most subcommands take a single word,
   * but "glk map zoom in" takes two.
   */
  posn += strspn (string_copy + posn, "\t ");
  argument = string_copy + posn;
  posn = (int) strlen (argument);
  while (posn > 0
         && (argument[posn - 1] == ' ' || argument[posn - 1] == '\t'))
    argument[--posn] = '\0';

  /*
   * Try to handle the command and argument as a Glk subcommand.  If it
   * doesn't run unambiguously, print command usage.  Treat an empty command
   * as "help".
   */
  if (strlen (command) > 0)
    {
      gsc_commandref_t matched;
      int matches;

      /* Find the table entry the command names, allowing abbreviation. */
      matched = gsc_command_lookup (command, &matches);

      /* If the match was unambiguous, call the command handler. */
      if (matched)
        {
          gsc_normal_char ('\n');

          /* "glk <command> help" is another way of writing "glk help
             <command>"; the two print the same thing. */
          if (scr_strcasecmp (argument, "help") == 0
              && matched->handler != gsc_command_help)
            {
              gsc_command_help (matched->command);
              free (string_copy);
              return TRUE;
            }

          matched->handler (argument);

          if (!(matched->flags & GSC_CMD_ARGUMENT) && strlen (argument) > 0)
            {
              gsc_normal_string ("[The ");
              gsc_standout_string (matched->command);
              gsc_normal_string (" command ignores arguments.]\n");
            }
        }

      /* No match, or the command was ambiguous. */
      else
        {
          gsc_normal_string ("\nThe Glk command ");
          gsc_standout_string (command);
          gsc_normal_string (" is ");
          gsc_normal_string (matches == 0 ? "not valid" : "ambiguous");
          gsc_normal_string (".  Try ");
          gsc_standout_string ("glk help");
          gsc_normal_string (" for more information.\n");
        }
    }
  else
    {
      gsc_normal_char ('\n');
      gsc_command_help ("");
    }

  /* The string contained a Glk command; return TRUE. */
  free (string_copy);
  return TRUE;
}

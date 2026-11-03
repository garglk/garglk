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
 * os_glk_input.cpp: line input for ADRIFT <=4 -- abbreviations, os_read_line
 * and its debugger twin, os_confirm.  Split out of os_glk.cpp; see
 * os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*---------------------------------------------------------------------*/
/*  Glk port input functions                                           */
/*---------------------------------------------------------------------*/

/* Table of single-character command abbreviations. */
typedef const struct
{
  const char abbreviation;      /* Abbreviation character. */
  const char *const expansion;  /* Expansion string. */
} gsc_abbreviation_t;
typedef gsc_abbreviation_t *gsc_abbreviationref_t;

static gsc_abbreviation_t GSC_ABBREVIATIONS[] = {
  {'c', "close"},    {'g', "again"},  {'i', "inventory"},
  {'k', "attack"},   {'l', "look"},   {'p', "open"},
  {'q', "quit"},     {'r', "drop"},   {'t', "take"},
  {'x', "examine"},  {'y', "yes"},    {'z', "wait"},
  {'\0', nullptr}
};


/*
 * gsc_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems.
 */
static void
gsc_expand_abbreviations (char *buffer, int size)
{
  char *command, abbreviation;
  const char *expansion;
  gsc_abbreviationref_t entry;
  assert (buffer);

  /* Ignore anything that isn't a single letter command. */
  command = buffer + strspn (buffer, "\t ");
  if (!(strlen (command) == 1
        || (strlen (command) > 1 && isspace (command[1]))))
    return;

  /* Scan the abbreviations table for a match. */
  abbreviation = glk_char_to_lower ((unsigned char) command[0]);
  expansion = nullptr;
  for (entry = GSC_ABBREVIATIONS; entry->expansion; entry++)
    {
      if (entry->abbreviation == abbreviation)
        {
          expansion = entry->expansion;
          break;
        }
    }

  /*
   * Give author-defined commands precedence over our conveniences.  Many
   * games use single letters as menu choices (battle/conversation menus); if
   * the game already recognises the raw input, leave it untouched rather than
   * expanding it (e.g. "c" -> "close", "k" -> "attack").  The probe matches
   * against the literal letter the player typed.
   */
  if (expansion)
    {
      char literal[2];

      literal[0] = command[0];
      literal[1] = '\0';
      if (scr_does_command_match (gsc_game, literal))
        return;
    }

  /*
   * If a match found, check for a fit, then replace the character with the
   * expansion string.
   */
  if (expansion)
    {
      if (strlen (buffer) + strlen (expansion) - 1 >= (unsigned int) size)
        return;

      memmove (command + strlen (expansion) - 1, command, strlen (command) + 1);
      memcpy (command, expansion, strlen (expansion));

      gsc_standout_string ("[");
      gsc_standout_char (abbreviation);
      gsc_standout_string (" -> ");
      gsc_standout_string (expansion);
      gsc_standout_string ("]\n");
    }
}


/*
 * gsc_readlog_line()
 *
 * Take the next line of an input log being read back ("glk readlog on") into
 * buffer, and return its length, newline included.  At the end of the log,
 * close the stream and return 0, so that the caller falls through to a normal
 * line request.  Only for use while gsc_readlog_stream is open.
 */
glui32
gsc_readlog_line (char *buffer, glui32 length)
{
  glui32 chars;
  assert (gsc_readlog_stream);

  memset (buffer, 0, length);
  chars = glk_get_line_stream (gsc_readlog_stream, buffer, length);
  if (chars > 0)
    return chars;

  glk_stream_close (gsc_readlog_stream, nullptr);
  gsc_readlog_stream = nullptr;
  return 0;
}


/*
 * os_read_line()
 *
 * Read and return a line of player input.
 */
static scr_bool gsc_read_command (scr_char *buffer, scr_int length);

scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  scr_bool status;

#ifdef SPATTERLIGHT
  /* A turn run again to reach a pause never gets as far as a prompt, unless
     the pause did not come round this time; either way that is over. */
  gsc_sc_silent = FALSE;
#endif
  status = gsc_read_command (buffer, length);
#ifdef SPATTERLIGHT
  if (status)
    gsc_sc_note_command (buffer);
#endif
  return status;
}

static scr_bool
gsc_read_command (scr_char *buffer, scr_int length)
{
  scr_int characters;
  assert (buffer && length > 0);

  /* If a help request is pending, provide a user hint. */
  gsc_output_provide_help_hint ();

  /*
   * Ensure normal style, update the status line, and issue an input prompt.
   */
  gsc_reset_glk_style ();
  gsc_status_notify ();

  /* Anything a meta-command that unwound the interpreter left to be said --
     "Ok." for a restore, the note that a turn was undone -- belongs above the
     prompt of the game it landed in. */
  gsc_meta_report ();

  /* The map follows the game: the ADRIFT 4 layout is centred on the room you
     are in and grows as you explore, so it is redrawn at every prompt -- and a
     restart, which reaches the front end no other way, is caught here first. */
  gsc_map_notice_restart ();
  gsc_map_redraw ();

#ifdef SPATTERLIGHT
  if (gsc_autorestored)
    {
      /* The restored transcript already ends with the old prompt; skip
         printing another and just take input. */
      gsc_autorestored = FALSE;
      gsc_sc_note_turn_start ();
      /* ...or, closed at a <waitkey> pause in the turn this prompt began,
         with the command too: it is run again, unseen, as far as the pause
         (gsc_sc_autosave_waitkey). */
      if (gsc_sc_take_replay_command (buffer, length))
        {
          gsc_sc_silent = TRUE;
          return TRUE;
        }
    }
  else
    {
      gsc_put_prompt (">");
      /* Autosave at every top-level prompt: after the prompt is printed (so
         the GUI snapshot ends with it) but before input is requested (so
         the archived windows carry no pending request and a restore
         re-enters cleanly right here). */
      gsc_autosave ();
    }
#else
  gsc_put_prompt (">");
#endif

  /* A walk set going by a click on the map supplies the next direction itself,
     in place of reading one from the player, and so does a `go <place>`
     walk.  Echo it so the transcript reads as though it had been typed.
     Not at the name and gender questions, whose line is an answer. */
  if ((run_startup_prompt () == 0 && gsc_sc_walk_next (buffer, length))
      || scr_take_scripted_line (buffer, length))
    {
      gsc_echo_input (gsc_put_literal, buffer, TRUE);
      return TRUE;
    }

  /*
   * If we have an input log to read from, use that until it is exhausted.
   * On end of file, the stream is closed and input resumes from line requests.
   */
  if (gsc_readlog_stream)
    {
      if (gsc_readlog_line (buffer, (glui32) length) > 0)
        {
          /* Echo the line just read (newline included) in input style, and
             return it as player input. */
          gsc_echo_input (gsc_put_string, buffer, FALSE);
          return TRUE;
        }
    }

  /*
   * No input log being read, or we just hit the end of file on one.  Revert
   * to normal line input; start by getting a new line from Glk.
   */
  characters = gsc_read_line (buffer, length - 1);
  assert (characters <= length);
  buffer[characters] = '\0';

  /* A click on the map cancelled the request to start a walk.  Its first
     step takes the place of the line; where the library echoed the cancelled
     line, the prompt's line has ended, and the step gets a prompt of its
     own. */
  if (gsc_sc_walk_clicked)
    {
      gsc_sc_walk_clicked = FALSE;
      if (gsc_sc_walk_next (buffer, length))
        {
          if (!gsc_line_echo_manual)
            gsc_put_prompt (">");
          gsc_echo_input (gsc_put_literal, buffer, TRUE);
          return TRUE;
        }
      gsc_colour_echo (FALSE);
    }

  /*
   * If neither abbreviations nor local commands are enabled, use the data
   * read above without further massaging.
   */
  if (gsc_abbreviations_enabled || gsc_commands_enabled)
    {
      char *command;

      /*
       * If the first non-space input character is a quote, bypass all
       * abbreviation expansion and local command recognition, and use the
       * unadulterated input, less introductory quote.
       */
      command = buffer + strspn (buffer, "\t ");
      if (command[0] == GSC_QUOTED_INPUT)
        {
          /* Delete the quote with memmove(). */
          memmove (command, command + 1, strlen (command));
        }
      else
        {
          /* Check for, and expand, and abbreviated commands. */
          if (gsc_abbreviations_enabled)
            gsc_expand_abbreviations (buffer, length);

          /*
           * Check for standalone "help", then for Glk port special commands;
           * suppress the interpreter's use of this input for Glk commands by
           * returning FALSE.
           */
          if (gsc_commands_enabled)
            {
              int posn;

              posn = strspn (buffer, "\t ");
              gsc_note_help_request (buffer + posn);

              if (gsc_command_escape (buffer))
                {
                  gsc_output_silence_help_hints ();

                  /* UNDO / RESTORE / RESTART / QUIT happen here rather than in
                     the handler: the dispatcher above has since freed the line,
                     and all four may unwind out of the interpreter without
                     coming back.  A successful one never returns; the rest fall
                     through to another prompt.

                     Empty the line first.  It is the interpreter's own input
                     buffer, kept across calls so that "get lamp. go north"
                     runs as two commands, and an unwind skips the point where
                     the next read would have cleared it -- leaving the restored
                     or restarted game to parse "glk restart" as its first
                     command. */
                  memset (buffer, 0, length);
                  gsc_meta_perform ();
                  return FALSE;
                }

              /* A bare MAP shows the map pane, as it did in the ADRIFT 4
                 runner -- unless the game has a MAP command of its own, in
                 which case the game's wins and the pane is reached with
                 "glk map". */
              if (!gsc_map_taken
                  && scr_strcasecmp (buffer + posn, "map") == 0)
                {
                  gsc_map_toggle ();
                  gsc_output_silence_help_hints ();
                  return FALSE;
                }
            }
        }
    }

  /*
   * If there is an input log active, log this input string to it.  Note that
   * by logging here we get any abbreviation expansions but we won't log glk
   * special commands, nor any input read from a current open input log.
   */
  if (gsc_inputlog_stream)
    {
      glk_put_string_stream (gsc_inputlog_stream, buffer);
      glk_put_char_stream (gsc_inputlog_stream, '\n');
    }

  return TRUE;
}


/*
 * os_read_line_debug()
 *
 * Read and return a debugger command line.  There's no dedicated debugging
 * window, so this is just a call to the normal readline, with an additional
 * prompt.
 */
scr_bool
os_read_line_debug (scr_char *buffer, scr_int length)
{
  scr_bool status;

  gsc_output_silence_help_hints ();
  gsc_reset_glk_style ();
  gsc_put_literal ("[Scarier debug]");
#ifdef SPATTERLIGHT
  /* A debugger prompt is mid-turn: not a state worth autosaving. */
  gsc_in_debug_read = TRUE;
#endif
  status = os_read_line (buffer, length);
#ifdef SPATTERLIGHT
  gsc_in_debug_read = FALSE;
#endif
  return status;
}


/*
 * gsc_get_choice_key()
 *
 * Wait for a keypress matching one of the uppercase characters in `choices`,
 * ignoring Glk special keys, and return it uppercased.
 */
scr_char
gsc_get_choice_key (const char *choices)
{
  scr_char response;

  do
    {
      event_t event;

      /* Wait for a standard key, ignoring Glk special keys. */
#ifdef SPATTERLIGHT
      gsc_sc_silent = FALSE;
      gsc_sc_note_mid_turn_input ();
#endif
      do
        {
          glk_request_char_event (gsc_main_window);
          gsc_event_wait (evtype_CharInput, &event);
        }
      while (event.val1 > UCHAR_MAX);
      response = glk_char_to_upper (event.val1);
    }
  while (response == '\0' || !strchr (choices, response));

  return response;
}


/*
 * os_confirm()
 *
 * Confirm a game action with a yes/no prompt.
 */
scr_bool
os_confirm (scr_int type)
{
  scr_char response;

  /*
   * Always allow game saves and hint display, and if we're reading from an
   * input log, allow everything no matter what, on the assumption that the
   * user knows what they are doing.
   */
  if (gsc_readlog_stream
      || type == SCR_CONF_SAVE || type == SCR_CONF_VIEW_HINTS)
    return TRUE;

  /* Ensure back to normal style, and update status. */
  gsc_reset_glk_style ();
  gsc_status_notify ();

  /* Prompt for the confirmation, based on the type. */
  if (type == GSC_CONF_SUBTLE_HINT)
    gsc_put_literal ("View the subtle hint for this topic");
  else if (type == GSC_CONF_UNSUBTLE_HINT)
    gsc_put_literal ("View the unsubtle hint for this topic");
  else if (type == GSC_CONF_CONTINUE_HINTS)
    gsc_put_literal ("Continue with hints");
  else
    {
      gsc_put_literal ("Do you really want to ");
      switch (type)
        {
        case SCR_CONF_QUIT:
          gsc_put_literal ("quit");
          break;
        case SCR_CONF_RESTART:
          gsc_put_literal ("restart");
          break;
        case SCR_CONF_SAVE:
          gsc_put_literal ("save");
          break;
        case SCR_CONF_RESTORE:
          gsc_put_literal ("restore");
          break;
        case SCR_CONF_VIEW_HINTS:
          gsc_put_literal ("view hints");
          break;
        default:
          gsc_put_literal ("do that");
          break;
        }
    }
  gsc_put_literal ("? ");

  /* Wait until 'yes' or 'no' entered. */
  response = gsc_get_choice_key ("YN");

  /* Echo the confirmation response, and a new line. */
  glk_set_style (style_Input);
  glk_put_string ((char *)(response == 'Y' ? "Yes" : "No"));
  glk_set_style (style_Normal);
  glk_put_char ('\n');

  /* Use a short delay on restarts, if confirmed. */
  if (type == SCR_CONF_RESTART && response == 'Y')
    gsc_short_delay ();

  /* Return TRUE if 'Y' was entered. */
  return (response == 'Y');
}

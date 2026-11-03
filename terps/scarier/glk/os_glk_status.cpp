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
 * os_glk_status.cpp: the status line, windowed and non-windowed, for both
 * engines.  Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*---------------------------------------------------------------------*/
/*  Glk port status line functions                                     */
/*---------------------------------------------------------------------*/

/* Size of saved status buffer used for non-windowing Glk status lines. */
enum { GSC_STATUS_BUFFER_LENGTH = 74 };

/* Scratch space gsc_status_line_text() formats or trims a status line into. */
enum { GSC_STATUS_TEXT_LENGTH = 256 };

/*
 * gsc_is_string_usable()
 *
 * Return TRUE if string is non-null, not zero-length or contains characters
 * other than whitespace.
 */
static scr_bool
gsc_is_string_usable (const scr_char *string)
{
  /* If non-null, scan for any non-space character. */
  if (string)
    {
      scr_int index_;

      for (index_ = 0; string[index_] != '\0'; index_++)
        {
          if (!strchr (WHITESPACE, string[index_]))
            return TRUE;
        }
    }

  /* NULL, or no characters other than whitespace. */
  return FALSE;
}


/*
 * gsc_status_begin()
 * gsc_status_end()
 *
 * Open and close a status line redraw, shared by the ADRIFT <=4 and ADRIFT 5
 * status lines.  gsc_status_begin() makes the status window current, blanks
 * it, and fills its width with spaces in the bar's style so that the bar
 * spans the whole line; it returns FALSE, having done nothing, if there is
 * no status window or it has no height, in which case the caller has nothing
 * to draw.  gsc_status_end() hands the current window back to the main one.
 */
scr_bool
gsc_status_begin (glui32 *width)
{
  glui32 height, index;

  if (!gsc_status_window)
    return FALSE;

  glk_window_get_size (gsc_status_window, width, &height);
  if (height == 0)
    return FALSE;

  glk_window_clear (gsc_status_window);
  glk_window_move_cursor (gsc_status_window, 0, 0);
  glk_set_window (gsc_status_window);

#ifdef GSC_HAVE_ZCOLORS
  /* The colours have to be named again after every clear, not once when colour
     mode is turned on: in Gargoyle a grid clear re-seeds the window's
     attributes from the library's global override colours, which would wipe a
     colour set earlier on this stream. */
  if (gsc_colour_enabled)
    garglk_set_zcolors_stream (glk_window_get_stream (gsc_status_window),
                               gsc_colour_background, gsc_colour_output);
#endif

  /* Out of colour mode the bar is a reverse-video User1 one, the way every
     other Glk port draws a status line.  In colour mode the bar is still
     reversed -- the Runner draws it as the inverse of the story text, the
     game's text colour behind and its background colour for the letters -- but
     the inversion is in the colours named just above rather than in the style.
     Doing it that way is what makes every library agree: stylehint_ReverseColor
     is honoured by some and dropped by others once a zcolor is in force, so a
     User1 bar here would come out inverted in one interpreter and plain in the
     next.  An interpreter that is ignoring the colours we name takes the
     reverse-video bar as well: the pair named just above would be dropped,
     leaving the bar in the theme's plain text colours and so indistinguishable
     from the story window. */
  glk_set_style (gsc_colour_visible () ? style_Normal : style_User1);
  for (index = 0; index < *width; index++)
    glk_put_char (' ');
  glk_window_move_cursor (gsc_status_window, 0, 0);

  return TRUE;
}

void
gsc_status_end (void)
{
#ifdef GSC_HAVE_ZCOLORS
  /* Name the story window's own colours again.  A library whose zcolors are
     global as well as per-stream (Gargoyle) paints window backgrounds from the
     last colours it was told about, so leaving the bar's inverted pair in force
     would tint the story window too.  Safe with respect to the input echo:
     gsc_colour_echo() runs from gsc_read_line_locale(), after the status
     redraw, so it always has the last word on the prompt's colour. */
  if (gsc_colour_enabled && gsc_main_window)
    garglk_set_zcolors_stream (glk_window_get_stream (gsc_main_window),
                               gsc_colour_main_fg, gsc_colour_background);
#endif

  glk_set_window (gsc_main_window);
}


/*
 * gsc_status_line_text()
 *
 * The game's status line or, when the game does not set a usable one, the
 * score, either way with trailing whitespace removed, and using the caller's
 * buffer where it needs one.
 *
 * Authors pad StatusBoxText by hand to place it -- Three Monkeys, One Cage
 * stores "     The game is %winnable%        " -- because the Runner draws the
 * status box left-aligned.  Right-justifying that string as it stands ends the
 * line with the author's eight spaces, leaving the text itself floating short
 * of the right margin and looking mis-aligned.  The trailing run carries no
 * information here, so drop it and justify the text.  Leading spaces are kept:
 * they cost nothing at the left of a right-justified string, and are eaten
 * first by head-truncation in a narrow window.
 */
static const scr_char *
gsc_status_line_text (char *buffer, size_t length)
{
  const scr_char *status;
  size_t trimmed;

  status = scr_get_game_status_line (gsc_game);
  if (!gsc_is_string_usable (status))
    {
      snprintf (buffer, length, "Score: %ld", scr_get_game_score (gsc_game));
      return buffer;
    }

  for (trimmed = strlen (status);
       trimmed > 0 && strchr (WHITESPACE, status[trimmed - 1]);
       trimmed--)
    ;

  /* Nothing to trim, or a status too long to copy: use it where it lies. */
  if (status[trimmed] == '\0' || trimmed >= length)
    return status;

  memcpy (buffer, status, trimmed);
  buffer[trimmed] = '\0';
  return buffer;
}


static const scr_char *
gsc_status_next_byte (const scr_char *string)
{
  return string + 1;
}

static const gsc_status_writer_t GSC_STATUS_WRITER = {
  gsc_status_printed_width, gsc_put_string, gsc_status_next_byte
};

/* Head-truncation marker, U+2026, and the least text worth printing after
   it.  The marker is one column wide. */
enum { GSC_STATUS_ELLIPSIS = 0x2026, GSC_STATUS_MIN_TAIL = 1 };


/*
 * gsc_status_put_right()
 *
 * Print the status text right-justified on the status line, ending one column
 * short of the right edge to match the one-column indent the room name gets at
 * the left.  The room name is already on the line, and room_end is the first
 * free column after it, so the status may use only the columns from there on,
 * and must leave at least one blank between the two.
 *
 * A status too long for that gap is truncated at its head: the tail carries
 * the parts that change -- score, moves, time, whatever the game keeps there --
 * so it is the head that gets replaced with an ellipsis.  The ellipsis goes
 * straight after the room name and the tail straight after the ellipsis, with
 * no blanks around it.  If not even the ellipsis and a character of text will
 * fit, the status is dropped rather than allowed to run into the room name.
 */
void
gsc_status_put_right (glui32 width, glui32 room_end,
                      const scr_char *status,
                      const gsc_status_writer_t *writer)
{
  glui32 avail, status_width;

  /* Columns between the room name (plus one blank) and the right margin. */
  if (width < room_end + 2)
    return;
  avail = width - room_end - 2;

  status_width = writer->width (status);
  if (status_width <= avail)
    {
      glk_window_move_cursor (gsc_status_window,
                              width - status_width - 1, 0);
      writer->print (status);
      return;
    }

  /* Too wide: the ellipsis takes the column the blank would have had, so the
     longest tail that fits in the rest goes behind it. */
  if (avail < GSC_STATUS_MIN_TAIL)
    return;

  while (*status != '\0')
    {
      status = writer->next (status);

      /* A cut landing in a gap would leave a blank after the ellipsis. */
      if (*status == ' ')
        continue;

      status_width = writer->width (status);
      if (status_width <= avail)
        {
          if (status_width < GSC_STATUS_MIN_TAIL)
            return;
          glk_window_move_cursor (gsc_status_window, room_end, 0);
          glk_put_char_uni (GSC_STATUS_ELLIPSIS);
          writer->print (status);
          return;
        }
    }
}


/*
 * gsc_status_update()
 *
 * Update the status line from the current game state.  This is for windowing
 * Glk libraries.
 */
static void
gsc_status_update (void)
{
  glui32 width;
  assert (gsc_status_window);

  if (gsc_status_begin (&width))
    {
      const scr_char *room;

      /* See if the game is indicating any current player room. */
      room = scr_get_game_room (gsc_game);
      if (!gsc_is_string_usable (room))
        {
          /*
           * Player location is indeterminate, so print out a generic status,
           * showing the game name and author.
           */
          glk_window_move_cursor (gsc_status_window, 1, 0);
          gsc_put_string (scr_get_game_name (gsc_game));
          gsc_put_literal (" | ");
          gsc_put_string (scr_get_game_author (gsc_game));
        }
      else
        {
          const scr_char *status;
          char text[GSC_STATUS_TEXT_LENGTH] = {0};

          /* Print the player location. */
          glk_window_move_cursor (gsc_status_window, 1, 0);
          gsc_put_string (room);

          /* Get the game's status line, or if none, format score. */
          status = gsc_status_line_text (text, sizeof (text));

          /* Print the status line or score at window right. */
          gsc_status_put_right (width, 1 + gsc_status_printed_width (room),
                                status, &GSC_STATUS_WRITER);
        }

      gsc_status_end ();
    }
}


/*
 * gsc_status_safe_strcat()
 *
 * Helper for gsc_status_print(), concatenates strings only up to the
 * available length.
 */
static void
gsc_status_safe_strcat (char *dest, size_t length, const char *src)
{
  size_t available, src_length;

  /* Append only as many characters as will fit. */
  src_length = strlen (src);
  available = length - strlen (dest) - 1;
  if (available > 0)
    strncat (dest, src, src_length < available ? src_length : available);
}


/*
 * gsc_status_print()
 *
 * Print the current contents of the completed status line buffer out in the
 * main window, if it has changed since the last call.  This is for non-
 * windowing Glk libraries.
 */
static void
gsc_status_print (void)
{
  static char current_status[GSC_STATUS_BUFFER_LENGTH + 1];

  const scr_char *room;

  /* Do nothing if the game isn't indicating any current player room. */
  room = scr_get_game_room (gsc_game);
  if (gsc_is_string_usable (room))
    {
      char buffer[GSC_STATUS_BUFFER_LENGTH + 1] = {0};
      const scr_char *status;
      char text[GSC_STATUS_TEXT_LENGTH] = {0};

      /* Make an attempt at a status line, starting with player location. */
      gsc_status_safe_strcat (buffer, sizeof (buffer), room);

      /* Get the game's status line, or if none, format score. */
      status = gsc_status_line_text (text, sizeof (text));

      /* Append the status line or score. */
      gsc_status_safe_strcat (buffer, sizeof (buffer), " | ");
      gsc_status_safe_strcat (buffer, sizeof (buffer), status);

      /* If this matches the current saved status line, do nothing more. */
      if (strcmp (buffer, current_status) != 0)
        {
          /* Bracket, and output the status line buffer. */
          gsc_put_literal ("[ ");
          gsc_put_string (buffer);
          gsc_put_literal (" ]\n");

          /* Save the details of the printed status buffer. */
          snprintf(current_status, sizeof(current_status), "%s", buffer);
        }
    }
}


/*
 * gsc_status_notify()
 *
 * Front end function for updating status.  Either updates the status window
 * or prints the status line to the main window.
 */
void
gsc_status_notify (void)
{
  if (gsc_is_a5)
    {
      /* ADRIFT 5 games have no scare game state, so both v4 renderers would
         ask scr_* about a NULL game and paint "[invalid game]".  os_confirm()
         reaches here for the shared hint display and the quit/restart
         prompts, which both engines use. */
      if (gsc_a5_run)
        gsc_a5_status (gsc_a5_run);
      return;
    }

  if (gsc_status_window)
    gsc_status_update ();
  else
    gsc_status_print ();
}


/*
 * gsc_status_redraw()
 *
 * Redraw the contents of any status window with the constructed status string.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
void
gsc_status_redraw (void)
{
  if (gsc_status_window)
    {
      winid_t parent;

      /*
       * Rearrange the status window, without changing its actual arrangement
       * in any way.  This is a hack to work round incorrect window repainting
       * in Xglk; it forces a complete repaint of affected windows on Glk
       * window resize and arrange events, and works in part because Xglk
       * doesn't check for actual arrangement changes in any way before
       * invalidating its windows.  The hack should be harmless to Glk
       * libraries other than Xglk, moreover, we're careful to activate it
       * only on resize and arrange events.
       */
      parent = glk_window_get_parent (gsc_status_window);
      glk_window_set_arrangement (parent, winmethod_Above | winmethod_Fixed,
                                  1, nullptr);
      if (gsc_is_a5)
        {
          /* ADRIFT 5 games have no scare game state; use the a5 renderer. */
          if (gsc_a5_run)
            gsc_a5_status (gsc_a5_run);
        }
      else
        gsc_status_update ();
    }

  /* The map is rasterised to the pixel size of its window, so a resize has to
     redraw it, not just repaint it -- and the window has lost what we drew
     there, so all of it must go out again, not just what the game changed. */
  gsc_map_full_flush = TRUE;
  gsc_map_redraw ();
}

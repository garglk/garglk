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
 * it, and fills every row with spaces in the bar's style so that the bar
 * spans the whole width; it returns FALSE, having done nothing, if there is
 * no status window or it has no height, in which case the caller has nothing
 * to draw.  gsc_status_end() hands the current window back to the main one.
 */
scr_bool
gsc_status_begin (glui32 *width)
{
  glui32 height, index, row;

  if (!gsc_io.status_window)
    return FALSE;

  glk_window_get_size (gsc_io.status_window, width, &height);
  if (height == 0)
    return FALSE;

  glk_window_clear (gsc_io.status_window);
  glk_window_move_cursor (gsc_io.status_window, 0, 0);
  glk_set_window (gsc_io.status_window);

#ifdef GSC_HAVE_ZCOLORS
  /* The colours have to be named again after every clear, not once when colour
     mode is turned on: in Gargoyle a grid clear re-seeds the window's
     attributes from the library's global override colours, which would wipe a
     colour set earlier on this stream. */
  if (gsc_colour.enabled)
    garglk_set_zcolors_stream (glk_window_get_stream (gsc_io.status_window),
                               gsc_colour.background, gsc_colour.output);
#endif

  /* Out of colour mode the bar is a reverse-video User1 one, the way every
     other Glk port draws a status line.  In colour mode the bar is still
     reversed -- the Runner draws it as the inverse of the story text, the
     game's text colour behind and its background colour for the letters -- but
     the inversion is in the colours named just above rather than in the style.
     Doing it that way is what makes every library agree:
     stylehint_ReverseColor is honoured by some and dropped by others once a
     zcolor is in force, so a User1 bar here would come out inverted in one
     interpreter and plain in the next.  An interpreter that is ignoring the
     colours we name takes the reverse-video bar as well: the pair named just
     above would be dropped, leaving the bar in the theme's plain text colours
     and so indistinguishable from the story window. */
  glk_set_style (gsc_colour_visible () ? style_Normal : style_User1);
  for (row = 0; row < height; row++)
    {
      glk_window_move_cursor (gsc_io.status_window, 0, row);
      for (index = 0; index < *width; index++)
        glk_put_char (' ');
    }
  glk_window_move_cursor (gsc_io.status_window, 0, 0);

  return TRUE;
}

void
gsc_status_end (void)
{
#ifdef GSC_HAVE_ZCOLORS
  /* Name the story window's own colours again.  A library whose zcolors are
     global as well as per-stream (Gargoyle) paints window backgrounds from the
     last colours it was told about, so leaving the bar's inverted pair in
     force would tint the story window too.  Safe with respect to the input
     echo: gsc_colour_echo() runs from gsc_read_line_locale(), after the status
     redraw, so it always has the last word on the prompt's colour. */
  if (gsc_colour.enabled && gsc_io.main_window)
    garglk_set_zcolors_stream (glk_window_get_stream (gsc_io.main_window),
                               gsc_colour.main_fg, gsc_colour.background);
#endif

  glk_set_window (gsc_io.main_window);
}


/*
 * gsc_status_line_text()
 *
 * The game's status line or, when the game does not set a usable one, the
 * score, either way with leading and trailing whitespace removed, and using
 * the caller's buffer where it needs one.
 *
 * Authors pad StatusBoxText by hand to place it -- Three Monkeys, One Cage
 * stores "     The game is %winnable%        " -- because the Runner draws the
 * status box left-aligned.  Right-justifying that string as it stands ends the
 * line with the author's eight spaces, leaving the text itself floating short
 * of the right margin and looking mis-aligned.  The leading run does as much
 * harm on the bar's second row, where the status is left-aligned under the
 * room name, and beside the room name it would count towards the width that
 * decides whether the status moves down there at all (and earn an ellipsis
 * when only blanks were cut).  Neither run carries any information here, so
 * drop both.
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

  /* A usable status has something other than whitespace in it. */
  status += strspn (status, WHITESPACE);
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
 * gsc_status_tail()
 *
 * The longest tail of status that fits in avail columns, without the blank a
 * cut landing in a gap would start it with, or NULL if not even
 * GSC_STATUS_MIN_TAIL columns of it fit.  The tail carries the parts that
 * change -- score, moves, time, whatever the game keeps there -- so it is the
 * head that a status too long for its room loses.
 */
static const scr_char *
gsc_status_tail (const scr_char *status, glui32 avail,
                 const gsc_status_writer_t *writer)
{
  glui32 status_width;

  if (avail < GSC_STATUS_MIN_TAIL)
    return nullptr;

  while (*status != '\0')
    {
      status = writer->next (status);
      if (*status == ' ')
        continue;

      status_width = writer->width (status);
      if (status_width <= avail)
        return status_width < GSC_STATUS_MIN_TAIL ? nullptr : status;
    }
  return nullptr;
}


/*
 * gsc_status_put_right()
 *
 * Print the status text right-justified on the status line, ending one column
 * short of the right edge to match the one-column indent the room name gets at
 * the left.  The room name is already on the line, and room_end is the first
 * free column after it, so the status may use only the columns from there on,
 * and must leave at least one blank between the two.
 *
 * A status too long for that gap is truncated at its head (gsc_status_tail).
 * The ellipsis goes straight after the room name and the tail straight after
 * the ellipsis, with no blanks around it.  If not even the ellipsis and a
 * character of text will fit, the status is dropped rather than allowed to
 * run into the room name.
 */
static void
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
      glk_window_move_cursor (gsc_io.status_window,
                              width - status_width - 1, 0);
      writer->print (status);
      return;
    }

  /* Too wide: the ellipsis takes the column the blank would have had, so the
     longest tail that fits in the rest goes behind it. */
  status = gsc_status_tail (status, avail, writer);
  if (status)
    {
      glk_window_move_cursor (gsc_io.status_window, room_end, 0);
      glk_put_char_uni (GSC_STATUS_ELLIPSIS);
      writer->print (status);
    }
}


/*
 * gsc_status_put_below()
 *
 * Print the status text left-aligned on the bar's second row, at the same
 * one-column indent as the room name above it.  A status too long even for a
 * row of its own loses its head to an ellipsis, as on the first row.
 */
static void
gsc_status_put_below (glui32 width, const scr_char *status,
                      const gsc_status_writer_t *writer)
{
  glui32 avail;

  if (width < 3)
    return;
  avail = width - 2;

  glk_window_move_cursor (gsc_io.status_window, 1, 1);
  if (writer->width (status) <= avail)
    {
      writer->print (status);
      return;
    }

  status = gsc_status_tail (status, avail - 1, writer);
  if (status)
    {
      glk_put_char_uni (GSC_STATUS_ELLIPSIS);
      writer->print (status);
    }
}


/*
 * gsc_status_wants_second_row()
 *
 * Whether the status belongs on a second row of the bar, under the room name,
 * rather than right-justified beside it.  The rule is Question's for moving
 * its status from the banner to the side pane: the status leaves the first
 * row once less than three quarters of it would survive gsc_status_put_right's
 * cut, and -- below, the bar already having its second row -- comes back up
 * only when all of it fits, so that a window dragged across the threshold does
 * not flicker between the two.  The bar's own height is the memory of where
 * the status was, so the answer survives an autorestore without a flag of its
 * own.
 */
static scr_bool
gsc_status_wants_second_row (glui32 width, glui32 room_end,
                             const scr_char *status, scr_bool below,
                             const gsc_status_writer_t *writer)
{
  glui32 avail, status_width, shown_width;
  const scr_char *shown;

  if (!gsc_is_string_usable (status))
    return FALSE;

  /* The same arithmetic as gsc_status_put_right(). */
  avail = width >= room_end + 2 ? width - room_end - 2 : 0;
  status_width = writer->width (status);
  if (status_width <= avail)
    return FALSE;
  if (below)
    return TRUE;

  shown = gsc_status_tail (status, avail, writer);
  shown_width = shown ? writer->width (shown) : 0;
  return shown_width * 4 < status_width * 3;
}


/*
 * gsc_status_set_rows()
 *
 * Give the status window the given number of rows, if it has not got them.
 */
static void
gsc_status_set_rows (glui32 rows)
{
  glui32 height;

  glk_window_get_size (gsc_io.status_window, nullptr, &height);
  if (height != rows)
    glk_window_set_arrangement (glk_window_get_parent (gsc_io.status_window),
                                winmethod_Above | winmethod_Fixed, rows,
                                nullptr);
}


/*
 * gsc_status_draw()
 *
 * Redraw the status bar, shared by the ADRIFT <=4 and ADRIFT 5 status lines:
 * the room name at the left of the first row, and the status either
 * right-justified beside it or, when it does not fit there (see
 * gsc_status_wants_second_row), left-aligned on a second row the bar grows
 * for it.  room may be NULL, and status NULL or empty, for a bar with
 * nothing of that kind to show; either way the bar drops back to one row.
 */
void
gsc_status_draw (const scr_char *room, const scr_char *status,
                 const gsc_status_writer_t *writer)
{
  glui32 width, height, room_end;
  scr_bool below;

  if (!gsc_io.status_window)
    return;
  glk_window_get_size (gsc_io.status_window, &width, &height);
  if (height == 0)
    return;

  room_end = 1 + (room ? writer->width (room) : 0);
  below = status
          && gsc_status_wants_second_row (width, room_end, status,
                                          height > 1, writer);
  gsc_status_set_rows (below ? 2 : 1);

  if (!gsc_status_begin (&width))
    return;

  glk_window_move_cursor (gsc_io.status_window, 1, 0);
  if (room)
    writer->print (room);

  if (status && *status != '\0')
    {
      if (below)
        gsc_status_put_below (width, status, writer);
      else
        gsc_status_put_right (width, room_end, status, writer);
    }

  gsc_status_end ();
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
  const scr_char *room;
  assert (gsc_io.status_window);

  /* See if the game is indicating any current player room. */
  room = scr_get_game_room (gsc_game);
  if (!gsc_is_string_usable (room))
    {
      /*
       * Player location is indeterminate, so print out a generic status,
       * showing the game name and author.
       */
      glui32 width;

      gsc_status_set_rows (1);
      if (gsc_status_begin (&width))
        {
          glk_window_move_cursor (gsc_io.status_window, 1, 0);
          gsc_put_string (scr_get_game_name (gsc_game));
          gsc_put_literal (" | ");
          gsc_put_string (scr_get_game_author (gsc_game));
          gsc_status_end ();
        }
    }
  else
    {
      char text[GSC_STATUS_TEXT_LENGTH] = {0};

      /* The game's status line, or if none, the score. */
      gsc_status_draw (room, gsc_status_line_text (text, sizeof (text)),
                       &GSC_STATUS_WRITER);
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
      /* ADRIFT 5 games have no ADRIFT 4 game state, so both v4 renderers would
         ask scr_* about a NULL game and paint "[invalid game]".  os_confirm()
         reaches here for the shared hint display and the quit/restart
         prompts, which both engines use. */
      if (gsc_a5_run)
        gsc_a5_status (gsc_a5_run);
      return;
    }

  if (gsc_io.status_window)
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
  if (gsc_io.status_window)
    {
      winid_t parent;
      glui32 height;

      /*
       * Rearrange the status window, without changing its actual arrangement
       * in any way.  This is a hack to work round incorrect window repainting
       * in Xglk; it forces a complete repaint of affected windows on Glk
       * window resize and arrange events, and works in part because Xglk
       * doesn't check for actual arrangement changes in any way before
       * invalidating its windows.  The hack should be harmless to Glk
       * libraries other than Xglk, moreover, we're careful to activate it
       * only on resize and arrange events.  The bar keeps the rows it has;
       * the redraw below gives or takes away a second one if the new width
       * calls for it.
       */
      parent = glk_window_get_parent (gsc_io.status_window);
      glk_window_get_size (gsc_io.status_window, nullptr, &height);
      glk_window_set_arrangement (parent, winmethod_Above | winmethod_Fixed,
                                  height, nullptr);
      if (gsc_is_a5)
        {
          /* ADRIFT 5 games have no ADRIFT 4 game state; use the a5 renderer. */
          if (gsc_a5_run)
            gsc_a5_status (gsc_a5_run);
        }
      else
        gsc_status_update ();
    }

  /* The map is rasterised to the pixel size of its window, so a resize has to
     redraw it, not just repaint it -- and the window has lost what we drew
     there, so all of it must go out again, not just what the game changed. */
  gsc_map_pane.full_flush = TRUE;
  gsc_map_redraw ();
}

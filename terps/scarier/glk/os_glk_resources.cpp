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
 * os_glk_resources.cpp: ADRIFT <=4 sound and graphics resources, and the
 * title window.  Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*---------------------------------------------------------------------*/
/*  Glk resource handling functions                                    */
/*---------------------------------------------------------------------*/

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
/*
 * Sounds and graphics go through garglk_add_resource_from_file(): Gargoyle's
 * own, or glkimp's in Spatterlight.  Either way it takes a file in the game's
 * directory, named without a path, and returns a resource number for the
 * standard Glk drawing and playing routines.
 */

/*
 * gsc_load_resource()
 *
 * Common part of os_play_sound() and os_show_graphic(): register a resource,
 * embedded or external, and return its number, or zero if there is nothing to
 * load.
 *
 * An embedded resource is a chunk of the game file.  A game with "Embedded"
 * off, like Druggy Lane, instead names each sound or graphic by the full path
 * it had on the author's machine -- "C:\My Documents\Adrift\...\day.wav" --
 * and ships the files beside the .taf; that is the path's last component, in
 * the game's directory.  Spatterlight's garglk_add_resource_from_file() finds
 * it whatever its case -- a Windows author's capitals need not match the
 * files on disk -- and takes a zero length as the whole file.  Gargoyle's
 * makes no such promises, so there external resources stay unsupported.
 */
static glui32
gsc_load_resource (const scr_char *filepath, scr_int offset, scr_int length,
                   int is_sound)
{
  glui32 usage = is_sound ? giblorb_ID_Snd : giblorb_ID_Pict;

  if (length > 0)
    return garglk_add_resource_from_file (usage, gsc_gamefile,
                                          (glui32) offset, (glui32) length);
#ifdef SPATTERLIGHT
  {
    const char *name, *p;

    if (scr_strempty (filepath))
      return 0;
    /* The name is whatever follows the last separator, of either kind. */
    name = filepath;
    for (p = filepath; *p != '\0'; p++)
      {
        if (*p == '\\' || *p == '/' || *p == ':')
          name = p + 1;
      }
    if (*name == '\0')
      return 0;
    return garglk_add_resource_from_file (usage, name, 0, 0);
  }
#else
  (void) filepath;
  return 0;
#endif
}

#ifdef SPATTERLIGHT
/*
 * gsc_load_external_resource()
 *
 * The ADRIFT 5 side's way in: load a media file named by an <img>/<audio>
 * src from beside the game, returning its resource number, or zero if there
 * is no such file.
 */
glui32
gsc_load_external_resource (const char *filepath, int is_sound)
{
  return gsc_load_resource (filepath, 0, 0, is_sound);
}
#endif

schanid_t sound_channel;

void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  glui32 id;

  /* The app has put the sounds back as they were at the pause. */
  if (gsc_sc_silent)
    return;
  if (sound_channel == nullptr)
    sound_channel = glk_schannel_create (0);
  if (sound_channel == nullptr)
    return;

  id = gsc_load_resource (filepath, offset, length, TRUE);
  if (id == 0)
    return;
  glk_schannel_play_ext (sound_channel, id, is_looping ? 0xffffffff : 1, 0);
}

void
os_stop_sound (void)
{
  if (gsc_sc_silent)
    return;
  if (sound_channel != nullptr)
    glk_schannel_stop (sound_channel);
}
#else
/*
 * os_play_sound()
 * os_stop_sound()
 *
 * Stub functions.
 */
void
os_play_sound (const scr_char * /* filepath */, scr_int /* offset */,
               scr_int /* length */, scr_bool /* is_looping */)
{
}

void
os_stop_sound (void)
{
}
#endif


#ifdef GSC_HAVE_TITLE_WINDOW
/*
 * Title/cover graphic support.  Adrift games can carry an "IntroRes" cover
 * image, shown by the engine before the game's first turn.  A cover is shown
 * one of two ways, never half-way between them:
 *
 *   - a cover that fits comfortably in the main window (a wide banner) is
 *     drawn inline at the top of the story, like any other graphic -- the
 *     redraw-after-clear in os_glk_output.cpp keeps it on screen when the
 *     intro opens with a <cls>, as "To Hell in a Hamper" does;
 *
 *   - a bigger one (SS Whore's portrait cover, say) becomes a title screen: a
 *     graphics window over the whole display, the image scaled to fit and
 *     centred, until the player presses a key or clicks.  The intro text,
 *     still buffered, then appears in the full-height main window.
 *
 * An earlier version showed every cover in a pane above the main window while
 * the intro text ran beneath it, which left neither the picture nor the text
 * enough room.
 */
winid_t gsc_graphics_window = nullptr;
glui32 gsc_title_image = 0;
int gsc_seen_input = FALSE;


/*
 * gsc_title_redraw()
 *
 * Draw, or redraw on a resize, the title image scaled to fit within the
 * graphics window, preserving its aspect ratio and centring it.
 */
static void
gsc_title_redraw (void)
{
  glui32 win_width, win_height, img_width, img_height, draw_width, draw_height;

  if (gsc_graphics_window == nullptr || gsc_title_image == 0)
    return;

  glk_window_get_size (gsc_graphics_window, &win_width, &win_height);
  if (win_width == 0 || win_height == 0)
    return;
  if (!glk_image_get_info (gsc_title_image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return;

  /* Fit within the pane, limited by width or height, whichever binds first. */
  if (win_width * img_height <= win_height * img_width)
    {
      draw_width = win_width;
      draw_height = img_height * win_width / img_width;
    }
  else
    {
      draw_height = win_height;
      draw_width = img_width * win_height / img_height;
    }

  glk_window_fill_rect (gsc_graphics_window, 0, 0, 0, win_width, win_height);
  glk_image_draw_scaled (gsc_graphics_window, gsc_title_image,
                         (glsi32) ((win_width - draw_width) / 2),
                         (glsi32) ((win_height - draw_height) / 2),
                         draw_width, draw_height);
}

/*
 * gsc_show_title_graphic()
 *
 * Open a graphics window covering the whole display -- the root is split, so
 * the status line and any map are hidden too -- and draw the title image into
 * it.  Returns TRUE on success, FALSE if graphics windows are unavailable.
 */
int
gsc_show_title_graphic (glui32 image)
{
  glui32 img_width, img_height;

  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return FALSE;
  if (!glk_image_get_info (image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return FALSE;

  if (gsc_graphics_window == nullptr)
    {
      gsc_graphics_window = glk_window_open (glk_window_get_root (),
                                             winmethod_Above
                                             | winmethod_Proportional
                                             | winmethod_NoBorder,
                                             100, wintype_Graphics, 0);
      if (gsc_graphics_window == nullptr)
        return FALSE;
    }

  gsc_title_image = image;
  gsc_title_redraw ();
  return TRUE;
}

/*
 * gsc_close_title_graphic()
 *
 * Close the title window, if open, returning the display to the windows it
 * covered.
 */
void
gsc_close_title_graphic (void)
{
  if (gsc_graphics_window != nullptr)
    {
      glk_window_close (gsc_graphics_window, nullptr);
      gsc_graphics_window = nullptr;
      gsc_title_image = 0;
    }
}

/*
 * gsc_title_fits_inline()
 *
 * Given the title window just opened over the display, decide whether the
 * cover would sit comfortably inline instead: no more than half the display's
 * height once a text buffer has narrowed it to the display's width (as
 * Spatterlight does; images narrower than that are drawn at their own size).
 */
static int
gsc_title_fits_inline (void)
{
  glui32 win_width, win_height, img_width, img_height, inline_height;

  glk_window_get_size (gsc_graphics_window, &win_width, &win_height);
  if (win_width == 0 || win_height == 0)
    return TRUE;
  if (!glk_image_get_info (gsc_title_image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return TRUE;

  inline_height = img_width > win_width
                  ? img_height * win_width / img_width : img_height;
  return inline_height * 2 <= win_height;
}

/*
 * gsc_title_screen_wait()
 *
 * Wait for any key, or a click, on the title screen, then close it.
 */
static void
gsc_title_screen_wait (void)
{
  winid_t key_window;
  int mouse;
  event_t event;

  if (gsc_graphics_window == nullptr)
    return;

  /* Take the key on the title itself where the library allows it; otherwise
     the hidden main window still receives keys, so ask there. */
  key_window = gsc_main_window;
#ifdef gestalt_GraphicsCharInput
  if (glk_gestalt (gestalt_GraphicsCharInput, 0))
    key_window = gsc_graphics_window;
#endif
  glk_request_char_event (key_window);
  mouse = glk_gestalt (gestalt_MouseInput, wintype_Graphics);
  if (mouse)
    glk_request_mouse_event (gsc_graphics_window);

  for (;;)
    {
      glk_select (&event);
      if (gsc_service_window_event (&event))
        continue;
      if (event.type == evtype_CharInput && event.win == key_window)
        break;
      if (event.type == evtype_MouseInput && event.win == gsc_graphics_window)
        break;
    }

  if (event.type == evtype_CharInput)
    {
      if (mouse)
        glk_cancel_mouse_event (gsc_graphics_window);
    }
  else
    glk_cancel_char_event (key_window);

  gsc_close_title_graphic ();
}
#endif /* GSC_HAVE_TITLE_WINDOW */


/*
 * gsc_refresh_windows()
 *
 * Refresh the size-sensitive windows, on Glk Arrange and Redraw events.
 */
static void
gsc_refresh_windows (void)
{
  /* An Arrange is how a library preference flip announces itself, and the
     status redraw below asks gsc_colour_visible, so the cache goes first. */
  gsc_normal_measure_state = -1;

  gsc_status_redraw ();
#ifdef GSC_HAVE_TITLE_WINDOW
  gsc_title_redraw ();
#endif
}


/*
 * gsc_service_window_event()
 *
 * The housekeeping every event loop does before looking at what it is
 * waiting for: redraw on Arrange and Redraw, and let the ADRIFT 5 sound
 * channels know when a sound ends.  Returns TRUE when the event was one of
 * those, so the loop can go back to glk_select() without further ado.
 */
int
gsc_service_window_event (const event_t *event)
{
  switch (event->type)
    {
    case evtype_Arrange:
    case evtype_Redraw:
      gsc_refresh_windows ();
      return TRUE;

    case evtype_SoundNotify:
      gsc_a5_sound_finished (event);
      return TRUE;

    default:
      return FALSE;
    }
}


#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
/*
 * os_show_graphic()
 *
 * Register the requested image, a chunk of the game file or a file beside it
 * (see gsc_load_resource).  Before the player's first input, a cover too
 * big to sit inline is shown as a full-display title screen (see above);
 * anything else is drawn inline in the main window.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  glui32 id;

  if (gsc_main_window == nullptr)
    return;

  id = gsc_load_resource (filepath, offset, length, FALSE);
  if (id == 0)
    return;

  if (!gsc_sc_silent && !gsc_seen_input && gsc_show_title_graphic (id))
    {
      if (!gsc_title_fits_inline ())
        {
          gsc_title_screen_wait ();
          return;
        }
      gsc_close_title_graphic ();
    }

  gsc_draw_inline_graphic (id);
}
#else
/*
 * os_show_graphic()
 *
 * Without Gargoyle's file resources there is no way to show a game graphic,
 * so this is a stub function.
 */
void
os_show_graphic (const scr_char * /* filepath */, scr_int /* offset */,
                 scr_int /* length */)
{
}
#endif

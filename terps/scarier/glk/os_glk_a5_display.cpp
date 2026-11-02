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
 * os_glk_a5_display.cpp: ADRIFT 5 text display -- span styles, the side
 * window, the colour stack, media events, the cover and the status line.
 * Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*
 * gsc_a5_draw_image()
 *
 * Draw Blorb Pict resource `number` centred on its own line in window `win`.
 * Writes straight to `win`'s stream (not the current window), so an image
 * inside a <window> span and its surrounding blank lines both land in the
 * same window regardless of which window is currently selected for output.
 * Returns TRUE if an image was actually drawn.
 */
static int
gsc_a5_draw_image (winid_t win, glui32 number)
{
  glui32 width = 0, height = 0;
  strid_t str;

  if (!gsc_a5_graphics_ok || number == 0 || win == nullptr)
    return FALSE;
  if (!glk_image_get_info (number, &width, &height))
    return FALSE;
  str = glk_window_get_stream (win);
  glk_window_flow_break (win);
  glk_put_char_stream (str, '\n');
  glk_image_draw (win, number, imagealign_InlineCenter, 0);
  glk_put_char_stream (str, '\n');
  return TRUE;
}

/*
 * gsc_a5_span_style()
 *
 * Pick the Glk style for a text span given its alignment and weight/oblique
 * nesting depths.  Glk styles don't combine, so alignment wins over character
 * styles: centered+bold keeps User2 (weight-hinted); right uses Note with no
 * bold/italic combo.  Unaligned bold+italic (or bold+underline) maps to Alert;
 * italic or underline alone to Emphasized; bold alone to Subheader (as the
 * ADRIFT 4 path does in gsc_set_glk_style).  Underline marks are distinct for
 * a future CSS path; for now they share italic's Glk styles.
 *
 * `input_colour` says the span's ink is the input colour (a <c> span, or the
 * game title, which the Runner Displays as one).  That ink only exists in
 * colour mode, so with colours off such a span falls back to Emphasized --
 * the same stand-in gsc_set_glk_style() gives the ADRIFT 4 path -- rather
 * than coming out indistinguishable from body text.  Alignment and bold keep
 * their precedence over the stand-in, as they do there.
 */
static glui32
gsc_a5_span_style (int center_depth, int right_depth,
                   int bold_depth, int italic_depth, int underline_depth,
                   int input_colour)
{
  const int oblique = italic_depth > 0 || underline_depth > 0;

  if (right_depth > 0)
    return style_Note;
  if (center_depth > 0)
    return bold_depth > 0 ? style_User2 : style_User1;
  if (bold_depth > 0 && oblique)
    return style_Alert;
  if (oblique)
    return style_Emphasized;
  if (bold_depth > 0)
    return style_Subheader;
  if (input_colour && !gsc_colour_visible ())
    return style_Emphasized;
  return style_Normal;
}

/*
 * gsc_a5_open_side_window()
 *
 * Return the author-defined secondary output window, opening it lazily the
 * first time the game routes text to one (ADRIFT 5 <window NAME>).  It splits
 * the main story window, taking ~40% of the width on the right, as a text
 * buffer -- Alien Diver's "Status" pane (ship-repair progress and command
 * reference).  Once open it is kept for the rest of the session, like the
 * official Runner's additional windows.  Returns NULL if Glk cannot split
 * (the caller then leaves output in the main window).
 */
winid_t
gsc_a5_open_side_window (void)
{
  if (gsc_a5_side_window == nullptr)
    {
      gsc_a5_side_window = glk_window_open (gsc_main_window,
                                            winmethod_Right
                                              | winmethod_Proportional,
                                            40, wintype_TextBuffer, 0);
#ifdef GSC_HAVE_ZCOLORS
      /* A window opened in colour mode starts in the theme's background: it
         takes the game's only from a clear made with the colours in force,
         and there is nothing in it yet to lose to one. */
      if (gsc_colour_enabled && gsc_a5_side_window)
        {
          gsc_colour_apply (gsc_a5_side_window, GSC_COLOUR_NONE);
          glk_window_clear (gsc_a5_side_window);
        }
#endif
    }
  return gsc_a5_side_window;
}

/*
 * gsc_a5_colour_top()
 *
 * The colour in force given a stack of nested colour spans: the innermost one
 * that names a colour.  A span that names none -- <font size=20> with no
 * colour attribute -- is transparent to colour, so the enclosing <font
 * colour="red"> still applies inside it, as it does in the Runner.
 */
static glui32
gsc_a5_colour_top (const glui32 *stack, int depth)
{
  while (depth > 0)
    {
      if (stack[depth - 1] != GSC_COLOUR_NONE)
        return stack[depth - 1];
      depth--;
    }
  return GSC_COLOUR_NONE;
}


/*
 * gsc_a5_colour_top_is_input()
 *
 * TRUE when the colour in force is the input colour -- i.e. the span
 * gsc_a5_colour_top() picked out is a <c> rather than a <font colour>.  Skips
 * colourless <font> spans the same way, so both answers describe one span.
 */
static int
gsc_a5_colour_top_is_input (const glui32 *stack, const int *is_input,
                            int depth)
{
  while (depth > 0)
    {
      if (stack[depth - 1] != GSC_COLOUR_NONE)
        return is_input[depth - 1];
      depth--;
    }
  return FALSE;
}


/* Every mark gsc_a5_display() acts on, as a string for strchr(): the text
   between two of these is a plain run for gsc_a5_put_string().  None of the
   marks is NUL, so a NUL never matches; the caller tests for it first. */
static const char GSC_A5_DISPLAY_MARKS[] = {
  A5_CLS_MARK, A5_WAITKEY_MARK, A5_IMG_MARK, A5_CENTER_MARK,
  A5_ENDCENTER_MARK, A5_BOLD_MARK, A5_ENDBOLD_MARK, A5_ITALIC_MARK,
  A5_ENDITALIC_MARK, A5_UNDERLINE_MARK, A5_ENDUNDERLINE_MARK, A5_RIGHT_MARK,
  A5_ENDRIGHT_MARK, A5_WINDOW_MARK, A5_ENDWINDOW_MARK, A5_SOUND_MARK,
  A5_COMMIT_MARK, A5_WAIT_MARK, A5_COLOUR_MARK, A5_ENDCOLOUR_MARK, '\0'
};

/*
 * One call of gsc_a5_display(): where it is in the text, the spans open
 * there, and the window the text is going to.  One member per mark.
 */
namespace {

struct gsc_a5_display_t
{
  const char *p;
  int center_depth, right_depth, bold_depth, italic_depth, underline_depth;
  glui32 colour_stack[GSC_MAX_STYLE_NESTING];
  int colour_is_input[GSC_MAX_STYLE_NESTING];
  int colour_depth;

  /* The window a run of text is currently going to: the main story window, or
     an author-defined side window between an A5_WINDOW_MARK span and its
     A5_ENDWINDOW_MARK.  A <cls> inside the span clears that side window. */
  winid_t cur_window;

  /* Whether a <waitkey> pause here is one a relaunch can come back to (the
     text is a turn's or the first opening's), and whether this is such a
     relaunch, still short of the pause the game was closed at.  Up to there
     everything is already in the restored windows and channels, so nothing
     is printed, drawn, played or waited for: only the spans and the window
     the text is routed to are followed, to be in force when it resumes. */
  int saved_pauses;
  int silent;

  void plain_run (const char *seg);
  void cls ();
  void window_open ();
  void window_close ();
  void waitkey ();
  void commit ();
  void colour_open ();
  void colour_close ();
  void span ();
  void wait ();
  void sound ();
  void image ();
  void finish ();
};

} /* namespace */

/* Flush the plain text run before this mark (or before the end). */
void
gsc_a5_display_t::plain_run (const char *seg)
{
  size_t n = (size_t) (p - seg);
  char *chunk = (char *) gsc_malloc (n + 1);
  memcpy (chunk, seg, n);
  chunk[n] = '\0';
  if (!silent)
    gsc_a5_put_string (chunk);
  else if (cur_window == gsc_main_window)
    gsc_main_window_empty = FALSE;
  free (chunk);
}

void
gsc_a5_display_t::cls ()
{
  if (!silent)
    glk_window_clear (cur_window);
  if (cur_window == gsc_main_window)
    gsc_main_window_empty = TRUE;
}

void
gsc_a5_display_t::window_open ()
{
  /* Side-window span opens: \022<name>\022.  Route text to the side
     window (opened lazily, right split); fall back to the main window
     if Glk cannot split.  The name delimits the span but is unused --
     this build routes every <window> to the one side window. */
  const char *e = strchr (p + 1, A5_WINDOW_MARK);
  winid_t w = gsc_a5_open_side_window ();

  cur_window = w != nullptr ? w : gsc_main_window;
  glk_set_window (cur_window);
  gsc_colour_apply (cur_window,
                    gsc_a5_colour_top (colour_stack, colour_depth));
  if (e != nullptr)
    p = e;
}

void
gsc_a5_display_t::window_close ()
{
  /* Side-window span closes: text returns to the main story window. */
  cur_window = gsc_main_window;
  glk_set_window (cur_window);
  gsc_colour_apply (cur_window,
                    gsc_a5_colour_top (colour_stack, colour_depth));
}

void
gsc_a5_display_t::waitkey ()
{
  event_t event;
  int resumed = FALSE;

  if (saved_pauses)
    gsc_a5_waitkey_count++;
  if (silent)
    {
      if (gsc_a5_waitkey_count < gsc_a5_waitkey_skip)
        return;
      /* The pause the game was closed at: what follows is new. */
      silent = FALSE;
      gsc_a5_waitkey_skip = 0;
      resumed = TRUE;
    }

  /* Bring the status line and the map up to date before pausing:
     a turn that walks into a room and then plays a cutscene there
     should show the new room on the map now, not at the next prompt
     once the whole cutscene has been paged through. */
  if (gsc_a5_run)
    gsc_a5_status (gsc_a5_run);
  gsc_map_redraw ();
#ifdef SPATTERLIGHT
  /* Save the game at the pause: after the panes are brought up to
     date (so the GUI snapshot has them) and before the key is asked
     for (so the archived windows carry no pending request).  Not
     when this is the pause just come back to, which is the save on
     disk already. */
  if (saved_pauses && !resumed)
    gsc_a5_autosave_waitkey ();
#else
  (void) resumed;
#endif
  glk_request_char_event (gsc_main_window);
  gsc_event_wait (evtype_CharInput, &event);
  /* gsc_a5_status (and gsc_map_redraw, if it opens the pane) leave
     the main window selected; restore the span's routing window so
     text after the pause stays in it. */
  glk_set_window (cur_window);
}

void
gsc_a5_display_t::commit ()
{
  /* Display-commit boundary with a dangling span (a5text.h): each
     commit is its own Source2HTML parse in the Runner, so a <center>,
     <right>, <b>, or <i> left open there ends now -- Death Shack's
     Introduction never closes its <center>, and the first room
     description (the next commit) must come out left-aligned, not
     centered. */
  center_depth = right_depth = bold_depth = italic_depth
    = underline_depth = 0;
  colour_depth = 0;
  glk_set_style (gsc_a5_span_style (0, 0, 0, 0, 0, FALSE));
  gsc_colour_apply (cur_window, GSC_COLOUR_NONE);
}

void
gsc_a5_display_t::colour_open ()
{
  /* Colour span opens: \027<value>\027, the value being a colour name
     or hex triplet, the reserved token "input" for <c> (which the
     Runner draws in its input colour), or empty for a <font> tag with
     no colour of its own. */
  const char *e = strchr (p + 1, A5_COLOUR_MARK);

  if (e != nullptr)
    {
      char token[32];
      size_t n = (size_t) (e - (p + 1));

      if (n >= sizeof token)
        n = sizeof token - 1;
      memcpy (token, p + 1, n);
      token[n] = '\0';

      if (colour_depth < GSC_MAX_STYLE_NESTING)
        {
          const int is_input = strcmp (token, "input") == 0;

          colour_is_input[colour_depth] = is_input;
          colour_stack[colour_depth++]
            = is_input ? gsc_colour_input
                       : gsc_colour_lookup (token);
          gsc_colour_apply (cur_window,
                            gsc_a5_colour_top (colour_stack,
                                               colour_depth));
          /* Colour spans carry a style too, for the colourless case
             (see gsc_a5_span_style); with colours on this re-asserts
             the style already in force. */
          glk_set_style (gsc_a5_span_style (center_depth, right_depth,
                                            bold_depth, italic_depth,
                                            underline_depth,
                                            gsc_a5_colour_top_is_input
                                              (colour_stack,
                                               colour_is_input,
                                               colour_depth)));
        }
      p = e;
    }
}

void
gsc_a5_display_t::colour_close ()
{
  if (colour_depth > 0)
    colour_depth--;
  gsc_colour_apply (cur_window,
                    gsc_a5_colour_top (colour_stack, colour_depth));
  glk_set_style (gsc_a5_span_style (center_depth, right_depth,
                                    bold_depth, italic_depth,
                                    underline_depth,
                                    gsc_a5_colour_top_is_input
                                      (colour_stack, colour_is_input,
                                       colour_depth)));
}

void
gsc_a5_display_t::span ()
{
  /* Alignment or character-style span boundary: like the Runner, only
     the style changes -- the game's own line breaks around an aligned
     span delimit the paragraph.  A <b> inside a <center> gets the
     weight-hinted centered style (User2); elsewhere bold is Subheader
     and italic/underline are Emphasized (Alert when combined with
     bold).  Right alignment uses style_Note and wins over character
     styles. */
  if (*p == A5_CENTER_MARK)
    center_depth++;
  else if (*p == A5_ENDCENTER_MARK && center_depth > 0)
    center_depth--;
  else if (*p == A5_RIGHT_MARK)
    right_depth++;
  else if (*p == A5_ENDRIGHT_MARK && right_depth > 0)
    right_depth--;
  else if (*p == A5_BOLD_MARK)
    bold_depth++;
  else if (*p == A5_ENDBOLD_MARK && bold_depth > 0)
    bold_depth--;
  else if (*p == A5_ITALIC_MARK)
    italic_depth++;
  else if (*p == A5_ENDITALIC_MARK && italic_depth > 0)
    italic_depth--;
  else if (*p == A5_UNDERLINE_MARK)
    underline_depth++;
  else if (*p == A5_ENDUNDERLINE_MARK && underline_depth > 0)
    underline_depth--;
  glk_set_style (gsc_a5_span_style (center_depth, right_depth,
                                    bold_depth, italic_depth,
                                    underline_depth,
                                    gsc_a5_colour_top_is_input
                                      (colour_stack, colour_is_input,
                                       colour_depth)));
}

void
gsc_a5_display_t::wait ()
{
  /* Timed-pause slot: \026<seconds>\026.  Run the same cancelable
     timer delay the ADRIFT 4 path gives its <wait x.x> tag, so
     Death Shack's letter-at-a-time title cards play out in real
     time (an empty or unparsable argument pauses not at all). */
  const char *e = strchr (p + 1, A5_WAIT_MARK);
  if (e != nullptr)
    {
      char arg[16];
      size_t n = (size_t) (e - (p + 1));
      if (n >= sizeof arg)
        n = sizeof arg - 1;
      memcpy (arg, p + 1, n);
      arg[n] = '\0';
      if (!silent)
        gsc_handle_wait_tag (arg);
      p = e;
    }
}

void
gsc_a5_display_t::sound ()
{
  /* Sound slot: \024<media event index>\024.  Fire it here, at the
     tag's place in the text -- before any later <waitkey>, so e.g. a
     sting cued ahead of a keypress-paced cutscene starts immediately
     (Pervert Action Crisis' maid encounter). */
  const char *e = strchr (p + 1, A5_SOUND_MARK);
  if (e != nullptr)
    {
      /* Passed in silence, a sound is on its restored channel
         already (or was stopped there); it is still marked as dealt
         with, to keep the after-the-turn sweep off it. */
      if (gsc_a5_run != nullptr && silent)
        a5run_media_note_shown (gsc_a5_run, (int) atol (p + 1));
      else if (gsc_a5_run != nullptr)
        gsc_a5_media_fire (gsc_a5_run, (int) atol (p + 1));
      p = e;
    }
}

void
gsc_a5_display_t::image ()
{
  /* Image slot: \006<Blorb resource number>\006. */
  const char *e = strchr (p + 1, A5_IMG_MARK);
  if (e != nullptr)
    {
      if (!silent)
        gsc_a5_draw_image (cur_window, (glui32) atol (p + 1));
      p = e;
    }
}

void
gsc_a5_display_t::finish ()
{
  /* A replay that never reached the pause it was after (the turn did not
     come out as it did before) is over all the same. */
  if (silent)
    gsc_a5_waitkey_skip = 0;

  /* A dangling span must not bleed into prompts and later turns. */
  if (center_depth > 0 || right_depth > 0
      || bold_depth > 0 || italic_depth > 0 || underline_depth > 0)
    glk_set_style (style_Normal);
  /* Nor a dangling colour span: the prompt and the player's input follow. */
  if (colour_depth > 0)
    gsc_colour_apply (cur_window, GSC_COLOUR_NONE);
  /* Likewise a dangling <window> span: prompts and input echo belong in the
     main story window. */
  if (cur_window != gsc_main_window)
    glk_set_window (gsc_main_window);
}

/*
 * gsc_a5_display()
 *
 * Present one turn's text.  The engine runs in interactive mode (see
 * a5text.h), so the text carries presentation marks: draw each embedded
 * image at its marked position, pause for a keypress at each <waitkey>
 * mark and for a timed delay at each <wait N> mark, clear the story window
 * at each <cls> mark, show <center> spans in the centered style_User1
 * (hinted for centered justification before the window opened), <right>
 * spans in RightFlush style_Note, <b>/<i> spans in bold/italic, and -- in
 * colour mode -- <font colour> and <c> spans in their colours.  This is the
 * same presentation the official Runner's output pane gives, e.g., Anno
 * 1700's intro: credits and cover image, "Press any key", then a cleared
 * screen for the opening narrative.

 */
void
gsc_a5_display (const char *text)
{
  const char *seg = text;
  gsc_a5_display_t d = {};

  d.p = text;
  d.cur_window = gsc_main_window;
  d.saved_pauses = gsc_a5_popup_context != GSC_A5_POPUP_ELSEWHERE;
  d.silent = d.saved_pauses && gsc_a5_waitkey_skip > 0;

  if (text == nullptr)
    return;

  glk_set_window (gsc_main_window);
  gsc_colour_apply (gsc_main_window, GSC_COLOUR_NONE);

  while (TRUE)
    {
      if (*d.p != '\0' && strchr (GSC_A5_DISPLAY_MARKS, *d.p) == nullptr)
        {
          d.p++;
          continue;
        }

      if (d.p > seg)
        d.plain_run (seg);
      if (*d.p == '\0')
        break;

      switch (*d.p)
        {
        case A5_CLS_MARK:
          d.cls ();
          break;
        case A5_WINDOW_MARK:
          d.window_open ();
          break;
        case A5_ENDWINDOW_MARK:
          d.window_close ();
          break;
        case A5_WAITKEY_MARK:
          d.waitkey ();
          break;
        case A5_COMMIT_MARK:
          d.commit ();
          break;
        case A5_COLOUR_MARK:
          d.colour_open ();
          break;
        case A5_ENDCOLOUR_MARK:
          d.colour_close ();
          break;
        case A5_CENTER_MARK:
        case A5_ENDCENTER_MARK:
        case A5_RIGHT_MARK:
        case A5_ENDRIGHT_MARK:
        case A5_BOLD_MARK:
        case A5_ENDBOLD_MARK:
        case A5_ITALIC_MARK:
        case A5_ENDITALIC_MARK:
        case A5_UNDERLINE_MARK:
        case A5_ENDUNDERLINE_MARK:
          d.span ();
          break;
        case A5_WAIT_MARK:
          d.wait ();
          break;
        case A5_SOUND_MARK:
          d.sound ();
          break;
        case A5_IMG_MARK:
          d.image ();
          break;
        }
      seg = ++d.p;
    }

  d.finish ();
}

/*
 * gsc_a5_sound_event()
 *
 * Execute one collected sound event: start, stop or pause its channel.
 */
static void
gsc_a5_sound_event (const a5_media_event_t *m)
{
  int ch = m->channel;
  int trace = getenv ("A5_TRACE_MEDIA") != nullptr;

  if (!gsc_a5_sound_ok)
    {
      if (trace)
        fprintf (stderr, "[a5 media] kind=%d ch=%d (no sound support)\n",
                 m->kind, ch);
      return;
    }
  /* The Runner has channels 1..8; a channel outside that range makes
     the whole tag a no-op there (clsSound.vb), so ignore it here too. */
  if (ch < 1 || ch >= GSC_A5_MAX_CHANNELS)
    {
      if (trace)
        fprintf (stderr, "[a5 media] ignore kind=%d ch=%d (range)\n",
                 m->kind, ch);
      return;
    }
  if (gsc_a5_channels[ch] == nullptr)
    gsc_a5_channels[ch] = glk_schannel_create ((glui32) ch);
  if (gsc_a5_channels[ch] == nullptr)
    return;
  if (m->kind == A5_MEDIA_SOUND_STOP)
    {
      if (trace)
        fprintf (stderr, "[a5 media] stop ch=%d\n", ch);
      glk_schannel_stop (gsc_a5_channels[ch]);
      gsc_a5_chan_sound[ch] = 0;
    }
  else if (m->kind == A5_MEDIA_SOUND_PAUSE)
    {
      if (trace)
        fprintf (stderr, "[a5 media] pause ch=%d\n", ch);
      glk_schannel_pause (gsc_a5_channels[ch]);
    }
  else if (m->number > 0)
    {
      /* Playing the sound a channel is already playing leaves it
         alone in the Runner ("just leave as is", clsSound.vb) -- a
         room description that embeds its background music must not
         restart the track every time the room is re-shown.  Only a
         different sound, or one that has finished (see
         gsc_a5_sound_finished), (re)starts the channel. */
      if ((glui32) m->number == gsc_a5_chan_sound[ch])
        {
          if (trace)
            fprintf (stderr, "[a5 media] keep ch=%d snd=%d (already "
                     "playing)\n", ch, m->number);
          glk_schannel_unpause (gsc_a5_channels[ch]);
        }
      else
        {
          if (trace)
            fprintf (stderr, "[a5 media] play ch=%d snd=%d loop=%d\n",
                     ch, m->number, m->loop);
          glk_schannel_play_ext (gsc_a5_channels[ch],
                                 (glui32) m->number,
                                 m->loop ? 0xffffffffu : 1, (glui32) ch);
          gsc_a5_chan_sound[ch] = (glui32) m->number;
        }
    }
}

/*
 * gsc_a5_sound_finished()
 *
 * Handle the notification that a sound played by gsc_a5_sound_event has
 * finished, so that playing it again on its channel restarts it.
 */
void
gsc_a5_sound_finished (const event_t *event)
{
  glui32 ch = event->val2;

  if (ch > 0 && ch < GSC_A5_MAX_CHANNELS
      && gsc_a5_chan_sound[ch] == event->val1)
    gsc_a5_chan_sound[ch] = 0;
}

/*
 * gsc_a5_media_fire()
 *
 * Fire the sound event behind a positional A5_SOUND_MARK in the turn text --
 * at its place in the display, ahead of any later <waitkey> pause, the way
 * the Runner acts on an <audio> tag the moment its DisplayText reaches it.
 * Flags the event shown so gsc_a5_show_media's after-the-turn sweep does not
 * replay it.
 */
void
gsc_a5_media_fire (a5_run_t *run, int idx)
{
  const a5_media_event_t *m = a5run_media_get (run, idx);

  if (m == nullptr || m->kind == A5_MEDIA_IMAGE)
    return;
  gsc_a5_sound_event (m);
  a5run_media_note_shown (run, idx);
}

/*
 * gsc_a5_show_media()
 *
 * Present the media events the engine collected for the turn just rendered:
 * start/stop sounds on their channels (images are drawn inline at their text
 * marks by gsc_a5_display, and sounds whose text reached the display already
 * fired at their own positional marks -- this sweep catches events whose
 * rendering was dropped, e.g. a deduped repeat response).  Returns the number
 * of images the turn embedded, so the caller can decide whether to fall back
 * to the cover.
 *
 * A leftover that merely REPEATS an event already fired at its mark is not
 * replayed: a description is rendered several times inside one turn (the stock
 * Look's two pre/post-action test renders, then the real one), so its <audio>
 * is recorded once per render while only the displayed render's copy carries a
 * mark.  The Runner never sees the test renders at all -- they never reach
 * DisplayText -- and replaying one here is not the no-op it looks like: the
 * sweep runs after the whole turn has been displayed, so a repeat play landing
 * behind a stop the player really did see restarts a track that was just
 * switched off (Grandpa's Ranch: MUSIC OFF, then the room view's own
 * background-music tag comes back through the sweep).  Only an event the
 * displayed text did not already carry -- e.g. a sound-only message dropped as
 * output-less -- still needs the sweep to fire it.
 */
int
gsc_a5_show_media (a5_run_t *run)
{
  int n = a5run_media_count (run), i, j, images = 0;
  int trace = getenv ("A5_TRACE_MEDIA") != nullptr;

  for (i = 0; i < n; i++)
    {
      const a5_media_event_t *m = a5run_media_get (run, i);

      if (m->kind == A5_MEDIA_IMAGE)
        {
          /* Images are drawn inline at their text marks by gsc_a5_display;
             count them here only so the intro's cover-art fallback knows an
             image was already part of the intro. */
          if (m->number > 0)
            images++;
          continue;
        }
      if (m->shown)
        {
          if (trace)
            fprintf (stderr, "[a5 media] skip %d/%d kind=%d ch=%d snd=%d "
                     "(fired at its mark)\n", i + 1, n, m->kind, m->channel,
                     m->number);
          continue;
        }
      for (j = 0; j < n; j++)
        {
          const a5_media_event_t *s = a5run_media_get (run, j);

          if (s->shown && s->kind == m->kind && s->number == m->number
              && s->channel == m->channel)
            break;
        }
      if (j < n)
        {
          if (trace)
            fprintf (stderr, "[a5 media] skip %d/%d kind=%d ch=%d snd=%d "
                     "(repeat of shown %d)\n", i + 1, n, m->kind, m->channel,
                     m->number, j + 1);
          continue;
        }
      if (trace)
        fprintf (stderr, "[a5 media] sweep %d/%d kind=%d ch=%d snd=%d (never "
                 "displayed)\n", i + 1, n, m->kind, m->channel, m->number);
      gsc_a5_sound_event (m);
    }
  return images;
}

/*
 * gsc_a5_undo_look()
 *
 * Re-show the player's surroundings after a successful UNDO, the way the v4
 * engine reprints the room name -- here the full room view, separated from
 * the "undone" line by a blank line.
 */
void
gsc_a5_undo_look (a5_run_t *run)
{
  char *look = a5run_look (run);

  if (look != nullptr && look[0] != '\0')
    {
      gsc_a5_put_string ("\n");
      gsc_a5_display (look);
      gsc_a5_show_media (run);
    }
  free (look);
}

/*
 * gsc_a5_show_cover()
 *
 * Draw the Blorb frontispiece (cover) image.  Used only when a game's intro
 * does not itself embed any image, so the cover is not shown twice.
 */
static void
gsc_a5_show_cover (void)
{
  giblorb_map_t *map;
  giblorb_result_t chunk;
  const unsigned char *p;

  if (!gsc_a5_graphics_ok)
    return;
  map = giblorb_get_resource_map ();
  if (map == nullptr)
    return;
  if (giblorb_load_chunk_by_type (map, giblorb_method_Memory, &chunk,
                                  giblorb_make_id ('F', 's', 'p', 'c'), 0)
      != giblorb_err_None)
    return;
  if (chunk.length >= 4 && chunk.data.ptr != nullptr)
    {
      p = (const unsigned char *) chunk.data.ptr;
      gsc_a5_draw_image (gsc_main_window,
                         ((glui32) p[0] << 24) | ((glui32) p[1] << 16)
                         | ((glui32) p[2] << 8) | (glui32) p[3]);
    }
  giblorb_unload_chunk (map, chunk.chunknum);
}

/*
 * gsc_a5_present_intro_media()
 *
 * Show the intro's media; if the intro embedded no image of its own, fall back
 * to the cover so a game with only a frontispiece still shows it.
 */
void
gsc_a5_present_intro_media (a5_run_t *run)
{
  if (gsc_a5_show_media (run) == 0)
    gsc_a5_show_cover ();
}

/*
 * gsc_a5_printed_width()
 * gsc_a5_next_char()
 *
 * The grid cells gsc_a5_put_string() fills for a UTF-8 string, and the start of
 * the character following the one a string points at.  With Unicode output each
 * multi-byte sequence prints as a single cell; without it the bytes go out as
 * they are, one cell each.  Stepping is by whole sequences either way, so that
 * a truncated tail stays well-formed UTF-8.
 */
static glui32
gsc_a5_printed_width (const char *string)
{
  const unsigned char *p = (const unsigned char *) string;
  glui32 width = 0;

  if (!gsc_unicode_enabled)
    return (glui32) strlen (string);

  for (; *p != '\0'; p++)
    {
      if ((*p & 0xc0) != 0x80)
        width++;
    }
  return width;
}

static const char *
gsc_a5_next_char (const char *string)
{
  const unsigned char *p = (const unsigned char *) string + 1;

  while ((*p & 0xc0) == 0x80)
    p++;
  return (const char *) p;
}

static const gsc_status_writer_t GSC_A5_STATUS_WRITER = {
  gsc_a5_printed_width, gsc_a5_put_string, gsc_a5_next_char
};


/*
 * gsc_a5_status()
 *
 * Redraw the one-line status window: the current room name at the left, and the
 * score (when the game keeps a Score variable) plus move count at the right.
 */
void
gsc_a5_status (a5_run_t *run)
{
  glui32 width;
  char *room;
  char right[96];
  long score, maxscore;
  glui32 room_end;

  if (!gsc_status_begin (&width))
    return;

  /* Right-hand score / moves. */
  score = a5run_score (run);
  maxscore = a5run_maxscore (run);
  if (maxscore > 0)
    snprintf (right, sizeof right, "Score: %ld/%ld  Moves: %d",
              score, maxscore, a5run_turns (run));
  else if (score != 0)
    snprintf (right, sizeof right, "Score: %ld  Moves: %d",
              score, a5run_turns (run));
  else
    snprintf (right, sizeof right, "Moves: %d", a5run_turns (run));

  /* Room name at the left. */
  glk_window_move_cursor (gsc_status_window, 1, 0);
  room = a5run_location_name (run);
  room_end = 1;
  if (room)
    {
      gsc_a5_put_string (room);
      room_end += gsc_a5_printed_width (room);
      free (room);
    }

  /* Right-justified score/moves. */
  gsc_status_put_right (width, room_end, right, &GSC_A5_STATUS_WRITER);

  gsc_status_end ();
}

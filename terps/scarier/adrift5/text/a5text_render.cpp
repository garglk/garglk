/* vi: set ts=8:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * Written from a reading of the ADRIFT 5 Runner source as released in
 * FrankenDrift, to reproduce its behaviour; no code from it is used.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * ADRIFT 5 support for Scarier -- text engine: the plain-text renderer
 * (split from a5text.cpp).  HTML-ish markup down to text, plus the media and
 * interactive-mode side channels.  See a5text.h and a5text_internal.h.
 */

#include "a5text_internal.h"

/* ------------------------------------------------------- media side channel */

static a5_media_cb a5_media_sink = nullptr;
static void *a5_media_sink_ctx = nullptr;

void
a5text_set_media_sink (a5_media_cb cb, void *ctx)
{
  a5_media_sink = cb;
  a5_media_sink_ctx = ctx;
}

/* Interactive-presentation mode: keep <cls>/<waitkey>/<img> as positional
   marks for the host instead of pre-resolving them (see a5text.h). */
static int a5_interactive_mode = 0;

void
a5text_set_interactive (int on)
{
  a5_interactive_mode = on;
}

int
a5text_interactive (void)
{
  return a5_interactive_mode;
}

/* Parse an <img>/<audio> tag body (without the angle brackets) and report it to
   the installed media sink: src="..." (image/sound file), and for <audio> the
   play/stop verb, channel=N and an optional loop flag.  Returns the sink's
   resolved resource number for an image, its recorded-event slot for a sound
   (see a5_media_cb), -1 when the sink has nothing for the caller to mark. */
static int
a5_emit_media (const std::string &tag, int is_img)
{
  std::string src;
  size_t sp = tag.find ("src=");
  if (sp != std::string::npos)
    {
      sp += 4;
      char quote = 0;
      if (sp < tag.size () && (tag[sp] == '"' || tag[sp] == '\''))
        quote = tag[sp++];
      while (sp < tag.size ()
             && (quote ? tag[sp] != quote : (tag[sp] != ' ' && tag[sp] != '>')))
        src.push_back (tag[sp++]);
    }

  if (is_img)
    return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_IMAGE, src.c_str (), 0, 0);

  /* <audio>: play (default), stop or pause, with channel=N and an optional
     loop flag.  This mirrors the Runner's tag parse (Global.vb, DisplayText):
     the verb is found by substring (" pause" / " stop", else play), the
     channel DEFAULTS TO 1 when there is no channel= attribute, and attribute
     values may be quoted.  The channel-1 default matters: games freely mix
     bare <audio play src=...> with <audio ... channel=1>, and both must land
     on the same channel so a new background track replaces the old one
     instead of layering over it. */
  {
    std::string low = tag;
    for (size_t k = 0; k < low.size (); k++)
      low[k] = (char) tolower ((unsigned char) low[k]);
    int is_pause = low.find (" pause") != std::string::npos;
    int is_stop = !is_pause && low.find (" stop") != std::string::npos;
    /* Loop only when explicitly loop=Y / loop=1 / loop=True.  ADRIFT writes
       loop=Y / loop=N, so a bare substring test for "loop" wrongly loops the
       (common) loop=N case forever. */
    int loop = 0;
    size_t lp = low.find ("loop=");
    if (lp != std::string::npos)
      {
        lp += 5;
        if (lp < low.size () && (low[lp] == '"' || low[lp] == '\''))
          lp++;
        loop = lp < low.size ()
               && (low[lp] == 'y' || low[lp] == '1' || low[lp] == 't');
      }
    int channel = 1;
    size_t cp = low.find ("channel=");
    if (cp != std::string::npos)
      {
        cp += 8;
        if (cp < tag.size () && (tag[cp] == '"' || tag[cp] == '\''))
          cp++;
        channel = atoi (tag.c_str () + cp);
      }
    if (is_pause)
      return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_SOUND_PAUSE, nullptr,
                            channel, 0);
    else if (is_stop)
      return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_SOUND_STOP, nullptr,
                            channel, 0);
    else
      return a5_media_sink (a5_media_sink_ctx, A5_MEDIA_SOUND, src.c_str (),
                            channel, loop);
  }
}

/* The value of a <font> tag's colour= / color= attribute, lowercased and with
   its quotes stripped; "" when the tag names no colour.  The attribute name is
   matched only at a word boundary, so the bgcolour= of a <font bgcolour="...">
   cannot be read as the text colour.  Whitespace is allowed on either side of
   the '=', because the Runner hands the tag to an HTML renderer and those
   allow it: Alyas of Starhollow rewrites the library movement message as
   "<font color = white>You move %PCase[%direction%]%.</font>", and without
   this the line comes out in the adventure's output colour rather than white.
   The result is capped well above any colour name or hex triplet -- it becomes
   the payload of a mark-delimited span, which nothing downstream should have
   to treat as unbounded. */
static std::string
a5_font_colour (const std::string &tag)
{
  static const char *const names[2] = { "colour", "color" };
  std::string low = tag;

  for (size_t k = 0; k < low.size (); k++)
    low[k] = (char) tolower ((unsigned char) low[k]);

  for (int pass = 0; pass < 2; pass++)
    {
      const std::string name = names[pass];

      for (size_t at = low.find (name); at != std::string::npos;
           at = low.find (name, at + 1))
        {
          size_t q = at + name.size ();
          char quote = 0;
          std::string out;

          if (at > 0 && isalpha ((unsigned char) low[at - 1]))
            continue;                          /* the tail of "bgcolour" */
          while (q < low.size () && isspace ((unsigned char) low[q]))
            q++;
          if (q >= low.size () || low[q] != '=')
            continue;                          /* an attribute of its own */
          q++;
          while (q < low.size () && isspace ((unsigned char) low[q]))
            q++;
          if (q < low.size () && (low[q] == '"' || low[q] == '\''))
            quote = low[q++];
          while (q < low.size () && out.size () < 24
                 && (quote ? low[q] != quote
                           : (!isspace ((unsigned char) low[q])
                              && low[q] != '>')))
            out.push_back (low[q++]);
          return out;
        }
    }
  return std::string ();
}

/* ----------------------------------------------------------- plain renderer */

/* <del>: delete the previous output glyph. */
static void
a5_render_del (sb_t &sb)
{
  /* Delete the previous output glyph (ADRIFT TextBoxes <del>).
     Operates on this fragment's buffer before any Glk flush, so
     style marks and newlines are still mutable -- including the
     common ALR rewrite of "(standing up first)" to <del> that
     undoes a paragraph break.  When this fragment has nothing left
     to eat the delete reaches back into the turn's earlier text
     (a whole-turn operator, like <cls>), which only finish_turn can
     see: leave an A5_DEL_MARK for sb_resolve_del.  Leave
     A5_ALR_MARK too so boundary ALRs cannot match across the former
     tag site. */
  if (sb_del_glyph (&sb, sb.len) == 0)
    sb_putc (&sb, A5_DEL_MARK);
  sb_putc (&sb, A5_ALR_MARK);
}

/* <cls>: clear what this fragment has buffered and leave the mark. */
static void
a5_render_cls (sb_t &sb)
{
  /* Screen clear: drop everything buffered so far in THIS fragment,
     mirroring the GlkHtmlWin / FrankenDrift.Headless handling of
     <cls>.  An Anno 1700-style intro ends with a <cls>, so its
     credits/preamble are wiped and only the post-clear text survives.
     But the runner's Display accumulates the whole turn's text and renders it
     once, so a <cls> also wipes everything emitted EARLIER in the
     turn (other fragments already in `out`).  Leave a marker byte so
     the per-turn flush can drop that earlier text too.

     Interactively the pre-clear text is a real page the player must
     see (the Runner shows it, then clears its window): keep the
     text, leave only the mark for the host to clear at. */
  if (!a5_interactive_mode)
    sb_clear (&sb);
  sb_putc (&sb, A5_CLS_MARK);
}

/* Interactive <wait N>: the delay as a mark-delimited span. */
static void
a5_render_wait (sb_t &sb, const char *tag)
{
  /* Timed pause <wait N>: leave the delay argument as a
     positional \026<seconds>\026 span for the host to run a
     cancelable timer at, the way the Runner's rendering pauses
     mid-Display (<wait is in its valid-tag list,
     clsUserSession.vb).  Death Shack's title cards type one
     80-point letter per <wait 3>. */
  const char *arg = tag[4] == ' ' ? tag + 5 : "";
  sb_putc (&sb, A5_WAIT_MARK);
  sb_puts (&sb, arg);
  sb_putc (&sb, A5_WAIT_MARK);
}

/* The paired interactive style tags that are one mark each -- centre, bold,
   italic, underline, right.  Returns 1 when NAME was one and is written. */
static int
a5_render_style_tag (sb_t &sb, const char *name)
{
  if (a5_interactive_mode
      && (strcmp (name, "center") == 0
          || strcmp (name, "centre") == 0))
    /* Centered span opens; like waitkey, the mark also blocks ALRs. */
    sb_putc (&sb, A5_CENTER_MARK);
  else if (a5_interactive_mode
           && (strcmp (name, "/center") == 0
               || strcmp (name, "/centre") == 0))
    sb_putc (&sb, A5_ENDCENTER_MARK);
  else if (a5_interactive_mode && strcmp (name, "b") == 0)
    /* Bold span opens; like center, the mark also blocks ALRs (a bare
       control byte can't sit inside a game OldText, so the boundary
       strstr cannot match across it).  Headlessly <b> still drops to
       A5_ALR_MARK below, so the ground-truth text is unchanged. */
    sb_putc (&sb, A5_BOLD_MARK);
  else if (a5_interactive_mode && strcmp (name, "/b") == 0)
    sb_putc (&sb, A5_ENDBOLD_MARK);
  else if (a5_interactive_mode && strcmp (name, "i") == 0)
    /* Italic span opens; same ALR-blocking / headless-strip contract
       as <b>. */
    sb_putc (&sb, A5_ITALIC_MARK);
  else if (a5_interactive_mode && strcmp (name, "/i") == 0)
    sb_putc (&sb, A5_ENDITALIC_MARK);
  else if (a5_interactive_mode && strcmp (name, "u") == 0)
    /* Underline span opens; distinct marks so a future CSS/Glk host
       can paint real underline.  Until then the Glk path styles them
       like italic (Emphasized / Alert with bold). */
    sb_putc (&sb, A5_UNDERLINE_MARK);
  else if (a5_interactive_mode && strcmp (name, "/u") == 0)
    sb_putc (&sb, A5_ENDUNDERLINE_MARK);
  else if (a5_interactive_mode && strcmp (name, "right") == 0)
    /* Right-aligned span opens; same ALR-blocking / headless-strip
       contract as <center>. */
    sb_putc (&sb, A5_RIGHT_MARK);
  else if (a5_interactive_mode && strcmp (name, "/right") == 0)
    sb_putc (&sb, A5_ENDRIGHT_MARK);
  else
    return 0;
  return 1;
}

/* Interactive <c> / <font>: the colour as a mark-delimited span.  The tag
   body runs from P + 1 to TAGEND. */
static void
a5_render_colour (sb_t &sb, const char *name, const char *p,
                  const char *tagend)
{
  /* Colour span opens: \027<value>\027 (see a5text.h).  <c> asks
     for the adventure's InputColour, which is not spelled out in
     the text, so it writes the reserved token "input"; a <font>
     writes its colour= attribute, or nothing at all when it names
     no colour -- it then inherits the enclosing colour, and its
     </font> still has a span to pop.  Headlessly both tags drop to
     A5_ALR_MARK below, so ground truth is unchanged. */
  sb_putc (&sb, A5_COLOUR_MARK);
  if (name[0] == 'c')
    sb_puts (&sb, "input");
  else
    sb_puts (&sb, a5_font_colour (std::string (p + 1, tagend))
                    .c_str ());
  sb_putc (&sb, A5_COLOUR_MARK);
}

/* Interactive <window>: the window name as a mark-delimited span. */
static void
a5_render_window (sb_t &sb, const char *tag)
{
  /* Secondary output window opens: leave the window name delimited
     by A5_WINDOW_MARK (like an image span) so the host can route the
     enclosed text to that named side window.  The name comes from
     the tag argument (lowercased with the rest of the tag); default
     to "main" when the author wrote a bare <window>.  Headlessly this
     tag drops to A5_ALR_MARK below, so ground truth is unchanged. */
  const char *wn = tag;                /* skip the "window" name */
  while (*wn != '\0' && *wn != ' ') wn++;
  while (*wn == ' ') wn++;             /* then any separating spaces */
  sb_putc (&sb, A5_WINDOW_MARK);
  sb_puts (&sb, *wn != '\0' ? wn : "main");
  sb_putc (&sb, A5_WINDOW_MARK);
}

/* <img> / <audio> with a media sink installed.  The tag body runs from
   P + 1 to TAGEND. */
static void
a5_render_media (sb_t &sb, const char *name, const char *p,
                 const char *tagend)
{
  /* Embedded media: report it out of band; it still drops from the
     plain text (so the text output is unchanged).  Interactively an
     image also leaves \006<resource>\006 at the tag's position so
     the host draws it where the author placed it, and a sound
     leaves \024<event index>\024 so the host starts/stops it there
     -- ahead of any later <waitkey>, as the Runner does. */
  int is_img = strcmp (name, "img") == 0;
  int res = a5_emit_media (std::string (p + 1, tagend), is_img);
  if (a5_interactive_mode && is_img && res > 0)
    {
      char nbuf[16];
      snprintf (nbuf, sizeof nbuf, "%c%d%c",
                A5_IMG_MARK, res, A5_IMG_MARK);
      sb_puts (&sb, nbuf);
    }
  else if (a5_interactive_mode && !is_img && res >= 0)
    {
      char nbuf[16];
      snprintf (nbuf, sizeof nbuf, "%c%d%c",
                A5_SOUND_MARK, res, A5_SOUND_MARK);
      sb_puts (&sb, nbuf);
    }
  else
    sb_putc (&sb, A5_ALR_MARK);
}

/* One tag, P at its '<': write what it renders to and return the position
   past its '>' (or at the terminator, for a tag left open). */
static const char *
a5_render_tag (sb_t &sb, const char *p)
{
  const char *q = p + 1;
  char tag[16];
  size_t tl = 0;
  while (*q && *q != '>')
    {
      if (tl + 1 < sizeof tag)
        tag[tl++] = (char) tolower ((unsigned char) *q);
      q++;
    }
  tag[tl] = '\0';
  const char *tagend = q;     /* at '>' or '\0' */
  if (*q == '>')
    q++;
  /* Tag name = the leading non-space run (img, audio, br, cls, ...). */
  char name[16];
  {
    size_t k = 0;
    while (tag[k] != '\0' && tag[k] != ' ' && k + 1 < sizeof name)
      { name[k] = tag[k]; k++; }
    name[k] = '\0';
  }
  if (strcmp (name, "br") == 0)
    sb_putc (&sb, '\n');
  else if (strcmp (name, "del") == 0)
    a5_render_del (sb);
  else if (strcmp (name, "cls") == 0)
    a5_render_cls (sb);
  else if (a5_interactive_mode && strcmp (name, "waitkey") == 0)
    /* Interactive pause point; doubles as the usual ALR tag blocker. */
    sb_putc (&sb, A5_WAITKEY_MARK);
  else if (a5_interactive_mode && strcmp (name, "wait") == 0)
    a5_render_wait (sb, tag);
  else if (a5_render_style_tag (sb, name))
    /* a paired style mark, written */;
  else if (a5_interactive_mode
           && (strcmp (name, "c") == 0 || strcmp (name, "font") == 0))
    a5_render_colour (sb, name, p, tagend);
  else if (a5_interactive_mode
           && (strcmp (name, "/c") == 0 || strcmp (name, "/font") == 0))
    sb_putc (&sb, A5_ENDCOLOUR_MARK);
  else if (a5_interactive_mode && strcmp (name, "window") == 0)
    a5_render_window (sb, tag);
  else if (a5_interactive_mode && strcmp (name, "/window") == 0)
    sb_putc (&sb, A5_ENDWINDOW_MARK);
  else if (a5_media_sink != nullptr
           && (strcmp (name, "img") == 0 || strcmp (name, "audio") == 0))
    a5_render_media (sb, name, p, tagend);
  else
    /* every other tag (<>, <left>...) drops -- but leave A5_ALR_MARK
       so the display-boundary ALR pass cannot match an OldText ACROSS
       the stripped tag (the runner's ALR sees the
       tag and is blocked; see a5text.h).  finish_turn strips the mark. */
    sb_putc (&sb, A5_ALR_MARK);
  return q;
}

char *
a5text_render_plain (const char *src)
{
  sb_t sb;
  const char *p = src;
  sb_init (&sb);

  while (*p != '\0')
    {
      if (*p == '<')
        {
          p = a5_render_tag (sb, p);
          continue;
        }
      if (*p == '&')
        {
          if (strncmp (p, "&perc;", 6) == 0) { sb_putc (&sb, '%'); p += 6; continue; }
          if (strncmp (p, "&amp;", 5) == 0)  { sb_putc (&sb, '&'); p += 5; continue; }
          if (strncmp (p, "&lt;", 4) == 0)   { sb_putc (&sb, '<'); p += 4; continue; }
          if (strncmp (p, "&gt;", 4) == 0)   { sb_putc (&sb, '>'); p += 4; continue; }
          if (strncmp (p, "&quot;", 6) == 0) { sb_putc (&sb, '"'); p += 6; continue; }
        }
      sb_putc (&sb, *p);
      p++;
    }
  return sb_finish (&sb);
}

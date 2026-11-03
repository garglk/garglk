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
 * os_glk_output.cpp: game output -- tags, the font stack and styles, the
 * game palette (colour mode), inline graphics and hints.  Split out of
 * os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*---------------------------------------------------------------------*/
/*  Glk port output functions                                          */
/*---------------------------------------------------------------------*/

/*
 * Flag for if the user entered "help" as their last input, or if hints have
 * been silenced as a result of already using a Glk command.
 */
static int gsc_help_requested = FALSE,
           gsc_help_hints_silenced = FALSE;

/*
 * Adrift text colours ("glk colour").
 *
 * Adrift games are written for a Runner that paints its output pane in a
 * palette rather than in the interpreter's theme: a background, an "output"
 * (replies) colour for the story, and an "input" (typed) colour that <c>...</c>
 * also asks for, on top of whatever <font colour="..."> the text names.  Glk
 * has no colour of its own, so by default this port drops all of that and
 * shows the story in the interpreter's styles, as SCARE always has.
 *
 * "glk colour on" turns the palette back on, through the Gargoyle/Spatterlight
 * garglk_set_zcolors extension.  Where that extension is missing (cheapglk,
 * glkterm) the mode cannot be offered at all, so everything below compiles out
 * and the command says so.
 *
 * A game that cannot be read without the palette -- an ADRIFT 5 adventure that
 * set colours of its own, or text that paints a background or names a colour
 * too close to the interpreter's to see -- turns the mode on for itself at load
 * time, before a word is printed; gsc_colour_detect is what asks.
 *
 * Where the palette comes from differs by engine.  ADRIFT 5 stores it in the
 * adventure itself (a5model.h bg_colour and friends).  ADRIFT <=4 stores none:
 * the .taf has no colour fields, and the colours are Runner preferences.  The
 * defaults below are the ones run400.exe ships, read out of the settings it
 * writes under HKCU\Software\VB and VBA Program Settings\ADRIFT\Runner:
 * Background 0, Text1 3289855 and Text2 65280 -- OLE colour integers (low byte
 * red), i.e. black, #FF3232 for typed text and #00FF00 for replies.  They match
 * the swatches in the Runner's own Options -> Display & Media dialog ("Set
 * typed colour", "Set replies colour", "Set background colour").
 */
/* GSC_HAVE_ZCOLORS is in os_glk_internal.h; the palette itself and the on/off
   flag are with the other module options in os_glk.cpp, since the status line
   draws itself in the game's colours as well. */

/* Font descriptor type, encapsulating size, monospaced boolean, face, and the
   text colour a <font colour="..."> asked for (GSC_COLOUR_NONE when it asked
   for none, which inherits the enclosing colour). */
typedef struct {
  scr_bool is_monospaced;
  scr_int size;
  gsc_symbol_font_t symbol_font;
  glui32 colour;
} gsc_font_size_t;

/* Font stack and attributes for nesting tags. */
static gsc_font_size_t gsc_font_stack[GSC_MAX_STYLE_NESTING];
static glui32 gsc_font_index = 0;
static glui32 gsc_attribute_bold = 0,
              gsc_attribute_italic = 0,
              gsc_attribute_underline = 0,
              gsc_attribute_secondary_colour = 0,
              gsc_attribute_center = 0,
              gsc_attribute_right = 0;

/* Notional default font size, and limit font sizes. */
static const scr_int GSC_DEFAULT_FONT_SIZE = 12,
                    GSC_MEDIUM_FONT_SIZE = 14,
                    GSC_LARGE_FONT_SIZE = 16;

/* Milliseconds per second and timeouts count for delay tags. */
static const glui32 GSC_MILLISECONDS_PER_SECOND = 1000;
static const glui32 GSC_TIMEOUTS_COUNT = 10;

/* The keypresses used to cancel any <wait x.x> early. */
static const glui32 GSC_CANCEL_WAIT_1 = ' ',
                    GSC_CANCEL_WAIT_2 = keycode_Return;


/*
 * gsc_note_help_request()
 * gsc_output_silence_help_hints()
 * gsc_output_provide_help_hint()
 *
 * Register a standalone "help" at the start of the player's input, and print
 * a note of how to get Glk command help from the interpreter unless silenced.
 */
void
gsc_note_help_request (const char *command)
{
  if (scr_strncasecmp (command, "help", strlen ("help")) == 0
      && strspn (command + strlen ("help"), "\t ")
         == strlen (command + strlen ("help")))
    gsc_help_requested = TRUE;
}

void
gsc_output_silence_help_hints (void)
{
  gsc_help_hints_silenced = TRUE;
}

void
gsc_output_provide_help_hint (void)
{
  if (gsc_help_requested && !gsc_help_hints_silenced)
    {
      glk_set_style (style_Emphasized);
      gsc_put_literal ("[Try 'glk help' for help on special interpreter"
                       " commands]\n");

      gsc_help_requested = FALSE;
      glk_set_style (style_Normal);
    }
}


/*
 * gsc_colour_lookup()
 *
 * Turn an Adrift colour token -- a name, or a hex triplet with or without its
 * leading '#' -- into an 0xRRGGBB colour.  Returns GSC_COLOUR_NONE for the
 * empty token and for "default", both of which mean "back to the surrounding
 * colour" rather than a colour of their own.
 *
 * The names, and their values, are the Runner's own table (Global.ColourLookup),
 * including its aliases (cyan/turquoise/aqua, magenta/fuchsia) and its
 * treatment of anything unrecognised as a hex triplet right-padded with zeros.
 * `token` must already be lowercased and unquoted.
 */
glui32
gsc_colour_lookup (const char *token)
{
  static const struct { const char *name; glui32 rgb; } COLOUR_NAMES[] = {
    {"black",     0x000000}, {"blue",      0x0000ff},
    {"cyan",      0x00ffff}, {"turquoise", 0x00ffff}, {"aqua", 0x00ffff},
    {"gray",      0x808080}, {"grey",      0x808080},
    {"green",     0x008000}, {"lime",      0x00ff00},
    {"magenta",   0xff00ff}, {"fuchsia",   0xff00ff},
    {"maroon",    0x800000}, {"navy",      0x000080},
    {"olive",     0x808000}, {"orange",    0xff8000},
    {"pink",      0xff8888}, {"purple",    0x800080},
    {"red",       0xff0000}, {"silver",    0xc0c0c0},
    {"teal",      0x008080}, {"white",     0xffffff},
    {"yellow",    0xffff00}, {nullptr, 0}
  };
  char hex[7];
  int index_;
  glui32 rgb;

  if (token == nullptr || token[0] == '\0' || strcmp (token, "default") == 0)
    return GSC_COLOUR_NONE;

  for (index_ = 0; COLOUR_NAMES[index_].name; index_++)
    {
      if (strcmp (token, COLOUR_NAMES[index_].name) == 0)
        return COLOUR_NAMES[index_].rgb;
    }

  /* Not a name, so a hex triplet.  The Runner drops a leading '#', gives up on
     anything longer than six digits (white), and right-pads a short one with
     zeros -- "<font colour=f00>" is 0xf00000 there, not 0xff0000. */
  if (token[0] == '#')
    token++;
  if (strlen (token) > 6)
    return 0xffffff;
  for (index_ = 0; index_ < 6; index_++)
    hex[index_] = token[index_] != '\0' ? token[index_] : '0';
  hex[6] = '\0';
  for (index_ = 0; index_ < 6; index_++)
    {
      if (!isxdigit ((unsigned char) hex[index_]))
        return GSC_COLOUR_NONE;
    }
  rgb = (glui32) strtoul (hex, nullptr, 16);
  return rgb;
}


/*
 * gsc_colour_from_tag()
 *
 * Dig the value of `key` -- "colour" or "bgcolour" -- out of a tag argument,
 * accepting the American spelling too, and return the colour it names.
 * Returns GSC_COLOUR_NONE when the tag carries no such attribute, so a
 * <font face=...> with no colour leaves the colour alone.
 *
 * The key has to match at a word boundary or "colour" would also be found
 * inside "bgcolour", and a background would silently become a foreground.
 *
 * Whitespace is allowed on either side of the '=', as it is in the HTML the
 * Runner hands its tags to; see a5_font_colour() in adrift5/text/a5text_render.cpp.
 */
static glui32
gsc_colour_from_tag (const scr_char *tag, const char *key)
{
  char american[16], token[32];
  const char *keys[2];
  scr_char *lower;
  glui32 result = GSC_COLOUR_NONE;
  int pass;
  scr_int index_;

  /* Both the key and the value it finds are compared lowercased; the tag
     arrives as the author wrote it. */
  lower = (decltype (lower)) gsc_malloc (strlen (tag) + 1);
  memcpy (lower, tag, strlen (tag) + 1);
  for (index_ = 0; lower[index_] != '\0'; index_++)
    lower[index_] = glk_char_to_lower (lower[index_]);

  /* "colour" -> "color": drop the 'u' before the final 'r'. */
  {
    size_t length = strlen (key), index_ = 0, out = 0;

    assert (length < sizeof (american));
    for (; index_ < length; index_++)
      {
        if (!(key[index_] == 'u' && index_ + 2 == length))
          american[out++] = key[index_];
      }
    american[out] = '\0';
  }
  keys[0] = key;
  keys[1] = american;

  for (pass = 0; pass < 2 && result == GSC_COLOUR_NONE; pass++)
    {
      const scr_char *at = lower;

      while ((at = strstr (at, keys[pass])) != nullptr)
        {
          const scr_char *value;
          size_t length = 0;

          if (at > lower && (isalnum ((unsigned char) at[-1]) || at[-1] == '_'))
            {
              at += strlen (keys[pass]);
              continue;
            }
          value = at + strlen (keys[pass]);
          while (isspace ((unsigned char) *value))
            value++;
          if (*value != '=')
            {
              /* A "colour" that is not an attribute assignment at all. */
              at += strlen (keys[pass]);
              continue;
            }
          value++;
          while (isspace ((unsigned char) *value))
            value++;
          if (*value == '"' || *value == '\'')
            {
              scr_char quote = *value++;
              while (value[length] != '\0' && value[length] != quote)
                length++;
            }
          else
            {
              while (value[length] != '\0'
                     && !isspace ((unsigned char) value[length])
                     && value[length] != '>' && value[length] != '"')
                length++;
            }
          if (length >= sizeof (token))
            length = sizeof (token) - 1;
          memcpy (token, value, length);
          token[length] = '\0';
          result = gsc_colour_lookup (token);
          break;
        }
    }

  free (lower);
  return result;
}


#ifdef GSC_HAVE_ZCOLORS
/*
 * gsc_colour_luminance()
 * gsc_colour_legible()
 *
 * Whether text in one colour can be read against another.  `gsc_colour_legible`
 * answers the WCAG contrast test -- the ratio of the two relative luminances,
 * each lifted by 0.05, against a 3:1 floor, which is the threshold for text
 * that is large or merely decorative rather than a page of body copy.
 *
 * Luminance is the sRGB one with the transfer curve approximated as a plain
 * square rather than the piecewise 2.4 power, which keeps this in integers (the
 * scale is 255*255) and, checked against the exact formula over the Runner's
 * whole colour table, changes no verdict here: red, green, blue, teal, navy and
 * grey stay legible on white, while white, silver, yellow, lime, cyan, orange
 * and pink stay illegible on it.
 */
static glui32
gsc_colour_luminance (glui32 rgb)
{
  const glui32 r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;

  return (2126 * r * r + 7152 * g * g + 722 * b * b) / 10000;
}

static scr_bool
gsc_colour_legible (glui32 fg, glui32 bg)
{
  /* 0.05 of the 255*255 luminance scale, the constant WCAG adds to both sides
     so that black on black is a ratio of 1 rather than a division by zero. */
  static const glui32 FLARE = 3251;
  const glui32 one = gsc_colour_luminance (fg) + FLARE,
               two = gsc_colour_luminance (bg) + FLARE;
  const glui32 lighter = one > two ? one : two,
               darker = one > two ? two : one;

  return lighter >= 3 * darker;
}


/*
 * gsc_colour_wanted_t
 * gsc_colour_wanted_by()
 *
 * Decide, from one authored string, whether this game was written for the
 * Runner's own pane rather than for an interpreter theme -- the question
 * gsc_colour_detect() asks of every string in the game.
 *
 * Two things in a string answer it, and a bare <font colour="..."> is not one
 * of them: a game that puts a single red word in a paragraph is not asking for
 * a black screen, and a quarter of the ADRIFT <=4 corpus embeds a colour that
 * mild.  What does answer it is
 *
 *   - <bgcolour="..."> (ADRIFT <=4), which repaints the output pane outright.
 *     Without colour mode the repaint has nowhere to land, so the text it was
 *     meant to sit behind arrives on whatever the theme provides;
 *
 *   - a <font colour="..."> the theme cannot show: white, yellow, lime and the
 *     other pale colours authors reach for when they know the pane behind them
 *     is black.  On a light theme that text is invisible, which is the failure
 *     colour mode exists to prevent; on a dark theme it reads perfectly well
 *     already, and `bg` being the measured theme background is what lets the
 *     same game decide differently in each.
 *
 * <c> needs no case of its own even though it colours text too: it names the
 * palette's input colour, which for ADRIFT <=4 is the Runner's red (legible on
 * any theme) and for ADRIFT 5 is the author's, already caught by
 * a5model_custom_palette.
 */
typedef struct {
  glui32 background;      /* what the story text would otherwise sit on */
} gsc_colour_wanted_t;

static scr_bool
gsc_colour_wanted_by (const scr_char *text, void *opaque)
{
  const gsc_colour_wanted_t *wanted = (const gsc_colour_wanted_t *) opaque;
  const scr_char *at;

  for (at = text; (at = strchr (at, '<')) != nullptr; at++)
    {
      const scr_char *end = strchr (at, '>');
      scr_char *tag;
      size_t length;
      glui32 colour;

      if (end == nullptr)
        break;

      /* Only <font ...> and <bgcolour=...> can carry one, and every other tag
         in the language is far commoner than either; test the name first so
         the tag body is only copied for the few that could match. */
      length = (size_t) (end - at) - 1;
      if (scr_strncasecmp (at + 1, "font", 4) != 0
          && scr_strncasecmp (at + 1, "bgcolo", 6) != 0)
        continue;

      tag = (decltype (tag)) gsc_malloc (length + 1);
      memcpy (tag, at + 1, length);
      tag[length] = '\0';

      colour = gsc_colour_from_tag (tag, "bgcolour");
      if (colour != GSC_COLOUR_NONE)
        {
          free (tag);
          return TRUE;
        }
      colour = gsc_colour_from_tag (tag, "colour");
      free (tag);
      if (colour != GSC_COLOUR_NONE
          && !gsc_colour_legible (colour, wanted->background))
        return TRUE;

      at = end;
    }
  return FALSE;
}


/*
 * gsc_colour_detect()
 *
 * Whether this game starts in its own palette without being asked for it.
 *
 * ADRIFT 5 answers from its header: an adventure whose palette differs from the
 * Runner's default has a look the author sat down and chose, and showing it in
 * the interpreter's theme instead throws that away.  ADRIFT <=4 has no header
 * to ask -- the .taf carries no colour fields at all, the palette being a Runner
 * preference -- so both engines fall back on the authored text, which is where
 * gsc_colour_wanted_by describes what counts.
 *
 * Called while the loading window is still up, so the theme's background can be
 * measured before colour mode publishes stylehints of its own; the answer feeds
 * gsc_colour_startup, which the -c switch also sets, and so reaches the palette
 * by the same path -- hints published before the first window open, and no
 * rebuild.  That also means it is never consulted on an autorestore, which is
 * right: a restored session brings back the mode the player left it in.
 */
scr_bool
gsc_colour_detect (winid_t window)
{
  gsc_colour_wanted_t wanted;
  glui32 measured;

  /* White when the library cannot measure: an interpreter that answers nothing
     is likeliest to be showing the light background Glk defaults to, and that
     is the case where pale authored text disappears. */
  wanted.background = 0xffffff;
  if (window != nullptr
      && glk_style_measure (window, style_Normal, stylehint_BackColor,
                            &measured))
    wanted.background = measured;

  if (gsc_is_a5)
    return gsc_a5_adv != nullptr
           && (a5model_custom_palette (gsc_a5_adv)
               || a5model_scan_text (gsc_a5_adv, gsc_colour_wanted_by,
                                     &wanted));
  return gsc_game != nullptr
         && scr_game_scan_strings (gsc_game, gsc_colour_wanted_by, &wanted);
}
#endif


/*
 * gsc_colour_apply()
 *
 * Set the colours later text written to `win` comes out in: `fg` if it names
 * one, otherwise the game's normal output colour, always over the game's
 * background.  Does nothing at all unless the colour mode is on, so every
 * caller can call it unconditionally.
 */
void
gsc_colour_apply (winid_t win, glui32 fg)
{
#ifdef GSC_HAVE_ZCOLORS
  if (!gsc_colour_enabled || win == nullptr)
    return;
  if (fg == GSC_COLOUR_NONE)
    fg = gsc_colour_output;
  if (win == gsc_main_window)
    gsc_colour_main_fg = fg;
  garglk_set_zcolors_stream (glk_window_get_stream (win),
                             fg, gsc_colour_background);
#else
  (void) win;
  (void) fg;
#endif
}


/*
 * gsc_colour_echo()
 *
 * Switch between the colour the player's typing is echoed in and the colour
 * the game's own text comes out in, for gsc_read_line_locale().  Does
 * nothing at all unless colour mode is on.
 */
void
gsc_colour_echo (scr_bool typing)
{
  gsc_colour_apply (gsc_main_window,
                    typing ? gsc_colour_input : GSC_COLOUR_NONE);
}


/*
 * gsc_put_prompt()
 *
 * Print the input prompt.  The Runner has no inline prompt of its own -- it
 * takes commands in a separate box, drawn in the "input" colour, the same
 * secondary colour <c> spans use -- so in colour mode the ">" belongs to the
 * typing it introduces rather than to the game's output, and is written in
 * that colour.  Outside colour mode gsc_colour_echo does nothing and this is
 * an ordinary print.
 *
 * The colour is deliberately left in force: what follows a prompt is always
 * the player's line, which the read routines echo in the same ink before
 * putting the output colour back.
 */
void
gsc_put_prompt (const char *prompt)
{
  gsc_colour_echo (TRUE);
  gsc_put_literal (prompt);
}


/*
 * gsc_echo_input()
 *
 * Echo a line of input the player did not type -- a step of a map walk, a
 * scripted line, a line played back from an input log -- in the input style,
 * so that the transcript reads as though it had been typed.  `put` is the
 * print routine for the engine's own text encoding; `end_line` adds the
 * newline a typed line would have ended with.  No keyboard read follows to
 * take the prompt's input colour back off before the turn's own text, so
 * that is done here.
 */
void
gsc_echo_input (void (*put) (const char *), const char *line,
                scr_bool end_line)
{
  glk_set_style (style_Input);
  put (line);
  glk_set_style (style_Normal);
  if (end_line)
    put ("\n");
  gsc_colour_echo (FALSE);
}


/*
 * gsc_font_top()
 *
 * The current top of the font stack, or the default font on an empty stack.
 */
static gsc_font_size_t
gsc_font_top (void)
{
  gsc_font_size_t font;

  if (gsc_font_index > 0)
    font = gsc_font_stack[gsc_font_index - 1];
  else
    {
      font.is_monospaced = FALSE;
      font.size = GSC_DEFAULT_FONT_SIZE;
      font.symbol_font = GSC_SYMBOL_NONE;
      font.colour = GSC_COLOUR_NONE;
    }
  return font;
}


/*
 * gsc_set_glk_style()
 *
 * Set a Glk style based on the top of the font stack and attributes.
 */
static void
gsc_set_glk_style (void)
{
  const gsc_font_size_t font = gsc_font_top ();
  const scr_bool is_monospaced = font.is_monospaced;
  const scr_int font_size = font.size;

  /*
   * In colour mode the Adrift colours ride alongside the Glk style: an
   * explicit <font colour=...> if one is open, otherwise the typed colour
   * while <c> is (the Runner's "replies" colour being the normal one).  The
   * Glk style is still set below, so bold and centering keep working; only
   * the ink changes.
   */
  gsc_colour_apply (gsc_main_window,
                    font.colour != GSC_COLOUR_NONE ? font.colour
                    : gsc_attribute_secondary_colour > 0 ? gsc_colour_input
                    : GSC_COLOUR_NONE);

  /*
   * Map the font and current attributes into a Glk style.  Because Glk styles
   * aren't cumulative this has to be done by precedences.
   */
  if (is_monospaced)
    {
      /*
       * No matter the size or attributes, if monospaced use Preformatted
       * style, as it's all we have.
       */
      glk_set_style (style_Preformatted);
    }
  else if (gsc_attribute_right > 0)
    {
      /* RightFlush-hinted style_Note; character styles are not combined. */
      glk_set_style (style_Note);
    }
  else if (gsc_attribute_center > 0)
    {
      /*
       * Centered sections use the two justification-hinted user styles set up
       * before the main window opened: User1 for plain centered text, User2
       * for centered bold (title lines are typically <center><b>..., and Glk
       * styles don't combine, so bold gets its own centered style).
       */
      glk_set_style (gsc_attribute_bold > 0 ? style_User2 : style_User1);
    }
  else
    {
      /*
       * For large and medium point sizes, use Header or Subheader styles
       * respectively.
       */
      if (font_size >= GSC_LARGE_FONT_SIZE)
        glk_set_style (style_Header);
      else if (font_size >= GSC_MEDIUM_FONT_SIZE)
        glk_set_style (style_Subheader);
      else
        {
          /*
           * For bold, use Subheader; for italics or underline, use Emphasized.
           * With colours disabled, <c> secondary-colour text keeps its old
           * Emphasized stand-in so it stays visually distinct; bold still
           * wins over it, as it always has.
           */
          if (gsc_attribute_bold > 0
              && (gsc_attribute_italic > 0 || gsc_attribute_underline > 0))
            glk_set_style (style_Alert);
          else if (gsc_attribute_bold > 0)
            glk_set_style (style_Subheader);
          else if (gsc_attribute_italic > 0
                   || gsc_attribute_underline > 0
                   || (gsc_attribute_secondary_colour > 0
                       && !gsc_colour_visible ()))
            glk_set_style (style_Emphasized);
          else
            {
              /*
               * There's nothing special about this text, so drop down to
               * Normal style.
               */
              glk_set_style (style_Normal);
            }
        }
    }
}


/*
 * gsc_handle_font_tag()
 * gsc_handle_endfont_tag()
 *
 * Push the settings of a font tag onto the font stack, and pop on end of
 * font tag.  Set the appropriate Glk style.
 */
static void
gsc_handle_font_tag (const scr_char *argument)
{
  /* Ignore the call on stack overrun. */
  if (gsc_font_index < GSC_MAX_STYLE_NESTING)
    {
      scr_char *lower, *face, *size;
      scr_int index_;

      /* Start from the current top of stack, or default on empty stack. */
      gsc_font_size_t font = gsc_font_top ();

      /* Copy and convert argument to all lowercase. */
      lower = (decltype(lower)) gsc_malloc (strlen (argument) + 1);
      memcpy (lower, argument, strlen (argument) + 1);
      for (index_ = 0; lower[index_] != '\0'; index_++)
        lower[index_] = glk_char_to_lower (lower[index_]);

      /* Find any face= portion of the tag argument. */
      face = strstr (lower, "face=");
      if (face)
        {
          /*
           * There may be plenty of monospaced fonts, but we do only courier
           * and terminal.
           */
          font.is_monospaced = strncmp (face, "face=\"courier\"", 14) == 0
                               || strncmp (face, "face=\"terminal\"", 15) == 0;

          /*
           * Symbol faces need their content translated to Unicode rather than
           * printed as text; see gsc_put_string_symbol().
           */
          font.symbol_font = gsc_symbol_font_from_face (face);
        }

      /*
       * Find any colour= portion of the tag argument.  Absent one the font
       * keeps the colour it inherited from the enclosing tag, which is what
       * the Runner does -- <font colour="red"><font size=20>x</font></font>
       * is red throughout.
       */
      {
        const glui32 colour = gsc_colour_from_tag (lower, "colour");
        if (colour != GSC_COLOUR_NONE)
          font.colour = colour;
      }

      /* Find the size= portion of the tag argument. */
      size = strstr (lower, "size=");
      if (size)
        {
          scr_uint value;

          /* Deal with incremental and absolute sizes. */
          if (strncmp (size, "size=+", 6) == 0
              && sscanf (size, "size=+%lu", &value) == 1)
            font.size += value;
          else if (strncmp (size, "size=-", 6) == 0
                   && sscanf (size, "size=-%lu", &value) == 1)
            font.size -= value;
          else if (sscanf (size, "size=%lu", &value) == 1)
            font.size = value;
        }

      /* Done with tag argument copy. */
      free (lower);

      /*
       * Push the new font setting onto the font stack, and set Glk style.
       */
      gsc_font_stack[gsc_font_index++] = font;
      gsc_set_glk_style ();
    }
}

static void
gsc_handle_endfont_tag (void)
{
  /* Unless underrun, pop the font stack and set Glk style. */
  if (gsc_font_index > 0)
    {
      gsc_font_index--;
      gsc_set_glk_style ();
    }
}


/*
 * gsc_handle_attribute_tag()
 *
 * Switch the required attribute on, or off on its end tag, and set the
 * appropriate Glk style.  Bold, italics and underline are flat states in the
 * Runner, one byte each in its display routine (run390 45BE84-45BF04), not
 * nesting depths: a second <i> changes nothing and one </i> ends both, so
 * the title "<i>Plains of Fantasy.<i>", closed with the wrong tag, is no
 * deeper in italics than one that was never closed at all.  The colour tag
 * keeps its count.
 */
static void
gsc_handle_attribute_tag (scr_int tag)
{
  switch (tag)
    {
    case SCR_TAG_BOLD:
      gsc_attribute_bold = 1;
      break;
    case SCR_TAG_ITALICS:
      gsc_attribute_italic = 1;
      break;
    case SCR_TAG_UNDERLINE:
      gsc_attribute_underline = 1;
      break;
    case SCR_TAG_COLOUR:
      gsc_attribute_secondary_colour++;
      break;
    default:
      break;
    }
  gsc_set_glk_style ();
}

static void
gsc_handle_endattribute_tag (scr_int tag)
{
  switch (tag)
    {
    case SCR_TAG_ENDBOLD:
      gsc_attribute_bold = 0;
      break;
    case SCR_TAG_ENDITALICS:
      gsc_attribute_italic = 0;
      break;
    case SCR_TAG_ENDUNDERLINE:
      gsc_attribute_underline = 0;
      break;
    case SCR_TAG_ENDCOLOUR:
      if (gsc_attribute_secondary_colour > 0)
        gsc_attribute_secondary_colour--;
      break;
    default:
      break;
    }
  gsc_set_glk_style ();
}


/*
 * gsc_handle_wait_tag()
 *
 * If Glk offers timers, delay for the requested period.  Otherwise, this
 * function does nothing.
 */
void
gsc_handle_wait_tag (const scr_char *argument)
{
  double delay = 0.0;

#ifdef SPATTERLIGHT
  /* Honour Spatterlight's delays preference: when it is off, timed waits are
     skipped outright.  Both engines funnel their pauses through here -- the
     ADRIFT 4 <wait x.x> tag and the ADRIFT 5 A5_WAIT_MARK span. */
  if (!gli_sa_delays)
    return;
#endif
  /* Ignore the wait tag if the Glk doesn't have timers. */
  if (!glk_gestalt (gestalt_Timer, 0))
    return;

  /* Determine the delay time, and convert to milliseconds. */
  if (sscanf (argument, "%lf", &delay) == 1 && delay > 0.0)
    {
      glui32 milliseconds, timeout;

      /*
       * Work with timeouts at 1/10 of the wait period, to minimize Glk
       * timer jitter.  Allow the timeout to be canceled by keypress, as a
       * user convenience.
       */
      milliseconds = (glui32) (delay * GSC_MILLISECONDS_PER_SECOND);
      timeout = milliseconds / GSC_TIMEOUTS_COUNT;
      if (timeout > 0)
        {
          glui32 delayed;
          scr_bool is_completed;

          /* Request timer events, and let a keypress cancel the wait. */
          glk_request_char_event (gsc_main_window);
          glk_request_timer_events (timeout);

          /* Loop until delay completed or canceled by a keypress. */
          is_completed = TRUE;
          for (delayed = 0; delayed < milliseconds; delayed += timeout)
            {
              event_t event;

              gsc_event_wait_2 (evtype_CharInput, evtype_Timer, &event);
              if (event.type == evtype_CharInput)
                {
                  /* Cancel the delay, or reissue the input request. */
                  if (event.val1 == GSC_CANCEL_WAIT_1
                      || event.val1 == GSC_CANCEL_WAIT_2)
                    {
                      is_completed = FALSE;
                      break;
                    }
                  else
                    glk_request_char_event (gsc_main_window);
                }
             }

          /* Cancel any pending character input, and stop timers. */
          if (is_completed)
            glk_cancel_char_event (gsc_main_window);
          glk_request_timer_events (0);
        }
    }
}


/*
 * gsc_reset_glk_style()
 *
 * Drop all stacked fonts and nested attributes, and return to normal Glk
 * style.
 */
void
gsc_reset_glk_style (void)
{
  /* Reset the font stack and attributes, and set a normal style.  Centering
     goes too: this runs at prompts, and a dangling <center> must not leave the
     prompt, the player's input, and every turn after it centered (the a5
     renderer drops a dangling <center> at the end of its block for the same
     reason). */
  gsc_font_index = 0;
  gsc_attribute_bold = 0;
  gsc_attribute_italic = 0;
  gsc_attribute_underline = 0;
  gsc_attribute_secondary_colour = 0;
  gsc_attribute_center = 0;
  gsc_attribute_right = 0;
  gsc_set_glk_style ();
}


#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
/*
 * Tracking for redrawing a graphic that an immediate screen clear would
 * otherwise wipe.  Adrift shows graphics in a separate pane that text clears
 * don't affect; this port draws them inline in the main window, so a graphic
 * drawn at the start of a turn is lost if that same turn's text then clears
 * the screen (as "examine girl" in Majesty Mall Center does).  We remember the
 * graphic drawn since the last player input and, on a clear, redraw it.
 */
static glui32 gsc_pending_graphic_id = 0;
scr_bool gsc_graphic_drawn_since_input = FALSE;

/*
 * gsc_draw_inline_graphic()
 *
 * Draw a graphic inline in the main window, on its own line -- break before it
 * if not already at the start of a line, and always after it so the following
 * text starts below the image rather than wrapping up alongside its tall line
 * fragment -- and note it for a possible redraw after a screen clear.
 */
void
gsc_draw_inline_graphic (glui32 id)
{
  strid_t stream = glk_window_get_stream (gsc_main_window);

  /* On the way back to a pause the picture is on screen already; only note
     it, for the clear that may yet come. */
  if (!gsc_sc_silent)
    {
      if (!gsc_main_at_line_start)
        glk_put_char_stream (stream, '\n');
      glk_image_draw (gsc_main_window, id, imagealign_InlineDown, 0);
      glk_put_char_stream (stream, '\n');
    }
  gsc_main_at_line_start = TRUE;

  gsc_pending_graphic_id = id;
  gsc_graphic_drawn_since_input = TRUE;
}
#endif


/*
 * gsc_handle_cls_tag()
 * gsc_set_alignment()
 * gsc_handle_center_tag()
 * gsc_handle_right_tag()
 * gsc_handle_bgcolour_tag()
 * gsc_handle_waitkey_tag()
 *
 * The os_print_tag() cases with a body of their own.
 */
static void
gsc_handle_cls_tag (void)
{
  /* Clear the main text display window. */
  if (!gsc_sc_silent)
    glk_window_clear (gsc_main_window);
  gsc_main_at_line_start = TRUE;
  gsc_main_window_empty = TRUE;
#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
  /*
   * If a graphic was drawn earlier this turn (no player input since), the
   * clear just wiped it.  Redraw it so it survives the clear, matching
   * Adrift's separate graphics pane.
   */
  if (gsc_graphic_drawn_since_input)
    gsc_draw_inline_graphic (gsc_pending_graphic_id);
#endif
}

static void
gsc_set_alignment (glui32 centered, glui32 right)
{
  /*
   * Alignment in the Runner is a flat state, not a nesting depth: it
   * keeps one byte holding centered, right or left, and <center> sets
   * centered while </center>, <right> and </right> set it away again
   * (run400 Sub_22_27 stores 2, 1 and 0 into it, with no count of how
   * many <center>s are open).  Counting nesting instead leaves a game
   * that opens <center> twice and closes it once -- as "To Hell in a
   * Hamper" does on its title page -- centered for the rest of the game.
   *
   * Justification is a paragraph attribute, so a change of alignment
   * needs its own paragraph: put the newline first -- it closes the
   * previous paragraph in that paragraph's own style (on ENDCENTER it is
   * the centered paragraph's terminator) -- and only then switch styles.
   * A tag that doesn't change the alignment breaks no paragraph.
   */
  if (centered != gsc_attribute_center || right != gsc_attribute_right)
    {
      if (!gsc_sc_silent)
        glk_put_char ('\n');
      gsc_attribute_center = centered;
      gsc_attribute_right = right;
      gsc_set_glk_style ();
    }
}

static void
gsc_handle_center_tag (scr_int tag)
{
  gsc_set_alignment (tag == SCR_TAG_CENTER, 0);
}

static void
gsc_handle_right_tag (scr_int tag)
{
  /*
   * Right uses style_Note (RightFlush-hinted); entering or leaving right
   * also clears centering, as the Runner does.
   */
  gsc_set_alignment (0, tag == SCR_TAG_RIGHT);
}

static void
gsc_handle_bgcolour_tag (const scr_char *argument)
{
  /*
   * <bgcolour="red"> repaints the Runner's output pane.  There is no Glk
   * way to change a window's background short of clearing it, and a game
   * that sets a background mid-turn is not asking for its text to be
   * wiped, so only the colour later text is drawn over changes; the
   * window catches up at the next clear.  The argument is the whole tag
   * contents (scprintf passes it verbatim, to match <font colour=...>),
   * so it is parsed the same way -- and "default" returns the mode's own
   * background rather than clearing the colour.
   *
   * The one repaint that is free is the one onto an empty window: a game
   * that clears the screen and then names its colour -- The Dead Man ends
   * each blackout vision with "<cls><bgcolor=default>" -- has nothing on
   * screen to lose, and without the second clear its pane keeps the vision's
   * white behind every line that follows.
   */
  const glui32 colour = gsc_colour_from_tag (argument, "bgcolour");
  const glui32 wanted = (colour == GSC_COLOUR_NONE) ? 0x000000 : colour;
  const scr_bool changed = (wanted != gsc_colour_background);

  gsc_colour_background = wanted;
  gsc_set_glk_style ();

  if (changed && gsc_colour_enabled && !gsc_sc_silent
      && gsc_main_window_empty && gsc_main_window)
    glk_window_clear (gsc_main_window);
}

static void
gsc_handle_waitkey_tag (void)
{
  event_t event;

  /*
   * If reading an input log, ignore; it disrupts replay.  Write a newline
   * to separate off any unterminated game output instead.
   */
  if (!gsc_readlog_stream)
    {
      strid_t stream;
#ifdef SPATTERLIGHT
      scr_bool resumed = FALSE;

      /* Running the turn again after a relaunch: go by the pauses the
         player had already been through, and stop at the one the game
         was closed at -- whose save is the one just restored. */
      gsc_sc_waitkey_count++;
      if (gsc_sc_silent)
        {
          if (gsc_sc_waitkey_count < gsc_sc_waitkey_skip)
            return;
          gsc_sc_silent = FALSE;
          gsc_sc_waitkey_skip = 0;
          resumed = TRUE;
        }
#endif

      /* Update the status line now only if it has its own window. */
      if (gsc_status_window)
        gsc_status_notify ();

      /* The turn that led here has already moved the player and marked
         the room seen; show that on the map now rather than at the next
         prompt, after the whole cutscene.  Opening the pane can move the
         current stream (gsc_map_show), so put it back. */
      stream = glk_stream_get_current ();
      gsc_map_redraw ();
      glk_stream_set_current (stream);

#ifdef SPATTERLIGHT
      /* Before the request, as at a prompt: the archived window then
         has none pending and the relaunch asks for the key itself. */
      if (!resumed)
        gsc_sc_autosave_waitkey ();
#endif

      /* Request a character event, and wait for it to be filled. */
      glk_request_char_event (gsc_main_window);
      gsc_event_wait (evtype_CharInput, &event);
    }
  else
    glk_put_char ('\n');
}


/*
 * os_print_tag()
 *
 * Interpret selected Adrift output control tags.  Not all are implemented
 * here; several are ignored.
 */
void
os_print_tag (scr_int tag, const scr_char *argument)
{
  assert (argument);

  switch (tag)
    {
    case SCR_TAG_CLS:
      gsc_handle_cls_tag ();
      break;

    case SCR_TAG_FONT:
      /* Handle with specific tag handler function. */
      gsc_handle_font_tag (argument);
      break;

    case SCR_TAG_ENDFONT:
      /* Handle with specific endtag handler function. */
      gsc_handle_endfont_tag ();
      break;

    case SCR_TAG_BOLD:
    case SCR_TAG_ITALICS:
    case SCR_TAG_UNDERLINE:
    case SCR_TAG_COLOUR:
      /* Handle with common attribute tag handler function. */
      gsc_handle_attribute_tag (tag);
      break;

    case SCR_TAG_ENDBOLD:
    case SCR_TAG_ENDITALICS:
    case SCR_TAG_ENDUNDERLINE:
    case SCR_TAG_ENDCOLOUR:
      /* Handle with common attribute endtag handler function. */
      gsc_handle_endattribute_tag (tag);
      break;

    case SCR_TAG_CENTER:
    case SCR_TAG_ENDCENTER:
      gsc_handle_center_tag (tag);
      break;

    case SCR_TAG_RIGHT:
    case SCR_TAG_ENDRIGHT:
      gsc_handle_right_tag (tag);
      break;

    case SCR_TAG_BGCOLOUR:
      gsc_handle_bgcolour_tag (argument);
      break;

    case SCR_TAG_WAIT:
      /*
       * Update the status line now only if it has its own window, then
       * handle with a specialized handler.
       */
      if (gsc_sc_silent)
        break;
      if (gsc_status_window)
        gsc_status_notify ();
      gsc_handle_wait_tag (argument);
      break;

    case SCR_TAG_WAITKEY:
      gsc_handle_waitkey_tag ();
      break;

    default:
      /* Ignore unimplemented and unknown tags. */
      break;
    }
}


/*
 * os_print_string()
 *
 * Print a text string to the main output window.
 */
void
os_print_string (const scr_char *string)
{
  assert (string);
  assert (glk_stream_get_current ());

  /* Already on the restored screen: see gsc_sc_silent. */
  if (gsc_sc_silent)
    {
      if (string[0] != '\0')
        {
          gsc_main_at_line_start = (string[strlen (string) - 1] == '\n');
          gsc_main_window_empty = FALSE;
        }
      return;
    }

  /* The first output of a replayed opening is where a restart becomes visible
     to the front end; take the map pane down before the text is laid out. */
  gsc_map_notice_restart ();

  /*
   * If the current top of the font stack is monospaced, we may need to use an
   * alternative function to write this string.
   *
   * The main window should always be the currently set window at this point,
   * so we never be attempting monospaced output to the status window.
   * Nevertheless, check anyway.
   */
  if (gsc_font_top ().symbol_font != GSC_SYMBOL_NONE
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_put_string_symbol (string, gsc_font_top ().symbol_font);
  else if (gsc_font_top ().is_monospaced
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_put_string_alternate (string);
  else
    gsc_put_string (string);
}


/*
 * os_print_string_debug()
 *
 * Debugging output goes to the main Glk window -- no special effects or
 * dedicated debugging window attempted.
 */
void
os_print_string_debug (const scr_char *string)
{
  assert (string);
  assert (glk_stream_get_current ());

  gsc_put_string (string);
}


/*
 * gsc_styled_string()
 * gsc_styled_char()
 * gsc_standout_string()
 * gsc_standout_char()
 * gsc_normal_string()
 * gsc_normal_char()
 * gsc_header_string()
 *
 * Convenience functions to print strings in assorted styles.  A standout
 * string is one that hints that it's from the interpreter, not the game.
 */
static void
gsc_styled_string (glui32 style, const char *message)
{
  assert (message);

  glk_set_style (style);
  glk_put_string ((char *) message);
  glk_set_style (style_Normal);
}

static void
gsc_styled_char (glui32 style, char c)
{
  char buffer[2];

  buffer[0] = c;
  buffer[1] = '\0';
  gsc_styled_string (style, buffer);
}

void
gsc_standout_string (const char *message)
{
  gsc_styled_string (style_Emphasized, message);
}

void
gsc_standout_char (char c)
{
  gsc_styled_char (style_Emphasized, c);
}

void
gsc_normal_string (const char *message)
{
  gsc_styled_string (style_Normal, message);
}

void
gsc_normal_char (char c)
{
  gsc_styled_char (style_Normal, c);
}

void
gsc_header_string (const char *message)
{
  gsc_styled_string (style_Header, message);
}


/*
 * gsc_report_fatal()
 *
 * Tell the player why an engine call just failed, if it was a fatal engine
 * error -- those are otherwise only written to stderr, which a player never
 * sees.  With `stopping` the game cannot go on and the report says so.
 * Returns TRUE if there was an error to report.
 *
 * Spatterlight has an error channel of its own, which shows an alert or
 * keeps quiet as the player's error-handling setting asks.  Set to ignore
 * errors it shows the last one only if the interpreter then fails, so an
 * error that stops the game exits here with a failure status.  Any other Glk
 * library gets the message in the main window.
 */
scr_bool
gsc_report_fatal (scr_bool stopping)
{
  const scr_char *const message = scr_take_fatal_message ();

  if (!message)
    return FALSE;

#ifdef SPATTERLIGHT
  win_showerror ((std::string ("Scarier internal error: ") + message).c_str ());
  if (stopping && gli_error_handling == IGNORE_ERRORS)
    {
      win_flush ();
      exit (EXIT_FAILURE);
    }
#else
  if (!gsc_main_window)
    return TRUE;

  glk_set_window (gsc_main_window);
  gsc_normal_string (gsc_main_at_line_start ? "\n" : "\n\n");
  gsc_header_string ("Scarier internal error");
  gsc_normal_string (": ");
  gsc_normal_string (message);
  gsc_normal_string (stopping ? "\n\nThe game cannot continue.\n" : "\n");
#endif
  return TRUE;
}


/*
 * gsc_copy_string()
 *
 * strdup() that passes NULL through rather than crashing on it; hint strings
 * are NULL when the game leaves them empty.
 */
char *
gsc_copy_string (const char *string)
{
  return string ? strdup (string) : nullptr;
}


/*
 * gsc_hint_present()
 *
 * Offer one hint: pop the question, then each of its two answers behind its
 * own confirm, the subtle nudge before the sledgehammer.  Returns FALSE when
 * the player turned an answer down, which is what the callers' refusal count
 * measures; turning the subtle answer down skips this hint's sledgehammer too.
 *
 * Both engines' hint displays funnel through here, so the ADRIFT 4 hints
 * (task Question/Hint1/Hint2, reached by the game's own "hints" command) and
 * the ADRIFT 5 ones (clsHint, reached by "glk hints") cannot drift apart.
 *
 * All three strings must stay valid for the whole call; see os_display_hints
 * for why that is not free on the ADRIFT 4 side.
 */
int
gsc_hint_present (const char *question, const char *subtle,
                  const char *unsubtle)
{
  gsc_normal_char ('\n');
  gsc_standout_string (question);
  gsc_normal_char ('\n');

  if (subtle)
    {
      if (!os_confirm (GSC_CONF_SUBTLE_HINT))
        return FALSE;
      gsc_normal_char ('\n');
      gsc_standout_string (subtle);
      gsc_normal_string ("\n\n");
    }

  if (unsubtle)
    {
      if (!os_confirm (GSC_CONF_UNSUBTLE_HINT))
        return FALSE;
      gsc_normal_char ('\n');
      gsc_standout_string (unsubtle);
      gsc_normal_string ("\n\n");
    }

  return TRUE;
}


/*
 * os_display_hints()
 *
 * This is a very basic hints display.  In mitigation, very few games use
 * hints at all, and those that do are usually sparse in what they hint at, so
 * it's sort of good enough for the moment.
 */
void
os_display_hints (scr_game game)
{
  scr_game_hint hint;
  scr_int refused;

  /* For each hint, print the question, and confirm hint display. */
  refused = 0;
  for (hint = scr_get_first_game_hint (game);
       hint; hint = scr_get_next_game_hint (game, hint))
    {
      const scr_char *unsubtle;
      char *question, *subtle;

      /* If enough refusals, offer a way out of the loop. */
      if (refused >= GSC_HINT_REFUSAL_LIMIT)
        {
          if (!os_confirm (GSC_CONF_CONTINUE_HINTS))
            break;
          refused = 0;
        }

      /* All three getters hand back the same buffer, refilled -- SCARE's
         run_get_hint_common() notes its return is "valid only until the next
         hint call" -- so the first two have to be copied before the fetch
         that overwrites them.  Print without copying and the question comes
         out as whatever the sledgehammer left behind.  The last fetch has
         nothing after it, so it can be used where it lies. */
      question = gsc_copy_string (scr_get_game_hint_question (game, hint));
      subtle = gsc_copy_string (scr_get_game_subtle_hint (game, hint));
      unsubtle = scr_get_game_unsubtle_hint (game, hint);

      if (!gsc_hint_present (question, subtle, unsubtle))
        refused++;

      free (question);
      free (subtle);
    }
}

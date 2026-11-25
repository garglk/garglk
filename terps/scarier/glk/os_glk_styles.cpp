/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

/*
 * os_glk_styles.cpp: the style census and assignment; see os_glk_styles.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>

#include "os_glk_styles.h"
#include "adrift5/a5xml.h"

/* The deepest <font> nesting the frontend tracks, as in os_glk_internal.h. */
static const int GSC_STYLE_MAX_NESTING = 32;

/* The property path of the introduction. */
static const char *const GSC_STYLE_INTRO_PATH = "Header/StartupText";

/*
 * The game name, which the engine prints above the introduction wrapped in
 * markup of its own (run_main_intro(): "<font size=14><c>" ... "</c></font>").
 * The census wraps it the same way, so the title's state is counted as the
 * introduction text it is.
 */
static const char *const GSC_STYLE_TITLE_PATH = "Globals/GameName";


/*
 * gsc_style_combo()
 * gsc_style_combo_step()
 * gsc_style_combo_describe()
 *
 * Build an attribute state, read its size step back, and spell it out for a
 * report ("bold+centre size+3").
 */
gsc_style_combo_t
gsc_style_combo (scr_bool bold, scr_bool italic, scr_bool monospaced,
                 scr_bool center, scr_bool right, scr_bool standin,
                 scr_int points)
{
  gsc_style_combo_t combo = 0;
  scr_int step;

  if (bold)
    combo |= GSC_COMBO_BOLD;
  if (italic)
    combo |= GSC_COMBO_ITALIC;
  if (monospaced)
    combo |= GSC_COMBO_MONOSPACED;

  /* Alignment is one state in the frontend; right is what it says last. */
  if (right)
    combo |= GSC_COMBO_RIGHT;
  else if (center)
    combo |= GSC_COMBO_CENTER;

  /*
   * Two points to a step, truncating toward the default, so 13pt is still
   * the default size and 11pt is too; the clamp is the range a Size hint
   * can sensibly ask for.
   */
  step = (points - GSC_STYLE_DEFAULT_POINTS) / 2;
  step = std::max<scr_int> (GSC_STYLE_MIN_STEP,
                            std::min<scr_int> (GSC_STYLE_MAX_STEP, step));
  combo |= ((gsc_style_combo_t) step << GSC_COMBO_SIZE_SHIFT)
           & GSC_COMBO_SIZE_MASK;

  /* <c> with the colours off is Emphasized, but only on plain text. */
  if (standin && combo == 0)
    combo = GSC_COMBO_ITALIC;

  return combo;
}

scr_int
gsc_style_combo_step (gsc_style_combo_t combo)
{
  return (signed char) ((combo & GSC_COMBO_SIZE_MASK) >> GSC_COMBO_SIZE_SHIFT);
}

void
gsc_style_combo_describe (gsc_style_combo_t combo, char *buffer, size_t length)
{
  static const struct
  {
    gsc_style_combo_t bit;
    const char *name;
  } names[] = {
    {GSC_COMBO_BOLD, "bold"}, {GSC_COMBO_ITALIC, "italic"},
    {GSC_COMBO_MONOSPACED, "mono"}, {GSC_COMBO_CENTER, "centre"},
    {GSC_COMBO_RIGHT, "right"}
  };
  const scr_int step = gsc_style_combo_step (combo);
  size_t used = 0;
  size_t index_;

  buffer[0] = '\0';
  for (index_ = 0; index_ < sizeof (names) / sizeof (names[0]); index_++)
    {
      if (combo & names[index_].bit)
        used += snprintf (buffer + used, length > used ? length - used : 0,
                          "%s%s", used > 0 ? "+" : "", names[index_].name);
    }
  if (step != 0)
    used += snprintf (buffer + used, length > used ? length - used : 0,
                      "%ssize%+ld", used > 0 ? " " : "", step);
  if (used == 0)
    snprintf (buffer, length, "plain");
}


/*
 * gsc_style_face_is_monospaced()
 * gsc_style_apply_font_size()
 *
 * The face= and size= parts of a lowercased <font> argument.  There may be
 * plenty of monospaced fonts, but we do only courier and terminal.  Sizes are
 * absolute, or relative to the enclosing font with a sign.
 */
scr_bool
gsc_style_face_is_monospaced (const char *face)
{
  return strncmp (face, "face=\"courier\"", 14) == 0
         || strncmp (face, "face=\"terminal\"", 15) == 0;
}

scr_int
gsc_style_apply_font_size (const char *size, scr_int points)
{
  scr_uint value;

  if (strncmp (size, "size=+", 6) == 0
      && sscanf (size, "size=+%lu", &value) == 1)
    return points + value;
  else if (strncmp (size, "size=-", 6) == 0
           && sscanf (size, "size=-%lu", &value) == 1)
    return points - value;
  else if (sscanf (size, "size=%lu", &value) == 1)
    return value;
  return points;
}


/*
 * gsc_style_table_natural()
 * gsc_style_lookup()
 * gsc_style_name()
 *
 * The assignment today's fixed precedence amounts to, the print-time lookup
 * with its fallback chain, and the styles' names for reports.
 */
void
gsc_style_table_natural (gsc_style_table_t *table)
{
  static const struct
  {
    scr_int style;
    gsc_style_combo_t combo;
  } naturals[] = {
    {GSC_STYLE_NORMAL, 0},
    {GSC_STYLE_EMPHASIZED, GSC_COMBO_ITALIC},
    {GSC_STYLE_PREFORMATTED, GSC_COMBO_MONOSPACED},
    {GSC_STYLE_SUBHEADER, GSC_COMBO_BOLD},
    {GSC_STYLE_ALERT, GSC_COMBO_BOLD | GSC_COMBO_ITALIC},
    {GSC_STYLE_NOTE, GSC_COMBO_RIGHT},
    {GSC_STYLE_USER1, GSC_COMBO_CENTER},
    {GSC_STYLE_USER2, GSC_COMBO_CENTER | GSC_COMBO_BOLD}
  };
  size_t index_;

  for (index_ = 0; index_ < GSC_STYLE_COUNT; index_++)
    {
      table->assigned[index_] = FALSE;
      table->natural[index_] = TRUE;
      table->combo[index_] = 0;
    }
  for (index_ = 0; index_ < sizeof (naturals) / sizeof (naturals[0]); index_++)
    {
      table->assigned[naturals[index_].style] = TRUE;
      table->combo[naturals[index_].style] = naturals[index_].combo;
    }
}

static scr_int
gsc_style_find (const gsc_style_table_t *table, gsc_style_combo_t combo)
{
  scr_int style;

  for (style = 0; style < GSC_STYLE_COUNT; style++)
    {
      if (table->assigned[style] && table->combo[style] == combo)
        return style;
    }
  return -1;
}

scr_int
gsc_style_lookup (const gsc_style_table_t *table, gsc_style_combo_t combo)
{
  /*
   * Each try drops attributes and none adds one, so text never shows an
   * attribute it didn't ask for.  The tries go by what they cost: italic and
   * bold one each, then the alignment, worth more than both together since
   * losing it moves the text where losing a slant or weight only restyles
   * it, then the face, worth more than all of those.  On a tie italic goes
   * before bold (so right-aligned bold stays right-aligned, as it is today).
   * So centred bold italic with no style of its own lands on centred text
   * before on bold italic, and centred italic with no centred style lands on
   * italic, not on Normal.  Under each set of attributes the size goes a step
   * at a time toward the default, so a size the game uses a little of lands
   * on a near size it uses more.  Normal is always assigned, so the walk
   * ends there at the latest.
   */
  static const struct
  {
    gsc_style_combo_t flags;
    unsigned int cost;
  } drops[] = {
    {GSC_COMBO_ITALIC, 1}, {GSC_COMBO_BOLD, 1},
    {GSC_COMBO_CENTER | GSC_COMBO_RIGHT, 3}, {GSC_COMBO_MONOSPACED, 6}
  };
  static const size_t DROPS = sizeof (drops) / sizeof (drops[0]);
  static const unsigned int DROP_SETS = 1u << DROPS, MAX_COST = 11;
  const gsc_style_combo_t flags = combo & GSC_COMBO_FLAGS;
  const scr_int size = gsc_style_combo_step (combo);
  unsigned int cost, set;
  scr_int step, style;
  size_t index_;

  for (cost = 0; cost <= MAX_COST; cost++)
    {
      for (set = 0; set < DROP_SETS; set++)
        {
          gsc_style_combo_t kept = flags;
          unsigned int set_cost = 0;

          for (index_ = 0; index_ < DROPS; index_++)
            {
              if (set & (1u << index_))
                {
                  kept &= ~drops[index_].flags;
                  set_cost += drops[index_].cost;
                }
            }
          if (set_cost != cost)
            continue;

          for (step = size; ; step += (step < 0) - (step > 0))
            {
              style = gsc_style_find (table, kept
                                      | (((gsc_style_combo_t) step
                                          << GSC_COMBO_SIZE_SHIFT)
                                         & GSC_COMBO_SIZE_MASK));
              if (style >= 0)
                return style;
              if (step == 0)
                break;
            }
        }
    }
  return GSC_STYLE_NORMAL;
}

const char *
gsc_style_name (scr_int style)
{
  static const char *const names[GSC_STYLE_COUNT] = {
    "Normal", "Emphasized", "Preformatted", "Header", "Subheader", "Alert",
    "Note", "BlockQuote", "Input", "User1", "User2"
  };

  return style >= 0 && style < GSC_STYLE_COUNT ? names[style] : "?";
}


/*
 * gsc_style_census_a4()
 *
 * Feed every string in an ADRIFT 4 game through a copy of the frontend's tag
 * state (os_glk_output.cpp, gsc_handle_font_tag() and friends): flat bold,
 * italic and underline flags, a counted <c>, a font stack of the same depth,
 * and one alignment.  Each string starts from the plain state the frontend
 * resets to at every prompt.  Text is counted in visible characters, so one
 * big title weighs what its words do rather than what its tags do.
 */
namespace {

struct gsc_style_walk_t
{
  struct font_t
  {
    scr_bool monospaced;
    scr_int points;
  };

  font_t fonts[GSC_STYLE_MAX_NESTING];
  int font_count;
  scr_bool bold, italic, underline, center, right;
  int colour;

  scr_bool in_intro;
  std::map<gsc_style_combo_t, gsc_style_census_row_t> rows;
  unsigned long strings;

  void reset ();
  font_t top () const;
  gsc_style_combo_t combo () const;
  void text (const scr_char *string);
  void tag (scr_int tag, const scr_char *argument);
};

} /* namespace */

void
gsc_style_walk_t::reset ()
{
  font_count = 0;
  bold = italic = underline = center = right = FALSE;
  colour = 0;
}

gsc_style_walk_t::font_t
gsc_style_walk_t::top () const
{
  if (font_count > 0)
    return fonts[font_count - 1];

  font_t font = {FALSE, GSC_STYLE_DEFAULT_POINTS};
  return font;
}

gsc_style_combo_t
gsc_style_walk_t::combo () const
{
  const font_t font = top ();

  /* The census takes the colours as off, the case where <c> has a style. */
  return gsc_style_combo (bold, italic || underline, font.monospaced,
                          center, right, colour > 0, font.points);
}

void
gsc_style_walk_t::text (const scr_char *string)
{
  unsigned long visible = 0;
  const scr_char *cursor;

  for (cursor = string; *cursor != '\0'; cursor++)
    {
      if (*cursor != ' ' && *cursor != '\t'
          && *cursor != '\n' && *cursor != '\r')
        visible++;
    }
  if (visible == 0)
    return;

  const gsc_style_combo_t current = combo ();
  std::map<gsc_style_combo_t, gsc_style_census_row_t>::iterator
      row = rows.find (current);
  if (row == rows.end ())
    {
      gsc_style_census_row_t fresh = {current, 0, 0};
      row = rows.insert (std::make_pair (current, fresh)).first;
    }

  row->second.characters += visible;
  if (in_intro)
    row->second.intro_characters += visible;
}

void
gsc_style_walk_t::tag (scr_int tag, const scr_char *argument)
{
  switch (tag)
    {
    case SCR_TAG_FONT:
      /* Ignored on overrun, as the frontend ignores it. */
      if (font_count < GSC_STYLE_MAX_NESTING)
        {
          font_t font = top ();
          std::string lower (argument);
          size_t index_;

          for (index_ = 0; index_ < lower.size (); index_++)
            lower[index_] = (char) tolower ((unsigned char) lower[index_]);

          const size_t face = lower.find ("face=");
          if (face != std::string::npos)
            font.monospaced = gsc_style_face_is_monospaced (lower.c_str ()
                                                            + face);
          const size_t size = lower.find ("size=");
          if (size != std::string::npos)
            font.points = gsc_style_apply_font_size (lower.c_str () + size,
                                                     font.points);
          fonts[font_count++] = font;
        }
      break;

    case SCR_TAG_ENDFONT:
      if (font_count > 0)
        font_count--;
      break;

    case SCR_TAG_BOLD:
      bold = TRUE;
      break;
    case SCR_TAG_ENDBOLD:
      bold = FALSE;
      break;
    case SCR_TAG_ITALICS:
      italic = TRUE;
      break;
    case SCR_TAG_ENDITALICS:
      italic = FALSE;
      break;
    case SCR_TAG_UNDERLINE:
      underline = TRUE;
      break;
    case SCR_TAG_ENDUNDERLINE:
      underline = FALSE;
      break;
    case SCR_TAG_COLOUR:
      colour++;
      break;
    case SCR_TAG_ENDCOLOUR:
      if (colour > 0)
        colour--;
      break;

    /* One alignment, as in gsc_set_alignment(). */
    case SCR_TAG_CENTER:
    case SCR_TAG_ENDCENTER:
      center = (tag == SCR_TAG_CENTER);
      right = FALSE;
      break;
    case SCR_TAG_RIGHT:
    case SCR_TAG_ENDRIGHT:
      right = (tag == SCR_TAG_RIGHT);
      center = FALSE;
      break;

    default:
      break;
    }
}

static void
gsc_style_walk_begin (const scr_char *path, void *opaque)
{
  gsc_style_walk_t *walk = (gsc_style_walk_t *) opaque;

  walk->reset ();
  walk->in_intro = strcmp (path, GSC_STYLE_INTRO_PATH) == 0;
  walk->strings++;

  if (strcmp (path, GSC_STYLE_TITLE_PATH) == 0)
    {
      walk->in_intro = TRUE;
      walk->tag (SCR_TAG_FONT, "size=14");
      walk->tag (SCR_TAG_COLOUR, "");
    }
}

static void
gsc_style_walk_text (const scr_char *text, void *opaque)
{
  ((gsc_style_walk_t *) opaque)->text (text);
}

static void
gsc_style_walk_tag (scr_int tag, const scr_char *argument, void *opaque)
{
  ((gsc_style_walk_t *) opaque)->tag (tag, argument);
}

/*
 * The order the assignment hands out styles in: by how much text a state
 * covers, the simpler state first on a tie, and the combo value last so the
 * order never depends on the map.  Introduction text counts many times over,
 * it being the first screen the player sees and its title states being the
 * reason game styles exist at all; but not so many that a ten-character
 * byline outranks a state that covers whole rooms.  Across the corpus, ranking
 * the introduction strictly first leaves twice the text without a style of
 * its own that this weight does.
 */
static const unsigned long GSC_STYLE_INTRO_WEIGHT = 20;

static unsigned long
gsc_style_row_weight (const gsc_style_census_row_t &row)
{
  return row.characters + GSC_STYLE_INTRO_WEIGHT * row.intro_characters;
}

static unsigned int
gsc_style_combo_bits (gsc_style_combo_t combo)
{
  unsigned int bits = gsc_style_combo_step (combo) != 0;

  for (combo &= GSC_COMBO_FLAGS; combo != 0; combo &= combo - 1)
    bits++;
  return bits;
}

static bool
gsc_style_rank_before (const gsc_style_census_row_t &a,
                       const gsc_style_census_row_t &b)
{
  const unsigned long a_weight = gsc_style_row_weight (a);
  const unsigned long b_weight = gsc_style_row_weight (b);
  const unsigned int a_bits = gsc_style_combo_bits (a.combo);
  const unsigned int b_bits = gsc_style_combo_bits (b.combo);

  if (a_weight != b_weight)
    return a_weight > b_weight;
  if (a_bits != b_bits)
    return a_bits < b_bits;
  return a.combo < b.combo;
}

static void
gsc_style_census_finish (const gsc_style_walk_t &walk,
                         gsc_style_census_t *census)
{
  std::map<gsc_style_combo_t, gsc_style_census_row_t>::const_iterator row;

  census->rows.clear ();
  for (row = walk.rows.begin (); row != walk.rows.end (); ++row)
    census->rows.push_back (row->second);
  std::sort (census->rows.begin (), census->rows.end (),
             gsc_style_rank_before);
  census->strings = walk.strings;
}

void
gsc_style_census_a4 (scr_game game, gsc_style_census_t *census)
{
  gsc_style_walk_t walk;
  scr_markup_sink_t sink;

  walk.reset ();
  walk.in_intro = FALSE;
  walk.strings = 0;

  sink.begin = gsc_style_walk_begin;
  sink.text = gsc_style_walk_text;
  sink.tag = gsc_style_walk_tag;
  sink.opaque = &walk;
  scr_game_scan_markup (game, &sink);
  gsc_style_census_finish (walk, census);
}


/*
 * gsc_style_census_a5()
 *
 * The same census over an ADRIFT 5 adventure: every <Text> of every
 * description in the file, each a string of its own (each is its own display
 * commit, which ends its spans), those under the <Introduction> counting as
 * introduction text.  So does the title, which the Runner prints as a <c>
 * span ahead of the introduction.  The tags are the ones a5text_render.cpp
 * turns into span marks; text inside expressions and functions is counted as
 * it is written, there being no state to evaluate it in.
 */
static void
gsc_style_walk_a5_string (gsc_style_walk_t *walk, const char *string,
                          scr_bool in_intro)
{
  std::string run;
  const char *cursor = string;

  walk->reset ();
  walk->in_intro = in_intro;
  walk->strings++;
  while (*cursor != '\0')
    {
      if (*cursor != '<')
        {
          run.push_back (*cursor++);
          continue;
        }

      const char *end = strchr (cursor, '>');
      if (end == nullptr)
        break;
      walk->text (run.c_str ());
      run.clear ();

      std::string tag (cursor + 1, end);
      size_t index_;
      for (index_ = 0; index_ < tag.size (); index_++)
        tag[index_] = (char) tolower ((unsigned char) tag[index_]);
      const size_t space = tag.find (' ');
      const std::string name = tag.substr (0, space);
      const std::string argument = space == std::string::npos
                                   ? std::string () : tag.substr (space + 1);

      static const struct
      {
        const char *name;
        scr_int tag;
      } tags[] = {
        {"b", SCR_TAG_BOLD}, {"/b", SCR_TAG_ENDBOLD},
        {"i", SCR_TAG_ITALICS}, {"/i", SCR_TAG_ENDITALICS},
        {"u", SCR_TAG_UNDERLINE}, {"/u", SCR_TAG_ENDUNDERLINE},
        {"c", SCR_TAG_COLOUR}, {"/c", SCR_TAG_ENDCOLOUR},
        {"font", SCR_TAG_FONT}, {"/font", SCR_TAG_ENDFONT},
        {"center", SCR_TAG_CENTER}, {"/center", SCR_TAG_ENDCENTER},
        {"centre", SCR_TAG_CENTER}, {"/centre", SCR_TAG_ENDCENTER},
        {"right", SCR_TAG_RIGHT}, {"/right", SCR_TAG_ENDRIGHT},
      };
      for (index_ = 0; index_ < sizeof (tags) / sizeof (tags[0]); index_++)
        {
          if (name == tags[index_].name)
            walk->tag (tags[index_].tag, argument.c_str ());
        }
      cursor = end + 1;
    }
  walk->text (run.c_str ());
}

static void
gsc_style_walk_a5_tree (gsc_style_walk_t *walk, const a5_xml_node_t *node,
                        const a5_xml_node_t *introduction, scr_bool in_intro)
{
  const a5_xml_node_t *child;

  in_intro = in_intro || node == introduction;
  if (strcmp (node->name, "Text") == 0 && node->text[0] != '\0')
    gsc_style_walk_a5_string (walk, node->text, in_intro);
  for (child = node->first_child; child != nullptr; child = child->next)
    gsc_style_walk_a5_tree (walk, child, introduction, in_intro);
}

void
gsc_style_census_a5 (const a5_xml_node_t *root,
                     const a5_xml_node_t *introduction, const char *title,
                     gsc_style_census_t *census)
{
  gsc_style_walk_t walk;

  walk.strings = 0;
  if (title != nullptr && title[0] != '\0')
    gsc_style_walk_a5_string (&walk, (std::string ("<c>") + title
                                      + "</c>").c_str (), TRUE);
  if (root != nullptr)
    gsc_style_walk_a5_tree (&walk, root, introduction, FALSE);
  gsc_style_census_finish (walk, census);
}


/* How many times its weight aligned text counts against an evicted state. */
static const unsigned long GSC_STYLE_ALIGNMENT_WEIGHT = 4;

/*
 * gsc_style_default_look()
 * gsc_style_spare_mismatch()
 *
 * What a frontend that ignores hints draws a style in: Spatterlight with
 * game styles off and Gargoyle's defaults agree that Header and Subheader
 * are bold, Alert bold italic and the rest plain.  Size and alignment are
 * lost either way.  The mismatch is how far a state drawn in a spare's
 * default look is from itself; an attribute the look adds costs more than
 * one it loses, as the fallback chain never adds one either.  A state
 * above the default size that isn't bold wants a bold look all the same:
 * drawn without its size, bigger text reads closest as bold, and the title
 * every ADRIFT 4 game prints as 14pt text then keeps the bold it has always
 * had where the hints don't apply.
 */
static gsc_style_combo_t
gsc_style_default_look (scr_int style)
{
  switch (style)
    {
    case GSC_STYLE_HEADER:
    case GSC_STYLE_SUBHEADER:
      return GSC_COMBO_BOLD;
    case GSC_STYLE_ALERT:
      return GSC_COMBO_BOLD | GSC_COMBO_ITALIC;
    default:
      return 0;
    }
}

static unsigned int
gsc_style_spare_mismatch (scr_int style, gsc_style_combo_t combo)
{
  const gsc_style_combo_t look = gsc_style_default_look (style);
  gsc_style_combo_t want = combo & (GSC_COMBO_BOLD | GSC_COMBO_ITALIC);

  if (gsc_style_combo_step (combo) > 0)
    want |= GSC_COMBO_BOLD;
  return 2 * gsc_style_combo_bits (look & ~want)
         + gsc_style_combo_bits (want & ~look);
}


/*
 * gsc_style_assign()
 *
 * Start from the natural table, then give each ranked state that has no
 * style of its own a spare: Header and BlockQuote, which today's precedence
 * leaves to error text or doesn't use, and the natural styles whose state
 * the game never shows.  Emphasized and Preformatted are never spares -- the
 * frontend prints its own notes in Emphasized -- and Normal and Input are
 * what they are.
 *
 * Each state takes the spare whose default look is nearest its own, so a
 * frontend that ignores the hints still draws a bold title bold (Header or
 * Subheader) and a plain one plain (BlockQuote, User1...), losing only the
 * centring and the size.  Ties go in the order above.
 *
 * Handing out User1, User2 or Note can leave an alignment the game uses
 * with no style at all -- a game whose centred text is all italic or sized
 * shows no plain centre, so User1 goes to some size and its centred text
 * then falls back to unaligned -- and losing the alignment moves text where
 * losing a size or slant only restyles it.  So when that happens, the last
 * state handed a spare gives it up to the plain alignment, which every
 * aligned state can fall back to -- provided the aligned text weighs at
 * least a quarter of that state's, so a few right-aligned words don't cost
 * a sized body text its style.
 */
void
gsc_style_assign (const gsc_style_census_t *census, gsc_style_table_t *table)
{
  static const scr_int fixed_spares[] = {GSC_STYLE_HEADER,
                                         GSC_STYLE_BLOCKQUOTE};
  static const scr_int natural_spares[] = {GSC_STYLE_USER1, GSC_STYLE_USER2,
                                           GSC_STYLE_NOTE, GSC_STYLE_ALERT,
                                           GSC_STYLE_SUBHEADER};
  static const struct
  {
    gsc_style_combo_t combo;
    scr_int style;
  } alignments[] = {{GSC_COMBO_CENTER, GSC_STYLE_USER1},
                    {GSC_COMBO_RIGHT, GSC_STYLE_NOTE}};
  std::vector<scr_int> pool, handed;
  size_t index_;

  gsc_style_table_natural (table);

  pool.assign (fixed_spares,
               fixed_spares + sizeof (fixed_spares) / sizeof (fixed_spares[0]));
  for (index_ = 0;
       index_ < sizeof (natural_spares) / sizeof (natural_spares[0]); index_++)
    {
      const scr_int style = natural_spares[index_];
      std::vector<gsc_style_census_row_t>::const_iterator row;
      scr_bool used = FALSE;

      for (row = census->rows.begin (); row != census->rows.end (); ++row)
        {
          if (row->combo == table->combo[style])
            used = TRUE;
        }
      if (!used)
        pool.push_back (style);
    }

  /* The spares handed out, in order. */
  for (index_ = 0; index_ < census->rows.size () && !pool.empty (); index_++)
    {
      const gsc_style_combo_t combo = census->rows[index_].combo;
      size_t best, candidate;
      scr_int style;

      if (gsc_style_find (table, combo) >= 0)
        continue;

      best = 0;
      for (candidate = 1; candidate < pool.size (); candidate++)
        {
          if (gsc_style_spare_mismatch (pool[candidate], combo)
              < gsc_style_spare_mismatch (pool[best], combo))
            best = candidate;
        }
      style = pool[best];
      pool.erase (pool.begin () + (long) best);
      handed.push_back (style);
      table->assigned[style] = TRUE;
      table->natural[style] = FALSE;
      table->combo[style] = combo;
    }

  for (index_ = 0; index_ < sizeof (alignments) / sizeof (alignments[0]);
       index_++)
    {
      const gsc_style_combo_t alignment = alignments[index_].combo;
      const scr_int home = alignments[index_].style;
      std::vector<gsc_style_census_row_t>::const_iterator row;
      std::vector<scr_int>::iterator earlier;
      unsigned long aligned = 0, evicted = 0;
      scr_int style;

      if (handed.empty ())
        break;
      for (style = 0; style < GSC_STYLE_COUNT; style++)
        {
          if (table->assigned[style] && (table->combo[style] & alignment))
            break;
        }
      if (style < GSC_STYLE_COUNT)
        continue;
      for (row = census->rows.begin (); row != census->rows.end (); ++row)
        {
          if (row->combo & alignment)
            aligned += gsc_style_row_weight (*row);
          if (row->combo == table->combo[handed.back ()])
            evicted = gsc_style_row_weight (*row);
        }
      if (aligned == 0 || GSC_STYLE_ALIGNMENT_WEIGHT * aligned < evicted)
        continue;

      /*
       * The last state handed a spare loses it.  If the alignment's own
       * natural style went to a state ranked above that one, the state moves
       * to the freed spare and the natural style takes its old hints back.
       */
      style = handed.back ();
      handed.pop_back ();
      for (earlier = handed.begin (); earlier != handed.end (); ++earlier)
        {
          if (*earlier == home)
            {
              table->combo[style] = table->combo[home];
              *earlier = style;
              style = home;
            }
        }
      table->combo[style] = alignment;
      table->natural[style] = style == home;
    }
}

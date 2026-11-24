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
 * os_glk_styles.h: game-derived Glk style assignment.
 *
 * Glk styles don't combine, so the frontend prints each run of text in the
 * one style that stands for its whole attribute state -- bold, italic,
 * monospaced, centred, a <font size>.  Which states get a style of their own
 * is decided per game, before the main window opens: the game's text is
 * scanned for the states it uses (the census), the states today's fixed
 * precedence already gives a home keep it, and the rest are handed the
 * styles the game leaves free, hinted to look like the state they stand for.
 * Text in a state without a style falls back to the nearest state that has
 * one, by dropping attributes, never adding them.
 *
 * Nothing here calls Glk, so the census and the assignment build into the
 * headless harnesses as well (test/adrift4/styles/); the GSC_STYLE_* numbers
 * are the Glk spec's style_* values, checked against glk.h in os_glk.cpp.
 */

#ifndef OS_GLK_STYLES_H
#define OS_GLK_STYLES_H

#include <vector>

#include "core/scarier.h"

#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/* The eleven Glk styles, numbered as the Glk spec numbers them. */
enum
{
  GSC_STYLE_NORMAL = 0,
  GSC_STYLE_EMPHASIZED = 1,
  GSC_STYLE_PREFORMATTED = 2,
  GSC_STYLE_HEADER = 3,
  GSC_STYLE_SUBHEADER = 4,
  GSC_STYLE_ALERT = 5,
  GSC_STYLE_NOTE = 6,
  GSC_STYLE_BLOCKQUOTE = 7,
  GSC_STYLE_INPUT = 8,
  GSC_STYLE_USER1 = 9,
  GSC_STYLE_USER2 = 10,
  GSC_STYLE_COUNT = 11
};

/*
 * An attribute state.  The low bits are flags; underline shares the italic
 * bit, since Glk has no underline and today's precedence already prints both
 * as Emphasized.  So does <c> text with the game's colours not showing, which
 * keeps its Emphasized stand-in -- but only on otherwise plain text, as
 * everything else won over it in the fixed precedence.  Bits 8..15 hold the
 * size as a signed step of two points from the 12-point default, which is
 * also the stylehint_Size value that draws it.
 */
typedef unsigned int gsc_style_combo_t;
enum
{
  GSC_COMBO_BOLD = 1 << 0,
  GSC_COMBO_ITALIC = 1 << 1,
  GSC_COMBO_MONOSPACED = 1 << 2,
  GSC_COMBO_CENTER = 1 << 3,
  GSC_COMBO_RIGHT = 1 << 4,
  GSC_COMBO_FLAGS = (1 << 5) - 1,
  GSC_COMBO_SIZE_SHIFT = 8,
  GSC_COMBO_SIZE_MASK = 0xff << GSC_COMBO_SIZE_SHIFT
};

/* The Runner's default font size, and the range of size steps kept. */
enum
{
  GSC_STYLE_DEFAULT_POINTS = 12,
  GSC_STYLE_MIN_STEP = -3,
  GSC_STYLE_MAX_STEP = 6
};

extern gsc_style_combo_t gsc_style_combo (scr_bool bold, scr_bool italic,
                                          scr_bool monospaced,
                                          scr_bool center, scr_bool right,
                                          scr_bool standin, scr_int points);
extern scr_int gsc_style_combo_step (gsc_style_combo_t combo);
extern void gsc_style_combo_describe (gsc_style_combo_t combo,
                                      char *buffer, size_t length);

/*
 * The parts of a <font> tag that decide a style, shared by the census and
 * the live tag handler so the two can't disagree.  Both take the tag
 * argument already lowercased; the first is given the "face=" it found in
 * it, the second the "size=" and the enclosing size.
 */
extern scr_bool gsc_style_face_is_monospaced (const char *face);
extern scr_int gsc_style_apply_font_size (const char *size, scr_int points);

/*
 * The assignment: the state each style stands for, and whether print-time
 * lookups may land on it.  A style whose `natural` flag is set keeps the
 * hints today's frontend has always given it; the others are hinted from
 * their combo.
 */
typedef struct
{
  scr_bool assigned[GSC_STYLE_COUNT];
  scr_bool natural[GSC_STYLE_COUNT];
  gsc_style_combo_t combo[GSC_STYLE_COUNT];
} gsc_style_table_t;

extern void gsc_style_table_natural (gsc_style_table_t *table);
extern scr_int gsc_style_lookup (const gsc_style_table_t *table,
                                 gsc_style_combo_t combo);
extern const char *gsc_style_name (scr_int style);

/*
 * The census: for each state the game's text uses, how many visible
 * characters it covers, in all and in the introduction.
 * gsc_style_census_a4() and gsc_style_census_a5() rank the rows in the order
 * the assignment hands out styles.
 */
typedef struct
{
  gsc_style_combo_t combo;
  unsigned long characters;
  unsigned long intro_characters;
} gsc_style_census_row_t;

typedef struct
{
  std::vector<gsc_style_census_row_t> rows;
  unsigned long strings;
} gsc_style_census_t;

extern void gsc_style_census_a4 (scr_game game, gsc_style_census_t *census);
extern void gsc_style_census_a5 (const struct a5_xml_node_s *root,
                                 const struct a5_xml_node_s *introduction,
                                 const char *title,
                                 gsc_style_census_t *census);
extern void gsc_style_assign (const gsc_style_census_t *census,
                              gsc_style_table_t *table);

#endif

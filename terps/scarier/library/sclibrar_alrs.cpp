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
 * ALR sniffers: tests of whether one of the game's ALRs could be matching a
 * literal the Runner prints where Scarier would print something else.
 */

#include "sclibrar.h"


/*
 * lib_alrs_see_gap()
 *
 * TRUE if any of the game's ALRs could match across the Runner's literal
 * two-space gap after a line break: an Original that opens with a space, or
 * one that spans a break.  perspectives' ' Also here is a gun. ' is one --
 * it only fires on the Runner's "<br><br>  Also here is a gun.  " -- and
 * which ALRs fire is game behaviour, not text shape.
 */
scr_bool
lib_alrs_see_gap (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int alr_count, alr;

  vt_key[0].string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (alr = 0; alr < alr_count; alr++)
    {
      const scr_char *original, *cursor;

      vt_key[1].integer = alr;
      vt_key[2].string = "Original";
      original = prop_get_string (bundle, "S<-sis", vt_key);
      if (original[0] == ' ' || strchr (original, '\n'))
        return TRUE;
      for (cursor = strchr (original, '<'); cursor;
           cursor = strchr (cursor + 1, '<'))
        {
          if (scr_strncasecmp (cursor, "<br>", 4) == 0)
            return TRUE;
        }
    }
  return FALSE;
}


/*
 * lib_alrs_see_list_verb()
 *
 * TRUE if one of the game's ALRs could be matching the Runner's literal
 * " is " in the contents listing of this container: an Original holding
 * "Inside/On [the] <its short name> is" ("Inside the first aid kit is a
 * large bandage roll, ...", deadman; "On <X> is ", dolg), the joined
 * ", and inside is",
 * or a bare " is " (les_feux translates it).  Such a listing keeps the
 * Runner's " is " so the same ALRs fire -- which ALRs fire is game
 * behaviour, not text shape.
 */
static scr_bool
lib_alrs_word_before (const scr_char *text, const scr_char *end,
                      const scr_char *word)
{
  const size_t length = strlen (word);

  return (size_t) (end - text) >= length
         && scr_strncasecmp (end - length, word, length) == 0
         && (end - length == text || !scr_isalpha (end[-(scr_int) length - 1]));
}

static scr_bool
lib_alrs_prefixed (const scr_char *text, const scr_char *end)
{
  const scr_char *word_end;
  scr_int pass;

  /*
   * "Inside/On <name>", allowing up to four words between: an article and
   * the rest of the object's prefix ("Inside the first aid kit is").
   */
  for (pass = 0; pass < 5; pass++)
    {
      while (end > text && end[-1] == ' ')
        end--;
      if (lib_alrs_word_before (text, end, "inside")
          || lib_alrs_word_before (text, end, "on"))
        return TRUE;
      word_end = end;
      while (end > text && end[-1] != ' ')
        end--;
      if (end == word_end)
        break;
    }
  return FALSE;
}

static scr_bool
lib_alrs_text_has (const scr_char *text, const scr_char *first,
                   const scr_char *second, scr_bool prefixed)
{
  const size_t first_length = strlen (first);
  const size_t second_length = strlen (second);
  const scr_char *cursor;

  for (cursor = text; *cursor; cursor++)
    {
      if (scr_strncasecmp (cursor, first, first_length) == 0
          && scr_strncasecmp (cursor + first_length,
                              second, second_length) == 0
          && (!prefixed || lib_alrs_prefixed (text, cursor)))
        return TRUE;
    }
  return FALSE;
}

scr_bool
lib_alrs_see_list_verb (scr_gameref_t game, scr_int container)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  scr_int alr_count, alr;
  const scr_char *name;

  vt_key[0].string = "Objects";
  vt_key[1].integer = container;
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);

  vt_key[0].string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", vt_key);
  for (alr = 0; alr < alr_count; alr++)
    {
      const scr_char *original;

      vt_key[1].integer = alr;
      vt_key[2].string = "Original";
      original = prop_get_string (bundle, "S<-sis", vt_key);
      if ((!scr_strempty (name)
           && lib_alrs_text_has (original, name, " is", TRUE))
          || lib_alrs_text_has (original, "inside", " is", FALSE))
        return TRUE;
      original += strspn (original, " ");
      if (scr_strncasecmp (original, "is", 2) == 0
          && strspn (original + 2, " ") == strlen (original + 2))
        return TRUE;
    }
  return FALSE;
}

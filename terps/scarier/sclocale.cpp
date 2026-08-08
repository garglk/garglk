/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Module notes:
 *
 * o Standard libc ctype.h functions vary their results according to the
 *   currently set locale.  We want consistent Windows codepage 1252 or
 *   codepage 1251 (WinLatin1 or WinCyrillic) results.  To get this, then,
 *   we have to define the needed functions internally to SCARIER.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"


/*
 * All the ctype boolean and character tables contain 256 elements, one for
 * each possible scr_char value.  This is used to size arrays and to verify
 * that range setting functions do not overrun array boundaries.
 */
enum { TABLE_SIZE = 256 };


/*
 * loc_setrange_bool()
 * loc_setranges_bool()
 *
 * Helpers for building ctype tables.  Sets all elements from start to end
 * inclusive to TRUE, and iterate this on a ranges array.
 */
static void
loc_setrange_bool (scr_int start, scr_int end, scr_bool table[])
{
  scr_int index_;

  for (index_ = start; index_ <= end; index_++)
    {
      assert (index_ > -1 && index_ < TABLE_SIZE);
      table[index_] = TRUE;
    }
}

static void
loc_setranges_bool (const scr_int ranges[], scr_bool table[])
{
  scr_int index_;

  for (index_ = 0; ranges[index_] > -1; index_ += 2)
    {
      assert (ranges[index_] <= ranges[index_ + 1]);
      loc_setrange_bool (ranges[index_], ranges[index_ + 1], table);
    }
}


/*
 * loc_setrange_char()
 * loc_setranges_char()
 *
 * Helpers for building ctype conversion tables.  Sets all elements from start
 * to end inclusive to their index value plus the given offset, and iterate
 * this on a ranges array.
 */
static void
loc_setrange_char (scr_int start, scr_int end, scr_int offset, scr_char table[])
{
  scr_int index_;

  for (index_ = start; index_ <= end; index_++)
    {
      assert (index_ > -1 && index_ < TABLE_SIZE);
      assert (index_ + offset > -1 && index_ + offset < TABLE_SIZE);
      table[index_] = index_ + offset;
    }
}

static void
loc_setranges_char (const scr_int ranges[], scr_char table[])
{
  scr_int index_;

  for (index_ = 0; ranges[index_] > -1; index_ += 3)
    {
      assert (ranges[index_] <= ranges[index_ + 1]);
      loc_setrange_char (ranges[index_],
                         ranges[index_ + 1], ranges[index_ + 2], table);
    }
}


/*
 * A locale consists of a name, ranges for each table, and signatures for
 * autodetection based on the game's compilation date.  This is the static
 * data portion of a locale.
 */
enum { RANGES_LENGTH = 32 };
enum { SIGNATURE_COUNT = 24, SIGNATURE_LENGTH = 3 };
typedef struct
{
  const scr_char *const name;
  const scr_int isspace_ranges[RANGES_LENGTH];
  const scr_int isdigit_ranges[RANGES_LENGTH];
  const scr_int isalpha_ranges[RANGES_LENGTH];
  const scr_int toupper_ranges[RANGES_LENGTH];
  const scr_int tolower_ranges[RANGES_LENGTH];
  const scr_byte signature[SIGNATURE_COUNT][SIGNATURE_LENGTH];
} scr_locale_t;


/*
 * The locale table set is built from a locale using its ranges.  There is one
 * table for each function, and a pointer to the locale used to construct the
 * table, for synchronization with changed locales.  This is the dynamic data
 * portion of a locale.
 */
typedef struct
{
  const scr_locale_t *locale;
  scr_bool isspace[TABLE_SIZE];
  scr_bool isdigit[TABLE_SIZE];
  scr_bool isalpha[TABLE_SIZE];
  scr_char toupper[TABLE_SIZE];
  scr_char tolower[TABLE_SIZE];
} scr_locale_table_t;

/*
 * Define a single static locale table set.  This set re-initializes if it
 * detects a locale change.
 */
static scr_locale_table_t loc_locale_tables = {NULL, {0}, {0}, {0}, {0}, {0}};


/*
 * loc_synchronize_tables()
 * loc_check_tables_synchronized()
 *
 * Initialize tables for a locale.  And compare the locale tables to a locale
 * and if not for the same locale, (re-)initialize.
 */
static void
loc_synchronize_tables (const scr_locale_t *locale)
{
  /* Clear all tables and the locale pointer. */
  memset (&loc_locale_tables, 0, sizeof (loc_locale_tables));

  /* Set ranges and attach the new locale. */
  loc_setranges_bool (locale->isspace_ranges, loc_locale_tables.isspace);
  loc_setranges_bool (locale->isdigit_ranges, loc_locale_tables.isdigit);
  loc_setranges_bool (locale->isalpha_ranges, loc_locale_tables.isalpha);
  loc_setranges_char (locale->toupper_ranges, loc_locale_tables.toupper);
  loc_setranges_char (locale->tolower_ranges, loc_locale_tables.tolower);

  loc_locale_tables.locale = locale;
}

static void
loc_check_tables_synchronized (const scr_locale_t *locale)
{
  if (locale != loc_locale_tables.locale)
    loc_synchronize_tables (locale);
}


/*
 * Locale for Latin1.  The signatures in this locale are null since it is the
 * default locale; no matching required.  Also, none may be practical, as this
 * locale works for a large number of Western European languages (though in
 * practice, it seems that only English and French Adrift Latin1 games exist).
 */
static const scr_locale_t LATIN1_LOCALE = {
  "Latin1",
  {9,13,  32,32,  160,160,  -1},
  {48,57,  -1},
  {65,90,  97,122,  192,214,  216,246,  248,255,  138,138,  140,140,
   142,142,  154,154,  156,156,  158,158,  159,159,  -1},
  {0,TABLE_SIZE-1,0,  97,122,-32,  224,246,-32,  248,254,-32,  154,154,-16,
   156,156,-16,  158,158,-16,  255,255,-96,  -1},
  {0,TABLE_SIZE-1,0,  65,90,32,  192,214,32,  216,222,32,  138,138,16,
   140,140,16,  142,142,16,  159,159,96,  -1},
  {{0}}
};


/*
 * Locale for Cyrillic.  The signatures in this locale are month names in
 * both mixed case and lowercase Russian Cyrillic.
 */
static const scr_locale_t CYRILLIC_LOCALE = {
  "Cyrillic",
  {9,13,  32,32,  160,160,  -1},
  {48,57,  -1},
  {65,90,  97,122,  168,168,  184,184,  175,175,  191,191,  178,179,
   192,255,  -1},
  {0,TABLE_SIZE-1,0,  97,122,-32,  184,184,-16,  191,191,-16,  179,179,-1,
   224,255,-32,  -1},
  {0,TABLE_SIZE-1,0,  65,90,32,  168,168,16,  175,175,16,  178,178,1,
   192,223,32,  -1},
  {{223, 237, 226}, {212, 229, 226}, {204, 224, 240}, {192, 239, 240},
   {204, 224, 233}, {200, 254, 237}, {200, 254, 235}, {192, 226, 227},
   {209, 229, 237}, {206, 234, 242}, {205, 238, 255}, {196, 229, 234},
   {255, 237, 226}, {244, 229, 226}, {236, 224, 240}, {224, 239, 240},
   {236, 224, 233}, {232, 254, 237}, {232, 254, 235}, {224, 226, 227},
   {241, 229, 237}, {238, 234, 242}, {237, 238, 255}, {228, 229, 234}}
};


/* List of pointers to supported and available locales, NULL terminated. */
static const scr_locale_t *const AVAILABLE_LOCALES[] = {
  &LATIN1_LOCALE,
  &CYRILLIC_LOCALE,
  NULL
};

/*
 * The locale for the game, set below explicitly or on game startup, and
 * a flag to note if it's been set explicitly, to prevent autodetection from
 * overwriting a manual setting.
 */
static const scr_locale_t *loc_locale = &LATIN1_LOCALE;
static scr_bool loc_is_autodetect_enabled = TRUE;


/*
 * loc_locate_signature_in_date()
 *
 * Checks the format of the input date to ensure it matches the format
 * "dd [Mm]mm yyyy".  Returns the address of the month part of the string, or
 * NULL if it doesn't match the expected format.
 */
static const scr_char *
loc_locate_signature_in_date (const scr_char *date)
{
  scr_int day, year, converted;
  scr_char signature[SIGNATURE_LENGTH + 1];

  /* Clear signature, and convert using a scanf format. */
  memset (signature, 0, sizeof (signature));
  converted = sscanf (date, "%2ld %3[^ 0-9] %4ld", &day, signature, &year);

  /* Valid if we converted three values, and month has three characters. */
  if (converted == 3 && strlen (signature) == SIGNATURE_LENGTH)
    return strstr (date, signature);
  else
    return NULL;
}


/*
 * loc_compare_locale_signatures()
 *
 * Search a locale's signatures for a match with the signature passed in.
 * Returns TRUE if a match found, FALSE otherwise.  Uses memcmp rather than
 * any strcasecmp() variant because the signatures are in the locale's
 * codepage, but the locale is not yet (by definition) set.
 */
static scr_bool
loc_compare_locale_signatures (const char *signature, const scr_locale_t *locale)
{
  scr_int index_;
  scr_bool is_matched;

  /* Compare signatures, stopping on the first match found. */
  is_matched = FALSE;
  for (index_ = 0; index_ < SIGNATURE_COUNT; index_++)
    {
      if (memcmp (locale->signature[index_],
                  signature, sizeof (locale->signature[0])) == 0)
        {
           is_matched = TRUE;
           break;
        }
    }

  return is_matched;
}


/*
 * loc_find_matching_locale()
 *
 * Look at the incoming date, expected to be in the format "dd [Mm]mm yyyy",
 * where "[Mm]mm" is a standard month abbreviation of the locale in which the
 * Generator was run.  Match this with locale signatures, and return the
 * first locale that matches, or NULL if none match.
 */
static const scr_locale_t *
loc_find_matching_locale (const scr_char *date,
                          const scr_locale_t *const *locales)
{
  const scr_char *signature;
  const scr_locale_t *matched = NULL;

  /* Get the month part of date, and if valid, search locale signatures. */
  signature = loc_locate_signature_in_date (date);
  if (signature)
    {
      const scr_locale_t *const *iterator;

      /* Search for this signature in the locale's signatures. */
      for (iterator = locales; *iterator; iterator++)
        {
          if (loc_compare_locale_signatures (signature, *iterator))
            {
              matched = *iterator;
              break;
            }
        }
    }

  /* Return the matching locale, NULL if none matched. */
  return matched;
}


/*
 * loc_detect_game_locale()
 *
 * Set an autodetected value for the locale based on looking at a game's
 * compilation date.
 */
void
loc_detect_game_locale (scr_prop_setref_t bundle)
{
  assert (bundle);

  /* If an explicit locale has already been set, ignore the call. */
  if (loc_is_autodetect_enabled)
    {
      scr_vartype_t vt_key[1];
      const scr_char *compile_date;
      const scr_locale_t *matched;

      /* Read the game's compilation date from the properties. */
      vt_key[0].string = "CompileDate";
      compile_date = prop_get_string (bundle, "S<-s", vt_key);

      /* Search for a matching locale based on the game compilation date. */
      matched = loc_find_matching_locale (compile_date, AVAILABLE_LOCALES);

      /* If a locale matched, set the global locale to it. */
      if (matched)
        loc_locale = matched;
    }
}


/*
 * loc_ascii_tolower()
 * loc_ascii_strncasecmp()
 *
 * The standard scr_strncasecmp() calls scr_tolower(), which is locale specific.
 * This isn't a particular problem because it will set the Latin1 locale
 * automatically before continuing.  However, since locale names should always
 * be in ascii anyway, it's slightly safer to just use an ascii-only version
 * of this function.
 */
static scr_char
loc_ascii_tolower (scr_char ch)
{
  return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
}

static scr_int
loc_ascii_strncasecmp (const scr_char *s1, const scr_char *s2, scr_int n)
{
  scr_int index_;

  for (index_ = 0; index_ < n; index_++)
    {
      scr_int diff;

      diff = loc_ascii_tolower (s1[index_]) - loc_ascii_tolower (s2[index_]);
      if (diff < 0 || diff > 0)
        return diff < 0 ? -1 : 1;
    }

  return 0;
}


/*
 * loc_set_locale()
 * loc_get_locale()
 *
 * Set a locale explicitly from the name passed in, returning TRUE if a locale
 * matched the name.  Get the current locale, which may be the default locale
 * if none yet set.
 */
scr_bool
loc_set_locale (const scr_char *name)
{
  const scr_locale_t *matched = NULL;
  const scr_locale_t *const *iterator;
  assert (name);

  /*
   * Search locales for a matching name, abbreviated if necessary.  Stop on
   * the first match found.
   */
  for (iterator = AVAILABLE_LOCALES; *iterator; iterator++)
    {
      const scr_locale_t *const locale = *iterator;

      if (loc_ascii_strncasecmp (name, locale->name, strlen (name)) == 0)
        {
          matched = locale;
          break;
        }
    }

  /* If matched, set the global locale, and lock out future autodetection. */
  if (matched)
    {
      loc_locale = matched;
      loc_is_autodetect_enabled = FALSE;
    }

  return matched ? TRUE : FALSE;
}

const scr_char *
loc_get_locale (void)
{
  return loc_locale->name;
}


/*
 * loc_debug_dump_new_line()
 * loc_debug_dump_bool_table()
 * loc_debug_dump_char_table()
 * loc_debug_dump()
 *
 * Print out locale tables.
 */
static int
loc_debug_dump_new_line (scr_int index_, scr_int count)
{
  return index_ < TABLE_SIZE - 1 && index_ % count == count - 1;
}

static void
loc_debug_dump_bool_table (const scr_char *label,
                           scr_int count, const scr_bool table[])
{
  scr_int index_;

  scr_trace ("loc_locale_tables.%s = {\n  ", label);
  for (index_ = 0; index_ < TABLE_SIZE; index_++)
    {
      scr_trace ("%s%s", table[index_] ? "T" : "F",
                loc_debug_dump_new_line (index_, count) ? "\n  " : "");
    }
  scr_trace ("\n}\n");
}

static void
loc_debug_dump_char_table (const scr_char *label,
                           scr_int count, const scr_char table[])
{
  scr_int index_;

  scr_trace ("loc_locale_tables.%s = {\n  ", label);
  for (index_ = 0; index_ < TABLE_SIZE; index_++)
    {
      scr_trace ("%02lx%s", (scr_int) (scr_byte) table[index_],
                loc_debug_dump_new_line (index_, count) ? "\n  " : " ");
    }
  scr_trace ("\n}\n");
}

void
loc_debug_dump (void)
{
  scr_trace ("Locale: debug dump follows...\n");

  loc_check_tables_synchronized (loc_locale);
  scr_trace ("loc_locale_tables"
            ".locale->name = %s\n", loc_locale_tables.locale->name);

  loc_debug_dump_bool_table ("isspace", 64, loc_locale_tables.isspace);
  loc_debug_dump_bool_table ("isdigit", 64, loc_locale_tables.isdigit);
  loc_debug_dump_bool_table ("isalpha", 64, loc_locale_tables.isalpha);
  loc_debug_dump_char_table ("toupper", 16, loc_locale_tables.toupper);
  loc_debug_dump_char_table ("tolower", 16, loc_locale_tables.tolower);
}


/*
 * loc_bool_template()
 * loc_char_template()
 *
 * "Template" functions for locale variant ctype functions.  Synchronize
 * tables to the currently set locale, and return the value from the table.
 */
static scr_bool
loc_bool_template (scr_char character, const scr_bool table[])
{
  loc_check_tables_synchronized (loc_locale);
  return table[(scr_byte) character];
}

static scr_char
loc_char_template (scr_char character, const scr_char table[])
{
  loc_check_tables_synchronized (loc_locale);
  return table[(scr_byte) character];
}


/*
 * scr_isspace()
 * scr_isalpha()
 * scr_isdigit()
 * scr_tolower()
 * scr_toupper()
 *
 * Public entry points into locale variant ctype functions.
 */
scr_bool
scr_isspace (scr_char character)
{
  return loc_bool_template (character, loc_locale_tables.isspace);
}

scr_bool
scr_isalpha (scr_char character)
{
  return loc_bool_template (character, loc_locale_tables.isalpha);
}

scr_bool
scr_isdigit (scr_char character)
{
  return loc_bool_template (character, loc_locale_tables.isdigit);
}

scr_char
scr_toupper (scr_char character)
{
  return loc_char_template (character, loc_locale_tables.toupper);
}

scr_char
scr_tolower (scr_char character)
{
  return loc_char_template (character, loc_locale_tables.tolower);
}

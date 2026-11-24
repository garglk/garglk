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
 * Noun phrases: the articles and names of objects and NPCs.
 */

#include "sclibrar.h"


/*
 * The Runner's own case-sensitive test, for the one caller that builds a
 * command line rather than text: the battle "with" prefix re-parsed on the
 * next turn (lib_battle_definite_name), which keeps the measured spelling.
 * The folding one printed names use is lib_compare_article(), in
 * core/scobjcts.cpp.
 */
scr_bool
lib_compare_article_binary (const scr_char *string, const scr_char *word,
                            scr_int length)
{
  assert (string && word);

  return strncmp (string, word, length) == 0
         && (string[length] == NUL || scr_isspace (string[length]));
}


/*
 * lib_print_object_np()
 * lib_print_object()
 *
 * Convenience functions to print out an object's name, with a "normalized"
 * prefix -- any "a"/"an"/"some" is replaced by "the" -- and with the full
 * prefix.  The normalized form is lib_definite_object_name(), in
 * core/scobjcts.cpp, where the measurements behind it are.
 */
void
lib_print_object_np (scr_gameref_t game, scr_int object)
{
  pf_buffer_string (gs_get_filter (game),
                    lib_definite_object_name (gs_get_bundle (game),
                                              object).c_str ());
}

void
lib_print_object (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix, *name;

  /*
   * Get the object's prefix and print it, then a space; both unconditionally,
   * the Runner's builder being a plain concatenation.  An object authored
   * with no prefix at all arrives here carrying the literal "a" the loader
   * substituted into it (see parse_trim_object_names in sctafpar.cpp), never
   * inflected to "an" before a vowel: run380 on the pwearv probe, four
   * wearables with empty prefixes and vowel-initial names, answers "You put
   * on a apple." and lists them back as "You are wearing a apple, a orange, a
   * egg and a umbrella".
   */
  vt_key[0].string = "Objects";
  vt_key[1].integer = object;
  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, prefix);
  pf_buffer_character (filter, ' ');

  /* Print object name. */
  vt_key[2].string = "Short";
  name = prop_get_string (bundle, "S<-sis", vt_key);
  pf_buffer_string (filter, name);
}


/*
 * lib_print_object_raw()
 *
 * Print an object as the pre-3.9 "remove" handler does: the authored prefix, a
 * space, and the name, with no normalizing and no default for an empty prefix.
 * run370 @42980D and run380 @42FF38 both build the message by plain
 * concatenation, `... & " remove " & ob(0) & " " & ob(4) & "."`, where every
 * other lister in those Runners passes the prefix through tense() first
 * (run370 drop @430A50, run380 drop @438C3B).  3.9 unified the two: run390
 * builds both its drop @00045B93 and its remove @00039E15 out of the same
 * General.Sub_3_45 helper, so from 3.9 on "remove" normalizes like the rest.
 * Measured live under Wine, akron.taf (3.8) in run380: "You remove a grubby
 * sweatshirt." and "You remove a pair of ragged jeans." where the same game in
 * Scarier said "the".
 *
 * Scarier deliberately keeps "the" in the success reports all the same
 * (deviation policy) -- an indefinite article on the object just removed,
 * worn or put away reads as if it were some other one -- so this printer is
 * left to the refusals ("You are not wearing a hat!"), where it reads fine.
 */
void
lib_print_object_raw (scr_gameref_t game, scr_int object)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[3];
  const scr_char *prefix;

  vt_key[0].string = "Objects";
  vt_key[1].integer = object;

  vt_key[2].string = "Prefix";
  prefix = prop_get_string (bundle, "S<-sis", vt_key);

  /*
   * "No default for an empty prefix" is right about the message builder and
   * wrong about what reaches it: the pre-3.9 loaders rewrite an empty Prefix
   * to a literal "a" before any of this (run370 @43F5DA, run380 @4481B2, and
   * run400 at loc_4900EC too), so the concatenation never sees one.  That
   * rewrite is modelled in parse_trim_object_names now, so nothing is needed
   * here.
   */
  pf_buffer_string (filter, prefix);
  pf_buffer_character (filter, ' ');

  vt_key[2].string = "Short";
  pf_buffer_string (filter, prop_get_string (bundle, "S<-sis", vt_key));
}


/*
 * lib_print_npc_np
 *
 * Convenience function to print out an NPC's name, without any prefix.
 */
void
lib_print_npc_np (scr_gameref_t game, scr_int npc)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  /* Get the NPC's short description, and print it. */
  pf_buffer_string (filter,
                    prop_get_indexed_string (bundle, "NPCs", npc, "Name"));
}

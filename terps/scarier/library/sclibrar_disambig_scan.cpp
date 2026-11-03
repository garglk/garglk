/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
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
 */

/*
 * The Runners' checkobject scan as the pre-4.0 dispatchers run it: term
 * containment and shadowing, long names and aliases, the candidate and
 * mode tests, lib_runner_co_scan() and the 3.7-3.9 ambiguity prompt.
 *
 * See sclibrar_disambig_internal.h for what the disambiguation files
 * share.
 */

#include <algorithm>

#include "sclibrar.h"
#include "sclibrar_internal.h"
#include "sclibrar_disambig_internal.h"

/*
 * lib_co_contains()
 * lib_co_lastword()
 * lib_runner_co_scan()
 * lib_co_ambiguity_prompt()
 * lib_trace_runner_co()
 *
 * The pre-4.0 Runners' object-ambiguity test, and the end-of-turn prompt it
 * raises.  Scarier's own `%object%` matcher is positional, so `take truck
 * keys` binds only the truck keys; the Runner has no positional matcher and
 * asks instead.
 *
 * The Runner matches an object by scanning the *whole* typed command for its
 * Short name or, failing that, its Alias (run380 `c()` @429048: a
 * case-insensitive InStr whose hit must start at the string start or after a
 * space, and end at the string end, a space or a comma).  Its `co()`
 * @42DE60 (run370 @4261B4, run390 @43B6BC) then takes the term that
 * matched, counts every object *present* (obhere: in the room, carried,
 * worn, or in/on something here) whose Short or Alias is exactly that term,
 * and if more than one is present flags the command ambiguous by stamping
 * the object's number into MemVar_44F124 (@42DDC7).  One escape hatch: if
 * the player also typed the last word of the object's own Prefix ("take
 * *silver* key"), @42DD4C stamps the resolved marker &HFE instead, and that
 * marker outranks any ambiguity flagged by any other object in the same
 * scan (@42DDC1 only writes an object number when the marker is not already
 * set).  The list is built once, by the first ambiguous object (@42DC1E:
 * every present object answering to that term, tense(Prefix) & " " & Short,
 * joined with ", " and " or "); the prompt's term comes from the LAST
 * flagged object.
 *
 * generaltasks() runs that scan over every object at the start of EVERY
 * command (run380 loc_441D5D, straight after the built-in input rewrites),
 * so the flag is raised whatever handler goes on to answer the line.  It is
 * read at the very end of the turn, after events have ticked (@4431B0,
 * `If (MemVar_44F124 < 0) Or (MemVar_44F12C = 1)`): unless a game task ran
 * (MemVar_44F12C, set at 44D0BA inside the task executor) the turn's whole
 * output is thrown away and replaced by
 *
 *     Which <term>.  <The X, the Y or the Z>?
 *
 * (@4432AA with the Short when the player typed it, @443303 with the Alias
 * otherwise).  Everything the turn DID still stands: mikes.taf cmd 27 `take
 * truck keys`, with the carried mustang keys and the truck keys both
 * aliased "keys", answers "Which keys.  The mustang keys or the truck
 * keys?" -- and the truck keys are taken, because `drive truck bob` works
 * 30 commands later (run380 under Wine, runner_probes/life_of_mike.run380.rtf
 * and the 2026-09-04 re-drive; an earlier note that the keys were NOT taken
 * was wrong).  The next command is not eaten as an answer -- `east` after the
 * prompt simply moves east.  4.0 narrows differently (the up-front word score
 * of Proc_21_58_463640, see lib_absent_seen_object()) and never raises this
 * prompt from the dispatcher, so the port stops at 3.9.
 */
scr_bool
lib_co_contains (const scr_char *command, const scr_char *term)
{
  scr_int term_length, index_;

  if (!command || !term || term[0] == NUL)
    return FALSE;

  /*
   * Deliberate deviations: the Runner's c() takes InStr's FIRST hit and
   * fails if that one is not word-bounded, so "key" is never found in
   * `x keyring key`; and it matches the raw stored name, so a Short
   * authored with a trailing space ("necko wafers ", superliam) is never
   * found at all.  Scarier looks on past a bad hit and ignores trailing
   * blanks.
   */
  term_length = strlen (term);
  while (term_length > 0 && scr_isspace (term[term_length - 1]))
    term_length--;
  if (term_length == 0)
    return FALSE;
  for (index_ = 0; command[index_] != NUL; index_++)
    {
      scr_char after;

      if (scr_strncasecmp (command + index_, term, term_length) != 0)
        continue;
      if (index_ > 0 && command[index_ - 1] != ' ')
        continue;

      after = command[index_ + term_length];
      if (after == NUL || after == ' ' || after == ',')
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_co_term_shadowed()
 *
 * Deliberate deviation: TRUE when the line holds the object's term only
 * inside a longer Short or Alias of another object -- `take key ring` with a
 * "key" and a "key ring" does not name the key, nor `take truck keys` the
 * "mustang keys" aliased "keys".  The Runners' co() counts both, and gives
 * up on the pair or asks which.  Each longer name is blanked out of a copy
 * of the line, so `put key on key ring` still names the key.
 */
scr_bool
lib_co_term_shadowed (scr_gameref_t game, const scr_char *line,
                      scr_int object, const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const size_t term_length = term ? strlen (term) : 0;
  std::string rest;
  scr_bool shadowed = FALSE;
  scr_int other;

  if (!line || term_length == 0 || !lib_co_contains (line, term))
    return FALSE;

  rest = line;
  GS_FOR_EACH_OBJECT (game, other)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      if (other == object)
        continue;
      /*
       * An object never seen and not here is nothing the player can mean,
       * so its longer name hides nobody's: Scene of the Crime (4.00) `drop
       * tiny card` with "a small card" in hand and "a tiny card" (alias
       * "tiny card") not yet in play drops the small card (run400x
       * runner_probes/sotc.run400.drop.txt, 2026-10-09), where the shadow
       * left the line to "I am not holding the small card.".
       */
      if (!gs_object_seen (game, other)
          && !obj_indirectly_in_room (game, other, gs_playerroom (game))
          && !obj_indirectly_held_by_player (game, other))
        continue;
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", other);
      for (alias = -1; alias < alias_count; alias++)
        {
          const scr_char *name;
          size_t length, posn;

          if (alias < 0)
            name = prop_get_indexed_string (bundle, "Objects", other,
                                            "Short");
          else
            {
              vt_key[3].integer = alias;
              name = prop_get_string (bundle, "S<-sisi", vt_key);
            }
          if (!name || strlen (name) <= term_length
              || !lib_co_contains (name, term))
            continue;

          length = strlen (name);
          for (posn = 0; posn + length <= rest.size (); posn++)
            {
              if (scr_strncasecmp (rest.c_str () + posn, name, length) != 0
                  || (posn > 0 && rest[posn - 1] != ' ')
                  || (posn + length < rest.size ()
                      && rest[posn + length] != ' '
                      && rest[posn + length] != ','))
                continue;
              rest.replace (posn, length, length, '#');
              shadowed = TRUE;
            }
        }
    }
  return shadowed && !lib_co_contains (rest.c_str (), term);
}

/*
 * lib_references_are_namesakes()
 *
 * TRUE when two or more objects are referenced and all share one Short.
 * Deliberate deviation (2026-09-30): the pre-4.0 loops that act on every
 * referenced object (open/close at 3.7-3.9, wear/remove at 3.7) are for a
 * line naming several things; a crowd of namesakes is asked about instead,
 * so `open door` with a red and a blue door no longer opens both, nor
 * `wear hat` put on both hats (83c4a19c0, 2e4d7ba0e).
 */
scr_bool
lib_references_are_namesakes (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *first = nullptr;
  scr_int index_, count = 0;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      const scr_char *shortname;

      if (!game->object_references[index_])
        continue;
      shortname = prop_get_indexed_string (bundle, "Objects", index_, "Short");
      if (!shortname)
        return FALSE;
      if (!first)
        first = shortname;
      else if (scr_strcasecmp (first, shortname) != 0)
        return FALSE;
      count++;
    }
  if (count > 1)
    SCR_DEVIATION ("namesakes_asked", "short=\"%s\"", first);
  return count > 1;
}

/*
 * lib_cache_long_names()
 * lib_forget_bundle()
 *
 * The names lib_mask_long_names() masks: every object's Short and Aliases,
 * then every character's Name and Aliases, in that order, trimmed of outer
 * spaces, keeping those of two words or more.  The bundle fixes them for
 * the whole game, and the masking runs several times a command, where
 * reading them back out of the properties each time was a seventh of a
 * walkthrough's run time.  So they are collected once per bundle; the
 * runner calls lib_forget_bundle() before destroying one, so a later bundle
 * allocated at the same address can't inherit them.
 */
void
lib_forget_bundle (scr_prop_setref_t bundle)
{
  if (disambig.long_names.bundle == bundle)
    {
      size_t category;

      disambig.long_names.bundle = nullptr;
      for (category = 0; category < LIB_LONG_NAME_CATEGORIES; category++)
        disambig.long_names.names[category].clear ();
    }
}

static void
lib_cache_long_names (scr_gameref_t game)
{
  static const scr_char *const CATEGORIES[LIB_LONG_NAME_CATEGORIES]
    = { "Objects", "NPCs" };
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  size_t category;

  if (disambig.long_names.bundle == bundle)
    return;

  lib_forget_bundle (disambig.long_names.bundle);
  disambig.long_names.bundle = bundle;
  for (category = 0; category < LIB_LONG_NAME_CATEGORIES; category++)
    {
      const scr_char *const kind = CATEGORIES[category];
      const scr_int count = category == 0 ? gs_object_count (game)
                                          : gs_npc_count (game);
      scr_int index_;

      for (index_ = 0; index_ < count; index_++)
        {
          scr_vartype_t vt_key[4];
          scr_int alias_count, alias;

          alias_count = lib_alias_prepare (bundle, vt_key, kind, index_);
          for (alias = -1; alias < alias_count; alias++)
            {
              const scr_char *name;
              size_t length;

              if (alias < 0)
                name = prop_get_indexed_string (bundle, kind, index_,
                                                category == 0 ? "Short"
                                                              : "Name");
              else
                {
                  vt_key[3].integer = alias;
                  name = prop_get_string (bundle, "S<-sisi", vt_key);
                }
              if (!name)
                continue;
              name += strspn (name, " ");
              length = strlen (name);
              while (length > 0 && name[length - 1] == ' ')
                length--;
              if (length == 0 || !memchr (name, ' ', length))
                continue;
              disambig.long_names.names[category].emplace_back (name, length);
            }
        }
    }
}

/*
 * lib_mask_long_names()
 *
 * Deliberate deviation (2026-09-30).  A copy of LINE, the same length, with
 * every whole-word occurrence of an object's Short or Alias, or a
 * character's Name or Alias, that runs to two words or more overwritten by
 * LIB_NAME_MASK.  The Runner ports that look for a verb anywhere on the line
 * (two-verb lines, the verb hoist, sitstand) test the masked copy, so the
 * "pick" of an ice pick, the "drop" of a cough drop or the "stand" of a hat
 * stand is no verb: `x ice pick` examines it, where the ports answered
 * "Take what?".  OBJECTS_ONLY leaves character names standing, for the
 * tests that ask whether the line names a character at all.
 */
std::string
lib_mask_long_names (scr_gameref_t game, const scr_char *line,
                     scr_bool objects_only)
{
  std::string masked (line ? line : "");
  size_t category;

  lib_cache_long_names (game);
  for (category = 0; category < (objects_only ? 1u : 2u); category++)
    {
      for (const std::string &name : disambig.long_names.names[category])
        {
          const size_t length = name.size ();
          size_t posn;

          for (posn = 0; posn + length <= masked.size (); posn++)
            {
              if (scr_strncasecmp (masked.c_str () + posn, name.c_str (),
                                   length) != 0
                  || (posn > 0 && masked[posn - 1] != ' ')
                  || (posn + length < masked.size ()
                      && masked[posn + length] != ' '
                      && masked[posn + length] != ','
                      && masked[posn + length] != '.'))
                continue;
              masked.replace (posn, length, length, LIB_NAME_MASK);
            }
        }
    }
  return masked;
}

const scr_char *
lib_co_lastword (const scr_char *string)
{
  const scr_char *space;

  if (!string)
    return nullptr;
  space = strrchr (string, ' ');
  return space ? space + 1 : string;
}

/*
 * lib_co_names_prefix()
 *
 * Whether the typed line names the object's Prefix, when a term has several
 * present namesakes.  The Runners' co() matches one Prefix word: 3.90 and
 * 4.00 take the last word of the whole Prefix; 3.70 and 3.80 drop its
 * FIRST word first, so a Prefix of one word
 * -- "a", "the", or a bare adjective like "big" -- distinguishes nothing at
 * all, and a "big red" tells nothing apart from a "small red".
 *
 * Measured on p*TAKEP and p*TAKEQ (runner_probes/takep.run*.* and
 * runner_probes/takeq.run*.*, 2026-09-20), everything loose in one lit
 * room:
 *
 *                               3.70 / 3.80        3.90            4.00
 *   "big" gem, "small" gem
 *     take gem                  no             no              no
 *     take big gem              no             the big gem     the big gem
 *   "a" orb, "a" orb / "the" cog, "the" cog
 *     take orb / take cog       no             no              no
 *   "old red" pin, "new red" pin
 *     take pin                  no             no              no
 *     take red pin              BOTH           the old pin     no
 *   "a very red" gem, "a very blue" gem
 *     take very gem             no             no              no
 *     take red gem              the red gem    the red gem     the red gem
 *   "big red" pin, "small red" pin
 *     take big pin              no             no              the big pin
 *
 * ("no" is the version's own refusal: "Take what?" at 3.70, "Which pin.
 * Old red pin or new red pin?" at 3.80/3.90, and at 4.00 either that
 * question or "It is not clear which pin you are referring to." from
 * drops.)  `take red gem` answering while `take very gem` does not is what
 * makes it the LAST word of what is left and not the second; `take big
 * pin` failing at 3.90 as well is what makes 3.90's word the last of the
 * WHOLE Prefix and not the first-dropped one.  4.00 is the word score
 * instead (lib_verb_object_name_score()), which counts every Prefix word.
 *
 * Deliberate deviation: Scarier asks instead whether the line names some
 * word of the Prefix other than an article, and no word that only another
 * object of the same Short has in its Prefix, and no other object of that
 * Short passes the same test.  So `take big gem` picks the big gem at
 * 3.70/3.80, and `take old pin` and `take old red pin` the old red pin
 * everywhere, where the Runners refuse, ask "Which pin?" or take both; and
 * `take red pin` asks "Which pin?" everywhere, where 3.70/3.80 take both
 * and 3.90 the old pin.
 */
static scr_bool
lib_co_is_article (const std::string &word)
{
  return scr_strcasecmp (word.c_str (), "a") == 0
         || scr_strcasecmp (word.c_str (), "an") == 0
         || scr_strcasecmp (word.c_str (), "the") == 0
         || scr_strcasecmp (word.c_str (), "some") == 0;
}

static std::vector<std::string>
lib_co_prefix_words (scr_gameref_t game, scr_int object)
{
  const scr_char *prefix;
  std::vector<std::string> words;
  size_t start, end;

  prefix = prop_get_indexed_string (gs_get_bundle (game), "Objects", object,
                                    "Prefix");
  if (!prefix)
    return words;

  const std::string text (prefix);
  for (start = 0; start < text.size (); start = end + 1)
    {
      end = text.find (' ', start);
      if (end == std::string::npos)
        end = text.size ();
      if (end > start && !lib_co_is_article (text.substr (start, end - start)))
        words.push_back (text.substr (start, end - start));
    }
  return words;
}

/* TRUE if the line names a Prefix word that only another object of the same
   Short has: `take old pin` rules out the new red pin. */
scr_bool
lib_co_prefix_excluded (scr_gameref_t game, const scr_char *line,
                        scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const std::vector<std::string> own = lib_co_prefix_words (game, object);
  const scr_char *shortname;
  scr_int other;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  GS_FOR_EACH_OBJECT (game, other)
    {
      const scr_char *name
        = prop_get_indexed_string (bundle, "Objects", other, "Short");

      if (other == object || !name || !shortname
          || scr_strcasecmp (name, shortname) != 0)
        continue;
      for (const std::string &word : lib_co_prefix_words (game, other))
        {
          if (std::find (own.begin (), own.end (), word) == own.end ()
              && lib_co_contains (line, word.c_str ()))
            return TRUE;
        }
    }
  return FALSE;
}

static scr_bool
lib_co_prefix_named (scr_gameref_t game, const scr_char *line, scr_int object)
{
  scr_bool named = FALSE;

  for (const std::string &word : lib_co_prefix_words (game, object))
    named |= lib_co_contains (line, word.c_str ());
  return named && !lib_co_prefix_excluded (game, line, object);
}

scr_bool
lib_co_names_prefix (scr_gameref_t game, const scr_char *line, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname;
  scr_int other;

  if (!lib_co_prefix_named (game, line, object))
    return FALSE;

  /* A namesake the line names just as well is a tie: `take red pin`. */
  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  GS_FOR_EACH_OBJECT (game, other)
    {
      const scr_char *name
        = prop_get_indexed_string (bundle, "Objects", other, "Short");

      if (other != object && name && shortname
          && scr_strcasecmp (name, shortname) == 0
          && lib_co_prefix_named (game, line, other))
        return FALSE;
    }
  return TRUE;
}

/*
 * lib_alias_prepare()
 *
 * Point vt_key[0..2] at the given object's/NPC's ("Objects"/"NPCs") Alias
 * list, and return its count -- ready for a "vt_key[3].integer = alias"
 * loop fetching each one with "S<-sisi".
 */
scr_int
lib_alias_prepare (const scr_prop_setref_t bundle, scr_vartype_t *vt_key,
                   const scr_char *category, scr_int index)
{
  vt_key[0].string = category;
  vt_key[1].integer = index;
  vt_key[2].string = "Alias";
  return prop_get_child_count (bundle, "I<-sis", vt_key);
}


/*
 * lib_first_alias()
 *
 * The object's/NPC's first Alias string, or NULL if it has none.
 */
const scr_char *
lib_first_alias (const scr_prop_setref_t bundle, scr_vartype_t *vt_key,
                 const scr_char *category, scr_int index)
{
  if (lib_alias_prepare (bundle, vt_key, category, index) < 1)
    return nullptr;

  vt_key[3].integer = 0;
  return prop_get_string (bundle, "S<-sisi", vt_key);
}


/* TRUE if the object's Short or Alias is exactly the term. */
scr_bool
lib_named_answers_to (scr_gameref_t game, const scr_char *class_,
                      scr_int index_, const scr_char *name_field,
                      const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[4];
  const scr_char *name;
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, class_, index_, name_field);
  if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
    return TRUE;

  alias_count = lib_alias_prepare (bundle, vt_key, class_, index_);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
        return TRUE;
    }
  return FALSE;
}

scr_bool
lib_co_object_answers_to (scr_gameref_t game, scr_int object,
                          const scr_char *term)
{
  return lib_named_answers_to (game, "Objects", object, "Short", term);
}

/*
 * TRUE if the object counts as a namesake candidate: present (obhere) and,
 * from 3.90, also seen -- run390 co() @43B2FB and the list loop @43B4AE both
 * test the object's seen byte (field 44) beside obhere; run380 co() has no
 * such test.
 */
scr_bool
lib_co_candidate (scr_gameref_t game, scr_int object, scr_int room)
{
  if (!obj_indirectly_in_room (game, object, room))
    return FALSE;
  return prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390
         || gs_object_seen (game, object);
}

scr_bool
lib_object_held_pre380 (scr_gameref_t game, scr_int object)
{
  return gs_object_position (game, object) == OBJ_HELD_PLAYER
         || gs_object_position (game, object) == OBJ_WORN_PLAYER;
}

/*
 * lib_namesake_crowded_pre380()
 *
 * Whether this object is one of the namesakes 3.7's loops find themselves
 * crowded on: its Short is in the typed line, and two or more present
 * objects on the verb's own side -- loose for takes(), held for drops() --
 * have their Shorts in it.  The count (takes' var_116, 436250) is of every
 * such Short, not of one shared name: `take coin hat` with both loose is
 * "Take what?" (p37ORD, run370 runner_probes/ord.run370.mult.rtf), so
 * same-Short namesakes are only the commonest crowd.  3.7's handlers never
 * call co(), so what makes a 3.7 crowd is the SHORT alone: two objects
 * answering to one word through an Alias are not namesakes to it, and each
 * simply acts.  p37OPENA (runner_probes/opena.run370.rtf, 2026-09-20) is that
 * side: a gem and a rock aliased "gem", both Prefixed "a", and `take gem` is
 * "You pick up the rock." with BOTH in the inventory afterwards -- while two
 * objects both Short "orb" and both Prefixed "a" are "Take what?" (p37TAKEP,
 * runner_probes/takep.run370.rtf).
 */
scr_bool
lib_namesake_crowded_pre380 (scr_gameref_t game, const scr_char *line,
                             scr_int object, scr_bool want_held)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int room = gs_playerroom (game);
  const scr_char *shortname;
  scr_int other, present;

  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (scr_strempty (shortname) || !line || !lib_co_contains (line, shortname)
      || lib_co_term_shadowed (game, line, object, shortname))
    return FALSE;
  /* takes() acts on nothing on a "from" line (43648C), so its take-from
     lines are no crowd. */
  if (!want_held && lib_co_contains (line, "from"))
    return FALSE;

  present = 0;
  GS_FOR_EACH_OBJECT (game, other)
    {
      const scr_char *name
        = prop_get_indexed_string (bundle, "Objects", other, "Short");

      if (lib_co_candidate (game, other, room)
          && lib_object_held_pre380 (game, other) == want_held
          && !scr_strempty (name) && lib_co_contains (line, name)
          && !lib_co_term_shadowed (game, line, other, name))
        present++;
    }
  return present > 1;
}

/* TRUE if the object's Short or any Alias is a whole word run of the line,
   which is how 3.7's takes()/drops() loop names an object. */
scr_bool
lib_names_object_370 (scr_gameref_t game, const scr_char *line,
                      scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && lib_co_contains (line, name))
    return TRUE;
  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && lib_co_contains (line, name))
        return TRUE;
    }
  return FALSE;
}

/* TRUE if every Short and Alias of the object that the line names sits
   inside a longer name another object answers to -- "key" in `take key
   ring`.  Deliberate deviation: the Runner counts the key into the crowd
   and answers "Take what?"; Scarier leaves it out.  See
   lib_co_term_shadowed(). */
scr_bool
lib_names_only_shadowed_370 (scr_gameref_t game, const scr_char *line,
                             scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  name = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (!scr_strempty (name) && lib_co_contains (line, name)
      && !lib_co_term_shadowed (game, line, object, name))
    return FALSE;
  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (!scr_strempty (name) && lib_co_contains (line, name)
          && !lib_co_term_shadowed (game, line, object, name))
        return FALSE;
    }
  return TRUE;
}

/*
 * lib_takes_offers_tasks_370()
 *
 * TRUE when run370's takes() hands the typed line to the task matcher
 * itself.  takes() is entered on c("get") Or c("take") Or c("pick") Or
 * c(<the game's own take word>) And Not c("from") -- the And binds to the
 * slot word alone (435E28).  With no "all" and no "and" on the line, its last
 * loop (436B8D-436CC6) finds the first object, in index order and wherever it
 * is, whose Short or Alias the line names, calls tasks(1) with the line as
 * typed (436CAD) and leaves.  takes() then returns Empty -- it has no store
 * to its result slot; the `takes = MemVar_4460E4` VB Decompiler prints at
 * 436D17 is its ExitProc -- so generaltasks goes on to its own tasks(0) at
 * 43B972 and the matcher runs again.  See run_takes_second_pass_370().
 */
scr_bool
lib_takes_offers_tasks_370 (scr_gameref_t game, const scr_char *line)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object;

  if (prop_get_taf_version (bundle) != TAF_VERSION_370 || !line)
    return FALSE;

  const std::string slot = lib_command_slot_370 (bundle, 11);
  if (!(lib_co_contains (line, "get") || lib_co_contains (line, "take")
        || lib_co_contains (line, "pick")
        || (lib_co_contains (line, slot.c_str ())
            && !lib_co_contains (line, "from"))))
    return FALSE;
  if (lib_co_contains (line, "all") || lib_co_contains (line, "and"))
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      if (lib_names_object_370 (game, line, object))
        return TRUE;
    }
  return FALSE;
}

/*
 * lib_co_mode_fits()
 *
 * Whether an object is on the side a co() mode recounts: 1 loose (take),
 * 2 isheld (drop), 3 held and not worn (wear), 4 worn (remove).
 */
scr_bool
lib_co_mode_fits (scr_gameref_t game, scr_int object, scr_int mode)
{
  switch (mode)
    {
    case 1:
      return !obj_is_static (game, object)
             && gs_object_position (game, object) == gs_playerroom (game) + 1;
    case 2:
      return lib_isheld_390 (game, object);
    case 3:
      return gs_object_position (game, object) == OBJ_HELD_PLAYER;
    case 4:
      return gs_object_position (game, object) == OBJ_WORN_PLAYER;
    default:
      return TRUE;
    }
}

/*
 * Which objects fitted the line's co() mode when the line began.  The
 * end-of-turn scan runs after the handler has acted -- `drop hat` has put
 * the held hat on the floor by then -- so it counts by this snapshot
 * (disambig.line.top_mode, top_fits).
 */
static scr_bool
lib_co_mode_fitted (scr_gameref_t game, scr_int object, scr_int mode)
{
  if (mode == disambig.line.top_mode && mode != 0
      && object < (scr_int) disambig.line.top_fits.size ())
    return disambig.line.top_fits[object];
  return lib_co_mode_fits (game, object, mode);
}

/* The co() mode for a line's leading verb, as lib_co_mode_fits() reads it. */
scr_int
lib_co_verb_mode (const scr_char *line)
{
  while (*line && scr_isspace (*line))
    line++;
  const scr_char *end = line;
  while (*end && !scr_isspace (*end))
    end++;
  const std::string word (line, end - line);
  const scr_char *rest = end;
  while (*rest && scr_isspace (*rest))
    rest++;
  if (scr_strcasecmp (word.c_str (), "drop") == 0)
    return 2;
  if (scr_strcasecmp (word.c_str (), "wear") == 0
      || scr_strcasecmp (word.c_str (), "don") == 0)
    return 3;
  if (scr_strcasecmp (word.c_str (), "remove") == 0
      || scr_strcasecmp (word.c_str (), "doff") == 0)
    return 4;
  if (scr_strcasecmp (word.c_str (), "take") == 0
      || scr_strcasecmp (word.c_str (), "get") == 0
      || scr_strcasecmp (word.c_str (), "pick") == 0)
    return scr_strncasecmp (rest, "off", 3) == 0
           && (rest[3] == NUL || scr_isspace (rest[3])) ? 4 : 1;
  return 0;
}

/*
 * The co() mode the present namesakes answering to term are counted by:
 * the verb's own, or 0 (every one) when none fits it -- lib_co_pre400()'s
 * recount.
 */
static scr_int
lib_co_effective_mode (scr_gameref_t game, const scr_char *term,
                       scr_int mode)
{
  const scr_int room = gs_playerroom (game);
  scr_int object;

  for (object = 0; mode != 0 && object < gs_object_count (game); object++)
    {
      if (lib_co_candidate (game, object, room)
          && lib_co_object_answers_to (game, object, term)
          && lib_co_mode_fitted (game, object, mode))
        return mode;
    }
  return 0;
}

/*
 * Reproduce the scan.  Returns TRUE when the Runner would prompt, with
 * *prompt_term the last flagged object's term, *list_term the term the list
 * was built from (the first ambiguous object's), and *present that first
 * object's count of present namesakes.
 */
scr_bool
lib_runner_co_scan (scr_gameref_t game, const scr_char *command,
                    const scr_char **prompt_term, const scr_char **list_term,
                    scr_int *present_count)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, room;
  const scr_char *flagged_term = nullptr, *first_term = nullptr;
  scr_int first_present = 0;
  scr_bool resolved = FALSE;

  if (!command)
    return FALSE;
  room = gs_playerroom (game);

  /*
   * Deliberate deviation: the Runner counts every present namesake, so with
   * a red hat held and a blue one on the floor `drop hat` is "Which hat?"
   * (run380 before dropping, run390 after dropping the red hat).  Scarier
   * counts only the ones the verb can act on, as lib_co_pre400() does, and
   * a single one needs no asking.
   */
  const scr_int mode = lib_co_verb_mode (command);

  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *shortname, *term;
      scr_int alias_count, alias, other, present;

      shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");

      /* Pick the term the Runner would have matched on: Short, then Alias. */
      term = nullptr;
      if (lib_co_contains (command, shortname))
        term = shortname;
      else
        {
          scr_vartype_t vt_key[4];

          alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
          for (alias = 0; alias < alias_count; alias++)
            {
              const scr_char *alias_name;

              vt_key[3].integer = alias;
              alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
              if (lib_co_contains (command, alias_name))
                {
                  term = alias_name;
                  break;
                }
            }
        }
      if (!term || lib_co_term_shadowed (game, command, object, term))
        continue;

      /* Count present objects whose Short or Alias is exactly that term. */
      const scr_int term_mode = lib_co_effective_mode (game, term, mode);
      present = 0;
      GS_FOR_EACH_OBJECT (game, other)
        {
          if (lib_co_candidate (game, other, room)
              && lib_co_object_answers_to (game, other, term)
              && lib_co_mode_fitted (game, other, term_mode))
            present++;
        }

      if (present > 1)
        {
          if (!first_term)
            {
              first_term = term;
              first_present = present;
            }
          if (lib_co_names_prefix (game, command, object))
            resolved = TRUE;
          else if (!resolved)
            flagged_term = term;
        }
    }

  if (!flagged_term || resolved)
    return FALSE;
  if (prompt_term)
    *prompt_term = flagged_term;
  if (list_term)
    *list_term = first_term;
  if (present_count)
    *present_count = first_present;
  return TRUE;
}

scr_bool
lib_co_ambiguity_prompt (scr_gameref_t game, const scr_char *command)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *prompt_term, *list_term;
  scr_int present, room, object, listed;

  /*
   * 3.7 to 3.9.  run390 runs the same scan from generaltasks() -- co(obj, 0)
   * for every object at 45F346, just before takes()/drops() -- and reads its
   * flag MemVar_468190 at the end of the turn (@4606BD, `(468190 < 0) Or
   * (468198 = 1)`, 468198 being set by the task executor @43F032) to print
   * "Which <term>.  <list>?" @4607BC/460832.  The difference is the seen
   * gate in lib_co_candidate(): troll.taf T64 `drop cup`, with Sid's seen
   * small cup on the bar and the carried empty cup, is "Which cup. The small
   * cup or the empty cup?" in run390 (runner_transcripts/troll).  Scarier
   * counts only the cups the drop can act on and prints the drop
   * (deliberate deviation, see lib_runner_co_scan()).  4.0 has its own
   * handler-scoped prompt.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    return FALSE;
  /*
   * run370 has no scan in generaltasks: co() is called only from therest()
   * (43CC86) and insides(), so a line one of the other handlers settled --
   * `wear hat`, `drop hat`, `x hat` with two hats -- never asks; each of
   * them answers its own way (lib_disambiguate_object_common).  p37TASK,
   * runner_probes/task.run370.pname.rtf /
   * runner_probes/task.run370.pname2.rtf, 2026-09-19.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380
      && disambig.line.prompt_370_blocked)
    return FALSE;
  if (!lib_runner_co_scan (game, command, &prompt_term, &list_term, &present))
    return FALSE;

  /*
   * The whole turn's output goes; only the prompt is shown.  The bare-give
   * completion's "(to Nobody)" is not part of it: the Runner prints that
   * echo as it rewrites the line (run390 45FAB9 -> 47B568), so `give hat`
   * with two hats is "(to Nobody)" and then "Which hat.  The red hat or
   * the blue hat?" (p39TASK, run390x runner_probes/task.run390.pclear.txt,
   * 2026-09-19).
   */
  {
    const std::string echo (pf_leading_reference (filter));

    pf_empty (filter);
    if (!echo.empty ())
      pf_rebuffer_reference (filter,
                             echo.substr (1, echo.size () - 3).c_str ());
  }
  lib_which_head (game, prompt_term, nullptr);

  room = gs_playerroom (game);
  listed = 0;
  const scr_int list_mode
    = lib_co_effective_mode (game, list_term, lib_co_verb_mode (command));
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!lib_co_candidate (game, object, room)
          || !lib_co_object_answers_to (game, object, list_term)
          || !lib_co_mode_fitted (game, object, list_mode))
        continue;

      if (listed > 0)
        pf_buffer_string (filter, listed == present - 1 ? " or " : ", ");
      else
        pf_new_sentence (filter);
      lib_print_object_np (game, object);
      listed++;
    }
  pf_buffer_string (filter, "?");
  pf_buffer_answer_break (filter);

  /*
   * 3.9 takes an answer.  The prompt leaves Short & "|" & line in
   * MemVar_4681D0 (460810; the Alias form 460886) and generaltasks, on a next
   * line nothing answered (460022), splices the answer into the line where
   * the term was and runs that: `wear hat` / `red` is "You put on the red
   * hat.", `open box` / `red` is "The red box is already open!", `close
   * box` / `red box` closes it.  An answer that leaves it ambiguous just
   * asks again: `remove hat` / `hat` and `wear hat` / `zzz` (the line
   * becomes `wear zzz hat`); `open box` / `hat` is "Which box.  The red hat
   * or the blue hat?" from the rerun `open hat box` (the list is built once
   * per turn, the term is the last flagged).  A line something answers
   * drops the question (`close box` / `look`).  3.7/3.8 store nothing
   * (run380 4432AA, run370 43C997).  See lib_battle_who_continuation().
   * p39TASK run390x runner_probes/task.run390.pfx.txt, 2026-09-19.
   */
  if (prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_390)
    lib_battle_who_store (std::string (prompt_term) + "|" + command);
  return TRUE;
}

/*
 * lib_prepass_seen_3738()
 *
 * 3.7/3.8 generaltasks opens every line element with a pass over the object
 * table that counts the objects the line names -- by Short or Alias,
 * anywhere in the game (run370 43B502, run380 co() at 441D5D).  Exactly one
 * makes it the antecedent and stops there.  Any other count -- none, or two
 * and more -- runs a second loop (run370 43B6C6, run380 441F21) whose tail
 * sits outside its name test and stamps the seen byte on EVERY present
 * object: dynamic ones held, worn or loose on the player's floor, statics
 * whose room array covers the player's room (run370 43B8C3-43B918, run380
 * 442124-442179).  Nothing inside a container or on a surface is touched.
 *
 * So `n` marks everything in the room the player is walking out of, while
 * `frob stone`, naming one object, leaves the stone unknown.  Measured on
 * p38EXAM/p37EXAM (runner_probes/exam.run380.seena.rtf,
 * runner_probes/exam.run380.seenb.rtf,
 * runner_probes/exam.run370.seena.rtf,
 * runner_probes/exam.run370.seenb.rtf): `frob stone` twice in the start
 * room is "What stone?" both times; after `n` the stone is known ("You
 * must be in the same room as the stone ..." in 3.8), and `frob coin`, the
 * coin inside the crate, is still "What coin?".
 */
void
lib_prepass_seen_3738 (scr_gameref_t game, const scr_char *command)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, room, named;

  if (!command || prop_get_taf_version (bundle) >= TAF_VERSION_390)
    return;

  named = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;
      scr_bool names;

      names = lib_co_contains (command, prop_get_indexed_string
                                          (bundle, "Objects", object, "Short"));
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; !names && alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          names = lib_co_contains (command,
                                   prop_get_string (bundle, "S<-sisi", vt_key));
        }
      if (names)
        named++;
    }
  if (named == 1)
    return;

  room = gs_playerroom (game);
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (gs_object_seen (game, object))
        continue;
      if (obj_is_static (game, object)
          ? obj_directly_in_room (game, object, room)
            || (!gs_object_static_unmoved (game, object)
                && gs_object_position (game, object) == OBJ_HELD_PLAYER)
          : gs_object_position (game, object) == OBJ_HELD_PLAYER
            || gs_object_position (game, object) == OBJ_WORN_PLAYER
            || gs_object_position (game, object) == room + 1)
        gs_set_object_seen (game, object, TRUE);
    }
}

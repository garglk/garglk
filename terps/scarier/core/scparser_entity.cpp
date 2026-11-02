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
 * %object% and %character% matching: the strict and the Runner-tolerant
 * reference comparators, whole-word containment, and the per-game cache of
 * match candidates.  See scparser_internal.h.
 */

#include "scparser_internal.h"


/*
 * Strict %object% / %character% matching.
 *
 * MEASURED 2026-08-25 on p4BURN.taf under run400
 * (runner_probes/burn.run400.txt and runner_probes/burn.run400.b.txt, every
 * command echoed).  A task command pattern is matched by the Runner at 458BBC:
 * it takes the lowercased pattern, does a binary Replace() of "%object%" with
 * the object's Short -- or, in the loop just below, one of its Aliases --
 * VERBATIM, and compares the result for exact equality with the lowercased
 * input.  Nothing else takes part: no Prefix, no article, no partial name, and
 * no case folding of the name itself.
 *
 *   task "PX %object%", object Short "coin"        px coin  -> runs
 *                                                  PX coin  -> runs
 *   task "pa %object%", object Short "Widget"      pa widget -> no
 *                                                  pa Widget -> no
 *   task "pa %object%", Short "brass key",         pa brass key       -> runs
 *                       Prefix "a small"           pa key             -> no
 *                                                  pa a brass key     -> no
 *                                                  pa the brass key   -> no
 *                                                  pa small brass key -> no
 *   task "pa %object%", Short "gem", Alias "jewel" pa gem   -> runs
 *                                                  pa jewel -> runs
 *                                                  pa a gem -> no
 *
 * So an object whose Short carries a capital letter can never bind a bare
 * %object% -- which is why The X-Files' task 24, `Burn %object%` over objects
 * named "Memo", "Coffee Mug" and "Gun Holster", never fires in the Runner
 * however the player phrases it.  Bisecting the game itself confirmed it:
 * lowering the verb and the Short together makes `burn memo` run and print
 * the CompleteText (runner_probes/xfiles.run400.bisect.txt).
 *
 * %character% is NOT affected: its half of the same matcher (46918F) runs the
 * NPC Name and each Alias through LCase() before the Replace(), where the
 * object half (458BF4) substitutes the Short raw.  The asymmetry is the whole
 * bug, and it is one-sided -- ADRIFTMAS Party's `[kiss {the} %character%]`
 * over an NPC named "Mystery" runs in the Runner, and still runs here.
 *
 * MEASURED 2026-08-25 on p39CASE.taf under run390
 * (runner_probes/case.run390.txt, all 19 commands echoed): 3.90 substitutes
 * just as strictly, but it DOES fold case.  The same cells, one Runner down --
 *
 *   task "pa %object%", Short "Widget"          pa widget -> runs
 *                                               pa Widget -> runs
 *   task "pa %object%", Short "brass key",      pa brass key       -> runs
 *                       Prefix "a small"        pa key             -> no
 *                                               pa a brass key     -> no
 *                                               pa the brass key   -> no
 *                                               pa small brass key -> no
 *   task "pa %object%", Short "gem",            pa gem   -> runs
 *                       Alias "jewel"           pa jewel -> runs
 *                                               pa a gem / pa the gem -> no
 *
 * -- so strict binding starts at 3.90, and only the case fold is lost at 4.0.
 * run390 does it in Form1.frm:13991ff, through c() and the seen byte at
 * .global_44, rewriting the task command in place.
 *
 * Before 3.90 the substitution is a different animal, and an earlier draft of
 * this comment had it wrong: the UTF-16 string census finds "%object%" in all
 * four Runners (only "%character%" is missing below 3.90).  run370 and run380
 * do substitute, but they pick ONE object by the line rather than by the
 * pattern's position -- see run_pre400_substitute_references() -- and then
 * compare the rewritten pattern against the whole line, so the strict flag
 * never reaches this matcher there.  Measured on p37CHREF/p38CHREF
 * (runner_probes/chref.run370.b.rtf, runner_probes/chref.run380.b.rtf,
 * 2026-09-20): `nurb rock` runs the task, `nurb a big rock` and `nurb big
 * rock` do not.
 *
 * This is task-command matching only.  The library's own patterns and the
 * variable functions go through the Runner's noun resolver, which is
 * prefix- and case-tolerant, so the flag is set only around
 * run_match_task_commands().
 *
 * uip.match.strict_is_character is set by uip_match_entity() for the
 * duration of a call, because uip_compare_reference_strict() cannot
 * otherwise tell an NPC from an object.
 */


/*
 * uip_set_strict_reference()
 *
 * Set the strict %object%/%character% comparison, and whether it folds case
 * (cleared for 3.90, which lower-cases the name it substitutes), around task
 * command matching; see above.
 */
void
uip_set_strict_reference (scr_bool strict, scr_bool match_case)
{
  uip.options.strict_reference = strict;
  uip.options.strict_case = match_case;
}


/*
 * uip_set_containment()
 *
 * Whole-word containment for a trailing %object% (see uip_match_entity()).
 * Off by default; run_all_commands() turns it on for one last library pass
 * after every positional pass has declined, so that a verb whose pattern
 * binds in place ("take off %object%") is never pre-empted by a shorter one
 * ("take %object%") reaching past "off" to the object.
 */
void
uip_set_containment (scr_bool enabled)
{
  uip.options.containment = enabled;
}


/*
 * uip_is_word_end()
 *
 * Punctuation that ends a name the way a space does.  Every Runner's c()
 * accepts a hit followed by a space or the end of the line; run370 423C80
 * and run380 429048 also accept a ",", and run390 4334B0 a "," or a ".".
 * That only shows where the splitter leaves the punctuation in the line:
 * 3.8 never splits on a comma, so `x stone, look` examines the stone
 * (runner_probes/ask.run380.split.rtf), and 3.9 never cuts at a period with no
 * space after it, so `x stone.` and `x stone.look` do
 * (runner_probes/ask.run390.split.txt).  4.0 is untouched: it cuts a line's
 * final period off before the parser sees it (run_find_split_400).  Held in
 * uip.match.word_end_punctuation, set per match from the game's version in
 * uip_match_pattern().
 */

static scr_bool
uip_is_word_end (scr_char character)
{
  return character == NUL || scr_isspace (character)
         || strchr (uip.match.word_end_punctuation, character) != nullptr;
}


/*
 * uip_compare_reference_strict()
 *
 * The strict comparator described above: the name must appear at the current
 * position exactly as authored -- 4.0 -- or bar its case -- 3.90 -- against a
 * lowercased view of the input, and must end on a word boundary.  Returns the
 * new position on match, else zero.
 *
 * A %character% folds case at 4.0 as well, because run400's matcher LCase()s
 * whichever of the Name (4691B4) or the Alias (469207) it is about to put in
 * place of the reference, where the %object% arm substitutes the Short raw.
 * The asymmetry is one-sided and it is the Runner's, not ours.
 */
static scr_int
uip_compare_reference_strict (const scr_char *name)
{
  scr_int wpos, posn;

  posn = uip.match.posn;
  for (wpos = 0; name[wpos] != NUL; wpos++, posn++)
    {
      const scr_char wanted = uip.options.strict_case
                              && !uip.match.strict_is_character
                              ? name[wpos] : scr_tolower (name[wpos]);

      if (wanted != scr_tolower (uip.match.string[posn]))
        return 0;
    }

  if (uip_is_word_end (uip.match.string[posn]))
    return posn;
  return 0;
}


/*
 * uip_compare_reference()
 *
 * Helper for %character% and %object% matchers.  Matches multiple words
 * if necessary, at the current position.  Returns zero if the string
 * didn't match, otherwise the length of the current position that matched
 * the words passed in (the new value of uip.match.posn on match).
 */
static scr_int
uip_compare_reference (const scr_char *words)
{
  scr_int wpos, posn;

  /* Skip articles and lead in space on words and string. */
  wpos = uip_skip_article (words, 0);
  posn = uip_skip_article (uip.match.string, uip.match.posn);

  /*
   * A name with nothing after its article matches nothing.  At the end of
   * the input it used to compare one terminator with the other, find them
   * equal, and step past both.
   */
  if (words[wpos] == NUL)
    return 0;

  /* Match characters from words with the string at position. */
  while (TRUE)
    {
      /* Any character mismatch means no words match. */
      if (scr_tolower (words[wpos]) != scr_tolower (uip.match.string[posn]))
        return 0;

      /* Move to next character in each. */
      wpos++;
      posn++;

      /*
       * If at space, advance over whitespace in words list.  Stop when we
       * hit the end of the words list.
       *
       * Whitespace at the very end of a name is not forgiven by the real
       * Runner: its c() matches the RAW stored Short/Alias with InStr and
       * requires the character after the match to be a space, comma or
       * end-of-input (run380.bas '429048; run390's c() LCases but keeps the
       * same shape; the 4.0 strict comparator above already refuses), so a
       * name authored with a trailing space can only match input holding
       * two consecutive spaces -- which scr_normalize_string() never
       * delivers.  Measured live 2026-08-31 on superliam.taf (3.80): object
       * Short "necko wafers " makes `take necko wafers` answer "Take
       * what?" in run380.exe, while the (task-matched) `eat necko wafers`
       * still works.  Deliberate deviation: Scarier forgives it outside
       * strict task matching, so the wafers can be taken.
       */
      while (scr_isspace (words[wpos]) && words[wpos] != NUL)
        wpos++;
      if (words[wpos] == NUL)
        break;

      /*
       * About to match another word, so advance over whitespace in the
       * current string too -- which must be there.  Every Runner matches
       * a name with InStr against the line and whole-word edges (run400
       * 454CB0), so "mailbox" is not the Short "mail box": chasingrussian
       * T8 `x mailbox` is "You see no such thing." (run400x,
       * runner_transcripts/chasingrussian.txt).
       */
      if (scr_isspace (words[wpos - 1])
          && !scr_isspace (uip.match.string[posn]))
        return 0;
      while (scr_isspace (uip.match.string[posn])
             && uip.match.string[posn] != NUL)
        posn++;
    }

  /*
   * We reached the end of words.  If we're at the end of the match string, or
   * at spaces, we've matched.
   */
  if (uip_is_word_end (uip.match.string[posn]))
    return posn;

  /* More text after the match, so it's not quite a match. */
  return 0;
}


/*
 * Cached %object% and %character% match candidates.
 *
 * The reference matchers historically re-read every entity's Prefix, Name/
 * Short and Alias properties from the bundle and re-composed the "prefix
 * name" comparison string on every %object%/%character% match attempt --
 * per entity, per attempt, per input line -- which profiled as the largest
 * single share of parsing.  Those properties are fixed once the game is
 * loaded (the only runtime-mutable properties are the player's name and
 * gender globals, which the matchers never read), so build each game's
 * candidate list once and reuse it.
 *
 * Each candidate also precomputes the lead character (the first character
 * past any article, lowercased) of both its composed prefixed string and
 * its plain name.  uip_compare_reference() cannot match unless the input's
 * own lead character equals one of these, so the matchers use that as an
 * exact one-character rejection test before comparing in full.
 *
 * The cache (uip.cache; the candidate types are scr_uip_candidate_t and
 * scr_uip_entity_t in scparser_internal.h) tracks a single game (SCARE runs
 * one at a time); gs_destroy() calls uip_forget_game() so a later game
 * reusing the same allocation address cannot be mistaken for a cached one.
 */


/*
 * uip_lead_char()
 *
 * Return the lowercased first character of a string past any leading
 * article, mirroring what uip_compare_reference() compares first.
 */
static scr_char
uip_lead_char (const scr_char *string)
{
  return scr_tolower (string[uip_skip_article (string, 0)]);
}


/*
 * uip_build_candidate()
 *
 * Fill in one match candidate from a prefix and a name.
 *
 * The forms are "prefix name", then that same string with the prefix's
 * leading words dropped one at a time, and finally the bare name.  Dropping
 * prefix words is what lets the real Runner answer to a partial prefix:
 * Monsters (Release 2) has Prefix "Sissy's four poster" on Short "bed", and
 * the author's own published transcript shows "examine the four poster bed"
 * returning the object's description.  Matching only the whole prefix or the
 * bare noun -- as this did before -- turned any such line into "I see no such
 * thing".  Only prefix words are droppable; the name itself is never cut
 * down, so a two-word Short still has to be given in full.
 */
static void
uip_build_candidate (scr_uip_candidate_t *candidate,
                     const scr_char *prefix, const scr_char *name)
{
  std::string composed;
  size_t word;

  /* Compose "prefix name". */
  composed.assign (prefix);
  composed.append (1, ' ');
  composed.append (name);

  candidate->forms.clear ();
  candidate->forms.push_back (composed);

  /*
   * Add one form per remaining prefix word boundary.  Walk only as far as the
   * prefix extends -- past that we would be eating into the name.
   */
  for (word = 0; word < strlen (prefix); word++)
    {
      if (!scr_isspace (composed[word]) || scr_isspace (composed[word + 1]))
        continue;

      candidate->forms.push_back (composed.substr (word + 1));
    }

  /*
   * Deliberate deviation: any one Prefix word before the name, too, so
   * `take old pin` finds the "old red" pin.  The Runners' co() and the
   * forms above both need the words to run on up to the name.  Articles
   * are skipped, and a form already present is not added again.
   */
  {
    const size_t prefix_length = strlen (prefix);
    size_t start, end;

    for (start = 0; start < prefix_length; start = end + 1)
      {
        std::string form;

        end = composed.find (' ', start);
        if (end == std::string::npos || end > prefix_length)
          end = prefix_length;
        if (end == start
            || uip_skip_article (composed.substr (start).c_str (), 0) > 0)
          continue;

        form = composed.substr (start, end - start) + ' ' + name;
        if (std::find (candidate->forms.begin (), candidate->forms.end (),
                       form) == candidate->forms.end ())
          candidate->forms.push_back (form);
      }
  }

  /* The bare name, unless a wholly empty prefix already made it form 0. */
  if (candidate->forms.back () != name)
    candidate->forms.push_back (name);

  candidate->plain = name;

  candidate->leads.clear ();
  for (word = 0; word < candidate->forms.size (); word++)
    {
      const scr_char *form = candidate->forms[word].c_str ();

      candidate->leads.append (1, uip_lead_char (form));
    }
}


/*
 * uip_build_entities()
 *
 * Build the candidate list for one entity class ("Objects" with "Short"
 * names, or "NPCs" with "Name" names), reading the same properties the
 * matchers read before candidates were cached.
 */
static void
uip_build_entities (std::vector<scr_uip_entity_t> &entities,
                    scr_gameref_t game, scr_int count,
                    const scr_char *class_key, const scr_char *name_key)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int index_;

  entities.clear ();
  entities.resize (count);
  for (index_ = 0; index_ < count; index_++)
    {
      scr_uip_entity_t &entity = entities[index_];
      scr_vartype_t vt_key[4];
      const scr_char *prefix, *name;
      scr_int alias_count, alias;

      /* Get the entity's prefix and name. */
      vt_key[0].string = class_key;
      vt_key[1].integer = index_;
      vt_key[2].string = "Prefix";
      prefix = prop_get_string (bundle, "S<-sis", vt_key);
      /*
       * A character's Prefix is 4.0's alone in the Runners.  Below it,
       * characters() knows a character by `c(Name) Or c(Alias(0))` and
       * nothing else (run390 459109), so `x blue guard` at 3.9 is just `x
       * guard` (p39PFX, run390x runner_probes/pfx.run390.txt, 2026-09-20).
       * Scarier builds the forms at every version (deliberate deviation), so
       * the Prefix picks the guard.
       */
      vt_key[2].string = name_key;
      name = prop_get_string (bundle, "S<-sis", vt_key);
      uip_build_candidate (&entity.name, prefix, name);

      /* Add each non-empty alias (3.9 games introduce empty aliases). */
      vt_key[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (scr_strempty (alias_name))
            continue;

          entity.aliases.push_back (scr_uip_candidate_t ());
          uip_build_candidate (&entity.aliases.back (), prefix, alias_name);
        }
    }
}


/*
 * uip_synchronize_cache()
 *
 * Ensure the candidate cache describes the given game.
 */
static void
uip_synchronize_cache (scr_gameref_t game)
{
  if (uip.cache.game == game)
    return;

  uip_build_entities (uip.cache.objects,
                      game, gs_object_count (game), "Objects", "Short");
  uip_build_entities (uip.cache.npcs,
                      game, gs_npc_count (game), "NPCs", "Name");
  uip.cache.game = game;
}


/*
 * uip_forget_game()
 *
 * Drop any candidate cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
uip_forget_game (const void *game)
{
  if (uip.cache.game == game)
    {
      uip.cache.game = nullptr;
      uip.cache.objects.clear ();
      uip.cache.npcs.clear ();
    }
}


/*
 * uip_compare_candidate()
 *
 * Attempt a reference match against each of a candidate's forms, longest
 * first (the fully prefixed name down to the bare one, which is the order
 * uip_build_candidate() stores them in, and which keeps the longest match
 * winning as the matchers always assumed).  Returns the extent of the match,
 * or zero if no match.
 */
static scr_int
uip_compare_candidate (const scr_uip_candidate_t &candidate)
{
  size_t form;

  /*
   * A task command substitutes the bare name, and nothing else -- for a
   * %character% as much as for a %object%.  Both Runners that carry the
   * reference rewrite the command with Replace(): run400 at 4691D8/46922E
   * and run390 at 44AD8A/44B385 put the Name or Alias in place of the
   * pattern's %character%, and neither ever composes a Prefix form.  So
   * p4CHREF's `frob a big dave`, `frob big dave` and `frob the dave`, with
   * Dave's Prefix "a big", all miss the `frob %character%` task in run400
   * and run390 alike and fall to the character catch-all, "I don't
   * understand what you want to do with Dave."
   * (runner_probes/chref.run400.b.txt, runner_probes/chref.run390.b.txt,
   * 2026-09-20).
   */
  if (uip.options.strict_reference)
    return uip_compare_reference_strict (candidate.plain);

  for (form = 0; form < candidate.forms.size (); form++)
    {
      scr_int extent = uip_compare_reference (candidate.forms[form].c_str ());

      if (extent > 0)
        return extent;
    }

  return 0;
}


/*
 * uip_nothing_follows()
 *
 * True when a reference is the last thing its pattern can match: no sibling
 * at all (it closes a group), or only the end-of-string marker.
 */
static scr_bool
uip_nothing_follows (scr_ptnoderef_t node)
{
  scr_ptnoderef_t next = node->right_sibling;

  /*
   * A trailing " *" counts as nothing: the wildcard matches the empty
   * string, so a containing reference that has consumed the whole line
   * still satisfies "pull %object% *".  Vardock Bates (4.00, run400
   * transcript 2026-08-29): `tirar de la palanca` -> synonym -> `pull de la
   * palanca` answers "You pull la palanca, but nothing happens." -- the
   * lever found by co() -- where we fell to "You pull, but nothing happens."
   */
  while (next && (next->type == NODE_WHITESPACE || next->type == NODE_WILDCARD))
    next = next->right_sibling;

  return !next || (next->type == NODE_EOS && !next->right_sibling);
}


/*
 * uip_contains_words()
 *
 * True if 'words' occurs, case-insensitively and on word boundaries, anywhere
 * in the input at or after the current position.
 */
static scr_bool
uip_contains_words (const scr_char *words)
{
  const size_t length = strlen (words);
  scr_int posn;

  if (length == 0)
    return FALSE;

  for (posn = uip.match.posn; uip.match.string[posn] != NUL; posn++)
    {
      if (posn > 0 && !scr_isspace (uip.match.string[posn - 1]))
        continue;
      if (scr_strncasecmp (uip.match.string + posn, words, length) != 0)
        continue;
      if (uip_is_word_end (uip.match.string[posn + length]))
        return TRUE;
    }

  return FALSE;
}


/*
 * uip_match_remainder()
 *
 * Helper for %character% and %object% matchers.  Matches the remainder
 * of a pattern, to resolve the difference between, say, "table leg" and
 * "table".
 */
static scr_bool
uip_match_remainder (scr_ptnoderef_t node, scr_int extent)
{
  scr_ptnode_t list = {};
  scr_int start_posn;
  scr_bool matched;

  /*
   * If the reference is the last element of its list there is nothing left to
   * match, and the remainder is vacuously satisfied.  This happens whenever a
   * %character% or %object% closes a choice or optional group, as in the very
   * common idioms "[kiss {the} %character%]" and
   * "[smack/hit/punch/kick]{the}[%character%]".  Without this the temporary
   * list below is empty, uip_match_list() fails it by design, and the
   * reference can never match -- a divergence from the real Runner, which
   * matches both of those (verified against run400.exe on ADRIFTMAS Party).
   * At the top level the case does not arise: uip_parse_list() appends a
   * NODE_EOS there, so end-of-string is still enforced after the group.
   */
  if (!node->right_sibling)
    return TRUE;

  /* Note the start position, then advance to the given extent. */
  start_posn = uip.match.posn;
  uip.match.posn = extent;

  /*
   * Try to match everything after the node passed in, at this position in the
   * string.
   */
  list.type = NODE_LIST;
  list.left_child = node->right_sibling;

  /* Match on the temporary list, and restore position. */
  matched = uip_match_node (&list);
  uip.match.posn = start_posn;

  /* Return TRUE if the pattern remainder matched. */
  return matched;
}


/*
 * uip_case_folds_name_in()
 * uip_case_folds_name()
 *
 * The last thing the Runner's character resolver does before it answers
 * "yes, this command names that character".
 *
 * run400 Proc_21_40_45E99C picks the Name -- or, failing that, the LAST
 * matching Alias (loc_45E623..45E67D assigns without breaking) -- with the
 * case-insensitive whole-word test Proc_21_38_454CB0, lower-cases it into
 * var_98 at loc_45E6A6..45E6B2, and then returns
 *
 *     InStr(1, cmd, var_98, 0)            ' loc_45E743, 45E8B3, 45E938
 *
 * with compare mode 0 = vbBinaryCompare.  That final test is CASE-SENSITIVE
 * against the live command line, and it is a plain substring, the word
 * boundaries having already been settled by the selection above.
 *
 * Normally it can never fail: the whole typed line was lower-cased before
 * the parser ever saw it (run_player_input(), run400 loc_45C5DC), so a name
 * that matched case-insensitively matches case-sensitively too.  The one
 * way upper case gets back into a command is the game's own SYNONYM table,
 * which rewrites whole words AFTER the LCase and splices the author's
 * replacement text in verbatim.  A replacement that carries a capital
 * therefore makes that character permanently unreferenceable by any library
 * command -- only the author's own tasks, which match case-insensitively,
 * can still reach them.
 *
 * Measured 2026-09-07 on Bandera.taf (4.00), whose SYNONYM table maps
 * marife/Marife/marife' all to the capitalised "Marife'": run400 answers
 * `x marife`, `x Marife` and `x MARIFE` alike with the ALR'd "You see no
 * such thing." (runner_transcripts/bandera.txt,
 * runner_probes/bandera.run400.case.txt), while `hablar con marife` and `besar
 * a marife` reach her tasks.  Repacking the same game with the five
 * replacements lower-cased makes `x marife` print her description
 * (runner_probes/bandera.run400.lc.txt) -- the capital is the whole cause.
 *
 * Objects are not affected: the Runner resolves them through co()
 * (Proc_21_39_46486C), which has no such trailing binary compare.
 */
scr_bool
uip_case_folds_name_in (const scr_char *command, const scr_char *name)
{
  std::string wanted (name);

  scr_lowercase (&wanted[0]);

  return !wanted.empty ()
         && strstr (command, wanted.c_str ()) != nullptr;
}

static scr_bool
uip_case_folds_name (const scr_char *name)
{
  /* Deliberate deviation: see uip_set_lenient_tasks(). */
  if (uip.options.lenient_tasks)
    return TRUE;
  return uip_case_folds_name_in (uip.match.string, name);
}


/*
 * uip_entity_admitted()
 *
 * The admission gate of uip_match_entity(): may entity `index` be compared
 * with the input at all in this scope and pass?  Every test below is a
 * measured Runner rule; none of them looks at the typed text.
 */
static scr_bool
uip_entity_admitted (scr_gameref_t game, scr_bool is_character, scr_int index,
                     scr_int scope, scr_bool strict_scoped, scr_int pass)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));

  /*
   * A task command's %object% binds only an object the player has seen:
   * run400's matcher skips any object whose seen byte is clear (458E6C,
   * gate on [48]), and so does run390's checktask binding (44ABEA,
   * [44]).  Glum Fiddle `take cushion`, with the cushion lying unlisted
   * on the pile of boulders, so misses task 19 `[take/get/pick up]
   * %object%` and the library answers "Take what?"
   * (runner_probes/glum_fiddle.run400.seen.txt T16).
   */
  if (uip.options.strict_reference && !is_character
      && !gs_object_seen (game, index))
    return FALSE;

  /*
   * 458E6C's own argument: scope 0 is the present pass, scope 1 the absent
   * one, and scope 1 runs only when scope 0 bound nothing.
   */
  if (strict_scoped)
    {
      const scr_bool present =
          obj_indirectly_in_room (game, index, gs_playerroom (game));

      if (scope == 0 ? !present : present)
        return FALSE;
    }

  /*
   * A task command's %character% has a seen gate of its own at 4.0, and
   * it is the SEEN BYTE ALONE: run400's %character% matcher (468DFC,
   * loop 469162) admits an NPC on `CInt(npc.global_26) = 1` -- field 26,
   * the byte npc_in_command() reads as var_DC(26) -- and tests no room
   * at all.  So a character the player has met and walked away from
   * still binds, and one never met never does.  p4CHREF `frob eve`,
   * with Eve in the Cave and unmet, is "I don't understand."; after a
   * `n` and an `s` the same line runs the task, and `frob dave` typed
   * in the Cave runs it on the Dave left behind in the Lit Room
   * (runner_probes/chref.run400.b.txt, 2026-09-20).  This is the gate behind
   * xfiles `look up byers` against task "Look up *%character%*": at the FBI
   * parking garage the Lone Gunmen have not been met, so run400 answers with
   * examines' "You see no such thing." (runner_transcripts/xfiles.txt).
   *
   * run390's checktask has no gate whatever -- its loops at 44AD48 and
   * 44B323 walk the whole NPC array -- so 3.9 binds a character who is
   * nowhere at all: p39CHREF `frob fay`, Fay having been given no start
   * room, runs the task (runner_probes/chref.run390.b.txt).
   */
  if (uip.options.strict_reference && is_character && version >= TAF_VERSION_400
      && !gs_npc_seen (game, index))
    return FALSE;

  /* npc_in_command mode 0: only a present, seen character binds. */
  if (pass > 0 && is_character && version >= TAF_VERSION_400
      && !(gs_npc_seen (game, index)
           && npc_in_room (game, index, gs_playerroom (game))))
    return FALSE;

  return TRUE;
}


/*
 * uip_record_entity_match()
 *
 * Save a match in variables and game: reference the entity, and store it as
 * the referenced character or object unless a 4.0 task command has already
 * bound its first.
 */
static void
uip_record_entity_match (scr_gameref_t game, scr_var_setref_t vars,
                         scr_bool is_character, scr_int index,
                         std::vector<scr_bool> &references,
                         scr_bool strict_scoped, scr_bool &strict_first_bound)
{
  /*
   * Save match in variables and game.  A 4.0 task command's
   * %character% keeps the FIRST NPC in index order: run400's
   * matcher (468DFC, loop 469162) leaves for 469574 on the first
   * compare that succeeds and stores that index in 49420A.
   * iqsfot T158 `kick guard` against task 1339 `*kick
   * *%character%*`, with Drash the Guard (NPC 7, alias guard) and
   * a guard (NPC 15) both absent: run400 fails "%character% is
   * not here." as "Drash the Guard is not here."
   * (runner_transcripts/iqsfot.txt:1240), not "guard is not here."
   *
   * A 4.0 task command's %object% keeps the FIRST object in
   * index order too, within the scope pass that found it:
   * 458E6C Exit Subs on the hit.  p4OBJREF `nurb rock` in the
   * Lit Room, rocks 0 "a big" and 2 "a red" both present and
   * seen, is "NURBED a big rock." (runner_probes/objref.run400.txt).
   */
  if (!(uip.options.strict_reference && strict_first_bound))
    {
      if (is_character)
        var_set_ref_character (vars, index);
      else
        var_set_ref_object (vars, index);
    }
  references[index] = TRUE;
  if ((is_character || strict_scoped)
      && prop_get_taf_version (gs_get_bundle (game)) >= TAF_VERSION_400)
    strict_first_bound = TRUE;
}


/*
 * uip_candidate_extent()
 *
 * Match one name or alias of a %character% or %object% candidate (alias -1
 * for the name itself) against the input: in place at uip.match.posn on pass
 * 0, by whole-word containment anywhere in the line on pass 1, when a match
 * takes the rest of the input (input_end).  input_lead is the input's lead
 * character, which a positional match's leads must include.  Returns the
 * match's extent, or 0 for no match.
 */
static scr_int
uip_candidate_extent (const scr_uip_candidate_t &candidate, scr_int alias,
                      scr_bool is_character, scr_int pass,
                      scr_char input_lead, scr_int input_end)
{
  scr_int extent;

  /*
   * A 3.9 task's %character% is the Name and nothing else.  run400
   * substitutes the Name (4691A9) and then walks the Alias array (4691F8);
   * run390's checktask reads `.global_0` only, in both of its loops (44AD5C,
   * 44B334), and never touches the alias.  Eve is aliased "spook" in
   * p39CHREF: run390 answers `frob spook` with the library -- "Who?" before
   * she is met, "Eve is not here!" after -- where run400 runs the task
   * (2026-09-20).
   */
  if (uip.options.strict_reference && is_character && alias >= 0
      && prop_get_taf_version (gs_get_bundle (uip_get_game ()))
         < TAF_VERSION_400)
    return 0;

  if (uip.trace)
    scr_trace ("UIParser: trying %s%s\n",
               alias < 0 ? "" : "alias ", candidate.plain);

  if (pass == 0)
    {
      if (!uip.options.strict_reference
          && candidate.leads.find (input_lead) == std::string::npos)
        return 0;

      extent = uip_compare_candidate (candidate);
    }
  else
    extent = uip_contains_words (candidate.plain) ? input_end : 0;

  /*
   * A character has to survive the resolver's case-sensitive tail test as
   * well -- see uip_case_folds_name().  Task commands go through a different
   * Runner routine and are exempt.
   */
  if (extent > 0 && is_character && !uip.options.strict_reference
      && !uip_case_folds_name (candidate.plain))
    extent = 0;

  return extent;
}


/*
 * uip_drop_shorter_matches()
 *
 * After a library command's %character% or %object% has matched, unreference
 * every entity whose match was shorter than max_extent, and make the last one
 * left the reference proper.
 *
 * Deliberate deviation: in a library command the match continues from the
 * longest extent, so a shorter name that also matched is not what the player
 * typed: `take key ring` with a "key" and a "key ring" here is the key ring.
 * The Runners' co() counts every Short the line contains and gives up on the
 * pair ("Take what?" below 4.0, "It is not clear which key ring you are
 * referring to." at 4.0).
 */
static void
uip_drop_shorter_matches (scr_var_setref_t vars, scr_bool is_character,
                          std::vector<scr_bool> &references,
                          const std::vector<scr_int> &matched_extent,
                          scr_int max_extent)
{
  const scr_int entity_count = matched_extent.size ();
  scr_int index, last;

  last = -1;
  for (index = 0; index < entity_count; index++)
    {
      if (matched_extent[index] > 0 && matched_extent[index] < max_extent)
        references[index] = FALSE;
      if (references[index])
        last = index;
    }
  if (last != -1)
    {
      if (is_character)
        var_set_ref_character (vars, last);
      else
        var_set_ref_object (vars, last);
    }
}


/*
 * The sweeps uip_match_entity() makes over the cached candidates: what the
 * match is looking at, and what has matched so far.
 */
namespace {

struct uip_entity_sweep_t
{
  scr_ptnoderef_t node;
  scr_bool is_character;
  scr_gameref_t game;
  scr_var_setref_t vars;
  const std::vector<scr_uip_entity_t> &cache;
  std::vector<scr_bool> &references;
  scr_char input_lead;
  scr_int input_end;
  scr_bool strict_scoped;
  std::vector<scr_int> matched_extent;
  scr_bool strict_first_bound;
  scr_int max_extent;

  void run (scr_int scope, scr_int pass);
  void drop_unknown ();
};


/* One sweep: reference every admitted entity a name or alias of which
   matches, and note the extents. */
void
uip_entity_sweep_t::run (scr_int scope, scr_int pass)
{
  const scr_int entity_count = cache.size ();
  scr_int index;

  for (index = 0; index < entity_count; index++)
    {
      const scr_uip_entity_t &entity = cache[index];
      scr_int alias_count, alias;

      if (!uip_entity_admitted (game, is_character, index,
                                scope, strict_scoped, pass))
        continue;

      /*
       * Compare the entity's name, then each of its aliases, both
       * prefixed and not.  Alias -1 stands for the name itself.
       */
      alias_count = entity.aliases.size ();
      for (alias = -1; alias < alias_count; alias++)
        {
          const scr_int extent =
              uip_candidate_extent (alias < 0 ? entity.name
                                              : entity.aliases[alias],
                                    alias, is_character, pass,
                                    input_lead, input_end);

          if (extent > 0 && uip_match_remainder (node, extent))
            {
              if (uip.trace)
                scr_trace ("UIParser: matched (pass %ld, extent %ld)\n",
                           (long) pass, (long) extent);

              /* Increase the maximum match extent if required. */
              max_extent = (extent > max_extent) ? extent : max_extent;
              if (extent > matched_extent[index])
                matched_extent[index] = extent;

              uip_record_entity_match (game, vars, is_character,
                                       index, references,
                                       strict_scoped,
                                       strict_first_bound);
            }
        }
    }
}


/*
 * A name matched in place is no match when nothing it names has ever been
 * seen and none of it is here: run400's resolver (463640) scores present,
 * seen objects only, so the line is left to whatever else it contains.
 * Scene of the Crime (4.00) has "a small card" in hand and "a tiny card"
 * (alias "tiny card") not yet in play: `x tiny card` describes the small
 * card (run400x, Delron walkthrough replay 2026-10-09), where the in-place
 * match answered "I see no such thing.".  A name seen elsewhere keeps the
 * line (not measured), and below 4.0 is not measured.
 */
void
uip_entity_sweep_t::drop_unknown ()
{
  const scr_int entity_count = cache.size ();
  scr_int index;
  const scr_int room = gs_playerroom (game);
  scr_bool is_known = FALSE;

  for (index = 0; index < entity_count && !is_known; index++)
    {
      is_known = matched_extent[index] > 0
                 && (gs_object_seen (game, index)
                     || obj_indirectly_in_room (game, index, room)
                     || obj_indirectly_held_by_player (game, index));
    }
  if (!is_known)
    {
      for (index = 0; index < entity_count; index++)
        {
          if (matched_extent[index] > 0)
            {
              references[index] = FALSE;
              matched_extent[index] = 0;
            }
        }
      max_extent = 0;
    }
}

} /* namespace */


/*
 * uip_match_entity()
 * uip_match_character()
 * uip_match_object()
 *
 * Match a %character% or an %object% reference.  These search all of the NPC
 * or object names and aliases for possible matches, and set the game's
 * npc_references or object_references flag for any that match.  One of them
 * is also stored in variables as the reference proper: normally the final
 * one to match, but a 4.0 task command's %character% or %object% keeps the
 * first, and its %object% weighs scope before index.
 */
static scr_bool
uip_match_entity (scr_ptnoderef_t node, scr_bool is_character)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  scr_char input_lead;

  if (uip.trace)
    scr_trace ("UIParser: attempting to match %s\n",
               is_character ? "%character%" : "%object%");

  uip.match.strict_is_character = is_character;

  /* Clear all current references. */
  if (is_character)
    gs_clear_npc_references (game);
  else
    gs_clear_object_references (game);

  /*
   * Before 3.90 a task command's %character% matches nothing whatever the
   * player types.  Neither run370.exe nor run380.exe holds the literal
   * "%character%" at all -- the UTF-16 string census finds one copy in
   * run390.exe and one in run400.exe and none in the two older Runners,
   * where "%object%" is present in all four -- so their checktask never
   * rewrites the command and the pattern can only meet a line spelling the
   * reference out.  p37CHREF and p38CHREF answer `frob dave` "I don't
   * understand." with Dave standing in the room and seen, and answer every
   * other cell of the p4CHREF feed the same way
   * (runner_probes/chref.run370.b.rtf, runner_probes/chref.run380.b.rtf,
   * 2026-09-20).  The %object% half of this has its own pre-3.9 shape, a
   * single substitution chosen by the line rather than by the pattern's
   * position; see run_pre400_substitute_references().
   */
  if (is_character && uip.options.task_commands && version < TAF_VERSION_390)
    return FALSE;

  /*
   * Ensure cached candidates for this game, and get the input's lead
   * character -- a candidate whose own leads differ cannot match.
   */
  uip_synchronize_cache (game);
  input_lead
    = scr_tolower (uip.match.string[uip_skip_article (uip.match.string,
                                                      uip.match.posn)]);

  const std::vector<scr_uip_entity_t> &cache = is_character
                                               ? uip.cache.npcs
                                               : uip.cache.objects;
  std::vector<scr_bool> &references = is_character
                                      ? game->npc_references
                                      : game->object_references;

  /*
   * A trailing %object% in a library command is resolved the way the Runner's
   * co() resolves it -- run400 Proc_21_39_46486C, run380 c() @429048 -- by
   * whole-word containment: an object matches when its Short or any Alias
   * occurs as whole words anywhere in the typed command, not only at the
   * position the pattern has reached.  Measured on man_overboard (4.00) turn
   * 49: `x silver key`, with a "silver key" nowhere in the game but "a key"
   * in the room, examines the key; we answered "You see no such thing."
   * The containing match consumes the rest of the input.  Task commands
   * (strict, 4.0) and characters keep their positional matching, and so
   * does any reference with more pattern after it, where the extent decides
   * what the rest of the pattern sees.
   *
   * 4.0 resolves a trailing %character% by containment as well: run400's
   * per-NPC check npc_in_command() (Proc_21_40_45E99C) LCase()s whichever of
   * the Name or an Alias answers and returns InStr(Me(304), word) -- whole-
   * word containment anywhere in the line, 45E740-45E75B -- and the Prefix
   * contest (lib_npc_400_prefix_score()) then picks among the hits, not the
   * word order.  p4PFX.taf has Ann "a red", Bob "a blue" and Cid "a red" all
   * aliased "guard": run400 answers `x guard blue` with Bob's description
   * (runner_probes/pfx.run400.txt, 2026-09-20), where a positional match sees
   * no candidate at all and we used to say "You see no such thing."
   *
   * Pre-4.0 binds a trailing %character% by containment too -- run390's
   * characters() enters the per-NPC block on `c(Name) Or c(Alias(0))`
   * anywhere in the line (run390 459109, run380 4401AA) -- and, having no
   * contest, simply lets the last NPC in index order win: run390 answers
   * `x blue guard`, `x a blue guard` and a bare `blue guard` with Cid's
   * description, the same as `x guard` (runner_probes/pfx.run390.txt).  What
   * it does NOT have is a "<Prefix> <Name>" form to match positionally;
   * Scarier builds those at every version (deliberate deviation, see
   * uip_build_entities()).
   *
   * At 4.0 only a PRESENT, SEEN character binds.  npc_in_command's loop
   * at 45E6C5
   * counts the characters answering to the matched word that stand in the
   * player's room (var_DC(14) = Me(92)) and are seen (var_DC(26) = 1), and
   * mode 0 -- the mode every reference site uses, the examine scan at
   * 48B556 included -- takes neither early return at 45E740 (`count > 0 And
   * mode = 3`, `count = 0 And mode = 1`).  With one such namesake it falls
   * to 45E8F4, which tests this NPC's own room and seen bytes before
   * returning the containment; with several it runs the contest at 45E892
   * and only the winner returns TRUE; with none it returns FALSE.  Mode 1
   * is the absent tails' own test (the ask block at 47F8E5, the attack tail
   * at 47F40D -- lib_attack_absent_npc()), which rescan the line for
   * themselves, and mode 3, at 48B9A5 alone, answers a raised question.  So
   * an absent character never binds here: mutaydid (4.00) rewrites `butcher
   * mystery meat` to `attack mystery meat` through its synonym table, and
   * although "meat" is an alias of The Mother Meat (asleep on Planet
   * Mutaydid), run400 answers with the object arm, "You can't see the
   * mystery meat." (runner_transcripts/mutaydid.txt).
   */
  const scr_bool contain = uip.options.containment
                           && !uip.options.strict_reference
                           && uip_nothing_follows (node);
  const scr_int input_end = strlen (uip.match.string);

  /*
   * Pass 0 is the positional match.  Pass 1 is the containment fallback,
   * taken only when nothing matched in place, and it takes names and aliases
   * TOGETHER: the Runner's up-front resolver (run400 Proc_21_58_463640, pass
   * 1) scores every present-and-seen object by Short-as-a-whole-word plus
   * alias-as-a-whole-word, and referencedob's own co(i, 3) loop then counts
   * only present namesakes, so an absent object's Short never outranks a
   * present object's Alias.  Measured on escape_to_new_york (4.00) turn 107,
   * `look under table` in Cabin E86: the Runner describes the present
   * "mahogany furniture" (alias "table"); a names-first pass bound the
   * absent "table" (object 50) instead and answered "You see no such
   * thing."  The absent namesakes are still referenced here and left for
   * the handler's disambiguation to drop, which is where the seen-but-
   * absent answers come from.  "unlock iron chest with golden key"
   * (shadowpeak) binds in place and never reaches this pass.
   *
   * A 4.0 task command's %object% is bound by SCOPE first and index second.
   * run400's matcher (458E6C) takes the scope it wants as an argument and
   * tests `obj_indirectly_in_room(i) = arg_14` -- 44B578, -1 where the
   * object stands within reach and 0 where it does not -- so one call sees
   * PRESENT objects only; it Exit Subs on the first object that also passes
   * the seen gate, storing that index in 494208; and the tail at 458E64
   * calls itself again with arg_14 = 0, the ABSENT-but-seen pass.  So the
   * lowest-indexed present namesake wins, and an absent one binds only when
   * no present namesake does.  p4OBJREF has three rocks sharing the Short
   * "rock" with Prefixes "a big" (0, Lit Room), "a small" (1, Cave) and "a
   * red" (2, Lit Room), and task `nurb %object%` printing "NURBED
   * %object%.": run400 answers `nurb rock` in the Lit Room "NURBED a big
   * rock." and the same line in the Cave "NURBED a small rock."
   * (runner_probes/objref.run400.txt, 2026-09-20).  We bound the last namesake
   * to match, "a red rock", in both rooms.
   *
   * Below 4.0 there is no scope test at all: run390's checktask walks the
   * whole object array under `c(name, cmd) And .global_44 = 1` (44AAD6 for
   * the Short, 44AB65 for the Alias) with no break, so the LAST seen
   * namesake wins wherever it stands -- run390 answers all three cells "NURBED
   * a red rock." (runner_probes/objref.run390.txt) -- and pre-3.9 the
   * substitution is the line's own, see run_pre400_substitute_references().
   */
  const scr_bool strict_scoped = uip.options.strict_reference && !is_character
                                 && version >= TAF_VERSION_400;

  uip_entity_sweep_t sweep = {
    node, is_character, game, vars, cache, references, input_lead, input_end,
    strict_scoped, std::vector<scr_int> (cache.size (), 0), FALSE, 0
  };

  for (scr_int scope = 0;
       scope < (strict_scoped ? 2 : 1) && sweep.max_extent == 0; scope++)
    {
      for (scr_int pass = 0; pass < 2 && sweep.max_extent == 0; pass++)
        {
          if (pass > 0 && !contain)
            break;

          sweep.run (scope, pass);

          /* 4.0: a name matched in place that nothing seen or present
             bears is no match; see drop_unknown(). */
          if (pass == 0 && contain && sweep.max_extent > 0 && !is_character
              && version >= TAF_VERSION_400)
            sweep.drop_unknown ();
        }
    }

  if (sweep.max_extent > 0 && !uip.options.strict_reference)
    uip_drop_shorter_matches (vars, is_character, references,
                              sweep.matched_extent, sweep.max_extent);

  /* On match, advance position and return successfully. */
  if (sweep.max_extent > 0)
    {
      uip.match.posn = sweep.max_extent;
      return TRUE;
    }

  /* No match. */
  return FALSE;
}

scr_bool
uip_match_character (scr_ptnoderef_t node)
{
  return uip_match_entity (node, TRUE);
}

scr_bool
uip_match_object (scr_ptnoderef_t node)
{
  return uip_match_entity (node, FALSE);
}

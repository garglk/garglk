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
 * Pronouns: substituting "it", "him", "her" and "them" with their
 * antecedents, the antecedent and last-named-character registers, the
 * ask/talk rewrite and its echo.  See scparser_internal.h.
 */

#include "scparser_internal.h"


/*
 * Antecedent article bookkeeping for the 4.0 pronoun echo.  Set by
 * uip_replace_pronouns() when a pronoun was substituted this turn (the
 * Runner leaves its antecedent alone for such commands), and by the library's
 * unknown-verb handler, whose antecedent is definite; both are consumed and
 * cleared by uip_assign_pronouns().  See uip_definite_form() below.  They
 * are uip.pronoun.used and uip.pronoun.pending_definite.
 */


/*
 * uip_tense_prefix_3738()
 *
 * run370/run380 tense() (run370 420F28, run380 425FA8): a whole "a" becomes
 * "the", and a leading "a " or "an " becomes "the ".  A whole "an", "some"
 * and everything else are left as authored.  Case-sensitive.
 */
static std::string
uip_tense_prefix_3738 (const scr_char *prefix)
{
  if (strcmp (prefix, "a") == 0)
    return "the";
  if (strncmp (prefix, "a ", 2) == 0)
    return std::string ("the ") + (prefix + 2);
  if (strncmp (prefix, "an ", 3) == 0)
    return std::string ("the ") + (prefix + 3);
  return prefix;
}


/*
 * uip_note_definite_reference()
 *
 * The library's unknown-verb handler marks its antecedent definite; see
 * uip.pronoun.pending_definite above.
 */
void
uip_note_definite_reference (void)
{
  uip.pronoun.pending_definite = TRUE;
}


/*
 * The 4.0 antecedent is whatever string the setter Proc_21_41_448C24 was
 * last handed on the line, and more of the Runner calls it than the
 * handlers uip_definite_form() models.  In the order a line meets them:
 *
 *   generaltasks 48A3F5-48A42E  the line's own 463640 winner (mode 0), if
 *                               one scores alone: "the X"
 *   co() 46460F                 the -2 arm, the Prefix contest won by the
 *                               object being resolved: Prefix & " " & Short
 *   co() 464788                 the park arm (Me(424) = object): the bare
 *                               Short, "stone" -- so a later `x it` is `x
 *                               stone` and asks again
 *   examines 471749-471789      referencedob's pick when present, Prefix &
 *                               " " & Short, even when generaltasks goes on
 *                               to replace the description with a question
 *
 * (co()'s third call, 464848, sits behind var_8A > 1 on a path only mode 3
 * reaches, and mode 3 returned at 4643B2 for any var_8A > 0: dead.)  The
 * setter stores nothing when the line holds the word "it" (flag 0), which
 * is uip.pronoun.used here.  A question is not a turn, but these writes
 * stand: `cut rope with stone` then `x it` echoes "(stone)" and asks
 * "Which stone." again.
 *
 * Measured on p4WTIE/p4WTIE3 (make_400_withtieprobe.py [--desc], run400
 * runner_probes/wtie.run400.it1.txt, runner_probes/wtie3.run400.it2.txt,
 * runner_probes/wtie.run400.it3.txt, 2026-09-21): `zzz red stone` and `cut
 * stone red` "(the red stone)" (the scorer; "red" is a Prefix word), `cut rope
 * with blue stone ruby` "(the blue stone)", `cut red stone with stone` "(a red
 * stone)" (the -2 arm; the line asks), `cut rope with gems` "(emerald)", `x
 * stone` "(a blue stone)" (pass A's last mark, described and then asked
 * about), `x blue stone red stone` "(stone)" (pass C ties: nothing described),
 * and `open box with stone`, whose tail openclose scores without co(), leaves
 * the antecedent alone.
 *
 * Scarier's library reaches these points in its own order, so the writes
 * are kept by stage, and a handler's write -- ours, or the verb-driven
 * uip_definite_form() one -- outranks co()'s whatever order they came in.
 * The staged write is uip.pronoun.antecedent_object / _form / _stage.
 */


/*
 * uip_begin_antecedent_400()
 * uip_note_antecedent_400()
 * uip_pronoun_was_used()
 *
 * Stage the line's antecedent write: clear it at the start of the line, note
 * a candidate from a given stage (a later stage outranks an earlier one, and a
 * line that used a pronoun takes none), and tell whether a pronoun was used.
 */
void
uip_begin_antecedent_400 (void)
{
  uip.pronoun.antecedent_object = -1;
  uip.pronoun.antecedent_stage = -1;
}

void
uip_note_antecedent_400 (scr_int object, scr_int form, scr_int stage)
{
  if (uip.pronoun.used || object < 0 || stage < uip.pronoun.antecedent_stage)
    return;
  uip.pronoun.antecedent_object = object;
  uip.pronoun.antecedent_form = form;
  uip.pronoun.antecedent_stage = stage;
}

scr_bool
uip_pronoun_was_used (void)
{
  return uip.pronoun.used;
}


/*
 * uip_commit_antecedent_400()
 *
 * Store the line's noted antecedent.  uip_assign_pronouns() calls this for
 * a turn; the runner calls it itself for a 4.0 line that was not one.
 */
void
uip_commit_antecedent_400 (scr_gameref_t game)
{
  if (uip.pronoun.antecedent_object >= 0 && !uip.pronoun.used)
    {
      game->it_object = uip.pronoun.antecedent_object;
      game->it_form = uip.pronoun.antecedent_form;
      game->it_npc = -1;
    }
  uip.pronoun.antecedent_object = -1;
  uip.pronoun.antecedent_stage = -1;
}


/*
 * uip_get_pronoun_flags()
 * uip_set_pronoun_flags()
 *
 * A line nothing answered never reaches uip_assign_pronouns(), so both flags
 * can outlast it; a Spatterlight autosave keeps them.
 */
void
uip_get_pronoun_flags (scr_bool *used, scr_bool *pending_definite)
{
  *used = uip.pronoun.used;
  *pending_definite = uip.pronoun.pending_definite;
}

void
uip_set_pronoun_flags (scr_bool used, scr_bool pending_definite)
{
  uip.pronoun.used = used;
  uip.pronoun.pending_definite = pending_definite;
}


/*
 * uip_pronoun_t
 *
 * One pronoun word of the line, for uip_replace_pronouns(): the word, the
 * register it refers to ('object' or 'npc', or neither), and what goes into
 * the command for it -- 'prefix' and 'name', over 'extent' characters of the
 * line -- with 'echo' set where the bracket echo is not that same text.
 * 'definite' owns the prefix when it had to be composed.
 */
namespace {

struct uip_pronoun_t
{
  scr_gameref_t game;
  scr_prop_setref_t bundle;
  const scr_char *word;
  scr_bool is_character_word;

  scr_int object, npc, extent;
  const scr_char *prefix, *name, *echo;
  std::string definite;

  void find_register ();
  void object_phrase ();
  void no_object ();
  void no_character ();
  scr_bool resolve ();
  scr_int substitute (std::string &buffer, scr_int offset) const;
};


/*
 * uip_pronoun_t::find_register()
 *
 * Set 'object' or 'npc', and 'extent', when the word is a pronoun whose
 * register holds something.
 */
void
uip_pronoun_t::find_register ()
{
  /*
   * Search for pronouns where we have an assigned object or NPC.  We
   * can't be sure of getting plurality right, and it's not always
   * intuitive even in English -- is "a pair of scissors" an "it", or
   * a "them"?  Because of this, we just treat "it" and "them" equally
   * for now.
   */
  if (game->it_object != -1 && scr_compare_word (word, "it", 2))
    {
      object = game->it_object;
      extent = 2;
    }
  else if (game->it_object != -1 && scr_compare_word (word, "them", 4))
    {
      object = game->it_object;
      extent = 4;
    }
  else if (game->him_npc != -1 && scr_compare_word (word, "him", 3))
    {
      npc = game->him_npc;
      extent = 3;
    }
  else if (game->him_npc != -1 && scr_compare_word (word, "he", 2))
    {
      npc = game->him_npc;
      extent = 2;
    }
  else if (game->her_npc != -1 && scr_compare_word (word, "her", 3))
    {
      npc = game->her_npc;
      extent = 3;
    }
  else if (game->her_npc != -1 && scr_compare_word (word, "she", 3))
    {
      npc = game->her_npc;
      extent = 3;
    }
  else if (game->it_npc != -1 && scr_compare_word (word, "it", 2))
    {
      npc = game->it_npc;
      extent = 2;
    }
}


/*
 * uip_pronoun_t::object_phrase()
 *
 * The prefix and name for an object antecedent, in the form the version and
 * the last handler to set it call for.
 */
void
uip_pronoun_t::object_phrase ()
{
  prefix = prop_get_indexed_string (bundle, "Objects", object,
                                    "Prefix");
  name = prop_get_indexed_string (bundle, "Objects", object, "Short");

  /*
   * An empty Prefix is an "a" prefix: every Runner's .taf loader
   * substitutes the literal on the way in (run380 @4481B2, run370
   * @43F5DA, run400 loc_4900EC), so the antecedent the Runner builds
   * through Proc_21_31_448710 reads "a Cupboard".  Measured on
   * man_overboard (4.00): `open it` after `x cupboard` echoes "(a
   * Cupboard)", and likewise "(a wardrobe)", "(a pantry)", "(a
   * toolbox)".  parse_trim_object_names does the substitution now, so
   * the prefix read back here already carries it.
   */

  /*
   * 4.0 keeps the antecedent as a string composed by whichever
   * handler last set it, and the composer (run400 Proc_21_31_448710)
   * has two modes: mode 1 is Prefix & " " & Short as authored, mode 0
   * passes the prefix through tense (Proc_21_13_44F474: "a", "an" and
   * "some" become "the", anything else is left alone).  Examine
   * composes in mode 1 (loc_471749-471789), take/drop/open/close and
   * the "I don't understand what you want me to do with" reply in
   * mode 0 -- see uip_definite_form().  Measured on humbug (4.00,
   * runner_probes/humbug.run400.it.txt, 2026-08-29): `x plane` then
   * `x it` echoes "(a paper aeroplane)", `get plane` then `x it`
   * "(the paper aeroplane)", `open satchel` "(the satchel)", `throw
   * shovel` (no such verb) "(the shovel)"; "some gloves" and "an
   * envelope" become "the gloves" / "the envelope" after `get`
   * (runner_probes/humbug.run400.b.txt).
   *
   * 3.7 and 3.8 store the antecedent in generaltasks' pre-pass over
   * the typed line, before any handler: when exactly one object is
   * named it becomes tense(Prefix) & " " & Short (run380 441EF1 and
   * 442038, run370 43B696 and 43B805), held or not, whatever the
   * verb.  Measured on wrecked (3.80, run380x
   * runner_transcripts/wrecked.rtf, 2026-09-14): `get jacket` then `x
   * it` echoes "(the tweed jacket)", `get form` then `x it` "(an
   * application form)" (Prefix "an", which tense() leaves alone), and
   * `wave wand` on the held wand makes the later `drop it` the wand.
   */
  if (game->it_form == UIP_IT_BARE
      && prop_get_taf_version (bundle) >= TAF_VERSION_400)
    prefix = "";            /* co()'s park arm; see above */
  else if (game->it_form == UIP_IT_DEFINITE
           && prop_get_taf_version (bundle) < TAF_VERSION_390)
    {
      definite = uip_tense_prefix_3738 (prefix);
      prefix = definite.c_str ();
    }
  else if (game->it_form == UIP_IT_DEFINITE)
    {
      if (scr_compare_word (prefix, "a", 1))
        definite = std::string ("the") + (prefix + 1);
      else if (scr_compare_word (prefix, "an", 2))
        definite = std::string ("the") + (prefix + 2);
      else if (scr_compare_word (prefix, "some", 4))
        definite = std::string ("the") + (prefix + 4);
      else
        definite = prefix;
      prefix = definite.c_str ();
    }
}


/*
 * uip_pronoun_t::no_object()
 * uip_pronoun_t::no_character()
 *
 * The seeded strings the Runner substitutes for a pronoun nothing has been
 * referred to by yet.
 */
void
uip_pronoun_t::no_object ()
{
  /*
   * No antecedent at all for "it"/"them".  The Runner's object
   * antecedent is a pair of strings seeded at new game -- run400
   * Proc_19_4_45AA98 calls Proc_21_41_448C24 (flag 0, "nothing",
   * "Absolutely nothing") -- and never a "no reference" state, so
   * the pronoun is still replaced: "nothing" goes into the command
   * and "Absolutely nothing" is what the bracket echo prints.
   * Measured on ptgood_again (4.00) turns 5-6: `x it` with nothing
   * yet referenced answers "(Absolutely nothing)" then "You see no
   * such thing."; `g` repeats "(x it)", "(Absolutely nothing)".
   */
  prefix = "";
  name = "nothing";
  echo = "Absolutely nothing";
  extent = scr_compare_word (word, "it", 2) ? 2 : 4;
}

void
uip_pronoun_t::no_character ()
{
  /*
   * The same thing for the character pronouns, and again the Runner
   * has no "no reference" state -- the register is a seeded string,
   * so him/he/her/she are rewritten even before anything has been
   * referred to.  3.9 and 4.0 keep a register per gender, seeded
   * "No male" and "No female" (run400 loc_45A7F9/45A800 in
   * Proc_19_4_45AA98, run390 loc_434969/434970 in clear(); assigned
   * the NPC's Name by gender byte at run400 loc_47F3B9/47F3D0 and
   * run390 loc_4592D7/4592EE, which is Scarier's him_npc/her_npc).
   * 3.7 and 3.8 have ONE character register for all four pronouns,
   * seeded "Nobody" (run370 loc_42398D MemVar_4460B4, read by every
   * branch of its() at 42CAFA/42CBC6/42CC9B/42CD67; run380
   * loc_4289F1).  Unlike the object register there is no second
   * lower-case copy: the echo and the text spliced into the command
   * are the same string, and the whole command is lower-cased after
   * the splice.
   *
   * Measured on showtime (4.00, runner_transcripts/showtime.txt turn
   * 59): `get her hand` with no female yet referenced answers "(No
   * female)" and then runs the game's task, which survives because
   * its command is the wildcard `get * hand` and the rewritten line
   * is "get no female hand".
   */
  prefix = "";
  if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
    name = (scr_compare_word (word, "him", 3)
            || scr_compare_word (word, "he", 2))
           ? "No male" : "No female";
  else
    name = "Nobody";
  echo = name;
  extent = scr_compare_word (word, "he", 2) ? 2 : 3;
}


/*
 * uip_pronoun_t::resolve()
 *
 * Work out what the word stands for.  Returns TRUE if it is a pronoun to
 * replace: prefix and name then indicate what to insert, and extent shows
 * how much of the buffer to replace with them.
 */
scr_bool
uip_pronoun_t::resolve ()
{
  /* Initially, no object or NPC, no names, and a zero extent. */
  object = npc = -1;
  prefix = name = echo = nullptr;
  extent = 0;

  find_register ();

  /*
   * Assign prefix and name to the full object or NPC name, if any.
   *
   * An object goes in as its whole noun phrase, prefix and all; a
   * character goes in as its bare Name.  That asymmetry is the Runner's,
   * and it holds in all four generations -- the two antecedents are
   * separate variables, seeded with "Absolutely nothing" / "nothing" for
   * the object and "Nobody" (4.0 adds "No male" / "No female") for the
   * character, and the character one is assigned the Name field alone:
   *
   *   run370  loc_438332   MemVar_4460B4 = var_164(0)         [Name]
   *   run370  loc_43B696   MemVar_4460AC = tense(Prefix) & " " & Short
   *   run400  loc_47F3B9   MemVar_494184 = var_140(0)         [Name]
   *
   * Field 0 of a character really is the Name, not a prefix: run390's
   * room lister builds "<(0)> is <(4)> <(8)>." at loc_459248 -- "Chloe is
   * a girl." -- and matches the typed word against (0) and (8) at
   * loc_4592B8.  We used to prepend the NPC's Prefix too, which turned
   * "ask him about pens" into "ask the Harold about pens" (wrecked, 3.80)
   * where the Runner writes "ask harold about pens".
   */
  if (object > -1)
    object_phrase ();
  else if (npc > -1)
    {
      prefix = "";
      name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
    }
  else if (scr_compare_word (word, "it", 2)
           || scr_compare_word (word, "them", 4))
    no_object ();
  else if (is_character_word)
    no_character ();

  return prefix && name && extent > 0;
}


/*
 * uip_pronoun_t::substitute()
 *
 * Print the bracket echo and splice the antecedent into the buffer at the
 * offset, in place of the pronoun.  Returns the length of what went in.
 */
scr_int
uip_pronoun_t::substitute (std::string &buffer, scr_int offset) const
{
  std::string replacement;

  /* Build the replacement: "<prefix> <name>", or the bare name. */
  replacement.reserve (strlen (prefix) + 1 + strlen (name));
  if (prefix[0] != NUL)
    {
      replacement.append (prefix);
      replacement.push_back (' ');
    }
  replacement.append (name);

  /*
   * Every Runner echoes the antecedent in round brackets on a line of
   * its own before the command's response -- "(a trophy)".  4.0 does
   * it whenever Options -> Display & Media... -> "References in
   * brackets" is ticked, the reference setting Scarier models: run400
   * Proc_19_49_461F38 (him/he/her/she/it/them branches, e.g. "it" at
   * loc_461DA5) tests MemVar_4942BA then prints "(" & antecedent &
   * ")" & vbCrLf through Proc_21_19_47B568.  Measured on adrift_maze
   * (4.00) commands 24-25: "read it" answers "(a trophy)" then "It
   * has the following engraved on it."
   *
   * 3.9 is the same line behind the same setting: run390 Sub its()
   * @43D968 tests m_showbrackets (loc_43D6C2 for "it", 43D7BF "them",
   * 43D884 "one") and prints "(" & MemVar_46811C & ")" through
   * Proc_2_28_45CBD0; the menu item is read from ADRIFT\Runner
   * "showbrackets" at loc_44526F with default True (loc_44526B).
   * Measured on Archie's Birthday (3.90, run390, 2026-09-05): `x
   * camcorder` then `take it` answers "(a camcorder)" then "You take
   * a camcorder from the desk."; and on veteran (3.90, run390, same
   * day) `x bag`, `take it`, `open it` echo "(a bag)" both times --
   * 3.9's takes @455067 and drops @445BE6 compose the antecedent in
   * mode 1 (authored Prefix).  Lines no handler names an object on
   * keep co()'s mode-0 "the X" from its pre-pass (43B69E), a form
   * Scarier doesn't model: see uip_assign_pronouns() on 8f7dc3d7a.
   * (An earlier reading of run390 had it keeping showbrackets only
   * for the "ask about"/"talk about" rewrite at loc_459036/459107
   * and echoing nothing -- wrong.)
   *
   * 3.7 and 3.8 print the same line UNGATED -- run370 Sub Form1.its
   * @2CA9C (loc_42CAFA ...) and run380 @326B4 have no Appearance menu
   * -- with the same antecedents: the NPC's Name, or tense(Prefix) &
   * " " & Short for an object, which is what 'replacement' holds.
   * Measured on wrecked (3.80, run380x
   * runner_transcripts/wrecked.rtf, 2026-09-14).
   */
  pf_buffer_reference (gs_get_filter (game),
                       echo ? echo : replacement.c_str (),
                       gs_get_vars (game), gs_get_bundle (game));

  /*
   * Splice the replacement in for the matched extent, and lower-case
   * the whole line again -- the Runner assigns
   * `cmd = LCase(Left$ & antecedent & Right$)` after every splice
   * (run400 Proc_19_49_461F38 loc_461ACB..461AD7 for "him",
   * loc_461BA5..461BB1 for "he", and so on down the branches), so an
   * authored capital in the Name never survives into the parsed
   * command.  Without this, uip_case_folds_name() would refuse the
   * very character the pronoun just named: "ask him about pens" in
   * wrecked (3.80) becomes "ask harold about pens", not "ask Harold
   * about pens".
   *
   * 3.7 and 3.8 echo tense(Prefix) & " " & Short but splice only the
   * Short into the line: the pre-pass stores the two apart (run380
   * 441EF1 / 441F09, run370 43B696 / 43B6AE) and its() writes
   * Left$ & " " & MemVar_44F0B8 & Right$ (run380 432A95).  Measured
   * on wrecked T129 (run380x): `ask aslan about it` after the wand
   * answers the "wand" topic, not the later "witch" one that "the
   * witch's magic wand" would hit under the substring topic match.
   *
   * 3.9 keeps the same pair (MemVar_46811C echo, MemVar_468120
   * splice).  Its handlers write one composed string to both
   * (takes 455067/455071, drops 445BE6/445BF0), but co()'s pre-pass
   * writes "the X" to the echo (43B69E) and the bare Short to the
   * splice (global_4, 43B6B0).  Blue Sky: `ask maria about burger`
   * then `give it to her` echoes "(the burger)" and runs the game's
   * `give burger to maria` task, which "give the burger to maria"
   * would not match.
   */
  if (object > -1
      && (prop_get_taf_version (bundle) < TAF_VERSION_390
          || (prop_get_taf_version (bundle) < TAF_VERSION_400
              && game->it_form == UIP_IT_DEFINITE)))
    replacement = name;
  buffer.replace (offset, extent, replacement);
  scr_lowercase (&buffer[0]);

  return replacement.size ();
}

} /* namespace */


/*
 * uip_replace_pronouns()
 *
 * Replaces pronouns by their respective object or NPC names, and returns the
 * resulting string to the caller, or NULL if no pronouns were replaced.  The
 * return string is malloc'ed, so the caller needs to remember to free it.
 */
scr_char *
uip_replace_pronouns (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::string buffer;
  scr_bool modified;
  const scr_char *current;
  scr_int offset, pass;
  assert (string);

  if (uip.trace)
    scr_trace ("UIParser: pronoun search \"%s\"\n", string);

  /*
   * 'current' is the active string -- the input until a pronoun fires, then
   * the 'buffer' copy (basic copy-on-write).  'offset' is our position within
   * it; we work by offset because std::string edits may reallocate.
   */
  modified = FALSE;
  current = string;

  /*
   * Search for pronouns until no more string remains, the character ones
   * first and "it"/"them" on a second pass: the Runner works through them
   * by kind, not by where they stand on the line (run390 its(): "him"
   * 43D310, "he" 43D3EB, "her" 43D4CF, "she" 43D5AA, "it" 43D685, "them"
   * 43D782), and each prints its bracket echo as it goes.  Blue Sky (3.90,
   * run390x, 2026-10-09): `give it to her` echoes "(Maria)" and then "(the
   * burger)".
   */
  for (pass = 0; pass < 2; pass++)
    {
      offset = strspn (current, WHITESPACE);
      while (current[offset] != NUL)
        {
          const scr_char *const word = current + offset;
          uip_pronoun_t pronoun;
          const scr_bool is_character_word
              = scr_compare_word (word, "him", 3)
                || scr_compare_word (word, "he", 2)
                || scr_compare_word (word, "her", 3)
                || scr_compare_word (word, "she", 3);

          if (is_character_word != (pass == 0))
            {
              offset += strcspn (current + offset, WHITESPACE);
              offset += strspn (current + offset, WHITESPACE);
              continue;
            }

          pronoun.game = game;
          pronoun.bundle = bundle;
          pronoun.word = word;
          pronoun.is_character_word = is_character_word;

          if (pronoun.resolve ())
            {
              uip.pronoun.used = TRUE;

              /*
               * If not yet copied, copy the input string into the buffer now
               * and switch 'current' to it.  The offset is unchanged since
               * the buffer starts as an exact copy; basic copy-on-write.
               */
              if (!modified)
                {
                  buffer = string;
                  modified = TRUE;
                }

              /* Echo and splice, then skip over the replacement. */
              offset += pronoun.substitute (buffer, offset);
              current = buffer.c_str ();

              if (uip.trace)
                scr_trace ("Parser: pronoun \"%s\"\n", buffer.c_str ());
            }
          else
            {
              /* If no match, advance over the unmatched word. */
              offset += strcspn (current + offset, WHITESPACE);
            }

          /* Set offset to the next word start. */
          offset += strspn (current + offset, WHITESPACE);
        }
    }

  /* Return the final string, or NULL if no pronoun replacements. */
  return modified ? scr_strdup (buffer) : nullptr;
}


/*
 * uip_lowered()
 *
 * Lower-cased copy of a string, for the Runner's LCase() comparisons.
 */
static std::string
uip_lowered (const scr_char *string)
{
  std::string lowered (string);
  scr_lowercase (&lowered[0]);
  return lowered;
}


/*
 * uip_phrase_in()
 *
 * The Runner's c(): is the (lower-cased) phrase in the lower-cased command
 * as a whole word or words -- run400 Proc_21_38_454CB0, an InStr() loop that
 * accepts a hit only when what surrounds it is a space or the line's end.
 */
static scr_bool
uip_phrase_in (const std::string &lowered, const scr_char *phrase)
{
  std::string wanted = uip_lowered (phrase);
  if (wanted.empty ())
    return FALSE;

  for (std::string::size_type pos = lowered.find (wanted);
       pos != std::string::npos; pos = lowered.find (wanted, pos + 1))
    {
      const std::string::size_type end = pos + wanted.size ();
      const scr_bool left = pos == 0 || lowered[pos - 1] == ' ';
      const scr_bool right = end == lowered.size () || lowered[end] == ' ';
      if (left && right)
        return TRUE;
    }
  return FALSE;
}


/*
 * uip_npc_named()
 *
 * Does the command name this NPC?  run400 Proc_21_40_45E99C: c(LCase(Name))
 * and, only if that fails, c(LCase(alias)) for each alias -- the alias loop
 * assigns without breaking (loc_45E623..45E67D), so the LAST matching alias
 * is the one that survives, while a matching Name jumps the loop entirely
 * (loc_45E620).  There is no presence test -- an NPC two rooms away still
 * counts.
 *
 * Whichever string was chosen is then lower-cased and looked for in the
 * command with a case-SENSITIVE InStr; see uip_case_folds_name().
 *
 * run390 names a character by c(LCase(Name)) Or c(LCase(Alias(0))) alone --
 * characters() 4592B8 for the register, the give rewrite's count at
 * 45F9FB-45FA89 -- and c() lower-cases both sides (4334B0).
 */
static scr_bool
uip_npc_named (scr_gameref_t game, scr_int npc, const std::string &lowered,
               const scr_char *command)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_bool is_390 = prop_get_taf_version (bundle) == TAF_VERSION_390;
  const scr_char *chosen = nullptr;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Name";
  chosen = prop_get_string (bundle, "S<-sis", vt_key);
  if (!uip_phrase_in (lowered, chosen))
    {
      chosen = nullptr;

      vt_key[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      if (is_390 && alias_count > 1)
        alias_count = 1;
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (!scr_strempty (alias_name) && uip_phrase_in (lowered, alias_name))
            chosen = alias_name;
        }
    }

  if (is_390 || uip.options.lenient_tasks)
    return chosen != nullptr;
  return chosen && uip_case_folds_name_in (command, chosen);
}


/*
 * uip_last_npc_name()
 *
 * The "character most recently named by a command" register -- run400
 * MemVar_494180, seeded "Nobody" at loc_45A7F5 (run370 MemVar_4460B4 the
 * same) -- as the Runner prints it.
 */
static const scr_char *
uip_last_npc_name (scr_gameref_t game, scr_int npc)
{
  scr_vartype_t vt_key[3];

  if (npc == -1)
    return "Nobody";

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Name";
  return prop_get_string (gs_get_bundle (game), "S<-sis", vt_key);
}


/*
 * The registers as they stood before uip_note_named_npcs() ran, for
 * uip_renote_named_npcs() to put back, are uip.pronoun.*_before_noting.
 */


/*
 * uip_note_npc_pronoun()
 *
 * The same loop in characters() writes the him/her registers too, by the
 * NPC's gender byte, for every character the line names by Name or alias --
 * no presence or seen test (run400 loc_47F3A2..47F402, run390 loc_4592B8..
 * 459326); a neuter character goes to the object antecedent (run400 pushes
 * Name to Proc_21_41_448C24).  Pronoun replacement has already run on the
 * line by then, so it read the previous line's value.  Before 3.9 there is
 * no gender byte and uip_assign_pronouns() keeps the old handling.
 * Measured on British Fox (4.00, runner_transcripts/britishfox.txt turn
 * 188): Sharon was last seen at reception, yet `ask about grace` in the
 * cells makes `tell her it was sabrina` echo "(Grace)", Grace being named
 * but never seen.
 */
static void
uip_note_npc_pronoun (scr_gameref_t game, scr_int npc)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);

  if (prop_get_taf_version (bundle) < TAF_VERSION_390)
    return;

  switch (prop_get_indexed_integer (bundle, "NPCs", npc, "Gender"))
    {
    case NPC_MALE:
      game->him_npc = npc;
      break;
    case NPC_FEMALE:
      game->her_npc = npc;
      break;
    case NPC_NEUTER:
      game->it_npc = npc;
      game->it_object = -1;
      break;
    default:
      break;
    }
}


/*
 * uip_note_named_npcs()
 *
 * Update the last-named-character register from a command about to run.
 * run400 Proc_19_0_480674 loc_47F2C5..47F3A2 walks the NPCs in index order
 * and assigns Name for every one Proc_21_40_45E99C finds in the line, so
 * the highest-indexed match wins; it runs AFTER the routine's own "ask
 * about" rewrite, which is why the rewrite sees the previous command's
 * value (run_all_commands() keeps that by remembering the index it had
 * before this ran).
 *
 * The loop is unconditional, and characters() itself is reached at 48B56E
 * on EVERY line -- the task dispatch's early exits jump to 48B4E3, which is
 * still above it -- so a line a task answered names its characters too.
 * Measured 2026-09-05 (runner_probes/sommeril.run400.npcprobe.txt,
 * sommeril.taf): "ask about zzz" in the gargoyle's street echoes "(Nobody)"
 * even with GARGOYLE listed in the room description, then the task-answered
 * "give silver orb to gargoyle" makes the next "ask about zzz" echo
 * "(GARGOYLE)".
 */
void
uip_note_named_npcs (scr_gameref_t game, const scr_char *string)
{
  const std::string lowered = uip_lowered (string);
  scr_int index_;

  uip.pronoun.npc_before_noting = game->last_npc;
  uip.pronoun.him_before_noting = game->him_npc;
  uip.pronoun.her_before_noting = game->her_npc;
  uip.pronoun.it_before_noting = game->it_npc;
  uip.pronoun.it_object_before_noting = game->it_object;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (uip_npc_named (game, index_, lowered, string))
        {
          game->last_npc = index_;
          uip_note_npc_pronoun (game, index_);
        }
    }
}


/*
 * uip_renote_named_npcs()
 *
 * gotoplace cuts the GLOBAL command line in place before characters() reads
 * it -- Right(line, Len-5) for "goto", wherever c() found it, 6 for "go to",
 * 3 for "go" from 3.9, and 3.7's own goto word first (run400 4649C1..4649CA,
 * run380 31BC4) -- so the register is noted from what is left of the line,
 * not from what was typed; after a walk the line is "&&&" and characters()
 * never runs (run400 48B56E is jumped to 48BBAA, run370 43C8A6 needs a
 * message).  Undo this line's noting and, when it did not walk, redo it on
 * the cut line.
 *
 * Before 4.0 c() decides a hit at position 1 at once: TRUE when the line is
 * empty or the character after the match is a space or a comma (run380
 * 429048, run370 423C80), so c("") is TRUE on a line starting with a space
 * and the `c(Name) Or c(Alias)` test names a character with an empty alias.
 * `where is goto cave` cuts to " is goto cave" at 3.8/3.9 and notes Bob,
 * whose alias is empty (p3xORD); 3.7 cuts its goto word first and walks,
 * and 4.0's Proc_21_40_45E99C never tries an empty alias.  `ask bob about
 * goto cave` cuts to "ob about goto cave" everywhere, naming nobody.
 * run370x..run400x runner_probes/ord.run*.goto.* (make_orderprobe.py): the
 * `give goto cave` after them echoes "(to Nobody)" at 3.7/4.0 and "(to Bob)"
 * at 3.8/3.9.
 */
void
uip_renote_named_npcs (scr_gameref_t game, const scr_char *cut_line,
                       scr_bool walked)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_bool pre_400 = prop_get_taf_version (bundle) < TAF_VERSION_400;
  const std::string lowered = uip_lowered (cut_line);
  const scr_bool empty_hits = pre_400
                              && (lowered.empty () || lowered[0] == ' '
                                  || lowered[0] == ',');
  scr_int index_;

  game->last_npc = uip.pronoun.npc_before_noting;
  game->him_npc = uip.pronoun.him_before_noting;
  game->her_npc = uip.pronoun.her_before_noting;
  if (game->it_npc != uip.pronoun.it_before_noting)
    {
      game->it_npc = uip.pronoun.it_before_noting;
      game->it_object = uip.pronoun.it_object_before_noting;
    }
  if (walked)
    return;

  GS_FOR_EACH_NPC (game, index_)
    {
      scr_bool named = uip_npc_named (game, index_, lowered, cut_line);

      if (!named && empty_hits)
        {
          scr_vartype_t vt_key[4];
          const scr_char *alias_name = "";

          vt_key[0].string = "NPCs";
          vt_key[1].integer = index_;
          vt_key[2].string = "Alias";
          if (prop_get_child_count (bundle, "I<-sis", vt_key) > 0)
            {
              vt_key[3].integer = 0;
              alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
            }
          named = scr_strempty (alias_name);
        }
      if (named)
        {
          game->last_npc = index_;
          uip_note_npc_pronoun (game, index_);
        }
    }
}


/*
 * uip_line_names_npc()
 *
 * TRUE if the line names any character at all -- the same walk over
 * Proc_21_40_45E99C that run400 makes at loc_48B53C..48B569, whose result
 * (var_29C) gates the DontUnderstand text at 48B585.  See the note in
 * run_element_not_understood().
 */
scr_bool
uip_line_names_npc (scr_gameref_t game, const scr_char *string)
{
  const std::string lowered = uip_lowered (string);
  scr_int index_;

  GS_FOR_EACH_NPC (game, index_)
    {
      if (uip_npc_named (game, index_, lowered, string))
        return TRUE;
    }
  return FALSE;
}


/*
 * uip_ask_echo_skip()
 *
 * The length of the "ask about " / "talk about " prefix this line carries,
 * or zero if it carries neither.
 */
static std::string::size_type
uip_ask_echo_skip (const std::string &lowered)
{
  if (lowered.compare (0, 10, "ask about ") == 0)
    return 10;
  if (lowered.compare (0, 11, "talk about ") == 0)
    return 11;
  return 0;
}


/*
 * uip_print_ask_echo()
 *
 * Print the "(<npc>)" the ask/talk rewrite echoes, and say whether it did.
 *
 * The echo comes out BEFORE task matching.  run400 prints it from inside the
 * parser proper, and a task that answers the line still prints after it --
 * measured 2026-09-05 on two opposite task shapes: `SPAM.taf` turn 11, whose
 * `* ingredients *` wildcard task answers `ask about ingredients` under a
 * bare "(Nobody)" (runner_probes/spam.run400.probe.txt), and `sommeril.taf`
 * turn 36, whose LITERAL task `ask about glass framed page` answers under
 * "(GARGOYLE)" (runner_probes/sommeril.run400.probe.txt).  The literal case is
 * the interesting one: it proves the echo is not suppressed by an earlier
 * typed-command dispatch, and -- since a pattern spelled `ask about glass
 * framed page` cannot match the rewritten `ask gargoyle about glass framed
 * page` -- that the REWRITTEN string is the library's alone.  Tasks go on
 * matching the line the player typed.
 *
 * So only the echo is hoisted here; uip_rewrite_references() still splices
 * the name in, on the library path, where the give rewrite also stays (that
 * one lives at run400 loc_48A98A, after the typed-command dispatch at
 * 48A481, and a matched task does jump past it).
 */
scr_bool
uip_print_ask_echo (scr_gameref_t game, const scr_char *string)
{
  const std::string lowered = uip_lowered (string);

  if (uip_ask_echo_skip (lowered) == 0)
    return FALSE;

  pf_buffer_reference (gs_get_filter (game),
                       uip_last_npc_name (game, game->last_npc),
                       gs_get_vars (game), gs_get_bundle (game));
  return TRUE;
}


/*
 * uip_rewrite_references()
 *
 * The two "References in brackets" rewrites that fill in a character the
 * player left out, each echoing what it assumed in round brackets on a line
 * of its own (Scarier models the box ticked):
 *
 *   give X [no character named, no "to"]  ->  "(to Bob)"  give X to bob
 *   ask about X / talk about X            ->  "(Bob)"     ask bob about X
 *
 * run400: the give rewrite is loc_48A98A..48AA38 in the input routine
 * Proc_19_61_48C0F0 (c("give"), a Proc_21_40_45E99C count of zero, Not
 * c("to")); the ask/talk rewrite opens the parser proper, Proc_19_0_480674
 * loc_47F134..47F2BA (Left(cmd, 10) = "ask about ", Left(cmd, 11) = "talk
 * about ").  Both take MemVar_494180 as it stands from the previous
 * command, print through Proc_21_19_47B568 behind MemVar_4942BA, and splice
 * LCase(Name) into the line.  run370 loc_43BED8/43BFC9 and loc_4380CF/
 * 438185 (run380 the same) do both with no gate -- 3.7 has no Appearance
 * menu.  run390 has both too: the ask/talk pair at loc_459036/459107 and
 * the give rewrite in generaltasks at 45F9D5-45FB13 ("(to " at 45FAB9).
 * Measured p39ABSNPC (run390x, runner_probes/absnpc.run390.txt): `give stone`
 * after `talk to gina` echoes "(to Gina)" and runs as `give stone to gina`.
 * vardock_bates turn 16 showed "(to Vagabundo)" live in 4.0.
 *
 * Returns a fresh string if anything was rewritten, else NULL.
 */
scr_char *
uip_rewrite_references (scr_gameref_t game, const scr_char *string,
                        scr_int prior_npc, scr_bool echo_printed)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *name = uip_last_npc_name (game, prior_npc);
  const std::string lowered_name = uip_lowered (name);
  std::string command (string);
  std::string lowered = uip_lowered (string);
  scr_bool modified = FALSE;

  if (uip_phrase_in (lowered, "give") && !uip_phrase_in (lowered, "to"))
    {
      scr_int index_;
      scr_bool named = FALSE;

      for (index_ = 0; index_ < gs_npc_count (game) && !named; index_++)
        named = uip_npc_named (game, index_, lowered, command.c_str ());

      if (!named)
        {
          const std::string echo = std::string ("to ") + name;

          pf_buffer_reference (gs_get_filter (game), echo.c_str (),
                               gs_get_vars (game), bundle);
          command += " to ";
          command += lowered_name;
          lowered = uip_lowered (command.c_str ());
          modified = TRUE;
        }
    }

  const std::string::size_type skip = uip_ask_echo_skip (lowered);
  if (skip > 0)
    {
      /* The echo is printed up front now; see uip_print_ask_echo(). */
      if (!echo_printed)
        pf_buffer_reference (gs_get_filter (game), name,
                             gs_get_vars (game), bundle);
      command = "ask " + lowered_name + " about " + command.substr (skip);
      modified = TRUE;
    }

  if (modified && uip.trace)
    scr_trace ("Parser: reference rewrite \"%s\"\n", command.c_str ());

  return modified ? scr_strdup (command) : nullptr;
}


/*
 * uip_definite_form()
 *
 * Decide whether the object antecedent that 'command' just assigned is held
 * in its definite ("the X") or indefinite (Prefix & " " & Short) form.  4.0
 * only: 3.9 keeps the authored prefix, and 3.7/3.8 always store the tense()'d
 * one (uip_tense_prefix_3738(), run380 441EF1).
 *
 * The Runner's antecedent is a string, composed by whichever code last called
 * the setter Proc_21_41_448C24, in the composer mode that code chose:
 *
 *   co() (object resolution)   4.0 stores only from its crowd arms: the -2
 *                              arm (46460F) Prefix & " " & Short, the park
 *                              arm (464788) the bare Short; the arm at 464848
 *                              is dead.  A lone namesake stores nothing, so
 *                              this code keeps what was there (UIP_FORM_KEEP)
 *                              and the crowd arms note through
 *                              uip_note_antecedent_400().  (The old "found in
 *                              the room" definite store is run390's co
 *                              @43B69E; `look in dustbin` "(the dustbin)" at
 *                              4.0 is the line-start scorer, 48A3F5.)
 *   examines @471749-471789    mode 1, indefinite: `x shovel` after `get
 *                              shovel` is back to "(a shovel)"; so is `x the
 *                              shovel`.
 *   takes? @462AAC-462AD9      mode 0 at entry, before the task pre-match,
 *                              which is why humbug's `get can`, intercepted by
 *                              its "get * can *" FailMessage task, still
 *                              leaves "(the can)"; a second `get shovel`
 *                              ("already have") stays "(the shovel)".  Taking
 *                              from a character goes another way: `Get
 *                              Document` from Grandad leaves "(a document)"
 *                              (runner_probes/humbug.run400.b.txt 1406).
 *   takes @47BFCF/@47C058      mode 1 on the refusal paths; a failed `get
 *                              shovel from satchel` left "(a shovel)".
 *   drop helper @465F5E        mode 0: `drop shovel` "(the shovel)"; drops'
 *                              refusal sites @46F77B/@46F81E are mode 1.
 *   openclose @47585A ...      mode 0 at all four sites, the lock/unlock
 *                              path (@476288-4762A1) included: "(the
 *                              satchel)" after `open satchel` and `close
 *                              satchel`; provenance's `unlock door` then
 *                              `open it` "(the trap door)" is from P-code.
 *   generaltasks @48A409       mode 0: `throw shovel` "(the shovel)".
 *   put                        never touches it: `put shovel in satchel`
 *                              (refused) and `put banana in dustbin` leave
 *                              whatever was there.
 *
 * A command that used the pronoun leaves the antecedent exactly as it was:
 * `drop it`, `x it`, `look at it`, `examine it` all echo the previous form
 * (runner_probes/humbug.run400.it.txt 321-349), so the caller skips assignment
 * altogether for those.  All measured on humbug, run400, 2026-08-29.
 */
enum uip_form_t
{
  UIP_FORM_KEEP, UIP_FORM_INDEFINITE, UIP_FORM_DEFINITE
};

static uip_form_t
uip_definite_form (scr_gameref_t game, const scr_char *command,
                   const scr_char *at, scr_int object)
{
  const scr_char *verb = command + strspn (command, WHITESPACE);
  const scr_int was_at = gs_object_position (game->temporary, object);
  const scr_bool was_held = was_at == OBJ_HELD_PLAYER
                            || was_at == OBJ_WORN_PLAYER;
  const scr_bool was_with_npc = was_at == OBJ_HELD_NPC
                                || was_at == OBJ_WORN_NPC;
  scr_bool has_from = FALSE;
  const scr_char *scan;

  if (uip.pronoun.pending_definite)
    return UIP_FORM_DEFINITE;

  for (scan = verb; scan[0] != NUL; scan++)
    {
      if (scr_isspace (scan[0]) && scr_compare_word (scan + 1, "from", 4))
        {
          has_from = TRUE;
          break;
        }
    }

  /*
   * The object named after "from" is the source, not the antecedent, and
   * put's two objects are neither: `put banana in dustbin` left "(the
   * banana)", `put shovel in satchel` "(a shovel)", and the failed `get
   * shovel from satchel` "(a shovel)".
   */
  if (has_from && at > scan)
    return UIP_FORM_KEEP;
  if (scr_compare_word (verb, "put", 3) || scr_compare_word (verb, "insert", 6))
    return UIP_FORM_KEEP;

  if (scr_compare_word (verb, "x", 1)
      || scr_compare_word (verb, "ex", 2)
      || scr_compare_word (verb, "exam", 4)
      || scr_compare_word (verb, "examine", 7)
      || scr_compare_word (verb, "read", 4)
      || ((scr_compare_word (verb, "l", 1)
           || scr_compare_word (verb, "look", 4))
          && strstr (verb, " at ")))
    return UIP_FORM_INDEFINITE;

  if (scr_compare_word (verb, "get", 3)
      || scr_compare_word (verb, "take", 4)
      || scr_compare_word (verb, "pick", 4))
    {
      if (was_with_npc)
        return UIP_FORM_INDEFINITE;
      if (has_from)
        {
          const scr_int now_at = gs_object_position (game, object);
          return now_at == OBJ_HELD_PLAYER && !was_held
                 ? UIP_FORM_DEFINITE : UIP_FORM_INDEFINITE;
        }
      return UIP_FORM_DEFINITE;
    }

  if (scr_compare_word (verb, "drop", 4))
    return was_held ? UIP_FORM_DEFINITE : UIP_FORM_INDEFINITE;

  if (scr_compare_word (verb, "open", 4) || scr_compare_word (verb, "close", 5)
      || scr_compare_word (verb, "lock", 4)
      || scr_compare_word (verb, "unlock", 6))
    return UIP_FORM_DEFINITE;

  /*
   * Anything else is not a handler's: what the line leaves is generaltasks'
   * scorer write or co()'s (uip_note_antecedent_400()).  The old guess here,
   * "definite for a room object, untouched for a held one", was run390's
   * co() (43B69E / 43B456).
   */
  return UIP_FORM_KEEP;
}


/*
 * uip_handler_composes_390()
 *
 * Whether a 3.9 line is one of those whose handler composes the antecedent
 * itself, as Prefix & " " & Short for echo and splice alike: a take from the
 * floor, drop, wear, remove, examine or read.  Any other line -- open,
 * close, put, take-from, a game task, an unknown verb -- keeps what co()'s
 * pre-pass left, "the X" for the echo and the bare Short for the splice
 * (run390 43B69E / 43B6B0; make_39_itprobe.py, `open microwave` then "(the
 * microwave)").
 */
static scr_bool
uip_handler_composes_390 (const scr_char *command)
{
  const scr_char *verb = command + strspn (command, WHITESPACE);

  if (scr_compare_word (verb, "get", 3) || scr_compare_word (verb, "take", 4)
      || scr_compare_word (verb, "pick", 4))
    return strstr (verb, " from ") == nullptr;

  return scr_compare_word (verb, "drop", 4)
         || scr_compare_word (verb, "wear", 4)
         || scr_compare_word (verb, "remove", 6)
         || scr_compare_word (verb, "x", 1)
         || scr_compare_word (verb, "ex", 2)
         || scr_compare_word (verb, "exam", 4)
         || scr_compare_word (verb, "examine", 7)
         || scr_compare_word (verb, "read", 4)
         || ((scr_compare_word (verb, "l", 1)
              || scr_compare_word (verb, "look", 4))
             && strstr (verb, " at ") != nullptr);
}


/*
 * uip_assign_pronouns()
 *
 * Search a player command for object and NPC names, and assign any found to
 * game pronouns.  The string is searched from front to back, assigning
 * pronouns for objects or NPC names as found.  Later ones will overwrite
 * earlier ones if there is more than one in the string.
 */
void
uip_assign_pronouns (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *current;
  scr_int saved_ref_object, saved_ref_character;
  scr_bool handler_set;
  assert (string);

  if (uip.trace)
    scr_trace ("UIParser: pronoun assignment \"%s\"\n", string);

  /*
   * A 4.0 command that went through a pronoun leaves every antecedent as it
   * was -- see uip_definite_form() -- so there is nothing to assign.
   */
  if (uip.pronoun.used && prop_get_taf_version (bundle) >= TAF_VERSION_400)
    {
      uip.pronoun.used = FALSE;
      uip.pronoun.pending_definite = FALSE;
      uip_begin_antecedent_400 ();
      return;
    }

  /*
   * The library's own writes go in first (see uip_note_antecedent_400());
   * the verb-driven form below is a handler's, and replaces anything but
   * another handler's.
   */
  handler_set = uip.pronoun.antecedent_stage == UIP_STAGE_HANDLER;
  uip_commit_antecedent_400 (game);
  uip.pronoun.used = FALSE;

  /*
   * run390 points "it" by a co() pre-pass (generaltasks 45F318-45F430): the
   * LAST object in index order that the line names, present, seen and with
   * no present namesake, unless a take, drop, wear, remove, examine or read
   * then names its own object.  So `get jet` from the open cabinet (takes'
   * auto-"from" rewrite, 4552EE) left "it" on the cabinet, and a put, open
   * or task line on whichever object came last in the file (make_39_itprobe
   * .py, crossworlds4 T240/T241, 8f7dc3d7a).  Deliberate deviation: 3.9 uses
   * the assignment below, which follows the objects the line names.
   */

  /* Save var references so we can restore them later. */
  saved_ref_object = var_get_ref_object (vars);
  saved_ref_character = var_get_ref_character (vars);

  /* Search for object and NPC names until no more string remains. */
  current = string + strspn (string, WHITESPACE);
  while (current[0] != NUL)
    {
      if (uip_match ("%object% *", current, game))
        {
          scr_int count, index_, object;

          /*
           * "Disambiguate" by rejecting objects that the player hasn't seen
           * or can't see.  If the reference is unique, assign to the 'it'
           * object pronoun.
           */
          count = 0;
          object = -1;
          GS_FOR_EACH_OBJECT (game, index_)
            {
              if (game->object_references[index_]
                  && (gs_object_seen (game, index_)
                      || prop_get_taf_version (bundle) < TAF_VERSION_390)
                  && obj_indirectly_in_room (game,
                                             index_, gs_playerroom (game)))
                {
                  count++;
                  object = index_;
                }
            }

          if (count == 1)
            {
              uip_form_t form = UIP_FORM_INDEFINITE;

              if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
                form = uip_definite_form (game, string, current, object);
              else if (prop_get_taf_version (bundle) < TAF_VERSION_390
                       || !uip_handler_composes_390 (string))
                form = UIP_FORM_DEFINITE;

              if (form != UIP_FORM_KEEP && !handler_set)
                {
                  game->it_object = object;
                  game->it_form = form == UIP_FORM_DEFINITE
                                  ? UIP_IT_DEFINITE : UIP_IT_INDEFINITE;
                  game->it_npc = -1;
                }

              if (uip.trace)
                {
                  scr_trace ("UIParser: object 'it/them' assigned %ld,"
                             " form %d\n", object, (int) form);
                }
            }
        }

      /* 3.9 on, uip_note_named_npcs() has already done the characters. */
      if (prop_get_taf_version (bundle) <= TAF_VERSION_380
          && uip_match ("%character% *", current, game))
        {
          scr_int npc;

          /* Do the same "disambiguation" as for objects above. */
          if (npc_count_referenced_present (game, &npc) == 1)
            {
              /*
               * Version 3.8 games lack NPC gender information, so for this
               * case set "him"/"her" on each match, and never set "it"; this
               * matches the version 3.8 runner.  Version 3.7 has no gender
               * field either (its NPC record is version 3.8's), so it takes
               * the same treatment.  (3.9 on never reaches here; see the
               * version gate above.)
               */
              game->him_npc = npc;
              game->her_npc = npc;
              game->it_npc = -1;

              if (uip.trace)
                {
                  scr_trace ("UIParser: 3.8 pronouns"
                            " 'him' and 'her' assigned %ld\n", npc);
                }
            }
        }

      /*
       * Advance the string position by a complete word.  This saves a lot
       * of time -- there's no point looking for an object or NPC name in
       * mid-word, and anyway it's not the right thing to do.
       */
      current += strcspn (current, WHITESPACE);
      current += strspn (current, WHITESPACE);
    }

  /* Restore variables references. */
  var_set_ref_object (vars, saved_ref_object);
  var_set_ref_character (vars, saved_ref_character);
  uip.pronoun.pending_definite = FALSE;
}

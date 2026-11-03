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
 * 4.0 noun scoring and the unhandled verb-object / verb-NPC catch-alls.
 *
 * See sclibrar.h for what the library files share and sclibrar_internal.h
 * for what the core files share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"

/*
 * lib_verb_object_resolve_400()
 *
 * The 4.0 Runner decides which object an otherwise unhandled line is
 * "about" before any verb runs: generaltasks calls the noun resolver
 * Proc_21_58_463640 at 48A3F5 (mode 0: candidates are the objects both
 * PRESENT and seen) and parks the answer in MemVar_4942F8.  The resolver
 * scores every candidate by the words of its name the line contains --
 * one for the Short as a whole word, one more for any alias (4632D3: the
 * alias loop runs whether or not the Short hit), and one more for each
 * word of the Prefix found (4632A9-463387, whole-word test
 * Proc_21_38_454CB0) -- and keeps the unique maximum.  thelasthour's
 * spyhole (Short "spyhole", alias "spyhole") outscores its bowl 2 to 1 on
 * `put bowl near spyhole` (runner_probes/thelasthour.run400.spyhole.txt);
 * Shadowpeak's "cell door" scores 0 on `put sword near cell door`, "door"
 * being no alias of it (runner_probes/shadowpeak.run400.celldoor.txt).  Two
 * candidates with different names and the same score leave it with a negative
 * index (4633C3-463405), and the "I don't understand what you want me to do
 * with" catch-all at 48B1B0-48B236 needs MemVar_4942F8 > -1, so a tie prints
 * NOTHING and the line falls to the game's DontUnderstand text.
 *
 * Measured on House (4.00) from the fireplace, run400
 * runner_probes/house.run400.cathy_l.txt and
 * runner_probes/house.run400.cathy_m.txt, 2026-09-06: `throw diary at cathy`,
 * `throw diary at zzz`, `wibble diary` and `wibble diary cathy` all answer "I
 * don't understand what you want me to do with the diary." (an NPC is not a
 * candidate), while `throw diary at fireplace`, `throw fireplace at diary`,
 * `throw diary at hook`, `throw fireplace diary` and `wibble diary fireplace`
 * all print "What?", House's [error=6] DontUnderstand -- with the fireplace
 * freshly examined or not.  Scarier used to speak for the first object its
 * `* %object% *` row bound.
 *
 * Returns the resolver's object, -1 for a tie, -2 when nothing scored.
 * 3.8 (run380 442F5D) instead walks the objects in index order and speaks
 * for the first present, seen one -- unmeasured, and left to the caller's
 * own choice.
 */
scr_int
lib_verb_object_name_score (scr_gameref_t game,
                            scr_int object, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *shortname, *prefix;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias, score;

  /* A name the line holds only inside a longer one does not score; see
     lib_co_term_shadowed() (deliberate deviation). */
  score = 0;
  shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
  if (shortname && shortname[0] != NUL
      && lib_input_contains_word_400 (input, shortname)
      && !lib_co_term_shadowed (game, input, object, shortname))
    score = 1;

  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      const scr_char *alias_name;

      vt_key[3].integer = alias;
      alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
      if (alias_name && alias_name[0] != NUL
          && lib_input_contains_word_400 (input, alias_name)
          && !lib_co_term_shadowed (game, input, object, alias_name))
        {
          score++;
          break;
        }
    }
  if (score == 0)
    return 0;

  /*
   * The 4.0 loader stores "a" for an empty Prefix (run400 4900EC), so an
   * object authored without one scores the word "a" like any "a X" object:
   * House's fireplace ties the diary on `throw a diary at fireplace` and
   * `throw diary at a fireplace` (runner_probes/house.run400.cathy_n.txt,
   * runner_probes/house.run400.cathy_o.txt), and matches the hook's "the" on
   * `throw a fireplace at the hook` (runner_probes/house.run400.cathy_p.txt).
   */
  prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
  if (!prefix || prefix[0] == NUL)
    prefix = "a";

  return score + lib_prefix_words_in_input_400 (input, prefix);
}

/*
 * present_only is the co(i, 0) gate the Runner puts on most of its calls.
 * The one caller that leaves it off is put_drop_list's own " on " split
 * test; see lib_put_split_400().
 */
typedef scr_bool (*lib_resolve_admit_t) (scr_gameref_t game,
                                         scr_int object, scr_int pass);

scr_bool
lib_resolve_admit_mode0 (scr_gameref_t game, scr_int object, scr_int pass)
{
  if (!gs_object_seen (game, object))
    return FALSE;
  return pass > 0
         || obj_indirectly_in_room (game, object, gs_playerroom (game));
}

/*
 * The take handler's 463640 mode 1 (463161-463224): a candidate is
 * visible where it is (44B578), dynamic, seen, and on the first pass not
 * held or worn by the player, directly or inside something held (44615C,
 * which ignores openness).  A second pass admits held objects when the
 * first found no unique winner (46360D: any negative result, a tie
 * included).  hub `take drying-up cloth` with the sink cloth held, and
 * ilgolem `prendi mano mozzata del nonno` with the static "nonno" in the
 * room, both resolve uniquely in run400 (runner_transcripts/hub.txt:251,
 * ilgolem.txt:312) where a plain present-and-seen scan would tie.
 */
static scr_bool
lib_resolve_held_400 (scr_gameref_t game, scr_int object)
{
  switch (gs_object_position (game, object))
    {
    case OBJ_HELD_PLAYER:
    case OBJ_WORN_PLAYER:
      return TRUE;
    case OBJ_IN_OBJECT:
    case OBJ_ON_OBJECT:
      return lib_resolve_held_400 (game, gs_object_parent (game, object));
    default:
      return FALSE;
    }
}

scr_bool
lib_resolve_admit_take (scr_gameref_t game, scr_int object, scr_int pass)
{
  if (obj_is_static (game, object) || !gs_object_seen (game, object))
    return FALSE;
  if (!obj_indirectly_in_room (game, object, gs_playerroom (game)))
    return FALSE;
  return pass > 0 || !lib_resolve_held_400 (game, object);
}

/*
 * Mode 1 with a container named (463161-463224, arg <> &HFF): visible where
 * it is (44B578) and its parent the container.  Nothing else is tested.
 */
scr_bool
lib_resolve_admit_parent (scr_gameref_t game, scr_int object,
                          scr_int /* pass */)
{
  const scr_int parent = disambig.resolve_parent_400;

  if (parent < 0 || gs_object_parent (game, object) != parent)
    return FALSE;
  switch (gs_object_position (game, object))
    {
    case OBJ_ON_OBJECT:
      return TRUE;
    case OBJ_IN_OBJECT:
      return gs_object_openness (game, parent) <= OBJ_OPEN;
    default:
      return FALSE;
    }
}

static scr_int
lib_verb_object_resolve_400_scan (scr_gameref_t game, const scr_char *input,
                                  std::vector<scr_int> *tied,
                                  lib_resolve_admit_t admit, scr_int pass)
{
  scr_int index_, object, best, best_count;

  if (tied)
    tied->clear ();
  if (!input)
    return -2;

  object = -2;
  best = 0;
  best_count = 0;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      scr_int score;

      if (!admit (game, index_, pass))
        continue;

      score = lib_verb_object_name_score (game, index_, input);
      if (score == 0)
        continue;
      if (score > best)
        {
          object = index_;
          best = score;
          best_count = 1;
          if (tied)
            {
              tied->clear ();
              tied->push_back (index_);
            }
        }
      else if (score == best)
        {
          best_count++;
          if (tied)
            tied->push_back (index_);
        }
    }

  return best_count > 1 ? -1 : object;
}

scr_int
lib_verb_object_resolve_400_string (scr_gameref_t game, const scr_char *input,
                                    std::vector<scr_int> *tied,
                                    scr_bool present_only)
{
  return lib_verb_object_resolve_400_scan (game, input, tied,
                                           lib_resolve_admit_mode0,
                                           present_only ? 0 : 1);
}

/* Mode 1: two passes, the second only after no unique winner (46360D). */
scr_int
lib_take_resolve_400_string (scr_gameref_t game, const scr_char *input,
                             std::vector<scr_int> *tied)
{
  scr_int object;

  object = lib_verb_object_resolve_400_scan (game, input, tied,
                                             lib_resolve_admit_take, 0);
  if (object < 0)
    object = lib_verb_object_resolve_400_scan (game, input, tied,
                                               lib_resolve_admit_take, 1);
  return object;
}

static scr_int
lib_verb_object_resolve_400_common (scr_gameref_t game,
                                    std::vector<scr_int> *tied)
{
  return lib_verb_object_resolve_400_string (game, run_get_dispatch_input (),
                                             tied, TRUE);
}

/*
 * lib_verb_object_note_line_top()
 *
 * run400 resolves the catch-all's object once, at the top of the line:
 * MemVar_4942F8 is written only at 48A3FD, from Proc_21_58_463640, before
 * any task runs.  The catch-all at 48B19A then tests THAT object's presence
 * with obhere (48B1E1), after the tasks.  Noted here from run_all_commands()
 * ahead of task dispatch; -2 when there is none or the game is not 4.0.
 */
static scr_int lib_verb_object_line_top = -2;

void
lib_verb_object_note_line_top (scr_gameref_t game)
{
  lib_verb_object_line_top
    = lib_is_version_400 (game)
      ? lib_verb_object_resolve_400_common (game, nullptr) : -2;
}

scr_int
lib_verb_object_resolve_400 (scr_gameref_t game)
{
  return lib_verb_object_resolve_400_common (game, nullptr);
}

/*
 * lib_put_where_400()
 * lib_cmd_put_where_400()
 *
 * The 4.0 put parser (Proc 459DB4, branch 46DC34-46DD2C) takes a line
 * that starts "put", has neither " in " nor " on " in it and no word
 * "down", and that no task pre-matched (453C50 = 0): it asks the same noun
 * resolver as the catch-all (46DCC7, mode 0) and answers "Where do you
 * want to put <the object>?" for a unique winner (46DCDB) or "Where do
 * you want to put that?" (46DD19) for a tie or nothing scoring.  Both set
 * MemVar_494281 (46DD25), so like the catch-all the answer is not a turn.
 *
 * Measured 2026-09-06: thelasthour `put bowl near spyhole` answers "Where
 * do you want to put the spyhole?" and no event fires
 * (runner_probes/thelasthour.run400.spyhole.txt); Shadowpeak `put sword near
 * cell door` answers "Where do you want to put the sword?" and Seeker stays
 * quiet (runner_probes/shadowpeak.run400.celldoor.txt).  Scarier's catch-all
 * used to answer "I don't understand what you want me to do with the
 * bowl." and tick.
 *
 * Only lines the can't-see clause above lets through get here, which is
 * the Runner's order too (therest 4887C1 runs before the put parser: see
 * lib_cmd_unclear_object).  A line with "into"/"onto" is left to the put
 * handlers, as the Runner's " in "/" on " tests may be preceded by a
 * rewrite we have not read.
 */
scr_bool
lib_is_put_where_line_400 (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!lib_is_version_400 (game) || !input)
    return FALSE;
  /* The Runner's gate is the WHOLE WORD "put" anywhere in the line, not a
   * "put " at the front: put_drop_list is entered on c("put") Or c("drop")
   * and 46DC34 re-tests the same word.  So bare `put` and `blorp put` both
   * land here, both answering "Where do you want to put that?" (p4REW,
   * runner_probes/rew.run400.casc.txt).  See run_hoist_verb_line(). */
  if (!(lib_input_contains_word (input, "put")
         && !strstr (input, " in ") && !strstr (input, " on ")
         && !strstr (input, " into ") && !strstr (input, " onto ")
         && !lib_input_contains_word (input, "down")))
    return FALSE;
  /* A task pre-match skips the question (46DC85), and a held object then
     has the line claimed by insides; see lib_put_held_unsplit_400(). */
  return !(lib_put_held_unsplit_400 (game, input)
           && lib_task_prematches_input (game, 0));
}

static scr_bool
lib_put_where_400_common (scr_gameref_t game, scr_int resolved)
{
  const scr_filterref_t filter = gs_get_filter (game);

  if (resolved >= 0)
    {
      var_set_ref_object (gs_get_vars (game), resolved);
      lib_print_wrapped_object (game, "Where do you want to put ",
                                resolved, "?\n");
    }
  else
    pf_buffer_string (filter, "Where do you want to put that?\n");

  /* MemVar_494281's exit: not a turn, and with an ambiguity question open
   * the line is claimed as its answer (p4PUT2 `put coin on jar` / `put zzz
   * on jar` -> "That is still ambiguous!",
   * runner_probes/put2.run400.txt). */
  game->is_admin = TRUE;
  lib_co_400_note_refusal ();
  return TRUE;
}

static scr_bool
lib_put_where_400 (scr_gameref_t game, scr_int resolved)
{
  if (!lib_is_put_where_line_400 (game))
    return FALSE;
  return lib_put_where_400_common (game, resolved);
}

scr_bool
lib_cmd_put_where_400 (scr_gameref_t game)
{
  return lib_put_where_400 (game, lib_verb_object_resolve_400 (game));
}

/*
 * lib_put_where_question_400()
 *
 * The question put_drop_list's clauseless branch writes for the line being
 * dispatched, without printing it or ending the line: the Runner writes it
 * into the message buffer and falls out unclaimed, so a take on the same
 * line still has its turn (see run_put_take_400()).  FALSE for a line that
 * branch does not take.
 */
scr_bool
lib_put_where_question_400 (scr_gameref_t game, std::string *question)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t mark = pf_buffer_length (filter);
  scr_int resolved;

  if (!lib_is_put_where_line_400 (game))
    return FALSE;
  resolved = lib_verb_object_resolve_400 (game);
  if (resolved >= 0)
    {
      var_set_ref_object (gs_get_vars (game), resolved);
      lib_print_wrapped_object (game, "Where do you want to put ",
                                resolved, "?");
    }
  else
    pf_buffer_string (filter, "Where do you want to put that?");
  *question = pf_cut_tail (filter, mark);
  return TRUE;
}

/*
 * lib_cmd_put_container_400()
 *
 * 4.0's put/drop list parser (run400 put_drop_list Proc_19_40_459DB4 ->
 * name_object Proc_19_41_46E5D8) names the CONTAINER of a "put X in Y" /
 * "put X on Y" line before it ever looks at X, and three of its exits
 * answer for Y alone.  Every line holding the whole word "put" or "drop"
 * goes through it, ahead of the task dispatcher, and it starts with the
 * four plain Replace() calls run_normalise_put_line() repeats ("drop " ->
 * "put ", "inside"/"into" -> "in", "onto" -> "on").  Then (459BCD-459C39):
 *
 *   - a whole-word "in" splits the line at the first " in ";
 *   - a whole-word "on" splits it at the first " on " unless an " in " came
 *     earlier (Proc_19_44_4434F4 keeps the earlier positive), and -- for a
 *     line without "all" -- resolves Left(line, split), "put zzz ", with
 *     the noun scorer (Proc_21_58_463640 mode 0); when that names nothing
 *     the split is ZEROED, so the line reaches name_object with no
 *     preposition at all;
 *   - no split means the whole line.
 *
 * name_object (46DC34-46DE99) then:
 *
 *   - with no " in "/" on " at or after the split, and the whole word "put"
 *     but not "down", and no task pre-matching the line (453C50): "Where
 *     do you want to put <the X>?" for the line's unique winner, "Where do
 *     you want to put that?" otherwise, MemVar_494281 set -- NOT a turn.
 *     For a line that never had a preposition that is lib_cmd_put_where_400
 *     above; this handler adds the zeroed "on" split: `put zzz on desk`
 *     answers "Where do you want to put the desk?", `put zzz on box` "...
 *     the box?", `put zzz on yyy` "... that?";
 *   - else the text after the preposition (46DD34, Right past " in " /
 *     " on ") is resolved with the same scorer (46DD65, mode 0) and gated
 *     by co(i, 0) (46DD7F).  Nothing: "I don't understand what you want to
 *     put things inside." / "... onto." (46DDBC), MemVar_494281 set -- NOT
 *     a turn -- unless a task pre-matches the line, in which case the line
 *     falls through to the dispatcher untouched;
 *   - a winner that is not a container ("inside") or not a surface
 *     ("onto"): "<You> can't put anything inside/onto <the Y>!" (46DE47), a
 *     turn, again unless a task pre-matches;
 *   - a fitting winner goes on to name X at 46DE99 -- the ordinary put rows
 *     and lib_put_in_multiple_common()'s 46E142 clobber.
 *
 * All three of these gates call the pre-matcher UNFILTERED (46DCB2, 46DDAB
 * and 46DE29 push 0 for its class argument, so any task pattern matching
 * the line -- with a put word or without -- keeps the line for the
 * dispatcher); only the 46E142 clobber's gate at 46E15A, and put_drop_list's
 * own at 459B19, pass class mode 2.  herrdoktor turns on the difference:
 * its task 3 `*roll*jetpack*` has no put word, and `put roll in jetpack`
 * -- whose jetpack sits unseen inside the worn lab coat, so the scorer
 * names nothing -- must reach it (runner_transcripts/herrdoktor.txt).
 *
 * Measured 2026-09-08 on the hand-built p4PUT/p4PUT2 probes
 * (runner_probes/put.run400.txt, runner_probes/put2.run400.txt;
 * make_400_putprobe.py), 44 cells: `put coin in zzz`, `put
 * coin into zzz`, `drop coin in zzz`, `put zzz in yyy`, `put zzz in bob`
 * (an NPC is nothing here), `put all in zzz` all say "... put things
 * inside." with no tick; the "on"/"onto" spellings "... put things onto.";
 * `put zzz in desk` "You can't put anything inside the desk!" and ticks,
 * `put coin on box` "You can't put anything onto the box!" -- "onto", where
 * 3.7/3.8 store "on" as a literal and 3.9 is unmeasured, so the wording is
 * 4.0's alone.  A container the player has SEEN but left behind names
 * nothing: `put coin in bag` from the next room is "... put things inside."
 * even though `x bag` there says "You can't see the bag from here!"  (The
 * comment on lib_cmd_put_unclear() that the seen-but-absent clause speaks
 * first for a put line was never measured and is wrong for the container;
 * whether it holds for a seen-but-absent X is still unmeasured.)  Two
 * present containers sharing a Short raise the ordinary ambiguity prompt
 * (`put coin in jar` -> "Which jar.  The jar or the jar?"), so a tie is
 * left to the rows below.  A line with " and " at or beyond the split runs
 * put_drop_list's own clause loop (459C75), and its clauses reach this
 * handler one at a time; see lib_put_clauses_400().  (`put coin in zzz and
 * yyy` is not that loop at all -- the top-level splitter cuts it into two
 * commands, which is why "... put things inside." and the DontUnderstand
 * text are two turns; see run_find_split_400().)
 *
 * Pre-4.0 Runners have none of this; run390's put parser (461769) says
 * "<You> can't put anything <inside/on> that!" for an unknown container,
 * unmeasured and unported.
 */
static scr_bool
lib_phrase_has_word (const std::string &line, const scr_char *word)
{
  return lib_input_contains_word (line.c_str (), word);
}

/*
 * lib_put_split_400()
 *
 * The preposition split put_drop_list computes at 459BCD-459C4A: the first
 * whole-word " in ", or the first " on " when no " in " came earlier
 * (Proc_19_44_4434F4 keeps the earlier positive of the two).  With ON_TEST
 * set -- which is how the routine enters, but NOT how its clause loop
 * re-enters (459D11-459D6B has the two whole-word tests and nothing else)
 * -- a line without "all" also has Left(line, split) resolved by the noun
 * scorer, and a split whose left half names nothing is ZEROED, so the line
 * goes on with no preposition at all.  *ON_BRANCH, when asked for, says
 * whether that test ran.
 */
std::string::size_type
lib_put_split_400 (scr_gameref_t game, const std::string &line,
                   scr_bool has_all, scr_bool on_test, scr_bool *on_branch)
{
  std::string::size_type split, on_at;

  if (on_branch)
    *on_branch = FALSE;
  split = std::string::npos;
  if (lib_phrase_has_word (line, "in"))
    split = line.find (" in ");
  if (lib_phrase_has_word (line, "on"))
    {
      on_at = line.find (" on ");
      if (on_at != std::string::npos
          && (split == std::string::npos || split > on_at))
        split = on_at;
      if (on_test && split != std::string::npos && !has_all)
        {
          const std::string fragment = line.substr (0, split + 1);

          if (on_branch)
            *on_branch = TRUE;
          /*
           * And this one scorer runs UNGATED by co(): an object the player
           * has seen and walked away from still keeps the split.  p4SURF
           * under run400, 2026-09-12: `put stone on lamp` from the lit room
           * with the stone left behind in the cave is "You can't put
           * anything onto the lamp!" (runner_probes/surf3.run400.txt:6) -- the
           * split held and the lamp was weighed as a supporter -- where the
           * never-seen coin of `put coin on zzzz` zeroes it and the line falls
           * through to "Where do you want to put that?" (p4SURF,
           * runner_probes/surf2.run400.txt:15).  The right half of the split
           * keeps its co() gate: `put lamp on table` with the table a room
           * away but seen is "I don't understand what you want to put things
           * onto." (runner_probes/surf3.run400.txt:5).
           *
           * Only "nothing named" zeroes the split, not a tie.  Widening the
           * scorer past co() makes namesakes elsewhere in the game tie where
           * the present one used to win alone, and a tie must not be allowed
           * to undo a split the narrow scorer kept: provenance's two wooden
           * canteens turn `put canteen on altar` into "Where do you want to
           * put that?" (against runner_probes/provenance.run400.txt, which
           * puts it on the altar), and Dragon Shrine's two bodies do the same
           * to `put body on slab`.  The measured zeroing case is a name
           * nothing scores on at all -- `put coin on zzzz` with the coin never
           * seen (p4SURF, runner_probes/surf2.run400.txt:15).
           */
          if (lib_verb_object_resolve_400_string (game, fragment.c_str (),
                                                  nullptr, FALSE) == -2)
            split = std::string::npos;
        }
    }
  return split;
}

/*
 * lib_put_clauses_400()
 *
 * put_drop_list's own multi-clause loop (run400 loc_459C75).  With the
 * split in hand the routine looks for " and " AT OR BEYOND it (459C60, the
 * InStr starts at the split, and a line with no preposition splits at
 * Len(line), so its " and " is never found).  Each turn of the loop takes
 * Left(line, and_at - 1) as a clause and runs the whole of name_object on
 * it, drops the clause and its " and " from the line, puts "put " back on
 * the front if the remainder lost it, and recomputes the split -- this time
 * without the "on" scorer test and without the Len(line) fallback.  A
 * remainder with no preposition left therefore ends the loop and is DROPPED
 * unrun (459D8C pushes &HFF into the loop variable, which terminates it,
 * and the final name_object at 459D94 is gated on split > 0).
 *
 * So `put coin in box and hat in desk` is ONE turn that runs two puts, and
 * their two answers come out concatenated with no separator at all: "The
 * coin is too big to fit inside the box.You can't put anything inside the
 * desk!" (p4AND, runner_probes/and.run400.txt).  `put coin in box and hat in
 * desk and hat in box` runs three, implicit take included
 * (runner_probes/and.run400.b.txt), and `put coin in box and hat` runs the
 * first clause only and never answers for the hat.
 *
 * The clauses are handed back to run_game_commands_common(), which is where
 * Scarier's equivalent of name_object lives -- the priority put rows, run
 * once per clause.  A line whose " and " the top-level splitter would have
 * cut never gets here: run_find_split_400() has already carved it into
 * separate commands, and only an " and " followed by an object name (which
 * is what "hat in desk" starts with) survives to reach this routine.
 */
scr_bool
lib_put_clauses_400 (scr_gameref_t game, const scr_char *input,
                     std::vector<std::string> &clauses)
{
  std::string line;
  std::string::size_type split, and_at;
  scr_bool has_all;

  clauses.clear ();
  if (!lib_is_version_400 (game) || !input)
    return FALSE;
  if (!lib_input_contains_word (input, "put")
      && !lib_input_contains_word (input, "drop"))
    return FALSE;

  line = run_normalise_put_line (input);
  if (line.empty ())
    return FALSE;
  has_all = lib_phrase_has_word (line, "all");

  split = lib_put_split_400 (game, line, has_all, TRUE, nullptr);
  if (split == std::string::npos)
    split = line.length () - 1;               /* 459C55: split = Len(line) */

  and_at = line.find (" and ", split);
  if (and_at == std::string::npos)
    return FALSE;

  while (and_at != std::string::npos)
    {
      clauses.push_back (line.substr (0, and_at));

      line = line.substr (and_at + 5);
      if (line.compare (0, 4, "put ") != 0)
        line = "put " + line;

      split = lib_put_split_400 (game, line, has_all, FALSE, nullptr);
      and_at = (split == std::string::npos)
               ? std::string::npos : line.find (" and ", split);
    }
  if (split != std::string::npos)
    clauses.push_back (line);
  return TRUE;
}

/*
 * lib_put_task_tie_400()
 *
 * A put line with no in/on split goes to name_object whole (459C55), and
 * when a task pre-matches it (453C50, unfiltered) 46DC85 skips "Where do
 * you want to put ..." for the mode-2 scorer at 46DE99.  Its flat tie exit
 * at 46E192 has no task gate: "It is not clear which <term> you are
 * referring to." prints and the dispatcher then runs the task, joined on.
 * makeshift T7 `put needle through balloon`, the held large round balloon
 * and the loose long balloons both aliased "balloon"
 * (runner_transcripts/makeshift.txt).  A unique winner or a tie with a
 * pending object is unmeasured here and prints nothing.
 *
 * The line is not a named put row, so run_all_commands() calls this ahead
 * of its task passes.  Returns TRUE when it has printed, the line left
 * unclaimed for the tasks.
 */
scr_bool
lib_put_task_tie_400 (scr_gameref_t game, const scr_char *input)
{
  const scr_filterref_t filter = gs_get_filter (game);
  std::vector<scr_int> marked;
  std::string line;
  scr_bool on_branch;
  scr_int pending, last_tied, mark_count;

  if (!lib_is_version_400 (game) || !input
      || !lib_input_contains_word (input, "put"))
    return FALSE;
  line = run_normalise_put_line (input);
  if (lib_phrase_has_word (line, "all")
      || line.find (" and ") != std::string::npos
      || lib_put_split_400 (game, line, FALSE, TRUE, &on_branch)
         != std::string::npos
      || !lib_phrase_has_word (line, "put")
      || lib_phrase_has_word (line, "down")
      || !lib_task_prematches_input (game, 0))
    return FALSE;

  if (lib_name_object_resolve_400 (game, line.c_str (), 2, &pending,
                                   &last_tied, &marked, &mark_count) != -1
      || pending >= 0)
    return FALSE;

  lib_print_not_clear_which_400
    (game, lib_drop_named_term_400 (game, last_tied, line.c_str (), FALSE));
  pf_buffer_character (filter, '\n');
  return TRUE;
}

/*
 * lib_put_held_unsplit_400()
 *
 * A put line with no in/on split whose named object is HELD belongs to
 * run400's insides handler (46639C), which offers the line to put-family
 * tasks only (453C50 class 2) and claims it whatever they do.  amnesiakid
 * T46 `put M-80 under bookshelf`, the M-80 in hand: the task `* M-80 *
 * bookshelf` names no put word, so it never runs, and the Runner prints
 * the game's DontUnderstand text ("Shut Up!!") -- where marika's `put
 * blanket under door`, the blanket on the bed, reaches its task.
 */
scr_bool
lib_put_held_unsplit_400 (scr_gameref_t game, const scr_char *input)
{
  std::vector<scr_int> marked;
  std::string line;
  scr_bool on_branch;
  scr_int object, pending, last_tied, mark_count;

  if (!lib_is_version_400 (game) || !input
      || !lib_input_contains_word (input, "put"))
    return FALSE;
  line = run_normalise_put_line (input);
  if (lib_phrase_has_word (line, "all")
      || line.find (" and ") != std::string::npos
      || lib_put_split_400 (game, line, FALSE, TRUE, &on_branch)
         != std::string::npos
      || on_branch
      || !lib_phrase_has_word (line, "put")
      || lib_phrase_has_word (line, "down"))
    return FALSE;

  object = lib_name_object_resolve_400 (game, line.c_str (), 2, &pending,
                                        &last_tied, &marked, &mark_count);
  return object >= 0
         && gs_object_position (game, object) == OBJ_HELD_PLAYER;
}

scr_bool
lib_cmd_put_container_400 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  std::string line, phrase;
  std::string::size_type split, in_at, on_at;
  const scr_char *preposition;
  scr_bool has_all, on_branch;
  scr_int container;

  if (!lib_is_version_400 (game) || !input)
    return FALSE;
  if (!lib_input_contains_word (input, "put")
      && !lib_input_contains_word (input, "drop"))
    return FALSE;

  line = run_normalise_put_line (input);
  has_all = lib_phrase_has_word (line, "all");
  split = lib_put_split_400 (game, line, has_all, TRUE, &on_branch);

  /*
   * An " and " at or beyond the split is put_drop_list's clause loop, and
   * the clauses reach this handler one at a time from
   * run_game_commands_common() -- so a line that still holds one is not
   * this call's to answer.  (With no split at all the Runner looks for the
   * " and " from Len(line) and so never finds one; the whole-line branches
   * below are left out of this all the same, which is where the list loops
   * at 46E04E / 46E0B2 -- unported -- would speak.)
   */
  if (line.find (" and ", split == std::string::npos ? 0 : split)
      != std::string::npos)
    return FALSE;

  if (split == std::string::npos)
    {
      /*
       * Only the zeroed "on" split is ours; a line with no preposition at
       * all is lib_cmd_put_where_400's, further down the standard table.
       */
      if (!on_branch
          || !lib_phrase_has_word (line, "put")
          || lib_phrase_has_word (line, "down"))
        return FALSE;
      if (lib_task_prematches_input (game, 0))
        return FALSE;
      /*
       * The question does not claim the line: wears() runs after
       * put_drop_list and overwrites it whenever it can act, so `put on
       * <held wearable>` is the wear and `put on <worn>` "You are already
       * wearing ...".  See lib_wear_would_act_390().
       */
      if (lib_wear_would_act_390 (game))
        return FALSE;
      return lib_put_where_400_common
               (game, lib_verb_object_resolve_400_string (game, line.c_str (),
                                                          nullptr, TRUE));
    }

  in_at = line.find (" in ", split);
  on_at = line.find (" on ", split);
  if (in_at != std::string::npos)
    {
      preposition = "inside";
      phrase = line.substr (in_at + 4);
    }
  else if (on_at != std::string::npos)
    {
      preposition = "onto";
      phrase = line.substr (on_at + 4);
    }
  else
    return FALSE;

  container = lib_verb_object_resolve_400_string (game, phrase.c_str (),
                                                 nullptr, TRUE);
  if (container == -1)
    return FALSE;
  if (container == -2)
    {
      if (lib_task_prematches_input (game, 0))
        return FALSE;
      pf_buffer_string (filter,
                        "I don't understand what you want to put things ");
      pf_buffer_string (filter, preposition);
      pf_buffer_string (filter, ".\n");
      game->is_admin = TRUE;
      lib_co_400_note_refusal ();
      return TRUE;
    }

  if ((strcmp (preposition, "inside") == 0
       && !obj_is_container (game, container))
      || (strcmp (preposition, "onto") == 0
          && !obj_is_surface (game, container)))
    {
      std::string second, first, third;

      if (lib_task_prematches_input (game, 0))
        return FALSE;
      /* Deferred in the tentative priority pass; see lib_put_in_is_valid. */
      if (run_priority_defer_if_active ())
        return FALSE;
      second = std::string ("You can't put anything ") + preposition + " ";
      first = std::string ("I can't put anything ") + preposition + " ";
      third = std::string ("%player% can't put anything ") + preposition + " ";
      lib_print_response_object (game, second.c_str (), first.c_str (),
                                 third.c_str (), container, "!\n");
      return TRUE;
    }

  return FALSE;
}

/*
 * lib_put_container_fits_400()
 *
 * TRUE when name_object's container look-up above (46DD34-46DD65) names a
 * container for an " in " line or a surface for an " on " line.  run400
 * resolves the container with its noun scorer, which is indifferent to
 * words that belong to no object, so `put my homework on my desk` puts the
 * homework on "your school desk" (Prefix "your school") ahead of every
 * task -- schoolday T9, where TASK 10 `*homework*desk*` would have run
 * (runner_transcripts/schoolday.txt).  Scarier's put rows bind their
 * %object% in place, and "my desk" is not a spelling of the desk, so
 * run_is_put_command() asks this before it retries them by containment.
 */
scr_bool
lib_put_container_fits_400 (scr_gameref_t game, const scr_char *input)
{
  std::string line, phrase;
  std::string::size_type split, in_at, on_at;
  scr_int container;
  scr_bool inside;

  if (!lib_is_version_400 (game) || !input)
    return FALSE;
  line = run_normalise_put_line (input);
  if (line.empty () || !lib_phrase_has_word (line, "put"))
    return FALSE;
  split = lib_put_split_400 (game, line, lib_phrase_has_word (line, "all"),
                             TRUE, nullptr);
  if (split == std::string::npos
      || line.find (" and ", split) != std::string::npos)
    return FALSE;

  in_at = line.find (" in ", split);
  on_at = line.find (" on ", split);
  if (in_at != std::string::npos)
    {
      inside = TRUE;
      phrase = line.substr (in_at + 4);
    }
  else if (on_at != std::string::npos)
    {
      inside = FALSE;
      phrase = line.substr (on_at + 4);
    }
  else
    return FALSE;

  container = lib_verb_object_resolve_400_string (game, phrase.c_str (),
                                                 nullptr, TRUE);
  if (container < 0)
    return FALSE;
  return inside ? obj_is_container (game, container)
                : obj_is_surface (game, container);
}

/*
 * lib_cmd_verb_object()
 * lib_cmd_verb_npc()
 *
 * Handlers for unrecognized verbs with known object/NPC.
 */
/*
 * lib_verb_object_catch_all_pre390()
 *
 * The 3.7/3.8 catch-all for a line no handler answered: run380 442F5D-443134
 * (run370 has the same loop).  It walks every object co() matches, in index
 * order, and keeps one message:
 *
 *   seen and present   "I don't understand what you want me to do with X."
 *   seen, not present  "<player> must be in the same room as X to be able
 *                       to do anything with it."
 *   never seen         "What <Short>?"
 *
 * A seen object speaks while nothing has been said yet or while a seen
 * object has already spoken, and the present answer outranks the absent
 * one, so the first present object on the line wins; an unseen object
 * speaks only into an empty buffer.  co() never reads the seen byte before
 * 3.9, so an unseen object is matched and asked about -- `frob stone` in the
 * Test Room of p38EXAM, the stone not yet listed, is "What stone?"
 * (runner_probes/exam.run380.verb.rtf, and the same in 3.7's
 * runner_probes/exam.run370.verb.rtf).
 *
 * 3.7's therest() refuses an absent object with "You can't see X." before
 * the catch-all is reached (see run_therest_absent_370()), so 3.7 never
 * hears the same-room answer: every absent line in
 * runner_probes/exam.run370.verb.rtf is "You can't see the X.", and only the
 * unseen-present answer is carried to 3.7, at the tail of
 * lib_cmd_verb_object().  This is 3.8's.  Its therest()
 * checks only the first present
 * object (443C69), so an absent one falls through to this loop: `frob
 * stone`, `z stone` and `eat statue` from the wrong room are "You must be in
 * the same room as the X to be able to do anything with it.", and `frob
 * coin`, the coin never listed out of its crate, is "What coin?".
 *
 * Returns the present object the caller goes on to answer for, -2 once this
 * has printed its own answer, or -1 when there is nothing to say.
 *
 * The walk's membership is the Runner's, not our matcher's: run380 442F5D
 * asks co() about EVERY object, so every object whose Short or one of whose
 * Aliases is in the line is a candidate and index order alone decides which
 * of them speaks.  Our references bind one candidate off the line, which
 * puts the answer in line order instead: with a gem at index 0 aliased
 * "stone" and a rock at index 1, run370 and run380 answer both `zug rock
 * stone` and `zug stone rock` with "I don't understand what you want me to
 * do with the gem." (p37/p38TEXTSRC, runner_probes/textsrc.run370.rtf,
 * runner_probes/textsrc.run380.rtf, 2026-09-20), where we used to answer the
 * rock for the first line.  3.70 resolves the alias here although its takes()
 * test is a bare c(Short) -- `blip stone` against a game with no matching task
 * is the gem's catch-all in both older Runners -- so the scan below reads
 * the Aliases at both versions, unlike lib_co_pre400().
 */
scr_bool
lib_catch_all_names_pre390 (scr_gameref_t game, scr_int object)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *line = run_get_dispatch_input ();
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  if (!line)
    return FALSE;
  if (lib_co_contains (line, prop_get_indexed_string (bundle, "Objects",
                                                      object, "Short")))
    return TRUE;

  alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
  for (alias = 0; alias < alias_count; alias++)
    {
      vt_key[3].integer = alias;
      if (lib_co_contains (line, prop_get_string (bundle, "S<-sisi", vt_key)))
        return TRUE;
    }
  return FALSE;
}

/*
 * The refusal for an object the player has seen that is not in the room,
 * which becomes the referenced object as it is named.
 */
static void
lib_must_share_room_with_object (scr_gameref_t game, scr_int object)
{
  var_set_ref_object (gs_get_vars (game), object);
  lib_print_response_object (game,
                             "You must be in the same room as ",
                             "I must be in the same room as ",
                             "%player% must be in the same room as ",
                             object,
                             " to be able to do anything with it.\n");
}

static scr_int
lib_verb_object_catch_all_pre390 (scr_gameref_t game)
{
  const scr_filterref_t filter = gs_get_filter (game);
  scr_int object, rank, pick, unseen;

  rank = 0;
  pick = -1;
  unseen = -1;
  GS_FOR_EACH_OBJECT (game, object)
    {
      if (!game->object_references[object]
          && !lib_catch_all_names_pre390 (game, object))
        continue;

      /*
       * co()'s crowded arm refuses a present namesake whose Prefix's last
       * word the line lacks, so `poke hat` beside a red and a blue hat
       * walks nothing: the buffer stays empty, no tick runs, and the
       * end-of-turn "Which hat." answers even when an event runs a task
       * every turn (p38EVQ2, runner_probes/evq2.run380.rtf, 2026-09-20).
       */
      if (run_get_dispatch_input ()
          && lib_catch_all_names_pre390 (game, object)
          && !lib_co_pre400 (game, run_get_dispatch_input (), object, 0))
        continue;

      if (gs_object_seen (game, object))
        {
          if (!(unseen == -1 || rank > 0))
            continue;

          if (obj_indirectly_in_room (game, object, gs_playerroom (game)))
            {
              if (rank < 2)
                {
                  pick = object;
                  rank = 2;
                }
            }
          else if (rank < 1)
            {
              pick = object;
              rank = 1;
            }
        }
      else if (unseen == -1 && rank == 0)
        unseen = object;
    }

  if (rank == 2)
    return pick;

  if (rank == 1)
    {
      lib_must_share_room_with_object (game, pick);
      return -2;
    }

  if (unseen >= 0)
    {
      pf_buffer_string (filter, "What ");
      pf_buffer_string (filter,
                        prop_get_indexed_string (gs_get_bundle (game),
                                                 "Objects", unseen, "Short"));
      pf_buffer_string (filter, "?\n");
      return -2;
    }

  return -1;
}

/*
 * lib_therest_with_silent_400()
 *
 * Whether run400's therest leaves a " with " line without a word: it splits
 * at the first " with " (4883C5) and resolves each half with 463640 in mode
 * 0 (present and seen, then seen only; lib_with_half_400()), and a half
 * that resolves nothing restores the line and exits (488430 for the head,
 * 4884DB for the tail) ahead of every verb arm and of the can't-see clause
 * 4887A0.  On TRUE, *top is the whole line's mode-0 object, what the
 * unhandled-verb catch-all 48B19A then speaks for (MemVar_4942F8), or -1.
 */
static scr_bool
lib_therest_with_silent_400 (scr_gameref_t game, scr_int *top)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *tail = input ? strstr (input, " with ") : nullptr;
  std::string head;

  *top = -1;
  if (!lib_is_version_400 (game) || !tail)
    return FALSE;

  head.assign (input, tail - input);
  if (lib_with_half_400 (game, head.c_str ()) >= 0
      && lib_with_half_400 (game, tail + 6) >= 0)
    return FALSE;

  *top = lib_with_half_400 (game, input);
  return TRUE;
}

/*
 * What one stage of lib_cmd_verb_object() tells the driver: go on to the
 * next stage, or return TRUE (the line is answered, or a question about it
 * is raised) or FALSE (not ours: the later rows and then the game's
 * DontUnderstand text get the line).
 */
enum lib_vo_stage_t
{
  LIB_VO_GO_ON,
  LIB_VO_RETURN_TRUE,
  LIB_VO_RETURN_FALSE
};

/*
 * The stages of lib_cmd_verb_object(), in the order it runs them.  They
 * share its count and object: the number of objects the line names and the
 * one it answers for, which is meaningful only while the count is 1.
 */
static void
lib_verb_object_count_present (scr_gameref_t game,
                               scr_int *count, scr_int *object)
{
  scr_int index_;

  /* Ensure the reference is unambiguous. */
  *count = 0;
  *object = -1;
  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (game->object_references[index_]
          && (gs_object_seen (game, index_)
              || !lib_matcher_requires_seen (game))
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        {
          (*count)++;
          *object = index_;
        }
    }
}

/*
 * 4.0: two present objects the player named by their shared Short is
 * the Runner's ambiguity prompt, and it comes out here -- our
 * positional matcher binds both trees for `chop tree`, so the count is
 * 2 and the 4.0 resolver below is never reached.  A bare `put` is left
 * alone: lib_cmd_put_where_400() answers it with "Where do you want to
 * put that?", which is what the tie was measured to give there.
 *
 * The other way round, our matcher can bind an object that is not
 * here while the line also names one that is.  run400 resolves the
 * noun from the present, seen objects alone (48A3F5), so the present
 * one wins outright: The Magic Show's `show rabbit to audience`, the
 * rabbit back in the worn hat, answers "I don't understand what you
 * want to do with the audience."
 * (runner_probes/magicshow.run400.a.txt:47,
 * runner_probes/magicshow.run400.b.txt:40).  A unique winner goes on to
 * the catch-all below as if our own count had found it.
 */
static lib_vo_stage_t
lib_verb_object_tie_400 (scr_gameref_t game, scr_int *count, scr_int *object)
{
  if (lib_is_version_400 (game) && !lib_is_put_where_line_400 (game))
    {
      std::vector<scr_int> tied, crowd;
      const scr_int resolved =
          lib_verb_object_resolve_400_common (game, &tied);

      /*
       * A " with " line asks about one half of itself, though the object
       * it answers for stays the whole line's -- hcw's `unlock door with
       * keys` speaks for the keys.  See lib_with_split_crowd_400().
       */
      scr_int with_pending, with_head;

      disambig.element.therest_split = TRUE;
      if (lib_with_split_crowd_400 (game, FALSE, &crowd, &with_pending,
                                    &with_head))
        tied = crowd;
      if (lib_co_400_raise_for_with_tail (game, with_pending, crowd))
        return LIB_VO_RETURN_TRUE;
      if (resolved == -1
          && lib_co_400_raise_for_short_tie (game, tied))
        return LIB_VO_RETURN_TRUE;
      if (resolved >= 0)
        {
          *count = 1;
          *object = resolved;
        }
    }
  return LIB_VO_GO_ON;
}

/*
 * 3.7 reaches the same walk as 3.8's catch-all only for its unseen arm
 * (its therest() has already refused an absent object), but the object
 * it speaks for is
 * picked the same way: the first present one the line names, in index
 * order.  p37TEXTSRC answers `zug rock stone` with the gem, index 0 and
 * aliased "stone", not with the rock our references bound.
 */
static void
lib_verb_object_first_named_370 (scr_gameref_t game,
                                 scr_int *count, scr_int *object)
{
  scr_int index_;

  GS_FOR_EACH_OBJECT (game, index_)
    {
      if (lib_catch_all_names_pre390 (game, index_)
          && (gs_object_seen (game, index_)
              || !lib_matcher_requires_seen (game))
          && obj_indirectly_in_room (game, index_, gs_playerroom (game)))
        {
          *count = 1;
          *object = index_;
          break;
        }
    }
}

/*
 * 3.9: the catch-all is a walk of the objects in index order (run390
 * 4601D4-460284) -- co(obj, 0), the seen byte, obhere() -- and the FIRST
 * present match is the one named (var_CA = 2 shuts the later ones out).
 * co() with two present namesakes needs the Prefix's last word in the
 * line, so a bare `red box` beside the blue box names the red one and
 * `box` alone names neither (the end-of-turn "Which box." asks instead).
 * Our matcher's references are the wrong instrument for that: `red box`
 * binds both boxes.  runner_probes/task.run390.pfx.txt T35-37 (`red box`,
 * `red hat`, `blue hat`), 2026-09-19.
 */
static void
lib_verb_object_first_named_390 (scr_gameref_t game,
                                 scr_int *count, scr_int *object)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  scr_int index_;

  *count = 0;
  *object = -1;
  for (index_ = 0; input && index_ < gs_object_count (game); index_++)
    {
      if (gs_object_seen (game, index_)
          && obj_indirectly_in_room (game, index_, room)
          && lib_co_pre400 (game, input, index_, 0))
        {
          *count = 1;
          *object = index_;
          break;
        }
    }
}

/*
 * The line names no single present object: say that an absent one cannot
 * be seen, or leave the line alone.  4.0 may still find the object it
 * answers for here.
 */
static lib_vo_stage_t
lib_verb_object_absent (scr_gameref_t game, scr_int *count, scr_int *object)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int index_;

  /*
   * 3.9: co(obj, 0) (run390 43B6BC) matches only an object that is both
   * present and seen, so a line naming nothing here matches nothing, the
   * catch-all at 4601D4 stays silent and the DontUnderstand text answers.
   * p39EXAM (runner_probes/exam.run390.verb.txt): `frob stone`, `z stone`,
   * `frob statue` and `eat statue` from the wrong room are all "I don't
   * understand."
   */
  if (lib_is_version_390 (game))
    return LIB_VO_RETURN_FALSE;

  /*
   * No object of that name is here.  Before giving up on the command --
   * which hands it to the game's DontUnderstand text -- see whether the
   * name refers unambiguously to an object that is simply elsewhere, and
   * if it does, say so.  Every Runner from 3.7 to 4.0 answers an
   * otherwise unhandled command this way: therest() resolves a noun,
   * and when obhere() says the object is not present it prints
   * "<player> can't see <the object>." and returns without running any
   * of the verb branches below it (run370 43D169 @Form1.frm:3336,
   * run380 443C6A, run400 4887C1 @mdlSpreadTheLoad.bas:41196).  The
   * clause is reached only when nothing else produced output, which is
   * exactly where we are.
   *
   * Measured on hauntedhouse.taf (runner_probes/hauntedhouse.run400.txt,
   * turn 34): "melt statue" from the Front porch, with the statue in the
   * Entrance, answers "You can't see the statue." and not the game's own
   * DontUnderstand text.
   *
   * 4.0 picks that object with the 463640 score, not with our matcher's
   * references: House's `5 7 9 6 2 7 3 1 9` on the Landing, house number
   * 7 (alias "7") seen outside, is "Huh?" in run400
   * (runner_probes/house_sober.run400.txt, T275) -- the line scores no
   * unique seen object, so the DontUnderstand text answers.
   */
  if (lib_is_version_400 (game))
    {
      /*
       * therest splits the line at its first " with " before anything
       * else (4883C5) and leaves SILENTLY when either half resolves no
       * object (488430 / 4884DB), so its can't-see clause 4887A0 never
       * speaks for such a line; the unhandled-verb catch-all 48B19A
       * does, on the line-top 463640 object (MemVar_4942F8, 48A3FD:
       * present and seen first, then seen only).  Seen and absent is
       * its second arm, 48B24B, "must be in the same room as", and a
       * turn.  wonderland T7-T10 `attack card guard with knife`, the
       * knife two rooms back: dobattle names nothing present, the head
       * "attack card guard" resolves no object, and run400 answers
       * "You must be in the same room as the ethereal knife to be able
       * to do anything with it." with the Card Guard's blow appended
       * (runner_transcripts/wonderland.txt, 2026-09-25).
       */
      scr_int top;

      if (!lib_therest_with_silent_400 (game, &top))
        return lib_cant_see_absent_object (game, ".\n", TRUE)
                   ? LIB_VO_RETURN_TRUE : LIB_VO_RETURN_FALSE;
      if (top < 0)
        return LIB_VO_RETURN_FALSE;
      if (gs_object_seen (game, top)
          && !obj_indirectly_in_room (game, top, gs_playerroom (game)))
        {
          lib_must_share_room_with_object (game, top);
          return LIB_VO_RETURN_TRUE;
        }
      *count = 1;
      *object = top;
    }

  if (*count != 1)
    {
      *count = 0;
      *object = -1;
      GS_FOR_EACH_OBJECT (game, index_)
        {
          if (game->object_references[index_]
              && (gs_object_seen (game, index_)
                  || !lib_matcher_requires_seen (game)))
            {
              (*count)++;
              *object = index_;
            }
        }
      if (*count != 1)
        return LIB_VO_RETURN_FALSE;

      var_set_ref_object (vars, *object);
      lib_print_response_object (game,
                                 "You can't see ",
                                 "I can't see ",
                                 "%player% can't see ",
                                 *object, ".\n");
      return LIB_VO_RETURN_TRUE;
    }
  return LIB_VO_GO_ON;
}

/*
 * 4.0: the object is the resolver's, not the first one our `* %object% *`
 * row bound, and a tie between two present objects' names means the
 * Runner says nothing here -- see lib_verb_object_resolve_400().
 */
static lib_vo_stage_t
lib_verb_object_pick_400 (scr_gameref_t game, scr_int *object)
{
  std::vector<scr_int> tied, crowd;
  const scr_int resolved =
      lib_verb_object_resolve_400_common (game, &tied);

  scr_int with_pending, with_head;

  /* One half asks the question; see lib_with_split_crowd_400(). */
  disambig.element.therest_split = TRUE;
  if (lib_with_split_crowd_400 (game, FALSE, &crowd, &with_pending,
                                &with_head))
    tied = crowd;
  if (lib_put_where_400 (game, resolved))
    return LIB_VO_RETURN_TRUE;
  if (lib_co_400_raise_for_with_tail (game, with_pending, crowd))
    return LIB_VO_RETURN_TRUE;
  if (resolved == -1)
    {
      /*
       * A tie the player named by Short is the Runner's own ambiguity
       * prompt -- `chop tree` with two trees called "tree" asks "Which
       * tree.  The red tree or the blue tree?", while the alias ties
       * `chop shed` and `chop keys` fall through to DontUnderstand as
       * before.  See lib_co_400_raise().
       */
      if (lib_co_400_raise_for_short_tie (game, tied))
        return LIB_VO_RETURN_TRUE;
      return LIB_VO_RETURN_FALSE;
    }
  if (resolved >= 0)
    *object = resolved;
  return LIB_VO_GO_ON;
}

/*
 * 4.0: the object is the one the line resolved to before its task ran (see
 * lib_verb_object_note_line_top()), and a task that took it away leaves
 * the second arm of the catch-all, 48B24B-48B282: "You must be in the
 * same room as <the object> to be able to do anything with it."  That arm
 * sets no MemVar_494281, so unlike the first it is a turn.  seaside's
 * `do form` (runner_transcripts/seaside.txt:123): silent TASK3 hides the
 * leisure access card form and hands over the completed form, which is what
 * our own resolution after the task found.
 */
static scr_bool
lib_verb_object_top_taken_400 (scr_gameref_t game, scr_int object)
{
  const scr_int top = lib_verb_object_line_top;

  if (top >= 0 && top != object && gs_object_seen (game, top)
      && !obj_indirectly_in_room (game, top, gs_playerroom (game)))
    {
      lib_must_share_room_with_object (game, top);
      return TRUE;
    }
  return FALSE;
}

/* The answer itself, for the one object the stages above settled on. */
static scr_bool
lib_verb_object_answer (scr_gameref_t game, scr_int object)
{
  const scr_var_setref_t vars = gs_get_vars (game);

  /* Save in variables. */
  var_set_ref_object (vars, object);

  /*
   * 4.0: this answer is not a turn.  The catch-all at run400 48B19A-48B30A
   * stores 1 in MemVar_494281 (48B232), and the end-of-turn walk+event tick
   * at 48B599 runs only while that flag is 0; the DontUnderstand path a tie
   * takes (48B573) jumps past the tick too.  So neither answer moves an NPC
   * or advances an event.  Shadowpeak's second `attack margo with sword`
   * used to be followed by "Seeker hums!" here; run400 is silent.
   */
  if (lib_is_version_400 (game))
    game->is_admin = TRUE;

  /*
   * Print don't understand message.  run400's generaltasks composes this
   * object as the antecedent in the definite form (loc_48A409-48A42E):
   * `throw shovel` then `x it` echoes "(the shovel)" -- see
   * uip_definite_form() in scparser.cpp.
   */
  /*
   * 3.7: therest() has already refused an absent object, so the catch-all
   * only ever meets present ones -- and one the player has not been shown
   * is "What <Short>?" (see lib_verb_object_catch_all_pre390()).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_380
      && !gs_object_seen (game, object))
    {
      pf_buffer_string (gs_get_filter (game), "What ");
      pf_buffer_string (gs_get_filter (game),
                        prop_get_indexed_string (gs_get_bundle (game),
                                                 "Objects", object, "Short"));
      pf_buffer_string (gs_get_filter (game), "?\n");
      return TRUE;
    }

  if (lib_is_version_400 (game))
    uip_note_antecedent_400 (object, UIP_IT_DEFINITE, UIP_STAGE_SCORER);
  lib_print_wrapped_object (game, "I don't understand what you want me to do with ",
                            object, ".\n");
  lib_non_answer = TRUE;
  lib_co_400_note_refusal ();
  lib_battle_who_note_unanswered ();
  return TRUE;
}

scr_bool
lib_cmd_verb_object (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  scr_int count, object;
  lib_vo_stage_t stage;

  lib_verb_object_count_present (game, &count, &object);

  if (count != 1)
    {
      stage = lib_verb_object_tie_400 (game, &count, &object);
      if (stage != LIB_VO_GO_ON)
        return stage == LIB_VO_RETURN_TRUE;
    }

  if (version == TAF_VERSION_370)
    lib_verb_object_first_named_370 (game, &count, &object);

  /*
   * 3.8: the Runner's own catch-all speaks here, walking every object
   * the line names in index order -- see lib_verb_object_catch_all_pre390().
   * It hands back the present object the rest of this handler answers for,
   * or says its piece itself.
   */
  if (version == TAF_VERSION_380)
    {
      const scr_int present = lib_verb_object_catch_all_pre390 (game);

      if (present == -2)
        return TRUE;
      if (present >= 0)
        {
          count = 1;
          object = present;
        }
      else
        return FALSE;
    }

  if (lib_is_version_390 (game))
    lib_verb_object_first_named_390 (game, &count, &object);

  if (count != 1)
    {
      stage = lib_verb_object_absent (game, &count, &object);
      if (stage != LIB_VO_GO_ON)
        return stage == LIB_VO_RETURN_TRUE;
    }

  if (lib_is_version_400 (game))
    {
      stage = lib_verb_object_pick_400 (game, &object);
      if (stage != LIB_VO_GO_ON)
        return stage == LIB_VO_RETURN_TRUE;
    }

  /*
   * 4.0: a task that has just ended the game takes this answer off the line.
   * run400's generaltasks tests the gameover byte right after the turn
   * counter -- `If MemVar_4941AD <> 0 Then GoTo loc_48B4E3` at loc_48AC62 --
   * and 48B4E3 is the tail of the routine, past the object-counting loop at
   * 48AFF0 and past this catch-all at 48B19A.  The verb branches ahead of
   * 48AC62 (open/close 48A515, movement 48A5D8, wear, remove, look, the wait
   * loop) all still run, so an ending does not silence the library as such;
   * only the unhandled-verb tail is lost, and what the tail does print is
   * characters() (48B56E) and then, with the message buffer still empty and
   * no character named, the game's DontUnderstand text (48B58F).
   *
   * Measured live on relojero.taf (4.00, run400
   * runner_probes/relojero.run400.b.txt, 2026-09-07):
   * from the state the walkthrough reaches at `x trozo`, holding both the
   * Phoenix and the broken cord, `pulir fenix`, bare `fenix`, `pulir trozo`,
   * `tirar fenix`, `dar fenix` and `poner fenix` all get this catch-all
   * (which the game's ALR rewrites to "Extranos pensamientos afloran en mi
   * mente a proposito de <object>."), while `arreglar fenix` -- the one line
   * of the seven that matches a task, T5, which is silent and whose only
   * action is End Game (win) -- answers "Disculpa pero no te entiendo.", the
   * game's DontUnderstand from plain line 25, and only then prints the
   * WinText.
   *
   * It is the ending and not the task that does this.  A silent task on its
   * own leaves the catch-all family alone: seaside's T25 `do form` runs task
   * 3 (hides two objects, moves the form to the player, scores) and run400
   * still answers with an object message, "You must be in the same room as
   * the leisure access card form to be able to do anything with it."
   * (runner_transcripts/seaside.txt:123).
   *
   * Returning FALSE hands the line to `put *` and `* %character% *` and then
   * to the DontUnderstand text in run_element_not_understood(), which is the
   * tail's order too.
   */
  if (lib_is_version_400 (game) && game->pending_endgame != 0)
    return FALSE;

  if (lib_is_version_400 (game)
      && lib_verb_object_top_taken_400 (game, object))
    return TRUE;

  return lib_verb_object_answer (game, object);
}

/*
 * lib_npc_absent_or_unknown()
 *
 * The other two arms of the character catch-all.  run400's characters()
 * walks every NPC (the outer loop 47F2C5..48066E) and, for the first one the
 * line refers to while nothing has been printed yet (4805DA-4805F1:
 * `MemVar_4941B0 = vbNullString And Proc_21_40_45E99C(index, 0)`), answers
 * with one of three lines and nothing else:
 *
 *   the NPC is in the player's room (field 14 = the player-room global)
 *       "I don't understand what you want to do with <Name>."   [480603]
 *       -- and only this arm is not a turn (48061A stores 1 in the
 *          MemVar_494281 the tick is gated on); it is lib_cmd_verb_npc();
 *   else the NPC has been seen (field 26 = 1)
 *       "<Name> is not here!"                                   [480640]
 *   else
 *       "Who?"                                                  [480659]
 *
 * The last two fall straight through to 480660, so both are ordinary turns
 * and the walk+event tick runs after them.  4.0 puts the name of the middle
 * arm through the Runner's capitaliser (Proc_21_3_446BB4 at 480638); 3.9
 * pushes the raw field (45AC9F), the same split npc_announce() carries.
 *
 * 3.9 has the identical three-arm tail -- characters() @45ACD8, the branch
 * 45AC65..45ACC1, tests in the same order, "I don't understand what you want
 * to do with " at 45AC77, `" is not here!"` at 45ACA4 and "Who?" at 45ACBA --
 * so the floor is 3.90 and not lower: neither run370.exe nor run380.exe
 * contains the string "Who?" at all, and their `" is not here!"` lines
 * (run370 43865D and 438707, run380 4404D9 and 440596) sit inside the
 * per-verb attack and take handlers, not in a generic tail.
 *
 * The ceiling is 4.00, and that is measured, not assumed.  3.9 spells its
 * own reach test out inline at 45ABFB-45AC56 -- nothing printed yet, AND the
 * typed line whole-word contains the NPC's LCase Name (var_16C(0), tested at
 * 45AC24) OR its LCase Alias (var_16C(8), 45AC4D) -- while 4.0 replaced that
 * pair of c() calls with the opaque Proc_21_40_45E99C(index, 0) (4805E8),
 * whose body the decompile does not carry.  Whatever it tests, the 2026-09-08
 * capture shows it failing on exactly the lines 3.9 would answer: maincourse
 * (4.00) answers `attack cat` and `attack human` with the game's own
 * DontUnderstand text, and thepkgirl (4.00) answers `revive ethan`, `spray
 * chadwick` and `hug katryn` with "Pardon me?" -- never the tail.  So this
 * is ported for 3.90 and 3.9x only.
 *
 * thepkgirl's `attack chadwick` -> "The man is not here!" is a different
 * site: run400 keeps the pre-battle per-verb attack handler, and its own
 * absent-NPC else prints `" is not here!"` at 47F700, inside the verb branch
 * that ends at 47F70B where "take"/"get" begins.  Ported for the Battle
 * System off as lib_attack_absent_npc().
 *
 * Measured on the 2026-09-08 whole-corpus capture: alexis_worn_cube (3.90)
 * answers `attack narfild`, `attack goblin` and `attack monster` from rooms
 * those NPCs have never been seen in with "Who?" -- 94 turns of it, and
 * eight more on `alexis` -- where Scarier printed the DontUnderstand text.
 *
 * The Runner's tail consults no reference flags: it rescans the typed line
 * for every NPC in index order, so this does the same rather than reading
 * game->npc_references[], which the parser sets from prefixes and positions
 * the tail knows nothing about.  The in-room case returns FALSE and leaves
 * lib_cmd_verb_npc()'s own unambiguous-reference test to answer it, which is
 * where this differs from the Runner: the Runner has no such test and prints
 * the first named NPC's catch-all whether or not the line is ambiguous.
 */
static scr_bool
lib_npc_absent_or_unknown (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *input = run_get_dispatch_input ();
  scr_int index_;

  if (prop_get_taf_version (bundle) < TAF_VERSION_390
      || lib_is_version_400 (game) || !input)
    return FALSE;

  GS_FOR_EACH_NPC (game, index_)
    {
      const scr_char *name;

      if (!lib_npc_named_in_line (game, index_, input))
        continue;

      if (npc_in_room (game, index_, gs_playerroom (game)))
        return FALSE;

      if (!gs_npc_seen (game, index_))
        {
          pf_buffer_string (filter, "Who?\n");
          return TRUE;
        }

      name = prop_get_indexed_string (bundle, "NPCs", index_, "Name");
      if (!name || name[0] == NUL)
        return FALSE;
      pf_buffer_string (filter, name);
      pf_buffer_string (filter, " is not here!\n");
      return TRUE;
    }

  return FALSE;
}

scr_bool
lib_cmd_verb_npc (scr_gameref_t game)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int count, npc, index_;

  /*
   * 4.0: the ending takes the character catch-all off the line as well.
   * This message is the tail of run400's characters() (Proc_19_0_480674,
   * printed at 480603), and the instruction before it is
   * `loc_4805CD: If MemVar_4941AD > 0 Then Exit Sub` -- the same gameover
   * byte the object catch-all above is gated on, tested a second time
   * because characters() is called from the routine's tail (48B56E), below
   * the jump at 48AC62.
   *
   * Measured live on easter.taf (run400
   * runner_probes/easter.run400.txt:304-308, 2026-09-07): `show basket to
   * shopkeeper`, the winning move, prints the task's text and the WinText and
   * nothing in between -- no "I don't understand what you want to do with
   * shopkeeper.", and no DontUnderstand either, because the line names a
   * character (48B573's var_29C).
   */
  if (lib_is_version_400 (game) && game->pending_endgame != 0)
    return FALSE;

  /*
   * 3.7/3.8 have no character catch-all: run380 characters() has arms for
   * named verbs only, so `hug dave`, `eat dave` or a bare `dave` end on
   * DontUnderstand (run380x runner_probes/npcamb.run380.one.rtf, run370x
   * runner_probes/npcamb.run370.one.rtf).
   */
  if (prop_get_taf_version (gs_get_bundle (game)) < TAF_VERSION_390)
    return FALSE;

  /*
   * Ensure the reference is unambiguous.  3.9 fills an empty buffer, so of
   * two present characters the FIRST answers: `hug guard` with Ann and Bob
   * both guards is "... with Ann." (run390x runner_probes/npcamb.run390.txt).
   */
  count = 0;
  npc = -1;
  GS_FOR_EACH_NPC (game, index_)
    {
      if (game->npc_references[index_]
          && gs_npc_seen (game, index_)
          && npc_in_room (game, index_, gs_playerroom (game)))
        {
          count++;
          if (npc == -1 || lib_is_version_400 (game))
            npc = index_;
        }
    }
  if (count > 1 && !lib_is_version_400 (game))
    count = 1;
  /*
   * 4.0: the Prefix contest settles the crowd before the question is even
   * considered.  The tail calls npc_in_command(index, 0) per NPC (4805EB),
   * and mode 0 with two or more present, seen namesakes falls to 45E892:
   * `If Proc_21_49_450610(word) = index` -- the contest's winner returns
   * the containment TRUE and every rival returns FALSE, so the catch-all
   * names the winner.  Only a tie (450610 = &HFF) flags Me(424) and leaves
   * the "Which <term>." question to generaltasks.  p4PFX `blue guard`, a
   * line with no verb at all and three characters aliased "guard", is
   * "I don't understand what you want to do with Bob."
   * (runner_probes/pfx.run400.txt turn 5), where we printed the bare
   * DontUnderstand.
   */
  if (count > 1 && lib_is_version_400 (game) && run_get_dispatch_input ())
    {
      const scr_char *line = run_get_dispatch_input ();
      scr_int best, kept, winner;

      best = -1;
      kept = 0;
      winner = -1;
      GS_FOR_EACH_NPC (game, index_)
        {
          scr_int score;

          if (!game->npc_references[index_]
              || !gs_npc_seen (game, index_)
              || !npc_in_room (game, index_, gs_playerroom (game)))
            continue;
          score = lib_npc_400_prefix_score (game, index_, line);
          if (score > best)
            {
              best = score;
              kept = 1;
              winner = index_;
            }
          else if (score == best)
            kept++;
        }

      if (best > 0 && kept == 1)
        {
          count = 1;
          npc = winner;
        }
    }
  /*
   * 4.0: namesakes get generaltasks' "Which <term>." question, which is
   * asked of the line before any library branch (run400 48B6AE-48BB92).
   * `attack droid and guard` splits to a bare `guard` that lands here:
   * "Which Guard.  A guard or a guard?"
   * (runner_probes/battlemulti.run400.txt).  Not while the battle
   * library's "Who do you want to attack?  Guard or Droid?" is open: that
   * `guard` is its answer, and falls through to the catch-all so that
   * lib_battle_who_continuation() re-runs the line with it.
   */
  if (count > 1 && lib_is_version_400 (game)
      && !lib_battle_which_pending ()
      && lib_npc_400_raise_for_line (game))
    return TRUE;
  if (count != 1)
    return lib_npc_absent_or_unknown (game);

  /* Save in variables. */
  var_set_ref_character (vars, npc);

  /*
   * 4.0: like the object catch-all above, this answer is not a turn.  The
   * character pass stores 1 in MemVar_494281 -- the not-a-turn flag the
   * walk+event tick at 48B599 is gated on -- as the last thing it does with
   * this message (run400 loc_48061A-48061E), and neither of the two
   * neighbouring branches does: `" is not here!"` at 480640 and `"Who?"` at
   * 480659 both fall straight through to 480660.
   *
   * Measured on p4REPEAT3.taf (run400 runner_probes/repeat3.run400.txt,
   * 2026-09-08): every line of that probe is followed by the every-turn
   * event's "TICK." except the two `bob, hello` turns, which print "I don't
   * understand what you want to do with Bob." and the game's DontUnderstand
   * text and stop.
   */
  if (lib_is_version_400 (game))
    game->is_admin = TRUE;

  /* Under a character question this line is its answer; see the slot in
   * run_element_questions() and lib_co_400_npc_answer_line(). */
  if (lib_is_version_400 (game) && lib_co_400_question_pending ()
      && lib_co_400_pending_is_npc ())
    lib_co_400_note_refusal ();
  lib_battle_who_note_unanswered ();

  /* Print don't understand message; unlike objects, there's no "me" here. */
  lib_print_wrapped_npc (game, "I don't understand what you want to do with ",
                         npc, ".\n");
  return TRUE;
}


/*
 * lib_debug_trace()
 *
 * Set library tracing on/off.
 */
void
lib_debug_trace (scr_bool flag)
{
  lib_trace = flag;
}

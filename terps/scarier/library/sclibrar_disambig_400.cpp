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
 * The 4.0 "which do you mean" question machinery: the pending question
 * and its answer slot (lib_co_400_*), the prompts each scan raises, and
 * the character namesake questions (lib_npc_400_*).
 *
 * See sclibrar_disambig_internal.h for what the disambiguation files
 * share.
 */

#include "sclibrar.h"
#include "sclibrar_internal.h"
#include "sclibrar_disambig_internal.h"

/*
 * lib_co_400_*()
 *
 * The 4.0 object-ambiguity prompt, its pending question, and the answer
 * slot the question opens.  Measured 2026-09-07 with harness/make_400_coprobe.py
 * -> p4CO.taf, two rooms and eight static objects sharing one description so
 * that only the CHOICE shows: two trees whose Short is "tree", a rock (the
 * unique control), a mustang key and a truck key both aliased "keys", a hut
 * aliased "shed" beside an object whose Short IS "shed", and a third "tree"
 * in the far room as the presence control.  One task `poke %object%`, the
 * game's DontUnderstand "NO IDEA.", and a one-shot event printing "TICK." so
 * that a swallowed turn shows.  Transcripts runner_probes/co.run400.t9*.txt,
 * run400 under Wine (harness/make_400_coprobe.py).
 *
 * 4.0 does raise this prompt -- from the two handlers rather than from the
 * turn driver, under
 * two different tests (runner_probes/co.run400.t926.txt, every cell isolated
 * by a neutral `look`):
 *
 *     chop tree   ->  Which tree.  The red tree or the blue tree?
 *     x    tree   ->  Which tree.  The red tree or the blue tree?
 *     chop shed   ->  NO IDEA.
 *     x    shed   ->  Which shed.  The hut or the shed?
 *     chop keys   ->  NO IDEA.
 *     x    keys   ->  Which keys.  The mustang key or the truck key?
 *     chop rock   ->  I don't understand what you want me to do with the rock.
 *     x    rock   ->  A plain thing.
 *
 * So the library EXAMINE path prompts whenever two or more PRESENT objects
 * answer to the typed term by Short or by Alias ("shed" is one Short plus one
 * alias, "keys" two aliases; both prompt), while the unhandled-verb path
 * prompts only where the term is the SHORT of every tied candidate -- a
 * Short+alias or alias+alias tie is not ambiguous enough for it and the
 * game's DontUnderstand comes out instead.  Presence really is filtered: with
 * only the far room's tree present, `chop tree` gives the plain "I don't
 * understand what you want me to do with the tree."
 * (runner_probes/co.run400.t924.txt).
 *
 * The wording is the 3.7/3.8 one already ported above -- `Which <term>.
 * <NP> or <NP>?`, a full stop, two spaces, the noun phrases in index order
 * joined ", " / " or " -- which is why lca T91's two identically named trees
 * read "Which tree.  The tree or the tree?".
 *
 * What counts as a name, what gets listed, and where the term comes from
 * (runner_probes/co.run400.t928.txt, runner_probes/co.run400.t929.txt,
 * runner_probes/co.run400.t930.txt):
 *
 *     chop key        ->  NO IDEA.
 *     x    key        ->  You see no such thing.
 *     chop mustang    ->  NO IDEA.
 *     tree            ->  Which tree.  The red tree or the blue tree?
 *     rock            ->  I don't understand what you want me to do with the
 *                         rock.
 *     x    tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
 *     x    rock tree  ->  Which tree.  The red tree, the blue tree or the rock?
 *     chop tree rock  ->  Which tree.  The red tree, the blue tree or the rock?
 *
 * A name matches whole or not at all -- "key" is a word inside two Shorts and
 * names nothing -- so an ambiguity 4.0 prompts about is always a shared whole
 * name.  The list is every object the LINE referenced, in index order, and
 * not just the term's namesakes (the rock is listed under "Which tree"), and
 * the term comes from the lowest-indexed ambiguous object rather than from
 * the order the nouns were typed (`x rock tree` still says "Which tree").  No
 * verb is needed for either path: a bare `tree` prompts, a bare `rock` gets
 * the unhandled-verb catch-all naming it.
 *
 * Neither the prompt nor any of its answers is a turn.  The probe's ticker
 * has StarterType 1 and Time1 = Time2 = 1, so its "TICK." lands on the first
 * real turn of the session: runner_probes/co.run400.t925.txt prints it after
 * the opening `look`, runner_probes/co.run400.t926.txt after the `look` that
 * FOLLOWS `chop tree`, and runner_probes/co.run400.t927.txt after the `look`
 * that follows both `x keys` and its answer `mustang`.  Every prompt and
 * every answer is therefore administrative.
 *
 * The prompt leaves a question pending and the NEXT line is read against it
 * (runner_probes/co.run400.t925.txt and runner_probes/co.run400.t927.txt,
 * and lca):
 *
 *     x keys / mustang     ->  That is still ambiguous!
 *     chop tree / red      ->  I don't understand what you want me to do with
 *                              the red tree.
 *     x tree / zzz         ->  That is still ambiguous!
 *     chop tree / x tree   ->  That is still ambiguous!
 *     x shed / chop keys   ->  That is still ambiguous!
 *     x tree rock / rock   ->  That is still ambiguous!
 *     x tree rock / blue   ->  A plain thing.
 *     x tree rock / x rock ->  A plain thing.
 *     x keys / x rock      ->  A plain thing.
 *     chop tree / look     ->  (the room description)
 *     chop tree / n        ->  (the player moves;
 *                                runner_transcripts/lca.txt:738)
 *
 * The rule that fits all of them is not "the next line is an answer": it is
 * that the pending question changes only the places a line can end up with
 * nothing to say.
 *
 *   - A line that DID something runs normally and drops the question
 *     (`look`, `n`, `x rock`).
 *   - A line that raises a NEW ambiguity prints "That is still ambiguous!"
 *     instead of a second full prompt (`x tree`, and `chop keys`, whose
 *     alias tie the unhandled-verb path would otherwise pass over to
 *     DontUnderstand).
 *   - A line that did nothing goes to the answer slot, and its own output
 *     goes with it: that is either the game's DontUnderstand text or the
 *     unhandled-verb catch-all.  `rock` / `x rock` is the pair that settles
 *     the second half -- bare `rock` gets the catch-all in isolation
 *     (runner_probes/co.run400.t930.txt), so with a question open it is
 *     claimed and answers "That is still ambiguous!", while `x rock`
 *     examines the rock.
 *
 * The slot does not look at the candidates at all: it splices the typed
 * words into the stored command where the term stood and re-runs the whole
 * rebuilt line (48B097-48B15B, lib_co_400_object_answer_line()).  `chop
 * tree` / `red` runs `chop red tree`, whose catch-all names the red tree;
 * `x tree rock` / `blue` runs `x blue tree rock` and describes the blue
 * tree.  "That is still ambiguous!" is not an answer this slot gives, it is
 * the re-run tying AGAIN on what the last prompt offered: `x keys` /
 * `mustang` runs `x mustang keys`, where neither Short is whole and both
 * aliases still match, and `x tree rock` / `rock` runs `x rock tree rock`.
 * So naming a listed object outright does not pick it -- but a word that
 * narrows the rebuilt line does, wherever it lands in it.
 *
 * Either answer clears the question: runner_probes/co.run400.t925.txt's
 * `x keys` gets the full prompt again immediately after `chop keys` had
 * answered "That is still ambiguous!".
 *
 * The sibling string "That wasn't one of the options!" belongs to the OTHER
 * half of the state.  generaltasks keeps two things, not one: the question
 * itself (MemVar_494234, "term|command") and the LIST the last prompt
 * offered (MemVar_4941F4).  They are spent differently.
 *
 *   - The question is taken at the top of a typed LINE and spent by its
 *     first element: 489FD4 copies it into var_98 -- and 489FEB, where the
 *     queue drain and the answer re-runs jump back in, skips that copy -- and
 *     48B5FC clears it again when it is still the same string.  A question
 *     raised by an EARLIER ELEMENT of the line being run is therefore never
 *     spent, and 48B6AE finds it still standing: the candidate list is not
 *     consulted at all, the answer is "That wasn't one of the options!", and
 *     the question is dropped (48BB5D/48BB8E).
 *   - The offered list outlives that.  co() accumulates the candidates'
 *     names into MemVar_4941F0 as it walks them (the Which arm 464560) and
 *     48BB53 copies that string into 4941F4 for every full prompt; 48B6F6
 *     (and 48B80F for a character) compares the two and prints "That is
 *     still ambiguous!" only when they are equal, clearing both.  An element
 *     that flagged no ambiguity at all forgets it (48B61F), and 48BB92
 *     clears the flagged list at the end of every element.
 *
 * Measured on p4CO with run400 (runner_probes/co.run400.co7.txt,
 * runner_probes/co.run400.co8.txt, runner_probes/co.run400.co9.txt,
 * runner_probes/co.run400.co10.txt, runner_probes/co.run400.co11.txt,
 * 2026-09-20), all administrative -- `turns` never moves:
 *
 *     x tree and x tree        ->  prompt / wasn't one of the options
 *     x tree then x tree       ->  the same, and so is `x tree. x tree`
 *     x keys and x tree        ->  prompt / wasn't one of the options
 *     x tree and x tree and x tree
 *                              ->  prompt / wasn't / still ambiguous
 *     x tree and x tree and x keys
 *                              ->  prompt / wasn't / Which keys...?
 *     x keys and x tree and x tree
 *                              ->  Which keys...? / wasn't / Which tree...?
 *     x tree and x rock and x tree
 *                              ->  prompt / A plain thing. / wasn't
 *     x tree / x tree / x tree ->  prompt / still ambiguous / prompt
 *     x tree / x keys          ->  prompt / Which keys...?
 *     x rock and x tree / x keys
 *                              ->  prompt / Which keys...?
 *
 * The last two are what proves the "still ambiguous" arm is a comparison and
 * not a flag: a DIFFERENT ambiguity right after a pending question gets a
 * full prompt.  `x tree and x rock and x tree` is what proves the question
 * survives an element that did something, and `x tree and x tree and x tree`
 * that "wasn't one of the options" leaves the offered list behind for the
 * element after it.  That it is a list and not the term is
 * runner_probes/co.run400.co14.txt: `x tree` / `x tree rock` -- one term,
 * two objects then three -- is prompt / prompt, where `x tree` twice is
 * prompt / still ambiguous.
 *
 * An element that says NOTHING never reaches any of this: the answer slot
 * claims it first, and takes the rest of the typed line with it.  That is
 * the whole difference between `chop tree and chop tree` (prompt, then the
 * slot's own "That is still ambiguous!") and `x tree and x tree`, and
 * `chop tree and chop tree and chop tree` prints nothing for its third
 * element at all -- see lib_co_400_raise_for_short_tie().
 */

/*
 * The state all of that lives in is disambig.question, disambig.element
 * and disambig.offered (sclibrar_internal.h):
 *
 *   question.spend           var_98: the question this typed line began
 *                            with, still to be spent.
 *   element.named_raised     a handler's own prompt (name_object's, not the
 *                            generaltasks scan's) was raised by this line
 *                            element; see lib_openclose_with_half_400().
 *   element.therest_split    therest's with-split scored this element and
 *                            so owns Me(424); see
 *                            lib_openclose_with_half_raise_400().
 *   offered.list, .seen, .flagged
 *                            MemVar_4941F4, and whether this element
 *                            flagged an ambiguity at all.
 *
 * 4941F4 holds the LIST the last prompt offered, not its term: co()
 * accumulates the candidates' names into MemVar_4941F0 as it walks them
 * (the Which arm at 464560), 48BB53 copies that into 4941F4, and 48B6FF
 * compares the two strings.  `x tree` then `x tree rock` is the cell --
 * same term, two objects then three, and run400 asks the whole question
 * again (runner_probes/co.run400.co14.txt, 2026-09-20); the term model
 * said "That is still ambiguous!".  We compare the candidate objects
 * rather than the rendered string, which differs only where two different
 * sets render alike.
 */

void
lib_co_400_reset (void)
{
  /*
   * The "Which" form is cached per bundle; a game reloaded in the same
   * process can land its bundle at the old address, so forget it here.
   */
  disambig.which = {};
  disambig.question = {};
  disambig.element = {};
  disambig.offered = {};
}

/*
 * The question as it stands between two lines, for a Spatterlight autosave;
 * see run_session_state().  The rest lives only while a line is dispatched.
 *
 * Two things cross the line boundary, matching generaltasks' two memory
 * cells: the question itself (494234), and the list the last prompt OFFERED
 * (4941F4), which is what "That is still ambiguous!" compares the next
 * line's tie against.  The offered list is reported only when the next
 * lib_co_400_begin_line() will keep it -- the last element flagged an
 * ambiguity -- since otherwise that call drops it before anything reads it.
 * Without it a relaunch answered `chop tree` / `zzz` with the whole prompt
 * again where a live session says "That is still ambiguous!".
 */
void
lib_co_400_get_question (scr_bool *pending, std::string *term,
                         std::string *command,
                         std::vector<scr_int> *candidates,
                         scr_bool *offered,
                         std::vector<scr_int> *offered_list)
{
  *pending = disambig.question.pending;
  *term = disambig.question.term;
  *command = disambig.question.command;
  *candidates = disambig.question.candidates;
  *offered = disambig.offered.flagged && disambig.offered.seen;
  if (*offered)
    *offered_list = disambig.offered.list;
  else
    offered_list->clear ();
}

void
lib_co_400_set_question (scr_bool pending, const std::string &term,
                         const std::string &command,
                         const std::vector<scr_int> &candidates,
                         scr_bool offered,
                         const std::vector<scr_int> &offered_list)
{
  lib_co_400_reset ();
  disambig.question.pending = pending;
  disambig.question.term = term;
  disambig.question.command = command;
  disambig.question.candidates = candidates;
  if (offered)
    {
      /* As the prompt left them (48BB53), with the flag that carries the
         list past the next begin_line. */
      disambig.offered.list = offered_list;
      disambig.offered.seen = TRUE;
      disambig.offered.flagged = TRUE;
    }
}

/*
 * Called once per typed line element, before it is dispatched.  IS_NEW_LINE
 * marks the first element of a typed line, the only one that can spend the
 * question that was standing when the line was read (489FD4 / 48B5FC); a
 * question raised by an earlier element of the SAME line survives into this
 * one and becomes "That wasn't one of the options!".
 *
 * The prompted term (48BB53) is forgotten by the first element that flags no
 * ambiguity at all (48B61F), which is why `x tree` / `look` / `x tree` gets
 * two full prompts.
 */
void
lib_co_400_begin_line (scr_bool is_new_line)
{
  if (!disambig.offered.flagged)
    {
      disambig.offered.list.clear ();
      disambig.offered.seen = FALSE;
    }
  disambig.offered.flagged = FALSE;
  disambig.element.named_raised = FALSE;
  disambig.element.therest_split = FALSE;

  if (is_new_line)
    disambig.question.spend = disambig.question.pending;

  /* The answer slot reads the question as it stood BEFORE the spend. */
  disambig.question.was_pending = disambig.question.pending;
  if (disambig.question.spend)
    {
      disambig.question.pending = FALSE;
      disambig.question.spend = FALSE;
    }
  disambig.question.refused = FALSE;
}

/*
 * lib_antecedent_begin_line_400()
 *
 * generaltasks' first write of the line (48A3F5-48A42E): the whole line
 * scored by 463640 in mode 0, and a lone winner named in its definite form
 * -- `zzz red stone` "(the red stone)", `cut rope with blue stone ruby`
 * "(the blue stone)" (runner_probes/wtie.run400.it1.txt,
 * runner_probes/wtie3.run400.it2.txt, 2026-09-21).  This is also the "I
 * don't understand what you want me to do with" reply's antecedent.
 */

void
lib_antecedent_begin_line_400 (scr_gameref_t game, const scr_char *line)
{
  std::vector<scr_int> marked;
  scr_int object, pending, last_tied, mark_count;

  uip_begin_antecedent_400 ();
  if (!lib_is_version_400 (game) || !line || uip_pronoun_was_used ())
    return;

  object = lib_name_object_resolve_400 (game, line, 0, &pending, &last_tied,
                                        &marked, &mark_count);
  if (object >= 0)
    uip_note_antecedent_400 (object, UIP_IT_DEFINITE, UIP_STAGE_SCORER);
}

scr_bool
lib_co_400_question_pending (void)
{
  return disambig.question.was_pending;
}

/*
 * Either answer takes the question with it before the rebuilt line is
 * re-run (48B152 and 48B193, both just before the jump back to 489FEB).
 * The term, the command and the candidates stay where they are: the slot is
 * still reading them, and the next prompt writes them all again.
 *
 * 489FEB is the top of the element loop, so the re-run starts with a clean
 * reply as well: it must be able to raise its own question rather than be
 * claimed by the one it is answering.  The prompted term of 48BB53 is NOT
 * cleared here -- it is what turns the re-run's raise into "That is still
 * ambiguous!".
 */
void
lib_co_400_take_question (void)
{
  disambig.question.pending = FALSE;
  disambig.question.was_pending = FALSE;
  disambig.question.refused = FALSE;
}

/*
 * Did a handler's own "Which" prompt go up for this line element?  The
 * generaltasks scan's prompt (48B6AE) is raised after openclose has run and
 * is not this; see lib_openclose_with_half_400().
 */
scr_bool
lib_co_400_named_question_raised (void)
{
  return disambig.element.named_raised;
}

/*
 * The prompt was printed but generaltasks never registered the question:
 * 48B60C saw Me(424) = -1 and printed the buffer as it stood.  The text
 * stays; the question, its term, its command and its candidates go, and
 * the next line is no answer to anything.
 */
void
lib_co_400_drop_question (void)
{
  disambig.question.pending = FALSE;
  disambig.question.refused = FALSE;
  disambig.question.term.clear ();
  disambig.question.command.clear ();
  disambig.question.candidates.clear ();
}

/*
 * The unhandled-verb catch-all leaves the turn as empty-handed as the
 * DontUnderstand path does, so a pending question claims that line too; see
 * the answer slot in run_element_questions().
 */
void
lib_co_400_note_refusal (void)
{
  disambig.question.refused = TRUE;
}

scr_bool
lib_co_400_line_refused (void)
{
  return disambig.question.refused;
}

void
lib_co_400_print_still_ambiguous (scr_gameref_t game)
{
  pf_buffer_string (gs_get_filter (game), "That is still ambiguous!\n");
  game->is_admin = TRUE;
}

/*
 * The character question (lib_npc_400_raise_for_line()) records no
 * candidates, and its answer is not scored: run400 re-runs the original line
 * with the typed words in front of the term.  Measured on p4BATTLEMULTI
 * (runner_probes/battlemulti.run400.whichf.txt, 2026-09-13): a fresh `attack
 * guard and droid` prints "Which Guard.  A guard or a guard?" for its first
 * half, and its second half `droid`, which alone gets only the catch-all,
 * answers it -- "That is still ambiguous!" and twelve more draws, the three
 * blows of `attack droid guard`.
 */
scr_bool
lib_co_400_pending_is_npc (void)
{
  return disambig.question.candidates.empty ()
         && !disambig.question.term.empty ();
}

std::string
lib_co_400_npc_answer_line (const scr_char *line)
{
  const std::string &command = disambig.question.command;
  const std::string &term = disambig.question.term;
  std::string::size_type at;

  for (at = 0; at + term.size () <= command.size (); at++)
    {
      if (scr_strncasecmp (command.c_str () + at, term.c_str (),
                           term.size ()) == 0
          && (at == 0 || command[at - 1] == ' ')
          && (at + term.size () == command.size ()
              || command[at + term.size ()] == ' '))
        return command.substr (0, at) + line + " " + command.substr (at);
    }
  return command + " " + line;
}

/* How many of the candidates answer to exactly this name. */
static scr_int
lib_co_400_namesake_count (scr_gameref_t game,
                           const std::vector<scr_int> &objects,
                           const scr_char *term)
{
  scr_int index_, count;

  count = 0;
  for (index_ = 0; index_ < (scr_int) objects.size (); index_++)
    {
      if (lib_co_object_answers_to (game, objects[index_], term))
        count++;
    }
  return count;
}

/*
 * Raise the question.  With one already open the generaltasks scan does not
 * print a second prompt, only the short refusal; either way the line is
 * administrative and the question that was open is now spent.
 *
 * FROM_SCAN says the question is that scan's.  name_object's own prompt --
 * the one a crowded take, drop or put reaches through 463640 -- is raised by
 * a handler that RAN, and it prints in full whatever was open before: run400
 * answers p4TAKER's `drop cog` with "Which cog.  The cog or the cog?" and
 * the `drop pad` right after it with "Which pad.  The pad or the pad?"
 * (runner_probes/taker.run400.txt, 2026-09-20).
 */
static void
lib_co_400_raise_common (scr_gameref_t game, const scr_char *term,
                         const std::vector<scr_int> &objects,
                         scr_bool from_scan)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_char *command;
  scr_int index_;

  game->is_admin = TRUE;

  if (from_scan)
    {
      /* 48B6AE, on a question an earlier element of this line raised. */
      if (disambig.question.pending)
        {
          pf_buffer_string (filter, "That wasn't one of the options!\n");
          disambig.question.pending = FALSE;
          disambig.question.term.clear ();
          disambig.question.command.clear ();
          disambig.question.candidates.clear ();
          disambig.offered.flagged = TRUE;
          return;
        }

      /* 48B6F6/48B80F: the same list offered twice running. */
      disambig.offered.flagged = TRUE;
      if (disambig.offered.seen && disambig.offered.list == objects)
        {
          pf_buffer_string (filter, "That is still ambiguous!\n");
          disambig.offered.list.clear ();
          disambig.offered.seen = FALSE;
          return;
        }
    }

  lib_which_head (game, term, nullptr);

  pf_new_sentence (filter);
  for (index_ = 0; index_ < (scr_int) objects.size (); index_++)
    {
      if (index_ > 0)
        pf_buffer_string (filter,
                          index_ == (scr_int) objects.size () - 1
                          ? " or " : ", ");
      lib_print_object_np (game, objects[index_]);
    }
  pf_buffer_string (filter, "?");
  pf_buffer_answer_break (filter);

  command = run_get_dispatch_input ();
  disambig.question.pending = TRUE;
  disambig.question.term = term;
  disambig.question.command = command ? command : "";
  disambig.question.candidates = objects;
  if (from_scan)
    {
      disambig.offered.list = objects;         /* 48BB53 */
      disambig.offered.seen = TRUE;
    }
  else
    disambig.element.named_raised = TRUE;
}

void
lib_co_400_raise (scr_gameref_t game, const scr_char *term,
                  const std::vector<scr_int> &objects)
{
  lib_co_400_raise_common (game, term, objects, TRUE);
}

/* name_object's prompt; see lib_co_400_raise_common(). */
void
lib_co_400_raise_named (scr_gameref_t game, const scr_char *term,
                        const std::vector<scr_int> &objects)
{
  lib_co_400_raise_common (game, term, objects, FALSE);
}

/*
 * lib_with_split_crowd_400()
 *
 * The question a line holding " with " raises comes out of ONE half, not out
 * of the whole line.  therest splits before any verb test (4883C5) and
 * scores each half with 463640, so the marked candidates the prompt reads
 * back are the ones the last half scored.  p4WTIE (run400,
 * runner_probes/wtie.run400.1.txt and runner_probes/wtie.run400.w[6-9].txt,
 * 2026-09-20), with "stone" the Short of two objects and a knife, a box and
 * a rope beside them:
 *
 *   head ties       `cut stone with knife`, `cut stone with zzz`,
 *                   `chop stone with knife` -- "Which stone.  The red stone
 *                   or the blue stone?", the knife left out of the list
 *                   although the line names it.  therest leaves at 488430
 *                   and the catch-all asks.
 *   head resolves   `cut knife with stone`, `cut rope with stone`, `open box
 *                   with stone`, `close box with stone`, `x box with stone`,
 *                   `x knife with stone`, `x rope with stone` -- the tail is
 *                   scored next (4884DB) and ITS tie is the question, again
 *                   the two stones alone.
 *   head names
 *   nothing         `chop zzz with stone` is the game's DontUnderstand text:
 *                   therest left before the tail was ever scored, and an
 *                   empty candidate list raises nothing.
 *
 * Examine is the exception.  It sits above therest and answers first: where
 * the head ties, with the whole line's reference set -- `x stone with knife`
 * is "Which stone.  The knife, the red stone or the blue stone?" and `x
 * stone with box` names the box the same way.  Where the head does not tie
 * it describes the head's object and asks nothing of its own; `x knife with
 * stone` and `x box with stone` list the stones alone because openclose's
 * with-half asked first and examines never undid it (see
 * lib_openclose_with_half_raise_400()).  The unhandled verb's head naming
 * nothing asks nothing either (therest's restart leaves Me(424) at -1).
 *
 * Fills *crowd with the objects to ask about (empty = ask nothing), and
 * *pending with the tail tie's pending object when the tail asks, *head_object
 * with the head's object when it named one; returns TRUE when the split
 * decides, FALSE leaves the caller its own whole-line list.
 */
static scr_int
lib_with_half_tied_400 (scr_gameref_t game, const scr_char *half,
                        std::vector<scr_int> *tied)
{
  scr_int object;

  object = lib_verb_object_resolve_400_string (game, half, tied, TRUE);
  if (object < 0)
    object = lib_verb_object_resolve_400_string (game, half, tied, FALSE);

  /* Only a tie leaves a crowd behind; a winner marked just itself. */
  if (object != -1)
    tied->clear ();
  return object;
}

scr_bool
lib_with_split_crowd_400 (scr_gameref_t game, scr_bool examine,
                          std::vector<scr_int> *crowd, scr_int *pending,
                          scr_int *head_object)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *tail = input ? strstr (input, " with ") : nullptr;
  std::vector<scr_int> marked;
  std::string head;
  scr_int object, tied_pending, last_tied, mark_count;

  crowd->clear ();
  *pending = -1;
  *head_object = -1;
  if (!lib_is_version_400 (game) || !tail)
    return FALSE;

  head.assign (input, tail - input);
  object = lib_with_half_tied_400 (game, head.c_str (), crowd);
  if (object == -1)
    return !examine;

  /*
   * Past a head that does not tie, examine asks nothing of its own: the
   * question is openclose's, which ran before it and which examines never
   * undoes (lib_openclose_with_half_raise_400()).  `x ruby with stone`
   * describes, where `x rope with stone` asks.
   */
  crowd->clear ();
  *head_object = object;
  if (examine || object == -2)
    return TRUE;

  /*
   * The tail is 463640's, pending object and all: the question's term is
   * that object's Short, replaced by the last of its own aliases the whole
   * line holds -- `cut pebble with stone` is "Which pebble.", `cut flint
   * with stone` "Which stone." (the blue stone parks, "flint" is the red
   * one's), and `cut rope with stone flint pebble` ties 2-2 and asks
   * "Which pebble." too (runner_probes/wtie2.run400.19.txt, 2026-09-21).
   * It asks whether or not the whole line named one object.
   */
  if (lib_name_object_resolve_400 (game, tail + 6, 0, &tied_pending,
                                   &last_tied, &marked, &mark_count) == -1
      && tied_pending >= 0 && (scr_int) marked.size () == mark_count
      && marked.size () >= 2)
    {
      *crowd = marked;
      *pending = tied_pending;
    }
  return TRUE;
}

/* The with-split's tail question; see lib_with_split_crowd_400(). */
scr_bool
lib_co_400_raise_for_with_tail (scr_gameref_t game, scr_int pending,
                                const std::vector<scr_int> &crowd)
{
  const scr_char *input = run_get_dispatch_input ();

  if (!input || pending < 0 || crowd.size () < 2)
    return FALSE;

  /* An open question takes the element first; see
     lib_co_400_raise_for_short_tie(). */
  if (lib_co_400_question_pending ())
    {
      lib_co_400_note_refusal ();
      return FALSE;
    }

  lib_co_400_raise (game, lib_drop_named_term_400 (game, pending, input, TRUE),
                    crowd);
  return TRUE;
}

/*
 * The examine path's test, read off run400's walk rather than off the
 * reference set.  examines() hands a line 463640 tied (MemVar_4942F8 < -1)
 * to referencedob (457034), and it is co() -- not a scan of its own -- that
 * leaves the question behind:
 *
 *   463640   Me(424) = the tie arm's pending object (4633F0, the index+2
 *            quirk; see lib_name_object_resolve_400()) and Me(428) = the
 *            pass-0 marks, "the X, the Y or the Z?" (46348B).
 *   pass A   co(i, 3) marks every object whose word has a present, seen
 *            namesake; fewer than two marked ends the walk.
 *   pass B   co(i, 0) over the marked, in index order.  A word with one
 *            namesake resets Me(424) = -1 (46485E).  A crowded word runs
 *            the Prefix contest 454454: a unique winner that is i sets
 *            Me(424) = -2; otherwise Me(428) is REBUILT from the word's
 *            namesakes only when it is empty or does not hold the word as a
 *            binary substring (46462A-464733), and a contest with no winner
 *            parks Me(424) = i when Me(424) < 0 or i is present (464767).
 *
 * generaltasks then asks whenever Me(424) is an object (48B6B1), whatever
 * examines found: the term is Short(Me(424)) replaced by the last of its
 * aliases the line holds, the list is Me(428).  Measured on p4CO with
 * run400 (runner_probes/co.run400.co15.txt, 2026-09-21) -- red tree 0,
 * blue tree 1, rock 2, mustang key 3 and truck key 4 (both aliased
 * "keys"), hut 5 (aliased "shed"), shed 6, and a tree 7 in the other
 * room:
 *
 *   x keys shed, x shed keys   Which shed.  The hut or the shed?
 *                              (the keys rebuild, then "shed" rebuilds)
 *   x tree keys, x keys tree,  Which keys.  The red tree or the blue tree?
 *   x rock tree keys           (the absent tree 7 rebuilds last and parks
 *                              nothing; the truck key parked last)
 *   x rock keys                Which keys.  The mustang key or the truck
 *                              key?  (the rock resets, the keys park)
 *   x tree rock                Which tree.  The red tree, the blue tree or
 *                              the rock?  (463640's list stands; the rock
 *                              resets and the absent tree 7 parks)
 *   x shed tree                Which shed.  The red tree, the blue tree,
 *                              the hut or the shed?
 *   x tree hut                 Which tree.  The red tree, the blue tree or
 *                              the hut?
 *
 * And runner_probes/co.run400.t925.txt's `chop keys` answering `x shed`:
 * the rebuilt `x chop keys shed` ends on "The hut or the shed?" again, the
 * list 48B6FF compares, so "That is still ambiguous!".
 */

/* Me(428) as co() builds it, for its substring test. */
static std::string
lib_co_400_list_string (scr_gameref_t game, const std::vector<scr_int> &list)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::string text;
  scr_int index_;

  for (index_ = 0; index_ < (scr_int) list.size (); index_++)
    {
      if (index_ > 0)
        text += index_ == (scr_int) list.size () - 1 ? " or " : ", ";
      text += "the ";
      text += prop_get_indexed_string (bundle, "Objects", list[index_],
                                       "Short");
    }
  return text + "?";
}

/* How many words of the object's Prefix ("a" when empty) the line holds. */
static scr_int
lib_co_400_prefix_hits (scr_gameref_t game, scr_int object,
                        const scr_char *input)
{
  const scr_char *prefix;
  std::string copy;
  std::string::size_type at, next;
  scr_int found;

  prefix = prop_get_indexed_string (gs_get_bundle (game), "Objects", object,
                                    "Prefix");
  copy = scr_strempty (prefix) ? "a" : prefix;

  found = 0;
  for (at = 0; at <= copy.size (); at = next + 1)
    {
      std::string word;

      next = copy.find (' ', at);
      if (next == std::string::npos)
        next = copy.size ();
      word = copy.substr (at, next - at);
      if (!word.empty () && lib_input_contains_word_400 (input, word.c_str ()))
        found++;
    }
  return found;
}

/*
 * 454454(word, room): over the objects in the room, every name field equal
 * to the word scores its object's Prefix words in the line; the strict
 * maximum above zero wins, anything else is -1.
 */
scr_int
lib_co_400_prefix_contest (scr_gameref_t game, const scr_char *word,
                           const scr_char *input, scr_int room)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int object, result, best;

  result = -1;
  best = 0;
  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *name;
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      if (!obj_directly_in_room (game, object, room))
        continue;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = -1; alias < alias_count; alias++)
        {
          scr_int hits;

          if (alias < 0)
            name = prop_get_indexed_string (bundle, "Objects", object,
                                            "Short");
          else
            {
              vt_key[3].integer = alias;
              name = prop_get_string (bundle, "S<-sisi", vt_key);
            }
          if (scr_strempty (name) || scr_strcasecmp (name, word) != 0)
            continue;

          hits = lib_co_400_prefix_hits (game, object, input);
          if (hits == best)
            result = -1;
          else if (hits > best)
            {
              result = object;
              best = hits;
            }
        }
    }
  return result;
}

/*
 * One co(object, 0) call, as pass B below and openclose's with-half loop
 * make it: *ME is Me(424), *LIST Me(428), *LIST_OK whether that list is one
 * we can render (see lib_co_400_raise_for_references()).
 */
void
lib_co_400_walk_step (scr_gameref_t game, scr_int object,
                      const scr_char *input, scr_int *me,
                      std::vector<scr_int> *list, scr_bool *list_ok)
{
  const scr_int room = gs_playerroom (game);
  const scr_char *word;
  scr_int count;

  word = lib_co_400_name_word (game, object, input);
  if (!word)
    return;
  count = lib_co_400_present_namesakes (game, word);
  if (count == 1)
    {
      *me = -1;
      return;
    }
  if (count < 2)
    return;

  /*
   * Both arms hand the antecedent setter a name as they go (46460F, 464788;
   * see uip_note_antecedent_400()): the -2 arm Prefix & " " & Short, the
   * park arm the bare Short.
   */
  if (lib_co_400_prefix_contest (game, word, input, room) == object)
    {
      *me = -2;
      uip_note_antecedent_400 (object, UIP_IT_INDEFINITE, UIP_STAGE_CO);
      return;
    }

  if (list->empty ()
      || !strstr (lib_co_400_list_string (game, *list).c_str (), word))
    {
      scr_int other;

      list->clear ();
      GS_FOR_EACH_OBJECT (game, other)
        {
          if (gs_object_seen (game, other)
              && obj_indirectly_in_room (game, other, room)
              && lib_co_object_answers_to (game, other, word))
            list->push_back (other);
        }
      *list_ok = TRUE;
    }

  if (lib_co_400_prefix_contest (game, word, input, room) == -1
      && (*me < 0 || obj_indirectly_in_room (game, object, room)))
    {
      *me = object;
      uip_note_antecedent_400 (object, UIP_IT_BARE, UIP_STAGE_CO);
    }
}

scr_bool
lib_co_400_raise_for_references (scr_gameref_t game)
{
  const scr_char *input = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  std::vector<scr_int> marked, list, walk;
  scr_int object, me, last_tied, mark_count, index_;
  scr_bool list_ok;

  if (!input)
    return FALSE;

  object = lib_name_object_resolve_400 (game, input, 0, &me, &last_tied,
                                        &marked, &mark_count);
  if (object != -1)
    return FALSE;
  list = marked;
  list_ok = (scr_int) marked.size () == mark_count;

  /* Pass A. */
  GS_FOR_EACH_OBJECT (game, object)
    {
      const scr_char *word = lib_co_400_name_word (game, object, input);

      if (word && lib_co_400_present_namesakes (game, word) > 0)
        walk.push_back (object);
    }

  /* Pass B. */
  if (walk.size () >= 2)
    {
      for (index_ = 0; index_ < (scr_int) walk.size (); index_++)
        lib_co_400_walk_step (game, walk[index_], input, &me, &list,
                              &list_ok);
    }

  if (me < 0 || !list_ok || list.size () < 2)
    return FALSE;

  /*
   * examines has already described referencedob's pick, and named it to
   * the antecedent setter (471749-471789), before generaltasks replaces the
   * description with the question: `x stone` asks, and `x it` then echoes
   * "(a blue stone)", pass A's last mark
   * (runner_probes/wtie.run400.it1.txt, 2026-09-21).
   */
  object = lib_examine_referencedob_ex_400 (game, input, TRUE);
  if (object >= 0 && obj_indirectly_in_room (game, object, room))
    uip_note_antecedent_400 (object, UIP_IT_INDEFINITE, UIP_STAGE_HANDLER);

  lib_co_400_raise (game, lib_drop_named_term_400 (game, me, input, TRUE),
                    list);
  return TRUE;
}

/*
 * The examine path's test for a line SCARE's parser bound to one object: the
 * Runner's pass 1 settles on any present object whose Short the line holds;
 * failing that, every present object with a Short or alias in the line is a
 * candidate, and the first contained name (candidates in index order, Short
 * then aliases) that two or more of them answer to is the question's term.
 */
scr_bool
lib_co_400_raise_for_contained_aliases (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_int room = gs_playerroom (game);
  std::vector<scr_int> candidates;
  scr_int object, index_;

  if (!input)
    return FALSE;

  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;
      scr_bool hit;

      if (!lib_co_candidate (game, object, room))
        continue;
      if (lib_co_contains (input, prop_get_indexed_string (bundle, "Objects",
                                                           object, "Short")))
        return FALSE;

      /* Deliberate deviation: an alias the line holds only inside another
         object's longer name does not make a candidate; see
         lib_co_term_shadowed(). */
      hit = FALSE;
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count && !hit; alias++)
        {
          const scr_char *name;

          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          hit = lib_co_contains (input, name)
                && !lib_co_term_shadowed (game, input, object, name);
        }
      if (hit)
        candidates.push_back (object);
    }
  if (candidates.size () < 2)
    return FALSE;

  for (index_ = 0; index_ < (scr_int) candidates.size (); index_++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      object = candidates[index_];
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", object);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *name;

          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (scr_strempty (name) || !lib_co_contains (input, name)
              || lib_co_400_namesake_count (game, candidates, name) < 2)
            continue;

          lib_co_400_raise (game, name, candidates);
          return TRUE;
        }
    }
  return FALSE;
}

/*
 * The unhandled-verb path's test.  The candidates are the objects the 4.0
 * noun score tied on, and the term is the Short of the lowest-indexed
 * candidate that the line contains and that two or more of them share --
 * aliases do not count here, which is why run400 answers `chop shed` (the
 * hut answers to "shed" only by alias) with the game's DontUnderstand while
 * `x shed` raises the question.  A candidate that shares no name still gets
 * listed: `chop tree rock` prompts with all three
 * (runner_probes/co.run400.t929.txt).
 */
/*
 * The term the scan prompts with.  The Short tie above is what RAISES the
 * question, but the word it asks about is not always that Short: an ALIAS
 * two or more of the candidates share, typed as a whole word of the line,
 * takes its place.  p4CO, run400, 2026-09-20
 * (runner_probes/co.run400.co12.txt, runner_probes/co.run400.co13.txt):
 *
 *     chop keys tree        Which keys.  ... the mustang key or the truck key?
 *     chop tree keys        Which keys.  (same list; the typed order is
 *                           not what picks the term)
 *     chop shed keys tree   Which keys.  ... the hut or the shed?
 *     chop shed tree        Which tree.  The red tree, the blue tree, the
 *                           hut or the shed?
 *     chop tree hut         Which tree.  The red tree, the blue tree or
 *                           the hut?
 *     chop keys             NO IDEA.     (an alias tie alone raises nothing)
 *
 * "keys" is an Alias of BOTH keys; "shed" is the hut's Alias and the shed's
 * Short, and it never becomes the term.  So the replacement wants two
 * candidates answering by the same field, which is the shape of the count
 * above -- and "shed" alone, like "keys" alone, raises nothing at all.
 *
 * In generaltasks this is not a second pass but one object: 48B73C-48B78C
 * reads the record of MemVar_4941EC and takes its Short, replaced by each
 * of ITS aliases that is a whole word of the line, so the object 4941EC
 * ended on decides the word.  co() parks it as it walks (index/find.py -v
 * run400 46486C: the Which arm at 464560 stores arg_C when the prefix
 * contest 454454 returns -1), and the last park wins -- which is the keys,
 * index 3 and 4, over the trees at 0 and 1.  Why the hut and the shed,
 * walked last of all, park nothing is 463640's index+2 quirk, and the
 * unhandled-verb line now reads the pending object itself (see
 * lib_co_400_raise_for_pending_tie()) and the examine line co()'s walk
 * (lib_co_400_raise_for_references()); only the " with " split's HEAD tie
 * still takes this alias test (its tail reads the pending object too, see
 * lib_with_split_crowd_400()).
 */
static const scr_char *
lib_co_400_scan_term_400 (scr_gameref_t game, const std::vector<scr_int> &tied,
                          const scr_char *term)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *replacement = term;
  scr_int index_;

  for (index_ = 0; index_ < (scr_int) tied.size (); index_++)
    {
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      alias_count = lib_alias_prepare (bundle, vt_key, "Objects",
                                       tied[index_]);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *name;
          scr_int other, count;

          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (scr_strempty (name) || !lib_input_contains_word (input, name))
            continue;

          count = 0;
          for (other = 0; other < (scr_int) tied.size (); other++)
            {
              scr_vartype_t vt_other[4];
              scr_int other_count, index2;

              other_count = lib_alias_prepare (bundle, vt_other, "Objects",
                                               tied[other]);
              for (index2 = 0; index2 < other_count; index2++)
                {
                  const scr_char *other_name;

                  vt_other[3].integer = index2;
                  other_name = prop_get_string (bundle, "S<-sisi", vt_other);
                  if (!scr_strempty (other_name)
                      && scr_strcasecmp (other_name, name) == 0)
                    {
                      count++;
                      break;
                    }
                }
            }
          if (count > 1)
            replacement = name;
        }
    }

  return replacement;
}

scr_bool
lib_co_400_raise_for_short_tie (scr_gameref_t game,
                                const std::vector<scr_int> &tied)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *input = run_get_dispatch_input ();
  const scr_char *term;
  scr_int index_, count;

  if (!input || tied.size () < 2)
    return FALSE;

  /*
   * With a question already open this element never reaches the prompt at
   * all.  generaltasks tries the ANSWER first (48AFF3), and its gate is
   * "this element has said nothing yet" -- MemVar_4941B0, cleared at the top
   * of every element (489FEE) and still empty here because the ambiguity
   * flag holds the unhandled-verb catch-all back.  So `chop tree` twice over
   * is the answer slot's "That is still ambiguous!" and not the scan's
   * "That wasn't one of the options!", which only an element that DID answer
   * -- an examine, whose reply is in the buffer -- can reach
   * (runner_probes/co.run400.co11.txt, 2026-09-20).
   */
  if (lib_co_400_question_pending ())
    {
      lib_co_400_note_refusal ();
      return FALSE;
    }

  /*
   * The crowd's FIRST object decides: the walk keeps one best, and only a
   * tie whose Short matches the best's parks the pending object the
   * question is asked from.  So a namesake pair the line names after some
   * other object of the same score never asks -- p4WTIE's `chop stone
   * knife` is the game's DontUnderstand text, where `chop stone` and
   * p4CO's `chop tree rock` (the pair first, the odd one after) both ask
   * (runner_probes/wtie.run400.w6.txt, runner_probes/wtie.run400.w7.txt,
   * 2026-09-20).
   */
  term = prop_get_indexed_string (bundle, "Objects", tied[0], "Short");
  if (scr_strempty (term) || !lib_input_contains_word (input, term))
    return FALSE;

  count = 0;
  for (index_ = 0; index_ < (scr_int) tied.size (); index_++)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "Objects", tied[index_],
                                      "Short");
      if (!scr_strempty (name) && scr_strcasecmp (name, term) == 0)
        count++;
    }
  if (count < 2)
    return FALSE;

  lib_co_400_raise (game, lib_co_400_scan_term_400 (game, tied, term), tied);
  return TRUE;
}

/*
 * lib_npc_400_raise_for_line()
 *
 * The character half of the same question.  generaltasks raises it at
 * 48B815-48B928 (48BA87-48BB53 on its second pass): the term var_A4 is the
 * flagged NPC's Name (field 0), replaced by every one of its aliases (field
 * 8, count field 12) that is a whole word of the line, and the prompt is
 * "Which " & term & ".  " & list & "?", with "That is still ambiguous!" in
 * its place while a question is already open.  Measured 2026-09-13 on
 * harness/make_400_battlemultiprobe.py
 * (runner_probes/battlemulti.run400.txt): two NPCs both Named "Guard",
 * Prefix "a", in the room --
 *
 *     attack guard                     ->  Which Guard.  A guard or a guard?
 *     attack droid guard with blaster  ->  Which Guard.  A guard or a guard?
 *     attack guard and droid           ->  That is still ambiguous!  (the
 *                                          question from the line before)
 *
 * and on light_up (runner_probes/light_up.run400.txt T294), where "Red
 * Riven" and "Blue Riven" (Prefixes "Red"/"Blue") share the alias "riven":
 * `attack riven` -> "Which riven.  Red riven or Blue riven?".  So only the
 * term's namesakes are listed -- the droid the line also names is not --
 * each as its Prefix and the lower-cased term, the first capitalised; and
 * nothing is struck, not even the droid.  Neither the question nor its
 * answer is a turn.
 */
static scr_bool
lib_npc_answers_to (scr_gameref_t game, scr_int npc, const scr_char *term)
{
  return lib_named_answers_to (game, "NPCs", npc, "Name", term);
}

/*
 * lib_prefix_words_in_input_400()
 *
 * Count the words of a Prefix that the input line holds, the 4.0 Prefix
 * contest's score for objects and characters alike.  The Prefix is cut on
 * single spaces, so a double space yields an empty word, which scores
 * nothing.
 */
scr_int
lib_prefix_words_in_input_400 (const scr_char *input, const scr_char *prefix)
{
  scr_owned_string copy (scr_strdup (prefix));
  scr_char *word, *next;
  scr_int score;

  score = 0;
  for (word = copy.get (); word; word = next)
    {
      next = strchr (word, ' ');
      if (next)
        *next++ = NUL;
      if (word[0] != NUL && lib_input_contains_word_400 (input, word))
        score++;
    }
  return score;
}

/*
 * lib_npc_400_prefix_score()
 *
 * The character half of the 4.0 Prefix contest: run400's namesake check
 * (Proc_21_40_45E99C) asks Proc_21_49_450610 which of the term's namesakes
 * the line describes best, and the answer is simply how many of a
 * character's OWN Prefix words the line holds as whole words.  A strict
 * maximum wins outright and nothing is asked; anything else -- a tie at
 * any height, zero included -- leaves the ambiguity standing.
 *
 * Measured 2026-09-20 on p4PFX2.taf (make_prefixprobe.py ... 2): Ann "a big
 * red", Bob "a red" and Cid "the red", all three aliased "guard" and all in
 * the room, run400x runner_probes/pfx2.run400.txt.
 *
 *     x guard          ->  Which guard.  A big red guard, a red guard or
 *                          the red guard?      (0-0-0)
 *     x red guard      ->  the same question                    (1-1-1)
 *     x a red guard    ->  the same question                    (2-2-1)
 *     x the red guard  ->  CID DESC.                            (1-1-2)
 *     x big guard      ->  ANN DESC.                            (1-0-0)
 *     x a guard        ->  the same question                    (1-1-0)
 *
 * `x the red guard` is the cell that settles the articles: "the" has to
 * score like any other Prefix word for Cid to win it, and `x a red guard`
 * would be Cid's too if only "red" counted.  So the whole Prefix is split
 * on spaces and every word scores -- the same rule the object scorer
 * 463640 follows (lib_verb_object_name_score()).
 *
 * The question's LIST is untouched by the contest: `x a red guard` still
 * offers all three guards though Cid scores under the others.  Only the
 * object question narrows itself to its winners; see
 * lib_disambiguate_object_common().
 *
 * An empty NPC Prefix scores nothing, and that one is from the decompile
 * rather than from a probe: the 4.0 loader substitutes "a" for an empty
 * OBJECT prefix (4900EC) -- which is what makes a prefix-less object score
 * a typed "a" -- and mdlSpreadTheLoad has no such default for characters.
 *
 * 450610's own body (4504BC-45060D) differs from ours in three ways that
 * no probe has reached yet, all of them noted rather than modelled:
 *
 *   - it has NO admission test whatever, so an ABSENT character answering
 *     to the term joins the contest and can win it away from the two in
 *     the room.  We score only the namesakes, which are room-gated.
 *   - its counter is reset once per character (4504DF), not once per name
 *     as the object contest's is (45433D), so a character whose Name AND
 *     one alias both equal the term counts its Prefix words twice.
 *   - Split() is called with the delimiter argument Missing, so the Prefix
 *     is cut on single spaces with no trimming; a double space yields an
 *     empty word, which scores nothing because 454CB0("") returns 0.  Ours
 *     does the same by walking to the next ' '.
 *
 * Below 4.0 there is no character contest at all -- run390 holds no Split
 * call anywhere, and its one Prefix test, lastword() (42DA40), is reached
 * only from takes, drops, referencedob, examines and co, all objects.  Its
 * characters() picks by index order instead; see NPC_PICK_FIRST/LAST.
 */
scr_int
lib_npc_400_prefix_score (scr_gameref_t game, scr_int npc,
                          const scr_char *input)
{
  const scr_char *prefix;

  prefix = prop_get_indexed_string (gs_get_bundle (game), "NPCs",
                                    npc, "Prefix");
  if (scr_strempty (prefix))
    return 0;

  return lib_prefix_words_in_input_400 (input, prefix);
}

/* TRUE when the contest picks one of these namesakes outright. */
static scr_bool
lib_npc_400_prefix_settles (scr_gameref_t game,
                            const std::vector<scr_int> &namesakes,
                            const scr_char *input)
{
  scr_int index_, best, best_count;

  best = -1;
  best_count = 0;
  for (index_ = 0; index_ < (scr_int) namesakes.size (); index_++)
    {
      const scr_int score = lib_npc_400_prefix_score (game,
                                                      namesakes[index_],
                                                      input);

      if (score > best)
        {
          best = score;
          best_count = 1;
        }
      else if (score == best)
        best_count++;
    }

  return best > 0 && best_count == 1;
}

static scr_bool
lib_npc_400_find_namesakes_in (scr_gameref_t game, const scr_char *input,
                               std::string *term_out,
                               std::vector<scr_int> *namesakes_out,
                               scr_int *flagged_out,
                               std::string *list_term_out = nullptr)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int room = gs_playerroom (game);
  scr_int npc, found;

  if (!input)
    return FALSE;

  found = FALSE;
  GS_FOR_EACH_NPC (game, npc)
    {
      std::vector<scr_int> namesakes;
      scr_vartype_t vt_key[4];
      const scr_char *name, *term, *list_term;
      scr_int alias_count, alias, other;

      if (!npc_in_room (game, npc, room))
        continue;

      name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
      term = (!scr_strempty (name) && lib_input_contains_word (input, name))
             ? name : nullptr;
      list_term = term;
      alias_count = lib_alias_prepare (bundle, vt_key, "NPCs", npc);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (!scr_strempty (alias_name)
              && lib_input_contains_word (input, alias_name))
            term = alias_name;
        }
      if (!term)
        continue;
      /* npc_in_command's list term (var_98) is the Name when it is a whole
       * word of the line, the alias loop skipped (GoTo 45E682); only the
       * header term (48B815) takes the alias.  noximion T57 `kill venemous
       * buzzard with sword`: "Which buzzard.   venemous buzzard or  venemous
       * buzzard?" (runner_transcripts/noximion.txt). */
      if (!list_term)
        list_term = term;

      GS_FOR_EACH_NPC (game, other)
        {
          if (npc_in_room (game, other, room)
              && lib_npc_answers_to (game, other, term))
            namesakes.push_back (other);
        }
      if (namesakes.size () < 2)
        continue;

      /* The Prefix contest can settle the term outright; see above. */
      if (lib_npc_400_prefix_settles (game, namesakes, input))
        continue;

      /*
       * The Runner's loop (48B547) has no break: the term and the list are
       * the FIRST hit's -- a later namesake writes neither, its term being
       * in the list already (45E7B5) -- but MemVar_4941EC, the flagged
       * index, is overwritten by every hit that is in the player's room
       * (45E8CA), so it ends up the LAST one's.  All of ours are in the
       * room, so: first hit for the text, last for the index.
       */
      if (!found)
        {
          if (term_out)
            *term_out = term;
          if (list_term_out)
            *list_term_out = list_term;
          if (namesakes_out)
            *namesakes_out = namesakes;
        }
      found = TRUE;
      if (flagged_out)
        *flagged_out = npc;
      if (!flagged_out)
        break;
    }

  return found;
}

scr_bool
lib_npc_400_find_namesakes (scr_gameref_t game, std::string *term_out,
                            std::vector<scr_int> *namesakes_out)
{
  return lib_npc_400_find_namesakes_in (game, run_get_dispatch_input (),
                                        term_out, namesakes_out, nullptr);
}

/*
 * lib_npc_400_line_names_namesakes()
 *
 * TRUE if a 4.0 line names a term that two or more present characters answer
 * to -- the test generaltasks makes before its "Which" question.  Exposed for
 * run_player_input(), which needs it after the dispatch input is cleared.
 */
scr_bool
lib_npc_400_line_names_namesakes (scr_gameref_t game, const scr_char *line)
{
  return lib_is_version_400 (game)
         && lib_npc_400_find_namesakes_in (game, line,
                                           nullptr, nullptr, nullptr);
}

static scr_bool
lib_npc_400_raise_for_line_in (scr_gameref_t game, const scr_char *input)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_filterref_t filter = gs_get_filter (game);
  std::vector<scr_int> namesakes;
  std::string term_string, npc_term, lower;
  const scr_char *term;
  scr_int index_, flagged;

  flagged = -1;
  if (!lib_npc_400_find_namesakes_in (game, input, &term_string, &namesakes,
                                      &flagged, &npc_term))
    return FALSE;
  /* The list is built by the character scan (45E7D3) and printed whole, so
   * it keeps the character's term even when the object half prints. */

  /*
   * The two halves of the question read ONE untyped index, and the object
   * half (48B6B1) gets first refusal: `MemVar_4941EC < MemVar_494050 And
   * co(3, MemVar_4941EC)`, i.e. the flagged CHARACTER index doubles as an
   * object index, and if the line names that object the term printed over
   * the character list is the object's -- its Short, replaced by each alias
   * of it that is a whole word of the line, the last winning.
   *
   * It is a plain collision, not a term choice.  p4BATT (run400x
   * runner_probes/batt.run400.txt and runner_probes/batt.run400.b.txt,
   * 2026-09-20) has Dave 0, Ann 1, Bob 2, Cora 3 with Ann and Bob both "a
   * guard", and objects sword 0, club 1, stone 2; the flagged index is
   * Bob's 2, so `attack guard with stone`, `give stone to guard`, `x guard
   * stone` and `x stone guard` all print "Which stone.  A guard or a
   * guard?" while `x guard sword`, `attack guard with club`, `give club to
   * guard` and `x guard dave` print "Which guard.".
   */
  if (flagged >= 0 && flagged < gs_object_count (game)
      && gs_object_seen (game, flagged)
      && obj_indirectly_in_room (game, flagged, gs_playerroom (game))
      && lib_co_400_name_word (game, flagged, input))
    {
      const scr_char *name;
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      name = prop_get_indexed_string (bundle, "Objects", flagged, "Short");
      term_string = name ? name : "";
      alias_count = lib_alias_prepare (bundle, vt_key, "Objects", flagged);
      for (alias = 0; alias < alias_count; alias++)
        {
          vt_key[3].integer = alias;
          name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (!scr_strempty (name) && lib_input_contains_word_400 (input, name))
            term_string = name;
        }
    }
  term = term_string.c_str ();

  /* One pass of the original loop; the braces keep its indentation. */
    {
      game->is_admin = TRUE;
      if (disambig.question.was_pending)
        {
          pf_buffer_string (filter, "That is still ambiguous!\n");
          return TRUE;
        }

      for (index_ = 0; npc_term[index_] != NUL; index_++)
        lower += (scr_char) tolower ((unsigned char) npc_term[index_]);

      lib_which_head (game, term, nullptr);
      pf_new_sentence (filter);
      for (index_ = 0; index_ < (scr_int) namesakes.size (); index_++)
        {
          const scr_char *prefix;

          if (index_ > 0)
            pf_buffer_string (filter,
                              index_ == (scr_int) namesakes.size () - 1
                              ? " or " : ", ");
          prefix = prop_get_indexed_string (bundle, "NPCs", namesakes[index_],
                                            "Prefix");
          /*
           * Prefix & " " & term with no empty-Prefix test (45E811-45E82A):
           * an empty Prefix leaves its space, and 446BB4's capital then
           * lands on it -- "Which woman.   woman or  woman?".  The loader
           * defaults only OBJECT prefixes to "a"; asteroid_after's and
           * Vendetta's ALRs are written against the spaced form.
           */
          /* Scarier drops the empty Prefix's space unless the game's ALRs
           * were written against the spaced form (lib_which_runner_form). */
          if (!scr_strempty (prefix) || lib_which_runner_form (game))
            {
              pf_buffer_string (filter, prefix);
              pf_buffer_character (filter, ' ');
            }
          pf_buffer_string (filter, lower.c_str ());
        }
      pf_buffer_string (filter, "?\n");

      disambig.question.pending = TRUE;
      disambig.question.term = term;
      disambig.question.command = input ? input : "";
      disambig.question.candidates.clear ();
      return TRUE;
    }
}

scr_bool
lib_npc_400_raise_for_line (scr_gameref_t game)
{
  return lib_npc_400_raise_for_line_in (game, run_get_dispatch_input ());
}

/* The same, for run_player_input()'s tail, where the dispatch input is gone. */
scr_bool
lib_npc_400_raise_for_line_string (scr_gameref_t game, const scr_char *line)
{
  return lib_npc_400_raise_for_line_in (game, line);
}

/*
 * The answer slot's line.  generaltasks does not score the answer against
 * the prompt's candidates at all: it REBUILDS the stored command with the
 * answer words spliced in front of the term and re-runs the whole thing
 * (48B097-48B15B), exactly as 3.90's prompt does -- `chop tree` / `red`
 * re-runs `chop red tree`, and the ordinary noun score settles it there.
 * The term is appended when the answer does not already hold it as a word
 * (the c() test at 48B0D8), and the part of the command past the term goes
 * on the end.  With the command not holding the term at all the answer is
 * simply put in front of it (48B178).
 *
 * This is what makes `chop tree and chop keys` a second FULL prompt, "Which
 * keys.  The red tree, the blue tree, the mustang key or the truck key?":
 * the rebuilt `chop chop keys tree` names the keys as well as the trees, and
 * a scorer confined to the prompt's own candidates can never see them
 * (runner_probes/co.run400.co11.txt turn 15, 2026-09-20).  "That is still
 * ambiguous!" is then not this slot's answer but the re-run's own raise,
 * meeting the term the last prompt left behind.
 */
std::string
lib_co_400_object_answer_line (const scr_char *line)
{
  const std::string &command = disambig.question.command;
  const std::string &term = disambig.question.term;
  const std::string answer (line ? line : "");
  std::string::size_type at;
  std::string built;

  if (term.empty ())
    return command + " " + answer;

  for (at = 0; at + term.size () <= command.size (); at++)
    {
      if (scr_strncasecmp (command.c_str () + at, term.c_str (),
                           term.size ()) == 0)
        break;
    }
  if (at + term.size () > command.size ())
    return command + " " + answer;      /* 48B178 */

  built = command.substr (0, at) + answer;
  if (!lib_co_contains (built.c_str (), term.c_str ()))
    built += " " + term;
  built += " " + command.substr (at + term.size ());
  return built;
}

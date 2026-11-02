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
 * Matching an input line to a pattern tree: the per-node matchers and the
 * state they walk, the matching options the runner sets, the Runner's "*"
 * wildcard matchers, and uip_match_pattern().  See scparser_internal.h.
 */

#include "scparser_internal.h"


/*
 * uip_match_start()
 * uip_match_end()
 *
 * Set up a string for matching to a pattern tree, and wrap up matching.
 */
static void
uip_match_start (const scr_char *string, scr_gameref_t game)
{
  /* Save string, and restart index. */
  uip.match.string = string;
  uip.match.posn = 0;

  /* Save the game we're working on. */
  uip.match.game = game;
}

static void
uip_match_end (void)
{
  /* Clear match target string, and variable set. */
  uip.match.string = nullptr;
  uip.match.posn = 0;
  uip.match.game = nullptr;
}


/*
 * uip_get_game()
 *
 * Safety wrapper to ensure module code sees a valid game when it requires
 * one.
 */
scr_gameref_t
uip_get_game (void)
{
  assert (gs_is_game_valid (uip.match.game));
  return uip.match.game;
}


/*
 * uip_set_binary_input()
 *
 * run400's wildcard task matcher (Proc_19_50_457D68, body 457AF8-457D65)
 * lower-cases the PATTERN only (457B17) and then locates its literal pieces
 * in the line with binary-compare InStr/Left/Right.  A typed line was
 * lower-cased at read, so that never shows -- but the library's rebuilt
 * lines are not: the take piece (Proc_19_39_46302C) and the insides handler
 * (Proc_19_43_46639C) pre-match an LCase()d copy (462B0D, 465D21) and hand
 * the dispatcher the raw one (462BFE, 465E51), capitals from the object's
 * name included.  With the flag set, a literal word in a pattern that has a
 * `*` and no group or %reference% must match the input byte for byte after
 * lower-casing the pattern word.  A pattern with no wildcard and no group is
 * the bridge's whole-line LCase() equality (45DA29-45DA51) and stays
 * case-free.  A pattern with a [] or {} group goes to the NewParse matcher
 * (45D940), whose literal compares (45D7FA, 45D835) are binary with no
 * LCase() on either side: Professor's lab `take mailbox` pre-matches task 9
 * "[check/get/pull]{the}[mailbox]..." on "get the mailbox on-a rope", misses
 * it on the dispatched "get the Mailbox on-a Rope", and run400 answers
 * DontUnderstand (runner_probes/professor.run400.mail2.txt T24).  %reference%
 * patterns are unmeasured and left alone.
 *
 * Deliberate deviation (2026-09-30): on a line no task matches strictly
 * (uip_set_lenient_tasks()) the rebuilt line is compared case-free.  The
 * pre-match hits case-free and the dispatch then missed byte for byte, so
 * `take golden idol` for a "Golden Idol" with a `get *idol*` task was
 * swallowed as DontUnderstand, neither taken nor answered (5489f80fe).
 */
void
uip_set_binary_input (scr_bool binary)
{
  uip.options.binary_input = binary;
}


/*
 * uip_set_task_commands()
 *
 * Set around task command matching (run_match_task_commands()), which keeps
 * a pre-3.9 comma out of the word boundary; uip.match.comma_is_space is set
 * per match from it and the game's version -- see uip_match_whitespace().
 */
void
uip_set_task_commands (scr_bool task_commands)
{
  uip.options.task_commands = task_commands;
}


/*
 * uip_set_lenient_tasks()
 *
 * Deliberate deviation.  Set by run_all_commands() for a line that no task
 * command matches the Runner's way (run_line_matches_task_strictly()): the
 * author's tasks then get a second, forgiving look at it, so a phrasing the
 * Runner turns away -- an article, a Prefix, an alias, a capital, a stray
 * space the author left at the end of a command -- still reaches the task
 * written for it.  A line the Runner does match is untouched, so no answer
 * the Runner gives changes; only its refusals do.  Here it forgives a
 * command's trailing space (uip_match_whitespace()) and a character name a
 * SYNONYM put a capital back into (uip_case_folds_name()); the strict
 * %object% binding is simply not turned on (run_match_task_commands()).
 */
void
uip_set_lenient_tasks (scr_bool lenient)
{
  uip.options.lenient_tasks = lenient;
}

scr_bool
uip_get_lenient_tasks (void)
{
  return uip.options.lenient_tasks;
}


/*
 * uip_wildcard_prepare()
 *
 * The preamble shared by the two wildcard matchers below: lower-case the
 * pattern (and the line when asked), trim the line, and pad it with a space
 * for a pattern starting "* " or ending " *" when asked.  Returns the
 * position of the pattern's first '*', or npos for a pattern without one.
 */
static size_t
uip_wildcard_prepare (std::string &pat, std::string &line,
                      scr_bool lower_line, scr_bool pad)
{
  scr_lowercase (&pat[0]);
  if (lower_line)
    scr_lowercase (&line[0]);
  while (!line.empty () && scr_isspace (line.front ()))
    line.erase (0, 1);
  while (!line.empty () && scr_isspace (line.back ()))
    line.pop_back ();

  const size_t first_star = pat.find ('*');
  if (first_star == std::string::npos)
    return first_star;

  if (pad && pat.compare (0, 2, "* ") == 0)
    line.insert (0, " ");
  if (pad && pat.size () >= 2 && pat.compare (pat.size () - 2, 2, " *") == 0)
    line.append (" ");
  return first_star;
}


/*
 * uip_wildcard_match_400()
 *
 * run400's wildcard matcher itself (Proc_19_50_457D68), for a task command
 * with a '*' and no group or %reference%.  It walks the pattern left to right
 * with no backtracking: each literal piece is found by the FIRST InStr in
 * what is left of the line, and the line is cut past it (457CDA-457D02).  A
 * space is put back in front of the line only when the pattern text after a
 * piece's '*' starts with one (457D2E), so "* " can match nothing; the extra
 * stars of a "**" or "***" run are dropped one at a time by the Var_96 = 1 arm
 * (457C4E-457C6C), which puts nothing back.  A leading "* " or trailing " *"
 * pads the line with a space first (457B62, 457B92), and the text before the
 * first '*' must be the line's prefix (457BD0).  run390's checkwild (4346A8)
 * never cuts the line, so this is 4.0 only.
 *
 * Measured on The Town of Azra (4.00): task 27 "buy *** *rawhide armor*"
 * misses `buy rawhide armor` -- the lone " " piece eats the space inside
 * "rawhide armor", and "rawhide armor" is then not found -- so run400 answers
 * "I don't think that is for sale." (runner_transcripts/the_town_of_azra.txt
 * T13), while task 28 "buy *** bronze helmet*" takes `buy a bronze helmet`.
 * run390 sells the armor on the same line (the_town_of_azra_v390).
 *
 * The pattern is lower-cased and the line compared as uip_match() would see
 * it: byte for byte while uip.options.binary_input is set, else lower-cased
 * too.
 */
scr_bool
uip_wildcard_match_400 (const scr_char *pattern, const scr_char *string)
{
  std::string pat (pattern), line (string);
  scr_bool matched = TRUE;

  const size_t first_star
    = uip_wildcard_prepare (pat, line, !uip.options.binary_input
                                       || uip.options.lenient_tasks, TRUE);
  if (first_star == std::string::npos)
    return FALSE;
  if (line.substr (0, first_star) != pat.substr (0, first_star))
    matched = FALSE;

  while (!pat.empty ())
    {
      const size_t star = pat.find ('*');

      if (star == std::string::npos)
        {
          /* The tail must end the line (Right(line, Len(pattern))). */
          const std::string tail = line.size () > pat.size ()
                                   ? line.substr (line.size () - pat.size ())
                                   : line;
          if (tail != pat)
            matched = FALSE;
          pat.clear ();
        }
      else if (star == 0)
        pat.erase (0, 1);
      else
        {
          const size_t at = line.find (pat.substr (0, star));

          if (at == std::string::npos)
            {
              matched = FALSE;
              pat.clear ();
              continue;
            }
          line.erase (0, at + star);
          pat.erase (0, star + 1);
          if (!pat.empty () && pat[0] == ' ')
            line.insert (0, " ");
        }
    }

  return matched;
}


/*
 * uip_wildcard_match_pre400()
 *
 * The 3.7-3.9 checkwild (run390 4346A8, run380 4295E0, run370 4243D4), for a
 * task command with a '*'.  The text before the first '*' must be the line's
 * prefix; every later literal piece need only occur somewhere in the line
 * (InStr on the whole line, nothing cut, so order is free); the text after the
 * last '*' must be the line's end, Right(line, Len(piece)).  run390 pads the
 * line with a space first for a pattern starting "* " or ending " *"
 * (4344F0, 434520); run380 and run370 pad nothing, so there "throw %object% *"
 * needs something typed after the object and "* x" something before it.
 *
 * The caller has already put an object's name in place of any %object%.
 * Case is ignored: run390 lower-cases the pattern and the line is lower-case
 * by then.
 */
scr_bool
uip_wildcard_match_pre400 (const scr_char *pattern, const scr_char *string,
                           scr_bool pad)
{
  scr_char pat_local[UIP_ALLOCATION_AVOIDANCE_SIZE];
  scr_char line_local[UIP_ALLOCATION_AVOIDANCE_SIZE];
  std::string pat_heap, line_heap;
  const size_t pat_length = strlen (pattern);
  size_t length = strlen (string), line_length = 0, index_;
  scr_char *pat = pat_local, *line = line_local;
  const scr_char *piece, *pat_end, *line_end, *star;

  /*
   * Work on lower-cased copies, the line trimmed and padded, held on the
   * stack unless they are unusually long: this is tried against every '*'
   * command of every task on every line.
   */
  if (pat_length + 1 > sizeof (pat_local))
    {
      pat_heap.resize (pat_length + 1);
      pat = &pat_heap[0];
    }
  for (index_ = 0; index_ <= pat_length; index_++)
    pat[index_] = scr_tolower (pattern[index_]);
  pat_end = pat + pat_length;

  star = (const scr_char *) memchr (pat, '*', pat_length);
  if (!star)
    return FALSE;

  if (length + 3 > sizeof (line_local))
    {
      line_heap.resize (length + 3);
      line = &line_heap[0];
    }
  while (length > 0 && scr_isspace (scr_tolower (string[0])))
    string++, length--;
  while (length > 0 && scr_isspace (scr_tolower (string[length - 1])))
    length--;
  if (pad && pat_length >= 2 && pat[0] == '*' && pat[1] == ' ')
    line[line_length++] = ' ';
  for (index_ = 0; index_ < length; index_++)
    line[line_length++] = scr_tolower (string[index_]);
  if (pad && pat_length >= 2
      && pat_end[-2] == ' ' && pat_end[-1] == '*')
    line[line_length++] = ' ';
  line_end = line + line_length;

  /* The text before the first '*' must be the line's prefix. */
  if ((size_t) (star - pat) > line_length
      || memcmp (line, pat, star - pat) != 0)
    return FALSE;

  for (piece = pat; piece < pat_end; piece = star + 1)
    {
      const size_t rest = pat_end - piece;

      star = (const scr_char *) memchr (piece, '*', rest);
      if (!star)
        return line_length >= rest
               && memcmp (line_end - rest, piece, rest) == 0;
      if (star > piece
          && std::search (line, line + line_length, piece, star)
             == line_end)
        return FALSE;
    }
  return TRUE;
}


/*
 * uip_skip_spaces()
 *
 * Advance the input position over a run of whitespace.
 */
static void
uip_skip_spaces (void)
{
  while (uip.match.string[uip.match.posn] != NUL
         && scr_isspace (uip.match.string[uip.match.posn]))
    uip.match.posn++;
}


/*
 * uip_match_eos()
 * uip_match_word()
 * uip_match_whitespace()
 * uip_match_list()
 * uip_match_alternatives()
 * uip_match_choice()
 * uip_match_optional()
 * uip_match_wildcard()
 *
 * Text element and list/choice element match functions.  Return TRUE, and
 * advance position if necessary, on match, FALSE on no match, with position
 * unchanged.  The matchers between them that carry a comment of their own
 * are the same shape.
 */
static scr_bool
uip_match_eos (void)
{
  /* Check that we hit the string's end. */
  return uip.match.string[uip.match.posn] == NUL;
}

static scr_bool
uip_match_word (scr_ptnoderef_t node)
{
  const scr_char *word, *string;
  scr_int index_;

  /* Get the word to match. */
  assert (node->word);
  word = node->word;
  string = uip.match.string + uip.match.posn;

  if (uip.match.binary_active)
    {
      for (index_ = 0; word[index_] != NUL; index_++)
        {
          if (string[index_] != scr_tolower (word[index_]))
            return FALSE;
        }
      uip.match.posn += index_;
      return TRUE;
    }

  /* Compare string text with this node's word, ignore case. */
  for (index_ = 0; word[index_] != NUL; index_++)
    {
      if (string[index_] != word[index_]
          && scr_tolower (string[index_]) != scr_tolower (word[index_]))
        return FALSE;
    }

  /* Word match, advance position and return. */
  uip.match.posn += index_;
  return TRUE;
}


/*
 * uip_match_variable()
 *
 * A %marker% that is none of the four references is one of the game's own
 * variables, and the Runners substitute its value into the task command
 * before they test it -- see run_substitute_variable_references().  What is
 * left for the tree is a command those arms hand back (one mixing a
 * variable with an %object% at 4.00, say, or a group like COBL's
 * "[throw/drop] [%theobject%/%object%] ..."), and there the marker has to
 * answer the same way it would there: var_get_command_number() is the whole
 * rule, so the value is the NUMERIC one even for a string variable, a
 * capitalised variable Name is unreachable, and "%t_<name>%" and a marker
 * naming nothing match nothing at all.  Measured on p39VARREF/p4VARREF
 * (make_varrefprobe.py, runner_probes/varref.run390.txt,
 * runner_probes/varref.run400.txt, 2026-09-20).
 */
static scr_bool
uip_match_variable (scr_ptnoderef_t node)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_char value[32];
  scr_int number, length;

  /* Get the variable name to match, from overloaded word. */
  assert (node->word);
  if (uip.options.lenient_tasks)
    {
      std::string text;

      /* Deliberate deviation; see var_get_command_text(). */
      if (!var_get_command_text (vars, node->word, text))
        return FALSE;
      length = (scr_int) text.size ();
      if (scr_strncasecmp (uip.match.string + uip.match.posn,
                           text.c_str (), length) != 0)
        return FALSE;
      uip.match.posn += length;
      return TRUE;
    }
  if (!var_get_command_number (vars, node->word, &number))
    return FALSE;

  /* Compare the value against the current string position. */
  snprintf (value, sizeof (value), "%ld", number);
  length = strnlen (value, sizeof (value));
  if (strncmp (uip.match.string + uip.match.posn, value, length) == 0)
    {
      /* Match, advance position and return. */
      uip.match.posn += length;
      return TRUE;
    }

  return FALSE;
}

static scr_bool
uip_match_whitespace (scr_bool hard)
{
  /* If next character is space, read whitespace and return. */
  if (scr_isspace (uip.match.string[uip.match.posn]))
    {
      /* Space match, advance position and return. */
      uip_skip_spaces ();
      return TRUE;
    }

  /*
   * 3.7/3.8 never split a line at a comma, and every handler finds its verb
   * with c(), whose word may end at a comma (run370 423C80, run380 429048).
   * So a comma right after a word is a space to the library: `x, coin`,
   * `look at, coin`, `read, coin`, `drop, coin` and `take, coin` answer as
   * they do without it.  The word after it must still start after a space,
   * so `x,coin` examines nothing: the boundary matches but the comma stays
   * for the wildcard, and the answer is "Nothing special.".  p37EXAM/p38EXAM,
   * run370x runner_probes/exam.run370.comma.rtf / run380x
   * runner_probes/exam.run380.comma.rtf.  Library patterns only; how a pre-3.9
   * task command meets a comma is unmeasured.
   */
  if (uip.match.comma_is_space && uip.match.string[uip.match.posn] == ','
      && uip.match.posn > 0
      && !scr_isspace (uip.match.string[uip.match.posn - 1]))
    {
      if (uip.match.string[uip.match.posn + 1] == NUL
          || scr_isspace (uip.match.string[uip.match.posn + 1]))
        {
          uip.match.posn++;
          while (uip.match.string[uip.match.posn] != NUL
                 && scr_isspace (uip.match.string[uip.match.posn]))
            uip.match.posn++;
        }
      return TRUE;
    }

  /*
   * A space the author left after the last WORD of a pattern is a space the
   * input has to have, and the player's line is trimmed, so such a pattern
   * matches nothing at all.  "Sommeril" has a task whose commands are "get
   * placemat " (trailing space), "take placemat", "get placemat page" and
   * three more; run400 answers `take placemat` with the task and `get
   * placemat` with the library's "Take what?", the placemat being unlisted
   * and so unreferenceable (measured 2026-09-05,
   * runner_probes/sommeril.run400.placemat.txt).  Without this the
   * end-of-string rule below matched the stray space and the task claimed
   * both.
   *
   * Only after a word: a trailing space after a [] or {} group is ignored,
   * and the corpus is emphatic about it -- "Woof"'s "[chase/hunt]
   * [bizet/Bizet/cat] " has to answer `chase cat`, and eight more rows
   * (the_cat_in_the_tree, skydiver, apokalupsis, wax_worx, vendetta, hub,
   * magicshow, house, valley) fail the same way if the space is made to
   * count there.  uip_parse_list() marks the ones that do count.
   *
   * Deliberate deviation: on a line no task matches the Runner's way, the
   * stray space is forgiven -- see uip_set_lenient_tasks().
   */
  if (hard && !uip.options.lenient_tasks)
    return FALSE;

  /*
   * No match.  However, if we're trying to match space, this is a word
   * boundary.  So... even though we're not sitting on a space, if the string
   * prior character is whitespace, "double-match" the space.
   *
   * Also, match if we haven't yet matched any text.  In effect, this means
   * leading spaces on patterns will be ignored -- harmless, even useful,
   * and long since validated by the walkthrough corpus.
   */
  if (uip.match.posn == 0 || scr_isspace (uip.match.string[uip.match.posn - 1]))
    return TRUE;

  /*
   * And that's not all.  We also want to match whitespace if we're at the end
   * of a string (another word boundary).  This will permit patterns that end
   * in optional elements to succeed since options and wildcards always match,
   * even if to no text.
   */
  if (uip.match.string[uip.match.posn] == NUL)
    return TRUE;

  /* No match.  Really. */
  return FALSE;
}


/*
 * uip_match_double_whitespace()
 *
 * Two or more spaces in the pattern: only a run of two or more in the input
 * will do, literally, with none of the word-boundary leniency above.  No
 * Runner collapses the pattern, and 4.0 collapses the typed line, so a
 * pattern like "ask  *girl* about *" is unreachable from the keyboard in
 * every version; see uip_parse_element().
 */
static scr_bool
uip_match_double_whitespace (void)
{
  if (scr_isspace (uip.match.string[uip.match.posn])
      && uip.match.string[uip.match.posn] != NUL
      && scr_isspace (uip.match.string[uip.match.posn + 1])
      && uip.match.string[uip.match.posn + 1] != NUL)
    {
      uip_skip_spaces ();
      return TRUE;
    }

  return FALSE;
}


/*
 * uip_match_group_double_whitespace()
 *
 * Two or more spaces right after a [] or {} group.  run400's NewParse
 * (45D940) Trim()s the remainder when the group is omitted, so the run is
 * then an ordinary word boundary; with an alternative included the run is
 * the start of a literal stretch and needs the typed doubles a 4.0 line
 * never has -- see uip_next_token().  uip_match_optional() and
 * uip_match_choice() leave whether the group just matched empty in
 * uip.match.last_group_empty; this node is always that group's right
 * sibling, so nothing else runs between them.
 */
static scr_bool
uip_match_group_double_whitespace (void)
{
  if (uip.match.last_group_empty)
    return uip_match_whitespace (FALSE);

  /*
   * Deliberate deviation (2026-09-30): on a line no task matches the
   * Runner's way (uip_set_lenient_tasks()), a doubled space after an
   * included {} option is an ordinary word boundary.  An author's
   * accidental `zap {the}  {standard} washing machine` otherwise answers
   * only the bare `zap washing machine`.  Only after an option: the Runner
   * still reaches such a task with the option left out, so the author saw
   * it run.  A doubled space anywhere else makes the task unreachable in
   * every Runner, and waking it wakes text nobody tested -- The Ticket's
   * `ask  *girl* about *` would answer every question to the girl with a
   * random phrase and swallow her topics.
   */
  if (uip.options.lenient_tasks && uip.match.last_group_optional)
    return uip_match_whitespace (FALSE);

  return uip_match_double_whitespace ();
}


/*
 * uip_match_join()
 *
 * Optional whitespace, invented between two adjacent [] or {} groups.  Eat a
 * space if one is present, but never fail -- see uip_parse_list().
 */
static scr_bool
uip_match_join (void)
{
  uip_skip_spaces ();
  return TRUE;
}

static scr_bool
uip_match_list (scr_ptnoderef_t node)
{
  scr_ptnoderef_t child;

  /*
   * If this list is empty, fail the match.  This special-case handling is
   * what catches constructed temporary lists for wildcard-like items that
   * don't actually encompass anything.
   */
  if (!node->left_child)
    return FALSE;

  /* Match everything listed sequentially. */
  for (child = node->left_child; child; child = child->right_sibling)
    {
      if (!uip_match_node (child))
        {
          /* No match. */
          return FALSE;
        }
    }

  /* Matched. */
  return TRUE;
}

static scr_bool
uip_match_alternatives (scr_ptnoderef_t node)
{
  scr_ptnoderef_t child;
  scr_int start_posn, extent;
  scr_bool matched;

  /* Note the start position for rewind between tries. */
  start_posn = uip.match.posn;

  /*
   * Try a match on each of the children, looking to see which one moves the
   * position on the furthest.  Match on this one.  This is a "maximal munch".
   */
  extent = uip.match.posn;
  matched = FALSE;
  for (child = node->left_child; child; child = child->right_sibling)
    {
      uip.match.posn = start_posn;
      if (uip_match_node (child))
        {
          /* Matched. */
          matched = TRUE;
          if (uip.match.posn > extent)
            extent = uip.match.posn;
        }
    }

  /* If matched, set position to extent; if not, back to start. */
  uip.match.posn = matched ? extent : start_posn;

  /* Return match status. */
  return matched;
}

static scr_bool
uip_match_choice (scr_ptnoderef_t node)
{
  scr_int start_posn;
  scr_bool matched;

  /*
   * Return the result of matching alternatives.  The choice will therefore
   * fail if none of the alternatives match.  Note for a following double
   * space whether the choice consumed anything (an empty alternative is
   * spliced as "" and Trim()ed away just like an omitted option).
   */
  start_posn = uip.match.posn;
  matched = uip_match_alternatives (node);
  uip.match.last_group_empty = uip.match.posn == start_posn;
  uip.match.last_group_optional = FALSE;
  return matched;
}

static scr_bool
uip_match_optional (scr_ptnoderef_t node)
{
  scr_int start_posn;
  scr_ptnode_t list = {};
  scr_bool matched;

  /* Note the start position for rewind on empty match. */
  start_posn = uip.match.posn;

  /*
   * Look ahead to see if we can match to nothing, and still have the main
   * pattern match.  If we can, we'll go with this.  It's a "minimal munch"-ish
   * strategy, but seems to be what Adrift does in this situation.
   */
  list.type = NODE_LIST;
  list.left_child = node->right_sibling;

  /* Match on the temporary list, with this option counting as empty. */
  uip.match.last_group_empty = TRUE;
  matched = uip_match_list (&list);

  /*
   * If the temporary matched and consumed text, rewind position to match
   * nothing.  If it didn't, rewind anyway -- a failed look-ahead may still
   * have advanced the position, since uip_match_list() has no backtracking of
   * its own -- and then match alternatives to consume anything that may match
   * our options.
   *
   * The rewind before uip_match_alternatives() matters whenever an option is
   * a prefix of the word actually in the input.  Monsters (Release 2) has
   * "shine {the} [flashlight/light] {on} {the} {brainsucker} {brain}
   * {monster}": against "shine flashlight on the brainsucker" the look-ahead
   * from {brainsucker} lets {brain} eat the first five letters of
   * "brainsucker" (uip_match_word() is a prefix compare with no word-boundary
   * check), then fails on the trailing "sucker".  Without the rewind the
   * alternatives are tried at "sucker", {brainsucker} matches nothing, and
   * the whole pattern dies -- which lost the game its brainsucker task even
   * though the author's own published transcript shows the command working.
   */
  if (matched && uip.match.posn > start_posn)
    uip.match.posn = start_posn;
  else
    {
      uip.match.posn = start_posn;
      uip_match_alternatives (node);
    }

  /* Note for a following double space whether anything was consumed. */
  uip.match.last_group_empty = uip.match.posn == start_posn;
  uip.match.last_group_optional = TRUE;

  /* Return TRUE no matter what. */
  return TRUE;
}


/*
 * uip_match_right_siblings()
 *
 * Helper for wildcard and %text% matching.  Match the tree to the right of
 * the given node at successive character positions after the current one,
 * and return TRUE with the position advanced to the first that matches, or
 * FALSE with the position restored.  This is a "minimal munch", which may
 * or may not be the right thing to be doing here.
 */
static scr_bool
uip_match_right_siblings (scr_ptnoderef_t node)
{
  scr_int start_posn, limit, index_;
  scr_bool matched;
  scr_ptnode_t list = {};

  /* Nothing to the right matches nowhere; see uip_match_list(). */
  if (!node->right_sibling)
    return FALSE;

  /* Note the start position for rewind on no match. */
  start_posn = uip.match.posn;

  /*
   * To make life a little easier, we'll match on the tree to the right of
   * this node by constructing a temporary list node, containing stuff to the
   * right of the node, and then matching on that.  The node lives on the
   * stack: the pool is usually spent on the compiled task commands.
   */
  list.type = NODE_LIST;
  list.left_child = node->right_sibling;

  /*
   * Repeatedly try to match the rest of the tree at successive character
   * positions, and stop if we succeed.  When scanning forward, take care to
   * include the NUL, needed to match TOK_EOS.
   */
  matched = FALSE;
  limit = strlen (uip.match.string) + 1;
  for (index_ = uip.match.posn + 1; index_ < limit; index_++)
    {
      uip.match.posn = index_;
      if (uip_match_list (&list))
        {
          /* The rest of the tree matches at this point. */
          uip.match.posn = index_;
          matched = TRUE;
          break;
        }
    }

  /* If we didn't match in the loop, restore position. */
  if (!matched)
    uip.match.posn = start_posn;

  return matched;
}

static scr_bool
uip_match_wildcard (scr_ptnoderef_t node)
{
  /*
   * At least one game uses patterns like "thing******...".  Why?  Who knows.
   * But if we're in a list of wildcards, and not the first, ignore the call;
   * only the final one needs handling.
   */
  if (node->right_sibling && node->right_sibling->type == NODE_WILDCARD)
    return TRUE;

  /* Match the rest of the tree, and return TRUE whether it matched or not. */
  uip_match_right_siblings (node);
  return TRUE;
}


/*
 * uip_match_number()
 * uip_match_text()
 *
 * Attempt to match a number, or a run of text, from the string.
 */
static scr_bool
uip_match_number (void)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int number;

  /* Attempt to read a number from input. */
  if (sscanf (uip.match.string + uip.match.posn, "%ld", &number) == 1)
    {
      /* Advance position over the number. */
      while (uip.match.string[uip.match.posn] == MINUS
             || uip.match.string[uip.match.posn] == PLUS)
        uip.match.posn++;
      while (scr_isdigit (uip.match.string[uip.match.posn]))
        uip.match.posn++;

      /* Set number reference in variables and return. */
      var_set_ref_number (vars, number);
      return TRUE;
    }

  /* No match. */
  return FALSE;
}

static scr_bool
uip_match_text (scr_ptnoderef_t node)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int start_posn;

  /*
   * Note the start position, and, as with wildcards, match the rest of the
   * tree at successive character positions; on no match the position is
   * restored.
   */
  start_posn = uip.match.posn;
  if (uip_match_right_siblings (node))
    {
      /* Found a match; create a string and save the text. */
      std::string string (uip.match.string + start_posn,
                          uip.match.posn - start_posn);

      /*
       * Adrift seems to save referenced text as all-lowercase; we need to do
       * the same.
       */
      scr_lowercase (&string[0]);
      var_set_ref_text (vars, string.c_str ());

      /* Return TRUE since we matched text. */
      return TRUE;
    }

  /* Return FALSE on no match. */
  return FALSE;
}


/*
 * uip_skip_article()
 *
 * Skip over any "a"/"an"/"the"/"some" at the head of a string.  Helper for
 * %character% and %object% matchers.  Returns the revised string position.
 */
scr_int
uip_skip_article (const scr_char *string, scr_int start)
{
  scr_int posn;

  /* Skip over articles. */
  posn = start;
  if (scr_compare_word (string + posn, "a", 1))
    posn += 1;
  else if (scr_compare_word (string + posn, "an", 2))
    posn += 2;
  else if (scr_compare_word (string + posn, "the", 3))
    posn += 3;
  else if (scr_compare_word (string + posn, "some", 4))
    posn += 4;

  /* Skip any whitespace, and return. */
  while (scr_isspace (string[posn]) && string[posn] != NUL)
    posn++;
  return posn;
}


/*
 * uip_match_node()
 *
 * Attempt to match the given node to the current match string/position.
 * Return TRUE, with position advanced, on match, FALSE on fail with the
 * position unchanged.
 */
scr_bool
uip_match_node (scr_ptnoderef_t node)
{
  scr_bool match = FALSE;

  /* Match depending on node type. */
  switch (node->type)
    {
    case NODE_EOS:
      match = uip_match_eos ();
      break;
    case NODE_WORD:
      match = uip_match_word (node);
      break;
    case NODE_VARIABLE:
      match = uip_match_variable (node);
      break;
    case NODE_WHITESPACE:
      match = uip_match_whitespace (FALSE);
      break;
    case NODE_HARD_WHITESPACE:
      match = uip_match_whitespace (TRUE);
      break;
    case NODE_DOUBLE_WHITESPACE:
      match = uip_match_double_whitespace ();
      break;
    case NODE_GROUP_DOUBLE_WHITESPACE:
      match = uip_match_group_double_whitespace ();
      break;
    case NODE_JOIN:
      match = uip_match_join ();
      break;
    case NODE_LIST:
      match = uip_match_list (node);
      break;
    case NODE_CHOICE:
      match = uip_match_choice (node);
      break;
    case NODE_OPTIONAL:
      match = uip_match_optional (node);
      break;
    case NODE_WILDCARD:
      match = uip_match_wildcard (node);
      break;
    case NODE_CHARACTER_REFERENCE:
      match = uip_match_character (node);
      break;
    case NODE_OBJECT_REFERENCE:
      match = uip_match_object (node);
      break;
    case NODE_NUMBER_REFERENCE:
      match = uip_match_number ();
      break;
    case NODE_TEXT_REFERENCE:
      match = uip_match_text (node);
      break;
    default:
      scr_fatal ("uip_match_node: invalid type, %ld\n", (scr_int) node->type);
    }

  return match;
}


/*
 * uip_needs_trim()
 *
 * TRUE if uip_cleanse_string() would change the string -- leading or
 * trailing whitespace.  Most input lines have none, and are matched in
 * place rather than copied.
 */
static scr_bool
uip_needs_trim (const scr_char *string)
{
  const size_t length = strlen (string);

  return length > 0
         && (scr_isspace (string[0]) || scr_isspace (string[length - 1]));
}


/*
 * uip_is_referenced_text()
 *
 * TRUE if the string lies inside the game's referenced text.  Such a string
 * cannot be matched in place: a %text% match stores its capture by
 * reallocating that very buffer, and the matcher then carries on reading the
 * string -- an entity reference tries each candidate against the rest of the
 * pattern in turn.  lib_parse_next_object() matches "%object% and %text%"
 * against the referenced text itself, so with two objects answering to the
 * first noun, the second was compared with the freed or overwritten string.
 */
static scr_bool
uip_is_referenced_text (const scr_char *string, scr_gameref_t game)
{
  const scr_char *const text = var_get_ref_text (gs_get_vars (game));
  std::less_equal<const scr_char *> not_after;

  return not_after (text, string) && not_after (string, text + strlen (text));
}


/*
 * uip_opens_differently()
 *
 * TRUE if the tree at node can only open with a literal word, and none of
 * the words it could open with starts with the given character -- the test
 * uip_match_word() would fail on its first comparison.  Sets *in_group when
 * the refusal is a [choice]'s, which leaves its mark on the match state.
 */
static scr_bool
uip_opens_differently (scr_ptnoderef_t node, scr_char first, scr_bool binary,
                       scr_bool *in_group)
{
  scr_ptnoderef_t child;

  switch (node->type)
    {
    case NODE_WORD:
      if (node->word[0] == NUL)
        return FALSE;
      return binary ? first != scr_tolower (node->word[0])
                    : (first != node->word[0]
                       && scr_tolower (first) != scr_tolower (node->word[0]));

    case NODE_LIST:
      return node->left_child
             && uip_opens_differently (node->left_child, first, binary,
                                       in_group);

    case NODE_CHOICE:
      if (!node->left_child)
        return FALSE;
      for (child = node->left_child; child; child = child->right_sibling)
        {
          if (!uip_opens_differently (child, first, binary, in_group))
            return FALSE;
        }
      *in_group = TRUE;
      return TRUE;

    default:
      return FALSE;
    }
}


/*
 * uip_match_pattern()
 *
 * Match a string to a compiled pattern, and return TRUE on match, FALSE
 * otherwise.
 */
scr_bool
uip_match_pattern (scr_uip_patternref_t compiled, const scr_char *string,
                   scr_gameref_t game)
{
  scr_char *cleansed;
  scr_char buffer[UIP_ALLOCATION_AVOIDANCE_SIZE];
  scr_bool match;
  assert (compiled && string && game);

  /* A pattern that failed to parse matches nothing. */
  if (!compiled->tree)
    return FALSE;

  /* Dump out the pattern tree if requested. */
  if (if_get_trace_flag (SCR_DUMP_PARSER_TREES))
    uip_debug_dump (compiled->tree);

  /*
   * Most of a game's task commands open with a verb the line does not, and
   * every one of them is tried on every line: turn those away before any of
   * the set-up below.  A line with leading space is left to the full match.
   */
  if (!uip.trace && string[0] != NUL && !scr_isspace (string[0]))
    {
      scr_bool in_group = FALSE;

      if (uip_opens_differently (compiled->tree, string[0],
                                 uip.options.binary_input
                                 && !uip.options.lenient_tasks
                                 && compiled->binary_candidate, &in_group))
        {
          if (in_group)
            {
              uip.match.last_group_empty = TRUE;
              uip.match.last_group_optional = FALSE;
            }
          return FALSE;
        }
    }

  /* Match the string to the pattern tree. */
  cleansed = (uip_needs_trim (string) || uip_is_referenced_text (string, game))
             ? uip_cleanse_string (string, buffer, sizeof (buffer)) : nullptr;
  if (uip.trace)
    scr_trace ("UIParser: string \"%s\"\n", cleansed ? cleansed : string);
  uip_match_start (cleansed ? cleansed : string, game);
  {
    const scr_bool was_binary = uip.match.binary_active;
    const scr_char *const was_punctuation = uip.match.word_end_punctuation;
    const scr_bool was_comma_space = uip.match.comma_is_space;
    const scr_int version = prop_get_taf_version (gs_get_bundle (game));

    uip.match.word_end_punctuation = (version == TAF_VERSION_390) ? ",."
                                     : (version < TAF_VERSION_390) ? "," : "";
    uip.match.comma_is_space = version < TAF_VERSION_390
                               && !uip.options.task_commands;

    uip.match.binary_active = uip.options.binary_input
                              && !uip.options.lenient_tasks
                              && compiled->binary_candidate;
    match = uip_match_node (compiled->tree);
    uip.match.binary_active = was_binary;
    uip.match.word_end_punctuation = was_punctuation;
    uip.match.comma_is_space = was_comma_space;
  }

  /* Clean up matching. */
  uip_match_end ();
  if (cleansed)
    uip_free_cleansed_string (cleansed, buffer);

  /* Return result of matching. */
  if (uip.trace)
    scr_trace ("UIParser: %s\n", match ? "MATCHED!" : "No match");
  return match;
}

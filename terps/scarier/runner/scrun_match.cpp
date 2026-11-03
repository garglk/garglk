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
 * Task command matching: the per-task pattern cache, reference
 * substitution, run_match_task_commands() and the take/put class filter.
 *
 * Split out of scrunner.cpp; see scrunner.h for what the runner files
 * share and scrun_match_internal.h for what the three match files do.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "scrun_match_internal.h"



/*
 * run_is_task_function()
 *
 * Check for the presence of a command function in a task command, and action
 * it if found.  This is a 4.0 feature -- at present, only getdynfromroom()
 * exists.  Returns TRUE if function found and handled.
 *
 * The syntax and the selection are measured against the live 4.0 Runner (see
 * test/adrift4/notes/RUNNER_TESTS_TODO.md section 9, probes GDA..GDR of
 * test/adrift4/harness/make_arena_probe.py).  run400 squeezes every space out
 * of the command, requires what is left to open "#%object%=getdynfromroom("
 * and the *raw* command to end in ")" -- so "getdynfromroom(larder)x" is not
 * a function at all -- then compares the squeezed argument to room names
 * case-insensitively and takes the first non-static object standing directly
 * in the room it finds.  Object order decides between candidates (a room
 * holding "ring" then "gem" yields the ring), a room holding only statics
 * yields nothing, and the reference set here survives the rest of the turn.
 *
 * Two run400 bugs are deliberately not reproduced, both of which can only
 * lose a match the author meant to make:
 *
 *   deliberate: run400 scans rooms with "For r = 0 To roomCount - 1" over a
 *     1-based array, so the game's *last* room can never be found.  Proved
 *     live: "larder" as room 10 of 10 matched nothing, and started returning
 *     its pie the moment a spare 11th room was appended.  (Its object loop
 *     has no such fencepost -- an object added last is still found.)
 *   deliberate: run400 squeezes the spaces out of the argument but compares
 *     it against the unsqueezed room name, so no room whose name contains a
 *     space is reachable -- not even by the manual's own worked example,
 *     "getdynfromroom(The Park)".  We squeeze both sides, which keeps every
 *     match run400 can make and adds the ones it drops.
 *
 * The 3.9 Runner has no getdynfromroom at all (not one occurrence in its
 * P-code), hence the version gate.
 */
scr_bool
run_is_task_function (const scr_char *pattern, scr_gameref_t game)
{
  static const scr_char *const FUNCTION = "#%object%=getdynfromroom(";

  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int length, argument_length, room, object;
  scr_char *argument;

  if (run_get_version (bundle) < TAF_VERSION_400)
    return FALSE;

  /* The Runner tests the raw command's final character for the ")". */
  length = strlen (pattern);
  if (length == 0 || pattern[length - 1] != ')')
    return FALSE;

  std::vector<scr_char> squeezed (length + 1);
  run_squeeze_spaces (pattern, squeezed.data ());
  if (scr_strncasecmp (squeezed.data (), FUNCTION, strlen (FUNCTION)) != 0)
    return FALSE;

  /* Take the argument, dropping the ")" that the raw test guarantees. */
  argument = squeezed.data () + strlen (FUNCTION);
  argument_length = strlen (argument);
  assert (argument_length > 0 && argument[argument_length - 1] == ')');
  argument[argument_length - 1] = NUL;

  /* Compare the argument read in against known room names. */
  GS_FOR_EACH_ROOM (game, room)
    {
      const scr_char *name;

      name = prop_get_indexed_string (bundle, "Rooms", room, "Short");
      std::vector<scr_char> compressed (strlen (name) + 1);
      run_squeeze_spaces (name, compressed.data ());
      if (scr_strcasecmp (compressed.data (), argument) == 0)
        break;
    }
  if (room == gs_room_count (game))
    return FALSE;

  /* Select the first dynamic object standing on the room's floor. */
  GS_FOR_EACH_OBJECT (game, object)
    {
      scr_bool bstatic;

      bstatic = prop_get_indexed_boolean (bundle, "Objects", object, "Static");
      if (!bstatic && obj_directly_in_room (game, object, room))
        break;
    }
  if (object == gs_object_count (game))
    return FALSE;

  /* Set this object reference, unambiguously, as if %object% match. */
  gs_clear_object_references (game);
  game->object_references[object] = TRUE;
  var_set_ref_object (vars, object);

  return TRUE;
}


/*
 * Cached per-task command patterns.
 *
 * run_match_task_commands() is called for every task on every player command,
 * and before caching it re-read the task's (Reverse)Command pattern strings
 * from the bundle on each attempt.  The patterns are immutable once a game
 * is loaded, and prop_get_string() returns stable pointers into the bundle,
 * so remember each task's pattern list (per direction) the first time it is
 * walked; the walk itself goes through the same fatal-checking prop_get_*
 * wrappers the uncached code used.  The cache tracks a single game;
 * gs_destroy() calls run_forget_game().
 */
/*
 * What run_match_task_commands() reads off each pattern before and after
 * matching it, worked out once with the pattern.  refs is
 * run_pattern_references()'s answer, or -1 when that answer hangs on a
 * marker naming one of the game's own variables and has to be asked live.
 * compiled is the pattern parsed for uip_match_pattern(), on first use.
 */
typedef struct
{
  scr_int first;                  /* strspn (pattern, WHITESPACE) */
  scr_int refs;
  scr_bool wild, group;           /* holds '*'; holds '[' or '{' */
  scr_bool has_object, has_character;
  scr_uip_patternref_t compiled;
} scr_pattern_facts_t;

typedef struct
{
  scr_bool known[2];                          /* indexed [forwards] */
  std::vector<const scr_char *> patterns[2];
  std::vector<scr_pattern_facts_t> facts[2];  /* one per patterns[] entry */
  /* A command whose markers were lower-cased lives here, and patterns[]
     points into it.  See run_lower_command_markers(). */
  std::vector<std::string> rewritten[2];
  /* run_task_passes_class_filter()'s take/put reading of the forwards
     patterns, as 1 + (take ? 1 : 0) + (put ? 2 : 0); 0 = not yet read. */
  scr_byte take_put_class;
} scr_task_commands_t;

static const void *run_cache_game = nullptr;

static std::vector<scr_task_commands_t> run_cache;

static scr_int run_pattern_references (scr_gameref_t game,
                                       const scr_char *pattern);

/*
 * run_clear_cache()
 *
 * Empty the cache, freeing the patterns it compiled.
 */
static void
run_clear_cache (void)
{
  for (scr_task_commands_t &cached : run_cache)
    for (std::vector<scr_pattern_facts_t> &facts : cached.facts)
      for (scr_pattern_facts_t &fact : facts)
        uip_destroy_pattern (fact.compiled);
  run_cache.clear ();
}

/*
 * run_lower_command_markers()
 *
 * Lower-case every `%...%` marker of a task command, in place; TRUE when
 * anything changed.
 *
 * A task command is already lower-case by the time any Runner's matcher sees
 * it -- the folding happens at load, not in checktask, which holds no LCase
 * above 44B0BA and reads the command straight out of the task record
 * (44AAA5, 44AF12).  p*VARREF said so sideways first: the variable arm builds
 * its marker as `"%" & Name & "%"` and looks for it with a binary-compare
 * InStr (44AF43), yet `wibb %NUM%` reached a variable named `num` while
 * `bork %Big%` AND `snib %big%` over a variable named `Big` reached nothing
 * -- one rule explains both, and it is that the command was folded and the
 * stored Name was not (var_get_command_number()).
 *
 * p*CASEREF then asked the four known markers directly
 * (runner_probes/caseref.run370.rtf, runner_probes/caseref.run380.rtf,
 * runner_probes/caseref.run390.txt, runner_probes/caseref.run400.txt,
 * 2026-09-20): tasks `frob %Object%`, `nurb %CHARACTER%`, `blip %Number%` and
 * `murg %TEXT%` beside their lower-case twins, fed `frob rock`, `nurb fay`,
 * `blip 7`, `murg quux`.  Every capitalised marker runs its task, in every
 * Runner that knows the marker at all (%character% and %number% from 3.90,
 * %text% at 4.00; below that the command is a literal either way and the
 * cells answer DontUnderstand on both sides).  So X-Files task 30, `Molest
 * *%Character%`, is a live %character% command and not the unknown-marker
 * literal this took it for.
 *
 * Only the markers are folded here, not the whole command.  Everything else
 * a command holds is already compared case-insensitively -- the equality
 * LCase()s both sides (44B0BA/44B0DA), checkwild and 4.0's wildcard matcher
 * fold too (p*CASEREF's `* Zag * GEM *` runs on `xxx zag xxx gem xxx`
 * everywhere, and did here before this) -- so the two models differ only
 * where a literal's case can still show, which is 4.0's NewParse group
 * compare (45D7FA/45D835, binary on both sides; see uip_set_binary_input()),
 * and no measurement separates them there.
 */
static scr_bool
run_lower_command_markers (std::string &command)
{
  scr_bool changed = FALSE;
  size_t at = 0;

  while ((at = command.find ('%', at)) != std::string::npos)
    {
      const size_t end = command.find ('%', at + 1);
      size_t index_;

      if (end == std::string::npos)
        break;
      for (index_ = at + 1; index_ < end; index_++)
        {
          const scr_char lowered = scr_tolower (command[index_]);

          if (lowered != command[index_])
            {
              command[index_] = lowered;
              changed = TRUE;
            }
        }
      at = end + 1;
    }
  return changed;
}

const std::vector<const scr_char *> &
run_task_command_patterns (scr_gameref_t game, scr_int task,
                           scr_bool forwards)
{
  const int direction = forwards ? 1 : 0;
  scr_task_commands_t *cached;

  if (run_cache_game != game)
    {
      run_clear_cache ();
      run_cache.assign (gs_task_count (game), scr_task_commands_t ());
      run_cache_game = game;
    }

  cached = &run_cache[task];
  if (!cached->known[direction])
    {
      const scr_prop_setref_t bundle = gs_get_bundle (game);
      scr_vartype_t vt_key[4];
      scr_int command_count, command;

      vt_key[0].string = "Tasks";
      vt_key[1].integer = task;
      vt_key[2].string = forwards ? "Command" : "ReverseCommand";
      command_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      cached->patterns[direction].reserve (command_count);
      for (command = 0; command < command_count; command++)
        {
          vt_key[3].integer = command;
          cached->patterns[direction]
              .push_back (prop_get_string (bundle, "S<-sisi", vt_key));
        }
      cached->rewritten[direction].resize (command_count);

      /* A command's markers reach the matcher lower-cased; see above. */
      for (command = 0; command < command_count; command++)
        {
          std::string lowered (cached->patterns[direction][command]);

          if (run_lower_command_markers (lowered))
            {
              cached->rewritten[direction][command] = lowered;
              cached->patterns[direction][command] =
                  cached->rewritten[direction][command].c_str ();
            }
        }

      cached->facts[direction].resize (command_count);
      for (command = 0; command < command_count; command++)
        {
          const scr_char *const pattern = cached->patterns[direction][command];
          scr_pattern_facts_t *const facts = &cached->facts[direction][command];

          facts->first = strspn (pattern, WHITESPACE);
          facts->refs = run_pattern_references (nullptr, pattern);
          facts->wild = strchr (pattern, WILDCARD_PATTERN) != nullptr;
          facts->group = strpbrk (pattern, "[{") != nullptr;
          facts->has_object = strstr (pattern, "%object%") != nullptr;
          facts->has_character = strstr (pattern, "%character%") != nullptr;
          facts->compiled = nullptr;
        }
      cached->known[direction] = TRUE;
    }
  return cached->patterns[direction];
}


/*
 * 3.7's permanent task-command rewrite -- not ported.
 *
 * 3.7 substitutes a task command's %object% INTO THE TASK, and the rewrite
 * outlives the turn.  run370's checktask (4332CA) copies the command aside
 * into var_D0, replaces %object% with the Short of each object its c() finds
 * in the line -- writing the result back over the command record at 433377,
 * so only the first match has a %object% left to replace -- and restores the
 * saved copy at 43342B when the command did not match.  On a MATCH the
 * original never goes back, and the task is left holding a command naming
 * one object for the rest of the session, with no %object% for a later turn
 * to bind.
 *
 * p37OBJREF, whose task 1 is `nurb %object%` printing "NURBED %object%.",
 * with three rocks sharing the Short "rock":
 *
 *   `nurb rock`, then `nurb rock` again -> "NURBED a red rock." and then
 *   "NURBED %object%.": the second line still matches the command, now
 *   literally "nurb rock", but binds nothing
 *   (runner_probes/objref.run370.rtf).
 *   `nurb gem` first instead -> "NURBED a gem.", and `nurb rock` after it
 *   is the library's "Which rock.  The big rock or the red rock?", the task
 *   being spelled "nurb gem" now (runner_probes/objref.run370.b.rtf).
 *   A line that does NOT match leaves the command alone: `frob rock` (the
 *   object catch-all) and `x coin` both keep `nurb coin` working as a task
 *   afterwards (runner_probes/objref.run370.e.rtf,
 *   runner_probes/objref.run370.c.rtf, 2026-09-20).
 *
 * run380 works on a copy throughout -- p38OBJREF answers every cell of both
 * feeds normally (runner_probes/objref.run380.rtf,
 * runner_probes/objref.run380.b.rtf) -- so this is 3.7's alone.  The rewrite
 * belongs to the loaded game rather than to game state: like the Runner's own
 * task record it is not undone by UNDO and not restored from a save.
 *
 * Deliberate deviation (2026-09-27): not ported.  After one `eat apple`, a
 * task `eat %object%` would answer nothing but the apple for the rest of the
 * session, which only ever takes an author's task away from the player.
 */

/*
 * run_forget_game()
 *
 * Drop any command pattern cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
run_forget_game (const void *game)
{
  if (run_cache_game == game)
    {
      run_cache_game = nullptr;
      run_clear_cache ();
    }
}

/*
 * run_pattern_names_verb()
 *
 * Helper for run_match_task_commands().  Return TRUE if the pattern
 * contains, as a standalone whitespace-delimited token, the first word of
 * the string passed in (case insensitive).
 */
static scr_bool
run_pattern_names_verb (const scr_char *pattern, const scr_char *string)
{
  static const scr_char *const PATTERN_WORD_BREAK = "\t\n\v\f\r *";
  const scr_char *verb;
  scr_int verb_length;

  /* Isolate the first word of the string; no word, no possible match. */
  verb = string + strspn (string, WHITESPACE);
  verb_length = strcspn (verb, WHITESPACE);
  if (verb_length == 0)
    return FALSE;

  /*
   * Scan pattern tokens for a case-insensitive whole-word match.  A '*' ends
   * a token as surely as a space does: authors write wildcards glued to their
   * words, and frustrated.taf's `*drop*tree*` names the verb "drop" just as
   * plainly as "* drop * tree *" would.  Treating the glued form as one
   * eleven-character token hid the verb, and the pattern -- being
   * wildcard-leading -- was then skipped by every library call, so the
   * 4.0 drop handler's look-up could not find it (run400
   * runner_transcripts/frustrated.txt gives it the line).
   */
  for (pattern += strspn (pattern, PATTERN_WORD_BREAK); *pattern != NUL;)
    {
      const scr_int token_length = strcspn (pattern, PATTERN_WORD_BREAK);

      if (token_length == verb_length
          && scr_strncasecmp (pattern, verb, verb_length) == 0)
        return TRUE;

      pattern += token_length;
      pattern += strspn (pattern, PATTERN_WORD_BREAK);
    }

  return FALSE;
}

/* Set while a 4.0 question continuation with a double space runs; see
 * run_match_task_commands(). */
scr_bool run_rerun_skips_tasks = FALSE;

/* 3.9 runs a prefix rerun as joined and compares task commands against it
   space for space; see run_match_task_commands(). */
scr_bool run_rerun_exact_spaces = FALSE;

/* Set while run_line_yields_to_library() asks whether a task command that
   names a verb takes the line, and whether that task's restrictions let it
   run; see there. */
scr_bool run_named_verb_only = FALSE;
scr_bool run_passing_only = FALSE;


/* How many times run_note_task_ran() has noted a task; a caller compares two
   readings to learn whether a pass ran anything, a task that had already run
   for the line included. */
scr_int run_task_runs_noted = 0;

void
run_note_task_ran (scr_gameref_t game, scr_int task)
{
  run_task_runs_noted++;
  run_co_task_claimed = TRUE;
  if (run_tasks_ran_this_command.size () != (size_t) gs_task_count (game))
    run_tasks_ran_this_command.assign (gs_task_count (game), FALSE);
  run_tasks_ran_this_command[task] = TRUE;
}

scr_bool
run_task_ran_this_command (scr_int task)
{
  return (size_t) task < run_tasks_ran_this_command.size ()
         && run_tasks_ran_this_command[task];
}

/*
 * run_npc_library_blocked()
 *
 * TRUE while the library's NPC rows are to be skipped: a 3.9+ game in which
 * some task has already run for the current line.  See the note in
 * run_try_command_table().
 */
scr_bool
run_any_task_ran_this_command (void)
{
  for (const scr_bool ran : run_tasks_ran_this_command)
    {
      if (ran)
        return TRUE;
    }
  return FALSE;
}


/*
 * Measured 2026-08-23 (make_39_doneprobe.py, run390): below 4.0 a game task
 * that matches the command element claims it even when it says nothing, so the
 * standard library verb that would otherwise answer never gets a turn.  `x
 * book` on a spent `* x * book *` task answers "You have already done that."
 * instead of the book's description, where `look at book` -- matching no task
 * -- prints the description; and a silent task that runs and prints nothing
 * leaves "I don't understand." rather than the library answer.  4.0 dropped
 * this: run400 falls through to the library examine in both cells.
 *
 * The spent half is PORTED (2026-09-13): run_spent_task_390() in
 * run_all_commands() makes the claim where run390's checktask makes it, ahead
 * of everything but the take/drop/inventory/put handlers and the NPC examine.
 * It costs the walkthroughs that used to win past a spent `*` task -- The
 * Long Journey Home stops at 5/90 in the Lair, exactly where run390 does --
 * and those goldens now hold the Runner's brick.
 *
 * The silent half is PORTED too (2026-09-20, 3.9 only): its clock went first
 * (2026-09-19, the line is not a turn) and now the claim, silent_task_390 in
 * run_all_commands() skipping the whole library block so the DontUnderstand
 * text comes out.  test/adrift4/notes/RUNNER_TESTS_TODO.md section 4 priced
 * that at 15 goldens and four lost walkthroughs, but that was before the
 * spent-task port had taken most of those cells: re-measured it costs six
 * goldens and no win -- alexis, alexis_worn_cube, lifesimulation, life and
 * everything change one line of text, the_hangover two turns -- and all six
 * rows then match their Runner transcripts exactly (but for the "Done."
 * that replaced the DontUnderstand text on 2026-10-15, a deliberate
 * deviation; see run_line_t::settle()).  3.7/3.8 are left alone:
 * checktask is the same routine there, but no probe has been run and no corpus
 * row asks.
 */

/*
 * scr_strict_reference_guard
 *
 * Turns strict %object% matching on for the lifetime of the guard, and off
 * again however the scope is left.  match_case separates the two Runners that
 * bind strictly: 4.0 substitutes the name as authored, 3.90 lower-cases it.
 */
class scr_strict_reference_guard
{
public:
  scr_strict_reference_guard (scr_bool strict, scr_bool match_case)
    : strict_ (strict)
  {
    if (strict_)
      uip_set_strict_reference (TRUE, match_case);
  }

  ~scr_strict_reference_guard ()
  {
    if (strict_)
      uip_set_strict_reference (FALSE, FALSE);
  }

  scr_strict_reference_guard (const scr_strict_reference_guard &) = delete;
  scr_strict_reference_guard &
  operator= (const scr_strict_reference_guard &) = delete;

private:
  const scr_bool strict_;
};


/*
 * run_line_names_word()
 *
 * TRUE when run380's c() (429048) finds WORD in LINE, which must already be
 * lowercased: the word starts the line or follows a space, and ends the line
 * or is followed by a space or a comma.  Only the first occurrence that
 * starts a word is looked at, exactly as c() does.
 */
static scr_bool
run_line_names_word (const std::string &line, const scr_char *name)
{
  std::string word (name ? name : "");
  size_t at = 0;

  if (word.empty ())
    return FALSE;
  for (char &c : word)
    c = scr_tolower (c);
  while ((at = line.find (word, at)) != std::string::npos)
    {
      const size_t end = at + word.size ();

      if (at == 0 || line[at - 1] == ' ')
        return end == line.size () || line[end] == ' ' || line[end] == ',';
      at++;
    }
  return FALSE;
}


/* Put WITH in place of every FIND in TEXT, the way VB's Replace() with a
   count of -1 does. */
static void
run_replace_all (std::string &text, const scr_char *find,
                 const scr_char *with)
{
  const size_t length = strlen (find);
  size_t at = 0;

  while ((at = text.find (find, at)) != std::string::npos)
    {
      text.replace (at, length, with);
      at += strlen (with);
    }
}


/*
 * run_val()
 *
 * VB's Val() over a token with no spaces in it: an optional sign, digits,
 * an optional fractional part, and everything from the first character that
 * is none of those thrown away.  Val("007") is 7 and Val("3x") is 3, which
 * is the whole reason a line and the command it just spelled can disagree.
 */
static double
run_val (const std::string &token)
{
  double sign = 1.0, value = 0.0, scale = 0.1;
  size_t at = 0;

  if (at < token.size () && (token[at] == '-' || token[at] == '+'))
    sign = token[at++] == '-' ? -1.0 : 1.0;
  for (; at < token.size () && isdigit ((unsigned char) token[at]); at++)
    value = value * 10.0 + (token[at] - '0');
  if (at < token.size () && token[at] == '.')
    for (at++; at < token.size () && isdigit ((unsigned char) token[at]); at++)
      {
        value += (token[at] - '0') * scale;
        scale *= 0.1;
      }
  return sign * value;
}


/*
 * run_line_number()
 *
 * numintext() (run390 4332C8, run400 Proc_19_52_453A48), and it is nobody's
 * idea of a parser.  Take the LOWEST position in the line at which any of
 * the characters "0".."9" occurs -- a 0..9 loop over InStr keeping the
 * minimum, 433112-43317E -- collect the non-space run STARTING AT THAT
 * CHARACTER (433198-433208), prepend "-" when the character before it is one
 * (433227-433265), and take Val() of that (433268), clamped to a signed
 * 32-bit range and rounded by CLng (4332B1).  Return FALSE, leaving the
 * stored number alone, when the line holds no digit at all: the whole body
 * including the write to MemVar_4681AC sits inside `If var_8A < 32000`
 * (433183-4332BB), so `zork five apples` does not clear what the line
 * before it set.
 */
static scr_bool
run_line_number (const std::string &line, scr_int *number)
{
  size_t at = std::string::npos, end;
  std::string token;
  double value;
  scr_int digit;

  for (digit = 0; digit < 10; digit++)
    {
      const size_t found = line.find ((scr_char) ('0' + digit));

      if (found != std::string::npos
          && (at == std::string::npos || found < at))
        at = found;
    }
  if (at == std::string::npos)
    return FALSE;

  for (end = at; end < line.size () && line[end] != ' '; end++)
    ;
  token = line.substr (at, end - at);
  if (at > 0 && line[at - 1] == '-')
    token.insert (token.begin (), '-');

  value = run_val (token);
  if (value > 2147483647.0)
    value = 2147483647.0;
  if (value < -2147483647.0)
    value = -2147483647.0;
  *number = (scr_int) nearbyint (value);
  return TRUE;
}


/*
 * run_line_number_word()
 *
 * numintext2() (run390 42946C, run400 Proc_19_53_444458): `For n = 0 To
 * &H14`, `c(int2text(n), "")` -- c() with an empty second argument searches
 * the input line -- and no early exit, so it is looking for a number SPELLED
 * OUT, whole-word, and the LAST n whose word is in the line wins.
 */
static scr_bool
run_line_number_word (const std::string &line, scr_int *number)
{
  scr_bool is_found = FALSE;
  scr_int n;

  for (n = 0; n <= 20; n++)
    {
      if (run_line_names_word (line, var_number_word (n)))
        {
          *number = n;
          is_found = TRUE;
        }
    }
  return is_found;
}


/*
 * run_substitute_number_references()
 *
 * Put in place of a task command's %number% and %t_number% the DIGITS of the
 * number the line names, and store that number as the game's referenced one.
 *
 * checktask does this straight after the %object% and %character% walks and
 * before it tests the command at all (run390 44ADDF and 44AE8B; run400's
 * substituter Proc_19_36_45F268 is the same routine in mdlSpreadTheLoad,
 * %number% at 45F03C and %t_number% at 45F0AA).  Two things follow:
 *
 *  - %number% is not a positional wildcard: it is a SUBSTITUTION, like
 *    %object%, so a '*' command carrying one is decided pre-4.0 by checkwild,
 *    whose pieces are found by InStr anywhere in the line and in any order.
 *    p39NUMREF's task 2 is "* zog * %number% *", and run390 runs it on `blip
 *    7 zog` and answers `blip 9 zog 3 blip` with "NUM2 [9]." -- numintext
 *    took the 9 because it is the leftmost digit, not because the pattern
 *    reached it.  run400 refuses both, its own matcher cutting the line as it
 *    goes (runner_probes/numref.run390.txt, runner_probes/numref.run400.txt,
 *    2026-09-20).
 *  - the substituted command has to equal the line that produced it, and
 *    Val() makes that fail: `zork 007 apples` spells "zork 7 apples" and
 *    `zork 3x apples` spells "zork 3 apples", so both are refused at 3.90
 *    and at 4.00 while a positional matcher takes them.
 *
 * Both markers are replaced with digits -- Format(MemVar_4681AC) at 44AE15
 * and CStr(MemVar_4681AC) at 44AEBE -- although the OUTPUT filter spells
 * %t_number% out (45B21B, int2text).  So the words-vs-digits split is real
 * everywhere except here, and a %t_number% command can match nothing: the
 * line says "five" and the command it is compared against says "5"
 * (p*NUMREF task 3 "frob %t_number%" is refused for `frob five` and for
 * `frob 5` in all four Runners).
 *
 * Both write the same Long, and they write it whether or not the task goes
 * on to match: `nurb 5 blip` runs nothing and the next `zap` still prints
 * "ZAP [5] [five].".
 */
static void
run_substitute_number_references (scr_gameref_t game,
                                  const std::string &lowered,
                                  std::string &literal)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_char digits[32];
  scr_int number;

  if (literal.find ("%number%") != std::string::npos
      && run_line_number (lowered, &number))
    {
      var_set_ref_number (vars, number);
      snprintf (digits, sizeof (digits), "%ld", number);
      run_replace_all (literal, "%number%", digits);
    }
  if (literal.find ("%t_number%") != std::string::npos
      && run_line_number_word (lowered, &number))
    {
      var_set_ref_number (vars, number);
      snprintf (digits, sizeof (digits), "%ld", number);
      run_replace_all (literal, "%t_number%", digits);
    }
}


/*
 * run_substitute_variable_references()
 *
 * Put in place of a task command's %<user variable>% the variable's value.
 *
 * The last arm of checktask's substitution, after the numbers: a loop over
 * the whole variable array, each InStr'ing `"%" & Name & "%"` in the command
 * and Replacing it with the value (run390 44AF07-44AFDA, run400
 * 45F105-45F1B3).  What a marker is worth, which markers reach a variable at
 * all and why "%t_<name>%" reaches none are all var_get_command_number().
 */
static void
run_substitute_variable_references (scr_gameref_t game, std::string &literal)
{
  const scr_var_setref_t vars = gs_get_vars (game);
  size_t at;

  for (at = 0; (at = literal.find ('%', at)) != std::string::npos; )
    {
      const size_t end = literal.find ('%', at + 1);
      scr_char digits[32];
      scr_int number;

      if (end == std::string::npos)
        break;
      if (uip_get_lenient_tasks ())
        {
          std::string text;

          /* Deliberate deviation; see var_get_command_text(). */
          if (!var_get_command_text (vars,
                                     literal.substr (at + 1,
                                                     end - at - 1).c_str (),
                                     text))
            {
              at = end + 1;
              continue;
            }
          literal.replace (at, end - at + 1, text);
          at += text.size ();
          continue;
        }
      if (!var_get_command_number (vars,
                                   literal.substr (at + 1,
                                                   end - at - 1).c_str (),
                                   &number))
        {
          at = end + 1;
          continue;
        }
      snprintf (digits, sizeof (digits), "%ld", number);
      literal.replace (at, end - at + 1, digits);
      at += strlen (digits);
    }
}


/*
 * run_pattern_references()
 *
 * The set of %reference% markers a task command carries, as a bitmask, or
 * RUN_REF_OTHER for a marker that is none of the four and reaches no
 * variable of the game's own either.  With a NULL game, -1 where the answer
 * would depend on the game's variables.
 */
enum
{
  RUN_REF_OBJECT = 1, RUN_REF_CHARACTER = 2,
  RUN_REF_NUMBER = 4, RUN_REF_TEXT = 8, RUN_REF_VARIABLE = 16,
  RUN_REF_OTHER = 32
};

static scr_int
run_pattern_references (scr_gameref_t game, const scr_char *pattern)
{
  scr_int found = 0;
  const scr_char *at;

  for (at = strchr (pattern, '%'); at; )
    {
      const scr_char *const end = strchr (at + 1, '%');
      const size_t length = end ? end - at + 1 : 0;
      scr_int number;

#define RUN_TOKEN_IS(literal) \
      (length == sizeof (literal) - 1 && memcmp (at, literal, length) == 0)
      if (RUN_TOKEN_IS ("%object%"))
        found |= RUN_REF_OBJECT;
      else if (RUN_TOKEN_IS ("%character%"))
        found |= RUN_REF_CHARACTER;
      else if (RUN_TOKEN_IS ("%number%") || RUN_TOKEN_IS ("%t_number%"))
        found |= RUN_REF_NUMBER;
      else if (RUN_TOKEN_IS ("%text%"))
        found |= RUN_REF_TEXT;
      else if (length > 2 && !game)
        return -1;
      else if (length > 2
               && var_get_command_number (gs_get_vars (game),
                                          std::string (at + 1, length - 2)
                                              .c_str (), &number))
        found |= RUN_REF_VARIABLE;
      else
        return found | RUN_REF_OTHER;
#undef RUN_TOKEN_IS
      at = strchr (end + 1, '%');
    }
  return found;
}


/*
 * run_pre400_substitute_references()
 *
 * Put in place of a pre-4.0 task command's %object% and %character% the name
 * the LINE names, and say which entity the command has thereby bound.
 * Returns FALSE when the command holds a reference this cannot substitute,
 * in which case there is nothing to test and the tree's answer stands.
 *
 * checktask (run390 44AA5A, run380 43B78B, run370 4332CA) does this before
 * it tests the command at all, and the walk is over the whole object array
 * with no break: every hit stores its index in MemVar_4681A8, so the LAST hit
 * is the reference the task's text expands, while Replace() only ever fires
 * once -- the FIRST hit spells the literal that is then compared.  The two
 * come apart whenever a line names two namesakes, and p39WILDREF answers
 * `blip zog blip rock blip gem blip` against "* zog * %object% *" with "WILD1
 * a gem.": the rock (index 0) made the string that matched and the gem (index
 * 1) is what %object% expands to (runner_probes/wildref.run390.txt,
 * 2026-09-20).
 *
 * 3.90 tests the Short (44AAD6) and then the Aliases (44AB65) of the SAME
 * object before it moves on to the next -- one `For var_138 ... Next
 * var_138`, the Next at 44ABE5 -- and the Alias arm substitutes the ALIAS,
 * .global_8, not the Short.  So the order is obj0.Short, obj0.Alias,
 * obj1.Short, obj1.Alias, ..., and a line naming one object's Short and an
 * earlier object's Alias is spelled with the ALIAS and expands to the
 * later object.  p39TEXTSRC (make_textsrcprobe.py,
 * runner_probes/textsrc.run390.txt, 2026-09-20) answers both `zug rock stone`
 * and `zug stone rock` against "* zug * %object% *" with "WILD [a rock].": the
 * gem is index 0 and binds through its alias "stone", spelling "* zug * stone
 * *", and the rock binds after it.  Both gates are the seen byte .global_44
 * and nothing else -- there is no scope test, so an absent object binds.
 *
 * 4.00 is the one that walks twice, every Short and then every Alias:
 * run400 answers the same two lines with "WILD [a gem]." -- the rock's
 * Short spells the string and the gem's alias is the last hit either way.
 * That path is uip's, not this one.
 *
 * 3.7/3.8 know only the Short and gate on nothing: p38WILDREF binds the coin
 * two rooms away and unseen, and p38TEXTSRC answers `blip stone` against
 * "blip %object%" with the object catch-all, the alias reaching nothing.
 * %character% arrives at 3.90 (44AD2A), by Name, with no gate whatever: the
 * King binds from the Cave he is not in.
 *
 * %number% and %t_number% follow, at 3.90 and not below -- see
 * run_substitute_number_references().  A marker this version has never heard
 * of is left standing, which is a substitution of a kind: below 3.90
 * "%number%" is a literal the player has to type, exactly as "%character%"
 * is below 3.80 (run370 and run380 hold no "%number%", "%t_number%" or
 * "%text%" string anywhere), and "%text%" is one below 4.00.
 *
 * The game's own variables come last (44AF07) -- see
 * run_substitute_variable_references().
 *
 * Not emulated, and measured 2026-09-20 (p39TEXTSRC,
 * runner_probes/textsrc.run390.txt):
 * the string 3.90 searches is not `line` at all but MemVar_468224, a
 * SNAPSHOT of the line taken at the end of the synonym pass (45F20F), so
 * none of generaltasks' own rewrites below it -- "everything"->"all"
 * (45F225), "slap"->"hit" (45F246), "except"/"apart from"->"but" (45F267,
 * 45F288), the `with ` history prepend (45F2AF) -- reaches the walk, while
 * the string the command is TESTED against is the rewritten one.  Task `zog
 * %object%` with objects named `hit` and `slap` is refused by `zog slap`:
 * the walk binds the slap and spells "zog slap", the test is against "zog
 * hit", and the library answers instead ("You hit the hit, but nothing
 * happens.").  The second pair of loops at 44ABFE/44AC98 does repeat the
 * walk against checktask's own `text` argument, but its guard is
 * `MemVar_4681A8 = &HFF` -- the per-TURN referenced object, cleared once at
 * 45EC68 -- so the first task command whose walk binds anything consumes it
 * for every later task on the same line.  `nurb except` with an object
 * named `but` is therefore refused by task `nurb %object%`: task 1's
 * `blip %object%` fell back first, bound the but and spelled "blip but".
 * Scarier keeps one string and no fallback, so it takes both lines.
 *
 * A command holding a marker that is neither a known reference nor a
 * variable of the game's own is still handed back.
 */
static scr_bool
run_pre400_substitute_references (scr_gameref_t game, const scr_char *line,
                                  const scr_char *pattern,
                                  std::string &literal,
                                  scr_int *object, scr_int *character)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = run_get_version (bundle);

  literal = pattern;
  *object = *character = -1;

  /* A command with no marker has nothing to substitute. */
  if (!strchr (pattern, '%'))
    return TRUE;

  std::string lowered (line);
  for (char &c : lowered)
    c = scr_tolower (c);

  if (run_pattern_references (game, pattern) & RUN_REF_OTHER)
    return FALSE;

  if (literal.find ("%object%") != std::string::npos)
    {
      scr_int index;

      GS_FOR_EACH_OBJECT (game, index)
        {
          scr_int alias, alias_count;
          scr_vartype_t vt_key[4];
          const scr_char *name;

          if (version >= TAF_VERSION_390 && !gs_object_seen (game, index))
            continue;
          if (version == TAF_VERSION_390 && obj_is_static (game, index))
            continue;

          /* The object's Short first ... */
          name = prop_get_indexed_string (bundle, "Objects", index, "Short");
          if (run_line_names_word (lowered, name))
            {
              *object = index;
              if (literal.find ("%object%") != std::string::npos)
                run_replace_all (literal, "%object%", name);
            }
          if (version < TAF_VERSION_390)
            continue;

          /* ... then, in the same turn of the same loop, its Aliases, which
           * substitute the ALIAS and not the Short.
           */
          vt_key[0].string = "Objects";
          vt_key[1].integer = index;
          vt_key[2].string = "Alias";
          alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
          for (alias = 0; alias < alias_count; alias++)
            {
              vt_key[3].integer = alias;
              name = prop_get_string (bundle, "S<-sisi", vt_key);
              if (!run_line_names_word (lowered, name))
                continue;
              *object = index;
              if (literal.find ("%object%") != std::string::npos)
                run_replace_all (literal, "%object%", name);
              break;
            }
        }
    }

  if (version >= TAF_VERSION_390
      && literal.find ("%character%") != std::string::npos)
    {
      scr_int npc;

      GS_FOR_EACH_NPC (game, npc)
        {
          const scr_char *name =
              prop_get_indexed_string (bundle, "NPCs", npc, "Name");

          if (!run_line_names_word (lowered, name))
            continue;
          *character = npc;
          if (literal.find ("%character%") != std::string::npos)
            run_replace_all (literal, "%character%", name);
        }
    }

  if (version >= TAF_VERSION_390)
    {
      run_substitute_number_references (game, lowered, literal);
      run_substitute_variable_references (game, literal);
    }

  return TRUE;
}


/*
 * One task command being tried against the input line: the command, what
 * run_task_command_patterns() cached about it, and the answer so far.
 * run_match_task_commands() runs the stages below in order.
 *
 * `lenient` is a deliberate deviation: on a line no task matches the
 * Runner's way, a command is matched leniently -- the tolerant tree, none of
 * the Runner's substitute-and-compare below; see
 * run_line_matches_task_strictly().  A %character% command stays strict:
 * thenightmoon's `attack giant rat with longsword` would otherwise run task
 * 5's warning for striking a friend, which no Runner ever shows, and lose
 * the game.
 */
struct run_command_match_t
{
  scr_gameref_t game;
  scr_int task;
  const scr_char *string;
  scr_int version;
  const scr_char *pattern;
  scr_pattern_facts_t &facts;
  scr_int first;
  scr_bool lenient;

  /* The answer, and the line that gave it: string or run_dispatch_input. */
  scr_bool is_matched;
  const scr_char *matched_input;

  /* Set by classify_references(). */
  scr_bool wild, group;
  scr_int refs;
  scr_bool numeric, variable, literal_ref;

  run_command_match_t (scr_gameref_t game_, scr_int task_,
                       const scr_char *string_, scr_int version_,
                       const scr_char *pattern_, scr_pattern_facts_t &facts_)
    : game (game_), task (task_), string (string_), version (version_),
      pattern (pattern_), facts (facts_), first (facts_.first),
      lenient (uip_get_lenient_tasks () && !facts_.has_character),
      is_matched (FALSE), matched_input (string_),
      wild (FALSE), group (FALSE), refs (0),
      numeric (FALSE), variable (FALSE), literal_ref (FALSE) {}

  void tree_match (scr_bool is_library);
  void classify_references (void);
  void recheck_400 (void);
  void recheck_pre400 (void);
  void bind_unmatched_object_390 (void);
#ifdef SCARIER_DUMP_TOOLS
  void trace (void) const;
#endif
};

/* The tolerant tree matcher's answer, which the later stages overrule. */
void
run_command_match_t::tree_match (scr_bool is_library)
{
  /*
   * Make a special case of library calls and commands that begin with a
   * wildcard.  Probed live in run400 (2026-08-22, probes pPREC and
   * pPREC2): a wildcard-leading pattern
   * with failing messaged restrictions blocks the system take only when
   * the pattern explicitly names a verb -- either the library's
   * canonical verb ("* get * tent *" blocks both "get tent" and "take
   * tent") or the verb the player actually typed ("* take * tent *"
   * blocks "take tent" but NOT "get tent").  A verb-less "* ball *"
   * pattern never blocks: the system take wins even though the same
   * pattern with passing restrictions would run as a game command.
   * Failing restrictions with an empty message fall through to the
   * system command silently in every case (that drops out of the
   * loudly-restricted machinery here without special handling).
   *
   * The library constructs its match string with the canonical verb
   * ("get <object>"), so a pattern naming only the typed verb cannot
   * match it; for those, retry the match against the player's actual
   * input, stashed by run_all_commands().
   */
  /*
   * A pre-4.0 '*' command with no marker in it is checkwild's alone -- see
   * recheck_pre400() -- and the tree's answer for it is never read.  The
   * library's wildcard-leading case below still chooses the line.
   */
  if (version < TAF_VERSION_400 && !lenient && facts.wild
      && pattern[first] != SPECIAL_PATTERN
      && !(is_library && pattern[first] == WILDCARD_PATTERN)
      && !strchr (pattern, '%'))
    return;

  if (lenient && version >= TAF_VERSION_390)
    uip_set_strict_reference (FALSE, FALSE);

  if (pattern[first] != SPECIAL_PATTERN && !facts.compiled)
    facts.compiled = uip_compile_pattern (pattern);
  if (pattern[first] == SPECIAL_PATTERN)
    ;
  else if (is_library && pattern[first] == WILDCARD_PATTERN)
    {
      if (run_pattern_names_verb (pattern, string))
        is_matched = uip_match_pattern (facts.compiled, string, game);
      if (!is_matched && run_dispatch_input != nullptr
          && run_pattern_names_verb (pattern, run_dispatch_input))
        {
          is_matched = uip_match_pattern (facts.compiled,
                                          run_dispatch_input, game);
          matched_input = run_dispatch_input;
        }
    }
  else
    is_matched = uip_match_pattern (facts.compiled, string, game);

  if (lenient && version >= TAF_VERSION_390)
    uip_set_strict_reference (TRUE, version >= TAF_VERSION_400);
}

void
run_command_match_t::classify_references (void)
{
  wild = facts.wild;
  group = facts.group;

  /*
   * Which %reference% markers the command carries decides who answers
   * it.  %number%, %t_number% and a marker naming one of the game's own
   * variables are a SUBSTITUTION in every Runner that knows them, never
   * a positional wildcard, so the tree's answer is beside the point:
   * see run_substitute_number_references() and
   * run_substitute_variable_references().  Below 3.90 numbers are not
   * markers at all, nor is %text% below 4.00, and a command carrying
   * one has to be typed with the percent signs in it; below 3.90 the
   * file has no Variables section either, so nothing there can name a
   * variable and the question does not arise.
   */
  refs = facts.refs >= 0
         ? facts.refs
         : run_pattern_references (game, pattern);
  numeric = (refs & RUN_REF_NUMBER) && version >= TAF_VERSION_390;
  variable = (refs & RUN_REF_VARIABLE) != 0;
  literal_ref = ((refs & RUN_REF_NUMBER) && version < TAF_VERSION_390)
                || ((refs & RUN_REF_TEXT) && version < TAF_VERSION_400);
}

void
run_command_match_t::recheck_400 (void)
{
  /*
   * 4.0's command loop (45D9FC-45DBA4) tests a command three ways, in
   * this order, stopping at the first that takes:
   *
   *   1. `LCase(line) = LCase(cmd)` (45DA51) -- plain equality;
   *   2. `If InStr(cmd, "*") > 0` (45DA8C), Proc_19_50_457D68 -- the
   *      non-backtracking wildcard matcher, uip_wildcard_match_400();
   *   3. `If ([ And ]) Or ({ And })` (45DADB-45DB42), Proc_9_4_45D940 --
   *      NewParse, the group expander, which is the tree here.
   *
   * So a '*' command is answered by the wildcard matcher, which refuses
   * some lines the tree takes (The Town of Azra `buy rawhide armor`
   * against "buy *** *rawhide armor*"), and a GROUP is expanded LAST --
   * after the whole command has been tried as a literal, brackets and
   * all, and after a '*' in it has been matched literally too.
   *
   * Measured on p4GROUP (make_groupprobe.py,
   * runner_probes/group.run400.txt and runner_probes/group.run400.b.txt,
   * 2026-09-20).  Task 1 is "zog [rock/gem]": run400 takes
   * `zog rock` and `zog gem` by step 3 AND `zog [rock/gem]` by step 1,
   * while `zog [gem/rock]`, `zog [rock/gem ]` and `zog rock/gem` are
   * refused -- equality on the raw pattern, no normalising beyond the
   * case fold and the input's own space collapse (`zog  [rock/gem]`
   * takes).  Task 3 is "* blip [red/blue] *": `xxx blip [red/blue] yyy`
   * runs it by step 2 and `xxx blip red yyy` matches nothing, so step 3
   * never expands a group in a '*' command.  run390 answers the same
   * feed identically (runner_probes/group.run390.b.txt) -- its checktask
   * has the same equality-then-checkwild shape with step 3 missing.
   *
   * Only steps 1 and 2 are done here; step 3 is the tree, which has
   * already run.  A command with a %reference% belongs to 458E6C /
   * 46918C, which substitute and then repeat these same three tests.
   *
   * One cell is knowingly left: the Runner collapses a KEYBOARD line's
   * spaces before all three tests, so `zog  [rock/gem]` runs task 1
   * there and not here.  It does not collapse a line it builds itself,
   * which is what run_rerun_skips_tasks above is for, and separating
   * the two would buy one cell that needs a typed line carrying both a
   * double space and a bracket.
   */
  if (version >= TAF_VERSION_400 && !lenient
      && (refs & ~(RUN_REF_NUMBER | RUN_REF_VARIABLE)) == 0)
    {
      if (numeric || variable)
        {
          /*
           * A 4.0 command whose only markers are numbers is substituted
           * first (Proc_19_36_45F268) and then put through the same
           * three tests, on the spelled-out command.  Step 3 is
           * uip_match(), here only as the group expander, and it is
           * reached only when step 2's `If InStr(cmd, "*") > 0` was
           * false, so a group inside a '*' command is still never
           * expanded.  humbug's task 123 is why step 3 has to stay:
           * "[push/press] {button} [%number%/%t_number%]" is a group
           * whose alternatives ARE the two markers, and `push button 2`
           * reaches NewParse spelled "[push/press] {button}
           * [2/%t_number%]".
           *
           * A marker the line did not name is left standing and the
           * tree can be trusted with it: "%t_number%" is a plain word to
           * the tokenizer, and a leftover "%number%" means the line
           * holds no digit at all -- numintext takes the leftmost one,
           * so there is no third case -- and uip_match_number() then
           * refuses wherever the pattern puts it.
           */
          std::string lowered (matched_input), literal;

          for (char &c : lowered)
            c = scr_tolower (c);
          literal = pattern;
          if (numeric)
            run_substitute_number_references (game, lowered, literal);
          run_substitute_variable_references (game, literal);

          is_matched =
              scr_strcasecmp (literal.c_str (), matched_input) == 0
              || (wild
                  ? uip_wildcard_match_400 (literal.c_str (),
                                            matched_input)
                  : (group && uip_match (literal.c_str (),
                                         matched_input, game)));
        }
      else if (is_matched && wild)
        is_matched = uip_wildcard_match_400 (pattern, matched_input);
      else if (!is_matched && group)
        is_matched = scr_strcasecmp (pattern, matched_input) == 0
                     || (wild && uip_wildcard_match_400 (pattern,
                                                         matched_input));
    }
}

void
run_command_match_t::recheck_pre400 (void)
{
  /*
   * 3.7-3.9 send a command with a '*' to checkwild instead, which
   * compares the pieces literally; see uip_wildcard_match_pre400().
   * checkwild is the whole test there and not a veto on the tree:
   * run390's checktask compares a command with no '*' for equality
   * (44B0E2) and, when InStr(cmd, "*") > 0 (44B10D), takes checkwild's
   * answer as the match flag, with nothing else consulted.  So a line
   * the tree refuses runs the task all the same, and the two ways that
   * happens are both measured on p*WILDORD (make_wildorderprobe.py,
   * runner_probes/wildord.run370.rtf, runner_probes/wildord.run380.rtf,
   * runner_probes/wildord.run390.txt, 2026-09-20):
   *
   * - Each middle piece is looked for with InStr over the WHOLE line
   *   and the line is never cut, so ORDER IS FREE.  Task "* king *
   *   rose *" runs on `blip rose blip king blip` in all three
   *   pre-4.0 Runners; 4.0's own matcher cuts, so it refuses there
   *   (runner_probes/wildord.run400.txt) and the tree's answer is
   *   right.
   * - Nothing being consumed, one occurrence satisfies a piece twice:
   *   "* zog * zog *" runs on `a zog b`, again pre-4.0 only.
   *
   * Two of checkwild's refusals matter the other way round:
   *
   * - The text after the last '*' must equal the end of the line, so a
   *   command ending in a stray space matches nothing.  Alchemist (3.90)
   *   task 114's only command taking `give rose to king` is "* rose *
   *   king ", and run390 answers the line with the library's "Rudolph
   *   II. doesn't seem interested in the rose." (runner_transcripts/
   *   alchemist.txt T300).
   * - run380/run370 never pad the line, so "throw %object% *" needs
   *   something after the object.  Marooned (3.80) T53 `throw map` at the
   *   lagoon: run380 skips task 15 ("You toss it into the water and the
   *   shark darts for it") for task 45's "throw %object%" ("You throw it
   *   and it lands in the ocean", runner_transcripts/marooned.rtf).
   *
   * Before either test, checktask puts in place of the command's
   * %object% and %character% the name the line names -- see
   * run_pre400_substitute_references(), which is the whole reason a
   * reference-bearing command reaches checkwild at all.  A task command
   * whose reference the line does not name keeps the literal
   * "%object%" and so matches nothing: p39WILDREF answers `blip zog
   * blip` against "* zog * %object% *" with "I don't understand."
   */
  /*
   * The same substitution decides a pre-3.9 command with no '*' at all,
   * and there the comparison is plain equality: checktask rewrites its
   * copy of the command and tests it against the whole line.  So the
   * Short has to be typed bare.  p37CHREF and p38CHREF answer `nurb
   * rock` with task 2's "NURBED a big rock." and `nurb a big rock` and
   * `nurb big rock` -- the rock's Prefix is "a big" -- with the object
   * catch-all, "I don't understand what you want me to do with the big
   * rock." (runner_probes/chref.run370.b.rtf,
   * runner_probes/chref.run380.b.rtf, 2026-09-20).
   * That is 3.9's and 4.0's answer too, where the strict comparator in
   * uip_compare_candidate() already refuses the prefixed forms; the
   * tolerant tree matcher took all three here because nothing below 3.90
   * had ever turned the substitution on outside checkwild.
   */
  /*
   * A GROUP is 4.0 syntax, and below 4.0 it is not syntax at all: it is
   * punctuation the command has to be typed with.  checktask holds no
   * '[', ']', '{' or '}' literal anywhere -- not in run390 (body
   * 44AA5A-44B6E6), not in run380 (43B6A3-43C51D), not in run370
   * (433227-433E4A) -- so after the substitution there is nothing but
   * the equality at 44B0E2 and, for a '*' command, checkwild at 44B139
   * to route a group to.  Measured on p*GROUP (make_groupprobe.py,
   * runner_probes/group.run370.scrollback.txt,
   * runner_probes/group.run380.scrollback.txt,
   * runner_probes/group.run390.txt, 2026-09-20): all three Runners answer
   * `zog rock` and `zog gem` against task 1's "zog [rock/gem]" with the
   * object catch-all and run the task on `zog [rock/gem]`; `nurb rock` and
   * `nurb the rock` miss "nurb {the} rock" and `nurb {the} rock` takes it;
   * `frob` and `frob up` miss "frob {up}" and `frob {up}` takes it.  So a
   * group command joins the pre-4.0 arm rather than skipping it, and it
   * joins even with no '*' in it, where the test is plain equality.
   *
   * Zero corpus exposure, measured 2026-09-20 over every .taf in
   * games/ and downloaded/ (SCR_DUMP_TASKS, 405 games that load; the 14
   * pre-4.0 stragglers whose dump never fires scanned raw with
   * taf_pattern_scan.plaintext()): 6685 task commands carry a group and
   * every one of them is in a 4.00 file.  Below 4.00 the bracketed
   * lines are all ALR keys and display text ("[month=1]", "[talk=3]").
   */
  if (version < TAF_VERSION_400 && !lenient
      && (wild
          || group
          || numeric
          || variable
          || literal_ref
          || (is_matched && facts.has_object)
          /*
           * A 3.9 %character% command is decided the same way, not by
           * the tree's position: checktask's NPC walk (44AD48-44ADC2)
           * stores every NPC whose Name the line contains in
           * MemVar_4681AA but Replace()s only on the first, so the
           * LOWEST-indexed Name in the line spells the command.
           * thenightmoon T50 `attack skeleton guard with longsword`
           * contains NPC 0's Name "Guard" as well as NPC 11's "Skeleton
           * guard"; the command becomes "attack Guard with longsword",
           * equals nothing, and task 5 ("As you strike %character%,
           * they realise you are not as friendly...") never runs --
           * run390 hands the line to dobattle, "You hit the skeleton
           * with your longsword." (runner_transcripts/thenightmoon.txt).
           */
          || (is_matched && version >= TAF_VERSION_390
              && facts.has_character)))
    {
      std::string literal;
      scr_int ref_object = -1, ref_character = -1;
      const scr_bool has_marker = strchr (pattern, '%') != nullptr;
      const scr_bool checkable =
          !has_marker
          || run_pre400_substitute_references (game, matched_input, pattern,
                                               literal, &ref_object,
                                               &ref_character);
      const scr_char *const spelled = has_marker ? literal.c_str () : pattern;

      if (checkable && wild)
        is_matched = uip_wildcard_match_pre400
            (spelled, matched_input, version >= TAF_VERSION_390);
      else if (checkable)
        is_matched = scr_strcasecmp (spelled, matched_input) == 0;

      /*
       * The reference the task's text expands is the walk's, not the
       * tree's: the walk reads the line, the tree reads the pattern's
       * position, and where they disagree the Runner prints the walk's.
       */
      if (is_matched && checkable)
        {
          const scr_var_setref_t vars = gs_get_vars (game);

          if (ref_object >= 0)
            {
              gs_clear_object_references (game);
              game->object_references[ref_object] = TRUE;
              var_set_ref_object (vars, ref_object);
            }
          if (ref_character >= 0)
            {
              gs_clear_npc_references (game);
              game->npc_references[ref_character] = TRUE;
              var_set_ref_character (vars, ref_character);
            }
        }

      /* Deliberate deviation: 3.7 keeps the substitution it just made,
         for good, when the command matched on it -- see the note above
         run_forget_game() -- and Scarier does not. */
    }
}

void
run_command_match_t::bind_unmatched_object_390 (void)
{
  /*
   * run390's %object% walk stores every hit in the per-turn referenced
   * object MemVar_4681A8 (44AB0D, 44AB90) before checkwild or the
   * equality test is reached, so a %object% command that then fails to
   * match still leaves its object referenced for the rest of the turn.
   * thenightmoon's task 17 is literal `behead dark elf`, but its
   * restriction "the referenced object must be held" answers "You do not
   * have dead dark elf." there (run390x
   * runner_probes/thenightmoon.run390.probe2.txt), the body having been
   * bound by a %object% command checked earlier.
   */
  if (!is_matched && version == TAF_VERSION_390 && !lenient
      && facts.has_object)
    {
      std::string literal;
      scr_int ref_object, ref_character;

      if (run_pre400_substitute_references (game, matched_input, pattern,
                                            literal, &ref_object,
                                            &ref_character)
          && ref_object >= 0)
        var_set_ref_object (gs_get_vars (game), ref_object);
    }
}

#ifdef SCARIER_DUMP_TOOLS
void
run_command_match_t::trace (void) const
{
  SCR_TRACE_IF ("SCR_TRACE_MATCH", "MATCH task=%ld pattern=[%s] input=[%s]\n",
                task, pattern, string);
  {
    /* SCR_TRACE_SCOPE: flag a turn where scope decides which
     * object a %object% task command binds.  What the audit was
     * written for is now measured and ported -- 4.0 makes two
     * passes, present-and-seen then absent-but-seen, and takes the
     * FIRST in index order of whichever pass bound (p4OBJREF,
     * runner_probes/objref.run400.txt; uip_match_entity()) -- so this
     * is no longer a divergence report but a way of finding the corpus
     * turns that exercise the rule, and of watching the older Runners,
     * which have no scope test at all and let the LAST seen namesake
     * win wherever it stands.  SCOPE-MISS = nothing that matched is
     * present, so 4.0 bound on its second pass; SCOPE-BIND = the bound
     * object is absent while a present one also matched, which below
     * 4.0 is the Runner's answer and at 4.0 should no longer
     * happen. */
    if (SCR_TRACE_ON ("SCR_TRACE_SCOPE")
        && strstr (pattern, "%object%") != nullptr)
      {
        const scr_var_setref_t vars = gs_get_vars (game);
        scr_int object, present, matched, room, bound_present;

        room = gs_playerroom (game);
        present = matched = 0;
        bound_present = FALSE;
        GS_FOR_EACH_OBJECT (game, object)
          {
            if (!game->object_references[object])
              continue;
            matched++;
            if (obj_indirectly_in_room (game, object, room))
              {
                present++;
                if (object == var_get_ref_object (vars))
                  bound_present = TRUE;
              }
          }
        if (matched > 0 && present == 0)
          fprintf (stderr, "SCOPE-MISS task=%ld pattern=[%s]"
                   " input=[%s]\n", task, pattern, string);
        else if (present > 0 && !bound_present)
          fprintf (stderr, "SCOPE-BIND task=%ld pattern=[%s]"
                   " input=[%s]\n", task, pattern, string);
      }
  }
}
#endif


/*
 * run_match_task_commands()
 *
 * Helper for run_game_commands_common().
 *
 * Search task command for a match to the string passed in, returning TRUE
 * if a task command matches, FALSE otherwise.  Ordinary or reverse commands
 * are selected by 'forwards'.
 */
scr_bool
run_match_task_commands (scr_gameref_t game,
                         scr_int task, const scr_char *string,
                         scr_bool forwards, scr_bool is_library)
{
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, forwards);
  std::vector<scr_pattern_facts_t> &all_facts =
      run_cache[task].facts[forwards ? 1 : 0];
  const scr_int command_count = (scr_int) patterns.size ();
  scr_int command;
  scr_bool is_matched;

  /* 3.90 and up bind %object% strictly, and only 4.0 binds it case-
   * sensitively -- see the note above uip_compare_reference_strict().  Task
   * commands only; the library's own patterns keep the tolerant matcher. */
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_strict_reference_guard strict_reference
      (version >= TAF_VERSION_390, version >= TAF_VERSION_400);
  const scr_task_commands_guard task_commands;

  /*
   * 4.0 compares a task command against the line as it stands, so the two
   * spaces of a "...with?" continuation (`cut rope with ` & " " & `knife`)
   * match no task: run400 answers `cut rope`, `knife` with the library's
   * "You can't cut the rope with the knife." although the game has a task
   * `cut rope with knife` (runner_probes/withq.run400.txt).  Scarier's
   * matchers want single spaces, so run_player_input() runs such a rerun
   * collapsed and raises run_rerun_skips_tasks for it instead.
   */
  if (run_rerun_skips_tasks && !is_library)
    return FALSE;

  /*
   * 3.9 does not collapse the two spaces either, and its task matcher sees
   * them: p39WITHQ's `saw rope` / `knife` fires the task wired
   * `saw rope with  knife` and not the `saw rope with knife` before it
   * (run390x runner_probes/withq.run390.txt, 2026-09-25).  Scarier's matchers
   * let a double space through, so a pattern that has neither a double space
   * nor a wildcard to swallow it is skipped instead.  A double space in a
   * pattern is modelled only as far as that: such a pattern still matches
   * through the usual single-space matcher.
   */
  if (run_rerun_exact_spaces && !is_library)
    {
      scr_bool any = FALSE;

      for (command = 0; command < command_count && !any; command++)
        any = strstr (patterns[command], "  ") != nullptr
              || strpbrk (patterns[command], "*%") != nullptr;
      if (!any)
        return FALSE;
    }

  /* Iterate over commands, looking for patterns that match string. */
  is_matched = FALSE;
  for (command = 0; command < command_count; command++)
    {
      run_command_match_t match (game, task, string, version,
                                 patterns[command], all_facts[command]);

      if (run_named_verb_only && match.pattern[match.first] == WILDCARD_PATTERN)
        continue;

      match.tree_match (is_library);
      match.classify_references ();
      match.recheck_400 ();
      match.recheck_pre400 ();
      match.bind_unmatched_object_390 ();

      /* Stop searching if we find a match. */
      if (match.is_matched)
        {
#ifdef SCARIER_DUMP_TOOLS
          match.trace ();
#endif
          is_matched = TRUE;
          break;
        }
    }

  /* Return TRUE if we found a pattern match. */
  return is_matched;
}

/*
 * run_task_class_filter, run_set_task_class_filter()
 * run_task_passes_class_filter()
 *
 * The 4.0 Runner's task pre-matcher (Proc_19_35_453C50) takes a mode byte and
 * skips every task that Proc_21_57_4494FC rejects for it: mode 1 keeps only
 * tasks whose record byte 104 is set, mode 2 only those with byte 105 set,
 * mode 3 either.  Both bytes are computed once at LOAD (mdlSpreadTheLoad
 * @4931B5 and @493225): byte 104 when any of the task's command patterns
 * contains "get", "take" or "pick" as a substring, byte 105 when one contains
 * "drop", "leave" or "put", and a pattern that is exactly "*" sets both
 * (@493281-49328D).  The mode each caller passes is fixed: the put handler's
 * implicit-take gate and the get handler's refusal exits pre-match with 1,
 * the put and drop handlers' own look-ups (name_object's "Drop what?" and
 * "It is not clear" gates, the insides handler's canonical rebuild, "drop
 * all") with 2, and the insides handler's typed-line fallback with 0, no
 * filter at all.
 *
 * The consequence is visible in House (House.taf): task 459
 * "[put/place/drop] {some} [wood] {in/into/in to} {the} [fireplace/fire
 * place]" carries the put flag only, so "put wood in fireplace" with the
 * wood on the floor sails past the mode-1 take gate into "(Taking the wood
 * first)", where the old unfiltered pre-match would have hit the task and
 * suppressed the take.  The Runner's substring test is a binary-compare
 * InStr on the stored pattern; ours is case-insensitive, which only differs
 * for an author who capitalized a verb inside a pattern.
 */
static scr_int run_task_class_filter = 0;

/* A held-object put with no in/on split admits put-family tasks only,
   whatever the caller's mode; see lib_put_held_unsplit_400(). */
scr_bool run_put_class_only = FALSE;

void
run_set_task_class_filter (scr_int mode)
{
  assert (mode >= 0 && mode <= 3);
  run_task_class_filter = mode;
}

scr_bool
run_task_passes_class_filter (scr_gameref_t game, scr_int task)
{
  static const scr_char *const TAKE_WORDS[] = { "get", "take", "pick" };
  static const scr_char *const PUT_WORDS[] = { "drop", "leave", "put" };
  scr_bool is_take = FALSE, is_put = FALSE;
  const scr_int mode = run_task_class_filter != 0 ? run_task_class_filter
                       : run_put_class_only ? 2 : 0;
  scr_byte *take_put_class;

  if (mode == 0)
    return TRUE;

  /*
   * The reading depends only on the task's patterns, which never change, so
   * it is made once per task; the scan below is six case-insensitive
   * substring searches per pattern, and this filter runs for every task on
   * every take/put line.  run_task_command_patterns() has just (re)built the
   * cache entry for this game, so the slot is valid.
   */
  const std::vector<const scr_char *> &patterns =
      run_task_command_patterns (game, task, TRUE);
  take_put_class = &run_cache[task].take_put_class;
  if (*take_put_class == 0)
    {
      for (const scr_char *pattern : patterns)
        {
          size_t index_;

          if (strcmp (pattern, "*") == 0)
            is_take = is_put = TRUE;
          for (index_ = 0; index_ < 3; index_++)
            {
              if (run_instr (pattern, TAKE_WORDS[index_]) >= 0)
                is_take = TRUE;
              if (run_instr (pattern, PUT_WORDS[index_]) >= 0)
                is_put = TRUE;
            }
        }
      *take_put_class = 1 + (is_take ? 1 : 0) + (is_put ? 2 : 0);
    }
  is_take = (*take_put_class - 1) & 1;
  is_put = ((*take_put_class - 1) & 2) != 0;

  switch (mode)
    {
    case 1:
      return is_take;
    case 2:
      return is_put;
    default:
      return is_take || is_put;
    }
}


#ifdef SCARIER_DUMP_TOOLS
/* run_census_matches_task(): run_match_task_commands() for the census. */
scr_bool
run_census_matches_task (scr_gameref_t game, scr_int task,
                         const scr_char *string)
{
  return run_match_task_commands (game, task, string, TRUE, FALSE)
         || run_match_task_commands (game, task, string, FALSE, FALSE);
}
#endif

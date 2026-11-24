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
 * The lines the pre-4.0 Runners match by substring rather than by pattern:
 * the C-word tests, therest() and its arms, and the score/wait "anywhere"
 * lines.
 *
 * See scrunner.h for what the `run_` files share.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"
#include "scrunner.h"


/*
 * run_c_word_pre400()
 *
 * The pre-4.0 Runners' c(word) test: the FIRST case-insensitive InStr hit
 * that starts the line or follows a space decides, and it is true only if
 * the word ends there -- at the end of the line, a space or a comma, and at
 * 3.9 a period too.  A hit
 * inside a word is skipped, so `unblock` never holds `block`; a word-start
 * hit that runs on stops the search, so `lookout look` holds no `look`.
 * Returns the hit's offset, or -1.
 */
scr_int
run_c_word_pre400 (scr_int version, const scr_char *line, const scr_char *word)
{
  const scr_int length = strlen (word);
  const scr_char *hit;
  scr_char end;

  for (hit = line; *hit != NUL; hit++)
    {
      if (scr_strncasecmp (hit, word, length) != 0)
        continue;
      if (hit == line || hit[-1] == ' ')
        break;
    }
  if (*hit == NUL)
    return -1;

  end = hit[length];
  if (end == NUL || end == ' ' || end == ','
      || (end == '.' && version == TAF_VERSION_390))
    return hit - line;
  return -1;
}

/*
 * run_c_word()
 *
 * c(word) at any version: 4.0's whole-word test (lib_input_contains_word())
 * or the pre-4.0 first-hit rule above.
 */
scr_bool
run_c_word (scr_int version, const scr_char *line, const scr_char *word)
{
  return version >= TAF_VERSION_400
         ? lib_input_contains_word (line, word)
         : run_c_word_pre400 (version, line, word) >= 0;
}


/* In therest() order; '?' marks an arm that needs an empty message. */
static const scr_char *const THEREST_ARMS_380[] = {
  "open", "close", "eat", "drink", "give", "ask", "talk to", "?talk",
  "say", "look", "clean", "run", "stop", "read", "wash", "cut", "hit",
  "kill", "move", "lift", "light", "suck", "feel", "turn", "go", "enter",
  "smell", "push", "pull", "press", "shake", "clear", "clean", "kick",
  "punch", "fight", "jump", "feed", "unblock", "?block", "unlock",
  "?lock", "climb", "listen", "shout", "sing", "hum", "dance", "whistle",
  "cry", "wait", "buy", "sell", "break", "destroy", "smash", "kiss", "fly",
  "feed", "feel", "please", "fix", "repair", "mend", "date", "time",
  "sleep", "sit on", "sit in", "stand on", "stand in", "lie on", "lie in",
  nullptr
};

static const scr_char *const THEREST_ARMS_390[] = {
  "open", "close", "eat", "drink", "give", "ask", "talk to", "?talk",
  "say", "look", "clean", "run", "stop", "read", "wash", "cut", "kill",
  "move", "lift", "light", "suck", "feel", "touch", "turn", "go", "enter",
  "smell", "push", "pull", "press", "shake", "kick", "hit", "clear",
  "punch", "fight", "jump", "feed", "unblock", "block", "unlock", "lock",
  "climb", "listen", "shout", "sing", "hum", "dance", "whistle", "cry",
  "wait", "examine", "ex", "x", "buy", "sell", "break", "destroy", "smash",
  "kiss", "fly", "feed", "feel", "please", "fix", "repair", "mend", "date",
  "time", "sleep", "sit on", "sit in", "stand on", "stand in", "lie on",
  "lie in", "xyzzy", nullptr
};

/* The arm table for a version; run370 has no shake arm. */
static const scr_char *const *
run_therest_arms (scr_int version)
{
  return version == TAF_VERSION_390 ? THEREST_ARMS_390 : THEREST_ARMS_380;
}

static scr_bool
run_therest_arm_skipped (scr_int version, const scr_char *word)
{
  return version < TAF_VERSION_380 && strcmp (word, "shake") == 0;
}

/*
 * run_therest_arm_at()
 *
 * TRUE when one of therest()'s own verb arms begins at WORD.  Used to tell
 * a line that reaches therest from one an earlier handler owns; the arms
 * that need an empty message count too, being arms all the same.
 */
scr_bool
run_therest_arm_at (scr_int version, const scr_char *word)
{
  const scr_char *const *arm;

  for (arm = run_therest_arms (version); *arm; arm++)
    {
      const scr_char *const entry = *arm + ((*arm)[0] == '?' ? 1 : 0);
      const scr_int length = strlen (entry);

      if (run_therest_arm_skipped (version, entry))
        continue;
      if (scr_strncasecmp (word, entry, length) == 0
          && (word[length] == NUL || word[length] == ' '))
        return TRUE;
    }
  return FALSE;
}


/*
 * run_therest_winner_pre400()
 *
 * Pre-4.0 therest() is not a verb-first table.  It is one long run of
 * keyword arms, each `If c("<verb>") Then msg = ...`, over the whole line,
 * and each arm overwrites the message the one before it wrote, so the LAST
 * arm whose keyword the line holds answers (at 3.9 most arms go through
 * checkverb with the same c() test).  Measured on p38ASK/p39ASK with
 * harness/make_38_askprobe.py and make_39_askprobe.py
 * (runner_probes/ask.run3[89]0.kw.*): `zzz push stone`, `stone jump` and `zzz
 * cut stone` answer the verb wherever it stands; `push and pull stone` and
 * `push stone pull` pull, `push stone and kick` kicks, `buy stone kiss`
 * kisses, `drink push stone` pushes and `climb stone and sit on stone` sits.
 * talk, block and lock only write an empty message, so they lose to any
 * earlier arm.  The arms that live above therest() (open, read, ...) have
 * answered before any of this runs.
 *
 * Returns the winning arm's keyword and sets *offset to where c() found it,
 * or returns NULL.
 */
static const scr_char *
run_therest_winner_pre400 (scr_int version, const scr_char *line,
                           scr_int *offset)
{
  const scr_char *const *arm;
  const scr_char *winner = nullptr;

  for (arm = run_therest_arms (version); *arm; arm++)
    {
      const scr_bool needs_empty = (*arm)[0] == '?';
      const scr_char *const word = *arm + (needs_empty ? 1 : 0);
      scr_int found;

      if (run_therest_arm_skipped (version, word))
        continue;
      if (needs_empty && winner)
        continue;

      found = run_c_word_pre400 (version, line, word);
      if (found >= 0)
        {
          winner = word;
          *offset = found;
        }
    }

  return winner;
}


/*
 * run_therest_pre400()
 *
 * When the winning arm's keyword does not open the line, answer the line as
 * if it did: the keyword is moved to the front and the library runs again,
 * so its "<verb> %object% *" row answers with the object therest() found
 * (the fallback rows resolve by containment).  The look arm, and 3.9's
 * examine arms, answer the flat "Nothing special." whatever object the line
 * names.  Returns TRUE if the line was answered.
 */
scr_bool
run_therest_pre400 (scr_gameref_t game, const scr_char *string)
{
  static scr_bool is_active = FALSE;
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *winner;
  scr_int offset = -1;
  std::string moved;
  scr_bool answered;

  if (is_active || version >= TAF_VERSION_400
      || game->turn.pending_endgame != 0)
    return FALSE;

  /*
   * A winner opening the line has had its own row.  That includes a 3.7/3.8
   * `look X`, which the Runner leaves to therest's look arm ("Nothing
   * special." for anything at all; p37EXAM/p38EXAM, run370x plookobj37,
   * run380x plookobj38).  Deliberate deviation: Scarier examines it, as from
   * 3.9.
   */
  winner = run_therest_winner_pre400 (version, string, &offset);
  if (!winner || offset == 0)
    return FALSE;

  /*
   * generaltasks calls takes, drops, wears, removes and examines ahead of
   * therest, so a line naming any of them never reaches these arms at all;
   * see lib_earlier_handler_claims_pre400().  The 3.7 refusal above this
   * one has always known it (run_therest_absent_370's EARLIER list).
   */
  if (lib_earlier_handler_claims_pre400 (game, string))
    return FALSE;

  if (strcmp (winner, "look") == 0 || strcmp (winner, "examine") == 0
      || strcmp (winner, "ex") == 0 || strcmp (winner, "x") == 0)
    {
      pf_buffer_string (gs_get_filter (game), "Nothing special.\n");
      return TRUE;
    }

  moved = winner;
  moved += ' ';
  moved.append (string, offset);
  moved += ' ';
  moved.append (string + offset + strlen (winner));

  /* Tidy the spaces the cut left behind. */
  {
    std::string tidy;
    for (const scr_char c : moved)
      if (c != ' ' || (!tidy.empty () && tidy.back () != ' '))
        tidy += c;
    while (!tidy.empty () && tidy.back () == ' ')
      tidy.pop_back ();
    moved = tidy;
  }

  is_active = TRUE;
  answered = run_standard_verb_commands (game, moved.c_str ())
             || run_standard_give_npc_commands (game, moved.c_str ())
             || run_standard_fallback_commands (game, moved.c_str ());
  is_active = FALSE;
  return answered;
}


/*
 * run_therest_absent_370()
 *
 * 3.7's therest() refuses a line naming an absent object before any verb
 * arm -- see lib_therest_absent_370().  Only a line that reaches therest()
 * gets it: one holding a therest arm keyword that no earlier generaltasks
 * handler takes.  Those handlers enter on c() words anywhere in the line --
 * takes, drops, wears and removes, openclose, examines, give, wait,
 * whereis, gotoplace, thank and the question words -- and keep their own
 * wording, so a line holding any of them is
 * left to them.  ask, talk and say are left alone too: not measured.
 */
scr_bool
run_therest_absent_370 (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const EARLIER[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "open", "close", "x", "examine", "look at", "ex", "exam", "read",
    "give", "wait", "where", "find", "locate", "goto", "go to", "thank",
    "when", "who", "what", "how", "can", "why", "score", "ask", "talk", "say",
    nullptr
  };
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));
  const scr_char *const *word;
  scr_int offset;

  if (version >= TAF_VERSION_380 || game->turn.pending_endgame != 0)
    return FALSE;
  if (!run_therest_winner_pre400 (version, string, &offset))
    return FALSE;
  /*
   * The " with " split is made above this test; see
   * lib_with_clause_claims().
   */
  if (lib_with_clause_claims (game))
    return FALSE;
  for (word = EARLIER; *word; word++)
    if (run_c_word_pre400 (version, string, *word) >= 0)
      return FALSE;
  if (lib_sitstand_claims_370 (game))
    return FALSE;

  return lib_therest_absent_370 (game);
}


/*
 * run_line_for_anywhere()
 *
 * The standard rows run on a line of a pass's own making: the sit words cut
 * out for lib_sitstand_anywhere()'s pre-run of wears, removes and the
 * 3.7/4.0 put, the open/close word brought to the head for
 * lib_openclose_anywhere(), the where-is half of the line for
 * lib_whereis_anywhere().
 */
scr_bool
run_line_for_anywhere (scr_gameref_t game, const scr_char *line)
{
  const run_dispatch_input_guard input (line);

  return run_standard_verb_commands (game, line);
}


/*
 * run_score_anywhere()
 *
 * Every Runner's score arm is `If c("score") Then` -- the whole word
 * anywhere in the line -- and it assigns the message, overwriting whatever
 * sitstand, inventory, the help hint or examines had written, and marks the
 * line administrative.  It sits above the
 * wait gate, so `score wait` is the score and no time passes; below takes,
 * drops, wears and removes, which claim or act first, and above the
 * swearing arm, which overwrites it.  Measured on p37SITN..p4SITN
 * (harness/make_3738_sitnpcprobe.py: `score sit`, `score wait`; the sit case
 * is lib_sitstand_anywhere()'s).  A line holding one of those earlier or
 * later handlers' words is left to its rows: not measured.
 */
scr_bool
run_score_anywhere (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const OTHERS[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "sit", "stand", "lie", "x", "examine", "look at", "ex", "exam",
    "read", "shit", "fuck", "bastard", "cunt", "crap", "hell", "shag",
    "bollocks", "bollox", "piss", "bugger", "bloody", nullptr
  };
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *const *word;
  const auto has_word = [&] (const scr_char *what) -> scr_bool
    {
      return run_c_word (version, string, what);
    };

  if (game->turn.pending_endgame != 0 || !has_word ("score")
      || scr_strcasecmp (string, "score") == 0)
    return FALSE;
  /*
   * `notify` is SCARE's meta command, not the Runner's, so `notify score`
   * is left to its usage line rather than taken for the score.
   */
  if (has_word ("notify") || has_word ("notification"))
    return FALSE;
  for (word = OTHERS; *word; word++)
    if (has_word (*word))
      return FALSE;
  if (version >= TAF_VERSION_400 && has_word ("lay"))
    return FALSE;
  if ((version >= TAF_VERSION_380 && has_word ("look in"))
      || (version >= TAF_VERSION_390 && (has_word ("look") || has_word ("l"))))
    return FALSE;

  return lib_cmd_score (game);
}


/*
 * run_wait_anywhere()
 *
 * Every Runner answers a line holding the whole word `wait` ANYWHERE with
 * "Time passes..." and the wait turns, if no handler above has written a
 * message by then: `If c("wait") [Or line = "z"] And msg = ""` (4.0's c()
 * too).  That gate
 * sits after the tasks and the handlers that enter on words anywhere in the
 * line (takes, drops, wears, removes, sitstand, openclose, examines, score,
 * profanity), and before whereis, gotoplace and therest.  So `wait stone` with
 * the stone elsewhere, `please wait`, `wait here`, `push stone wait` and `turn
 * wait` all pass time.  A line holding an earlier handler's word is left to
 * the rows that already answer it.  Give, say, inventory and a direction are
 * NOT such words: inventory() writes a message sitstand-fashion but nothing
 * below wears() claims without moving something, and give, say and the
 * directions are all `If msg = ""` arms below the gate -- so `give coin to bob
 * wait`, `say hello wait`, `i wait`, `inventory wait` and `n wait` are "Time
 * passes..." with nothing given, listed or walked (p37SITN..p4SITN,
 * harness/make_3738_sitnpcprobe.py: runner_probes/sitn.run*).
 * ask/talk stay with their rows: characters() overwrites the wait text with
 * the reply (`ask bob about hat wait` is "BOB HAT.").  `score wait` is the
 * score's (run_score_anywhere() runs first).  up/down/in/out with wait: not
 * measured.  run370's openclose writes nothing unless the line names an
 * openable object, so there `open stone wait` passes time; 3.8 on
 * refuse the open.  `look wait` is examines' from 3.9 (bare `look`), time
 * passing at 3.7/3.8.
 *
 * gotoplace still runs after it and adds its "Unknown place." -- `goto hall
 * wait` answers both.  A goto that walks prints "Moving to..." straight to
 * the screen and jumps past the message print and the turn's tick (run370
 * 42BDEE "&&&"), so the "Time passes..." is lost and only the wait loop's
 * WaitTurns - 1 ticks run: the walk is administrative and the wait counter
 * stays set.  run370x reaches that with `wait goto hall`, the game's goto
 * word and "goto" each cut from the front.
 *
 * Measured on p37GOTO, p38GOTO, p39GOTO and p4EXAM
 * (harness/make_37_gotoprobe.py, make_38_gotoprobe.py,
 * make_39_gotoprobe.py, make_400_examprobe.py):
 * runner_probes/goto.run3*.wait.*, runner_probes/exam.run400.wait.txt.
 */
scr_bool
run_wait_anywhere (scr_gameref_t game, const scr_char *string)
{
  static const scr_char *const EARLIER[] = {
    "get", "take", "pick", "from", "drop", "put", "leave", "wear", "remove",
    "strip", "sit", "stand", "lie", "x", "examine", "look at", "ex", "exam",
    "read", "score", "ask", "talk", "up", "down", "in", "out",
    "shit", "fuck", "bastard", "cunt", "crap", "hell",
    "shag", "bollocks", "bollox", "piss", nullptr
  };
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *const *word;
  const auto has_word = [&] (const scr_char *what) -> scr_bool
    {
      return run_c_word (version, string, what);
    };

  if (game->turn.pending_endgame != 0 || !has_word ("wait"))
    return FALSE;

  /* The plain forms keep their own rows. */
  if (scr_strncasecmp (string, "wait", 4) == 0
      && strspn (string + 4, " 0123456789") == strlen (string + 4))
    return FALSE;

  for (word = EARLIER; *word; word++)
    if (has_word (*word))
      return FALSE;
  if ((version >= TAF_VERSION_390 && has_word ("bugger"))
      || (version < TAF_VERSION_400 && has_word ("bloody"))
      || (version >= TAF_VERSION_380 && has_word ("look in"))
      || (version >= TAF_VERSION_390 && (has_word ("look") || has_word ("l"))))
    return FALSE;

  if (has_word ("open") || has_word ("close"))
    {
      scr_int object;

      if (version >= TAF_VERSION_380)
        return FALSE;
      GS_FOR_EACH_OBJECT (game, object)
        {
          const scr_char *name =
              prop_get_indexed_string (bundle, "Objects", object, "Short");
          const scr_int openness = gs_object_openness (game, object);

          if ((openness == OBJ_OPEN || openness == OBJ_CLOSED)
              && name && name[0] != NUL && has_word (name))
            return FALSE;
        }
    }

  const size_t mark = pf_buffer_length (filter);
  const scr_bool was_admin = game->turn.is_admin;

  lib_cmd_wait (game);
  const size_t after = pf_buffer_length (filter);
  game->turn.is_admin = FALSE;
  if (lib_cmd_go_place (game) && game->turn.is_admin)
    {
      const std::string moving = pf_cut_tail (filter, after);

      pf_truncate (filter, mark);
      pf_buffer_string (filter, moving.c_str ());
      return TRUE;
    }

  game->turn.is_admin = was_admin;
  return TRUE;
}

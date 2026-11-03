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
 * Line re-spelling ahead of the standard library: the goto and battle
 * classifiers, verb hoisting and the 4.0 two-verb pass.
 *
 * Split out of scrunner.cpp; see scrunner.h for what the runner files
 * share.
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
 * run_hoist_verb_line()
 *
 * Every Runner matches its library verb ANYWHERE in the line, and Scarier
 * anchors most of it.  Every handler the input routine calls enters on c(<word>) --
 * the whole word, wherever it sits -- so a nonsense head changes nothing:
 * run400 answers `blorp take` "Take what?", `blorp take coin` "You take the
 * coin.", `blorp eat` "I don't understand what you are trying to eat.",
 * `blorp sit` "You sit down on the ground." and so on for twenty-nine of the
 * thirty-four verbs the probe types, though `blorp` is in no vocabulary.
 * (`search`, `wave` and `throw` are "I don't understand." because no arm
 * holds those words outside dobattle; `give` is the shape that shows the two
 * halves are separate, printing the "(to Nobody)" echo and then "Give
 * what?".)  Measured 2026-09-20 on p4REW with make_rewriteprobe.py, run400x
 * runner_probes/rew.run400.casc.txt.
 *
 * Scarier's table is anchored at the head, so the port is a rewrite rather
 * than a re-plumbing: hoist the verb to the front and let the ordinary rows
 * answer the line they already know.  The object half needs nothing -- the
 * 4.0 rows already bind their noun by score over the whole line, so `take
 * blorp coin` is already "You take the coin.".
 *
 * Below 4.0 the same is true, but only five handlers are still anchored in
 * Scarier: takes, drops, wears, removes and examines (HOIST_VERBS_PRE400
 * carries their words and the addresses).  Everything else already matches
 * its word anywhere, which is why `blorp drink`, `blorp push`, `blorp sit`
 * and `blorp read` already agreed.  run370x runner_probes/rew.run370.casc.rtf
 * and run380x runner_probes/rew.run380.casc.rtf answer `blorp take` "Take
 * what?", `blorp drop` "Drop what?", `blorp wear` "Wear what?", `blorp remove`
 * "Remove what?" and `blorp examine` "Nothing special."; run390x
 * runner_probes/rew.run390.casc.txt the same five.  What is left after the
 * hoist at those versions is the NOUN half -- `blorp take coin` is "You pick
 * up the coin." because takes()/drops() resolve with co() over the whole line,
 * where Scarier reads the text after the verb -- and run390's `blorp put`,
 * which answers "Give what?"; both are open leads.
 *
 * The hoist is deliberately narrow, and the narrowing is where the rule is
 * still owed:
 *
 *  - Nothing happens when the line's FIRST word is one of these verbs.  The
 *    anchored pass has already had that line and declined, so a hoist could
 *    only re-answer it, and every walkthrough line that starts with a verb
 *    is left exactly as it was.
 *  - Nothing happens when the line holds two or more of them: that is
 *    run_two_verb_line_400()'s business at 4.0 and
 *    lib_two_verb_line_pre400()'s below it, and whatever they decline is
 *    left exactly as it was.
 *
 * The word list is read straight out of the Runner: the literals each of
 * those procs hands to c() (Proc_21_38_454CB0) and to therest's verb helper
 * Proc_19_86_4455F8, in call order.  The particles those arms test as a
 * SECOND word are left out ("with", "about", "off", "on", "to", "from",
 * "all", "in"), as are the meta words the input routine answers itself
 * ("score", "wait", "turns", "version", "undo" -- `wait` is already matched
 * anywhere by run_wait_anywhere(), `score` by run_score_anywhere(), and the
 * sitstand words by lib_sitstand_anywhere()).  dobattle's verbs are in only
 * while the Battle System is on, which is the gate run400 puts on the call
 * itself (48A4A2).
 *
 * The list is kept one table per handler, in generaltasks' call order,
 * because run_two_verb_line_400() needs to know which handler a word
 * belongs to as well as that it is a verb.
 */
static const scr_char *const HOIST_VERBS_400_PUTDROP[] = {
  "put", "drop", nullptr
};

static const scr_char *const HOIST_VERBS_400_TAKE[] = {
  "empty", "get", "take", "pick", nullptr
};

static const scr_char *const HOIST_VERBS_400_WEAR[] = {
  "wear", "put on", nullptr
};

static const scr_char *const HOIST_VERBS_400_REMOVE[] = {
  "remove", "take off", nullptr
};

/* openclose 476468, whereis 4684E4, gotoplace 464E90 and characters
   480674: handlers a two-verb line leaves to passes of their own
   (lib_openclose_anywhere(), lib_whereis_anywhere(), run_goto_line_class())
   or whose place in the order is not measured. */
static const scr_char *const HOIST_VERBS_400_OTHER[] = {
  "open", "close", "lock", "unlock",
  "where", "find", "locate", "goto", "go to", "go",
  "speak to", "pick up", nullptr
};

/* give 48A985 and the character handler's give 48022F: both only ever fill
   an EMPTY buffer, so give ranks below every other handler on the line. */
static const scr_char *const HOIST_VERBS_400_GIVE[] = {
  "give", nullptr
};

static const scr_char *const HOIST_VERBS_400_EXAMINE[] = {
  "examine", "look at", "look in", "read", "look", nullptr
};

/* therest 489F4C, in its own cascade order: the LAST arm a line names is
   the one that speaks. */
static const scr_char *const HOIST_VERBS_400_THEREST[] = {
  "eat", "drink", "ask", "talk to", "talk", "say", "clean", "run", "stop",
  "wash", "cut", "kill", "move", "lift", "light", "suck", "feel", "touch",
  "rub", "turn", "enter", "smell", "push", "pull", "press", "shake", "kick",
  "hit", "clear", "punch", "fight", "jump", "feed", "unblock", "block",
  "climb", "listen", "shout", "sing", "hum", "dance", "whistle", "cry",
  "buy", "sell", "break", "destroy", "smash", "kiss", "fly", "please",
  "fix", "repair", "mend", "sleep", "xyzzy", nullptr
};

static const scr_char *const *const HOIST_TABLES_400[] = {
  HOIST_VERBS_400_PUTDROP, HOIST_VERBS_400_TAKE, HOIST_VERBS_400_WEAR,
  HOIST_VERBS_400_REMOVE, HOIST_VERBS_400_OTHER, HOIST_VERBS_400_GIVE,
  HOIST_VERBS_400_EXAMINE, HOIST_VERBS_400_THEREST, nullptr
};

/*
 * run_goto_line_class()
 *
 * gotoplace (run400 464E90, called at 48ACD7; run390 45FD8A, run380 442AE4,
 * run370 43C22D) sits below takes, drops, wears, removes, openclose,
 * examines and whereis in generaltasks, above therest and characters.  Its
 * two messages are APPENDS (`MemVar_4941B0 = MemVar_4941B0 & "Unknown
 * place."`, 464E49; " can't get there from here." 464E3B), while a walk
 * ("&&&") drops whatever was said before it.  So on a line that holds a goto
 * and one other verb, the other verb's handler answers first and gotoplace
 * then either stays out (the handler claimed), adds its message with no
 * break, or walks.  Measured on p37ORD..p4ORD with make_orderprobe.py
 * (run370x..run400x runner_probes/ord.run*.goto.*, 2026-09-21):
 *   x/examine goto cave        "Nothing special." (4.0 "You see no such
 *                              thing."), no goto: examines claims
 *   take goto cave             "Take what?Unknown place." (3.7 walks)
 *   take box goto cave         3.7-3.9 "You've already got a box!Unknown
 *                              place."; 4.0 "You are already carrying the
 *                              box." alone
 *   goto cave take box         "You pick up the box.", no goto text
 *   drop goto cave             "Drop what?" alone below 3.9, "Drop
 *                              what?Unknown place." at 3.9 and 4.0
 *   wear goto cave             "Wear what?Unknown place." (3.8+)
 *   where is goto cave         "I don't know where that is!Unknown place."
 *   goto cave open box         "You open the box....Unknown place."
 *   open/push goto cave, ask bob about goto cave   "Unknown place." alone
 *   goto cave x box            examines below 4.0, "Unknown place." at 4.0
 *                              (x counts at the head only)
 * The class says which handler answers the line before gotoplace does;
 * RUN_GOTO_NONE is a line this pass leaves to the ordinary order: no goto,
 * no verb or two of them, a splitter, or give, wait or the inventory, which
 * answer as the Runner does left to that order.  sit/stand/lie and score
 * are RUN_GOTO_KEEP: "You sit down on the ground.Unknown place." at every
 * version (make_orderprobe.py, runner_probes/ord.run*.rest.*).
 */

scr_int
run_goto_line_class (scr_gameref_t game, const scr_char *line)
{
  static const scr_char *const BAIL[] = {
    "and", "then", "with", "wait", "give", "i", "inv", "inventory",
    "put on", "take off", nullptr
  };
  static const scr_char *const SITSTAND[] = {
    "sit", "stand", "lie", "lay", "score", nullptr
  };
  static const scr_char *const EXAMINE[] = {
    "examine", "look at", "read", nullptr
  };
  static const scr_char *const EXAMINE_SHORT[] = { "x", "ex", "exam", nullptr };
  static const scr_char *const TAKE[] = { "get", "take", "pick", nullptr };
  static const scr_char *const DROP[] = { "drop", "put", "leave", nullptr };
  static const scr_char *const KEEP[] = {
    "wear", "remove", "where", "find", "locate", nullptr
  };
  static const scr_char *const OPEN[] = { "open", "close", nullptr };
  const scr_int version = run_get_version (gs_get_bundle (game));
  scr_int found = RUN_GOTO_NONE, groups = 0;

  if (!line || strchr (line, ',') || strchr (line, '.')
      || !lib_goto_line_enters (game, line))
    return RUN_GOTO_NONE;

  const auto has_word = [&] (const scr_char *w) -> scr_bool
    {
      return run_c_word (version, line, w);
    };
  const auto any = [&] (const scr_char *const *words) -> scr_bool
    {
      for (const scr_char *const *w = words; *w; w++)
        if (has_word (*w))
          return TRUE;
      return FALSE;
    };
  const auto at_head = [&] (const scr_char *const *words) -> scr_bool
    {
      for (const scr_char *const *w = words; *w; w++)
        {
          const size_t size = strlen (*w);

          if (scr_strncasecmp (line, *w, size) == 0
              && (line[size] == NUL || line[size] == ' '))
            return TRUE;
        }
      return FALSE;
    };
  const auto note = [&] (scr_bool hit, scr_int group)
    {
      if (hit)
        {
          found = group;
          groups++;
        }
    };

  if (any (BAIL))
    return RUN_GOTO_NONE;

  if (version >= TAF_VERSION_400)
    {
      note (any (EXAMINE) || has_word ("look in") || at_head (EXAMINE_SHORT),
            RUN_GOTO_EXAMINE);
      note (any (HOIST_VERBS_400_THEREST)
            || (any (EXAMINE_SHORT) && !at_head (EXAMINE_SHORT)),
            RUN_GOTO_BELOW);
    }
  else
    {
      note (any (EXAMINE) || any (EXAMINE_SHORT)
            || (version >= TAF_VERSION_380 && has_word ("look in"))
            || (version >= TAF_VERSION_390
                && (has_word ("look") || has_word ("l"))),
            RUN_GOTO_EXAMINE);
      note (any (HOIST_VERBS_400_THEREST), RUN_GOTO_BELOW);
    }
  note (any (TAKE), RUN_GOTO_TAKE);
  note (any (DROP), RUN_GOTO_DROP);
  note (any (KEEP), RUN_GOTO_KEEP);
  note (any (OPEN), RUN_GOTO_OPEN);
  note (any (SITSTAND), RUN_GOTO_KEEP);

  return groups == 1 ? found : RUN_GOTO_NONE;
}

/*
 * run_goto_strip_head()
 *
 * The line without a goto word at its head ("goto", "go to", from 3.9 "go",
 * and 3.7's own goto word), for the handlers that answer a goto line before
 * gotoplace does.
 */
scr_bool
run_goto_strip_head (scr_gameref_t game, const scr_char *line,
                     std::string &rest)
{
  const scr_int version = run_get_version (gs_get_bundle (game));
  std::vector<std::string> heads = { "goto", "go to" };
  scr_vartype_t vt_key[3], vt_rvalue;

  if (version >= TAF_VERSION_390)
    heads.push_back ("go");
  if (version < TAF_VERSION_380)
    {
      vt_key[0].string = "Commands";
      vt_key[1].integer = 15;
      vt_key[2].string = "Word";
      if (prop_get (gs_get_bundle (game), "S<-sis", &vt_rvalue, vt_key)
          && vt_rvalue.string && vt_rvalue.string[0] != NUL)
        heads.push_back (vt_rvalue.string);
    }
  for (const std::string &head : heads)
    if (scr_strncasecmp (line, head.c_str (), head.size ()) == 0
        && line[head.size ()] == ' ')
      {
        rest = line + head.size () + strspn (line + head.size (), " ");
        return !rest.empty ();
      }
  return FALSE;
}

/*
 * run_goto_after()
 *
 * gotoplace's turn on a line run_goto_line_class() sorted.  examines claims
 * outright; a take or drop that moved something claims, and so does a 4.0
 * take or drop naming an object (its own refusal is the answer) and any
 * drop below 3.9 ("Drop what?" alone).  Otherwise gotoplace runs on the line
 * as typed, since it cuts its words from the front of the whole line: a walk
 * drops what the handler said, and anything else is added to it with no
 * break.  therest and characters are below gotoplace and say nothing once it
 * has spoken, and openclose says nothing when no object is named.
 */
scr_bool
run_goto_after (scr_gameref_t game, const scr_char *typed, scr_int goto_class,
                size_t mark, const std::vector<scr_int> &places,
                scr_bool status)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_bool named = lib_goto_line_names_object (game, typed);
  scr_bool moved = FALSE, claimed = FALSE;
  scr_int object;

  lib_go_place_off = FALSE;
  for (object = 0; object < gs_object_count (game)
                   && 2 * object + 1 < (scr_int) places.size (); object++)
    if (gs_object_position (game, object) != places[2 * object]
        || gs_object_parent (game, object) != places[2 * object + 1])
      moved = TRUE;

  switch (goto_class)
    {
    case RUN_GOTO_EXAMINE:
      claimed = TRUE;
      break;
    case RUN_GOTO_TAKE:
      claimed = moved || (version >= TAF_VERSION_400 && named);
      break;
    case RUN_GOTO_DROP:
      claimed = moved || version < TAF_VERSION_390
                || (version >= TAF_VERSION_400 && named);
      break;
    default:
      break;
    }
  if (claimed || game->pending_endgame != 0)
    return status;

  std::string earlier = pf_cut_tail (filter, mark);
  if (goto_class == RUN_GOTO_BELOW || (goto_class == RUN_GOTO_OPEN && !named))
    earlier.clear ();
  while (!earlier.empty () && earlier[earlier.size () - 1] == '\n')
    earlier.erase (earlier.size () - 1);

  const scr_bool first_admin = game->is_admin;
  run_dispatch_input = typed;
  game->is_admin = FALSE;
  if (lib_cmd_go_place (game) && game->is_admin)
    return TRUE;

  const std::string said = pf_cut_tail (filter, mark);
  pf_buffer_string (filter, earlier.c_str ());
  pf_buffer_string (filter, said.c_str ());
  if (goto_class != RUN_GOTO_BELOW && !earlier.empty ())
    game->is_admin = first_admin;
  return status || !earlier.empty () || !said.empty ();
}

/*
 * run_battle_line_class()
 * run_battle_line()
 *
 * dobattle (run400 47F084 at 48A4A2, run390 45F4AF) is a plain Call, made
 * below wears and removes and above everything else in the library, and it
 * claims nothing.  What decides a line holding one of its verbs AND another
 * handler's word is what it does to the message buffer: once var_90 -- the
 * first of attack, fight, kill, kick, chop, cut, hit, shoot, stab and throw
 * that the line holds as a whole word -- is set, it ASSIGNS "" to the
 * buffer (run390 44CBFD, run400 47EAEF) before its target loop.  Measured
 * 2026-09-21 on p39BORD/p4BORD (make_battleorderprobe.py), run390x
 * runner_probes/bord.run390.batt.txt / runner_probes/bord.run390.batt2.txt and
 * run400x runner_probes/bord.run400.batt.txt /
 * runner_probes/bord.run400.batt2.txt:
 *
 *  - Above it, a take or drop that acts claims the line and no blow is
 *    struck.  4.0's get_outer and put_drop_list read their word anywhere on
 *    such a line and claim on a refusal too: `hit bob drop coin`, the coin
 *    on the floor, is "You are not holding the coin.".  A 3.9 take or drop
 *    that only refuses is wiped, and the same line is "You hit Bob.".
 *  - wears and removes act and their text is wiped: `hit bob wear hat`
 *    wears the hat and says "You hit Bob.".
 *  - The blow needs its verb BEFORE the character's name (47EBC9); with no
 *    target the answer is "Who do you want to attack?".
 *  - Below it, sitstand, an openclose that names an object, examines, score,
 *    whereis and characters()' take/where/examine/talk/ask arms overwrite
 *    the blow; gotoplace appends to it ("You hit Bob.Unknown place."); an
 *    objectless openclose (its refusals are buffer-gated), give, wait and
 *    therest say nothing.
 *
 * 4.0's examines and characters()' examine arm take x/ex/exam at the HEAD
 * only (Proc_21_37_447B18, 47FE19) and examine/look anywhere, so `hit bob
 * x box` keeps the blow there, where 3.9 describes Bob.  run390's ask arm
 * wants the character's name at column 5 (InStr = 5, 459818), which is
 * where `hit bob ask bob about hat` has it.
 *
 * Only lines holding a word of one of those handlers are taken; a line of
 * the battle verb alone keeps the %character% rows.  Lines naming two of
 * the other handlers are not measured, and take their steps in call order.
 */

static const struct
{
  const scr_char *const word;
  const scr_int kind;
} RUN_BATTLE_WORDS[] = {
  {"wear", RUN_BATTLE_WEAR}, {"put on", RUN_BATTLE_WEAR},
  {"remove", RUN_BATTLE_REMOVE}, {"take off", RUN_BATTLE_REMOVE},
  {"get", RUN_BATTLE_TAKE}, {"take", RUN_BATTLE_TAKE},
  {"pick", RUN_BATTLE_TAKE},
  {"drop", RUN_BATTLE_DROP}, {"leave", RUN_BATTLE_DROP},
  {"sit", RUN_BATTLE_SIT}, {"stand", RUN_BATTLE_SIT},
  {"lie", RUN_BATTLE_SIT},
  {"open", RUN_BATTLE_OPEN}, {"close", RUN_BATTLE_OPEN},
  {"x", RUN_BATTLE_EXAMINE}, {"ex", RUN_BATTLE_EXAMINE},
  {"exam", RUN_BATTLE_EXAMINE}, {"examine", RUN_BATTLE_EXAMINE},
  {"look at", RUN_BATTLE_EXAMINE}, {"look", RUN_BATTLE_EXAMINE},
  {"read", RUN_BATTLE_EXAMINE},
  {"score", RUN_BATTLE_SCORE}, {"give", RUN_BATTLE_GIVE},
  {"where", RUN_BATTLE_WHERE}, {"find", RUN_BATTLE_WHERE},
  {"locate", RUN_BATTLE_WHERE},
  {"goto", RUN_BATTLE_GOTO}, {"go to", RUN_BATTLE_GOTO},
  {"wait", RUN_BATTLE_WAIT},
  {"talk to", RUN_BATTLE_TALK}, {"speak to", RUN_BATTLE_TALK},
  {"ask", RUN_BATTLE_ASK},
  {nullptr, 0}
};

scr_int
run_battle_line_class (scr_gameref_t game, const scr_char *line)
{
  const scr_int version = run_get_version (gs_get_bundle (game));
  scr_int index, kinds = 0;

  if (!line || !lib_battle_line_verb (game, line))
    return 0;
  for (index = 0; RUN_BATTLE_WORDS[index].word; index++)
    {
      const scr_char *const word = RUN_BATTLE_WORDS[index].word;

      if (run_c_word_pre400 (version, line, word) < 0)
        continue;
      /* 4.0's examines anchors x/ex/exam to the head (447B18). */
      if (version >= TAF_VERSION_400 && strlen (word) <= 4
          && RUN_BATTLE_WORDS[index].kind == RUN_BATTLE_EXAMINE
          && strncmp (word, "look", 4) != 0 && strcmp (word, "read") != 0
          && run_c_word_pre400 (version, line, word) != 0)
        {
          /* No examine, but still not the %character% rows' line: `hit
             bob x coin` is a blow. */
          kinds |= RUN_BATTLE_PLAIN;
          continue;
        }
      kinds |= RUN_BATTLE_WORDS[index].kind;
    }
  return kinds;
}

/* LINE with the whole word WORD taken out, once; LINE if it has none. */
static std::string
run_battle_cut_word (scr_int version, const std::string &line,
                     const scr_char *word)
{
  const scr_int at = run_c_word_pre400 (version, line.c_str (), word);
  std::string out;

  if (at < 0)
    return line;
  out = line.substr (0, at);
  out += line.substr (at + strlen (word) + strspn (line.c_str () + at
                                                   + strlen (word), " "));
  while (!out.empty () && out[out.size () - 1] == ' ')
    out.erase (out.size () - 1);
  return out;
}

/* Every object's position and parent, to tell whether a handler acted. */
std::vector<scr_int>
run_battle_places (scr_gameref_t game)
{
  std::vector<scr_int> places;
  scr_int object;

  GS_FOR_EACH_OBJECT (game, object)
    {
      places.push_back (gs_object_position (game, object));
      places.push_back (gs_object_parent (game, object));
    }
  return places;
}

/* The first word of KIND the line holds, NULL for none. */
const scr_char *
run_battle_kind_word (scr_int version, const scr_char *line, scr_int kind)
{
  scr_int index;

  for (index = 0; RUN_BATTLE_WORDS[index].word; index++)
    {
      if (RUN_BATTLE_WORDS[index].kind == kind
          && run_c_word_pre400 (version, line, RUN_BATTLE_WORDS[index].word)
             >= 0)
        return RUN_BATTLE_WORDS[index].word;
    }
  return nullptr;
}

/*
 * LINE re-spelled for the handler of KIND: the battle verb and the handler's
 * own word cut out, and HEAD put in front.
 */
std::string
run_battle_respell (scr_gameref_t game, const scr_char *line, scr_int kind,
                    const scr_char *head)
{
  const scr_int version = run_get_version (gs_get_bundle (game));
  std::string rest = run_battle_cut_word (version, line,
                                          lib_battle_line_verb (game, line));
  const scr_char *word = run_battle_kind_word (version, rest.c_str (), kind);

  if (word)
    rest = run_battle_cut_word (version, rest, word);
  return rest.empty () ? std::string (head) : std::string (head) + " " + rest;
}

/* The standard rows' answer to LINE, taken back out of the buffer. */
static std::string
run_battle_answer (scr_gameref_t game, const std::string &line)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const size_t mark = pf_buffer_length (filter);
  std::string lower (line);

  /* Typed lines reach the rows lower-cased; so must a Name put in one. */
  scr_lowercase (lower);
  {
    const run_dispatch_input_guard input (lower.c_str ());

    run_standard_commands (game, lower.c_str ());
  }
  return pf_cut_tail (filter, mark);
}

scr_bool
run_battle_line (scr_gameref_t game, const scr_char *typed, scr_int kinds)
{
  const scr_filterref_t filter = gs_get_filter (game);
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = run_get_version (bundle);
  const size_t mark = pf_buffer_length (filter);
  const scr_int npc = lib_battle_line_npc (game, typed);
  const scr_char *const name = npc >= 0
      ? prop_get_indexed_string (bundle, "NPCs", npc, "Name") : nullptr;
  std::string answer, said;
  scr_bool struck, admin;

  {
    const run_dispatch_input_guard input (typed);

    struck = lib_cmd_attack_npcs (game);
  }
  if (!struck)
    {
      pf_truncate (filter, mark);
      return FALSE;
    }
  answer = pf_cut_tail (filter, mark);
  admin = game->is_admin;

  /* wears and removes act above dobattle, and it wipes what they said. */
  if (kinds & RUN_BATTLE_WEAR)
    run_battle_answer (game, run_battle_respell (game, typed,
                                                 RUN_BATTLE_WEAR, "wear"));
  if (kinds & RUN_BATTLE_REMOVE)
    run_battle_answer (game, run_battle_respell (game, typed,
                                                 RUN_BATTLE_REMOVE,
                                                 "remove"));

  if (kinds & RUN_BATTLE_SIT)
    {
      /* sitstand reads its words anywhere in the line (27044). */
      const std::string line = run_battle_cut_word (version, typed,
                                   lib_battle_line_verb (game, typed));
      std::string rest;

      {
        const run_dispatch_input_guard input (line.c_str ());

        lib_sitstand_anywhere (game, run_line_for_anywhere, &rest);
      }
      said = pf_cut_tail (filter, mark);
      if (!said.empty ())
        answer = said;
    }
  /* openclose's " can't open " and " can't see " wait for an empty
     buffer (4756EA, 475952); its other answers are assignments. */
  if (kinds & RUN_BATTLE_OPEN)
    {
      const scr_char *const word = run_battle_kind_word (version, typed,
                                                         RUN_BATTLE_OPEN);
      said = run_battle_answer (game, run_battle_respell (game, typed,
                                                          RUN_BATTLE_OPEN,
                                                          word));
      if (!said.empty () && said.find (" can't open ") == std::string::npos
          && said.find (" can't close ") == std::string::npos
          && said.find (" can't see ") == std::string::npos)
        answer = said;
    }
  if (kinds & RUN_BATTLE_EXAMINE)
    {
      said = run_battle_answer (game, run_battle_respell (game, typed,
                                                          RUN_BATTLE_EXAMINE,
                                                          "examine"));
      if (!said.empty ())
        answer = said;
    }
  if (kinds & RUN_BATTLE_SCORE)
    {
      said = run_battle_answer (game, "score");
      if (!said.empty ())
        answer = said;
      /* score is not a turn, blow or no blow (run390 45F6B5, run400
         48A6AE). */
      admin = game->is_admin;
    }
  if (kinds & RUN_BATTLE_WHERE)
    {
      std::string line;

      if (name)
        line = std::string ("where is ") + name;
      else
        {
          line = run_battle_cut_word (version, typed,
                                      lib_battle_line_verb (game, typed));
          line = line.substr (run_c_word_pre400 (version, line.c_str (),
                              run_battle_kind_word (version, line.c_str (),
                                                    RUN_BATTLE_WHERE)));
        }
      said = run_battle_answer (game, line);
      if (!said.empty ())
        answer = said;
    }
  if (kinds & RUN_BATTLE_GOTO)
    {
      while (!answer.empty () && answer[answer.size () - 1] == '\n')
        answer.erase (answer.size () - 1);
      {
        const run_dispatch_input_guard input (typed);

        lib_cmd_go_place (game);
      }
      answer += pf_cut_tail (filter, mark);
    }

  /* characters(), last: its take, talk-to and ask arms assign. */
  if (name && (kinds & RUN_BATTLE_TAKE))
    answer = run_battle_answer (game, std::string ("take ") + name);
  if (name && (kinds & RUN_BATTLE_TALK))
    answer = run_battle_answer (game, std::string ("talk to ") + name);
  if (name && (kinds & RUN_BATTLE_ASK)
      && (version >= TAF_VERSION_400
          || run_instr (typed, name) == 4))
    {
      const scr_int about = run_c_word_pre400 (version, typed, "about");
      std::string line = std::string ("ask ") + name;

      if (about >= 0)
        line += std::string (" ") + (typed + about);
      said = run_battle_answer (game, line);
      if (!said.empty ())
        answer = said;
    }

  pf_buffer_string (filter, answer.c_str ());
  game->is_admin = admin;
  return TRUE;
}

/* dobattle 47F084, called only with the Battle System on. */
static const scr_char *const HOIST_VERBS_BATTLE_400[] = {
  "wield", "attack", "chop", "shoot", "stab", "throw", nullptr
};

/*
 * Below 4.0 only five handlers carry a `%object%` and an "X what?" form, and
 * those are the ones Scarier anchors: takes (run380 43D788, run370 435E28)
 * `get` / `take` / `pick` -- and `pick` only with no "from" in the line --
 * drops (438659 / 430475) `drop` / `put down` / `leave`, wears (432D5C /
 * 42C533) `wear` / `put on`, removes (42FD4C / 4295FF) `remove` / `take
 * off`, and examines (43C69D / 434E2A) `x` / `examine` / `look at` / `ex` /
 * `exam` / `read`, with `look in` added at 3.80 and NO bare `look` at
 * either.  Everything else pre-4.0 already matches its word anywhere
 * (run_therest_pre400(), run_therest_absent_370(), lib_sitstand_anywhere()),
 * which is why `blorp drink`, `blorp push` and `blorp sit` already agree.
 * run370 also takes the game's own word for each of these from command
 * slots 10-14, the way lib_cmd_go_place() takes slot 15; a line holding one
 * is re-spelled by lib_two_verb_line_pre400() rather than hoisted here.
 */
static const scr_char *const HOIST_VERBS_PRE400[] = {
  "get", "take", "pick",
  "drop", "put down", "leave",
  "wear", "put on", "remove", "take off",
  "x", "examine", "look at", "ex", "exam", "read",
  nullptr
};

/*
 * Verb words that are not hoistable but still decide a line: if one of these
 * opens the line, or stands beside the hoistable verb, the line is left as
 * it was.  This is the pre-4.0 half of HOIST_VERBS_400 plus the spellings
 * only the older Runners have.
 */
static const scr_char *const HOIST_VERBS_EXTRA[] = {
  "x", "ex", "exam", "leave", "strip", "put down", "put on", "take off",
  "look at", "look in", "pick up", "get up", nullptr
};

/*
 * Heads the anchored pass owns that are not hoistable verbs themselves:
 * the abbreviations, the meta rows and the words the input routine answers
 * before any of the handlers above.  A line starting with one of these has
 * already been offered to the table as it stands, so it is left alone -- `x
 * light` stays an examine and does not become `light x`.
 */
/* The line run_hoist_verb_at() is scanning, for its cross-word tests. */
static const scr_char *run_hoist_line = nullptr;

static const scr_char *const HOIST_HEADS_400[] = {
  "x", "ex", "exam", "l", "i", "inv", "inventory", "z", "wait",
  "score", "turns", "time", "date", "version", "undo", "quit", "save",
  "restore", "restart", "help", "hint", "about", "credits", "search",
  "wave", "throw", "leave", "strip", "sit", "stand", "lie", "lay",
  "n", "s", "e", "w", "ne", "nw", "se", "sw", "u", "d", "in", "out",
  "north", "south", "east", "west", "northeast", "northwest",
  "southeast", "southwest", "up", "down", "yes", "no", "wield", "attack",
  nullptr
};

/*
 * The longest list entry whose words sit at WORD, or NULL.  Words are
 * separated by single spaces: the line reaches here lower-cased and already
 * rewritten, the shape c()'s padded InStr sees.
 */
static const scr_char *
run_hoist_longest (const scr_char *const *table, const scr_char *word)
{
  const scr_char *const *entry;
  const scr_char *best = nullptr;

  for (entry = table; *entry; entry++)
    {
      const scr_int length = strlen (*entry);

      if (scr_strncasecmp (word, *entry, length) == 0
          && (word[length] == NUL || word[length] == ' ')
          && (!best || length > (scr_int) strlen (best)))
        best = *entry;
    }
  return best;
}

/* The longest entry any of the 4.0 handler tables holds at WORD. */
static const scr_char *
run_hoist_longest_400 (const scr_char *word)
{
  const scr_char *const *const *table;
  const scr_char *best = nullptr;

  for (table = HOIST_TABLES_400; *table; table++)
    {
      const scr_char *const hit = run_hoist_longest (*table, word);

      if (hit && (!best || strlen (hit) > strlen (best)))
        best = hit;
    }
  return best;
}

/* The verb this word begins, or NULL: the one the line would be re-spelled
   around.  4.0 hoists every library verb, the older Runners only the five
   handlers that anchor. */
static const scr_char *
run_hoist_verb_at (scr_gameref_t game, const scr_char *word)
{
  const scr_int version = run_get_version (gs_get_bundle (game));
  const scr_char *best;

  if (version < TAF_VERSION_400)
    {
      best = run_hoist_longest (HOIST_VERBS_PRE400, word);
      /* takes() wants `pick` with no "from" in the line (43D788). */
      if (best && strcmp (best, "pick") == 0
          && run_c_word_pre400 (version, run_hoist_line, "from") >= 0)
        best = nullptr;
      /* examines() has no "look in" before 3.80 (434E2A). */
      if (best && version < TAF_VERSION_380 && strcmp (best, "look in") == 0)
        best = nullptr;
      return best;
    }

  best = run_hoist_longest_400 (word);
  if (!best && battle_is_enabled (game))
    best = run_hoist_longest (HOIST_VERBS_BATTLE_400, word);
  return best;
}

/* A verb word of any kind, hoistable or not. */
static const scr_char *
run_hoist_any_verb_at (scr_gameref_t game, const scr_char *word)
{
  const scr_char *best = run_hoist_verb_at (game, word);

  if (!best)
    best = run_hoist_longest_400 (word);
  if (!best)
    best = run_hoist_longest (HOIST_VERBS_EXTRA, word);
  if (!best && battle_is_enabled (game))
    best = run_hoist_longest (HOIST_VERBS_BATTLE_400, word);
  return best;
}


/*
 * run_two_verb_line_400()
 *
 * A 4.0 line naming TWO library verbs.  Every handler generaltasks calls
 * enters on a whole-word c() over the WHOLE line, so `x get coin` satisfies
 * get_outer() and examines() alike, and the answer comes from the call order
 * at 48A462-48B56E and from which of those handlers CLAIMS the line:
 *
 *     put_drop_list 459DB4 (48A462)   If CBool(...) Then GoTo the turn tail
 *     get_outer     4582D8 (48A46D)   the same
 *     tasks         44CCE0 (48A481)   the same
 *     wears         463C30 (48A48C)   a plain Call -- can never claim
 *     removes       4624B0 (48A491)   a plain Call -- can never claim
 *     examines      471F94 (48A67B)   claims again
 *     therest       489F4C (48AFE4)   called ONLY with the buffer empty
 *     characters    480674 (48B56E)   overwrites whatever is there
 *
 * wears and removes write into the message without claiming, and their
 * refusals are guarded by an EMPTY buffer (run400 463B8B in front of
 * 463BBC), so the first of the two to write is the one that speaks; but
 * examines runs below them and overwrites unguarded, and therest never runs
 * at all once they have written (the 48AFE1 test is MemVar_4941B0 = "").
 * So the order that decides a line is
 *
 *     put/drop, take, examine, wear, remove, therest
 *
 * and WORD ORDER NEVER DECIDES: `take drop coin` and `drop take coin` are
 * both the drop.  Within therest the LAST arm the line names wins, that
 * cascade being one `If c(...)` after another each overwriting the message
 * before it -- `push pull coin` and `pull push coin` are both "You pull the
 * coin, but nothing happens.", `kick hit coin` and `hit kick coin` both the
 * hit.
 *
 * Measured 2026-09-21 on p4REW (make_twoverbprobe.py, run400x
 * runner_probes/rew.run400.verb.txt) and on p4TWO
 * (runner_probes/two.run400.verb3.txt); the cells that carry the rule, coin
 * loose unless said otherwise:
 *
 *   `take drop coin`   "You are not holding the coin."   put_drop_list
 *   `drop take coin`   the same
 *   `examine take coin` (held) "You are already carrying the coin."
 *   `examine drop coin` "You are not holding the coin."
 *   `wear take coin`   (held) "You are already carrying the coin."
 *   `remove drop coin` "You are not holding the coin."
 *   `remove wear coin` "You are not holding the coin."   wears, not removes
 *   `x get coin`       "You take the coin."
 *   `push take coin`   (held) "You are already carrying the coin."
 *   `push examine coin` "A gold coin."
 *   `wear examine`     "You see no such thing."   examines over "Wear what?"
 *   `take drop`        "Drop what?"
 *   `remove take hat`  "You take the hat."
 *   `x take off hat`   (worn) "You are already carrying the hat."
 *
 * Scarier answers a line from one anchored row, so the port is the same
 * re-spelling run_hoist_verb_line() does for a single verb: hoist the verb
 * whose handler the order leaves speaking to the front and leave the rest of
 * the line exactly as it stands, the other verb word included -- the winning
 * handler resolves its noun over the whole line too, which is why `wear
 * examine` has to become `examine wear` and not a bare `examine` (that would
 * trip examines' whole-line bare-verb exit at 471340).
 *
 * Narrow on purpose.  A list line ("all", "and") is left alone, as are the
 * claiming handlers whose place in the order is not measured (openclose,
 * give, whereis, gotoplace, characters, dobattle) and a `put` with no
 * container clause, whose branch at 46DC34 does not claim either -- see "A
 * put refusal silences the wear only where it CLAIMS" for what happens
 * there.  Two groups named by ONE word span ("take off" is get_outer's
 * `take` and removes' `take off`; "put on" is put_drop_list's `put` and
 * wears' `put on`) are not a two-verb line at all, and the spans have to
 * be distinct before any of this runs.
 */
enum
{
  RUN_400_PUTDROP = 1 << 0,
  RUN_400_TAKE = 1 << 1,
  RUN_400_EXAMINE = 1 << 2,
  RUN_400_WEAR = 1 << 3,
  RUN_400_REMOVE = 1 << 4,
  RUN_400_THEREST = 1 << 5,
  RUN_400_OTHER = 1 << 6,
  RUN_400_GIVE = 1 << 7
};

/* Precedence, highest first: the call order above, with wears and removes
   dropped below examines because neither can claim, and give last of all
   because it never writes over anything. */
static const scr_int RUN_400_ORDER[] = {
  RUN_400_PUTDROP, RUN_400_TAKE, RUN_400_EXAMINE,
  RUN_400_WEAR, RUN_400_REMOVE, RUN_400_THEREST, RUN_400_GIVE
};

/*
 * The examine spellings HOIST_VERBS_400_EXAMINE leaves out because the
 * anchored pass owns them as heads (HOIST_HEADS_400); the single-verb hoist
 * deliberately does not see them, so that `blorp x coin` is left exactly as
 * it was.  examines() enters on c("l") wherever it stands, but reads x, ex
 * and exam at the HEAD only (Proc_21_37_447B18): `give x hat bob` is give's
 * "Bob doesn't seem interested in the hat." (p4ORD make_orderprobe.py,
 * run400x runner_probes/ord.run400.give.txt) and `open x box` openclose's "You
 * open the box.".
 */
static const scr_char *const HOIST_VERBS_400_EXAMINE_HEADS[] = {
  "l", nullptr
};

static const scr_char *const HOIST_VERBS_400_EXAMINE_AT_HEAD[] = {
  "x", "ex", "exam", nullptr
};

static const struct
{
  const scr_char *const *table;
  scr_int group;
  scr_bool head_only;
}
RUN_400_GROUPS[] = {
  { HOIST_VERBS_400_PUTDROP, RUN_400_PUTDROP, FALSE },
  { HOIST_VERBS_400_TAKE, RUN_400_TAKE, FALSE },
  { HOIST_VERBS_400_EXAMINE, RUN_400_EXAMINE, FALSE },
  { HOIST_VERBS_400_EXAMINE_HEADS, RUN_400_EXAMINE, FALSE },
  { HOIST_VERBS_400_EXAMINE_AT_HEAD, RUN_400_EXAMINE, TRUE },
  { HOIST_VERBS_400_WEAR, RUN_400_WEAR, FALSE },
  { HOIST_VERBS_400_REMOVE, RUN_400_REMOVE, FALSE },
  { HOIST_VERBS_400_THEREST, RUN_400_THEREST, FALSE },
  { HOIST_VERBS_400_OTHER, RUN_400_OTHER, FALSE },
  { HOIST_VERBS_400_GIVE, RUN_400_GIVE, FALSE },
  { nullptr, 0, FALSE }
};

/*
 * Every handler whose word stands at WORD, as a mask, and the whole span's
 * length in *LENGTH.  0 when no verb begins there.  A span can carry two
 * handlers, and then the two read different amounts of it: `take off` is
 * removes' own two-word spelling and get_outer's one-word `take` at once,
 * which is why the length a handler claims is asked for separately by
 * run_two_verb_word_400() and never taken from here.
 */
static scr_int
run_two_verb_groups_400 (scr_gameref_t game, const scr_char *word,
                         scr_bool at_head, scr_int *length)
{
  scr_int index, groups = 0;

  *length = 0;
  for (index = 0; RUN_400_GROUPS[index].table; index++)
    {
      const scr_char *const hit
        = RUN_400_GROUPS[index].head_only && !at_head
          ? nullptr : run_hoist_longest (RUN_400_GROUPS[index].table, word);

      if (!hit)
        continue;
      groups |= RUN_400_GROUPS[index].group;
      if ((scr_int) strlen (hit) > *length)
        *length = strlen (hit);
    }
  if (!groups && battle_is_enabled (game))
    {
      const scr_char *const hit = run_hoist_longest (HOIST_VERBS_BATTLE_400,
                                                     word);

      if (hit)
        {
          groups = RUN_400_OTHER;
          *length = strlen (hit);
        }
    }
  return groups;
}

/* The spelling GROUP's own tables read at WORD, longest first. */
static const scr_char *
run_two_verb_word_400 (const scr_char *word, scr_bool at_head,
                       scr_int group)
{
  const scr_char *best = nullptr;
  scr_int index;

  for (index = 0; RUN_400_GROUPS[index].table; index++)
    {
      const scr_char *hit;

      if (RUN_400_GROUPS[index].group != group
          || (RUN_400_GROUPS[index].head_only && !at_head))
        continue;
      hit = run_hoist_longest (RUN_400_GROUPS[index].table, word);
      if (hit && (!best || strlen (hit) > strlen (best)))
        best = hit;
    }
  return best;
}

/* The winning span's position in therest's cascade, or -1. */
static scr_int
run_therest_rank_400 (const scr_char *word)
{
  const scr_char *const *entry;
  scr_int rank = -1, index;

  for (entry = HOIST_VERBS_400_THEREST, index = 0; *entry; entry++, index++)
    {
      const scr_int size = strlen (*entry);

      if (scr_strncasecmp (word, *entry, size) == 0
          && (word[size] == NUL || word[size] == ' '))
        rank = index;
    }
  return rank;
}

scr_bool
run_two_verb_line_400 (scr_gameref_t game, const scr_char *line,
                       std::string &hoisted)
{
  const scr_int version = run_get_version (gs_get_bundle (game));
  std::vector<const scr_char *> spans;
  const scr_char *scan, *winner_at = nullptr, *winner_word = nullptr;
  scr_int winner_rank = -1, seen = 0, index;

  if (version < TAF_VERSION_400 || !line || line[0] == NUL)
    return FALSE;
  const scr_bool list = run_c_word_pre400 (version, line, "all") >= 0
                        || run_c_word_pre400 (version, line, "and") >= 0;

  for (scan = line; *scan != NUL; )
    {
      scr_int length, groups;

      if (scan != line && scan[-1] != ' ')
        {
          scan++;
          continue;
        }
      /* A list arm walks co() over the whole line, so there a mid-line x
         is only a word to step over: `take x all` is the take. */
      groups = run_two_verb_groups_400 (game, scan, list || scan == line,
                                        &length);
      if (!groups)
        {
          scan++;
          continue;
        }
      /* openclose, whereis, gotoplace, characters and dobattle answer from
         passes of their own or are not measured, so nothing is re-spelled. */
      if (groups & RUN_400_OTHER)
        return FALSE;
      spans.push_back (scan);
      seen |= groups;
      scan += length;
    }
  if (spans.size () < 2)
    return FALSE;

  /*
   * A list line goes to put_drop_list's or get_outer's list arm, whichever
   * the line holds, and the arm walks co() over the whole line itself:
   * `x take all`, `push take all` and `wear take all` are "You take the
   * coin and the box.", `drop x all` "You drop the hat and the coin.", and
   * `x take coin and hat` with the hat held "You take the coin. You are
   * already carrying the hat.".  p4ORD make_orderprobe.py (run400x
   * runner_probes/ord.run400.rest.txt, 2026-09-21).  A put, and a line holding
   * both, are not measured.
   */
  if (list)
    {
      const scr_int group = (seen & RUN_400_TAKE) ? RUN_400_TAKE
                                                  : RUN_400_PUTDROP;
      const scr_char *word = nullptr;

      if (!(seen & (RUN_400_TAKE | RUN_400_PUTDROP))
          || ((seen & RUN_400_TAKE) && (seen & RUN_400_PUTDROP)))
        return FALSE;
      for (const scr_char *span : spans)
        if (!word)
          word = run_two_verb_word_400 (span, span == line, group);
      if (!word || scr_strcasecmp (word, "put") == 0)
        return FALSE;
      hoisted.assign (word);
      for (scan = line; *scan != NUL; )
        {
          scr_int length = 0;

          if (scan == line || scan[-1] == ' ')
            run_two_verb_groups_400 (game, scan, TRUE, &length);
          if (length > 0)
            {
              scan += length;
              scan += strspn (scan, " ");
              continue;
            }
          if (hoisted.size () == strlen (word))
            hoisted += " ";
          hoisted.push_back (*scan++);
        }
      return TRUE;
    }

  /*
   * put_drop_list's clauseless put branch (46DC34-46DD2C) prints and falls
   * out without claiming, so a `put` with no container clause leaves the
   * line to the handlers below it.  Beside examine or drop that is what
   * the ordinary order already does; beside a take see run_put_take_400().
   */
  if ((seen & RUN_400_PUTDROP)
      && run_c_word_pre400 (version, line, "drop") < 0
      && run_c_word_pre400 (version, line, "down") < 0
      && run_c_word_pre400 (version, line, "in") < 0
      && run_c_word_pre400 (version, line, "into") < 0
      && run_c_word_pre400 (version, line, "inside") < 0
      && run_c_word_pre400 (version, line, "on") < 0
      && run_c_word_pre400 (version, line, "onto") < 0)
    return FALSE;

  for (index = 0; !winner_at
       && index < (scr_int) (sizeof RUN_400_ORDER / sizeof RUN_400_ORDER[0]);
       index++)
    {
      const scr_int group = RUN_400_ORDER[index];
      std::vector<const scr_char *>::const_iterator span;

      if (!(seen & group))
        continue;

      /* The span that carries the winning handler: the first of them, or
         for therest the one standing last in its cascade.  Its length is
         that handler's OWN spelling and not the span's -- get_outer reads
         only the `take` out of removes' `take off`. */
      for (span = spans.begin (); span != spans.end (); span++)
        {
          const scr_char *const word
            = run_two_verb_word_400 (*span, *span == line, group);

          if (!word)
            continue;
          if (group == RUN_400_THEREST)
            {
              const scr_int rank = run_therest_rank_400 (*span);

              if (rank <= winner_rank)
                continue;
              winner_rank = rank;
            }
          else if (winner_at)
            continue;
          winner_at = *span;
          winner_word = word;
        }
    }
  /* Already at the head: the anchored pass answers it as it stands. */
  if (!winner_at || winner_at == line)
    return FALSE;

  {
    const std::string head (line, winner_at - line);

    hoisted.assign (winner_word);
    hoisted += " ";
    /* The verb took its own separating space with it. */
    hoisted.append (head, 0, head.size () - 1);
    hoisted += winner_at + strlen (winner_word);
  }
  return TRUE;
}


scr_bool
run_hoist_verb_line (scr_gameref_t game, const scr_char *string,
                    std::string &hoisted)
{
  const scr_char *scan, *found = nullptr;
  const scr_char *found_at = nullptr;
  const scr_char *body = nullptr;

  if (!string || string[0] == NUL)
    return FALSE;

  /*
   * Deliberate deviation (2026-09-30): a verb word inside an object's or a
   * character's longer name is no verb.  Every rewrite below must stand on
   * the masked line too (lib_mask_long_names()), so `x ice pick`, `get
   * cough drop` and `open drop box` keep their one verb.
   */
  const std::string masked = lib_mask_long_names (game, string);
  const scr_bool has_mask = masked != string;

  /*
   * Below 4.0 a line naming TWO of the five anchored handlers is decided by
   * generaltasks' call order and not by where the words sit, so it is
   * re-spelled even when the head is a verb itself -- `drop take hat` with
   * the hat worn is "You drop the hat.", `take remove hat` is "You remove
   * the hat.".  See lib_two_verb_line_pre400().
   */
  {
    std::string decided;

    if (run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
        && lib_two_verb_line_pre400 (game, string, &decided))
      {
        std::string unused;

        if (!has_mask
            || lib_two_verb_line_pre400 (game, masked.c_str (), &unused))
          {
            hoisted = decided;
            return TRUE;
          }
        SCR_DEVIATION ("mask_two_verb", "runner=\"%s\"", decided.c_str ());
        return FALSE;
      }
  }

  /* At 4.0 the same is true of a different call order, and there too the
     head being a verb is no reason to leave the line alone: `x get coin` is
     get_outer's answer.  See run_two_verb_line_400(). */
  if (run_two_verb_line_400 (game, string, hoisted))
    {
      std::string unused;

      if (!has_mask || run_two_verb_line_400 (game, masked.c_str (), unused))
        return TRUE;
      SCR_DEVIATION ("mask_two_verb", "runner=\"%s\"", hoisted.c_str ());
      hoisted.clear ();
      return FALSE;
    }

  if (run_hoist_longest (HOIST_HEADS_400, string))
    return FALSE;

  run_hoist_line = string;
  for (scan = string; *scan != NUL; scan++)
    {
      const scr_char *verb;

      if (scan != string && scan[-1] != ' ')
        continue;
      if (masked[scan - string] == LIB_NAME_MASK)
        {
          if (SCR_TRACING_DEVIATIONS () && run_hoist_any_verb_at (game, scan))
            SCR_DEVIATION ("mask_hoist", "verb at \"%s\"", scan);
          continue;
        }
      verb = run_hoist_any_verb_at (game, scan);
      if (!verb)
        continue;
      /*
       * The head is the anchored pass's, and it has already declined --
       * unless it is one of therest()'s own arms below 4.0, which
       * generaltasks does not reach until the five anchored handlers have
       * had the line: with the coin in hand `push take coin` is "You've
       * already got a coin!", not the push arm's "but nothing happens",
       * and `push examine coin` is the coin's description at 3.7, 3.8 and
       * 3.9 alike.  p3xREW make_twoverbprobe.py cells 31, 35 and 39
       * (runner_probes/rew.run370.verb.rtf, runner_probes/rew.run380.verb.rtf,
       * runner_probes/rew.run390.verb.txt, 2026-09-21).
       */
      if (scan == string
          && !(run_get_version (gs_get_bundle (game)) < TAF_VERSION_400
               && !run_hoist_verb_at (game, scan)
               && run_therest_arm_at (run_get_version (gs_get_bundle (game)),
                                      scan)))
        return FALSE;
      if (scan == string)
        {
          /* The arm word plays no part in the handler's own c() walk, so
             it goes with the head rather than into the object clause:
             3.9's drops row wants `drop coin`, not `drop push coin`. */
          body = scan + strlen (verb);
          body += strspn (body, " ");
          continue;
        }
      /* Two verbs: the Runner's order decides, and it is not measured. */
      if (found)
        return FALSE;
      verb = run_hoist_verb_at (game, scan);
      if (!verb)
        return FALSE;
      found = verb;
      found_at = scan;
    }
  if (!found)
    return FALSE;

  hoisted = found;
  if (!body)
    body = string;
  const std::string head (body, found_at > body ? found_at - body : 0);
  const std::string tail (found_at + strlen (found));

  if (!head.empty ())
    {
      hoisted += " ";
      /* The verb's own trailing space went with it. */
      hoisted.append (head, 0, head.size () - 1);
    }
  if (!tail.empty ())
    hoisted += tail;
  return TRUE;
}


scr_bool
run_standard_commands (scr_gameref_t game, const scr_char *string)
{
  if (run_standard_verb_commands (game, string))
    return TRUE;

  if (run_standard_give_npc_commands (game, string))
    return TRUE;

  if (run_therest_pre400 (game, string))
    return TRUE;

  if (run_standard_fallback_commands (game, string))
    return TRUE;

  /* Nothing matched the string.  Or if it did, its handler failed. */
  return FALSE;
}

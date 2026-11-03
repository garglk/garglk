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
 * The lenient census (SCR_CENSUS_LENIENT), a dump-tool audit of the lines
 * that reach a task Scarier's way but not the Runner's.
 *
 * Developer tooling, compiled only with SCARIER_DUMP_TOOLS (see scdump.cpp
 * beside it); the whole file is inside that guard.  Split out of
 * runner/scrun_dispatch.cpp, and still a runner file in everything but its
 * directory: see runner/scrunner.h for what it shares with the runner.
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
#include "runner/scrunner.h"


#ifdef SCARIER_DUMP_TOOLS
/*
 * run_lenient_census()
 *
 * SCR_CENSUS_LENIENT: for every task, the lines a player would type to reach
 * it -- each command expanded, [a/b] and {a/b} options in every combination
 * (capped), `*` left empty or holding a word, %object% each of three
 * objects' Shorts (lower-case non-statics first: a 3.9 %object% never binds
 * a static), %character% a character's Name, a variable its value, all
 * lower-cased as the Runner lower-cases a typed line -- run the Runner's way
 * (its split, its spelling, strict matching) and Scarier's
 * (run_separator_task_line(), the spelling run_element_filtered() picks,
 * lenient matching when no other task takes the line strictly).  A task no
 * line reaches the Runner's way but some line
 * reaches Scarier's is printed as
 *
 *   CENSUS woken task=<n> via=<separator|spelling|lenient> line="<line>"
 *          ran="<spelling>" cmds={<commands>}
 *
 * then a CENSUS summary.  Restrictions, where, scope and the library's
 * retries are not looked at; every object and character is marked seen so
 * that the seen gate does not stand in for them.  For auditing what the
 * deviations wake; see test/adrift4/harness/census_lenient.sh.
 */
static std::vector<std::string>
run_census_expand (const std::string &pattern, size_t &at, scr_char close)
{
  static const size_t CAP = 48;
  std::vector<std::string> result (1, std::string ());

  auto append = [&result] (const std::vector<std::string> &tails)
    {
      std::vector<std::string> joined;

      for (const std::string &head : result)
        for (const std::string &tail : tails)
          if (joined.size () < CAP)
            joined.push_back (head + tail);
      result.swap (joined);
    };

  while (at < pattern.size ())
    {
      const scr_char c = pattern[at];

      if (c == close || (close != NUL && c == '/'))
        return result;
      at++;
      if (c == '[' || c == '{')
        {
          const scr_char end = c == '[' ? ']' : '}';
          std::vector<std::string> alternatives;

          if (c == '{')
            alternatives.push_back (std::string ());
          for (;;)
            {
              std::vector<std::string> one
                  = run_census_expand (pattern, at, end);

              alternatives.insert (alternatives.end (), one.begin (),
                                   one.end ());
              if (at < pattern.size () && pattern[at] == '/')
                {
                  at++;
                  continue;
                }
              if (at < pattern.size ())
                at++;
              break;
            }
          append (alternatives);
        }
      else if (c == '*')
        {
          /* A wildcard left empty, or holding a word. */
          if (at >= pattern.size () || pattern[at] != '*')
            append (std::vector<std::string> ({std::string (),
                                               std::string ("it")}));
        }
      else
        append (std::vector<std::string> (1, std::string (1, c)));
    }
  return result;
}

static std::string
run_census_normal (const scr_char *text)
{
  std::string line;

  for (; *text != NUL; text++)
    {
      const scr_char c = scr_tolower (*text);

      if (scr_isspace (c))
        {
          if (!line.empty () && line[line.size () - 1] != ' ')
            line += ' ';
        }
      else
        line += c;
    }
  while (!line.empty () && line[line.size () - 1] == ' ')
    line.erase (line.size () - 1);
  return line;
}

static std::string
run_census_references (scr_gameref_t game, const std::string &pattern,
                       scr_int pick)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  std::string line (pattern);
  size_t at = 0;

  /* The pick'th name: lower-case, non-static objects first (a 3.9
     %object% never binds a static). */
  auto first_name = [&] (const scr_char *kind, const scr_char *field,
                         scr_int count) -> std::string
    {
      std::vector<std::string> lower_names, other_names;
      scr_int index_;

      for (index_ = 0; index_ < count; index_++)
        {
          const scr_char *name = prop_get_indexed_string (bundle, kind, index_,
                                                          field);
          std::string text (name ? name : "");
          scr_bool lower = !text.empty ();

          for (const scr_char c : text)
            lower = lower && !(c >= 'A' && c <= 'Z');
          if (lower && kind[0] == 'O')
            lower = !prop_get_indexed_boolean (bundle, kind, index_,
                                               "Static");
          if (!text.empty ())
            (lower ? lower_names : other_names).push_back (text);
        }
      lower_names.insert (lower_names.end (), other_names.begin (),
                          other_names.end ());
      return lower_names.empty () ? std::string ()
             : lower_names[pick % lower_names.size ()];
    };

  while ((at = line.find ('%', at)) != std::string::npos)
    {
      const size_t end = line.find ('%', at + 1);
      std::string name, value;
      scr_int type;
      scr_vartype_t vt_value;

      if (end == std::string::npos)
        break;
      name = line.substr (at + 1, end - at - 1);
      if (name == "object")
        value = first_name ("Objects", "Short", gs_object_count (game));
      else if (name == "character")
        value = first_name ("NPCs", "Name", gs_npc_count (game));
      else if (name == "number")
        value = "1";
      else if (name == "text")
        value = "hello";
      else if (var_get (vars, name.c_str (), &type, &vt_value))
        value = type == VAR_INTEGER ? std::to_string (vt_value.integer)
                                    : std::string (vt_value.string);
      else
        value = name;
      line.replace (at, end - at + 1, value);
      at += value.size ();
    }
  return line;
}

/*
 * run_census_t
 *
 * One lenient census: the game and what every pass reads from it, the
 * matcher and the Runner's splitter both halves use, the counts the
 * summary prints, and the cross half's per-task record -- how many verb
 * lines woke the task, the first of them with its spelling, and the
 * reason.
 */
namespace {

static const scr_char *const REASONS[] = {
  "separator", "spelling", "lenient"
};

struct run_census_t
{
  scr_gameref_t game;
  scr_prop_setref_t bundle;
  scr_int version, task_count;
  scr_int woken, reachable, dead, crossed;
  scr_int by_reason[3];

  std::vector<scr_int> cross_lines, cross_via;
  std::vector<std::string> cross_first, cross_ran;

  explicit run_census_t (scr_gameref_t game_);

  void mark_all_seen ();
  scr_bool matches (scr_int which, const std::string &line,
                    scr_bool lenient) const;
  scr_bool splits (const std::string &line) const;
  std::vector<std::string> cross_names () const;
  void cross_line (const scr_char *verb, const std::string &name);
  void report_cross () const;
  void summary () const;
};

/*
 * run_census_task_t
 *
 * One task of the wake half: the lines a player could type for it and the
 * commands they came from, then what probing them found -- whether some
 * line reaches the task the Runner's way, else the first line that reaches
 * it Scarier's way, its spelling and the reason.
 */
struct run_census_task_t
{
  run_census_t &census;
  scr_int task;
  std::vector<std::string> lines, commands;
  scr_bool runner_alive;
  std::string woke_line, woke_ran;
  scr_int woke_reason;

  run_census_task_t (run_census_t &census_, scr_int task_)
    : census (census_), task (task_), runner_alive (FALSE),
      woke_reason (-1) {}

  void gather ();
  void probe ();
  void report ();
};


run_census_t::run_census_t (scr_gameref_t game_)
  : game (game_), bundle (gs_get_bundle (game_)),
    version (prop_get_taf_version (bundle)),
    task_count (gs_task_count (game_)),
    woken (0), reachable (0), dead (0), crossed (0), by_reason {0, 0, 0},
    cross_lines (task_count, 0), cross_via (task_count, -1),
    cross_first (task_count), cross_ran (task_count)
{
}


/*
 * run_census_t::mark_all_seen()
 *
 * Every object and character seen, so that the seen gate does not stand in
 * for the restrictions the census ignores; and strict matching to start.
 */
void
run_census_t::mark_all_seen ()
{
  scr_int index_;

  GS_FOR_EACH_OBJECT (game, index_)
    gs_set_object_seen (game, index_, TRUE);
  GS_FOR_EACH_NPC (game, index_)
    gs_set_npc_seen (game, index_, TRUE);
  uip_set_lenient_tasks (FALSE);
}


/*
 * run_census_t::matches()
 * run_census_t::splits()
 *
 * Does task 'which' take 'line', strictly or leniently; and does the Runner
 * split 'line' at a separator.
 */
scr_bool
run_census_t::matches (scr_int which, const std::string &line,
                       scr_bool lenient) const
{
  const scr_lenient_tasks_guard lenient_tasks (lenient);

  return run_census_matches_task (game, which, line.c_str ());
}

scr_bool
run_census_t::splits (const std::string &line) const
{
  scr_int unused = 0;

  if (version >= TAF_VERSION_400)
    return run_find_split_400 (game, line.c_str (), &unused) >= 0;
  return run_find_split_pre400 (version, line.c_str (), &unused,
                                version < TAF_VERSION_390
                                && run_comma_splits_pre390
                                     (game, line.c_str ())) >= 0;
}


/*
 * run_census_task_t::gather()
 *
 * The lines for the task: each command pattern, forwards and reverse,
 * expanded, with %object% as each of three names.
 */
void
run_census_task_t::gather ()
{
  const scr_gameref_t game = census.game;
  const scr_task_commands_guard task_commands;

  for (const scr_bool forwards : {scr_bool (TRUE), scr_bool (FALSE)})
    for (const scr_char *pattern :
         run_task_command_patterns (game, task, forwards))
      {
        if (pattern[strspn (pattern, WHITESPACE)] == SPECIAL_PATTERN)
          continue;
        commands.push_back (pattern);
        for (scr_int pick = 0; pick < 3; pick++)
          {
            const std::string text
                = run_census_references (game, pattern, pick);
            size_t at = 0;

            for (const std::string &line :
                 run_census_expand (text, at, NUL))
              {
                const std::string typed
                    = run_census_normal (line.c_str ());

                if (!typed.empty ())
                  lines.push_back (typed);
              }
            if (text == pattern)
              break;
          }
      }
}


/*
 * run_census_task_t::probe()
 *
 * Each line the Runner's way, then Scarier's: a strict match on the
 * Runner's spelling of an unsplit line keeps the task reachable and ends
 * the probe; otherwise the first line Scarier's spelling, or a lenient
 * match no other task pre-empts, gives the task is kept with its reason.
 */
void
run_census_task_t::probe ()
{
  const scr_gameref_t game = census.game;
  const scr_prop_setref_t bundle = census.bundle;
  const scr_int task_count = census.task_count;

  for (const std::string &typed : lines)
    {
      run_dispatch_input = typed.c_str ();

      scr_owned_string runner_filtered (pf_filter_input (typed.c_str (),
                                                         bundle));
      const std::string runner_line = run_census_normal
          (runner_filtered ? runner_filtered.get () : typed.c_str ());
      const scr_bool runner_split = census.splits (typed);

      if (!runner_split && census.matches (task, runner_line, FALSE))
        {
          runner_alive = TRUE;
          break;
        }
      if (woke_reason >= 0)
        continue;

      const scr_bool our_split
          = runner_split && !run_separator_task_line (game, typed.c_str ());
      if (our_split)
        continue;
      strncpy (run_line_element, typed.c_str (), LINE_BUFFER_SIZE - 1);
      run_line_element[LINE_BUFFER_SIZE - 1] = NUL;
      scr_owned_string ours (run_element_filtered (game));
      const std::string our_line = run_census_normal
          (ours ? ours.get () : typed.c_str ());
      scr_bool lenient = TRUE;
      scr_int other;

      for (other = 0; other < task_count && lenient; other++)
        if (other != task && census.matches (other, our_line, FALSE))
          lenient = FALSE;
      if (census.matches (task, our_line, FALSE)
          || (lenient && census.matches (task, our_line, TRUE)))
        {
          woke_line = typed;
          woke_ran = our_line;
          woke_reason = runner_split ? 0 : our_line != runner_line ? 1 : 2;
        }
    }
  run_dispatch_input = nullptr;
}


/*
 * run_census_task_t::report()
 *
 * Count the task as reachable, dead or woken, and print the woken line.
 */
void
run_census_task_t::report ()
{
  if (runner_alive)
    census.reachable++;
  else if (woke_reason < 0)
    {
      census.dead++;
      if (getenv ("SCR_CENSUS_DEAD"))
        scr_dump_log ("CENSUS dead task=%ld line=\"%s\"\n", task,
              lines[0].c_str ());
    }
  else
    {
      std::string joined;

      for (const std::string &command : commands)
        joined += (joined.empty () ? "" : " | ") + command;
      census.woken++;
      census.by_reason[woke_reason]++;
      scr_dump_log ("CENSUS woken task=%ld via=%s line=\"%s\""
            " ran=\"%s\" cmds={%s}\n", task, REASONS[woke_reason],
            woke_line.c_str (), woke_ran.c_str (),
            joined.substr (0, 400).c_str ());
    }
}


/*
 * Cross wakes: library verbs on every name an object or character
 * answers to.  A line the Runner hands to the library (no task matches
 * its spelling strictly) that Scarier gives to a task -- through its
 * spelling or leniently -- is printed once per task:
 *
 *   CENSUS cross task=<n> via=<spelling|lenient> lines=<count>
 *          line="<first line>" ran="<spelling>" cmds={<commands>}
 *
 * Candidates only: the library handlers that claim a line before the
 * lenient retry (a take of a static, a put) are not modelled, so check a
 * hit in play with SCR_TRACE_DEVIATIONS.  ShadricksUnderground's ` * in *`
 * is a real one -- `look in black jar` in the cellar enters the tunnel --
 * and Glum Fiddle's `get the gatehouse` is not.  SCR_CENSUS_NOCROSS skips
 * this half, which is most of the run time; SCR_CENSUS_CROSS_LINES prints
 * every hit as `CENSUS crossline task=<n> line="<line>"`, which
 * probe_lenient.sh plays for real along the walkthroughs.
 */
static const scr_char *const VERBS[] = {
  "get", "take", "drop", "examine", "x", "look at", "open", "close",
  "wear", "remove", "take off", "push", "pull", "eat", "drink", "read",
  "sit on", "stand on", "lie on", "talk to", "ask", "give", "hit", "kill",
  "attack", "put", "turn", "move", "search", "climb", "enter", "unlock",
  "lock", "wield", "use", "go", "throw", nullptr
};

/*
 * run_census_t::cross_names()
 *
 * Every name an object or character answers to: its Short or Name, bare,
 * with "the" and with its Prefix, and each Alias.
 */
std::vector<std::string>
run_census_t::cross_names () const
{
  std::vector<std::string> names;
  scr_int index_;

  for (const scr_char *category : {"Objects", "NPCs"})
    {
      const scr_bool is_object = category[0] == 'O';
      const scr_int count = is_object ? gs_object_count (game)
                                      : gs_npc_count (game);

      for (index_ = 0; index_ < count; index_++)
        {
          const scr_char *name = prop_get_indexed_string
              (bundle, category, index_, is_object ? "Short" : "Name");
          const scr_char *prefix = prop_get_indexed_string
              (bundle, category, index_, "Prefix");
          scr_vartype_t vt_key[4];
          scr_int alias, aliases;

          if (!scr_strempty (name))
            {
              names.push_back (name);
              names.push_back (std::string ("the ") + name);
              if (!scr_strempty (prefix))
                names.push_back (std::string (prefix) + " " + name);
            }
          vt_key[0].string = category;
          vt_key[1].integer = index_;
          vt_key[2].string = "Alias";
          aliases = prop_get_child_count (bundle, "I<-sis", vt_key);
          for (alias = 0; alias < aliases; alias++)
            {
              vt_key[3].integer = alias;
              const scr_char *text = prop_get_string (bundle, "S<-sisi",
                                                      vt_key);
              if (!scr_strempty (text))
                names.push_back (text);
            }
        }
    }
  return names;
}


/*
 * run_census_t::cross_line()
 *
 * One verb on one name: a line no task takes the Runner's way, that some
 * task takes Scarier's way, is counted against that task.
 */
void
run_census_t::cross_line (const scr_char *verb, const std::string &name)
{
  const std::string typed
      = run_census_normal ((std::string (verb) + " " + name).c_str ());
  scr_bool strict = FALSE;
  scr_int other, via = -1, woke = -1;

  if (splits (typed) && !run_separator_task_line (game, typed.c_str ()))
    return;
  run_dispatch_input = typed.c_str ();
  scr_owned_string runner_filtered (pf_filter_input (typed.c_str (),
                                                     bundle));
  const std::string runner_line = run_census_normal
      (runner_filtered ? runner_filtered.get () : typed.c_str ());

  for (other = 0; other < task_count && !strict; other++)
    strict = matches (other, runner_line, FALSE);
  if (strict || splits (typed))
    {
      run_dispatch_input = nullptr;
      return;
    }

  strncpy (run_line_element, typed.c_str (), LINE_BUFFER_SIZE - 1);
  run_line_element[LINE_BUFFER_SIZE - 1] = NUL;
  scr_owned_string ours (run_element_filtered (game));
  const std::string our_line = run_census_normal
      (ours ? ours.get () : typed.c_str ());
  run_dispatch_input = nullptr;

  for (other = 0; other < task_count && woke < 0; other++)
    if (matches (other, our_line, FALSE))
      woke = other, via = 1;
  for (other = 0; other < task_count && woke < 0; other++)
    if (matches (other, our_line, TRUE))
      woke = other, via = 2;
  if (woke < 0)
    return;
  if (getenv ("SCR_CENSUS_CROSS_LINES"))
    scr_dump_log ("CENSUS crossline task=%ld line=\"%s\"\n", woke,
          typed.c_str ());
  if (cross_lines[woke]++ == 0)
    {
      crossed++;
      cross_first[woke] = typed;
      cross_ran[woke] = our_line;
      cross_via[woke] = via;
    }
}


/*
 * run_census_t::report_cross()
 * run_census_t::summary()
 *
 * The CENSUS cross line for each task some verb line woke, then the totals.
 */
void
run_census_t::report_cross () const
{
  scr_int task;

  for (task = 0; task < task_count; task++)
    if (cross_lines[task] > 0)
      {
        std::string joined;
        const scr_task_commands_guard task_commands;

        for (const scr_bool forwards : {scr_bool (TRUE), scr_bool (FALSE)})
          for (const scr_char *pattern :
               run_task_command_patterns (game, task, forwards))
            joined += (joined.empty () ? "" : " | ") + std::string (pattern);
        scr_dump_log ("CENSUS cross task=%ld via=%s lines=%ld"
              " line=\"%s\" ran=\"%s\" cmds={%s}\n", task,
              REASONS[cross_via[task]], cross_lines[task],
              cross_first[task].c_str (), cross_ran[task].c_str (),
              joined.substr (0, 400).c_str ());
      }
}

void
run_census_t::summary () const
{
  scr_dump_log ("CENSUS summary tasks=%ld runner_reachable=%ld dead=%ld"
        " woken=%ld separator=%ld spelling=%ld lenient=%ld crossed=%ld\n",
        task_count, reachable, dead, woken, by_reason[0], by_reason[1],
        by_reason[2], crossed);
  scr_dump_flush ();
}

} /* namespace */


/*
 * run_lenient_census()
 *
 * The census: the wake half, one task at a time, then the cross half over
 * every library verb and name, then the summary.
 */
static void
run_lenient_census (scr_gameref_t game)
{
  run_census_t census (game);
  std::vector<std::string> names;
  scr_int task;

  census.mark_all_seen ();
  for (task = 0; task < census.task_count; task++)
    {
      run_census_task_t one (census, task);

      one.gather ();
      if (one.lines.empty ())
        continue;
      one.probe ();
      one.report ();
    }

  names = census.cross_names ();
  if (getenv ("SCR_CENSUS_NOCROSS"))
    names.clear ();
  for (const scr_char *const *verb = VERBS; *verb; verb++)
    for (const std::string &name : names)
      census.cross_line (*verb, name);
  census.report_cross ();
  census.summary ();
}

/*
 * run_meta_census()
 *
 * SCR_CENSUS_META: the tasks that take a line one of SCARE's own meta
 * commands answers -- the words no Runner has, which a task can therefore
 * lose to us whenever it does not claim the line itself (restrictions that
 * fail without a message, a task with nothing to say, a task already done).
 */
static void
run_meta_census (scr_gameref_t game)
{
  static const scr_char *const WORDS[] = {
    "verbose", "brief", "notify", "notification", "notify on", "notify off",
    "hints", "hist", "hist 1", "redo", "redo 1", "gpl", "license",
    "statusline", "stats", "q", "g", "i", "z", nullptr
  };
  run_census_t census (game);

  census.mark_all_seen ();
  for (scr_int task = 0; task < census.task_count; task++)
    {
      /* A task that takes nonsense too is a catch-all, not a claim on the
         word. */
      if (census.matches (task, "qzxqzx", TRUE)
          || census.matches (task, "qzxqzx 1", TRUE))
        continue;

      for (const scr_char *const *word = WORDS; *word; word++)
        {
          const scr_bool strict = census.matches (task, *word, FALSE);

          if (strict || census.matches (task, *word, TRUE))
            scr_trace ("CENSUS meta word=\"%s\" task=%ld %s\n", *word,
                       (long) task, strict ? "strict" : "lenient");
        }
    }
}

scr_bool
run_census_requested (scr_gameref_t game)
{
  if (getenv ("SCR_CENSUS_META"))
    {
      run_meta_census (game);
      return TRUE;
    }
  if (getenv ("SCR_CENSUS_LENIENT") == nullptr)
    return FALSE;
  run_lenient_census (game);
  return TRUE;
}
#endif

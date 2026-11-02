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
 * Line splitting helpers and the version gate shared by the runner.
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
 * run_counts_line_elements()
 *
 * TRUE if the game's turn counter advances once per input line element as
 * it is read, rather than once per completed non-administrative turn.  3.9
 * and 3.8 do this; see run_player_input().  run380 adds one to its counter
 * MemVar_44F138 at the top of generaltasks (441A21), where the jump back for
 * the next `then` element (443453) also lands, so its `turns` counts itself:
 * `look probe clear cls clr turns` answers 6 (p38ADMIN,
 * runner_probes/admin.run380.rtf).  run370 has no counter and no `turns`;
 * run400's is not measured apart from its administrative turns.
 */
scr_bool
run_counts_line_elements (scr_gameref_t game)
{
  const scr_int version = prop_get_taf_version (gs_get_bundle (game));

  return version >= TAF_VERSION_380 && version < TAF_VERSION_400;
}


/*
 * run_then_in_line_pre400()
 * run_find_split_pre400()
 *
 * The pre-4.0 splitters.  run370 43B29B and run380 441A4B have one test,
 * c("then") at the top of generaltasks: when the line holds "then" as a
 * word, it is cut at the first InStr "then" -- a substring, so `x athens
 * then look` runs "x a", "s" and "look" -- the head less one trailing
 * space, the tail less one leading space and run as the next command.
 * There is no comma and no ". " pass, so `x stone, look` is one command.
 * run390 (45EC8E-45F091) cuts at the first ",", then in what is left at the
 * first ". ", then -- if c("then") holds on that -- at the first "then",
 * each tail queued ahead of the rest and re-split when it is read back, so
 * the earliest surviving kind is the cut.  No Runner below 4.0 cuts at a
 * period with no space after it: `look.` is one command.
 *
 * c() (run370 423C80, run380 429048, run390 4334B0): the first InStr hit
 * that starts the text or follows a space decides; it is a word if it runs
 * to the end or is followed by a space or "," -- or "." from 3.9.
 *
 * Measured 2026-09-19 on p38ASK / p39ASK (harness/make_38_askprobe.py,
 * make_39_askprobe.py; run380x runner_probes/ask.run380.split.rtf,
 * run390x runner_probes/ask.run390.split.txt).
 */
static scr_bool
run_then_in_line_pre400 (const scr_char *line, scr_int length,
                         scr_bool period_ends)
{
  scr_int posn;

  for (posn = 0; posn + 4 <= length; posn++)
    {
      if (strncmp (line + posn, "then", 4) != 0)
        continue;
      if (posn == 0 || line[posn - 1] == ' ')
        {
          const scr_int after = posn + 4;

          return after == length || line[after] == ' ' || line[after] == ','
                 || (period_ends && line[after] == '.');
        }
    }
  return FALSE;
}

/*
 * Return the length of the head of LINE, with *TAIL the offset its tail
 * starts at, or -1 when the line holds no cut.
 */
scr_int
run_find_split_pre400 (scr_int version, const scr_char *line, scr_int *tail,
                       scr_bool comma_splits)
{
  const scr_char *found;
  scr_int head = (scr_int) strlen (line), cut = -1, sep_length = 0;

  /* 3.7/3.8 commas are a deviation; see run_comma_splits_pre390(). */
  if (version >= TAF_VERSION_390 || comma_splits)
    {
      found = strchr (line, ',');
      if (found)
        {
          head = cut = found - line;
          sep_length = 1;
        }
    }
  if (version >= TAF_VERSION_390)
    {
      for (found = line; found - line + 1 < head; found++)
        {
          if (found[0] == '.' && found[1] == ' ')
            {
              head = cut = found - line;
              sep_length = 2;
              break;
            }
        }
    }

  if (run_then_in_line_pre400 (line, head, version >= TAF_VERSION_390))
    {
      for (found = line; strncmp (found, "then", 4) != 0; found++)
        ;
      cut = found - line;
      sep_length = 4;
    }

  if (cut < 0)
    return -1;
  *tail = cut + sep_length + (line[cut + sep_length] == ' ' ? 1 : 0);
  return cut;
}

/*
 * run_empty_then_head_390()
 *
 * run390's "then" pass ends at 45F079: `If line = "" Then line = queue :
 * queue = ""`.  An empty head -- `then look` -- is replaced by everything
 * queued behind it, and that command is not split again: the queue is the
 * then-tail, the ". "-tail and the comma-tail, each less one leading space,
 * joined with ", " (45EDEF, 45EF5C).  So `then look` is one `look`, and
 * `look then then look` runs `look` twice
 * (runner_probes/ask.run390.split2.txt).  run370/run380 have no such test
 * and answer the empty head with DontUnderstand.
 */
std::string
run_empty_then_head_390 (const scr_char *line)
{
  const scr_char *comma, *stop;
  std::string queue;
  scr_int head;

  comma = strchr (line, ',');
  head = comma ? comma - line : (scr_int) strlen (line);
  stop = line + head;
  for (const scr_char *dot = line; dot + 1 < line + head; dot++)
    {
      if (dot[0] == '.' && dot[1] == ' ')
        {
          stop = dot;
          break;
        }
    }

  auto append = [&queue] (const scr_char *from, const scr_char *to)
    {
      std::string text (from, to - from);

      if (!text.empty () && text[0] == ' ')
        text.erase (0, 1);
      if (text.empty ())
        return;
      if (!queue.empty ())
        queue += ", ";
      queue += text;
    };

  append (line + 4, stop);
  if (stop < line + head)
    append (stop + 2, line + head);
  if (comma)
    append (comma + 1, comma + strlen (comma));
  return queue;
}


/*
 * run_split_word_names_object()
 * run_find_split_400()
 *
 * 4.0's own input splitter.  run400 generaltasks calls Proc_19_60_459764
 * four times over, on "," then ". " then " and " then " then " (48A0DA,
 * 48A0E8, 48A0F6, 48A104), each pass working on what the pass before it
 * left, and each cut queueing its tail (MemVar_4942E4) to be read back as
 * the next command.  The catch is the loop at 459525: before it cuts, the
 * splitter takes the FIRST WORD of the tail (Proc_19_59_449980, leading
 * spaces stripped) and walks the whole object table, comparing that word
 * against every object's Short (field 4), every space-separated word of its
 * Prefix (field 0, Split on " ") and every one of its Aliases (field 8).  A
 * hit sends it round to look for the NEXT occurrence of the same separator
 * instead -- so a separator followed by something the game calls an object
 * is not a separator at all, and the line stays whole.
 *
 * That is what makes `get coin and hat` one command that takes both, while
 * `get coin and zzz` is two ("You take the coin." then the DontUnderstand
 * text); the same test covers commas, so `drop coin, hat` is one command
 * too.  The table walk has NO scope test -- an object two rooms away
 * suppresses just as well (`x hat and coin` typed in the empty second room
 * of the probe is one command) -- and the comparisons are VB's binary `=`,
 * so they are case-SENSITIVE against the already-lower-cased line: the
 * probe's alias "Widget" does NOT suppress the typed "widget", even though
 * the object resolver matches it.
 *
 * Measured 2026-09-08 on the hand-built p4AND probe (make_400_andprobe.py;
 * transcripts runner_probes/and.run400.txt and
 * runner_probes/and.run400.b.txt, 41 cells).  `x coin and hat and zzz` cuts
 * at the SECOND " and " (the first is suppressed by "hat"); `drop coin and
 * hat, x box` cuts at the comma, the earliest cut of any kind that survives
 * suppression; `wave zzz and yyy` is cut in two and the task named by the
 * whole line never fires; `put coin in box and put hat in desk` is cut
 * ("put" is not an object) into two turns, where `put coin in box and hat
 * in desk` is one turn through put_drop_list's own clause loop
 * (lib_put_clauses_400).
 *
 * Pre-4.0 Runners split on far less and never consult the object table;
 * see run_find_split_pre400() above.
 */
static scr_bool
run_split_word_names_object (scr_gameref_t game, const scr_char *word)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int count, object;

  if (word[0] == NUL)
    return FALSE;

  count = gs_object_count (game);
  for (object = 0; object < count; object++)
    {
      const scr_char *shortname, *prefix, *cursor;
      scr_vartype_t vt_key[4];
      scr_int alias_count, alias;

      shortname = prop_get_indexed_string (bundle, "Objects", object, "Short");
      if (shortname && strcmp (shortname, word) == 0)
        return TRUE;

      /* Split(Prefix, " ") -- each word of the prefix on its own. */
      prefix = prop_get_indexed_string (bundle, "Objects", object, "Prefix");
      for (cursor = prefix; cursor && *cursor != NUL; )
        {
          const scr_char *space = strchr (cursor, ' ');
          const size_t length = space ? (size_t) (space - cursor)
                                      : strlen (cursor);

          if (length == strlen (word) && strncmp (cursor, word, length) == 0)
            return TRUE;
          if (!space)
            break;
          cursor = space + 1;
        }

      vt_key[0].string = "Objects";
      vt_key[1].integer = object;
      vt_key[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (alias_name && alias_name[0] != NUL
              && strcmp (alias_name, word) == 0)
            return TRUE;
        }
    }
  return FALSE;
}

/*
 * Return the offset of the effective cut in LINE, with *SEP_LENGTH the
 * length of the separator there, or -1 when the line holds no cut.  The
 * four kinds are tried in the Runner's PASS order, not by position:
 * generaltasks calls the splitter once per separator (48A0DA "," then
 * 48A0E8 ". ", 48A0F6 " and ", 48A104 " then "), and each pass works on
 * the head the previous pass left, so a later kind can only shorten the
 * head.  The queue the Runner keeps is its tails joined ", ", the later
 * pass's tail first (459742-459753), which reads back exactly as the rest
 * of the line does here once the caller strips the whitespace after the
 * separator.  Measured on p4AND (run400 runner_probes/and.run400.b.txt):
 * `x coin and box, x hat` is "x coin and box" then "x hat", the comma cut
 * first and the " and " then suppressed by "box"; the earliest-position
 * order Scarier used cut at " and " because "box," carries the comma and
 * names nothing.
 */
scr_int
run_find_split_400 (scr_gameref_t game, const scr_char *line,
                    scr_int *sep_length)
{
  static const scr_char *const SEPARATORS[] = {",", ". ", " and ", " then "};

  scr_int best = -1;
  size_t kind;

  for (kind = 0; kind < sizeof (SEPARATORS) / sizeof (*SEPARATORS); kind++)
    {
      const scr_char *const separator = SEPARATORS[kind];
      const size_t length = strlen (separator);
      const scr_int limit = (best < 0) ? (scr_int) strlen (line) : best;
      scr_int posn;

      /*
       * Position 0 is never a cut here: the element loop below always takes
       * the first character of the line, so that input like "." is one
       * parser complaint rather than two empty commands.  Only the current
       * head, before any earlier pass's cut, is searched.
       */
      for (posn = 1; posn < limit && line[posn] != NUL; posn++)
        {
          const scr_char *tail;
          scr_char word[LINE_BUFFER_SIZE];
          size_t extent;

          if (strncmp (line + posn, separator, length) != 0)
            continue;

          /*
           * first_word() of the tail, leading spaces stripped (449980):
           * trailing punctuation stays on the word, so "box," names
           * nothing -- but the tail ends where the current head does,
           * since an earlier pass's cut has already taken the rest of the
           * line (and its comma) off to the queue.
           */
          tail = line + posn + length;
          tail += strspn (tail, " ");
          extent = strcspn (tail, " ");
          if (tail + extent > line + limit)
            extent = (tail < line + limit) ? (line + limit) - tail : 0;
          if (extent >= sizeof (word))
            extent = sizeof (word) - 1;
          memcpy (word, tail, extent);
          word[extent] = NUL;

          if (run_split_word_names_object (game, word))
            continue;

          best = posn;
          *sep_length = (scr_int) length;
          break;
        }
    }
  if (best >= 0)
    return best;

  /*
   * A period at the very end of the line is not one of the Runner's four
   * separators -- it has no space after it -- but cutting there costs
   * nothing (the tail is empty) and keeps "n." typed by a walkthrough
   * working exactly as it always has.
   */
  {
    const size_t length = strlen (line);

    if (length > 1 && line[length - 1] == '.')
      {
        *sep_length = 1;
        return (scr_int) length - 1;
      }
  }
  return -1;
}


/*
 * run_get_version()
 *
 * Return the game's TAF version from the bundle's top-level "Version"
 * property -- a TAF_VERSION_* value, set once at parse time.
 */
scr_int
run_get_version (const scr_prop_setref_t bundle)
{
  return prop_get_taf_version (bundle);
}


/*
 * run_squeeze_spaces()
 *
 * Copy 'string' into 'buffer' with every space dropped.  The 4.0 Runner
 * squeezes the whole command this way before it looks for a task command
 * function, which is what makes the spacing in the function free-form.
 */
void
run_squeeze_spaces (const scr_char *string, scr_char *buffer)
{
  const scr_char *cursor;
  scr_char *out;

  for (cursor = string, out = buffer; *cursor != NUL; cursor++)
    {
      if (*cursor != ' ')
        *out++ = *cursor;
    }
  *out = NUL;
}


/*
 * run_instr()
 *
 * Case-insensitive InStr: the offset of the first WORD in LINE, or -1.
 */
scr_int
run_instr (const scr_char *line, const scr_char *word)
{
  const size_t length = strlen (word);
  const scr_char *scan;

  for (scan = line; *scan != NUL; scan++)
    {
      if (scr_strncasecmp (scan, word, length) == 0)
        return scan - line;
    }
  return -1;
}

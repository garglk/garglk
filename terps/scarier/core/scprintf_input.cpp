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
 * The input side of the printfilter: tag stripping, escaping, the word
 * rewrites and the synonym table as the Runners apply them, and
 * pf_filter_input().
 */

#include "scprintf_internal.h"

/*
 * pf_strip_tags_common()
 *
 * Strip HTML-like tags from a string.  Used to process strings used in ways
 * other than being passed to if_print_string(), for example room names and
 * status lines.  It ignores all tags except <br>, which it replaces with
 * a newline if requested by allow_newlines.
 */
static void
pf_strip_tags_common (scr_char *string, scr_bool allow_newlines)
{
  scr_char *marker, *cursor;

  /* Run through the string looking for <...> tags. */
  marker = string;
  for (cursor = (scr_char *) strchr (marker, LESSTHAN);
       cursor; cursor = (scr_char *) strchr (marker, LESSTHAN))
    {
      scr_char *tag_end;

      /* Locate tag end, and break if unterminated. */
      tag_end = strchr (cursor, GREATERTHAN);
      if (!tag_end)
        break;

      /* If the tag is <br>, replace with newline if requested. */
      if (allow_newlines && tag_end - cursor == 3
          && scr_strncasecmp (cursor + 1, "br", 2) == 0)
        *cursor++ = '\n';

      /* Remove the tag from the string, then advance input. */
      memmove (cursor, tag_end + 1, strlen (tag_end));
      marker = cursor;
    }
}


/*
 * pf_strip_tags()
 * pf_strip_tags_for_hints()
 *
 * Public interfaces to pf_strip_tags_common().  The hints version will
 * allow <br> tags to map into newlines in hints strings.
 */
void
pf_strip_tags (scr_char *string)
{
  pf_strip_tags_common (string, FALSE);
}

void
pf_strip_tags_for_hints (scr_char *string)
{
  pf_strip_tags_common (string, TRUE);
}


/*
 * pf_escape()
 *
 * Escape <, >, and % characters in the input string.  Used to filter player
 * input prior to storing in referenced text.
 *
 * Adrift offers no escapes for & and +, so for these we convert to the
 * character itself followed by a space.  The return string is malloc'ed, so
 * the caller needs to remember to free it.
 */
scr_char *
pf_escape (const scr_char *string)
{
  const scr_char *marker, *cursor;
  std::string buffer;

  /* Run through the string looking for <, >, %, or other escapes. */
  marker = string;
  for (cursor = marker + strcspn (marker, ESCAPES);
       cursor[0] != NUL; cursor = marker + strcspn (marker, ESCAPES))
    {
      const scr_char *escape;
      scr_char escape_buffer[3];

      /* Extend buffer to hold the string so far. */
      if (cursor > marker)
        buffer.append (marker, cursor - marker);

      /* Determine the appropriate character escape. */
      if (cursor[0] == LESSTHAN)
        escape = ENTITY_LESSTHAN;
      else if (cursor[0] == GREATERTHAN)
        escape = ENTITY_GREATERTHAN;
      else if (cursor[0] == PERCENT)
        escape = ENTITY_PERCENT;
      else
        {
          /*
           * No real escape available, so fake, badly, by appending a space
           * for cases where we've encountered a character entity; leave
           * others untouched.
           */
          escape_buffer[0] = cursor[0];
          if (scr_strncasecmp (cursor,
                              ENTITY_LESSTHAN, ENTITY_LENGTH) == 0
              || scr_strncasecmp (cursor,
                                 ENTITY_GREATERTHAN, ENTITY_LENGTH) == 0
              || scr_strncasecmp (cursor,
                                 ENTITY_PERCENT, PERCENT_LENGTH) == 0)
            {
              escape_buffer[1] = ' ';
              escape_buffer[2] = NUL;
            }
          else
            escape_buffer[1] = NUL;
          escape = escape_buffer;
        }

      buffer.append (escape);

      /* Pass over character escaped and continue. */
      cursor++;
      marker = cursor;
    }

  /* Add all remaining characters to the buffer. */
  if (cursor > marker)
    buffer.append (marker, cursor - marker);

  return scr_strdup (buffer);
}


/*
 * pf_compare_words()
 *
 * Matches multiple words from words in string.  Returns the extent of
 * the match if the string matched, 0 otherwise.
 */
static scr_int
pf_compare_words (const scr_char *string, const scr_char *words)
{
  scr_int word_posn, posn;

  /* None expected, but skip leading space. */
  word_posn = 0;
  while (scr_isspace (words[word_posn]) && words[word_posn] != NUL)
    word_posn++;

  /* Match characters from words with the string at position. */
  posn = 0;
  while (TRUE)
    {
      /* Any character mismatch means no words match. */
      if (scr_tolower (words[word_posn]) != scr_tolower (string[posn]))
        return 0;

      /* Move to next character in each. */
      word_posn++;
      posn++;

      /*
       * If at space, advance over whitespace in words list.  Stop when we
       * hit the end of the words list.
       */
      while (scr_isspace (words[word_posn]) && words[word_posn] != NUL)
        word_posn++;
      if (words[word_posn] == NUL)
        break;

      /*
       * About to match another word, so advance over whitespace in the
       * current string too.
       */
      while (scr_isspace (string[posn]) && string[posn] != NUL)
        posn++;
    }

  /*
   * We reached the end of words.  If we're at the end of the match string,
   * or at spaces, we've matched.
   */
  if (scr_isspace (string[posn]) || string[posn] == NUL)
    return posn;

  /* More text after the match, so it's not quite a match. */
  return 0;
}


/*
 * pf_rewrite_whole_words()
 *
 * One whole-string rewrite pass: replace every whole-word occurrence of
 * original in the current input with replacement.  'buffer' is materialised
 * from 'string' on the first change, 'current' always points at the text
 * being walked, and 'modified' records that buffer is live.
 */
static void
pf_rewrite_whole_words (const scr_char *string, std::string &buffer,
                        scr_bool &modified, const scr_char *&current,
                        const scr_char *original, const scr_char *replacement,
                        size_t replacement_length, const scr_char *what)
{
  scr_int offset;

  /* Walk the current string a word at a time, replacing each match. */
  offset = strspn (current, WHITESPACE);
  while (current[offset] != NUL)
    {
      scr_int extent;

      extent = pf_compare_words (current + offset, original);
      if (extent > 0)
        {
          if (!modified)
            {
              buffer = string;
              modified = TRUE;
            }
          buffer.replace (offset, extent, replacement, replacement_length);
          current = buffer.c_str ();
          offset += replacement_length;

          if (pf.trace)
            scr_trace ("Printfilter: %s \"%s\"\n", what, buffer.c_str ());
        }
      else
        offset += strcspn (current + offset, WHITESPACE);

      offset += strspn (current + offset, WHITESPACE);
    }
}


/*
 * pf_rewrite_substring()
 *
 * One whole-string rewrite pass in VB's `Replace(line, old, new, 1, -1, 0)`
 * shape: every occurrence of original, anywhere, word boundaries or not,
 * the scan resuming past each replacement as Replace's does.  3.90 and 4.00
 * reach the built-in rewrites this way where 3.70/3.80 reach them through
 * change(), which is a loop over the whole-word matcher c().  Replace's
 * compare argument is 0, vbBinaryCompare -- but the Runner has lower-cased
 * the line long before this point (`zog SLAP blip` runs the `zog hit blip`
 * task in all four Runners, p*REW), so the match is folded here.
 */
static void
pf_rewrite_substring (const scr_char *string, std::string &buffer,
                      scr_bool &modified, const scr_char *&current,
                      const scr_char *original, const scr_char *replacement,
                      size_t replacement_length, const scr_char *what)
{
  const size_t original_length = strlen (original);
  size_t offset = 0;

  while (strlen (current) >= original_length
         && offset <= strlen (current) - original_length)
    {
      if (scr_strncasecmp (current + offset, original, original_length) != 0)
        {
          offset++;
          continue;
        }

      if (!modified)
        {
          buffer = string;
          modified = TRUE;
        }
      buffer.replace (offset, original_length, replacement,
                      replacement_length);
      current = buffer.c_str ();
      offset += replacement_length;

      if (pf.trace)
        scr_trace ("Printfilter: %s \"%s\"\n", what, buffer.c_str ());
    }
}


/*
 * pf_find_folded()
 *
 * Case-folded std::string::find(): the position of the first occurrence of
 * word in line at or after from, or npos.  VB InStr(from, line, word, 1),
 * with the LCase() both sides that the Runners' word gates apply first.
 */
static size_t
pf_find_folded (const std::string &line, size_t from, const scr_char *word)
{
  const size_t word_length = strlen (word);

  if (word_length == 0 || word_length > line.size ())
    return std::string::npos;

  for (size_t offset = from; offset + word_length <= line.size (); offset++)
    {
      if (scr_strncasecmp (line.c_str () + offset, word, word_length) == 0)
        return offset;
    }
  return std::string::npos;
}


/*
 * pf_runner_word_gate()
 *
 * The Runner's whole-word gate c(search[, line]) as the synonym loop calls
 * it, all three versions that have one:
 *
 *   run380 429048 (Form1)     run390 4334B0 (Form1)    run400 454CB0 (General)
 *
 * All three LCase() both strings and walk InStr() hits.  A hit COUNTS only
 * where it starts the line or follows a space; a mid-word hit is skipped and
 * the search resumes one character on.  A counting hit is a whole word when
 * it ends the line or is followed by a terminator:
 *
 *   3.80  " " ","          (428FF3)
 *   3.90  " " "," "."      (43340F..433461)
 *   4.00  " " "," "." "?"  (454C07..454C39)
 *
 * The version split that matters is what happens when a counting hit is NOT
 * a whole word.  3.80 and 3.90 answer FALSE there and then -- the FIRST
 * boundary-start hit decides.  Dolg (3.90) maps в->in, and `войти в дом`
 * starts with the `в` of `войти`: that hit is followed by `о`, the gate
 * says FALSE, and the whole-word ` в ` later in the line is never looked
 * at, so the line reaches the tasks intact and task 126 ("Я вошел в дом.")
 * matches (run390 under Wine, runner_probes/dolg.run390.txt 2026-09-25).
 * 4.00 (454C78: `var_98 = var_96 + 1`, then back to 454B51) carries on from
 * the next character, so any whole-word occurrence anywhere passes it.
 *
 * An empty search never passes: 4.00 returns 0 outright (454B0F), and the
 * others' Replace() of an empty original is a no-op anyway.
 */
static scr_bool
pf_runner_word_gate (const std::string &line, const scr_char *word,
                     scr_int version)
{
  const size_t word_length = strlen (word);
  size_t from = 0;

  while (TRUE)
    {
      size_t hit, end;

      hit = pf_find_folded (line, from, word);
      if (hit == std::string::npos)
        return FALSE;

      if (hit == 0 || line[hit - 1] == ' ')
        {
          end = hit + word_length;
          if (end == line.size ()
              || line[end] == ' ' || line[end] == ','
              || (version >= TAF_VERSION_390 && line[end] == '.')
              || (version >= TAF_VERSION_400 && line[end] == '?'))
            return TRUE;

          /* Pre-4.0: the first boundary-start hit is the only one asked. */
          if (version < TAF_VERSION_400)
            return FALSE;
        }

      from = hit + 1;
    }
}


/*
 * pf_replace_binary()
 *
 * VB `Replace(line, from, to, 1, -1, 0)`: every occurrence of from, anywhere,
 * exact byte compare, scanning left to right and resuming past each
 * replacement.  An empty from is a no-op, as Replace's is.  Returns TRUE if
 * anything changed.
 *
 * The binary compare is real: the Runner has lower-cased the typed line
 * long before the synonym loop (scrunner.cpp, run_player_input), but the
 * synonym's Original is used as the author typed it, so an Original with a
 * capital letter in it passes the (LCase'd) gate and then replaces nothing.
 * CS2 (Место преступления 2, 3.90) writes its compass synonyms that way,
 * [СВ] -> [northeast] and friends, and run390 answers a typed "св" with
 * the game's own "Да?" (Wine, runner_transcripts/cs2.txt, 2026-09-25); the
 * port keeps the Runner's shape rather than guess a kinder one.
 */
static scr_bool
pf_replace_binary (std::string &line, const std::string &from,
                   const std::string &to)
{
  scr_bool changed = FALSE;
  size_t offset = 0;

  if (from.empty ())
    return FALSE;

  while ((offset = line.find (from, offset)) != std::string::npos)
    {
      line.replace (offset, from.size (), to);
      offset += to.size ();
      changed = TRUE;
    }
  return changed;
}


/*
 * pf_lcase()
 *
 * VB LCase() on a std::string, through the locale's tolower table -- the
 * same fold run_player_input() gives the typed line.
 */
static std::string
pf_lcase (const scr_char *text)
{
  std::string folded (text);

  return scr_lowercase (folded);
}


/*
 * pf_v380_verb_name()
 *
 * The 3.80 synonym table does not hold replacement TEXT.  Its Replacement
 * field is the index of a built-in verb, and the Runner's loop (run380
 * 441C11) expands it through Proc_2_24_42CF5C (Module1) -- a 41-entry
 * If-chain, index 0 "Ask" .. 40 "Where", anything else "" -- before
 * LCase()ing the result.  Crime_Adventure.taf (3.80) carries e->6, d->3 and
 * z->37: East, Down and Wait.  A non-numeric Replacement cannot come out of
 * the 3.80 Generator (CByte() of it would fault the Runner); it is passed
 * through untouched here so a hand-edited file still means something.
 */
static std::string
pf_v380_verb_name (const scr_char *replacement)
{
  static const scr_char *const VERBS[] = {
    "Ask", "Attack", "Close", "Down", "Drink", "Drop", "East", "Examine",
    "Find", "Get", "Give", "Goto", "Help", "Hit", "In", "Inventory", "Kick",
    "Kill", "Lie", "Locate", "Look", "North", "Open", "Out", "Pick", "Press",
    "Pull", "Punch", "Push", "Put", "Remove", "Restore", "Sit", "South",
    "Stand", "Take", "Up", "Wait", "Wear", "West", "Where"
  };
  enum { VERBS_SIZE = sizeof (VERBS) / sizeof (VERBS[0]) };
  scr_char *end;
  long index_;

  index_ = strtol (replacement, &end, 10);
  if (end == replacement || *end != NUL)
    return pf_lcase (replacement);

  if (index_ < 0 || index_ >= VERBS_SIZE)
    return "";
  return pf_lcase (VERBS[index_]);
}


/*
 * pf_apply_synonym()
 *
 * One game synonym applied to the typed line the way the loading Runner's
 * synonym loop applies it.  The loops:
 *
 *   run370 -- none.  generaltasks 43CB04 goes straight from its() to the
 *             built-in change()s; the synonyms Scarier synthesises for the
 *             renamed 3.70 commands (|V370_GLOBAL:_Synonyms_|, sctafpar.cpp)
 *             keep the whole-word rewrite they were built on.
 *   run380 441BD7-441C30: change(orig, LCase(verbname(CByte(repl)))).
 *             change() 425634 is `Do While c(orig)`: InStr(1, line, orig,
 *             0) -- the FIRST SUBSTRING hit, binary compare, mid-word or
 *             not, and not necessarily the hit the gate counted -- is
 *             spliced out and the new text spliced in, until c() fails.
 *             The Runner loops for ever when the new text still contains
 *             orig as a whole word; the port stops after a few rounds,
 *             a deliberate deviation.
 *   run390 45F18C-45F206: If c(orig) Then line = Replace(line, orig,
 *             LCase(repl), 1, -1, 0).  Gate once, then substring-replace
 *             EVERY occurrence, letters inside other words included.
 *   run400 48A129-48A2C3: the gate is 454CB0 (any whole-word occurrence);
 *             behind it four steps, each reading the line the previous one
 *             left:
 *               (a) Replace(line, " " & orig & " ", " " & LCase(repl) & " ")
 *               (b) If Left(line, Len(orig) + 1) = orig & " " Then
 *                     line = Replace(line, orig, LCase(repl), 1, 1, 0)
 *               (c) If Right(line, Len(orig) + 1) = " " & orig Then
 *                     line = Left(line, Len(line) - Len(orig)) & repl
 *               (d) If line = orig Then line = repl
 *             (a) is Replace(.., 1, -1, 0), every occurrence; (b) is
 *             Replace(.., 1, 1, 0) (48A204-48A20E: count 1), the leading
 *             word only -- Ghost town (4.00) maps x->examine, and `x box`
 *             must reach the tasks as `examine box`, not `examine
 *             boexamine` (runner_transcripts/ghosttown.txt T25).  (c) and
 *             (d) paste repl in the author's case; (a) and (b) fold it.
 *             Vardock Bates (`hablar con jason` -> `talk con jason jason
 *             dhirco`) and Yak Shaving (`x flags` -> `x clothes line
 *             clothes line`, one `line` fewer than a whole-word-everywhere
 *             rewrite gives) both come out of these four steps as the
 *             run400 transcripts have them.
 */
static void
pf_apply_synonym (std::string &line, const pf_str_pair_t &entry,
                  scr_int version)
{
  const scr_char *original = entry.original;

  switch (version)
    {
    case TAF_VERSION_400:
      {
        const std::string orig (original);
        const std::string folded = pf_lcase (entry.replacement);

        if (!pf_runner_word_gate (line, original, version))
          return;

        pf_replace_binary (line, " " + orig + " ", " " + folded + " ");
        if (line.compare (0, orig.size () + 1, orig + " ") == 0)
          line.replace (0, orig.size (), folded);
        if (line.size () > orig.size ()
            && line.compare (line.size () - orig.size () - 1,
                             orig.size () + 1, " " + orig) == 0)
          line.replace (line.size () - orig.size (), orig.size (),
                        entry.replacement);
        if (line == orig)
          line = entry.replacement;
        return;
      }

    case TAF_VERSION_390:
      if (pf_runner_word_gate (line, original, version))
        pf_replace_binary (line, original, pf_lcase (entry.replacement));
      return;

    case TAF_VERSION_380:
      {
        const std::string orig (original);
        const std::string verb = pf_v380_verb_name (entry.replacement);
        scr_int rounds;

        /* A few rounds is more than a real line needs; see above. */
        for (rounds = 0; rounds < 16; rounds++)
          {
            size_t hit;

            if (!pf_runner_word_gate (line, original, version))
              return;
            hit = line.find (orig);
            if (hit == std::string::npos)
              return;
            line.replace (hit, orig.size (), verb);
          }
        return;
      }

    default:
      {
        /* 3.70 (synthesised synonyms only): the whole-word rewrite. */
        std::string buffer;
        scr_bool modified = FALSE;
        const scr_char *current = line.c_str ();

        pf_rewrite_whole_words (current, buffer, modified, current,
                                original, entry.replacement,
                                entry.replacement_length, "synonym");
        if (modified)
          line = buffer;
        return;
      }
    }
}


/*
 * pf_has_take_off()
 *
 * TRUE if STRING holds the whole words "take off".  Deliberate deviation
 * (2026-09-30): run380's take->get rewrite turns `take off hat` into "get
 * off hat", which is a dismount, so the hat stays on; Scarier leaves a
 * "take off" line to the remove handler.
 */
static scr_bool
pf_has_take_off (const scr_char *string)
{
  const std::string line = pf_lcase (string);
  size_t hit;

  for (hit = line.find ("take off"); hit != std::string::npos;
       hit = line.find ("take off", hit + 1))
    {
      const size_t end = hit + strlen ("take off");

      if ((hit == 0 || scr_isspace (line[hit - 1]))
          && (end == line.size () || scr_isspace (line[end])))
        return TRUE;
    }
  return FALSE;
}


/*
 * pf_apply_synonyms_by_word()
 *
 * The game's synonyms applied as Scarier applied them before the Runner's
 * loops were ported (2026-09-25; see pf_apply_synonym()), kept as a
 * deliberate deviation for the lines the Runner's loops spoil.  The line is
 * walked a word at a time, and the Original is matched as whole words, in
 * any case.  At each word the first synonym that matches fires; a later
 * synonym fires on that text only when its Original is the whole of the
 * replacement, and the walk then resumes after the replacement.  So Lair of
 * the Vampire's harris->steve->harris still ends as "harris", while Vardock
 * Bates' `hablar con jason` becomes "talk con jason dhirco" (the Runner's
 * loops write "jason jason dhirco"), Dolg's `позвонить в звонок` keeps its
 * words whole ("позвонить in звонок", where run390 writes "позinонить in
 * зinонок"), and a synonym whose Original has a capital still fires.
 */
static std::string
pf_apply_synonyms_by_word (const scr_char *string, scr_int version)
{
  std::string buffer (string);
  size_t offset;

  offset = strspn (buffer.c_str (), WHITESPACE);
  while (offset < buffer.size ())
    {
      size_t span = 0;
      size_t index_;

      for (index_ = 0; index_ < pf.synonym.cache.size (); index_++)
        {
          const pf_str_pair_t &entry = pf.synonym.cache[index_];
          scr_int extent;

          extent = pf_compare_words (buffer.c_str () + offset, entry.original);
          if (extent == 0 || (span > 0 && (size_t) extent != span))
            continue;

          const std::string replacement
            = version == TAF_VERSION_380
              ? pf_v380_verb_name (entry.replacement)
              : std::string (entry.replacement);
          buffer.replace (offset, extent, replacement);
          span = replacement.size ();
        }

      if (span > 0)
        offset += span;
      else
        offset += strcspn (buffer.c_str () + offset, WHITESPACE);
      offset += strspn (buffer.c_str () + offset, WHITESPACE);
    }

  return buffer;
}


/*
 * pf_filter_synonyms()
 *
 * The first half of pf_filter_input(): the game's own synonym table.
 *
 * The Runner applies the table as a SEQUENCE OF WHOLE-STRING REWRITES, in
 * table order: synonym 0 rewrites the input, synonym 1 rewrites synonym
 * 0's output, and so on.  A later synonym therefore sees -- and rewrites
 * -- the words an earlier one wrote, and an earlier synonym never sees a
 * later one's.  What ONE rewrite does is per version; see
 * pf_apply_synonym() above for the loops and their gates.
 *
 * Vardock Bates pins the order (run400 under Wine,
 * runner_probes/vardock_bates.run400.txt 2026-08-29).  Its table has
 * hablar->talk [101], then jason->"jason dhirco" [160], then
 * dhirco->"jason dhirco" [161], and the task is
 * [talk]{con}[dhirco/jason/jason dhirco]:
 *
 *   hablar con dhirco        -> talk con jason dhirco          task runs
 *   hablar con jason         -> talk con jason jason dhirco    generic
 *   hablar con jason dhirco  -> talk con jason dhirco jason dhirco  generic
 *   talk con jason dhirco    -> (the same as the line above)   generic
 *
 * Only the spelling that reaches the table AFTER [160] has done its work
 * survives; the other three double the surname and fall to "Nadie escucha
 * tus delirios."  Lair of the Vampire (harris->steve then steve->harris:
 * "ask harris" ends as harris) fits too.
 *
 * The gate is what the whole-word-everywhere rule this replaced (2026-09-25)
 * got wrong, both ways.  Dolg (3.90) maps в->in.  `войти в дом` must
 * reach the tasks untouched -- its first boundary-start `в` is the one
 * inside `войти`, the gate fails there and stops -- where the old rule
 * gave `войти in дом` and no task matched.  `позвонить в звонок` goes
 * the other way: the mid-word hits are skipped, the standalone ` в `
 * passes the gate, and the substring Replace behind it then rewrites
 * every `в`, giving `позinонить in зinонок`, which run390 does not
 * understand; only `дернуть за шнурок` rings the bell
 * (runner_probes/dolg.run390.txt).
 *
 * The compare inside Replace() is binary against the lower-cased line,
 * so an Original with a capital letter never fires.  Measured 2026-09-25
 * on patched copies of two games: run390 with Dolg's инв->inv and
 * себя->me re-spelt `Инв`/`Себя` answers `инв` with "Я не понимаю, что
 * вы хотите!" (runner_probes/dolg.run390.cap.txt); run400 with Vardock
 * Bates' hablar->talk re-spelt `Hablar` leaves `hablar con el taxista`
 * unhandled (runner_probes/vardock_bates.run400.cap.txt) where the real
 * game runs the taxi task.
 */
static void
pf_filter_synonyms (const scr_char *string, scr_prop_setref_t bundle,
                    scr_int version, scr_bool runner_synonyms,
                    std::string &buffer, scr_bool &modified,
                    const scr_char *&current)
{
  scr_vartype_t vt_key;
  scr_int synonym_count, index_;

  /* Obtain a count of synonyms. */
  vt_key.string = "Synonyms";
  synonym_count = prop_get_child_count (bundle, "I<-s", &vt_key);

  /*
   * Resolve the immutable synonym table from the property tree once per game
   * (see scr_pf_state_t); after that the word loop reads it
   * directly, with no prop_get() lookups.  The count is fixed for a game, so a
   * lazy build on first use is safe.
   */
  if (!pf.synonym.built
      || (scr_int) pf.synonym.cache.size () != synonym_count)
    pf_synonym_cache_build (bundle, synonym_count);

  std::string line (string);

  if (!runner_synonyms && version >= TAF_VERSION_380)
    {
      line = pf_apply_synonyms_by_word (string, version);
      synonym_count = 0;
    }

  for (index_ = 0; index_ < synonym_count; index_++)
    {
      const std::string before = line;

      pf_apply_synonym (line, pf.synonym.cache[index_], version);

      if (pf.trace && line != before)
        scr_trace ("Printfilter: synonym \"%s\"\n", line.c_str ());
    }

  modified = (line != string);
  if (modified)
    buffer = line;
  current = modified ? buffer.c_str () : string;
}

/*
 * pf_filter_builtins()
 *
 * The second half of pf_filter_input(): the Runner's own rewrites.
 *
 * The Runner's own built-in rewrites come AFTER the game's synonyms, in
 * this order, each a whole-word change() of every occurrence (change() is
 * a c()-driven loop, case-insensitive):
 *
 *   run370 43B41F everything->all, 43B430 slap->hit
 *   run380 441C3F everything->all, 441C50 slap->hit,
 *          441C61 take->get,       441C72 except->but
 *
 * 3.9 and 4.0 have the same four rewrites as VB `Replace(line, old, new,
 * 1, -1, 0)` calls -- a plain SUBSTRING replace -- and neither touches
 * `take`:
 *
 *   run390 45F225 everything->all, 45F246 slap->hit,
 *          45F267 except->but,     45F288 "apart from"->"but"
 *   run400 48A30F " everything "->" all ", 48A330 " slap "->" hit ",
 *          48A351 " but "->" except ",     48A372 " apart from "->" except "
 *
 * 4.0's literals carry their own spaces, so its rewrites only fire in the
 * MIDDLE of a line and its exclusion word runs the other way, but->except
 * where 3.90 goes except->but.  Measured 2026-09-20 on p37REW/p38REW/
 * p39REW/p4REW (make_rewriteprobe.py; runner_probes/rew.run370.b.rtf,
 * runner_probes/rew.run380.b.rtf, runner_probes/rew.run390.b.txt,
 * runner_probes/rew.run400.b.txt), tasks spelled with the POST-rewrite
 * text:
 *
 *                        3.70      3.80      3.90       4.00
 *   zog slap             ZOGHIT.   ZOGHIT.   ZOGHIT.    DontUnderstand
 *   zog slap blip        ZOGHITB.  ZOGHITB.  ZOGHITB.   ZOGHITB.
 *   zog unslap           no        no        ZOGUNHIT.  no
 *   zog exception        no        no        ZOGBUTION. no
 *   zog except           no        ZOGBUT.   ZOGBUT.    no
 *   zog except blip      ZOGEXCB.  ZOGBUTB.  ZOGBUTB.   ZOGEXCB.
 *   zog but blip         ZOGBUTB.  ZOGBUTB.  ZOGBUTB.   ZOGEXCB.
 *   zog apart from blip  no        no        ZOGBUTB.   ZOGEXCB.
 *   zog everything       ZOGALL.   ZOGALL.   ZOGALL.    DontUnderstand
 *   everything blip      ALLLEAD.  ALLLEAD.  ALLLEAD.   DontUnderstand
 *   zog take             Take what? ZOGGET.  Take what? Take what?
 *   zog SLAP blip        ZOGHITB.  ZOGHITB.  ZOGHITB.   ZOGHITB.
 *
 * The substring reach is what `unslap` and `exception` show: from 3.90 an
 * object named `exception` cannot be examined at all, its line becoming
 * `x bution` (the same transcripts).  `zog SLAP blip` is the case cell:
 * Replace's binary compare never sees a capital, so the line is already
 * lower-cased by here.
 *
 * Every cell above is Scarier's answer too, bar the `zog take` column:
 * where no rewrite makes the line a task, the Runner still answers "Take
 * what?" and Scarier says "I don't understand."  That is a different
 * rule -- the library verb is matched anywhere in the line -- and it is
 * an open lead in WINE-TRANSCRIPTS-TODO, not a fault of this table.
 *
 * The 3.80-only take->get is the one that matters: it runs before ANY
 * task matching, so a 3.80 task whose command slots say only `take X` /
 * `pick up X` never sees a typed `take X` -- the library take handles it.
 * Measured live on great.taf (run380 under Wine, greatx1 probe,
 * 2026-09-04): task 2 is "take wallet"/"pick up wallet" (score 5, no
 * pickup), and `pick up wallet` ran it ("You have your wallet", +5) while
 * `take wallet` fell to the library ("You pick up the wallet.", score
 * unchanged, wallet then carried).  Every other measured pre-4.0 pair
 * fits: the great t-shirt/stun-gun/picasso tasks (take-only slots) go to
 * the library, the dictionary (`get dictionary` slot), crime's `get cash`
 * and tra's `get *...*` wildcards run as tasks.
 */
static void
pf_filter_builtins (const scr_char *string, scr_int version,
                    scr_bool take_to_get, std::string &buffer,
                    scr_bool &modified, const scr_char *&current)
{
  struct pf_builtin_rewrite_t
  {
    const scr_char *original;
    const scr_char *replacement;
    scr_int min_version, max_version;
    scr_bool substring;
  };
  static const pf_builtin_rewrite_t BUILTIN[] = {
    /* change(), whole words. */
    {"everything", "all", TAF_VERSION_370, TAF_VERSION_380, FALSE},
    {"slap", "hit", TAF_VERSION_370, TAF_VERSION_380, FALSE},
    {"take", "get", TAF_VERSION_380, TAF_VERSION_380, FALSE},
    {"except", "but", TAF_VERSION_380, TAF_VERSION_380, FALSE},
    /*
     * Replace(), substring, in the Runners' own order.  Deliberate
     * deviation: Scarier makes 3.9's four whole-word rewrites, so a name
     * like `exception` or `unslap` survives the line.
     */
    {"everything", "all", TAF_VERSION_390, TAF_VERSION_390, FALSE},
    {"slap", "hit", TAF_VERSION_390, TAF_VERSION_390, FALSE},
    {"except", "but", TAF_VERSION_390, TAF_VERSION_390, FALSE},
    {"apart from", "but", TAF_VERSION_390, TAF_VERSION_390, FALSE},
    {" everything ", " all ", TAF_VERSION_400, TAF_VERSION_400, TRUE},
    {" slap ", " hit ", TAF_VERSION_400, TAF_VERSION_400, TRUE},
    {" but ", " except ", TAF_VERSION_400, TAF_VERSION_400, TRUE},
    {" apart from ", " except ", TAF_VERSION_400, TAF_VERSION_400, TRUE},
  };

  for (const pf_builtin_rewrite_t &entry : BUILTIN)
    {
      if (version < entry.min_version || version > entry.max_version)
        continue;
      if (strcmp (entry.original, "take") == 0
          && (!take_to_get || pf_has_take_off (current)))
        {
          if (take_to_get)
            SCR_DEVIATION ("take_off_kept", nullptr);
          continue;
        }
      if (entry.substring)
        pf_rewrite_substring (string, buffer, modified, current,
                              entry.original, entry.replacement,
                              strlen (entry.replacement), "builtin");
      else
        pf_rewrite_whole_words (string, buffer, modified, current,
                                entry.original, entry.replacement,
                                strlen (entry.replacement), "builtin");
    }
}

/*
 * pf_filter_input()
 *
 * Applies synonym changes to a player input string, and returns the resulting
 * string to the caller, or NULL if no synonym changes were needed.  The
 * return string is malloc'ed, so the caller needs to remember to free it.
 * TAKE_TO_GET FALSE leaves out the 3.80-only take->get rewrite, and
 * RUNNER_SYNONYMS FALSE applies the game's synonyms the way Scarier did before
 * the Runner port (pf_apply_synonyms_by_word()), for the caller that offers
 * the tasks the other spellings of a line (run_element_command()).
 * BUILTINS FALSE leaves out the Runner's own everything/slap/take/except
 * rewrites altogether, for the same caller.
 */
scr_char *
pf_filter_input (const scr_char *string, scr_prop_setref_t bundle,
                 scr_bool take_to_get, scr_bool runner_synonyms,
                 scr_bool builtins)
{
  scr_int version;
  std::string buffer;
  scr_bool modified;
  const scr_char *current;
  assert (string && bundle);

  if (pf.trace)
    scr_trace ("Printfilter: input \"%s\"\n", string);

  version = prop_get_taf_version (bundle);

  pf_filter_synonyms (string, bundle, version, runner_synonyms,
                      buffer, modified, current);
  if (builtins)
    pf_filter_builtins (string, version, take_to_get,
                        buffer, modified, current);

  /* Return the final string, or NULL if no replacements. */
  return modified ? scr_strdup (buffer) : nullptr;
}

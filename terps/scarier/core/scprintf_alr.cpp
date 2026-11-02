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
 * %variable% interpolation and the ALR (Automatic Language Replacement)
 * walk, as the Runners do them: pf_interpolate_vars(), pf_replace_alrs().
 */

#include "scprintf_internal.h"

/*
 * pf_interpolate_vars()
 *
 * Replace %...% elements in a string by their variable values.  If any
 * variables were interpolated, returns an allocated string with replacements
 * done, otherwise returns NULL.
 *
 * If a %...% element exists that is not a variable, then it's left in as is.
 * Similarly, an unmatched (single) % in a string is also left as is.  There
 * appears to be no facility in the file format for escaping literal '%'
 * characters, and since some games have strings with this character in them,
 * this is probably all that can be done.
 */
scr_char *
pf_interpolate_vars (const scr_char *string, scr_var_setref_t vars)
{
  std::string buffer;
  std::vector<scr_char> name;
  const scr_char *marker, *cursor;
  scr_bool is_interpolated = FALSE;

  /* Run through the string looking for variables. */
  marker = string;
  for (cursor = strchr (marker, PERCENT);
       cursor; cursor = strchr (marker, PERCENT))
    {
      scr_int type;
      scr_vartype_t vt_rvalue;
      scr_char close;

      /*
       * Append up to the percent character to the buffer (amortized O(1)).
       * If not yet done, allocate a name buffer guaranteed long enough.
       */
      buffer.append (marker, cursor - marker);
      if (name.empty ())
        name.resize (strlen (string) + 1);

      /*
       * Get the variable name, and from that, the value.  If we encounter a
       * mismatched '%' or unknown variable, skip it.
       */
      if (sscanf (cursor, "%%%[^%]%c", name.data (), &close) != 2
          || close != PERCENT
          || var_is_user_ordered (vars, name.data ())
          || var_is_unknown_reference (vars, name.data ())
          || !var_get (vars, name.data (), &type, &vt_rvalue))
        {
          buffer.append (cursor, 1);
          marker = cursor + 1;
          continue;
        }

      /* Get variable value and append to the string. */
      switch (type)
        {
        case VAR_INTEGER:
          {
            scr_char value[32];

            snprintf (value, sizeof(value), "%ld", vt_rvalue.integer);
            buffer.append (value);
            break;
          }

        case VAR_STRING:
          buffer.append (vt_rvalue.string);
          break;

        default:
          scr_fatal ("pf_interpolate_vars: invalid variable type, %ld\n", type);
        }

      /* Advance over the %...% variable name, and note success. */
      marker = cursor + strlen (name.data ()) + 2;
      is_interpolated = TRUE;
    }

  /*
   * Append the remainder of the string (all of it if there was no '%'), and
   * if we interpolated into it, return it.  If we didn't interpolate
   * successfully (the input contained a rogue '%' character), throw out the
   * buffer as it will be the same as our input, and return NULL.
   */
  buffer.append (marker);
  if (var_interpolate_user_ordered (vars, buffer))
    is_interpolated = TRUE;

  if (is_interpolated)
    return scr_strdup (buffer);
  return nullptr;
}


/*
 * pf_replace_alr()
 *
 * Helper for pf_replace_alrs().  Replace one ALR found in the string with
 * 'replacement' -- the ALR's own replacement text for 3.9, the recursively
 * filtered form of it for 4.0.  If any replacement was made, the rebuilt
 * string is handed back in 'out' and TRUE returned; otherwise 'out' is
 * untouched and FALSE returned.
 */
scr_bool
pf_replace_alr (const scr_char *string, std::string &out,
                const pf_str_pair_t &entry, const scr_char *replacement)
{
  const scr_char *marker, *cursor;
  scr_bool replaced;

  /* Ignore pathological empty originals. */
  if (entry.original[0] == NUL)
    return FALSE;

  /*
   * Where the replacement contains the original (a self-containing ALR), note
   * each offset the original sits at inside it, so that a match which is
   * already part of an earlier expansion can be recognised below.
   */
  std::vector<size_t> self_offsets;
  const size_t replacement_length = strlen (replacement);
  for (cursor = strstr (replacement, entry.original);
       cursor; cursor = strstr (cursor + 1, entry.original))
    self_offsets.push_back (cursor - replacement);

  /* Run through the marker string looking for things to replace. */
  std::string result;
  replaced = FALSE;
  marker = string;
  for (cursor = strstr (marker, entry.original);
       cursor; cursor = strstr (marker, entry.original))
    {
      scr_bool expanded;

      /*
       * Deliberate deviation from run400: a version 4.0 turn walks its text
       * once per completing task and again at the flush (pf_refilter()), so
       * the Runner re-expands a self-containing ALR on every walk -- humbug's
       * "Okay.  Okay.  I put the sweet on the plinth.", sophie's exits list
       * with eight "north (to the farmhouse)", private_eye's doubled "I've
       * seen him in hurt from time to time.".  That is a display accident, so
       * Scarier leaves alone a match that already sits inside a complete
       * copy of its replacement: each walk still runs, but a self-containing
       * ALR expands a given piece of text only once.
       */
      expanded = FALSE;
      for (size_t index_ = 0; index_ < self_offsets.size (); index_++)
        {
          const size_t offset = self_offsets[index_];

          if ((size_t) (cursor - string) >= offset
              && strncmp (cursor - offset, replacement,
                          replacement_length) == 0)
            {
              expanded = TRUE;
              break;
            }
        }
      if (expanded)
        {
          result.append (marker, cursor + entry.original_length - marker);
          marker = cursor + entry.original_length;
          continue;
        }

      /* Append the text up to the match, then the replacement. */
      result.append (marker, cursor - marker);
      result.append (replacement);

      /* Advance over the original. */
      marker = cursor + entry.original_length;
      replaced = TRUE;
    }

  /*
   * If a replacement was made, append any trailing text and hand the rebuilt
   * string back through 'out'.  If nothing matched, leave 'out' untouched.
   */
  if (replaced)
    {
      result.append (marker);
      out = std::move (result);
    }
  return replaced;
}


/*
 * pf_alr_candidates()
 *
 * Collect, in ascending cache order, every ALR slot whose original occurs in
 * 'text'.  Only those can fire, so the replacement loop below walks this list
 * instead of probing all of the game's ALRs with strstr().
 *
 * This is a pure filter: the answer is exact (an original either occurs or it
 * does not), so the ALRs that fire, and the order they fire in, are unchanged.
 * It matters because ALR tables get big -- cursed.taf carries 9724 of them, and
 * the old all-slots sweep spent 17GB of strstr() on one walkthrough replay.
 */
static void
pf_alr_candidates (const scr_char *text, std::vector<scr_int> &candidates)
{
  const unsigned char *const filter = pf.alr.filter.data ();
  size_t length, offset;

  candidates.clear ();

  /* Originals too short to key on: probe them directly. */
  for (size_t index_ = 0; index_ < pf.alr.unkeyed.size (); index_++)
    {
      const pf_str_pair_t &entry = pf.alr.cache[pf.alr.unkeyed[index_]];

      if (strstr (text, entry.original))
        candidates.push_back (pf.alr.unkeyed[index_]);
    }

  /* One pass over the text, looking each position's prefix up in the index. */
  length = strlen (text);
  if (pf.alr.index.empty ())
    length = 0;
  for (offset = 0; offset + PF_ALR_KEY_LENGTH <= length; offset++)
    {
      pf_alr_index_t::const_iterator bucket;
      const unsigned char *const bytes = (const unsigned char *) text + offset;
      const scr_uint key = (scr_uint) bytes[0] | ((scr_uint) bytes[1] << 8)
                           | ((scr_uint) bytes[2] << 16)
                           | ((scr_uint) bytes[3] << 24);
      const size_t slot = pf_alr_filter_slot (key);

      if (!(filter[slot >> 3] & (1u << (slot & 7))))
        continue;
      bucket = pf.alr.index.find (key);
      if (bucket == pf.alr.index.end ())
        continue;

      for (size_t index_ = 0; index_ < bucket->second.size (); index_++)
        {
          const pf_str_pair_t &entry = pf.alr.cache[bucket->second[index_]];

          if (entry.original_length <= length - offset
              && memcmp (text + offset, entry.original,
                         entry.original_length) == 0)
            candidates.push_back (bucket->second[index_]);
        }
    }

  /* The loop below needs cache order, and one entry per slot. */
  std::sort (candidates.begin (), candidates.end ());
  candidates.erase (std::unique (candidates.begin (), candidates.end ()),
                    candidates.end ());
}


/*
 * pf_alr_rewrites_span()
 *
 * TRUE if some ALR original, matched at a position before 'start' in 'text',
 * runs over the whole of text[start, end) -- the original may carry on past
 * the end of 'text', provided the part inside it agrees.  The library asks
 * this of its bare third-person messages ("Irvine open ") to see whether the
 * game's author has already rewritten the verb with an ALR written against
 * the Runner's own un-conjugated output ("Irvine open" -> "Irvine uncloses");
 * see lib_conjugate_third_person().
 */
scr_bool
pf_alr_rewrites_span (scr_prop_setref_t bundle, const scr_char *text,
                      size_t start, size_t end)
{
  scr_vartype_t vt_key;
  scr_int alr_count;
  size_t length;

  vt_key.string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);
  if (alr_count == 0)
    return FALSE;
  if (!pf.alr.built || (scr_int) pf.alr.cache.size () != alr_count)
    pf_alr_cache_build (bundle, alr_count);

  length = strlen (text);
  for (size_t index_ = 0; index_ < pf.alr.cache.size (); index_++)
    {
      const pf_str_pair_t &entry = pf.alr.cache[index_];
      size_t position;

      if (entry.original_length == 0)
        continue;
      for (position = 0; position < start && position < length; position++)
        {
          size_t overlap;

          if (position + entry.original_length < end)
            continue;
          overlap = length - position;
          if (overlap > entry.original_length)
            overlap = entry.original_length;
          if (memcmp (text + position, entry.original, overlap) == 0)
            return TRUE;
        }
    }
  return FALSE;
}


/*
 * pf_alr_mentions()
 *
 * TRUE if any ALR Original contains text verbatim (case-sensitive, the way
 * ALRs match).  A game whose ALRs quote a Runner-shaped phrase -- "assassin
 * manages " in Les Feux de l'enfer -- was written against that exact shape,
 * so callers use this to keep the Runner's text there rather than a
 * deliberately cleaned-up one; see battle_npc_name().
 */
scr_bool
pf_alr_mentions (scr_prop_setref_t bundle, const scr_char *text)
{
  scr_vartype_t vt_key;
  scr_int alr_count;

  if (!text || text[0] == NUL)
    return FALSE;
  vt_key.string = "ALRs";
  alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);
  if (alr_count == 0)
    return FALSE;
  if (!pf.alr.built || (scr_int) pf.alr.cache.size () != alr_count)
    pf_alr_cache_build (bundle, alr_count);

  for (size_t index_ = 0; index_ < pf.alr.cache.size (); index_++)
    {
      const pf_str_pair_t &entry = pf.alr.cache[index_];

      if (entry.original_length > 0 && strstr (entry.original, text))
        return TRUE;
    }
  return FALSE;
}


/*
 * A pair of ALRs that rewrite each other (A -> B and B -> A) recurses for
 * ever, and the Runner really would overflow its stack on one.  No game in
 * the corpus has such a pair, so these caps are guards, not a model.  The
 * walk is deterministic, so re-entering an ALR already on the recursion path
 * would repeat that path for ever: that ALR's replacement is spliced in
 * unfiltered instead (the path is pf.alr.on_path, one mark per cache slot).
 * The depth and size caps bound what is left -- long acyclic chains, and
 * replacements that multiply on each level (A -> "B B", B -> "C C", ...),
 * which would otherwise double the text per level.
 */
enum { PF_ALR_DEPTH_LIMIT = 32 };
static const size_t PF_ALR_SIZE_LIMIT = 65536;


/*
 * pf_alr_walk()
 * pf_replace_alrs()
 *
 * Replace any ALRs found in the string with their equivalents.  If any ALRs
 * were replaced, pf_replace_alrs() returns an allocated string with the
 * replacements done, otherwise it returns NULL.
 *
 * This is run400's Proc_21_20_44C7DC read straight (the run390 twin is the
 * loop at loc_45BD43, the tail of its output filter Proc_2_28_45CBD0):
 *
 *    Function ALRs(text, noalr)
 *      text = substitute_percent_tags(text)         ' 47A3DC
 *      If dont_convert_ALRs Then Return text
 *      If noalr <> 1 Then
 *        For i = 0 To ALRCount - 1                  ' length-descending order
 *          If InStr(1, text, ALR(i).Original, 0) > 0 Then
 *            If text = ALR(i).Replacement Then Return text          ' 44C75E
 *            expansion = ALRs(ALR(i).Replacement, 0)                ' 44C76F
 *            text = Replace(text, ALR(i).Original, expansion, 1, -1, 0)
 *          End If
 *        Next
 *      End If
 *      Return text
 *
 * So a walk is ONE pass down the length-sorted list, and the depth comes from
 * the recursion on the REPLACEMENT, not from repeating the pass.  The two
 * differ wherever a replacement combines with the text around it to spell
 * another ALR's original: the recursion cannot see that, a repeated pass
 * would.  Qui a tue Dana (4.00) is the measurement -- its 602 ALRs carry
 * both [You move] -> [Vous vous deplacez] and, right after it, the author's
 * attempted fix-up [Vous vous deplacez in.] -> [Vous entrez.] -- and run400
 * answers `in` with "Vous vous deplacez in."
 * (runner_transcripts/qui_a_tue_dana.txt:210), so the fix-up never fires.
 * It cannot: it is 22 characters against 8, so the sort has already walked
 * past it when [You move] writes its original into the line, and the walk
 * never comes back.
 *
 * The equality test at 44C75E is the recursion's only brake, and it is a
 * whole-text one: filtering [Okay.  I put ] for the self-containing ALR
 * [I put ] -> [Okay.  I put ] finds the original, sees that the text IS the
 * replacement, and returns it untouched -- which is what holds a
 * self-containing ALR to exactly one expansion per walk.  Note that it exits
 * the whole function, so the remaining ALRs are skipped too.
 *
 * 3.9 has neither the recursion nor the test; version 3.9 games therefore
 * take the same single pass with the replacement spliced in verbatim.
 */
void
pf_alr_walk (const scr_char *string, std::string &out, scr_var_setref_t vars,
             scr_bool recursive, scr_int depth)
{
  std::vector<scr_int> candidates;
  size_t position;
  scr_int next;
  scr_int iteration;

  out.assign (string);
  if (depth > PF_ALR_DEPTH_LIMIT || out.size () > PF_ALR_SIZE_LIMIT)
    return;
  if (pf.alr.on_path.size () != pf.alr.cache.size ())
    pf.alr.on_path.assign (pf.alr.cache.size (), FALSE);

  /*
   * 47A3DC, the first thing ALRs() does with whatever text it was handed:
   * substitute the %...% tags.  At depth 0 the caller has already done this
   * and it is a no-op; what it is here for is the recursion, where it is the
   * step that lets an ALR replacement name a variable and have the ALRs
   * keyed on that variable's value still fire.  Cursed (4.00) is built on
   * it: [You move north.] -> [You %s north.<br>] with %s a state variable,
   * and then [%s-fox] -> [trot] and so on for each state, so the printed
   * verb follows what the player has been turned into.
   */
  for (iteration = 0; iteration < ITERATION_LIMIT; iteration++)
    {
      scr_char *interpolated;

      interpolated = pf_interpolate_vars (out.c_str (), vars);
      if (!interpolated)
        break;
      out = interpolated;
      scr_free (interpolated);
    }

  /*
   * Walk only the ALRs whose original is actually in the text, in the
   * length-sorted order baked into the cache at build time.  'next' is the
   * lowest slot still to consider, so that a re-scan after a replacement
   * resumes where the sweep had got to rather than starting over -- the For
   * loop above never goes back.
   */
  pf_alr_candidates (out.c_str (), candidates);
  next = 0;
  position = 0;

  while (position < candidates.size ())
    {
      std::string expansion, rebuilt;
      scr_int index_;

      index_ = candidates[position];
      if (index_ < next)
        {
          position++;
          continue;
        }

      const pf_str_pair_t &entry = pf.alr.cache[index_];

      if (recursive)
        {
          /* 44C75E: the text IS this ALR's replacement, so stop dead. */
          if (out.compare (entry.replacement) == 0)
            return;

          /* 44C76F: the replacement is itself filtered before it is spliced. */
          if (pf.alr.on_path[index_])
            expansion.assign (entry.replacement);
          else
            {
              pf.alr.on_path[index_] = TRUE;
              pf_alr_walk (entry.replacement, expansion, vars, TRUE, depth + 1);
              pf.alr.on_path[index_] = FALSE;
            }
        }
      else
        expansion.assign (entry.replacement);

      if (pf_replace_alr (out.c_str (), rebuilt, entry, expansion.c_str ()))
        {
          out = std::move (rebuilt);
          if (out.size () > PF_ALR_SIZE_LIMIT)
            return;

          /*
           * The text changed under us, so which originals are present may have
           * changed too: re-scan, and resume at the slot after this one.
           */
          next = index_ + 1;
          pf_alr_candidates (out.c_str (), candidates);
          position = std::lower_bound (candidates.begin (), candidates.end (),
                                       next) - candidates.begin ();
        }
      else
        position++;
    }
}


scr_char *
pf_replace_alrs (const scr_char *string, scr_var_setref_t vars,
                 scr_prop_setref_t bundle, scr_int alr_count,
                 scr_bool recursive)
{
  std::string current;

  /*
   * Resolve the immutable ALR table from the property tree once per game (see
   * scr_pf_state_t); after that the hot loop reads it directly, with no
   * prop_get() lookups at all.  The count is fixed for a game, so a lazy build
   * on first use is safe.
   */
  if (!pf.alr.built || (scr_int) pf.alr.cache.size () != alr_count)
    pf_alr_cache_build (bundle, alr_count);

  /*
   * The Runner's paragraphs are strings that pspace() has left ending in
   * spaces, and its ALR pass sees them that way; ours end at a bare newline.
   * So an original ending in a space, which the Runner's generators write from
   * exactly such text, would never match the last sentence of a paragraph
   * here.  The Reluctant Vampire's `open freezer` ALR "You open the freezer.
   * Some jam and a bottle are inside the freezer. " -> "... Lurking inside are
   * some jam and a bottle." fires in run400
   * (runner_probes/reluctantvampire.run400.txt line 744).  Where a game has
   * such an original, give each line end the Runner's two spaces for the walk,
   * behind a marker, and take back whatever the walk left of them afterwards.
   */
  if (pf.alr.trailing_space)
    {
      static const scr_char marker = '\x01';
      std::string padded, walked;
      const scr_char *cursor;

      for (cursor = string;; cursor++)
        {
          if ((*cursor == '\n' || *cursor == NUL)
              && !padded.empty ()
              && padded.back () != '\n' && padded.back () != ' ')
            {
              padded.append ("  ");
              padded.push_back (marker);
            }
          if (*cursor == NUL)
            break;
          padded.push_back (*cursor);
        }

      pf_alr_walk (padded.c_str (), walked, vars, recursive, 0);

      for (size_t index_ = 0; index_ < walked.size (); index_++)
        {
          if (walked[index_] == marker)
            {
              while (!current.empty () && current.back () == ' ')
                current.pop_back ();
            }
          else
            current.push_back (walked[index_]);
        }
    }
  else
    pf_alr_walk (string, current, vars, recursive, 0);

  /* Return the rebuilt string if any replacement was made, else NULL. */
  return current.compare (string) == 0 ? nullptr : scr_strdup (current);
}

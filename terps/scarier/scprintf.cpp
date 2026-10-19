/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Module notes:
 *
 * o Is the whole interpolation and ALR passes thing right?  There's no
 *   documentation on it, and it's not intuitively implemented in Adrift.
 *
 * o Is dissecting HTML tags the right thing to do?
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "scarier.h"
#include "scprotos.h"


/*
 * pf_strdup()
 *
 * Copy a std::string into a freshly scr_malloc'ed C string, so callers that
 * expect to scr_free() the result keep working unchanged.  Used as the boundary
 * between the std::string accumulators below and the engine's char* contract.
 */
static scr_char *
pf_strdup (const std::string &string)
{
  scr_char *buffer = (scr_char *) scr_malloc (string.size () + 1);
  memcpy (buffer, string.c_str (), string.size () + 1);
  return buffer;
}


/* Assorted definitions and constants. */
static const scr_uint PRINTFILTER_MAGIC = 0xb4736417;
enum
{ ITERATION_LIMIT = 32
};
static const scr_char NUL = '\0';
static const scr_char LESSTHAN = '<';
static const scr_char GREATERTHAN = '>';
static const scr_char PERCENT = '%';
static const scr_char *const ENTITY_LESSTHAN = "&lt;",
                     *const ENTITY_GREATERTHAN = "&gt;",
                     *const ENTITY_PERCENT = "+percent+";
enum
{ ENTITY_LENGTH = 4,
  PERCENT_LENGTH = 9
};
static const scr_char *const ESCAPES = "<>%&+";
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";

/* Trace flag, set before running. */
static scr_bool pf_trace = FALSE;


/*
 * Table tying HTML-like tag strings to enumerated tag types.  Since it's
 * scanned sequentially by strncmp(), it's ordered so that longer strings
 * come before shorter ones.  The <br> tag is missing because this is
 * handled separately, as a simple put of '\n'.
 */
typedef struct
{
  const scr_char *const name;
  const scr_int length;
  const scr_int tag;
} scr_html_tags_t;

static const scr_html_tags_t HTML_TAGS_TABLE[] = {
  {"bgcolour", 8, SCR_TAG_BGCOLOUR}, {"bgcolor", 7, SCR_TAG_BGCOLOUR},
  {"waitkey", 7, SCR_TAG_WAITKEY},
  {"center", 6, SCR_TAG_CENTER}, {"/center", 7, SCR_TAG_ENDCENTER},
  {"centre", 6, SCR_TAG_CENTER}, {"/centre", 7, SCR_TAG_ENDCENTER},
  {"right", 5, SCR_TAG_RIGHT}, {"/right", 6, SCR_TAG_ENDRIGHT},
  {"font", 4, SCR_TAG_FONT}, {"/font", 5, SCR_TAG_ENDFONT},
  {"wait", 4, SCR_TAG_WAIT}, {"cls", 3, SCR_TAG_CLS},
  {"i", 1, SCR_TAG_ITALICS}, {"/i", 2, SCR_TAG_ENDITALICS},
  {"b", 1, SCR_TAG_BOLD}, {"/b", 2, SCR_TAG_ENDBOLD},
  {"u", 1, SCR_TAG_UNDERLINE}, {"/u", 2, SCR_TAG_ENDUNDERLINE},
  {"c", 1, SCR_TAG_COLOUR}, {"/c", 2, SCR_TAG_ENDCOLOUR},
  {NULL, 0, SCR_TAG_UNKNOWN}
};

/*
 * Printfilter structure definition.  It defines a buffer for output,
 * associated size and length, a note of any conversion to apply to the next
 * buffered character, and a flag to let the filter ignore incoming text.
 */
typedef struct scr_filter_s
{
  scr_uint magic;
  std::string buffer;
  scr_bool new_sentence;
  scr_bool is_muted;
  scr_bool needs_filtering;
  /* Buffer length just after pf_buffer_paragraph_line() supplied a trailing
     newline of its own; -1 if the last one did not, or if anything has been
     buffered since.  See pf_undo_auto_break(). */
  scr_int auto_break_at;
  /* Buffer length just after a newline that a Runner really stores in its
     output string (4.0's "Time passes..." & vbCrLf), so that a join keeps it
     instead of popping it, and so that pf_buffer_paragraph() does not collapse
     a leading break against it; -1 otherwise.  See pf_buffer_hard_break(). */
  scr_int hard_break_at;
  /* Length of a prefix of the buffer that the paragraph-spacing helpers are
     to treat as if it were not there.  See pf_hide_prefix(). */
  size_t hidden;
  /* Buffer length just after pf_buffer_reference() buffered one of the 4.0
     Runner's bracketed reference lines; -1 otherwise.  While the line is
     still the last thing buffered, pf_buffer_paragraph() leaves a leading
     break on the next text alone. */
  scr_int reference_at;
  /* TRUE while the next text buffered is to be joined onto the current
     end of the buffer with the Runner's two-space separator, taking back an
     auto-break of ours first.  See pf_buffer_join_pending(). */
  scr_bool join_pending;
} scr_filter_t;


/*
 * Original/replacement string-pair table caches.
 *
 * Two immutable game tables drive the filters below as (original -> replacement)
 * string pairs: the ALRs (Automatic Language Replacements) applied to output by
 * pf_replace_alrs(), and the Synonyms applied to input by pf_filter_input().
 * Both used to re-resolve every entry from the string-keyed property tree
 * ("<table>"/index/"Original"|"Replacement") on every call -- an O(count)
 * prop_get() storm per call, dominated by the prop_find_child() strcmp scan
 * each lookup drives.  For the ALRs, which run on every output flush and every
 * filter pass, profiling text-heavy ADRIFT 4 games (humbug) put ~30% of engine
 * time there.
 *
 * Each table is resolved once into a flat array the consumer iterates directly,
 * with no prop_get() calls left in the hot loop.  The two tables differ enough
 * to keep separate builders -- ALRs are taken in a length-sorted order held in
 * "ALRs2"/"ALRIndex", synonyms in raw property order -- but they share the
 * entry shape, the storage (pointers straight into the property store, which
 * owns the strings for the game's lifetime; no copies), and the lifetime.
 *
 * Lifetime: both are reset by pf_cache_reset() from pf_create(), which
 * run_create() calls exactly once per game, so the cached pointers can never
 * outlive the bundle they point into.
 */
typedef struct
{
  const scr_char *original;
  size_t original_length;
  const scr_char *replacement;
  size_t replacement_length;
} pf_str_pair_t;

static std::vector<pf_str_pair_t> pf_alr_cache;
static scr_bool pf_alr_cache_built = FALSE;

/*
 * Multi-pattern prefilter for the ALR loop (see pf_alr_candidates below).
 * pf_alr_index maps the four-byte prefix of an ALR original to every cache slot
 * that starts with it; originals shorter than four bytes are too weak a key for
 * that, and live in pf_alr_short where they are probed with strstr() as before.
 * Empty originals are in neither -- pf_replace_alr() refuses them anyway.
 *
 * Both are rebuilt by pf_alr_cache_build() and cleared by pf_cache_reset(), so
 * they share the cache's per-game lifetime and its pointers into the property
 * store.
 */
static std::unordered_map<scr_uint, std::vector<scr_int> > pf_alr_index;
static std::vector<scr_int> pf_alr_short;
static std::vector<pf_str_pair_t> pf_synonym_cache;
static scr_bool pf_synonym_cache_built = FALSE;

static void
pf_cache_reset (void)
{
  pf_alr_cache.clear ();
  pf_alr_index.clear ();
  pf_alr_short.clear ();
  pf_alr_cache_built = FALSE;
  pf_synonym_cache.clear ();
  pf_synonym_cache_built = FALSE;
}

/* Populate 'entry' from the property tree, given the resolved original and
   replacement strings; shared by both builders below. */
static void
pf_str_pair_set (pf_str_pair_t &entry,
                 const scr_char *original, const scr_char *replacement)
{
  entry.original = original;
  entry.original_length = strlen (original);
  entry.replacement = replacement;
  entry.replacement_length = strlen (replacement);
}

/*
 * pf_alr_key()
 *
 * Pack the first PF_ALR_KEY_LENGTH bytes at 'text' into the key used by
 * pf_alr_index.  The caller guarantees that many bytes are readable.
 */
enum
{ PF_ALR_KEY_LENGTH = 4
};

static scr_uint
pf_alr_key (const scr_char *text)
{
  const unsigned char *bytes = (const unsigned char *) text;

  return (scr_uint) bytes[0] | ((scr_uint) bytes[1] << 8)
         | ((scr_uint) bytes[2] << 16) | ((scr_uint) bytes[3] << 24);
}


static void
pf_alr_cache_build (scr_prop_setref_t bundle, scr_int alr_count)
{
  scr_int index_;

  pf_alr_cache.clear ();
  pf_alr_cache.reserve (alr_count);

  for (index_ = 0; index_ < alr_count; index_++)
    {
      scr_vartype_t vt_key[3];
      pf_str_pair_t entry;
      const scr_char *original, *replacement;
      scr_int alr;

      /* The ALRs are applied in a length-sorted order held in ALRs2; map this
       * slot to the underlying ALR the way pf_replace_alrs() always has. */
      vt_key[0].string = "ALRs2";
      vt_key[1].integer = index_;
      vt_key[2].string = "ALRIndex";
      alr = prop_get_integer (bundle, "I<-sis", vt_key);

      vt_key[0].string = "ALRs";
      vt_key[1].integer = alr;
      vt_key[2].string = "Original";
      original = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = "Replacement";
      replacement = prop_get_string (bundle, "S<-sis", vt_key);

      pf_str_pair_set (entry, original, replacement);
      pf_alr_cache.push_back (entry);
    }

  /* Index the cache for pf_alr_candidates(). */
  pf_alr_index.clear ();
  pf_alr_short.clear ();
  for (index_ = 0; index_ < alr_count; index_++)
    {
      const pf_str_pair_t &entry = pf_alr_cache[index_];

      if (entry.original_length == 0)
        continue;
      if (entry.original_length < PF_ALR_KEY_LENGTH)
        pf_alr_short.push_back (index_);
      else
        pf_alr_index[pf_alr_key (entry.original)].push_back (index_);
    }

  pf_alr_cache_built = TRUE;
}

static void
pf_synonym_cache_build (scr_prop_setref_t bundle, scr_int synonym_count)
{
  scr_int index_;

  pf_synonym_cache.clear ();
  pf_synonym_cache.reserve (synonym_count);

  for (index_ = 0; index_ < synonym_count; index_++)
    {
      scr_vartype_t vt_key[3];
      pf_str_pair_t entry;
      const scr_char *original, *replacement;

      /* Synonyms are taken in raw property order -- no ALRs2-style sort. */
      vt_key[0].string = "Synonyms";
      vt_key[1].integer = index_;
      vt_key[2].string = "Original";
      original = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = "Replacement";
      replacement = prop_get_string (bundle, "S<-sis", vt_key);

      pf_str_pair_set (entry, original, replacement);
      pf_synonym_cache.push_back (entry);
    }

  pf_synonym_cache_built = TRUE;
}


/*
 * pf_is_valid()
 *
 * Return TRUE if pointer is a valid printfilter, FALSE otherwise.
 */
static scr_bool
pf_is_valid (scr_filterref_t filter)
{
  return filter && filter->magic == PRINTFILTER_MAGIC;
}


/*
 * pf_create()
 *
 * Create and return a new printfilter.
 */
scr_filterref_t
pf_create (void)
{
  static scr_bool initialized = FALSE;

  scr_filterref_t filter;

  /* On first call only, verify the string lengths in the table. */
  if (!initialized)
    {
      const scr_html_tags_t *entry;

      /* Compare table lengths with string lengths. */
      for (entry = HTML_TAGS_TABLE; entry->name; entry++)
        {
          if (entry->length != (scr_int) strlen (entry->name))
            {
              scr_fatal ("pf_create:"
                        " table string length is wrong for \"%s\"\n",
                        entry->name);
            }
        }

      initialized = TRUE;
    }

  /* A new filter means a new game (run_create is the sole caller): drop the
   * string-pair caches held for the previous game, whose bundle is going away. */
  pf_cache_reset ();

  /* Create a new printfilter; 'buffer' default-constructs empty. */
  filter = new scr_filter_t ();
  filter->magic = PRINTFILTER_MAGIC;
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
  filter->needs_filtering = FALSE;
  filter->auto_break_at = -1;
  filter->hard_break_at = -1;
  filter->hidden = 0;
  filter->reference_at = -1;
  filter->join_pending = FALSE;

  return filter;
}


/*
 * pf_destroy()
 *
 * Destroy a printfilter and free its allocated memory.
 */
void
pf_destroy (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /* Poison the magic, then delete (frees the std::string buffer too). */
  filter->magic = 0;
  delete filter;
}


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
static scr_char *
pf_interpolate_vars (const scr_char *string, scr_var_setref_t vars)
{
  std::string buffer;
  std::vector<scr_char> name;
  const scr_char *marker, *cursor;
  scr_bool buffer_used, is_interpolated;

  /*
   * Begin with an empty buffer (and "unused" flag, mirroring the original lazy
   * allocation), an unallocated name buffer, and a clear interpolation flag.
   */
  buffer_used = FALSE;
  is_interpolated = FALSE;

  /* Run through the string looking for variables. */
  marker = string;
  for (cursor = strchr (marker, PERCENT);
       cursor; cursor = strchr (marker, PERCENT))
    {
      scr_int type;
      scr_vartype_t vt_rvalue;
      scr_char close;

      /*
       * Append up to the percent character to the buffer (amortized O(1)), and
       * note the buffer as now in use.  If not yet done, allocate a name buffer
       * guaranteed long enough.
       */
      buffer.append (marker, cursor - marker);
      buffer_used = TRUE;
      if (name.empty ())
        name.resize (strlen (string) + 1);

      /*
       * Get the variable name, and from that, the value.  If we encounter a
       * mismatched '%' or unknown variable, skip it.
       */
      if (sscanf (cursor, "%%%[^%]%c", name.data (), &close) != 2
          || close != PERCENT
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
   * If we used the buffer and interpolated into it, append the remainder of
   * the string and return it.  If we didn't interpolate successfully (the
   * input contained a rogue '%' character), throw out the buffer as it will be
   * the same as our input, and return NULL.
   */
  if (buffer_used && is_interpolated)
    {
      buffer.append (marker);
      return pf_strdup (buffer);
    }
  return NULL;
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
static scr_bool
pf_replace_alr (const scr_char *string, std::string &out,
                const pf_str_pair_t &entry, const scr_char *replacement)
{
  const scr_char *marker, *cursor;
  scr_bool replaced;

  /* Ignore pathological empty originals. */
  if (entry.original[0] == NUL)
    return FALSE;

  /* Run through the marker string looking for things to replace. */
  std::string result;
  replaced = FALSE;
  marker = string;
  for (cursor = strstr (marker, entry.original);
       cursor; cursor = strstr (marker, entry.original))
    {
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
  size_t length, offset;

  candidates.clear ();

  /* Originals too short to key on: probe them directly. */
  for (size_t index_ = 0; index_ < pf_alr_short.size (); index_++)
    {
      const pf_str_pair_t &entry = pf_alr_cache[pf_alr_short[index_]];

      if (strstr (text, entry.original))
        candidates.push_back (pf_alr_short[index_]);
    }

  /* One pass over the text, looking each position's prefix up in the index. */
  length = strlen (text);
  for (offset = 0; offset + PF_ALR_KEY_LENGTH <= length; offset++)
    {
      std::unordered_map<scr_uint, std::vector<scr_int> >::const_iterator bucket;

      bucket = pf_alr_index.find (pf_alr_key (text + offset));
      if (bucket == pf_alr_index.end ())
        continue;

      for (size_t index_ = 0; index_ < bucket->second.size (); index_++)
        {
          const pf_str_pair_t &entry = pf_alr_cache[bucket->second[index_]];

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
 * answers `in` with "Vous vous deplacez in." (Adrift_369:210), so the fix-up
 * never fires.  It cannot: it is 22 characters against 8, so the sort has
 * already walked past it when [You move] writes its original into the line,
 * and the walk never comes back.
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

/*
 * A pair of ALRs that rewrite each other (A -> B and B -> A) recurses for
 * ever, and the Runner really would overflow its stack on one.  No game in
 * the corpus has such a pair, so this cap is a guard, not a model.
 */
enum { PF_ALR_DEPTH_LIMIT = 32 };

static void
pf_alr_walk (const scr_char *string, std::string &out, scr_var_setref_t vars,
             scr_bool recursive, scr_int depth)
{
  std::vector<scr_int> candidates;
  size_t position;
  scr_int next;
  scr_int iteration;

  out.assign (string);
  if (depth > PF_ALR_DEPTH_LIMIT)
    return;

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

      const pf_str_pair_t &entry = pf_alr_cache[index_];

      if (recursive)
        {
          /* 44C75E: the text IS this ALR's replacement, so stop dead. */
          if (out.compare (entry.replacement) == 0)
            return;

          /* 44C76F: the replacement is itself filtered before it is spliced. */
          pf_alr_walk (entry.replacement, expansion, vars, TRUE, depth + 1);
        }
      else
        expansion.assign (entry.replacement);

      if (pf_replace_alr (out.c_str (), rebuilt, entry, expansion.c_str ()))
        {
          out = std::move (rebuilt);

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

static scr_char *
pf_replace_alrs (const scr_char *string, scr_var_setref_t vars,
                 scr_prop_setref_t bundle, scr_int alr_count,
                 scr_bool recursive)
{
  std::string current;

  /*
   * Resolve the immutable ALR table from the property tree once per game (see
   * pf_alr_cache above); after that the hot loop reads it directly, with no
   * prop_get() lookups at all.  The count is fixed for a game, so a lazy build
   * on first use is safe.
   */
  if (!pf_alr_cache_built || (scr_int) pf_alr_cache.size () != alr_count)
    pf_alr_cache_build (bundle, alr_count);

  pf_alr_walk (string, current, vars, recursive, 0);

  /* Return the rebuilt string if any replacement was made, else NULL. */
  return current.compare (string) == 0 ? NULL : pf_strdup (current);
}


/*
 * pf_output_text()
 *
 * Edit the tag-stripped text element passed in, substituting &lt; &gt;
 * +percent+ with < > %, then send to the OS-specific output functions.
 */
static void
pf_output_text (const scr_char *string)
{
  scr_int index_;
  std::string buffer;

  /* Optimize away the allocation and copy if possible. */
  if (!(strstr (string, ENTITY_LESSTHAN)
        || strstr (string, ENTITY_GREATERTHAN)
        || strstr (string, ENTITY_PERCENT)))
    {
      if_print_string (string);
      return;
    }

  /*
   * Copy characters from the string into the buffer, replacing any &..;
   * elements by their single-character equivalents.  We also replace any
   * +percent+ elements by percent characters; apparently an undocumented
   * Adrift Runner extension.
   */
  buffer.reserve (strlen (string));
  for (index_ = 0; string[index_] != NUL; index_++)
    {
      if (scr_strncasecmp (string + index_,
                          ENTITY_LESSTHAN, ENTITY_LENGTH) == 0)
        {
          buffer.push_back (LESSTHAN);
          index_ += ENTITY_LENGTH - 1;
        }
      else if (scr_strncasecmp (string + index_,
                               ENTITY_GREATERTHAN, ENTITY_LENGTH) == 0)
        {
          buffer.push_back (GREATERTHAN);
          index_ += ENTITY_LENGTH - 1;
        }
      else if (scr_strncasecmp (string + index_,
                               ENTITY_PERCENT, PERCENT_LENGTH) == 0)
        {
          buffer.push_back (PERCENT);
          index_ += PERCENT_LENGTH - 1;
        }
      else
        buffer.push_back (string[index_]);
    }

  /* Print the rebuilt string. */
  if_print_string (buffer.c_str ());
}


/*
 * pf_output_tag()
 *
 * Output an HTML-like tag element to the OS-specific tag handling function.
 */
static void
pf_output_tag (const scr_char *contents)
{
  const scr_html_tags_t *entry;
  const scr_char *argument;

  /* For a simple <br> tag, just print out a newline. */
  if (scr_compare_word (contents, "br", 2))
    {
      if_print_character ('\n');
      return;
    }

  /*
   * Search for the name in the HTML tags table.  It should be a full match,
   * that is, the character after the matched name must be space or NUL.
   * The <bgcolour="xyz"> tag is the exception; here the terminator is '='.
   * So is <wait>: run400 (Proc at 47A82C) tests Left(LCase(tag), 5) =
   * "<wait" and takes Val() of the rest, so "<wait3>" is a three-second
   * pause exactly like "<wait 3>" (Through time writes the spaceless form
   * throughout).  <waitkey> sits earlier in the table and still wins.
   */
  for (entry = HTML_TAGS_TABLE; entry->name; entry++)
    {
      if (scr_strncasecmp (contents, entry->name, entry->length) == 0)
        {
          scr_char next;

          next = contents[entry->length];
          /*
           * <waitkey> is an exact-string test in run400 (47A779 compares
           * the whole tag with "<waitkey>"); anything else starting "<wait"
           * is a timed <wait> whose Val() of the remainder decides the
           * delay, so "<waitkey 4>" is a zero-second wait and NOT a
           * keypress pause.  Vardock Bates (4.00) writes one after the
           * Jhave wall; run400 does not stop there (transcript 2026-08-29).
           */
          if (entry->tag == SCR_TAG_WAITKEY)
            {
              if (next == NUL)
                break;
              continue;
            }
          if (next == NUL || scr_isspace (next)
              || (entry->tag == SCR_TAG_BGCOLOUR && next == '=')
              || entry->tag == SCR_TAG_WAIT)
            break;
        }
    }

  /* If not matched, output an unknown tag with contents as its argument. */
  if (!entry->name)
    {
      if_print_tag (SCR_TAG_UNKNOWN, contents);
      return;
    }

  /*
   * Find the argument by skipping the tag name and any spaces.  Again, for
   * <bgcolour="xyz">, make a special case, passing the complete contents as
   * argument (to match <font colour=...> for the client.
   */
  argument = contents;
  argument += (entry->tag != SCR_TAG_BGCOLOUR) ? entry->length : 0;
  while (scr_isspace (argument[0]))
    argument++;
  if_print_tag (entry->tag, argument);
}


/*
 * pf_output_untagged()
 *
 * Break apart HTML-like string into normal text elements, and HTML-like
 * tags.
 */
static void
pf_output_untagged (const scr_char *string)
{
  scr_char *untagged, *contents, *cursor;
  const scr_char *marker;

  /*
   * Optimize away the allocation and copy if possible.  We need to check
   * here both for tags and for entities; only if neither occurs is it safe
   * to output the string directly.
   */
  if (!strchr (string, LESSTHAN)
      && !(strstr (string, ENTITY_LESSTHAN)
           || strstr (string, ENTITY_GREATERTHAN)
           || strstr (string, ENTITY_PERCENT)))
    {
      if_print_string (string);
      return;
    }

  /*
   * Create a general temporary string, and alias it to both untagged text
   * and the tag name, for sharing inside the loop.
   */
  std::vector<scr_char> temporary (strlen (string) + 1);
  untagged = contents = temporary.data ();

  /* Run through the string looking for <...> tags. */
  marker = string;
  for (cursor = (scr_char *) strchr (marker, LESSTHAN);
       cursor; cursor = (scr_char *) strchr (marker, LESSTHAN))
    {
      scr_char close;

      /* Handle characters up to the tag start; untagged text. */
      memcpy (untagged, marker, cursor - marker);
      untagged[cursor - marker] = NUL;
      pf_output_text (untagged);

      /* Catch and ignore completely empty tags. */
      if (cursor[1] == GREATERTHAN)
        {
          marker = cursor + 2;
          continue;
        }

      /*
       * Get the text within the tag, reusing the temporary buffer.  If this
       * fails, allow the remainder of the line to be delivered as a tag;
       * unknown, probably.
       */
      if (sscanf (cursor, "<%[^>]%c", contents, &close) != 2
          || close != GREATERTHAN)
        {
          if (sscanf (cursor, "<%[^>]", contents) != 1)
            {
              scr_error ("pf_output_untagged: mismatched '%c'\n", LESSTHAN);
              if_print_character (LESSTHAN);
              marker = cursor + 1;
              continue;
            }
        }

      /* Output tag, and advance marker over the <...> tag. */
      if (!scr_strempty (contents))
        pf_output_tag (contents);
      marker = cursor + strlen (contents) + 1;
      marker += (marker[0] == GREATERTHAN) ? 1 : 0;
    }

  /* Output any remaining string text; the temporary buffer frees itself. */
  pf_output_text (marker);
}


/*
 * pf_filter_internal()
 *
 * Filters an output string, interpolating variables and replacing ALR's.  If
 * any filtering was done, returns an allocated string that the caller needs
 * to free; otherwise, return NULL.
 *
 * Bundle may be NULL, requesting that the function suppress ALR replacements,
 * and do only variables; used for game info strings.
 *
 * How many times the Runner walks the ALR list is a version split, and both
 * halves are measured rather than argued (harness/make_400_alrprobe.py and
 * harness/make_39_alrprobe.py, one task per cell, run under run400.exe and
 * run390.exe in Wine on 2026-08-24):
 *
 *                      ALRs                       run390     run400
 *    "AAA."      AAA -> qAAA                      qAAA.      qqAAA.
 *    "EEE."      EEE -> EEE EEE                   EEE EEE.   EEE EEE EEE EEE.
 *    "RRR."      PPPP -> QQ, RRR -> PPPP          PPPP.      QQ.
 *    "UUU."      WWWWW -> done, VVVV -> WWWWW,
 *                UUU -> VVVV                      VVVV.      done.
 *    "MMM."      MM -> short, MMM -> long         long.      long.
 *    "ZZ ZZ."    ZZ -> z                          z z.       z z.
 *
 * Both versions make ONE pass down the length-sorted list, replacing every
 * occurrence of each original; a chain only runs in the direction of the walk,
 * which is what stops run390's "RRR." at "PPPP." (PPPP was already behind the
 * cursor when RRR produced it).  What 4.0 adds is not a second pass but
 * RECURSION: it filters each replacement before splicing it in, so "UUU."
 * runs all three hops and "RRR." both inside the one pass.  The two routines
 * are read out in full above pf_alr_walk(); run390's is the loop at
 * loc_45BD43, the tail of its output filter Proc_2_28_45CBD0
 * (run390_3.bas:55465), and run400's is Proc_21_20_44C7DC.
 *
 * That distinction is measurable, and measured: a repeated pass would let a
 * replacement combine with the text around it to spell some other ALR's
 * original, and the recursion cannot.  See pf_alr_walk() for Qui a tue Dana's
 * "Vous vous deplacez in.".
 *
 * A self-containing ALR is held to one expansion per walk not by a retirement
 * flag -- SCARE's, which this used to keep -- but by 4.0's whole-text equality
 * test: filtering [Okay.  I put ] for the ALR [I put ] -> [Okay.  I put ]
 * finds the original, sees the text IS the replacement and hands it back
 * untouched.  Every other ALR fires whenever its original is in front of the
 * cursor.  Measured, with [TOKEN] -> [tok] and [zebra] -> [TOKEN] over
 * "TOKEN zebra.":
 *
 *                                                 run390     run400
 *    "TOKEN zebra."                               tok TOKEN. tok tok.
 *
 * SCARE retired every ALR that fired, which gives run390's answer for a
 * version 4.0 game.
 *
 * All of the above is one walk over the whole turn's text at flush.  Version
 * 4.0 gives ONE KIND OF TEXT a second walk of its own: a completing task's
 * CompleteText and AdditionalMessage, filtered again as the task prints them
 * (see pf_buffer_task_paragraph_line() below, and the measurement in
 * harness/make_400_alrsrcprobe.py).  Every cell of the probe above is a task
 * CompleteText, so that second walk is where run400's extra "q" and its third
 * and fourth "EEE" come from -- one recursive walk of "AAA." gives "qAAA.",
 * and walking that again gives "qqAAA.".  That is the humbug (4.00) divergence
 * this all started from -- its "[I put ] -> [Okay.  I put ]" is
 * self-containing, so task 80's CompleteText answers `Put sweet on plinth`
 * with "Okay.  Okay.  I put the sweet on the plinth."
 * (Adrift_30_humbug.txt:841) while the library's own put, in the same
 * transcript, says "Okay.  I put the watch onto the rectangular table." with
 * a single "Okay.".
 *
 * Versions 3.8 and 3.7 cannot reach any of this: neither schema in
 * sctafpar.cpp carries an ALRs section, so those games have no ALRs at all.
 */
static scr_char *
pf_filter_internal (const scr_char *string,
                    scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  scr_int alr_count, iteration;
  scr_bool alr_single_pass, alr_pass_done;
  std::string current;
  scr_bool have_current;
  assert (string && vars);

  if (pf_trace)
    scr_trace ("Printfilter: initial \"%s\"\n", string);

  /* If including ALRs, create a common set of ALR application flags. */
  if (bundle)
    {
      scr_vartype_t vt_key;

      /* Obtain a count of ALRs. */
      vt_key.string = "ALRs";
      alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);

      /*
       * What a walk of the ALR list is -- the measured version split
       * described above.  Both versions get one walk; 4.0's filters each
       * replacement recursively on the way in, 3.9's does not.
       */
      alr_single_pass = prop_get_taf_version (bundle) < TAF_VERSION_400;
    }
  else
    {
      /* Not including ALRs, so set alr count to 0. */
      alr_count = 0;
      alr_single_pass = FALSE;
    }
  alr_pass_done = FALSE;

  /*
   * Loop for a sort-of arbitrary number of passes; probably enough.
   * 'have_current' tracks whether any replacement has produced a 'current'
   * string yet; until then we work on the input 'string'.
   */
  have_current = FALSE;
  for (iteration = 0; iteration < ITERATION_LIMIT; iteration++)
    {
      scr_int inner_iteration;
      scr_bool changed;
      scr_char *intermediate;

      /* Note whether this iteration changes anything, to check for no change. */
      changed = FALSE;

      for (inner_iteration = 0;
           inner_iteration < ITERATION_LIMIT; inner_iteration++)
        {
          /*
           * Interpolate variables.  If any changes were made, adopt the
           * interpolated version as current, freeing the returned char*.
           * Work on the current string, if any, otherwise the input string.
           */
          intermediate = pf_interpolate_vars (have_current ? current.c_str ()
                                                           : string, vars);
          if (intermediate)
            {
              current = intermediate;
              scr_free (intermediate);
              have_current = TRUE;
              changed = TRUE;
              if (pf_trace)
                {
                  scr_trace ("Printfilter: interpolated [%ld,%ld] \"%s\"\n",
                            iteration, inner_iteration, current.c_str ());
                }
            }
          else
            break;
        }

      /* If we have ALRs to process, search out and replace all findable. */
      if (alr_count > 0 && !alr_pass_done)
        {
          /*
           * Both versions walk the list exactly once for the whole filter --
           * not once per iteration of the loop we are in.  What differs is
           * what a walk does with a replacement: 4.0 filters it recursively
           * before splicing it in, 3.9 splices it verbatim.  See
           * pf_alr_walk().
           */
          alr_pass_done = TRUE;

          intermediate
            = pf_replace_alrs (have_current ? current.c_str () : string,
                               vars, bundle, alr_count, !alr_single_pass);
          if (intermediate)
            {
              current = intermediate;
              scr_free (intermediate);
              have_current = TRUE;
              changed = TRUE;
              if (pf_trace)
                {
                  scr_trace ("Printfilter: replaced [%ld,-] \"%s\"\n",
                            iteration, current.c_str ());
                }
            }
        }

      /* If nothing changed this iteration, stop now. */
      if (!changed)
        break;
    }

  /* Return an allocated current, or NULL if nothing changed. */
  return have_current ? pf_strdup (current) : NULL;
}


/*
 * pf_filter()
 *
 * A facet of pf_filter_internal().  Filter an output string, interpolating
 * variables and replacing ALR's.  Returns an allocated string that the caller
 * needs to free.
 */
scr_char *
pf_filter (const scr_char *string,
           scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  scr_char *current;

  /* Filter this string, including ALRs replacements. */
  current = pf_filter_internal (string, vars, bundle);

  /* Our contract is to return an allocated string; copy if required. */
  if (!current)
    {
      current = (decltype(current)) scr_malloc (strlen (string) + 1);
      memcpy (current, string, strlen (string) + 1);
    }

  return current;
}


/*
 * pf_filter_for_info()
 *
 * A facet of pf_filter_internal().  Filters output, interpolating variables
 * only (no ALR replacement), and returns the resulting string to the caller.
 * Used on informational strings such as the game title and author.  Returns
 * an allocated string that the caller needs to free.
 */
scr_char *
pf_filter_for_info (const scr_char *string, scr_var_setref_t vars)
{
  scr_char *current;

  /* Filter this string, excluding ALRs replacements. */
  current = pf_filter_internal (string, vars, NULL);

  /* Our contract is to return an allocated string; copy if required. */
  if (!current)
    {
      current = (decltype(current)) scr_malloc (strlen (string) + 1);
      memcpy (current, string, strlen (string) + 1);
    }

  return current;
}


/*
 * pf_flush()
 *
 * Filter buffered data, interpolating variables and replacing ALR's, and
 * send the resulting string to the output channel.
 */
void
pf_flush (scr_filterref_t filter,
          scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  /* See if there is any buffered data to flush. */
  if (!filter->buffer.empty ())
    {
      /*
       * Filter the buffered string, then print it untagged.  Remember to free
       * the filtered version.  If filtering made no difference, or if the
       * buffer was already filtered by, say, checkpointing, just print the
       * original buffer untagged instead.
       */
      if (filter->needs_filtering)
        {
          scr_char *filtered;

          filtered = pf_filter_internal (filter->buffer.c_str (), vars, bundle);
          if (filtered)
            {
              pf_output_untagged (filtered);
              scr_free (filtered);
            }
          else
            pf_output_untagged (filter->buffer.c_str ());
        }
      else
        pf_output_untagged (filter->buffer.c_str ());

      /* Remove buffered data. */
      filter->buffer.clear ();
      filter->needs_filtering = FALSE;
    }

  /* Reset new sentence and mute flags. */
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
}


/*
 * pf_append_string()
 *
 * Append a string to the filter buffer.
 */
static void
pf_append_string (scr_filterref_t filter, const scr_char *string)
{
  /* std::string handles growth (amortized) and termination for us. */
  filter->buffer.append (string);
}


/*
 * pf_checkpoint()
 *
 * Filter buffered data, interpolating variables and replacing ALR's, and
 * store the result back in the buffer.  This allows a string to be inter-
 * polated in between main flushes; used to update buffered text with variable
 * values before those values are updated by task actions.
 */
void
pf_checkpoint (scr_filterref_t filter,
               scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  /* See if there is any buffered data to filter. */
  if (!filter->buffer.empty ())
    {
      /*
       * Filter the buffered string, and place the filtered result, if any,
       * back into the filter buffer.
       */
      if (filter->needs_filtering)
        {
          scr_char *filtered;

          filtered = pf_filter_internal (filter->buffer.c_str (), vars, bundle);
          if (filtered)
            {
              filter->buffer.assign (filtered);
              scr_free (filtered);
            }
        }

      /* Note the buffer as filtered, to avoid pointless filtering. */
      filter->needs_filtering = FALSE;
    }
}


/*
 * pf_refilter()
 *
 * Filter buffered data the way pf_checkpoint() does, but leave it marked as
 * still needing filtering, so that the flush at the end of the turn filters
 * it once more.
 *
 * This is what a completing version 4.0 task does to the turn's output, and
 * it is why some text comes out with its ALRs applied twice.  Measured under
 * run400.exe in Wine on 2026-08-24 with harness/make_400_alrsrcprobe.py,
 * whose ALRs include the self-containing "ball -> qball" (transcripts
 * Adrift_11/12/13 in ~/adrift-battle/runner/wine/pfx/drive_c/adrift):
 *
 *    look       "LONG ball ..." (room, no task)      -> LONG qball
 *    zulu       task CompleteText                    -> You take the qqball.
 *    victor     CompleteText "CT n=%n% TXT %w%.",
 *               its action then setting n = 9        -> CT n=9 TXT qqball.
 *    uniform    CompleteText "CTU ball.", an action
 *               running zulu, AdditionalMessage
 *               "AMU ball."     -> CTU qqqball.  You take the qqqball.
 *                                  AMU qqball.
 *    tango      CompleteText "CTT ball." and
 *               ShowRoomDesc    -> CTT qqball. / LONG qqball ... qqAAA
 *
 * Every one of those falls out of one rule: at the end of a task that
 * completes, the Runner runs its output filter over the whole of the turn's
 * buffered text, and the turn's own flush runs it once more.  So the "qqq" in
 * the uniform cell is the inner task's pass, the outer task's pass and the
 * flush; "AMU" misses the inner one because it was printed after it; and the
 * room description tango prints is caught by tango's pass just like tango's
 * own CompleteText.
 *
 * The variables go the same way -- the pass interpolates them where it finds
 * them, which is why "CT n=%n%" comes out as the value the task's own action
 * has just set, and why "%w%" (holding "ball") comes out with both walks
 * applied to its value.
 */
void
pf_refilter (scr_filterref_t filter,
             scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  if (!filter->buffer.empty () && filter->needs_filtering)
    {
      const size_t length = filter->buffer.size ();
      scr_char *filtered;

      filtered = pf_filter_internal (filter->buffer.c_str (), vars, bundle);
      if (filtered)
        {
          const scr_bool at_end = filter->auto_break_at >= 0
                                  && (size_t) filter->auto_break_at == length;

          filter->buffer.assign (filtered);
          scr_free (filtered);

          /* Keep the note of our own trailing newline pointing at the end of
             the rewritten text, so pf_undo_auto_break() can still take it
             back. */
          if (at_end && pf_text_ends_with_break (filter->buffer.c_str ()))
            filter->auto_break_at = (scr_int) filter->buffer.size ();
        }
    }
}


/*
 * pf_get_buffer()
 * pf_transfer_buffer()
 *
 * Return the raw, unfiltered, buffered text.  Returns NULL if no buffered
 * data available.  Transferring the buffer transfers ownership of the buffer
 * string to the caller, who is then responsible for freeing it.
 *
 * The second function is an optimization to avoid allocations and copying
 * in client code.
 */
const scr_char *
pf_get_buffer (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /* Return the visible buffer if it holds any text, otherwise NULL.  Text
     before the barrier, if one is set, is invisible here (pf_hide_prefix()). */
  if (filter->buffer.size () > filter->hidden)
    return filter->buffer.c_str () + filter->hidden;
  else
    return NULL;
}

scr_char *
pf_transfer_buffer (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /*
   * If the filter holds text, hand the caller a freshly allocated copy (which
   * they scr_free) and reset the filter to an empty state.
   */
  if (!filter->buffer.empty ())
    {
      scr_char *retval;

      /* Copy out the buffered text for the caller to own. */
      retval = pf_strdup (filter->buffer);

      /* Clear all filter fields down to empty values. */
      filter->buffer.clear ();
      filter->new_sentence = FALSE;
      filter->is_muted = FALSE;
      filter->needs_filtering = FALSE;

      /*
       * Deliberately not resetting auto_break_at: task running transfers the
       * buffer out, runs task actions, then prepends it back, and the note of
       * our own trailing newline has to survive that round trip.  It stays
       * meaningless while the buffer is empty, since it can only match a
       * buffer length equal to the transferred text's own.
       */

      /* Return the allocated buffered text. */
      return retval;
    }
  else
    return NULL;
}


/*
 * pf_empty()
 *
 * Empty any text currently buffered in the filter.
 */
void
pf_empty (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /* Free any allocation, and return the filter to initialization state. */
  filter->buffer.clear ();
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
  filter->needs_filtering = FALSE;
  filter->auto_break_at = -1;
  filter->hard_break_at = -1;
  filter->hidden = 0;
  filter->reference_at = -1;
  filter->join_pending = FALSE;
}


/*
 * pf_buffer_reference()
 *
 * Buffer one of the 4.0 Runner's bracketed reference lines -- "(a trophy)"
 * for a pronoun, "(look)" for "again" -- as its own line, and note where it
 * ends.  The Runner builds each of these as
 * "(" & text & ")" & vbCrLf and appends the turn's response after it
 * unchanged, so a task text that opens with "<br>" still puts a blank line
 * between the reference and itself.  Unmarked, the line's newline would make
 * pf_buffer_paragraph() swallow that leading break as if it were one of
 * SCARIER's own terminators.  Only that one collapse is switched off: the
 * hidden-prefix barrier would also stop a walk announcement joining the
 * paragraph (see pf_buffer_paragraph_join()), which the Runner still does.
 */
void
pf_buffer_reference (scr_filterref_t filter, const scr_char *text)
{
  assert (pf_is_valid (filter));
  assert (text);

  pf_buffer_string (filter, "(");
  pf_buffer_string (filter, text);
  pf_buffer_string (filter, ")\n");
  if (!filter->is_muted)
    filter->reference_at = (scr_int) filter->buffer.size ();
}


/*
 * pf_hide_prefix()
 * pf_reveal_prefix()
 *
 * Hide everything buffered so far from the paragraph-spacing helpers, and put
 * it back.  pf_hide_prefix() returns the barrier that was in force, which the
 * caller hands to pf_reveal_prefix() when it is done, so that the pair nests.
 *
 * Version 4.0 task running keeps the turn's text in the buffer while a task's
 * actions run, because a task that an action completes filters that text along
 * with its own (see pf_refilter()).  Pre-4.0 task running instead transfers
 * the text out and prepends it back afterwards, and the spacing helpers there
 * see an empty buffer for the duration.  Hiding rather than transferring keeps
 * that -- text run by an action opens its paragraph the same way under both --
 * so the version split stays confined to when the filter runs.
 */
size_t
pf_hide_prefix (scr_filterref_t filter)
{
  size_t previous;

  assert (pf_is_valid (filter));

  previous = filter->hidden;
  filter->hidden = filter->buffer.size ();

  /*
   * pf_transfer_buffer() clears these two as a side effect of resetting the
   * filter, and pf_prepend_string() does not put them back; match it.
   */
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;

  return previous;
}

void
pf_reveal_prefix (scr_filterref_t filter, size_t previous)
{
  assert (pf_is_valid (filter));

  filter->hidden = previous;
}


/*
 * pf_buffer_string()
 * pf_buffer_character()
 * pf_buffer_integer()
 *
 * Add a string, a single character, and a decimal integer, to the
 * printfilter buffer.  If muted, these functions do nothing.
 */
void
pf_buffer_string (scr_filterref_t filter, const scr_char *string)
{
  assert (pf_is_valid (filter));
  assert (string);

  /* Ignore the call if the printfilter is muted. */
  if (!filter->is_muted)
    {
      size_t noted;

      /* A pending join runs this text straight on from the last text, two
         spaces between, as the Runner's string concatenation would. */
      if (filter->join_pending)
        {
          filter->join_pending = FALSE;
          pf_undo_auto_break (filter);
          pf_buffer_pspace (filter);
        }

      /* Note append start, then append the string to the buffer. */
      noted = filter->buffer.size ();
      pf_append_string (filter, string);

      /* Adjust the first character of the appended string if flagged. */
      if (filter->new_sentence && noted < filter->buffer.size ())
        filter->buffer[noted] = scr_toupper (filter->buffer[noted]);

      /* Clear new sentence, and note as currently needing filtering. */
      filter->needs_filtering = TRUE;
      filter->new_sentence = FALSE;

      /*
       * Anything buffered invalidates a note of our own trailing newline,
       * and equally the two notes of a break the Runner keeps.  All three are
       * positions, and a position only means anything while it is still the
       * end of the buffer; left standing, a stale one collides with any later
       * buffer that happens to reach exactly that length.  Measured on
       * circus.taf: the room heading three turns earlier left hard_break_at
       * at 53, and "You get no reply from the videotape.\n" is 53 characters
       * too, so the walk announcement that should have joined it broke
       * instead.
       */
      filter->auto_break_at = -1;
      filter->hard_break_at = -1;
      filter->reference_at = -1;
    }
}

void
pf_buffer_character (scr_filterref_t filter, scr_char character)
{
  scr_char buffer[2];
  assert (pf_is_valid (filter));

  buffer[0] = character;
  buffer[1] = NUL;
  pf_buffer_string (filter, buffer);
}

void
pf_buffer_integer (scr_filterref_t filter, scr_int value)
{
  scr_char buffer[32];

  snprintf (buffer, sizeof (buffer), "%ld", value);
  pf_buffer_string (filter, buffer);
}


/*
 * pf_text_ends_with_break()
 * pf_text_leads_with_break()
 *
 * Return TRUE if text ends with / begins with a line break -- either a literal
 * newline or a "<br>" tag (the unfiltered buffer holds tags verbatim).
 */
scr_bool
pf_text_ends_with_break (const scr_char *text)
{
  scr_int length = strlen (text);

  if (length > 0 && text[length - 1] == '\n')
    return TRUE;
  if (length >= 4 && !scr_strncasecmp (text + length - 4, "<br>", 4))
    return TRUE;
  return FALSE;
}

static scr_bool
pf_text_leads_with_break (const scr_char *text)
{
  return text[0] == '\n' || !scr_strncasecmp (text, "<br>", 4);
}


/*
 * pf_text_ends_with_newline()
 *
 * TRUE if the text ends in a literal newline, as opposed to a "<br>" tag.
 * The distinction is the whole of pf_buffer_paragraph()'s test: a newline in
 * the buffer is one SCARIER put there to end a section of its own, while a
 * "<br>" still standing at the end is one the author wrote, and the Runner
 * has it too.
 */
static scr_bool
pf_text_ends_with_newline (const scr_char *text)
{
  scr_int length = strlen (text);

  return length > 0 && text[length - 1] == '\n';
}


/*
 * pf_buffer_paragraph()
 *
 * Buffer a block of text that conventionally begins with its own line break(s)
 * for spacing -- Adrift event and atmosphere texts typically start with "<br>"
 * or "<br><br>".  The Runner relies on those leading breaks alone for
 * paragraph spacing, whereas SCARIER also terminates the preceding room
 * description, exits list, NPC announcement and task text with a newline of
 * its own.  To avoid a doubled blank line, a single leading break is dropped
 * from the text -- but only when the break it would double is one of ours.
 *
 * The buffer says which it is.  Tags are translated at filter time, not here,
 * so an author's trailing "<br>" is still standing verbatim at the end of the
 * buffer, while every break SCARIER supplies is a literal newline.  A trailing
 * "<br>" is therefore the author's, the Runner has it too, and run400 really
 * does print a blank line between it and the next paragraph's "<br>" -- see
 * Ghost town's "...but follows you anyway.<br>" in Adrift_325_ghosttown.txt
 * 586-592.  The one literal newline that is not ours is 4.0's
 * "Time passes...\n" (Adrift_254_patient7.txt 81-87), which
 * pf_buffer_hard_break() records, and the 4.0 bracketed reference line, which
 * pf_buffer_reference() records.  Measured over the whole transcript archive:
 * harness/sweep_wine_breaks.py, and the write-up under "Ported 2026-09-07: a
 * leading <br> is collapsed only against a break of SCARIER's own".
 */
void
pf_buffer_paragraph (scr_filterref_t filter, const scr_char *string)
{
  const scr_char *buffered;

  assert (pf_is_valid (filter));
  assert (string);

  buffered = pf_get_buffer (filter);
  if (buffered
      && !filter->join_pending
      && pf_text_ends_with_newline (buffered)
      && pf_text_leads_with_break (string)
      && !(filter->hard_break_at >= 0
           && (size_t) filter->hard_break_at == filter->buffer.size ())
      && !(filter->reference_at >= 0
           && (size_t) filter->reference_at == filter->buffer.size ()))
    {
      /* Skip exactly one leading break -- a literal newline or a "<br>" tag. */
      string += (*string == '\n') ? 1 : 4;
    }
  pf_buffer_string (filter, string);
}


/*
 * pf_buffer_paragraph_line()
 *
 * Buffer a paragraph and terminate it with a line break, but only add one if
 * the paragraph did not already end with a break of its own -- the trailing
 * counterpart to the leading-break collapse above.
 *
 * Task and event texts are terminated with a newline so the next thing printed
 * starts on a fresh line.  Authors who end their text with "<br>" get that line
 * break as well, and the doubled break shows as a blank line the Adrift runner
 * does not print.  "The Warlord, The Princess & The Bulldog" builds its
 * inventory from one task per object, each with text like "*<tab>your uniform
 * (worn)<br>", so every item in the listing was followed by a blank line.
 */
void
pf_buffer_paragraph_line (scr_filterref_t filter, const scr_char *string)
{
  const scr_char *buffered;

  pf_buffer_paragraph (filter, string);

  filter->auto_break_at = -1;
  buffered = pf_get_buffer (filter);
  if (!buffered || !pf_text_ends_with_break (buffered))
    {
      size_t before = filter->buffer.size ();

      pf_buffer_character (filter, '\n');

      /* Note the terminator as ours, so that pf_undo_auto_break() can take it
         back.  Muting makes the call above a no-op, hence the length test. */
      if (filter->buffer.size () > before)
        filter->auto_break_at = (scr_int) filter->buffer.size ();
    }
}


/*
 * pf_undo_auto_break()
 *
 * Take back the line terminator that the last pf_buffer_paragraph_line()
 * supplied of its own accord, if it is still the last thing in the buffer.
 * Returns TRUE if one was removed.
 *
 * The pre-4.0 Runners do not terminate a task's text: whatever is printed
 * next either supplies its own separator or runs straight on from where the
 * text stopped.  (4.0 does terminate it, which is why the one caller so far,
 * task_print_end_game_message(), asks for this on a pre-4.0 game only.)
 * SCARIER instead ends each section with a newline, which is right for almost
 * every caller but wrong for the ones that the Runner butts up against the
 * text with nothing in between.  Measured live in run380 finishing
 * microwaveman.taf, where the winning task's "You win the game." and the
 * game's WinText appear as one run-on line, "You win the game.You have
 * destroyed Coffee Man...".  Only that section's own terminator is taken
 * back; a break the author wrote is left alone, which is why the position is
 * recorded rather than a flag.
 */
scr_bool
pf_undo_auto_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (filter->auto_break_at >= 0
      && (size_t) filter->auto_break_at == filter->buffer.size ()
      && !filter->buffer.empty ()
      && filter->buffer[filter->buffer.size () - 1] == '\n')
    {
      filter->buffer.erase (filter->buffer.size () - 1);
      filter->auto_break_at = -1;
      return TRUE;
    }

  return FALSE;
}


/*
 * pf_note_trailing_auto_break()
 *
 * Record a newline already at the end of the buffer as a terminator of our
 * own, as if pf_buffer_paragraph_line() had just supplied it, so that
 * pf_ends_with_double_space() can look through it at the text the Runner
 * really assembled.  Needed for the task ShowRoomDesc block: the pre-4.0
 * Runners store no break after a room description -- the next piece is glued
 * on by the two-space separator logic -- so the newline SCARIER's room
 * printer adds there stands where the Runner's string simply stopped.
 * Measured live on superliam.taf in run380 (2026-08-31): tasks 23 ("press
 * button") and 24 ("up") show rooms whose Long ends in two spaces, and the
 * real Runner suppresses both AdditionalMessages ("a cat runs out of a
 * door...", "X1 raising his gunlike arms...") where SCARIER printed them.
 */
void
pf_note_trailing_auto_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (!filter->buffer.empty ()
      && filter->buffer[filter->buffer.size () - 1] == '\n')
    filter->auto_break_at = (scr_int) filter->buffer.size ();
}


/*
 * pf_ends_with_double_space()
 *
 * TRUE if the text buffered so far ends in two spaces, ignoring a trailing
 * newline that pf_buffer_paragraph_line() supplied of its own accord.
 *
 * The Runners assemble a turn's output by concatenating onto one string, and
 * separate the pieces with two spaces.  Before adding that separator they ask
 * whether the string already ends in one, so an author's own trailing spaces
 * are not doubled up.  run380 wrote the test for the task's AdditionalMessage
 * the wrong way round -- see task_run_task_unrestricted() -- and that is the
 * only caller so far.  The auto-break is skipped because it is ours and not
 * the Runner's: it stands where the Runner's string simply stopped.
 */
scr_bool
pf_ends_with_double_space (scr_filterref_t filter)
{
  size_t length;

  assert (pf_is_valid (filter));

  length = filter->buffer.size ();
  if (filter->auto_break_at >= 0
      && (size_t) filter->auto_break_at == length
      && length > 0
      && filter->buffer[length - 1] == '\n')
    length--;

  return length >= 2
         && filter->buffer[length - 1] == ' '
         && filter->buffer[length - 2] == ' ';
}


/*
 * pf_text_trailing_breaks()
 * pf_buffer_paragraph_break()
 *
 * Ensure that whatever is buffered next opens a new paragraph: if the buffer
 * already holds text, top it up with breaks until it ends in a blank line.
 * Does nothing on an empty buffer, so a paragraph never opens with leading
 * whitespace.
 *
 * This exists for the inline room name.  With the 3.9+ "Room names in
 * descriptions" Appearance option ticked -- which is how the archive was
 * measured -- the Runner does print the name into the transcript, and puts a
 * break in front of it: the blank line this tops the buffer up to is the
 * Runner's own leading break landing after text that already ended in one.
 * (Pre-3.9 there is no heading at all, and lib_describe_player_room() never
 * calls this.)
 */
static scr_int
pf_text_trailing_breaks (const scr_char *text)
{
  scr_int length, count;

  count = 0;
  for (length = strlen (text); length > 0; )
    {
      if (text[length - 1] == '\n')
        {
          length -= 1;
          count++;
        }
      else if (length >= 4 && !scr_strncasecmp (text + length - 4, "<br>", 4))
        {
          length -= 4;
          count++;
        }
      else
        break;
    }

  return count;
}

void
pf_buffer_paragraph_break (scr_filterref_t filter)
{
  const scr_char *buffered;
  scr_int breaks;

  assert (pf_is_valid (filter));

  /*
   * Nothing buffered means nothing to separate from -- but look through the
   * hidden barrier before deciding that.  A 4.0 task's actions run with the
   * turn's text hidden (pf_hide_prefix), so a task an action runs shows its
   * room into what looks like an empty buffer, and the heading lost its
   * break; the Runner has no barrier and puts its own break in regardless.
   * Measured: baroo t120/t121, blood t30/t70, cursed t76/t134, thepkgirl
   * t125/t177/t257 and vendetta t113 all break before a task-driven room
   * heading where SCARIER ran it onto the line above.
   */
  buffered = pf_get_buffer (filter);
  if (!buffered || scr_strempty (buffered))
    {
      if (filter->hidden == 0 || filter->buffer.empty ())
        return;
      buffered = filter->buffer.c_str ();
    }

  for (breaks = pf_text_trailing_breaks (buffered); breaks < 2; breaks++)
    pf_buffer_character (filter, '\n');
}


/*
 * pf_buffer_join()
 * pf_buffer_join_always()
 *
 * Append a string as a continuation of the current output line, with the
 * Adrift runner's two-space sentence separator.  The runner builds a turn's
 * output as one paragraph joined with two spaces; our section printers
 * instead terminate each section with a newline of their own.  To join text
 * onto the previous section the way the runner does -- event look text runs
 * on after the room's character lines, and an NPC walk announcement runs on
 * after whatever the turn has printed -- remove a single terminating newline
 * first, then separate with two spaces unless the text before it ends with
 * an author's own break.
 *
 * 3.9 and 4.0 factor that out into a sub, pspace (run390 loc_45A99E, run400
 * Proc_21_50_44A9F4 @ General.bas:10341), which is
 *
 *   If buf <> "" And Right(buf, 2) <> "  " And Right(buf, 1) <> Chr(10)
 *      And Right(buf, 4) <> "<br>" Then buf = buf & "  "
 *
 * -- so text that already ends in the separator does not get a second one.
 * The two older runners write the test out inline at each site and it is
 * shorter, with no such clause:
 *
 *   run370 loc_4395AA / run380 loc_441740
 *      If Right(buf, 1) <> Chr(10) And Len(buf) > 0 Then buf = buf & "  "
 *
 * so a 3.7 or 3.8 join onto text ending in two spaces really does make four.
 * pf_buffer_join_always() is that form, for callers that carry the version
 * split; the "<br>" clause is not split, both being folded into
 * pf_text_ends_with_break(), because no game has yet shown the difference.
 */
static scr_bool
pf_buffer_ends_with_two_spaces (scr_filterref_t filter)
{
  const size_t length = filter->buffer.size ();

  return length >= filter->hidden + 2
         && filter->buffer[length - 1] == ' '
         && filter->buffer[length - 2] == ' ';
}

static void
pf_buffer_join_internal (scr_filterref_t filter, const scr_char *string,
                         scr_bool is_pspace)
{
  assert (pf_is_valid (filter));
  assert (string);

  if (!filter->is_muted && filter->buffer.size () > filter->hidden)
    {
      if (filter->buffer.back () == '\n')
        {
          /* A newline the Runner's own string carries stays, and pspace
             then adds no separator to text ending in Chr(10). */
          if (filter->hard_break_at >= 0
              && (size_t) filter->hard_break_at == filter->buffer.size ())
            {
              pf_buffer_string (filter, string);
              return;
            }
          filter->buffer.pop_back ();
        }

      if (filter->buffer.size () > filter->hidden
          && !pf_text_ends_with_break (filter->buffer.c_str ())
          && !(is_pspace && pf_buffer_ends_with_two_spaces (filter)))
        pf_append_string (filter, "  ");
    }
  pf_buffer_string (filter, string);
}

void
pf_buffer_join (scr_filterref_t filter, const scr_char *string)
{
  pf_buffer_join_internal (filter, string, TRUE);
}

void
pf_buffer_join_always (scr_filterref_t filter, const scr_char *string)
{
  pf_buffer_join_internal (filter, string, FALSE);
}


/*
 * pf_buffer_pspace()
 *
 * The pspace() sub by itself, for a caller that needs the separator without
 * pf_buffer_join()'s removal of a trailing newline: append the two-space
 * separator unless the buffer is empty or already ends with "  ", Chr(10)
 * or "<br>" (run390 @42C920).  run390's win path calls it unconditionally
 * before appending the game's WinText (execute_task loc_43F255), where the
 * accumulated text still ends with the winning task's unterminated text --
 * see task_print_end_game_message().  The caller is responsible for taking
 * back our own section terminator first (pf_undo_auto_break), since the
 * Runner's string never had one.
 */
void
pf_buffer_pspace (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (filter->is_muted)
    return;

  if (filter->buffer.size () > filter->hidden
      && !pf_text_ends_with_break (filter->buffer.c_str ())
      && !pf_buffer_ends_with_two_spaces (filter))
    pf_append_string (filter, "  ");
}


/*
 * pf_buffer_join_pending()
 * pf_clear_join_pending()
 *
 * Arrange for the NEXT text buffered, whatever prints it, to be joined onto
 * the text already there: our own trailing auto-break is taken back and the
 * Runner's two-space separator put in its place (pf_buffer_pspace), and only
 * then is the new text appended.  Nothing happens if nothing follows, which
 * is what makes this a note rather than an immediate join: the caller does
 * not yet know whether anything will.  Clear the note when the turn's
 * dispatch is over.
 *
 * The one user is the 4.0 put refusal that does not claim its command: run400
 * prints "The rock is too big to fit inside the slot." and then lets a
 * matching task answer the same line, concatenated onto the refusal --
 * "The rock is too big to fit inside the slot.  PUTBIG." (arena probe PUT7,
 * Adrift_87, 2026-09-05; Zack Smackfoot's `put knife in slot`, Adrift_57).
 * If no task claims, the refusal stands alone on its own line, exactly as
 * buffered.
 */
void
pf_buffer_join_pending (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->join_pending = TRUE;
}

void
pf_clear_join_pending (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->join_pending = FALSE;
}


/*
 * pf_buffer_hard_break()
 *
 * Note that the newline the buffer currently ends with is one the Runner
 * keeps in its output string, not a section terminator of ours, so that
 * pf_buffer_join() runs the joined text on after it rather than in place of
 * it.  Anything buffered afterwards makes the note stale by position.
 */
void
pf_buffer_hard_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (!filter->is_muted && !filter->buffer.empty ()
      && filter->buffer.back () == '\n')
    filter->hard_break_at = (scr_int) filter->buffer.size ();
}


/*
 * pf_prepend_string()
 *
 * Add a string to the front of the printfilter buffer, rather than to the
 * end.  Generally less efficient than an append, these are for use by task
 * running code, which needs to run task actions and then prepend the task's
 * completion text.  If muted, this function does nothing.
 */
void
pf_prepend_string (scr_filterref_t filter, const scr_char *string)
{
  assert (pf_is_valid (filter));
  assert (string);

  /* Ignore the call if the printfilter is muted. */
  if (!filter->is_muted)
    {
      /* Prepending leaves the tail of the buffer alone, so a note of our own
         trailing newline survives it -- including down the empty-buffer path
         below, which routes through pf_buffer_string() and would clear it. */
      const scr_int auto_break_at = filter->auto_break_at;

      if (!filter->buffer.empty ())
        {
          /* Take a copy of the current buffered string. */
          std::string copy = filter->buffer;

          /* Now restart buffering with the input string passed in. */
          filter->buffer.clear ();
          pf_append_string (filter, string);

          /* Append the string saved above; the copy frees itself. */
          pf_append_string (filter, copy.c_str ());

          /* Adjust the first character of the prepended string if flagged. */
          if (filter->new_sentence)
            filter->buffer[0] = scr_toupper (filter->buffer[0]);

          /* Clear new sentence, and note as currently needing filtering. */
          filter->needs_filtering = TRUE;
          filter->new_sentence = FALSE;
        }
      else
        /* No data, so the call is equivalent to a normal buffer. */
        pf_buffer_string (filter, string);

      filter->auto_break_at = auto_break_at;
    }
}


/*
 * pf_new_sentence()
 *
 * Tells the printfilter to force the next non-space character to uppercase.
 * Ignored if the printfilter is muted.
 */
void
pf_new_sentence (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (!filter->is_muted)
    filter->new_sentence = TRUE;
}


/*
 * pf_mute()
 * pf_clear_mute()
 *
 * A muted printfilter ignores all new text additions.
 */
void
pf_mute (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->is_muted = TRUE;
}

void
pf_clear_mute (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->is_muted = FALSE;
}


/*
 * pf_buffer_tag()
 *
 * Insert an HTML-like tag into the buffered output data.  The call is ignored
 * if the printfilter is muted.
 */
void
pf_buffer_tag (scr_filterref_t filter, scr_int tag)
{
  const scr_html_tags_t *entry;
  assert (pf_is_valid (filter));

  /* Search the tags table for this tag. */
  for (entry = HTML_TAGS_TABLE; entry->name; entry++)
    {
      if (tag == entry->tag)
        break;
    }

  /* If found, output the equivalent string, enclosed in '<>' characters. */
  if (entry->name)
    {
      pf_buffer_character (filter, LESSTHAN);
      pf_buffer_string (filter, entry->name);
      pf_buffer_character (filter, GREATERTHAN);
    }
  else
    scr_error ("pf_buffer_tag: invalid tag, %ld\n", tag);
}


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
      if (allow_newlines)
        {
          if (tag_end - cursor == 3
              && scr_strncasecmp (cursor + 1, "br", 2) == 0)
            *cursor++ = '\n';
        }

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
 * Adrift offers no escapes for & and + escapes, so for these we convert to
 * the character itself followed by a space.  The return string is malloc'ed,
 * so the caller needs to remember to free it.
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

  return pf_strdup (buffer);
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
  for (word_posn = 0; scr_isspace (words[word_posn]) && words[word_posn] != NUL;)
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

          if (pf_trace)
            scr_trace ("Printfilter: %s \"%s\"\n", what, buffer.c_str ());
        }
      else
        offset += strcspn (current + offset, WHITESPACE);

      offset += strspn (current + offset, WHITESPACE);
    }
}

/*
 * pf_filter_input()
 *
 * Applies synonym changes to a player input string, and returns the resulting
 * string to the caller, or NULL if no synonym changes were needed.  The
 * return string is malloc'ed, so the caller needs to remember to free it.
 */
scr_char *
pf_filter_input (const scr_char *string, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  scr_int synonym_count, index_;
  std::string buffer;
  scr_bool modified;
  const scr_char *current;
  assert (string && bundle);

  if (pf_trace)
    scr_trace ("Printfilter: input \"%s\"\n", string);

  /* Obtain a count of synonyms. */
  vt_key[0].string = "Synonyms";
  synonym_count = prop_get_child_count (bundle, "I<-s", vt_key);

  /*
   * Resolve the immutable synonym table from the property tree once per game
   * (see the string-pair caches above); after that the word loop reads it
   * directly, with no prop_get() lookups.  The count is fixed for a game, so a
   * lazy build on first use is safe.
   */
  if (!pf_synonym_cache_built
      || (scr_int) pf_synonym_cache.size () != synonym_count)
    pf_synonym_cache_build (bundle, synonym_count);

  /*
   * The Runner applies the table as a SEQUENCE OF WHOLE-STRING REWRITES, in
   * table order: synonym 0 replaces every whole-word occurrence of its
   * original in the input, synonym 1 does the same to synonym 0's output,
   * and so on.  A later synonym therefore sees -- and rewrites -- the words
   * an earlier one wrote, and an earlier synonym never sees a later one's.
   *
   * Vardock Bates pins this (run400 under Wine, Adrift_3_vardock_bates.txt 2026-08-29).
   * Its table has hablar->talk [101], then jason->"jason dhirco" [160], then
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
   * "ask harris" ends as harris) and Yak Shaving (flags->"clothes line",
   * then line and clothes -> "clothes line": "x flags" grows to "x clothes
   * line clothes line line", which the containment matcher still resolves)
   * both fit, and were the two games the previous first-match-then-whole-
   * only rule was built around.
   */
  modified = FALSE;
  current = string;

  for (index_ = 0; index_ < synonym_count; index_++)
    {
      const pf_str_pair_t &entry = pf_synonym_cache[index_];

      pf_rewrite_whole_words (string, buffer, modified, current,
                              entry.original, entry.replacement,
                              entry.replacement_length, "synonym");
    }

  /*
   * The Runner's own built-in rewrites come AFTER the game's synonyms, in
   * this order, each a whole-word change() of every occurrence (change() is
   * a c()-driven loop, case-insensitive):
   *
   *   run370 43B41F everything->all, 43B430 slap->hit
   *   run380 441C3F everything->all, 441C50 slap->hit,
   *          441C61 take->get,       441C72 except->but
   *
   * 3.9 and 4.0 replace everything/slap/except/"apart from" too (run390
   * 45F222-45F28B, run400 48A330 area) but as substring Replace()s, and
   * neither touches `take`; 4.0 task matching is verb-literal.  Those
   * versions' rewrites are covered by the grammar alternatives instead.
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
  {
    struct pf_builtin_rewrite_t
    {
      const scr_char *original;
      const scr_char *replacement;
      scr_int min_version, max_version;
    };
    static const pf_builtin_rewrite_t BUILTIN[] = {
      {"everything", "all", TAF_VERSION_370, TAF_VERSION_380},
      {"slap", "hit", TAF_VERSION_370, TAF_VERSION_380},
      {"take", "get", TAF_VERSION_380, TAF_VERSION_380},
      {"except", "but", TAF_VERSION_380, TAF_VERSION_380},
    };
    scr_int version = prop_get_taf_version (bundle);

    for (const pf_builtin_rewrite_t &entry : BUILTIN)
      {
        if (version < entry.min_version || version > entry.max_version)
          continue;
        pf_rewrite_whole_words (string, buffer, modified, current,
                                entry.original, entry.replacement,
                                strlen (entry.replacement), "builtin");
      }
  }

  /* Return the final string, or NULL if no replacements. */
  return modified ? pf_strdup (buffer) : NULL;
}


/*
 * pf_debug_trace()
 *
 * Set filter tracing on/off.
 */
void
pf_debug_trace (scr_bool flag)
{
  pf_trace = flag;
}

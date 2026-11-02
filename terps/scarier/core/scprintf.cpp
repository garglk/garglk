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
 * The printfilter object: creation and destruction, the per-game ALR and
 * synonym caches, and tracing.  The filter's behaviour lives in the four
 * scprintf_*.cpp files listed in scprintf_internal.h.
 */

#include "scprintf_internal.h"

/* Assorted definitions and constants. */
static const scr_uint PRINTFILTER_MAGIC = 0xb4736417;

/* The printfilter's module state; see scr_pf_state_t. */
scr_pf_state_t pf;

/* The HTML-like tag table; see scprintf_internal.h. */
const scr_html_tags_t HTML_TAGS_TABLE[] = {
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
  {nullptr, 0, SCR_TAG_UNKNOWN}
};

/*
 * pf_state_reset()
 * pf_str_pair_set()
 *
 * Put the per-game printfilter state back to its start -- drop both caches,
 * for a new game; the trace flag stays, since the interface sets it before
 * any game exists.  And populate one cache entry from its resolved original
 * and replacement strings, for both builders below.
 */
static void
pf_state_reset (void)
{
  pf.alr = {};
  pf.synonym = {};
}

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
 * pf.alr.index.  The caller guarantees that many bytes are readable.
 */
static scr_uint
pf_alr_key (const scr_char *text)
{
  const unsigned char *bytes = (const unsigned char *) text;

  return (scr_uint) bytes[0] | ((scr_uint) bytes[1] << 8)
         | ((scr_uint) bytes[2] << 16) | ((scr_uint) bytes[3] << 24);
}


/*
 * pf_alr_cache_build()
 * pf_synonym_cache_build()
 *
 * Resolve the ALR and Synonym tables from the property tree into the caches
 * above.  The ALRs are taken in the length-sorted order held in "ALRs2" and
 * indexed for pf_alr_candidates(); the synonyms in raw property order.
 */
void
pf_alr_cache_build (scr_prop_setref_t bundle, scr_int alr_count)
{
  scr_int index_;

  pf.alr.cache.clear ();
  pf.alr.cache.reserve (alr_count);

  for (index_ = 0; index_ < alr_count; index_++)
    {
      scr_vartype_t vt_key[3];
      pf_str_pair_t entry;
      const scr_char *original, *replacement;
      scr_int alr;

      /*
       * The ALRs are applied in a length-sorted order held in ALRs2; map this
       * slot to the underlying ALR the way pf_replace_alrs() always has.
       */
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
      pf.alr.cache.push_back (entry);
    }

  /* Index the cache for pf_alr_candidates(). */
  pf.alr.index.clear ();
  pf.alr.filter.assign ((size_t) 1 << (PF_ALR_FILTER_BITS - 3), 0);
  pf.alr.unkeyed.clear ();
  pf.alr.trailing_space = FALSE;
  for (index_ = 0; index_ < alr_count; index_++)
    {
      const pf_str_pair_t &entry = pf.alr.cache[index_];

      if (entry.original_length == 0)
        continue;
      if (entry.original[entry.original_length - 1] == ' ')
        pf.alr.trailing_space = TRUE;
      if (entry.original_length < PF_ALR_KEY_LENGTH)
        pf.alr.unkeyed.push_back (index_);
      else
        {
          const scr_uint key = pf_alr_key (entry.original);
          const size_t slot = pf_alr_filter_slot (key);

          pf.alr.index[key].push_back (index_);
          pf.alr.filter[slot >> 3] |= 1u << (slot & 7);
        }
    }

  pf.alr.built = TRUE;
}

void
pf_synonym_cache_build (scr_prop_setref_t bundle, scr_int synonym_count)
{
  scr_int index_;

  pf.synonym.cache.clear ();
  pf.synonym.cache.reserve (synonym_count);

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
      pf.synonym.cache.push_back (entry);
    }

  pf.synonym.built = TRUE;
}


/*
 * pf_is_valid()
 *
 * Return TRUE if pointer is a valid printfilter, FALSE otherwise.
 */
scr_bool
pf_is_valid (scr_filterref_t filter)
{
  return filter && filter->magic == PRINTFILTER_MAGIC;
}


/*
 * pf_reset_state()
 *
 * Set a filter's state flags and markers to their initial values.  The
 * buffer is the caller's to empty.
 */
void
pf_reset_state (scr_filterref_t filter)
{
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
  filter->needs_filtering = FALSE;
  filter->auto_break_at = -1;
  filter->hard_break_at = -1;
  filter->hidden = 0;
  filter->frozen = 0;
  filter->printed_to = -1;
  filter->reference_at = -1;
  filter->join_pending = FALSE;
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

  /*
   * A new filter means a new game (run_create is the sole caller): drop the
   * string-pair caches held for the previous game, whose bundle is going
   * away.
   */
  pf_state_reset ();

  /* Create a new printfilter; 'buffer' default-constructs empty. */
  filter = new scr_filter_t ();
  filter->magic = PRINTFILTER_MAGIC;
  pf_reset_state (filter);

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
 * pf_debug_trace()
 *
 * Set filter tracing on/off.
 */
void
pf_debug_trace (scr_bool flag)
{
  pf.trace = flag;
}

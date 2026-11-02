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

#ifndef SCPRINTF_INTERNAL_H
#define SCPRINTF_INTERNAL_H

/*
 * Internal header of the printfilter: what scprintf.cpp and the four files
 * split out of it share.  They were one translation unit, so much here is
 * shared state rather than interface.
 *
 *   scprintf.cpp         the filter object, its create/destroy, the ALR
 *                        and synonym caches, tracing
 *   scprintf_alr.cpp     %variable% interpolation and the ALR walk
 *   scprintf_output.cpp  the output side: tag dissection, entity
 *                        unescaping, the filter passes and pf_flush()
 *   scprintf_buffer.cpp  the pending-output buffer and its Runner
 *                        paragraph, break and join rules
 *   scprintf_input.cpp   the input side: tag stripping, escaping, the
 *                        synonym and word rewrites, pf_filter_input()
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
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "scvars.h"
#include "scprintf.h"
#include "sclocale.h"
#include "scinterf.h"
#include "scdump.h"

/* Assorted definitions and constants. */
enum
{ ITERATION_LIMIT = 32
};
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

extern const scr_html_tags_t HTML_TAGS_TABLE[];

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
   * newline of its own; -1 if the last one did not, or if anything has been
   * buffered since.  See pf_undo_auto_break(). */
  scr_int auto_break_at;
  /* Buffer length just after a newline that a Runner really stores in its
   * output string (4.0's "Time passes..." & vbCrLf), so that a join keeps it
   * instead of popping it, and so that pf_buffer_paragraph() does not collapse
   * a leading break against it; -1 otherwise.  See pf_buffer_hard_break(). */
  scr_int hard_break_at;
  /* Length of a prefix of the buffer that the paragraph-spacing helpers are
   * to treat as if it were not there.  See pf_hide_prefix(). */
  size_t hidden;
  /* Length of a prefix of the buffer the Runner has already printed, and so
   * already filtered: no later filtering touches it.  See pf_print_so_far(). */
  size_t frozen;
  /* Buffer length at the last pf_print_so_far(), where the Runner's own
   * string restarted; -1 otherwise.  Unlike frozen, it rides the task
   * running transfer-and-prepend round trip, shifted by the text put back in
   * front of it.  See pf_printed_to(). */
  scr_int printed_to;
  /* Buffer length just after pf_buffer_reference() buffered one of the 4.0
   * Runner's bracketed reference lines; -1 otherwise.  While the line is
   * still the last thing buffered, pf_buffer_paragraph() leaves a leading
   * break on the next text alone. */
  scr_int reference_at;
  /* TRUE while the next text buffered is to be joined onto the current
   * end of the buffer with the Runner's two-space separator, taking back an
   * auto-break of ours first.  See pf_buffer_join_pending(). */
  scr_bool join_pending;
  /* Everything pf_flush() has printed since pf_take_printed() last took it:
   * the output of the turn in progress, for undo to replay later. */
  std::string printed;
} scr_filter_t;


/*
 * Original/replacement string-pair table caches.
 *
 * Two immutable game tables drive the filters below as (original ->
 * replacement) string pairs: the ALRs (Automatic Language Replacements)
 * applied to output by pf_replace_alrs(), and the Synonyms applied to input
 * by pf_filter_input().
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
 * Lifetime: both are reset by pf_state_reset() from pf_create(), which
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

/*
 * Multi-pattern prefilter for the ALR loop (see pf_alr_candidates() in
 * scprintf_alr.cpp).  pf.alr.index maps the four-byte prefix of an ALR
 * original to every cache slot that starts with it; originals shorter than
 * four bytes are too weak a key for that, and live in pf.alr.unkeyed where
 * they are probed with strstr() as before.  Empty originals are in neither --
 * pf_replace_alr() refuses them anyway.
 */
enum
{ PF_ALR_KEY_LENGTH = 4
};
typedef std::unordered_map<scr_uint, std::vector<scr_int> > pf_alr_index_t;

/*
 * A bit per hashed key in front of pf.alr.index: nearly every position of
 * the text starts no original at all, and one bit test turns it away without
 * a hash-table probe.
 */
enum
{ PF_ALR_FILTER_BITS = 16
};

static inline size_t
pf_alr_filter_slot (scr_uint key)
{
  return (size_t) (((unsigned int) key * 2654435761u)
                   >> (32 - PF_ALR_FILTER_BITS));
}

/*
 * The printfilter's module state, in one instance (pf, defined in
 * scprintf.cpp).  It is grouped by lifetime:
 *
 *   trace    the interface's printfilter trace flag (pf_debug_trace()), set
 *            before any game exists and kept across them.
 *   alr      the ALR cache described above, with its prefilter index, the
 *            trailing-space flag pf_replace_alrs() keys on, and the
 *            recursion-path marks of pf_alr_walk().  Per game.
 *   synonym  the synonym cache, in raw property order.  Per game.
 *
 * Nothing here is saved: both caches are rebuilt lazily from the bundle on
 * first use, and the walk's path marks are all clear between walks.
 * pf_state_reset() drops the per-game groups; pf_create() calls it.
 */
struct scr_pf_state_s
{
  scr_bool trace = FALSE;

  struct
  {
    std::vector<pf_str_pair_t> cache;
    scr_bool built = FALSE;
    /* TRUE when some ALR original ends in a space; see pf_replace_alrs(). */
    scr_bool trailing_space = FALSE;
    pf_alr_index_t index;
    std::vector<unsigned char> filter;  /* see pf_alr_filter_slot() */
    std::vector<scr_int> unkeyed;   /* slots too short for the index */
    std::vector<char> on_path;      /* ALRs on the recursion path of a walk */
  } alr;

  struct
  {
    std::vector<pf_str_pair_t> cache;
    scr_bool built = FALSE;
  } synonym;
};
typedef struct scr_pf_state_s scr_pf_state_t;

extern scr_pf_state_t pf;


/* The functions the five files share; each is defined where it is
 * described, next to the rest of its topic. */

/* scprintf.cpp */
extern scr_bool pf_is_valid (scr_filterref_t filter);
extern void pf_reset_state (scr_filterref_t filter);
extern void pf_alr_cache_build (scr_prop_setref_t bundle, scr_int alr_count);
extern void pf_synonym_cache_build (scr_prop_setref_t bundle,
                                    scr_int synonym_count);

/* scprintf_alr.cpp */
extern scr_char *pf_interpolate_vars (const scr_char *string,
                                      scr_var_setref_t vars);
extern scr_bool pf_replace_alr (const scr_char *string, std::string &out,
                                const pf_str_pair_t &entry,
                                const scr_char *replacement);
extern void pf_alr_walk (const scr_char *string, std::string &out,
                         scr_var_setref_t vars, scr_bool recursive,
                         scr_int depth);
extern scr_char *pf_replace_alrs (const scr_char *string,
                                  scr_var_setref_t vars,
                                  scr_prop_setref_t bundle,
                                  scr_int alr_count, scr_bool recursive);

/* scprintf_output.cpp */
extern scr_bool pf_filter_buffer (scr_filterref_t filter,
                                  scr_var_setref_t vars,
                                  scr_prop_setref_t bundle);

#endif

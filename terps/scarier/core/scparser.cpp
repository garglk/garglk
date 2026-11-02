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
 * The parser's entry point: uip_match() and its cache of compiled patterns,
 * pattern-match tracing, and the string cleansing the other files share.  The
 * parser is split by topic; scparser_internal.h lists the files and what each
 * holds.
 */

#include "scparser_internal.h"


/* The parser's module state; see scr_uip_state_t. */
scr_uip_state_t uip;


/*
 * uip_state_reset()
 *
 * Put the between-games parser state back to its start: the options, the
 * candidate cache and the pronoun registers.  run_create() calls this, so a
 * game created after another in the same process starts the parser clean.
 * The pools stay, since the cached compiled patterns below are built from
 * them; the per-call groups are cleared by their own start/end pairs.
 */
void
uip_state_reset (void)
{
  uip.options = {};
  uip_forget_game (uip.cache.game);
  uip.pronoun = {};
}


/*
 * uip_cleanse_string()
 * uip_free_cleansed_string()
 *
 * Given a string, copy it to the given buffer, and trim leading and (unless
 * told otherwise) trailing spaces from it.  If it does not fit, malloc
 * enough for a string copy and use that instead.  The caller needs to free
 * this allocation if it happens (detectable by comparing the return value
 * to the buffer passed in), or call uip_free_cleansed_string.
 */
scr_char *
uip_cleanse_string (const scr_char *original, scr_char *buffer, scr_int length,
                    scr_bool trim_trailing)
{
  scr_int required;
  scr_char *string;

  /*
   * Use the supplied buffer if it is long enough, otherwise allocate, and
   * copy the string.
   */
  required = strlen (original) + 1;
  string = (required < length)
           ? buffer : (decltype(+buffer)) scr_malloc (required);
  strncpy (string, original, required);

  /*
   * Trim, and return the string.  A PATTERN keeps its trailing space: the
   * Runner treats one as a space the input must have, so a task command
   * spelled "get placemat " matches nothing (see uip_match_whitespace()).
   * Leading space is dropped from both -- uip_match_whitespace() ignores a
   * leading one in the pattern anyway.
   */
  if (trim_trailing)
    scr_trim_string (string);
  else
    {
      scr_char *const start = string;
      scr_int skip = 0;

      while (scr_isspace (start[skip]))
        skip++;
      if (skip > 0)
        memmove (start, start + skip, strlen (start + skip) + 1);
    }
  return string;
}

scr_char *
uip_free_cleansed_string (scr_char *string, const scr_char *buffer)
{
  /* Free if the string was allocated by the function above. */
  if (string != buffer)
    scr_free (string);

  /* Always returns NULL, for the syntactic convenience of the caller. */
  return nullptr;
}


/*
 * uip_debug_trace()
 *
 * Set pattern match tracing on/off.
 */
void
uip_debug_trace (scr_bool flag)
{
  uip.trace = flag;
}


/*
 * Cache of compiled patterns, keyed by the original pattern text.
 *
 * uip_match() historically built and destroyed a fresh parse tree on every
 * call, and with the whole library plus every task command template matched
 * against each input line, that tree churn profiles at a third of the whole
 * interpreter.  Library patterns come from fixed tables, so parse each
 * distinct pattern once and keep it.  (Task commands are compiled by the
 * runner instead -- see uip_compile_pattern() -- and a large game has more
 * than this cache holds.)
 *
 * Cached patterns are never destroyed or evicted.  Nothing in the matcher
 * calls back into uip_match() today, but eviction would still be the one way
 * for a cached tree to vanish under a walk, so if the cache ever fills,
 * further patterns just fall back to parse-and-destroy.
 *
 * The keys are private copies of the patterns (never freed, like the
 * trees), hashed and compared as C strings, so a lookup builds no
 * std::string.
 */
struct uip_cstring_hash
{
  size_t
  operator() (const scr_char *string) const
  {
    size_t hash = 2166136261u;              /* FNV-1a */

    for (; *string != NUL; string++)
      hash = (hash ^ (unsigned char) *string) * 16777619u;
    return hash;
  }
};

struct uip_cstring_equal
{
  bool
  operator() (const scr_char *left, const scr_char *right) const
  {
    return strcmp (left, right) == 0;
  }
};

enum { UIP_TREE_CACHE_SIZE = 4096 };
static std::unordered_map<const scr_char *, scr_uip_patternref_t,
                          uip_cstring_hash, uip_cstring_equal> uip_tree_cache;


/*
 * uip_match()
 *
 * Match a string to a pattern, and return TRUE on match, FALSE otherwise,
 * reusing the pattern's compiled form through uip_tree_cache.
 */
scr_bool
uip_match (const scr_char *pattern, const scr_char *string, scr_gameref_t game)
{
  scr_uip_patternref_t compiled;
  scr_bool match, is_cached;
  assert (pattern && string && game);

  /* Reuse any previously compiled pattern. */
  const auto cached = uip_tree_cache.find (pattern);
  if (cached != uip_tree_cache.end ())
    {
      compiled = cached->second;
      is_cached = TRUE;
      if (uip.trace && compiled->tree)
        scr_trace ("UIParser: pattern \"%s\" (cached tree)\n", pattern);
    }
  else
    {
      compiled = uip_compile_pattern (pattern);
      is_cached = uip_tree_cache.size () < UIP_TREE_CACHE_SIZE;
      if (is_cached)
        {
          const size_t length = strlen (pattern);
          scr_char *const key = (decltype(+key)) scr_malloc (length + 1);

          memcpy (key, pattern, length + 1);
          uip_tree_cache.emplace (key, compiled);
        }
    }

  match = uip_match_pattern (compiled, string, game);
  if (!is_cached)
    uip_destroy_pattern (compiled);
  return match;
}

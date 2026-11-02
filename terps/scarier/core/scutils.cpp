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
 * Module notes:
 *
 * o Implement smarter selective module tracing.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "sclocale.h"

/*
 * The shared erkyrath_random() (terps/common_utils/randomness.c, vendored in
 * common_utils/ for out-of-tree builds): the seedable xoshiro128** that the
 * ADRIFT 5 engine and the Scott, TaylorMade, Plus and Comprehend ports draw
 * from.  The Runner-compatible generator below is that one, in every build;
 * the Spatterlight build routes the other two handlers through it as well.
 */
#include "common_utils/randomness.h"


/*
 * scr_trace()
 *
 * Debugging trace function; printf wrapper that writes to stderr.
 */
void
scr_trace (const scr_char *format, ...)
{
  va_list ap;
  assert (format);

  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
}


/*
 * scr_error()
 * scr_fatal()
 *
 * Error reporting functions.  scr_error() prints a message and continues.
 * scr_fatal() prints a message, then calls abort().
 */
void
scr_error (const scr_char *format, ...)
{
  va_list ap;
  assert (format);

  fprintf (stderr, "scarier: ");
  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
}

void
scr_fatal (const scr_char *format, ...)
{
  va_list ap;
  scr_char message[1024];
  assert (format);

  /* Format the message once, both for stderr diagnostics and the exception. */
  va_start (ap, format);
  vsnprintf (message, sizeof (message), format, ap);
  va_end (ap);

  fprintf (stderr, "scarier: internal error: %s", message);

  /*
   * Throw rather than abort().  The public entry points in scinterf.cpp catch
   * this at the host boundary and return a clean failure, so a corrupt or
   * pathological game does not take down the whole application.  From an
   * unwrapped path it still terminates the process, as abort() did.
   */
  throw scr_fatal_error (message);
}


/* Unique non-heap address for zero size malloc() and realloc() requests. */
static void *scr_zero_allocation = &scr_zero_allocation;


/*
 * scr_malloc()
 * scr_realloc()
 * scr_free()
 *
 * Non-failing wrappers around malloc functions.  Newly allocated memory is
 * cleared to zero.  In ANSI/ISO C, zero byte allocations are implementation-
 * defined, so we have to take special care to get predictable behavior.
 */
void *
scr_malloc (size_t size)
{
  void *allocated;

  if (size == 0)
    return scr_zero_allocation;

  allocated = (decltype(allocated)) malloc (size);
  if (!allocated)
    scr_fatal ("scr_malloc: requested %lu bytes\n", (scr_uint) size);
  else if (allocated == scr_zero_allocation)
    scr_fatal ("scr_malloc: zero-byte allocation address returned\n");

  memset (allocated, 0, size);
  return allocated;
}

void *
scr_realloc (void *pointer, size_t size)
{
  void *allocated;

  if (size == 0)
    {
      scr_free (pointer);
      return scr_zero_allocation;
    }

  if (pointer == scr_zero_allocation)
    pointer = nullptr;

  allocated = (decltype(allocated)) realloc (pointer, size);
  if (!allocated)
    scr_fatal ("scr_realloc: requested %lu bytes\n", (scr_uint) size);
  else if (allocated == scr_zero_allocation)
    scr_fatal ("scr_realloc: zero-byte allocation address returned\n");

  if (!pointer)
    memset (allocated, 0, size);
  return allocated;
}

void
scr_free (void *pointer)
{
  if (scr_zero_allocation != &scr_zero_allocation)
    scr_fatal ("scr_free: write to zero-byte allocation address detected\n");

  if (pointer && pointer != scr_zero_allocation)
    free (pointer);
}


/*
 * scr_strdup()
 *
 * Copy a std::string into a freshly scr_malloc'ed C string, so callers that
 * expect to scr_free() the result keep working unchanged.  The boundary between
 * std::string internals and the engine's char* contract.  The C string form
 * is the engine's strdup(): a copy to scr_free(), never NULL.
 */
scr_char *
scr_strdup (const std::string &string)
{
  scr_char *buffer = (scr_char *) scr_malloc (string.size () + 1);
  memcpy (buffer, string.c_str (), string.size () + 1);
  return buffer;
}

scr_char *
scr_strdup (const scr_char *string)
{
  const size_t size = strlen (string) + 1;
  scr_char *buffer = (scr_char *) scr_malloc (size);

  memcpy (buffer, string, size);
  return buffer;
}


/*
 * scr_strncasecmp()
 * scr_strcasecmp()
 *
 * Strncasecmp and strcasecmp are not ANSI functions, so here are local
 * definitions to do the same jobs.
 */
scr_int
scr_strncasecmp (const scr_char *s1, const scr_char *s2, scr_int n)
{
  scr_int index_;
  assert (s1 && s2);

  for (index_ = 0; index_ < n; index_++)
    {
      scr_int diff;

      /* Equal bytes fold equal; skip the two table lookups. */
      if (s1[index_] == s2[index_])
        continue;

      diff = scr_tolower (s1[index_]) - scr_tolower (s2[index_]);
      if (diff != 0)
        return diff < 0 ? -1 : 1;
    }

  return 0;
}

scr_int
scr_strcasecmp (const scr_char *s1, const scr_char *s2)
{
  scr_int s1len, s2len, result;
  assert (s1 && s2);

  s1len = strlen (s1);
  s2len = strlen (s2);

  result = scr_strncasecmp (s1, s2, s1len < s2len ? s1len : s2len);
  if (result != 0)
    return result;
  else
    return s1len < s2len ? -1 : s1len > s2len ? 1 : 0;
}


/*
 * scr_platform_rand()
 * congruential_step()
 * scr_congruential_rand()
 *
 * Internal random number generation functions.  We offer two: one is a self-
 * seeding wrapper around the platform's rand(), which should generate good
 * random numbers but with a sequence that is platform-dependent; the other
 * is a linear congruential generator with a long period that is guaranteed
 * to return the same sequence for all platforms.  The default is the first,
 * with the latter intended for predictability of game actions.
 *
 * Both branches below are live.  SPATTERLIGHT is defined only by the Xcode
 * build, which routes both generators through the shared erkyrath_random();
 * the headless test suite and the plain `make` builds take the #else branch
 * and keep SCARE's original rand() wrapper and congruential generator.
 */
#ifdef SPATTERLIGHT


/*
 * In the Spatterlight build both handlers draw from the shared
 * erkyrath_random().  That generator is itself portable and reproducible when
 * seeded, so the two modes differ only in their default (unseeded) seed: the
 * platform handler uses a native seed (0 -> erkyrath picks its own), while the
 * congruential ("portable, predictable") handler defaults to the fixed seed 1.
 * Keeping the two function bodies distinct also preserves the function-pointer
 * identity that scr_is_congruential_random() relies on.  The low bit is dropped
 * to map the unsigned 32-bit value to a positive signed result.
 */
static scr_int
scr_platform_rand (scr_uint new_seed)
{
  static scr_bool is_seeded = FALSE;

  if (new_seed > 0)
    {
      set_erkyrath_random (new_seed);
      is_seeded = TRUE;
      return 0;
    }
  if (!is_seeded)
    {
      set_erkyrath_random (0);
      is_seeded = TRUE;
    }
  return (scr_int) (erkyrath_random () >> 1);
}

static scr_int
scr_congruential_rand (scr_uint new_seed)
{
  static scr_bool is_seeded = FALSE;

  if (new_seed > 0)
    {
      set_erkyrath_random (new_seed);
      is_seeded = TRUE;
      return 0;
    }
  if (!is_seeded)
    {
      set_erkyrath_random (1);
      is_seeded = TRUE;
    }
  return (scr_int) (erkyrath_random () >> 1);
}

#else

static scr_int
scr_platform_rand (scr_uint new_seed)
{
  static scr_bool is_seeded = FALSE;

  /* If reseeding, seed with the value supplied, note seeded, and return 0. */
  if (new_seed > 0)
    {
      srand (new_seed);
      is_seeded = TRUE;
      return 0;
    }
  else
    {
      /* If not explicitly seeded yet, generate a seed from time(). */
      if (!is_seeded)
        {
          srand ((scr_uint) time (nullptr));
          is_seeded = TRUE;
        }

      /* Return the next rand() number in the sequence. */
      return rand ();
    }
}

static scr_uint congruential_state = 1;


/*
 * congruential_step()
 *
 * Advance the random state, using constants from Park & Miller (1988).
 * To keep the values the same for both 32 and 64 bit longs, mask out any
 * bits above the bottom 32.  The cycle length is 2^30.
 */
static void
congruential_step (void)
{
  congruential_state = (congruential_state * 16807 + 2147483647) & 0xffffffff;
}

static scr_int
scr_congruential_rand (scr_uint new_seed)
{
  static scr_bool is_seeded = FALSE;

  /* If reseeding, seed with the value supplied, and note seeded. */
  if (new_seed > 0)
    {
      congruential_state = new_seed;
      is_seeded = TRUE;

      /*
       * Discard one value before returning any, so that the seed itself is
       * never one step away from the first result.  The generator's additive
       * constant is 2147483647, so for every seed below 127774 the state after
       * a single step lands in [2^31, 2^32) -- the top bit is pinned to 1, and
       * scr_randomint()'s multiply-shift then maps that to the TOP HALF of
       * whatever range it is given, whatever the seed.  Before this warm-up the
       * first draw of a session was therefore constant: rand(0,1) returned 1
       * for all of seeds 1..127773, rand(1,6) never returned 1-3, and so on.
       * Games whose very first roll is a coin flip had that flip decided for
       * them -- Scandal.taf's opening sea-battle turn could never come up "the
       * Croatoan fires", where run400 fires about half the time.  One step is
       * enough; from the second draw on every range tested is uniform across
       * seeds.
       */
      congruential_step ();
      return 0;
    }
  else
    {
      /* If not explicitly seeded yet, generate a seed from time(). */
      if (!is_seeded)
        {
          congruential_state = (scr_uint) time (nullptr);
          is_seeded = TRUE;
          congruential_step ();
        }

      congruential_step ();

      /*
       * Discard the lowest bit as a way to map 32-bits unsigned to a 32-bit
       * positive signed.
       */
      return congruential_state >> 1;
    }
}

#endif


/*
 * scr_runner_rand()
 *
 * Runner-compatible generator: erkyrath_random()'s xoshiro128** with
 * SplitMix32 seeding, which is also what the vbrng.dll hook feeds the Wine
 * ADRIFT Runners of the Wine Runner harness.  A Runner
 * under that hook sees `Rnd` = (w >> 8) / 2^24 -- VB's 24-bit Single -- and
 * rolls `Int(Rnd * n) + lo`.  Presenting the same word here as the 31-bit
 * value (w >> 8) << 7 makes scr_randomint()'s multiply-shift compute exactly
 * floor(((w >> 8) * n) / 2^24) = Int(Rnd * n), so Scarier and the Runner draw
 * identical integers from identical words: a seed-matched transcript pair can
 * then be diffed byte for byte.  Selected by SCR_RNG=xoshiro (see
 * scr_set_portable_random); SCR_TRACE_RAND=1 prints every draw to stderr in
 * the VBRNG_TRACE format, `RND #n = f (w=xxxxxxxx)`, so the two streams can be
 * aligned draw for draw.
 *
 * The generator's words are erkyrath_random()'s own; the draw count, the
 * trace switch and the default seed are kept here.  A seed is its low 32
 * bits, and seed 0 means "pick one" to set_erkyrath_random(), so a seed
 * whose low word is zero is given as 1.
 */
static unsigned int runner_draws = 0;
static scr_bool runner_trace = FALSE;
static scr_bool runner_seeded = FALSE;

static void
runner_seed (scr_uint seed)
{
  const uint32_t word = (uint32_t) seed;

  set_erkyrath_random (word ? word : 1);
  runner_trace = (getenv ("SCR_TRACE_RAND") != nullptr);
  runner_seeded = TRUE;
}

static scr_int
scr_runner_rand (scr_uint new_seed)
{
  uint32_t w;

  if (new_seed > 0)
    {
      runner_seed (new_seed);
      runner_draws = 0;
      return 0;
    }
  if (!runner_seeded)
    runner_seed (1234);

  w = erkyrath_random ();
  runner_draws++;
  if (runner_trace)
    fprintf (stderr, "RND #%u = %.7f (w=%08x)\n",
             runner_draws, (double) ((float) (w >> 8) / 16777216.0f),
             (unsigned int) w);
  return (scr_int) ((w >> 8) << 7);
}


/* Function pointer for the actual random number generator in use. */
static scr_int (*scr_rand_function) (scr_uint) = scr_platform_rand;


/*
 * scr_set_congruential_random()
 * scr_set_platform_random()
 * scr_set_runner_random()
 * scr_is_congruential_random()
 * scr_is_runner_random()
 *
 * Public interface to random functions: select the random handler in use,
 * and report which one is selected.  "Congruential" here means "portable and
 * predictable", which the Runner-compatible generator is too: the clock
 * freeze in scvars.cpp and the debugger's Random report key off this.
 */
void
scr_set_congruential_random (void)
{
  scr_rand_function = scr_congruential_rand;
}

void
scr_set_platform_random (void)
{
  scr_rand_function = scr_platform_rand;
}

void
scr_set_runner_random (void)
{
  scr_rand_function = scr_runner_rand;
}

scr_bool
scr_is_congruential_random (void)
{
  return scr_rand_function == scr_congruential_rand
         || scr_rand_function == scr_runner_rand;
}

scr_bool
scr_is_runner_random (void)
{
  return scr_rand_function == scr_runner_rand;
}


/*
 * scr_get_runner_random_state()
 * scr_set_runner_random_state()
 *
 * The Runner-compatible generator's exact position, for the Spatterlight
 * autosave, so that a session played under SCR_RNG=xoshiro does not restart
 * its stream from the seed at every relaunch.  The words are the shared
 * erkyrath_random() state; the draw count is only kept here.  The getter
 * returns FALSE (and stores nothing) unless that generator is the active
 * one; the setter selects it and places it, seeded, at `words`.
 */
scr_bool
scr_get_runner_random_state (scr_uint words[4], scr_uint *draws)
{
  int ix, native = 0, count = 0;
  uint32_t *table = nullptr;

  if (!scr_is_runner_random ())
    return FALSE;
  if (!runner_seeded)
    scr_runner_rand (1234);
  erkyrath_random_get_detstate (&native, &table, &count);
  if (!table || count != 4)
    return FALSE;
  for (ix = 0; ix < 4; ix++)
    words[ix] = table[ix];
  if (draws)
    *draws = runner_draws;
  return TRUE;
}

void
scr_set_runner_random_state (const scr_uint words[4], scr_uint draws)
{
  uint32_t table[4];
  int ix;

  scr_set_runner_random ();
  for (ix = 0; ix < 4; ix++)
    table[ix] = (uint32_t) words[ix];
  erkyrath_random_set_detstate (0, table, 4);
  runner_draws = draws;
  runner_trace = (getenv ("SCR_TRACE_RAND") != nullptr);
  runner_seeded = TRUE;
}


/*
 * scr_seed_random()
 * scr_rand()
 * scr_randomint()
 * scr_vb_rnd()
 * scr_randomint_exclusive()
 *
 * Reseed the random handler in use, draw the next number from it, and the
 * draws the engine makes within a range.
 */
void
scr_seed_random (scr_uint new_seed)
{
  /* Ignore zero values of new_seed by simply using 1 instead. */
  scr_rand_function (new_seed > 0 ? new_seed : 1);
}

static scr_int
scr_rand (void)
{
  /* Passing zero indicates this is not a seed operation. */
  return scr_rand_function (0);
}

scr_int
scr_randomint (scr_int low, scr_int high)
{
  const scr_int span = high - low + 1;

  /*
   * Map into the range with a multiply-shift on the full 31-bit value rather
   * than a modulo.  The low bits of the congruential generator have very
   * short periods (bit k of an LCG mod 2^32 cycles within 2^k draws), so
   * "% 2" of it alternates strictly and a per-turn draw cadence can pin a
   * small-range result to one value forever -- seen live as a Battle System
   * enemy that never switched targets.  Scaling from the top also matches
   * the Runner's VB6 Int(Rnd * N), which consumes the high part of Rnd.
   *
   * Both author-facing callers -- the "change variable to/by a random value"
   * task action (task_run_change_variable_action) and the `rand(x,y)`
   * expression function -- are literally `Int(Rnd * ((hi - lo) + 1)) + lo` in
   * the Runner (run400 48D1E0/48D261 in execute_action, 485C44 in the
   * expression evaluator), so a range the author entered BACKWARDS is not
   * rejected there: the span goes negative, Rnd is still drawn, and VB's Int()
   * floors towards minus infinity.  `rand(-3,-10)` therefore yields -9..-4 --
   * one inside each entered bound -- and NOT a constant -3.  Measured on
   * hyper_b_s.taf, whose whole scripted battle is two backwards ranges
   * (FLARERATHP += rand(-3,-10), HP += rand(-3,-15), five firings each):
   * run400 takes 8, 5, 9, 5, 7 off the rat and 13, 8, 8, 10, 8 off the
   * player, none of them the flat 3 that returning `low` produced -- which is
   * why the rat could never be killed.  With this, scarier reproduces that
   * fight exactly over all ten draws.  A span of -1 (`rand(-1,-3)`) makes
   * Int(Rnd * -1) = -1, so such an action is a constant -2, not -1.
   *
   * floor(-x) == -ceil(x), so the negative span is the same multiply-shift
   * rounded the other way.  A zero span (high == low - 1) draws and yields
   * low, as Int(Rnd * 0) does.
   */
  scr_int result;

  if (span > 0)
    {
      result = low + (scr_int) (((unsigned long long) scr_rand ()
                                 * (unsigned long long) span) >> 31);
    }
  else
    {
      result = low - (scr_int) (((unsigned long long) scr_rand ()
                                 * (unsigned long long) -span
                                 + 0x7fffffffULL) >> 31);
    }

  if (runner_trace)
    fprintf (stderr, "  randomint(%ld,%ld) = %ld\n",
             (long) low, (long) high, (long) result);
  return result;
}

double
scr_vb_rnd (void)
{
  /*
   * The next Single from the 3.9/3.8 codec's LCG (taf_runtime_rnd), traced
   * like the game-stream draws; the runner-compatible loader's source for
   * what run390 draws before it seeds with Timer.
   */
  const double value = taf_runtime_rnd ();

  if (runner_trace)
    fprintf (stderr, "  vbrnd(load) = %.7f\n", value);
  return value;
}

scr_int
scr_randomint_exclusive (scr_int low, scr_int high)
{
  /*
   * The Runners roll event lengths and delays as lo + Int(Rnd * (hi - lo)),
   * so the high bound is never drawn unless it equals the low one -- unlike
   * the author-facing rand(x,y) expression, which is inclusive.  Rnd is
   * consumed even when the range is degenerate, so draw unconditionally to
   * keep the stream cadence identical either way.
   */
  scr_int result;

  if (high <= low)
    {
      scr_rand ();
      result = low;
    }
  else
    {
      result = low + (scr_int) (((unsigned long long) scr_rand ()
                                 * (unsigned long long) (high - low)) >> 31);
    }

  if (runner_trace)
    fprintf (stderr, "  randomint_exclusive(%ld,%ld) = %ld\n",
             (long) low, (long) high, (long) result);
  return result;
}


/* Miscellaneous general ascii constants. */
static const scr_char SPACE = ' ';


/*
 * scr_strempty()
 *
 * Return TRUE if a string is either zero-length or contains only whitespace.
 */
scr_bool
scr_strempty (const scr_char *string)
{
  scr_int index_;
  assert (string);

  /* Scan for any non-space character. */
  for (index_ = 0; string[index_] != NUL; index_++)
    {
      if (!scr_isspace (string[index_]))
        return FALSE;
    }

  /* None found, so string is empty. */
  return TRUE;
}


/*
 * scr_trim_string()
 *
 * Trim leading and trailing whitespace from a string.  Modifies the string
 * in place, and returns the string address for convenience.
 */
scr_char *
scr_trim_string (scr_char *string)
{
  scr_int index_;
  assert (string);

  for (index_ = strlen (string) - 1;
       index_ >= 0 && scr_isspace (string[index_]); index_--)
    string[index_] = NUL;

  index_ = 0;
  while (scr_isspace (string[index_]))
    index_++;
  memmove (string, string + index_, strlen (string) - index_ + 1);

  return string;
}


/*
 * scr_normalize_string()
 *
 * Trim a string, and set all runs of whitespace to a single space character.
 * Modifies the string in place, and returns the string address for
 * convenience.
 */
scr_char *
scr_normalize_string (scr_char *string)
{
  scr_int index_;
  assert (string);

  /* Trim all leading and trailing spaces. */
  string = scr_trim_string (string);

  /* Compress multiple whitespace runs into a single space character. */
  for (index_ = 0; string[index_] != NUL; index_++)
    {
      if (scr_isspace (string[index_]))
        {
          scr_int cursor;

          string[index_] = SPACE;
          cursor = index_ + 1;
          while (scr_isspace (string[cursor]))
            cursor++;
          memmove (string + index_ + 1,
                   string + cursor, strlen (string + cursor) + 1);
        }
    }

  return string;
}


/*
 * scr_lowercase()
 *
 * Lower-case a string in place, character by character through
 * scr_tolower(), and return the string address for convenience.  This is
 * VB's LCase(): the locale's fold, the one the typed line gets, so a
 * caller that re-folds a line or a name must use it and never libc's
 * tolower(), which leaves the cp1251 letters above 0x7f alone.
 */
scr_char *
scr_lowercase (scr_char *string)
{
  scr_int index_;
  assert (string);

  for (index_ = 0; string[index_] != NUL; index_++)
    string[index_] = scr_tolower (string[index_]);

  return string;
}

std::string &
scr_lowercase (std::string &string)
{
  for (std::string::size_type index_ = 0; index_ < string.size (); index_++)
    string[index_] = scr_tolower (string[index_]);

  return string;
}


/*
 * scr_compare_word()
 *
 * Return TRUE if the first word in the string is word, case insensitive.
 */
scr_bool
scr_compare_word (const scr_char *string, const scr_char *word, scr_int length)
{
  assert (string && word);

  /* Return TRUE if string starts with word, then space or string end. */
  return scr_strncasecmp (string, word, length) == 0
         && (string[length] == NUL || scr_isspace (string[length]));
}


/*
 * scr_hash()
 *
 * Hash a string, hashpjw algorithm, from 'Compilers, principles, techniques,
 * and tools', page 436, unmodulo'ed and somewhat restyled.
 */
scr_uint
scr_hash (const scr_char *string)
{
  scr_int index_;
  scr_uint hash;
  assert (string);

  hash = 0;
  for (index_ = 0; string[index_] != NUL; index_++)
    {
      scr_uint temp;

      hash = (hash << 4) + string[index_];
      temp = hash & 0xf0000000;
      if (temp != 0)
        {
          hash = hash ^ (temp >> 24);
          hash = hash ^ temp;
        }
    }

  return hash;
}

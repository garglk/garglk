/* vi: set ts=8:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * Written from a reading of the ADRIFT 5 Runner source as released in
 * FrankenDrift, to reproduce its behaviour; no code from it is used.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * ADRIFT 5 support for Scarier -- randomness.  See a5rand.h.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <string>
#include <vector>

#include "a5rand.h"

/* The shared generator, terps/common_utils/randomness.c (vendored here in
   common_utils/ for out-of-tree builds). */
#include "common_utils/randomness.h"

static int a5rand_seeded = 0;

/* urand()'s no-repeat pools (the runner clsVariable.NoRepeatRandom /
   Adventure.dictRandValues): one shuffled value list per "min-max" range,
   consumed without repeats and rebuilt when exhausted.  Adventure-lifetime in
   the runner (not part of the save state, survives undo/restore in-session), so a
   plain static map cleared on a5rand_seed (== new game) mirrors it. */
static std::map<std::string, std::vector<long> > a5rand_pools;

void
a5rand_seed (unsigned int seed)
{
  set_erkyrath_random ((uint32_t) seed);
  a5rand_seeded = 1;
  a5rand_pools.clear ();
}

long a5rand_draw_count = 0;   /* diagnostic: draws since process start */

long
a5rand_between (long lo, long hi)
{
  unsigned long span;
  long t;
  if (hi == lo)            /* a fixed value draws no RNG (mirrors a from==to
                              FromTo, and keeps the RAND() task sequence stable
                              regardless of how many fixed-length events tick) */
    return lo;
  if (!a5rand_seeded)
    {
      /* Deterministic by default, like the other Scarier headless engines. */
      set_erkyrath_random (1234u);
      a5rand_seeded = 1;
    }
  if (hi < lo) { t = lo; lo = hi; hi = t; }
  /* Compute the span in the unsigned domain: hi - lo as signed long overflows
     (UB) for a range approaching the full long range from a corrupt game file.
     hi >= lo here, so the unsigned difference is the exact magnitude. */
  span = (unsigned long) hi - (unsigned long) lo;
  if (span == ~0UL)               /* full domain: the +1 below would wrap to 0 */
    return lo + (long) erkyrath_random ();
  span += 1UL;
  {
    long r = lo + (long) (erkyrath_random () % span);
    /* A5_TRACE_RAND=1: print every draw, for aligning the stream against an
       equally-instrumented FrankenDrift XoshiroRandom (FD_RNG_TRACE=1). */
    static int trace = -1;
    a5rand_draw_count++;
    if (trace < 0)
      trace = getenv ("A5_TRACE_RAND") != nullptr;
    if (trace)
      fprintf (stderr, "RAND(%ld,%ld)=%ld\n", lo, hi, r);
    return r;
  }
}

/* Step to the next thing that can open an integer: a digit or a minus sign. */
static const char *
a5rand_skip_to_int (const char *p)
{
  while (*p != '\0' && *p != '-' && !(*p >= '0' && *p <= '9'))
    p++;
  return p;
}

long
a5rand_between_args (const char *args)
{
  char *end;
  long lo, hi;

  lo = strtol (a5rand_skip_to_int (args), &end, 10);
  args = a5rand_skip_to_int (end);
  hi = (*args != '\0') ? strtol (args, nullptr, 10) : lo;
  return a5rand_between (lo, hi);
}

enum { A5RAND_POOL_LIMIT = 65536 };

long
a5rand_norepeat (long lo, long hi)
{
  /* The runner clsVariable.NoRepeatRandom, verbatim: the pool for "lo-hi" is (re)built
     empty->full by inserting each value at Random(count) -- a Fisher-Yates-like
     shuffle whose draws MUST hit the shared stream in the same order -- then one
     random element is picked and removed.  (Bounds are NOT swapped: the runner builds
     `For i = iMin To iMax`, which is empty when min > max and then picks from
     an empty list; no shipped game does that.) */
  char keybuf[48];
  /* A pool is one long per value and built with O(n^2) inserts, so a huge
     range (urand(1,100000000) in a corrupt or hostile game) would stall and
     exhaust memory; draw such a range with plain Random() instead.  The
     pool loop stops at hi rather than incrementing past it, which would overflow
     when hi == LONG_MAX. */
  if (hi > lo && (unsigned long) hi - (unsigned long) lo >= A5RAND_POOL_LIMIT)
    return a5rand_between (lo, hi);
  snprintf (keybuf, sizeof keybuf, "%ld-%ld", lo, hi);
  std::vector<long> &pool = a5rand_pools[keybuf];
  if (pool.empty ())
    for (long i = lo; i <= hi; i++)
      {
        long pos = a5rand_between (0, (long) pool.size ());
        pool.insert (pool.begin () + (size_t) pos, i);
        if (i == hi)
          break;
      }
  if (pool.empty ())
    return lo;
  long idx = a5rand_between (0, (long) pool.size () - 1);
  long val = pool[(size_t) idx];
  pool.erase (pool.begin () + (size_t) idx);
  return val;
}

void
a5rand_get_state (int *native, unsigned int state[4])
{
  uint32_t *arr = nullptr;
  int count = 0, n = 0, i;
  erkyrath_random_get_detstate (&n, &arr, &count);
  if (native != nullptr)
    *native = n;
  for (i = 0; i < 4; i++)
    state[i] = (arr != nullptr && i < count) ? (unsigned int) arr[i] : 0u;
}

void
a5rand_set_state (int native, const unsigned int state[4])
{
  uint32_t arr[4];
  int i;
  for (i = 0; i < 4; i++)
    arr[i] = (uint32_t) state[i];
  erkyrath_random_set_detstate (native, arr, 4);
  a5rand_seeded = 1;
}

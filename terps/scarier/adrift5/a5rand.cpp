/* vi: set ts=8:
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

/* The shared generator, defined in terps/common_utils/randomness.c.  Declared
   here (rather than including randomness.h) so the v5 engine does not pull in
   glk.h at this layer; glui32 is uint32_t. */
extern "C" {
  void   set_erkyrath_random (uint32_t seed);
  uint32_t erkyrath_random   (void);
  void erkyrath_random_get_detstate (int *usenative, uint32_t **arr, int *count);
  void erkyrath_random_set_detstate (int usenative, uint32_t *arr, int count);
}

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
      trace = getenv ("A5_TRACE_RAND") != NULL;
    if (trace)
      fprintf (stderr, "RAND(%ld,%ld)=%ld\n", lo, hi, r);
    return r;
  }
}

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
  snprintf (keybuf, sizeof keybuf, "%ld-%ld", lo, hi);
  std::vector<long> &pool = a5rand_pools[keybuf];
  if (pool.empty ())
    for (long i = lo; i <= hi; i++)
      {
        long pos = a5rand_between (0, (long) pool.size ());
        pool.insert (pool.begin () + (size_t) pos, i);
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
  uint32_t *arr = NULL;
  int count = 0, n = 0, i;
  erkyrath_random_get_detstate (&n, &arr, &count);
  if (native != NULL)
    *native = n;
  for (i = 0; i < 4; i++)
    state[i] = (arr != NULL && i < count) ? (unsigned int) arr[i] : 0u;
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

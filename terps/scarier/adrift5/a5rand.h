/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- randomness.
 *
 * v5 randomness is routed through the shared erkyrath_random()
 * (terps/common_utils/randomness.c), the same seedable, cross-platform
 * generator the other Scarier engines (and the v3.8/3.9/4.0 path in
 * scutils.cpp) use.  a5rand_seed(1234) selects the deterministic xoshiro128**
 * sequence used for reproducible walkthroughs/regression scripts; seed 0
 * selects the platform-native RNG.  The first draw lazily seeds 1234 if the
 * caller never seeded, so the headless harnesses are deterministic by default.
 */

#ifndef SCARIER_A5RAND_H
#define SCARIER_A5RAND_H

#ifdef __cplusplus
extern "C" {
#endif

/* (Re)seed the v5 RNG.  Call once per game so each run is reproducible. */
extern void a5rand_seed (unsigned int seed);

/* A random integer in the inclusive range [lo, hi], mirroring the Adrift 5 runner
   Global.Random(iMin, iMax) (which returns r.Next(iMin, iMax + 1)).  Swaps the
   bounds if hi < lo, exactly like the Adrift 5 runner. */
extern long a5rand_between (long lo, long hi);

/* urand(min,max): the runner clsVariable.NoRepeatRandom -- a per-"min-max" shuffled
   pool consumed without repeats (rebuilt when exhausted).  Pools reset on
   a5rand_seed (new game); they are NOT part of the save state, like the runner's
   Adventure.dictRandValues. */
extern long a5rand_norepeat (long lo, long hi);

/* Diagnostic: total draws since process start (A5_TRACE_TASK correlation). */
extern long a5rand_draw_count;

/* Save/restore the full generator state, so a saved game replays the same RNG
   sequence after restore (the v5 save format records it).  `state` holds the
   four xoshiro128** words; `native` is the platform-native-RNG flag.  Mirrors
   erkyrath_random_get_detstate / set_detstate (autorestore). */
extern void a5rand_get_state (int *native, unsigned int state[4]);
extern void a5rand_set_state (int native, const unsigned int state[4]);

#ifdef __cplusplus
}
#endif

#endif

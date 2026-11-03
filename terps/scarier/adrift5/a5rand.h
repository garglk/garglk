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

/* Draw for the bounds written after a function name: `args` points past the
   name of a "RAND (1, 4)" literal, and the bounds are the first two integers
   found there, whatever separates them.  A lone integer is both bounds, and
   so draws nothing. */
extern long a5rand_between_args (const char *args);

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

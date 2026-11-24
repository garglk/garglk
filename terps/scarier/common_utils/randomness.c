/*
 *  randomness.c
 *
 *  Copyright (c) 1999-2023, Andrew Plotkin
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *  The random number code of Glulxe's osdepend.c, without its Glk types and
 *  memory functions, so that other terps can use it. Glulxe keeps its own copy;
 *  autorestore depends on the get/set_detstate format of both.
 *  terps/scarier/common_utils has a byte-identical copy: after an edit, copy
 *  this file there and run `make -f Makefile.headless vendorcheck` in scarier.
 *
 *  Which native generator a build gets:
 *    __APPLE__                      arc4random (Spatterlight)
 *    OS_UNIX + UNIX_RAND_ARC4       arc4random
 *    OS_UNIX + UNIX_RAND_GETRANDOM  xoshiro128**, seeded from getrandom()
 *    OS_WINDOWS with Visual C++     rand_s
 *    anything else                  xoshiro128**, seeded from the clock
 *  Non-zero seeds always use xoshiro128**.
 */

#include "randomness.h"

/* osdepend.c: Glulxe platform-dependent code.
 Designed by Andrew Plotkin <erkyrath@eblong.com>
 http://eblong.com/zarf/glulx/index.html
 */

/* time() and NULL for the clock-seeded paths below */
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* This file contains definitions for platform-dependent code. Since
 Glk takes care of I/O, this is a short list -- memory allocation
 and random numbers.

 The Makefile (or whatever) should define OS_UNIX, or some other
 symbol. Code contributions welcome.
 */


/* We have a slightly baroque random-number scheme. If the Glulx
 @setrandom opcode is given seed 0, we use "true" randomness, from a
 platform native RNG if possible. If @setrandom is given a nonzero
 seed, we use a simple xoshiro128** RNG (provided below). The
 use of a known algorithm aids cross-platform testing and debugging.
 (Those being the cases where you'd set a nonzero seed.)

 To define a native RNG, define the macros RAND_SET_SEED() (seed the
 RNG with the clock or some other truly random source) and RAND_GET()
 (grab a number). Note that RAND_SET_SEED() does not take an argument;
 it is only called when seed=0. If RAND_GET() calls a non-seeded RNG
 API (such as arc4random()), then RAND_SET_SEED() should be a no-op.

 If RAND_SET_SEED/RAND_GET are not provided, we call back to the same
 xoshiro128** RNG as before, but seeded from the system clock.
 */

static uint32_t xo_random(void);
static void xo_seed_random(uint32_t seed);
static void xo_seed_random_4(uint32_t seed0, uint32_t seed1, uint32_t seed2, uint32_t seed3);

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef OS_STDC

/* Use our xoshiro128** as the native RNG, seeded from the clock. */
#define RAND_SET_SEED() (xo_seed_random(time(NULL)))
#define RAND_GET() (xo_random())

#endif /* OS_STDC */

#ifdef OS_UNIX

#include <stdlib.h>

#ifdef UNIX_RAND_ARC4

/* Use arc4random() as the native RNG. It doesn't need to be seeded. */
#define RAND_SET_SEED() (0)
#define RAND_GET() (arc4random())

#elif defined(UNIX_RAND_GETRANDOM)

/* Use xoshiro128** as the native RNG, seeded from getrandom(). */
#include <sys/random.h>

static void rand_set_seed(void)
{
    uint32_t seeds[4];
    int res = getrandom(seeds, 4*sizeof(uint32_t), 0);
    if (res < 0) {
        /* Error; fall back to the clock. */
        xo_seed_random(time(NULL));
    }
    else {
        xo_seed_random_4(seeds[0], seeds[1], seeds[2], seeds[3]);
    }
}

#define RAND_SET_SEED() (rand_set_seed())
#define RAND_GET() (xo_random())

#else /* UNIX_RAND_... */

/* Use our xoshiro128** as the native RNG, seeded from the clock. */
#define RAND_SET_SEED() (xo_seed_random(time(NULL)))
#define RAND_GET() (xo_random())

#endif /* UNIX_RAND_... */

#endif /* OS_UNIX */

#ifdef __APPLE__

#include <stdlib.h>

/* Use arc4random() as the native RNG. It doesn't need to be seeded. */
#define RAND_SET_SEED() (0)
#define RAND_GET() (arc4random())

#endif /* __APPLE__ */

#ifdef OS_WINDOWS

#ifdef _MSC_VER /* For Visual C++, get rand_s() */
#define _CRT_RAND_S
#endif
#include <stdlib.h>

#ifdef _MSC_VER /* Visual C++ */

/* Do nothing, as rand_s() has no seed. */
static void msc_srandom()
{
}

/* Use the Visual C++ function rand_s() as the native RNG.
 This calls the OS function RtlGetRandom(). */
static uint32_t msc_random()
{
    uint32_t value;
    rand_s(&value);
    return value;
}

#define RAND_SET_SEED() (msc_srandom())
#define RAND_GET() (msc_random())

#else /* Other Windows compilers */

/* Use our xoshiro128** as the native RNG, seeded from the clock. */
#define RAND_SET_SEED() (xo_seed_random(time(NULL)))
#define RAND_GET() (xo_random())

#endif

#endif /* OS_WINDOWS */


/* If no native RNG is defined above, use the xoshiro128** implementation. */
#ifndef RAND_SET_SEED
#define RAND_SET_SEED() (xo_seed_random(time(NULL)))
#define RAND_GET() (xo_random())
#endif /* RAND_SET_SEED */

static int rand_use_native = TRUE;

/* Set the random-number seed, and also select which RNG to use.
 */
void set_erkyrath_random(uint32_t seed)
{
    if (seed == 0) {
        rand_use_native = TRUE;
        RAND_SET_SEED();
    }
    else {
        rand_use_native = FALSE;
        xo_seed_random(seed);
    }
}

/* Return a random number in the range 0 to 2^32-1. */
uint32_t erkyrath_random(void)
{
    if (rand_use_native) {
        return RAND_GET();
    }
    else {
        return xo_random();
    }
}


/* This is the "xoshiro128**" random-number generator and seed function.
 Adapted from: https://prng.di.unimi.it/xoshiro128starstar.c
 About this algorithm: https://prng.di.unimi.it/
 */
static uint32_t xo_table[4] = { 0, 0, 0, 0 };

/* The get_detstate() and set_detstate() routines save and restore the
 entire RNG state. These are used only by autorestore. */
void erkyrath_random_get_detstate(int *usenative, uint32_t **arr, int *count)
{
    *usenative = rand_use_native;
    *arr = xo_table;
    *count = 4;
}

void erkyrath_random_set_detstate(int usenative, uint32_t *arr, int count)
{
    rand_use_native = usenative;
    if (arr != NULL && count == 4) {
        xo_seed_random_4(arr[0], arr[1], arr[2], arr[3]);
    }
}

static void xo_seed_random_4(uint32_t seed0, uint32_t seed1, uint32_t seed2, uint32_t seed3)
{
    /* Set up the 128-bit state from four integers. Use this if you can get
     four high-quality random values. */
    xo_table[0] = seed0;
    xo_table[1] = seed1;
    xo_table[2] = seed2;
    xo_table[3] = seed3;
}

static void xo_seed_random(uint32_t seed)
{
    int ix;
    /* Set up the 128-bit state from a single 32-bit integer. We rely
     on a different RNG, SplitMix32. This isn't high-quality, but we
     just need to get a bunch of bits into xo_table. */
    for (ix=0; ix<4; ix++) {
        seed += 0x9E3779B9;
        uint32_t s = seed;
        s ^= s >> 15;
        s *= 0x85EBCA6B;
        s ^= s >> 13;
        s *= 0xC2B2AE35;
        s ^= s >> 16;
        xo_table[ix] = s;
    }
}

static uint32_t xo_random(void)
{
    /* I've inlined the utility function:
     rotl(x, k) => (x << k) | (x >> (32 - k))
     */

    const uint32_t t1x5 = xo_table[1] * 5;
    const uint32_t result = ((t1x5 << 7) | (t1x5 >> (32-7))) * 9;

    const uint32_t t1s9 = xo_table[1] << 9;

    xo_table[2] ^= xo_table[0];
    xo_table[3] ^= xo_table[1];
    xo_table[1] ^= xo_table[2];
    xo_table[0] ^= xo_table[3];

    xo_table[2] ^= t1s9;

    const uint32_t t3 = xo_table[3];
    xo_table[3] =  ((t3 << 11) | (t3 >> (32-11)));

    return result;
}

// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "random.h"
#include "iff.h"
#include "io.h"
#include "process.h"
#include "stash.h"
#include "util.h"
#include "zterp.h"

// The random number generator can be in either a random or predictable
// state. The predictable state always uses a PRNG with a seed provided
// by the game. The random state uses either a PRNG or a random device
// file. If no device file is used, a seed comes either from the user or
// from the current time.
//
// In predictable mode, autosaves include the current state of the PRNG
// so autorestores can pick up right where they left off. In random
// mode, the state is not stored.
//
// In other saves (normal and meta), the state of the PRNG is never
// stored. Doing so portably would be impossible since different
// interpreters use different PRNGs, and intepreters may even be using
// non-deterministic RNGs, e.g. /dev/urandom on Linux. Even if a private
// chunk were used (such as that used for autosaves), Infocom never
// intended for seeds to be stored anyway: per the EZIP documentation:
// “The seed for RANDOM should not be saved or restored”.
//
// This implementation is actually at odds with Infocom’s EZIP
// documentation, which is at odds with the standard’s recommendation
// (or rather the other way around: the standard is at odds with
// Infocom). Infocom mandates that predictable mode generate a sequence
// of numbers from 1 to S, where S is the provided seed value. The
// standard recommends this but only for values of S less than 1000.
// Here a PRNG is always used, which is what ZVM also does on the
// recommendation of Andrew Plotkin:
// https://intfiction.org/t/z-machine-rng/7298
static enum mode {
    Random,
    Predictable,
} mode = Random;

// The PRNG used here is Xorshift32.
static uint32_t xstate;

static void zterp_srand(uint32_t s)
{
    if (s == 0) {
        s = 1;
    }

    xstate = s;
}

static FILE *random_fp;

static uint32_t zterp_rand(void)
{
    if (mode == Random && random_fp != NULL) {
        uint32_t value;

        if (fread(&value, sizeof value, 1, random_fp) == 1) {
            return value;
        } else {
            warning("error reading from %s: switching to PRNG", options.random_device);
            fclose(random_fp);
            random_fp = NULL;
        }
    }

    xstate ^= xstate << 13;
    xstate ^= xstate >> 17;
    xstate ^= xstate <<  5;

    return xstate;
}

// Called with 0, set the PRNG to random mode. Then seed it with either
// a) a user-provided seed (via -z) if available, or
// b) a seed derived from a hash of the constituent bytes of the value
//    returned by time(NULL)
//
// Otherwise, set the PRNG to predictable mode and seed with the
// provided value.
static void seed_random(uint32_t seed)
{
    if (seed == 0) {
        mode = Random;

        if (options.random_seed == -1) {
            time_t t = time(NULL);
            unsigned char *p = (unsigned char *)&t;
            uint32_t s = 0;

            // time_t hashing based on code by Lawrence Kirby.
            for (size_t i = 0; i < sizeof t; i++) {
                s = s * (UCHAR_MAX + 2U) + p[i];
            }

            zterp_srand(s);
        } else {
            zterp_srand(options.random_seed);
        }
    } else {
        mode = Predictable;

        zterp_srand(seed);
    }
}

enum RNGType {
    XORShift = 0,
};

TypeID random_write_rand(zterp_io *io, void *data)
{
    if (mode == Random) {
        return NULL;
    }

    zterp_io_write16(io, XORShift);
    zterp_io_write32(io, xstate);

    return &"Rand";
}

void random_read_rand(zterp_io *io)
{
    uint16_t rng_type;
    uint32_t state;

    if (zterp_io_read16(io, &rng_type) &&
        rng_type == XORShift &&
        zterp_io_read32(io, &state) &&
        state != 0) {

        xstate = state;
        mode = Predictable;
    }
}

static struct {
    enum mode mode;
    uint32_t xstate;
} stash;

static void random_stash_backup(void)
{
    stash.mode = mode;
    stash.xstate = xstate;
}

static bool random_stash_restore(void)
{
    mode = stash.mode;
    xstate = stash.xstate;

    return true;
}

static void random_stash_free(void)
{
}

void init_random(void)
{
    static bool tried_device = false;
    static bool first_run = true;

    seed_random(0);

    if (options.random_device != NULL && random_fp == NULL && !tried_device) {
        random_fp = fopen(options.random_device, "rb");
        if (random_fp == NULL) {
            warning("unable to open random device %s: %s\n", options.random_device, strerror(errno));
        }
        tried_device = true;
    }

    if (first_run) {
        stash_register(random_stash_backup, random_stash_restore, random_stash_free);

        first_run = false;
    }
}

void zrandom(void)
{
    int16_t v = as_signed(zargs[0]);

    if (v <= 0) {
        seed_random(-v);
        store(0);
    } else {
        store(zterp_rand() % zargs[0] + 1);
    }
}

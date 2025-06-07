// Copyright 2010-2025 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <fstream>
#include <memory>

#include "random.h"
#include "iff.h"
#include "io.h"
#include "options.h"
#include "process.h"
#include "stash.h"
#include "types.h"
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
// interpreters use different PRNGs, and interpreters may even be using
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
static enum class Mode {
    Random,
    Predictable,
} mode = Mode::Random;

class Xorshift32 {
public:
    void srand(uint32_t s) {
        if (s == 0) {
            s = 1;
        }

        m_state = s;
    }

    uint32_t rand() {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state <<  5;

        return m_state;
    }

    uint32_t state() const {
        return m_state;
    }

    void set_state(uint32_t state) {
        if (state != 0) {
            m_state = state;
        }
    }

private:
    uint32_t m_state = 1;
};

static Xorshift32 xorshift32;

static std::ifstream random_file;

static uint32_t zterp_rand()
{
    if (mode == Mode::Random && random_file.is_open()) {
        uint32_t value;

        if (random_file.read(reinterpret_cast<char *>(&value), sizeof value)) {
            return value;
        } else {
            warning("error reading from %s: switching to PRNG", options.random_device->c_str());
            random_file.close();
        }
    }

    return xorshift32.rand();
}

// Called with 0, set the PRNG to random mode. Then seed it with either
// a) a user-provided seed (via -z) if available, or
// b) a seed derived from a hash of the constituent bytes of the value
//    returned by time(nullptr)
//
// Otherwise, set the PRNG to predictable mode and seed with the
// provided value.
static void seed_random(uint32_t seed)
{
    if (seed == 0) {
        mode = Mode::Random;

        if (options.random_seed == nullptr) {
            auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            const unsigned char *p = reinterpret_cast<const unsigned char *>(&t);
            uint32_t s = 0;

            // time_t hashing based on code by Lawrence Kirby.
            for (size_t i = 0; i < sizeof t; i++) {
                s = s * (UCHAR_MAX + 2U) + p[i];
            }

            xorshift32.srand(s);
        } else {
            xorshift32.srand(*options.random_seed);
        }
    } else {
        mode = Mode::Predictable;

        xorshift32.srand(seed);
    }
}

enum class RNGType {
    Xorshift32 = 0,
};

IFF::TypeID random_write_rand(IO &io)
{
    if (mode == Mode::Random) {
        return IFF::TypeID();
    }

    io.write16(static_cast<uint16_t>(RNGType::Xorshift32));
    io.write32(xorshift32.state());

    return IFF::TypeID("Rand");
}

void random_read_rand(IO &io)
{
    uint16_t rng_type;
    uint32_t state;

    try {
        rng_type = io.read16();
        state = io.read32();
    } catch (const IO::IOError &) {
        return;
    }

    if (rng_type == static_cast<uint16_t>(RNGType::Xorshift32) && state != 0) {
        xorshift32.set_state(state);
        mode = Mode::Predictable;
    }
}

class RandomStasher : public Stasher {
public:
    void backup() override {
        m_mode = mode;
        m_xorshift32 = xorshift32;
    }

    bool restore() override {
        mode = m_mode;
        xorshift32 = m_xorshift32;

        return true;
    }

    void free() override {
    }

private:
    Mode m_mode = Mode::Random;
    Xorshift32 m_xorshift32;
};

void init_random(bool first_run)
{
    seed_random(0);

    if (first_run) {
        if (options.random_device != nullptr && !random_file.is_open()) {
            random_file.open(options.random_device->c_str(), std::ifstream::binary);
            if (!random_file.is_open()) {
                warning("unable to open random device %s: %s\n", options.random_device->c_str(), std::strerror(errno));
            }
        }

        stash_register(std::make_unique<RandomStasher>());
    }
}

void zrandom()
{
    int16_t v = as_signed(zargs[0]);

    if (v <= 0) {
        // Cast to long for the pathological case of -32768 and 16-bit
        // int: 32768 isn’t representable in an int, so -v is undefined.
        seed_random(-static_cast<long>(v));
        store(0);
    } else {
        store(zterp_rand() % zargs[0] + 1);
    }
}

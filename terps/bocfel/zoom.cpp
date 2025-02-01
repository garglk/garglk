// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <ctime>
#include <string>

#include "zoom.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

static std::clock_t start_clock, end_clock;

void zstart_timer()
{
    start_clock = std::clock();
}

void zstop_timer()
{
    end_clock = std::clock();
}

void zread_timer()
{
    store(100 * (end_clock - start_clock) / CLOCKS_PER_SEC);
}

void zprint_timer()
{
    std::string buf = fstring("%.2f seconds", (end_clock - start_clock) / static_cast<double>(CLOCKS_PER_SEC));
    for (const auto &c : buf) {
        put_char(c);
    }
}

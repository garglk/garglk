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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

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

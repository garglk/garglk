/*-
 * Copyright 2010-2012 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <time.h>

#include "zoom.h"
#include "screen.h"
#include "zterp.h"

static clock_t start_clock, end_clock;

void zstart_timer(void)
{
  start_clock = clock();
}

void zstop_timer(void)
{
  end_clock = clock();
}

void zread_timer(void)
{
  store(100 * (end_clock - start_clock) / CLOCKS_PER_SEC);
}

void zprint_timer(void)
{
  char buf[32];
  snprintf(buf, sizeof buf, "%.2f seconds", (end_clock - start_clock) / (double)CLOCKS_PER_SEC);
  for(int i = 0; buf[i] != 0; i++) put_char(buf[i]);
}

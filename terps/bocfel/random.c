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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

#include "random.h"
#include "process.h"
#include "zterp.h"

/* Mersenne Twister. */
static uint32_t mt[624];
static uint32_t mt_idx = 0;

static void zterp_srand(uint32_t s)
{
  mt[0] = s;
  for(int i = 1; i < 624; i++)
  {
    mt[i] = 1812433253UL * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i;
  }
  mt_idx = 0;
}

static void mt_gen_num(void)
{
  for(int i = 0; i < 624; i++)
  {
    uint32_t y;

    y = mt[i] & 0x80000000UL;
    y |= (mt[(i + 1) % 624]) & 0x7fffffffUL;

    mt[i] = mt[(i + 397) % 624] ^ (y >> 1);

    if(y % 2 == 1) mt[i] ^= 2567483615UL;
  }
}

static uint32_t zterp_rand(void)
{
  uint32_t y;

  if(mt_idx == 0) mt_gen_num();

  y = mt[mt_idx];
  y ^= (y >> 11);
  y ^= (y << 7) & 2636928640UL;
  y ^= (y << 15) & 4022730752UL;
  y ^= (y >> 18);

  mt_idx = (mt_idx + 1) % 624;

  return y;
}

static int rng_interval = 0;
static int rng_counter  = 0;

/* Called with 0, seed the PRNG with either
 * a) a user-provided seed (via -z) if available, or
 * b) a seed read from a user-provided file/device (via -Z) if
 *    available, or
 * c) a seed derived from a hash of the constituent bytes of the value
 *    returned by time(NULL)
 *
 * Called with a value 0 < S < 1000, generate a string of numbers 1, 2,
 * 3, ..., S, 1, 2, 3, ... S, ... as recommended in the remarks to §2.
 *
 * Called with a value >= 1000, use that value as a normal seed.
 */
void seed_random(long value)
{
  if(value == 0)
  {
    if(options.random_seed == -1)
    {
      time_t t = time(NULL);
      unsigned char *p = (unsigned char *)&t;
      uint32_t s = 0;

      /* time_t hashing based on code by Lawrence Kirby. */
      for(size_t i = 0; i < sizeof t; i++) s = s * (UCHAR_MAX + 2U) + p[i];

      if(options.random_device != NULL)
      {
        FILE *fp;
        uint32_t temp;

        fp = fopen(options.random_device, "r");
        if(fp != NULL)
        {
          if(fread(&temp, sizeof temp, 1, fp) == 1) s = temp;
          fclose(fp);
        }
      }

      zterp_srand(s);
    }
    else
    {
      zterp_srand(options.random_seed);
    }

    rng_interval = 0;
  }
  else if(value < 1000)
  {
    rng_counter = 0;
    rng_interval = value;
  }
  else
  {
    zterp_srand(value);
    rng_interval = 0;
  }
}

void zrandom(void)
{
  long v = (int16_t)zargs[0];

  if(v <= 0)
  {
    seed_random(-v);
    store(0);
  }
  else
  {
    uint32_t res;

    if(rng_interval != 0)
    {
      res = rng_counter++;
      if(rng_counter == rng_interval) rng_counter = 0;
    }
    else
    {
      res = zterp_rand();
    }

    store(res % zargs[0] + 1);
  }
}

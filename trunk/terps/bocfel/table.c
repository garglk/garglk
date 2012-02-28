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
#include <stdint.h>

#include "table.h"
#include "branch.h"
#include "memory.h"
#include "process.h"
#include "zterp.h"

void zcopy_table(void)
{
  uint16_t first = zargs[0], second = zargs[1], size = zargs[2];

  if(second == 0)
  {
    for(uint16_t i = 0; i < size; i++) user_store_byte(first + i, 0);
  }
  else if( (first > second) || (int16_t)size < 0 )
  {
    long n = labs((int16_t)size);
    for(long i = 0; i < n; i++) user_store_byte(second + i, user_byte(first + i));
  }
  else
  {
    for(uint16_t i = 0; i < size; i++) user_store_byte(second + size - i - 1, user_byte(first + size - i - 1));
  }
}

void zscan_table(void)
{
  uint16_t addr = zargs[1];

  if(znargs < 4) zargs[3] = 0x82;

  for(uint16_t i = 0; i < zargs[2]; i++)
  {
    if(
        ( (zargs[3] & 0x80) && (user_word(addr) == zargs[0])) ||
        (!(zargs[3] & 0x80) && (user_byte(addr) == zargs[0]))
      )
    {
      store(addr);
      branch_if(1);
      return;
    }

    addr += zargs[3] & 0x7f;
  }

  store(0);
  branch_if(0);
}

void zloadw(void)
{
  store(user_word(zargs[0] + (2 * zargs[1])));
}

void zloadb(void)
{
  store(user_byte(zargs[0] + zargs[1]));
}

void zstoreb(void)
{
  user_store_byte(zargs[0] + zargs[1], zargs[2]);
}

void zstorew(void)
{
  user_store_word(zargs[0] + (2 * zargs[1]), zargs[2]);
}

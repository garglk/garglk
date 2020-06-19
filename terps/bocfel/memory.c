/*-
 * Copyright 2010-2015 Chris Spiegel.
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
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "memory.h"
#include "branch.h"
#include "process.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

uint8_t *memory, *dynamic_memory;
uint32_t memory_size;

bool in_globals(uint16_t addr)
{
  return addr >= header.globals && addr < header.globals + 480;
}

bool is_global(uint16_t addr)
{
  return in_globals(addr) && (addr - header.globals) % 2 == 0;
}

static unsigned long addr_to_global(uint16_t addr)
{
  return (addr - header.globals) / 2;
}

const char *addrstring(uint16_t addr)
{
  static char str[8];

  if(is_global(addr)) snprintf(str, sizeof str, "G%02lx", addr_to_global(addr));
  else                snprintf(str, sizeof str, "0x%lx", (unsigned long)addr);

  return str;
}

void user_store_byte(uint16_t addr, uint8_t v)
{
  /* If safety checks are off, there’s no point in checking these
   * special cases.
   */
#ifndef ZTERP_NO_SAFETY_CHECKS
#ifdef ZTERP_TANDY
  if(addr == 0x01)
  {
    ZASSERT(v == byte(addr) || (byte(addr) ^ v) == 8, "not allowed to modify any bits but 3 at 0x0001");
  }
  else
#endif

  /* 0x10 can’t be modified, but let it slide if the story is storing
   * the same value that’s already there.  This is useful because the
   * flags at 0x10 are stored in a word, so the story possibly could use
   * @storew at 0x10 to modify the bits in 0x11.
   */
  if(addr == 0x10 && byte(addr) == v)
  {
    return;
  }
  else
#endif

  if(addr == 0x11)
  {
    ZASSERT((byte(addr) ^ v) < 8, "not allowed to modify bits 3-7 at 0x0011");

    if(!output_stream((v & FLAGS2_TRANSCRIPT) ? OSTREAM_SCRIPT : -OSTREAM_SCRIPT, 0)) v &= ~FLAGS2_TRANSCRIPT;

    header_fixed_font = v & FLAGS2_FIXED;
    set_current_style();
  }

  else
  {
    ZASSERT(addr >= 0x40 && addr < header.static_start, "attempt to write to read-only address 0x%lx", (unsigned long)addr);
  }

  store_byte(addr, v);
}

void user_store_word(uint16_t addr, uint16_t v)
{
#ifndef ZTERP_NO_WATCHPOINTS
  watch_check(addr, user_word(addr), v);
#endif

  user_store_byte(addr + 0, v >> 8);
  user_store_byte(addr + 1, v & 0xff);
}

void zcopy_table(void)
{
  uint16_t first = zargs[0], second = zargs[1], size = zargs[2];

  if(second == 0)
  {
    for(uint16_t i = 0; i < size; i++) user_store_byte(first + i, 0);
  }
  else if( (first > second) || as_signed(size) < 0 )
  {
    long n = labs(as_signed(size));
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
      branch_if(true);
      return;
    }

    addr += zargs[3] & 0x7f;
  }

  store(0);
  branch_if(false);
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

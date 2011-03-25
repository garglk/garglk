/*-
 * Copyright 2010-2011 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include "memory.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

uint8_t *memory, *dynamic_memory;
uint32_t memory_size;

void user_store_byte(uint16_t addr, uint8_t v)
{
  /* If safety checks are off, there’s no point in checking these
   * special cases. */
#ifndef ZTERP_NO_SAFETY_CHECKS
#ifdef ZTERP_TANDY
  if(addr == 0x01)
  {
    ZASSERT(v == BYTE(addr) || (BYTE(addr) ^ v) == 8, "not allowed to modify any bits but 3 at 0x0001");
  }
  else
#endif

  /* 0x10 can’t be modified, but let it slide if the story is storing
   * the same value that’s already there.  This is useful because the
   * flags at 0x10 are stored in a word, so the story possibly could use
   * @storew at 0x10 to modify the bits in 0x11.
   */
  if(addr == 0x10 && BYTE(addr) == v)
  {
    return;
  }
  else
#endif

  if(addr == 0x11)
  {
    ZASSERT((BYTE(addr) ^ v) < 8, "not allowed to modify bits 3-7 at 0x0011");

    output_stream((v & FLAGS2_TRANSCRIPT) ? OSTREAM_TRANS : -OSTREAM_TRANS, 0);

    header_fixed_font = v & FLAGS2_FIXED;
    set_current_style();
  }

  else
  {
    ZASSERT(addr >= 0x40 && addr < header.static_start, "attempt to write to read-only address 0x%lx", (unsigned long)addr);
  }

  STORE_BYTE(addr, v);
}

void user_store_word(uint16_t addr, uint16_t v)
{
  user_store_byte(addr + 0, v >> 8);
  user_store_byte(addr + 1, v & 0xff);
}

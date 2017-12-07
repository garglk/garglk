/*-
 * Copyright 2017 Chris Spiegel.
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

#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "patches.h"
#include "memory.h"
#include "zterp.h"

struct patch
{
  char *title;
  char serial[sizeof header.serial];
  uint16_t release;
  uint16_t checksum;
  uint32_t addr;
  size_t n;
  uint8_t *expected;
  uint8_t *replacement;
};

#define B(...)	(uint8_t[]){__VA_ARGS__}

static const struct patch patches[] =
{
  /* This is in a routine which iterates over all attributes of an
   * object, but due to an off-by-one error, attribute 48 (0x30) is
   * included, which is not valid, as the last attribute is 47 (0x2f);
   * there are 48 attributes but the count starts at 0.
   */
  {
    .title = "Sherlock", .serial = "871214", .release = 21, .checksum = 0x79b2,
    .addr = 0x223ac, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },
  {
    .title = "Sherlock", .serial = "880127", .release = 26, .checksum = 0x26ba,
    .addr = 0x22818, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },

  /* The operands to @get_prop here are swapped, so swap them back. */
  {
    .title = "Stationfall", .serial = "870430", .release = 107, .checksum = 0x2871,
    .addr = 0xe3fe, .n = 3, .expected = B(0x31, 0x0c, 0x77), .replacement = B(0x51, 0x77, 0x0c),
  },

  /* The Solid Gold (V5) version of Wishbringer calls @show_status, but
   * that is an illegal instruction outside of V3. Convert it to @nop.
   */
  {
    .title = "Wishbringer", .serial = "880706", .release = 23, .checksum = 0x4222,
    .addr = 0x1f910, .n = 1, .expected = B(0xbc), .replacement = B(0xb4),
  },

  { .replacement = NULL },
};

void apply_patches(void)
{
  for(const struct patch *patch = patches; patch->replacement != NULL; patch++)
  {
    if(memcmp(patch->serial, header.serial, sizeof header.serial) == 0 &&
       patch->release == header.release &&
       patch->checksum == header.checksum &&
       patch->addr >= header.static_start &&
       patch->addr + patch->n < memory_size &&
       memcmp(&memory[patch->addr], patch->expected, patch->n) == 0)
    {
      memcpy(&memory[patch->addr], patch->replacement, patch->n);
    }
  }
}

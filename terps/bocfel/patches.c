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
  /* Beyond Zork tries to treat a dictionary word as an object in four
   * places. This affects all releases and so needs to be patched in
   * four places each release, resulting in several patch entries.
   *
   * The code looks something like:
   *
   * [ KillFilm;
   *   @clear_attr circlet 3
   *   @call_vn ReplaceSyn "circlet" "film" "zzzp"
   *   @call_vn ReplaceAdj "circlet" "swirling" "zzzp"
   *   @rfalse;
   * ];
   *
   * For the calls, "circlet" is the dictionary word, not the object.
   * Given that the object number of the circlet object is easily
   * discoverable, these calls can be rewritten to use the object number
   * instead of the incorrect dictionary entry. The following set of
   * patches does this.
   */
  {
    .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
    .addr = 0x2f8e6, .n = 2, .expected = B(0xa3, 0x9a), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
    .addr = 0x2f8f0, .n = 2, .expected = B(0xa3, 0x9a), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
    .addr = 0x2f902, .n = 2, .expected = B(0xa3, 0x9a), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
    .addr = 0x2f90c, .n = 2, .expected = B(0xa3, 0x9a), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
    .addr = 0x2f8b6, .n = 2, .expected = B(0xa3, 0x9c), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
    .addr = 0x2f8c0, .n = 2, .expected = B(0xa3, 0x9c), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
    .addr = 0x2f8d2, .n = 2, .expected = B(0xa3, 0x9c), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
    .addr = 0x2f8dc, .n = 2, .expected = B(0xa3, 0x9c), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
    .addr = 0x2f762, .n = 2, .expected = B(0xa3, 0x8d), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
    .addr = 0x2f76c, .n = 2, .expected = B(0xa3, 0x8d), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
    .addr = 0x2f77e, .n = 2, .expected = B(0xa3, 0x8d), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
    .addr = 0x2f788, .n = 2, .expected = B(0xa3, 0x8d), .replacement = B(0x01, 0x86),
  },
  {
    .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
    .addr = 0x2fc72, .n = 2, .expected = B(0xa3, 0xba), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
    .addr = 0x2fc7c, .n = 2, .expected = B(0xa3, 0xba), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
    .addr = 0x2fc8e, .n = 2, .expected = B(0xa3, 0xba), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
    .addr = 0x2fc98, .n = 2, .expected = B(0xa3, 0xba), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
    .addr = 0x2fbfe, .n = 2, .expected = B(0xa3, 0xc0), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
    .addr = 0x2fc08, .n = 2, .expected = B(0xa3, 0xc0), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
    .addr = 0x2fc1a, .n = 2, .expected = B(0xa3, 0xc0), .replacement = B(0x01, 0x87),
  },
  {
    .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
    .addr = 0x2fc24, .n = 2, .expected = B(0xa3, 0xc0), .replacement = B(0x01, 0x87),
  },

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
    .title = "Sherlock", .serial = "880112", .release = 22, .checksum = 0xcb96,
    .addr = 0x225a4, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },
  {
    .title = "Sherlock", .serial = "880127", .release = 26, .checksum = 0x26ba,
    .addr = 0x22818, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },
  {
    .title = "Sherlock", .serial = "880324", .release = 4, .checksum = 0x7086,
    .addr = 0x22498, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },

  /* The operands to @get_prop here are swapped, so swap them back. */
  {
    .title = "Stationfall", .serial = "870326", .release = 87, .checksum = 0x71ae,
    .addr = 0xd3d4, .n = 3, .expected = B(0x31, 0x0c, 0x73), .replacement = B(0x51, 0x73, 0x0c),
  },
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

  /* Robot Finds Kitten attempts to sleep with the following:
   *
   * [ Func junk;
   *   @aread junk 0 10 PauseFunc -> junk;
   * ];
   *
   * However, since “junk” is a local variable with value 0 instead of a
   * text buffer, this is asking to read from/write to address 0. This
   * works in some interpreters, but Bocfel is more strict, and aborts
   * the program. Rewrite this instead to:
   *
   * @read_char 1 10 PauseFunc -> junk;
   * @nop; ! This is for padding.
   */
  {
    .title = "Robot Finds Kitten", .serial = "130320", .release = 7, .checksum = 0x4a18,
    .addr = 0x4912, .n = 8,
    .expected = B(0xe4, 0x94, 0x05, 0x00, 0x0a, 0x12, 0x5a, 0x05),
    .replacement = B(0xf6, 0x53, 0x01, 0x0a, 0x12, 0x5a, 0x05, 0xb4),
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

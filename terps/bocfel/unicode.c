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

#include <string.h>
#include <stdint.h>

#include "unicode.h"
#include "memory.h"
#include "util.h"
#include "zterp.h"

/*
 * The index is the ZSCII value, minus 155 (so entry 0 refers to ZSCII
 * value 155); and the value at the index is the Unicode character that
 * the ZSCII value maps to.  Because Latin-1 and Unicode are equivalent
 * for 0–255, this table maps to both Unicode and Latin1, with the
 * caveat that values greater than 255 should be considered invalid in
 * Latin-1, and are translated as a question mark below in
 * setup_tables() where appropriate.
 */
#define UNICODE_TABLE_SIZE	97
static int unicode_entries = 69;
static uint16_t unicode_table[UNICODE_TABLE_SIZE] = {
0x00e4, 0x00f6, 0x00fc, 0x00c4, 0x00d6, 0x00dc, 0x00df, 0x00bb, 0x00ab,
0x00eb, 0x00ef, 0x00ff, 0x00cb, 0x00cf, 0x00e1, 0x00e9, 0x00ed, 0x00f3,
0x00fa, 0x00fd, 0x00c1, 0x00c9, 0x00cd, 0x00d3, 0x00da, 0x00dd, 0x00e0,
0x00e8, 0x00ec, 0x00f2, 0x00f9, 0x00c0, 0x00c8, 0x00cc, 0x00d2, 0x00d9,
0x00e2, 0x00ea, 0x00ee, 0x00f4, 0x00fb, 0x00c2, 0x00ca, 0x00ce, 0x00d4,
0x00db, 0x00e5, 0x00c5, 0x00f8, 0x00d8, 0x00e3, 0x00f1, 0x00f5, 0x00c3,
0x00d1, 0x00d5, 0x00e6, 0x00c6, 0x00e7, 0x00c7, 0x00fe, 0x00f0, 0x00de,
0x00d0, 0x00a3, 0x0153, 0x0152, 0x00a1, 0x00bf,
};

/* If a non-default Unicode table is found, this function is called with
 * its address; it updates the Unicode table above.
 */
void parse_unicode_table(uint16_t utable)
{
  if(utable >= memory_size) die("corrupted story: unicode table out of range");

  unicode_entries = byte(utable++);

  if(unicode_entries > UNICODE_TABLE_SIZE) die("corrupted story: too many entries in the unicode table");
  if(utable + (2 * unicode_entries) > memory_size) die("corrupted story: unicode table out of range");

  for(int i = 0; i < unicode_entries; i++)
  {
    unicode_table[i] = word(utable + (2 * i));
  }
}

/* Table used to convert a ZSCII value to Unicode; and since this is
 * only used for output, non-output values will be returned as a
 * question mark.
 */
uint16_t zscii_to_unicode[UINT8_MAX + 1];

/* These tables translate a Unicode or (Latin-1) character into its
 * ZSCII equivalent.  Only valid Unicode characters are translated (that
 * is, those in the range 32–126, or 160 and above).  These are meant
 * for output, so do not translate delete and escape.
 *
 * The first table will translate invalid Unicode characters to zero;
 * the second, to a question mark.
 */
uint8_t unicode_to_zscii  [UINT16_MAX + 1];
uint8_t unicode_to_zscii_q[UINT16_MAX + 1];

/* Convenience table: pass through all values 0–255, but yield a question mark
 * for others. */
uint8_t unicode_to_latin1[UINT16_MAX + 1];

/* Convert ZSCII to Unicode line-drawing/rune characters. */
uint16_t zscii_to_font3[UINT8_MAX + 1];

/* Lookup table to see if a character is in the alphabet table.  Key is
 * the character, value is the index in the alphabet table, or -1.
 */
int atable_pos[UINT8_MAX + 1];

/* Not all fonts provide all characters, so there
 * may well be a lot of question marks.
 */
static void build_font3_table(void)
{
  for(int i = 0; i < UINT8_MAX; i++) zscii_to_font3[i] = UNICODE_REPLACEMENT;

  zscii_to_font3[ 32] = UNICODE_SPACE;
  zscii_to_font3[ 33] = 0x2190; /* ← */
  zscii_to_font3[ 34] = 0x2192; /* → */
  zscii_to_font3[ 35] = 0x2571; /* ╱ */
  zscii_to_font3[ 36] = 0x2572; /* ╲ */
  zscii_to_font3[ 37] = UNICODE_SPACE;
  zscii_to_font3[ 38] = 0x2500; /* ─ */
  zscii_to_font3[ 39] = 0x2500; /* ─ */
  zscii_to_font3[ 40] = 0x2502; /* │ */
  zscii_to_font3[ 41] = 0x2502; /* │ (this should be slightly offset, but whatever) */
  zscii_to_font3[ 42] = 0x2534; /* ┴ */
  zscii_to_font3[ 43] = 0x252c; /* ┬ */
  zscii_to_font3[ 44] = 0x251c; /* ├ */
  zscii_to_font3[ 45] = 0x2524; /* ┤ */
  zscii_to_font3[ 46] = 0x2514; /* └ */
  zscii_to_font3[ 47] = 0x250c; /* ┌ */
  zscii_to_font3[ 48] = 0x2510; /* ┐ */
  zscii_to_font3[ 49] = 0x2518; /* ┘ */

  /* There are a few characters that have no box-drawing equivalents.
   * These are the pieces that have connections sticking out of them,
   * used to link rooms together.  There are two options: have filled
   * boxes with no connections which makes the rooms look nice but the
   * connections look bad, or unfilled boxes with connections which
   * results in bad looking rooms but attached connections.  The end
   * result is something like this:
   *
   * No connections:         Connections:
   * ╲       ╱               ╲       ╱
   *  ┌─┐ ▗▄▖                 ╲─┐ ▗▄╱
   *  │ ├─▐█▌─                │ ├─┤█├─
   *  └─┘ ▝▀▘                 └─┘ ▝┬▘
   *       │                       │
   *
   * By default the former is done, but the latter can be chosen.
   */
  zscii_to_font3[ 50] = options.enable_alt_graphics ? 0x2571 : 0x2514; /* ╱ or └ */
  zscii_to_font3[ 51] = options.enable_alt_graphics ? 0x2572 : 0x250c; /* ╲ or ┌ */
  zscii_to_font3[ 52] = options.enable_alt_graphics ? 0x2571 : 0x2510; /* ╱ or ┐ */
  zscii_to_font3[ 53] = options.enable_alt_graphics ? 0x2572 : 0x2518; /* ╲ or ┘ */

  zscii_to_font3[ 54] = 0x2588; /* █ */
  zscii_to_font3[ 56] = 0x2584; /* ▄ */
  zscii_to_font3[ 55] = 0x2580; /* ▀ */
  zscii_to_font3[ 57] = 0x258c; /* ▌ */
  zscii_to_font3[ 58] = 0x2590; /* ▐ */

  zscii_to_font3[ 59] = options.enable_alt_graphics ? 0x2534 : 0x2584; /* ┴ or ▄ */
  zscii_to_font3[ 60] = options.enable_alt_graphics ? 0x252c : 0x2580; /* ┬ or ▀ */
  zscii_to_font3[ 61] = options.enable_alt_graphics ? 0x251c : 0x258c; /* ├ or ▌ */
  zscii_to_font3[ 62] = options.enable_alt_graphics ? 0x2524 : 0x2590; /* ┤ or ▐ */

  zscii_to_font3[ 63] = 0x259d; /* ▝ */
  zscii_to_font3[ 64] = 0x2597; /* ▗ */
  zscii_to_font3[ 65] = 0x2596; /* ▖ */
  zscii_to_font3[ 66] = 0x2598; /* ▘ */

  zscii_to_font3[ 67] = options.enable_alt_graphics ? 0x2571 : 0x259d; /* ╱ or ▝ */
  zscii_to_font3[ 68] = options.enable_alt_graphics ? 0x2572 : 0x2597; /* ╲ or ▗ */
  zscii_to_font3[ 69] = options.enable_alt_graphics ? 0x2571 : 0x2596; /* ╱ or ▖ */
  zscii_to_font3[ 70] = options.enable_alt_graphics ? 0x2572 : 0x2598; /* ╲ or ▘ */

  zscii_to_font3[ 75] = 0x2594; /* ▔ */
  zscii_to_font3[ 76] = 0x2581; /* ▁ */
  zscii_to_font3[ 77] = 0x258f; /* ▏ */
  zscii_to_font3[ 78] = 0x2595; /* ▕ */

  zscii_to_font3[ 79] = UNICODE_SPACE;
  zscii_to_font3[ 80] = 0x258f; /* ▏ */
  zscii_to_font3[ 81] = 0x258e; /* ▎ */
  zscii_to_font3[ 82] = 0x258d; /* ▍ */
  zscii_to_font3[ 83] = 0x258c; /* ▌ */
  zscii_to_font3[ 84] = 0x258b; /* ▋ */
  zscii_to_font3[ 85] = 0x258a; /* ▊ */
  zscii_to_font3[ 86] = 0x2589; /* ▉ */
  zscii_to_font3[ 87] = 0x2588; /* █ */
  zscii_to_font3[ 88] = 0x2595; /* ▕ */
  zscii_to_font3[ 89] = 0x258f; /* ▏ */

  zscii_to_font3[ 90] = 0x2573; /* ╳ */
  zscii_to_font3[ 91] = 0x253c; /* ┼ */
  zscii_to_font3[ 92] = 0x2191; /* ↑ */
  zscii_to_font3[ 93] = 0x2193; /* ↓ */
  zscii_to_font3[ 94] = 0x2195; /* ↕ */
  zscii_to_font3[ 95] = 0x2b1c; /* ⬜ */
  zscii_to_font3[ 96] = 0x003f; /* ? */
  zscii_to_font3[ 97] = 0x16aa; /* ᚪ */
  zscii_to_font3[ 98] = 0x16d2; /* ᛒ */
  zscii_to_font3[ 99] = 0x16c7; /* ᛇ */
  zscii_to_font3[100] = 0x16de; /* ᛞ */
  zscii_to_font3[101] = 0x16d6; /* ᛖ */
  zscii_to_font3[102] = 0x16a0; /* ᚠ */
  zscii_to_font3[103] = 0x16b7; /* ᚷ */
  zscii_to_font3[104] = 0x16bb; /* ᚻ */
  zscii_to_font3[105] = 0x16c1; /* ᛁ */
  zscii_to_font3[106] = 0x16e8; /* ᛨ */
  zscii_to_font3[107] = 0x16e6; /* ᛦ */
  zscii_to_font3[108] = 0x16da; /* ᛚ */
  zscii_to_font3[109] = 0x16d7; /* ᛗ */
  zscii_to_font3[110] = 0x16be; /* ᚾ */
  zscii_to_font3[111] = 0x16a9; /* ᚩ */
  zscii_to_font3[112] = 0x15be; /* ᖾ */
  zscii_to_font3[113] = 0x0068; /* Unicode 'h'; close to the rune. */
  zscii_to_font3[114] = 0x16b1; /* ᚱ */
  zscii_to_font3[115] = 0x16cb; /* ᛋ */
  zscii_to_font3[116] = 0x16cf; /* ᛏ */
  zscii_to_font3[117] = 0x16a2; /* ᚢ */
  zscii_to_font3[118] = 0x16e0; /* ᛠ */
  zscii_to_font3[119] = 0x16b9; /* ᚹ */
  zscii_to_font3[120] = 0x16c9; /* ᛉ */
  zscii_to_font3[121] = 0x16a5; /* ᚥ */
  zscii_to_font3[122] = 0x16df; /* ᛟ */

  /* These are reversed (see §16); a slightly ugly hack in screen.c is
   * used to accomplish this.
   */
  zscii_to_font3[123] = 0x2191; /* ↑ */
  zscii_to_font3[124] = 0x2193; /* ↓ */
  zscii_to_font3[125] = 0x2195; /* ↕ */
  zscii_to_font3[126] = 0x003f; /* ? */

  /* If the interpreter number is 9 (Apple IIc), Beyond Zork uses (at
   * least some) MouseText characters instead of the characters
   * specified in the standard for font 3.  The following were obtained
   * by looking at a disassembly of Beyond Zork; the list will be
   * revised if more non-standard character uses are discovered.
   *
   * As with the quirky behavior in the DOS version of Beyond Zork (see
   * process_story() in zterp.c), this is only active when the story
   * file is Beyond Zork, because as far as the standard is concerned,
   * this behavior is wrong, and font 3 should be identical for all
   * interpreter types.
   */
  if(options.int_number == 9 && is_beyond_zork())
  {
    zscii_to_font3[ 72] = 0x2190; /* ← */
    zscii_to_font3[ 74] = 0x2193; /* ↓ */
    zscii_to_font3[ 75] = 0x2191; /* ↑ */
    zscii_to_font3[ 76] = 0x2594; /* ▔ */
    zscii_to_font3[ 85] = 0x2192; /* → */
    zscii_to_font3[ 90] = 0x2595; /* ▕ */
    zscii_to_font3[ 95] = 0x258f; /* ▏ */
  }
}

void setup_tables(void)
{
  /*** ZSCII to Unicode table. ***/

  for(int i = 0; i < UINT8_MAX + 1; i++) zscii_to_unicode[i] = UNICODE_REPLACEMENT;
  zscii_to_unicode[0] = 0;
  zscii_to_unicode[ZSCII_NEWLINE] = UNICODE_LINEFEED;

  if(zversion == 6) zscii_to_unicode[ 9] = UNICODE_SPACE; /* Tab. */
  if(zversion == 6) zscii_to_unicode[11] = UNICODE_SPACE; /* Sentence space. */

  for(int i = 32; i < 127; i++) zscii_to_unicode[i] = i;
  for(int i = 0; i < unicode_entries; i++)
  {
    uint16_t c = unicode_table[i];

    if(!valid_unicode(c)) c = UNICODE_REPLACEMENT;

    zscii_to_unicode[i + 155] = c;
  }

  /*** Unicode to ZSCII tables. ***/

  /* Default values. */
  memset(unicode_to_zscii, 0, sizeof unicode_to_zscii);
  memset(unicode_to_zscii_q, ZSCII_QUESTIONMARK, sizeof unicode_to_zscii_q);

  /* First fill up the entries found in the Unicode table. */
  for(int i = 0; i < unicode_entries; i++)
  {
    uint16_t c = unicode_table[i];

    if(valid_unicode(c))
    {
      unicode_to_zscii  [c] = i + 155;
      unicode_to_zscii_q[c] = i + 155;
    }
  }

  /* Now the values that are equivalent in ZSCII and Unicode. */
  for(int i = 32; i < 127; i++)
  {
    unicode_to_zscii  [i] = i;
    unicode_to_zscii_q[i] = i;
  }

  /* Properly translate a newline. */
  unicode_to_zscii  [UNICODE_LINEFEED] = ZSCII_NEWLINE;
  unicode_to_zscii_q[UNICODE_LINEFEED] = ZSCII_NEWLINE;

  /*** Unicode to Latin1 table. ***/

  memset(unicode_to_latin1, LATIN1_QUESTIONMARK, sizeof unicode_to_latin1);
  for(int i = 0; i < 256; i++) unicode_to_latin1[i] = i;

  /*** ZSCII to character graphics table. ***/

  build_font3_table();

  /*** Alphabet table. ***/

  for(int i = 0; i < 256; i++) atable_pos[i] = -1;

  /* 52 is A2 character 6, which is special and should not
   * be matched, so skip over it.
   */
  for(int i = 0;  i < 52    ; i++) atable_pos[atable[i]] = i;
  for(int i = 53; i < 26 * 3; i++) atable_pos[atable[i]] = i;
}

/* This is adapted from Zip2000 (Copyright 2001 Kevin Bracey). */
uint16_t unicode_tolower(uint16_t c)
{
  static const unsigned char basic_latin[0x100] =
  {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xd7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
  };
  static const unsigned char latin_extended_a[0x80] =
  {
    0x01, 0x01, 0x03, 0x03, 0x05, 0x05, 0x07, 0x07, 0x09, 0x09, 0x0b, 0x0b, 0x0d, 0x0d, 0x0f, 0x0f,
    0x11, 0x11, 0x13, 0x13, 0x15, 0x15, 0x17, 0x17, 0x19, 0x19, 0x1b, 0x1b, 0x1d, 0x1d, 0x1f, 0x1f,
    0x21, 0x21, 0x23, 0x23, 0x25, 0x25, 0x27, 0x27, 0x29, 0x29, 0x2b, 0x2b, 0x2d, 0x2d, 0x2f, 0x2f,
    0x00, 0x31, 0x33, 0x33, 0x35, 0x35, 0x37, 0x37, 0x38, 0x3a, 0x3a, 0x3c, 0x3c, 0x3e, 0x3e, 0x40,
    0x40, 0x42, 0x42, 0x44, 0x44, 0x46, 0x46, 0x48, 0x48, 0x49, 0x4b, 0x4b, 0x4d, 0x4d, 0x4f, 0x4f,
    0x51, 0x51, 0x53, 0x53, 0x55, 0x55, 0x57, 0x57, 0x59, 0x59, 0x5b, 0x5b, 0x5d, 0x5d, 0x5f, 0x5f,
    0x61, 0x61, 0x63, 0x63, 0x65, 0x65, 0x67, 0x67, 0x69, 0x69, 0x6b, 0x6b, 0x6d, 0x6d, 0x6f, 0x6f,
    0x71, 0x71, 0x73, 0x73, 0x75, 0x75, 0x77, 0x77, 0x00, 0x7a, 0x7a, 0x7c, 0x7c, 0x7e, 0x7e, 0x7f
  };
  static const unsigned char greek[0x50] =
  {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0xac, 0x87, 0xad, 0xae, 0xaf, 0x8b, 0xcc, 0x8d, 0xcd, 0xce,
    0x90, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xa2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf
  };
  static const unsigned char cyrillic[0x60] =
  {
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
  };

  if     (c  < 0x0100)             c = basic_latin[c];
  else if(c == 0x0130)             c = 0x0069; /* capital i with dot -> lower case i */
  else if(c == 0x0178)             c = 0x00ff; /* capital y diaeresis -> lower case y diaeresis */
  else if(c <  0x0180)             c = latin_extended_a[c - 0x100] + 0x100;
  else if(c >= 0x380 && c < 0x3d0) c = greek           [c - 0x380] + 0x300;
  else if(c >= 0x400 && c < 0x460) c = cyrillic        [c - 0x400] + 0x400;

  return c;
}

/* Convert a char (assumed to be ASCII) to Unicode.  Printable values,
 * plus newline, are converted to their corresponding Unicode values;
 * the rest are converted to the Unicode replacement character (�).
 * This is a function rather than a table because it is rarely used, so
 * performance is not a concern.
 */
uint16_t char_to_unicode(char c)
{
  if(c == '\n')              return UNICODE_LINEFEED;
  else if(c < 32 || c > 126) return UNICODE_REPLACEMENT;
  else                       return c;
}

// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <array>

#ifdef ZTERP_ICU
#include <unicode/uchar.h>
#endif

#include "unicode.h"
#include "memory.h"
#include "options.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

// The index is the ZSCII value, minus 155 (so entry 0 refers to ZSCII
// value 155); and the value at the index is the Unicode character that
// the ZSCII value maps to. Because Latin-1 and Unicode are equivalent
// for 0–255, this table maps to both Unicode and Latin-1, with the
// caveat that values greater than 255 should be considered invalid in
// Latin-1, and are translated as a question mark below in
// setup_tables() where appropriate.
#define UNICODE_TABLE_SIZE	97
static int unicode_entries = 69;
static std::array<uint16_t, UNICODE_TABLE_SIZE> unicode_table = {
    0x00e4, 0x00f6, 0x00fc, 0x00c4, 0x00d6, 0x00dc, 0x00df, 0x00bb, 0x00ab,
    0x00eb, 0x00ef, 0x00ff, 0x00cb, 0x00cf, 0x00e1, 0x00e9, 0x00ed, 0x00f3,
    0x00fa, 0x00fd, 0x00c1, 0x00c9, 0x00cd, 0x00d3, 0x00da, 0x00dd, 0x00e0,
    0x00e8, 0x00ec, 0x00f2, 0x00f9, 0x00c0, 0x00c8, 0x00cc, 0x00d2, 0x00d9,
    0x00e2, 0x00ea, 0x00ee, 0x00f4, 0x00fb, 0x00c2, 0x00ca, 0x00ce, 0x00d4,
    0x00db, 0x00e5, 0x00c5, 0x00f8, 0x00d8, 0x00e3, 0x00f1, 0x00f5, 0x00c3,
    0x00d1, 0x00d5, 0x00e6, 0x00c6, 0x00e7, 0x00c7, 0x00fe, 0x00f0, 0x00de,
    0x00d0, 0x00a3, 0x0153, 0x0152, 0x00a1, 0x00bf,
};

// If a non-default Unicode table is found, this function is called with
// its address; it updates the Unicode table above.
void parse_unicode_table(uint16_t utable)
{
    if (utable >= memory_size) {
        die("corrupted story: unicode table out of range");
    }

    unicode_entries = byte(utable++);

    if (unicode_entries > UNICODE_TABLE_SIZE) {
        die("corrupted story: too many entries in the unicode table");
    }
    if (utable + (2 * unicode_entries) > memory_size) {
        die("corrupted story: unicode table out of range");
    }

    for (int i = 0; i < unicode_entries; i++) {
        unicode_table[i] = word(utable + (2 * i));
    }
}

// Table used to convert a ZSCII value to Unicode; and since this is
// only used for output, non-output values will be returned as the
// Unicode replacement character.
std::array<uint16_t, UINT8_MAX + 1> zscii_to_unicode;

// These tables translate a Unicode or Latin-1 character into its ZSCII
// equivalent. Only valid Unicode characters are translated (that is,
// those in the range 32–126, or 160 and above). These are meant for
// output, so do not translate delete and escape.
//
// The first table will translate invalid Unicode characters to zero;
// the second, to a question mark.
std::array<uint8_t, UINT16_MAX + 1> unicode_to_zscii;
std::array<uint8_t, UINT16_MAX + 1> unicode_to_zscii_q;

// Convenience table: pass through all values 0–255, but yield a question mark
// for others.
std::array<uint8_t, UINT16_MAX + 1> unicode_to_latin1;

// Convert ZSCII to Unicode line-drawing/rune characters.
std::array<uint16_t, UINT8_MAX + 1> zscii_to_font3;

// Lookup table to see if a character is in the alphabet table. Key is
// the character, value is the index in the alphabet table, or -1.
std::array<int, UINT8_MAX + 1> atable_pos;

static void build_zscii_to_unicode_table()
{
    zscii_to_unicode.fill(UNICODE_REPLACEMENT);
    zscii_to_unicode[0] = 0;
    zscii_to_unicode[ZSCII_NEWLINE] = UNICODE_LINEFEED;

    if (zversion == 6) {
        zscii_to_unicode[ 9] = UNICODE_SPACE; // Tab.
        zscii_to_unicode[11] = UNICODE_SPACE; // Sentence space.
    }

    for (int i = 32; i < 127; i++) {
        zscii_to_unicode[i] = i;
    }
    for (int i = 0; i < unicode_entries; i++) {
        uint16_t c = unicode_table[i];

        if (!valid_unicode(c)) {
            c = UNICODE_REPLACEMENT;
        }

        zscii_to_unicode[i + 155] = c;
    }
}

static void build_unicode_to_zscii_tables()
{
    // Default values.
    unicode_to_zscii.fill(0);
    unicode_to_zscii_q.fill(ZSCII_QUESTIONMARK);

    // First fill up the entries found in the Unicode table.
    for (int i = 0; i < unicode_entries; i++) {
        uint16_t c = unicode_table[i];

        if (valid_unicode(c)) {
            unicode_to_zscii  [c] = i + 155;
            unicode_to_zscii_q[c] = i + 155;
        }
    }

    // Now the values that are equivalent in ZSCII and Unicode.
    for (int i = 32; i < 127; i++) {
        unicode_to_zscii  [i] = i;
        unicode_to_zscii_q[i] = i;
    }

    // Properly translate a newline.
    unicode_to_zscii  [UNICODE_LINEFEED] = ZSCII_NEWLINE;
    unicode_to_zscii_q[UNICODE_LINEFEED] = ZSCII_NEWLINE;
}

static void build_unicode_to_latin1_table()
{
    unicode_to_latin1.fill(LATIN1_QUESTIONMARK);
    for (int i = 0; i < 256; i++) {
        unicode_to_latin1[i] = i;
    }
}

// Not all fonts provide all characters, so there
// may well be a lot of question marks.
static void build_zscii_to_character_graphics_table()
{
    zscii_to_font3.fill(UNICODE_REPLACEMENT);

    zscii_to_font3[ 32] = UNICODE_SPACE;
    zscii_to_font3[ 33] = 0x2190; // ←
    zscii_to_font3[ 34] = 0x2192; // →
    zscii_to_font3[ 35] = 0x2571; // ╱
    zscii_to_font3[ 36] = 0x2572; // ╲
    zscii_to_font3[ 37] = UNICODE_SPACE;
    zscii_to_font3[ 38] = 0x2500; // ─
    zscii_to_font3[ 39] = 0x2500; // ─
    zscii_to_font3[ 40] = 0x2502; // │
    zscii_to_font3[ 41] = 0x2502; // │ (this should be slightly offset, but whatever)
    zscii_to_font3[ 42] = 0x2534; // ┴
    zscii_to_font3[ 43] = 0x252c; // ┬
    zscii_to_font3[ 44] = 0x251c; // ├
    zscii_to_font3[ 45] = 0x2524; // ┤
    zscii_to_font3[ 46] = 0x2514; // └
    zscii_to_font3[ 47] = 0x250c; // ┌
    zscii_to_font3[ 48] = 0x2510; // ┐
    zscii_to_font3[ 49] = 0x2518; // ┘

    // There are a few characters that have no box-drawing equivalents.
    // These are the pieces that have connections sticking out of them,
    // used to link rooms together. There are two options: have filled
    // boxes with no connections which makes the rooms look nice but the
    // connections look bad, or unfilled boxes with connections which
    // results in bad looking rooms but attached connections. The end
    // result is something like this:
    //
    // No connections:         Connections:
    // ╲       ╱               ╲       ╱
    //  ┌─┐ ▗▄▖                 ╲─┐ ▗▄╱
    //  │ ├─▐█▌─                │ ├─┤█├─
    //  └─┘ ▝▀▘                 └─┘ ▝┬▘
    //       │                       │
    //
    // By default the former is done, but the latter can be chosen.
    zscii_to_font3[ 50] = options.enable_alt_graphics ? 0x2571 : 0x2514; // ╱ or └
    zscii_to_font3[ 51] = options.enable_alt_graphics ? 0x2572 : 0x250c; // ╲ or ┌
    zscii_to_font3[ 52] = options.enable_alt_graphics ? 0x2571 : 0x2510; // ╱ or ┐
    zscii_to_font3[ 53] = options.enable_alt_graphics ? 0x2572 : 0x2518; // ╲ or ┘

    zscii_to_font3[ 54] = 0x2588; // █
    zscii_to_font3[ 55] = 0x2580; // ▀
    zscii_to_font3[ 56] = 0x2584; // ▄
    zscii_to_font3[ 57] = 0x258c; // ▌
    zscii_to_font3[ 58] = 0x2590; // ▐

    zscii_to_font3[ 59] = options.enable_alt_graphics ? 0x2534 : 0x2584; // ┴ or ▄
    zscii_to_font3[ 60] = options.enable_alt_graphics ? 0x252c : 0x2580; // ┬ or ▀
    zscii_to_font3[ 61] = options.enable_alt_graphics ? 0x251c : 0x258c; // ├ or ▌
    zscii_to_font3[ 62] = options.enable_alt_graphics ? 0x2524 : 0x2590; // ┤ or ▐

    zscii_to_font3[ 63] = 0x259d; // ▝
    zscii_to_font3[ 64] = 0x2597; // ▗
    zscii_to_font3[ 65] = 0x2596; // ▖
    zscii_to_font3[ 66] = 0x2598; // ▘

    zscii_to_font3[ 67] = options.enable_alt_graphics ? 0x2571 : 0x259d; // ╱ or ▝
    zscii_to_font3[ 68] = options.enable_alt_graphics ? 0x2572 : 0x2597; // ╲ or ▗
    zscii_to_font3[ 69] = options.enable_alt_graphics ? 0x2571 : 0x2596; // ╱ or ▖
    zscii_to_font3[ 70] = options.enable_alt_graphics ? 0x2572 : 0x2598; // ╲ or ▘

    zscii_to_font3[ 75] = 0x2594; // ▔
    zscii_to_font3[ 76] = 0x2581; // ▁
    zscii_to_font3[ 77] = 0x258f; // ▏
    zscii_to_font3[ 78] = 0x2595; // ▕

    zscii_to_font3[ 79] = UNICODE_SPACE;
    zscii_to_font3[ 80] = 0x258f; // ▏
    zscii_to_font3[ 81] = 0x258e; // ▎
    zscii_to_font3[ 82] = 0x258d; // ▍
    zscii_to_font3[ 83] = 0x258c; // ▌
    zscii_to_font3[ 84] = 0x258b; // ▋
    zscii_to_font3[ 85] = 0x258a; // ▊
    zscii_to_font3[ 86] = 0x2589; // ▉
    zscii_to_font3[ 87] = 0x2588; // █
    zscii_to_font3[ 88] = 0x2595; // ▕
    zscii_to_font3[ 89] = 0x258f; // ▏

    zscii_to_font3[ 90] = 0x2573; // ╳
    zscii_to_font3[ 91] = 0x253c; // ┼
    zscii_to_font3[ 92] = 0x2191; // ↑
    zscii_to_font3[ 93] = 0x2193; // ↓
    zscii_to_font3[ 94] = 0x2195; // ↕
    zscii_to_font3[ 95] = 0x2395; // ⎕
    zscii_to_font3[ 96] = 0x003f; // ?
    zscii_to_font3[ 97] = 0x16aa; // ᚪ
    zscii_to_font3[ 98] = 0x16d2; // ᛒ
    zscii_to_font3[ 99] = 0x16c7; // ᛇ
    zscii_to_font3[100] = 0x16de; // ᛞ
    zscii_to_font3[101] = 0x16d6; // ᛖ
    zscii_to_font3[102] = 0x16a0; // ᚠ
    zscii_to_font3[103] = 0x16b7; // ᚷ
    zscii_to_font3[104] = 0x16bb; // ᚻ
    zscii_to_font3[105] = 0x16c1; // ᛁ
    zscii_to_font3[106] = 0x16c4; // ᛄ
    zscii_to_font3[107] = 0x16e6; // ᛦ
    zscii_to_font3[108] = 0x16da; // ᛚ
    zscii_to_font3[109] = 0x16d7; // ᛗ
    zscii_to_font3[110] = 0x16be; // ᚾ
    zscii_to_font3[111] = 0x16a9; // ᚩ
    zscii_to_font3[112] = 0x15be; // ᖾ
    zscii_to_font3[113] = 0x16b3; // ᚳ
    zscii_to_font3[114] = 0x16b1; // ᚱ
    zscii_to_font3[115] = 0x16cb; // ᛋ
    zscii_to_font3[116] = 0x16cf; // ᛏ
    zscii_to_font3[117] = 0x16a2; // ᚢ
    zscii_to_font3[118] = 0x16e0; // ᛠ
    zscii_to_font3[119] = 0x16b9; // ᚹ
    zscii_to_font3[120] = 0x16c9; // ᛉ
    zscii_to_font3[121] = 0x16a5; // ᚥ
    zscii_to_font3[122] = 0x16df; // ᛟ

    // These are reversed (see §16); a slightly ugly hack in screen.cpp
    // is used to accomplish this.
    zscii_to_font3[123] = 0x2191; // ↑
    zscii_to_font3[124] = 0x2193; // ↓
    zscii_to_font3[125] = 0x2195; // ↕
    zscii_to_font3[126] = 0x003f; // ?
}

static void build_alphabet_table()
{
    atable_pos.fill(-1);

    // 52 is A2 character 6, which is special and should not
    // be matched, so skip over it.
    for (int i = 0; i < 52; i++) {
        atable_pos[atable[i]] = i;
    }
    for (int i = 53; i < 26 * 3; i++) {
        atable_pos[atable[i]] = i;
    }
}

void setup_tables()
{
    build_zscii_to_unicode_table();
    build_unicode_to_zscii_tables();
    build_unicode_to_latin1_table();
    build_zscii_to_character_graphics_table();
    build_alphabet_table();
}

uint16_t unicode_tolower(uint16_t c)
{
#ifdef ZTERP_ICU
    uint32_t lower = u_tolower(c);

    if (lower > UINT16_MAX) {
        return UNICODE_REPLACEMENT;
    }

    return lower;
#else
    // This is adapted from Zip2000 (Copyright 2001 Kevin Bracey).
    static const std::array<unsigned char, 0x100> basic_latin = {
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
    static const std::array<unsigned char, 0x80> latin_extended_a = {
        0x01, 0x01, 0x03, 0x03, 0x05, 0x05, 0x07, 0x07, 0x09, 0x09, 0x0b, 0x0b, 0x0d, 0x0d, 0x0f, 0x0f,
        0x11, 0x11, 0x13, 0x13, 0x15, 0x15, 0x17, 0x17, 0x19, 0x19, 0x1b, 0x1b, 0x1d, 0x1d, 0x1f, 0x1f,
        0x21, 0x21, 0x23, 0x23, 0x25, 0x25, 0x27, 0x27, 0x29, 0x29, 0x2b, 0x2b, 0x2d, 0x2d, 0x2f, 0x2f,
        0x00, 0x31, 0x33, 0x33, 0x35, 0x35, 0x37, 0x37, 0x38, 0x3a, 0x3a, 0x3c, 0x3c, 0x3e, 0x3e, 0x40,
        0x40, 0x42, 0x42, 0x44, 0x44, 0x46, 0x46, 0x48, 0x48, 0x49, 0x4b, 0x4b, 0x4d, 0x4d, 0x4f, 0x4f,
        0x51, 0x51, 0x53, 0x53, 0x55, 0x55, 0x57, 0x57, 0x59, 0x59, 0x5b, 0x5b, 0x5d, 0x5d, 0x5f, 0x5f,
        0x61, 0x61, 0x63, 0x63, 0x65, 0x65, 0x67, 0x67, 0x69, 0x69, 0x6b, 0x6b, 0x6d, 0x6d, 0x6f, 0x6f,
        0x71, 0x71, 0x73, 0x73, 0x75, 0x75, 0x77, 0x77, 0x00, 0x7a, 0x7a, 0x7c, 0x7c, 0x7e, 0x7e, 0x7f
    };
    static const std::array<unsigned char, 0x50> greek = {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0xac, 0x87, 0xad, 0xae, 0xaf, 0x8b, 0xcc, 0x8d, 0xcd, 0xce,
        0x90, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
        0xc0, 0xc1, 0xa2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xac, 0xad, 0xae, 0xaf,
        0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf
    };
    static const std::array<unsigned char, 0x60> cyrillic = {
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
    };

    if (c < 0x0100) {
        c = basic_latin[c];
    } else if (c == 0x0130) {
        c = 0x0069; // capital i with dot -> lower case i
    } else if (c == 0x0178) {
        c = 0x00ff; // capital y diaeresis -> lower case y diaeresis
    } else if (c < 0x0180) {
        c = latin_extended_a[c - 0x100] + 0x100;
    } else if (c >= 0x380 && c < 0x3d0) {
        c = greek[c - 0x380] + 0x300;
    } else if (c >= 0x400 && c < 0x460) {
        c = cyrillic[c - 0x400] + 0x400;
    }

    return c;
#endif
}

#ifdef ZTERP_GLK
// Convert a char (assumed to be ASCII) to Unicode. Printable values,
// plus newline, are converted to their corresponding Unicode values;
// the rest are converted to the Unicode replacement character (�).
// This is a function rather than a table because it is rarely used, so
// performance is not a concern.
uint16_t char_to_unicode(char c)
{
    if (c == '\n') {
        return UNICODE_LINEFEED;
    } else if (c < 32 || c > 126) {
        return UNICODE_REPLACEMENT;
    } else {
        return c;
    }
}
#endif

// Standard 1.1 notes that Unicode characters 0–31 and 127–159
// are invalid due to the fact that they’re control codes.
bool valid_unicode(uint16_t c)
{
    return (c >= 32 && c <= 126) || c >= 160;
}

#ifdef ZTERP_OS_DOS
// Convert from Unicode to code page 437, used by DOS.
uint8_t unicode_to_437(uint16_t c)
{
    switch (c) {
    case 0x0020: return  32; case 0x0021: return  33; case 0x0022: return  34; case 0x0023: return  35;
    case 0x0024: return  36; case 0x0025: return  37; case 0x0026: return  38; case 0x0027: return  39;
    case 0x0028: return  40; case 0x0029: return  41; case 0x002A: return  42; case 0x002B: return  43;
    case 0x002C: return  44; case 0x002D: return  45; case 0x002E: return  46; case 0x002F: return  47;
    case 0x0030: return  48; case 0x0031: return  49; case 0x0032: return  50; case 0x0033: return  51;
    case 0x0034: return  52; case 0x0035: return  53; case 0x0036: return  54; case 0x0037: return  55;
    case 0x0038: return  56; case 0x0039: return  57; case 0x003A: return  58; case 0x003B: return  59;
    case 0x003C: return  60; case 0x003D: return  61; case 0x003E: return  62; case 0x003F: return  63;
    case 0x0040: return  64; case 0x0041: return  65; case 0x0042: return  66; case 0x0043: return  67;
    case 0x0044: return  68; case 0x0045: return  69; case 0x0046: return  70; case 0x0047: return  71;
    case 0x0048: return  72; case 0x0049: return  73; case 0x004A: return  74; case 0x004B: return  75;
    case 0x004C: return  76; case 0x004D: return  77; case 0x004E: return  78; case 0x004F: return  79;
    case 0x0050: return  80; case 0x0051: return  81; case 0x0052: return  82; case 0x0053: return  83;
    case 0x0054: return  84; case 0x0055: return  85; case 0x0056: return  86; case 0x0057: return  87;
    case 0x0058: return  88; case 0x0059: return  89; case 0x005A: return  90; case 0x005B: return  91;
    case 0x005C: return  92; case 0x005D: return  93; case 0x005E: return  94; case 0x005F: return  95;
    case 0x0060: return  96; case 0x0061: return  97; case 0x0062: return  98; case 0x0063: return  99;
    case 0x0064: return 100; case 0x0065: return 101; case 0x0066: return 102; case 0x0067: return 103;
    case 0x0068: return 104; case 0x0069: return 105; case 0x006A: return 106; case 0x006B: return 107;
    case 0x006C: return 108; case 0x006D: return 109; case 0x006E: return 110; case 0x006F: return 111;
    case 0x0070: return 112; case 0x0071: return 113; case 0x0072: return 114; case 0x0073: return 115;
    case 0x0074: return 116; case 0x0075: return 117; case 0x0076: return 118; case 0x0077: return 119;
    case 0x0078: return 120; case 0x0079: return 121; case 0x007A: return 122; case 0x007B: return 123;
    case 0x007C: return 124; case 0x007D: return 125; case 0x007E: return 126; case 0x00A1: return 173;
    case 0x00A2: return 155; case 0x00A3: return 156; case 0x00A5: return 157; case 0x00AA: return 166;
    case 0x00AB: return 174; case 0x00AC: return 170; case 0x00B0: return 248; case 0x00B1: return 241;
    case 0x00B2: return 253; case 0x00B5: return 230; case 0x00B7: return 250; case 0x00BA: return 167;
    case 0x00BB: return 175; case 0x00BC: return 172; case 0x00BD: return 171; case 0x00BF: return 168;
    case 0x00C4: return 142; case 0x00C5: return 143; case 0x00C6: return 146; case 0x00C7: return 128;
    case 0x00C9: return 144; case 0x00D1: return 165; case 0x00D6: return 153; case 0x00DC: return 154;
    case 0x00DF: return 225; case 0x00E0: return 133; case 0x00E1: return 160; case 0x00E2: return 131;
    case 0x00E4: return 132; case 0x00E5: return 134; case 0x00E6: return 145; case 0x00E7: return 135;
    case 0x00E8: return 138; case 0x00E9: return 130; case 0x00EA: return 136; case 0x00EB: return 137;
    case 0x00EC: return 141; case 0x00ED: return 161; case 0x00EE: return 140; case 0x00EF: return 139;
    case 0x00F1: return 164; case 0x00F2: return 149; case 0x00F3: return 162; case 0x00F4: return 147;
    case 0x00F6: return 148; case 0x00F7: return 246; case 0x00F9: return 151; case 0x00FA: return 163;
    case 0x00FB: return 150; case 0x00FC: return 129; case 0x00FF: return 152; case 0x0192: return 159;
    case 0x0393: return 226; case 0x0398: return 233; case 0x03A3: return 228; case 0x03A6: return 232;
    case 0x03A9: return 234; case 0x03B1: return 224; case 0x03B4: return 235; case 0x03B5: return 238;
    case 0x03C0: return 227; case 0x03C3: return 229; case 0x03C4: return 231; case 0x03C6: return 237;
    case 0x207F: return 252; case 0x20A7: return 158; case 0x2219: return 249; case 0x221A: return 251;
    case 0x221E: return 236; case 0x2229: return 239; case 0x2248: return 247; case 0x2261: return 240;
    case 0x2264: return 243; case 0x2265: return 242; case 0x2302: return 127; case 0x2310: return 169;
    case 0x2320: return 244; case 0x2321: return 245; case 0x2500: return 196; case 0x2502: return 179;
    case 0x250C: return 218; case 0x2510: return 191; case 0x2514: return 192; case 0x2518: return 217;
    case 0x251C: return 195; case 0x2524: return 180; case 0x252C: return 194; case 0x2534: return 193;
    case 0x253C: return 197; case 0x2550: return 205; case 0x2551: return 186; case 0x2552: return 213;
    case 0x2553: return 214; case 0x2554: return 201; case 0x2555: return 184; case 0x2556: return 183;
    case 0x2557: return 187; case 0x2558: return 212; case 0x2559: return 211; case 0x255A: return 200;
    case 0x255B: return 190; case 0x255C: return 189; case 0x255D: return 188; case 0x255E: return 198;
    case 0x255F: return 199; case 0x2560: return 204; case 0x2561: return 181; case 0x2562: return 182;
    case 0x2563: return 185; case 0x2564: return 209; case 0x2565: return 210; case 0x2566: return 203;
    case 0x2567: return 207; case 0x2568: return 208; case 0x2569: return 202; case 0x256A: return 216;
    case 0x256B: return 215; case 0x256C: return 206; case 0x2580: return 223; case 0x2584: return 220;
    case 0x2588: return 219; case 0x258C: return 221; case 0x2590: return 222; case 0x2591: return 176;
    case 0x2592: return 177; case 0x2593: return 178; case 0x25A0: return 254;

    case UNICODE_LINEFEED:
        return 10;

    // Question mark.
    default:
        return 63;
    }
}
#endif

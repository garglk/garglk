/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * os_glk_locale.cpp: the game locale -- the codepage tables and conversion
 * to Unicode, and the character/string output they drive.  Split out of
 * os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*---------------------------------------------------------------------*/
/*  Glk port locale data                                               */
/*---------------------------------------------------------------------*/

/*
 * Lookup table pair for converting a given single character into unicode and
 * iso 8859-1 (the lower byte of the unicode representation, assuming an upper
 * byte of zero), and an ascii substitute should nothing else be available.
 * Tables are 256 elements; although the first 128 characters of a codepage
 * are usually standard ascii, making tables full-sized allows for support of
 * codepages where they're not (dingbats, for example).
 */
enum { GSC_TABLE_SIZE = 256 };
typedef struct {
  const glui32 unicode[GSC_TABLE_SIZE];
  const scr_char *const ascii[GSC_TABLE_SIZE];
} gsc_codepages_t;

/*
 * Locale contains a name and a pair of codepage structures, a main one and
 * an alternate.  The latter is intended for monospaced output.
 */
typedef struct {
  const scr_char *const name;
  const gsc_codepages_t main;
  const gsc_codepages_t alternate;
} gsc_locale_t;


/*
 * Bytes 0x00 to 0x7f mean the same in all four codepages below: ASCII, with
 * the control characters dropped apart from tab and the line ends.  The
 * tables spell out only their upper halves, sixteen rows of eight as in a
 * codepage chart, after one of these.
 */
#define GSC_ASCII_TO_UNICODE \
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,            \
  0x0000, 0x0000, 0x000a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,            \
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,            \
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,            \
  0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,            \
  0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,            \
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,            \
  0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,            \
  0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,            \
  0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,            \
  0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,            \
  0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,            \
  0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,            \
  0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,            \
  0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,            \
  0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x0000

#define GSC_ASCII_TO_ASCII \
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    \
  nullptr, "        ","\n",    nullptr, nullptr, "\n",    nullptr, nullptr,  \
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    \
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,    \
  " ",     "!",     "\"",    "#",     "$",     "%",     "&",     "'",        \
  "(",     ")",     "*",     "+",     ",",     "-",     ".",     "/",        \
  "0",     "1",     "2",     "3",     "4",     "5",     "6",     "7",        \
  "8",     "9",     ":",     ";",     "<",     "=",     ">",     "?",        \
  "@",     "A",     "B",     "C",     "D",     "E",     "F",     "G",        \
  "H",     "I",     "J",     "K",     "L",     "M",     "N",     "O",        \
  "P",     "Q",     "R",     "S",     "T",     "U",     "V",     "W",        \
  "X",     "Y",     "Z",     "[",     "\\",    "]",     "^",     "_",        \
  "`",     "a",     "b",     "c",     "d",     "e",     "f",     "g",        \
  "h",     "i",     "j",     "k",     "l",     "m",     "n",     "o",        \
  "p",     "q",     "r",     "s",     "t",     "u",     "v",     "w",        \
  "x",     "y",     "z",     "{",     "|",     "}",     "~",     nullptr


/*
 * Locale for Latin1 -- cp1252 and cp850.
 *
 * The ascii representations of characters in this table are based on the
 * general look of the characters, rather than pronounciation.  Accented
 * characters are generally rendered unaccented, and box drawing, shading,
 * and other non-alphanumeric glyphs as either a similar shape, or as a
 * character that might be recognizable as what it's trying to emulate.
 */
static const gsc_locale_t GSC_LATIN1_LOCALE = {
  "Latin1",
  /* cp1252 to unicode. */
{ { GSC_ASCII_TO_UNICODE,
    0x20ac, 0x0000, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
    0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017d, 0x0000,
    0x0000, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x0000, 0x017e, 0x0178,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff },
  /* cp1252 to ascii. */
  { GSC_ASCII_TO_ASCII,
    "E",     nullptr, ",",     "f",     ",,",    "...",   "+",     "#",
    "^",     "%",     "S",     "<",     "OE",    nullptr, "Z",     nullptr,
    nullptr, "'",     "'",     "\"",    "\"",    "*",     "-",     "-",
    "~",     "[TM]",  "s",     ">",     "oe",    nullptr, "z",     "Y",
    " ",     "!",     "c",     "GBP",   "*",     "Y",     "|",     "S",
    "\"",    "(C)",   "a",     "<<",    "-",     "-",     "(R)",   "-",
    "o",     "+/-",   "2",     "3",     "'",     "u",     "P",     "*",
    ",",     "1",     "o",     ">>",    "1/4",   "1/2",   "3/4",   "?",
    "A",     "A",     "A",     "A",     "A",     "A",     "AE",    "C",
    "E",     "E",     "E",     "E",     "I",     "I",     "I",     "I",
    "D",     "N",     "O",     "O",     "O",     "O",     "O",     "x",
    "O",     "U",     "U",     "U",     "U",     "Y",     "p",     "ss",
    "a",     "a",     "a",     "a",     "a",     "a",     "ae",    "c",
    "e",     "e",     "e",     "e",     "i",     "i",     "i",     "i",
    "d",     "n",     "o",     "o",     "o",     "o",     "o",     "/",
    "o",     "u",     "u",     "u",     "u",     "y",     "P",     "y" } },
  /* cp850 to unicode. */
{ { GSC_ASCII_TO_UNICODE,
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00f8, 0x00a3, 0x00d8, 0x00d7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x00ae, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00c1, 0x00c2, 0x00c0,
    0x00a9, 0x2563, 0x2551, 0x2557, 0x255d, 0x00a2, 0x00a5, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x00e3, 0x00c3,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x00a4,
    0x00f0, 0x00d0, 0x00ca, 0x00cb, 0x00c8, 0x0131, 0x00cd, 0x00ce,
    0x00cf, 0x2518, 0x250c, 0x2588, 0x2584, 0x00a6, 0x00cc, 0x2580,
    0x00d3, 0x00df, 0x00d4, 0x00d2, 0x00f5, 0x00d5, 0x00b5, 0x00fe,
    0x00de, 0x00da, 0x00db, 0x00d9, 0x00fd, 0x00dd, 0x00af, 0x00b4,
    0x00ad, 0x00b1, 0x2017, 0x00be, 0x00b6, 0x00a7, 0x00f7, 0x00b8,
    0x00b0, 0x00a8, 0x00b7, 0x00b9, 0x00b3, 0x00b2, 0x25a0, 0x00a0 },
  /* cp850 to ascii. */
  { GSC_ASCII_TO_ASCII,
    "C",     "u",     "e",     "a",     "a",     "a",     "a",     "c",
    "e",     "e",     "e",     "i",     "i",     "i",     "A",     "A",
    "E",     "ae",    "AE",    "o",     "o",     "o",     "u",     "u",
    "y",     "O",     "U",     "o",     "GBP",   "O",     "x",     "f",
    "a",     "i",     "o",     "u",     "n",     "N",     "a",     "o",
    "?",     "(R)",   "-",     "1/2",   "1/4",   "i",     "<<",    ">>",
    "#",     "#",     "#",     "|",     "+",     "A",     "A",     "A",
    "(C)",   "+",     "|",     "+",     "+",     "c",     "Y",     "+",
    "+",     "+",     "+",     "+",     "-",     "+",     "a",     "A",
    "+",     "+",     "+",     "+",     "+",     "=",     "+",     "*",
    "d",     "D",     "E",     "E",     "E",     "i",     "I",     "I",
    "I",     "+",     "+",     ".",     ".",     "|",     "I",     ".",
    "O",     "ss",    "O",     "O",     "o",     "O",     "u",     "p",
    "P",     "U",     "U",     "U",     "y",     "Y",     "-",     "'",
    "-",     "+/-",   "=",     "3/4",   "P",     "S",     "/",     ",",
    "deg",   "\"",    "*",     "1",     "3",     "2",     ".",     " " } }
};


/*
 * Locale for Cyrillic -- cp1251 and cp866.
 *
 * The ascii representations in this table, for alphabetic characters, follow
 * linguistic rather than appearance rules, the essence of gost 16876-71.
 * Capitalized cyrillic letters that translate to multiple ascii characters
 * have the first ascii character only of the sequence translated.  This gives
 * the best appearance in normal sentences, but is not optimal in a run of
 * all capitals (headings, for example).  For non-alphanumeric characters,
 * the general appearance and shape of the character being emulated is used.
 */
static const gsc_locale_t GSC_CYRILLIC_LOCALE = {
  "Cyrillic",
  /* cp1251 to unicode. */
{ { GSC_ASCII_TO_UNICODE,
    0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020, 0x2021,
    0x20ac, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
    0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0x0000, 0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f,
    0x00a0, 0x040e, 0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7,
    0x0401, 0x00a9, 0x0404, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407,
    0x00b0, 0x00b1, 0x0406, 0x0456, 0x0491, 0x00b5, 0x00b6, 0x00b7,
    0x0451, 0x2116, 0x0454, 0x00bb, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f },
  /* cp1251 to gost 16876-71 ascii. */
  { GSC_ASCII_TO_ASCII,
    nullptr, nullptr, ",",     nullptr, ",,",    "...",   "+",     "#",
    "E",     "%",     nullptr, "<",     nullptr, nullptr, nullptr, nullptr,
    nullptr, "'",     "'",     "\"",    "\"",    "*",     "-",     "-",
    nullptr, "[TM]",  nullptr, ">",     nullptr, nullptr, nullptr, nullptr,
    " ",     nullptr, nullptr, nullptr, "*",     "G",     "|",     "S",
    "Jo",    "(C)",   "Je",    "<<",    "-",     "-",     "(R)",   "Ji",
    "o",     "+/-",   "I",     "i",     "g",     "u",     "P",     "*",
    "jo",    nullptr, "je",    ">>",    "j",     "S",     "s",     "ji",
    "A",     "B",     "V",     "G",     "D",     "E",     "Zh",    "Z",
    "I",     "Jj",    "K",     "L",     "M",     "N",     "O",     "P",
    "R",     "S",     "T",     "U",     "F",     "Kh",    "C",     "Ch",
    "Sh",    "Shh",   "\"",    "Y",     "'",     "Eh",    "Ju",    "Ja",
    "a",     "b",     "v",     "g",     "d",     "e",     "zh",    "z",
    "i",     "jj",    "k",     "l",     "m",     "n",     "o",     "p",
    "r",     "s",     "t",     "u",     "f",     "kh",    "c",     "ch",
    "sh",    "shh",   "\"",    "y",     "'",     "eh",    "ju",    "ja" } },
  /* cp866 to unicode. */
{ { GSC_ASCII_TO_UNICODE,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f,
    0x0401, 0x0451, 0x0404, 0x0454, 0x0407, 0x0457, 0x040e, 0x045e,
    0x00b0, 0x2022, 0x00b7, 0x221a, 0x2116, 0x00a4, 0x25a0, 0x00a0 },
  /* cp866 to gost 16876-71 ascii. */
  { GSC_ASCII_TO_ASCII,
    "A",     "B",     "V",     "G",     "D",     "E",     "Zh",    "Z",
    "I",     "Jj",    "K",     "L",     "M",     "N",     "O",     "P",
    "R",     "S",     "T",     "U",     "F",     "Kh",    "C",     "Ch",
    "Sh",    "Shh",   "\"",    "Y",     "'",     "Eh",    "Ju",    "Ja",
    "a",     "b",     "v",     "g",     "d",     "e",     "zh",    "z",
    "i",     "jj",    "k",     "l",     "m",     "n",     "o",     "p",
    "#",     "#",     "#",     "|",     "+",     "+",     "+",     "+",
    "+",     "+",     "|",     "+",     "+",     "+",     "+",     "+",
    "+",     "+",     "+",     "+",     "-",     "+",     "+",     "+",
    "+",     "+",     "+",     "+",     "+",     "|",     "+",     "+",
    "+",     "+",     "+",     "+",     "+",     "+",     "+",     "+",
    "+",     "+",     "+",     "+",     ".",     ".",     ".",     ".",
    "r",     "s",     "t",     "u",     "f",     "kh",    "c",     "ch",
    "sh",    "shh",   "\"",    "y",     "'",     "eh",    "ju",    "ja",
    "Jo",    "jo",    "Je",    "je",    "Ji",    "ji",    nullptr, nullptr,
    "deg",   "*",     "*",     nullptr, nullptr, "*",     ".",     " " } }
};


/*---------------------------------------------------------------------*/
/*  Glk port locale control and conversion functions                   */
/*---------------------------------------------------------------------*/

/* Both Glk libraries this is built against (cheapglk, glkimp) define
   GLK_MODULE_UNICODE, so the port's unicode paths are always present; the flag
   stays so the runtime "-nu" switch and gestalt check still have a meaning. */
const scr_bool gsc_has_unicode = TRUE;

/*
 * Known valid character printing range.  Some Glk libraries aren't accurate
 * about what will and what won't print when queried with glk_gestalt(), so
 * we also make an explicit range check against guaranteed to print chars.
 */
static const glui32 GSC_MIN_PRINTABLE = ' ',
                    GSC_MAX_PRINTABLE = '~';


/* List of pointers to supported and available locales, NULL terminated. */
static const gsc_locale_t *const GSC_AVAILABLE_LOCALES[] = {
  &GSC_LATIN1_LOCALE,
  &GSC_CYRILLIC_LOCALE,
  nullptr
};

/*
 * The locale for the game, set below explicitly or on game startup, and
 * a fallback locale to use in case none has been set.
 */
static const gsc_locale_t *gsc_locale = nullptr;
static const gsc_locale_t *const gsc_fallback_locale = &GSC_LATIN1_LOCALE;

/*
 * gsc_current_locale()
 *
 * The locale to read and write with: the game's, or the fallback.
 */
static const gsc_locale_t *
gsc_current_locale (void)
{
  return gsc_locale ? gsc_locale : gsc_fallback_locale;
}


/*
 * gsc_set_locale()
 *
 * Set a locale explicitly from the name passed in.
 */
void
gsc_set_locale (const scr_char *name)
{
  const gsc_locale_t *matched = nullptr;
  const gsc_locale_t *const *iterator;
  assert (name);

  /*
   * Search locales for a matching name, abbreviated if necessary.  Stop on
   * the first match found.
   */
  for (iterator = GSC_AVAILABLE_LOCALES; *iterator; iterator++)
    {
      const gsc_locale_t *const locale = *iterator;

      if (scr_strncasecmp (name, locale->name, strlen (name)) == 0)
        {
          matched = locale;
          break;
        }
    }

  /* If matched, set the global locale. */
  if (matched)
    gsc_locale = matched;
}


/*
 * Tracks whether the next character written to the main window would start a
 * new line, i.e. the last thing printed there was a newline.  Used by
 * os_show_graphic() to avoid emitting a redundant leading break (and a blank
 * line) when an image is already at the start of a line.
 */
scr_bool gsc_main_at_line_start = TRUE;

/*
 * gsc_put_char_locale()
 *
 * Write a single character using the supplied locale.  Select either the
 * main or the alternate codepage depending on the flag passed in.
 */
static void
gsc_put_char_locale (scr_char ch,
                     const gsc_locale_t *locale, scr_bool is_alternate)
{
  const gsc_codepages_t *codepage;

  /*
   * Track whether the next main-window character would start a new line, but
   * only when output is actually targeting the main window.  Status-window
   * rendering also routes through here, and must not clobber the flag that
   * os_show_graphic() relies on to decide whether to emit a leading break.
   */
  if (gsc_main_window
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    {
      gsc_main_at_line_start = (ch == '\n');
      gsc_main_window_empty = FALSE;
    }
  unsigned char character;
  glui32 unicode;
  const char *ascii;

  /*
   * Select either the main or the alternate codepage for this locale, and
   * retrieve the unicode and ascii representations of the character.
   */
  codepage = is_alternate ? &locale->alternate : &locale->main;
  character = (unsigned char) ch;
  unicode = codepage->unicode[character];
  ascii = codepage->ascii[character];

  /*
   * If a unicode representation exists, use for either iso 8859-1 or, if
   * possible, direct unicode output.
   */
  if (unicode > 0)
    {
      /*
       * If unicode is in the range 1-255, this value is directly equivalent
       * to the iso 8859-1 representation; otherwise the character has no
       * direct iso 8859-1 glyph.
       */
      if (unicode < GSC_ISO_8859_EQUIVALENCE)
        {
          /*
           * If the iso 8859-1 character is one that this Glk library will
           * print exactly, print and return.  We add a check here for the
           * guaranteed printable characters, since some Glk libraries don't
           * return the correct values for gestalt_CharOutput for these.
           */
          if (unicode == '\n'
              || (unicode >= GSC_MIN_PRINTABLE && unicode <= GSC_MAX_PRINTABLE)
              || glk_gestalt (gestalt_CharOutput,
                              unicode) == gestalt_CharOutput_ExactPrint)
            {
              glk_put_char ((unsigned char) unicode);
              return;
            }
        }

      /*
       * If no usable iso 8859-1 representation, see if unicode is enabled and
       * if the Glk library can print the character exactly.  If yes, output
       * the character that way.  The transcript is a unicode stream
       * (gsc_open_log_stream), so the echo writes it too.
       */
      if (gsc_unicode_enabled)
        {
          if (glk_gestalt (gestalt_CharOutput,
                           unicode) == gestalt_CharOutput_ExactPrint)
            {
              glk_put_char_uni (unicode);
              return;
            }
        }
    }

  /*
   * No success with iso 8859-1 or unicode, so try for an ascii substitute.
   * Substitute strings use only 7-bit ascii, and so all are safe to print
   * directly with Glk.
   */
  if (ascii)
    {
      glk_put_string ((char *) ascii);
      return;
    }

  /* No apparent way to output this character, so print a '?'. */
  glk_put_char ('?');
}


/*
 * gsc_put_char()
 * gsc_put_char_alternate()
 * gsc_put_buffer_using()
 * gsc_put_string()
 * gsc_put_string_alternate()
 *
 * Public functions for writing using the current or fallback locale.
 */
static void
gsc_put_char (const scr_char character)
{
  gsc_put_char_locale (character, gsc_current_locale (), FALSE);
}

static void
gsc_put_char_alternate (const scr_char character)
{
  gsc_put_char_locale (character, gsc_current_locale (), TRUE);
}

static void
gsc_put_buffer_using (const scr_char *buffer,
                      scr_int length, void (*putchar_function) (scr_char))
{
  scr_int index_;

  for (index_ = 0; index_ < length; index_++)
    putchar_function (buffer[index_]);
}

void
gsc_put_string (const scr_char *string)
{
  assert (string);

  gsc_put_buffer_using (string, strlen (string), gsc_put_char);
}

void
gsc_put_string_alternate (const scr_char *string)
{
  assert (string);

  gsc_put_buffer_using (string, strlen (string), gsc_put_char_alternate);
}


/*
 * gsc_unicode_to_locale()
 * gsc_unicode_buffer_to_locale()
 *
 * Convert a unicode character back to an scr_char through a locale.  Used for
 * reverse translations in line input.  Returns '?' if there is no translation
 * available.
 */
static scr_char
gsc_unicode_to_locale (glui32 unicode, const gsc_locale_t *locale)
{
  const gsc_codepages_t *codepage;
  scr_int character;

  /* Always use the main codepage for input. */
  codepage = &locale->main;

  /*
   * Search the unicode table sequentially for the input character.  This is
   * inefficient, but because game input is usually not copious, excusable.
   */
  for (character = 0; character < GSC_TABLE_SIZE; character++)
    {
      if (codepage->unicode[character] == unicode)
        break;
    }

  /* Return the character translation, or '?' if none. */
  return character < GSC_TABLE_SIZE ? (scr_char) character : '?';
}

static void
gsc_unicode_buffer_to_locale (const glui32 *unicode, scr_int length,
                              scr_char *buffer, const gsc_locale_t *locale)
{
  scr_int index_;

  for (index_ = 0; index_ < length; index_++)
    buffer[index_] = gsc_unicode_to_locale (unicode[index_], locale);
}


/* Whether the line being read is echoed by us rather than by the library:
   set by every gsc_read_line_locale. */
int gsc_line_echo_manual = FALSE;

/*
 * gsc_read_line_locale()
 *
 * Read in a line and translate out of the given locale.  Returns the count
 * of characters placed in the buffer.  When a click on the map cancelled the
 * request to start a walk, gsc_sc_walk_clicked is set, the count is 0, and
 * the prompt is left as it was, in the input colour, for the walk's first
 * step to follow.
 */
static scr_int
gsc_read_line_locale (scr_char *buffer,
                      scr_int length, const gsc_locale_t *locale)
{
  event_t event;

  /* Take over input echo wherever the library lets us: a click on the map
     cancels the pending request to start a walk, and with auto-echo the
     cancel would commit an empty input line to the window, ending the
     prompt's line.  The completed line is echoed manually below instead.
     Set every time: echo mode is library state that a Spatterlight autosave
     archives and re-applies, and an older autosave holds it switched on.  A
     library without line echo control (cheapglk) warns about the call. */
  gsc_line_echo_manual = FALSE;
#ifdef GLK_MODULE_LINE_ECHO
  if (glk_gestalt (gestalt_LineInputEcho, 0))
    {
      glk_set_echo_line_event (gsc_main_window, 0);
      gsc_line_echo_manual = TRUE;
    }
#endif
  gsc_sc_walk_clicked = FALSE;

  /* The Runner echoes what the player types in its own "typed" colour, a
     different one from the text the game writes back. */
  gsc_colour_echo (TRUE);

  /*
   * If we have unicode, we have to use it to ensure that characters not in
   * the Latin1 locale are properly translated.
   */
  if (gsc_unicode_enabled)
    {
      glui32 *unicode;

      /*
       * Allocate a unicode buffer long enough to hold all the characters,
       * then read in a unicode line.
       */
      unicode = (decltype(unicode)) gsc_malloc (length * sizeof (*unicode));
      memset (unicode, 0, length * sizeof (*unicode));
      glk_request_line_event_uni (gsc_main_window, unicode, length, 0);
      gsc_event_wait (evtype_LineInput, &event);

      /* A click on the map took the request: there is no line. */
      if (gsc_sc_walk_clicked)
        {
          free (unicode);
          return 0;
        }
      if (gsc_line_echo_manual)
        {
          glk_set_style (style_Input);
          glk_put_buffer_uni (unicode, event.val1);
          glk_set_style (style_Normal);
          glk_put_char ('\n');
        }

      /* Convert the unicode buffer out, then free it. */
      gsc_unicode_buffer_to_locale (unicode, event.val1, buffer, locale);
      free (unicode);

      /* Return the count of characters placed in the buffer. */
      gsc_colour_echo (FALSE);
      return event.val1;
    }

  /* No success with unicode, so fall back to standard line input. */
  glk_request_line_event (gsc_main_window, buffer, length, 0);
  gsc_event_wait (evtype_LineInput, &event);

  if (gsc_sc_walk_clicked)
    return 0;
  if (gsc_line_echo_manual)
    {
      glk_set_style (style_Input);
      glk_put_buffer (buffer, event.val1);
      glk_set_style (style_Normal);
      glk_put_char ('\n');
    }

  /* Return the count of characters placed in the buffer. */
  gsc_colour_echo (FALSE);
  return event.val1;
}


/*
 * gsc_read_line()
 *
 * Public function for reading using the current or fallback locale.
 */
scr_int
gsc_read_line (scr_char *buffer, scr_int length)
{
  return gsc_read_line_locale (buffer, length, gsc_current_locale ());
}


/*
 * gsc_status_printed_width()
 *
 * The number of grid cells gsc_put_string() will fill for this string.  Nearly
 * always one per byte, but a character with neither an iso 8859-1 nor a
 * printable unicode form falls back to a multi-character ascii substitute (see
 * gsc_put_char_locale()) -- "sh" for a Cyrillic sha, say -- so a plain strlen()
 * can under-count.  This mirrors gsc_put_char_locale()'s decision tree so that
 * right-justification lands where the text actually ends.
 *
 * Upstream instead subtracted a flat ten-column "slop" from the right margin to
 * absorb that case, which left every status line -- all-ascii ones included --
 * floating well short of the right edge.
 */
glui32
gsc_status_printed_width (const scr_char *string)
{
  const gsc_codepages_t *codepage = &gsc_current_locale ()->main;
  glui32 width = 0;
  scr_int index_;

  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      const unsigned char character = (unsigned char) string[index_];
      const glui32 unicode = codepage->unicode[character];
      const char *const ascii = codepage->ascii[character];

      if (unicode > 0)
        {
          /* Printed as iso 8859-1, or directly as unicode: one cell either way. */
          if (unicode < GSC_ISO_8859_EQUIVALENCE
              && (unicode == '\n'
                  || (unicode >= GSC_MIN_PRINTABLE
                      && unicode <= GSC_MAX_PRINTABLE)
                  || glk_gestalt (gestalt_CharOutput,
                                  unicode) == gestalt_CharOutput_ExactPrint))
            {
              width++;
              continue;
            }
          if (gsc_unicode_enabled
              && glk_gestalt (gestalt_CharOutput,
                              unicode) == gestalt_CharOutput_ExactPrint)
            {
              width++;
              continue;
            }
        }

      /* Otherwise an ascii substitute, or the '?' stand-in for no mapping. */
      width += ascii ? (glui32) strlen (ascii) : 1;
    }

  return width;
}

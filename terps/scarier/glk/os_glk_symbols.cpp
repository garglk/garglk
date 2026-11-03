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
 * os_glk_symbols.cpp: the symbol-font (Webdings, Wingdings, Wingdings 3,
 * Symbol) to Unicode tables and the string writer that uses them.  Split
 * out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

/*
 * Webdings-to-Unicode translation table, indexed by character code.
 *
 * Webdings is a symbol font: its byte values are pictograms, not letters, so a
 * game that writes <font face="Webdings">...</font> is drawing pictures with
 * what would otherwise be text.  Glk has no way to select a font by name, but
 * most Webdings pictograms were given Unicode code points in Unicode 7.0 (the
 * Wingdings/Webdings additions), so they can be reproduced by translating to
 * the equivalent Unicode character and printing that instead.
 *
 * The character-to-pictogram assignments were read off the glyph names in the
 * `post` table of the Webdings font itself, rather than guessed: entry 0xFF,
 * for instance, is the glyph named "peace", i.e. U+1F54A DOVE OF PEACE.  That
 * is the one this table actually exists for -- Topaz (Woodfish) draws a dove
 * under its title with <font face="Webdings" size=72>\xFF</font>, which
 * without translation prints as a stray "y-umlaut".  The rest are filled in
 * where the Unicode equivalent is unambiguous; entries left 0 have no good
 * equivalent and print as '?'.
 */
static const glui32 GSC_WEBDINGS_TO_UNICODE[] = {
  /* 0x00 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x20 space, spider, web, no-piracy, sunglasses, trophy, medal, link */
  0x0020, 0x1F577, 0x1F578, 0x1F572, 0x1F576, 0x1F3C6, 0x1F396, 0x1F517,
  /* 0x28 speech bubbles, then new/updated/hot/ribbon/checkerboard (no map) */
  0x1F5E8, 0x1F5E9, 0, 0, 0, 0, 0, 0,
  /* 0x30 minimise/tile (no map), window controls, transport controls */
  0, 0x25A1, 0, 0x25C0, 0x25B6, 0x25B2, 0x25BC, 0x23EA,
  0x23E9, 0x23EE, 0x23ED, 0x23F8, 0x23F9, 0x23FA, 0, 0x1F5F3,
  /* 0x40 tools, construction, town/city/site/desert/factory (no map), home */
  0x1F6E0, 0x1F6A7, 0, 0, 0, 0, 0, 0,
  0x1F3E0, 0x1F3D6, 0x1F3DD, 0x1F6E3, 0x1F50D, 0x26F0, 0x1F441, 0x1F442,
  /* 0x50 park, tent, rail (no map), stadium, ship, sound on/off */
  0x1F3DE, 0x26FA, 0, 0x1F3DF, 0x1F6F3, 0x1F50A, 0x1F507, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x60 loop (no map), check, bicycle, ... fire ... */
  0, 0x2714, 0x1F6B2, 0, 0, 0, 0x1F525, 0,
  0x2695, 0x2139, 0x1F6E9, 0x1F6F0, 0x2726, 0, 0x25CF, 0x26F5,
  /* 0x70 police, refresh, close, help, train, metro, bus, flag */
  0x1F693, 0x1F503, 0x2716, 0x2753, 0x1F686, 0x1F687, 0x1F68D, 0x1F6A9,
  0, 0x26D4, 0x1F6AD, 0, 0, 0, 0, 0,
  /* 0x80 men, women, boy/girl (no map), baby */
  0x1F6B9, 0x1F6BA, 0, 0, 0x1F6BC, 0, 0, 0x26F7,
  0, 0x1F3CC, 0x1F3CA, 0x1F3C4, 0x1F3CD, 0x1F3CE, 0x1F698, 0,
  /* 0x90 ... credit card ... mail ... */
  0, 0, 0, 0x1F4B3, 0, 0, 0, 0,
  0, 0, 0, 0x1F582, 0, 0, 0, 0,
  /* 0xA0 ... clipboard, calendar, book, news, art, theatre, music */
  0, 0, 0, 0, 0x1F4CB, 0, 0x1F4C5, 0x1F56E,
  0, 0x1F5DE, 0, 0, 0, 0x1F3A8, 0x1F3AD, 0x1F3B5,
  /* 0xB0 microphone, headphones, disc, film frames, ticket, clapper, video */
  0, 0x1F3A4, 0x1F3A7, 0x1F4BF, 0x1F39E, 0, 0x1F39F, 0x1F3AC,
  0, 0x1F4F9, 0, 0x1F4FB, 0x1F39A, 0x1F39B, 0x1F4FA, 0,
  /* 0xC0 ... joystick ... fax, pager, mobile ... printer */
  0, 0, 0, 0x1F579, 0, 0, 0x1F4E0, 0x1F4DF,
  0x1F4F1, 0, 0x1F5A8, 0, 0, 0, 0, 0x1F512,
  /* 0xD0 unlocked, ... weather from 0xD5 */
  0x1F513, 0, 0, 0, 0, 0x2600, 0x1F324, 0x1F325,
  0x1F326, 0x2601, 0x1F328, 0x1F327, 0x1F329, 0x1F32A, 0x1F32C, 0x1F32B,
  /* 0xE0 moon, thermometer, ... dining ... parking, wheelchair, warning */
  0x1F319, 0x1F321, 0, 0, 0x1F374, 0, 0, 0,
  0x1F17F, 0x267F, 0x26A0, 0, 0, 0, 0, 0,
  /* 0xF0 ... airplane, animal, bird, fish, dog, cat, rockets */
  0, 0x2708, 0, 0x1F426, 0x1F41F, 0x1F415, 0x1F408, 0,
  /* 0xF8 rockets (no map), world map, globes, and the peace dove at 0xFF */
  0, 0, 0, 0x1F5FA, 0x1F30D, 0x1F30E, 0x1F30F, 0x1F54A
};

/*
 * Wingdings, the other symbol face Adrift games reach for -- roughly four
 * times as often as Webdings across the v4 corpus.  Read off the `post` table
 * glyph names of Wingdings.ttf the same way the Webdings table above was, and
 * cross-checked against the Unicode character names (Unicode 7.0 encoded this
 * font's pictograms specifically so that documents using it could be
 * converted): 0x53 is the glyph named "droplet", i.e. U+1F4A7 DROPLET.
 *
 * The table matters for more than decoration.  The Most Average Man in the
 * World types its aliens' speech as plain English inside a Wingdings tag, so
 * without translation the interpreter hands the player a crib the Runner never
 * shows ("welcome to your ship. Everything is ready sir ..."); and The
 * Reluctant Resurrectee counts down a row of droplets, 0x53, which reads as
 * "SSSSS" untranslated.
 *
 * Entries left 0 have no good Unicode equivalent and print as '?' -- which for
 * 0xB4, a question mark in a diamond, is very nearly the glyph itself.
 */
static const glui32 GSC_WINGDINGS_TO_UNICODE[] = {
  /* 0x00 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x20 space, pencil, scissors, glasses, bell, book, candle */
  0x0020, 0x1F589, 0x2702, 0x2701, 0x1F453, 0x1F56D, 0x1F56E, 0x1F56F,
  /* 0x28 phones, envelopes, mailboxes */
  0x1F57F, 0x2706, 0x1F582, 0x1F583, 0x1F4EA, 0x1F4EB, 0x1F4EC, 0x1F4ED,
  /* 0x30 folders, documents, filing cabinet, hourglass */
  0x1F4C1, 0x1F4C2, 0x1F5CE, 0x1F5CF, 0x1F5D0, 0x1F5C4, 0x231B, 0x2328,
  /* 0x38 mouse, trackball, PC, disks, tape, writing hands */
  0x1F5B0, 0x1F5B2, 0x1F5A5, 0x1F5B4, 0x1F5AB, 0x1F5AC, 0x2707, 0x270D,
  /* 0x40 hands and faces */
  0x1F58E, 0x270C, 0x1F44C, 0x1F44D, 0x1F44E, 0x1F448, 0x1F449, 0x1F446,
  /* 0x48 more hands, faces, bomb, skull, flag */
  0x1F447, 0x1F590, 0x263A, 0x1F610, 0x2639, 0x1F4A3, 0x2620, 0x1F3F3,
  /* 0x50 pennant, plane, sun, drop, snowflake, crosses */
  0x1F3F1, 0x2708, 0x263C, 0x1F4A7, 0x2744, 0x1F546, 0x271E, 0x1F548,
  /* 0x58 crosses, star of David, crescent, yin-yang, om, dharma */
  0x2720, 0x2721, 0x262A, 0x262F, 0x0950, 0x2638, 0x2648, 0x2649,
  /* 0x60 zodiac (Gemini .. Pisces), ampersand ornaments */
  0x264A, 0x264B, 0x264C, 0x264D, 0x264E, 0x264F, 0x2650, 0x2651,
  /* 0x68 zodiac continued, ampersands, circles and squares */
  0x2652, 0x2653, 0x1F670, 0x1F675, 0x2B24, 0x1F53E, 0x25A0, 0x25A1,
  /* 0x70 squares, lozenges, diamonds, keyboard marks, florettes */
  0x1F790, 0x2751, 0x2752, 0x2B27, 0x29EB, 0x25C6, 0x2756, 0x2B25,
  /* 0x78 clear/escape/command, florettes, heavy quotes */
  0x2327, 0x29B8, 0x2318, 0x2740, 0x273F, 0x275D, 0x275E, 0,
  /* 0x80 circled sans-serif digits 0-7 */
  0x1F10B, 0x2780, 0x2781, 0x2782, 0x2783, 0x2784, 0x2785, 0x2786,
  /* 0x88 digits 8-10, then negative circled 0-4 */
  0x2787, 0x2788, 0x2469, 0x1F10C, 0x278A, 0x278B, 0x278C, 0x278D,
  /* 0x90 negative circled digits 5-10 */
  0x278E, 0x278F, 0x2790, 0x2791, 0x2792, 0x277F, 0x1F652, 0x1F650,
  /* 0x98 bud and vine leaves, small circles */
  0x1F651, 0x1F653, 0x1F65E, 0x1F65C, 0x1F65D, 0x1F65F, 0x2022, 0x25CF,
  /* 0xA0 small square, rings, target, shadowed circle, square */
  0x25AA, 0x25E6, 0x25CB, 0x25EF, 0x1F78A, 0x1F78B, 0x1F53F, 0x25FE,
  /* 0xA8 small box, then 3-, 4-, 5-, 6-, 8-pointed stars */
  0x25AB, 0x1F7C0, 0x1F7C4, 0x1F7C9, 0x1F7CB, 0x1F7CE, 0x1F7D2, 0x1F7CF,
  /* 0xB0 registration marks, cusps, query (no map), circled star */
  0x2BD0, 0x2316, 0x2BCE, 0x2BCF, 0, 0x272A, 0x2730, 0x1F550,
  /* 0xB8 clock faces two through nine o'clock */
  0x1F551, 0x1F552, 0x1F553, 0x1F554, 0x1F555, 0x1F556, 0x1F557, 0x1F558,
  /* 0xC0 ten, eleven, twelve o'clock, then elbow arrows */
  0x1F559, 0x1F55A, 0x1F55B, 0x21B2, 0x21B3, 0x21B0, 0x21B1, 0x2B11,
  /* 0xC8 elbow arrows, quilt squares, leaves */
  0x2B0F, 0x2B10, 0x2B0E, 0x1F668, 0x1F669, 0x1F655, 0x1F654, 0x1F657,
  /* 0xD0 leaves, delete keys, solid arrowheads */
  0x1F656, 0x1F658, 0x1F659, 0x1F65A, 0x1F65B, 0x232B, 0x2326, 0x2B9C,
  /* 0xD8 arrowheads, circled arrows, light barb arrows */
  0x27A4, 0x2B9D, 0x2B9F, 0x2B88, 0x2B8A, 0x2B89, 0x2B8B, 0x1F860,
  /* 0xE0 light barb arrows continued */
  0x1F862, 0x1F861, 0x1F863, 0x1F864, 0x1F865, 0x1F867, 0x1F866, 0x1F868,
  /* 0xE8 barb arrows */
  0x1F86A, 0x1F869, 0x1F86B, 0x1F86C, 0x1F86D, 0x1F86F, 0x1F86E, 0x2B05,
  /* 0xF0 block arrows */
  0x27A1, 0x2B06, 0x2B07, 0x2B0C, 0x2B0D, 0x2B09, 0x2B08, 0x2B0B,
  /* 0xF8 block arrow, dashes (no map), X and check marks, logo */
  0x2B0A, 0, 0, 0x2718, 0x2714, 0x2612, 0x2611, 0
};

/*
 * Wingdings 3, which is almost entirely arrows and triangles.  The corpus uses
 * exactly two of them -- 0x5F and 0x61, the outlined right-pointing block
 * arrows either side of "orightshadrt" -- but the arrow ranges are unambiguous
 * enough to map wholesale.  Beyond 0x88 the font runs into chevron and curved
 * arrow sets with no Unicode counterpart; those stay 0.
 */
static const glui32 GSC_WINGDINGS3_TO_UNICODE[] = {
  /* 0x00 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x20 space, then thin arrows in eight directions */
  0x0020, 0x2190, 0x2192, 0x2191, 0x2193, 0x2196, 0x2197, 0x2199,
  /* 0x28 south-east, tab and paging arrows */
  0x2198, 0x21E4, 0x21E5, 0x2912, 0x2913, 0x21F1, 0x21F2, 0x21DE,
  /* 0x30 page down, two-headed and dashed arrows */
  0x21DF, 0x2194, 0x2195, 0x21E0, 0x21E2, 0x21E1, 0x21E3, 0x21AF,
  /* 0x38 elbow arrows */
  0x21B2, 0x21B3, 0x21B0, 0x21B1, 0x2B11, 0x2B0F, 0x2B10, 0x2B0E,
  /* 0x40 return and newline, opposed and parallel arrows */
  0x21B5, 0x21B4, 0x21B5, 0x21B4, 0x21C4, 0x21C5, 0, 0,
  /* 0x48 parallel arrows, then unmapped u-turns */
  0x21C7, 0x21C9, 0x21C8, 0x21CA, 0, 0, 0, 0,
  /* 0x50 circular arrows, keyboard marks */
  0x21BB, 0x21BA, 0x238B, 0x2324, 0x2303, 0x2325, 0, 0,
  /* 0x58 shift/caps lock, outlined block arrows */
  0x21EA, 0x21EA, 0x21E6, 0x21E8, 0x21E6, 0x21E8, 0x21E6, 0x21E8,
  /* 0x60 outlined block arrows continued */
  0x21E6, 0x21E8, 0x21E6, 0x21E8, 0x21E6, 0x21E8, 0x2B05, 0x27A1,
  /* 0x68 solid block arrows */
  0x2B06, 0x2B07, 0x2B09, 0x2B08, 0x2B0B, 0x2B0A, 0x2B0C, 0x2B0D,
  /* 0x70 triangles, solid and open */
  0x25B2, 0x25BC, 0x25B3, 0x25BD, 0x25C0, 0x25B6, 0x25C1, 0x25B7,
  /* 0x78 corner and isosceles triangles */
  0x25E3, 0x25E2, 0x25E4, 0x25E5, 0x1F780, 0x1F782, 0x1F781, 0,
  /* 0x80 isosceles and small triangles, arrowheads */
  0x1F783, 0x25B4, 0x25BE, 0x25C2, 0x25B8, 0x2B9C, 0x2B9E, 0x2B9D,
  /* 0x88 arrowhead; the chevron sets beyond have no equivalents */
  0x2B9F, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Symbol, the Adobe/PostScript symbol face -- mostly Greek, with mathematics
 * either side of it.  Unlike Webdings and Wingdings this one has a published
 * encoding, so the table is Adobe's `symbol.txt` glyph list rather than
 * anything read off a font here (macOS ships no Symbol.ttf to read).  Only one
 * v4 game reaches for it, and only for 0xB7, the bullet.
 */
static const glui32 GSC_SYMBOL_TO_UNICODE[] = {
  /* 0x00 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x20 ASCII punctuation, with universal, existential, such-that */
  0x0020, 0x0021, 0x2200, 0x0023, 0x2203, 0x0025, 0x0026, 0x220B,
  /* 0x28 parens, asteriskmath, plus, comma, minus, period, slash */
  0x0028, 0x0029, 0x2217, 0x002B, 0x002C, 0x2212, 0x002E, 0x002F,
  /* 0x30 digits */
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
  /* 0x38 digits, colon, semicolon, relations, question */
  0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
  /* 0x40 congruent, then Alpha .. Eta */
  0x2245, 0x0391, 0x0392, 0x03A7, 0x0394, 0x0395, 0x03A6, 0x0393,
  /* 0x48 Eta .. Omicron */
  0x0397, 0x0399, 0x03D1, 0x039A, 0x039B, 0x039C, 0x039D, 0x039F,
  /* 0x50 Pi .. Upsilon, final sigma, Omega */
  0x03A0, 0x0398, 0x03A1, 0x03A3, 0x03A4, 0x03A5, 0x03C2, 0x03A9,
  /* 0x58 Xi, Psi, Zeta, brackets, therefore, perpendicular, overbar */
  0x039E, 0x03A8, 0x0396, 0x005B, 0x2234, 0x005D, 0x22A5, 0x005F,
  /* 0x60 alpha .. eta */
  0x203E, 0x03B1, 0x03B2, 0x03C7, 0x03B4, 0x03B5, 0x03C6, 0x03B3,
  /* 0x68 eta .. omicron */
  0x03B7, 0x03B9, 0x03D5, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BF,
  /* 0x70 pi .. omega */
  0x03C0, 0x03B8, 0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03D6, 0x03C9,
  /* 0x78 xi, psi, zeta, braces, bar, similar */
  0x03BE, 0x03C8, 0x03B6, 0x007B, 0x007C, 0x007D, 0x223C, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  /* 0xA0 Euro, Upsilon hook, prime, relations, florin, card suits */
  0x20AC, 0x03D2, 0x2032, 0x2264, 0x2044, 0x221E, 0x0192, 0x2663,
  /* 0xA8 card suits, arrows */
  0x2666, 0x2665, 0x2660, 0x2194, 0x2190, 0x2191, 0x2192, 0x2193,
  /* 0xB0 degree, plus-minus, double prime, times, partial, bullet */
  0x00B0, 0x00B1, 0x2033, 0x2265, 0x00D7, 0x221D, 0x2202, 0x2022,
  /* 0xB8 divide, relations, ellipsis, arrow extenders, carriage return */
  0x00F7, 0x2260, 0x2261, 0x2248, 0x2026, 0x23D0, 0x23AF, 0x21B5,
  /* 0xC0 aleph, fraktur, Weierstrass, circled operators, set theory */
  0x2135, 0x2111, 0x211C, 0x2118, 0x2297, 0x2295, 0x2205, 0x2229,
  /* 0xC8 set theory continued */
  0x222A, 0x2283, 0x2287, 0x2284, 0x2282, 0x2286, 0x2208, 0x2209,
  /* 0xD0 angle, gradient, marks, product, radical, logic */
  0x2220, 0x2207, 0x00AE, 0x00A9, 0x2122, 0x220F, 0x221A, 0x22C5,
  /* 0xD8 logic, double arrows */
  0x00AC, 0x2227, 0x2228, 0x21D4, 0x21D0, 0x21D1, 0x21D2, 0x21D3,
  /* 0xE0 lozenge, angle bracket, sans marks, summation, bracket pieces */
  0x25CA, 0x2329, 0x00AE, 0x00A9, 0x2122, 0x2211, 0x239B, 0x239C,
  /* 0xE8 bracket and brace pieces */
  0x239D, 0x23A1, 0x23A2, 0x23A3, 0x23A7, 0x23A8, 0x23A9, 0x23AA,
  /* 0xF0 angle bracket, integral and its pieces, bracket pieces */
  0, 0x232A, 0x222B, 0x2320, 0x23AE, 0x2321, 0x239E, 0x239F,
  /* 0xF8 bracket and brace pieces */
  0x23A0, 0x23A4, 0x23A5, 0x23A6, 0x23AB, 0x23AC, 0x23AD, 0
};

/*
 * gsc_symbol_font_from_face()
 *
 * Return the symbol font a lowercased face= tag argument names, or
 * GSC_SYMBOL_NONE for the ordinary text faces.  The comparisons include the
 * closing quote, so "wingdings" and "wingdings 3" separate cleanly.
 */
gsc_symbol_font_t
gsc_symbol_font_from_face (const char *face)
{
  assert (face);

  if (strncmp (face, "face=\"webdings\"", 15) == 0)
    return GSC_SYMBOL_WEBDINGS;
  if (strncmp (face, "face=\"wingdings\"", 16) == 0)
    return GSC_SYMBOL_WINGDINGS;
  if (strncmp (face, "face=\"wingdings 3\"", 18) == 0)
    return GSC_SYMBOL_WINGDINGS3;
  if (strncmp (face, "face=\"symbol\"", 13) == 0)
    return GSC_SYMBOL_SYMBOL;

  return GSC_SYMBOL_NONE;
}


/*
 * gsc_put_string_symbol()
 *
 * Write a string whose bytes are symbol-font pictograms, translating each to
 * its Unicode equivalent.  Characters with no equivalent, and any character
 * the Glk library cannot print exactly, fall back to '?' -- the same thing
 * gsc_put_char_locale() does when it runs out of options.
 */
void
gsc_put_string_symbol (const scr_char *string, gsc_symbol_font_t symbol_font)
{
  scr_int index_;

  assert (string);

  /* This path writes through glk_put_char directly rather than through
     gsc_put_char_locale, so mark the window here as well. */
  if (string[0] != '\0' && gsc_main_window
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_main_window_empty = FALSE;

  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      const unsigned char character = (unsigned char) string[index_];
      glui32 unicode = 0;

      switch (symbol_font)
        {
        case GSC_SYMBOL_WEBDINGS:
          unicode = GSC_WEBDINGS_TO_UNICODE[character];
          break;
        case GSC_SYMBOL_WINGDINGS:
          unicode = GSC_WINGDINGS_TO_UNICODE[character];
          break;
        case GSC_SYMBOL_WINGDINGS3:
          unicode = GSC_WINGDINGS3_TO_UNICODE[character];
          break;
        case GSC_SYMBOL_SYMBOL:
          unicode = GSC_SYMBOL_TO_UNICODE[character];
          break;
        default:
          break;
        }

      /*
       * Control characters are layout, not pictograms -- none of these fonts
       * puts a glyph below 0x20 -- and have to survive translation so that
       * centring and line breaks still work inside the tag.  Provenance's
       * table of contents, for one, is a Symbol bullet followed by a tab.
       */
      if (character < ' ')
        {
          glk_put_char (character);
          continue;
        }

      if (unicode > 0 && unicode < GSC_ISO_8859_EQUIVALENCE)
        {
          glk_put_char ((unsigned char) unicode);
          continue;
        }

      if (unicode > 0 && gsc_unicode_enabled
          && glk_gestalt (gestalt_CharOutput,
                          unicode) == gestalt_CharOutput_ExactPrint)
        {
          glk_put_char_uni (unicode);
          continue;
        }

      glk_put_char ('?');
    }
}

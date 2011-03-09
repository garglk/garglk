/*-
 * Copyright 2009-2010 Chris Spiegel.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "dict.h"
#include "memory.h"
#include "process.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

static uint8_t *separators;
static uint8_t num_separators;

static uint16_t GET_WORD(uint8_t *base)
{
  return (base[0] << 8) | base[1];
}
static void MAKE_WORD(uint8_t *base, uint16_t val)
{
  base[0] = val >> 8;
  base[1] = val & 0xff;
}

static int in_atable(int c)
{
  /* 52 is A2 character 6, which is special and should not
   * be matched, so skip over it.
   */
  for(int i = 0;  i < 52    ; i++) if(atable[i] == c) return i;
  for(int i = 53; i < 26 * 3; i++) if(atable[i] == c) return i;

  return -1;
}

/* Add the character c to the nth position of the encoded text.  c is a
 * 5-bit value (either a shift character, which selects an alphabet, or
 * the index into the current alphabet).
 */
static void add_zchar(int c, int n, uint8_t *encoded)
{
  uint16_t w = GET_WORD(&encoded[2 * (n / 3)]);

  /* From §3.2:
   * --first byte-------   --second byte---
   * 7    6 5 4 3 2  1 0   7 6 5  4 3 2 1 0
   * bit  --first--  --second---  --third--
   *
   * So to figure out which third of the word to store to:
   * If n is 0, 3, 6, ... then store to the first (left shift 10).
   * If n is 1, 4, 7, ... then store to the second (left shift 5).
   * If n is 2, 5, 8, ... then store to the third (left shift 0).
   * “Or” into the previous value because, unless this is the first
   * character, there are already values we’ve stored there.
   */
  w |= (c & 0x1f) << (5 * (2 - (n % 3)));

  MAKE_WORD(&encoded[2 * (n / 3)], w);
}

/* Encode the text at “s”, of length “len” (there is not necessarily a
 * terminating null character) into the buffer “encoded”.
 *
 * For V3 the encoded text is 6 Z-characters (4 bytes); for V4 and above
 * it’s 9 characters (6 bytes).  Due to the nature of the loop here,
 * it’s possible to encode too many bytes.  For example, if the string
 * given is "aaa<" in a V3 game, the three 'a' characters will take up a
 * word (all three being packed into one), but the single '<' character
 * will take up two words (one full word and a third of the next) due to
 * the fact that '<' is not in the alphabet table.  Thus the encoded
 * text will be 7 characters. This is OK because routines that use the
 * encoded string are smart enough to only pay attention to the first 6
 * or 9 Z-characters; and partial Z-characters are OK per §3.6.1.
 *
 * 1.1 of the standard revises the encoding for V1 and V2 games.  I am
 * not implementing the new rules for two basic reasons:
 * 1) It apparently only affects three (unnecessary) dictionary words in
 *    the known V1-2 games.
 * 2) Because of 1, it is not worth the effort to peek ahead and see
 *    what the next character is to determine whether to shift once or
 *    to lock.
 */
static void encode_string(const uint8_t *s, size_t len, uint8_t encoded[static 8])
{
  int n = 0;
  const int res = zversion <= 3 ? 6 : 9;
  const int shiftbase = zversion <= 2 ? 1 : 3;

  memset(encoded, 0, 8);

  for(size_t i = 0; i < len && n < res; i++)
  {
    int pos;

    pos = in_atable(s[i]);
    if(pos >= 0)
    {
      int shift = pos / 26;
      int c = pos % 26;

      if(shift) add_zchar(shiftbase + shift, n++, encoded);
      add_zchar(c + 6, n++, encoded);
    }
    else
    {
      add_zchar(5, n++, encoded);
      add_zchar(6, n++, encoded);
      add_zchar(s[i] >> 5, n++, encoded);
      add_zchar(s[i] & 0x1f, n++, encoded);
    }
  }

  while(n < res)
  {
    add_zchar(5, n++, encoded);
  }

  /* §3.2: the MSB of the last encoded word must be set. */
  if(zversion <= 3) encoded[2] |= 0x80;
  else              encoded[4] |= 0x80;
}

static int dict_compar(const void *a, const void *b)
{
  return memcmp(a, b, zversion <= 3 ? 4 : 6);
}
static uint16_t dict_find(const uint8_t *token, size_t len, uint16_t dictionary)
{
  uint8_t elength;
  uint16_t base;
  long nentries;
  uint8_t *ret = NULL;
  uint8_t encoded[8];

  encode_string(token, len, encoded);

  elength = user_byte(dictionary + num_separators + 1);
  nentries = (int16_t)user_word(dictionary + num_separators + 2);
  base = dictionary + num_separators + 2 + 2;

  if(nentries > 0)
  {
    ret = bsearch(encoded, &memory[base], nentries, elength, dict_compar);
  }
  else
  {
    for(long i = 0; i < -nentries; i++)
    {
      uint8_t *entry = &memory[base + (i * elength)];

      if(dict_compar(encoded, entry) == 0)
      {
        ret = entry;
        break;
      }
    }
  }

  if(ret == NULL) return 0;

  return base + (ret - &memory[base]);
}

static int is_sep(uint8_t c)
{
  return c == ZSCII_SPACE || memchr(separators, c, num_separators) != NULL;
}

static void handle_token(const uint8_t *base, const uint8_t *token, int len, uint16_t parse, uint16_t dictionary, int found, int flag)
{
  uint16_t d;
  const uint8_t examine[] = { 'e', 'x', 'a', 'm', 'i', 'n', 'e' };
  const uint8_t again[] = { 'a', 'g', 'a', 'i', 'n' };
  const uint8_t wait[] = { 'w', 'a', 'i', 't' };

  d = dict_find(token, len, dictionary);

  if(!options.disable_abbreviations && base == token && len == 1)
  {
    if     (*token == 'x') d = dict_find(examine, sizeof examine, dictionary);
    else if(*token == 'g') d = dict_find(again, sizeof again, dictionary);
    else if(*token == 'z') d = dict_find(wait, sizeof wait, dictionary);
  }

  if(flag && d == 0) return;

  parse = parse + 2 + (found * 4);

  user_store_word(parse, d);

  user_store_byte(parse + 2, len);

  if(zversion <= 4) user_store_byte(parse + 3, token - base + 1);
  else              user_store_byte(parse + 3, token - base + 2);
}

/* This is strlen(), except for the types. */
static uint32_t find_zero(const uint8_t *p)
{
  const uint8_t *base = p;

  while(*p != 0) p++;

  return p - base;
}

/* The behavior of tokenize is described in §15 (under the read opcode)
 * and §13.
 *
 * For the text buffer, byte 0 is ignored in both V3/4 and V5+.
 * Byte 1 of V3/4 is the start of the string, while in V5+ it is the
 * length of the string.
 * Byte 2 of V5+ is the start of the string.  V3/4 strings have a null
 * terminator, while V5+ do not.
 *
 * For the parse buffer, byte 0 contains the maximum number of tokens
 * that can be read.
 * The number of tokens found is stored in byte 1.
 * Each token is then represented by a 4-byte chunk with the following
 * information:
 * • The first two bytes are the byte address of the dictionary entry
 *   for the token, or 0 if the token was not found in the dictionary.
 * • The next byte is the length of the token.
 * • The final byte is the offset in the string of the token.
 */
void tokenize(uint16_t text, uint16_t parse, uint16_t dictionary, int flag)
{
  const uint8_t *p, *lastp;
  uint8_t *string = &memory[text + 1 + (zversion >= 5)];
  uint32_t text_len;
  const int maxwords = user_byte(parse);
  int in_word = 0;
  int found = 0;

  if(dictionary == 0) dictionary = header.dictionary;

  ZASSERT(dictionary != 0, "attempted to tokenize without a valid dictionary");

  num_separators = user_byte(dictionary);
  separators = &memory[dictionary + 1];

  if(zversion >= 5) text_len = user_byte(text + 1);
  else              text_len = find_zero(&memory[text + 1]);

  for(p = string; p - string < text_len && *p == ZSCII_SPACE; p++);
  lastp = p;

  text_len -= (p - string);

  do
  {
    if(!in_word && text_len != 0 && !is_sep(*p))
    {
      in_word = 1;
      lastp = p;
    }

    if(text_len == 0 || is_sep(*p))
    {
      if(in_word)
      {
        handle_token(string, lastp, p - lastp, parse, dictionary, found++, flag);
      }

      /* §13.6.1: Separators (apart from a space) are tokens too. */
      if(text_len != 0 && *p != ZSCII_SPACE)
      {
        handle_token(string, p, 1, parse, dictionary, found++, flag);
      }

      if(found == maxwords) break;

      in_word = 0;
    }

    p++;

  } while(text_len--);

  user_store_byte(parse + 1, found);
}

static void encode_text(uint32_t text, uint16_t len, uint16_t coded)
{
  uint8_t encoded[8];

  encode_string(&memory[text], len, encoded);
  memcpy(&memory[coded], encoded, 6);
}

void ztokenise(void)
{
  if(znargs < 3) zargs[2] = 0;
  if(znargs < 4) zargs[3] = 0;

  tokenize(zargs[0], zargs[1], zargs[2], zargs[3]);
}

void zencode_text(void)
{
  encode_text(zargs[0] + zargs[2], zargs[1], zargs[3]);
}

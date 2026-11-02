//
//  decompress_text.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Implements the text decompression scheme used by Robin of Sherwood
//  and Seas of Blood.
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <string.h>

#include "decompress_text.h"
#include "scott.h"

#define DECOMPRESS_BUF_SIZE 256

/* Decompress a 5-bit packed string from the Robin of Sherwood / Seas of Blood
   text encoding.

   Strings are stored back-to-back. Each starts with a length/flags byte:
   bits 0–6 give the byte offset to the next string, and bit 6 (inverted)
   is the initial capitalize flag. The payload is a stream of 5-bit character
   codes packed 8 characters per 5 bytes, indexed into the alphabet below.
   Code 0x1C (→ '\x01') is a sentence-break sentinel that capitalizes the
   next letter. Code 0x1F falls past the 31-char alphabet onto its NUL
   terminator, ending the string. Periods and commas auto-insert a trailing
   space (and periods also set the capitalize flag). */
char *DecompressText(uint8_t *source, int stringindex)
{
    /* 5-bit lookup: indices 0–30 map to printable characters;
       index 28 (0x1C) is the capitalize sentinel '\x01';
       index 31 (0x1F) reads the C string's NUL terminator → end of string */
    static const char *alphabet = " abcdefghijklmnopqrstuvwxyz'\x01,.";

    uint8_t decompressed[DECOMPRESS_BUF_SIZE];
    int idx = 0;

    /* Skip past preceding strings using each one's length byte */
    for (int i = 0; i < stringindex; i++) {
        source += *source & 0x7F;
        if (source >= entire_file + file_length)
            return NULL;
    }

    /* Bit 6 clear means the first letter should be capitalized */
    int capitalize = ((*source & 0x40) == 0);
    source++;

    do {
        if (source + 4 >= entire_file + file_length)
            return NULL;

        /* Pack 5 bytes (40 bits) into a uint64 for easy 5-bit extraction */
        uint64_t fortybits = ((uint64_t)source[0] << 32) | ((uint64_t)source[1] << 24)
            | ((uint64_t)source[2] << 16) | ((uint64_t)source[3] << 8) | source[4];
        source += 5;

        /* Extract 8 characters (5 bits each) from the 40-bit group */
        for (int j = 35; j >= 0; j -= 5) {
            int c = alphabet[(fortybits >> j) & 0x1f];

            if (c == 0x01) {
                /* Capitalize sentinel: emit a space now, capitalize next letter */
                capitalize = 1;
                c = ' ';
            } else if (c >= 'a' && capitalize) {
                c = toupper(c);
                capitalize = 0;
            }

            decompressed[idx++] = c;

            /* 5-bit value 0x1F maps past the alphabet to its NUL terminator */
            if (c == 0) {
                char *result = MemAlloc(idx);
                memcpy(result, decompressed, idx);
                return result;
            }

            /* Punctuation is followed by an automatic space */
            if (c == '.' || c == ',') {
                if (c == '.')
                    capitalize = 1;
                if (idx < DECOMPRESS_BUF_SIZE)
                    decompressed[idx++] = ' ';
            }

            /* Buffer full without hitting a terminator — force NUL and return */
            if (idx >= DECOMPRESS_BUF_SIZE - 1) {
                decompressed[idx] = '\0';
                char *result = MemAlloc(idx + 1);
                memcpy(result, decompressed, idx + 1);
                return result;
            }
        }
    } while (idx < DECOMPRESS_BUF_SIZE);
    return NULL;
}

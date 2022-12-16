//
//  decompresstext.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <string.h>

#include "decompresstext.h"

#include "scott.h"

static int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x80) > 0);
    *byte = *byte << 1;
    if (last_carry)
        *byte = *byte | 0x01;
    return carry;
}

static int decompress_one(uint8_t *bytes)
{
    uint8_t result = 0;
    int carry;
    for (int i = 0; i < 5; i++) {
        carry = 0;
        for (int j = 0; j < 5; j++) {
            carry = rotate_left_with_carry(bytes + 4 - j, carry);
        }
        rotate_left_with_carry(&result, carry);
    }
    return result;
}

char *DecompressText(uint8_t *source, int stringindex)
{
    // Lookup table
    const char *alphabet = " abcdefghijklmnopqrstuvwxyz'\x01,.\x00";

    int pos, c, uppercase, i, j;
    uint8_t decompressed[256];
    uint8_t buffer[5];
    int idx = 0;

    // Find the start of the compressed message
    for (i = 0; i < stringindex; i++) {
        pos = *source;
        pos = pos & 0x7F;
        source += pos;
    };

    uppercase = ((*source & 0x40) == 0); // Test bit 6

    source++;
    do {
        // Get five compressed bytes
        for (i = 0; i < 5; i++) {
            buffer[i] = *source++;
        }
        for (j = 0; j < 8; j++) {
            // Decompress one character:
            int next = decompress_one(buffer);

            c = alphabet[next];

            if (c == 0x01) {
                uppercase = 1;
                c = ' ';
            }

            if (c >= 'a' && uppercase) {
                c = toupper(c);
                uppercase = 0;
            }
            decompressed[idx++] = c;

            if (idx > 255)
                return NULL;

            if (idx == 255)
                c = 0; // We've gone too far, return

            if (c == 0) {
                char *result = malloc(idx);
                memcpy(result, decompressed, idx);
                return result;
            } else if (c == '.' || c == ',') {
                if (c == '.')
                    uppercase = 1;
                decompressed[idx++] = ' ';
            }
        }
    } while (idx < 0xff); // Chosen arbitrarily, might be too small
    return NULL;
}

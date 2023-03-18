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

char *DecompressText(uint8_t *source, int stringindex)
{
    // Lookup table
    const char *alphabet = " abcdefghijklmnopqrstuvwxyz'\x01,.\x00";

    int c, uppercase, i, j;
    uint8_t decompressed[256];
    int idx = 0;

    // Find the start of the compressed message
    for (i = 0; i < stringindex; i++) {
        source += *source & 0x7F;
    };

    uppercase = ((*source & 0x40) == 0); // Test bit 6

    source++;
    do {
        // Get eight characters squeezed into five bytes
        uint64_t fortybits = ((uint64_t)source[0] << 32) | ((uint64_t)source[1] << 24) | ((uint64_t)source[2] << 16) | ((uint64_t)source[3] << 8) | source[4];
        source += 5;
        for (j = 35; j >= 0; j -= 5) {
            // Decompress one character:
            int next = (fortybits >> j) & 0x1f;
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

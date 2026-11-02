/* c64unpack.h — helpers shared by the ScottFree and TaylorMade C64 loaders
   (terps/scott/ai_uk/c64decrunch.c and terps/taylor/c64decrunch.c).

   These are header-only because they allocate with MemAlloc, of
   common_utils, which unp64 itself does not depend on. */

#ifndef c64unpack_h
#define c64unpack_h

#include <stdint.h>
#include <stdlib.h>

#include "memory_allocation.h"
#include "unp64_interface.h"

/* Simple 16-bit additive checksum. Used together with the file length to
   identify known C64 disk and tape images. */
static inline uint16_t C64Checksum(const uint8_t *data, size_t length)
{
    uint16_t c = 0;
    for (size_t i = 0; i < length; i++)
        c += data[i];
    return c;
}

/* Run unp64 up to iterations times, peeling off one compression layer
   per pass, and passing switches (if not NULL) on pass switch_pass only.
   Each successful pass installs the unpacked buffer as the new input;
   a failed pass aborts the chain but keeps whatever was already
   unpacked. */
static inline void C64DecompressIterations(uint8_t **sf, size_t *length,
    int iterations, const char *switches, int switch_pass)
{
    for (int i = 1; i <= iterations; i++) {
        const char *pass_switches =
            (i == switch_pass && switches != NULL) ? switches : NULL;
        uint8_t *output = MemAlloc(UNP64_OUTPUT_CAPACITY);
        size_t decompressedLength = 0;
        if (!unp64(*sf, *length, output, &decompressedLength, pass_switches)) {
            free(output);
            break;
        }
        /* The freshly-unpacked output replaces the input buffer */
        free(*sf);
        *sf = output;
        *length = decompressedLength;
    }
}

#endif /* c64unpack_h */

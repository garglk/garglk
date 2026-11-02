//
//  decompressz80_internal.h
//
//  What the source files of the decompressz80 library share with each other.
//  Not for the interpreters: they include decompressz80.h.
//

#ifndef decompressz80_internal_h
#define decompressz80_internal_h

#include <stdint.h>
#include <stdio.h>

/* The interpreters hand every file they are given to DecompressZ80 and ask
   for tape blocks that may not be there, so a file that is rejected is an
   ordinary result and nothing is printed about it. Build with
   -DDECOMPRESSZ80_VERBOSE to have the reason on stderr. */
#ifdef DECOMPRESSZ80_VERBOSE
#define ZX_LOG(...) (fprintf(stderr, "decompressz80: " __VA_ARGS__), fputc('\n', stderr))
#else
#define ZX_LOG(...) ((void)0)
#endif

/* A little-endian number of 1 to 4 bytes */
static inline uint32_t ReadLE(const uint8_t *p, int bytes)
{
    uint32_t value = 0;
    for (int i = bytes - 1; i >= 0; i--)
        value = (value << 8) | p[i];
    return value;
}

#endif /* decompressz80_internal_h */

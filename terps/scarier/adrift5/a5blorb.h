/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Blorb / IFF container reader.
 *
 * A small, self-contained reader for the Blorb (IFF "FORM"/"IFRS") wrapper that
 * ADRIFT 5 games ship in.  Modelled on Bocfel's iff.cpp/blorb.cpp, but operating
 * on an in-memory byte buffer with no I/O abstraction, so it is trivial to link
 * and reuse from both the headless harness and the interpreter proper.
 *
 * This file is deliberately kept at the "container edge": plain stdint types,
 * no dependency on the rest of the engine.
 */

#ifndef SCARIER_A5BLORB_H
#define SCARIER_A5BLORB_H

#include <stddef.h>
#include <stdint.h>

/* Build a big-endian FourCC constant, e.g. A5_FOURCC('A','D','R','I'). */
#define A5_FOURCC(a, b, c, d) \
  (((uint32_t) (unsigned char) (a) << 24) | ((uint32_t) (unsigned char) (b) << 16) \
   | ((uint32_t) (unsigned char) (c) << 8) | (uint32_t) (unsigned char) (d))

/* Standard Blorb resource usages. */
#define A5_USAGE_EXEC A5_FOURCC('E', 'x', 'e', 'c')
#define A5_USAGE_PICT A5_FOURCC('P', 'i', 'c', 't')
#define A5_USAGE_SND  A5_FOURCC('S', 'n', 'd', ' ')
#define A5_USAGE_DATA A5_FOURCC('D', 'a', 't', 'a')

/* ADRIFT executable chunk type, found at usage 'Exec', number 0. */
#define A5_CHUNK_ADRI A5_FOURCC('A', 'D', 'R', 'I')

typedef struct a5_blorb_chunk_s {
  uint32_t type;             /* chunk FourCC, e.g. A5_CHUNK_ADRI            */
  const uint8_t *data;       /* pointer into the source buffer (chunk body) */
  uint32_t size;             /* chunk body size, in bytes                   */
} a5_blorb_chunk_t;

/*
 * Locate a resource by usage FourCC and number.  On success returns nonzero
 * and fills *out with the chunk type and a pointer/size for its body.  The
 * pointer aliases into buf, so it stays valid as long as buf does.
 */
extern int a5blorb_find (const uint8_t *buf, uint32_t length,
                         uint32_t usage, uint32_t number,
                         a5_blorb_chunk_t *out);

/* Convenience wrapper for the ADRIFT Exec resource (usage 'Exec', number 0). */
extern int a5blorb_find_exec (const uint8_t *buf, uint32_t length,
                              a5_blorb_chunk_t *out);

#endif

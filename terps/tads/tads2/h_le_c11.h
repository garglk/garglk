/*
 *   Copyright (c) 2016 Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  h_le_c11.h - hardware definitions for little-endian CPUs.

Function
  These definitions are for little-endian CPUs and suitable for all flat
  memory addressing widths (so it's suitable for both 32-bit as well as
  64-bit systems.)

  The definitions provided here will never perform unaligned memory access.

  A C11 compiler is required to use this file with TADS 2, and a C++11
  compiler is required for TADS 3.

Notes
  This file is especially useful for CPUs where unaligned memory access is
  either not allowed or is problematic (like some of the ARM CPU models). It
  should however be just as fast on x86 and x86-64 as the h_ix86.h and
  h_ix86_64.h definitions due to compiler optimizations. So if you have
  access to a C11 (and C++11) compiler, you can use this file on all
  little-endian architectures.

  Note that even though the routines below use memcpy(), virtually all
  compilers will optimize away the memcpy() and produce inlined data
  movement machine instructions.

Modified
  03/23/2016 Nikos Chantziaras  - Creation
*/

#ifndef H_LE_C11_H
#define H_LE_C11_H

#include <stdint.h>
#include <string.h>
#ifndef __cplusplus
#include <stdalign.h>
#endif

/* Round a size up to worst-case alignment boundary. */
static inline size_t osrndsz(const size_t siz)
{
    return (siz + alignof(intmax_t) - 1) & ~(alignof(intmax_t) - 1);
}

/* Round a pointer up to worst-case alignment boundary. */
static inline const void* osrndpt(const void* p)
{
    return (void*)(((uintptr_t)p + alignof(void*) - 1)
                   & ~(alignof(void*) - 1));
}

/* Read an unaligned portable unsigned 2-byte value, returning int. */
static inline int osrp2(const void* p)
{
    uint16_t tmp;
    memcpy(&tmp, p, 2);
    return tmp;
}

/* Read an unaligned portable signed 2-byte value, returning int. */
static inline int osrp2s(const void* p)
{
    int16_t tmp;
    memcpy(&tmp, p, 2);
    return tmp;
}

/* Write unsigned int to unaligned portable 2-byte value. */
static inline void oswp2(void* p, const unsigned i)
{
    const uint16_t tmp = i;
    memcpy(p, &tmp, 2);
}

/* Write signed int to unaligned portable 2-byte value. */
static inline void oswp2s(void* p, const int i)
{
    const int16_t tmp = i;
    memcpy(p, &tmp, 2);
}

/* Read an unaligned unsigned portable 4-byte value, returning long. */
static inline unsigned long osrp4(const void* p)
{
    uint32_t tmp;
    memcpy(&tmp, p, 4);
    return tmp;
}

/* Read an unaligned signed portable 4-byte value, returning long. */
static inline long osrp4s(const void *p)
{
    int32_t tmp;
    memcpy(&tmp, p, 4);
    return tmp;
}

/* Write an unsigned long to an unaligned portable 4-byte value. */
static inline void oswp4(void* p, const unsigned long l)
{
    const uint32_t tmp = l;
    memcpy(p, &tmp, 4);
}

/* Write a signed long to an unaligned portable 4-byte value. */
static inline void oswp4s(void* p, const long l)
{
    const int32_t tmp = l;
    memcpy(p, &tmp, 4);
}

#endif

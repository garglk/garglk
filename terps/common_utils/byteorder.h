/*
 *  byteorder.h
 *
 *  Copyright (c) 2025 Petter Sjölund
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *  Reading little- and big-endian integers from a byte buffer.
 */

#ifndef byteorder_h
#define byteorder_h

#include <stdint.h>

/* Defined here as static inline so that every caller can inline them, and
   so that no compiler has to decide which file owns the one external copy
   (MSVC and clang-cl emit none for a plain inline definition). */

static inline uint16_t READ_LE_UINT16(const void *ptr) {
    const uint8_t *b = (const uint8_t *)ptr;
    return (uint16_t)((b[1] << 8) | b[0]);
}

static inline uint32_t READ_LE_UINT32(const void *ptr) {
    const uint8_t *b = (const uint8_t *)ptr;
    return ((uint32_t)b[3] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | b[0];
}

static inline uint16_t READ_BE_UINT16(const void *ptr) {
    const uint8_t *b = (const uint8_t *)ptr;
    return (uint16_t)((b[0] << 8) | b[1]);
}

static inline uint16_t READ_LE_UINT16_AND_ADVANCE(uint8_t **ptr) {
    uint16_t value = READ_LE_UINT16(*ptr);
    (*ptr) += 2;
    return value;
}

static inline uint16_t READ_BE_UINT16_AND_ADVANCE(uint8_t **ptr) {
    uint16_t value = READ_BE_UINT16(*ptr);
    (*ptr) += 2;
    return value;
}

#endif /* byteorder_h */

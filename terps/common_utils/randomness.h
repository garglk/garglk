/*
 *  randomness.h
 *
 *  Copyright (c) 1999-2023, Andrew Plotkin
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
 *  The seedable random number generator of Glulxe, shared by many terps.
 *  terps/scarier/common_utils has a byte-identical copy: after an edit, copy
 *  this file there and run `make -f Makefile.headless vendorcheck` in scarier.
 */

#ifndef randomness_h
#define randomness_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seed 0 selects the native generator of randomness.c; any other seed the
   xoshiro128** one, whose sequence is the same on every platform. */
void set_erkyrath_random(uint32_t seed);
uint32_t erkyrath_random(void);
void erkyrath_random_get_detstate(int *usenative, uint32_t **arr, int *count);
void erkyrath_random_set_detstate(int usenative, uint32_t *arr, int count);

#ifdef __cplusplus
}
#endif

#endif /* randomness_h */

/*
 *  debugprint.h
 *
 *  Copyright (c) 2022 Petter Sjölund
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
 *  debug_print(fmt, ...) prints to stderr if DEBUG_PRINT is non-zero. Define
 *  DEBUG_PRINT to 1 before including this, or with -D, to turn it on. When off,
 *  the call still compiles, so its format string and arguments cannot go stale,
 *  and the optimizer removes it.
 */

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0
#endif

#ifndef debug_print

#include <stdio.h>

#define debug_print(...)                     \
    do {                                     \
        if (DEBUG_PRINT)                     \
            fprintf(stderr, __VA_ARGS__);    \
    } while (0)

#endif /* debug_print */

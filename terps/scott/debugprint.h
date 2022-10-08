//
//  debugprint.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-10-07.
//

#ifndef DEBUG_PRINT

#define DEBUG_PRINT 0

#endif /* DEBUG_PRINT */

#ifndef debug_print

#ifdef DEBUG

#define debug_print(fmt, ...) \
do { if (DEBUG_PRINT) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#else

#define debug_print(fmt, ...)

#endif /* DEBUG */

#endif /* debug_print */

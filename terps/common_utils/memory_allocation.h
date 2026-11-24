/*
 *  memory_allocation.h
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Allocators that call Fatal() rather than return NULL.
 */

#ifndef memory_allocation_h
#define memory_allocation_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A size of 0 gives a valid pointer to free, like any other. */
void *MemAlloc(size_t size);
void *MemCalloc(size_t size);
void *MemRealloc(void *ptr, size_t size);

/* Copies exactly len bytes of string, which must have that many, into a new
   allocation and adds a terminating zero. Unlike strndup() it does not stop at
   a zero byte. */
char *MemStrndup(const char *string, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* memory_allocation_h */

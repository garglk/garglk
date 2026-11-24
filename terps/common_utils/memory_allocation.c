/*
 *  memory_allocation.c
 *  Shares code with ScottFree, by Alan Cox, Chris Spiegel and Petter Sjölund.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  Allocators that call Fatal() rather than return NULL.
 */

#include <stdlib.h>
#include <string.h>

#include "fatal.h"

#include "memory_allocation.h"

/* malloc(0) may legitimately return NULL, and realloc(p, 0) may free p */
static size_t NonZero(size_t size)
{
    return size ? size : 1;
}

void *MemAlloc(size_t size)
{
    void *t = malloc(NonZero(size));
    if (t == NULL)
        Fatal("Out of memory");
    return t;
}

void *MemCalloc(size_t size)
{
    void *t = calloc(1, NonZero(size));
    if (t == NULL)
        Fatal("Out of memory");
    return t;
}

void *MemRealloc(void *ptr, size_t size)
{
    void *t = realloc(ptr, NonZero(size));
    if (t == NULL)
        Fatal("Out of memory");
    return t;
}

char *MemStrndup(const char *string, size_t len)
{
    char *t = MemAlloc(len + 1);
    memcpy(t, string, len);
    t[len] = 0;
    return t;
}

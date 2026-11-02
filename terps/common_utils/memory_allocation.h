//
//  memory_allocation.h
//  Spatterlight
//
//  Created by Administrator on 2025-12-20.
//

#ifndef memory_allocation_h
#define memory_allocation_h

void *MemAlloc(size_t size);
void *MemCalloc(size_t size);
void *MemRealloc(void *ptr, size_t size);
/* Copy len bytes of string into a new NUL-terminated allocation */
char *MemStrndup(const char *string, size_t len);

#endif /* memory_allocation_h */

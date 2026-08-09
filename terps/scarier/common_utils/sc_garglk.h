#ifndef SC_GARGOYLE_H
#define SC_GARGOYLE_H

#ifdef GARGLK_NEEDS_STRNDUP

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *strndup (const char *s, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* GARGLK_NEEDS_STRNDUP */

#endif

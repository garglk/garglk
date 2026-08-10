#include "sc_garglk.h"

typedef int sc_garglk_translation_unit_not_empty_;

#ifdef GARGLK_NEEDS_STRNDUP

#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n) {
    const char *end = memchr(s, '\0', n);
    size_t len = end ? (end - s) : n;

    char *copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, s, len);
    copy[len] = '\0';

    return copy;
}

#endif /* GARGLK_NEEDS_STRNDUP */

#ifndef ZTERP_IFF_H
#define ZTERP_IFF_H

#include <stdint.h>

#include "io.h"

typedef struct zterp_iff zterp_iff;

/* Translate an IFF tag into the corresponding 32-bit integer. */
#define STRID(s) ( \
    (((uint32_t)(s)[0]) << 24) | \
    (((uint32_t)(s)[1]) << 16) | \
    (((uint32_t)(s)[2]) <<  8) | \
    (((uint32_t)(s)[3]) <<  0)   \
    )

/* Reverse of above. */
#define IDSTR(n) ((char[5]){ \
    ((uint32_t)n >> 24) & 0xff, \
    ((uint32_t)n >> 16) & 0xff, \
    ((uint32_t)n >>  8) & 0xff, \
    ((uint32_t)n >>  0) & 0xff, \
    })


void zterp_iff_free(zterp_iff *);
zterp_iff *zterp_iff_parse(zterp_io *, const char [4]);
int zterp_iff_find(zterp_iff *, const char [4], uint32_t *);

#endif

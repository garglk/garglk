// vim: set ft=c:

#ifndef ZTERP_IFF_H
#define ZTERP_IFF_H

#include <stdint.h>
#include <stdbool.h>

#include "io.h"

typedef struct zterp_iff zterp_iff;

typedef char (*TypeID)[5];

uint32_t STRID(TypeID type);

void zterp_iff_free(zterp_iff *iff);
zterp_iff *zterp_iff_parse(zterp_io *io, TypeID form_type);
bool zterp_iff_find(zterp_iff *iff, TypeID tag, uint32_t *size);

#endif

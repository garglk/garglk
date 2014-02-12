#ifndef ZTERP_BLORB_H
#define ZTERP_BLORB_H

#include <stdint.h>
#include <stddef.h>

#include "io.h"

#define BLORB_PICT	0x50696374UL
#define BLORB_SND	0x536e6420UL
#define BLORB_EXEC	0x45786563UL

typedef struct zterp_blorb zterp_blorb;

typedef struct
{
  uint32_t usage;
  uint32_t number;
  uint32_t type;
  char name[5];
  uint32_t offset;
  uint32_t size;
} zterp_blorb_chunk;

zterp_blorb *zterp_blorb_parse(zterp_io *);
void zterp_blorb_free(zterp_blorb *);
const zterp_blorb_chunk *zterp_blorb_find(zterp_blorb *, uint32_t, uint32_t);

#endif

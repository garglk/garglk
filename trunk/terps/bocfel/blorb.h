#ifndef ZTERP_BLORB_H
#define ZTERP_BLORB_H

#include <stdint.h>
#include <stddef.h>

#include "io.h"

#define BLORB_PICT	0x50696374
#define BLORB_SND	0x536e6420
#define BLORB_EXEC	0x45786563

typedef struct zterp_blorb zterp_blorb;

typedef struct
{
  uint32_t usage;
  int number;
  uint32_t type;
  char name[5];
  size_t offset;
  size_t size;
} zterp_blorb_chunk;

zterp_blorb *zterp_blorb_parse(zterp_io *);
void zterp_blorb_free(zterp_blorb *);
const zterp_blorb_chunk *zterp_blorb_find(zterp_blorb *, uint32_t, int);

#endif

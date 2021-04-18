// vim: set ft=c:

#ifndef ZTERP_MT_H
#define ZTERP_MT_H

#include "iff.h"
#include "io.h"

void init_random(void);
TypeID random_write_rand(zterp_io *io, void *data);
void random_read_rand(zterp_io *io);

void zrandom(void);

#endif

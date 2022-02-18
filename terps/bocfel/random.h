// vim: set ft=cpp:

#ifndef ZTERP_MT_H
#define ZTERP_MT_H

#include "iff.h"
#include "io.h"

void init_random(bool first_run);
IFF::TypeID random_write_rand(IO &io);
void random_read_rand(IO &io);

void zrandom();

#endif

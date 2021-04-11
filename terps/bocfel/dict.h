// vim: set ft=c:

#ifndef ZTERP_DICT_H
#define ZTERP_DICT_H

#include <stdint.h>
#include <stdbool.h>

void tokenize(uint16_t text, uint16_t parse, uint16_t dictaddr, bool flag);

void ztokenise(void);
void zencode_text(void);

#endif

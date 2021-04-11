// vim: set ft=c:

#ifndef ZTERP_META_H
#define ZTERP_META_H

#include <stdint.h>
#include <stdbool.h>

const uint32_t *handle_meta_command(const uint32_t *string, uint8_t len);

#ifndef ZTERP_NO_CHEAT
bool cheat_add(char *how, bool print);
bool cheat_find_freeze(uint32_t addr, uint16_t *val);
#endif

#ifndef ZTERP_NO_WATCHPOINTS
void watch_check(uint16_t addr, unsigned long oldval, unsigned long newval);
#endif

#endif

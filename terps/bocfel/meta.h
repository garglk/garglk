#ifndef ZTERP_META_H
#define ZTERP_META_H

#include <stdint.h>
#include <stdbool.h>

const uint32_t *handle_meta_command(const uint32_t *, uint8_t);

#ifndef ZTERP_NO_CHEAT
bool cheat_add(char *, bool);
bool cheat_find_freeze(uint32_t, uint16_t *);
#endif

#ifndef ZTERP_NO_WATCHPOINTS
void watch_check(uint16_t, unsigned long, unsigned long);
#endif

#endif

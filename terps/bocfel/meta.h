// vim: set ft=cpp:

#ifndef ZTERP_META_H
#define ZTERP_META_H

#include <string>
#include <utility>

#include "iff.h"
#include "io.h"
#include "types.h"

enum class MetaResult {
    Rerequest,
    Say,
};

std::pair<MetaResult, std::string> handle_meta_command(const uint16_t *string, uint8_t len);

#ifndef ZTERP_NO_CHEAT
bool cheat_add(const std::string &how, bool print);
bool cheat_find_freeze(uint32_t addr, uint16_t &val);
#endif

#ifndef ZTERP_NO_WATCHPOINTS
void watch_check(uint16_t addr, unsigned long oldval, unsigned long newval);
#endif

IFF::TypeID meta_write_bfnt(IO &savefile);
void meta_read_bfnt(IO &io, uint32_t size);

void init_meta(bool first_run);

#endif

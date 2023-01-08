// vim: set ft=cpp:

#ifndef ZTERP_MEMORY_H
#define ZTERP_MEMORY_H

#include <string>

#include "types.h"

extern uint8_t *memory, *dynamic_memory;
extern uint32_t memory_size;

bool in_globals(uint16_t addr);
bool is_global(uint16_t addr);
std::string addrstring(uint16_t addr);

uint8_t byte(uint32_t addr);
void store_byte(uint32_t addr, uint8_t val);
uint8_t user_byte(uint16_t addr);
void user_store_byte(uint16_t addr, uint8_t v);

uint16_t word(uint32_t addr);
void store_word(uint32_t addr, uint16_t val);
uint16_t user_word(uint16_t addr);
void user_store_word(uint16_t addr, uint16_t v);

void zcopy_table();
void zscan_table();
void zloadw();
void zloadb();
void zstoreb();
void zstorew();

#endif

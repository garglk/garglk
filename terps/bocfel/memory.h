#ifndef ZTERP_MEMORY_H
#define ZTERP_MEMORY_H

#include <stdint.h>

#include "util.h"
#include "zterp.h"

extern uint8_t *memory, *dynamic_memory;
extern uint32_t memory_size;

#define BYTE(addr)		(memory[addr])
#define STORE_BYTE(addr, val)	((void)(memory[addr] = (val)))

zunused
static uint16_t WORD(uint32_t addr)
{
#ifndef ZTERP_NO_CHEAT
  uint16_t cheat_val;
  if(cheat_find_freezew(addr, &cheat_val)) return cheat_val;
#endif
  return (memory[addr] << 8) | memory[addr + 1];
}

zunused
static void STORE_WORD(uint32_t addr, uint16_t val)
{
  memory[addr + 0] = val >> 8;
  memory[addr + 1] = val & 0xff;
}

zunused
static uint8_t user_byte(uint32_t addr)
{
  ZASSERT(addr < header.static_end, "attempt to read out-of-bounds address 0x%04lx", (unsigned long)addr);

  return BYTE(addr);
}

zunused
static uint16_t user_word(uint32_t addr)
{
  ZASSERT(addr < header.static_end - 1, "attempt to read out-of-bounds address 0x%04lx", (unsigned long)addr);
  
  return WORD(addr);
}

void user_store_byte(uint32_t, uint8_t);
void user_store_word(uint32_t, uint16_t);

#endif

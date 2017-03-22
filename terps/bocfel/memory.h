#ifndef ZTERP_MEMORY_H
#define ZTERP_MEMORY_H

#include <stdint.h>

#include "util.h"
#include "meta.h"
#include "zterp.h"

/* Story files do not have access to memory beyond 64K.  If they do
 * something that would cause such access, wrap appropriately.  This is
 * the approach Frotz uses (at least for @loadw/@loadb), and is endorsed
 * by Andrew Plotkin (see http://www.intfiction.org/forum/viewtopic.php?f=38&t=2052).
 * The standard isn’t exactly clear on the issue, and this appears to be
 * the most sensible way to deal with the problem.
 */

extern uint8_t *memory, *dynamic_memory;
extern uint32_t memory_size;

int in_globals(uint16_t);
int is_global(uint16_t);
unsigned long addr_to_global(uint16_t);
const char *addrstring(uint16_t);

static inline uint8_t BYTE(uint32_t addr)
{
  return memory[addr];
}

static inline void STORE_BYTE(uint32_t addr, uint8_t val)
{
  memory[addr] = val;
}

static inline uint16_t WORD(uint32_t addr)
{
#ifndef ZTERP_NO_CHEAT
  uint16_t cheat_val;
  if(cheat_find_freeze(addr, &cheat_val)) return cheat_val;
#endif
  return (memory[addr] << 8) | memory[addr + 1];
}

static inline void STORE_WORD(uint32_t addr, uint16_t val)
{
#ifndef ZTERP_NO_WATCHPOINTS
  if(addr < header.static_start - 1) watch_check(addr, WORD(addr), val);
#endif

  memory[addr + 0] = val >> 8;
  memory[addr + 1] = val & 0xff;
}

static inline uint8_t user_byte(uint16_t addr)
{
  ZASSERT(addr < header.static_end, "attempt to read out-of-bounds address 0x%lx", (unsigned long)addr);

  return BYTE(addr);
}

static inline uint16_t user_word(uint16_t addr)
{
  ZASSERT(addr < header.static_end - 1, "attempt to read out-of-bounds address 0x%lx", (unsigned long)addr);

  return WORD(addr);
}

void user_store_byte(uint16_t, uint8_t);
void user_store_word(uint16_t, uint16_t);

void zcopy_table(void);
void zscan_table(void);
void zloadw(void);
void zloadb(void);
void zstoreb(void);
void zstorew(void);

#endif

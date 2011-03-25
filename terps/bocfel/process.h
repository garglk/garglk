#ifndef ZTERP_PROCESS_h
#define ZTERP_PROCESS_H

#include <stdint.h>

extern uint16_t zargs[];
extern int znargs;

void break_from(long);
long interrupt_level(void);

void setup_opcodes(void);
void process_instructions(void);

#endif

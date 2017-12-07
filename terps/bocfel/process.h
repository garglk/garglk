#ifndef ZTERP_PROCESS_H
#define ZTERP_PROCESS_H

#include <stdint.h>
#include <stdbool.h>

extern unsigned long pc;
extern unsigned long current_instruction;

extern uint16_t zargs[];
extern int znargs;

bool in_interrupt(void);
void interrupt_return(void);
void interrupt_reset(void);
void interrupt_quit(void);

void setup_opcodes(void);
void process_instructions(void);

#endif

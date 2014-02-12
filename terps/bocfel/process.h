#ifndef ZTERP_PROCESS_H
#define ZTERP_PROCESS_H

#include <stdint.h>

extern uint16_t zargs[];
extern int znargs;

int in_interrupt(void);
void interrupt_return(void);
void interrupt_reset(void);
void interrupt_quit(void);

void setup_opcodes(void);
void process_instructions(void);

#endif

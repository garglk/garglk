// vim: set ft=c:

#ifndef ZTERP_PROCESS_H
#define ZTERP_PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#include "stack.h"

extern unsigned long pc;
extern unsigned long current_instruction;

extern uint16_t zargs[];
extern int znargs;

enum IntType {
    IntTypeReturn = 1,
    IntTypeContinue = 2,
    IntTypeContinueRead = 3,
    IntTypeContinueReadChar = 4,
};

bool in_interrupt(void);
void interrupt_return(void);
void interrupt_restore(enum SaveOpcode saveopcode);
void interrupt_restart(void);
void interrupt_quit(void);

void setup_opcodes(void);
void process_instructions(void);

#endif

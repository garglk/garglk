#ifndef ZTERP_PROCESS_h
#define ZTERP_PROCESS_H

#include <stdint.h>
#include <signal.h>

extern uint16_t zargs[];
extern int znargs;

extern volatile sig_atomic_t running;

void setup_opcodes(void);
void process_instructions(int);

#endif

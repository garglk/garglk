// vim: set ft=cpp:

#ifndef ZTERP_PROCESS_H
#define ZTERP_PROCESS_H

#include <array>
#include <exception>

#include "stack.h"
#include "types.h"

namespace Operation {
// Jump back to the previous round of interpreting. This is used
// when an interrupt routine returns.
class Return : std::exception {
};

// Jump back to the first round of processing and continue; this is
// used for @restart, which is assumed to have put everything back
// in a clean state.
class Restart : std::exception {
};

// Jump back to the first round of processing, but tell it to
// immediately stop. This is used to implement @quit.
class Quit : std::exception {
};

// Jump back to the first round of processing, optionally calling
// zread() or zread_char(). This is used after a successful restore.
struct Restore : std::exception {
    explicit Restore(SaveOpcode saveopcode_) : saveopcode(saveopcode_) {
    }

    const SaveOpcode saveopcode;
};
}

extern unsigned long pc;
extern unsigned long current_instruction;

extern std::array<uint16_t, 8> zargs;
extern int znargs;

bool in_interrupt();
void setup_opcodes();
void process_instructions();
void process_loop();

#endif

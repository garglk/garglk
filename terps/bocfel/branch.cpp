// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include "branch.h"
#include "memory.h"
#include "process.h"
#include "stack.h"
#include "types.h"
#include "util.h"

void branch_if(bool do_branch)
{
    uint8_t branch;
    uint16_t offset;

    branch = byte(pc++);

    if (!do_branch) {
        branch ^= 0x80;
    }

    offset = branch & 0x3f;

    if ((branch & 0x40) == 0) {
        offset = (offset << 8) | byte(pc++);

        // Get the sign right.
        if ((offset & 0x2000) == 0x2000) {
            offset |= 0xc000;
        }
    }

    if ((branch & 0x80) == 0x80) {
        if (offset > 1) {
            pc += as_signed(offset) - 2;
            ZASSERT(pc < memory_size, "branch to invalid address 0x%lx", static_cast<unsigned long>(pc));
        } else {
            do_return(offset);
        }
    }
}

void zjump()
{
    // -= 2 because pc has been advanced past the jump instruction.
    pc += as_signed(zargs[0]);
    pc -= 2;

    ZASSERT(pc < memory_size, "@jump to invalid address 0x%lx", static_cast<unsigned long>(pc));
}

void zjz()
{
    branch_if(zargs[0] == 0);
}

void zje()
{
    if (znargs == 1) {
        branch_if(false);
    } else if (znargs == 2) {
        branch_if(zargs[0] == zargs[1]);
    } else if (znargs == 3) {
        branch_if(zargs[0] == zargs[1] || zargs[0] == zargs[2]);
    } else {
        branch_if(zargs[0] == zargs[1] || zargs[0] == zargs[2] || zargs[0] == zargs[3]);
    }
}

void zjl()
{
    branch_if(as_signed(zargs[0]) < as_signed(zargs[1]));
}

void zjg()
{
    branch_if(as_signed(zargs[0]) > as_signed(zargs[1]));
}

// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include "mathop.h"
#include "branch.h"
#include "process.h"
#include "stack.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

void zinc()
{
    store_variable(zargs[0], variable(zargs[0]) + 1);
}

void zdec()
{
    store_variable(zargs[0], variable(zargs[0]) - 1);
}

void znot()
{
    store(~zargs[0]);
}

void zdec_chk()
{
    int16_t newval;
    int16_t val = as_signed(zargs[1]);

    zdec();

    // The z-spec 1.1 requires indirect variable references to the stack not to push/pop
    if (zargs[0] == 0) {
        newval = as_signed(*stack_top_element());
    } else {
        newval = as_signed(variable(zargs[0]));
    }

    branch_if(newval < val);
}

void zinc_chk()
{
    int16_t newval;
    int16_t val = as_signed(zargs[1]);

    zinc();

    // The z-spec 1.1 requires indirect variable references to the stack not to push/pop
    if (zargs[0] == 0) {
        newval = as_signed(*stack_top_element());
    } else {
        newval = as_signed(variable(zargs[0]));
    }

    branch_if(newval > val);
}

void ztest()
{
    branch_if((zargs[0] & zargs[1]) == zargs[1]);
}

void zor()
{
    store(zargs[0] | zargs[1]);
}

void zand()
{
    store(zargs[0] & zargs[1]);
}

void zadd()
{
    store(zargs[0] + zargs[1]);
}

void zsub()
{
    store(zargs[0] - zargs[1]);
}

void zmul()
{
    store(static_cast<uint32_t>(zargs[0]) * static_cast<uint32_t>(zargs[1]));
}

void zdiv()
{
    ZASSERT(zargs[1] != 0, "divide by zero");
    store(as_signed(zargs[0]) / as_signed(zargs[1]));
}

void zmod()
{
    ZASSERT(zargs[1] != 0, "divide by zero");
    store(as_signed(zargs[0]) % as_signed(zargs[1]));
}

void zlog_shift()
{
    int16_t places = as_signed(zargs[1]);

    // Shifting more than 15 bits is undefined (as of Standard 1.1), but
    // do the most sensible thing.
    if (places < -15 || places > 15) {
        store(0);
        return;
    }

    if (places < 0) {
        store(zargs[0] >> -places);
    } else {
        store(zargs[0] << places);
    }
}

void zart_shift()
{
    int16_t number = as_signed(zargs[0]), places = as_signed(zargs[1]);

    // Shifting more than 15 bits is undefined (as of Standard 1.1), but
    // do the most sensible thing.
    if (places < -15 || places > 15) {
        store(number < 0 ? -1 : 0);
        return;
    }

    // Shifting a negative value in C++ has some consequences:
    // • Shifting a negative value left is undefined.
    // • Shifting a negative value right is implementation defined.
    //
    // Thus these are done by hand. The Z-machine requires a right-shift
    // of a negative value to propagate the sign bit. This is easily
    // accomplished by complementing the value (yielding a positive
    // number), shifting it right (zero filling), and complementing again
    // (flip the shifted-in zeroes to ones).
    //
    // For a left-shift, the result should presumably be the same as a
    // logical shift, so do that.
    if (number < 0) {
        if (places < 0) {
            store(~(~number >> -places));
        } else {
            store(zargs[0] << places);
        }
    } else {
        if (places < 0) {
            store(zargs[0] >> -places);
        } else {
            store(zargs[0] << places);
        }
    }
}

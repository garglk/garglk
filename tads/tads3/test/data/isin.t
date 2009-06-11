#include "tads.h"
#include "t3.h"

preinit() { }

f1(x)
{
    "This is f1(<<x>>)\n";
    return x;

}

main(args)
{
    local x;

    "is in:\b";
    
    /* is in - short-circuit */
    x = 3;
    if (x is in (f1(1), f1(2), f1(3), f1(4), f1(5)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* is in - not found */
    if (x is in (f1(10), f1(11), f1(12), f1(13), f1(14)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* is in - found as constant in middle */
    if (3 is in (f1(1), f1(2), 3, f1(4)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* found as constant in first position */
    if (3 is in (3, f1(1), f1(2), f1(4)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* found as constant in second position */
    if (3 is in (f1(1), 3, f1(4), f1(5)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* found as constant in last */
    if (3 is in (f1(1), f1(2), 3))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* found as constant in second-to-last position */
    if (3 is in (f1(1), f1(2), f1(4), 3, f1(5)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* found as constant among other constants */
    if (3 is in (f1(1), 2, 3, 4, f1(5)))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* found, all constants */
    if (3 is in (1, 2, 3, 4, 5))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* not found, all constants */
    if (3 is in (1, 2, 4, 5))
        "Found it!\n";
    else
        "Didn't find it!\n";

    /* -------------------------------------------------------------------- */
    "\bnot in:\b";

    /* not in - short-circuit */
    x = 3;
    if (x not in (f1(1), f1(2), f1(3), f1(4), f1(5)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* not in - not found */
    if (x not in (f1(10), f1(11), f1(12), f1(13), f1(14)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* not in - found as constant in middle */
    if (3 not in (f1(1), f1(2), 3, f1(4)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* found as constant in first position */
    if (3 not in (3, f1(1), f1(2), f1(4)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* found as constant in second position */
    if (3 not in (f1(1), 3, f1(4), f1(5)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* found as constant in last */
    if (3 not in (f1(1), f1(2), 3))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* found as constant in second-to-last position */
    if (3 not in (f1(1), f1(2), f1(4), 3, f1(5)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* found as constant among other constants */
    if (3 not in (f1(1), 2, 3, 4, f1(5)))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* found, all constants */
    if (3 not in (1, 2, 3, 4, 5))
        "Didn't find it!\n";
    else
        "Found it!\n";

    /* not found, all constants */
    if (3 not in (1, 2, 4, 5))
        "Didn't find it!\n";
    else
        "Found it!\n";
}

/*
 *   This file just has a bunch of unclosed '<< >>' blocks for exercising
 *   the compiler's error recovery 
 */

#include "tads.h"
#include "t3.h"
#include "bignum.h"

main(args)
{
    local i;
    local j;

    "Here's an expression: <<i.formatString(5)> \n";

    "Here's one where we continue on
    for a few lines afterwards: <<i.formatString(5))
    So, clearly we forgot the string, but
    now we continue on for a few lines.";

    "Here's multiple missing close brackets:
    <<i.formatString(5)><<j.logE(10)>>\n";

    "Here's one where we close the quote immediately: <<i.logE(7)>>";
}


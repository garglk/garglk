/*
 *   random number generator - performance test
 */

#include "tads.h"
#include "t3.h"

class Throwable: object
    // basic exception class
    display = "basic exception object";
;

class RuntimeError: Throwable
    construct(errno, ...) { errno_ = errno; }
    errno_ = 0
    display = "RuntimeError: error = <<errno_>>"
;

_say_embed(str) { tadsSay(str); }

_main(args)
{
    t3SetSay(_say_embed);
    try
    {
        main();
    }
    catch (Throwable th)
    {
        "\n\n*** Unhandled exception: << th.display >>\n";
    }
}

main()
{
    local i;
    local buckets = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    local start;

    randomize();

    "Generating random numbers from 1 to 10...\n";
    start = getTime(GetTimeTicks);
    for (i = 0 ; i < 1000000 ; ++i)
    {
        local val;

        val = rand(10) + 1;
        ++buckets[val];
    }
    "Elapsed time: <<getTime(GetTimeTicks) - start>>\n";

    "Number of values at each integer:\n";
    for (i = 1 ; i <= 10 ; ++i)
        " <<i>>: <<buckets[i]>>\n";
}


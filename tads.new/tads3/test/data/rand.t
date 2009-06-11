/*
 *   random number generator test
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

    randomize();

    "Generating random numbers from 1 to 10...\n";
    for (i = 0 ; i < 10000 ; ++i)
    {
        local val;

        val = rand(10) + 1;
        ++buckets[val];
    }

    i = i * 3;
    "Number of values at each integer:\n";
    for (i = 1 ; i <= 10 ; ++i)
        " <<i>>: <<buckets[i]>>\n";
    "\n";

    local num_names = ['one', 'two', 'three', 'four', 'five',
                       'six', 'seven', 'eight', 'nine', 'ten'];
    "Picking random list elements\n";
    for (i = 0 ; i < 100 ; ++i)
        "<<rand(num_names)>>, ";
    "\n\n";

    "Picking random arguments\n";
    for (i = 0 ; i < 100 ; ++i)
        "<<rand('a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
                'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z')>>, ";
    "\n\n";

    "Pick some 0/1 values...\n";
    buckets = [0, 0];
    for (i = 0 ; i < 1000 ; ++i)
    {
        local val;

        val = rand(2);
        "<<val>>";
        buckets[val+1]++;
    }
    "\n0's: <<buckets[1]>>; 1's: <<buckets[2]>>\n\n";

#if 0
    "Pick some 0/1 values from middle bits...\n";
    buckets = [0, 0];
    for (i = 0 ; i < 1000 ; ++i)
    {
        local val;

        val = (rand(128) >> 6) & 1;
        "<<val>>";
        buckets[val+1]++;
    }
    "\n0's: <<buckets[1]>>; 1's: <<buckets[2]>>\n\n";

    "Pick some using sequence shortening\n";
    buckets = [0, 0];
    for (i = 0, local lastval = 0, local seqlen = 0 ; i < 1000 ; ++i)
    {
        local val;

        val = rand(2);
        if (val == lastval)
        {
            ++seqlen;
            if (rand(100) < seqlen * 15)
            {
                val = 1 - lastval;
                seqlen = 0;
            }
        }
        else
        {
            seqlen = 0;
        }
        lastval = val;
        "<<val>>";
        buckets[val+1]++;
    }
    "\n0's: <<buckets[1]>>; 1's: <<buckets[2]>>\n\n";
#endif
}


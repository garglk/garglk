/*
 *   random number generator - performance test
 */

#include "tads.h"
#include "t3.h"
#include "vector.h"

preinit()
{
}

main(args)
{
    local i;
    local buckets;
    local start;

    buckets = new Vector(10);
    buckets.fillValue(0, 1, 10);
    
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


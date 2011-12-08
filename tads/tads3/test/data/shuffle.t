/*
 *   Basic list shuffler algorithm 
 */

#include <tads.h>

main(args)
{
    local n = 20;
    local vec = Vector.generate({i: i}, n);
    for (local i in n..2 step -1)
    {
        local pick = rand(i) + 1;
        local tmp = vec[i];
        vec[i] = vec[pick];
        vec[pick] = tmp;
    }

    for (local i in 1..n)
        "<<vec[i]>><<if i < n>>, ";
    "\n";
}

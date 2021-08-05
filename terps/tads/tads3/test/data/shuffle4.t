/*
 *   Test the <<one of>>...<<shuffle>> algorithm.  In particular, this
 *   tests for repeats at re-shuffle boundaries. 
 */

#include <tads.h>
#include <bignum.h>


main(args)
{
    local n = args.length() > 1 ? toInteger(args[2]) : 10, trials = 5000;

    local lst = new OneOfIndexGen();
    lst.numItems = n;
    lst.listAttrs = 'shuffle';

    for (local i in 1..trials, local last = nil)
    {
        local pick = lst.getNextIndex();
        "<<pick>><<if pick == last>> *** REPEAT!!! ***<<end>><<
          if i < trials>>, ";

        last = pick;
    }
}

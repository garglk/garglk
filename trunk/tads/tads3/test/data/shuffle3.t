/*
 *   Test rand-wt (weighted random - 'as decreasingly likely outcomes') by
 *   make a histogram of values and making sure that the statistics come
 *   out the way they should.  The defined weighting is that the last item
 *   in the list has relative weight 1, the second to last has weight 2,
 *   the third to last has weight 3, ..., the first item in the last has
 *   weight N.
 */

#include <tads.h>
#include <bignum.h>


main(args)
{
    local n = args.length() > 1 ? toInteger(args[2]) : 5, trials = 5000;

    local lst = new OneOfIndexGen();
    lst.numItems = n;
    lst.listAttrs = 'rand-wt';

    local hist = new Vector(n).fillValue(0, 1, n);
    for (local i in 1..trials)
        hist[lst.getNextIndex()]++;

    local sum = n*(n+1)/2;
    for (local i in 1..n)
    {
        local pct = new BigNumber(hist[i])/trials;
        "<<i>> = <<sprintf('%.2f%% (%.1f/%d)', pct*100, pct*sum, sum)>>\n";
    }
}

#include <tads.h>
#include <bignum.h>

/*
 *   Test of random number generator distribution for percentage tests.
 *   We'll generate a set of values in the range 0 to 99, and check to see
 *   what percentage fall under the given percent value.  
 */
main(args)
{
    local pct;
    local numReport;
    local numTotal;
    local numUnder;

    /* check for the percentage argument */
    if (args.length() != 2
        || (pct = toInteger(args[2])) < 1 || pct > 100)
    {
        "usage: t3run randpct &lt;pct&gt;\n
        'pct' is a number from 1 to 100; we'll generate a set of
        random values from 0 to 99, and determine how many are less than
        the 'pct'.  A good uniform random number generator will
        average out over a large number of trials so that 'pct' percent
        of the values are less than 'pct'.\n";
        return;
    }

    /* zero the counters */
    numTotal = numUnder = 0;

    /* seed the random number generator */
    randomize();

    /* run our tests, reporting at several points */
    foreach (numReport in [100, 1000, 10000, 50000])
    {
        /* run tests until we get to the next reporting point */
        for ( ; numTotal < numReport ; ++numTotal)
        {
            /* 
             *   generate a random number in the range 0 to 99, and note if
             *   it's less than 'pct' 
             */
            if (rand(100) < pct)
                ++numUnder;
        }

        /* report the distribution so far */
        "Total samples = <<numTotal>>; samples &lt; <<pct>> = <<numUnder>>
        (<<(100.0 * numUnder / numTotal).formatString(5, 0, 3, 2)>>%)\n";
    }
}


#include <tads.h>

main(args)
{
    if (args.length < 2)
    {
        "usage: test &lt;n&gt;, where n is the number of list elements
        to build\n";
        return;
    }
    local limit = toInteger(args[2]);
    local tot = 0;
    for (local i = 1 ; i < limit ; ++i)
        tot += stringOf(i).length();
    "Number of elements: <<limit>>
    \nTotal combined string length: <<tot>>
    \nTotal with TADS 2 list overhead: <<tot + 3*limit + 2>>\n";
}

stringOf(i)
{
    local rv = '';
    while (--i > 0)
        rv += toString(i);
    return rv;
}

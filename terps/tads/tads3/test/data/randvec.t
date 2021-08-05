#include <tads.h>

main(args)
{
    local i = 65;
    local v = new Vector(26).fillValue(nil, 1, 26)
        .applyAll({x: x = makeString(i++)});

    for (i = 1 ; i <= 50 ; ++i)
        "<<rand(v)>> ";

    randomize();
    "\b";

    for (i = 1 ; i <= 500 ; ++i)
        "<<rand(v)>> ";
}


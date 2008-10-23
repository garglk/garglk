#include "tads.h"
#include "t3.h"

main(args)
{
    local a, b;

    a = [1, 2, 3];
    b = [];
    sayList(a - b);
}

sayList(lst)
{
    "[";
    for (local i = 1, local cnt = lst.length() ; i <= cnt ; ++i)
    {
        if (i != 1)
            ", ";
        tadsSay(lst[i]);
    }
    "]";
}


#include <tads.h>

main(args)
{
    local i;
    
    "Entering an infinite loop...\n";
    for (i = 1 ; ; ++i)
    {
        f(i);
    }
}

f(x)
{
    return x;
}


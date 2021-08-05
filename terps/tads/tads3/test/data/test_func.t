#include <tads.h>

main(args)
{
    local f = g;

    f(1);
}

f(x)
{
    "This is f(<<x>>)\n";
}

g(x)
{
    "This is g(<<x>>)\n";
}

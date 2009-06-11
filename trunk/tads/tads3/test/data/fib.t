#include <tads.h>

main(args)
{
    local t0 = getTime(GetTimeTicks);
    local n = (args.length() >= 2 ? toInteger(args[2]) : 10);
    
    "fib(<<n>>) = <<fib(n)>>\n";
    "elapsed time = <<getTime(GetTimeTicks) - t0>> ms\n";
}

fib(n)
{
    if (n <= 2)
        return 1;
    else
        return fib(n-2) + fib(n-1);
}


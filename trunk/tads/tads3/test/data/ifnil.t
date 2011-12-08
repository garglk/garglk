#include <tads.h>

main(args)
{
    local a = 123;
    local b = nil;
    local c = 456;
    
    "Constant tests:\n";
    "nil ?? 100 = << nil ?? 100 >>\n";
    "nil ?? a = <<nil ?? a>>\n";
    "200 ?? 300 = <<200 ?? 300>>\n";
    "400 ?? a = <<400 ?? a>>\n";

    "\b";
    "Non-constant tests:\n";
    "a ?? 500 = <<a ?? 500>>\n";
    "a ?? c = <<a ?? c>>\n";
    "b ?? 600 = <<b ?? 600>>\n";
    "b ?? c = <<b ?? c>>\n";

    "\b";
    "Side effect tests:\n";
    "f(a) ?? f(700) = <<f(a) ?? f(700)>>\n";
    "f(b) ?? f(800) = <<f(b) ?? f(800)>>\n";
    "nil ?? f(900) = <<nil ?? f(900)>>\n";
    "1000 ?? f(1100) = <<1000 ?? f(1100)>>\n";
}

f(x)
{
    " [This is f(<<x>>)] ";
    return x;
}

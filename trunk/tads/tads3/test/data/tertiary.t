#include <tads.h>

f(x, y) { "f(<<x>>)\n"; return y; }

main(args)
{
    local a;
    
    a = f(1, nil) ? f(2, true) ? f(3, true) : f(4, true) : f(5, true);
    "\b";
    a = f(1, true) ? f(2, true) ? f(3, true) : f(4, true) : f(5, true);
    "\b";
    a = f(1, true) ? f(2, true) : f(3, true) ? f(4, true) : f(5, true);
    "\b";
    a = f(1, nil) ? f(2, true) : f(3, true) ? f(4, true) : f(5, true);
}

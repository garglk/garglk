/*
 *   test of throwing an exception through a native code callback 
 */

#include "tads.h"
#include "t3.h"

preinit()
{
}

main(args)
{
    local lst;
    
    try
    {
        lst = [1, 2, 3, 4, 5];
        lst = lst.mapAll(&f1);
    }
    catch (Exception exc)
    {
        "Back in main: caught exception: <<exc.displayException()>>\n";
    }

    try
    {
        lst = [5, 4, 3, 2, 1];
        lst = lst.mapAll(&f1);
    }
    catch (Exception exc)
    {
        "Back in main: caught exception: <<exc.displayException()>>\n";
        throw exc;
    }
}

class F1Exception: Exception
    displayException() { "f1: x = <<x_>>"; }
    construct(x) { x_ = x; }
    x_ = nil
;

f1(x)
{
    "f1(<<x>>)\n";
    if (x == 3)
        throw new F1Exception(x);
}


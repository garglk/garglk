#include "tads.h"
#include "t3.h"

myObject: object
    prop1 = "this is prop1"
    prop2 = "this is prop2"
;

main(args)
{
    "calling prop1: <<callprop(&prop1)>>\n";
    "calling prop2: <<callprop(&prop2)>>\n";
    "calling prop3: <<callprop(&prop3)>>\n";
}

callprop(prop)
{
    myObject.(prop)();
}


#include "tads.h"

/* define some properties - do two in one list to check comma handling */
property prop3, prop4;

/* do one in a list by itself to check that syntax as well */
property prop5;

obj1: object
    prop1 = 'this is prop1'

    test()
    {
        "calling prop1: <<prop1>>\n";
        "calling prop2: <<prop2>>\n";
        "calling prop3: <<prop3>>\n";
    }
;

main(args)
{
    obj1.test();
    obj1.prop6();
}

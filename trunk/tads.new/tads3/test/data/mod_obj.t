#include "tads.h"

corePrint(val, obj)
{
    tadsSay('core: val=<');
    tadsSay(val);
    tadsSay('>\n');
}

modify Object
    print(x)
    {
        corePrint(x, self);
    }
;

modify Collection
    print(x)
    {
        tadsSay('collection: val=<');
        tadsSay(x);
        tadsSay('>self=[');
        local first = true;
        foreach (local obj in self)
        {
            if (!first) tadsSay(',');
            first = nil;
            tadsSay(obj);
        }
        tadsSay(']\n');
    }
;

myObj: object
    foo = nil
;

main(args)
{
    myObj.print('hello');
    [1, 2, 3].print('bye');
}


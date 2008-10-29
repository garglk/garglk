#include <tads.h>

modify TadsObject
{
    myMethod()
    {
        "This is TadsObject.myMethod - inheriting...\n";
        inherited();
        "... back from inheriting in TadsObject.myMethod\n";
    }
}

modify TadsObject
{
    myMethod()
    {
        "This is the MODIFIED TadsObject.myMethod - inheriting...\n";
        inherited();
        "... back from inheriting in MODIFIED TadsObject.myMethod\n";
    }
}

modify Object
{
    myMethod() { "This is Object.myMethod\n"; }
}

modify Object
{
    myMethod()
    {
        "This is the MODIFIED Object.myMethod - inheriting...\n";
        inherited();
        "... back from inheriting in MODIFIED Object.myMethod\n";
    }
}

obj1: object
    desc = "This is obj1"
    myMethod()
    {
        "This is obj1.myMethod - inheriting...\n";
        inherited();
        "... back from inheriting in obj1.myMethod\n";
    }
;

main(args)
{
    "obj1.desc: <<obj1.desc>>\n";
    "calling obj1.myMethod...\n";
    obj1.myMethod();
    "back from calling obj1.myMethod\n";
}

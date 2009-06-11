/*
 *   test of TadsObject.createClone() 
 */

#include "tads.h"

class MyClass: object
    name = 'my class'
    desc = "This is my class"
    weight = 1
    bulk = 1
;

myobj: MyClass
    name = 'my obj'
    bulk = 5
    sayName() { tadsSay(name); }
;
    
otherobj: MyClass
    foo = 7
;

main(args)
{
    local obj;

    myobj.weight = 10;
    myobj.bulk = 20;
    myobj.foo = 30;

    obj = myobj.createClone();

    foreach (local prop in obj.getPropList())
    {
        "<<reflectionServices.valToSymbol(prop)>>: type code =
         <<obj.propType(prop)>>";

        switch(obj.propType(prop))
        {
        case TypeCode:
            " &lt;method>";
            break;

        case TypeNativeCode:
            " &lt;native code>";
            break;

        default:
            ", val = <<reflectionServices.valToSymbol(obj.(prop))>>";
            break;
        }

        "\n";
    }

    "\b";
    "obj.sayName: <<obj.sayName>>\n";
}

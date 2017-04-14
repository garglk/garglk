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

class PropDesc: object
    construct(p)
    {
        prop = p;
        name = reflectionServices.valToSymbol(p);
    }

    prop = nil
    name = nil
;

main(args)
{
    local obj;

    myobj.weight = 10;
    myobj.bulk = 20;
    myobj.foo = 30;

    obj = myobj.createClone();

    foreach (local desc in obj.getPropList().mapAll({p: new PropDesc(p)}).
             sort(SortAsc, {a, b: a.name.compareIgnoreCase(b.name)}))
     {
        "<<desc.name>>: type code = <<obj.propType(desc.prop)>>";

        switch(obj.propType(desc.prop))
        {
        case TypeCode:
            " &lt;method>";
            break;

        case TypeNativeCode:
            " &lt;native code>";
            break;

        default:
            ", val = <<reflectionServices.valToSymbol(obj.(desc.prop))>>";
            break;
        }

        "\n";
    }

    "\b";
    "obj.sayName: <<obj.sayName>>\n";
}

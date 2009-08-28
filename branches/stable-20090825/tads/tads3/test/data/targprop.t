#include <tads.h>

class myClass: object
    prop1()
    {
        "This is myClass.prop1 - targetprop = <<
        reflectionServices.valToSymbol(targetprop)>>\n";
    }
    prop2()
    {
        "This is myClass.prop2 - targetprop = <<
        reflectionServices.valToSymbol(targetprop)>>\n";
    }
;

myObj: myClass
    prop1()
    {
        "This is myObj.prop1 - targetprop = <<
        reflectionServices.valToSymbol(targetprop)>>\n";

        "...inheriting...\n";
        inherited.prop1();

        "...inheriting implicitly...\n";
        inherited();

        "...again without arguments...\n";
        inherited;
    }
;

main(args)
{
    myObj.prop1();
    myObj.prop2();
}

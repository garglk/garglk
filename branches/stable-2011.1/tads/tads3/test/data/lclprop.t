#include "tads.h"

obj1: object
    prop1 = 100
    prop2 = 200
    prop3 = 300
    m1()
    {
        local prop2 = '--lcl--';

        "This is m1.  prop1 = <<prop1>>, prop2 = <<prop2>>,
        prop3 = <<prop3>>\n";

        "self.prop1 = <<self.prop1>>, self.prop2 = <<self.prop2>>,
        self.prop3 = <<self.prop3>>\n";

        prop2 = '--new/lcl--';
        self.prop2 = 400;
        "prop2 = <<prop2>>, self.prop2 = <<self.prop2>>\n";

        local prop1 = &prop3;
        "self.prop1 = <<self.prop1>>, self.(prop1) = <<self.(prop1)>>\n";
    }
;

main(args)
{
    obj1.m1();
}


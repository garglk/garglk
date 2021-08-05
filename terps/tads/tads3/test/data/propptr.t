#include <tads.h>

obj1: object
    p1 = nil

    p2()
    {
        local a = &p1;
        self.(a) = 11;
        "self.(a)=11: <<self.(a)>>\n";

        self.(a) += 6;
        "self.(a)+=6: <<self.(a)>>\n";
    }
;

main(args)
{
    local a = obj1;
    local b = &p1;

    obj1.p1 = 2;
    "obj1.p1=2: <<obj1.p1>>\n";

    obj1.p1 += 13;
    "obj1.p1+=13: <<obj1.p1>>\n";

    a.p1 = 6;
    "a.p1=6: <<a.p1>>\n";

    a.p1 += 17;
    "a.p1+=17: <<a.p1>>\n";

    obj1.(b) = 4;
    "obj1.(b)=4: <<obj1.(b)>>\n";
    
    obj1.(b) += 5;
    "obj1.(b)+=5: <<obj1.(b)>>\n";
    
    a.(b) = 7;
    "a.(b)=7: <<a.(b)>>\n";

    a.(b) += 3;
    "a.(b)+=3: <<a.(b)>>\n";

    obj1.p2();
}

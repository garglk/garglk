#include <tads.h>

class A: object
    construct(a)
    {
        "A.construct: a = <<a>>\n";
        propA = a;
    }
    propA = 'A.default'
;

class B: object
    construct(b)
    {
        "B.construct: b = <<b>>\n";
        propB = b;
    }
    propB = 'B.default'
;

class C: object
    construct(c, d)
    {
        "C.construct: c = <<c>>, d = <<d>>\n";
        propC = c;
        propD = d;
    }
    propC = 'C.default'
    propD = 'D.default'
;

main(args)
{
    local x = TadsObject.createInstanceOf([A, 'a1'], B, [C, 'c3', 'd4']);
    "created x: A,B,C:\n
    x.propA = <<x.propA>>\n
    x.propB = <<x.propB>>\n
    x.propC = <<x.propC>>\n
    x.propD = <<x.propD>>\n";
}

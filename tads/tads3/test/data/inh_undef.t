#include <tads.h>

property propNotDefined;
export propNotDefined;

main(args)
{
    local x = new C(new B());

    "x.p1...\n";
    x.p1(100);

    "\bx.p2\n";
    x.p2(200);
}

class A: object
    p1(x) { "This is A.p1(<<x>>)\n"; }
    p2(x) { "This is A.p2(<<x>>)\n"; }
;

class B: A
    p1(x) { "This is B.p1(<<x>>) - inheriting...\n"; inherited(x+1); }
;

class C: object
    construct(b) { baseobj = b; }
    baseobj = nil
    
    propNotDefined(prop, [args])
    {
        "This is C.propNotDefined - delegating to base object...\n";
        return baseobj.(prop)(args...);
    }
;

modify C
    p1(x) { "This is mod-C.p1(<<x>>) - inheriting...\n"; inherited(x+1); }

    propNotDefined(prop, [args])
    {
        "This is mod-C.propNotDefined - inheriting...\n";
        return inherited(prop, args...);
    }
;


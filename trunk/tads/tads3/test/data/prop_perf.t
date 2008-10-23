/*
 *   performance timing test for property getter 
 */
#include <tads.h>

class A: object
    prop1 = 'this is A.prop1'
;

class B: A
;

class C: B
    prop1()
    {
        return inherited();
    }
;

class D: B
    prop1()
    {
        return inherited();
    }
;

class E: C, D
    prop1()
    {
        return inherited();
    }
;

class F: E
;

main(args)
{
    local x;
    local startTime = getTime(GetTimeTicks);
    
    for (local i = 1 ; i < 100000 ; ++i)
        x = F.prop1();

    "done - x = <<x>>,
    time = <<getTime(GetTimeTicks) - startTime>> milliseconds\n";
}


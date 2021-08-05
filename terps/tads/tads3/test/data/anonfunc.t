#charset "us-ascii"

#include <tads.h>

main(args)
{
    local i = 100;

    local f = { x: x + 1 };
    "[main 1] type(f) = <<funcType(f)>>, f(7) = <<f(7)>>\n"; // fn 8
    
    f = { x: x + i + 1 };
    "[main 2] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 108
    
    f = { x: x + 2 };
    "[main 3] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // fn 9
    
    obj.m(50);
}

funcType(f)
{
    f = dataType(f);
    return (f == TypeFuncPtr ? 'fn' : f == TypeObject ? 'obj' : 'other');
}

class BaseClass: object
    m(x) { return x + 1000; }
;

obj: BaseClass
    m(x)
    {
        local i = 200;
        local f;

        f = { x: x + 1 };
        "[obj.m 1] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // fn 8
        
        f = { x: x + i + 1 };
        "[obj.m 2] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 208
        
        f = { x: x + p + 1 };
        "[obj.m 3] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 308

        f = { x: x + 2 };
        "[obj.m 4] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // fn 9
        
        f = { x: inherited(x) + 1 };
        "[obj.m 5] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 1008

        f = { x: x + i + p + 1 };
        "[obj.m 6] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 508

        f = { y: y + x + p + 1 };
        "[obj.m 7] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 358

        f = new function(y)
        {
            local g = { z: inherited(z) + 1 };
            return g(y + 1);
        };
        "[obj.m 8] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 1009

        f = new function(y)
        {
            local g = { z: z + 700 };
            return g(y + 1);
        };
        "[obj.m 9] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // fn 708

        f = new function(y)
        {
            local g = { z: z + i + 700 };
            return g(y + 1);
        };
        "[obj.m 10] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 908

        f = new function(y)
        {
            local g = { z: inherited(z) + 10 };
            return g(y + 1);
        };
        "[obj.m 11] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 1018

        f = new function(y)
        {
            local g = { z: z + p + 500 };
            return g(y + 1);
        };
        "[obj.m 12] type(f) = <<funcType(f)>>,  f(7) = <<f(7)>>\n"; // obj 808
    }
    
    p = 300
;


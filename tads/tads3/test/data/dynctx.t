/*
 *   tests of method context with dynamic functions 
 */

#include <tads.h>
#include <dynfunc.h>

main(args)
{
    local i = 7;
    useval(i);
    
    run('test 1: function with no method context',
        'function(x) { return x*i; }', 2);

    objA.test2();
    objB.test4();
}

useval(n)
{
}

objA: object
    name = 'objA'
    test2()
    {
        run('test 2: function with a method context',
            'function(x) { return x*p1; }', 3);

        run('test 3: method with a method context',
            'method(x) { return x*p1; }', 4);
    }
    test4() { return 11; }
    testMethod() { return 41; }
    p1 = 13
;

objB: objA
    name = 'objB'
    test4()
    {
        run('test 4: function with a full method context',
            'function(x) { return x*p1*inherited(); }', 5);

        run('test 5: function with a full method context',
            'function() { return definingobj.p1; }');

        run('test 6: method with a full method context',
            'method(x) { return x*p1*inherited(); }', 7);
    }
    p1 = 37
;

objC: objA
    name = 'objc'
    p1 = 19
;

run(desc, src, [args])
{
    local frames = t3GetStackTrace(nil, T3GetStackDesc)
        .sublist(2)
        .mapAll({x: x.frameDesc_});

    "<<desc>>\n<<src>>\n";
    "self available: <<frames[1].getSelf() != nil ? 'yes' : 'no'>>\n";
    try
    {
        local func = Compiler.compile(src, frames);
        try
        {
            "As function: <<func(args...)>>\n";
        }
        catch (Exception e)
        {
            "error: <<e.displayException()>>\n";
        }

        objC.setMethod(&testMethod, func);
        try
        {
            "As method: <<objC.testMethod(args...)>>\n";
        }
        catch (Exception e)
        {
            "error: <<e.displayException()>>\n";
        }
    }
    catch (Exception e)
    {
        "Compilation error: <<e.displayException()>>\n";
    }
    "\b";
}

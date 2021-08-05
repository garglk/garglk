#include <tads.h>
#include <dynfunc.h>

showIfc(p)
{
    if (p != nil)
        "{ argc=<<p[1]>>, optArgc=<<p[2]>>, varargs=<<p[3] ? 'yes' : 'no'>> }";
    else
        "(not found)";
}

obj: object
;

main(args)
{
    local f = &tadsSay;
    "f=tadsSay: ifc=<<showIfc(getFuncParams(f))>>\n";
    f('this is a pointer call to tadsSay!!!\n');
    "\b";

    obj.setMethod(&m1, &tadsSay);
    "obj.m1=tadsSay: ifc=<<showIfc(obj.getPropParams(&m1))>>\n";
    obj.m1('this is a setMethod call to tadsSay!!!\n');
    "\b";

    f = Compiler.compile(
        'function(x) {'
        + 'local f = &tadsSay;'
        + '"in DynamicFunc: f=tadsSay: ifc=<\<showIfc(getFuncParams(f))>>\\n";'
        + 'f(\'DynamicFunc pointer call to tadsSay! x=<\<x>>\\n\');'
        + '}');
    f(1234);
}

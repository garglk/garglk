#include <tads.h>
#include <dynfunc.h>

f(x)
{
    return 2*x;
}

main(args)
{
    local f1 = f;
    local zero = 0;
    local f2 = { x: 3*x + zero };
    local f3 = defined(DynamicFunc)
        ? new DynamicFunc('function(x) { return 4*x; }')
        : nil;

    test('f1', f1);
    test('f2', f2);
    test('f3', f3);

    "f3.ofKind(DynamicFunc) ? <<if f3.ofKind(DynamicFunc)>>yes<<else
      >>no<<end>>\n";
}

test(name, func)
{
    if (func != nil)
        "<<name>>(5) = <<func(5)>>\n";
    "dataType(<<name>>) = <<dataType(func)>>\n";
    "dataTypeXlat(<<name>>) = <<dataTypeXlat(func)>>\n";
    "\b";
}


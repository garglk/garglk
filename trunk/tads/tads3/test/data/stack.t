#include <tads.h>

class myClass: object
    prop1(a, b)
    {
        local lst;

        lst = t3GetStackTrace();

        "myClass.prop1:\n";
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
            "<<i>>: <<reflectionServices.formatStackFrame(lst[i], true)>>\n";
        "\b";
    }
;

myObj: myClass
    prop2(x, y)
    {
        local lst;

        lst = t3GetStackTrace();

        "myObj.prop2:\n";
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
            "<<i>>: <<reflectionServices.formatStackFrame(lst[i], true)>>\n";
        "\b";

        prop1(x, y);
    }
;

func2(str, len, num)
{
    local lst;
    
    lst = t3GetStackTrace();

    "func2:\n";
    for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        "<<i>>: <<reflectionServices.formatStackFrame(lst[i], true)>>\n";
    "\b";

    myObj.prop2(str, num);
}

func1(x, y)
{
    func2(x, x.length(), y*2);
}

main(args)
{
    func1('abc', 123);
    func1('def', 456);
}


#include <tads.h>

main(args)
{
    testObj.testMethod();
}

testObj: object
    testMethod()
    {
        local lst = [1, 2, 3, 4, 5];
        
        lst.forEach(new function(a)
        {
            "outer: a = <<a>>\n";
            lst.forEach(new function(b)
            {
                "inner: a = <<a>>, b = <<b>>\n";
                if (a != b && a < b)
                    lst -= b;
            });
        });
    }
;

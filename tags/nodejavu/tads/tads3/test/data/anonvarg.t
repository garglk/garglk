/*
 *   Test of anonymous functions with varargs-list arguments.  Anonymous
 *   functions and varargs-list arguments both use non-standard parameter
 *   value references, and the different special handlings interfered with
 *   one another in older versions of the compiler. 
 */

#include <tads.h>

main(args)
{
    fixedfunc(1, 2);
    varfunc(4, 5, 6);
    varfunc2(7, 8, 9);
}

fixedfunc(a, b)
{
    local func = {x, y: tadsSay('fixedcb: x=' + x + ', y=' + y + '\n')};
    (func)(a, b);
    (func)(a*2, b*2);
}

varfunc([args])
{
    local func = new function(lst) {
        foreach (local x in lst)
            "\t<<x>>\n";
    };

    "varfunc: via callback\n";
    (func)(args);

    "varfunc: args.length() = <<args.length()>>\n";
}

varfunc2([args])
{
    local func = new function(hdr) {
        "<<hdr>>\n";
        if (args == nil)
            "\t<args = nil>\n";
        else
            foreach (local x in args)
                "\t<<x>>\n";
    };

    (func)('varfunc2 callback:');

    if (args == nil)
        "varfunc2: args = nil\n";
    else
        "varfunc2: args.length() = <<args.length()>>\n";
}

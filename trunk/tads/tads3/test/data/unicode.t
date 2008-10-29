/*
 *   unicode conversion function tests 
 */

#include "tads.h"
#include "t3.h"


main(args)
{
    local p;
    local i;
    
    "makeString(42) = '<<makeString(42)>>'\n";
    "makeString([42, 43, 44]) = '<<makeString([42, 43, 44])>>'\n";
    "makeString('asdf') = '<<makeString('asdf')>>'\n";

    "makeString(42, 10) = '<<makeString(42, 10)>>'\n";
    "makeString([42, 43, 44], 10) = '<<makeString([42, 43, 44], 10)>>'\n";
    "makeString('abcd', 5) = '<<makeString('abcd', 5)>>'\n";

    p = [42, 43, 44];
    p += [55, 56, 57];
    "makeString(p[lst], 20) = '<<makeString(p, 20)>>'\n";

    p = 'Hello';
    p += '!!!';
    "makeString(p[str], 8) = '<<makeString(p, 8)>>'\n";

    "makeString([0x7000, 0x7001, 0x7002], 5) =
     '<<makeString([0x7000, 0x7001, 0x7002], 5)>>'\n";

    "\b";

    for (i = 1, p = 'asdf' ; i <= p.length() ; ++i)
        "'<<p>>'.toUnicode(<<i>>) = <<p.toUnicode(i)>>\n";

    "'<<p>>'.toUnicode() = <<sayList(p.toUnicode())>>\n";

    p = '\u7000\u7001abc\u7002\u7003def\xA8';
    "'<<p>>'.toUnicode() = <<sayList(p.toUnicode())>>\n";
}

sayList(lst)
{
    if (lst == nil)
        "nil";
    else
    {
        "[";
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            tadsSay(lst[i]);
            if (i != len)
                ", ";
        }
        "]";
    }
}

preinit()
{
}



#include <tads.h>

main(args)
{
    "length (global.l1) == <<global.l1.length()>>\n";
    for (local i = 1 ; i < global.l1.length() ; ++i)
        "[<<i>>] = \"<<global.l1[i]>>\"\n";
}

global: PreinitObject
    execute()
    {
        "In preinit\n";
        for (local limit = 0 ; limit < 1000 ; ++limit)
            l1 += stringOf(limit);
    }
    l1 = []
;

stringOf(i)
{
    local rv = '';
    while (--i > 0)
        rv += toString(i);
    return rv;
}

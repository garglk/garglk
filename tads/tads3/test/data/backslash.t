#include <tads.h>

main(args)
{
    for (;;)
    {
        "&gt;";
        local s = inputLine();
        s = rexReplace(
            ['^<space>*"', '\\"', '\\\\', '\\n', '\\r', '"<space>*$'], s,
            ['', '"', '\\', '\n', '\r', '']);
        "<<s.htmlify()>><br>";
    }
}

#include "tads.h"
#include "t3.h"

preinit() { }

main(args)
{
    for (;;)
    {
        local pat, str, match;

        "regular expression: "; pat = inputLine();
        if (pat == '')
            break;
        
        "string: "; str = inputLine();

        match = rexMatch(pat, str);
        if (match == nil)
            "No match.\n";
        else
        {
            "Match length = <<match>>\n";
            for (local i = 1 ; i < 9 ; ++i)
            {
                local g = rexGroup(i);
                if (g != nil)
                    "group <<i>> = '<<g[3]>>'\n";
            }
        }

        "\b";
    }
}

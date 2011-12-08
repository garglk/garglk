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

        match = rexSearch(pat, str);
        if (match == nil)
            "No match.\n";
        else
        {
            "Match=[<<match[3]>>] (ofs <<match[1]>>, len <<match[2]>>)\n";
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

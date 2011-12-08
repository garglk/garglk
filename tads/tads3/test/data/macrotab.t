#include <tads.h>

globals: PreinitObject
    execute() { macroTab = t3GetGlobalSymbols(T3PreprocMacros); }
    macroTab = nil
;

#define VARMAC(a, b...) tadsSay(a, ##b)

main(args)
{
    local t = globals.macroTab;
    if (t != nil)
    {
        t.forEachAssoc(new function(key, val) {
            tadsSay(key);
            local flags = val[3];
            if (flags & 0x0001)
            {
                "(";
                local args = val[2];
                for (local i = 1, local len = args.length() ; i <= len ; ++i)
                {
                    if (i > 1)
                        ", ";
                    tadsSay(args[i]);
                }
                if (flags & 0x0002)
                    tadsSay(args.length() > 0 ? ', ...' : '...');
                ")";
            }
            " -&gt; <<val[1].htmlify()>>\n";
        });
    }
    else
        "No macro table available.\n";
}

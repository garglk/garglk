#include <tads.h>

main(args)
{
    local r = new function(s, idx, orig)
    {
        "callback: group(1) = <<rexGroup(1)[3]>>\n";

        /* don't capitalize certain small words, except at the beginning */
        if (idx > 1 && ['a', 'an', 'of', 'the', 'to'].indexOf(s.toLower()) != nil)
            return s;

        /* capitalize the first letter */
        return s.substr(1, 1).toUpper() + s.substr(2);
    };
    tadsSay(rexReplace('%<(<alphanum>+)%>', args[2], r, ReplaceAll));
    "\n";

    tadsSay(rexReplace('-(<digit>+)', args[2], '<font color=red>(%1)</font>',
                       ReplaceAll));
    "\n";
}

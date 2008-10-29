#include "tads.h"
#include "t3.h"

_say_embed(str) { tadsSay(str); }

_main(args)
{
    t3SetSay(_say_embed);
    main();
}

showlist(lst)
{
    "[";
    for (local i = 1 ; i <= lst.length() ; ++i)
    {
        if (i > 1)
            " ";
        tadsSay(lst[i]);
    }
    "]";
}

main()
{
    local x = [];

    "initially: x = <<showlist(x)>>\n";

    x += ['one'];
    "after adding [one]: x = <<showlist(x)>>\n";

    x += ['two'];
    "after adding [two]: x = <<showlist(x)>>\n";
    
    x += ['three', 'four'];
    "after adding [three, four]: x = <<showlist(x)>>\n";
}


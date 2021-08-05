#include <tads.h>

main(args)
{
    for (;;)
    {
        ">";
        local t = inputLine();
        if (t == nil)
            break;

        "<<t.htmlify()>> -&gt; <<rand(t).htmlify()>>\b";
    }
}

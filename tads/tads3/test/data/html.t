/*
 *   HTML output tests
 */

#include "t3.h"
#include "tads.h"

_say_embed(str) { tadsSay(str); }

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime Error: <<errno_>>"
    errno_ = 0
;

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

function main()
{
    "HTML mode...\n";

    "Translations: <b>&lt;&amp;&gt;</b>.\n";

    "\bDone!\n";
}

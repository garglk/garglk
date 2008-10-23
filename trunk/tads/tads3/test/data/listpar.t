/*
 *   list varargs parameters
 */

#include "tads.h"
#include "t3.h"

class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    display = "Runtime error: <<exceptionMessage>>"
    errno_ = 0
    exceptionMessage = ''
;

_say_embed(str) { tadsSay(str); }

_main(args)
{
    try
    {
        t3SetSay(_say_embed);
        main(args);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

main(args)
{
    func(9);
    func(8, 2);
    func(7, 2, 3);
    func(6, 2, 3, 4);
}

func(a, [b])
{
    "func: a = <<a>>, b = <<sayList(b)>>\n";
    func2(b..., a);
    func2(a, b...);
}

func2([x])
{
    "+ func2: x = <<sayList(x)>>\n";
}

sayList(lst)
{
    if (lst == nil)
    {
        "nil";
    }
    else
    {
        "[";
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            if (i != 1)
                ", ";
            tadsSay(lst[i]);
        }
        "]";
    }
}


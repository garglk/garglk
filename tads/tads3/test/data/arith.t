#include "t3.h"
#include "tads.h"

A: object
    x_ = 123
    y_ = 456
;

main(args)
{
    local x, y;

    x = 678;
    y = 531;

    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";
    "x*y = <<x*y>>, x/y = <<x/y>>, x%y = <<x % y>>\n";

    "A.x_ = <<A.x_>>, A.y_ = <<A.y_>>, A.x_+A.y_ = <<A.x_+A.y_>>,
        A.x_-A.y_ = <<A.x_-A.y_>>\n";
    "A.x_*A.y_ = <<A.x_*A.y_>>, A.x_/A.y_ = <<A.x_/A.y_>>,
        A.x_%A.y_ = <<A.x_ % A.y_>>\n";
}

preinit()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   some boilerplate setup stuff 
 */

class Exception: object
    display = "Unknown exception"
;

class RuntimeError: Exception
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
        if (!global.preinited_)
        {
            preinit();
            global.preinited_ = true;
        }
        if (!t3GetVMPreinitMode())
            main(args);
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

global: object
    preinited_ = nil
;

#include "tads.h"
#include "t3.h"
#include "bignum.h"

#define time_start(msg) \
    { tadsSay(msg); "...\n"; local ts__ = getTime(GetTimeTicks);
#define time_end \
    ts__ = getTime(GetTimeTicks) - ts__; "\n...done: time = <<ts__>> ms\b"; }

main(args)
{
    local i;
    local x, y;
    local iter = 100;

    x = new BigNumber('2.53000000');

    time_start('exp');
    for (i = 1 ; i < iter ; ++i)
        y = x.expE();
    time_end;

    time_start('log');
    for (i = 1 ; i < iter ; ++i)
        y = x.logE();
    time_end;

    time_start('sinh');
    for (i = 1 ; i < iter ; ++i)
        y = x.sinh();
    time_end;

    time_start('cosh');
    for (i = 1 ; i < iter ; ++i)
        y = x.cosh();
    time_end;

    time_start('tanh');
    for (i = 1 ; i < iter ; ++i)
        y = x.tanh();
    time_end;

    "done!\n";
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
        t3SetSay(&_say_embed);
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

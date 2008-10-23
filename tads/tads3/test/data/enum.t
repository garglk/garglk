/*
 *   enumerator
 */

#include "tads.h"
#include "t3.h"

enum apple, orange, grape, banana, pear, plum, cherry;

enum token tokWordDict, tokWordUnk, tokPunctuation, tokInvalid;

preinit()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   some boilerplate setup stuff 
 */

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
        t3SetSay(&_say_embed);
        if (!global.preinited_)
        {
            preinit();
            global.preinited_ = true;
        }
        if (!t3GetVMPreinitMode())
            main();
    }
    catch (RuntimeError rte)
    {
        "\n<<rte.display>>\n";
    }
}

global: object
    preinited_ = nil
;

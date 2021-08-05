#include "tads.h"
#include "t3.h"

main(args)
{
    if (t3DebugTrace(T3DebugCheck))
        "The debugger is present.\n";
    else
        "No debugger is available.\n";

    for (local i = 1 ; i <= 10 ; ++i)
        "i = <<i>>\n";

    if (!t3DebugTrace(T3DebugBreak))
        "<Unable to stop in debugger - no debugger present>\n";

    for (local i = 10 ; i >= 1 ; --i)
        "i = <<i>>\n";
}

preinit()
{
}

#include "tads.h"
#include "t3.h"

preinit() { }
main(args)
{
    "Arguments:\n";
    for (local i = 1, local cnt = args.length() ; i <= cnt ; ++i)
        "\t[<<i>>] = '<<args[i]>>'\n";
    "\b";

    "Input test - enter a blank line to terminate.\b";

    for (;;)
    {
        local str;

        ">";
        str = inputLine();
        if (str == '')
            break;
        "You entered: '<<str>>'\b";
    }

    "Done!\n";
}


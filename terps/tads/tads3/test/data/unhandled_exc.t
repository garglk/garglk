#include <tads.h>

main(args)
{
    if (args.length() == 2)
        throw new ProgramException(args[2]);
    else
        throw new ProgramException('usage: unhandled_exc <message>');
}


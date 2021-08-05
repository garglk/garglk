#include <tads.h>

main(args)
{
    for (local i = 1, local len = args.length() ; i <= len ; ++i)
        tadsSay('[', i, '] = "', args[i], '"\n');
}

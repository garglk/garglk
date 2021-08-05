#include "tads.h"
#include "t3.h"

main(args)
{
    local x = toString(9) > toString(3) ? 1 : toString(9) < toString(3) ? -1 : 0;
    tadsSay(x);
}

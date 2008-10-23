#include "tads.h"
#include "t3.h"

dictionary gDict;

replace gDict: object
    x = 1
;

modify gDict
    y = 2
    replace z = 3
;

main(args)
{
}

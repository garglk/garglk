#include "t3.h"
#include "tads.h"

function _main(args)
{
    local i, j;

    tadsSay('we\'re about to cause an unhandled run-time exception...\n');
        
    i = 'hello';
    j = 'goodbye';

    i = i / j;

    tadsSay('??? we shouldn\'t ever get here!\n');
}

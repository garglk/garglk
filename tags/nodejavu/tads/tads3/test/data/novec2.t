#charset "us-ascii"

#include <tadsgen.h>

/*
 *   program without including vector metaclass definition, to test
 *   creation of metaclass globals
 */
main(args)
{
    local result;
    
    /* say hello */
    "This is main() - restartID = <<mainGlobal.restartID>>\n";

    /* try saving */
    result = saveGame('novec2.t3s');
    if (result != nil)
    {
        "Successfully restored!\n";
    }
    else
    {
        "Successfully saved - now trying restore.\n";
        restoreGame('novec2.t3s', 1);
    }
}

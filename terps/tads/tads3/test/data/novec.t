#charset "us-ascii"

#include <tadsgen.h>

/*
 *   program without including vector metaclass definition, to test
 *   creation of metaclass globals
 */
main(args)
{
    if (mainGlobal.restartID > 10000)
    {
        "Done!\n";
        return;
    }

    /* say hello */
    "This is main() - restartID = <<mainGlobal.restartID>>\n";
    
    /* try restarting */
    restartGame(_mainRestart, mainGlobal.restartID + 1);
}


#include "tads.h"

function _main(args)
{
    local i;
    
    i = 1;

    /* this one should fail - transfers into a 'finally' block */
    if (i > 50)
        goto label_in_finally;
    
    for (local i = 1 ; i <= 100 ; ++i)
    {
        /* this one should also fail */
        if (i > 50)
            goto label_in_finally;

        try
        {
            "This is a test.";

            /* 
             *   this one should fail, too - we can't even transfer into
             *   our own 'finally' block within a 'try' or 'catch' 
             */
            if (i > 30)
                goto label_in_finally;
        }
        finally
        {
            "In the 'finally'";

        label_in_finally:
            "After the label in the finally!";

            /* this one should work - it's within the same block */
            if (i > 75)
                goto label_in_finally;

            try
            {
                "Sub-try";
            }
            finally
            {
                /* this one should also work */
                goto label_in_finally;
            }
        }
    }
}

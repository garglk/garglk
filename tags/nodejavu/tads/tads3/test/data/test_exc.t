#include "tads.h"

function _main(args)
{
    tadsSay('entering main\n');
    exc_test();
    tadsSay('done with main\n');
}


function exc_test()
{
    local i;

    tadsSay('entering try\n');
    try
    {
        tadsSay('this should be innocuous!\n');
    }
    finally
    {
        tadsSay('this is the first finally\n');
    }
        
    tadsSay('entering try 2\n');
    try
    {
        tadsSay('entering nested try 2a\n');
        try
        {
            tadsSay('in try - computing invalid value\n');
            i = 5;
            i = i + 'x';
            tadsSay('??? after forming invalid value\n');
        }
        finally
        {
            tadsSay('this is finally 2a\n');
        }

        tadsSay('after nested try???\n');
    }
    finally
    {
        tadsSay('this is the second finally block!\n\n');
    }
}


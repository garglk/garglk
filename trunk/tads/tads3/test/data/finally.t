#include "tads.h"

function _main(args)
{
    tadsSay('entering main\n');

    break_test();

    cont_test();

    tadsSay('ret_test(nil) = ' + ret_test(nil));
    tadsSay('ret_test(true) = ' + ret_test(true));

    goto_test();
  
    exc_test();

    tadsSay('done with main\n');
}

/* ------------------------------------------------------------------------ */
/*
 *   This function tests processing of 'finally' when breaking out of
 *   loops and switches. 
 */
function break_test()
{
    tadsSay('Break Tests\b');
    
    tadsSay('loop 1\n');
    for (local i = 1 ; i < 5 ; ++i)
    {
        tadsSay('entering try1\n');
        try
        {
            tadsSay('loop 1 - i = ' + i + '\n');
            if (i > 3)
            {
                tadsSay('breaking...\n');
                break;
            }
        }
        finally
        {
            tadsSay('this is the finally for try 1\n');
        }
        tadsSay('done with try1\n');
    }
    tadsSay('end of loop 1\b');

    tadsSay('loop 2 - label LOOP2:\n');
loop2:
    for (local i = 1 ; i < 5 ; ++i)
    {
        tadsSay('(( inner loop 2 - i = ' + i + '\n');
        for (local j = 1 ; j < i ; ++j)
        {
            tadsSay('\t+ loop 2a - j = ' + j + '\n');
            if (j == 3)
            {
                tadsSay('breaking out of LOOP2...\n');
                break loop2;
            }
        }
        tadsSay(')) end of inner loop 2 - i = ' + i + '\n'); 
    }
    tadsSay('end of loop 2\n');
}

/* ------------------------------------------------------------------------ */
/*
 *   This function tests continuing from protected code 
 */
function cont_test()
{
    tadsSay('\bContinue Test\b');

    local i;
    i = 0;
loop1:
    while (i < 5)
    {
        try
        {
            ++i;
            tadsSay('loop1 - i = ' + i + '\n');

            for (local j = 1 ; j < 100 ; ++j)
            {
                try
                {
                    tadsSay('\tinner loop 1a - j = ' + j + '\n');
                    if (j == 2)
                    {
                        tadsSay('+ continuing...\n');
                        continue loop1;
                    }
                }
                finally
                {
                    tadsSay('inner finally\n');
                }
            }
        }
        finally
        {
            tadsSay('outer finally\n');
        }
    }
    tadsSay('end loop1\n');
}

/* ------------------------------------------------------------------------ */
/*
 *   This function tests returning from protected code 
 */
function ret_test(ret_middle)
{
    tadsSay('\bReturn Test -- will ' + (ret_middle ? '' : 'not ') +
        'return in the middle of the try block\b');

    tadsSay('entering try 1\n');
    try
    {
        tadsSay('entering try 1a\n');
        try
        {
            if (ret_middle)
            {
                tadsSay('returning...\n');
                return 'TRUE!!!';
            }
            else
            {
                tadsSay('not returning\n');
            }

            tadsSay('end of inner try 1a\n');
        }
        finally
        {
            tadsSay('finally 1a\n');
        }

        tadsSay('end out outer try 1\n');
    }
    finally
    {
        tadsSay('finally 1\n');
    }

    tadsSay('end of ret_test\n');

    return 'NIL!!!';
}

/* ------------------------------------------------------------------------ */
/*
 *   goto test - test jumping around within a function with 'goto', and
 *   the interaction with 'finally' handlers 
 */
function goto_test()
{
    tadsSay('\nGoto Tests\b');

    tadsSay('simple goto s1...\n');
    goto s1;

s3:
    tadsSay('now at s3!\n');

    tadsSay('jumping into a try - s4\n');
    goto s4;

s1:
    tadsSay('here we are at s1!\n');

    tadsSay('entering try 1\n');
    try
    {
        tadsSay('goto within try - s2\n');
        goto s2;

    s4:
        tadsSay('at s4!\n');

        tadsSay('leaving try 1 again - s5\n');
        goto s5;

    s2:
        tadsSay('here we are at s2!\n');

        tadsSay('jumping out of try 1 to s3\n');
        goto s3;

        // this is unreachable
        tadsSay('??? how did we get here???\n');
    }
    finally
    {
        tadsSay('finally 1\n');
    }

    // this is also unreachable
    tadsSay('??? unreachable!\n');

s5:
    tadsSay('okay, this is s5\n');
}


/* ------------------------------------------------------------------------ */
/*
 *   This function ends up throwing an uncaught error (a VM-level run-time
 *   exception), so it will never return.  This tests processing 'finally'
 *   blocks while throwing an exception out of a protected block.  
 */
function exc_test()
{
    local i;

    tadsSay('\bException Throw Tests\b');
    
    tadsSay('entering try 1\n');
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
            tadsSay('in try - calling exc_test_thrower - valid value...\n');
            exc_test_thrower(3);

            tadsSay('in try - calling exc_test_thrower - INVALID value...\n');
            exc_test_thrower('x');

            tadsSay('??? back from invalid exc_test_thrower call ???\n');
        }
        finally
        {
            tadsSay('this is finally 2a\n');
        }

        tadsSay('after nested try???\n');
    }
    finally
    {
        tadsSay('this is the second finally block!\b');
    }
}

function exc_test_thrower(x)
{
    tadsSay('+ in exc_test_thrower - entering try\n');
    try
    {
        tadsSay('+ in try - computing value\n');
        local i = 5 + x;
        tadsSay('+ result = ' + i + '\n');
    }
    finally
    {
        tadsSay('+ exc_test_thrower finally\n');
    }

    tadsSay('+ exc_test_thrower - returning\n');
}


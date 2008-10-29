/*
 *   Top-level parsing test - input for test_prs_top
 */

#include "tads.h"

function main()
{
    local i, j;
    local iter;

    for (iter = 0 ; iter < 5 ; ++iter)
    {
        i = 7;
        j = factorial(i);
        
        tadsSay('iter[' + iter + ']: ' + i + '! = ' + j + '\n');
    }

    test_for_locals();

    test_break_cont();

    test_while();

    test_do_while();

    test_switch();

//    test_catch();
}

#if 0
function test_catch()
{
    try
    {
        local i, j;
        
        tadsSay('calculating 5 + \'x\'...\n');
        i = 5;
        j = 'x';
        tadsSay(i + j);
    }
    catch (RuntimeError exc)
    {
        tadsSay('caught a run-time error!\n');
    }

    try
    {
        tadsSay('here\'s a "try" with a "finally" block...\n');
    }
    finally
    {
        tadsSay('okay, we\'re in the finally!\n');
    }

    try
    {
        local i, j;
        
        tadsSay('calculating 5 + \'x\' in "try" with "catch" and "finally"...\n');
        i = 5;
        j = 'x';
        tadsSay(i + j);
    }
    catch (RuntimeError exc)
    {
        tadsSay('caught a RuntimeError\n');
    }
    finally
    {
        tadsSay('this is the finally block\n');
    }

    tadsSay('done with exception tests\n');
}
#endif

function factorial(x)
{
    if (x > 1)
        return x * factorial(x-1);
    else
        return 1;
}

function test_for_locals()
{
    local x;
    local i = 'outer i';
    
    for (local i = 1, x = 'bye', local j = 'hello' ; i < 5 ; ++i)
        tadsSay('i = ' + i + ', j = ' + j + ', x = ' + x + '\n');

    tadsSay('at outer scope: i = ' + i + '\n');
}

function test_break_cont()
{
    for (local i = 1 ; i < 10 ; ++i)
    {
        tadsSay('test break - i = ' + i + '\n');
        if (i == 5)
            break;
    }

    for (local i = 1 ; i < 10 ; ++i)
    {
        tadsSay('test continue - i = ' + i + '\n');
        if (i >= 5)
            continue;

        tadsSay('...not continuing this time!\n');
    }
}

function test_while()
{
    local i;

    i = 0;
    while (i < 5)
    {
        tadsSay('this is while loop iteration #' + i + '\n');
        ++i;
    }

    while (i > 0)
    {
        tadsSay('this is while loop 2 - iteration #' + i + '\n');
        break;
    }

    while (i > 0)
    {
        tadsSay('this is while loop 3 - iteration #' + i + '\n');
        --i;
        if (i <= 2)
            continue;
        --i;
    }
}

function test_do_while()
{
    local i;

    i = 0;
    do
    {
        tadsSay('this is do loop - iteration #' + i + '\n');
    } while (i++ < 5);

    do
    {
        tadsSay('this is do loop 2 - iteration #' + i + '\n');
        if (i > 6)
            break;
        ++i;
    }
    while (i < 10);

    do
    {
        tadsSay('this is do loop 3 - iteration #' + i + '\n');
        --i;
        if (i < 3)
            continue;
        --i;
    } while (i > 0);
}

function test_switch()
{
    local i;

    i = 3;
    tadsSay('no breaks - i = ' + i + '\n');
    switch(i)
    {
    case 1:
        tadsSay('case 1\n');
        
    case 2:
        tadsSay('case 2\n');
        
    case 3:
        tadsSay('case 3\n');
        
    case 4:
        tadsSay('case 4\n');
        
    case 5:
        tadsSay('case 5\n');

    default:
        tadsSay('default 1\n');
    }

    i = 2;
    tadsSay('with breaks - i = ' + i + '\n');
    switch(i)
    {
    case 1:
        tadsSay('case 1\n');
        break;
        
    case 2:
        tadsSay('case 2\n');
        break;
        
    case 3:
        tadsSay('case 3\n');
        break;
        
    case 4:
        tadsSay('case 4\n');
        break;
        
    case 5:
        tadsSay('case 5\n');
        break;
    }
}

function test()
{
    /* this loop never terminates... */
    for (;;)
    {
    }

    /* ...so this statement is unreachable - we should get a warning */
    test();
    main();
}

function test2()
{
    /* 
     *   This function only returns with a value, because the end of the
     *   "for" is unreachable - we should NOT get a warning about
     *   returning with and without a value 
     */
    for (;;)
    {
        return 1;
    }
}    

function test2()
{
    /* 
     *   this function implicitly returns both with and without a value -
     *   we should get a warning about this 
     */
    for (local i = 1, local j = 2 ; i < 100 ; ++i)
    {
        return i;
    }
}

/*
 *   required entrypoint function 
 */
function _main(args)
{
    tadsSay('... this is _main() ...\n');
    main();
    tadsSay('... _main() exiting ...\n');
}

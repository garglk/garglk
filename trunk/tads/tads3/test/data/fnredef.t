/*
 *   Re-define a function name 
 */

function _main(args)
{
    test();
}

function test()
{
    test2();
}

function test2()
{
    tadsSay('hello from test2!\n');
}

function test()
{
    tadsSay('this is the redefined test()\n');
}


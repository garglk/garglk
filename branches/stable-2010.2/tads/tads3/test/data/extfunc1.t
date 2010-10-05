/*
 *   test of 'extern function' declarations 
 */

#include "tads.h"

extern function test1(a, b, c);
extern function test2(d, e);

function _main(args)
{
    tadsSay('this is main - calling test1(1, 2, 3)...\n');
    test1(1, 2, 3);

    tadsSay('back in main - calling test2(4, 5)...\n');
    test2(4, 5);

    tadsSay('back in main - terminating!\n');
}

function test2(a, b)
{
    tadsSay('-> this is test2(' + a + ', ' + b + ')\n');
}

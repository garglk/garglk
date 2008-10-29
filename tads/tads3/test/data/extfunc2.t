/*
 *   test of 'extern function' declarations 
 */

#include "tads.h"

extern function test1(a, b, c);
extern function test2(d, e);

function test1(a, b, c)
{
    tadsSay('++ this is test1(' + a + ', ' + b + ', ' + c + ')\n');
    tadsSay('++ calling test2(888,999)...\n');
    test2(888, 999);

    tadsSay('++ back in test1 - returning\n');
}

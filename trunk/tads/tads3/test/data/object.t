/*
 *   Object Tests 
 */

#include "tads.h"

/* define the T3 system interface */
intrinsic 't3vm/010000'
{
    t3RunGC();
    t3SetSay(funcptr);
    t3GetVMVsn();
    t3GetVMID();
    t3GetVMBanner();
    t3GetVMPreinitMode();
}

test: object
    p1 = 'This is test.p1\n'
    p2 = 'This is test.p2\n'
;

test2: test
    p1 = 'This is test2.p1\n'

    p3(a, b)
    {
        "Here we are in test2.p3!\n";
        "test2.p3 arguments: a = << a >>, b = << b >>\n";
    }

    p4 = ('this is test2.p4: self.p5 = ' + p5)
    p5 = 'This is test2.p5!\n'
    p6 = "This is a double-quoted string in test2.p6!!!\n"
    p7 = "This is test2.p7, a dstring with embedding -
          self.p3(9,10) = << p3(9,10) >>!!!\n"
;

function _say_embed(str)
{
    tadsSay(str);
}

function _main(args)
{
    t3SetSay(&_say_embed);

    "Property test - calling test.p1\n";
    tadsSay(test.p1);

    "calling test.p2\n";
    tadsSay(test.p2);

    "calling test2.p1\n";
    tadsSay(test2.p1);

    test.p3 = 1;

    "calling test2.p2\n";
    tadsSay(test2.p2);

    "calling test2.p3(1,'x')\n";
    test2.p3(1, 'x');

    "calling test2.p4\n";
    tadsSay(test2.p4);

    "calling test2.p5\n";
    tadsSay(test2.p5);

    "calling test2.p6\n";
    test2.p6;

    "calling test2.p7\n";
    test2.p7;

    "Done!\n";
}


/*
 *   test of 'replace function' 
 */

#include "tads.h"
#include "t3.h"

function _say_embed(str)
{
    tadsSay(str);
}

function _main(args)
{
    t3SetSay(_say_embed);

    "Calling f1(123, 456)...\n";
    f1(123, 456);

    "Calling f2(789, 987, 323)...\n";
    f2(789, 987, 323);
}

function f1(a, b)
{
    "This is the original f1: a = <<a>>, b = <<b>>.\n";
    f2(a, b, 'f1.c');
}

function f2(a, b, c)
{
    "This is the original f2: a = <<a>>, b = <<b>>, c = <<c>>\n";
}

replace function f1(a, b)
{
    "Replaced f1 - a = <<a>>, b = <<b>>.\n";
}

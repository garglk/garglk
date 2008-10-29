/*
 *   extern2 - this is one part of a three-part program in which the files
 *   reference symbols in one another 
 */

#include "tads.h"

function f2_a()
{
    tadsSay('this is f2_a\n');
    tadsSay(' - calling f1_b\n');
    f1_b();
}

function f2_b()
{
    tadsSay('this is f2_b\n');
}


/*
 *   extern1 - this is one part of a three-part program in which the
 *   source files reference symbols in one another 
 */

#include "tads.h"

function f1_a()
{
    tadsSay('this is f1_a\n');
    tadsSay(' - calling f2_b\n');
    f2_b();
}

function f1_b()
{
    tadsSay('this is f1_b\n');
}


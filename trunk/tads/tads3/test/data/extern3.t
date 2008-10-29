/*
 *   extern3 - this is one part of a three-part program in which the
 *   source files reference symbols in one another 
 */

#include "tads.h"

function _main(args)
{
    tadsSay('entering main\n');

    tadsSay('calling f1_a\n');
    f1_a();

    tadsSay('calling f2_a\n');
    f2_a();
}


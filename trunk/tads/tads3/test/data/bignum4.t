#include "tads.h"
#include "t3.h"
#include "bignum.h"

main(args)
{
    /* 
     *   try displaying a BigNumber with more whole places than digits in
     *   the entire number 
     */
    tadsSay(1.234567).formatString(5, 0, 30, -1, -1));
    "\n";
}

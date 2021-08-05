#include "tads.h"
#include "t3.h"
#include "bignum.h"

main(args)
{
    local x;
    x = -1.4e-5;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";
}

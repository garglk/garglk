#include "tads.h"

myObj: object
    bar = 1
;

main(args)
{
    local x = myObj;
    local y = [1, 2, 3];

    "x.bar = <<x.bar>>, -x.bar = <<-x.bar>>\n";
    "y[2] = <<y[2]>>, ~y[2] = <<~y[2]>>\n";
}

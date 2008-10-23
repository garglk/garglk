#include "tads.h"
#include "t3.h"

main(args)
{
    local x = &prop1;
    local y = testObj;

    (y).(x)(123, 456);
}

testObj: object
    prop1(a, b) { "this is prop1: a = <<a>>, b = <<b>>\n"; }
;


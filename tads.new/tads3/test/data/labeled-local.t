#include <tads.h>

main(args)
{
    local i = 1;
    
label1:
    local j = 2;

    "1: i = <<i>>, j = <<j>>\n";
    ++i;
    ++j;
    "2: i = <<i>>, j = <<j>>\b";

    if (i <= 2)
        goto label1;
}
